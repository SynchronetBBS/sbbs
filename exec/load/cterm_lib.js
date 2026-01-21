// $Id: cterm_lib.js,v 1.24 2020/06/06 08:37:50 rswindell Exp $

// Library for dealing with CTerm/SyncTERM enhanced features (e.g. fonts)

load('sbbsdefs.js');
var xbin = load({}, 'xbin_defs.js');
var ansiterm = load({}, 'ansiterm_lib.js');

const cterm_version_supports_fonts = 1155;
const cterm_version_supports_mode_query = 1160;
const cterm_version_supports_fontstate_query = 1161;
const cterm_version_supports_palettes = 1167;
const cterm_version_supports_sixel = 1189;
const cterm_version_supports_fontdim_query = 1198;
const cterm_version_supports_xtsrga = 1208;
const cterm_version_supports_b64_fonts = 1213;
const cterm_version_supports_copy_buffers = 1316;
const cterm_version_supports_jpegxl = 1318;
var font_slot_first = 43;
const font_slot_last = 255;
const font_styles = { normal:0, high:1, blink:2, highblink:3 };
const font_state_field_first = 0;
const font_state_field_result = 1;
const font_state_field_style = 2;
const da_ver_major = 0;
const da_ver_minor = 1;
const cterm_device_attributes = {
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

function query_ctda(which)
{
	var response = query("\x1b[<c");
	if(response.substr(0, 3) != "\x1b[<" || response.substr(-1) != "c")
		return false;
	var attributes = response.slice(3, -1).split(/;/);
	if(which !== undefined)
		return attributes.indexOf(which) >= 0;
	return attributes;
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
// Returns false when we are pretty confident that fonts are *not* supported in the terminal (not CTerm).
// Returns undefined when we aren't really sure because it's a version of CTerm (e.g. SyncTERM 1.0)
// which didn't support queries
// ... and it may be running in a video output mode that doesn't support fonts (e.g. Win32 Console).
function supports_fonts()
{
	if(console.cterm_version == undefined || console.cterm_version < cterm_version_supports_fonts)
		return false;
	if(console.cterm_font_state === undefined)
		query_fontstate();
	if(console.cterm_font_state === undefined || console.cterm_font_state[font_state_field_result] == undefined)
		return undefined;
	var setfont_result = parseInt(console.cterm_font_state[font_state_field_result], 10);
	return setfont_result == 0 || setfont_result == 99;
}

// Right this function may return true when in fact the terminal is running in a video mode where
// palette redefinitions are not supported
function supports_palettes()
{
	if(console.cterm_version == undefined || console.cterm_version < cterm_version_supports_palettes)
		return false;
	return true;
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
	var op = enable === false ? "clear" : "set";
	ansiterm.send("ext_mode", op, "bg_bright_intensity");
	ansiterm.send("ext_mode", op, "no_blink");
}

// Leave as last line:
this;
