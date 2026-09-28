/**
 * Mezcla de hechizos (comando "Mix" del original). Verifica y consume `qty` de
 * cada reagente requerido por el hechizo e incrementa su cantidad mezclada.
 *
 * Convención (igual que party.ts/shops.ts): muta `GameState` in situ y devuelve
 * `{ ok, message }`; ante fallo NO muta nada.
 */
import type { GameState } from "../state.js";
import { MAX_TRACKED_SPELL_INDEX, type SpellDef } from "./spells.js";
import { reagentMask } from "./mixReagentPicker.js";
import { chestTrap, type TrapResult } from "../world/commands.js";
import { firstConsciousIndex } from "../party.js";
import type { RandFn } from "../world/survival.js";

/**
 * Mezcla `qty` copias de `def`. Requiere `qty` de cada reagente de `def.reagents`.
 * Éxito → "Done!" (DS 0x8ffc); el caller imprime antes "Mixing..." (DS 0x8ff0). El
 * fallo por reagentes insuficientes devuelve "Insufficient reagents!" (DS 0x8f7e),
 * el string REAL del comando Mix (CMDS.OVL 0x1a70 @0x1aa7). ⚠ Ojo: "None mixed!"
 * (DS 0x463a) NO es un string de Mix — es el gate de (C)ast "no has mezclado este
 * hechizo" (cast.ts:277); antes se usaba aquí por misatribución (ya corregida). Las
 * guardas qty<=0 / índice no-mezclable son defensivas: la UI (askMixQuantity) ya las
 * pre-filtra, así que su mensaje no llega a mostrarse.
 */
export function mixSpell(state: GameState, def: SpellDef, qty: number): { ok: boolean; message: string } {
  if (qty <= 0) return { ok: false, message: "Insufficient reagents!" };
  if (def.index < 0 || def.index > MAX_TRACKED_SPELL_INDEX) {
    // Nox (índice 48) no tiene slot en spellQuantities: no es mezclable.
    return { ok: false, message: "Insufficient reagents!" };
  }

  // Comprobar que hay reagentes suficientes ANTES de consumir nada.
  for (const r of def.reagents) {
    const have = state.reagentQuantities[r] ?? 0;
    if (have < qty) return { ok: false, message: "Insufficient reagents!" };
  }

  // Consumir reagentes e incrementar el hechizo mezclado. La cuenta mezclada satura
  // en 99 (CMDS.OVL 0x1be7 `cmp byte [bx+0x57f0], 0x63` → `ja` fija 0x63): se gastan
  // los `qty` reagentes completos aunque la carga del hechizo llegue al tope.
  for (const r of def.reagents) {
    state.reagentQuantities[r] = (state.reagentQuantities[r] ?? 0) - qty;
  }
  state.spellQuantities[def.index] = Math.min(99, (state.spellQuantities[def.index] ?? 0) + qty);

  return { ok: true, message: "Done!" };
}

/**
 * Mezcla con los reagentes SELECCIONADOS a mano en el selector (CMDS.OVL 0x18be →
 * consumo 0x1baa + comprobación 0x1bc2). Calca la mecánica EXACTA del binario:
 *
 *   1. Consume `qty` de CADA reagente marcado (0x1baa `sub byte[si+0x5850], al`),
 *      con independencia de si la mezcla acierta — los reagentes se GASTAN igual.
 *   2. Si la máscara marcada == la máscara REQUERIDA del hechizo (tabla DS 0x1cc0 =
 *      OR de `0x80>>r` de `def.reagents`) y el índice es mezclable → suma `qty` a la
 *      carga (cap 99, 0x1be7) y devuelve `correct:true` ("Done!" lo imprime el caller).
 *   3. Si NO casa (reagentes incorrectos) → los reagentes ya se gastaron, no hay carga
 *      **y salta la TRAMPA DEL COFRE** (#105). La rama 0x1bf6 no se queda ahí:
 *      `putchar('\n')` (0x1bf6) → `party_conscious_state` (0x1bfd → kernel 0x39fc,
 *      deja el índice en g_cmb_scratch_x) → `chest_trap_trigger` (0x1c04 `call 0x7050`
 *      = kernel 0x2fd0), el mismo despachador que ya clona `world/commands.ts::chestTrap`.
 *      Mezclar mal EXPLOTA. Derivación: re/notes/mix-trap-105-acta.md §1 y §3.
 *
 * ⚠ **POR ESO ESTA FUNCIÓN EXIGE `rand`**: la rama mala consume tiradas (de 1 a 7 según
 * el tipo, §4 del acta), así que Mix **sí toca el RNG** — lo contrario de lo que
 * declaraban `cmds.md §12`, las dos citas del ledger y dos comentarios de `main.ts`,
 * todos corregidos. `rand` es OBLIGATORIO a propósito: hacerlo opcional dejaría que un
 * caller que lo olvide recayera EN SILENCIO en la mecánica vieja, que es justo el
 * defecto que esto cierra.
 *
 * ⚠ **DESVIACIÓN DECLARADA (§1.1 del acta):** si NADIE está consciente, kernel 0x39fc
 * no escribe `g_cmb_scratch_x` y `cmd_mix` —que ignora el retorno— empuja el valor
 * RANCIO de un uso anterior. Esa global no se modela, así que aquí se usa el slot 0.
 * El alcance de la desviación está ACOTADO Y MEDIDO: el índice del que abre **no
 * cambia ni una tirada** (ACID tira 1 vez sea quien sea; BOMB tira por miembro vivo;
 * POISON/GAS no tiran), así que sólo puede cambiar QUIÉN come el daño, nunca el stream.
 *
 * El caller pre-valida cantidad y suficiencia (askMixQuantity, 0x1a70), y trata la
 * selección VACÍA con "Nothing to mix!" ANTES de llamar aquí (0x1b78). `selected` son
 * índices de reagente 0..7.
 */
export interface MixTrap {
  /** Miembro que la come: kernel 0x39fc (primer 'G'/'P'); 0 si no hay consciente. */
  opener: number;
  /** Tipo, mensaje y slots de daño (flash+blip 0x2a52) — el caller los presenta (ver `chestTrap`). */
  result: TrapResult;
}

/**
 * Veredicto de la respuesta a "How much? " (CMDS.OVL 0x1a70 + el tramo 0x1b6b-0x1b78
 * de `cmd_mix`), en el orden del binario — A3-HF10, re/notes/mix-hf10-command-parity.md §3:
 *
 *   · `n === 0` → "abort": 0x1a8e sale sin comprobar nada y 0x1b71 (`jg`) aborta en silencio.
 *   · algún reagente MARCADO con cuenta < n → "insufficient" ("Insufficient reagents!" y
 *     RE-PREGUNTA, 0x1ac6). La comparación es SIN SIGNO de 16 bits (0x1aa5 `jae`): un `n`
 *     negativo ("-5", el getnum acepta signo) es 0xfffb y siempre falta si hay algo marcado.
 *     Los reagentes NO marcados no se leen.
 *   · `n < 0` (sólo posible con la máscara vacía) → "abort" (0x1b71).
 *   · máscara vacía → "nothing" ("Nothing to mix!", 0x1b78).
 *   · si no → "mix".
 */
export type MixQuantityVerdict = "abort" | "insufficient" | "nothing" | "mix";

export function mixQuantityVerdict(
  n: number,
  selected: readonly number[],
  qtyOf: (reagentId: number) => number,
): MixQuantityVerdict {
  if (n === 0) return "abort";
  const wanted = n & 0xffff;
  if (selected.some((r) => Math.max(0, qtyOf(r)) < wanted)) return "insufficient";
  if (n < 0) return "abort";
  if (selected.length === 0) return "nothing";
  return "mix";
}

export function mixSelected(
  state: GameState,
  def: SpellDef,
  selected: readonly number[],
  qty: number,
  rand: RandFn,
): { correct: boolean; trap: MixTrap | null } {
  // 1. Consumo de los reagentes marcados (gasto incondicional).
  for (const r of selected) {
    state.reagentQuantities[r] = (state.reagentQuantities[r] ?? 0) - qty;
  }
  // 2. Corrección: máscara marcada == máscara requerida (y hechizo mezclable).
  const mixable = def.index >= 0 && def.index <= MAX_TRACKED_SPELL_INDEX;
  const correct = mixable && reagentMask(selected) === reagentMask(def.reagents);
  if (correct) {
    state.spellQuantities[def.index] = Math.min(99, (state.spellQuantities[def.index] ?? 0) + qty);
    return { correct: true, trap: null }; // 0x1bd6: la rama buena NO tira
  }
  // 3. Máscara equivocada (0x1bd1 `jne 0x1bf6`) → la trampa, con el stream vivo.
  const found = firstConsciousIndex(state); // 0x1bfd → kernel 0x39fc
  const opener = found >= 0 ? found : 0; // desviación declarada arriba
  const result = chestTrap(state.position.location, opener, state.characters, rand, state.partySize); // 0x1c04
  return { correct: false, trap: { opener, result } };
}
