/**
 * INTERFAZ DE TIENDA (Shop interface) — el interruptor, y nada más.
 *
 * DOS RÉGIMENES, UNA SOLA MECÁNICA:
 *   · `classic` — el original entero: el mercader es una CONVERSACIÓN en la consola del
 *     marco EGA y se le contesta TECLEANDO la letra que imprime su menú (SHOPPES*.OVL,
 *     ret 2 del dispatcher de Talk; ver `ui/shop-console.ts`).
 *   · `modern`  — el MISMO flujo, con una superficie más: un panel compacto pinta las
 *     opciones que la fase de tienda VIVA ya expone (`ShopConsole.snapshot()`), y tocar
 *     una SINTETIZA su letra por el mismo `press()` que produciría el teclado. Ni una
 *     regla de comercio cambia de sitio, y el teclado sigue siendo camino de primera.
 *
 * 🔴 ESTE MÓDULO NO SABE NADA DE DOM, DE TIENDAS NI DE PRECIOS. Es el predicado y su
 * persistencia — el mismo reparto que `enhanced/spells/mode.ts` (lanzamiento),
 * `enhanced/mode.ts` (mandos) y `ui/regimen-tactil.ts` (régimen).
 *
 * TRES REGLAS, calcadas del interruptor hermano (`enhanced/spells/mode.ts`):
 *   1. NO ES SÓLO TÁCTIL. Acertar una letra de un menú que se acaba de imprimir cuesta
 *      lo mismo en un escritorio que en un teléfono (y el teléfono ni siquiera tiene
 *      teclado a mano), así que el ajuste se ofrece en las dos superficies — igual que
 *      la lista de hechizos y a diferencia de los mandos Enhanced.
 *   2. CLÁSICO POR DEFECTO. La identidad del port es la fidelidad; una capa nueva se gana
 *      el default con kilómetros, no con el commit que la estrena.
 *   3. LA URL MANDA SOBRE LO GUARDADO (`?shop=modern` / `?shop=classic`), como `?touch=1`,
 *      `?deck=`, `?reflow=`, `?enhanced=` y `?casting=` — es lo que permite un proyecto de
 *      Playwright por régimen sin tocar `localStorage`.
 */

/** Los dos regímenes. El valor persistido es EXACTAMENTE una de estas dos cadenas. */
export type ShopUi = "classic" | "modern";

/** Clave de preferencia. Mismo prefijo `u5.` que `u5.castingUI` / `u5.enhancedControls`. */
export const SHOP_UI_KEY = "u5.shopUI";

/** Régimen por defecto: el original, sin capas. */
export const SHOP_UI_DEFAULT: ShopUi = "classic";

/** Normaliza cualquier cadena al régimen que representa; `null` si no representa ninguno. */
function normaliza(v: string | null | undefined): ShopUi | null {
  if (v === null || v === undefined) return null;
  const s = v.trim().toLowerCase();
  if (s === "modern" || s === "moderno" || s === "panel" || s === "1") return "modern";
  if (s === "classic" || s === "clasico" || s === "clásico" || s === "0") return "classic";
  return null;
}

/**
 * Bandera de URL: `?shop=modern` / `?shop=classic`. Ausente (o valor que no nombra un
 * régimen) = `null` ⇒ manda lo guardado. PURA (recibe el `search`) para poder probarla
 * sin `window`, igual que `castingUiFlag` y `enhancedFlag`.
 */
export function shopUiFlag(search: string): ShopUi | null {
  try {
    return normaliza(new URLSearchParams(search).get("shop"));
  } catch {
    return null;
  }
}

/** Preferencia PERSISTIDA (sin mirar la URL). Default `classic`. */
export function shopUiGuardado(): ShopUi {
  try {
    return normaliza(localStorage.getItem(SHOP_UI_KEY)) ?? SHOP_UI_DEFAULT;
  } catch {
    // Almacenamiento denegado (modo privado): la preferencia no persiste, sin romper.
    return SHOP_UI_DEFAULT;
  }
}

/** Guarda la preferencia. No aplica nada: el régimen se lee en cada fase de tienda. */
export function guardarShopUi(ui: ShopUi): void {
  try {
    localStorage.setItem(SHOP_UI_KEY, ui);
  } catch {
    /* sin almacenamiento: la sesión vale igual, la preferencia no sobrevive. */
  }
}

/**
 * Régimen VIVO: URL primero, preferencia después. `search` es inyectable para los tests;
 * por defecto lee la URL viva.
 *
 * Se consulta cuando hay una tienda ABIERTA y no una vez en el arranque: cambiar el
 * ajuste no re-monta nada ni recarga la página (no hay DOM instalado que reconstruir, a
 * diferencia de los mandos Enhanced), así que vale para el mercader siguiente — y para
 * el de ahora mismo, en cuanto la fase cambie.
 */
export function shopUiActivo(search?: string): ShopUi {
  let s = search;
  if (s === undefined) {
    try {
      s = window.location.search;
    } catch {
      s = "";
    }
  }
  return shopUiFlag(s) ?? shopUiGuardado();
}

/** Atajo legible para los call-sites: ¿conduce el panel la fase de tecleo de tienda? */
export function panelDeTiendaActivo(search?: string): boolean {
  return shopUiActivo(search) === "modern";
}
