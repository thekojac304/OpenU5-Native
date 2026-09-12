/**
 * AGRUPACIÓN DE COMANDOS del cajón Enhanced — LA ÚNICA FUENTE DE VERDAD, y pura.
 *
 * 🔴 NO HAY UN SEGUNDO INVENTARIO DE COMANDOS. Los rótulos, las teclas y los títulos
 * siguen saliendo de `WORLD_BUTTONS` / `DUNGEON_BUTTONS` / `COMBAT_BUTTONS`
 * (`ui/touch.ts`), que son los CENSOS adjudicados contra el despachador del binario
 * (`kernel_cmd_dispatch` 0x3178 · `COMBAT.OVL` 0x0838 · el gate por localización de la
 * mazmorra). Este módulo sólo dice, para una TECLA, a qué cajón visual pertenece —
 * y lo hace con un único mapa `tecla → grupo`.
 *
 * POR QUÉ POR TECLA Y NO POR RÓTULO. El rótulo pasa por `ts()` y cambia con el idioma
 * («Search» → «Buscar»); la tecla es el byte que el binario lee y no cambia jamás. Un
 * mapa por rótulo se habría roto el día que alguien re-adjudicara un vocablo ES —
 * exactamente lo que `deck-i18n-coherencia.test.ts` existe para permitir.
 *
 * POR QUÉ SE CONSERVA EL ORDEN DE LA TABLA DENTRO DE CADA GRUPO. La regla escrita en
 * `WORLD_BUTTONS` («se AÑADEN al final, sin reordenar lo que el usuario ya tiene bajo
 * el pulgar») sigue vigente: agrupar re-parte los comandos en cajones, pero DENTRO de
 * cada cajón el orden es el de la tabla, así que un comando nuevo sigue apareciendo
 * detrás de sus compañeros de grupo y nada de lo anterior se mueve. El agrupado es
 * ESTÁTICO: no depende del estado del mundo, no reordena por «utilidad prevista» y no
 * esconde nada — las tres cosas que la auditoría descartó por confusas.
 */
import {
  WORLD_BUTTONS,
  DUNGEON_BUTTONS,
  COMBAT_BUTTONS,
  type ButtonDef,
} from "../../ui/touch.js";

/** Contexto del deck. Espejo del que publica `ui/touch.ts` en su `refresh()`. */
export type DeckCtx = "world" | "dungeon" | "combat";

/** Los cinco cajones. El orden de esta tupla ES el orden de pintado. */
export const GROUP_IDS = ["interaction", "items", "travel", "party", "system"] as const;
export type GroupId = (typeof GROUP_IDS)[number];

/** Rótulo BASE INGLÉS de cada cajón (lo traduce `ts()` en el render, como todo el cromo). */
export const GROUP_LABELS: Record<GroupId, string> = {
  interaction: "Interaction",
  items: "Items & Magic",
  travel: "Travel",
  party: "Party",
  system: "System",
};

/**
 * TECLA → CAJÓN. Cubre la UNIÓN de las tres tablas (27 teclas distintas), y la guarda
 * `enhanced-groups.test.ts` exige que la cobertura sea TOTAL: una tecla nueva en
 * cualquiera de los tres censos hace fallar el test hasta que se le asigne cajón. Ésa es
 * la razón de que el mapa viva aquí y no disperso por el renderizador.
 *
 * Criterio, comando a comando:
 *   · interaction — lo que se hace SOBRE una casilla o un ser: los siete direccionales
 *     del `getdir` del kernel (Talk/Open/Look/Get/Search/Jimmy/Push), el ataque, la
 *     fuente de la mazmorra ('d', QoL del port ya declarado en DUNGEON_BUTTONS) y el
 *     Pass — pasar turno es la acción de «no hacer nada AQUÍ», no un asunto del grupo.
 *   · items     — inventario y magia: Cast, Mix, Ready, Use y la antorcha.
 *   · travel    — cambiar de sitio o de vehículo: Board, Enter, X-it, Klimb, y los dos
 *     del barco (Yell iza/arría vela, Fire dispara la andanada).
 *   · party     — el grupo y su información: Ztats, New order, Hole up, View gem.
 *   · system    — cromo del port y salida: Save (F5, tecla de SHELL, no del binario) y
 *     Quit & Save (la 'q' del flujo fiel por consola).
 */
export const GROUP_OF_KEY: Readonly<Record<string, GroupId>> = {
  // interaction
  t: "interaction", // Talk
  o: "interaction", // Open
  l: "interaction", // Look
  g: "interaction", // Get
  s: "interaction", // Search
  j: "interaction", // Jimmy
  p: "interaction", // Push
  a: "interaction", // Attack
  d: "interaction", // Drink (QoL del port, sólo mazmorra)
  " ": "interaction", // Pass (sólo la arena lo lleva como comando con rótulo)
  // items & magic
  c: "items", // Cast
  m: "items", // Mix
  r: "items", // Ready
  u: "items", // Use
  i: "items", // Ignite torch
  // travel
  b: "travel", // Board
  e: "travel", // Enter
  x: "travel", // X-it
  k: "travel", // Klimb
  y: "travel", // Yell
  f: "travel", // Fire
  // party
  z: "party", // Ztats
  n: "party", // New order
  h: "party", // Hole up
  v: "party", // View gem
  // system
  F5: "system", // Save (tecla de shell del port)
  q: "system", // Quit & Save
};

/**
 * ── LAS ACCIONES RÁPIDAS: los verbos que no deberían costar un cajón ─────────────────
 *
 * Encargo del usuario (13-09, refinado el 12-09 tras probar en un teléfono real): «figuring
 * out what the most common commands are (like open) and having them directly accessible».
 * La pregunta es cuáles, y la respuesta NO sale de la intuición.
 *
 * 🔴 LO QUE **NO** VALE COMO PRUEBA, y se declara para que nadie lo cite como si valiera.
 * Conté las pulsaciones de comando del corpus del Grand Tour (67 ficheros, 218
 * pulsaciones) y da este orden: Cast 20 · Attack 19 · Enter 18 · Yell 17 · Talk 14 ·
 * New order 11 · Xit 11 · Open 11 · Get 10 · Board 10 · Klimb 8 · Use 8 · Mix 8 ·
 * Search 7 · Quit 7 · Look 6 · Hole up 6 · Ztats 6 · Push 5 · Torch 4 · Ready 4 ·
 * Fire 3 · View gem 3 · Jimmy 2. **Ese corpus mide COBERTURA, no juego**: es una partida
 * automatizada cuyo objetivo declarado es tocar TODO el contenido una vez, así que
 * «Yell» sale por arriba por las palabras de poder y las velas, no porque nadie grite
 * cada dos pasos. Usarlo como ranking de uso sería confundir el instrumento con el sujeto
 * — el mismo error que este repo ya documentó con los `$rageclick` de la cruceta.
 *
 * ★ EL CRITERIO QUE SÍ SE SOSTIENE es ESTRUCTURAL, y sale del binario: los comandos que
 * se pulsan constantemente son los que actúan sobre la CASILLA DE AL LADO, o sea los que
 * pasan por el `getdir` del kernel (0x35EC) — Talk, Open, Look, Get, Search, Jimmy, Push
 * y Attack. Son el «interactúa con lo que tengo delante» de un pueblo o un pasillo, que
 * es lo que se hace cada pocos pasos. Todo lo demás es EPISÓDICO: Cast/Mix/Ready/Use se
 * usan por ráfagas, Board/Xit/Yell/Fire son de viaje o de barco, Hole up/New order/View
 * gem/Quit/Save son de sesión.
 *
 * ── POR QUÉ CUATRO, Y POR QUÉ UN JUEGO POR CONTEXTO ──────────────────────────────────
 * CUATRO y no seis: la fila dejó de ser una banda de ancho completo para caber AL LADO de
 * la cruceta (`css.ts` §4b), o sea una rejilla 2×2 en el hueco que los layouts izquierda y
 * derecha desperdiciaban. Seis celdas ahí serían 3×2 con celdas por debajo del suelo
 * táctil de 44 px en un teléfono de 375.
 *
 * 🔴 UN JUEGO POR CONTEXTO NO ES «REORDENADO DINÁMICO» — eso está prohibido por el encargo
 * y no se hace. La fila NO se recompone con el estado del mundo: se elige UNA vez por
 * contexto, y los contextos son los TRES que el deck ya publica (`world`/`dungeon`/
 * `combat`, `onDeckContext`). Sin esto la fila no podía funcionar: un juego único centrado
 * en el mundo (Open/Talk/Look/Search) deja la fila ENTERA vacía en la arena, porque
 * `COMBAT_BUTTONS` no trae ninguno de los cuatro — el censo los tacharía todos y el
 * jugador se quedaría con cuatro huecos justo donde más prisa hay.
 *
 * Comando a comando:
 *   · MUNDO — Open · Talk · Look · Search.
 *       Open   es el verbo de toda puerta, cofre y arcón: el ejemplo que el propio usuario
 *              citó, y el que costaba TRES toques (Commands → Interaction → Open).
 *       Talk   es la mitad del juego: en Ultima V la trama entera se saca por conversación.
 *       Look   sube aquí DESDE la fila de sistema (§8 del encargo): es el único verbo que
 *              NO gasta turno, y es el que se pulsa sin tener aún un objetivo — leer un
 *              cartel, identificar un bulto. Al subir, su celda de la barra se retira: una
 *              sola vía, cero duplicados.
 *       Search es lo que destapa lo escondido (pasadizos, trampas, alijos) y no tiene
 *              ninguna otra pista visual que lo recuerde.
 *       Get NO entra, y es la exclusión que más cuesta: va casi siempre DETRÁS de un Open
 *              o un Search (el cofre ya abierto, el alijo ya destapado), así que su coste
 *              real es un viaje al cajón por botín, no por paso.
 *       Enter TAMPOCO, pese a su 18 del tour: sólo vale ESTANDO sobre el tile de un pueblo
 *              o una mazmorra, o sea unas pocas veces por sesión y siempre con el mapa
 *              diciéndotelo. Es el contraejemplo exacto de «útil sin contexto raro».
 *   · MAZMORRA — Open · Search · Look · Klimb.
 *       Talk y Enter los RECHAZA el overlay allí, así que ni se plantean. Entra `Klimb`,
 *       que es la única vía entre niveles (escaleras arriba/abajo) y vivía enterrada en el
 *       cajón «Travel»; sale Talk. Search se mantiene: los pasadizos secretos son el pan
 *       de la mazmorra.
 *   · ARENA — Attack · Cast · Use · Get.
 *       Attack y Cast son literalmente el turno de combate. `Use` es la poción de curación
 *       —la urgencia del combate—, y `Get` el botín del suelo, que en la arena SÍ hay que
 *       recoger antes de salir. `Pass` NO entra aunque sea el verbo más pulsado de un
 *       combate: ya tiene celda PERSISTENTE en la fila de sistema (la tecla Espacio), y
 *       duplicarlo es justo lo que §8 del encargo manda quitar.
 *
 * 🔴 ESTÁTICA, y filtrada SÓLO por el censo del contexto. No se reordena por lo que haya
 * alrededor, y no se esconde nada por «utilidad prevista»: una celda desaparece únicamente
 * cuando el BINARIO rechaza ese comando ahí — el mismo gate de la ficha #71.
 */

/** Celdas de la fila rápida. Es también el número de ranuras personalizables. */
export const QUICK_SLOTS = 4;

/** El juego por defecto de cada contexto. Los personaliza (sólo el mundo) `quickslots.ts`. */
export const QUICK_DEFAULTS: Readonly<Record<DeckCtx, readonly string[]>> = {
  world: ["o", "t", "l", "s"],
  dungeon: ["o", "s", "l", "k"],
  combat: ["a", "c", "u", "g"],
};

/** La tabla CENSADA del contexto. Es la misma referencia que usa el deck clásico. */
export function tableFor(ctx: DeckCtx): readonly ButtonDef[] {
  return ctx === "combat" ? COMBAT_BUTTONS : ctx === "dungeon" ? DUNGEON_BUTTONS : WORLD_BUTTONS;
}

/** Un cajón ya resuelto para un contexto. `defs` conserva el orden de la tabla. */
export interface Group {
  id: GroupId;
  label: string;
  defs: ButtonDef[];
}

/**
 * Reparte la tabla del contexto en cajones. PURA y total: cada `ButtonDef` de la tabla
 * cae en exactamente un grupo, los grupos vacíos se omiten, y la concatenación de los
 * `defs` es una PERMUTACIÓN de la tabla — ni un comando de menos (lo asevera
 * `enhanced-comandos-completos.test.ts`).
 *
 * Una tecla sin cajón asignado NO se pierde: cae a `system`, que es el cajón de «cromo y
 * salida» y el sitio menos dañino para un recién llegado. El test es quien avisa de que
 * hay que adjudicarle su sitio; el producto, mientras tanto, no esconde el comando.
 */
/**
 * Los `ButtonDef` de la fila rápida para un contexto, en el orden de `keys` y filtrados por
 * el CENSO: si la tabla del contexto no trae esa tecla, la celda no existe.
 * PURA. Devuelve `null` en las posiciones sin comando para que el consumidor pueda dejar
 * el hueco en vez de recomponer la fila (memoria muscular).
 *
 * `keys` se pasa DESDE FUERA —`quickslots.ts` lo resuelve, porque ahí vive la preferencia
 * del jugador— y este módulo se queda puro: la misma separación de siempre entre la
 * decisión y el render.
 */
export function quickFor(ctx: DeckCtx, keys: readonly string[]): (ButtonDef | null)[] {
  const tabla = tableFor(ctx);
  return keys.map((k) => tabla.find((d) => d.key === k) ?? null);
}

export function groupsFor(ctx: DeckCtx): Group[] {
  const buckets = new Map<GroupId, ButtonDef[]>();
  for (const def of tableFor(ctx)) {
    const id = GROUP_OF_KEY[def.key] ?? "system";
    const list = buckets.get(id);
    if (list) list.push(def);
    else buckets.set(id, [def]);
  }
  return GROUP_IDS.filter((id) => buckets.has(id)).map((id) => ({
    id,
    label: GROUP_LABELS[id],
    defs: buckets.get(id)!,
  }));
}
