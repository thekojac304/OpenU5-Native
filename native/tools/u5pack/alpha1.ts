import { createHash } from "node:crypto";
import { mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { PNG } from "pngjs";
import { alphaEnemyName } from "./alpha1-enemy.js";
import { ALPHA_SIGN_RECORD_BYTES, buildAlphaSignData } from "./alpha1-signs.js";
import { buildDungeonArt } from "./alpha1-dungeon-art.js";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "../../..");
const OUT = resolve(ROOT, "native/assets/openu5-alpha1-resources.bin");
const HEADER = 32;
const ENTRY = 64;
const MAGIC = Buffer.from("OU5A1RES", "ascii");

type Entry = { name: string; data: Buffer; records?: number; stride?: number };

function u8map(path: string): Buffer {
  const value = JSON.parse(readFileSync(resolve(ROOT, path), "utf8"));
  const rows = value.tiles ?? value;
  const flat = rows.flat(2);
  if (flat.length !== 65536 || flat.some((n: number) => !Number.isInteger(n) || n < 0 || n > 255)) {
    throw new Error(`${path} is not a 256x256 byte map`);
  }
  return Buffer.from(flat);
}

function smallMaps(): Buffer {
  const maps = JSON.parse(readFileSync(resolve(ROOT, "game/assets/maps/smallmaps.json"), "utf8"));
  const floors: { location: number; floor: number; tiles: number[] }[] = [];
  for (const map of maps) for (const floor of map.floors) {
    const tiles = floor.tiles.flat();
    if (tiles.length !== 1024) throw new Error(`small map ${map.id}:${floor.z} is not 32x32`);
    floors.push({ location: map.id, floor: floor.z, tiles });
  }
  const directory = 4 + floors.length * 12;
  const out = Buffer.alloc(directory + floors.length * 1024);
  out.writeUInt32LE(floors.length, 0);
  floors.forEach((floor, index) => {
    const at = 4 + index * 12;
    out.writeUInt8(floor.location, at);
    out.writeInt16LE(floor.floor, at + 2);
    out.writeUInt32LE(directory + index * 1024, at + 4);
    out.writeUInt32LE(1024, at + 8);
    Buffer.from(floor.tiles).copy(out, directory + index * 1024);
  });
  return out;
}

function dungeons(): Buffer {
  const maps = JSON.parse(readFileSync(resolve(ROOT, "game/assets/maps/dungeons.json"), "utf8"));
  const out = Buffer.alloc(4 + maps.length * 516);
  out.writeUInt32LE(maps.length, 0);
  maps.forEach((map: any, index: number) => {
    const at = 4 + index * 516;
    out.writeUInt8(map.location, at);
    const cells = map.floors.flat(3).map((cell: any) => ((cell.type & 15) << 4) | (cell.sub & 15));
    if (cells.length !== 512) throw new Error(`dungeon ${map.location} is not 8x8x8`);
    Buffer.from(cells).copy(out, at + 4);
  });
  return out;
}

function npcs(): Buffer {
  const source = JSON.parse(readFileSync(resolve(ROOT, "game/assets/npcs.json"), "utf8"));
  const records: Buffer[] = [];
  for (let location = 1; location <= 32; location++) {
    const list = source[String(location)] ?? [];
    for (const npc of list) {
      const out = Buffer.alloc(24);
      out.writeUInt8(location, 0); out.writeUInt8(npc.slot, 1);
      out.writeUInt8(npc.type, 2); out.writeUInt8(npc.dialogNumber, 3);
      for (let i = 0; i < 3; i++) {
        out.writeUInt8(npc.aiTypes[i], 4 + i);
        out.writeUInt8(npc.x[i], 7 + i); out.writeUInt8(npc.y[i], 10 + i);
        out.writeUInt8(npc.z[i], 13 + i);
      }
      for (let i = 0; i < 4; i++) out.writeUInt8(npc.times[i], 16 + i);
      records.push(out);
    }
  }
  const header = Buffer.alloc(4); header.writeUInt32LE(records.length, 0);
  return Buffer.concat([header, ...records]);
}

function worldTables(): Buffer {
  const data = JSON.parse(readFileSync(resolve(ROOT, "game/assets/data.json"), "utf8"));
  const locations = Math.min(data.locationsX.length, data.locationsY.length);
  const search = data.searchObjects ?? [];
  const phases = data.moonPhases ?? [];
  const spawns = data.shardSpawns ?? [];
  const out = Buffer.alloc(16 + locations * 2 + phases.length * 4 + search.length * 24 + spawns.length * 12);
  out.writeUInt32LE(locations, 0); out.writeUInt32LE(phases.length, 4);
  out.writeUInt32LE(search.length, 8); out.writeUInt32LE(spawns.length, 12);
  let at = 16;
  for (let i = 0; i < locations; i++) { out.writeUInt8(data.locationsX[i], at++); out.writeUInt8(data.locationsY[i], at++); }
  for (const value of phases) { out.writeInt32LE(value, at); at += 4; }
  for (const value of search) for (const key of ["id", "quality", "location", "floor", "x", "y"]) {
    out.writeInt32LE(value[key], at); at += 4;
  }
  for (const value of spawns) for (const key of ["x", "y", "z"]) { out.writeInt32LE(value[key], at); at += 4; }
  return out;
}

function combatData(): Buffer {
  const maps = JSON.parse(readFileSync(resolve(ROOT, "game/assets/maps/combatmaps.json"), "utf8"));
  const data = JSON.parse(readFileSync(resolve(ROOT, "game/assets/data.json"), "utf8"));
  const enemyFlags = JSON.parse(readFileSync(
    resolve(ROOT, "game/src/core/data/AdditionalEnemyFlags.json"), "utf8",
  ));
  const rows = readFileSync(resolve(ROOT, "native/core/fixtures/fixed-enemies.txt"), "utf8")
    .trim().split(/\r?\n/).map((line) => line.trim().split(/\s+/).map(Number));
  const tableCount = Math.max(data.attackValues.length, data.attackRangeValues.length,
    data.defenseValues.length, data.reqStrengthEquip.length);
  const pieces: Buffer[] = [];
  const header = Buffer.alloc(16);
  header.writeUInt32LE(maps.length, 0); header.writeUInt32LE(rows.length, 4);
  header.writeUInt32LE(tableCount, 8); header.writeUInt32LE(0, 12);
  pieces.push(header);
  const directions = ["east", "west", "south", "north"];
  for (const map of maps) {
    const b = Buffer.alloc(556); let at = 0;
    b.writeInt32LE(map.index, at); at += 4;
    const tiles = map.tiles.flat();
    if (tiles.length !== 121) throw new Error(`combat map ${map.index} is not 11x11`);
    for (const tile of tiles) { b.writeInt16LE(tile, at); at += 2; }
    for (const direction of directions) {
      const starts = map.playerStarts[direction] ?? [];
      for (let i = 0; i < 6; i++) {
        b.writeInt16LE(starts[i]?.x ?? 0, at); at += 2;
        b.writeInt16LE(starts[i]?.y ?? 0, at); at += 2;
      }
    }
    for (let i = 0; i < 16; i++) {
      b.writeInt16LE(map.units[i]?.x ?? 0, at); at += 2;
      b.writeInt16LE(map.units[i]?.y ?? 0, at); at += 2;
    }
    for (const direction of directions) b.writeUInt8(Math.min(6, map.playerStarts[direction]?.length ?? 0), at++);
    b.writeUInt8(Math.min(16, map.units.length), at++);
    b.writeUInt8(Math.min(8, map.triggers.length), at++);
    for (let i = 0; i < 8; i++) {
      const t = map.triggers[i];
      for (const value of [t?.sprite ?? 0, t?.at?.x ?? 0, t?.at?.y ?? 0,
        t?.pos1?.x ?? 0, t?.pos1?.y ?? 0, t?.pos2?.x ?? 0, t?.pos2?.y ?? 0]) {
        b.writeInt16LE(value, at); at += 2;
      }
    }
    for (let i = 0; i < 16; i++) { b.writeInt16LE(map.units[i]?.sprite ?? 0, at); at += 2; }
    if (at !== b.length) throw new Error(`combat map layout drift: ${at}`);
    pieces.push(b);
  }
  for (const row of rows) {
    if (row.length !== 13) throw new Error("fixed enemy row layout drift");
    const b = Buffer.alloc(88); let at = 0;
    for (let i = 0; i < 10; i++) { b.writeInt32LE(row[i]!, at); at += 4; }
    b.writeUInt16LE(row[10]!, at); at += 2;
    b.writeUInt8(row[11]!, at++); b.writeUInt8(row[12]!, at++);
    const tile = row[0] === 8 ? 300 : 320 + row[0]! * 4;
    b.writeInt16LE(tile, at); at += 2;
    const name = alphaEnemyName(row[0]!, data.monsterNamesMixed, enemyFlags).slice(0, 20);
    const group = String(data.monsterNamesUpper[row[0]!] ?? name).slice(0, 20);
    b.write(name, at, 21, "ascii"); at += 21; b.write(group, at, 21, "ascii"); at += 21;
    pieces.push(b);
  }
  for (const values of [data.attackValues, data.attackRangeValues, data.defenseValues, data.reqStrengthEquip]) {
    const b = Buffer.alloc(tableCount * 4);
    for (let i = 0; i < tableCount; i++) b.writeInt32LE(values[i] ?? 0, i * 4);
    pieces.push(b);
  }
  return Buffer.concat(pieces);
}

function shopData(): Buffer {
  const data = JSON.parse(readFileSync(resolve(ROOT, "game/assets/data.json"), "utf8"));
  const keepers = JSON.parse(readFileSync(resolve(ROOT, "game/src/core/data/ShoppeKeeperMap.json"), "utf8").replace(/^\uFEFF/, ""));
  const locationByName: Record<string, number> = {
    Moonglow: 1, Britain: 2, Jhelom: 3, Yew: 4, Minoc: 5, Trinsic: 6,
    Skara_Brae: 7, New_Magincia: 8, Lord_Britishs_Castle: 17,
    Palace_of_Blackthorn: 18, West_Britanny: 19, North_Britanny: 20,
    East_Britanny: 21, Paws: 22, Cove: 23, Buccaneers_Den: 24,
    Bordermarch: 26, Lycaeum: 30, Empath_Abbey: 31, Serpents_Hold: 32,
  };
  const typeIndex: Record<string, number> = { Blacksmith: 0, Barkeeper: 1, HorseSeller: 2,
    Shipwright: 3, MagicSeller: 4, GuildMaster: 5, Healer: 6, InnKeeper: 7 };
  const names = data.shoppeKeeperNames.filter((name: string) => name !== "Simplon");
  const records = Object.entries(keepers).map(([key, value]: [string, any]) => {
    const index = Number(key); const b = Buffer.alloc(76);
    b.writeInt32LE(locationByName[value.Location] ?? -1, 0);
    b.writeUInt8(typeIndex[value.ShoppeKeeperType], 4); b.writeInt32LE(index, 8);
    b.write(String(data.storeNames[index] ?? "").trim(), 12, 31, "ascii");
    b.write(String(names[index] ?? "").trim(), 44, 31, "ascii");
    return b;
  });
  const arrays = [data.equipmentBasePrices, data.weaponsSoldByMerchants.flat(), data.reagentBasePrices,
    data.reagentQuantities, data.healPrices, data.curePrices, data.resurrectPrices];
  const header = Buffer.alloc(32); header.writeUInt32LE(records.length, 0);
  arrays.forEach((values: number[], i: number) => header.writeUInt32LE(values.length, 4 + i * 4));
  return Buffer.concat([header, ...records, ...arrays.map((values: number[]) => {
    const b = Buffer.alloc(values.length * 4); values.forEach((value, i) => b.writeInt32LE(value, i * 4)); return b;
  })]);
}

const TALK_OPS = ["Text", "AvatarsName", "NewLine", "Rune", "Pause", "KeyWait", "Gold", "Change",
  "JoinParty", "KarmaPlusOne", "KarmaMinusOne", "CallGuards", "EndConversation", "IfElseKnowsName",
  "AskName", "Label", "StartLabelDefinition", "DefineLabel", "DoNothingSection", "EndScript", "Or",
  "StartNewSection", "Unknown"];
function talkText(value: string): Buffer {
  const text = Buffer.from(value ?? "", "utf16le");
  if (text.length / 2 > 65535) throw new Error("dialogue text too long");
  const size = Buffer.alloc(2); size.writeUInt16LE(text.length / 2); return Buffer.concat([size, text]);
}
function talkLine(line: any[]): Buffer {
  const count = Buffer.alloc(2); count.writeUInt16LE(line.length);
  return Buffer.concat([count, ...line.map((item) => {
    const opName = item.kind === "text" ? "Text" : item.op;
    const op = TALK_OPS.indexOf(opName); if (op < 0) throw new Error(`unknown talk op ${opName}`);
    const head = Buffer.alloc(5); head.writeUInt8(op, 0); head.writeInt32LE(item.data ?? item.value ?? 0, 1);
    return Buffer.concat([head, talkText(item.text ?? "")]);
  })]);
}
function talkQa(qa: any[]): Buffer {
  const count = Buffer.alloc(2); count.writeUInt16LE(qa.length);
  return Buffer.concat([count, ...qa.map((item) => {
    const keywords = Buffer.alloc(2); keywords.writeUInt16LE(item.keywords.length);
    const answers = Buffer.alloc(2); answers.writeUInt16LE(item.answer.length);
    return Buffer.concat([keywords, ...item.keywords.map(talkText), answers, ...item.answer.map(talkLine)]);
  })]);
}
function dialogueData(): Buffer {
  const files = ["towne", "dwelling", "castle", "keep"];
  const scripts: { master: number; dialog: number; blob: Buffer }[] = [];
  files.forEach((name, master) => {
    const source = JSON.parse(readFileSync(resolve(ROOT, `game/assets/talk/${name}.json`), "utf8"));
    for (const script of source) {
      const index = Buffer.alloc(4); index.writeInt32LE(script.npcIndex, 0);
      const labels = Buffer.alloc(2); labels.writeUInt16LE(script.labels.length);
      const blob = Buffer.concat([index, talkLine(script.name), talkLine(script.description),
        talkLine(script.greeting), talkLine(script.job), talkLine(script.bye), talkQa(script.qa), labels,
        ...script.labels.map((label: any) => {
          const id = Buffer.alloc(4); id.writeInt32LE(label.label, 0);
          const defaults = Buffer.alloc(2); defaults.writeUInt16LE(label.defaultAnswers.length);
          return Buffer.concat([id, talkLine(label.initialLine), defaults,
            ...label.defaultAnswers.map(talkLine), talkQa(label.qa)]);
        })]);
      scripts.push({ master: master + 1, dialog: script.npcIndex, blob });
    }
  });
  const headerBytes = 4 + scripts.length * 16; let cursor = headerBytes;
  const header = Buffer.alloc(headerBytes); header.writeUInt32LE(scripts.length, 0);
  scripts.forEach((script, i) => { const at = 4 + i * 16; header.writeUInt8(script.master, at);
    header.writeInt32LE(script.dialog, at + 4); header.writeUInt32LE(cursor, at + 8);
    header.writeUInt32LE(script.blob.length, at + 12); cursor += script.blob.length; });
  return Buffer.concat([header, ...scripts.map((script) => script.blob)]);
}
function shrineData(): Buffer {
  const data = JSON.parse(readFileSync(resolve(ROOT, "game/assets/data.json"), "utf8"));
  const out = Buffer.alloc(4 + 8 * 56); out.writeUInt32LE(8, 0);
  for (let i = 0; i < 8; i++) { const at = 4 + i * 56;
    Buffer.from(String(data.virtues[i] ?? ""), "utf16le").copy(out, at, 0, 32);
    Buffer.from(String(data.mantras[i] ?? ""), "utf16le").copy(out, at + 32, 0, 16);
    out.writeInt32LE(data.shrineX[i] ?? 0, at + 48); out.writeInt32LE(data.shrineY[i] ?? 0, at + 52);
  }
  return out;
}

function questionData(): Buffer {
  const questions = JSON.parse(readFileSync(resolve(ROOT, "game/assets/questions.json"), "utf8")).questions as string[];
  if (questions.length !== 28) throw new Error(`QUESTION.DAT extraction has ${questions.length} questions; expected 28`);
  const strings = questions.map((q) => Buffer.from(q.replace(/\u00ad/g, ""), "utf8"));
  const header = Buffer.alloc(4 + (questions.length + 1) * 4); header.writeUInt32LE(questions.length, 0);
  let at = 0; strings.forEach((s, i) => { header.writeUInt32LE(at, 4 + i * 4); at += s.length + 1; });
  header.writeUInt32LE(at, 4 + questions.length * 4);
  return Buffer.concat([header, ...strings.flatMap((s) => [s, Buffer.from([0])])]);
}

function introText(): Buffer {
  const scenes = JSON.parse(readFileSync(resolve(ROOT, "game/assets/intro-scenes.json"), "utf8")) as { text: string }[];
  if (scenes.length !== 21) throw new Error(`INTRO.OVL extraction has ${scenes.length} scenes; expected 21`);
  const strings = scenes.map((scene) => Buffer.from(scene.text.replace(/\u00ad/g, "").replace(/\n/g, " "), "utf8"));
  const header = Buffer.alloc(4 + (strings.length + 1) * 4); header.writeUInt32LE(strings.length, 0);
  let at = 0; strings.forEach((s, i) => { header.writeUInt32LE(at, 4 + i * 4); at += s.length + 1; });
  header.writeUInt32LE(at, 4 + strings.length * 4);
  return Buffer.concat([header, ...strings.flatMap((s) => [s, Buffer.from([0])])]);
}

function introTitle(): Buffer {
  const png = PNG.sync.read(readFileSync(resolve(ROOT, "game/assets/intro-pics.png")));
  const frameBytes = 320 * 110 * 2;
  const out = Buffer.alloc(frameBytes * 4);
  const blit = (frame: number, sx: number, sy: number, width: number, height: number, dx: number, dy: number) => {
    for (let y = 0; y < height; ++y) for (let x = 0; x < width; ++x) {
      const source = ((sy + y) * png.width + sx + x) * 4;
      if (png.data[source + 3] < 128) continue;
      const r = png.data[source], g = png.data[source + 1], b = png.data[source + 2];
      out.writeUInt16LE(((r & 0xf8) << 8) | ((g & 0xfc) << 3) | (b >> 3), frame * frameBytes + ((dy + y) * 320 + dx + x) * 2);
    }
  };
  // Exact extracted INTRO.OVL ultima:0 logo plus all four subtitle fire frames.
  const subtitleY = [3221, 3270, 3319, 3368];
  const subtitleH = [49, 49, 49, 49];
  for (let frame = 0; frame < 4; ++frame) {
    blit(frame, 0, 3160, 319, 61, 0, 0);
    blit(frame, 0, subtitleY[frame]!, 288, subtitleH[frame]!, 16, 61);
  }
  return out;
}

function creditsPanel(): Buffer {
  const png = PNG.sync.read(readFileSync(resolve(ROOT, "game/assets/intro-pics.png")));
  const width = 288, height = 137, sx = 0, sy = 3555;
  const out = Buffer.alloc(width * height * 2);
  for (let y = 0; y < height; ++y) for (let x = 0; x < width; ++x) {
    const source = ((sy + y) * png.width + sx + x) * 4;
    const r = png.data[source]!, g = png.data[source + 1]!, b = png.data[source + 2]!;
    out.writeUInt16LE(((r & 0xf8) << 8) | ((g & 0xfc) << 3) | (b >> 3), (y * width + x) * 2);
  }
  return out;
}

function creationSprites(): Buffer {
  const png = PNG.sync.read(readFileSync(resolve(ROOT, "game/assets/intro-pics.png")));
  const atlas = JSON.parse(readFileSync(resolve(ROOT, "game/assets/intro-pics.json"), "utf8"));
  const records = Array.from({ length: 11 }, (_, id) => {
    const entry = atlas.entries.find((item: any) => item.name === `create:${id}`);
    if (!entry) throw new Error(`Missing extracted CREATE.16 sprite create:${id}`);
    const pixels = Buffer.alloc(entry.width * entry.height * 3);
    for (let y = 0; y < entry.height; ++y) for (let x = 0; x < entry.width; ++x) {
      const source = ((entry.y + y) * png.width + entry.x + x) * 4;
      const target = (y * entry.width + x) * 3;
      const r = png.data[source]!, g = png.data[source + 1]!, b = png.data[source + 2]!;
      pixels.writeUInt16LE(((r & 0xf8) << 8) | ((g & 0xfc) << 3) | (b >> 3), target);
      pixels[target + 2] = png.data[source + 3]!;
    }
    return { id, width: entry.width, height: entry.height, pixels };
  });
  const headerBytes = 16 + records.length * 16;
  const header = Buffer.alloc(headerBytes); header.write("OU5CRT1", 0, "ascii");
  header.writeUInt32LE(records.length, 8); header.writeUInt32LE(headerBytes, 12);
  let offset = headerBytes;
  records.forEach((record, i) => { const at = 16 + i * 16;
    header.writeUInt16LE(record.id, at); header.writeUInt16LE(record.width, at + 2);
    header.writeUInt16LE(record.height, at + 4); header.writeUInt32LE(offset, at + 8);
    header.writeUInt32LE(record.pixels.length, at + 12); offset += record.pixels.length;
  });
  return Buffer.concat([header, ...records.map(record => record.pixels)]);
}
function lookData(): Buffer {
  const rows: string[] = JSON.parse(readFileSync(resolve(ROOT, "game/assets/look2.json"), "utf8"));
  const encoded = rows.map((row) => Buffer.from(`${row}\0`, "utf8"));
  const header = Buffer.alloc(4 + (rows.length + 1) * 4); header.writeUInt32LE(rows.length, 0);
  let at = 0; encoded.forEach((row, i) => { header.writeUInt32LE(at, 4 + i * 4); at += row.length; });
  header.writeUInt32LE(at, 4 + rows.length * 4); return Buffer.concat([header, ...encoded]);
}

function encodeStringRecords(rows: string[]): Buffer {
  const encoded = rows.map((row) => Buffer.from(`${row}\0`, "utf8"));
  const header = Buffer.alloc(4 + (rows.length + 1) * 4);
  header.writeUInt32LE(rows.length, 0);
  let at = 0;
  encoded.forEach((row, i) => { header.writeUInt32LE(at, 4 + i * 4); at += row.length; });
  header.writeUInt32LE(at, 4 + rows.length * 4);
  return Buffer.concat([header, ...encoded]);
}

function stringRecords(path: string): Buffer {
  return encodeStringRecords(JSON.parse(readFileSync(resolve(ROOT, path), "utf8")));
}

// MISCMSG.DAT, extracted from the user's own original data by
// extractor/src/parsers/ds-strings.ts into game/assets/ds-strings.json. This
// is the borrowed-English text ShrineServices::record serves to both
// blackthorn.cpp's capture/interrogation records (0-11) and shrine.cpp's
// Codex/mantra records (12-44); see native/core/include/openu5/shrine.h.
function miscMsgRecords(): Buffer {
  const ds = JSON.parse(readFileSync(resolve(ROOT, "game/assets/ds-strings.json"), "utf8"));
  const rows: string[] = ds["MISCMSG.DAT"];
  if (!Array.isArray(rows) || rows.length < 45) {
    throw new Error(`ds-strings.json MISCMSG.DAT has ${rows?.length ?? 0} records; expected at least 45`);
  }
  for (const row of rows) {
    if ([...row].some((ch) => ch.codePointAt(0)! > 127)) {
      throw new Error("MISCMSG.DAT record has a non-ASCII code point; stringRecords' utf8 encoding " +
        "would no longer be byte-identical to the original latin1 extraction");
    }
  }
  return encodeStringRecords(rows);
}

// Blackthorn's private throne room (#324 / audit R-32): MISCMAPS.DAT record 0,
// the 11x11 grid the capture scene stages the party, the guards and Blackthorn
// himself on. The extractor already produces it as shrine-scene.json's
// `capture` key (the same parser that yields the shrine and Codex rooms); this
// is its first native consumer, so it is packed here for the first time rather
// than hand-copied into device code. Wire format: cols, rows, then cols*rows
// int16 tiles, row-major -- the layout native/core's BlackthornSceneScript
// mounts verbatim.
function blackthornScene(): Buffer {
  const scenes = JSON.parse(readFileSync(resolve(ROOT, "game/assets/shrine-scene.json"), "utf8"));
  const capture = scenes?.capture;
  if (!capture) throw new Error("shrine-scene.json has no `capture` room");
  const { cols, rows, tiles } = capture;
  if (cols !== 11 || rows !== 11) throw new Error(`capture room is ${cols}x${rows}; expected 11x11`);
  const flat: number[] = tiles.flat();
  if (flat.length !== cols * rows) throw new Error(`capture room has ${flat.length} tiles; expected ${cols * rows}`);
  const out = Buffer.alloc(8 + flat.length * 2);
  out.writeUInt32LE(cols, 0);
  out.writeUInt32LE(rows, 4);
  flat.forEach((tile, i) => {
    if (!Number.isInteger(tile) || tile < 0 || tile > 0xffff) throw new Error(`capture tile ${i} is ${tile}`);
    out.writeInt16LE(tile, 8 + i * 2);
  });
  return out;
}

function signData(): Buffer {
  return buildAlphaSignData(JSON.parse(readFileSync(resolve(ROOT, "game/assets/signs.json"), "utf8")));
}

function crc32(data: Uint8Array): number {
  let crc = 0xffffffff;
  for (const byte of data) {
    crc ^= byte;
    for (let bit = 0; bit < 8; bit++) crc = (crc >>> 1) ^ (0xedb88320 & -(crc & 1));
  }
  return (crc ^ 0xffffffff) >>> 0;
}

const file = (name: string, path: string): Entry => ({ name, data: readFileSync(resolve(ROOT, path)) });
const packedSigns = signData();
const demoScene = (() => {
  const value = JSON.parse(readFileSync(resolve(ROOT, "game/assets/demo-scene.json"), "utf8"));
  const maps: number[] = value.maps.flat(3);
  const script: number[] = value.script;
  if (value.cols !== 19 || value.rows !== 4 || value.maps.length !== 4 || maps.length !== 304 ||
      script.length !== 655 || [...maps, ...script].some(n => !Number.isInteger(n) || n < 0 || n > 255))
    throw new Error("demo-scene.json does not match the derived MISCMAPS attract record");
  const out = Buffer.alloc(16 + maps.length + script.length);
  out.write("OU5DEMO1", 0, "ascii"); out[8] = 19; out[9] = 4; out[10] = 4;
  out.writeUInt16LE(script.length, 12); Buffer.from(maps).copy(out, 16); Buffer.from(script).copy(out, 320);
  return out;
})();
const dungeonArt = buildDungeonArt(resolve(ROOT, "original/u5/ultima5"));
const entries: Entry[] = [
  file("init.gam", "game/assets/init.gam"),
  file("init.ool", "game/assets/init.ool"),
  { name: "overworld.map", data: u8map("game/assets/maps/overworld.json"), records: 65536, stride: 1 },
  { name: "underworld.map", data: u8map("game/assets/maps/underworld.json"), records: 65536, stride: 1 },
  { name: "smallmaps.bin", data: smallMaps() },
  { name: "dungeons.bin", data: dungeons(), records: 8, stride: 516 },
  { name: "npcs.bin", data: npcs(), stride: 24 },
  { name: "worldtables.bin", data: worldTables() },
  { name: "combat.bin", data: combatData() },
  { name: "shops.bin", data: shopData() },
  { name: "talk.bin", data: dialogueData() },
  { name: "shrines.bin", data: shrineData() },
  { name: "questions.bin", data: questionData(), records: 28 },
  { name: "intro-text.bin", data: introText(), records: 21 },
  { name: "intro-title.rgb565", data: introTitle(), records: 4 * 320 * 110, stride: 2 },
  { name: "credits.rgb565", data: creditsPanel(), records: 288 * 137, stride: 2 },
  { name: "creation-sprites.bin", data: creationSprites(), records: 11 },
  { name: "demo-scene.bin", data: demoScene, records: 4, stride: 76 },
  { name: "look.bin", data: lookData() },
  { name: "shop-records.bin", data: stringRecords("game/assets/shoppe.json") },
  { name: "misc-records.bin", data: miscMsgRecords() },
  { name: "blackthorn-scene.bin", data: blackthornScene(), records: 11 * 11, stride: 2 },
  { name: "signs.bin", data: packedSigns, records: packedSigns.readUInt32LE(0), stride: ALPHA_SIGN_RECORD_BYTES },
  // Batch 9C / R-05 -- the AUTHORED dungeon art (alpha1-dungeon-art.ts). Five
  // ordinary TOC entries, not a second asset system. The three wall variants stay
  // separate because a dungeon uses exactly one: that keeps each variant its own
  // CRC-checked unit and leaves a one-resident-variant loader open if PSRAM ever
  // gets tight (the device currently keeps all three -- see dungeon_art_cache.h
  // for why). ITEMS and all eight MON banks are shared by every dungeon and are
  // small, so each travels whole.
  { name: "dungeon-dng1.art", data: dungeonArt.wall[0]!, records: 28 },
  { name: "dungeon-dng2.art", data: dungeonArt.wall[1]!, records: 28 },
  { name: "dungeon-dng3.art", data: dungeonArt.wall[2]!, records: 28 },
  { name: "dungeon-items.art", data: dungeonArt.items, records: 20 },
  { name: "dungeon-mon.art", data: dungeonArt.mon, records: 8 * 6 },
  file("runes.ch", "original/u5/ultima5/runes.ch"),
  file("combatmaps.json", "game/assets/maps/combatmaps.json"),
  file("data.json", "game/assets/data.json"),
  file("shoppe.json", "game/assets/shoppe.json"),
  file("talk-towne.json", "game/assets/talk/towne.json"),
  file("talk-dwelling.json", "game/assets/talk/dwelling.json"),
  file("talk-castle.json", "game/assets/talk/castle.json"),
  file("talk-keep.json", "game/assets/talk/keep.json"),
  file("look2.json", "game/assets/look2.json"),
  file("signs.json", "game/assets/signs.json"),
  file("endgame.json", "game/assets/endgame.json"),
];

let cursor = HEADER + entries.length * ENTRY;
const output = Buffer.alloc(cursor + entries.reduce((n, e) => n + e.data.length, 0));
MAGIC.copy(output, 0);
output.writeUInt16LE(2, 8); output.writeUInt16LE(0, 10);
output.writeUInt16LE(HEADER, 12); output.writeUInt16LE(ENTRY, 14);
output.writeUInt32LE(entries.length, 16); output.writeUInt32LE(output.length, 20);
for (let i = 0; i < entries.length; i++) {
  const entry = entries[i]!;
  const toc = HEADER + i * ENTRY;
  if (Buffer.byteLength(entry.name) > 31) throw new Error(`resource name too long: ${entry.name}`);
  output.write(entry.name, toc, "ascii");
  output.writeUInt32LE(cursor, toc + 32); output.writeUInt32LE(entry.data.length, toc + 36);
  output.writeUInt32LE(crc32(entry.data), toc + 40);
  output.writeUInt32LE(entry.records ?? 0, toc + 44); output.writeUInt32LE(entry.stride ?? 0, toc + 48);
  entry.data.copy(output, cursor); cursor += entry.data.length;
}
output.writeUInt32LE(crc32(output.subarray(HEADER + entries.length * ENTRY)), 24);
output.writeUInt32LE(crc32(output.subarray(HEADER, HEADER + entries.length * ENTRY)), 28);
mkdirSync(dirname(OUT), { recursive: true });
writeFileSync(OUT, output);
console.log(`Wrote ${OUT}`);
console.log(`Size: ${output.length} bytes; entries: ${entries.length}`);
console.log(`SHA-256: ${createHash("sha256").update(output).digest("hex")}`);
