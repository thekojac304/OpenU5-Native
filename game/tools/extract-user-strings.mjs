/**
 * F1.12 — extractor ÚNICO de strings user-facing del core (compartido por
 * `tests/string-manifest.test.ts` y `tools/gen-string-manifest.mjs`, para que la
 * guarda y el regenerador nunca se desincronicen).
 *
 * SINKS que captura (con resolución de referencias LOCALES del módulo):
 *   - propiedad `text:` (GameEvent) y `message:` (resultados de comando).
 *   - `<arr>.push(<expr>)` donde `<arr>` es un builder de mensajes
 *     (`messages`/`lines`/`events`/`segments`/`out`/`log`/`warnings`).
 *   - array-literal inicializador de una var con ese mismo nombre-builder
 *     (`const messages = [ ... ]`).
 * RESOLUCIÓN (≥1 nivel de indirección, SÓLO dentro del módulo): un valor que sea
 *   Identifier → `const NAME = <literal|template>`; ElementAccess `NAME[i]` → todos
 *   los elementos del `const NAME = [<literales>]`; concatenación `a + b` / template →
 *   se resuelve cada operando; ternario `cond ? A : B` → AMBAS ramas (cada una
 *   resuelta recursivamente; la condición no). Así `text: X`, `text: ARR[i]`,
 *   `lines.push(HEADER + NAME[i])`, `hud.message(cond ? "X!" : "Y!")` afloran sus
 *   literales. (Cierra la evasión por const que el review de F1.12 destapó —p.ej.
 *   ritual.ts— y la evasión por ternario que destapó el carril i18n F0b.)
 *
 * FUERA DE ALCANCE (limitación declarada — no lo sigue): indirección CROSS-módulo
 *   (una const importada de otro fichero), construcción DINÁMICA (strings formados en
 *   runtime por concatenación con variables no-const, `.map`/`.join`, plantillas con
 *   sub-expresiones no literales) y builders con nombre de var fuera de la lista.
 *   Un string así se declara manualmente en el manifiesto (el barrido inverso lo cubre).
 *   ⇒ #331: la mitad CROSS-módulo de esta limitación ya no es un punto ciego mudo — el
 *   censo `censusImportedSinkConsts` (abajo) deriva del código todo identificador
 *   IMPORTADO que llega a un sink y `tests/imported-consts-censo.test.ts` exige que
 *   cada uno esté clasificado (default-DENY). El extractor sigue sin RESOLVER el valor
 *   (el manifiesto no lo cataloga); lo que se cierra es que llegue INVISIBLE.
 *
 * Los templates se normalizan reemplazando cada `${…}` por `{}`.
 */
import ts from "typescript";
import { readFileSync, readdirSync } from "node:fs";
import { join, dirname } from "node:path";

const MSG_VAR = /^(messages?|lines|events|segments|out|log|warnings?)$/;

/**
 * Constantes-display del core cuyos VALORES string son user-facing pero que el
 * barrido de sinks (text:/message:/push) no alcanza por ser literales de OBJETO/
 * ARRAY (i18n F0b, hueco a). Allowlist ESTRECHA y citada — NO un barrido general de
 * objetos (metería basura interna). Cada una va comentada con su cita al binario:
 *   DIR_NAMES    (core/game.ts)          → DATA.OVL DS 0x29db/0x2676 "North\0South\0…"
 *   DIR_WORDS    (core/world/cmd-strings)→ DATA.OVL DS 0xa2a6/a2ae/a2bc/a2b6 (getdir eco)
 *   WIND_NAMES   (core/world/wind.ts)    → banda de vientos "Calm/North/South/East/West"
 *   WOUND_LABELS (core/combat/combat.ts) → COMBAT:0x1A5C (1a84-1afd) "critical!"/…
 *   FLAME_BY_LOCATION (core/game.ts)     → DS 0x4821+ FLAME_NAME "Truth/Love/Courage"
 *                                          (sufijo de "the Flame of " en (L)ook, F1.10)
 *   FURNITURE_SEARCH_PROSE (core/world/search.ts) → prosa del (S)earch por mueble,
 *                                          SJOG 0x0a6a-0x0ae8 + DS 0x8950-0x8a38 (C6)
 *   CAST_TARGET_UI (core/world/cmd-strings) → «On who: » DS 0x94f4 / «None!» DS 0x94fe
 *                                          (CAST2 0x9e, target-select de cast curativo)
 *   SHOP_UI      (core/world/cmd-strings) → prompts de mercader, DS-citados en el fichero
 *                                          (0x7F48/0x78D4/0x80AA/0x5066/…); accedido por
 *                                          `.prop` cross-módulo, que el resolve() no sigue →
 *                                          escapaba a §A. Ver docs/i18n/censo-blindspot-
 *                                          property-access.md.
 *   --- FAMILIA (A) del blind spot PropertyAccess (carril guard-hardening, lote 1). Todos
 *       VERBATIM en DATA.OVL (verificado byte a byte; ver approved-strings + cmd-strings.ts): ---
 *   CMD_STRINGS  (core/world/cmd-strings) → ecos de comando del dispatcher (DS 0xa134..0xa396)
 *   READY_UI     (core/world/cmd-strings) → UI de (R)eady (ZSTATS, DS 0x96b4/0x9998/0x9970/…)
 *   USE_UI       (core/world/cmd-strings) → UI de (U)se (CAST, DS 0x48b1/0x9976/0x489f)
 *   MIX_UI       (core/world/cmd-strings) → UI de (M)ix (CMDS cmd_mix 0x1AD8, DS 0x8f64..0x9004)
 *   TALK_UI      (core/world/cmd-strings) → prompts de conversación (PHRASES_CONVERSATION 0x0c/0x11)
 *   COMBAT_STRINGS (core/world/cmd-strings)→ "Aim! " (fileoff 39566) / "Set active plr:" (DS 0x6e66)
 *   --- LABELS (B) del blind spot (carril guard-hardening, lote 2). Todos VERBATIM en DATA.OVL: ---
 *   CLASS_LABELS (core/party.ts)         → clases Ztats "Mage/Bard/Fighter/…" (exact-line en DATA.OVL)
 *   DUNGEON_BY_X (core/game.ts)          → nombres de mazmorra "Shame/Destard/Deceit/…" (proper nouns)
 *   LOOK_DESC / FIELD_DESC (core/dungeon/dungeon.ts) → descripciones del (L)ook 3D,
 *                                          DNGLOOK tablas DS 0x7618-0x76f0 / 0x754c-0x7598
 *   TRAP_NAMES   (core/world/commands.ts)→ "ACID/POISON/BOMB/GAS" (+tf "{}!" = DATA.OVL "ACID!"…)
 *   LOOT_OPEN_NAMES (core/world/commands.ts)→ nombres del dispatcher 0x12A al abrir cofre
 *                                          ("a weapon!"/"a gem!"/…, DATA.OVL 0x850E-0x85F4);
 *                                          accedido por `TBL[id]` en lootOpenLine → el sink
 *                                          `text:` no lo aflora. Fuga cazada por soak b15.
 *   SHARD_MSG_NAME (core/quest/items.ts) → "Falsehood!/Hatred!/Cowardice!" (DS 0x8D18/24/2E)
 *   LEVELUP_STAT_WORDS (core/quest/lordbritish.ts) → "stronger!/quicker!/wiser!" (exact-line)
 *   SHRINE_ATTR_LABELS (core/game.ts, ex-"LABEL") → "Strength/Dexterity/Intelligence" (Ztats stats)
 *   STATUS_LABELS (core/party.ts)        → estado Ztats "Good Health/Poisoned/Dead/Asleep/Charmed"
 *                                          (DS 0x1a6a; alineado al binario, 8ª caza — antes G:"Good"/
 *                                          S:"Sleeping" divergían)
 *   --- ARRAYS estáticos de clase (carril i18n-prompts). VERBATIM DATA.OVL/KARMA.DAT. Se
 *       emiten por `events.push({text: ARR[i]})` (variable → el sink text: no los aflora) → el
 *       choke t() bajo 'es' los dejaba en inglés (hallazgo soak-2026-07-19). Son PropertyDeclaration
 *       (`private static readonly`), no VariableDeclaration → rama dedicada abajo: ---
 *   REFUGE_NARRATION (core/game.ts)      → narración del refuge (party_refuge 0x0910, DS 0x70e2+)
 *   BLACKSMITH_SELL_* (core/shops/shoppe-greetings.ts) → charla del sell-flow del
 *                                          herrero (tablas DS 0x3d2e/0x3d3e/0x3d36,
 *                                          carril sell-chatter); accedidos por `ARR[rand]`
 *   BLACKSMITH_BUY_* (core/shops/shoppe-greetings.ts) → charla del buy-flow del
 *                                          herrero (tablas DS 0x3d4a/0x3d52/0x3cb6/
 *                                          0x3ca6/0x3cae, carril buy-herrero); mismo
 *                                          patrón `ARR[rand]`
 *                                          cross-módulo desde ui/shop-console.ts
 *   USE_ITEM_NAMES (core/usePicker.ts)   → nombres de fila del picker de (U)se
 *                                          (name-table ZSTATS 0x1916, tabla extendida
 *                                          0xB9EE); emitidos por el sink `name:` de las
 *                                          filas, que el barrido NO ve → los 3 Shards
 *                                          salían EN INGLÉS bajo ES (auditoría G2,
 *                                          mismo patrón que la fuga LOOT_OPEN_NAMES)
 *   --- 🔴 RETIRADOS EL 25-08 (FICHA β), y NO por limpieza: ORDAINED_PAGES,
 *       REFUGE_KARMA_MESSAGES, CAMP_KARMA_MESSAGES y BLCKTHRN_MISCMSG ya NO contienen
 *       literales. Su texto (KARMA/MISCMSG.DAT) sale hoy de `game/assets/ds-strings.json`
 *       vía `dsRec()`, así que no hay literal que aflorar ni que aprobar: el allowlist
 *       sobraba y `display-consts-censo.test.ts` lo dijo por sus nombres en cuanto las
 *       tablas dejaron de existir. La cobertura i18n de esas frases NO se pierde — pasa a
 *       la superficie `ds-strings.json` del corpus (game/tools/i18n-corpus.mjs). ---
 *   LOOT_GRANT_STRINGS (core/world/commands.ts) → fijos de lootItemName que sólo
 *                                          viven en un `return` del switch (id14
 *                                          sandalwood 0x14F0; carril i18n-huecos)
 */
/**
 * 🔴 AVISO A QUIEN LIMPIE EXPORTS: ESTA LISTA BLANCA ES EL ÚNICO LECTOR DE VARIAS DE ESTAS
 * TABLAS, Y LAS CONSUME **POR SU NOMBRE**, NO IMPORTÁNDOLAS.
 *
 * Para cualquier herramienta que razone sobre el grafo de imports —un linter de «unused
 * export», un barrido de símbolos sin lector, o una persona con `grep`— esas tablas parecen
 * MUERTAS. No lo están: el consumo ocurre aquí, comparando el identificador contra esta
 * lista. Es invisible al análisis de imports **por construcción**, igual que un near-call
 * resuelto por banda da cero en un grep del texto del disasm.
 *
 * MEDIDO (2026-08-06, ficha del consumidor-por-nombre): de 513 `export const` en MAYÚSCULAS
 * de game/src, un barrido que enumeraba src + tests + e2e daba 8 «sin lector». Cuatro de
 * esos ocho —`CLASS_LABELS`, `STATUS_LABELS`, `HUD_FAITHFUL_LABELS` y `WIND_BAND_LABELS`—
 * los lee ESTA lista. Borrarlas o renombrarlas rompe la extracción de cadenas de i18n **en
 * silencio**, y el fallo aparece lejos del cambio.
 *
 * ⇒ Desde #276 el desajuste en AMBOS sentidos lo caza un TEST, no este aviso:
 * `tests/display-consts-censo.test.ts` enrojece si (a) aparece una constante-display
 * candidata que no está NI aquí NI en `EXCLUDED_DISPLAY_CONSTS` NI cubierta por otra vía
 * de extracción, o (b) un nombre de esta lista deja de existir en el código (renombre/
 * borrado). El aviso queda como explicación; la guarda es el test.
 */
export const DISPLAY_CONST_NAMES = [
  "DIR_NAMES", "DIR_WORDS", "WIND_NAMES", "WIND_BAND_LABELS", "WOUND_LABELS",
  "HUD_FAITHFUL_LABELS", "FLAME_BY_LOCATION", "SHADOWLORD_DOMAIN", "FLAME_NAME", "SHOP_UI",
  "CMD_STRINGS", "READY_UI", "USE_UI", "TALK_UI", "COMBAT_STRINGS", "CLASS_LABELS",
  "DUNGEON_BY_X", "TRAP_NAMES", "LOOT_OPEN_NAMES", "SHARD_MSG_NAME", "LEVELUP_STAT_WORDS",
  "SHRINE_ATTR_LABELS", "STATUS_LABELS",
  "MISC_ECHO_STRINGS", "REFUGE_NARRATION", "BLACKSMITH_SELL_PROMPTS",
  "BLACKSMITH_SELL_MORE", "BLACKSMITH_SELL_BYES", "BLACKSMITH_BUY_EXCLAIMS",
  "BLACKSMITH_BUY_INTROS", "BLACKSMITH_BUY_WHICH", "BLACKSMITH_BUY_ASKS",
  "BLACKSMITH_BUY_BROKE", "FURNITURE_SEARCH_PROSE", "CAST_TARGET_UI", "MIX_UI", "SHRINE_UI",
  "WELL_UI", "BLACKTHORN_UI", "LOOK_DESC", "FIELD_DESC", "USE_ITEM_NAMES",
  "LOOT_GRANT_STRINGS",
];
const DISPLAY_CONSTS = new RegExp(`^(${DISPLAY_CONST_NAMES.join("|")})$`);

/**
 * #331 (cura de las [deuda] del censo de importadas) — consts de MENSAJE PLANO (string
 * o concatenación de literales) cuyo valor viaja por IMPORT hasta un sink de OTRO
 * módulo. Ninguna vía las cubría: `objArrayStrings` (DISPLAY_CONSTS) sólo baja a
 * objeto/array, y el `resolve()` del fichero del sink no cruza módulos (limitación
 * declarada). Esta allowlist hace aflorar sus literales EN LA DECLARACIÓN (misma
 * población: todas viven en core/), con lo que:
 *   - sus valores exigen entrada con cita en approved-strings.json (string-manifest), y
 *   - la const queda EXENTA MECÁNICAMENTE en el censo #331 (resolved + strings ⊆ live),
 *     y el trinquete de imported-consts-censo obliga a retirar su [deuda] del ledger.
 * Anti-rancia (mismo régimen bidireccional, sin aserto nuevo): renombrar/borrar la
 * declaración deja huérfanas sus entradas del manifiesto → string-manifest ROJA
 * (aserto «mirror exacto» — verificado: los valores sólo existen en estas declaraciones;
 * aquí NO va un recuento, que caduca con cada alta y nadie carea).
 * Concatenación: las PIEZAS afloran por separado (mismo criterio que el censo #331);
 * SHOP_CLOSED_MESSAGE parte EXACTO en la frontera de las dos cadenas del binario
 * (DS 0x9196 / DS 0x91c4, contiguas en DATA.OVL).
 */
export const STRING_CONST_NAMES = [
  "EARTHQUAKE_MESSAGE", "WHIRLPOOL_MESSAGE", "MIRROR_BROKEN_MSG",
  "SHOP_CLOSED_MESSAGE", "COMBAT_ABSORBED_MESSAGE", "GATE_TRAVEL_PROMPT",
  // 22-08 (carril cast-completo): «Disabled!» = DS 0xa3ce, el rechazo de
  // `resolve_command_char` (ULTIMA.EXE 0x4a4e) que reengancha el prompt. Viaja por
  // import desde core/world/cmd-strings.ts hasta `hud.messageAppend` en ui/pickers.ts,
  // que no es sink extraído ⇒ el censo #331 la destapó SIN clasificar. Curada por la
  // vía recetada (aflora en su declaración), no exentada.
  "COMMAND_CHAR_DISABLED",
];
const STRING_CONSTS = new RegExp(`^(${STRING_CONST_NAMES.join("|")})$`);

/**
 * #276 — LEDGER DE EXCLUSIONES del censo de constantes-display (la otra mitad del
 * default-DENY). Todo candidato estructural (ver `censusDisplayCandidates`) que NO esté
 * en `DISPLAY_CONST_NAMES` y cuyos strings NO afloren ya por otra vía de extracción debe
 * tener aquí una entrada RAZONADA, o `tests/display-consts-censo.test.ts` enrojece
 * nombrándolo. La clave es el NOMBRE del identificador (como en DISPLAY_CONST_NAMES: una
 * entrada cubre homónimos en distintos ficheros — hoy sólo POTION_COLORS tiene dos).
 *
 * Categorías (prefijo obligatorio, validado por el test):
 *   [interna]  — sus strings NO se muestran al usuario: claves/ids/needles de matching.
 *   [deuda]    — user-facing conocido SIN entrada en el manifiesto: hueco heredado del
 *                trinquete, VISIBLE aquí en vez de invisible. Curarlo = añadirlo a
 *                DISPLAY_CONST_NAMES + entradas con cita en approved-strings.json y
 *                retirar esta línea. NO añadir deuda nueva: un objeto NUEVO user-facing
 *                se cura, no se apunta.
 *   [decision] — user-facing con decisión documentada de quedar fuera del corpus.
 */
export const EXCLUDED_DISPLAY_CONSTS = {
  // --- [interna]: valores que no llegan a pantalla ---
  USE_SCROLL_ROW_NAMES:
    "[interna] entradas CODIFICADAS de la name-table DS 0x1916 para las filas de pergamino del picker de (U)se (`*VL`, `*IS`…): el primer byte es el SIGILO DE FORMATO que print_list_row @0x0638 consume, y lo que llega a pantalla es la decoración DS 0x977c + la sigla rúnica del hechizo, que es neutra de idioma. No es prosa traducible (gemela de coreview.SCROLL_SIGIL_NAMES, ya fuera del barrido)",
  USE_POTION_ROW_NAMES:
    "[interna] ídem para las filas de poción (`!Blue`…): el `!` es el sigilo que print_list_row @0x0664 consume; lo user-facing es el COLOR, que la piel saca por t() y ya vive en el corpus de datos (DS 0x19C2 → data.json). Gemela de coreview.POTION_SIGIL_NAMES",
  VIEWPORT_MODAL_KEYS: "[interna] claves de ViewSnapshot que declinan el motionScroll (skin/fiel/skin.ts:1466, #253) — nombres de campo, no salida",
  PROFANITY_KEYWORDS: "[interna] needles del matcher de profanidad (conversation.ts:780) — input, no salida",
  VICTORY_TAIL: "[interna] ids de fase de la secuencia final (endgame/sequence.ts:204)",
  ATTACK_SLOTS: "[interna] claves de slot de equipo (equip.ts:186)",
  RECORD_SLOT_ORDER: "[interna] claves de slot de equipo (equip.ts:222)",
  ALL_SLOTS: "[interna] claves de slot de equipo (equip.ts:334+)",
  ENTIDADES: "[interna] tabla de escape HTML (escape-html.ts:32)",
  LOCATION_SLUGS: "[interna] slugs de localización para rutas/ids (location-display.ts:202)",
  SPELL_WORDS_ORDER: "[interna] claves de hechizo con guion bajo «In_Lor» (magic/spells.ts)",
  REAGENT_FLAG_KEYS: "[interna] claves de flags de reactivo «SulfurAsh» (magic/spells.ts)",
  TIME_STATUS: "[interna] códigos de 1 letra de g_time_spell (cast.ts:313 → state.timeSpell); ningún sitio del port los pinta",
  YELL_NAMES: "[interna] needles del matcher del Yell ritual (quest/ritual.ts:119) — el usuario los TECLEA, el juego no los imprime",
  SHADOWLORDS: "[interna] claves de questFlags (quest/shadowlord-keys.ts)",
  SHARD_ITEMS: "[interna] ids de item «shard-falsehood» (quest/underworld-seed.ts:94)",
  RUMOR_KEYWORDS: "[interna] needles del matcher de rumores (shops/shops.ts:1104)",
  TAVERN_KEYS: "[interna] claves de subtipo de taberna (ui/shop-console.ts:2497)",
  LOCATION_NAMES: "[interna] claves de matching contra shoppeKeeperMap «Britannia_Underworld» (shops/shops.ts:612)",
  SAVE_OPTIONALS_ABSENCE_MEANS: "[interna] claves de campos opcionales del save (state.ts:824)",
  WISH_HORSE_WORDS: "[interna] needles del matcher del deseo del pozo (wishingwell.ts:69)",
  INTRO_SKIN_CHOICES: "[interna] sus strings PROPIOS son ids («faithful»); las etiquetas vienen de SKIN_LABELS por referencia (main.ts:194)",
  NEW_GAME_SIDECAR: "[interna] valores de sidecar de partida nueva (main.ts:3418)",
  SHOP_TYPES: "[interna] claves de tipo contra ShoppeKeeperType del dataset (main.ts:2146, shops.ts:617)",
  // --- [deuda]: user-facing heredado SIN manifiesto (censado por #276; curación pendiente) ---
  PHRASES: "[deuda] frases fijas del motor de diálogo con citas DS en comentarios (conversation.ts:199); PropertyAccess → los sinks no las ven",
  LOCATION_DISPLAY_NAMES: "[deuda] nombres de lugar pintados (savepanel/coreview vía locationDisplayName, location-display.ts:197)",
  RUNE_SYLLABLE_BY_INITIAL: "[deuda] sílabas rúnicas ecoadas al teclear conjuro (ui/prompt-manager.ts:193 → echoSetLast)",
  CARDINAL_UNITS: "[deuda] piezas del pergamino del endgame vía cardinal() como arg de tf (quest/endgame.ts:117)",
  CARDINAL_TENS: "[deuda] ídem CARDINAL_UNITS (quest/endgame.ts:118)",
  ORDINAL_WORDS: "[deuda] ídem CARDINAL_UNITS vía ordinal() (quest/endgame.ts:135)",
  WINE_NAMES: "[deuda] filas del picker de vinos, sink name: (ui/shop-console.ts:1278)",
  RUMOR_SUBJECTS: "[deuda] sujeto del rumor de taberna interpolado (shops/shops.ts:1125)",
  RUMOR_PLACES: "[deuda] lugar del rumor de taberna interpolado (shops/shops.ts:1128)",
  POTION_COLORS: "[deuda] color de poción como arg de t() runtime (commands.ts:626) y picker (usePicker.ts:25); dos homónimos (usePotion.ts:19, commands.ts:563)",
  SCROLL_NAMES: "[deuda] etiquetas del picker de pergaminos, sink name: (usePicker.ts:26) — mismo género que USE_ITEM_NAMES",
  SCROLL_CODES: "[deuda] códigos impresos «A scroll: VL!» como arg de tf (commands.ts:638,669)",
  TAVERN_ALIVE_WORD: "[deuda] palabra insertada en prosa de taberna, citada 2..6/0x00aa (shops/shops.ts:875)",
  REAGENT_PATCHES: "[deuda] name: impreso tras « sprigs of\\n», citas DS 0x8692/0x86a2/0x86b2 en el fichero (reagent-patches.ts:75)",
  SHADOWLORD_QUALITIES: "[deuda] interpolado en la plantilla «An air of…» (shadowlord-urban.ts:66)",
  SKIN_LABELS: "[deuda] etiquetas [Q] del switcher de piel «1988 (fiel)»/«Shader (xBR)» (main.ts:188)",
  // Las cuatro siguientes las destapó la ficha relpaths (18-08) al meter shop-console.ts
  // y coreview.ts en la población del censo: heredadas, VISIBLES aquí en vez de invisibles.
  REAGENT_NAMES: "[deuda] nombres de reactivo de las filas de compra (ui/shop-console.ts:183) y del panel de partidas abreviados «Sp. Silk» (skin/coreview.ts:198); dos homónimos — mismo género que REAGENT_PATCHES, citas DS pendientes",
  SPELL_NAMES: "[deuda] nombres de conjuro «In Lor» de las filas del panel de partidas (skin/coreview.ts:182) — la tabla con espacios (la homónima de claves «In_Lor» es SPELL_WORDS_ORDER [interna])",
  SCROLL_SIGIL_NAMES: "[deuda] filas «*VL» del panel de partidas (skin/coreview.ts:247): composición QoL del prefijo * + los códigos de pergamino (SCROLL_CODES, también [deuda])",
  POTION_SIGIL_NAMES: "[deuda] filas «!Blue» del panel de partidas (skin/coreview.ts:248): composición QoL del prefijo ! + los colores de poción (POTION_COLORS, también [deuda])",
  // --- [decision]: fuera del corpus a propósito ---
  WINE_MENU_LINES: "[decision] las 6 líneas VERBATIM de la carta (#325, DS 0x9b8c..0x9be6): sin traducir a propósito — tipografía de ancho fijo del binario (shop-tables.ts:97, comentario con la decisión)",
};
const EXCLUDED_NAMES = new Set(Object.keys(EXCLUDED_DISPLAY_CONSTS));
// Un nombre no puede estar clasificado DOS veces (incluido Y excluido): fail-fast aquí
// además del aserto del test, porque este módulo lo importan 3 herramientas más.
for (const n of DISPLAY_CONST_NAMES) {
  if (EXCLUDED_NAMES.has(n)) throw new Error(`display-consts: «${n}» está en DISPLAY_CONST_NAMES y en EXCLUDED_DISPLAY_CONSTS a la vez`);
}

/** Sinks por LLAMADA a método user-facing en el shell (main.ts): hud.* / selector. */
function callSinkArgs(ts_, n) {
  if (!ts_.isCallExpression(n) || !ts_.isPropertyAccessExpression(n.expression)) return null;
  const obj = n.expression.expression;
  const method = n.expression.name.text;
  // `message`/`echo` abren fila nueva; `messageAppend` CONTINÚA la fila viva (p.ej. la
  // cabecera del handler de (U)se tras "Item: "): igual de user-facing → mismo sink.
  // ⇒ Ficha relpaths (18-08): el RECEPTOR ya no se restringe a `hud`. La consola de
  // tienda emite por `this.deps.message(…)` y la UI de repeticiones por `deps.message(…)`
  // — mismo canal user-facing con otro nombre delante, y restringir a `hud` dejaba esos
  // literales invisibles (6 en shop-console.ts, medidos). MEDIDO en el mismo careo:
  // core/ tiene CERO llamadas a métodos llamados message/echo/messageAppend/prompt, y en
  // el shell los únicos receptores existentes son hud/this.deps/deps — el sink por
  // NOMBRE DE MÉTODO no aflora ruido hoy; si un método técnico homónimo naciera, la
  // guarda del manifiesto lo haría visible (default-DENY), no invisible.
  if (method === "message" || method === "echo" || method === "messageAppend") return n.arguments;
  if (ts_.isIdentifier(obj) && obj.text === "selector" && method === "prompt") return n.arguments;
  return null;
}

function normTemplate(node) {
  let out = node.head.text;
  for (const s of node.templateSpans) out += "{}" + s.literal.text;
  return out;
}

function walk(dir) {
  const out = [];
  for (const ent of readdirSync(dir, { withFileTypes: true })) {
    const p = join(dir, ent.name);
    if (ent.isDirectory()) {
      if (ent.name === "__parity__") continue;
      out.push(...walk(p));
    } else if (ent.name.endsWith(".ts")) out.push(p);
  }
  return out;
}

/**
 * Extrae de un fichero. Devuelve [{text, loc}].
 *
 * `mode` elige la POLÍTICA DE HOJAS (los SINKS son los mismos en ambos modos, y eso es
 * el punto: guarda y lint no pueden derivar):
 *
 *   - "catalog" (defecto): todo literal user-facing alcanzable, con `t()`/`tf()`
 *     atravesados como identidad. Es lo que consumen el manifiesto y el corpus i18n.
 *   - "native": SÓLO plantillas `${…}` NATIVAS que llegan al sink SIN pasar por
 *     `t()`/`tf()` — la firma exacta del género «plantilla i18n muerta por
 *     interpolación nativa» (#126). Los literales planos no interpolan y no son
 *     candidatos; lo envuelto en `t()`/`tf()` ya está cableado y se PODA.
 */
function extractFile(ts_, file, coreLen, mode = "catalog") {
  const sf = ts_.createSourceFile(file, readFileSync(file, "utf8"), ts_.ScriptTarget.Latest, true);
  const rel = file.slice(coreLen + 1);
  // symbol table: const name -> {str?} | {arr?}
  const consts = new Map();
  // `tplStr`/`tplArr` son el subconjunto que venía de una plantilla `${…}` NATIVA (lo
  // único que el modo "native" puede aflorar a través de una const).
  const asConst = (init) => {
    if (ts_.isStringLiteral(init) || ts_.isNoSubstitutionTemplateLiteral(init)) return { str: init.text };
    if (ts_.isTemplateExpression(init)) return { str: normTemplate(init), tplStr: normTemplate(init) };
    if (ts_.isArrayLiteralExpression(init)) {
      const arr = init.elements.flatMap((e) =>
        ts_.isStringLiteral(e) || ts_.isNoSubstitutionTemplateLiteral(e)
          ? [e.text]
          : ts_.isTemplateExpression(e)
            ? [normTemplate(e)]
            : [],
      );
      const tplArr = init.elements.flatMap((e) => (ts_.isTemplateExpression(e) ? [normTemplate(e)] : []));
      return { arr, tplArr };
    }
    return null;
  };
  (function collect(n) {
    if (ts_.isVariableDeclaration(n) && n.name && ts_.isIdentifier(n.name) && n.initializer) {
      const v = asConst(n.initializer);
      if (v) consts.set(n.name.text, v);
    }
    ts_.forEachChild(n, collect);
  })(sf);

  const resolve = (node, depth = 0) => {
    if (depth > 6 || !node) return [];
    if (ts_.isStringLiteral(node) || ts_.isNoSubstitutionTemplateLiteral(node)) return [node.text];
    if (ts_.isTemplateExpression(node)) return [normTemplate(node)];
    // Array-literal EN el sink (`messages: ["…"]`): cada elemento es un mensaje.
    if (ts_.isArrayLiteralExpression(node)) return node.elements.flatMap((e) => resolve(e, depth + 1));
    if (
      ts_.isParenthesizedExpression(node) ||
      ts_.isNonNullExpression(node) ||
      ts_.isAsExpression(node) ||
      ts_.isSatisfiesExpression(node)
    )
      return resolve(node.expression, depth + 1);
    if (ts_.isIdentifier(node)) {
      const c = consts.get(node.text);
      if (!c) return [];
      return c.str !== undefined ? [c.str] : (c.arr ?? []);
    }
    if (ts_.isElementAccessExpression(node) && ts_.isIdentifier(node.expression)) {
      const c = consts.get(node.expression.text);
      return c && c.arr ? c.arr : [];
    }
    // Ternario `cond ? A : B`: ambas ramas son alcanzables en runtime, así que
    // afloramos las dos (resolviendo cada una recursivamente — cubre literales,
    // consts, plantillas y ternarios anidados). Un mensaje elegido por ternario
    // —`text: cond ? "X!" : "Y!"`, `hud.message(cond ? A : B)`— es tan user-facing
    // como uno literal; antes se evadía la guarda. La CONDICIÓN no se resuelve
    // (no es user-facing). i18n F0b hueco c.
    if (ts_.isConditionalExpression(node)) {
      return [...resolve(node.whenTrue, depth + 1), ...resolve(node.whenFalse, depth + 1)];
    }
    // ★ ENVOLTORIO i18n `t("…")` — el punto ciego de #132. `resolve` no tenía NINGÚN caso
    // para CallExpression, así que todo literal envuelto en `t()` devolvía [] y la guarda
    // no lo veía: 7 cadenas user-facing sin entrada en el manifiesto, 6 de ellas de otros
    // carriles. `t` es IDENTIDAD sobre el literal en tiempo de extracción
    // (`i18n/index.ts:173 export function t(str: string): string`): lo que se cataloga es
    // el string FUENTE, igual que si estuviera desnudo.
    //   ⚠ Sólo `t`, y a propósito: un caso genérico de CallExpression afloraría el primer
    //   argumento de CUALQUIER llamada y llenaría el manifiesto de literales técnicos.
    //   `tf` ya tiene su caso en el SINK (abajo) — pero ver la nota de ese punto: hacía
    //   falta también aquí, y por eso va incluido.
    if (
      ts_.isCallExpression(node) &&
      ts_.isIdentifier(node.expression) &&
      (node.expression.text === "t" || node.expression.text === "tf") &&
      node.arguments.length > 0
    ) {
      return resolve(node.arguments[0], depth + 1);
    }
    if (
      ts_.isBinaryExpression(node) &&
      (node.operatorToken.kind === ts_.SyntaxKind.PlusToken ||
        node.operatorToken.kind === ts_.SyntaxKind.QuestionQuestionToken ||
        node.operatorToken.kind === ts_.SyntaxKind.BarBarToken)
    ) {
      // `+` concatena; `??`/`||` aflora AMBAS ramas (el literal de fallback de un
      // `e.text ?? "Mantra:"` es user-facing). i18n F0b.
      return [...resolve(node.left, depth + 1), ...resolve(node.right, depth + 1)];
    }
    return [];
  };

  /**
   * POLÍTICA "native" — misma topología que `resolve` (mismos nodos de tránsito:
   * paréntesis, ternario, `+`/`??`/`||`, const e indexado), pero con las hojas
   * INVERTIDAS: la plantilla `${…}` es lo que se aflora y el literal plano no, y
   * `t(…)`/`tf(…)` se PODAN en vez de atravesarse (lo que pasa por el choke i18n ya
   * está cableado — es justo lo que el género NO es).
   */
  const resolveNative = (node, depth = 0) => {
    if (depth > 6 || !node) return [];
    if (ts_.isTemplateExpression(node)) return [normTemplate(node)];
    if (ts_.isStringLiteral(node) || ts_.isNoSubstitutionTemplateLiteral(node)) return [];
    if (ts_.isArrayLiteralExpression(node)) return node.elements.flatMap((e) => resolveNative(e, depth + 1));
    if (
      ts_.isParenthesizedExpression(node) ||
      ts_.isNonNullExpression(node) ||
      ts_.isAsExpression(node) ||
      ts_.isSatisfiesExpression(node)
    )
      return resolveNative(node.expression, depth + 1);
    if (ts_.isIdentifier(node)) {
      const c = consts.get(node.text);
      if (!c) return [];
      return c.tplStr !== undefined ? [c.tplStr] : (c.tplArr ?? []);
    }
    if (ts_.isElementAccessExpression(node) && ts_.isIdentifier(node.expression)) {
      const c = consts.get(node.expression.text);
      return c && c.tplArr ? c.tplArr : [];
    }
    if (ts_.isConditionalExpression(node)) {
      return [...resolveNative(node.whenTrue, depth + 1), ...resolveNative(node.whenFalse, depth + 1)];
    }
    // PODA (la diferencia con `resolve`): lo envuelto en el choke i18n NO es del género.
    if (
      ts_.isCallExpression(node) &&
      ts_.isIdentifier(node.expression) &&
      (node.expression.text === "t" || node.expression.text === "tf")
    ) {
      return [];
    }
    if (
      ts_.isBinaryExpression(node) &&
      (node.operatorToken.kind === ts_.SyntaxKind.PlusToken ||
        node.operatorToken.kind === ts_.SyntaxKind.QuestionQuestionToken ||
        node.operatorToken.kind === ts_.SyntaxKind.BarBarToken)
    ) {
      return [...resolveNative(node.left, depth + 1), ...resolveNative(node.right, depth + 1)];
    }
    return [];
  };

  /**
   * POLÍTICA "imports" (#331) — misma topología de tránsito que `resolve`, pero lo que
   * aflora son NOMBRES de identificador que el resolutor local NO puede seguir: la
   * firma exacta de la cadena que llega por const cross-módulo. Diferencias con las
   * hojas de `resolve`:
   *   - Identifier NO resuelto localmente → su nombre (resuelto localmente → nada:
   *     esos ya los cataloga `resolve`).
   *   - ElementAccess/PropertyAccess: aflora el identificador BASE más interno si no
   *     es const local (PropertyAccess es tránsito NUEVO aquí: `SHOP_UI.x` en un sink
   *     tiene por base una const importada; `resolve` no lo sigue por diseño).
   *   - Literales/plantillas → nada (no son identificadores).
   *   - Llamadas que no sean t()/tf() → nada (construcción dinámica, fuera de alcance
   *     declarado — un valor que pasa por función no es «una const importada»).
   * El FILTRO a «importado de verdad» (contra los ImportDeclaration del fichero) lo
   * aplica `censusImportedSinkConsts`, que es quien tiene el mapa de imports.
   */
  // Nombres LIGADOS localmente (parámetros, cualquier declaración de variable/función/
  // clase): un identificador así en un sink es una variable de runtime aunque COINCIDA
  // con un nombre importado (medido: `print: (t) => hud.message(t)` en main.ts sombrea
  // al `t` de i18n). Sólo lo consume el modo "imports"; ciego a scopes A PROPÓSITO
  // (mismo trato que el mapa `consts`): sobre-excluir aquí es no-flagear una sombra,
  // nunca inventar un rojo.
  const localBindings = new Set();
  if (mode === "imports") {
    const bindNames = (bn) => {
      if (!bn) return;
      if (ts_.isIdentifier(bn)) localBindings.add(bn.text);
      else if (ts_.isObjectBindingPattern(bn) || ts_.isArrayBindingPattern(bn)) {
        for (const el of bn.elements) if (ts_.isBindingElement(el)) bindNames(el.name);
      }
    };
    (function collectBindings(n) {
      if (ts_.isVariableDeclaration(n) || ts_.isParameter(n) || ts_.isBindingElement(n)) bindNames(n.name);
      if ((ts_.isFunctionDeclaration(n) || ts_.isClassDeclaration(n)) && n.name) localBindings.add(n.name.text);
      ts_.forEachChild(n, collectBindings);
    })(sf);
  }

  const resolveImportedIdents = (node, depth = 0) => {
    if (depth > 6 || !node) return [];
    if (ts_.isIdentifier(node)) return consts.has(node.text) || localBindings.has(node.text) ? [] : [node.text];
    if (ts_.isElementAccessExpression(node) || ts_.isPropertyAccessExpression(node)) {
      let base = node.expression;
      while (ts_.isElementAccessExpression(base) || ts_.isPropertyAccessExpression(base)) base = base.expression;
      return ts_.isIdentifier(base) && !consts.has(base.text) && !localBindings.has(base.text) ? [base.text] : [];
    }
    if (ts_.isArrayLiteralExpression(node)) return node.elements.flatMap((e) => resolveImportedIdents(e, depth + 1));
    if (
      ts_.isParenthesizedExpression(node) ||
      ts_.isNonNullExpression(node) ||
      ts_.isAsExpression(node) ||
      ts_.isSatisfiesExpression(node)
    )
      return resolveImportedIdents(node.expression, depth + 1);
    if (ts_.isConditionalExpression(node)) {
      return [...resolveImportedIdents(node.whenTrue, depth + 1), ...resolveImportedIdents(node.whenFalse, depth + 1)];
    }
    if (
      ts_.isCallExpression(node) &&
      ts_.isIdentifier(node.expression) &&
      (node.expression.text === "t" || node.expression.text === "tf") &&
      node.arguments.length > 0
    ) {
      return resolveImportedIdents(node.arguments[0], depth + 1);
    }
    if (
      ts_.isBinaryExpression(node) &&
      (node.operatorToken.kind === ts_.SyntaxKind.PlusToken ||
        node.operatorToken.kind === ts_.SyntaxKind.QuestionQuestionToken ||
        node.operatorToken.kind === ts_.SyntaxKind.BarBarToken)
    ) {
      return [...resolveImportedIdents(node.left, depth + 1), ...resolveImportedIdents(node.right, depth + 1)];
    }
    return [];
  };

  /** Resolución activa según el modo (los SINKS de abajo son comunes a todos). */
  const R = mode === "native" ? resolveNative : mode === "imports" ? resolveImportedIdents : resolve;

  /** Valores string de un literal de OBJETO o ARRAY (para las constantes-display). */
  const objArrayStrings = (init) => {
    let node = init;
    while (node && (ts_.isAsExpression(node) || ts_.isSatisfiesExpression(node) || ts_.isParenthesizedExpression(node))) {
      node = node.expression;
    }
    if (!node) return [];
    if (ts_.isArrayLiteralExpression(node)) return node.elements.flatMap((e) => R(e));
    if (ts_.isObjectLiteralExpression(node)) {
      return node.properties.flatMap((p) => (ts_.isPropertyAssignment(p) ? R(p.initializer) : []));
    }
    return [];
  };

  const found = [];
  /** #154: prosa = tiene espacio interno o salto; los identificadores de una palabra NO. */
  const isProse = (t) => / /.test(String(t).trim()) || String(t).includes("\n");
  const emit = (vals, node) => {
    const { line } = sf.getLineAndCharacterOfPosition(node.getStart(sf));
    for (const v of vals) found.push({ text: v, loc: `${rel}:${line + 1}` });
  };
  (function visit(n) {
    if (ts_.isPropertyAssignment(n)) {
      const nm = n.name;
      const key = ts_.isIdentifier(nm) ? nm.text : ts_.isStringLiteral(nm) ? nm.text : null;
      if (key === "text" || key === "message" || key === "messages") emit(R(n.initializer), n);
    }
    // ★ #154 — SINK `return`. Un `return` NO era sink, y por ahí se escapaban de la guarda
    // anti-fabricación el anuncio de CADA objeto de trama y la cascada del encarcelamiento
    // (13 cadenas, medidas en re/notes/sink-return-154-radio.md §4.bis).
    // GUARDA DE PROSA obligatoria: sin ella el sink aflora 51 IDENTIFICADORES internos
    // ("monsters", "towne", "keep", "helmet"…) que `isTechnical` no filtra y que llenarían
    // de basura un manifiesto cuyo valor entero es que cada línea lleve cita. El criterio
    // —espacio interno o salto de línea— parte las 64 en 13/51 EXACTO, sin afinar (§6).
    // (En modo "imports" la guarda de prosa no aplica: lo aflorado son NOMBRES de
    // identificador, no strings — filtrarlos por espacio interno los borraría todos.)
    if (ts_.isReturnStatement(n) && n.expression) emit(R(n.expression).filter(mode === "imports" ? () => true : isProse), n);
    if (
      ts_.isCallExpression(n) &&
      ts_.isPropertyAccessExpression(n.expression) &&
      n.expression.name.text === "push" &&
      ts_.isIdentifier(n.expression.expression) &&
      MSG_VAR.test(n.expression.expression.text)
    ) {
      for (const a of n.arguments) emit(R(a), n);
    }
    if (
      ts_.isVariableDeclaration(n) &&
      n.name &&
      ts_.isIdentifier(n.name) &&
      MSG_VAR.test(n.name.text) &&
      n.initializer &&
      ts_.isArrayLiteralExpression(n.initializer)
    ) {
      for (const el of n.initializer.elements) emit(R(el), n);
    }
    // Constante-display allowlistada (hueco a): aflora sus valores string (obj/array).
    if (
      ts_.isVariableDeclaration(n) &&
      n.name &&
      ts_.isIdentifier(n.name) &&
      DISPLAY_CONSTS.test(n.name.text) &&
      n.initializer
    ) {
      emit(objArrayStrings(n.initializer), n);
    }
    // Igual, pero para arrays estáticos de CLASE (`private static readonly ARR = [...]`):
    // son PropertyDeclaration, no VariableDeclaration. MISMA allowlist DISPLAY_CONSTS →
    // sólo aflora los nombres explícitamente permitidos (no un barrido general de props).
    if (
      ts_.isPropertyDeclaration(n) &&
      n.name &&
      ts_.isIdentifier(n.name) &&
      DISPLAY_CONSTS.test(n.name.text) &&
      n.initializer
    ) {
      emit(objArrayStrings(n.initializer), n);
    }
    // Const de mensaje PLANO allowlistada (#331, cura): aflora su literal (o las piezas
    // de su concatenación) en la DECLARACIÓN — el sink está en OTRO módulo y `resolve`
    // no cruza módulos por diseño; la exención mecánica del censo #331 exige que el
    // valor aflore aquí. En modos "native"/"imports" R no devuelve nada para literales.
    if (
      ts_.isVariableDeclaration(n) &&
      n.name &&
      ts_.isIdentifier(n.name) &&
      STRING_CONSTS.test(n.name.text) &&
      n.initializer
    ) {
      emit(R(n.initializer), n);
    }
    // Sink por llamada del shell (hueco b): hud.message/echo(…), selector.prompt(…).
    const callArgs = callSinkArgs(ts_, n);
    if (callArgs) for (const a of callArgs) emit(R(a), n);
    // Sink del cableado i18n: `tf(TEMPLATE, …args)` — el 1er arg es la PLANTILLA
    // user-facing (posicional `{}`), la misma normalización que un `text:`. Los args
    // (nombres/números) no son literales fijos y no se afloran aquí.
    if (ts_.isCallExpression(n) && ts_.isIdentifier(n.expression) && n.expression.text === "tf" && n.arguments.length > 0) {
      emit(R(n.arguments[0]), n);
    }
    ts_.forEachChild(n, visit);
  })(sf);
  return found;
}

/**
 * Extrae los literales user-facing de `coreDir` (recursivo) MÁS `extraFiles`
 * sueltos (p.ej. `src/main.ts`, el shell — hueco b de F0b). El `loc` es relativo
 * al PADRE de `coreDir` (`src/`), así `core/game.ts` y `main.ts` conviven.
 * Devuelve Map<text, loc>.
 */
export function extractUserStrings(coreDir, extraFiles = []) {
  const baseLen = dirname(coreDir).length; // rel a src/
  const map = new Map();
  for (const f of [...walk(coreDir), ...extraFiles]) {
    for (const e of extractFile(ts, f, baseLen)) {
      if (!map.has(e.text)) map.set(e.text, e.loc);
    }
  }
  return map;
}

/**
 * Igual que `extractUserStrings` pero en modo "native": devuelve TODAS las plantillas
 * `${…}` (normalizadas a `{}`) que llegan a un sink user-facing SIN pasar por
 * `t()`/`tf()`. Es el censo crudo del lint de #126 — SIN dedupe y SIN filtrar por
 * es.json, porque el sitio (fichero:línea) es el producto y la misma plantilla puede
 * tener DOS emisores (el reloj de bolsillo y el de pie lo demostraron).
 */
export function extractNativeInterpolations(coreDir, extraFiles = []) {
  const baseLen = dirname(coreDir).length; // rel a src/
  const out = [];
  for (const f of [...walk(coreDir), ...extraFiles]) out.push(...extractFile(ts, f, baseLen, "native"));
  return out;
}

/**
 * #276 — CENSO ESTRUCTURAL de candidatos a constante-display, DERIVADO del código (no de
 * una lista). Un candidato es toda declaración, en la MISMA población que escanea el
 * extractor (coreDir recursivo + extraFiles), que cumple la FORMA de los 47 miembros de
 * `DISPLAY_CONST_NAMES` (leída de ellos, no inventada):
 *   - `VariableDeclaration` o `PropertyDeclaration` (los arrays estáticos de clase,
 *     rama REFUGE_NARRATION) con identificador EN MAYÚSCULAS (`/^[A-Z][A-Z0-9_]*$/`),
 *   - inicializador OBJETO o ARRAY literal (tras quitar `as`/`satisfies`/paréntesis) —
 *     la forma que el barrido de sinks NO alcanza (razón de ser de la allowlist),
 *   - con ≥1 string anidado (profundidad ≤4) con contenido alfabético (`isTechnical`).
 * Devuelve [{name, loc, strings}] con loc `fichero:línea` relativo al padre de coreDir.
 *
 * El consumidor es `tests/display-consts-censo.test.ts` (default-DENY): cada candidato
 * debe estar en `DISPLAY_CONST_NAMES`, en `EXCLUDED_DISPLAY_CONSTS`, o tener TODOS sus
 * strings ya aflorados por otra vía de `extractUserStrings` (exención mecánica que no
 * caduca: si esa otra vía desaparece, la exención desaparece con ella). Un objeto de
 * corpus NUEVO sin clasificar → ROJO nombrándolo. VERIFICADO 2026-08-18 sobre main
 * c95e8296: 107 candidatos = 49 declaraciones en lista + 19 exentas mecánicamente +
 * 39 declaraciones (38 nombres) en el ledger — resto cero.
 */
export function censusDisplayCandidates(coreDir, extraFiles = []) {
  const baseLen = dirname(coreDir).length;
  const ALLCAPS = /^[A-Z][A-Z0-9_]*$/;
  const unwrap = (n) => {
    while (n && (ts.isAsExpression(n) || ts.isSatisfiesExpression(n) || ts.isParenthesizedExpression(n))) n = n.expression;
    return n;
  };
  const stringsOf = (node, depth = 0) => {
    node = unwrap(node);
    if (depth > 4 || !node) return [];
    if (ts.isStringLiteral(node) || ts.isNoSubstitutionTemplateLiteral(node)) return [node.text];
    if (ts.isTemplateExpression(node)) return [normTemplate(node)];
    if (ts.isArrayLiteralExpression(node)) return node.elements.flatMap((e) => stringsOf(e, depth + 1));
    if (ts.isObjectLiteralExpression(node))
      return node.properties.flatMap((p) => (ts.isPropertyAssignment(p) ? stringsOf(p.initializer, depth + 1) : []));
    return [];
  };
  const out = [];
  for (const f of [...walk(coreDir), ...extraFiles]) {
    const sf = ts.createSourceFile(f, readFileSync(f, "utf8"), ts.ScriptTarget.Latest, true);
    const rel = f.slice(baseLen + 1);
    (function visit(n) {
      if (
        (ts.isVariableDeclaration(n) || ts.isPropertyDeclaration(n)) &&
        n.name &&
        ts.isIdentifier(n.name) &&
        ALLCAPS.test(n.name.text) &&
        n.initializer
      ) {
        const init = unwrap(n.initializer);
        if (init && (ts.isArrayLiteralExpression(init) || ts.isObjectLiteralExpression(init))) {
          const strings = stringsOf(n.initializer).filter((s) => !isTechnical(s));
          if (strings.length > 0) {
            const { line } = sf.getLineAndCharacterOfPosition(n.getStart(sf));
            out.push({ name: n.name.text, loc: `${rel}:${line + 1}`, strings });
          }
        }
      }
      ts.forEachChild(n, visit);
    })(sf);
  }
  return out;
}

/**
 * Ficheros del SHELL (fuera de `src/core/`) con strings user-facing — FUENTE
 * ÚNICA para los 3 consumidores (i18n-corpus, gen-string-manifest y la guarda
 * tests/string-manifest.test.ts): `main.ts` (hueco b de F0b) MÁS los conductores
 * de escena extraídos de boot() (TRAMO 1 del refactor estructural, auditoría
 * MANT-1/ARQ-2) — los strings que VIAJARON de main.ts a estos módulos deben
 * seguir viéndose (p.ej. "Zzzzzz..." en camp-sleep, las líneas rúnicas del
 * pergamino en endgame-pacer); sin esto la guarda los daría por huérfanos.
 * Rutas relativas a `src/`.
 */
export const SHELL_FILE_RELPATHS = [
  "main.ts",
  "ui/autowalk.ts",
  "ui/moongate-gate.ts",
  "ui/combat-pacer.ts",
  "ui/camp-sleep.ts",
  "ui/troll-sneak.ts",
  "ui/endgame-pacer.ts",
  // TRAMO 2: prompt vivo + conversación por consola + pickers de PJ.
  "ui/prompt-manager.ts",
  "ui/talk-console.ts",
  "ui/pickers.ts",
  // Ficha relpaths (18-08, hija de #276): emisores del shell que el censo por-fichero
  // (`censusShellEmitterFiles`) destapó FUERA de la lista — sus strings no entraban al
  // manifiesto ni a la guarda. Desde ahora un emisor nuevo sin clasificar lo enrojece
  // `tests/shell-files-censo.test.ts`, no un careo manual.
  "ui/shop-console.ts", // 6 literales inline vía this.deps.message: ecos R/L (putchar), degradaciones QoL
  "ui/replay-ui.ts", // "{} B"/"{} KB" de fmtBytes (sink return); el resto va tras ts() (limitación declarada)
  "skin/coreview.ts", // fallback `Item {}` y línea de lugar `{} · L{} · {}` del panel de partidas
  "skin/fiel/skin.ts", // eco "Z-stats..." (DS 0xa28c sin \n final) despachado por la piel fiel
];

/** Rutas absolutas de los ficheros shell dado `srcDir` (= <game>/src). */
export const shellFiles = (srcDir) => SHELL_FILE_RELPATHS.map((f) => join(srcDir, f));

/**
 * Ficha relpaths — LEDGER DE EXCLUSIONES del censo por-FICHERO (la otra mitad del
 * default-DENY, espejo de `EXCLUDED_DISPLAY_CONSTS` a nivel fichero). Todo fichero del
 * shell (src/ fuera de core/) donde el extractor aflora ≥1 string no-técnico y que NO
 * está en `SHELL_FILE_RELPATHS` debe tener aquí una entrada RAZONADA, o
 * `tests/shell-files-censo.test.ts` enrojece nombrándolo. La clave es la ruta relativa
 * a `src/`. Mismas categorías que #276 ([interna]/[deuda]/[decision]).
 */
export const EXCLUDED_SHELL_FILES = {
  "debug/debugApi.ts":
    "[interna] fachada del menú DEBUG + hook e2e `testHookInnLeave` (arnés __u5test.innLeave del espejo, extraído de main.ts en la ficha E1 para ser testeable): sus dos strings son del INSTRUMENTO — la plantilla «{}? I know of no such person.» (que era el residual de main.ts del lint #126 y se retiró de RESIDUAL al mudarse aquí) y el rechazo location=0 en castellano. No es texto del juego servido al jugador",
  "debug/teleportPicker.ts": "[interna] filas del picker del panel de DEBUG («{} (sub {})», «{} f{} ({},{})») — herramienta de desarrollo, no texto del juego",
  "skin/portrait/deck-ancho.ts": "[interna] hojas CSS en template literals que salen por `return` (el sink return + isProse las ve como prosa); son estilo, no texto del juego",
  "skin/fiel/intro.ts": "[interna] valores de color hex («#ffffff») en DEFAULT_INTRO_COLORS, cuyo campo se llama `text:` y colisiona con el sink homónimo; no es prosa",
  "enhanced/spells/css.ts": "[interna] hoja CSS de la lista de hechizos en template literal, que sale por `return` (el sink return + isProse la ve como prosa por sus docblocks); es estilo, no texto del juego. El cromo de ESA lista (`enhanced/spells/panel.ts`) sí es texto y va entero por `ts()`, como el resto del shell",
  "enhanced/party/css.ts": "[interna] hoja CSS del selector compacto de miembro del grupo, gemela de `enhanced/spells/css.ts` y excluida por lo mismo: sale por `return` dentro de un template literal con docblocks largos, que el sink return + isProse leen como prosa. Es estilo, no texto del juego; el cromo del selector (`enhanced/party/panel.ts`) sí es texto y va entero por `ts()`",
};
const EXCLUDED_FILE_SET = new Set(Object.keys(EXCLUDED_SHELL_FILES));
for (const f of SHELL_FILE_RELPATHS) {
  if (EXCLUDED_FILE_SET.has(f)) throw new Error(`shell-files: «${f}» está en SHELL_FILE_RELPATHS y en EXCLUDED_SHELL_FILES a la vez`);
}

/**
 * Ficha relpaths — CENSO por-FICHERO de emisores del shell, DERIVADO del código (no de
 * una lista): la clase de #276 a nivel fichero. `SHELL_FILE_RELPATHS` censaba por lista
 * cableada, y un fichero de UI nuevo (o 30 de los 39 de src/ui/) que emitiera
 * user-facing nacía INVISIBLE al trinquete anti-fabricación.
 *
 * POBLACIÓN: todo `.ts` bajo `srcDir` FUERA de `core/` (core lo barre entero
 * `extractUserStrings`; el shell es la zona ciega). SEÑAL de emisor (estructural, la
 * misma vara que el extractor — no una heurística aparte): `extractFile` en modo
 * catálogo aflora ≥1 string NO-técnico en el fichero. Así «emisor» significa
 * exactamente «el extractor vería algo aquí si mirase»: un fichero que sólo emite
 * consts cross-módulo (p.ej. `t(SHOP_UI.x)`) no es candidato porque sus strings ya
 * afloran en su declaración de core.
 *
 * Devuelve [{rel, strings}] con rel relativo a `srcDir`. Consumidor:
 * `tests/shell-files-censo.test.ts` (default-DENY contra SHELL_FILE_RELPATHS ∪
 * EXCLUDED_SHELL_FILES).
 */
export function censusShellEmitterFiles(srcDir) {
  const walkShell = (dir, top) => {
    const out = [];
    for (const ent of readdirSync(dir, { withFileTypes: true })) {
      const p = join(dir, ent.name);
      if (ent.isDirectory()) {
        if (top && ent.name === "core") continue;
        if (ent.name === "__parity__") continue;
        out.push(...walkShell(p, false));
      } else if (ent.name.endsWith(".ts")) out.push(p);
    }
    return out;
  };
  const baseLen = srcDir.length;
  const res = [];
  for (const f of walkShell(srcDir, true)) {
    const strings = [...new Set(extractFile(ts, f, baseLen).map((e) => e.text))].filter((s) => !isTechnical(s));
    if (strings.length > 0) res.push({ rel: f.slice(baseLen + 1), strings });
  }
  return res;
}

/** Técnico = sin contenido alfabético (vacío, pura interpolación/puntuación). */
export const isTechnical = (s) => !/[A-Za-z]/.test(s);

/**
 * #331 — strings de la declaración de `name` en el módulo `absPath` (para carear el
 * contenido de una const importada contra lo ya extraído). Mismas hojas que el
 * `stringsOf` del censo #276: literal/plantilla/array/objeto, profundidad ≤4.
 * Devuelve [] si el fichero no se puede leer o la declaración no existe/no es literal.
 */
function moduleConstStrings(absPath, name, hops = 0) {
  if (hops > 3) return null; // cadena de re-exports demasiado larga: fail-closed
  let src;
  try {
    src = readFileSync(absPath, "utf8");
  } catch {
    return null;
  }
  const sf = ts.createSourceFile(absPath, src, ts.ScriptTarget.Latest, true);
  const leaves = (node, depth = 0) => {
    while (node && (ts.isAsExpression(node) || ts.isSatisfiesExpression(node) || ts.isParenthesizedExpression(node))) node = node.expression;
    if (depth > 4 || !node) return [];
    if (ts.isStringLiteral(node) || ts.isNoSubstitutionTemplateLiteral(node)) return [node.text];
    if (ts.isTemplateExpression(node)) return [normTemplate(node)];
    if (ts.isArrayLiteralExpression(node)) return node.elements.flatMap((e) => leaves(e, depth + 1));
    if (ts.isObjectLiteralExpression(node))
      return node.properties.flatMap((p) => (ts.isPropertyAssignment(p) ? leaves(p.initializer, depth + 1) : []));
    // Concatenación de literales adyacentes (medido: SHOP_CLOSED_MESSAGE en
    // shop-hours.ts:51 es `'…' + "…"`): afloran las DOS piezas.
    if (ts.isBinaryExpression(node) && node.operatorToken.kind === ts.SyntaxKind.PlusToken)
      return [...leaves(node.left, depth + 1), ...leaves(node.right, depth + 1)];
    return [];
  };
  let found = false;
  let out = [];
  (function visit(n) {
    if (
      (ts.isVariableDeclaration(n) || ts.isPropertyDeclaration(n) || ts.isEnumMember(n)) &&
      n.name &&
      (ts.isIdentifier(n.name) ? n.name.text === name : false) &&
      n.initializer
    ) {
      found = true;
      out.push(...leaves(n.initializer));
    }
    // enum importado usado como valor: sus miembros string son el contenido
    if (ts.isEnumDeclaration(n) && n.name.text === name) {
      found = true;
      for (const m of n.members) if (m.initializer) out.push(...leaves(m.initializer));
    }
    ts.forEachChild(n, visit);
  })(sf);
  if (found) return out;
  // RE-EXPORT (medido: COMBAT_ABSORBED_MESSAGE viaja por combat/index.ts): si el módulo
  // no DECLARA el nombre, seguir `export { X } from "./y"` / `export * from "./y"`.
  for (const st of sf.statements) {
    if (!ts.isExportDeclaration(st) || !st.moduleSpecifier || !ts.isStringLiteral(st.moduleSpecifier)) continue;
    const spec = st.moduleSpecifier.text;
    if (!spec.startsWith(".")) continue;
    let exportsName = !st.exportClause; // `export * from` re-exporta todo
    if (st.exportClause && ts.isNamedExports(st.exportClause)) {
      exportsName = st.exportClause.elements.some((el) => el.name.text === name);
    }
    if (!exportsName) continue;
    const next = resolveImportPath(absPath, spec);
    if (!next) continue;
    const r = moduleConstStrings(next, name, hops + 1);
    if (r !== null) return r;
  }
  return null; // declaración no hallada estáticamente: fail-closed (exige ledger)
}

/**
 * #331 — resuelve un specifier RELATIVO a fichero .ts real (o null si no aparece).
 * Los imports del port van en estilo ESM con extensión `.js` («./world/wind.js») y el
 * fichero real es `.ts`: se prueba primero el swap de extensión.
 */
function resolveImportPath(fromFile, spec) {
  const base = join(dirname(fromFile), spec);
  for (const cand of [base.replace(/\.m?js$/, ".ts"), base.endsWith(".ts") ? base : `${base}.ts`, join(base, "index.ts")]) {
    try {
      readFileSync(cand);
      return cand;
    } catch {
      /* siguiente candidato */
    }
  }
  return null;
}

/**
 * #331 — LEDGER DE EXCLUSIONES del censo de consts importadas en sinks (la otra mitad
 * del default-DENY, espejo de `EXCLUDED_DISPLAY_CONSTS`). Todo identificador IMPORTADO
 * que llegue a un sink user-facing y cuyo contenido NO esté ya clasificado (ni aflora
 * por otra vía de `extractUserStrings`, ni su nombre está en `DISPLAY_CONST_NAMES` /
 * `EXCLUDED_DISPLAY_CONSTS`) debe tener aquí una entrada RAZONADA, o
 * `tests/imported-consts-censo.test.ts` enrojece nombrándolo. La clave es el NOMBRE
 * del identificador (una entrada cubre homónimos). Mismas categorías que #276 — y la
 * misma doctrina: las 6 [deuda] de abajo son HEREDADAS (medidas al estrenar la guarda,
 * 2026-08-20); una const importada NUEVA user-facing se cura (uso local que el
 * extractor resuelva, o vía de extracción + manifiesto), no se apunta. Curar una =
 * retirar su línea; si sana (sus strings pasan a aflorar por otra vía), el aserto de
 * ranciedad ordena retirarla.
 */
export const EXCLUDED_IMPORTED_SINK_CONSTS = {
  // VACÍO desde la cura de las 6 [deuda] heredadas (2026-08-20, carril fix-deudas):
  // EARTHQUAKE_MESSAGE · WHIRLPOOL_MESSAGE · MIRROR_BROKEN_MSG · SHOP_CLOSED_MESSAGE ·
  // COMBAT_ABSORBED_MESSAGE · GATE_TRAVEL_PROMPT afloran ahora por STRING_CONST_NAMES
  // (exención mecánica por valor) y sus 7 strings tienen entrada [D] byte-exacta en
  // approved-strings.json. El trinquete de imported-consts-censo.test.ts obligó a
  // retirarlas (deuda sanada = rojo hasta mover el ledger).
};

/**
 * #331 — CENSO de identificadores IMPORTADOS que llegan a un sink user-facing, DERIVADO
 * del código (no de una lista). La clase que mata (#331, de vinos-325): la cadena
 * user-facing que llega por CONSTANTE IMPORTADA era invisible a las TRES guardas
 * (string-manifest, display-consts-censo, shell-files-censo) — el `resolve()` del
 * extractor sólo sigue consts LOCALES (limitación declarada en su cabecera), el censo
 * #276 censa DECLARACIONES (no usos), y el censo por-fichero censa EMISORES (el módulo
 * que sólo exporta la const no tiene sink). Siembra medida 2026-08-20: const exportada
 * en fichero nuevo + `events.push({text: SEED})` en otro → 14/14 verdes.
 *
 * POBLACIÓN: la misma que el extractor (coreDir recursivo + extraFiles), con la MISMA
 * vara (los sinks de `extractFile`, en modo "imports" — no una heurística aparte).
 * SEÑAL: identificador que llega a un sink, que el resolutor local NO resuelve, y que
 * figura en un ImportDeclaration del fichero (los no importados son variables de
 * runtime: construcción dinámica, fuera de alcance declarado del extractor).
 *
 * Devuelve [{name, froms, locs, strings, resolved}] — strings NO-técnicos leídos de la
 * DECLARACIÓN en el módulo de origen (siguiendo specifier relativo, swap .js→.ts y
 * re-exports hasta 3 saltos; concatenación `+` aflora sus piezas). `resolved=false` si
 * ALGÚN origen no fue legible estáticamente (specifier no relativo, namespace import,
 * declaración ausente): fail-closed, el consumidor no puede exentarlo mecánicamente.
 * LIMITACIÓN DECLARADA (un salto): si la declaración resuelta no tiene literales pero
 * REFERENCIA a otra const, el contenido referido responde ante su propia declaración
 * (censada por #276 o por este mismo censo en su fichero) — no se persigue la cadena.
 * Consumidor: `tests/imported-consts-censo.test.ts` (default-DENY).
 */
export function censusImportedSinkConsts(coreDir, extraFiles = []) {
  const baseLen = dirname(coreDir).length;
  const byName = new Map();
  for (const f of [...walk(coreDir), ...extraFiles]) {
    const sf = ts.createSourceFile(f, readFileSync(f, "utf8"), ts.ScriptTarget.Latest, true);
    const imports = new Map(); // nombre local -> specifier
    for (const st of sf.statements) {
      if (!ts.isImportDeclaration(st) || !st.importClause || !ts.isStringLiteral(st.moduleSpecifier)) continue;
      if (st.importClause.isTypeOnly) continue; // `import type` no existe en runtime
      const spec = st.moduleSpecifier.text;
      const nb = st.importClause.namedBindings;
      if (nb && ts.isNamedImports(nb)) {
        for (const el of nb.elements) if (!el.isTypeOnly) imports.set(el.name.text, spec);
      }
      if (nb && ts.isNamespaceImport(nb)) imports.set(nb.name.text, spec);
      if (st.importClause.name) imports.set(st.importClause.name.text, spec);
    }
    if (imports.size === 0) continue;
    for (const e of extractFile(ts, f, baseLen, "imports")) {
      const spec = imports.get(e.text);
      if (spec === undefined) continue;
      let row = byName.get(e.text);
      if (!row) {
        row = { name: e.text, froms: [], locs: [], strings: [], resolved: true };
        byName.set(e.text, row);
      }
      if (!row.froms.includes(spec)) {
        row.froms.push(spec);
        const srcPath = spec.startsWith(".") ? resolveImportPath(f, spec) : null;
        const found = srcPath ? moduleConstStrings(srcPath, e.text) : null;
        if (found === null) {
          row.resolved = false; // fail-closed: sin declaración legible no hay exención mecánica
        } else {
          for (const s of found) {
            if (!isTechnical(s) && !row.strings.includes(s)) row.strings.push(s);
          }
        }
      }
      if (!row.locs.includes(e.loc)) row.locs.push(e.loc);
    }
  }
  return [...byName.values()];
}
