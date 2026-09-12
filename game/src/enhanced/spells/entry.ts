/**
 * EL EMPALME — el único punto donde la lista moderna toca el juego, y el sitio donde se
 * cumple (o se rompe) la promesa del encargo.
 *
 * ── LA REGLA, ENTERA ─────────────────────────────────────────────────────────────────
 * Modo CLÁSICO:
 *     (C)ast → `pickCaster` → `pickSpellTyped` → el jugador TECLEA las iniciales →
 *     `matchSpellByInitials` → `castSpell` → efecto/objetivo
 * Modo MODERNO:
 *     (C)ast → `pickCaster` → LISTA → se elige un hechizo → **el panel teclea sus
 *     iniciales por el MISMO `pickSpellTyped`** → `matchSpellByInitials` → `castSpell` → …
 *
 * O sea: lo ÚNICO que cambia es quién pulsa las teclas. No hay una segunda vía de
 * resolución, no se llama a `castSpell` desde aquí, no se toca `GameState`, no se salta
 * ningún prompt de objetivo, no se altera el orden del RNG y no se ahorra ni un gate.
 *
 * 🔴 POR QUÉ TECLAS SINTÉTICAS Y NO `submit(initials)` A PELO. Llamar al callback
 * directamente saltaría todo lo que el getstring rúnico hace ADEMÁS de acumular letras:
 * las dos filas de consola («Spell name:» + la fila de cursor `:`), el ECO rúnico en
 * mayúsculas («:IN MANI CORP»), el estado modal de `PromptManager`, el gate del cursor
 * (`refreshAwaiting`) y —la que más importa— el GRABADOR DE REPETICIONES, que registra
 * teclas: una partida lanzada desde la lista tiene que poder reproducirse en modo clásico,
 * y sólo lo consigue si lo que se grabó son las mismas teclas que habría tecleado el
 * jugador. `press()` es la misma primitiva que ya usan los botones del deck táctil
 * (`ui/touch.ts`: un `KeyboardEvent` sobre `document.body` que burbujea a `window`), así
 * que la geometría del evento es la de una tecla real.
 *
 * ★ EL ORDEN IMPORTA Y NO ES NEGOCIABLE: primero `pickSpellTyped` (que ARMA el prompt), y
 * sólo DESPUÉS las teclas. Al revés, las iniciales llegarían a un juego sin prompt vivo y
 * se interpretarían como comandos sueltos — que es exactamente el modo de fallo que
 * describe el gate «Absorbed!» del combate («teclear c,i,v,p,y no casteaba: la 'c'
 * consumía el turno y las runas caían como comandos»).
 *
 * ★ Y LA LISTA SE CIERRA ANTES DE TECLEAR: su listener de captura se traga toda tecla
 * mientras vive (ver `panel.ts`), así que teclear con el panel abierto no llegaría a
 * ninguna parte. `onPick`/`onCancel` se invocan ya desmontado.
 */
import { press as pressReal } from "../../ui/touch.js";
import { listaDeHechizosActiva } from "./mode.js";
import { openSpellPicker, type CastPlace, type CasterInfo } from "./panel.js";
import type { SpellEntry } from "./catalog.js";

/** La firma EXACTA de `pickers.pickSpellTyped` — el getstring rúnico fiel, sin envolver. */
export type PickSpellTyped = (prefix: string, submit: (initials: string) => void) => void;

export interface CastEntryDeps {
  /** El getstring rúnico FIEL. Es la ÚNICA vía de resolución, en los dos regímenes. */
  pickSpellTyped: PickSpellTyped;
  /** Catálogo vivo (48 filas). Función para que el llamador no tenga que construirlo antes. */
  catalog(): readonly SpellEntry[];
  /** Cantidad mezclada (`GameState.spellQuantities[index]`). Sólo para pintar. */
  quantity(index: number): number;
  /** Dónde se lanza AHORA — exterior / pueblo / mazmorra / arena. Sólo para atenuar filas. */
  place(): CastPlace;
  /** Lanzador ya resuelto por `pickCaster`. Sólo para atenuar filas. */
  caster(): CasterInfo | null;
  /** Régimen vivo. Inyectable; por defecto la preferencia real (`mode.ts`). */
  modern?(): boolean;
  /** Emisor de teclas. Inyectable; por defecto el `press()` del deck. */
  press?(key: string): void;
}

/**
 * Teclea unas iniciales por el getstring rúnico y las envía.
 *
 * Una tecla por sílaba (el reductor de `prompt-manager.ts` traduce la INICIAL a la palabra
 * rúnica: 'I'→"IN") y `Enter` para enviar, que es el `0x0d` del binario (CAST2 0x00de).
 * Los 48 hechizos tienen 4 sílabas o menos, que es el tope `max` del prompt.
 */
export function typeInitials(initials: string, press: (key: string) => void): void {
  for (const ch of initials) press(ch);
  press("Enter");
}

/**
 * Construye el sustituto de `pickSpellTyped` para los tres (C)ast (exterior/pueblo,
 * mazmorra y arena). MIX NO PASA POR AQUÍ: mezclar es otro comando, con otro prompt
 * («For what spell?») y otra mecánica, y el encargo es sobre lanzar.
 */
export function makeCastSpellEntry(deps: CastEntryDeps): PickSpellTyped {
  const press = deps.press ?? pressReal;
  const modern = deps.modern ?? (() => listaDeHechizosActiva());
  return (prefix, submit) => {
    if (!modern()) {
      deps.pickSpellTyped(prefix, submit); // CLÁSICO: ni un cambio, ni una capa.
      return;
    }
    openSpellPicker({
      entries: deps.catalog(),
      quantity: deps.quantity,
      place: deps.place(),
      caster: deps.caster(),
      onPick: (entry) => {
        deps.pickSpellTyped(prefix, submit); // arma el prompt fiel…
        typeInitials(entry.initials, press); // …y lo teclea por el camino del teclado
      },
      onCancel: () => {
        // Cancelar es EXACTAMENTE el ESC del getstring del original: `submit("")` ⇒ el
        // llamador imprime «None!» (DS 0x4611) y no se lanza nada. No se inventa una
        // salida nueva; se usa la que el binario ya tiene.
        deps.pickSpellTyped(prefix, submit);
        press("Escape");
      },
    });
  };
}
