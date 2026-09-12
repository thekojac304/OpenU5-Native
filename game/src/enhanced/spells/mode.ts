/**
 * INTERFAZ DE LANZAMIENTO (Casting interface) — el interruptor, y nada más.
 *
 * DOS RÉGIMENES, UNA SOLA MECÁNICA:
 *   · `classic` — el original entero: (C)ast → «Spell name:» → el jugador TECLEA la
 *     INICIAL de cada sílaba rúnica (`CAST2.OVL:0x00de`, ver `ui/pickers.ts`).
 *   · `modern`  — el MISMO flujo, pero la fase de TECLEO la conduce una lista de
 *     hechizos: se elige uno y el panel escribe sus iniciales POR EL MISMO camino de
 *     teclado que usaría el jugador. Ni una regla de magia cambia de sitio.
 *
 * 🔴 ESTE MÓDULO NO SABE NADA DE DOM, DE HECHIZOS NI DE REGLAS. Es el predicado y su
 * persistencia — el mismo reparto que `enhanced/mode.ts` (mandos) y `ui/regimen-tactil.ts`.
 *
 * TRES REGLAS, heredadas del interruptor hermano (`enhanced/mode.ts`) salvo la primera:
 *   1. NO ES SÓLO TÁCTIL, y es la única diferencia deliberada con los mandos Enhanced:
 *      teclear cuatro iniciales rúnicas de memoria cuesta lo mismo en un escritorio que
 *      en un teléfono, así que el ajuste se ofrece en las dos superficies.
 *   2. CLÁSICO POR DEFECTO. La identidad del port es la fidelidad; una capa nueva se gana
 *      el default con kilómetros, no con el commit que la estrena.
 *   3. LA URL MANDA SOBRE LO GUARDADO (`?casting=modern` / `?casting=classic`), como
 *      `?touch=1`, `?deck=`, `?reflow=` y `?enhanced=` — es lo que permite un proyecto de
 *      Playwright por régimen sin tocar `localStorage`.
 */

/** Los dos regímenes. El valor persistido es EXACTAMENTE una de estas dos cadenas. */
export type CastingUi = "classic" | "modern";

/** Clave de preferencia. Mismo prefijo `u5.` que `u5.padSide` / `u5.enhancedControls`. */
export const CASTING_UI_KEY = "u5.castingUI";

/** Régimen por defecto: el original, sin capas. */
export const CASTING_UI_DEFAULT: CastingUi = "classic";

/** Normaliza cualquier cadena al régimen que representa; `null` si no representa ninguno. */
function normaliza(v: string | null | undefined): CastingUi | null {
  if (v === null || v === undefined) return null;
  const s = v.trim().toLowerCase();
  if (s === "modern" || s === "moderno" || s === "list" || s === "1") return "modern";
  if (s === "classic" || s === "clasico" || s === "clásico" || s === "0") return "classic";
  return null;
}

/**
 * Bandera de URL: `?casting=modern` / `?casting=classic`. Ausente (o valor que no
 * nombra un régimen) = `null` ⇒ manda lo guardado. PURA (recibe el `search`) para poder
 * probarla sin `window`, igual que `enhancedFlag`.
 */
export function castingUiFlag(search: string): CastingUi | null {
  try {
    return normaliza(new URLSearchParams(search).get("casting"));
  } catch {
    return null;
  }
}

/** Preferencia PERSISTIDA (sin mirar la URL). Default `classic`. */
export function castingUiGuardado(): CastingUi {
  try {
    return normaliza(localStorage.getItem(CASTING_UI_KEY)) ?? CASTING_UI_DEFAULT;
  } catch {
    // Almacenamiento denegado (modo privado): la preferencia no persiste, sin romper.
    return CASTING_UI_DEFAULT;
  }
}

/** Guarda la preferencia. No aplica nada: el régimen se lee en cada (C)ast. */
export function guardarCastingUi(ui: CastingUi): void {
  try {
    localStorage.setItem(CASTING_UI_KEY, ui);
  } catch {
    /* sin almacenamiento: la sesión vale igual, la preferencia no sobrevive. */
  }
}

/**
 * Régimen VIVO: URL primero, preferencia después. `search` es inyectable para los tests;
 * por defecto lee la URL viva.
 *
 * Se consulta EN CADA (C)ast y no una vez en el arranque: cambiar el ajuste no re-monta
 * nada ni recarga la página (a diferencia de los mandos Enhanced, que sí re-montan la
 * chapa táctil), porque aquí no hay DOM instalado que reconstruir.
 */
export function castingUiActivo(search?: string): CastingUi {
  let s = search;
  if (s === undefined) {
    try {
      s = window.location.search;
    } catch {
      s = "";
    }
  }
  return castingUiFlag(s) ?? castingUiGuardado();
}

/** Atajo legible para los call-sites: ¿conduce la lista la fase de tecleo? */
export function listaDeHechizosActiva(search?: string): boolean {
  return castingUiActivo(search) === "modern";
}
