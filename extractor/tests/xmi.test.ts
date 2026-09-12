/**
 * Conversión XMI → MIDI de las pistas del Ultima V Upgrade Patch.
 *
 * 🔴 LOS XMI ESTÁN EN DOS SITIOS SEGÚN LA COPIA, y este test lo aprendió por las malas:
 * hasta el 2026-09-11 miraba SÓLO en `<u5>/upgrade/` y en una instalación con el parche
 * aplicado en plano (los XMI sueltos junto al ULTIMA.EXE) reventaba con un ENOENT que no
 * nombraba la causa — parecía que faltaba el parche cuando estaba entero. Es el mismo
 * defecto que tenía el pipeline, y se arregla igual: se prueban las dos ubicaciones.
 *
 * Sin ninguna de las dos (copia sin parche) el test se SALTA con motivo visible, no falla:
 * el parche es opcional y su ausencia no acusa a nadie.
 */
import { existsSync, readFileSync, readdirSync } from "node:fs";
import { describe, expect, it } from "vitest";
import { xmiToMidi } from "../src/audio/xmi2midi.js";
import { U5_DIR } from "./helpers.js";

/** Primer directorio que contenga XMI del parche: plano o bajo `upgrade/`. */
function dirDelParche(): string | undefined {
  for (const dir of [U5_DIR, `${U5_DIR}/upgrade`]) {
    if (!existsSync(dir)) continue;
    if (readdirSync(dir).some((f) => f.toUpperCase().endsWith(".XMI"))) return dir;
  }
  return undefined;
}

const DIR = dirDelParche();
const conParche = DIR !== undefined;

if (!conParche) {
  // eslint-disable-next-line no-console
  console.warn(
    "\n⚠️  xmi.test.ts SALTADO: no hay .XMI en original/u5/ultima5[/upgrade].\n" +
      "   Es lo esperado en una copia sin el Ultima V Upgrade Patch aplicado.\n",
  );
}

describe.skipIf(!conParche)("xmiToMidi", () => {
  /** Los 15 del tracklist; una copia puede traer además otros sueltos, y da igual. */
  const DEL_PARCHE = [
    "U5THEME", "BRITLAND", "WRLDBLW", "STONES", "RULEBRIT", "BLCKTHRN", "ENGGMNT",
    "HALLS", "FANFARE", "HORNPIPE", "GREYSON", "LADYNAN", "MONARCH", "REUNION", "AMIGA",
  ];

  it("convierte todos los .XMI del parche comunitario a MIDI válido", () => {
    const xmis = readdirSync(DIR!).filter((f) => f.toUpperCase().endsWith(".XMI"));
    // Las 15 del parche tienen que estar; que haya extras no es un rojo.
    for (const esperado of DEL_PARCHE) {
      expect(
        xmis.some((f) => f.toUpperCase() === `${esperado}.XMI`),
        `falta ${esperado}.XMI`,
      ).toBe(true);
    }
    for (const name of xmis) {
      const midis = xmiToMidi(new Uint8Array(readFileSync(`${DIR}/${name}`)));
      expect(midis.length, name).toBeGreaterThanOrEqual(1);
      for (const midi of midis) {
        // Header MThd + longitud 6 + formato 0
        expect(Array.from(midi.subarray(0, 4)), name).toEqual([0x4d, 0x54, 0x68, 0x64]);
        expect(midi.length, name).toBeGreaterThan(100);
      }
    }
  });

  it("U5THEME produce una canción con eventos de nota", () => {
    const [midi] = xmiToMidi(new Uint8Array(readFileSync(`${DIR}/U5THEME.XMI`)));
    // Busca al menos un Note On (0x9n con velocidad > 0) en el track
    let noteOns = 0;
    for (let i = 22; i < midi!.length - 2; i++) {
      if ((midi![i]! & 0xf0) === 0x90 && midi![i + 2]! > 0) noteOns++;
    }
    expect(noteOns).toBeGreaterThan(50);
  });
});
