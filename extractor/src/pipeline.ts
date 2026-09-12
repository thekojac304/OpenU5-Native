/**
 * PIPELINE COMPARTIDO del extractor: la secuencia completa de extracción,
 * parametrizada por sinks de E/S para poder correr igual en Node (cli.ts)
 * que en el navegador (browser.ts, demo BYO-files).
 *
 * REGLA: aquí no entra ningún import de `node:*` ni de pngjs — solo parsers
 * puros (Uint8Array → objetos/RGBA). La escritura real (fs / Cache Storage)
 * la ponen los wrappers vía PipelineIO.
 */
import { decompressLzw } from "./parsers/lzw.js";
import { parseTiles, tilesToAtlasRgba } from "./parsers/tiles.js";
import { dngViewsToAtlas, parseDngView, parseItemsView } from "./parsers/dngtiles.js";
import { parseMonView } from "./parsers/monview.js";
import { fontToAtlasRgba, parseFont } from "./parsers/font.js";
import { extAtlasRgba } from "./font-ext.js";
import { xbr4x } from "./upscale/xbr.js";
import { parseLargeMap } from "./parsers/largemap.js";
import { parseSmallMaps } from "./parsers/smallmap.js";
import { parseDungeons } from "./parsers/dungeon.js";
import { parseCombatMaps } from "./parsers/combatmap.js";
import { parseDataOvl } from "./parsers/dataovl.js";
import { parseAllNpcs } from "./parsers/npc.js";
import { parseSaveGame } from "./parsers/savegame.js";
import { parseSigns } from "./parsers/signs.js";
import { parseLook2 } from "./parsers/look2.js";
import { parseQuestions } from "./parsers/questions.js";
import { parseStory } from "./parsers/story.js";
import { parseIntroScenes } from "./parsers/intro-scenes.js";
import { parseBritishPath } from "./parsers/pth.js";
import { parseDemoScene } from "./parsers/demo-scene.js";
import { parseEndgameScene, parseEndgameArts } from "./parsers/endgame-scene.js";
import { parseShrineScenes } from "./parsers/shrine-scene.js";
import { parseEndgameNarration, parseEndgameDialogue } from "./parsers/endgame-msg.js";
import { parseDsStrings } from "./parsers/ds-strings.js";
import { packPic16Atlas, parsePic16, type Pic16Image } from "./parsers/pic16.js";
import { bitToPixels, parseBit } from "./parsers/bit.js";
import { packProportFont, parseProport } from "./parsers/proport.js";
import { EGA_PALETTE } from "./parsers/tiles.js";
import { extractCompressedWords, parseTlkFile } from "./parsers/tlk.js";
import { parseShoppeDat } from "./parsers/shoppe.js";
import { verificaCatalogo } from "./assets-catalog.js";
import { xmiToMidi } from "./audio/xmi2midi.js";
import { TRACKS } from "./audio/tracklist.js";

/** E/S que aporta cada wrapper (Node o navegador). Rutas relativas al out. */
export interface PipelineIO {
  /** Lee un fichero del directorio de datos originales del usuario. */
  read(name: string): Uint8Array;
  /** ¿Existe el fichero original opcional? */
  exists(name: string): boolean;
  putJson(path: string, value: unknown): void | Promise<void>;
  putPng(path: string, rgba: Uint8Array, width: number, height: number): void | Promise<void>;
  putBin(path: string, bytes: Uint8Array): void | Promise<void>;
  log(message: string): void;
}

export interface PipelineOptions {
  skipTiles?: boolean;
}

/** Ficheros imprescindibles con su tamaño exacto esperado (validación de entrada). */
export const REQUIRED_FILES: Record<string, number | null> = {
  "TILES.16": null,
  "IBM.CH": 1024,
  "RUNES.CH": 1024,
  "BRIT.DAT": null,
  "UNDER.DAT": 65536,
  "TOWNE.DAT": 16384,
  "CASTLE.DAT": 16384,
  "DWELLING.DAT": 16384,
  "KEEP.DAT": 16384,
  "DUNGEON.DAT": 4096,
  "BRIT.CBT": 5632,
  "DUNGEON.CBT": 39424,
  "DATA.OVL": null,
  "TOWNE.NPC": 4608,
  "CASTLE.NPC": 4608,
  "DWELLING.NPC": 4608,
  "KEEP.NPC": 4608,
  "TOWNE.TLK": null,
  "CASTLE.TLK": null,
  "DWELLING.TLK": null,
  "KEEP.TLK": null,
  "INIT.GAM": null,
  "SIGNS.DAT": null,
  "SHOPPE.DAT": null,
  "LOOK2.DAT": null,
  "MISCMAPS.DAT": 1871,
  "END.DAT": 3698,
  "ENDMSG.DAT": 786,
  // KARMA.DAT y MISCMSG.DAT entran el 25-08 con `ds-strings.json` (FICHA β): son los
  // mensajes que el original carga al búfer DS 0xB21E y los ÚNICOS ficheros de texto del
  // juego que no extraía nadie — por eso sus cadenas seguían transcritas a mano en
  // `game/src/**` (638 de las 653 palabras de prosa de EA que el árbol público servía,
  // medidas en `re/notes/acta-630-prosa-publicada.md`).
  // Llevan tamaño exacto —a diferencia de los `null` de abajo— porque el motivo de
  // aquellos no aplica: no son ficheros con variación legítima conocida entre ediciones,
  // son dos blobs de texto fijo de la misma edición DOS contra la que se verificó el port,
  // y su troceado por NUL es lo que fija los índices de registro que cita el manifiesto de
  // fidelidad. Si una edición trajera otros tamaños, `inspectSourceFiles` lo DICE (con la
  // cifra de las dos) en vez de emitir un asset con los registros corridos en silencio.
  "KARMA.DAT": 761,
  "MISCMSG.DAT": 2745,
  // 🔴 ESTOS TRES ENTRARON EL 07-08 Y NO SON UN AÑADIDO: ERAN UN AGUJERO.
  // `runPipeline` los lee con `read()` **sin guarda de existencia** (líneas 361, 370/373
  // y 378) y sin `try` alrededor — el `try` más cercano empieza en la 402. Faltando
  // cualquiera de los tres, la carpeta **pasaba la validación entera**, llegaba al estado
  // «listo» de /byo y reventaba después con un fallo genérico («falta QUESTION.DAT») sin
  // lista accionable. O sea: **el estado de ÉXITO mintiendo**, en la única pantalla cuyo
  // trabajo es decirte qué te falta ANTES de intentarlo.
  // El discriminante que los separa de los otros tres que también se leen fuera de la
  // tabla: `ITEMS.16`, `BRIT.OOL` y `UNDER.OOL` están guardados por `exists()`, y
  // `PROPORT.PCS` por un `try/catch` que registra «ausente, omitida». Ésos SÍ son
  // opcionales de verdad y por eso NO entran aquí.
  // Van a `null` A PROPÓSITO: fijarles el tamaño de UNA copia no es medir, es una
  // derivación con un supuesto dentro, y convertiría variación legítima entre ediciones
  // en acusaciones falsas (ficha abierta en docs/publicacion/web/TRASPASO-BYO.md §1).
  "QUESTION.DAT": null,
  "STORY.DAT": null,
  "BRITISH.PTH": null,
};

/** Un fichero exigido que ESTÁ pero no mide lo que esperábamos. */
export interface SizeMismatch {
  name: string;
  /** Lo que mide la copia del usuario. */
  size: number;
  /** Lo que mide la edición DOS contra la que se verificó el port. */
  expected: number;
}

/**
 * Resultado ESTRUCTURADO de mirar la carpeta del usuario.
 *
 * 🔴 POR QUÉ ESTO EXISTE Y NO BASTA `validateSourceFiles`: esa función emite las dos
 * clases de problema —AUSENTE y PRESENTE-CON-OTRO-TAMAÑO— ya distinguidas, pero
 * FUNDIDAS en una lista de cadenas en castellano. Quien quiera decidir sobre la
 * distinción (la interfaz de /byo: «te faltan ficheros» es otro problema, con otro
 * siguiente paso, que «esto es otra edición») tendría que hacer parsing de prosa. Se
 * devuelven los datos; el rótulo lo pone quien pinta.
 *
 * 🔴 Y LA DISCRIMINACIÓN ES PARCIAL — por eso `medibles` viaja en el resultado y no
 * se calcula fuera: sólo 17 de los 31 exigidos llevan tamaño esperado; los otros 14
 * son `null` y NINGUNA discrepancia suya es detectable. Una edición que difiera sólo
 * en esos catorce pasa esta comprobación entera. Todo rótulo construido sobre
 * `sizeMismatch` tiene que poder decir sobre CUÁNTOS ficheros se pronunció, y ese
 * número sale de aquí para que no pueda quedarse rancio si algún día se rellenan.
 * (Rellenarlos NO es medir: ver la ficha abierta en docs/publicacion/web/TRASPASO-BYO.md
 * §1 — si esos ficheros varían legítimamente entre ediciones, fijarlos con los de UNA
 * copia convierte variación normal en acusaciones falsas.)
 */
export interface SourceInspection {
  /** Exigidos que NO están. */
  missing: string[];
  /** Presentes con otro tamaño. Sólo puede contener ficheros de los `medibles`. */
  sizeMismatch: SizeMismatch[];
  /** Exigidos presentes y sin discrepancia (el numerador del «N de M»). */
  correctos: number;
  /** Cuántos ficheros exige el extractor (el denominador). */
  total: number;
  /** De los exigidos, sobre cuántos SE PUEDE afirmar algo del tamaño. */
  medibles: number;
  /** Los mismos problemas de `validateSourceFiles`, en el mismo orden. */
  problems: string[];
}

/** Mira el origen y devuelve qué falta y qué no mide lo esperado. */
export function inspectSourceFiles(io: Pick<PipelineIO, "read" | "exists">): SourceInspection {
  const entries = Object.entries(REQUIRED_FILES);
  const missing: string[] = [];
  const sizeMismatch: SizeMismatch[] = [];
  const problems: string[] = [];
  for (const [name, expectedSize] of entries) {
    if (!io.exists(name)) {
      missing.push(name);
      problems.push(`falta ${name}`);
      continue;
    }
    const size = io.read(name).length;
    if (expectedSize !== null && size !== expectedSize) {
      sizeMismatch.push({ name, size, expected: expectedSize });
      problems.push(`${name}: tamaño ${size}, esperado ${expectedSize}`);
    }
  }
  return {
    missing,
    sizeMismatch,
    correctos: entries.length - missing.length - sizeMismatch.length,
    total: entries.length,
    medibles: entries.filter(([, s]) => s !== null).length,
    problems,
  };
}

/**
 * Valida que el origen tiene los ficheros DOS de Ultima V. Devuelve la lista
 * de problemas (vacía = válido) para que el wrapper decida cómo informar.
 *
 * Se deriva de `inspectSourceFiles` para que las dos vistas no puedan divergir: el
 * orden y el texto de los problemas son los de siempre (la CLI los imprime).
 */
export function validateSourceFiles(io: Pick<PipelineIO, "read" | "exists">): string[] {
  return inspectSourceFiles(io).problems;
}

/**
 * Envuelve el sink para APUNTAR cada ruta escrita. La lista de assets de una extracción
 * no se mantiene a mano en ningún sitio: se deriva de aquí (ficha #293). El envoltorio no
 * toca los bytes ni el orden — sólo añade la ruta a `emitidos` antes de delegar.
 */
function registraEmisiones(io: PipelineIO, emitidos: Set<string>): PipelineIO {
  return {
    read: (name) => io.read(name),
    exists: (name) => io.exists(name),
    log: (message) => io.log(message),
    putJson: (path, value) => {
      emitidos.add(path);
      return io.putJson(path, value);
    },
    putPng: (path, rgba, width, height) => {
      emitidos.add(path);
      return io.putPng(path, rgba, width, height);
    },
    putBin: (path, bytes) => {
      emitidos.add(path);
      return io.putBin(path, bytes);
    },
  };
}

/**
 * Corre la extracción completa, música INCLUIDA. La música dejó de vivir en el wrapper
 * de Node cuando dejó de renderizarse a audio: ahora son datos (MIDI + banco de timbres)
 * y los sintetiza el juego, así que corre igual aquí que en el navegador de /byo.
 */
export async function runPipeline(ioReal: PipelineIO, opts: PipelineOptions = {}): Promise<void> {
  const emitidos = new Set<string>();
  const io = registraEmisiones(ioReal, emitidos);
  const { read } = io;

  // 1. Tiles
  if (!opts.skipTiles) {
    io.log("• Tiles…");
    const tiles = parseTiles(decompressLzw(read("TILES.16")));
    const ega = tilesToAtlasRgba(tiles, 32);
    await io.putPng("tiles-ega.png", ega.rgba, ega.width, ega.height);
    // HD: upscalear cada tile por separado (sin sangrado entre vecinos) y componer a mano
    const hdTiles = tiles.map((t) => xbr4x(t, 16, 16).rgba);
    const cols = 32;
    const rows = Math.ceil(hdTiles.length / cols);
    const hdW = cols * 64;
    const hdH = rows * 64;
    const hdRgba = new Uint8Array(hdW * hdH * 4);
    hdTiles.forEach((tile, i) => {
      const tx = (i % cols) * 64;
      const ty = Math.floor(i / cols) * 64;
      for (let y = 0; y < 64; y++) {
        const srcOff = y * 64 * 4;
        const dstOff = ((ty + y) * hdW + tx) * 4;
        hdRgba.set(tile.subarray(srcOff, srcOff + 64 * 4), dstOff);
      }
    });
    await io.putPng("tiles-hd.png", hdRgba, hdW, hdH);

    // 1a. Pack de PERSPECTIVA de mazmorra (DNG1/2/3.16): rodajas de pared en
    // perspectiva del renderer first-person (dnglook-raster-spec.md §3, task
    // #31). Tres variantes de textura (oliva/rojiza/gris). Se emite un atlas +
    // JSON de rects. Guardado por existencia: si faltan, se avisa y se omite.
    const dngFiles = ["DNG1.16", "DNG2.16", "DNG3.16"] as const;
    if (dngFiles.every((f) => io.exists(f))) {
      io.log("• Perspectiva de mazmorra (DNG*.16)…");
      const variants = dngFiles.map((f) => ({
        name: f.replace(/\.16$/i, "").toLowerCase(),
        images: parseDngView(decompressLzw(read(f))),
      }));
      const atlas = dngViewsToAtlas(variants);
      await io.putPng("dungeon-persp.png", atlas.rgba, atlas.width, atlas.height);
      await io.putJson("dungeon-persp.json", atlas.meta);
    } else {
      io.log("• Perspectiva de mazmorra… (DNG*.16 ausentes, omitido)");
    }

    // 1b. Pack de FEATURES de mazmorra (ITEMS.16, banco a9c4): 20 imágenes REALES
    // (escalera 0-3 / fuente 4-7 / trampa 8-11 / cofre 12-15 / cofre abierto
    // 16-19, cada una a 4 profundidades; dungeon3d-audit §8). Contenedor de 20
    // PARES u16 (offImagen, offMáscara-AND) como MON*.16; la transparencia va por
    // MÁSCARA (horneada en el alpha por parseItemsView), no por color-0.
    if (io.exists("ITEMS.16")) {
      io.log("• Features de mazmorra (ITEMS.16)…");
      const feats = parseItemsView(decompressLzw(read("ITEMS.16")));
      const atlas = dngViewsToAtlas([{ name: "items", images: feats }]);
      await io.putPng("dungeon-feat.png", atlas.rgba, atlas.width, atlas.height);
      await io.putJson("dungeon-feat.json", atlas.meta);
    } else {
      io.log("• Features de mazmorra… (ITEMS.16 ausente, omitido)");
    }

    // 1c. Pack del MONSTRUO ERRANTE 3D (MON0-7.16): 8 bancos de sprite del
    // errante de pasillo (DUNGEON 0x0151-0x01C0: rand(0,7) elige banco, carga
    // perezosa vía tabla DS 0x25FA; atributos DS 0x173C/0x1744). Cada banco =
    // 2 frames × 3 profundidades con AND-mask 1bpp (ver parsers/monview.ts).
    const monFiles = Array.from({ length: 8 }, (_, i) => `MON${i}.16`);
    if (monFiles.every((f) => io.exists(f))) {
      io.log("• Monstruo errante de mazmorra (MON0-7.16)…");
      const banks = monFiles.map((f) => ({
        name: f.replace(/\.16$/i, "").toLowerCase(),
        images: parseMonView(decompressLzw(read(f))),
      }));
      const atlas = dngViewsToAtlas(banks);
      await io.putPng("dungeon-mon.png", atlas.rgba, atlas.width, atlas.height);
      await io.putJson("dungeon-mon.json", atlas.meta);
    } else {
      io.log("• Monstruo errante… (MON*.16 ausentes, omitido)");
    }
  } else {
    io.log("• Tiles… (omitido)");
  }

  // 1b. Fuente de juego IBM.CH → atlas 128×64 (16×8 glifos, blanco/transparente).
  // La usa la CAPA DE TEXTO de la piel fiel (consola, chrome). Barato: siempre.
  io.log("• Fuente IBM.CH…");
  {
    const glyphs = parseFont(read("IBM.CH"));
    const font = fontToAtlasRgba(glyphs, 16);
    await io.putPng("font-ibm.png", font.rgba, font.width, font.height);

    // 1b-bis. ATLAS DE EXTENSIÓN LATINO-1 (#339). NO es cosmético: sin él la piel fiel
    // enmascara el codepoint con `code & 0x7f` y los acentos caen en OTRA LETRA — «é»
    // sale «i», «ñ» sale «q», «montaña» → «montaqa». Afecta al 19,5 % de las cadenas
    // del corpus ES. Se emite SIEMPRE y junto a la base porque es composición pura
    // sobre ella (la letra del juego + el signo), sin refinado y sin pedir ningún
    // fichero más del original. Ver `font-ext.ts` para el porqué y para el careo con
    // el productor histórico `docs/skin-remaster/shader-evolucion/font_ext.py`.
    const ext = extAtlasRgba(glyphs);
    await io.putPng("font-ibm-ext.png", ext.rgba, ext.width, ext.height);
  }

  // 1c. Fuente RÚNICA RUNES.CH (mismo formato 128×8 que IBM.CH) → font-runes.png.
  // Es la FUENTE 1 del kernel (`0x1c9e(1)`): la banda celeste imprime el sol
  // (glifo 0x2A = ráfaga de 8 rayos) y las lunas (0x30-0x37 = las 8 FASES, círculos
  // menguantes/crecientes) con ESTA fuente, no con IBM.CH. El scout marcó por error
  // "dígitos IBM"; la rutina 0x4ac0 conmuta a la fuente 1 (RUNES) antes del bucle de
  // impresión 0x4b70 y restaura la 0 (IBM) al salir. Ver ui-text-layer.md §9.
  io.log("• Fuente RUNES.CH…");
  {
    const runes = fontToAtlasRgba(parseFont(read("RUNES.CH")), 16);
    await io.putPng("font-runes.png", runes.rgba, runes.width, runes.height);
  }

  // 2. DATA.OVL (necesario para overlay del overworld y palabras comprimidas)
  io.log("• DATA.OVL…");
  const dataOvlBytes = read("DATA.OVL");
  const dataOvl = parseDataOvl(dataOvlBytes);
  await io.putJson("data.json", dataOvl);

  // 3. Mapas grandes
  io.log("• Overworld/Underworld…");
  const overlay = dataOvlBytes.subarray(0x3886, 0x3986);
  await io.putJson("maps/overworld.json", parseLargeMap(read("BRIT.DAT"), overlay));
  await io.putJson("maps/underworld.json", parseLargeMap(read("UNDER.DAT"), null));

  // 4. Small maps
  io.log("• Pueblos…");
  await io.putJson(
    "maps/smallmaps.json",
    parseSmallMaps({
      castle: read("CASTLE.DAT"),
      towne: read("TOWNE.DAT"),
      dwelling: read("DWELLING.DAT"),
      keep: read("KEEP.DAT"),
    }),
  );

  // 5. Dungeons + combate
  io.log("• Mazmorras y mapas de combate…");
  await io.putJson("maps/dungeons.json", parseDungeons(read("DUNGEON.DAT")));
  await io.putJson(
    "maps/combatmaps.json",
    parseCombatMaps(read("BRIT.CBT"), read("DUNGEON.CBT")),
  );

  // 6. NPCs
  io.log("• NPCs…");
  await io.putJson(
    "npcs.json",
    parseAllNpcs({
      castle: read("CASTLE.NPC"),
      towne: read("TOWNE.NPC"),
      dwelling: read("DWELLING.NPC"),
      keep: read("KEEP.NPC"),
    }),
  );

  // 7. TLK
  io.log("• Diálogos…");
  const words = extractCompressedWords(dataOvlBytes);
  for (const master of ["castle", "towne", "dwelling", "keep"] as const) {
    await io.putJson(
      `talk/${master}.json`,
      parseTlkFile(read(`${master.toUpperCase()}.TLK`), words),
    );
  }
  // Diálogos de mercader (SHOPPE.DAT, mismo diccionario comprimido que los .TLK)
  await io.putJson("shoppe.json", parseShoppeDat(read("SHOPPE.DAT"), words));

  // 8. Estado inicial
  io.log("• INIT.GAM…");
  const initGamBytes = read("INIT.GAM");
  await io.putJson("initial-state.json", parseSaveGame(initGamBytes));
  // Plantilla base de 4192 B para el guardado NATIVO (task #27): el serializer
  // parchea los campos modelados sobre esta base y preserva los bytes oscuros
  // (tabla de objetos 0x6B4, char-states 0x9B8, movement lists…). Se sirve como
  // /assets/init.gam y el runtime la carga como plantilla del export SAVED.GAM.
  await io.putBin("init.gam", initGamBytes);
  // Plantilla del SAVED.OOL (512 B = BRIT.OOL ++ UNDER.OOL, formato derivado en
  // re/notes/oracle-pending-sweep.md Objetivo B: un SAVED.OOL recién iniciado es
  // byte-idéntico a esta concatenación). La consume buildNativeOol (saveNative.ts)
  // como siembra del export nativo; guardado por existencia (no son REQUIRED).
  if (io.exists("BRIT.OOL") && io.exists("UNDER.OOL")) {
    const brit = read("BRIT.OOL");
    const under = read("UNDER.OOL");
    const ool = new Uint8Array(0x200);
    ool.set(brit.subarray(0, 0x100), 0);
    ool.set(under.subarray(0, 0x100), 0x100);
    await io.putBin("init.ool", ool);
  } else {
    io.log("• init.ool… (BRIT.OOL/UNDER.OOL ausentes, omitido)");
  }

  // 8b. Carteles (SIGNS.DAT + cartel extra de DATA.OVL)
  io.log("• Carteles…");
  await io.putJson("signs.json", parseSigns(read("SIGNS.DAT"), dataOvlBytes));

  // 8c. Cuestionario de la gitana (creación de personaje)
  io.log("• QUESTION.DAT…");
  await io.putJson("questions.json", parseQuestions(read("QUESTION.DAT")));

  // 8d. Frases de (L)ook por tile (LOOK2.DAT → "Thou dost see <frase>")
  io.log("• LOOK2.DAT…");
  await io.putJson("look2.json", parseLook2(read("LOOK2.DAT")));

  // 8e. Guion de la cinemática de introducción (The Summoning + The Story) —
  // páginas de texto EXACTO del binario para la piel fiel (E1-S13).
  io.log("• STORY.DAT…");
  await io.putJson("story.json", parseStory(read("STORY.DAT")));
  // Tablas de escena de The Summoning (21 escenas: fondo STORYn.16 + texto +
  // layout). Derivado+citado (intro-scene-tables.md), E1-S13c Etapa 2.
  await io.putJson("intro-scenes.json", parseIntroScenes(dataOvlBytes, read("STORY.DAT")));

  // 8f. Rutas de las figuras andantes del attract del título (BRITISH.PTH) —
  // codec derivado de path_walk_anim (INTRO.OVL 0x50), E1-S13b.
  io.log("• BRITISH.PTH…");
  await io.putJson("british-path.json", parseBritishPath(read("BRITISH.PTH")));

  // 8f-bis. Escena del DEMO del attract (MISCMAPS.DAT[704:]) — el cine-guion de 4
  // escenas que el motor reproduce en el hueco del menú (task #46 Stage 3;
  // re/notes/demo-scene-data.md). Mapas + bytecode + tablas de dirección.
  io.log("• Demo scene (MISCMAPS.DAT)…");
  await io.putJson("demo-scene.json", parseDemoScene(read("MISCMAPS.DAT"), read("DATA.OVL")));

  // 8f-quinquies. Mapas de las escenas de SANTUARIO (MISCMAPS.DAT[176:352]) y CÁMARA DEL
  // CODEX ([352:528]) — los dos registros que el (E)nter carga en CAST2 0x0ed2-0x0ef6 y que
  // este pipeline saltaba (ya leía el mismo fichero para el demo 704 y el endgame 528).
  // Ficha #277. Sólo la rejilla; la escena la deriva core/world/shrine-scene.ts.
  io.log("• Shrine/Codex scenes (MISCMAPS.DAT)…");
  await io.putJson("shrine-scene.json", parseShrineScenes(read("MISCMAPS.DAT")));

  // 8f-ter. Secuencia final del juego (task #20): el mapa de la sala
  // (MISCMAPS.DAT[528:704]), la narración de cierre (END.DAT, 6 páginas) y el diálogo
  // del trono con el fork de la caja (ENDMSG.DAT). Ver .superpowers/sdd/brief-endgame.md
  // + re/notes/endgame.md. Sólo datos derivados; el render/fork es de la piel/core.
  io.log("• Endgame (MISCMAPS/END.DAT/ENDMSG.DAT)…");
  await io.putJson("endgame.json", {
    scene: parseEndgameScene(read("MISCMAPS.DAT")),
    narration: parseEndgameNarration(read("END.DAT")),
    dialogue: parseEndgameDialogue(read("ENDMSG.DAT")),
  });

  // 8f-sexies. Mensajes del búfer DS 0xB21E (KARMA/MISCMSG/ENDMSG.DAT) — FICHA β de
  // `re/notes/acta-380-i18n-en-claro.md`. Es el prerrequisito que faltaba para que los
  // discursos de resurrección, el interrogatorio de Blackthorn y la máxima del santuario
  // dejen de ser literales TRANSCRITOS en `game/src/**`: extraídos aquí, el port los lee
  // del juego del propio usuario, como ya hace con los TLK. ENDMSG.DAT va también aquí
  // —además de dentro de `endgame.json`, donde vive con su escena y su narración— para
  // que el registro-a-registro que consume el core tenga UNA sola puerta y un solo
  // formato; son 786 B.
  io.log("• Mensajes DS 0xB21E (KARMA/MISCMSG/ENDMSG.DAT)…");
  await io.putJson("ds-strings.json", parseDsStrings(read));

  // 8f-quater. Láminas EGA de las pantallas de historia del cierre (GAP 6): la casa del
  // Avatar (END1.16), el sueño de Blackthorn (END2.16) y el fondo del pergamino
  // (ENDSC.16) — ficheros `.16` LZW de sub-imágenes 4bpp (formato pic16, EXTRAÍDO no
  // redibujado). → atlas endgame-scenes.png + manifiesto que la piel blitea bajo las
  // páginas de END.DAT (espejo del compositor STORYn.16 de la intro).
  try {
    const endArts: { name: string; image: Pic16Image }[] = [];
    for (const art of parseEndgameArts(read)) {
      art.images.forEach((image, i) => {
        if (image.width > 0 && image.height > 0 && image.width <= 320 && image.height <= 200) {
          endArts.push({ name: `${art.name}:${i}`, image });
        }
      });
    }
    if (endArts.length > 0) {
      const atlas = packPic16Atlas(endArts, EGA_PALETTE);
      await io.putPng("endgame-scenes.png", atlas.rgba, atlas.width, atlas.height);
      await io.putJson("endgame-scenes.json", { entries: atlas.entries });
    }
  } catch {
    io.log("  (END1/END2/ENDSC.16 ausentes u omitidas)");
  }

  // 8g. Láminas de la cinemática (.16 = archivo LZW de sub-imágenes 4bpp) —
  // retrato de la gitana + pebeteros/símbolos de virtud (CREATE.16) y escenas de
  // The Summoning (STORY*.16). Decoder derivado (pic16.ts), E1-S13c. → atlas +
  // manifiesto que la piel fiel blitea.
  io.log("• Láminas de intro (.16)…");
  {
    const named: { name: string; image: Pic16Image }[] = [];
    const addFile = (file: string, tag: string): void => {
      try {
        const imgs = parsePic16(decompressLzw(read(file)));
        imgs.forEach((image, i) => {
          if (image.width > 0 && image.height > 0 && image.width <= 320 && image.height <= 200) {
            named.push({ name: `${tag}:${i}`, image });
          }
        });
      } catch {
        io.log(`  (${file} ausente u omitida)`);
      }
    };
    addFile("CREATE.16", "create");
    // Marcos/cartones de título de la Summoning (TYPE 1, escenas 0/7/14): TEXT.16
    // se blitea con el mismo cargador .16 (intro-scene-tables.md §3, TYPE 1).
    addFile("TEXT.16", "text16");
    for (let s = 1; s <= 6; s++) addFile(`STORY${s}.16`, `story${s}`);
    addFile("ULTIMA.16", "ultima"); // ultima:0 = logo gótico "Ultima V" (319×61)
    // Acknowledgements (INTRO play_acknowledgements 0x072E): STARTSC.16 = 3 sub-
    // imágenes → startsc:1 (288×137) el pergamino de créditos con el texto HORNEADO
    // ("Produced and Designed by Lord British" …), y startsc:0 / startsc:2 (16×137
    // c/u) los dos rodillos del pergamino que se despliegan hacia los lados.
    addFile("STARTSC.16", "startsc");
    // Logos de arranque (.BIT = máscaras 1bpp): ORIGIN SYSTEMS (TITLE.BIT, la
    // última sub-imagen = el logo a tamaño completo) + "Lord British" (BRITISH.BIT).
    // Video-diff E1. Decoder bit.ts (E1-S13c).
    // `onIndex` = color EGA horneado de la máscara. La máscara .BIT es 1bpp y el
    // blit del original (EGA.DRV 0x190e, SEL 0x4e) pone el pixel en los 4 planos
    // (índice 15); el COLOR MOSTRADO lo fija la paleta EGA activa en cada escena
    // (INTRO reprograma registros de paleta, intro.md §226-227). Medido en el
    // vídeo P del usuario (task #74): ORIGIN SYSTEMS = azul brillante (idx 9 =
    // 0x5555FF; frames f005-f012), "Lord British" = blanco (idx 15; frame f022).
    const addBit = (
      file: string,
      name: string,
      onIndex: number,
      pick: (imgs: ReturnType<typeof parseBit>) => number,
    ): void => {
      try {
        const imgs = parseBit(decompressLzw(read(file)));
        if (imgs.length === 0) return;
        const idx = pick(imgs);
        if (idx < 0 || idx >= imgs.length) return;
        named.push({ name, image: bitToPixels(imgs[idx]!, onIndex) });
      } catch {
        io.log(`  (${file} ausente u omitida)`);
      }
    };
    // ORIGIN SYSTEMS INC. = la sub-imagen MÁS ANCHA de TITLE.BIT (el logo a tamaño
    // completo; las menores son la animación de zoom del arranque). Azul (idx 9).
    addBit("TITLE.BIT", "origin", 9, (imgs) => {
      let best = 0;
      for (let i = 1; i < imgs.length; i++) if (imgs[i]!.width > imgs[best]!.width) best = i;
      return best;
    });
    // Textos GÓTICOS BLANCOS de los cartones de arranque: sub-imágenes de TITLE.BIT
    // que INTRO.OVL blitea APARTE del logo (task #67, NO son PROPORT.PCS — la fuente
    // proporcional es slab-serif; los cartones usan estos bitmaps blackletter). El
    // blit es `blit(y, x, subIndex, buffer)` (kernel 0xffff8e84): INTRO.OVL.asm
    //   0x0b6f push 7  → sub 7 "Presents"   (104×33)  en (y=140, x=108)
    //   0x0bc6 push 8  → sub 8 "a"          (16×15)   en (y=0,   x=152)
    //   0x0c64 push 9  → sub 9 "Production" (112×33)  en (y=160, x=104)
    // Blancas (idx 15; witness PRESENTS_ORIGINAL / LB_PRODUCTION_ORIGINAL). Las posiciones
    // salen centradas en x=160 (pantalla 320) — confirma el orden (y,x) de los args.
    addBit("TITLE.BIT", "presents", 15, () => 7);
    addBit("TITLE.BIT", "card-a", 15, () => 8);
    addBit("TITLE.BIT", "production", 15, () => 9);
    // "Lord British" (firma cursiva) = blanco (idx 15) en el vídeo P.
    addBit("BRITISH.BIT", "lordbritish", 15, () => 0);
    if (named.length > 0) {
      const atlas = packPic16Atlas(named, EGA_PALETTE);
      await io.putPng("intro-pics.png", atlas.rgba, atlas.width, atlas.height);
      await io.putJson("intro-pics.json", { entries: atlas.entries });
    }
  }

  // 8h. Fuente PROPORCIONAL de las cinemáticas (PROPORT.PCS → 91 glifos 1bpp de
  // alto 8 y ancho variable). Decoder proport.ts (E1-S13c). → atlas + manifiesto.
  io.log("• PROPORT.PCS…");
  try {
    const font = packProportFont(parseProport(decompressLzw(read("PROPORT.PCS"))));
    await io.putPng("proport-font.png", font.rgba, font.width, font.height);
    await io.putJson("proport-font.json", { height: font.height, glyphs: font.glyphs });
  } catch {
    io.log("  (PROPORT.PCS ausente, omitida)");
  }

  // 9. MÚSICA DEL PARCHE (Ultima V Upgrade Patch, de Voyager Dragon) — OPCIONAL.
  //
  // NO se renderiza a audio: se copian los DATOS y el juego los sintetiza en vivo con su
  // emulador de OPL (`game/src/ui/opl/`). Por eso este paso vive en el pipeline COMPARTIDO
  // y no en el wrapper de Node, que es donde estaba: la vía OGG anterior necesitaba
  // fluidsynth, ffmpeg y un soundfont, era Node-only, y por eso la demo del navegador no
  // podía tener música NUNCA. Aquí son dos lecturas y un cambio de formato, en cualquier
  // sitio donde corra el pipeline.
  //
  // 🔴 LOS XMI ESTÁN EN DOS SITIOS SEGÚN LA COPIA: sueltos en el directorio del juego, o
  // bajo `upgrade/`, según cómo se aplicara el parche. Se prueban los dos — mirar sólo uno
  // se salda con «no hay música» en la mitad de las instalaciones, sin decir por qué.
  io.log("• Música…");
  const leeParche = (nombre: string): Uint8Array | undefined => {
    for (const ruta of [nombre, `upgrade/${nombre}`]) {
      if (io.exists(ruta)) return read(ruta);
    }
    return undefined;
  };
  const bancoOpl = leeParche("FAT.OPL");
  if (bancoOpl) {
    await io.putBin("music/fat.opl", bancoOpl);
    let pistas = 0;
    for (const track of TRACKS) {
      const xmi = leeParche(track.xmi);
      if (!xmi) continue;
      const midi = xmiToMidi(xmi)[0];
      if (!midi) continue;
      await io.putBin(track.out, midi);
      pistas++;
    }
    io.log(`  banco de timbres + ${pistas} pistas`);
  } else {
    io.log("  (sin FAT.OPL: esta copia no tiene el parche de música; el juego va igual)");
  }

  // ── PUERTA DE VERSIÓN (#293) ───────────────────────────────────────────────────────
  // Lo emitido tiene que cuadrar con `assets-catalog.ts`, que es la lista contra la que
  // el arranque del juego decide si la extracción de quien juega se quedó vieja. Va aquí
  // y no en un test porque un test se puede no correr: esto corre en TODA extracción,
  // la de la CLI y la del navegador. Ver el porqué de las dos direcciones en el catálogo.
  verificaCatalogo(emitidos, { skipTiles: opts.skipTiles === true });
}
