/**
 * CATÁLOGO DE HECHIZOS para la lista moderna — DERIVADO, nunca inventado.
 *
 * 🔴 NO ES UNA SEGUNDA TABLA DE HECHIZOS. Cada campo sale de una fuente que ya manda en el
 * juego, y este módulo sólo la re-empaqueta para pintarla:
 *   · `index` / `key` / `words` / `circle` / `reagents` / `targetType` / descripción
 *     → `SpellDef` (`core/magic/spells.ts`, construido de `data/MagicDefinitions.json`);
 *   · `initials` → las MISMAS iniciales que el jugador teclea en el getstring rúnico
 *     (`CAST2.OVL:0x00de`), o sea la primera letra de cada sílaba;
 *   · `mpCost` → `SpellDef.mpCost`, que `cast.ts` fija al CÍRCULO (CAST:0x0ef8);
 *   · `contexts` → `TIME_PERMITTED_BITS` (DS:0x1C90), la tabla que el dispatcher consulta
 *     en `CAST:0x0e1a` — NO la cadena `TimePermitted` del JSON, que es documentación de
 *     terceros y no el gate vivo. Donde las dos discrepan manda el binario;
 *   · nombres de reactivo → los pasa el llamador desde `data.json.reagents`
 *     (= name-table DATA.OVL DS 0x19D2, la que el jugador ve en Ztats).
 *
 * ── QUÉ **NO** HAY EN EL REPOSITORIO, y por qué esta lista no se lo inventa ───────────
 * En Ultima V el NOMBRE de un hechizo **son sus palabras de poder**: la name-table de
 * Ztats (DS 0x19E2) lista «In Mani Co», «Vas Flam»… y no existe en ninguna parte del
 * binario, de los assets ni de las notas de RE un rótulo legible tipo «Resurrect». Por eso
 * la lista enseña las PALABRAS como título y la descripción mecánica debajo; fabricar
 * nombres de fantasía sería contenido nuevo con apariencia de dato del original, que es
 * justo lo que este repositorio no hace.
 *
 * La descripción (`effect`) es el campo `SimpleDescription` de `MagicDefinitions.json`,
 * verbatim salvo la limpieza de abajo.
 *
 * ⚠ EL ASSET TRAE UN CAMPO MÁS QUE ESTA LISTA **NO** ENSEÑA, y conviene que conste por qué:
 * `SimilarFunction` («como una antorcha», «como la poción azul») son 25 valores distintos
 * que nombran OBJETOS del juego. Su vocabulario castellano vive en `es.json` (son cadenas
 * del binario) y no en la capa del shell, así que enseñarlos aquí sería o media traducción
 * —25 rótulos en inglés dentro de un panel en castellano— o un segundo vocabulario de
 * objetos que podría discrepar del que usa el inventario. Tampoco está en lo que el encargo
 * pide de cada fila. Se deja en el asset, sin leer.
 *
 * ⚠ DOS RAREZAS DEL ASSET, saneadas aquí y declaradas (el fichero NO se reescribe: es un
 * volcado de terceros y se deja como está):
 *   · **la RAYA sola significa «ninguna»**. Veinte de los 49 `SimilarFunction` valen
 *     exactamente «–» (U+2013) y uno vale «» — o sea, «este hechizo no se parece a nada».
 *     Pintar una raya suelta en la ficha sería pintar un dato que no existe, así que
 *     `sanea()` los convierte en ausencia.
 *   · **puntuación TIPOGRÁFICA**: «reveals caster’s location» trae U+2019, y el asset usa
 *     U+2013 como guion. Se normalizan a ASCII (`'` y `-`) por la lección de #248: los
 *     rótulos de esta capa acaban en tablas de traducción y, en la piel fiel, en un atlas
 *     que sólo cubre 0x00-0x7F — un carácter sin glifo no se pinta pero sí reserva ancho.
 *     Con todo en ASCII, las claves de `i18n/shell.ts` también son tecleables.
 */
import { MAX_TRACKED_SPELL_INDEX, type SpellDef } from "../../core/magic/spells.js";
import { TIME_PERMITTED_BITS } from "../../core/magic/tables.js";

/** Los cuatro contextos del gate de ventana temporal (DS:0x1C90, bits 0x08/0x04/0x02/0x01). */
export interface SpellContexts {
  /** Exterior — `g_location == 0` (bit 0x08). */
  outdoor: boolean;
  /** Pueblo/castillo — `1 ≤ g_location ≤ 0x20` (bit 0x04). */
  town: boolean;
  /** Mazmorra — `0x21 ≤ g_location ≤ 0x7F` (bit 0x02). */
  dungeon: boolean;
  /** Arena de combate — `g_location ≥ 0x80` (bit 0x01). */
  combat: boolean;
}

/** Una fila del catálogo. Todo DERIVADO: ver el docblock del módulo para cada fuente. */
export interface SpellEntry {
  /** Índice canónico 0..47 = posición en `GameState.spellQuantities` y en la jump table. */
  index: number;
  /** Clave del JSON de magia ("In_Mani_Corp"). */
  key: string;
  /** Palabras de poder canónicas, tal cual las nombra el juego ("In Mani Corp"). */
  words: string;
  /** Las sílabas sueltas, por si la vista quiere maquetarlas ("In","Mani","Corp"). */
  syllables: readonly string[];
  /** INICIALES que se teclean en el getstring rúnico ("IMC"). Es lo que la lista «escribe». */
  initials: string;
  /** Círculo 1..8. Es a la vez el coste de maná y el nivel mínimo del lanzador. */
  circle: number;
  /** Coste de maná (= círculo, CAST:0x0ef8). Se expone aparte por legibilidad de la vista. */
  mpCost: number;
  /** Nivel mínimo del lanzador (= círculo, gate CAST:0x0f01). */
  minLevel: number;
  /** Ids de reactivo 0..7 (SulfurAsh..MandrakeRoot) exigidos por la receta. */
  reagents: readonly number[];
  /** Los mismos, con el nombre que el jugador ve en Ztats. */
  reagentNames: readonly string[];
  /** Familia del JSON ("attack" | "peace" | "support" | "debuff"). */
  type: string;
  /** Modo de apuntado declarado por el JSON ("noSelection", "direction", …). */
  targetType: string;
  /** Dónde permite lanzarlo el dispatcher (tabla del binario, no el JSON). */
  contexts: SpellContexts;
  /** Descripción mecánica corta (`SimpleDescription`, saneada). */
  effect: string;
}

/**
 * Normaliza un campo de texto del asset (ver docblock). Devuelve `null` cuando el valor
 * es «ninguno»: vacío, o SÓLO signos de guion (que es como el asset lo codifica).
 */
export function sanea(raw: string | undefined | null): string | null {
  if (!raw) return null;
  const s = raw
    .replace(/[‘’]/g, "'") // comillas simples tipográficas → apóstrofo ASCII
    .replace(/[“”]/g, '"') // comillas dobles tipográficas → comilla ASCII
    .replace(/[–—]/g, "-") // raya y semirraya → guion ASCII
    .trim();
  // «-», «--», «- -»… = el asset diciendo «nada». Sin letras ni dígitos no hay dato.
  if (!/[\p{L}\p{N}]/u.test(s)) return null;
  return s;
}

/** Desmenuza los 4 bits de `TIME_PERMITTED_BITS` en el registro legible. */
export function contextsOf(index: number): SpellContexts {
  const bits = TIME_PERMITTED_BITS[index] ?? 0;
  return {
    outdoor: (bits & 0x08) !== 0,
    town: (bits & 0x04) !== 0,
    dungeon: (bits & 0x02) !== 0,
    combat: (bits & 0x01) !== 0,
  };
}

/**
 * INICIALES de un hechizo — la primera letra de cada sílaba, en MAYÚSCULAS y EN ORDEN.
 *
 * Es literalmente lo que el jugador pulsa: `matchSpellByInitials` ORDENA antes de comparar
 * (CAST2 0x01e2-0x02fc, contra la tabla DS:0x1c30 que está pre-ordenada), así que el orden
 * da igual para el emparejamiento — pero se conserva el de las palabras porque es lo que
 * se ecoa en la consola («:IN MANI CORP») y lo que el jugador reconoce.
 */
export function initialsOf(def: SpellDef): string {
  return def.syllables.map((s) => s[0]!.toUpperCase()).join("");
}

/**
 * Construye el catálogo: los 48 hechizos LANZABLES, en orden canónico.
 *
 * 🔴 NOX QUEDA FUERA, y no es una omisión de diseño: es el índice 48, no tiene slot en
 * `spellQuantities` (48 slots, 0..47 — `MAX_TRACKED_SPELL_INDEX`), no está en la tabla de
 * emparejamiento DS:0x1c30 y su ventana temporal es «never». O sea, el original no lo deja
 * teclear; una lista que lo ofreciera estaría ofreciendo algo que no se puede lanzar.
 *
 * Tampoco se filtra por «mezclados»: el original deja teclear CUALQUIER hechizo y el gate
 * «None mixed!» (CAST:0x0ebb) lo aplica el dispatcher DESPUÉS. La lista enseña los 48 por
 * la misma razón, y la cantidad viva la pinta la vista aparte.
 */
export function buildSpellCatalog(
  defs: readonly SpellDef[],
  reagentNames: readonly string[],
): SpellEntry[] {
  const out: SpellEntry[] = [];
  for (const def of defs) {
    if (def.index > MAX_TRACKED_SPELL_INDEX) continue; // Nox: incastable, ver arriba
    out.push({
      index: def.index,
      key: def.key,
      words: def.name,
      syllables: [...def.syllables],
      initials: initialsOf(def),
      circle: def.circle,
      mpCost: def.mpCost,
      minLevel: def.circle,
      reagents: [...def.reagents],
      reagentNames: def.reagents.map((r) => reagentNames[r] ?? `Reagent ${r}`),
      type: def.type,
      targetType: def.targetType,
      contexts: contextsOf(def.index),
      effect: sanea(def.description) ?? "",
    });
  }
  return out;
}

/** Los ocho círculos, en orden. Es la agrupación que usa la lista (ver `groupByCircle`). */
export const CIRCLES: readonly number[] = [1, 2, 3, 4, 5, 6, 7, 8];

/**
 * Agrupa por CÍRCULO — la jerarquía real del sistema de magia de Ultima V, no una
 * taxonomía inventada: el círculo ES el coste de maná (CAST:0x0ef8), ES el nivel mínimo
 * del lanzador (CAST:0x0f01) y es el orden en que el propio juego lista los hechizos en
 * Ztats. Las alternativas (alfabética, «por función») habrían necesitado un criterio nuevo
 * que el original no tiene.
 *
 * Dentro de cada círculo se conserva el ORDEN CANÓNICO (índice de `SpellWords`), que es el
 * de la name-table de Ztats: quien reconoce la lista de su inventario reconoce ésta.
 */
export function groupByCircle(
  entries: readonly SpellEntry[],
): { circle: number; spells: SpellEntry[] }[] {
  return CIRCLES.map((circle) => ({
    circle,
    spells: entries.filter((e) => e.circle === circle),
  })).filter((g) => g.spells.length > 0);
}

/**
 * MODO DE APUNTADO → rótulo legible (base inglesa; la traduce `ts()` en la vista).
 *
 * Es una TRADUCCIÓN del enum que ya trae el asset (`SpellTargetType`), no una
 * clasificación nueva: cada clave de la izquierda aparece literal en
 * `MagicDefinitions.json`. Las dos variantes de caja (`SelectedMapPosition` /
 * `selectedMapPosition`) también están las dos en el fichero — el asset es inconsistente
 * ahí y normalizar por `toLowerCase()` es más barato que mantener dos filas.
 *
 * ⚠ ESTO ES INFORMATIVO Y NO CONDUCE NADA. Quien pide (y cuenta) el objetivo sigue siendo
 * el flujo del original (`pickCastTarget`, el `getdir` del kernel, el cursor de apuntado
 * del combate): la lista no apunta, no pre-selecciona y no salta ningún prompt.
 */
const TARGET_LABELS: Readonly<Record<string, string>> = {
  noselection: "no target",
  direction: "a direction",
  selectedcombatplayer: "a party member",
  castingcombatplayer: "the caster",
  selectedmapunit: "a creature or object",
  selectedmapposition: "a map square",
  selectedcombatmapposition: "a combat square",
};

/** Rótulo del modo de apuntado, o `null` si el asset trae un valor que no está en la tabla. */
export function targetLabel(targetType: string): string | null {
  return TARGET_LABELS[targetType.trim().toLowerCase()] ?? null;
}

/**
 * ¿Casa la fila con el texto tecleado en el buscador? Compara contra las palabras, las
 * iniciales y la descripción — sin mayúsculas, y sin espacios en las iniciales (teclear
 * «imc» encuentra In Mani Corp).
 */
export function matchesQuery(entry: SpellEntry, query: string): boolean {
  const q = query.trim().toLowerCase();
  if (q === "") return true;
  const plano = q.replace(/\s+/g, "");
  return (
    entry.words.toLowerCase().includes(q) ||
    entry.words.toLowerCase().replace(/\s+/g, "").includes(plano) ||
    entry.initials.toLowerCase().startsWith(plano) ||
    entry.effect.toLowerCase().includes(q)
  );
}
