/**
 * Secuenciador: lectura del SMF, colocación de los eventos en el tiempo y reproducción.
 *
 * Lo que se vigila aquí es el RELOJ, que es donde un error no se oye como un error sino
 * como «la música va rara»: la conversión tick→muestra, los cambios de tempo, el status
 * de ejecución (running status) del MIDI y el resampleo a la tasa de salida.
 */
import { describe, expect, it } from "vitest";
import type { MilesOplBank, OplTimbre } from "../src/ui/opl/bank.js";
import { OPL_CLOCK_HZ } from "../src/ui/opl/chip.js";
import {
  eventSampleTimes,
  OplSongPlayer,
  parseSmf,
  type Smf,
} from "../src/ui/opl/sequencer.js";

/** Envuelve un track crudo en un SMF de formato 0. */
function buildSmf(division: number, track: readonly number[]): Uint8Array {
  const t = [...track, 0x00, 0xff, 0x2f, 0x00];
  return new Uint8Array([
    0x4d, 0x54, 0x68, 0x64, 0, 0, 0, 6, 0, 0, 0, 1, (division >> 8) & 0xff, division & 0xff,
    0x4d, 0x54, 0x72, 0x6b,
    (t.length >>> 24) & 0xff, (t.length >>> 16) & 0xff, (t.length >>> 8) & 0xff, t.length & 0xff,
    ...t,
  ]);
}

/** El tempo que emite el extractor: 500000 µs/negra. */
const TEMPO_500K = [0x00, 0xff, 0x51, 0x03, 0x07, 0xa1, 0x20];

function timbre(): OplTimbre {
  const op = {
    amVibEgKsrMult: 0x21,
    kslTl: 0x00,
    attackDecay: 0xf0,
    sustainRelease: 0x0f,
    waveform: 0,
  };
  return {
    bank: 0, patch: 0, fixedNote: 0,
    modulator: { ...op }, carrier: { ...op },
    feedbackConnection: 0x08,
  };
}

function banco(): MilesOplBank {
  const t = timbre();
  return { timbres: [t], get: () => t, melodic: () => t, percussion: () => undefined };
}

const pico = (b: Float32Array): number => b.reduce((m, x) => Math.max(m, Math.abs(x)), 0);

describe("parseSmf", () => {
  it("lee la division y los eventos", () => {
    const smf = parseSmf(buildSmf(60, [...TEMPO_500K, 0x00, 0x90, 60, 100, 0x20, 0x90, 60, 0]));
    expect(smf.division).toBe(60);
    const notas = smf.events.filter((e) => (e.bytes[0]! & 0xf0) === 0x90);
    expect(notas).toHaveLength(2);
    expect(notas[0]!.tick).toBe(0);
    expect(notas[1]!.tick).toBe(0x20);
  });

  it("🔴 entiende el running status (un mensaje sin su byte de estado)", () => {
    // Dos note-on seguidos, el segundo SIN repetir el 0x90: es MIDI legal y frecuente.
    const smf = parseSmf(buildSmf(60, [0x00, 0x90, 60, 100, 0x10, 62, 100]));
    const notas = smf.events.filter((e) => (e.bytes[0]! & 0xf0) === 0x90);
    expect(notas).toHaveLength(2);
    expect(notas[1]!.bytes).toEqual([0x90, 62, 100]);
  });

  it("se salta el SysEx sin descolocarse", () => {
    const smf = parseSmf(
      buildSmf(60, [0x00, 0xf0, 0x03, 0x01, 0x02, 0xf7, 0x00, 0x90, 60, 100]),
    );
    const notas = smf.events.filter((e) => (e.bytes[0]! & 0xf0) === 0x90);
    expect(notas).toHaveLength(1);
    expect(notas[0]!.bytes).toEqual([0x90, 60, 100]);
  });

  it("rechaza lo que no es un MIDI", () => {
    expect(() => parseSmf(new Uint8Array(20))).toThrow(/MThd/);
  });

  it("rechaza una division SMPTE en vez de interpretarla mal", () => {
    expect(() => parseSmf(buildSmf(0xe728, [0x00, 0x90, 60, 100]))).toThrow(/division/);
  });
});

describe("eventSampleTimes — tick → muestra", () => {
  it("🔴 division 60 + tempo 500000 dan los 120 ticks/s del XMI", () => {
    const smf = parseSmf(buildSmf(60, [...TEMPO_500K, 0x78, 0x90, 60, 100])); // 0x78 = 120
    const t = eventSampleTimes(smf, OPL_CLOCK_HZ);
    const nota = smf.events.findIndex((e) => (e.bytes[0]! & 0xf0) === 0x90);
    // 120 ticks a 120 ticks/s = exactamente 1 segundo.
    expect(t[nota]!).toBeCloseTo(OPL_CLOCK_HZ, 3);
  });

  it("honra un cambio de tempo a mitad de pista", () => {
    const smf = parseSmf(
      buildSmf(60, [
        ...TEMPO_500K,
        0x3c, 0xff, 0x51, 0x03, 0x03, 0xd0, 0x90, // en el tick 60: 250000 µs (el doble de rápido)
        0x3c, 0x90, 60, 100, // 60 ticks más tarde, ya al tempo nuevo
      ]),
    );
    const t = eventSampleTimes(smf, OPL_CLOCK_HZ);
    const nota = smf.events.findIndex((e) => (e.bytes[0]! & 0xf0) === 0x90);
    // 60 ticks a 414,3 muestras + 60 ticks a 207,15 = 37287
    expect(t[nota]!).toBeCloseTo(24858 + 12429, 0);
  });

  it("escala con la tasa de muestreo que se le pida", () => {
    const smf = parseSmf(buildSmf(60, [...TEMPO_500K, 0x78, 0x90, 60, 100]));
    const nota = smf.events.findIndex((e) => (e.bytes[0]! & 0xf0) === 0x90);
    expect(eventSampleTimes(smf, 48000)[nota]!).toBeCloseTo(48000, 3);
  });
});

describe("OplSongPlayer", () => {
  const cancion = (): Smf =>
    parseSmf(buildSmf(60, [...TEMPO_500K, 0x00, 0x90, 69, 127, 0x78, 0x90, 69, 0]));

  it("suena: una nota al principio produce audio", () => {
    const p = new OplSongPlayer({ bank: banco(), smf: cancion(), loop: false });
    const l = new Float32Array(4096);
    const r = new Float32Array(4096);
    p.render(l, r, 4096);
    expect(pico(l)).toBeGreaterThan(0);
  });

  it("🔴 el silencio inicial se respeta: nada suena antes de su evento", () => {
    // La nota entra en el tick 120 = 1 segundo. El primer bloque tiene que ser mudo.
    const smf = parseSmf(buildSmf(60, [...TEMPO_500K, 0x78, 0x90, 69, 127]));
    const p = new OplSongPlayer({ bank: banco(), smf, loop: false });
    const l = new Float32Array(8192);
    const r = new Float32Array(8192);
    p.render(l, r, 8192);
    expect(pico(l)).toBe(0);
  });

  it("resamplea: pedir N muestras devuelve N, a cualquier tasa", () => {
    for (const sampleRate of [44100, 48000, OPL_CLOCK_HZ, 96000]) {
      const p = new OplSongPlayer({ bank: banco(), smf: cancion(), sampleRate, loop: false });
      const l = new Float32Array(3000);
      const r = new Float32Array(3000);
      p.render(l, r, 3000);
      expect(l.every((x) => Number.isFinite(x))).toBe(true);
      expect(r.every((x) => Number.isFinite(x))).toBe(true);
    }
  });

  it("🔴 a 48 kHz una nota cae donde toca (el resampleo no descoloca el reloj)", () => {
    // Nota en el tick 120 = 1 s. A 48000 tiene que empezar a sonar cerca de la muestra 48000.
    const smf = parseSmf(buildSmf(60, [...TEMPO_500K, 0x78, 0x90, 69, 127]));
    const p = new OplSongPlayer({ bank: banco(), smf, sampleRate: 48000, loop: false });
    const l = new Float32Array(60000);
    const r = new Float32Array(60000);
    p.render(l, r, 60000);
    let primera = -1;
    for (let i = 0; i < l.length; i++) {
      if (Math.abs(l[i]!) > 1e-4) {
        primera = i;
        break;
      }
    }
    expect(primera).toBeGreaterThan(47000);
    expect(primera).toBeLessThan(49000);
  });

  it("sin bucle acaba, y lo dice", () => {
    const p = new OplSongPlayer({ bank: banco(), smf: cancion(), loop: false });
    const l = new Float32Array(4096);
    const r = new Float32Array(4096);
    expect(p.ended).toBe(false);
    // La pista dura 1 s + 1 s de cola: con 4 s de render sobra.
    for (let i = 0; i < 50; i++) p.render(l, r, 4096);
    expect(p.ended).toBe(true);
  });

  it("con bucle NO acaba nunca", () => {
    const p = new OplSongPlayer({ bank: banco(), smf: cancion(), loop: true });
    const l = new Float32Array(4096);
    const r = new Float32Array(4096);
    for (let i = 0; i < 50; i++) p.render(l, r, 4096);
    expect(p.ended).toBe(false);
  });

  it("🔴 NO sintetiza más de lo que pide el bloque (el defecto del «pop»)", () => {
    /**
     * La primera versión rellenaba un búfer de 2048 muestras de chip en cuanto se
     * agotaba. Con bloques de 128 —los de un `AudioWorklet`— eso significaba que catorce
     * de cada quince llamadas no hacían nada y la quinceava sintetizaba 2048 de golpe.
     * Medido a 48 kHz con un presupuesto de 2,67 ms por llamada: mediana 0,001 ms, pico
     * 5,996 ms. Cada pico que se pasaba del plazo era un CHASQUIDO audible.
     *
     * 🔴 Y NO SE VEÍA EN UNA PRUEBA OFFLINE: sin plazo que incumplir, el fichero
     * renderizado salía perfecto, sin una sola discontinuidad. Por eso esto NO comprueba
     * la señal, sino la FORMA DEL TRABAJO: cuántas muestras de chip llega a tener el
     * búfer. Si alguien vuelve a rellenar de golpe, el número se dispara y esto enrojece.
     */
    const p = new OplSongPlayer({ bank: banco(), smf: cancion(), sampleRate: 48000 });
    const priv = p as unknown as { have: number };
    const l = new Float32Array(128);
    const r = new Float32Array(128);
    let maxHave = 0;
    for (let i = 0; i < 200; i++) {
      p.render(l, r, 128);
      maxHave = Math.max(maxHave, priv.have);
    }
    // 128 de salida a 48 kHz consumen ~133 de chip; con el margen del interpolador, ~135.
    // Un relleno por lotes dejaría `have` en 2048.
    expect(maxHave).toBeLessThan(160);
  });

  it("el bloque grande del ScriptProcessor (4096) también se atiende entero", () => {
    // La vía de respaldo pide bloques de 4096; el búfer tiene que crecer una vez y ya.
    const p = new OplSongPlayer({ bank: banco(), smf: cancion(), sampleRate: 48000 });
    const l = new Float32Array(4096);
    const r = new Float32Array(4096);
    p.render(l, r, 4096);
    expect(l.every((x) => Number.isFinite(x))).toBe(true);
    expect(pico(l)).toBeGreaterThan(0);
  });

  it("🔴 el bucle empalma en el ÚLTIMO EVENTO, sin cola ni corte", () => {
    /**
     * La versión anterior esperaba UN SEGUNDO tras el último evento y luego llamaba a
     * `allNotesOff()`. Medido el 2026-09-11: las 15 pistas acaban con sus note-off en el
     * último tick (cero notas colgadas), así que esa cola era silencio puro — y encima
     * los releases de este banco tardan MÁS de 3 s, con lo que el corte llegaba a mitad.
     * Silencio audible cada vuelta Y un tijeretazo.
     *
     * La canción de prueba dura 1 s exacto (nota en el tick 0, note-off en el 120). Si el
     * bucle empalma bien, el segundo pase suena tan fuerte como el primero; con la cola de
     * un segundo, a 1,2 s no habría más que el release apagándose.
     */
    const p = new OplSongPlayer({ bank: banco(), smf: cancion(), sampleRate: 48000, loop: true });
    const total = Math.floor(48000 * 2.5);
    const buf = new Float32Array(total);
    const l = new Float32Array(128);
    const r = new Float32Array(128);
    for (let i = 0; i * 128 < total - 128; i++) {
      p.render(l, r, 128);
      buf.set(l, i * 128);
    }
    const rms = (a: number, b: number): number => {
      let s = 0;
      for (let i = Math.floor(a * 48000); i < Math.floor(b * 48000); i++) s += buf[i]! * buf[i]!;
      return Math.sqrt(s / ((b - a) * 48000));
    };
    const primeraVuelta = rms(0.2, 0.5);
    const segundaVuelta = rms(1.2, 1.5);
    expect(primeraVuelta).toBeGreaterThan(0.001);
    // Con la cola de un segundo esto era prácticamente silencio.
    expect(segundaVuelta).toBeGreaterThan(primeraVuelta * 0.5);
  });

  it("reset vuelve al principio sin dejar notas colgadas", () => {
    const p = new OplSongPlayer({ bank: banco(), smf: cancion(), loop: false });
    const l = new Float32Array(4096);
    const r = new Float32Array(4096);
    p.render(l, r, 4096);
    p.reset();
    expect(p.ended).toBe(false);
  });
});
