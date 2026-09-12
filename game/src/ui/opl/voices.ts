/**
 * ASIGNADOR DE VOCES OPL — traduce un flujo de eventos MIDI a ESCRITURAS DE REGISTRO
 * del OPL2/OPL3. Capa PURA: no emula el chip, no toca Web Audio, no lee el reloj. Entra
 * un evento, salen los bytes que habría que volcar al puerto. Eso la hace testeable
 * entera sin audio, igual que la capa pura de `skin/fiel/speaker.ts`.
 *
 * El que EMULA el chip y el que lo RELOJEA son otros dos módulos; éste sólo decide
 * «qué voz, con qué timbre, a qué altura y a qué volumen», que es donde está toda la
 * lógica que se puede equivocar en silencio.
 *
 * ═══ MEDIDO SOBRE EL CORPUS REAL (los 15 XMI del parche, 2026-09-11) ═════════════════
 *
 * El alcance de este módulo no se ha adivinado: se ha medido recorriendo los flujos que
 * produce `extractor/src/audio/xmi2midi.ts` sobre los 15 XMI instalados por el parche.
 *
 *   · CANALES 0-8, 10, 11, 12. **El canal 9 no aparece NUNCA** ⇒ el corpus de Ultima V
 *     no tiene percusión, y los 53 timbres de batería de `FAT.OPL` no se usan. Aun así
 *     se implementan (son ~15 líneas y el banco los trae): callarlos sería un agujero
 *     arbitrario, y tratarlos como melódicos sería un defecto latente.
 *   · 🔴 **NO HAY UN SOLO NOTE-OFF 0x80.** Todos los finales de nota llegan como
 *     NOTE-ON CON VELOCIDAD 0 — es lo que emite el conversor al expandir la duración
 *     VLQ del XMI. Un asignador que sólo mire 0x8n deja las notas colgadas para siempre
 *     y el defecto no se ve hasta que suenan diecisiete a la vez.
 *   · CONTROLADORES presentes: 1, 7, 10, 11, 32, 64, 91, 93, 119, 121. De ésos, los que
 *     CAMBIAN lo que se oye aquí son 7 (volumen), 10 (pan), 11 (expresión), 64 (pedal)
 *     y 121 (reset). El resto se acepta y se ignora, a propósito y documentado abajo.
 *   · 103 eventos de PITCH BEND ⇒ hay que recalcular F-Number/Block en vivo.
 *   · PICO DE 17 NOTAS SIMULTÁNEAS. Un OPL2 tiene NUEVE voces: el robo de voz no es un
 *     caso de borde teórico, es el caso normal de este corpus. Por eso el chip por
 *     defecto es el OPL3 (18 voces, donde 17 sí caben) y el OPL2 queda como opción.
 *
 * ═══ EL ESPACIO DE REGISTROS ════════════════════════════════════════════════════════
 *
 * `OplWrite.reg` va en 0x000..0x1FF: por debajo de 0x100 es el primer banco de
 * registros; de 0x100 en adelante, el SEGUNDO (las voces 9..17, sólo OPL3). Es la
 * convención de los emuladores tipo DBOPL, y evita tener que sacar un «puerto» aparte.
 */
import type { MilesOplBank, OplOperator, OplTimbre } from "./bank.js";

/** Una escritura de registro: `reg` en 0x000..0x1FF (≥0x100 = segundo banco, OPL3). */
export interface OplWrite {
  readonly reg: number;
  readonly value: number;
}

/** Chip de destino. El corpus pide 17 voces, así que el default es el OPL3. */
export type OplChip = "opl2" | "opl3";

/** Reloj del OPL en Hz — el divisor de la fórmula de F-Number. */
export const OPL_SAMPLE_RATE = 49716;

/** Canal de percusión de General MIDI (0-indexado). */
export const DRUM_CHANNEL = 9;

/**
 * Rango del pitch bend, en semitonos. Fijo a ±2 (el default de GM) porque el corpus
 * NO usa los RPN que lo cambiarían: entre los controladores medidos no hay 100/101/6.
 */
export const BEND_RANGE_SEMITONES = 2;

/** Array vacío COMPARTIDO: devolverlo no asigna. */
const SIN_ESCRITURAS: OplWrite[] = [];

/** Desplazamiento de los operadores de cada una de las 9 voces de un banco. */
const OP_OFFSET = [0x00, 0x01, 0x02, 0x08, 0x09, 0x0a, 0x10, 0x11, 0x12] as const;
const VOICES_PER_ARRAY = 9;

const REG_TEST_WAVE_ENABLE = 0x01;
const REG_CSM_NOTESEL = 0x08;
const REG_RHYTHM = 0xbd;
const REG_OPL3_4OP = 0x104;
const REG_OPL3_ENABLE = 0x105;

/** Bits 4/5 de 0xC0: salida derecha e izquierda del OPL3. */
const STEREO_RIGHT = 0x10;
const STEREO_LEFT = 0x20;
const STEREO_BOTH = STEREO_LEFT | STEREO_RIGHT;

/** Bit 5 de 0xB0: key-on. */
const KEY_ON = 0x20;

/**
 * Nota MIDI (con bend ya aplicado, en semitonos) → F-Number y Block del OPL.
 *
 * `F-Number = freq · 2^(20−Block) / 49716`. Se elige el Block MÁS BAJO en el que el
 * F-Number entra en 10 bits, que es el que deja más resolución de afinación.
 * Comprobación de mesa: La 440 Hz ⇒ Block 4, F-Number 580.
 */
export function noteToFnumBlock(
  note: number,
  bendSemitones = 0,
): { fnum: number; block: number } {
  const freq = 440 * Math.pow(2, (note + bendSemitones - 69) / 12);
  let block = 0;
  let fnum = (freq * 0x100000) / OPL_SAMPLE_RATE;
  while (fnum > 1023 && block < 7) {
    block++;
    fnum /= 2;
  }
  return { fnum: Math.max(0, Math.min(1023, Math.round(fnum))), block };
}

/**
 * Aplica un volumen 0..1 al byte KSL/TL de un operador.
 *
 * TL es ATENUACIÓN (0 = máximo volumen, 63 = mudo), así que la escala va hacia arriba.
 * A volumen 1 se respeta EXACTAMENTE el TL que el banco le dio al timbre — el diseño
 * del instrumento manda — y a volumen 0 se llega a 63. Monótona y sin recortes raros.
 * Los bits de KSL (7-6) se conservan.
 */
export function scaleTl(kslTl: number, vol01: number): number {
  const base = kslTl & 0x3f;
  const v = Math.max(0, Math.min(1, vol01));
  const tl = Math.round(base + (63 - base) * (1 - v));
  return (kslTl & 0xc0) | Math.max(0, Math.min(63, tl));
}

interface ChannelState {
  program: number;
  volume: number;
  expression: number;
  pan: number;
  bendSemitones: number;
  sustain: boolean;
}

interface Voice {
  midiCh: number;
  /** Nota del EVENTO (con la que llegará su note-off). */
  note: number;
  /** Nota que realmente suena (la fija del timbre, si es percusión). */
  playedNote: number;
  timbre: OplTimbre | null;
  velocity: number;
  keyed: boolean;
  sustained: boolean;
  fnum: number;
  block: number;
  ageOn: number;
  ageOff: number;
}

function nuevoCanal(): ChannelState {
  // Defaults de General MIDI: volumen 100, expresión al máximo, pan centrado.
  return {
    program: 0,
    volume: 100,
    expression: 127,
    pan: 64,
    bendSemitones: 0,
    sustain: false,
  };
}

function nuevaVoz(): Voice {
  return {
    midiCh: -1,
    note: -1,
    playedNote: -1,
    timbre: null,
    velocity: 0,
    keyed: false,
    sustained: false,
    fnum: 0,
    block: 0,
    ageOn: 0,
    ageOff: 0,
  };
}

export interface OplVoiceAllocatorOptions {
  readonly bank: MilesOplBank;
  /** Chip de destino. Default `"opl3"` (18 voces: el corpus llega a 17 simultáneas). */
  readonly chip?: OplChip;
  /**
   * Si se da, cada escritura se entrega AQUÍ y los métodos dejan de construir arrays.
   *
   * 🔴 EXISTE PARA NO ASIGNAR EN EL HILO DE AUDIO. Sin él, cada nota construye un array
   * y ~13 objetos `{reg,value}`: medido, 234 bytes por `noteOn` y ~13 KB/s durante la
   * reproducción. No es mucho en absoluto, pero es LA ÚNICA asignación que queda en el
   * camino de audio, y lo que fastidia ahí no es el volumen sino la PAUSA: cuando el
   * recolector entra, el hilo de audio se para y se oye un fallo. Con sink, cero.
   *
   * Sin sink el API sigue devolviendo arrays, que es lo que usan los tests.
   */
  readonly sink?: (reg: number, value: number) => void;
}

/**
 * Asignador de voces. Cada método devuelve las escrituras que provoca, en orden; si un
 * evento no cambia nada audible devuelve un array vacío (nunca `null`).
 */
export class OplVoiceAllocator {
  readonly chip: OplChip;
  readonly voiceCount: number;
  private readonly bank: MilesOplBank;
  private readonly channels: ChannelState[] = [];
  private readonly voices: Voice[] = [];
  private age = 0;
  /** Destino directo de las escrituras (sin asignar). Ver `OplVoiceAllocatorOptions`. */
  private readonly sink: ((reg: number, value: number) => void) | null;
  /** Acumulador del API de arrays; `null` cuando hay sink. */
  private salida: OplWrite[] | null = null;

  /** Una escritura: al sink si lo hay, al array si no. */
  private emit(reg: number, value: number): void {
    if (this.sink !== null) this.sink(reg, value);
    else this.salida!.push({ reg, value });
  }

  private abre(): void {
    if (this.sink === null) this.salida = [];
  }

  private cierra(): OplWrite[] {
    if (this.sink !== null) return SIN_ESCRITURAS;
    const s = this.salida!;
    this.salida = null;
    return s;
  }

  /** Salida vacía sin asignar (los caminos que no escriben nada). */
  private vacio(): OplWrite[] {
    return SIN_ESCRITURAS;
  }

  constructor(options: OplVoiceAllocatorOptions) {
    this.bank = options.bank;
    this.sink = options.sink ?? null;
    this.chip = options.chip ?? "opl3";
    this.voiceCount = this.chip === "opl3" ? 18 : 9;
    for (let i = 0; i < 16; i++) this.channels.push(nuevoCanal());
    for (let i = 0; i < this.voiceCount; i++) this.voices.push(nuevaVoz());
  }

  /** Registro base del banco al que pertenece la voz (0x000 o 0x100). */
  private arrayBase(voice: number): number {
    return voice < VOICES_PER_ARRAY ? 0x000 : 0x100;
  }

  /** Índice de canal DENTRO de su banco (0..8). */
  private slot(voice: number): number {
    return voice % VOICES_PER_ARRAY;
  }

  private opOffset(voice: number): number {
    return OP_OFFSET[this.slot(voice)]!;
  }

  /**
   * Deja el chip en un estado conocido y mudo. En OPL3 hay que habilitarlo ANTES de
   * nada: sin el bit de `0x105` el segundo banco de registros sencillamente no existe
   * y las nueve voces altas se escriben al vacío.
   */
  reset(): OplWrite[] {
    this.abre();
    if (this.chip === "opl3") {
      this.emit(REG_OPL3_ENABLE, 0x01);
      this.emit(REG_OPL3_4OP, 0x00); // las 18 voces, todas de 2 operadores
    }
    // Bit 5 de 0x01 = selector de onda. En OPL2 sin él los registros 0xE0 se ignoran y
    // todo suena a seno; en OPL3 es inocuo.
    this.emit(REG_TEST_WAVE_ENABLE, 0x20);
    this.emit(REG_CSM_NOTESEL, 0x00);
    this.emit(REG_RHYTHM, 0x00);
    if (this.chip === "opl3") this.emit(0x100 + REG_RHYTHM, 0x00);

    for (let i = 0; i < this.voiceCount; i++) {
      const base = this.arrayBase(i);
      const off = this.opOffset(i);
      this.emit(base + 0xb0 + this.slot(i), 0x00);
      this.emit(base + 0x40 + off, 0x3f);
      this.emit(base + 0x40 + off + 3, 0x3f);
      this.voices[i] = nuevaVoz();
    }
    for (let i = 0; i < 16; i++) this.channels[i] = nuevoCanal();
    this.age = 0;
    return this.cierra();
  }

  /** Volumen efectivo 0..1 de una nota: velocidad × volumen de canal × expresión. */
  private volumeOf(ch: ChannelState, velocity: number): number {
    return (velocity / 127) * (ch.volume / 127) * (ch.expression / 127);
  }

  /** Byte 0xC0 del timbre con los bits de estéreo que el banco no trae (ver bank.ts). */
  private stereoFor(ch: ChannelState, timbre: OplTimbre): number {
    if (this.chip !== "opl3") return timbre.feedbackConnection & 0x0f;
    // El OPL3 sólo tiene izquierda/derecha, no pan continuo: se reparte en tres tramos.
    const pan = ch.pan <= 42 ? STEREO_LEFT : ch.pan >= 85 ? STEREO_RIGHT : STEREO_BOTH;
    return (timbre.feedbackConnection & 0x0f) | pan;
  }

  private writeOperator(
    base: number,
    off: number,
    op: OplOperator,
    kslTl: number,
  ): void {
    this.emit(base + 0x20 + off, op.amVibEgKsrMult);
    this.emit(base + 0x40 + off, kslTl);
    this.emit(base + 0x60 + off, op.attackDecay);
    this.emit(base + 0x80 + off, op.sustainRelease);
    this.emit(base + 0xe0 + off, op.waveform);
  }

  /**
   * Vuelca el timbre entero de una voz, con el volumen ya aplicado.
   *
   * 🔴 SI LA CONEXIÓN ES ADITIVA (bit 0 de 0xC0 = 1) LOS DOS OPERADORES SON
   * PORTADORAS, y el volumen tiene que escalar los dos. Escalando sólo el segundo, los
   * timbres aditivos del banco ignoran el volumen del canal a medias: suenan siempre
   * casi igual de fuertes y el crossfade entre contextos deja de funcionar en ellos.
   */
  private writeTimbre(voice: number, vol01: number): void {
    const v = this.voices[voice]!;
    const t = v.timbre;
    if (!t) return;
    const base = this.arrayBase(voice);
    const off = this.opOffset(voice);
    const aditivo = (t.feedbackConnection & 0x01) === 1;
    this.writeOperator(
      base,
      off,
      t.modulator,
      aditivo ? scaleTl(t.modulator.kslTl, vol01) : t.modulator.kslTl,
    );
    this.writeOperator(base, off + 3, t.carrier, scaleTl(t.carrier.kslTl, vol01));
    this.emit(base + 0xc0 + this.slot(voice), this.stereoFor(this.channels[v.midiCh]!, t));
  }

  /** Escribe F-Number/Block de una voz, con o sin key-on. */
  private writePitch(voice: number, keyOn: boolean): void {
    const v = this.voices[voice]!;
    const base = this.arrayBase(voice);
    const slot = this.slot(voice);
    this.emit(base + 0xa0 + slot, v.fnum & 0xff);
    this.emit(base + 0xb0 + slot, ((v.fnum >> 8) & 0x03) | ((v.block & 0x07) << 2) | (keyOn ? KEY_ON : 0));
  }

  /**
   * Elige voz para (canal, nota). Orden: la MISMA nota si ya sonaba (retrigger), luego
   * una libre, luego una retenida por el pedal, y sólo al final se roba la más antigua
   * que esté sonando. Robar una que suena es lo más audible de todo, así que va última.
   */
  private pickVoice(midiCh: number, note: number): number {
    let libre = -1;
    let libreAge = Infinity;
    let retenida = -1;
    let retenidaAge = Infinity;
    let sonando = -1;
    let sonandoAge = Infinity;
    for (let i = 0; i < this.voices.length; i++) {
      const v = this.voices[i]!;
      if (v.keyed && v.midiCh === midiCh && v.note === note) return i;
      if (!v.keyed && !v.sustained) {
        if (v.ageOff < libreAge) {
          libreAge = v.ageOff;
          libre = i;
        }
      } else if (v.sustained) {
        if (v.ageOn < retenidaAge) {
          retenidaAge = v.ageOn;
          retenida = i;
        }
      } else if (v.ageOn < sonandoAge) {
        sonandoAge = v.ageOn;
        sonando = i;
      }
    }
    if (libre >= 0) return libre;
    if (retenida >= 0) return retenida;
    return sonando >= 0 ? sonando : 0;
  }

  /**
   * Note-on. Con `velocity` 0 delega en `noteOff` — que es COMO LLEGAN TODOS los
   * finales de nota de este corpus (ver la cabecera).
   */
  noteOn(midiCh: number, note: number, velocity: number): OplWrite[] {
    if (velocity === 0) return this.noteOff(midiCh, note);
    const ch = this.channels[midiCh & 0x0f];
    if (!ch) return this.vacio();

    const timbre =
      (midiCh & 0x0f) === DRUM_CHANNEL
        ? this.bank.percussion(note)
        : this.bank.melodic(ch.program);
    // Percusión que el banco no trae: se ignora la nota en vez de sonar cualquier cosa.
    if (!timbre) return this.vacio();

    const voice = this.pickVoice(midiCh & 0x0f, note);
    const v = this.voices[voice]!;
    this.abre();

    // Una voz que se roba se apaga ANTES de reprogramarla: si no, el envolvente no
    // rearranca y la nota nueva hereda la fase de la vieja (un «clic» y un ataque perdido).
    if (v.keyed || v.sustained) {
      const base = this.arrayBase(voice);
      this.emit(base + 0xb0 + this.slot(voice), 0x00);
    }

    v.midiCh = midiCh & 0x0f;
    v.note = note;
    v.playedNote = timbre.fixedNote > 0 ? timbre.fixedNote : note;
    v.timbre = timbre;
    v.velocity = velocity;
    v.keyed = true;
    v.sustained = false;
    v.ageOn = ++this.age;
    const { fnum, block } = noteToFnumBlock(v.playedNote, ch.bendSemitones);
    v.fnum = fnum;
    v.block = block;

    this.writeTimbre(voice, this.volumeOf(ch, velocity));
    this.writePitch(voice, true);
    return this.cierra();
  }

  /** Note-off. Con el pedal pisado la voz NO se suelta: queda retenida. */
  noteOff(midiCh: number, note: number): OplWrite[] {
    const ch = this.channels[midiCh & 0x0f];
    if (!ch) return this.vacio();
    this.abre();
    for (let i = 0; i < this.voices.length; i++) {
      const v = this.voices[i]!;
      if (!v.keyed || v.midiCh !== (midiCh & 0x0f) || v.note !== note) continue;
      if (ch.sustain) {
        v.sustained = true;
        continue;
      }
      this.releaseVoice(i);
    }
    return this.cierra();
  }

  private releaseVoice(voice: number): void {
    const v = this.voices[voice]!;
    v.keyed = false;
    v.sustained = false;
    v.ageOff = ++this.age;
    this.writePitch(voice, false);
  }

  programChange(midiCh: number, program: number): OplWrite[] {
    const ch = this.channels[midiCh & 0x0f];
    if (ch) ch.program = program & 0x7f;
    // No se reprograman las voces vivas: en MIDI el cambio de programa afecta a las
    // notas SIGUIENTES, no a las que ya están sonando.
    return this.vacio();
  }

  pitchBend(midiCh: number, value14: number): OplWrite[] {
    const ch = this.channels[midiCh & 0x0f];
    if (!ch) return this.vacio();
    ch.bendSemitones = ((value14 - 8192) / 8192) * BEND_RANGE_SEMITONES;
    this.abre();
    this.refreshPitch(midiCh & 0x0f);
    return this.cierra();
  }

  /** Emisor puro: NO abre ni cierra — lo hace quien lo llama (se componen entre sí). */
  private refreshPitch(midiCh: number): void {
    const ch = this.channels[midiCh]!;
    for (let i = 0; i < this.voices.length; i++) {
      const v = this.voices[i]!;
      if (v.midiCh !== midiCh || (!v.keyed && !v.sustained)) continue;
      const { fnum, block } = noteToFnumBlock(v.playedNote, ch.bendSemitones);
      v.fnum = fnum;
      v.block = block;
      this.writePitch(i, true);
    }
  }

  /** Emisor puro, como `refreshPitch`. */
  private refreshVolume(midiCh: number): void {
    const ch = this.channels[midiCh]!;
    for (let i = 0; i < this.voices.length; i++) {
      const v = this.voices[i]!;
      if (v.midiCh !== midiCh || (!v.keyed && !v.sustained) || !v.timbre) continue;
      this.writeTimbre(i, this.volumeOf(ch, v.velocity));
    }
  }

  /**
   * Control change. Los que no cambian nada audible aquí (1 modulación, 32 bank LSB,
   * 91 reverb, 93 chorus, 119) se ACEPTAN Y SE IGNORAN: están en el corpus medido, y
   * el OPL no tiene con qué representarlos. Devolver `[]` es la respuesta correcta —
   * no un `default:` que se cuele como nota.
   */
  controlChange(midiCh: number, controller: number, value: number): OplWrite[] {
    const c = midiCh & 0x0f;
    const ch = this.channels[c];
    if (!ch) return this.vacio();
    this.abre();
    switch (controller) {
      case 7:
        ch.volume = value & 0x7f;
        this.refreshVolume(c);
        break;
      case 11:
        ch.expression = value & 0x7f;
        this.refreshVolume(c);
        break;
      case 10:
        ch.pan = value & 0x7f;
        this.refreshVolume(c); // reescribe 0xC0 con los bits de estéreo nuevos
        break;
      case 64: {
        const pisado = value >= 64;
        if (ch.sustain !== pisado) {
          ch.sustain = pisado;
          if (!pisado) {
            for (let i = 0; i < this.voices.length; i++) {
              const v = this.voices[i]!;
              if (v.midiCh === c && v.sustained) this.releaseVoice(i);
            }
          }
        }
        break;
      }
      case 120:
      case 123:
        this.sueltaTodas(c);
        break;
      case 121: {
        // Reset all controllers: vuelve a los defaults SIN tocar el programa ni las
        // notas vivas (eso es 120/123), que es lo que dice GM.
        ch.volume = 100;
        ch.expression = 127;
        ch.pan = 64;
        ch.bendSemitones = 0;
        if (ch.sustain) {
          ch.sustain = false;
          for (let i = 0; i < this.voices.length; i++) {
            const v = this.voices[i]!;
            if (v.midiCh === c && v.sustained) this.releaseVoice(i);
          }
        }
        this.refreshVolume(c);
        this.refreshPitch(c);
        break;
      }
      default:
        break;
    }
    return this.cierra();
  }

  /** Suelta todas las notas: de un canal, o de todos si no se da canal. */
  allNotesOff(midiCh?: number): OplWrite[] {
    this.abre();
    this.sueltaTodas(midiCh);
    return this.cierra();
  }

  /** Emisor puro de `allNotesOff` (lo reusa CC120/123 sin anidar abre/cierra). */
  private sueltaTodas(midiCh?: number): void {
    for (let i = 0; i < this.voices.length; i++) {
      const v = this.voices[i]!;
      if (midiCh !== undefined && v.midiCh !== (midiCh & 0x0f)) continue;
      if (v.keyed || v.sustained) this.releaseVoice(i);
    }
  }

  /** Instantánea de depuración: cuántas voces suenan y cuáles están retenidas. */
  get activeVoices(): number {
    return this.voices.filter((v) => v.keyed || v.sustained).length;
  }
}
