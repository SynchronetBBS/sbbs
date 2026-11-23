/* pdf2sixel_lib.js
 *
 * PDF rendering library used by pdf2sixel.js (the viewer).  Pure Synchronet
 * JavaScript (SpiderMonkey 1.8.5 compatible): no external programs or libraries
 * and no terminal I/O.
 *
 * Quick conversion:
 *
 *	load("pdf2sixel_lib.js");
 *	var sixel = pdf_to_sixel("/full/path/file.pdf", 1, 640, 384);
 *	console.write(sixel);			// 1-based page, window size in pixels
 *
 * Interactive use (what pdf2sixel.js does):
 *
 *	var doc = load_pdf(path);		// throws a string on failure
 *	var view = new View(doc, w, h);	// a w x h pixel window onto one page
 *	view.fit_width();			// or fit_page(), zoom(factor)
 *	view.sx += 50; view.sy += 50;	// scroll (device pixels), then:
 *	view.clamp();
 *	view.render(abort_cb, 8);		// draws into FB (FW x FH pixels, 0xRRGGBB)
 *	var sixel = sixel_encode(FB, FW, FH);
 *
 * Exported functions/objects:
 *	pdf_to_sixel(file, page, w, h)	load and render one page, returns Sixel data
 *	load_pdf(file)			returns a PDFDoc (doc.get_pages().length = page count)
 *	View				page window: page, scale, sx, sy, fit_width(),
 *					fit_page(), zoom(), set_page(), clamp(), render()
 *	sixel_encode(fb, w, h)		0xRRGGBB framebuffer -> Sixel string (h multiple of 6)
 *	inflate_zlib(bytes)		zlib/deflate decompression (loaded from inflate.js)
 *
 * Requires inflate.js (Synchronet has no built-in zlib support in JS).
 *
 * Contains:
 *	- a PDF parser: xref tables and streams, object streams, recovery of damaged
 *	  files, standard-security-handler decryption (empty password; RC4 and AES-128)
 *	- stream filters: Flate (inflate.js), LZW, ASCII85, ASCIIHex, RunLength,
 *	  CCITT G3/G4 and baseline DCT/JPEG
 *	- a content-stream interpreter: paths, clipping, strokes, colour spaces,
 *	  functions, shadings, images and masks, forms, annotations, Type 3 fonts
 *	- TrueType/OpenType, CFF and Type 1 font outlines; non-embedded fonts use the
 *	  outlines in pdf2sixel_fonts.js, falling back to the bitmap fonts in ctrl/fonts
 *	- an anti-aliased scan converter and the Sixel encoder
 * See pdf2sixel.js for the list of unsupported features.
 */

"use strict";

load("inflate.js");

/* ======================================================================= */
/* Part 1: byte utilities, Flate/LZW/ASCII filters, PDF object parser       */
/* ======================================================================= */

var P2S_VERSION = "1.0";

function Name(n) { this.name = n; }
function Ref(n, g) { this.num = n; this.gen = g; }
function Cmd(c) { this.cmd = c; }
var EOF_TOKEN = { eof: true };

function isDict(v)
{
	return v !== null && typeof v == "object" && !(v instanceof Array)
		&& !(v instanceof Name) && !(v instanceof Ref) && !(v instanceof Cmd)
		&& !(v instanceof PDFStream) && v !== EOF_TOKEN;
}
function isName(v, n)
{
	return (v instanceof Name) && (n === undefined || v.name == n);
}
function isNum(v) { return typeof v == "number"; }

/* Convert a binary string (as returned by File.read) to a Uint8Array */
function string_to_bytes(s)
{
	var n = s.length;
	var b = new Uint8Array(n);
	for (var i = 0; i < n; i++)
		b[i] = s.charCodeAt(i) & 0xff;
	return b;
}

/* Sub-range of a Uint8Array as a new view. Unlike TypedArray.subarray() this is correct when
 * the source is itself a view (SpiderMonkey 1.8.5 ignores the source's own offset). */
function bytes_sub(a, s, e)
{
	if (e === undefined || e > a.length)
		e = a.length;
	if (s < 0)
		s = 0;
	return new Uint8Array(a.buffer, a.byteOffset + s, e > s ? e - s : 0);
}

function bytes_to_string(b, start, end)
{
	var s = [];
	if (start === undefined)
		start = 0;
	if (end === undefined)
		end = b.length;
	for (var i = start; i < end; i += 4096)
		s.push(String.fromCharCode.apply(null, Array.prototype.slice.call(b, i, Math.min(end, i + 4096))));
	return s.join("");
}

function find_bytes(buf, str, from, to)
{
	var n = str.length;
	var c0 = str.charCodeAt(0);
	if (to === undefined || to > buf.length)
		to = buf.length;
	to -= n;
	for (var i = from; i <= to; i++) {
		if (buf[i] != c0)
			continue;
		var j = 1;
		while (j < n && buf[i + j] == str.charCodeAt(j))
			j++;
		if (j == n)
			return i;
	}
	return -1;
}

function rfind_bytes(buf, str, from)
{
	var n = str.length;
	var c0 = str.charCodeAt(0);
	for (var i = Math.min(from, buf.length - n); i >= 0; i--) {
		if (buf[i] != c0)
			continue;
		var j = 1;
		while (j < n && buf[i + j] == str.charCodeAt(j))
			j++;
		if (j == n)
			return i;
	}
	return -1;
}

/* ----- LZW ----- */
function lzw_decode(data, early)
{
	if (early === undefined)
		early = 1;
	var out = new Uint8Array(Math.max(4096, data.length * 4));
	var op = 0;
	var dict_prefix = new Int32Array(4097);
	var dict_char = new Uint8Array(4097);
	var dict_len = new Int32Array(4097);
	var next = 258, bits = 9, bb = 0, bc = 0, pos = 0, prev = -1, i;
	var stack = new Uint8Array(4100);
	for (;;) {
		while (bc < bits && pos < data.length) {
			bb = (bb << 8) | data[pos++];
			bc += 8;
		}
		if (bc < bits)
			break;
		var code = (bb >> (bc - bits)) & ((1 << bits) - 1);
		bc -= bits;
		bb &= (1 << bc) - 1;
		if (code == 256) {
			next = 258;
			bits = 9;
			prev = -1;
			continue;
		}
		if (code == 257)
			break;
		var sp = 0, c = code;
		if (code >= next) {
			if (prev < 0)
				break;
			/* KwKwK case */
			var p = prev;
			while (p >= 258)
				p = dict_prefix[p];
			stack[sp++] = p;	/* first char of prev (appended last) */
			c = prev;
		}
		while (c >= 258 && sp < 4096) {
			stack[sp++] = dict_char[c];
			c = dict_prefix[c];
		}
		stack[sp++] = c;
		if (op + sp > out.length) {
			var n = new Uint8Array(out.length * 2 + sp);
			n.set(out.subarray(0, op));
			out = n;
		}
		var first = stack[sp - 1];
		if (code >= next) {
			/* stack holds [firstchar-of-prev, ...prev reversed]; output prev then first char */
			for (i = sp - 1; i >= 1; i--)
				out[op++] = stack[i];
			out[op++] = stack[0];
			first = stack[sp - 1];
		}
		else {
			for (i = sp - 1; i >= 0; i--)
				out[op++] = stack[i];
		}
		if (prev >= 0 && next < 4096) {
			dict_prefix[next] = prev;
			dict_char[next] = first;
			next++;
		}
		prev = code;
		if (next + early >= (1 << bits) && bits < 12)
			bits++;
	}
	return out.subarray(0, op);
}

function ascii_hex_decode(data)
{
	var out = [], hi = -1;
	for (var i = 0; i < data.length; i++) {
		var c = data[i], v;
		if (c == 0x3e)
			break;
		if (c >= 0x30 && c <= 0x39)
			v = c - 0x30;
		else if (c >= 0x41 && c <= 0x46)
			v = c - 0x41 + 10;
		else if (c >= 0x61 && c <= 0x66)
			v = c - 0x61 + 10;
		else
			continue;
		if (hi < 0)
			hi = v;
		else {
			out.push((hi << 4) | v);
			hi = -1;
		}
	}
	if (hi >= 0)
		out.push(hi << 4);
	return new Uint8Array(out);
}

function ascii85_decode(data)
{
	var out = [], grp = [], i;
	for (i = 0; i < data.length; i++) {
		var c = data[i];
		if (c == 0x7e)
			break;
		if (c <= 0x20)
			continue;
		if (c == 0x7a && grp.length == 0) {
			out.push(0, 0, 0, 0);
			continue;
		}
		if (c < 0x21 || c > 0x75)
			continue;
		grp.push(c - 33);
		if (grp.length == 5) {
			var v = ((((grp[0] * 85 + grp[1]) * 85 + grp[2]) * 85 + grp[3]) * 85 + grp[4]);
			out.push((v >>> 24) & 255, (v >>> 16) & 255, (v >>> 8) & 255, v & 255);
			grp = [];
		}
	}
	if (grp.length > 1) {
		var n = grp.length;
		while (grp.length < 5)
			grp.push(84);
		var v2 = ((((grp[0] * 85 + grp[1]) * 85 + grp[2]) * 85 + grp[3]) * 85 + grp[4]);
		var b = [(v2 >>> 24) & 255, (v2 >>> 16) & 255, (v2 >>> 8) & 255, v2 & 255];
		for (i = 0; i < n - 1; i++)
			out.push(b[i]);
	}
	return new Uint8Array(out);
}

function runlength_decode(data)
{
	var out = [], i = 0;
	while (i < data.length) {
		var n = data[i++];
		if (n == 128)
			break;
		if (n < 128) {
			for (var j = 0; j <= n && i < data.length; j++)
				out.push(data[i++]);
		}
		else {
			var v = data[i++];
			for (var k = 0; k < 257 - n; k++)
				out.push(v);
		}
	}
	return new Uint8Array(out);
}

function apply_predictor(data, parms, doc)
{
	if (!isDict(parms))
		return data;
	var pred = doc.resolve(parms.Predictor);
	if (!pred || pred < 2)
		return data;
	var colors = doc.resolve(parms.Colors) || 1;
	var bpc = doc.resolve(parms.BitsPerComponent) || 8;
	var cols = doc.resolve(parms.Columns) || 1;
	var rowbytes = Math.ceil(colors * bpc * cols / 8);
	var bpp = Math.max(1, Math.ceil(colors * bpc / 8));
	var x, y;
	if (pred == 2) {
		if (bpc == 8) {
			var nrows = Math.floor(data.length / rowbytes);
			for (y = 0; y < nrows; y++) {
				var ro = y * rowbytes;
				for (x = bpp; x < rowbytes; x++)
					data[ro + x] = (data[ro + x] + data[ro + x - bpp]) & 255;
			}
		}
		return data;
	}
	var rows = Math.floor((data.length + rowbytes) / (rowbytes + 1));
	var out = new Uint8Array(rows * rowbytes);
	for (y = 0; y < rows; y++) {
		var ft = data[y * (rowbytes + 1)];
		var so = y * (rowbytes + 1) + 1;
		if (so + rowbytes > data.length)
			break;	/* truncated final row */
		var o = y * rowbytes, po = o - rowbytes;
		var d = so - o;	/* data[o + x + d] is the filtered byte for column x */
		var e = o + rowbytes;
		var xi, a, b, c, p, pa, pb, pc;
		if (ft == 1) {
			for (xi = o; xi < o + bpp && xi < e; xi++)
				out[xi] = data[xi + d];
			for (; xi < e; xi++)
				out[xi] = (data[xi + d] + out[xi - bpp]) & 255;
		}
		else if (ft == 2 && y > 0) {
			for (xi = o; xi < e; xi++)
				out[xi] = (data[xi + d] + out[xi - rowbytes]) & 255;
		}
		else if (ft == 3) {
			var xe = o + bpp < e ? o + bpp : e;
			for (xi = o; xi < xe; xi++)
				out[xi] = (data[xi + d] + ((y > 0 ? out[xi - rowbytes] : 0) >> 1)) & 255;
			if (y > 0) {
				for (; xi < e; xi++)
					out[xi] = (data[xi + d] + ((out[xi - bpp] + out[xi - rowbytes]) >> 1)) & 255;
			}
			else {
				for (; xi < e; xi++)
					out[xi] = (data[xi + d] + (out[xi - bpp] >> 1)) & 255;
			}
		}
		else if (ft == 4) {
			var xe4 = o + bpp < e ? o + bpp : e;
			if (y > 0) {
				for (xi = o; xi < xe4; xi++)
					out[xi] = (data[xi + d] + out[xi - rowbytes]) & 255;
				for (; xi < e; xi++) {
					a = out[xi - bpp];
					b = out[xi - rowbytes];
					c = out[xi - rowbytes - bpp];
					pa = b - c;
					pb = a - c;
					pc = pa + pb;
					if (pa < 0) pa = -pa;
					if (pb < 0) pb = -pb;
					if (pc < 0) pc = -pc;
					out[xi] = (data[xi + d] + ((pa <= pb && pa <= pc) ? a : (pb <= pc ? b : c))) & 255;
				}
			}
			else {
				for (xi = o; xi < xe4; xi++)
					out[xi] = data[xi + d];
				for (; xi < e; xi++)
					out[xi] = (data[xi + d] + out[xi - bpp]) & 255;
			}
		}
		else {
			for (xi = o; xi < e; xi++)
				out[xi] = data[xi + d];
		}
	}
	return out;
}


/* ----- CCITT Group 3 / Group 4 fax decoding ----- */

var CCITT_WHITE = "00110101,000111,0111,1000,1011,1100,1110,1111,10011,10100,00111,01000,001000,000011,110100,110101,101010,101011,0100111,0001100,0001000,"
	+ "0010111,0000011,0000100,0101000,0101011,0010011,0100100,0011000,00000010,00000011,00011010,00011011,00010010,00010011,00010100,00010101,"
	+ "00010110,00010111,00101000,00101001,00101010,00101011,00101100,00101101,00000100,00000101,00001010,00001011,01010010,01010011,01010100,"
	+ "01010101,00100100,00100101,01011000,01011001,01011010,01011011,01001010,01001011,00110010,00110011,00110100";
var CCITT_WHITE_MAKEUP = "11011,10010,010111,0110111,00110110,00110111,01100100,01100101,01101000,01100111,011001100,011001101,011010010,011010011,"
	+ "011010100,011010101,011010110,011010111,011011000,011011001,011011010,011011011,010011000,010011001,010011010,011000,010011011";
var CCITT_BLACK = "0000110111,010,11,10,011,0011,0010,00011,000101,000100,0000100,0000101,0000111,00000100,00000111,000011000,0000010111,0000011000,"
	+ "0000001000,00001100111,00001101000,00001101100,00000110111,00000101000,00000010111,00000011000,000011001010,000011001011,000011001100,"
	+ "000011001101,000001101000,000001101001,000001101010,000001101011,000011010010,000011010011,000011010100,000011010101,000011010110,"
	+ "000011010111,000001101100,000001101101,000011011010,000011011011,000001010100,000001010101,000001010110,000001010111,000001100100,"
	+ "000001100101,000001010010,000001010011,000000100100,000000110111,000000111000,000000100111,000000101000,000001011000,000001011001,"
	+ "000000101011,000000101100,000001011010,000001100110,000001100111";
var CCITT_BLACK_MAKEUP = "0000001111,000011001000,000011001001,000001011011,000000110011,000000110100,000000110101,0000001101100,0000001101101,"
	+ "0000001001010,0000001001011,0000001001100,0000001001101,0000001110010,0000001110011,0000001110100,0000001110101,0000001110110,"
	+ "0000001110111,0000001010010,0000001010011,0000001010100,0000001010101,0000001011010,0000001011011,0000001100100,0000001100101";
var CCITT_EXT_MAKEUP = "00000001000,00000001100,00000001101,000000010010,000000010011,000000010100,000000010101,000000010110,000000010111,"
	+ "000000011100,000000011101,000000011110,000000011111";

var CCITT_TRIES = null;

/* trie stored as arrays: t[node*2 + bit] = next node (>0) or -(value+1) for a leaf */
function ccitt_add(trie, code, value)
{
	var node = 0;
	for (var i = 0; i < code.length; i++) {
		var b = code.charCodeAt(i) - 48;
		var idx = node * 2 + b;
		if (i == code.length - 1) {
			trie.t[idx] = -(value + 1);
			return;
		}
		if (!trie.t[idx]) {
			trie.t[idx] = ++trie.n;
		}
		node = trie.t[idx];
	}
}

function ccitt_build()
{
	var white = { t: [], n: 0 }, black = { t: [], n: 0 }, mode = { t: [], n: 0 };
	var i, a;
	a = CCITT_WHITE.split(",");
	for (i = 0; i < a.length; i++)
		ccitt_add(white, a[i], i);
	a = CCITT_WHITE_MAKEUP.split(",");
	for (i = 0; i < a.length; i++)
		ccitt_add(white, a[i], 64 * (i + 1));
	a = CCITT_BLACK.split(",");
	for (i = 0; i < a.length; i++)
		ccitt_add(black, a[i], i);
	a = CCITT_BLACK_MAKEUP.split(",");
	for (i = 0; i < a.length; i++)
		ccitt_add(black, a[i], 64 * (i + 1));
	a = CCITT_EXT_MAKEUP.split(",");
	for (i = 0; i < a.length; i++) {
		ccitt_add(white, a[i], 1792 + 64 * i);
		ccitt_add(black, a[i], 1792 + 64 * i);
	}
	/* modes: 0 = pass, 1 = horizontal, 2..8 = V0, VR1, VR2, VR3, VL1, VL2, VL3 */
	ccitt_add(mode, "0001", 0);
	ccitt_add(mode, "001", 1);
	ccitt_add(mode, "1", 2);
	ccitt_add(mode, "011", 3);
	ccitt_add(mode, "000011", 4);
	ccitt_add(mode, "0000011", 5);
	ccitt_add(mode, "010", 6);
	ccitt_add(mode, "000010", 7);
	ccitt_add(mode, "0000010", 8);
	return { white: white, black: black, mode: mode };
}

function ccitt_decode(data, parms, doc, height)
{
	if (!CCITT_TRIES)
		CCITT_TRIES = ccitt_build();
	var K = 0, columns = 1728, rows = 0, blackis1 = false, align = false;
	if (isDict(parms)) {
		var v = doc.resolve(parms.K);
		if (typeof v == "number")
			K = v;
		v = doc.resolve(parms.Columns);
		if (typeof v == "number")
			columns = v;
		v = doc.resolve(parms.Rows);
		if (typeof v == "number")
			rows = v;
		blackis1 = doc.resolve(parms.BlackIs1) === true;
		align = doc.resolve(parms.EncodedByteAlign) === true;
	}
	if (!rows && height)
		rows = height;
	var rowbytes = (columns + 7) >> 3;
	var out = new Uint8Array(rowbytes * (rows || 1024));
	var outrows = 0;
	var bp = 0, nbits = data.length * 8;
	var white = CCITT_TRIES.white.t, black = CCITT_TRIES.black.t, modet = CCITT_TRIES.mode.t;

	function bit()
	{
		if (bp >= nbits) {
			bp++;
			return 0;
		}
		var b = (data[bp >> 3] >> (7 - (bp & 7))) & 1;
		bp++;
		return b;
	}
	function peek(n)
	{
		var v = 0;
		for (var i = 0; i < n; i++) {
			var p = bp + i;
			v = (v << 1) | (p < nbits ? ((data[p >> 3] >> (7 - (p & 7))) & 1) : 0);
		}
		return v;
	}
	/* decode one code from a trie: returns value or -1 */
	function code(t)
	{
		var node = 0;
		for (var i = 0; i < 14; i++) {
			var e = t[node * 2 + bit()];
			if (!e)
				return -1;
			if (e < 0)
				return -e - 1;
			node = e;
		}
		return -1;
	}
	/* total run length for a colour */
	function run(isblack)
	{
		var total = 0;
		for (;;) {
			var r = code(isblack ? black : white);
			if (r < 0)
				return -1;
			total += r;
			if (r < 64)
				return total;
		}
	}
	function skip_eol()
	{
		/* consume fill zeros and an EOL (000000000001) if present; returns true if an EOL was consumed */
		var save = bp;
		var zeros = 0;
		while (bp < nbits && !bit())
			zeros++;
		if (zeros >= 11 && bp <= nbits)
			return true;
		bp = save;
		return false;
	}

	var ref = [columns, columns, columns];
	var failed = false;
	while (!failed && (!rows || outrows < rows) && bp < nbits) {
		if (align && (bp & 7))
			bp = (bp + 7) & ~7;
		var cur = [];
		var is2d = K < 0;
		if (K >= 0) {
			/* optional EOLs (and the tag bit for mixed mode) */
			var eols = 0;
			while (skip_eol()) {
				eols++;
				if (K > 0)
					is2d = bit() == 0;
			}
			if (eols >= 6)
				break;	/* RTC */
			if (K > 0 && eols == 0)
				is2d = false;
			if (bp >= nbits)
				break;
		}
		else {
			/* G4: EOFB is two EOL codes (000000000001000000000001) */
			if (peek(12) == 1 && ((bp + 24) <= nbits)) {
				break;
			}
		}
		if (!is2d) {
			var x = 0, isb = false;
			while (x < columns) {
				var r = run(isb);
				if (r < 0) {
					failed = true;
					break;
				}
				x += r;
				if (x > columns)
					x = columns;
				cur.push(x);
				isb = !isb;
			}
		}
		else {
			var a0 = -1, color = 0, bi = 0;
			while (a0 < columns) {
				var m = code(modet);
				if (m < 0) {
					failed = true;
					break;
				}
				/* locate b1 / b2 */
				bi = (bi & ~1) | color;
				while (bi >= 2 && ref[bi - 2] > a0)
					bi -= 2;
				while (ref[bi] <= a0 && ref[bi] < columns)
					bi += 2;
				var b1 = ref[bi], b2 = ref[bi + 1];
				if (b1 === undefined) {
					b1 = columns;
					b2 = columns;
				}
				if (m == 0) {
					a0 = b2;
				}
				else if (m == 1) {
					var start = a0 < 0 ? 0 : a0;
					var r1 = run(color == 1), r2 = run(color == 0);
					if (r1 < 0 || r2 < 0) {
						failed = true;
						break;
					}
					var p1 = Math.min(columns, start + r1), p2 = Math.min(columns, p1 + r2);
					cur.push(p1, p2);
					a0 = p2;
				}
				else {
					var dlt = [0, 0, 0, 1, 2, 3, -1, -2, -3][m];
					var a1 = b1 + dlt;
					if (a1 < 0)
						a1 = 0;
					if (a1 > columns)
						a1 = columns;
					cur.push(a1);
					a0 = a1;
					color ^= 1;
				}
			}
		}
		/* paint the row: cur holds change positions starting with white->black */
		if (outrows * rowbytes + rowbytes > out.length) {
			var no = new Uint8Array(out.length * 2);
			no.set(out);
			out = no;
		}
		var ro = outrows * rowbytes;
		if (!blackis1)
			for (var q = 0; q < rowbytes; q++)
				out[ro + q] = 255;
		for (var ci = 0; ci < cur.length; ci += 2) {
			var xs = cur[ci], xe = ci + 1 < cur.length ? cur[ci + 1] : columns;
			for (var px = xs; px < xe; px++) {
				if (blackis1)
					out[ro + (px >> 3)] |= (0x80 >> (px & 7));
				else
					out[ro + (px >> 3)] &= ~(0x80 >> (px & 7));
			}
		}
		outrows++;
		/* the changing elements of this row become the reference line */
		ref = cur.slice();
		ref.push(columns, columns, columns);
	}
	if (rows && outrows < rows) {
		/* undecoded rows are white */
		var fill = blackis1 ? 0 : 255;
		for (var yy = outrows * rowbytes; yy < rows * rowbytes; yy++)
			out[yy] = fill;
		return out.subarray(0, rows * rowbytes);
	}
	return out.subarray(0, outrows * rowbytes);
}

/* ----- Lexer ----- */

var LEX_CLASS = new Uint8Array(256);	/* 1 = whitespace, 2 = delimiter */
(function () {
	var ws = [0, 9, 10, 12, 13, 32];
	var dl = "()<>[]{}/%";
	var i;
	for (i = 0; i < ws.length; i++)
		LEX_CLASS[ws[i]] = 1;
	for (i = 0; i < dl.length; i++)
		LEX_CLASS[dl.charCodeAt(i)] = 2;
})();

function Lexer(buf, pos, end)
{
	this.buf = buf;
	this.pos = pos || 0;
	this.end = end === undefined ? buf.length : end;
}

Lexer.prototype.skip_ws = function ()
{
	var buf = this.buf, end = this.end, pos = this.pos;
	while (pos < end) {
		var c = buf[pos];
		if (LEX_CLASS[c] == 1)
			pos++;
		else if (c == 0x25) {	/* % comment */
			while (pos < end && buf[pos] != 10 && buf[pos] != 13)
				pos++;
		}
		else
			break;
	}
	this.pos = pos;
};

Lexer.prototype.token = function ()
{
	this.skip_ws();
	var buf = this.buf, end = this.end, pos = this.pos;
	if (pos >= end)
		return EOF_TOKEN;
	var c = buf[pos];
	var s, i;
	switch (c) {
		case 0x2f:	/* / name */
			pos++;
			s = [];
			while (pos < end && !LEX_CLASS[buf[pos]]) {
				var ch = buf[pos];
				if (ch == 0x23 && pos + 2 < end) {
					var h = parseInt(String.fromCharCode(buf[pos + 1], buf[pos + 2]), 16);
					if (!isNaN(h)) {
						s.push(String.fromCharCode(h));
						pos += 3;
						continue;
					}
				}
				s.push(String.fromCharCode(ch));
				pos++;
			}
			this.pos = pos;
			return new Name(s.join(""));
		case 0x28: {	/* ( string ) */
			pos++;
			var depth = 1;
			s = [];
			while (pos < end) {
				ch = buf[pos++];
				if (ch == 0x5c) {	/* backslash */
					ch = buf[pos++];
					switch (ch) {
						case 0x6e: s.push("\n"); break;
						case 0x72: s.push("\r"); break;
						case 0x74: s.push("\t"); break;
						case 0x62: s.push("\b"); break;
						case 0x66: s.push("\f"); break;
						case 0x0d:
							if (buf[pos] == 0x0a)
								pos++;
							break;
						case 0x0a: break;
						default:
							if (ch >= 0x30 && ch <= 0x37) {
								var o = ch - 0x30;
								for (i = 0; i < 2; i++) {
									var d = buf[pos];
									if (d >= 0x30 && d <= 0x37) {
										o = o * 8 + d - 0x30;
										pos++;
									}
									else
										break;
								}
								s.push(String.fromCharCode(o & 255));
							}
							else
								s.push(String.fromCharCode(ch));
					}
				}
				else if (ch == 0x28) {
					depth++;
					s.push("(");
				}
				else if (ch == 0x29) {
					if (--depth == 0)
						break;
					s.push(")");
				}
				else
					s.push(String.fromCharCode(ch));
			}
			this.pos = pos;
			return s.join("");
		}
		case 0x3c:	/* < */
			if (buf[pos + 1] == 0x3c) {
				this.pos = pos + 2;
				return new Cmd("<<");
			}
			pos++;
			s = [];
			var hi = -1;
			while (pos < end && buf[pos] != 0x3e) {
				ch = buf[pos++];
				var v;
				if (ch >= 0x30 && ch <= 0x39)
					v = ch - 0x30;
				else if (ch >= 0x41 && ch <= 0x46)
					v = ch - 0x41 + 10;
				else if (ch >= 0x61 && ch <= 0x66)
					v = ch - 0x61 + 10;
				else
					continue;
				if (hi < 0)
					hi = v;
				else {
					s.push(String.fromCharCode((hi << 4) | v));
					hi = -1;
				}
			}
			if (hi >= 0)
				s.push(String.fromCharCode(hi << 4));
			this.pos = pos + 1;
			return s.join("");
		case 0x3e:	/* > */
			if (buf[pos + 1] == 0x3e) {
				this.pos = pos + 2;
				return new Cmd(">>");
			}
			this.pos = pos + 1;
			return this.token();
		case 0x5b: this.pos = pos + 1; return new Cmd("[");
		case 0x5d: this.pos = pos + 1; return new Cmd("]");
		case 0x7b: this.pos = pos + 1; return new Cmd("{");
		case 0x7d: this.pos = pos + 1; return new Cmd("}");
		case 0x29: this.pos = pos + 1; return this.token();	/* stray ) */
	}
	/* number or keyword */
	var st = pos;
	while (pos < end && !LEX_CLASS[buf[pos]])
		pos++;
	this.pos = pos;
	var w = bytes_to_string(buf, st, pos);
	c = w.charCodeAt(0);
	if ((c >= 0x30 && c <= 0x39) || c == 0x2b || c == 0x2d || c == 0x2e) {
		if (/^[+-]?(\d+\.?\d*|\.\d+)$/.test(w))
			return parseFloat(w);
		/* things like "--5" or "1.2.3": be forgiving */
		var m = /^[+-]*(\d*\.?\d*)/.exec(w);
		if (m && m[1].length && /\d/.test(m[1]))
			return (w.charAt(0) == '-' ? -1 : 1) * parseFloat(m[1]);
	}
	return new Cmd(w);
};

function parse_value(lx, tok, noref)
{
	var t, arr, obj, v;
	if (typeof tok == "number") {
		if (!noref && tok >= 0 && Math.floor(tok) === tok) {
			var save = lx.pos;
			var t2 = lx.token();
			if (typeof t2 == "number" && t2 >= 0 && Math.floor(t2) === t2) {
				var t3 = lx.token();
				if ((t3 instanceof Cmd) && t3.cmd == "R")
					return new Ref(tok, t2);
			}
			lx.pos = save;
		}
		return tok;
	}
	if (tok instanceof Cmd) {
		switch (tok.cmd) {
			case "[":
				arr = [];
				for (;;) {
					t = lx.token();
					if (t === EOF_TOKEN || ((t instanceof Cmd) && t.cmd == "]"))
						break;
					arr.push(parse_value(lx, t, noref));
				}
				return arr;
			case "<<":
				obj = {};
				for (;;) {
					t = lx.token();
					if (t === EOF_TOKEN || ((t instanceof Cmd) && t.cmd == ">>"))
						break;
					if (!(t instanceof Name))
						continue;
					v = lx.token();
					if (v === EOF_TOKEN || ((v instanceof Cmd) && v.cmd == ">>"))
						break;
					obj[t.name] = parse_value(lx, v, noref);
				}
				return obj;
			case "true": return true;
			case "false": return false;
			case "null": return null;
		}
	}
	return tok;
}

/* ----- Encryption support (standard security handler, empty user password) ----- */

var MD5_K = null;
var MD5_S = [7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20,
	4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21];

/* MD5 of a binary string; returns a 16 character binary string */
function md5_raw(str)
{
	var i;
	if (!MD5_K) {
		MD5_K = [];
		for (i = 0; i < 64; i++)
			MD5_K[i] = Math.floor(Math.abs(Math.sin(i + 1)) * 4294967296) >>> 0;
	}
	var n = str.length;
	var total = (((n + 8) >> 6) + 1) << 6;
	var m = new Uint8Array(total);
	for (i = 0; i < n; i++)
		m[i] = str.charCodeAt(i) & 255;
	m[n] = 0x80;
	var bits = n * 8;
	m[total - 8] = bits & 255;
	m[total - 7] = (bits >>> 8) & 255;
	m[total - 6] = (bits >>> 16) & 255;
	m[total - 5] = (bits >>> 24) & 255;
	var a0 = 0x67452301, b0 = 0xefcdab89 | 0, c0 = 0x98badcfe | 0, d0 = 0x10325476;
	var w = new Array(16);
	for (var off = 0; off < total; off += 64) {
		for (i = 0; i < 16; i++)
			w[i] = m[off + i * 4] | (m[off + i * 4 + 1] << 8) | (m[off + i * 4 + 2] << 16) | (m[off + i * 4 + 3] << 24);
		var a = a0, b = b0, c = c0, d = d0;
		for (i = 0; i < 64; i++) {
			var f, g;
			if (i < 16) {
				f = (b & c) | (~b & d);
				g = i;
			}
			else if (i < 32) {
				f = (d & b) | (~d & c);
				g = (5 * i + 1) & 15;
			}
			else if (i < 48) {
				f = b ^ c ^ d;
				g = (3 * i + 5) & 15;
			}
			else {
				f = c ^ (b | ~d);
				g = (7 * i) & 15;
			}
			f = (f + a + MD5_K[i] + w[g]) | 0;
			a = d;
			d = c;
			c = b;
			b = (b + ((f << MD5_S[i]) | (f >>> (32 - MD5_S[i])))) | 0;
		}
		a0 = (a0 + a) | 0;
		b0 = (b0 + b) | 0;
		c0 = (c0 + c) | 0;
		d0 = (d0 + d) | 0;
	}
	var out = "";
	var v = [a0, b0, c0, d0];
	for (i = 0; i < 4; i++)
		out += String.fromCharCode(v[i] & 255, (v[i] >>> 8) & 255, (v[i] >>> 16) & 255, (v[i] >>> 24) & 255);
	return out;
}

function rc4_crypt(keystr, data)
{
	var S = new Uint8Array(256), i, j = 0, t;
	var kl = keystr.length;
	for (i = 0; i < 256; i++)
		S[i] = i;
	for (i = 0; i < 256; i++) {
		j = (j + S[i] + keystr.charCodeAt(i % kl)) & 255;
		t = S[i];
		S[i] = S[j];
		S[j] = t;
	}
	var out = new Uint8Array(data.length);
	var x = 0, y = 0;
	for (var n = 0; n < data.length; n++) {
		x = (x + 1) & 255;
		y = (y + S[x]) & 255;
		t = S[x];
		S[x] = S[y];
		S[y] = t;
		out[n] = data[n] ^ S[(S[x] + S[y]) & 255];
	}
	return out;
}

var PDF_PAD = "\x28\xbf\x4e\x5e\x4e\x75\x8a\x41\x64\x00\x4e\x56\xff\xfa\x01\x08\x2e\x2e\x00\xb6\xd0\x68\x3e\x80\x2f\x0c\xa9\xfe\x64\x53\x69\x7a";

/* ----- Streams ----- */

function PDFStream(doc, dict, start, len)
{
	this.doc = doc;
	this.dict = dict;
	this.start = start;
	this.len = len;
	this.decoded = null;
	this.num = 0;
	this.buf = null;	/* private buffer (inline images); defaults to the document buffer */
}

/* Returns { data: Uint8Array, filter: <unapplied image filter name or null>, parms: dict } */
PDFStream.prototype.decode = function ()
{
	if (this.decoded)
		return this.decoded;
	var doc = this.doc;
	var data = bytes_sub(this.buf || doc.buf, this.start, this.start + this.len);
	if (this.crypt && doc.crypt)
		data = doc.decrypt_bytes(data, this.crypt.num, this.crypt.gen, true);
	var f = doc.resolve(this.dict.Filter);
	var p = doc.resolve(this.dict.DecodeParms);
	if (p === undefined)
		p = doc.resolve(this.dict.DP);
	if (f === undefined)
		f = doc.resolve(this.dict.F);
	var filters = [], parms = [];
	if (f instanceof Array) {
		for (var i = 0; i < f.length; i++) {
			filters.push(doc.resolve(f[i]));
			parms.push(p instanceof Array ? doc.resolve(p[i]) : undefined);
		}
	}
	else if (f instanceof Name) {
		filters.push(f);
		parms.push(p instanceof Array ? doc.resolve(p[0]) : p);
	}
	var result = { data: data, filter: null, parms: null };
	for (var k = 0; k < filters.length; k++) {
		var fn = filters[k].name;
		var pm = parms[k];
		switch (fn) {
			case "FlateDecode": case "Fl":
				data = apply_predictor(inflate_zlib(data), pm, doc);
				break;
			case "LZWDecode": case "LZW":
				data = apply_predictor(lzw_decode(data, isDict(pm) && doc.resolve(pm.EarlyChange) !== undefined ? doc.resolve(pm.EarlyChange) : 1), pm, doc);
				break;
			case "ASCIIHexDecode": case "AHx":
				data = ascii_hex_decode(data);
				break;
			case "ASCII85Decode": case "A85":
				data = ascii85_decode(data);
				break;
			case "RunLengthDecode": case "RL":
				data = runlength_decode(data);
				break;
			case "CCITTFaxDecode": case "CCF":
				data = ccitt_decode(data, pm, doc, doc.resolve(this.dict.Height) || doc.resolve(this.dict.H) || 0);
				break;
			case "Crypt":
				break;
			default:	/* DCTDecode, JPXDecode, CCITTFaxDecode, JBIG2Decode */
				result.filter = fn;
				result.parms = pm;
				k = filters.length;
		}
	}
	result.data = data;
	this.decoded = result;
	return result;
};

PDFStream.prototype.bytes = function ()
{
	return this.decode().data;
};

/* ----- Document ----- */

function PDFDoc(buf)
{
	this.buf = buf;
	this.xref = {};
	this.cache = {};
	this.trailer = null;
	this.rebuilt = false;
	this.encrypted = false;
	this.crypt = null;
	this.crypt_error = null;
	this.pages = null;
	this.init();
}

PDFDoc.prototype.init = function ()
{
	var ok = false;
	try {
		ok = this.read_xref_chain();
		if (ok) {
			this.setup_encryption();
			var root = this.resolve(this.trailer.Root);
			if (!isDict(root) || !isDict(this.resolve(root.Pages)))
				ok = false;
		}
	}
	catch (e) {
		ok = false;
	}
	if (!ok)
		this.rebuild();
	if (!this.trailer || !isDict(this.resolve(this.trailer.Root)))
		throw "Unable to locate the document catalog (not a valid PDF?)";
};


/* Initialise decryption (empty user password). Sets this.crypt or this.crypt_error. */
PDFDoc.prototype.setup_encryption = function ()
{
	if (this.crypt || !this.trailer || this.trailer.Encrypt === undefined)
		return;
	this.encrypted = true;
	this.cache = {};
	var enc = this.resolve(this.trailer.Encrypt);
	if (!isDict(enc)) {
		this.crypt_error = "Unreadable encryption dictionary";
		return;
	}
	var filter = this.resolve(enc.Filter);
	var V = this.resolve(enc.V) || 0, R = this.resolve(enc.R) || 0;
	if (!isName(filter, "Standard") || V > 4 || R > 4 || R < 2) {
		this.crypt_error = "Unsupported encryption (V" + V + " R" + R + ")";
		return;
	}
	var O = this.resolve(enc.O), U = this.resolve(enc.U);
	var P = this.resolve(enc.P);
	if (typeof O != "string" || typeof U != "string" || typeof P != "number") {
		this.crypt_error = "Bad encryption dictionary";
		return;
	}
	var length = this.resolve(enc.Length) || 40;
	var stm = "rc4", str = "rc4";
	if (V == 4) {
		var cf = this.resolve(enc.CF);
		var kind = function (doc, nm) {
			var n = doc.resolve(nm);
			if (!(n instanceof Name) || n.name == "Identity")
				return "none";
			var f = isDict(cf) ? doc.resolve(cf[n.name]) : null;
			var m = isDict(f) ? doc.resolve(f.CFM) : null;
			if (isName(m, "AESV2"))
				return "aes";
			if (isName(m, "V2"))
				return "rc4";
			return "none";
		};
		stm = kind(this, enc.StmF);
		str = kind(this, enc.StrF);
		length = 128;
	}
	if (V == 1)
		length = 40;
	var id0 = "";
	var ids = this.resolve(this.trailer.ID);
	if (ids instanceof Array && typeof this.resolve(ids[0]) == "string")
		id0 = this.resolve(ids[0]);
	var encmeta = this.resolve(enc.EncryptMetadata) !== false;
	var data = PDF_PAD + O.substr(0, 32);
	var pu = P >>> 0;
	data += String.fromCharCode(pu & 255, (pu >>> 8) & 255, (pu >>> 16) & 255, (pu >>> 24) & 255);
	data += id0;
	if (R >= 4 && !encmeta)
		data += "\xff\xff\xff\xff";
	var h = md5_raw(data);
	var n = R == 2 ? 5 : Math.min(16, length >> 3);
	var i;
	if (R >= 3)
		for (i = 0; i < 50; i++)
			h = md5_raw(h.substr(0, n));
	var key = h.substr(0, n);
	/* verify that the empty password is the user password (Algorithms 4/5) */
	var okpw;
	if (R == 2)
		okpw = bytes_to_string(rc4_crypt(key, string_to_bytes(PDF_PAD))) == U.substr(0, 32);
	else {
		var x = bytes_to_string(rc4_crypt(key, string_to_bytes(md5_raw(PDF_PAD + id0))));
		for (i = 1; i <= 19; i++) {
			var k2 = "";
			for (var j = 0; j < key.length; j++)
				k2 += String.fromCharCode(key.charCodeAt(j) ^ i);
			x = bytes_to_string(rc4_crypt(k2, string_to_bytes(x)));
		}
		okpw = x == U.substr(0, 16);
	}
	if (!okpw) {
		this.crypt_error = "The document is password protected";
		return;
	}
	this.crypt = { key: key, stm: stm, str: str };
};

PDFDoc.prototype.object_key = function (num, gen, aes)
{
	var k = this.crypt.key;
	var s = k + String.fromCharCode(num & 255, (num >> 8) & 255, (num >> 16) & 255, gen & 255, (gen >> 8) & 255);
	if (aes)
		s += "sAlT";
	return md5_raw(s).substr(0, Math.min(k.length + 5, 16));
};

PDFDoc.prototype.decrypt_bytes = function (data, num, gen, isstream)
{
	var mode = isstream ? this.crypt.stm : this.crypt.str;
	if (mode == "none")
		return data;
	var key = this.object_key(num, gen, mode == "aes");
	if (mode == "rc4")
		return rc4_crypt(key, data);
	if (data.length < 32)
		return new Uint8Array(0);
	var iv = bytes_to_string(data, 0, 16);
	var body = bytes_to_string(data, 16, data.length - ((data.length - 16) & 15));
	var cc = new CryptContext(CryptContext.ALGO.AES);
	cc.mode = CryptContext.MODE.CBC;
	cc.set_key(key);
	cc.iv = iv;
	var pt = cc.decrypt(body);
	var padn = pt.length ? pt.charCodeAt(pt.length - 1) : 0;
	if (padn >= 1 && padn <= 16)
		pt = pt.substr(0, pt.length - padn);
	return string_to_bytes(pt);
};

PDFDoc.prototype.decrypt_strings = function (v, num, gen)
{
	if (typeof v == "string") {
		if (v.length == 0)
			return v;
		return bytes_to_string(this.decrypt_bytes(string_to_bytes(v), num, gen, false));
	}
	if (v instanceof Array) {
		for (var i = 0; i < v.length; i++)
			v[i] = this.decrypt_strings(v[i], num, gen);
		return v;
	}
	if (isDict(v)) {
		for (var k in v)
			v[k] = this.decrypt_strings(v[k], num, gen);
	}
	return v;
};

PDFDoc.prototype.resolve = function (v)
{
	var n = 0;
	while (v instanceof Ref && n++ < 32)
		v = this.get_object(v.num);
	return v;
};

PDFDoc.prototype.read_xref_chain = function ()
{
	var buf = this.buf;
	var sx = rfind_bytes(buf, "startxref", buf.length);
	if (sx < 0)
		return false;
	var lx = new Lexer(buf, sx + 9);
	var pos = lx.token();
	if (typeof pos != "number")
		return false;
	var seen = {};
	while (typeof pos == "number" && !seen[pos]) {
		seen[pos] = true;
		var tr = this.read_xref_section(pos);
		if (!tr)
			return false;
		if (!this.trailer)
			this.trailer = tr;
		if (typeof tr.XRefStm == "number")
			this.read_xref_section(tr.XRefStm);
		pos = tr.Prev;
	}
	return this.trailer && this.trailer.Root !== undefined;
};

PDFDoc.prototype.read_xref_section = function (pos)
{
	var buf = this.buf;
	var lx = new Lexer(buf, pos);
	var t = lx.token();
	var i;
	if ((t instanceof Cmd) && t.cmd == "xref") {
		for (;;) {
			t = lx.token();
			if ((t instanceof Cmd) && t.cmd == "trailer")
				break;
			if (typeof t != "number")
				return null;
			var first = t;
			var count = lx.token();
			if (typeof count != "number")
				return null;
			for (i = 0; i < count; i++) {
				var off = lx.token();
				var gen = lx.token();
				var flag = lx.token();
				if (typeof off != "number" || !(flag instanceof Cmd))
					return null;
				if (this.xref[first + i] === undefined)
					this.xref[first + i] = flag.cmd == "n" ? { t: 1, off: off, gen: gen } : { t: 0 };
			}
		}
		var d = parse_value(lx, lx.token());
		return isDict(d) ? d : null;
	}
	if (typeof t == "number") {
		var obj = this.parse_indirect(pos);
		if (!(obj instanceof PDFStream))
			return null;
		var dict = obj.dict;
		var data = obj.bytes();
		var W = dict.W;
		if (!(W instanceof Array))
			return null;
		var size = this.resolve(dict.Size) || 0;
		var index = dict.Index instanceof Array ? dict.Index : [0, size];
		var rl = W[0] + W[1] + W[2];
		var p = 0;
		for (var s = 0; s + 1 < index.length; s += 2) {
			for (i = 0; i < index[s + 1]; i++) {
				if (p + rl > data.length)
					break;
				var f = [0, 0, 0];
				for (var k = 0; k < 3; k++) {
					var v = 0;
					for (var b = 0; b < W[k]; b++)
						v = v * 256 + data[p++];
					f[k] = v;
				}
				if (W[0] == 0)
					f[0] = 1;
				var num = index[s] + i;
				if (this.xref[num] !== undefined)
					continue;
				if (f[0] == 1)
					this.xref[num] = { t: 1, off: f[1], gen: f[2] };
				else if (f[0] == 2)
					this.xref[num] = { t: 2, stm: f[1], idx: f[2] };
				else
					this.xref[num] = { t: 0 };
			}
		}
		return dict;
	}
	return null;
};

/* Parse "n g obj ... endobj" at byte offset pos; returns value or PDFStream */
PDFDoc.prototype.parse_indirect = function (pos, expect)
{
	var buf = this.buf;
	var lx = new Lexer(buf, pos);
	var n = lx.token(), g = lx.token(), o = lx.token();
	if (typeof n != "number" || typeof g != "number" || !(o instanceof Cmd) || o.cmd != "obj")
		throw "bad object header at " + pos;
	if (expect !== undefined && n != expect)
		throw "object number mismatch";
	var v = parse_value(lx, lx.token());
	if (this.crypt && this.crypt.str != "none")
		v = this.decrypt_strings(v, n, g);
	if (isDict(v)) {
		var save = lx.pos;
		var t = lx.token();
		if ((t instanceof Cmd) && t.cmd == "stream") {
			var p = lx.pos;
			if (buf[p] == 13 && buf[p + 1] == 10)
				p += 2;
			else if (buf[p] == 10 || buf[p] == 13)
				p++;
			var len = this.resolve(v.Length);
			var ok = false;
			if (typeof len == "number" && len >= 0 && p + len <= buf.length) {
				var q = p + len;
				while (q < buf.length && LEX_CLASS[buf[q]] == 1)
					q++;
				if (buf[q] == 0x65 && find_bytes(buf, "endstream", q, q + 9) == q)
					ok = true;
			}
			if (!ok) {
				var e = find_bytes(buf, "endstream", p);
				if (e < 0)
					e = buf.length;
				len = e - p;
				/* strip a single EOL before endstream */
				if (len > 0 && buf[p + len - 1] == 10)
					len--;
				if (len > 0 && buf[p + len - 1] == 13)
					len--;
			}
			var st = new PDFStream(this, v, p, len);
			st.num = n;
			if (this.crypt && this.crypt.stm != "none" && !isName(v.Type, "XRef"))
				st.crypt = { num: n, gen: g };
			return st;
		}
		lx.pos = save;
	}
	return v;
};

PDFDoc.prototype.get_object = function (num)
{
	if (this.cache.hasOwnProperty(num))
		return this.cache[num];
	var e = this.xref[num];
	var v = null;
	if (e && e.t == 1) {
		try {
			v = this.parse_indirect(e.off, num);
		}
		catch (err) {
			if (!this.rebuilt) {
				this.rebuild();
				return this.get_object(num);
			}
			v = null;
		}
	}
	else if (e && e.t == 2) {
		var stm = this.get_object(e.stm);
		if (stm instanceof PDFStream) {
			if (!stm.objs)
				this.load_objstm(stm);
			v = stm.objs[num] === undefined ? null : stm.objs[num];
		}
	}
	this.cache[num] = v;
	return v;
};

PDFDoc.prototype.load_objstm = function (stm)
{
	var objs = {};
	stm.objs = objs;
	var data = stm.bytes();
	var n = this.resolve(stm.dict.N) || 0;
	var first = this.resolve(stm.dict.First) || 0;
	var lx = new Lexer(data, 0);
	var nums = [], offs = [];
	for (var i = 0; i < n; i++) {
		var a = lx.token(), b = lx.token();
		if (typeof a != "number" || typeof b != "number")
			break;
		nums.push(a);
		offs.push(b);
	}
	for (var k = 0; k < nums.length; k++) {
		var l2 = new Lexer(data, first + offs[k]);
		objs[nums[k]] = parse_value(l2, l2.token());
	}
};

/* Rebuild the cross reference by scanning the whole file for "n g obj" */
PDFDoc.prototype.rebuild = function ()
{
	this.rebuilt = true;
	var buf = this.buf;
	this.xref = {};
	this.cache = {};
	var n = buf.length;
	var trailers = [];
	var i = 0;
	while (i < n) {
		var c = buf[i];
		/* look for "obj" keyword: digits ws digits ws obj */
		if (c == 0x6f && buf[i + 1] == 0x62 && buf[i + 2] == 0x6a && (i + 3 >= n || LEX_CLASS[buf[i + 3]])
			&& i > 4 && LEX_CLASS[buf[i - 1]] == 1) {
			var j = i - 1;
			while (j > 0 && LEX_CLASS[buf[j]] == 1)
				j--;
			var ge = j + 1;
			while (j >= 0 && buf[j] >= 0x30 && buf[j] <= 0x39)
				j--;
			var gs = j + 1;
			if (gs < ge && j >= 0 && LEX_CLASS[buf[j]] == 1) {
				while (j > 0 && LEX_CLASS[buf[j]] == 1)
					j--;
				var ne = j + 1;
				while (j >= 0 && buf[j] >= 0x30 && buf[j] <= 0x39)
					j--;
				var ns = j + 1;
				if (ns < ne) {
					var num = parseInt(bytes_to_string(buf, ns, ne), 10);
					this.xref[num] = { t: 1, off: ns, gen: parseInt(bytes_to_string(buf, gs, ge), 10) };
				}
			}
			i += 3;
			continue;
		}
		if (c == 0x74 && buf[i + 1] == 0x72 && find_bytes(buf, "trailer", i, i + 7) == i) {
			var lx = new Lexer(buf, i + 7);
			try {
				var d = parse_value(lx, lx.token());
				if (isDict(d))
					trailers.push(d);
			}
			catch (e) { }
			i += 7;
			continue;
		}
		i++;
	}
	var trailer = null;
	for (i = trailers.length - 1; i >= 0; i--) {
		if (trailers[i].Root !== undefined) {
			trailer = trailers[i];
			break;
		}
	}
	if (trailer && trailer.Encrypt !== undefined) {
		this.trailer = trailer;
		this.setup_encryption();
		this.cache = {};
	}
	/* object streams and xref streams */
	var keys = [];
	for (var k in this.xref)
		keys.push(parseInt(k, 10));
	var catalog = null;
	for (i = 0; i < keys.length; i++) {
		var o;
		try {
			o = this.get_object(keys[i]);
		}
		catch (e2) {
			continue;
		}
		if (o instanceof PDFStream) {
			var t = this.resolve(o.dict.Type);
			if (isName(t, "ObjStm")) {
				try {
					this.load_objstm(o);
					for (var on in o.objs) {
						if (this.xref[on] === undefined || this.xref[on].t != 1)
							this.xref[on] = { t: 2, stm: keys[i], idx: 0 };
						this.cache[on] = o.objs[on];
					}
				}
				catch (e3) { }
			}
			else if (isName(t, "XRef") && !trailer && o.dict.Root !== undefined)
				trailer = o.dict;
		}
		else if (isDict(o) && isName(o.Type, "Catalog"))
			catalog = new Ref(keys[i], 0);
	}
	if (!trailer && catalog)
		trailer = { Root: catalog };
	else if (!trailer) {
		for (var kk in this.cache) {
			var oo = this.cache[kk];
			if (isDict(oo) && isName(oo.Type, "Catalog")) {
				trailer = { Root: new Ref(parseInt(kk, 10), 0) };
				break;
			}
		}
	}
	this.trailer = trailer;
	if (trailer && trailer.Encrypt !== undefined && !this.crypt && !this.crypt_error) {
		this.setup_encryption();
		this.cache = {};
	}
};

function rect_of(doc, v)
{
	v = doc.resolve(v);
	if (!(v instanceof Array) || v.length < 4)
		return null;
	var r = [];
	for (var i = 0; i < 4; i++) {
		r[i] = doc.resolve(v[i]);
		if (typeof r[i] != "number")
			return null;
	}
	return [Math.min(r[0], r[2]), Math.min(r[1], r[3]), Math.max(r[0], r[2]), Math.max(r[1], r[3])];
}

/* Flattened page list: [{ dict, res, box:[x0,y0,x1,y1], rotate }] */
PDFDoc.prototype.get_pages = function ()
{
	if (this.pages)
		return this.pages;
	var pages = [];
	var doc = this;
	var visited = {};
	var root = this.resolve(this.trailer.Root);
	function walk(node, inh, depth)
	{
		var ref = node;
		node = doc.resolve(node);
		if (!isDict(node) || depth > 64)
			return;
		if (ref instanceof Ref) {
			if (visited[ref.num])
				return;
			visited[ref.num] = true;
		}
		var my = { Resources: inh.Resources, MediaBox: inh.MediaBox, CropBox: inh.CropBox, Rotate: inh.Rotate };
		var keys = ["Resources", "MediaBox", "CropBox", "Rotate"];
		for (var i = 0; i < keys.length; i++)
			if (node[keys[i]] !== undefined)
				my[keys[i]] = node[keys[i]];
		var kids = doc.resolve(node.Kids);
		if (kids instanceof Array && !isName(node.Type, "Page")) {
			for (var k = 0; k < kids.length; k++)
				walk(kids[k], my, depth + 1);
			return;
		}
		var box = rect_of(doc, my.MediaBox) || [0, 0, 612, 792];
		var crop = rect_of(doc, my.CropBox);
		if (crop) {
			var x0 = Math.max(box[0], crop[0]), y0 = Math.max(box[1], crop[1]);
			var x1 = Math.min(box[2], crop[2]), y1 = Math.min(box[3], crop[3]);
			if (x1 > x0 && y1 > y0)
				box = [x0, y0, x1, y1];
		}
		var rot = doc.resolve(my.Rotate) || 0;
		rot = ((Math.round(rot / 90) % 4) + 4) % 4 * 90;
		pages.push({ dict: node, res: doc.resolve(my.Resources) || {}, box: box, rotate: rot,
			num: ref instanceof Ref ? ref.num : -1 });
	}
	walk(root.Pages, {}, 0);
	this.pages = pages;
	return pages;
};

/* ======================================================================= */
/* Part 2: functions, colour spaces, fonts and encodings                    */
/* ======================================================================= */

function num_array(doc, v)
{
	v = doc.resolve(v);
	if (!(v instanceof Array))
		return null;
	var r = [];
	for (var i = 0; i < v.length; i++) {
		var n = doc.resolve(v[i]);
		r.push(typeof n == "number" ? n : 0);
	}
	return r;
}

function clampn(v, lo, hi)
{
	return v < lo ? lo : (v > hi ? hi : v);
}

/* ----- PostScript calculator (type 4) functions ----- */

function ps_parse(lx)
{
	var code = [];
	for (;;) {
		var t = lx.token();
		if (t === EOF_TOKEN)
			break;
		if (t instanceof Cmd) {
			if (t.cmd == "}")
				break;
			if (t.cmd == "{")
				code.push(ps_parse(lx));
			else
				code.push(t.cmd);
		}
		else if (typeof t == "number")
			code.push(t);
	}
	return code;
}

function ps_exec(code, st)
{
	var a, b, c;
	for (var i = 0; i < code.length; i++) {
		var it = code[i];
		if (typeof it == "number") {
			st.push(it);
			continue;
		}
		if (it instanceof Array) {
			if (code[i + 1] === "if") {
				c = st.pop();
				if (c)
					ps_exec(it, st);
				i++;
			}
			else if ((code[i + 1] instanceof Array) && code[i + 2] === "ifelse") {
				c = st.pop();
				ps_exec(c ? it : code[i + 1], st);
				i += 2;
			}
			continue;
		}
		switch (it) {
			case "add": b = st.pop(); a = st.pop(); st.push(a + b); break;
			case "sub": b = st.pop(); a = st.pop(); st.push(a - b); break;
			case "mul": b = st.pop(); a = st.pop(); st.push(a * b); break;
			case "div": b = st.pop(); a = st.pop(); st.push(b ? a / b : 0); break;
			case "idiv": b = st.pop(); a = st.pop(); st.push(b ? Math.floor(a / b) : 0); break;
			case "mod": b = st.pop(); a = st.pop(); st.push(b ? a % b : 0); break;
			case "neg": st.push(-st.pop()); break;
			case "abs": st.push(Math.abs(st.pop())); break;
			case "ceiling": st.push(Math.ceil(st.pop())); break;
			case "floor": st.push(Math.floor(st.pop())); break;
			case "round": st.push(Math.floor(st.pop() + 0.5)); break;
			case "truncate": case "cvi": a = st.pop(); st.push(a < 0 ? Math.ceil(a) : Math.floor(a)); break;
			case "cvr": break;
			case "sqrt": st.push(Math.sqrt(Math.max(0, st.pop()))); break;
			case "sin": st.push(Math.sin(st.pop() * Math.PI / 180)); break;
			case "cos": st.push(Math.cos(st.pop() * Math.PI / 180)); break;
			case "atan":
				b = st.pop(); a = st.pop();
				c = Math.atan2(a, b) * 180 / Math.PI;
				st.push(c < 0 ? c + 360 : c);
				break;
			case "exp": b = st.pop(); a = st.pop(); st.push(Math.pow(a, b)); break;
			case "ln": st.push(Math.log(st.pop())); break;
			case "log": st.push(Math.log(st.pop()) / Math.LN10); break;
			case "eq": b = st.pop(); a = st.pop(); st.push(a == b); break;
			case "ne": b = st.pop(); a = st.pop(); st.push(a != b); break;
			case "gt": b = st.pop(); a = st.pop(); st.push(a > b); break;
			case "ge": b = st.pop(); a = st.pop(); st.push(a >= b); break;
			case "lt": b = st.pop(); a = st.pop(); st.push(a < b); break;
			case "le": b = st.pop(); a = st.pop(); st.push(a <= b); break;
			case "and":
				b = st.pop(); a = st.pop();
				st.push(typeof a == "boolean" ? (a && b) : (a & b));
				break;
			case "or":
				b = st.pop(); a = st.pop();
				st.push(typeof a == "boolean" ? (a || b) : (a | b));
				break;
			case "xor":
				b = st.pop(); a = st.pop();
				st.push(typeof a == "boolean" ? (a != b) : (a ^ b));
				break;
			case "not":
				a = st.pop();
				st.push(typeof a == "boolean" ? !a : ~a);
				break;
			case "bitshift":
				b = st.pop(); a = st.pop();
				st.push(b >= 0 ? a << b : a >> -b);
				break;
			case "true": st.push(true); break;
			case "false": st.push(false); break;
			case "pop": st.pop(); break;
			case "exch": b = st.pop(); a = st.pop(); st.push(b); st.push(a); break;
			case "dup": a = st.pop(); st.push(a); st.push(a); break;
			case "copy":
				a = st.pop();
				var base = st.length - a;
				for (var k = 0; k < a; k++)
					st.push(st[base + k]);
				break;
			case "index": a = st.pop(); st.push(st[st.length - 1 - a]); break;
			case "roll":
				var j = st.pop();
				var n = st.pop();
				if (n > 0 && n <= st.length) {
					var seg = st.splice(st.length - n, n);
					j = ((j % n) + n) % n;
					for (var q = 0; q < n; q++)
						st.push(seg[(q - j + n) % n]);
				}
				break;
		}
	}
}

/* ----- Function objects: { call(inputs[]) -> outputs[] } ----- */

function make_function(doc, fobj)
{
	fobj = doc.resolve(fobj);
	var i;
	if (fobj instanceof Array) {
		var fs = [];
		for (i = 0; i < fobj.length; i++)
			fs.push(make_function(doc, fobj[i]));
		return {
			call: function (x) {
				var o = [];
				for (var j = 0; j < fs.length; j++)
					o.push(fs[j].call(x)[0]);
				return o;
			}
		};
	}
	if (fobj && fobj._fn)
		return fobj._fn;
	var dict = (fobj instanceof PDFStream) ? fobj.dict : fobj;
	if (!isDict(dict))
		return { call: function (x) { return [0]; } };
	var type = doc.resolve(dict.FunctionType);
	var domain = num_array(doc, dict.Domain) || [0, 1];
	var range = num_array(doc, dict.Range);
	var fn = null;
	if (type == 2) {
		var c0 = num_array(doc, dict.C0) || [0];
		var c1 = num_array(doc, dict.C1) || [1];
		var N = doc.resolve(dict.N);
		fn = {
			call: function (x) {
				var t = clampn(x[0], domain[0], domain[1]);
				var p = N == 1 ? t : Math.pow(t, N);
				var o = [];
				for (var j = 0; j < c0.length; j++)
					o.push(c0[j] + p * ((c1[j] === undefined ? c0[j] : c1[j]) - c0[j]));
				return o;
			}
		};
	}
	else if (type == 3) {
		var subs = doc.resolve(dict.Functions) || [];
		var funcs = [];
		for (i = 0; i < subs.length; i++)
			funcs.push(make_function(doc, subs[i]));
		var bounds = num_array(doc, dict.Bounds) || [];
		var enc = num_array(doc, dict.Encode) || [];
		fn = {
			call: function (x) {
				var t = clampn(x[0], domain[0], domain[1]);
				var k = 0;
				while (k < bounds.length && t >= bounds[k])
					k++;
				if (k >= funcs.length)
					k = funcs.length - 1;
				var lo = k == 0 ? domain[0] : bounds[k - 1];
				var hi = k >= bounds.length ? domain[1] : bounds[k];
				var e0 = enc[2 * k] === undefined ? 0 : enc[2 * k];
				var e1 = enc[2 * k + 1] === undefined ? 1 : enc[2 * k + 1];
				var v = hi == lo ? e0 : e0 + (t - lo) * (e1 - e0) / (hi - lo);
				return funcs[k].call([v]);
			}
		};
	}
	else if (type == 0 && fobj instanceof PDFStream) {
		var size = num_array(doc, dict.Size) || [2];
		var bps = doc.resolve(dict.BitsPerSample) || 8;
		var m = size.length;
		var nout = range ? range.length / 2 : 1;
		var encode = num_array(doc, dict.Encode);
		if (!encode) {
			encode = [];
			for (i = 0; i < m; i++)
				encode.push(0, size[i] - 1);
		}
		var decode = num_array(doc, dict.Decode) || range;
		var data = fobj.bytes();
		var maxv = Math.pow(2, bps) - 1;
		var getsample = function (idx, o) {
			var bitpos = (idx * nout + o) * bps;
			var v = 0;
			if (bps == 8)
				return data[bitpos >> 3] || 0;
			if (bps == 16)
				return ((data[bitpos >> 3] << 8) | data[(bitpos >> 3) + 1]) || 0;
			for (var b = 0; b < bps; b++) {
				var bp = bitpos + b;
				v = (v << 1) | ((data[bp >> 3] >> (7 - (bp & 7))) & 1);
			}
			return v;
		};
		fn = {
			call: function (x) {
				var idx = [], fr = [];
				var j;
				for (j = 0; j < m; j++) {
					var t = clampn(x[j], domain[2 * j], domain[2 * j + 1]);
					var e = domain[2 * j + 1] == domain[2 * j] ? 0 :
						encode[2 * j] + (t - domain[2 * j]) * (encode[2 * j + 1] - encode[2 * j]) / (domain[2 * j + 1] - domain[2 * j]);
					e = clampn(e, 0, size[j] - 1);
					idx.push(Math.floor(e));
					fr.push(e - Math.floor(e));
				}
				var o = [];
				for (var oo = 0; oo < nout; oo++) {
					var v;
					var stride = 1, off0 = 0, off1 = 0;
					if (m == 1) {
						var i1 = Math.min(idx[0] + 1, size[0] - 1);
						v = getsample(idx[0], oo) * (1 - fr[0]) + getsample(i1, oo) * fr[0];
					}
					else {
						for (j = 0; j < m; j++) {
							var ij = fr[j] >= 0.5 ? Math.min(idx[j] + 1, size[j] - 1) : idx[j];
							off0 += ij * stride;
							stride *= size[j];
						}
						v = getsample(off0, oo);
					}
					v = decode[2 * oo] + v * (decode[2 * oo + 1] - decode[2 * oo]) / maxv;
					if (range)
						v = clampn(v, range[2 * oo], range[2 * oo + 1]);
					o.push(v);
				}
				return o;
			}
		};
	}
	else if (type == 4 && fobj instanceof PDFStream) {
		var lx = new Lexer(fobj.bytes(), 0);
		var t0 = lx.token();
		var prog = (t0 instanceof Cmd && t0.cmd == "{") ? ps_parse(lx) : [];
		fn = {
			call: function (x) {
				var st = [];
				for (var j = 0; j < x.length; j++)
					st.push(clampn(x[j], domain[2 * j], domain[2 * j + 1]));
				try {
					ps_exec(prog, st);
				}
				catch (e) { }
				var n = range ? range.length / 2 : st.length;
				var out = st.slice(Math.max(0, st.length - n));
				for (j = 0; j < out.length; j++) {
					if (typeof out[j] != "number")
						out[j] = out[j] ? 1 : 0;
					if (range)
						out[j] = clampn(out[j], range[2 * j], range[2 * j + 1]);
				}
				return out;
			}
		};
	}
	if (!fn)
		fn = { call: function (x) { return [x[0] === undefined ? 0 : x[0]]; } };
	if (fobj && typeof fobj == "object")
		fobj._fn = fn;
	return fn;
}

/* ----- Colour spaces ----- */

function c255(v)
{
	return v <= 0 ? 0 : (v >= 255 ? 255 : Math.floor(v + 0.5));
}

var CS_GRAY = {
	n: 1, init: [0],
	rgb: function (c) { var g = c255(c[0] * 255); return [g, g, g]; }
};
var CS_RGB = {
	n: 3, init: [0, 0, 0],
	rgb: function (c) { return [c255(c[0] * 255), c255(c[1] * 255), c255(c[2] * 255)]; }
};
var CS_CMYK = {
	n: 4, init: [0, 0, 0, 1],
	rgb: function (c) {
		var k = 1 - clampn(c[3], 0, 1);
		return [c255(255 * (1 - clampn(c[0], 0, 1)) * k), c255(255 * (1 - clampn(c[1], 0, 1)) * k), c255(255 * (1 - clampn(c[2], 0, 1)) * k)];
	}
};
var CS_PATTERN = { n: 0, init: [], pattern: true, rgb: function (c) { return [176, 176, 176]; } };

function lab_to_rgb(L, a, b, wp)
{
	var fy = (L + 16) / 116, fx = fy + a / 500, fz = fy - b / 200;
	function g(t) { return t > 6 / 29 ? t * t * t : 3 * (6 / 29) * (6 / 29) * (t - 4 / 29); }
	var X = wp[0] * g(fx), Y = wp[1] * g(fy), Z = wp[2] * g(fz);
	var r = 3.1339 * X - 1.6169 * Y - 0.4906 * Z;
	var gg = -0.9785 * X + 1.9160 * Y + 0.0333 * Z;
	var bb = 0.0720 * X - 0.2290 * Y + 1.4057 * Z;
	function gam(u) {
		u = clampn(u, 0, 1);
		return u <= 0.0031308 ? 12.92 * u : 1.055 * Math.pow(u, 1 / 2.4) - 0.055;
	}
	return [c255(gam(r) * 255), c255(gam(gg) * 255), c255(gam(bb) * 255)];
}

function make_cs(doc, obj, res)
{
	var o = doc.resolve(obj);
	var i;
	if (o instanceof Name) {
		switch (o.name) {
			case "DeviceGray": case "G": case "CalGray": return CS_GRAY;
			case "DeviceRGB": case "RGB": case "CalRGB": return CS_RGB;
			case "DeviceCMYK": case "CMYK": return CS_CMYK;
			case "Pattern": return CS_PATTERN;
		}
		if (res && isDict(res)) {
			var csr = doc.resolve(res.ColorSpace);
			if (isDict(csr) && csr[o.name] !== undefined) {
				var target = doc.resolve(csr[o.name]);
				if (!(target instanceof Name && target.name == o.name))
					return make_cs(doc, csr[o.name], null);
			}
		}
		return CS_GRAY;
	}
	if (!(o instanceof Array) || o.length == 0)
		return CS_GRAY;
	if (o._cs)
		return o._cs;
	var kind = doc.resolve(o[0]);
	var cs = CS_GRAY;
	if (!(kind instanceof Name))
		return cs;
	switch (kind.name) {
		case "DeviceGray": case "G": case "CalGray": cs = CS_GRAY; break;
		case "DeviceRGB": case "RGB": case "CalRGB": cs = CS_RGB; break;
		case "DeviceCMYK": case "CMYK": cs = CS_CMYK; break;
		case "ICCBased": {
			var st = doc.resolve(o[1]);
			var n = (st instanceof PDFStream) ? doc.resolve(st.dict.N) : 3;
			cs = n == 1 ? CS_GRAY : (n == 4 ? CS_CMYK : CS_RGB);
			break;
		}
		case "Lab": {
			var ld = doc.resolve(o[1]);
			var wp = (isDict(ld) && num_array(doc, ld.WhitePoint)) || [0.9505, 1, 1.089];
			var rg = (isDict(ld) && num_array(doc, ld.Range)) || [-100, 100, -100, 100];
			cs = {
				n: 3, init: [0, 0, 0], lab: true,
				rgb: function (c) {
					return lab_to_rgb(clampn(c[0], 0, 100), clampn(c[1], rg[0], rg[1]), clampn(c[2], rg[2], rg[3]), wp);
				}
			};
			break;
		}
		case "Indexed": case "I": {
			var base = make_cs(doc, o[1], res);
			var hival = doc.resolve(o[2]) || 0;
			var lk = doc.resolve(o[3]);
			var bytes;
			if (lk instanceof PDFStream)
				bytes = lk.bytes();
			else if (typeof lk == "string")
				bytes = string_to_bytes(lk);
			else
				bytes = new Uint8Array(0);
			var pal = [];
			cs = {
				n: 1, init: [0], indexed: true,
				rgb: function (c) {
					var ix = clampn(Math.round(c[0]), 0, hival);
					if (pal[ix])
						return pal[ix];
					var comps = [];
					for (var k = 0; k < base.n; k++)
						comps.push((bytes[ix * base.n + k] || 0) / 255);
					if (base.lab)
						comps = [comps[0] * 100, comps[1] * 255 - 128, comps[2] * 255 - 128];
					pal[ix] = base.rgb(comps);
					return pal[ix];
				}
			};
			break;
		}
		case "Separation": case "DeviceN": {
			var names = doc.resolve(o[1]);
			var nn = kind.name == "Separation" ? 1 : (names instanceof Array ? names.length : 1);
			var alt = make_cs(doc, o[2], res);
			var fn = make_function(doc, o[3]);
			var none = false;
			if (kind.name == "Separation" && isName(names, "None"))
				none = true;
			var init = [];
			for (i = 0; i < nn; i++)
				init.push(1);
			cs = {
				n: nn, init: init, none: none,
				rgb: function (c) { return alt.rgb(fn.call(c)); }
			};
			break;
		}
		case "Pattern":
			cs = { n: 0, init: [], pattern: true, base: o.length > 1 ? make_cs(doc, o[1], res) : null, rgb: CS_PATTERN.rgb };
			break;
	}
	o._cs = cs;
	return cs;
}

/* ----- Encodings ----- */

var CP437_HIGH = "ÇüéâäàåçêëèïîìÄÅÉæÆôöòûùÿÖÜ¢£¥₧ƒáíóúñÑªº¿⌐¬½¼¡«»░▒▓│┤╡╢╖╕╣║╗╝╜╛┐└┴┬├─┼╞╟╚╔╩╦╠═╬╧╨╤╥╙╘╒╓╫╪┘┌█▄▌▐▀αßΓπΣσµτΦΘΩδ∞φε∩≡±≥≤⌠⌡÷≈°∙·√ⁿ²■ ";
var MACROMAN_HIGH = "ÄÅÇÉÑÖÜáàâäãåçéèêëíìîïñóòôöõúùûü†°¢£§•¶ß®©™´¨≠ÆØ∞±≤≥¥µ∂∑∏π∫ªºΩæø¿¡¬√ƒ≈∆«»… ÀÃÕŒœ–—“”‘’÷◊ÿŸ⁄€‹›ﬁﬂ‡·‚„‰ÂÊÁËÈÍÎÏÌÓÔÒÚÛÙıˆ˜¯˘˙˚¸˝˛ˇ";
var WINANSI_80 = [0x20ac, 0, 0x201a, 0x0192, 0x201e, 0x2026, 0x2020, 0x2021, 0x02c6, 0x2030, 0x0160, 0x2039, 0x0152, 0, 0x017d, 0,
	0, 0x2018, 0x2019, 0x201c, 0x201d, 0x2022, 0x2013, 0x2014, 0x02dc, 0x2122, 0x0161, 0x203a, 0x0153, 0, 0x017e, 0x0178];
var STANDARD_HIGH = { 0xa1: 0xa1, 0xa2: 0xa2, 0xa3: 0xa3, 0xa4: 0x2044, 0xa5: 0xa5, 0xa6: 0x192, 0xa7: 0xa7, 0xa8: 0xa4, 0xa9: 0x27,
	0xaa: 0x201c, 0xab: 0xab, 0xae: 0xfb01, 0xaf: 0xfb02, 0xb1: 0x2013, 0xb2: 0x2020, 0xb3: 0x2021, 0xb4: 0xb7, 0xb6: 0xb6,
	0xb7: 0x2022, 0xb8: 0x201a, 0xb9: 0x201e, 0xba: 0x201d, 0xbb: 0xbb, 0xbc: 0x2026, 0xbd: 0x2030, 0xbf: 0xbf, 0xd0: 0x2014 };

var CP437_REV = {};
(function () {
	for (var i = 0; i < 128; i++)
		CP437_REV[CP437_HIGH.charCodeAt(i)] = 128 + i;
})();

/* Latin-1 supplement folding to the nearest ASCII letter (U+00C0 - U+00FF) */
var LATIN1_FOLD = "AAAAAAACEEEEIIIIDNOOOOOxOUUUUYPsaaaaaaaceeeeiiiidnooooo/ouuuuypy";

function build_glyph_names()
{
	var t = {};
	var i;
	var ascii = "space exclam quotedbl numbersign dollar percent ampersand quotesingle parenleft parenright asterisk plus comma hyphen period slash " +
		"zero one two three four five six seven eight nine colon semicolon less equal greater question at " +
		"A B C D E F G H I J K L M N O P Q R S T U V W X Y Z bracketleft backslash bracketright asciicircum underscore grave " +
		"a b c d e f g h i j k l m n o p q r s t u v w x y z braceleft bar braceright asciitilde";
	var names = ascii.split(" ");
	for (i = 0; i < names.length; i++)
		t[names[i]] = String.fromCharCode(32 + i);
	var hi = "nbspace exclamdown cent sterling currency yen brokenbar section dieresis copyright ordfeminine guillemotleft logicalnot sfthyphen registered macron " +
		"degree plusminus twosuperior threesuperior acute mu paragraph periodcentered cedilla onesuperior ordmasculine guillemotright onequarter onehalf threequarters questiondown " +
		"Agrave Aacute Acircumflex Atilde Adieresis Aring AE Ccedilla Egrave Eacute Ecircumflex Edieresis Igrave Iacute Icircumflex Idieresis " +
		"Eth Ntilde Ograve Oacute Ocircumflex Otilde Odieresis multiply Oslash Ugrave Uacute Ucircumflex Udieresis Yacute Thorn germandbls " +
		"agrave aacute acircumflex atilde adieresis aring ae ccedilla egrave eacute ecircumflex edieresis igrave iacute icircumflex idieresis " +
		"eth ntilde ograve oacute ocircumflex otilde odieresis divide oslash ugrave uacute ucircumflex udieresis yacute thorn ydieresis";
	names = hi.split(" ");
	for (i = 0; i < names.length; i++)
		t[names[i]] = String.fromCharCode(0xa0 + i);
	var extra = {
		quoteright: 0x2019, quoteleft: 0x2018, quotedblleft: 0x201c, quotedblright: 0x201d, quotesinglbase: 0x201a, quotedblbase: 0x201e,
		endash: 0x2013, emdash: 0x2014, bullet: 0x2022, ellipsis: 0x2026, minus: 0x2212, trademark: 0x2122, dagger: 0x2020, daggerdbl: 0x2021,
		Euro: 0x20ac, florin: 0x192, perthousand: 0x2030, guilsinglleft: 0x2039, guilsinglright: 0x203a, OE: 0x152, oe: 0x153,
		Scaron: 0x160, scaron: 0x161, Zcaron: 0x17d, zcaron: 0x17e, Ydieresis: 0x178, circumflex: 0x2c6, tilde: 0x2dc, dotlessi: 0x131,
		fraction: 0x2044, Lslash: 0x141, lslash: 0x142, alpha: 0x3b1, beta: 0x3b2, gamma: 0x3b3, delta: 0x3b4, pi: 0x3c0,
		sigma: 0x3c3, Omega: 0x3a9, infinity: 0x221e, notequal: 0x2260, lessequal: 0x2264, greaterequal: 0x2265,
		summation: 0x2211, radical: 0x221a, approxequal: 0x2248, middot: 0xb7, hyphenminus: 0x2d,
		arrowright: 0x2192, arrowleft: 0x2190
	};
	for (var k in extra)
		if (extra[k])
			t[k] = String.fromCharCode(extra[k]);
	t.fi = "fi";
	t.fl = "fl";
	t.ff = "ff";
	t.ffi = "ffi";
	t.ffl = "ffl";
	t.ellipsis = "…";
	return t;
}
var GLYPH_NAMES = build_glyph_names();

function glyph_to_unicode(name)
{
	if (GLYPH_NAMES.hasOwnProperty(name))
		return GLYPH_NAMES[name];
	var dot = name.indexOf(".");
	if (dot > 0)
		return glyph_to_unicode(name.substr(0, dot));
	var m = /^uni([0-9A-Fa-f]{4})/.exec(name);
	if (m)
		return String.fromCharCode(parseInt(m[1], 16));
	m = /^u([0-9A-Fa-f]{4,6})$/.exec(name);
	if (m) {
		var cp = parseInt(m[1], 16);
		return cp < 0x10000 ? String.fromCharCode(cp) : "?";
	}
	return null;
}

/* Build a 256-entry code -> string table for a base encoding name */
function base_encoding(name)
{
	var t = [];
	var i;
	for (i = 0; i < 256; i++)
		t[i] = null;
	for (i = 32; i < 127; i++)
		t[i] = String.fromCharCode(i);
	if (name == "MacRomanEncoding") {
		for (i = 0; i < 128; i++)
			t[128 + i] = String.fromCharCode(MACROMAN_HIGH.charCodeAt(i));
	}
	else if (name == "StandardEncoding") {
		t[0x27] = "’";
		t[0x60] = "‘";
		for (var k in STANDARD_HIGH)
			t[parseInt(k, 10)] = String.fromCharCode(STANDARD_HIGH[k]);
	}
	else {	/* WinAnsi and everything else */
		for (i = 0xa0; i < 256; i++)
			t[i] = String.fromCharCode(i);
		for (i = 0; i < 32; i++)
			if (WINANSI_80[i])
				t[0x80 + i] = String.fromCharCode(WINANSI_80[i]);
		t[0x7f] = "•";
		t[0xad] = "-";
	}
	return t;
}

/* Parse a ToUnicode CMap stream into { code: string } */
function parse_tounicode(doc, stm)
{
	var map = {};
	var data;
	try {
		data = stm.bytes();
	}
	catch (e) {
		return map;
	}
	var lx = new Lexer(data, 0);
	function code_of(s)
	{
		var v = 0;
		for (var i = 0; i < s.length; i++)
			v = v * 256 + s.charCodeAt(i);
		return v;
	}
	function ustr(s)
	{
		var r = "";
		for (var i = 0; i + 1 < s.length; i += 2)
			r += String.fromCharCode((s.charCodeAt(i) << 8) | s.charCodeAt(i + 1));
		if (s.length == 1)
			r = s;
		return r;
	}
	var prev = null;
	for (;;) {
		var t = lx.token();
		if (t === EOF_TOKEN)
			break;
		if (!(t instanceof Cmd)) {
			prev = t;
			continue;
		}
		if (t.cmd == "beginbfchar") {
			for (;;) {
				var a = lx.token();
				if (a === EOF_TOKEN || a instanceof Cmd)
					break;
				var b = lx.token();
				if (typeof a == "string" && typeof b == "string")
					map[code_of(a)] = ustr(b);
			}
		}
		else if (t.cmd == "beginbfrange") {
			for (;;) {
				var lo = lx.token();
				if (lo === EOF_TOKEN || lo instanceof Cmd)
					break;
				var hi = lx.token();
				var dst = parse_value(lx, lx.token(), true);
				if (typeof lo != "string" || typeof hi != "string")
					continue;
				var l = code_of(lo), h = code_of(hi);
				if (h - l > 65535)
					continue;
				if (typeof dst == "string") {
					var base = ustr(dst);
					for (var c = l; c <= h; c++) {
						if (base.length == 0)
							break;
						var last = base.charCodeAt(base.length - 1) + (c - l);
						map[c] = base.substr(0, base.length - 1) + String.fromCharCode(last & 0xffff);
					}
				}
				else if (dst instanceof Array) {
					for (var d = 0; d < dst.length && l + d <= h; d++)
						if (typeof dst[d] == "string")
							map[l + d] = ustr(dst[d]);
				}
			}
		}
	}
	return map;
}

/* ----- Standard font metrics (ASCII 32..126, 1000 units/em) ----- */

var WIDTHS_SANS = [278, 278, 355, 556, 556, 889, 667, 191, 333, 333, 389, 584, 278, 333, 278, 278,
	556, 556, 556, 556, 556, 556, 556, 556, 556, 556, 278, 278, 584, 584, 584, 556,
	1015, 667, 667, 722, 722, 667, 611, 778, 722, 278, 500, 667, 556, 833, 722, 778,
	667, 778, 722, 667, 611, 722, 667, 944, 667, 667, 611, 278, 278, 278, 469, 556,
	333, 556, 556, 500, 556, 556, 278, 556, 556, 222, 222, 500, 222, 833, 556, 556,
	556, 556, 333, 500, 278, 556, 500, 722, 500, 500, 500, 334, 260, 334, 584];
var WIDTHS_SERIF = [250, 333, 408, 500, 500, 833, 778, 180, 333, 333, 500, 564, 250, 333, 250, 278,
	500, 500, 500, 500, 500, 500, 500, 500, 500, 500, 278, 278, 564, 564, 564, 444,
	921, 722, 667, 667, 722, 611, 556, 722, 722, 333, 389, 722, 611, 889, 722, 722,
	556, 722, 667, 556, 611, 722, 722, 944, 722, 722, 611, 333, 278, 333, 469, 500,
	333, 444, 500, 444, 500, 444, 333, 500, 500, 278, 278, 500, 278, 778, 500, 500,
	500, 500, 333, 389, 278, 500, 500, 722, 500, 500, 444, 480, 200, 480, 541];

function std_char_width(family, bold, ch)
{
	var u = ch.charCodeAt(0);
	var w;
	if (family == 2)
		return 600;
	var tab = family == 1 ? WIDTHS_SERIF : WIDTHS_SANS;
	if (u >= 32 && u < 127)
		w = tab[u - 32];
	else if (u == 0x2019 || u == 0x2018)
		w = family == 1 ? 333 : 222;
	else if (u == 0x201c || u == 0x201d)
		w = family == 1 ? 444 : 333;
	else if (u == 0x2013)
		w = 556;
	else if (u == 0x2014 || u == 0x2026)
		w = 1000;
	else if (u == 0x2022)
		w = 350;
	else
		w = family == 1 ? 500 : 556;
	return bold ? w * 1.05 : w;
}

/* ----- Font objects ----- */

var FONT_UID = 0;

function make_font(doc, fobj, res)
{
	var fdict = doc.resolve(fobj);
	if (!isDict(fdict))
		return null;
	if (fdict._font)
		return fdict._font;
	var font = {
		kind: "simple", family: 0, bold: false, italic: false, uni: null, tounicode: null,
		widths: null, first: 0, missing: 0, dw: 1000, cidw: null, twobyte: false, mono: false,
		type3: null, ucs2: false, name: "", desc: null, symbolic: false, hasencoding: false, glyphnames: null,
		cidtogid: null, uid: ++FONT_UID, prog: undefined, ol: null
	};
	var subtype = doc.resolve(fdict.Subtype);
	var base = doc.resolve(fdict.BaseFont);
	var bname = (base instanceof Name) ? base.name : "";
	bname = bname.replace(/^[A-Z]{6}\+/, "");
	font.name = bname;
	var desc = null;
	var i;

	if (isName(subtype, "Type0")) {
		font.kind = "type0";
		font.twobyte = true;
		var df = doc.resolve(fdict.DescendantFonts);
		var cf = (df instanceof Array) ? doc.resolve(df[0]) : doc.resolve(df);
		if (isDict(cf)) {
			desc = doc.resolve(cf.FontDescriptor);
			var c2g = doc.resolve(cf.CIDToGIDMap);
			if (c2g instanceof PDFStream)
				font.cidtogid = c2g.bytes();
			font.dw = doc.resolve(cf.DW);
			if (typeof font.dw != "number")
				font.dw = 1000;
			var w = doc.resolve(cf.W);
			font.cidw = {};
			if (w instanceof Array) {
				for (i = 0; i < w.length;) {
					var c1 = doc.resolve(w[i]);
					var second = doc.resolve(w[i + 1]);
					if (second instanceof Array) {
						for (var j = 0; j < second.length; j++)
							font.cidw[c1 + j] = doc.resolve(second[j]);
						i += 2;
					}
					else {
						var wv = doc.resolve(w[i + 2]);
						if (typeof second == "number" && second - c1 < 70000)
							for (var c = c1; c <= second; c++)
								font.cidw[c] = wv;
						i += 3;
					}
				}
			}
			if (!bname) {
				var bn = doc.resolve(cf.BaseFont);
				if (bn instanceof Name)
					font.name = bn.name.replace(/^[A-Z]{6}\+/, "");
			}
		}
		var enc = doc.resolve(fdict.Encoding);
		if (isName(enc) && /UCS2|UTF16/.test(enc.name))
			font.ucs2 = true;
	}
	else if (isName(subtype, "Type3")) {
		font.kind = "type3";
		var fm = num_array(doc, fdict.FontMatrix) || [0.001, 0, 0, 0.001, 0, 0];
		font.type3 = {
			matrix: fm,
			procs: doc.resolve(fdict.CharProcs) || {},
			res: doc.resolve(fdict.Resources) || res,
			names: []
		};
	}
	else
		desc = doc.resolve(fdict.FontDescriptor);

	/* style */
	font.desc = desc;
	font.hasencoding = fdict.Encoding !== undefined;
	var lname = font.name.toLowerCase();
	var flags = 0;
	if (isDict(desc)) {
		flags = doc.resolve(desc.Flags) || 0;
		font.symbolic = (flags & 4) != 0 && (flags & 32) == 0;
		var ia = doc.resolve(desc.ItalicAngle);
		if (typeof ia == "number" && ia != 0)
			font.italic = true;
		var fw = doc.resolve(desc.FontWeight);
		if (typeof fw == "number" && fw >= 600)
			font.bold = true;
		if ((flags & 0x40000))
			font.bold = true;
		if (flags & 0x40)
			font.italic = true;
		if (flags & 1)
			font.mono = true;
		else if (flags & 2)
			font.family = 1;
		var mw = doc.resolve(desc.MissingWidth);
		if (typeof mw == "number")
			font.missing = mw;
	}
	if (/bold|black|heavy|demi/.test(lname))
		font.bold = true;
	if (/italic|oblique|slanted/.test(lname))
		font.italic = true;
	if (/courier|mono|consolas|typewriter|lucidaconsole|menlo/.test(lname))
		font.mono = true;
	else if (/times|serif|roman|georgia|garamond|palatino|bookman|century|cambria|minion|charter|cmr|cmbx|cmti|nimbusrom/.test(lname) && !/sans/.test(lname))
		font.family = 1;
	if (font.mono)
		font.family = 2;

	/* encoding for simple fonts */
	var tu = doc.resolve(fdict.ToUnicode);
	if (tu instanceof PDFStream)
		font.tounicode = parse_tounicode(doc, tu);

	if (font.kind != "type0") {
		var enc2 = doc.resolve(fdict.Encoding);
		var bename = null;
		var diffs = null;
		if (isName(enc2))
			bename = enc2.name;
		else if (isDict(enc2)) {
			var be = doc.resolve(enc2.BaseEncoding);
			if (isName(be))
				bename = be.name;
			diffs = doc.resolve(enc2.Differences);
		}
		var uni = base_encoding(bename || "WinAnsiEncoding");
		if (diffs instanceof Array) {
			var code = 0;
			for (i = 0; i < diffs.length; i++) {
				var d = doc.resolve(diffs[i]);
				if (typeof d == "number")
					code = d;
				else if (d instanceof Name) {
					if (code >= 0 && code < 256) {
						var u = glyph_to_unicode(d.name);
						uni[code] = u;
						if (!font.glyphnames)
							font.glyphnames = {};
						font.glyphnames[code] = d.name;
						if (font.type3)
							font.type3.names[code] = d.name;
					}
					code++;
				}
			}
		}
		font.uni = uni;
		var fc = doc.resolve(fdict.FirstChar);
		font.first = typeof fc == "number" ? fc : 0;
		font.widths = num_array(doc, fdict.Widths);
	}
	fdict._font = font;
	return font;
}

/* Returns the string of unicode characters for a character code */
function font_unicode(font, code)
{
	var s = null;
	if (font.tounicode && font.tounicode[code] !== undefined)
		s = font.tounicode[code];
	else if (font.kind == "type0")
		s = font.ucs2 ? String.fromCharCode(code) : null;
	else if (font.uni)
		s = font.uni[code];
	return s;
}

/* Advance width of a character code, in em units */
function font_width(font, code, str)
{
	var w;
	if (font.kind == "type0") {
		w = font.cidw && font.cidw[code];
		if (typeof w != "number")
			w = font.dw;
		return w / 1000;
	}
	if (font.kind == "type3") {
		w = font.widths && font.widths[code - font.first];
		return typeof w == "number" ? w * font.type3.matrix[0] : 0;
	}
	if (font.widths) {
		w = font.widths[code - font.first];
		if (typeof w == "number")
			return w / 1000;
		return (font.missing || 0) / 1000;
	}
	if (str === null || str === undefined || str.length == 0)
		return 0.5;
	var tot = 0;
	for (var i = 0; i < str.length; i++)
		tot += std_char_width(font.family, font.bold, str.charAt(i));
	return tot / 1000;
}

/* ======================================================================= */
/* Part 2b: embedded font programs (TrueType, CFF/OpenType, Type 1)         */
/*                                                                          */
/* Glyph outlines are returned as paths in em units (y up):                 */
/*   [op, args...] flattened into one array, op: 0=M 1=L 2=Q 3=C 4=Z       */
/* ======================================================================= */

var PATH_M = 0, PATH_L = 1, PATH_Q = 2, PATH_C = 3, PATH_Z = 4;

/* ----- TrueType ----- */

function TTFont(d)
{
	this.d = d;
	this.tables = {};
	var off = 0;
	var tag4 = String.fromCharCode(d[0], d[1], d[2], d[3]);
	if (tag4 == "ttcf")
		off = this.u32(12);
	var n = this.u16(off + 4);
	for (var i = 0; i < n; i++) {
		var e = off + 12 + i * 16;
		var tag = String.fromCharCode(d[e], d[e + 1], d[e + 2], d[e + 3]);
		this.tables[tag] = { off: this.u32(e + 8), len: this.u32(e + 12) };
	}
	var head = this.tables.head;
	this.upem = head ? this.u16(head.off + 18) : 1000;
	if (!this.upem)
		this.upem = 1000;
	this.locfmt = head ? this.s16(head.off + 50) : 0;
	var maxp = this.tables.maxp;
	this.nglyphs = maxp ? this.u16(maxp.off + 4) : 0;
	this.cache = {};
	this.cmaps = null;
	if (this.tables["CFF "])
		this.cffdata = bytes_sub(d, this.tables["CFF "].off, this.tables["CFF "].off + this.tables["CFF "].len);
}

TTFont.prototype.u16 = function (o) { return (this.d[o] << 8) | this.d[o + 1]; };
TTFont.prototype.s16 = function (o) { var v = (this.d[o] << 8) | this.d[o + 1]; return v >= 32768 ? v - 65536 : v; };
TTFont.prototype.u32 = function (o) { return ((this.d[o] << 24) | (this.d[o + 1] << 16) | (this.d[o + 2] << 8) | this.d[o + 3]) >>> 0; };

TTFont.prototype.glyph_range = function (gid)
{
	var loca = this.tables.loca, glyf = this.tables.glyf;
	if (!loca || !glyf || gid < 0 || gid >= this.nglyphs)
		return null;
	var a, b;
	if (this.locfmt == 0) {
		a = this.u16(loca.off + gid * 2) * 2;
		b = this.u16(loca.off + gid * 2 + 2) * 2;
	}
	else {
		a = this.u32(loca.off + gid * 4);
		b = this.u32(loca.off + gid * 4 + 4);
	}
	if (b <= a)
		return null;
	return { s: glyf.off + a, e: glyf.off + b };
};

/* Returns an array of contours; each contour is an array of [x, y, onCurve] in font units */
TTFont.prototype.contours = function (gid, depth)
{
	var r = this.glyph_range(gid);
	if (!r || depth > 6)
		return [];
	var d = this.d;
	var p = r.s;
	var nc = this.s16(p);
	p += 10;
	var out = [];
	var i, j;
	if (nc > 600)
		return [];
	if (nc >= 0) {
		var ends = [];
		for (i = 0; i < nc; i++) {
			ends.push(this.u16(p));
			p += 2;
		}
		var npts = nc ? ends[nc - 1] + 1 : 0;
		if (npts > 10000 || p + 2 > r.e)
			return [];
		var ilen = this.u16(p);
		p += 2 + ilen;
		var flags = [];
		while (flags.length < npts && p < r.e) {
			var f = d[p++];
			flags.push(f);
			if (f & 8) {
				var rep = d[p++];
				while (rep-- > 0 && flags.length < npts)
					flags.push(f);
			}
		}
		var xs = [], ys = [], v = 0;
		for (i = 0; i < npts; i++) {
			var fl = flags[i];
			if (fl & 2) {
				var dx = d[p++];
				v += (fl & 16) ? dx : -dx;
			}
			else if (!(fl & 16)) {
				v += this.s16(p);
				p += 2;
			}
			xs.push(v);
		}
		v = 0;
		for (i = 0; i < npts; i++) {
			var fl2 = flags[i];
			if (fl2 & 4) {
				var dy = d[p++];
				v += (fl2 & 32) ? dy : -dy;
			}
			else if (!(fl2 & 32)) {
				v += this.s16(p);
				p += 2;
			}
			ys.push(v);
		}
		var start = 0;
		for (i = 0; i < nc; i++) {
			var c = [];
			for (j = start; j <= ends[i] && j < npts; j++)
				c.push([xs[j], ys[j], flags[j] & 1]);
			if (c.length)
				out.push(c);
			start = ends[i] + 1;
		}
		return out;
	}
	/* composite glyph */
	for (var ncomp = 0; ncomp < 64 && p + 4 < r.e + 4; ncomp++) {
		var cf = this.u16(p), gi = this.u16(p + 2);
		p += 4;
		var a1, a2;
		if (cf & 1) {
			if (cf & 2) {
				a1 = this.s16(p);
				a2 = this.s16(p + 2);
			}
			else {
				a1 = a2 = 0;
			}
			p += 4;
		}
		else {
			if (cf & 2) {
				a1 = d[p] >= 128 ? d[p] - 256 : d[p];
				a2 = d[p + 1] >= 128 ? d[p + 1] - 256 : d[p + 1];
			}
			else {
				a1 = a2 = 0;
			}
			p += 2;
		}
		var m00 = 1, m01 = 0, m10 = 0, m11 = 1;
		if (cf & 8) {
			m00 = m11 = this.s16(p) / 16384;
			p += 2;
		}
		else if (cf & 0x40) {
			m00 = this.s16(p) / 16384;
			m11 = this.s16(p + 2) / 16384;
			p += 4;
		}
		else if (cf & 0x80) {
			m00 = this.s16(p) / 16384;
			m01 = this.s16(p + 2) / 16384;
			m10 = this.s16(p + 4) / 16384;
			m11 = this.s16(p + 6) / 16384;
			p += 8;
		}
		var sub = this.contours(gi, depth + 1);
		for (i = 0; i < sub.length; i++) {
			var cc = [];
			for (j = 0; j < sub[i].length; j++) {
				var pt = sub[i][j];
				cc.push([pt[0] * m00 + pt[1] * m10 + a1, pt[0] * m01 + pt[1] * m11 + a2, pt[2]]);
			}
			out.push(cc);
		}
		if (!(cf & 0x20))
			break;
	}
	return out;
};

/* Glyph outline as a path in em units */
TTFont.prototype.outline = function (gid)
{
	if (this.cache[gid] !== undefined)
		return this.cache[gid];
	var cons = this.contours(gid, 0);
	var s = 1 / this.upem;
	var path = [];
	var total = 0;
	for (var cj = 0; cj < cons.length; cj++)
		total += cons[cj].length;
	if (total > 20000)
		cons = [];
	for (var ci = 0; ci < cons.length; ci++) {
		var c = cons[ci];
		var n = c.length;
		if (n < 2)
			continue;
		var si = -1, i;
		for (i = 0; i < n; i++)
			if (c[i][2]) {
				si = i;
				break;
			}
		var sx, sy;
		var pts = [];
		if (si < 0) {
			sx = (c[0][0] + c[n - 1][0]) / 2;
			sy = (c[0][1] + c[n - 1][1]) / 2;
			for (i = 0; i < n; i++)
				pts.push(c[i]);
		}
		else {
			sx = c[si][0];
			sy = c[si][1];
			for (i = 1; i <= n; i++)
				pts.push(c[(si + i) % n]);
		}
		path.push(PATH_M, sx * s, sy * s);
		var pend = null;	/* pending off-curve point */
		for (i = 0; i < pts.length; i++) {
			var pt = pts[i];
			if (pt[2]) {
				if (pend) {
					path.push(PATH_Q, pend[0] * s, pend[1] * s, pt[0] * s, pt[1] * s);
					pend = null;
				}
				else
					path.push(PATH_L, pt[0] * s, pt[1] * s);
			}
			else {
				if (pend) {
					var mx = (pend[0] + pt[0]) / 2, my = (pend[1] + pt[1]) / 2;
					path.push(PATH_Q, pend[0] * s, pend[1] * s, mx * s, my * s);
				}
				pend = pt;
			}
		}
		if (pend)
			path.push(PATH_Q, pend[0] * s, pend[1] * s, sx * s, sy * s);
		path.push(PATH_Z);
	}
	this.cache[gid] = path;
	return path;
};

TTFont.prototype.load_cmaps = function ()
{
	if (this.cmaps)
		return this.cmaps;
	this.cmaps = [];
	var t = this.tables.cmap;
	if (!t)
		return this.cmaps;
	var n = this.u16(t.off + 2);
	for (var i = 0; i < n; i++) {
		var e = t.off + 4 + i * 8;
		var sub = t.off + this.u32(e + 4);
		this.cmaps.push({ pid: this.u16(e), eid: this.u16(e + 2), off: sub, fmt: this.u16(sub) });
	}
	return this.cmaps;
};

TTFont.prototype.find_cmap = function (pid, eid)
{
	var cm = this.load_cmaps();
	for (var i = 0; i < cm.length; i++)
		if (cm[i].pid == pid && (eid < 0 || cm[i].eid == eid))
			return cm[i];
	return null;
};

TTFont.prototype.cmap_lookup = function (cm, code)
{
	if (!cm)
		return 0;
	var o = cm.off, i;
	switch (cm.fmt) {
		case 0:
			return code < 256 ? this.d[o + 6 + code] : 0;
		case 4: {
			var segx2 = this.u16(o + 6);
			var endc = o + 14, startc = endc + segx2 + 2, delta = startc + segx2, rangeo = delta + segx2;
			for (i = 0; i < segx2; i += 2) {
				if (code <= this.u16(endc + i)) {
					var sc = this.u16(startc + i);
					if (code < sc)
						return 0;
					var ro = this.u16(rangeo + i);
					if (ro == 0)
						return (code + this.u16(delta + i)) & 0xffff;
					var gi = this.u16(rangeo + i + ro + (code - sc) * 2);
					return gi ? (gi + this.u16(delta + i)) & 0xffff : 0;
				}
			}
			return 0;
		}
		case 6: {
			var first = this.u16(o + 6), cnt = this.u16(o + 8);
			if (code >= first && code < first + cnt)
				return this.u16(o + 10 + (code - first) * 2);
			return 0;
		}
		case 12: {
			var ng = this.u32(o + 12);
			for (i = 0; i < ng; i++) {
				var g = o + 16 + i * 12;
				var s0 = this.u32(g), e0 = this.u32(g + 4);
				if (code >= s0 && code <= e0)
					return this.u32(g + 8) + (code - s0);
			}
			return 0;
		}
	}
	return 0;
};

/* ----- CFF ----- */

var CFF_STD_STRINGS = (".notdef space exclam quotedbl numbersign dollar percent ampersand quoteright parenleft parenright asterisk plus comma hyphen period slash "
	+ "zero one two three four five six seven eight nine colon semicolon less equal greater question at A B C D E F G H I J K L M N O P Q R S T U V W X Y Z "
	+ "bracketleft backslash bracketright asciicircum underscore quoteleft a b c d e f g h i j k l m n o p q r s t u v w x y z braceleft bar braceright asciitilde "
	+ "exclamdown cent sterling fraction yen florin section currency quotesingle quotedblleft guillemotleft guilsinglleft guilsinglright fi fl endash dagger daggerdbl "
	+ "periodcentered paragraph bullet quotesinglbase quotedblbase quotedblright guillemotright ellipsis perthousand questiondown grave acute circumflex tilde macron "
	+ "breve dotaccent dieresis ring cedilla hungarumlaut ogonek caron emdash AE ordfeminine Lslash Oslash OE ordmasculine ae dotlessi lslash oslash oe germandbls "
	+ "onesuperior logicalnot mu trademark Eth onehalf plusminus Thorn onequarter divide brokenbar degree thorn threequarters twosuperior registered minus eth multiply "
	+ "threesuperior copyright Aacute Acircumflex Adieresis Agrave Aring Atilde Ccedilla Eacute Ecircumflex Edieresis Egrave Iacute Icircumflex Idieresis Igrave Ntilde "
	+ "Oacute Ocircumflex Odieresis Ograve Otilde Scaron Uacute Ucircumflex Udieresis Ugrave Yacute Ydieresis Zcaron aacute acircumflex adieresis agrave aring atilde "
	+ "ccedilla eacute ecircumflex edieresis egrave iacute icircumflex idieresis igrave ntilde oacute ocircumflex odieresis ograve otilde scaron uacute ucircumflex "
	+ "udieresis ugrave yacute ydieresis zcaron").split(" ");

/* Standard Encoding code -> SID for codes >= 161 */
var CFF_STD_HIGH = [161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175, 177, 178, 179, 180, 182, 183, 184, 185, 186, 187, 188, 189, 191,
	193, 194, 195, 196, 197, 198, 199, 200, 202, 203, 205, 206, 207, 208, 225, 227, 232, 233, 234, 235, 241, 245, 248, 249, 250, 251];

function cff_std_encoding_sid(code)
{
	if (code >= 32 && code <= 126)
		return code - 31;
	var i = CFF_STD_HIGH.indexOf(code);
	return i >= 0 ? 96 + i : 0;
}

function CFFFont(d)
{
	this.d = d;
	this.cache = {};
	var hdrsize = d[2];
	var p = hdrsize;
	var idx = this.read_index(p);	/* Name INDEX */
	p = idx.end;
	var topidx = this.read_index(p);
	p = topidx.end;
	var stridx = this.read_index(p);
	p = stridx.end;
	this.strings = stridx;
	this.gsubrs = this.read_index(p);
	var top = this.parse_dict(topidx.items[0].s, topidx.items[0].e);
	this.top = top;
	this.matrix = top[1207] || null;
	this.charstrings = this.read_index(top[17][0]);
	this.nglyphs = this.charstrings.count;
	this.iscid = top[1230] !== undefined;
	this.fds = [];
	this.fdselect = null;
	var i;
	if (this.iscid && top[1236]) {
		var fdarr = this.read_index(top[1236][0]);
		for (i = 0; i < fdarr.count; i++) {
			var fd = this.parse_dict(fdarr.items[i].s, fdarr.items[i].e);
			this.fds.push(this.parse_private(fd));
		}
		if (top[1237])
			this.fdselect = this.read_fdselect(top[1237][0]);
	}
	else
		this.fds.push(this.parse_private(top));
	this.charset = this.read_charset(top[15] ? top[15][0] : 0);
	this.encoding = null;
	this.name_to_gid = null;
	this.cid_to_gid = null;
}

CFFFont.prototype.read_index = function (p)
{
	var d = this.d;
	var count = (d[p] << 8) | d[p + 1];
	if (count == 0)
		return { count: 0, items: [], end: p + 2 };
	var os = d[p + 2];
	var offs = [];
	var q = p + 3;
	for (var i = 0; i <= count; i++) {
		var v = 0;
		for (var k = 0; k < os; k++)
			v = v * 256 + d[q++];
		offs.push(v);
	}
	var base = q - 1;
	var items = [];
	for (i = 0; i < count; i++)
		items.push({ s: base + offs[i], e: base + offs[i + 1] });
	return { count: count, items: items, end: base + offs[count] };
};

CFFFont.prototype.parse_dict = function (s, e)
{
	var d = this.d;
	var out = {};
	var st = [];
	var p = s;
	while (p < e) {
		var b = d[p++];
		if (b <= 21) {
			var op = b;
			if (b == 12)
				op = 1200 + d[p++];
			out[op] = st;
			st = [];
		}
		else if (b == 28) {
			var v = (d[p] << 8) | d[p + 1];
			st.push(v >= 32768 ? v - 65536 : v);
			p += 2;
		}
		else if (b == 29) {
			st.push((d[p] << 24) | (d[p + 1] << 16) | (d[p + 2] << 8) | d[p + 3]);
			p += 4;
		}
		else if (b == 30) {
			var str = "";
			var done = false;
			while (!done && p < e) {
				var by = d[p++];
				var nib = [by >> 4, by & 15];
				for (var k = 0; k < 2; k++) {
					var n = nib[k];
					if (n <= 9)
						str += n;
					else if (n == 10)
						str += ".";
					else if (n == 11)
						str += "E";
					else if (n == 12)
						str += "E-";
					else if (n == 14)
						str += "-";
					else if (n == 15) {
						done = true;
						break;
					}
				}
			}
			st.push(parseFloat(str));
		}
		else if (b >= 32 && b <= 246)
			st.push(b - 139);
		else if (b >= 247 && b <= 250) {
			st.push((b - 247) * 256 + d[p] + 108);
			p++;
		}
		else if (b >= 251 && b <= 254) {
			st.push(-(b - 251) * 256 - d[p] - 108);
			p++;
		}
	}
	return out;
};

CFFFont.prototype.parse_private = function (dict)
{
	var pv = { defw: 0, nomw: 0, subrs: null, matrix: dict[1207] || null };
	if (!dict[18] || dict[18].length < 2)
		return pv;
	var size = dict[18][0], off = dict[18][1];
	var pd = this.parse_dict(off, off + size);
	if (pd[20])
		pv.defw = pd[20][0];
	if (pd[21])
		pv.nomw = pd[21][0];
	if (pd[19])
		pv.subrs = this.read_index(off + pd[19][0]);
	return pv;
};

CFFFont.prototype.read_fdselect = function (p)
{
	var d = this.d;
	var fmt = d[p];
	var sel = new Uint8Array(this.nglyphs);
	var i;
	if (fmt == 0) {
		for (i = 0; i < this.nglyphs; i++)
			sel[i] = d[p + 1 + i];
	}
	else if (fmt == 3) {
		var nr = (d[p + 1] << 8) | d[p + 2];
		var q = p + 3;
		for (var r = 0; r < nr; r++) {
			var first = (d[q] << 8) | d[q + 1];
			var fd = d[q + 2];
			var next = (d[q + 3] << 8) | d[q + 4];
			for (i = first; i < next && i < this.nglyphs; i++)
				sel[i] = fd;
			q += 3;
		}
	}
	return sel;
};

/* charset: array gid -> SID (or CID for CID-keyed fonts) */
CFFFont.prototype.read_charset = function (off)
{
	var d = this.d;
	var cs = [0];
	var n = this.nglyphs;
	var i;
	if (off <= 2) {
		for (i = 1; i < n; i++)
			cs.push(i);
		return cs;
	}
	var fmt = d[off];
	var p = off + 1;
	if (fmt == 0) {
		for (i = 1; i < n; i++) {
			cs.push((d[p] << 8) | d[p + 1]);
			p += 2;
		}
	}
	else {
		while (cs.length < n && p < d.length) {
			var first = (d[p] << 8) | d[p + 1];
			p += 2;
			var left;
			if (fmt == 1) {
				left = d[p];
				p += 1;
			}
			else {
				left = (d[p] << 8) | d[p + 1];
				p += 2;
			}
			for (i = 0; i <= left && cs.length < n; i++)
				cs.push(first + i);
		}
	}
	return cs;
};

CFFFont.prototype.sid_name = function (sid)
{
	if (sid < CFF_STD_STRINGS.length)
		return CFF_STD_STRINGS[sid];
	var si = sid - 391;
	if (si >= 0 && si < this.strings.count) {
		var it = this.strings.items[si];
		return bytes_to_string(this.d, it.s, it.e);
	}
	return null;
};

CFFFont.prototype.gid_for_name = function (name)
{
	if (!this.name_to_gid) {
		this.name_to_gid = {};
		if (!this.iscid)
			for (var g = 0; g < this.charset.length; g++) {
				var nm = this.sid_name(this.charset[g]);
				if (nm !== null && this.name_to_gid[nm] === undefined)
					this.name_to_gid[nm] = g;
			}
	}
	var v = this.name_to_gid[name];
	return v === undefined ? 0 : v;
};

CFFFont.prototype.gid_for_cid = function (cid)
{
	if (!this.iscid)
		return cid < this.nglyphs ? cid : 0;
	if (!this.cid_to_gid) {
		this.cid_to_gid = {};
		for (var g = 0; g < this.charset.length; g++)
			this.cid_to_gid[this.charset[g]] = g;
	}
	var v = this.cid_to_gid[cid];
	return v === undefined ? 0 : v;
};

/* built-in encoding: code -> gid */
CFFFont.prototype.gid_for_code = function (code)
{
	if (!this.encoding) {
		var enc = {};
		var d = this.d;
		var eo = this.top[16] ? this.top[16][0] : 0;
		var g;
		if (eo <= 1) {
			/* Standard (or Expert, approximated) encoding */
			var sid2gid = {};
			for (g = 0; g < this.charset.length; g++)
				sid2gid[this.charset[g]] = g;
			for (var c = 0; c < 256; c++) {
				var sid = cff_std_encoding_sid(c);
				if (sid && sid2gid[sid] !== undefined)
					enc[c] = sid2gid[sid];
			}
		}
		else {
			var fmt = d[eo];
			var p = eo + 1;
			if ((fmt & 127) == 0) {
				var nc = d[p++];
				for (var i = 0; i < nc; i++)
					enc[d[p++]] = i + 1;
			}
			else {
				var nr = d[p++];
				g = 1;
				for (var r = 0; r < nr; r++) {
					var first = d[p++], left = d[p++];
					for (var k = 0; k <= left; k++)
						enc[first + k] = g++;
				}
			}
			if (fmt & 128) {
				var ns = d[p++];
				for (var s = 0; s < ns; s++) {
					var code2 = d[p], sid3 = (d[p + 1] << 8) | d[p + 2];
					p += 3;
					for (g = 0; g < this.charset.length; g++)
						if (this.charset[g] == sid3) {
							enc[code2] = g;
							break;
						}
				}
			}
		}
		this.encoding = enc;
	}
	var v = this.encoding[code];
	return v === undefined ? 0 : v;
};

/* Type 2 charstring interpreter; returns a path in em units */
CFFFont.prototype.outline = function (gid, raw)
{
	var ckey = raw ? "raw" + gid : gid;
	if (this.cache[ckey] !== undefined)
		return this.cache[ckey];
	var fdi = this.fdselect ? this.fdselect[gid] : 0;
	var priv = this.fds[fdi] || this.fds[0] || { defw: 0, nomw: 0, subrs: null };
	var self = this;
	var path = [];
	var stack = [];
	var x = 0, y = 0, nstems = 0, havewidth = false, open = false, done = false, steps = 0;
	var gsubrs = this.gsubrs, lsubrs = priv.subrs;
	var d = this.d;

	function bias(idx)
	{
		var n = idx ? idx.count : 0;
		return n < 1240 ? 107 : (n < 33900 ? 1131 : 32768);
	}
	var gbias = bias(gsubrs), lbias = bias(lsubrs);

	function moveto(dx, dy)
	{
		if (open)
			path.push(PATH_Z);
		x += dx;
		y += dy;
		path.push(PATH_M, x, y);
		open = true;
	}
	function lineto(dx, dy)
	{
		x += dx;
		y += dy;
		path.push(PATH_L, x, y);
	}
	function curveto(dx1, dy1, dx2, dy2, dx3, dy3)
	{
		var x1 = x + dx1, y1 = y + dy1;
		var x2 = x1 + dx2, y2 = y1 + dy2;
		x = x2 + dx3;
		y = y2 + dy3;
		path.push(PATH_C, x1, y1, x2, y2, x, y);
	}
	function checkwidth(expected)
	{
		/* a width argument is present if there is one more operand than expected */
		if (!havewidth) {
			havewidth = true;
			if (stack.length > expected)
				stack.shift();
		}
	}
	function run(s, e, depth)
	{
		var p = s;
		var i;
		while (p < e && !done) {
			if (++steps > 60000 || path.length > 100000) {
				done = true;
				return;
			}
			var b = d[p++];
			if (b >= 32 || b == 28) {
				if (b == 28) {
					var v = (d[p] << 8) | d[p + 1];
					stack.push(v >= 32768 ? v - 65536 : v);
					p += 2;
				}
				else if (b <= 246)
					stack.push(b - 139);
				else if (b <= 250) {
					stack.push((b - 247) * 256 + d[p++] + 108);
				}
				else if (b <= 254) {
					stack.push(-(b - 251) * 256 - d[p++] - 108);
				}
				else {
					var f = (d[p] << 24) | (d[p + 1] << 16) | (d[p + 2] << 8) | d[p + 3];
					stack.push(f / 65536);
					p += 4;
				}
				continue;
			}
			switch (b) {
				case 1: case 3: case 18: case 23:	/* stems */
					if (!havewidth) {
						havewidth = true;
						if (stack.length & 1)
							stack.shift();
					}
					nstems += stack.length >> 1;
					stack = [];
					break;
				case 19: case 20:	/* hintmask / cntrmask */
					if (!havewidth) {
						havewidth = true;
						if (stack.length & 1)
							stack.shift();
					}
					nstems += stack.length >> 1;
					stack = [];
					p += (nstems + 7) >> 3;
					break;
				case 21:	/* rmoveto */
					checkwidth(2);
					moveto(stack[0] || 0, stack[1] || 0);
					stack = [];
					break;
				case 22:	/* hmoveto */
					checkwidth(1);
					moveto(stack[0] || 0, 0);
					stack = [];
					break;
				case 4:	/* vmoveto */
					checkwidth(1);
					moveto(0, stack[0] || 0);
					stack = [];
					break;
				case 5:	/* rlineto */
					for (i = 0; i + 1 < stack.length; i += 2)
						lineto(stack[i], stack[i + 1]);
					stack = [];
					break;
				case 6: case 7: {	/* hlineto / vlineto */
					var horiz = (b == 6);
					for (i = 0; i < stack.length; i++) {
						if (horiz)
							lineto(stack[i], 0);
						else
							lineto(0, stack[i]);
						horiz = !horiz;
					}
					stack = [];
					break;
				}
				case 8:	/* rrcurveto */
					for (i = 0; i + 5 < stack.length; i += 6)
						curveto(stack[i], stack[i + 1], stack[i + 2], stack[i + 3], stack[i + 4], stack[i + 5]);
					stack = [];
					break;
				case 24:	/* rcurveline */
					for (i = 0; i + 5 < stack.length - 2; i += 6)
						curveto(stack[i], stack[i + 1], stack[i + 2], stack[i + 3], stack[i + 4], stack[i + 5]);
					if (i + 1 < stack.length)
						lineto(stack[i], stack[i + 1]);
					stack = [];
					break;
				case 25:	/* rlinecurve */
					for (i = 0; i + 1 < stack.length - 6; i += 2)
						lineto(stack[i], stack[i + 1]);
					if (i + 5 < stack.length)
						curveto(stack[i], stack[i + 1], stack[i + 2], stack[i + 3], stack[i + 4], stack[i + 5]);
					stack = [];
					break;
				case 26: {	/* vvcurveto */
					i = 0;
					var dx1 = 0;
					if (stack.length & 1)
						dx1 = stack[i++];
					for (; i + 3 < stack.length; i += 4) {
						curveto(dx1, stack[i], stack[i + 1], stack[i + 2], 0, stack[i + 3]);
						dx1 = 0;
					}
					stack = [];
					break;
				}
				case 27: {	/* hhcurveto */
					i = 0;
					var dy1 = 0;
					if (stack.length & 1)
						dy1 = stack[i++];
					for (; i + 3 < stack.length; i += 4) {
						curveto(stack[i], dy1, stack[i + 1], stack[i + 2], stack[i + 3], 0);
						dy1 = 0;
					}
					stack = [];
					break;
				}
				case 30: case 31: {	/* vhcurveto / hvcurveto */
					var hv = (b == 31);
					i = 0;
					while (i + 3 < stack.length) {
						var last = (stack.length - i == 5) ? stack[i + 4] : 0;
						if (hv)
							curveto(stack[i], 0, stack[i + 1], stack[i + 2], last, stack[i + 3]);
						else
							curveto(0, stack[i], stack[i + 1], stack[i + 2], stack[i + 3], last);
						hv = !hv;
						i += 4;
					}
					stack = [];
					break;
				}
				case 10: {	/* callsubr */
					var si = stack.pop() + lbias;
					if (lsubrs && si >= 0 && si < lsubrs.count && depth < 10)
						run(lsubrs.items[si].s, lsubrs.items[si].e, depth + 1);
					break;
				}
				case 29: {	/* callgsubr */
					var gi = stack.pop() + gbias;
					if (gi >= 0 && gi < gsubrs.count && depth < 10)
						run(gsubrs.items[gi].s, gsubrs.items[gi].e, depth + 1);
					break;
				}
				case 11:	/* return */
					return;
				case 14:	/* endchar */
					checkwidth(stack.length >= 4 ? 4 : 0);
					if (open)
						path.push(PATH_Z);
					open = false;
					done = true;
					if (stack.length >= 4) {
						/* seac-like accent composition: adx ady bchar achar */
						var adx = stack[stack.length - 4], ady = stack[stack.length - 3];
						var bch = stack[stack.length - 2], ach = stack[stack.length - 1];
						var bgid = self.gid_for_name(CFF_STD_STRINGS[cff_std_encoding_sid(bch)] || ""), agid = self.gid_for_name(CFF_STD_STRINGS[cff_std_encoding_sid(ach)] || "");
						var bp = bgid ? self.raw_outline(bgid) : [], ap = agid ? self.raw_outline(agid) : [];
						for (i = 0; i < bp.length; i++)
							path.push(bp[i]);
						var j = 0;
						while (j < ap.length) {
							var op = ap[j];
							var cnt = op == PATH_M || op == PATH_L ? 2 : (op == PATH_C ? 6 : 0);
							path.push(op);
							for (var q = 0; q < cnt; q += 2)
								path.push(ap[j + 1 + q] + adx, ap[j + 2 + q] + ady);
							j += 1 + cnt;
						}
					}
					stack = [];
					return;
				case 12: {
					var b2 = d[p++];
					switch (b2) {
						case 35:	/* flex */
							curveto(stack[0], stack[1], stack[2], stack[3], stack[4], stack[5]);
							curveto(stack[6], stack[7], stack[8], stack[9], stack[10], stack[11]);
							break;
						case 34:	/* hflex */
							curveto(stack[0], 0, stack[1], stack[2], stack[3], 0);
							curveto(stack[4], 0, stack[5], -stack[2], stack[6], 0);
							break;
						case 36:	/* hflex1 */
							curveto(stack[0], stack[1], stack[2], stack[3], stack[4], 0);
							curveto(stack[5], 0, stack[6], stack[7], stack[8], -(stack[1] + stack[3] + stack[7]));
							break;
						case 37: {	/* flex1 */
							var dx = stack[0] + stack[2] + stack[4] + stack[6] + stack[8];
							var dy = stack[1] + stack[3] + stack[5] + stack[7] + stack[9];
							curveto(stack[0], stack[1], stack[2], stack[3], stack[4], stack[5]);
							if (Math.abs(dx) > Math.abs(dy))
								curveto(stack[6], stack[7], stack[8], stack[9], stack[10], -dy);
							else
								curveto(stack[6], stack[7], stack[8], stack[9], -dx, stack[10]);
							break;
						}
					}
					stack = [];
					break;
				}
				default:
					stack = [];
			}
		}
	}
	var it = this.charstrings.items[gid];
	if (it)
		run(it.s, it.e, 0);
	if (open)
		path.push(PATH_Z);
	/* apply the font matrix: the Font DICT's matrix (if any) concatenated with the Top DICT's */
	var m;
	if (raw)
		m = [1, 0, 0, 1, 0, 0];
	else if (priv.matrix && this.matrix)
		m = mat_mul(priv.matrix, this.matrix);
	else
		m = priv.matrix || this.matrix || [0.001, 0, 0, 0.001, 0, 0];
	var out = [];
	var i2 = 0;
	while (i2 < path.length) {
		var op2 = path[i2++];
		out.push(op2);
		var cnt2 = op2 == PATH_M || op2 == PATH_L ? 2 : (op2 == PATH_C ? 6 : 0);
		for (var q2 = 0; q2 < cnt2; q2 += 2) {
			var px = path[i2++], py = path[i2++];
			out.push(px * m[0] + py * m[2] + m[4], px * m[1] + py * m[3] + m[5]);
		}
	}
	this.cache[ckey] = out;
	return out;
};

/* outline in font units (used by seac composition) */
CFFFont.prototype.raw_outline = function (gid)
{
	return this.outline(gid, true);
};

/* ----- Type 1 ----- */

function T1Font(data, len1, len2)
{
	var i;
	/* strip PFB segment headers if present */
	if (data[0] == 0x80) {
		var parts = [], p = 0;
		while (p + 6 <= data.length && data[p] == 0x80) {
			var t = data[p + 1];
			if (t == 3)
				break;
			var l = data[p + 2] | (data[p + 3] << 8) | (data[p + 4] << 16) | (data[p + 5] << 24);
			parts.push(bytes_sub(data, p + 6, p + 6 + l));
			p += 6 + l;
		}
		var tot = 0;
		for (i = 0; i < parts.length; i++)
			tot += parts[i].length;
		var nd = new Uint8Array(tot), o = 0;
		for (i = 0; i < parts.length; i++) {
			nd.set(parts[i], o);
			o += parts[i].length;
		}
		data = nd;
		len1 = 0;
	}
	var ee = find_bytes(data, "eexec", 0);
	if (ee < 0)
		throw "no eexec";
	var clear = bytes_to_string(data, 0, ee);
	var p2 = ee + 5;
	while (p2 < data.length && (data[p2] == 10 || data[p2] == 13 || data[p2] == 32 || data[p2] == 9))
		p2++;
	var enc = bytes_sub(data, p2);
	/* ASCII hex form? */
	var ishex = true;
	for (i = 0; i < 4 && i < enc.length; i++) {
		var c = enc[i];
		if (!((c >= 48 && c <= 57) || (c >= 65 && c <= 70) || (c >= 97 && c <= 102)))
			ishex = false;
	}
	if (ishex)
		enc = ascii_hex_decode(enc);
	var priv = this.decrypt(enc, 55665, 4);
	this.clear = clear;
	this.priv = priv;
	this.cache = {};
	this.fm = [0.001, 0, 0, 0.001, 0, 0];
	var fm = /\/FontMatrix\s*\[\s*([-\d.eE+]+)\s+([-\d.eE+]+)\s+([-\d.eE+]+)\s+([-\d.eE+]+)\s+([-\d.eE+]+)\s+([-\d.eE+]+)/.exec(clear);
	if (fm)
		this.fm = [parseFloat(fm[1]), parseFloat(fm[2]), parseFloat(fm[3]), parseFloat(fm[4]), parseFloat(fm[5]), parseFloat(fm[6])];
	var privs = bytes_to_string(priv);
	var lm = /\/lenIV\s+(\d+)/.exec(privs);
	this.leniv = lm ? parseInt(lm[1], 10) : 4;
	this.subrs = [];
	this.chars = {};
	/* Subrs */
	var re = /dup\s+(\d+)\s+(\d+)\s+(?:RD|-\|)\s/g;
	var m;
	var csStart = privs.indexOf("/CharStrings");
	while ((m = re.exec(privs)) !== null) {
		if (csStart >= 0 && m.index > csStart)
			break;
		var idx = parseInt(m[1], 10), ln = parseInt(m[2], 10);
		var s0 = m.index + m[0].length;
		this.subrs[idx] = this.decrypt(bytes_sub(priv, s0, s0 + ln), 4330, this.leniv);
		re.lastIndex = s0 + ln;
	}
	/* CharStrings */
	if (csStart >= 0) {
		var re2 = /\/(\S+)\s+(\d+)\s+(?:RD|-\|)\s/g;
		re2.lastIndex = csStart + 12;
		while ((m = re2.exec(privs)) !== null) {
			var ln2 = parseInt(m[2], 10);
			var s1 = m.index + m[0].length;
			this.chars[m[1]] = this.decrypt(bytes_sub(priv, s1, s1 + ln2), 4330, this.leniv);
			re2.lastIndex = s1 + ln2;
		}
	}
	/* built-in encoding */
	this.encoding = null;
	var ei = clear.indexOf("/Encoding");
	if (ei >= 0 && !/\/Encoding\s+StandardEncoding/.test(clear.substr(ei, 40))) {
		this.encoding = {};
		var re3 = /dup\s+(\d+)\s*\/(\S+)\s+put/g;
		re3.lastIndex = ei;
		var m3;
		while ((m3 = re3.exec(clear)) !== null)
			this.encoding[parseInt(m3[1], 10)] = m3[2];
	}
}

T1Font.prototype.decrypt = function (data, r, skip)
{
	var n = data.length;
	var out = new Uint8Array(Math.max(0, n - skip));
	for (var i = 0; i < n; i++) {
		var c = data[i];
		var p = c ^ (r >> 8);
		r = ((c + r) * 52845 + 22719) & 0xffff;
		if (i >= skip)
			out[i - skip] = p;
	}
	return out;
};

T1Font.prototype.has_glyph = function (name)
{
	return this.chars[name] !== undefined;
};

/* Type 1 charstring interpreter; returns an em-unit path */
T1Font.prototype.outline = function (name, noscale)
{
	var key = noscale ? "raw:" + name : name;
	if (this.cache[key] !== undefined)
		return this.cache[key];
	var cs = this.chars[name];
	var self = this;
	var path = [];
	if (!cs) {
		this.cache[key] = path;
		return path;
	}
	var stack = [], ps = [];
	var x = 0, y = 0, sbx = 0, open = false, done = false, steps = 0;
	var inflex = false, flex = [];

	function moveto(dx, dy)
	{
		x += dx;
		y += dy;
		if (inflex) {
			flex.push(x, y);
			return;
		}
		if (open)
			path.push(PATH_Z);
		path.push(PATH_M, x, y);
		open = true;
	}
	function lineto(dx, dy)
	{
		x += dx;
		y += dy;
		if (!open) {
			path.push(PATH_M, x - dx, y - dy);
			open = true;
		}
		path.push(PATH_L, x, y);
	}
	function curveto(dx1, dy1, dx2, dy2, dx3, dy3)
	{
		if (!open) {
			path.push(PATH_M, x, y);
			open = true;
		}
		var x1 = x + dx1, y1 = y + dy1, x2 = x1 + dx2, y2 = y1 + dy2;
		x = x2 + dx3;
		y = y2 + dy3;
		path.push(PATH_C, x1, y1, x2, y2, x, y);
	}
	function run(code, depth)
	{
		var p = 0, n = code.length, i;
		while (p < n && !done) {
			if (++steps > 60000 || path.length > 100000) {
				done = true;
				return;
			}
			var b = code[p++];
			if (b >= 32) {
				if (b <= 246)
					stack.push(b - 139);
				else if (b <= 250)
					stack.push((b - 247) * 256 + code[p++] + 108);
				else if (b <= 254)
					stack.push(-(b - 251) * 256 - code[p++] - 108);
				else {
					stack.push((code[p] << 24) | (code[p + 1] << 16) | (code[p + 2] << 8) | code[p + 3]);
					p += 4;
				}
				continue;
			}
			switch (b) {
				case 1: case 3:
					stack = [];
					break;
				case 4: moveto(0, stack[0]); stack = []; break;
				case 5: lineto(stack[0], stack[1]); stack = []; break;
				case 6: lineto(stack[0], 0); stack = []; break;
				case 7: lineto(0, stack[0]); stack = []; break;
				case 8: curveto(stack[0], stack[1], stack[2], stack[3], stack[4], stack[5]); stack = []; break;
				case 9:
					if (open)
						path.push(PATH_Z);
					open = false;
					stack = [];
					break;
				case 10: {
					var si = stack.pop();
					if (self.subrs[si] && depth < 10)
						run(self.subrs[si], depth + 1);
					break;
				}
				case 11: return;
				case 13:	/* hsbw */
					sbx = stack[0];
					x = sbx;
					y = 0;
					stack = [];
					break;
				case 14:
					if (open)
						path.push(PATH_Z);
					open = false;
					done = true;
					return;
				case 21: moveto(stack[0], stack[1]); stack = []; break;
				case 22: moveto(stack[0], 0); stack = []; break;
				case 30: curveto(0, stack[0], stack[1], stack[2], stack[3], 0); stack = []; break;
				case 31: curveto(stack[0], 0, stack[1], stack[2], 0, stack[3]); stack = []; break;
				case 12: {
					var b2 = code[p++];
					switch (b2) {
						case 0: case 1: case 2:
							stack = [];
							break;
						case 6: {	/* seac: asb adx ady bchar achar */
							var asb = stack[0], adx = stack[1], ady = stack[2], bch = stack[3], ach = stack[4];
							var bn = CFF_STD_STRINGS[cff_std_encoding_sid(bch)], an = CFF_STD_STRINGS[cff_std_encoding_sid(ach)];
							var bp = self.outline(bn, true), ap = self.outline(an, true);
							if (open)
								path.push(PATH_Z);
							open = false;
							for (i = 0; i < bp.length; i++)
								path.push(bp[i]);
							var j = 0;
							var ox = sbx - asb + adx;
							while (j < ap.length) {
								var op = ap[j];
								var cnt = op == PATH_M || op == PATH_L ? 2 : (op == PATH_C ? 6 : 0);
								path.push(op);
								for (var q = 0; q < cnt; q += 2)
									path.push(ap[j + 1 + q] + ox - sbx, ap[j + 2 + q] + ady);
								j += 1 + cnt;
							}
							done = true;
							return;
						}
						case 7:	/* sbw */
							sbx = stack[0];
							x = sbx;
							y = stack[1];
							stack = [];
							break;
						case 12: {	/* div */
							var dv = stack.pop(), nu = stack.pop();
							stack.push(nu / dv);
							break;
						}
						case 16: {	/* callothersubr */
							var os = stack.pop(), na = stack.pop();
							var args = [];
							for (i = 0; i < na; i++)
								args.push(stack.pop());
							if (os == 1) {
								inflex = true;
								flex = [];
							}
							else if (os == 2) {
								/* flex point recorded by rmoveto */
							}
							else if (os == 0) {
								inflex = false;
								if (flex.length >= 14) {
									if (!open) {
										path.push(PATH_M, flex[0], flex[1]);
										open = true;
									}
									path.push(PATH_C, flex[2], flex[3], flex[4], flex[5], flex[6], flex[7]);
									path.push(PATH_C, flex[8], flex[9], flex[10], flex[11], flex[12], flex[13]);
								}
								/* args: [y, x, flexheight] (popped in reverse) */
								ps.push(args[0], args[1]);
								x = args[1];
								y = args[0];
							}
							else if (os == 3)
								ps.push(args[0]);
							else {
								for (i = args.length - 1; i >= 0; i--)
									ps.push(args[i]);
							}
							break;
						}
						case 17:	/* pop */
							stack.push(ps.length ? ps.pop() : 0);
							break;
						case 33:	/* setcurrentpoint */
							stack = [];
							break;
						default:
							stack = [];
					}
					break;
				}
				default:
					stack = [];
			}
		}
	}
	run(cs, 0);
	if (open)
		path.push(PATH_Z);
	if (noscale) {
		this.cache[key] = path;
		return path;
	}
	var m = this.fm;
	var out = [];
	var i2 = 0;
	while (i2 < path.length) {
		var op2 = path[i2++];
		out.push(op2);
		var cnt2 = op2 == PATH_M || op2 == PATH_L ? 2 : (op2 == PATH_C ? 6 : 0);
		for (var q2 = 0; q2 < cnt2; q2 += 2) {
			var px = path[i2++], py = path[i2++];
			out.push(px * m[0] + py * m[2] + m[4], px * m[1] + py * m[3] + m[5]);
		}
	}
	this.cache[key] = out;
	return out;
};

/* ----- attaching a font program to a PDF font ----- */

var REV_GLYPH_NAMES = null;

function unicode_glyph_name(ch)
{
	if (!REV_GLYPH_NAMES) {
		REV_GLYPH_NAMES = {};
		for (var n in GLYPH_NAMES)
			if (GLYPH_NAMES.hasOwnProperty(n) && GLYPH_NAMES[n].length == 1) {
				var cc = GLYPH_NAMES[n];
				if (REV_GLYPH_NAMES[cc] === undefined)
					REV_GLYPH_NAMES[cc] = n;
			}
		REV_GLYPH_NAMES[" "] = "space";
		REV_GLYPH_NAMES["-"] = "hyphen";
	}
	return REV_GLYPH_NAMES[ch];
}

/* Attempt to load the embedded font program. Returns an object or null. */
function load_font_program(doc, font)
{
	var desc = font.desc;
	if (!isDict(desc))
		return null;
	var ff = doc.resolve(desc.FontFile2), ff3 = doc.resolve(desc.FontFile3), ff1 = doc.resolve(desc.FontFile);
	try {
		if (ff instanceof PDFStream) {
			var tt = new TTFont(ff.bytes());
			if (tt.tables.glyf)
				return { kind: "tt", f: tt };
			return null;
		}
		if (ff3 instanceof PDFStream) {
			var data = ff3.bytes();
			var sub = doc.resolve(ff3.dict.Subtype);
			if (isName(sub, "OpenType") || (data[0] == 0x4f && data[1] == 0x54 && data[2] == 0x54 && data[3] == 0x4f) || (data[0] == 0 && data[1] == 1 && data[2] == 0 && data[3] == 0)) {
				var ot = new TTFont(data);
				if (ot.tables.glyf)
					return { kind: "tt", f: ot };
				if (ot.cffdata)
					return { kind: "cff", f: new CFFFont(ot.cffdata) };
				return null;
			}
			return { kind: "cff", f: new CFFFont(data) };
		}
		if (ff1 instanceof PDFStream) {
			var l1 = doc.resolve(ff1.dict.Length1), l2 = doc.resolve(ff1.dict.Length2);
			return { kind: "t1", f: new T1Font(ff1.bytes(), l1, l2) };
		}
	}
	catch (e) {
		if (typeof log == "function")
			log(LOG_DEBUG, "pdf2sixel: font program error: " + e);
	}
	return null;
}

/* Returns an em-unit path for a character code, or null if the font program has no glyph for it */
function font_outline(doc, font, code)
{
	if (font.prog === undefined)
		font.prog = font.desc ? load_font_program(doc, font) : null;
	var prog = font.prog;
	if (!prog)
		return null;
	if (!font.ol)
		font.ol = {};
	var cached = font.ol[code];
	if (cached !== undefined)
		return cached;
	var path = null;
	var f = prog.f;
	var uni = font_unicode(font, code);
	var gid = 0, i;
	if (prog.kind == "tt") {
		if (font.kind == "type0") {
			var cid = code;
			if (font.cidtogid) {
				var o = cid * 2;
				gid = o + 1 < font.cidtogid.length ? (font.cidtogid[o] << 8) | font.cidtogid[o + 1] : 0;
			}
			else
				gid = cid;
		}
		else {
			var c31 = f.find_cmap(3, 1) || f.find_cmap(0, -1), c30 = f.find_cmap(3, 0), c10 = f.find_cmap(1, 0);
			var symbolic = font.symbolic;
			if (!symbolic && uni && uni.length == 1 && c31)
				gid = f.cmap_lookup(c31, uni.charCodeAt(0));
			if (!gid && c30) {
				gid = f.cmap_lookup(c30, code);
				if (!gid)
					gid = f.cmap_lookup(c30, 0xf000 + code);
				if (!gid)
					gid = f.cmap_lookup(c30, 0xf100 + code);
				if (!gid)
					gid = f.cmap_lookup(c30, 0xf200 + code);
			}
			if (!gid && c10)
				gid = f.cmap_lookup(c10, code);
			if (!gid && c31 && uni && uni.length == 1)
				gid = f.cmap_lookup(c31, uni.charCodeAt(0));
			if (!gid && c31)
				gid = f.cmap_lookup(c31, code);
			if (!gid && !c31 && !c30 && !c10 && code < f.nglyphs)
				gid = code;
		}
		if (gid > 0)
			path = f.outline(gid);
	}
	else if (prog.kind == "cff") {
		if (font.kind == "type0")
			gid = f.gid_for_cid(code);
		else {
			var nm = font.glyphnames && font.glyphnames[code];
			if (!nm && uni && uni.length == 1)
				nm = unicode_glyph_name(uni);
			if (!nm && uni && uni.length > 1)
				nm = uni;	/* ligatures: fi, fl, ... */
			if (nm)
				gid = f.gid_for_name(nm);
			if (!gid && !font.hasencoding)
				gid = f.gid_for_code(code);
			if (!gid && font.symbolic)
				gid = f.gid_for_code(code);
		}
		if (gid > 0)
			path = f.outline(gid);
	}
	else if (prog.kind == "t1") {
		var name = font.glyphnames && font.glyphnames[code];
		if (!name && uni && uni.length == 1)
			name = unicode_glyph_name(uni);
		if (!name && uni && uni.length > 1)
			name = uni;
		if ((!name || !f.has_glyph(name)) && f.encoding && f.encoding[code] && (!font.hasencoding || !name))
			name = f.encoding[code];
		if ((!name || !f.has_glyph(name)) && !font.hasencoding) {
			var stdn = CFF_STD_STRINGS[cff_std_encoding_sid(code)];
			if (stdn)
				name = stdn;
		}
		if (name && f.has_glyph(name))
			path = f.outline(name);
	}
	font.ol[code] = path;
	return path;
}

/* ----- built-in outlines for the standard text faces (pdf2sixel_fonts.js) ----- */

var BUILTIN_STATE = 0;	/* 0 = not tried, 1 = loaded, -1 = unavailable */
var BUILTIN_CACHE = {};
var BUILTIN_FOBJ = {};

function builtin_path(font, u)
{
	if (BUILTIN_STATE == 0) {
		BUILTIN_STATE = -1;
		try {
			load("pdf2sixel_fonts.js");
			if (typeof P2S_FONTS == "object")
				BUILTIN_STATE = 1;
		}
		catch (e) { }
	}
	if (BUILTIN_STATE < 0 || /symbol|dingbat|wingding/i.test(font.name))
		return null;
	var fam = font.family == 2 ? "mono" : (font.family == 1 ? "serif" : "sans");
	var key = fam + (font.bold ? "b" : "");
	var set = P2S_FONTS[key];
	if (set[u] === undefined) {
		key = fam;
		set = P2S_FONTS[key];
		if (set[u] === undefined)
			return null;
	}
	var ck = key + "/" + u;
	var p = BUILTIN_CACHE[ck];
	if (p === undefined) {
		p = [];
		var re = /([mlcz])([^mlcz]*)/g;
		var m, cx = 0, cy = 0;
		while ((m = re.exec(set[u])) !== null) {
			var op = m[1];
			if (op == "z") {
				p.push(PATH_Z);
				continue;
			}
			var n = m[2].split(" ");
			var v = [];
			for (var i = 0; i < n.length; i++)
				v.push(parseInt(n[i], 10) / 250);
			if (op == "c") {
				p.push(PATH_C, cx + v[0], cy + v[1], cx + v[2], cy + v[3], cx + v[4], cy + v[5]);
				cx += v[4];
				cy += v[5];
			}
			else {
				cx += v[0];
				cy += v[1];
				p.push(op == "m" ? PATH_M : PATH_L, cx, cy);
			}
		}
		if (!BUILTIN_FOBJ[key])
			BUILTIN_FOBJ[key] = { uid: "B" + key };
		p.fobj = BUILTIN_FOBJ[key];
		BUILTIN_CACHE[ck] = p;
	}
	return p;
}

/* ======================================================================= */
/* Part 3: rasteriser (anti-aliased path fill, stroke, glyphs, images)      */
/* ======================================================================= */

var FB = null;		/* Int32Array of 0xRRGGBB pixels */
var CLIPMASK = null;	/* optional Uint8Array coverage mask (general clip paths) */
var COVTARGET = null;	/* when set, fill_polys writes coverage (0-255) here instead of painting */
var FW = 0, FH = 0;
var ACC = null, DIFF = null;
var SUBSAMPLES = 4;
var ROW255 = null, ROWBUF = null, ROWCOL = -1, ROWFILLED = 0;

function fb_run(start, n, packed)
{
	var i;
	if (n < 12) {
		for (i = 0; i < n; i++)
			FB[start + i] = packed;
		return;
	}
	if (ROWCOL !== packed) {
		ROWCOL = packed;
		ROWFILLED = 0;
	}
	if (ROWFILLED < n) {
		for (i = ROWFILLED; i < n; i++)
			ROWBUF[i] = packed;
		ROWFILLED = n;
	}
	FB.set(ROWBUF.subarray(0, n), start);
}

function fb_init(w, h, rgb)
{
	FW = w;
	FH = h;
	FB = new Int32Array(w * h);
	if (rgb)
		for (var i = 0; i < FB.length; i++)
			FB[i] = rgb;
	ACC = new Float32Array(w + 3);
	DIFF = new Float32Array(w + 3);
	MARK = new Uint8Array(w + 3);
	BMARK = new Uint8Array((w >> 4) + 3);
	ROWBUF = new Int32Array(w + 3);
	ROW255 = new Uint8Array(w + 3);
	for (var j = 0; j < ROW255.length; j++)
		ROW255[j] = 255;
	ROWCOL = -1;
	ROWFILLED = 0;
}

function rgb_pack(c)
{
	return (c[0] << 16) | (c[1] << 8) | c[2];
}

/* matrices: [a b c d e f], x' = a*x + c*y + e, y' = b*x + d*y + f */
function mat_mul(m, n)
{
	return [m[0] * n[0] + m[1] * n[2], m[0] * n[1] + m[1] * n[3],
		m[2] * n[0] + m[3] * n[2], m[2] * n[1] + m[3] * n[3],
		m[4] * n[0] + m[5] * n[2] + n[4], m[4] * n[1] + m[5] * n[3] + n[5]];
}

function mat_inv(m)
{
	var det = m[0] * m[3] - m[1] * m[2];
	if (Math.abs(det) < 1e-12)
		return null;
	var id = 1 / det;
	return [m[3] * id, -m[1] * id, -m[2] * id, m[0] * id,
		(m[2] * m[5] - m[3] * m[4]) * id, (m[1] * m[4] - m[0] * m[5]) * id];
}

function mat_apply(m, x, y)
{
	return [m[0] * x + m[2] * y + m[4], m[1] * x + m[3] * y + m[5]];
}

function mat_scale(m)
{
	return Math.sqrt(Math.abs(m[0] * m[3] - m[1] * m[2]));
}

/* ----- scan conversion ----- */

var MARK = null, BMARK = null;

/* Paint a run of n pixels with constant coverage cv (steady interior of a shape) */
function fill_steady(base, xa, xb, cv, r, g, b, alpha, opaque, packed)
{
	var n = xb - xa, q, qe;
	if (COVTARGET) {
		if (cv >= 0.998 && n >= 16)
			COVTARGET.set(ROW255.subarray(0, n), base + xa);
		else {
			var cb = cv * 255 + 0.5;
			for (q = base + xa, qe = base + xb; q < qe; q++)
				COVTARGET[q] = cb;
		}
	}
	else if (!CLIPMASK && opaque && cv >= 0.996)
		fb_run(base + xa, n, packed);
	else {
		for (q = base + xa, qe = base + xb; q < qe; q++) {
			var ca = cv * alpha;
			if (CLIPMASK)
				ca *= CLIPMASK[q] * 0.00392157;
			if (ca <= 0.003)
				continue;
			var pv = FB[q];
			var pr = (pv >> 16) & 255, pg = (pv >> 8) & 255, pb = pv & 255;
			FB[q] = ((pr + (r - pr) * ca + 0.5) << 16) | ((pg + (g - pg) * ca + 0.5) << 8) | ((pb + (b - pb) * ca + 0.5) | 0);
		}
	}
}

/* polys: array of flat [x0,y0,x1,y1,...] arrays (implicitly closed).
 * clip: [x0,y0,x1,y1] in device pixels. */
function fill_polys(polys, evenodd, rgb, alpha, clip)
{
	var cx0 = Math.max(0, clip[0]), cy0 = Math.max(0, clip[1]);
	var cx1 = Math.min(FW, clip[2]), cy1 = Math.min(FH, clip[3]);
	if (cx1 <= cx0 || cy1 <= cy0 || alpha <= 0)
		return;
	var edges = [];
	var ymin = 1e30, ymax = -1e30;
	var p, i, n, pts;
	for (p = 0; p < polys.length; p++) {
		pts = polys[p];
		n = pts.length;
		if (n < 6)
			continue;
		var px = pts[n - 2], py = pts[n - 1];
		for (i = 0; i < n; i += 2) {
			var qx = pts[i], qy = pts[i + 1];
			if (py != qy) {
				var e;
				if (py < qy)
					e = { y0: py, y1: qy, x0: px, m: (qx - px) / (qy - py), d: 1 };
				else
					e = { y0: qy, y1: py, x0: qx, m: (px - qx) / (py - qy), d: -1 };
				edges.push(e);
				if (e.y0 < ymin)
					ymin = e.y0;
				if (e.y1 > ymax)
					ymax = e.y1;
			}
			px = qx;
			py = qy;
		}
	}
	if (edges.length == 0)
		return;
	if (ymin < cy0)
		ymin = cy0;
	if (ymax > cy1)
		ymax = cy1;
	if (ymax <= ymin)
		return;
	edges.sort(function (a, b) { return a.y0 - b.y0; });
	var SS = SUBSAMPLES;
	var wgt = 1 / SS;
	var row0 = Math.floor(ymin), row1 = Math.ceil(ymax);
	var active = [];
	var ei = 0, ne = edges.length;
	var xs = [], ds = [];
	var r = rgb[0], g = rgb[1], b = rgb[2];
	var opaque = alpha >= 0.999;
	var packed = (r << 16) | (g << 8) | b;
	for (var row = row0; row < row1; row++) {
		var tminx = FW + 8, tmaxx = -1;
		for (var s = 0; s < SS; s++) {
			var y = row + (s + 0.5) * wgt;
			while (ei < ne && edges[ei].y0 <= y)
				active.push(edges[ei++]);
			var cnt = 0;
			for (i = 0; i < active.length;) {
				var ae = active[i];
				if (ae.y1 <= y) {
					active[i] = active[active.length - 1];
					active.pop();
					continue;
				}
				var x = ae.x0 + (y - ae.y0) * ae.m;
				/* insertion sort */
				var k = cnt++;
				while (k > 0 && xs[k - 1] > x) {
					xs[k] = xs[k - 1];
					ds[k] = ds[k - 1];
					k--;
				}
				xs[k] = x;
				ds[k] = ae.d;
				i++;
			}
			if (y < cy0 || y >= cy1)
				continue;
			var wind = 0, inside = false, xa = 0;
			for (i = 0; i < cnt; i++) {
				wind += ds[i];
				var now = evenodd ? ((i & 1) == 0) : (wind != 0);
				if (now) {
					if (!inside) {
						inside = true;
						xa = xs[i];
					}
					continue;
				}
				if (!inside)
					continue;
				inside = false;
				var xb = xs[i];
				if (xa < cx0)
					xa = cx0;
				if (xb > cx1)
					xb = cx1;
				if (xb <= xa)
					continue;
				var ia = xa | 0, ib = xb | 0;
				if (ia < tminx)
					tminx = ia;
				if (ib > tmaxx)
					tmaxx = ib;
				MARK[ia] = 1;
				BMARK[ia >> 4] = 1;
				if (ia == ib)
					ACC[ia] += (xb - xa) * wgt;
				else {
					ACC[ia] += (ia + 1 - xa) * wgt;
					ACC[ib] += (xb - ib) * wgt;
					DIFF[ia + 1] += wgt;
					DIFF[ib] -= wgt;
					MARK[ia + 1] = 1;
					BMARK[(ia + 1) >> 4] = 1;
					MARK[ib] = 1;
					BMARK[ib >> 4] = 1;
				}
			}
		}
		if (tmaxx < 0)
			continue;
		var base = row * FW;
		var lastx = -1, run = 0;
		for (var bi = tminx >> 4, be = tmaxx >> 4; bi <= be; bi++) {
			if (!BMARK[bi])
				continue;
			BMARK[bi] = 0;
			for (var t = bi << 4, te = t + 16; t < te; t++) {
				if (!MARK[t])
					continue;
				MARK[t] = 0;
				if (lastx >= 0 && t > lastx + 1 && run > 0.004)
					fill_steady(base, lastx + 1, t < FW ? t : FW, run > 1 ? 1 : run, r, g, b, alpha, opaque, packed);
				run += DIFF[t];
				var cov = ACC[t] + run;
				ACC[t] = 0;
				DIFF[t] = 0;
				lastx = t;
				if (t >= FW || cov <= 0.004)
					continue;
				if (cov > 1)
					cov = 1;
				var idx = base + t;
				if (COVTARGET) {
					COVTARGET[idx] = cov * 255 + 0.5;
					continue;
				}
				if (CLIPMASK) {
					cov *= CLIPMASK[idx] * 0.00392157;
					if (cov <= 0.004)
						continue;
				}
				if (opaque && cov >= 0.996)
					FB[idx] = packed;
				else {
					var a = cov * alpha;
					var pv = FB[idx];
					var pr = (pv >> 16) & 255, pg = (pv >> 8) & 255, pb = pv & 255;
					FB[idx] = ((pr + (r - pr) * a + 0.5) << 16) | ((pg + (g - pg) * a + 0.5) << 8) | ((pb + (b - pb) * a + 0.5) | 0);
				}
			}
		}
	}
}

/* ----- bezier flattening: appends points to pts (flat array) ----- */
function flatten_curve(pts, x0, y0, x1, y1, x2, y2, x3, y3)
{
	var len = Math.abs(x1 - x0) + Math.abs(y1 - y0) + Math.abs(x2 - x1) + Math.abs(y2 - y1) + Math.abs(x3 - x2) + Math.abs(y3 - y2);
	var n = Math.ceil(Math.sqrt(len) * 0.9);
	if (n < 3)
		n = 3;
	else if (n > 48)
		n = 48;
	for (var i = 1; i <= n; i++) {
		var t = i / n, u = 1 - t;
		var a = u * u * u, b = 3 * u * u * t, c = 3 * u * t * t, d = t * t * t;
		pts.push(a * x0 + b * x1 + c * x2 + d * x3, a * y0 + b * y1 + c * y2 + d * y3);
	}
}

/* ----- stroking ----- */

function poly_area(p)
{
	var a = 0, n = p.length;
	var px = p[n - 2], py = p[n - 1];
	for (var i = 0; i < n; i += 2) {
		a += px * p[i + 1] - p[i] * py;
		px = p[i];
		py = p[i + 1];
	}
	return a;
}

function push_poly(out, p)
{
	if (poly_area(p) < 0) {
		var r = [];
		for (var i = p.length - 2; i >= 0; i -= 2)
			r.push(p[i], p[i + 1]);
		p = r;
	}
	out.push(p);
}

function circle_poly(cx, cy, rad)
{
	var n = rad < 2 ? 8 : (rad < 8 ? 14 : 24);
	var p = [];
	for (var i = 0; i < n; i++) {
		var a = 2 * Math.PI * i / n;
		p.push(cx + rad * Math.cos(a), cy + rad * Math.sin(a));
	}
	return p;
}

/* sub: { pts:[x,y,...], closed:bool } in device space. Returns polygons. */
function stroke_subpath(sub, hw, cap, join, miterlimit, out)
{
	var src = sub.pts;
	var pts = [];
	var i;
	for (i = 0; i < src.length; i += 2) {
		var n = pts.length;
		if (n >= 2 && Math.abs(pts[n - 2] - src[i]) < 1e-6 && Math.abs(pts[n - 1] - src[i + 1]) < 1e-6)
			continue;
		pts.push(src[i], src[i + 1]);
	}
	var closed = sub.closed;
	if (closed && pts.length >= 4 && Math.abs(pts[0] - pts[pts.length - 2]) < 1e-6 && Math.abs(pts[1] - pts[pts.length - 1]) < 1e-6) {
		pts.pop();
		pts.pop();
	}
	var np = pts.length / 2;
	if (np == 1) {
		/* degenerate: dot (only if round cap) */
		if (cap == 1)
			push_poly(out, circle_poly(pts[0], pts[1], hw));
		else if (cap == 2)
			push_poly(out, [pts[0] - hw, pts[1] - hw, pts[0] + hw, pts[1] - hw, pts[0] + hw, pts[1] + hw, pts[0] - hw, pts[1] + hw]);
		return;
	}
	if (np < 2)
		return;
	var segs = closed ? np : np - 1;
	var dirs = [];
	for (i = 0; i < segs; i++) {
		var j = (i + 1) % np;
		var dx = pts[2 * j] - pts[2 * i], dy = pts[2 * j + 1] - pts[2 * i + 1];
		var l = Math.sqrt(dx * dx + dy * dy);
		dirs.push([dx / l, dy / l, l]);
	}
	for (i = 0; i < segs; i++) {
		var j2 = (i + 1) % np;
		var d = dirs[i];
		var x0 = pts[2 * i], y0 = pts[2 * i + 1], x1 = pts[2 * j2], y1 = pts[2 * j2 + 1];
		var nx = -d[1] * hw, ny = d[0] * hw;
		var ex0 = 0, ex1 = 0;
		if (!closed && cap == 2) {
			if (i == 0)
				ex0 = hw;
			if (i == segs - 1)
				ex1 = hw;
		}
		x0 -= d[0] * ex0;
		y0 -= d[1] * ex0;
		x1 += d[0] * ex1;
		y1 += d[1] * ex1;
		push_poly(out, [x0 + nx, y0 + ny, x1 + nx, y1 + ny, x1 - nx, y1 - ny, x0 - nx, y0 - ny]);
	}
	/* joins */
	var firstj = closed ? 0 : 1;
	var lastj = closed ? np - 1 : np - 2;
	for (i = firstj; i <= lastj; i++) {
		var d1 = dirs[(i - 1 + segs) % segs], d2 = dirs[i % segs];
		var vx = pts[2 * i], vy = pts[2 * i + 1];
		var cr = d1[0] * d2[1] - d1[1] * d2[0];
		var dt = d1[0] * d2[0] + d1[1] * d2[1];
		if (Math.abs(cr) < 1e-4 && dt > 0)
			continue;
		if (join == 1) {
			if (hw <= 0.6)
				continue;
			/* round join: a pie wedge between the two outer offsets */
			var sd = cr > 0 ? -1 : 1;
			var a1 = Math.atan2(d1[0] * sd, -d1[1] * sd), a2 = Math.atan2(d2[0] * sd, -d2[1] * sd);
			var da = a2 - a1;
			while (da > Math.PI)
				da -= 2 * Math.PI;
			while (da < -Math.PI)
				da += 2 * Math.PI;
			var steps = Math.max(1, Math.ceil(Math.abs(da) / (hw < 3 ? 0.9 : (hw < 12 ? 0.45 : 0.3))));
			var wedge = [vx, vy];
			for (var st = 0; st <= steps; st++) {
				var aa = a1 + da * st / steps;
				wedge.push(vx + hw * Math.cos(aa), vy + hw * Math.sin(aa));
			}
			push_poly(out, wedge);
			continue;
		}
		var side = cr > 0 ? -1 : 1;
		var n1x = -d1[1] * hw * side, n1y = d1[0] * hw * side;
		var n2x = -d2[1] * hw * side, n2y = d2[0] * hw * side;
		var poly = [vx, vy, vx + n1x, vy + n1y];
		if (join == 0 && dt > -0.999) {
			var ratio = Math.sqrt(2 / (1 + dt));
			if (ratio <= miterlimit) {
				var k = 1 / (1 + (n1x * n2x + n1y * n2y) / (hw * hw));
				poly.push(vx + (n1x + n2x) * k, vy + (n1y + n2y) * k);
			}
		}
		poly.push(vx + n2x, vy + n2y);
		push_poly(out, poly);
	}
	if (!closed && cap == 1) {
		push_poly(out, circle_poly(pts[0], pts[1], hw));
		push_poly(out, circle_poly(pts[2 * np - 2], pts[2 * np - 1], hw));
	}
}

/* apply a dash pattern (device units) to a subpath; returns list of open subpaths */
function dash_subpath(sub, dash, phase)
{
	var pts = sub.pts.slice();
	if (sub.closed && pts.length >= 4)
		pts.push(pts[0], pts[1]);
	var out = [];
	var total = 0, i;
	for (i = 0; i < dash.length; i++)
		total += dash[i];
	if (total <= 0.01)
		return [{ pts: pts, closed: sub.closed }];
	var idx = 0, on = true;
	var rem = phase % (total * (dash.length % 2 ? 2 : 1));
	if (rem < 0)
		rem = 0;
	while (rem >= dash[idx % dash.length] && rem > 0) {
		rem -= dash[idx % dash.length];
		idx++;
		on = !on;
		if (idx > 10000)
			break;
	}
	var left = dash[idx % dash.length] - rem;
	var cur = on ? [pts[0], pts[1]] : null;
	var count = 0;
	for (i = 0; i + 3 < pts.length; i += 2) {
		var x0 = pts[i], y0 = pts[i + 1], x1 = pts[i + 2], y1 = pts[i + 3];
		var dx = x1 - x0, dy = y1 - y0;
		var len = Math.sqrt(dx * dx + dy * dy);
		var pos = 0;
		while (len - pos > left) {
			pos += left;
			var px = x0 + dx * pos / len, py = y0 + dy * pos / len;
			if (on) {
				cur.push(px, py);
				out.push({ pts: cur, closed: false });
				cur = null;
			}
			else
				cur = [px, py];
			on = !on;
			idx++;
			left = dash[idx % dash.length];
			if (left <= 0.001)
				left = 0.001;
			if (++count > 20000)
				return out;
		}
		left -= (len - pos);
		if (on && cur)
			cur.push(x1, y1);
	}
	if (on && cur && cur.length >= 4)
		out.push({ pts: cur, closed: false });
	return out;
}

/* ----- bitmap-font glyph rendering ----- */

var BITMAP_FONTS = {};
var FONT_FILES = [["swiss-1.f16", "swiss-3.f16", "ibmstd.f16"], ["roman-1.f16", "roman.f16", "swiss-1.f16"], ["courier.f16", "ibmstd.f16"]];
var CAP_EM = 0.72;

function load_bitmap_font(family)
{
	if (BITMAP_FONTS[family])
		return BITMAP_FONTS[family];
	var names = FONT_FILES[family];
	var data = null;
	for (var i = 0; i < names.length && !data; i++) {
		var f = new File(system.ctrl_dir + "fonts/" + names[i]);
		if (f.open("rb")) {
			var s = f.read(4096);
			f.close();
			if (s && s.length == 4096)
				data = string_to_bytes(s);
		}
	}
	var font = { ok: !!data, data: data, height: 16, caprows: 10, base: 12, inkw: 7 };
	if (data) {
		/* measure the capital 'H' for the cap height and baseline */
		var top = 16, bot = -1, maxcol = 0, r;
		for (r = 0; r < 16; r++) {
			if (data[72 * 16 + r]) {
				if (r < top)
					top = r;
				bot = r;
			}
		}
		if (bot >= top) {
			font.caprows = bot - top + 1;
			font.base = bot + 1;
		}
		for (var ch = 33; ch < 127; ch++) {
			for (r = 0; r < 16; r++) {
				var v = data[ch * 16 + r];
				for (var c = 0; c < 8; c++)
					if ((v >> (7 - c)) & 1 && c + 1 > maxcol)
						maxcol = c + 1;
			}
		}
		font.inkw = Math.max(5, maxcol);
	}
	BITMAP_FONTS[family] = font;
	return font;
}

function unicode_to_cp437(u)
{
	if (u >= 32 && u < 127)
		return u;
	if (u < 32)
		return -2;	/* blank */
	if (CP437_REV[u] !== undefined)
		return CP437_REV[u];
	switch (u) {
		case 0x2018: case 0x2019: case 0x2032: return 39;
		case 0x201c: case 0x201d: case 0x201e: case 0x2033: return 34;
		case 0x2010: case 0x2011: case 0x2012: case 0x2013: case 0x2014: case 0x2015: case 0x2212: return 45;
		case 0x2022: return 7;
		case 0x2026: return 46;
		case 0x2044: return 47;
		case 0x20ac: return 69;
		case 0x2122: return 84;
		case 0x0131: return 105;
		case 0x2039: return 60;
		case 0x203a: return 62;
		case 0x00a0: case 0x2002: case 0x2003: case 0x2009: case 0x200b: case 0xfeff: case 0xad: return -2;
	}
	if (u >= 0xc0 && u <= 0xff)
		return LATIN1_FOLD.charCodeAt(u - 0xc0);
	if (u >= 0x100 && u <= 0x17f)
		return -1;
	return -1;
}

/* Padded (11 x 19) copy of a glyph bitmap so bilinear sampling needs no bounds checks.
 * Index = (row + 1) * 11 + (col + 1). A bold variant is smeared one cell to the right. */
function glyph_padded(bf, cp, bold)
{
	var key = bold ? cp + 256 : cp;
	if (!bf.pad)
		bf.pad = {};
	if (bf.pad[key])
		return bf.pad[key];
	var p = new Uint8Array(11 * 19);
	for (var r = 0; r < 16; r++) {
		var v = bf.data[cp * 16 + r];
		if (bold)
			v |= (v >> 1);
		for (var c = 0; c < 8; c++)
			if ((v >> (7 - c)) & 1)
				p[(r + 1) * 11 + c + 1] = 1;
	}
	bf.pad[key] = p;
	return p;
}

/* Scale2x (EPX) edge-smoothing upscale of a binary image */
function scale2x(src, w, h)
{
	var dw = w * 2, dst = new Uint8Array(dw * h * 2);
	for (var y = 0; y < h; y++) {
		for (var x = 0; x < w; x++) {
			var P = src[y * w + x];
			var A = y > 0 ? src[(y - 1) * w + x] : 0;
			var D = y < h - 1 ? src[(y + 1) * w + x] : 0;
			var C = x > 0 ? src[y * w + x - 1] : 0;
			var B = x < w - 1 ? src[y * w + x + 1] : 0;
			var o = y * 2 * dw + x * 2;
			dst[o] = (C == A && C != D && A != B) ? A : P;
			dst[o + 1] = (A == B && A != C && B != D) ? B : P;
			dst[o + dw] = (D == C && D != B && C != A) ? C : P;
			dst[o + dw + 1] = (B == D && B != A && D != C) ? D : P;
		}
	}
	return dst;
}

/* 4x edge-smoothed copy of a glyph, padded by one cell: dims 34 x 66, index (r+1)*34 + (c+1) */
function glyph_hires(bf, cp, bold)
{
	var key = (bold ? cp + 256 : cp);
	if (!bf.hires)
		bf.hires = {};
	if (bf.hires[key])
		return bf.hires[key];
	var src = new Uint8Array(8 * 16);
	for (var r = 0; r < 16; r++) {
		var v = bf.data[cp * 16 + r];
		if (bold)
			v |= (v >> 1);
		for (var c = 0; c < 8; c++)
			src[r * 8 + c] = (v >> (7 - c)) & 1;
	}
	var big = scale2x(scale2x(src, 8, 16), 16, 32);	/* 32 x 64 */
	var p = new Uint8Array(34 * 66);
	for (var y = 0; y < 64; y++)
		for (var x = 0; x < 32; x++)
			p[(y + 1) * 34 + x + 1] = big[y * 32 + x];
	bf.hires[key] = p;
	return p;
}

var GLYPH_CACHE = {};
var GLYPH_CACHE_N = 0;

/* Rasterise a glyph into a coverage bitmap. M maps em-space (x right, y up, 1 = font
 * size) to device pixels with the glyph origin at a sub-pixel offset from (0,0). */
function glyph_render(bf, cp, M, adv, bold)
{
	var inv = mat_inv(M);
	if (!inv)
		return null;
	var rowem = CAP_EM / bf.caprows;
	var w = adv > 0.02 ? adv : 0.02;
	var ys = [bf.base * rowem, (bf.base - 16) * rowem];
	var minx = 1e9, maxx = -1e9, miny = 1e9, maxy = -1e9;
	for (var i = 0; i < 4; i++) {
		var p = mat_apply(M, (i & 1) ? w : 0, ys[i >> 1]);
		if (p[0] < minx) minx = p[0];
		if (p[0] > maxx) maxx = p[0];
		if (p[1] < miny) miny = p[1];
		if (p[1] > maxy) maxy = p[1];
	}
	var x0 = Math.floor(minx), y0 = Math.floor(miny);
	var gw = Math.ceil(maxx) - x0, gh = Math.ceil(maxy) - y0;
	if (gw <= 0 || gh <= 0 || gw > 400 || gh > 400)
		return null;
	var cellpx = Math.sqrt(Math.abs(M[0] * M[3] - M[1] * M[2])) * rowem;
	var hires = cellpx >= 1.0;
	var ns = cellpx < 0.4 ? 3 : (cellpx < 0.62 ? 2 : 1);
	var sharpen = cellpx >= 1.2;
	var uscale = bf.inkw / w;
	var pad = hires ? glyph_hires(bf, cp, bold) : glyph_padded(bf, cp, bold);
	var base = bf.base;
	var gwid = hires ? 34 : 11;
	var zoom = hires ? 4 : 1;
	var umax = 8 * zoom, vmax = 16 * zoom;
	var slope = hires ? Math.max(1, cellpx / 4) : 1;
	var cov = new Uint8Array(gw * gh);
	var inv0 = inv[0], inv1 = inv[1], inv2 = inv[2], inv3 = inv[3], inv4 = inv[4], inv5 = inv[5];
	var norm = 1 / (ns * ns);
	var invrow = 1 / rowem;
	var uk = uscale * zoom, vk = invrow * zoom, bk = base * zoom;
	var px, py, sx, sy;
	for (py = 0; py < gh; py++) {
		for (px = 0; px < gw; px++) {
			var tot = 0;
			for (sy = 0; sy < ns; sy++) {
				for (sx = 0; sx < ns; sx++) {
					var dx = x0 + px + (sx + 0.5) / ns, dy = y0 + py + (sy + 0.5) / ns;
					var xe = inv0 * dx + inv2 * dy + inv4;
					var ye = inv1 * dx + inv3 * dy + inv5;
					var u = xe * uk - 0.5;
					var v = bk - ye * vk - 0.5;
					if (u < -1 || u >= umax || v < -1 || v >= vmax)
						continue;
					var c0 = Math.floor(u), r0 = Math.floor(v);
					var fu = u - c0, fv = v - r0;
					var i0 = (r0 + 1) * gwid + c0 + 1;
					var a = pad[i0], b = pad[i0 + 1], cc = pad[i0 + gwid], d = pad[i0 + gwid + 1];
					var top = a + (b - a) * fu, bot = cc + (d - cc) * fu;
					tot += top + (bot - top) * fv;
				}
			}
			var cv = tot * norm;
			if (hires) {
				cv = (cv - 0.5) * slope + 0.5;
				if (cv < 0)
					cv = 0;
				else if (cv > 1)
					cv = 1;
			}
			else if (sharpen) {
				cv = cv * 1.25 - 0.1;
				if (cv < 0)
					cv = 0;
				else if (cv > 1)
					cv = 1;
				cv = cv * cv * (3 - 2 * cv);
			}
			cov[py * gw + px] = cv * 255 + 0.5;
		}
	}
	return { x0: x0, y0: y0, w: gw, h: gh, a: cov };
}

/* Blit a cached coverage bitmap whose origin pixel is (ix, iy) */
function blit_cov(e, ix, iy, rgb, alpha, clip)
{
	var gx = ix + e.x0, gy = iy + e.y0;
	var px0 = Math.max(0, Math.ceil(clip[0] - 0.5)), py0 = Math.max(0, Math.ceil(clip[1] - 0.5));
	var px1 = Math.min(FW, Math.ceil(clip[2] - 0.5)), py1 = Math.min(FH, Math.ceil(clip[3] - 0.5));
	var xa = Math.max(gx, px0), xb = Math.min(gx + e.w, px1);
	var ya = Math.max(gy, py0), yb = Math.min(gy + e.h, py1);
	if (xb <= xa || yb <= ya)
		return;
	var r = rgb[0], g = rgb[1], b = rgb[2];
	var cov = e.a, ew = e.w;
	var mask = CLIPMASK;
	var k = alpha / 255;
	for (var y = ya; y < yb; y++) {
		var co = (y - gy) * ew - gx;
		var fo = y * FW;
		for (var x = xa; x < xb; x++) {
			var cv = cov[co + x];
			if (cv == 0)
				continue;
			var a = cv * k;
			var idx = fo + x;
			if (mask)
				a *= mask[idx] * 0.00392157;
			if (a <= 0.003)
				continue;
			if (a >= 0.996)
				FB[idx] = (r << 16) | (g << 8) | b;
			else {
				var pv = FB[idx];
				var pr = (pv >> 16) & 255, pg = (pv >> 8) & 255, pb = pv & 255;
				FB[idx] = ((pr + (r - pr) * a + 0.5) << 16) | ((pg + (g - pg) * a + 0.5) << 8) | ((pb + (b - pb) * a + 0.5) | 0);
			}
		}
	}
}

/* Draw one bitmap-font glyph. M maps em-space to device pixels. */
function draw_glyph(bf, cp, M, adv, rgb, alpha, bold, clip)
{
	var qx = Math.floor(M[4] * 4 + 0.5), qy = Math.floor(M[5] * 2 + 0.5);
	var ix = Math.floor(qx / 4), iy = Math.floor(qy / 2);
	var fx = (qx - ix * 4) / 4, fy = (qy - iy * 2) / 2;
	var key = cp + "|" + (bold ? 1 : 0) + "|" + Math.round(M[0] * 64) + "," + Math.round(M[1] * 64) + "," + Math.round(M[2] * 64) + "," + Math.round(M[3] * 64)
		+ "|" + Math.round(adv * 100) + "|" + (qx - ix * 4) + (qy - iy * 2);
	var e = GLYPH_CACHE[key];
	if (e === undefined) {
		if (GLYPH_CACHE_N > 6000) {
			GLYPH_CACHE = {};
			GLYPH_CACHE_N = 0;
		}
		e = glyph_render(bf, cp, [M[0], M[1], M[2], M[3], fx, fy], adv, bold);
		GLYPH_CACHE[key] = e;
		GLYPH_CACHE_N++;
	}
	if (e)
		blit_cov(e, ix, iy, rgb, alpha, clip);
}

/* Flatten an em-unit outline path through matrix M into device-space polygons */
function flatten_path(path, M, polys)
{
	var a = M[0], b = M[1], c = M[2], d = M[3], e = M[4], f = M[5];
	var i = 0, n = path.length;
	var cur = null;
	var cx = 0, cy = 0;
	var minx = 1e9, miny = 1e9, maxx = -1e9, maxy = -1e9;
	while (i < n) {
		var op = path[i++];
		var x, y;
		if (op == PATH_M) {
			if (cur && cur.length >= 6)
				polys.push(cur);
			x = path[i++];
			y = path[i++];
			cx = a * x + c * y + e;
			cy = b * x + d * y + f;
			cur = [cx, cy];
		}
		else if (op == PATH_L) {
			x = path[i++];
			y = path[i++];
			cx = a * x + c * y + e;
			cy = b * x + d * y + f;
			cur.push(cx, cy);
		}
		else if (op == PATH_Q) {
			var x1 = path[i++], y1 = path[i++];
			x = path[i++];
			y = path[i++];
			var qx1 = a * x1 + c * y1 + e, qy1 = b * x1 + d * y1 + f;
			var qx2 = a * x + c * y + e, qy2 = b * x + d * y + f;
			var len = Math.abs(qx1 - cx) + Math.abs(qy1 - cy) + Math.abs(qx2 - qx1) + Math.abs(qy2 - qy1);
			var ns = Math.ceil(Math.sqrt(len) * 0.6) + 1;
			if (ns > 24)
				ns = 24;
			for (var k = 1; k <= ns; k++) {
				var t = k / ns, u = 1 - t;
				cur.push(u * u * cx + 2 * u * t * qx1 + t * t * qx2, u * u * cy + 2 * u * t * qy1 + t * t * qy2);
			}
			cx = qx2;
			cy = qy2;
		}
		else if (op == PATH_C) {
			var cx1 = path[i++], cy1 = path[i++], cx2 = path[i++], cy2 = path[i++];
			x = path[i++];
			y = path[i++];
			var p1x = a * cx1 + c * cy1 + e, p1y = b * cx1 + d * cy1 + f;
			var p2x = a * cx2 + c * cy2 + e, p2y = b * cx2 + d * cy2 + f;
			var p3x = a * x + c * y + e, p3y = b * x + d * y + f;
			flatten_curve(cur, cx, cy, p1x, p1y, p2x, p2y, p3x, p3y);
			cx = p3x;
			cy = p3y;
		}
		else if (op == PATH_Z) {
			if (cur && cur.length >= 6) {
				polys.push(cur);
				cur = [cx, cy];
				/* subsequent segments without a new moveto start at the contour start */
				cur = null;
			}
		}
	}
	if (cur && cur.length >= 6)
		polys.push(cur);
}

function glyph_render_outline(path, M)
{
	var polys = [];
	flatten_path(path, M, polys);
	if (polys.length == 0)
		return { x0: 0, y0: 0, w: 0, h: 0, a: null };
	var minx = 1e9, miny = 1e9, maxx = -1e9, maxy = -1e9, i, j;
	for (i = 0; i < polys.length; i++) {
		var p = polys[i];
		for (j = 0; j < p.length; j += 2) {
			if (p[j] < minx) minx = p[j];
			if (p[j] > maxx) maxx = p[j];
			if (p[j + 1] < miny) miny = p[j + 1];
			if (p[j + 1] > maxy) maxy = p[j + 1];
		}
	}
	var x0 = Math.floor(minx) - 1, y0 = Math.floor(miny) - 1;
	var gw = Math.ceil(maxx) + 1 - x0, gh = Math.ceil(maxy) + 1 - y0;
	if (gw <= 0 || gh <= 0 || gw > 600 || gh > 600)
		return null;
	for (i = 0; i < polys.length; i++) {
		var q = polys[i];
		for (j = 0; j < q.length; j += 2) {
			q[j] -= x0;
			q[j + 1] -= y0;
		}
	}
	var buf = new Uint8Array(gw * gh);
	var sfw = FW, sfh = FH, scm = CLIPMASK, sct = COVTARGET;
	FW = gw;
	FH = gh;
	CLIPMASK = null;
	COVTARGET = buf;
	fill_polys(polys, false, [0, 0, 0], 1, [0, 0, gw, gh]);
	FW = sfw;
	FH = sfh;
	CLIPMASK = scm;
	COVTARGET = sct;
	return { x0: x0, y0: y0, w: gw, h: gh, a: buf };
}

/* Draw an outline glyph (em-unit path) with matrix M (em -> device) */
function draw_outline_glyph(font, code, path, M, rgb, alpha, clip)
{
	var scale = Math.sqrt(Math.abs(M[0] * M[3] - M[1] * M[2]));
	if (scale > 70) {
		/* very large glyph: rasterise directly rather than caching a huge bitmap */
		var polys = [];
		flatten_path(path, M, polys);
		if (polys.length)
			fill_polys(polys, false, rgb, alpha, clip);
		return;
	}
	/* positions are quantised (1/3 pixel across, whole pixel down) so that glyph bitmaps can be reused */
	var qx = Math.floor(M[4] * 3 + 0.5);
	var ix = Math.floor(qx / 3), iy = Math.floor(M[5] + 0.5);
	var fx = (qx - ix * 3) / 3, fy = 0;
	var key = "o" + font.uid + "|" + code + "|" + Math.round(M[0] * 32) + "," + Math.round(M[1] * 32) + "," + Math.round(M[2] * 32) + "," + Math.round(M[3] * 32)
		+ "|" + (qx - ix * 3);
	var e = GLYPH_CACHE[key];
	if (e === undefined) {
		if (GLYPH_CACHE_N > 6000) {
			GLYPH_CACHE = {};
			GLYPH_CACHE_N = 0;
		}
		e = glyph_render_outline(path, [M[0], M[1], M[2], M[3], fx, fy]);
		GLYPH_CACHE[key] = e;
		GLYPH_CACHE_N++;
	}
	if (e && e.a)
		blit_cov(e, ix, iy, rgb, alpha, clip);
}

/* ----- images ----- */

/* img: { w, h, n (1 or 3), data: Uint8Array, alpha: Uint8Array|null, mask: bool }
 * Box-filter reduction by an integer factor f. */
function reduce_image(img, f)
{
	var nw = Math.max(1, Math.floor(img.w / f)), nh = Math.max(1, Math.floor(img.h / f));
	var n = img.n;
	var out = new Uint8Array(nw * nh * n);
	var oa = img.alpha ? new Uint8Array(nw * nh) : null;
	var area = f * f;
	var x, y, fx, fy, c;
	for (y = 0; y < nh; y++) {
		for (x = 0; x < nw; x++) {
			var sum0 = 0, sum1 = 0, sum2 = 0, suma = 0;
			for (fy = 0; fy < f; fy++) {
				var rowoff = ((y * f + fy) * img.w + x * f) * n;
				var aoff = (y * f + fy) * img.w + x * f;
				for (fx = 0; fx < f; fx++) {
					if (n == 1)
						sum0 += img.data[rowoff + fx];
					else {
						var o = rowoff + fx * 3;
						sum0 += img.data[o];
						sum1 += img.data[o + 1];
						sum2 += img.data[o + 2];
					}
					if (oa)
						suma += img.alpha[aoff + fx];
				}
			}
			var oo = (y * nw + x) * n;
			if (n == 1)
				out[oo] = sum0 / area;
			else {
				out[oo] = sum0 / area;
				out[oo + 1] = sum1 / area;
				out[oo + 2] = sum2 / area;
			}
			if (oa)
				oa[y * nw + x] = suma / area;
		}
	}
	return { w: nw, h: nh, n: n, data: out, alpha: oa, mask: img.mask };
}


/* Fast path for axis-aligned images */
function draw_image_aa(img, M, alpha, rgb, clip, x0, x1, y0, y1, smooth)
{
	var w = img.w, h = img.h, data = img.data, n = img.n, am = img.alpha;
	var ismask = img.mask;
	var ma = M[0], md = M[3], me = M[4], mf = M[5];
	var nx = x1 - x0;
	var cxa = new Int32Array(nx), cxb = new Int32Array(nx), tw = new Float32Array(nx);
	var i, ix;
	for (i = 0; i < nx; i++) {
		var u = (x0 + i + 0.5 - me) / ma;
		if (u < 0 || u >= 1) {
			cxa[i] = -1;
			continue;
		}
		if (smooth && w > 1) {
			var sx = u * w - 0.5;
			ix = Math.floor(sx);
			tw[i] = sx - ix;
			cxa[i] = ix < 0 ? 0 : (ix >= w ? w - 1 : ix);
			cxb[i] = ix + 1 < 0 ? 0 : (ix + 1 >= w ? w - 1 : ix + 1);
		}
		else {
			ix = (u * w) | 0;
			cxa[i] = ix >= w ? w - 1 : ix;
			cxb[i] = cxa[i];
			tw[i] = 0;
		}
	}
	var r = rgb ? rgb[0] : 0, g = rgb ? rgb[1] : 0, b = rgb ? rgb[2] : 0;
	var simple = !am && !ismask && !CLIPMASK && alpha >= 0.996;
	var GL = null;
	if (simple && n == 1) {
		GL = new Int32Array(256);
		for (i = 0; i < 256; i++)
			GL[i] = (i << 16) | (i << 8) | i;
	}
	var mask = CLIPMASK;
	for (var py = y0; py < y1; py++) {
		var v = (py + 0.5 - mf) / md;
		if (v < 0 || v >= 1)
			continue;
		var sy = (1 - v) * h;
		var r0, r1, ty = 0;
		if (smooth && h > 1) {
			var fy = sy - 0.5;
			var iy = Math.floor(fy);
			ty = fy - iy;
			r0 = (iy < 0 ? 0 : (iy >= h ? h - 1 : iy)) * w;
			r1 = (iy + 1 < 0 ? 0 : (iy + 1 >= h ? h - 1 : iy + 1)) * w;
		}
		else {
			var iy2 = sy | 0;
			r0 = (iy2 >= h ? h - 1 : iy2) * w;
			r1 = r0;
		}
		var fo = py * FW + x0;
		var bil = ty > 0 || smooth;
		if (simple && smooth) {
			if (n == 3) {
				for (i = 0; i < nx; i++) {
					var sa = cxa[i];
					if (sa < 0)
						continue;
					var stx = tw[i];
					var oa0 = (r0 + sa) * 3, oa1 = (r0 + cxb[i]) * 3, ob0 = (r1 + sa) * 3, ob1 = (r1 + cxb[i]) * 3;
					var u0 = data[oa0] + (data[oa1] - data[oa0]) * stx, u1 = data[ob0] + (data[ob1] - data[ob0]) * stx;
					var v0 = data[oa0 + 1] + (data[oa1 + 1] - data[oa0 + 1]) * stx, v1 = data[ob0 + 1] + (data[ob1 + 1] - data[ob0 + 1]) * stx;
					var w0 = data[oa0 + 2] + (data[oa1 + 2] - data[oa0 + 2]) * stx, w1 = data[ob0 + 2] + (data[ob1 + 2] - data[ob0 + 2]) * stx;
					FB[fo + i] = ((u0 + (u1 - u0) * ty + 0.5) << 16) | ((v0 + (v1 - v0) * ty + 0.5) << 8) | ((w0 + (w1 - w0) * ty + 0.5) | 0);
				}
			}
			else {
				for (i = 0; i < nx; i++) {
					var ga = cxa[i];
					if (ga < 0)
						continue;
					var gtx = tw[i];
					var ga0 = r0 + ga, ga1 = r0 + cxb[i], gb0 = r1 + ga, gb1 = r1 + cxb[i];
					var g0 = data[ga0] + (data[ga1] - data[ga0]) * gtx, g1 = data[gb0] + (data[gb1] - data[gb0]) * gtx;
					var gv = (g0 + (g1 - g0) * ty + 0.5) | 0;
					FB[fo + i] = (gv << 16) | (gv << 8) | gv;
				}
			}
			continue;
		}
		if (simple && !smooth) {
			if (n == 1) {
				for (i = 0; i < nx; i++) {
					var c0 = cxa[i];
					if (c0 >= 0)
						FB[fo + i] = GL[data[r0 + c0]];
				}
			}
			else {
				for (i = 0; i < nx; i++) {
					var c1 = cxa[i];
					if (c1 >= 0) {
						var o = (r0 + c1) * 3;
						FB[fo + i] = (data[o] << 16) | (data[o + 1] << 8) | data[o + 2];
					}
				}
			}
			continue;
		}
		for (i = 0; i < nx; i++) {
			var ca = cxa[i];
			if (ca < 0)
				continue;
			var cb = cxb[i], tx = tw[i];
			var pr, pg, pb, pa = 255;
			if (bil && (tx > 0 || ty > 0)) {
				var w00 = (1 - tx) * (1 - ty), w10 = tx * (1 - ty), w01 = (1 - tx) * ty, w11 = tx * ty;
				var q00 = r0 + ca, q10 = r0 + cb, q01 = r1 + ca, q11 = r1 + cb;
				if (n == 1)
					pr = pg = pb = data[q00] * w00 + data[q10] * w10 + data[q01] * w01 + data[q11] * w11;
				else {
					var o00 = q00 * 3, o10 = q10 * 3, o01 = q01 * 3, o11 = q11 * 3;
					pr = data[o00] * w00 + data[o10] * w10 + data[o01] * w01 + data[o11] * w11;
					pg = data[o00 + 1] * w00 + data[o10 + 1] * w10 + data[o01 + 1] * w01 + data[o11 + 1] * w11;
					pb = data[o00 + 2] * w00 + data[o10 + 2] * w10 + data[o01 + 2] * w01 + data[o11 + 2] * w11;
				}
				if (am)
					pa = am[q00] * w00 + am[q10] * w10 + am[q01] * w01 + am[q11] * w11;
			}
			else {
				var q = r0 + ca;
				if (n == 1)
					pr = pg = pb = data[q];
				else {
					var o3 = q * 3;
					pr = data[o3];
					pg = data[o3 + 1];
					pb = data[o3 + 2];
				}
				if (am)
					pa = am[q];
			}
			var a;
			if (ismask) {
				a = pr / 255 * alpha;
				pr = r;
				pg = g;
				pb = b;
			}
			else
				a = (pa / 255) * alpha;
			var idx = fo + i;
			if (mask)
				a *= mask[idx] * 0.00392157;
			if (a <= 0.003)
				continue;
			if (a >= 0.996)
				FB[idx] = ((pr + 0.5) << 16) | ((pg + 0.5) << 8) | ((pb + 0.5) | 0);
			else {
				var pv = FB[idx];
				var br = (pv >> 16) & 255, bg = (pv >> 8) & 255, bb = pv & 255;
				FB[idx] = ((br + (pr - br) * a + 0.5) << 16) | ((bg + (pg - bg) * a + 0.5) << 8) | ((bb + (pb - bb) * a + 0.5) | 0);
			}
		}
	}
}

function draw_image(img, M, alpha, rgb, clip)
{
	var inv = mat_inv(M);
	if (!inv)
		return;
	var minx = 1e9, maxx = -1e9, miny = 1e9, maxy = -1e9, i;
	var cs = [[0, 0], [1, 0], [1, 1], [0, 1]];
	for (i = 0; i < 4; i++) {
		var p = mat_apply(M, cs[i][0], cs[i][1]);
		if (p[0] < minx) minx = p[0];
		if (p[0] > maxx) maxx = p[0];
		if (p[1] < miny) miny = p[1];
		if (p[1] > maxy) maxy = p[1];
	}
	var x0 = Math.max(Math.floor(minx), Math.max(0, Math.floor(clip[0])));
	var x1 = Math.min(Math.ceil(maxx), Math.min(FW, Math.ceil(clip[2])));
	var y0 = Math.max(Math.floor(miny), Math.max(0, Math.floor(clip[1])));
	var y1 = Math.min(Math.ceil(maxy), Math.min(FH, Math.ceil(clip[3])));
	if (x1 <= x0 || y1 <= y0)
		return;
	/* source pixels per device pixel, to choose a reduction factor */
	var sxr = img.w * Math.sqrt(inv[0] * inv[0] + inv[1] * inv[1]);
	var syr = img.h * Math.sqrt(inv[2] * inv[2] + inv[3] * inv[3]);
	var ratio = Math.min(sxr, syr);
	var f = Math.floor(ratio);
	if (f >= 2) {
		if (f > 32)
			f = 32;
		if (!img.reduced)
			img.reduced = {};
		if (!img.reduced[f])
			img.reduced[f] = reduce_image(img, f);
		img = img.reduced[f];
	}
	var w = img.w, h = img.h, data = img.data, n = img.n, am = img.alpha;
	var smooth = ratio < 1.5;
	if (Math.abs(M[1]) < 1e-9 && Math.abs(M[2]) < 1e-9 && M[0] != 0 && M[3] != 0) {
		draw_image_aa(img, M, alpha, rgb, clip, x0, x1, y0, y1, ratio >= 0.5 && ratio < 2.5);
		return;
	}
	var r = rgb ? rgb[0] : 0, g = rgb ? rgb[1] : 0, b = rgb ? rgb[2] : 0;
	var ismask = img.mask;
	for (var py = y0; py < y1; py++) {
		for (var px = x0; px < x1; px++) {
			var dx = px + 0.5, dy = py + 0.5;
			var u = inv[0] * dx + inv[2] * dy + inv[4];
			var v = inv[1] * dx + inv[3] * dy + inv[5];
			if (u < 0 || u >= 1 || v < 0 || v >= 1)
				continue;
			var sx = u * w, sy = (1 - v) * h;
			var pr, pg, pb, pa = 255;
			if (smooth && w > 1 && h > 1) {
				var fx = sx - 0.5, fy = sy - 0.5;
				var ix = Math.floor(fx), iy = Math.floor(fy);
				var tx = fx - ix, ty = fy - iy;
				var ix1 = ix + 1, iy1 = iy + 1;
				if (ix < 0) ix = 0;
				if (iy < 0) iy = 0;
				if (ix1 >= w) ix1 = w - 1;
				if (iy1 >= h) iy1 = h - 1;
				if (ix >= w) ix = w - 1;
				if (iy >= h) iy = h - 1;
				if (ix1 < 0) ix1 = 0;
				if (iy1 < 0) iy1 = 0;
				var w00 = (1 - tx) * (1 - ty), w10 = tx * (1 - ty), w01 = (1 - tx) * ty, w11 = tx * ty;
				var o00 = (iy * w + ix) * n, o10 = (iy * w + ix1) * n, o01 = (iy1 * w + ix) * n, o11 = (iy1 * w + ix1) * n;
				if (n == 1) {
					pr = pg = pb = data[o00] * w00 + data[o10] * w10 + data[o01] * w01 + data[o11] * w11;
				}
				else {
					pr = data[o00] * w00 + data[o10] * w10 + data[o01] * w01 + data[o11] * w11;
					pg = data[o00 + 1] * w00 + data[o10 + 1] * w10 + data[o01 + 1] * w01 + data[o11 + 1] * w11;
					pb = data[o00 + 2] * w00 + data[o10 + 2] * w10 + data[o01 + 2] * w01 + data[o11 + 2] * w11;
				}
				if (am)
					pa = am[iy * w + ix] * w00 + am[iy * w + ix1] * w10 + am[iy1 * w + ix] * w01 + am[iy1 * w + ix1] * w11;
			}
			else {
				var qx = Math.floor(sx), qy = Math.floor(sy);
				if (qx >= w) qx = w - 1;
				if (qy >= h) qy = h - 1;
				var o = (qy * w + qx) * n;
				if (n == 1)
					pr = pg = pb = data[o];
				else {
					pr = data[o];
					pg = data[o + 1];
					pb = data[o + 2];
				}
				if (am)
					pa = am[qy * w + qx];
			}
			var a;
			if (ismask) {
				a = pr / 255 * alpha;
				pr = r;
				pg = g;
				pb = b;
			}
			else
				a = (pa / 255) * alpha;
			var idx = py * FW + px;
			if (CLIPMASK)
				a *= CLIPMASK[idx] * 0.00392157;
			if (a <= 0.003)
				continue;
			if (a >= 0.996)
				FB[idx] = ((pr + 0.5) << 16) | ((pg + 0.5) << 8) | ((pb + 0.5) | 0);
			else {
				var pv = FB[idx];
				var br = (pv >> 16) & 255, bg = (pv >> 8) & 255, bb = pv & 255;
				FB[idx] = ((br + (pr - br) * a + 0.5) << 16) | ((bg + (pg - bg) * a + 0.5) << 8) | ((bb + (pb - bb) * a + 0.5) | 0);
			}
		}
	}
}

/* ======================================================================= */
/* Part 4: JPEG decoder and PDF image loading                               */
/* ======================================================================= */

var JPEG_ZZ = [0, 1, 8, 16, 9, 2, 3, 10, 17, 24, 32, 25, 18, 11, 4, 5, 12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13, 6, 7, 14, 21, 28,
	35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51, 58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63];

var IDCT_TABLES = {};
function idct_table(N)
{
	if (IDCT_TABLES[N])
		return IDCT_TABLES[N];
	var t = new Float64Array(N * N);
	for (var x = 0; x < N; x++) {
		for (var u = 0; u < N; u++) {
			var cu = u == 0 ? Math.SQRT1_2 : 1;
			t[x * N + u] = 0.5 * cu * Math.cos((2 * x + 1) * u * Math.PI / (2 * N));
		}
	}
	IDCT_TABLES[N] = t;
	return t;
}

function jpeg_build_huff(counts, syms)
{
	var tab = new Int32Array(65536);
	var code = 0, k = 0;
	for (var len = 1; len <= 16; len++) {
		for (var i = 0; i < counts[len - 1]; i++) {
			var base = code << (16 - len);
			var n = 1 << (16 - len);
			var ent = (len << 8) | syms[k++];
			for (var j = 0; j < n; j++)
				tab[base + j] = ent;
			code++;
		}
		code <<= 1;
	}
	return tab;
}

/* Decode a baseline/extended-sequential JPEG. scaleN is the output size of each
 * 8x8 block (1, 2, 4 or 8). Returns null if the file can't be decoded.
 * Result: { w, h, n, data } where n is 1, 3 (RGB) or 4 (raw CMYK samples). */
function jpeg_decode(data, scaleN, abort)
{
	var pos = 2;
	var qt = [];
	var hts = { dc: [], ac: [] };
	var comps = [];
	var frame = null;
	var restart = 0;
	var adobe = -1;
	var N = scaleN || 8;
	var T = idct_table(N);
	var len = data.length;
	var i;

	if (data[0] != 0xff || data[1] != 0xd8)
		return null;
	var scan_done = false;
	while (pos < len) {
		if (data[pos] != 0xff) {
			pos++;
			continue;
		}
		var marker = data[pos + 1];
		pos += 2;
		if (marker == 0 || marker == 0xd8 || marker == 0x01 || (marker >= 0xd0 && marker <= 0xd7) || marker == 0xff) {
			if (marker == 0xff)
				pos--;
			continue;
		}
		if (marker == 0xd9)
			break;
		var seglen = (data[pos] << 8) | data[pos + 1];
		var segend = pos + seglen;
		var p = pos + 2;
		switch (marker) {
			case 0xdb:
				while (p < segend) {
					var pq = data[p] >> 4, tq = data[p] & 15;
					p++;
					var q = new Int32Array(64);
					for (i = 0; i < 64; i++) {
						if (pq) {
							q[i] = (data[p] << 8) | data[p + 1];
							p += 2;
						}
						else
							q[i] = data[p++];
					}
					qt[tq] = q;
				}
				break;
			case 0xc4:
				while (p < segend) {
					var tc = data[p] >> 4, th = data[p] & 15;
					p++;
					var counts = [], total = 0;
					for (i = 0; i < 16; i++) {
						counts[i] = data[p++];
						total += counts[i];
					}
					var syms = [];
					for (i = 0; i < total; i++)
						syms.push(data[p++]);
					hts[tc ? "ac" : "dc"][th] = jpeg_build_huff(counts, syms);
				}
				break;
			case 0xc0: case 0xc1:
				frame = { prec: data[p], h: (data[p + 1] << 8) | data[p + 2], w: (data[p + 3] << 8) | data[p + 4], n: data[p + 5] };
				p += 6;
				for (i = 0; i < frame.n; i++) {
					comps.push({ id: data[p], hs: data[p + 1] >> 4, vs: data[p + 1] & 15, tq: data[p + 2] });
					p += 3;
				}
				break;
			case 0xc2: case 0xc3: case 0xc5: case 0xc6: case 0xc7: case 0xc9: case 0xca: case 0xcb: case 0xcd: case 0xce: case 0xcf:
				return null;	/* progressive / lossless / arithmetic: unsupported */
			case 0xdd:
				restart = (data[p] << 8) | data[p + 1];
				break;
			case 0xee:
				if (data[p] == 0x41 && data[p + 1] == 0x64 && data[p + 2] == 0x6f && data[p + 3] == 0x62 && data[p + 4] == 0x65)
					adobe = data[p + 11];
				break;
			case 0xda: {
				if (!frame)
					return null;
				var ns = data[p++];
				var scomps = [];
				for (i = 0; i < ns; i++) {
					var cid = data[p], tbl = data[p + 1];
					p += 2;
					for (var ci = 0; ci < comps.length; ci++)
						if (comps[ci].id == cid) {
							comps[ci].dc = tbl >> 4;
							comps[ci].ac = tbl & 15;
							scomps.push(comps[ci]);
						}
				}
				pos = segend;
				pos = jpeg_decode_scan(data, pos, frame, comps, scomps, qt, hts, restart, N, T, abort);
				scan_done = true;
				continue;
			}
		}
		pos = segend;
	}
	if (!frame || !scan_done)
		return null;
	return jpeg_output(frame, comps, adobe, N);
}

function jpeg_decode_scan(data, pos, frame, comps, scomps, qt, hts, restart, N, T, abort)
{
	var len = data.length;
	var hmax = 1, vmax = 1, i;
	for (i = 0; i < comps.length; i++) {
		if (comps[i].hs > hmax) hmax = comps[i].hs;
		if (comps[i].vs > vmax) vmax = comps[i].vs;
	}
	var mcux = Math.ceil(frame.w / (8 * hmax)), mcuy = Math.ceil(frame.h / (8 * vmax));
	frame.hmax = hmax;
	frame.vmax = vmax;
	for (i = 0; i < comps.length; i++) {
		var c = comps[i];
		c.bw = mcux * c.hs;
		c.bh = mcuy * c.vs;
		c.pw = c.bw * N;
		c.ph = c.bh * N;
		if (!c.plane)
			c.plane = new Uint8Array(c.pw * c.ph);
		c.pred = 0;
	}
	var bitbuf = 0, bitcnt = 0;
	var coef = new Int32Array(64);
	var tmp = new Float64Array(64);
	var dconly = (N == 1);

	function fill()
	{
		while (bitcnt <= 16) {
			var b = 0;
			if (pos < len) {
				b = data[pos];
				if (b == 0xff) {
					if (data[pos + 1] == 0)
						pos += 2;
					else
						b = 0;	/* marker: feed zeros, don't advance */
				}
				else
					pos++;
			}
			bitbuf = (bitbuf << 8) | b;
			bitcnt += 8;
		}
	}

	function decode_block(c, bx, by)
	{
		var q = qt[c.tq];
		var dct = hts.dc[c.dc], act = hts.ac[c.ac];
		var e, l, s, r, k, v;
		/* DC coefficient */
		if (bitcnt < 16)
			fill();
		e = dct[(bitbuf >> (bitcnt - 16)) & 0xffff];
		l = e >> 8;
		bitcnt -= l ? l : 1;
		s = e & 255;
		if (s) {
			if (bitcnt < s)
				fill();
			v = (bitbuf >> (bitcnt - s)) & ((1 << s) - 1);
			bitcnt -= s;
			c.pred += v < (1 << (s - 1)) ? v - (1 << s) + 1 : v;
		}
		var dc = c.pred * q[0];
		var uc = 0, vc = 0;
		k = 1;
		if (dconly) {
			/* skip the AC coefficients without storing them */
			while (k < 64) {
				if (bitcnt < 16)
					fill();
				e = act[(bitbuf >> (bitcnt - 16)) & 0xffff];
				l = e >> 8;
				bitcnt -= l ? l : 1;
				var rs0 = e & 255;
				s = rs0 & 15;
				if (s == 0) {
					if (rs0 == 0xf0) {
						k += 16;
						continue;
					}
					break;
				}
				k += (rs0 >> 4) + 1;
				if (bitcnt < s)
					fill();
				bitcnt -= s;
			}
		}
		else {
			while (k < 64) {
				if (bitcnt < 16)
					fill();
				e = act[(bitbuf >> (bitcnt - 16)) & 0xffff];
				l = e >> 8;
				bitcnt -= l ? l : 1;
				var rs = e & 255;
				s = rs & 15;
				if (s == 0) {
					if (rs == 0xf0) {
						k += 16;
						continue;
					}
					break;
				}
				k += rs >> 4;
				if (k > 63)
					break;
				if (bitcnt < s)
					fill();
				v = (bitbuf >> (bitcnt - s)) & ((1 << s) - 1);
				bitcnt -= s;
				if (v < (1 << (s - 1)))
					v -= (1 << s) - 1;
				var zi = JPEG_ZZ[k];
				coef[zi] = v * q[k];
				var cl = (zi & 7) + 1, rw = (zi >> 3) + 1;
				if (cl > uc)
					uc = cl;
				if (rw > vc)
					vc = rw;
				k++;
			}
		}
		var plane = c.plane, pw = c.pw;
		var ox = bx * N, oy = by * N;
		var x, y, u, vv, sum, o;
		if (uc == 0 || N == 1) {
			var dcv = dc / 8 + 128;
			var dcb = dcv < 0 ? 0 : (dcv > 255 ? 255 : dcv + 0.5);
			for (y = 0; y < N; y++) {
				o = (oy + y) * pw + ox;
				for (x = 0; x < N; x++)
					plane[o + x] = dcb;
			}
			if (uc) {
				for (vv = 0; vv < vc; vv++)
					for (u = 0; u < uc; u++)
						coef[vv * 8 + u] = 0;
			}
			return;
		}
		coef[0] = dc;
		var ucl = uc < N ? uc : N, vcl = vc < N ? vc : N;
		if (vcl < 1)
			vcl = 1;
		if (ucl < 1)
			ucl = 1;
		/* pass 1: along rows (u -> x) for the rows that have data */
		for (vv = 0; vv < vcl; vv++) {
			var rb = vv * 8;
			for (x = 0; x < N; x++) {
				sum = 0;
				for (u = 0; u < ucl; u++)
					sum += T[x * N + u] * coef[rb + u];
				tmp[vv * N + x] = sum;
			}
		}
		/* pass 2: along columns (v -> y) */
		for (y = 0; y < N; y++) {
			o = (oy + y) * pw + ox;
			for (x = 0; x < N; x++) {
				sum = 128;
				for (vv = 0; vv < vcl; vv++)
					sum += T[y * N + vv] * tmp[vv * N + x];
				plane[o + x] = sum < 0 ? 0 : (sum > 255 ? 255 : sum + 0.5);
			}
		}
		var uf = uc, vf = vc;
		for (vv = 0; vv < vf; vv++)
			for (u = 0; u < uf; u++)
				coef[vv * 8 + u] = 0;
		coef[0] = 0;
	}

	var mcu_count = 0;
	var x, y, ci;
	if (scomps.length == 1) {
		/* non-interleaved: blocks cover the component's own dimensions */
		var sc = scomps[0];
		var bw = Math.ceil(Math.ceil(frame.w * sc.hs / hmax) / 8), bh = Math.ceil(Math.ceil(frame.h * sc.vs / vmax) / 8);
		for (y = 0; y < bh; y++) {
			if (abort && abort())
				throw "aborted";
			for (x = 0; x < bw; x++) {
				if (restart && mcu_count && (mcu_count % restart) == 0) {
					bitcnt = 0;
					bitbuf = 0;
					while (pos < len && !(data[pos] == 0xff && data[pos + 1] >= 0xd0 && data[pos + 1] <= 0xd7))
						pos++;
					pos += 2;
					sc.pred = 0;
				}
				decode_block(sc, x, y);
				mcu_count++;
			}
		}
		return pos;
	}
	for (var my = 0; my < mcuy; my++) {
		if (abort && abort())
			throw "aborted";
		for (var mx = 0; mx < mcux; mx++) {
			if (restart && mcu_count && (mcu_count % restart) == 0) {
				bitcnt = 0;
				bitbuf = 0;
				while (pos < len && !(data[pos] == 0xff && data[pos + 1] >= 0xd0 && data[pos + 1] <= 0xd7))
					pos++;
				pos += 2;
				for (ci = 0; ci < scomps.length; ci++)
					scomps[ci].pred = 0;
			}
			for (ci = 0; ci < scomps.length; ci++) {
				var cc = scomps[ci];
				for (var vv2 = 0; vv2 < cc.vs; vv2++)
					for (var hh = 0; hh < cc.hs; hh++)
						decode_block(cc, mx * cc.hs + hh, my * cc.vs + vv2);
			}
			mcu_count++;
			if (pos >= len && bitcnt <= 0)
				return pos;
		}
	}
	return pos;
}

var YCC_TABLES = null;

function jpeg_output(frame, comps, adobe, N)
{
	var w = Math.ceil(frame.w * N / 8), h = Math.ceil(frame.h * N / 8);
	var nc = comps.length;
	var outn = nc == 1 ? 1 : (nc == 3 ? 3 : 4);
	var out = new Uint8Array(w * h * outn);
	var hmax = frame.hmax, vmax = frame.vmax;
	var x, y, c, i;
	var xmap = [], ymap = [];
	for (c = 0; c < nc; c++) {
		var xm = new Int32Array(w), ym = new Int32Array(h);
		for (x = 0; x < w; x++)
			xm[x] = Math.min(comps[c].pw - 1, Math.floor(x * comps[c].hs / hmax));
		for (y = 0; y < h; y++)
			ym[y] = Math.min(comps[c].ph - 1, Math.floor(y * comps[c].vs / vmax)) * comps[c].pw;
		xmap.push(xm);
		ymap.push(ym);
	}
	var transform = nc == 3 ? (adobe == 0 ? 0 : 1) : (adobe == 2 ? 1 : 0);
	if (nc == 3 && adobe < 0 && comps[0].id == 0x52 && comps[1].id == 0x47 && comps[2].id == 0x42)
		transform = 0;
	if (nc == 1) {
		var pl = comps[0].plane;
		var o1 = 0;
		for (y = 0; y < h; y++) {
			var yb = ymap[0][y], xm0 = xmap[0];
			for (x = 0; x < w; x++)
				out[o1++] = pl[yb + xm0[x]];
		}
		return { w: w, h: h, n: 1, data: out };
	}
	if (!YCC_TABLES) {
		var t = { crr: new Int32Array(256), cbb: new Int32Array(256), crg: new Int32Array(256), cbg: new Int32Array(256), clamp: new Uint8Array(768) };
		for (i = 0; i < 256; i++) {
			t.crr[i] = Math.round(1.402 * (i - 128));
			t.cbb[i] = Math.round(1.772 * (i - 128));
			t.crg[i] = Math.round(-0.714136 * (i - 128) * 65536);
			t.cbg[i] = Math.round(-0.344136 * (i - 128) * 65536);
		}
		for (i = 0; i < 768; i++)
			t.clamp[i] = i < 256 ? 0 : (i < 512 ? i - 256 : 255);
		YCC_TABLES = t;
	}
	var crr = YCC_TABLES.crr, cbb = YCC_TABLES.cbb, crg = YCC_TABLES.crg, cbg = YCC_TABLES.cbg, clamp = YCC_TABLES.clamp;
	var p0 = comps[0].plane, p1 = comps[1].plane, p2 = comps[2].plane;
	var p3 = nc == 4 ? comps[3].plane : null;
	var xm0b = xmap[0], xm1b = xmap[1], xm2b = xmap[2];
	var o = 0;
	for (y = 0; y < h; y++) {
		var y0 = ymap[0][y], y1 = ymap[1][y], y2 = ymap[2][y], y3 = nc == 4 ? ymap[3][y] : 0;
		for (x = 0; x < w; x++) {
			var a = p0[y0 + xm0b[x]];
			var b = p1[y1 + xm1b[x]];
			var cc = p2[y2 + xm2b[x]];
			if (transform) {
				var Y = a;
				var r = clamp[256 + Y + crr[cc]];
				var g = clamp[256 + Y + ((cbg[b] + crg[cc]) >> 16)];
				var bl = clamp[256 + Y + cbb[b]];
				if (nc == 4) {
					r = 255 - r;
					g = 255 - g;
					bl = 255 - bl;
				}
				a = r;
				b = g;
				cc = bl;
			}
			out[o++] = a;
			out[o++] = b;
			out[o++] = cc;
			if (nc == 4)
				out[o++] = p3[y3 + xmap[3][x]];
		}
	}
	return { w: w, h: h, n: outn, data: out };
}

/* ----- PDF image XObjects / inline images ----- */

var IMG_FAIL = {};

function image_placeholder(w, h, why)
{
	if (why)
		IMG_FAIL[why] = (IMG_FAIL[why] || 0) + 1;
	var d = new Uint8Array(1);
	d[0] = 205;
	return { w: 1, h: 1, n: 1, data: d, alpha: null, mask: false, placeholder: true };
}

/* Decode the 'bits per component' sample stream into 8-bit samples, one byte per component.
 * Returns a Uint8Array of w*h*ncomp values scaled to 0..255 (or raw index values for indexed). */
function unpack_samples(data, w, h, ncomp, bpc, raw)
{
	var rowbytes = Math.ceil(w * ncomp * bpc / 8);
	var total = w * h * ncomp;
	var out = new Uint8Array(total);
	var x, y, k;
	if (bpc == 8) {
		var lim = Math.min(total, data.length);
		for (k = 0; k < lim; k++)
			out[k] = data[k];
		return out;
	}
	var perrow = w * ncomp;
	var maxv = (1 << bpc) - 1;
	var scale = raw ? 1 : 255 / maxv;
	for (y = 0; y < h; y++) {
		var ro = y * rowbytes;
		var oo = y * perrow;
		if (bpc == 1) {
			for (x = 0; x < perrow; x++) {
				var byt = data[ro + (x >> 3)];
				out[oo + x] = ((byt >> (7 - (x & 7))) & 1) * (raw ? 1 : 255);
			}
		}
		else if (bpc == 16) {
			for (x = 0; x < perrow; x++)
				out[oo + x] = data[ro + x * 2] || 0;
		}
		else {
			var per = 8 / bpc;
			for (x = 0; x < perrow; x++) {
				var byt2 = data[ro + Math.floor(x / per)] || 0;
				var sh = 8 - bpc * ((x % per) + 1);
				out[oo + x] = ((byt2 >> sh) & maxv) * scale;
			}
		}
	}
	return out;
}

/* desired: approximate number of device pixels across the image (for JPEG scaled decoding) */
function load_image_raw(doc, st, res, desired, maxN)
{
	var dict = st.dict;
	var w = doc.resolve(dict.Width), h = doc.resolve(dict.Height);
	if (typeof w != "number" || typeof h != "number" || w < 1 || h < 1)
		return null;
	var ismask = doc.resolve(dict.ImageMask) === true;
	var dec = st.decode();
	var data = dec.data;
	var filter = dec.filter;
	if (filter == "DCT")
		filter = "DCTDecode";
	var bpc = ismask ? 1 : (doc.resolve(dict.BitsPerComponent) || 8);
	var cs = ismask ? CS_GRAY : make_cs(doc, dict.ColorSpace, res);
	var decode = num_array(doc, dict.Decode);
	var img = null;
	var i, x, y, k;

	if (filter == "DCTDecode") {
		var ratio = desired > 0 ? w / desired : 1;
		var Nneed = ratio >= 6 ? 1 : (ratio >= 3 ? 2 : (ratio >= 1.5 ? 4 : 8));
		var N = Math.min(Nneed, maxN || 8);
		if (N < Nneed && RS)
			RS.degraded = true;
		if (st._jpeg && st._jpegN >= N)
			img = st._jpeg;
		else {
			var j = null;
			try {
				j = jpeg_decode(data, N, RS ? RS.abort : null);
			}
			catch (e) {
				if (e === "aborted")
					throw e;
				j = null;
			}
			if (!j)
				return image_placeholder(w, h, "jpeg");
			if (j.n == 4) {
				/* CMYK samples -> RGB (honouring /Decode) */
				var rgb = new Uint8Array(j.w * j.h * 3);
				var inv = decode && decode[0] > decode[1];
				for (i = 0, k = 0; i < j.w * j.h; i++) {
					var c0 = j.data[i * 4], m0 = j.data[i * 4 + 1], y0 = j.data[i * 4 + 2], k0 = j.data[i * 4 + 3];
					if (inv) {
						c0 = 255 - c0;
						m0 = 255 - m0;
						y0 = 255 - y0;
						k0 = 255 - k0;
					}
					/* c0 etc are now ink amounts */
					rgb[k++] = (255 - c0) * (255 - k0) / 255;
					rgb[k++] = (255 - m0) * (255 - k0) / 255;
					rgb[k++] = (255 - y0) * (255 - k0) / 255;
				}
				img = { w: j.w, h: j.h, n: 3, data: rgb, alpha: null, mask: false };
			}
			else {
				img = { w: j.w, h: j.h, n: j.n, data: j.data, alpha: null, mask: false };
				if (j.n == 1 && cs.n == 1 && cs.rgb !== CS_GRAY.rgb) {
					/* indexed / separation applied to a gray JPEG: map through the colour space */
					var rgb1 = new Uint8Array(j.w * j.h * 3);
					for (i = 0; i < j.w * j.h; i++) {
						var cc = cs.rgb([j.data[i] / 255]);
						rgb1[i * 3] = cc[0];
						rgb1[i * 3 + 1] = cc[1];
						rgb1[i * 3 + 2] = cc[2];
					}
					img = { w: j.w, h: j.h, n: 3, data: rgb1, alpha: null, mask: false };
				}
				else if (j.n == 1 && decode && decode[0] > decode[1]) {
					for (i = 0; i < j.data.length; i++)
						j.data[i] = 255 - j.data[i];
				}
			}
			st._jpeg = img;
			st._jpegN = N;
		}
	}
	else if (filter) {
		return image_placeholder(w, h, filter);
	}
	else if (ismask) {
		var bits = unpack_samples(data, w, h, 1, 1, true);
		var invert = decode && decode[0] == 1;
		var cov = new Uint8Array(w * h);
		for (i = 0; i < cov.length; i++)
			cov[i] = ((bits[i] ? 1 : 0) != (invert ? 1 : 0)) ? 0 : 255;
		img = { w: w, h: h, n: 1, data: cov, alpha: null, mask: true };
	}
	else {
		var nc = cs.n;
		if (cs.pattern || nc < 1)
			return null;
		var indexed = !!cs.indexed;
		var raw = unpack_samples(data, w, h, nc, bpc, indexed || cs.n != 1 && cs.n != 3 && cs.n != 4 ? true : false);
		var maxv = (1 << Math.min(bpc, 8)) - 1;
		var keymask = doc.resolve(dict.Mask);
		var alpha = null;
		if (keymask instanceof Array)
			keymask = num_array(doc, keymask);
		else
			keymask = null;
		if (cs === CS_GRAY && !keymask) {
			if (decode && decode[0] > decode[1])
				for (i = 0; i < raw.length; i++)
					raw[i] = 255 - raw[i];
			img = { w: w, h: h, n: 1, data: raw, alpha: null, mask: false };
		}
		else if (cs === CS_RGB && !keymask) {
			if (decode && decode[0] > decode[1])
				for (i = 0; i < raw.length; i++)
					raw[i] = 255 - raw[i];
			img = { w: w, h: h, n: 3, data: raw, alpha: null, mask: false };
		}
		else if (cs === CS_CMYK && !keymask && !decode && bpc == 8) {
			var cm = new Uint8Array(w * h * 3);
			for (i = 0, k = 0; i < w * h; i++) {
				var kk = 255 - raw[i * 4 + 3];
				cm[k++] = (255 - raw[i * 4]) * kk / 255;
				cm[k++] = (255 - raw[i * 4 + 1]) * kk / 255;
				cm[k++] = (255 - raw[i * 4 + 2]) * kk / 255;
			}
			img = { w: w, h: h, n: 3, data: cm, alpha: null, mask: false };
		}
		else {
			var out = new Uint8Array(w * h * 3);
			var cache = {};
			var comps = [];
			var dmin = [], dmax = [];
			for (k = 0; k < nc; k++) {
				dmin[k] = decode ? decode[2 * k] : 0;
				dmax[k] = decode ? decode[2 * k + 1] : 1;
			}
			if (keymask && bpc <= 8)
				alpha = new Uint8Array(w * h);
			var o = 0;
			for (i = 0; i < w * h; i++) {
				var key = 0;
				for (k = 0; k < nc; k++)
					key = key * 256 + raw[i * nc + k];
				var col = nc <= 3 ? cache[key] : undefined;
				if (!col) {
					for (k = 0; k < nc; k++) {
						var s = raw[i * nc + k];
						if (indexed && !decode)
							comps[k] = s;
						else if (indexed)
							comps[k] = dmin[k] + s * (dmax[k] - dmin[k]) / maxv;
						else {
							var f = bpc == 8 || bpc == 16 ? s / 255 : s / 255;
							comps[k] = dmin[k] + f * (dmax[k] - dmin[k]);
							if (cs.lab) {
								comps[0] = comps[0] * (nc == 3 ? 100 : 1);
							}
						}
					}
					col = cs.rgb(comps);
					if (nc <= 3)
						cache[key] = col;
				}
				out[o++] = col[0];
				out[o++] = col[1];
				out[o++] = col[2];
				if (alpha) {
					var masked = true;
					for (k = 0; k < nc; k++) {
						var rv = bpc == 8 || indexed ? raw[i * nc + k] : Math.floor(raw[i * nc + k] * maxv / 255 + 0.5);
						if (rv < keymask[2 * k] || rv > keymask[2 * k + 1]) {
							masked = false;
							break;
						}
					}
					alpha[i] = masked ? 0 : 255;
				}
			}
			img = { w: w, h: h, n: 3, data: out, alpha: alpha, mask: false };
		}
	}

	/* soft mask / stencil mask */
	if (!img.mask && img.alpha === null) {
		var sm = doc.resolve(dict.SMask);
		var mk = doc.resolve(dict.Mask);
		var mimg = null;
		if (sm instanceof PDFStream)
			mimg = load_image(doc, sm, res, desired, 8);
		else if (mk instanceof PDFStream)
			mimg = load_image(doc, mk, res, desired, 8);
		if (mimg && !mimg.placeholder) {
			var am = new Uint8Array(img.w * img.h);
			for (y = 0; y < img.h; y++) {
				var sy = Math.min(mimg.h - 1, Math.floor(y * mimg.h / img.h));
				for (x = 0; x < img.w; x++) {
					var sx = Math.min(mimg.w - 1, Math.floor(x * mimg.w / img.w));
					am[y * img.w + x] = mimg.data[(sy * mimg.w + sx) * mimg.n];
				}
			}
			/* make a shallow copy so the cached decode of a shared JPEG isn't altered */
			img = { w: img.w, h: img.h, n: img.n, data: img.data, alpha: am, mask: false };
		}
	}
	return img;
}

var IMG_CACHE = [];
var IMG_CACHE_BYTES = 0;
var IMG_CACHE_LIMIT = 80 * 1024 * 1024;

/* Decode an image XObject (cached). JPEGs are decoded at a reduced size when they will be
 * displayed small; maxN caps the detail level (1, 2, 4 or 8 pixels per 8x8 block). */
function load_image(doc, st, res, desired, maxN)
{
	var w = doc.resolve(st.dict.Width);
	var f = st.decode().filter;
	var isdct = (f == "DCTDecode" || f == "DCT");
	var N = 8;
	if (isdct) {
		var ratio = desired > 0 ? w / desired : 1;
		var Nneed = ratio >= 6 ? 1 : (ratio >= 3 ? 2 : (ratio >= 1.5 ? 4 : 8));
		N = Math.min(Nneed, maxN || 8);
	}
	if (st._img && st._imgN >= N)
		return st._img;
	var img = load_image_raw(doc, st, res, desired, maxN);
	if (img && !img.placeholder) {
		st._img = img;
		st._imgN = N;
		IMG_CACHE.push(st);
		IMG_CACHE_BYTES += img.data.length + (img.alpha ? img.alpha.length : 0);
		if (IMG_CACHE_BYTES > IMG_CACHE_LIMIT) {
			for (var i = 0; i < IMG_CACHE.length; i++) {
				if (IMG_CACHE[i] !== st) {
					IMG_CACHE[i]._img = null;
					IMG_CACHE[i]._jpeg = null;
				}
			}
			IMG_CACHE = [st];
			IMG_CACHE_BYTES = img.data.length;
		}
	}
	return img;
}

/* ======================================================================= */
/* Part 5: content stream interpreter                                       */
/* ======================================================================= */

var RS = null;		/* current render state */
var LAST_DEGRADED = false;	/* true if the last render used reduced image quality */

var INLINE_KEYS = { BPC: "BitsPerComponent", CS: "ColorSpace", D: "Decode", DP: "DecodeParms", F: "Filter",
	H: "Height", IM: "ImageMask", I: "Interpolate", W: "Width" };

function find_ei(bytes, p)
{
	var n = bytes.length;
	for (var i = p; i + 1 < n; i++) {
		if (bytes[i] == 0x45 && bytes[i + 1] == 0x49 && (i == p || LEX_CLASS[bytes[i - 1]] == 1)
			&& (i + 2 >= n || LEX_CLASS[bytes[i + 2]] == 1))
			return i;
	}
	return n;
}

/* Tokenise a content stream into [[operator, [operands]], ...] */
function parse_content(doc, bytes)
{
	var lx = new Lexer(bytes, 0);
	var ops = [];
	var args = [];
	for (;;) {
		var t = lx.token();
		if (t === EOF_TOKEN)
			break;
		var v = parse_value(lx, t, true);
		if (!(v instanceof Cmd)) {
			args.push(v);
			if (args.length > 512)
				args.shift();
			continue;
		}
		var op = v.cmd;
		if (op == "BI") {
			var d = {};
			for (;;) {
				var k = lx.token();
				if (k === EOF_TOKEN || ((k instanceof Cmd) && k.cmd == "ID"))
					break;
				if (!(k instanceof Name))
					continue;
				var val = parse_value(lx, lx.token(), true);
				d[INLINE_KEYS[k.name] || k.name] = val;
			}
			var p = lx.pos;
			if (bytes[p] == 13 && bytes[p + 1] == 10)
				p += 2;
			else if (LEX_CLASS[bytes[p]] == 1)
				p++;
			var e;
			if (d.Filter === undefined) {
				var iw = d.Width || 0, ih = d.Height || 0, bpc = d.BitsPerComponent || 8, nc = 1;
				if (d.ImageMask === true)
					bpc = 1;
				else {
					var csn = d.ColorSpace;
					if (isName(csn)) {
						if (csn.name == "RGB" || csn.name == "DeviceRGB")
							nc = 3;
						else if (csn.name == "CMYK" || csn.name == "DeviceCMYK")
							nc = 4;
					}
				}
				var want = ih * Math.ceil(iw * nc * bpc / 8);
				e = find_ei(bytes, Math.min(bytes.length, p + want));
				if (e - p < want)
					e = find_ei(bytes, p);
			}
			else
				e = find_ei(bytes, p);
			var len = e - p;
			var st = new PDFStream(doc, d, p, len);
			st.buf = bytes;
			ops.push(["BI", [st]]);
			lx.pos = Math.min(bytes.length, e + 2);
			args = [];
			continue;
		}
		ops.push([op, args]);
		args = [];
	}
	return ops;
}

function page_ops(doc, page)
{
	if (page.ops)
		return page.ops;
	var c = doc.resolve(page.dict.Contents);
	var list = (c instanceof Array) ? c : [c];
	var parts = [], total = 0, i;
	for (i = 0; i < list.length; i++) {
		var s = doc.resolve(list[i]);
		if (s instanceof PDFStream) {
			var b = s.bytes();
			parts.push(b);
			total += b.length + 1;
		}
	}
	var all = new Uint8Array(total);
	var o = 0;
	for (i = 0; i < parts.length; i++) {
		all.set(parts[i], o);
		o += parts[i].length;
		all[o++] = 10;
	}
	page.ops = parse_content(doc, all);
	return page.ops;
}

/* ----- graphics state ----- */

function new_gs(ctm, clip)
{
	return {
		ctm: ctm, clip: clip, mask: null,
		fcs: CS_GRAY, scs: CS_GRAY, fcol: [0], scol: [0], frgb: [0, 0, 0], srgb: [0, 0, 0],
		lw: 1, cap: 0, join: 0, miter: 10, dash: null, dphase: 0, fa: 1, sa: 1,
		font: null, fs: 0, tc: 0, tw: 0, th: 1, tl: 0, trise: 0, tmode: 0, fpat: null
	};
}

function clone_gs(g)
{
	var n = {};
	for (var k in g)
		n[k] = g[k];
	return n;
}

function num(v)
{
	return typeof v == "number" ? v : 0;
}

function res_get(res, category, name)
{
	if (!isDict(res))
		return undefined;
	var d = RS.doc.resolve(res[category]);
	if (!isDict(d))
		return undefined;
	return RS.doc.resolve(d[name]);
}

/* ----- optional content ----- */

function ocg_off_set(doc)
{
	if (doc._ocoff)
		return doc._ocoff;
	var off = {};
	try {
		var root = doc.resolve(doc.trailer.Root);
		var ocp = doc.resolve(root.OCProperties);
		var d = isDict(ocp) ? doc.resolve(ocp.D) : null;
		if (isDict(d)) {
			var offs = doc.resolve(d.OFF);
			if (offs instanceof Array)
				for (var i = 0; i < offs.length; i++)
					if (offs[i] instanceof Ref)
						off[offs[i].num] = true;
			if (isName(doc.resolve(d.BaseState), "OFF")) {
				off._all = true;
				var on = doc.resolve(d.ON);
				off._on = {};
				if (on instanceof Array)
					for (var j = 0; j < on.length; j++)
						if (on[j] instanceof Ref)
							off._on[on[j].num] = true;
			}
		}
	}
	catch (e) { }
	doc._ocoff = off;
	return off;
}

function ocg_ref_hidden(doc, ref)
{
	var off = ocg_off_set(doc);
	if (!(ref instanceof Ref))
		return false;
	if (off._all)
		return !off._on[ref.num];
	return !!off[ref.num];
}

/* v: a raw (unresolved) /OC value; returns true if the content is hidden */
function oc_hidden(doc, v)
{
	var d = doc.resolve(v);
	if (!isDict(d))
		return false;
	if (isName(doc.resolve(d.Type), "OCMD")) {
		var g = d.OCGs;
		var gl = doc.resolve(g);
		if (gl instanceof Array) {
			if (gl.length == 0)
				return false;
			for (var i = 0; i < gl.length; i++)
				if (!ocg_ref_hidden(doc, g[i]))
					return false;
			return true;
		}
		return ocg_ref_hidden(doc, g);
	}
	return ocg_ref_hidden(doc, v);
}

/* ----- path helpers ----- */

function path_move(x, y)
{
	var m = RS.gs.ctm;
	var px = m[0] * x + m[2] * y + m[4], py = m[1] * x + m[3] * y + m[5];
	var sp = { pts: [px, py], closed: false };
	RS.path.push(sp);
	RS.cur = sp;
	RS.cx = px;
	RS.cy = py;
}

function path_line(x, y)
{
	if (!RS.cur) {
		path_move(x, y);
		return;
	}
	var m = RS.gs.ctm;
	var px = m[0] * x + m[2] * y + m[4], py = m[1] * x + m[3] * y + m[5];
	RS.cur.pts.push(px, py);
	RS.cx = px;
	RS.cy = py;
}

function path_curve(x1, y1, x2, y2, x3, y3)
{
	if (!RS.cur)
		path_move(x1, y1);
	var m = RS.gs.ctm;
	var ax = m[0] * x1 + m[2] * y1 + m[4], ay = m[1] * x1 + m[3] * y1 + m[5];
	var bx = m[0] * x2 + m[2] * y2 + m[4], by = m[1] * x2 + m[3] * y2 + m[5];
	var cx = m[0] * x3 + m[2] * y3 + m[4], cy = m[1] * x3 + m[3] * y3 + m[5];
	flatten_curve(RS.cur.pts, RS.cx, RS.cy, ax, ay, bx, by, cx, cy);
	RS.cx = cx;
	RS.cy = cy;
}

function path_close()
{
	if (RS.cur && RS.cur.pts.length >= 2) {
		RS.cur.closed = true;
		RS.cx = RS.cur.pts[0];
		RS.cy = RS.cur.pts[1];
		/* a new subpath implicitly starts at the same point */
		var sp = { pts: [RS.cx, RS.cy], closed: false };
		RS.path.push(sp);
		RS.cur = sp;
	}
}

function path_polys()
{
	var polys = [];
	for (var i = 0; i < RS.path.length; i++)
		if (RS.path[i].pts.length >= 4)
			polys.push(RS.path[i].pts);
	return polys;
}

function path_is_rect(path)
{
	var subs = [];
	for (var i = 0; i < path.length; i++)
		if (path[i].pts.length >= 4)
			subs.push(path[i]);
	if (subs.length != 1)
		return null;
	var p = subs[0].pts;
	var n = p.length / 2;
	if (n == 5 && Math.abs(p[0] - p[8]) < 1e-3 && Math.abs(p[1] - p[9]) < 1e-3)
		n = 4;
	if (n != 4)
		return null;
	var e = 1e-3;
	var ok = (Math.abs(p[0] - p[2]) < e && Math.abs(p[3] - p[5]) < e && Math.abs(p[4] - p[6]) < e && Math.abs(p[7] - p[1]) < e)
		|| (Math.abs(p[1] - p[3]) < e && Math.abs(p[2] - p[4]) < e && Math.abs(p[5] - p[7]) < e && Math.abs(p[6] - p[0]) < e);
	if (!ok)
		return null;
	return [Math.min(p[0], p[2], p[4], p[6]), Math.min(p[1], p[3], p[5], p[7]),
		Math.max(p[0], p[2], p[4], p[6]), Math.max(p[1], p[3], p[5], p[7])];
}

function intersect_rect(a, b)
{
	return [Math.max(a[0], b[0]), Math.max(a[1], b[1]), Math.min(a[2], b[2]), Math.min(a[3], b[3])];
}

function apply_clip(evenodd)
{
	var gs = RS.gs;
	var path = RS.path;
	var polys = path_polys();
	if (polys.length == 0) {
		gs.clip = [0, 0, 0, 0];
		return;
	}
	var rect = path_is_rect(path);
	if (rect) {
		gs.clip = intersect_rect(gs.clip, rect);
		return;
	}
	var minx = 1e9, miny = 1e9, maxx = -1e9, maxy = -1e9;
	for (var i = 0; i < polys.length; i++) {
		var p = polys[i];
		for (var j = 0; j < p.length; j += 2) {
			if (p[j] < minx) minx = p[j];
			if (p[j] > maxx) maxx = p[j];
			if (p[j + 1] < miny) miny = p[j + 1];
			if (p[j + 1] > maxy) maxy = p[j + 1];
		}
	}
	var bbox = intersect_rect(gs.clip, [minx, miny, maxx, maxy]);
	gs.clip = bbox;
	if (bbox[2] <= bbox[0] || bbox[3] <= bbox[1])
		return;
	var mask = new Uint8Array(FW * FH);
	var savemask = CLIPMASK;
	CLIPMASK = null;
	COVTARGET = mask;
	fill_polys(polys, evenodd, [0, 0, 0], 1, bbox);
	COVTARGET = null;
	CLIPMASK = savemask;
	if (gs.mask) {
		var old = gs.mask;
		var y0 = Math.max(0, Math.floor(bbox[1])), y1 = Math.min(FH, Math.ceil(bbox[3]));
		var x0 = Math.max(0, Math.floor(bbox[0])), x1 = Math.min(FW, Math.ceil(bbox[2]));
		for (var y = y0; y < y1; y++)
			for (var x = x0; x < x1; x++)
				mask[y * FW + x] = mask[y * FW + x] * old[y * FW + x] / 255;
	}
	gs.mask = mask;
}

function paint_path(dofill, dostroke, evenodd)
{
	var gs = RS.gs;
	CLIPMASK = gs.mask;
	if (!RS.hidden) {
		var polys;
		if (dofill && !gs.fcs.none) {
			polys = path_polys();
			if (polys.length) {
				if (gs.fpat)
					fill_with_shading(polys, evenodd, gs.fpat);
				else
					fill_polys(polys, evenodd, gs.frgb, gs.fa, gs.clip);
			}
		}
		if (dostroke && !gs.scs.none)
			stroke_current_path();
	}
	if (RS.pending) {
		apply_clip(RS.pending == 2);
		RS.pending = 0;
	}
	RS.path = [];
	RS.cur = null;
}

function stroke_current_path()
{
	var gs = RS.gs;
	var sc = mat_scale(gs.ctm);
	var lw = gs.lw * sc;
	if (lw < 1)
		lw = 1;
	var hw = lw / 2;
	var polys = [];
	var dash = null;
	if (gs.dash && gs.dash.length) {
		dash = [];
		for (var i = 0; i < gs.dash.length; i++)
			dash.push(Math.max(0, gs.dash[i] * sc));
		var tot = 0;
		for (i = 0; i < dash.length; i++)
			tot += dash[i];
		if (tot < 0.05)
			dash = null;
	}
	for (var s = 0; s < RS.path.length; s++) {
		var sub = RS.path[s];
		if (sub.pts.length < 2 || (sub.pts.length == 2 && !sub.closed && RS.path.length > 1))
			continue;
		if (dash) {
			var parts = dash_subpath(sub, dash, gs.dphase * sc);
			for (var k = 0; k < parts.length; k++)
				stroke_subpath(parts[k], hw, gs.cap, gs.join, gs.miter, polys);
		}
		else
			stroke_subpath(sub, hw, gs.cap, gs.join, gs.miter, polys);
	}
	if (polys.length)
		fill_polys(polys, false, gs.srgb, gs.sa, gs.clip);
}

/* ----- colours ----- */

function set_fill(comps)
{
	var gs = RS.gs;
	if (RS.nocolor)
		return;
	gs.fcol = comps;
	gs.fpat = null;
	if (!gs.fcs.pattern)
		gs.frgb = gs.fcs.rgb(comps);
}

function set_stroke(comps)
{
	var gs = RS.gs;
	if (RS.nocolor)
		return;
	gs.scol = comps;
	if (!gs.scs.pattern)
		gs.srgb = gs.scs.rgb(comps);
}

function nums(args, n)
{
	var r = [];
	for (var i = 0; i < n; i++)
		r.push(num(args[i]));
	return r;
}

function shading_color(sh)
{
	var doc = RS.doc;
	sh = doc.resolve(sh);
	var d = (sh instanceof PDFStream) ? sh.dict : sh;
	if (!isDict(d))
		return [176, 176, 176];
	var cs = make_cs(doc, d.ColorSpace, null);
	if (d.Function === undefined)
		return [176, 176, 176];
	var fn = make_function(doc, d.Function);
	var dom = num_array(doc, d.Domain) || [0, 1];
	try {
		return cs.rgb(fn.call([(dom[0] + dom[1]) / 2]));
	}
	catch (e) {
		return [176, 176, 176];
	}
}

/* Returns { rgb: approximate solid colour, pat: shading pattern or null } */
function pattern_info(res, name, comps, cs)
{
	var doc = RS.doc;
	var pat = res_get(res, "Pattern", name);
	var d = (pat instanceof PDFStream) ? pat.dict : pat;
	if (!isDict(d))
		return { rgb: [176, 176, 176], pat: null };
	var pt = doc.resolve(d.PatternType);
	if (pt == 2) {
		var sh = doc.resolve(d.Shading);
		var m = num_array(doc, d.Matrix) || [1, 0, 0, 1, 0, 0];
		var rgb = shading_color(d.Shading);
		var t = (sh instanceof PDFStream) ? doc.resolve(sh.dict.ShadingType) : (isDict(sh) ? doc.resolve(sh.ShadingType) : 0);
		return { rgb: rgb, pat: (t == 2 || t == 3) ? { sh: sh, m: m } : null };
	}
	return { rgb: pattern_rgb_tiling(d, comps, cs), pat: null };
}

function pattern_rgb_tiling(d, comps, cs)
{
	if (RS.doc.resolve(d.PaintType) == 2 && cs.base && comps.length)
		return cs.base.rgb(comps);
	return [200, 200, 200];
}

/* Paint an axial (2) or radial (3) shading. M maps shading space to device pixels; box is the
 * device-space bounding box to paint; cover is an optional Uint8Array coverage mask (a shape). */
function paint_shading(sh, M, box, cover, alpha)
{
	var doc = RS.doc;
	var gs = RS.gs;
	var d = (sh instanceof PDFStream) ? sh.dict : sh;
	if (!isDict(d))
		return;
	var type = doc.resolve(d.ShadingType);
	if (type != 2 && type != 3)
		return;
	var coords = num_array(doc, d.Coords);
	if (!coords)
		return;
	var cs = make_cs(doc, d.ColorSpace, null);
	var fn = make_function(doc, d.Function);
	var dom = num_array(doc, d.Domain) || [0, 1];
	var ext = doc.resolve(d.Extend) || [false, false];
	var e0 = doc.resolve(ext[0]) === true, e1 = doc.resolve(ext[1]) === true;
	var lut = [];
	for (var i = 0; i < 256; i++) {
		var t = dom[0] + (dom[1] - dom[0]) * i / 255;
		try {
			lut.push(cs.rgb(fn.call([t])));
		}
		catch (e) {
			lut.push([128, 128, 128]);
		}
	}
	var inv = mat_inv(M);
	if (!inv)
		return;
	if (type == 3 && !(doc.resolve(ext[0]) === true || doc.resolve(ext[1]) === true)) {
		/* bounded: clip the box to the two circles */
		var rmax = Math.max(coords[2], coords[5]);
		var bx0 = Math.min(coords[0], coords[3]) - rmax, bx1 = Math.max(coords[0], coords[3]) + rmax;
		var by0 = Math.min(coords[1], coords[4]) - rmax, by1 = Math.max(coords[1], coords[4]) + rmax;
		var cn = [mat_apply(M, bx0, by0), mat_apply(M, bx1, by0), mat_apply(M, bx1, by1), mat_apply(M, bx0, by1)];
		box = intersect_rect(box, [Math.min(cn[0][0], cn[1][0], cn[2][0], cn[3][0]), Math.min(cn[0][1], cn[1][1], cn[2][1], cn[3][1]),
			Math.max(cn[0][0], cn[1][0], cn[2][0], cn[3][0]), Math.max(cn[0][1], cn[1][1], cn[2][1], cn[3][1])]);
	}
	CLIPMASK = gs.mask;
	var x0 = Math.max(0, Math.floor(box[0])), y0 = Math.max(0, Math.floor(box[1]));
	var x1 = Math.min(FW, Math.ceil(box[2])), y1 = Math.min(FH, Math.ceil(box[3]));
	var cx = coords[0], cy = coords[1];
	var dx = coords[2] - coords[0], dy = coords[3] - coords[1];
	var den = dx * dx + dy * dy;
	var r0 = coords[2], dr = 0, cdx = 0, cdy = 0, qa = 0;
	if (type == 3) {
		cdx = coords[3] - coords[0];
		cdy = coords[4] - coords[1];
		dr = coords[5] - coords[2];
		qa = cdx * cdx + cdy * cdy - dr * dr;
	}
	for (var py = y0; py < y1; py++) {
		for (var px = x0; px < x1; px++) {
			var idx = py * FW + px;
			var aa = alpha;
			if (cover) {
				if (cover[idx] == 0)
					continue;
				aa *= cover[idx] * 0.00392157;
			}
			if (CLIPMASK)
				aa *= CLIPMASK[idx] * 0.00392157;
			if (aa <= 0.003)
				continue;
			var ux = inv[0] * (px + 0.5) + inv[2] * (py + 0.5) + inv[4];
			var uy = inv[1] * (px + 0.5) + inv[3] * (py + 0.5) + inv[5];
			var s;
			if (type == 2) {
				if (den == 0)
					continue;
				s = ((ux - cx) * dx + (uy - cy) * dy) / den;
			}
			else {
				var pdx = ux - cx, pdy = uy - cy;
				var b = pdx * cdx + pdy * cdy + r0 * dr;
				var c = pdx * pdx + pdy * pdy - r0 * r0;
				var found = false;
				if (Math.abs(qa) < 1e-9) {
					if (Math.abs(b) < 1e-9)
						continue;
					s = c / (2 * b);
					found = r0 + s * dr >= 0;
				}
				else {
					var disc = b * b - qa * c;
					if (disc < 0)
						continue;
					var sq = Math.sqrt(disc);
					var s1 = (b + sq) / qa, s2 = (b - sq) / qa;
					if (s1 < s2) {
						var tt = s1;
						s1 = s2;
						s2 = tt;
					}
					var cand = [s1, s2];
					for (var q = 0; q < 2 && !found; q++) {
						var sv = cand[q];
						if (r0 + sv * dr < 0)
							continue;
						if ((sv < 0 && !e0) || (sv > 1 && !e1))
							continue;
						s = sv;
						found = true;
					}
				}
				if (!found)
					continue;
			}
			if (s < 0) {
				if (!e0)
					continue;
				s = 0;
			}
			else if (s > 1) {
				if (!e1)
					continue;
				s = 1;
			}
			var col = lut[(s * 255 + 0.5) | 0];
			if (aa >= 0.996)
				FB[idx] = (col[0] << 16) | (col[1] << 8) | col[2];
			else {
				var pv = FB[idx];
				var pr = (pv >> 16) & 255, pg = (pv >> 8) & 255, pb = pv & 255;
				FB[idx] = ((pr + (col[0] - pr) * aa + 0.5) << 16) | ((pg + (col[1] - pg) * aa + 0.5) << 8) | ((pb + (col[2] - pb) * aa + 0.5) | 0);
			}
		}
	}
}

/* Fill polygons with a shading pattern: rasterise the shape to a coverage mask, then paint through it */
function fill_with_shading(polys, evenodd, pat)
{
	var gs = RS.gs;
	var minx = 1e9, miny = 1e9, maxx = -1e9, maxy = -1e9;
	for (var i = 0; i < polys.length; i++) {
		var p = polys[i];
		for (var j = 0; j < p.length; j += 2) {
			if (p[j] < minx) minx = p[j];
			if (p[j] > maxx) maxx = p[j];
			if (p[j + 1] < miny) miny = p[j + 1];
			if (p[j + 1] > maxy) maxy = p[j + 1];
		}
	}
	var box = intersect_rect(gs.clip, [minx, miny, maxx, maxy]);
	if (box[2] <= box[0] || box[3] <= box[1])
		return;
	var mask = new Uint8Array(FW * FH);
	var save = CLIPMASK;
	CLIPMASK = null;
	COVTARGET = mask;
	fill_polys(polys, evenodd, [0, 0, 0], 1, box);
	COVTARGET = null;
	CLIPMASK = save;
	paint_shading(pat.sh, mat_mul(pat.m, RS.patbase), box, mask, gs.fa);
}

/* ----- text ----- */

function draw_unknown_box(M, adv, rgb, alpha)
{
	var gs = RS.gs;
	var x0 = adv * 0.12, x1 = adv * 0.88, y0 = 0, y1 = 0.62;
	var pts = [];
	var c = [[x0, y0], [x1, y0], [x1, y1], [x0, y1]];
	for (var i = 0; i < 4; i++) {
		var p = mat_apply(M, c[i][0], c[i][1]);
		pts.push(p[0], p[1]);
	}
	var polys = [pts];
	fill_polys(polys, false, rgb, alpha * 0.25, gs.clip);
}

function draw_char(font, code, uni, w0)
{
	var gs = RS.gs;
	var trm = mat_mul([gs.fs * gs.th, 0, 0, gs.fs, 0, gs.trise], mat_mul(RS.tm, gs.ctm));
	var size = Math.abs(trm[0]) + Math.abs(trm[1]) + Math.abs(trm[2]) + Math.abs(trm[3]);
	var ox = trm[4], oy = trm[5];
	var cl = gs.clip;
	var reach = size * (font.kind == "type3" ? 40 : 3 + w0);
	if (ox < cl[0] - reach || ox > cl[2] + reach || oy < cl[1] - reach || oy > cl[3] + reach)
		return;
	if (size < 0.15)
		return;
	CLIPMASK = gs.mask;
	var strokeonly = gs.tmode == 1 || gs.tmode == 5;
	var rgb = strokeonly ? gs.srgb : gs.frgb;
	var alpha = strokeonly ? gs.sa : gs.fa;
	if (font.kind == "type3") {
		var name = font.type3.names[code];
		var proc = name ? RS.doc.resolve(font.type3.procs[name]) : null;
		if (!(proc instanceof PDFStream) || RS.depth > 8)
			return;
		var ops = proc._ops || (proc._ops = parse_content(RS.doc, proc.bytes()));
		var saved = RS.gs;
		var savestack = RS.stack.length;
		var savenocolor = RS.nocolor;
		var savepath = RS.path, savecur = RS.cur, savetm = RS.tm, savetlm = RS.tlm;
		RS.stack.push(saved);
		RS.gs = clone_gs(saved);
		RS.gs.ctm = mat_mul(font.type3.matrix, trm);
		RS.path = [];
		RS.cur = null;
		RS.depth++;
		exec_ops(ops, font.type3.res, 0);
		RS.depth--;
		while (RS.stack.length > savestack)
			RS.stack.pop();
		RS.gs = saved;
		RS.nocolor = savenocolor;
		RS.path = savepath;
		RS.cur = savecur;
		RS.tm = savetm;
		RS.tlm = savetlm;
		return;
	}
	if (font.desc && font.prog !== null) {
		var ol = null;
		try {
			ol = font_outline(RS.doc, font, code);
			if (ol) {
				if (ol.length)
					draw_outline_glyph(font, code, ol, trm, rgb, alpha, gs.clip);
				return;
			}
		}
		catch (e) {
			if (e === "aborted")
				throw e;
			font.prog = null;	/* give up on this font program; use the bitmap font */
			COVTARGET = null;
		}
	}
	if (uni === null || uni === undefined) {
		draw_unknown_box(trm, w0, rgb, alpha);
		return;
	}
	if (uni.length == 0)
		return;
	var bf = load_bitmap_font(font.family);
	var G = font.italic ? [1, 0, 0.2, 1, 0, 0] : null;
	var M0 = G ? mat_mul(G, trm) : trm;
	var adv = w0 / uni.length;
	for (var k = 0; k < uni.length; k++) {
		var u = uni.charCodeAt(k);
		var Mk = k ? mat_mul([1, 0, 0, 1, k * adv, 0], M0) : M0;
		var bp = builtin_path(font, u);
		if (bp) {
			if (bp.length)
				draw_outline_glyph(bp.fobj, u, bp, Mk, rgb, alpha, gs.clip);
			continue;
		}
		var cp = unicode_to_cp437(u);
		if (cp == -2)
			continue;
		if (cp == -1 || !bf.ok)
			draw_unknown_box(Mk, adv, rgb, alpha);
		else
			draw_glyph(bf, cp, Mk, adv, rgb, alpha, font.bold, gs.clip);
	}
}

function show_text(str)
{
	var gs = RS.gs;
	var font = gs.font;
	if (!font || typeof str != "string")
		return;
	var vis = !RS.hidden && gs.tmode != 3 && gs.tmode != 7;
	var n = str.length;
	var i = 0;
	while (i < n) {
		var code;
		if (font.twobyte) {
			code = ((str.charCodeAt(i) & 255) << 8) | (str.charCodeAt(i + 1) & 255);
			i += 2;
		}
		else
			code = str.charCodeAt(i++) & 255;
		var uni = font_unicode(font, code);
		var w0 = font_width(font, code, uni);
		if (vis)
			draw_char(font, code, uni, w0);
		var tx = (w0 * gs.fs + gs.tc + ((!font.twobyte && code == 32) ? gs.tw : 0)) * gs.th;
		var tm = RS.tm;
		RS.tm = [tm[0], tm[1], tm[2], tm[3], tx * tm[0] + tm[4], tx * tm[1] + tm[5]];
	}
}

function text_next_line(tx, ty)
{
	var t = mat_mul([1, 0, 0, 1, tx, ty], RS.tlm);
	RS.tlm = t;
	RS.tm = t;
}

/* ----- XObjects ----- */

function paint_image(st, res)
{
	var gs = RS.gs;
	if (RS.hidden)
		return;
	if (st.dict.OC !== undefined && oc_hidden(RS.doc, st.dict.OC))
		return;
	var ctm = gs.ctm;
	var devw = Math.sqrt(ctm[0] * ctm[0] + ctm[1] * ctm[1]);
	var img = load_image(RS.doc, st, res, devw, RS.maxN);
	if (!img)
		return;
	CLIPMASK = gs.mask;
	if (img.placeholder) {
		var pts = [];
		var c = [[0, 0], [1, 0], [1, 1], [0, 1]];
		for (var i = 0; i < 4; i++) {
			var p = mat_apply(ctm, c[i][0], c[i][1]);
			pts.push(p[0], p[1]);
		}
		fill_polys([pts], false, [205, 205, 205], gs.fa, gs.clip);
		return;
	}
	draw_image(img, ctm, gs.fa, gs.frgb, gs.clip);
}

function paint_form(st, res)
{
	var doc = RS.doc;
	if (RS.depth > 10 || RS.hidden)
		return;
	if (st.dict.OC !== undefined && oc_hidden(doc, st.dict.OC))
		return;
	var m = num_array(doc, st.dict.Matrix) || [1, 0, 0, 1, 0, 0];
	var bbox = rect_of(doc, st.dict.BBox);
	var saved = RS.gs;
	var savestack = RS.stack.length;
	var savepath = RS.path, savecur = RS.cur, savetm = RS.tm, savetlm = RS.tlm, savepending = RS.pending, savepb = RS.patbase;
	RS.stack.push(saved);
	var gs = clone_gs(saved);
	RS.gs = gs;
	gs.ctm = mat_mul(m, gs.ctm);
	RS.patbase = gs.ctm;
	if (bbox) {
		var minx = 1e9, miny = 1e9, maxx = -1e9, maxy = -1e9;
		var cs = [[bbox[0], bbox[1]], [bbox[2], bbox[1]], [bbox[2], bbox[3]], [bbox[0], bbox[3]]];
		for (var i = 0; i < 4; i++) {
			var p = mat_apply(gs.ctm, cs[i][0], cs[i][1]);
			if (p[0] < minx) minx = p[0];
			if (p[0] > maxx) maxx = p[0];
			if (p[1] < miny) miny = p[1];
			if (p[1] > maxy) maxy = p[1];
		}
		gs.clip = intersect_rect(gs.clip, [minx, miny, maxx, maxy]);
	}
	RS.path = [];
	RS.cur = null;
	RS.pending = 0;
	var fres = doc.resolve(st.dict.Resources);
	if (!isDict(fres))
		fres = res;
	var ops = st._ops || (st._ops = parse_content(doc, st.bytes()));
	RS.depth++;
	if (gs.clip[2] > gs.clip[0] && gs.clip[3] > gs.clip[1])
		exec_ops(ops, fres, 0);
	RS.depth--;
	while (RS.stack.length > savestack)
		RS.stack.pop();
	RS.gs = saved;
	RS.path = savepath;
	RS.cur = savecur;
	RS.tm = savetm;
	RS.tlm = savetlm;
	RS.pending = savepending;
	RS.patbase = savepb;
}

/* ----- the interpreter loop ----- */

function exec_ops(ops, res, depth)
{
	var doc = RS.doc;
	var i, gs, a;
	var startstack = RS.stack.length;
	var startmc = RS.mc.length;
	for (var oi = 0; oi < ops.length; oi++) {
		if ((++RS.opcount & 127) == 0 && RS.abort && RS.abort()) {
			RS.aborted = true;
			return;
		}
		if (RS.aborted)
			return;
		var op = ops[oi][0];
		var args = ops[oi][1];
		gs = RS.gs;
		var sdepth = RS.depth, sstack = RS.stack.length, sgs = gs;
		try {
		switch (op) {
			case "q":
				RS.stack.push(gs);
				RS.gs = clone_gs(gs);
				break;
			case "Q":
				if (RS.stack.length > startstack)
					RS.gs = RS.stack.pop();
				break;
			case "cm":
				gs.ctm = mat_mul(nums(args, 6), gs.ctm);
				break;
			case "w": gs.lw = num(args[0]); break;
			case "J": gs.cap = num(args[0]); break;
			case "j": gs.join = num(args[0]); break;
			case "M": gs.miter = num(args[0]); break;
			case "d":
				if (args[0] instanceof Array) {
					gs.dash = [];
					for (i = 0; i < args[0].length; i++)
						gs.dash.push(num(args[0][i]));
					gs.dphase = num(args[1]);
				}
				break;
			case "gs": {
				var eg = res_get(res, "ExtGState", args[0] instanceof Name ? args[0].name : "");
				if (isDict(eg)) {
					if (eg.LW !== undefined) gs.lw = num(doc.resolve(eg.LW));
					if (eg.LC !== undefined) gs.cap = num(doc.resolve(eg.LC));
					if (eg.LJ !== undefined) gs.join = num(doc.resolve(eg.LJ));
					if (eg.ML !== undefined) gs.miter = num(doc.resolve(eg.ML));
					if (eg.ca !== undefined) gs.fa = clampn(num(doc.resolve(eg.ca)), 0, 1);
					if (eg.CA !== undefined) gs.sa = clampn(num(doc.resolve(eg.CA)), 0, 1);
					var dsh = doc.resolve(eg.D);
					if (dsh instanceof Array && doc.resolve(dsh[0]) instanceof Array) {
						var da = num_array(doc, dsh[0]);
						gs.dash = da;
						gs.dphase = num(doc.resolve(dsh[1]));
					}
					var fnt = doc.resolve(eg.Font);
					if (fnt instanceof Array) {
						var ff = make_font(doc, fnt[0], res);
						if (ff) {
							gs.font = ff;
							gs.fs = num(doc.resolve(fnt[1]));
						}
					}
				}
				break;
			}
			case "m": path_move(num(args[0]), num(args[1])); break;
			case "l": path_line(num(args[0]), num(args[1])); break;
			case "c": path_curve(num(args[0]), num(args[1]), num(args[2]), num(args[3]), num(args[4]), num(args[5])); break;
			case "v": {
				/* first control point = current point (in user space; approximate via inverse) */
				var inv = mat_inv(gs.ctm);
				var cu = inv ? mat_apply(inv, RS.cx, RS.cy) : [num(args[0]), num(args[1])];
				path_curve(cu[0], cu[1], num(args[0]), num(args[1]), num(args[2]), num(args[3]));
				break;
			}
			case "y": path_curve(num(args[0]), num(args[1]), num(args[2]), num(args[3]), num(args[2]), num(args[3])); break;
			case "h": path_close(); break;
			case "re": {
				var rx = num(args[0]), ry = num(args[1]), rw = num(args[2]), rh = num(args[3]);
				path_move(rx, ry);
				path_line(rx + rw, ry);
				path_line(rx + rw, ry + rh);
				path_line(rx, ry + rh);
				path_close();
				break;
			}
			case "S": paint_path(false, true, false); break;
			case "s": path_close(); paint_path(false, true, false); break;
			case "f": case "F": paint_path(true, false, false); break;
			case "f*": paint_path(true, false, true); break;
			case "B": paint_path(true, true, false); break;
			case "B*": paint_path(true, true, true); break;
			case "b": path_close(); paint_path(true, true, false); break;
			case "b*": path_close(); paint_path(true, true, true); break;
			case "n": paint_path(false, false, false); break;
			case "W": RS.pending = 1; break;
			case "W*": RS.pending = 2; break;

			/* colour */
			case "g": gs.fcs = CS_GRAY; set_fill(nums(args, 1)); break;
			case "G": gs.scs = CS_GRAY; set_stroke(nums(args, 1)); break;
			case "rg": gs.fcs = CS_RGB; set_fill(nums(args, 3)); break;
			case "RG": gs.scs = CS_RGB; set_stroke(nums(args, 3)); break;
			case "k": gs.fcs = CS_CMYK; set_fill(nums(args, 4)); break;
			case "K": gs.scs = CS_CMYK; set_stroke(nums(args, 4)); break;
			case "cs":
				gs.fcs = make_cs(doc, args[0], res);
				gs.fcol = gs.fcs.init.slice();
				gs.fpat = null;
				if (!gs.fcs.pattern && !RS.nocolor)
					gs.frgb = gs.fcs.rgb(gs.fcol);
				break;
			case "CS":
				gs.scs = make_cs(doc, args[0], res);
				gs.scol = gs.scs.init.slice();
				if (!gs.scs.pattern && !RS.nocolor)
					gs.srgb = gs.scs.rgb(gs.scol);
				break;
			case "sc": case "scn": case "SC": case "SCN": {
				var fill = (op == "sc" || op == "scn");
				var cspace = fill ? gs.fcs : gs.scs;
				var comps = [];
				var pname = null;
				for (i = 0; i < args.length; i++) {
					if (typeof args[i] == "number")
						comps.push(args[i]);
					else if (args[i] instanceof Name)
						pname = args[i].name;
				}
				if (RS.nocolor)
					break;
				if (cspace.pattern) {
					var pinfo = pname ? pattern_info(res, pname, comps, cspace) : { rgb: [176, 176, 176], pat: null };
					if (fill) {
						gs.frgb = pinfo.rgb;
						gs.fpat = pinfo.pat;
					}
					else
						gs.srgb = pinfo.rgb;
				}
				else if (fill)
					set_fill(comps);
				else
					set_stroke(comps);
				break;
			}

			/* text */
			case "BT":
				RS.tm = [1, 0, 0, 1, 0, 0];
				RS.tlm = RS.tm;
				break;
			case "ET": break;
			case "Tc": gs.tc = num(args[0]); break;
			case "Tw": gs.tw = num(args[0]); break;
			case "Tz": gs.th = num(args[0]) / 100; break;
			case "TL": gs.tl = num(args[0]); break;
			case "Tr": gs.tmode = num(args[0]); break;
			case "Ts": gs.trise = num(args[0]); break;
			case "Tf": {
				var fd = res_get(res, "Font", args[0] instanceof Name ? args[0].name : "");
				gs.font = fd ? make_font(doc, fd, res) : null;
				gs.fs = num(args[1]);
				break;
			}
			case "Td": text_next_line(num(args[0]), num(args[1])); break;
			case "TD":
				gs.tl = -num(args[1]);
				text_next_line(num(args[0]), num(args[1]));
				break;
			case "Tm":
				RS.tm = nums(args, 6);
				RS.tlm = RS.tm;
				break;
			case "T*": text_next_line(0, -gs.tl); break;
			case "Tj": show_text(args[0]); break;
			case "'":
				text_next_line(0, -gs.tl);
				show_text(args[0]);
				break;
			case "\"":
				gs.tw = num(args[0]);
				gs.tc = num(args[1]);
				text_next_line(0, -gs.tl);
				show_text(args[2]);
				break;
			case "TJ":
				a = args[0];
				if (a instanceof Array) {
					for (i = 0; i < a.length; i++) {
						if (typeof a[i] == "string")
							show_text(a[i]);
						else if (typeof a[i] == "number") {
							var adj = -a[i] / 1000 * gs.fs * gs.th;
							var tm2 = RS.tm;
							RS.tm = [tm2[0], tm2[1], tm2[2], tm2[3], adj * tm2[0] + tm2[4], adj * tm2[1] + tm2[5]];
						}
					}
				}
				break;

			/* objects */
			case "Do": {
				var xo = res_get(res, "XObject", args[0] instanceof Name ? args[0].name : "");
				if (xo instanceof PDFStream) {
					var sub = doc.resolve(xo.dict.Subtype);
					if (isName(sub, "Image"))
						paint_image(xo, res);
					else if (isName(sub, "Form"))
						paint_form(xo, res);
				}
				break;
			}
			case "BI":
				paint_image(args[0], res);
				break;
			case "sh": {
				var shd = res_get(res, "Shading", args[0] instanceof Name ? args[0].name : "");
				if (shd && !RS.hidden)
					paint_shading(shd, gs.ctm, gs.clip, null, gs.fa);
				break;
			}
			case "d1":
				RS.nocolor = true;
				break;

			/* marked content (optional content groups) */
			case "BMC":
				RS.mc.push(false);
				break;
			case "BDC": {
				var hid = false;
				if (isName(args[0], "OC")) {
					var prop = args[1];
					if (prop instanceof Name) {
						var pd = doc.resolve(res.Properties);
						if (isDict(pd) && pd[prop.name] !== undefined)
							hid = oc_hidden(doc, pd[prop.name]);
					}
				}
				RS.mc.push(hid);
				if (hid)
					RS.hidden++;
				break;
			}
			case "EMC":
				if (RS.mc.length > startmc && RS.mc.pop())
					RS.hidden--;
				break;
		}
		}
		catch (err) {
			if (err === "aborted")
				throw err;
			/* a malformed or unsupported operation: restore state and carry on */
			RS.depth = sdepth;
			while (RS.stack.length > sstack)
				RS.stack.pop();
			RS.gs = sgs;
			RS.path = [];
			RS.cur = null;
			CLIPMASK = null;
			COVTARGET = null;
			RS.errors = (RS.errors || 0) + 1;
			if (RS.errors < 5 && typeof log == "function")
				log(LOG_DEBUG, "pdf2sixel: operator " + op + ": " + err);
		}
	}
	while (RS.mc.length > startmc)
		if (RS.mc.pop())
			RS.hidden--;
	while (RS.stack.length > startstack)
		RS.gs = RS.stack.pop();
}

/* Draw annotation appearance streams (form fields, stamps, comments, ...) */
function paint_annotations(doc, page)
{
	var annots = doc.resolve(page.dict.Annots);
	if (!(annots instanceof Array))
		return;
	var base = RS.patbase;
	for (var i = 0; i < annots.length; i++) {
		var a = doc.resolve(annots[i]);
		if (!isDict(a))
			continue;
		var flags = doc.resolve(a.F) || 0;
		if (flags & (2 | 32))	/* Hidden, NoView */
			continue;
		var st = doc.resolve(a.Subtype);
		if (isName(st, "Popup"))
			continue;
		var ap = doc.resolve(a.AP);
		if (!isDict(ap))
			continue;
		var n = doc.resolve(ap.N);
		if (isDict(n)) {
			var as = doc.resolve(a.AS);
			n = isName(as) ? doc.resolve(n[as.name]) : null;
		}
		if (!(n instanceof PDFStream))
			continue;
		var rect = rect_of(doc, a.Rect);
		if (!rect)
			continue;
		var bbox = rect_of(doc, n.dict.BBox) || [0, 0, rect[2] - rect[0], rect[3] - rect[1]];
		var m = num_array(doc, n.dict.Matrix) || [1, 0, 0, 1, 0, 0];
		/* bounding box of the transformed form bbox */
		var cs = [[bbox[0], bbox[1]], [bbox[2], bbox[1]], [bbox[2], bbox[3]], [bbox[0], bbox[3]]];
		var minx = 1e9, miny = 1e9, maxx = -1e9, maxy = -1e9;
		for (var k = 0; k < 4; k++) {
			var p = mat_apply(m, cs[k][0], cs[k][1]);
			if (p[0] < minx) minx = p[0];
			if (p[0] > maxx) maxx = p[0];
			if (p[1] < miny) miny = p[1];
			if (p[1] > maxy) maxy = p[1];
		}
		var sx = maxx > minx ? (rect[2] - rect[0]) / (maxx - minx) : 1;
		var sy = maxy > miny ? (rect[3] - rect[1]) / (maxy - miny) : 1;
		var A = [sx, 0, 0, sy, rect[0] - minx * sx, rect[1] - miny * sy];
		RS.gs = new_gs(mat_mul(A, base), RS.gs.clip);
		RS.stack = [];
		paint_form(n, page.res);
	}
}

/* Render a page into FB (which must already be initialised).
 * base maps PDF user space to device pixels. Returns false if aborted. */
function render_page(doc, page, base, clip, abort_cb, maxN)
{
	RS = {
		doc: doc, gs: new_gs(base, clip), stack: [], path: [], cur: null, cx: 0, cy: 0,
		pending: 0, tm: [1, 0, 0, 1, 0, 0], tlm: [1, 0, 0, 1, 0, 0], hidden: 0, mc: [],
		abort: abort_cb, opcount: 0, aborted: false, nocolor: false, depth: 0, patbase: base,
		maxN: maxN || 8, degraded: false
	};
	CLIPMASK = null;
	COVTARGET = null;
	try {
		var ops = page_ops(doc, page);
		exec_ops(ops, page.res, 0);
		if (!RS.aborted)
			paint_annotations(doc, page);
	}
	catch (e) {
		CLIPMASK = null;
		COVTARGET = null;
		if (e === "aborted")
			RS.aborted = true;
		else
			RS.error = e;
	}
	CLIPMASK = null;
	var ok = !RS.aborted;
	var err = RS.error;
	LAST_DEGRADED = RS.degraded;
	RS = null;
	if (err !== undefined && typeof log == "function")
		log(LOG_DEBUG, "pdf2sixel: render error: " + err);
	return ok;
}

/* ======================================================================= */
/* Part 6: Sixel encoder, viewer and program entry point                    */
/* ======================================================================= */

/* ----- Sixel encoding ----- */

var SIX_GRAYS = 32;
var SIX_LEVELS = [0, 51, 102, 153, 204, 255];
var SIX_NCOLORS = SIX_GRAYS + 216;

var SIX_INVERT = false;

function sixel_color_rgb(ci)
{
	var c = sixel_color_base(ci);
	return SIX_INVERT ? [255 - c[0], 255 - c[1], 255 - c[2]] : c;
}

function sixel_color_base(ci)
{
	if (ci < SIX_GRAYS) {
		var v = Math.floor(ci * 255 / (SIX_GRAYS - 1) + 0.5);
		return [v, v, v];
	}
	var c = ci - SIX_GRAYS;
	return [SIX_LEVELS[Math.floor(c / 36)], SIX_LEVELS[Math.floor(c / 6) % 6], SIX_LEVELS[c % 6]];
}

/* Quantise to the fixed palette (32 greys + a 6x6x6 colour cube) */
function sixel_quantise_fixed(fb, n)
{
	var q = new Uint8Array(n);
	var cache = new Int32Array(4096);	/* tiny direct-mapped cache keyed on the pixel value */
	var ckey = new Int32Array(4096);
	var i;
	for (i = 0; i < 4096; i++)
		ckey[i] = -1;
	var lastp = -1, lastc = 0;
	for (i = 0; i < n; i++) {
		var p = fb[i];
		if (p === lastp) {
			q[i] = lastc;
			continue;
		}
		var h = (p ^ (p >> 12)) & 4095;
		var ci;
		if (ckey[h] === p)
			ci = cache[h];
		else {
			var r = (p >> 16) & 255, g = (p >> 8) & 255, b = p & 255;
			var mx = r > g ? (r > b ? r : b) : (g > b ? g : b);
			var mn = r < g ? (r < b ? r : b) : (g < b ? g : b);
			if (mx - mn <= 10)
				ci = Math.floor((r + g + b) / 3 * (SIX_GRAYS - 1) / 255 + 0.5);
			else
				ci = SIX_GRAYS + Math.floor((r + 25) / 51) * 36 + Math.floor((g + 25) / 51) * 6 + Math.floor((b + 25) / 51);
			ckey[h] = p;
			cache[h] = ci;
		}
		lastp = p;
		lastc = ci;
		q[i] = ci;
	}
	return q;
}

var SIX_DITHER = true;
var SIX_BAYER = [0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5];
var SIX_FIXED_PAL = null;

/* Quantise a W x H framebuffer.  Images with few colours (documents, line art) use a fixed
 * palette; photographs and gradients get an adaptive 255-colour palette (median cut) and
 * ordered dithering.  Returns { q: Uint8Array of palette indices, pal: [[r,g,b], ...] } */
function sixel_quantise(fb, W, H)
{
	var n = W * H;
	var step = n > 120000 ? 2 : 1;
	var cnt = new Int32Array(4096), sr = new Int32Array(4096), sg = new Int32Array(4096), sb = new Int32Array(4096);
	var x, y, i, p, r, g, b, bi;
	for (y = 0; y < H; y += step) {
		var base = y * W;
		for (x = 0; x < W; x += step) {
			p = fb[base + x];
			r = (p >> 16) & 255;
			g = (p >> 8) & 255;
			b = p & 255;
			bi = ((r >> 4) << 8) | ((g >> 4) << 4) | (b >> 4);
			cnt[bi]++;
			sr[bi] += r;
			sg[bi] += g;
			sb[bi] += b;
		}
	}
	var bins = [];
	for (i = 0; i < 4096; i++)
		if (cnt[i])
			bins.push(i);
	if (bins.length <= 48 || !SIX_DITHER) {
		if (!SIX_FIXED_PAL) {
			SIX_FIXED_PAL = [];
			for (i = 0; i < SIX_NCOLORS; i++)
				SIX_FIXED_PAL.push(sixel_color_base(i));
		}
		return { q: sixel_quantise_fixed(fb, n), pal: SIX_FIXED_PAL };
	}

	/* median cut on the 4-bit-per-channel histogram */
	function make_box(list)
	{
		var tot = 0, rmin = 15, rmax = 0, gmin = 15, gmax = 0, bmin = 15, bmax = 0;
		for (var k = 0; k < list.length; k++) {
			var v = list[k];
			tot += cnt[v];
			var rr = v >> 8, gg = (v >> 4) & 15, bb = v & 15;
			if (rr < rmin) rmin = rr;
			if (rr > rmax) rmax = rr;
			if (gg < gmin) gmin = gg;
			if (gg > gmax) gmax = gg;
			if (bb < bmin) bmin = bb;
			if (bb > bmax) bmax = bb;
		}
		var wr = (rmax - rmin) * 2, wg = (gmax - gmin) * 3, wb = (bmax - bmin);
		return { bins: list, n: tot, ch: (wg >= wr && wg >= wb) ? 1 : (wr >= wb ? 0 : 2), score: tot * Math.max(wr, wg, wb) };
	}
	var boxes = [make_box(bins)];
	while (boxes.length < 255) {
		var bestk = -1, best = 0;
		for (i = 0; i < boxes.length; i++)
			if (boxes[i].bins.length > 1 && boxes[i].score > best) {
				best = boxes[i].score;
				bestk = i;
			}
		if (bestk < 0)
			break;
		var bx = boxes[bestk];
		var shift = bx.ch == 0 ? 8 : (bx.ch == 1 ? 4 : 0);
		var hist = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
		var k;
		for (k = 0; k < bx.bins.length; k++)
			hist[(bx.bins[k] >> shift) & 15] += cnt[bx.bins[k]];
		var cum = 0, cut = 0, half = bx.n / 2, lo = 15, hi = 0;
		for (k = 0; k < 16; k++) {
			if (hist[k]) {
				if (k < lo) lo = k;
				if (k > hi) hi = k;
			}
		}
		for (k = lo; k < hi; k++) {
			cum += hist[k];
			cut = k;
			if (cum >= half)
				break;
		}
		var left = [], right = [];
		for (k = 0; k < bx.bins.length; k++)
			if (((bx.bins[k] >> shift) & 15) <= cut)
				left.push(bx.bins[k]);
			else
				right.push(bx.bins[k]);
		if (!left.length || !right.length) {
			bx.score = 0;
			continue;
		}
		boxes[bestk] = make_box(left);
		boxes.push(make_box(right));
	}
	var pal = [];
	var binmap = new Int16Array(4096);
	for (i = 0; i < 4096; i++)
		binmap[i] = -1;
	for (i = 0; i < boxes.length; i++) {
		var tr = 0, tg = 0, tb = 0, tn = 0;
		for (var j = 0; j < boxes[i].bins.length; j++) {
			var v2 = boxes[i].bins[j];
			tr += sr[v2];
			tg += sg[v2];
			tb += sb[v2];
			tn += cnt[v2];
			binmap[v2] = i;
		}
		pal.push([Math.round(tr / tn), Math.round(tg / tn), Math.round(tb / tn)]);
	}
	var np = pal.length;
	function nearest(v)
	{
		var rr = (v >> 8) * 17, gg = ((v >> 4) & 15) * 17, bb = (v & 15) * 17;
		var bk = 0, bd = 1e9;
		for (var c = 0; c < np; c++) {
			var dr = pal[c][0] - rr, dg = pal[c][1] - gg, db = pal[c][2] - bb;
			var d = dr * dr * 2 + dg * dg * 3 + db * db;
			if (d < bd) {
				bd = d;
				bk = c;
			}
		}
		return bk;
	}
	var q = new Uint8Array(n);
	var lastp = -1, lastc = 0, o = 0;
	for (y = 0; y < H; y++) {
		var yb = (y & 3) * 4;
		for (x = 0; x < W; x++, o++) {
			p = fb[o];
			if (p === lastp) {
				q[o] = lastc;
				continue;
			}
			if (y > 0 && fb[o - W] === p) {	/* flat area: keep the colour of the pixel above */
				lastc = q[o - W];
				lastp = p;
				q[o] = lastc;
				continue;
			}
			r = (p >> 16) & 255;
			g = (p >> 8) & 255;
			b = p & 255;
			var off = (SIX_BAYER[yb + (x & 3)] - 7.5) * 1.2;
			r += off;
			g += off;
			b += off;
			if (r < 0) r = 0; else if (r > 255) r = 255;
			if (g < 0) g = 0; else if (g > 255) g = 255;
			if (b < 0) b = 0; else if (b > 255) b = 255;
			bi = (((r | 0) >> 4) << 8) | (((g | 0) >> 4) << 4) | ((b | 0) >> 4);
			var kk = binmap[bi];
			if (kk < 0) {
				kk = nearest(bi);
				binmap[bi] = kk;
			}
			q[o] = kk;
			lastp = p;
			lastc = kk;
		}
	}
	return { q: q, pal: pal };
}

/* Encode fb (W x H, H a multiple of 6) as a complete Sixel DCS string */
function sixel_encode(fb, W, H)
{
	var Q = sixel_quantise(fb, W, H);
	var q = Q.q, pal = Q.pal;
	var out = [];
	var used = new Uint8Array(256);
	var i, x;
	for (i = 0; i < q.length; i++)
		used[q[i]] = 1;
	out.push("\x1bPq\"1;1;" + W + ";" + H);
	for (i = 0; i < pal.length; i++) {
		if (!used[i])
			continue;
		var c = SIX_INVERT ? [255 - pal[i][0], 255 - pal[i][1], 255 - pal[i][2]] : pal[i];
		out.push("#" + i + ";2;" + Math.floor(c[0] * 100 / 255 + 0.5) + ";" + Math.floor(c[1] * 100 / 255 + 0.5) + ";" + Math.floor(c[2] * 100 / 255 + 0.5));
	}
	var lines = [];
	for (i = 0; i < 256; i++)
		lines.push(used[i] ? new Uint8Array(W) : null);
	var flag = new Uint8Array(256);
	var lo = new Int32Array(256), hi = new Int32Array(256);
	var bands = Math.floor(H / 6);
	var wide = (W % 4) == 0;
	var q32 = wide ? new Uint32Array(q.buffer) : null;
	for (var band = 0; band < bands; band++) {
		var seen = [];
		var o0 = band * 6 * W;
		var g0 = o0 >> 2, g1 = (o0 + W) >> 2, g2 = (o0 + 2 * W) >> 2, g3 = (o0 + 3 * W) >> 2, g4 = (o0 + 4 * W) >> 2, g5 = (o0 + 5 * W) >> 2;
		var o1 = o0 + W, o2 = o1 + W, o3 = o2 + W, o4 = o3 + W, o5 = o4 + W;
		for (var xg = 0; xg < W; xg += 4) {
			var xe = xg + 4 > W ? W : xg + 4;
			if (wide) {
				var gi = xg >> 2;
				var wd = q32[g0 + gi];
				if (q32[g1 + gi] === wd && q32[g2 + gi] === wd && q32[g3 + gi] === wd && q32[g4 + gi] === wd && q32[g5 + gi] === wd) {
					var cu = wd & 255;
					if (((cu * 16843009) >>> 0) === wd) {
						if (!flag[cu]) {
							flag[cu] = 1;
							seen.push(cu);
							lo[cu] = xg;
						}
						hi[cu] = xg + 3;
						var lu = lines[cu];
						lu[xg] = 63;
						lu[xg + 1] = 63;
						lu[xg + 2] = 63;
						lu[xg + 3] = 63;
						continue;
					}
				}
			}
		for (x = xg; x < xe; x++) {
			var c0 = q[o0 + x], c1 = q[o1 + x], c2 = q[o2 + x], c3 = q[o3 + x], c4 = q[o4 + x], c5 = q[o5 + x];
			if (!flag[c0]) {
				flag[c0] = 1;
				seen.push(c0);
				lo[c0] = x;
			}
			hi[c0] = x;
			if (c1 == c0 && c2 == c0 && c3 == c0 && c4 == c0 && c5 == c0) {
				lines[c0][x] = 63;
				continue;
			}
			lines[c0][x] |= 1;
			if (!flag[c1]) {
				flag[c1] = 1;
				seen.push(c1);
				lo[c1] = x;
			}
			hi[c1] = x;
			lines[c1][x] |= 2;
			if (!flag[c2]) {
				flag[c2] = 1;
				seen.push(c2);
				lo[c2] = x;
			}
			hi[c2] = x;
			lines[c2][x] |= 4;
			if (!flag[c3]) {
				flag[c3] = 1;
				seen.push(c3);
				lo[c3] = x;
			}
			hi[c3] = x;
			lines[c3][x] |= 8;
			if (!flag[c4]) {
				flag[c4] = 1;
				seen.push(c4);
				lo[c4] = x;
			}
			hi[c4] = x;
			lines[c4][x] |= 16;
			if (!flag[c5]) {
				flag[c5] = 1;
				seen.push(c5);
				lo[c5] = x;
			}
			hi[c5] = x;
			lines[c5][x] |= 32;
		}
		}
		var first = true;
		for (var s = 0; s < seen.length; s++) {
			var ci = seen[s];
			flag[ci] = 0;
			var lr = lines[ci];
			var last = hi[ci];
			if (!first)
				out.push("$");
			first = false;
			var row = ["#" + ci];
			x = lo[ci];
			if (x > 0)
				row.push(x > 3 ? "!" + x + "?" : (x == 1 ? "?" : (x == 2 ? "??" : "???")));
			while (x <= last) {
				var v = lr[x];
				var run = 1;
				while (x + run <= last && lr[x + run] == v)
					run++;
				var ch = String.fromCharCode(63 + v);
				if (run > 3)
					row.push("!" + run + ch);
				else {
					for (var k = 0; k < run; k++)
						row.push(ch);
				}
				x += run;
			}
			out.push(row.join(""));
			for (x = lo[ci]; x <= last; x++)
				lr[x] = 0;
		}
		if (band < bands - 1)
			out.push("-");
	}
	out.push("\x1b\\");
	return out.join("");
}

/* ----- Links (annotations that jump to a page or name a URL) ----- */

/* Find a named destination (old-style /Dests dictionary or the /Names /Dests name tree) */
PDFDoc.prototype.find_dest = function (name)
{
	var doc = this;
	var root = this.resolve(this.trailer.Root);
	if (!isDict(root))
		return undefined;
	var d = this.resolve(root.Dests);
	if (isDict(d) && d[name] !== undefined)
		return this.resolve(d[name]);
	var names = this.resolve(root.Names);
	if (!isDict(names))
		return undefined;
	function walk(node, depth)
	{
		node = doc.resolve(node);
		if (!isDict(node) || depth > 24)
			return undefined;
		var arr = doc.resolve(node.Names);
		var i;
		if (arr instanceof Array) {
			for (i = 0; i + 1 < arr.length; i += 2) {
				if (String(doc.resolve(arr[i])) == name)
					return doc.resolve(arr[i + 1]);
			}
		}
		var kids = doc.resolve(node.Kids);
		if (kids instanceof Array) {
			for (i = 0; i < kids.length; i++) {
				var r = walk(kids[i], depth + 1);
				if (r !== undefined)
					return r;
			}
		}
		return undefined;
	}
	return walk(names.Dests, 0);
};

/* Turn a destination (array, name, string or dict with /D) into { page, x, y } (page = -1 if unknown) */
PDFDoc.prototype.resolve_dest = function (dest)
{
	dest = this.resolve(dest);
	if (isName(dest))
		dest = this.find_dest(dest.name);
	else if (typeof dest == "string")
		dest = this.find_dest(dest);
	if (isDict(dest))
		dest = this.resolve(dest.D);
	if (!(dest instanceof Array) || dest.length < 1)
		return null;
	var pages = this.get_pages();
	var target = dest[0];
	var idx = -1;
	if (target instanceof Ref) {
		for (var i = 0; i < pages.length; i++) {
			if (pages[i].num == target.num) {
				idx = i;
				break;
			}
		}
	}
	else if (typeof target == "number")
		idx = target;
	if (idx < 0 || idx >= pages.length)
		return null;
	var kind = this.resolve(dest[1]);
	var x, y;
	function n(v) { v = typeof v == "object" ? undefined : v; return typeof v == "number" ? v : undefined; }
	if (isName(kind, "XYZ")) {
		x = n(this.resolve(dest[2]));
		y = n(this.resolve(dest[3]));
	}
	else if (isName(kind, "FitH") || isName(kind, "FitBH"))
		y = n(this.resolve(dest[2]));
	else if (isName(kind, "FitR"))
		y = n(this.resolve(dest[5]));
	return { page: idx, x: x, y: y };
};

/* Decode a PDF text string (PDFDocEncoding or UTF-16) to plain CP437-friendly text */
function pdf_text_string(v)
{
	if (typeof v != "string")
		return "";
	var out = "";
	var i, c;
	if (v.charCodeAt(0) == 0xfe && v.charCodeAt(1) == 0xff) {
		for (i = 2; i + 1 < v.length; i += 2) {
			c = v.charCodeAt(i) * 256 + v.charCodeAt(i + 1);
			if (c >= 0x20 && c < 0x7f)
				out += String.fromCharCode(c);
			else if (c == 0x2018 || c == 0x2019)
				out += "'";
			else if (c == 0x201c || c == 0x201d)
				out += '"';
			else if (c == 0x2013 || c == 0x2014)
				out += "-";
			else if (c == 0x2026)
				out += "...";
			else if (c == 0xa0 || c == 0x09)
				out += " ";
			else if (c >= 0xc0 && c < 0x100)
				out += "AAAAAAACEEEEIIIIDNOOOOOxOUUUUYTsaaaaaaaceeeeiiiidnooooo/ouuuuyty".charAt(c - 0xc0);
			else if (c > 0x7e)
				out += "?";
		}
		return out;
	}
	for (i = 0; i < v.length; i++) {
		c = v.charCodeAt(i);
		out += (c >= 0x20 && c < 0x7f) ? v.charAt(i) : c == 0x09 ? " " : c >= 0xc0 ? "?" : "";
	}
	return out;
}

/* Bookmarks / table of contents: [{ title, level, page (-1 if none), x, y }] in document order */
PDFDoc.prototype.outline = function ()
{
	if (this.outline_cache)
		return this.outline_cache;
	var out = [];
	var doc = this;
	var seen = {};
	var root = this.resolve(this.trailer.Root);
	var o = isDict(root) ? this.resolve(root.Outlines) : null;
	function walk(ref, level)
	{
		var guard = 0;
		while (ref !== undefined && ref !== null && out.length < 3000 && guard++ < 3000) {
			if (ref instanceof Ref) {
				if (seen[ref.num])
					return;
				seen[ref.num] = true;
			}
			var item = doc.resolve(ref);
			if (!isDict(item))
				return;
			try {
				var act = doc.resolve(item.A);
				var dest = item.Dest !== undefined ? item.Dest : (isDict(act) && isName(doc.resolve(act.S), "GoTo") ? act.D : undefined);
				var d = dest !== undefined ? doc.resolve_dest(dest) : null;
				out.push({ title: pdf_text_string(doc.resolve(item.Title)).replace(/^\s+|\s+$/g, ""), level: level,
					page: d ? d.page : -1, x: d ? d.x : undefined, y: d ? d.y : undefined });
			}
			catch (e) {}
			if (item.First !== undefined && level < 8)
				walk(item.First, level + 1);
			ref = item.Next;
		}
	}
	if (isDict(o) && o.First !== undefined)
		walk(o.First, 0);
	this.outline_cache = out;
	return out;
};

PDFDoc.prototype.has_outline = function ()
{
	var root = this.resolve(this.trailer.Root);
	var o = isDict(root) ? this.resolve(root.Outlines) : null;
	return isDict(o) && o.First !== undefined;
};

/* Links on page number pageno (0-based) that jump to another place in the document, in reading
 * order.  Each is { rect: [x0, y0, x1, y1] (PDF user space), page: index, x, y } */
PDFDoc.prototype.page_links = function (pageno)
{
	if (!this.link_cache)
		this.link_cache = {};
	if (this.link_cache[pageno])
		return this.link_cache[pageno];
	var out = [];
	var page = this.get_pages()[pageno];
	var annots = page ? this.resolve(page.dict.Annots) : null;
	if (annots instanceof Array) {
		for (var i = 0; i < annots.length && out.length < 500; i++) {
			try {
				var a = this.resolve(annots[i]);
				if (!isDict(a) || !isName(this.resolve(a.Subtype), "Link"))
					continue;
				var rect = rect_of(this, a.Rect);
				if (!rect)
					continue;
				/* only links to somewhere inside the document are kept */
				var act = this.resolve(a.A);
				var d = this.resolve_dest(isDict(act) && isName(this.resolve(act.S), "GoTo") ? act.D : a.Dest);
				if (d)
					out.push({ rect: rect, page: d.page, x: d.x, y: d.y });
			}
			catch (e) {}
		}
	}
	out.sort(function (p, q) {
		var dy = q.rect[3] - p.rect[3];
		return Math.abs(dy) > 2 ? dy : p.rect[0] - q.rect[0];
	});
	this.link_cache[pageno] = out;
	return out;
};

/* ----- View management ----- */

var VIEW_MARGIN = 8;

function page_size(page)
{
	var w = page.box[2] - page.box[0], h = page.box[3] - page.box[1];
	return (page.rotate == 90 || page.rotate == 270) ? { w: h, h: w } : { w: w, h: h };
}

function page_matrix(page, s, ox, oy)
{
	var b = page.box;
	switch (page.rotate) {
		case 90: return [0, s, s, 0, -s * b[1] + ox, -s * b[0] + oy];
		case 180: return [-s, 0, 0, s, s * b[2] + ox, -s * b[1] + oy];
		case 270: return [0, -s, -s, 0, s * b[3] + ox, s * b[2] + oy];
	}
	return [s, 0, 0, -s, -s * b[0] + ox, s * b[3] + oy];
}

function View(doc, W, H)
{
	this.doc = doc;
	this.pages = doc.get_pages();
	this.W = W;
	this.H = H;
	this.page = 0;
	this.scale = 1;
	this.sx = 0;
	this.sy = 0;
}

View.prototype.dims = function ()
{
	var d = page_size(this.pages[this.page]);
	return { w: d.w * this.scale, h: d.h * this.scale };
};

View.prototype.clamp = function ()
{
	var d = this.dims();
	var m = VIEW_MARGIN;
	if (d.w + 2 * m <= this.W)
		this.sx = -(this.W - d.w) / 2;
	else
		this.sx = Math.max(-m, Math.min(d.w - this.W + m, this.sx));
	if (d.h + 2 * m <= this.H)
		this.sy = -(this.H - d.h) / 2;
	else
		this.sy = Math.max(-m, Math.min(d.h - this.H + m, this.sy));
};

View.prototype.fit_width = function ()
{
	var d = page_size(this.pages[this.page]);
	this.scale = (this.W - 2 * VIEW_MARGIN) / d.w;
	this.sx = 0;
	this.sy = -VIEW_MARGIN;
	this.clamp();
};

View.prototype.fit_page = function ()
{
	var d = page_size(this.pages[this.page]);
	this.scale = Math.min((this.W - 2 * VIEW_MARGIN) / d.w, (this.H - 2 * VIEW_MARGIN) / d.h);
	this.sx = 0;
	this.sy = 0;
	this.clamp();
};

View.prototype.zoom = function (factor)
{
	var ns = Math.max(0.1, Math.min(16, this.scale * factor));
	if (ns == this.scale)
		return;
	/* keep the page point at the centre of the view fixed */
	var cx = (this.sx + this.W / 2) / this.scale, cy = (this.sy + this.H / 2) / this.scale;
	this.scale = ns;
	this.sx = cx * ns - this.W / 2;
	this.sy = cy * ns - this.H / 2;
	this.clamp();
};

View.prototype.set_page = function (n, at_bottom)
{
	n = Math.max(0, Math.min(this.pages.length - 1, n));
	var keep = this.scale;
	this.page = n;
	this.scale = keep;
	this.sy = at_bottom ? 1e9 : -1e9;
	this.clamp();
};

/* Render the current view into FB */
View.prototype.render = function (abort_cb, maxN)
{
	var page = this.pages[this.page];
	var W = this.W, H = this.H;
	fb_init(W, H, 0);
	var d = this.dims();
	var ox = -this.sx, oy = -this.sy;
	var x0 = Math.max(0, Math.round(ox)), y0 = Math.max(0, Math.round(oy));
	var x1 = Math.min(W, Math.round(ox + d.w)), y1 = Math.min(H, Math.round(oy + d.h));
	/* background and white page, one row at a time using native typed-array copies */
	var bgrow = new Int32Array(W), pagerow = new Int32Array(W);
	var x;
	for (x = 0; x < W; x++) {
		bgrow[x] = 0x585858;
		pagerow[x] = (x >= x0 && x < x1) ? 0xffffff : 0x585858;
	}
	for (var y = 0; y < H; y++)
		FB.set((y >= y0 && y < y1) ? pagerow : bgrow, y * W);
	if (x1 <= x0 || y1 <= y0)
		return true;
	var clip = [x0, y0, x1, y1];
	var M = page_matrix(page, this.scale, ox, oy);
	if (this.doc.render)	/* non-PDF documents (images) supply their own renderer */
		return this.doc.render(page, M, clip, abort_cb, maxN);
	return render_page(this.doc, page, M, clip, abort_cb, maxN);
};

function load_pdf(filename)
{
	var f = new File(filename);
	if (!f.exists)
		throw "File not found: " + filename;
	if (f.length > 96 * 1024 * 1024)
		throw "File is too large";
	if (!f.open("rb"))
		throw "Unable to open " + filename;
	var s = f.read();
	f.close();
	if (!s || s.length < 16)
		throw "File is empty or too short";
	var bytes = string_to_bytes(s);
	s = null;
	/* tolerate junk before the %PDF header */
	var h = find_bytes(bytes, "%PDF-", 0, 1024);
	if (h > 0)
		bytes = bytes_sub(bytes, h);
	var doc = new PDFDoc(bytes);
	if (doc.encrypted && !doc.crypt)
		throw (doc.crypt_error || "Encrypted PDF files are not supported");
	if (doc.get_pages().length < 1)
		throw "The document contains no pages";
	return doc;
}

/* Load a PDF and render one page (1-based) into a w x h window, fitted to the page width */
function pdf_to_sixel(filename, page, width, height)
{
	var doc = load_pdf(filename);
	height -= height % 6;
	var view = new View(doc, width, height);
	view.page = Math.max(0, Math.min(view.pages.length - 1, (page || 1) - 1));
	view.fit_width();
	view.render(null, 8);
	return sixel_encode(FB, FW, FH);
}

/* ----- Utilities ----- */

/* Write a framebuffer as a binary PPM (P6) file */

function write_ppm(filename, fb, W, H)
{
	var f = new File(filename);
	if (!f.open("wb"))
		throw "Unable to create " + filename;
	f.write("P6\n" + W + " " + H + "\n255\n");
	var row = [];
	for (var y = 0; y < H; y++) {
		row = [];
		for (var x = 0; x < W; x++) {
			var p = fb[y * W + x];
			row.push(String.fromCharCode((p >> 16) & 255, (p >> 8) & 255, p & 255));
		}
		f.write(row.join(""));
	}
	f.close();
}

