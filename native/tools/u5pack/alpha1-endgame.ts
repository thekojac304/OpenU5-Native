/**
 * Alpha 4 A4-END1 -- the ENDING's static screens, pre-composed for the device.
 *
 * ENDGAME.OVL story_screens (0x0000) composes six pages and the scroll's
 * background into the EGA backbuffer and presents each whole. Every one of them
 * is STATIC -- no name, date or state reaches them (the proclamation that is
 * printed over the scroll at runtime is the device's job, endgame_scene.cpp).
 * So they are composed here, once, from the user's own original files, exactly
 * as the overlay composes them, and the firmware only blits 4bpp pages: no LZW,
 * no .16 parser and no proportional text engine in flash.
 *
 * SOURCES, all from `original/u5/ultima5`, through the extractor's accepted
 * parsers (lzw.ts, pic16.ts, proport.ts):
 *   END1.16 / END2.16   the art, 3 sub-images each (page i = file 0x3df4[i], sub 0x3dee[i])
 *   TEXT.16             the gothic headlines (The = 0, Homecoming = 4, Dream = 5)
 *   ENDSC.16            the scroll
 *   PROPORT.PCS         the proportional glyphs (char - 0x20)
 *   END.DAT             the pages' text, NUL-terminated, at the offsets 0x3dca[i]
 *   DATA.OVL            the per-page tables (DS 0x3da6 .. 0x3e06), the glyph
 *                       widths (DS 0x50ca) and the space width (DS 0x5154 = 5);
 *                       file offset = DS + 0x10.
 *
 * COMPOSITION, page i (story_screens 0x0077-0x0185):
 *   1. put_char(0xff) clears window 0 to black (0x00b8);
 *   2. pages with 0x3e06[i] = 1 draw their headlines first, in code order:
 *      page 0 The (216,0) then Homecoming (152,28) (0x00d6 / 0x00e7), page 3
 *      Dream (224,0) then The (176,0) (0x01c4 / 0x01d6);
 *   3. the art (0x00fc), OPAQUE (no .16 here carries a mask) -- so the art's
 *      right edge overwrites the H of "Homecoming", as in 1988;
 *   4. the text through render_justified_text (FONT.OVL 0x0000), ported below
 *      instruction for instruction: white glyphs OR'ed over the page (EGA.DRV
 *      0x190e), two bands per page, the column re-chosen at every line.
 *   The scroll's background (0x01ed-0x0217): black, then ENDSC.16 at (40,0).
 *
 * CONTAINER "endgame-pages.bin":
 *   0   char magic[8] = "OU5END01"
 *   8   u16  width (320)   10 u16 height (200)   12 u16 pages (7)   14 u16 reserved
 *   16  pages x (width*height/2) bytes: 4bpp, nibble HIGH = left pixel, rows top
 *       to bottom; pages 0..5 = the story, 6 = the scroll's background.
 * "endgame-room.bin": MISCMAPS.DAT[0x210], the 11 x 11 room (11 rows of 0x10).
 */
import { readFileSync } from "node:fs";
import { resolve } from "node:path";
import { decompressLzw } from "../../../extractor/src/parsers/lzw.js";
import { parsePic16, type Pic16Image } from "../../../extractor/src/parsers/pic16.js";
import { parseProport, type ProportGlyph } from "../../../extractor/src/parsers/proport.js";

export const PAGE_W = 320;
export const PAGE_H = 200;
export const STORY_PAGES = 6;
const MAGIC = "OU5END01";

export interface EndgameSources {
  end: Pic16Image[][]; // [END1, END2]
  text: Pic16Image[];
  scroll: Pic16Image[];
  glyphs: ProportGlyph[];
  endDat: Uint8Array;
  data: Uint8Array; // DATA.OVL
  room: Uint8Array; // 121 bytes
}

export function loadEndgameSources(dir: string): EndgameSources {
  const read = (f: string) => new Uint8Array(readFileSync(resolve(dir, f)));
  const misc = read("MISCMAPS.DAT");
  const room = new Uint8Array(121);
  for (let row = 0; row < 11; row++) for (let col = 0; col < 11; col++) room[row * 11 + col] = misc[0x210 + row * 16 + col]!;
  return {
    end: [parsePic16(decompressLzw(read("END1.16"))), parsePic16(decompressLzw(read("END2.16")))],
    text: parsePic16(decompressLzw(read("TEXT.16"))),
    scroll: parsePic16(decompressLzw(read("ENDSC.16"))),
    glyphs: parseProport(decompressLzw(read("PROPORT.PCS"))),
    endDat: read("END.DAT"),
    data: read("DATA.OVL"),
    room,
  };
}

const ds = (data: Uint8Array, off: number) => data[off + 0x10]!;
const dsWord = (data: Uint8Array, off: number) => data[off + 0x10]! | (data[off + 0x11]! << 8);

/** The six pages' parameters, story_screens 0x0077-0x0157 over DATA.OVL. */
export interface PageLayout {
  file: number; sub: number; artX: number; artY: number; headlines: boolean;
  textOffset: number;
  left: [number, number]; right: [number, number]; // [0x5146, 0x5148], [0x514c, 0x514e]
  cut: number; bottom: number; penX: number; penY: number; // 0x5150, 0x5152, 0x5156, 0x5158
}

export function pageLayouts(data: Uint8Array): PageLayout[] {
  return Array.from({ length: STORY_PAGES }, (_, i) => ({
    file: ds(data, 0x3df4 + i), sub: ds(data, 0x3dee + i),
    artX: ds(data, 0x3dfa + i), artY: ds(data, 0x3e00 + i),
    headlines: ds(data, 0x3e06 + i) === 1,
    textOffset: dsWord(data, 0x3dca + i * 2),
    left: [ds(data, 0x3da6 + i * 2), ds(data, 0x3da7 + i * 2)],
    right: [dsWord(data, 0x3db2 + i * 4), dsWord(data, 0x3db4 + i * 4)],
    cut: ds(data, 0x3dd6 + i), bottom: ds(data, 0x3ddc + i),
    penX: ds(data, 0x3de2 + i), penY: ds(data, 0x3de8 + i),
  }));
}

/** kernel 0x0d4c -> EGA.DRV 0x12b4 (SEL 0x4b): an opaque sub-image blit, clipped to the screen. */
export function blit(page: Uint8Array, img: Pic16Image, x0: number, y0: number): void {
  for (let y = 0; y < img.height; y++) {
    const py = y0 + y;
    if (py < 0 || py >= PAGE_H) continue;
    for (let x = 0; x < img.width; x++) {
      const px = x0 + x;
      if (px < 0 || px >= PAGE_W) continue;
      page[py * PAGE_W + px] = img.pixels[y * img.width + x]!;
    }
  }
}

/** What the engine drew, for the checks: one entry per glyph and per line. */
export interface TextTrace {
  glyphs: { code: number; x: number; y: number }[];
  lines: { start: number; end: number; y: number; x0: number; x1: number; justified: boolean; band: number }[];
}

/**
 * render_justified_text, FONT.OVL 0x0000-0x02a1 (`push text; call` with the
 * per-page globals already set by story_screens). Variables keep the
 * original's frame slots: lineStart [bp-0xc], lineEnd [bp-4], nat [bp-2],
 * width [bp-0xa], spaces [bp-8], extra [bp-6].
 */
export function renderJustified(
  page: Uint8Array, text: Uint8Array, start: number, lay: PageLayout,
  glyphs: ProportGlyph[], data: Uint8Array, trace?: TextTrace,
): void {
  const W = (c: number) => ds(data, 0x50ca + c);
  const space = dsWord(data, 0x5154);
  const at = (i: number) => text[start + i] ?? 0;
  const band = (y: number) => (lay.cut < y && y < lay.bottom ? 1 : 0); // 0x000f-0x0028 / 0x0243-0x025c
  let penX = lay.penX, penY = lay.penY;
  let b = band(penY);
  let width = lay.right[b] - lay.left[b];      // 0x002f-0x0037
  let lineStart = 0;                            // 0x003f
  let nat = penX - lay.left[b];                 // 0x0042-0x0049: the first line starts at the caller's pen
  const draw = (code: number, x: number, y: number) => { // EGA.DRV 0x190e: set bits OR'ed white
    if (y >= 0xc0) return; // 0x01c9 / 0x021f: the clip skips the draw, never the layout
    const g = glyphs[code - 0x20];
    trace?.glyphs.push({ code, x, y });
    if (!g) return;
    for (let gy = 0; gy < g.height; gy++)
      for (let gx = 0; gx < g.width; gx++) {
        if (!g.mask[gy * g.width + gx]) continue;
        const px = x + gx, py = y + gy;
        if (px >= 0 && px < PAGE_W && py >= 0 && py < PAGE_H) page[py * PAGE_W + px] = 15;
      }
  };
  while (at(lineStart) !== 0) { // 0x027b-0x0284
    // Break scan 0x0079-0x009e.
    let spaces = 0, cx = lineStart, si = nat;
    for (;;) {
      const c = at(cx);
      if (c === 0 || width <= si || c === 0x0a) break;
      if (c > 0x20) si += c === 0x7b ? 0x0f : c === 0x5f ? 0 : W(c) + 1; // 0x0050-0x0076
      else { si += space; ++spaces; }                                      // 0x0099-0x009d
      ++cx;
    }
    // Backtrack 0x00a0-0x01ad: to the last space, or a soft hyphen with room for its '-'.
    const hy = W(0x2d);
    for (;;) {
      if (at(cx) === 0 && si < width) break;  // 0x00c5-0x00cc
      if (at(cx) === 0x0a) break;              // 0x00ce-0x00d5
      --cx;
      if (lineStart >= cx) break;              // 0x00d8
      const c = at(cx);
      if (c === 0x20) { si -= space; --spaces; break; }        // 0x00e1-0x00ed
      if (c === 0x5f && hy + si + 1 < width) { si += hy + 1; break; } // 0x016e-0x017d
      if (c === 0x5f || c === 0x7b) continue;                  // 0x0184-0x0199
      si -= W(c) + 1;                                          // 0x019c-0x01ab
    }
    // Draw 0x00f0-0x0207, justifying unless the line ends the text or a paragraph.
    nat = si;
    let extra = width - si;
    const lineEnd = cx;
    const lastc = at(lineEnd);
    const justified = spaces !== 0 && lastc !== 0 && lastc !== 0x0a;
    const x0 = penX;
    for (let i = lineStart; i < lineEnd; i++) {
      const c = at(i);
      if (c <= 0x20) {                                         // 0x012f-0x0167
        penX += space;
        if (spaces !== 0 && lastc !== 0 && lastc !== 0x0a) {
          const q = Math.trunc(extra / spaces);                // idiv: truncates toward zero
          penX += q; extra -= q; --spaces;
        }
        continue;
      }
      if (c === 0x7b) { penX += 0x0f; continue; }              // 0x01b4-0x01be
      if (c === 0x5f) continue;                                // 0x01c4
      draw(c, penX, penY);                                     // 0x01d1-0x01e8
      penX += W(c) + 1;                                        // 0x01eb-0x01fa
    }
    trace?.lines.push({ start: lineStart, end: lineEnd, y: penY, x0, x1: penX, justified, band: b });
    // Next line 0x020a-0x0278.
    lineStart = lineEnd;
    if (at(lineEnd) === 0) break;
    if (at(lineEnd) === 0x5f) draw(0x2d, penX, penY);          // 0x021a-0x0236: the hyphen
    nat = 0;
    penY += 9;                                                 // 0x023e
    b = band(penY);
    penX = lay.left[b];
    width = lay.right[b] - penX;
    ++lineStart;                                               // 0x0278: skip the break character
  }
}

export function composeStoryPage(src: EndgameSources, i: number, trace?: TextTrace): Uint8Array {
  const page = new Uint8Array(PAGE_W * PAGE_H); // put_char(0xff): black
  const lay = pageLayouts(src.data)[i]!;
  if (lay.headlines) {
    const t = src.text;
    if (i === 0) { blit(page, t[0]!, 216, 0); blit(page, t[4]!, 152, 28); }
    else { blit(page, t[5]!, 224, 0); blit(page, t[0]!, 176, 0); }
  }
  blit(page, src.end[lay.file]![lay.sub]!, lay.artX, lay.artY);
  renderJustified(page, src.endDat, lay.textOffset, lay, src.glyphs, src.data, trace);
  return page;
}

export function composeScrollPage(src: EndgameSources): Uint8Array {
  const page = new Uint8Array(PAGE_W * PAGE_H);
  blit(page, src.scroll[0]!, 40, 0); // 0x020b-0x0217
  return page;
}

export function pack4bpp(page: Uint8Array): Buffer {
  const out = Buffer.alloc(page.length / 2);
  for (let i = 0; i < out.length; i++) out[i] = ((page[i * 2]! & 15) << 4) | (page[i * 2 + 1]! & 15);
  return out;
}

export function buildEndgamePages(dir: string): { pages: Buffer; room: Buffer } {
  const src = loadEndgameSources(dir);
  const header = Buffer.alloc(16);
  header.write(MAGIC, 0, "ascii");
  header.writeUInt16LE(PAGE_W, 8);
  header.writeUInt16LE(PAGE_H, 10);
  header.writeUInt16LE(STORY_PAGES + 1, 12);
  const pages = [...Array.from({ length: STORY_PAGES }, (_, i) => composeStoryPage(src, i)), composeScrollPage(src)];
  return { pages: Buffer.concat([header, ...pages.map(pack4bpp)]), room: Buffer.from(src.room) };
}
