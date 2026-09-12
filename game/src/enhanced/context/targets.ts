/**
 * ADYACENCIA Y RUMBO — la mitad PURA del tap contextual, y nada más.
 *
 * Lo único que hace este fichero es convertir un DESPLAZAMIENTO en celdas (la celda
 * tocada menos la celda del party) en la dirección que el comando direccional del
 * original aceptaría, o en `null`. Sin DOM, sin `Game`, sin snapshot: es la pieza que
 * puede probarse a mano y la que decide, ella sola, que un tap lejano NO es un objetivo.
 *
 * 🔴 CUATRO VÍAS, NUNCA CINCO. El `getdir` del kernel (0x35EC) y el `Direction` del motor
 * son de cuatro, y `docs/controls.md` lo declara («No diagonal movement (faithful to the
 * original)»). Una diagonal NO es «casi adyacente»: es una celda que el comando (T)alk no
 * puede apuntar, así que aquí devuelve `null` y el tap cae al camino de siempre. Escribir
 * aquí un radio de Chebyshev —o un «si está a un paso y medio, redondea»— inventaría un
 * objetivo que el jugador del original no tiene.
 *
 * 🔴 Y NUNCA UN CAMINO. Tampoco se persigue al NPC lejano: `|dx|+|dy| === 1` o nada. Un
 * auto-caminar-y-luego-hablar sería una SEGUNDA mecánica (pasos, turnos, encuentros por
 * el camino) disfrazada de comodidad de input, y este carril sólo cambia QUIÉN pulsa las
 * teclas. El tap lejano ya tiene dueño desde siempre: la auto-marcha A* de `ui/autowalk.ts`.
 *
 * El mapeo rumbo→tecla es el MISMO que `KEY_DIRECTIONS` de `main.ts` leído al revés (y el
 * mismo que las cuatro celdas de `enhanced/mobile/movement.ts`): la tecla sintética tiene
 * que ser la que produce una flecha física, o el despachador vería otra cosa.
 */
import type { Direction } from "../../core/world/movement.js";

/** Rumbo → tecla de flecha. Inversa exacta de `KEY_DIRECTIONS` (main.ts). */
export const TECLA_DE_DIRECCION: Readonly<Record<Direction, string>> = {
  north: "ArrowUp",
  south: "ArrowDown",
  east: "ArrowRight",
  west: "ArrowLeft",
};

/**
 * Rumbo del comando direccional para el desplazamiento `(dx,dy)` en celdas, o `null` si
 * esa celda NO es un objetivo inmediato legal: la propia celda del party (0,0), una
 * diagonal, o cualquier cosa a más de un paso.
 *
 * La convención de ejes es la del mapa (y crece hacia el SUR), la misma de
 * `DIRECTION_DELTA` en `core/world/movement.ts`.
 */
export function direccionAdyacente(dx: number, dy: number): Direction | null {
  if (!Number.isInteger(dx) || !Number.isInteger(dy)) return null;
  if (Math.abs(dx) + Math.abs(dy) !== 1) return null; // (0,0), diagonales y lejanas
  if (dx === 1) return "east";
  if (dx === -1) return "west";
  return dy === -1 ? "north" : "south";
}
