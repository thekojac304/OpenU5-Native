/**
 * Alpha 3 A3-01 -- music capability of a user's Ultima V DOS install.
 *
 * The stock DOS game has NO music: its only audio device is the PC speaker
 * (re/notes/audio-profile-1988.md). Music exists only in installs patched with
 * the community "Exodus Project Ultima V Upgrade" 1.0 (Voyager Dragon, 2001),
 * which adds a MIDPAK driver kit, sixteen XMI songs, the FAT.OPL timbre bank
 * and a song selector, mid.drv, whose logic is derived byte for byte in
 * re/notes/music-location-mapping.md. The native port re-implements that
 * selector, so it can vouch for music ONLY when the selector it copied is the
 * one the install carries.
 *
 * Evidence, strongest first -- never file names alone:
 *   1. mid.drv: SHA-256 of the whole 823-byte driver. The known hash pins the
 *      selection logic the port implements; any other driver is an unknown
 *      variant, whatever it is called.
 *   2. The song files are the ones mid.drv's own table names (pointer table at
 *      0x20), each a structurally valid single-sequence XMI that the reference
 *      converter (extractor/src/audio/xmi2midi.ts) also accepts.
 *   3. FAT.OPL parses as a Miles AIL timbre library (game/src/ui/opl/bank.ts),
 *      the bank the reference synthesizer plays the XMI with.
 *   4. Informational only: DATA.OVL's driver slot (fileoff 0x5350). The
 *      patch's u5data.exe rewrites "T1K.DRV" there to "MID.DRV", so the slot
 *      proves only that u5data.exe once ran on that file -- not that a driver
 *      or a single song is present -- and it decides nothing by itself.
 *
 * Files are looked up case-insensitively in the game directory, then in its
 * upgrade/ subdirectory (the GOG layout), exactly as the extractor does.
 */
import { createHash } from "node:crypto";
import { existsSync, readdirSync, readFileSync, statSync } from "node:fs";
import { join } from "node:path";
import { xmiToMidi } from "../../../extractor/src/audio/xmi2midi.js";
import { parseMilesOplBank } from "../../../game/src/ui/opl/bank.js";

export type MusicCapability =
  | "stock-no-music"
  | "supported-music-patch"
  | "incomplete-music-patch"
  | "unknown-music-variant";

/** The byte the audio pack stores (openu5::MusicCapability). */
export const CAPABILITY_CODE: Record<MusicCapability, number> = {
  "stock-no-music": 0,
  "supported-music-patch": 1,
  "incomplete-music-patch": 2,
  "unknown-music-variant": 3,
};

export interface KnownDriver {
  id: number;
  name: string;
  size: number;
  sha256: string;
}

/** The only driver whose song selection the port implements. */
export const EXODUS_U5_UPGRADE_1_0: KnownDriver = {
  id: 1,
  name: "Exodus Project Ultima V Upgrade 1.0 mid.drv",
  size: 823,
  sha256: "d5a0d0c2ce93e782aa72fe8f340e4a406c4c43ac1e862bf95a9ed49e5b77b7c2",
};

/**
 * The file table of that driver (0x20 -> 0x40..), song id = index. Used only
 * as EVIDENCE that patch files are present when the driver itself is missing;
 * with the driver present the names are read from the driver's own table.
 */
export const EXODUS_SONG_FILES = [
  "U5THEME.XMI", "BRITLAND.XMI", "HORNPIPE.XMI", "ENGGMNT.XMI", "STONES.XMI", "GREYSON.XMI",
  "FANFARE.XMI", "MONARCH.XMI", "TRNTLLA.XMI", "HALLS.XMI", "WRLDBLW.XMI", "BLCKTHRN.XMI",
  "LADYNAN.XMI", "REUNION.XMI", "RULEBRIT.XMI", "AMIGA.XMI",
] as const;
export const SONG_COUNT = 16;
/** SETM.EXE's own test song, part of the MIDPAK kit, never a game song. */
const MIDPAK_TEST_SONG = "SETM.XMI";
export const DETECTOR_VERSION = 1;

/** A read-only view of an install: real directories, or an in-memory one in tests. */
export interface SourceFiles {
  /** File names (any case) directly in the game dir ("") or in its upgrade/ subdir. */
  list(dir: "" | "upgrade"): string[];
  /** Bytes of `name` (exactly as listed) in `dir`. */
  read(dir: "" | "upgrade", name: string): Uint8Array;
}

export function directorySource(root: string): SourceFiles {
  const path = (dir: "" | "upgrade") => (dir ? join(root, dir) : root);
  return {
    list: (dir) => {
      const p = path(dir);
      if (!existsSync(p) || !statSync(p).isDirectory()) return [];
      return readdirSync(p).filter((n) => statSync(join(p, n)).isFile());
    },
    read: (dir, name) => new Uint8Array(readFileSync(join(path(dir), name))),
  };
}

type Found = { dir: "" | "upgrade"; name: string; bytes: Uint8Array };

function finder(src: SourceFiles): (upper: string) => Found | undefined {
  const index = new Map<string, { dir: "" | "upgrade"; name: string }>();
  for (const dir of ["upgrade", ""] as const) // root last, so it wins
    for (const name of src.list(dir)) index.set(name.toUpperCase(), { dir, name });
  return (upper) => {
    const hit = index.get(upper);
    return hit ? { ...hit, bytes: src.read(hit.dir, hit.name) } : undefined;
  };
}

const sha256 = (b: Uint8Array) => createHash("sha256").update(b).digest("hex");

/** mid.drv's file table: 16 u16 pointers at 0x20, each (file offset + 0x100) of a NUL-terminated name. */
export function driverSongTable(driver: Uint8Array): string[] | undefined {
  if (driver.length < 0x40) return undefined;
  const names: string[] = [];
  for (let i = 0; i < SONG_COUNT; i++) {
    const at = (driver[0x20 + 2 * i]! | (driver[0x21 + 2 * i]! << 8)) - 0x100;
    if (at < 0x40 || at >= driver.length) return undefined;
    const end = driver.indexOf(0, at);
    if (end < 0 || end - at < 5 || end - at > 12) return undefined;
    const name = String.fromCharCode(...driver.subarray(at, end));
    if (!/^[A-Z0-9]{1,8}\.XMI$/.test(name)) return undefined;
    names.push(name);
  }
  return names;
}

const be32 = (b: Uint8Array, at: number) =>
  ((b[at]! << 24) | (b[at + 1]! << 16) | (b[at + 2]! << 8) | b[at + 3]!) >>> 0;
const le16 = (b: Uint8Array, at: number) => b[at]! | (b[at + 1]! << 8);
const tag = (b: Uint8Array, at: number) => String.fromCharCode(...b.subarray(at, at + 4));

/**
 * The Exodus XMI shape: FORM:XDIR { INFO(u16 1) } CAT :XMID { FORM:XMID { [TIMB] EVNT } },
 * every chunk inside its parent. The rules of openu5::validate_xmi, plus the
 * reference converter must turn it into exactly one MIDI track.
 */
export function xmiProblem(b: Uint8Array): string | undefined {
  const chunk = (at: number, end: number) => {
    if (at + 8 > end) return undefined;
    const length = be32(b, at + 4);
    return at + 8 + length <= end ? { id: tag(b, at), body: at + 8, length } : undefined;
  };
  const xdir = chunk(0, b.length);
  if (!xdir || xdir.id !== "FORM" || xdir.length < 4 || tag(b, xdir.body) !== "XDIR") return "no FORM:XDIR";
  const xdirEnd = xdir.body + xdir.length;
  const info = chunk(xdir.body + 4, xdirEnd);
  if (!info || info.id !== "INFO" || info.length < 2) return "no INFO";
  if (le16(b, info.body) !== 1) return `INFO declares ${le16(b, info.body)} sequences, not 1`;
  const catAt = xdirEnd + (xdir.length & 1);
  const cat = chunk(catAt, b.length);
  if (!cat || cat.id !== "CAT " || cat.length < 4 || tag(b, cat.body) !== "XMID") return "no CAT :XMID";
  const catEnd = cat.body + cat.length;
  const form = chunk(cat.body + 4, catEnd);
  if (!form || form.id !== "FORM" || form.length < 4 || tag(b, form.body) !== "XMID") return "no FORM:XMID";
  const formEnd = form.body + form.length;
  let events = false;
  for (let at = form.body + 4; at < formEnd; ) {
    const sub = chunk(at, formEnd);
    if (!sub) return "chunk overruns FORM:XMID";
    if (sub.id === "EVNT" && sub.length > 0) events = true;
    at = sub.body + sub.length + (sub.length & 1);
  }
  if (!events) return "no EVNT";
  try {
    if (xmiToMidi(b).length !== 1) return "reference converter did not yield one track";
  } catch (e) {
    return `reference converter rejected it: ${(e as Error).message}`;
  }
  return undefined;
}

/** A Miles AIL timbre library the reference synthesizer can load. */
export function bankProblem(b: Uint8Array): string | undefined {
  try {
    parseMilesOplBank(b);
    return undefined;
  } catch (e) {
    return (e as Error).message;
  }
}

export interface SongDetection {
  id: number;
  file: string;
  found?: Found;
  problem?: string;
}

export interface Detection {
  capability: MusicCapability;
  reason: string;
  driver: "absent" | "known" | "unrecognized";
  driverName?: string;
  driverSha256?: string;
  bank: "absent" | "valid" | "invalid";
  bankBytes?: Uint8Array;
  songs: SongDetection[];
  songsPresent: number;
  songsValid: number;
  /** bit 0: a patch file was in the game dir; bit 1: in upgrade/. */
  patchLocation: number;
  /** DATA.OVL fileoff 0x5350: 0 unread, 1 T1K.DRV, 2 MID.DRV, 3 other. */
  dataOvlMarker: number;
  foreignXmi: string[];
}

export function detectMusicCapability(
  src: SourceFiles,
  known: readonly KnownDriver[] = [EXODUS_U5_UPGRADE_1_0],
): Detection {
  const find = finder(src);
  let patchLocation = 0;
  const note = (f: Found | undefined) => {
    if (f) patchLocation |= f.dir ? 2 : 1;
    return f;
  };

  const dataOvl = find("DATA.OVL");
  let dataOvlMarker = 0;
  if (dataOvl && dataOvl.bytes.length >= 0x5358) {
    const slot = String.fromCharCode(...dataOvl.bytes.subarray(0x5350, 0x5358));
    dataOvlMarker = slot === "T1K.DRV\0" ? 1 : slot === "MID.DRV\0" ? 2 : 3;
  }

  const driverFile = note(find("MID.DRV"));
  const driverHash = driverFile ? sha256(driverFile.bytes) : undefined;
  const knownDriver = driverFile
    ? known.find((k) => k.size === driverFile.bytes.length && k.sha256 === driverHash)
    : undefined;
  const table = knownDriver && driverFile ? driverSongTable(driverFile.bytes) : undefined;
  const songFiles = table ?? [...EXODUS_SONG_FILES];

  const songs: SongDetection[] = songFiles.map((file, id) => {
    const found = note(find(file.toUpperCase()));
    return { id, file, found, problem: found ? xmiProblem(found.bytes) : "missing" };
  });
  const songsPresent = songs.reduce((m, s) => (s.found ? m | (1 << s.id) : m), 0);
  const songsValid = songs.reduce((m, s) => (s.found && !s.problem ? m | (1 << s.id) : m), 0);

  const bankFile = note(find("FAT.OPL"));
  const bank: Detection["bank"] = !bankFile ? "absent" : bankProblem(bankFile.bytes) ? "invalid" : "valid";

  const songSet = new Set<string>([...songFiles, ...EXODUS_SONG_FILES].map((n) => n.toUpperCase()));
  const foreignXmi = (["", "upgrade"] as const)
    .flatMap((dir) => src.list(dir).map((n) => (dir ? `${dir}/${n}` : n)))
    .filter((p) => {
      const base = p.split("/").pop()!.toUpperCase();
      return base.endsWith(".XMI") && base !== MIDPAK_TEST_SONG && !songSet.has(base);
    })
    .sort();

  const base = {
    driver: (!driverFile ? "absent" : knownDriver ? "known" : "unrecognized") as Detection["driver"],
    driverName: knownDriver?.name,
    driverSha256: driverHash,
    bank,
    bankBytes: bank === "valid" ? bankFile!.bytes : undefined,
    songs,
    songsPresent,
    songsValid,
    patchLocation,
    dataOvlMarker,
    foreignXmi,
  };
  const missing = songs.filter((s) => !s.found).map((s) => s.file);
  const broken = songs.filter((s) => s.found && s.problem).map((s) => `${s.file} (${s.problem})`);

  if (driverFile && !knownDriver)
    return {
      ...base,
      capability: "unknown-music-variant",
      reason: `mid.drv is not the ${EXODUS_U5_UPGRADE_1_0.name} (sha256 ${driverHash})`,
    };
  if (!driverFile) {
    if (songsPresent || bankFile)
      return {
        ...base,
        capability: "incomplete-music-patch",
        reason: "music patch files without their mid.drv song selector",
      };
    if (foreignXmi.length)
      return {
        ...base,
        capability: "unknown-music-variant",
        reason: `XMI files of no known music patch: ${foreignXmi.join(", ")}`,
      };
    return { ...base, capability: "stock-no-music", reason: "no music driver and no patch song files" };
  }
  if (!table)
    return { ...base, capability: "incomplete-music-patch", reason: "mid.drv song table unreadable" };
  if (missing.length || broken.length || bank !== "valid") {
    const parts = [
      missing.length ? `missing ${missing.join(", ")}` : "",
      broken.length ? `invalid ${broken.join(", ")}` : "",
      bank !== "valid" ? `FAT.OPL ${bank}` : "",
    ].filter(Boolean);
    return { ...base, capability: "incomplete-music-patch", reason: parts.join("; ") };
  }
  return {
    ...base,
    capability: "supported-music-patch",
    reason: `${knownDriver!.name}: 16 songs and the FAT.OPL timbre bank`,
  };
}
