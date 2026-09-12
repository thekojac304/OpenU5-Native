/**
 * LA FORMA DE LA CHAPA — apilada o en BANDA, y el único booleano que el CSS no puede sacar.
 *
 * ── QUÉ DECIDE ───────────────────────────────────────────────────────────────────────
 * En el LAYOUT PARTIDO vertical el mapa se lleva el ancho entero y su alto sale de ahí
 * (`layout-cuadrado.ts`, ruling «EL MAPA NO NEGOCIA»), así que lo que le queda a la
 * botonera es `100dvh − --u5-reflow-content` y puede ser MUY poco. Medido en un iPhone SE
 * emulado (375×667): 123 px, contra los 274 que la chapa apilada pide en reposo — de ahí
 * los 151 px de solape sobre la banda de roster y consola que este carril arregla.
 *
 * La chapa responde en DOS escalones, y sólo el segundo necesita JS:
 *   · CONTINUO (puro CSS, `enhanced/mobile/css.ts` §2d): la celda de la cruceta se encoge
 *     con el hueco — de 48 a 40 px en la forma apilada, y de 48 a 34 en la banda, que es
 *     donde el hueco de verdad aprieta. Es aritmética, y `clamp()` la hace sola.
 *   · DISCRETO (esto): por debajo de cierto hueco la chapa deja de ser COLUMNA y pasa a
 *     ser BANDA —cruceta a un lado, los seis comandos al otro en 3×2, fila rápida al
 *     cajón—, que cambia `flex-direction`. Eso es una PALABRA CLAVE, no un número: no hay
 *     `clamp()` que la interpole ni `@media` que pregunte por el valor de una custom
 *     property, así que hace falta un booleano.
 *
 * ── POR QUÉ ESTO NO ES «OTRO OBSERVADOR DE LAYOUT» ───────────────────────────────────
 * 🔴 La cabecera de `css.ts` prohíbe realimentar el bucle de la reserva, y este módulo no
 * puede hacerlo — no por cuidado, sino por la forma de sus entradas:
 *
 *     innerHeight ──┐
 *                   ├──► hueco ──► forma ──► alto del deck ──► --u5-touch-reserve
 *   --u5-reflow-content ┘                                              │
 *                   ▲                                                  │
 *                   └──────────────── NO VUELVE ◄──────────────────────┘
 *
 * `--u5-reflow-content` lo publica la piel desde `squareLayout()`, que saca la escala del
 * ANCHO (`saLibre = W / FRAME_W`) y cuya escala de banda IGNORA el alto disponible — la
 * firma lo declara: `squareBandScalePortrait(availW, _availH, cap)`. Y el deck vertical
 * sólo consume ALTO: su reserva es `padding-bottom` de `#app`, que no cambia `clientWidth`.
 * O sea que cambiar de forma no puede mover ninguna de las dos entradas de la decisión.
 * (La elección reflow/clásico de `squareLayout` sí mira el alto, pero en vertical sale
 * SIEMPRE «reflow»: `W/191 > min(W/320, H/200)` se cumple en los dos casos del `min`.)
 *
 * ── HISTÉRESIS, Y POR QUÉ NO ES OPCIONAL ─────────────────────────────────────────────
 * Con un umbral único, un viewport que oscile un píxel alrededor de él —y el `dvh` de un
 * móvil oscila: la barra del navegador entra y sale al scrollear— haría que la chapa
 * cambiara de forma en bucle bajo el dedo. Dos umbrales separados por 24 px convierten esa
 * oscilación en un no-evento: hay que cruzar la banda entera para conmutar.
 */

/**
 * Hueco (px) por debajo del cual la chapa APILADA ya no cabe ni con las celdas al suelo.
 *
 * No es un número redondo elegido a ojo, es la suma de la forma apilada en su versión más
 * compacta: fila rápida 44 + hueco 4 + cruceta (3×40 + 2×5 = 130) + hueco 4 + barra 44 +
 * relleno 12 ≈ **238**. Se deja en 246 para que la conmutación ocurra ANTES de que las
 * celdas lleguen a su suelo de 40, y no justo cuando ya están estranguladas.
 */
export const UMBRAL_BANDA = 246;

/**
 * Hueco (px) a partir del cual se vuelve a apilar. 24 px por encima del otro = la banda de
 * histéresis (ver la cabecera). 24 y no 2: es el orden de magnitud de lo que entra y sale
 * la barra de direcciones de un navegador móvil, que es la oscilación que hay que absorber.
 */
export const UMBRAL_APILADA = UMBRAL_BANDA + 24;

/** Atributo que el CSS lee (`.touch-controls[data-compacto="1"]`). */
export const ATTR_COMPACTO = "compacto";

/**
 * ¿Toca la forma BANDA? PURA, con la forma anterior como entrada — que es lo que hace la
 * histéresis posible sin estado escondido.
 *
 * `hueco` en px. `antes` = si la forma vigente ya era la banda.
 */
export function formaCompacta(hueco: number, antes: boolean): boolean {
  if (antes) return hueco < UMBRAL_APILADA; // para SALIR de banda hay que subir del alto
  return hueco < UMBRAL_BANDA; // para ENTRAR en banda hay que bajar del bajo
}

/**
 * Lee el hueco vivo: `100dvh − --u5-reflow-content`.
 *
 * Sin la propiedad —layout clásico, apaisado, escritorio— devuelve el viewport entero, o
 * sea un hueco que jamás cruza el umbral: fuera del layout partido esto no decide nada, y
 * ésa es la misma forma «el fallback la vuelve inerte» que usa el CSS.
 *
 * `innerHeight` y no `clientHeight` del root: es la medida que el `100dvh` del CSS iguala
 * en los motores móviles con la barra desplegada, que es el caso que hay que acertar.
 */
export function huecoVivo(): number {
  if (typeof document === "undefined" || typeof window === "undefined") return Infinity;
  const raw = getComputedStyle(document.documentElement)
    .getPropertyValue("--u5-reflow-content")
    .trim();
  const alto = window.innerHeight || 0;
  if (raw === "") return alto; // sin layout partido: todo el viewport es hueco
  const n = Number.parseFloat(raw);
  if (!Number.isFinite(n)) return alto;
  return Math.max(0, alto - n);
}

export interface FormaHandle {
  /** Re-evalúa y aplica. Idempotente: si la forma no cambia, no toca el DOM. */
  sync(): void;
  /** ¿Está la chapa en forma BANDA ahora mismo? */
  compacta(): boolean;
  /** Deja de escuchar y retira el atributo. */
  dispose(): void;
}

/**
 * Engancha la forma al deck. Escucha el MISMO `resize` que la reserva ya dispara
 * (`syncReserve()` emite uno sintético en cada cambio de caja), así que no añade ni un
 * temporizador ni un `ResizeObserver` propio: se entera por la señal que ya existía.
 *
 * `orientationchange` va aparte porque en iOS llega ANTES de que `innerHeight` se haya
 * actualizado en algunos motores; el `resize` que le sigue re-sincroniza, y llamar dos
 * veces es gratis (la función es idempotente).
 */
export function attachForma(deck: HTMLElement): FormaHandle {
  let compacta = false;

  const sync = (): void => {
    const quiere = formaCompacta(huecoVivo(), compacta);
    // SALIDA TEMPRANA SI NADA CAMBIA, y no es higiene: este `sync` corre en cada `resize`,
    // y el `resize` lo dispara `syncReserve()` — que a su vez reacciona al alto del deck,
    // que es lo que estas dos líneas escriben. Escribir el mismo atributo otra vez no
    // cambiaría el layout, pero sí gasta un ciclo de estilo por evento. Con la guarda, el
    // segundo pase de cada cambio de forma muere aquí y la cadena termina.
    if (quiere === compacta && (deck.dataset[ATTR_COMPACTO] === "1") === quiere) return;
    compacta = quiere;
    if (quiere) deck.dataset[ATTR_COMPACTO] = "1";
    else delete deck.dataset[ATTR_COMPACTO];
  };

  // `sync` se registra DIRECTAMENTE: su firma ya es la del listener y envolverlo sólo
  // añadiría una referencia más que `dispose()` tendría que acertar a dar de baja.
  window.addEventListener("resize", sync);
  window.addEventListener("orientationchange", sync);
  sync();

  return {
    sync,
    compacta: () => compacta,
    dispose(): void {
      window.removeEventListener("resize", sync);
      window.removeEventListener("orientationchange", sync);
      delete deck.dataset[ATTR_COMPACTO];
    },
  };
}
