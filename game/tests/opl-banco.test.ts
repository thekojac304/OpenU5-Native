/**
 * Lector del banco de timbres «Miles AIL» (`.OPL`) del parche de Ultima V.
 *
 * DOS BLOQUES, a propósito:
 *   1. SINTÉTICO — bancos construidos byte a byte aquí mismo. Corre SIEMPRE, también en
 *      el repositorio público, y es el que fija el contrato del formato.
 *   2. `FAT.OPL` REAL — sólo en un árbol que tenga la copia del usuario (`original/`).
 *      Es el que comprueba que lo que dice la cabecera de `bank.ts` sobre el fichero de
 *      verdad SIGUE siendo cierto, incluidas las dos invariantes que fijan el orden de
 *      los campos (ondas en 0..3, bits 6-7 de 0xC0 siempre a cero).
 *
 * ⚠️ La lectura del fichero real va DENTRO de los `it` — `describe.skip` ejecuta su
 * cuerpo al recolectar, así que un `readFileSync` suelto tumbaría el fichero entero en
 * el árbol público (ver la advertencia de `assets-opcionales.ts`).
 */
import { describe, expect, it } from "vitest";
import { readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import {
  MILES_BANK_MELODIC,
  MILES_BANK_PERCUSSION,
  parseMilesOplBank,
} from "../src/ui/opl/bank.js";
import { describeSiViaja } from "./assets-opcionales.js";

const AQUI = dirname(fileURLToPath(import.meta.url));
const FAT_OPL_REL = "original/u5/ultima5/FAT.OPL";
const FAT_OPL_ABS = join(AQUI, "..", "..", FAT_OPL_REL);

interface TimbreSpec {
  bank: number;
  patch: number;
  fixedNote: number;
  /** 11 bytes: mod(5) + fb/cnt(1) + car(5). */
  bytes: readonly number[];
}

/** Construye un banco `.OPL` válido: índice de 6 bytes por entrada + bloques de 14. */
function buildBank(specs: readonly TimbreSpec[]): Uint8Array {
  const indexBytes = specs.length * 6 + 2;
  const out = new Uint8Array(indexBytes + specs.length * 14);
  const dv = new DataView(out.buffer);
  specs.forEach((s, i) => {
    const off = indexBytes + i * 14;
    const p = i * 6;
    out[p] = s.patch;
    out[p + 1] = s.bank;
    dv.setUint32(p + 2, off, true);
    dv.setUint16(off, 14, true);
    out[off + 2] = s.fixedNote;
    for (let k = 0; k < 11; k++) out[off + 3 + k] = s.bytes[k]!;
  });
  dv.setUint16(specs.length * 6, 0xffff, true);
  return out;
}

/** Los 11 bytes del piano real de `FAT.OPL` (patch 0), como caso testigo. */
const PIANO = [0x21, 0x8f, 0xf2, 0x45, 0x00, 0x08, 0x21, 0x06, 0xf2, 0x76, 0x00] as const;

describe("parseMilesOplBank — formato sintético", () => {
  it("lee el índice y los bloques, en el orden del fichero", () => {
    const bank = parseMilesOplBank(
      buildBank([
        { bank: 0, patch: 0, fixedNote: 0, bytes: PIANO },
        { bank: 0, patch: 48, fixedNote: 0, bytes: PIANO },
      ]),
    );
    expect(bank.timbres).toHaveLength(2);
    expect(bank.timbres.map((t) => t.patch)).toEqual([0, 48]);
  });

  it("reparte los 11 bytes en modulador / fb-cnt / portadora", () => {
    const bank = parseMilesOplBank(
      buildBank([{ bank: 0, patch: 0, fixedNote: 0, bytes: PIANO }]),
    );
    const t = bank.get(0, 0)!;
    expect(t.modulator).toEqual({
      amVibEgKsrMult: 0x21,
      kslTl: 0x8f,
      attackDecay: 0xf2,
      sustainRelease: 0x45,
      waveform: 0x00,
    });
    expect(t.feedbackConnection).toBe(0x08);
    expect(t.carrier).toEqual({
      amVibEgKsrMult: 0x21,
      kslTl: 0x06,
      attackDecay: 0xf2,
      sustainRelease: 0x76,
      waveform: 0x00,
    });
  });

  it("conserva el 0xC0 del fichero SIN los bits de estéreo (los pone voices.ts)", () => {
    const bank = parseMilesOplBank(
      buildBank([{ bank: 0, patch: 0, fixedNote: 0, bytes: PIANO }]),
    );
    expect(bank.get(0, 0)!.feedbackConnection & 0x30).toBe(0);
  });

  it("distingue melódicos de percusión, y la percusión lleva NOTA fija", () => {
    const bank = parseMilesOplBank(
      buildBank([
        { bank: MILES_BANK_MELODIC, patch: 0, fixedNote: 0, bytes: PIANO },
        { bank: MILES_BANK_PERCUSSION, patch: 35, fixedNote: 35, bytes: PIANO },
      ]),
    );
    expect(bank.melodic(0).fixedNote).toBe(0);
    expect(bank.percussion(35)!.fixedNote).toBe(35);
    // Y no se confunden: mismo "patch" 35 en bancos distintos son timbres distintos.
    expect(bank.get(MILES_BANK_MELODIC, 35)).toBeUndefined();
  });

  it("melodic() cae al patch 0 ante un programa ausente (sonar raro > callar)", () => {
    const bank = parseMilesOplBank(
      buildBank([{ bank: 0, patch: 0, fixedNote: 0, bytes: PIANO }]),
    );
    expect(bank.melodic(77)).toBe(bank.get(0, 0));
  });

  it("percussion() devuelve undefined si el banco no trae esa nota", () => {
    const bank = parseMilesOplBank(
      buildBank([{ bank: 0, patch: 0, fixedNote: 0, bytes: PIANO }]),
    );
    expect(bank.percussion(42)).toBeUndefined();
  });
});

describe("parseMilesOplBank — un banco corrupto es un ROJO, no un silencio", () => {
  it("rechaza un fichero que no llega ni a una entrada", () => {
    expect(() => parseMilesOplBank(new Uint8Array(3))).toThrow(/truncado/);
  });

  it("un índice cortado a media entrada no se lee «lo que se pueda»", () => {
    const ok = buildBank([{ bank: 0, patch: 0, fixedNote: 0, bytes: PIANO }]);
    // Cortar por el centinela deja una entrada íntegra cuyo OFFSET ya no cabe: salta
    // primero la comprobación de límites, que es la que describe el daño de verdad.
    expect(() => parseMilesOplBank(ok.subarray(0, 6))).toThrow(/fuera del fichero/);
  });

  it("rechaza un índice que se sale del fichero sin encontrar el centinela", () => {
    // Llegar a esa rama cuesta: un índice sin centinela suele morir antes en la
    // comprobación de límites. Hace falta que TODAS las entradas validen y que el
    // recorrido acabe con menos de 2 bytes por leer. Tres entradas idénticas
    // {patch:14, bank:0, offset:0} lo consiguen — el offset 0 apunta al propio índice,
    // cuyos dos primeros bytes (14, 0) se leen como un `size` de 14 perfectamente
    // válido — y el byte suelto del final deja el recorrido a mitad de entrada.
    const b = new Uint8Array(19);
    for (const base of [0, 6, 12]) b[base] = 14;
    expect(() => parseMilesOplBank(b)).toThrow(/centinela/);
  });

  it("rechaza un offset que apunta fuera del fichero", () => {
    const b = buildBank([{ bank: 0, patch: 0, fixedNote: 0, bytes: PIANO }]);
    new DataView(b.buffer).setUint32(2, 9999, true);
    expect(() => parseMilesOplBank(b)).toThrow(/fuera del fichero/);
  });

  it("rechaza un bloque que no sea de 2 operadores en vez de adivinar", () => {
    const b = buildBank([{ bank: 0, patch: 0, fixedNote: 0, bytes: PIANO }]);
    const off = new DataView(b.buffer).getUint32(2, true);
    new DataView(b.buffer).setUint16(off, 26, true);
    expect(() => parseMilesOplBank(b)).toThrow(/2 operadores/);
  });
});

describeSiViaja([FAT_OPL_REL], "FAT.OPL real — el banco que instala el parche", () => {
  const leeBanco = (): ReturnType<typeof parseMilesOplBank> =>
    parseMilesOplBank(new Uint8Array(readFileSync(FAT_OPL_ABS)));

  it("trae 181 timbres: 128 melódicos + 53 de percusión", () => {
    const bank = leeBanco();
    expect(bank.timbres).toHaveLength(181);
    const mel = bank.timbres.filter((t) => t.bank === MILES_BANK_MELODIC);
    const per = bank.timbres.filter((t) => t.bank === MILES_BANK_PERCUSSION);
    expect(mel).toHaveLength(128);
    expect(per).toHaveLength(53);
    expect(Math.min(...mel.map((t) => t.patch))).toBe(0);
    expect(Math.max(...mel.map((t) => t.patch))).toBe(127);
    expect(Math.min(...per.map((t) => t.patch))).toBe(35);
    expect(Math.max(...per.map((t) => t.patch))).toBe(87);
  });

  it("🔴 INVARIANTE QUE FIJA EL ORDEN: las 362 ondas caen en 0..3", () => {
    for (const t of leeBanco().timbres) {
      expect(t.modulator.waveform).toBeLessThanOrEqual(3);
      expect(t.carrier.waveform).toBeLessThanOrEqual(3);
    }
  });

  it("🔴 INVARIANTE QUE FIJA EL ORDEN: 0xC0 nunca enciende los bits 6-7", () => {
    for (const t of leeBanco().timbres) {
      expect(t.feedbackConnection & 0xc0).toBe(0);
    }
  });

  it("el fixedNote es 0 en TODO melódico y una nota MIDI en TODA percusión", () => {
    for (const t of leeBanco().timbres) {
      if (t.bank === MILES_BANK_MELODIC) expect(t.fixedNote).toBe(0);
      else {
        expect(t.fixedNote).toBeGreaterThan(0);
        expect(t.fixedNote).toBeLessThanOrEqual(127);
      }
    }
  });

  it("trae TODOS los programas que pide el corpus de Ultima V", () => {
    // Medidos el 2026-09-11 recorriendo los 15 XMI del parche (ver cabecera de voices.ts).
    const pedidos = [
      0, 6, 14, 19, 22, 24, 32, 34, 35, 46, 47, 48, 49, 50, 53, 56, 57, 58, 60, 62, 68,
      69, 71, 72, 73, 88, 91, 119,
    ];
    const bank = leeBanco();
    for (const p of pedidos) expect(bank.get(MILES_BANK_MELODIC, p)).toBeDefined();
  });
});
