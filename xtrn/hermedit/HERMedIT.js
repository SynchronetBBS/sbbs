"use strict";
(function() {
  // build/core/dropfiles.js
  function emptyMeta() {
    return { from: "", to: "", subject: "", area: "", privateMsg: false, charset: "", source: "none" };
  }
  function parseEditorInf(lines) {
    var m = emptyMeta();
    m.source = "editor.inf";
    m.subject = lines.length > 0 ? String(lines[0]) : "";
    m.to = lines.length > 1 ? String(lines[1]) : "";
    m.from = lines.length > 3 ? String(lines[3]) : "";
    return m;
  }
  function parseMsgInf(lines) {
    var m = emptyMeta();
    m.source = "msginf";
    m.from = lines.length > 0 ? String(lines[0]) : "";
    m.to = lines.length > 1 ? String(lines[1]) : "";
    m.subject = lines.length > 2 ? String(lines[2]) : "";
    m.area = lines.length > 4 ? String(lines[4]) : "";
    m.privateMsg = lines.length > 5 && String(lines[5]).toUpperCase() === "YES";
    if (lines.length > 7) {
      var cs = String(lines[7]).toUpperCase();
      if (cs === "UTF-8" || cs === "CP437")
        m.charset = cs;
    }
    return m;
  }
  function buildResultEd(subject, editorIdent) {
    return "0\r\n" + subject + "\r\n" + editorIdent + "\r\n";
  }

  // build/core/cp437.js
  var CP437_UNICODE = [
    32,
    9786,
    9787,
    9829,
    9830,
    9827,
    9824,
    8226,
    9688,
    9675,
    9689,
    9794,
    9792,
    9834,
    9835,
    9788,
    9658,
    9668,
    8597,
    8252,
    182,
    167,
    9644,
    8616,
    8593,
    8595,
    8594,
    8592,
    8735,
    8596,
    9650,
    9660,
    32,
    33,
    34,
    35,
    36,
    37,
    38,
    39,
    40,
    41,
    42,
    43,
    44,
    45,
    46,
    47,
    48,
    49,
    50,
    51,
    52,
    53,
    54,
    55,
    56,
    57,
    58,
    59,
    60,
    61,
    62,
    63,
    64,
    65,
    66,
    67,
    68,
    69,
    70,
    71,
    72,
    73,
    74,
    75,
    76,
    77,
    78,
    79,
    80,
    81,
    82,
    83,
    84,
    85,
    86,
    87,
    88,
    89,
    90,
    91,
    92,
    93,
    94,
    95,
    96,
    97,
    98,
    99,
    100,
    101,
    102,
    103,
    104,
    105,
    106,
    107,
    108,
    109,
    110,
    111,
    112,
    113,
    114,
    115,
    116,
    117,
    118,
    119,
    120,
    121,
    122,
    123,
    124,
    125,
    126,
    8962,
    199,
    252,
    233,
    226,
    228,
    224,
    229,
    231,
    234,
    235,
    232,
    239,
    238,
    236,
    196,
    197,
    201,
    230,
    198,
    244,
    246,
    242,
    251,
    249,
    255,
    214,
    220,
    162,
    163,
    165,
    8359,
    402,
    225,
    237,
    243,
    250,
    241,
    209,
    170,
    186,
    191,
    8976,
    172,
    189,
    188,
    161,
    171,
    187,
    9617,
    9618,
    9619,
    9474,
    9508,
    9569,
    9570,
    9558,
    9557,
    9571,
    9553,
    9559,
    9565,
    9564,
    9563,
    9488,
    9492,
    9524,
    9516,
    9500,
    9472,
    9532,
    9566,
    9567,
    9562,
    9556,
    9577,
    9574,
    9568,
    9552,
    9580,
    9575,
    9576,
    9572,
    9573,
    9561,
    9560,
    9554,
    9555,
    9579,
    9578,
    9496,
    9484,
    9608,
    9604,
    9612,
    9616,
    9600,
    945,
    223,
    915,
    960,
    931,
    963,
    181,
    964,
    934,
    920,
    937,
    948,
    8734,
    966,
    949,
    8745,
    8801,
    177,
    8805,
    8804,
    8992,
    8993,
    247,
    8776,
    176,
    8729,
    183,
    8730,
    8319,
    178,
    9632,
    160
  ];
  function encodeUtf8(cp) {
    if (cp < 128)
      return String.fromCharCode(cp);
    if (cp < 2048) {
      return String.fromCharCode(192 | cp >> 6, 128 | cp & 63);
    }
    if (cp < 65536) {
      return String.fromCharCode(224 | cp >> 12, 128 | cp >> 6 & 63, 128 | cp & 63);
    }
    return String.fromCharCode(240 | cp >> 18, 128 | cp >> 12 & 63, 128 | cp >> 6 & 63, 128 | cp & 63);
  }
  function utf8ToCp437(s, fallback) {
    var fb = fallback === void 0 ? "?" : fallback;
    var out = "";
    var lost = 0;
    var i = 0;
    while (i < s.length) {
      var b = s.charCodeAt(i) & 255;
      var cp = -1;
      var len = 1;
      if (b < 128) {
        cp = b;
      } else if (b >= 192 && b < 224 && i + 1 < s.length) {
        cp = (b & 31) << 6 | s.charCodeAt(i + 1) & 63;
        len = 2;
      } else if (b >= 224 && b < 240 && i + 2 < s.length) {
        cp = (b & 15) << 12 | (s.charCodeAt(i + 1) & 63) << 6 | s.charCodeAt(i + 2) & 63;
        len = 3;
      } else if (b >= 240 && i + 3 < s.length) {
        cp = (b & 7) << 18 | (s.charCodeAt(i + 1) & 63) << 12 | (s.charCodeAt(i + 2) & 63) << 6 | s.charCodeAt(i + 3) & 63;
        len = 4;
      }
      i += len;
      if (cp < 0) {
        out += fb;
        lost++;
        continue;
      }
      if (cp < 128) {
        out += String.fromCharCode(cp);
        continue;
      }
      var mapped = -1;
      for (var c = 128; c < 256; c++) {
        if (CP437_UNICODE[c] === cp) {
          mapped = c;
          break;
        }
      }
      if (mapped < 0) {
        out += fb;
        lost++;
      } else {
        out += String.fromCharCode(mapped);
      }
    }
    return { text: out, lost: lost };
  }
  function displayChar(code2, utf8Terminal) {
    var b = code2 & 255;
    if (utf8Terminal)
      return encodeUtf8(CP437_UNICODE[b]);
    if (b < 32 || b === 127)
      b = 32;
    return String.fromCharCode(b);
  }

  // build/core/tdf.js
  var OUTLINE_FONT = 0;
  var COLOR_FONT = 2;
  var NUM_CHARS = 94;
  var CHARLIST = "!\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~";
  var LIGHTGRAY = 7;
  var MAGIC = [19, 84, 104, 101, 68, 114, 97, 119, 32, 70, 79, 78, 84, 83, 32, 102, 105, 108, 101, 26];
  var OUTLINE_SUB = {
    65: 205,
    66: 196,
    67: 179,
    68: 186,
    69: 213,
    70: 187,
    71: 214,
    72: 191,
    73: 200,
    74: 190,
    75: 192,
    76: 189,
    77: 181,
    78: 199,
    79: 32,
    64: 32,
    38: 38
  };
  function byte(s, i) {
    return s.charCodeAt(i) & 255;
  }
  function parseTdf(data) {
    if (data.length < 233)
      return null;
    for (var m = 0; m < MAGIC.length; m++) {
      if (byte(data, m) !== MAGIC[m])
        return null;
    }
    var namelen = byte(data, 24);
    var name = "";
    for (var n = 0; n < namelen && n < 16; n++)
      name += String.fromCharCode(byte(data, 25 + n));
    var font = {
      name: name,
      fonttype: byte(data, 41),
      spacing: byte(data, 42),
      height: 0,
      charlist: [],
      glyphs: []
    };
    for (var c = 0; c < NUM_CHARS; c++) {
      var o = 45 + c * 2;
      font.charlist.push(byte(data, o) | byte(data, o + 1) << 8);
    }
    var glyphData = data.substring(233);
    for (var h = 0; h < NUM_CHARS; h++) {
      var off = font.charlist[h];
      if (off !== 65535 && off + 1 < glyphData.length) {
        var gh = byte(glyphData, off + 1);
        if (gh > font.height)
          font.height = gh;
      }
    }
    for (var g = 0; g < NUM_CHARS; g++) {
      font.glyphs.push(font.charlist[g] !== 65535 ? parseGlyph(g, font, glyphData) : null);
    }
    return font;
  }
  function parseGlyph(idx, font, glyphData) {
    var off = font.charlist[idx];
    if (off + 1 >= glyphData.length)
      return null;
    var width = byte(glyphData, off);
    var glyph = { width: width, height: byte(glyphData, off + 1), cell: [] };
    var size = width * font.height;
    for (var s = 0; s < size; s++)
      glyph.cell.push({ ch: 32, color: 0 });
    var p = off + 2;
    var row = 0;
    var col = 0;
    while (p < glyphData.length && byte(glyphData, p) !== 0) {
      var ch = byte(glyphData, p++);
      if (ch === 13) {
        row++;
        col = 0;
        continue;
      }
      if (p >= glyphData.length)
        break;
      var color = font.fonttype === COLOR_FONT ? byte(glyphData, p++) : LIGHTGRAY;
      if (ch < 32)
        ch = 32;
      var ci = row * width + col;
      if (ci < size) {
        var sub = OUTLINE_SUB[ch];
        var fc = font.fonttype === OUTLINE_FONT && sub !== void 0 ? sub : ch;
        glyph.cell[ci].ch = fc & 255;
        glyph.cell[ci].color = color;
        col++;
      }
    }
    return glyph;
  }
  function lookup(ch, font) {
    var code2 = ch.charCodeAt(0);
    for (var i = 0; i < NUM_CHARS; i++) {
      if (CHARLIST.charCodeAt(i) === code2)
        return font.charlist[i] !== 65535 ? i : -1;
    }
    var up = ch.toUpperCase().charCodeAt(0);
    if (up !== code2) {
      for (var j = 0; j < NUM_CHARS; j++) {
        if (CHARLIST.charCodeAt(j) === up)
          return font.charlist[j] !== 65535 ? j : -1;
      }
    }
    return -1;
  }
  function renderTdf(font, text) {
    var h = font.height;
    var rows = [];
    for (var r = 0; r < h; r++) {
      var line = [];
      for (var ci = 0; ci < text.length; ci++) {
        var gi = lookup(text.charAt(ci), font);
        if (gi === -1) {
          line.push({ ch: 32, color: 0 });
          if (ci < text.length - 1)
            for (var sp = 0; sp < font.spacing; sp++)
              line.push({ ch: 32, color: 0 });
          continue;
        }
        var glyph = font.glyphs[gi];
        for (var col = 0; col < glyph.width; col++) {
          var cell = glyph.cell[r * glyph.width + col];
          if (cell)
            line.push({ ch: cell.ch, color: cell.color });
          else
            line.push({ ch: 32, color: 0 });
        }
        if (ci < text.length - 1)
          for (var s2 = 0; s2 < font.spacing; s2++)
            line.push({ ch: 32, color: 0 });
      }
      rows.push(line);
    }
    var bounds = [];
    var cx = 0;
    for (var b = 0; b < text.length; b++) {
      var bgi = lookup(text.charAt(b), font);
      var gw = bgi === -1 ? 1 : font.glyphs[bgi].width;
      bounds.push({ start: cx, width: gw });
      cx += gw;
      if (b < text.length - 1)
        cx += font.spacing;
    }
    return { width: rows.length > 0 ? rows[0].length : 0, height: h, rows: rows, charBounds: bounds };
  }
  function glyphWidthIn(font, ch) {
    var gi = lookup(ch, font);
    return gi === -1 ? 1 : font.glyphs[gi].width;
  }
  function measureStyledSpan(text, fonts, from, to) {
    var w = 0;
    for (var i = from; i < to; i++) {
      var f = fonts[i];
      w += glyphWidthIn(f, text.charAt(i));
      if (i < to - 1)
        w += f.spacing;
    }
    return w;
  }
  function renderStyledSpan(text, fonts, from, to, minHeight) {
    var h = minHeight;
    for (var i = from; i < to; i++) {
      var fh = fonts[i].height;
      if (fh > h)
        h = fh;
    }
    var rows = [];
    for (var r = 0; r < h; r++)
      rows.push([]);
    var bounds = [];
    var cx = 0;
    for (var ci = from; ci < to; ci++) {
      var font = fonts[ci];
      var isColor = font.fonttype === COLOR_FONT;
      var yOff = h - font.height;
      var gi = lookup(text.charAt(ci), font);
      var gw = gi === -1 ? 1 : font.glyphs[gi].width;
      var spacing = ci < to - 1 ? font.spacing : 0;
      for (var y = 0; y < h; y++) {
        var row = rows[y];
        for (var x = 0; x < gw + spacing; x++) {
          var cell = { ch: 32, color: 0 };
          if (gi !== -1 && x < gw && y >= yOff) {
            var glyph = font.glyphs[gi];
            var src = glyph.cell[(y - yOff) * gw + x];
            if (src) {
              cell.ch = src.ch;
              cell.color = src.color;
              if (isColor)
                cell.cf = true;
            }
          }
          row.push(cell);
        }
      }
      bounds.push({ start: cx, width: gw });
      cx += gw + spacing;
    }
    return { width: cx, height: h, rows: rows, charBounds: bounds };
  }
  function layoutTdfWpStyled(text, fonts, maxWidth, lineGap, defaultFont) {
    var lines = [];
    var width = 0;
    var paraStart = 0;
    while (paraStart <= text.length) {
      var nl = text.indexOf("\n", paraStart);
      var paraEnd = nl === -1 ? text.length : nl;
      layoutStyledParagraph(text, fonts, paraStart, paraEnd, maxWidth, defaultFont, lines);
      if (nl === -1)
        break;
      paraStart = nl + 1;
      if (paraStart > text.length)
        break;
    }
    if (lines.length === 0) {
      lines.push({ startIdx: 0, text: "", render: renderStyledSpan(text, fonts, 0, 0, defaultFont.height), yTop: 0 });
    }
    var yCur = 0;
    for (var y = 0; y < lines.length; y++) {
      var ln = lines[y];
      ln.yTop = yCur;
      yCur += ln.render.height + lineGap;
      if (ln.render.width > width)
        width = ln.render.width;
    }
    var height = yCur - lineGap > 0 ? yCur - lineGap : lines[0].render.height;
    return { lines: lines, width: width, height: height, lineHeight: lines[0].render.height + lineGap };
  }
  function layoutStyledParagraph(text, fonts, start, end, maxWidth, defaultFont, out) {
    var mkLine = function(from, to) {
      var hintFont = fonts[from] !== void 0 ? fonts[from] : fonts[from - 1] !== void 0 ? fonts[from - 1] : defaultFont;
      return { startIdx: from, text: text.substring(from, to), render: renderStyledSpan(text, fonts, from, to, hintFont.height), yTop: 0 };
    };
    if (start === end) {
      out.push(mkLine(start, end));
      return;
    }
    var lineStart = start;
    var lineEnd = start;
    var pos = start;
    while (pos < end) {
      var sp = text.indexOf(" ", pos);
      if (sp >= end)
        sp = -1;
      var wordEnd = sp === -1 ? end : sp;
      if (lineEnd > lineStart && measureStyledSpan(text, fonts, lineStart, wordEnd) > maxWidth) {
        out.push(mkLine(lineStart, lineEnd));
        lineStart = pos;
      }
      lineEnd = wordEnd;
      pos = sp === -1 ? end : sp + 1;
    }
    out.push(mkLine(lineStart, lineEnd));
  }
  function tdfWpHitTest(layout, localX, localY) {
    if (layout.lines.length === 0)
      return 0;
    var li = 0;
    for (var l = 0; l < layout.lines.length; l++) {
      if (layout.lines[l].yTop <= localY)
        li = l;
    }
    var ln = layout.lines[li];
    var bounds = ln.render.charBounds;
    for (var i = 0; i < bounds.length; i++) {
      var b = bounds[i];
      if (localX < b.start + b.width / 2)
        return ln.startIdx + i;
    }
    return ln.startIdx + ln.text.length;
  }
  function tdfWpCaretXY(layout, caretIdx) {
    for (var i = layout.lines.length - 1; i >= 0; i--) {
      var ln = layout.lines[i];
      if (caretIdx >= ln.startIdx) {
        var within = caretIdx - ln.startIdx;
        if (within > ln.text.length)
          within = ln.text.length;
        var x = within >= ln.render.charBounds.length ? ln.render.width : ln.render.charBounds[within].start;
        return { x: x, y: ln.yTop, line: i };
      }
    }
    return { x: 0, y: 0, line: 0 };
  }

  // build/host/sbbs.js
  var EDITOR_IDENT = "HERMedIT v0.1";
  function readAllLines(path) {
    var f = new File(path);
    if (!f.open("r"))
      return null;
    var lines = f.readAll();
    f.close();
    return lines;
  }
  function readRaw(path) {
    var f = new File(path);
    if (!f.open("rb"))
      return null;
    var data = f.read();
    f.close();
    return data === null ? "" : data;
  }
  function loadSession() {
    var bodyPath = argc > 0 ? String(argv[0]) : system.temp_dir + "INPUT.MSG";
    var meta = emptyMeta();
    var msginfPath = file_getcase(system.node_dir + "msginf");
    if (msginfPath !== void 0) {
      var ml = readAllLines(msginfPath);
      if (ml !== null)
        meta = parseMsgInf(ml);
    } else {
      var infPath;
      while ((infPath = file_getcase(system.node_dir + "editor.inf")) !== void 0) {
        var il = readAllLines(infPath);
        if (il !== null) {
          var parsed = parseEditorInf(il);
          parsed.subject = strip_ctrl(parsed.subject);
          parsed.to = strip_ctrl(parsed.to);
          parsed.from = strip_ctrl(parsed.from);
          meta = parsed;
        }
        if (!file_remove(infPath))
          break;
      }
    }
    var utf8 = sessionIsUtf8(meta);
    var quoteLines = [];
    var quotesPath = file_getcase(system.node_dir + "quotes.txt");
    if (quotesPath !== void 0) {
      var q = readRaw(quotesPath);
      if (q !== null && q.length > 0) {
        if (utf8)
          q = utf8ToCp437(q).text;
        q = strip_ansi(q);
        q = strip_ctrl_a(q);
        q = q.replace(/\r\n/g, "\n").replace(/\r/g, "\n");
        quoteLines = q.split("\n");
        while (quoteLines.length > 0 && quoteLines[quoteLines.length - 1] === "")
          quoteLines.pop();
      }
    }
    var sourceText = "";
    if (quotesPath === void 0 && file_exists(bodyPath)) {
      var src = readRaw(bodyPath);
      if (src !== null && src.length > 0) {
        if (utf8)
          src = utf8ToCp437(src).text;
        sourceText = strip_ctrl_a(strip_ansi(src));
      }
    }
    return {
      bodyPath: bodyPath,
      meta: meta,
      sourceText: sourceText,
      quoteLines: quoteLines,
      utf8: utf8
    };
  }
  function sessionIsUtf8(meta) {
    if (meta.charset === "UTF-8")
      return true;
    if (meta.charset === "CP437")
      return false;
    return Boolean(console.term_supports(USER_UTF8));
  }
  function saveMessage(session, bodyCp437, subject) {
    var f = new File(session.bodyPath);
    if (!f.open("wb")) {
      log(LOG_ERR, "future_edit: cannot open body file " + session.bodyPath);
      return false;
    }
    f.write(bodyCp437);
    f.close();
    var stale;
    while ((stale = file_getcase(system.node_dir + "result.ed")) !== void 0) {
      if (!file_remove(stale))
        break;
    }
    var r = new File(system.node_dir + "result.ed");
    if (r.open("wb")) {
      r.write(buildResultEd(subject, EDITOR_IDENT));
      r.close();
    }
    return true;
  }
  var savedSysStatus = 0;
  var savedCtrlkey = 0;
  var savedStatus = 0;
  var savedMouseMode = 0;
  function initTerminal() {
    savedSysStatus = bbs.sys_status;
    savedCtrlkey = console.ctrlkey_passthru;
    savedStatus = console.status;
    savedMouseMode = console.mouse_mode;
    js.on_exit("bbs.sys_status = " + bbs.sys_status);
    js.on_exit("console.ctrlkey_passthru = " + Number(console.ctrlkey_passthru));
    js.on_exit("console.status = " + console.status);
    js.on_exit("console.mouse_mode = false");
    js.on_exit("console.write('\\x1b[0m')");
    bbs.sys_status &= ~SS_PAUSEON;
    bbs.sys_status |= SS_PAUSEOFF;
    console.ctrlkey_passthru = "+ACDGKLNOPQRSTUVWXYZ_";
    if (typeof console.getdimensions === "function")
      console.getdimensions();
    var mouse = false;
    console.status |= CON_MOUSE_CLK_PASSTHRU | CON_MOUSE_REL_PASSTHRU;
    console.mouse_mode = MOUSE_MODE_BTN | MOUSE_MODE_EXT;
    mouse = true;
    return {
      cols: console.screen_columns,
      rows: console.screen_rows,
      utf8: Boolean(console.term_supports(USER_UTF8)),
      mouse: mouse
    };
  }
  var lastDimsProbe = 0;
  function terminalSize() {
    var now = (/* @__PURE__ */ new Date()).getTime();
    if (now - lastDimsProbe >= 3e3) {
      lastDimsProbe = now;
      try {
        if (typeof console.ansi_getdims === "function")
          console.ansi_getdims();
      } catch (e) {
      }
    }
    return { cols: console.screen_columns, rows: console.screen_rows };
  }
  function restoreTerminal() {
    console.mouse_mode = savedMouseMode;
    console.status = savedStatus;
    console.ctrlkey_passthru = savedCtrlkey;
    bbs.sys_status = savedSysStatus;
    console.write("\x1B[0m");
    console.attributes = 7;
  }
  function fontsDir() {
    return js.exec_dir + "fonts/";
  }
  function createSbbsFontProvider() {
    var cache = {};
    var indexCache = null;
    return {
      list: function() {
        if (indexCache !== null)
          return indexCache;
        indexCache = [];
        var raw = readRaw(fontsDir() + "tdfont_index.json");
        if (raw === null) {
          log(LOG_ERR, "future_edit: font index not found in " + fontsDir());
          return indexCache;
        }
        try {
          var arr = JSON.parse(raw);
          if (Object.prototype.toString.call(arr) === "[object Array]")
            indexCache = arr;
        } catch (e) {
          log(LOG_ERR, "future_edit: bad font index: " + String(e));
        }
        return indexCache;
      },
      load: function(name) {
        if (Object.prototype.hasOwnProperty.call(cache, name))
          return cache[name];
        var safe = String(name).replace(/[^a-zA-Z0-9_\-!#]/g, "");
        var data = readRaw(system.ctrl_dir + "tdfonts/" + safe + ".tdf");
        var font = data === null ? null : parseTdf(data);
        cache[name] = font;
        return font;
      }
    };
  }

  // build/host/input.js
  var ESC_CONT_TIMEOUT = 60;
  var MOUSE_TRACE = false;
  function mouseEvent(button, x, y, press, motion) {
    var wheel = 0;
    if (button === 64)
      wheel = -1;
    else if (button === 65)
      wheel = 1;
    var ev = {
      type: "mouse",
      x: x,
      y: y,
      button: wheel !== 0 ? 0 : button & 3,
      press: wheel !== 0 ? false : press,
      release: wheel !== 0 ? false : !press,
      motion: motion,
      wheel: wheel
    };
    if (MOUSE_TRACE) {
      debugLog("mouse raw=" + button + " x=" + x + " y=" + y + (ev.press ? " press" : "") + (ev.release ? " release" : "") + (ev.motion ? " motion" : "") + (ev.wheel !== 0 ? " wheel=" + ev.wheel : ""));
    }
    return ev;
  }
  var FKEY_TILDE = {
    "11": "F1",
    "12": "F2",
    "13": "F3",
    "14": "F4",
    "15": "F5",
    "17": "F6",
    "18": "F7",
    "19": "F8",
    "20": "F9",
    "21": "F10",
    "23": "F11",
    "24": "F12"
  };
  var FKEY_SS3 = {
    // Standard SS3 function keys.
    P: "F1",
    Q: "F2",
    R: "F3",
    S: "F4",
    // VT100+/HP extension (PuTTY "VT100+" mode and several BBS clients
    // continue the run past F4): SS3 T..Z for F5-F11.
    T: "F5",
    U: "F6",
    V: "F7",
    W: "F8",
    X: "F9",
    Y: "F10",
    Z: "F11"
  };
  function debugLog(msg) {
    try {
      log(LOG_DEBUG, "future_edit input: " + msg);
    } catch (e) {
    }
  }
  var KEY_UP = "";
  var KEY_DOWN = "\n";
  var KEY_RIGHT = "";
  var KEY_LEFT = "";
  var KEY_HOME = "";
  var KEY_END = "";
  var KEY_INSERT = "";
  var KEY_DEL = "\x7F";
  var KEY_PAGEUP = "";
  var KEY_PAGEDN = "";
  var NAV_TILDE = {
    "1": KEY_HOME,
    "2": KEY_INSERT,
    "3": KEY_DEL,
    "4": KEY_END,
    "5": KEY_PAGEUP,
    "6": KEY_PAGEDN,
    "7": KEY_HOME,
    "8": KEY_END
  };
  var NAV_LETTER = {
    A: KEY_UP,
    B: KEY_DOWN,
    C: KEY_RIGHT,
    D: KEY_LEFT,
    H: KEY_HOME,
    F: KEY_END
  };
  function readInput(timeoutMs) {
    var key = console.inkey(K_NONE, timeoutMs);
    if (key === "" || key === null || key === void 0)
      return { type: "none" };
    if (key !== "\x1B")
      return { type: "key", key: key };
    var seq = "";
    var next = console.inkey(K_NONE, ESC_CONT_TIMEOUT);
    if (next === "" || next === void 0 || next === null)
      return { type: "key", key: "\x1B" };
    if (next === "O") {
      var fin = console.inkey(K_NONE, ESC_CONT_TIMEOUT);
      var name = FKEY_SS3[fin];
      if (name !== void 0)
        return { type: "key", key: name };
      if (fin === "" || fin === void 0 || fin === null)
        return { type: "key", key: "\x1B" };
      debugLog("unknown SS3 key: ESC O " + fin);
      return { type: "none" };
    }
    if (next !== "[") {
      console.ungetstr(next);
      return { type: "key", key: "\x1B" };
    }
    var c = console.inkey(K_NONE, ESC_CONT_TIMEOUT);
    if (c === "M") {
      var b = console.inkey(K_NONE, ESC_CONT_TIMEOUT);
      var xc = console.inkey(K_NONE, ESC_CONT_TIMEOUT);
      var yc = console.inkey(K_NONE, ESC_CONT_TIMEOUT);
      if (b === "" || xc === "" || yc === "")
        return { type: "none" };
      var bv = ascii(b) - 32;
      var x = ascii(xc) - 32;
      var y = ascii(yc) - 32;
      var motion = (bv & 32) !== 0;
      var btn = bv & 195;
      if (btn === 3)
        return mouseEvent(0, x, y, false, motion);
      return mouseEvent(btn, x, y, true, motion);
    }
    seq = c;
    var guard = 0;
    while (guard++ < 24) {
      var last = seq.charAt(seq.length - 1);
      if (seq.length > 0 && (last === "~" || last >= "A" && last <= "Z" || last >= "a" && last <= "z"))
        break;
      var nc = console.inkey(K_NONE, ESC_CONT_TIMEOUT);
      if (nc === "" || nc === void 0 || nc === null)
        break;
      seq += nc;
    }
    var ev = classifyCsi(seq);
    if (ev.type === "none" && seq.length > 0)
      debugLog("unknown CSI sequence: ESC [ " + seq);
    return ev;
  }
  function classifyCsi(seq) {
    var m = seq.match(/^<([0-9]+);([0-9]+);([0-9]+)([Mm])$/);
    if (m !== null) {
      var sb = parseInt(m[1], 10);
      var sx = parseInt(m[2], 10);
      var sy = parseInt(m[3], 10);
      var sMotion = (sb & 32) !== 0;
      var sBtn = sb & 195;
      return mouseEvent(sBtn, sx, sy, m[4] === "M", sMotion);
    }
    var t = seq.match(/^([0-9]+)~$/);
    if (t !== null) {
      var nav = NAV_TILDE[t[1]];
      if (nav !== void 0)
        return { type: "key", key: nav };
      var fname = FKEY_TILDE[t[1]];
      if (fname !== void 0)
        return { type: "key", key: fname };
    }
    if (seq.length === 1) {
      var letter = NAV_LETTER[seq];
      if (letter !== void 0)
        return { type: "key", key: letter };
      if (seq === "Z")
        return { type: "key", key: "STAB" };
    }
    var mk = seq.match(/^([0-9]+);([0-9]+)u$/);
    var xk = seq.match(/^27;([0-9]+);([0-9]+)~$/);
    var mCode = -1;
    var mMods = 0;
    if (mk !== null) {
      mCode = parseInt(mk[1], 10);
      mMods = parseInt(mk[2], 10);
    } else if (xk !== null) {
      mCode = parseInt(xk[2], 10);
      mMods = parseInt(xk[1], 10);
    }
    if (mCode >= 0 && (mMods - 1 & 4) !== 0) {
      if (mCode === 44)
        return { type: "key", key: "C-," };
      if (mCode === 46)
        return { type: "key", key: "C-." };
      if (mCode === 47)
        return { type: "key", key: "C-/" };
    }
    return { type: "none" };
  }

  // build/core/attr.js
  var BLACK = 0;
  var BLUE = 1;
  var GREEN = 2;
  var CYAN = 3;
  var RED = 4;
  var MAGENTA = 5;
  var BROWN = 6;
  var LIGHTGRAY2 = 7;
  var HIGH = 8;
  var BLINK = 128;
  var DEFAULT_ATTR = LIGHTGRAY2;
  function makeAttr(fg, bg, high, blink) {
    var a = fg & 7 | (bg & 7) << 4;
    if (high)
      a |= HIGH;
    if (blink)
      a |= BLINK;
    return a;
  }
  var CTRLA_FG = ["K", "B", "G", "C", "R", "M", "Y", "N"];
  var CTRLA_BG = ["0", "4", "2", "6", "1", "5", "3", "7"];
  function ctrlATransition(last, next) {
    if (last === next)
      return "";
    var s = "";
    var cur = last;
    if (!(next & BLINK) && cur & BLINK || !(next & HIGH) && cur & HIGH) {
      cur = LIGHTGRAY2;
      s += "N";
    }
    if (next & BLINK && !(cur & BLINK))
      s += "I";
    if (next & HIGH && !(cur & HIGH))
      s += "H";
    if ((next & 7) !== (cur & 7))
      s += "" + CTRLA_FG[next & 7];
    if ((next & 112) !== (cur & 112))
      s += "" + CTRLA_BG[next >> 4 & 7];
    return s;
  }
  function applyColorChannel(existing, brush, channel) {
    if (channel === "fg")
      return existing & 240 | brush & 15;
    if (channel === "bg")
      return existing & 15 | brush & 240;
    return brush & 255;
  }
  var ANSI_COLOR = [0, 4, 2, 6, 1, 5, 3, 7];
  function ansiFromAttr(attr) {
    var parts = "0";
    if (attr & HIGH)
      parts += ";1";
    if (attr & BLINK)
      parts += ";5";
    parts += ";3" + ANSI_COLOR[attr & 7];
    parts += ";4" + ANSI_COLOR[attr >> 4 & 7];
    return "\x1B[" + parts + "m";
  }

  // build/ui/screen.js
  var Screen = (
    /** @class */
    (function() {
      function Screen2(cols, rows, utf8, writer) {
        this.ch = [];
        this.attr = [];
        this.prevCh = [];
        this.prevAttr = [];
        this.prevValid = false;
        this.cursorX = 1;
        this.cursorY = 1;
        this.cursorVisible = true;
        this.cols = cols;
        this.rows = rows;
        this.utf8 = utf8;
        this.writer = writer;
        for (var i = 0; i < cols * rows; i++) {
          this.ch.push(32);
          this.attr.push(7);
          this.prevCh.push(32);
          this.prevAttr.push(7);
        }
      }
      Screen2.prototype.put = function(x, y, code2, attr) {
        if (x < 0 || y < 0 || x >= this.cols || y >= this.rows)
          return;
        var i = y * this.cols + x;
        this.ch[i] = code2 & 255;
        this.attr[i] = attr & 255;
      };
      Screen2.prototype.putStr = function(x, y, s, attr) {
        for (var i = 0; i < s.length; i++)
          this.put(x + i, y, s.charCodeAt(i), attr);
      };
      Screen2.prototype.fill = function(x, y, w, h, code2, attr) {
        for (var yy = y; yy < y + h; yy++) {
          for (var xx = x; xx < x + w; xx++)
            this.put(xx, yy, code2, attr);
        }
      };
      Screen2.prototype.hline = function(x, y, w, code2, attr) {
        for (var i = 0; i < w; i++)
          this.put(x + i, y, code2, attr);
      };
      Screen2.prototype.invalidate = function() {
        this.prevValid = false;
      };
      Screen2.prototype.resize = function(cols, rows) {
        this.cols = cols;
        this.rows = rows;
        this.ch = [];
        this.attr = [];
        this.prevCh = [];
        this.prevAttr = [];
        for (var i = 0; i < cols * rows; i++) {
          this.ch.push(32);
          this.attr.push(7);
          this.prevCh.push(32);
          this.prevAttr.push(7);
        }
        this.prevValid = false;
      };
      Screen2.prototype.flush = function() {
        var out = "";
        var lastAttr = -1;
        for (var y = 0; y < this.rows; y++) {
          var x = 0;
          while (x < this.cols) {
            var i = y * this.cols + x;
            var changed = !this.prevValid || this.ch[i] !== this.prevCh[i] || this.attr[i] !== this.prevAttr[i];
            if (!changed) {
              x++;
              continue;
            }
            out += "\x1B[" + (y + 1) + ";" + (x + 1) + "H";
            while (x < this.cols) {
              var j = y * this.cols + x;
              if (y === this.rows - 1 && x === this.cols - 1) {
                this.prevCh[j] = this.ch[j];
                this.prevAttr[j] = this.attr[j];
                x++;
                break;
              }
              var cChanged = !this.prevValid || this.ch[j] !== this.prevCh[j] || this.attr[j] !== this.prevAttr[j];
              if (!cChanged)
                break;
              var a = this.attr[j];
              if (a !== lastAttr) {
                out += ansiFromAttr(a);
                lastAttr = a;
              }
              out += displayChar(this.ch[j], this.utf8);
              this.prevCh[j] = this.ch[j];
              this.prevAttr[j] = a;
              x++;
            }
          }
        }
        out += "\x1B[" + this.cursorY + ";" + this.cursorX + "H";
        out += this.cursorVisible ? "\x1B[?25h" : "\x1B[?25l";
        this.prevValid = true;
        if (out.length > 0)
          this.writer(out);
      };
      return Screen2;
    })()
  );

  // build/core/std.js
  function clamp(v, lo, hi) {
    if (v < lo)
      return lo;
    if (v > hi)
      return hi;
    return v;
  }
  function hasOwn(obj, key) {
    return Object.prototype.hasOwnProperty.call(obj, key);
  }
  function objectKeys(obj) {
    var out = [];
    for (var k in obj) {
      if (hasOwn(obj, k))
        out.push(k);
    }
    return out;
  }

  // build/core/doc.js
  var UNDO_LIMIT = 200;
  function copyLine(l) {
    return { text: l.text, attr: l.attr.slice(0), hardcr: l.hardcr };
  }
  function copyLines(lines) {
    var out = [];
    for (var i = 0; i < lines.length; i++)
      out.push(copyLine(lines[i]));
    return out;
  }
  function copyFlows(flows) {
    var out = [];
    for (var i = 0; i < flows.length; i++) {
      var f = flows[i];
      out.push({ region: f.region === null ? null : { left: f.region.left, top: f.region.top, width: f.region.width, height: f.region.height, pre: f.region.pre, lang: f.region.lang }, lines: copyLines(f.lines) });
    }
    return out;
  }
  function regionsEqual(a, b) {
    if (a === null || b === null)
      return a === b;
    return a.left === b.left && a.top === b.top && a.width === b.width && a.height === b.height;
  }
  function copyArt(art) {
    var out = {};
    var keys = objectKeys(art);
    for (var i = 0; i < keys.length; i++) {
      var k = keys[i];
      var c = art[k];
      out[k] = { ch: c.ch, attr: c.attr };
    }
    return out;
  }
  function artKey(x, y) {
    return y + "," + x;
  }
  var Document = (
    /** @class */
    (function() {
      function Document2(width) {
        this.art = {};
        this.caret = { row: 0, col: 0 };
        this.curAttr = DEFAULT_ATTR;
        this.insertMode = true;
        this.dirty = false;
        this.flows = [{ region: null, lines: [{ text: "", attr: [], hardcr: true }] }];
        this.active = 0;
        this.undoStack = [];
        this.redoStack = [];
        this.lastOpTag = "";
        this.width = width;
      }
      Object.defineProperty(Document2.prototype, "lines", {
        /** The active flow's paragraph lines (editing operates here). */
        get: function() {
          return this.flows[this.active].lines;
        },
        set: function(v) {
          this.flows[this.active].lines = v;
        },
        enumerable: false,
        configurable: true
      });
      Object.defineProperty(Document2.prototype, "region", {
        /** The active flow's region: a box interior, or null for the body. */
        get: function() {
          return this.flows[this.active].region;
        },
        enumerable: false,
        configurable: true
      });
      Document2.prototype.loadText = function(text) {
        this.active = 0;
        this.lines = [];
        var raw = text.replace(/\r\n/g, "\n").replace(/\r/g, "\n").split("\n");
        if (raw.length > 1 && raw[raw.length - 1] === "")
          raw.pop();
        for (var i = 0; i < raw.length; i++) {
          var s = raw[i];
          var attr = [];
          for (var j = 0; j < s.length; j++)
            attr.push(DEFAULT_ATTR);
          this.lines.push({ text: s, attr: attr, hardcr: true });
          if (s.length > this.ew())
            this.rewrapParagraphAt(this.lines.length - 1);
        }
        if (this.lines.length === 0)
          this.lines.push({ text: "", attr: [], hardcr: true });
        this.caret = { row: 0, col: 0 };
        this.dirty = false;
        this.undoStack = [];
        this.redoStack = [];
      };
      Document2.prototype.ew = function() {
        var r = this.region;
        if (r === null)
          return this.width;
        return r.pre ? 1e9 : r.width;
      };
      Document2.prototype.markRegionCode = function(lang) {
        var r = this.region;
        if (r === null)
          return;
        this.pushUndo("");
        r.pre = true;
        r.lang = lang;
        this.dirty = true;
      };
      Document2.prototype.caretDocX = function() {
        return (this.region ? this.region.left : 0) + this.caret.col;
      };
      Document2.prototype.caretDocY = function() {
        return (this.region ? this.region.top : 0) + this.caret.row;
      };
      Document2.prototype.setRegion = function(region) {
        for (var i = 1; i < this.flows.length; i++) {
          if (regionsEqual(this.flows[i].region, region)) {
            this.active = i;
            this.clampCaret();
            return;
          }
        }
        this.flows.push({ region: { left: region.left, top: region.top, width: region.width, height: region.height }, lines: [{ text: "", attr: [], hardcr: true }] });
        this.active = this.flows.length - 1;
        this.caret = { row: 0, col: 0 };
      };
      Document2.prototype.clearRegion = function() {
        this.active = 0;
        this.clampCaret();
      };
      Document2.prototype.flowList = function() {
        return this.flows;
      };
      Document2.prototype.clampCaret = function() {
        if (this.caret.row >= this.lines.length)
          this.caret.row = this.lines.length - 1;
        if (this.caret.row < 0)
          this.caret.row = 0;
        var len = this.lines[this.caret.row].text.length;
        if (this.caret.col > len)
          this.caret.col = len;
        if (this.caret.col < 0)
          this.caret.col = 0;
      };
      Document2.prototype.detectBox = function(x, y) {
        if (this.artAt(x, y) !== null)
          return null;
        var left = -1;
        var right = -1;
        var top = -1;
        var bottom = -1;
        for (var lx = x - 1; lx >= 0; lx--) {
          if (this.artAt(lx, y) !== null) {
            left = lx;
            break;
          }
        }
        for (var rx = x + 1; rx < this.width; rx++) {
          if (this.artAt(rx, y) !== null) {
            right = rx;
            break;
          }
        }
        var maxY = this.maxArtRow();
        for (var ty = y - 1; ty >= 0; ty--) {
          if (this.artAt(x, ty) !== null) {
            top = ty;
            break;
          }
        }
        for (var by = y + 1; by <= maxY; by++) {
          if (this.artAt(x, by) !== null) {
            bottom = by;
            break;
          }
        }
        if (left < 0 || right < 0 || top < 0 || bottom < 0)
          return null;
        var iw = right - left - 1;
        var ih = bottom - top - 1;
        if (iw < 1 || ih < 1)
          return null;
        return { left: left + 1, top: top + 1, width: iw, height: ih };
      };
      Document2.prototype.snapshot = function() {
        return { flows: copyFlows(this.flows), active: this.active, art: copyArt(this.art), caret: { row: this.caret.row, col: this.caret.col } };
      };
      Document2.prototype.restore = function(s) {
        this.flows = copyFlows(s.flows);
        this.active = s.active < this.flows.length ? s.active : 0;
        this.art = copyArt(s.art);
        this.caret = { row: s.caret.row, col: s.caret.col };
      };
      Document2.prototype.pushUndo = function(tag) {
        if (tag !== "" && tag === this.lastOpTag)
          return;
        this.lastOpTag = tag;
        this.undoStack.push(this.snapshot());
        if (this.undoStack.length > UNDO_LIMIT)
          this.undoStack.shift();
        this.redoStack = [];
      };
      Document2.prototype.undo = function() {
        var s = this.undoStack.pop();
        if (!s)
          return false;
        this.redoStack.push(this.snapshot());
        this.restore(s);
        this.lastOpTag = "";
        this.dirty = true;
        return true;
      };
      Document2.prototype.redo = function() {
        var s = this.redoStack.pop();
        if (!s)
          return false;
        this.undoStack.push(this.snapshot());
        this.restore(s);
        this.lastOpTag = "";
        this.dirty = true;
        return true;
      };
      Document2.prototype.breakUndoGroup = function() {
        this.lastOpTag = "";
      };
      Document2.prototype.paragraphStart = function(row) {
        var p = row;
        while (p > 0 && !this.lines[p - 1].hardcr)
          p--;
        return p;
      };
      Document2.prototype.paragraphEnd = function(row) {
        var e = row;
        while (e < this.lines.length - 1 && !this.lines[e].hardcr)
          e++;
        return e;
      };
      Document2.prototype.flattenParagraph = function(start, end) {
        var text = "";
        var attr = [];
        for (var i = start; i <= end; i++) {
          var l = this.lines[i];
          text += l.text;
          for (var j = 0; j < l.attr.length; j++)
            attr.push(l.attr[j]);
        }
        return { text: text, attr: attr };
      };
      Document2.prototype.splitFlat = function(text, attr, hardcr) {
        var out = [];
        var pos = 0;
        var w = this.ew();
        while (text.length - pos > w) {
          var brk = -1;
          for (var i = pos + w - 1; i >= pos; i--) {
            if (text.charAt(i) === " ") {
              brk = i + 1;
              break;
            }
          }
          if (brk <= pos)
            brk = pos + w;
          out.push({ text: text.substring(pos, brk), attr: attr.slice(pos, brk), hardcr: false });
          pos = brk;
        }
        out.push({ text: text.substring(pos), attr: attr.slice(pos), hardcr: hardcr });
        return out;
      };
      Document2.prototype.caretToOffset = function(start) {
        var off = 0;
        for (var i = start; i < this.caret.row; i++)
          off += this.lines[i].text.length;
        return off + this.caret.col;
      };
      Document2.prototype.offsetToCaret = function(start, end, off) {
        for (var i = start; i <= end; i++) {
          var len = this.lines[i].text.length;
          if (off < len || i === end)
            return { row: i, col: clamp(off, 0, len) };
          off -= len;
        }
        return { row: end, col: this.lines[end].text.length };
      };
      Document2.prototype.rewrapParagraphAt = function(row) {
        var start = this.paragraphStart(row);
        var end = this.paragraphEnd(row);
        var caretInside = this.caret.row >= start && this.caret.row <= end;
        var off = caretInside ? this.caretToOffset(start) : 0;
        var flat = this.flattenParagraph(start, end);
        var hardcr = this.lines[end].hardcr;
        var repl = this.splitFlat(flat.text, flat.attr, hardcr);
        var args = [start, end - start + 1];
        for (var i = 0; i < repl.length; i++)
          args.push(repl[i]);
        Array.prototype.splice.apply(this.lines, args);
        if (caretInside)
          this.caret = this.offsetToCaret(start, start + repl.length - 1, off);
      };
      Document2.prototype.curLine = function() {
        return this.lines[this.caret.row];
      };
      Document2.prototype.curLineText = function() {
        return this.curLine().text;
      };
      Document2.prototype.insertChar = function(chCode) {
        var r = this.region;
        var lGuard = this.curLine();
        if (r !== null && r.pre === true && lGuard.text.length >= r.width && (this.insertMode || this.caret.col >= lGuard.text.length)) {
          return;
        }
        this.pushUndo("type");
        var ch = String.fromCharCode(chCode & 255);
        var l = this.curLine();
        var col = this.caret.col;
        if (this.insertMode || col >= l.text.length) {
          l.text = l.text.substring(0, col) + ch + l.text.substring(col);
          l.attr.splice(col, 0, this.curAttr);
        } else {
          l.text = l.text.substring(0, col) + ch + l.text.substring(col + 1);
          l.attr[col] = this.curAttr;
        }
        this.caret.col++;
        if (l.text.length > this.ew())
          this.rewrapParagraphAt(this.caret.row);
        this.dirty = true;
      };
      Document2.prototype.insertBreak = function() {
        this.pushUndo("");
        var l = this.curLine();
        var col = this.caret.col;
        var right = {
          text: l.text.substring(col),
          attr: l.attr.slice(col),
          hardcr: l.hardcr
        };
        l.text = l.text.substring(0, col);
        l.attr = l.attr.slice(0, col);
        l.hardcr = true;
        this.lines.splice(this.caret.row + 1, 0, right);
        this.caret = { row: this.caret.row + 1, col: 0 };
        this.rewrapParagraphAt(this.caret.row);
        this.dirty = true;
      };
      Document2.prototype.backspace = function() {
        var l = this.curLine();
        if (this.caret.col > 0) {
          this.pushUndo("erase");
          var col = this.caret.col;
          l.text = l.text.substring(0, col - 1) + l.text.substring(col);
          l.attr.splice(col - 1, 1);
          this.caret.col--;
          this.rewrapParagraphAt(this.caret.row);
        } else if (this.caret.row > 0) {
          this.pushUndo("");
          var prev = this.lines[this.caret.row - 1];
          this.caret = { row: this.caret.row - 1, col: prev.text.length };
          if (prev.hardcr) {
            prev.hardcr = false;
            this.rewrapParagraphAt(this.caret.row);
          } else {
            if (prev.text.length > 0) {
              prev.text = prev.text.substring(0, prev.text.length - 1);
              prev.attr.pop();
              this.caret.col--;
            }
            this.rewrapParagraphAt(this.caret.row);
          }
        }
        this.dirty = true;
      };
      Document2.prototype.deleteForward = function() {
        var l = this.curLine();
        var col = this.caret.col;
        if (col < l.text.length) {
          this.pushUndo("erase");
          l.text = l.text.substring(0, col) + l.text.substring(col + 1);
          l.attr.splice(col, 1);
          this.rewrapParagraphAt(this.caret.row);
        } else if (l.hardcr && this.caret.row < this.lines.length - 1) {
          this.pushUndo("");
          l.hardcr = false;
          this.rewrapParagraphAt(this.caret.row);
        } else if (!l.hardcr && this.caret.row < this.lines.length - 1) {
          this.pushUndo("erase");
          var next = this.lines[this.caret.row + 1];
          if (next.text.length > 0) {
            next.text = next.text.substring(1);
            next.attr.shift();
          }
          this.rewrapParagraphAt(this.caret.row);
        }
        this.dirty = true;
      };
      Document2.prototype.getRangeText = function(r0, c0, r1, c1) {
        if (r0 === r1)
          return this.lines[r0].text.substring(c0, c1);
        var out = this.lines[r0].text.substring(c0);
        for (var r = r0; r < r1; r++) {
          if (this.lines[r].hardcr)
            out += "\n";
          if (r + 1 < r1)
            out += this.lines[r + 1].text;
        }
        out += this.lines[r1].text.substring(0, c1);
        return out;
      };
      Document2.prototype.deleteRange = function(r0, c0, r1, c1) {
        this.pushUndo("");
        this.caret = { row: r1, col: c1 };
        var guard = 0;
        while ((this.caret.row > r0 || this.caret.col > c0) && guard++ < 1e5)
          this.backspace();
        this.dirty = true;
      };
      Document2.prototype.insertString = function(s) {
        this.pushUndo("");
        for (var i = 0; i < s.length; i++) {
          var ch = s.charAt(i);
          if (ch === "\n")
            this.insertBreak();
          else if (ch === "\r") {
          } else
            this.insertChar(ch.charCodeAt(0));
        }
        this.dirty = true;
      };
      Document2.prototype.insertLines = function(lines) {
        if (lines.length === 0)
          return;
        this.pushUndo("");
        for (var i = 0; i < lines.length; i++) {
          var src = lines[i];
          this.lines.splice(this.caret.row + i, 0, {
            text: src.text,
            attr: src.attr.slice(0),
            hardcr: true
          });
        }
        this.caret = { row: this.caret.row + lines.length, col: 0 };
        this.dirty = true;
      };
      Document2.prototype.moveLeft = function() {
        if (this.caret.col > 0)
          this.caret.col--;
        else if (this.caret.row > 0) {
          this.caret.row--;
          this.caret.col = this.curLine().text.length;
        }
      };
      Document2.prototype.moveRight = function() {
        if (this.caret.col < this.curLine().text.length)
          this.caret.col++;
        else if (this.caret.row < this.lines.length - 1) {
          this.caret.row++;
          this.caret.col = 0;
        }
      };
      Document2.prototype.moveVert = function(delta, desiredCol) {
        var row = clamp(this.caret.row + delta, 0, this.lines.length - 1);
        this.caret.row = row;
        this.caret.col = clamp(desiredCol, 0, this.curLine().text.length);
      };
      Document2.prototype.moveHome = function() {
        this.caret.col = 0;
      };
      Document2.prototype.moveEnd = function() {
        this.caret.col = this.curLine().text.length;
      };
      Document2.prototype.moveWordLeft = function() {
        this.moveLeft();
        var l = this.curLine();
        while (this.caret.col > 0 && l.text.charAt(this.caret.col - 1) !== " ")
          this.caret.col--;
        while (this.caret.col > 0 && l.text.charAt(this.caret.col - 1) === " ") {
          if (this.caret.col === 1)
            break;
          this.caret.col--;
        }
      };
      Document2.prototype.moveWordRight = function() {
        var l = this.curLine();
        var len = l.text.length;
        var col = this.caret.col;
        while (col < len && l.text.charAt(col) !== " ")
          col++;
        while (col < len && l.text.charAt(col) === " ")
          col++;
        if (col === this.caret.col)
          this.moveRight();
        else
          this.caret.col = col;
      };
      Document2.prototype.setArt = function(x, y, cell) {
        if (x < 0 || y < 0 || x >= this.width)
          return;
        this.pushUndo("draw");
        this.art[artKey(x, y)] = { ch: cell.ch, attr: cell.attr };
        this.dirty = true;
      };
      Document2.prototype.eraseArt = function(x, y) {
        var k = artKey(x, y);
        if (this.art[k] === void 0)
          return;
        this.pushUndo("draw");
        delete this.art[k];
        this.dirty = true;
      };
      Document2.prototype.paintCells = function(cells) {
        if (cells.length === 0)
          return;
        this.pushUndo("");
        for (var i = 0; i < cells.length; i++) {
          var c = cells[i];
          if (c.x < 0 || c.y < 0 || c.x >= this.width)
            continue;
          if (c.ch < 0)
            delete this.art[artKey(c.x, c.y)];
          else
            this.art[artKey(c.x, c.y)] = { ch: c.ch & 255, attr: c.attr };
        }
        this.dirty = true;
      };
      Document2.prototype.recolorCell = function(x, y, ink, channel) {
        var a = this.art[artKey(x, y)];
        if (a !== void 0) {
          var next = applyColorChannel(a.attr, ink, channel);
          if (next === a.attr)
            return false;
          this.pushUndo("recolor");
          this.art[artKey(x, y)].attr = next;
          this.dirty = true;
          return true;
        }
        for (var i = this.flows.length - 1; i >= 0; i--) {
          var f = this.flows[i];
          var r = f.region;
          if (i > 0) {
            var rr = r;
            if (x < rr.left || x >= rr.left + rr.width || y < rr.top || y >= rr.top + rr.height)
              continue;
          }
          var px = r ? x - r.left : x;
          var py = r ? y - r.top : y;
          var miss = px < 0 || py < 0 || py >= f.lines.length || px >= (f.lines[py] === void 0 ? 0 : f.lines[py].text.length);
          if (miss) {
            if (i > 0)
              return false;
            continue;
          }
          var l = f.lines[py];
          var cur = l.attr[px];
          var nx = applyColorChannel(cur, ink, channel);
          if (nx === cur)
            return false;
          this.pushUndo("recolor");
          l.attr[px] = nx;
          this.dirty = true;
          return true;
        }
        return false;
      };
      Document2.prototype.artAt = function(x, y) {
        var c = this.art[artKey(x, y)];
        return c === void 0 ? null : c;
      };
      Document2.prototype.hasArt = function() {
        return objectKeys(this.art).length > 0;
      };
      Document2.prototype.maxArtRow = function() {
        var max = -1;
        var keys = objectKeys(this.art);
        for (var i = 0; i < keys.length; i++) {
          var y = parseInt(keys[i].split(",")[0], 10);
          if (y > max)
            max = y;
        }
        return max;
      };
      Document2.prototype.rowCount = function() {
        var rows = 0;
        for (var i = 0; i < this.flows.length; i++) {
          var f = this.flows[i];
          var bottom = f.region ? f.region.top + Math.min(f.lines.length, f.region.height) : f.lines.length;
          if (bottom > rows)
            rows = bottom;
        }
        var artMax = this.maxArtRow();
        if (artMax + 1 > rows)
          rows = artMax + 1;
        return rows;
      };
      Document2.prototype.flowCell = function(f, x, y) {
        var r = f.region;
        var px = r ? x - r.left : x;
        var py = r ? y - r.top : y;
        if (px < 0 || py < 0)
          return null;
        if (r && (px >= r.width || py >= r.height))
          return null;
        if (py >= f.lines.length)
          return null;
        var l = f.lines[py];
        if (px >= l.text.length)
          return null;
        return { ch: l.text.charCodeAt(px) & 255, attr: l.attr[px] };
      };
      Document2.prototype.cellAt = function(x, y) {
        var a = this.art[artKey(x, y)];
        if (a !== void 0)
          return { ch: a.ch, attr: a.attr, isArt: true };
        for (var i = this.flows.length - 1; i >= 1; i--) {
          var f = this.flows[i];
          var r = f.region;
          if (x >= r.left && x < r.left + r.width && y >= r.top && y < r.top + r.height) {
            var bc = this.flowCell(f, x, y);
            if (bc !== null)
              return { ch: bc.ch, attr: bc.attr, isArt: false };
            return { ch: 32, attr: DEFAULT_ATTR, isArt: false };
          }
        }
        var mc = this.flowCell(this.flows[0], x, y);
        if (mc !== null)
          return { ch: mc.ch, attr: mc.attr, isArt: false };
        return { ch: 32, attr: DEFAULT_ATTR, isArt: false };
      };
      Document2.prototype.proseCharCount = function() {
        var n = 0;
        for (var f = 0; f < this.flows.length; f++) {
          var lines = this.flows[f].lines;
          for (var i = 0; i < lines.length; i++) {
            var t = lines[i].text;
            for (var j = 0; j < t.length; j++)
              if (t.charAt(j) !== " ")
                n++;
          }
        }
        return n;
      };
      Document2.prototype.artCellCount = function() {
        return objectKeys(this.art).length;
      };
      Document2.prototype.toAnsiBody = function() {
        return this.compositeBody("ansi");
      };
      Document2.prototype.toMessageBody = function(embedColors) {
        if (this.flows.length > 1)
          return this.compositeBody(embedColors ? "ctrla" : "none");
        var out = "";
        var lastattr = DEFAULT_ATTR;
        var anyArt = this.hasArt();
        var rows = this.rowCount();
        for (var y = 0; y < rows; y++) {
          var line = y < this.lines.length ? this.lines[y] : null;
          var hard = anyArt || line === null || line.hardcr || y === rows - 1;
          var textLen = line === null ? 0 : line.text.length;
          var rowLen = textLen;
          var lastArtX = -1;
          for (var x = 0; x < this.width; x++) {
            if (this.art[artKey(x, y)] !== void 0 && x > lastArtX)
              lastArtX = x;
          }
          if (lastArtX + 1 > rowLen)
            rowLen = lastArtX + 1;
          if (hard && line !== null) {
            var lastInk = -1;
            for (var i = 0; i < textLen; i++) {
              var c = line.text.charAt(i);
              if (c !== " " && c !== "	")
                lastInk = i;
            }
            var keep = lastInk + 1 > lastArtX + 1 ? lastInk + 1 : lastArtX + 1;
            if (keep < rowLen)
              rowLen = keep;
            if (textLen > keep)
              textLen = keep;
          }
          for (var x2 = 0; x2 < rowLen; x2++) {
            var cell = this.cellAt(x2, y);
            if (embedColors) {
              out += ctrlATransition(lastattr, cell.attr);
              lastattr = cell.attr;
            }
            out += String.fromCharCode(cell.ch);
          }
          if (hard)
            out += "\r\n";
        }
        return out;
      };
      Document2.prototype.compositeBody = function(mode) {
        var out = mode === "ansi" ? "\x1B[0m" : "";
        var lastattr = DEFAULT_ATTR;
        var rows = this.rowCount();
        for (var y = 0; y < rows; y++) {
          var rowLen = 0;
          for (var x = 0; x < this.width; x++) {
            var probe = this.cellAt(x, y);
            if (probe.isArt || probe.ch !== 32)
              rowLen = x + 1;
          }
          for (var x2 = 0; x2 < rowLen; x2++) {
            var cell = this.cellAt(x2, y);
            if (mode === "ctrla") {
              out += ctrlATransition(lastattr, cell.attr);
              lastattr = cell.attr;
            } else if (mode === "ansi" && cell.attr !== lastattr) {
              out += ansiFromAttr(cell.attr);
              lastattr = cell.attr;
            }
            out += String.fromCharCode(cell.ch);
          }
          out += "\r\n";
        }
        if (mode === "ansi")
          out += "\x1B[0m";
        return out;
      };
      return Document2;
    })()
  );

  // build/core/shapes.js
  var BOX_SINGLE = { tl: 218, tr: 191, bl: 192, br: 217, h: 196, v: 179 };
  var BOX_DOUBLE = { tl: 201, tr: 187, bl: 200, br: 188, h: 205, v: 186 };
  function linePoints(x0, y0, x1, y1) {
    var pts = [];
    var dx = Math.abs(x1 - x0);
    var dy = Math.abs(y1 - y0);
    var sx = x0 < x1 ? 1 : -1;
    var sy = y0 < y1 ? 1 : -1;
    var err = dx - dy;
    var x = x0;
    var y = y0;
    for (; ; ) {
      pts.push({ x: x, y: y });
      if (x === x1 && y === y1)
        break;
      var e2 = 2 * err;
      if (e2 > -dy) {
        err -= dy;
        x += sx;
      }
      if (e2 < dx) {
        err += dx;
        y += sy;
      }
    }
    return pts;
  }
  function boxCells(ax, ay, bx, by, dbl) {
    var x0 = Math.min(ax, bx);
    var x1 = Math.max(ax, bx);
    var y0 = Math.min(ay, by);
    var y1 = Math.max(ay, by);
    var g = dbl ? BOX_DOUBLE : BOX_SINGLE;
    var cells = [];
    if (x0 === x1 && y0 === y1) {
      cells.push({ x: x0, y: y0, ch: g.v });
      return cells;
    }
    if (y0 === y1) {
      for (var xh = x0; xh <= x1; xh++)
        cells.push({ x: xh, y: y0, ch: g.h });
      return cells;
    }
    if (x0 === x1) {
      for (var yv = y0; yv <= y1; yv++)
        cells.push({ x: x0, y: yv, ch: g.v });
      return cells;
    }
    cells.push({ x: x0, y: y0, ch: g.tl });
    cells.push({ x: x1, y: y0, ch: g.tr });
    cells.push({ x: x0, y: y1, ch: g.bl });
    cells.push({ x: x1, y: y1, ch: g.br });
    for (var x = x0 + 1; x < x1; x++) {
      cells.push({ x: x, y: y0, ch: g.h });
      cells.push({ x: x, y: y1, ch: g.h });
    }
    for (var y = y0 + 1; y < y1; y++) {
      cells.push({ x: x0, y: y, ch: g.v });
      cells.push({ x: x1, y: y, ch: g.v });
    }
    return cells;
  }
  var HALF = { top: 223, bottom: 220, left: 221, right: 222, full: 219 };
  function halfBlockLineCells(x0, y0, x1, y1) {
    var pts = linePoints(x0, y0 * 2, x1, y1 * 2);
    var halves = {};
    var order = [];
    for (var i = 0; i < pts.length; i++) {
      var p = pts[i];
      var cy = p.y >> 1;
      var key = p.x + "," + cy;
      if (halves[key] === void 0) {
        halves[key] = { top: false, bottom: false };
        order.push(key);
      }
      if ((p.y & 1) === 0)
        halves[key].top = true;
      else
        halves[key].bottom = true;
    }
    var out = [];
    for (var k = 0; k < order.length; k++) {
      var kk = order[k];
      var parts = kk.split(",");
      var h = halves[kk];
      var ch = h.top && h.bottom ? HALF.full : h.top ? HALF.top : HALF.bottom;
      out.push({ x: parseInt(parts[0], 10), y: parseInt(parts[1], 10), ch: ch });
    }
    return out;
  }
  function halfBlockBoxCells(ax, ay, bx, by) {
    var x0 = Math.min(ax, bx);
    var x1 = Math.max(ax, bx);
    var y0 = Math.min(ay, by);
    var y1 = Math.max(ay, by);
    var cells = [];
    if (y0 === y1) {
      for (var xh = x0; xh <= x1; xh++)
        cells.push({ x: xh, y: y0, ch: HALF.top });
      return cells;
    }
    if (x0 === x1) {
      for (var yv = y0; yv <= y1; yv++)
        cells.push({ x: x0, y: yv, ch: HALF.left });
      return cells;
    }
    for (var x = x0; x <= x1; x++) {
      cells.push({ x: x, y: y0, ch: HALF.top });
      cells.push({ x: x, y: y1, ch: HALF.bottom });
    }
    for (var y = y0 + 1; y < y1; y++) {
      cells.push({ x: x0, y: y, ch: HALF.left });
      cells.push({ x: x1, y: y, ch: HALF.right });
    }
    return cells;
  }
  function ellipseFillPoints(ax, ay, bx, by) {
    var x0 = Math.min(ax, bx);
    var x1 = Math.max(ax, bx);
    var y0 = Math.min(ay, by);
    var y1 = Math.max(ay, by);
    var rx = (x1 - x0) / 2;
    var ry = (y1 - y0) / 2;
    var cx = (x0 + x1) / 2;
    var cy = (y0 + y1) / 2;
    var pts = [];
    if (rx === 0 || ry === 0)
      return pts;
    for (var y = y0; y <= y1; y++) {
      for (var x = x0; x <= x1; x++) {
        var nx = (x - cx) / rx;
        var ny = (y - cy) / ry;
        if (nx * nx + ny * ny <= 1)
          pts.push({ x: x, y: y });
      }
    }
    return pts;
  }
  function ellipsePoints(ax, ay, bx, by) {
    var x0 = Math.min(ax, bx);
    var x1 = Math.max(ax, bx);
    var y0 = Math.min(ay, by);
    var y1 = Math.max(ay, by);
    var rx = (x1 - x0) / 2;
    var ry = (y1 - y0) / 2;
    var cx = (x0 + x1) / 2;
    var cy = (y0 + y1) / 2;
    if (rx === 0 && ry === 0)
      return [{ x: x0, y: y0 }];
    var seen = {};
    var pts = [];
    var steps = Math.max(8, Math.round((rx + ry) * 4));
    for (var i = 0; i < steps; i++) {
      var t = i / steps * Math.PI * 2;
      var x = Math.round(cx + rx * Math.cos(t));
      var y = Math.round(cy + ry * Math.sin(t));
      var key = x + "," + y;
      if (!seen[key]) {
        seen[key] = true;
        pts.push({ x: x, y: y });
      }
    }
    return pts;
  }
  function shapeToolAction(ev, hasAnchor, atAnchorCell) {
    var down = ev.press && !ev.motion;
    if (down && ev.button === 1)
      return "eyedrop";
    if (down && ev.button === 2)
      return "cancel";
    if (ev.release)
      return hasAnchor && !atAnchorCell ? "commit" : "none";
    if (ev.motion)
      return hasAnchor ? "preview" : "none";
    if (down && ev.button === 0)
      return hasAnchor ? "commit" : "set-anchor";
    return "none";
  }
  function floodFill(startX, startY, width, height, sample, maxCells) {
    var cap = maxCells === void 0 ? 4e3 : maxCells;
    if (startX < 0 || startY < 0 || startX >= width || startY >= height)
      return [];
    var target = sample(startX, startY);
    var out = [];
    var seen = {};
    var stack = [{ x: startX, y: startY }];
    while (stack.length > 0 && out.length < cap) {
      var p = stack.pop();
      if (p.x < 0 || p.y < 0 || p.x >= width || p.y >= height)
        continue;
      var k = p.x + "," + p.y;
      if (seen[k])
        continue;
      seen[k] = true;
      if (sample(p.x, p.y) !== target)
        continue;
      out.push({ x: p.x, y: p.y });
      stack.push({ x: p.x + 1, y: p.y });
      stack.push({ x: p.x - 1, y: p.y });
      stack.push({ x: p.x, y: p.y + 1 });
      stack.push({ x: p.x, y: p.y - 1 });
    }
    return out;
  }

  // build/core/quote.js
  function authorInitials(name) {
    var parts = String(name || "").replace(/^\s+|\s+$/g, "").split(/\s+/);
    var out = "";
    for (var i = 0; i < parts.length && out.length < 3; i++) {
      if (parts[i].length > 0)
        out += parts[i].charAt(0).toUpperCase();
    }
    return out.length > 0 ? out : "?";
  }
  function quotePrefix(style, author) {
    if (style === "gt")
      return "> ";
    if (style === "initials")
      return " " + authorInitials(author) + "> ";
    return "";
  }
  function formatQuote(lines, author, style, attribution) {
    var out = [];
    if (attribution && author && author.length > 0)
      out.push(author + " wrote:");
    var prefix = quotePrefix(style, author);
    for (var i = 0; i < lines.length; i++) {
      var line = lines[i];
      if (style === "gt" && line.charAt(0) === ">")
        out.push(">" + line);
      else
        out.push(prefix + line);
    }
    return out;
  }

  // build/core/syntax.js
  var HL_DEFAULT = 7;
  var HL_KEYWORD = 11;
  var HL_STRING = 14;
  var HL_COMMENT = 8;
  var HL_NUMBER = 10;
  var HL_PREPROC = 13;
  function initialHlState() {
    return { block: -1, mstr: -1 };
  }
  function kw(words) {
    var out = {};
    var list = words.split(" ");
    for (var i = 0; i < list.length; i++)
      out[list[i]] = true;
    return out;
  }
  var JS_BASE = "var let const function return if else for while do break continue switch case default new delete typeof instanceof in of class extends super this null true false undefined try catch finally throw void yield async await import export from static get set with debugger";
  var LANGS = [
    {
      id: "javascript",
      name: "JavaScript",
      keywords: kw(JS_BASE),
      lineComments: ["//"],
      blockComments: [["/*", "*/"]],
      quotes: [{ q: "'" }, { q: '"' }],
      multiStrings: ["`"]
    },
    {
      id: "typescript",
      name: "TypeScript",
      keywords: kw(JS_BASE + " interface type enum implements declare namespace module public private protected readonly abstract as is keyof infer never unknown any number string boolean symbol object"),
      lineComments: ["//"],
      blockComments: [["/*", "*/"]],
      quotes: [{ q: "'" }, { q: '"' }],
      multiStrings: ["`"]
    },
    {
      id: "python",
      name: "Python",
      keywords: kw("def class return if elif else for while break continue pass import from as with lambda yield global nonlocal try except finally raise assert del in is not and or None True False async await match case self print"),
      lineComments: ["#"],
      blockComments: [],
      quotes: [{ q: "'" }, { q: '"' }],
      multiStrings: ['"""', "'''"]
    },
    {
      id: "cpp",
      name: "C++",
      keywords: kw("int char long short float double void bool unsigned signed const static struct class public private protected virtual override final template typename namespace using new delete return if else for while do switch case default break continue goto sizeof enum union typedef extern inline friend operator this nullptr true false try catch throw auto constexpr mutable volatile register explicit noexcept decltype wchar_t size_t std"),
      lineComments: ["//"],
      blockComments: [["/*", "*/"]],
      quotes: [{ q: "'" }, { q: '"' }],
      multiStrings: [],
      hashPreproc: true
    },
    {
      id: "pascal",
      name: "Pascal",
      keywords: kw("program begin end procedure function var const type uses unit interface implementation if then else for to downto do while repeat until case of record array set string integer real boolean char byte word longint shortint cardinal writeln write readln read new dispose nil not and or xor div mod in with goto label packed file text object constructor destructor inherited private public protected published property class exit break continue result true false"),
      lineComments: ["//"],
      blockComments: [["{", "}"], ["(*", "*)"]],
      quotes: [{ q: "'", dbl: true }],
      multiStrings: [],
      caseInsensitive: true
    },
    {
      id: "plain",
      name: "Plain (no highlight)",
      keywords: {},
      lineComments: [],
      blockComments: [],
      quotes: [],
      multiStrings: []
    }
  ];
  function langById(id) {
    for (var i = 0; i < LANGS.length; i++) {
      if (LANGS[i].id === id)
        return LANGS[i];
    }
    return null;
  }
  function isDigit(c) {
    return c >= "0" && c <= "9";
  }
  function isIdentStart(c) {
    return c >= "a" && c <= "z" || c >= "A" && c <= "Z" || c === "_";
  }
  function isIdentPart(c) {
    return isIdentStart(c) || isDigit(c);
  }
  function startsAt(text, i, s) {
    return text.substring(i, i + s.length) === s;
  }
  function fillAttr(attrs, from, to, a) {
    for (var i = from; i < to; i++)
      attrs[i] = a;
  }
  function highlightLine(text, st, def) {
    var len = text.length;
    var attrs = [];
    for (var p = 0; p < len; p++)
      attrs.push(HL_DEFAULT);
    var i = 0;
    while (i < len) {
      if (st.block >= 0) {
        var close = def.blockComments[st.block][1];
        var end = text.indexOf(close, i);
        if (end === -1) {
          fillAttr(attrs, i, len, HL_COMMENT);
          return attrs;
        }
        fillAttr(attrs, i, end + close.length, HL_COMMENT);
        i = end + close.length;
        st.block = -1;
        continue;
      }
      if (st.mstr >= 0) {
        var mdelim = def.multiStrings[st.mstr];
        var mend = text.indexOf(mdelim, i);
        if (mend === -1) {
          fillAttr(attrs, i, len, HL_STRING);
          return attrs;
        }
        fillAttr(attrs, i, mend + mdelim.length, HL_STRING);
        i = mend + mdelim.length;
        st.mstr = -1;
        continue;
      }
      var c = text.charAt(i);
      var lc = -1;
      for (var l = 0; l < def.lineComments.length; l++) {
        if (startsAt(text, i, def.lineComments[l])) {
          lc = l;
          break;
        }
      }
      if (lc >= 0) {
        fillAttr(attrs, i, len, HL_COMMENT);
        return attrs;
      }
      var bc = -1;
      for (var b = 0; b < def.blockComments.length; b++) {
        if (startsAt(text, i, def.blockComments[b][0])) {
          bc = b;
          break;
        }
      }
      if (bc >= 0) {
        var opener = def.blockComments[bc][0];
        fillAttr(attrs, i, i + opener.length, HL_COMMENT);
        i += opener.length;
        st.block = bc;
        continue;
      }
      var ms = -1;
      for (var m = 0; m < def.multiStrings.length; m++) {
        if (startsAt(text, i, def.multiStrings[m])) {
          ms = m;
          break;
        }
      }
      if (ms >= 0) {
        var mo = def.multiStrings[ms];
        fillAttr(attrs, i, i + mo.length, HL_STRING);
        i += mo.length;
        st.mstr = ms;
        continue;
      }
      var qi = -1;
      for (var q = 0; q < def.quotes.length; q++) {
        if (def.quotes[q].q === c) {
          qi = q;
          break;
        }
      }
      if (qi >= 0) {
        var qd = def.quotes[qi];
        var j = i + 1;
        while (j < len) {
          var cj = text.charAt(j);
          if (!qd.dbl && cj === "\\") {
            j += 2;
            continue;
          }
          if (cj === qd.q) {
            if (qd.dbl && text.charAt(j + 1) === qd.q) {
              j += 2;
              continue;
            }
            j++;
            break;
          }
          j++;
        }
        if (j > len)
          j = len;
        fillAttr(attrs, i, j, HL_STRING);
        i = j;
        continue;
      }
      if (def.hashPreproc && c === "#" && text.substring(0, i).replace(/ +/g, "") === "") {
        var pj = i + 1;
        while (pj < len && isIdentPart(text.charAt(pj)))
          pj++;
        fillAttr(attrs, i, pj, HL_PREPROC);
        i = pj;
        continue;
      }
      if (isDigit(c)) {
        var nj = i + 1;
        while (nj < len) {
          var nc = text.charAt(nj);
          if (isDigit(nc) || nc === "." || nc === "x" || nc === "X" || nc >= "a" && nc <= "f" || nc >= "A" && nc <= "F")
            nj++;
          else
            break;
        }
        fillAttr(attrs, i, nj, HL_NUMBER);
        i = nj;
        continue;
      }
      if (isIdentStart(c)) {
        var ij = i + 1;
        while (ij < len && isIdentPart(text.charAt(ij)))
          ij++;
        var word = text.substring(i, ij);
        if (def.caseInsensitive)
          word = word.toLowerCase();
        if (def.keywords[word] === true)
          fillAttr(attrs, i, ij, HL_KEYWORD);
        i = ij;
        continue;
      }
      i++;
    }
    return attrs;
  }
  function highlightLines(lines, def) {
    var st = initialHlState();
    for (var i = 0; i < lines.length; i++) {
      var line = lines[i];
      line.attr = highlightLine(line.text, st, def);
    }
  }
  function fenceTag(text) {
    var m = /^```\s*([A-Za-z+#]*)\s*$/.exec(text);
    if (m === null)
      return null;
    return m[1].toLowerCase();
  }
  var FENCE_ALIASES = {
    js: "javascript",
    javascript: "javascript",
    ts: "typescript",
    typescript: "typescript",
    py: "python",
    python: "python",
    c: "cpp",
    cpp: "cpp",
    "c++": "cpp",
    cxx: "cpp",
    pas: "pascal",
    pascal: "pascal",
    delphi: "pascal"
  };
  var DETECT_MARKERS = {
    javascript: ["=>", "===", "function ", "console.", "require("],
    typescript: [": string", ": number", ": boolean", "interface ", "=>"],
    python: ["def ", "elif ", "import ", "self.", "):"],
    cpp: ["#include", "::", "->", "std::", ");"],
    pascal: [":=", "begin", "end;", "writeln", "procedure "]
  };
  function countOccurrences(haystack, needle) {
    var n = 0;
    var i = 0;
    for (; ; ) {
      var at = haystack.indexOf(needle, i);
      if (at === -1)
        return n;
      n++;
      i = at + needle.length;
    }
  }
  function detectLanguage(sample) {
    var bestId = "plain";
    var bestScore = 0;
    for (var li = 0; li < LANGS.length; li++) {
      var def = LANGS[li];
      if (def.id === "plain")
        continue;
      var score = 0;
      for (var s = 0; s < sample.length; s++) {
        var line = sample[s];
        var scan = def.caseInsensitive ? line.toLowerCase() : line;
        var word = "";
        for (var i = 0; i <= scan.length; i++) {
          var c = i < scan.length ? scan.charAt(i) : " ";
          if (isIdentPart(c))
            word += c;
          else {
            if (word.length > 1 && def.keywords[word] === true)
              score++;
            word = "";
          }
        }
        var markers = DETECT_MARKERS[def.id];
        if (markers !== void 0) {
          for (var mk = 0; mk < markers.length; mk++) {
            score += countOccurrences(scan, markers[mk]) * 3;
          }
        }
      }
      if (score > bestScore) {
        bestScore = score;
        bestId = def.id;
      }
    }
    return bestId;
  }
  function resolveFenceLang(tag, sample) {
    var id = tag === "" ? detectLanguage(sample) : FENCE_ALIASES[tag] !== void 0 ? FENCE_ALIASES[tag] : tag;
    var def = langById(id);
    return def !== null ? def : langById("plain");
  }

  // build/ui/theme.js
  var theme = {
    /** Title bar. */
    title: makeAttr(LIGHTGRAY2, BLUE, true),
    titleDim: makeAttr(CYAN, BLUE, false),
    /** Title-bar values (e.g. the recipient): light cyan. */
    titleValue: makeAttr(CYAN, BLUE, true),
    /** Button bar background. */
    bar: makeAttr(BLACK, CYAN, false),
    /** Button body: label text. */
    button: makeAttr(BLACK, LIGHTGRAY2, false),
    /** Button key hint (the part in brackets). */
    buttonKey: makeAttr(RED, LIGHTGRAY2, false),
    /** Directional arrows inside buttons (distinct from the key hint color). */
    buttonArrow: makeAttr(BLUE, LIGHTGRAY2, false),
    /** Focused/active button. */
    buttonActive: makeAttr(LIGHTGRAY2, BLUE, true),
    buttonActiveKey: makeAttr(BROWN, BLUE, true),
    /** Divider lines between chrome and canvas. */
    divider: makeAttr(CYAN, BLACK, false),
    /** Right-edge safe-width boundary marker. */
    boundary: makeAttr(BLACK, BLACK, true),
    /** Status bar: black background; dark-gray labels, white values. */
    status: makeAttr(LIGHTGRAY2, BLACK, false),
    statusLabel: makeAttr(BLACK, BLACK, true),
    // dark gray
    statusValue: makeAttr(LIGHTGRAY2, BLACK, true),
    // bright white
    statusHi: makeAttr(LIGHTGRAY2, BLACK, true),
    /** Canvas default. */
    canvas: makeAttr(LIGHTGRAY2, BLACK, false),
    /** Modal window frame/body. */
    modalFrame: makeAttr(LIGHTGRAY2, BLUE, true),
    modalBody: makeAttr(LIGHTGRAY2, BLUE, false),
    modalTitle: makeAttr(BROWN, BLUE, true),
    modalSel: makeAttr(BLUE, LIGHTGRAY2, false),
    /** Character-set bar (above the status bar). */
    charsetBar: makeAttr(LIGHTGRAY2, MAGENTA, false),
    charsetKey: makeAttr(LIGHTGRAY2, MAGENTA, true),
    /** Keyboard hints that live outside buttons (status row, panel rows). */
    keyHint: makeAttr(BROWN, BLACK, true),
    // yellow
    /** Side panel. */
    panel: makeAttr(LIGHTGRAY2, BLACK, false),
    panelTitle: makeAttr(CYAN, BLACK, true),
    panelSel: makeAttr(LIGHTGRAY2, MAGENTA, true),
    /** Quote picker line colors. */
    quote: makeAttr(GREEN, BLACK, false),
    quoteSel: makeAttr(LIGHTGRAY2, BLUE, true)
  };

  // build/ui/widgets.js
  var HitMap = (
    /** @class */
    (function() {
      function HitMap2() {
        this.regions = [];
      }
      HitMap2.prototype.clear = function() {
        this.regions = [];
      };
      HitMap2.prototype.add = function(id, x1, y1, x2, y2) {
        this.regions.push({ id: id, x1: x1, y1: y1, x2: x2, y2: y2 });
      };
      HitMap2.prototype.test = function(x, y) {
        for (var i = this.regions.length - 1; i >= 0; i--) {
          var r = this.regions[i];
          if (x >= r.x1 && x <= r.x2 && y >= r.y1 && y <= r.y2)
            return r.id;
        }
        return null;
      };
      return HitMap2;
    })()
  );
  function drawButton(scr, hits, x, y, id, keyLabel, label, active) {
    var body = active ? theme.buttonActive : theme.button;
    var key = active ? theme.buttonActiveKey : theme.buttonKey;
    var text = "[" + keyLabel + " " + label + "]";
    scr.put(x, y, "[".charCodeAt(0), body);
    scr.putStr(x + 1, y, keyLabel, key);
    scr.putStr(x + 1 + keyLabel.length, y, " " + label, body);
    scr.put(x + text.length - 1, y, "]".charCodeAt(0), body);
    hits.add(id, x, y, x + text.length - 1, y);
    return text.length;
  }
  var BOX = {
    tl: 218,
    tr: 191,
    bl: 192,
    br: 217,
    h: 196,
    v: 179,
    teeDown: 194,
    teeUp: 193,
    teeRight: 195,
    teeLeft: 180
  };
  var DBOX = {
    tl: 201,
    tr: 187,
    bl: 200,
    br: 188,
    h: 205,
    v: 186
  };
  function drawFrame(scr, x, y, w, h, attr, dbl, title, titleAttr) {
    var g = dbl ? DBOX : BOX;
    scr.put(x, y, g.tl, attr);
    scr.put(x + w - 1, y, g.tr, attr);
    scr.put(x, y + h - 1, g.bl, attr);
    scr.put(x + w - 1, y + h - 1, g.br, attr);
    scr.hline(x + 1, y, w - 2, g.h, attr);
    scr.hline(x + 1, y + h - 1, w - 2, g.h, attr);
    for (var yy = y + 1; yy < y + h - 1; yy++) {
      scr.put(x, yy, g.v, attr);
      scr.put(x + w - 1, yy, g.v, attr);
    }
    if (title !== void 0 && title.length > 0) {
      var t = " " + title + " ";
      if (t.length > w - 4)
        t = t.substring(0, w - 4);
      var tx = x + Math.floor((w - t.length) / 2);
      scr.putStr(tx, y, t, titleAttr === void 0 ? attr : titleAttr);
    }
  }
  function fillBox(scr, x, y, w, h, attr) {
    scr.fill(x, y, w, h, 32, attr);
  }

  // build/ui/keys.js
  var KEY_UP2 = "";
  var KEY_DOWN2 = "\n";
  var KEY_RIGHT2 = "";
  var KEY_LEFT2 = "";
  var KEY_HOME2 = "";
  var KEY_END2 = "";
  var KEY_INSERT2 = "";
  var KEY_DEL2 = "\x7F";
  var KEY_PAGEUP2 = "";
  var KEY_PAGEDN2 = "";
  var KEY_ENTER = "\r";
  var KEY_ESC = "\x1B";
  var KEY_BACKSPACE = "\b";
  var KEY_TAB = "	";
  var CTRL_A = "";
  var CTRL_C = "";
  var CTRL_D = "";
  var CTRL_G = "\x07";
  var CTRL_K = "\v";
  var CTRL_L = "\f";
  var CTRL_O = "";
  var CTRL_Q = "";
  var CTRL_R = "";
  var CTRL_S = "";
  var CTRL_T = "";
  var CTRL_W = "";
  var CTRL_X = "";
  var CTRL_Y = "";
  var CTRL_Z = "";
  function isPrintable(key) {
    if (key.length !== 1)
      return false;
    var c = key.charCodeAt(0);
    return c >= 32 && c < 127 || c >= 128 && c <= 255;
  }

  // build/ui/fontpicker.js
  var SIZE_FILTERS = [
    { label: "All", test: function() {
      return true;
    } },
    { label: "Small", test: function(h) {
      return h <= 5;
    } },
    { label: "Medium", test: function(h) {
      return h >= 6 && h <= 8;
    } },
    { label: "Large", test: function(h) {
      return h >= 9;
    } }
  ];
  function applyFilters(all, sizeIdx, nameFilter) {
    var sf = SIZE_FILTERS[sizeIdx];
    var needle = nameFilter.toLowerCase();
    var out = [];
    for (var i = 0; i < all.length; i++) {
      var f = all[i];
      if (!sf.test(f.height))
        continue;
      if (needle.length > 0 && f.name.toLowerCase().indexOf(needle) === -1)
        continue;
      out.push(f);
    }
    return out;
  }
  function fontPicker(scr, input, alive, provider, initialText) {
    var all = provider.list();
    if (all.length === 0)
      return null;
    var w = Math.min(scr.cols - 2, 76);
    var h = Math.min(scr.rows - 2, 22);
    var x = Math.floor((scr.cols - w) / 2);
    var y = Math.floor((scr.rows - h) / 2);
    var text = initialText.length > 0 ? initialText : "Hello";
    var sizeIdx = 0;
    var nameFilter = "";
    var filtered = applyFilters(all, sizeIdx, nameFilter);
    var sel = 0;
    var top = 0;
    var focus = 0;
    var listX = x + 2;
    var listY = y + 5;
    var listW = 22;
    var listH = h - 8;
    var previewX = listX + listW + 2;
    var previewW = x + w - 2 - previewX;
    var cacheKey = "";
    var cached = { font: null, render: null };
    function refilter() {
      filtered = applyFilters(all, sizeIdx, nameFilter);
      if (sel >= filtered.length)
        sel = filtered.length - 1;
      if (sel < 0)
        sel = 0;
      top = 0;
    }
    function currentFont() {
      return sel >= 0 && sel < filtered.length ? filtered[sel] : null;
    }
    function ensurePreview() {
      var fm = currentFont();
      if (fm === null) {
        cached = { font: null, render: null };
        cacheKey = "";
        return;
      }
      var key = fm.name + "|" + text;
      if (key === cacheKey)
        return;
      cacheKey = key;
      var font = provider.load(fm.name);
      cached = { font: font, render: font === null ? null : renderTdf(font, text.length > 0 ? text : " ") };
    }
    while (alive()) {
      if (sel < top)
        top = sel;
      if (sel >= top + listH)
        top = sel - listH + 1;
      ensurePreview();
      var hits = new HitMap();
      fillBox(scr, x, y, w, h, theme.modalBody);
      drawFrame(scr, x, y, w, h, theme.modalFrame, true, "TheDraw font", theme.modalTitle);
      scr.putStr(x + 2, y + 2, "Text:", theme.modalBody);
      var fieldAttr = makeAttr(7, 0, true);
      var fieldW = w - 10;
      scr.fill(x + 8, y + 2, fieldW, 1, 32, focus === 0 ? fieldAttr : theme.modalBody);
      scr.putStr(x + 8, y + 2, text.substring(text.length > fieldW ? text.length - fieldW : 0), focus === 0 ? fieldAttr : theme.modalBody);
      hits.add("field", x + 8, y + 2, x + 8 + fieldW - 1, y + 2);
      var fx = x + 2;
      scr.putStr(fx, y + 3, "Size:", theme.modalBody);
      fx += 6;
      for (var si = 0; si < SIZE_FILTERS.length; si++) {
        var sfa = si === sizeIdx ? theme.modalSel : theme.modalBody;
        var lbl = " " + SIZE_FILTERS[si].label + " ";
        scr.putStr(fx, y + 3, lbl, sfa);
        hits.add("size" + si, fx, y + 3, fx + lbl.length - 1, y + 3);
        fx += lbl.length + 1;
      }
      scr.putStr(fx + 1, y + 3, "Name: " + (nameFilter.length ? nameFilter : "(any)"), theme.modalBody);
      hits.add("namefilter", fx + 1, y + 3, x + w - 3, y + 3);
      scr.putStr(x + 2, y + 4, filtered.length + " fonts  (Tab field/list \x1B size)", theme.modalTitle);
      fillBox(scr, listX, listY, listW, listH, theme.modalBody);
      for (var li = 0; li < listH; li++) {
        var idx = top + li;
        if (idx >= filtered.length)
          break;
        var fm2 = filtered[idx];
        var rowAttr = idx === sel ? theme.modalSel : theme.modalBody;
        var rowTxt = fm2.name;
        if (rowTxt.length > listW - 5)
          rowTxt = rowTxt.substring(0, listW - 5);
        scr.fill(listX, listY + li, listW, 1, 32, rowAttr);
        scr.putStr(listX, listY + li, rowTxt, rowAttr);
        scr.putStr(listX + listW - 3, listY + li, "" + fm2.height, idx === sel ? theme.modalSel : theme.modalTitle);
        hits.add("font" + idx, listX, listY + li, listX + listW - 1, listY + li);
      }
      drawFrame(scr, previewX - 1, listY - 1, previewW + 2, listH + 2, theme.divider, false);
      var pv = cached.render;
      var fmc = currentFont();
      if (fmc !== null) {
        var dims = pv ? pv.width + "x" + pv.height : "?";
        scr.putStr(previewX, listY - 1, " " + fmc.name + " (" + fmc.height + ") " + dims + " ", theme.modalTitle);
      }
      if (pv !== null) {
        var isColor = cached.font !== null && cached.font.fonttype === COLOR_FONT;
        for (var ry = 0; ry < listH && ry < pv.rows.length; ry++) {
          var prow = pv.rows[ry];
          for (var rx = 0; rx < previewW && rx < prow.length; rx++) {
            var pc = prow[rx];
            var pAttr = isColor ? pc.color & 255 : makeAttr(7, 0, true);
            scr.put(previewX + rx, listY + ry, pc.ch, pc.ch === 32 && !(isColor && pc.color & 112) ? theme.modalBody : pAttr);
          }
        }
        if (pv.width > previewW)
          scr.putStr(previewX, listY + listH - 1, " wider than pane ", theme.modalTitle);
      } else if (fmc !== null) {
        scr.putStr(previewX, listY + 1, "(font failed to load)", theme.modalBody);
      }
      var by = y + h - 2;
      var bx = x + 2;
      bx += drawButton(scr, hits, bx, by, "ok", "Enter", "Use font") + 1;
      drawButton(scr, hits, bx, by, "cancel", "Esc", "Cancel");
      scr.cursorVisible = focus === 0;
      scr.cursorX = x + 8 + Math.min(text.length, fieldW);
      scr.cursorY = y + 2;
      scr.flush();
      var ev = input(3e4);
      if (ev.type === "mouse") {
        if (ev.wheel !== 0) {
          sel = clampSel(sel + ev.wheel * 3, filtered.length);
          continue;
        }
        if (ev.press && ev.button === 0) {
          var id = hits.test(ev.x - 1, ev.y - 1);
          if (id === null)
            continue;
          if (id === "ok")
            return commit(currentFont(), text);
          if (id === "cancel")
            return null;
          if (id === "field") {
            focus = 0;
            continue;
          }
          if (id === "namefilter") {
            focus = 2;
            continue;
          }
          if (id.substring(0, 4) === "size") {
            sizeIdx = parseInt(id.substring(4), 10);
            refilter();
            continue;
          }
          if (id.substring(0, 4) === "font") {
            sel = parseInt(id.substring(4), 10);
            focus = 1;
            continue;
          }
        }
        continue;
      }
      if (ev.type !== "key")
        continue;
      var k = ev.key;
      if (k === KEY_ESC)
        return null;
      if (k === KEY_ENTER)
        return commit(currentFont(), text);
      if (k === KEY_TAB) {
        focus = focus === 1 ? 0 : 1;
        continue;
      }
      if (k === KEY_UP2) {
        sel = clampSel(sel - 1, filtered.length);
        focus = 1;
        continue;
      }
      if (k === KEY_DOWN2) {
        sel = clampSel(sel + 1, filtered.length);
        focus = 1;
        continue;
      }
      if (k === KEY_PAGEUP2) {
        sel = clampSel(sel - listH, filtered.length);
        continue;
      }
      if (k === KEY_PAGEDN2) {
        sel = clampSel(sel + listH, filtered.length);
        continue;
      }
      if (k === KEY_LEFT2) {
        sizeIdx = (sizeIdx + SIZE_FILTERS.length - 1) % SIZE_FILTERS.length;
        refilter();
        continue;
      }
      if (k === KEY_RIGHT2) {
        sizeIdx = (sizeIdx + 1) % SIZE_FILTERS.length;
        refilter();
        continue;
      }
      var erase = k === KEY_BACKSPACE || k === KEY_DEL2;
      if (focus === 2) {
        if (erase)
          nameFilter = nameFilter.substring(0, nameFilter.length - 1);
        else if (isPrintable(k))
          nameFilter += k;
        refilter();
      } else {
        if (erase)
          text = text.substring(0, text.length - 1);
        else if (isPrintable(k))
          text += k;
        focus = 0;
      }
    }
    return null;
  }
  function commit(fm, text) {
    if (fm === null || text.length === 0)
      return null;
    return { fontName: fm.name, text: text };
  }
  function clampSel(v, n) {
    if (n <= 0)
      return 0;
    if (v < 0)
      return 0;
    if (v >= n)
      return n - 1;
    return v;
  }

  // build/ui/charsets.js
  var CHARSETS = [
    [218, 191, 192, 217, 196, 179, 195, 180, 193, 194],
    // single-line box
    [201, 187, 200, 188, 205, 186, 204, 185, 202, 203],
    // double-line box
    [213, 184, 212, 190, 205, 179, 198, 181, 207, 209],
    // double-H/single-V box
    [214, 183, 211, 189, 196, 186, 199, 182, 208, 210],
    // single-H/double-V box
    [197, 206, 216, 215, 232, 232, 155, 156, 153, 239],
    // crosses & currency
    [176, 177, 178, 219, 223, 220, 221, 222, 254, 250],
    // blocks (the classic)
    [1, 2, 3, 4, 5, 6, 240, 14, 15, 32],
    // faces, suits, notes
    [24, 25, 30, 31, 16, 17, 18, 29, 20, 21],
    // arrows & markers
    [174, 175, 242, 243, 169, 170, 253, 246, 171, 172],
    // guillemets & math
    [227, 241, 244, 245, 234, 157, 228, 248, 251, 252],
    // greek & math
    [224, 225, 226, 229, 230, 231, 235, 236, 237, 238],
    // greek
    [128, 135, 165, 164, 152, 159, 247, 249, 173, 168],
    // c-cedilla, n-tilde...
    [131, 132, 133, 160, 166, 134, 142, 143, 145, 146],
    // accented a
    [136, 137, 138, 130, 144, 140, 139, 141, 161, 158],
    // accented e / i
    [147, 148, 149, 162, 167, 150, 129, 151, 163, 154],
    // accented o / u
    [47, 92, 40, 41, 123, 125, 91, 93, 96, 39]
    // ASCII slashes/brackets
  ];
  var DEFAULT_CHARSET = 5;

  // build/ui/modals.js
  function center(w, total) {
    var x = Math.floor((total - w) / 2);
    return x < 0 ? 0 : x;
  }
  function messageBox(scr, input, alive, title, lines, buttons, defaultIdx) {
    var maxLine = 0;
    for (var i = 0; i < lines.length; i++)
      if (lines[i].length > maxLine)
        maxLine = lines[i].length;
    var btnW = 0;
    for (var b = 0; b < buttons.length; b++) {
      var bb = buttons[b];
      btnW += bb.key.length + bb.label.length + 4;
    }
    var w = Math.max(maxLine + 6, btnW + 4, title.length + 6);
    if (w > scr.cols - 2)
      w = scr.cols - 2;
    var h = lines.length + 6;
    var x = center(w, scr.cols);
    var y = center(h, scr.rows);
    var sel = defaultIdx === void 0 ? 0 : defaultIdx;
    var hits = new HitMap();
    while (alive()) {
      hits.clear();
      fillBox(scr, x, y, w, h, theme.modalBody);
      drawFrame(scr, x, y, w, h, theme.modalFrame, true, title, theme.modalTitle);
      for (var li = 0; li < lines.length; li++) {
        scr.putStr(x + 3, y + 2 + li, lines[li].substring(0, w - 6), theme.modalBody);
      }
      var bx = x + center(btnW, w);
      var by = y + h - 3;
      for (var bi = 0; bi < buttons.length; bi++) {
        var btn = buttons[bi];
        bx += drawButton(scr, hits, bx, by, btn.id, btn.key, btn.label, bi === sel) + 1;
      }
      scr.cursorVisible = false;
      scr.flush();
      var ev = input(3e4);
      if (ev.type === "mouse") {
        if (ev.press && ev.button === 0) {
          var id = hits.test(ev.x - 1, ev.y - 1);
          if (id !== null)
            return id;
        }
        continue;
      }
      if (ev.type !== "key")
        continue;
      var k = ev.key;
      if (k === KEY_ESC)
        return null;
      if (k === KEY_ENTER)
        return buttons[sel].id;
      if (k === KEY_LEFT2 || k === KEY_TAB && false)
        sel = (sel + buttons.length - 1) % buttons.length;
      else if (k === KEY_RIGHT2 || k === KEY_TAB)
        sel = (sel + 1) % buttons.length;
      else {
        for (var s = 0; s < buttons.length; s++) {
          var cand = buttons[s];
          if (cand.key.length === 1 && k.toUpperCase() === cand.key.toUpperCase())
            return cand.id;
          if (cand.key.length > 1 && k === cand.key)
            return cand.id;
        }
      }
    }
    return null;
  }
  function dropdownMenu(scr, input, alive, x, y, items) {
    var w = 0;
    for (var i = 0; i < items.length; i++) {
      var it = items[i];
      var lw = it.label.length + it.keyLabel.length + 6;
      if (lw > w)
        w = lw;
    }
    var h = items.length + 2;
    if (x + w > scr.cols)
      x = scr.cols - w;
    if (y + h > scr.rows)
      y = scr.rows - h;
    var sel = 0;
    while (items[sel].separator)
      sel++;
    var hits = new HitMap();
    while (alive()) {
      hits.clear();
      fillBox(scr, x, y, w, h, theme.modalBody);
      drawFrame(scr, x, y, w, h, theme.modalFrame, false);
      for (var li = 0; li < items.length; li++) {
        var item = items[li];
        var iy = y + 1 + li;
        if (item.separator) {
          scr.hline(x + 1, iy, w - 2, 196, theme.modalFrame);
          continue;
        }
        var attr = li === sel ? theme.modalSel : theme.modalBody;
        scr.fill(x + 1, iy, w - 2, 1, 32, attr);
        scr.putStr(x + 2, iy, item.label, attr);
        scr.putStr(x + w - 2 - item.keyLabel.length, iy, item.keyLabel, li === sel ? theme.modalSel : theme.modalTitle);
        hits.add(item.id, x + 1, iy, x + w - 2, iy);
      }
      scr.cursorVisible = false;
      scr.flush();
      var ev = input(3e4);
      if (ev.type === "mouse") {
        if (ev.wheel !== 0)
          continue;
        var overId = hits.test(ev.x - 1, ev.y - 1);
        if (ev.motion || ev.release) {
          if (overId !== null) {
            for (var hv = 0; hv < items.length; hv++) {
              if (items[hv].id === overId && !items[hv].separator) {
                sel = hv;
                break;
              }
            }
          }
          continue;
        }
        if (ev.press && ev.button === 0) {
          if (overId !== null)
            return overId;
          return null;
        }
        continue;
      }
      if (ev.type !== "key")
        continue;
      var k = ev.key;
      if (k === KEY_ESC)
        return null;
      if (k === KEY_ENTER)
        return items[sel].id;
      if (k === KEY_UP2)
        sel = moveSel(items, sel, -1);
      else if (k === KEY_DOWN2)
        sel = moveSel(items, sel, 1);
      else {
        for (var hk = 0; hk < items.length; hk++) {
          var cand = items[hk];
          if (!cand.separator && cand.label.charAt(0).toUpperCase() === k.toUpperCase())
            return cand.id;
        }
      }
    }
    return null;
  }
  function moveSel(items, sel, dir) {
    var n = items.length;
    var s = sel;
    for (var i = 0; i < n; i++) {
      s = (s + dir + n) % n;
      if (!items[s].separator)
        return s;
    }
    return sel;
  }
  function glyphPicker(scr, input, alive, current) {
    var COLS = 32;
    var ROWS = 7;
    var w = COLS + 4;
    var h = ROWS + 6;
    var x = center(w, scr.cols);
    var y = center(h, scr.rows);
    var sel = current >= 32 ? current - 32 : 219 - 32;
    var hits = new HitMap();
    while (alive()) {
      hits.clear();
      fillBox(scr, x, y, w, h, theme.modalBody);
      drawFrame(scr, x, y, w, h, theme.modalFrame, true, "Choose a character", theme.modalTitle);
      for (var g = 0; g < COLS * ROWS; g++) {
        var gx = x + 2 + g % COLS;
        var gy = y + 2 + Math.floor(g / COLS);
        var attr = g === sel ? theme.modalSel : theme.modalBody;
        scr.put(gx, gy, g + 32, attr);
      }
      hits.add("grid", x + 2, y + 2, x + 1 + COLS, y + 1 + ROWS);
      var info = "Code " + (sel + 32) + "   Arrows move \xB7 Enter picks \xB7 Esc cancels";
      scr.putStr(x + 2, y + h - 3, info.substring(0, w - 4), theme.modalBody);
      var bx = x + 2;
      var by = y + h - 2;
      bx += drawButton(scr, hits, bx, by, "ok", "Enter", "Pick") + 1;
      drawButton(scr, hits, bx, by, "cancel", "Esc", "Cancel");
      scr.cursorVisible = false;
      scr.flush();
      var ev = input(3e4);
      if (ev.type === "mouse") {
        if (ev.press && ev.button === 0) {
          var id = hits.test(ev.x - 1, ev.y - 1);
          if (id === "grid") {
            var cx = ev.x - 1 - (x + 2);
            var cy = ev.y - 1 - (y + 2);
            sel = cy * COLS + cx;
            return sel + 32;
          }
          if (id === "ok")
            return sel + 32;
          if (id === "cancel")
            return null;
        }
        continue;
      }
      if (ev.type !== "key")
        continue;
      var k = ev.key;
      if (k === KEY_ESC)
        return null;
      if (k === KEY_ENTER)
        return sel + 32;
      if (k === KEY_LEFT2)
        sel = (sel + COLS * ROWS - 1) % (COLS * ROWS);
      else if (k === KEY_RIGHT2)
        sel = (sel + 1) % (COLS * ROWS);
      else if (k === KEY_UP2)
        sel = (sel + COLS * ROWS - COLS) % (COLS * ROWS);
      else if (k === KEY_DOWN2)
        sel = (sel + COLS) % (COLS * ROWS);
      else if (isPrintable(k))
        return k.charCodeAt(0);
    }
    return null;
  }
  function colorPicker(scr, input, alive, currentAttr) {
    var w = 44;
    var h = 12;
    var x = center(w, scr.cols);
    var y = center(h, scr.rows);
    var fg = currentAttr & 15;
    var bg = currentAttr >> 4 & 7;
    var blink = (currentAttr & BLINK) !== 0;
    var hits = new HitMap();
    var NAMES = ["Blk", "Blu", "Grn", "Cyn", "Red", "Mag", "Brn", "Gry"];
    while (alive()) {
      hits.clear();
      fillBox(scr, x, y, w, h, theme.modalBody);
      drawFrame(scr, x, y, w, h, theme.modalFrame, true, "Colors", theme.modalTitle);
      scr.putStr(x + 2, y + 2, "Text color (\x1B keys):", theme.modalBody);
      for (var f = 0; f < 16; f++) {
        var fx = x + 2 + f % 8 * 2;
        var fy = y + 3 + Math.floor(f / 8);
        var swatch = makeAttr(f & 7, 0, f >= 8);
        scr.put(fx, fy, f === fg ? 219 : 254, swatch);
        scr.put(fx + 1, fy, 32, theme.modalBody);
        hits.add("fg" + f, fx, fy, fx, fy);
      }
      scr.putStr(x + 22, y + 2, "Background ( keys):", theme.modalBody);
      for (var g2 = 0; g2 < 8; g2++) {
        var gx = x + 22 + g2 * 2;
        var isB = g2 === bg;
        scr.put(gx, y + 3, isB ? 254 : 32, makeAttr(g2 === 7 ? 0 : 7, g2, isB && g2 !== 7));
        hits.add("bg" + g2, gx, y + 3, gx, y + 3);
      }
      scr.putStr(x + 2, y + 6, "[" + (blink ? "X" : " ") + "] Blink  (K toggles)", theme.modalBody);
      hits.add("blink", x + 2, y + 6, x + 22, y + 6);
      var preview = makeAttr(fg & 7, bg, fg >= 8, blink);
      scr.putStr(x + 2, y + 8, "Sample: ", theme.modalBody);
      scr.putStr(x + 10, y + 8, " Sample text \xB0\xB1\xB2\xDB ", preview);
      var bx = x + 2;
      var by = y + h - 2;
      bx += drawButton(scr, hits, bx, by, "ok", "Enter", "Use this") + 1;
      drawButton(scr, hits, bx, by, "cancel", "Esc", "Cancel");
      scr.cursorVisible = false;
      scr.flush();
      var ev = input(3e4);
      if (ev.type === "mouse") {
        if (ev.press && ev.button === 0) {
          var id = hits.test(ev.x - 1, ev.y - 1);
          if (id === null)
            continue;
          if (id === "ok")
            return makeAttr(fg & 7, bg, fg >= 8, blink);
          if (id === "cancel")
            return null;
          if (id === "blink")
            blink = !blink;
          else if (id.substring(0, 2) === "fg")
            fg = parseInt(id.substring(2), 10);
          else if (id.substring(0, 2) === "bg")
            bg = parseInt(id.substring(2), 10);
        }
        continue;
      }
      if (ev.type !== "key")
        continue;
      var k = ev.key;
      if (k === KEY_ESC)
        return null;
      if (k === KEY_ENTER)
        return makeAttr(fg & 7, bg, fg >= 8, blink);
      if (k === KEY_LEFT2)
        fg = (fg + 15) % 16;
      else if (k === KEY_RIGHT2)
        fg = (fg + 1) % 16;
      else if (k === KEY_UP2)
        bg = (bg + 7) % 8;
      else if (k === KEY_DOWN2)
        bg = (bg + 1) % 8;
      else if (k.toUpperCase() === "K")
        blink = !blink;
    }
    return null;
  }
  function quotePicker(scr, input, alive, quoteLines) {
    var w = scr.cols - 4;
    var h = scr.rows - 4;
    if (h < 8)
      h = scr.rows;
    var x = center(w, scr.cols);
    var y = center(h, scr.rows);
    var listH = h - 4;
    var top = 0;
    var cur = 0;
    var selected = [];
    for (var i = 0; i < quoteLines.length; i++)
      selected.push(false);
    var hits = new HitMap();
    while (alive()) {
      hits.clear();
      if (cur < top)
        top = cur;
      if (cur >= top + listH)
        top = cur - listH + 1;
      fillBox(scr, x, y, w, h, theme.modalBody);
      drawFrame(scr, x, y, w, h, theme.modalFrame, true, "Quote original message", theme.modalTitle);
      for (var li = 0; li < listH; li++) {
        var idx = top + li;
        if (idx >= quoteLines.length)
          break;
        var ly = y + 1 + li;
        var attr = idx === cur ? theme.quoteSel : theme.quote;
        var mark = selected[idx] ? "[x] " : "[ ] ";
        var text = mark + quoteLines[idx];
        if (text.length > w - 2)
          text = text.substring(0, w - 2);
        scr.fill(x + 1, ly, w - 2, 1, 32, attr);
        scr.putStr(x + 1, ly, text, attr);
        hits.add("line" + idx, x + 1, ly, x + w - 2, ly);
      }
      var bx = x + 2;
      var by = y + h - 2;
      bx += drawButton(scr, hits, bx, by, "toggle", "Space", "Mark") + 1;
      bx += drawButton(scr, hits, bx, by, "all", "A", "All") + 1;
      bx += drawButton(scr, hits, bx, by, "insert", "Enter", "Insert") + 1;
      drawButton(scr, hits, bx, by, "cancel", "Esc", "Cancel");
      scr.cursorVisible = false;
      scr.flush();
      var ev = input(3e4);
      if (ev.type === "mouse") {
        if (ev.wheel !== 0) {
          cur = clampIdx(cur + ev.wheel * 3, quoteLines.length);
          continue;
        }
        if (ev.press && ev.button === 0) {
          var id = hits.test(ev.x - 1, ev.y - 1);
          if (id === null)
            continue;
          if (id === "cancel")
            return null;
          if (id === "all") {
            for (var a = 0; a < selected.length; a++)
              selected[a] = true;
          } else if (id === "insert")
            return collectSelection(quoteLines, selected, cur);
          else if (id === "toggle")
            selected[cur] = !selected[cur];
          else if (id.substring(0, 4) === "line") {
            cur = parseInt(id.substring(4), 10);
            selected[cur] = !selected[cur];
          }
        }
        continue;
      }
      if (ev.type !== "key")
        continue;
      var k = ev.key;
      if (k === KEY_ESC)
        return null;
      if (k === KEY_ENTER)
        return collectSelection(quoteLines, selected, cur);
      if (k === " ") {
        selected[cur] = !selected[cur];
        cur = clampIdx(cur + 1, quoteLines.length);
      } else if (k === KEY_UP2)
        cur = clampIdx(cur - 1, quoteLines.length);
      else if (k === KEY_DOWN2)
        cur = clampIdx(cur + 1, quoteLines.length);
      else if (k === KEY_PAGEUP2)
        cur = clampIdx(cur - listH, quoteLines.length);
      else if (k === KEY_PAGEDN2)
        cur = clampIdx(cur + listH, quoteLines.length);
      else if (k === KEY_HOME2)
        cur = 0;
      else if (k === KEY_END2)
        cur = quoteLines.length - 1;
      else if (k.toUpperCase() === "A") {
        for (var a2 = 0; a2 < selected.length; a2++)
          selected[a2] = true;
      } else if (k.toUpperCase() === "N") {
        for (var n2 = 0; n2 < selected.length; n2++)
          selected[n2] = false;
      }
    }
    return null;
  }
  function clampIdx(v, n) {
    if (v < 0)
      return 0;
    if (v >= n)
      return n - 1;
    return v;
  }
  function collectSelection(lines, selected, cur) {
    var out = [];
    for (var i = 0; i < lines.length; i++) {
      if (selected[i])
        out.push(lines[i]);
    }
    if (out.length === 0 && lines.length > 0)
      out.push(lines[cur]);
    return out;
  }
  function promptLine(scr, input, alive, title, initial, maxLen) {
    var w = Math.min(scr.cols - 4, Math.max(40, maxLen + 6));
    var h = 7;
    var x = center(w, scr.cols);
    var y = center(h, scr.rows);
    var text = initial;
    var col = text.length;
    var fieldW = w - 6;
    var hits = new HitMap();
    while (alive()) {
      hits.clear();
      fillBox(scr, x, y, w, h, theme.modalBody);
      drawFrame(scr, x, y, w, h, theme.modalFrame, true, title, theme.modalTitle);
      var fieldAttr = makeAttr(7, 0, true);
      scr.fill(x + 3, y + 2, fieldW, 1, 32, fieldAttr);
      var scroll = col > fieldW - 1 ? col - (fieldW - 1) : 0;
      scr.putStr(x + 3, y + 2, text.substring(scroll, scroll + fieldW), fieldAttr);
      var bx = x + 3;
      var by = y + h - 2;
      bx += drawButton(scr, hits, bx, by, "ok", "Enter", "OK") + 1;
      drawButton(scr, hits, bx, by, "cancel", "Esc", "Cancel");
      scr.cursorX = x + 4 + (col - scroll);
      scr.cursorY = y + 3;
      scr.cursorVisible = true;
      scr.flush();
      var ev = input(3e4);
      if (ev.type === "mouse") {
        if (ev.press && ev.button === 0) {
          var id = hits.test(ev.x - 1, ev.y - 1);
          if (id === "ok")
            return text;
          if (id === "cancel")
            return null;
        }
        continue;
      }
      if (ev.type !== "key")
        continue;
      var k = ev.key;
      if (k === KEY_ESC)
        return null;
      if (k === KEY_ENTER)
        return text;
      if (k === KEY_LEFT2 && col > 0)
        col--;
      else if (k === KEY_RIGHT2 && col < text.length)
        col++;
      else if (k === KEY_HOME2)
        col = 0;
      else if (k === KEY_END2)
        col = text.length;
      else if (k === KEY_BACKSPACE && col > 0) {
        text = text.substring(0, col - 1) + text.substring(col);
        col--;
      } else if (k === KEY_DEL2 && col < text.length) {
        text = text.substring(0, col) + text.substring(col + 1);
      } else if (isPrintable(k) && text.length < maxLen) {
        text = text.substring(0, col) + k + text.substring(col);
        col++;
      }
    }
    return null;
  }
  function helpOverlay(scr, input, alive, mode) {
    var lines;
    if (mode === "draw") {
      lines = [
        "DRAW MODE - paint CP437 art that stays fixed in place",
        "",
        "Tab cycles tools: Pencil, Type, Select, Line, Box,",
        "        Circle, Fill, Recolor.  Shift+Tab opens the",
        "        current tool's options (size, style, fill...).",
        "Pencil: left paints, right erases, middle picks up a cell.",
        "Type:   click anywhere, then type text freely \u2014 it lands as",
        "        fixed art (no wrap). Enter returns to your start",
        "        column; Backspace erases. Text art placed anywhere.",
        "Line/Box/Circle: drag start-to-end, or Space to set start,",
        "        move, Space to commit. Fill: left/Space fills.",
        "Recolor: drag over glyphs to repaint their color, keeping",
        "        the character. 1=FG only, 2=BG only, 3=both.",
        "F1-F10: type the characters previewed on the bar above",
        "        the status line; F11/F12 or   cycle its sets.",
        "^L  colors    ^K  character    ^W  eyedrop   ^T  text mode",
        "",
        "Art cells never move when message text is edited or",
        "rewrapped. Draw a box, switch to text mode, and click",
        "inside it to type constrained within its walls."
      ];
    } else {
      lines = [
        "TEXT MODE - write your message like a normal editor",
        "",
        "Type to insert text; Enter starts a new paragraph.",
        "Arrows, Home/End, PgUp/PgDn move. Ins toggles overwrite.",
        "Click anywhere to place the cursor; wheel scrolls.",
        "Click inside a drawn box to type constrained within it;",
        'the menu\'s "Leave text box" returns to full width.',
        "",
        "^O save & post    ^R quote reply    ^D draw mode",
        "^L text colors    ^Z undo   ^Y redo   ^G this help",
        "^A abort          Esc opens the full menu.",
        "F1-F10 insert the art characters previewed on the bar",
        "above; F11/F12 or the   arrows cycle its sets.",
        "",
        "``` on its own line opens a code block (```js tags the",
        "language, bare ``` auto-detects); another ``` closes it.",
        "Arrows walk out of boxes through their top/bottom edge.",
        "",
        "The right-edge marker shows the 79-column safe width",
        "for posting; long paragraphs wrap automatically."
      ];
    }
    var w = 0;
    for (var i = 0; i < lines.length; i++)
      if (lines[i].length > w)
        w = lines[i].length;
    w += 6;
    var h = lines.length + 4;
    var x = center(w, scr.cols);
    var y = center(h, scr.rows);
    fillBox(scr, x, y, w, h, theme.modalBody);
    drawFrame(scr, x, y, w, h, theme.modalFrame, true, "Help  (^G)", theme.modalTitle);
    for (var li = 0; li < lines.length; li++)
      scr.putStr(x + 3, y + 2 + li, lines[li], theme.modalBody);
    scr.putStr(x + 3, y + h - 2, "Press any key or click to continue...", theme.modalTitle);
    scr.cursorVisible = false;
    scr.flush();
    while (alive()) {
      var ev = input(3e4);
      if (ev.type === "key")
        return;
      if (ev.type === "mouse" && ev.press)
        return;
    }
  }

  // build/ui/controller.js
  var DRAW_TOOLS = ["pencil", "type", "select", "line", "box", "circle", "fill", "recolor"];
  var DRAW_TOOL_LABEL = {
    pencil: "Pencil",
    type: "Type",
    select: "Select",
    line: "Line",
    box: "Box",
    circle: "Circle",
    fill: "Fill",
    recolor: "Recolor"
  };
  var CHANNEL_LABEL = { fg: "FG", bg: "BG", both: "Both" };
  var MESSAGE_SAFE_WIDTH = 79;
  var TAB_STOP = 4;
  var Controller = (
    /** @class */
    (function() {
      function Controller2(session, caps, scr, input, alive, fonts, getSize) {
        this.hits = new HitMap();
        this.mode = "text";
        this.topRow = 0;
        this.desiredCol = 0;
        this.brush = { x: 0, y: 0 };
        this.glyph = 219;
        this.recentGlyphs = [219, 176, 177, 178, 196, 179, 220, 223];
        this.charsetIdx = DEFAULT_CHARSET;
        this.drawTool = "pencil";
        this.anchor = null;
        this.previewEnd = null;
        this.textOrigin = 0;
        this.recolorChannel = "both";
        this.pencilSize = 1;
        this.lineMode = "char";
        this.boxStyle = "single";
        this.boxFill = "none";
        this.circleFill = "none";
        this.fillMode = "both";
        this.pendingStamp = null;
        this.pendingW = 0;
        this.pendingH = 0;
        this.wp = null;
        this.clipArt = null;
        this.clipText = null;
        this.selRect = null;
        this.textAnchor = null;
        this.saveMode = null;
        this.canvasTop = 3;
        this.canvasBottom = 0;
        this.canvasW = 0;
        this.panelX = -1;
        this.panelW = 0;
        this.bottomPanel = false;
        this.idle = false;
        this.fencedRows = [];
        this.session = session;
        this.caps = caps;
        this.scr = scr;
        this.input = input;
        this.alive = alive;
        this.fonts = fonts === void 0 ? null : fonts;
        this.getSize = getSize === void 0 ? null : getSize;
        this.subject = session.meta.subject;
        var docWidth = Math.min(MESSAGE_SAFE_WIDTH, caps.cols - 1);
        this.doc = new Document(docWidth);
        if (session.sourceText.length > 0)
          this.doc.loadText(session.sourceText);
        this.applyLayout();
      }
      Controller2.prototype.canvasRows = function() {
        return this.canvasBottom - this.canvasTop + 1;
      };
      Controller2.prototype.applyLayout = function() {
        var caps = this.caps;
        this.panelX = -1;
        this.panelW = 0;
        if (caps.cols >= 100) {
          this.panelX = this.doc.width + 1;
          this.panelW = Math.min(20, caps.cols - this.panelX);
        }
        this.canvasW = Math.min(this.doc.width, this.panelX === -1 ? caps.cols : this.panelX - 1);
        this.bottomPanel = this.panelX === -1;
        var panelRows = 0;
        if (this.bottomPanel) {
          panelRows = this.mode === "draw" ? 2 + (this.toolOptionGroups().length > 0 ? 1 : 0) : 1;
        }
        this.canvasBottom = caps.rows - 3 - panelRows;
      };
      Controller2.prototype.pollResize = function() {
        if (this.getSize === null)
          return;
        var sz = this.getSize();
        if (sz.cols === this.caps.cols && sz.rows === this.caps.rows)
          return;
        if (sz.cols < 40 || sz.rows < 10)
          return;
        this.caps.cols = sz.cols;
        this.caps.rows = sz.rows;
        this.scr.resize(sz.cols, sz.rows);
      };
      Controller2.prototype.ensureVisible = function(row) {
        var h = this.canvasRows();
        if (row < this.topRow)
          this.topRow = row;
        if (row >= this.topRow + h)
          this.topRow = row - h + 1;
        if (this.topRow < 0)
          this.topRow = 0;
      };
      Controller2.prototype.compose = function() {
        var scr = this.scr;
        var cols = this.caps.cols;
        var rows = this.caps.rows;
        this.hits.clear();
        scr.fill(0, 0, cols, 1, 32, theme.title);
        var title = " HERMedIT ";
        scr.putStr(0, 0, title, theme.title);
        var tx = title.length;
        var putCtx = function(s, attr) {
          if (tx < cols)
            scr.putStr(tx, 0, s.substring(0, cols - tx), attr);
          tx += s.length;
        };
        if (this.session.meta.area.length > 0)
          putCtx(this.session.meta.area + "  ", theme.titleDim);
        if (this.session.meta.to.length > 0) {
          putCtx("To: ", theme.titleDim);
          putCtx(this.session.meta.to, theme.titleValue);
          putCtx("  ", theme.titleDim);
        }
        putCtx("Subj: ", theme.titleDim);
        putCtx(this.subject.length > 0 ? this.subject : "(none)", theme.titleValue);
        var stamp = true ? "2026-07-19 14:38" : "dev";
        if (tx + stamp.length + 2 < cols) {
          scr.putStr(cols - stamp.length - 1, 0, stamp, theme.titleDim);
        }
        scr.fill(0, 1, cols, 1, 32, theme.bar);
        var bx = 1;
        bx += drawButton(scr, this.hits, bx, 1, "menu", "Esc", "Menu") + 1;
        bx += drawButton(scr, this.hits, bx, 1, "help", "^G", "Help") + 1;
        bx += drawButton(scr, this.hits, bx, 1, "save", "^O", "Save") + 1;
        if (this.session.quoteLines.length > 0) {
          bx += drawButton(scr, this.hits, bx, 1, "quote", "^R", "Quote") + 1;
        }
        if (this.mode === "text") {
          bx += drawButton(scr, this.hits, bx, 1, "mode-draw", "^D", "Draw", false) + 1;
        } else {
          bx += drawButton(scr, this.hits, bx, 1, "mode-text", "^T", "Text", true) + 1;
        }
        bx += drawButton(scr, this.hits, bx, 1, "color", "^L", "Color") + 1;
        if (this.mode === "draw") {
          bx += drawButton(scr, this.hits, bx, 1, "glyph", "^K", "Char") + 1;
        }
        var lowerDiv = this.bottomPanel ? this.canvasBottom + 2 : this.canvasBottom + 1;
        scr.hline(0, lowerDiv, cols, BOX.h, theme.divider);
        if (this.panelX >= 0) {
          var divX = this.panelX - 1;
          for (var dy = this.canvasTop; dy <= this.canvasBottom; dy++)
            scr.put(divX, dy, BOX.v, theme.divider);
          scr.put(divX, lowerDiv, BOX.teeUp, theme.divider);
        }
        for (var y = this.canvasTop; y <= this.canvasBottom; y++) {
          var docY = this.topRow + (y - this.canvasTop);
          for (var x = 0; x < this.canvasW; x++) {
            var cell = this.doc.cellAt(x, docY);
            scr.put(x, y, cell.ch, cell.attr);
          }
          if (this.doc.width < (this.panelX === -1 ? cols : this.panelX - 1)) {
            scr.put(this.doc.width, y, BOX.v, theme.boundary);
          }
          var clearFrom = this.doc.width + 1;
          var clearTo = this.panelX === -1 ? cols : this.panelX - 1;
          for (var cx = clearFrom; cx < clearTo; cx++)
            scr.put(cx, y, 32, theme.canvas);
          if (this.panelX >= 0) {
            for (var cx2 = this.panelX + this.panelW; cx2 < cols; cx2++)
              scr.put(cx2, y, 32, theme.canvas);
          }
        }
        this.hits.add("canvas", 0, this.canvasTop, this.canvasW - 1, this.canvasBottom);
        this.drawPreview();
        this.drawSelection();
        this.drawTextSelection();
        if (this.pendingStamp !== null)
          this.drawPendingStamp();
        if (this.wp !== null)
          this.drawWp();
        if (this.panelX >= 0)
          this.composePanel();
        if (this.bottomPanel)
          this.composeBottomPanel();
        this.composeCharsetBar();
        this.composeStatus();
        if (this.wp === null) {
          var cur = this.mode === "text" ? { x: this.doc.caretDocX(), y: this.doc.caretDocY() } : this.brush;
          scr.cursorX = clamp(cur.x, 0, this.canvasW - 1) + 1;
          scr.cursorY = this.canvasTop + (cur.y - this.topRow) + 1;
          scr.cursorVisible = true;
        }
      };
      Controller2.prototype.composePanel = function() {
        var scr = this.scr;
        var px = this.panelX;
        var pw = this.panelW;
        for (var y = this.canvasTop; y <= this.canvasBottom; y++)
          scr.fill(px, y, pw, 1, 32, theme.panel);
        var y0 = this.canvasTop;
        scr.putStr(px + 1, y0, "Mode", theme.panelTitle);
        var mx = px + 1;
        mx += drawButton(scr, this.hits, mx, y0 + 1, "mode-text", "^T", "Text", this.mode === "text") + 1;
        drawButton(scr, this.hits, mx, y0 + 1, "mode-draw", "^D", "Draw", this.mode === "draw");
        scr.putStr(px + 1, y0 + 3, "Foreground", theme.panelTitle);
        for (var f = 0; f < 16; f++) {
          var fx = px + 1 + f % 8 * 2;
          var fy = y0 + 4 + Math.floor(f / 8);
          var sw = makeAttr(f & 7, 0, f >= 8);
          var isCur = (this.doc.curAttr & 15) === f;
          scr.put(fx, fy, isCur ? 219 : 254, sw);
          this.hits.add("fg" + f, fx, fy, fx + 1, fy);
        }
        scr.putStr(px + 1, y0 + 6, "Background", theme.panelTitle);
        for (var b = 0; b < 8; b++) {
          var bxp = px + 1 + b * 2;
          var isCurB = (this.doc.curAttr >> 4 & 7) === b;
          scr.put(bxp, y0 + 7, isCurB ? 254 : 32, makeAttr(b === 7 ? 0 : 7, b, isCurB && b !== 7));
          this.hits.add("bg" + b, bxp, y0 + 7, bxp + 1, y0 + 7);
        }
        if (this.mode === "draw") {
          var py = y0 + 9;
          if (py <= this.canvasBottom) {
            scr.putStr(px + 1, py, "Tools", theme.panelTitle);
            scr.putStr(px + 8, py, "Tab", theme.keyHint);
          }
          py++;
          for (var tt = 0; tt < DRAW_TOOLS.length && py <= this.canvasBottom; tt++, py++) {
            var tool = DRAW_TOOLS[tt];
            var active = tool === this.drawTool;
            var tAttr = active ? theme.panelSel : theme.panel;
            scr.fill(px + 1, py, pw - 2, 1, 32, tAttr);
            scr.putStr(px + 2, py, (active ? " " : "  ") + DRAW_TOOL_LABEL[tool], tAttr);
            this.hits.add("tool-" + tool, px + 1, py, px + pw - 2, py);
          }
          if (this.toolOptionGroups().length > 0 && py <= this.canvasBottom) {
            scr.putStr(px + 2, py, "+ Options", theme.panelTitle);
            scr.putStr(px + 12, py, "S-Tab", theme.keyHint);
            this.hits.add("tool-opts", px + 1, py, px + pw - 2, py);
            py++;
          }
          py++;
          if (py <= this.canvasBottom) {
            scr.putStr(px + 1, py, "Char", theme.panelTitle);
            scr.put(px + 6, py, this.glyph, this.doc.curAttr);
            scr.putStr(px + 8, py, "^K", theme.keyHint);
            this.hits.add("glyph", px + 6, py, px + 6, py);
          }
          py++;
          if (py + 1 <= this.canvasBottom) {
            scr.putStr(px + 1, py, "Recent", theme.panelTitle);
            py++;
            for (var r = 0; r < this.recentGlyphs.length && r < 8; r++) {
              var rx = px + 1 + r * 2;
              scr.put(rx, py, this.recentGlyphs[r], theme.panel);
              this.hits.add("recent" + r, rx, py, rx + 1, py);
            }
          }
        } else {
          scr.putStr(px + 1, y0 + 9, "Keys", theme.panelTitle);
          scr.putStr(px + 1, y0 + 10, "Enter", theme.keyHint);
          scr.putStr(px + 7, y0 + 10, "paragraph", theme.panel);
          scr.putStr(px + 1, y0 + 11, "Ins", theme.keyHint);
          scr.putStr(px + 6, y0 + 11, "overwrite", theme.panel);
          scr.putStr(px + 1, y0 + 12, "^Z/^Y", theme.keyHint);
          scr.putStr(px + 7, y0 + 12, "undo/redo", theme.panel);
          scr.putStr(px + 1, y0 + 14, "Click places the", theme.panel);
          scr.putStr(px + 1, y0 + 15, "cursor; wheel", theme.panel);
          scr.putStr(px + 1, y0 + 16, "scrolls.", theme.panel);
        }
      };
      Controller2.prototype.composeBottomPanel = function() {
        var scr = this.scr;
        var y1 = this.canvasBottom + 3;
        var y2 = this.canvasBottom + 4;
        scr.fill(0, y1, this.caps.cols, this.mode === "draw" ? 2 : 1, 32, theme.panel);
        var x = 1;
        scr.putStr(x, y1, "FG", theme.panelTitle);
        x += 3;
        for (var f = 0; f < 16; f++) {
          var sw = makeAttr(f & 7, 0, f >= 8);
          var isCur = (this.doc.curAttr & 15) === f;
          scr.put(x, y1, isCur ? 219 : 254, sw);
          this.hits.add("fg" + f, x, y1, x, y1);
          x++;
        }
        x += 2;
        scr.putStr(x, y1, "BG", theme.panelTitle);
        x += 3;
        for (var b = 0; b < 8; b++) {
          var isCurB = (this.doc.curAttr >> 4 & 7) === b;
          scr.put(x, y1, isCurB ? 254 : 32, makeAttr(b === 7 ? 0 : 7, b, isCurB && b !== 7));
          this.hits.add("bg" + b, x, y1, x, y1);
          x++;
        }
        if (this.mode !== "draw")
          return;
        x += 2;
        scr.putStr(x, y1, "Char", theme.panelTitle);
        x += 5;
        scr.put(x, y1, this.glyph, this.doc.curAttr);
        this.hits.add("glyph", x, y1, x, y1);
        x += 2;
        scr.putStr(x, y1, "^K", theme.keyHint);
        x += 3;
        for (var r = 0; r < this.recentGlyphs.length && r < 8; r++) {
          scr.put(x, y1, this.recentGlyphs[r], theme.panel);
          this.hits.add("recent" + r, x, y1, x, y1);
          x++;
        }
        var tx = 1;
        scr.putStr(tx, y2, "Tab", theme.keyHint);
        tx += 4;
        for (var t = 0; t < DRAW_TOOLS.length; t++) {
          var tool = DRAW_TOOLS[t];
          var label = DRAW_TOOL_LABEL[tool];
          var active = tool === this.drawTool;
          scr.putStr(tx, y2, label, active ? theme.panelSel : theme.panel);
          this.hits.add("tool-" + tool, tx, y2, tx + label.length - 1, y2);
          tx += label.length + 1;
        }
        var groups = this.toolOptionGroups();
        if (groups.length === 0)
          return;
        var y3 = y2 + 1;
        scr.fill(0, y3, this.caps.cols, 1, 32, theme.panel);
        var ox = 1;
        scr.putStr(ox, y3, "S-Tab", theme.keyHint);
        this.hits.add("tool-opts", ox, y3, ox + 4, y3);
        ox += 6;
        for (var g = 0; g < groups.length; g++) {
          var grp = groups[g];
          scr.putStr(ox, y3, grp.title + ":", theme.panelTitle);
          ox += grp.title.length + 2;
          for (var o = 0; o < grp.opts.length; o++) {
            var opt = grp.opts[o];
            scr.putStr(ox, y3, opt.label, opt.cur ? theme.panelSel : theme.panel);
            this.hits.add(opt.id, ox, y3, ox + opt.label.length - 1, y3);
            ox += opt.label.length + 1;
          }
          ox += 2;
        }
      };
      Controller2.prototype.openToolOptionsMenu = function() {
        var groups = this.toolOptionGroups();
        if (groups.length === 0)
          return;
        var items = [];
        for (var g = 0; g < groups.length; g++) {
          var grp = groups[g];
          if (g > 0)
            items.push({ id: "sep-opt" + g, label: "", keyLabel: "", separator: true });
          for (var o = 0; o < grp.opts.length; o++) {
            var opt = grp.opts[o];
            items.push({ id: opt.id, label: (opt.cur ? "\x07 " : "  ") + grp.title + ": " + opt.label, keyLabel: "" });
          }
        }
        var mx = this.panelX >= 0 ? Math.max(1, this.panelX - 24) : 1;
        var id = dropdownMenu(this.scr, this.input, this.alive, mx, 2, items);
        if (id !== null)
          this.action(id);
      };
      Controller2.prototype.composeCharsetBar = function() {
        var scr = this.scr;
        var cols = this.caps.cols;
        var y = this.bottomPanel ? this.canvasBottom + 1 : this.caps.rows - 1;
        var set = CHARSETS[this.charsetIdx];
        scr.fill(0, y, cols, 1, 32, theme.charsetBar);
        var ind = this.charsetIdx + 1 + "/" + CHARSETS.length;
        var arrowW = 8;
        var fullW = arrowW + 1 + 41 + 1 + arrowW;
        var digitW = arrowW + 1 + 30 + 1 + arrowW;
        var fullLabels = fullW <= cols - 1;
        var clusterW = fullLabels ? fullW : digitW;
        var showInd = clusterW + 2 + ind.length <= cols - 1;
        var total = clusterW + (showInd ? 2 + ind.length : 0);
        var x = Math.max(0, Math.floor((cols - total) / 2));
        scr.put(x, y, 91, theme.button);
        scr.putStr(x + 1, y, "<-", theme.buttonArrow);
        scr.put(x + 3, y, 32, theme.button);
        scr.putStr(x + 4, y, "F11", theme.buttonKey);
        scr.put(x + 7, y, 93, theme.button);
        this.hits.add("charset-prev", x, y, x + 7, y);
        x += arrowW + 1;
        for (var i = 0; i < 10; i++) {
          var label = fullLabels ? "F" + (i + 1) : i === 9 ? "0" : String(i + 1);
          scr.putStr(x, y, label, theme.charsetKey);
          scr.put(x + label.length, y, set[i], this.doc.curAttr);
          this.hits.add("fkey" + i, x, y, x + label.length, y);
          x += label.length + 2;
        }
        x += 1;
        scr.put(x, y, 91, theme.button);
        scr.putStr(x + 1, y, "F12", theme.buttonKey);
        scr.put(x + 4, y, 32, theme.button);
        scr.putStr(x + 5, y, "->", theme.buttonArrow);
        scr.put(x + 7, y, 93, theme.button);
        this.hits.add("charset-next", x, y, x + 7, y);
        if (showInd)
          scr.putStr(x + arrowW + 2, y, ind, theme.charsetKey);
      };
      Controller2.prototype.composeStatus = function() {
        var scr = this.scr;
        var y = 2;
        var cols = this.caps.cols;
        scr.fill(0, y, cols, 1, 32, theme.status);
        var segs = [];
        var hints = [];
        var fgSwatch = { l: "FG:", a: "FG:", v: "", glyph: 219, gattr: this.doc.curAttr & 15, hit: "color" };
        var bgSwatch = { l: "BG:", a: "BG:", v: "", glyph: 32, gattr: this.doc.curAttr & 112, hit: "color" };
        if (this.wp !== null) {
          segs.push({ l: "Mode:", a: "Md:", v: "FONT WP" });
          segs.push({ l: "Font:", a: "Fn:", v: this.wp.curFont.name });
          segs.push(fgSwatch);
          segs.push(bgSwatch);
          hints = [{ key: "^K", word: "font" }, { key: "Esc", word: "done" }];
        } else if (this.pendingStamp !== null) {
          segs.push({ l: "Mode:", a: "Md:", v: "PLACE" });
          segs.push({ l: "Size:", a: "Sz:", v: this.pendingW + "x" + this.pendingH });
          segs.push({ l: "Pos:", a: "P:", v: this.brush.x + 1 + "," + (this.brush.y + 1) });
          hints = [{ key: "Enter", word: "stamp" }, { key: "Esc", word: "cancel" }];
        } else if (this.mode === "text") {
          segs.push({ l: "Mode:", a: "Md:", v: "TEXT" });
          var reg = this.doc.region;
          if (reg !== null && reg.pre === true) {
            var rl = reg.lang;
            segs.push({ l: "Code:", a: "Cd:", v: rl !== void 0 && rl !== "" ? rl : "plain" });
          } else if (reg !== null) {
            segs.push({ l: "Box:", a: "Bx:", v: reg.width + "x" + reg.height });
          }
          segs.push({ l: "Ln:", a: "Ln:", v: String(this.doc.caret.row + 1) });
          segs.push({ l: "Col:", a: "Co:", v: String(this.doc.caret.col + 1) });
          segs.push({ l: "", a: "", v: this.doc.insertMode ? "Ins" : "Ovr" });
          if (reg === null)
            segs.push({ l: "Width:", a: "W:", v: String(this.doc.width) });
          segs.push(fgSwatch);
          segs.push(bgSwatch);
        } else {
          segs.push({ l: "Mode:", a: "Md:", v: "DRAW" });
          var toolName = DRAW_TOOL_LABEL[this.drawTool];
          var tn = this.drawTool === "recolor" ? toolName + ":" + CHANNEL_LABEL[this.recolorChannel] : toolName;
          segs.push({ l: "Tool:", a: "Tl:", v: tn, hit: "tool-opts" });
          segs.push({ l: "Pos:", a: "P:", v: this.brush.x + 1 + "," + (this.brush.y + 1) });
          segs.push({ l: "Char:", a: "Ch:", v: "", glyph: this.glyph, gattr: this.doc.curAttr, hit: "glyph" });
          segs.push(fgSwatch);
          segs.push(bgSwatch);
          hints = [{ key: "Tab", word: "tool" }, { key: "S-Tab", word: "opts" }];
        }
        var hintW = 1;
        for (var h = 0; h < hints.length; h++)
          hintW += hints[h].key.length + 1 + hints[h].word.length + 2;
        var hx = cols - hintW;
        for (var h2 = 0; h2 < hints.length; h2++) {
          scr.putStr(hx, y, hints[h2].key, theme.keyHint);
          hx += hints[h2].key.length + 1;
          scr.putStr(hx, y, hints[h2].word, theme.statusLabel);
          hx += hints[h2].word.length + 2;
        }
        var limit = cols - hintW - 2;
        var full = 1;
        for (var m = 0; m < segs.length; m++) {
          var sm = segs[m];
          full += sm.l.length + (sm.glyph !== void 0 ? 1 : sm.v.length) + 2;
        }
        var abbr = full - 1 > limit;
        var x = 1;
        for (var i = 0; i < segs.length; i++) {
          var sg = segs[i];
          var label = abbr ? sg.a : sg.l;
          var vw = sg.glyph !== void 0 ? 1 : sg.v.length;
          if (x + label.length + vw > limit)
            break;
          var x0 = x;
          scr.putStr(x, y, label, theme.statusLabel);
          x += label.length;
          if (sg.glyph !== void 0) {
            scr.put(x, y, sg.glyph, sg.gattr === void 0 ? theme.statusValue : sg.gattr);
            x += 1;
          } else {
            scr.putStr(x, y, sg.v, theme.statusValue);
            x += sg.v.length;
          }
          if (sg.hit !== void 0)
            this.hits.add(sg.hit, x0, y, x - 1, y);
          x += 2;
        }
      };
      Controller2.prototype.run = function() {
        while (this.alive()) {
          if (this.idle)
            this.pollResize();
          this.applyLayout();
          var focusRow;
          if (this.wp !== null) {
            var wpLay = this.wpLayout();
            var cpos = tdfWpCaretXY(wpLay, this.wp.caret);
            focusRow = this.wp.originY + cpos.y + wpLay.lines[cpos.line].render.height - 1;
          } else {
            focusRow = this.mode === "text" ? this.doc.caretDocY() : this.brush.y;
          }
          this.ensureVisible(focusRow);
          this.compose();
          this.scr.flush();
          var ev = this.input(1e3);
          this.idle = ev.type === "none";
          if (ev.type === "none")
            continue;
          var result = ev.type === "mouse" ? this.handleMouse(ev) : this.handleKey(ev.key);
          if (result !== null)
            return result;
        }
        return { action: "abort", bodyCp437: this.doc.toMessageBody(true), subject: this.subject };
      };
      Controller2.prototype.screenToDoc = function(mx, my) {
        var x = mx - 1;
        var y = my - 1;
        if (y < this.canvasTop || y > this.canvasBottom)
          return null;
        if (x < 0 || x >= this.canvasW)
          return null;
        return { x: x, y: this.topRow + (y - this.canvasTop) };
      };
      Controller2.prototype.handleMouse = function(ev) {
        if (ev.wheel !== 0) {
          this.scrollBy(ev.wheel * 3);
          return null;
        }
        var pos = this.screenToDoc(ev.x, ev.y);
        if (this.wp !== null) {
          if (pos !== null) {
            if (ev.press && ev.button === 0) {
              this.wp.caret = tdfWpHitTest(this.wpLayout(), pos.x - this.wp.originX, pos.y - this.wp.originY);
              this.wpSyncFont();
            }
            return null;
          }
          if (ev.press && ev.button === 0) {
            var wid = this.hits.test(ev.x - 1, ev.y - 1);
            if (wid === null)
              return null;
            if (wid === "help" || wid === "color" || wid === "charset-prev" || wid === "charset-next" || wid.substring(0, 2) === "fg" || wid.substring(0, 2) === "bg") {
              return this.action(wid);
            }
            return this.finishWp();
          }
          return null;
        }
        if (this.pendingStamp !== null) {
          if (pos !== null) {
            this.brush = { x: pos.x, y: pos.y };
            if (ev.press && !ev.motion && ev.button === 0)
              this.commitStamp();
            else if (ev.press && !ev.motion && ev.button === 2)
              this.pendingStamp = null;
          }
          return null;
        }
        if (this.mode === "draw") {
          if (pos !== null) {
            this.handleDrawCanvasMouse(ev, pos);
            return null;
          }
          if (ev.release && this.anchor !== null && this.previewEnd !== null)
            this.commitShape();
          if (ev.press && ev.button === 0) {
            var did = this.hits.test(ev.x - 1, ev.y - 1);
            if (did !== null)
              return this.action(did);
          }
          return null;
        }
        if (pos !== null && ev.button === 0 && (ev.press || ev.motion)) {
          if (ev.press && !ev.motion) {
            this.placeCaretAt(pos.x, pos.y);
            this.textAnchor = { row: this.doc.caret.row, col: this.doc.caret.col };
          } else if (ev.motion && this.textAnchor !== null) {
            this.extendCaretTo(pos.x, pos.y);
          }
          return null;
        }
        if (ev.press && ev.button === 0) {
          var id = this.hits.test(ev.x - 1, ev.y - 1);
          if (id !== null)
            return this.action(id);
        }
        return null;
      };
      Controller2.prototype.placeCaretAt = function(docX, docY) {
        var active = this.doc.region;
        if (active !== null && this.withinBoxOuter(active, docX, docY)) {
          this.moveCaretInBox(active, docX, docY);
        } else {
          var box = this.doc.detectBox(docX, docY);
          if (box !== null) {
            this.doc.setRegion(box);
            this.moveCaretInBox(box, docX, docY);
          } else {
            this.doc.clearRegion();
            var row = clamp(docY, 0, this.doc.lines.length - 1);
            var l2 = this.doc.lines[row];
            this.doc.caret = { row: row, col: clamp(docX, 0, l2 === void 0 ? 0 : l2.text.length) };
          }
        }
        this.desiredCol = this.doc.caret.col;
        this.doc.breakUndoGroup();
      };
      Controller2.prototype.withinBoxOuter = function(r, docX, docY) {
        return docX >= r.left - 1 && docX <= r.left + r.width && docY >= r.top - 1 && docY <= r.top + r.height;
      };
      Controller2.prototype.moveCaretInBox = function(r, docX, docY) {
        var row = clamp(docY - r.top, 0, this.doc.lines.length - 1);
        var line = this.doc.lines[row];
        this.doc.caret = { row: row, col: clamp(docX - r.left, 0, line === void 0 ? 0 : line.text.length) };
      };
      Controller2.prototype.scrollBy = function(delta) {
        var maxTop = Math.max(0, this.doc.rowCount() - this.canvasRows());
        this.topRow = clamp(this.topRow + delta, 0, maxTop);
        if (this.mode === "text" && this.doc.region === null) {
          var r = clamp(this.doc.caret.row, this.topRow, this.topRow + this.canvasRows() - 1);
          if (r !== this.doc.caret.row) {
            var line = this.doc.lines[clamp(r, 0, this.doc.lines.length - 1)];
            this.doc.caret = {
              row: clamp(r, 0, this.doc.lines.length - 1),
              col: clamp(this.desiredCol, 0, line === void 0 ? 0 : line.text.length)
            };
          }
        } else if (this.mode === "draw") {
          this.brush.y = clamp(this.brush.y, this.topRow, this.topRow + this.canvasRows() - 1);
        }
      };
      Controller2.prototype.action = function(id) {
        if (id === "canvas")
          return null;
        if (id === "menu")
          return this.openMenu();
        if (id === "help") {
          helpOverlay(this.scr, this.input, this.alive, this.mode);
          return null;
        }
        if (id === "save")
          return this.trySave();
        if (id === "abort")
          return this.tryAbort();
        if (id === "quote") {
          this.openQuotes();
          return null;
        }
        if (id === "mode" || id === "mode-text" || id === "mode-draw") {
          if (id === "mode")
            this.mode = this.mode === "text" ? "draw" : "text";
          else
            this.mode = id === "mode-draw" ? "draw" : "text";
          if (this.mode === "draw") {
            this.brush = { x: clamp(this.doc.caret.col, 0, this.doc.width - 1), y: this.doc.caret.row };
          }
          return null;
        }
        if (id === "color") {
          var attr = colorPicker(this.scr, this.input, this.alive, this.doc.curAttr);
          if (attr !== null)
            this.doc.curAttr = attr;
          return null;
        }
        if (id === "glyph") {
          var g = glyphPicker(this.scr, this.input, this.alive, this.glyph);
          if (g !== null)
            this.setGlyph(g);
          return null;
        }
        if (id === "pick") {
          this.eyedrop();
          return null;
        }
        if (id === "tool-opts") {
          this.openToolOptionsMenu();
          return null;
        }
        if (id.substring(0, 5) === "tool-") {
          if (this.mode !== "draw")
            this.mode = "draw";
          this.setTool(id.substring(5));
          return null;
        }
        if (id.substring(0, 5) === "chan-") {
          this.recolorChannel = id.substring(5);
          return null;
        }
        if (id.substring(0, 4) === "opt-") {
          this.applyToolOption(id);
          return null;
        }
        if (id === "font") {
          this.openFontPicker();
          return null;
        }
        if (id === "fontwp") {
          this.openFontWordProcessor();
          return null;
        }
        if (id === "copy") {
          if (this.mode === "draw")
            this.copyArtSelection(false);
          else
            this.copyTextSelection(false);
          return null;
        }
        if (id === "cut") {
          if (this.mode === "draw")
            this.copyArtSelection(true);
          else
            this.copyTextSelection(true);
          return null;
        }
        if (id === "paste") {
          if (this.mode === "draw") {
            if (this.clipArt !== null)
              this.pasteArt();
          } else {
            if (this.clipText !== null)
              this.pasteText();
          }
          return null;
        }
        if (id === "code-insert") {
          this.insertCodeBlock();
          return null;
        }
        if (id === "subject") {
          var ns = promptLine(this.scr, this.input, this.alive, "Message subject", this.subject, 70);
          if (ns !== null)
            this.subject = ns;
          return null;
        }
        if (id === "undo") {
          this.doc.undo();
          return null;
        }
        if (id === "redo") {
          this.doc.redo();
          return null;
        }
        if (id === "leave-box") {
          this.doc.clearRegion();
          this.desiredCol = this.doc.caret.col;
          return null;
        }
        if (id.substring(0, 2) === "fg") {
          var f = parseInt(id.substring(2), 10);
          this.doc.curAttr = this.doc.curAttr & ~15 | f & 7 | (f >= 8 ? 8 : 0);
          return null;
        }
        if (id.substring(0, 2) === "bg") {
          var b = parseInt(id.substring(2), 10);
          this.doc.curAttr = this.doc.curAttr & ~112 | (b & 7) << 4;
          return null;
        }
        if (id.substring(0, 6) === "recent") {
          var r = parseInt(id.substring(6), 10);
          var rg = this.recentGlyphs[r];
          if (rg !== void 0)
            this.setGlyph(rg);
          return null;
        }
        if (id === "charset-prev") {
          this.cycleCharset(-1);
          return null;
        }
        if (id === "charset-next") {
          this.cycleCharset(1);
          return null;
        }
        if (id.substring(0, 4) === "fkey") {
          this.typeCharsetChar(parseInt(id.substring(4), 10));
          return null;
        }
        return null;
      };
      Controller2.prototype.cycleCharset = function(delta) {
        var n = CHARSETS.length;
        this.charsetIdx = (this.charsetIdx + delta + n) % n;
      };
      Controller2.prototype.typeCharsetChar = function(slot) {
        var set = CHARSETS[this.charsetIdx];
        if (set === void 0)
          return;
        var code2 = set[slot];
        if (code2 === void 0)
          return;
        if (this.mode === "draw") {
          this.doc.setArt(this.brush.x, this.brush.y, { ch: code2, attr: this.doc.curAttr });
          this.brush.x = clamp(this.brush.x + 1, 0, this.doc.width - 1);
        } else {
          this.textAnchor = null;
          this.doc.insertChar(code2);
          this.desiredCol = this.doc.caret.col;
          this.rehighlightCode();
        }
      };
      Controller2.prototype.setGlyph = function(g) {
        this.glyph = g;
        for (var i = 0; i < this.recentGlyphs.length; i++) {
          if (this.recentGlyphs[i] === g)
            this.recentGlyphs.splice(i, 1);
        }
        this.recentGlyphs.unshift(g);
        if (this.recentGlyphs.length > 8)
          this.recentGlyphs.pop();
      };
      Controller2.prototype.brushOffsets = function() {
        var s = this.pencilSize;
        var out = [];
        var from = s === 3 ? -1 : 0;
        var to = s === 1 ? 0 : 1;
        for (var dy = from; dy <= to; dy++) {
          for (var dx = from; dx <= to; dx++)
            out.push({ dx: dx, dy: dy });
        }
        return out;
      };
      Controller2.prototype.paint = function() {
        var offs = this.brushOffsets();
        for (var i = 0; i < offs.length; i++) {
          var x = this.brush.x + offs[i].dx;
          var y = this.brush.y + offs[i].dy;
          if (x < 0 || x >= this.doc.width || y < 0)
            continue;
          this.doc.setArt(x, y, { ch: this.glyph, attr: this.doc.curAttr });
        }
      };
      Controller2.prototype.eraseBrush = function(x, y) {
        var offs = this.drawTool === "pencil" ? this.brushOffsets() : [{ dx: 0, dy: 0 }];
        for (var i = 0; i < offs.length; i++) {
          var ex = x + offs[i].dx;
          var ey = y + offs[i].dy;
          if (ex < 0 || ex >= this.doc.width || ey < 0)
            continue;
          this.doc.eraseArt(ex, ey);
        }
      };
      Controller2.prototype.eyedrop = function() {
        var cell = this.doc.cellAt(this.brush.x, this.brush.y);
        this.setGlyph(cell.ch);
        this.doc.curAttr = cell.attr;
      };
      Controller2.prototype.toolOptionGroups = function() {
        var t = this.drawTool;
        if (t === "pencil") {
          return [{ title: "Size", opts: [
            { id: "opt-psize-1", label: "1", cur: this.pencilSize === 1 },
            { id: "opt-psize-2", label: "2", cur: this.pencilSize === 2 },
            { id: "opt-psize-3", label: "3", cur: this.pencilSize === 3 }
          ] }];
        }
        if (t === "type") {
          return [{ title: "Text", opts: [
            { id: "opt-type-ascii", label: "ASCII", cur: true },
            { id: "opt-type-tdf", label: "TDF font...", cur: false }
          ] }];
        }
        if (t === "line") {
          return [{ title: "Style", opts: [
            { id: "opt-line-char", label: "Char", cur: this.lineMode === "char" },
            { id: "opt-line-half", label: "Half-block", cur: this.lineMode === "half" }
          ] }];
        }
        if (t === "box") {
          return [
            { title: "Style", opts: [
              { id: "opt-boxstyle-single", label: "Single", cur: this.boxStyle === "single" },
              { id: "opt-boxstyle-double", label: "Double", cur: this.boxStyle === "double" },
              { id: "opt-boxstyle-char", label: "Char", cur: this.boxStyle === "char" },
              { id: "opt-boxstyle-half", label: "Half", cur: this.boxStyle === "half" }
            ] },
            { title: "Fill", opts: [
              { id: "opt-boxfill-none", label: "None", cur: this.boxFill === "none" },
              { id: "opt-boxfill-color", label: "Color", cur: this.boxFill === "color" },
              { id: "opt-boxfill-char", label: "Char", cur: this.boxFill === "char" }
            ] }
          ];
        }
        if (t === "circle") {
          return [{ title: "Fill", opts: [
            { id: "opt-circfill-none", label: "None", cur: this.circleFill === "none" },
            { id: "opt-circfill-color", label: "Color", cur: this.circleFill === "color" },
            { id: "opt-circfill-char", label: "Char", cur: this.circleFill === "char" }
          ] }];
        }
        if (t === "fill") {
          return [{ title: "Apply", opts: [
            { id: "opt-fillmode-both", label: "Char+Color", cur: this.fillMode === "both" },
            { id: "opt-fillmode-color", label: "Color", cur: this.fillMode === "color" },
            { id: "opt-fillmode-char", label: "Char", cur: this.fillMode === "char" }
          ] }];
        }
        if (t === "recolor") {
          return [{ title: "Channel", opts: [
            { id: "chan-fg", label: "FG", cur: this.recolorChannel === "fg" },
            { id: "chan-bg", label: "BG", cur: this.recolorChannel === "bg" },
            { id: "chan-both", label: "Both", cur: this.recolorChannel === "both" }
          ] }];
        }
        return [];
      };
      Controller2.prototype.applyToolOption = function(id) {
        if (id.substring(0, 10) === "opt-psize-") {
          this.pencilSize = parseInt(id.substring(10), 10);
          return true;
        }
        if (id === "opt-type-ascii")
          return true;
        if (id === "opt-type-tdf") {
          this.openFontWordProcessor();
          return true;
        }
        if (id.substring(0, 9) === "opt-line-") {
          this.lineMode = id.substring(9);
          return true;
        }
        if (id.substring(0, 13) === "opt-boxstyle-") {
          this.boxStyle = id.substring(13);
          return true;
        }
        if (id.substring(0, 12) === "opt-boxfill-") {
          this.boxFill = id.substring(12);
          return true;
        }
        if (id.substring(0, 13) === "opt-circfill-") {
          this.circleFill = id.substring(13);
          return true;
        }
        if (id.substring(0, 13) === "opt-fillmode-") {
          this.fillMode = id.substring(13);
          return true;
        }
        return false;
      };
      Controller2.prototype.setTool = function(tool) {
        this.drawTool = tool;
        this.anchor = null;
        this.previewEnd = null;
        if (tool !== "select")
          this.selRect = null;
        if (tool === "type")
          this.textOrigin = this.brush.x;
      };
      Controller2.prototype.cycleTool = function() {
        var i = 0;
        for (var t = 0; t < DRAW_TOOLS.length; t++)
          if (DRAW_TOOLS[t] === this.drawTool)
            i = t;
        this.setTool(DRAW_TOOLS[(i + 1) % DRAW_TOOLS.length]);
      };
      Controller2.prototype.handleDrawCanvasMouse = function(ev, pos) {
        this.brush = { x: pos.x, y: pos.y };
        var tool = this.drawTool;
        if (tool === "pencil") {
          if (ev.press || ev.motion) {
            if (ev.button === 0)
              this.paint();
            else if (ev.button === 2)
              this.eraseBrush(pos.x, pos.y);
            else if (ev.button === 1 && ev.press)
              this.eyedrop();
          }
          return;
        }
        var down = ev.press && !ev.motion;
        if (tool === "type") {
          if (down && ev.button === 0) {
            this.brush = { x: pos.x, y: pos.y };
            this.textOrigin = pos.x;
          } else if (down && ev.button === 2)
            this.doc.eraseArt(pos.x, pos.y);
          else if (down && ev.button === 1)
            this.eyedrop();
          return;
        }
        if (tool === "recolor") {
          if ((ev.press || ev.motion) && (ev.button === 0 || ev.button === 2))
            this.recolorAt(pos.x, pos.y);
          else if (down && ev.button === 1)
            this.eyedrop();
          return;
        }
        if (tool === "fill") {
          if (down && ev.button === 0)
            this.fillAt(pos.x, pos.y, false);
          else if (down && ev.button === 2)
            this.fillAt(pos.x, pos.y, true);
          else if (down && ev.button === 1)
            this.eyedrop();
          return;
        }
        var atAnchor = this.anchor !== null && this.anchor.x === pos.x && this.anchor.y === pos.y;
        var act = shapeToolAction(ev, this.anchor !== null, atAnchor);
        if (act === "eyedrop")
          this.eyedrop();
        else if (act === "cancel") {
          this.anchor = null;
          this.previewEnd = null;
        } else if (act === "set-anchor") {
          if (tool === "select")
            this.selRect = null;
          this.anchor = { x: pos.x, y: pos.y };
          this.previewEnd = { x: pos.x, y: pos.y };
        } else if (act === "preview")
          this.previewEnd = { x: pos.x, y: pos.y };
        else if (act === "commit") {
          this.previewEnd = { x: pos.x, y: pos.y };
          this.commitShape();
        } else if (tool === "select" && ev.release && atAnchor) {
          this.anchor = null;
          this.previewEnd = null;
          this.selRect = null;
        }
      };
      Controller2.prototype.fillGlyph = function(fill) {
        return fill === "color" ? 32 : this.glyph;
      };
      Controller2.prototype.shapeCells = function(a, b) {
        var out = [];
        var i = 0;
        if (this.drawTool === "line") {
          if (this.lineMode === "half")
            return halfBlockLineCells(a.x, a.y, b.x, b.y);
          var pts = linePoints(a.x, a.y, b.x, b.y);
          for (i = 0; i < pts.length; i++)
            out.push({ x: pts[i].x, y: pts[i].y, ch: this.glyph });
          return out;
        }
        if (this.drawTool === "box") {
          if (this.boxFill !== "none") {
            var fx0 = Math.min(a.x, b.x) + 1;
            var fx1 = Math.max(a.x, b.x) - 1;
            var fy0 = Math.min(a.y, b.y) + 1;
            var fy1 = Math.max(a.y, b.y) - 1;
            var fg = this.fillGlyph(this.boxFill);
            for (var by = fy0; by <= fy1; by++) {
              for (var bx = fx0; bx <= fx1; bx++)
                out.push({ x: bx, y: by, ch: fg });
            }
          }
          var border = this.boxStyle === "half" ? halfBlockBoxCells(a.x, a.y, b.x, b.y) : boxCells(a.x, a.y, b.x, b.y, this.boxStyle === "double");
          for (i = 0; i < border.length; i++) {
            var bc = border[i];
            out.push({ x: bc.x, y: bc.y, ch: this.boxStyle === "char" ? this.glyph : bc.ch });
          }
          return out;
        }
        if (this.drawTool === "circle") {
          if (this.circleFill !== "none") {
            var fp = ellipseFillPoints(a.x, a.y, b.x, b.y);
            var cg = this.fillGlyph(this.circleFill);
            for (i = 0; i < fp.length; i++)
              out.push({ x: fp[i].x, y: fp[i].y, ch: cg });
          }
          var ep = ellipsePoints(a.x, a.y, b.x, b.y);
          for (i = 0; i < ep.length; i++)
            out.push({ x: ep[i].x, y: ep[i].y, ch: this.glyph });
          return out;
        }
        return out;
      };
      Controller2.prototype.commitShape = function() {
        if (this.anchor === null || this.previewEnd === null)
          return;
        if (this.drawTool === "select") {
          this.selRect = normRect(this.anchor, this.previewEnd);
          this.anchor = null;
          this.previewEnd = null;
          return;
        }
        var cells = this.shapeCells(this.anchor, this.previewEnd);
        var out = [];
        for (var i = 0; i < cells.length; i++) {
          out.push({ x: cells[i].x, y: cells[i].y, ch: cells[i].ch, attr: this.doc.curAttr });
        }
        this.doc.paintCells(out);
        this.anchor = null;
        this.previewEnd = null;
      };
      Controller2.prototype.copyArtSelection = function(cut) {
        if (this.selRect === null)
          return;
        var r = this.selRect;
        var cells = [];
        var erase = [];
        for (var y = r.y0; y <= r.y1; y++) {
          for (var x = r.x0; x <= r.x1; x++) {
            var a = this.doc.artAt(x, y);
            if (a !== null) {
              cells.push({ dx: x - r.x0, dy: y - r.y0, ch: a.ch, attr: a.attr });
              if (cut)
                erase.push({ x: x, y: y, ch: -1, attr: 0 });
            }
          }
        }
        this.clipArt = { w: r.x1 - r.x0 + 1, h: r.y1 - r.y0 + 1, cells: cells };
        if (cut && erase.length > 0)
          this.doc.paintCells(erase);
      };
      Controller2.prototype.textSelectionRange = function() {
        if (this.textAnchor === null)
          return null;
        var a = this.textAnchor;
        var b = this.doc.caret;
        if (a.row === b.row && a.col === b.col)
          return null;
        if (a.row < b.row || a.row === b.row && a.col < b.col)
          return { r0: a.row, c0: a.col, r1: b.row, c1: b.col };
        return { r0: b.row, c0: b.col, r1: a.row, c1: a.col };
      };
      Controller2.prototype.copyTextSelection = function(cut) {
        var range = this.textSelectionRange();
        if (range === null)
          return;
        this.clipText = this.doc.getRangeText(range.r0, range.c0, range.r1, range.c1);
        if (cut)
          this.doc.deleteRange(range.r0, range.c0, range.r1, range.c1);
        this.textAnchor = null;
        this.desiredCol = this.doc.caret.col;
      };
      Controller2.prototype.pasteText = function() {
        if (this.clipText === null)
          return;
        if (this.mode !== "text")
          this.mode = "text";
        var range = this.textSelectionRange();
        if (range !== null)
          this.doc.deleteRange(range.r0, range.c0, range.r1, range.c1);
        this.textAnchor = null;
        this.doc.insertString(this.clipText);
        this.desiredCol = this.doc.caret.col;
        this.rehighlightCode();
      };
      Controller2.prototype.extendCaretTo = function(docX, docY) {
        if (this.doc.region !== null) {
          this.moveCaretInBox(this.doc.region, docX, docY);
        } else {
          var row = clamp(docY, 0, this.doc.lines.length - 1);
          var l = this.doc.lines[row];
          this.doc.caret = { row: row, col: clamp(docX, 0, l === void 0 ? 0 : l.text.length) };
        }
        this.desiredCol = this.doc.caret.col;
      };
      Controller2.prototype.drawTextSelection = function() {
        if (this.mode !== "text")
          return;
        var range = this.textSelectionRange();
        if (range === null)
          return;
        var offX = this.doc.region ? this.doc.region.left : 0;
        var offY = this.doc.region ? this.doc.region.top : 0;
        var attr = makeAttr(7, 4, true);
        for (var row = range.r0; row <= range.r1; row++) {
          var line = this.doc.lines[row];
          if (line === void 0)
            continue;
          var cStart = row === range.r0 ? range.c0 : 0;
          var cEnd = row === range.r1 ? range.c1 : line.text.length;
          for (var col = cStart; col < cEnd; col++) {
            var dx = offX + col;
            var sy = this.canvasTop + (offY + row - this.topRow);
            if (sy < this.canvasTop || sy > this.canvasBottom)
              continue;
            if (dx < 0 || dx >= this.canvasW)
              continue;
            var ch = col < line.text.length ? line.text.charCodeAt(col) & 255 : 32;
            this.scr.put(dx, sy, ch, attr);
          }
        }
      };
      Controller2.prototype.pasteArt = function() {
        if (this.clipArt === null)
          return;
        if (this.mode !== "draw")
          this.mode = "draw";
        var out = [];
        for (var i = 0; i < this.clipArt.cells.length; i++) {
          var c = this.clipArt.cells[i];
          out.push({ x: this.brush.x + c.dx, y: this.brush.y + c.dy, ch: c.ch, attr: c.attr });
        }
        this.doc.paintCells(out);
      };
      Controller2.prototype.recolorAt = function(x, y) {
        this.doc.recolorCell(x, y, this.doc.curAttr, this.recolorChannel);
      };
      Controller2.prototype.fillAt = function(x, y, erase) {
        var self = this;
        var width = this.doc.width;
        var height = Math.max(this.doc.rowCount(), y + 1) + 1;
        var sample = function(sx, sy) {
          var c = self.doc.cellAt(sx, sy);
          return c.ch + ":" + c.attr;
        };
        var pts = floodFill(x, y, width, height, sample);
        var cells = [];
        for (var i = 0; i < pts.length; i++) {
          var px = pts[i].x;
          var py = pts[i].y;
          if (erase) {
            cells.push({ x: px, y: py, ch: -1, attr: 0 });
            continue;
          }
          var existing = this.doc.artAt(px, py);
          var ch = this.fillMode === "color" ? existing !== null ? existing.ch : 32 : this.glyph;
          var attr = this.fillMode === "char" ? existing !== null ? existing.attr : this.doc.curAttr : this.doc.curAttr;
          cells.push({ x: px, y: py, ch: ch, attr: attr });
        }
        this.doc.paintCells(cells);
      };
      Controller2.prototype.drawPreview = function() {
        if (this.mode !== "draw" || this.anchor === null || this.previewEnd === null)
          return;
        if (this.drawTool === "select") {
          this.highlightRect(normRect(this.anchor, this.previewEnd));
          return;
        }
        var cells = this.shapeCells(this.anchor, this.previewEnd);
        for (var i = 0; i < cells.length; i++) {
          var c = cells[i];
          var sy = this.canvasTop + (c.y - this.topRow);
          if (sy < this.canvasTop || sy > this.canvasBottom)
            continue;
          if (c.x < 0 || c.x >= this.canvasW)
            continue;
          this.scr.put(c.x, sy, c.ch, this.doc.curAttr);
        }
      };
      Controller2.prototype.drawSelection = function() {
        if (this.mode !== "draw" || this.drawTool !== "select" || this.selRect === null)
          return;
        this.highlightRect(this.selRect);
      };
      Controller2.prototype.highlightRect = function(r) {
        var attr = makeAttr(7, 0, true);
        var self = this;
        var put = function(x2, y2, ch) {
          if (x2 < 0 || x2 >= self.canvasW)
            return;
          var sy = self.canvasTop + (y2 - self.topRow);
          if (sy < self.canvasTop || sy > self.canvasBottom)
            return;
          self.scr.put(x2, sy, ch, attr);
        };
        for (var x = r.x0 + 1; x < r.x1; x++) {
          put(x, r.y0, 45);
          put(x, r.y1, 45);
        }
        for (var y = r.y0 + 1; y < r.y1; y++) {
          put(r.x0, y, 124);
          put(r.x1, y, 124);
        }
        put(r.x0, r.y0, 218);
        put(r.x1, r.y0, 191);
        put(r.x0, r.y1, 192);
        put(r.x1, r.y1, 217);
      };
      Controller2.prototype.openMenu = function() {
        var items = [
          { id: "save", label: "Save & post message", keyLabel: "^O" },
          { id: "subject", label: "Edit subject", keyLabel: "" }
        ];
        if (this.session.quoteLines.length > 0) {
          items.push({ id: "quote", label: "Quote original", keyLabel: "^R" });
        }
        items.push({ id: "sep1", label: "", keyLabel: "", separator: true });
        if (this.mode === "text") {
          items.push({ id: "mode-draw", label: "Switch to draw mode", keyLabel: "^D" });
        } else {
          items.push({ id: "mode-text", label: "Switch to text mode", keyLabel: "^T" });
        }
        items.push({ id: "color", label: "Colors...", keyLabel: "^L" });
        if (this.fonts !== null) {
          items.push({ id: "fontwp", label: "Font word processor...", keyLabel: "" });
        }
        if (this.mode === "draw") {
          items.push({ id: "glyph", label: "Character...", keyLabel: "^K" });
          if (this.fonts !== null) {
            items.push({ id: "font", label: "Font text (stamp)...", keyLabel: "" });
          }
          items.push({ id: "sep-tools", label: "", keyLabel: "", separator: true });
          for (var ti = 0; ti < DRAW_TOOLS.length; ti++) {
            var tool = DRAW_TOOLS[ti];
            var mark = tool === this.drawTool ? "\x07 " : "  ";
            items.push({ id: "tool-" + tool, label: mark + DRAW_TOOL_LABEL[tool], keyLabel: ti === 0 ? "Tab" : "" });
          }
          if (this.toolOptionGroups().length > 0) {
            items.push({ id: "tool-opts", label: "  Tool options...", keyLabel: "" });
          }
        }
        if (this.mode === "text" && this.doc.region === null) {
          items.push({ id: "code-insert", label: "Insert code block...", keyLabel: "" });
        }
        if (this.mode === "text" && this.doc.region !== null) {
          items.push({ id: "leave-box", label: "Leave text box (full width)", keyLabel: "" });
        }
        items.push({ id: "sep2", label: "", keyLabel: "", separator: true });
        items.push({ id: "undo", label: "Undo", keyLabel: "^Z" });
        items.push({ id: "redo", label: "Redo", keyLabel: "^Y" });
        var canCopy = this.mode === "draw" ? this.selRect !== null : this.textAnchor !== null;
        var canPaste = this.mode === "draw" ? this.clipArt !== null : this.clipText !== null;
        if (canCopy) {
          items.push({ id: "copy", label: "Copy", keyLabel: "^C" });
          items.push({ id: "cut", label: "Cut", keyLabel: "^X" });
        }
        if (canPaste)
          items.push({ id: "paste", label: "Paste", keyLabel: "^V" });
        items.push({ id: "sep3", label: "", keyLabel: "", separator: true });
        items.push({ id: "help", label: "Help", keyLabel: "^G" });
        items.push({ id: "abort", label: "Abort message", keyLabel: "^A" });
        var id = dropdownMenu(this.scr, this.input, this.alive, 1, 2, items);
        if (id === null)
          return null;
        return this.action(id);
      };
      Controller2.prototype.pickFenceLang = function() {
        var items = [{ id: "auto", label: "  Auto-detect", keyLabel: "" }];
        for (var i = 0; i < LANGS.length; i++) {
          var L = LANGS[i];
          items.push({ id: L.id, label: "  " + L.name, keyLabel: "" });
        }
        var id = dropdownMenu(this.scr, this.input, this.alive, 4, 4, items);
        if (id === null)
          return null;
        return id === "auto" ? "" : id;
      };
      Controller2.prototype.insertCodeBlock = function(langTag) {
        if (this.mode !== "text")
          this.mode = "text";
        if (this.doc.region !== null)
          this.doc.clearRegion();
        var tag = langTag === void 0 ? this.pickFenceLang() : langTag;
        if (tag === null)
          return;
        var row = this.doc.caret.row;
        var mk = function(t) {
          var a = [];
          for (var i = 0; i < t.length; i++)
            a.push(HL_COMMENT);
          return { text: t, attr: a };
        };
        this.doc.insertLines([mk("```" + tag), mk(""), mk("```")]);
        this.doc.caret = { row: row + 1, col: 0 };
        this.desiredCol = 0;
        this.rehighlightCode();
      };
      Controller2.prototype.rehighlightCode = function() {
        var r = this.doc.region;
        if (r === null) {
          this.rehighlightBody();
          return;
        }
        if (r.pre !== true)
          return;
        var def = langById(r.lang === void 0 ? "" : r.lang);
        if (def === null)
          return;
        highlightLines(this.doc.lines, def);
      };
      Controller2.prototype.rehighlightBody = function() {
        if (this.doc.region !== null)
          return;
        var lines = this.doc.lines;
        var touched = [];
        var r = 0;
        while (r < lines.length) {
          var tag = fenceTag(lines[r].text);
          if (tag === null) {
            r++;
            continue;
          }
          var end = lines.length;
          for (var e = r + 1; e < lines.length; e++) {
            if (fenceTag(lines[e].text) !== null) {
              end = e;
              break;
            }
          }
          var sample = [];
          for (var s = r + 1; s < end; s++)
            sample.push(lines[s].text);
          var def = resolveFenceLang(tag, sample);
          this.paintLineAttr(r, HL_COMMENT);
          touched.push(r);
          var st = initialHlState();
          for (var i = r + 1; i < end; i++) {
            lines[i].attr = highlightLine(lines[i].text, st, def);
            touched.push(i);
          }
          if (end < lines.length) {
            this.paintLineAttr(end, HL_COMMENT);
            touched.push(end);
          }
          r = end + 1;
        }
        for (var o = 0; o < this.fencedRows.length; o++) {
          var row = this.fencedRows[o];
          var still = false;
          for (var t = 0; t < touched.length; t++)
            if (touched[t] === row) {
              still = true;
              break;
            }
          if (!still && row < lines.length)
            this.paintLineAttr(row, DEFAULT_ATTR);
        }
        this.fencedRows = touched;
      };
      Controller2.prototype.paintLineAttr = function(row, attr) {
        var l = this.doc.lines[row];
        if (l === void 0)
          return;
        var attrs = [];
        for (var i = 0; i < l.text.length; i++)
          attrs.push(attr);
        l.attr = attrs;
      };
      Controller2.prototype.exitBox = function(below) {
        var r = this.doc.region;
        if (r === null)
          return;
        this.doc.clearRegion();
        var row = below ? r.top + r.height + 1 : r.top - 2;
        row = clamp(row, 0, this.doc.lines.length - 1);
        var line = this.doc.lines[row];
        this.doc.caret = { row: row, col: clamp(this.desiredCol, 0, line === void 0 ? 0 : line.text.length) };
        this.doc.breakUndoGroup();
      };
      Controller2.prototype.openQuotes = function() {
        if (this.mode !== "text")
          this.mode = "text";
        var picked = quotePicker(this.scr, this.input, this.alive, this.session.quoteLines);
        if (picked === null || picked.length === 0)
          return;
        var author = this.session.meta.to;
        var styleChoice = messageBox(this.scr, this.input, this.alive, "Quote style", ["How should the quoted text be formatted?"], [
          { id: "gt", key: ">", label: "Standard >" },
          { id: "initials", key: "I", label: author.length > 0 ? authorInitials(author) + ">" : "Initials>" },
          { id: "none", key: "P", label: "Plain" }
        ], 0);
        if (styleChoice === null)
          return;
        var style = styleChoice;
        var attribution = false;
        if (author.length > 0) {
          attribution = messageBox(this.scr, this.input, this.alive, "Attribution", ['Add a "' + author + ' wrote:" header?'], [{ id: "yes", key: "Y", label: "Yes" }, { id: "no", key: "N", label: "No" }], 1) === "yes";
        }
        var quoted = formatQuote(picked, author, style, attribution);
        var toInsert = [];
        var prefix = quotePrefix(style, author);
        var wrapWidth = this.doc.region ? this.doc.region.width : this.doc.width;
        for (var i = 0; i < quoted.length; i++) {
          var q = quoted[i];
          while (q.length > wrapWidth) {
            var brk = q.lastIndexOf(" ", wrapWidth - 1);
            if (brk <= prefix.length)
              brk = wrapWidth;
            toInsert.push(quoteLineObj(q.substring(0, brk)));
            q = prefix + q.substring(brk).replace(/^ +/, "");
          }
          toInsert.push(quoteLineObj(q));
        }
        this.doc.insertLines(toInsert);
      };
      Controller2.prototype.detectSaveMode = function() {
        if (this.saveMode !== null)
          return this.saveMode;
        var body = this.doc.flowList()[0].lines;
        for (var i = 0; i < body.length; i++) {
          if (fenceTag(body[i].text) !== null)
            return "ctrla";
        }
        var art = this.doc.artCellCount();
        var prose = this.doc.proseCharCount();
        return art >= 40 && art > prose ? "ansi" : "ctrla";
      };
      Controller2.prototype.ansiTagSubject = function(subject, ansi) {
        var m = /^\s*\[ANSI\]\s*/i.exec(subject);
        var base = m ? subject.substring(m[0].length) : subject;
        return ansi ? "[ANSI] " + base : base;
      };
      Controller2.prototype.trySave = function() {
        var mode = this.detectSaveMode();
        var lines = [];
        if (this.session.meta.to.length > 0)
          lines.push("To:      " + this.session.meta.to);
        lines.push("Subject: " + (this.subject.length > 0 ? this.subject : "(none)"));
        lines.push("Format:  " + (mode === "ansi" ? "ANSI art  (subject gets [ANSI]; needs an ANSI terminal)" : "Colored text (Ctrl-A; degrades to monochrome cleanly)"));
        lines.push("");
        lines.push("Post this message?");
        var buttons = [
          { id: "post", key: "Enter", label: "Post" },
          { id: "format", key: "F", label: mode === "ansi" ? "Use text" : "Use ANSI" },
          { id: "subject", key: "S", label: "Subject" },
          { id: "back", key: "Esc", label: "Keep editing" }
        ];
        var choice = messageBox(this.scr, this.input, this.alive, "Save message", lines, buttons, 0);
        if (choice === "post") {
          var body = mode === "ansi" ? this.doc.toAnsiBody() : this.doc.toMessageBody(true);
          return { action: "save", bodyCp437: body, subject: this.ansiTagSubject(this.subject, mode === "ansi") };
        }
        if (choice === "format") {
          this.saveMode = mode === "ansi" ? "ctrla" : "ansi";
          return this.trySave();
        }
        if (choice === "subject") {
          this.action("subject");
          return this.trySave();
        }
        return null;
      };
      Controller2.prototype.tryAbort = function() {
        if (!this.doc.dirty) {
          return { action: "abort", bodyCp437: "", subject: this.subject };
        }
        var choice = messageBox(this.scr, this.input, this.alive, "Abort message", ["Throw away this message?", "Unsaved text and artwork will be lost."], [
          { id: "discard", key: "D", label: "Discard" },
          { id: "back", key: "Esc", label: "Keep editing" }
        ], 1);
        if (choice === "discard")
          return { action: "abort", bodyCp437: "", subject: this.subject };
        return null;
      };
      Controller2.prototype.handleKey = function(k) {
        if (this.wp !== null)
          return this.handleWpKey(k);
        if (this.pendingStamp !== null)
          return this.handleStampKey(k);
        if (k === KEY_ESC)
          return this.openMenu();
        var fk = fkeySlot(k);
        if (fk >= 0) {
          this.typeCharsetChar(fk);
          return null;
        }
        if (k === "F11" || k === "C-,") {
          this.cycleCharset(-1);
          return null;
        }
        if (k === "F12" || k === "C-.") {
          this.cycleCharset(1);
          return null;
        }
        if (k === "C-/") {
          this.charsetIdx = DEFAULT_CHARSET;
          return null;
        }
        if (k === CTRL_G)
          return this.action("help");
        if (k === CTRL_O || k === CTRL_S)
          return this.action("save");
        if (k === CTRL_A || k === CTRL_Q)
          return this.action("abort");
        if (k === CTRL_R)
          return this.action("quote");
        if (k === CTRL_T)
          return this.action("mode-text");
        if (k === CTRL_D)
          return this.action("mode-draw");
        if (k === CTRL_L)
          return this.action("color");
        if (k === CTRL_Z)
          return this.action("undo");
        if (k === CTRL_Y)
          return this.action("redo");
        if (k === CTRL_C)
          return this.action("copy");
        if (k === CTRL_X)
          return this.action("cut");
        if (k === KEY_INSERT2)
          return this.action("paste");
        if (this.mode === "draw") {
          if (k === CTRL_K)
            return this.action("glyph");
          if (k === CTRL_W)
            return this.action("pick");
          return this.handleDrawKey(k);
        }
        return this.handleTextKey(k);
      };
      Controller2.prototype.handleTextKey = function(k) {
        var doc = this.doc;
        this.textAnchor = null;
        if (k === KEY_LEFT2) {
          doc.moveLeft();
          this.desiredCol = doc.caret.col;
          doc.breakUndoGroup();
        } else if (k === KEY_RIGHT2) {
          doc.moveRight();
          this.desiredCol = doc.caret.col;
          doc.breakUndoGroup();
        } else if (k === KEY_UP2) {
          if (doc.region !== null && doc.caret.row === 0)
            this.exitBox(false);
          else {
            doc.moveVert(-1, this.desiredCol);
            doc.breakUndoGroup();
          }
        } else if (k === KEY_DOWN2) {
          if (doc.region !== null && doc.caret.row >= doc.lines.length - 1)
            this.exitBox(true);
          else {
            doc.moveVert(1, this.desiredCol);
            doc.breakUndoGroup();
          }
        } else if (k === KEY_PAGEUP2) {
          doc.moveVert(-this.canvasRows(), this.desiredCol);
          doc.breakUndoGroup();
        } else if (k === KEY_PAGEDN2) {
          doc.moveVert(this.canvasRows(), this.desiredCol);
          doc.breakUndoGroup();
        } else if (k === KEY_HOME2) {
          doc.moveHome();
          this.desiredCol = 0;
        } else if (k === KEY_END2) {
          doc.moveEnd();
          this.desiredCol = doc.caret.col;
        } else if (k === KEY_INSERT2) {
          doc.insertMode = !doc.insertMode;
        } else if (k === KEY_ENTER) {
          if (doc.region !== null && doc.region.pre === true && doc.curLineText() === "```") {
            doc.deleteRange(doc.caret.row, 0, doc.caret.row, 3);
            this.exitBox(true);
            return null;
          }
          doc.insertBreak();
          this.desiredCol = 0;
        } else if (k === KEY_BACKSPACE || k === KEY_DEL2) {
          doc.backspace();
          this.desiredCol = doc.caret.col;
        } else if (k === KEY_TAB) {
          var n = TAB_STOP - doc.caret.col % TAB_STOP;
          for (var i = 0; i < n; i++)
            doc.insertChar(32);
          this.desiredCol = doc.caret.col;
        } else if (isPrintable(k)) {
          doc.insertChar(k.charCodeAt(0));
          this.desiredCol = doc.caret.col;
        }
        this.rehighlightCode();
        return null;
      };
      Controller2.prototype.handleDrawKey = function(k) {
        if (k === KEY_TAB) {
          this.cycleTool();
          return null;
        }
        if (k === "STAB") {
          this.openToolOptionsMenu();
          return null;
        }
        if (this.drawTool === "type")
          return this.handleTypeKey(k);
        if (this.drawTool === "recolor")
          return this.handleRecolorKey(k);
        var maxY = Math.max(this.doc.rowCount() + this.canvasRows(), this.brush.y + 1);
        var twoPoint = this.drawTool === "line" || this.drawTool === "box" || this.drawTool === "circle" || this.drawTool === "select";
        if (k === KEY_LEFT2)
          this.brush.x = clamp(this.brush.x - 1, 0, this.doc.width - 1);
        else if (k === KEY_RIGHT2)
          this.brush.x = clamp(this.brush.x + 1, 0, this.doc.width - 1);
        else if (k === KEY_UP2)
          this.brush.y = clamp(this.brush.y - 1, 0, maxY);
        else if (k === KEY_DOWN2)
          this.brush.y = clamp(this.brush.y + 1, 0, maxY);
        else if (k === KEY_PAGEUP2)
          this.brush.y = clamp(this.brush.y - this.canvasRows(), 0, maxY);
        else if (k === KEY_PAGEDN2)
          this.brush.y = clamp(this.brush.y + this.canvasRows(), 0, maxY);
        else if (k === KEY_HOME2)
          this.brush.x = 0;
        else if (k === KEY_END2)
          this.brush.x = this.doc.width - 1;
        else if (k === " " || k === KEY_ENTER) {
          if (this.drawTool === "pencil") {
            this.paint();
            this.brush.x = clamp(this.brush.x + 1, 0, this.doc.width - 1);
          } else if (this.drawTool === "fill") {
            this.fillAt(this.brush.x, this.brush.y, false);
          } else if (this.anchor === null) {
            if (this.drawTool === "select")
              this.selRect = null;
            this.anchor = { x: this.brush.x, y: this.brush.y };
            this.previewEnd = { x: this.brush.x, y: this.brush.y };
          } else {
            this.previewEnd = { x: this.brush.x, y: this.brush.y };
            this.commitShape();
          }
        } else if (k === KEY_DEL2 || k === KEY_BACKSPACE) {
          if (this.anchor !== null) {
            this.anchor = null;
            this.previewEnd = null;
          } else {
            if (k === KEY_BACKSPACE)
              this.brush.x = clamp(this.brush.x - 1, 0, this.doc.width - 1);
            this.doc.eraseArt(this.brush.x, this.brush.y);
          }
        } else if (this.drawTool === "pencil" && isPrintable(k)) {
          this.doc.setArt(this.brush.x, this.brush.y, { ch: k.charCodeAt(0), attr: this.doc.curAttr });
          this.brush.x = clamp(this.brush.x + 1, 0, this.doc.width - 1);
        }
        if (twoPoint && this.anchor !== null)
          this.previewEnd = { x: this.brush.x, y: this.brush.y };
        return null;
      };
      Controller2.prototype.openFontPicker = function() {
        if (this.fonts === null)
          return;
        if (this.mode !== "draw")
          this.mode = "draw";
        var choice = fontPicker(this.scr, this.input, this.alive, this.fonts, "");
        this.scr.invalidate();
        if (choice === null)
          return;
        var font = this.fonts.load(choice.fontName);
        if (font === null)
          return;
        var render = renderTdf(font, choice.text);
        var isColor = font.fonttype === COLOR_FONT;
        var cells = [];
        for (var yy = 0; yy < render.rows.length; yy++) {
          var row = render.rows[yy];
          for (var xx = 0; xx < row.length; xx++) {
            var c = row[xx];
            if (c.ch === 32 && !(isColor && c.color & 112))
              continue;
            cells.push({ x: xx, y: yy, ch: c.ch, attr: isColor ? c.color & 255 : this.doc.curAttr });
          }
        }
        if (cells.length === 0)
          return;
        this.pendingStamp = cells;
        this.pendingW = render.width;
        this.pendingH = render.height;
      };
      Controller2.prototype.openFontWordProcessor = function() {
        if (this.fonts === null)
          return;
        if (this.mode !== "draw")
          this.mode = "draw";
        var choice = fontPicker(this.scr, this.input, this.alive, this.fonts, "");
        this.scr.invalidate();
        if (choice === null)
          return;
        var font = this.fonts.load(choice.fontName);
        if (font === null)
          return;
        this.wp = { fonts: [], curFont: font, text: "", caret: 0, originX: this.brush.x, originY: this.brush.y, gap: 0 };
      };
      Controller2.prototype.wpMaxWidth = function() {
        if (this.wp === null)
          return this.doc.width;
        var w = this.canvasW - this.wp.originX;
        return w < 8 ? 8 : w;
      };
      Controller2.prototype.wpLayout = function() {
        var wp = this.wp;
        return layoutTdfWpStyled(wp.text, wp.fonts, this.wpMaxWidth(), wp.gap, wp.curFont);
      };
      Controller2.prototype.wpSyncFont = function() {
        var wp = this.wp;
        if (wp === null)
          return;
        var f = wp.caret > 0 ? wp.fonts[wp.caret - 1] : wp.fonts[0];
        if (f !== void 0)
          wp.curFont = f;
      };
      Controller2.prototype.wpCells = function() {
        var wp = this.wp;
        var lay = this.wpLayout();
        var out = [];
        for (var li = 0; li < lay.lines.length; li++) {
          var line = lay.lines[li];
          for (var ry = 0; ry < line.render.rows.length; ry++) {
            var row = line.render.rows[ry];
            for (var rx = 0; rx < row.length; rx++) {
              var c = row[rx];
              var isColor = c.cf === true;
              if (c.ch === 32 && !(isColor && c.color & 112))
                continue;
              out.push({ x: wp.originX + rx, y: wp.originY + line.yTop + ry, ch: c.ch, attr: isColor ? c.color & 255 : this.doc.curAttr });
            }
          }
        }
        return out;
      };
      Controller2.prototype.drawWp = function() {
        if (this.wp === null)
          return;
        var cells = this.wpCells();
        for (var i = 0; i < cells.length; i++) {
          var c = cells[i];
          var sy = this.canvasTop + (c.y - this.topRow);
          if (sy < this.canvasTop || sy > this.canvasBottom)
            continue;
          if (c.x < 0 || c.x >= this.canvasW)
            continue;
          this.scr.put(c.x, sy, c.ch, c.attr);
        }
        var wp = this.wp;
        var lay = this.wpLayout();
        var pos = tdfWpCaretXY(lay, wp.caret);
        var lineH = lay.lines[pos.line].render.height;
        var cx = wp.originX + pos.x;
        var caretAttr = makeAttr(7, 0, true, true);
        for (var cr = 0; cr < lineH; cr++) {
          var csy = this.canvasTop + (wp.originY + pos.y + cr - this.topRow);
          if (csy < this.canvasTop || csy > this.canvasBottom)
            continue;
          if (cx < 0 || cx >= this.canvasW)
            continue;
          this.scr.put(cx, csy, 221, caretAttr);
        }
        this.scr.cursorVisible = false;
      };
      Controller2.prototype.commitWp = function() {
        if (this.wp === null)
          return;
        var cells = this.wpCells();
        if (cells.length > 0)
          this.doc.paintCells(cells);
        this.wp = null;
      };
      Controller2.prototype.wpLineStart = function() {
        var wp = this.wp;
        var i = wp.text.lastIndexOf("\n", wp.caret - 1);
        return i + 1;
      };
      Controller2.prototype.wpLineEnd = function() {
        var wp = this.wp;
        var i = wp.text.indexOf("\n", wp.caret);
        return i === -1 ? wp.text.length : i;
      };
      Controller2.prototype.wpInsert = function(s) {
        var wp = this.wp;
        if (wp === null)
          return;
        wp.text = wp.text.substring(0, wp.caret) + s + wp.text.substring(wp.caret);
        for (var i = 0; i < s.length; i++)
          wp.fonts.splice(wp.caret + i, 0, wp.curFont);
        wp.caret += s.length;
      };
      Controller2.prototype.wpPickFont = function() {
        if (this.fonts === null || this.wp === null)
          return;
        var choice = fontPicker(this.scr, this.input, this.alive, this.fonts, "");
        this.scr.invalidate();
        if (choice === null)
          return;
        var font = this.fonts.load(choice.fontName);
        if (font !== null)
          this.wp.curFont = font;
      };
      Controller2.prototype.handleWpKey = function(k) {
        var wp = this.wp;
        if (k === KEY_ESC)
          return this.finishWp();
        if (k === CTRL_K) {
          this.wpPickFont();
        } else if (k === KEY_ENTER) {
          this.wpInsert("\n");
        } else if (k === KEY_TAB) {
          this.wpInsert("  ");
        } else if (k === KEY_BACKSPACE || k === KEY_DEL2) {
          if (wp.caret > 0) {
            wp.text = wp.text.substring(0, wp.caret - 1) + wp.text.substring(wp.caret);
            wp.fonts.splice(wp.caret - 1, 1);
            wp.caret--;
            this.wpSyncFont();
          }
        } else if (k === KEY_LEFT2) {
          if (wp.caret > 0)
            wp.caret--;
          this.wpSyncFont();
        } else if (k === KEY_RIGHT2) {
          if (wp.caret < wp.text.length)
            wp.caret++;
          this.wpSyncFont();
        } else if (k === KEY_HOME2) {
          wp.caret = this.wpLineStart();
          this.wpSyncFont();
        } else if (k === KEY_END2) {
          wp.caret = this.wpLineEnd();
          this.wpSyncFont();
        } else if (k === KEY_UP2) {
          this.wpMoveVert(-1);
          this.wpSyncFont();
        } else if (k === KEY_DOWN2) {
          this.wpMoveVert(1);
          this.wpSyncFont();
        } else if (isPrintable(k)) {
          this.wpInsert(k);
        }
        return null;
      };
      Controller2.prototype.wpMoveVert = function(delta) {
        var wp = this.wp;
        var lay = this.wpLayout();
        var here = tdfWpCaretXY(lay, wp.caret);
        var target = here.line + delta;
        if (target < 0 || target >= lay.lines.length)
          return;
        var curLine = lay.lines[here.line];
        var offset = wp.caret - curLine.startIdx;
        var tgt = lay.lines[target];
        wp.caret = tgt.startIdx + Math.min(offset, tgt.text.length);
      };
      Controller2.prototype.finishWp = function() {
        var choice = messageBox(this.scr, this.input, this.alive, "Font text", ["Stamp this text onto the canvas?"], [
          { id: "stamp", key: "Enter", label: "Stamp it" },
          { id: "keep", key: "K", label: "Keep editing" },
          { id: "discard", key: "D", label: "Discard" }
        ], 0);
        if (choice === "stamp")
          this.commitWp();
        else if (choice === "discard")
          this.wp = null;
        return null;
      };
      Controller2.prototype.drawPendingStamp = function() {
        if (this.pendingStamp === null)
          return;
        for (var i = 0; i < this.pendingStamp.length; i++) {
          var c = this.pendingStamp[i];
          var dx = this.brush.x + c.x;
          var dy = this.brush.y + c.y;
          var sy = this.canvasTop + (dy - this.topRow);
          if (sy < this.canvasTop || sy > this.canvasBottom)
            continue;
          if (dx < 0 || dx >= this.canvasW)
            continue;
          this.scr.put(dx, sy, c.ch, c.attr);
        }
      };
      Controller2.prototype.commitStamp = function() {
        if (this.pendingStamp === null)
          return;
        var out = [];
        for (var i = 0; i < this.pendingStamp.length; i++) {
          var c = this.pendingStamp[i];
          out.push({ x: this.brush.x + c.x, y: this.brush.y + c.y, ch: c.ch, attr: c.attr });
        }
        this.doc.paintCells(out);
        this.pendingStamp = null;
      };
      Controller2.prototype.handleStampKey = function(k) {
        var maxY = Math.max(this.doc.rowCount() + this.canvasRows(), this.brush.y + 1);
        if (k === KEY_ESC)
          this.pendingStamp = null;
        else if (k === KEY_ENTER || k === " ")
          this.commitStamp();
        else if (k === KEY_LEFT2)
          this.brush.x = clamp(this.brush.x - 1, 0, this.doc.width - 1);
        else if (k === KEY_RIGHT2)
          this.brush.x = clamp(this.brush.x + 1, 0, this.doc.width - 1);
        else if (k === KEY_UP2)
          this.brush.y = clamp(this.brush.y - 1, 0, maxY);
        else if (k === KEY_DOWN2)
          this.brush.y = clamp(this.brush.y + 1, 0, maxY);
        else if (k === KEY_HOME2)
          this.brush.x = 0;
        return null;
      };
      Controller2.prototype.handleRecolorKey = function(k) {
        var maxY = Math.max(this.doc.rowCount() + this.canvasRows(), this.brush.y + 1);
        if (k === KEY_LEFT2)
          this.brush.x = clamp(this.brush.x - 1, 0, this.doc.width - 1);
        else if (k === KEY_RIGHT2)
          this.brush.x = clamp(this.brush.x + 1, 0, this.doc.width - 1);
        else if (k === KEY_UP2)
          this.brush.y = clamp(this.brush.y - 1, 0, maxY);
        else if (k === KEY_DOWN2)
          this.brush.y = clamp(this.brush.y + 1, 0, maxY);
        else if (k === KEY_PAGEUP2)
          this.brush.y = clamp(this.brush.y - this.canvasRows(), 0, maxY);
        else if (k === KEY_PAGEDN2)
          this.brush.y = clamp(this.brush.y + this.canvasRows(), 0, maxY);
        else if (k === KEY_HOME2)
          this.brush.x = 0;
        else if (k === KEY_END2)
          this.brush.x = this.doc.width - 1;
        else if (k === "1")
          this.recolorChannel = "fg";
        else if (k === "2")
          this.recolorChannel = "bg";
        else if (k === "3")
          this.recolorChannel = "both";
        else if (k === " ")
          this.recolorAt(this.brush.x, this.brush.y);
        else if (k === KEY_DEL2)
          this.doc.eraseArt(this.brush.x, this.brush.y);
        return null;
      };
      Controller2.prototype.handleTypeKey = function(k) {
        var maxY = Math.max(this.doc.rowCount() + this.canvasRows(), this.brush.y + 1);
        if (k === KEY_LEFT2) {
          this.brush.x = clamp(this.brush.x - 1, 0, this.doc.width - 1);
          this.textOrigin = this.brush.x;
        } else if (k === KEY_RIGHT2) {
          this.brush.x = clamp(this.brush.x + 1, 0, this.doc.width - 1);
          this.textOrigin = this.brush.x;
        } else if (k === KEY_UP2)
          this.brush.y = clamp(this.brush.y - 1, 0, maxY);
        else if (k === KEY_DOWN2)
          this.brush.y = clamp(this.brush.y + 1, 0, maxY);
        else if (k === KEY_PAGEUP2)
          this.brush.y = clamp(this.brush.y - this.canvasRows(), 0, maxY);
        else if (k === KEY_PAGEDN2)
          this.brush.y = clamp(this.brush.y + this.canvasRows(), 0, maxY);
        else if (k === KEY_HOME2) {
          this.brush.x = 0;
          this.textOrigin = 0;
        } else if (k === KEY_END2) {
          this.brush.x = this.doc.width - 1;
          this.textOrigin = this.brush.x;
        } else if (k === KEY_ENTER) {
          this.brush.x = clamp(this.textOrigin, 0, this.doc.width - 1);
          this.brush.y = clamp(this.brush.y + 1, 0, maxY);
        } else if (k === KEY_BACKSPACE || k === KEY_DEL2) {
          this.brush.x = clamp(this.brush.x - 1, 0, this.doc.width - 1);
          this.doc.eraseArt(this.brush.x, this.brush.y);
        } else if (isPrintable(k)) {
          this.doc.setArt(this.brush.x, this.brush.y, { ch: k.charCodeAt(0), attr: this.doc.curAttr });
          this.brush.x = clamp(this.brush.x + 1, 0, this.doc.width - 1);
        }
        return null;
      };
      return Controller2;
    })()
  );
  function fkeySlot(k) {
    if (k.length < 2 || k.length > 3 || k.charAt(0) !== "F")
      return -1;
    var n = parseInt(k.substring(1), 10);
    return n >= 1 && n <= 10 ? n - 1 : -1;
  }
  function normRect(a, b) {
    return { x0: Math.min(a.x, b.x), y0: Math.min(a.y, b.y), x1: Math.max(a.x, b.x), y1: Math.max(a.y, b.y) };
  }
  function quoteLineObj(text) {
    var attr = [];
    for (var i = 0; i < text.length; i++)
      attr.push(makeAttr(2, 0, false));
    return { text: text, attr: attr };
  }

  // build/main.js
  load("sbbsdefs.js");
  function isAlive() {
    return Boolean(bbs.online) && !js.terminated;
  }
  function main() {
    if (typeof bbs === "undefined" || typeof console === "undefined") {
      return 1;
    }
    var caps = initTerminal();
    log(LOG_INFO, EDITOR_IDENT + " build " + (true ? "2026-07-19 14:38" : "dev") + " starting; terminal " + caps.cols + "x" + caps.rows + (caps.utf8 ? " utf8" : " cp437") + (caps.mouse ? " mouse" : ""));
    var session = loadSession();
    var exitCode = 1;
    try {
      var scr = new Screen(caps.cols, caps.rows, caps.utf8, function(s) {
        console.write(s);
      });
      console.write("\x1B[0m\x1B[2J\x1B[H");
      var controller = new Controller(session, caps, scr, readInput, isAlive, createSbbsFontProvider(), terminalSize);
      var result = controller.run();
      if (result.action === "save") {
        if (saveMessage(session, result.bodyCp437, result.subject)) {
          exitCode = 0;
        } else {
          exitCode = 1;
        }
      } else {
        if (!isAlive() && result.bodyCp437.length > 0) {
          saveMessage(session, result.bodyCp437, result.subject);
        }
        exitCode = 1;
      }
    } catch (e) {
      log(LOG_ERR, "future_edit: " + String(e));
      exitCode = 1;
    }
    restoreTerminal();
    console.write("\x1B[0m\x1B[2J\x1B[H");
    return exitCode;
  }
  var code = main();
  log(LOG_INFO, EDITOR_IDENT + " exiting with code " + code);
  exit(code);
})();
