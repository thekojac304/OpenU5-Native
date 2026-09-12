/**
 * CATÁLOGO DE LO QUE EMITE LA EXTRACCIÓN — la fuente ÚNICA de la puerta de versión (#293).
 *
 * ── EL DEFECTO QUE ESTE FICHERO EXISTE PARA CERRAR ───────────────────────────────────
 * El sitio público NO lleva datos del juego: los assets los genera la extracción de /byo
 * EN el navegador de quien juega y se los sirve su service worker desde `u5-assets-v1`.
 * Cuando el pipeline aprende a emitir un asset NUEVO, toda extracción anterior queda coja
 * en esa función — y en SILENCIO: la petición cae a la red, openu5.org devuelve el
 * soft-404 (#63: HTTP 200 con la home), y el `.catch()` best-effort del arranque degrada
 * sin decir nada. Le pasó al usuario el 14-08 con `shrine-scene.json`: entró en un
 * santuario y no había escena (medido: openu5.org/assets/shrine-scene.json = 404 · el
 * staging, que sirve los assets del servidor, = 200/3421 B).
 *
 * ── POR QUÉ UNA LISTA Y NO UN NÚMERO DE VERSIÓN ──────────────────────────────────────
 * Un `VERSION = 7` a mano es una FOTO A MANO: quien añade el asset nº 8 tiene que
 * acordarse de subirlo, y el día que no se acuerde el aviso no salta y volvemos al
 * silencio de hoy. La lista se DERIVA de las emisiones reales (`verificaCatalogo` de
 * pipeline.ts aborta la extracción si alguien emite una ruta que no está aquí), así que
 * olvidarse es imposible: no hay estado que actualizar, hay un aserto que se rompe.
 *
 * ── UNA FUENTE, DOS CONSUMIDORES ─────────────────────────────────────────────────────
 * Lo consumen el PRODUCTOR (`extractor/src/pipeline.ts`, que se auto-verifica al acabar)
 * y el CONSUMIDOR (`game/src/web/extraccion.ts`, que carea esta lista contra lo que hay
 * en la caché del visitante al arrancar). Si hubiera dos listas podrían divergir, y la
 * que se quedaría rancia sería justo la del aviso.
 *
 * 🔴 ESTE MÓDULO NO IMPORTA NADA, Y ES UN REQUISITO, NO UN GUSTO: lo importa el bundle
 * del JUEGO, y en este repo ya se midió que importar una constante importa su GRAFO
 * (477 KB colados por un import). Un `import` aquí arrastraría los parsers del extractor
 * —pngjs, lzw, atlas— a la página de quien juega. Si necesitas un tipo, decláralo aquí.
 */

/** Nombre de la caché donde la extracción de /byo deja los assets. Lo copia `public/sw.js`. */
export const BYO_CACHE = "u5-assets-v1";

/** Prefijo de URL bajo el que el juego pide los assets (y bajo el que se cachean). */
export const ASSETS_PREFIX = "/assets/";

/** Una salida del pipeline. `ruta` es relativa al prefijo: «maps/overworld.json». */
export interface AssetDeExtraccion {
  readonly ruta: string;
  /**
   * OPCIONAL = su emisión depende de ficheros del original que NO son obligatorios
   * (`REQUIRED_FILES` de pipeline.ts no los lista: DNG*.16, ITEMS.16, MON*.16, los .OOL,
   * las láminas .16/.BIT del cierre y la intro, PROPORT.PCS). Que falte NO acusa a nadie:
   * puede ser una copia legítima sin esos ficheros. Por eso quedan FUERA del careo del
   * arranque — meterlos daría el aviso a quien no tiene nada que arreglar.
   */
  readonly opcional?: boolean;
  /** Ficheros del original de los que sale. Documenta el porqué de `opcional`. */
  readonly de: string;
  /** Sólo lo emite el pipeline con tiles: `--skip-tiles` de la CLI lo apaga. */
  readonly conTiles?: boolean;
}

/**
 * Todo lo que `runPipeline` puede escribir, en el orden en que lo escribe.
 *
 * La disciplina para quien añada un asset: añadir su línea AQUÍ en el mismo commit. No
 * hay que acordarse — `verificaCatalogo()` rompe la extracción nombrando la ruta huérfana.
 */
export const ASSETS_DE_EXTRACCION: readonly AssetDeExtraccion[] = [
  // 1. Tiles y sus derivados (bloque `if (!opts.skipTiles)`).
  { ruta: "tiles-ega.png", de: "TILES.16", conTiles: true },
  { ruta: "tiles-hd.png", de: "TILES.16 (xBR ×4)", conTiles: true },
  { ruta: "dungeon-persp.png", de: "DNG1/2/3.16", conTiles: true, opcional: true },
  { ruta: "dungeon-persp.json", de: "DNG1/2/3.16", conTiles: true, opcional: true },
  { ruta: "dungeon-feat.png", de: "ITEMS.16", conTiles: true, opcional: true },
  { ruta: "dungeon-feat.json", de: "ITEMS.16", conTiles: true, opcional: true },
  { ruta: "dungeon-mon.png", de: "MON0-7.16", conTiles: true, opcional: true },
  { ruta: "dungeon-mon.json", de: "MON0-7.16", conTiles: true, opcional: true },
  // 1b/1c. Fuentes de la capa de texto (baratas: siempre).
  { ruta: "font-ibm.png", de: "IBM.CH" },
  // SIN opcional a propósito (#339): sale de un fichero OBLIGATORIO (IBM.CH), así que
  // su ausencia acusa a una extracción VIEJA — que verificaCatalogo aborte nombrándola
  // es la señal correcta de re-extraer, no un fallo.
  { ruta: "font-ibm-ext.png", de: "IBM.CH (composición: letra del juego + signo 8×8)" },
  { ruta: "font-runes.png", de: "RUNES.CH" },
  // 2-8. El cuerpo obligatorio.
  { ruta: "data.json", de: "DATA.OVL" },
  { ruta: "maps/overworld.json", de: "BRIT.DAT + DATA.OVL" },
  { ruta: "maps/underworld.json", de: "UNDER.DAT" },
  { ruta: "maps/smallmaps.json", de: "CASTLE/TOWNE/DWELLING/KEEP.DAT" },
  { ruta: "maps/dungeons.json", de: "DUNGEON.DAT" },
  { ruta: "maps/combatmaps.json", de: "BRIT.CBT + DUNGEON.CBT" },
  { ruta: "npcs.json", de: "CASTLE/TOWNE/DWELLING/KEEP.NPC" },
  { ruta: "talk/castle.json", de: "CASTLE.TLK" },
  { ruta: "talk/towne.json", de: "TOWNE.TLK" },
  { ruta: "talk/dwelling.json", de: "DWELLING.TLK" },
  { ruta: "talk/keep.json", de: "KEEP.TLK" },
  { ruta: "shoppe.json", de: "SHOPPE.DAT" },
  { ruta: "initial-state.json", de: "INIT.GAM" },
  { ruta: "init.gam", de: "INIT.GAM (plantilla del guardado nativo)" },
  { ruta: "init.ool", de: "BRIT.OOL + UNDER.OOL", opcional: true },
  { ruta: "signs.json", de: "SIGNS.DAT + DATA.OVL" },
  { ruta: "questions.json", de: "QUESTION.DAT" },
  { ruta: "look2.json", de: "LOOK2.DAT" },
  { ruta: "story.json", de: "STORY.DAT" },
  { ruta: "intro-scenes.json", de: "DATA.OVL + STORY.DAT" },
  { ruta: "british-path.json", de: "BRITISH.PTH" },
  { ruta: "demo-scene.json", de: "MISCMAPS.DAT + DATA.OVL" },
  { ruta: "shrine-scene.json", de: "MISCMAPS.DAT" },
  { ruta: "endgame.json", de: "MISCMAPS.DAT + END.DAT + ENDMSG.DAT" },
  // SIN `opcional` a propósito (mismo criterio que `font-ibm-ext.png` de #339): sale de
  // ficheros OBLIGATORIOS, así que su ausencia acusa a una extracción VIEJA y el aviso de
  // arranque (#293) debe echarla de menos por su nombre. Es justo lo que hace falta aquí:
  // sin este asset el port NO tiene los discursos que imprime — antes los llevaba
  // transcritos en el código (FICHA β).
  { ruta: "ds-strings.json", de: "KARMA.DAT + MISCMSG.DAT + ENDMSG.DAT" },
  { ruta: "endgame-scenes.png", de: "END1/END2/ENDSC.16", opcional: true },
  { ruta: "endgame-scenes.json", de: "END1/END2/ENDSC.16", opcional: true },
  { ruta: "intro-pics.png", de: "CREATE/TEXT/STORY*/ULTIMA/STARTSC.16 + *.BIT", opcional: true },
  { ruta: "intro-pics.json", de: "CREATE/TEXT/STORY*/ULTIMA/STARTSC.16 + *.BIT", opcional: true },
  // 9. MÚSICA DEL PARCHE COMUNITARIO (Ultima V Upgrade Patch). `opcional` de libro y por
  //    la razón exacta que define el campo: una copia SIN el parche no trae ni los XMI ni
  //    `FAT.OPL`, y esa ausencia no acusa a una extracción vieja — acusa a que esa copia
  //    nunca tuvo música. Meterlas en el careo del arranque daría el aviso de «re-extrae»
  //    a quien no tiene absolutamente nada que arreglar.
  //    Los nombres van por CANCIÓN y en orden de id del driver (`audio/tracklist.ts`),
  //    no por contexto: tres de los nombres-por-contexto viejos resultaron falsos al leer
  //    `mid.drv` (RULEBRIT no es el castillo, HORNPIPE no es la taberna).
  { ruta: "music/fat.opl", de: "FAT.OPL (banco de timbres AdLib del parche)", opcional: true },
  { ruta: "music/theme.mid", de: "U5THEME.XMI (parche)", opcional: true },
  { ruta: "music/britannia.mid", de: "BRITLAND.XMI (parche)", opcional: true },
  { ruta: "music/hornpipe.mid", de: "HORNPIPE.XMI (parche)", opcional: true },
  { ruta: "music/engagement.mid", de: "ENGGMNT.XMI (parche)", opcional: true },
  { ruta: "music/stones.mid", de: "STONES.XMI (parche)", opcional: true },
  { ruta: "music/greyson.mid", de: "GREYSON.XMI (parche)", opcional: true },
  { ruta: "music/fanfare.mid", de: "FANFARE.XMI (parche)", opcional: true },
  { ruta: "music/monarch.mid", de: "MONARCH.XMI (parche)", opcional: true },
  { ruta: "music/tarantella.mid", de: "TRNTLLA.XMI (parche)", opcional: true },
  { ruta: "music/halls.mid", de: "HALLS.XMI (parche)", opcional: true },
  { ruta: "music/worlds-below.mid", de: "WRLDBLW.XMI (parche)", opcional: true },
  { ruta: "music/blackthorn.mid", de: "BLCKTHRN.XMI (parche)", opcional: true },
  { ruta: "music/ladynan.mid", de: "LADYNAN.XMI (parche)", opcional: true },
  { ruta: "music/reunion.mid", de: "REUNION.XMI (parche)", opcional: true },
  { ruta: "music/rule-britannia.mid", de: "RULEBRIT.XMI (parche)", opcional: true },
  { ruta: "music/amiga.mid", de: "AMIGA.XMI (parche)", opcional: true },
  { ruta: "proport-font.png", de: "PROPORT.PCS", opcional: true },
  { ruta: "proport-font.json", de: "PROPORT.PCS", opcional: true },
];

/**
 * Las rutas cuya AUSENCIA en una extracción significa «esa extracción es vieja» — las que
 * salen sólo de ficheros OBLIGATORIOS del original. Es la lista contra la que carea el
 * arranque del juego.
 */
export const ASSETS_EXIGIDOS: readonly string[] = ASSETS_DE_EXTRACCION.filter(
  (a) => !a.opcional,
).map((a) => a.ruta);

/** Índice ruta → ficha, para que el aviso pueda decir DE QUÉ fichero del original sale. */
export const ASSET_POR_RUTA: ReadonlyMap<string, AssetDeExtraccion> = new Map(
  ASSETS_DE_EXTRACCION.map((a) => [a.ruta, a]),
);

/**
 * Careo del catálogo contra lo que se emitió DE VERDAD. La llama `runPipeline` al acabar.
 *
 * Las dos direcciones importan y fallan por motivos distintos:
 *  - emitido-y-no-catalogado: alguien añadió un asset y no lo registró ⇒ el aviso del
 *    arranque nunca lo echaría de menos y volvemos al silencio de #293. Es EL caso.
 *  - catalogado-y-no-emitido: el pipeline dejó de producir algo que el catálogo promete
 *    ⇒ las extracciones nuevas nacerían cojas y el aviso acusaría a extracciones sanas.
 *
 * Se lanza en vez de registrar: una extracción que no cuadra con su propio catálogo no
 * debe llegar a la caché de nadie, y la condición NO depende de los datos del usuario
 * (el conjunto de rutas posibles son literales del pipeline), así que esto se dispara en
 * la primera corrida de quien lo rompe, jamás en el navegador de un visitante.
 */
export function verificaCatalogo(
  emitidos: ReadonlySet<string>,
  opts: { readonly skipTiles?: boolean } = {},
): void {
  const huerfanos = [...emitidos].filter((r) => !ASSET_POR_RUTA.has(r)).sort();
  if (huerfanos.length > 0) {
    throw new Error(
      `extracción: ${huerfanos.length} asset(s) emitidos y NO catalogados: ${huerfanos.join(", ")}. ` +
        `Añádelos a extractor/src/assets-catalog.ts EN ESTE COMMIT — el arranque del juego ` +
        `carea esa lista para avisar a quien tenga una extracción vieja (#293).`,
    );
  }
  const ausentes = ASSETS_DE_EXTRACCION.filter((a) => {
    if (emitidos.has(a.ruta)) return false;
    if (a.opcional) return false; // depende de ficheros que la copia puede no traer
    if (a.conTiles && opts.skipTiles) return false; // apagado a propósito por la CLI
    return true;
  }).map((a) => a.ruta);
  if (ausentes.length > 0) {
    throw new Error(
      `extracción: el catálogo promete ${ausentes.length} asset(s) que el pipeline NO emitió: ` +
        `${ausentes.join(", ")}. O el pipeline dejó de producirlos, o su línea del catálogo ` +
        `está de más (extractor/src/assets-catalog.ts).`,
    );
  }
}
