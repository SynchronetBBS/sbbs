// $Id: xbin_defs.js,v 1.1 2018/02/02 12:48:35 rswindell Exp $

// Xbin file format definition
// https://web.archive.org/web/20120204063040/http://www.acid.org/info/xbin/x_spec.htm

var id = "XBIN\x1a"
var ID_LEN					= 5;
var PALETTE_LEN				= 48;
var FLAG_PALETTE			= (1<<0);
var FLAG_FONT_NORMAL		= (1<<1);
var FLAG_COMPRESSED			= (1<<2);
var FLAG_NONBLINK			= (1<<3);	// The blink attribute flag does not enable blinking text
var FLAG_FONT_HIGH			= (1<<4);	// Also called "512Chars"
// Synchronet-extensions:
var FLAG_FONT_BLINK			= (1<<5);	// Alt-font selected via BLINK attribute included	
var FLAG_FONT_HIGHBLINK		= (1<<6);	// Alt-font selected via HIGH and BLINK attributes included
var FLAG_NONHIGH			= (1<<7);	// The HIGH attribute flag has no effect on the color

// The order of font (character set) definitions in the file has this precedence:
// BLINK
// HIGHBLINK
// NORMAL
// HIGH

this;
