/**
 * FACHADA DE SÓLO LECTURA del mundo VISIBLE — la única puerta por la que el tap
 * contextual se entera de algo.
 *
 * ══════════════════════════════════════════════════════════════════════════════════════
 * ★★ POR QUÉ LA VISIBILIDAD SE LEE DE LA VENTANA CENSURADA Y NO DEL ESTADO CRUDO
 * ══════════════════════════════════════════════════════════════════════════════════════
 * `npcManager.npcsAt()` devuelve TODOS los NPC de la planta, estén donde estén: detrás de
 * un muro, al otro lado del pueblo, o en plena noche sin antorcha. Es el estado CRUDO, y
 * el port ya tiene una regla dura para eso (interview.md #2): la censura se aplica en el
 * CORE y la piel «JAMÁS recibe el mapa crudo de noche o tras un muro»
 * (`skin/api.ts`, `TILE_HIDDEN`).
 *
 * Un tap contextual que preguntara sólo al estado crudo sería un FILTRADO: tocar a
 * oscuras la celda donde el juego sabe que hay un tabernero abriría una conversación que
 * el jugador de 1988 no puede iniciar porque no ve a nadie ahí. Y el filtrado no necesita
 * llegar a hablar para existir — bastaría con que el tap se comportara DISTINTO (que no
 * caminase, que destellara otra cosa) para convertir el tap en un detector de personas en
 * la oscuridad. Por eso el orden de esta fachada es **visibilidad primero, identidad
 * después**: `hayNpc()` sólo tiene sentido sobre una celda que `celdaVisible()` ya aprobó,
 * y `clasificar()` (classify.ts) lo respeta.
 *
 * La fuente de verdad es `ViewSnapshot.window`: la ventana 11×11 YA COMPUESTA y YA
 * CENSURADA que las pieles pintan — el mismo búfer, no una re-derivación. Ahí una celda
 * no visible vale `TILE_HIDDEN` y una fuera de mapa `TILE_OFFMAP` (ambas se pintan a
 * negro), y los actores sólo se hornean sobre celdas con `field[idx] === 1`
 * (`skin/coreview.ts`, bucle de entidades). O sea: si el NPC no está en la ventana, el
 * jugador no lo ve, y aquí tampoco.
 *
 * 🔴 NO SE RE-CALCULA LA MÁSCARA. `visField()` es privada de `CoreView` a propósito, y
 * recomponerla aquí (radio de luz + LOS por muros + el bypass de revelado de #319/#326)
 * sería una SEGUNDA implementación de la censura, capaz de divergir de la que se pinta.
 * Lo que se lee es lo que se ve.
 *
 * ══════════════════════════════════════════════════════════════════════════════════════
 * LA PUERTA DE INPUT: SE REUSA, NO SE INVENTA
 * ══════════════════════════════════════════════════════════════════════════════════════
 * `aceptaComando` NO trae una lista propia de modales. El port ya tiene el predicado y ya
 * lo publica: `isModalOpen()` (`ui/awaiting-gate.ts`) alimenta `setAwaitingInput()`, y
 * eso es exactamente `snapshot().awaitingInput` — prompt Y/N, getstring, numérico,
 * rúnico, party-select, selector de Cast/Ready, consola de diálogo, tienda, acampada,
 * sueño, refugio y los pacers. Dos listas de modales para la misma pregunta es la
 * divergencia que ese fichero vino a cerrar; aquí se le cree a él.
 *
 * A esa señal se le suman DOS conjunciones, y las dos son por lo que un `press("t")`
 * haría en ese instante:
 *   · `awaitingDirection` — hay un `getdir` VIVO (pendingDirCommand/Cast/skull key).
 *     Con él, la 't' NO abre un Talk: la rama `if (pendingDirCommand)` de `main.ts` la
 *     lee como «no es dirección» y CANCELA el comando en curso («Cancelled.»). Sintetizar
 *     ahí sería destruir el comando que el jugador acaba de empezar.
 *   · `mode === "world"` — en combate el `tap-tile` YA tiene dueño (el ataque de
 *     `playerAttack`, con su corrección de puntería), y en mazmorra el bucle es otro
 *     (`handleDungeonKey`). Este carril no toca ninguno de los dos.
 * Las superficies DOM del shell (panel de guardado, selector) viajan aparte en
 * `shellAbierto`: capturan el teclado ANTES del bucle (`keyRec.drop()`) y no entran en
 * `isModalOpen`, así que el sink las consulta con el predicado que ya existe allí
 * (`shellSurfaceOpen()`).
 */
import { TILE_HIDDEN, TILE_OFFMAP, VIEW_HALF, VIEW_WINDOW } from "../../skin/api.js";
import { DIRECTION_DELTA, type Direction } from "../../core/world/movement.js";

/**
 * Lo MÍNIMO del snapshot que esta fachada necesita. Estructural (no `ViewSnapshot`
 * entero) para que un test pueda construirlo a mano sin montar un `CoreView`: lo que se
 * quiere probar es la regla, no el adaptador.
 */
export interface VentanaVisible {
  /** Centro de la ventana 11×11 en coords de MAPA (la party). */
  readonly center: { readonly x: number; readonly y: number };
  /** Los 121 tiles ya compuestos y ya censurados. Ver `ViewSnapshot.window`. */
  readonly window: { readonly length: number; readonly [i: number]: number };
}

/** El mundo tal y como el tap contextual puede mirarlo: visible, y nada más. */
export interface VistaMundo {
  /** Celda del party en coords de mapa (centro de la ventana). */
  readonly centro: { readonly x: number; readonly y: number };
  /**
   * ¿La celda (x,y) de MAPA es visible AHORA para quien juega? Falso también si cae
   * fuera de la ventana 11×11, fuera del mapa, o censurada por luz/muro.
   */
  celdaVisible(x: number, y: number): boolean;
  /**
   * ¿Hay un NPC del mapa vivo en (x,y)? 🔴 Es estado CRUDO: sólo puede preguntarse
   * DESPUÉS de que `celdaVisible` haya dicho que sí (ver la cabecera).
   */
  hayNpc(x: number, y: number): boolean;
  /**
   * ¿Estamos en el bucle del MUNDO? En combate el `tap-tile` tiene otro dueño (el ataque,
   * con su propia corrección de puntería) y en mazmorra el bucle es `handleDungeonKey`:
   * en los dos, este carril se aparta del todo y NO consume nada.
   */
  esModoMundo(): boolean;
  /** ¿Puede una tecla de COMANDO entrar ahora mismo en el bucle del mundo? */
  aceptaComando(): boolean;
  /**
   * Las (como mucho) CUATRO celdas ortogonales al party que son VISIBLES y tienen un NPC
   * — o sea, los objetivos LEGALES de un (T)alk inmediato, ya censurados.
   *
   * Es la población de la corrección de puntería (`actions.ts`), y va acotada a esas
   * cuatro A PROPÓSITO: un candidato que no sea objetivo legal no puede ganarse un tap
   * ajustándole la puntería a nadie.
   */
  npcsAdyacentesVisibles(): readonly { readonly x: number; readonly y: number }[];
  /**
   * ¿Es (x,y) un destino de UN PASO desde el party (una de las cuatro ortogonales)?
   *
   * 🔴 Lo consulta la corrección de puntería para NO robarle celdas al tap-para-andar:
   * ver el porqué en `actions.ts` §puntería.
   */
  esPasoDeUnaCasilla(x: number, y: number): boolean;
  /** ¿Régimen TÁCTIL? Fija el radio de la corrección de puntería (0 en escritorio). */
  readonly tactil: boolean;
}

/** Lo que el sink de intents inyecta para construir la fachada. */
export interface FuentesDeMundo {
  /** El snapshot YA censurado (`view.snapshot()`). */
  readonly ventana: VentanaVisible;
  /** Modo del snapshot: sólo `"world"` tiene tap contextual. */
  readonly modo: "world" | "dungeon" | "combat";
  /** `snapshot().awaitingInput` — el gate de modales que el port ya publica. */
  readonly esperandoInput: boolean;
  /** `snapshot().awaitingDirection` — hay un `getdir` vivo. */
  readonly esperandoDireccion: boolean;
  /** ¿Hay una superficie DOM del shell capturando el teclado? (`shellSurfaceOpen()`) */
  readonly shellAbierto: boolean;
  /** `npcManager.npcAt(loc, floor, x, y) != null` para el mapa VIVO. */
  npcEn(x: number, y: number): boolean;
  /**
   * ¿Régimen táctil? (`html.u5-touch`). El MISMO predicado con el que el tap de COMBATE
   * elige el radio de `snapAttackCell` (`main.ts`): en escritorio el ratón es exacto y no
   * se corrige nada.
   */
  readonly tactil: boolean;
}

/** Índice en la ventana 11×11 de una celda de MAPA, o `-1` si cae fuera. */
function indiceDeCelda(ventana: VentanaVisible, x: number, y: number): number {
  const col = x - (ventana.center.x - VIEW_HALF);
  const row = y - (ventana.center.y - VIEW_HALF);
  if (col < 0 || row < 0 || col >= VIEW_WINDOW || row >= VIEW_WINDOW) return -1;
  return row * VIEW_WINDOW + col;
}

/** Los cuatro rumbos, en el orden de la tabla del motor. Sin diagonales (el original no las tiene). */
const RUMBOS: readonly Direction[] = ["north", "south", "east", "west"];

/** Construye la fachada de sólo lectura a partir de las fuentes vivas. */
export function vistaMundo(f: FuentesDeMundo): VistaMundo {
  const cx = f.ventana.center.x;
  const cy = f.ventana.center.y;
  const vista: VistaMundo = {
    centro: { x: cx, y: cy },
    tactil: f.tactil,
    celdaVisible(x, y) {
      const idx = indiceDeCelda(f.ventana, x, y);
      if (idx < 0) return false;
      const tile = f.ventana.window[idx];
      // Los DOS centinelas negros del contrato de `window`. Se comprueban los dos y no
      // «tile < 0»: son constantes exportadas con significado propio y una tercera
      // futura no debe colarse como visible por ser negativa.
      return tile !== TILE_HIDDEN && tile !== TILE_OFFMAP;
    },
    hayNpc(x, y) {
      return f.npcEn(x, y);
    },
    esModoMundo() {
      return f.modo === "world";
    },
    aceptaComando() {
      return f.modo === "world" && f.esperandoInput && !f.esperandoDireccion && !f.shellAbierto;
    },
    npcsAdyacentesVisibles() {
      const out: { x: number; y: number }[] = [];
      for (const dir of RUMBOS) {
        const { dx, dy } = DIRECTION_DELTA[dir];
        const x = cx + dx;
        const y = cy + dy;
        // 🔴 VISIBILIDAD ANTES QUE IDENTIDAD, igual que en `clasificar` — si no, la
        // corrección de puntería sería una vía lateral para consultar el estado crudo
        // sobre celdas censuradas, que es justo lo que la cabecera prohíbe.
        if (!vista.celdaVisible(x, y)) continue;
        if (!f.npcEn(x, y)) continue;
        out.push({ x, y });
      }
      return out;
    },
    esPasoDeUnaCasilla(x, y) {
      return Math.abs(x - cx) + Math.abs(y - cy) === 1;
    },
  };
  return vista;
}
