/**
 * NPCs de los small maps: spawn por horario y un tick de IA por turno.
 *
 * Reglas EXACTAS re-derivadas de NPC.OVL (re/notes/npc.md): movimiento por
 * horario (`schedule_index` 0x12E0), distancia MANHATTAN (0x06A0), y la
 * jump-table de aiType 0..7 (0x0D00) con el wander exacto (0x0C50): skip ~50%
 * (`rand(0,255)&8`), dirección `rand(0,64)&3 + 1` (span 65), radio Manhattan (wander=3,
 * big-wander=0=sin límite). El RNG es el `OriginalRng` del kernel (stream
 * compartido, consumido en orden de slot).
 * Sin dependencias de render.
 */
import type { GameState } from "../state.js";
import * as sonda from "./ai-probe.js";
import { scheduleIndex } from "../time.js";
import { isPassable } from "../world/movement.js";
import { getActiveMap, type ActiveMap, type WorldData } from "../world/map.js";
import { DoorManager } from "../world/doors.js";
import { OriginalRng } from "../rng-original.js";
import { cargaFielActiva } from "./carga-fiel.js";
import type { RandFn } from "../world/survival.js";
import { guardWanderStep, type GuardState } from "../world/loops/guards.js";
import {
  NPC_SLOT_COUNT,
  POSSESSED_AITYPE,
  POSSESSED_DIALOG,
  isPersonType,
  possessGateRoll,
} from "../world/shadowlord-urban.js";

/** Un slot del fichero .NPC (shape de game/assets/npcs.json). */
export interface NpcSlot {
  slot: number;
  aiTypes: [number, number, number];
  x: [number, number, number];
  y: [number, number, number];
  z: [number, number, number];
  times: [number, number, number, number];
  type: number;
  dialogNumber: number;
}

export interface NpcRuntime {
  slot: number; // índice 0-31 (== npcIndex para flags met/dead)
  location: number;
  type: number; // sprite base = type + 0x100 → tile del atlas = type + 256
  dialogNumber: number;
  aiTypes: [number, number, number];
  schedX: [number, number, number];
  schedY: [number, number, number];
  schedZ: [number, number, number];
  times: [number, number, number, number];
  x: number;
  y: number;
  z: number; // posición actual
  /**
   * PESTILLO HORARIO (#84) — el campo `+0` del registro vivo `0x5F5E + slot*16`.
   * 1 = cerrado (idle); 2 = caminata en la planta visible; 3 = seguir ruta hacia
   * la escalera (único escritor 0x0f8c, dentro de `npc_change_floor`); 4/5 =
   * recompute con el NPC fuera de la planta visible (0x0dd4/0x0dd9); 6/7 = caminar
   * a la escalera para subir/bajar (0x0ea6); 8 = transición invisible (teleport
   * directo, 0x0ebf sin careo de escalera). Partición de seis casos de
   * `npc_check_schedule` 0x09cc-0x0a28. Lo arma `npc_check_schedule` (0x0938) SOLO
   * en la hora en que arranca una ranura; lo cierran los dos únicos escritores de
   * `+0xe` (0x0ee6/0x1089): AL LLEGAR. PERSISTIDO en el save (#108, acta §4).
   */
  state: number;
  /** `+0xe` del registro vivo: la ranura de horario SERVIDA (init 0x0152 del cargador). */
  servedSlot: number;
  /**
   * El BUFFER DE CAMINO real — `0x615E + slot*32` (#108): 32 bytes = hasta 16
   * pares RLE (repeticiones, dirección), escritos por `npc_path_backtrace` 0x04AC
   * y consumidos por `npc_follow_path` 0x0F94 (`0x103a dec byte [bx]`). Las
   * direcciones van en la convención de `step_dir` 0x0632 (1=x++ · 2=y-- · 3=x-- ·
   * 4=y++): el backtrace INVIERTE las de la sonda del escáner al revertir el RLE
   * (0x05ef-0x060a `((d+1)&3)+1`), y como `step_dir(k)` es exactamente la inversa
   * de la sonda k, la composición reproduce el paso padre→celda del escaneo.
   * PERSISTIDO en el save (ventana 0x55A6..0x6605, acta §4) — `GameState.npcWalk`.
   */
  pathBuf: number[];
  /** `0x655E + slot*2`: índice (par) dentro del buffer; −1 = sin camino. PERSISTIDO. */
  pathIdx: number;
  /**
   * `0x65C2 + slot*2` — el CONTADOR DE ATASCO (#108): lo incrementa el paso
   * bloqueado (0x10a3), lo satura a 0xC8 el escaneo fallido (0x11bf), lo resetean
   * el paso bueno (0x103c), el escaneo bueno (0x10d6), el descarte de camino
   * (0x10d6 vía 0x10cb `stuck>3`) y el envejecimiento al pasar 0xCC (0x1208).
   * Con 1..0xC7 el re-escaneo paga la MONEDA `rand(0,2)==1` (0x1155). PERSISTIDO.
   */
  stuck: number;
}

/**
 * AI types del fichero .NPC — jump-table exacta de NPC.OVL:0x0D00 (re/notes/npc.md
 * §4.0). El tile de sprite del NPC no cambia el aiType; éste sale de aiTypes[schedIdx].
 */
const AI_FIXED = 0; // 0x0DAC — quieto
const AI_WANDER = 1; // 0x0D60 — wander radio Manhattan 3
const AI_BIG_WANDER = 2; // 0x0D38 — wander sin límite (maxdist=0)
const AI_RUN_AWAY = 3; // 0x0D76 — huye si el party está a Manhattan<4
/**
 * 🔴 aiType 4 — handler PROPIO (`0x0D40`, el único de los ocho que no comparte destino).
 * Corregido 2026-08-06 (#82); antes este `case` llamaba a `fleeStep`, o sea a HUIR, y este
 * mismo comentario ya describía bien el binario — **comentario correcto sobre código
 * incorrecto**, que es peor que un comentario rancio: el rancio miente y se caza leyendo el
 * código, el correcto ACREDITA que alguien ya lo miró y desactiva la sospecha. Cuatro veces
 * seguidas (#52 el 6, #78 el 5 y el 7, #82 el 4) el clon mandó a `fleeStep` una rama que
 * persigue.
 * Puerta: `0x0d5b cmp ax,4 / jl 0xd91` ⇒ Manhattan < 4 entra en `0x06e4`; si no, wander 3.
 * Dentro, `0x0824 cmp [bp-2],3 / jne 0x884` deja la rama de HUIDA **sólo para el 3**: el 4
 * cae en `0x0884`, cuyo `0x088c cmp [bx],ax / jge skip` exige candidata MENOR = ACERCARSE.
 * Coste medido: el camino del 4 NO atraviesa la cola de erratismo de `0x0892` (exclusiva del
 * 5 y el 7) ⇒ **cero tiradas**; el wander gasta 1 (`rand(0,255)`, 0x0c57). El fix no cambia
 * el consumo, cambia la POSICIÓN. Derivación en re/notes/npc-aitype4-persigue.md.
 */
const AI_MERCHANT = 4; // 0x0D40 — si party a Manhattan<4 de su PUESTO entra en 0x06e4 y PERSIGUE; si no, wander 3
/**
 * 🔴 aiType 5 — hostil SIN gate de distancia. Corregido 2026-08-06 (#78); antes se
 * llamaba `AI_FLEE` y el clon lo mandaba a `fleeStep`, o sea a HUIR.
 * `NPC.OVL:0x0D91` entra en `0x06e4` **directamente**, sin el `cmp ax,4` que sí tienen
 * el 3 y el 6. Dentro cae en la rama `0x0884` (ACERCARSE, igual que el 6) y además
 * es uno de los dos aiType que atraviesan la cola de erratismo de `0x0892`.
 * ⚠️ El 5 es INALCANZABLE con los datos de fábrica (cero ocurrencias en el fichero de
 * NPC); se modela igual que el 7 y la única diferencia medida entre ambos queda
 * declarada en re/notes/npc-aitype57-hostil.md §6.
 */
const AI_HOSTILE = 5; // 0x0D91 → 0x06e4 sin gate — ACERCARSE + cola de erratismo 0x0892
/**
 * 🔴 aiType 6 — MISMO HANDLER que el 3 (`0x0D76`), pero NO el mismo comportamiento.
 * Corregido 2026-08-06 (#52). Este comentario decía «igual que 3» y el clon lo hacía HUIR.
 * El handler compartido sólo aporta el gate de distancia (`0x0d8c cmp ax,4 / jge` = actúa
 * si Manhattan al party < 4) y llama a `NPC.OVL:0x06e4` con el aiType intacto. Dentro,
 * `0x0824 cmp [bp-2],3 / jne 0x884` deja la rama de HUIDA **sólo para el 3**; el 6 cae en
 * `0x0884`, cuyo criterio es `cmp [bx],ax / jge skip / mov di,si` = adopta la puntuación
 * MENOR = **ACERCARSE**. Derivación y las tres vías que lo acreditan en
 * re/notes/npc-aitype6-persigue.md.
 */
const AI_CHASE = 6; // 0x0D76 → 0x06e4 rama 0x0884 — PERSIGUE si el party está a Manhattan<4
/**
 * 🔴 aiType 7 — el gemelo alcanzable del 5 (`0x0D91`, mismo destino de la tabla de salto).
 * Corregido 2026-08-06 (#78); se llamaba `AI_FLEE_2` y también HUÍA. Lo llevan 12 NPC en
 * 36 ranuras de horario, repartidos en tres localizaciones. A diferencia del 6, esta rama
 * **SÍ consume el generador**: la cola de `0x0892` tira SIEMPRE una vez y hasta dos más.
 */
const AI_HOSTILE_2 = 7; // 0x0D91 — idéntico al 5 salvo la salvedad de §6 de la nota

const SMALL = 32;

/** El fichero .NPC codifica el sótano como 0xFF; los small maps lo guardan como -1. */
function normZ(z: number): number {
  return z === 0xff ? -1 : z;
}

/**
 * Los 32 slots de actor de un .NPC no son sólo personas: algunos tienen un tile de
 * OBJETO (`type` = tile − 0x100). En el binario TODOS viven en la misma tabla 0x5C5A
 * (objects.md:16); el clon los separa — personas → NpcManager, objetos → worldObjects
 * (Game.hydrateInteriorObjects). Este es el clasificador compartido: censo derivado byte
 * a byte de CASTLE/TOWNE/DWELLING/KEEP.NPC (re/notes/npc-object-actors.md, scout-castle-bugs
 * Anomalía 2). "chest" (type 1) = contenedor con mecánica (O)pen; "prop" = inerte
 * (cadáver 30); "plot" = objeto recogible por (G)et vía get_special_item
 * (corona 0xB5 en Blackthorn slot 1, cetro 0xB6 en Stonegate slot 9, caja de
 * sándalo 14 en LB castle loc 17 slot 31 tras el pasadizo del clavicémbalo — F1.10-T4 /
 * #51 — y alfombra mágica 27 en LB castle slot 22, SJOG 0x149e). Sólo tipos NO-cero: type 0 (StarPattern) es ambiguo con el marcador de slot vacío
 * y las monturas/Shadowlords siguen en NpcManager / su sistema propio. */
export type NpcObjectKind = "chest" | "prop" | "plot";
export function npcSlotObjectKind(type: number): NpcObjectKind | null {
  switch (type) {
    case 1: // tile 257 Chest
      return "chest";
    case 30: // tile 286 DeadBody
      return "prop";
    case 27: // tile 283 Carpet2 — (G)et: inc g_carpets, SJOG rama 0x149e (switch por object-tile 0x1b)
    case 14: // tile 270 ItemSandalwoodBox — (G)et fija g_wooden_box (0x14F0), #51
    case 0xb5: // corona de Lord British (Blackthorn loc 18, slot 1) — F1.10-T4
    case 0xb6: // cetro de Lord British (Stonegate loc 29, slot 9) — F1.10-T4
      return "plot";
    default:
      return null;
  }
}

/** Colocación runtime de un slot-objeto del .NPC (posición ya resuelta por horario). */
export interface NpcObjectPlacement {
  slot: number;
  type: number;
  kind: NpcObjectKind;
  x: number;
  y: number;
  z: number;
}

/** dist(x1,y1,x2,y2) = |dx|+|dy| — MANHATTAN (NPC.OVL:0x06A0, §3.1). */
function manhattan(ax: number, ay: number, bx: number, by: number): number {
  return Math.abs(ax - bx) + Math.abs(ay - by);
}

/**
 * step_dir(dir) — NPC.OVL:0x0632 (§3.2). Un paso 1..4 sobre (x,y):
 * 1→x++ (este), 2→y-- (norte), 3→x-- (oeste), 4→y++ (sur).
 *
 * 🐛 BUG-FOR-BUG (#108): los cuatro clamps del binario testean la coordenada
 * CONTRARIA a la que acotan (0x0652 `cmp g_cmb_scratch_y,0x20` para clampear X;
 * 0x0666 `cmp x,0` para clampear Y a 0; 0x067a/0x068e igual de cruzados). Con
 * coordenadas 0..31 los cuatro guards son inalcanzables — se portan literales
 * para que nadie los «arregle» al lado correcto y cambie la conducta.
 */
function stepDir(x: number, y: number, dir: number): [number, number] {
  switch (dir) {
    case 1:
      x++;
      if (y > 0x20) x = 0x20; // 0x0652-0x0659 (guard cruzado, inerte en rango)
      return [x, y];
    case 2:
      y--;
      if (x < 0) y = 0; // 0x0666-0x066d
      return [x, y];
    case 3:
      x--;
      if (y < 0) x = 0; // 0x067a-0x0681
      return [x, y];
    default:
      y++;
      if (x > 0x20) y = 0x20; // 0x068e-0x0695
      return [x, y]; // 4
  }
}

/**
 * Direcciones de la SONDA del escáner (`npc_scan` 0x036e-0x0413) — la convención
 * INVERSA de `step_dir`: 1→x−1 (O) · 2→y+1 (S) · 3→x+1 (E) · 4→y−1 (N). La celda
 * visitada guarda en su nibble alto la dirección de sonda padre→celda (0x03ef).
 */
const SCAN_INV = (d: number): number => ((d + 1) & 3) + 1; // 0x05ef `inc·and 3·inc`

/** Escala arriba/abajo — los marcadores EXACTOS 0xC8/0xC9 del raster (0x01e0/
 *  0x01ea) y de los careos de escalera (0x0a8c/0x0ac2/0x0e71/0x0e7b). */
const LADDER_UP_TILE = 200; // 0xC8
const LADDER_DOWN_TILE = 201; // 0xC9

/**
 * `npc_scan` — NPC.OVL:0x032C EXACTO: BFS sobre la rejilla del raster con una
 * COLA CIRCULAR de 32 entradas y CUATRO sondas por celda desencolada.
 *
 *  · La cola arranca con el inicio en la entrada 0 (lectura 0, escritura 1;
 *    0x0334-0x0361).
 *  · Por celda desencolada, la PRIMERA sonda es la dirección del nibble alto de
 *    la propia celda (0x038e-0x0394: el inicio lleva 4 = Norte) y las siguientes
 *    rotan `((d&3)+1)` (0x0442-0x0449) — el sesgo «sigue recto primero» que
 *    diferencia sus caminos de un A*.
 *  · Sonda fuera de rango (x<0 · y<0 · x>32 · y>32 — el 32 PERMITIDO, quirk
 *    `jle 0x20` de 0x0405/0x040d; la fila 32 es la guarda del raster) → se
 *    salta (dx=2, 0x03b9-0x03bf).
 *  · Celda con valor ≥ 0x10 (obstáculo 0x90, visitada dir<<4, o el inicio 0x46)
 *    → se salta (0x03cb-0x03ce).
 *  · Celda libre: guarda su nibble bajo, la MARCA `dir<<4` (0x03df-0x03ef,
 *    borrando el nibble bajo), y si el nibble bajo era 5 → ENCONTRADO: devuelve
 *    sus coordenadas (0x03f1-0x0401, g_cmb_scratch). Si no, la encola — SALVO
 *    con la cola llena (escritura == lectura → se DESCARTA, 0x0416-0x041c), y
 *    la escritura envuelve en 32 (0x0435-0x043b).
 *  · Agotadas las 4 sondas: lectura++ con envoltura (0x0486-0x048f); si
 *    lectura == escritura, la cola se vació → SIN CAMINO (0x0494-0x049a).
 *
 * Muta la rejilla (las marcas son el rastro que lee `npcBacktrace`).
 */
export function npcScan(
  grid: Uint8Array,
  sy: number,
  sx: number,
): { x: number; y: number } | null {
  const qx = new Uint8Array(32);
  const qy = new Uint8Array(32);
  qx[0] = sx;
  qy[0] = sy;
  let read = 0;
  let write = 1;
  for (;;) {
    const cx = qx[read]!;
    const cy = qy[read]!;
    let dir = grid[cy * SMALL + cx]! >> 4; // 0x038e-0x0394
    for (let probe = 0; probe < 4; probe++) {
      let x = cx;
      let y = cy;
      let oob = false;
      switch (dir) {
        case 1: // 0x03b6: Oeste
          x--;
          if (x < 0) oob = true;
          break;
        case 2: // 0x0404: Sur (32 permitido — quirk jle 0x20)
          y++;
          if (y > 0x20) oob = true;
          break;
        case 3: // 0x040c: Este (ídem)
          x++;
          if (x > 0x20) oob = true;
          break;
        case 4: // 0x0412: Norte
          y--;
          if (y < 0) oob = true;
          break;
        default:
          break; // 0x03b3: dir fuera de 1..4 → sonda sin mover (inalcanzable)
      }
      if (!oob) {
        const cell = grid[y * SMALL + x]!;
        if (cell < 0x10) {
          const low = cell & 0xf;
          grid[y * SMALL + x] = (dir << 4) & 0xff; // 0x03ef — borra el nibble bajo
          if (low === 5) return { x, y }; // 0x03f1-0x0401
          if (write !== read) {
            // 0x0416-0x0432: encolar; con la cola llena se DESCARTA.
            qx[write] = x;
            qy[write] = y;
            write++;
            if (write >= 0x20) write = 0; // 0x0435-0x043b
          }
        }
      }
      dir = (dir & 3) + 1; // 0x0442-0x0449
    }
    read++;
    if (read >= 0x20) read = 0; // 0x0486-0x048f
    if (read === write) return null; // 0x0494-0x049a
  }
}

/**
 * `npc_path_backtrace` — NPC.OVL:0x04AC EXACTO. Desde la celda ENCONTRADA
 * (normalmente el destino donde vivía el 5) sigue las marcas padre de la rejilla
 * MARCHA ATRÁS hasta la celda de inicio (nibble bajo 6) escribiendo pares RLE
 * (cuenta, dirección-de-sonda) en `pathBuf`, con `pathIdx = 0` (0x04b9-0x04c0) y
 * tope de 32 bytes = 16 pares (0x0580). Después INVIERTE el buffer in situ
 * (0x0588-0x0624): las cuentas se intercambian por pares y cada dirección se
 * convierte con `((d+1)&3)+1` (0x05ef/0x0604) — la sonda 1 (O) pasa a step_dir 3
 * (O), la 2 (S) a 4 (S)… ⇒ el buffer queda en ORDEN DE MARCHA inicio→destino con
 * las direcciones en la convención de `step_dir`.
 *
 * Quirks fieles: el paso que ATERRIZA en el inicio no se cuenta (0x0512
 * `cmp di,6 / je` salta el ++) — las cuentas suman los pasos inicio→destino
 * exactos; y si el buffer se llena antes de llegar al inicio, el camino queda
 * TRUNCADO por el lado del inicio (el NPC andará tramos desplazados y la
 * maquinaria de atasco lo recogerá). Devuelve los bytes escritos.
 */
export function npcBacktrace(
  npc: Pick<NpcRuntime, "pathBuf" | "pathIdx">,
  fy: number,
  fx: number,
  grid: Uint8Array,
): number {
  const buf = npc.pathBuf;
  npc.pathIdx = 0; // 0x04b9-0x04c0
  let x = fx;
  let y = fy;
  let cell = grid[y * SMALL + x]!;
  let di = cell & 0xf;
  let si = cell >> 4;
  let runDir = si; // [bp-0xa]
  let run = 0; // [bp-4]
  let dx = 0; // bytes escritos
  for (;;) {
    // 0x04f2-0x0508: paso MARCHA ATRÁS (deshace la sonda padre→celda).
    switch (si) {
      case 1:
        x++;
        break; // 0x050a
      case 2:
        y--;
        break; // 0x0546
      case 3:
        x--;
        break; // 0x054c
      case 4:
        y++;
        break; // 0x0552
      default:
        break;
    }
    if (runDir === si && di !== 6) run++; // 0x050d-0x0517
    if (runDir !== si || di === 6) {
      // 0x0524-0x053d: volcar el par (cuenta, dirección).
      buf[dx++] = run & 0xff;
      buf[dx++] = runDir;
      if (di === 6) break; // 0x053e-0x0543 — llegó al inicio
      runDir = si; // 0x0558
      run = 1; // 0x055b
    }
    // 0x0560-0x057e: leer la celda de la posición nueva.
    cell = grid[y * SMALL + x]!;
    si = cell >> 4;
    di = cell & 0xf;
    if (dx >= 0x20) break; // 0x0580 — buffer lleno: camino truncado
  }
  // 0x0588-0x0624: inversión in situ + conversión sonda→step_dir.
  let i = 0;
  let j = dx - 2;
  while (j >= i) {
    const c = buf[i]!;
    buf[i] = buf[j]!;
    buf[j] = c;
    const a = SCAN_INV(buf[i + 1]!);
    const b = SCAN_INV(buf[j + 1]!);
    buf[i + 1] = b;
    buf[j + 1] = a;
    i += 2;
    j -= 2;
  }
  return dx;
}

const STEPS: ReadonlyArray<readonly [number, number]> = [
  [0, -1],
  [0, 1],
  [1, 0],
  [-1, 0],
];

/**
 * `npc_check_schedule` — NPC.OVL:0x0938, cuerpo entero (leído instrucción a
 * instrucción; acta re/notes/npc-maquina-caminata-acta.md §3.2 re-verificada
 * sobre el árbol de hoy). Único llamador: `npc_tick_all` 0x129b, con estado ≤ 1.
 *
 * 1. `0x0961-0x0974`: recorre `times[0..3]` buscando `times[j] == hour`. Si
 *    NINGUNA coincide, `di` sigue en −1 y devuelve **0** (`0x09c4`) ⇒ el llamador
 *    cae en la IA (gate de planta + `npc_ai_step`) SIN mirar si el NPC está en su
 *    puesto. Éste es el pestillo: fuera de la hora de arranque de una ranura no
 *    hay «vuelve a tu puesto».
 * 2. Con coincidencia, `di = schedule_index(hour)` (`0x097d`). Si `servedSlot ==
 *    di` (`0x0982`) la ranura ya está servida ⇒ estado = 1 (`0x0987`).
 * 3. Si la ranura CAMBIÓ: partición completa y disjunta de SEIS casos por plantas
 *    (`0x09cc-0x0a28`), con `Zn` = planta del NPC, `Zd` = planta del puesto de la
 *    ranura nueva y `V` = la planta visible (`g_floor`). Los `jle` del asm son
 *    comparaciones de byte CON SIGNO ⇒ el sótano 0xFF equivale al −1 normalizado
 *    del port:
 *      8 (0x09de): Zn≠V ∧ Zd≠V · 2 (0x09f6): Zn=V ∧ Zd=V · 6 (0x0a04): Zn=V ∧ Zd>V
 *      7 (0x0a0c): Zn=V ∧ Zd<V · 4 (0x0a1c): Zn>V · 5 (0x0a24): Zn<V
 * 4. `0x098e-0x09ba`: con `di ≥ 0`, si la posición viva YA coincide con el puesto
 *    de `di` en las TRES coordenadas, sobrescribe estado = 1 y el resultado pasa a
 *    0; el retorno es `resultado + 1` (`0x0a3c`) — SIEMPRE ≠ 0 con hora casada.
 *    OJO: este camino NO escribe `servedSlot` (sus dos únicos escritores son los
 *    cierres de caminata 0x0ee6/0x1089) ⇒ la «hora inerte»: el NPC en su puesto
 *    re-entra aquí cada tick de esa hora y queda en estado 1 sin ejecutar IA
 *    (ni consumir RNG, mientras el presupuesto de escaneo siga libre).
 *
 * Devuelve 0 = «sin evento de horario, corre la IA»; ≠0 = «despacha por estado».
 */
export function npcCheckSchedule(
  npc: Pick<
    NpcRuntime,
    "times" | "schedX" | "schedY" | "schedZ" | "x" | "y" | "z" | "servedSlot"
  > & { state: number },
  hour: number,
  visibleFloor: number,
): number {
  // 0x0961-0x0974: ¿arranca alguna ranura ESTA hora?
  let matched = false;
  for (let j = 0; j < 4; j++) {
    if (npc.times[j] === hour) {
      matched = true;
      break;
    }
  }
  if (!matched) return 0; // di = -1 → [bp-8] = -1 → 0x09c4 devuelve 0

  const di = scheduleIndex(npc.times, hour); // 0x097d call 0x12e0
  let result = di; // [bp-8] = di (0x098b)
  if (npc.servedSlot === di) {
    npc.state = 1; // 0x0987 — ranura ya servida
  } else {
    // 0x09cc-0x0a28 — partición de plantas (comparaciones CON SIGNO del asm).
    const zn = npc.z;
    const zd = normZ(npc.schedZ[di]!);
    const v = visibleFloor;
    if (zn !== v && zd !== v) npc.state = 8; // 0x09de
    else if (zn === v && zd === v) npc.state = 2; // 0x09f6
    else if (zn === v && zd > v) npc.state = 6; // 0x0a04
    else if (zn === v) npc.state = 7; // 0x0a0c (Zd < V)
    else if (zn > v) npc.state = 4; // 0x0a1c
    else npc.state = 5; // 0x0a24
  }
  // 0x098e-0x09ba: careo de la posición viva contra el puesto de la ranura di.
  if (
    npc.x === npc.schedX[di]! &&
    npc.y === npc.schedY[di]! &&
    npc.z === normZ(npc.schedZ[di]!)
  ) {
    result = 0; // 0x09b5
    npc.state = 1; // 0x09ba — ya está donde toca: nada que caminar
  }
  return result + 1; // 0x0a3c inc — nunca 0 por este camino
}

export class NpcManager {
  private readonly npcs = new Map<number, NpcRuntime[]>();
  /** RNG del kernel (g_rng_seed): stream compartido, consumido por el wander en
   * orden de slot. El juego vivo inyecta la instancia `liveRng` del Game vía
   * `setRng` (game.ts constructor, #57), de modo que el wander rueda sobre el
   * MISMO g_rng_seed global que todo el turno. Sin inyección (arneses de paridad
   * puros) arranca con semilla 0. */
  private rng: OriginalRng;

  constructor(
    private readonly npcData: Record<number, NpcSlot[]>,
    rng?: OriginalRng,
  ) {
    this.rng = rng ?? new OriginalRng(0);
  }

  /** Sustituye el RNG (arnés de paridad). */
  setRng(rng: OriginalRng): void {
    this.rng = rng;
  }

  /**
   * Spawnea los NPCs de una location en sus posiciones de horario para la hora
   * actual. Excluye slot 0, slots vacíos (type 0 y x/y todo 0) y NPCs muertos.
   *
   * `restore=true` (SOLO la carga de partida): en vez de re-inicializar, rehidrata
   * la máquina de caminata desde `state.npcWalk` si pertenece a esta location —
   * la semántica del original, donde las cinco piezas VIAJAN en el .GAM (ventana
   * 0x55A6..0x6605) y un save mid-caminata reanuda donde iba. Toda ENTRADA de mapa
   * (loadSmallMap/wakeSnapNpcs) re-inicializa como `npc_activate_all` 0x00D6:
   * estado=1 (0x011b) · servedSlot=ranura de la hora (0x0152) · pathIdx=−1 (0x015f).
   */
  enterMap(location: number, state: GameState, restore = false): void {
    const slots = this.npcData[location];
    if (!slots) {
      this.npcs.set(location, []);
      return;
    }
    const dead = state.npcDead[location - 1] ?? [];
    const runtimes: NpcRuntime[] = [];
    for (const s of slots) {
      if (s.slot === 0) continue;
      const empty =
        s.type === 0 && s.x.every((v) => v === 0) && s.y.every((v) => v === 0);
      if (empty) continue;
      // Slots-OBJETO (cofre/cadáver/alfombra/caja): NO son personas → los materializa
      // Game.hydrateInteriorObjects en worldObjects, no NpcManager. (task #3)
      if (npcSlotObjectKind(s.type)) continue;
      if (dead[s.slot]) continue;
      const idx = scheduleIndex(s.times, state.time.hour);
      runtimes.push({
        slot: s.slot,
        location,
        type: s.type,
        dialogNumber: s.dialogNumber,
        aiTypes: [...s.aiTypes],
        schedX: [...s.x],
        schedY: [...s.y],
        schedZ: [...s.z],
        times: [...s.times],
        x: s.x[idx]!,
        y: s.y[idx]!,
        z: normZ(s.z[idx]!),
        // npc_activate_all 0x00d6: estado = 1 (0x011b) · servedSlot = ranura de la
        // hora de entrada (0x0152 `[si+0x5f6c] = slot`) · pathIdx = -1 (0x015f).
        // pathBuf/stuck a cero: el cargador no los toca (quedarían del pueblo
        // anterior, inalcanzables con pathIdx=-1); el reset es la elección del
        // port declarada — el residuo del binario no está censado (acta #108).
        state: 1,
        servedSlot: idx,
        pathBuf: new Array<number>(32).fill(0),
        pathIdx: -1,
        stuck: 0,
      });
    }
    // PUERTA #D1 (carga-fiel.ts): con la puerta ABIERTA la AUSENCIA de `npcWalk` significa
    // lo que significa en el binario —la ventana del save viene vacía, NO HAY NADIE— y no
    // «no sé, re-deriva». `town_load_map` con `fresh=0` (TOWN.OVL:0x11FF/0x1203) se salta la
    // lectura del `.NPC` y la activación entera: el mapa se queda con lo que traiga el save.
    // Con la puerta cerrada (default) se re-deriva, que es la divergencia declarada.
    if (restore && !state.npcWalk && cargaFielActiva()) {
      this.npcs.set(location, []);
      this.syncWalkToState(location, state);
      return;
    }
    if (restore && state.npcWalk && state.npcWalk.location === location) {
      const bySlot = new Map(state.npcWalk.slots.map((w) => [w.slot, w]));
      // El binario restaura la ventana 0x55A6..0x6605 VERBATIM y NO relee el .NPC:
      // un slot vaciado por `npc_clear_slot` (TOWN 0x00B0: presencia [0x5F5E+idx*16]=0,
      // crudo 0x00d3-0x00e3) NO existe tras la carga — sólo re-entrar al mapa lo
      // revive. `npcWalk` es el espejo completo de la lista viva (syncWalkToState
      // corre en enterMap, en cada tick y en clearSlot), así que un slot ausente de
      // él ES un slot vaciado: se descarta, no se re-deriva de npcData. (Cerraba el
      // residuo del tren #122: el monstruo cleared revivía antes de hora al load.)
      for (let i = runtimes.length - 1; i >= 0; i--) {
        if (!bySlot.has(runtimes[i]!.slot)) runtimes.splice(i, 1);
      }
      for (const rt of runtimes) {
        const w = bySlot.get(rt.slot);
        if (!w) continue;
        rt.x = w.x;
        rt.y = w.y;
        rt.z = w.z;
        rt.state = w.state;
        rt.servedSlot = w.servedSlot;
        rt.pathBuf = w.pathBuf.slice(0, 32);
        while (rt.pathBuf.length < 32) rt.pathBuf.push(0);
        rt.pathIdx = w.pathIdx;
        rt.stuck = w.stuck;
      }
    }
    this.npcs.set(location, runtimes);
    this.syncWalkToState(location, state);
  }

  /**
   * La mitad NPC de `town_populate_npcs` (TOWN.OVL:0x1694), la que corre el cargador de
   * planta 0x0408 con argumento 1 (escalera/escala 0x052E → `push 1; call 0x408`,
   * 0x0517/0x051d). NO relee el `.NPC` ni reconstruye la lista: recorre los slots 1..31
   * cuyo byte de tipo (DS 0x659E) no es 0 —un slot vaciado por `npc_clear_slot` 0x00B0
   * tiene tipo 0 y sigue fuera— y para cada uno toma el tramo que `schedule_index`
   * (NPC.OVL:0x12E0) elige para `g_hour` (0x16d1), copia X/Y/Z del horario (+3/+6/+9) al
   * registro vivo (0x1841-0x1856, en TODAS las plantas), estado = 1 (0x1856/0x16fc),
   * servedSlot = tramo (0x1705) y pathIdx = −1 (0x170c). El contador de atasco (DS 0x65C2),
   * el diálogo y los AI del horario no se tocan. Batch 24 (H-161).
   */
  snapToSchedule(location: number, state: GameState): void {
    const list = this.npcs.get(location);
    if (!list) return;
    for (const n of list) {
      const idx = scheduleIndex(n.times, state.time.hour);
      n.x = n.schedX[idx]!;
      n.y = n.schedY[idx]!;
      n.z = normZ(n.schedZ[idx]!);
      n.state = 1;
      n.servedSlot = idx;
      n.pathIdx = -1;
    }
    this.syncWalkToState(location, state);
  }

  /**
   * Vuelca la máquina de caminata (cinco piezas + x/y/z) a `state.npcWalk` para
   * que viaje en el save — el espejo del bloque 0x55A6..0x6605 del .GAM. Se llama
   * al entrar al mapa y al final de cada tick (como `tickDoors` → `openDoors`).
   */
  private syncWalkToState(location: number, state: GameState): void {
    const list = this.npcs.get(location);
    if (!list) {
      state.npcWalk = null;
      return;
    }
    state.npcWalk = {
      location,
      slots: list.map((n) => ({
        slot: n.slot,
        x: n.x,
        y: n.y,
        z: n.z,
        state: n.state,
        servedSlot: n.servedSlot,
        pathBuf: [...n.pathBuf],
        pathIdx: n.pathIdx,
        stuck: n.stuck,
      })),
    };
  }

  /**
   * Un tick por turno: cada NPC decide su acción según su aiType y horario.
   * Recorre los NPC en orden de slot (como el bucle n=0..0x1F de npc_tick_all,
   * 0x0DB4) para que el wander consuma el RNG compartido en el mismo orden.
   */
  tick(state: GameState, world: WorldData, doors: DoorManager): void {
    const location = state.position.location;
    if (location === 0) return; // sin NPCs en el overworld
    const list = this.npcs.get(location);
    if (!list || list.length === 0) return;
    this.tickInner(state, world, doors, location, list);
    // Volcado de la máquina al estado serializable (espejo de 0x55A6..0x6605).
    // Fuera de tickInner para que el ABORTO de pasada del estado 3 (0x12d8)
    // también sincronice.
    this.syncWalkToState(location, state);
  }

  private tickInner(
    state: GameState,
    world: WorldData,
    doors: DoorManager,
    location: number,
    list: NpcRuntime[],
  ): void {
    const bySlot = [...list].sort((a, b) => a.slot - b.slot);
    // [bp-0x12] — el PRESUPUESTO DE UN ESCANEO POR TICK (#84): se pone a 0 una vez
    // por llamada a npc_tick_all (0x0dbc), se incrementa justo antes de npc_scan
    // (0x116c) y se fuerza a 1 en los brazos de los estados 4/5 (0x12c2) y 6/7
    // (0x0f14). Sólo un NPC por tick recalcula camino.
    let scanBudget = 0;
    for (const npc of bySlot) {
      // 0x127b: la ranura de la hora ACTUAL se recalcula cada tick ([bp-0xc]).
      const idx = scheduleIndex(npc.times, state.time.hour);

      // 0x1290-0x12a0: con estado ≤ 1, npc_check_schedule decide. Devuelve 0
      // (ninguna ranura arranca esta hora) → gate de planta + npc_ai_step, SIN
      // mirar la distancia al puesto — ÉSTE es el pestillo del binario, que
      // sustituye al disparador derivado-de-distancia que tenía el clon.
      if (npc.state <= 1) {
        const r = npcCheckSchedule(npc, state.time.hour, state.position.floor);
        if (r === 0) {
          this.aiStep(npc, idx, state, world, doors, location, list);
          continue;
        }
        // r ≠ 0 → cae al reparto por estado (mismo tick, 0x12a2).
      }

      // 0x12a2-0x12b9: reparto por estado.
      if (npc.state <= 3) {
        // npc_follow_path 0x0f94: con camino en el buffer (pathIdx > −1 y
        // cuenta del par actual ≠ 0, 0x0f99/0x0fae), un paso por tick GRATIS
        // (no toca el presupuesto — el buffer ya está pagado).
        if (npc.pathIdx > -1 && npc.pathBuf[npc.pathIdx] !== 0) {
          this.followPathStep(npc, idx, world, doors, location, list, state);
          continue;
        }
        // 0x10e0-0x1121: estado 3 SIN camino (pathIdx == −1) — el buffer hacia
        // la escalera se agotó: re-deriva el sentido contra g_floor (schedZ >
        // g_floor → 6, si no → 7) y ★ ABORTA LA PASADA ENTERA de npc_tick_all
        // (0x1116/0x1121 `jmp 0x12d8`): los slots restantes de este tick no se
        // procesan. Quirk portado tal cual.
        if (npc.pathIdx === -1 && npc.state === 3) {
          npc.state =
            normZ(npc.schedZ[idx]!) > state.position.floor ? 6 : 7;
          return;
        }
        // La guarda del presupuesto (0x1124).
        if (scanBudget >= 1) {
          // 0x120e: presupuesto GASTADO → gate de planta + npc_ai_step con la
          // ranura SERVIDA (0x1249 empuja `[bx+0xe]`, no [bp-0xc]) — el quirk que
          // agujerea la hora inerte: un estado-1 detrás del escaneo de otro NPC
          // SÍ ejecuta su IA ese tick.
          this.aiStep(npc, npc.servedSlot, state, world, doors, location, list);
          continue;
        }
        // 0x112d-0x1135: estado 1 sin camino y con presupuesto libre → NADA.
        // (La HORA INERTE: ni IA ni RNG durante toda la hora de arranque.)
        if (npc.state === 1) continue;
        // 0x1138-0x120c: la máquina de ATASCO + MONEDA del re-escaneo.
        //  · stuck == 0 → escanea directo (0x114a `or si,si / je 0x1160`).
        //  · 0 < stuck < 0xC8 → paga la MONEDA `rand(0,2) == 1` (0x114e-0x115b);
        //    si pierde, envejece.
        //  · stuck ≥ 0xC8 (saturado por escaneo fallido) → NUNCA tira la moneda:
        //    envejece 0xC8→0xC9→…→0xCD y al pasar 0xCC se resetea a 0 (0x11e8-
        //    0x1208) — cinco ticks de enfriamiento tras el fallo.
        if (npc.stuck < 0xc8 && (npc.stuck === 0 || this.rng.next(0, 2) === 1)) {
          if (npc.pathIdx === -1) {
            // 0x116c: presupuesto++ JUSTO antes del escaneo.
            scanBudget++;
            // 0x1194 npc_scan(sel=0, destino=puesto, inicio=vivo): rejilla
            // fresca (0x01d2) + BFS de cola circular (0x032c).
            const tx = npc.schedX[idx]!;
            const ty = npc.schedY[idx]!;
            const grid = this.buildScanGrid(
              npc, 0, ty, tx, npc.y, npc.x, idx, state, world, doors, location, list,
            );
            const f = npcScan(grid, npc.y, npc.x);
            if (f) {
              // 0x11b0 backtrace (puesto→vivo, invertido a pares RLE andables)
              // + 0x11b6 `jmp 0x10d6`: stuck=0. El tick del escaneo NO da paso.
              npcBacktrace(npc, ty, tx, grid);
              npc.stuck = 0;
              continue;
            }
            // 0x11bf: fallo → SATURA el contador a 0xC8 y vaga (0x11c5, RNG).
            npc.stuck = 0xc8;
          }
          // 0x11c5-0x11e1: wander sin límite — también la vía del par rancio
          // (pathIdx ≠ −1 con cuenta 0, 0x1165 `jne 0x11c5`), que NO escanea.
          this.wanderStep(
            npc,
            npc.schedX[idx]!,
            npc.schedY[idx]!,
            0,
            getActiveMap(world, location, state.position.floor),
            doors,
            location,
            list,
            state,
          );
          continue;
        }
        // 0x11e8-0x120c: envejecimiento del contador saturado.
        if (npc.stuck >= 0xc8) npc.stuck++;
        if (npc.stuck > 0xcc) npc.stuck = 0;
        continue;
      }
      if (npc.state === 4 || npc.state === 5) {
        // 0x12bc: presupuesto gastado → NADA este tick (ni siquiera IA);
        // libre → se fuerza a 1 (0x12c2) y se recalcula (0x0dd4/0x0dd9).
        if (scanBudget >= 1) continue;
        scanBudget = 1;
        this.recomputeOffFloor(npc, idx, state, world, doors, location, list);
        continue;
      }
      // Estados 6/7/8 → npc_change_floor 0x0ea6.
      if (this.changeFloorStep(npc, idx, state, world, doors, location, list, scanBudget)) {
        scanBudget = 1; // 0x0f14 — el brazo 6/7 gastó el presupuesto planificando
      }
    }
  }

  /**
   * Gate de planta (NPC.OVL:0x1251-0x1259 `cmp [bx+6],g_floor; jne skip`) +
   * `npc_ai_step` 0x0D00 con la ranura que dicte el llamador: `[bp-0xc]` (hora
   * actual) por la vía 0x124e, o `[bx+0xe]` (servida) por la vía 0x120e del
   * presupuesto gastado. El binario SOLO ejecuta la IA (y su consumo de RNG)
   * para los NPC en la planta del jugador.
   */
  private aiStep(
    npc: NpcRuntime,
    slotIdx: number,
    state: GameState,
    world: WorldData,
    doors: DoorManager,
    location: number,
    list: NpcRuntime[],
  ): void {
    if (npc.z !== state.position.floor) return;
    if (!this.floorExists(world, location, npc.z)) return;
    const baseMap = getActiveMap(world, location, npc.z);
    const tx = npc.schedX[slotIdx]!;
    const ty = npc.schedY[slotIdx]!;
    const ai = npc.aiTypes[slotIdx]!;

    switch (ai) {
        case AI_FIXED:
          break; // quieto (0x0DAC)
        case AI_WANDER:
          this.wanderStep(npc, tx, ty, 3, baseMap, doors, location, list, state);
          break;
        case AI_BIG_WANDER:
          this.wanderStep(npc, tx, ty, 0, baseMap, doors, location, list, state);
          break;
        case AI_RUN_AWAY:
          this.runAwayStep(npc, tx, ty, baseMap, doors, location, list, state);
          break;
        case AI_CHASE:
          // MISMO gate de distancia que el 3 (handler compartido `0x0D76`), dirección
          // OPUESTA (rama `0x0884` de `0x06e4`). Ver la cabecera de AI_CHASE.
          sonda.cuenta(AI_CHASE);
          this.chaseStep(npc, baseMap, doors, location, list, state, AI_CHASE);
          break;
        case AI_MERCHANT: {
          // 0x0D40: si el party está a Manhattan<4 de su PUESTO entra en `0x06e4` y
          // PERSIGUE (rama 0x0884, no la de huida que es exclusiva del 3); si no,
          // wander radio 3. Ver la cabecera de AI_MERCHANT.
          const p = state.position;
          const near =
            p.location === location &&
            p.floor === npc.z &&
            manhattan(p.x, p.y, tx, ty) < 4;
          sonda.cuenta(AI_MERCHANT);
          // La puerta ABIERTA es el único punto donde huir y perseguir se distinguen:
          // con ella cerrada el NPC hace wander en los dos códigos. Ver ai-probe.ts.
          if (near) sonda.cuentaDivergente(AI_MERCHANT);
          if (near) this.chaseMove(npc, baseMap, doors, location, list, state);
          else this.wanderStep(npc, tx, ty, 3, baseMap, doors, location, list, state);
          break;
        }
        case AI_HOSTILE:
        case AI_HOSTILE_2:
          // SIN gate de distancia (`0x0D91` entra directo en `0x06e4`) y con la cola
          // de erratismo, que es la que mueve el stream. Ver la cabecera de hostileStep.
          // Sin puerta ⇒ alcanzar el case YA es divergir: el clon viejo huía aquí siempre.
          sonda.cuenta(ai);
          sonda.cuentaDivergente(ai);
          this.hostileStep(npc, baseMap, doors, location, list, state);
          break;
        default:
          break;
      }
  }

  /**
   * Un paso del buffer de camino — `npc_follow_path` 0x0F94 EXACTO. El llamador
   * garantiza `pathIdx > −1` y cuenta del par actual ≠ 0.
   *
   * 1. `0x0fb8-0x0fd2`: la dirección es el byte IMPAR del par (`0x615f + idx*32 +
   *    pathIdx`) y el paso se da con `step_dir` 0x0632 (clamps cruzados y todo).
   * 2. `0x0fec call 0xb9e` (`canMoveStep`): 0 = bloqueado · 1 = pisa · 2 = pisa
   *    Y es el PUESTO de la ranura.
   * 3. Bloqueado (0x109e): atasco++ + `npc_wander` ESTE tick (0x10c3, consume
   *    RNG) y con atasco > 3 el camino se DESCARTA (0x10c6-0x10dc: pathIdx=−1,
   *    atasco=0).
   * 4. Paso bueno: mueve (0x0ff9-0x1024), decrementa la cuenta del par (0x103a) y
   *    resetea el atasco (0x103c). Si la cuenta llegó a 0: avanza al par
   *    siguiente BORRANDO el byte de dirección consumido (0x104a-0x1058
   *    `buf[old+1]=0`), y si se acabó el buffer (≥0x20) o el par siguiente trae
   *    cuenta 0, pathIdx=−1 (0x105d-0x106f). SOLO entonces se mira la llegada:
   *    `can_move == 2` (0x107a) CIERRA el pestillo — `servedSlot = [bp-0xc]` +
   *    estado 1 + pathIdx=−1 (0x1083-0x1095, uno de los DOS escritores de +0xe).
   */
  private followPathStep(
    npc: NpcRuntime,
    idx: number,
    world: WorldData,
    doors: DoorManager,
    location: number,
    list: NpcRuntime[],
    state: GameState,
  ): void {
    const dir = npc.pathBuf[npc.pathIdx + 1]!;
    const [nx, ny] = stepDir(npc.x, npc.y, dir);
    const r = this.canMoveStep(npc, idx, nx, ny, state, world, doors, location, list);
    if (r === 0) {
      // 0x109e: paso bloqueado → atasco++ + wander (RNG) + descarte si > 3.
      npc.stuck++;
      this.wanderStep(
        npc,
        npc.schedX[idx]!,
        npc.schedY[idx]!,
        0,
        getActiveMap(world, location, state.position.floor),
        doors,
        location,
        list,
        state,
      );
      if (npc.stuck > 3) {
        npc.pathIdx = -1;
        npc.stuck = 0;
      }
      return;
    }
    npc.x = nx;
    npc.y = ny;
    npc.pathBuf[npc.pathIdx] = npc.pathBuf[npc.pathIdx]! - 1; // 0x103a
    npc.stuck = 0; // 0x103c
    if (npc.pathBuf[npc.pathIdx] !== 0) return; // 0x1042-0x1047
    const oldDirByte = npc.pathIdx + 1;
    npc.pathIdx += 2; // 0x104a/0x1052
    npc.pathBuf[oldDirByte] = 0; // 0x1058
    if (npc.pathIdx >= 0x20 || npc.pathBuf[npc.pathIdx]! < 1) {
      npc.pathIdx = -1; // 0x105d-0x1074
    }
    if (r === 2) {
      // Cierre del pestillo AL LLEGAR (0x107a-0x1095).
      npc.servedSlot = idx;
      npc.state = 1;
      npc.pathIdx = -1;
    }
  }

  /**
   * `npc_can_move` — NPC.OVL:0x0B9E. Devuelve 0 (bloqueado) · 1 (pisa) · 2 (pisa
   * y la casilla es el PUESTO de la ranura `slotIdx`). Orden exacto del binario:
   * fuera de 0..31 → 0 (0x0ba9-0x0bc1) · grupo de tile `&0xfc == 0x30` → pisa
   * (0x0bec) · grupo 0x90-0x93 (SILLAS) → sólo el estado 2 sigue al careo de
   * 0x0adc, el resto 0 (0x0bf2-0x0c0c: un caminante puede sentarse en su silla-
   * puesto; nadie más pisa sillas) · resto → `walkableForScan` (0x0adc). La
   * OCUPACIÓN (far 0x9472) se carea aparte y anula cualquier resultado (0x0c26-
   * 0x0c42). El flag-fantasma 0x659e==0xFC (0x0bde) no se modela: el port no
   * tiene ese estado (declarado en el acta).
   *
   * Los tiles salen de `effectiveTile` de la PLANTA VISIBLE con puertas
   * regulares→suelo — la fuente única del port donde el binario lee 0x6608/far
   * 0xa172 (stand-in de tiles declarado, heredado de #84).
   */
  private canMoveStep(
    npc: NpcRuntime,
    slotIdx: number,
    x: number,
    y: number,
    state: GameState,
    world: WorldData,
    doors: DoorManager,
    location: number,
    list: NpcRuntime[],
  ): 0 | 1 | 2 {
    if (x < 0 || x >= SMALL || y < 0 || y >= SMALL) return 0;
    const floor = state.position.floor;
    const tile = this.walkTileAt(x, y, floor, world, doors, location);
    const group = tile & 0xfc;
    let walk: 0 | 1 | 2;
    if (group === 0x30) walk = 1;
    else if (group === 0x90 && npc.state !== 2) walk = 0;
    else walk = this.walkableForScan(npc, slotIdx, x, y, floor, tile);
    if (walk === 0) return 0;
    return this.isOccupied(x, y, floor, npc.slot, location, list, state) ? 0 : walk;
  }

  /** Tile efectivo de la planta visible con puertas regulares→suelo (fuente única
   *  de tiles de la máquina de caminata; el binario lee 0x6608 en el raster/0xadc
   *  y el mapa compuesto far 0xa172 en 0xb9e/0xa4a — stand-in declarado). */
  private walkTileAt(
    x: number,
    y: number,
    floor: number,
    world: WorldData,
    doors: DoorManager,
    location: number,
  ): number {
    const baseMap = getActiveMap(world, location, floor);
    const t = doors.effectiveTile(location, floor, x, y, baseMap.tileAt(x, y));
    return t === 184 || t === 186 ? 68 : t;
  }

  /**
   * El careo de transitabilidad del escáner — NPC.OVL:0x0ADC.
   *  · 0x0af5-0x0b1a: si la casilla es EXACTAMENTE el puesto de la ranura
   *    (X, Y y PLANTA), devuelve 2 SIN mirar el tile — así una silla/objeto en el
   *    puesto no impide llegar, y el 2 es la señal de llegada de 0x0b9e/0x107a.
   *  · 0x0b58-0x0b67: con el NPC en estado 3 (camino a la escalera), las escalas
   *    0xC8/0xC9 son transitables — el paso final del brazo 6/7.
   *  · resto: el bitmap de transitabilidad (0x367e ≙ el bitmap canónico de
   *    DATA.OVL 0x54E4 que ya usa `isPassable(t,"foot")`).
   */
  private walkableForScan(
    npc: NpcRuntime,
    slotIdx: number,
    x: number,
    y: number,
    floor: number,
    tile: number,
  ): 0 | 1 | 2 {
    if (
      slotIdx > -1 &&
      npc.schedX[slotIdx] === x &&
      npc.schedY[slotIdx] === y &&
      normZ(npc.schedZ[slotIdx]!) === floor
    ) {
      return 2;
    }
    if (npc.state === 3 && (tile === LADDER_UP_TILE || tile === LADDER_DOWN_TILE)) {
      return 1;
    }
    return isPassable(tile, "foot") ? 1 : 0;
  }

  /**
   * El RASTERIZADOR de la rejilla de escaneo — NPC.OVL:0x01D2. Devuelve la
   * rejilla 32×32 (+ fila de guarda, ver abajo) sobre la que corren `npcScan` y
   * `npcBacktrace`:
   *  · transitable (0x0adc ≠ 0) → 0 · no → 0x90 (0x0222-0x023d);
   *  · con `sel < 0` (buscar ESCALERA): las casillas cuyo tile es EXACTAMENTE
   *    0xC8 (sel −1, subir) o 0xC9 (sel −2, bajar) → 5 (0x0240-0x0259; los
   *    marcadores 0xc8/0xc9 de 0x01e0/0x01ea contra el mapa 0x6608 — las
   *    escaleras 0xC4-0xC7 NO son destino del escáner);
   *  · otros VIVOS a Manhattan < 4 del NPC → 0x90 (0x0284-0x02c7, la tabla
   *    0x5c5a barrida de atrás adelante; port: NPCs de la planta visible);
   *  · el JUGADOR a Manhattan < 4 → 0x90 (0x02d1-0x02fa);
   *  · con `sel ≥ 0`: el DESTINO (ty,tx) → 5 (0x02fd-0x0311);
   *  · el INICIO (sy,sx) → 0x46 SIEMPRE EL ÚLTIMO (0x0313-0x0321): nibble alto
   *    4 = primera sonda N, nibble bajo 6 = marca de inicio del backtrace. Si
   *    inicio == destino, el 0x46 PISA el 5 y el escaneo FALLA (quirk fiel: un
   *    NPC ya en su puesto en estado 2 satura el atasco y vaga).
   *
   * La rejilla lleva 33 filas: la fila 32 es una GUARDA a 0x90. El binario
   * permite sondar y==32/x==32 (`jle 0x20` en 0x0405/0x040d) leyendo la memoria
   * que sigue al buffer [0xb11c] — contenido NO censado; el port la bloquea y lo
   * declara. El aliasing REAL de x==32 (índice y*32+32 == (y+1)*32+0) sí se
   * reproduce, porque opera dentro del mismo array plano.
   */
  private buildScanGrid(
    npc: NpcRuntime,
    sel: number,
    ty: number,
    tx: number,
    sy: number,
    sx: number,
    slotIdx: number,
    state: GameState,
    world: WorldData,
    doors: DoorManager,
    location: number,
    list: NpcRuntime[],
  ): Uint8Array {
    const floor = state.position.floor;
    const grid = new Uint8Array(33 * SMALL);
    grid.fill(0x90, SMALL * SMALL); // fila de guarda (memoria no censada → bloqueada)
    for (let y = 0; y < SMALL; y++) {
      for (let x = 0; x < SMALL; x++) {
        const tile = this.walkTileAt(x, y, floor, world, doors, location);
        const w = this.walkableForScan(npc, slotIdx, x, y, floor, tile);
        grid[y * SMALL + x] = w !== 0 ? 0 : 0x90;
        if (sel < 0) {
          const want = sel === -1 ? LADDER_UP_TILE : LADDER_DOWN_TILE;
          if (tile === want) grid[y * SMALL + x] = 5;
        }
      }
    }
    // 0x0284-0x02c7: otros vivos (personas/monstruos de la planta visible) a
    // Manhattan < 4 del NPC bloquean su casilla.
    for (const o of list) {
      if (o.slot === npc.slot || o.z !== floor) continue;
      if (manhattan(npc.x, npc.y, o.x, o.y) < 4) grid[o.y * SMALL + o.x] = 0x90;
    }
    // 0x02d1-0x02fa: el jugador a Manhattan < 4.
    const p = state.position;
    if (p.location === location && manhattan(npc.x, npc.y, p.x, p.y) < 4) {
      grid[p.y * SMALL + p.x] = 0x90;
    }
    if (sel > -1) grid[ty * SMALL + tx] = 5; // 0x02fd-0x0311
    grid[sy * SMALL + sx] = 0x46; // 0x0313-0x0321 — el último, SIEMPRE
    return grid;
  }

  /**
   * RECOMPUTE de los estados 4/5 (NPC fuera de la planta visible) —
   * NPC.OVL:0x0dd4-0x0e9c. Con presupuesto libre:
   *  1. `0x0e00 call 0x1a0`: escaneo desde el PUESTO buscando la escala más
   *     cercana (sel −1 = 0xC8 para el estado 4 · sel −2 = 0xC9 para el 5).
   *  2. `0x0e2c call 0x32c`: segundo escaneo escala→puesto (sel = 3/4 ≥ 0) +
   *     backtrace (0x0e57) ⇒ el buffer queda con la caminata escala→puesto.
   *  3. `0x0e5a-0x0e98`: si el tile de la escala es el que toca (0xC8 con modo 3
   *     · 0xC9 con modo 4 · o escalera `&0xfc == 0xC4`), MATERIALIZA al NPC en
   *     la escala de la planta visible (far 0xd89a con g_floor).
   *  4. `0x0e9c`: estado = 2 — al tick siguiente camina el buffer a la vista.
   *  Si cualquiera de los dos escaneos falla → nada este tick (0x0e32/0x0e38).
   */
  private recomputeOffFloor(
    npc: NpcRuntime,
    idx: number,
    state: GameState,
    world: WorldData,
    doors: DoorManager,
    location: number,
    list: NpcRuntime[],
  ): void {
    const mode = npc.state === 4 ? 3 : 4; // 0x12cf / 0x0dd4
    const sel = mode === 3 ? -1 : -2; // 0x0def-0x0dfc → 0x01a6-0x01b4
    const tx = npc.schedX[idx]!;
    const ty = npc.schedY[idx]!;
    const g1 = this.buildScanGrid(npc, sel, 0, 0, ty, tx, idx, state, world, doors, location, list);
    const stairs = npcScan(g1, ty, tx);
    if (!stairs) return; // 0x0e32 → 0x1264
    const g2 = this.buildScanGrid(
      npc, mode, ty, tx, stairs.y, stairs.x, idx, state, world, doors, location, list,
    );
    const found = npcScan(g2, stairs.y, stairs.x);
    if (!found) return;
    npcBacktrace(npc, ty, tx, g2); // 0x0e57 — desde el puesto (donde vive el 5)
    const floor = state.position.floor;
    const stile = this.walkTileAt(stairs.x, stairs.y, floor, world, doors, location);
    if (
      (mode === 3 && stile === LADDER_UP_TILE) ||
      (mode === 4 && stile === LADDER_DOWN_TILE) ||
      (stile & 0xfc) === 0xc4
    ) {
      // far 0xd89a(idx, x, y, g_floor): coloca al NPC en la escala visible.
      npc.x = stairs.x;
      npc.y = stairs.y;
      npc.z = floor;
    }
    npc.state = 2; // 0x0e9f
  }

  /**
   * `npc_change_floor` — NPC.OVL:0x0EA6 (estados 6/7/8). Devuelve true si GASTÓ
   * el presupuesto (0x0f14).
   *  · 6/7 sobre la escalera correcta (0x0a4a) — o estado 8 SIEMPRE (0x0ea8/
   *    0x0ead saltan el careo) — → TELETRANSPORTE al puesto de la ranura
   *    (far 0xd89a con schedX/Y/Z, 0x0ec9-0x0edd) + CIERRE 0x0ee6: servedSlot +
   *    pathIdx=−1 + estado 1.
   *  · 6/7 fuera de la escalera: con presupuesto libre (0x0f0b), planifica el
   *    camino vivo→escalera con DOS escaneos (0x0f42 `call 0x1a0` localiza la
   *    escala sel −1/−2 · 0x0f6b re-escanea con el destino plantado · 0x0f86
   *    backtrace) y pasa a estado 3 (0x0f8c — su ÚNICO escritor).
   */
  private changeFloorStep(
    npc: NpcRuntime,
    idx: number,
    state: GameState,
    world: WorldData,
    doors: DoorManager,
    location: number,
    list: NpcRuntime[],
    scanBudget: number,
  ): boolean {
    const floor = state.position.floor;
    let arrive = npc.state === 8; // 0x0ea8/0x0ead: el 8 no carea escalera
    if (!arrive) {
      // 0x0a4a: ¿está el NPC sobre la escalera/escala que toca?
      const tile = this.walkTileAt(npc.x, npc.y, floor, world, doors, location);
      const dest = normZ(npc.schedZ[idx]!);
      arrive =
        dest < floor
          ? tile === LADDER_DOWN_TILE || (tile & 0xf4) === 0xc4 // 0x0a8c-0x0a99
          : tile === LADDER_UP_TILE || (tile & 0xf4) === 0xc4; // 0x0ac2-0x0aca
    }
    if (arrive) {
      // 0x0ebf-0x0ef7: far 0xd89a(idx, schedX, schedY, schedZ) + cierre 0x0ee6.
      const tz = normZ(npc.schedZ[idx]!);
      if (this.floorExists(world, location, tz)) {
        npc.x = npc.schedX[idx]!;
        npc.y = npc.schedY[idx]!;
        npc.z = tz;
      }
      // Sin la planta destino (dato imposible en los .NPC de fábrica) el binario
      // caminaría para siempre; el port cierra igual y lo declara.
      npc.servedSlot = idx; // 0x0ee6
      npc.pathIdx = -1; // 0x0eee
      npc.state = 1; // 0x0ef7
      return false;
    }
    if (scanBudget >= 1) return false; // 0x0f0b
    const sel = npc.state === 6 ? -1 : -2; // 0x0f19-0x0f26
    const g1 = this.buildScanGrid(npc, sel, 0, 0, npc.y, npc.x, idx, state, world, doors, location, list);
    const stairs = npcScan(g1, npc.y, npc.x);
    if (!stairs) return true; // 0x0f48 → 0x0f71 → 0x1264 (presupuesto ya gastado, 0x0f14)
    const mode = npc.state === 6 ? 1 : 2; // [bp-4]
    const g2 = this.buildScanGrid(
      npc, mode, stairs.y, stairs.x, npc.y, npc.x, idx, state, world, doors, location, list,
    );
    const found = npcScan(g2, npc.y, npc.x);
    if (found) {
      npcBacktrace(npc, stairs.y, stairs.x, g2); // 0x0f86
      npc.state = 3; // 0x0f8c
    }
    return true;
  }

  /**
   * Slots-OBJETO de una location con su posición resuelta por horario (task #3). Fuente
   * de la rehidratación por-entrada de los objetos de interior: Game.hydrateInteriorObjects
   * los mapea a worldObjects (cofre → contents Clase C; prop → inerte). Ignora slot 0 y
   * vacíos; NO consulta npcDead (un cofre saqueado se modela retirándolo de worldObjects +
   * no-rehidratar-al-cargar, no con el bitmap de NPCs muertos). Espejo de la re-lectura del
   * bloque .NPC estático al entrar (NPC.OVL 0x0000). */
  objectPlacements(location: number, hour: number): NpcObjectPlacement[] {
    const slots = this.npcData[location];
    if (!slots) return [];
    const out: NpcObjectPlacement[] = [];
    for (const s of slots) {
      if (s.slot === 0) continue;
      const empty =
        s.type === 0 && s.x.every((v) => v === 0) && s.y.every((v) => v === 0);
      if (empty) continue;
      const kind = npcSlotObjectKind(s.type);
      if (!kind) continue;
      const idx = scheduleIndex(s.times, hour);
      out.push({ slot: s.slot, type: s.type, kind, x: s.x[idx]!, y: s.y[idx]!, z: normZ(s.z[idx]!) });
    }
    return out;
  }

  npcsAt(location: number, floor: number): NpcRuntime[] {
    return (this.npcs.get(location) ?? []).filter((n) => n.z === floor);
  }

  /**
   * T1 — Guardias-actor (tile 0x10/0x11, es decir `type & 0xFE == 0x10`) en la
   * planta del jugador, en ORDEN DE SLOT ascendente (fiel al barrido de la tabla
   * de objetos 0x5C5A que hace `guard_wander` TOWN 0x0C78). Los guardias viven en
   * `this.npcs` como cualquier actor-persona (el clasificador `npcSlotObjectKind`
   * no los saca a worldObjects); este getter los aísla para el motor de guardia.
   */
  guardsOnFloor(location: number, floor: number): NpcRuntime[] {
    return (this.npcs.get(location) ?? [])
      .filter((n) => (n.type & 0xfe) === 0x10 && n.z === floor)
      .sort((a, b) => a.slot - b.slot);
  }

  /**
   * T3 — `guard_wander` en vivo (TOWN 0x0C78, PASO 5 del turno de pueblo, call-site
   * 0x165F). Recorre los guardias de la planta del jugador en orden de slot y aplica
   * `guardWanderStep` sobre el RNG COMPARTIDO (`this.rng`, el `liveRng`), de modo que
   * las 1–3 rands por guardia caen en el stream vivo en el punto exacto del binario:
   * DESPUÉS de post_turn/housekeeping y ANTES de `npc_tick_all` (0x166E). El caller
   * (game.ts) lo invoca en el hook `afterHousekeeping` ANTES de `tick`.
   *
   * Cada guardia se muta EN EL ACTO (posición y facing 0x10/0x11) antes de procesar el
   * siguiente, igual que el binario reescribe su entrada de 0x5C5A por iteración — así
   * la comprobación de destino-ocupado del guardia n+1 ve la posición NUEVA del n.
   *
   * Predicados derivados del asm:
   *  · `neighborBlocks` (0x0C4A ×4): algún vecino de la pos ACTUAL con tile 0xA2/0x43.
   *  · `destBlocked` (0x0D55-0x0D8B): destino fuera de rango / no transitable / ocupado.
   *
   * SOLAPAMIENTO = FIEL (witness T4, `re/notes/witness-guard-wander-overlap.md`): el binario
   * DOBLE-PROCESA a un guardia con horario en la planta del jugador — guard_wander (0x165F,
   * aquí) Y npc_tick_all (0x166E, `tick`), cada uno con su propio RNG, en ese orden. El
   * witness lo probó en vivo (divergencia obj≠rt del slot-15 type-16 forzado a la planta:
   * guard_wander escribe el slot de objeto 0x5C5A independientemente de npc_tick_all, que
   * escribe la tabla de horario 0x5F5E). Por eso NO se excluyen los type 16/17 del `tick`:
   * los 19 guardias `aiType 0 = FIXED` son inocuos ahí (quietos, 0 rand) y guard_wander los
   * mueve (1–3 rand); el ÚNICO `aiType 1` (Iolo's Hut loc 13) pasa por AMBOS motores → doble
   * consumo de RNG, que ES la conducta del binario (refuta la exclusión que asumió el scout).
   */
  tickGuards(state: GameState, world: WorldData, doors: DoorManager): void {
    const location = state.position.location;
    if (location === 0) return;
    const list = this.npcs.get(location);
    if (!list || list.length === 0) return;
    const floor = state.position.floor;
    const guards = this.guardsOnFloor(location, floor);
    if (guards.length === 0) return;

    const baseMap = getActiveMap(world, location, floor);
    const tileAt = (x: number, y: number): number =>
      doors.effectiveTile(location, floor, x, y, baseMap.tileAt(x, y));
    // 0x0C4A: vecino de la posición ACTUAL con tile 0xA2/0x43 (bloquea la salida del guardia).
    const neighborBlocks = (x: number, y: number): boolean =>
      STEPS.some(([dx, dy]) => {
        const t = tileAt(x + dx, y + dy);
        return t === 0xa2 || t === 0x43;
      });
    const rand: RandFn = (lo, hi) => this.rng.next(lo, hi);

    for (const g of guards) {
      // 0x0D55-0x0D8B: DESTINO inválido = fuera de rango, tile no transitable, u ocupado.
      // exceptSlot = g.slot (el guardia no se bloquea a sí mismo en su casilla vieja).
      const destBlocked = (x: number, y: number): boolean => {
        if (x < 0 || y < 0 || x >= SMALL || y >= SMALL) return true;
        if (!isPassable(tileAt(x, y), "foot")) return true;
        return this.isOccupied(x, y, floor, g.slot, location, list, state);
      };
      const gs: GuardState = { x: g.x, y: g.y, tile: g.type };
      const step = guardWanderStep(rand, gs, neighborBlocks, destBlocked);
      g.x = step.x;
      g.y = step.y;
      g.type = step.tile; // facing 0x10/0x11 (solo la rama X re-facea; sprite = type+0x100)
    }
  }

  npcAt(location: number, floor: number, x: number, y: number): NpcRuntime | null {
    const list = this.npcs.get(location);
    if (!list) return null;
    return (
      list.find((n) => n.z === floor && n.x === x && n.y === y) ?? null
    );
  }

  /**
   * `npc_dead_bit_set` (TOWN 0x0052) + `npc_clear_slot` (TOWN 0x00B0), colapsados en
   * la única observable del port: el runtime SALE de la lista (la ranura queda vacía;
   * render, ticks y adyacencias dejan de verlo). Lo llama el commit del ataque urbano
   * (`town_attack_engine_commit` TOWN 0x09BC, acta asm-town-zstats-acta.md §2): en el
   * binario el bit de muerto se marca ANTES del combate (acto 1) y la ranura se vacía
   * DESPUÉS (acto 3); como el pueblo no se pinta durante el combate, retirar aquí (al
   * armar el encuentro) es observacionalmente lo mismo, sin ramas.
   *
   * El commit es INCONDICIONAL — no mira el desenlace del combate: huir también deja
   * la ranura vacía. (Es la válvula anti-encajonamiento de 1988: cada gárgola/daemon
   * hostil solo puede acorralarte UNA vez.)
   *
   * Resurrección = la del binario: re-entrar al mapa recarga el .NPC (`enterMap`
   * reconstruye los runtimes desde npcData). Y para los MONSTRUOS eso es FIEL
   * TAMBIÉN en el save (residual 3 de gargolas-hostiles-palacio.md §7, cerrado
   * por REFUTACIÓN — acta §8): `npc_dead_bit_set` TOWN 0x52 tiene GATE (crudo
   * 0x0073-0x0082) y a familia ≥0x80 ≠0xb4 (gárgola 0xb8, rata 0x90, daemon
   * 0xd8…) NO le marca bit — en el binario el hostil también revive al recargar
   * el .NPC. Las PERSONAS sí reciben bit (Game.townNpcDeadBitSet → state.npcDead
   * = SAVED.GAM 0x5B4, ya nativo; `enterMap` lo filtra).
   * ~~⚠ RESIDUO ACOTADO (ficha, no pieza): un save DENTRO del pueblo tras un clear
   * diverge~~ **CERRADO (carril save-residuos)**: `npc_clear_slot` TOWN 0x00B0
   * escribe TODO dentro de la ventana persistida 0x55A6..0x6605 (acta caminata
   * #108 §4.2: SAVED.GAM = 4192 B = 0x1060 desde DS:0x55A6) — palabra de presencia
   * del vivo `[si+0x5F5E]=0` (crudo 0x00d3-0x00e3, también +2/+4/+6/+0xa), los 3
   * primeros bytes del horario 0x5D5E (0x00fd-0x0105), el flag activo 0x659E
   * (0x010c) y el registro-objeto 0x5C5A (0x00e7-0x00f9). El save in-town lleva el
   * slot VACÍO y la carga restaura la ventana verbatim (sin releer el .NPC): el
   * monstruo no revive hasta RE-ENTRAR al mapa. El espejo del port son las dos
   * piezas de abajo: sync inmediato aquí (no esperar al tick) y el filtro de
   * ausentes en el overlay de restore de `enterMap`.
   */
  clearSlot(location: number, slot: number, state: GameState): void {
    const list = this.npcs.get(location);
    if (!list) return;
    const i = list.findIndex((n) => n.slot === slot);
    if (i >= 0) list.splice(i, 1);
    // TOWN 0x00B0 escribe DIRECTAMENTE en la ventana del save: el espejo del port
    // (state.npcWalk) se sincroniza AQUÍ — un save entre el clear y el fin del
    // siguiente tick llevaría el slot todavía presente si se dejara al tick.
    this.syncWalkToState(location, state);
  }

  /**
   * Posesión de NPCs por un Shadowlord urbano (TOWN.OVL:0x1156, F1.10-T6). Con
   * Astaroth (idx 1) o Nosfentor (idx 2) presente, recorre los 32 slots del .NPC
   * en orden y para cada uno **consume 1 rand(0,1)** (`possessGateRoll`). Consume
   * EXACTAMENTE 32 rands (uno por slot) — el orden del stream es el del bucle
   * si=0..0x1F del binario. Faulinei (0) y ninguno → no entra aquí (0 rands).
   * Llamar tras `enterMap`.
   *
   * 🐛 BUG-FOR-BUG (TOWN 0x111f-0x1121): el gate de tipo del 0x10f2 usa el tipo
   * CONSTANTE del **slot #4** del pueblo (`slot4IsPerson`), no el del slot evaluado.
   * Ramas por Shadowlord (contrastadas 0x85e vs 0x8d4):
   *  - Astaroth (0x85e): posee si pasa el gate — SIN re-chequeo → puede poseer
   *    NPCs no-persona si el slot #4 es persona.
   *  - Nosfentor (0x8d4 0x0905/0x090c): RE-CHEQUEA el tipo REAL del slot → sólo
   *    person-tiles; y si el slot #4 NO es persona, el gate falla para todos → no
   *    posee a NADIE aunque haya person-NPCs. (En las 8 ciudades de la virtud el
   *    slot #4 siempre es persona, así que el bug queda DORMIDO en juego canónico.)
   */
  possessForShadowlord(location: number, presentIdx: number, rand: RandFn): void {
    const dialog = POSSESSED_DIALOG[presentIdx];
    if (dialog === undefined) return; // sólo Astaroth/Nosfentor
    const aiType = POSSESSED_AITYPE[presentIdx]!;
    const slots = this.npcData[location] ?? [];
    const slotByIndex = new Map(slots.map((s) => [s.slot, s]));
    // 🐛 gate de tipo CONSTANTE = tipo del slot #4 del pueblo (TOWN 0x111f-0x1121).
    const slot4IsPerson = isPersonType(slotByIndex.get(4)?.type ?? 0);
    const rtByIndex = new Map((this.npcs.get(location) ?? []).map((r) => [r.slot, r]));
    for (let si = 0; si < NPC_SLOT_COUNT; si++) {
      const s = slotByIndex.get(si);
      const present = !!s && s.times.some((t) => t !== 0); // 0x1109
      const gate = possessGateRoll(present, slot4IsPerson, rand); // rand SIEMPRE
      if (!gate) continue;
      // Nosfentor (0x8d4) re-chequea el tipo REAL del slot; Astaroth (0x85e) no.
      if (presentIdx === 2 && !isPersonType(s?.type ?? 0)) continue;
      const rt = rtByIndex.get(si);
      if (rt) {
        rt.dialogNumber = dialog;
        rt.aiTypes = [aiType, aiType, aiType];
      }
    }
  }

  /**
   * Alarma de arresto «Then defend thyself, rogue!» — TOWN.OVL:0x0958 (único
   * caller: la rama 'N' del arresto, 0x12ae@0x1343). Recorre los 32 slots en orden
   * (sólo records runtime presentes, 0x096d `cmp word[0x5f5e+si*16],0`):
   *  - tile de GUARDIA {0xfc, 0xd8, 0x70} (0x097a-0x0989) → 0x085e: aiTypes
   *    fijados a HOSTIL (tile>=0x2f → 7, else 6; 0x0868-0x0876) y HORARIO BORRADO
   *    (times=[0,0,0,0], 0x0890-0x0896) — hostilidad permanente hasta recargar el
   *    .NPC (re-entrada al pueblo). No toca dialogNumber.
   *  - resto → rand(0,255) (kernel 0x9ec2 [= CS 0x2092 → ULTIMA.EXE:0x2092] @0x0992-0x0999; consume SIEMPRE 1 rand
   *    por slot no-guardia presente) y si < 0x80 → 0x08d4: si el tipo es persona
   *    ([0x40,0x74), 0x0905-0x0911) y (dialog==0xfe ∨ tiene horario, 0x08ec/0x0917)
   *    → dialogNumber=0xfd («Don't hurt me!») + aiTypes=[3,3,3] (0x092f-0x094b).
   * Mismos helpers 0x085e/0x08d4 que la posesión de Shadowlord (otro caller).
   */
  arrestAlarm(location: number, rand: RandFn): void {
    const list = this.npcs.get(location) ?? [];
    const bySlot = [...list].sort((a, b) => a.slot - b.slot);
    for (const rt of bySlot) {
      const tile = rt.type & 0xff;
      if (tile === 0xfc || tile === 0xd8 || tile === 0x70) {
        const hostil = tile >= 0x2f ? 7 : 6; // 0x0868-0x0876
        rt.aiTypes = [hostil, hostil, hostil]; // 0x08b1-0x08c8
        rt.times = [0, 0, 0, 0]; // 0x0890-0x0896
        continue;
      }
      const r = rand(0, 0xff); // 0x0992-0x0999: SIEMPRE (parte del stream)
      if (r >= 0x80) continue; // 0x099c cmp ax,0x80; jge skip
      const hasSchedule = rt.times.some((t) => t !== 0); // 0x08e8-0x08fa
      if (!isPersonType(tile)) continue; // 0x0905-0x0911 (fuera de [0x40,0x74))
      if (rt.dialogNumber !== 0xfe && !hasSchedule) continue; // 0x0917-0x0921
      rt.dialogNumber = 0xfd; // 0x092f («Don't hurt me!»)
      rt.aiTypes = [3, 3, 3]; // 0x093d-0x094b
    }
  }

  private floorExists(world: WorldData, location: number, z: number): boolean {
    const loc = world.smallMaps.get(location);
    return !!loc && loc.floors.some((f) => f.z === z);
  }

  /** ¿(x,y) del piso `floor` está ocupado por el jugador u otro NPC? */
  private isOccupied(
    x: number,
    y: number,
    floor: number,
    exceptSlot: number,
    location: number,
    list: NpcRuntime[],
    state: GameState,
  ): boolean {
    const p = state.position;
    if (p.location === location && p.floor === floor && p.x === x && p.y === y) {
      return true;
    }
    for (const o of list) {
      if (o.slot !== exceptSlot && o.z === floor && o.x === x && o.y === y) {
        return true;
      }
    }
    return false;
  }

  /**
   * npc_wander — NPC.OVL:0x0C50 (§4.1). Consumo de RNG EXACTO:
   *  1. `rand(0,255)`: si bit3 (`&8`) es 0 el NPC NO se mueve este turno (~50%).
   *  2. `rand(0,64)`: `dir = (r & 3) + 1` (1..4, dirección BLINDA; span 65).
   * Se mueve sólo si ESA dirección concreta cae en radio Manhattan (`maxdist`;
   * 0 = sin límite, big-wander) y la casilla es transitable + libre. Si no, se
   * queda quieto (no reintenta otra dirección). Las puertas bloquean el wander.
   */
  private wanderStep(
    npc: NpcRuntime,
    postX: number,
    postY: number,
    maxdist: number,
    baseMap: ActiveMap,
    doors: DoorManager,
    location: number,
    list: NpcRuntime[],
    state: GameState,
  ): void {
    // 0x0C57: rand(0,255); test al,8 → si bit3=0 no se mueve (RNG ya consumido).
    if ((this.rng.next(0, 255) & 8) === 0) return;
    // 0x0C68: `push 0x40; call rand0` = rand(0,64) AMBOS inclusive (span 65);
    // and 3; inc → dir 1..4. OJO: (raw%65)&3 ≠ raw&3 (65 no es múltiplo de 4).
    const dir = (this.rng.next(0, 64) & 3) + 1;
    const [nx, ny] = stepDir(npc.x, npc.y, dir);
    if (nx < 0 || ny < 0 || nx >= SMALL || ny >= SMALL) return;
    // 0x0C9A: si maxdist!=0 y Manhattan(destino,puesto) > maxdist → aborta.
    if (maxdist !== 0 && manhattan(nx, ny, postX, postY) > maxdist) return;
    const tile = doors.effectiveTile(location, npc.z, nx, ny, baseMap.tileAt(nx, ny));
    if (!isPassable(tile, "foot")) return;
    if (this.isOccupied(nx, ny, npc.z, npc.slot, location, list, state)) return;
    npc.x = nx;
    npc.y = ny;
  }

  /**
   * aiType 3/6 (run-away, niños) — NPC.OVL:0x0D76 (§4.0). Si `dist(party,npc) >= 4`
   * (Manhattan) el NPC no hace nada (0x0D8C `cmp ax,4; jge`); si el party está a
   * <4 huye. La rutina exacta de huida (target_for_attack 0x06E4) está ligada a
   * la hostilidad (Task 3.9/3.10); aquí aproximamos con un paso greedy que
   * maximiza la distancia Manhattan (⚠️→formulado).
   */
  private runAwayStep(
    npc: NpcRuntime,
    _postX: number,
    _postY: number,
    baseMap: ActiveMap,
    doors: DoorManager,
    location: number,
    list: NpcRuntime[],
    state: GameState,
  ): void {
    const p = state.position;
    const near =
      p.location === location &&
      p.floor === npc.z &&
      manhattan(npc.x, npc.y, p.x, p.y) < 4;
    if (!near) return; // dist >= 4 → quieto (como el binario)
    this.fleeStep(npc, baseMap, doors, location, list, state);
  }

  /**
   * PERSECUCIÓN del aiType 6 — `NPC.OVL:0x0D76` (gate de distancia, compartido con el 3)
   * → `0x06e4`, rama `0x0884`. Mismo gate que `runAwayStep`, criterio INVERTIDO:
   * `0x088a cmp [bx],ax / 0x088c jge skip / 0x088e mov di,si` ⇒ adopta la puntuación
   * ESTRICTAMENTE MENOR = la casilla que MÁS ACERCA al party. Barrido en el orden de
   * `STEPS` y con el primero-gana en caso de igualdad (el `jge` descarta los empates),
   * igual que `fleeStep` con el signo cambiado.
   *
   * NO consume RNG: las tres tiradas de `0x06e4` están tras gates de aiType 3 / 5 / 7
   * (`0x0824`, `0x0892`, `0x0898`), y ninguna alcanza al 6. La rama `0x0884` no tiene
   * moneda — por eso este arreglo NO mueve el stream.
   */
  private chaseStep(
    npc: NpcRuntime,
    baseMap: ActiveMap,
    doors: DoorManager,
    location: number,
    list: NpcRuntime[],
    state: GameState,
    /** Sólo para la sonda de alcanzabilidad; no interviene en el comportamiento. */
    aiTypeParaSonda?: number,
  ): void {
    const p = state.position;
    const near =
      p.location === location &&
      p.floor === npc.z &&
      manhattan(npc.x, npc.y, p.x, p.y) < 4;
    if (!near) return; // dist >= 4 → quieto (gate 0x0d8c, idéntico al del 3)
    // Puerta abierta = el punto donde el clon viejo (huir) y el nuevo (perseguir) divergen.
    if (aiTypeParaSonda !== undefined) sonda.cuentaDivergente(aiTypeParaSonda);
    this.chaseMove(npc, baseMap, doors, location, list, state);
  }

  /**
   * El PASO de la rama `0x0884`, SIN puerta de distancia — el barrido y el criterio,
   * nada más. Se separó de `chaseStep` al portar el aiType 4 (#82): el gate `0x0d8c`
   * pertenece al DESPACHADOR `0x0D76` (aiType 3 y 6), no al cuerpo de `0x06e4`, y el 4
   * llega por `0x0D40` con **su propia** puerta, que además mide contra otra cosa —
   * el PUESTO, no el NPC vivo (#84). Meterlo dentro de `chaseStep` le aplicaba al 4 un
   * gate que el binario no le aplica; hoy sería inerte porque el `switch` sólo se alcanza
   * con el NPC en su puesto, pero es precisamente la clase de coincidencia que se rompe
   * sola cuando alguien toca esa guarda.
   */
  private chaseMove(
    npc: NpcRuntime,
    baseMap: ActiveMap,
    doors: DoorManager,
    location: number,
    list: NpcRuntime[],
    state: GameState,
  ): void {
    const p = state.position;
    let best: { x: number; y: number } | null = null;
    let bestDist = manhattan(npc.x, npc.y, p.x, p.y);
    for (const [dx, dy] of STEPS) {
      const nx = npc.x + dx;
      const ny = npc.y + dy;
      if (nx < 0 || ny < 0 || nx >= SMALL || ny >= SMALL) continue;
      const tile = doors.effectiveTile(location, npc.z, nx, ny, baseMap.tileAt(nx, ny));
      if (!isPassable(tile, "foot")) continue;
      if (this.isOccupied(nx, ny, npc.z, npc.slot, location, list, state)) continue;
      const d = manhattan(nx, ny, p.x, p.y);
      if (d < bestDist) {
        bestDist = d;
        best = { x: nx, y: ny };
      }
    }
    if (best) {
      npc.x = best.x;
      npc.y = best.y;
    }
  }

  /**
   * HOSTIL de los aiType 5 y 7 — `NPC.OVL:0x0D91` → `0x06e4` **entero**, incluida la cola
   * de `0x0892` que es exclusiva de estos dos y es la que MUEVE EL STREAM.
   *
   * Tres piezas, todas leídas del cuerpo (#78, acta re/notes/npc-aitype57-hostil.md):
   *
   * 1. **Sin gate de distancia.** `0x0D91` empuja y llama sin el `cmp ax,4` que el 3 y el
   *    6 traen de `0x0D76`: estos dos actúan a cualquier distancia.
   * 2. **Adyacencia = no moverse y no tirar.** `0x0723 cmp ax,1` + `0x0728 cmp [bp-2],3 /
   *    jle`: con el party a Manhattan 1 y aiType > 3 el flujo se va a la vía de ataque
   *    adyacente (`0x07be` para el 7) y **sale antes de tocar el generador**. El aviso de
   *    ataque en sí (`g_unk_65be`) es la maquinaria de hostilidad del clon y NO se modela
   *    aquí; lo que sí es obligatorio replicar es el **cero consumo**.
   * 3. **Adopción por PRIMERA mejora, no por mínimo.** `0x0884 cmp [bx],ax / jge 0x854` con
   *    `ax = [bp-4]` = la distancia ACTUAL del NPC, que **no se actualiza dentro del
   *    bucle**; al adoptar hace `mov di,si / jmp 0x84b`, que **sale del bucle**. Con
   *    vecindad de 4 y Manhattan toda mejora vale exactamente −1, así que «primera mejora»
   *    y «mínimo» coinciden — por eso `chaseStep` (que sí busca el mínimo) es equivalente.
   *
   * Y la cola de erratismo (`0x0892-0x08ee`), que es el consumo:
   * - `0x08a5` **tira SIEMPRE** `rand_range(0,0x3f)` y sigue sólo si sale `< 0x10` (25%).
   * - Si pasa, recorre las cuatro direcciones saltando la ya elegida y las impasables: la
   *   **primera viable se adopta SIN tirar** (`0x08cb je 0x8dc`) y cada siguiente cuesta
   *   una tirada (`0x08d4`) que la adopta si sale `< 0x10`.
   * ⇒ **1 tirada siempre, 3 como mucho** cuando había dirección elegida (una de las cuatro
   * queda excluida), y **4 en el caso sin candidato** (`[bp-0x18] = -1`, no se excluye
   * ninguna). La cola puede MOVER al NPC aunque ninguna dirección mejorara.
   *
   * Convenciones heredadas y NO derivadas aquí, declaradas para que nadie las lea como
   * medidas: el orden de las cuatro direcciones (`STEPS` vs el que impone `0x0632`) y que
   * la distancia de `0x06a0` sea Manhattan. Las comparte con `fleeStep` desde antes.
   */
  private hostileStep(
    npc: NpcRuntime,
    baseMap: ActiveMap,
    doors: DoorManager,
    location: number,
    list: NpcRuntime[],
    state: GameState,
  ): void {
    const p = state.position;
    if (p.location !== location || p.floor !== npc.z) return;

    const cur = manhattan(npc.x, npc.y, p.x, p.y);
    if (cur === 1) return; // 0x0723/0x0728 — ataque adyacente: ni paso ni tirada

    // Puntuación de las CUATRO direcciones (0x075a-0x07d8, si=1..4). Lo intransitable u
    // ocupado recibe el centinela 0x63 (0x07c9), que los dos barridos saltan.
    const IMPASSABLE = 0x63;
    const score: number[] = [];
    const cell: ({ x: number; y: number } | null)[] = [];
    for (const [dx, dy] of STEPS) {
      const nx = npc.x + dx;
      const ny = npc.y + dy;
      let ok = nx >= 0 && ny >= 0 && nx < SMALL && ny < SMALL;
      if (ok) {
        const tile = doors.effectiveTile(location, npc.z, nx, ny, baseMap.tileAt(nx, ny));
        ok =
          isPassable(tile, "foot") &&
          !this.isOccupied(nx, ny, npc.z, npc.slot, location, list, state);
      }
      score.push(ok ? manhattan(nx, ny, p.x, p.y) : IMPASSABLE);
      cell.push(ok ? { x: nx, y: ny } : null);
    }

    // 0x0884: primera dirección ESTRICTAMENTE mejor que la actual, y se corta ahí.
    let chosen = -1;
    for (let i = 0; i < STEPS.length; i++) {
      if (score[i] === IMPASSABLE) continue;
      if (score[i]! < cur) {
        chosen = i;
        break;
      }
    }

    // 0x08a5: la tirada del 25% se paga SIEMPRE, haya candidato o no.
    if (this.rng.next(0, 0x3f) < 0x10) {
      let pick = chosen;
      for (let i = 0; i < STEPS.length; i++) {
        if (i === chosen) continue; // 0x08bb — la ya elegida no compite
        if (score[i] === IMPASSABLE) continue; // 0x08c0
        if (pick === chosen) {
          pick = i; // 0x08cb — la primera viable entra GRATIS
          continue;
        }
        if (this.rng.next(0, 0x3f) < 0x10) pick = i; // 0x08d4
      }
      chosen = pick;
    }

    const dest = chosen >= 0 ? cell[chosen] : null;
    if (dest) {
      npc.x = dest.x;
      npc.y = dest.y;
    }
  }

  /**
   * flee-move — hoy sólo del aiType 3 (`0x0D76`→`0x06e4` rama `0x082a`) y del 4 cuando
   * el party se le acerca al puesto. El 5 y el 7 SALIERON de aquí en #78: no huían.
   * Un paso a la casilla adyacente transitable que MÁS aleja del party (Manhattan).
   *
   * ⚠️ DOS huecos declarados y NO arreglados aquí, los dos medidos en #78:
   * - **El 3 tira moneda por cada mejora salvo la primera.** `0x0831 cmp di,-1 / je 0x844`
   *   deja entrar gratis al primer candidato estrictamente mejor; los siguientes pasan por
   *   `0x083d rand_range(0,1)` y sólo se adoptan si sale 1 ⇒ **hasta 3 tiradas**. Este
   *   método no las gasta. Arreglarlo MUEVE STREAM, y el 3 lo llevan **2 NPC en todo el
   *   juego**, así que no entra en esta tanda.
   * - **El aiType 4 no debería llamar aquí.** `0x0D40` mide al PUESTO y, si el party está
   *   a <4, entra en `0x06e4` con aiType 4, que cae en la rama `0x0884` = **ACERCARSE**,
   *   la misma del 6. Es la TERCERA fila con la etiqueta invertida. No se toca en #78 a
   *   propósito: son 33 NPC en 42 ranuras y están en TODAS las localizaciones de las cinco
   *   escenas de sellos, así que meterlo aquí haría ilegible la ventana. Ficha aparte.
   */
  private fleeStep(
    npc: NpcRuntime,
    baseMap: ActiveMap,
    doors: DoorManager,
    location: number,
    list: NpcRuntime[],
    state: GameState,
  ): void {
    const p = state.position;
    let best: { x: number; y: number } | null = null;
    let bestDist = manhattan(npc.x, npc.y, p.x, p.y);
    for (const [dx, dy] of STEPS) {
      const nx = npc.x + dx;
      const ny = npc.y + dy;
      if (nx < 0 || ny < 0 || nx >= SMALL || ny >= SMALL) continue;
      const tile = doors.effectiveTile(location, npc.z, nx, ny, baseMap.tileAt(nx, ny));
      if (!isPassable(tile, "foot")) continue;
      if (this.isOccupied(nx, ny, npc.z, npc.slot, location, list, state)) continue;
      const d = manhattan(nx, ny, p.x, p.y);
      if (d > bestDist) {
        bestDist = d;
        best = { x: nx, y: ny };
      }
    }
    if (best) {
      npc.x = best.x;
      npc.y = best.y;
    }
  }
}
