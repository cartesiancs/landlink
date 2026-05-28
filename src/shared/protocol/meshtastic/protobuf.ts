// Hand-rolled protobuf primitives sufficient for the Meshtastic message
// subset we need. No protobuf-js dependency: the wire format is stable and
// we only touch a handful of message types, so the upkeep is cheaper than a
// fully generic codec.
//
// Wire types:
//   0  VARINT     (uint32/uint64/int32/int64/bool/enum)
//   1  FIXED64    (fixed64/sfixed64/double)
//   2  LEN        (string/bytes/message)
//   5  FIXED32    (fixed32/sfixed32/float)

export const WIRE_VARINT = 0 as const;
export const WIRE_FIXED64 = 1 as const;
export const WIRE_LEN = 2 as const;
export const WIRE_FIXED32 = 5 as const;

// ---------------------------------------------------------------------------
// Reader
// ---------------------------------------------------------------------------

export class PbReader {
  private readonly buf: Uint8Array;
  private off: number;
  private readonly end: number;

  constructor(buf: Uint8Array, offset = 0, length = buf.byteLength - offset) {
    this.buf = buf;
    this.off = offset;
    this.end = offset + length;
  }

  eof(): boolean {
    return this.off >= this.end;
  }

  remaining(): number {
    return this.end - this.off;
  }

  readVarint(): number {
    let v = 0;
    let shift = 0;
    while (this.off < this.end) {
      const b = this.buf[this.off++] ?? 0;
      v |= (b & 0x7f) << shift;
      if ((b & 0x80) === 0) return v >>> 0;
      shift += 7;
      if (shift >= 35) {
        // Drain the rest of the varint without overflowing JS Number. We
        // only care about uint32-sized fields; higher-order bits get
        // discarded. Continues until the high bit clears.
        while (this.off < this.end) {
          if ((this.buf[this.off++]! & 0x80) === 0) return v >>> 0;
        }
        return v >>> 0;
      }
    }
    throw new Error("varint: unexpected EOF");
  }

  readBool(): boolean {
    return this.readVarint() !== 0;
  }

  readFixed32(): number {
    if (this.off + 4 > this.end) throw new Error("fixed32: EOF");
    const b0 = this.buf[this.off++]!;
    const b1 = this.buf[this.off++]!;
    const b2 = this.buf[this.off++]!;
    const b3 = this.buf[this.off++]!;
    return (b0 | (b1 << 8) | (b2 << 16) | (b3 << 24)) >>> 0;
  }

  readSFixed32(): number {
    const u = this.readFixed32();
    return u | 0;
  }

  readFloat(): number {
    if (this.off + 4 > this.end) throw new Error("float: EOF");
    const dv = new DataView(
      this.buf.buffer,
      this.buf.byteOffset + this.off,
      4,
    );
    const v = dv.getFloat32(0, true);
    this.off += 4;
    return v;
  }

  readBytes(): Uint8Array {
    const len = this.readVarint();
    if (this.off + len > this.end) throw new Error("bytes: EOF");
    const out = this.buf.slice(this.off, this.off + len);
    this.off += len;
    return out;
  }

  readString(): string {
    const bytes = this.readBytes();
    return new TextDecoder().decode(bytes);
  }

  // Returns a reader scoped to the next length-delimited submessage, advancing
  // the parent past it. Useful for nested message decoding.
  readMessage(): PbReader {
    const len = this.readVarint();
    if (this.off + len > this.end) throw new Error("message: EOF");
    const sub = new PbReader(this.buf, this.off, len);
    this.off += len;
    return sub;
  }

  skipField(wireType: number): void {
    if (wireType === WIRE_VARINT) {
      this.readVarint();
    } else if (wireType === WIRE_FIXED64) {
      if (this.off + 8 > this.end) throw new Error("fixed64 skip: EOF");
      this.off += 8;
    } else if (wireType === WIRE_LEN) {
      const len = this.readVarint();
      if (this.off + len > this.end) throw new Error("len skip: EOF");
      this.off += len;
    } else if (wireType === WIRE_FIXED32) {
      if (this.off + 4 > this.end) throw new Error("fixed32 skip: EOF");
      this.off += 4;
    } else {
      throw new Error(`unknown wire type ${wireType.toString()}`);
    }
  }
}

// ---------------------------------------------------------------------------
// Writer
// ---------------------------------------------------------------------------

export class PbWriter {
  // We append into a growing list of chunks to avoid quadratic copies, then
  // assemble at the end. For our message sizes (<256B typical) a single
  // Uint8Array with a grow strategy would be fine too — using chunks keeps
  // the API simple for nested messages.
  private chunks: number[] = [];

  size(): number {
    return this.chunks.length;
  }

  finish(): Uint8Array {
    return new Uint8Array(this.chunks);
  }

  writeTag(fieldNumber: number, wireType: number): void {
    this.writeVarint(((fieldNumber << 3) | wireType) >>> 0);
  }

  writeVarint(v: number): void {
    let n = v >>> 0;
    while (n >= 0x80) {
      this.chunks.push((n & 0x7f) | 0x80);
      n >>>= 7;
    }
    this.chunks.push(n & 0x7f);
  }

  writeUint32(fieldNumber: number, v: number): void {
    this.writeTag(fieldNumber, WIRE_VARINT);
    this.writeVarint(v);
  }

  writeBool(fieldNumber: number, v: boolean): void {
    this.writeTag(fieldNumber, WIRE_VARINT);
    this.writeVarint(v ? 1 : 0);
  }

  writeFixed32(fieldNumber: number, v: number): void {
    this.writeTag(fieldNumber, WIRE_FIXED32);
    const u = v >>> 0;
    this.chunks.push(u & 0xff, (u >>> 8) & 0xff, (u >>> 16) & 0xff, (u >>> 24) & 0xff);
  }

  writeBytes(fieldNumber: number, bytes: Uint8Array): void {
    this.writeTag(fieldNumber, WIRE_LEN);
    this.writeVarint(bytes.byteLength);
    for (let i = 0; i < bytes.byteLength; i++) {
      this.chunks.push(bytes[i] ?? 0);
    }
  }

  writeString(fieldNumber: number, s: string): void {
    this.writeBytes(fieldNumber, new TextEncoder().encode(s));
  }

  writeMessage(fieldNumber: number, encode: (sub: PbWriter) => void): void {
    const sub = new PbWriter();
    encode(sub);
    this.writeBytes(fieldNumber, sub.finish());
  }
}

// Convenience helper for the common pattern: decode a top-level message that
// dispatches on (fieldNumber, wireType).
export function readFields(
  reader: PbReader,
  handler: (fieldNumber: number, wireType: number, r: PbReader) => boolean,
): void {
  while (!reader.eof()) {
    const tag = reader.readVarint();
    const fieldNumber = tag >>> 3;
    const wireType = tag & 0x07;
    const handled = handler(fieldNumber, wireType, reader);
    if (!handled) reader.skipField(wireType);
  }
}
