/**
 * (U)se — herramientas de endgame (jump-table CAST.OVL 0x185d), extraídas de la
 * clase Game (TRAMO 3 del refactor estructural, auditoría ARQ-3): cada rama del
 * dispatcher (U)se está derivada byte-a-byte de re/disasm/CAST.OVL.asm; el índice
 * en el picker (ZSTATS extended item table 0xB9EE) enruta por
 * `jmp word cs:[bx-0x2522]` (tabla en file 0x1b5e, entrada = CS_target = file+0xbf80):
 *   0x10 Carpet · 0x11 Skull Key · 0x12 Amuleto · 0x13 Corona · 0x14 Cetro ·
 *   0x1d/1e/1f Shards · 0x20 Spyglass · 0x21 HMS Cape("Plans") · 0x22 Sextant ·
 *   0x23 Pocket Watch · 0x24 Black Badge · 0x25 Box.
 * Strings byte-exactos de DATA.OVL (DS+0x10). Ver re/notes/use-merchants.md §Use.
 * Ninguna de estas ramas toca [bp-0xa] (queda en 1) ⇒ NO imprime "Failed!" (tail
 * 0x1b8a) ni consume RNG; el (U)se no rueda turno en el port (igual que useSkullKey).
 *
 * Game delega aquí con un contexto ESTRECHO (`UseToolsCtx`): estado + los 4
 * helpers de mapa/transporte que estas ramas tocan. Firmas públicas de Game
 * intactas (fachada); comentarios-cita transplantados ÍNTEGROS.
 */
import type { GameState } from "../state.js";
import type { GameEvent } from "../game.js";
import type { RandFn } from "../world/survival.js";
import { isBelowGround } from "../world/survival.js";
import { tf } from "../../i18n/index.js";
import type { DungeonState } from "../dungeon/dungeon.js";
import { sfxEvent } from "../sfx.js";
import { buildZodiacView } from "../world/zodiac-view.js";
import { buryMoonstone } from "../world/moongates.js";
import { shadowlordDeadFlag, SHADOWLORDS, type ShadowlordKey } from "../quest/shadowlords.js";
import { castShardIntoFlame, SHADOWLORD_TILE, type RitualInput } from "../quest/ritual.js";
import {
  TIME_SPELL_AMULET,
  TIME_SPELL_BADGE,
  TIME_SPELL_CROWN,
  TIME_SPELL_PERMANENT,
} from "../world/blackthorn.js";

export interface UseToolsCtx {
  state: GameState;
  dungeonState: DungeonState | null;
  rand: RandFn;
  mapTileWithOverrides(x: number, y: number): number;
  setMapOverride(x: number, y: number, tile: number): void;
  /** Escritura de TERRENO en el búfer vivo (volátil, no viaja en el save). Ver #119. */
  setVolatileTerrain(x: number, y: number, tile: number): void;
  syncTransportFromTile(tile: number): void;
}

/**
 * `g_location` EFECTIVO para los gates de (U)se — mismo puente que
 * `Game.effectiveLocation` (#123). Dentro de la mazmorra el clon deja
 * `position.location` en 0 (la mazmorra vive en `dungeonState`), mientras el binario
 * escribe ahí 0x21..0x28 al entrar (MAINOUT `cmd_enter_dungeon` 0x0887-0x088c:
 * `mov al,[bp-2]` / `inc al` / `mov [g_location],al` sobre el índice 0x20..0x27 de las
 * tablas DS 0x1e8a/0x1eb2). Sin esto los tres gates `location >= 0x21` de este fichero
 * eran CÓDIGO MUERTO y el (U)se —que SÍ se despacha en el bucle de mazmorra— corría el
 * camino de superficie: la alfombra se desplegaba dentro de la mazmorra, el catalejo
 * enseñaba estrellas y la moonstone se enterraba en el mapa de SUPERFICIE.
 */
function effectiveLocation(ctx: UseToolsCtx): number {
  return ctx.dungeonState ? ctx.dungeonState.pos.dungeon : ctx.state.position.location;
}

export function useShard(ctx: UseToolsCtx, which: ShadowlordKey): GameEvent[] {
  const events: GameEvent[] = [];
  const i = SHADOWLORDS.indexOf(which); // 0=falsehood, 1=hatred, 2=cowardice
  const pos = ctx.state.position;
  const input: RitualInput = {
    shardIdx: i,
    partyX: pos.x,
    partyY: pos.y,
    location: pos.location,
    floor: pos.floor,
    // El binario lee el tile en (x, y-1); aquí = un Shadowlord convocado allí.
    tileAbove: shadowlordPresentAt(ctx.state, pos.location, pos.floor, pos.x, pos.y - 1)
      ? SHADOWLORD_TILE
      : 0,
    summonedIdx: ctx.state.shadowlordSummoned ?? -1,
  };
  const result = castShardIntoFlame(input);
  // BARRIDOS DEL RITUAL (#201). `CAST.OVL:0x15dd-0x162a` son DOS bucles CONSECUTIVOS sobre
  // `tone_sweep` (0x6212 → `ULTIMA.EXE:0x2192`, base 0xBF80): `si` sube 2000→25000 de 50 en
  // 50 y vuelve 25000→2000 de −50 en −50, 460 llamadas cada uno, sin nada en medio.
  // 🔴 Los DOS van SIEMPRE: terminan en `0x162c`, y el gate de posición que decide si el
  // ritual surte efecto está DESPUÉS, en `0x162f` (compara x/y/location/floor contra la
  // tabla de `bx+0x4882`). Colgar el descendente del éxito sería inventar una condición.
  // Su sitio exacto es tras imprimir el nombre del shard (`0x15d9 call 0x58d0` con
  // 0x47cc/0x47d9/0x47e3 = las tres cadenas) — de ahí que la 1ª línea se emita suelta.
  const [shardLine, ...restLines] = result.lines;
  if (shardLine !== undefined) events.push({ kind: "message", text: shardLine });
  events.push(sfxEvent("shard-sweep"));
  for (const line of restLines) events.push({ kind: "message", text: line });
  if (result.destroyed) {
    // COREOGRAFÍA DE LA DESTRUCCIÓN (#201) — calcada de la cola de `use_shard_at_flame`, en
    // su orden y con sus cifras. El ruling del lead del 12-08 levantó el veto de «canal
    // nuevo» tras el censo (ver `skin/world-fx.ts`):
    //   0x1674  push 7; call 0x7b66   → PAUSA de 7 unidades (redraw + espera calibrada)
    //   0x169d/0x16a0/0x16a3          → `screen_shake_fx` ×3 (kernel 0x3072, cero args)
    //   0x16aa  push 3; call 0x7b66   → PAUSA de 3
    //   0x16f4  call `explosion_fx_at_cell` → tile 0 + noise_burst, ×7 sobre (x, y−1)
    //   0x1759  call 0x83e8           → `sfx_victory_fanfare` (ULTIMA.EXE:0x4368)
    //
    // 🔴 La SACUDIDA no necesita canal nuevo y esto NO lo inventa: `{kind:"quake"}` ya es un
    // evento del mundo y `world/shrine-ceremonies.ts:299` lo emite ×3 citando el MISMO
    // `kernel 0x3072`. La piel fiel cuenta las del batch y alarga la sacudida a
    // N·QUAKE_PULSES, así que las tres van seguidas igual que en la ceremonia del Codex.
    // (Corrijo lo que yo mismo declaré antes: dije que la sacudida sólo salía por
    // `routeCombatFx` — el camino de mundo existía, y con precedente.)
    for (let k = 0; k < 3; k++) events.push({ kind: "quake" });
    events.push(sfxEvent("quake"));
    // La celda va como DESPLAZAMIENTO respecto al grupo, no en absoluto: el snapshot que
    // reciben las pieles no lleva la posición del mundo, así que unas coordenadas absolutas
    // no serían pintables — y el binario la expresa igual (`party_y − 1`, 0x16ad). La PAUSA(7)
    // previa no viaja como evento suelto: la lleva dentro `preDelayUnits`, porque es el único
    // efecto cuyo instante depende de ella.
    // 🔴 `underTile` (#243) — EL ORDEN, cableado por la vía de presentación. En el binario los
    // escritos de estado (`CAST 0x1708`) van DESPUÉS de las siete explosiones
    // (`0x16e1-0x16fa`); aquí `removeShadowlordAt` muta `worldObjects` de forma SÍNCRONA doce
    // líneas más abajo y la piel pinta del snapshot ya commiteado, así que la coreografía
    // entera caía sobre una celda que ya enseñaba la Llama. Diciendo QUÉ tile sigue debajo, la
    // piel reproduce el orden observable sin que el core tenga que aplazar la mutación (las dos
    // vías, y por qué se elige ésta, en `skin/world-fx.ts` §`underTile`).
    // 🔴 `underTile` viaja CRUDO (0xFC), SIN el banco alto — y esto no es un detalle de estilo:
    // la primera versión mandaba `SHADOWLORD_TILE + SPRITE_BANK` importando la constante de
    // `render/tileanim.js`, y eso es un import de `core/` a `render/` que la Regla A de
    // `tests/skin-import-guard.test.ts` PROHÍBE (lo cazó ella, no yo). En qué BANCO vive un
    // índice es asunto de presentación: el core dice QUÉ objeto sigue en la celda y la piel
    // resuelve su sprite — `skin/world-fx.ts` suma el banco al blitear, con el mismo convenio
    // que `world/enemies.ts:171` («CRUDO, sin banco: el call-site suma el banco»).
    // El binario coincide: el slot guarda 0xFC (TOWN 0x3a1) y es el BLIT DE CELDA quien pinta
    // del banco de móviles (`FONT.OVL 0x02a2`/`0x02e3` ⇒ 0x1FC `ShadowLord1`). Sin esa suma
    // —ahora en la piel— saldría `Bellows1`, el «soplador dorado» de #195.
    events.push({
      kind: "cell-explosion",
      cellFx: {
        dx: 0,
        dy: -1,
        bursts: 7,
        preDelayUnits: 3,
        underTile: SHADOWLORD_TILE,
      },
    });
    // 🔴 El `spell-zap` que había aquí estaba MAL ATRIBUIDO y se retira: `sfx.ts:59` lo
    // documenta como el `tone_sweep(0x2648,1,0x6d60,0x3e8,2)` que emite el DESPACHADOR DE
    // CONJUROS en `CAST.OVL:0x0e6e` — otra rutina y otro sitio. No se pierde un efecto: se
    // corrige una atribución, y suena lo que el binario emite de verdad.
    events.push(sfxEvent("victory-fanfare")); // cola del ritual (CAST 0x1759 → 0x4368)
    // La rama de FRACASO (`0x1656`, cuando el gate de posición no casa) tiene sonido PROPIO
    // — `call 0x842e` con (0x320, 0x7d0, 1, 0x32) — y salta a `0x175c` = el `ret`, sin
    // fanfarria. NO se emite: `0x842e` es el sujeto de la disputa de FIRMA de la ficha #83
    // (dos lecturas de su cuerpo con el orden de argumentos INVERTIDO), y sin adjudicar eso,
    // calcar sus cuatro argumentos sería elegir una de las dos lecturas a ciegas.
    ctx.state.questFlags[shadowlordDeadFlag(which)] = true; // fuente de verdad
    if (Array.isArray(ctx.state.shadowlordLocs)) ctx.state.shadowlordLocs[i] = 0xff; // 0x170b
    ctx.state.shards[which] = false; // 0x1710: consume el shard
    ctx.state.shadowlordDoomBits = (ctx.state.shadowlordDoomBits ?? 0) | result.doomBit; // 0x171d
    // ★ #238 — el 0x171d del binario es `or word [g_npc_dead_bitmap+112]`: el «bitmap de
    // doom» NO es un word aparte, son los bits de NPC-MUERTO de la fila de STONEGATE (loc
    // 29, la morada de los Shadowlords). DOOM_BIT[idx] = 0x02<<idx cae, MSB-first, en el
    // slot 6−idx (Falsehood→6, Hatred→5, Cowardice→4): el filtro genérico de NPCs muertos
    // es el lector real. Sin esta línea el estado nace inconsistente con su propio .GAM
    // (el export vuelca el bit vía doomBits y el import lo lee vía npcDead).
    // (la guarda: los arneses puros de ritual fabrican estados sin la matriz npcDead —
    // casts `as` que vitest no typechequea; en un GameState real la matriz siempre existe.)
    const stonegate = ctx.state.npcDead?.[29 - 1];
    if (stonegate) stonegate[6 - i] = true;
    ctx.state.shadowlordSummoned = undefined;
    removeShadowlordAt(ctx.state, pos.location, pos.floor, pos.x, pos.y - 1);
    events.push({ kind: "map-changed" });
    // ✅ EL ORDEN Y LA FASE, CABLEADOS (#243 — este bloque enumeraba los dos como pendientes).
    // Los dos se resuelven en PRESENTACIÓN, y por eso desde aquí sólo se ve el `underTile` de
    // arriba: el core sigue commiteando el estado en este mismo lote (a propósito — ver la
    // discusión de las dos vías en `skin/world-fx.ts`).
    //   · ORDEN — `cellFx.underTile` dice qué tile sigue viéndose en la celda durante toda la
    //     coreografía, así que la ráfaga cae SOBRE el Shadowlord como en `CAST 0x16e1-0x16fa`
    //     (los escritos de `0x1708` van después) en vez de sobre la Llama.
    //   · FASE — la presentación calcula el `leadMs` recorriendo ESTE array en orden
    //     (`planTurnPhase`, skin/turn-phase.ts): todo cue de `BLOCKING_CUES` que aparezca
    //     ANTES de un evento visual lo empuja hacia atrás su duración, que es justo lo que
    //     hace el binario cuando el barrido de `0x15dd-0x162a` gira hasta acabar antes de
    //     la `pause(7)` de `0x1674`. ★ #208 cerró la INVERSA: la fanfarria de abajo espera
    //     a la sacudida y a las explosiones (CAST 0x1759 va tras 0x16e1-0x16fa) porque el
    //     MISMO recorrido proyecta también la espera de cada cue (`sfxLeadMs`) y el bus la
    //     lleva al speaker. ⚠ POR ESO EL ORDEN DE ESTE ARRAY YA NO ES SÓLO DOCUMENTAL:
    //     mover `shard-sweep` detrás de la sacudida no reordenaría un log, borraría la
    //     espera — y mover la fanfarria delante de la explosión la haría sonar encima.
    //   · SIGUE SIN VIAJAR la `pause(7)` de `CAST 0x1674` — 7 × 55 = 385 ms de silencio entre
    //     el barrido y la primera sacudida. Se declara en vez de colarla dentro del `leadMs`:
    //     el `leadMs` que la piel calcula sale ENTERO de duraciones que la piel ya conoce
    //     (catálogo de audio + su propia `QuakeShake`), y meterle un literal de 385 ms sería
    //     una constante del ritual escondida en un manejador genérico. La de 3 sí viaja, en
    //     `preDelayUnits`, porque ahí el descriptor tiene campo para ella.
    // Derivación completa y cifras en `re/notes/shadowlord-av-243.md` §2 y §3, y el careo
    // contra el vídeo del usuario (16-08) en `re/notes/shadowlord-fx-243.md`.
  }
  return events;
}

/**
 * (U)se HMS Cape ("Plans", CAST.OVL 0x1a76). En fragata (transport_tile & 0xF8 ==
 * 0x20) → `g_hms_cape |= 0x80` (no-op: el (G)et ya dejó 0xFF) + eco veraz — el
 * anuncio del original («Ship rigged…», DS 0x49C2) se movió al Get (ficha #24);
 * el efecto de doble velocidad ya es fiel (navalStepCost, transport.ts §5). Fuera
 * de barco → "Only usable on shipboard!\n". Fragata del port: transport === "ship".
 */
export function useHmsCape(ctx: UseToolsCtx): GameEvent[] {
  const events: GameEvent[] = [{ kind: "message", text: "Plans" }]; // str 0x49ba
  if (ctx.state.transport === "ship") {
    // La escritura SE CONSERVA tal cual (0x1a86 `or g_hms_cape,0x80`): es no-op porque el
    // (G)et ya dejó el byte en 0xFF, y calcarla mantiene la mecánica intacta.
    ctx.state.specialItems.hmsCape = true; // 0x1a86: or g_hms_cape,0x80
    // ★ BUG DEL ORIGINAL ARREGLADO (registro §1.6, ficha #24). Aquí el original imprimía
    // «Ship rigged for double speed!» (DS 0x49C2) anunciando un cambio de estado que había
    // ocurrido AL RECOGER LOS PLANOS. El anuncio se ha movido al (G)et, donde es verdad, y
    // este punto emite un eco veraz.
    // ⚠ TEXTO NUESTRO, no de EA: no lleva cita DS porque no existe en el binario.
    events.push({ kind: "message", text: "The ship is already rigged." });
  } else {
    events.push({ kind: "message", text: "Only usable on shipboard!" }); // 0x49e1
  }
  return events;
}

/**
 * (U)se Magic Carpet (CAST.OVL 0x1862 → deploy 0x18a1). DESPLIEGA la alfombra bajo el
 * party (el otro punto de entrada además de (B)oard sobre un tile 0x1b suelto). Eco
 * "Carpet\n\n" (DS 0x48bf). Gates byte-fieles:
 *  - g_location < 0x21 (overworld/pueblo; en MAZMORRA → "Not here!\n" DS 0x48f3).
 *  - A PIE (g_transport_tile==0x1c): en barco → "X-it ship first!\n" (0x48d2); a
 *    caballo/skiff → "Only on foot!\n" (0x48e4).
 * Al desplegar consume UNA alfombra (`dec g_carpets`, 0x18a1) y fija el tile de
 * transporte `0x14 + rand(0,1)` (facing N/E ALEATORIO, +1 tirada) — misma mecánica que
 * `sinkPlayerShip`. El gate de tile-bajo-party != 0xc del binario queda Clase C (tile
 * 0xc sin derivar). Sólo se ofrece en el picker con magicCarpets>0 → el guard es defensivo.
 */
export function useMagicCarpet(ctx: UseToolsCtx): GameEvent[] {
  const events: GameEvent[] = [{ kind: "message", text: "Carpet" }]; // DS 0x48bf
  if (effectiveLocation(ctx) >= 0x21) {
    events.push({ kind: "message", text: "Not here!" }); // DS 0x48f3 (mazmorra)
    return events;
  }
  if (ctx.state.transport === "ship") {
    events.push({ kind: "message", text: "X-it ship first!" }); // DS 0x48d2
    return events;
  }
  if (ctx.state.transport !== "foot") {
    events.push({ kind: "message", text: "Only on foot!" }); // DS 0x48e4
    return events;
  }
  if ((ctx.state.magicCarpets ?? 0) <= 0) return events; // defensivo (el picker filtra)
  // ★ La rama de ÉXITO estaba MUDA — y es el camino normal: 0x188b imprime DS 0x48c8
  // tras comprobar `g_transport_tile == 0x1c` (a pie), que aquí ya garantiza el gate
  // de arriba. #133
  events.push({ kind: "message", text: "Boarded!" }); // DS 0x48c8 (CAST 0x188b)
  ctx.syncTransportFromTile(0x14 + ctx.rand(0, 1)); // TILE_CARPET|facing N/E (+1 RNG)
  ctx.state.magicCarpets = (ctx.state.magicCarpets ?? 0) - 1; // dec g_carpets (0x18a1)
  return events;
}

/**
 * (U)se Spyglass (CAST.OVL 0x1a3a). Gate: overworld/pueblo (g_location < 0x21 y
 * g_floor < 0x80; en mazmorra/combate → "Not here!\n"). De día (hora 6..18) →
 * "No stars!\n"; de noche (hora < 6 ó > 18) → "Looking...\n" + vista de estrellas
 * (call 0xffffbf9a; el catalejo muestra las lunas/estrellas). La vista celeste es
 * cosmética (no derivada aquí); el mensaje y los gates son byte-fieles.
 */
export function useSpyglass(ctx: UseToolsCtx): GameEvent[] {
  const events: GameEvent[] = [{ kind: "message", text: "Spyglass" }]; // str 0x498d
  const { floor } = ctx.state.position;
  const location = effectiveLocation(ctx);
  // #171: `isBelowGround` y NO `floor >= 0x80` literal — el sótano del port es z = −1
  // (con signo), no 0xFF, y el literal lo dejaba pasar: se podía usar el catalejo en el
  // sótano de un castillo, donde el original responde "Not here!".
  if (location >= 0x21 || isBelowGround(floor)) {
    events.push({ kind: "message", text: "Not here!" }); // 0x49af
    return events;
  }
  const hour = ctx.state.time.hour;
  if (hour >= 6 && hour <= 0x12) {
    events.push({ kind: "message", text: "No stars!" }); // 0x49a4 (día)
    return events;
  }
  events.push({ kind: "message", text: "Looking..." }); // 0x4998 (noche)
  // Vista celeste (call 0xffffbf9a → look_sky night 0x03aa): 80 estrellas + zodíaco de 8
  // signos, con línea en los signos donde hay un Shadowlord. Cosmético; se cierra con
  // cualquier tecla (como look_sky 0x4ee). Ver world/zodiac-view.ts + zodiac-derivation.md.
  events.push({ kind: "zodiac-view", zodiacView: buildZodiacView(ctx.state, ctx.rand) });
  return events;
}

/**
 * (U)se Sextant (CAST.OVL 0x1a96). Gate: SÓLO exterior (g_floor <= 0x7f y
 * g_location == 0; si no → "Only outdoors!\n") y SÓLO de noche (hora <= 5 ó >= 19;
 * si no → "Only at night!\n"). Imprime "Position:" + coordenadas (x,y = lat/long).
 * El binario formatea las coords vía call 0xffffc13e; aquí se listan x/y del party.
 */
export function useSextant(ctx: UseToolsCtx): GameEvent[] {
  const events: GameEvent[] = [{ kind: "message", text: "Sextant" }]; // str 0x49fc
  const { location, floor, x, y } = ctx.state.position;
  // #171 — auditado y SE QUEDA LITERAL: aquí el término de piso es REDUNDANTE porque
  // `location !== 0` ya excluye todo pueblo/castillo (los sótanos sólo existen en small
  // maps, location 1-0x20). Con location 0 los únicos pisos son 0 (Britannia) y 0xFF
  // (Underworld), y 0xFF > 0x7f evalúa bien sin enmascarar. No es el defecto de #150.
  if (floor > 0x7f || location !== 0) {
    events.push({ kind: "message", text: "Only outdoors!" }); // 0x4a06
    return events;
  }
  const hour = ctx.state.time.hour;
  if (hour > 5 && hour < 0x13) {
    events.push({ kind: "message", text: "Only at night!" }); // 0x4a16
    return events;
  }
  // i18n: plantilla catalogada («Position: {}, {}»); con `${}` nativo el compuesto no
  // casaba NUNCA con la key y salía en inglés bajo 'es' (#126). En 'en' tf() es identidad.
  events.push({ kind: "message", text: tf("Position: {}, {}", x, y) }); // 0x4a26 "Position:"
  return events;
}

/**
 * (U)se Pocket Watch (CAST.OVL 0x1ad4). Sin gate: "The pocket watch reads H:MM
 * AM/PM.\n" con H = hora%12 (0→12), MM con relleno a 2 dígitos, AM si hora <= 11
 * (0x1b1a `cmp g_hour,0xb; jbe`) si no PM.
 */
export function usePocketWatch(ctx: UseToolsCtx): GameEvent[] {
  const { hour, minute } = ctx.state.time;
  let h12 = hour % 12;
  if (h12 === 0) h12 = 12; // 0x1aef: si el resto es 0 → 12
  const mm = String(minute).padStart(2, "0");
  const ampm = hour <= 0xb ? "AM" : "PM"; // 0x1b1a
  // i18n: igual que el sextante (#126). `ampm` viaja como ARG string, así que además
  // pasa por t() — hoy identidad (AM/PM no están en el corpus), pero queda cableado.
  return [{ kind: "message", text: `Watch` }, // str 0x4a30 (prefijo "Watch\n\n")
    { kind: "message", text: tf("The pocket watch reads {}:{} {}.", h12, mm, ampm) }];
}

/**
 * (U)se Black Badge (CAST.OVL 0x1b2e). Toggle de g_time_spell 0x1d: si ya está
 * puesto → "Removed!\n" (rama de quitar del toggle 0x1764); si no → "Badge worn!\n"
 * + g_time_spell = 0x1d permanente.
 *
 * ★ 30-07 — escribe el MISMO BYTE que los efectos temporales, no un flag aparte. El port
 * modelaba `state.wornBadge` independiente de `state.timeSpell`, lo que permitía estados
 * que el binario no puede tener (insignia Y Quickness a la vez). Censo por bytes de
 * DS 0x587a: 9 escrituras, 33 lecturas, un solo byte. Escribir aquí PISA el efecto
 * temporal, y castear un efecto PISA la insignia. Repara además una pérdida de datos:
 * `wornBadge` no viajaba en el save; `timeSpell` sí.
 */
export function useBlackBadge(ctx: UseToolsCtx): GameEvent[] {
  const events: GameEvent[] = [{ kind: "message", text: "Badge" }]; // str 0x4a5b
  if (ctx.state.timeSpell === TIME_SPELL_BADGE) {
    ctx.state.timeSpell = undefined; // toggle-off 0x1764 → el byte queda a «ninguno»
    ctx.state.timeSpellTurns = undefined;
    events.push({ kind: "message", text: "Removed!" }); // 0x4895 (toggle-off)
  } else {
    ctx.state.timeSpell = TIME_SPELL_BADGE; // 0x1b47 `mov byte [0x587a], 0x1d`
    ctx.state.timeSpellTurns = TIME_SPELL_PERMANENT; // 0xFF = no lo toca el decay
    events.push({ kind: "message", text: "Badge worn!" }); // 0x4a63
  }
  return events;
}

/**
 * (U)se Amuleto de Lord British (CAST.OVL 0x1908). Toggle de g_time_spell 0xe: si
 * ya puesto → "Removed!\n"; si no → "Wearing the Amulet of Lord British...\n" +
 * set_time_spell(0xe, 0xff, efecto 9). El bonus de combate del efecto 9 es endgame
 * profundo (motor #20/#44) → aquí sólo el verbo + toggle fiel (state.wornAmulet).
 */
export function useAmulet(ctx: UseToolsCtx): GameEvent[] {
  const events: GameEvent[] = [{ kind: "message", text: "Amulet" }]; // str 0x4914
  if (ctx.state.timeSpell === TIME_SPELL_AMULET) {
    ctx.state.timeSpell = undefined;
    ctx.state.timeSpellTurns = undefined;
    events.push({ kind: "message", text: "Removed!" }); // 0x4895
  } else {
    ctx.state.timeSpell = TIME_SPELL_AMULET; // set_time_spell(0xe, 0xff, efecto 9)
    ctx.state.timeSpellTurns = TIME_SPELL_PERMANENT;
    events.push({ kind: "message", text: "Wearing the Amulet of Lord British..." }); // 0x491d+0x4a84
  }
  return events;
}

/**
 * (U)se Corona de Lord British (CAST.OVL 0x193e). Toggle g_time_spell 0x1c:
 * puesto → "Removed!\n"; si no → "Thou dost don the Crown of Lord British...\n" +
 * set_time_spell(0x1c, 0xff, 9).
 *
 * ★ A4-PARITY1 (P1b) — escribe el byte de los efectos temporales [0x587a], como la
 * insignia y el amuleto, no un flag aparte: 0x1945 push 0x1c → 0x1764 (toggle: si ya vale
 * 0x1c, "Removed!" y limpia 0x587a/0x588e) → 0x1961 push 0x1c / 0xff / 9 → thunk
 * ULTIMA.EXE 0x80b2 (overlay 0x12) → CAST2 0x08f8 `set_time_spell` (0x08fe escribe 0x587a,
 * 0x0904 0x588e). PISA cualquier efecto temporal y viaja en el save (0x2D4/0x2E8). El
 * `state.wornCrown` anterior era un invento: no viajaba en el save y no es lo que lee el
 * gate del Palacio (ese lee la POSESIÓN, `lbArtifacts.crown`). Sus lectores de combate
 * (COMBAT 0x0196 / 0x0f36, COMSUBS 0x0112) tratan 0x1c como Negate: `negatesEnemyMagic`.
 */
export function useCrown(ctx: UseToolsCtx): GameEvent[] {
  const events: GameEvent[] = [{ kind: "message", text: "Crown" }]; // str 0x4930
  if (ctx.state.timeSpell === TIME_SPELL_CROWN) {
    ctx.state.timeSpell = undefined; // toggle-off 0x1764
    ctx.state.timeSpellTurns = undefined;
    events.push({ kind: "message", text: "Removed!" }); // 0x4895
  } else {
    ctx.state.timeSpell = TIME_SPELL_CROWN; // set_time_spell(0x1c, 0xff, efecto 9)
    ctx.state.timeSpellTurns = TIME_SPELL_PERMANENT;
    events.push({ kind: "message", text: "Thou dost don the Crown of Lord British..." }); // 0x4938+0x4a84
  }
  return events;
}

/**
 * (U)se Cetro de Lord British (CAST.OVL 0x1966, verificado instrucción a instrucción;
 * ver re/notes/overworld-b34-sceptre-correction.md). Empuja "Sceptre" (0x4950) +
 * "Wielding the Sceptre of Lord British..." (0x495a+0x4a84) + SFX de blandido
 * (0x198f `call 0x6212`), y RAMIFICA por ubicación:
 *
 *  • OVERWORLD/PUEBLO (g_location < 0x21 o > 0x28): BARRIDO 3×3 alrededor del party
 *    (0x19a5-0x19f9, dx/dy ∈ -1..+1, INCLUYE la celda central). Cada tile con nibble
 *    alto 0x70 (0x70-0x7F = ShadowlordBoundary, `(tile&0xf0)==0x70`, 0x19c8) → Grass
 *    (5, 0x19ce), en SILENCIO (sólo SFX por celda, sin texto). Si disolvió ≥1 termina
 *    mudo (0x1a01 `jmp fin`); si disolvió 0, cae a la rama residente (abajo). NO toca
 *    los campos elementales 0x80-0x83 de In Flam Grav (eso es An Grav). En el port
 *    0x70-0x7F no aparece en ningún mapa no-combate alcanzable, así que el observable
 *    normal sigue siendo "No effect!" — pero ahora por el MECANISMO fiel, no hardcode.
 *
 *  • MAZMORRA (g_location ∈ 0x21..0x28): el barrido se SALTA (gate 0x199e-0x19a3), el
 *    contador queda en 0 y SIEMPRE cae la rama residente 0x1a04 (`call 0xffffc126`) →
 *    disuelve el campo mágico ENCARADO. Devuelve 1 → "Field dissolved!" (0x496f); 0 →
 *    "No effect!" (0x4981). Geometría por encima del techo del disasm → lectura
 *    conservable = celda encarada (DungeonState.dissolveFacingField). Doom (loc 0x28)
 *    y Deceit/Despise/Destard/Shame/Hythloth NO tienen campos (DUNGEON.DAT) → "No
 *    effect!" fiel; Wrong (0x24) y Covetous (0x25) SÍ (energy fields) → aquí actúa.
 */
export function useSceptre(ctx: UseToolsCtx): GameEvent[] {
  const events: GameEvent[] = [
    { kind: "message", text: "Sceptre" }, // str 0x4950
    { kind: "message", text: "Wielding the Sceptre of Lord British..." }, // 0x495a+0x4a84
    sfxEvent("sceptre"), // 0x198f call 0x6212 (blandido)
  ];
  // Mazmorra: barrido saltado (0x199e) → rama residente 0x1a04 sobre el campo encarado.
  if (ctx.dungeonState) {
    const dissolved = ctx.dungeonState.dissolveFacingField();
    events.push({ kind: "message", text: dissolved ? "Field dissolved!" : "No effect!" });
    return events;
  }
  // Overworld/pueblo: barrido 3×3 de 0x70-0x7F → Grass(5), silencioso; "No effect!"
  // sólo si 0 disueltos (0x19fb je 0x1a04, rama residente devuelve 0 aquí → 0x4981).
  let dissolved = 0;
  const { x: px, y: py } = ctx.state.position;
  for (let dx = -1; dx <= 1; dx++) {
    for (let dy = -1; dy <= 1; dy++) {
      const tx = px + dx;
      const ty = py + dy;
      if ((ctx.mapTileWithOverrides(tx, ty) & 0xf0) === 0x70) {
        // 0x19ce `mov byte [di],5` — y DI es el puntero que devolvió `tile_addr`
        // (0x19c1 `call 0x8482` → ULTIMA.EXE 0x4402), o sea el búfer de TERRENO vivo,
        // NO la tabla de objetos. Canal VOLÁTIL: no viaja en el save y muere en la
        // siguiente carga de mapa (#119; derivación completa en el campo
        // `volatileTerrain` de game.ts y en re/notes/terreno-119-acta.md).
        ctx.setVolatileTerrain(tx, ty, 5); // Grass
        dissolved++;
      }
    }
  }
  if (dissolved === 0) events.push({ kind: "message", text: "No effect!" }); // 0x4981
  return events;
}

/**
 * (U)se Moonstone (CAST.OVL 0x153c). Entierra la moonstone `phase` a los pies del
 * party, plantando su moongate personal. Derivación instrucción a instrucción:
 *   · 0x1542-0x1551: lee el tile BAJO el party (get_tile 0x8482) → `tile`.
 *   · 0x1558: imprime "Moonstone " (DS 0x4768) — cabecera del verbo.
 *   · 0x155f: `cmp g_location,0x21; jae fail` → fuera del overworld/pueblo (loc>=0x21,
 *     mazmorra/combate) NO se puede enterrar.
 *   · 0x1566-0x157c: tile enterrable si `tile∈{0x2c,0x2d}` o `0x04<=tile<=0x0a` (grass y
 *     afines); si no → "cannot be buried here!\n" (DS 0x477c).
 *   · 0x1585-0x15a0: guarda X (0x5830), Y (0x5838), LOCATION destino (0x5840=g_location) y
 *     FLOOR destino (0x5848=g_floor) de la stone; queda enterrada → sale de la lista de
 *     usables (el (U)se sólo lista las stones LLEVADAS, table build 0x099a con flag 0xff).
 *   · 0x157e: eco de éxito "buried!\n" (DS 0x4773).
 * El clon modela la stone como {x,y,buried,z} (world/moongates.ts): enterrar = fijar
 * (x,y,z=floor) + buried=true, equivalente a copiar los 4 arrays del binario. Sin RNG.
 */
export function useMoonstone(ctx: UseToolsCtx, phase: number): GameEvent[] {
  const events: GameEvent[] = [{ kind: "message", text: "Moonstone" }]; // DS 0x4768
  const stone = ctx.state.moonstones[phase];
  if (!stone || stone.buried) return events; // defensivo: el picker sólo lista las llevadas
  const { floor, x, y } = ctx.state.position;
  const location = effectiveLocation(ctx);
  if (location >= 0x21) {
    // 0x155f: mazmorra/combate → no enterrable.
    events.push({ kind: "message", text: "cannot be buried here!" }); // DS 0x477c
    return events;
  }
  const tile = ctx.mapTileWithOverrides(x, y); // 0x1542: tile bajo el party
  const buryable = tile === 0x2c || tile === 0x2d || (tile >= 0x04 && tile <= 0x0a);
  if (!buryable) {
    events.push({ kind: "message", text: "cannot be buried here!" }); // DS 0x477c
    return events;
  }
  // 0x1585-0x15a0: entierra la stone en (x,y) con el floor de destino (0 Britannia /
  // 0xFF Underworld). `z` = g_floor, como el binario copia g_floor a 0x5848.
  buryMoonstone(ctx.state, phase, x, y, floor, location);
  events.push({ kind: "message", text: "buried!" }); // DS 0x4773
  return events;
}

/**
 * (U)se Wooden Box / caja de sándalo (CAST.OVL 0x1b58). El binario sólo imprime
 * "Box\n\nHow?\n" y sale: la caja NO tiene efecto por (U)se normal — su carga (la
 * escena del trono en el rescate de Lord British) es endgame (#20). Verbo fiel = eco.
 */
export function useWoodenBox(): GameEvent[] {
  return [{ kind: "message", text: "Box" }, { kind: "message", text: "How?" }]; // str 0x4a70
}

/** ¿Hay un Shadowlord convocado (tile 0xFC) en esta celda exacta? CAST 0x16c1. */
function shadowlordPresentAt(
  state: GameState,
  location: number,
  floor: number,
  x: number,
  y: number,
): boolean {
  return (state.worldObjects ?? []).some(
    (o) =>
      o.tile === SHADOWLORD_TILE &&
      o.location === location &&
      o.floor === floor &&
      o.x === x &&
      o.y === y,
  );
}

/** Retira el objeto Shadowlord de esta celda tras destruirlo. */
function removeShadowlordAt(
  state: GameState,
  location: number,
  floor: number,
  x: number,
  y: number,
): void {
  if (!Array.isArray(state.worldObjects)) return;
  state.worldObjects = state.worldObjects.filter(
    (o) =>
      !(
        o.tile === SHADOWLORD_TILE &&
        o.location === location &&
        o.floor === floor &&
        o.x === x &&
        o.y === y
      ),
  );
}
