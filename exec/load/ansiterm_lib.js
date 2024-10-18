/* Example usage:
	var ansi = load({}, 'ansiterm_lib.js');
	ansi.send('ext_mode', 'set', 'bg_bright_intensity');
	ansi.send('attributes', 'set', ansi.BG_RED|ansi.BLINK);
	ansi.send('screen', 'clear');
*/

load("cga_defs.js");

const defs = {
	// SyncTerm extended modes
	ext_mode: {
		origin: 				6,
		autowrap: 				7,
		cursor:					25,
		brght_alt_charset:		31,
		no_bright_intensity:	32,
		bg_bright_intensity:	33,
		blink_alt_charset:		34,
		no_blink:				35,
	},
	
	mouse_reporting: {
		x10_compatible:			9,
		normal_tracking:		1000,
		highlight_tracking:		1001,	// Not supported by SyncTERM
		button_events:			1002,
		any_events:				1003,
		focus_event_tracking:	1004,	// Not supported by SyncTERM
		utf8_extended_coord:	1005,	// Not supported by SyncTERM
		extended_coord:			1006,	// modifies the above modes
		alt_scroll_mode:		1007,	// Not supported by SyncTERM
		urxvt_extended_coord:	1015,	// Not supported by SyncTERM
	},

	// SyncTerm emulation speed map
	speed_map: {
		unlimited:				0,
		300:					1,
		600:					2,
		1200:					3,
		2400:					4,
		4800:					5,
		9600:					6,
		19200:					7,
		38400:					8,
		57600:					9,
		76800:					10,
		115200:					11,
	},

	// standard
	cursor_move: {
		up:						'A',
		down:					'B',
		right:					'C',
		left:					'D',
	},
	scroll_dir: {
		up:						'S',
		down:					'T',
	},
	portion: {
		to_end:					0,
		to_start:				1,
		entire:					2,
	},
	attr: {
		normal: 				0,
		high: 					1,
		blink:					5,
		fg: { // aligned with cga_defs.colors[]
			BLACK:				30,
			RED:				31,
			GREEN:				32,
			BROWN:				33,
			BLUE:				34,
			MAGENTA:			35,
			CYAN:				36,
			LIGHTGRAY:			37,
		},
		bg: { // aligned with cga_defs.colors[]
			BLACK:				40,
			RED:				41,
			GREEN:				42,
			BROWN:				43,
			BLUE:				44,
			MAGENTA:			45,
			CYAN:				46,
			LIGHTGRAY:			47,
		},
	},
};

function attr(atr, curatr, color)
{
    var str="";
    if(curatr==undefined)
        curatr=0;

	if(color==false) {  /* eliminate colors if terminal doesn't support them */
		if(atr&LIGHTGRAY)       /* if any foreground bits set, set all */
			atr|=LIGHTGRAY;
		if(atr&BG_LIGHTGRAY)  /* if any background bits set, set all */
			atr|=BG_LIGHTGRAY;
		if((atr&LIGHTGRAY) && (atr&BG_LIGHTGRAY))
			atr&=~LIGHTGRAY;    /* if background is solid, foreground is black */
		if(!atr)
			atr|=LIGHTGRAY;		/* don't allow black on black */
	}
	if(curatr==atr) { /* text hasn't changed. no sequence needed */
		return str;
	}

	str = "\x1b[";
	if(atr == ANSI_NORMAL)
		return str + defs.attr.normal + "m";

	if((!(atr&HIGH) && curatr&HIGH) || (!(atr&BLINK) && curatr&BLINK)
		|| atr==LIGHTGRAY) {
		str += defs.attr.normal + ";";
		curatr=LIGHTGRAY;
	}
	if(atr&BLINK) {                     /* special attributes */
		if(!(curatr&BLINK))
			str += defs.attr.blink + ";";
	}
	if(atr&HIGH) {
		if(!(curatr&HIGH))
			str += defs.attr.high + ";";
	}
	if((atr&0x07) != (curatr&0x07)) {
		str += defs.attr.fg[colors[atr&0x07]] + ";";
	}
	if((atr&0x70) != (curatr&0x70)) {
		str += defs.attr.bg[colors[(atr&0x70) >> 4]] + ";";
	}
	if(str.length <= 2)	/* Convert <ESC>[ to blank */
		return "";
	return str.substring(0, str.length-1) + 'm';
}

var ext_mode = {
	set: 	function(mode) 	{ return format("\x1b[?%uh", defs.ext_mode[mode]); },
	clear:	function(mode) 	{ return format("\x1b[?%ul", defs.ext_mode[mode]); },
	save:	function(mode)	{ return format("\x1b[?%us", defs.ext_mode[mode]); },
	restore: function(mode) { return format("\x1b[?%uu", defs.ext_mode[mode]); },
	save_all:		function()	{ return "\x1b[?s"; },
	restore_all:	function()	{ return "\x1b[?u"; }
}

function mouse_reporting_modes(mode)
{
	var list = [];
	if(mode == 'all')
		for(var i in defs.mouse_reporting)
			list.push(defs.mouse_reporting[i]);
	else if(typeof mode == 'array')
		for(var i in mode)
			list.push(defs.mouse_reporting[mode[i]]);
	else
		list.push(defs.mouse_reporting[mode]);
	return list.join(";");
}

var mouse = {
	set: 	function(mode)	{ return format("\x1b[?%sh", mouse_reporting_modes(mode)); },
	clear: 	function(mode)	{ return format("\x1b[?%sl", mouse_reporting_modes(mode)); }
}

var speed = {
	set: 	function(rate)	{ return format("\x1b[;%u*r", rate < 300 ? rate : defs.speed_map[rate]); },
	clear:	function()		{ return "\x1b[*r"; }
}

var cursor_position = {
	move:	function(dir,n)	{ return format("\x1b[%s%s", n ? n : "", defs.cursor_move[dir]); },
	set:	function(x,y)	{ return format("\x1b[%u;%uH", y, x); },
	home:	function()		{ return "\x1b[H"; },
	save:	function()		{ return "\x1b[s"; },
	restore: function()		{ return "\x1b[u"; }
}

var screen = {
	scroll: function(dir,n)	{ return format("\x1b[%s%s", n ? n : "", defs.scroll_dir[dir]); },
	clear: function(p)		{ return format("\x1b[%uJ", p ? defs.portion[p] : defs.portion.entire); }
}

var line = {
	clear: function(p)		{ return format("\x1b[%uK", p ? defs.portion[p] : defs.portion.entire); },
	insert: function(n)		{ return format("\x1b[%sL", n ? n : ""); },
	remove: function(n)		{ return format("\x1b[%sM", n ? n : ""); }
}

var attributes = {
	current: 0,
	set: function(a) 		{ return set_attributes(a); }
}

var state = {
	reset: function()		{ return "\x1bc"; }
}

var color = true;

function set_attributes(a)
{
	var str = attr(a, this.attributes.current, this.color);
	this.attributes.current = a;
	return str;
}

function send(a,b,c,d)
{
	log(LOG_DEBUG, "ansiterm.sending: " + this[a][b](c,d));
	console.write(this[a][b](c,d));
}

function expand_ctrl_a(str)
{
	return str.replace(/\x01./g, function(seq) {
		var cga_val = from_attr_code[seq[1].toUpperCase()];
		if(cga_val === undefined)
			return "";
		return attr(cga_val);
	});
}

/* Leave as last line for convenient load() usage: */
this;
