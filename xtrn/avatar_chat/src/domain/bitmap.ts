export interface BitmapCell {
  ch: string;
  charCode: number;
  fg: number;
  bg: number;
}

export interface ParsedBitmapMessage {
  width: number;
  height: number;
  fromName: string;
  hexData: string;
}

export interface DecodedBitmap {
  bitmap: BitmapCell[];
  width: number;
  height: number;
  actualWidth: number;
  actualHeight: number;
}

export interface BitmapEntry extends DecodedBitmap {
  fromName: string;
  sourceChannel: string;
  time: number;
  isPrivate: boolean;
  isIncomingPrivate: boolean;
}

interface InflateState {
  bytes: number[];
  position: number;
  bitBuffer: number;
  bitCount: number;
}

interface HuffmanTable {
  maxBits: number;
  map: { [key: string]: number };
}

const LENGTH_BASE = [3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258];
const LENGTH_EXTRA = [0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0];
const DISTANCE_BASE = [1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577];
const DISTANCE_EXTRA = [0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13];

export function isBitmapMessage(text: string): boolean {
  return typeof text === "string" && text.indexOf("[BITMAP|") === 0 && text.charAt(text.length - 1) === "]";
}

export function parseBitmapMessage(text: string): ParsedBitmapMessage | null {
  let inner = "";
  let parts: string[] = [];
  let width = 0;
  let height = 0;

  if (!isBitmapMessage(text)) {
    return null;
  }

  inner = text.slice(1, -1);
  parts = inner.split("|");

  if (parts.length !== 5 || parts[0] !== "BITMAP") {
    return null;
  }

  width = parseInt(parts[1] || "", 10) || 0;
  height = parseInt(parts[2] || "", 10) || 0;

  if (width < 1 || height < 1 || !(parts[4] || "").length || ((parts[4] || "").length % 2) !== 0) {
    return null;
  }

  return {
    width: width,
    height: height,
    fromName: parts[3] || "",
    hexData: parts[4] || ""
  };
}

export function decodeBitmap(hexData: string, expectedWidth: number, expectedHeight: number): DecodedBitmap {
  const compressed = hexToBytes(hexData);
  const decompressed = inflateZlib(compressed, 0);
  const bitmap: BitmapCell[] = [];
  const fgs: number[] = [];
  const bgs: number[] = [];
  const chars: number[] = [];
  let dataHeight = 0;
  let dataLength = 0;
  let slicePoint = 0;
  let totalPixels = 0;
  let dataWidth = 0;
  let width = 0;
  let height = 0;
  let actualWidth = 0;
  let actualHeight = 0;
  let index = 0;

  if (decompressed.length < 4) {
    return {
      bitmap: bitmap,
      width: 0,
      height: 0,
      actualWidth: 0,
      actualHeight: 0
    };
  }

  dataHeight = decompressed[0] || 0;
  if (dataHeight < 1) {
    return {
      bitmap: bitmap,
      width: 0,
      height: 0,
      actualWidth: 0,
      actualHeight: 0
    };
  }

  dataLength = decompressed.length - 1;
  slicePoint = Math.floor(dataLength / 3);
  totalPixels = slicePoint;
  dataWidth = Math.floor(totalPixels / dataHeight);

  width = expectedWidth || dataWidth;
  height = expectedHeight || dataHeight;
  if (width * height !== totalPixels) {
    width = dataWidth;
    height = dataHeight;
  }

  actualWidth = dataWidth;
  actualHeight = dataHeight;

  for (index = 0; index < slicePoint; index += 1) {
    fgs.push(decompressed[1 + index] || 0);
    bgs.push(decompressed[1 + slicePoint + index] || 0);
    chars.push(decompressed[1 + slicePoint * 2 + index] || 32);
  }

  for (index = 0; index < totalPixels; index += 1) {
    const charCode = chars[index] || 32;
    let ch = "";

    try {
      ch = ascii(charCode);
    } catch (_error) {
      ch = String.fromCharCode(charCode);
    }

    bitmap.push({
      ch: ch,
      charCode: charCode,
      fg: fgs[index] || 0,
      bg: bgs[index] || 0
    });
  }

  return {
    bitmap: bitmap,
    width: width,
    height: height,
    actualWidth: actualWidth,
    actualHeight: actualHeight
  };
}

function hexToBytes(hex: string): number[] {
  const bytes: number[] = [];
  let index = 0;

  for (index = 0; index < hex.length; index += 2) {
    bytes.push(parseInt(hex.substr(index, 2), 16) || 0);
  }

  return bytes;
}

function createInflateState(bytes: number[], offset: number): InflateState {
  return {
    bytes: bytes,
    position: offset,
    bitBuffer: 0,
    bitCount: 0
  };
}

function readByte(state: InflateState): number {
  const value = state.bytes[state.position];
  state.position += 1;
  return value === undefined ? 0 : value;
}

function readBits(state: InflateState, count: number): number {
  let buffer = state.bitBuffer;
  let available = state.bitCount;
  let out = 0;

  while (available < count) {
    buffer |= readByte(state) << available;
    available += 8;
  }

  out = buffer & ((1 << count) - 1);
  state.bitBuffer = buffer >>> count;
  state.bitCount = available - count;
  return out;
}

function alignByte(state: InflateState): void {
  state.bitBuffer = 0;
  state.bitCount = 0;
}

function reverseBits(value: number, count: number): number {
  let result = 0;
  let index = 0;

  for (index = 0; index < count; index += 1) {
    result = (result << 1) | (value & 1);
    value >>= 1;
  }

  return result;
}

function buildHuffmanTable(codeLengths: number[]): HuffmanTable {
  const table: HuffmanTable = {
    maxBits: 0,
    map: {}
  };
  const counts: number[] = [];
  const nextCodes: number[] = [];
  let index = 0;
  let code = 0;

  for (index = 0; index < codeLengths.length; index += 1) {
    if ((codeLengths[index] || 0) > table.maxBits) {
      table.maxBits = codeLengths[index] || 0;
    }
  }

  for (index = 0; index <= table.maxBits; index += 1) {
    counts[index] = 0;
  }

  for (index = 0; index < codeLengths.length; index += 1) {
    counts[codeLengths[index] || 0] = (counts[codeLengths[index] || 0] || 0) + 1;
  }

  counts[0] = 0;
  for (index = 1; index <= table.maxBits; index += 1) {
    code = (code + (counts[index - 1] || 0)) << 1;
    nextCodes[index] = code;
  }

  for (index = 0; index < codeLengths.length; index += 1) {
    const length = codeLengths[index] || 0;
    if (!length) {
      continue;
    }
    const nextCode = nextCodes[length] || 0;
    const key = String(reverseBits(nextCode, length) | (length << 16));
    table.map[key] = index;
    nextCodes[length] = nextCode + 1;
  }

  return table;
}

function readHuffmanCode(table: HuffmanTable, state: InflateState): number {
  let code = 0;
  let length = 0;

  for (length = 1; length <= table.maxBits; length += 1) {
    code |= readBits(state, 1) << (length - 1);
    if (table.map[String(code | (length << 16))] !== undefined) {
      return table.map[String(code | (length << 16))] || 0;
    }
  }

  throw new Error("Huffman decode failed");
}

function buildFixedLiteralTable(): HuffmanTable {
  const lengths: number[] = [];
  let index = 0;

  for (index = 0; index <= 287; index += 1) {
    lengths[index] = 0;
  }
  for (index = 0; index <= 143; index += 1) {
    lengths[index] = 8;
  }
  for (index = 144; index <= 255; index += 1) {
    lengths[index] = 9;
  }
  for (index = 256; index <= 279; index += 1) {
    lengths[index] = 7;
  }
  for (index = 280; index <= 287; index += 1) {
    lengths[index] = 8;
  }

  return buildHuffmanTable(lengths);
}

function buildFixedDistanceTable(): HuffmanTable {
  const lengths: number[] = [];
  let index = 0;

  for (index = 0; index < 32; index += 1) {
    lengths[index] = 5;
  }

  return buildHuffmanTable(lengths);
}

function decodeDynamicTables(state: InflateState): { lit: HuffmanTable; dist: HuffmanTable } {
  const hlit = readBits(state, 5) + 257;
  const hdist = readBits(state, 5) + 1;
  const hclen = readBits(state, 4) + 4;
  const order = [16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15];
  const codeLengths: number[] = [];
  let index = 0;

  for (index = 0; index < 19; index += 1) {
    codeLengths[index] = 0;
  }

  for (index = 0; index < hclen; index += 1) {
    codeLengths[order[index] || 0] = readBits(state, 3);
  }

  const codeTable = buildHuffmanTable(codeLengths);

  function readLengths(count: number): number[] {
    const out: number[] = [];
    let previous = 0;

    while (out.length < count) {
      const symbol = readHuffmanCode(codeTable, state);
      let repeat = 0;
      let repeatIndex = 0;

      if (symbol <= 15) {
        out.push(symbol);
        previous = symbol;
        continue;
      }

      if (symbol === 16) {
        repeat = 3 + readBits(state, 2);
        for (repeatIndex = 0; repeatIndex < repeat; repeatIndex += 1) {
          out.push(previous);
        }
        continue;
      }

      if (symbol === 17) {
        repeat = 3 + readBits(state, 3);
        previous = 0;
        for (repeatIndex = 0; repeatIndex < repeat; repeatIndex += 1) {
          out.push(0);
        }
        continue;
      }

      if (symbol === 18) {
        repeat = 11 + readBits(state, 7);
        previous = 0;
        for (repeatIndex = 0; repeatIndex < repeat; repeatIndex += 1) {
          out.push(0);
        }
        continue;
      }

      throw new Error("Bad RLE in code lengths");
    }

    return out;
  }

  return {
    lit: buildHuffmanTable(readLengths(hlit)),
    dist: buildHuffmanTable(readLengths(hdist))
  };
}

function inflateRaw(bytes: number[], start: number): number[] {
  const state = createInflateState(bytes, start);
  const output: number[] = [];
  const fixedLiterals = buildFixedLiteralTable();
  const fixedDistances = buildFixedDistanceTable();
  let done = false;

  while (!done) {
    const isFinal = readBits(state, 1);
    const blockType = readBits(state, 2);
    let literalTable: HuffmanTable | null = null;
    let distanceTable: HuffmanTable | null = null;

    if (blockType === 0) {
      let length = 0;
      let notLength = 0;
      let index = 0;

      alignByte(state);
      length = readByte(state) | (readByte(state) << 8);
      notLength = readByte(state) | (readByte(state) << 8);
      if ((length ^ 65535) !== notLength) {
        throw new Error("Stored block length mismatch");
      }
      for (index = 0; index < length; index += 1) {
        output.push(readByte(state));
      }
    } else {
      if (blockType === 1) {
        literalTable = fixedLiterals;
        distanceTable = fixedDistances;
      } else if (blockType === 2) {
        const tables = decodeDynamicTables(state);
        literalTable = tables.lit;
        distanceTable = tables.dist;
      } else {
        throw new Error("Invalid DEFLATE block type");
      }

      while (literalTable && distanceTable) {
        const symbol = readHuffmanCode(literalTable, state);
        if (symbol < 256) {
          output.push(symbol);
          continue;
        }
        if (symbol === 256) {
          break;
        }

        const lengthIndex = symbol - 257;
        const length = (LENGTH_BASE[lengthIndex] || 0) + ((LENGTH_EXTRA[lengthIndex] || 0) ? readBits(state, LENGTH_EXTRA[lengthIndex] || 0) : 0);
        const distanceSymbol = readHuffmanCode(distanceTable, state);
        const distance = (DISTANCE_BASE[distanceSymbol] || 0) + ((DISTANCE_EXTRA[distanceSymbol] || 0) ? readBits(state, DISTANCE_EXTRA[distanceSymbol] || 0) : 0);
        const base = output.length - distance;
        let copyIndex = 0;

        if (base < 0) {
          throw new Error("Invalid DEFLATE distance");
        }

        for (copyIndex = 0; copyIndex < length; copyIndex += 1) {
          output.push(output[base + copyIndex] || 0);
        }
      }
    }

    if (isFinal) {
      done = true;
    }
  }

  return output;
}

function inflateZlib(bytes: number[], offset: number): number[] {
  let position = offset || 0;
  const cmf = bytes[position] || 0;
  const flg = bytes[position + 1] || 0;

  position += 2;
  if ((cmf & 15) !== 8) {
    throw new Error("Unsupported zlib compression method");
  }
  if (flg & 32) {
    position += 4;
  }

  return inflateRaw(bytes, position);
}
