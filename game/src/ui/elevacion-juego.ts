/**
 * EL LIFT DEL JUEGO — **UNA SOLA AUTORIDAD** sobre `contenedorJuego.style.transform`.
 *
 * ══════════════════════════════════════════════════════════════════════════════════════
 * POR QUÉ EXISTE (y es la misma lección que ya paga `e2e/mobile/suelo-tactil.ts`)
 * ══════════════════════════════════════════════════════════════════════════════════════
 * «Subir la pila de juego para que el teclado no tape la consola» tenía DOS demandantes y
 * ninguno sabía del otro:
 *
 *   · `skin/portrait/deck-nativo.ts:elevar()` — compensa el TECLADO DEL SISTEMA, midiendo
 *     contra `visualViewport`. Y su primera línea, cuando decide que no toca elevar, es
 *     `contenedorJuego.style.transform = ""`.
 *   · la CAPA DE TECLADO PROPIA (`ui/teclado-capa.ts`) — compensa la hoja A–Z/123/Sí-No,
 *     que desde el carril de consistencia es un overlay `fixed` y por tanto NO encoge el
 *     canvas por la vía de la reserva en los layouts donde el mapa no negocia su alto.
 *
 * Los dos escriben la MISMA propiedad del MISMO elemento. Sin árbitro, el que corre
 * segundo gana: un `visualViewport` `resize` (la barra del navegador apareciendo, que en
 * un teléfono pasa constantemente) dispara el `elevar()` del sistema, que no ve teclado
 * del sistema abierto, y **borra el lift de la hoja propia** — la consola vuelve a quedar
 * debajo del teclado y el defecto se lee como intermitente.
 *
 * Aquí las demandas se REGISTRAN por fuente y se aplica la MAYOR (la más negativa): dos
 * teclados abiertos a la vez —se puede: la hoja propia arriba y el botón «ABC» pulsado—
 * piden lo suyo y el juego sube lo que haga falta para los dos. Retirar una demanda no
 * puede pisar la otra.
 *
 * 🔴 ES UN `transform`, NO UN CAMBIO DE LAYOUT, y eso es lo que lo hace seguro para el
 * bucle de la reserva: no mueve el border box de `.touch-controls` (que es lo que
 * `syncReserve()` mide), no re-escala el canvas y no re-dispara `relayout()`. La única
 * lectura de layout es la del demandante, que mide SIN el lift puesto (`medirSinLift`).
 */

/** Quién pide subir el juego. Cerrado a propósito: dos fuentes, y las dos declaradas. */
export type FuenteElevacion = "sistema" | "hoja";

/** Demandas vivas, en px ≤ 0 (la convención de `keyboardClearance`). */
const demandas = new Map<FuenteElevacion, number>();

/** El elemento que sube. Lo fija quien lo conoce (el puente nativo, la capa de teclado). */
let objetivo: HTMLElement | null = null;

/**
 * Declara QUÉ elemento se eleva. Idempotente y de «último que llega manda» a propósito:
 * las dos fuentes resuelven al mismo nodo (el contenedor del canvas de la piel), así que
 * un desacuerdo sería un defecto de resolución, no un estado a conciliar. Cambiar de
 * objetivo LIMPIA el anterior: una piel que se desmonta no puede dejar el `transform`
 * puesto sobre un nodo huérfano.
 */
export function fijarObjetivoElevacion(el: HTMLElement | null): void {
  if (objetivo === el) return;
  if (objetivo) {
    objetivo.style.transform = "";
    objetivo.removeAttribute("data-u5-lift");
  }
  objetivo = el;
  aplicar();
}

/** El objetivo vivo (para el arnés y para que un demandante no re-resuelva si ya está). */
export function objetivoElevacion(): HTMLElement | null {
  return objetivo;
}

/**
 * Registra (o retira, con 0) la demanda de una fuente y re-aplica el resultado.
 * `dy` es ≤ 0; un valor positivo se ignora (nadie BAJA el juego).
 */
export function pedirElevacion(fuente: FuenteElevacion, dy: number): void {
  const v = dy < 0 ? dy : 0;
  if ((demandas.get(fuente) ?? 0) === v) return;
  if (v === 0) demandas.delete(fuente);
  else demandas.set(fuente, v);
  aplicar();
}

/** El lift aplicado ahora mismo (px ≤ 0). Lo lee el arnés y los tests. */
export function elevacionViva(): number {
  let dy = 0;
  for (const v of demandas.values()) dy = Math.min(dy, v);
  return dy;
}

/**
 * Mide con el lift RETIRADO y lo restaura. Sin esto la corrección se realimenta:
 * `getBoundingClientRect()` incluye los `transform` de los ancestros, así que medir con el
 * lift puesto devuelve un canvas que «ya cabe» y la demanda se evapora al frame siguiente
 * (el defecto clásico del lift que parpadea). Es la misma precaución que `elevar()` ya
 * tomaba a mano con su `transform = ""` antes de medir; aquí vive UNA vez.
 */
export function medirSinLift<T>(fn: () => T): T {
  const el = objetivo;
  if (!el) return fn();
  const previo = el.style.transform;
  el.style.transform = "";
  try {
    return fn();
  } finally {
    el.style.transform = previo;
  }
}

/** Suelta TODO (desmontaje de piel / `dispose()` del deck). Idempotente. */
export function limpiarElevacion(): void {
  demandas.clear();
  if (objetivo) {
    objetivo.style.transform = "";
    objetivo.removeAttribute("data-u5-lift");
  }
  objetivo = null;
}

/**
 * DESGLOSE POR FUENTE, en un `data-` del propio elemento.
 *
 * ⚠ ES PARA EL ARNÉS, Y SE DECLARA COMO TAL — pero no es un `if (test)`: es estado
 * publicado, del mismo tipo que `data-deck-sheet` o `data-orient`, y cuesta un `setAttribute`
 * por cambio de demanda (un puñado por sesión).
 *
 * La razón concreta: el `transform` resultante es la SUMA de dos demandas y desde fuera son
 * indistinguibles. Los tests de ② (`e2e/mobile/deck-modos.spec.ts`) miden que **el teclado
 * del SISTEMA** no eleve cuando no está desplegado, y que suelte su lift al cerrarse — dos
 * afirmaciones sobre UNA de las fuentes. Sin desglose sólo se puede mirar el agregado, y
 * desde el 12-09 el agregado lleva también la demanda de la hoja propia: los dos tests
 * acusaban al sistema de un lift que era del teclado del port.
 */
function publicarDesglose(): void {
  if (!objetivo) return;
  const partes: string[] = [];
  for (const [f, v] of demandas) partes.push(`${f}:${Math.round(v)}`);
  if (partes.length === 0) objetivo.removeAttribute("data-u5-lift");
  else objetivo.setAttribute("data-u5-lift", partes.sort().join(" "));
}

function aplicar(): void {
  if (!objetivo) return;
  const dy = elevacionViva();
  const valor = dy === 0 ? "" : `translateY(${dy}px)`;
  if (objetivo.style.transform !== valor) objetivo.style.transform = valor;
  publicarDesglose();
}
