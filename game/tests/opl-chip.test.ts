/**
 * Emulador OPL2/OPL3: las propiedades que, si se rompen, hacen que la música suene
 * mal de una forma que nadie sabe atribuir.
 *
 * No se carean muestras contra una captura de hardware real (no la hay en el árbol);
 * se comprueban INVARIANTES FÍSICAS y estructurales, que es lo que sí se puede afirmar:
 * las dos ROM generadas contra sus anclas conocidas, la escala logarítmica (cada 256
 * unidades = mitad de amplitud), la frecuencia real de una nota medida por cruces por
 * cero, la forma de cada onda, y el recorrido de la envolvente. Capa pura: sin audio.
 */
import { describe, expect, it } from "vitest";
import { OplEmulator, OPL_CLOCK_HZ, __tables } from "../src/ui/opl/chip.js";

interface Voz {
  wave?: number;
  fnum: number;
  block: number;
  additive?: boolean;
  feedback?: number;
  modTl?: number;
  carTl?: number;
  ar?: number;
  dr?: number;
  sl?: number;
  rr?: number;
  egSustain?: boolean;
  mult?: number;
}

/** Monta UNA voz en el canal 0 y la levanta. Devuelve el chip listo para generar. */
function chipConVoz(v: Voz, kind: "opl2" | "opl3" = "opl3"): OplEmulator {
  const c = new OplEmulator(kind);
  if (kind === "opl3") c.writeReg(0x105, 0x01);
  c.writeReg(0x01, 0x20);
  const eg = v.egSustain ?? true ? 0x20 : 0x00;
  const mult = v.mult ?? 1;
  c.writeReg(0x20, eg | mult);
  c.writeReg(0x23, eg | mult);
  c.writeReg(0x40, v.modTl ?? 63);
  c.writeReg(0x43, v.carTl ?? 0);
  c.writeReg(0x60, ((v.ar ?? 15) << 4) | (v.dr ?? 0));
  c.writeReg(0x63, ((v.ar ?? 15) << 4) | (v.dr ?? 0));
  c.writeReg(0x80, ((v.sl ?? 0) << 4) | (v.rr ?? 15));
  c.writeReg(0x83, ((v.sl ?? 0) << 4) | (v.rr ?? 15));
  c.writeReg(0xe0, v.wave ?? 0);
  c.writeReg(0xe3, v.wave ?? 0);
  c.writeReg(0xc0, ((v.feedback ?? 0) << 1) | (v.additive ?? true ? 1 : 0) | 0x30);
  c.writeReg(0xa0, v.fnum & 0xff);
  c.writeReg(0xb0, 0x20 | ((v.block & 7) << 2) | ((v.fnum >> 8) & 3));
  return c;
}

function render(c: OplEmulator, samples: number): Float32Array {
  const l = new Float32Array(samples);
  const r = new Float32Array(samples);
  c.generate(l, r, samples);
  return l;
}

/** Frecuencia estimada contando cruces por cero ascendentes. */
function frecuencia(buf: Float32Array): number {
  let cruces = 0;
  for (let i = 1; i < buf.length; i++) {
    if (buf[i - 1]! <= 0 && buf[i]! > 0) cruces++;
  }
  return (cruces * OPL_CLOCK_HZ) / buf.length;
}

const pico = (b: Float32Array): number => b.reduce((m, x) => Math.max(m, Math.abs(x)), 0);

/**
 * Lóbulos por segundo: cruces ASCENDENTES DE LA MEDIA. Para señales que nunca bajan de
 * cero (las ondas 1..3 son no negativas) los cruces por cero no miden nada — hay que
 * cruzar por la media. El seno rectificado da dos lóbulos por ciclo y el medio seno uno,
 * que es justo la diferencia que se quiere comprobar.
 */
function lobulos(buf: Float32Array): number {
  let suma = 0;
  for (const x of buf) suma += x;
  const media = suma / buf.length;
  let cruces = 0;
  for (let i = 1; i < buf.length; i++) {
    if (buf[i - 1]! <= media && buf[i]! > media) cruces++;
  }
  return (cruces * OPL_CLOCK_HZ) / buf.length;
}

describe("las dos ROM del OPL, generadas por fórmula", () => {
  it("LOG_SIN tiene sus anclas conocidas", () => {
    expect(__tables.LOG_SIN[0]).toBe(2137);
    expect(__tables.LOG_SIN[255]).toBe(0);
  });

  it("LOG_SIN es monótona decreciente (es un cuarto de seno en log)", () => {
    for (let i = 1; i < 256; i++) {
      expect(__tables.LOG_SIN[i]!).toBeLessThanOrEqual(__tables.LOG_SIN[i - 1]!);
    }
  });

  it("EXP arranca en 0 y no se sale de su rango", () => {
    expect(__tables.EXP[0]).toBe(0);
    expect(__tables.EXP[255]).toBeLessThan(2048);
  });
});

describe("expo — la escala logarítmica del chip", () => {
  it("🔴 a atenuación 0 da la escala completa de 13 BITS (4085, no 2042)", () => {
    // Con ×1024 en vez de ×2048 todo sonaría afinado y correcto pero 6 dB bajo, y el
    // defecto se «arreglaría» subiendo ganancia en otro sitio. Ver la nota de EXP.
    expect(__tables.expo(0)).toBe(4085);
  });

  it("🔴 cada 256 unidades es exactamente MEDIA amplitud (−6 dB)", () => {
    const full = __tables.expo(0);
    expect(__tables.expo(256)).toBe(full >> 1);
    expect(__tables.expo(512)).toBe(full >> 2);
    expect(__tables.expo(768)).toBe(full >> 3);
  });

  it("es monótona y acaba en silencio", () => {
    let prev = Infinity;
    for (let a = 0; a < 0x1800; a += 37) {
      const v = __tables.expo(a);
      expect(v).toBeLessThanOrEqual(prev);
      prev = v;
    }
    expect(__tables.expo(0x1800)).toBe(0);
  });
});

describe("silencio", () => {
  it("un chip recién creado no suena", () => {
    const c = new OplEmulator("opl3");
    expect(c.isSilent).toBe(true);
    expect(pico(render(c, 512))).toBe(0);
  });

  it("tras el key-off la voz acaba apagándose del todo", () => {
    const c = chipConVoz({ fnum: 580, block: 4, rr: 15 });
    render(c, 2000);
    expect(c.isSilent).toBe(false);
    c.writeReg(0xb0, (4 << 2) | 2); // mismo bloque/fnum, sin key-on
    render(c, 20000);
    expect(c.isSilent).toBe(true);
  });
});

describe("🔴 afinación — una nota tiene que sonar a su frecuencia", () => {
  it("F-Number 580 / Block 4 suena a 440 Hz", () => {
    const c = chipConVoz({ fnum: 580, block: 4 });
    const f = frecuencia(render(c, OPL_CLOCK_HZ));
    expect(f).toBeGreaterThan(437);
    expect(f).toBeLessThan(443);
  });

  it("un bloque más es una octava arriba", () => {
    const f = frecuencia(render(chipConVoz({ fnum: 580, block: 5 }), OPL_CLOCK_HZ));
    expect(f).toBeGreaterThan(875);
    expect(f).toBeLessThan(885);
  });

  it("MULT ×2 dobla la frecuencia; MULT ×0,5 la parte por dos", () => {
    const dos = frecuencia(render(chipConVoz({ fnum: 580, block: 4, mult: 2 }), OPL_CLOCK_HZ));
    const medio = frecuencia(render(chipConVoz({ fnum: 580, block: 4, mult: 0 }), OPL_CLOCK_HZ));
    expect(dos).toBeGreaterThan(875);
    expect(dos).toBeLessThan(885);
    expect(medio).toBeGreaterThan(217);
    expect(medio).toBeLessThan(223);
  });
});

describe("formas de onda", () => {
  const buf = (wave: number): Float32Array =>
    render(chipConVoz({ fnum: 580, block: 4, wave, modTl: 63 }), 4096);

  it("la onda 0 (seno) tiene las dos mitades", () => {
    const b = buf(0);
    expect(Math.min(...b)).toBeLessThan(0);
    expect(Math.max(...b)).toBeGreaterThan(0);
  });

  it("🔴 la onda 1 (medio seno) no baja de cero", () => {
    expect(Math.min(...buf(1))).toBeGreaterThanOrEqual(0);
  });

  it("🔴 la onda 2 (seno rectificado) tampoco, y da el DOBLE de lóbulos", () => {
    const b = buf(2);
    expect(Math.min(...b)).toBeGreaterThanOrEqual(0);
    // Rectificar convierte cada ciclo en dos lóbulos; el medio seno deja sólo uno.
    expect(lobulos(b)).toBeGreaterThan(lobulos(buf(1)) * 1.7);
  });

  it("la onda 3 (cuarto de seno) está callada la mayor parte del ciclo", () => {
    const b = buf(3);
    const mudas = b.reduce((n, x) => n + (x === 0 ? 1 : 0), 0);
    expect(mudas / b.length).toBeGreaterThan(0.4);
  });
});

describe("envolvente", () => {
  it("ataca, sostiene mientras la tecla está pulsada y suelta al levantarla", () => {
    const c = chipConVoz({ fnum: 580, block: 4, ar: 15, dr: 0, sl: 0, rr: 7, egSustain: true });
    const ataque = pico(render(c, 256));
    const sostenido = pico(render(c, 8192));
    expect(sostenido).toBeGreaterThan(0.001);
    expect(ataque).toBeGreaterThan(0);
    c.writeReg(0xb0, (4 << 2) | 2); // key-off
    render(c, 4096);
    const tras = pico(render(c, 4096));
    expect(tras).toBeLessThan(sostenido);
  });

  it("🔴 sin EG-sustain la nota decae aunque la tecla siga pulsada", () => {
    const c = chipConVoz({
      fnum: 580, block: 4, ar: 15, dr: 8, sl: 2, rr: 4, egSustain: false,
    });
    render(c, 4096);
    const pronto = pico(render(c, 4096));
    render(c, 40000);
    const tarde = pico(render(c, 4096));
    expect(tarde).toBeLessThan(pronto);
  });

  it("un ataque lento tarda más en llegar que uno rápido", () => {
    const rapido = pico(render(chipConVoz({ fnum: 580, block: 4, ar: 15 }), 1024));
    const lento = pico(render(chipConVoz({ fnum: 580, block: 4, ar: 3 }), 1024));
    expect(lento).toBeLessThan(rapido);
  });
});

describe("modulación", () => {
  it("🔴 FM con modulador audible genera armónicos que el aditivo no tiene", () => {
    const fm = render(chipConVoz({ fnum: 580, block: 4, additive: false, modTl: 0 }), 8192);
    const puro = render(chipConVoz({ fnum: 580, block: 4, additive: true, modTl: 63 }), 8192);
    // Un seno puro cruza el cero dos veces por ciclo; con FM profunda la forma se
    // retuerce y aparecen cruces de más.
    expect(frecuencia(fm)).toBeGreaterThan(frecuencia(puro));
  });

  it("el feedback cambia el timbre sin cambiar la nota", () => {
    const sin = render(chipConVoz({ fnum: 580, block: 4, additive: false, modTl: 0, feedback: 0 }), 8192);
    const con = render(chipConVoz({ fnum: 580, block: 4, additive: false, modTl: 0, feedback: 7 }), 8192);
    let distintas = 0;
    for (let i = 0; i < sin.length; i++) if (Math.abs(sin[i]! - con[i]!) > 1e-6) distintas++;
    expect(distintas).toBeGreaterThan(sin.length / 10);
  });

  it("el volumen del operador (TL) atenúa de verdad", () => {
    const alto = pico(render(chipConVoz({ fnum: 580, block: 4, carTl: 0 }), 4096));
    const bajo = pico(render(chipConVoz({ fnum: 580, block: 4, carTl: 40 }), 4096));
    expect(bajo).toBeLessThan(alto / 2);
  });
});

describe("OPL2 frente a OPL3", () => {
  it("el OPL2 da 9 canales y el OPL3 dieciocho", () => {
    expect(new OplEmulator("opl2").channelCount).toBe(9);
    expect(new OplEmulator("opl3").channelCount).toBe(18);
  });

  it("🔴 el OPL2 ignora el segundo banco de registros", () => {
    const c = new OplEmulator("opl2");
    c.writeReg(0x101 + 0x20, 0x21); // escritura al banco alto: no debe existir
    c.writeReg(0x1b0, 0x3f);
    expect(c.isSilent).toBe(true);
  });

  it("en el OPL3 el pan duro deja un lado mudo", () => {
    const c = chipConVoz({ fnum: 580, block: 4 });
    c.writeReg(0xc0, 0x01 | 0x20); // sólo izquierda
    const l = new Float32Array(4096);
    const r = new Float32Array(4096);
    c.generate(l, r, 4096);
    expect(pico(l)).toBeGreaterThan(0);
    expect(pico(r)).toBe(0);
  });

  it("las 18 voces del OPL3 suenan a la vez sin romperse", () => {
    const c = new OplEmulator("opl3");
    c.writeReg(0x105, 0x01);
    for (let v = 0; v < 18; v++) {
      const base = v < 9 ? 0x000 : 0x100;
      const slot = v % 9;
      const off = [0x00, 0x01, 0x02, 0x08, 0x09, 0x0a, 0x10, 0x11, 0x12][slot]!;
      c.writeReg(base + 0x20 + off, 0x21);
      c.writeReg(base + 0x23 + off, 0x21);
      c.writeReg(base + 0x40 + off, 0x3f);
      c.writeReg(base + 0x43 + off, 0x00);
      c.writeReg(base + 0x60 + off, 0xf0);
      c.writeReg(base + 0x63 + off, 0xf0);
      c.writeReg(base + 0x80 + off, 0x0f);
      c.writeReg(base + 0x83 + off, 0x0f);
      c.writeReg(base + 0xc0 + slot, 0x31);
      c.writeReg(base + 0xa0 + slot, 580 & 0xff);
      c.writeReg(base + 0xb0 + slot, 0x20 | (4 << 2) | 2);
    }
    const b = render(c, 4096);
    expect(pico(b)).toBeGreaterThan(0);
    expect(b.every((x) => Number.isFinite(x) && x >= -1 && x <= 1)).toBe(true);
  });
});
