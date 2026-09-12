/**
 * EMULADOR OPL2 / OPL3 (Yamaha YM3812 / YMF262) — el sintetizador de verdad.
 *
 * Entra un flujo de escrituras de registro (las que produce `voices.ts`), salen
 * muestras. No sabe de MIDI, ni de XMI, ni de Web Audio: registros y PCM.
 *
 * ⚠️ QUÉ ES Y QUÉ NO ES ESTO. El resto del repositorio es un port BYTE-EXACTO de
 * `ULTIMA.EXE` derivado de su desensamblado. Esto NO: es una emulación de hardware de
 * TERCEROS (Yamaha), para una función que el juego de 1988 no tenía, escrita desde el
 * comportamiento documentado del chip — no desde un volcado del silicio ni careada
 * contra una captura de una tarjeta real. Es fiel al OPL en estructura y en escala, y
 * aproximada en el temple fino de un par de constantes que abajo se señalan UNA A UNA.
 * Ese listón es el correcto para música QoL, y decirlo es parte del trabajo.
 *
 * ═══ POR QUÉ HACE FALTA EL CHIP ENTERO ══════════════════════════════════════════════
 *
 * Tentación razonable: implementar sólo lo que el banco usa y ahorrarse el resto. Se
 * midió (2026-09-11) sobre los 362 operadores de `FAT.OPL`, y el banco NO deja nada
 * fuera: 65 operadores con trémolo, 60 con vibrato, 175 con EG-sustain, 101 con KSR,
 * los cuatro ajustes de KSL, los ocho niveles de feedback, MULT de ×0,5 a ×14 y 8
 * timbres aditivos. No hay atajo disponible: o está el chip, o suenan mal instrumentos
 * concretos y de forma difícil de atribuir.
 *
 * ═══ LAS TABLAS SE GENERAN, NO SE COPIAN ════════════════════════════════════════════
 *
 * `LOG_SIN` y `EXP` son las dos ROM del OPL, y salen de su FÓRMULA cerrada — no hay
 * que copiar datos de nadie. Anclas comprobables: `LOG_SIN[0] = 2137`, `LOG_SIN[255] =
 * 0`, `EXP[0] = 0`. Todo el chip trabaja en el dominio LOGARÍTMICO, en unidades de
 * 1/256 de log2 (≈ 0,0235 dB); `expo()` es la única puerta de vuelta al lineal.
 *
 * Con esas unidades cuadran las tres escalas del chip, y cuadrar es la comprobación:
 *   · TL  (6 bits, pasos de 0,75 dB)  → `tl << 5`   ⇒ 63 = 47,25 dB, el máximo real.
 *   · EG  (9 bits, pasos de 0,1875 dB)→ `eg << 3`   ⇒ 511 = 96 dB, silencio.
 *   · KSL (tabla en pasos de 0,375 dB)→ `ksl * 16`.
 *
 * ═══ LAS DOS CONSTANTES TEMPLADAS (y no derivadas) ══════════════════════════════════
 *
 * Honestidad sobre lo que no está medido, que es poco pero existe:
 *   1. `MOD_SCALE` — cuánto desplaza la fase la salida del modulador. Fija la PROFUNDIDAD
 *      FM, es decir el brillo. Aquí se usa la proporción de DBOPL (salida de operador a
 *      escala completa ≈ ±4 ciclos de fase).
 *   2. `OUTPUT_SCALE` — el divisor a coma flotante. Elegido para que el chip sature como
 *      el hardware (16 bits con recorte duro) y no antes.
 * Ninguna de las dos altera QUÉ notas suenan ni CUÁNDO: sólo brillo y nivel.
 */

/** Reloj nativo del OPL, en Hz. Aquí se genera SIEMPRE a esta tasa. */
export const OPL_CLOCK_HZ = 49716;

/** Variante de chip. El OPL3 añade 9 voces, estéreo y cuatro ondas más. */
export type OplChipKind = "opl2" | "opl3";

// ═══ ROM 1: cuarto de seno en dominio logarítmico ════════════════════════════════════
const LOG_SIN = new Uint16Array(256);
for (let i = 0; i < 256; i++) {
  LOG_SIN[i] = Math.round(-Math.log2(Math.sin(((i + 0.5) * Math.PI) / 512)) * 256);
}

// ═══ ROM 2: exponencial (vuelta al dominio lineal) ═══════════════════════════════════
//
// 🔴 LA ESCALA ES DE 13 BITS, NO DE 12, y la diferencia no es cosmética. La salida de un
// operador del OPL a escala completa es ±4085; con el ×1024 «redondo» sale ±2042, que es
// EXACTAMENTE la mitad. El modo de fallo es de los que no señalan a su causa: todo suena
// bien —afinado, con su timbre y su envolvente— pero 6 dB por debajo, y quien lo oiga
// pensará que sobra ganancia en el reproductor y la subirá ahí, tapando el defecto en vez
// de arreglarlo. Medido: con ×1024 las pistas del corpus picaban a 0,06..0,15.
const EXP = new Uint16Array(256);
for (let i = 0; i < 256; i++) {
  // `Math.floor(0.5 + x)` y no `Math.round`: reproduce el `(int)(0.5 + x)` de C,
  // que TRUNCA. Difieren justo en los .5 exactos — con `round`, `EXP[0]` sale 1 y no 0.
  EXP[i] = Math.floor(0.5 + (Math.pow(2, i / 256) - 1) * 2048);
}

/**
 * Atenuación logarítmica → amplitud lineal. `att` en unidades de 1/256 de log2.
 * `expo(0) = 4085` (escala completa de 13 bits) y cada 256 unidades divide por dos —
 * que es la comprobación de que la escala es la correcta.
 */
function expo(att: number): number {
  const a = att < 0 ? 0 : att > 0x1fff ? 0x1fff : att;
  const shift = a >> 8;
  if (shift >= 20) return 0;
  return (EXP[255 - (a & 0xff)]! + 2048) >> shift;
}

/** Atenuación que representa «mudo» sin ramificar: al pasar por `expo` da 0. */
const SILENT = 0x1000;

/** MULT del registro 0x20, en MEDIAS unidades (índice 0 = ×0,5). */
const MULT2 = [1, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 20, 24, 24, 30, 30] as const;

/** ROM de KSL, en pasos de 0,375 dB, indexada por los 4 bits altos del F-Number. */
const KSL_ROM = [0, 32, 40, 45, 48, 51, 53, 55, 56, 58, 59, 60, 61, 62, 63, 64] as const;
/** Desplazamiento por ajuste de KSL. El 31 del ajuste 0 anula la atenuación entera. */
const KSL_SHIFT = [31, 1, 2, 0] as const;

/** Patrón de incremento del generador de envolvente (4 fases × 8 pasos). */
const EG_INC = [
  [0, 1, 0, 1, 0, 1, 0, 1],
  [0, 1, 0, 1, 1, 1, 0, 1],
  [0, 1, 1, 1, 0, 1, 1, 1],
  [0, 1, 1, 1, 1, 1, 1, 1],
] as const;

/** Trémolo: triángulo 0→26→0 (26 pasos de 0,1875 dB ≈ 4,875 dB) a 3,7 Hz. */
const TREMOLO_STEPS = 52;
const TREMOLO_PERIOD = Math.round(OPL_CLOCK_HZ / 3.7 / TREMOLO_STEPS);
/** Vibrato: triángulo de 8 posiciones a 6,1 Hz. */
const VIBRATO_STEPS = [0, 1, 2, 1, 0, -1, -2, -1] as const;
const VIBRATO_PERIOD = Math.round(OPL_CLOCK_HZ / 6.1 / 8);

/** Ver la nota «constantes templadas» de la cabecera. */
const MOD_SCALE = 1;
/**
 * Divisor de la mezcla. NO es redondo por gusto: se midió el pico de las 17 pistas XMI
 * del parche (2026-09-11) sumando a 1/32768, y la más alta —BLCKTHRN— picaba a 0,4208,
 * con CERO recortes en todo el corpus. 1/16384 la deja en 0,84: nivel de escucha sano,
 * margen de sobra para la pista más fuerte y una potencia de dos exacta. Subirlo más
 * recortaría Blackthorn y Stones, que son las dos densas.
 */
const OUTPUT_SCALE = 1 / 16384;

const ENV_OFF = 0;
const ENV_ATTACK = 1;
const ENV_DECAY = 2;
const ENV_SUSTAIN = 3;
const ENV_RELEASE = 4;

/** Desplazamiento de los operadores de cada una de las 9 voces de un banco. */
const OP_OFFSET = [0x00, 0x01, 0x02, 0x08, 0x09, 0x0a, 0x10, 0x11, 0x12] as const;

/** offset de registro (0x00..0x15) → [canal, ¿portadora?], o undefined si no existe. */
const REG_TO_OP: ([number, boolean] | undefined)[] = new Array(0x16).fill(undefined);
for (let ch = 0; ch < 9; ch++) {
  REG_TO_OP[OP_OFFSET[ch]!] = [ch, false];
  REG_TO_OP[OP_OFFSET[ch]! + 3] = [ch, true];
}

class Operator {
  // — registros crudos —
  am = false;
  vib = false;
  egSustain = false;
  ksr = false;
  mult = 0;
  ksl = 0;
  tl = 0;
  attackRate = 0;
  decayRate = 0;
  sustainLevel = 0;
  releaseRate = 0;
  waveform = 0;

  // — estado vivo —
  phase = 0;
  egState = ENV_OFF;
  egLevel = 511;
  kslAtten = 0;
  /** Salidas anteriores, para el feedback (sólo las usa el modulador). */
  prev1 = 0;
  prev2 = 0;

  /** Recalcula la atenuación por KSL, que depende de la nota del canal. */
  updateKsl(fnum: number, block: number): void {
    if (this.ksl === 0) {
      this.kslAtten = 0;
      return;
    }
    const base = KSL_ROM[(fnum >> 6) & 0x0f]! - ((7 - block) << 3);
    this.kslAtten = base <= 0 ? 0 : (base >> KSL_SHIFT[this.ksl]!) * 16;
  }

  keyOn(): void {
    this.egState = ENV_ATTACK;
    this.phase = 0;
    this.prev1 = 0;
    this.prev2 = 0;
  }

  keyOff(): void {
    if (this.egState !== ENV_OFF) this.egState = ENV_RELEASE;
  }

  /** Ritmo efectivo de una fase de la envolvente, con el escalado por nota (KSR). */
  private rateFor(reg: number, keyCode: number): number {
    if (reg === 0) return 0;
    const add = this.ksr ? keyCode : keyCode >> 2;
    const r = reg * 4 + add;
    return r > 63 ? 63 : r;
  }

  /** Un paso de la envolvente. `egCounter` es el reloj global de muestras. */
  advanceEnvelope(egCounter: number, keyCode: number): void {
    if (this.egState === ENV_OFF) return;
    const reg =
      this.egState === ENV_ATTACK
        ? this.attackRate
        : this.egState === ENV_DECAY
          ? this.decayRate
          : this.egState === ENV_SUSTAIN
            ? this.egSustain
              ? 0 // sostiene: la envolvente se queda quieta hasta el key-off
              : this.releaseRate
            : this.releaseRate;
    const rate = this.rateFor(reg, keyCode);
    if (rate === 0) return;

    const hi = rate >> 2;
    const shift = hi < 13 ? 13 - hi : 0;
    const scale = hi < 13 ? 1 : 1 << (hi - 13);
    if ((egCounter & ((1 << shift) - 1)) !== 0) return;
    const inc = EG_INC[rate & 3]![(egCounter >> shift) & 7]! * scale;
    if (inc === 0) return;

    if (this.egState === ENV_ATTACK) {
      // El ataque se acerca a 0 de forma EXPONENCIAL: el paso es proporcional a lo que
      // queda. `~egLevel` es negativo, así que esto resta.
      this.egLevel += (~this.egLevel * inc) >> 3;
      if (this.egLevel <= 0) {
        this.egLevel = 0;
        this.egState = ENV_DECAY;
      }
    } else {
      this.egLevel += inc;
      if (this.egLevel >= 511) {
        this.egLevel = 511;
        if (this.egState === ENV_RELEASE) this.egState = ENV_OFF;
      } else if (this.egState === ENV_DECAY && this.egLevel >= this.sustainLevel) {
        this.egLevel = this.sustainLevel;
        this.egState = ENV_SUSTAIN;
      }
    }
  }

  /**
   * Una muestra del operador. `modulation` ya viene en unidades de índice de fase.
   * Devuelve amplitud con signo, aproximadamente ±2045.
   */
  sample(phaseInc: number, modulation: number, tremolo: number): number {
    this.phase = (this.phase + phaseInc) >>> 0;
    if (this.egState === ENV_OFF) return 0;

    const idx = ((this.phase >>> 10) + modulation) & 0x3ff;
    const w = waveAtten(this.waveform, idx);
    const att =
      (w & WAVE_ATT_MASK) +
      (this.egLevel << 3) +
      (this.tl << 5) +
      this.kslAtten +
      (this.am ? tremolo : 0);
    const mag = expo(att);
    return w & WAVE_NEG ? -mag : mag;
  }
}

/**
 * Forma de onda: índice de fase de 10 bits → atenuación logarítmica Y SIGNO, EMPAQUETADOS
 * EN UN ENTERO (`att` en los bits bajos, `WAVE_NEG` como bit de signo).
 *
 * ═══ 🔴 POR QUÉ UN ENTERO Y NO UN `{ att, neg }` ════════════════════════════════════
 *
 * Porque devolver un objeto ASIGNA MEMORIA, y esto corre DOS VECES POR CANAL Y POR
 * MUESTRA: 2 operadores × 18 canales × 49716 Hz = **1.789.776 objetos por segundo**, en
 * el hilo de audio. El recolector de basura no falla por eso —son objetos efímeros— pero
 * SÍ para el hilo de vez en cuando, y una pausa en el hilo de audio es exactamente un
 * CHASQUIDO. Es la regla de oro de `AudioWorkletProcessor`: en `process()` no se asigna.
 *
 * El modo de fallo es de los peores para diagnosticar, y costó dos rondas: en un render
 * OFFLINE no se nota NADA —no hay plazo que incumplir y la señal sale limpia, medida: 9
 * transitorios en 40 s, ninguno en un evento MIDI— y en tiempo real se oye un pop cada
 * pocos segundos. La señal nunca estuvo mal; lo que fallaba era el ritmo al producirla.
 */
const WAVE_NEG = 0x10000;
const WAVE_ATT_MASK = 0xffff;

function waveAtten(wave: number, phase: number): number {
  const quarter = phase & 0xff;
  const mirrored = phase & 0x100 ? 255 - quarter : quarter;
  const half = phase & 0x200;
  switch (wave) {
    case 0: // seno completo
      return LOG_SIN[mirrored]! | (half ? WAVE_NEG : 0);
    case 1: // medio seno: la mitad negativa se calla
      return half ? SILENT : LOG_SIN[mirrored]!;
    case 2: // seno rectificado: nunca negativo
      return LOG_SIN[mirrored]!;
    case 3: // cuarto de seno: sólo el primer cuarto de cada mitad
      return phase & 0x100 ? SILENT : LOG_SIN[quarter]!;
    // ─── Sólo OPL3. `FAT.OPL` no usa ninguna (medido: todas sus ondas son 0..3);
    //     están por corrección del chip, no porque el corpus las ejercite.
    case 4: {
      if (half) return SILENT;
      const d = (phase << 1) & 0x3ff;
      const q = d & 0xff;
      return LOG_SIN[d & 0x100 ? 255 - q : q]! | (d & 0x200 ? WAVE_NEG : 0);
    }
    case 5: {
      if (half) return SILENT;
      const d = (phase << 1) & 0x3ff;
      const q = d & 0xff;
      return LOG_SIN[d & 0x100 ? 255 - q : q]!;
    }
    case 6: // cuadrada
      return half ? WAVE_NEG : 0;
    default: {
      // diente de sierra logarítmico
      const v = phase & 0x1ff;
      return ((half ? 0x1ff - v : v) << 3) | (half ? WAVE_NEG : 0);
    }
  }
}

class Channel {
  readonly mod = new Operator();
  readonly car = new Operator();
  fnum = 0;
  block = 0;
  keyed = false;
  feedback = 0;
  additive = false;
  left = true;
  right = true;

  get keyCode(): number {
    // NTS = 0 (registro 0x08 a cero, que es como lo deja `voices.ts`).
    return (this.block << 1) | ((this.fnum >> 9) & 1);
  }

  updateKsl(): void {
    this.mod.updateKsl(this.fnum, this.block);
    this.car.updateKsl(this.fnum, this.block);
  }
}

/**
 * El chip. Se le vuelcan registros con `writeReg` y se le piden muestras con
 * `generate`, a la tasa nativa de 49716 Hz.
 */
export class OplEmulator {
  readonly kind: OplChipKind;
  readonly channelCount: number;
  private readonly channels: Channel[] = [];
  private egCounter = 0;
  private tremoloCounter = 0;
  private tremoloPos = 0;
  private vibratoCounter = 0;
  private vibratoPos = 0;
  private tremoloDepth = false;
  private vibratoDepth = false;
  private opl3Enabled = false;

  constructor(kind: OplChipKind = "opl3") {
    this.kind = kind;
    this.channelCount = kind === "opl3" ? 18 : 9;
    for (let i = 0; i < this.channelCount; i++) this.channels.push(new Channel());
  }

  /** Vuelca un registro. `reg` en 0x000..0x1FF (≥0x100 = segundo banco del OPL3). */
  writeReg(reg: number, value: number): void {
    const array = reg >= 0x100 ? 1 : 0;
    const r = reg & 0xff;
    const v = value & 0xff;
    if (array === 1 && this.kind !== "opl3") return;

    if (reg === 0x105) {
      this.opl3Enabled = (v & 1) !== 0;
      return;
    }
    if (reg === 0x104 || r === 0x01 || r === 0x08 || r === 0x02 || r === 0x03) return;
    if (r === 0xbd) {
      this.tremoloDepth = (v & 0x80) !== 0;
      this.vibratoDepth = (v & 0x40) !== 0;
      return; // el modo percusión del OPL no se usa: `voices.ts` lo deja siempre a 0
    }

    const chBase = array * 9;
    if (r >= 0x20 && r <= 0x35) {
      const m = REG_TO_OP[r - 0x20];
      if (!m) return;
      const op = this.op(chBase + m[0], m[1]);
      if (!op) return;
      op.am = (v & 0x80) !== 0;
      op.vib = (v & 0x40) !== 0;
      op.egSustain = (v & 0x20) !== 0;
      op.ksr = (v & 0x10) !== 0;
      op.mult = v & 0x0f;
      return;
    }
    if (r >= 0x40 && r <= 0x55) {
      const m = REG_TO_OP[r - 0x40];
      if (!m) return;
      const ch = this.channels[chBase + m[0]];
      if (!ch) return;
      const op = m[1] ? ch.car : ch.mod;
      op.ksl = v >> 6;
      op.tl = v & 0x3f;
      op.updateKsl(ch.fnum, ch.block);
      return;
    }
    if (r >= 0x60 && r <= 0x75) {
      const m = REG_TO_OP[r - 0x60];
      if (!m) return;
      const op = this.op(chBase + m[0], m[1]);
      if (!op) return;
      op.attackRate = v >> 4;
      op.decayRate = v & 0x0f;
      return;
    }
    if (r >= 0x80 && r <= 0x95) {
      const m = REG_TO_OP[r - 0x80];
      if (!m) return;
      const op = this.op(chBase + m[0], m[1]);
      if (!op) return;
      // SL = 15 significa −93 dB, no −45: el salto a 31 es el del hardware.
      const sl = v >> 4;
      op.sustainLevel = (sl === 15 ? 31 : sl) << 4;
      op.releaseRate = v & 0x0f;
      return;
    }
    if (r >= 0xe0 && r <= 0xf5) {
      const m = REG_TO_OP[r - 0xe0];
      if (!m) return;
      const op = this.op(chBase + m[0], m[1]);
      if (!op) return;
      // En OPL2 sólo hay 4 ondas; en OPL3, 8.
      op.waveform = this.kind === "opl3" && this.opl3Enabled ? v & 0x07 : v & 0x03;
      return;
    }
    if (r >= 0xa0 && r <= 0xa8) {
      const ch = this.channels[chBase + (r - 0xa0)];
      if (!ch) return;
      ch.fnum = (ch.fnum & 0x300) | v;
      ch.updateKsl();
      return;
    }
    if (r >= 0xb0 && r <= 0xb8) {
      const ch = this.channels[chBase + (r - 0xb0)];
      if (!ch) return;
      ch.fnum = (ch.fnum & 0xff) | ((v & 0x03) << 8);
      ch.block = (v >> 2) & 0x07;
      ch.updateKsl();
      const on = (v & 0x20) !== 0;
      if (on && !ch.keyed) {
        ch.mod.keyOn();
        ch.car.keyOn();
      } else if (!on && ch.keyed) {
        ch.mod.keyOff();
        ch.car.keyOff();
      }
      ch.keyed = on;
      return;
    }
    if (r >= 0xc0 && r <= 0xc8) {
      const ch = this.channels[chBase + (r - 0xc0)];
      if (!ch) return;
      ch.feedback = (v >> 1) & 0x07;
      ch.additive = (v & 0x01) !== 0;
      if (this.kind === "opl3" && this.opl3Enabled) {
        ch.left = (v & 0x20) !== 0;
        ch.right = (v & 0x10) !== 0;
      } else {
        ch.left = true;
        ch.right = true;
      }
    }
  }

  private op(channel: number, carrier: boolean): Operator | undefined {
    const ch = this.channels[channel];
    if (!ch) return undefined;
    return carrier ? ch.car : ch.mod;
  }

  /** Incremento de fase de un operador, con el vibrato ya aplicado. */
  private phaseInc(ch: Channel, op: Operator): number {
    let fnum = ch.fnum;
    if (op.vib) {
      const unit = this.vibratoDepth ? fnum >> 8 : fnum >> 9;
      fnum += VIBRATO_STEPS[this.vibratoPos]! * unit;
      if (fnum < 0) fnum = 0;
    }
    return ((fnum << ch.block) * MULT2[op.mult]!) >> 1;
  }

  /** Avanza los dos LFO y el reloj de envolventes. */
  private advanceClocks(): void {
    this.egCounter = (this.egCounter + 1) >>> 0;
    if (++this.tremoloCounter >= TREMOLO_PERIOD) {
      this.tremoloCounter = 0;
      this.tremoloPos = (this.tremoloPos + 1) % TREMOLO_STEPS;
    }
    if (++this.vibratoCounter >= VIBRATO_PERIOD) {
      this.vibratoCounter = 0;
      this.vibratoPos = (this.vibratoPos + 1) & 7;
    }
  }

  /** Atenuación de trémolo de esta muestra, en unidades de 1/256 log2. */
  private tremoloAtten(): number {
    const half = TREMOLO_STEPS / 2;
    const tri = this.tremoloPos < half ? this.tremoloPos : TREMOLO_STEPS - this.tremoloPos;
    return (this.tremoloDepth ? tri : tri >> 2) * 8;
  }

  /**
   * Genera `count` muestras a 49716 Hz en `left`/`right` a partir de `offset`.
   *
   * 🔴 EL `offset` EXISTE PARA NO ASIGNAR. Antes el llamador pasaba `l.subarray(a, b)`, y
   * un `subarray` es un OBJETO NUEVO: dos por cada tramo generado, varias veces por bloque
   * de 128 muestras, en el hilo de audio. Asignar en `process()` acaba en pausa de GC, y
   * una pausa de GC en el hilo de audio es un chasquido. Con un índice no se asigna nada.
   */
  generate(left: Float32Array, right: Float32Array, count: number, offset = 0): void {
    for (let i = offset; i < offset + count; i++) {
      this.advanceClocks();
      const trem = this.tremoloAtten();
      let l = 0;
      let r = 0;

      for (let c = 0; c < this.channelCount; c++) {
        const ch = this.channels[c]!;
        if (ch.mod.egState === ENV_OFF && ch.car.egState === ENV_OFF) continue;
        const keyCode = ch.keyCode;
        ch.mod.advanceEnvelope(this.egCounter, keyCode);
        ch.car.advanceEnvelope(this.egCounter, keyCode);

        // Feedback: el modulador se realimenta con la MEDIA de sus dos salidas
        // anteriores, que es lo que hace el hardware (y por eso hace falta guardar dos).
        const fb =
          ch.feedback === 0 ? 0 : (ch.mod.prev1 + ch.mod.prev2) >> (9 - ch.feedback);
        const modOut = ch.mod.sample(this.phaseInc(ch, ch.mod), fb, trem);
        ch.mod.prev2 = ch.mod.prev1;
        ch.mod.prev1 = modOut;

        const carOut = ch.car.sample(
          this.phaseInc(ch, ch.car),
          ch.additive ? 0 : (modOut * MOD_SCALE) | 0,
          trem,
        );
        const out = ch.additive ? modOut + carOut : carOut;
        if (ch.left) l += out;
        if (ch.right) r += out;
      }

      // El hardware satura a 16 bits; aquí el recorte duro hace lo mismo.
      const lf = l * OUTPUT_SCALE;
      const rf = r * OUTPUT_SCALE;
      left[i] = lf > 1 ? 1 : lf < -1 ? -1 : lf;
      right[i] = rf > 1 ? 1 : rf < -1 ? -1 : rf;
    }
  }

  /** ¿Hay alguna voz sonando? Útil para no gastar CPU en silencio. */
  get isSilent(): boolean {
    for (const ch of this.channels) {
      if (ch.mod.egState !== ENV_OFF || ch.car.egState !== ENV_OFF) return false;
    }
    return true;
  }
}

/** Sólo para tests: las anclas comprobables de las dos ROM generadas. */
export const __tables = { LOG_SIN, EXP, expo };
