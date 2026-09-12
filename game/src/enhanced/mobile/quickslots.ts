/**
 * RANURAS DE LA FILA RÁPIDA — la preferencia del jugador, su validación y su persistencia.
 *
 * `groups.ts` decide los juegos POR DEFECTO (y razona cuáles y por qué); este módulo es la
 * única puerta por la que el jugador puede cambiarlos, y la única que toca `localStorage`.
 * Separados a propósito: la decisión de producto es pura y testeable sin almacenamiento, y
 * la preferencia no puede inventarse un comando que las tablas no tengan.
 *
 * ── SÓLO EL MUNDO ES PERSONALIZABLE, Y ES UNA DECISIÓN, NO UNA LIMITACIÓN ────────────
 * El encargo pide (§6) evaluar la personalización y NO construir un sistema de ajustes
 * desproporcionado. Con TRES juegos —mundo, mazmorra, arena— «cuatro ranuras» son doce
 * selectores en el panel de ajustes, que es exactamente el sistema desproporcionado que el
 * encargo excluye. Se personaliza el juego del MUNDO, que es donde transcurre la inmensa
 * mayoría del tiempo de juego y el único cuyo reparto es discutible (Get vs Search vs
 * Enter); mazmorra y arena conservan su juego curado, que está dictado casi entero por lo
 * que el binario ACEPTA allí — en la arena sólo hay trece comandos y cuatro de ellos son
 * los elegidos.
 *
 * 🔴 CERO LÓGICA DE JUEGO DUPLICADA. Las opciones que el ajuste ofrece son literalmente
 * `WORLD_BUTTONS` (el censo adjudicado contra `kernel_cmd_dispatch` 0x3178): ni un rótulo,
 * ni una tecla, ni un título se escriben aquí. Si mañana el censo gana un comando, el
 * selector lo ofrece sin tocar este fichero.
 *
 * ── TODO VALOR GUARDADO ES SOSPECHOSO ────────────────────────────────────────────────
 * Lo que vuelve de `localStorage` puede venir de otra versión del port, de una tecla que
 * se retiró del censo, o de un dedo en las devtools. `loadQuickWorld()` valida RANURA A
 * RANURA y cae al defecto de ESA ranura, no del juego entero: así una sola entrada podrida
 * no tira las otras tres que el jugador sí eligió. Y ningún comando queda inalcanzable por
 * una preferencia rota — el cajón sigue sirviendo los 25.
 */
import { WORLD_BUTTONS, type ButtonDef } from "../../ui/touch.js";
import { QUICK_DEFAULTS, QUICK_SLOTS, type DeckCtx } from "./groups.js";

/** Clave de preferencia. Mismo prefijo `u5.` que `u5.padSide` / `u5.enhancedPadPos`. */
export const QUICK_WORLD_KEY = "u5.quickWorld";

/** Los comandos que el selector de ajustes puede ofrecer: el censo del mundo, tal cual. */
export function quickOptions(): readonly ButtonDef[] {
  return WORLD_BUTTONS;
}

/** ¿Es `k` una tecla que el censo del mundo sirve de verdad? */
export function esTeclaDelMundo(k: unknown): k is string {
  return typeof k === "string" && WORLD_BUTTONS.some((d) => d.key === k);
}

/**
 * El juego vigente del MUNDO: lo guardado donde sea válido, el defecto donde no.
 *
 * Siempre devuelve exactamente `QUICK_SLOTS` teclas, para que el render no tenga que
 * defenderse de una fila corta — un array de longitud variable sería un `undefined` bajo
 * el pulgar en cuanto alguien guardara tres.
 */
export function loadQuickWorld(): string[] {
  const base = QUICK_DEFAULTS.world;
  let guardado: unknown = null;
  try {
    const raw = localStorage.getItem(QUICK_WORLD_KEY);
    if (raw) guardado = JSON.parse(raw);
  } catch {
    /* denegado o JSON corrupto: se juega con los defectos. */
  }
  const lista = Array.isArray(guardado) ? guardado : [];
  return Array.from({ length: QUICK_SLOTS }, (_, i) => {
    const v = lista[i];
    return esTeclaDelMundo(v) ? v : (base[i] ?? base[0]!);
  });
}

/** Persiste el juego del mundo. No-op silencioso si el almacenamiento está denegado. */
export function saveQuickWorld(keys: readonly string[]): void {
  try {
    localStorage.setItem(QUICK_WORLD_KEY, JSON.stringify([...keys]));
  } catch {
    /* la preferencia no persiste, pero la sesión funciona. */
  }
}

/** Borra la personalización: el mundo vuelve a su juego por defecto. */
export function resetQuickWorld(): void {
  try {
    localStorage.removeItem(QUICK_WORLD_KEY);
  } catch {
    /* idem. */
  }
  avisar();
}

/**
 * Las teclas de la fila para un contexto. Ésta es la función que el render llama, y la
 * ÚNICA costura por la que la personalización entra: mazmorra y arena la atraviesan sin
 * tocar almacenamiento.
 */
export function quickKeysFor(ctx: DeckCtx): readonly string[] {
  return ctx === "world" ? loadQuickWorld() : QUICK_DEFAULTS[ctx];
}

const oyentes = new Set<() => void>();
function avisar(): void {
  for (const fn of [...oyentes]) fn();
}

/**
 * Cambia UNA ranura y avisa. `slot` fuera de rango o tecla que el censo no trae = no-op:
 * la guarda vive aquí, en la única vía de escritura, y no repartida por los llamadores.
 */
export function setQuickSlot(slot: number, key: string): void {
  if (!Number.isInteger(slot) || slot < 0 || slot >= QUICK_SLOTS) return;
  if (!esTeclaDelMundo(key)) return;
  const keys = loadQuickWorld();
  if (keys[slot] === key) return;
  keys[slot] = key;
  saveQuickWorld(keys);
  avisar();
}

/** (Des)suscribe al cambio de ranuras. Devuelve la baja, como el resto de los `on*`. */
export function onQuickSlotsChange(fn: () => void): () => void {
  oyentes.add(fn);
  return () => {
    oyentes.delete(fn);
  };
}
