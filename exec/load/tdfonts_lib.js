/*
 * Synchronet JS conversion of tdfiglet.c
 * Based on the C code by Unknown/Modified by The Draw
 * Converted to Synchronet JS by Nelgin
 * @format.tab-size 4, @format.use-tabs true
 */
require("cga_defs.js", "LIGHTGRAY");
// Constants (using var as requested)
var OUTLN_FNT = 0;
var BLOCK_FNT = 1;
var COLOR_FNT = 2;
var NUM_CHARS = 94;
var MAX_UTFSTR = 5; // Max bytes for UTF-8 character + null terminator
var LEFT_JUSTIFY = 0;
var RIGHT_JUSTIFY = 1;
var CENTER_JUSTIFY = 2;
var DEFAULT_WIDTH = 80;
var FONT_DIR = system.ctrl_dir + 'tdfonts/';
var FONT_EXT = "tdf";
var DEFAULT_FONT = "brndamgx";
// Character list
var charlist = "!\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~";

function loadfont(fn_arg) {
	var font = {}; // Use object for font_t
	var map = null; // Represents the font file data
	var fn = null;
	var magic = "\x13TheDraw FONTS file\x1a"; // TheDraw font file magic number
	// Construct font file path
	if (fn_arg.indexOf('/') === -1) {
		if (file_getext(fn_arg)) {
			fn = file_getcase(FONT_DIR + fn_arg);
		}
		else {
			fn = file_getcase(FONT_DIR + fn_arg + "." + FONT_EXT);
		}
	}
	else {
		fn = file_getcase(fn_arg); // Assuming full path is provided
	}
	if (!fn || !file_exists(fn)) {
		log("Error: Font file not found: " + fn_arg);
		exit(1);
	}
	if (this.opt && opt.info) {
		writeln("file: " + fn);
	}
	// Read the font file content
	var f = new File(fn);
	if (!f.open("rb")) { // Open in binary read mode
		log("Error: Unable to open font file: " + f.error);
		exit(1);
	}
	var len = f.length;
	map = f.read(len); // Read the whole file content
	f.close();
	if (map.length !== len) {
		log("Error: Failed to read complete font file.");
		exit(1);
	}
	// Parse the font header (offsets based on C code)
	// Synchronet JS provides byte access to file data strings/buffers.
	// The provided C code uses a raw byte array (uint8_t *map).
	// We'll treat the `map` variable (which is a string or Buffer in JS depending on Synchronet version)
	// as a byte sequence. Accessing bytes can be done via charCodeAt(index) or similar depending on how read() returns data.
	// For simplicity and assuming read() returns a string of bytes, we'll use charCodeAt.
	// A more robust solution might involve using ArrayBuffer and DataView if available in Synchronet JS.
	try {
		const sequence = "\x55\xaa\x00\xff";
		if (this.opt && opt.random && opt.index === undefined)
			opt.index = random(map.split(sequence).length);
		if (this.opt && opt.index > 0) {
			var index = 20;
			var n = 0;
			while (n < opt.index) {
				index = map.indexOf(sequence, index + 1);
				if (index === -1)
					break;
				n++;
			}
			if (index !== -1)
				map = map.slice(0, 20) + map.slice(index);
		}
		font.namelen = map.charCodeAt(24);
		font.name = map.substring(25, 25 + font.namelen);
		font.fonttype = map.charCodeAt(41);
		font.spacing = map.charCodeAt(42);
		// blocksize is uint16_t, read two bytes
		font.blocksize = map.charCodeAt(43) | (map.charCodeAt(44) << 8);
		// charlist is uint16_t array, starting at offset 45
		// There are NUM_CHARS (94) entries
		font.charlist = [];
		for (var i = 0; i < NUM_CHARS; i++) {
			var offset = 45 + i * 2;
			font.charlist[i] = map.charCodeAt(offset) | (map.charCodeAt(offset + 1) << 8);
		}
		font.data = map.substring(233); // The rest of the data is glyph data
		font.height = 0;
		// Check magic number and font type
		if (map.substring(0, magic.length) !== magic) {
			log("Invalid font file: " + fn);
			exit(1);
		}
	}
	catch (e) {
		log("Error parsing font file header: " + e);
		exit(1);
	}
	if (this.opt && opt.info) {
		writeln("index: " + opt.index);
		writeln("font: " + font.name);
		writeln("type: " + font.fonttype);
		write("char list: ");
	}
	// Determine overall font height and validate glyph addresses
	for (var i = 0; i < NUM_CHARS; i++) {
		// In JS, we can't easily get the "address" like in C `&map[233]` or `map + st.st_size`.
		// We'll work with string offsets relative to `font.data`.
		// charlist[i] is the offset within the original file 'map'.
		// The glyph data offset within 'font.data' is charlist[i] (since font.data starts at map[233]).
		var glyph_data_offset = font.charlist[i];
		// Check if the character exists in the font (not 0xffff)
		if (glyph_data_offset !== 0xffff) {
			if (this.opt && opt.info)
				write(charlist[i]);
			// Read glyph width and height from font.data
			// The width is at glyph_data_offset, height at glyph_data_offset + 1
			try {
				var glyph_width = font.data.charCodeAt(glyph_data_offset);
				var glyph_height = font.data.charCodeAt(glyph_data_offset + 1); // Height is at offset + 1 in the glyph data
				if (glyph_height > font.height) {
					font.height = glyph_height;
				}
			}
			catch (e) {
				log("Error reading glyph dimensions for char index " + i + ": " + e);
				// Continue or exit depending on desired error handling
			}
		}
	}
	if (this.opt && opt.info)
		writeln("");
	// Read and store glyph data
	font.glyphs = [];
	for (var i = 0; i < NUM_CHARS; i++) {
		var glyph_data_offset = font.charlist[i];
		if (glyph_data_offset !== 0xffff)
			font.glyphs[i] = readchar(i, font); // Pass font to readchar
		else font.glyphs[i] = null;
	}
	return font;
}
// based on table from https://www.roysac.com/blog/2014/04/thedraw-fonts-file-tdf-specifications/
var outline_charmap = {
	'A': 205,
	'B': 196,
	'C': 179,
	'D': 186,
	'E': 213,
	'F': 187,
	'G': 214,
	'H': 191,
	'I': 200,
	'J': 190,
	'K': 192,
	'L': 189,
	'M': 181,
	'N': 199,
	'O': 32,
	'@': 32,
	'&': 38,
};

function readchar(i, font) { // glyph argument is no longer needed, we return the glyph object
	var glyph_data_offset = font.charlist[i];
	// No need to check for 0xffff here, loadfont already does it and doesn't call readchar for those.
	var p = glyph_data_offset; // Pointer/offset into font.data
	var glyph = {}; // Use object for glyph_t
	try {
		glyph.width = font.data.charCodeAt(p);
		p++;
		glyph.height = font.data.charCodeAt(p);
		p++;
	}
	catch (e) {
		log("Error reading glyph dimensions for char index " + i + ": " + e);
		return null; // Return null or handle error appropriately
	}
	var row = 0;
	var col = 0;
	var width = glyph.width;
	var height = glyph.height;
	// Adjust overall font height if this glyph is taller
	if (height > font.height) {
		font.height = height;
	}
	// Initialize the cell array
	glyph.cell = [];
	for (var cell_idx = 0; cell_idx < width * font.height; cell_idx++) {
		glyph.cell[cell_idx] = { utfchar: ' ', color: 0 };
	}
	// Parse glyph data
	while (p < font.data.length && font.data.charCodeAt(p) !== 0x00) { // Loop until null terminator
		var ch = font.data.charCodeAt(p);
		p++;
		if (ch === 0x0d) { // Carriage return
			row++;
			col = 0;
		}
		else {
			if (p >= font.data.length) {
				log("Error reading color byte for char index " + i + " at offset " + (p - 1));
				break; // Prevent reading past data end
			}
			var color = font.fonttype == COLOR_FNT ? font.data.charCodeAt(p++) : LIGHTGRAY;
			if (ch < 0x20) { // Replace control characters with space (or '?')
				ch = ' ';
			}
			var cell_idx = row * width + col;
			if (cell_idx < glyph.cell.length) {
				if (font.fonttype == OUTLN_FNT)
					ch = outline_charmap[ascii(ch)];
				if (this.opt && opt.utf8) {
					try {
						glyph.cell[cell_idx].utfchar = utf8_encode(String.fromCharCode(ch));
					}
					catch (e) {
						log("Error converting CP437 to UTF-8 for char " + ch + ": " + e);
						glyph.cell[cell_idx].utfchar = '?'; // Fallback
					}
				}
				else {
					glyph.cell[cell_idx].utfchar = String.fromCharCode(ch); // Use ASCII/CP437 character directly
				}
				glyph.cell[cell_idx].color = color;
				col++;
			}
			else {
				if (font.fonttype != OUTLN_FNT)
					log("Warning: Exceeded glyph cell bounds for char index " + i + " at row " + row + ", col " + col);
				// This might indicate a font file issue or parsing error.
			}
		}
	}
	return glyph;
}

function lookupchar_code(c, font) {
	var char_code = c.charCodeAt(0); // Get the ASCII value of the character
	for (var i = 0; i < NUM_CHARS; i++) {
		// We need to find the index `i` in `charlist` that corresponds to `c`.
		// The C code uses `charlist[i] == c`.
		if (charlist.charCodeAt(i) === char_code) {
			// Check if this character is present in the font's charlist (not 0xffff)
			if (font.charlist[i] !== 0xffff) {
				return i; // Return the index in charlist (and glyphs array)
			}
			else {
				return -1; // Character is in charlist but not defined in font
			}
		}
	}
	return -1; // Character not found in charlist
}
// Lookup the uppercase char if lowercase char not mapped to font
function lookupchar(c, font) {
	var result = lookupchar_code(c, font);
	if (result == -1)
		result = lookupchar_code(c.toUpperCase(), font);
	return result;
}

function printcolor(color) {
	var fg = color & 0x0f;
	var bg = (color & 0xf0) >> 4;
	var out = "";
	if (this.opt == undefined || !opt.ansi) {
		var fgcolors = ["-K", "-B", "-G", "-C", "-R", "-M", "-Y", "-W", "HK", "HB", "HG", "HC", "HR", "HM", "HY", "HW"]; // Normal/Bright
		var bgcolors = ["0", "4", "2", "6", "1", "5", "3", "7"]; // Backgrounds (normal only for 8 colors)
		out += "\x01" + fgcolors[fg].split("").join("\x01");
		out += "\x01" + bgcolors[bg];
	}
	else {
		var fgcolors = [30, 34, 32, 36, 31, 35, 33, 37, 90, 94, 92, 96, 91, 95, 93, 97]; // Normal/Bright
		var bgcolors = [40, 44, 42, 46, 41, 45, 43, 47]; // Backgrounds (normal only for 8 colors)
		out += "\x1b[";
		out += (fgcolors[fg] + ";");
		out += (bgcolors[bg] + "m");
	}
	return out;
}

function printrow(glyph, row) {
	var utfchar;
	var color;
	var lastcolor = -1; // Use -1 or similar to ensure color is printed for the first cell
	var out = "";
	for (var i = 0; i < glyph.width; i++) {
		var cell_idx = glyph.width * row + i;
		if (cell_idx < glyph.cell.length) {
			utfchar = glyph.cell[cell_idx].utfchar;
			color = glyph.cell[cell_idx].color;
			if (i === 0 || color !== lastcolor) {
				out += printcolor(color);
				lastcolor = color;
			}
			out += utfchar;
		}
		else {
			// Should not happen if glyph.cell is initialized correctly, but for safety
			out += " ";
		}
	}
	out += reset_color();
	return out;
}

function reset_color() {
	// Reset color at the end of the row
	if (this.opt == undefined || !opt.ansi)
		return "\x01N";
	return "\x1b[0m";
}

function getlist() {
	return directory(FONT_DIR + "*." + FONT_EXT); // Get all .tdf files
}

function output(str, font) {
	var orgfont = font;
	while (true) {
		try {
			if (!font) { // Random font file selection
				var files = getlist();
				if (files.length > 0) {
					var randomIndex = random((files.length) + 1);
					font = file_getname(files[randomIndex]);
				}
			}
			if (typeof font == "string")
				font = loadfont(font);
			var width = getwidth(str, font);
			if (width > screen_width() || (this.opt && opt.wrap)) { // Word-wrap
				var array = str.split(/\s+/);
				if (array.length > 1) {
					var out = "";
					var word;
					while (word = array.shift()) {
						out += output_line(word, font);
						if (array.length && (!this.opt || opt.blankline !== false))
							out += "\r\n";
					}
					return out;
				}
			}
			return output_line(str, font);
		}
		catch (e) {
			if (!orgfont && this.opt && opt.retry === true) {
				if (opt.info)
					alert("exception: " + e);
				font = undefined;
				continue;
			}
			throw e;
		}
	}
}
// Calculate the total width of the string using the font
function getwidth(str, font) {
	var linewidth = 0;
	// Calculate the total width of the string using the font
	for (var i = 0; i < str.length; i++) {
		var char_index = lookupchar(str[i], font);
		if (char_index === -1) {
			continue; // Skip characters not found in the font
		}
		var g = font.glyphs[char_index];
		linewidth += g.width;
		if (i < str.length - 1) { // Add spacing between characters, but not after the last one
			linewidth += font.spacing;
		}
	}
	return linewidth;
}

function screen_width() {
	var width = this.opt && opt.width;
	if (!width) {
		if (js.global.console) // Auto-detect screen width, when possible
			width = console.screen_columns;
		else width = DEFAULT_WIDTH;
	}
	return width;
}

function output_line(str, font) {
	var maxheight = font.height; // Use the pre-calculated max height from loadfont
	var linewidth = getwidth(str, font);
	var len = str.length;
	var n = 0;
	var out = "";
	var width = screen_width();
	var margin = this.opt && opt.margin ? opt.margin : 0;
	var padding = margin;
	var justify = this.opt ? opt.justify : LEFT_JUSTIFY;
	// Calculate padding for justification
	if (justify === CENTER_JUSTIFY) {
		padding = Math.floor((width - linewidth) / 2);
	}
	else if (justify === RIGHT_JUSTIFY) {
		padding = width - (linewidth + padding);
	}
	padding = Math.max(0, padding);
	linewidth += padding;
	if (linewidth > width)
		throw new Error(format("Rendered line width (%u) > screen width (%u)", linewidth, width));
	// Print each row of the font text
	for (var i = 0; i < maxheight; i++) {
		// Print padding spaces
		for (var p = 0; p < padding; p++) {
			out += " ";
		}
		// Print glyphs for each character in the string
		for (var c = 0; c < len; c++) {
			var char_index = lookupchar(str[c], font);
			if (char_index === -1) {
				// If character not found, print spaces equivalent to default glyph width or 1?
				// Let's print spaces equal to the font's spacing + a minimal width (e.g., 1)
				for (var s = 0; s < font.spacing + 1; s++) {
					out += " ";
				}
				continue;
			}
			var g = font.glyphs[char_index];
			// printrow handles printing the characters and colors for a single row of the glyph
			out += printrow(g, i);
			// Print spacing between glyphs (except after the last glyph)
			if (c < len - 1) {
				for (var s = 0; s < font.spacing; s++) {
					out += " ";
				}
			}
		}
		// End the line and reset color
		out += reset_color();
		if (!(justify === RIGHT_JUSTIFY && padding == 0))
			out += "\r\n";
	}
	return out;
}

function printstr(str, font) {
	write(output(str, font));
}
this;