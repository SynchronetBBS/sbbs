// $Id: cterm_lib.js,v 1.24 2020/06/06 08:37:50 rswindell Exp $

// Library for dealing with CTerm/SyncTERM enhanced features (e.g. fonts)

load('sbbsdefs.js');
var xbin = load({}, 'xbin_defs.js');
var ansiterm = load({}, 'ansiterm_lib.js');

var cterm_version_supports_fonts = 1155;
var cterm_version_supports_mode_query = 1160;
var cterm_version_supports_fontstate_query = 1161;
var cterm_version_supports_palettes = 1167;
var cterm_version_supports_sixel = 1189;
var cterm_version_supports_fontdim_query = 1198;
var cterm_version_supports_ctda = 1207;
var cterm_version_supports_xtsrga = 1208;
var cterm_version_supports_b64_fonts = 1213;
var cterm_version_supports_copy_buffers = 1316;
var cterm_version_supports_jpegxl = 1318;
var font_slot_first = 43;
var font_slot_last = 255;
var font_styles = { normal:0, high:1, blink:2, highblink:3 };
var font_state_field_first = 0;
var font_state_field_result = 1;
var font_state_field_style = 2;
var da_ver_major = 0;
var da_ver_minor = 1;
var cterm_device_attributes = {
	valid:'0',
	loadable_fonts:'1',
	bright_background:'2',
	palette_settable:'3',
	pixelops_supported:'4',
	font_selectable:'5',
	extended_palette:'6',
	mouse_available:'7'
};

if(console.cterm_version === undefined || console.cterm_version < 0) {
	var response = query_da();
	if(response) {
		da_response = response.split(/;/);
		console.cterm_version = (parseInt(da_response[da_ver_major], 10)*1000) + parseInt(da_response[da_ver_minor], 10);
	}
}

if(console.cterm_fonts_loaded === undefined)
	console.cterm_fonts_loaded = [];

// Stash these in console, so they're persistent among multiple contexts
var version = console.cterm_version;
var fonts_loaded = console.cterm_fonts_loaded;
var fonts_active = console.cterm_fonts_active;

function query(request)
{
	var oldctrl = console.ctrlkey_passthru;
	console.ctrlkey_passthru=-1;

	console.write(request);
	var response='';

	var lastch;
	while(1) {
		var ch=console.inkey(0, 3000);
		if(ch=="")
			break;
		if(!response.length) {
			if(lastch == '\x1b' && ch == '[')
				response = '\x1b[';
			else {
				if(lastch)
					log(LOG_DEBUG, "CTerm Discarding: " + format("'%c' (%02X)", lastch, ascii(lastch)));
				lastch = ch;
			}
    		continue;
		}
		response += ch;
		if((ch < ' ' || ch > '/') && (ch < '0' || ch > '?'))
			break;
	}
	console.ctrlkey_passthru = oldctrl;
	var printable=response;
	printable=printable.replace(/\x1b/g,"");
	log(LOG_DEBUG, "CTerm query response: "+printable);
	return response;
}

/*
 * Like query() but you pass a string of intermediate bytes and final
 * byte, and it waits for and returns just that response.
 * 
 * It also requests a DSR to terminate the request, so we almost never
 * timeout.
 */
function query_fb(request, fb)
{
	var response='';
	var buf='';
	var lastch;
	var m;
	var i;
	var ch;
	var re;
	var printable;

	var oldctrl = console.ctrlkey_passthru;
	console.ctrlkey_passthru=-1;

	// Clean out the input buffer...
	console.clearkeybuffer()
	console.write(request);
	if (fb.slice(-1) != 'R') {
		console.write('\x1b[6n');
		re = new RegExp('^(.*?)(\\x1b\\[[0-?]*'+fb+')?\\x1b\\[[0-?]*R');
	}
	else {
		console.write('\x1b[5n');
		re = new RegExp('^(.*?)(\\x1b\\[[0-?]*'+fb+')?\\x1b\\[[0-?]*n');
	}
	while(1) {
		ch=console.inkey(0, 3000);
		if(ch=="")
			break;
		buf += ch;
		m = re.exec(buf);
		if (m !== null) {
			if (m[1].length > 0) {
				for (i = 0; i < m[1].length; i++)
					log(LOG_DEBUG, "CTerm Discarding: " + format("'%c' (%02X)", m[1].substr(i, 1), ascii(m[1].substr(i, 1))));
			}
			if (m[2] !== undefined)
				response = m[2];
			break;
		}
	}
	console.ctrlkey_passthru = oldctrl;
	var printable=response;
	printable=printable.replace(/\x1b/g,"");
	log(LOG_DEBUG, "CTerm query response: "+printable);
	return response;
}

function query_da()
{
	var response = query("\x1b[c");
	if(response && response.substr(0,21) == "\x1b[=67;84;101;114;109;" && response.substr(-1) == "c")
		response = response.slice(21, -1);
	else
		response = false;
	return response;
}

function query_fontstate(field)
{
	if(console.cterm_version < cterm_version_supports_fontstate_query)
		return undefined;

	var response = query("\x1b[=1n");
	if(!response)
		return response;
	if(response.substr(0,5) == "\x1b[=1;" && response.substr(-1) == "n") {
		console.cterm_font_state = response.slice(5, -1).split(/;/);
		font_slot_first = console.cterm_font_state[font_state_field_first];
		console.cterm_fonts_active = console.cterm_font_state.slice(font_state_field_style, font_state_field_style + 4)
		if(!field)
			return console.cterm_font_state;
		return console.cterm_font_state[field];
	}
	return false;
}

function query_mode(which)
{
	if(console.cterm_version < cterm_version_supports_mode_query)
		return false;

	var response = query("\x1b[=2n");
	if(!response)
		return response;
	if(response == "\x1b[=2n")
		return [];	// No ext_modes enabled
	if(response.substr(0,5) == "\x1b[=2;" && response.substr(-1) == "n") {
		var enabled_modes = response.slice(5, -1).split(/;/);
		if(which)
			return enabled_modes.indexOf(which) >= 0;
		return enabled_modes;
	}
	return false;
}

function query_fontdim()
{
	if(console.cterm_version < cterm_version_supports_fontdim_query)
		return false;

	var response = query("\x1b[=3n");
	if(!response)
		return response;
	if(response.substr(0,5) == "\x1b[=3;" && response.substr(-1) == "n")
		return response.slice(5, -1).split(/;/);
	return false;
}

function query_fontdims()
{
	if(console.cterm_version < cterm_version_supports_fontdim_query)
		return false;

	var response = query("\x1b[=3n");
	if(!response)
		return null;
	var m = response.match(/\x1b\[=3;([0-9]+);([0-9]+)n/);
	if (m == null)
		return null;
	return {height: parseInt(m[1], 10), width: parseInt(m[2], 10)};
}

function query_graphicsdim()
{
	if(console.cterm_version < cterm_version_supports_xtsrga)
		return null;
	var response = query("\x1b[?2;1S");
	if (!response)
		return null;
	var m = response.match(/\x1b\[\?2;0;([0-9]+);([0-9]+)S/);
	if (m === null)
		return null;
	return {width: parseInt(m[1], 10), height: parseInt(m[2], 10)};
}

// Resolve the terminal's character + pixel geometry for pixel-addressed protocols (e.g. Z-machine v6).
// Returns { cols, rows, cellW, cellH, pxW, pxH }. Cell size comes from query_fontdims() (ESC[=3n),
// falling back to { width: 8, height: charheight() } (the 8-px graphics cell; never the 9-px VGA text
// cell). Pixel canvas comes from query_graphicsdim() (ESC[?2;1S), falling back to cols*cellW x rows*cellH.
function cterm_screen_geometry()
{
	var cols = console.screen_columns || 80, rows = console.screen_rows || 24;
	var fd = query_fontdims();                        // { height, width } or null/false
	var cellW = (fd && fd.width)  ? fd.width  : 8;
	var cellH = (fd && fd.height) ? fd.height : charheight(rows);
	var gd = query_graphicsdim();                     // { width, height } or null
	var pxW = (gd && gd.width)  ? gd.width  : cols * cellW;
	var pxH = (gd && gd.height) ? gd.height : rows * cellH;
	return { cols: cols, rows: rows, cellW: cellW, cellH: cellH, pxW: pxW, pxH: pxH };
}

// Query the CTerm Device Attributes (CSI < c), which reflect the capabilities of the
// terminal's *current* video output mode (e.g. no pixelops/loadable-fonts in a text mode).
// The response is cached in console.cterm_da (persistent among multiple contexts,
// like cterm_version) so at most one round-trip is spent per session; a failure is cached
// too, so an unanswered query can't stall (3 seconds) more than once.
function query_ctda(which)
{
	if(console.cterm_da === undefined) {
		if(console.cterm_version == undefined || console.cterm_version < cterm_version_supports_ctda)
			console.cterm_da = false;
		else {
			var response = query("\x1b[<c");
			if(response.substr(0, 3) == "\x1b[<" && response.substr(-1) == "c")
				console.cterm_da = response.slice(3, -1).split(/;/);
			else
				console.cterm_da = false;
		}
	}
	if(console.cterm_da === false)
		return false;
	if(which !== undefined)
		return console.cterm_da.indexOf(which) >= 0;
	return console.cterm_da;
}

function fontsize(n)
{
	switch(n) {
		case 4096:
			return 0;
			break;
		case 3584:
			return 1;
			break;
		case 2048:
			return 2;
			break;
	}
	return -1;
}

function charheight(rows)
{
	if(console.cterm_charheight)
		return console.cterm_charheight;

	var dim = query_fontdim();
	if(dim && dim.length)
		return console.cterm_charheight = dim[0];	/* height */

	if(rows === undefined)
		rows = console.screen_rows;

	switch(rows) {
		case 27:
		case 28:
		case 33:
		case 34:
			return 14;
	}
	if(rows <= 30)
		return 16;
	return 8;
}

// This may return true, false, or undefined
// Returns true when we know for sure fonts are supported in the terminal (to the best of our knowledge).
// Returns false when we are pretty confident that fonts are *not* supported: either not CTerm,
// or the terminal's current video output mode doesn't support loadable/selectable fonts
// (e.g. Win32 Console, curses) per the CTerm Device Attributes.
// Returns undefined when we aren't really sure because it's a version of CTerm (e.g. SyncTERM 1.0)
// which didn't support the necessary queries.
function supports_fonts()
{
	if(console.cterm_version == undefined || console.cterm_version < cterm_version_supports_fonts)
		return false;
	if(console.cterm_font_state === undefined)
		query_fontstate();
	var setfont_result;
	if(console.cterm_font_state !== undefined && console.cterm_font_state[font_state_field_result] != undefined)
		setfont_result = parseInt(console.cterm_font_state[font_state_field_result], 10);
	if(setfont_result === 0)	// a font operation already succeeded in this mode
		return true;
	if(setfont_result > 0 && setfont_result != 99)	// a font operation already failed
		return false;
	// No font operation attempted yet (99 = CTERM_NO_SETFONT_REQUESTED) or state unknown:
	// the version alone can't tell us whether the current video output mode supports fonts,
	// so ask via the CTerm Device Attributes when the terminal is new enough to answer
	var attributes = query_ctda();
	if(attributes)
		return attributes.indexOf(cterm_device_attributes.loadable_fonts) >= 0
			&& attributes.indexOf(cterm_device_attributes.font_selectable) >= 0;
	return undefined;
}

// This may return true, false, or undefined
// Returns true/false per the CTerm Device Attributes, which reflect whether the current
// video output mode actually supports palette redefinition.
// Returns undefined for CTerm versions that support palettes but predate the CTDA query
// (can't verify the video output mode).
function supports_palettes()
{
	if(console.cterm_version == undefined || console.cterm_version < cterm_version_supports_palettes)
		return false;
	var attributes = query_ctda();
	if(attributes)
		return attributes.indexOf(cterm_device_attributes.palette_settable) >= 0;
	return undefined;
}

function supports_sixel()
{
	if(console.cterm_version == undefined || console.cterm_version < cterm_version_supports_sixel)
		return false;
	return query_ctda(cterm_device_attributes.pixelops_supported);
}

function supports_jpegxl()
{
	if(console.cterm_version == undefined || console.cterm_version < cterm_version_supports_jpegxl)
		return false;
	var res = query_fb('\x1b_SyncTERM:Q;JXL\x1b\\', '-n');
	if (res.indexOf('\x1b[=1;1-n') !== -1)
		return true;
	return false;
}

// Returns:
//	true:		font activation successful
//	undefined:	unsure if font activation was successful (e.g. SyncTERM 1.0)
//	number:		font activation failure (error number)
//	false:		incorrect usage
function activate_font(style, slot, wantblink)
{
	if(style == undefined) {
		log(LOG_WARNING, "CTerm activate_font: style is undefined");
		return false;
	}
	log(LOG_DEBUG, format("CTerm activate_font: %u %u", style, slot));
	load('sbbsdefs.js');
	var console_status = console.status;
	console.write(format("\x1b[%u;%u D", style, slot));
	var result = query_fontstate(font_state_field_result);
	if(result > 0) {
//		alert('fontstate : ' + result);
		return result;
	}

	if(slot)
		console.status |= (CON_NORM_FONT << style);
	else
		console.status &=~(CON_NORM_FONT << style);

	if(console.status != console_status) {
		if(console.status&(CON_BLINK_FONT|CON_HBLINK_FONT))
			console.write("\x1b[?34h\x1b[?35" + (wantblink === true ? "l":"h"));
		else
			console.write("\x1b[?34l\x1b[?35l");
		if(console.status&(CON_HIGH_FONT|CON_HBLINK_FONT))
			console.write("\x1b[?31h");
		else
			console.write("\x1b[?31l");
	}
	log(LOG_DEBUG, "CTerm activate_font result: " + result);
	if(result === '0')
		result = true;
	if(console.cterm_fonts_active === undefined)
		console.cterm_fonts_active = [];
	console.cterm_fonts_active[style] = slot;
	return result;
}

/* Used to set all 4 font styles, e.g. as copied from console.cterm_fonts_active */
function activate_fonts(slots)
{
	for(var i in slots) {
		activate_font(/* style */i, slots[i]);
	}
}

function load_font(slot, data, force)
{
	if(force != true && console.cterm_fonts_loaded[slot] == true) {
		log(LOG_DEBUG, format("CTerm load_font: slot %u already loaded", slot));
		return true;
	}
	log(LOG_DEBUG, format("CTerm load_font: slot %u with %u bytes", slot, data.length));
	load('sbbsdefs.js');
	if (console.cterm_version < cterm_version_supports_b64_fonts) {
		if(!(console.telnet_mode&TELNET_MODE_OFF)) {
			if(!console.telnet_cmd(TELNET_WILL, TELNET_BINARY_TX, 1000))
				mswait(100);	// Insure we enter binary mode *before* sending font data
		}
	}
	var fsize = fontsize(data.length);
	if(fsize < 0) {
		log(LOG_WARNING, format("CTerm Unsupported font file size: %lu bytes", data.length));
		return false;
	}
	if (console.cterm_version < cterm_version_supports_b64_fonts) {
		console.write(format("\x1b[=%u;%u{", slot, fsize));
		console.write(data);
	}
	else {
		console.write("\x1bPCTerm:Font:"+(slot)+":"+base64_encode(data)+"\x1b\\");
	}
	if(fsize == 1 && console.cterm_version < 1168)
		console.write("\x00\x00");	// Work-around cterm bug for 8x14 fonts
	if (console.cterm_version < cterm_version_supports_b64_fonts) {
		if(!(console.telnet_mode&TELNET_MODE_OFF))
			console.telnet_cmd(TELNET_WONT, TELNET_BINARY_TX);
	}
	console.cterm_fonts_loaded[slot] = true;
	return true;
}

function reset_palette()
{
	if(!this.supports_palettes())
		return false;
	return console.write("\x1b]104\x1b\\");
}

/* Palette is a 2-dim array: [16][3], each entry in R/G/B format */
/* Bit is the number of bits per color channel */
function redefine_palette(palette, bits)
{
	if(!this.supports_palettes())
		return false;
	if(!bits)	// per color channel (options are 4, 8, 12, or 16)
		bits = 8;
	var str = "\x1b]4";
	for(var i =0 ; i <  palette.length; i++)
		str += format(";%u;rgb:%0*X/%0*X/%0*X", i
			, bits / 4, palette[i][0]
			, bits / 4, palette[i][1]
			, bits / 4, palette[i][2]);
	str += "\x1b\\";
//	log(LOG_DEBUG, "CTerm New palette: " + str);
	console.write(str);
	return true;
}

// Scale from 6-bit to 8-bit RGB color channel
function scale_rgb_channel_value(val)
{
	val &= 0x3f;

	var newval = (val << 2) | (val >> 4);

//	log(LOG_DEBUG, format("scaled color channel from %d (%X) to %d (%x)", val, val, newval, newval));

	return newval;
}

function xbin_draw(image, xpos, ypos, fg_color, bg_color, delay, cycle)
{
	load('graphic.js');

	if(delay === undefined) {
		if(cycle)
			delay=250;
		else
			delay = 0;
	}

	console.clear(LIGHTGRAY|BG_BLACK);

	if(this.supports_fonts() != false && image.font && image.charheight == this.charheight()) {
		if(console.cterm_fonts_active)
			this.saved_fonts = console.cterm_fonts_active.slice();
		else
			this.saved_fonts = undefined;
		log(LOG_DEBUG, "CTerm saving active fonts slots: " + this.saved_fonts);
		for(var i = 0; i < image.font.length; i++)	{
	//		print("Loading font " + image.font[i].length + " bytes");
			this.load_font(0xff - i, image.font[i], true);
		}
		for(var p in this.font_styles) {
			var font_set = image[p];
			if(font_set == undefined) {
				this.activate_font(this.font_styles[p], 0);
				continue;
			}
	//		printf("font_set = %u\r\n", font_set);
			if(font_set < image.font_count) {
	//			print(format("Activating font " + font_set + " for " + p));
				if(this.activate_font(this.font_styles[p], 0xff - font_set) == false) {
					return "activate font failed";
				}
			}
		}
	}
	if(image.flags&xbin.FLAG_NONHIGH)
		ansiterm.send("ext_mode", "set", "no_bright_intensity");
	if(image.flags&xbin.FLAG_NONBLINK) {
		ansiterm.send("ext_mode", "set", "no_blink");
		if(!(console.status&(CON_BLINK_FONT|CON_HBLINK_FONT)))
			ansiterm.send("ext_mode", "set", "bg_bright_intensity");
	}

	if(image.palette && this.supports_palettes()) {
		var palette = [];
		for(var i = 0; i < 16; i++) {
			/* Must send the color definitions in the Xterm/ANSI order: */
			var offset;
			switch(i) {
				case 0:
					offset = BLACK;
					break;
				case 1:	/* Maroon */
					offset = RED;
					break;
				case 2:	/* Green */
					offset = GREEN;
					break;
				case 3: /* Olive */
					offset = BROWN;
					break;
				case 4:	/* Navy */
					offset = BLUE;
					break;
				case 5:	/* Purple */
					offset = MAGENTA;
					break;
				case 6:	/* Teal */
					offset = CYAN;
					break;
				case 7: /* Silver */
					offset = LIGHTGRAY;
					break;
				case 8:	/* Grey */
					offset = DARKGRAY;
					break;
				case 9:	/* Red */
					offset = LIGHTRED;
					break;
				case 10:	/* Lime */
					offset = LIGHTGREEN;
					break;
				case 11:
					offset = YELLOW;
					break;
				case 12:	/* Blue */
					offset = LIGHTBLUE;
					break;
				case 13:	/* Fuchsia */
					offset = LIGHTMAGENTA;
					break;
				case 14:	/* Aqua */
					offset = LIGHTCYAN;
					break;
				case 15:	/* White */
					offset = WHITE;
					break;
			}
			palette.push([
				scale_rgb_channel_value(image.palette[offset][0]),
				scale_rgb_channel_value(image.palette[offset][1]),
				scale_rgb_channel_value(image.palette[offset][2])
				]);
		}
		this.redefine_palette(palette);
	}
	
	ansiterm.send("ext_mode", "clear", "cursor");
	ansiterm.send("ext_mode", "clear", "autowrap");
//	log(LOG_DEBUG, 'CTerm Enabled modes ' + this.query_mode());
	var graphic = new Graphic(image.width, image.height);
	graphic.autowrap = false;
	graphic.BIN = image.bin;
	var width = image.width;
	var height = image.height;
	if(width > console.screen_columns)
		width = console.screen_columns;
	if(height > console.screen_rows)
		height = console.screen_rows;
	if(fg_color !== undefined || bg_color !== undefined)
		graphic.change_colors(fg_color, bg_color);
	var xoff = 0;
	var yoff = 0;
	var last_xoff,last_yoff;
	do {
		if(xoff && xoff + width > image.width)
			xoff = image.width - width;
		if(yoff && yoff + height > image.height)
			yoff = image.height - height;
		if(xoff < 0)
			xoff = 0;
		if(yoff < 0)
			yoff = 0;
		if(xoff != last_xoff || yoff != last_yoff) {	// Don't redraw redundantly
			try {
				graphic.draw(xpos, ypos, width, height, xoff, yoff);
			} catch(e) {
				log(LOG_WARNING, e);
				return e.toString();
			}
			last_xoff = xoff;
			last_yoff = yoff;
		}
//		console.write(graphic.ANSI);
//		console.print(graphic.MSG);
		if(cycle) {
			if(fg_color === undefined)
				fg_color = ansiterm.LIGHTGRAY;
			if(bg_color === undefined)
				bg_color = ansiterm.BLACK;
			fg_color++;
			if((fg_color&7) == (bg_color&7))
				bg_color--;
			graphic.change_colors(fg_color, bg_color);
		}
		var key;
		if(delay == 0)
			key = console.getkey(K_UPPER);
		else
			key = console.inkey(K_UPPER, delay);
		switch(key) {
			case '5':	/* middle */
				xoff = Math.floor(width / 2);
				yoff = Math.floor(height / 2);
				break;
			case '\b':
				yoff--;
				break;
			case '\r':
				yoff++;
				break;
			case '6':
				xoff++;
				break;
			case '4':
				xoff--;
				break;
			case '8':
				yoff--;
				break;
			case '7':
				yoff--;
				xoff--;
				break;
			case '9':
				yoff--;
				xoff++;
				break;
			case '3':
				yoff++;
				xoff++;
				break;
			case '1':
				yoff++;
				xoff--;
				break;
			case '2':
				yoff++;
				break;
			case KEY_PAGEUP:
				yoff -= height;
				break;
			case KEY_LEFT:
				xoff -= 10;
				break;
			case KEY_RIGHT:
				xoff += 10;
				break;
			case KEY_UP:
				yoff -= 5;
				break;
			case KEY_DOWN:
				yoff += 5;
				break;
			case KEY_PAGEDN:
				yoff += height;
				break;
			case KEY_HOME:
				xoff = 0;
				yoff = 0;
				break;
			case KEY_END:
				yoff = image.height - height;
				break;
			case 'Q':
				return false;
		}
		if(!key && !cycle)
			break;
	} while(!console.aborted);

	return true;
}

function xbin_cleanup(image)
{
	if(image && image.palette && this.supports_palettes())
		this.reset_palette();

	console.clear(ansiterm.LIGHTGRAY|ansiterm.BG_BLACK);
	ansiterm.send("ext_mode", "set", "cursor");
	ansiterm.send("ext_mode", "set", "autowrap");

	if(this.supports_fonts() != false
		&& (image===undefined || (image.font && image.charheight == this.charheight()))) {
		if(this.saved_fonts && this.saved_fonts.length) {
			log(LOG_DEBUG, "CTerm restoring active fonts slots: " + this.saved_fonts);
			activate_fonts(this.saved_fonts);
		} else
			for(var p in this.font_styles)
				if(image==undefined || image[p] < image.font_count)
					this.activate_font(this.font_styles[p], 0);
	}
	if(image==undefined || image.flags&xbin.FLAG_NONHIGH)
		ansiterm.send("ext_mode", "clear", "no_bright_intensity");
	if(image==undefined || image.flags&xbin.FLAG_NONBLINK) {
		ansiterm.send("ext_mode", "clear", "no_blink");
		ansiterm.send("ext_mode", "clear", "bg_bright_intensity");
	}
}

function bright_background(enable)
{
	// Skip only when the terminal positively reports (via CTDA) that the current video
	// output mode can't display bright backgrounds; non-CTerm terminals (which can't be
	// queried) may honor the (i)CE color sequences, so send by default
	var attributes = query_ctda();
	if(attributes && attributes.indexOf(cterm_device_attributes.bright_background) < 0)
		return false;
	var op = enable === false ? "clear" : "set";
	ansiterm.send("ext_mode", op, "bg_bright_intensity");
	ansiterm.send("ext_mode", op, "no_blink");
	return true;
}

// ---------------------------------------------------------------------------
// SyncTERM audio APCs ("SyncTERM:A" -- see cterm.adoc "Audio APCs").
//
// Model: SyncTERM holds 256 patch slots (decoded PCM) and mixes channels 2..15
// (0/1 are its own). Flow: cache a sound FILE once (audio_store), decode it into
// a slot (audio_load), then play the slot on a channel (audio_queue). Music loops;
// audio_volume adjusts a playing channel live; audio_synth makes a tone with no
// file at all.  Check supports_audio()/supports_audio_files() first.
//
// File formats are whatever the CLIENT's runtime libsndfile decodes: WAV, VOC,
// OGG/Opus and FLAC are safe everywhere; MP3 only on libsndfile >= 1.1.0. There
// is no per-format probe -- prefer OGG/WAV for portability. `data` is the file's
// raw bytes as a binary string (File.open("rb").read()).
// ---------------------------------------------------------------------------

var audio_chan_first = 2;      // channels 2..15 are APC-mixable (0/1 are cterm's)
var audio_chan_last  = 15;
var audio_slot_last  = 255;    // 256 patch slots (0..255)

var _audio_caps = undefined;

// -1 = no audio APC; 0 = audio APC but Synth tones only (no libsndfile);
//  1 = audio APC + libsndfile (can decode sound files). Cached after first query.
function query_audio()
{
	if(_audio_caps !== undefined)
		return _audio_caps;
	var r = query_fb('\x1b_SyncTERM:Q;libsndfile\x1b\\', 'n');
	var m = r.match(/\x1b\[=7;100;([01])n/);
	_audio_caps = m ? parseInt(m[1], 10) : -1;
	return _audio_caps;
}

// Synth/Queue usable (audio APC present, possibly Synth-tones only).
function supports_audio()
{
	return query_audio() >= 0;
}

// audio_load can decode sound files (client libsndfile present).
function supports_audio_files()
{
	return query_audio() === 1;
}

// --- low-level APC emitters (one per wire verb) ---

function _audio_apc(body)
{
	return '\x1b_SyncTERM:' + body + '\x1b\\';
}
function _audio_clamp(v, lo, hi)
{
	v = parseInt(v, 10);
	if(isNaN(v)) v = 0;
	return v < lo ? lo : (v > hi ? hi : v);
}

// Cache a complete sound FILE (`data` = its raw bytes as a binary string) under
// `name` for a later audio_load. libsndfile sniffs the format from the content.
function audio_store(name, data)
{
	console.write(_audio_apc('C;S;' + name + ';' + base64_encode(data)));
}

// Decode the cached `name` into patch slot `slot` (0..255).
function audio_load(slot, name)
{
	console.write(_audio_apc('A;Load;S=' + _audio_clamp(slot, 0, audio_slot_last) + ';' + name));
}

// Play patch `slot` on channel `ch` (2..15). opts: vol 0..100 (default 100),
// pan -100(left)..0..+100(right) (default 0), loop (default false).
function audio_queue(ch, slot, opts)
{
	if(opts === undefined) opts = {};
	var vol = (opts.vol === undefined) ? 100 : _audio_clamp(opts.vol, 0, 100);
	var pan = (opts.pan === undefined) ? 0 : _audio_clamp(opts.pan, -100, 100);
	var vl = pan > 0 ? Math.floor(vol * (100 - pan) / 100) : vol;
	var vr = pan < 0 ? Math.floor(vol * (100 + pan) / 100) : vol;
	console.write(_audio_apc('A;Queue;C=' + _audio_clamp(ch, audio_chan_first, audio_chan_last)
	    + ';S=' + _audio_clamp(slot, 0, audio_slot_last)
	    + ';VL=' + vl + ';VR=' + vr + (opts.loop ? ';L' : '')));
}

// Set channel `ch`'s live mix volume (0..100) -- adjusts a playing/looping sound.
function audio_volume(ch, vol)
{
	console.write(_audio_apc('A;Volume;C=' + _audio_clamp(ch, audio_chan_first, audio_chan_last)
	    + ';V=' + _audio_clamp(vol, 0, 100)));
}

// Set channel `ch`'s live per-side volume (each 0..100) -- live stereo balance.
function audio_volume_lr(ch, vl, vr)
{
	console.write(_audio_apc('A;Volume;C=' + _audio_clamp(ch, audio_chan_first, audio_chan_last)
	    + ';VL=' + _audio_clamp(vl, 0, 100) + ';VR=' + _audio_clamp(vr, 0, 100)));
}

// Synthesize a `ms`-ms, `freq`-Hz tone of waveform `shape` ("SINE", "SQUARE",
// "SAWTOOTH", ...) into slot `slot`. Works without libsndfile (supports_audio()).
function audio_synth(slot, shape, freq, ms)
{
	console.write(_audio_apc('A;Synth;S=' + _audio_clamp(slot, 0, audio_slot_last)
	    + ';W=' + shape + ';F=' + _audio_clamp(freq, 0, 0x7fffffff)
	    + ';T=' + _audio_clamp(ms, 0, 0x7fffffff)));
}

// Stop channel `ch`, with an optional `fade_ms` fade-out (0/omitted = abrupt).
function audio_flush(ch, fade_ms)
{
	var s = 'A;Flush;C=' + _audio_clamp(ch, audio_chan_first, audio_chan_last);
	if(fade_ms)
		s += ';O=' + _audio_clamp(fade_ms, 0, 0x7fffffff);
	console.write(_audio_apc(s));
}

// --- convenience layer: slot/channel allocator + upload-once cache ---

var _audio_slots = {};                         // name -> slot (upload once per session)
var _audio_next_slot = 0;
var audio_music_chan = audio_chan_first;       // channel reserved for play_music
var _audio_next_chan = audio_chan_first + 1;   // rotating SFX channel

function _audio_alloc_slot()
{
	var s = _audio_next_slot;
	_audio_next_slot = (_audio_next_slot + 1) % (audio_slot_last + 1);
	return s;
}

function _audio_alloc_chan()
{
	var c = _audio_next_chan;
	if(++_audio_next_chan > audio_chan_last)
		_audio_next_chan = audio_chan_first + 1;   // skip the reserved music channel
	return c;
}

// Store+load `name`/`data` exactly once per session; returns its slot.
function _audio_ensure(name, data)
{
	if(_audio_slots[name] === undefined) {
		audio_store(name, data);
		var slot = _audio_alloc_slot();
		audio_load(slot, name);
		_audio_slots[name] = slot;
	}
	return _audio_slots[name];
}

// Play a sound-effect file (see audio_store for formats): stores/loads once per
// `name`, then queues it on a rotating SFX channel. opts: vol/pan/loop, ch (force
// a channel). Returns { slot, ch }. Needs supports_audio_files().
function play_sound(name, data, opts)
{
	if(opts === undefined) opts = {};
	var slot = _audio_ensure(name, data);
	var ch = (opts.ch === undefined) ? _audio_alloc_chan() : opts.ch;
	audio_queue(ch, slot, opts);
	return { slot: slot, ch: ch };
}

// Play a synthesized tone (no file / libsndfile needed -- supports_audio()).
// opts: shape (default "SINE"), vol, pan, ch. Returns { slot, ch }.
function play_tone(freq, ms, opts)
{
	if(opts === undefined) opts = {};
	var slot = _audio_alloc_slot();
	audio_synth(slot, (opts.shape === undefined) ? 'SINE' : opts.shape, freq, ms);
	var ch = (opts.ch === undefined) ? _audio_alloc_chan() : opts.ch;
	audio_queue(ch, slot, opts);
	return { slot: slot, ch: ch };
}

// Play a music file looped on the reserved music channel. opts: vol, ch (override).
// Returns a handle { volume: function(0..100), stop: function([fade_ms]) } for a
// live volume slider / stop. Needs supports_audio_files().
function play_music(name, data, opts)
{
	if(opts === undefined) opts = {};
	var slot = _audio_ensure(name, data);
	var ch = (opts.ch === undefined) ? audio_music_chan : opts.ch;
	audio_queue(ch, slot, { vol: opts.vol, loop: true });
	return {
		volume: function(v) { audio_volume(ch, v); },
		stop:   function(fade) { audio_flush(ch, fade); }
	};
}

// Leave as last line:
this;
