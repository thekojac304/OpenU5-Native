export const ALPHA_SIGN_HEADER_BYTES = 16;
export const ALPHA_SIGN_RECORD_BYTES = 24;

export interface AlphaSignSource {
  location: number;
  floor: number;
  x: number;
  y: number;
  text: string;
  raw: number[];
}

export interface AlphaSignRecord extends AlphaSignSource {
  textOffset: number;
  textLength: number;
  rawOffset: number;
  rawLength: number;
}

function unsignedByte(name: string, value: number): number {
  if (!Number.isInteger(value) || value < 0 || value > 0xff) {
    throw new Error(`${name} must be an unsigned byte; got ${value}`);
  }
  return value;
}

function signedWord(name: string, value: number): number {
  if (!Number.isInteger(value) || value < -0x8000 || value > 0x7fff) {
    throw new Error(`${name} must be a signed 16-bit integer; got ${value}`);
  }
  return value;
}

/**
 * Compact, pointer-free sign table for the Alpha resource pack.
 *
 * Header: count, record size, UTF-8 text-pool size, raw-glyph-pool size (u32 LE).
 * Each 24-byte record stores location/u8, floor/i16, x/y/u8 and offset+length
 * pairs into the two pools. Text entries include a trailing NUL which is not
 * counted in textLength. Raw entries are the authoritative SIGNS.DAT glyph bytes.
 */
export function buildAlphaSignData(source: readonly AlphaSignSource[]): Buffer {
  const texts: Buffer[] = [];
  const raws: Buffer[] = [];
  const records: AlphaSignRecord[] = [];
  let textBytes = 0;
  let rawBytes = 0;

  source.forEach((sign, index) => {
    const location = unsignedByte(`sign ${index} location`, sign.location);
    const floor = signedWord(`sign ${index} floor`, sign.floor);
    const x = unsignedByte(`sign ${index} x`, sign.x);
    const y = unsignedByte(`sign ${index} y`, sign.y);
    if (sign.text.includes("\0")) throw new Error(`sign ${index} text contains NUL`);
    const text = Buffer.from(`${sign.text}\0`, "utf8");
    const raw = Buffer.from(sign.raw.map((value, rawIndex) =>
      unsignedByte(`sign ${index} raw ${rawIndex}`, value)));
    records.push({
      location, floor, x, y, text: sign.text, raw: [...sign.raw],
      textOffset: textBytes, textLength: text.length - 1,
      rawOffset: rawBytes, rawLength: raw.length,
    });
    texts.push(text);
    raws.push(raw);
    textBytes += text.length;
    rawBytes += raw.length;
  });

  const directory = Buffer.alloc(ALPHA_SIGN_HEADER_BYTES + records.length * ALPHA_SIGN_RECORD_BYTES);
  directory.writeUInt32LE(records.length, 0);
  directory.writeUInt32LE(ALPHA_SIGN_RECORD_BYTES, 4);
  directory.writeUInt32LE(textBytes, 8);
  directory.writeUInt32LE(rawBytes, 12);
  records.forEach((sign, index) => {
    const at = ALPHA_SIGN_HEADER_BYTES + index * ALPHA_SIGN_RECORD_BYTES;
    directory.writeUInt8(sign.location, at);
    directory.writeInt16LE(sign.floor, at + 2);
    directory.writeUInt8(sign.x, at + 4);
    directory.writeUInt8(sign.y, at + 5);
    directory.writeUInt32LE(sign.textOffset, at + 8);
    directory.writeUInt32LE(sign.textLength, at + 12);
    directory.writeUInt32LE(sign.rawOffset, at + 16);
    directory.writeUInt32LE(sign.rawLength, at + 20);
  });
  return Buffer.concat([directory, ...texts, ...raws]);
}

/** Decode helper used by pack regression tests and offline inspection. */
export function inspectAlphaSignData(data: Uint8Array): AlphaSignRecord[] {
  const bytes = Buffer.from(data.buffer, data.byteOffset, data.byteLength);
  if (bytes.length < ALPHA_SIGN_HEADER_BYTES) throw new Error("sign resource header is truncated");
  const count = bytes.readUInt32LE(0);
  const recordBytes = bytes.readUInt32LE(4);
  const textBytes = bytes.readUInt32LE(8);
  const rawBytes = bytes.readUInt32LE(12);
  if (recordBytes !== ALPHA_SIGN_RECORD_BYTES) throw new Error(`unsupported sign record size ${recordBytes}`);
  const textBase = ALPHA_SIGN_HEADER_BYTES + count * recordBytes;
  const rawBase = textBase + textBytes;
  if (rawBase + rawBytes !== bytes.length) throw new Error("sign resource length does not match its pools");

  const result: AlphaSignRecord[] = [];
  for (let index = 0; index < count; index++) {
    const at = ALPHA_SIGN_HEADER_BYTES + index * recordBytes;
    const textOffset = bytes.readUInt32LE(at + 8);
    const textLength = bytes.readUInt32LE(at + 12);
    const rawOffset = bytes.readUInt32LE(at + 16);
    const rawLength = bytes.readUInt32LE(at + 20);
    if (textOffset + textLength >= textBytes || bytes[textBase + textOffset + textLength] !== 0) {
      throw new Error(`sign ${index} has an invalid text slice`);
    }
    if (rawOffset + rawLength > rawBytes) throw new Error(`sign ${index} has an invalid raw slice`);
    result.push({
      location: bytes.readUInt8(at), floor: bytes.readInt16LE(at + 2),
      x: bytes.readUInt8(at + 4), y: bytes.readUInt8(at + 5),
      textOffset, textLength, rawOffset, rawLength,
      text: bytes.toString("utf8", textBase + textOffset, textBase + textOffset + textLength),
      raw: [...bytes.subarray(rawBase + rawOffset, rawBase + rawOffset + rawLength)],
    });
  }
  return result;
}
