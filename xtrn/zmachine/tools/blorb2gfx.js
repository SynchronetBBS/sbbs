// blorb2gfx.js — offline pre-bake: a Blorb's pictures -> a <game>.gfx/ PPM cache + slim manifest.
// Sysop tool, run via jsexec (NOT the live door): jsexec tools/blorb2gfx.js <blorb.blb> <outdir.gfx>
// It shells out to ImageMagick `convert` freely. ES5 / SpiderMonkey 1.8.5.

function blorbBe32(b, o) { return ((b[o] << 24) | (b[o + 1] << 16) | (b[o + 2] << 8) | b[o + 3]) >>> 0; }
function blorbTag4(b, o) { return String.fromCharCode(b[o], b[o + 1], b[o + 2], b[o + 3]); }

// parseBlorb(bytes:Uint8Array) -> { release, pics:[{num,fmt,dataOffset,dataLen,w,h}], apal:[num], bpal:[{scene,adaptive,variant}] }
function parseBlorb(b) {
  if (b.length < 12 || blorbTag4(b, 0) !== "FORM" || blorbTag4(b, 8) !== "IFRS") return null;
  // Walk ALL top-level chunks from offset 12, recording RIdx + the palette/release chunks.
  var p = 12, ridxOff = -1, ridxLen = 0, end = b.length, release = 0, apal = [], bpal = [], k;
  while (p + 8 <= end) {
    var id = blorbTag4(b, p), len = blorbBe32(b, p + 4), pay = p + 8;
    if (id === "RIdx") { ridxOff = pay; ridxLen = len; }
    else if (id === "RelN" && len >= 2) { release = (b[pay] << 8) | b[pay + 1]; }
    else if (id === "APal") { for (k = 0; k + 4 <= len; k += 4) apal.push(blorbBe32(b, pay + k)); }
    else if (id === "BPal") {
      for (k = 0; k + 12 <= len; k += 12)
        bpal.push({ scene: blorbBe32(b, pay + k), adaptive: blorbBe32(b, pay + k + 4), variant: blorbBe32(b, pay + k + 8) });
    }
    p += 8 + len + (len & 1);
  }
  if (ridxOff < 0) return { release: release, pics: [], apal: apal, bpal: bpal };
  var count = blorbBe32(b, ridxOff), pics = [], i;
  for (i = 0; i < count; i++) {
    var e = ridxOff + 4 + i * 12;
    if (e + 12 > end) break;                       // truncated RIdx: stop
    if (blorbTag4(b, e) !== "Pict") continue;
    var num = blorbBe32(b, e + 4), off = blorbBe32(b, e + 8);
    if (off + 8 > end) continue;                   // rogue resource offset: skip
    var cid = blorbTag4(b, off), clen = blorbBe32(b, off + 4), fmt, w = 0, h = 0;
    if (cid === "Rect") {
      if (off + 16 > end) continue;                // truncated Rect payload: skip
      fmt = "rect"; w = blorbBe32(b, off + 8); h = blorbBe32(b, off + 12);
    } else if (cid === "JPEG") { fmt = "jpeg"; }
    else { fmt = "png"; }                           // "PNG "
    pics.push({
      num: num, fmt: fmt,
      dataOffset: (fmt === "rect") ? 0 : off + 8,
      dataLen:    (fmt === "rect") ? 0 : clen,
      w: w, h: h
    });
  }
  pics.sort(function (a, c) { return a.num - c.num; });
  return { release: release, pics: pics, apal: apal, bpal: bpal };
}

// Pad a string on the right to at least n chars (manifest columns are space-aligned for readability).
function pad(s, n) { s = "" + s; while (s.length < n) s += " "; return s; }

// buildManifestText(count, release, entries, remap) -> the slim-manifest file text.
// entries: [{num, type:"bitmap"|"rect", w, h, file:"<n>.ppm"|null, mask:"<n>.pbm"|null}], caller-sorted.
// remap (optional): [{scene, adaptive, variant}] -> "@ scene adaptive variant" lines.
function buildManifestText(count, release, entries, remap) {
  var out = "# count=" + count + "  release=" + release + "\n", i;
  for (i = 0; i < entries.length; i++) {
    var e = entries[i];
    out += e.num + "  " + pad(e.type, 6) + "  " + e.w + "  " + e.h + "  " +
           (e.file ? e.file : "-") + "  " + (e.mask ? e.mask : "-") + "\n";
  }
  if (remap) for (i = 0; i < remap.length; i++) {
    out += "@ " + remap[i].scene + " " + remap[i].adaptive + " " + remap[i].variant + "\n";
  }
  return out;
}

// --- CLI (sysop tool; not used by the live door) ----------------------------------------------

// Read first ~64 header bytes of a binary PPM and return {w,h} (P6 <w> <h> <maxval>).
function ppmDims(path) {
  var f = new File(path); if (!f.open("rb")) return null;
  var n = Math.min(128, f.length); var raw = f.read(n); f.close();
  // Strip PPM comment lines (ImageMagick may emit "# ..."), then tokenize: toks[0]="P6", [1]=w, [2]=h.
  var toks = ("" + raw).replace(/#[^\n]*/g, "").replace(/^\s+/, "").split(/\s+/);
  if (toks.length < 3) return null;
  var w = parseInt(toks[1], 10), h = parseInt(toks[2], 10);
  if (isNaN(w) || isNaN(h)) return null;
  return { w: w, h: h };
}

function readBlorbBytes(path) {
  var f = new File(path); if (!f.open("rb")) return null;
  var b = f.read(f.length); f.close();
  var u = new Uint8Array(b.length), i; for (i = 0; i < b.length; i++) u[i] = b.charCodeAt(i) & 255;
  return u;
}

function writePayload(path, bytes, off, len) {
  var f = new File(path); if (!f.open("wb")) return false;
  var i; for (i = 0; i < len; i++) f.writeBin(bytes[off + i], 1);
  f.close(); return true;
}

function main(blorbPath, outDir) {
  var bytes = readBlorbBytes(blorbPath);
  if (!bytes) { print("blorb2gfx: cannot read " + blorbPath); return 1; }
  var info = parseBlorb(bytes);
  if (!info) { print("blorb2gfx: not an IFRS Blorb: " + blorbPath); return 1; }
  if (!file_isdir(outDir) && !mkdir(outDir)) { print("blorb2gfx: cannot create " + outDir); return 1; }

  // ImageMagick command. Prefer IM7's unified "magick" -- it exists on modern Linux/macOS AND avoids the
  // Windows install gotcha where bare "convert" is the OS disk-format tool that shadows ImageMagick's
  // convert on the PATH (silently breaks the bake). Fall back to IM6 "convert"/"identify".
  var imProbe = outDir + "/.im_probe", haveMagick = system.exec('magick -version >"' + imProbe + '" 2>&1') === 0;
  file_remove(imProbe);
  var IM = haveMagick ? "magick" : "convert", IMID = haveMagick ? "magick identify" : "identify";
  print("blorb2gfx: ImageMagick command = '" + IM + "'");

  var entries = [], i, ext = { png: ".png", jpeg: ".jpg" };
  for (i = 0; i < info.pics.length; i++) {
    var p = info.pics[i];
    if (p.fmt === "rect") { entries.push({ num: p.num, type: "rect", w: p.w, h: p.h, file: null, mask: null }); continue; }
    var tmp = outDir + "/." + p.num + (ext[p.fmt] || ".png");
    var ppm = outDir + "/" + p.num + ".ppm";
    if (!writePayload(tmp, bytes, p.dataOffset, p.dataLen)) { print("blorb2gfx: temp write failed for #" + p.num); continue; }
    file_remove(ppm);
    // Two output settings, both load-bearing:
    //  * -depth 8 must come AFTER the input (an OUTPUT-depth op) to force PPM maxval 255. On ImageMagick 7,
    //    -depth BEFORE the input is only a read setting, so a 4-bit (16-colour) Blorb source writes maxval 15
    //    -- which SyncTERM renders washed-out (and looks "fine" in GIMP, which normalises maxval). IM6 wrote
    //    255 either way; placing it after the input is correct on both.
    //  * -gamma 0.55 PRE-DARKENS to cancel SyncTERM's BT.709->sRGB pixel gamma (term.c pnm_gamma), which
    //    brightens ~display=PPM^(1/2.2). 0.55 makes the on-screen colour match reference renders (PPM
    //    #202060 -> displays #2F2F73; #606090 -> #7373A0; fit from two live captures vs example3.png).
    system.exec(IM + ' "' + tmp + '" -gamma 0.55 -depth 8 "' + ppm + '"');
    var d = ppmDims(ppm);
    if (!d) { print("blorb2gfx: convert failed for #" + p.num); file_remove(tmp); continue; }
    // Transparency: if the PNG has an alpha channel, emit a 1bpp PBM mask where opaque pixels
    // are black (bit 1 = "draw" per cterm MBUF). identify's %A is "Blend"/"True" when alpha is present.
    var maskFile = null;
    var probe = outDir + "/.a" + p.num;
    file_remove(probe);
    system.exec(IMID + ' -format "%A" "' + tmp + '" > "' + probe + '" 2>/dev/null');
    var av = "", af = new File(probe);
    if (af.open("r")) { av = ("" + (af.read() || "")).replace(/\s+/g, ""); af.close(); }
    file_remove(probe);
    if (av === "Blend" || av === "True") {
      var pbm = outDir + "/" + p.num + ".pbm";
      file_remove(pbm);
      // Pad the mask width up to a multiple of 8 (extra columns white = skip, never sampled since
      // the paste only covers the image width). Works around a SyncTERM bug: the masked Paste steps
      // the mask bit-index by mask->width per row, but PBM rows are byte-padded, so a mask whose
      // width is not a multiple of 8 drifts (shears) each row. A multiple-of-8 width has no padding.
      var w8 = Math.ceil(d.w / 8) * 8;
      system.exec(IM + ' "' + tmp + '" -alpha extract -threshold 50% -negate -background white -gravity west -extent ' + w8 + 'x' + d.h + ' "' + pbm + '"');
      var pf = new File(pbm);
      if (pf.exists && pf.length > 0) maskFile = p.num + ".pbm"; else file_remove(pbm);
    }
    file_remove(tmp);
    entries.push({ num: p.num, type: "bitmap", w: d.w, h: d.h, file: p.num + ".ppm", mask: maskFile });
  }

  // Adaptive-palette remap: keep BPal entries whose adaptive picture is in APal.
  var apalSet = {}, k, remap = [];
  for (k = 0; k < info.apal.length; k++) apalSet[info.apal[k]] = true;
  for (k = 0; k < info.bpal.length; k++) if (apalSet[info.bpal[k].adaptive]) remap.push(info.bpal[k]);

  var mf = new File(outDir + "/manifest");
  if (!mf.open("w")) { print("blorb2gfx: cannot write manifest"); return 1; }
  mf.write(buildManifestText(info.pics.length, info.release, entries, remap));
  mf.close();
  print("blorb2gfx: " + entries.length + " resources, " + remap.length + " remap -> " + outDir + "/manifest");
  return 0;
}

// argv[0] = blorb path, argv[1] = output .gfx dir. Only run when invoked as a script (argv present).
if (typeof argv !== "undefined" && argv.length >= 2) {
  exit(main(argv[0], argv[1]));
}
