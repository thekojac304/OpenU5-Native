/**
 * Guardias, cárcel y Blackthorn — reglas EXACTAS del binario DOS (BLCKTHRN.OVL +
 * los enganches de kernel/TOWN/TALK). Task 3.10.
 *
 * El clon original NO modelaba NADA de este subsistema; estas son las reglas del
 * binario, no una aproximación:
 *  - Captura DETERMINISTA en el Palacio de Blackthorn (loc 0x12) con el party no
 *    completamente muerto: escena de interrogatorio sobre el mantra de un
 *    santuario todavía en pie (BLCKTHRN:0x060e `blackthorn_capture`, disparada por
 *    el town-turn TOWN:0x12ae). Ceder el mantra = traición: karma −5, el santuario
 *    se marca caído, y (si quedaba >1 vivo) un compañero es ejecutado.
 *  - "Refuge" de party-wipe (BLCKTHRN:0x0910 `party_refuge`): cuando TODO el party
 *    está muerto (kernel 0x39fc == −1) NO es game-over — despiertas en el castillo
 *    de Lord British con el party revivido y el karma restaurado a 75.
 *  - Peaje/tributo/password de guardias (TALK.OVL:0x01e2 dialogNum 0xFF), gated
 *    por g_location.
 *  - Merma de oro de la Falsedad (SHOPPES:0x019a): con Falsehood (Shadowlord 0) en
 *    la ciudad, cada compra sisa `rand(1,64)` de oro.
 *
 * NINGUNA rama consume RNG salvo la merma de la Falsedad y la alarma cosmética; la
 * captura/interrogatorio/refuge son deterministas por posición y estado. Citas asm
 * en re/notes/blackthorn.md; globals en re/notes/globals.md.
 */
import type { CharacterState, GameState } from "../state.js";

/** location del Palacio de Blackthorn (SingleMapReference 18). */
export const LOC_BLACKTHORN = 0x12;
/** location del castillo de Lord British — punto de despertar del refuge (17). */
export const LOC_LORD_BRITISH = 0x11;
/** location de Minoc — el guardia "half thy gold to charity" (5). */
export const LOC_MINOC = 5;

/** g_transport_tile a pie (BLCKTHRN capture 0x064a / refuge 0x0c13). */
export const TILE_FOOT = 0x1c;
/** Número de santuarios/virtudes. */
export const SHRINE_COUNT = 8;
/** Karma topa en 0x63 (99) como en todo el binario. */
export const KARMA_MAX = 0x63;
/** El refuge restaura el karma a este suelo (BLCKTHRN 0x0bfd: `if karma<0x4b: karma=0x4b`). */
export const REFUGE_KARMA_FLOOR = 0x4b; // 75
/** Traición: karma −= 5 con suelo 0 (kernel 0x3f36 sub_floor sobre g_karma). */
const BETRAYAL_KARMA_PENALTY = 5;
/** Tributo de guardia por miembro vivo (TALK 0x0230: Σ 0xa por miembro). */
const TRIBUTE_PER_MEMBER = 0xa; // 10 gp
/** Password del guardia del Palacio (DS:0x4A9A = "IMPE\0"; TALK 0x02e0 strcmp). */
export const BLACKTHORN_PASSWORD = "IMPE";

/**
 * DOMINIO de `g_time_spell` (DS 0x587a) — UN SOLO BYTE, valores mutuamente excluyentes.
 * Censo por bytes: 9 escrituras, 33 lecturas, todas contra el mismo byte. No son estados
 * independientes: escribir uno PISA al anterior.
 *  · 0x1d Black Badge puesta (escrito por CAST.OVL 0x1b47; leído por TALK.OVL 0x02a4)
 *  · 0x0e Amuleto de Lord British (leído por MAINOUT.OVL 0x0a31)
 *  · 'T'/'Q'/'N'/'C'/'P' efectos temporales · 0/vacío = ninguno (ULTIMA.EXE 0x2bc2).
 */
export const TIME_SPELL_BADGE = "\x1d";
/** Amuleto de Lord British — el MISMO byte que la insignia, otro valor (0x0e). */
export const TIME_SPELL_AMULET = "\x0e";
/**
 * Corona de Lord British PUESTA — el MISMO byte, valor 0x1c (A4-PARITY1, P1b). Escrito
 * por (U)se Corona: CAST.OVL 0x193e → toggle 0x1764(0x1c) → set_time_spell(anim 9,
 * turnos 0xff, 0x1c) vía el thunk ULTIMA.EXE 0x80b2 → CAST2 0x08f8. Leído por COMBAT
 * 0x0196 / 0x0f36 y COMSUBS 0x0112, siempre junto a Negate 'N'. NO es la posesión: el gate
 * de "Absorbed!" del Palacio lee `g_crown` [0x57b4] (= `lbArtifacts.crown`).
 */
export const TIME_SPELL_CROWN = "\x1c";
/** Turnos de un efecto PERMANENTE (`g_time_spell_turns` = 0xFF): no lo toca el decay. */
export const TIME_SPELL_PERMANENT = 0xff;
/**
 * Tipo de NPC del guardia del Palacio (npcs.json loc 18 slots 8-15, `type` 112).
 * El binario lo comprueba como el byte-bajo del tile del objeto en la tabla
 * g_world_objects (0x5C5A): npc_engine TOWN 0x13a7 `cmp byte[bx],0x70` (sprite del
 * guardia = type+0x100 = 0x170; byte bajo = 0x70). Es el gate del ATAQUE que
 * dispara la captura (§ trigger real).
 */
export const PALACE_GUARD_TYPE = 0x70;

// ---------------------------------------------------------------------------
// Estado de consciencia del party — kernel 0x39FC `party_conscious_state`
// ---------------------------------------------------------------------------

/**
 * Réplica de kernel 0x39fc (§2.2). Recorre el status (record+0 =
 * 'G'ood/'P'oison/'S'leep/'C'harmed/'D'ead) de los `g_party_size` primeros miembros
 * del roster — los que van EN EL GRUPO, no el roster entero (0x3a0e lee
 * `[g_party_size]`, early-out `or ax,ax / je 0x3a5e` @0x3a15, cierre `cmp ax,[bp-8]`
 * @0x3a50; NO hay `cmp si,6`):
 *  -  0 si HAY ≥1 miembro CONSCIENTE ('G' o 'P'),
 *  -  1 si nadie consciente pero ≥1 DORMIDO/KO ('S'),
 *  - −1 si nadie consciente ni dormido (todo el GRUPO muerto — un compañero muerto
 *    que se quedó FUERA del grupo no cuenta, y uno vivo fuera del grupo NO salva).
 * Es el gate tanto de la captura (>=0 dispara) como del refuge (==−1 dispara).
 */
export function partyConsciousState(state: GameState): number {
  let sleeping = 0;
  const n = state.partySize ?? state.characters.length;
  for (let i = 0; i < n; i++) {
    const s = state.characters[i]?.status;
    if (s === "G" || s === "P") return 0; // consciente
    if (s === "S") sleeping++;
  }
  return sleeping > 0 ? 1 : -1;
}

// ---------------------------------------------------------------------------
// Disparo de la captura — TOWN.OVL:0x12ae
// ---------------------------------------------------------------------------

/**
 * ¿Se dispara `blackthorn_capture()`? (TOWN 0x12b9-0x12ca). Estar en el Palacio
 * de Blackthorn (loc 0x12) con el party no completamente muerto
 * (party_conscious_state >= 0). Determinista, sin RNG.
 */
export function blackthornCaptureTriggers(state: GameState): boolean {
  return state.position.location === LOC_BLACKTHORN && partyConsciousState(state) >= 0;
}

/** Manhattan `|dx|+|dy|` (NPC.OVL 0x06A0 dist_manhattan; ver npc.md §3.1). */
function manhattan(x1: number, y1: number, x2: number, y2: number): number {
  return Math.abs(x2 - x1) + Math.abs(y2 - y1);
}

/** Un guardia (posición + tipo + planta) tal como lo expone el gestor de NPCs. */
export interface GuardActor {
  x: number;
  y: number;
  /** `type` del slot .NPC (guardia del Palacio = 112 = 0x70). */
  type: number;
  /** planta (z) del guardia. */
  floor: number;
  /**
   * Índice del slot .NPC (1..0x1F). ⚠ NO es decorativo: `[0x65bf]` guarda UN solo
   * índice y el bucle `NPC.OVL 0x0db4,` lo recorre ASCENDENTE, así que **el índice
   * mayor PISA al menor**. Sin este campo el port no puede expresar la absorción.
   * Opcional para no romper los call-sites que sólo prueban la adyacencia; ausente
   * se trata como 0 (pierde todos los desempates).
   */
  slot?: number;
  /** `aiTypes[schedIdx]` del NPC. Sólo `> 3` arma el slot (`0x072c,` `jle`). */
  aiType?: number;
  /** `dialogNumber`: los aiType 4/5 sólo arman con `!= 0` (`0x0740,` `je`). */
  dialogNumber?: number;
}

/**
 * ¿Este actor ARMA el slot `[0x65bf]` estando adyacente? Calco de `NPC.OVL`:
 *   `0x072c,` `cmp word ptr [bp - 2], 3` / `jle` → aiType <= 3 NO arma (se va al
 *   bucle de movimiento) · `0x073d,`-`0x0744,` aiType 4/5 exigen `dlgNum != 0`
 *   (marcador 0x74 en `0x0746,`) · aiType 6/7 arman SIN gate de diálogo (marcador
 *   0x61 en `0x07be,`, que cae en el mismo `mov byte ptr [0x65bf], al` por el
 *   `jmp 0x74b` de `0x07c3,`).
 *
 * ★ EL `aiType` QUE ENTRA AQUÍ ES EL DE LA RANURA HORARIA, no un campo estático
 * (#64, derivación entera en `re/notes/horaria-64-adjudicacion.md`). El binario lee
 * `byte [0x5d5e + npcIndex*16 + RANURA]` (`0x0703,`), y la RANURA sale de
 * `NPC.OVL 0x12e0,` — `argmin((hora - times[k]) & 0xff)` con remapeo 3→1 — que
 * recibe `g_hour` desde `TOWN.OVL 1668,`. El port calca ese selector en
 * `scheduleIndex` (`core/time.ts`) y lo aplica en `palaceGuards()`
 * (`blackthorn-capture.ts`) y en `checkGuardTribute` (`guard-encounters.ts`).
 * Consecuencia medible: los ocho guardias del Palacio valen `aiType 0` en su ranura
 * 0, así que NINGUNO captura entre las 21:00 y las 04:59.
 *
 * Un actor SIN `aiType` declarado NO arma (defecto conservador, `?? 0`). El
 * `undefined` no existe en el binario: es territorio de actores sintéticos, y este
 * defecto es el MISMO que aplica `guard-encounters.ts` — antes divergían y esa
 * asimetría era el defecto, no una decisión.
 */
function armaElSlot(g: GuardActor): boolean {
  const ai = g.aiType ?? 0;
  if (ai <= 3) return false; // 0x072c `cmp word ptr [bp - 2], 3` / `jle`
  if (ai === 4 || ai === 5) return (g.dialogNumber ?? 0xff) !== 0; // 0x0740 `je`
  return true; // 6/7 — hostiles, sin gate de diálogo (marcador 0x61)
}

/**
 * ¿Hay un guardia del Palacio (type 0x70) ORTOGONALMENTE adyacente al party en su
 * misma planta?
 *
 * ADYACENCIA = PRECONDICIÓN NECESARIA de la captura (asm-derivada): un NPC sólo se
 * vuelve el "activo" `[0x65bf]` que habilita el gate de `npc_engine` por el FAST-PATH
 * de `npc_target_for_attack` (NPC.OVL 0x06E4, 0x0723 `cmp ax,1` sobre `dist_manhattan`);
 * `[0x65bf]` tiene sólo 2 escrituras en NPC.OVL (0x074e =idx post-adyacencia, 0x0dc6 =0
 * reset). El loop de persecución (0x083d `rand(0,0x3f)`) no lo toca → no dispara la
 * captura (Clase C de movimiento; `manager.ts:fleeStep`).
 *
 * SUFICIENCIA = B-RUNTIME (testigo DOSBox 2026-07-14, re/notes/oracle-blackthorn.md)
 * y, desde 30-07, DERIVADA: el «handler opaco 0x1912» está RESUELTO — TOWN 0x13cf es
 * `call` al kernel 0x7AE2 (stub PLINK) → TALK 0x031E `talk_converse_dispatch`, leído
 * ENTERO (re/notes/talk-031e-resolucion.md). Para un guardia (dlgNum 0xFF) corre el
 * guard_demand TALK 0x1e2: sin insignia ret 1 SILENCIOSO → captura; con insignia,
 * reto de password EN LA INTERCEPCIÓN (tarjeta T-A del acta §5). La adyacencia basta
 * sin la insignia, que es el caso de este trigger.
 */
export function palaceGuardAdjacent(
  px: number,
  py: number,
  pfloor: number,
  guards: readonly GuardActor[],
): boolean {
  // El slot `[0x65bf]` es UNO y gana el ÍNDICE MAYOR: no basta con que haya un
  // guardia adyacente, hace falta que NINGÚN otro actor que ARME el slot y tenga
  // índice superior esté también adyacente. Por eso se busca el GANADOR y sólo
  // después se pregunta si es guardia — que es el orden del binario.
  let ganador: GuardActor | null = null;
  for (const g of guards) {
    if (g.floor !== pfloor) continue;
    if (manhattan(px, py, g.x, g.y) !== 1) continue; // 0x0723 `cmp ax,1` / `jne`
    if (!armaElSlot(g)) continue; // 0x072c / 0x0740
    if (ganador === null || (g.slot ?? 0) >= (ganador.slot ?? 0)) ganador = g; // PISA
  }
  return ganador !== null && (ganador.type & 0xff) === PALACE_GUARD_TYPE;
}

/**
 * Trigger de la captura de Blackthorn (npc_engine TOWN 0x1352 → 0x12ae). Dispara
 * cuando: (1) un guardia del Palacio (type 0x70) está ADYACENTE (manhattan==1) al
 * party en su planta — precondición NECESARIA derivada (§palaceGuardAdjacent) — y
 * (2) el gate de 0x12ae se cumple: loc 0x12 + party no del todo muerto
 * (party_conscious_state>=0, `blackthornCaptureTriggers`). Sustituye al gate
 * POR-TURNO aproximado de F1.7-T2 (que capturaba cada turno con sólo estar en loc
 * 0x12). Determinista, sin RNG.
 *
 * SUFICIENCIA: B-RUNTIME (testigo 2026-07-14) y DERIVADA desde 30-07 — la rama
 * npc_engine 0x13b4 con `result==1` llama a TALK 0x031E (el ex-«opaco 0x1912»,
 * resuelto y leído: re/notes/talk-031e-resolucion.md), que para el guardia corre el
 * guard_demand 0x1e2: sin insignia ret 1 → 0x12ae. El clon captura por adyacencia
 * sola, que calca el caso sin-insignia; el caso CON insignia (reto en la
 * intercepción) es la tarjeta T-A del acta.
 */
export function blackthornGuardCaptureTriggers(
  state: GameState,
  guards: readonly GuardActor[],
): boolean {
  return (
    blackthornCaptureTriggers(state) &&
    palaceGuardAdjacent(state.position.x, state.position.y, state.position.floor, guards)
  );
}

/**
 * Santuario por el que interroga Blackthorn (BLCKTHRN 0x0659): el PRIMER santuario
 * cuyo byte g_shrine_destroyed[i] es exactamente 0 (todavía en pie y no cedido).
 * Devuelve null si los 8 ya cayeron (0x0665: si>=8 → no hay interrogatorio).
 */
export function pickInterrogationShrine(state: GameState): number | null {
  const destroyed = state.shrineDestroyed ?? [];
  for (let i = 0; i < SHRINE_COUNT; i++) {
    if (((destroyed[i] ?? 0) & 0xff) === 0) return i;
  }
  return null;
}

// ---------------------------------------------------------------------------
// Matcher de mantra — BLCKTHRN:0x02ea `check_mantra`
// ---------------------------------------------------------------------------

/**
 * ¿La respuesta contiene el mantra? Búsqueda de SUBSTRING case-insensitive
 * (toupper) de hasta 14 chars (kernel 0x3b1c lee 14; 0x216c strlen; 0x2032
 * toupper). "AHM", "ahm ", "the ahm" cuentan como correcto para "Ahm".
 */
export function mantraMatches(input: string, mantra: string): boolean {
  if (!mantra) return false;
  return input.slice(0, 14).toUpperCase().includes(mantra.toUpperCase());
}

// ---------------------------------------------------------------------------
// Sacrificio de compañero — BLCKTHRN:0x03ae `sacrifice_member`
// ---------------------------------------------------------------------------

/**
 * Quita PERMANENTEMENTE al primer compañero (el 2º miembro VIVO, status != 'D';
 * BLCKTHRN 0x0438: cuenta vivos, cx==2 → víctima). El Avatar (miembro 0) nunca se
 * sacrifica. Compacta el roster y decrementa party_size. Devuelve el nombre de la
 * víctima (para el texto) o null si no había un 2º vivo. Sin RNG.
 *
 * Layout de save — el APARCAMIENTO EN EL SLOT 15 (cerrada la ex-Task F, carril
 * save-residuos): el binario 0x046c-0x047f copia el record entero (32 B, `repne movsw`
 * ×0x10) a un local; 0x0487 `cmp ax,0xf / jge` salta la compactación si victim==15;
 * 0x04ab-0x04c0 sube UNA posición los records victim+1..15 (bucle hasta `cmp si,0x57a8`
 * = fin de los 16 slots — TODOS los records, no sólo el party); 0x04c2-0x04cd copia el
 * record aparcado a DS:0x5788 (= g_party_records[15]); 0x04cf escribe el byte final
 * `[0x57A7]=0x7f` (offset 0x1F del record = partyStatus — 0x7f también RETIRA el
 * 0x00 «en party», no es sólo un marcador). El ejecutado PERSISTE ahí en la ventana
 * del save. El clon: `splice` = la compactación (el array lleva los 16 records) y la
 * asignación a [15] = el aparcamiento.
 */
export function sacrificeFirstCompanion(state: GameState): string | null {
  const n = state.partySize ?? state.characters.length;
  let living = 0;
  let victim = -1;
  for (let i = 0; i < n; i++) {
    if (state.characters[i]?.status !== "D") {
      living++;
      if (living === 2) {
        victim = i;
        break;
      }
    }
  }
  if (victim < 0) return null;
  const rec = state.characters[victim]!;
  const name = rec.name ?? "";
  state.characters.splice(victim, 1); // 0x04ab-0x04c0: compactación de los 16 slots
  state.characters[15] = rec; // 0x04c2-0x04cd: record entero aparcado en el slot 15
  rec.partyStatus = 0x7f; // 0x04cf: byte final [0x57A7] = 0x7f
  state.partySize = Math.max(0, n - 1);
  return name;
}

// ---------------------------------------------------------------------------
// Interrogatorio — BLCKTHRN:0x054a `interrogate`
// ---------------------------------------------------------------------------

/** Resultado del interrogatorio (efecto observable). */
export interface InterrogationResult {
  /** "betrayal" cediste el mantra; "pendulum" 4 fallos con party>1; "dungeon" solo el Avatar. */
  outcome: "betrayal" | "pendulum" | "dungeon";
  /** Índice del santuario marcado caído (solo betrayal), o null. */
  shrineCeded: number | null;
  /** Compañeros ejecutados (0 o 1). */
  sacrificed: number;
  /** true si el Avatar estaba solo y Blackthorn lo perdonó ("rewarded with thy life"). */
  rewardedWithLife: boolean;
  /** Minutos de reloj consumidos por las amenazas (advance_clock(2) por ronda tras el 1er aviso). */
  clockMinutes: number;
  /** Rondas planteadas antes de terminar (1..4). */
  roundsAsked: number;
}

/**
 * Bucle de interrogatorio de hasta 4 rondas (BLCKTHRN 0x054a). `responses` es la
 * secuencia de respuestas tecleadas (una por ronda). MUTA `state` (karma, santuario,
 * roster) igual que el binario. Reglas:
 *  - Acertar el mantra en CUALQUIER ronda = traición: g_shrine_destroyed[subj]=0xff,
 *    karma −5 (suelo 0) y, si numLiving>1, ejecuta un compañero (else "rewarded
 *    with thy life"). Termina.
 *  - Avatar solo (numLiving<2) fallando → "To the dungeon!" y termina, sin sacrificio.
 *  - party>1 fallando: 1er fallo = aviso; fallos siguientes escalan (advance_clock(2))
 *    y en la 4ª ronda (índice 3) el péndulo ejecuta un compañero ("treachery").
 */
export function runInterrogation(
  state: GameState,
  numLiving: number,
  subjIdx: number,
  mantra: string,
  responses: string[],
): InterrogationResult {
  let warned = false;
  let clockMinutes = 0;
  for (let round = 0; round < 4; round++) {
    const guess = responses[round] ?? "";
    if (mantraMatches(guess, mantra)) {
      const arr = (state.shrineDestroyed ??= []);
      arr[subjIdx] = 0xff;
      state.karma = state.karma <= BETRAYAL_KARMA_PENALTY ? 0 : state.karma - BETRAYAL_KARMA_PENALTY;
      if (numLiving > 1) {
        sacrificeFirstCompanion(state);
        return {
          outcome: "betrayal",
          shrineCeded: subjIdx,
          sacrificed: 1,
          rewardedWithLife: false,
          clockMinutes,
          roundsAsked: round + 1,
        };
      }
      return {
        outcome: "betrayal",
        shrineCeded: subjIdx,
        sacrificed: 0,
        rewardedWithLife: true,
        clockMinutes,
        roundsAsked: round + 1,
      };
    }
    // Fallo.
    if (numLiving < 2) {
      return {
        outcome: "dungeon",
        shrineCeded: null,
        sacrificed: 0,
        rewardedWithLife: false,
        clockMinutes,
        roundsAsked: round + 1,
      };
    }
    if (warned) {
      clockMinutes += 2; // advance_clock(2)
      if (round === 3) {
        sacrificeFirstCompanion(state); // péndulo
        return {
          outcome: "pendulum",
          shrineCeded: null,
          sacrificed: 1,
          rewardedWithLife: false,
          clockMinutes,
          roundsAsked: 4,
        };
      }
    } else {
      warned = true; // 1er fallo: solo amenaza
    }
  }
  // Inalcanzable con party>1 (la ronda 3 siempre resuelve por péndulo); defensivo.
  return {
    outcome: "pendulum",
    shrineCeded: null,
    sacrificed: 0,
    rewardedWithLife: false,
    clockMinutes,
    roundsAsked: 4,
  };
}

// ---------------------------------------------------------------------------
// Depósito post-captura — BLCKTHRN:0x08e7-0x0905
// ---------------------------------------------------------------------------

/**
 * Estado final tras la escena de captura (BLCKTHRN 0x08e7-0x0905): te sueltan en
 * (10,7) **planta 0xff = el SÓTANO (z=−1), la celda del calabozo del Palacio**
 * (loc 0x12), con las llaves confiscadas (g_keys=0), a pie.
 *
 * CIERRE de la ex-Clase C «planta del depósito» (blackthorn.md §3.1 la leía como
 * «centinela de redibujado sin re-fijar»): `0x08e7 c6069558ff mov [g_floor],0xff`
 * ES la planta de aterrizaje, no un centinela —
 *   · 0xff es el valor ORDINARIO del sótano: KLIMB (TOWN 0x052e) hace
 *     `inc [g_floor]` (0x0548) / `dec [g_floor]` (0x0566) sobre la MISMA variable
 *     (DS 0x5895) — bajar la escalera desde la planta 0 deja exactamente 0xff, y el
 *     save nativo persiste ese byte tal cual (saveNative.ts:405-420, 0xff ↔ z=−1
 *     de maps/smallmaps.json; normZ del gestor de NPCs ídem, manager.ts:149).
 *   · El depósito gemelo del arresto de pueblo SÍ elige planta explícita: TOWN
 *     0x1332 `sub al,al` / 0x1337 `mov [g_floor],al` = 0 para la celda de Yew.
 *     Cada depósito escribe su destino completo; el de la captura escribe 0xff.
 *   · Control por tiles (maps/smallmaps.json, loc 18): (10,7) SÓLO es pisable en
 *     z=−1 (68, interior de la celda con su puerta-rastrillo 187 en (10,9)); en
 *     z=0/1/2 es muro 79 y en z=3 es 81. Depositar en planta 0 encastraba al
 *     party DENTRO del muro sur del almacén de barriles (vídeo del usuario,
 *     24-08: «¡Bloqueado!» en las cuatro direcciones).
 * La escritura va ANTES del fade (0x08ec redraw / 0x08f3 fade) y NADA entre
 * 0x08e7 y el ret 0x090f la re-toca ⇒ el aterrizaje es SIEMPRE el sótano, se
 * dispare la captura desde la planta que sea.
 */
function blackthornCaptureDeposit(state: GameState): void {
  state.position.location = LOC_BLACKTHORN;
  state.position.floor = -1; // g_floor=0xff (BLCKTHRN 0x08e7) = sótano/celda
  state.position.x = 0x0a;
  state.position.y = 0x07;
  state.keys = 0;
  state.transport = "foot";
  state.transportTile = TILE_FOOT;
}

/** Resultado agregado de la escena de captura completa. */
export interface CaptureResult {
  /** Santuario interrogado, o null si los 8 ya cayeron (sin interrogatorio). */
  shrine: number | null;
  /** Resultado del interrogatorio, o null si no hubo. */
  interrogation: InterrogationResult | null;
}

/**
 * Escena de captura completa (BLCKTHRN:0x060e). Cuenta vivos, elige el santuario a
 * interrogar, corre el interrogatorio con las `responses` dadas y deposita al party.
 * `mantras` = data.json mantras[] (índice = virtud). MUTA `state`.
 */
export function blackthornCapture(
  state: GameState,
  responses: string[],
  mantras: string[],
): CaptureResult {
  const numLiving = countLiving(state);
  const shrine = pickInterrogationShrine(state);
  let interrogation: InterrogationResult | null = null;
  if (shrine !== null) {
    interrogation = runInterrogation(state, numLiving, shrine, mantras[shrine] ?? "", responses);
  }
  blackthornCaptureDeposit(state);
  return { shrine, interrogation };
}

/** Miembros vivos (status != 'D'), sobre party_size (BLCKTHRN 0x0616 loop 'D'). */
export function countLiving(state: GameState): number {
  const n = state.partySize ?? state.characters.length;
  let c = 0;
  for (let i = 0; i < n; i++) if (state.characters[i]?.status !== "D") c++;
  return c;
}

// ---------------------------------------------------------------------------
// Refuge / party-wipe — BLCKTHRN:0x0910 `party_refuge`
// ---------------------------------------------------------------------------

/** Resultado observable del refuge. */
export interface RefugeResult {
  /**
   * Miembros revividos: los que estaban 'D' **dentro del GRUPO** — los `g_party_size`
   * primeros del roster, no el roster entero (BLCKTHRN 0x0b54/0x0baa). Con roster de 16
   * y grupo de 6, los 10 de fuera NI se revisan NI cuentan aquí.
   */
  revived: number;
  /** true si se rellenó la comida (estaba a 0). */
  foodRefilled: boolean;
}

/**
 * Recuperación de party-wipe (BLCKTHRN:0x0910). Se dispara cuando TODO el party
 * está muerto (party_conscious_state == −1) en pueblo/overworld/mazmorra — NO es
 * game-over. Despiertas en el castillo de Lord British (loc 0x11, planta 1, en
 * (10,10)) a pie, con el party REVIVIDO. Efectos exactos del final de la rutina
 * (0x0bfd-0x0c4d):
 *  - karma restaurado a un SUELO de 75 (0x0bfd: `if karma<0x4b: karma=0x4b`),
 *  - g_location=0x11, g_floor=1, (g_party_x,g_party_y)=(10,10), a pie,
 *  - g_time_spell y sus turnos a 0; reloj avanzado a las 6:00 (bucle advance_clock(9)),
 *  - g_light_spell_mins y g_torch_mins a 0,
 *  - si g_food==0 → g_food=63 (0x3f).
 * Revive por miembro (0x0b54-0x0bb1): **HP restaurado a full asm-directo** (0x0b98:
 * `word[rec+0x10] := word[rec+0x12]`, currentHp := maxHp). El barrido va sobre los
 * `g_party_size` primeros del roster, NO sobre el roster entero: 0x0b54 lee
 * `[g_party_size]` con early-out `or ax,ax / je 0xbb6`, y 0x0baa lo RE-LEE para cerrar
 * (`cmp si,ax` @0x0baf / `jb 0xb68`). Sin `cmp si,6` — a diferencia de los bucles de la
 * trampa de cofre (0x2aa8/0x3054), que sí lo llevan; la cota de CADA rutina se deriva de
 * su propio asm (ficha #41-bis, re/notes/blackthorn-cota-party-size.md). El
 * byte de STATUS lo fija kernel 0xdc66 [= CS 0x7ef6 → CAST2.OVL:0x05e0](i,0xff) — su valor exacto queda ⚠️ (el clon usa
 * 'G'); ver re/verified/blackthorn.md.
 */
export function partyRefuge(state: GameState): RefugeResult {
  // Revive: por miembro DEL GRUPO (i < g_party_size, 0x0b54/0x0baa — los del roster que
  // no van en el grupo no se tocan), currentHp := maxHp (0x0b98, incondicional) y status
  // vivo ('G' como observable; el byte exacto que fija kernel 0xdc66
  // [= CS 0x7ef6 → CAST2.OVL:0x05e0] queda abierto). `revived` cuenta los que estaban 'D'
  // (la escena se dispara con el GRUPO entero muerto — party_conscious_state == −1, que
  // también se mide sobre g_party_size: kernel 0x39fc @0x3a0e).
  const n = state.partySize ?? state.characters.length;
  let revived = 0;
  for (let i = 0; i < n; i++) {
    const c = state.characters[i];
    if (!c) continue;
    if (c.status === "D") revived++;
    c.currentHp = c.maxHp;
    c.status = "G";
  }
  if (state.karma < REFUGE_KARMA_FLOOR) state.karma = REFUGE_KARMA_FLOOR;
  state.position.location = LOC_LORD_BRITISH;
  state.position.floor = 1;
  state.position.x = 0x0a;
  state.position.y = 0x0a;
  state.transport = "foot";
  state.transportTile = TILE_FOOT;
  state.timeSpell = undefined;
  state.timeSpellTurns = 0;
  state.time.hour = 6;
  state.time.minute = 0;
  state.lightSpellMins = 0;
  state.torchTurns = 0;
  let foodRefilled = false;
  if (state.food === 0) {
    state.food = 0x3f;
    foodRefilled = true;
  }
  return { revived, foodRefilled };
}

// ---------------------------------------------------------------------------
// Merma de oro de la Falsedad — SHOPPES.OVL:0x019a
// ---------------------------------------------------------------------------

/**
 * Índice del Shadowlord presente en la ciudad actual (TOWN 0x02b6-0x0306): recorre
 * g_shadowlord_locs[0..2] y devuelve el índice del primero cuya localización iguala
 * la del party; −1 si ninguno. Shadowlord 0 = Falsehood.
 */
export function shadowlordPresentIndex(state: GameState): number {
  const locs = state.shadowlordLocs ?? [];
  const here = state.position.location;
  for (let i = 0; i < 3; i++) {
    if (locs[i] === here) return i;
  }
  return -1;
}

/**
 * Fila que APAGA la colocación del Shadowlord — `TOWN 0x02bb cmp [g_party_y],4`.
 * ★ NO es «la fila de entrada estándar» (esa es la 30, `SMALL_MAP_ENTRY`): es la fila de
 * la CELDA DE YEW. El binario mete al arrestado en (25,**4**) en 0x1313/0x1318 y acto
 * seguido RECARGA el pueblo (`0x12cd push 1 / call 0x11f0`, misma rutina que llama a la
 * colocación en 0x1239) ⇒ la guarda existe para que despertar preso no coloque, anuncie
 * ni posea. El propio port ya sitúa la celda en (25,4) citando TOWN 0x130e
 * (`guardArrestJail`), por una vía independiente de esta lectura.
 */
const SHADOWLORD_SUPPRESSED_ROW = 4;

/**
 * COLOCACIÓN del Shadowlord (`town_place_shadowlord` TOWN 0x02AE), calcada: el flag
 * físico se recalcula UNA vez por carga de mapa/planta y vale hasta la siguiente.
 *
 * ```
 * 02b6  mov [g_unk_5958],0xff        ; sentinel «ninguno»
 * 02bb  cmp [g_party_y],4 / je 0x2dd ; ★ con y==4 SE SALTA la búsqueda entera
 * 02c8  mov cl,[g_unk_5958]          ; cl precargado con 0xFF…
 * 02cc  cmp [si+0x58c8],dl           ; …bucle si=0..2 contra g_location
 * 02d4  mov cl,al                    ; …sólo se pisa cl si hubo match
 * 02d9  mov [g_unk_5958],cl          ; ⇒ no-encontrado deja el sentinel
 * ```
 * Devuelve el índice 0/1/2, o **−1** por el 0xFF.
 */
export function computeShadowlordHere(state: GameState): number {
  if (state.position.y === SHADOWLORD_SUPPRESSED_ROW) return -1; // 0x02bb/0x02c0
  return shadowlordPresentIndex(state); // 0x02c2-0x02d9
}

/**
 * LECTOR del flag físico para los tres consumidores que en el binario cuelgan de
 * `g_unk_5958` — la merma de oro (SHOPPES 0x019a), la rama de TALK (0x1187) y la
 * posesión de NPCs (TOWN 0x1156). Los tres preguntan por un VALOR concreto, no por
 * «¿hay alguno?»: `cmp 0` es «¿es el de la FALSEDAD?», y con el sentinel 0xFF el `jne`
 * se cumple igual que con 1 ó 2 (las dos ramas colapsan, por eso basta el `cmp 0`).
 *
 * Fallback a la consulta lógica cuando el estado nunca pasó por una carga de mapa
 * (`undefined`): arneses puros y saves viejos siguen viendo lo de siempre.
 */
export function shadowlordHereIndex(state: GameState): number {
  return state.shadowlordHere ?? shadowlordPresentIndex(state);
}

/**
 * Merma de oro tras una compra (SHOPPES:0x019a `post_purchase_gold_rand`): si el
 * Shadowlord presente es la Falsedad (índice 0), `gold -= roll` con suelo 0, donde
 * `roll` = rand(1,64) del kernel. Se llama tras CADA pago en tienda. `roll` lo
 * provee el caller (consume 1 rand del stream de mundo). Devuelve el oro mermado.
 */
export function postPurchaseGoldDrain(state: GameState, roll: number): number {
  // El gate lee el FLAG FÍSICO (`g_unk_5958`), no la tabla lógica: con la colocación
  // suprimida (celda de Yew) el binario no merma aunque la Falsedad esté en el pueblo.
  if (shadowlordHereIndex(state) !== 0) return 0;
  const before = state.gold;
  state.gold = state.gold <= roll ? 0 : state.gold - roll;
  return before - state.gold;
}

// ---------------------------------------------------------------------------
// Peaje / tributo / password de guardias — TALK.OVL:0x01e2 (dialogNum 0xFF)
// ---------------------------------------------------------------------------

/** Resultado de la demanda de un guardia gated por location. */
export interface GuardResult {
  /** "password" Palacio · "charity" Minoc medio-oro · "tribute" tributo por miembro. */
  kind: "password" | "charity" | "tribute";
  /**
   * Valor de retorno EXACTO del handler (TALK 0x01e2, `jmp 0x318`):
   *  - **0** = guardia SATISFECHO (pagaste el peaje/tributo o el password es correcto).
   *  - **1** = ruta de ESCALADA (rehúsas, no puedes pagar, gate falla o password
   *    erróneo). El caller de 3.13 usa este 1 para reaccionar (hostilidad/guardias).
   */
  ret: number;
  /** Oro cobrado por el guardia (0 si no cobró). */
  goldTaken: number;
}

/**
 * Demanda de un guardia de Blackthorn (TALK 0x01e2, dialogNum 0xFF), gated por
 * g_location. `response` es la respuesta tecleada; `agree` = el jugador respondió
 * 'Y' al prompt yes/no ("Dost thou pay?", helper TALK 0x00ac: **'Y'→ret 0**, 'N'→ret
 * 1). MUTA `state.gold`. Ojo: **pagas al ACEPTAR** ('Y'), no al rehusar.
 *  - loc 0x12 (Palacio): GATE `g_time_spell == 0x1d` (0x02a4) **+** strcmp del password
 *    contra "IMPE" (0x02e0). ★ DERIVADO (#277): 0x1D es la **BLACK BADGE PUESTA** — su
 *    ÚNICO escritor en todo el binario es `(U)se Badge` (CAST.OVL 0x1b47, tras imprimir
 *    "Badge worn!" DS 0x4a63; turnos 0xFF = permanente) y `g_time_spell` es el efecto
 *    temporal ÚNICO activo. Si el gate NO se cumple, `jmp 0x216` = la rama sin password
 *    (el guardia ni pregunta: no hay disfraz que valga). ⇒ el reto de password sólo
 *    existe **llevando puesta la insignia**; sin ella, "IMPE" no abre nada.
 *    Correcto (con badge) → "Pass, friend!" ret 0; else ret 1.
 *    **La comparación es sobre los PRIMEROS 4 chars**: 0x02dc `mov byte[bp-0xc],0`
 *    escribe un NUL en `buf[4]` (buf=[bp-0x10], [bp-0xc]=buf+4) ANTES del strcmp,
 *    truncando el input a 4 chars. ⇒ "IMPER"/"IMPERIAL"/"IMPE " PASAN ("primeros 4 =
 *    IMPE"), "IMP"/"XMPE" fallan. NO es igualdad exacta de cadena (bug-for-bug).
 *  - loc 5 (Minoc): "Dost thou pay?" → 'Y' → `g_gold /= 2` (idiv, 0x021f) ret 0; 'N' →
 *    oro intacto ret 1.
 *  - otras loc: tributo = 10 gp por miembro vivo; 'Y' y `tributo<=oro` → cobra ret 0;
 *    'N' o no puede pagar → ret 1 sin cobro.
 */
export function guardDemand(state: GameState, response: string, agree: boolean): GuardResult {
  const loc = state.position.location;
  if (loc === LOC_BLACKTHORN) {
    // GATE 0x02a4: sin la Black Badge PUESTA el binario salta a 0x216 (rama sin
    // password) — el password no abre nada sin el disfraz (#277).
    if (state.timeSpell !== TIME_SPELL_BADGE) return { kind: "password", ret: 1, goldTaken: 0 };
    // buf[4]=0 (0x02dc) trunca el input a 4 chars → "primeros 4 == IMPE".
    const ok = response.slice(0, 4).toUpperCase() === BLACKTHORN_PASSWORD.toUpperCase();
    return { kind: "password", ret: ok ? 0 : 1, goldTaken: 0 };
  }
  if (loc === LOC_MINOC) {
    if (!agree) return { kind: "charity", ret: 1, goldTaken: 0 }; // 'N' → oro intacto
    const before = state.gold;
    state.gold = Math.trunc(state.gold / 2); // 'Y' → idiv 2 (signed; oro>=0 → floor)
    return { kind: "charity", ret: 0, goldTaken: before - state.gold };
  }
  const tribute = countLiving(state) * TRIBUTE_PER_MEMBER;
  if (!agree) return { kind: "tribute", ret: 1, goldTaken: 0 }; // 'N'
  if (state.gold < tribute) return { kind: "tribute", ret: 1, goldTaken: 0 }; // no puede pagar
  state.gold -= tribute;
  return { kind: "tribute", ret: 0, goldTaken: tribute };
}

// ---------------------------------------------------------------------------
// Arresto en pueblo — TOWN.OVL:0x12ae (rama g_location != 0x12)
// ---------------------------------------------------------------------------

/** location de Yew — el pueblo-cárcel donde despiertas arrestado (TOWN 0x130e). */
export const LOC_YEW = 4;
/** Celda de despertar del arresto (TOWN 0x1313/0x1318: g_party_x=0x19, g_party_y=4). */
const JAIL_X = 0x19;
const JAIL_Y = 4;
/** Hora del despertar en la celda (TOWN 0x132b: bucle advance_clock(0x14) hasta hour==8). */
const JAIL_WAKE_HOUR = 8;

/**
 * Tributo que demanda el guardia fuera de Minoc/Palacio (TALK 0x0230-0x0269):
 * `Σ 0xa` por cada miembro con status != 'D' (0x025a `cmp byte[si],0x44`).
 * Es el número que se imprime en «A guard demands a N gp tribute to Blackthorn!»
 * (DS 0x90cc + N + DS 0x90e0) ANTES del yes/no (TALK helper 0x00ac, DS 0x9052).
 */
export function guardTributeAmount(state: GameState): number {
  return countLiving(state) * TRIBUTE_PER_MEMBER;
}

/**
 * Mutación del arresto «come quietly» (TOWN 0x12ae, rama 'Y' 0x12fa-0x133a):
 * despiertas en la CELDA de Yew — pueblo caminable, SIN cutscene de celda.
 *  - 0x130e-0x1318: g_location=4 (Yew), (x,y)=(0x19,4).
 *  - 0x1332-0x1337: g_keys=0 (llaves CONFISCADAS), g_floor=0.
 *  - 0x1324-0x1330: bucle `advance_clock(0x14)` HASTA g_hour==8. Si ya son las 8
 *    en punto de hora, el bucle no corre ni un tick (chequeo antes del cuerpo,
 *    0x1322 jmp 0x132b) y el minuto queda intacto. El minuto exacto post-bucle
 *    depende del arranque (múltiplos de 20 min); como en `partyRefuge`, el clon
 *    fija minute=0 como observable canónico (⚠️ día no modelado, mismo criterio).
 * El caller (0x133a jmp 0x12cd) re-entra al mapa (push 1; call 0x11f0) — en el
 * clon lo cubre el evento map-changed del caller.
 */
export function guardArrestJail(state: GameState): void {
  state.position.location = LOC_YEW; // 0x130e
  state.position.x = JAIL_X; // 0x1313
  state.position.y = JAIL_Y; // 0x1318
  state.position.floor = 0; // 0x1337
  state.keys = 0; // 0x1332
  // ★ Tras la celda el binario RECARGA el pueblo (`0x12cd push 1 / call 0x11f0`), y esa
  // rutina vuelve a colocar el Shadowlord en 0x1239. Con la party en la fila 4 la guarda
  // 0x02bb lo suprime ⇒ despertar preso deja el flag en −1. Es el ÚNICO camino del juego
  // que dispara esa guarda.
  state.shadowlordHere = computeShadowlordHere(state);
  if (state.time.hour !== JAIL_WAKE_HOUR) {
    state.time.hour = JAIL_WAKE_HOUR; // 0x132b (bucle hasta las 8)
    state.time.minute = 0; // canónico (ver doc)
  }
}

/**
 * ⛔ `guardTributeTrigger` RETIRADO (T-C, 2026-07-30). Existió como fallback (d) del
 * ruling 2026-07-22: el disparo por adyacencia quedó APARCADO (`enabled=false`)
 * esperando «el limitador interno» que se suponía en el handler. `re/notes/
 * tc-result-producer.md` lo cierra: NO EXISTE limitador en ninguna capa —
 * ni en TALK 0x031E, ni en TALK 0x01e2 (cuyas ÚNICAS escrituras de estado global en
 * 0x13b bytes son `g_gold`, 0x0225 y 0x029d), ni en el productor de `result`. La
 * guarda completa es la del fast-path NPC.OVL 0x06e4, re-evaluada DESDE CERO cada
 * turno porque el prólogo de la pasada de NPCs limpia `[0x65be]`/`[0x65bf]`
 * (0x0dc1/0x0dc6): `manhattan==1 ∧ aiType ∈ {4,5} ∧ dlgNum != 0`. La rareza de la
 * demanda en el corpus de LPs no era un limitador: es que no se suele circular
 * pegado a un guardia. El disparo vive ahora en `checkGuardTribute`, sin gate.
 */
