/**
 * Lord British: subida de nivel (aparición del campamento) y endgame.
 *
 * El level-up de U5 NO ocurre al HABLAR con Lord British: el binario lo computa AL
 * ACAMPAR, en la rutina `outsubs_camp_results` (OUTSUBS 0x0658), cuya secuencia se
 * abre con "An apparition!" (DS 0x7750) — la aparición del espectro de Lord British
 * que recompensa las gestas de la party. La escritura del byte de nivel (roster+0x16,
 * DS 0x55be @0x070e) y del maxHP (roster+0x12, DS 0x55ba @0x0717) son ÚNICAS en TODO
 * el juego: ningún handler de TALK/CASTLE sube niveles (barrido exhaustivo del
 * desensamblado). Por eso `campApparition` se dispara desde `Game.camp()`, no desde el
 * diálogo de LB (trigger de LB retirado como corrección de fabricación, F1-postfase),
 * y SÓLO cuando la acampada cruza el gate del 25 % (`re/notes/oracle-camp-event.md`).
 *
 * El endgame es rescatar a Lord British de su celda en Doom, lo que exige haber
 * destruido a los tres Shadowlords y portar sus tres regalías (amuleto, corona,
 * cetro). Con eso, y estando dentro de Doom, Britannia queda liberada.
 */
import type { GameState } from "../state.js";
import type { RandFn } from "../world/survival.js";
import { addByteCapped } from "../counters.js";
import { tf } from "../../i18n/index.js";
import { canReachDoom } from "./shadowlords.js";
import { questScroll } from "./endgame.js";
import { dsRec } from "../data/ds-strings.js";

/** Nivel máximo alcanzable (U5). El binario no lo clampa explícitamente: exp
 * cabe en 9999 → floor(9999/100)=99 → nivel 8 por la fórmula. Se clampa por
 * seguridad ante estados de exp fuera de rango. */
export const MAX_LEVEL = 8;

/** Multiplicador de HP máximo por nivel: maxHP = 30·nivel — OUTSUBS 0x0712 `mov ax,0x1e` y
 *  OUTSUBS 0x0715 `imul dx`. El 0x1e llega por `ax`: no hay `imul` de inmediato ahí (#243). */
export const HP_PER_LEVEL = 30;

/**
 * Nivel derivado de la experiencia — REGLA EXACTA de OUTSUBS `outsubs_camp_results`
 * @0x0658 (kernel-survival.md §5.5). El binario NO usa un umbral lineal: computa
 *   cx = floor(exp / 100)            (0x08e7 `mov ax,[exp]`; 0x08ec `mov cx,100`; idiv)
 *   nivel = 1; while (cx > 0) { nivel++; cx >>= 1; }   (0x06ea `inc dx; sar cx,1; jg`)
 * es decir **nivel = bit_length(floor(exp/100)) + 1**. Umbrales resultantes (exp):
 *   L2≥100 · L3≥200 · L4≥400 · L5≥800 · L6≥1600 · L7≥3200 · L8≥6400 (potencias de 2,
 *   NO 100·nivel). exp máx 9999 ⇒ nivel 8. Verificado byte a byte en OUTSUBS.OVL.asm.
 */
export function levelForExp(exp: number): number {
  let cx = Math.floor(Math.max(0, exp) / 100);
  let level = 1; // dx inicial (OUTSUBS 0x08db `mov [bp-0xc],1`)
  while (cx > 0) {
    level += 1;
    cx >>= 1;
  }
  return Math.min(level, MAX_LEVEL);
}

/** Un PASO por-miembro de la escena de la aparición (bucle OUTSUBS 0x07fb-0x08f9):
 *  el miembro `charIdx` DESPIERTA (su sprite pasa de tumbado 0x11e al tile de pie de
 *  su clase, 0x0868-0x0874 vía tabla 0x1ade) con campanilla+flash+acorde, y si subió
 *  de nivel lleva su arenga (`message`). Orden = roster, saltando muertos (0x080f). */
export interface CampWakeStep {
  charIdx: number;
  message?: string;
}

export interface LevelUpResult {
  /** Arengas de level-up en orden (compat; == steps[].message no-vacíos). */
  messages: string[];
  /** Pasos por-miembro VIVO de la escena, en orden de roster (0x07fb). */
  steps: CampWakeStep[];
}

/** Atributo que sube al subir de nivel, por resultado de `rand(1,3)` (OUTSUBS 0x0752):
 *  1 → STR "stronger!" (0x76e, roster+0x0C) · 2 → DEX "quicker!" (0x7be, roster+0x0D) ·
 *  3 → INT "wiser!" (0x7d2, roster+0x0E). Strings byte-exactos de DATA.OVL
 *  (fileoff 0x77d0/0x77da/0x77e4 = DS 0x77c0/0x77ca/0x77d4 + 0x10). */
const LEVELUP_STAT_WORDS = ["stronger!", "quicker!", "wiser!"] as const;

/** Tope del boost de atributo por acampada (OUTSUBS 0x0784 `mov ax,0x1e` → add_capped
 *  con max 30): sube +1 pero nunca por encima de 30. */
const LEVELUP_STAT_CAP = 30;

/**
 * La APARICIÓN del campamento — `outsubs_camp_results` (OUTSUBS 0x0658). Es el cuerpo
 * completo del handler `0xbfd6` que el gate del 25 % invoca (CMDS 0x0502 → kernel
 * 0x7F56 → OUTSUBS 0x0658; ver `re/notes/oracle-camp-event.md`). NO es incondicional:
 * corre SÓLO cuando la acampada cruza `rand(0,99)<25` (lo cablea `Game.camp()`).
 *
 * Recorre los primeros `partySize` miembros del roster (0x07fb `cmp [bp-6],
 * g_party_size`), en orden, saltando a los muertos (status 'D'/0x44, 0x080f `cmp
 * [bx+0x55b3],0x44; je`). Por cada miembro VIVO — **incluido el que monta guardia**,
 * el loop de la aparición NO respeta el watch, a diferencia del heal parcial de
 * `campHoleUp` — y ANTES del chequeo de nivel:
 *  - **cura total** `currentHP := maxHP` (0x0820 `mov ax,[bx+0x55ba]` → 0x0824 `mov
 *    [bx+0x55b8],ax`).
 *  - **`status := 'G'`** (0x0828 `mov byte [bx+0x55b3],0x47`): cura veneno/dormido de
 *    TODO miembro vivo (no sólo de los que suben).
 * Después, el level-up condicional:
 *  - nivel := `levelForExp(exp)` desde roster+0x14 (exp word, 0x08e7); si **DIFIERE**
 *    del actual (0x0704 `cmp ax,dx; jne` — si igual, ni mensaje ni cambio) → lo fija
 *    (0x070e) y pone **maxHP = currentHP = 30·nivel** (0x0717/0x071b: SET, sobrescribe
 *    la cura total de arriba con el tope del nuevo nivel).
 *  - sube **+1 un atributo aleatorio** (`rand(1,3)` @0x0752 → STR/DEX/INT, tope 30
 *    @0x0784) y emite la arenga de la aparición.
 *
 * Consumo de RNG: **1 `rand(1,3)` por miembro que SÍ sube de nivel** (0 si nadie sube).
 * La cura total y `status='G'` son deterministas (0 rand). El orden es exacto **DENTRO
 * del helper 0x0400 aislado**: heal-parcial+MP de `campHoleUp` → gate `rand(0,99)` →
 * esta rutina (rand(1,3) por miembro que sube) — CMDS 0x046e… → 0x04f4 → 0x0502 →
 * OUTSUBS 0x0752. ⚠️ NO es paridad del stream de `camp()` completo: el bucle de sueño
 * previo (CMDS 0x01ee-0x030c) tira `rand(0,63)` por hora cruzada que el port no modela
 * (Clase C, ver `Game.camp` y `port-t7-review.md` ⚖️1).
 *
 * Texto byte-exacto (DATA.OVL, fileoff = DS + 0x10): "An apparition!" abre la escena
 * (0x0660, la imprime `Game.camp()`); por level-up se compone
 * `\n"Hail, `(0x776a)` + nombre + `!\nFor thy valiant deeds, I shall reward thee!\n`
 * (0x7774)` + `Thou art now level `(0x77a4)` + N + `, and\n`(0x77b8)` + palabra +
 * `" `(0x77dc)` + `\n`.
 *
 * Tras el level-up (o directamente para los MUERTOS, 0x0814 `je 0x79c`), **recálculo de
 * MP por clase** (0x079c-0x07b6): 'A'/'M' → `MP := INT` (0x07e6-0x07f1, roster+0x0e →
 * +0x0f); 'B' → `MP := INT>>1` (0x08fc-0x090b `shr ax,1`); el resto queda intacto
 * (0x07bb `jmp 0x7f5`). Corre para TODO miembro del party, incluidos guardia y muertos.
 *
 * ⚠ NO modelado (Clase C, ver `deliberate-divergences.md`): (1) los beeps `0x3ae6(1)`
 * entre pasos (0x087b/0x08d1 ×3 — timbre/cadencia, la piel pacea con sus pulsos); (2) las
 * pausas por tecla `getkey_with_redraw 0x266c` tras cada arenga (0x0792) y tras el
 * discurso de karma (0x0961) — el port pacea a reloj de pared; (3) el `advance_clock(0)`
 * de teardown (0x0993); (4) el flag opaco `[bp+8]&0x82` (0x04e7) que salta el gate
 * entero — residual del `campHoleUp`/gate. El discurso de karma (0x090e-0x099b) SÍ está
 * modelado ahora: lo compone `Game.campWake` (`campKarmaMessage`), ver
 * `re/notes/camp-apparition-scene.md`.
 */
export function campApparition(state: GameState, rand: RandFn): LevelUpResult {
  const messages: string[] = [];
  const steps: CampWakeStep[] = [];
  const members = state.characters.slice(0, state.partySize);
  for (const [charIdx, member] of members.entries()) {
    if (member.status === "D") {
      // 0x080f/0x0814: muerto → salta cura/despertar/level-up, pero SÍ pasa por el
      // recálculo de MP (je 0x79c) y el redraw del panel. Sin paso de escena.
      recomputeApparitionMp(member);
      continue;
    }
    member.currentHp = member.maxHp; // 0x0820/0x0824: cura total (para TODO miembro vivo)
    member.status = "G"; // 0x0828: status='G' cura veneno/dormido
    let message: string | undefined;
    const newLevel = levelForExp(member.exp);
    if (newLevel !== member.level) {
      // 0x0704 `cmp ax,dx; jne` — si igual, ni mensaje ni rand (salta a MP 0x79c).
      member.level = newLevel; // 0x070e
      member.maxHp = HP_PER_LEVEL * newLevel; // 0x0717: maxHP = 30·nivel (SET)
      member.currentHp = member.maxHp; // 0x071b: currentHP = 30·nivel (tope del nuevo nivel)
      const roll = rand(1, 3); // 0x0752
      const word = LEVELUP_STAT_WORDS[roll - 1]!;
      if (roll === 1) member.strength = addByteCapped(member.strength, 1, LEVELUP_STAT_CAP);
      else if (roll === 2) member.dexterity = addByteCapped(member.dexterity, 1, LEVELUP_STAT_CAP);
      else member.intelligence = addByteCapped(member.intelligence, 1, LEVELUP_STAT_CAP);
      // tf() y no template-literal: el compuesto entero llegaba al choke t() ya interpolado
      // y salía en INGLÉS bajo 'es' (fuga cazada por el campProbe del soak, carril
      // sell-chatter). La plantilla {} ya estaba traducida en es.json; en 'en' tf es
      // composición idéntica byte a byte. `word` pasa por t() dentro de tf (string arg).
      message = tf('\n"Hail, {}!\nFor thy valiant deeds, I shall reward thee!\nThou art now level {}, and\n{}" \n', member.name, newLevel, word);
      messages.push(message);
    }
    recomputeApparitionMp(member); // 0x079c (cae aquí tanto con como sin level-up)
    steps.push({ charIdx, message });
  }
  return { messages, steps };
}

/**
 * Recálculo de MP del epílogo por-miembro de la aparición — OUTSUBS 0x079c-0x07b6:
 * lee la letra de clase (roster+0x0a, 0x07a3) y fija el MP actual (roster+0x0f):
 * 'A' (0x07a9) y 'M' (0x07b6) → `MP := INT` (0x07e6: copia +0x0e → +0x0f);
 * 'B' (0x07ae) → `MP := INT >> 1` (0x08fc-0x0909 `shr ax,1`);
 * cualquier otra letra → intacto (0x07bb `jmp 0x7f5`, sin escritura).
 */
function recomputeApparitionMp(member: { class: string; intelligence: number; currentMp: number }): void {
  if (member.class === "A" || member.class === "M") member.currentMp = member.intelligence;
  else if (member.class === "B") member.currentMp = member.intelligence >> 1;
}

/**
 * ¿Están dadas las condiciones para el rescate final? Los tres Shadowlords
 * destruidos y las tres regalías de Lord British en poder de la party.
 */
export function endgameReady(state: GameState): boolean {
  const a = state.lbArtifacts;
  return canReachDoom(state) && a.amulet && a.crown && a.sceptre;
}

export interface RescueResult {
  ok: boolean;
  /**
   * Tipo de desenlace (ENDGAME_main 0x08c2, fork `g_wooden_box`):
   * - `victory`: con la Sandalwood Box → LB abre la caja y el Orb of the Moons abre
   *   la puerta a casa (rama buena + pergamino de cierre).
   * - `stranded`: sin la caja → LB queda varado contigo ("pull up a chair"), sin salida
   *   ni pergamino (el final alternativo/gag del original).
   * - `incomplete`: aún no se dan las condiciones del rescate.
   */
  ending: "victory" | "stranded" | "incomplete";
  messages: string[];
}

/**
 * Rescata a Lord British de su celda en Doom. Al alcanzarlo marca
 * `questFlags["game-won"]` (el finale se dispara una sola vez) y BIFURCA por la
 * Sandalwood Box (ENDGAME_main 0x08c2): con caja = victoria + Orb + pergamino;
 * sin caja = final alternativo "varado" (sin pergamino).
 *
 * #179 — `viaAbsorption`: la vía FIEL (absorción total en la celda de LB, cm127)
 * NO comprueba regalías ni `in-doom`: la cadena del binario (absorb → centinela
 * 0x4d → overlay 13) no tiene NINGÚN gate de inventario — llegar a la celda ya
 * exigió todo lo demás. Los gates de abajo pertenecen a la vieja vía sintética
 * (`checkDoomRescue` por planta) y se conservan para los llamadores directos
 * (tests/debug) que ejercen el precondicionado.
 */
export function rescueLordBritish(
  state: GameState,
  opts?: { viaAbsorption?: boolean },
): RescueResult {
  if (!opts?.viaAbsorption) {
    if (!endgameReady(state)) {
      // `const messages` (no literal en la propiedad) para que la guarda anti-fab lo VEA.
      const messages = ["The way to Lord British remains barred. Thy quest is not yet complete."];
      return { ok: false, ending: "incomplete", messages };
    }
    if (!state.questFlags["in-doom"]) {
      const messages = ["Lord British languishes in the depths of Doom. Thou must seek him there."];
      return { ok: false, ending: "incomplete", messages };
    }
  }
  state.questFlags["game-won"] = true;
  // Alpha 4 A4-UI4 (D-73): la vía FIEL no imprime nada aquí. `absorb` arma el
  // centinela y el teardown salta a ENDGAME.OVL (SJOG 0x2046 / DUNGEON 0x00cb), que
  // imprime ENDMSG.DAT en SU orden desde la escena (el guión, buildEndgameScript);
  // el conector y las copias de ENDMSG / del pergamino en consola de abajo eran
  // narración del port. Se conservan sólo para los llamadores directos (tests/debug).
  if (opts?.viaAbsorption) {
    return { ok: true, ending: state.specialItems.woodenBox ? "victory" : "stranded", messages: [] };
  }
  // Conector del rescate mecánico (narración del port; ENDGAME 0x0648 no lo enuncia).
  const messages = [
    "Bearing amulet, crown and sceptre, thou dost shatter the final seal of Doom.",
    "Lord British rises, unbroken, and takes again the throne of Britannia.",
  ];
  if (state.specialItems.woodenBox) {
    // Rama BUENA — diálogo fiel de ENDMSG.DAT (records 3 y 9) + el pergamino de cierre.
    messages.push("Lord British carefully opens the box...");
    messages.push(
      dsRec("ENDMSG.DAT", 9).replace(/\n+$/, ""),
    );
    messages.push(...questScroll(state));
    return { ok: true, ending: "victory", messages };
  }
  // Rama ALTERNATIVA — sin la caja no hay salida (ENDMSG.DAT record 10). Sin pergamino.
  messages.push('"Well then, pull up a chair."');
  messages.push('"We shall be here a while."');
  return { ok: true, ending: "stranded", messages };
}
