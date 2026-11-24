/* inflate.js
 *
 * Pure JavaScript inflate (RFC 1950 zlib / RFC 1951 deflate decompression), for
 * Synchronet scripts that need to decompress data (PDF, PNG, gzip, ...).
 * SpiderMonkey 1.8.5 compatible; uses typed arrays.  Tab size is 4.
 *
 *	load("inflate.js");
 *	var out = inflate_zlib(bytes);			// zlib stream (2 byte header + deflate)
 *	var out = inflate_raw(bytes, start, end);	// raw deflate data in bytes[start..end)
 *
 * "bytes" is a Uint8Array (see string_to_bytes() below for converting a binary
 * string, e.g. from File.read()); the result is a Uint8Array.  Truncated or
 * corrupt input returns whatever could be decoded rather than throwing.
 *
 * Larger zlib streams (2 KB or more) are decompressed natively through Synchronet's Archive
 * class (libarchive) when it is available, by wrapping the data in a temporary ZIP file under
 * system.temp_dir; this is far faster than the JavaScript decoder.  Any failure falls back
 * to the JavaScript implementation, which can also be called directly as inflate_raw().
 */

"use strict";

/* Convert a binary string (as returned by File.read) to a Uint8Array */
function string_to_bytes(s)
{
	var n = s.length;
	var b = new Uint8Array(n);
	for (var i = 0; i < n; i++)
		b[i] = s.charCodeAt(i) & 0xff;
	return b;
}

/* ----- Flate (zlib/deflate) ----- */

var INF_LBASE = [3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258];
var INF_LEXT = [0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0];
var INF_DBASE = [1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577];
var INF_DEXT = [0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13];
var INF_CLORDER = [16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15];
var INF_FIXED = null;

function inflate_table(lengths, n)
{
	var count = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
	var i;
	for (i = 0; i < n; i++)
		count[lengths[i]]++;
	count[0] = 0;
	var maxlen = 1;
	for (i = 15; i > 0; i--) {
		if (count[i]) {
			maxlen = i;
			break;
		}
	}
	var size = 1 << maxlen;
	var table = new Int32Array(size);
	var next = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
	var code = 0;
	for (i = 1; i <= 15; i++) {
		code = (code + count[i - 1]) << 1;
		next[i] = code;
	}
	for (i = 0; i < n; i++) {
		var len = lengths[i];
		if (!len)
			continue;
		var c = next[len]++;
		var r = 0;
		for (var b = 0; b < len; b++) {
			r = (r << 1) | (c & 1);
			c >>= 1;
		}
		var entry = (i << 4) | len;
		for (var j = r; j < size; j += (1 << len))
			table[j] = entry;
	}
	return { t: table, bits: maxlen };
}

/* Inflate raw deflate data in data[pos..end). Returns whatever could be
 * decoded (truncated/corrupt data yields a partial result, not an error). */
function inflate_raw(data, pos, end)
{
	var out = new Uint8Array(Math.min(Math.max((end - pos) * 4, 4096), 1 << 25));
	var op = 0;
	var bb = 0, bc = 0, eof = 0;
	var lit, dst, tb, e, l, sym, final_block, type, i;

	function grow(need)
	{
		var nl = out.length * 2;
		while (nl < op + need)
			nl *= 2;
		var n = new Uint8Array(nl);
		n.set(out.subarray(0, op));
		out = n;
	}
	function bits(n)
	{
		while (bc < n) {
			if (pos < end)
				bb |= data[pos++] << bc;
			else if (++eof > 8)
				throw "inflate: eof";
			bc += 8;
		}
		var v = bb & ((1 << n) - 1);
		bb >>>= n;
		bc -= n;
		return v;
	}
	function decode(t)
	{
		while (bc < t.bits) {
			if (pos < end)
				bb |= data[pos++] << bc;
			else if (++eof > 8)
				throw "inflate: eof";
			bc += 8;
		}
		var ent = t.t[bb & ((1 << t.bits) - 1)];
		var ln = ent & 15;
		if (!ln)
			throw "inflate: bad code";
		bb >>>= ln;
		bc -= ln;
		return ent >> 4;
	}

	try {
		do {
			final_block = bits(1);
			type = bits(2);
			if (type == 0) {
				pos -= (bc >> 3);
				bb = 0;
				bc = 0;
				var len = data[pos] | (data[pos + 1] << 8);
				pos += 4;
				if (pos + len > end)
					len = end - pos;
				if (op + len > out.length)
					grow(len);
				for (i = 0; i < len; i++)
					out[op++] = data[pos++];
				continue;
			}
			if (type == 1) {
				if (!INF_FIXED) {
					var fl = [];
					for (i = 0; i < 288; i++)
						fl[i] = i < 144 ? 8 : i < 256 ? 9 : i < 280 ? 7 : 8;
					var fd = [];
					for (i = 0; i < 30; i++)
						fd[i] = 5;
					INF_FIXED = { l: inflate_table(fl, 288), d: inflate_table(fd, 30) };
				}
				lit = INF_FIXED.l;
				dst = INF_FIXED.d;
			}
			else if (type == 2) {
				var hlit = bits(5) + 257;
				var hdist = bits(5) + 1;
				var hclen = bits(4) + 4;
				var cl = [];
				for (i = 0; i < 19; i++)
					cl[i] = 0;
				for (i = 0; i < hclen; i++)
					cl[INF_CLORDER[i]] = bits(3);
				var clt = inflate_table(cl, 19);
				var lens = [];
				var total = hlit + hdist;
				while (lens.length < total) {
					sym = decode(clt);
					if (sym < 16)
						lens.push(sym);
					else {
						var rep, val = 0;
						if (sym == 16) {
							val = lens.length ? lens[lens.length - 1] : 0;
							rep = 3 + bits(2);
						}
						else if (sym == 17)
							rep = 3 + bits(3);
						else
							rep = 11 + bits(7);
						while (rep-- > 0)
							lens.push(val);
					}
				}
				lit = inflate_table(lens.slice(0, hlit), hlit);
				dst = inflate_table(lens.slice(hlit, total), hdist);
			}
			else
				throw "inflate: bad block type";

			/* hot loop: inlined symbol decode for literals/lengths */
			var lt = lit.t, lbits = lit.bits, lmask = (1 << lit.bits) - 1;
			for (;;) {
				while (bc <= 23) {
					if (pos < end)
						bb |= data[pos++] << bc;
					else if (++eof > 8)
						throw "inflate: eof";
					bc += 8;
				}
				if (op + 2 >= out.length)
					grow(2);
				e = lt[bb & lmask];
				l = e & 15;
				if (!l)
					throw "inflate: bad literal code";
				bb >>>= l;
				bc -= l;
				if (e < 4096) {
					out[op++] = e >> 4;
					/* a second literal can often be decoded without refilling */
					if (bc >= lbits) {
						e = lt[bb & lmask];
						if (e && e < 4096) {
							l = e & 15;
							bb >>>= l;
							bc -= l;
							out[op++] = e >> 4;
						}
					}
					continue;
				}
				sym = e >> 4;
				if (sym < 256) {
					out[op++] = sym;
				}
				else if (sym == 256)
					break;
				else {
					sym -= 257;
					if (sym >= 29)
						throw "inflate: bad length";
					var length = INF_LBASE[sym] + (INF_LEXT[sym] ? bits(INF_LEXT[sym]) : 0);
					var ds = decode(dst);
					if (ds >= 30)
						throw "inflate: bad dist";
					var dist = INF_DBASE[ds] + (INF_DEXT[ds] ? bits(INF_DEXT[ds]) : 0);
					if (dist > op)
						throw "inflate: dist too far";
					if (op + length > out.length)
						grow(length);
					var from = op - dist;
					for (i = 0; i < length; i++)
						out[op++] = out[from++];
				}
			}
		} while (!final_block);
	}
	catch (err) {
		/* keep what we have */
	}
	return out.subarray(0, op);
}

/* ----- optional native acceleration via Synchronet's Archive class ----- */

var INFLATE_NATIVE_MIN = 16384;		/* don't bother for smaller inputs */
var inflate_native_ok = (typeof Archive == "function" && typeof system != "undefined" && typeof mkdir == "function");

function inflate_le(v, n)
{
	var s = "";
	for (var i = 0; i < n; i++)
		s += String.fromCharCode((v >>> (8 * i)) & 255);
	return s;
}

/* Decompress raw deflate data in data[start..end) by wrapping it in a ZIP file and letting
 * libarchive extract it.  The ZIP header needs the uncompressed size and CRC-32, which are
 * learned from the (deliberately provoked) error messages of the first two attempts.
 * Returns a Uint8Array, or null if anything goes wrong (callers fall back to inflate_raw). */
function inflate_native(data, start, end)
{
	var dir = backslash(system.temp_dir) + "inflate-" + Date.now() + "-" + Math.floor(Math.random() * 1000000);
	var zipname = dir + ".zip";
	var result = null;
	var parts = [], i;
	for (i = start; i < end; i += 4096)
		parts.push(String.fromCharCode.apply(null, Array.prototype.slice.call(data, i, Math.min(end, i + 4096))));
	var body = parts.join("");
	function build(usize, crc)
	{
		var name = "x.bin";
		var lh = "PK\x03\x04" + inflate_le(20, 2) + inflate_le(0, 2) + inflate_le(8, 2) + inflate_le(0, 2) + inflate_le(0x21, 2)
			+ inflate_le(crc, 4) + inflate_le(body.length, 4) + inflate_le(usize, 4) + inflate_le(name.length, 2) + inflate_le(0, 2) + name;
		var cd = "PK\x01\x02" + inflate_le(20, 2) + inflate_le(20, 2) + inflate_le(0, 2) + inflate_le(8, 2) + inflate_le(0, 2) + inflate_le(0x21, 2)
			+ inflate_le(crc, 4) + inflate_le(body.length, 4) + inflate_le(usize, 4) + inflate_le(name.length, 2) + inflate_le(0, 2) + inflate_le(0, 2)
			+ inflate_le(0, 2) + inflate_le(0, 2) + inflate_le(0, 4) + inflate_le(0, 4) + name;
		var eo = "PK\x05\x06" + inflate_le(0, 2) + inflate_le(0, 2) + inflate_le(1, 2) + inflate_le(1, 2) + inflate_le(cd.length, 4)
			+ inflate_le(lh.length + body.length, 4) + inflate_le(0, 2);
		var f = new File(zipname);
		if (!f.open("wb"))
			throw "can't create temp file";
		f.write(lh);
		f.write(body);
		f.write(cd);
		f.write(eo);
		f.close();
	}
	try {
		if (!mkdir(dir))
			return null;
		var usize = 0, crc = 0, got = false, m;
		for (var attempt = 0, tries = 0; attempt < 3 && tries < 6 && !got; tries++) {
			build(usize, crc);
			try {
				new Archive(zipname).extract(dir);
				got = true;
			}
			catch (e) {
				var msg = String(e);
				if ((m = /ZIP compressed data is wrong size \(read (\d+),/.exec(msg)) !== null)
					body = body.substr(0, parseInt(m[1], 10));	/* trailing bytes, e.g. the zlib Adler-32 */
				else if ((m = /ZIP uncompressed data is wrong size \(read (\d+),/.exec(msg)) !== null && attempt == 0) {
					usize = parseInt(m[1], 10);
					attempt++;
				}
				else if ((m = /ZIP bad CRC: 0x([0-9a-fA-F]+) should be/.exec(msg)) !== null && attempt == 1) {
					crc = parseInt(m[1], 16);
					attempt++;
				}
				else
					return null;
			}
		}
		if (got) {
			var of = new File(dir + "/x.bin");
			if (of.open("rb")) {
				var s = of.read();
				of.close();
				if (s !== undefined && s.length == usize) {
					result = new Uint8Array(s.length);
					for (i = 0; i < s.length; i++)
						result[i] = s.charCodeAt(i) & 255;
				}
			}
		}
	}
	catch (e2) {
		result = null;
	}
	finally {
		file_remove(dir + "/x.bin");
		rmdir(dir);
		file_remove(zipname);
	}
	return result;
}

function inflate_zlib(data)
{
	if (data.length < 2)
		return new Uint8Array(0);
	var start = 2;
	if (data[1] & 0x20)
		start += 4;
	/* Some producers emit raw deflate; sanity check the zlib header */
	if ((data[0] & 0x0f) != 8 || ((data[0] << 8) | data[1]) % 31 != 0)
		start = 0;
	if (inflate_native_ok && data.length - start >= INFLATE_NATIVE_MIN) {
		var r = null;
		try {
			r = inflate_native(data, start, data.length);
		}
		catch (e) {
			r = null;
		}
		if (r)
			return r;
		/* a failed attempt may mean corrupt data or a missing feature; use the JavaScript decoder */
	}
	return inflate_raw(data, start, data.length);
}

