/**
 * Alpha 4 A4-END1 -- checks for the pre-composed ending screens
 * (alpha1-endgame.ts), over the user's own original files.
 *
 *   npx tsx native/tools/u5pack/check-endgame-pages.ts --real original/u5/ultima5 [--pack <alpha1 pack>] [--png <dir>]
 *
 * What it proves is the port of render_justified_text and story_screens'
 * order of drawing, not a picture someone liked: every justified line ends on
 * its band's right edge, every unjustified one ends a paragraph or the text,
 * no glyph is drawn at y >= 0xc0, every printable byte of the page is drawn
 * once (in order, with a '-' at each soft-hyphen break), the art is drawn over
 * the headlines, and the page bytes match the recorded goldens. `--png` writes
 * the seven screens for a human look. `--pack` also holds the shipped resource
 * pack's two ending entries to a fresh build (the firmware locks that pack by
 * size, CRC and SHA-256, so a stale pack is a defect, not a choice).
 */
import { createHash } from "node:crypto";
import { mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { resolve } from "node:path";
import { PNG } from "pngjs";
import {
  PAGE_H, PAGE_W, STORY_PAGES, buildEndgamePages, composeScrollPage, composeStoryPage, loadEndgameSources,
  pack4bpp, pageLayouts, renderJustified, type TextTrace,
} from "./alpha1-endgame.js";

// The U5 EGA palette (extractor/src/png.ts; no brown fix).
const EGA: [number, number, number][] = [
  [0, 0, 0], [0, 0, 170], [0, 170, 0], [0, 170, 170], [170, 0, 0], [170, 0, 170], [170, 85, 0], [170, 170, 170],
  [85, 85, 85], [85, 85, 255], [85, 255, 85], [85, 255, 255], [255, 85, 85], [255, 85, 255], [255, 255, 85], [255, 255, 255],
];

// SHA-256 of each packed 4bpp screen, recorded from this composition after the
// screens were looked at (A4-END1); a change to the port must explain itself.
const GOLDEN: string[] = [
  "3d67508b9ac7e42a52f96cd6a77b3566eebca3d9bf8eddbbadb62b4662622c2f", // The Homecoming
  "a0ca8941d7868f6e879a4a3c1e155e1509f9736af37b9ac4bdd1d0259fc4f152", // the house
  "7ff66e27ddf47be1a01c4630c5d8eec27e18127feaa944d516620bcc08df8b60", // the night
  "bd1f28f4301cafa2fe61374528fe7cb22a2fb0d326b3509f36ba4855fc050237", // The Dream
  "40088e4acf5c7fec38f99ebf9d0f47e54e7f805a1b3166bfb0b3ab7e410f931e", // the choice
  "c6612d5ea411c6f3b3f23b4a3374a7ed56bf7ba7362677de76c69efcfdba3d61", // the gate
  "750dec8e1d767af9bd3d8ad40f10453b4f27e2f2689bc8f9b6829b0a930b030b", // the scroll
];

let checks = 0, failures = 0;
function check(ok: boolean, id: string, what: string) {
  ++checks;
  if (!ok) ++failures;
  console.log(`  ${ok ? "GREEN" : "RED  "} ${id.padEnd(5)} ${what}`);
}

const args = process.argv.slice(2);
const dir = args[args.indexOf("--real") + 1] ?? "original/u5/ultima5";
const pngDir = args.includes("--png") ? args[args.indexOf("--png") + 1] : undefined;
const packPath = args.includes("--pack") ? args[args.indexOf("--pack") + 1] : undefined;
const src = loadEndgameSources(dir);
const layouts = pageLayouts(src.data);
console.log(`A4-END1 ending screens -- ${dir}`);

check(layouts.map((l) => l.textOffset).join(",") === "0,424,956,1530,2280,2932" &&
      layouts.map((l) => `${l.left}|${l.right}|${l.cut}|${l.bottom}|${l.penX},${l.penY}`).join(" ") ===
        "172,0|320,320|126|200|172,66 0,0|320,320|126|200|0,92 0,196|320,320|42|148|0,9 " +
        "179,0|320,320|100|200|179,38 0,161|320,320|82|200|0,9 0,0|154,320|112|200|0,0",
      "L1", "DATA.OVL 0x3da6-0x3e06: the six pages' text offsets, bands, cuts and pens");

const pages: Uint8Array[] = [];
const traces: TextTrace[] = [];
for (let i = 0; i < STORY_PAGES; i++) {
  const trace: TextTrace = { glyphs: [], lines: [] };
  pages.push(composeStoryPage(src, i, trace));
  traces.push(trace);
}
pages.push(composeScrollPage(src));

// J: justification.
let jbad = "", nbad = "";
const hyW = src.data[0x50ca + 0x2d + 0x10]! + 1;
traces.forEach((t, i) => {
  const lay = layouts[i]!;
  for (const l of t.lines) {
    const text = src.endDat;
    const last = text[lay.textOffset + l.end] ?? 0;
    if (l.justified) {
      const right = lay.right[l.band]!;
      const want = last === 0x5f ? right - hyW : right;
      if (l.x1 !== want) jbad += ` p${i}y${l.y}:${l.x1}!=${want}`;
    } else if (last !== 0 && last !== 0x0a) {
      // An unjustified break at a space can only be a line without spaces.
      let spaces = 0;
      for (let k = l.start; k < l.end; k++) if ((text[lay.textOffset + k] ?? 0) <= 0x20) ++spaces;
      if (spaces) nbad += ` p${i}y${l.y}`;
    }
  }
});
check(!jbad, "J1", "every justified line ends on its band's right edge (a soft-hyphen line one '-' short)" + jbad);
check(!nbad, "J2", "every unjustified line ends a paragraph or the text" + nbad);

// C: the clip and the coverage.
let clipped = "", cover = "";
traces.forEach((t, i) => {
  if (t.glyphs.some((g) => g.y >= 0xc0)) clipped += ` p${i}`;
  const lay = layouts[i]!;
  const at = (k: number) => src.endDat[lay.textOffset + k] ?? 0;
  const expected: number[] = [];
  for (const l of t.lines) {
    for (let k = l.start; k < l.end; k++) { const c = at(k); if (c > 0x20 && c !== 0x7b && c !== 0x5f) expected.push(c); }
    if (at(l.end) === 0x5f) expected.push(0x2d);
  }
  let printable = 0;
  for (let k = 0; at(k); k++) { const c = at(k); if (c > 0x20 && c !== 0x7b && c !== 0x5f) ++printable; }
  const shown = expected.filter((c, k) => c !== 0x2d || at(0) === 0 || k >= 0).length;
  const breaks = t.lines.filter((l) => at(l.end) === 0x5f).length;
  const lastY = t.lines.length ? t.lines[t.lines.length - 1]!.y : 0;
  if (t.glyphs.map((g) => g.code).join(",") !== expected.join(",") || shown - breaks !== printable || lastY >= 0xc0)
    cover += ` p${i}:glyphs=${t.glyphs.length},printable=${printable},breaks=${breaks},lastY=${lastY}`;
});
check(!clipped, "C1", "no glyph is drawn at y >= 0xc0 (0x01c9 / 0x021f)" + clipped);
check(!cover, "C2", "each page's printable bytes are drawn once, in order, a '-' per soft-hyphen break, nothing laid out below y 192" + cover);
{ // C3: the clip itself. The real pages end above y 0xc0, so lay a text three pages long
  // on page 0's layout, whose 9-row leading from y 66 puts a line at exactly y 0xc0.
  const lay = layouts[0]!;
  const one: number[] = [];
  for (let k = lay.textOffset; src.endDat[k]; k++) one.push(src.endDat[k]!);
  const long = new Uint8Array([...one, 0x20, ...one, 0x20, ...one, 0]);
  const trace: TextTrace = { glyphs: [], lines: [] };
  renderJustified(new Uint8Array(PAGE_W * PAGE_H), long, 0, lay, src.glyphs, src.data, trace);
  const below = trace.lines.filter((l) => l.y >= 0xc0).length;
  const atCut = trace.lines.some((l) => l.y === 0xc0);
  check(below > 0 && atCut && trace.glyphs.length > 0 && trace.glyphs.every((g) => g.y < 0xc0), "C3",
        `the clip on an over-long text: ${below} lines laid out at y >= 0xc0, none of their glyphs drawn (0x01c9)`);
}

// O: story_screens' order of drawing -- the art over the headlines.
const t4 = src.text[4]!, art0 = src.end[0]![0]!;
let over = true;
for (let y = 28; y < 28 + t4.height; y++)
  for (let x = 152; x < art0.width; x++)
    if (y < art0.height && pages[0]![y * PAGE_W + x] !== art0.pixels[y * art0.width + x]) over = false;
let hBelow = false;
for (let y = 28; y < 28 + t4.height; y++)
  for (let x = art0.width; x < 152 + t4.width; x++)
    if (t4.pixels[(y - 28) * t4.width + (x - 152)] && pages[0]![y * PAGE_W + x] === t4.pixels[(y - 28) * t4.width + (x - 152)]) hBelow = true;
check(over && hBelow, "O1", "page 0: the art (opaque, drawn after) covers Homecoming's first 15 columns; the rest shows");
const sc = src.scroll[0]!;
check(pages[6]![0] === 0 && pages[6]![40 + 0 * PAGE_W] === sc.pixels[0] && pages[6]![PAGE_W * 199 + 319] === 0,
      "O2", "the scroll page: black, ENDSC.16 at (40,0)");

// G: the bytes.
const hashes = pages.map((p) => createHash("sha256").update(pack4bpp(p)).digest("hex"));
if (GOLDEN.length) check(hashes.every((h, i) => h === GOLDEN[i]), "G1", "the seven screens match the recorded goldens");
else console.log("  (no goldens recorded) " + hashes.map((h, i) => `${i}:${h.slice(0, 16)}`).join(" "));
const built = buildEndgamePages(dir);
check(built.pages.length === 16 + 7 * PAGE_W * PAGE_H / 2 && built.pages.subarray(0, 8).toString("ascii") === "OU5END01" &&
      built.room.length === 121 && built.room[4 * 11 + 5] === 0x44,
      "G2", `endgame-pages.bin is ${built.pages.length} B (7 screens), endgame-room.bin 121 B`);

// The shipped pack (alpha1.ts's layout: "OU5A1RES", the header size at 12, the
// entry size at 14, the count at 16; a 32-byte name, offset and size per entry).
if (packPath) {
  const pack = readFileSync(packPath);
  const entry = (name: string): Buffer | undefined => {
    if (pack.subarray(0, 8).toString("ascii") !== "OU5A1RES") return undefined;
    const header = pack.readUInt16LE(12), size = pack.readUInt16LE(14), count = pack.readUInt32LE(16);
    for (let i = 0; i < count; i++) {
      const toc = header + i * size;
      const raw = pack.subarray(toc, toc + 32);
      if (raw.subarray(0, raw.indexOf(0) < 0 ? 32 : raw.indexOf(0)).toString("ascii") !== name) continue;
      const at = pack.readUInt32LE(toc + 32);
      return pack.subarray(at, at + pack.readUInt32LE(toc + 36));
    }
    return undefined;
  };
  const shippedPages = entry("endgame-pages.bin"), shippedRoom = entry("endgame-room.bin");
  check(!!shippedPages && !!shippedRoom && Buffer.compare(shippedPages, Buffer.from(built.pages)) === 0 &&
        Buffer.compare(shippedRoom, Buffer.from(built.room)) === 0,
        "G3", "the shipped pack's endgame-pages.bin and endgame-room.bin are this build, byte for byte");
}

if (pngDir) {
  mkdirSync(pngDir, { recursive: true });
  pages.forEach((p, i) => {
    const png = new PNG({ width: PAGE_W, height: PAGE_H });
    for (let k = 0; k < p.length; k++) {
      const [r, g, b] = EGA[p[k]!]!;
      png.data[k * 4] = r; png.data[k * 4 + 1] = g; png.data[k * 4 + 2] = b; png.data[k * 4 + 3] = 255;
    }
    writeFileSync(resolve(pngDir, i < STORY_PAGES ? `story-${i}.png` : "scroll.png"), PNG.sync.write(png));
  });
  console.log(`  wrote ${pages.length} PNGs to ${pngDir}`);
}
void PAGE_H;
console.log(`A4-END1 ending screens: ${checks - failures}/${checks} checks GREEN`);
process.exit(failures ? 1 : 0);
