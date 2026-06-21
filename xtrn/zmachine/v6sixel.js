// v6sixel.js — pure-ES5 PPM/PBM -> Sixel encoder + Sixel surface for the v6 picture bridge.
// No external deps; the door load()s this when the chosen render tier is Sixel.

// Accepts a Uint8Array OR a plain Array of byte values.
function JSZM_sixelHeaderNums(bytes, magic2, count, start) {
  // skip ASCII whitespace and #-comments, read `count` decimal integers; returns {nums, pos} or null.
  var i = start, n = bytes.length, nums = [], c, s;
  while (nums.length < count) {
    // skip ws / comments
    while (i < n) {
      c = bytes[i];
      if (c === 35) { while (i < n && bytes[i] !== 10) i++; }            // '#' comment to EOL
      else if (c === 32 || c === 9 || c === 10 || c === 13) i++;        // space/tab/LF/CR
      else break;
    }
    s = "";
    while (i < n && bytes[i] >= 48 && bytes[i] <= 57) { s += String.fromCharCode(bytes[i]); i++; }
    if (s === "") return null;
    nums.push(parseInt(s, 10));
  }
  return { nums: nums, pos: i + 1 };   // +1 consumes the single whitespace before the raster
}

function JSZM_sixelParsePPM(bytes) {
  if (bytes[0] !== 80 || bytes[1] !== 54) return null;                  // "P6"
  var h = JSZM_sixelHeaderNums(bytes, 54, 3, 2);
  if (!h) return null;
  var w = h.nums[0], ht = h.nums[1];
  return { w: w, h: ht, rgb: bytes.subarray ? bytes.subarray(h.pos) : bytes.slice(h.pos) };
}

function JSZM_sixelParsePBM(bytes) {
  if (bytes[0] !== 80 || bytes[1] !== 52) return null;                  // "P4"
  var h = JSZM_sixelHeaderNums(bytes, 52, 2, 2);
  if (!h) return null;
  var w = h.nums[0], ht = h.nums[1], stride = (w + 7) >> 3;
  return { w: w, h: ht, stride: stride, bits: bytes.subarray ? bytes.subarray(h.pos) : bytes.slice(h.pos) };
}

// PBM bit 1 (black) = opaque/draw (blorb2gfx convention). No mask -> all opaque.
function JSZM_sixelMaskOpaque(pbm, x, y) {
  if (!pbm) return true;
  var byte = pbm.bits[y * pbm.stride + (x >> 3)];
  return (byte & (0x80 >> (x & 7))) !== 0;
}

// Encode the sub-rect [sx,sy,sw,sh] of ppm (+ optional pbm mask) to a Sixel DCS string.
// P2=1 -> unset pixels stay transparent. topPad transparent pixel rows are prepended (vertical
// sub-cell offset). Returns null if the sub-rect needs > 256 distinct colours. ppm.rgb may be a
// Uint8Array or a plain Array. Colour channels are scaled 0-255 -> 0-100 (Sixel range).
function JSZM_sixelEncode(ppm, pbm, sx, sy, sw, sh, topPad, leftPad) {
  topPad = topPad || 0; leftPad = leftPad || 0;
  var totalH = topPad + sh, totalW = leftPad + sw, rgb = ppm.rgb, pw = ppm.w;
  var palIndex = {}, palOrder = [], nreg = 0, x, y, o, key;
  // Build palette over opaque pixels of the sub-rect.
  for (y = 0; y < sh; y++) {
    o = (sy + y) * pw * 3;
    for (x = 0; x < sw; x++) {
      if (!JSZM_sixelMaskOpaque(pbm, sx + x, sy + y)) continue;
      var oo = o + (sx + x) * 3;
      key = (rgb[oo] << 16) | (rgb[oo + 1] << 8) | rgb[oo + 2];
      if (palIndex[key] === undefined) {
        if (nreg >= 256) return null;
        palIndex[key] = nreg; palOrder.push(key); nreg++;
      }
    }
  }
  var out = ["\x1bP0;1;0q\"1;1;" + totalW + ";" + totalH], i, r, g, b;
  for (i = 0; i < nreg; i++) {
    key = palOrder[i]; r = (key >> 16) & 255; g = (key >> 8) & 255; b = key & 255;
    out.push("#" + i + ";2;" + Math.round(r * 100 / 255) + ";" + Math.round(g * 100 / 255) + ";" + Math.round(b * 100 / 255));
  }
  // Emit bands of 6 OUTPUT rows. cols[reg] is a per-band column-bitmap array.
  var band, rowsInBand, row, outRow, imgRow, col, reg, cols, line, cnt, ch, sc, z;
  for (band = 0; band * 6 < totalH; band++) {
    var bandTop = band * 6;
    rowsInBand = (totalH - bandTop < 6) ? (totalH - bandTop) : 6;
    cols = {};
    for (row = 0; row < rowsInBand; row++) {
      outRow = bandTop + row;
      if (outRow < topPad) continue;                 // transparent pad row
      imgRow = outRow - topPad;
      o = (sy + imgRow) * pw * 3;
      for (col = 0; col < sw; col++) {
        if (!JSZM_sixelMaskOpaque(pbm, sx + col, sy + imgRow)) continue;
        var op = o + (sx + col) * 3;
        reg = palIndex[(rgb[op] << 16) | (rgb[op + 1] << 8) | rgb[op + 2]];
        if (!cols[reg]) cols[reg] = [];
        cols[reg][leftPad + col] = (cols[reg][leftPad + col] | 0) | (1 << row);   // shift content right by leftPad transparent columns
      }
    }
    // Emit each present register in palette order; "$" = overlay (CR), "-" = next band (LF).
    for (i = 0; i < nreg; i++) {
      if (!cols[i]) continue;
      var arr = cols[i];
      line = "#" + i; col = 0;
      while (col < totalW) {
        ch = arr[col] | 0; cnt = 1;
        while (col + cnt < totalW && (arr[col + cnt] | 0) === ch) cnt++;
        sc = String.fromCharCode(0x3F + ch);
        if (cnt >= 3) line += "!" + cnt + sc; else for (z = 0; z < cnt; z++) line += sc;
        col += cnt;
      }
      out.push(line + "$");
    }
    out.push("-");
  }
  out.push("\x1b\\");
  return out.join("");
}

// A solid (r,g,b) rectangle w x h, topPad transparent rows. Avoids building a full raster.
function JSZM_sixelEncodeSolid(r, g, b, w, h, topPad, leftPad) {
  topPad = topPad || 0; leftPad = leftPad || 0;
  var totalH = topPad + h, totalW = leftPad + w, out = ["\x1bP0;1;0q\"1;1;" + totalW + ";" + totalH];
  out.push("#0;2;" + Math.round(r * 100 / 255) + ";" + Math.round(g * 100 / 255) + ";" + Math.round(b * 100 / 255));
  var band, rowsInBand, bits, sc, row, z, s, pad;
  for (band = 0; band * 6 < totalH; band++) {
    var bandTop = band * 6;
    rowsInBand = (totalH - bandTop < 6) ? (totalH - bandTop) : 6;
    bits = 0;
    for (row = 0; row < rowsInBand; row++) if (bandTop + row >= topPad) bits |= (1 << row);
    if (bits) {
      sc = String.fromCharCode(0x3F + bits);
      pad = (leftPad >= 3) ? ("!" + leftPad + "?") : (function () { var p = "", k; for (k = 0; k < leftPad; k++) p += "?"; return p; })();
      if (w >= 3) { out.push("#0" + pad + "!" + w + sc + "$"); }
      else { s = ""; for (z = 0; z < w; z++) s += sc; out.push("#0" + pad + s + "$"); }
    }
    out.push("-");
  }
  out.push("\x1b\\");
  return out.join("");
}

// Solid-black opaque Sixel of w x h with topPad/leftPad transparent leading rows/columns.
function JSZM_sixelBlackBand(w, h, topPad, leftPad) { return JSZM_sixelEncodeSolid(0, 0, 0, w, h, topPad, leftPad); }

// Nearest-neighbour resample of the [sx,sy,sw,sh] sub-rect of ppm (+ optional pbm) to devW x devH
// device pixels -> { ppm:{w,h,rgb}, pbm:{w,h,stride,bits}|null }. Lets the surface scale each picture
// to the TERMINAL's real cell size when it differs from the door's logical cell.
function JSZM_sixelResample(ppm, pbm, sx, sy, sw, sh, devW, devH) {
  var rgb = [], ox, oy, srcx, srcy, o, so, out = { ppm: { w: devW, h: devH, rgb: rgb }, pbm: null };
  for (oy = 0; oy < devH; oy++) {
    srcy = sy + Math.floor(oy * sh / devH);
    o = srcy * ppm.w * 3;
    for (ox = 0; ox < devW; ox++) {
      srcx = sx + Math.floor(ox * sw / devW); so = o + srcx * 3;
      rgb.push(ppm.rgb[so], ppm.rgb[so + 1], ppm.rgb[so + 2]);
    }
  }
  if (pbm) {
    var stride = (devW + 7) >> 3, bits = [], total = stride * devH, k;
    for (k = 0; k < total; k++) bits.push(0);
    for (oy = 0; oy < devH; oy++) {
      srcy = sy + Math.floor(oy * sh / devH);
      for (ox = 0; ox < devW; ox++) {
        srcx = sx + Math.floor(ox * sw / devW);
        if (JSZM_sixelMaskOpaque(pbm, srcx, srcy)) bits[oy * stride + (ox >> 3)] |= (0x80 >> (ox & 7));
      }
    }
    out.pbm = { w: devW, h: devH, stride: stride, bits: bits };
  }
  return out;
}

// Sixel surface. opts:
//   write(str), cellW, cellH,             // the door's LOGICAL cell (positions images/text)
//   devCellW, devCellH,                   // the TERMINAL's real cell px (CSI 16t); images scale to this
//   loadPPM(file) -> {w,h,rgb} | null   (reads + parses the SCALED ppm)
//   loadPBM(mask) -> {w,h,stride,bits} | null
function JSZM_makeSixelSurface(opts) {
  var cw = opts.cellW || 8, chh = opts.cellH || 16;
  var dcw = opts.devCellW || cw, dch = opts.devCellH || chh;   // terminal cell px (CSI 16t)
  // Map a door rect to GLOBAL device coordinates: round(door * scale) so a door pixel lands at the SAME
  // device pixel regardless of which blit draws it -> adjacent images and their black-fill clears tile
  // exactly (no sub-pixel seams). Position the cursor at the enclosing terminal cell; recover the sub-cell
  // offset with transparent leading rows/columns (topPad/leftPad). Returns {col,row,leftPad,topPad,w,h}.
  function place(dstX, dstY, srcW, srcH) {
    var devL = Math.round(dstX * dcw / cw), devR = Math.round((dstX + srcW) * dcw / cw);
    var devT = Math.round(dstY * dch / chh), devB = Math.round((dstY + srcH) * dch / chh);
    var col = Math.floor(devL / dcw) + 1, row = Math.floor(devT / dch) + 1;
    var w = devR - devL, h = devB - devT; if (w < 1) w = 1; if (h < 1) h = 1;
    return { col: col, row: row, leftPad: devL - (col - 1) * dcw, topPad: devT - (row - 1) * dch, w: w, h: h };
  }
  return {
    kind: "sixel",
    blit: function (b) {
      var ppm = opts.loadPPM(b.file); if (!ppm) return false;
      var pbm = (b.useMask && b.mask && opts.loadPBM) ? opts.loadPBM(b.mask) : null;
      var p = place(b.dstX, b.dstY, b.srcW, b.srcH);
      var rs = JSZM_sixelResample(ppm, pbm, b.srcX, b.srcY, b.srcW, b.srcH, p.w, p.h);   // -> exact device size
      var six = JSZM_sixelEncode(rs.ppm, rs.pbm, 0, 0, p.w, p.h, p.topPad, p.leftPad);
      if (six === null) { return false; }   // >256 colours -> let caller degrade (no cursor moved yet)
      opts.write("\x1b7\x1b[" + p.row + ";" + p.col + "H");   // save cursor + position
      opts.write(six);
      opts.write("\x1b8");                                    // restore cursor
      return true;
    },
    fillBlack: function (r) {
      var p = place(r.dstX, r.dstY, r.w, r.h);
      opts.write("\x1b7\x1b[" + p.row + ";" + p.col + "H");
      opts.write(JSZM_sixelBlackBand(p.w, p.h, p.topPad, p.leftPad));
      opts.write("\x1b8");
    },
    canFillBlack: function () { return true; },
    setForceUpload: function () {}, forgetUploads: function () {}
  };
}
