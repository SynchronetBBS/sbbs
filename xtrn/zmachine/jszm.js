/*
  JSZM - JavaScript implementation of Z-machine (Z-code version 3)
  This program is in public domain.

  ES5 / SpiderMonkey 1.8.5 (Synchronet) port of zzo38's JSZM v2.0.2.
  This port is SYNCHRONOUS: run() executes straight through to QUIT, and the
  user-supplied callbacks are plain functions (NOT generators):

  .print(text,scripting)  - print text; return value unused.
  .read(maxlen)           - RETURN the player's input string (truncated to maxlen),
                            or null to abort run() cleanly (e.g. disconnect/terminate).
  .save(buf)              - RETURN true on success, false on failure (buf is Uint8Array).
  .restore()              - RETURN a Uint8Array (same bytes passed to save) or false/null.
  .highlight(fixpitch)    - update highlight mode (optional).
  .restarted()            - called after (re)init, before execution (optional).
  .updateStatusLine(text,v18,v17) - draw status line (optional); see statusType.
  .split(height)          - SPLIT opcode (optional; set with screen to enable split).
  .screen(window)         - SCREEN opcode (optional; set with split to enable split).
  .setCursor(row,col)     - v4+ @set_cursor (optional; upper-window cursor).
  .getCursor()            - v4+ @get_cursor (optional; returns {row,col}).
  .eraseWindow(win)       - v4+ @erase_window (optional).
  .eraseLine(val)         - v4+ @erase_line (optional).
  .bufferMode(flag)       - v4+ @buffer_mode (optional; lower-window word-wrap).
  .run()                  - run from the beginning to QUIT.
  .verify()               - verify the story file (normal function).

  Properties: isTandy, statusType, serial, zorkid, seed (as in upstream).
*/

"use strict";

// ---- Polyfills (guarded) ----
if (typeof Math.imul !== "function") {
  Math.imul = function(a, b) {
    a = a | 0; b = b | 0;
    var ah = (a >>> 16) & 0xffff, al = a & 0xffff;
    var bh = (b >>> 16) & 0xffff, bl = b & 0xffff;
    return (al * bl + (((ah * bl + al * bh) << 16) >>> 0)) | 0;
  };
}
if (typeof Math.trunc !== "function") {
  Math.trunc = function(x) { return x < 0 ? Math.ceil(x) : Math.floor(x); };
}

// ---- Internal helpers (jszm_* prefix) ----
function jszm_bytesToString(arr, start, end) {
  var s = "", i;
  for (i = start; i < end; i++) s += String.fromCharCode(arr[i]);
  return s;
}
// Vocabulary as a plain object with a prefix (collision-safe vs Object.prototype names).
function jszm_vocabSet(map, word, addr) { map["$" + word] = addr; }
function jszm_vocabGet(map, word) { var v = map["$" + word]; return v === undefined ? 0 : v; }

var JSZM_Version = { major: 2, minor: 0, subminor: 2, timestamp: 1480624305074 };

// Z-spec 1.1 §3.8.5.3 default Unicode translation table: ZSCII 155..223 -> code point.
var JSZM_DEFAULT_UNICODE = [
  0x0e4,0x0f6,0x0fc,0x0c4,0x0d6,0x0dc,0x0df,0x0bb,0x0ab,0x0eb,
  0x0ef,0x0ff,0x0cb,0x0cf,0x0e1,0x0e9,0x0ed,0x0f3,0x0fa,0x0fd,
  0x0c1,0x0c9,0x0cd,0x0d3,0x0da,0x0dd,0x0e0,0x0e8,0x0ec,0x0f2,
  0x0f9,0x0c0,0x0c8,0x0cc,0x0d2,0x0d9,0x0e2,0x0ea,0x0ee,0x0f4,
  0x0fb,0x0c2,0x0ca,0x0ce,0x0d4,0x0db,0x0e5,0x0c5,0x0f8,0x0d8,
  0x0e3,0x0f1,0x0f5,0x0c3,0x0d1,0x0d5,0x0e6,0x0c6,0x0e7,0x0c7,
  0x0fe,0x0f0,0x0de,0x0d0,0x0a3,0x153,0x152,0x0a1,0x0bf
];

function JSZM(arr) {
  var mem;
  mem = this.memInit = new Uint8Array(arr);
  this.version = mem[0];
  this.vp = JSZM.versionParams(this.version);
  if (!this.vp) throw new Error("Unsupported Z-code version: " + this.version);
  this.byteSwapped = !!(mem[1] & 1);
  this.statusType = !!(mem[1] & 2);
  this.serial = jszm_bytesToString(mem, 18, 24);
  this.zorkid = (mem[2] << (this.byteSwapped ? 0 : 8)) | (mem[3] << (this.byteSwapped ? 8 : 0));
  this.routineOffset = 0; this.stringOffset = 0;
  if (this.version === 6) {
    this.routineOffset = this.byteSwapped ? (mem[0x29] << 8) | mem[0x28] : (mem[0x28] << 8) | mem[0x29];
    this.stringOffset  = this.byteSwapped ? (mem[0x2B] << 8) | mem[0x2A] : (mem[0x2A] << 8) | mem[0x2B];
  }
}

// Per-version structural parameters. objectsAdj = 2 + defProps*2 - objEntry, so
// (defprop + objectsAdj) + obj*objEntry addresses a 1-based object entry.
JSZM.versionParams = function (v) {
  if (v === 3)            return { addrShift: 1, objEntry: 9,  attrBytes: 4, objNumWidth: 1, defProps: 31, objectsAdj: 55,  lenDiv: 2 };
  if (v === 4 || v === 5) return { addrShift: 2, objEntry: 14, attrBytes: 6, objNumWidth: 2, defProps: 63, objectsAdj: 114, lenDiv: 4 };
  if (v === 6)            return { addrShift: 2, objEntry: 14, attrBytes: 6, objNumWidth: 2, defProps: 63, objectsAdj: 114, lenDiv: 8, splitPacked: true };
  if (v === 8)            return { addrShift: 3, objEntry: 14, attrBytes: 6, objNumWidth: 2, defProps: 63, objectsAdj: 114, lenDiv: 8 };
  return null;
};

JSZM.prototype = {
  byteSwapped: false,
  constructor: JSZM,
  version: 3,
  vp: null,
  routineOffset: 0,    // v6/v7 packed-routine base (header 0x28, words); 0 for v3/4/5/8
  stringOffset: 0,     // v6/v7 packed-string base (header 0x2A, words); 0 for v3/4/5/8
  windows: null,       // v6: array of 8 windows, each an 18-element property array (Z-spec §15); null until init
  curwin: 0,           // v6: currently selected window (set_window)
  deserialize: function (ar) {
    try {
      var r = quetzal.read(ar, this.memInit, this.getu(14));
      if (!r) return null;
      var m = this.memInit;                                 // story-binding check
      if (r.release !== (((m[2] & 255) << 8) | (m[3] & 255))) return null;
      if (r.checksum !== (((m[0x1C] & 255) << 8) | (m[0x1D] & 255))) return null;
      for (var i = 0; i < 6; i++) if ((r.serial[i] & 255) !== (m[0x12 + i] & 255)) return null;
      this.mem.set(r.dyn);                                  // restore dynamic memory
      return [r.ds, r.cs, r.pc];
    } catch (ex) { return null; }
  },
  endText: 0,
  fwords: null,
  unicode: null,       // ZSCII 155.. -> Unicode code point (story table or DEFAULT); null until init()
  alphabet: null,      // v5 custom alphabet table: 78 ZSCII codes (A0,A1,A2 x 26); null = built-in
  checkUnicode: null,   // (cp) -> bits: 1=printable, 2=receivable (door fills from console.charset)
  // Map a ZSCII output code to a JS code point. 32-126 are ASCII; 155.. are the "extra
  // characters" looked up in the Unicode translation table; anything else passes through.
  zsciiToUnicode: function (z) {
    if (z >= 155 && this.unicode && (z - 155) < this.unicode.length) return this.unicode[z - 155];
    return z;
  },
  ostream3: null,      // active @output_stream 3 redirect stack (array of {table, count}); null = none
  ostream1: true,      // @output_stream 1 (the screen): deselectable in v3+ (z-spec 7.1.2). Arthur (v6) turns
                       // it OFF to echo the typed command to the transcript only -- with it off we draw nothing.
  genPrint: function(text) {
    var st = this.ostream3;
    if (st && st.length) {                       // stream 3 active: capture, do NOT draw
      var top = st[st.length - 1];
      for (var i = 0; i < text.length; i++) {
        this.mem[(top.table + 2 + top.count) & 0xffff] = text.charCodeAt(i) & 0xff;
        top.count++;
      }
      this.put(top.table, top.count);            // keep the length word current
      return;
    }
    if (this.ostream1 === false) return;          // screen (stream 1) deselected via @output_stream -1: draw nothing
    this.__printed = true;                        // real screen output (for timed-read line redisplay)
    var x = this.get(16);
    if (x != this.savedFlags) {
      this.savedFlags = x;
      this.highlight(!!(x & 2));
    }
    this.print(text, !!(x & 1));
  },
  get: function(x) {
    var m = this.mem, v;
    if (this.byteSwapped) v = (m[x + 1] << 8) | m[x];
    else v = (m[x] << 8) | m[x + 1];
    return v & 0x8000 ? v - 0x10000 : v;
  },
  getText: function(addr) {
    var self = this;
    var o = "", ps = 0, ts = 0, w, y;
    var d = function(v) {
      if (ts == 3) {
        y = v << 5;
        ts = 4;
      } else if (ts == 4) {
        y += v;
        if (y == 13) o += "\n";
        else if (y) o += String.fromCharCode(self.zsciiToUnicode(y));
        ts = ps;
      } else if (ts == 5) {
        o += self.getText(self.getu(self.fwords + (y + v) * 2) * 2);
        ts = ps;
      } else if (v == 0) {
        o += " ";
      } else if (v < 4) {
        ts = 5;
        y = (v - 1) * 32;
      } else if (v < 6) {
        if (!ts) ts = v - 3;
        else if (ts == v - 3) ps = ts;
        else ps = ts = 0;
      } else if (v == 6 && ts == 2) {
        ts = 3;
      } else if (self.alphabet) {
        // Custom alphabet table: 78 ZSCII codes (A0,A1,A2 each 26, for Z-chars 6..31).
        o += String.fromCharCode(self.zsciiToUnicode(self.alphabet[ts * 26 + (v - 6)]));
        ts = ps;
      } else {
        o += "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ*\n0123456789.,!?_#'\"/\\-:()"[ts * 26 + v - 6];
        ts = ps;
      }
    };
    for (;;) {
      if (addr >= self.mem.length) break;   // defensive: a bad/garbage string address must never
                                            // decode past loaded memory (reads return 0 = endless
                                            // spaces with no terminator -> unbounded string -> OOM).
      w = self.getu(addr);
      addr += 2;
      d((w >> 10) & 31);
      d((w >> 5) & 31);
      d(w & 31);
      if (w & 32768) break;
    }
    self.endText = addr;
    return o;
  },
  getu: function(x) {
    var m = this.mem;
    return this.byteSwapped ? (m[x + 1] << 8) | m[x] : (m[x] << 8) | m[x + 1];
  },
  // Encode `len` ZSCII chars at src+from into the fixed-size dictionary-word form at dest:
  // 6 Z-chars / 2 words (v1-3) or 9 Z-chars / 3 words (v4+), padded with shift-char 5,
  // top bit set on the final word. Used by the encode_text opcode and word lookups.
  encodeText: function(src, len, from, dest) {
    var alpha = "abcdefghijklmnopqrstuvwxyz" +     // A0 (Z-chars 6..31)
                "ABCDEFGHIJKLMNOPQRSTUVWXYZ" +     // A1
                "\x01\x020123456789.,!?_#'\"/\\-:()"; // A2: idx 52/53 = Z6 escape / Z7 newline placeholders; '0' at idx 54 -> Z-char 8
    var nZ = this.version >= 4 ? 9 : 6, zc = [], i, c, k, idx;
    for (i = 0; i < len && zc.length < nZ; i++) {
      c = String.fromCharCode(this.mem[(src + from + i) & 65535]).toLowerCase();
      if (c === " ") { zc.push(0); continue; }
      idx = alpha.indexOf(c);
      if (idx >= 0 && idx < 26) zc.push(idx + 6);                 // A0
      else if (idx >= 54) { zc.push(5); zc.push((idx - 52) + 6); } // A2 shift then char
      else {
        // Not in A0/A2: emit ZSCII escape (A2 char 6 = 10-bit literal)
        k = this.mem[(src + from + i) & 65535];
        zc.push(5); zc.push(6); zc.push((k >> 5) & 31); zc.push(k & 31);
      }
    }
    while (zc.length < nZ) zc.push(5);                            // pad with 5s
    if (zc.length > nZ) zc.length = nZ;
    for (i = 0; i < nZ; i += 3) {
      var w = (zc[i] << 10) | (zc[i + 1] << 5) | zc[i + 2];
      if (i + 3 >= nZ) w |= 0x8000;                              // terminator on last word
      this.put((dest + (i / 3) * 2) & 65535, w);
    }
  },
  handleInput: function(str, t1, t2) {
    // v3/v4 sread: text buffer byte 0 = max len; chars stored from byte 1, zero-terminated.
    var i;
    str = str.toLowerCase().slice(0, this.mem[t1] - 1);
    for (i = 0; i < str.length; i++) this.mem[t1 + i + 1] = str.charCodeAt(i);
    this.mem[t1 + str.length + 1] = 0;
    if (t2) this.tokeniseBuffer(str, t2, 0, 0, 1);
  },
  // Lexical analysis shared by sread/aread and the tokenise opcode.
  //   str       = the typed text, lowercased (no buffer-header bytes)
  //   t2        = parse buffer address (byte0=max tokens, byte1=count, 4 bytes/token)
  //   dict      = dictionary address (0 = story's main dictionary)
  //   flag      = if nonzero, do NOT overwrite a parse slot whose word is unrecognised
  //   posBase   = byte offset of str's first char within the text buffer (1 for v3, 2 for v5)
  tokeniseBuffer: function(str, t2, dict, flag, posBase) {
    var self = this, br, i, maxtok = this.mem[t2];
    // Truncate a token to the dictionary's Z-char resolution: 6 Z-chars in v1-3, 9 in v4+.
    // (Capping at 6 for v4+ mis-truncated words >6 Z-chars -- e.g. "restart"->"restar",
    // "examine"->"examin" -- so they failed dictionary lookup.)
    var zcap = self.version >= 4 ? 10 : 7;
    var w = function(x) {
      var j = 0, out = "", k, ch, weight;
      for (k = 0; k < x.length; k++) {
        ch = x.charAt(k);
        weight = /[a-z]/.test(ch) ? 1 : /[0-9.,!?_#'"\/\\:\-()]/.test(ch) ? 2 : 4;
        j += weight;
        if (j < zcap) out += ch;
      }
      return out;
    };
    br = [];
    str.replace(this.regBreak, function(m, o) {
      br.push([m.length, self.dictLookup(w(m), dict), o + posBase]);
      return m;
    });
    if (br.length > maxtok) br.length = maxtok;
    this.mem[t2 + 1] = br.length;
    for (i = 0; i < br.length; i++) {
      if (flag && br[i][1] === 0) continue;   // leave existing slot untouched for unknown words
      this.putu(t2 + i * 4 + 2, br[i][1]);
      this.mem[t2 + i * 4 + 4] = br[i][0];
      this.mem[t2 + i * 4 + 5] = br[i][2];
    }
  },
  // Look up a (weight-truncated) lexed word; return its dictionary entry address (0 if absent).
  // dict==0 uses the cached main-dictionary map; a non-zero dict is searched directly.
  dictLookup: function(word, dict) {
    if (!dict) return jszm_vocabGet(this.vocabulary, word);
    var n = this.mem[dict], e, count, s, i;
    s = dict + 1 + n;                 // skip word-separator list
    e = this.mem[s++];                // entry length
    count = this.get(s); s += 2;      // entry count (negative => unsorted, |count| entries)
    if (count < 0) count = -count;
    for (i = 0; i < count; i++) {
      if (this.getText(s) === word) return s;
      s += e;
    }
    return 0;
  },
  highlight: function() {},
  setColour: null,     // (fg, bg) -> door updates colour state; null = ignore
  setTextStyle: null,  // (style)  -> door updates style state; null = ignore
  readTimedKey: null,  // (tenths) -> ZSCII code (>=0) | negative (timeout) | null (disconnect); null = no timed input
  echoInput: null,     // (x) -> echo on the timed read path: number=key (8=erase), string=re-show; null = no echo
  readPrompt: null,    // () -> door prepares a timed-input prompt (reset pager + repaint status), like cur.read()
  afterInterrupt: null,// (typedSoFar) -> door refresh after a timed interrupt printed: repaint status (live
                       //   clock), or re-show the in-progress input if the interrupt scrolled the lower window
  graphicsAvailable: null,  // () -> bool: door can display pictures (v6 Flags2 bit 3)
  sound: null,          // (number, effect, volume, repeats, chained) -> door plays/stops a
                        //   sound (@sound_effect); null = silent. Presence sets Flags2 bit 7
                        //   (v5+). chained = started inside a completion routine: queue it
                        //   behind the playing sound instead of replacing it.
  windowChanged: null,  // (win) -> door re-reads window geometry after move/size/style/margins
  scrollWindow: null,   // (win, pixels) -> door scrolls the window contents (v6; pixels signed, +up)
  setFont: null,        // (win, font) -> door reports/sets font; null -> engine reports 0 (no change)
  drawPicture: null,    // (pic, x, y, win) -> door blits the picture (v6)
  erasePicture: null,   // (pic, x, y, win) -> door clears the picture region (v6)
  pictureInfo: null,    // (pic) -> {width,height} | null  (manifest lookup; null = no such picture)
  screenWidthPx: 0,         // v6 screen width in pixels  (door-supplied; 0 = derive)
  screenHeightPx: 0,        // v6 screen height in pixels (door-supplied; 0 = derive)
  fontWidth: 0,             // v6 font cell width in pixels  (door-supplied; 0 -> 1)
  fontHeight: 0,            // v6 font cell height in pixels (door-supplied; 0 -> 1)
  isTandy: false,
  mem: null,
  memInit: null,
  parseVocab: function(s) {
    this.vocabulary = {};
    if (s === 0) {                                  // no main dictionary: default separators
      this.regBreak = new RegExp("[^ \\n\\t]+", "g");
      return;
    }
    var e, n, i, seps, parts, x;
    n = this.mem[s++];
    seps = this.selfInsertingBreaks = jszm_bytesToString(this.mem, s, s + n);
    parts = "";
    for (i = 0; i < seps.length; i++) {
      x = seps.charAt(i);
      parts += (x.toUpperCase() == x.toLowerCase() ? "" : "\\") + x;
    }
    e = parts + "]";
    this.regBreak = new RegExp("[" + e + "|[^ \\n\\t" + e + "+", "g");
    s += n;
    e = this.mem[s++];
    n = this.get(s);
    s += 2;
    while (n--) {
      jszm_vocabSet(this.vocabulary, this.getText(s), s);
      s += e;
    }
  },
  print: function() {},
  put: function(x, y) {
    var m = this.mem;
    if (this.byteSwapped) { m[x] = y & 0xff; m[x + 1] = (y >> 8) & 0xff; }
    else { m[x] = (y >> 8) & 0xff; m[x + 1] = y & 0xff; }
  },
  putu: function(x, y) { this.put(x, y & 65535); },
  objAddr: function (o) { return this.objects + o * this.vp.objEntry; },
  unpack: function (x) { return (x & 65535) << this.vp.addrShift; },
  unpackRoutine: function (x) { var a = (x & 65535) << this.vp.addrShift; return this.vp.splitPacked ? a + 8 * this.routineOffset : a; },
  unpackString:  function (x) { var a = (x & 65535) << this.vp.addrShift; return this.vp.splitPacked ? a + 8 * this.stringOffset  : a; },
  v6win: function (w) { w = w & 0xffff; if (w === 0xfffd) return this.curwin; return w & 7; },
  // f: 0=parent, 1=sibling, 2=child
  objField: function (o, f) {
    var a = this.objAddr(o) + this.vp.attrBytes + f * this.vp.objNumWidth;
    return this.vp.objNumWidth === 1 ? this.mem[a] : this.getu(a);
  },
  objSetField: function (o, f, val) {
    var a = this.objAddr(o) + this.vp.attrBytes + f * this.vp.objNumWidth;
    if (this.vp.objNumWidth === 1) this.mem[a] = val & 0xff; else this.put(a, val);
  },
  objPropTable: function (o) {
    return this.getu(this.objAddr(o) + this.vp.attrBytes + 3 * this.vp.objNumWidth);
  },
  firstProp: function (o) { var z = this.objPropTable(o); return z + this.mem[z] * 2 + 1; },
  // returns [propNumber, dataSize, dataAddr, nextHeaderAddr]
  propHeader: function (z) {
    var b = this.mem[z];
    if (this.version >= 4) {
      if (b & 0x80) { var sz = this.mem[z + 1] & 63; if (!sz) sz = 64; return [b & 63, sz, z + 2, z + 2 + sz]; }
      var s = (b & 0x40) ? 2 : 1; return [b & 63, s, z + 1, z + 1 + s];
    }
    var size = (b >> 5) + 1; return [b & 31, size, z + 1, z + 1 + size];
  },
  defProp: function (n) { return this.get(this.defprop + 2 * n); },   // signed, like get_prop (see case 17)
  // size of the property whose DATA begins at dataAddr (for get_prop_len)
  propLenAt: function (dataAddr) {
    if (dataAddr === 0) return 0;
    if (this.version >= 4) {
      var b = this.mem[dataAddr - 1];
      if (b & 0x80) { var s = b & 63; return s ? s : 64; }
      return (b & 0x40) ? 2 : 1;
    }
    return (this.mem[dataAddr - 1] >> 5) + 1;
  },
  attrWord: function (o, n) { return this.objAddr(o) + (n >> 4) * 2; },
  getAttr: function (o, n) { return (this.getu(this.attrWord(o, n)) & (1 << (15 - (n & 15)))) ? 1 : 0; },
  setAttr: function (o, n) { var a = this.attrWord(o, n); this.put(a, this.getu(a) | (1 << (15 - (n & 15)))); },
  clearAttr: function (o, n) { var a = this.attrWord(o, n); this.put(a, this.getu(a) & ~(1 << (15 - (n & 15)))); },
  read: function() { return ""; },
  regBreak: null,
  restarted: function() {},
  restore: function() { return null; },
  run: function() {
    var self = this;
    var mem, pc, cs, ds, op0, op1, op2, op3, opc, inst, x, y, z;
    var globals, objects, fwords, defprop;
    var addr, addrS, docall, fetch, initRng, init, move, opfetch, pcfetch, pcget, pcgetb, pcgetu, predicate, propfind, ret, store, xfetch, xstore;
    var step, runInterrupt, readTimedLine;
    var HALT = {};   // unique sentinel: step()/runInterrupt return this to unwind run()

    // Functions
    addr  = function (x) { return self.unpackRoutine(x); };
    addrS = function (x) { return self.unpackString(x); };
    fetch = function(x) {
      if (x == 0) return ds.pop();
      if (x < 16) return cs[0].local[x - 1];
      return self.get(globals + 2 * x);
    };
    initRng = function() {
      self.seed = (Math.random() * 0xFFFFFFFF) >>> 0;
    };
    init = function() {
      mem = self.mem = new Uint8Array(self.memInit);
      mem[1] &= 3;
      if (self.isTandy) mem[1] |= 8;
      if (!self.updateStatusLine) mem[1] |= 16;
      if (self.screen && self.split) mem[1] |= 32;
      if (self.version >= 4 && self.readTimedKey) mem[1] |= 0x80;   // Flags1 bit7: timed keyboard input available
      self.put(16, self.savedFlags);
      if (self.version >= 5) {                     // Flags2 bit7: sound effects available
        var f2s = self.get(16);
        if (self.sound) f2s |= 0x80; else f2s &= ~0x80;
        self.put(16, f2s); self.savedFlags = f2s;
      }
      if (!self.vocabulary) self.parseVocab(self.getu(8));
      defprop = self.defprop = self.getu(10) - 2;
      globals = self.globals = self.getu(12) - 32;
      self.fwords = fwords = self.getu(24);
      self.unicode = JSZM_DEFAULT_UNICODE;                  // default table
      if (self.version >= 5) {
        var hext = self.getu(0x36);                         // header-extension table addr (0 = none)
        if (hext) {
          var nw = self.getu(hext);                         // word 0 = number of further words
          if (nw >= 3) {
            var utab = self.getu(hext + 6);                 // word 3 = Unicode translation table addr
            if (utab) {
              var n = self.mem[utab], arr = [], k;          // byte 0 = count of entries
              for (k = 0; k < n; k++) arr.push(self.getu(utab + 1 + k * 2));
              self.unicode = arr;                           // story-supplied table replaces the default
            }
          }
        }
      }
      self.alphabet = null;                                 // null = built-in alphabets
      if (self.version >= 5) {
        var atab = self.getu(0x34);                         // custom alphabet table addr (0 = built-in)
        if (atab) {
          var a = [], j;
          for (j = 0; j < 78; j++) a.push(self.mem[atab + j]);  // 78 ZSCII codes (A0,A1,A2 x 26)
          self.alphabet = a;
        }
      }
      cs = [];
      ds = [];
      self.ostream3 = [];
      self.ostream1 = true;       // screen selected by default (a prior game may have left it off)
      if (self.version === 6) {
        // routineOffset/stringOffset were read in the constructor (Task 2); do not re-read here.
        var mainR = self.unpackRoutine(self.getu(6));        // 0x06 = packed addr of main routine
        var mnl = mem[mainR];                                // main's local count
        // Int16Array zeroes the locals; v5+ routines have no default-word list to read (unlike docall's v1-4 loop).
        cs.unshift({ ds: ds, pc: 0, local: new Int16Array(mnl), discard: true, capture: false, args: 0 });
        ds = [];
        pc = mainR + 1;                                      // v5+ routines: no default local words
        self.windows = []; self.curwin = 0;
        var wi, pp, wp;
        for (wi = 0; wi < 8; wi++) { wp = []; for (pp = 0; pp < 18; pp++) wp.push(0); self.windows.push(wp); }
        // window 0 default size (full screen, props 2/3) is set from the effective pixel dims in the v6 header block below.
      } else {
        pc = self.getu(6);
      }
      objects = self.objects = defprop + self.vp.objectsAdj;
      if (self.version >= 4) {            // interpreter fills these; v3 ignores them, so leave v3 memory untouched
        var rows = self.screenRows || 24, cols = self.screenCols || 80;
        self.mem[0x1e] = 6;               // interpreter number: 6 = IBM PC
        self.mem[0x1f] = 1;               // interpreter version
        self.mem[0x20] = rows & 0xff;     // screen height (lines)
        self.mem[0x21] = cols & 0xff;     // screen width (chars)
        if (self.version >= 5) {
          if (self.version === 6) {
            var fw = self.fontWidth || 1, fh = self.fontHeight || 1;
            var effW = self.screenWidthPx || (cols * fw), effH = self.screenHeightPx || (rows * fh);
            self.put(0x22, effW);                                 // screen width in pixels
            self.put(0x24, effH);                                 // screen height in pixels
            self.mem[0x26] = fw & 0xff; self.mem[0x27] = fh & 0xff;
            // Window property 13 = font size = (height << 8) | width (Z-spec; cf. frotz wp.font_size). Games read
            // it to compute row/column spacing for cursor-positioned layouts -- Arthur's InvisiClues hint menu and
            // its stat panel do `y += hi(font_size)` per line. Left 0, the height byte is 0, every line lands on the
            // same row and the menu/stats collapse. Seed every window with the interpreter font, matching the header.
            var fsz = ((fh & 0xff) << 8) | (fw & 0xff);
            if (self.windows) { self.windows[0][2] = effH & 0xffff; self.windows[0][3] = effW & 0xffff;  // window 0 = full screen, consistent w/ header
              for (var w13 = 0; w13 < 8; w13++) self.windows[w13][13] = fsz; }
            var f2 = self.get(16);
            if (self.graphicsAvailable && self.graphicsAvailable()) f2 |= 8; else f2 &= ~8;  // Flags2 bit3 pictures
            self.put(16, f2); self.savedFlags = f2;
          } else {
            self.put(0x22, cols);           // screen width in units
            self.put(0x24, rows);           // screen height in units
            self.mem[0x26] = 1; self.mem[0x27] = 1;   // font width/height = 1
          }
        }
      }
      initRng();
    };
    docall = function (argv, discard, capture) {
      var routine = argv[0] & 65535, n, i;
      if (!routine) { if (!discard) store(0); return; }   // call to address 0 returns false
      routine = addr(routine);
      n = mem[routine];                                   // number of locals
      cs.unshift({ ds: ds, pc: pc, local: new Int16Array(n), discard: discard, capture: capture, args: (self.version >= 4 ? argv.length - 1 : 0) });
      ds = [];
      pc = routine + 1;
      for (i = 0; i < n; i++) {
        if (self.version >= 5) cs[0].local[i] = 0;        // v5+: locals default 0, no default words
        else cs[0].local[i] = pcget();                    // v1-4: read the 2-byte default (advances pc)
      }
      for (i = 1; i < argv.length && i <= n; i++) cs[0].local[i - 1] = argv[i];
    };
    move = function(x, y) {
      var w, z;
      // Remove x from its old parent's child/sibling chain
      if (z = self.objField(x, 0)) {
        if (self.objField(z, 2) == x) self.objSetField(z, 2, self.objField(x, 1));
        else {
          z = self.objField(z, 2);
          while (z != x) { w = z; z = self.objField(z, 1); }
          self.objSetField(w, 1, self.objField(x, 1));
        }
      }
      // Insert x at the front of y's child chain
      self.objSetField(x, 0, y);
      if (y) { self.objSetField(x, 1, self.objField(y, 2)); self.objSetField(y, 2, x); }
      else self.objSetField(x, 1, 0);
    };
    opfetch = function(x, y) {
      if ((x &= 3) == 3) return;
      opc = y;
      return [pcget, pcgetb, pcfetch][x]();
    };
    pcfetch = function(x) { return fetch(mem[pc++]); };
    pcget = function() { pc += 2; return self.get(pc - 2); };
    pcgetb = function() { return mem[pc++]; };
    pcgetu = function() { pc += 2; return self.getu(pc - 2); };
    predicate = function(p) {
      var x = pcgetb();
      if (x & 128) p = !p;
      if (x & 64) x &= 63; else x = ((x & 63) << 8) | pcgetb();
      if (p) return;
      if (x == 0 || x == 1) return ret(x);
      if (x & 0x2000) x -= 0x4000;
      pc += x - 2;
    };
    propfind = function () {
      var z = self.firstProp(op0), h;
      while (self.mem[z]) {
        h = self.propHeader(z);
        if (h[0] == op1) { op3 = h[2]; return true; }
        z = h[3];
      }
      op3 = 0;
      return false;
    };
    ret = function (x) {
      var f = cs[0];
      ds = f.ds; pc = f.pc; cs.shift();
      if (f.capture) { self.__interruptResult = x; return; }   // interrupt routine: hand value to runInterrupt
      if (f.discard) return;     // procedure call (call_*n): no result is stored
      store(x);
    };
    store = function(y) {
      var x = pcgetb();
      if (x == 0) ds.push(y);
      else if (x < 16) cs[0].local[x - 1] = y;
      else self.put(globals + 2 * x, y);
    };
    xfetch = function(x) {
      if (x == 0) return ds[ds.length - 1];
      if (x < 16) return cs[0].local[x - 1];
      return self.get(globals + 2 * x);
    };
    xstore = function(x, y) {
      if (x == 0) ds[ds.length - 1] = y;
      else if (x < 16) cs[0].local[x - 1] = y;
      else self.put(globals + 2 * x, y);
    };

    // Run the interrupt routine (packed addr) to completion and return its value
    // (& 0xffff): nonzero = abort the read, 0 = keep waiting. Returns HALT if the
    // routine unwinds run() (e.g. QUITs). The routine takes no args.
    runInterrupt = function (packed) {
      if (!packed) return 0;
      var base = cs.length;
      self.__interruptResult = 0;
      docall([packed], false, true);            // store-form call, result captured (not written to memory)
      if (cs.length <= base) return 0;          // unpacked to address 0 (defensive)
      while (cs.length > base) { if (step()) return HALT; }
      return self.__interruptResult & 0xffff;
    };

    // Engine-owned minimal line editor for the timed read path (case 228). Drives
    // self.readTimedKey per time-slice and self.echoInput for echo. Returns
    // { text, aborted } on completion/abort, or HALT to unwind run().
    readTimedLine = function (rmax, time, routine) {
      var buf = "", k, r;
      for (;;) {
        k = self.readTimedKey(time);
        if (k === null) return HALT;                       // disconnect/idle -> unwind
        if (k < 0) {                                       // time slice elapsed -> interrupt
          self.__printed = false;
          r = runInterrupt(routine);
          if (r === HALT) return HALT;
          if (r) return { text: buf, aborted: true };      // routine aborted the read
          if (self.__printed && self.afterInterrupt) self.afterInterrupt(buf);   // routine printed -> live status / re-show typed line
          continue;
        }
        if (k === 13 || k === 10) {                                                 // Enter -> done
          if (self.echoInput) self.echoInput("\r\n");                              // echo the newline, like getstr does
          return { text: buf, aborted: false };
        }
        if (k === 8 || k === 127) {                        // backspace
          if (buf.length) { buf = buf.slice(0, -1); if (self.echoInput) self.echoInput(8); }
          continue;
        }
        if (k >= 32 && k <= 126 && buf.length < rmax) {    // printable -> append + echo
          buf += String.fromCharCode(k);
          if (self.echoInput) self.echoInput(k);
        }
        // other keys (non-printable / buffer full) are ignored
      }
    };

    // Initializations
    init();
    self.restarted();
    self.highlight(!!(self.savedFlags & 2));

    // Resume hook: if the host supplied a saved snapshot, restore it (mirrors
    // the RESTORE opcode). A foreign/corrupt slot deserializes to null -> the
    // freshly-init()'d new game stands. Unlike RESTORE we intentionally do NOT
    // re-apply the pre-restore Flags-2 byte; the snapshot's own Flags 2 is the
    // correct resume state, and genPrint() re-syncs highlight on first output.
    if (self.resumeData) {
      z = self.deserialize(self.resumeData);
      self.resumeData = null;
      if (z) { ds = z[0]; cs = z[1]; pc = z[2]; }
    }

    // One interpreter step: execute the instruction at pc. Returns falsy to
    // continue, or the HALT sentinel to make run() return (QUIT / disconnect /
    // read returned null). Extracted so timed input can re-enter the interpreter
    // (runInterrupt) from inside a read handler.
    step = function () {
      var instPC = pc;            // start-of-instruction pc, for resumable snapshots
      inst = pcgetb();
      if (inst < 128) {
        // 2OP
        if (inst & 64) op0 = pcfetch(); else op0 = pcgetb();
        if (inst & 32) op1 = pcfetch(); else op1 = pcgetb();
        inst &= 31;
        opc = 2;
      } else if (inst < 176) {
        // 1OP
        x = (inst >> 4) & 3;
        inst &= 143;
        if (x == 0) op0 = pcget();
        else if (x == 1) op0 = pcgetb();
        else if (x == 2) op0 = pcfetch();
      } else if (inst === 190 && self.version >= 5) {
        // v5 extended opcode: 0xBE <ext#> <types> <operands...>; map to switch case 256+ext.
        inst = 256 + pcgetb();
        x = pcgetb();
        op0 = opfetch(x >> 6, 1);
        op1 = opfetch(x >> 4, 2);
        op2 = opfetch(x >> 2, 3);
        op3 = opfetch(x >> 0, 4);
      } else if (inst >= 192) {
        // VAR form
        if (inst === 236 || inst === 250) {
          // call_vs2 / call_vn2 (up to 7 args): BOTH type bytes come first (consecutive),
          // THEN all 8 operands (spec 4.4.3). Build the FULL ordered arg list by filtering
          // omitted operands across all 8 slots.
          var t1 = pcgetb(), t2 = pcgetb(), k, v;
          self.__xargs = [];
          for (k = 0; k < 4; k++) { v = opfetch((t1 >> (6 - k * 2)), 0); if (v !== undefined) self.__xargs.push(v); }
          for (k = 0; k < 4; k++) { v = opfetch((t2 >> (6 - k * 2)), 0); if (v !== undefined) self.__xargs.push(v); }
        } else {
          x = pcgetb();
          op0 = opfetch(x >> 6, 1);
          op1 = opfetch(x >> 4, 2);
          op2 = opfetch(x >> 2, 3);
          op3 = opfetch(x >> 0, 4);
        }
        if (inst < 224) inst &= 31;
      }
      switch (inst) {
        case 1: // EQUAL?
          predicate(op0 == op1 || (opc > 2 && op0 == op2) || (opc == 4 && op0 == op3));
          break;
        case 2: // LESS?
          predicate(op0 < op1);
          break;
        case 3: // GRTR?
          predicate(op0 > op1);
          break;
        case 4: // DLESS?
          xstore(op0, x = xfetch(op0) - 1);
          predicate(x < op1);
          break;
        case 5: // IGRTR?
          xstore(op0, x = xfetch(op0) + 1);
          predicate(x > op1);
          break;
        case 6: // IN?
          predicate(self.objField(op0, 0) == op1);
          break;
        case 7: // BTST
          predicate((op0 & op1) == op1);
          break;
        case 8: // BOR
          store(op0 | op1);
          break;
        case 9: // BAND
          store(op0 & op1);
          break;
        case 10: // FSET? (test_attr)
          predicate(self.getAttr(op0, op1));
          break;
        case 11: // FSET (set_attr)
          self.setAttr(op0, op1);
          break;
        case 12: // FCLEAR (clear_attr)
          self.clearAttr(op0, op1);
          break;
        case 13: // SET
          xstore(op0, op1);
          break;
        case 14: // MOVE
          move(op0, op1);
          break;
        case 15: // GET
          store(self.get((op0 + op1 * 2) & 65535));
          break;
        case 16: // GETB
          store(mem[(op0 + op1) & 65535]);
          break;
        case 17: // GETP (get_prop): sign-extend the 2-byte value (self.get) to match the
          // engine's signed-value convention -- self.getu (unsigned) would put 0xFFFF on the
          // stack as 65535, which then mismatches a sign-extended -1 in je/jl/jg (Border Zone
          // camera "rewound" check). 1-byte props are 0..255 (no sign extension).
          if (propfind()) store(self.propLenAt(op3) == 1 ? self.mem[op3] : self.get(op3));
          else store(self.defProp(op1));
          break;
        case 18: // GETPT
          propfind();
          store(op3);
          break;
        case 19: // NEXTP (get_next_prop)
          if (op1) {
            propfind();                              // op3 = data addr of property op1
            // header of op1 is right before its data; its next-header addr is propHeader(thatHdr)[3]
            var hz = op3 - (self.version >= 4 ? ((self.mem[op3 - 1] & 0x80) ? 2 : 1) : 1);
            store(self.mem[self.propHeader(hz)[3]] ? self.propHeader(self.propHeader(hz)[3])[0] : 0);
          } else {
            store(self.propHeader(self.firstProp(op0))[0]);
          }
          break;
        case 20: // ADD
          store(op0 + op1);
          break;
        case 21: // SUB
          store(op0 - op1);
          break;
        case 22: // MUL
          store(Math.imul(op0, op1));
          break;
        case 23: // DIV
          store(Math.trunc(op0 / op1));
          break;
        case 24: // MOD
          store(op0 % op1);
          break;
        case 27: // set_colour (2OP, v5+): fg=op0, bg=op1 (0=current, 1=default, 2..9 palette)
          if (self.setColour) self.setColour(op0, op1);
          break;
        case 28: // throw (2OP, v5+): unwind call stack to catch-depth op1, then return op0
          while (cs.length > op1) cs.shift();
          ret(op0);
          break;
        case 128: // ZERO?
          predicate(!op0);
          break;
        case 129: // NEXT?
          store(x = self.objField(op0, 1));
          predicate(x);
          break;
        case 130: // FIRST?
          store(x = self.objField(op0, 2));
          predicate(x);
          break;
        case 131: // LOC
          store(self.objField(op0, 0));
          break;
        case 132: // PTSIZE (get_prop_len)
          store(self.propLenAt(op0));
          break;
        case 133: // INC
          x = xfetch(op0);
          xstore(op0, x + 1);
          break;
        case 134: // DEC
          x = xfetch(op0);
          xstore(op0, x - 1);
          break;
        case 135: // PRINTB
          self.genPrint(self.getText(op0 & 65535));
          break;
        case 137: // REMOVE
          move(op0, 0);
          break;
        case 138: // PRINTD
          self.genPrint(self.getText(self.objPropTable(op0) + 1));
          break;
        case 139: // RETURN
          ret(op0);
          break;
        case 140: // JUMP
          pc += op0 - 2;
          break;
        case 141: // PRINT
          self.genPrint(self.getText(addrS(op0)));
          break;
        case 142: // VALUE
          store(xfetch(op0));
          break;
        case 143: // BCOM (not) in v1-4; call_1n (1OP procedure) in v5+
          if (self.version >= 5) docall([op0], true);
          else store(~op0);
          break;
        case 176: // RTRUE
          ret(1);
          break;
        case 177: // RFALSE
          ret(0);
          break;
        case 178: // PRINTI
          self.genPrint(self.getText(pc));
          pc = self.endText;
          break;
        case 179: // PRINTR
          self.genPrint(self.getText(pc) + "\n");
          ret(1);
          break;
        case 180: // NOOP
          break;
        case 181: // SAVE
          self.savedFlags = self.get(16);
          predicate(self.save(self.serialize(ds, cs, pc)));
          break;
        case 182: // RESTORE
          self.savedFlags = self.get(16);
          if (z = self.restore()) z = self.deserialize(z);
          self.put(16, self.savedFlags);
          if (z) ds = z[0], cs = z[1], pc = z[2];
          predicate(z);
          break;
        case 183: // RESTART
          init();
          self.undoSlot = null;   // a pre-restart undo snapshot belongs to a finished session
          self.restarted();
          break;
        case 184: // RSTACK
          ret(ds[ds.length - 1]);
          break;
        case 185: // FSTACK: pop (v1-4) / catch (v5+, stores call-frame depth)
          if (self.version >= 5) store(cs.length);
          else ds.pop();
          break;
        case 186: // QUIT
          return HALT;
        case 187: // CRLF
          self.genPrint("\n");
          break;
        case 188: // USL (show_status): v3 only; v4+ games manage their own status line (no-op)
          if (self.version === 3 && self.updateStatusLine) self.updateStatusLine(self.getText(self.objPropTable(xfetch(16)) + 1), xfetch(18), xfetch(17));
          break;
        case 189: // VERIFY
          predicate(self.verify());
          break;
        case 191: // PIRACY (0OP, v5+): always branch (interpreter reports genuine)
          predicate(true);
          break;
        case 25: // call_2s (2OP, store)
          docall([op0, op1], false);
          break;
        case 26: // call_2n (2OP, procedure -> discard)
          docall([op0, op1], true);
          break;
        case 136: // call_1s (1OP, store)
          docall([op0], false);
          break;
        case 224: // CALL / call_vs (VAR, store)
          docall([op0, op1, op2, op3].slice(0, opc), false);
          break;
        case 236: // call_vs2 (VAR, up to 7 args, store)
        case 250: // call_vn2 (VAR, up to 7 args, procedure)
          docall(self.__xargs, inst === 250);
          break;
        case 249: // call_vn (VAR, procedure -> discard)
          docall([op0, op1, op2, op3].slice(0, opc), true);
          break;
        case 225: // PUT
          self.put((op0 + op1 * 2) & 65535, op2);
          break;
        case 226: // PUTB
          mem[(op0 + op1) & 65535] = op2;
          break;
        case 227: // PUTP (put_prop)
          propfind();
          if (self.propLenAt(op3) == 2) self.put(op3, op2);
          else mem[op3] = op2;
          break;
        case 228: // READ (sread v1-4 / aread v5+). v4+: op2=time, op3=routine -> timed line read
                  // (engine-owned echo/editor); else the untimed door read. See timed-input spec.
          self.genPrint("");
          if (self.version === 3 && self.updateStatusLine) self.updateStatusLine(self.getText(self.objPropTable(xfetch(16)) + 1), xfetch(18), xfetch(17));
          self.checkpoint = self.serialize(ds, cs, instPC);   // in-memory resumable point
          var rtb = op0 & 65535, rpb = op1 & 65535, rmax = mem[rtb];
          var input, aborted = false;
          if (self.version >= 4 && op2 > 0 && op3 !== undefined && op3 != 0 && self.readTimedKey) {
            if (self.readPrompt) self.readPrompt();      // pager reset + status repaint (cur.read() does this on the untimed path)
            var tline = readTimedLine(rmax, op2, op3);
            if (tline === HALT) return HALT;             // disconnect/idle mid-read -> unwind run()
            input = tline.text; aborted = tline.aborted;
          } else {
            input = self.read(rmax);
            if (input === null) return HALT;             // door signalled disconnect/terminate -> unwind run()
          }
          if (self.version >= 5) {
            // v5 text buffer: byte0=max, byte1=count typed, chars from byte2 (NOT zero-terminated).
            var rs = input.toLowerCase().slice(0, rmax), ri;
            for (ri = 0; ri < rs.length; ri++) mem[rtb + 2 + ri] = rs.charCodeAt(ri);
            mem[rtb + 1] = rs.length;
            if (rpb) self.tokeniseBuffer(rs, rpb, 0, 0, 2);
            // Terminator (v5+): 0 on interrupt-abort; else the terminating character the door reported via
            // self.readTerminator (a function/cursor key from the header-0x2e terminating-chars table, e.g. Arthur's
            // map keys), defaulting to Return (13). The door ends the line on such a key and sets the code.
            store(aborted ? 0 : (typeof self.readTerminator === "number" ? self.readTerminator : 13));
            self.readTerminator = undefined;
          } else {
            // v1-4 sread: not store-form, so no terminator byte; the typed chars are written
            // (handleInput) and on a timed abort the game infers it from its own routine's return.
            self.handleInput(input, rtb, rpb);
          }
          break;
        case 229: // PRINTC
          self.genPrint(op0 == 13 ? "\n" : op0 ? String.fromCharCode(op0) : "");
          break;
        case 230: // PRINTN
          self.genPrint(String(op0));
          break;
        case 231: // RANDOM
          if (op0 <= 0) {
            if (op0 === 0) {
              initRng();
            } else {
              self.seed = (op0 >>> 0);
            }
            store(0);
            break;
          }
          self.seed = (1664525 * self.seed + 1013904223) >>> 0;
          store(Math.floor((self.seed / 0xFFFFFFFF) * op0) + 1);
          break;
        case 232: // PUSH
          ds.push(op0);
          break;
        case 233: // pull. v1-5: pull (variable) -> store INTO the operand variable, NO store byte.
                   // v6: pull (stack) -> (result) is a STORE opcode (reads a store byte); op0 = user-stack
                   // address (omitted -> game stack). Pull the top value (free++ then read the freed slot),
                   // then store() it -- which consumes the store byte and keeps PC aligned.
          if (self.version === 6) {
            var puv;
            if (op0) { var pua = op0 & 0xffff, pun = (self.getu(pua) + 1) & 0xffff; puv = self.getu((pua + 2 * pun) & 0xffff); self.put(pua, pun); }
            else puv = ds.pop();
            store(puv);
          } else {
            xstore(op0, ds.pop());
          }
          break;
        case 234: // SPLIT
          if (self.split) self.split(op0);
          break;
        case 235: // SCREEN (set_window); v6 also tracks the current window for get/put_wind_prop
          if (self.version === 6) self.curwin = op0 & 7;
          if (self.screen) self.screen(op0);
          break;
        case 237: // erase_window (v4+): -1 = whole screen, -2 = whole screen keep split
          // (operands are already sign-extended to a signed 16-bit value by pcget/opfetch,
          //  so -1/-2 arrive as -1/-2 -- no further sign-extension needed)
          if (self.eraseWindow) self.eraseWindow(op0);
          break;
        case 238: // erase_line (v4+): op0==1 -> erase from cursor to end of line
          if (self.eraseLine) self.eraseLine(op0);
          break;
        case 239: // set_cursor: v4+ op0=row op1=col (upper window). v6: op0 -1/-2 = cursor off/on
                   // (visibility toggles, no position/window operands); op0>=0 = a real (y, x [, window])
                   // pixel position. Don't store the off/on sentinels as a position or forward them as one.
          if (self.version === 6) {
            if (op0 >= 0) {
              if (self.windows) {
                var scw = (op2 === undefined ? self.curwin : self.v6win(op2));
                self.windows[scw][4] = op0 & 0xffff; self.windows[scw][5] = op1 & 0xffff;
              }
              if (self.setCursor) self.setCursor(op0, op1);
            }
          } else if (self.setCursor) self.setCursor(op0, op1);
          break;
        case 240: // get_cursor (v4+): write current row,col into the 2-word table at op0
          if (op0) {
            var cur = self.getCursor ? self.getCursor() : { row: 1, col: 1 };
            self.put(op0 & 65535, cur.row); self.put((op0 + 2) & 65535, cur.col);
          }
          break;
        case 242: // buffer_mode (v4+): op0 0/1 -> word-wrap buffering off/on (lower window)
          if (self.bufferMode) self.bufferMode(op0);
          break;
        case 243: // output_stream: +N enable / -N disable; stream 3 = memory table (op1)
          // op0 arrives already sign-extended by the operand decode -- compare directly.
          if (op0 === 3) { self.ostream3.push({ table: op1 & 0xffff, count: 0 }); self.put(op1 & 0xffff, 0); }
          else if (op0 === -3) { if (self.ostream3.length) self.ostream3.pop(); }
          else if (op0 === 1) { self.ostream1 = true; }     // (re)select the screen
          else if (op0 === -1) { self.ostream1 = false; }   // deselect the screen (z-spec 7.1.2, v3+): suppress drawing
          // streams 2 (transcript), 4 (input log): no-op
          break;
        case 241: // set_text_style (v4+): bitmask 0=roman 1=reverse 2=bold 4=italic 8=fixed
          if (self.setTextStyle) self.setTextStyle(op0);
          break;
        case 244: // input_stream (v3+): no-op
          break;
        case 245: // sound_effect (v3+): number [,effect [,volume/repeats [,routine]]].
                   // number 1/2 = interpreter bleeps; 3+ = sampled sounds (The Lurking
                   // Horror, Sherlock). effect: 1 prepare, 2 start, 3 stop, 4 finish.
                   // op2 word: low byte = volume (1..8, 255 = loudest), high byte =
                   // repeats (v5+ only; 255 = forever). No operands at all (Beyond
                   // Zork does this) = a high bleep, per the Z-Standard's remarks.
                   // The v5+ completion routine (op3) runs IMMEDIATELY after a
                   // non-looping sample start (the classic interpreter shortcut): a
                   // sound started INSIDE the routine is flagged chained (hook arg 5)
                   // so the door queues it onto the same channel BEHIND the current
                   // one, and the chain plays in order client-side with no
                   // playback-end notification needed (Sherlock: the ambient-loop
                   // resume, the recursive Big Ben hour chimes). Looping starts
                   // (repeats 255) never finish, so no callback -- which also stops
                   // Sherlock's default resume-routine from recursing forever. The
                   // routine's game-state side effects thus land at queue time, not
                   // at true playback end -- harmless for the flag-clears and chime
                   // countdowns the sound-capable games actually do.
          if (self.sound) {
            var seN = (op0 === undefined) ? 1 : (op0 & 0xffff);
            var seFx = (op1 === undefined) ? 2 : (op1 & 0xffff);
            var seVR = (op2 === undefined) ? 0 : (op2 & 0xffff);
            var seRep = self.version >= 5 ? (seVR >> 8) & 0xff : 0;
            // Capture the routine BEFORE runInterrupt: it executes the routine's own
            // instructions, which overwrite the shared op0..op3 (cf. read_char below).
            var seRtn = (self.version >= 5 && op3 !== undefined) ? (op3 & 0xffff) : 0;
            self.sound(seN, seFx, seVR & 0xff, seRep, !!self.__soundChain);
            if (seRtn && seFx === 2 && seN > 2 && seRep !== 255) {
              var sePrev = self.__soundChain;   // save/restore: chime routines nest
              self.__soundChain = true;
              var seR = runInterrupt(seRtn);
              self.__soundChain = sePrev;
              if (seR === HALT) return HALT;
            }
          }
          break;
        case 246: // read_char (v4+): read one keypress, store its ZSCII code.
                   // op0 is the device (always 1); v4+ timed variant: op1=time (tenths),
                   // op2=interrupt routine (packed). Each elapsed slice runs the routine; a
                   // nonzero return aborts the read (stores 0). Not checkpointed: menus issue
                   // these in tight loops and a held/repeating key would serialize every
                   // iteration. The line read (case 228) is the resumable point.
          if (self.version >= 4 && op1 > 0 && op2 !== undefined && op2 != 0 && self.readTimedKey) {
            // Capture time/routine into locals BEFORE the loop: runInterrupt() executes
            // the routine's own instructions, which overwrite the shared op0..op3. Reading
            // op1/op2 after that would feed a garbage routine address to runInterrupt.
            var rcTime = op1, rcRoutine = op2, rcDone = false, rcK, rcR;
            if (self.readPrompt) self.readPrompt();      // pager reset + status repaint (cur.readKey() does this on the untimed path)
            while (!rcDone) {
              rcK = self.readTimedKey(rcTime);
              if (rcK === null) return HALT;               // disconnect/idle -> unwind run()
              if (rcK < 0) {                               // time slice elapsed -> interrupt
                self.__printed = false;
                rcR = runInterrupt(rcRoutine);
                if (rcR === HALT) return HALT;
                if (rcR) { store(0); rcDone = true; }      // routine aborted the read
                else if (self.__printed && self.afterInterrupt) self.afterInterrupt("");   // live status refresh
              } else { store(rcK); rcDone = true; }        // a key -> store its ZSCII code
            }
          } else {
            var kc = self.read(1);
            if (kc === null) return HALT;                  // disconnect/terminate -> unwind run()
            store(kc.length ? kc.charCodeAt(0) : 13);
          }
          break;
        case 247: // scan_table (v4+): search op2 entries of a table at op1 for value op0.
          // op3 = optional form byte (default 0x82): bit7 = word-sized compare, low7 = entry length.
          x = (op3 === undefined ? 0x82 : op3);
          y = x & 0x7f;                               // entry length in bytes
          var stAddr = op1 & 65535, stFound = 0, stN = op2, stE;
          while (stN-- > 0) {
            stE = (x & 0x80) ? self.getu(stAddr) : self.mem[stAddr];
            if (stE === (x & 0x80 ? (op0 & 0xffff) : (op0 & 0xff))) { stFound = stAddr; break; }
            stAddr += y;
          }
          store(stFound);
          predicate(stFound);
          break;
        case 251: // tokenise (v5+): lex text at op0 into parse buffer op1, dict op2, flag op3.
          // Text buffer layout matches v5 read: byte0=max, byte1=count, chars from byte2.
          var tkN = self.mem[(op0 & 65535) + 1], tkS = "", tki;
          for (tki = 0; tki < tkN; tki++) tkS += String.fromCharCode(self.mem[(op0 & 65535) + 2 + tki]);
          self.tokeniseBuffer(tkS.toLowerCase(), op1 & 65535, op2 ? (op2 & 65535) : 0, op3 || 0, 2);
          break;
        case 252: // encode_text (v5+): encode op1 ZSCII chars at op0+op2 into dict-word form at op3.
          self.encodeText(op0 & 65535, op1, op2, op3 & 65535);
          break;
        case 253: // copy_table (v5+): copy/zero op2 bytes from op0 to op1.
          var ctF = op0 & 65535, ctS = op1 & 65535, ctSz = ((op2 & 0xffff) << 16) >> 16, cti;
          if (ctS === 0) { for (cti = 0; cti < Math.abs(ctSz); cti++) mem[ctF + cti] = 0; }
          else if (ctSz < 0 || ctS <= ctF) { ctSz = Math.abs(ctSz); for (cti = 0; cti < ctSz; cti++) mem[ctS + cti] = mem[ctF + cti]; }
          else { for (cti = ctSz - 1; cti >= 0; cti--) mem[ctS + cti] = mem[ctF + cti]; }
          break;
        case 254: // print_table (v5+): print a width x height rectangle of ZSCII bytes at op0.
          var ptA = op0 & 65535, ptW = op1, ptH = (op2 === undefined ? 1 : op2), ptSk = op3 || 0, ptr, ptc, pts;
          for (ptr = 0; ptr < ptH; ptr++) {
            pts = "";
            for (ptc = 0; ptc < ptW; ptc++) pts += String.fromCharCode(self.mem[ptA + ptr * (ptW + ptSk) + ptc]);
            self.genPrint(ptr ? "\n" + pts : pts);
          }
          break;
        case 248: // not (VAR form, v5+): bitwise complement, store
          store(~op0 & 0xffff);
          break;
        case 255: // check_arg_count (VAR): branch if arg #op0 was supplied
          predicate(cs.length > 0 && op0 <= cs[0].args);
          break;
        case 256 + 0: // EXT save (v5 store-form): store 1 on success, 0 on failure
          self.savedFlags = self.get(16);
          store(self.save(self.serialize(ds, cs, pc)) ? 1 : 0);
          break;
        case 256 + 1: // EXT restore (v5 store-form): on success resume with restored state;
                      // the restored pc points at the @save result-variable byte, so store(2)
                      // reads it via pcgetb() and deposits 2 there. On failure store(0) here.
          self.savedFlags = self.get(16);
          z = self.restore();
          if (z) z = self.deserialize(z);
          self.put(16, self.savedFlags);
          if (z) { ds = z[0]; cs = z[1]; pc = z[2]; store(2); }
          else store(0);
          break;
        case 256 + 2: // EXT log_shift: shift op0 by op1 (left if op1>=0, logical right if op1<0)
          x = ((op1 & 0xffff) << 16) >> 16;  // sign-extend op1 to get signed shift count
          store((x >= 0 ? (op0 & 0xffff) << x : (op0 & 0xffff) >>> -x) & 0xffff);
          break;
        case 256 + 3: // EXT art_shift: arithmetic shift (sign-preserving on right shift)
          x = ((op1 & 0xffff) << 16) >> 16;  // sign-extend op1 to get signed shift count
          y = ((op0 & 0xffff) << 16) >> 16;  // sign-extend op0 to get signed value
          store((x >= 0 ? (y << x) : (y >> -x)) & 0xffff);
          break;
        case 256 + 9: // EXT save_undo: always succeeds (store 1); snapshot stored in-memory
          self.undoSlot = self.serialize(ds, cs, pc);
          store(1);
          break;
        case 256 + 10: // EXT restore_undo: on success resume with undo snapshot, store 2;
                       // on failure (no snapshot) store 0
          self.savedFlags = self.get(16);
          z = self.undoSlot ? self.deserialize(self.undoSlot) : null;
          self.put(16, self.savedFlags);   // preserve Flags-2 across the load, like @restore
          if (z) { ds = z[0]; cs = z[1]; pc = z[2]; store(2); }
          else store(0);
          break;
        case 256 + 4: // set_font (EXT): v6 has a window operand; report previous font, or 0 if declined
          if (self.version === 6 && self.setFont) store(self.setFont(self.v6win(op1 === undefined ? 0xfffd : op1), op0) || 0);
          else store(op0 === 1 || op0 === 4 ? 1 : 0);
          break;
        case 256 + 11: // EXT print_unicode: print one Unicode code point
          self.genPrint(String.fromCharCode(op0 & 0xffff));
          break;
        case 256 + 12: // EXT check_unicode: bit0=printable, bit1=receivable, per the host's charset
          store(self.checkUnicode ? self.checkUnicode(op0 & 0xffff) : (op0 < 0x100 ? 3 : 0));
          break;
        case 256 + 19: { // get_wind_prop window property -> (result)
          var gw = self.v6win(op0), gp = op1 & 0xffff;
          store(self.windows && gp < 18 ? (self.windows[gw][gp] & 0xffff) : 0);
          break;
        }
        case 256 + 25: { // put_wind_prop window property value
          var pw = self.v6win(op0), ppi = op1 & 0xffff;
          if (self.windows && ppi < 18) self.windows[pw][ppi] = op2 & 0xffff;
          break;
        }
        case 256 + 16: // move_window window y x
          { var mw = self.v6win(op0); self.windows[mw][0] = op1 & 0xffff; self.windows[mw][1] = op2 & 0xffff;
            if (self.windowChanged) self.windowChanged(mw); }
          break;
        case 256 + 17: // window_size window y x
          { var sw = self.v6win(op0); self.windows[sw][2] = op1 & 0xffff; self.windows[sw][3] = op2 & 0xffff;
            if (self.windowChanged) self.windowChanged(sw); }
          break;
        case 256 + 18: // window_style window flags operation (0 set,1 or,2 and,3 xor)
          { var yw = self.v6win(op0), fl = op1 & 0xffff, oper = (op2 === undefined ? 0 : op2 & 3), cur = self.windows[yw][10];
            self.windows[yw][10] = (oper === 1 ? cur | fl : oper === 2 ? cur & fl : oper === 3 ? cur ^ fl : fl) & 0xffff;
            if (self.windowChanged) self.windowChanged(yw); }
          break;
        case 256 + 8: // set_margins left right window
          { var gw2 = self.v6win(op2 === undefined ? 0xfffd : op2); self.windows[gw2][6] = op0 & 0xffff; self.windows[gw2][7] = op1 & 0xffff;
            if (self.windowChanged) self.windowChanged(gw2); }
          break;
        case 256 + 20: // scroll_window window pixels (signed; +up). Scrolling changes no geometry,
          // so the door needs the signed pixel delta directly (no model property to re-read).
          { var cw = self.v6win(op0); if (self.scrollWindow) self.scrollWindow(cw, op1 === undefined ? 0 : op1); }
          break;
        case 256 + 5: // draw_picture pic y x  (hook takes x,y)
          if (self.drawPicture) self.drawPicture(op0 & 0xffff, op2 === undefined ? 0 : op2 & 0xffff, op1 === undefined ? 0 : op1 & 0xffff, self.curwin);
          break;
        case 256 + 7: // erase_picture pic y x
          if (self.erasePicture) self.erasePicture(op0 & 0xffff, op2 === undefined ? 0 : op2 & 0xffff, op1 === undefined ? 0 : op1 & 0xffff, self.curwin);
          break;
        case 256 + 6: { // picture_data pic array ?(label): pic 0 = catalogue (count, release); else (h, w)
          var pinfo = self.pictureInfo ? self.pictureInfo(op0 & 0xffff) : null;
          if (op1 && pinfo) {
            if ((op0 & 0xffff) === 0) {
              self.put(op1 & 0xffff, (pinfo.count || 0) & 0xffff);
              self.put((op1 + 2) & 0xffff, (pinfo.release || 0) & 0xffff);
            } else {
              self.put(op1 & 0xffff, pinfo.height & 0xffff);
              self.put((op1 + 2) & 0xffff, pinfo.width & 0xffff);
            }
          }
          predicate(!!pinfo);
          break;
        }
        case 256 + 13: // set_true_colour fg bg (no-op at engine level; door owns palette)
          break;
        case 256 + 21: // pop_stack items (stack): user stack at op1 gives the slots back; else discard from ds
          { var pn = op0 & 0xffff;
            if (op1) self.put(op1 & 0xffff, (self.getu(op1 & 0xffff) + pn) & 0xffff);
            else while (pn-- > 0) ds.pop(); }
          break;
        case 256 + 24: // push_stack value (stack) ?(label): user stack at op1 fills top slot down; branch true if room
          if (op1) {
            var us = op1 & 0xffff, free = self.getu(us);
            if (free === 0) predicate(false);
            else { self.put((us + 2 * free) & 0xffff, op0 & 0xffff); self.put(us, (free - 1) & 0xffff); predicate(true); }
          } else { ds.push(op0); predicate(true); }
          break;
        case 256 + 26: // print_form: print a table captured by @output_stream 3 -- word 0 is the CHARACTER
                       // count, then that many ZSCII chars (same layout this engine's stream-3 writes).
                       // (The old code read word 0 as a row COUNT and looped, dumping megabytes of memory
                       // when the table held raw text -- the Arthur "beg your pardon? + garbage" bug.)
          { var pf = op0 & 0xffff, ln6 = self.getu(pf), s6 = "", c6, lim = self.mem.length;
            for (c6 = 0; c6 < ln6 && (pf + 2 + c6) < lim; c6++) s6 += String.fromCharCode(self.mem[pf + 2 + c6]);
            self.genPrint(s6); }
          break;
        case 256 + 28: // picture_table table: preload hint -> no-op (door has the cache)
          break;
        case 256 + 27: // make_menu number table ?(label): menus unsupported -> branch false
          predicate(false);
          break;
        case 256 + 22: // read_mouse array: mouse deferred -> leave array untouched
          break;
        case 256 + 23: // mouse_window window: mouse deferred -> no-op
          break;
        case 256 + 29: // buffer_screen mode -> (result): report 0 (no deferred buffering)
          store(0);
          break;
        default:
          throw new Error("JSZM: Invalid Z-machine opcode");
      }
      return 0;
    };

    // Main loop
    for (;;) { if (step()) return; }
  },
  __xargs: null,
  __interruptResult: 0,   // captured return value of the last interrupt routine (runInterrupt)
  __printed: false,       // set by genPrint on real screen output; sampled around runInterrupt
  save: function() { return false; },
  undoSlot: null,
  savedFlags: 0,
  selfInsertingBreaks: null,
  serial: null,
  serialize: function (ds, cs, pc) {
    return new Uint8Array(quetzal.write({
      mem: this.mem, orig: this.memInit, dynsize: this.getu(14),
      pc: pc, cs: cs, ds: ds
    }));
  },
  screen: null,
  setCursor: null,     // (row, col)        -- v4+ @set_cursor: move the upper-window cursor
  getCursor: null,     // () -> {row, col}  -- v4+ @get_cursor: report the upper-window cursor
  eraseWindow: null,   // (win)             -- v4+ @erase_window (-1 whole screen, -2 keep split)
  eraseLine: null,     // (val)             -- v4+ @erase_line (1 = cursor to end of line)
  bufferMode: null,    // (flag)            -- v4+ @buffer_mode: lower-window word-wrap on/off
  split: null,
  statusType: null,
  updateStatusLine: null,
  verify: function() {
    var plenth = this.getu(26);
    var pchksm = this.getu(28);
    var i = 64;
    while (i < plenth * this.vp.lenDiv) pchksm = (pchksm - this.memInit[i++]) & 65535;
    return !pchksm;
  },
  vocabulary: null,
  zorkid: null
};

JSZM.version = JSZM_Version;

try {
  if (module && module.exports) module.exports = JSZM;
} catch (e) {}
