/* univiewer_lib.js
 *
 * Terminal image/document viewer library: the full-screen scroll/zoom viewer used by
 * pdfview.js and univiewer.js, plus Sixel/PNG/JPEG/GIF/BMP/PPM image loading.
 * Requires pdf2sixel_lib.js (rasterizer, Sixel encoder, JPEG decoder) and, for PNG,
 * inflate.js.  Tab size is 4.
 *
 *	load("univiewer_lib.js");
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

/* Adam7-interlaced PNG data -> samples (ncomp per pixel, 8 bits each) in normal pixel order */
function png_adam7(data, w, h, ncomp, depth, indexed, fake)
{
	var passes = [[0, 0, 8, 8], [4, 0, 8, 8], [0, 4, 4, 8], [2, 0, 4, 4], [0, 2, 2, 4], [1, 0, 2, 2], [0, 1, 1, 2]];
	var out = new Uint8Array(w * h * ncomp), pos = 0;
	for (var ps = 0; ps < 7; ps++) {
		var x0 = passes[ps][0], y0 = passes[ps][1], dx = passes[ps][2], dy = passes[ps][3];
		var pw = w > x0 ? Math.ceil((w - x0) / dx) : 0, ph = h > y0 ? Math.ceil((h - y0) / dy) : 0;
		if (!pw || !ph)
			continue;
		var rowbytes = Math.ceil(pw * ncomp * depth / 8);
		var size = (rowbytes + 1) * ph;
		var seg = bytes_sub(data, pos, pos + size);
		pos += size;
		var raw = apply_predictor(seg, { Predictor: 15, Colors: ncomp, BitsPerComponent: depth, Columns: pw }, fake);
		var s = unpack_samples(raw, pw, ph, ncomp, depth, indexed);
		for (var y = 0; y < ph; y++) {
			for (var x = 0; x < pw; x++) {
				var d = ((y0 + y * dy) * w + x0 + x * dx) * ncomp, q = (y * pw + x) * ncomp;
				for (var k = 0; k < ncomp; k++)
					out[d + k] = s[q + k];
			}
		}
	}
	return out;
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
	var comp = new Uint8Array(total), o = 0, i;
	for (i = 0; i < idat.length; i++) {
		comp.set(idat[i], o);
		o += idat[i].length;
	}
	var ncomp = ctype == 0 ? 1 : (ctype == 2 ? 3 : (ctype == 3 ? 1 : (ctype == 4 ? 2 : 4)));
	var fake = { resolve: function (v) { return v; } };
	var raw = null, samples = null;
	if (interlace)
		samples = png_adam7(inflate_zlib(comp), w, h, ncomp, depth, ctype == 3, fake);
	else
		raw = apply_predictor(inflate_zlib(comp), { Predictor: 15, Colors: ncomp, BitsPerComponent: depth, Columns: w }, fake);
	if (raw && depth == 8 && !trns && (ctype == 2 || ctype == 0) && raw.length >= w * h * ncomp)
		return { w: w, h: h, n: ncomp, data: bytes_sub(raw, 0, w * h * ncomp), alpha: null, mask: false };
	var out, alpha = null, np = w * h;
	if (raw && depth == 8 && !trns && ctype == 6 && raw.length >= np * 4) {
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
	if (!samples)
		samples = unpack_samples(raw, w, h, ncomp, depth, ctype == 3);
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


/* GIF: every frame is composited into a full-size picture (honoring disposal and transparency).
 * The result is the first frame, with .frames (all of them, as { data, alpha, delay }) if there is more than one. */
function gif_lzw(b, p, minc, total_pixels)
{
	var blocks = [], total = 0, i;
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
	var idx = new Uint8Array(total_pixels), ip = 0;
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
	while (p < b.length && b[p])
		p += b[p] + 1;
	return { idx: idx, end: p + 1 };
}

function gif_decode(b)
{
	var p = 6;
	var sw = b[p] | (b[p + 1] << 8), sh = b[p + 2] | (b[p + 3] << 8), flags = b[p + 4];
	p += 7;
	if (!sw || !sh || sw * sh > 16 * 1024 * 1024)
		throw "Invalid GIF image";
	var gpal = null, i;
	if (flags & 0x80) {
		gpal = bytes_sub(b, p, p + 3 * (2 << (flags & 7)));
		p += 3 * (2 << (flags & 7));
	}
	var npx = sw * sh;
	var canvas = new Uint8Array(npx * 3), calpha = new Uint8Array(npx);
	var frames = [];
	var trans = -1, disposal = 0, delay = 10, usedtrans = false;
	var prevdisp = 0, prevrect = null, saved = null, savedalpha = null;
	var memory = 0;
	while (p < b.length) {
		var t = b[p++];
		if (t == 0x3b)
			break;
		if (t == 0x21) {	/* extension */
			var label = b[p++];
			if (label == 0xf9 && b[p] >= 4) {
				disposal = (b[p + 1] >> 2) & 7;
				delay = b[p + 2] | (b[p + 3] << 8);
				trans = (b[p + 1] & 1) ? b[p + 4] : -1;
			}
			while (p < b.length && b[p])
				p += b[p] + 1;
			p++;
		}
		else if (t == 0x2c) {
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
			var fr = gif_lzw(b, p, minc, w * h);
			p = fr.end;
			/* dispose of the previous frame first */
			if (prevdisp == 2 && prevrect) {
				for (var yy = prevrect[1]; yy < prevrect[1] + prevrect[3] && yy < sh; yy++)
					for (var xx = prevrect[0]; xx < prevrect[0] + prevrect[2] && xx < sw; xx++)
						calpha[yy * sw + xx] = 0;
			}
			else if (prevdisp == 3 && saved) {
				canvas.set(saved);
				calpha.set(savedalpha);
			}
			if (disposal == 3) {
				saved = new Uint8Array(canvas.length);
				saved.set(canvas);
				savedalpha = new Uint8Array(calpha.length);
				savedalpha.set(calpha);
			}
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
			for (i = 0; i < h; i++) {
				var dy = rows[i] + iy;
				if (dy >= sh)
					continue;
				for (var x = 0; x < w; x++) {
					var dx = x + ix;
					if (dx >= sw)
						continue;
					var ci = fr.idx[i * w + x];
					if (ci == trans) {
						usedtrans = true;
						continue;
					}
					var q = dy * sw + dx;
					canvas[q * 3] = pal[ci * 3];
					canvas[q * 3 + 1] = pal[ci * 3 + 1];
					canvas[q * 3 + 2] = pal[ci * 3 + 2];
					calpha[q] = 255;
				}
			}
			prevdisp = disposal;
			prevrect = [ix, iy, w, h];
			var fd = new Uint8Array(canvas.length);
			fd.set(canvas);
			var fa = new Uint8Array(calpha.length);
			fa.set(calpha);
			frames.push({ data: fd, alpha: fa, delay: Math.max(2, delay) * 10 });
			memory += npx * 4;
			if (frames.length >= 400 || memory > 96 * 1024 * 1024)
				break;
		}
		else
			throw "Invalid GIF image";
	}
	if (!frames.length)
		throw "Invalid GIF image";
	/* a frame's transparency only matters if something is transparent */
	var needalpha = false;
	for (i = 0; i < frames.length && !needalpha; i++) {
		{
			var al = frames[i].alpha;
			for (var k = 0; k < al.length; k++) {
				if (al[k] != 255) {
					needalpha = true;
					break;
				}
			}
		}
	}
	if (!needalpha) {
		for (i = 0; i < frames.length; i++)
			frames[i].alpha = null;
	}
	var f0 = frames[0];
	return { w: sw, h: sh, n: 3, data: f0.data, alpha: f0.alpha, mask: false, frames: frames.length > 1 ? frames : null };
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
		if (m == 0xc0 || m == 0xc1 || m == 0xc2)
			return { h: (b[p + 5] << 8) | b[p + 6], w: (b[p + 7] << 8) | b[p + 8] };
		if (m >= 0xc3 && m <= 0xcf && m != 0xc4 && m != 0xc8 && m != 0xcc)
			throw "Unsupported JPEG (arithmetic-coded and lossless files can't be decoded)";
		p += 2 + ((b[p + 2] << 8) | b[p + 3]);
	}
	throw "Invalid JPEG image";
}

/* Decode JPEG data at 1, 1/2, 1/4 or 1/8 detail (N = 1, 2, 4 or 8 pixels per 8x8 block side) */
function jpeg_image(b, N, abort)
{
	var j = jpeg_decode(b, N || 8, abort || null);
	if (!j)
		throw "Unsupported JPEG (arithmetic-coded and lossless files can't be decoded)";
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
	return image_from_string(s, filename);
}

/* Decode an image held in a string of raw bytes; filename is only used for its extension (and by the
 * text-mode art loaders, which read the file themselves) */
function image_from_string(s, filename)
{
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
	if (s.substr(0, 5) == "XBIN\x1a")
		return xbin_picture(filename);
	if (/\.bin$/i.test(filename))
		return bin_picture(filename);
	if (s.substr(0, 4) == "FORM" && (s.substr(8, 4) == "ILBM" || s.substr(8, 4) == "PBM "))
		return ilbm_decode(b);
	if ((s.substr(0, 4) == "II*\x00" || s.substr(0, 4) == "MM\x00*"))
		return tiff_decode(b);
	if (c0 == 0 && c1 == 0 && c2 == 1 && s.charCodeAt(3) == 0)
		return ico_decode(b);
	if (c0 == 10 && b[2] == 1 && (b[1] == 5 || b[1] == 3 || b[1] == 2 || b[1] == 0 || b[1] == 4))
		return pcx_decode(b);
	if (/\.tga$/i.test(filename))
		return tga_decode(b);
	throw "Unrecognized image format";
}

/* A one-page "document" wrapping a raster image, so the viewer can display it */
function ImageDoc(img)
{
	this.img = img;
	/* a very tall picture (long ANSI/XBin art, a scroll) opens at full width and is scrolled; fitting the whole of it
	 * on the screen would make it too small to read */
	this.initial_fit = img.textmode ? "actual" : (img.h > img.w * 2 ? "width" : "page");
	this.actual_scale = 1;
	this.pages = [{ box: [0, 0, img.w, img.h], rotate: 0 }];
	/* animated GIFs: img.frames is a list of { data, alpha, delay (ms) } */
	this.nframes = img.frames ? img.frames.length : 1;
	this.frame = 0;
}

/* The picture to draw now */
ImageDoc.prototype.current = function ()
{
	if (this.nframes < 2)
		return this.img;
	var f = this.img.frames[this.frame];
	return { w: this.img.w, h: this.img.h, n: 3, data: f.data, alpha: f.alpha, mask: false };
};

ImageDoc.prototype.next_frame = function ()
{
	this.frame = (this.frame + 1) % this.nframes;
};

ImageDoc.prototype.frame_delay = function ()
{
	return this.nframes < 2 ? 0 : this.img.frames[this.frame].delay;
};

ImageDoc.prototype.get_pages = function ()
{
	return this.pages;
};

ImageDoc.prototype.render = function (page, M, clip, abort_cb, maxN)
{
	var img = this.current();
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

/* Status line made of plain text and menu items such as "(C)ontents": text inside ( ) is yellow, the rest white.
 * Optional items are dropped, last first, if the line would not fit. */
/*
 * Clickable bottom menu bars.  Each bar records where its items are (bar_add), and a mouse click on the last
 * screen line (bar_key) is taken as that key being pressed.  Items with
 * two keys (Up/Dn, PgUp/PgDn, N/P ...) send the first key when the part before the slash is clicked and the
 * second for the part after it (on the description, the left or right half of it).
 */
var BAR_SPOTS = [];

function bar_label_keys(label)
{
	switch (label) {
		case "Enter": return ["\r"];
		case "Up/Dn": return [KEY_UP, KEY_DOWN];
		case "Left/Right": return [KEY_LEFT, KEY_RIGHT];
		case "PgUp/PgDn": return [KEY_PAGEUP, KEY_PAGEDN];
		case "Home/End": return [KEY_HOME, KEY_END];
		case "N/P": return ["n", "p"];
		case "</>": return ["<", ">"];
		case "Tab": return ["\t"];
	}
	return label.length == 1 ? [label.toLowerCase()] : [];
}

function bar_add(x1, x2, label, labeloff)
{
	var slash = label.indexOf("/");
	/* labeloff: how far into the item the label starts (1 for "(C)ontents", whose label sits inside the parentheses) */
	BAR_SPOTS.push({ x1: x1, x2: x2, lx: x1 + (labeloff || 0), keys: bar_label_keys(label), llen: label.length, slash: slash });
}

/* A click on the menu bar: returns the key it stands for, "" if it was on the bar but not on an item, or null
 * if it wasn't on the bar at all.  The key is used directly rather than pushed back into the input, because
 * control characters pushed back would be taken by Synchronet as its own hot keys (PgUp as Ctrl-P ...). */
function bar_key(m)
{
	if (!m || m.y != console.screen_rows)
		return null;
	for (var i = 0; i < BAR_SPOTS.length; i++) {
		var sp = BAR_SPOTS[i];
		if (m.x >= sp.x1 && m.x <= sp.x2) {
			var second = false;
			if (sp.keys.length > 1) {
				/* on the key name (Up/Dn): the half either side of the slash; on its description: the halves of that */
				if (m.x < sp.lx + sp.llen)
					second = sp.slash >= 0 && m.x > sp.lx + sp.slash;
				else
					second = m.x > (sp.lx + sp.llen + 1 + sp.x2) / 2;
			}
			return sp.keys[second ? 1 : 0] || "";
		}
	}
	return "";
}

function status_menu(prefix, items, optional)
{
	var max = console.screen_columns - 2;
	var all = items.concat(optional);
	var text;
	for (;;) {
		text = prefix + all.join("  ");
		if (text.length <= max || all.length <= 1)
			break;
		/* drop the last optional item that is still present, else the last item */
		var dropped = false;
		for (var i = all.length - 1; i >= 0; i--) {
			if (optional.indexOf(all[i]) >= 0) {
				all.splice(i, 1);
				dropped = true;
				break;
			}
		}
		if (!dropped)
			all.splice(all.length - 2, 1);
	}
	if (text.length > max)
		text = text.substr(0, max);
	BAR_SPOTS = [];
	var sm, smre = /\(([^)]+)\)( \w+|[A-Za-z]*)/g;
	while ((sm = smre.exec(text)) !== null)
		bar_add(sm.index + 1, sm.index + sm[0].length, sm[1], 1);
	var white = "\x01n\x01h\x01w\x014", yellow = "\x01n\x01h\x01y\x014";
	var colored = text.replace(/\([^)]+\)/g, function (m) { return yellow + m + white; });
	var pad = max - text.length;
	while (pad-- > 0)
		colored += " ";
	console.gotoxy(1, console.screen_rows);
	console.print(white + colored.replace(/\x01(?![nhyw4])/g, "") + "\x01n");
}

var HELP_TEXT = "Arrows:scroll(j/k:small) +/-:zoom W:width F:page 1:actual PgUp/PgDn <>:page G:goto /:find m/M:next/prev match V:invert D:dither C:contents Tab:links Enter:follow b:back L:show links Q:quit";

/* True if the user has pressed a key that hasn't been read yet */
function input_pending()
{
	return console.keyboard_buffer_level > 0;
}

/* Interactive full-screen viewer for a document object (a PDFDoc or an ImageDoc) */
/* If browse is true, N and P make viewer() return "next"/"prev" so a file browser can move to the
 * neighboring file (PDF pages are then changed with PgUp/PgDn or G instead). */
/*
 * Mouse support.  With the terminal's mouse reporting on, Synchronet turns the scroll wheel into the Up/Down
 * arrow keys by itself.  Lists also ask for click reports (mouse_clicks); a click then arrives as an ESC
 * sequence that read_mouse() decodes.  If the user's terminal settings don't include mouse support nothing
 * happens and these all do nothing.
 */
var MOUSE_STARTED = false;
var RIGHT_CLICKED = false;	/* a screen was just left with the right mouse button (see swallow_right_click) */

function mouse_start()
{
	if (MOUSE_STARTED)
		return;
	/* Ask Synchronet to turn mouse reporting on.  (Reading console.mouse_mode back does not show
	 * whether that worked, so it can't be used to check.)  Then also ask the terminal directly, in
	 * case Synchronet did nothing; asking twice is harmless. */
	try {
		/* CON_MOUSE_SCROLL stops Synchronet switching mouse reporting off whenever the screen is cleared */
		console.status |= CON_MOUSE_SCROLL;
		console.mouse_mode = MOUSE_MODE_NORM | MOUSE_MODE_EXT;
	}
	catch (e) {}
	try {
		console.write("\x1b[?1000h\x1b[?1006h");
		MOUSE_STARTED = true;
	}
	catch (e2) {}
}

function mouse_stop()
{
	try {
		console.status &= ~(CON_MOUSE_CLK_PASSTHRU | CON_MOUSE_SCROLL);
	}
	catch (e) {}
	try {
		if (MOUSE_STARTED) {
			console.mouse_mode = MOUSE_MODE_OFF;
			console.write("\x1b[?1006l\x1b[?1000l");
		}
	}
	catch (e2) {}
	MOUSE_STARTED = false;
}

/*
 * The terminal sends the right button press and its release, and Synchronet turns each into Ctrl-C.  After
 * a screen was left that way, throw the second one away so it doesn't also close the screen underneath.
 */
function swallow_right_click()
{
	if (!RIGHT_CLICKED)
		return;
	RIGHT_CLICKED = false;
	var k;
	while ((k = console.inkey(K_NONE, 150)) === "\x03")
		;
	if (k !== "" && k !== undefined)
		console.ungetstr(k);
	bbs.sys_status &= ~SS_ABORT;
}

/* Tell the terminal again that mouse reports are wanted (after anything that might have cleared that) */
function mouse_refresh()
{
	if (!MOUSE_STARTED)
		return;
	try {
		console.write("\x1b[?1000h\x1b[?1006h");
	}
	catch (e) {}
}

/* Ask for (or stop asking for) click reports while a list is on screen */
function mouse_clicks(on)
{
	if (!MOUSE_STARTED)
		return;
	if (on)
		console.status |= CON_MOUSE_CLK_PASSTHRU;
	else
		console.status &= ~CON_MOUSE_CLK_PASSTHRU;
}

/*
 * Call right after getkey() returned ESC while clicks are being reported.  Returns false for a plain Esc
 * key press, null for some other escape sequence (ignore it), or { button, x, y } (x and y from 1) for a
 * mouse button press.
 */
function read_mouse()
{
	if (!MOUSE_STARTED)
		return false;
	var c = console.inkey(K_NONE, 30);
	if (c === "" || c === undefined)
		return false;
	if (c !== "[") {
		console.ungetstr(c);
		return false;
	}
	var seq = "";
	for (var i = 0; i < 24; i++) {
		c = console.inkey(K_NONE, 100);
		if (c === "" || c === undefined)
			break;
		seq += c;
		if (/[A-Za-z~]/.test(c))
			break;
	}
	var m = seq.match(/^<(\d+);(\d+);(\d+)M$/);
	if (!m)
		return null;
	return { button: parseInt(m[1], 10), x: parseInt(m[2], 10), y: parseInt(m[3], 10) };
}

function viewer(doc, startpage, browse)
{
	mouse_start();
	/* clicks on the menu bar; the list this was opened from has stopped asking for clicks by now */
	var clicks_were_on = MOUSE_STARTED && (console.status & CON_MOUSE_CLK_PASSTHRU) != 0;
	mouse_clicks(true);
	try {
		return viewer_inner(doc, startpage, browse);
	}
	finally {
		mouse_clicks(clicks_were_on);
	}
}

function viewer_inner(doc, startpage, browse)
{
	mouse_start();
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
	if (doc.initial_fit == "actual")		/* text-mode art: one screen pixel per picture pixel, so the font stays crisp */
		view.zoom(actual / view.scale);
	console.write("\x1b[?80l");
	console.clear();
	var need = true;
	var showhelp = false;
	/* ---- links (PDF only): Tab/] next, [ previous, Enter follow, b back, L show all ---- */
	var linksel = -1, linkpage = view.page, showlinks = false, linkmsg = "";
	var linkresult = "";		/* set when a link leads to another document (web pages) */
	var noticed = false;	/* a document may report, after its first render, that parts of it can't be drawn */
	var animating = doc.nframes > 1;		/* animated GIF: A starts and stops it */
	var silent = false;		/* true while redrawing only because the next animation frame is due */
	var history = [];
	var base = null;		/* copy of the rendered page without the link overlay */
	var has_links = typeof doc.page_links == "function";

	function cur_links()
	{
		return has_links ? doc.page_links(view.page) : [];
	}

	/* Device-pixel box of a link: [x0, y0, x1, y1] */
	function link_box(l)
	{
		var M = page_matrix(view.pages[view.page], view.scale, -view.sx, -view.sy);
		var a = mat_apply(M, l.rect[0], l.rect[1]), b = mat_apply(M, l.rect[2], l.rect[3]);
		return [Math.min(a[0], b[0]), Math.min(a[1], b[1]), Math.max(a[0], b[0]), Math.max(a[1], b[1])];
	}

	function frame_rect(x0, y0, x1, y1, thick, rgb)
	{
		x0 = Math.max(0, Math.round(x0)); y0 = Math.max(0, Math.round(y0));
		x1 = Math.min(FW - 1, Math.round(x1)); y1 = Math.min(FH - 1, Math.round(y1));
		if (x1 < x0 || y1 < y0)
			return;
		var x, y, t;
		for (t = 0; t < thick; t++) {
			for (x = x0; x <= x1; x++) {
				if (y0 + t <= y1) FB[(y0 + t) * FW + x] = rgb;
				if (y1 - t >= y0) FB[(y1 - t) * FW + x] = rgb;
			}
			for (y = y0; y <= y1; y++) {
				if (x0 + t <= x1) FB[y * FW + x0 + t] = rgb;
				if (x1 - t >= x0) FB[y * FW + x1 - t] = rgb;
			}
		}
	}

	function overlay()
	{
		search_overlay();
		if (!has_links || (!showlinks && linksel < 0))
			return;
		var links = cur_links();
		for (var i = 0; i < links.length; i++) {
			if (i == linksel)
				continue;
			if (showlinks) {
				var b = link_box(links[i]);
				frame_rect(b[0], b[1], b[2], b[3], 1, 0x0070e0);
			}
		}
		if (linksel >= 0 && linksel < links.length) {
			var s = link_box(links[linksel]);
			frame_rect(s[0] - 2, s[1] - 2, s[2] + 2, s[3] + 2, 2, 0xffd000);
		}
	}

	/* ---- animated GIFs ----
	 * Drawing, quantizing and sending every frame in full is far too slow to keep to the GIF's timing,
	 * so the whole animation is prepared once when it starts: every frame is rendered and mapped to ONE
	 * palette (so that a pixel that doesn't change always has the same color), and for each frame only
	 * the part of the picture that differs from the previous frame is encoded.  The first frame is sent
	 * in full, then each following frame patches only what changed, on a steady clock. */
	var acache = null;		/* the prepared animation: { key, P, first, delta[], none } */
	var acur = 0;			/* the frame that is on screen */
	var adue = 0;			/* when (ms) the next frame is due */
	var ANIM_CACHE_MAX = 6000000;		/* characters of prepared sixel data to keep */

	function now_ms()
	{
		return system.timer * 1000;
	}

	/* Identifies what the prepared frames were rendered for; anything that changes it makes them stale */
	function anim_key()
	{
		return [view.page, view.scale, view.sx, view.sy, view.W, view.H, SIX_INVERT ? 1 : 0, SIX_DITHER ? 1 : 0].join(",");
	}

	function anim_ready()
	{
		return acache && !acache.none && acache.key == anim_key();
	}

	/* How long frame i stays up: the GIF's delay (browsers treat 10 ms or less as 100 ms), divided by the speed */
	var ASPEEDS = [0.25, 0.5, 0.75, 1, 1.5, 2, 3, 4];
	var ASPEED_DEF = 1;		/* index into ASPEEDS: half speed is the default */
	var aspeed = ASPEED_DEF;

	function frame_ms(i)
	{
		var d = doc.img.frames[i].delay;
		if (d <= 10)
			d = 100;
		return Math.max(20, d / ASPEEDS[aspeed]);
	}

	function anim_header()
	{
		console.write("\x1b[?80l");
		console.gotoxy(1, 1);
	}

	/* Render and quantize every frame, and encode them as a first frame plus patches.  Returns the
	 * prepared animation, { none: true } if it can't be used (too much data, or dithering is off, in
	 * which case frames are simply redrawn in full), or null if the user interrupted. */
	function build_animation()
	{
		var n = doc.nframes, key = anim_key();
		if (!SIX_DITHER)
			return { key: key, none: true };
		var P = sixel_palette_from_rgb(doc.img.frames.map(function (f) { return f.data; }), [0x585858, 0xffffff]);
		var q0 = null, prev = null, first = "", delta = [], total = 0, i, q, ch, str;
		for (i = 0; i < n; i++) {
			status_line(" Preparing animation, frame " + (i + 1) + "/" + n + " (press a key to cancel) ...");
			doc.frame = i;
			if (!view.render(abort_cb, 8)) {
				doc.frame = acur;
				return null;
			}
			q = sixel_map(FB, FW, FH, P);
			if (i == 0) {
				q0 = q;
				first = sixel_encode_q(q, P.pal, FW, FH, null);
				total = first.length;
			}
			else {
				ch = sixel_changed_bands(prev, q, FW, FH);
				str = ch ? sixel_encode_q(q, P.pal, FW, FH, ch) : "";
				delta.push(str);
				total += str.length;
			}
			prev = q;
			if (total > ANIM_CACHE_MAX) {
				doc.frame = acur;
				return { key: key, none: true };
			}
		}
		ch = sixel_changed_bands(prev, q0, FW, FH);		/* from the last frame back to the first */
		delta.push(ch ? sixel_encode_q(q0, P.pal, FW, FH, ch) : "");
		return { key: key, P: P, first: first, delta: delta };
	}

	/* Put the frame that should be on screen back, in full, in the animation's palette (after the screen
	 * was disturbed, e.g. by a popup).  Frames after this are patches to it. */
	function anim_resync()
	{
		doc.frame = acur;
		if (!view.render(abort_cb, 8))
			return false;
		var q = sixel_map(FB, FW, FH, acache.P);
		anim_header();
		send_sixel(sixel_encode_q(q, acache.P.pal, FW, FH, null));
		return true;
	}

	/* Show the next frame by drawing only what changed */
	function anim_step()
	{
		var t = now_ms();
		var due = Math.max(adue, t);
		var str = acache.delta[acur];
		if (str) {
			anim_header();
			send_sixel(str);
		}
		acur = (acur + 1) % doc.nframes;
		doc.frame = acur;
		adue = due + frame_ms(acur);
	}

	function emit_frame()
	{
		overlay();
		console.write("\x1b[?80l");
		console.gotoxy(1, 1);
		send_sixel(sixel_encode(FB, FW, FH));
	}

	function save_base()
	{
		base = new Int32Array(FB.length);
		base.set(FB);
	}

	/* Repaint with a changed highlight without re-rendering the page */
	function repaint()
	{
		if (anim_ready()) {
			anim_resync();
			return;
		}
		if (base) {
			FB.set(base);
			emit_frame();
		}
		else
			need = true;
	}

	/* Show the table of contents (bookmarks) and jump to the chosen entry */
	function contents()
	{
		var items = doc.outline();
		if (!items.length) {
			linkmsg = " This document has no table of contents";
			return false;
		}
		var W = console.screen_columns, H = console.screen_rows;
		var rows = H - 2, sel = 0, top = 0;
		for (var i = 0; i < items.length; i++) {	/* start on the entry for the current page */
			if (items[i].page <= view.page && items[i].page >= 0)
				sel = i;
		}
		function line(i, hi)
		{
			var it = items[i];
			var pg = it.page >= 0 ? String(it.page + 1) : "";
			var room = W - 2 - 8;
			var t = new Array(it.level * 2 + 1).join(" ") + (it.title || "(untitled)");
			if (t.length > room)
				t = t.substr(0, room);
			while (t.length < room)
				t += ".";
			if (hi)
				return "\x01n\x014\x01h\x01c " + t + " \x01w" + new Array(7 - pg.length).join(" ") + pg + " ";
			return "\x01n\x01c " + t + "\x01h\x01b " + new Array(7 - pg.length).join(" ") + pg + " ";
		}
		function draw_all()
		{
			console.write("\x1b[?80l");
			console.clear();
			console.gotoxy(1, 1);
			console.print("\x01n\x017\x01r Contents ");
			console.cleartoeol();
			for (var r = 0; r < rows; r++) {
				console.gotoxy(1, 2 + r);
				if (top + r < items.length)
					console.print(line(top + r, top + r == sel));
				console.print("\x01n");
				console.cleartoeol();
			}
			console.gotoxy(1, H);
			var tochelp = [["Enter", "Go to"], ["Up/Dn", "Select"], ["PgUp/PgDn", "Page"], ["Q", "Close"]];
			var tocs = "\x01n\x017", tx = 1;
			BAR_SPOTS = [];
			for (var th = 0; th < tochelp.length; th++) {
				tocs += "\x01r" + tochelp[th][0] + "\x01m)\x01b" + tochelp[th][1] + (th < tochelp.length - 1 ? "  " : " ");
				var tw = tochelp[th][0].length + 1 + tochelp[th][1].length;
				bar_add(tx, tx + tw - 1, tochelp[th][0]);
				tx += tw + 2;
			}
			console.print(tocs);
			console.cleartoeol();
			console.print("\x01n");
		}
		function fix_top()
		{
			if (sel < top)
				top = sel;
			if (sel >= top + rows)
				top = sel - rows + 1;
			if (top < 0)
				top = 0;
		}
		fix_top();
		top = Math.max(0, Math.min(Math.max(0, items.length - rows), sel - Math.floor(rows / 2)));
		draw_all();
		while (bbs.online) {
			var k = console.getkey(K_NONE);
			var old = sel;
			if (k == "\x1b") {
				var tmk = read_mouse();
				if (tmk !== false) {
					var tbk = tmk ? bar_key(tmk) : null;
					if (!tbk)
						continue;
					k = tbk;
				}
			}
			if (bbs.sys_status & SS_ABORT) {		/* right mouse button */
				bbs.sys_status &= ~SS_ABORT;
				k = "\x03";
			}
			switch (k) {
				case KEY_UP: sel--; break;
				case KEY_DOWN: sel++; break;
				case KEY_PAGEUP: sel -= rows - 1; break;
				case KEY_PAGEDN: sel += rows - 1; break;
				case KEY_HOME: sel = 0; break;
				case KEY_END: sel = items.length - 1; break;
				case "\r": case "\n":
					if (items[sel].page >= 0) {
						history.push({ page: view.page, scale: view.scale, sx: view.sx, sy: view.sy });
						view.set_page(items[sel].page, false);
						if (items[sel].y !== undefined) {
							var M = page_matrix(view.pages[view.page], view.scale, 0, 0);
							view.sy = mat_apply(M, items[sel].x !== undefined ? items[sel].x : 0, items[sel].y)[1] - 16;
						}
						linksel = -1;
						console.clear();
						return true;
					}
					break;
				case "q": case "Q": case "c": case "C": case KEY_ESC: case "\x03":
					if (k == "\x03")
						RIGHT_CLICKED = true;
					console.clear();
					return true;
			}
			sel = Math.max(0, Math.min(items.length - 1, sel));
			if (sel != old) {
				var oldtop = top;
				fix_top();
				if (top != oldtop)
					draw_all();
				else {
					console.gotoxy(1, 2 + old - top);
					console.print(line(old, false) + "\x01n");
					console.gotoxy(1, 2 + sel - top);
					console.print(line(sel, true) + "\x01n");
				}
			}
		}
		return true;
	}

	/* Pop-up box with the keys that apply to this document.  Up/Down scroll it if it is taller than the screen; any
	 * other key closes it. */
	function help_popup()
	{
		var L = [];		/* [key, description]; a heading has an empty key */
		L.push(["", "Moving around"]);
		L.push(["Arrow keys", "scroll (j / k in small steps)"]);
		L.push(["PgUp PgDn Space", "one screen" + (np > 1 ? " (turns page at the end)" : "")]);
		L.push(["Home End", "top / bottom"]);
		L.push(["", "Zoom"]);
		L.push(["+  -", "zoom in / out"]);
		L.push(["W  F  1", "fit width / fit page / actual size"]);
		if (np > 1) {
			L.push(["", "Pages"]);
			L.push(["<  >   ,  .", "previous / next page"]);
			L.push(["G", "go to a page number"]);
			if (!browse)
				L.push(["N  P", "next / previous page"]);
		}
		if (has_text) {
			L.push(["", "Search"]);
			L.push(["/", "find text (an empty answer ends the search)"]);
			L.push(["M  Shift-M", "next / previous match"]);
		}
		if (has_links) {
			L.push(["", "Contents and links"]);
			if (doc.has_outline())
				L.push(["C", "table of contents (Enter goes there)"]);
			L.push(["Tab  ]  [", "next / previous link"]);
			L.push(["Enter  click", "follow the link"]);
			L.push(["B  L", "go back / outline all the links"]);
		}
		if (browse) {
			L.push(["", "Library"]);
			L.push(["N  P", "next / previous file in the list"]);
		}
		L.push(["", "Display"]);
		L.push(["V  D", "invert colors / dithering on or off"]);
		if (doc.nframes > 1) {
			L.push(["A", "start / stop the animation"]);
			L.push(["<  >   ,  .", "animation slower / faster"]);
		}
		L.push(["H  ?", "this help"]);
		L.push(["Q  Esc", "close the viewer"]);
		/* on a short screen the headings go first, so that every key still fits without scrolling */
		if (L.length + 2 > console.screen_rows - 2) {
			var K2 = [];
			for (var li = 0; li < L.length; li++)
				if (L[li][0] !== "")
					K2.push(L[li]);
			L = K2;
		}

		var W = console.screen_columns, H = console.screen_rows;
		var bw = Math.min(W - 4, 64);
		var inner = bw - 4;
		var keyw = 18;
		var bh = Math.min(L.length + 2, H - 2);		/* the status line stays visible */
		var shown = bh - 2, top_row = 0;
		var x0 = Math.floor((W - bw) / 2) + 1, y0 = Math.max(1, Math.floor((H - 1 - bh) / 2) + 1);
		var ch = function (n) { return String.fromCharCode(n); };
		var bg = "\x01n\x01h\x01w\x014", bord = "\x01n\x01h\x01c\x014", yel = "\x01n\x01h\x01y\x014", hd = "\x01n\x01h\x01c\x014";
		function pad(t, n)
		{
			t = String(t);
			if (t.length > n)
				return t.substr(0, n);
			while (t.length < n)
				t += " ";
			return t;
		}
		function put(y, str)
		{
			console.gotoxy(x0, y);
			console.print(str + "\x01n");
		}
		function edge(left, title, right)
		{
			var fill = bw - 2 - title.length, l = Math.floor(fill / 2);
			return bord + ch(left) + new Array(l + 1).join(ch(0xc4)) + bg + title + bord + new Array(fill - l + 1).join(ch(0xc4)) + ch(right);
		}
		function draw_help()
		{
			put(y0, edge(0xda, " Help ", 0xbf));
			for (var i = 0; i < shown; i++) {
				var it = L[top_row + i], row;
				if (it[0] === "")
					row = hd + " " + pad(it[1], inner);
				else
					row = bg + "   " + yel + pad(it[0], keyw) + bg + pad(it[1], inner + 1 - 3 - keyw);
				put(y0 + 1 + i, bord + ch(0xb3) + row + bg + " " + bord + ch(0xb3));
			}
			put(y0 + 1 + shown, edge(0xc0, L.length > shown ? " Up/Down: more   other key: close " : " Press any key to close ", 0xd9));
		}
		for (;;) {
			draw_help();
			var hk = console.getkey(K_NONE);
			if (hk == "\x1b") {
				var hm = read_mouse();
				if (hm === null)
					continue;		/* some other escape sequence */
			}
			if (L.length > shown && (hk == KEY_DOWN || hk == KEY_UP || hk == KEY_PAGEDN || hk == KEY_PAGEUP)) {
				var step = (hk == KEY_PAGEDN || hk == KEY_PAGEUP) ? shown - 1 : 1;
				top_row = Math.max(0, Math.min(L.length - shown, top_row + (hk == KEY_DOWN || hk == KEY_PAGEDN ? step : -step)));
				continue;
			}
			break;
		}
		repaint();
	}

	/* The link under the mouse (x and y are the character cell of a click), or -1.  The picture starts at the top
	 * left of the screen, so a cell's pixels follow from the character cell size. */
	function link_at(x, y)
	{
		if (!has_links)
			return -1;
		var px = (x - 0.5) * geo.cellW, py = (y - 0.5) * geo.cellH, ll = cur_links();
		for (var i = 0; i < ll.length; i++) {
			var b = link_box(ll[i]);
			if (px >= b[0] && px <= b[2] && py >= b[1] && py <= b[3])
				return i;
		}
		return -1;
	}

	function link_visible(l)
	{
		var b = link_box(l);
		return b[2] > 0 && b[0] < view.W && b[3] > 0 && b[1] < view.H;
	}

	/* Scroll so that link l is on screen; returns true if the view moved */
	function reveal(l)
	{
		if (link_visible(l))
			return false;
		var b = link_box(l);
		view.sy += b[1] - view.H / 3;
		if (b[0] < 0 || b[2] > view.W)
			view.sx += b[0] - view.W / 4;
		return true;
	}


	/* ---- search (PDF, HTML pages and EPUB books): / asks for text, m goes to the next match, M to the previous ---- */
	var has_text = typeof doc.page_text == "function";
	var find = null;		/* while a search is active: { term, needle, hits: { page: [match, ...] }, page, idx } */

	/* The matches of the search on a page, in reading order: [{ rects: [[x0, y0, x1, y1], ...] }] in page coordinates
	 * (one rectangle for each line a match runs over).  Finding them reads the page's text, which is kept. */
	function page_hits(pg)
	{
		if (find.hits[pg])
			return find.hits[pg];
		var pt = doc.page_text(pg);
		var hay = pt.text.replace(/\n/g, " ").toLowerCase();
		var out = [];
		var at = hay.indexOf(find.needle);
		while (at >= 0 && out.length < 1000) {
			var rects = [];
			var cur = null;
			for (var i = at; i < at + find.needle.length; i++) {
				var b = pt.boxes[i];
				if (!b)
					continue;
				if (cur && Math.abs((b[1] + b[3]) / 2 - (cur[1] + cur[3]) / 2) < (cur[3] - cur[1]) * 0.5) {
					cur[0] = Math.min(cur[0], b[0]);
					cur[1] = Math.min(cur[1], b[1]);
					cur[2] = Math.max(cur[2], b[2]);
					cur[3] = Math.max(cur[3], b[3]);
				}
				else {
					cur = b.slice();
					rects.push(cur);
				}
			}
			if (rects.length)
				out.push({ rects: rects });
			at = hay.indexOf(find.needle, at + find.needle.length);
		}
		find.hits[pg] = out;
		return out;
	}

	/* Multiply the pixels of a box by a color (so black text stays black and white turns that color) */
	function tint_rect(x0, y0, x1, y1, rgb)
	{
		x0 = Math.max(0, Math.round(x0)); y0 = Math.max(0, Math.round(y0));
		x1 = Math.min(FW - 1, Math.round(x1)); y1 = Math.min(FH - 1, Math.round(y1));
		var cr = (rgb >> 16) & 255, cg = (rgb >> 8) & 255, cb = rgb & 255;
		for (var y = y0; y <= y1; y++) {
			for (var x = x0; x <= x1; x++) {
				var p = FB[y * FW + x];
				FB[y * FW + x] = ((((p >> 16) & 255) * cr >> 8) << 16) | ((((p >> 8) & 255) * cg >> 8) << 8) | ((p & 255) * cb >> 8);
			}
		}
	}

	/* Mark every match on the page; the one the search is at gets a stronger color and a frame */
	function search_overlay()
	{
		if (!find)
			return;
		var hits = page_hits(view.page);
		for (var i = 0; i < hits.length; i++) {
			var current = find.page == view.page && find.idx === i;
			for (var r = 0; r < hits[i].rects.length; r++) {
				var b = link_box({ rect: hits[i].rects[r] });
				tint_rect(b[0], b[1], b[2], b[3], current ? 0xff8c28 : 0xffe84a);
				if (current)
					frame_rect(b[0] - 1, b[1] - 1, b[2] + 1, b[3] + 1, 1, 0xe05000);
			}
		}
	}

	/* A line of text typed on the bottom line.  Enter accepts, Esc cancels (returns null). */
	function status_input(prompt, initial, maxlen)
	{
		var val = initial, key;
		var clicks_on = (console.status & CON_MOUSE_CLK_PASSTHRU) != 0;
		mouse_clicks(false);		/* mouse reports must not end up in the text */
		try {
			for (;;) {
				var head = " " + prompt + ": ", room = console.screen_columns - 2 - head.length;
				var shown = val.length > room ? val.substr(val.length - room) : val;
				status_line(head + shown);
				console.gotoxy(head.length + shown.length + 1, console.screen_rows);
				key = console.getkey(K_NONE);
				if (!bbs.online || !key)
					return null;
				if (key == "\r" || key == "\n")
					return val;
				if (key == "\x1b") {
					if (read_mouse() === false)
						return null;		/* the Esc key */
					continue;
				}
				if (key == "\b" || key == "\x7f")
					val = val.substr(0, val.length - 1);
				else if (key == "\x15")
					val = "";
				else if (key.length == 1 && key.charCodeAt(0) >= 0x20 && key.charCodeAt(0) != 0x7f && val.length < maxlen) {
					var u = utf8_encode(key), b0 = u.charCodeAt(0);		/* a typed character is CP437; the PDF's text is Unicode */
					val += b0 < 0x80 ? key : b0 < 0xe0 ? String.fromCharCode(((b0 & 0x1f) << 6) | (u.charCodeAt(1) & 0x3f)) : "?";
				}
			}
		}
		finally {
			mouse_clicks(clicks_on);
		}
	}

	/* Go to the next (dir 1) or previous (dir -1) match, reading the text of pages as needed (a key stops it).
	 * Returns true if the view has to be redrawn. */
	function find_step(dir)
	{
		var pg = view.page, idx, hits, steps = 0, wrapped = false, stopped = false;
		hits = page_hits(pg);
		if (find.idx !== null && find.page == pg)
			idx = find.idx;
		else if (dir > 0) {		/* a new search starts with the first match not wholly above the screen */
			idx = -1;
			while (idx + 1 < hits.length && link_box({ rect: hits[idx + 1].rects[hits[idx + 1].rects.length - 1] })[3] <= 0)
				idx++;
		}
		else {
			idx = hits.length;
			while (idx - 1 >= 0 && link_box({ rect: hits[idx - 1].rects[0] })[1] >= view.H)
				idx--;
		}
		for (;;) {
			if (dir > 0 ? idx + 1 < hits.length : idx - 1 >= 0) {
				idx += dir;
				break;
			}
			pg += dir;
			if (pg < 0 || pg >= np) {
				wrapped = true;
				pg = pg < 0 ? np - 1 : 0;
			}
			if (++steps > np || input_pending()) {
				stopped = steps <= np;
				hits = [];
				break;
			}
			if (np > 1)
				status_line(" Searching page " + (pg + 1) + "/" + np + " for \"" + find.term.substr(0, 30) + "\" (press a key to stop) ...");
			hits = page_hits(pg);
			idx = dir > 0 ? -1 : hits.length;
		}
		if (!hits.length) {
			linkmsg = stopped ? " Search stopped" : " Not found: " + find.term.substr(0, console.screen_columns - 20);
			return false;
		}
		var moved = view.page != pg;
		if (moved) {
			view.set_page(pg, false);
			linksel = -1;
		}
		find.page = pg;
		find.idx = idx;
		if (reveal({ rect: hits[idx].rects[0] }))
			moved = true;
		linkmsg = " \"" + find.term.substr(0, 30) + "\": match " + (idx + 1) + "/" + hits.length + " on page " + (pg + 1) +
			(wrapped ? " (search wrapped)" : "") + "   [m] next  [M] previous  [/] new search";
		return moved;
	}

	/* The / key: ask what to find (an empty answer ends the search) and go to the first match */
	function find_prompt()
	{
		if (!has_text) {
			linkmsg = " There is no text to search in this file";
			return false;
		}
		var v = status_input("Find", find ? find.term : "", 60);
		if (v === null)
			return false;
		v = v.replace(/^\s+|\s+$/g, "");
		if (!v.length) {
			find = null;
			repaint();
			return false;
		}
		find = { term: v, needle: v.toLowerCase().replace(/\s+/g, " "), hits: {}, page: -1, idx: null };
		return find_step(1);
	}

	function select_link(dir)
	{
		var links = cur_links();
		if (!links.length) {
			linkmsg = " No links on this page";
			return false;
		}
		if (linksel < 0) {
			linksel = dir > 0 ? 0 : links.length - 1;
			for (var i = 0; i < links.length; i++) {
				if (link_visible(links[i])) {
					linksel = i;
					break;
				}
			}
		}
		else
			linksel = (linksel + dir + links.length) % links.length;
		return true;
	}

	function follow_link()
	{
		var links = cur_links();
		var l = links[linksel];
		if (!l)
			return;
		if (l.uri) {
			linkresult = "link:" + l.uri;
			return;
		}
		if (l.page >= 0) {
			history.push({ page: view.page, scale: view.scale, sx: view.sx, sy: view.sy });
			view.set_page(l.page, false);
			var M = page_matrix(view.pages[view.page], view.scale, 0, 0);
			if (l.y !== undefined) {
				var p = mat_apply(M, l.x !== undefined ? l.x : 0, l.y);
				view.sy = p[1] - 16;
			}
			linksel = -1;
		}
	}
	var np = view.pages.length;
	while (input_pending())	/* discard any stray keystrokes (e.g. the Enter that launched us) */
		console.inkey(K_NONE, 0);

	function abort_cb()
	{
		return input_pending() || !bbs.online;
	}

	function draw()
	{
		if (anim_ready())		/* the screen is redrawn for some reason other than a new view */
			return anim_resync();
		if (!silent)		/* an animation redraws every frame, so its status line is left alone */
			status_line(np > 1 ? " Rendering page " + (view.page + 1) + "/" + np + " ..." : " Rendering ...");
		/* quick pass with reduced JPEG detail first, then refine if needed */
		var done = view.render(abort_cb, 1);
		if (!done)
			return false;
		var degraded = LAST_DEGRADED;
		save_base();
		emit_frame();
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
		if (linkmsg) {
			status_line(linkmsg);
			return;
		}
		if (linksel >= 0 && has_links) {
			var ll = cur_links();
			var cl = ll[linksel];
			if (cl) {
				status_line(" Link " + (linksel + 1) + "/" + ll.length + ": " +
					(cl.uri ? (doc.link_label && doc.link_label(cl.uri)) || "open " + String(cl.uri).replace(/[\x00-\x1f\x7f]/g, "").substr(0, console.screen_columns - 55) : "go to page " + (cl.page + 1)) +
					"   [Enter] follow  [Tab/[] next/prev  [b] back");
				return;
			}
		}
		if (showhelp) {
			status_line(" " + (browse ? HELP_TEXT.replace("G:goto", "G:goto N/P:next/prev file") : HELP_TEXT));
			return;
		}
		var menu = [];
		if (has_links && doc.has_outline())
			menu.push("(C)ontents");
		/* a document with pages has < > for them (N/P for the files of a library are in the help); a single picture keeps N/P */
		if (np > 1)
			menu.push("(</>) Page");
		else if (browse)
			menu.push("(N)ext/(P)rev File");
		menu.push("(Q)uit");
		menu.push("(H)elp");
		var opt = [];
		if (has_text)
			opt.push("(/) Find");
		if (has_links && cur_links().length)
			opt.push("(Tab) Links");
		if (history.length || doc.web)
			opt.push("(B)ack");
		if (doc.nframes > 1)
			opt.unshift(animating ? "(A) Stop" : "(A)nimate");
		status_menu(" " + (np > 1 ? "Page " + (view.page + 1) + "/" + np + "  " : doc.nframes > 1 ? "Frame " + (doc.frame + 1) + "/" + doc.nframes + "  " + (aspeed != ASPEED_DEF ? "Speed " + ASPEEDS[aspeed] + "x  " : "") : "") + "Zoom " +
			Math.round(view.scale / actual * 100) + "%  ", menu, opt);
	}

	while (bbs.online && !js.terminated) {
		if (need) {
			if (draw()) {
				need = false;
				if (!noticed && doc.notice) {
					noticed = true;
					linkmsg = " " + doc.notice;
				}
				if (!silent)
					show_status();
				silent = false;
			}
			else if (!input_pending())
				need = false;
		}
		/* an animation that hasn't been prepared for the current view yet */
		if (animating && !need && bbs.online && !input_pending() && !(acache && acache.key == anim_key())) {
			acur = doc.frame;
			acache = build_animation();
			if (acache && !acache.none) {
				acur = 0;
				doc.frame = 0;
				anim_header();
				send_sixel(acache.first);
				adue = now_ms() + frame_ms(0);
			}
			show_status();
			continue;
		}
		var playing = animating && !need && anim_ready();
		var key;
		if (need)
			key = console.inkey(K_NONE, 0);
		else if (playing)
			key = console.inkey(K_NONE, Math.max(0, Math.floor(adue - now_ms())));
		else if (animating)
			key = console.inkey(K_NONE, Math.floor(frame_ms(doc.frame)));
		else
			key = console.getkey(K_NONE);
		if (key)
			silent = false;
		if (!key && playing && bbs.online) {
			anim_step();
			continue;
		}
		if (!key && animating && !need && bbs.online) {
			doc.next_frame();
			need = true;
			silent = true;
			continue;
		}
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
			/* where the view is before the key: a key that only moves or zooms it needs no redraw if it didn't (at the
			 * last page there is nothing to render when Down is pressed again) */
			var before = view.page + "|" + view.scale + "|" + view.sx + "|" + view.sy;
			var moves = [KEY_UP, KEY_DOWN, "k", "j", KEY_LEFT, KEY_RIGHT, "l", KEY_PAGEDN, " ", "\r", KEY_PAGEUP, "\b", KEY_HOME, KEY_END,
				"+", "=", "i", "I", "-", "_", "o", "O", "<", ",", ">", ".", "w", "W", "f", "F", "1", "n", "N", "p", "P"].indexOf(k) >= 0;
			linkmsg = "";
			if (k == "\r" && linksel >= 0)
				k = "\x00follow";
			/* a mouse report that got through as an ESC sequence must not count as the Esc key */
			if (k == "\x1b") {
				var vmk = read_mouse();
				if (vmk !== false) {
					var vbk = vmk ? bar_key(vmk) : null;
					k = vbk ? vbk : "\x00ignore";
					if (!vbk && vmk && vmk.button == 0) {		/* a left click on a link follows it */
						var hit = link_at(vmk.x, vmk.y);
						if (hit >= 0) {
							linksel = hit;
							k = "\x00follow";
						}
					}
				}
			}
			switch (k) {
				case "\x00ignore":
					redraw = false;
					break;
				case "\x00follow":
					follow_link();
					if (linkresult) {
						console.write("\x1b[?80l");
						console.clear();
						return linkresult;
					}
					break;
				case "\t": case "]": case "[":
					if (!has_links) {
						redraw = false;
						break;
					}
					if (select_link(k == "[" ? -1 : 1)) {
						var moved = reveal(cur_links()[linksel]);
						if (!moved) {
							redraw = false;
							repaint();
							show_status();
						}
					}
					else {
						redraw = false;
						show_status();
					}
					break;
				case "/":
					redraw = find_prompt();
					if (!redraw) {
						repaint();
						show_status();
					}
					break;
				case "m": case "M":
					if (!find) {
						linkmsg = has_text ? " Press / to find text first" : " There is no text to search in this file";
						redraw = false;
						show_status();
						break;
					}
					redraw = find_step(k == "M" ? -1 : 1);
					if (!redraw) {
						repaint();
						show_status();
					}
					break;
				case "c": case "C":
					if (has_links && doc.has_outline()) {
						contents();
						linkpage = -1;
						need = true;
					}
					else {
						linkmsg = " This document has no table of contents";
						redraw = false;
						show_status();
					}
					break;
				case "L":
					showlinks = !showlinks;
					redraw = false;
					repaint();
					break;
				case "b": case "B":
					if (!history.length && doc.web) {
						console.write("\x1b[?80l");
						console.clear();
						return "back";
					}
					if (history.length) {
						var h = history.pop();
						view.page = h.page;
						view.scale = h.scale;
						view.sx = h.sx;
						view.sy = h.sy;
						linksel = -1;
					}
					else
						redraw = false;
					break;
				/* Up/Down (and so the mouse wheel, which Synchronet turns into them) move half a screen; at the
				 * bottom or top of a page they carry on to the next or previous page */
				case KEY_UP: case KEY_DOWN: case "k": case "j":
					var down = (k == KEY_DOWN || k == "j");
					var amount = (k == KEY_UP || k == KEY_DOWN) ? Math.max(stepy, Math.round(view.H / 2)) : stepy;
					if (down && view.sy >= d.h - view.H + VIEW_MARGIN - 1 && view.page < np - 1)
						view.set_page(view.page + 1, false);
					else if (!down && view.sy <= -VIEW_MARGIN + 1 && view.page > 0)
						view.set_page(view.page - 1, true);
					else
						view.sy += down ? amount : -amount;
					break;
				case KEY_LEFT: view.sx -= stepx; break;
				case KEY_RIGHT: view.sx += stepx; break;
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
				case "a": case "A":
					if (doc.nframes > 1) {
						animating = !animating;
						adue = now_ms();
						redraw = false;
						show_status();
					}
					else
						redraw = false;
					break;
				/* < and , / > and . : the previous / next page of the document (an animated GIF has no pages, so they
				 * change its speed instead) */
				case "<": case ",": case ">": case ".":
					if (doc.nframes > 1) {
						aspeed = Math.max(0, Math.min(ASPEEDS.length - 1, aspeed + (k == "<" || k == "," ? -1 : 1)));
						adue = now_ms();
						show_status();
						redraw = false;
					}
					else if (np > 1 && (k == "<" || k == ",") && view.page > 0)
						view.set_page(view.page - 1, false);
					else if (np > 1 && (k == ">" || k == ".") && view.page < np - 1)
						view.set_page(view.page + 1, false);
					else
						redraw = false;
					break;
				case "v": case "V": SIX_INVERT = !SIX_INVERT; break;
				case "d": case "D": SIX_DITHER = !SIX_DITHER; break;
				case "w": case "W": view.fit_width(); break;
				case "f": case "F": view.fit_page(); break;
				case "1": view.zoom(actual / view.scale); break;
				case "n": case "N":
					if (browse) {
						console.write("\x1b[?80l");
						console.clear();
						return "next";
					}
					if (view.page < np - 1)
						view.set_page(view.page + 1, false);
					break;
				case "p": case "P":
					if (browse) {
						console.write("\x1b[?80l");
						console.clear();
						return "prev";
					}
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
				case "?": case "h": case "H": help_popup(); show_status(); redraw = false; break;
				case "q": case "Q": case "\x1b": case "\x03":
					if (k == "\x03")
						RIGHT_CLICKED = true;
					console.write("\x1b[?80l");
					console.clear();
					return;
				default:
					redraw = false;
			}
			if (redraw) {
				if (view.page != linkpage) {
					linkpage = view.page;
					linksel = -1;
				}
				view.clamp();
				if (!moves || doc.nframes > 1 || before != view.page + "|" + view.scale + "|" + view.sx + "|" + view.sy)
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

/* ======================================================================
 * More image formats: PCX, TGA, ICO, TIFF (baseline), IFF/ILBM (Amiga), XBin and BIN (text-mode art,
 * drawn as pictures).  Each decoder returns { w, h, n: 3, data (RGB), alpha, mask: false }.
 * ====================================================================== */

function img_rgb(w, h)
{
	if (!(w > 0) || !(h > 0) || w * h > 64 * 1024 * 1024)
		throw "Invalid image size";
	return { w: w, h: h, n: 3, data: new Uint8Array(w * h * 3), alpha: null, mask: false };
}

/* ---- PCX ---- */
function pcx_decode(b)
{
	if (b.length < 129 || b[0] != 10 || b[2] != 1)
		throw "Invalid PCX image";
	var bpp = b[3], w = (b[8] | (b[9] << 8)) - (b[4] | (b[5] << 8)) + 1, h = (b[10] | (b[11] << 8)) - (b[6] | (b[7] << 8)) + 1;
	var np = b[65], bpl = b[66] | (b[67] << 8);
	var img = img_rgb(w, h), out = img.data;
	var rowlen = np * bpl;
	var row = new Uint8Array(rowlen);
	var p = 128;
	var pal = null;
	if (bpp == 8 && np == 1 && b.length > 769 && b[b.length - 769] == 12)
		pal = bytes_sub(b, b.length - 768);
	var x, y, k;
	for (y = 0; y < h; y++) {
		var n = 0;
		while (n < rowlen && p < b.length) {
			var c = b[p++];
			if (c >= 0xc0) {
				var cnt = c & 0x3f, v = p < b.length ? b[p++] : 0;
				while (cnt-- > 0 && n < rowlen)
					row[n++] = v;
			}
			else
				row[n++] = c;
		}
		for (x = 0; x < w; x++) {
			var o = (y * w + x) * 3, r, g, bl;
			if (bpp == 8 && np == 1) {
				k = row[x];
				if (pal) { r = pal[k * 3]; g = pal[k * 3 + 1]; bl = pal[k * 3 + 2]; }
				else { r = g = bl = k; }
			}
			else if (bpp == 8 && np >= 3) {
				r = row[x]; g = row[bpl + x]; bl = row[2 * bpl + x];
			}
			else {		/* 1 bit per plane, 1-4 planes: an index into the header's 16-color palette */
				k = 0;
				for (var pl = 0; pl < np && bpp == 1; pl++)
					k |= ((row[pl * bpl + (x >> 3)] >> (7 - (x & 7))) & 1) << pl;
				if (bpp == 4)
					k = (x & 1) ? row[x >> 1] & 15 : row[x >> 1] >> 4;
				if (np == 1 && bpp == 1) { r = g = bl = k ? 255 : 0; }
				else { r = b[16 + k * 3]; g = b[17 + k * 3]; bl = b[18 + k * 3]; }
			}
			out[o] = r; out[o + 1] = g; out[o + 2] = bl;
		}
	}
	return img;
}

/* ---- TGA ---- */
function tga_decode(b)
{
	if (b.length < 18)
		throw "Invalid TGA image";
	var idlen = b[0], cmt = b[1], type = b[2];
	var cmfirst = b[3] | (b[4] << 8), cmlen = b[5] | (b[6] << 8), cmbits = b[7];
	var w = b[12] | (b[13] << 8), h = b[14] | (b[15] << 8), bpp = b[16], desc = b[17];
	var rle = type >= 9, base = rle ? type - 8 : type;
	if ((base != 1 && base != 2 && base != 3) || (bpp != 8 && bpp != 15 && bpp != 16 && bpp != 24 && bpp != 32))
		throw "Unsupported TGA image";
	var img = img_rgb(w, h), out = img.data;
	var p = 18 + idlen, cm = null, i;
	if (cmt == 1) {
		cm = [];
		var eb = (cmbits + 7) >> 3;
		for (i = 0; i < cmlen; i++) {
			var q = p + i * eb;
			cm[cmfirst + i] = eb == 2 ? [(b[q + 1] >> 2 & 31) * 255 / 31, (((b[q + 1] & 3) << 3) | (b[q] >> 5)) * 255 / 31, (b[q] & 31) * 255 / 31]
				: [b[q + 2], b[q + 1], b[q]];
		}
		p += cmlen * eb;
	}
	var bytespp = (bpp + 7) >> 3;
	var npx = w * h, px = 0;
	var alpha = bpp == 32 && (desc & 15) ? new Uint8Array(npx) : null;
	function put(q)
	{
		var r, g, bl, a = 255;
		if (base == 1) {
			var c = cm && cm[b[q]] || [0, 0, 0];
			r = c[0]; g = c[1]; bl = c[2];
		}
		else if (base == 3) {
			r = g = bl = b[q];
		}
		else if (bytespp == 2) {
			var v = b[q] | (b[q + 1] << 8);
			r = ((v >> 10) & 31) * 255 / 31; g = ((v >> 5) & 31) * 255 / 31; bl = (v & 31) * 255 / 31;
		}
		else {
			bl = b[q]; g = b[q + 1]; r = b[q + 2];
			if (bytespp == 4)
				a = b[q + 3];
		}
		var y = Math.floor(px / w), x = px % w;
		if (!(desc & 0x20))
			y = h - 1 - y;
		if (desc & 0x10)
			x = w - 1 - x;
		var o = y * w + x;
		out[o * 3] = r; out[o * 3 + 1] = g; out[o * 3 + 2] = bl;
		if (alpha)
			alpha[o] = a;
		px++;
	}
	while (px < npx && p < b.length) {
		if (rle) {
			var hd = b[p++], cnt = (hd & 127) + 1;
			if (hd & 128) {
				for (i = 0; i < cnt && px < npx; i++)
					put(p);
				p += bytespp;
			}
			else {
				for (i = 0; i < cnt && px < npx; i++) {
					put(p);
					p += bytespp;
				}
			}
		}
		else {
			put(p);
			p += bytespp;
		}
	}
	img.alpha = alpha;
	return img;
}

/* ---- ICO: the largest image in the file (PNG or DIB) ---- */
function ico_decode(b)
{
	var count = b[4] | (b[5] << 8), best = -1, bs = -1, i;
	if (b.length < 22 || b[2] != 1 || !count)
		throw "Invalid icon file";
	for (i = 0; i < count && i < 64; i++) {
		var e = 6 + i * 16;
		var w = b[e] || 256, h = b[e + 1] || 256, bpp = b[e + 6] | (b[e + 7] << 8);
		var score = w * h * 100 + bpp;
		if (score > bs) {
			bs = score;
			best = e;
		}
	}
	var size = (b[best + 8] | (b[best + 9] << 8) | (b[best + 10] << 16) | (b[best + 11] << 24)) >>> 0;
	var off = (b[best + 12] | (b[best + 13] << 8) | (b[best + 14] << 16) | (b[best + 15] << 24)) >>> 0;
	var d = bytes_sub(b, off, off + size);
	if (d[0] == 0x89 && d[1] == 0x50)
		return png_decode(d);
	/* a BMP without its file header, with the height doubled to include the transparency mask */
	var hs = d[0] | (d[1] << 8) | (d[2] << 16) | (d[3] << 24);
	var bpp2 = d[14] | (d[15] << 8), used = d[32] | (d[33] << 8) | (d[34] << 16);
	var npal = bpp2 <= 8 ? (used || (1 << bpp2)) : 0;
	var f = new Uint8Array(14 + d.length);
	f[0] = 0x42; f[1] = 0x4d;
	var po = 14 + hs + npal * 4;
	f[10] = po & 255; f[11] = (po >> 8) & 255;
	f.set(d, 14);
	var hh = (d[8] | (d[9] << 8) | (d[10] << 16) | (d[11] << 24)) >> 1;
	f[14 + 8] = hh & 255; f[14 + 9] = (hh >> 8) & 255; f[14 + 10] = (hh >> 16) & 255; f[14 + 11] = 0;
	var img = bmp_decode(f);
	if (bpp2 < 32) {		/* the AND mask: 1 = transparent */
		var w2 = img.w, h2 = img.h;
		var rowb = ((w2 * bpp2 + 31) >> 5) << 2, mrow = ((w2 + 31) >> 5) << 2;
		var mo = hs + npal * 4 + rowb * h2;
		if (mo + mrow * h2 <= d.length) {
			var al = new Uint8Array(w2 * h2), any = false;
			for (var y = 0; y < h2; y++) {
				for (var x = 0; x < w2; x++) {
					var t = (d[mo + (h2 - 1 - y) * mrow + (x >> 3)] >> (7 - (x & 7))) & 1;
					al[y * w2 + x] = t ? 0 : 255;
					if (t)
						any = true;
				}
			}
			if (any)
				img.alpha = al;
		}
	}
	return img;
}

/* ---- TIFF (baseline: uncompressed, PackBits or LZW; 1, 4, 8 or 16 bits; gray, RGB, RGBA or palette) ---- */
function tiff_lzw(src, outlen)
{
	var out = new Uint8Array(outlen), op = 0;
	var prefix = new Int32Array(4096), suffix = new Uint8Array(4096), len = new Int32Array(4096);
	var next = 258, bits = 9, prev = -1, bb = 0, bc = 0, sp = 0, i;
	var tmp = new Uint8Array(4097);
	while (op < outlen) {
		while (bc < bits && sp < src.length) {
			bb = (bb << 8) | src[sp++];
			bc += 8;
		}
		if (bc < bits)
			break;
		var code = (bb >> (bc - bits)) & ((1 << bits) - 1);
		bc -= bits;
		bb &= (1 << bc) - 1;
		if (code == 257)
			break;
		if (code == 256) {
			next = 258;
			bits = 9;
			prev = -1;
			continue;
		}
		var n = 0, c = code;
		if (prev < 0) {
			out[op++] = code;
			prev = code;
			continue;
		}
		if (code >= next) {		/* KwKwK */
			c = prev;
			var fc = prev;
			while (fc >= 258)
				fc = prefix[fc];
			tmp[n++] = fc;
		}
		while (c >= 258 && n < 4096) {
			tmp[n++] = suffix[c];
			c = prefix[c];
		}
		tmp[n++] = c;
		var first = tmp[n - 1];
		for (i = n - 1; i >= 0 && op < outlen; i--)
			out[op++] = tmp[i];
		if (next < 4096) {
			prefix[next] = prev;
			suffix[next] = first;
			next++;
		}
		if (next + 1 >= (1 << bits) && bits < 12)
			bits++;
		prev = code;
	}
	return out;
}

function tiff_decode(b)
{
	var le = b[0] == 0x49;
	function u16(o) { return le ? b[o] | (b[o + 1] << 8) : (b[o] << 8) | b[o + 1]; }
	function u32(o) { return (le ? b[o] | (b[o + 1] << 8) | (b[o + 2] << 16) | (b[o + 3] << 24) : (b[o] << 24) | (b[o + 1] << 16) | (b[o + 2] << 8) | b[o + 3]) >>> 0; }
	var ifd = u32(4), n = u16(ifd), tags = {}, i, j;
	if (!n || n > 500)
		throw "Invalid TIFF image";
	for (i = 0; i < n; i++) {
		var e = ifd + 2 + i * 12, tag = u16(e), type = u16(e + 2), cnt = u32(e + 4);
		var sz = (type == 3 ? 2 : type == 4 ? 4 : 1) * cnt;
		var vo = sz <= 4 ? e + 8 : u32(e + 8);
		var vals = [];
		for (j = 0; j < cnt && j < 70000; j++)
			vals.push(type == 3 ? u16(vo + j * 2) : type == 4 ? u32(vo + j * 4) : b[vo + j]);
		tags[tag] = vals;
	}
	function t(tag, d) { return tags[tag] !== undefined ? tags[tag][0] : d; }
	var w = t(256), h = t(257), bps = t(258, 1), comp = t(259, 1), photo = t(262, 1), spp = t(277, 1);
	var rps = t(278, h), planar = t(284, 1), pred = t(317, 1);
	if (!w || !h || planar != 1)
		throw "Unsupported TIFF image";
	if (comp != 1 && comp != 5 && comp != 8 && comp != 32773 && comp != 32946)
		throw "Unsupported TIFF compression";
	if (bps != 1 && bps != 4 && bps != 8 && bps != 16)
		throw "Unsupported TIFF bit depth";
	var img = img_rgb(w, h), out = img.data;
	var rowbytes = Math.ceil(w * spp * bps / 8);
	var offs = tags[273], cnts = tags[279];
	if (!offs || !cnts)
		throw "Unsupported TIFF layout";
	var pix = new Uint8Array(rowbytes * h), pp = 0;
	for (i = 0; i < offs.length && pp < pix.length; i++) {
		var rows = Math.min(rps, h - i * rps);
		var want = rows * rowbytes;
		var src = bytes_sub(b, offs[i], offs[i] + cnts[i]), strip;
		if (comp == 1)
			strip = src;
		else if (comp == 5)
			strip = tiff_lzw(src, want);
		else if (comp == 8 || comp == 32946) {		/* zlib/deflate */
			var inf = inflate_zlib(src);
			strip = new Uint8Array(want);
			for (j = 0; j < want && j < inf.length; j++)
				strip[j] = inf[j];
		}
		else {
			strip = new Uint8Array(want);
			var sp = 0, op = 0;
			while (op < want && sp < src.length) {
				var c = src[sp++];
				if (c < 128) {
					for (j = 0; j <= c && op < want; j++)
						strip[op++] = src[sp++];
				}
				else if (c > 128) {
					var v = src[sp++];
					for (j = 0; j < 257 - c && op < want; j++)
						strip[op++] = v;
				}
			}
		}
		if (pred == 2 && bps == 16) {
			for (var r16 = 0; r16 < rows; r16++) {
				for (var x16 = spp; x16 < w * spp; x16++) {
					var o16 = r16 * rowbytes + x16 * 2, q16 = o16 - spp * 2;
					var a16 = le ? strip[o16] | (strip[o16 + 1] << 8) : (strip[o16] << 8) | strip[o16 + 1];
					var b16 = le ? strip[q16] | (strip[q16 + 1] << 8) : (strip[q16] << 8) | strip[q16 + 1];
					var v16 = (a16 + b16) & 0xffff;
					if (le) { strip[o16] = v16 & 255; strip[o16 + 1] = v16 >> 8; }
					else { strip[o16] = v16 >> 8; strip[o16 + 1] = v16 & 255; }
				}
			}
		}
		if (pred == 2 && bps == 8) {
			for (var r = 0; r < rows; r++)
				for (var xx = spp; xx < w * spp; xx++)
					strip[r * rowbytes + xx] = (strip[r * rowbytes + xx] + strip[r * rowbytes + xx - spp]) & 255;
		}
		for (j = 0; j < want && pp + j < pix.length; j++)
			pix[pp + j] = strip[j];
		pp += want;
	}
	var cmap = tags[320], alpha = null;
	if (spp == 4 || (spp == 2 && photo < 2))
		alpha = new Uint8Array(w * h);
	var maxv = (1 << bps) - 1;
	for (var y = 0; y < h; y++) {
		for (var x = 0; x < w; x++) {
			var o = (y * w + x) * 3, s = [];
			for (var k = 0; k < spp; k++) {
				var idx = x * spp + k, val;
				if (bps == 8) val = pix[y * rowbytes + idx];
				else if (bps == 16) val = le ? pix[y * rowbytes + idx * 2 + 1] : pix[y * rowbytes + idx * 2];
				else if (bps == 4) val = ((pix[y * rowbytes + (idx >> 1)] >> ((idx & 1) ? 0 : 4)) & 15);
				else val = (pix[y * rowbytes + (idx >> 3)] >> (7 - (idx & 7))) & 1;
				s.push(val);
			}
			var r2, g2, b2;
			if (photo == 3 && cmap) {
				var ncol = 1 << bps;
				r2 = cmap[s[0]] >> 8; g2 = cmap[ncol + s[0]] >> 8; b2 = cmap[2 * ncol + s[0]] >> 8;
			}
			else if (photo == 2 || photo == 5) {
				r2 = s[0]; g2 = s[1]; b2 = s[2];
				if (bps < 8) { r2 = r2 * 255 / maxv; g2 = g2 * 255 / maxv; b2 = b2 * 255 / maxv; }
			}
			else {
				var gv = bps < 8 ? s[0] * 255 / maxv : s[0];
				if (photo == 0)
					gv = 255 - gv;
				r2 = g2 = b2 = gv;
			}
			out[o] = r2; out[o + 1] = g2; out[o + 2] = b2;
			if (alpha)
				alpha[y * w + x] = s[spp - 1];
		}
	}
	img.alpha = alpha;
	return img;
}

/* ---- IFF ILBM / PBM (Amiga), including EHB, HAM and 24-bit ---- */
function ilbm_decode(b)
{
	function u32(o) { return ((b[o] << 24) | (b[o + 1] << 16) | (b[o + 2] << 8) | b[o + 3]) >>> 0; }
	function u16(o) { return (b[o] << 8) | b[o + 1]; }
	var form = String.fromCharCode(b[8], b[9], b[10], b[11]);
	var p = 12, bm = null, cmap = null, camg = 0, body = null, id;
	while (p + 8 <= b.length) {
		id = String.fromCharCode(b[p], b[p + 1], b[p + 2], b[p + 3]);
		var len = u32(p + 4), d = p + 8;
		if (id == "BMHD")
			bm = { w: u16(d), h: u16(d + 2), np: b[d + 8], mask: b[d + 9], comp: b[d + 10], trans: u16(d + 12) };
		else if (id == "CMAP")
			cmap = bytes_sub(b, d, d + len);
		else if (id == "CAMG")
			camg = u32(d);
		else if (id == "BODY")
			body = bytes_sub(b, d, d + len);
		p = d + len + (len & 1);
	}
	if (!bm || !body)
		throw "Invalid IFF image";
	var w = bm.w, h = bm.h, np = bm.np;
	var img = img_rgb(w, h), out = img.data;
	var chunky = form == "PBM ";
	var rowb = chunky ? w + (w & 1) : ((w + 15) >> 4) * 2;
	var nplanes = chunky ? 1 : np + (bm.mask == 1 ? 1 : 0);
	var rowlen = rowb * nplanes;
	var row = new Uint8Array(rowlen), bp = 0, y, x, i;
	var ham = !!(camg & 0x800) && np >= 5 && np <= 8 && !chunky;
	var ehb = !!(camg & 0x80) && np == 6 && !ham;
	var pal = [];
	var ncol = cmap ? Math.floor(cmap.length / 3) : 0;
	for (i = 0; i < ncol; i++)
		pal.push([cmap[i * 3], cmap[i * 3 + 1], cmap[i * 3 + 2]]);
	if (!ncol && np <= 8) {		/* no palette: gray ramp */
		for (i = 0; i < (1 << np); i++)
			pal.push([i * 255 / ((1 << np) - 1), i * 255 / ((1 << np) - 1), i * 255 / ((1 << np) - 1)]);
	}
	for (y = 0; y < h; y++) {
		if (bm.comp == 1) {
			var n = 0;
			while (n < rowlen && bp < body.length) {
				var c = body[bp++];
				if (c < 128) {
					for (i = 0; i <= c && n < rowlen; i++)
						row[n++] = body[bp++];
				}
				else if (c > 128) {
					var v = body[bp++];
					for (i = 0; i < 257 - c && n < rowlen; i++)
						row[n++] = v;
				}
			}
		}
		else {
			for (i = 0; i < rowlen; i++)
				row[i] = body[bp++];
		}
		var hr = 0, hg = 0, hb = 0;
		for (x = 0; x < w; x++) {
			var r, g, bl, idx = 0;
			if (chunky)
				idx = row[x];
			else if (np >= 24) {
				r = g = bl = 0;
				for (i = 0; i < 8; i++) {
					r |= ((row[i * rowb + (x >> 3)] >> (7 - (x & 7))) & 1) << i;
					g |= ((row[(8 + i) * rowb + (x >> 3)] >> (7 - (x & 7))) & 1) << i;
					bl |= ((row[(16 + i) * rowb + (x >> 3)] >> (7 - (x & 7))) & 1) << i;
				}
			}
			else {
				for (i = 0; i < np; i++)
					idx |= ((row[i * rowb + (x >> 3)] >> (7 - (x & 7))) & 1) << i;
			}
			if (np < 24 || chunky) {
				if (ham) {
					var bits = np - 2, val = idx & ((1 << bits) - 1), ctl = idx >> bits;
					var sc = bits == 4 ? 17 : 255 / ((1 << bits) - 1);
					if (ctl == 0) {
						var pc = pal[val] || [0, 0, 0];
						hr = pc[0]; hg = pc[1]; hb = pc[2];
					}
					else if (ctl == 1) hb = val * sc;
					else if (ctl == 2) hr = val * sc;
					else hg = val * sc;
					r = hr; g = hg; bl = hb;
				}
				else if (ehb && idx >= 32) {
					var hc = pal[idx - 32] || [0, 0, 0];
					r = hc[0] >> 1; g = hc[1] >> 1; bl = hc[2] >> 1;
				}
				else {
					var pc2 = pal[idx] || [0, 0, 0];
					r = pc2[0]; g = pc2[1]; bl = pc2[2];
				}
			}
			var o = (y * w + x) * 3;
			out[o] = r; out[o + 1] = g; out[o + 2] = bl;
		}
	}
	return img;
}

/* ---- XBin and BIN: text-mode art drawn as a picture, using the file's own font if it has one ---- */
var XB_PALETTE = [[0, 0, 0], [0, 0, 170], [0, 170, 0], [0, 170, 170], [170, 0, 0], [170, 0, 170], [170, 85, 0], [170, 170, 170],
	[85, 85, 85], [85, 85, 255], [85, 255, 85], [85, 255, 255], [255, 85, 85], [255, 85, 255], [255, 255, 85], [255, 255, 255]];

function textmode_picture(cols, rows, bin, font, charheight, palette, nonblink)
{
	var w = cols * 8, h = rows * charheight;
	var img = img_rgb(w, h), out = img.data;
	var pal = palette || XB_PALETTE;
	for (var cy = 0; cy < rows; cy++) {
		for (var cx = 0; cx < cols; cx++) {
			var q = (cy * cols + cx) * 2;
			var ch = bin.charCodeAt(q), at = bin.charCodeAt(q + 1);
			var fg = pal[at & 15], bg = pal[nonblink ? (at >> 4) & 15 : (at >> 4) & 7];
			for (var gy = 0; gy < charheight; gy++) {
				var bits = font.charCodeAt(ch * charheight + gy) || 0;
				var o = ((cy * charheight + gy) * w + cx * 8) * 3;
				for (var gx = 0; gx < 8; gx++) {
					var c = (bits >> (7 - gx)) & 1 ? fg : bg;
					out[o++] = c[0]; out[o++] = c[1]; out[o++] = c[2];
				}
			}
		}
	}
	img.textmode = true;		/* drawn at its own size, not shrunk to fit (see ImageDoc) */
	return img;
}

function default_font16()
{
	var f = new File(system.ctrl_dir + "fonts/ibmstd.f16");
	if (!f.open("rb"))
		throw "The default text font (ctrl/fonts/ibmstd.f16) is missing";
	var s = f.read(4096);
	f.close();
	return s;
}

function xbin_picture(filename)
{
	var xb = load({}, "xbin_lib.js");
	var f = new File(filename);
	if (!f.open("rb"))
		throw "Unable to open " + filename;
	var image = xb.read(f);
	f.close();
	if (!image || !image.bin)
		throw "Invalid XBin file";
	if (image.width * image.height > 4000000)
		throw "XBin image is too large";
	var font = image.font && image.font.length && image.font[0].length >= 256 * image.charheight ? image.font[0] : null;
	var ch = font ? image.charheight : 16;
	var pal = null;
	if (image.palette) {
		pal = [];
		for (var i = 0; i < 16; i++) {
			var c = image.palette[i];
			pal.push([(c[0] << 2) | (c[0] >> 4), (c[1] << 2) | (c[1] >> 4), (c[2] << 2) | (c[2] >> 4)]);
		}
	}
	return textmode_picture(image.width, image.height, image.bin, font || default_font16(), ch, pal, true);
}

function bin_picture(filename)
{
	var f = new File(filename);
	if (!f.open("rb"))
		throw "Unable to open " + filename;
	var s = f.read();
	f.close();
	var cols = 160;
	try {
		var sauce = load({}, "sauce_lib.js").read(filename);
		if (sauce && sauce.cols > 0)
			cols = sauce.cols;
	}
	catch (e) {}
	s = s.split("\x1aSAUCE")[0];
	var rows = Math.floor(s.length / 2 / cols);
	if (rows < 1)
		throw "Invalid BIN file";
	return textmode_picture(cols, rows, s, default_font16(), 16, null, true);
}

/* ======================================================================
 * SVG support.  A small XML parser, a CSS subset, and a renderer that draws SVG shapes with
 * the same scan converter that draws PDF pages (fill_polys, stroke_subpath).
 *
 * Supported: svg, g, a, switch, use (and symbol), path (all commands, including arcs), rect
 * (rounded too), circle, ellipse, line, polyline, polygon; viewBox with preserveAspectRatio
 * (meet/none); transforms; fill, stroke, opacity and the usual stroke properties; named
 * colors, #rgb, #rrggbb, rgb(), hsl(), currentColor; style attributes, and <style> rules with
 * simple selectors (tag, .class, #id).  Gradients are drawn as the average color of their stops.
 *
 * Not drawn: text, embedded images, clip paths, masks, filters, patterns, markers.  doc.notice
 * names whichever of these the file uses so the viewer can say so.
 * ====================================================================== */

/* ---------- XML ---------- */

function svg_unescape(s)
{
	if (s.indexOf("&") < 0)
		return s;
	return s.replace(/&(#x[0-9a-fA-F]+|#[0-9]+|lt|gt|amp|quot|apos);/g, function (m, e) {
		switch (e) {
			case "lt": return "<";
			case "gt": return ">";
			case "amp": return "&";
			case "quot": return '"';
			case "apos": return "'";
		}
		var c = e.charAt(1) == "x" ? parseInt(e.substr(2), 16) : parseInt(e.substr(1), 10);
		return c > 31 && c < 127 ? String.fromCharCode(c) : " ";
	});
}

/* Parse XML into { name, attrs, children, text } elements; the result is the document root element */
var SVG_TEXT_EL = { text: 1, tspan: 1, textPath: 1, a: 1 };

function svg_parse_xml(src)
{
	var n = src.length;
	var pos = 0;
	var top = { name: "#document", attrs: {}, children: [], text: "" };
	var stack = [top];
	var count = 0;
	var attre = /([^\s=\/>"']+)(?:\s*=\s*(?:"([^"]*)"|'([^']*)'|([^\s>]+)))?|(\/?>)/g;
	var e, i;
	while (pos < n) {
		var lt = src.indexOf("<", pos);
		if (lt < 0)
			break;
		if (lt > pos) {
			top.text += src.substring(pos, lt);
			if (SVG_TEXT_EL[top.name])		/* text keeps its place among the tspans around it */
				top.children.push({ name: "#text", attrs: {}, children: [], text: svg_unescape(src.substring(pos, lt)) });
		}
		if (src.substr(lt, 4) == "<!--") {
			e = src.indexOf("-->", lt + 4);
			pos = e < 0 ? n : e + 3;
			continue;
		}
		if (src.substr(lt, 9) == "<![CDATA[") {
			e = src.indexOf("]]>", lt + 9);
			top.text += src.substring(lt + 9, e < 0 ? n : e);
			if (SVG_TEXT_EL[top.name])
				top.children.push({ name: "#text", attrs: {}, children: [], text: src.substring(lt + 9, e < 0 ? n : e) });
			pos = e < 0 ? n : e + 3;
			continue;
		}
		if (src.substr(lt, 2) == "<?") {
			e = src.indexOf("?>", lt + 2);
			pos = e < 0 ? n : e + 2;
			continue;
		}
		if (src.substr(lt, 2) == "<!") {
			var depth = 0;
			for (i = lt + 2; i < n; i++) {
				var c = src.charAt(i);
				if (c == "[")
					depth++;
				else if (c == "]")
					depth--;
				else if (c == ">" && depth <= 0)
					break;
			}
			pos = i + 1;
			continue;
		}
		if (src.charAt(lt + 1) == "/") {
			e = src.indexOf(">", lt);
			if (stack.length > 1) {
				stack.pop();
				top = stack[stack.length - 1];
			}
			pos = e < 0 ? n : e + 1;
			continue;
		}
		/* start tag */
		var m = /^[^\s\/>]+/.exec(src.substr(lt + 1, 200));
		if (!m)
			break;
		var name = m[0].replace(/^.*:/, "");
		var el = { name: name, attrs: {}, children: [], text: "" };
		attre.lastIndex = lt + 1 + m[0].length;
		var selfclose = false;
		var am;
		while ((am = attre.exec(src)) !== null) {
			if (am[5] !== undefined) {
				selfclose = am[5] == "/>";
				break;
			}
			var an = am[1].replace(/^.*:/, "");
			var av = am[2] !== undefined ? am[2] : am[3] !== undefined ? am[3] : am[4] !== undefined ? am[4] : "";
			if (an != "xmlns" && an != "space")
				el.attrs[an] = svg_unescape(av);
		}
		if (am === null)
			break;
		pos = attre.lastIndex;
		if (++count > 200000 || stack.length > 100)
			throw "SVG is too complex";
		top.children.push(el);
		if (!selfclose) {
			stack.push(el);
			top = el;
		}
	}
	return top;
}

/* ---------- CSS (a small subset) ---------- */

function svg_css_decls(str)
{
	var o = {};
	var parts = str.split(";");
	for (var i = 0; i < parts.length; i++) {
		var k = parts[i].indexOf(":");
		if (k < 1)
			continue;
		var name = parts[i].substr(0, k).replace(/^\s+|\s+$/g, "").toLowerCase();
		var val = parts[i].substr(k + 1).replace(/\s*!important\s*$/i, "").replace(/^\s+|\s+$/g, "");
		o[name] = val;
	}
	return o;
}

/* rules: array of { tag, id, classes, spec, decls } */
function svg_parse_css(text, rules)
{
	text = text.replace(/\/\*[\s\S]*?\*\//g, "");
	var re = /([^{}]+)\{([^{}]*)\}/g, m;
	while ((m = re.exec(text)) !== null) {
		var decls = svg_css_decls(m[2]);
		var sels = m[1].split(",");
		for (var i = 0; i < sels.length; i++) {
			var s = sels[i].replace(/^\s+|\s+$/g, "");
			var p = /^([a-zA-Z0-9_-]*|\*)((?:[.#][\w-]+)*)$/.exec(s);
			if (!p)
				continue;		/* combinators, pseudo-classes and attribute selectors aren't supported */
			var rule = { tag: p[1] == "*" ? "" : p[1], id: "", classes: [], decls: decls, spec: 0 };
			var parts = p[2].match(/[.#][\w-]+/g) || [];
			for (var j = 0; j < parts.length; j++) {
				if (parts[j].charAt(0) == "#")
					rule.id = parts[j].substr(1);
				else
					rule.classes.push(parts[j].substr(1));
			}
			rule.spec = (rule.id ? 100 : 0) + rule.classes.length * 10 + (rule.tag ? 1 : 0);
			rule.order = rules.length;
			rules.push(rule);
		}
	}
}

function svg_rule_matches(rule, el)
{
	if (rule.tag && rule.tag != el.name)
		return false;
	if (rule.id && rule.id != el.attrs.id)
		return false;
	if (rule.classes.length) {
		var have = " " + (el.attrs["class"] || "").replace(/\s+/g, " ") + " ";
		for (var i = 0; i < rule.classes.length; i++) {
			if (have.indexOf(" " + rule.classes[i] + " ") < 0)
				return false;
		}
	}
	return true;
}

var SVG_PROPS = ["fill", "stroke", "stroke-width", "opacity", "fill-opacity", "stroke-opacity", "fill-rule",
	"stroke-linecap", "stroke-linejoin", "stroke-miterlimit", "stroke-dasharray", "stroke-dashoffset",
	"display", "visibility", "color", "stop-color", "stop-opacity", "font-size", "clip-path", "clip-rule", "mask", "filter",
	"font-family", "font-weight", "font-style", "text-anchor", "letter-spacing", "word-spacing", "text-decoration",
	"dominant-baseline", "alignment-baseline", "baseline-shift", "flood-color", "flood-opacity", "marker-start", "marker-mid",
	"marker-end", "marker", "paint-order", "mix-blend-mode"];

/* The properties an element sets itself: presentation attributes, then matching CSS rules (by specificity),
 * then its style attribute */
function svg_props(ctx, el)
{
	if (el.props)
		return el.props;
	var p = {};
	var i;
	for (i = 0; i < SVG_PROPS.length; i++) {
		if (el.attrs[SVG_PROPS[i]] !== undefined)
			p[SVG_PROPS[i]] = el.attrs[SVG_PROPS[i]];
	}
	var hits = [];
	for (i = 0; i < ctx.rules.length; i++) {
		if (svg_rule_matches(ctx.rules[i], el))
			hits.push(ctx.rules[i]);
	}
	hits.sort(function (a, b) { return a.spec != b.spec ? a.spec - b.spec : a.order - b.order; });
	var k;
	for (i = 0; i < hits.length; i++) {
		for (k in hits[i].decls)
			p[k] = hits[i].decls[k];
	}
	if (el.attrs.style) {
		var d = svg_css_decls(el.attrs.style);
		for (k in d)
			p[k] = d[k];
	}
	el.props = p;
	return p;
}

/* ---------- Colors and lengths ---------- */

var SVG_COLOR_LIST = "aliceblue:f0f8ff,antiquewhite:faebd7,aqua:00ffff,aquamarine:7fffd4,azure:f0ffff,beige:f5f5dc," +
	"bisque:ffe4c4,black:000000,blanchedalmond:ffebcd,blue:0000ff,blueviolet:8a2be2,brown:a52a2a,burlywood:deb887," +
	"cadetblue:5f9ea0,chartreuse:7fff00,chocolate:d2691e,coral:ff7f50,cornflowerblue:6495ed,cornsilk:fff8dc," +
	"crimson:dc143c,cyan:00ffff,darkblue:00008b,darkcyan:008b8b,darkgoldenrod:b8860b,darkgray:a9a9a9," +
	"darkgreen:006400,darkgrey:a9a9a9,darkkhaki:bdb76b,darkmagenta:8b008b,darkolivegreen:556b2f,darkorange:ff8c00," +
	"darkorchid:9932cc,darkred:8b0000,darksalmon:e9967a,darkseagreen:8fbc8f,darkslateblue:483d8b," +
	"darkslategray:2f4f4f,darkslategrey:2f4f4f,darkturquoise:00ced1,darkviolet:9400d3,deeppink:ff1493," +
	"deepskyblue:00bfff,dimgray:696969,dimgrey:696969,dodgerblue:1e90ff,firebrick:b22222,floralwhite:fffaf0," +
	"forestgreen:228b22,fuchsia:ff00ff,gainsboro:dcdcdc,ghostwhite:f8f8ff,gold:ffd700,goldenrod:daa520," +
	"gray:808080,grey:808080,green:008000,greenyellow:adff2f,honeydew:f0fff0,hotpink:ff69b4,indianred:cd5c5c," +
	"indigo:4b0082,ivory:fffff0,khaki:f0e68c,lavender:e6e6fa,lavenderblush:fff0f5,lawngreen:7cfc00," +
	"lemonchiffon:fffacd,lightblue:add8e6,lightcoral:f08080,lightcyan:e0ffff,lightgoldenrodyellow:fafad2," +
	"lightgray:d3d3d3,lightgreen:90ee90,lightgrey:d3d3d3,lightpink:ffb6c1,lightsalmon:ffa07a,lightseagreen:20b2aa," +
	"lightskyblue:87cefa,lightslategray:778899,lightslategrey:778899,lightsteelblue:b0c4de,lightyellow:ffffe0," +
	"lime:00ff00,limegreen:32cd32,linen:faf0e6,magenta:ff00ff,maroon:800000,mediumaquamarine:66cdaa," +
	"mediumblue:0000cd,mediumorchid:ba55d3,mediumpurple:9370db,mediumseagreen:3cb371,mediumslateblue:7b68ee," +
	"mediumspringgreen:00fa9a,mediumturquoise:48d1cc,mediumvioletred:c71585,midnightblue:191970,mintcream:f5fffa," +
	"mistyrose:ffe4e1,moccasin:ffe4b5,navajowhite:ffdead,navy:000080,oldlace:fdf5e6,olive:808000,olivedrab:6b8e23," +
	"orange:ffa500,orangered:ff4500,orchid:da70d6,palegoldenrod:eee8aa,palegreen:98fb98,paleturquoise:afeeee," +
	"palevioletred:db7093,papayawhip:ffefd5,peachpuff:ffdab9,peru:cd853f,pink:ffc0cb,plum:dda0dd,powderblue:b0e0e6," +
	"purple:800080,rebeccapurple:663399,red:ff0000,rosybrown:bc8f8f,royalblue:4169e1,saddlebrown:8b4513," +
	"salmon:fa8072,sandybrown:f4a460,seagreen:2e8b57,seashell:fff5ee,sienna:a0522d,silver:c0c0c0,skyblue:87ceeb," +
	"slateblue:6a5acd,slategray:708090,slategrey:708090,snow:fffafa,springgreen:00ff7f,steelblue:4682b4,tan:d2b48c," +
	"teal:008080,thistle:d8bfd8,tomato:ff6347,turquoise:40e0d0,violet:ee82ee,wheat:f5deb3,white:ffffff," +
	"whitesmoke:f5f5f5,yellow:ffff00,yellowgreen:9acd32";
var SVG_COLORS = null;

function svg_hsl(h, s, l)
{
	h = ((h % 360) + 360) % 360 / 360;
	function f(t)
	{
		if (t < 0) t += 1;
		if (t > 1) t -= 1;
		var q = l < 0.5 ? l * (1 + s) : l + s - l * s;
		var p = 2 * l - q;
		if (t < 1 / 6) return p + (q - p) * 6 * t;
		if (t < 1 / 2) return q;
		if (t < 2 / 3) return p + (q - p) * (2 / 3 - t) * 6;
		return p;
	}
	return [Math.round(f(h + 1 / 3) * 255), Math.round(f(h) * 255), Math.round(f(h - 1 / 3) * 255)];
}

/* Returns [r, g, b, alpha] or null for "no color" */
function svg_color(ctx, val, st, depth)
{
	if (val === undefined || val === null)
		return null;
	var v = String(val).replace(/^\s+|\s+$/g, "").toLowerCase();
	if (!v.length || v == "none" || v == "transparent")
		return null;
	if (v == "currentcolor")
		return depth > 3 ? null : svg_color(ctx, st.color, st, (depth || 0) + 1);
	var m;
	if (v.charAt(0) == "#") {
		var h = v.substr(1);
		if (h.length == 3 || h.length == 4)
			h = h.charAt(0) + h.charAt(0) + h.charAt(1) + h.charAt(1) + h.charAt(2) + h.charAt(2) + (h.length == 4 ? h.charAt(3) + h.charAt(3) : "");
		if (/^[0-9a-f]{6}([0-9a-f]{2})?$/.test(h))
			return [parseInt(h.substr(0, 2), 16), parseInt(h.substr(2, 2), 16), parseInt(h.substr(4, 2), 16),
				h.length == 8 ? parseInt(h.substr(6, 2), 16) / 255 : 1];
		return null;
	}
	if ((m = /^(rgba?|hsla?)\(([^)]*)\)$/.exec(v)) !== null) {
		var a = m[2].split(/[\s,\/]+/);
		var nums = [];
		for (var i = 0; i < a.length; i++) {
			if (!a[i].length)
				continue;
			nums.push(a[i].charAt(a[i].length - 1) == "%" ? parseFloat(a[i]) / 100 : parseFloat(a[i]));
			if (a[i].charAt(a[i].length - 1) == "%" && m[1].charAt(0) == "r" && i < 3)
				nums[nums.length - 1] *= 255;
		}
		if (nums.length < 3)
			return null;
		var alpha = nums.length > 3 ? Math.max(0, Math.min(1, nums[3])) : 1;
		if (m[1].charAt(0) == "r")
			return [Math.max(0, Math.min(255, Math.round(nums[0]))), Math.max(0, Math.min(255, Math.round(nums[1]))),
				Math.max(0, Math.min(255, Math.round(nums[2]))), alpha];
		var rgb = svg_hsl(nums[0], Math.max(0, Math.min(1, nums[1])), Math.max(0, Math.min(1, nums[2])));
		return [rgb[0], rgb[1], rgb[2], alpha];
	}
	if ((m = /^url\(\s*['"]?#([^'")\s]+)['"]?\s*\)\s*(.*)$/.exec(v)) !== null) {
		var mo = /^\s*url\(\s*['"]?#([^'")\s]+)['"]?\s*\)/i.exec(String(val));
		var target = ctx.ids[mo ? mo[1] : m[1]];
		if (target && /gradient$/i.test(target.name))
			return svg_gradient_color(ctx, target, st);
		if (target)
			return [128, 128, 128, 1];		/* pattern: a mid gray stands in for it */
		return m[2].length ? svg_color(ctx, m[2], st, (depth || 0) + 1) : null;
	}
	if (!SVG_COLORS) {
		SVG_COLORS = {};
		var list = SVG_COLOR_LIST.split(",");
		for (var k = 0; k < list.length; k++) {
			var kv = list[k].split(":");
			SVG_COLORS[kv[0]] = [parseInt(kv[1].substr(0, 2), 16), parseInt(kv[1].substr(2, 2), 16), parseInt(kv[1].substr(4, 2), 16), 1];
		}
	}
	var c = SVG_COLORS[v];
	return c ? [c[0], c[1], c[2], 1] : null;
}

/* A gradient is drawn as the average color of its stops */
function svg_gradient_color(ctx, g, st)
{
	var stops = [];
	for (var hop = 0; hop < 6 && g; hop++) {
		for (var i = 0; i < g.children.length; i++) {
			if (g.children[i].name == "stop")
				stops.push(g.children[i]);
		}
		if (stops.length)
			break;
		var href = g.attrs.href;
		g = href && href.charAt(0) == "#" ? ctx.ids[href.substr(1)] : null;
	}
	if (!stops.length)
		return null;
	var r = 0, gg = 0, b = 0, a = 0;
	for (var s = 0; s < stops.length; s++) {
		var p = svg_props(ctx, stops[s]);
		var col = svg_color(ctx, p["stop-color"] !== undefined ? p["stop-color"] : "black", st, 0) || [0, 0, 0, 1];
		r += col[0];
		gg += col[1];
		b += col[2];
		a += col[3] * (p["stop-opacity"] !== undefined ? parseFloat(p["stop-opacity"]) : 1);
	}
	var n = stops.length;
	return [Math.round(r / n), Math.round(gg / n), Math.round(b / n), a / n];
}

/* A length in px; ref is what a percentage is a percentage of */
function svg_len(str, ref, dflt)
{
	if (str === undefined || str === null)
		return dflt;
	var m = /^\s*([-+]?(?:\d+\.?\d*|\.\d+)(?:[eE][-+]?\d+)?)\s*(px|pt|pc|mm|cm|in|em|ex|%)?\s*$/.exec(String(str));
	if (!m)
		return dflt;
	var v = parseFloat(m[1]);
	switch (m[2]) {
		case "pt": return v * 4 / 3;
		case "pc": return v * 16;
		case "mm": return v * 96 / 25.4;
		case "cm": return v * 96 / 2.54;
		case "in": return v * 96;
		case "em": return v * 16;
		case "ex": return v * 8;
		case "%": return v * (ref || 0) / 100;
	}
	return v;
}

function svg_numbers(str)
{
	var out = [];
	var m, re = /[-+]?(?:\d+\.?\d*|\.\d+)(?:[eE][-+]?\d+)?/g;
	while ((m = re.exec(str || "")) !== null)
		out.push(parseFloat(m[0]));
	return out;
}

/* ---------- Transforms ---------- */

function svg_transform(str)
{
	var result = [1, 0, 0, 1, 0, 0];
	if (!str)
		return result;
	var re = /(matrix|translate|scale|rotate|skewX|skewY)\s*\(([^)]*)\)/g, m;
	while ((m = re.exec(str)) !== null) {
		var a = svg_numbers(m[2]);
		var t = null;
		switch (m[1]) {
			case "matrix":
				if (a.length >= 6)
					t = a.slice(0, 6);
				break;
			case "translate":
				t = [1, 0, 0, 1, a[0] || 0, a[1] || 0];
				break;
			case "scale":
				t = [a[0] === undefined ? 1 : a[0], 0, 0, a.length > 1 ? a[1] : (a[0] === undefined ? 1 : a[0]), 0, 0];
				break;
			case "rotate":
				var r = (a[0] || 0) * Math.PI / 180, c = Math.cos(r), s = Math.sin(r);
				t = [c, s, -s, c, 0, 0];
				if (a.length >= 3)
					t = mat_mul(mat_mul([1, 0, 0, 1, -a[1], -a[2]], t), [1, 0, 0, 1, a[1], a[2]]);
				break;
			case "skewX":
				t = [1, 0, Math.tan((a[0] || 0) * Math.PI / 180), 1, 0, 0];
				break;
			case "skewY":
				t = [1, Math.tan((a[0] || 0) * Math.PI / 180), 0, 1, 0, 0];
				break;
		}
		if (t)
			result = mat_mul(t, result);	/* "A B" applies B first, then A */
	}
	return result;
}

/* ---------- Path data ---------- */

/* Append the cubic curves for an SVG elliptical arc to cmds */
function svg_arc(cmds, x0, y0, rx, ry, phi, fa, fs, x, y)
{
	if (rx == 0 || ry == 0 || (x0 == x && y0 == y)) {
		cmds.push(["L", x, y]);
		return;
	}
	rx = Math.abs(rx);
	ry = Math.abs(ry);
	var ph = phi * Math.PI / 180, cp = Math.cos(ph), sp = Math.sin(ph);
	var dx2 = (x0 - x) / 2, dy2 = (y0 - y) / 2;
	var x1p = cp * dx2 + sp * dy2, y1p = -sp * dx2 + cp * dy2;
	var lam = x1p * x1p / (rx * rx) + y1p * y1p / (ry * ry);
	if (lam > 1) {
		var sl = Math.sqrt(lam);
		rx *= sl;
		ry *= sl;
	}
	var num = rx * rx * ry * ry - rx * rx * y1p * y1p - ry * ry * x1p * x1p;
	var den = rx * rx * y1p * y1p + ry * ry * x1p * x1p;
	var co = (fa == fs ? -1 : 1) * Math.sqrt(Math.max(0, den == 0 ? 0 : num / den));
	var cxp = co * rx * y1p / ry, cyp = -co * ry * x1p / rx;
	var cx = cp * cxp - sp * cyp + (x0 + x) / 2, cy = sp * cxp + cp * cyp + (y0 + y) / 2;
	var th1 = Math.atan2((y1p - cyp) / ry, (x1p - cxp) / rx);
	var th2 = Math.atan2((-y1p - cyp) / ry, (-x1p - cxp) / rx);
	var dth = th2 - th1;
	if (!fs && dth > 0)
		dth -= 2 * Math.PI;
	else if (fs && dth < 0)
		dth += 2 * Math.PI;
	var segs = Math.max(1, Math.ceil(Math.abs(dth) / (Math.PI / 2) - 1e-9));
	var delta = dth / segs;
	var t = 4 / 3 * Math.tan(delta / 4);
	function pt(X, Y)
	{
		return [cx + cp * rx * X - sp * ry * Y, cy + sp * rx * X + cp * ry * Y];
	}
	for (var i = 0; i < segs; i++) {
		var a = th1 + i * delta, b = a + delta;
		var c1 = pt(Math.cos(a) - t * Math.sin(a), Math.sin(a) + t * Math.cos(a));
		var c2 = pt(Math.cos(b) + t * Math.sin(b), Math.sin(b) - t * Math.cos(b));
		var e = i == segs - 1 ? [x, y] : pt(Math.cos(b), Math.sin(b));
		cmds.push(["C", c1[0], c1[1], c2[0], c2[1], e[0], e[1]]);
	}
}

/* Path data -> array of ["M", x, y] ["L", x, y] ["C", x1, y1, x2, y2, x, y] ["Z"] (all absolute) */
function svg_path_cmds(d)
{
	var cmds = [];
	var i = 0, n = d.length;
	var numre = /[-+]?(?:\d+\.?\d*|\.\d+)(?:[eE][-+]?\d+)?/g;
	var cx = 0, cy = 0, sx = 0, sy = 0;
	var lcx = 0, lcy = 0;		/* last control point, for S and T */
	var lastc = "";
	var cmd = "";
	var guard = 0;

	function skip()
	{
		while (i < n && /[\s,]/.test(d.charAt(i)))
			i++;
	}
	function num()
	{
		skip();
		numre.lastIndex = i;
		var m = numre.exec(d);
		if (!m || m.index != i)
			return NaN;
		i += m[0].length;
		return parseFloat(m[0]);
	}
	function flag()
	{
		skip();
		var c = d.charAt(i);
		if (c == "0" || c == "1") {
			i++;
			return c == "1" ? 1 : 0;
		}
		return NaN;
	}

	while (i < n && guard++ < 400000) {
		skip();
		if (i >= n)
			break;
		var ch = d.charAt(i);
		if (/[A-Za-z]/.test(ch)) {
			cmd = ch;
			i++;
		}
		else if (!cmd.length || cmd == "Z" || cmd == "z")
			break;
		var rel = cmd == cmd.toLowerCase();
		var op = cmd.toUpperCase();
		var x, y, x1, y1, x2, y2;
		if (op == "Z") {
			cmds.push(["Z"]);
			cx = sx;
			cy = sy;
			lastc = "Z";
			continue;
		}
		if (op == "M") {
			x = num(); y = num();
			if (isNaN(x) || isNaN(y)) break;
			if (rel) { x += cx; y += cy; }
			cmds.push(["M", x, y]);
			cx = sx = x; cy = sy = y;
			cmd = rel ? "l" : "L";		/* further pairs are line-tos */
		}
		else if (op == "L") {
			x = num(); y = num();
			if (isNaN(x) || isNaN(y)) break;
			if (rel) { x += cx; y += cy; }
			cmds.push(["L", x, y]);
			cx = x; cy = y;
		}
		else if (op == "H") {
			x = num();
			if (isNaN(x)) break;
			if (rel) x += cx;
			cmds.push(["L", x, cy]);
			cx = x;
		}
		else if (op == "V") {
			y = num();
			if (isNaN(y)) break;
			if (rel) y += cy;
			cmds.push(["L", cx, y]);
			cy = y;
		}
		else if (op == "C" || op == "S") {
			if (op == "C") {
				x1 = num(); y1 = num();
			}
			else {
				x1 = (lastc == "C" || lastc == "S") ? 2 * cx - lcx : cx;
				y1 = (lastc == "C" || lastc == "S") ? 2 * cy - lcy : cy;
			}
			x2 = num(); y2 = num(); x = num(); y = num();
			if (isNaN(x1) || isNaN(y1) || isNaN(x2) || isNaN(y2) || isNaN(x) || isNaN(y)) break;
			if (rel) {
				if (op == "C") { x1 += cx; y1 += cy; }
				x2 += cx; y2 += cy; x += cx; y += cy;
			}
			cmds.push(["C", x1, y1, x2, y2, x, y]);
			lcx = x2; lcy = y2;
			cx = x; cy = y;
		}
		else if (op == "Q" || op == "T") {
			var qx, qy;
			if (op == "Q") {
				qx = num(); qy = num();
			}
			else {
				qx = (lastc == "Q" || lastc == "T") ? 2 * cx - lcx : cx;
				qy = (lastc == "Q" || lastc == "T") ? 2 * cy - lcy : cy;
			}
			x = num(); y = num();
			if (isNaN(qx) || isNaN(qy) || isNaN(x) || isNaN(y)) break;
			if (rel) {
				if (op == "Q") { qx += cx; qy += cy; }
				x += cx; y += cy;
			}
			cmds.push(["C", cx + 2 / 3 * (qx - cx), cy + 2 / 3 * (qy - cy), x + 2 / 3 * (qx - x), y + 2 / 3 * (qy - y), x, y]);
			lcx = qx; lcy = qy;
			cx = x; cy = y;
		}
		else if (op == "A") {
			var rx = num(), ry = num(), rot = num(), fa = flag(), fs = flag();
			x = num(); y = num();
			if (isNaN(rx) || isNaN(ry) || isNaN(rot) || isNaN(fa) || isNaN(fs) || isNaN(x) || isNaN(y)) break;
			if (rel) { x += cx; y += cy; }
			svg_arc(cmds, cx, cy, rx, ry, rot, fa, fs, x, y);
			cx = x; cy = y;
		}
		else
			break;
		lastc = op;
	}
	return cmds;
}

/* ---------- Shapes ---------- */

function svg_ellipse_cmds(cx, cy, rx, ry)
{
	var k = 0.5522847498;
	return [["M", cx + rx, cy],
		["C", cx + rx, cy + k * ry, cx + k * rx, cy + ry, cx, cy + ry],
		["C", cx - k * rx, cy + ry, cx - rx, cy + k * ry, cx - rx, cy],
		["C", cx - rx, cy - k * ry, cx - k * rx, cy - ry, cx, cy - ry],
		["C", cx + k * rx, cy - ry, cx + rx, cy - k * ry, cx + rx, cy],
		["Z"]];
}

function svg_rect_cmds(x, y, w, h, rx, ry)
{
	if (rx > w / 2)
		rx = w / 2;
	if (ry > h / 2)
		ry = h / 2;
	if (rx <= 0 || ry <= 0)
		return [["M", x, y], ["L", x + w, y], ["L", x + w, y + h], ["L", x, y + h], ["Z"]];
	var k = 0.5522847498;
	return [["M", x + rx, y], ["L", x + w - rx, y],
		["C", x + w - rx + k * rx, y, x + w, y + ry - k * ry, x + w, y + ry],
		["L", x + w, y + h - ry],
		["C", x + w, y + h - ry + k * ry, x + w - rx + k * rx, y + h, x + w - rx, y + h],
		["L", x + rx, y + h],
		["C", x + rx - k * rx, y + h, x, y + h - ry + k * ry, x, y + h - ry],
		["L", x, y + ry],
		["C", x, y + ry - k * ry, x + rx - k * rx, y, x + rx, y],
		["Z"]];
}

/* The path commands for a shape element, or null */
function svg_shape_cmds(ctx, el, vw, vh)
{
	var a = el.attrs;
	var diag = Math.sqrt((vw * vw + vh * vh) / 2);
	switch (el.name) {
		case "path":
			return a.d ? svg_path_cmds(a.d) : null;
		case "rect":
			var w = svg_len(a.width, vw, 0), h = svg_len(a.height, vh, 0);
			if (w <= 0 || h <= 0)
				return null;
			var rx = svg_len(a.rx, vw, -1), ry = svg_len(a.ry, vh, -1);
			if (rx < 0 && ry < 0) {
				rx = 0;
				ry = 0;
			}
			else if (rx < 0)
				rx = ry;
			else if (ry < 0)
				ry = rx;
			return svg_rect_cmds(svg_len(a.x, vw, 0), svg_len(a.y, vh, 0), w, h, rx, ry);
		case "circle":
			var r = svg_len(a.r, diag, 0);
			return r > 0 ? svg_ellipse_cmds(svg_len(a.cx, vw, 0), svg_len(a.cy, vh, 0), r, r) : null;
		case "ellipse":
			var ex = svg_len(a.rx, vw, 0), ey = svg_len(a.ry, vh, 0);
			return ex > 0 && ey > 0 ? svg_ellipse_cmds(svg_len(a.cx, vw, 0), svg_len(a.cy, vh, 0), ex, ey) : null;
		case "line":
			return [["M", svg_len(a.x1, vw, 0), svg_len(a.y1, vh, 0)], ["L", svg_len(a.x2, vw, 0), svg_len(a.y2, vh, 0)]];
		case "polyline":
		case "polygon":
			var p = svg_numbers(a.points);
			if (p.length < 4)
				return null;
			var cmds = [["M", p[0], p[1]]];
			for (var i = 2; i + 1 < p.length; i += 2)
				cmds.push(["L", p[i], p[i + 1]]);
			if (el.name == "polygon")
				cmds.push(["Z"]);
			return cmds;
	}
	return null;
}

/* ---------- Painting ---------- */

/* Device-space subpaths [{ pts, closed }] from path commands */
function svg_flatten(cmds, M)
{
	var subs = [];
	var cur = null;
	var cx = 0, cy = 0, sx = 0, sy = 0;
	var a = M[0], b = M[1], c = M[2], d = M[3], e = M[4], f = M[5];
	for (var i = 0; i < cmds.length; i++) {
		var cm = cmds[i];
		var x, y;
		if (cm[0] == "M") {
			x = a * cm[1] + c * cm[2] + e;
			y = b * cm[1] + d * cm[2] + f;
			cur = { pts: [x, y], closed: false };
			subs.push(cur);
			cx = sx = x;
			cy = sy = y;
			continue;
		}
		if (cm[0] == "Z") {
			if (cur)
				cur.closed = true;
			cur = null;
			cx = sx;
			cy = sy;
			continue;
		}
		if (!cur) {		/* a segment right after a close starts at the previous start point */
			cur = { pts: [sx, sy], closed: false };
			subs.push(cur);
			cx = sx;
			cy = sy;
		}
		if (cm[0] == "L") {
			x = a * cm[1] + c * cm[2] + e;
			y = b * cm[1] + d * cm[2] + f;
			cur.pts.push(x, y);
			cx = x;
			cy = y;
		}
		else {
			var p1x = a * cm[1] + c * cm[2] + e, p1y = b * cm[1] + d * cm[2] + f;
			var p2x = a * cm[3] + c * cm[4] + e, p2y = b * cm[3] + d * cm[4] + f;
			x = a * cm[5] + c * cm[6] + e;
			y = b * cm[5] + d * cm[6] + f;
			flatten_curve(cur.pts, cx, cy, p1x, p1y, p2x, p2y, x, y);
			cx = x;
			cy = y;
		}
	}
	return subs;
}

/* ---------- Paint servers: gradients ---------- */

var SVG_COV = null;		/* reusable coverage buffer; every user leaves it all zero */

function svg_cov_buffer()
{
	if (!SVG_COV || SVG_COV.length < FW * FH)
		SVG_COV = new Uint8Array(FW * FH);
	return SVG_COV;
}

/* The element a url(#id) value points to, or null */
function svg_ref(ctx, val)
{
	if (val === undefined || val === null)
		return null;
	var m = /^\s*url\(\s*['"]?#([^'")\s]+)['"]?\s*\)/i.exec(String(val));
	return m ? (ctx.ids[m[1]] || null) : null;
}

/* The gradient or pattern a paint value refers to, or null for a plain color */
function svg_server(ctx, val)
{
	var el = svg_ref(ctx, val);
	if (el && (el.name == "linearGradient" || el.name == "radialGradient" || el.name == "pattern"))
		return el;
	return null;
}

/* The element followed by whatever it refers to with href, and so on */
function svg_href_chain(ctx, el)
{
	var chain = [];
	for (var hop = 0; hop < 8 && el; hop++) {
		chain.push(el);
		var href = el.attrs.href;
		el = href && href.charAt(0) == "#" ? ctx.ids[href.substr(1)] : null;
		if (el && chain.indexOf(el) >= 0)
			break;
	}
	return chain;
}

function svg_chain_attr(chain, name)
{
	for (var i = 0; i < chain.length; i++) {
		if (chain[i].attrs[name] !== undefined)
			return chain[i].attrs[name];
	}
	return undefined;
}

/* A number or percentage in objectBoundingBox units (a fraction), or a length in user units */
function svg_unit(ctx, str, dflt, obb, ref)
{
	if (str === undefined || str === null || !String(str).length)
		return dflt;
	str = String(str).replace(/^\s+|\s+$/g, "");
	if (obb) {
		var v = parseFloat(str);
		if (isNaN(v))
			return dflt;
		return /%$/.test(str) ? v / 100 : v;
	}
	return svg_len(str, ref, dflt);
}

/* Color stops of a gradient as a 256-entry lookup: { rgb: Int32Array (0xRRGGBB), a: Float32Array (0-1) } */
function svg_gradient_lut(ctx, chain, st)
{
	var stops = [];
	var i, k;
	for (var hop = 0; hop < chain.length && !stops.length; hop++) {
		var kids = chain[hop].children;
		for (i = 0; i < kids.length; i++) {
			if (kids[i].name != "stop")
				continue;
			var sp = svg_props(ctx, kids[i]);
			var offs = kids[i].attrs.offset;
			var off = 0;
			if (offs !== undefined) {
				off = parseFloat(offs);
				if (isNaN(off))
					off = 0;
				else if (/%\s*$/.test(offs))
					off /= 100;
			}
			var col = svg_color(ctx, sp["stop-color"] !== undefined ? sp["stop-color"] : "black", st, 0) || [0, 0, 0, 1];
			var so = sp["stop-opacity"] !== undefined ? parseFloat(sp["stop-opacity"]) : 1;
			stops.push({ off: Math.max(0, Math.min(1, off)), r: col[0], g: col[1], b: col[2], a: col[3] * (isNaN(so) ? 1 : Math.max(0, Math.min(1, so))) });
		}
	}
	if (!stops.length)
		return null;
	for (i = 1; i < stops.length; i++) {
		if (stops[i].off < stops[i - 1].off)
			stops[i].off = stops[i - 1].off;
	}
	var rgb = new Int32Array(256), al = new Float32Array(256);
	for (i = 0; i < 256; i++) {
		var t = i / 255;
		var a0, a1;
		if (t <= stops[0].off)
			a0 = a1 = stops[0];
		else if (t >= stops[stops.length - 1].off)
			a0 = a1 = stops[stops.length - 1];
		else {
			for (k = 0; k < stops.length - 1 && stops[k + 1].off < t; k++);
			a0 = stops[k];
			a1 = stops[k + 1];
		}
		var f = a1.off > a0.off ? (t - a0.off) / (a1.off - a0.off) : 0;
		rgb[i] = ((a0.r + (a1.r - a0.r) * f + 0.5) << 16) | ((a0.g + (a1.g - a0.g) * f + 0.5) << 8) | ((a0.b + (a1.b - a0.b) * f + 0.5) | 0);
		al[i] = a0.a + (a1.a - a0.a) * f;
	}
	return { rgb: rgb, a: al };
}

/* Build a painter for a linear or radial gradient.  bbox is the bounding box of the shape being
 * painted in its own user space, [x0, y0, x1, y1] (needed for objectBoundingBox units). */
function svg_gradient_painter(ctx, g, st, ctm, bbox)
{
	var chain = svg_href_chain(ctx, g);
	var lut = svg_gradient_lut(ctx, chain, st);
	if (!lut)
		return null;
	var obb = svg_chain_attr(chain, "gradientUnits") != "userSpaceOnUse";
	var M;
	var gt = svg_chain_attr(chain, "gradientTransform");
	var gtm = gt ? svg_transform(gt) : [1, 0, 0, 1, 0, 0];
	if (obb) {
		if (!bbox)
			return null;
		var bw = bbox[2] - bbox[0], bh = bbox[3] - bbox[1];
		if (bw <= 0 || bh <= 0)
			return { none: true };
		M = mat_mul(gtm, mat_mul([bw, 0, 0, bh, bbox[0], bbox[1]], ctm));
	}
	else
		M = mat_mul(gtm, ctm);
	var inv = mat_inv(M);
	if (!inv)
		return null;
	var spread = svg_chain_attr(chain, "spreadMethod");
	var pt = { inv: inv, lut: lut.rgb, la: lut.a, spread: spread == "reflect" ? 1 : spread == "repeat" ? 2 : 0 };
	var vw = ctx.vw, vh = ctx.vh;
	function a(name, dflt, ref)
	{
		return svg_unit(ctx, svg_chain_attr(chain, name), dflt, obb, ref);
	}
	if (g.name == "linearGradient") {
		pt.kind = 1;
		pt.x1 = a("x1", 0, vw);
		pt.y1 = a("y1", 0, vh);
		pt.x2 = a("x2", obb ? 1 : vw, vw);
		pt.y2 = a("y2", 0, vh);
		var dx = pt.x2 - pt.x1, dy = pt.y2 - pt.y1;
		pt.len2 = dx * dx + dy * dy;
		if (pt.len2 <= 0)		/* a zero-length gradient paints the last stop's color */
			return { solid: lut.rgb[255], alpha: lut.a[255] };
	}
	else {
		pt.kind = 2;
		var half = obb ? 0.5 : ctx.diag / 2;
		pt.cx = a("cx", obb ? 0.5 : vw / 2, vw);
		pt.cy = a("cy", obb ? 0.5 : vh / 2, vh);
		pt.r = a("r", obb ? 0.5 : half, ctx.diag);
		if (pt.r <= 0)
			return { solid: lut.rgb[255], alpha: lut.a[255] };
		pt.fx = a("fx", pt.cx, vw);
		pt.fy = a("fy", pt.cy, vh);
		var fdx = pt.fx - pt.cx, fdy = pt.fy - pt.cy, fd = Math.sqrt(fdx * fdx + fdy * fdy);
		if (fd > pt.r * 0.999) {		/* the focal point has to stay inside the circle */
			pt.fx = pt.cx + fdx * pt.r * 0.999 / fd;
			pt.fy = pt.cy + fdy * pt.r * 0.999 / fd;
		}
		pt.ex = pt.cx - pt.fx;
		pt.ey = pt.cy - pt.fy;
		pt.qa = pt.ex * pt.ex + pt.ey * pt.ey - pt.r * pt.r;
	}
	return pt;
}

/* Bounding box of a path in its user space, [x0, y0, x1, y1], or null */
function svg_cmds_box(cmds)
{
	var subs = svg_flatten(cmds, [1, 0, 0, 1, 0, 0]);
	var x0 = 1e30, y0 = 1e30, x1 = -1e30, y1 = -1e30, i, j;
	for (i = 0; i < subs.length; i++) {
		var pts = subs[i].pts;
		for (j = 0; j < pts.length; j += 2) {
			if (pts[j] < x0) x0 = pts[j];
			if (pts[j] > x1) x1 = pts[j];
			if (pts[j + 1] < y0) y0 = pts[j + 1];
			if (pts[j + 1] > y1) y1 = pts[j + 1];
		}
	}
	return x0 <= x1 ? [x0, y0, x1, y1] : null;
}

/* Paint the device-space polygons with a painter (gradient or pattern), anti-aliased through a
 * coverage buffer.  opac is the combined opacity (0-1). */
function svg_cover_paint(ctx, polys, evenodd, pt, opac)
{
	var clip = ctx.clip;
	var i, j;
	var minx = 1e30, miny = 1e30, maxx = -1e30, maxy = -1e30;
	for (i = 0; i < polys.length; i++) {
		var q = polys[i];
		for (j = 0; j < q.length; j += 2) {
			if (q[j] < minx) minx = q[j];
			if (q[j] > maxx) maxx = q[j];
			if (q[j + 1] < miny) miny = q[j + 1];
			if (q[j + 1] > maxy) maxy = q[j + 1];
		}
	}
	var x0 = Math.max(0, Math.floor(minx), Math.floor(clip[0])), y0 = Math.max(0, Math.floor(miny), Math.floor(clip[1]));
	var x1 = Math.min(FW, Math.ceil(maxx) + 1, Math.ceil(clip[2])), y1 = Math.min(FH, Math.ceil(maxy) + 1, Math.ceil(clip[3]));
	if (x1 <= x0 || y1 <= y0)
		return;
	var cov = svg_cov_buffer();
	var sct = COVTARGET;
	COVTARGET = cov;
	fill_polys(polys, evenodd, [0, 0, 0], 1, clip);
	COVTARGET = sct;
	var mask = CLIPMASK;
	var inv = pt.inv, ia = inv ? inv[0] : 0, ib = inv ? inv[1] : 0, ic = inv ? inv[2] : 0, id = inv ? inv[3] : 0, ie = inv ? inv[4] : 0, iff = inv ? inv[5] : 0;
	var lut = pt.lut, la = pt.la, spread = pt.spread, kind = pt.kind;
	var x, y, row, c, a, t, idx, gx, gy, pv, rr, gg, bb, col;
	for (y = y0; y < y1; y++) {
		row = y * FW;
		var Y = y + 0.5;
		for (x = x0; x < x1; x++) {
			c = cov[row + x];
			if (!c)
				continue;
			cov[row + x] = 0;
			a = c * 0.00392157 * opac;
			if (mask)
				a *= mask[row + x] * 0.00392157;
			if (a <= 0.003)
				continue;
			if (kind === 1 || kind === 2) {
				var X = x + 0.5;
				gx = ia * X + ic * Y + ie;
				gy = ib * X + id * Y + iff;
				if (kind === 1)
					t = ((gx - pt.x1) * (pt.x2 - pt.x1) + (gy - pt.y1) * (pt.y2 - pt.y1)) / pt.len2;
				else {
					var dx = gx - pt.fx, dy = gy - pt.fy;
					var cc = dx * dx + dy * dy, bq = dx * pt.ex + dy * pt.ey;
					if (pt.qa > -1e-9 && pt.qa < 1e-9)
						t = bq != 0 ? cc / (2 * bq) : 0;
					else {
						var disc = bq * bq - pt.qa * cc;
						t = (bq - Math.sqrt(disc < 0 ? 0 : disc)) / pt.qa;
					}
				}
				if (spread === 0)
					t = t < 0 ? 0 : (t > 1 ? 1 : t);
				else if (spread === 2)
					t = t - Math.floor(t);
				else {
					t = Math.abs(t) % 2;
					if (t > 1)
						t = 2 - t;
				}
				idx = (t * 255 + 0.5) | 0;
				col = lut[idx];
				a *= la[idx];
			}
			else if (kind === 3) {		/* pattern tile */
				var PX = x + 0.5;
				var u = (ia * PX + ic * Y + ie) - pt.tx, v = (ib * PX + id * Y + iff) - pt.ty;
				u = u / pt.tw - Math.floor(u / pt.tw);
				v = v / pt.th - Math.floor(v / pt.th);
				idx = ((v * pt.ph) | 0) * pt.pw + ((u * pt.pw) | 0);
				a *= pt.alpha[idx] * 0.00392157;
				col = pt.rgb[idx];
			}
			else
				col = pt.solid;
			if (a <= 0.003)
				continue;
			rr = (col >> 16) & 255;
			gg = (col >> 8) & 255;
			bb = col & 255;
			if (a >= 0.996)
				FB[row + x] = col;
			else {
				pv = FB[row + x];
				FB[row + x] = ((((pv >> 16) & 255) + (rr - ((pv >> 16) & 255)) * a + 0.5) << 16) |
					((((pv >> 8) & 255) + (gg - ((pv >> 8) & 255)) * a + 0.5) << 8) |
					(((pv & 255) + (bb - (pv & 255)) * a + 0.5) | 0);
			}
		}
	}
}

/* Fill polygons with a gradient or pattern element.  Returns false if it couldn't be done. */
function svg_fill_server(ctx, polys, evenodd, srv, st, ctm, bbox, opac)
{
	var pt = null;
	if (srv.name == "pattern")
		pt = svg_pattern_painter(ctx, srv, st, ctm, bbox);
	else
		pt = svg_gradient_painter(ctx, srv, st, ctm, bbox);
	if (!pt)
		return false;
	if (pt.none)
		return true;
	if (pt.solid !== undefined) {
		var o = opac * pt.alpha;
		if (o > 0)
			fill_polys(polys, evenodd, [(pt.solid >> 16) & 255, (pt.solid >> 8) & 255, pt.solid & 255], Math.min(1, o), ctx.clip);
		return true;
	}
	svg_cover_paint(ctx, polys, evenodd, pt, opac);
	return true;
}

/* Build a painter for a pattern: the tile is drawn once at device resolution, and shapes are then
 * filled by looking pixels up in it through the pattern's transform. */
function svg_pattern_painter(ctx, pel, st, ctm, bbox)
{
	if (ctx.patdepth > 2)
		return null;
	var chain = svg_href_chain(ctx, pel);
	var pobb = svg_chain_attr(chain, "patternUnits") != "userSpaceOnUse";
	var cobb = svg_chain_attr(chain, "patternContentUnits") == "objectBoundingBox";
	var bw = bbox ? bbox[2] - bbox[0] : 0, bh = bbox ? bbox[3] - bbox[1] : 0;
	if ((pobb || cobb) && (!bbox || bw <= 0 || bh <= 0))
		return { none: true };
	function a(name, dflt, ref)
	{
		return svg_unit(ctx, svg_chain_attr(chain, name), dflt, pobb, ref);
	}
	var x, y, w, h;
	if (pobb) {
		x = bbox[0] + a("x", 0, 1) * bw;
		y = bbox[1] + a("y", 0, 1) * bh;
		w = a("width", 0, 1) * bw;
		h = a("height", 0, 1) * bh;
	}
	else {
		x = a("x", 0, ctx.vw);
		y = a("y", 0, ctx.vh);
		w = a("width", 0, ctx.vw);
		h = a("height", 0, ctx.vh);
	}
	if (w <= 0 || h <= 0)
		return { none: true };
	var ptr = svg_chain_attr(chain, "patternTransform");
	var M = mat_mul(ptr ? svg_transform(ptr) : [1, 0, 0, 1, 0, 0], ctm);
	var inv = mat_inv(M);
	if (!inv)
		return null;
	var scx = Math.sqrt(M[0] * M[0] + M[1] * M[1]), scy = Math.sqrt(M[2] * M[2] + M[3] * M[3]);
	var tw = Math.max(1, Math.min(512, Math.round(w * scx))), th = Math.max(1, Math.min(512, Math.round(h * scy)));
	/* the element whose children are the tile's content */
	var src = null;
	for (var i = 0; i < chain.length && !src; i++) {
		for (var k = 0; k < chain[i].children.length; k++)
			if (!SVG_SKIP[chain[i].children[k].name] || chain[i].children[k].name == "defs") {
				src = chain[i];
				break;
			}
	}
	if (!src)
		return { none: true };
	/* content coordinates -> tile pixels */
	var cm;
	var vbs = svg_chain_attr(chain, "viewBox");
	var vb = vbs ? svg_numbers(vbs) : [];
	if (vb.length >= 4 && vb[2] > 0 && vb[3] > 0) {
		var par = String(svg_chain_attr(chain, "preserveAspectRatio") || "xMidYMid meet").split(/\s+/);
		var sx = w / vb[2], sy = h / vb[3];
		if (par[0] == "none")
			cm = [sx, 0, 0, sy, -vb[0] * sx, -vb[1] * sy];
		else {
			var sc = par[1] == "slice" ? Math.max(sx, sy) : Math.min(sx, sy);
			var ax = /xMin/.test(par[0]) ? 0 : (/xMax/.test(par[0]) ? 1 : 0.5), ay = /YMin/.test(par[0]) ? 0 : (/YMax/.test(par[0]) ? 1 : 0.5);
			cm = [sc, 0, 0, sc, (w - vb[2] * sc) * ax - vb[0] * sc, (h - vb[3] * sc) * ay - vb[1] * sc];
		}
	}
	else if (cobb)
		cm = [bw, 0, 0, bh, 0, 0];
	else
		cm = [1, 0, 0, 1, 0, 0];
	cm = mat_mul(cm, [tw / w, 0, 0, th / h, 0, 0]);
	/* draw the tile over black and over white to find its transparency */
	var sfb = FB, sfw = FW, sfh = FH, sclip = ctx.clip, smask = CLIPMASK, scov = SVG_COV;
	var black, white;
	ctx.patdepth++;
	try {
		FW = tw;
		FH = th;
		SVG_COV = null;
		ctx.clip = [0, 0, tw, th];
		var pst = svg_derive(ctx, src, ctx.st0);
		var drawtile = function () {
			CLIPMASK = null;
			for (var c = 0; c < src.children.length; c++)
				if (!svg_draw(ctx, src.children[c], pst, cm, 1))
					break;
		};
		var tile = function (bg) {
			var buf = new Int32Array(tw * th);
			if (bg)
				for (var j = 0; j < buf.length; j++)
					buf[j] = bg;
			FB = buf;
			drawtile();
			return buf;
		};
		black = tile(0x000000);
		white = tile(0xffffff);
	}
	finally {
		FB = sfb;
		FW = sfw;
		FH = sfh;
		ctx.clip = sclip;
		CLIPMASK = smask;
		SVG_COV = scov;
		ctx.patdepth--;
	}
	var rgb = new Int32Array(tw * th), alpha = new Uint8Array(tw * th);
	for (var n = 0; n < rgb.length; n++) {
		var bv = black[n], wv = white[n];
		var br = (bv >> 16) & 255, bg = (bv >> 8) & 255, bb = bv & 255;
		var al = 1 - (((wv >> 16) & 255) - br + ((wv >> 8) & 255) - bg + (wv & 255) - bb) / 765;
		if (al < 0.004)
			continue;
		al = Math.min(1, al);
		alpha[n] = al * 255 + 0.5;
		rgb[n] = (Math.min(255, br / al + 0.5) << 16) | (Math.min(255, bg / al + 0.5) << 8) | (Math.min(255, bb / al + 0.5) | 0);
	}
	return { kind: 3, inv: inv, tx: x, ty: y, tw: w, th: h, pw: tw, ph: th, rgb: rgb, alpha: alpha, lut: null, la: null, spread: 0 };
}

function svg_paint(ctx, cmds, st, ctm, filled)
{
	if (st.visibility == "hidden" || st.visibility == "collapse" || !cmds || !cmds.length)
		return;
	var subs = svg_flatten(cmds, ctm);
	var i;
	var bbox = null, bboxed = false;
	function get_bbox()
	{
		if (!bboxed) {
			bboxed = true;
			bbox = svg_cmds_box(cmds);
		}
		return bbox;
	}
	function paint_fill()
	{
		var fsrv = filled ? svg_server(ctx, st.fill) : null;
		var fillc = filled ? svg_color(ctx, st.fill, st, 0) : null;
		if (fillc || fsrv) {
			var polys = [];
			for (i = 0; i < subs.length; i++) {
				if (subs[i].pts.length >= 6)
					polys.push(subs[i].pts);
			}
			var fa = st.opacity * st.fillOpacity * (fsrv ? 1 : fillc[3]);
			if (polys.length && fa > 0) {
				if (!fsrv || !svg_fill_server(ctx, polys, st.fillRule == "evenodd", fsrv, st, ctm, get_bbox(), fa))
					if (fillc)
						fill_polys(polys, st.fillRule == "evenodd", [fillc[0], fillc[1], fillc[2]], Math.min(1, st.opacity * st.fillOpacity * fillc[3]), ctx.clip);
			}
		}
	}
	function paint_stroke()
	{
		var ssrv = svg_server(ctx, st.stroke);
		var strc = svg_color(ctx, st.stroke, st, 0);
		if ((strc || ssrv) && st.strokeWidth > 0) {
			var sc = mat_scale(ctm);
			var lw = Math.max(1, st.strokeWidth * sc);
			var hw = lw / 2;
			var cap = st.cap == "round" ? 1 : st.cap == "square" ? 2 : 0;
			var join = st.join == "round" ? 1 : st.join == "bevel" ? 2 : 0;
			var dash = null;
			if (st.dash && st.dash.length) {
				dash = [];
				var tot = 0;
				for (i = 0; i < st.dash.length; i++) {
					dash.push(Math.max(0, st.dash[i] * sc));
					tot += dash[dash.length - 1];
				}
				if (dash.length % 2)
					dash = dash.concat(dash);
				if (tot < 0.05)
					dash = null;
			}
			var out = [];
			for (i = 0; i < subs.length; i++) {
				var sub = subs[i];
				if (sub.pts.length < 4 && !(sub.pts.length == 2 && cap == 1))
					continue;
				if (dash) {
					var parts = dash_subpath(sub, dash, st.dashOffset * sc);
					for (var k = 0; k < parts.length; k++)
						stroke_subpath(parts[k], hw, cap, join, st.miter, out);
				}
				else
					stroke_subpath(sub, hw, cap, join, st.miter, out);
			}
			var sa = st.opacity * st.strokeOpacity * (ssrv ? 1 : strc[3]);
			if (out.length && sa > 0) {
				if (!ssrv || !svg_fill_server(ctx, out, false, ssrv, st, ctm, get_bbox(), sa))
					if (strc)
						fill_polys(out, false, [strc[0], strc[1], strc[2]], Math.min(1, st.opacity * st.strokeOpacity * strc[3]), ctx.clip);
			}
		}
	}
	/* paint-order: normal is fill then stroke; a stroke mentioned before fill (or on its own) goes first */
	var po = String(st.paintOrder || "normal").replace(/^\s+|\s+$/g, "").split(/\s+/);
	var strokefirst = false;
	for (i = 0; i < po.length; i++) {
		if (po[i] == "stroke") {
			strokefirst = true;
			break;
		}
		if (po[i] == "fill")
			break;
	}
	if (strokefirst) {
		paint_stroke();
		paint_fill();
	}
	else {
		paint_fill();
		paint_stroke();
	}
}

/* The inherited drawing state for an element, from its parent's state */
function svg_derive(ctx, el, parent)
{
	var p = svg_props(ctx, el);
	var st = {};
	var k;
	for (k in parent)
		st[k] = parent[k];
	function inherit(prop, key)
	{
		if (p[prop] !== undefined && p[prop] != "inherit")
			st[key] = p[prop];
	}
	inherit("color", "color");
	inherit("fill", "fill");
	inherit("stroke", "stroke");
	inherit("fill-rule", "fillRule");
	inherit("stroke-linecap", "cap");
	inherit("stroke-linejoin", "join");
	inherit("visibility", "visibility");
	if (p["font-size"] !== undefined)
		st.fontSize = svg_len(p["font-size"], parent.fontSize, parent.fontSize);
	inherit("font-family", "fontFamily");
	inherit("font-weight", "fontWeight");
	inherit("font-style", "fontStyle");
	inherit("text-anchor", "textAnchor");
	inherit("clip-rule", "clipRule");
	inherit("paint-order", "paintOrder");
	inherit("marker-start", "markerStart");
	inherit("marker-mid", "markerMid");
	inherit("marker-end", "markerEnd");
	if (p.marker !== undefined && p.marker != "inherit") {
		st.markerStart = st.markerMid = st.markerEnd = p.marker;
	}
	if (p["letter-spacing"] !== undefined)
		st.letterSpacing = p["letter-spacing"] == "normal" ? 0 : svg_len(p["letter-spacing"], parent.fontSize, 0);
	if (p["word-spacing"] !== undefined)
		st.wordSpacing = p["word-spacing"] == "normal" ? 0 : svg_len(p["word-spacing"], parent.fontSize, 0);
	if (p["text-decoration"] !== undefined && p["text-decoration"] != "inherit")
		st.textDecoration = p["text-decoration"];
	if (p["stroke-width"] !== undefined && p["stroke-width"] != "inherit")
		st.strokeWidth = svg_len(p["stroke-width"], ctx.diag, parent.strokeWidth);
	if (p["fill-opacity"] !== undefined)
		st.fillOpacity = Math.max(0, Math.min(1, parseFloat(p["fill-opacity"]) || 0));
	if (p["stroke-opacity"] !== undefined)
		st.strokeOpacity = Math.max(0, Math.min(1, parseFloat(p["stroke-opacity"]) || 0));
	if (p["stroke-miterlimit"] !== undefined)
		st.miter = Math.max(1, parseFloat(p["stroke-miterlimit"]) || 4);
	if (p["stroke-dasharray"] !== undefined) {
		st.dash = p["stroke-dasharray"] == "none" ? null : svg_numbers(p["stroke-dasharray"]);
	}
	if (p["stroke-dashoffset"] !== undefined)
		st.dashOffset = svg_len(p["stroke-dashoffset"], ctx.diag, 0);
	/* opacity doesn't inherit, but a group's opacity is applied to everything in it (an approximation) */
	if (p.opacity !== undefined && !SVG_CONTAINER[el.name] && ctx.noopac !== el) {
		var ov = parseFloat(p.opacity);
		st.opacity = parent.opacity * Math.max(0, Math.min(1, isNaN(ov) ? 1 : ov));
	}
	return st;
}

/* ---------- Bounding boxes ---------- */

/* Bounding box [x0, y0, x1, y1] of an element after matrix m (and, unless own is false, after the
 * element's own transform), or null if it draws nothing */
function svg_bbox(ctx, el, m, depth, own)
{
	if (depth > 14 || SVG_SKIP[el.name])
		return null;
	var mm = m;
	if (own !== false && el.attrs.transform)
		mm = mat_mul(svg_transform(el.attrs.transform), m);
	var box = null, i, j;
	function add(x, y)
	{
		if (!box)
			box = [x, y, x, y];
		else {
			if (x < box[0]) box[0] = x;
			if (x > box[2]) box[2] = x;
			if (y < box[1]) box[1] = y;
			if (y > box[3]) box[3] = y;
		}
	}
	function merge(b)
	{
		if (b) {
			add(b[0], b[1]);
			add(b[2], b[3]);
		}
	}
	if (SVG_SHAPES[el.name]) {
		var subs = svg_flatten(svg_shape_cmds(ctx, el, ctx.vw, ctx.vh), mm);
		for (i = 0; i < subs.length; i++)
			for (j = 0; j < subs[i].pts.length; j += 2)
				add(subs[i].pts[j], subs[i].pts[j + 1]);
	}
	else if (el.name == "image") {
		var ix = svg_len(el.attrs.x, ctx.vw, 0), iy = svg_len(el.attrs.y, ctx.vh, 0);
		var iw = svg_len(el.attrs.width, ctx.vw, 0), ih = svg_len(el.attrs.height, ctx.vh, 0);
		var cs = [[ix, iy], [ix + iw, iy], [ix + iw, iy + ih], [ix, iy + ih]];
		for (i = 0; i < 4; i++) {
			var pp = mat_apply(mm, cs[i][0], cs[i][1]);
			add(pp[0], pp[1]);
		}
	}
	else if (el.name == "text") {
		merge(svg_text_box(ctx, el, mm));
	}
	else if (el.name == "use") {
		var href = el.attrs.href;
		var target = href && href.charAt(0) == "#" ? ctx.ids[href.substr(1)] : null;
		if (target && ctx.usedepth < 6) {
			ctx.usedepth++;
			var um = mat_mul([1, 0, 0, 1, svg_len(el.attrs.x, ctx.vw, 0), svg_len(el.attrs.y, ctx.vh, 0)], mm);
			if (target.name == "symbol" || target.name == "svg") {
				for (i = 0; i < target.children.length; i++)
					merge(svg_bbox(ctx, target.children[i], um, depth + 1));
			}
			else
				merge(svg_bbox(ctx, target, um, depth + 1));
			ctx.usedepth--;
		}
	}
	else if (SVG_CONTAINER[el.name]) {
		for (i = 0; i < el.children.length; i++)
			merge(svg_bbox(ctx, el.children[i], mm, depth + 1));
	}
	return box;
}

/* ---------- Offscreen drawing ---------- */

/* Draw with fn() into a new buffer filled with the color bg (0 = black), and return that buffer.
 * The clip mask is off while it draws. */
function svg_offscreen(bg, fn)
{
	var saved = FB, savedMask = CLIPMASK;
	var buf = new Int32Array(FW * FH);
	if (bg) {
		for (var i = 0; i < buf.length; i++)
			buf[i] = bg;
	}
	FB = buf;
	CLIPMASK = null;
	try {
		fn();
	}
	finally {
		FB = saved;
		CLIPMASK = savedMask;
	}
	return buf;
}

/* The product of two masks (either may be null = everything) */
function svg_mask_mul(a, b)
{
	if (!a)
		return b;
	if (!b)
		return a;
	var n = FW * FH, r = new Uint8Array(n);
	for (var i = 0; i < n; i++) {
		if (a[i] && b[i])
			r[i] = (a[i] * b[i] + 127) / 255;
	}
	return r;
}

/* ---------- Clip paths and masks ---------- */

/* Coverage of a clipPath element as a mask (Uint8Array, 0-255 per pixel), or false if it clips
 * everything away.  m maps the user space of the clipped element to the device. */
function svg_clip_mask(ctx, cp, m, bboxfn)
{
	if (ctx.clipdepth > 4)
		return null;
	var cm = m;
	if (cp.attrs.transform)
		cm = mat_mul(svg_transform(cp.attrs.transform), cm);
	if (cp.attrs.clipPathUnits == "objectBoundingBox") {
		var bb = bboxfn();
		if (!bb || bb[2] <= bb[0] || bb[3] <= bb[1])
			return false;
		cm = mat_mul([bb[2] - bb[0], 0, 0, bb[3] - bb[1], bb[0], bb[1]], cm);
	}
	ctx.clipdepth++;
	var mask = new Uint8Array(FW * FH);
	var cov = svg_cov_buffer();
	var full = [0, 0, FW, FH];
	for (var ci = 0; ci < cp.children.length; ci++) {
		var c = cp.children[ci];
		if (SVG_SKIP[c.name])
			continue;
		var cprops = svg_props(ctx, c);
		if (cprops.display == "none")
			continue;
		var ccm = c.attrs.transform ? mat_mul(svg_transform(c.attrs.transform), cm) : cm;
		var shape = c;
		if (c.name == "use") {
			var href = c.attrs.href;
			shape = href && href.charAt(0) == "#" ? ctx.ids[href.substr(1)] : null;
			if (!shape)
				continue;
			ccm = mat_mul([1, 0, 0, 1, svg_len(c.attrs.x, ctx.vw, 0), svg_len(c.attrs.y, ctx.vh, 0)], ccm);
			if (shape.attrs.transform)
				ccm = mat_mul(svg_transform(shape.attrs.transform), ccm);
		}
		if (!SVG_SHAPES[shape.name])
			continue;
		var subs = svg_flatten(svg_shape_cmds(ctx, shape, ctx.vw, ctx.vh), ccm);
		var polys = [], i, j;
		for (i = 0; i < subs.length; i++)
			if (subs[i].pts.length >= 6)
				polys.push(subs[i].pts);
		if (!polys.length)
			continue;
		var rule = svg_props(ctx, shape)["clip-rule"] || cprops["clip-rule"];
		var box = svg_polys_box(polys, full);
		if (!box)
			continue;
		COVTARGET = cov;
		fill_polys(polys, rule == "evenodd", [0, 0, 0], 1, full);
		COVTARGET = null;
		var childref = cprops["clip-path"] && cprops["clip-path"] != "none" ? svg_ref(ctx, cprops["clip-path"]) : null;
		var childmask = null;
		if (childref && childref.name == "clipPath")
			childmask = svg_clip_mask(ctx, childref, ccm, function () { return svg_bbox(ctx, shape, [1, 0, 0, 1, 0, 0], 0, false); });
		for (i = box[1]; i < box[3]; i++) {
			var row = i * FW;
			for (j = box[0]; j < box[2]; j++) {
				var v = cov[row + j];
				if (!v)
					continue;
				cov[row + j] = 0;
				if (childmask)
					v = childmask === false ? 0 : (v * childmask[row + j] + 127) / 255;
				if (v > mask[row + j])
					mask[row + j] = v;
			}
		}
	}
	var own = cp.attrs["clip-path"] || svg_props(ctx, cp)["clip-path"];
	if (own && own != "none") {
		var oref = svg_ref(ctx, own);
		if (oref && oref.name == "clipPath") {
			var om = svg_clip_mask(ctx, oref, m, bboxfn);
			if (om === false) {
				ctx.clipdepth--;
				return false;
			}
			if (om)
				mask = svg_mask_mul(mask, om);
		}
	}
	ctx.clipdepth--;
	return mask;
}

/* Integer device box [x0, y0, x1, y1] of polygons, within clip, or null if there is nothing there */
function svg_polys_box(polys, clip)
{
	var minx = 1e30, miny = 1e30, maxx = -1e30, maxy = -1e30, i, j;
	for (i = 0; i < polys.length; i++) {
		var q = polys[i];
		for (j = 0; j < q.length; j += 2) {
			if (q[j] < minx) minx = q[j];
			if (q[j] > maxx) maxx = q[j];
			if (q[j + 1] < miny) miny = q[j + 1];
			if (q[j + 1] > maxy) maxy = q[j + 1];
		}
	}
	var x0 = Math.max(0, Math.floor(minx), Math.floor(clip[0])), y0 = Math.max(0, Math.floor(miny), Math.floor(clip[1]));
	var x1 = Math.min(FW, Math.ceil(maxx) + 1, Math.ceil(clip[2])), y1 = Math.min(FH, Math.ceil(maxy) + 1, Math.ceil(clip[3]));
	return x1 > x0 && y1 > y0 ? [x0, y0, x1, y1] : null;
}

/* A luminance mask from a mask element, as Uint8Array (0-255 per pixel), or false if nothing shows */
function svg_mask_array(ctx, mk, m, bboxfn)
{
	if (ctx.maskdepth > 3)
		return null;
	var obb = mk.attrs.maskUnits != "userSpaceOnUse";
	var bb = null;
	if (obb || mk.attrs.maskContentUnits == "objectBoundingBox") {
		bb = bboxfn();
		if (!bb || bb[2] <= bb[0] || bb[3] <= bb[1])
			return false;
	}
	var rx, ry, rw, rh;
	if (obb) {
		var bw = bb[2] - bb[0], bh = bb[3] - bb[1];
		rx = bb[0] + svg_unit(ctx, mk.attrs.x, -0.1, true, 1) * bw;
		ry = bb[1] + svg_unit(ctx, mk.attrs.y, -0.1, true, 1) * bh;
		rw = svg_unit(ctx, mk.attrs.width, 1.2, true, 1) * bw;
		rh = svg_unit(ctx, mk.attrs.height, 1.2, true, 1) * bh;
	}
	else {
		rx = svg_len(mk.attrs.x, ctx.vw, -0.1 * ctx.vw);
		ry = svg_len(mk.attrs.y, ctx.vh, -0.1 * ctx.vh);
		rw = svg_len(mk.attrs.width, ctx.vw, 1.2 * ctx.vw);
		rh = svg_len(mk.attrs.height, ctx.vh, 1.2 * ctx.vh);
	}
	var cs = [[rx, ry], [rx + rw, ry], [rx + rw, ry + rh], [rx, ry + rh]];
	var x0 = 1e30, y0 = 1e30, x1 = -1e30, y1 = -1e30, i;
	for (i = 0; i < 4; i++) {
		var pp = mat_apply(m, cs[i][0], cs[i][1]);
		x0 = Math.min(x0, pp[0]); x1 = Math.max(x1, pp[0]);
		y0 = Math.min(y0, pp[1]); y1 = Math.max(y1, pp[1]);
	}
	var region = [Math.max(0, Math.floor(x0), Math.floor(ctx.clip[0])), Math.max(0, Math.floor(y0), Math.floor(ctx.clip[1])),
		Math.min(FW, Math.ceil(x1), Math.ceil(ctx.clip[2])), Math.min(FH, Math.ceil(y1), Math.ceil(ctx.clip[3]))];
	if (region[2] <= region[0] || region[3] <= region[1])
		return false;
	var cm = m;
	if (mk.attrs.maskContentUnits == "objectBoundingBox")
		cm = mat_mul([bb[2] - bb[0], 0, 0, bb[3] - bb[1], bb[0], bb[1]], cm);
	var mst = svg_derive(ctx, mk, ctx.st0);
	var saved = ctx.clip;
	ctx.maskdepth++;
	var off;
	try {
		ctx.clip = region;
		off = svg_offscreen(0, function () {
			for (var k = 0; k < mk.children.length; k++)
				if (!svg_draw(ctx, mk.children[k], mst, cm, 1))
					break;
		});
	}
	finally {
		ctx.clip = saved;
		ctx.maskdepth--;
	}
	var mask = new Uint8Array(FW * FH), x, y, v;
	for (y = region[1]; y < region[3]; y++) {
		for (x = region[0]; x < region[2]; x++) {
			v = off[y * FW + x];
			if (v)
				mask[y * FW + x] = 0.2125 * ((v >> 16) & 255) + 0.7154 * ((v >> 8) & 255) + 0.0721 * (v & 255) + 0.5;
		}
	}
	return mask;
}

/* Blend the offscreen picture "work" (the element drawn over a copy of what was there before) into
 * FB with weight k (0-1) times the mask, which gives exactly an element drawn as a group with that
 * opacity and mask: result = before + k * (work - before) */
function svg_blend_group(work, k, mask)
{
	var n = FW * FH, back = FB, i;
	for (i = 0; i < n; i++) {
		var w = work[i], b = back[i];
		if (w === b)
			continue;
		var f = k;
		if (mask)
			f *= mask[i] * 0.00392157;
		if (f <= 0.002)
			continue;
		var br = (b >> 16) & 255, bg = (b >> 8) & 255, bb = b & 255;
		back[i] = ((br + (((w >> 16) & 255) - br) * f + 0.5) << 16) | ((bg + (((w >> 8) & 255) - bg) * f + 0.5) << 8) | ((bb + ((w & 255) - bb) * f + 0.5) | 0);
	}
}

/* Draw an element with its clip-path, mask, filter and (for groups) opacity.  Returns false if the
 * render was aborted. */
function svg_draw(ctx, el, parent, ctm, depth)
{
	if (depth > 40)
		return true;
	var name = el.name;
	if (SVG_SKIP[name])
		return true;
	var p = svg_props(ctx, el);
	if (p.display == "none")
		return true;
	var cpel = p["clip-path"] && p["clip-path"] != "none" ? svg_ref(ctx, p["clip-path"]) : null;
	if (cpel && cpel.name != "clipPath")
		cpel = null;
	var mkel = p.mask && p.mask != "none" ? svg_ref(ctx, p.mask) : null;
	if (mkel && mkel.name != "mask")
		mkel = null;
	var flel = p.filter && p.filter != "none" ? svg_ref(ctx, p.filter) : null;
	if (flel && flel.name != "filter")
		flel = null;
	var opac = 1, leafgroup = false;
	if (SVG_CONTAINER[name] && p.opacity !== undefined) {
		opac = parseFloat(p.opacity);
		opac = isNaN(opac) ? 1 : Math.max(0, Math.min(1, opac));
	}
	else if (p.opacity !== undefined && (SVG_SHAPES[name] || name == "text") && name != "line") {
		/* a shape with both a fill and a stroke has its opacity applied to the two together */
		var lo = parseFloat(p.opacity);
		if (!isNaN(lo) && lo < 1) {
			var lst = svg_derive(ctx, el, parent);
			if (lst.strokeWidth > 0 && (svg_color(ctx, lst.fill, lst, 0) || svg_server(ctx, lst.fill)) &&
				(svg_color(ctx, lst.stroke, lst, 0) || svg_server(ctx, lst.stroke))) {
				opac = Math.max(0, lo);
				leafgroup = true;
			}
		}
	}
	if (!cpel && !mkel && !flel && opac >= 1)
		return svg_draw_node(ctx, el, parent, ctm, depth);
	if (opac <= 0)
		return true;
	var m = el.attrs.transform ? mat_mul(svg_transform(el.attrs.transform), ctm) : ctm;
	var boxed = false, box = null;
	function bboxfn()
	{
		if (!boxed) {
			boxed = true;
			box = svg_bbox(ctx, el, [1, 0, 0, 1, 0, 0], 0, false);
		}
		return box;
	}
	var saved = CLIPMASK;
	var combined = saved;
	var ok = true;
	if (cpel) {
		var cm = svg_clip_mask(ctx, cpel, m, bboxfn);
		if (cm === false)
			return true;
		if (cm)
			combined = svg_mask_mul(combined, cm);
	}
	if (mkel) {
		var mm = svg_mask_array(ctx, mkel, m, bboxfn);
		if (mm === false)
			return true;
		if (mm)
			combined = svg_mask_mul(combined, mm);
	}
	if (!flel && opac >= 1) {		/* just clipped or masked: draw straight through the mask */
		CLIPMASK = combined;
		try {
			ok = svg_draw_node(ctx, el, parent, ctm, depth);
		}
		finally {
			CLIPMASK = saved;
		}
		return ok;
	}
	if (flel) {
		ok = svg_filter_draw(ctx, el, flel, parent, ctm, m, depth, combined, opac, bboxfn);
		return ok;
	}
	/* group opacity: draw the element over a copy of the picture, then blend between the two */
	var back = FB;
	var work = new Int32Array(back);
	FB = work;
	CLIPMASK = null;
	var savednoopac = ctx.noopac;
	if (leafgroup)
		ctx.noopac = el;
	try {
		ok = svg_draw_node(ctx, el, parent, ctm, depth);
	}
	finally {
		FB = back;
		CLIPMASK = saved;
		ctx.noopac = savednoopac;
	}
	svg_blend_group(work, opac, combined);
	return ok;
}

/* ---------- Filters ---------- */

/* A premultiplied RGBA picture of w x h pixels, one Float32Array (0-1) per channel */
function svg_fimg(w, h)
{
	var n = w * h;
	return { w: w, h: h, r: new Float32Array(n), g: new Float32Array(n), b: new Float32Array(n), a: new Float32Array(n) };
}

function svg_fimg_copy(s)
{
	var d = svg_fimg(s.w, s.h);
	d.r.set(s.r);
	d.g.set(s.g);
	d.b.set(s.b);
	d.a.set(s.a);
	return d;
}

/* One box-blur pass over a channel along rows (stride 1) or columns (stride w): each output is the mean
 * of the window [i - off, i - off + size - 1] along the line, with zero outside */
function svg_box_pass(src, dst, w, h, vertical, size, off)
{
	var len = vertical ? h : w, lines = vertical ? w : h, step = vertical ? w : 1, ls = vertical ? 1 : w;
	var inv = 1 / size;
	/* a new pixel enters the window while i < add_end, and an old one leaves from i = sub_start on */
	var add_end = Math.max(0, Math.min(len, len - size + off)), sub_start = Math.min(len, off);
	var first = Math.min(len - 1, size - 1 - off);
	for (var l = 0; l < lines; l++) {
		var base = l * ls, sum = 0, i, o, ia, is;
		for (i = 0, ia = base; i <= first; i++, ia += step)
			sum += src[ia];
		o = base;
		ia = base + (size - off) * step;
		is = base - off * step;
		i = 0;
		var b1 = Math.min(add_end, sub_start), b2 = Math.max(add_end, sub_start);
		/* until the first of the two things starts to apply */
		if (add_end <= sub_start) {
			for (; i < add_end; i++, o += step, ia += step, is += step) {
				dst[o] = sum * inv;
				sum += src[ia];
			}
			for (; i < sub_start; i++, o += step, ia += step, is += step)
				dst[o] = sum * inv;
			for (; i < len; i++, o += step, ia += step, is += step) {
				dst[o] = sum * inv;
				sum -= src[is];
			}
		}
		else {
			for (; i < sub_start; i++, o += step, ia += step, is += step) {
				dst[o] = sum * inv;
				sum += src[ia];
			}
			for (; i < add_end; i++, o += step, ia += step, is += step) {
				dst[o] = sum * inv;
				sum += src[ia] - src[is];
			}
			for (; i < len; i++, o += step, ia += step, is += step) {
				dst[o] = sum * inv;
				sum -= src[is];
			}
		}
	}
}

/* Gaussian blur of one channel by three box blurs (the way the SVG specification describes it) */
function svg_blur_channel(ch, w, h, sigx, sigy)
{
	var tmp = new Float32Array(ch.length);
	for (var axis = 0; axis < 2; axis++) {
		var sg = axis ? sigy : sigx;
		if (sg <= 0.05)
			continue;
		if (sg > 150)
			sg = 150;
		var d = Math.floor(sg * 3 * Math.sqrt(2 * Math.PI) / 4 + 0.5);
		if (d < 1)
			d = 1;
		var passes;
		if (d % 2)
			passes = [[d, (d - 1) / 2], [d, (d - 1) / 2], [d, (d - 1) / 2]];
		else
			passes = [[d, d / 2], [d, d / 2 - 1], [d + 1, d / 2]];
		for (var k = 0; k < 3; k++) {
			svg_box_pass(ch, tmp, w, h, axis == 1, passes[k][0], passes[k][1]);
			ch.set(tmp);
		}
	}
}

function svg_chan_used(ch)
{
	for (var i = 0, n = ch.length; i < n; i++)
		if (ch[i] !== 0)
			return true;
	return false;
}

function svg_fimg_blur(img, sigx, sigy)
{
	var o = svg_fimg_copy(img);
	if (svg_chan_used(o.r))
		svg_blur_channel(o.r, o.w, o.h, sigx, sigy);
	if (svg_chan_used(o.g))
		svg_blur_channel(o.g, o.w, o.h, sigx, sigy);
	if (svg_chan_used(o.b))
		svg_blur_channel(o.b, o.w, o.h, sigx, sigy);
	svg_blur_channel(o.a, o.w, o.h, sigx, sigy);
	return o;
}

/* Move a picture by whole pixels */
function svg_fimg_offset(img, dx, dy)
{
	var o = svg_fimg(img.w, img.h), w = img.w, h = img.h;
	dx = Math.round(dx);
	dy = Math.round(dy);
	for (var y = 0; y < h; y++) {
		var sy = y - dy;
		if (sy < 0 || sy >= h)
			continue;
		for (var x = Math.max(0, dx); x < Math.min(w, w + dx); x++) {
			var si = sy * w + x - dx, di = y * w + x;
			o.r[di] = img.r[si];
			o.g[di] = img.g[si];
			o.b[di] = img.b[si];
			o.a[di] = img.a[si];
		}
	}
	return o;
}

var SVG_TO_LIN = null, SVG_FROM_LIN = null;

function svg_lin_tables()
{
	if (SVG_TO_LIN)
		return;
	SVG_TO_LIN = new Float32Array(1024);
	SVG_FROM_LIN = new Float32Array(1024);
	for (var i = 0; i < 1024; i++) {
		var c = i / 1023;
		SVG_TO_LIN[i] = c <= 0.04045 ? c / 12.92 : Math.pow((c + 0.055) / 1.055, 2.4);
		SVG_FROM_LIN[i] = c <= 0.0031308 ? c * 12.92 : 1.055 * Math.pow(c, 1 / 2.4) - 0.055;
	}
}

/* Apply fn(rgba) to each pixel's un-premultiplied color (and alpha), optionally in linear light.
 * fn gets and returns an array [r, g, b, a] of 0-1 values. */
function svg_fimg_map(img, linear, fn)
{
	svg_lin_tables();
	var o = svg_fimg(img.w, img.h), n = img.w * img.h, v = [0, 0, 0, 0];
	for (var i = 0; i < n; i++) {
		var a = img.a[i], r = 0, g = 0, b = 0;
		if (a > 0.0001) {
			r = img.r[i] / a;
			g = img.g[i] / a;
			b = img.b[i] / a;
			r = r > 1 ? 1 : r;
			g = g > 1 ? 1 : g;
			b = b > 1 ? 1 : b;
			if (linear) {
				r = SVG_TO_LIN[(r * 1023 + 0.5) | 0];
				g = SVG_TO_LIN[(g * 1023 + 0.5) | 0];
				b = SVG_TO_LIN[(b * 1023 + 0.5) | 0];
			}
		}
		v = fn(r, g, b, a);
		r = v[0] < 0 ? 0 : (v[0] > 1 ? 1 : v[0]);
		g = v[1] < 0 ? 0 : (v[1] > 1 ? 1 : v[1]);
		b = v[2] < 0 ? 0 : (v[2] > 1 ? 1 : v[2]);
		a = v[3] < 0 ? 0 : (v[3] > 1 ? 1 : v[3]);
		if (linear) {
			r = SVG_FROM_LIN[(r * 1023 + 0.5) | 0];
			g = SVG_FROM_LIN[(g * 1023 + 0.5) | 0];
			b = SVG_FROM_LIN[(b * 1023 + 0.5) | 0];
		}
		o.r[i] = r * a;
		o.g[i] = g * a;
		o.b[i] = b * a;
		o.a[i] = a;
	}
	return o;
}

/* Combine two pictures pixel by pixel: fn(ar, ag, ab, aa, br, bg, bb, ba) returns [r, g, b, a] (premultiplied) */
function svg_fimg_combine(A, B, fn)
{
	var o = svg_fimg(A.w, A.h), n = A.w * A.h;
	for (var i = 0; i < n; i++) {
		var v = fn(A.r[i], A.g[i], A.b[i], A.a[i], B.r[i], B.g[i], B.b[i], B.a[i]);
		o.a[i] = v[3] < 0 ? 0 : (v[3] > 1 ? 1 : v[3]);
		o.r[i] = Math.max(0, Math.min(o.a[i], v[0]));
		o.g[i] = Math.max(0, Math.min(o.a[i], v[1]));
		o.b[i] = Math.max(0, Math.min(o.a[i], v[2]));
	}
	return o;
}

function svg_fimg_over(A, B)		/* A over B */
{
	var o = svg_fimg(A.w, A.h), n = A.w * A.h;
	for (var i = 0; i < n; i++) {
		var k = 1 - A.a[i];
		o.r[i] = A.r[i] + B.r[i] * k;
		o.g[i] = A.g[i] + B.g[i] * k;
		o.b[i] = A.b[i] + B.b[i] * k;
		o.a[i] = A.a[i] + B.a[i] * k;
	}
	return o;
}

/* Porter-Duff operators "in", "out", "atop" and "xor" of A with B (premultiplied) */
function svg_fimg_pd(A, B, op)
{
	var o = svg_fimg(A.w, A.h), n = A.w * A.h, i, ba, aa;
	var ar = A.r, ag = A.g, ab = A.b, aal = A.a, br = B.r, bg = B.g, bb = B.b, bal = B.a;
	var or = o.r, og = o.g, ob = o.b, oa = o.a;
	if (op == "in") {
		for (i = 0; i < n; i++) {
			ba = bal[i];
			or[i] = ar[i] * ba;
			og[i] = ag[i] * ba;
			ob[i] = ab[i] * ba;
			oa[i] = aal[i] * ba;
		}
	}
	else if (op == "out") {
		for (i = 0; i < n; i++) {
			ba = 1 - bal[i];
			or[i] = ar[i] * ba;
			og[i] = ag[i] * ba;
			ob[i] = ab[i] * ba;
			oa[i] = aal[i] * ba;
		}
	}
	else if (op == "atop") {
		for (i = 0; i < n; i++) {
			ba = bal[i];
			aa = 1 - aal[i];
			or[i] = ar[i] * ba + br[i] * aa;
			og[i] = ag[i] * ba + bg[i] * aa;
			ob[i] = ab[i] * ba + bb[i] * aa;
			oa[i] = ba;
		}
	}
	else {		/* xor */
		for (i = 0; i < n; i++) {
			ba = bal[i];
			aa = aal[i];
			or[i] = ar[i] * (1 - ba) + br[i] * (1 - aa);
			og[i] = ag[i] * (1 - ba) + bg[i] * (1 - aa);
			ob[i] = ab[i] * (1 - ba) + bb[i] * (1 - aa);
			oa[i] = aa * (1 - ba) + ba * (1 - aa);
		}
	}
	return o;
}

/* feBlend of A over B (premultiplied) in a given mode */
function svg_fimg_blend(A, B, mode)
{
	if (mode == "normal" || !/^(multiply|screen|darken|lighten)$/.test(mode))
		return svg_fimg_over(A, B);
	var o = svg_fimg(A.w, A.h), n = A.w * A.h, i, aa, ba;
	var ch = ["r", "g", "b"], c, x, y, p, q;
	for (i = 0; i < n; i++) {
		aa = A.a[i];
		ba = B.a[i];
		o.a[i] = 1 - (1 - aa) * (1 - ba);
	}
	for (c = 0; c < 3; c++) {
		var ac = A[ch[c]], bc = B[ch[c]], oc = o[ch[c]];
		for (i = 0; i < n; i++) {
			x = ac[i];
			y = bc[i];
			aa = A.a[i];
			ba = B.a[i];
			if (mode == "multiply")
				q = (1 - aa) * y + (1 - ba) * x + x * y;
			else if (mode == "screen")
				q = y + x - x * y;
			else {
				p = (1 - aa) * y + x;
				q = (1 - ba) * x + y;
				q = mode == "darken" ? (p < q ? p : q) : (p > q ? p : q);
			}
			oc[i] = q < 0 ? 0 : (q > o.a[i] ? o.a[i] : q);
		}
	}
	return o;
}

/* feColorMatrix with a 4x5 matrix (20 numbers), on un-premultiplied color, optionally in linear light */
function svg_fimg_cmatrix(img, m, linear)
{
	svg_lin_tables();
	var o = svg_fimg(img.w, img.h), n = img.w * img.h, i;
	var ir = img.r, ig = img.g, ib = img.b, ia = img.a, or = o.r, og = o.g, ob = o.b, oa = o.a;
	var m0 = m[0], m1 = m[1], m2 = m[2], m3 = m[3], m4 = m[4], m5 = m[5], m6 = m[6], m7 = m[7], m8 = m[8], m9 = m[9];
	var m10 = m[10], m11 = m[11], m12 = m[12], m13 = m[13], m14 = m[14], m15 = m[15], m16 = m[16], m17 = m[17], m18 = m[18], m19 = m[19];
	for (i = 0; i < n; i++) {
		var a = ia[i], r = 0, g = 0, b = 0;
		if (a > 0.0001) {
			r = ir[i] / a;
			g = ig[i] / a;
			b = ib[i] / a;
			r = r > 1 ? 1 : r;
			g = g > 1 ? 1 : g;
			b = b > 1 ? 1 : b;
			if (linear) {
				r = SVG_TO_LIN[(r * 1023 + 0.5) | 0];
				g = SVG_TO_LIN[(g * 1023 + 0.5) | 0];
				b = SVG_TO_LIN[(b * 1023 + 0.5) | 0];
			}
		}
		var nr = m0 * r + m1 * g + m2 * b + m3 * a + m4;
		var ng = m5 * r + m6 * g + m7 * b + m8 * a + m9;
		var nb = m10 * r + m11 * g + m12 * b + m13 * a + m14;
		var na = m15 * r + m16 * g + m17 * b + m18 * a + m19;
		nr = nr < 0 ? 0 : (nr > 1 ? 1 : nr);
		ng = ng < 0 ? 0 : (ng > 1 ? 1 : ng);
		nb = nb < 0 ? 0 : (nb > 1 ? 1 : nb);
		na = na < 0 ? 0 : (na > 1 ? 1 : na);
		if (linear) {
			nr = SVG_FROM_LIN[(nr * 1023 + 0.5) | 0];
			ng = SVG_FROM_LIN[(ng * 1023 + 0.5) | 0];
			nb = SVG_FROM_LIN[(nb * 1023 + 0.5) | 0];
		}
		or[i] = nr * na;
		og[i] = ng * na;
		ob[i] = nb * na;
		oa[i] = na;
	}
	return o;
}

/* The filter's color-interpolation-filters setting: linear light unless it says sRGB */
function svg_filter_linear(ctx, prim, flel)
{
	var v = svg_props(ctx, prim)["color-interpolation-filters"] || prim.attrs["color-interpolation-filters"] ||
		flel.attrs["color-interpolation-filters"] || svg_props(ctx, flel)["color-interpolation-filters"];
	return String(v || "linearRGB") != "sRGB";
}

/* Run a filter on an element and blend the result into the picture.  mask and opac are the clip, mask
 * and opacity that apply to the filtered result. */
function svg_filter_draw(ctx, el, flel, parent, ctm, m, depth, mask, opac, bboxfn)
{
	var chain = svg_href_chain(ctx, flel);
	var fu = svg_chain_attr(chain, "filterUnits") != "userSpaceOnUse";
	var pu = svg_chain_attr(chain, "primitiveUnits") == "objectBoundingBox";
	var bb = bboxfn();
	if ((fu || pu) && (!bb || bb[2] <= bb[0] || bb[3] <= bb[1]))
		return true;
	var bw = bb ? bb[2] - bb[0] : 0, bh = bb ? bb[3] - bb[1] : 0;
	function fattr(n)
	{
		return svg_chain_attr(chain, n);
	}
	var fx, fy, fw, fh;
	if (fu) {
		fx = bb[0] + svg_unit(ctx, fattr("x"), -0.1, true, 1) * bw;
		fy = bb[1] + svg_unit(ctx, fattr("y"), -0.1, true, 1) * bh;
		fw = svg_unit(ctx, fattr("width"), 1.2, true, 1) * bw;
		fh = svg_unit(ctx, fattr("height"), 1.2, true, 1) * bh;
	}
	else {
		fx = svg_len(fattr("x"), ctx.vw, -0.1 * ctx.vw);
		fy = svg_len(fattr("y"), ctx.vh, -0.1 * ctx.vh);
		fw = svg_len(fattr("width"), ctx.vw, 1.2 * ctx.vw);
		fh = svg_len(fattr("height"), ctx.vh, 1.2 * ctx.vh);
	}
	function dev_rect(x, y, w, h)
	{
		var cs = [[x, y], [x + w, y], [x + w, y + h], [x, y + h]], x0 = 1e30, y0 = 1e30, x1 = -1e30, y1 = -1e30;
		for (var i = 0; i < 4; i++) {
			var pp = mat_apply(m, cs[i][0], cs[i][1]);
			x0 = Math.min(x0, pp[0]); x1 = Math.max(x1, pp[0]);
			y0 = Math.min(y0, pp[1]); y1 = Math.max(y1, pp[1]);
		}
		return [Math.floor(x0), Math.floor(y0), Math.ceil(x1), Math.ceil(y1)];
	}
	var reg = dev_rect(fx, fy, fw, fh);
	reg = [Math.max(0, reg[0], Math.floor(ctx.clip[0])), Math.max(0, reg[1], Math.floor(ctx.clip[1])),
		Math.min(FW, reg[2], Math.ceil(ctx.clip[2])), Math.min(FH, reg[3], Math.ceil(ctx.clip[3]))];
	if (reg[2] <= reg[0] || reg[3] <= reg[1])
		return true;
	var scx = Math.sqrt(m[0] * m[0] + m[1] * m[1]), scy = Math.sqrt(m[2] * m[2] + m[3] * m[3]);
	var prims = [];
	for (var pi = 0; pi < chain.length && !prims.length; pi++)
		for (var ki = 0; ki < chain[pi].children.length; ki++)
			if (/^fe[A-Z]/.test(chain[pi].children[ki].name))
				prims.push(chain[pi].children[ki]);
	if (!prims.length)
		return true;
	/* an upper bound for how far the filters can spread the picture, to keep the work small */
	var margin = 2, k, pr, needfull = false;
	function numpair(str, dflt)
	{
		var a = svg_numbers(str === undefined ? "" : str);
		return a.length == 0 ? [dflt, dflt] : (a.length == 1 ? [a[0], a[0]] : [a[0], a[1]]);
	}
	function unitx(v) { return pu ? v * bw : v; }
	function unity(v) { return pu ? v * bh : v; }
	for (k = 0; k < prims.length; k++) {
		pr = prims[k];
		if (pr.name == "feGaussianBlur" || pr.name == "feDropShadow") {
			var sd = numpair(pr.attrs.stdDeviation, 0);
			margin += 3 * Math.max(unitx(sd[0]) * scx, unity(sd[1]) * scy);
		}
		if (pr.name == "feOffset" || pr.name == "feDropShadow")
			margin += Math.abs(unitx(parseFloat(pr.attrs.dx) || 0) * scx) + Math.abs(unity(parseFloat(pr.attrs.dy) || 0) * scy);
		if (pr.name == "feMorphology") {
			var rd = numpair(pr.attrs.radius, 0);
			margin += Math.max(rd[0] * scx, rd[1] * scy);
		}
		/* these can produce output where the element drew nothing, so the whole filter region is needed */
		if (pr.name == "feFlood" || pr.name == "feImage" || pr.name == "feTurbulence" || pr.name == "feTile" ||
			(pr.name == "feComposite" && pr.attrs.operator == "arithmetic") || pr.name == "feColorMatrix" ||
			pr.name == "feComponentTransfer" || pr.name == "feBlend")
			needfull = true;
	}
	/* render the element over black and over white to recover its transparency */
	var saved = ctx.clip;
	var black, white;
	try {
		ctx.clip = reg;
		black = svg_offscreen(0x000000, function () { svg_draw_node(ctx, el, parent, ctm, depth); });
		white = svg_offscreen(0xffffff, function () { svg_draw_node(ctx, el, parent, ctm, depth); });
	}
	finally {
		ctx.clip = saved;
	}
	/* the area that actually has something in it, plus the margin */
	var cx0 = reg[2], cy0 = reg[3], cx1 = reg[0], cy1 = reg[1], x, y;
	for (y = reg[1]; y < reg[3]; y++) {
		for (x = reg[0]; x < reg[2]; x++) {
			if (black[y * FW + x] !== 0 || white[y * FW + x] !== 0xffffff) {
				if (x < cx0) cx0 = x;
				if (x > cx1) cx1 = x;
				if (y < cy0) cy0 = y;
				if (y > cy1) cy1 = y;
			}
		}
	}
	var area;
	if (cx1 < cx0 || needfull) {
		area = reg;
	}
	else {
		area = [Math.max(reg[0], Math.floor(cx0 - margin)), Math.max(reg[1], Math.floor(cy0 - margin)),
			Math.min(reg[2], Math.ceil(cx1 + 1 + margin)), Math.min(reg[3], Math.ceil(cy1 + 1 + margin))];
	}
	var W = area[2] - area[0], H = area[3] - area[1];
	if (W <= 0 || H <= 0)
		return true;
	var src = svg_fimg(W, H);
	for (y = 0; y < H; y++) {
		for (x = 0; x < W; x++) {
			var bi = (area[1] + y) * FW + area[0] + x, i = y * W + x;
			var bv = black[bi], wv = white[bi];
			var br = (bv >> 16) & 255, bg = (bv >> 8) & 255, bbb = bv & 255;
			var wr = (wv >> 16) & 255, wg = (wv >> 8) & 255, wbb = wv & 255;
			var al = 1 - ((wr - br) + (wg - bg) + (wbb - bbb)) / 765;
			if (al < 0.002)
				continue;
			if (al > 1)
				al = 1;
			src.a[i] = al;
			src.r[i] = Math.min(al, br / 255);
			src.g[i] = Math.min(al, bg / 255);
			src.b[i] = Math.min(al, bbb / 255);
		}
	}
	black = white = null;
	var results = {}, last = src;
	var unsupported = false;
	var flood_defaults = { r: 0, g: 0, b: 0, a: 0 };
	function input(name)
	{
		if (name === undefined || name === "")
			return last;
		if (name == "SourceGraphic")
			return src;
		if (name == "SourceAlpha") {
			var sa = svg_fimg(W, H);
			sa.a.set(src.a);
			return sa;
		}
		if (results[name])
			return results[name];
		if (name == "BackgroundImage" || name == "BackgroundAlpha" || name == "FillPaint" || name == "StrokePaint")
			return svg_fimg(W, H);
		return last;
	}
	/* a primitive's own subregion, in the working area's pixels, or null for all of it */
	function subregion(pr)
	{
		var a = pr.attrs;
		if (a.x === undefined && a.y === undefined && a.width === undefined && a.height === undefined)
			return null;
		var sx = a.x !== undefined ? (pu ? bb[0] + svg_unit(ctx, a.x, 0, true, 1) * bw : svg_len(a.x, ctx.vw, fx)) : fx;
		var sy = a.y !== undefined ? (pu ? bb[1] + svg_unit(ctx, a.y, 0, true, 1) * bh : svg_len(a.y, ctx.vh, fy)) : fy;
		var sw = a.width !== undefined ? (pu ? svg_unit(ctx, a.width, 1, true, 1) * bw : svg_len(a.width, ctx.vw, fw)) : fw - (sx - fx);
		var sh = a.height !== undefined ? (pu ? svg_unit(ctx, a.height, 1, true, 1) * bh : svg_len(a.height, ctx.vh, fh)) : fh - (sy - fy);
		var r = dev_rect(sx, sy, sw, sh);
		return [r[0] - area[0], r[1] - area[1], r[2] - area[0], r[3] - area[1]];
	}
	function flood_color(pr)
	{
		var p = svg_props(ctx, pr);
		var fst = svg_derive(ctx, pr, ctx.st0);
		var c = svg_color(ctx, p["flood-color"] !== undefined ? p["flood-color"] : "black", fst, 0) || [0, 0, 0, 1];
		var fo = p["flood-opacity"] !== undefined ? parseFloat(p["flood-opacity"]) : 1;
		return [c[0] / 255, c[1] / 255, c[2] / 255, c[3] * (isNaN(fo) ? 1 : Math.max(0, Math.min(1, fo)))];
	}
	function filled(color, sr)
	{
		var o = svg_fimg(W, H);
		var x0 = sr ? Math.max(0, sr[0]) : 0, y0 = sr ? Math.max(0, sr[1]) : 0, x1 = sr ? Math.min(W, sr[2]) : W, y1 = sr ? Math.min(H, sr[3]) : H;
		for (var yy = y0; yy < y1; yy++)
			for (var xx = x0; xx < x1; xx++) {
				var ii = yy * W + xx;
				o.a[ii] = color[3];
				o.r[ii] = color[0] * color[3];
				o.g[ii] = color[1] * color[3];
				o.b[ii] = color[2] * color[3];
			}
		return o;
	}
	function nums(str, dflt)
	{
		var a = svg_numbers(str === undefined ? "" : str);
		return a.length ? a : dflt;
	}
	function shadow(img, pr)
	{
		var sd = numpair(pr.attrs.stdDeviation, 2);
		var dxu = unitx(parseFloat(pr.attrs.dx !== undefined ? pr.attrs.dx : 2) || 0), dyu = unity(parseFloat(pr.attrs.dy !== undefined ? pr.attrs.dy : 2) || 0);
		var al = svg_fimg(W, H);
		al.a.set(img.a);
		svg_blur_channel(al.a, W, H, unitx(sd[0]) * scx, unity(sd[1]) * scy);
		var fc = flood_color(pr);
		var sh = svg_fimg(W, H);
		for (var i = 0; i < al.a.length; i++) {
			sh.a[i] = al.a[i] * fc[3];
			sh.r[i] = fc[0] * sh.a[i];
			sh.g[i] = fc[1] * sh.a[i];
			sh.b[i] = fc[2] * sh.a[i];
		}
		return svg_fimg_offset(sh, m[0] * dxu + m[2] * dyu, m[1] * dxu + m[3] * dyu);
	}
	for (k = 0; k < prims.length; k++) {
		pr = prims[k];
		var out = null, lin = svg_filter_linear(ctx, pr, flel), a2 = pr.attrs;
		switch (pr.name) {
			case "feGaussianBlur": {
				var sdv = numpair(a2.stdDeviation, 0);
				out = svg_fimg_blur(input(a2["in"]), unitx(sdv[0]) * scx, unity(sdv[1]) * scy);
				break;
			}
			case "feOffset": {
				var odx = unitx(parseFloat(a2.dx) || 0), ody = unity(parseFloat(a2.dy) || 0);
				out = svg_fimg_offset(input(a2["in"]), m[0] * odx + m[2] * ody, m[1] * odx + m[3] * ody);
				break;
			}
			case "feFlood":
				out = filled(flood_color(pr), null);
				break;
			case "feDropShadow": {
				var dsrc = input(a2["in"]);
				out = svg_fimg_over(dsrc, shadow(dsrc, pr));
				break;
			}
			case "feMerge": {
				out = svg_fimg(W, H);
				for (var mi = 0; mi < pr.children.length; mi++)
					if (pr.children[mi].name == "feMergeNode")
						out = svg_fimg_over(input(pr.children[mi].attrs["in"]), out);
				break;
			}
			case "feComposite": {
				var ca = input(a2["in"]), cb = input(a2.in2), op = a2.operator || "over";
				if (op == "over")
					out = svg_fimg_over(ca, cb);
				else if (op == "in" || op == "out" || op == "atop" || op == "xor")
					out = svg_fimg_pd(ca, cb, op);
				else {
					var k1 = parseFloat(a2.k1) || 0, k2 = parseFloat(a2.k2) || 0, k3 = parseFloat(a2.k3) || 0, k4 = parseFloat(a2.k4) || 0;
					out = svg_fimg_combine(ca, cb, function (ar, ag, ab, aa, br, bg, bb2, ba) {
						return [k1 * ar * br + k2 * ar + k3 * br + k4, k1 * ag * bg + k2 * ag + k3 * bg + k4,
							k1 * ab * bb2 + k2 * ab + k3 * bb2 + k4, k1 * aa * ba + k2 * aa + k3 * ba + k4];
					});
				}
				break;
			}
			case "feBlend": {
				out = svg_fimg_blend(input(a2["in"]), input(a2.in2), a2.mode || "normal");
				break;
			}
			case "feColorMatrix": {
				var type = a2.type || "matrix", vals = svg_numbers(a2.values === undefined ? "" : a2.values), mat;
				if (type == "saturate") {
					var sv = vals.length ? vals[0] : 1;
					mat = [0.213 + 0.787 * sv, 0.715 - 0.715 * sv, 0.072 - 0.072 * sv, 0, 0, 0.213 - 0.213 * sv, 0.715 + 0.285 * sv, 0.072 - 0.072 * sv, 0, 0,
						0.213 - 0.213 * sv, 0.715 - 0.715 * sv, 0.072 + 0.928 * sv, 0, 0, 0, 0, 0, 1, 0];
				}
				else if (type == "hueRotate") {
					var an = (vals.length ? vals[0] : 0) * Math.PI / 180, co = Math.cos(an), si = Math.sin(an);
					mat = [0.213 + co * 0.787 - si * 0.213, 0.715 - co * 0.715 - si * 0.715, 0.072 - co * 0.072 + si * 0.928, 0, 0,
						0.213 - co * 0.213 + si * 0.143, 0.715 + co * 0.285 + si * 0.140, 0.072 - co * 0.072 - si * 0.283, 0, 0,
						0.213 - co * 0.213 - si * 0.787, 0.715 - co * 0.715 + si * 0.715, 0.072 + co * 0.928 + si * 0.072, 0, 0, 0, 0, 0, 1, 0];
				}
				else if (type == "luminanceToAlpha")
					mat = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0.2125, 0.7154, 0.0721, 0, 0];
				else
					mat = vals.length >= 20 ? vals : [1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0];
				out = svg_fimg_cmatrix(input(a2["in"]), mat, lin);
				break;
			}
			case "feComponentTransfer": {
				var funcs = { R: null, G: null, B: null, A: null };
				for (var fi = 0; fi < pr.children.length; fi++) {
					var fm = /^feFunc([RGBA])$/.exec(pr.children[fi].name);
					if (fm)
						funcs[fm[1]] = pr.children[fi].attrs;
				}
				var apply = function (f, v) {
					if (!f)
						return v;
					var t = f.type || "identity", tv = svg_numbers(f.tableValues === undefined ? "" : f.tableValues), n2 = tv.length;
					if (t == "table" && n2) {
						if (n2 == 1) return tv[0];
						var pos = v * (n2 - 1), i0 = Math.min(n2 - 2, Math.floor(pos));
						return tv[i0] + (tv[i0 + 1] - tv[i0]) * (pos - i0);
					}
					if (t == "discrete" && n2)
						return tv[Math.min(n2 - 1, Math.floor(v * n2))];
					if (t == "linear")
						return (f.slope !== undefined ? parseFloat(f.slope) : 1) * v + (f.intercept !== undefined ? parseFloat(f.intercept) : 0);
					if (t == "gamma")
						return (f.amplitude !== undefined ? parseFloat(f.amplitude) : 1) * Math.pow(v, f.exponent !== undefined ? parseFloat(f.exponent) : 1) + (f.offset !== undefined ? parseFloat(f.offset) : 0);
					return v;
				};
				out = svg_fimg_map(input(a2["in"]), lin, function (r, g, b, a) {
					return [apply(funcs.R, r), apply(funcs.G, g), apply(funcs.B, b), apply(funcs.A, a)];
				});
				break;
			}
			case "feMorphology": {
				var mr = numpair(a2.radius, 0), erode = a2.operator == "erode";
				var rx2 = Math.round(mr[0] * scx), ry2 = Math.round(mr[1] * scy);
				out = svg_fimg_copy(input(a2["in"]));
				var chs = [out.r, out.g, out.b, out.a];
				for (var ch2 = 0; ch2 < 4; ch2++) {
					var tmp2 = new Float32Array(chs[ch2].length);
					for (var ax = 0; ax < 2; ax++) {
						var rr = ax ? ry2 : rx2;
						if (rr <= 0)
							continue;
						var len = ax ? H : W, lines = ax ? W : H, stp = ax ? W : 1, lst = ax ? 1 : W;
						for (var ln = 0; ln < lines; ln++)
							for (var ii2 = 0; ii2 < len; ii2++) {
								var best = erode ? 1e9 : -1e9;
								for (var wi = Math.max(0, ii2 - rr); wi <= Math.min(len - 1, ii2 + rr); wi++) {
									var vv = chs[ch2][ln * lst + wi * stp];
									best = erode ? Math.min(best, vv) : Math.max(best, vv);
								}
								if (erode && (ii2 - rr < 0 || ii2 + rr > len - 1))
									best = 0;
								tmp2[ln * lst + ii2 * stp] = best;
							}
						chs[ch2].set(tmp2);
					}
				}
				break;
			}
			case "feTurbulence":
			case "feImage":
				unsupported = true;
				out = svg_fimg(W, H);
				break;
			default:
				unsupported = true;
				out = svg_fimg_copy(input(a2["in"]));
		}
		var sr = subregion(pr);
		if (sr) {		/* nothing outside the primitive's own region */
			for (y = 0; y < H; y++)
				for (x = 0; x < W; x++)
					if (x < sr[0] || x >= sr[2] || y < sr[1] || y >= sr[3]) {
						var oi = y * W + x;
						out.r[oi] = out.g[oi] = out.b[oi] = out.a[oi] = 0;
					}
		}
		last = out;
		if (a2.result)
			results[a2.result] = out;
	}
	if (unsupported)
		ctx.notes.filter = true;
	/* blend the result into the picture */
	for (y = 0; y < H; y++) {
		for (x = 0; x < W; x++) {
			var ri = y * W + x, ra = last.a[ri];
			if (ra <= 0.001 && last.r[ri] <= 0.001 && last.g[ri] <= 0.001 && last.b[ri] <= 0.001)
				continue;
			var di = (area[1] + y) * FW + area[0] + x;
			var kk = opac;
			if (mask)
				kk *= mask[di] * 0.00392157;
			if (kk <= 0.002)
				continue;
			var dv = FB[di], dr = (dv >> 16) & 255, dg = (dv >> 8) & 255, db = dv & 255;
			var nr = last.r[ri] * 255 * kk + dr * (1 - ra * kk), ng = last.g[ri] * 255 * kk + dg * (1 - ra * kk), nb = last.b[ri] * 255 * kk + db * (1 - ra * kk);
			FB[di] = ((nr > 255 ? 255 : nr + 0.5) << 16) | ((ng > 255 ? 255 : ng + 0.5) << 8) | ((nb > 255 ? 255 : nb + 0.5) | 0);
		}
	}
	return true;
}

/* ---------- Text ---------- */

/* A list of lengths such as x="10 20 30" */
function svg_len_list(str, ref)
{
	var out = [];
	var a = String(str).replace(/^\s+|\s+$/g, "").split(/[\s,]+/);
	for (var i = 0; i < a.length; i++)
		if (a[i].length)
			out.push(svg_len(a[i], ref, 0));
	return out;
}

/* 0 = sans, 1 = serif, 2 = monospace, from a font-family list */
function svg_font_family(st)
{
	var list = String(st.fontFamily || "").toLowerCase().split(",");
	for (var i = 0; i < list.length; i++) {
		var f = list[i].replace(/['"]/g, "").replace(/^\s+|\s+$/g, "");
		if (/mono|courier|consolas|menlo|monaco|typewriter|fixed|lucida console|terminal/.test(f))
			return 2;
		if (/sans|arial|helvetica|verdana|tahoma|calibri|segoe|trebuchet|dejavu|liberation sans|roboto|open|ubuntu|system-ui|inter/.test(f))
			return 0;
		if (f == "serif" || /times|georgia|garamond|palatino|book|cambria|minion|roman|serif/.test(f))
			return 1;
	}
	return 0;
}

function svg_font_bold(st)
{
	var w = String(st.fontWeight || "normal").toLowerCase();
	return w == "bold" || w == "bolder" || parseInt(w, 10) >= 600;
}

function svg_font_italic(st)
{
	return /italic|oblique/i.test(String(st.fontStyle || ""));
}

/* Lay a text element out.  Returns a list of chunks (runs of characters that move together, which
 * is where text-anchor applies), each { glyphs: [{ cp, x, y, adv, st }] }, in user units. */
function svg_text_layout(ctx, el, st)
{
	var chunks = [], cur = null, cx = 0, cy = 0, prev_space = true;
	var ctrls = [];
	function emit(str, nst)
	{
		var fam = svg_font_family(nst), bold = svg_font_bold(nst), fs = nst.fontSize;
		for (var i = 0; i < str.length; i++) {
			var ch = str.charAt(i);
			if (ch == "\n" || ch == "\r" || ch == "\t")
				ch = " ";
			if (ch == " ") {
				if (prev_space)
					continue;
				prev_space = true;
			}
			else
				prev_space = false;
			var nx = null, ny = null, ndx = 0, ndy = 0;
			for (var k = 0; k < ctrls.length; k++) {
				var c = ctrls[k], n = c.n++;
				if (c.x && n < c.x.length) nx = c.x[n];
				if (c.y && n < c.y.length) ny = c.y[n];
				if (c.dx && n < c.dx.length) ndx = c.dx[n];
				if (c.dy && n < c.dy.length) ndy = c.dy[n];
			}
			if (nx !== null)
				cx = nx;
			if (ny !== null)
				cy = ny;
			if (!cur || nx !== null || ny !== null) {
				cur = { glyphs: [] };
				chunks.push(cur);
			}
			cx += ndx;
			cy += ndy;
			var adv = std_char_width(fam, bold ? 1 : 0, ch) / 1000 * fs + (nst.letterSpacing || 0) + (ch == " " ? (nst.wordSpacing || 0) : 0);
			cur.glyphs.push({ ch: ch, cp: ch.charCodeAt(0), x: cx, y: cy, adv: adv, st: nst, fam: fam, bold: bold });
			cx += adv;
		}
	}
	function walk(node, nst)
	{
		var ctrl = null;
		if (node.name == "text" || node.name == "tspan") {
			var a = node.attrs;
			if (a.x !== undefined || a.y !== undefined || a.dx !== undefined || a.dy !== undefined)
				ctrl = {
					x: a.x !== undefined ? svg_len_list(a.x, ctx.vw) : null, y: a.y !== undefined ? svg_len_list(a.y, ctx.vh) : null,
					dx: a.dx !== undefined ? svg_len_list(a.dx, ctx.vw) : null, dy: a.dy !== undefined ? svg_len_list(a.dy, ctx.vh) : null, n: 0
				};
		}
		if (ctrl)
			ctrls.push(ctrl);
		for (var i = 0; i < node.children.length; i++) {
			var kid = node.children[i];
			if (kid.name == "#text")
				emit(kid.text, nst);
			else if (kid.name == "tspan" || kid.name == "a" || kid.name == "textPath" || kid.name == "altGlyph") {
				var kp = svg_props(ctx, kid);
				if (kp.display == "none")
					continue;
				if (kid.name == "textPath")
					ctx.notes.textpath = true;
				walk(kid, svg_derive(ctx, kid, nst));
			}
		}
		if (ctrl)
			ctrls.pop();
	}
	walk(el, st);
	/* a trailing space is dropped */
	if (chunks.length) {
		var lastc = chunks[chunks.length - 1];
		if (lastc.glyphs.length && lastc.glyphs[lastc.glyphs.length - 1].ch == " ")
			lastc.glyphs.pop();
	}
	/* text-anchor and baseline adjustments */
	for (var ci = 0; ci < chunks.length; ci++) {
		var g = chunks[ci].glyphs;
		if (!g.length)
			continue;
		var w = g[g.length - 1].x + g[g.length - 1].adv - g[0].x;
		var anchor = g[0].st.textAnchor;
		var shift = anchor == "middle" ? -w / 2 : (anchor == "end" ? -w : 0);
		for (var gi = 0; gi < g.length; gi++) {
			g[gi].x += shift;
			var gp = svg_props(ctx, el);
			var base = String(g[gi].st.baselineProp || "");
			g[gi].y += svg_baseline_shift(g[gi].st, el, ctx);
		}
	}
	return chunks;
}

/* How far down the text moves for dominant-baseline / baseline-shift */
function svg_baseline_shift(st, el, ctx)
{
	var p = svg_props(ctx, el);
	var b = String(p["dominant-baseline"] || p["alignment-baseline"] || "");
	var fs = st.fontSize, d = 0;
	if (b == "middle")
		d = 0.26 * fs;
	else if (b == "central")
		d = 0.35 * fs;
	else if (b == "hanging")
		d = 0.8 * fs;
	else if (b == "text-before-edge" || b == "before-edge")
		d = 0.9 * fs;
	else if (b == "text-after-edge" || b == "after-edge" || b == "ideographic")
		d = -0.2 * fs;
	return d;
}

/* The glyph outline for a character (a "?" if the face has none) */
function svg_glyph_path(g)
{
	var font = { family: g.fam, bold: g.bold, name: "" };
	var path = builtin_path(font, g.cp);
	if (!path && g.cp != 32 && g.cp != 160)
		path = builtin_path(font, 63);
	return path;
}

/* Bounding box of a text element's layout in user space, [x0, y0, x1, y1], or null */
function svg_text_layout_box(chunks)
{
	var box = null;
	for (var ci = 0; ci < chunks.length; ci++) {
		for (var gi = 0; gi < chunks[ci].glyphs.length; gi++) {
			var g = chunks[ci].glyphs[gi], fs = g.st.fontSize;
			var b = [g.x, g.y - 0.9 * fs, g.x + g.adv, g.y + 0.22 * fs];
			if (!box)
				box = b;
			else {
				box[0] = Math.min(box[0], b[0]);
				box[1] = Math.min(box[1], b[1]);
				box[2] = Math.max(box[2], b[2]);
				box[3] = Math.max(box[3], b[3]);
			}
		}
	}
	return box;
}

/* Bounding box of a text element after matrix m, for svg_bbox */
function svg_text_box(ctx, el, m)
{
	var st = svg_derive(ctx, el, ctx.st0);
	var box = svg_text_layout_box(svg_text_layout(ctx, el, st));
	if (!box)
		return null;
	var out = null, cs = [[box[0], box[1]], [box[2], box[1]], [box[2], box[3]], [box[0], box[3]]];
	for (var i = 0; i < 4; i++) {
		var p = mat_apply(m, cs[i][0], cs[i][1]);
		if (!out)
			out = [p[0], p[1], p[0], p[1]];
		else {
			out[0] = Math.min(out[0], p[0]); out[2] = Math.max(out[2], p[0]);
			out[1] = Math.min(out[1], p[1]); out[3] = Math.max(out[3], p[1]);
		}
	}
	return out;
}

/* Fill polygons with a color, gradient or pattern given as a paint value */
function svg_paint_polys(ctx, polys, evenodd, val, st, ctm, bbox, opac)
{
	if (!polys.length || opac <= 0)
		return;
	var srv = svg_server(ctx, val);
	if (srv && svg_fill_server(ctx, polys, evenodd, srv, st, ctm, bbox, opac))
		return;
	var col = svg_color(ctx, val, st, 0);
	if (col)
		fill_polys(polys, evenodd, [col[0], col[1], col[2]], Math.min(1, opac * col[3]), ctx.clip);
}

function svg_draw_text(ctx, el, st, m)
{
	if (st.visibility == "hidden" || st.visibility == "collapse")
		return true;
	var chunks = svg_text_layout(ctx, el, st);
	var box = svg_text_layout_box(chunks);
	if (!box)
		return true;
	var runs = [], i, j;
	for (i = 0; i < chunks.length; i++) {
		var gl = chunks[i].glyphs;
		for (j = 0; j < gl.length; j++) {
			var g = gl[j], fs = g.st.fontSize;
			if (fs <= 0)
				continue;
			var run = runs.length ? runs[runs.length - 1] : null;
			if (!run || run.st !== g.st) {
				run = { st: g.st, polys: [], decos: [] };
				runs.push(run);
			}
			var path = svg_glyph_path(g);
			if (path) {
				var skew = svg_font_italic(g.st) ? 0.21 : 0;
				flatten_path(path, mat_mul([fs, 0, skew * fs, -fs, g.x, g.y], m), run.polys);
			}
			var deco = String(g.st.textDecoration || "");
			if (/underline|line-through|overline/.test(deco) && g.ch != " ")
				run.decos.push(g);
		}
	}
	for (i = 0; i < runs.length; i++) {
		var r = runs[i], rs = r.st;
		/* underline, line-through and overline are thin rectangles per character */
		for (j = 0; j < r.decos.length; j++) {
			var d = r.decos[j], dfs = d.st.fontSize, th = Math.max(dfs / 16, 0.3);
			var dd = String(d.st.textDecoration);
			var ys = [];
			if (/underline/.test(dd)) ys.push(d.y + dfs * 0.12);
			if (/line-through/.test(dd)) ys.push(d.y - dfs * 0.3);
			if (/overline/.test(dd)) ys.push(d.y - dfs * 0.85);
			for (var yk = 0; yk < ys.length; yk++) {
				var c0 = mat_apply(m, d.x, ys[yk] - th / 2), c1 = mat_apply(m, d.x + d.adv, ys[yk] - th / 2);
				var c2 = mat_apply(m, d.x + d.adv, ys[yk] + th / 2), c3 = mat_apply(m, d.x, ys[yk] + th / 2);
				r.polys.push([c0[0], c0[1], c1[0], c1[1], c2[0], c2[1], c3[0], c3[1]]);
			}
		}
		if (!r.polys.length)
			continue;
		svg_paint_polys(ctx, r.polys, false, rs.fill, rs, m, box, rs.opacity * rs.fillOpacity);
		var strc = svg_color(ctx, rs.stroke, rs, 0), ssrv = svg_server(ctx, rs.stroke);
		if ((strc || ssrv) && rs.strokeWidth > 0) {
			var sc = mat_scale(m), hw = Math.max(1, rs.strokeWidth * sc) / 2, out = [];
			var cap = rs.cap == "round" ? 1 : rs.cap == "square" ? 2 : 0, join = rs.join == "round" ? 1 : rs.join == "bevel" ? 2 : 0;
			for (j = 0; j < r.polys.length; j++)
				stroke_subpath({ pts: r.polys[j], closed: true }, hw, cap, join, rs.miter, out);
			svg_paint_polys(ctx, out, false, rs.stroke, rs, m, box, rs.opacity * rs.strokeOpacity);
		}
	}
	return true;
}

/* ---------- Images ---------- */

/* The decoded picture an <image> refers to (a data: URI or a file beside the SVG), cached, or null */
function svg_image_source(ctx, href)
{
	if (!ctx.imgcache)
		ctx.imgcache = {};
	if (ctx.imgcache[href] !== undefined)
		return ctx.imgcache[href];
	var img = null;
	try {
		var m = /^data:([^;,]*)((?:;[^;,]*)*),(.*)$/i.exec(href.replace(/^\s+|\s+$/g, ""));
		if (m) {
			if (/svg/i.test(m[1]))
				throw "nested SVG";
			var bytes = /;base64/i.test(m[2]) ? base64_decode(m[3].replace(/\s+/g, "")) : unescape(m[3]);
			img = image_from_string(bytes, "image." + String(m[1]).replace(/^.*\//, ""));
		}
		else if (ctx.doc.dir && !/^[a-z][a-z0-9+.-]*:/i.test(href) && href.charAt(0) != "/" && href.charAt(0) != "\\" &&
			!/(^|[\/\\])\.\.([\/\\]|$)/.test(href)) {
			var fn = ctx.doc.dir + href.replace(/[?#].*$/, "");
			if (/\.svgz?$/i.test(fn))
				throw "nested SVG";
			img = load_image_file(fn);
		}
	}
	catch (e) {
		img = null;
	}
	if (img && img.frames)		/* an animated GIF: its first frame */
		img = { w: img.w, h: img.h, n: img.n, data: img.data, alpha: img.alpha, mask: img.mask };
	ctx.imgcache[href] = img;
	return img;
}

function svg_draw_image(ctx, el, st, m)
{
	if (st.visibility == "hidden" || st.visibility == "collapse")
		return true;
	var href = el.attrs.href;
	if (!href)
		return true;
	var img = svg_image_source(ctx, href);
	if (!img) {
		ctx.notes.image = true;
		return true;
	}
	var x = svg_len(el.attrs.x, ctx.vw, 0), y = svg_len(el.attrs.y, ctx.vh, 0);
	var w = el.attrs.width !== undefined ? svg_len(el.attrs.width, ctx.vw, 0) : img.w;
	var h = el.attrs.height !== undefined ? svg_len(el.attrs.height, ctx.vh, 0) : img.h;
	if (w <= 0 || h <= 0)
		return true;
	/* preserveAspectRatio: align + meet or slice, or none */
	var par = String(el.attrs.preserveAspectRatio || "xMidYMid meet").replace(/^\s*defer\s+/, "").split(/\s+/);
	var rx = x, ry = y, rw = w, rh = h;
	if (par[0] != "none") {
		var sc = (par[1] == "slice") ? Math.max(w / img.w, h / img.h) : Math.min(w / img.w, h / img.h);
		rw = img.w * sc;
		rh = img.h * sc;
		var ax = /xMin/.test(par[0]) ? 0 : (/xMax/.test(par[0]) ? 1 : 0.5);
		var ay = /YMin/.test(par[0]) ? 0 : (/YMax/.test(par[0]) ? 1 : 0.5);
		rx = x + (w - rw) * ax;
		ry = y + (h - rh) * ay;
	}
	var M = mat_mul([rw, 0, 0, -rh, rx, ry + rh], m);		/* the unit square of the picture (y up) -> device */
	/* clip to the viewport rectangle (only matters for slice) */
	var cs = [[x, y], [x + w, y], [x + w, y + h], [x, y + h]], bx0 = 1e30, by0 = 1e30, bx1 = -1e30, by1 = -1e30;
	for (var i = 0; i < 4; i++) {
		var pp = mat_apply(m, cs[i][0], cs[i][1]);
		bx0 = Math.min(bx0, pp[0]); bx1 = Math.max(bx1, pp[0]);
		by0 = Math.min(by0, pp[1]); by1 = Math.max(by1, pp[1]);
	}
	var clip = [Math.max(ctx.clip[0], Math.floor(bx0)), Math.max(ctx.clip[1], Math.floor(by0)),
		Math.min(ctx.clip[2], Math.ceil(bx1)), Math.min(ctx.clip[3], Math.ceil(by1))];
	if (clip[2] <= clip[0] || clip[3] <= clip[1])
		return true;
	if (img.jpeg) {		/* decode at a detail level that suits the size it is drawn at */
		var ratio = img.w / Math.max(1, Math.sqrt(M[0] * M[0] + M[1] * M[1]));
		var need = ratio >= 6 ? 1 : (ratio >= 3 ? 2 : (ratio >= 1.5 ? 4 : 8));
		if (!img.cache || img.cache.N < need) {
			try {
				img.cache = { N: need, img: jpeg_image(img.jpeg, need, null) };
			}
			catch (e) {
				return true;
			}
		}
		img = img.cache.img;
	}
	draw_image(img, M, st.opacity, [0, 0, 0], clip);
	return true;
}

/* ---------- Markers (arrowheads and the like on the vertices of a path) ---------- */

/* The vertices of a path with the directions of the segments coming in and going out */
function svg_marker_vertices(cmds)
{
	var verts = [], cur = null, cx = 0, cy = 0, sx = 0, sy = 0, start = null;
	function dirof(dx, dy)
	{
		return (dx == 0 && dy == 0) ? null : [dx, dy];
	}
	for (var i = 0; i < cmds.length; i++) {
		var c = cmds[i], v;
		if (c[0] == "M") {
			v = { x: c[1], y: c[2], din: null, dout: null };
			verts.push(v);
			cur = start = v;
			cx = sx = c[1];
			cy = sy = c[2];
		}
		else if (c[0] == "Z") {
			if (cur && (cx != sx || cy != sy)) {
				var d = dirof(sx - cx, sy - cy);
				if (!cur.dout)
					cur.dout = d;
				v = { x: sx, y: sy, din: d, dout: null };
				verts.push(v);
				cur = v;
			}
			if (start && cur && cur !== start) {
				if (!start.din)
					start.din = cur.din;
				if (!cur.dout)
					cur.dout = start.dout;
			}
			cx = sx;
			cy = sy;
			cur = null;
		}
		else if (cur) {
			var ex, ey, outd, ind;
			if (c[0] == "L") {
				ex = c[1];
				ey = c[2];
				outd = ind = dirof(ex - cx, ey - cy);
			}
			else {
				ex = c[5];
				ey = c[6];
				outd = dirof(c[1] - cx, c[2] - cy) || dirof(c[3] - cx, c[4] - cy) || dirof(ex - cx, ey - cy);
				ind = dirof(ex - c[3], ey - c[4]) || dirof(ex - c[1], ey - c[2]) || dirof(ex - cx, ey - cy);
			}
			if (!cur.dout)
				cur.dout = outd;
			v = { x: ex, y: ey, din: ind, dout: null };
			verts.push(v);
			cur = v;
			cx = ex;
			cy = ey;
		}
	}
	return verts;
}

function svg_marker_angle(v)
{
	var a = v.din, b = v.dout;
	if (a && b) {
		var la = Math.sqrt(a[0] * a[0] + a[1] * a[1]), lb = Math.sqrt(b[0] * b[0] + b[1] * b[1]);
		var x = a[0] / la + b[0] / lb, y = a[1] / la + b[1] / lb;
		if (Math.abs(x) < 1e-9 && Math.abs(y) < 1e-9)
			return Math.atan2(b[1], b[0]);
		return Math.atan2(y, x);
	}
	if (a || b) {
		var d = a || b;
		return Math.atan2(d[1], d[0]);
	}
	return 0;
}

/* Draw the markers of a shape.  cmds are its path commands in user space, m maps them to the device. */
function svg_draw_markers(ctx, cmds, st, m, depth)
{
	if (ctx.markdepth > 2)
		return true;
	var refs = [st.markerStart, st.markerMid, st.markerEnd], any = false, i;
	for (i = 0; i < 3; i++)
		if (refs[i] && refs[i] != "none")
			any = true;
	if (!any)
		return true;
	var verts = svg_marker_vertices(cmds);
	if (!verts.length)
		return true;
	ctx.markdepth++;
	try {
		for (var vi = 0; vi < verts.length; vi++) {
			var which = vi == 0 ? 0 : (vi == verts.length - 1 ? 2 : 1);
			var mk = refs[which] && refs[which] != "none" ? svg_ref(ctx, refs[which]) : null;
			if (!mk || mk.name != "marker")
				continue;
			var v = verts[vi];
			var orient = String(mk.attrs.orient || "0");
			var ang = 0;
			if (orient == "auto")
				ang = svg_marker_angle(v);
			else if (orient == "auto-start-reverse")
				ang = svg_marker_angle(v) + (which == 0 ? Math.PI : 0);
			else
				ang = (parseFloat(orient) || 0) * Math.PI / 180;
			var mw = svg_len(mk.attrs.markerWidth, ctx.vw, 3), mh = svg_len(mk.attrs.markerHeight, ctx.vh, 3);
			if (mw <= 0 || mh <= 0)
				continue;
			var scale = mk.attrs.markerUnits == "userSpaceOnUse" ? 1 : st.strokeWidth;
			var rx = svg_len(mk.attrs.refX, ctx.vw, 0), ry = svg_len(mk.attrs.refY, ctx.vh, 0);
			var vb = mk.attrs.viewBox ? svg_numbers(mk.attrs.viewBox) : [];
			var vm = [1, 0, 0, 1, 0, 0];
			if (vb.length >= 4 && vb[2] > 0 && vb[3] > 0) {
				var par = String(mk.attrs.preserveAspectRatio || "xMidYMid meet").split(/\s+/);
				var sx = mw / vb[2], sy = mh / vb[3];
				if (par[0] == "none")
					vm = [sx, 0, 0, sy, -vb[0] * sx, -vb[1] * sy];
				else {
					var sc = par[1] == "slice" ? Math.max(sx, sy) : Math.min(sx, sy);
					var ax = /xMin/.test(par[0]) ? 0 : (/xMax/.test(par[0]) ? 1 : 0.5), ay = /YMin/.test(par[0]) ? 0 : (/YMax/.test(par[0]) ? 1 : 0.5);
					vm = [sc, 0, 0, sc, (mw - vb[2] * sc) * ax - vb[0] * sc, (mh - vb[3] * sc) * ay - vb[1] * sc];
				}
			}
			/* the reference point (in marker content coordinates) lands on the vertex */
			var rp = mat_apply(vm, rx, ry);
			var co = Math.cos(ang), si = Math.sin(ang);
			var place = [co * scale, si * scale, -si * scale, co * scale, v.x, v.y];
			var cm = mat_mul(mat_mul(vm, [1, 0, 0, 1, -rp[0], -rp[1]]), mat_mul(place, m));
			var mst = svg_derive(ctx, mk, ctx.st0);
			mst.markerStart = mst.markerMid = mst.markerEnd = "none";
			for (var ci = 0; ci < mk.children.length; ci++)
				if (!svg_draw(ctx, mk.children[ci], mst, cm, depth + 1))
					return false;
		}
	}
	finally {
		ctx.markdepth--;
	}
	return true;
}

var SVG_SKIP = { "#text": 1, defs: 1, clipPath: 1, mask: 1, marker: 1, pattern: 1, symbol: 1, linearGradient: 1, radialGradient: 1,
	style: 1, title: 1, desc: 1, metadata: 1, filter: 1, script: 1, foreignObject: 1, "#document": 0 };
var SVG_CONTAINER = { g: 1, a: 1, svg: 1, "switch": 1, use: 1 };
var SVG_SHAPES = { path: 1, rect: 1, circle: 1, ellipse: 1, line: 1, polyline: 1, polygon: 1 };

/* Draw an element and its children, without the clipping, masking, filtering and group opacity that
 * svg_draw wraps around it.  Returns false if the render was aborted. */
function svg_draw_node(ctx, el, parent, ctm, depth)
{
	if (depth > 40)
		return true;
	if (++ctx.count % 40 == 0 && ctx.abort_cb && ctx.abort_cb()) {
		ctx.aborted = true;
		return false;
	}
	var name = el.name;
	if (SVG_SKIP[name])
		return true;
	var p = svg_props(ctx, el);
	if (p.display == "none")
		return true;
	var st = svg_derive(ctx, el, parent);
	var m = ctm;
	if (el.attrs.transform)
		m = mat_mul(svg_transform(el.attrs.transform), ctm);
	var i;
	if (SVG_SHAPES[name]) {
		var cmds = svg_shape_cmds(ctx, el, ctx.vw, ctx.vh);
		svg_paint(ctx, cmds, st, m, name != "line");
		if ((name == "path" || name == "line" || name == "polyline" || name == "polygon") && (st.markerStart != "none" || st.markerMid != "none" || st.markerEnd != "none"))
			return svg_draw_markers(ctx, cmds, st, m, depth);
		return true;
	}
	if (name == "text")
		return svg_draw_text(ctx, el, st, m);
	if (name == "image")
		return svg_draw_image(ctx, el, st, m);
	if (name == "use") {
		var href = el.attrs.href;
		var target = href && href.charAt(0) == "#" ? ctx.ids[href.substr(1)] : null;
		if (!target || ctx.usedepth > 8)
			return true;
		var um = mat_mul([1, 0, 0, 1, svg_len(el.attrs.x, ctx.vw, 0), svg_len(el.attrs.y, ctx.vh, 0)], m);
		ctx.usedepth++;
		var ok = true;
		if (target.name == "symbol" || target.name == "svg") {
			var sst = svg_derive(ctx, target, st);
			for (i = 0; i < target.children.length && ok; i++)
				ok = svg_draw(ctx, target.children[i], sst, um, depth + 1);
		}
		else {
			/* the referenced element is drawn as if it were a child of the use element */
			ok = svg_draw(ctx, target, st, um, depth + 1);
		}
		ctx.usedepth--;
		return ok;
	}
	if (name == "g" || name == "a" || name == "svg" || name == "switch" || name == "#document") {
		if (name == "svg" && depth > 0)		/* nested svg: only its x/y offset is honored */
			m = mat_mul([1, 0, 0, 1, svg_len(el.attrs.x, ctx.vw, 0), svg_len(el.attrs.y, ctx.vh, 0)], m);
		for (i = 0; i < el.children.length; i++) {
			if (!svg_draw(ctx, el.children[i], st, m, depth + 1))
				return false;
			if (name == "switch" && !SVG_SKIP[el.children[i].name])
				break;
		}
		return true;
	}
	return true;
}

/* ---------- The document ---------- */

function svg_find_root(doc)
{
	for (var i = 0; i < doc.children.length; i++) {
		if (doc.children[i].name == "svg")
			return doc.children[i];
	}
	return null;
}

function svg_collect(el, ids, rules, depth)
{
	if (el.attrs.id)
		ids[el.attrs.id] = el;
	if (el.name == "style" && el.text.length < 500000)
		svg_parse_css(el.text, rules);
	if (depth > 100)
		return;
	for (var i = 0; i < el.children.length; i++)
		svg_collect(el.children[i], ids, rules, depth + 1);
}

function SvgDoc(root, ids, rules)
{
	this.root = root;
	this.ids = ids;
	this.rules = rules;
	var a = root.attrs;
	var vb = a.viewBox ? svg_numbers(a.viewBox) : [];
	if (vb.length < 4 || vb[2] <= 0 || vb[3] <= 0)
		vb = null;
	var w = a.width !== undefined && !/%\s*$/.test(a.width) ? svg_len(a.width, 0, 0) : 0;
	var h = a.height !== undefined && !/%\s*$/.test(a.height) ? svg_len(a.height, 0, 0) : 0;
	if (vb) {
		if (w <= 0 && h > 0)
			w = h * vb[2] / vb[3];
		else if (h <= 0 && w > 0)
			h = w * vb[3] / vb[2];
		else if (w <= 0 && h <= 0) {
			w = vb[2];
			h = vb[3];
		}
	}
	if (w <= 0)
		w = 300;
	if (h <= 0)
		h = 150;
	var maxdim = 16384;
	if (w > maxdim || h > maxdim) {
		var k = maxdim / Math.max(w, h);
		w *= k;
		h *= k;
	}
	this.w = w;
	this.h = h;
	this.vb = vb;
	/* viewBox -> pixels, honoring preserveAspectRatio (meet, or none) */
	this.rootm = [1, 0, 0, 1, 0, 0];
	if (vb) {
		var sx = w / vb[2], sy = h / vb[3];
		if (/^\s*none/.test(a.preserveAspectRatio || "")) {
			this.rootm = [sx, 0, 0, sy, -vb[0] * sx, -vb[1] * sy];
		}
		else {
			var s = Math.min(sx, sy);
			this.rootm = [s, 0, 0, s, (w - vb[2] * s) / 2 - vb[0] * s, (h - vb[3] * s) / 2 - vb[1] * s];
		}
	}
	this.vw = vb ? vb[2] : w;
	this.vh = vb ? vb[3] : h;
	this.initial_fit = "page";
	this.actual_scale = 1;
	this.pages = [{ box: [0, 0, w, h], rotate: 0 }];
	this.notice = "";
}

SvgDoc.prototype.get_pages = function ()
{
	return this.pages;
};

SvgDoc.prototype.render = function (page, M, clip, abort_cb, maxN)
{
	CLIPMASK = null;
	SVG_COV = null;
	LAST_DEGRADED = false;
	var ctx = { doc: this, ids: this.ids, rules: this.rules, clip: clip, abort_cb: abort_cb, count: 0, aborted: false,
		vw: this.vw, vh: this.vh, diag: Math.sqrt((this.vw * this.vw + this.vh * this.vh) / 2), notes: {}, usedepth: 0,
		clipdepth: 0, maskdepth: 0, patdepth: 0, markdepth: 0, noopac: null, st0: null };
	/* user units -> pixels (y down) -> page space (y up) -> device */
	var ctm = mat_mul(this.rootm, mat_mul([1, 0, 0, -1, 0, this.h], M));
	var st0 = { fill: "black", stroke: "none", strokeWidth: 1, fillOpacity: 1, strokeOpacity: 1, fillRule: "nonzero",
		cap: "butt", join: "miter", miter: 4, dash: null, dashOffset: 0, visibility: "visible", color: "black",
		opacity: 1, fontSize: 16, fontFamily: "serif", fontWeight: "normal", fontStyle: "normal", textAnchor: "start",
		clipRule: "nonzero", letterSpacing: 0, wordSpacing: 0, textDecoration: "none", markerStart: "none", markerMid: "none",
		markerEnd: "none" };
	ctx.st0 = st0;
	var ok = svg_draw(ctx, this.root, st0, ctm, 0);
	var notes = [];
	if (ctx.notes.textpath)
		notes.push("text along a path");
	if (ctx.notes.image)
		notes.push("pictures that could not be loaded");
	if (ctx.notes.filter)
		notes.push("some filter effects");
	this.notice = notes.length ? "Not drawn: " + notes.join(", ") : "";
	return ok;
};

/* Read an SVG file and return a document for the viewer */
function load_svg(filename)
{
	var f = new File(filename);
	if (!f.exists)
		throw "File not found: " + filename;
	if (f.length > 8 * 1024 * 1024)
		throw "SVG file is too large";
	if (!f.open("rb"))
		throw "Unable to open " + filename;
	var text = f.read();
	f.close();
	text = text.replace(/^\xef\xbb\xbf/, "");
	if (text.charCodeAt(0) == 0x1f && text.charCodeAt(1) == 0x8b)
		throw "Compressed SVG (.svgz) files are not supported";
	var tree = svg_parse_xml(text);
	var root = svg_find_root(tree);
	if (!root)
		throw "Not an SVG file";
	var ids = {};
	var rules = [];
	svg_collect(root, ids, rules, 0);
	var sdoc = new SvgDoc(root, ids, rules);
	sdoc.dir = String(filename).replace(/[^\/\\]*$/, "");
	return sdoc;
}
