/**
 * Alpha 3 A3-01 -- the music-capability detector and the audio-pack writer.
 *
 *   node --import tsx native/tools/u5pack/check-audio-pack.ts \
 *        --real <original/u5/ultima5> --fixture <native/core/fixtures/a3-01-audio-stock.bin> [--write]
 *
 * Synthetic installs are built in memory and contain no game or music bytes
 * (a fake 823-byte driver, empty XMI shells, a one-timbre bank); the known
 * driver hash is injected for them. The real install is only ever READ, and
 * its variants (stock-shaped, a song missing, a flipped driver byte...) are
 * in-memory views over it. `--write` regenerates the committed stock fixture,
 * which the C++ side (a3_01_audio_contract) parses to prove both languages
 * agree on the format.
 */
import { createHash } from "node:crypto";
import { existsSync, readFileSync, writeFileSync } from "node:fs";
import {
  type Detection,
  EXODUS_SONG_FILES,
  EXODUS_U5_UPGRADE_1_0,
  type KnownDriver,
  type SourceFiles,
  detectMusicCapability,
  directorySource,
} from "./audio-capability.js";
import { AUDIO_MAGIC, buildAudioPack } from "./audio.js";
import { crc32 } from "./format.js";

let checks = 0;
let failures = 0;
function check(ok: boolean, label: string): void {
  checks++;
  if (!ok) failures++;
  console.log(`${ok ? "GREEN" : "RED"} ${label}`);
}

// ---- in-memory installs -----------------------------------------------------
type Tree = Record<string, Uint8Array>; // "NAME" or "upgrade/NAME"
function memory(tree: Tree): SourceFiles {
  const inDir = (dir: "" | "upgrade") =>
    Object.keys(tree)
      .filter((k) => (dir ? k.startsWith("upgrade/") : !k.includes("/")))
      .map((k) => (dir ? k.slice("upgrade/".length) : k));
  return { list: inDir, read: (dir, name) => tree[dir ? `upgrade/${name}` : name]! };
}
function view(base: SourceFiles, hide: (name: string) => boolean, replace: Record<string, Uint8Array> = {}): SourceFiles {
  return {
    list: (dir) => base.list(dir).filter((n) => !hide(n.toUpperCase())),
    read: (dir, name) => replace[name.toUpperCase()] ?? base.read(dir, name),
  };
}

const u16le = (v: number) => Uint8Array.of(v & 0xff, v >> 8);
const cat = (...parts: Uint8Array[]) => {
  const out = new Uint8Array(parts.reduce((n, p) => n + p.length, 0));
  let at = 0;
  for (const p of parts) {
    out.set(p, at);
    at += p.length;
  }
  return out;
};
const ascii = (s: string) => Uint8Array.from(s, (c) => c.charCodeAt(0));
function chunk(id: string, body: Uint8Array): Uint8Array {
  const head = new Uint8Array(8);
  head.set(ascii(id));
  new DataView(head.buffer).setUint32(4, body.length, false);
  return cat(head, body, body.length & 1 ? new Uint8Array(1) : new Uint8Array(0));
}
/** The Exodus XMI shape around an empty song (EVNT = end of track). */
function shellXmi(sequences = 1): Uint8Array {
  const xdir = chunk("FORM", cat(ascii("XDIR"), chunk("INFO", u16le(sequences))));
  const xmid = chunk("FORM", cat(ascii("XMID"), chunk("EVNT", Uint8Array.of(0xff, 0x2f, 0x00))));
  return cat(xdir, chunk("CAT ", cat(ascii("XMID"), xmid)));
}
/** One melodic timbre, the FAT.OPL record shape (6-byte index, 0xFFFF, 14-byte block). */
function shellBank(): Uint8Array {
  const b = new Uint8Array(8 + 14);
  b[0] = 0; // patch
  b[1] = 0; // bank
  new DataView(b.buffer).setUint32(2, 8, true);
  b[6] = 0xff;
  b[7] = 0xff;
  new DataView(b.buffer).setUint16(8, 14, true);
  return b;
}
/** An 823-byte stand-in driver: only its file table is real in shape. */
function shellDriver(names: readonly string[] = EXODUS_SONG_FILES): Uint8Array {
  const d = new Uint8Array(823);
  let at = 0x40;
  names.forEach((n, i) => {
    new DataView(d.buffer).setUint16(0x20 + 2 * i, at + 0x100, true);
    d.set(ascii(n), at);
    at += n.length + 1;
  });
  return d;
}
function shellDataOvl(slot: string): Uint8Array {
  const d = new Uint8Array(0x5358 + 16);
  d.set(ascii(slot), 0x5350);
  return d;
}
const sha = (b: Uint8Array) => createHash("sha256").update(b).digest("hex");
const shellKnown = (driver: Uint8Array): KnownDriver[] => [{ id: 1, name: "test driver", size: driver.length, sha256: sha(driver) }];
function shellPatch(prefix = ""): Tree {
  const t: Tree = { [`${prefix}MID.DRV`]: shellDriver(), [`${prefix}FAT.OPL`]: shellBank() };
  for (const n of EXODUS_SONG_FILES) t[`${prefix}${n}`] = shellXmi();
  return t;
}

// ---- pack reading (independent of the writer) --------------------------------
interface Parsed {
  entries: { name: string; kind: number; id: number; data: Buffer; crcOk: boolean }[];
  record: Buffer;
  headerOk: boolean;
}
function parse(pack: Buffer): Parsed {
  const count = pack.readUInt32LE(16);
  const entries = [];
  for (let i = 0; i < count; i++) {
    const row = 32 + 48 * i;
    const name = pack.toString("ascii", row, row + 24).replace(/\0.*$/, "");
    const offset = pack.readUInt32LE(row + 32);
    const length = pack.readUInt32LE(row + 36);
    const data = pack.subarray(offset, offset + length);
    entries.push({ name, kind: pack.readUInt32LE(row + 24), id: pack.readUInt32LE(row + 28), data,
                   crcOk: crc32(data) === pack.readUInt32LE(row + 40) });
  }
  const tableEnd = 32 + 48 * count;
  const headerOk =
    pack.subarray(0, 8).equals(AUDIO_MAGIC) && pack.readUInt16LE(8) === 1 && pack.readUInt16LE(12) === 32 &&
    pack.readUInt16LE(14) === 48 && pack.readUInt32LE(20) === pack.length &&
    crc32(pack.subarray(tableEnd)) === pack.readUInt32LE(24) &&
    crc32(pack.subarray(32, tableEnd)) === pack.readUInt32LE(28);
  return { entries, record: entries[0]?.data ?? Buffer.alloc(0), headerOk };
}
const capabilityByte = (d: Detection) => parse(buildAudioPack(d)).record[1];

// ---- arguments --------------------------------------------------------------
const argv = process.argv.slice(2);
const arg = (flag: string) => {
  const i = argv.indexOf(flag);
  return i >= 0 ? argv[i + 1] : undefined;
};
const realDir = arg("--real");
const fixturePath = arg("--fixture");

// ---- synthetic installs -----------------------------------------------------
{
  const d = detectMusicCapability(memory({}));
  check(d.capability === "stock-no-music" && d.dataOvlMarker === 0, "C1 an empty install is stock: no music");
}
const stockInstall = memory({ "DATA.OVL": shellDataOvl("T1K.DRV\0") });
const stock = detectMusicCapability(stockInstall);
const stockPack = buildAudioPack(stock);
{
  const p = parse(stockPack);
  check(stock.capability === "stock-no-music" && stock.dataOvlMarker === 1,
        "C2 a stock install (DATA.OVL slot T1K.DRV) is stock-no-music");
  check(p.headerOk && p.entries.length === 1 && p.entries[0]!.name === "capability" && p.entries[0]!.crcOk &&
        p.record.length === 32 && p.record[0] === 1 && p.record[1] === 0 && p.record[9] === 1,
        "C2 its pack is valid, carries ONLY the capability record, and says stock (a stock pack is not a failure)");
  check(stockPack.length === 32 + 48 + 32, "C2 the stock pack is 112 bytes");
  if (fixturePath) {
    if (argv.includes("--write")) writeFileSync(fixturePath, stockPack);
    const committed = existsSync(fixturePath) ? readFileSync(fixturePath) : Buffer.alloc(0);
    check(committed.equals(stockPack), "C2 the writer reproduces the committed stock fixture byte for byte");
  }
}
{
  const d = detectMusicCapability(memory({ "DATA.OVL": shellDataOvl("T1K.DRV\0"), "setm.xmi": shellXmi() }));
  check(d.capability === "stock-no-music" && d.foreignXmi.length === 0,
        "C3 MIDPAK's own SETM.XMI alone is not music for the game: stock");
}
{
  const d = detectMusicCapability(memory({ "OTHER.XMI": shellXmi() }));
  check(d.capability === "unknown-music-variant" && capabilityByte(d) === 3,
        "C4 XMI songs of no known patch are an UNKNOWN variant, never stock");
}
{
  const t = shellPatch();
  delete t["MID.DRV"];
  const d = detectMusicCapability(memory(t), shellKnown(shellDriver()));
  check(d.capability === "incomplete-music-patch" && d.songsPresent === 0xffff,
        "C5 the sixteen songs without their mid.drv selector are an incomplete patch");
}
{
  const d = detectMusicCapability(memory({ "FAT.OPL": shellBank() }));
  check(d.capability === "incomplete-music-patch", "C6 the timbre bank alone is an incomplete patch");
}
{
  const t = shellPatch();
  const d = detectMusicCapability(memory(t)); // the shell driver is NOT the Exodus driver
  check(d.capability === "unknown-music-variant" && d.driver === "unrecognized",
        "C7 a complete-looking patch whose mid.drv hash is not Exodus 1.0 is an unknown variant");
}
const shell = shellPatch();
const known = shellKnown(shell["MID.DRV"]!);
{
  const d = detectMusicCapability(memory(shell), known);
  const p = parse(buildAudioPack(d));
  check(d.capability === "supported-music-patch" && d.songsValid === 0xffff && d.bank === "valid",
        "C8 driver hash known + 16 valid songs + valid bank = supported");
  check(p.headerOk && p.entries.length === 18 && p.entries.every((e) => e.crcOk) &&
        p.entries.slice(1, 17).every((e, i) => e.kind === 1 && e.id === i) &&
        p.entries[17]!.kind === 2 && p.record[1] === 1 && p.record[2] === 1 && p.record[3] === 1 &&
        p.record.readUInt16LE(4) === 0xffff && p.record.readUInt16LE(6) === 0xffff,
        "C8 its pack carries the record, songs 0..15 in id order and the bank, every CRC valid");
  check(Buffer.from(p.entries[5]!.data).equals(Buffer.from(shell["GREYSON.XMI"]!)),
        "C8 song 5 is GREYSON.XMI, verbatim, found through the driver's own table");
}
{
  const t = { ...shell };
  delete t["STONES.XMI"];
  const d = detectMusicCapability(memory(t), known);
  const p = parse(buildAudioPack(d));
  check(d.capability === "incomplete-music-patch" && d.songsPresent === (0xffff & ~(1 << 4)) &&
        p.entries.length === 1, "C9 one song missing: incomplete, and the pack ships no music at all");
}
{
  const d = detectMusicCapability(memory({ ...shell, "HALLS.XMI": shellXmi(2) }), known);
  check(d.capability === "incomplete-music-patch" && d.songsValid === (0xffff & ~(1 << 9)) &&
        d.songsPresent === 0xffff, "C10 a structurally wrong song (two sequences): incomplete, present but not valid");
}
{
  const d = detectMusicCapability(memory({ ...shell, "FAT.OPL": Uint8Array.of(1, 2, 3) }), known);
  check(d.capability === "incomplete-music-patch" && d.bank === "invalid", "C11 an unreadable FAT.OPL: incomplete");
}
{
  const d = detectMusicCapability(memory({ "DATA.OVL": shellDataOvl("MID.DRV\0"), ...shellPatch("upgrade/") }), known);
  check(d.capability === "supported-music-patch" && d.patchLocation === 2 && d.dataOvlMarker === 2,
        "C12 the GOG layout (patch under upgrade/) is found and recorded");
}
{
  const t: Tree = {};
  for (const [k, v] of Object.entries(shell)) t[k === "TRNTLLA.XMI" ? "trntlla.xmi" : k] = v;
  const d = detectMusicCapability(memory(t), known);
  check(d.capability === "supported-music-patch", "C13 lookups ignore case (the shipped trntlla.xmi is lower case)");
}
{
  const a = buildAudioPack(detectMusicCapability(memory(shell), known));
  const b = buildAudioPack(detectMusicCapability(memory(shell), known));
  check(a.equals(b), "C14 the pack is deterministic");
}

// ---- the real development install (read-only; variants are in-memory views) --
if (!realDir || !existsSync(realDir)) {
  check(false, `R0 the real install is required (--real ${realDir ?? "<missing>"})`);
} else {
  const real = directorySource(realDir);
  const r = detectMusicCapability(real);
  check(r.capability === "supported-music-patch" && r.driverSha256 === EXODUS_U5_UPGRADE_1_0.sha256 &&
        r.songsValid === 0xffff && r.bank === "valid",
        "R1 the development install is the supported patch: Exodus 1.0 mid.drv, 16 valid songs, valid FAT.OPL");
  check(r.dataOvlMarker === 2 && r.foreignXmi.length === 0, "R1 its DATA.OVL slot reads MID.DRV; no foreign XMI");
  const pack = parse(buildAudioPack(r));
  check(pack.headerOk && pack.entries.length === 18 && pack.entries.every((e) => e.crcOk),
        "R1 its audio pack is 18 CRC-valid entries");
  const patchOnly = new Set<string>(["MID.DRV", "FAT.OPL", "SETM.XMI", ...EXODUS_SONG_FILES]);
  const stockShaped = view(real, (n) => patchOnly.has(n));
  const s = detectMusicCapability(stockShaped);
  check(s.capability === "stock-no-music" && s.dataOvlMarker === 2,
        "R2 the same files minus the music patch are stock -- even though DATA.OVL still says MID.DRV " +
        "(the slot only proves u5data.exe ran; it is recorded, never decisive)");
  const noStones = detectMusicCapability(view(real, (n) => n === "STONES.XMI"));
  check(noStones.capability === "incomplete-music-patch" && noStones.songsPresent === (0xffff & ~(1 << 4)),
        "R3 the real install without STONES.XMI is incomplete");
  const driverBytes = real.read("", real.list("").find((n) => n.toUpperCase() === "MID.DRV")!);
  const flipped = Uint8Array.from(driverBytes);
  flipped[0x16d] ^= 0x01; // one byte of the location selector
  const variant = detectMusicCapability(view(real, () => false, { "MID.DRV": flipped }));
  check(variant.capability === "unknown-music-variant",
        "R4 one changed byte in mid.drv's selector makes it an unknown variant (the port copies THAT selector)");
  const bankBytes = real.read("", real.list("").find((n) => n.toUpperCase() === "FAT.OPL")!);
  const truncated = detectMusicCapability(view(real, () => false, { "FAT.OPL": bankBytes.subarray(0, 100) }));
  check(truncated.capability === "incomplete-music-patch" && truncated.bank === "invalid",
        "R5 a truncated FAT.OPL is incomplete");
}

console.log(`A3-01 audio capability: ${checks - failures}/${checks} checks`);
process.exit(failures ? 1 : 0);
