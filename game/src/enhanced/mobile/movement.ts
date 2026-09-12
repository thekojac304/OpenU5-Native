/**
 * CRUCETA ENHANCED — cuatro direcciones y NADA MÁS.
 *
 * QUÉ CAMBIA respecto de la clásica (`ui/touch.ts`, `.touch-dpad`): el CONTENIDO, no el
 * comportamiento. La cruz clásica es una rejilla 3×3 cuyas celdas libres ocupan Enter,
 * Esc y Espacio (los muda allí `skin/portrait/deck-dom.ts`). Aquí esas tres celdas se
 * quedan VACÍAS: tres teclas de confirmar/cancelar pegadas a los cuatro objetivos más
 * pulsados del juego son la peor vecindad posible para el toque accidental, y en un juego
 * POR TURNOS un toque errado cuesta un turno —y puede costar un encuentro— sin deshacer.
 * Las esquinas vacías hacen de zona muerta entre direcciones.
 *
 * 🔴 EL CONTRATO DE TIEMPO NO SE TOCA, y por eso se REUSA `HoldRepeat` en vez de
 * reimplementarlo: primer disparo INMEDIATO, `HOLD_START_MS` (400) de contacto sostenido
 * antes de repetir, y `HOLD_REPEAT_MS` (220) de cadencia. Esos números son el arreglo
 * MEDIDO del defecto «la cruceta da DOS pasos con una pulsación» (una pulsación de pulgar
 * dura ~250 ms y el `setInterval` sin retardo inicial mandaba dos `keydown`). Escribir
 * aquí otros temporizadores sería re-comprar ese defecto en una segunda superficie.
 *
 * Y por el mismo motivo la cruceta conserva la ÚNICA EXCEPCIÓN declarada del deck a la
 * activación-al-soltar: dispara en `pointerdown`. Andar exige respuesta inmediata, y
 * mantener-para-repetir es incompatible con «dispara al soltar». No vive en ninguna zona
 * scrolleable, que es de donde nace esa regla.
 *
 * SIN DIAGONALES: el original no las tiene («No diagonal movement (faithful to the
 * original)», `docs/controls.md`) y el `Direction` del motor es de cuatro. Sin joystick y
 * sin swipe: la métrica del propio repo mide 5 toques = 5 pasos a 60 ms de cadencia, y un
 * eje analógico tendría que volver a cuantizar eso a pasos discretos añadiendo latencia
 * al único control que debe ser instantáneo.
 */
import { press, setTsLabel, DPAD_ARIA, CLASE_SIN_RAGECLICK } from "../../ui/touch.js";
import { HoldRepeat } from "../../ui/hold-repeat.js";

/** Las cuatro direcciones, con su clase de celda. NO hay una quinta entrada. */
export const ENHANCED_DIRS: readonly { key: string; glyph: string; cls: string }[] = [
  { key: "ArrowUp", glyph: "▲", cls: "u5e-up" },
  { key: "ArrowLeft", glyph: "◀", cls: "u5e-left" },
  { key: "ArrowRight", glyph: "▶", cls: "u5e-right" },
  { key: "ArrowDown", glyph: "▼", cls: "u5e-down" },
];

export interface MovementHandle {
  el: HTMLElement;
  dispose(): void;
}

/**
 * Construye la cruceta. `fire` es inyectable SÓLO para los tests (por defecto, el
 * `press()` del deck: `keydown` sobre `document.body`, la misma vía exacta que el deck
 * clásico y que una tecla física — ver la ficha #218 en `ui/touch.ts` sobre por qué el
 * target es `body` y no `window`).
 */
export function buildMovement(fire: (key: string) => void = press): MovementHandle {
  const el = document.createElement("div");
  el.className = "u5e-move";
  // Nombre accesible del BLOQUE, no de cada flecha: VoiceOver/TalkBack anuncian primero
  // el grupo y luego el rumbo de cada celda (los `DPAD_ARIA` de abajo).
  el.setAttribute("role", "group");
  setTsLabel(el, undefined, undefined, "Movement");

  let hold: HoldRepeat | null = null;
  const stop = (): void => {
    hold?.release();
    hold = null;
  };

  for (const dir of ENHANCED_DIRS) {
    const btn = document.createElement("button");
    btn.type = "button";
    // `ph-no-rageclick` — la MISMA clase que lleva la cruceta clásica, y por la misma
    // razón medida: el detector de PostHog cuenta 3 clics a <30 px y <1000 ms como
    // `$rageclick`, y andar tres casillas seguidas satisface ese predicado POR
    // CONSTRUCCIÓN (239 de 266 eventos de la auditoría eran estas cuatro flechas). Suprime
    // sólo la etiqueta `$rageclick`; el clic se sigue capturando como dato.
    btn.className = `u5e-dbtn ${dir.cls} ${CLASE_SIN_RAGECLICK}`;
    btn.dataset.key = dir.key;
    btn.textContent = dir.glyph;
    // Sin `aria-label` el lector lee el CONTENIDO («triángulo negro apuntando hacia
    // arriba») del control más usado del juego. Se nombra por el RUMBO, que es como el
    // juego habla de las direcciones — misma tabla que el deck clásico, traducible.
    setTsLabel(btn, undefined, DPAD_ARIA[dir.key], DPAD_ARIA[dir.key]);
    btn.addEventListener("pointerdown", (ev) => {
      ev.preventDefault();
      stop(); // idempotente: un pointerdown nuevo re-arma desde cero
      hold = new HoldRepeat(() => fire(dir.key));
      hold.press();
    });
    // Los TRES finales de gesto, como en el deck clásico. `pointerleave` importa tanto
    // como `pointerup`: sin él, deslizar el dedo fuera del botón dejaba al personaje
    // andando; `pointercancel` cubre que el navegador se quede el gesto.
    for (const evt of ["pointerup", "pointerleave", "pointercancel"]) {
      btn.addEventListener(evt, stop);
    }
    el.appendChild(btn);
  }

  return {
    el,
    dispose(): void {
      stop();
      el.remove();
    },
  };
}
