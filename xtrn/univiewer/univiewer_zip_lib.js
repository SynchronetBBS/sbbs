/* univiewer_zip_lib.js - ZIP extraction in JavaScript, for the old "imploded" files Synchronet's Archive support can't read
 *
 * Pure JavaScript, no external programs.  Synchronet's own Archive support extracts ZIPs well, except those made by
 * very old programs (PKZIP 1.x, early 1990s) with the Implode method, which is common in DOS-era files.  This reads
 * the ZIP itself and handles the methods Stored (0), Imploded (6) and Deflated (8, through inflate.js).
 *
 *   entries(path)               [{ name, method, flags, csize, usize, crc, offset }] from the ZIP's central directory
 *   extract(path, entry, out)   write the file to out; throws a message if it can't (unknown method, bad data, bad CRC)
 */
"use strict";
(function () {
	var MAX_FILE = 256 * 1048576;		/* a file is unpacked in memory, so there is a limit */

	function u16(s, o) { return s.charCodeAt(o) | (s.charCodeAt(o + 1) << 8); }
	function u32(s, o) { return (s.charCodeAt(o) | (s.charCodeAt(o + 1) << 8) | (s.charCodeAt(o + 2) << 16) | (s.charCodeAt(o + 3) << 24)) >>> 0; }

	function read_at(f, pos, len)
	{
		f.position = pos;
		var s = f.read(len);
		return s === undefined || s === null ? "" : s;
	}

	function entries(path)
	{
		var f = new File(path);
		if (!f.open("rb"))
			throw "Unable to open " + file_getname(path);
		try {
			var size = f.length;
			var tail = read_at(f, Math.max(0, size - 65557), Math.min(size, 65557));
			var e = tail.lastIndexOf("PK\x05\x06");
			if (e < 0)
				throw "Not a ZIP file";
			var count = u16(tail, e + 10), dirsize = u32(tail, e + 12), diroff = u32(tail, e + 16);
			var dir = read_at(f, diroff, dirsize);
			var out = [];
			var p = 0;
			for (var i = 0; i < count && p + 46 <= dir.length; i++) {
				if (dir.substr(p, 4) != "PK\x01\x02")
					break;
				var nlen = u16(dir, p + 28), elen = u16(dir, p + 30), clen = u16(dir, p + 32);
				out.push({ name: dir.substr(p + 46, nlen), flags: u16(dir, p + 8), method: u16(dir, p + 10), crc: u32(dir, p + 16),
					csize: u32(dir, p + 20), usize: u32(dir, p + 24), offset: u32(dir, p + 42) });
				p += 46 + nlen + elen + clen;
			}
			return out;
		}
		finally {
			f.close();
		}
	}

	/* PKWARE Implode (ZIP method 6).  Shannon-Fano codes are read from the data: a literal tree (only if flag bit 2 is
	 * set), then a length tree and a distance tree.  Bits come least significant first, and the codes' bits are inverted. */
	function explode(src, flags, outsize)
	{
		var out = new Uint8Array(outsize);
		var op = 0;
		var pos = 0, bitbuf = 0, bitcnt = 0;
		var big = (flags & 2) != 0;		/* an 8 KB dictionary: 7 low bits of a distance, else 6 */
		var three = (flags & 4) != 0;		/* three trees: the literals are coded too */

		function bits(n)
		{
			while (bitcnt < n) {
				if (pos >= src.length)
					throw "The data is cut short";
				bitbuf |= (src.charCodeAt(pos++) & 255) << bitcnt;
				bitcnt += 8;
			}
			var v = bitbuf & ((1 << n) - 1);
			bitbuf >>>= n;
			bitcnt -= n;
			return v;
		}

		/* A tree is stored as a count of bytes, then bytes holding (a code length - 1) in the low nibble and (how many
		 * symbols have that length - 1) in the high one.  Codes of a length go to symbols in order. */
		function tree(n)
		{
			var nb = bits(8) + 1, lengths = [], i, j;
			for (i = 0; i < nb; i++) {
				var b = bits(8);
				for (j = 0; j < (b >> 4) + 1; j++)
					lengths.push((b & 15) + 1);
			}
			if (lengths.length != n)
				throw "Damaged implode tree";
			var count = [], offs = [], symbol = [];
			for (i = 0; i <= 16; i++)
				count[i] = 0;
			for (i = 0; i < n; i++)
				count[lengths[i]]++;
			offs[1] = 0;
			for (i = 1; i < 16; i++)
				offs[i + 1] = offs[i] + count[i];
			for (i = 0; i < n; i++)
				symbol[offs[lengths[i]]++] = i;
			return { count: count, symbol: symbol };
		}

		function decode(t)
		{
			var code = 0, first = 0, index = 0;
			for (var len = 1; len <= 16; len++) {
				code |= bits(1) ^ 1;
				var c = t.count[len];
				if (code < first + c) {
					var sym = t.symbol[index + (code - first)];
					if (sym === undefined)
						throw "Damaged implode data";
					return sym;
				}
				index += c;
				first += c;
				first <<= 1;
				code <<= 1;
			}
			throw "Damaged implode data";
		}

		var lit = three ? tree(256) : null;
		var lengths = tree(64);
		var dists = tree(64);
		var low = big ? 7 : 6;
		while (op < outsize) {
			if (bits(1)) {
				out[op++] = three ? decode(lit) : bits(8);
				continue;
			}
			var d = bits(low);
			d = ((decode(dists) << low) | d) + 1;
			var len = decode(lengths);
			if (len == 63)
				len += bits(8);
			len += three ? 3 : 2;
			if (d > op)
				throw "Damaged implode data";
			while (len-- > 0 && op < outsize) {
				out[op] = out[op - d];
				op++;
			}
		}
		return out;
	}

	function bytes_to_binary(b, from, to, sink)
	{
		for (var i = from; i < to; i += 8192)
			sink(String.fromCharCode.apply(null, b.subarray(i, Math.min(to, i + 8192))));
	}

	function extract(path, ent, out)
	{
		var f = new File(path);
		if (!f.open("rb"))
			throw "Unable to open " + file_getname(path);
		var data;
		try {
			var lh = read_at(f, ent.offset, 30);
			if (lh.substr(0, 4) != "PK\x03\x04")
				throw "Damaged ZIP";
			var start = ent.offset + 30 + u16(lh, 26) + u16(lh, 28);
			if (ent.csize > MAX_FILE || start + ent.csize > f.length)
				throw "Damaged ZIP";
			data = read_at(f, start, ent.csize);
		}
		finally {
			f.close();
		}
		if (ent.flags & 1)
			throw "The file is encrypted";
		if (ent.usize > MAX_FILE)
			throw "The file is too large for the JavaScript reader";
		var raw;		/* the file as a binary string, or bytes */
		var bytes = null;
		if (ent.method == 0)
			raw = data;
		else if (ent.method == 6)
			bytes = explode(data, ent.flags, ent.usize);
		else if (ent.method == 8 && typeof inflate_raw == "function" && typeof string_to_bytes == "function")
			bytes = inflate_raw(string_to_bytes(data), 0, data.length);
		else
			throw "Unsupported ZIP method " + ent.method;
		var o = new File(out);
		if (!o.open("wb"))
			throw "Unable to create " + file_getname(out);
		var chunks = [];
		try {
			if (bytes === null)
				chunks.push(raw);
			else
				bytes_to_binary(bytes, 0, bytes.length, function (s) { chunks.push(s); });
			for (var i = 0; i < chunks.length; i++)
				o.write(chunks[i]);
		}
		finally {
			o.close();
		}
		/* check what was written against the ZIP's own record of the file */
		if (typeof crc32 == "function") {
			var chk = new File(out);
			if (chk.open("rb")) {
				var all = chk.read();
				chk.close();
				if (file_size(out) != ent.usize || (ent.usize > 0 && (crc32(all) >>> 0) != ent.crc)) {
					file_remove(out);
					throw "The file did not unpack correctly";
				}
			}
		}
	}

	return { entries: entries, extract: extract };
})();
