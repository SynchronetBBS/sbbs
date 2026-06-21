// quetzal.js -- Quetzal (IFF FORM/IFZS) save codec for jszm.
// ES5 / SpiderMonkey 1.8.5 (var only; works under SM128). Pure bytes in/out;
// no engine/console deps. Loaded as a global `quetzal` BEFORE jszm.js.
// Spec: https://inform-fiction.org/zmachine/standards/quetzal/index.html
var quetzal = (function () {
  function u32(a, o) { return ((a[o] << 24) | (a[o + 1] << 16) | (a[o + 2] << 8) | a[o + 3]) >>> 0; }
  function id4(a, o) { return String.fromCharCode(a[o] & 255, a[o + 1] & 255, a[o + 2] & 255, a[o + 3] & 255); }
  function pushU32(out, n) { out.push((n >>> 24) & 255, (n >>> 16) & 255, (n >>> 8) & 255, n & 255); }
  function pushId(out, s) { out.push(s.charCodeAt(0), s.charCodeAt(1), s.charCodeAt(2), s.charCodeAt(3)); }

  // Build a FORM IFZS from [{id, data:Array}]. Returns a plain Array (writeBin-friendly).
  function buildForm(chunks) {
    var body = [], i, j;
    pushId(body, "IFZS");
    for (i = 0; i < chunks.length; i++) {
      pushId(body, chunks[i].id);
      pushU32(body, chunks[i].data.length);
      for (j = 0; j < chunks[i].data.length; j++) body.push(chunks[i].data[j] & 255);
      if (chunks[i].data.length & 1) body.push(0);            // pad chunk to even
    }
    var out = [];
    pushId(out, "FORM");
    pushU32(out, body.length);
    for (i = 0; i < body.length; i++) out.push(body[i]);
    return out;
  }

  // Parse FORM IFZS -> { chunks:[{id, off, len}] } or null.
  function parseForm(a) {
    if (a.length < 12 || id4(a, 0) !== "FORM" || id4(a, 8) !== "IFZS") return null;
    var end = 8 + u32(a, 4); if (end > a.length) end = a.length;
    var chunks = [], o = 12;
    while (o + 8 <= end) {
      var cid = id4(a, o), clen = u32(a, o + 4);
      if (o + 8 + clen > a.length) break;             // truncated/corrupt chunk -> stop
      chunks.push({ id: cid, off: o + 8, len: clen });
      o += 8 + clen + (clen & 1);
    }
    return { chunks: chunks };
  }

  function getChunk(a, id) {
    var f = parseForm(a); if (!f) return null;
    for (var i = 0; i < f.chunks.length; i++) {
      if (f.chunks[i].id === id) {
        var c = f.chunks[i], out = [];
        for (var j = 0; j < c.len; j++) out.push(a[c.off + j] & 255);
        return out;
      }
    }
    return null;
  }

  // Append a chunk; returns a new FORM Array.
  function addChunk(a, id, data) {
    var f = parseForm(a); if (!f) return a;
    var chunks = [], i, j;
    for (i = 0; i < f.chunks.length; i++) {
      var c = f.chunks[i], d = [];
      for (j = 0; j < c.len; j++) d.push(a[c.off + j] & 255);
      chunks.push({ id: c.id, data: d });
    }
    var nd = [];
    for (i = 0; i < data.length; i++) nd.push(data[i] & 255);
    chunks.push({ id: id, data: nd });
    return buildForm(chunks);
  }

  // XOR dynamic memory vs orig, RLE-encode. Returns a plain Array.
  // A changed byte is written literally; a run of unchanged (zero-XOR) bytes is
  // 0x00 followed by (run-1); trailing unchanged bytes are dropped.
  function cmemEncode(mem, orig, dynsize) {
    var i, out = [], zeros = 0, last = -1;
    for (i = 0; i < dynsize; i++) if (((mem[i] ^ orig[i]) & 255) !== 0) last = i;
    for (i = 0; i <= last; i++) {
      var x = (mem[i] ^ orig[i]) & 255;
      if (x === 0) {
        zeros++;
        if (zeros === 256) { out.push(0, 255); zeros = 0; }
      } else {
        if (zeros) { out.push(0, zeros - 1); zeros = 0; }
        out.push(x);
      }
    }
    return out;
  }

  // Decode CMem against orig into a Uint8Array of length dynsize.
  function cmemDecode(data, orig, dynsize) {
    var dyn = new Uint8Array(dynsize), i = 0, p = 0, x, run;
    while (p < data.length && i < dynsize) {
      x = data[p++] & 255;
      if (x === 0) {
        run = (data[p++] & 255) + 1;
        while (run-- > 0 && i < dynsize) { dyn[i] = orig[i] & 255; i++; }
      } else {
        dyn[i] = (x ^ orig[i]) & 255; i++;
      }
    }
    while (i < dynsize) { dyn[i] = orig[i] & 255; i++; }
    return dyn;
  }

  function wU16(out, n) { out.push((n >>> 8) & 255, n & 255); }
  function wU24(out, n) { out.push((n >>> 16) & 255, (n >>> 8) & 255, n & 255); }
  function rU16(a, o) { return ((a[o] & 255) << 8) | (a[o + 1] & 255); }
  function rU24(a, o) { return (((a[o] & 255) << 16) | ((a[o + 1] & 255) << 8) | (a[o + 2] & 255)) >>> 0; }
  function rI16(a, o) { var v = rU16(a, o); return v >= 32768 ? v - 65536 : v; }
  function wI16(out, n) { n &= 0xffff; out.push((n >>> 8) & 255, n & 255); }

  // IFhd: release(2) serial(6) checksum(2) pc(3) = 13 bytes. Identity read from orig.
  function ifhdData(orig, pc) {
    var d = [orig[2] & 255, orig[3] & 255], i;
    for (i = 0; i < 6; i++) d.push(orig[0x12 + i] & 255);
    d.push(orig[0x1C] & 255, orig[0x1D] & 255);
    wU24(d, pc);
    return d;
  }

  // Count number of set bits in the low 8 bits of m (popcount).
  function countBits(m) {
    m = m & 0xff;
    m = m - ((m >> 1) & 0x55);
    m = (m & 0x33) + ((m >> 2) & 0x33);
    return ((m + (m >> 4)) & 0x0f);
  }

  // Stks: frames outermost-first. Dummy/root frame carries main's eval stack.
  // The dummy frame's flags byte MUST be 0 (no locals, discard bit clear): real
  // interpreters (e.g. frotz) reject a save whose initial frame sets the
  // procedure/discard bit (0x10). Our own reader ignores the bit, so this is only
  // observable via a cross-interpreter (dfrotz) restore -- see test/quetzal.js.
  // For real frames: discard bit (0x10) and args-supplied byte come from the frame.
  function stksData(mem, cs, ds) {
    var out = [], k, a, b;
    var frames = [];
    var rootEvs = cs.length ? cs[cs.length - 1].ds : ds;
    // Dummy/root frame: discard bit MUST be clear (frotz-compatibility).
    frames.push({ retpc: 0, discard: false, args: 0, resvar: 0, locals: [], evs: rootEvs });
    for (k = cs.length - 1; k >= 0; k--) {
      var f = cs[k], loc = [];
      for (a = 0; a < f.local.length; a++) loc.push(f.local[a]);
      // Store frames: f.pc points AT the call's store-variable byte, so the
      // Quetzal return-pc is f.pc+1 and the result-variable byte is mem[f.pc].
      // Discard (procedure-call) frames have NO store byte: f.pc already points
      // at the next instruction, so return-pc is f.pc and the variable byte is a
      // dummy 0. (Encoding both the same way round-trips internally but exports a
      // wrong return-pc for discard frames -- caught only by a dfrotz restore.)
      frames.push({
        retpc: (f.discard ? f.pc : f.pc + 1) >>> 0, discard: !!f.discard,
        args: (f.args || 0), resvar: (f.discard ? 0 : mem[f.pc] & 255),
        locals: loc, evs: (k === 0 ? ds : cs[k - 1].ds)
      });
    }
    for (var i = 0; i < frames.length; i++) {
      var fr = frames[i];
      var argsByte = fr.args ? ((1 << fr.args) - 1) & 0xff : 0;
      wU24(out, fr.retpc);
      out.push((fr.locals.length & 0x0f) | (fr.discard ? 0x10 : 0));
      out.push(fr.resvar & 255);
      out.push(argsByte);
      wU16(out, fr.evs.length);
      for (a = 0; a < fr.locals.length; a++) wI16(out, fr.locals[a]);
      for (b = 0; b < fr.evs.length; b++) wI16(out, fr.evs[b]);
    }
    return out;
  }

  function readStks(a) {
    var frames = [], o = 0;
    while (o + 8 <= a.length) {
      var retpc = rU24(a, o); o += 3;
      var flags = a[o++] & 255, nloc = flags & 0x0f, discard = !!(flags & 0x10);
      var resvar = a[o++] & 255;
      var argsByte = a[o++] & 255;
      var nstk = rU16(a, o); o += 2;
      var locals = [], evs = [], i;
      for (i = 0; i < nloc; i++) { locals.push(rI16(a, o)); o += 2; }
      for (i = 0; i < nstk; i++) { evs.push(rI16(a, o)); o += 2; }
      frames.push({ retpc: retpc, discard: discard, args: countBits(argsByte),
                    resvar: resvar, locals: locals, evs: evs });
    }
    return frames;
  }

  // frames outermost-first (frames[0] = dummy) -> jszm { ds, cs }.
  function framesToCs(frames) {
    var n = frames.length;
    var ds = n ? frames[n - 1].evs.slice() : [];
    var cs = [], q, i;
    for (q = n - 1; q >= 1; q--) {                    // real frames only (skip dummy)
      var fr = frames[q], outerEvs = frames[q - 1].evs;
      var local = new Int16Array(fr.locals.length);
      for (i = 0; i < fr.locals.length; i++) local[i] = fr.locals[i];
      // Mirror stksData: discard frames resume AT retpc (no store byte); store
      // frames resume at retpc-1 (our engine re-reads the store byte on return).
      cs.push({ ds: outerEvs.slice(), pc: (fr.discard ? fr.retpc : (fr.retpc - 1) >>> 0) >>> 0, local: local,
                discard: fr.discard, args: fr.args });
    }
    return { ds: ds, cs: cs };
  }

  function write(s) {
    return buildForm([
      { id: "IFhd", data: ifhdData(s.orig, s.pc) },
      { id: "CMem", data: cmemEncode(s.mem, s.orig, s.dynsize) },
      { id: "Stks", data: stksData(s.mem, s.cs, s.ds) }
    ]);
  }

  function read(bytes, orig, dynsize) {
    if (!parseForm(bytes)) return null;
    var ifhd = getChunk(bytes, "IFhd"); if (!ifhd || ifhd.length < 13) return null;
    var stks = getChunk(bytes, "Stks"); if (!stks) return null;
    var cmem = getChunk(bytes, "CMem"), umem = getChunk(bytes, "UMem"), dyn, i;
    if (cmem) dyn = cmemDecode(cmem, orig, dynsize);
    else if (umem) { dyn = new Uint8Array(dynsize); for (i = 0; i < dynsize; i++) dyn[i] = umem[i] & 255; }
    else return null;
    var m = framesToCs(readStks(stks));
    return {
      release: rU16(ifhd, 0), serial: ifhd.slice(2, 8), checksum: rU16(ifhd, 8),
      pc: rU24(ifhd, 10), dyn: dyn, cs: m.cs, ds: m.ds
    };
  }

  return {
    buildForm: buildForm, parseForm: parseForm, getChunk: getChunk, addChunk: addChunk,
    cmemEncode: cmemEncode, cmemDecode: cmemDecode,
    write: write, read: read
  };
})();
