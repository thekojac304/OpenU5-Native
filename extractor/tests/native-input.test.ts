import { readFileSync } from "node:fs";
import { resolve, dirname } from "node:path";
import { fileURLToPath } from "node:url";
import { describe, expect, it } from "vitest";

const root = resolve(dirname(fileURLToPath(import.meta.url)), "../..");
const matrixSource = readFileSync(resolve(root, "native/targets/tdeck/main/keyboard_matrix.cpp"), "utf8");
const controllerSource = readFileSync(resolve(root, "native/targets/tdeck/main/input_controller.cpp"), "utf8");
const hardwareSource = readFileSync(resolve(root, "native/targets/tdeck/main/tdeck_input.cpp"), "utf8");

type Modifiers = { symbol: boolean; alt: boolean; shift: boolean };
type Edge = { code: string; pressed: boolean; modifiers: Modifiers; modifierKey: boolean };
const baseCodes = new Map([["0,0", "q"], ["0,3", "a"], ["1,0", "e"]]);
const symbolCodes = new Map([
  ["0,0", "#"], ["0,3", "*"], ["1,0", "2"], ["1,1", "4"],
  ["1,4", "8"], ["2,6", "6"],
]);
const modifierKeys = new Set(["0,2", "0,4", "1,6", "2,3"]);

class MatrixModel {
  private down = new Set<string>();
  private activeCodes = new Map<string, string>();
  private synchronized = false;
  desynchronize(): void { this.down.clear(); this.activeCodes.clear(); this.synchronized = false; }
  modifiers(): Modifiers { return this.readModifiers(this.down); }
  private readModifiers(keys: Set<string>): Modifiers {
    return { symbol: keys.has("0,2"), alt: keys.has("0,4"), shift: keys.has("1,6") || keys.has("2,3") };
  }
  private resolveCode(key: string, modifiers: Modifiers): string {
    let code = (modifiers.symbol ? symbolCodes : baseCodes).get(key) ?? "";
    if (modifiers.shift && code >= "a" && code <= "z") code = code.toUpperCase();
    return code;
  }
  snapshot(next: Set<string>): Edge[] {
    const modifiers = this.readModifiers(next);
    if (!this.synchronized) {
      this.down = new Set(next);
      for (const key of next) this.activeCodes.set(key, this.resolveCode(key, modifiers));
      this.synchronized = true;
      return [];
    }
    const edges: Edge[] = [];
    for (const key of new Set([...this.down, ...next])) {
      const was = this.down.has(key);
      const pressed = next.has(key);
      if (was === pressed) continue;
      const code = pressed ? this.resolveCode(key, modifiers) : (this.activeCodes.get(key) ?? "");
      if (pressed) this.activeCodes.set(key, code); else this.activeCodes.delete(key);
      edges.push({ code, pressed, modifiers, modifierKey: modifierKeys.has(key) });
    }
    this.down = new Set(next);
    return edges;
  }
}

class TrackballModel {
  private readonly last = new Map<string, number>();
  edge(direction: string, atUs: number): string | null {
    const previous = this.last.get(direction) ?? 0;
    if (atUs - previous < 65_000) return null;
    this.last.set(direction, atUs);
    return direction;
  }
}

describe("Milestone 5 T-Deck raw input contract", () => {
  it("keeps raw matrix input and source-local trackball debounce", () => {
    expect(hardwareSource).toContain("kKeyboardRawModeCommand = 0x03");
    expect(hardwareSource).toContain("snapshot[kKeyboardColumns]");
    expect(hardwareSource).toContain("keyboard_matrix_.desynchronize()");
    expect(controllerSource).toContain("keyboard-reserved-for-command-text");
    expect(controllerSource).toContain("last_trackball_us_");
  });

  it("uses trackball as the only directional input", () => {
    expect(controllerSource).not.toMatch(/case '[2468]'/);
    expect(controllerSource).toContain("case tdeck::RawInputKind::TrackballUp");
    expect(controllerSource).toContain("case tdeck::RawInputKind::TrackballDown");
    expect(hardwareSource).not.toContain("Optional fallback: Symbol+");
    const trackball = new TrackballModel();
    for (const direction of ["north", "south", "west", "east"]) {
      expect(trackball.edge(direction, 100_000)).toBe(direction);
      expect(trackball.edge(direction, 101_000)).toBeNull();
    }
  });

  it("distinguishes lowercase and shifted letters with press/release codes", () => {
    const matrix = new MatrixModel();
    matrix.snapshot(new Set());
    expect(matrix.snapshot(new Set(["0,0"]))[0]).toMatchObject({ code: "q", pressed: true,
      modifiers: { symbol: false, alt: false, shift: false } });
    expect(matrix.snapshot(new Set())[0]).toMatchObject({ code: "q", pressed: false });
    const shift = matrix.snapshot(new Set(["1,6"]))[0]!;
    expect(shift.modifierKey).toBe(true);
    expect(shift.modifiers.shift).toBe(true);
    expect(matrix.snapshot(new Set(["1,6", "0,0"]))[0]).toMatchObject({ code: "Q", pressed: true,
      modifiers: { shift: true } });
    expect(matrix.snapshot(new Set(["1,6"]))[0]).toMatchObject({ code: "Q", pressed: false });
  });

  it("retains Symbol, numbers and punctuation without movement", () => {
    const matrix = new MatrixModel();
    matrix.snapshot(new Set());
    const symbol = matrix.snapshot(new Set(["0,2"]))[0]!;
    expect(symbol).toMatchObject({ pressed: true, modifierKey: true, modifiers: { symbol: true } });
    expect(matrix.snapshot(new Set(["0,2", "1,0"]))[0]).toMatchObject({ code: "2", pressed: true,
      modifiers: { symbol: true } });
    expect(matrix.snapshot(new Set(["0,2"]))[0]).toMatchObject({ code: "2", pressed: false });
    expect(matrix.snapshot(new Set(["0,2", "0,3"]))[0]).toMatchObject({ code: "*", pressed: true,
      modifiers: { symbol: true } });
    expect(matrix.modifiers().symbol).toBe(true);
    expect(controllerSource).not.toContain("raw.modifiers.symbol");
  });

  it("retains Alt independently and clears modifiers on resynchronization", () => {
    const matrix = new MatrixModel();
    matrix.snapshot(new Set());
    const alt = matrix.snapshot(new Set(["0,4"]))[0]!;
    expect(alt).toMatchObject({ pressed: true, modifierKey: true, modifiers: { alt: true } });
    expect(matrix.snapshot(new Set(["0,4", "0,0"]))[0]).toMatchObject({ code: "q", pressed: true,
      modifiers: { alt: true, symbol: false, shift: false } });
    matrix.desynchronize();
    expect(matrix.modifiers()).toEqual({ symbol: false, alt: false, shift: false });
    expect(matrix.snapshot(new Set())).toEqual([]);
  });

  it("matches the LilyGO base and Symbol layer tables", () => {
    expect(matrixSource).toContain("{'q', 'w', 0, 'a', 0, ' ', 0}");
    expect(matrixSource).toContain("{'2', '4', '5', '@', '8', '7', 0}");
  });
});
