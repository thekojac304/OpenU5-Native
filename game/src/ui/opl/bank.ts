/**
 * BANCO DE TIMBRES OPL DEL PARCHE — lector del formato «Miles AIL Global Timbre
 * Library» (`.OPL`). Capa PURA: bytes → definiciones de instrumento. No toca Web
 * Audio, ni el DOM, ni el estado del juego.
 *
 * POR QUÉ EXISTE. La música del port venía de pistas OGG pre-renderizadas con un
 * soundfont ajeno (GeneralUser GS) — «nuestra interpretación» del parche comunitario.
 * El parche de Voyager Dragon (Ultima V Upgrade Patch, The Exodus Project) instala en
 * el directorio del juego los XMI **y su banco de timbres**, `FAT.OPL`: las mismas
 * definiciones de registro que el driver AdLib de Miles (`ADLIB.ADV`) volcaba al chip.
 * Con el banco y los XMI se sintetiza en vivo lo que sonaba en una AdLib de verdad, sin
 * soundfont, sin fluidsynth, sin ffmpeg y sin 15 MB de OGG.
 *
 * ⚠️ ESTO NO ES FIDELIDAD, y no se vende como tal. `re/notes/audio-profile-1988.md` es
 * inequívoco: el DOS de 1988 en un PC estándar era MUDO (sólo PC-speaker). La música
 * sigue siendo un añadido de comunidad. Lo que mejora es la PROCEDENCIA: de «nuestro
 * render con un soundfont de terceros» a «los datos del propio parche, sobre el hardware
 * al que apuntaba». La entrada ❌ de `docs/FIDELITY.md` sigue siendo ❌.
 *
 * ═══ EL FORMATO, DERIVADO DE `FAT.OPL` (medido el 2026-09-11) ═══════════════════════
 *
 * No hay número mágico ni cabecera: el fichero ARRANCA con el índice.
 *
 *   ÍNDICE   registros de 6 bytes, terminados por el centinela u16 `0xFFFF`:
 *              u8  patch      (nº de programa GM, o nota MIDI si es percusión)
 *              u8  bank       (0 = melódico, 127 = percusión)
 *              u32 offset LE  (posición absoluta del bloque de timbre)
 *   BLOQUES  en `offset`, cada uno:
 *              u16 size LE    (tamaño del bloque, 14 en todo `FAT.OPL`)
 *              u8  fixedNote  (percusión: nota FIJA a la que suena; melódico: 0)
 *              11 bytes de registros OPL, en este orden:
 *                [0..4]  modulador: 0x20, 0x40, 0x60, 0x80, 0xE0
 *                [5]     0xC0 del canal: FB (bits 3-1) | CNT (bit 0)
 *                [6..10] portadora: 0x20, 0x40, 0x60, 0x80, 0xE0
 *
 * MEDIDA QUE FIJA ESE ORDEN (y no es una conjetura de tabla ajena): sobre los 181
 * timbres de `FAT.OPL`, los bytes [4] y [10] de los 362 operadores caen SIEMPRE en
 * 0..3 — el rango exacto del selector de onda del OPL2 — y el byte [5] no enciende
 * NUNCA los bits 6-7, que en 0xC0 no existen. Dos invariantes que sólo se cumplen si
 * el orden es ése; cualquier permutación las rompe.
 *
 * CONTENIDO DE `FAT.OPL`: 181 timbres = 128 melódicos (bank 0, patches 0..127, el GM
 * completo) + 53 de percusión (bank 127, patches 35..87, el mapa de batería GM). Todos
 * los bloques miden 14 bytes; no sobra un solo byte al final del fichero.
 *
 * 🔴 EL `fixedNote` NO ES UN «tipo». Se lee como si fuera un discriminante de formato
 * porque vale 0 en los 128 melódicos, pero en los 53 de percusión vale 24..84 — son
 * NOTAS MIDI. Un lector que lo trate como flag reproduce la batería a la altura
 * equivocada. En el corpus de Ultima V da igual (ninguna canción usa el canal 9,
 * medido), pero el banco SÍ trae los timbres y leerlos mal sería un defecto latente.
 */

/** Bank de los timbres melódicos (programas GM 0..127). */
export const MILES_BANK_MELODIC = 0;
/** Bank de los timbres de percusión (indexados por NOTA MIDI, no por programa). */
export const MILES_BANK_PERCUSSION = 127;

/** Bytes de índice por entrada, y centinela de fin de índice. */
const INDEX_ENTRY_BYTES = 6;
const INDEX_TERMINATOR = 0xffff;
/** Tamaño del bloque de un timbre de 2 operadores (los 181 de `FAT.OPL`). */
const BLOCK_BYTES_2OP = 14;

/**
 * Un operador del OPL, tal cual va a sus cinco registros. Los nombres dicen QUÉ
 * registro son, porque es lo único que hace falta para volcarlos.
 */
export interface OplOperator {
  /** reg 0x20+off — AM | VIB | EG-type | KSR | MULT. */
  readonly amVibEgKsrMult: number;
  /** reg 0x40+off — KSL (bits 7-6) | TL (bits 5-0; 0 = máximo volumen). */
  readonly kslTl: number;
  /** reg 0x60+off — Attack (bits 7-4) | Decay (bits 3-0). */
  readonly attackDecay: number;
  /** reg 0x80+off — Sustain (bits 7-4) | Release (bits 3-0). */
  readonly sustainRelease: number;
  /** reg 0xE0+off — selector de onda, 0..3. */
  readonly waveform: number;
}

/** Un timbre completo: los dos operadores y el 0xC0 del canal. */
export interface OplTimbre {
  readonly bank: number;
  readonly patch: number;
  /**
   * Percusión (bank 127): nota MIDI FIJA a la que suena el timbre, ignorando la del
   * evento. Melódicos (bank 0): 0, y la nota la pone el evento.
   */
  readonly fixedNote: number;
  readonly modulator: OplOperator;
  readonly carrier: OplOperator;
  /**
   * reg 0xC0+ch — Feedback (bits 3-1) | Connection (bit 0: 0 = FM, 1 = aditivo).
   *
   * 🔴 LOS BITS 4/5 (estéreo del OPL3) VIENEN A CERO en el banco, porque es material
   * de la era OPL2 donde no existen. En un OPL3 eso significa SIN SALIDA por ningún
   * canal: quien vuelque este byte tal cual a un OPL3 no oye nada. Encenderlos es
   * trabajo del asignador de voces (`voices.ts`), no del banco: aquí se conserva el
   * byte del fichero SIN TOCAR, que es lo que un lector de formato debe hacer.
   */
  readonly feedbackConnection: number;
}

/** Banco cargado, consultable por (bank, patch). */
export interface MilesOplBank {
  /** Todos los timbres, en el orden del índice del fichero. */
  readonly timbres: readonly OplTimbre[];
  /** Timbre exacto, o `undefined` si el banco no lo trae. */
  get(bank: number, patch: number): OplTimbre | undefined;
  /**
   * Timbre melódico por programa GM. Si el banco no trae ese programa cae al 0
   * (Acoustic Grand): un programa ausente debe sonar raro, no callar la voz.
   */
  melodic(program: number): OplTimbre;
  /** Timbre de percusión por NOTA MIDI (no por programa), o `undefined`. */
  percussion(note: number): OplTimbre | undefined;
}

function clave(bank: number, patch: number): number {
  return (bank << 8) | patch;
}

function leeOperador(bytes: Uint8Array, off: number): OplOperator {
  return {
    amVibEgKsrMult: bytes[off]!,
    kslTl: bytes[off + 1]!,
    attackDecay: bytes[off + 2]!,
    sustainRelease: bytes[off + 3]!,
    waveform: bytes[off + 4]!,
  };
}

/**
 * Lee un banco `.OPL` de Miles. Lanza con un motivo legible ante cualquier byte que
 * no cuadre — un banco corrupto tiene que ser un rojo, no un silencio: el modo de
 * fallo de «devolver lo que se pueda» es una partida entera sin música y sin causa.
 */
export function parseMilesOplBank(bytes: Uint8Array): MilesOplBank {
  if (bytes.length < INDEX_ENTRY_BYTES) {
    throw new Error(
      `banco OPL truncado: ${bytes.length} bytes, no cabe ni una entrada de índice`,
    );
  }
  const dv = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  const timbres: OplTimbre[] = [];
  const porClave = new Map<number, OplTimbre>();

  let pos = 0;
  for (;;) {
    if (pos + 2 > bytes.length) {
      throw new Error(
        `banco OPL sin centinela 0xFFFF: el índice se sale del fichero en ${pos}`,
      );
    }
    if (dv.getUint16(pos, true) === INDEX_TERMINATOR) break;
    if (pos + INDEX_ENTRY_BYTES > bytes.length) {
      throw new Error(`banco OPL truncado: entrada de índice incompleta en ${pos}`);
    }
    const patch = bytes[pos]!;
    const bank = bytes[pos + 1]!;
    const off = dv.getUint32(pos + 2, true);

    if (off + BLOCK_BYTES_2OP > bytes.length) {
      throw new Error(
        `banco OPL: el timbre ${bank}/${patch} apunta a ${off}, fuera del fichero ` +
          `(${bytes.length} bytes)`,
      );
    }
    const size = dv.getUint16(off, true);
    if (size !== BLOCK_BYTES_2OP) {
      // Los bancos de Miles admiten bloques de 4 operadores; `FAT.OPL` no trae ninguno
      // (los 181 miden 14). Se rechaza en vez de adivinar: un bloque de 4 operadores
      // leído como de 2 produce un instrumento plausible y MAL, que es peor que un rojo.
      throw new Error(
        `banco OPL: timbre ${bank}/${patch} declara ${size} bytes; sólo se soportan ` +
          `bloques de 2 operadores (${BLOCK_BYTES_2OP}). ¿Banco de 4 operadores?`,
      );
    }

    const timbre: OplTimbre = {
      bank,
      patch,
      fixedNote: bytes[off + 2]!,
      modulator: leeOperador(bytes, off + 3),
      feedbackConnection: bytes[off + 8]!,
      carrier: leeOperador(bytes, off + 9),
    };
    timbres.push(timbre);
    porClave.set(clave(bank, patch), timbre);
    pos += INDEX_ENTRY_BYTES;
  }

  if (timbres.length === 0) throw new Error("banco OPL vacío: el índice no trae timbres");

  const fallback = porClave.get(clave(MILES_BANK_MELODIC, 0)) ?? timbres[0]!;

  return {
    timbres,
    get: (bank, patch) => porClave.get(clave(bank, patch)),
    melodic: (program) =>
      porClave.get(clave(MILES_BANK_MELODIC, program & 0x7f)) ?? fallback,
    percussion: (note) => porClave.get(clave(MILES_BANK_PERCUSSION, note & 0x7f)),
  };
}
