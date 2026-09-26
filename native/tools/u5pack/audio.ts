/**
 * Alpha 3 A3-01 -- `npm run pack:audio [-- --source <dir>] [-- --output <file>]`.
 *
 * Builds the optional SD audio pack, /ultima5/openu5-audio.bin (OU5AUDIO 1.0,
 * format in native/core/include/openu5/audio_pack.h), from the user's OWN
 * Ultima V files. It always records the install's music capability
 * (audio-capability.ts); it carries the patch's XMI songs and FAT.OPL bytes
 * verbatim only when the supported music patch is complete. Stock installs
 * get a pack that says "no music" -- that is a valid, correct pack.
 *
 * The output is derived from copyrighted user files: it lives under the
 * git-ignored native/assets/ and must never be committed.
 */
import { createHash } from "node:crypto";
import { mkdirSync, writeFileSync } from "node:fs";
import { dirname, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import {
  CAPABILITY_CODE,
  DETECTOR_VERSION,
  type Detection,
  detectMusicCapability,
  directorySource,
} from "./audio-capability.js";
import { crc32 } from "./format.js";

export const AUDIO_MAGIC = Buffer.from("OU5AUDIO", "ascii");
export const AUDIO_VERSION = { major: 1, minor: 0 } as const;
const HEADER = 32;
const ENTRY = 48;
const RECORD = 32;
const KIND = { capability: 0, song: 1, timbreBank: 2 } as const;
const DRIVER_CODE = { absent: 0, known: 1, unrecognized: 2 } as const;
const BANK_CODE = { absent: 0, valid: 1, invalid: 2 } as const;

type PackEntry = { name: string; kind: number; id: number; data: Uint8Array };

/** The 32-byte capability record (layout in audio_pack.h). */
export function capabilityRecord(d: Detection): Buffer {
  const r = Buffer.alloc(RECORD);
  r[0] = 1;
  r[1] = CAPABILITY_CODE[d.capability];
  r[2] = DRIVER_CODE[d.driver];
  r[3] = BANK_CODE[d.bank];
  r.writeUInt16LE(d.songsPresent, 4);
  r.writeUInt16LE(d.songsValid, 6);
  r[8] = d.patchLocation;
  r[9] = d.dataOvlMarker;
  r[10] = Math.min(d.foreignXmi.length, 255);
  r.writeUInt32LE(DETECTOR_VERSION, 12);
  return r;
}

export function buildAudioPack(d: Detection): Buffer {
  const entries: PackEntry[] = [{ name: "capability", kind: KIND.capability, id: 0, data: capabilityRecord(d) }];
  if (d.capability === "supported-music-patch") {
    for (const s of d.songs) {
      const stem = s.file.replace(/\.XMI$/i, "").toUpperCase();
      entries.push({ name: `song-${s.id.toString(16).padStart(2, "0")}-${stem}`, kind: KIND.song, id: s.id, data: s.found!.bytes });
    }
    entries.push({ name: "timbre-bank-FAT.OPL", kind: KIND.timbreBank, id: 0, data: d.bankBytes! });
  }
  const tableEnd = HEADER + entries.length * ENTRY;
  const size = tableEnd + entries.reduce((n, e) => n + e.data.length, 0);
  const out = Buffer.alloc(size);
  AUDIO_MAGIC.copy(out, 0);
  out.writeUInt16LE(AUDIO_VERSION.major, 8);
  out.writeUInt16LE(AUDIO_VERSION.minor, 10);
  out.writeUInt16LE(HEADER, 12);
  out.writeUInt16LE(ENTRY, 14);
  out.writeUInt32LE(entries.length, 16);
  out.writeUInt32LE(size, 20);
  let at = tableEnd;
  entries.forEach((e, i) => {
    const row = HEADER + i * ENTRY;
    if (e.name.length > 23) throw new Error(`audio pack entry name too long: ${e.name}`);
    out.write(e.name, row, "ascii");
    out.writeUInt32LE(e.kind, row + 24);
    out.writeUInt32LE(e.id, row + 28);
    out.writeUInt32LE(at, row + 32);
    out.writeUInt32LE(e.data.length, row + 36);
    out.writeUInt32LE(crc32(e.data), row + 40);
    Buffer.from(e.data).copy(out, at);
    at += e.data.length;
  });
  out.writeUInt32LE(crc32(out.subarray(tableEnd)), 24);
  out.writeUInt32LE(crc32(out.subarray(HEADER, tableEnd)), 28);
  return out;
}

export function describe(d: Detection): string[] {
  const marker = ["unread", "T1K.DRV (stock)", "MID.DRV (patched by u5data.exe)", "other"][d.dataOvlMarker];
  return [
    `music capability: ${d.capability}`,
    `  reason: ${d.reason}`,
    `  mid.drv: ${d.driver}${d.driverName ? ` (${d.driverName})` : ""}${d.driverSha256 ? ` sha256 ${d.driverSha256}` : ""}`,
    `  songs present 0x${d.songsPresent.toString(16).padStart(4, "0")} valid 0x${d.songsValid.toString(16).padStart(4, "0")}`,
    `  FAT.OPL: ${d.bank}; patch files in: ${["none", "game dir", "upgrade/", "game dir + upgrade/"][d.patchLocation]}`,
    `  DATA.OVL driver slot: ${marker}${d.foreignXmi.length ? `; foreign XMI: ${d.foreignXmi.join(", ")}` : ""}`,
  ];
}

function main(): void {
  const root = resolve(dirname(fileURLToPath(import.meta.url)), "../../..");
  const argv = process.argv.slice(2);
  const arg = (flag: string) => {
    const i = argv.indexOf(flag);
    return i >= 0 ? argv[i + 1] : undefined;
  };
  const source = resolve(arg("--source") ?? resolve(root, "original/u5/ultima5"));
  const output = resolve(arg("--output") ?? resolve(root, "native/assets/openu5-audio.bin"));
  const detection = detectMusicCapability(directorySource(source));
  const pack = buildAudioPack(detection);
  mkdirSync(dirname(output), { recursive: true });
  writeFileSync(output, pack);
  console.log(`OpenU5 audio pack (OU5AUDIO ${AUDIO_VERSION.major}.${AUDIO_VERSION.minor})`);
  console.log(`  source: ${source}`);
  for (const line of describe(detection)) console.log(line);
  console.log(`  wrote ${output}`);
  console.log(`  ${pack.length} bytes, payload CRC32 ${pack.readUInt32LE(24).toString(16).padStart(8, "0")}, ` +
              `sha256 ${createHash("sha256").update(pack).digest("hex")}`);
  console.log("  copy it to the SD card as /ultima5/openu5-audio.bin (never commit it)");
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) main();
