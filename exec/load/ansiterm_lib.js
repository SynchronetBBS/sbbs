// $Id$
// vi: tabstop=4

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
	if((!(atr&HIGH) && curatr&HIGH) || (!(atr&BLINK) && curatr&BLINK)
		|| atr==LIGHTGRAY) {
		str += "0;";
		curatr=LIGHTGRAY;
	}
	if(atr&BLINK) {                     /* special attributes */
		if(!(curatr&BLINK))
			str += "5;";
	}
	if(atr&HIGH) {
		if(!(curatr&HIGH))
			str += "1;";
	}
	if((atr&0x07) != (curatr&0x07)) {
		switch(atr&0x07) {
			case BLACK:
				str += "30;";
				break;
			case RED:
				str += "31;";
				break;
			case GREEN:
				str += "32;";
				break;
			case BROWN:
				str += "33;";
				break;
			case BLUE:
				str += "34;";
				break;
			case MAGENTA:
				str += "35;";
				break;
			case CYAN:
				str += "36;";
				break;
			case LIGHTGRAY:
				str += "37;";
				break;
		}
	}
	if((atr&0x70) != (curatr&0x70)) {
		switch(atr&0x70) {
			/* The BG_BLACK macro is 0x200, so isn't in the mask */
			case 0 /* BG_BLACK */:
				str += "40;";
				break;
			case BG_RED:
				str += "41;";
				break;
			case BG_GREEN:
				str += "42;";
				break;
			case BG_BROWN:
				str += "43;";
				break;
			case BG_BLUE:
				str += "44;";
				break;
			case BG_MAGENTA:
				str += "45;";
				break;
			case BG_CYAN:
				str += "46;";
				break;
			case BG_LIGHTGRAY:
				str += "47;";
				break;
		}
	}
	if(str.length <= 2)	/* Convert <ESC>[ to blank */
		return "";
	return str.substring(0, str.length-1) + 'm';
}

var ext_mode = {
	set: 	function(mode) 	{ return format("\x1b[?%uh", defs.ext_mode[mode]); },
	clear:	function(mode) 	{ return format("\x1b[?%ul", defs.ext_mode[mode]); },
	save:	function(mode)	{ return format("\x1b[?%us", defs.ext_mode[mode]); },
	restore: function(mode) { return format("\x1b[?%uu", defs.ext_mode[mode]); }
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

var color = true;

function set_attributes(a)
{
	var str = attr(a, this.attributes.current, this.color);
	this.attributes.current = a;
	return str;
}

function send(a,b,c,d)
{
	console.write(this[a][b](c,d));
}

/* Leave as last line for convenient load() usage: */
this;
