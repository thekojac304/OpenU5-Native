/**
 * ★★★ EL DESPACHO — y la promesa entera del carril cabe en una frase:
 *     **lo único que cambia es quién pulsa las teclas.**
 *
 * Tocar un NPC adyacente y visible NO llama a `game.talkTarget()`, ni a `startTalk()`, ni
 * a nada del motor. Sintetiza, en orden, las DOS MISMAS TECLAS que teclea quien juega con
 * teclado — `t` y luego la flecha — sobre el MISMO objetivo (`document.body`) y por el
 * MISMO camino (`press()` de `ui/touch.ts`).
 *
 * ── POR QUÉ ASÍ Y NO «LLAMANDO DIRECTAMENTE», que es la tentación obvia ───────────────
 * El comando (T)alk del port no es una función: es una MÁQUINA DE DOS TIEMPOS que vive en
 * el despachador de `main.ts`, y saltársela perdería, en silencio, todo esto:
 *   · el ECO de consola «Talk-» (`CMD_STRINGS.talk`, DS 0xa210) que el binario imprime al
 *     pulsar la tecla, ANTES de saber siquiera si hay alguien delante;
 *   · el estado `pendingDirCommand = "talk"` y con él el `getdir` VIVO — que es lo que la
 *     piel pinta como cursor de ola junto al comando (`setAwaitingDirection`), lo que
 *     hace que la cruceta del deck sirva para contestar, y lo que ESC cancela;
 *   · el prólogo de tecla del bucle de pueblo: la intercepción de BORRACHERA
 *     (`commandDrunkIntercept`, town_read_command 0x0DD0) rueda POR TECLA LEÍDA y puede
 *     SUSTITUIR el comando por un tumbo — con «Hic!» el Talk no llega a correr;
 *   · las ramas que `startTalk` atraviesa antes del script (tienda con su gate horario,
 *     guardia de palacio 0xFF, poseído 0xFD/0xFE, la guarda del mercader a caballo);
 *   · el GRABADOR de teclas (`keyRec.commit(turno)`), que es lo que hace que una partida
 *     se pueda REPRODUCIR. Una conversación abierta por un atajo que no pasa por el
 *     keydown sencillamente no existiría en la repetición, y la suma de control por turno
 *     divergiría en la siguiente tecla.
 * Ninguna de esas seis cosas se «acuerda» de replicarse: o se pulsa la tecla, o se pierden.
 *
 * 🔴 `document.body`, NUNCA `window` — y no es un detalle de estilo: es la ficha #218
 * entera, razonada en el docblock de `press()` (`ui/touch.ts`). Una tecla FÍSICA baja en
 * captura window→document→body antes de burbujear, y los modales de piel la ven ANTES que
 * el despachador del mundo; un evento despachado SOBRE window tiene path `[window]` y cada
 * motor ordena at-target distinto (medido: Chromium andaba con la party con Ztats
 * abierto). Por eso aquí se REUSA `press()` y no se escribe un `dispatchEvent` propio: un
 * segundo despachador es un segundo sitio donde ese defecto puede renacer.
 *
 * Las teclas viajan como DATO en `AccionContextual.teclas` (no se disparan al resolver)
 * para que el test pueda carear la secuencia exacta contra la que teclea una persona sin
 * tener que montar el juego entero.
 */
import { press } from "../../ui/touch.js";
import { snapAttackCell } from "../../ui/attack-snap.js";
import type { Direction } from "../../core/world/movement.js";
import { clasificar } from "./classify.js";
import { TECLA_DE_DIRECCION, direccionAdyacente } from "./targets.js";
import { vistaMundo, type FuentesDeMundo, type VistaMundo } from "./world.js";

/** La tecla del comando (T)alk en el despachador original (`kernel_cmd_dispatch`). */
export const TECLA_TALK = "t";

/**
 * Lo que un tap contextual resuelve. DOS formas, y la segunda es la mitad del arreglo
 * del 12-09 (reporte del usuario: «works on one npc and not another… trying it again just
 * says blocked like I'm trying to move there»).
 *
 * ★★ POR QUÉ EXISTE `ninguna` — el defecto que cerró, MEDIDO en vivo.
 * La primera versión, con un NPC VISIBLE pero NO adyacente, devolvía `null` y dejaba caer
 * el tap al camino de siempre. Y el camino de siempre, sobre una celda OCUPADA POR UNA
 * PERSONA, no puede terminar nunca: la auto-marcha apunta a esa celda, da el paso, y el
 * NPC lo bloquea. Traza de la sesión (party 26,9 · NPC visible en 25,7, a tres casillas):
 *
 *     tap {25,7}  tile 336 (visible)  npc true  →  declina  →  auto-marcha
 *     consola: «North» … «Blocked!»            →  el party NO se movió, y el turno SE GASTÓ
 *
 * Antes de este carril nadie tocaba a los NPC y el cabo no se veía. Con el tap contextual
 * la conducta que se ENSEÑA es «toca a quien quieras hablar», así que cada fallo de
 * alcance se cobra un turno y contesta con un mensaje que habla de andar. Peor aún, no es
 * estable: una conversación CONSUME TURNOS, los NPC pasean, y el mismo NPC con el que
 * acabas de hablar ya no está a un paso — de ahí el «no parece repetible» del reporte.
 *
 * Así que un NPC visible fuera de alcance CONSUME el tap y no emite NADA: sin Talk, sin
 * paso, sin turno, sin mensaje. Es la decisión del usuario (12-09) y deroga a conciencia
 * la regla «si no es objetivo legal, cae al camino de siempre» del encargo original: esa
 * regla daba por bueno un camino que sobre una celda con NPC SIEMPRE acaba en topetazo.
 *
 * 🔴 Y NO ES UNA PROMOCIÓN DE COMANDO: no se camina hacia él, no se le habla desde lejos,
 * no se infiere ruta. Tocar a alguien fuera de alcance no hace nada, que es exactamente
 * lo que pasa en el original cuando le das a (T)alk apuntando al vacío… salvo que aquí ni
 * siquiera se gasta el turno, porque no ha llegado a haber comando.
 */
export type AccionContextual =
  | {
      readonly clase: "talk";
      /** Rumbo con el que se contesta al `getdir` del comando. */
      readonly dir: Direction;
      /** Las teclas a sintetizar, EN ORDEN. Exactamente lo que teclea una persona. */
      readonly teclas: readonly string[];
    }
  | {
      /** NPC VISIBLE fuera de alcance: el tap se consume y NO se emite ninguna tecla. */
      readonly clase: "ninguna";
      readonly teclas: readonly [];
    };

/**
 * PUNTERÍA TÁCTIL — la celda a la que el tap DEBE apuntar. Es `snapAttackCell` de
 * `ui/attack-snap.ts`, la MISMA pieza que corrige el tap de COMBATE, no una copia: si
 * algún día se cambia la regla conservadora (0 o ≥2 candidatos ⇒ no se toca nada), se
 * cambia en un sitio. Su razón se mide igual aquí que allí — la casilla mide 19,5 px CSS
 * en un teléfono vertical, menos de la mitad del objetivo táctil mínimo, y media yema de
 * error cae en la vecina.
 *
 * ── §PUNTERÍA: DOS ACOTACIONES QUE EL CASO DE COMBATE NO NECESITA ─────────────────────
 *  1. **La población son SÓLO los NPC ADYACENTES Y VISIBLES** (`npcsAdyacentesVisibles`),
 *     nunca todos los del mapa. Un NPC al que no se puede hablar YA no puede robarle la
 *     puntería a nadie, así que el ajuste jamás convierte un paseo largo en un silencio.
 *  2. **Las cuatro ORTOGONALES del party NO se ajustan NUNCA.** Y esto sí es propio de
 *     aquí: en combate no se anda tocando el suelo, pero en el mundo esas cuatro celdas
 *     son EXACTAMENTE el destino de un paso — tocar al oeste para andar al oeste. Sin
 *     esta acotación, con un NPC al norte el toque al oeste queda a distancia de rey 1 de
 *     él y el ajuste se lo llevaría: andar rodeando a una persona se volvería imposible.
 *     Lo que sí se ajusta son las celdas que NADIE pisa de un paso —las DIAGONALES (el
 *     original no tiene movimiento diagonal: `docs/controls.md`) y el anillo de dos—, que
 *     es donde de verdad aterrizan los fallos de puntería.
 *  3. **Una celda que YA TIENE a alguien visible encima NO se ajusta.** Es la primera
 *     regla de `snapAttackCell` («la celda tocada TIENE enemigo → esa, sin tocar nada»),
 *     que aquí hay que aplicar A MANO porque su población son sólo los adyacentes y no
 *     sabe de los demás. 🔴 SIN ESTO EL AJUSTE SE EQUIVOCA DE PERSONA, y está MEDIDO en
 *     vivo (12-09, Minoc): party en (26,9), mendigo adyacente en (26,8) y otro NPC en
 *     (25,8) — un toque limpio sobre el de (25,8) quedaba a distancia de rey 1 del
 *     mendigo y abría la conversación del MENDIGO. Un fallo de puntería es tocar donde no
 *     hay nadie; tocar a alguien concreto nunca lo es.
 */
function ajustarPunteria(vista: VistaMundo, x: number, y: number): { x: number; y: number } {
  if (vista.esPasoDeUnaCasilla(x, y)) return { x, y }; // destino de un paso: intocable
  // Hay alguien VISIBLE justo ahí: el tap no es un fallo, es una elección. (Censura antes
  // que identidad, igual que en todo el módulo: `celdaVisible` manda.)
  if (vista.celdaVisible(x, y) && vista.hayNpc(x, y)) return { x, y };
  const radio = vista.tactil ? 1 : 0; // escritorio: el ratón es exacto, no se corrige
  return snapAttackCell({ x, y }, vista.npcsAdyacentesVisibles(), radio);
}

/**
 * ¿Qué acción contextual corresponde al tap en la celda de MAPA (x,y)? `null` = ninguna,
 * y entonces quien llama DEBE dejar correr el camino de siempre (auto-marcha A*).
 *
 * El orden de las puertas es el orden en que pueden decir que no, de la más barata y más
 * importante a la más específica:
 *   1. ¿estamos en el bucle del MUNDO? Si no (combate, mazmorra), este carril se aparta
 *      del todo — el `tap-tile` de ahí tiene otro dueño y NO se le toca nada;
 *   2. ¿manda otro flujo de input? Entonces el tap queda INERTE (ver abajo — es el
 *      arreglo del 12-09 y la causa raíz del «Blocked!» del reporte);
 *   3. corrección de puntería (táctil, conservadora, y sin robarle celdas al paso);
 *   4. ¿es un NPC VISIBLE? (censura primero, identidad después — ver `classify.ts`).
 *      Si NO lo es, se cae al camino de siempre: esa mitad no cambia;
 *   5. ¿está a un paso y en una de las cuatro vías? (ver `targets.ts`). Si lo está, Talk;
 *      si no, el tap se CONSUME sin emitir nada (ver `AccionContextual`).
 *
 * ══════════════════════════════════════════════════════════════════════════════════════
 * ★★★ PUERTA 2 — CON UN MODAL ABIERTO EL TAP NO HABLA **Y TAMPOCO ANDA**
 * ══════════════════════════════════════════════════════════════════════════════════════
 * Ésta es LA causa del reporte («almost every npc right next to me produces blocked and no
 * conversation»), y la primera versión la tenía a medias: declinaba (devolvía `null`) para
 * no sintetizar la `t`, que es correcto… y con eso dejaba caer el tap al camino de siempre.
 * Pero **la auto-marcha NO está gateada por nada**: `autoWalkCtl.walkTo()` llama a
 * `game.move()` DIRECTAMENTE, sin pasar por el despachador de teclas. O sea que con una
 * tienda o una conversación abierta el tap seguía ANDANDO.
 *
 * Traza medida (Britain, «Iolo's Bows», 12-09) — los dos taps son sobre el MISMO tendero:
 *
 *   tap 1  awaitingInput:true   shop:false  →  Talk  →  «Good morning, and welcome…»
 *   tap 2  awaitingInput:false  shop:true   →  declina  →  auto-marcha  →  «West» «Blocked!»
 *
 * El testigo del usuario es ese segundo renglón, palabra por palabra. Y el modal se queda
 * abierto hasta que se cierra a mano, así que a partir del PRIMER mercader o conversación
 * **todos** los taps siguientes andaban contra la pared: de ahí el «uno sí y los demás no».
 *
 * 🔴 La conducta correcta no es una invención: es la que YA tiene el teclado. Con un modal
 * abierto, la cruceta tampoco anda — sus flechas las consume `prompts.handleKey` antes de
 * llegar al bucle de mundo. Un tap tiene que ser exactamente igual de inerte que una tecla;
 * lo raro era que el dedo pudiera hacer lo que el mando no puede.
 *
 * Sólo se consume en modo MUNDO (puerta 1). En combate y mazmorra no se toca nada, para no
 * robarle el `tap-tile` al ataque ni a su bucle. Y el modo Clásico no pasa por aquí en
 * absoluto: sigue con su agujero de siempre, intacto, como pide el encargo.
 */
export function resolverTapContextual(
  vista: VistaMundo,
  x: number,
  y: number,
): AccionContextual | null {
  if (!vista.esModoMundo()) return null; // combate/mazmorra: el tap es de otro
  if (!vista.aceptaComando()) return { clase: "ninguna", teclas: [] }; // modal ⇒ inerte
  const objetivo = ajustarPunteria(vista, x, y);
  if (clasificar(vista, objetivo.x, objetivo.y) !== "npc") return null;
  const dir = direccionAdyacente(objetivo.x - vista.centro.x, objetivo.y - vista.centro.y);
  if (dir === null) return { clase: "ninguna", teclas: [] };
  return { clase: "talk", dir, teclas: [TECLA_TALK, TECLA_DE_DIRECCION[dir]] };
}

/**
 * Dispara la acción. `fire` es inyectable SÓLO para los tests; por defecto es el `press()`
 * del deck, que es la misma vía exacta que una tecla física (ver la cabecera).
 */
export function ejecutarAccionContextual(
  accion: AccionContextual,
  fire: (key: string) => void = press,
): void {
  for (const tecla of accion.teclas) fire(tecla);
}

/**
 * LA ENTRADA que el sink de intents cablea. Las dos piezas van como FUNCIONES y el orden
 * en que se piden es el contrato:
 *   · `enhanced()` primero, y es lo único que paga el modo Clásico — un
 *     `classList.contains` (`chapaEnhancedViva`);
 *   · `mundo()` PEREZOSO: construir las fuentes cuesta un `view.snapshot()`, y con la
 *     chapa apagada ese snapshot no se debe ni pedir.
 */
export interface EntradaTapContextual {
  /** ¿Está la chapa Enhanced VIVA ahora mismo? Con `false` el handler declina sin mirar nada. */
  enhanced(): boolean;
  /** Fuentes vivas del mundo visible. Sólo se invoca si `enhanced()` dijo que sí. */
  mundo(): FuentesDeMundo;
}

/**
 * ★ EL HANDLER COMPLETO, en una función que un test puede conducir entera.
 *
 * `true` = el tap se ha consumido como acción contextual (las teclas YA salieron).
 * `false` = **no se ha tocado nada**: quien llama tiene que seguir con su camino de
 * siempre (auto-marcha A*, ataque en combate…) exactamente como antes de que este carril
 * existiera. Ese «no se ha tocado nada» es literal y es la mitad del encargo: con la
 * chapa apagada ni siquiera se pide el snapshot.
 */
export function manejarTapContextual(
  entrada: EntradaTapContextual,
  x: number,
  y: number,
  fire: (key: string) => void = press,
): boolean {
  if (!entrada.enhanced()) return false;
  const accion = resolverTapContextual(vistaMundo(entrada.mundo()), x, y);
  if (!accion) return false;
  ejecutarAccionContextual(accion, fire);
  return true;
}
