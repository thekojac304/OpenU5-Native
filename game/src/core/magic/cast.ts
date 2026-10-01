/**
 * Lanzamiento de hechizos (comando "Cast") — REGLAS EXACTAS del binario
 * (CAST.OVL:0x0dba dispatch + CAST2.OVL handlers; cada regla cita su offset en
 * re/notes/magic.md). Sustituye las aproximaciones previas (daño círculo·6,
 * curación fija, luz en turnos) por las fórmulas table-driven verificadas.
 *
 * Arquitectura: `castSpell` reproduce el DISPATCHER exacto (ventana temporal →
 * conocido → consumo → maná → nivel) y devuelve un DESCRIPTOR de efecto con los
 * parámetros exactos del binario. Los efectos GLOBALES sin objetivo (luz,
 * estado temporal, viento, comida) se aplican aquí sobre `GameState`; los que
 * requieren un objetivo elegido (curación/cura/despertar/resurrección) o el
 * motor de combate/mapa (ataques, campos, invocaciones…) se devuelven como
 * descriptor para que el aplicador correspondiente los resuelva con las helpers
 * de este módulo (heal/cure/awaken/resurrect + los rolls de combate).
 */
import type { CharacterState, GameState } from "../state.js";
import { CombatRng } from "../combat/formulas.js";
import { type SpellDef } from "./spells.js";
import {
  TIME_PERMITTED_BITS,
  requiredTimeBit,
  SPELL_WEAPON_GRAV_POR,
  SPELL_WEAPON_VAS_FLAM,
  SPELL_WEAPON_XEN_CORP,
  FIELD_WALL_TILE,
  FIELD_WALL_COMBAT_WEAPON,
  TIME_STATUS,
  LIGHT_MINUTES_IN_LOR,
  LIGHT_MINUTES_VAS_LOR,
  ARROW_TO_WIND,
  kalXenSummonType,
  resurrectionLevel,
  resurrectionMaxHp,
} from "./tables.js";

/**
 * Contexto de lanzamiento. `location` = g_location del mundo (0 exterior,
 * 1..0x20 pueblo, 0x21..0x7F mazmorra); `inCombat` fuerza el bit de combate
 * (el binario pone g_location>=0x80 al entrar al mapa de combate). `facingDir`
 * (0..4) y `windArrow` (dir de flecha para Rel Hur) los provee el llamador
 * cuando el hechizo apunta/lee dirección.
 *
 * `magicAbsorbed` — "Absorbed!" (DS 0x4624), gate CAST:0x0e3e/0x0e4c. Son DOS
 * ubicaciones con DOS reglas distintas:
 *   · loc 0x12 (Palacio de Blackthorn) absorbe **cuando NO llevas la Corona**
 *     (`cmp byte [g_crown],0; je 0x0e53`; g_crown pasa a 0xFF al recogerla).
 *   · loc 0x1D (Stonegate) absorbe **SIEMPRE**, sin condición.
 * ⚠️ ERRATA 07-08: este bloque decía «trono de LB/corona (CAST:0x0e37)». Tres cosas
 * mal y las tres inducían a error: la condición de la corona estaba INVERTIDA, faltaba
 * que 0x1D es incondicional, y 0x0e37 no es inicio de instrucción (cae dentro del
 * `test byte [bx+0x1c90],1` de 0x0e36). El canal hermano lo tiene bien desde antes:
 * `combat.ts` (gate de COMBATE, COMBAT.OVL 0x0936) cita `g_crown==0`.
 * PRODUCTOR: `castAbsorbedOutOfCombat` (abajo), invocado por el propio `castSpell` cuando
 * el llamador no fija el campo. Antes NADIE lo escribía —declarado y leído, sin productor—
 * y por eso fuera de combate el gate no existía.
 */
export type CastContext = {
  location: number;
  inCombat: boolean;
  facingDir?: number;
  windArrow?: number;
  /** Fíjalo sólo para FORZAR el gate; si se omite lo calcula `castAbsorbedOutOfCombat`. */
  magicAbsorbed?: boolean;
};

/**
 * Palacio de Blackthorn — absorbe SIN la Corona (`0x0e3e`/`0x0e45`).
 * ⚠ MISMO valor que `combat/combat.ts::LOC_PALACE_OF_BLACKTHORN`, duplicado a propósito:
 * `combat.ts` ya importa de este fichero y traerlo de allí cerraría el ciclo. Que no
 * diverjan lo vigila un aserto en `cast-absorbed-gate.test.ts`.
 */
export const LOC_PALACE_OF_BLACKTHORN = 0x12;
/** Stonegate — absorbe SIEMPRE, sin condición (`0x0e4c`). */
export const LOC_STONEGATE = 0x1d;

/**
 * "Absorbed!" del (C)ast FUERA DE COMBATE — gemelo de `combatCastAbsorbed`
 * (COMBAT.OVL 0x0936), que cubre el (C)ast de dentro de la arena. **El gate es DOBLE y
 * quien arregle uno solo arregla la mitad.**
 *
 * Cuerpo del gate de ubicación de `cast_command_dispatch` (CAST.OVL 0x0dba), leído entero
 * `0x0e1a-0x0e8e`. El ORDEN importa: las dos ramas de "Absorbed!" se evalúan EN MEDIO —
 * detrás del reparto exterior/combate y delante del test de bit pueblo/mazmorra:
 * ```
 * 0e1a  cmp byte [g_location],0     / jne 0e2c  → (exterior) test [bx+0x1c90],8 ; jmp 0e8e
 * 0e2c  cmp byte [g_location],0x7f  / jbe 0e3e  → (combate)  test [bx+0x1c90],1 ; jmp 0e8e
 * 0e3e  cmp byte [g_location],0x12  / jne 0e4c
 * 0e45  cmp byte [g_crown],0        / je  0e53  ★ absorbe cuando NO llevas la Corona
 * 0e4c  cmp byte [g_location],0x1d  / jne 0e74  ; si IGUAL cae en 0e53, SIN condición
 * 0e53  print DS 0x4624 "Absorbed!" ; tone_sweep ; jmp 0x11d9 (epílogo, NO la cola 0x11a6)
 * ```
 * ⇒ sólo alcanzable con `1 ≤ g_location ≤ 0x7f`, y la salida por `0x11d9` se salta el
 * resto entero: no consume el hechizo mezclado (`0x0ec8`), no cobra maná (`0x0ef8`) y no
 * llega a ningún efecto ⇒ `consumed: false` y CERO tiradas.
 *
 * `hasCrown` es `g_crown` (DS:0x57B4 = SAVED.GAM +0x20E = `lbArtifacts.crown`): la POSESIÓN.
 * A4-PARITY1 (NEW-1): su único escritor es el (G)et SJOG 0x16e6 (`mov byte [0x57b4],0xff`);
 * lo leen este gate, su gemelo de combate COMBAT 0x092f y ZSTATS 0x09e1. El (U)se de la
 * corona NO lo toca (escribe 0x1c en g_time_spell). Antes se pasaba `wornCrown`, un toggle
 * inventado: quien LLEVABA la corona sin "ponérsela" se comía "Absorbed!". Las dos mitades
 * leen lo mismo o vuelven a contradecirse.
 */
export function castAbsorbedOutOfCombat(
  location: number,
  inCombat: boolean,
  hasCrown: boolean,
): boolean {
  if (inCombat || location === 0 || location > 0x7f) return false; // 0x0e1a / 0x0e2c
  if (location === LOC_PALACE_OF_BLACKTHORN && !hasCrown) return true; // 0x0e3e + 0x0e45
  return location === LOC_STONEGATE; // 0x0e4c — incondicional
}

/** Descriptor del efecto de un hechizo (parámetros EXACTOS del binario). */
export type CastEffect =
  | { kind: "light"; mins: number }
  | { kind: "timeStatus"; status: string; turns: number }
  | { kind: "wind"; windCode: number }
  | { kind: "food"; amount: number }
  | { kind: "healTarget"; mode: "mani" | "full" }
  | { kind: "cure" }
  | { kind: "awaken" }
  | { kind: "resurrect" }
  | { kind: "combatAttack"; weaponId: number }
  | { kind: "fieldWall"; arg: number; fieldTile: number; combatWeapon: number }
  | { kind: "blink" }
  | { kind: "sealDoor" }
  | { kind: "dispelField" }
  | { kind: "poof" }
  | { kind: "disarmOrOpen" }
  | { kind: "repelUndead" }
  | { kind: "massFear" }
  | { kind: "charm" }
  | { kind: "polymorphRat" }
  | { kind: "invisibilitySelf" }
  | { kind: "revealInvisible" }
  | { kind: "peer" }
  | { kind: "deathVision" }
  | { kind: "summonAlly"; monsterType: number }
  | { kind: "summonSwarms"; monsterType: number; maxCount: number }
  | { kind: "summonDaemon"; alwaysAlly?: boolean }
  | { kind: "earthquake" }
  | { kind: "lineAoe"; mode: number; len: number }
  | { kind: "gateTravel" }
  | { kind: "dungeonAscend" }
  | { kind: "dungeonDescend" }
  | { kind: "illusion" }
  | { kind: "castAnimOnly" };

export type CastResult = {
  ok: boolean;
  message: string;
  effect: CastEffect | null;
  /**
   * true cuando el dispatcher CONSUMIÓ maná y el hechizo mezclado pero no hubo
   * efecto (falta de nivel, o maná insuficiente tras el consumo). Ambos gates
   * caen al TAIL común (CAST:0x11a6, result=0) → imprimen "Failed!" (el de maná
   * precedido de "M.P. too low!"). Reproduce el castigo del original (divergencias #6/#8).
   */
  consumed: boolean;
};

/**
 * Traduce el índice de hechizo a su descriptor de efecto — la JUMP TABLE de
 * CAST:0x1146 (48 entradas). Los parámetros son los del stub que precede a cada
 * handler. `ctx` aporta la dirección de apunte/viento cuando el hechizo la lee.
 */
function effectFor(def: SpellDef, ctx: CastContext, rng: CombatRng, state: GameState): CastEffect {
  switch (def.index) {
    case 0: // In Lor
      return { kind: "light", mins: LIGHT_MINUTES_IN_LOR };
    case 12: // Vas Lor
      return { kind: "light", mins: LIGHT_MINUTES_VAS_LOR };
    case 1: // Grav Por
      return { kind: "combatAttack", weaponId: SPELL_WEAPON_GRAV_POR };
    case 13: // Vas Flam
      return { kind: "combatAttack", weaponId: SPELL_WEAPON_VAS_FLAM };
    case 37: // Xen Corp
      return { kind: "combatAttack", weaponId: SPELL_WEAPON_XEN_CORP };
    case 2: // An Zu — despierta ('S'→'G')
      return { kind: "awaken" };
    case 3: // An Nox — cura veneno ('P'→'G')
      return { kind: "cure" };
    case 4: // Mani — cura rand30() cap maxHP
      return { kind: "healTarget", mode: "mani" };
    case 27: // Vas Mani — cura total (HP=maxHP)
      return { kind: "healTarget", mode: "full" };
    case 5: // An Ylem — "POOF!" destruye terreno/campo apuntado
      return { kind: "poof" };
    case 6: // An Sanct — quita cerrojo / abre cofre de mazmorra / desarma trampa
      // (ficha #286, CAST.OVL:0x02d2). Consumidores: pueblo/exterior →
      // main.ts pendingCastUnlock → Game.applyUnlockSpell (rama de PUERTA
      // 0x03c0-0x03db + fall-through a la TABLA DE OBJETOS 0x03de-0x0432, que
      // desde #103 corre sobre el pool unificado: desarma la trampa del
      // cofre-objeto apuntado, `and [si+5],0x7f`); mazmorra → main.ts
      // doDungeonCast → Game.applyAnSanctOpenChest (rama 0x02ee-0x0395). En
      // combate el descriptor cae al no-op declarado de combat.ts (la arena no
      // comparte la tabla; ver pool-103-unificacion-acta.md §5).
      return { kind: "disarmOrOpen" };
    case 7: // An Xen Corp — repele no-muertos
      return { kind: "repelUndead" };
    case 8: // Rel Hur — cambia el viento
      // NOTA (nuance para 3.7): el binario (CAST2:0x040a) distingue CANCELAR el
      // apunte (di<0 → no toca g_wind) de Space=calma (windArrow 0 → escribe 0).
      // Aquí windArrow undefined/0 escribe calma; el lado que provea el apunte
      // debe pasar el caso "cancelado" como no-cast, no como windArrow 0.
      return { kind: "wind", windCode: ARROW_TO_WIND[ctx.windArrow ?? 0] ?? 0 };
    case 9: // In Wis — localiza: coordenadas de SEXTANTE (consumidor: main.ts doCast
      // → inWisPeerText; ficha #319, acta re/notes/hechizos-inertes-319.md §3)
      return { kind: "peer" };
    case 10: // Kal Xen — invoca 1 aliado (tipo por rand(0,15))
      return { kind: "summonAlly", monsterType: kalXenSummonType(rng.rand0(0x0f)) };
    case 11: // In Xen Mani — crea comida rand(1,3)
      return { kind: "food", amount: rng.randRange(1, 3) };
    case 14: // In Flam Grav — muro de fuego
    case 15: // In Nox Grav — muro de veneno
    case 16: // In Zu Grav — muro de sueño
    case 20: { // In Sanct Grav — muro de energía
      const arg = def.index === 20 ? 3 : def.index - 14;
      return { kind: "fieldWall", arg, fieldTile: FIELD_WALL_TILE[arg]!, combatWeapon: FIELD_WALL_COMBAT_WEAPON[arg]! };
    }
    case 17: // In Por — blink
      return { kind: "blink" };
    case 18: // An Grav — disipa campo (consumidor de la rama g_location<0x80:
      // main.ts doDungeonCast → Game.applyAnGravDispel; ficha #319 §4. La rama de
      // COMBATE (CAST2:0x0866, barrido del pool 0x5c5a) está cableada en
      // Combat.castDispelField — cierre de cargador-ticket-amplio §6.)
      return { kind: "dispelField" };
    case 19: // In Sanct — protección
      return { kind: "timeStatus", ...TIME_STATUS.protection };
    case 29: // Rel Tym — quickness
      return { kind: "timeStatus", ...TIME_STATUS.quickness };
    case 31: // Quas An Wis — confusión
      return { kind: "timeStatus", ...TIME_STATUS.confusion };
    case 32: // In An — negate magic
      return { kind: "timeStatus", ...TIME_STATUS.negate };
    case 47: // An Tym — time stop
      // PRECONDICIÓN NO MODELADA (CAST:0x0d60-0d9b): el handler escanea la tabla
      // de objetos (0x5c5a, 32×8) y si ALGÚN objeto tiene tile 0xFC imprime
      // "Magic absorbed!" (DS:0x45f2) y FALLA sin aplicar el time-stop (maná y
      // hechizo YA gastados por el dispatcher). Requiere el modelo de objetos de
      // mapa/combate; se cierra en la integración. Ver re/notes/magic.md §4.
      return { kind: "timeStatus", ...TIME_STATUS.timeStop };
    case 21: // Uus Por — sube 1 nivel de mazmorra
      return { kind: "dungeonAscend" };
    case 22: // Des Por — baja 1 nivel de mazmorra
      return { kind: "dungeonDescend" };
    case 23: // Wis Quas — revela invisibles
      return { kind: "revealInvisible" };
    case 24: // In Bet Xen — invoca hasta 4 enjambres aliados
      return { kind: "summonSwarms", monsterType: 0x1f, maxCount: 4 };
    case 25: // An Ex Por — SELLA una puerta (CAST:0x1020 → sub 0x846): getdir +
      // efecto, y sobre el tile apuntado 0xB8/0xB9→0x97, 0xBA/0xBB→0x98 (magifica
      // la cerradura); NO consume skull key. Verificado en re/disasm/CAST.OVL.asm
      // 0x846-0x8ab — los dos saltos son `jbe` (0x0880, 0x088a), así que las
      // variantes IMPARES 0xB9/0xBB entran: son CUATRO tiles, no dos.
      // El mapeo NO vive aquí: este case sólo devuelve el descriptor. Lo hace
      // `game.ts::applyDoorSpell`, y tiene las cuatro. (Ficha #48: la frase que
      // había aquí —«el clon mapea 0xB8→0x97 / 0xBA→0x98»— describía el estado
      // ANTERIOR a `54198c51` y encima atribuía a este fichero un mapeo que no
      // hace; junto con la tabla muerta de `tables.ts` es lo que hizo publicable
      // una divergencia que no existe.)
      return { kind: "sealDoor" };
    case 26: // In Ex Por — NO abre cerraduras mágicas. Su handler real (CAST:0x1026)
      // es getdir + animación de casteo (efecto 5, kernel 0xffffc186) y NADA MÁS:
      // ni toca g_skull_keys ni cambia tiles (verificado en re/disasm/CAST.OVL.asm
      // 0x1026-0x1037 → tail 0xf2a). Lo que abre las skull doors es (U)se → Skull
      // Key (CAST:0x18c4, ver game.ts::useSkullKey). La atribución previa de 0x18c4
      // a este hechizo era ERRÓNEA (task #22; docs/superpowers/specs/
      // 2026-07-13-finding-skull-key-use.md). Modelado como no-op: sólo animación.
      return { kind: "castAnimOnly" };
    case 28: // In Zu — dormir en línea (mode 1, len 2)
      return { kind: "lineAoe", mode: 1, len: 2 };
    case 40: // In Nox Hur — viento venenoso en línea (mode 2, len 1)
      return { kind: "lineAoe", mode: 2, len: 1 };
    case 45: // In Flam Hur — fuego en línea (mode 3, len 2)
      return { kind: "lineAoe", mode: 3, len: 2 };
    case 44: // In Vas Grav Corp — muerte en línea (mode 4, len 1)
      return { kind: "lineAoe", mode: 4, len: 1 };
    case 30: // In Vas Por Ylem — terremoto
      return { kind: "earthquake" };
    case 33: // Wis An Ylem — death vision (consumidor: main.ts doCast →
      // view.revealViewport, 20 fotogramas de ventana SIN censura; ficha #319 §5)
      return { kind: "deathVision" };
    case 34: // An Xen Ex — charm 1 enemigo
      return { kind: "charm" };
    case 35: // Rel Xen Bet — polymorph a rata
      return { kind: "polymorphRat" };
    case 36: // Sanct Lor — invisibilidad (self)
      return { kind: "invisibilitySelf" };
    case 41: // In Quas Corp — miedo/huida en masa
      return { kind: "massFear" };
    case 42: // In Mani Corp — resurrección
      return { kind: "resurrect" };
    case 43: // Kal Xen Corp — invoca daemon
      return { kind: "summonDaemon" };
    case 46: // Vas Rel Por — gate travel
      return { kind: "gateTravel" };
    case 38: // In Quas Xen — clona la criatura APUNTADA (CAST.OVL:0x0b28, ficha #340).
      // Prompt "Creature: " (DS 0x45dc) + cursor de apuntado; el clon es una COPIA del
      // registro del objetivo colocada en celda de tablero al azar. Consume RNG: ver
      // `castIllusion` (combat.ts) para la derivación completa y el consumo.
      return { kind: "illusion" };
    case 39: // In Quas Wis — sólo animación de casteo (sin efecto adicional conocido)
      return { kind: "castAnimOnly" };
    default:
      // Nox (48) y cualquier índice fuera de la jump table (guard CAST:0x0f0f).
      return { kind: "castAnimOnly" };
  }
}

/** Aplica los efectos GLOBALES sin objetivo directamente a `state`. */
function applyGlobal(state: GameState, effect: CastEffect): void {
  switch (effect.kind) {
    case "light":
      state.lightSpellMins = effect.mins; // CAST2:0x08ea escribe, no suma
      break;
    case "timeStatus":
      state.timeSpell = effect.status;
      state.timeSpellTurns = effect.turns;
      break;
    case "wind":
      // Rel Hur desemboca en set_wind (kernel 0x2E96), único escritor de
      // g_wind: escribe el código y RESETEA el contador de deriva (0x2EA5).
      state.wind = effect.windCode;
      state.windDriftCtr = 0;
      break;
    case "food":
      state.food = Math.min(9999, state.food + effect.amount);
      break;
    default:
      break;
  }
}

export function castSpell(
  state: GameState,
  caster: CharacterState,
  def: SpellDef,
  ctx: CastContext,
  rng: CombatRng,
): CastResult {
  const circle = def.circle;

  // 1. VENTANA TEMPORAL (CAST:0x0e1a) — antes de todo lo demás.
  // "Absorbed!": el binario lo evalúa DENTRO del gate de ubicación, en la rama de
  // loc 0x01..0x7F (0x0e3e y 0x0e4c) — o sea después de despachar exterior (loc==0) y
  // combate (loc>0x7F), y antes del test de bit pueblo/mazmorra. Las DOS reglas:
  //   loc 0x12 Palacio de Blackthorn → absorbe **si NO llevas la Corona** (`g_crown==0`)
  //   loc 0x1D Stonegate            → absorbe **siempre**, sin condición
  // ⚠️ ERRATA 07-08: aquí decía «loc==0x12 CON corona, o loc==0x1D = trono de LB» —
  // la condición de la corona estaba INVERTIDA y 0x1D no es el trono. Ver CastContext.
  // CABLEADO (antes: declarado y leído SIN PRODUCTOR, o sea inexistente fuera de combate).
  // El productor es `castAbsorbedOutOfCombat` y vive AQUÍ, no en el llamador, para que no
  // pueda volver a quedarse sin escritor; `ctx.magicAbsorbed` sigue mandando si se fija.
  // El orden de este `if` respecto al bit de ubicación ya era el del binario para pueblos.
  const absorbed =
    ctx.magicAbsorbed ?? castAbsorbedOutOfCombat(ctx.location, ctx.inCombat, !!state.lbArtifacts?.crown);
  if (absorbed) {
    return { ok: false, message: "Absorbed!", effect: null, consumed: false };
  }
  const need = requiredTimeBit(ctx.location, ctx.inCombat);
  const allowed = (TIME_PERMITTED_BITS[def.index] ?? 0) & need;
  if (!allowed) {
    return { ok: false, message: "Not here!", effect: null, consumed: false };
  }

  // 2. ¿Conoce el hechizo? (cantidad mezclada > 0). Nox no tiene slot → 0.
  const known = state.spellQuantities[def.index] ?? 0;
  if (known <= 0) {
    return { ok: false, message: "None mixed!", effect: null, consumed: false };
  }

  // 3. CONSUME el hechizo mezclado ANTES del check de maná (CAST:0x0ec8).
  state.spellQuantities[def.index] = known - 1;

  // 4. Maná suficiente (coste = círculo). Si falta, el hechizo YA se gastó.
  //    El binario imprime "M.P. too low!" (DS 0x4647, CAST:0x0ede call 0x58d0) y ADEMÁS cae
  //    al TAIL común (0x11a6) con result=0 → "Failed!" (DS 0x4660). El "Failed!" del tail lo
  //    emite el llamador con la señal `!ok && consumed` (como el resto del tail Success!/Failed!,
  //    ya en main.ts). Verificado re/disasm/CAST.OVL.asm 0ed3-0eea + tail 0x11a6 realineado; el
  //    gameplay real lo confirma (video-diff spec "too low! / Failed!").
  if (caster.currentMp < circle) {
    return { ok: false, message: "M.P. too low!", effect: null, consumed: true };
  }

  // 5. COSTE DE MANÁ = círculo (CAST:0x0ef8).
  caster.currentMp -= circle;

  // 6. GATE DE NIVEL: nivel >= círculo (CAST:0x0f01). El gate en sí NO imprime, pero pone
  //    result=0 (0f0a jb→0ee5 [bp-0xa]=0) y cae al TAIL común (0x11a6), que con result=0
  //    imprime "Failed!" (DS 0x4660). NO es silencioso — corrige la derivación previa
  //    (magic.md línea ~31, "salir SILENCIOSO"). El "Failed!" lo emite el llamador con la
  //    señal `!ok && consumed`. Verificado re/disasm/CAST.OVL.asm 0f01-0f14 + 0x11a6.
  if (caster.level < circle) {
    return { ok: false, message: "", effect: null, consumed: true };
  }

  // 7. Efecto exacto (jump table CAST:0x1146).
  const effect = effectFor(def, ctx, rng, state);
  applyGlobal(state, effect);
  // El binario NO imprime el nombre del hechizo al castear (CAST:0x11a6): el éxito de
  // los hechizos GLOBALES (In Lor, Rel Hur, In Sanct…) es SILENCIOSO (el handler no toca
  // el código de resultado [bp-0xa]=0xffff → el tail no imprime nada); los TARGETED
  // (heal/cure/awaken/resurrect) caen al tail con result 1/0 → "Success!"/"Failed!"
  // GENÉRICO desde la capa de efecto (main.ts). "Resurrection!" (DS 0x46d2) es del lector
  // de pergaminos (0x11de), NO del Cast. El "{nombre}!" era FABRICADO (lote cast-echo; el
  // eco al jugador es la palabra RÚNICA tecleada, no el nombre legible). Éxito silencioso.
  return { ok: true, message: "", effect, consumed: true };
}

// ---------------------------------------------------------------------------
// Helpers de APLICACIÓN de efectos con objetivo (curación/estado a 1 PJ).
// Los invoca el aplicador tras elegir el PJ objetivo (picker del original).
// ---------------------------------------------------------------------------

/**
 * In Wis — CAST2.OVL:0x06ec (ficha #319; acta re/notes/hechizos-inertes-319.md §3,
 * cuerpo re-careado línea a línea el 16-08 sobre este árbol): emisión de texto PURA
 * que localiza al grupo en notación de SEXTANTE. La secuencia exacta del binario:
 *
 *   set_font(1) → '\n' → 'A'+(y>>4) → '\'' → 'A'+(y&0xf) → `", ` (DS 0x9548, crudo
 *   `22 2c 20 00`) → 'A'+(x>>4) → '\'' → 'A'+(x&0xf) → '"' → set_font(0) → '\n'
 *
 * Y primero (0x06fa), X después (0x072a) — el orden es del binario, no decorativo.
 * Cada nibble es una letra 'A'..'P' (0x41+0..15). CERO llamadas a rand_range en las
 * doce emisiones: In Wis NO mueve stream.
 *
 * 🔴 FUENTE (dato colateral de medicion-364c): todo glifo imprimible sale con la
 * fuente 1 (RÚNICA — `text_set_font`, kernel 0x1c9e; el push 1 va ANTES del primer
 * '\n' y el push 0 va DESPUÉS del '"'): tras la vuelta a IBM el binario sólo imprime
 * LF, que no pinta glifo. Por eso el consumidor puede usar el `rune` POR FILA de
 * `pushConsole` sin la cirugía por-tramo que #364-c dejó declarada: aquí no hay
 * tramo mixto en ninguna fila.
 *
 * Devuelve el texto CON los dos '\n' del binario: `pushConsole` modela el cursor
 * (#108) y el par produce exactamente la fila en blanco previa + el cierre de fila.
 */
export function inWisPeerText(x: number, y: number): string {
  const nib = (v: number): string => String.fromCharCode(0x41 + (v & 0x0f));
  const hi = (v: number): string => nib((v & 0xf0) >> 4);
  return `\n${hi(y)}'${nib(y)}", ${hi(x)}'${nib(x)}"\n`;
}

/**
 * Wis An Ylem — nº de fotogramas del revelado (CAST2.OVL:0x049d `mov si, 0x14`):
 * VEINTE vueltas de { tick condicional + repintado + espera de 1 fotograma }.
 * Consumidor: main.ts (doCast) → CoreViewImpl.revealViewport. Ficha #319 §5.
 */
export const DEATH_VISION_FRAMES = 0x14;

/**
 * Mani — CAST2:0x03c2: `HP = min(HP + rand30(), maxHP)`; no cura muertos ('D').
 * CONSUME 1 rand. Devuelve la cantidad curada (0 si era muerto).
 */
export function applyMani(target: CharacterState, rng: CombatRng): number {
  if (target.status === "D") return 0;
  const heal = rng.rand30();
  const before = target.currentHp;
  target.currentHp = Math.min(target.maxHp, target.currentHp + heal);
  return target.currentHp - before;
}

/**
 * An Sanct, rama de PUERTA — `CAST.OVL:0x02d2`, cuerpo 0x03c0-0x03db.
 *
 * ```
 * 03c0: cmp al,0xb9 ; je 0x3c8
 * 03c4: cmp al,0xbb ; jne 0x3de      ← si no es ninguna, cae al barrido de objetos
 * 03c8: dec byte [bx]                ← EL EFECTO ENTERO: un decremento
 * 03ca: or byte [g_unk_24e6],2       ← turno consumido
 * ```
 *
 * 🔴 ES UN `dec`, NO UN «ABRIR». 0xB9→0xB8 y 0xBB→0xBA: la puerta queda **cerrada y sin
 * cerrojo**, y sigue haciendo falta (O)pen. Durante meses DOS citas del ledger glosaron
 * esto como «(puerta cerrada/mágica) → dec = puerta abierta», con los dos términos mal:
 *   · 0xB9 = puerta CON CERROJO DE LLAVE; 0xBB = su variante CON VENTANA.
 *     La MÁGICA es 0x97/0x98 y la abre (U)se Skull Key (`unmagicDoorTile`, game.ts:3138)
 *     o la sella An Ex Por (`applyDoorSpell`, game.ts:3209). An Sanct **no las toca**.
 *   · 0xB8/0xBA no son «abierta»: son la puerta cerrada sin cerrojo. Canal del BINARIO,
 *     de dos filas ya selladas — `cmd_search` (SJOG.OVL:0x095c) ESCRIBE 0xB9/0xB8 al
 *     revelar una puerta secreta (una puerta recién revelada está cerrada), y `cmd_fire`
 *     (CMDS.OVL:0x0aea) para el proyectil en `0x97..0x99` O `0xB8..0xBB` con DS 0x42fa
 *     'Door destroyed!' (una abierta no lo pararía, y las dos familias van separadas).
 *
 * Devuelve el tile resultante, o `null` si An Sanct no actúa sobre ese tile.
 * NO consume RNG: el cuerpo del binario está medido a CERO tiradas hasta profundidad 2.
 * Defendido por `game/tests/an-sanct-unlock.test.ts`.
 */
export function unlockDoorTile(tile: number): number | null {
  const LOCKED = 0xb9, LOCKED_VIEW = 0xbb;
  if (tile !== LOCKED && tile !== LOCKED_VIEW) return null;
  return tile - 1; // `dec byte [bx]` @0x03c8 — literal, no una tabla
}

/** Vas Mani — CAST:0x08ac: HP = maxHP (curación total); no cura muertos. */
export function applyVasMani(target: CharacterState): boolean {
  if (target.status === "D") return false;
  target.currentHp = target.maxHp;
  return true;
}

/** An Nox — CAST:0x01ae: 'P' (envenenado) → 'G'. Sólo si estaba envenenado. */
export function applyCure(target: CharacterState): boolean {
  if (target.status !== "P") return false;
  target.status = "G";
  return true;
}

/** An Zu — CAST:0x0114: 'S' (dormido) → 'G'. Sólo si estaba dormido. */
export function applyAwaken(target: CharacterState): boolean {
  if (target.status !== "S") return false;
  target.status = "G";
  return true;
}

/**
 * Poción VERDE — CAST.OVL 0x1460 (bebedor 0x135a): 'G' (sano) → 'P' (envenenado).
 * ENVENENA por diseño; sólo surte efecto sobre un PJ sano. Ver re/notes/potions-scrolls.md.
 */
export function applyPoison(target: CharacterState): boolean {
  if (target.status !== "G") return false;
  target.status = "P";
  return true;
}

/**
 * Poción NARANJA — CAST.OVL 0x1478 (bebedor 0x135a, fuera de combate): 'G' (sano) → 'S'
 * (dormido). Sólo sobre un PJ sano. (En combate — g_location >= 0x80 — el binario llama a
 * ULTIMA.EXE 0x68AE desde CAST.OVL 0x1497, que escribe la 'S' en 0x68de; el (U)se nunca
 * está en combate, y esa rama de CAST.OVL 0x148f la escribe en línea.)
 * Ver re/notes/potions-scrolls.md.
 */
export function applySleep(target: CharacterState): boolean {
  if (target.status !== "G") return false;
  target.status = "S";
  return true;
}

/**
 * In Mani Corp — CAST2:0x05e0: resurrección EXACTA (sólo status 'D').
 * status→'G', HP=1, MP por clase (A/M = INT, B = INT>>1), si karma<98
 * `exp = floor(exp·karma/100)`, nivel = resurrectionLevel(exp),
 * maxHP = 30·nivel. Devuelve true si resucitó.
 */
export function applyResurrect(target: CharacterState, karma: number): boolean {
  if (target.status !== "D") return false;
  target.status = "G";
  target.currentHp = 1;
  if (target.class === "A" || target.class === "M") {
    target.currentMp = target.intelligence;
  } else if (target.class === "B") {
    target.currentMp = target.intelligence >> 1;
  }
  if (karma < 98) {
    target.exp = Math.floor((target.exp * karma) / 100);
  }
  const level = resurrectionLevel(target.exp);
  target.level = level;
  target.maxHp = resurrectionMaxHp(level);
  return true;
}
