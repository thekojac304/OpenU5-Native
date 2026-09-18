import { readFileSync } from "node:fs";
import { dirname, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { describe, expect, it } from "vitest";
import {
  ALPHA_SIGN_RECORD_BYTES,
  buildAlphaSignData,
  inspectAlphaSignData,
  type AlphaSignSource,
} from "../../native/tools/u5pack/alpha1-signs.js";

const root = resolve(dirname(fileURLToPath(import.meta.url)), "../..");
const source = JSON.parse(readFileSync(resolve(root, "game/assets/signs.json"), "utf8")) as AlphaSignSource[];

function key(sign: Pick<AlphaSignSource, "location" | "floor" | "x" | "y">): string {
  return `${sign.location}:${sign.floor}:${sign.x}:${sign.y}`;
}

describe("Alpha sign resource", () => {
  const packed = buildAlphaSignData(source);
  const decoded = inspectAlphaSignData(packed);
  const byCoordinate = new Map(decoded.map((sign) => [key(sign), sign]));

  it("round-trips every authoritative SIGNS.DAT record and its raw glyph payload", () => {
    expect(decoded).toHaveLength(source.length);
    expect(decoded).toHaveLength(79);
    expect(packed.readUInt32LE(4)).toBe(ALPHA_SIGN_RECORD_BYTES);
    for (const sign of source) {
      expect(byCoordinate.get(key(sign)), key(sign)).toMatchObject({ text: sign.text, raw: sign.raw });
    }
  });

  it("resolves the observed Iolo-area sign instead of the LOOK2 sentinel", () => {
    const sign = byCoordinate.get("0:0:55:66");
    expect(sign?.text).toBe("\n           \n BEWARE THE \n DEEP FOREST \n           \n");
    expect(sign?.text.trim()).toBe("BEWARE THE \n DEEP FOREST");
    expect(sign?.text).not.toBe("*");
  });

  it("samples independent surface, Underworld, town, and extra DATA.OVL signs", () => {
    const samples = ["0:0:95:148", "0:255:54:143", "1:0:14:20", "17:255:6:13", "32:0:15:19"];
    for (const coordinate of samples) {
      const actual = byCoordinate.get(coordinate);
      const expected = source.find((sign) => key(sign) === coordinate);
      expect(actual, coordinate).toBeDefined();
      expect(actual?.text, coordinate).toBe(expected?.text);
      expect(actual?.raw, coordinate).toEqual(expected?.raw);
    }
  });

  it("rejects a truncated or corrupt resource instead of exposing placeholder text", () => {
    expect(() => inspectAlphaSignData(packed.subarray(0, packed.length - 1))).toThrow(/length/);
    const corrupt = Buffer.from(packed);
    corrupt.writeUInt32LE(0xffffffff, 16 + 12);
    expect(() => inspectAlphaSignData(corrupt)).toThrow(/text slice/);
  });
});
