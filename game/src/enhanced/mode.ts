/**
 * MODO «ENHANCED CONTROLS» — el interruptor, y nada más.
 *
 * POR QUÉ EXISTE (auditoría de mandos móviles, fase 0). El deck clásico
 * (`ui/touch.ts` + los layouts de `skin/portrait/deck-ancho.ts`) enseña la LISTA
 * ENTERA de comandos del contexto: 25 en el mundo, 18 en mazmorra, 13 en la arena.
 * Medido por el propio repo, eso deja «4 de 25 visibles sin scroll» en el layout
 * partido y se come el 58 % del alto de la pantalla. La chapa Enhanced sustituye esa
 * pared por una fila persistente corta + un cajón desplegable con TODOS los comandos.
 *
 * 🔴 ESTE MÓDULO NO SABE NADA DE DOM NI DE LAYOUT. Es el predicado y su persistencia,
 * igual que `ui/regimen-tactil.ts` es el predicado del régimen táctil y nada más. Lo
 * consumen tres sitios: el arranque (`main.ts`), la piel del portrait (para NO instalar
 * el CSS del deck clásico) y la fila del drawer del shell.
 *
 * TRES REGLAS, y las tres son del encargo:
 *   1. SÓLO EN TÁCTIL. En escritorio no hay chapa móvil que ofrecer, así que
 *      `enhancedControlsDisponible()` delega en la primitiva viva de `regimen-tactil`
 *      — la MISMA que decide si el deck se pinta. Dos predicados distintos para «¿hay
 *      móvil?» es exactamente la divergencia que #334 vino a cerrar.
 *   2. APAGADO POR DEFECTO. La identidad del port es la fidelidad; una chapa nueva se
 *      gana el default con kilómetros, no con el commit que la estrena.
 *   3. LA URL MANDA SOBRE LO GUARDADO, como en `?touch=1`, `?deck=` y `?reflow=`. Es
 *      lo que hace posible un proyecto de Playwright por modo sin tocar localStorage.
 */
import { esTactilAhora } from "../ui/regimen-tactil.js";

/** Clave de preferencia. Mismo prefijo `u5.` que `u5.padSide` / `u5.skin`. */
export const ENHANCED_KEY = "u5.enhancedControls";

/**
 * Clase de raíz que ACTIVA todo el CSS de la chapa Enhanced.
 *
 * 🔴 SCOPE AISLADO A PROPÓSITO. El CSS de la chapa (`enhanced/mobile/css.ts`) cuelga
 * ENTERO de `html.u5-enhanced.u5-touch`: sin la clase no casa ni una regla, y con ella
 * el layout clásico ni siquiera se instala (ver el gate del arranque). No se comparte
 * cascada con `skin/portrait/deck-ancho.ts` — ese fichero tiene cuatro
 * `display:grid !important` con interdependencias medidas y un historial de inversión
 * documentado (su docblock de `TOUCH_CLASS`), y la forma barata de no pelearse con él
 * es no coincidir nunca.
 */
export const ENHANCED_CLASS = "u5-enhanced";

/**
 * Bandera de URL: `?enhanced=1` enciende, `?enhanced=0` apaga, ausente = `null` (manda
 * lo guardado). PURA (recibe el `search`) para poder probarla sin `window`.
 */
export function enhancedFlag(search: string): boolean | null {
  try {
    const v = new URLSearchParams(search).get("enhanced");
    if (v === null) return null;
    return v !== "0" && v !== "off" && v !== "false";
  } catch {
    return null;
  }
}

/** Preferencia PERSISTIDA (sin mirar la URL ni el régimen). Default `false`. */
export function enhancedControlsGuardado(): boolean {
  try {
    return localStorage.getItem(ENHANCED_KEY) === "1";
  } catch {
    // Almacenamiento denegado (modo privado): la preferencia no persiste, sin romper.
    return false;
  }
}

/** Guarda la preferencia. No aplica nada: quien conmuta decide cómo re-montar. */
export function guardarEnhancedControls(on: boolean): void {
  try {
    localStorage.setItem(ENHANCED_KEY, on ? "1" : "0");
  } catch {
    /* sin almacenamiento: la sesión vale igual, la preferencia no sobrevive. */
  }
}

/** ¿Tiene sentido OFRECER la chapa? Sólo en régimen táctil vivo. */
export function enhancedControlsDisponible(): boolean {
  return esTactilAhora();
}

/**
 * ¿Está la chapa Enhanced ACTIVA ahora? URL primero, preferencia después, y SIEMPRE
 * conjugado con el régimen táctil: una preferencia guardada en el móvil no puede
 * encender una chapa móvil en un escritorio (mismo cierre que #333 pieza A1 le puso al
 * layout partido).
 *
 * `search` es inyectable para los tests; por defecto lee la URL viva.
 */
export function enhancedControlsActivo(search?: string): boolean {
  if (!enhancedControlsDisponible()) return false;
  let s = search;
  if (s === undefined) {
    try {
      s = window.location.search;
    } catch {
      s = "";
    }
  }
  const url = enhancedFlag(s);
  return url ?? enhancedControlsGuardado();
}

/**
 * ¿Está la chapa MONTADA ahora mismo? — se pregunta al DOM, no a la preferencia.
 *
 * 🔴 NO ES UN SINÓNIMO DE `enhancedControlsActivo()`, y la diferencia importa en los dos
 * sentidos. Aquélla contesta «¿debería estarlo?» leyendo la URL y `localStorage`; ésta
 * contesta «¿lo está?» mirando la clase que `installEnhancedCss()` pone y
 * `uninstallEnhancedCss()` quita — o sea, la MISMA señal de la que cuelga todo el CSS de
 * la chapa. Entre el arranque y `aplicarChapaMovil()`, o durante una conmutación en
 * caliente, las dos pueden discrepar durante un instante, y quien pinta encima del deck
 * tiene que creerle al deck.
 *
 * Y es BARATA, que es la otra razón: la consulta `syncTouchExpect` en la cola de CADA
 * keydown, y `enhancedControlsActivo()` construye un `URLSearchParams` y toca
 * `localStorage` en cada llamada. Un `classList.contains` no hace ni lo uno ni lo otro.
 */
export function chapaEnhancedViva(): boolean {
  if (typeof document === "undefined") return false;
  return document.documentElement.classList.contains(ENHANCED_CLASS);
}

/** Publica (o retira) la clase de raíz. Idempotente; no-op sin `document`. */
export function aplicarClaseEnhanced(on: boolean): void {
  if (typeof document === "undefined") return;
  document.documentElement.classList.toggle(ENHANCED_CLASS, on);
}
