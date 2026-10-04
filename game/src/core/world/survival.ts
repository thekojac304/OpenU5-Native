/**
 * Supervivencia y reloj EXACTOS del original (Task 3.1).
 *
 * Port bit a bit de las rutinas del kernel de ULTIMA.EXE re-derivadas en
 * re/notes/kernel-survival.md (citas asm allí; paridad DOSBox en
 * re/parity/kernel/):
 *
 *  - kernel_advance_clock (0x4F7C): el reloj avanza N MINUTOS por acción
 *    (1 en pueblo, 2 en exterior, +2/+4 por terreno lento). Antorcha y
 *    hechizo de luz se consumen en minutos. Rollovers 60 min → hora,
 *    24 h → día, 28 días → mes, 13 meses → año; monthsAtInn++ al mes.
 *  - kernel_turn_housekeeping (0x2AE8): veneno 1 HP/turno; comidas a las
 *    6/12/18 (food -= miembros vivos no D/S); "Starving!" con food==0 en
 *    CADA cambio de hora (rand(1,8) de daño por miembro vivo); anillo de
 *    regeneración (1/8 de +1 HP).
 *  - Ignite torch (CMDS.OVL 0x0D98): 240 min fijos fuera de mazmorra,
 *    +112+rand(0,15) (tope 255) dentro.
 *  - Luz (kernel 0x50A1): noche 2, día 50, rampa de amanecer/atardecer,
 *    mínimos 18 (hechizo) / 10 (antorcha).
 *
 * Divergencias conscientes (documentadas en el informe y en FIDELITY):
 *  - turnsSinceStart no satura (el original usa un u8 saturante a 255).
 *  - La reubicación de Shadowlords a medianoche (0x4FF5→0x5004) la corre
 *    advanceClock en el rollover de día CUANDO recibe el `rand` vivo (F.2);
 *    los callers puros deterministas no lo pasan y conservan el no-op.
 *  - [CERRADA en #54 pieza 9] Los ceros de fin de mes ya NO se omiten para lo que el clon
 *    modela: `skullTreeFoundDay` (0x57B2) se borra en el rollover de mes, como el binario.
 *    Quedan fuera 0x5858-0x585A (las 3 parcelas: su flujo de cosecha no está portado) y
 *    0x5959 (semántica sin cerrar). El texto de abajo se conserva como historial del
 *    hallazgo; los dos «sin identificar» que mencionaba están ahora acotados.
 *  - Los ceros de fin de mes sobre 0x5858-0x585A/0x57B2/0x5959 se omitían.
 *    ⚠ La razón vieja («globals sin identificar») YA NO VALE para dos de ellos
 *    [ACTUALIZADO t#58]:
 *      · 0x57B2 = día de la última vez que se halló el árbol de skull keys de
 *        Minoc. Identificado y YA MODELADO en world/search.ts como
 *        `state.skullTreeFoundDay` (SJOG 0x0574-0x0580 lee el gate diario;
 *        0x05b1-0x05b4 escribe, escritura exclusiva de si==0x0e).
 *      · 0x5858 = TABLA de sellos-de-día, uno por PARCELA de cosecha (no un
 *        byte suelto): indexada por el mismo `si` que las tablas paralelas
 *        0x3e66 (x) / 0x3e6a (y) / 0x3e6e (reactivo) de
 *        `search_daily_reagent_patch` SJOG 0x045a — lee el gate en 0x0486-0x048d
 *        y sella el día en 0x048f.
 *      · 0x585A y 0x5959 siguen SIN identificar.
 *    CONSECUENCIA de seguir omitiendo el borrado: en el clon el árbol de skull
 *    keys y las parcelas arrastran su sello PARA SIEMPRE, así que no vuelven a
 *    dar nada al cambiar de mes. Es MECÁNICA AUSENTE, no un detalle de prosa.
 *    Cablearlo toca advanceClock (rollover de mes) + estado persistente ⇒ va por
 *    el lote de mecánica #54 (pieza 9), NO desde aquí.
 */
import type { GameState } from "../state.js";
import { OriginalRng } from "../rng-original.js";
import { MOON_PHASE_OFFSET } from "./moongates.js";

/** rand_range(lo, hi) del kernel — ambos inclusive. */
export type RandFn = (lo: number, hi: number) => number;

const sharedRng = new OriginalRng(0);
/** RNG por defecto (OriginalRng compartido); los sistemas deterministas inyectan el suyo. */
export const defaultRand: RandFn = (lo, hi) => sharedRng.next(lo, hi);

/** Coste en minutos de una acción según contexto (bucles TOWN 0x15D4 / MAINOUT 0xC39). */
export const MINUTES_PER_ACTION_TOWN = 1;
export const MINUTES_PER_ACTION_OUTDOORS = 2;
/**
 * Coste en minutos del turno de MAZMORRA — #159, derivado del bucle DUNGEON 0x0e2e.
 *
 * El bucle NO tiene un `advance_clock` propio: el camino sin hechizo temporal SALTA
 * DENTRO del bloque de Quickness y reutiliza su mismo `call`. Cola del bucle:
 *
 * | offset   | mnemónico                              | lectura |
 * |----------|----------------------------------------|---------|
 * | `0x0fd0` | `cmp [bp-0xe],0` / `jne 0xf93`         | sólo se cobra si el despachador devolvió 0 |
 * | `0x0fd6` | `cmp [g_time_spell],0x54` / `je 0xf1e` | 'T' An Tym |
 * | `0x0fe0` | `cmp [g_time_spell],0x51` / `je 0xf1e` | 'Q' Rel Tym |
 * | `0x0fea` | `mov di,1`                             | sin hechizo: di = 1 |
 * | `0x0fed` | `mov ax,di`                            | ax = 1 = el argumento |
 * | `0x0fef` | `jmp 0xf2e`                            | **entra en el bloque de 'Q', en su `push ax`** |
 * | `0x0f2e` | `push ax`                              | |
 * | `0x0f2f` | `call 0x4f7c`                          | `advance_clock(1)` |
 *
 * El salto está verificado byte a byte: `e9 3c ff` en 0x0fef ⇒ 0x0ff2 + (−196) = 0x0f2e.
 * Por eso un censo de call-sites ve UN solo `advance_clock` en DUNGEON.OVL (el de 0x0f2f,
 * bajo el `cmp 'Q'` de 0x0f1e) y concluye que el paso normal es gratis: la conclusión es
 * un artefacto de contar `call` en vez de seguir el `jmp`.
 *
 * Los otros dos regímenes NO son este coste y no se modelan aquí:
 *  · 'Q' (Rel Tym) — 0x0f25 `xor di,1` / 0x0f29 `je 0xf36`: cobra 1 minuto cada DOS
 *    turnos. (El `sar` interno de advance_clock 0x4f94 es no-op con n=1: la mitad la
 *    pone el bucle, no el kernel.)
 *  · 'T' (An Tym) — cae en 0x0f34 `sub di,di`: CERO llamadas (y 0x4fa6 lo rechazaría igual).
 *
 * El housekeeping va DESPUÉS (0x0f84 → DUNGEON 0x0c76, cuya cola común 0x0e1f/0x0e22
 * llama a kernel_status_redraw 0x2900 y kernel_turn_housekeeping 0x2AE8), igual que en
 * MAINOUT (0xc39 reloj → 0xcd3 housekeeping).
 *
 * Antes de #159 los CINCO call-sites de mazmorra dejaban el argumento en `undefined` y
 * caían en `minutesPerAction(position.location)`; como el clon deja `position.location`
 * en 0 dentro de la mazmorra (ver `Game.effectiveLocation`, #123), salía la rama de
 * EXTERIOR y el turno costaba 2 minutos — el doble. Los cinco: `game.ts` (comando de
 * mazmorra y su gemelo de turno consumido), y en `dungeon-cmds.ts` `dungeonCommand`,
 * `dungeonMagicChangeLevel` (Uus/Des Por) y `dungeonSpellTurn`. Los dos `advanceTurn`
 * de `__parity__/run.ts` NO son de esta familia y se quedan con `undefined`: ese arnés
 * es de estado puro y ningún escenario suyo entra en mazmorra.
 *
 * ⚠ ALCANCE DE LA COBERTURA, medido y no supuesto: al cerrar #159 NINGÚN test de las
 * 309 unidades mide este reloj a valor absoluto. Control de sensibilidad: poniendo esta
 * constante en 7 la suite entera sigue verde (3945 passed, 0 failed). El candado de
 * `dungeon-effective-location.test.ts` clava el LITERAL 1 justamente por eso — un test
 * que comparase contra esta constante bendeciría cualquier valor que se le pusiera.
 */
export const MINUTES_PER_ACTION_DUNGEON = 1;

/** Horas de comida: a las 6:00, 12:00 y 18:00 (kernel 0x2B7A-0x2B8D). */
export const MEAL_HOURS: readonly number[] = [6, 12, 18];

/** Ignite fuera de mazmorra: 240 minutos fijos (CMDS 0x0DD6). */
export const TORCH_MINUTES = 0xf0;
/** Ignite en mazmorra: 112 + rand(0,15), saturando en 255 (CMDS 0x0DBA-0x0DD0). */
const TORCH_MINUTES_DUNGEON_BASE = 0x70;

/** Rampa de luz del amanecer (5:00-5:59, paso de 10 min) — DS 0x6A80. */
export const SUNRISE_LIGHT_RAMP: readonly number[] = [2, 5, 10, 20, 34, 49];

/** Equipment id del Ring of Regeneration (kernel_ring_regen compara 0x2C). */
export const RING_OF_REGENERATION = 44;

/**
 * Barrido del Anillo de Regeneración — `kernel_ring_regen` 0x400C. Recorre los `count`
 * miembros del party (g_party_size) EN ORDEN y, por cada uno vivo con el Ring of
 * Regeneration equipado (ring==0x2C=44), tira `rand(0,7)`; con ==7 (1/8) invoca
 * `heal(i)` (el binario hace `counter_add_i16` +1 HP y marca g_unk_a9fa=1; es
 * SILENCIOSO). El gate es el BYTE DE ANILLO EQUIPADO `[charIdx*0x20+0x55C5]==0x2C`
 * (NO un "status secundario" — mislabel corregido en re/notes/kernel-sweep-4.md §3 y
 * camp-ambush-spec.md). Compartido por los TRES callers del binario, cada uno con su
 * cadencia y su destino de HP vía `heal`:
 *   · turno de MUNDO   0x2BCA (housekeeping)      → currentHp, 1×/turno
 *   · COMBATE          0x6794→0x67F6              → hp del combatiente, 1×/turno del
 *                                                    miembro-con-anillo que actúa
 *   · CAMP             0x0207 (bucle de 5 min)    → currentHp, 12×/hora (1 por paso)
 * El orden de tiradas (miembros 0..N) es el del asm → paridad de rand-stream si algún
 * miembro lleva el anillo (ninguno en la cadena del Grand Tour: 0 rands, sellos intactos).
 */
export function ringRegenSweep(
  members: readonly { ring: number; status: string }[],
  count: number,
  rand: RandFn,
  heal: (i: number) => void,
): void {
  const n = Math.min(count, members.length, 6);
  for (let i = 0; i < n; i++) {
    const m = members[i];
    if (!m || m.status === "D") continue; // 0x44 'D' → cuenta pero no actúa
    if (m.ring !== RING_OF_REGENERATION) continue; // [0x55C5]!=0x2C → salta
    if (rand(0, 7) === 7) heal(i); // ==7 (1/8) → +1 HP (counter_add_i16)
  }
}

/** Primera/última location de mazmorra (g_location 0x21..0x28). */
const FIRST_DUNGEON = 0x21;
const LAST_DUNGEON = 0x28;

/**
 * Minutos por acción del contexto actual: 2 en EXTERIOR (location 0), 1 en pueblo.
 *
 * NO tiene rama de mazmorra, y es DELIBERADO — no un pendiente. (La línea anterior decía
 * "mazmorra pendiente de Task 3.4: usa 1": el ticket está cerrado desde `dungeon.ts:4`, y
 * además era falso, porque aquí no hay rama que use 1.) Dentro de la mazmorra el clon deja
 * `position.location` en 0 (#123), así que quien llegase por el default de `advanceTurn`
 * (`movement.ts:145`) cobraría la rama de EXTERIOR — 2 minutos, el DOBLE de lo derivado
 * (DUNGEON 0x0FEF, `push 1` / `call advance_clock`). Ese era justo el bug que cerró #159.
 * La vía elegida fue pasar `MINUTES_PER_ACTION_DUNGEON` explícito en los cinco call-sites
 * de mazmorra, NO enchufar aquí `effectiveLocation`; razonado en `game.ts:3932`.
 */
export function minutesPerAction(location: number): number {
  return location === 0 ? MINUTES_PER_ACTION_OUTDOORS : MINUTES_PER_ACTION_TOWN;
}

function clamp0(n: number): number {
  return n > 0 ? n : 0;
}

/**
 * Re-sorteo de Shadowlords a medianoche — kernel_advance_clock 0x4FF5-0x504F.
 * En el rollover de día (g_hour 23→0) el binario reubica cada Shadowlord con
 * loc < 0x80 (en el mapa) a un pueblo nuevo `rand_range(1,8)` distinto de
 * g_location (la party) y de LOS TRES slots, con bucle de reintento SIN COTA.
 * Consume RNG del stream compartido. Se llama SÓLO desde el rollover de día de
 * advanceClock, por lo que dispara EXACTAMENTE una vez por cruce de medianoche
 * (kernel-survival.md:31-34: un único branch g_hour>23 por llamada).
 *
 * ★ #101-bis — «LOS TRES slots» incluye EL PROPIO. El bucle de comparación del
 * binario recorre `si = 0..2` (0x501c `sub si,si` … 0x502d `cmp si,3` / 0x5030
 * `jl 0x5020`) y la escritura del slot que se reubica es POSTERIOR (0x5044), así
 * que al comparar `locs[i]` todavía lleva su valor VIEJO: un candidato igual a la
 * ciudad donde el Shadowlord YA está se anula (0x502a `sub cx,cx`) y `0x5037
 * or di,di / 0x5039 je 0x5004` RE-TIRA. Ningún Shadowlord se queda nunca donde
 * estaba. Esto se escribía `locs[(i+1)%3]` y `locs[(i+2)%3]` —«los otros dos»—,
 * que aceptaba «quedarse» y por tanto consumía MENOS tiradas: medido sobre 60.000
 * medianoches, 31,5 % con distinto nº de tiradas y 32,3 % con distinto destino
 * (control: «se queda en su ciudad» = 12.170 en el clon vs 0 en el binario).
 * El bucle EXTERIOR sí está acotado a 3 (0x504b `cmp [bp-4],3`); lo ilimitado es
 * el re-sorteo de cada ranura.
 */
export function relocateShadowlordsAtMidnight(
  state: GameState,
  rand: RandFn,
  /** g_location tal como lo ve 0x5011 `cmp [0x5893],di`. Dentro de un arena vale 0xFF (A4-PARITY2 D-88). */
  partyLocation: number = state.position.location,
): void {
  const locs = state.shadowlordLocs;
  if (!locs) return;
  const party = partyLocation;
  for (let i = 0; i < 3; i++) {
    const cur = locs[i];
    if (cur === undefined || cur >= 0x80) continue; // 0x4ffd/0x5002: ausente/destruido
    let loc: number;
    do {
      loc = rand(1, 8); // 0x5004: rand_range(1,8)
      // 0x5016 g_location, y 0x5020 los TRES slots (si=0..2) incluido locs[i].
    } while (loc === party || locs[0] === loc || locs[1] === loc || locs[2] === loc);
    locs[i] = loc; // 0x5044
  }
}

/**
 * Contexto del REFRESCO DE LA BANDA CELESTE — ★ #176. Lo que `advanceClock` necesita
 * para evaluar las tres guardas de `0x514a-0x5161` y latchear las fases lunares.
 * Opcional: sin él el latch NO se refresca (los arneses puros de paridad lo omiten).
 * Lo produce `Game.skyRefreshCtx`.
 */
export interface SkyRefreshCtx {
  /** Tabla MOON_PHASES cruda de DATA.OVL (fo 0x1EEA, 56 bytes, cada byte +0x30). */
  moonPhasesRaw: number[];
  /**
   * `g_location` **EFECTIVO** (#123): 0x21..0x28 dentro de mazmorra. Pasar
   * `state.position.location` a secas rompería la guarda — en mazmorra vale 0 (la
   * superficie de la entrada) y dejaría pasar justo el caso que la guarda cierra.
   */
  location: number;
}

/**
 * ★ #176 — REFRESCO DEL LATCH DE FASES LUNARES: la cola de `advance_clock`,
 * `0x514a-0x5161`, verbatim del disasm:
 *
 * ```
 * 514a: a08058      mov al, byte ptr [g_prev_hour]
 * 514d: 38067f58    cmp byte ptr [g_hour], al
 * 5151: 7433        je 0x5186              ; (1) sólo si CAMBIÓ la hora
 * 5153: 803e935821  cmp byte ptr [g_location], 0x21
 * 5158: 730a        jae 0x5164             ; (2) mazmorra → NO refresca
 * 515a: 803e955880  cmp byte ptr [g_floor], 0x80
 * 515f: 7303        jae 0x5164             ; (3) bajo tierra → NO refresca
 * 5161: e820f9      call 0x4a84            ; draw_sky_strip: latchea las DOS fases
 * ```
 *
 * ⚠ Y ESTÁ DENTRO DE `advance_clock`, no en un consumidor aparte: la rutina que
 * empieza en `0x4f7c` (`push bp / mov bp,sp / sub sp,0xa / push di / push si`) no
 * tiene otra salida que `0x519c ret 2`, y en toda la banda `0x4f7c-0x519c` hay UN
 * solo `push bp` y UN solo `ret` (medido). El port la tenía PARTIDA en tres —
 * `advanceClock` (0x4f8d-0x509e), `lightLevel` (0x50a1-0x5145, puro y on-demand) y
 * el reloj de 12 h (0x5164-0x5183)—; el latch vuelve donde el binario lo pone.
 * Esto importa: `g_prev_hour` lo escribe la PROPIA rutina en `0x4fa3` justo antes
 * de avanzar, así que la comparación de `0x514d` es «¿cambió la hora en ESTA
 * llamada?». Fuera de aquí no se puede reproducir: el housekeeping consume el
 * flanco (`0x2b9c mov [g_prev_hour],al`, portado en `turnHousekeeping`).
 *
 * El escritor del latch es `draw_sky_strip` 0x4a84 (`0x4aeb` y `0x4b25`), las dos
 * ÚNICAS escrituras de las fases en todo el corpus (censo de #169). Indexa
 * `[bx+0x1ed8]` con `bx = g_day*2`; el array del port arranca en fo 0x1EEA ↔ DS
 * 0x1EDA = 0x1ED8+2, luego `0x1ed8 + day*2` ≡ `raw[(day-1)*2]` — la indexación de
 * `moonPhasesForDay`, comprobada aritméticamente.
 *
 * Devuelve `true` si latcheó (para los tests; el binario no devuelve nada).
 */
export function refreshMoonPhaseLatch(
  state: GameState,
  sky: SkyRefreshCtx,
  hourChanged: boolean,
): boolean {
  if (!hourChanged) return false; // 0x5151 je
  if (sky.location >= FIRST_DUNGEON) return false; // 0x5153/0x5158 cmp 0x21 / jae
  if (isBelowGround(state.position.floor)) return false; // 0x515a/0x515f cmp 0x80 / jae
  const i = (state.time.day - 1) * 2;
  // Bytes CRUDOS: el binario copia `[bx+0x1ed8]` tal cual (0x4aeb / 0x4b25); el
  // `-0x30` lo hace el consumidor. Ver `latchedMoonPhases` en world/moongates.ts.
  state.feluccaPhase = sky.moonPhasesRaw[i] ?? MOON_PHASE_OFFSET;
  state.trammelPhase = sky.moonPhasesRaw[i + 1] ?? MOON_PHASE_OFFSET;
  return true;
}

/**
 * kernel_advance_clock(n) — 0x4F7C. Avanza el reloj n MINUTOS con los
 * side effects exactos del original. n==0 en el original solo recalcula
 * la luz (aquí la luz es lightLevel(), on demand — no hay que llamar).
 *
 * `rand` opcional: presente = stream vivo (F.2); habilita el re-sorteo de
 * Shadowlords a medianoche (0x4FF5). Los callers puros lo omiten (no re-roll).
 *
 * `sky` opcional: contexto del REFRESCO DE FASES LUNARES de la cola de la misma
 * rutina (`refreshMoonPhaseLatch`, ★ #176). Sin él el latch no se toca.
 */
export function advanceClock(
  state: GameState,
  minutes: number,
  rand?: RandFn,
  sky?: SkyRefreshCtx,
  /** g_location para la exclusión del re-sorteo de medianoche (por defecto la posición viva; 0xFF en un arena). */
  partyLocation?: number,
): void {
  if (minutes === 0) {
    // ★ #184 — CALCO del cero. El binario NO retorna: `0x4f84 cmp word ptr [bp+4],0 /
    // 4f88 jne 0x4f8d` y, si es cero, `4f8a jmp 0x50a1` — que es la COLA de la propia
    // rutina (bloque de luz + refresco del latch #176), no el `ret`. Se salta el reloj
    // Y TAMBIÉN el snapshot de 0x4fa0-0x4fa3 (`g_prev_hour = g_hour`), así que el flanco
    // de la cola se compara contra el prevHour de la llamada ANTERIOR — por eso aquí no
    // se toca `state.prevHour`.
    //   INALCANZABLE hoy, medido y no declarado: los 14 call-sites de advanceClock en el
    //   repo pasan constantes positivas, y el único 0 que el port produce (resolveStep,
    //   rama «exterior bloqueado» MAINOUT 0xC30→jmp 0xD14) se consume como BOOLEANO en
    //   game.ts:1206 (`consumed: step.minutes > 0`), nunca como argumento del reloj. Se
    //   calca igualmente para que dejar de serlo no introduzca una divergencia en silencio.
    if (sky) refreshMoonPhaseLatch(state, sky, state.time.hour !== state.prevHour);
    return;
  }
  // NEGATIVOS: el `jne` del binario los manda por la vía NORMAL (sólo el cero salta), y
  // allí el rollover trabaja sobre bytes que se envuelven. El port NO modela byte-wrap,
  // así que calcarlo exigiría fabricar semántica; también es inalcanzable (mismo censo).
  // Se DECLARA y se corta, que es lo que se puede sostener con el binario delante.
  if (minutes < 0) return;
  // 'Q'uickness: mitad de coste, mínimo 1 (0x4F8D-0x4F9D).
  if (state.timeSpell === "Q") {
    minutes >>= 1;
    if (minutes === 0) minutes = 1;
  }
  // Snapshot de la hora para la detección de cambio (0x4FA0 → g_prev_hour).
  state.prevHour = state.time.hour;
  // 'T'ime-stop: el reloj no avanza (0x4FA6).
  if (state.timeSpell === "T") return;

  const t = state.time;
  t.minute += minutes;
  // Antorcha y hechizo de luz se consumen en MINUTOS (0x4FB4/0x4FBE).
  state.torchTurns = clamp0(state.torchTurns - minutes);
  if (state.lightSpellMins) state.lightSpellMins = clamp0(state.lightSpellMins - minutes);

  // Rollover exacto del original: UN solo if por nivel (0x4FC8-0x509A).
  if (t.minute > 59) {
    t.minute -= 60;
    t.hour++;
    if (t.hour > 23) {
      t.hour = 0;
      // 0x4FF5: re-sorteo de Shadowlords a medianoche. Sólo con el rand vivo
      // presente (los callers puros deterministas no lo pasan → sin re-roll).
      if (rand) relocateShadowlordsAtMidnight(state, rand, partyLocation);
      t.day++;
      if (t.day > 28) {
        t.day = 1;
        // ★ CEROS DE FIN DE MES (0x505c-0x506f) — antes OMITIDOS. El binario, en el mismo
        // bloque que pone el día a 1, hace `sub al,al` y escribe 0 en CINCO bytes:
        //   505e [0x585a] · 5061 [0x5859] · 5064 [0x5858] · 5067 [0x57b2] · 506f [0x5959]
        // Los TRES primeros son la MISMA tabla —un sello-de-día por PARCELA de reactivo—,
        // y el control que lo cierra es el bucle de su lectora: `search_daily_reagent_patch`
        // SJOG 0x045a recorre `si = 0..2` (`04fb cmp si,3 / jge`) indexando `[si+0x5858]`
        // ⇒ tres parcelas, tres bytes. (La nota vieja daba 0x5859/0x585A por «sin
        // identificar» siendo entradas 1 y 2 de la tabla que ya tenía identificada.)
        // Sin este borrado el sello se arrastra PARA SIEMPRE y ni el árbol de skull keys
        // ni las parcelas vuelven a dar nada al cambiar de mes.
        state.skullTreeFoundDay = 0; // 0x5067
        // Los sellos de las TRES parcelas de reactivo silvestre (#91, pieza 9b): el
        // binario escribe 0 en los tres bytes por separado, 0x585a · 0x5859 · 0x5858.
        // Sin esto, una parcela cosechada un día 1 no volvería a dar NUNCA el día 1 de
        // ningún mes posterior. Lectora: core/world/reagent-patches.ts (SJOG 0x045a).
        state.reagentPatchFoundDay = [0, 0, 0]; // 0x505e / 0x5061 / 0x5064
        // Cada mes, todos los personajes del roster envejecen su estancia
        // en posada: monthsAtInn++ con tope 25 (0x5072-0x5088).
        for (const ch of state.characters) {
          if (ch.monthsAtInn < 25) ch.monthsAtInn++;
        }
        t.month++;
        if (t.month > 13) {
          t.month = 1;
          t.year++;
        }
      }
    }
  }

  // ★ #176 — COLA DE LA MISMA RUTINA (0x514a-0x5161): refresco del latch de fases
  // lunares bajo sus tres guardas. Va DESPUÉS del rollover porque `draw_sky_strip`
  // indexa `g_day`, que el rollover acaba de incrementar (0x5051). El flanco se
  // compara aquí y no fuera porque `turnHousekeeping` lo CONSUME (0x2b9c).
  if (sky) refreshMoonPhaseLatch(state, sky, state.time.hour !== state.prevHour);
}

/**
 * kernel_apply_damage(idx, n) — 0x2A52: resta HP; a 0 → muerto ('D') y
 * deselecciona si era el personaje activo.
 */
export function applyDamage(state: GameState, idx: number, amount: number): void {
  const ch = state.characters[idx];
  if (!ch) return;
  ch.currentHp -= amount;
  if (ch.currentHp <= 0) {
    ch.currentHp = 0;
    ch.status = "D";
    if (state.activeCharacter === idx) state.activeCharacter = 0xff;
  }
}

/**
 * kernel_party_random_damage — 0x2AA8: rand(1,8) de daño a cada miembro
 * no muerto. La usan el hambre ("Starving!") y el cactus ("OUCH!").
 */
export function partyRandomDamage(state: GameState, rand: RandFn = defaultRand): void {
  for (let i = 0; i < state.partySize && i < 6; i++) {
    if (state.characters[i]?.status === "D") continue;
    applyDamage(state, i, rand(1, 8));
  }
}

/**
 * kernel_turn_housekeeping — 0x2AE8: efectos de fin de turno. Devuelve
 * los mensajes producidos (p.ej. "Starving!").
 */
export function turnHousekeeping(
  state: GameState,
  rand: RandFn = defaultRand,
  onPoisonTick?: (idx: number) => void,
): string[] {
  const messages: string[] = [];

  // Veneno y recuento de comensales (0x2B0B-0x2B57): 'D' y 'S' no comen
  // ni sufren; 'P' sufre 1 HP y SÍ come.
  // Divergencia (inalcanzable hoy): el asm además deselecciona en este
  // bucle a un miembro MUERTO que siguiera siendo el personaje activo
  // (0x2B14-0x2B25: status=='D' && g_active_char==i → 0xFF); el clon solo
  // lo hace al morir en applyDamage, único camino por el que un activo
  // puede pasar a 'D'.
  let eaters = 0;
  for (let i = 0; i < state.partySize && i < 6; i++) {
    const ch = state.characters[i];
    if (!ch) continue;
    const status = ch.status;
    if (status === "D" || status === "S") continue;
    if (status === "P") {
      applyDamage(state, i, 1);
      // ★ #213 — La PRESENTACIÓN del tick (0x2A52): el binario, ANTES de restar el HP,
      // invierte la fila del roster del miembro (0x2a59 `call 0x2a28`), emite el ruido
      // `noise_burst(10,1600,2000)` (0x2a68 `call 0x223c`) y la des-invierte (0x2a6e, el
      // mismo 0x2a28 = XOR). Con N envenenados son N secuencias COMPLETAS y
      // SECUENCIALES en ORDEN DE SLOT, porque este bucle (0x2b0b, `di` ascendente)
      // llama a 0x2a52 una vez por miembro y todo el altavoz del original es
      // mono-hilo bloqueante. El sonido NO toca `g_rng`: `0x223c` sortea su
      // frecuencia con un PRNG LOCAL en `[0x545c]` (sfx-catalog.md §1.2), así que
      // esta presentación es de STREAM CERO. El aplicador de daño del clon no puede
      // emitir por sí mismo (es núcleo puro), así que el llamador recoge los slots.
      onPoisonTick?.(i);
    }
    eaters++;
  }

  // Comidas y hambre, solo cuando la hora cambió en este turno (0x2B5D).
  if (state.time.hour !== state.prevHour) {
    if (state.food === 0) {
      messages.push("Starving!");
      partyRandomDamage(state, rand); // 0x2B74
    } else if (MEAL_HOURS.includes(state.time.hour)) {
      state.food = clamp0(state.food - eaters); // 0x2B8F counter_sub_i16
    }
    state.prevHour = state.time.hour;
  }

  // Contador de turnos (el original satura a 255; divergencia consciente).
  state.turnsSinceStart++;

  // Expiración del efecto Q/T (0x2BAE-0x2BC2). 0xFF = permanente.
  if (state.timeSpell && state.timeSpellTurns !== undefined) {
    if (state.timeSpellTurns > 0 && state.timeSpellTurns !== 0xff) {
      state.timeSpellTurns--;
      if (state.timeSpellTurns === 0) state.timeSpell = undefined;
    }
  }

  // Anillo de regeneración (kernel_ring_regen 0x400C, caller de MUNDO 0x2BCA): 1/8 +1 HP.
  ringRegenSweep(state.characters, state.partySize, rand, (i) => {
    const ch = state.characters[i]!;
    ch.currentHp = Math.min(ch.currentHp + 1, ch.maxHp);
  });

  return messages;
}

/**
 * Ignite torch — CMDS.OVL 0x0D98. Devuelve el mensaje a mostrar
 * ("None owned!" si no hay antorchas) o null si prendió.
 */
export function igniteTorch(
  state: GameState,
  rand: RandFn = defaultRand,
  /**
   * `g_location` EFECTIVO. Por defecto el del `position`, pero dentro de la mazmorra
   * el clon deja `position.location` en 0 (la mazmorra vive en `dungeonState`),
   * mientras el binario tiene ahí 0x21..0x28 (MAINOUT 0x088a-0x088c). Sin este
   * parámetro la rama de mazmorra era CÓDIGO MUERTO y la antorcha duraba 240 min
   * fijos en vez de `112 + rand(0,15)` — un rand de menos por (I)gnite (#123).
   */
  location: number = state.position.location,
): string | null {
  if (state.torches === 0) return "None owned!";
  state.torches--;
  const loc = location;
  if (loc >= FIRST_DUNGEON && loc <= LAST_DUNGEON) {
    state.torchTurns = Math.min(state.torchTurns + rand(0, 15) + TORCH_MINUTES_DUNGEON_BASE, 0xff);
  } else {
    state.torchTurns = TORCH_MINUTES;
  }
  return null;
}

/** Location 0x19 (25) siempre a oscuras (kernel 0x50B3). */
const DARK_LOCATION = 0x19;

/**
 * Nivel de luz (radio 2..50) — kernel 0x50A1-0x513B. Noche = 20:00-4:59;
 * amanecer/atardecer con rampa por decenas de minuto; mínimos por hechizo
 * de luz (18) y antorcha (10).
 */
/**
 * ¿La planta está BAJO TIERRA? = el `cmp byte ptr [g_floor],0x80 / jae` del binario.
 *
 * ★ Existe porque el literal `floor >= 0x80` es un NO-OP en el port para media
 * población (#171, residuo de #150): el binario lee `g_floor` como BYTE SIN SIGNO y
 * ahí el sótano es 0xFF, pero en el clon `position.floor` es un número CON SIGNO y
 * los sótanos de pueblo/castillo son **z = −1** (`smallmaps.json`); sólo el Underworld
 * se guarda como 0xFF. Sin enmascarar a byte, `-1 >= 0x80` es `false` y el gate deja
 * pasar exactamente el caso que venía a cerrar.
 *
 * Cubre por tanto los DOS: Underworld (0xFF) y sótanos (−1 → 0xFF al enmascarar).
 * `lightLevel` (abajo) ya hacía esta cuenta a mano desde antes; esto la unifica para
 * que no haya que redescubrirla en cada consumidor.
 */
export function isBelowGround(floor: number): boolean {
  return (floor & 0xff) >= 0x80;
}

export function lightLevel(state: GameState): number {
  const { hour, minute } = state.time;
  const pos = state.position;
  // El original compara g_floor como BYTE sin signo (`cmp byte [g_floor],0x7f;
  // ja`, 0x50BA): floor >= 0x80 → oscuro. Eso cubre el underworld (0xFF) Y los
  // SÓTANOS de pueblos/castillos (planta z=-1, que como byte es 0xFF) — que en
  // el original están a oscuras (necesitas antorcha). En el port `floor` es un
  // número con signo (-1 para el sótano), así que enmascaramos a byte.
  const floorByte = pos.floor & 0xff;
  let light: number;
  if (pos.location === DARK_LOCATION || floorByte > 0x7f || hour < 5 || hour > 19) {
    light = 2;
  } else if (hour === 5) {
    light = SUNRISE_LIGHT_RAMP[Math.floor(minute / 10)]!;
  } else if (hour === 19) {
    light = SUNRISE_LIGHT_RAMP[Math.floor((59 - minute) / 10)]!;
  } else {
    light = 0x32;
  }
  if (state.lightSpellMins && light < 0x12) light = 0x12;
  if (state.torchTurns > 0 && light < 0x0a) light = 0x0a;
  return light;
}
