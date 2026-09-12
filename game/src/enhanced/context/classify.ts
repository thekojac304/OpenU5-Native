/**
 * CLASIFICACIÓN DE LA CELDA TOCADA — pura, y DELIBERADAMENTE POBRE.
 *
 * Tres clases y ninguna más:
 *   · `oculto` — la celda no es visible para quien juega (fuera de la ventana, fuera del
 *     mapa, a oscuras o tras un muro). Es su PROPIA clase y no un `otro` cualquiera para
 *     que quien lea el código vea que la censura se decide ANTES que la identidad — y
 *     para que el test de filtrado tenga un sujeto que nombrar.
 *   · `npc`   — celda visible con un NPC del mapa vivo encima.
 *   · `otro`  — celda visible sin NPC: suelo, puerta, cofre, escalera, enemigo, barco…
 *
 * 🔴 POR QUÉ NO ESTÁN YA LAS DEMÁS CLASES (puerta→Open, enemigo→Attack, objeto→Get,
 * cofre→Search, escalera→Klimb, vehículo→Board). Porque este pase es una REBANADA
 * VERTICAL para probar la costura, y cada una de esas clases trae su propio pleito que
 * no se resuelve escribiendo una entrada en un enum:
 *   · «puerta» son varios tiles con estados (cerrada, atrancada, mágica) y (O)pen compite
 *     con (J)immy y con el Cast de An Sanct;
 *   · «enemigo» en el sobremundo dispara un ENCUENTRO, no un golpe, y el tap sobre
 *     enemigo en combate YA tiene dueño (`playerAttack` + su corrección de puntería);
 *   · «objeto» compite con la capa de botín y con el Get de mesa;
 *   · «escalera» tiene dos sentidos (arriba/abajo) que Klimb resuelve por tile.
 * Añadir el enum entero hoy y dejarlo sin cablear crearía la ilusión de un sistema
 * completo justo donde no lo hay. La clase se añade CON su comando y con sus tests.
 *
 * El orden visibilidad→identidad no es estético: es la regla anti-filtrado de
 * `world.ts`. Si se invirtiera, `hayNpc()` (estado CRUDO) se consultaría sobre celdas
 * que el jugador no ve.
 */
import type { VistaMundo } from "./world.js";

/** Las tres clases de este pase. NO es el universo futuro de objetivos. */
export type ClaseObjetivo = "npc" | "oculto" | "otro";

/**
 * Clase de la celda de MAPA (x,y) según lo que el jugador puede ver ahora.
 * `oculto` gana siempre: una celda censurada se clasifica sin preguntar por su contenido.
 */
export function clasificar(vista: VistaMundo, x: number, y: number): ClaseObjetivo {
  if (!vista.celdaVisible(x, y)) return "oculto";
  return vista.hayNpc(x, y) ? "npc" : "otro";
}
