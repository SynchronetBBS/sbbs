/*
 * Synchronet JS conversion of tdfiglet.c
 * Based on the C code by Unknown/Modified by The Draw
 * Converted to Synchronet JS by Nelgin
 *
 * More information at:
 *   https://wiki.synchro.net/custom:thedrawfonts
 *   https://wiki.synchro.net/module:tdfiglet
 *
 * @format.tab-size 4, @format.use-tabs true
 */

var OUTLN_FNT = 0;
var BLOCK_FNT = 1;
var COLOR_FNT = 2;

var NUM_CHARS = 94;
var LEFT_JUSTIFY = 0;
var RIGHT_JUSTIFY = 1;
var CENTER_JUSTIFY = 2;

var DEFAULT_WIDTH = 80;
var DEFAULT_FONT = "brndamgx.tdf";

var charlist = "!\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~";

function showinfo(font) {
	if (!this.opt || !this.opt.info || !font || typeof font !== "object") return;
	
	var typeStr = ["Outline", "Block", "Color"][font.fonttype] || "Unknown";
	var supported = "";
	for (var i = 0; i < NUM_CHARS; i++) {
		if (font.charlist[i] !== 0xffff) supported += charlist[i];
	}

	var sep = ""; for (var s = 0; s < 60; s++) sep += "=";
	var line = ""; for (var s = 0; s < 60; s++) line += "-";

	print(sep);
	print("File:      " + font.fullpath);
	print("Name:      " + font.name);
	print("Font No:   " + (font.index + 1));
	print("Type:      " + typeStr);
	print("Height:    " + font.height);
	print("Spacing:   " + font.spacing);
	print("Supported: " + supported);
	print(line);
}

function getlist() {
	var path = (this.opt && this.opt.fontpath) ? this.opt.fontpath : system.ctrl_dir + 'tdfonts/';
	if (path.slice(-1) !== '/' && path.slice(-1) !== '\\') path += '/';

	if (!file_exists(path) && !file_isdir(path)) return [];

	var files = directory(path + "*");
	var tdf_files = [];
	for (var i = 0; i < files.length; i++) {
		if (wildmatch(files[i], "*.tdf")) tdf_files.push(files[i]);
	}
	return tdf_files;
}

function loadfont(fn_arg) {
	var magic = "\x13TheDraw FONTS file\x1a";
	var path = (this.opt && this.opt.fontpath) ? this.opt.fontpath : system.ctrl_dir + 'tdfonts/';
	if (path.slice(-1) !== '/' && path.slice(-1) !== '\\') path += '/';

	var isRandom = (this.opt && (this.opt.random || this.opt.index === 0 || this.opt.index === "random"));
	var retryMax = (this.opt && this.opt.retryno !== undefined) ? this.opt.retryno : 3;
	var tryCount = 0;

	// Retry loop for random selection
	while (tryCount <= retryMax) {
		var font = { glyphs: [], charlist: [], height: 0, fullpath: "", index: 0 };
		var fn = "";

		if (isRandom && !fn_arg) {
			var tdf_files = this.getlist();
			if (tdf_files.length > 0) fn = tdf_files[Math.floor(Math.random() * tdf_files.length)];
		} 
		
		if (!fn) {
			var target = fn_arg || DEFAULT_FONT;
			fn = (target.indexOf('/') === -1 && target.indexOf('\\') === -1) ? path + target : target;
		}

		var f = new File(fn);
		if (f.exists && f.open("rb")) {
			var map = f.read();
			f.close();

			if (map.substring(0, 20) === magic) {
				var idxSetting = (this.opt && this.opt.index !== undefined) ? this.opt.index : 1;
				var targetIndex = 0;

				if (idxSetting === 0 || idxSetting === "random") {
					var totalInFile = 0;
					var checkPos = 20;
					while ((checkPos = map.indexOf("\x55\xaa\x00\xff", checkPos + 1)) !== -1) {
						totalInFile++;
					}
					targetIndex = Math.floor(Math.random() * (totalInFile + 1));
				} else {
					targetIndex = Math.max(0, parseInt(idxSetting) - 1);
				}

				var fontStart = 20;
				var found = true;
				for (var n = 0; n < targetIndex; n++) {
					fontStart = map.indexOf("\x55\xaa\x00\xff", fontStart + 1);
					if (fontStart === -1) { found = false; break; }
				}

				if (found) {
					font.fullpath = f.name;
					font.index = targetIndex; 
					font.namelen = map.charCodeAt(fontStart + 4);
					font.name = map.substring(fontStart + 5, fontStart + 5 + font.namelen).trim();
					font.fonttype = map.charCodeAt(fontStart + 21);
					font.spacing = map.charCodeAt(fontStart + 22);
					var dataBase = fontStart + 213;

					for (var i = 0; i < NUM_CHARS; i++) {
						var offPos = fontStart + 25 + (i * 2);
						var offVal = map.charCodeAt(offPos) | (map.charCodeAt(offPos + 1) << 8);
						font.charlist[i] = offVal;
						if (offVal !== 0xffff) {
							var h = map.charCodeAt(dataBase + offVal + 1);
							if (h > font.height) font.height = h;
							font.glyphs[i] = readchar(offVal, dataBase, map, font.fonttype, font.height);
						} else {
							font.glyphs[i] = null;
						}
					}
					return font;
				}
			}
		}
		
		// If we didn't return a font and retry is specified, try again
		if (isRandom && this.opt.retry) {
			tryCount++;
			// Clear fn_arg for next loop to ensure we pick a new file if needed
			if (!this.opt.index) fn_arg = null; 
		} else {
			break;
		}
	}
	return null;
}

function readchar(offset, dataBase, map, fontType, fontHeight) {
	var p = dataBase + offset;
	var glyph = { width: map.charCodeAt(p++), height: map.charCodeAt(p++), cell: [] };
	var currentRow = 0, col = 0;
	while (p < map.length) {
		if (currentRow >= glyph.height) break; 
		var ch = map.charCodeAt(p++);
		if (ch === 0x00) break;
		if (ch === 0x0d) { currentRow++; col = 0; }
		else {
			var color = (fontType === COLOR_FNT) ? map.charCodeAt(p++) : 0x07;
			if (ch < 0x20) {
				for (var s = 0; s < ch; s++) glyph.cell[currentRow * glyph.width + col + s] = { charCode: 32, color: color };
				col += ch;
			} else {
				glyph.cell[currentRow * glyph.width + col] = { charCode: ch, color: color };
				col++;
			}
		}
	}
	return glyph;
}

function printcolor(color) {
	var fg = color & 0x0f, bg = (color & 0xf0) >> 4;
	
	if (!this.opt || !this.opt.ansi) {
		var fgcodes = ["-K", "-B", "-G", "-C", "-R", "-M", "-Y", "-W", "HK", "HB", "HG", "HC", "HR", "HM", "HY", "HW"];
		var bgcodes = ["0", "4", "2", "6", "1", "5", "3", "7"];
		return "\x01" + fgcodes[fg].split("").join("\x01") + "\x01" + bgcodes[bg];
	}
	
	var blink = (color & 0x80);
	var ansi_fg = [30, 34, 32, 36, 31, 35, 33, 37, 90, 94, 92, 96, 91, 95, 93, 97];
	var ansi_bg = [40, 44, 42, 46, 41, 45, 43, 47];
	return "\x1b[0;" + (blink ? "5;" : "") + ansi_bg[bg & 0x07] + ";" + ansi_fg[fg] + "m";
}

function reset_color() {
	return (this.opt && this.opt.ansi === true) ? "\x1b[0m" : "\x01N";
}

function printrow(glyph, row) {
	if (!glyph) return "";
	var out = "", lastcolor = -1;
	if (row >= glyph.height) {
		for(var i=0; i < glyph.width; i++) out += " ";
		return out;
	}
	for (var i = 0; i < glyph.width; i++) {
		var cell = glyph.cell[row * glyph.width + i];
		if (!cell) { out += " "; continue; }
		if (cell.color !== lastcolor) { out += printcolor.call(this, cell.color); lastcolor = cell.color; }
		var ch = String.fromCharCode(cell.charCode);
		out += (this.opt && this.opt.utf8 === true && cell.charCode > 127) ? utf8_encode(ch) : ch;
	}
	out += reset_color.call(this);
	return out;
}

function lookupchar(c, font) {
	var char_code = c.charCodeAt(0);
	for (var i = 0; i < NUM_CHARS; i++) {
		if (charlist.charCodeAt(i) === char_code && font.charlist[i] !== 0xffff) return i;
	}
	return -1;
}

function getwidth(str, font) {
	var linewidth = 0;
	for (var i = 0; i < str.length; i++) {
		if (str[i] === " ") { linewidth += 6; continue; }
		var idx = lookupchar(str[i], font);
		if (idx !== -1) {
			linewidth += font.glyphs[idx].width;
			if (i < str.length - 1) linewidth += font.spacing;
		}
	}
	return linewidth;
}

function output_line(str, font) {
	var maxheight = font.height;
	var linewidth = getwidth(str, font);
	var width = (this.opt && this.opt.width) ? this.opt.width : DEFAULT_WIDTH;
	var margin = (this.opt && this.opt.margin) ? this.opt.margin : 0;
	var justify = (this.opt && this.opt.justify) ? this.opt.justify : LEFT_JUSTIFY;
	
	var padding = margin;
	if (justify === CENTER_JUSTIFY) padding = Math.floor((width - linewidth) / 2);
	else if (justify === RIGHT_JUSTIFY) padding = width - margin - linewidth;
	padding = Math.max(0, padding);

	var out = "";
	for (var i = 0; i < maxheight; i++) {
		for (var p = 0; p < padding; p++) out += " ";
		for (var c = 0; c < str.length; c++) {
			if (str[c] === " ") {
				for (var s = 0; s < 6; s++) out += " ";
				continue;
			}
			var idx = lookupchar(str[c], font);
			if (idx === -1) continue;
			out += printrow.call(this, font.glyphs[idx], i);
			if (c < str.length - 1) for (var s = 0; s < font.spacing; s++) out += " ";
		}
		out += "\r\n";
	}
	return out;
}

function output(str, font_arg) {
	var font = (typeof font_arg === "object" && font_arg !== null) ? font_arg : this.loadfont(font_arg);
	if (!font) return "";

	var out = "";
	var width = (this.opt && this.opt.width) ? this.opt.width : DEFAULT_WIDTH;
	var margin = (this.opt && this.opt.margin) ? this.opt.margin : 0;
	var max_allowed_width = width - (margin * 2);

	var lines = [];
	var workingStr = str;

	while (workingStr.length > 0) {
		var currentLineWidth = 0, breakPoint = 0, lastSpace = -1;
		for (var i = 0; i < workingStr.length; i++) {
			var charWidth = (workingStr[i] === " ") ? 6 : 0;
			if (workingStr[i] === " ") lastSpace = i;
			else {
				var idx = lookupchar(workingStr[i], font);
				if (idx !== -1) charWidth = font.glyphs[idx].width + (i < workingStr.length - 1 ? font.spacing : 0);
			}
			if (currentLineWidth + charWidth > max_allowed_width) {
				if (lastSpace !== -1) {
					breakPoint = lastSpace;
					workingStr = workingStr.substring(0, lastSpace) + workingStr.substring(lastSpace + 1);
				} else breakPoint = i;
				break;
			}
			currentLineWidth += charWidth;
			breakPoint = i + 1;
		}
		if (breakPoint === 0 && workingStr.length > 0) breakPoint = 1;
		lines.push(workingStr.substring(0, breakPoint));
		workingStr = workingStr.substring(breakPoint);
	}

	var useBlank = (this.opt && this.opt.blankline === false) ? false : true;
	for (var i = 0; i < lines.length; i++) {
		out += output_line.call(this, lines[i].trim(), font);
		if (useBlank && i < lines.length - 1) out += "\r\n";
	}
	return out;
}

function printstr(str, font_arg) {
	var font = (typeof font_arg === "object" && font_arg !== null) ? font_arg : this.loadfont(font_arg);
	if (!font) return;
	this.showinfo(font);
	write(this.output(str, font));
}

function display_all(text_to_print) {
	var files = this.getlist();
	var str = text_to_print || "Test";

	for (var i = 0; i < files.length; i++) {
		var fontNo = 1;
		while (true) {
			if (!this.opt) this.opt = {};
			this.opt.index = fontNo;
			var font = this.loadfont(files[i]);
			if (!font) break;

			this.printstr(str, font);

			if (this.opt && this.opt.pause === true) {
				print("\r\n[Press Enter...]");
				readln();
			}
			fontNo++;
		}
	}
}

this;
