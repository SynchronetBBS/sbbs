// Synchronet avatars: 10 x 6 character cells of CP437 block art, drawn on
// a canvas with the 8 x 16 VGA font (images/cp437-ibm-vga8.png: 64 glyphs
// per row, 4 rows, glyph pixels opaque on a transparent ground).
//
// avatarBadge(data, initial, cls)
//   a <span class="cls"> showing the initial; when data (base64 of 120
//   bytes: a CP437 character and a PC text attribute per cell, attribute
//   bits 0-3 the foreground, 4-6 the background; bit 7 is ignored) is
//   given and the font has loaded, the drawing (80 x 96 px) replaces it
//   and the span gets the class "art".  Decorative: aria-hidden.

import { h } from "./dom.js";

const COLS = 10;
const ROWS = 6;
const FW = 8;
const FH = 16;
const SHEET = "images/cp437-ibm-vga8.png";
const SHEET_COLS = 64;
// The PC text-mode palette, by attribute value
const PALETTE = ["#000000", "#0000a8", "#00a800", "#00a8a8", "#a80000", "#a800a8", "#a85400", "#a8a8a8",
	"#545454", "#5454fc", "#54fc54", "#54fcfc", "#fc5454", "#fc54fc", "#fcfc54", "#ffffff"];

let sheet = null;
const tinted = new Map();	// color -> the font sheet in that color

function loadSheet() {
	if (!sheet) {
		sheet = new Promise((resolve) => {
			const img = new Image();
			img.addEventListener("load", () => resolve(img));
			img.addEventListener("error", () => resolve(null));
			img.src = SHEET;
		});
	}
	return sheet;
}

function tint(img, color) {
	let c = tinted.get(color);
	if (c)
		return c;
	c = document.createElement("canvas");
	c.width = img.naturalWidth;
	c.height = img.naturalHeight;
	const g = c.getContext("2d");
	g.drawImage(img, 0, 0);
	g.globalCompositeOperation = "source-in";
	g.fillStyle = color;
	g.fillRect(0, 0, c.width, c.height);
	tinted.set(color, c);
	return c;
}

// The cell bytes, or null when data is not an avatar
function cells(data) {
	if (typeof data !== "string" || data === "")
		return null;
	let bin;
	try {
		bin = atob(data);
	} catch (e) {
		return null;
	}
	if (bin.length !== COLS * ROWS * 2)
		return null;
	const b = new Uint8Array(bin.length);
	for (let i = 0; i < bin.length; i++)
		b[i] = bin.charCodeAt(i);
	return b;
}

function draw(img, b) {
	const canvas = document.createElement("canvas");
	canvas.width = COLS * FW;
	canvas.height = ROWS * FH;
	canvas.setAttribute("aria-hidden", "true");
	const g = canvas.getContext("2d");
	for (let i = 0; i < COLS * ROWS; i++) {
		const ch = b[i * 2];
		const attr = b[i * 2 + 1];
		const x = (i % COLS) * FW;
		const y = Math.floor(i / COLS) * FH;
		g.fillStyle = PALETTE[(attr >> 4) & 7];
		g.fillRect(x, y, FW, FH);
		g.drawImage(tint(img, PALETTE[attr & 15]), (ch % SHEET_COLS) * FW, Math.floor(ch / SHEET_COLS) * FH, FW, FH,
			x, y, FW, FH);
	}
	return canvas;
}

export function avatarBadge(data, initial, cls) {
	const el = h("span", { class: cls, "aria-hidden": "true", text: initial });
	const b = cells(data);
	if (!b)
		return el;
	loadSheet().then((img) => {
		if (!img)
			return;	// the initial stays
		try {
			const canvas = draw(img, b);
			el.textContent = "";
			el.append(canvas);
			el.classList.add("art");
		} catch (e) {
			// a canvas this browser cannot draw: the initial stays
		}
	});
	return el;
}
