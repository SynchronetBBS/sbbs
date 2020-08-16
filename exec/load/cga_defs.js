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
							    /* background colors */
var   ANSI_NORMAL	=0x100;		/* special value for ansi() */
var   BG_BLACK		=0x200;		/* special value for ansi() */
var   BG_HIGH		=0x400;		/* not an ANSI.SYS compatible attribute */
var   BG_BLUE		=(BLUE<<4);
var   BG_GREEN		=(GREEN<<4);
var   BG_CYAN		=(CYAN<<4);
var   BG_RED		=(RED<<4);
var   BG_MAGENTA	=(MAGENTA<<4);
var   BG_BROWN		=(BROWN<<4);
var   BG_LIGHTGRAY	=(LIGHTGRAY<<4);

// Leave as last line:
this;
