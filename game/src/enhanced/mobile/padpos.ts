/**
 * POSICIÓN DE LA CRUCETA ENHANCED — izquierda · CENTRO · derecha, y su persistencia.
 *
 * ── POR QUÉ UN DATO PROPIO Y NO UN TERCER VALOR DE `u5.padSide` ──────────────────────
 * El encargo dice «extiende el ajuste de lado a tres», y la tentación era escribir
 * `"center"` en `u5.padSide` (`ui/touch.ts`). 🔴 NO SE HACE, y la razón es de CONSUMIDORES,
 * no de gusto: `data-pad-side` lo leen hoy SEIS sitios del deck CLÁSICO —`index.html`
 * (`#app`, `.touch-main`/`.touch-util`, la columna apaisada y los tres paneles flotantes)
 * y los dos raíles de `skin/portrait/deck-ancho.ts`—, todos escritos como pares
 * `[data-pad-side="left"] / [data-pad-side="right"]`. Un tercer valor no casa con NINGUNA
 * de las dos ramas de cada par, así que el clásico se quedaría con el `justify-content`
 * sin fijar y los raíles sin adjudicar: una regresión silenciosa en el layout del que este
 * carril, por diseño, no hereda nada (ver la cabecera de `css.ts`).
 *
 * Y hay una razón de SIGNIFICADO, que es la de fondo: `padSide` dice a qué BORDE de la
 * pantalla se pega el deck entero (en apaisado es literalmente de qué lado va la columna);
 * esto dice dónde cae la CRUZ dentro de la chapa. Son dos preguntas distintas y «centro»
 * sólo tiene respuesta en la segunda.
 *
 * ── LO QUE SÍ SE CONSERVA: EL COMPORTAMIENTO ANTERIOR ────────────────────────────────
 * Sin nada guardado, la posición se SIEMBRA del `u5.padSide` vigente, así que quien tenía
 * la cruceta a la derecha la sigue teniendo ahí tras actualizar. Y el ajuste, al elegir
 * izquierda o derecha, mueve TAMBIÉN el `padSide` clásico (lo hace `shell/sections.ts` con
 * el `swapPadSide()` de siempre): así el raíl apaisado y los paneles flotantes siguen a la
 * cruz en vez de quedarse en el lado contrario. «Centro» deja el `padSide` quieto — ver
 * abajo.
 *
 * ── APAISADO: NO HAY CENTRO, Y SE DECLARA ────────────────────────────────────────────
 * En apaisado el deck NO es una banda inferior sino una COLUMNA LATERAL pegada a un borde
 * (`index.html`, `html[data-orient="landscape"] .touch-controls`), con el mapa llenando el
 * resto. «Centrar» la cruz dentro de una columna de ~110 px de ancho no la mueve a ningún
 * sitio: la columna YA está centrada en su propio eje corto (`.u5e-move { align-self:
 * center }`, bloque 7 del CSS). Centrar en el VIEWPORT exigiría sacar la cruz de la columna
 * y ponerla sobre el mapa, que es justo la superposición que este deck evita. Así que en
 * apaisado «centro» se sirve como el lado que el `padSide` diga, y el ajuste lo dice.
 */

/** Las tres posiciones. El orden es el del selector de ajustes. */
export const PAD_POSITIONS = ["left", "center", "right"] as const;
export type PadPos = (typeof PAD_POSITIONS)[number];

/** Clave de preferencia. Mismo prefijo `u5.` que `u5.padSide` / `u5.enhancedControls`. */
export const PAD_POS_KEY = "u5.enhancedPadPos";

/** Atributo que el CSS lee (`html[data-u5e-pad="center"] …`). */
export const PAD_POS_ATTR = "u5ePad";

/** Lo que vale sin nada guardado y sin `u5.padSide` legible. */
export const PAD_POS_DEFAULT: PadPos = "left";

/** Clave del lado clásico. Se LEE para sembrar; nunca se escribe desde aquí. */
const PAD_SIDE_KEY = "u5.padSide";

/** ¿Es `v` una de las tres? Guarda de todo lo que entra de fuera (storage, URL, ajuste). */
export function isPadPos(v: unknown): v is PadPos {
  return typeof v === "string" && (PAD_POSITIONS as readonly string[]).includes(v);
}

/**
 * Lee la preferencia. SIEMBRA del `u5.padSide` guardado cuando no hay valor propio — que
 * es el caso de todo el que ya jugaba antes de este carril: su cruceta no se mueve de sitio
 * al actualizar, que es el requisito «preserve existing Left/Right behavior».
 *
 * Un valor corrupto (otra versión, edición a mano) cae al default en vez de romper: la
 * misma política que el resto de preferencias del port.
 */
export function loadPadPos(): PadPos {
  try {
    const propio = localStorage.getItem(PAD_POS_KEY);
    if (isPadPos(propio)) return propio;
    const heredado = localStorage.getItem(PAD_SIDE_KEY);
    if (heredado === "right" || heredado === "left") return heredado;
  } catch {
    /* almacenamiento denegado (modo privado): default, sin romper. */
  }
  return PAD_POS_DEFAULT;
}

/** Persiste. No-op silencioso si el almacenamiento está denegado. */
export function savePadPos(pos: PadPos): void {
  try {
    localStorage.setItem(PAD_POS_KEY, pos);
  } catch {
    /* idem `savePadSide`: la preferencia no persiste, pero la sesión funciona. */
  }
}

/**
 * Publica la posición en `<html>` para el CSS. Idempotente (no escribe si ya está), que
 * es lo que impide que un `sync` en cada `resize` gaste un ciclo de estilo por evento —
 * la misma guarda que `forma.ts`.
 */
export function applyPadPos(pos: PadPos): void {
  if (typeof document === "undefined") return;
  const root = document.documentElement;
  if (root.dataset[PAD_POS_ATTR] !== pos) root.dataset[PAD_POS_ATTR] = pos;
}

/** Retira el atributo (desmontaje de la chapa). */
export function clearPadPos(): void {
  if (typeof document === "undefined") return;
  delete document.documentElement.dataset[PAD_POS_ATTR];
}

/** Suscriptores vivos del cambio de posición (el ajuste avisa, la chapa re-aplica). */
const oyentes = new Set<(pos: PadPos) => void>();

/**
 * Fija la posición: persiste, publica y avisa. Es la ÚNICA vía de escritura — el ajuste
 * de `shell/sections.ts` y los tests pasan por aquí, así que no hay dos caminos que
 * puedan discrepar.
 */
export function setPadPos(pos: PadPos): void {
  savePadPos(pos);
  applyPadPos(pos);
  for (const fn of [...oyentes]) fn(pos);
}

/** (Des)suscribe. Devuelve la baja, como el resto de los `on*` del repo. */
export function onPadPosChange(fn: (pos: PadPos) => void): () => void {
  oyentes.add(fn);
  return () => {
    oyentes.delete(fn);
  };
}
