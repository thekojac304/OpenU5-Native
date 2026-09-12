/**
 * SECUENCIADOR — el reloj que une las tres piezas. Lee un Standard MIDI File, reparte
 * sus eventos en el tiempo, se los da al asignador de voces (`voices.ts`) y bombea el
 * chip (`chip.ts`) entre evento y evento.
 *
 * También RESAMPLEA: el OPL corre a 49716 Hz y Web Audio casi nunca. La conversión va
 * aquí, en el único sitio donde se conocen las dos tasas, y el chip sigue generando
 * SIEMPRE a la suya — que es la que hace que las notas caigan donde deben.
 *
 * ═══ DE DÓNDE SALE EL MIDI ══════════════════════════════════════════════════════════
 *
 * Del `.mid` que el extractor produce con `xmiToMidi` a partir del XMI del parche. Ese
 * conversor NORMALIZA el tiempo: descarta los meta-tempo del XMI (`type === 0x51 →
 * continue`) y emite UNO propio, 500000 µs/negra con division 60 = 120 ticks/s, que es
 * la tasa fija del XMI. Aquí se honra el tempo del fichero como en cualquier SMF, lo
 * cual da exactamente esos 120 ticks/s — general y correcto a la vez, sin hardcodear un
 * número que vendría de suponer quién generó el fichero.
 *
 * 🔴 LOS TIEMPOS SE PRECALCULAN EN UN PASE PREVIO, no evento a evento. Convertir ticks
 * a muestras sobre la marcha obliga a arrastrar el tempo vivo por el bucle de audio, y
 * cualquier error de acumulación ahí sale como una deriva lenta que sólo se nota a los
 * dos minutos de pista. Un pase previo con aritmética de doble precisión lo deja fijo.
 */
import type { MilesOplBank } from "./bank.js";
import { OplVoiceAllocator, type OplWrite } from "./voices.js";
import { OplEmulator, OPL_CLOCK_HZ, type OplChipKind } from "./chip.js";

/** Un evento MIDI ya situado en el tiempo. `bytes` es el mensaje crudo. */
export interface SmfEvent {
  readonly tick: number;
  readonly bytes: readonly number[];
}

export interface Smf {
  /** Ticks por negra (cabecera MThd). */
  readonly division: number;
  /** Eventos en orden de tick. */
  readonly events: readonly SmfEvent[];
}

function u32be(d: Uint8Array, o: number): number {
  return ((d[o]! << 24) | (d[o + 1]! << 16) | (d[o + 2]! << 8) | d[o + 3]!) >>> 0;
}

/**
 * Lee un Standard MIDI File de formato 0 ó 1. Acepta varios tracks y los MEZCLA por
 * tick (el conversor sólo emite uno, pero un lector que asuma eso se rompe en silencio
 * con cualquier otro fichero).
 */
export function parseSmf(bytes: Uint8Array): Smf {
  if (bytes.length < 14 || String.fromCharCode(...bytes.subarray(0, 4)) !== "MThd") {
    throw new Error("MIDI: falta la cabecera MThd");
  }
  const division = (bytes[12]! << 8) | bytes[13]!;
  if (division === 0 || (division & 0x8000) !== 0) {
    // SMPTE (bit alto) no lo produce el extractor y no se adivina.
    throw new Error(`MIDI: division no soportada (0x${division.toString(16)})`);
  }
  const events: SmfEvent[] = [];
  let pos = 8 + u32be(bytes, 4);

  while (pos + 8 <= bytes.length) {
    const id = String.fromCharCode(...bytes.subarray(pos, pos + 4));
    const len = u32be(bytes, pos + 4);
    const body = pos + 8;
    const end = Math.min(body + len, bytes.length);
    pos = body + len;
    if (id !== "MTrk") continue;

    let p = body;
    let tick = 0;
    let running = 0;
    while (p < end) {
      let delta = 0;
      for (;;) {
        const b = bytes[p++]!;
        delta = (delta << 7) | (b & 0x7f);
        if ((b & 0x80) === 0) break;
      }
      tick += delta;
      let status = bytes[p]!;
      if (status & 0x80) p++;
      else status = running;
      if (status === 0xff) {
        const type = bytes[p++]!;
        let l = 0;
        for (;;) {
          const b = bytes[p++]!;
          l = (l << 7) | (b & 0x7f);
          if ((b & 0x80) === 0) break;
        }
        events.push({ tick, bytes: [0xff, type, ...bytes.subarray(p, p + l)] });
        p += l;
        continue;
      }
      if (status === 0xf0 || status === 0xf7) {
        let l = 0;
        for (;;) {
          const b = bytes[p++]!;
          l = (l << 7) | (b & 0x7f);
          if ((b & 0x80) === 0) break;
        }
        p += l; // SysEx: el OPL no tiene nada que hacer con él
        continue;
      }
      running = status;
      const hi = status & 0xf0;
      const dataBytes = hi === 0xc0 || hi === 0xd0 ? 1 : 2;
      events.push({ tick, bytes: [status, ...bytes.subarray(p, p + dataBytes)] });
      p += dataBytes;
    }
  }

  // Estable: a igual tick se conserva el orden de lectura (importa en note-off/on juntos).
  const orden = new Map<SmfEvent, number>();
  events.forEach((e, i) => orden.set(e, i));
  events.sort((a, b) => a.tick - b.tick || orden.get(a)! - orden.get(b)!);
  return { division, events };
}

/**
 * Tiempo ABSOLUTO en muestras de cada evento, honrando los cambios de tempo.
 * Doble precisión: a 49716 Hz y pistas de minutos, `number` sobra de largo.
 */
export function eventSampleTimes(smf: Smf, sampleRate: number): Float64Array {
  const out = new Float64Array(smf.events.length);
  let usPerQuarter = 500000; // default MIDI, y el que emite el extractor
  let refTick = 0;
  let refSample = 0;
  let samplesPerTick = (usPerQuarter / smf.division / 1e6) * sampleRate;
  for (let i = 0; i < smf.events.length; i++) {
    const e = smf.events[i]!;
    out[i] = refSample + (e.tick - refTick) * samplesPerTick;
    if (e.bytes[0] === 0xff && e.bytes[1] === 0x51 && e.bytes.length >= 5) {
      refSample = out[i]!;
      refTick = e.tick;
      usPerQuarter = (e.bytes[2]! << 16) | (e.bytes[3]! << 8) | e.bytes[4]!;
      samplesPerTick = (usPerQuarter / smf.division / 1e6) * sampleRate;
    }
  }
  return out;
}

export interface OplSongPlayerOptions {
  readonly bank: MilesOplBank;
  readonly smf: Smf;
  readonly chip?: OplChipKind;
  /** Tasa de salida. Por defecto la nativa del chip (sin resampleo). */
  readonly sampleRate?: number;
  /** Repetir al acabar. Las pistas del parche no traen puntos de bucle (sin RBRN). */
  readonly loop?: boolean;
}

/**
 * Cola tras el último evento para una pista que NO se repite: deja acabar los releases
 * en vez de cortar en seco. No interviene en el bucle (ver `loopSample`).
 */
const TAIL_SAMPLES = OPL_CLOCK_HZ * 3;
/**
 * Capacidad INICIAL de los búferes de chip. Ya no es «lo que se rellena de golpe» — eso
 * era justo el defecto (ver `render`); ahora sólo evita realojar en el caso normal. El
 * worker de render pide trozos de 2 s (96 000 muestras a 48 kHz); `asegura()` agranda
 * una vez a esa medida y ya.
 */
const CHUNK = 4352;

/**
 * Reproductor de una pista: MIDI + banco → PCM estéreo a la tasa que se le pida.
 * No toca Web Audio; quien lo use le pasa los búferes.
 */
export class OplSongPlayer {
  private readonly alloc: OplVoiceAllocator;
  private readonly chip: OplEmulator;
  private readonly smf: Smf;
  private readonly times: Float64Array;
  private readonly loop: boolean;
  private readonly ratio: number;
  /**
   * Instante del BUCLE: el último evento, SIN cola. Ver el porqué en `fillChip`.
   */
  private readonly loopSample: number;
  /** Instante en que una pista sin bucle se da por acabada (último evento + cola). */
  private readonly endSample: number;

  private eventIndex = 0;
  private songSample = 0;
  private chipL = new Float32Array(CHUNK);
  private chipR = new Float32Array(CHUNK);
  private have = 0;
  private readPos = 0;
  private finished = false;

  constructor(options: OplSongPlayerOptions) {
    this.smf = options.smf;
    this.loop = options.loop ?? true;
    this.chip = new OplEmulator(options.chip ?? "opl3");
    // 🔴 SINK DIRECTO AL CHIP: el asignador escribe registro a registro en vez de
    // construir un array de objetos por evento. Medido: 234 bytes por `noteOn`, ~13 KB/s
    // durante la reproducción — la ÚLTIMA asignación que quedaba en el hilo de audio.
    // El problema nunca fue el volumen de memoria sino la PAUSA del recolector: para el
    // hilo de audio y se oye como un fallo. El cierre se crea UNA vez, aquí.
    this.alloc = new OplVoiceAllocator({
      bank: options.bank,
      chip: options.chip ?? "opl3",
      sink: (reg, value) => this.chip.writeReg(reg, value),
    });
    // Los tiempos SIEMPRE en muestras de chip: el resampleo es posterior y no debe
    // meterse en el reloj musical.
    this.times = eventSampleTimes(this.smf, OPL_CLOCK_HZ);
    const last = this.times.length > 0 ? this.times[this.times.length - 1]! : 0;
    // Mínimo de 1 muestra: una pista vacía no puede dar un bucle infinito y apretado.
    this.loopSample = Math.max(1, last);
    this.endSample = last + TAIL_SAMPLES;
    this.ratio = OPL_CLOCK_HZ / (options.sampleRate ?? OPL_CLOCK_HZ);
    this.apply(this.alloc.reset());
  }

  /**
   * Con `sink` puesto el asignador ya escribió en el chip y esto recibe siempre una lista
   * vacía. Se conserva por claridad en las llamadas y porque cuesta cero.
   */
  private apply(writes: readonly OplWrite[]): void {
    for (let i = 0; i < writes.length; i++) {
      const w = writes[i]!;
      this.chip.writeReg(w.reg, w.value);
    }
  }

  /** ¿Se acabó la pista (y no está en bucle)? */
  get ended(): boolean {
    return this.finished;
  }

  /** Vuelve al principio, sin notas colgadas. */
  reset(): void {
    this.apply(this.alloc.allNotesOff());
    this.apply(this.alloc.reset());
    this.eventIndex = 0;
    this.songSample = 0;
    this.finished = false;
  }

  private dispatch(e: SmfEvent): void {
    const status = e.bytes[0]!;
    if (status === 0xff) return; // metas: el tempo ya está horneado en `times`
    const ch = status & 0x0f;
    const d1 = e.bytes[1] ?? 0;
    const d2 = e.bytes[2] ?? 0;
    switch (status & 0xf0) {
      case 0x90:
        this.apply(this.alloc.noteOn(ch, d1, d2));
        return;
      case 0x80:
        this.apply(this.alloc.noteOff(ch, d1));
        return;
      case 0xb0:
        this.apply(this.alloc.controlChange(ch, d1, d2));
        return;
      case 0xc0:
        this.apply(this.alloc.programChange(ch, d1));
        return;
      case 0xe0:
        this.apply(this.alloc.pitchBend(ch, d1 | (d2 << 7)));
        return;
      default:
        return; // aftertouch y presión de canal: el OPL no los representa
    }
  }

  /** Genera `n` muestras A TASA DE CHIP, despachando los eventos que caigan dentro. */
  private fillChip(l: Float32Array, r: Float32Array, n: number, base = 0): void {
    let done = 0;
    while (done < n) {
      // 1. Todo lo que ya toca, antes de generar una sola muestra más.
      while (
        this.eventIndex < this.smf.events.length &&
        this.times[this.eventIndex]! <= this.songSample
      ) {
        this.dispatch(this.smf.events[this.eventIndex]!);
        this.eventIndex++;
      }

      // 2. Cuánto se puede generar de un tirón sin pasarse del siguiente evento.
      let budget = n - done;
      if (this.eventIndex < this.smf.events.length) {
        const until = Math.ceil(this.times[this.eventIndex]! - this.songSample);
        if (until < budget) budget = Math.max(1, until);
      } else if (this.loop) {
        // ═══ 🔴 BUCLE SIN COSTURA: se vuelve al principio EN EL ÚLTIMO EVENTO, sin cola
        //     y SIN cortar las notas ══════════════════════════════════════════════════
        //
        // La versión anterior esperaba un segundo entero y luego llamaba a
        // `allNotesOff()`. Las dos mitades estaban mal, y se midió por qué (2026-09-11):
        //
        //   · LA COLA sobraba: las 15 pistas acaban con sus note-off en el ÚLTIMO tick
        //     —cero notas colgadas, medido—, así que ese segundo era sólo el release
        //     apagándose. Un silencio de un segundo cada vuelta en música de fondo.
        //   · Y NO BASTABA: los releases de este banco tardan MÁS DE TRES segundos en
        //     extinguirse (medido en 5 de 6 pistas), o sea que al segundo exacto el
        //     `allNotesOff()` los cortaba a mitad. Cola audible Y corte: lo peor de ambas.
        //
        // Un secuenciador de verdad no hace nada de eso: rebobina y ya. Las colas de la
        // vuelta anterior siguen sonando ENCIMA del arranque de la nueva, y sus voces se
        // reutilizan solas — `pickVoice` prefiere las que no están pulsadas, y una voz en
        // release lo está. El resultado es el empalme que la pista tenía compuesto.
        this.eventIndex = 0;
        this.songSample = 0;
        continue;
      } else if (this.songSample >= this.endSample) {
        this.finished = true;
        l.fill(0, base + done, base + n);
        r.fill(0, base + done, base + n);
        return;
      }

      // Con `offset`, sin `subarray`: cero asignaciones en el hilo de audio.
      this.chip.generate(l, r, budget, base + done);
      this.songSample += budget;
      done += budget;
    }
  }

  /** Agranda los búferes de chip conservando lo ya generado. */
  private asegura(capacidad: number): void {
    if (capacidad <= this.chipL.length) return;
    const l = new Float32Array(capacidad);
    const r = new Float32Array(capacidad);
    l.set(this.chipL.subarray(0, this.have));
    r.set(this.chipR.subarray(0, this.have));
    this.chipL = l;
    this.chipR = r;
  }

  /**
   * Rellena `left`/`right` con `count` muestras a la tasa de salida pedida.
   * Interpolación lineal entre muestras de chip: a 49716→48000 el factor es 1,036, así
   * que la lineal sobra — no hay banda que proteger a esa proporción.
   *
   * ═══ 🔴 SE GENERA EXACTAMENTE LO DE ESTE BLOQUE, NUNCA UN BÚFER ENTERO ═════════════
   *
   * La primera versión rellenaba un búfer de 2048 muestras de chip en cuanto se agotaba.
   * Suena inofensivo y no lo es: un `AudioWorklet` pide bloques de 128 muestras, así que
   * de cada ~15 llamadas CATORCE no hacían nada y UNA sintetizaba 2048 muestras de golpe.
   * Medido el 2026-09-11 sobre la pista de portada a 48 kHz, con un presupuesto de
   * 2,67 ms por llamada:
   *
   *     mediana 0,001 ms · p99 0,975 ms · MÁXIMO 5,996 ms  ⇒ 2,2× por encima del plazo
   *
   * Mil veces de diferencia entre la mediana y el pico. Cada vez que el pico se pasaba
   * del plazo, el hilo de audio no llegaba y salía un CHASQUIDO — el «pop/skip» que
   * reportó el usuario en escritorio. Y no se veía en las pruebas offline porque offline
   * NO HAY PLAZO: el fichero renderizado salía perfecto, sin una sola discontinuidad.
   *
   * Ahora cada llamada genera sólo las muestras de chip que consume (unas 133 para un
   * bloque de 128 a 48 kHz), y el coste queda plano y repartido. El búfer conserva entre
   * llamadas la muestra suelta que la interpolación necesita mirar por delante.
   */
  render(left: Float32Array, right: Float32Array, count: number): void {
    if (count <= 0) return;

    // 1. Tirar lo ya consumido y recolocar el resto al principio.
    const drop = Math.floor(this.readPos);
    if (drop > 0) {
      this.chipL.copyWithin(0, drop, this.have);
      this.chipR.copyWithin(0, drop, this.have);
      this.have -= drop;
      this.readPos -= drop;
    }

    // 2. Generar SÓLO lo que pide este bloque. El `+2` cubre la muestra siguiente que
    //    mira el interpolador en la última posición.
    const necesarias = Math.floor(this.readPos + this.ratio * (count - 1)) + 2;
    this.asegura(necesarias);
    const faltan = necesarias - this.have;
    if (faltan > 0) {
      this.fillChip(this.chipL, this.chipR, faltan, this.have);
      this.have += faltan;
    }

    // 3. Remuestrear.
    for (let i = 0; i < count; i++) {
      const i0 = Math.floor(this.readPos);
      const frac = this.readPos - i0;
      const a = this.chipL[i0]!;
      const b = this.chipL[i0 + 1]!;
      const c = this.chipR[i0]!;
      const d = this.chipR[i0 + 1]!;
      left[i] = a + (b - a) * frac;
      right[i] = c + (d - c) * frac;
      this.readPos += this.ratio;
    }
  }
}
