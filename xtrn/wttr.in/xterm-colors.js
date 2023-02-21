/*
 * Adapted from
 * https://github.com/zaidhaan/xterm2ansi
 * Copyright (c) 2022 Zaidhaan Hussain
 * 
 * ISC License
 *
 * Copyright (c) 2022 Zaidhaan Hussain
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.

 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

const colors = [
	{ r: 0x00, g: 0x00, b: 0x00, code: 30, bold: 0 }, // black
	{ r: 0xCD, g: 0x00, b: 0x00, code: 31, bold: 0 }, // red
	{ r: 0x00, g: 0xCD, b: 0x00, code: 32, bold: 0 }, // green
	{ r: 0xCD, g: 0xCD, b: 0x00, code: 33, bold: 0 }, // yellow
	{ r: 0x00, g: 0x00, b: 0xEE, code: 34, bold: 0 }, // blue
	{ r: 0xCD, g: 0x00, b: 0xCD, code: 35, bold: 0 }, // magenta
	{ r: 0x00, g: 0xCD, b: 0xCD, code: 36, bold: 0 }, // cyan
	{ r: 0xE5, g: 0xE5, b: 0xE5, code: 37, bold: 0 }, // white
	{ r: 0x7F, g: 0x7F, b: 0x7F, code: 30, bold: 1 }, // bright black
	{ r: 0xFF, g: 0x00, b: 0x00, code: 31, bold: 1 }, // bright red
	{ r: 0x00, g: 0xFF, b: 0x00, code: 32, bold: 1 }, // bright green
	{ r: 0xFF, g: 0xFF, b: 0x00, code: 33, bold: 1 }, // bright yellow
	{ r: 0x5C, g: 0x5C, b: 0xFF, code: 34, bold: 1 }, // bright blue
	{ r: 0xFF, g: 0x00, b: 0xFF, code: 35, bold: 1 }, // bright magenta
	{ r: 0x00, g: 0xFF, b: 0xFF, code: 36, bold: 1 }, // bright cyan
	{ r: 0xFF, g: 0xFF, b: 0xFF, code: 37, bold: 1 }, // bright white
];

function colorDistance(r1, g1, b1, r2, g2, b2) {
	return Math.sqrt(Math.pow(r2 - r1, 2) + Math.pow(b2 - b1, 2) + Math.pow(g2 - g1, 2));
}

function rgbToANSI(rgb) {
	var nearest = 0;
	var minDistance = 256;
	for (var i = 0; i < 16; i++) {
		var currentColor = colors[i];
		var distance = colorDistance(rgb.r, rgb.g, rgb.b, currentColor.r, currentColor.g, currentColor.b);
		if (distance < minDistance) {
			minDistance = distance;
			nearest = currentColor;
		}
	}
	return nearest;
}

function xterm256ToRGB(color) {
	var r = 0;
	var g = 0;
	var b = 0;
	if (color < 16) {
		r = ansi_colors[color].r;
		g = ansi_colors[color].g;
		b = ansi_colors[color].b;
	} else if (color < 232) {
		color -= 16;
		const _r = (color / 36);
		const _g = (color % 36) / 6;
		const _b = (color % 6);
		r = _r ? _r * 40 + 55 : 0;
		g = _g ? _g * 40 + 55 : 0;
		b = _b ? _b * 40 + 55 : 0;
	} else {
		color -= 232;
		r = g = b = (color * 10) + 8;
	}
	return { r: r, g: g, b: b };
}

function xterm256ToANSI(color) {
	const rgb = xterm256ToRGB(color);
	return rgbToANSI(rgb);
}

function replace256(match, p1, p2, p3) {
	const color = parseInt(p2, 10);
	if (isNaN(color)) return '';
	const ansiColor = xterm256ToANSI(color);
	const code = (p1 === '48') ? (ansiColor.code + 10) : ansiColor.code;
	return '\x1b[' + ansiColor.bold + ';' + code + p3 + 'm';
}

function replaceRGB(match, p1, p2, p3, p4, p5) {
	const r = parseInt(p2, 10);
	if (isNaN(r)) return '';
	const g = parseInt(p3, 10);
	if (isNaN(g)) return '';
	const b = parseInt(p4, 10);
	if (isNaN(b)) return '';
	const ansiColor = rgbToANSI({ r: r, g: g, b:b });
	const code = (p1 === '48') ? (ansiColor.code + 10): ansiColor.code;
	return '\x1b[' + ansiColor.bold + ';' + code + p5 + 'm';
}

function convertColors(str) {
	return str.replace(
		/\x1b\[([34]8);5;(\d+)(;\d+)?m/g, replace256
	).replace(
		/\x1b\[([34]8);2;(\d+);(\d+);(\d+)(;\d+)?m/g, replaceRGB
	);
}

this;