/**
 * Selector de reagentes del comando (M)ix — MODELO PURO (sin DOM, sin estado del juego).
 *
 * Calca el bucle interactivo `mix_reagent_select` (CMDS.OVL @0x18be) que corre TRAS
 * teclear el nombre del hechizo y ANTES de la cantidad: pinta la lista "Reagents:"
 * (DS 0x8f64) con los reagentes POSEÍDOS (cuenta > 0), un cursor que se mueve con las
 * flechas, y RETURN/Space que MARCAN/DESMARCAN el reagente bajo el cursor; 'M' cierra
 * y mezcla, ESC cancela. Devuelve una MÁSCARA de reagentes marcados a mano (bit
 * `0x80 >> reagentId`, igual que `di` en el binario, @0x19f1). La mezcla con la máscara
 * INCORRECTA gasta igualmente los reagentes marcados (la corrección la comprueba el
 * caller contra la máscara requerida del hechizo, tabla DS 0x1cc0).
 *
 * Reparto (igual que `readyPicker.ts`):
 *   · Este módulo: qué reagentes se listan, la navegación del cursor y el toggle.
 *   · `mix.ts` (`mixSelected`): la MECÁNICA (consumo de los marcados + carga si la
 *     máscara casa con la requerida). Aquí NO se re-deriva.
 *   · `main.ts`: aplica y republica la vista; la piel fiel reutiliza el overlay de
 *     lista de Ready (`ReadyPickerView`, marca = glifo en la separación).
 *
 * Cita: CMDS.OVL 0x18be-0x1a6f; derivación en re/notes/mix-hf10-command-parity.md §2
 * (A3-HF10: `re/notes/mix-flow-acta.md`, la cita anterior, no existe en el árbol).
 */
import { MIX_ARROW_GLYPHS, MIX_UI } from "../world/cmd-strings.js";

/** Nº de reagentes (SulfurAsh..MandrakeRoot); la lista sólo muestra los poseídos. */
const REAGENT_COUNT = 8;

/**
 * PIE DE INSTRUCCIONES del selector de reagentes, en filas de consola.
 *
 * El binario lo imprime en `cmd_mix` @0x1b1e-0x1b5a, JUSTO ANTES de `call 0x18be`
 * (@0x1b5d) — o sea antes de abrir el panel "Reagents:" — con esta secuencia exacta:
 *
 *   1b1e  putchar(0x0a)                      ; LF: baja a la fila siguiente
 *   1b25  putchar(0x1b) `←`   1b2c putchar(0x2c) `,`
 *   1b33  putchar(0x1a) `→`   1b3a putchar(0x2c) `,`
 *   1b41  putchar(0x18) `↑`   1b48 putchar(0x2c) `,`
 *   1b4f  putchar(0x19) `↓`
 *   1b56  print_string(0x8fc6) = " to move,\nRETURN selects.\nType M to mix:"
 *
 * ⇒ el texto compuesto es "\n←,→,↑,↓ to move,\nRETURN selects.\nType M to mix:",
 * que son TRES filas. El `\n` inicial NO abre una fila en blanco: el cursor venía
 * al final de la fila del getstring (":VAS LOR" — `CAST2 0x00de` no imprime `\n`
 * al terminar), así que ese LF es exactamente el salto a la fila siguiente, que en
 * el modelo de consola del port ya lo hace `pushConsole` al empujar una línea nueva.
 *
 * El texto pasa por `t()` en el call-site (cadenas FIJAS); los glifos de flecha NO
 * (`MIX_ARROW_GLYPHS`, son códigos CP437, no texto).
 */
export function mixPickerFooterLines(): string[] {
  return [MIX_ARROW_GLYPHS + MIX_UI.toMove, MIX_UI.returnSelects, MIX_UI.typeMToMix];
}

/** Bit de máscara de un reagente (`0x80 >> id`; @0x19f1 `mov ax,0x80; sar ax,cl`). */
export function reagentBit(reagentId: number): number {
  return 0x80 >> reagentId;
}

/** Máscara OR de un conjunto de reagentes (usada para requerido y seleccionado). */
export function reagentMask(reagentIds: readonly number[]): number {
  return reagentIds.reduce((m, r) => m | reagentBit(r), 0);
}

/** Una fila del selector: un reagente POSEÍDO (cuenta > 0). */
export interface MixReagentRow {
  /** Índice del reagente 0..7 (SulfurAsh..MandrakeRoot). */
  reagentId: number;
  /** Nombre abreviado de la lista (data.json `reagents`, = DATA.OVL 0x19d2). */
  name: string;
  /** Cuenta poseída (se pinta a 2 dígitos, @0x194c). */
  qty: number;
}

/** Dependencias del constructor de filas (main.ts las cablea al estado del juego). */
export interface MixReagentRowDeps {
  /** `reagentQuantities[id]` — unidades poseídas. */
  qtyOf(reagentId: number): number;
  /** Nombre abreviado del reagente. */
  nameOf(reagentId: number): string;
}

/**
 * Filas del selector: los reagentes 0..7 con cuenta > 0, en orden ascendente
 * (@0x18ca-0x18dd construye el array de índices poseídos). Lista vacía sólo si no hay
 * reagentes — pero el caller ya cortó con "No reagents owned!" antes de llegar aquí.
 */
export function buildMixReagentRows(deps: MixReagentRowDeps): MixReagentRow[] {
  const rows: MixReagentRow[] = [];
  for (let id = 0; id < REAGENT_COUNT; id++) {
    const qty = deps.qtyOf(id);
    if (qty > 0) rows.push({ reagentId: id, name: deps.nameOf(id), qty });
  }
  return rows;
}

/** Posición + selección del selector: fila bajo el cursor + máscara de marcados. */
export interface MixReagentPickerModel {
  /** Índice ABSOLUTO en `rows` del reagente bajo el cursor. */
  cursor: number;
  /** Máscara de reagentes MARCADOS (bit `0x80 >> reagentId`). */
  selected: number;
}

/** Acción que el reductor devuelve al caller (main.ts la ejecuta). */
export type MixReagentAction =
  /** La tecla no cambió nada (el modal la traga; getkey re-lee). */
  | { kind: "none" }
  /** El cursor se movió → repinta. */
  | { kind: "move"; model: MixReagentPickerModel }
  /** RETURN/Space sobre `index`: marca/desmarca (el selector sigue abierto). */
  | { kind: "toggle"; model: MixReagentPickerModel }
  /** 'M' → cerrar y mezclar con la máscara actual (@0x19e0). */
  | { kind: "mix"; selected: number }
  /** ESC → cancelar SIN mezclar (@0x1a2e, devuelve -1). */
  | { kind: "close" };

/** Estado inicial: cursor en el primer reagente, nada marcado. */
export function initMixReagentPicker(): MixReagentPickerModel {
  return { cursor: 0, selected: 0 };
}

/**
 * Reductor de teclas del selector (`mix_reagent_select` @0x18be):
 *   · Flechas: ↑/← suben el cursor, ↓/→ lo bajan (el getdir 0x66ec devuelve 1/3 →
 *     arriba @0x19b2, 2/4 → abajo @0x19d0). Sin wrap: clamp en los extremos.
 *   · RETURN (0x0d) / Space (0x20) → TOGGLE del reagente del cursor (@0x19ee:
 *     `di ^= 0x80>>id`); el selector NO se cierra.
 *   · 'M' (0x4d) → `mix` con la máscara actual (@0x19e0/0x1a4b).
 *   · ESC (0x1b) → `cancel` (@0x1a2e). Resto: ignoradas (@0x1a50 → 0x19c5).
 * Con `rowCount<=0` (defensivo) cierra sin mezclar.
 */
export function mixReagentKey(
  model: MixReagentPickerModel,
  key: string,
  rows: readonly MixReagentRow[],
): MixReagentAction {
  const rowCount = rows.length;
  if (rowCount <= 0) return { kind: "close" };
  const moveTo = (raw: number): MixReagentAction => {
    const cursor = Math.max(0, Math.min(rowCount - 1, raw));
    if (cursor === model.cursor) return { kind: "none" };
    return { kind: "move", model: { cursor, selected: model.selected } };
  };
  switch (key) {
    case "ArrowUp":
    case "Up":
    case "ArrowLeft":
    case "Left":
      return moveTo(model.cursor - 1);
    case "ArrowDown":
    case "Down":
    case "ArrowRight":
    case "Right":
      return moveTo(model.cursor + 1);
    case "Enter":
    case " ":
    case "Spacebar": {
      const id = rows[model.cursor]!.reagentId;
      const selected = model.selected ^ reagentBit(id); // toggle (@0x19f9 `xor di,ax`)
      return { kind: "toggle", model: { cursor: model.cursor, selected } };
    }
    case "m":
    case "M":
      return { kind: "mix", selected: model.selected };
    case "Escape":
      return { kind: "close" };
    default:
      return { kind: "none" };
  }
}
