// $Id: cga_defs.js,v 1.5 2019/09/21 22:48:28 rswindell Exp $
// CGA (IBM Color Graphics Adapter) definitions

								/********************************************/
							    /* console.attributes, also used for ansi()	*/
							    /********************************************/
var   BLINK			=0x80;		/* blink bit */
var   HIGH			=0x08;		/* high intensity (bright) foreground bit */

							    /* foreground colors */
var   BLACK			=0;			/* dark colors (HIGH bit unset) */
var   BLUE			=1;
var   GREEN			=2;
var   CYAN			=3;
var   RED			=4;
var   MAGENTA		=5;
var   BROWN			=6;
var   LIGHTGRAY		=7;
var   DARKGRAY		=8;			/* light colors (HIGH bit set) */
var   LIGHTBLUE		=9;
var   LIGHTGREEN	=10;
var   LIGHTCYAN		=11;
var   LIGHTRED		=12;
var   LIGHTMAGENTA	=13;
var   YELLOW		=14;
var   WHITE			=15;

// This array allows a fast color-index -> name lookup
var colors = [
	'BLACK',
	'BLUE',
	'GREEN',
	'CYAN',
	'RED',
	'MAGENTA' ,
	'BROWN',
	'LIGHTGRAY',
	'DARKGRAY',
	'LIGHTBLUE',
	'LIGHTGREEN', 
	'LIGHTCYAN',
	'LIGHTRED',
	'LIGHTMAGENTA',
	'YELLOW',
	'WHITE'
	];
var   FG_UNKNOWN	=0x100;
var   BG_BLACK		=0x200;		/* special value for ansi() */
var   BG_HIGH		=0x400;		/* not an ANSI.SYS compatible attribute */
var   REVERSED		=0x800;
var   UNDERLINE		=0x1000;
var   CONCEALED		=0x2000;
var   BG_UNKNOWN	=0x4000;
var   ANSI_NORMAL	=(FG_UNKNOWN | BG_UNKNOWN);
							    /* background colors */
var   BG_BLUE		=(BLUE<<4);
var   BG_GREEN		=(GREEN<<4);
var   BG_CYAN		=(CYAN<<4);
var   BG_RED		=(RED<<4);
var   BG_MAGENTA	=(MAGENTA<<4);
var   BG_BROWN		=(BROWN<<4);
var   BG_LIGHTGRAY	=(LIGHTGRAY<<4);

// Map Synchronet Ctrl-A attribute code to CGA color value
var from_attr_code = {
	'K': BLACK,
	'R': RED,
	'G': GREEN,
	'Y': YELLOW,
	'B': BLUE,
	'M': MAGENTA,
	'C': CYAN,
	'W': LIGHTGRAY,
	'0': BG_BLACK,
	'1': BG_RED,
	'2': BG_GREEN,
	'3': BG_BROWN,
	'4': BG_BLUE,
	'5': BG_MAGENTA,
	'6': BG_CYAN,
	'7': BG_LIGHTGRAY,
	'H': HIGH,
	'I': BLINK,
	'-': ANSI_NORMAL,
	'_': ANSI_NORMAL,
	'N': ANSI_NORMAL,
};

// Leave as last line:
this;
