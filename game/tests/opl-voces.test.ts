/**
 * Asignador de voces OPL: eventos MIDI → escrituras de registro.
 *
 * Estos tests son DISCRIMINANTES sobre las cosas que este módulo puede equivocar en
 * silencio, que son casi todas las que importan: el note-off disfrazado de note-on con
 * velocidad 0 (la ÚNICA forma en que llegan en este corpus), el escalado de volumen en
 * los timbres aditivos, los bits de estéreo del OPL3 sin los cuales no sale nada, y el
 * robo de voz — que en Ultima V no es un borde teórico sino el caso normal, porque el
 * corpus llega a 17 notas simultáneas sobre un chip de 9 ó 18.
 *
 * Capa PURA: cero Web Audio, cero DOM, cero estado de juego. Corre en el árbol público.
 */
import { describe, expect, it } from "vitest";
import type { MilesOplBank, OplTimbre } from "../src/ui/opl/bank.js";
import {
  noteToFnumBlock,
  OplVoiceAllocator,
  scaleTl,
  type OplWrite,
} from "../src/ui/opl/voices.js";

function timbre(over: Partial<OplTimbre> = {}): OplTimbre {
  const op = {
    amVibEgKsrMult: 0x21,
    kslTl: 0x00,
    attackDecay: 0xf2,
    sustainRelease: 0x45,
    waveform: 0x00,
  };
  return {
    bank: 0,
    patch: 0,
    fixedNote: 0,
    modulator: { ...op },
    carrier: { ...op, kslTl: 0x06 },
    feedbackConnection: 0x08, // FB 4, conexión FM (bit 0 = 0)
    ...over,
  };
}

/** Banco de mentira: sólo lo que el asignador consulta. */
function bancoDe(opts: {
  melodic?: OplTimbre;
  percussion?: Map<number, OplTimbre>;
}): MilesOplBank {
  const mel = opts.melodic ?? timbre();
  const per = opts.percussion ?? new Map<number, OplTimbre>();
  return {
    timbres: [mel, ...per.values()],
    get: (bank, patch) => (bank === 0 && patch === mel.patch ? mel : per.get(patch)),
    melodic: () => mel,
    percussion: (note) => per.get(note),
  };
}

const valorDe = (w: readonly OplWrite[], reg: number): number | undefined =>
  w.filter((x) => x.reg === reg).pop()?.value;

describe("noteToFnumBlock — F-Number y Block del OPL", () => {
  it("La 440 (nota 69) ⇒ Block 4, F-Number 580 (comprobación de mesa)", () => {
    expect(noteToFnumBlock(69)).toEqual({ fnum: 580, block: 4 });
  });

  it("una octava arriba es el MISMO F-Number con un Block más", () => {
    expect(noteToFnumBlock(81)).toEqual({ fnum: 580, block: 5 });
    expect(noteToFnumBlock(57)).toEqual({ fnum: 580, block: 3 });
  });

  it("el bend desplaza la afinación y no se sale de 10 bits", () => {
    const sin = noteToFnumBlock(69);
    const con = noteToFnumBlock(69, 1);
    expect(con.fnum).toBeGreaterThan(sin.fnum);
    for (let n = 0; n <= 127; n++) {
      const r = noteToFnumBlock(n, 2);
      expect(r.fnum).toBeLessThanOrEqual(1023);
      expect(r.block).toBeLessThanOrEqual(7);
    }
  });
});

describe("scaleTl — el volumen es ATENUACIÓN", () => {
  it("a volumen 1 respeta EXACTAMENTE el TL que diseñó el banco", () => {
    expect(scaleTl(0x8f, 1)).toBe(0x8f);
  });

  it("a volumen 0 llega a 63 (mudo) conservando los bits de KSL", () => {
    expect(scaleTl(0x8f, 0)).toBe(0x80 | 0x3f);
  });

  it("es monótona entre medias", () => {
    const v = [0, 0.25, 0.5, 0.75, 1].map((x) => scaleTl(0x00, x) & 0x3f);
    for (let i = 1; i < v.length; i++) expect(v[i]!).toBeLessThanOrEqual(v[i - 1]!);
  });
});

describe("reset — deja el chip en un estado conocido", () => {
  it("🔴 en OPL3 habilita el chip ANTES de nada (sin 0x105 no hay segundo banco)", () => {
    const w = new OplVoiceAllocator({ bank: bancoDe({}), chip: "opl3" }).reset();
    expect(w[0]).toEqual({ reg: 0x105, value: 0x01 });
    expect(valorDe(w, 0x104)).toBe(0x00); // todas de 2 operadores
  });

  it("en OPL2 no escribe NADA en el segundo banco de registros", () => {
    const w = new OplVoiceAllocator({ bank: bancoDe({}), chip: "opl2" }).reset();
    expect(w.every((x) => x.reg < 0x100)).toBe(true);
  });

  it("habilita el selector de onda (sin 0x01 bit 5 el OPL2 ignora los 0xE0)", () => {
    const w = new OplVoiceAllocator({ bank: bancoDe({}), chip: "opl2" }).reset();
    expect(valorDe(w, 0x01)).toBe(0x20);
  });

  it("el OPL3 da 18 voces y el OPL2 nueve", () => {
    expect(new OplVoiceAllocator({ bank: bancoDe({}) }).voiceCount).toBe(18);
    expect(
      new OplVoiceAllocator({ bank: bancoDe({}), chip: "opl2" }).voiceCount,
    ).toBe(9);
  });
});

describe("noteOn / noteOff", () => {
  it("levanta la nota con su F-Number, su Block y el bit de key-on", () => {
    const a = new OplVoiceAllocator({ bank: bancoDe({}), chip: "opl2" });
    a.reset();
    const w = a.noteOn(0, 69, 127);
    expect(valorDe(w, 0xa0)).toBe(580 & 0xff);
    expect(valorDe(w, 0xb0)).toBe(((580 >> 8) & 0x03) | (4 << 2) | 0x20);
    expect(a.activeVoices).toBe(1);
  });

  it("🔴 NOTE-ON CON VELOCIDAD 0 ES UN NOTE-OFF (así llegan TODOS en este corpus)", () => {
    const a = new OplVoiceAllocator({ bank: bancoDe({}), chip: "opl2" });
    a.reset();
    a.noteOn(0, 60, 100);
    expect(a.activeVoices).toBe(1);
    const w = a.noteOn(0, 60, 0);
    expect(a.activeVoices).toBe(0);
    expect(valorDe(w, 0xb0)! & 0x20).toBe(0); // key-on apagado
  });

  it("el note-off apaga el key-on y conserva Block/F-Number", () => {
    const a = new OplVoiceAllocator({ bank: bancoDe({}), chip: "opl2" });
    a.reset();
    a.noteOn(0, 69, 127);
    const w = a.noteOff(0, 69);
    expect(valorDe(w, 0xb0)).toBe(((580 >> 8) & 0x03) | (4 << 2));
  });

  it("un note-off de una nota que no suena no escribe nada", () => {
    const a = new OplVoiceAllocator({ bank: bancoDe({}), chip: "opl2" });
    a.reset();
    expect(a.noteOff(0, 60)).toEqual([]);
  });
});

describe("volumen y conexión", () => {
  it("en FM (CNT=0) el volumen escala SÓLO la portadora", () => {
    const t = timbre({ feedbackConnection: 0x08 });
    const a = new OplVoiceAllocator({ bank: bancoDe({ melodic: t }), chip: "opl2" });
    a.reset();
    const w = a.noteOn(0, 60, 64);
    expect(valorDe(w, 0x40)).toBe(t.modulator.kslTl); // modulador intacto
    expect(valorDe(w, 0x43)).not.toBe(t.carrier.kslTl); // portadora atenuada
  });

  it("🔴 en aditivo (CNT=1) escala LOS DOS: son las dos portadoras", () => {
    const t = timbre({ feedbackConnection: 0x09 });
    const a = new OplVoiceAllocator({ bank: bancoDe({ melodic: t }), chip: "opl2" });
    a.reset();
    const vol = (64 / 127) * (100 / 127);
    const w = a.noteOn(0, 60, 64);
    expect(valorDe(w, 0x40)).toBe(scaleTl(t.modulator.kslTl, vol));
    expect(valorDe(w, 0x43)).toBe(scaleTl(t.carrier.kslTl, vol));
  });

  it("CC7 y CC11 reescriben el volumen de las voces YA sonando", () => {
    const a = new OplVoiceAllocator({ bank: bancoDe({}), chip: "opl2" });
    a.reset();
    a.noteOn(0, 60, 127);
    const bajo = a.controlChange(0, 7, 10);
    const alto = a.controlChange(0, 7, 127);
    expect((valorDe(bajo, 0x43)! & 0x3f) > (valorDe(alto, 0x43)! & 0x3f)).toBe(true);
  });
});

describe("estéreo del OPL3", () => {
  it("🔴 enciende los bits 4/5 que el banco no trae (sin ellos NO SALE SONIDO)", () => {
    const a = new OplVoiceAllocator({ bank: bancoDe({}), chip: "opl3" });
    a.reset();
    const w = a.noteOn(0, 60, 127);
    expect(valorDe(w, 0xc0)! & 0x30).toBe(0x30);
  });

  it("en OPL2 NO los enciende (ahí esos bits no existen)", () => {
    const a = new OplVoiceAllocator({ bank: bancoDe({}), chip: "opl2" });
    a.reset();
    const w = a.noteOn(0, 60, 127);
    expect(valorDe(w, 0xc0)! & 0x30).toBe(0);
  });

  it("el pan duro (CC10) manda la voz a un solo lado", () => {
    const a = new OplVoiceAllocator({ bank: bancoDe({}), chip: "opl3" });
    a.reset();
    a.noteOn(0, 60, 127);
    expect(valorDe(a.controlChange(0, 10, 0), 0xc0)! & 0x30).toBe(0x20);
    expect(valorDe(a.controlChange(0, 10, 127), 0xc0)! & 0x30).toBe(0x10);
    expect(valorDe(a.controlChange(0, 10, 64), 0xc0)! & 0x30).toBe(0x30);
  });
});

describe("asignación y robo de voz", () => {
  it("reparte voces distintas y usa el SEGUNDO banco a partir de la décima", () => {
    const a = new OplVoiceAllocator({ bank: bancoDe({}), chip: "opl3" });
    a.reset();
    for (let i = 0; i < 9; i++) a.noteOn(0, 60 + i, 100);
    const w = a.noteOn(0, 69, 100);
    expect(w.some((x) => x.reg >= 0x100)).toBe(true);
    expect(a.activeVoices).toBe(10);
  });

  it("el OPL3 sostiene las 17 simultáneas que pide el corpus", () => {
    const a = new OplVoiceAllocator({ bank: bancoDe({}), chip: "opl3" });
    a.reset();
    for (let i = 0; i < 17; i++) a.noteOn(0, 40 + i, 100);
    expect(a.activeVoices).toBe(17);
  });

  it("🔴 el OPL2 se queda en 9 y ROBA la más antigua, apagándola antes", () => {
    const a = new OplVoiceAllocator({ bank: bancoDe({}), chip: "opl2" });
    a.reset();
    for (let i = 0; i < 9; i++) a.noteOn(0, 60 + i, 100);
    expect(a.activeVoices).toBe(9);
    const w = a.noteOn(0, 72, 100);
    expect(a.activeVoices).toBe(9);
    // La víctima es la voz 0 (la más antigua) y lo primero es su key-off.
    expect(w[0]).toEqual({ reg: 0xb0, value: 0x00 });
  });

  it("repetir la MISMA nota reusa su voz en vez de gastar otra", () => {
    const a = new OplVoiceAllocator({ bank: bancoDe({}), chip: "opl2" });
    a.reset();
    a.noteOn(0, 60, 100);
    a.noteOn(0, 60, 100);
    expect(a.activeVoices).toBe(1);
  });

  it("prefiere una voz libre antes que robar una que suena", () => {
    const a = new OplVoiceAllocator({ bank: bancoDe({}), chip: "opl2" });
    a.reset();
    for (let i = 0; i < 9; i++) a.noteOn(0, 60 + i, 100);
    a.noteOff(0, 64); // deja una libre
    const w = a.noteOn(0, 90, 100);
    expect(a.activeVoices).toBe(9);
    expect(w[0]!.reg).not.toBe(0xb0); // no hubo key-off previo: no robó
  });
});

describe("pedal de resonancia (CC64) — está en el corpus medido", () => {
  it("con el pedal pisado el note-off NO suelta la voz", () => {
    const a = new OplVoiceAllocator({ bank: bancoDe({}), chip: "opl2" });
    a.reset();
    a.controlChange(0, 64, 127);
    a.noteOn(0, 60, 100);
    expect(a.noteOff(0, 60)).toEqual([]);
    expect(a.activeVoices).toBe(1);
  });

  it("al soltar el pedal caen todas las retenidas de ese canal", () => {
    const a = new OplVoiceAllocator({ bank: bancoDe({}), chip: "opl2" });
    a.reset();
    a.controlChange(0, 64, 127);
    a.noteOn(0, 60, 100);
    a.noteOn(0, 64, 100);
    a.noteOff(0, 60);
    a.noteOff(0, 64);
    expect(a.activeVoices).toBe(2);
    const w = a.controlChange(0, 64, 0);
    expect(a.activeVoices).toBe(0);
    expect(w.length).toBeGreaterThan(0);
  });

  it("una voz retenida se roba antes que una que sigue sonando", () => {
    const a = new OplVoiceAllocator({ bank: bancoDe({}), chip: "opl2" });
    a.reset();
    a.controlChange(0, 64, 127);
    for (let i = 0; i < 9; i++) a.noteOn(0, 60 + i, 100);
    a.noteOff(0, 68); // la voz 8 queda RETENIDA, no libre
    a.noteOn(0, 90, 100);
    expect(a.activeVoices).toBe(9);
  });
});

describe("pitch bend y program change", () => {
  it("el bend reafina las voces vivas sin soltarlas", () => {
    const a = new OplVoiceAllocator({ bank: bancoDe({}), chip: "opl2" });
    a.reset();
    a.noteOn(0, 69, 100);
    const w = a.pitchBend(0, 8192 + 4096); // +1 semitono
    expect(valorDe(w, 0xa0)).not.toBe(580 & 0xff);
    expect(valorDe(w, 0xb0)! & 0x20).toBe(0x20); // sigue sonando
  });

  it("el bend centrado vuelve a la afinación original", () => {
    const a = new OplVoiceAllocator({ bank: bancoDe({}), chip: "opl2" });
    a.reset();
    a.noteOn(0, 69, 100);
    a.pitchBend(0, 16383);
    const w = a.pitchBend(0, 8192);
    expect(valorDe(w, 0xa0)).toBe(580 & 0xff);
  });

  it("el program change NO retoca las notas que ya suenan (lo dice MIDI)", () => {
    const a = new OplVoiceAllocator({ bank: bancoDe({}), chip: "opl2" });
    a.reset();
    a.noteOn(0, 60, 100);
    expect(a.programChange(0, 48)).toEqual([]);
    expect(a.activeVoices).toBe(1);
  });
});

describe("percusión — el corpus no la usa, pero el banco la trae", () => {
  it("🔴 suena a la nota FIJA del timbre, no a la del evento", () => {
    const per = new Map([[38, timbre({ bank: 127, patch: 38, fixedNote: 60 })]]);
    const a = new OplVoiceAllocator({ bank: bancoDe({ percussion: per }), chip: "opl2" });
    a.reset();
    const w = a.noteOn(9, 38, 100);
    const esperado = noteToFnumBlock(60);
    expect(valorDe(w, 0xa0)).toBe(esperado.fnum & 0xff);
  });

  it("una nota de batería que el banco no trae se ignora en silencio", () => {
    const a = new OplVoiceAllocator({ bank: bancoDe({}), chip: "opl2" });
    a.reset();
    expect(a.noteOn(9, 99, 100)).toEqual([]);
    expect(a.activeVoices).toBe(0);
  });
});

describe("🔴 sink: el camino de audio no asigna", () => {
  /**
   * Con `sink`, el asignador escribe registro a registro y NO construye arrays ni objetos.
   * Medido el 2026-09-11: sin él, 234 bytes por `noteOn` y ~13 KB/s de basura en el hilo
   * de audio — la última asignación que quedaba ahí. Lo que molesta no es la memoria sino
   * la PAUSA del recolector: para el hilo de audio y se oye como un fallo.
   *
   * El API de arrays se conserva para los tests (los 36 de arriba lo usan); lo que cambia
   * es por dónde sale el trabajo cuando hay sink.
   */
  it("con sink, las escrituras van al sink y los métodos no devuelven array propio", () => {
    const escrituras: number[] = [];
    const a = new OplVoiceAllocator({
      bank: bancoDe({}),
      chip: "opl2",
      sink: (reg, value) => escrituras.push(reg, value),
    });
    a.reset();
    expect(escrituras.length).toBeGreaterThan(0);
    const antes = escrituras.length;
    const devuelto = a.noteOn(0, 69, 127);
    expect(escrituras.length).toBeGreaterThan(antes); // escribió por el sink
    expect(devuelto).toHaveLength(0); // y no construyó array
  });

  it("con sink produce EXACTAMENTE las mismas escrituras que sin él", () => {
    // Si las dos vías divergieran, los tests (que usan arrays) dejarían de describir lo
    // que de verdad suena.
    const conArray = new OplVoiceAllocator({ bank: bancoDe({}), chip: "opl2" });
    const plano: number[] = [];
    const conSink = new OplVoiceAllocator({
      bank: bancoDe({}),
      chip: "opl2",
      sink: (reg, value) => plano.push(reg, value),
    });
    const esperado: number[] = [];
    for (const acto of [
      (x: OplVoiceAllocator) => x.reset(),
      (x: OplVoiceAllocator) => x.noteOn(0, 69, 100),
      (x: OplVoiceAllocator) => x.controlChange(0, 7, 40),
      (x: OplVoiceAllocator) => x.pitchBend(0, 10000),
      (x: OplVoiceAllocator) => x.noteOff(0, 69),
      (x: OplVoiceAllocator) => x.controlChange(0, 121, 0),
    ]) {
      for (const w of acto(conArray)) esperado.push(w.reg, w.value);
      acto(conSink);
    }
    expect(plano).toEqual(esperado);
  });
});

describe("controladores que el corpus trae y el OPL no puede representar", () => {
  it("1, 32, 91, 93 y 119 se aceptan y no escriben nada", () => {
    const a = new OplVoiceAllocator({ bank: bancoDe({}), chip: "opl2" });
    a.reset();
    a.noteOn(0, 60, 100);
    for (const cc of [1, 32, 91, 93, 119]) {
      expect(a.controlChange(0, cc, 64)).toEqual([]);
    }
    expect(a.activeVoices).toBe(1);
  });

  it("CC121 devuelve los defaults sin matar las notas vivas", () => {
    const a = new OplVoiceAllocator({ bank: bancoDe({}), chip: "opl2" });
    a.reset();
    a.noteOn(0, 60, 100);
    a.controlChange(0, 7, 0);
    a.controlChange(0, 121, 0);
    expect(a.activeVoices).toBe(1);
  });

  it("CC123 sí las suelta todas", () => {
    const a = new OplVoiceAllocator({ bank: bancoDe({}), chip: "opl2" });
    a.reset();
    a.noteOn(0, 60, 100);
    a.noteOn(0, 64, 100);
    a.controlChange(0, 123, 0);
    expect(a.activeVoices).toBe(0);
  });
});
