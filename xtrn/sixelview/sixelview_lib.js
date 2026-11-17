/* sixelview_lib.js
 *
 * Terminal image/document viewer library: the full-screen scroll/zoom viewer used by
 * pdfview.js and sixelview.js, plus Sixel/PNG/JPEG/GIF/BMP/PPM image loading.
 * Requires pdf2sixel_lib.js (rasteriser, Sixel encoder, JPEG decoder) and, for PNG,
 * inflate.js.  Tab size is 4.
 *
 *	load("sixelview_lib.js");
 *	viewer(load_pdf(path), 1);				// interactive PDF viewer, first page
 *	viewer(new ImageDoc(load_image_file(path)), 0);	// interactive image viewer
 *
 * Exported functions:
 *	viewer(doc, startpage)	run the viewer on a PDFDoc or ImageDoc until the user quits
 *	load_image_file(file)	returns { w, h, n (1|3), data, alpha, mask }; throws a
 *				string if the file is missing or unsupported
 *	ImageDoc(img)		wraps a loaded image as a one-page document
 *	sixel_decode(string)	decode Sixel data to { w, h, n: 3, data }
 *
 * A "document" for viewer() provides get_pages() (each page { box: [x0, y0, x1, y1],
 * rotate }), render(page, matrix, clip, abort_cb, maxN) drawing into FB (optional;
 * PDFDoc is rendered by pdf2sixel_lib), and optionally initial_fit ("width"|"page")
 * and actual_scale (pixels per document unit at 100%).
 */

"use strict";

load("sbbsdefs.js");
load("pdf2sixel_lib.js");

/* ======================================================================= */
/* Image file loading: Sixel, PNG, JPEG, GIF, BMP and PPM/PGM                    */
/* ======================================================================= */

var SIXEL_DEFAULT_PALETTE = [[0, 0, 0], [20, 20, 80], [80, 13, 13], [20, 80, 20], [80, 20, 80], [20, 80, 80], [80, 80, 20], [53, 53, 53],
	[26, 26, 26], [33, 33, 60], [60, 26, 26], [33, 60, 33], [60, 33, 60], [33, 60, 60], [60, 60, 33], [80, 80, 80]];

function hls_to_rgb(h, l, s)
{
	/* DEC hue: 0 = blue, 120 = red, 240 = green; l and s are 0-100 */
	h = (h + 240) % 360;
	l /= 100;
	s /= 100;
	var q = l < 0.5 ? l * (1 + s) : l + s - l * s, p = 2 * l - q;
	function ch(t)
	{
		if (t < 0)
			t += 1;
		if (t > 1)
			t -= 1;
		if (t < 1 / 6)
			return p + (q - p) * 6 * t;
		if (t < 0.5)
			return q;
		if (t < 2 / 3)
			return p + (q - p) * (2 / 3 - t) * 6;
		return p;
	}
	return [Math.round(ch(h / 360 + 1 / 3) * 255), Math.round(ch(h / 360) * 255), Math.round(ch(h / 360 - 1 / 3) * 255)];
}

/* Decode a Sixel string (with or without its DCS introducer).
 * Returns { w, h, n: 3, data: Uint8Array RGB } */
function sixel_decode(s)
{
	var start = 0;
	var dcs = s.indexOf("\x1bP");
	if (dcs >= 0) {
		var q = s.indexOf("q", dcs);
		if (q < 0)
			throw "Not a Sixel image";
		start = q + 1;
	}
	var end = s.indexOf("\x1b\\", start);
	if (end < 0)
		end = s.length;
	var pal = [];
	var i;
	for (i = 0; i < 16; i++) {
		var d = SIXEL_DEFAULT_PALETTE[i];
		pal[i] = [Math.round(d[0] * 2.55), Math.round(d[1] * 2.55), Math.round(d[2] * 2.55)];
	}
	var pix = null, W = 0, H = 0;
	var pass;
	for (pass = 0; pass < 2; pass++) {
		var x = 0, y = 0, maxx = 0, color = 0, rgb = pal[0], p = start;
		var hintw = 0, hinth = 0, maxy = 0;
		while (p < end) {
			var c = s.charCodeAt(p++);
			if (c >= 63 && c <= 126) {
				if (pass == 1) {
					var bits = c - 63;
					if (bits && x < W) {
						for (var r = 0; r < 6; r++)
							if ((bits >> r) & 1 && y + r < H)
								pix[(y + r) * W + x] = rgb;
					}
				}
				x++;
				if (x > maxx)
					maxx = x;
				if (y + 6 > maxy)
					maxy = y + 6;
			}
			else if (c == 33) {	/* ! repeat */
				var n = 0;
				while (p < end && s.charCodeAt(p) >= 48 && s.charCodeAt(p) <= 57)
					n = n * 10 + s.charCodeAt(p++) - 48;
				var rc = s.charCodeAt(p++);
				if (rc >= 63 && rc <= 126) {
					if (pass == 1) {
						var rb = rc - 63;
						if (rb) {
							for (var k = 0; k < n; k++) {
								if (x + k >= W)
									break;
								for (var r2 = 0; r2 < 6; r2++)
									if ((rb >> r2) & 1 && y + r2 < H)
										pix[(y + r2) * W + x + k] = rgb;
							}
						}
					}
					x += n;
					if (x > maxx)
						maxx = x;
					if (y + 6 > maxy)
						maxy = y + 6;
				}
			}
			else if (c == 36)	/* $ */
				x = 0;
			else if (c == 45) {	/* - */
				x = 0;
				y += 6;
			}
			else if (c == 35 || c == 34) {	/* # color, " raster attributes */
				var nums = [];
				var cur = -1;
				while (p < end) {
					var ch = s.charCodeAt(p);
					if (ch >= 48 && ch <= 57) {
						cur = (cur < 0 ? 0 : cur) * 10 + ch - 48;
						p++;
					}
					else if (ch == 59) {
						nums.push(cur < 0 ? 0 : cur);
						cur = -1;
						p++;
					}
					else
						break;
				}
				if (cur >= 0)
					nums.push(cur);
				if (c == 35) {
					color = nums[0] || 0;
					if (nums.length >= 5) {
						if (nums[1] == 2)
							pal[color] = [Math.round(nums[2] * 2.55), Math.round(nums[3] * 2.55), Math.round(nums[4] * 2.55)];
						else if (nums[1] == 1)
							pal[color] = hls_to_rgb(nums[2], nums[3], nums[4]);
					}
					rgb = pal[color] || [0, 0, 0];
				}
				else if (nums.length >= 4) {
					hintw = nums[2];
					hinth = nums[3];
				}
			}
		}
		if (pass == 0) {
			W = Math.max(maxx, hintw);
			H = Math.max(maxy, hinth);
			if (W < 1 || H < 1 || W * H > 64 * 1024 * 1024)
				throw "Invalid Sixel image size";
			pix = new Array(W * H);
			for (i = 0; i < pix.length; i++)
				pix[i] = null;
		}
	}
	var out = new Uint8Array(W * H * 3);
	for (i = 0; i < W * H; i++) {
		var v = pix[i];
		if (v) {
			out[i * 3] = v[0];
			out[i * 3 + 1] = v[1];
			out[i * 3 + 2] = v[2];
		}
	}
	return { w: W, h: H, n: 3, data: out, alpha: null, mask: false };
}

function ppm_decode(b)
{
	var lx = new Lexer(b, 0);
	var magic = lx.token();
	if (!(magic instanceof Cmd) || (magic.cmd != "P6" && magic.cmd != "P5"))
		throw "Unsupported PNM format (only binary P5/P6)";
	var w = lx.token(), h = lx.token(), mx = lx.token();
	if (typeof w != "number" || typeof h != "number" || typeof mx != "number" || mx > 65535 || mx < 1)
		throw "Unsupported PNM header";
	var p = lx.pos + 1;
	var n = magic.cmd == "P6" ? 3 : 1;
	var data = new Uint8Array(w * h * n);
	var wide = mx > 255;
	var len = Math.min(data.length, Math.floor((b.length - p) / (wide ? 2 : 1)));
	for (var i = 0; i < len; i++)
		data[i] = wide ? b[p + i * 2] * 255 / (mx >> 8 ? 255 : mx) : b[p + i] * 255 / mx;
	return { w: w, h: h, n: n, data: data, alpha: null, mask: false };
}

function bmp_decode(b)
{
	function u16(o) { return b[o] | (b[o + 1] << 8); }
	function u32(o) { return (b[o] | (b[o + 1] << 8) | (b[o + 2] << 16) | (b[o + 3] << 24)) >>> 0; }
	function s32(o) { return b[o] | (b[o + 1] << 8) | (b[o + 2] << 16) | (b[o + 3] << 24); }
	var off = u32(10), hs = u32(14);
	if (hs < 40)
		throw "Unsupported BMP header";
	var w = s32(18), h = s32(22), bpp = u16(28), comp = u32(30), used = u32(46);
	if (comp != 0 && !(comp == 3 && bpp == 32))
		throw "Compressed BMP files are not supported";
	var topdown = h < 0;
	if (topdown)
		h = -h;
	var pal = [];
	if (bpp <= 8) {
		var np = used || (1 << bpp);
		for (var i = 0; i < np; i++) {
			var po = 14 + hs + i * 4;
			pal.push([b[po + 2], b[po + 1], b[po]]);
		}
	}
	var rowbytes = ((w * bpp + 31) >> 5) << 2;
	var data = new Uint8Array(w * h * 3);
	for (var y = 0; y < h; y++) {
		var ro = off + (topdown ? y : h - 1 - y) * rowbytes;
		for (var x = 0; x < w; x++) {
			var o = (y * w + x) * 3, r, g, bl;
			if (bpp == 24 || bpp == 32) {
				var q = ro + x * (bpp >> 3);
				bl = b[q];
				g = b[q + 1];
				r = b[q + 2];
			}
			else {
				var idx;
				if (bpp == 8)
					idx = b[ro + x];
				else if (bpp == 4)
					idx = (b[ro + (x >> 1)] >> ((x & 1) ? 0 : 4)) & 15;
				else if (bpp == 1)
					idx = (b[ro + (x >> 3)] >> (7 - (x & 7))) & 1;
				else
					throw "Unsupported BMP bit depth";
				var pc = pal[idx] || [0, 0, 0];
				r = pc[0];
				g = pc[1];
				bl = pc[2];
			}
			data[o] = r;
			data[o + 1] = g;
			data[o + 2] = bl;
		}
	}
	return { w: w, h: h, n: 3, data: data, alpha: null, mask: false };
}

function png_decode(b)
{
	var p = 8, w = 0, h = 0, depth = 8, ctype = 0, interlace = 0;
	var pal = null, idat = [], total = 0, trns = null;
	while (p + 8 <= b.length) {
		var len = ((b[p] << 24) | (b[p + 1] << 16) | (b[p + 2] << 8) | b[p + 3]) >>> 0;
		var type = String.fromCharCode(b[p + 4], b[p + 5], b[p + 6], b[p + 7]);
		var d = p + 8;
		if (type == "IHDR") {
			w = ((b[d] << 24) | (b[d + 1] << 16) | (b[d + 2] << 8) | b[d + 3]) >>> 0;
			h = ((b[d + 4] << 24) | (b[d + 5] << 16) | (b[d + 6] << 8) | b[d + 7]) >>> 0;
			depth = b[d + 8];
			ctype = b[d + 9];
			interlace = b[d + 12];
		}
		else if (type == "PLTE")
			pal = bytes_sub(b, d, d + len);
		else if (type == "tRNS")
			trns = bytes_sub(b, d, d + len);
		else if (type == "IDAT") {
			idat.push(bytes_sub(b, d, d + len));
			total += len;
		}
		else if (type == "IEND")
			break;
		p = d + len + 4;
	}
	if (!w || !h || w * h > 64 * 1024 * 1024)
		throw "Invalid PNG image";
	if (interlace)
		throw "Interlaced PNG files are not supported";
	var comp = new Uint8Array(total), o = 0, i;
	for (i = 0; i < idat.length; i++) {
		comp.set(idat[i], o);
		o += idat[i].length;
	}
	var ncomp = ctype == 0 ? 1 : (ctype == 2 ? 3 : (ctype == 3 ? 1 : (ctype == 4 ? 2 : 4)));
	var fake = { resolve: function (v) { return v; } };
	var raw = apply_predictor(inflate_zlib(comp), { Predictor: 15, Colors: ncomp, BitsPerComponent: depth, Columns: w }, fake);
	if (depth == 8 && !trns && (ctype == 2 || ctype == 0) && raw.length >= w * h * ncomp)
		return { w: w, h: h, n: ncomp, data: bytes_sub(raw, 0, w * h * ncomp), alpha: null, mask: false };
	var out, alpha = null, np = w * h;
	if (depth == 8 && !trns && ctype == 6 && raw.length >= np * 4) {
		out = new Uint8Array(np * 3);
		alpha = new Uint8Array(np);
		for (var i6 = 0, o6 = 0, q6 = 0; i6 < np; i6++, o6 += 4, q6 += 3) {
			out[q6] = raw[o6];
			out[q6 + 1] = raw[o6 + 1];
			out[q6 + 2] = raw[o6 + 2];
			alpha[i6] = raw[o6 + 3];
		}
		return { w: w, h: h, n: 3, data: out, alpha: alpha, mask: false };
	}
	var samples = unpack_samples(raw, w, h, ncomp, depth, ctype == 3);
	out = new Uint8Array(np * 3);
	var i;
	if (ctype == 3) {
		for (i = 0; i < np; i++) {
			var ix = samples[i] * 3;
			out[i * 3] = pal ? pal[ix] : samples[i];
			out[i * 3 + 1] = pal ? pal[ix + 1] : samples[i];
			out[i * 3 + 2] = pal ? pal[ix + 2] : samples[i];
		}
		if (trns) {
			alpha = new Uint8Array(np);
			for (i = 0; i < np; i++) {
				var t = trns[samples[i]];
				alpha[i] = t === undefined ? 255 : t;
			}
		}
	}
	else if (ctype == 0 || ctype == 4) {
		if (ctype == 4)
			alpha = new Uint8Array(np);
		for (i = 0; i < np; i++) {
			var gv = samples[i * ncomp];
			out[i * 3] = gv;
			out[i * 3 + 1] = gv;
			out[i * 3 + 2] = gv;
			if (alpha)
				alpha[i] = samples[i * ncomp + 1];
		}
	}
	else {
		if (ctype == 6)
			alpha = new Uint8Array(np);
		for (i = 0; i < np; i++) {
			out[i * 3] = samples[i * ncomp];
			out[i * 3 + 1] = samples[i * ncomp + 1];
			out[i * 3 + 2] = samples[i * ncomp + 2];
			if (alpha)
				alpha[i] = samples[i * ncomp + 3];
		}
	}
	return { w: w, h: h, n: 3, data: out, alpha: alpha, mask: false };
}


/* GIF (first frame; interlaced and transparent images supported) */
function gif_decode(b)
{
	var p = 6;
	var sw = b[p] | (b[p + 1] << 8), sh = b[p + 2] | (b[p + 3] << 8), flags = b[p + 4];
	p += 7;
	var gpal = null, i;
	if (flags & 0x80) {
		gpal = bytes_sub(b, p, p + 3 * (2 << (flags & 7)));
		p += 3 * (2 << (flags & 7));
	}
	var trans = -1;
	while (p < b.length) {
		var t = b[p++];
		if (t == 0x21) {	/* extension */
			var label = b[p++];
			if (label == 0xf9 && b[p] >= 4 && (b[p + 1] & 1))
				trans = b[p + 4];
			while (p < b.length && b[p])
				p += b[p] + 1;
			p++;
		}
		else if (t == 0x2c)
			break;
		else
			throw "Invalid GIF image";
	}
	var ix = b[p] | (b[p + 1] << 8), iy = b[p + 2] | (b[p + 3] << 8);
	var w = b[p + 4] | (b[p + 5] << 8), h = b[p + 6] | (b[p + 7] << 8), lf = b[p + 8];
	p += 9;
	var pal = gpal;
	if (lf & 0x80) {
		pal = bytes_sub(b, p, p + 3 * (2 << (lf & 7)));
		p += 3 * (2 << (lf & 7));
	}
	if (!pal || !w || !h)
		throw "Invalid GIF image";
	var minc = b[p++];
	var blocks = [], total = 0;
	while (p < b.length && b[p]) {
		blocks.push(bytes_sub(b, p + 1, p + 1 + b[p]));
		total += b[p];
		p += b[p] + 1;
	}
	var data = new Uint8Array(total), o = 0;
	for (i = 0; i < blocks.length; i++) {
		data.set(blocks[i], o);
		o += blocks[i].length;
	}
	/* LZW, least-significant-bit first */
	var idx = new Uint8Array(w * h), ip = 0;
	var clear = 1 << minc, eoi = clear + 1, next = eoi + 1, bits = minc + 1;
	var prefix = new Int32Array(4096), suffix = new Uint8Array(4096), stack = new Uint8Array(4097);
	var bb = 0, bc = 0, dp = 0, prev = -1, first = 0;
	while (ip < idx.length) {
		while (bc < bits && dp < data.length) {
			bb |= data[dp++] << bc;
			bc += 8;
		}
		if (bc < bits)
			break;
		var code = bb & ((1 << bits) - 1);
		bb >>>= bits;
		bc -= bits;
		if (code == clear) {
			next = eoi + 1;
			bits = minc + 1;
			prev = -1;
			continue;
		}
		if (code == eoi)
			break;
		var sp = 0, c = code;
		if (prev < 0) {
			idx[ip++] = code;
			first = code;
			prev = code;
			continue;
		}
		if (code >= next) {
			stack[sp++] = first;
			c = prev;
		}
		while (c >= clear + 2 && sp < 4096) {
			stack[sp++] = suffix[c];
			c = prefix[c];
		}
		first = c < clear ? c : 0;
		stack[sp++] = c;
		while (sp > 0 && ip < idx.length)
			idx[ip++] = stack[--sp];
		if (next < 4096) {
			prefix[next] = prev;
			suffix[next] = first;
			next++;
			if (next == (1 << bits) && bits < 12)
				bits++;
		}
		prev = code;
	}
	/* de-interlace */
	var rows = [];
	for (i = 0; i < h; i++)
		rows[i] = i;
	if (lf & 0x40) {
		rows = [];
		var passes = [[0, 8], [4, 8], [2, 4], [1, 2]];
		for (var ps = 0; ps < 4; ps++)
			for (var y = passes[ps][0]; y < h; y += passes[ps][1])
				rows.push(y);
	}
	var out = new Uint8Array(sw * sh * 3), alpha = new Uint8Array(sw * sh);
	for (i = 0; i < h; i++) {
		var dy = (lf & 0x40 ? rows[i] : i) + iy;
		if (dy >= sh)
			continue;
		for (var x = 0; x < w; x++) {
			var dx = x + ix;
			if (dx >= sw)
				continue;
			var ci = idx[i * w + x];
			var q = (dy * sw + dx);
			out[q * 3] = pal[ci * 3];
			out[q * 3 + 1] = pal[ci * 3 + 1];
			out[q * 3 + 2] = pal[ci * 3 + 2];
			alpha[q] = ci == trans ? 0 : 255;
		}
	}
	return { w: sw, h: sh, n: 3, data: out, alpha: trans >= 0 ? alpha : null, mask: false };
}

/* Image size of a JPEG without decoding it */
function jpeg_dims(b)
{
	var p = 2;
	while (p + 9 < b.length) {
		if (b[p] != 0xff) {
			p++;
			continue;
		}
		var m = b[p + 1];
		if (m == 0xff || m == 0 || m == 0x01 || (m >= 0xd0 && m <= 0xd8)) {
			p += (m == 0xff) ? 1 : 2;
			continue;
		}
		if (m == 0xc0 || m == 0xc1)
			return { h: (b[p + 5] << 8) | b[p + 6], w: (b[p + 7] << 8) | b[p + 8] };
		if (m >= 0xc2 && m <= 0xcf && m != 0xc4 && m != 0xc8 && m != 0xcc)
			throw "Unsupported JPEG (progressive and arithmetic-coded files can't be decoded)";
		p += 2 + ((b[p + 2] << 8) | b[p + 3]);
	}
	throw "Invalid JPEG image";
}

/* Decode JPEG data at 1, 1/2, 1/4 or 1/8 detail (N = 1, 2, 4 or 8 pixels per 8x8 block side) */
function jpeg_image(b, N, abort)
{
	var j = jpeg_decode(b, N || 8, abort || null);
	if (!j)
		throw "Unsupported JPEG (progressive and arithmetic-coded files can't be decoded)";
	if (j.n == 4) {
		var rgb = new Uint8Array(j.w * j.h * 3), k = 0;
		for (var i = 0; i < j.w * j.h; i++) {
			/* Adobe CMYK JPEGs store inverted values */
			var kk = j.data[i * 4 + 3];
			rgb[k++] = j.data[i * 4] * kk / 255;
			rgb[k++] = j.data[i * 4 + 1] * kk / 255;
			rgb[k++] = j.data[i * 4 + 2] * kk / 255;
		}
		return { w: j.w, h: j.h, n: 3, data: rgb, alpha: null, mask: false };
	}
	return { w: j.w, h: j.h, n: j.n, data: j.data, alpha: null, mask: false };
}

/* Load an image file (Sixel, PNG, JPEG, GIF, BMP, PPM/PGM) as { w, h, n, data, alpha, mask } */
function load_image_file(filename)
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
	if (!s || s.length < 8)
		throw "File is empty or too short";
	var c0 = s.charCodeAt(0), c1 = s.charCodeAt(1), c2 = s.charCodeAt(2);
	if (c0 == 0x1b || /\.six(el)?$/i.test(filename))
		return sixel_decode(s);
	var b = string_to_bytes(s);
	if (c0 == 0x89 && s.substr(1, 3) == "PNG")
		return png_decode(b);
	if (c0 == 0xff && c1 == 0xd8) {
		/* decoded later, at a detail level suited to the zoom (see ImageDoc.render) */
		var jd = jpeg_dims(b);
		return { w: jd.w, h: jd.h, n: 3, data: null, alpha: null, mask: false, jpeg: b };
	}
	if (c0 == 0x47 && c1 == 0x49 && c2 == 0x46)
		return gif_decode(b);
	if (c0 == 0x42 && c1 == 0x4d)
		return bmp_decode(b);
	if (c0 == 0x50 && (c1 == 0x35 || c1 == 0x36))
		return ppm_decode(b);
	throw "Unrecognised image format";
}

/* A one-page "document" wrapping a raster image, so the viewer can display it */
function ImageDoc(img)
{
	this.img = img;
	this.initial_fit = "page";
	this.actual_scale = 1;
	this.pages = [{ box: [0, 0, img.w, img.h], rotate: 0 }];
}

ImageDoc.prototype.get_pages = function ()
{
	return this.pages;
};

ImageDoc.prototype.render = function (page, M, clip, abort_cb, maxN)
{
	var img = this.img;
	CLIPMASK = null;
	LAST_DEGRADED = false;
	if (img.jpeg) {
		/* decode at a reduced size when the image is shown small; a quick low-detail
		 * pass first (maxN = 1), refined by a second pass */
		var ratio = 1 / Math.sqrt(M[0] * M[0] + M[1] * M[1]);
		var need = ratio >= 6 ? 1 : (ratio >= 3 ? 2 : (ratio >= 1.5 ? 4 : 8));
		var N = Math.min(need, maxN || 8);
		if (N < need)
			LAST_DEGRADED = true;
		if (!this.cache || this.cache.N < N) {
			try {
				this.cache = { N: N, img: jpeg_image(img.jpeg, N, abort_cb) };
			}
			catch (e) {
				if (e === "aborted")
					return false;
				throw e;
			}
		}
		img = this.cache.img;
	}
	draw_image(img, mat_mul([page.box[2], 0, 0, page.box[3], 0, 0], M), 1, [0, 0, 0], clip);
	return true;
};

/* ----- Terminal I/O ----- */

/* Send a Sixel image.  If the user presses a key part-way through (e.g. over a slow
 * connection) the image is terminated early so they don't have to wait for it. */
function send_sixel(data)
{
	var pos = 0;
	while (pos < data.length && bbs.online && !js.terminated) {
		if (pos > 0 && input_pending()) {
			console.write("\x1b\\");
			return false;
		}
		var space = console.output_buffer_space;
		if (space < 512) {
			mswait(2);
			continue;
		}
		var n = Math.min(space - 64, 8192, data.length - pos);
		console.write(data.substr(pos, n));
		pos += n;
	}
	return true;
}

function status_line(text)
{
	var cols = console.screen_columns;
	if (text.length > cols - 1)
		text = text.substr(0, cols - 1);
	while (text.length < cols - 1)
		text += " ";
	console.gotoxy(1, console.screen_rows);
	console.print("\x01n\x01h\x01w\x014" + text.replace(/\x01/g, "") + "\x01n");
}

var HELP_TEXT = "Arrows:scroll +/-:zoom W:width F:page 1:actual PgUp/PgDn N/P:page G:goto V:invert D:dither Q:quit";

/* True if the user has pressed a key that hasn't been read yet */
function input_pending()
{
	return console.keyboard_buffer_level > 0;
}

/* Interactive full-screen viewer for a document object (a PDFDoc or an ImageDoc) */
function viewer(doc, startpage)
{
	var cterm = load({}, "cterm_lib.js");
	var forced = argv.indexOf("force") >= 0;
	if (!forced && !cterm.supports_sixel()) {
		console.crlf();
		console.print("\x01n\x01h\x01rSorry, your terminal does not support Sixel graphics.\x01n\r\n");
		console.print("(SyncTERM 1.2 or later is required.)\r\n");
		console.pause();
		return;
	}
	var geo = cterm.cterm_screen_geometry();
	var W = Math.min(1024, geo.pxW);
	var H = Math.min(768, geo.pxH - geo.cellH);
	H -= H % 6;
	var view = new View(doc, W, H);
	if (startpage > 0)
		view.page = Math.min(view.pages.length - 1, startpage - 1);
	if (doc.initial_fit == "page")
		view.fit_page();
	else
		view.fit_width();
	var actual = doc.actual_scale || (96 / 72);
	console.write("\x1b[?80l");
	console.clear();
	var need = true;
	var showhelp = false;
	var np = view.pages.length;
	while (input_pending())	/* discard any stray keystrokes (e.g. the Enter that launched us) */
		console.inkey(K_NONE, 0);

	function abort_cb()
	{
		return input_pending() || !bbs.online;
	}

	function draw()
	{
		status_line(np > 1 ? " Rendering page " + (view.page + 1) + "/" + np + " ..." : " Rendering ...");
		/* quick pass with reduced JPEG detail first, then refine if needed */
		var done = view.render(abort_cb, 1);
		if (!done)
			return false;
		var degraded = LAST_DEGRADED;
		console.write("\x1b[?80l");
		console.gotoxy(1, 1);
		send_sixel(sixel_encode(FB, FW, FH));
		if (degraded) {
			status_line(" Refining images (press a key to skip) ...");
			if (!view.render(abort_cb, 8))
				return true;	/* interrupted: the quick version is on screen */
			console.write("\x1b[?80l");
			console.gotoxy(1, 1);
			send_sixel(sixel_encode(FB, FW, FH));
		}
		return true;
	}

	function show_status()
	{
		status_line(showhelp ? " " + HELP_TEXT :
			" " + (np > 1 ? "Page " + (view.page + 1) + "/" + np + "  " : "") + "Zoom " + Math.round(view.scale / actual * 100) + "%   [?] help  [Q] quit");
	}

	while (bbs.online && !js.terminated) {
		if (need) {
			if (draw()) {
				need = false;
				show_status();
			}
			else if (!input_pending())
				need = false;
		}
		var key = need ? console.inkey(K_NONE, 0) : console.getkey(K_NONE);
		if (!key) {
			if (!bbs.online)
				break;
			if (bbs.sys_status & SS_ABORT) {
				bbs.sys_status &= ~SS_ABORT;
				break;
			}
			continue;
		}
		var stepx = Math.max(16, Math.round(view.W / 8)), stepy = Math.max(16, Math.round(view.H / 8));
		for (;;) {
			var d = view.dims();
			var k = key;
			var redraw = true;
			switch (k) {
				case KEY_UP: view.sy -= stepy; break;
				case KEY_DOWN: view.sy += stepy; break;
				case KEY_LEFT: view.sx -= stepx; break;
				case KEY_RIGHT: view.sx += stepx; break;
				case "k": view.sy -= stepy; break;
				case "j": view.sy += stepy; break;
				case "h": view.sx -= stepx; break;
				case "l": view.sx += stepx; break;
				case KEY_PAGEDN: case " ": case "\r":
					if (view.sy >= d.h - view.H + VIEW_MARGIN - 1 && view.page < np - 1)
						view.set_page(view.page + 1, false);
					else
						view.sy += Math.round(view.H * 0.9);
					break;
				case KEY_PAGEUP: case "\b":
					if (view.sy <= -VIEW_MARGIN + 1 && view.page > 0)
						view.set_page(view.page - 1, true);
					else
						view.sy -= Math.round(view.H * 0.9);
					break;
				case KEY_HOME: view.sx = -1e9; view.sy = -1e9; break;
				case KEY_END: view.sx = -1e9; view.sy = 1e9; break;
				case "+": case "=": case "i": case "I": view.zoom(1.25); break;
				case "-": case "_": case "o": case "O": view.zoom(0.8); break;
				case "v": case "V": SIX_INVERT = !SIX_INVERT; break;
				case "d": case "D": SIX_DITHER = !SIX_DITHER; break;
				case "w": case "W": view.fit_width(); break;
				case "f": case "F": view.fit_page(); break;
				case "1": view.zoom(actual / view.scale); break;
				case "n": case "N":
					if (view.page < np - 1)
						view.set_page(view.page + 1, false);
					break;
				case "p": case "P":
					if (view.page > 0)
						view.set_page(view.page - 1, false);
					break;
				case "g": case "G":
					status_line(" Go to page (1-" + np + "): ");
					console.gotoxy(24 + String(np).length, console.screen_rows);
					var pn = console.getnum(np);
					if (pn > 0)
						view.set_page(pn - 1, false);
					break;
				case "?": showhelp = !showhelp; show_status(); redraw = false; break;
				case "q": case "Q": case "\x1b": case "\x03":
					console.write("\x1b[?80l");
					console.clear();
					return;
				default:
					redraw = false;
			}
			if (redraw) {
				view.clamp();
				need = true;
			}
			if (!input_pending())
				break;
			key = console.inkey(K_NONE, 0);
			if (!key)
				break;
		}
	}
}

