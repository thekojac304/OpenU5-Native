/**
 * FILA RÁPIDA — las cuatro acciones que no deberían costar un cajón.
 *
 * QUÉ RESUELVE (encargo del usuario, 13-09): con una sola puerta a los comandos, abrir
 * una casa costaba TRES toques (Commands → Interaction → Open) y tapaba el juego por el
 * camino. Los verbos de «actúa sobre lo que tengo delante» se pulsan cada pocos pasos, y
 * pagar un cajón por cada uno es el impuesto que esta fila quita.
 *
 * CUÁLES y POR QUÉ: los juegos por contexto viven en `groups.ts` (`QUICK_DEFAULTS`) con su
 * derivación entera —incluido el corpus que MIDÍ y que decidí NO usar como ranking, y por
 * qué—, y la personalización del juego de MUNDO en `quickslots.ts`. Aquí sólo se pinta. Un
 * sitio para la decisión, otro para la preferencia, otro para el render.
 *
 * DÓNDE SE PINTA: ya no es una banda de ancho completo encima de la cruceta, sino una
 * rejilla 2×2 AL LADO de ella, en el hueco que los layouts de cruceta a izquierda o a
 * derecha desperdiciaban (`css.ts` §4b). Con la cruceta al CENTRO vuelve a ser una fila de
 * cuatro, encima. La geometría la decide el CSS; este módulo no la conoce.
 *
 * CONTEXTO: las teclas CAMBIAN con el contexto (mundo, mazmorra, arena tienen juegos
 * distintos) y además se filtran por el CENSO. Si el binario rechaza ese comando ahí, la
 * celda se marca `hidden` y queda VACÍA: ofrecer una orden que el binario contesta con
 * «Not here!» mete tanta divergencia como quitarla (ficha #71).
 *
 * 🔴 LAS CUATRO CELDAS SE CREAN UNA VEZ Y NO SE RECONSTRUYEN, aunque ahora su tecla varíe:
 * el `bindTap` se cablea contra una casilla MUTABLE (`teclas[i]`) en vez de contra un
 * literal. Reconstruir el DOM en cada cambio de contexto tiraría el `TapGate` de un gesto
 * en curso (entrar en combate a mitad de toque) y obligaría a re-cablear cuatro listeners
 * por refresco.
 *
 * Despacho por `press()`, como todo lo demás: `keydown` sobre `document.body`.
 */
import { press, setTsLabel, bindTap } from "../../ui/touch.js";
import { QUICK_SLOTS, quickFor, type DeckCtx } from "./groups.js";
import { onQuickSlotsChange, quickKeysFor } from "./quickslots.js";

export interface QuickBarHandle {
  el: HTMLElement;
  /** Re-deriva las celdas para el contexto. Idempotente. */
  syncContext(ctx: DeckCtx): void;
  dispose(): void;
}

/** Construye la fila. `fire` es inyectable sólo para los tests (por defecto, `press`). */
export function buildQuickBar(fire: (key: string) => void = press): QuickBarHandle {
  const el = document.createElement("div");
  el.className = "u5e-quickbar";
  el.setAttribute("role", "group");
  setTsLabel(el, undefined, undefined, "Quick actions");

  // La tecla VIVA de cada celda. Es lo que el `bindTap` lee en el momento del toque, y por
  // eso cambiar de contexto —o de preferencia— no exige re-cablear nada.
  const teclas: string[] = Array.from({ length: QUICK_SLOTS }, () => "");

  const celdas = Array.from({ length: QUICK_SLOTS }, (_, i) => {
    const btn = document.createElement("button");
    btn.type = "button";
    btn.className = "u5e-quick";
    btn.dataset.u5eQuick = String(i);
    bindTap(btn, () => {
      const k = teclas[i];
      if (k) fire(k);
    });
    el.appendChild(btn);
    return btn;
  });

  let ctxVivo: DeckCtx = "world";

  const syncContext = (ctx: DeckCtx): void => {
    ctxVivo = ctx;
    const keys = quickKeysFor(ctx);
    const defs = quickFor(ctx, keys);
    defs.forEach((def, i) => {
      const btn = celdas[i]!;
      if (!def) {
        // Celda sin comando en este contexto: se apaga Y se desarma. Dejar la tecla viva
        // bajo un botón `hidden` sería un despacho fantasma si alguien lo revelara por CSS.
        teclas[i] = "";
        btn.hidden = true;
        delete btn.dataset.key;
        return;
      }
      teclas[i] = def.key;
      btn.hidden = false;
      btn.dataset.key = def.key;
      // El rótulo sale de la tabla CENSADA del contexto, no de una copia: así los vocablos
      // siguen diciendo lo que su tabla dice, y `relabel()` los traduce.
      setTsLabel(btn, def.label, def.title, def.title ?? def.label);
    });
  };

  // La preferencia puede cambiar con la chapa montada (el jugador abre Ajustes y elige otro
  // comando): se re-deriva el contexto VIVO, que es todo lo que hace falta.
  const offSlots = onQuickSlotsChange(() => syncContext(ctxVivo));

  return {
    el,
    syncContext,
    dispose(): void {
      offSlots();
      el.remove();
    },
  };
}
