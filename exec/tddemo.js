/*
 * tddemo.js
 * Gives a demo of what can be done with
 * the tdfonts_lib.js TheDraw fonts library.
 * 
 * Command out readln in pauseit() if you don't
 * want to have to press enter.
 *
 * Random font choices may not 100% dislay. This
 * is a work in progress but much better if you
 * add the opt.retry = true option.
 *
 * The end of the script has some commented out
 * items that you can optionally run by removing
 * the comments.
 *
 * @format.tab-size 4, @format.use-tabs true
 */

'use strict';

var tdf = load("tdfonts_lib.js");
var str = "Test";
tdf.opt = {};

var is_bbs = (js.global.console) ? true : false;

tdf.opt.ansi = false;
tdf.opt.utf8 = false;

function pauseit() {
	print("\r\nPress a key");
	readln();
}


if(is_bbs) {
	print("Testing BBS");
	tdf.printstr("Testing");
	pauseit();
}

if(!is_bbs) {
	print("Testing ANSI");
	tdf.opt.ansi = true;
	tdf.printstr("Testing");
	pauseit();


	print("Testing ANSI + UTF8");
	tdf.opt.utf8 = true;
	tdf.printstr("Testing");
	pauseit();
}

tdf.opt.wrap = true;
print("Testing NORMAL WRAP");
tdf.printstr("This is a Long Line, yes");
pauseit();

print("Testing ANSI + UTF8 + RIGHT JUSTIFY");
tdf.opt.justify = RIGHT_JUSTIFY;
tdf.printstr("Testing");
pauseit();


print("Testing ANSI + UTF8 + CENTER JUSTIFY");
tdf.opt.justify = CENTER_JUSTIFY;
tdf.printstr("Testing");
pauseit();

print("Testing ANSI + UTF8 + LEFT JUSTIFY");
tdf.opt.justify = LEFT_JUSTIFY;
tdf.printstr("Testing");
pauseit();

if(!is_bbs) {
	tdf.opt.width = 120;
	print("Testing ANSI + UTF8 + RIGHT JUSTIFY + WIDTH 120");
	tdf.opt.justify = RIGHT_JUSTIFY;
	tdf.printstr("Testing");
	pauseit();

	print("Testing ANSI + UTF8 + CENTER JUSTIFY + WIDTH 120");
	tdf.opt.justify = CENTER_JUSTIFY;
	tdf.printstr("Testing");
	pauseit();

	print("Testing ANSI + UTF8 + LEFT JUSTIFY + WIDTH 120");
	tdf.opt.justify = LEFT_JUSTIFY;
	tdf.printstr("Testing");
	pauseit();

	tdf.opt.margin = 10;
	print("Testing ANSI + UTF8 + RIGHT JUSTIFY + WIDTH 120 + MARGIN 10");
	tdf.opt.justify = RIGHT_JUSTIFY;
	tdf.printstr("Testing");
	pauseit();

	print("Testing ANSI + UTF8 + CENTER JUSTIFY + WIDTH 120 + MARGIN 10");
	tdf.opt.justify = CENTER_JUSTIFY;
	tdf.printstr("Testing");
	pauseit();

	print("Testing ANSI + UTF8 + LEFT JUSTIFY + WIDTH 120 + MARGIN 10");
	tdf.opt.justify = LEFT_JUSTIFY;
	tdf.printstr("Testing");
	pauseit();
}


print("Testing WRAP");
tdf.opt.margin = 0;
tdf.printstr("This is a Long Line, yes");
pauseit();

print("Testing WRAP RIGHT JUSTIFY");
tdf.opt.justify = RIGHT_JUSTIFY;
tdf.printstr("This is a Long Line, yes");
tdf.opt.justify = LEFT_JUSTIFY;
pauseit();


print("Testing WRAP BLANK font28x.tdf");
tdf.printstr("This is a Long Line, yes","font28x.tdf");
pauseit();

print("Testing WRAP NO BLANK font28x.tdf");
tdf.opt.blankline = false;
tdf.printstr("This is a Long Line, yes","font28x.tdf");
tdf.opt.blankline = true;
pauseit();

print("Testing WRAP BLANK font28x.tdf TEST 2");
tdf.printstr("This is a Long Line, yes","font28x.tdf");
pauseit();

tdf.opt.random = true;

print("Testing RANDOM 1");
tdf.printstr("Testing");
pauseit();

print("Testing RANDOM 2");
tdf.printstr("Testing");
pauseit();

print("Testing RANDOM 3");
tdf.printstr("Testing");
pauseit();

print("Testing RANDOM 1 INDEX 1");
tdf.opt.index = 1;
tdf.printstr("Testing");
pauseit();

print("Testing RANDOM 2 INDEX 2");
tdf.opt.index = 2;
tdf.printstr("Testing");
pauseit();

print("Testing RANDOM 3 INDEX 3");
tdf.opt.index = 3;
tdf.printstr("Testing");
pauseit();

print("Testing RANDOM 4 INDEX 4 INFO RETRY");
tdf.opt.index = 4;
tdf.opt.info = true;
tdf.opt.retry = true;
tdf.printstr("Testing");
pauseit();

print("Testing RANDOM 5 INDEX RANDOM INFO");
tdf.opt.index = "random";
tdf.opt.info = true;
tdf.opt.retry = true;
tdf.printstr("Testing");
pauseit();

tdf.opt.index = 1;
tdf.opt.info = false;
tdf.opt.retry = false;
tdf.opt.width = false;
tdf.opt.random = 1;
tdf.opt.wrap = false;
print("Testing LONG LINE NO FORCED WRAP");
tdf.printstr("ABCDEFGHIJKLMNOP");
pauseit();

print("Testing LONG LINE FORCED WRAP");
tdf.opt.wrap = true;
tdf.printstr("ABCDEFGHIJKLMNOP");
pauseit();

print("Testing LONG LINE FORCED WRAP NO BLANK");
tdf.opt.wrap = true;
tdf.opt.blankline = false;
tdf.printstr("ABCDEFGHIJKLMNOP");
pauseit();

print("Testing 1911 SINGLE FONT FONT RESET OPTIONS");
tdf.opt = {};
tdf.opt.info = true;
tdf.opt.ansi = true;
tdf.opt.utf8 = true;
tdf.printstr("Test","1911.tdf");
pauseit();

// If you have an alternate font path, you
// can try it here by uncommenting this
// section.
/*
var fp = "/home/bbs/THEDRAWFONTS/SETS/"
print("Testing FONTPATH RANDOM INFO");
tdf.opt.wrap = true;
tdf.opt.random = true;
tdf.opt.info = true;
tdf.opt.fontpath = fp;
tdf.printstr("Testing");
tdf.opt.fontpath = false;
pauseit();
*/


// This will get a list of font files in
// the fontpath. Useful to pull your own
// list.
/*
var fp = "/home/bbs/THEDRAWFONTS/SETS/"
print("Fetch list of fonts with FONTPATH");
tdf.opt.fontpath = fp;
var list = tdf.getlist(fp)
print(list);
pauseit();
*/


// Ths will produce a list of all the fonts in
// all the font files in the font path (which
// by default is /sbbs/ctrl/tdfonts but you
// can use tdf.opt.fontpath to specify another
// destination. It produces a LOT of fonts and
// a scrollback buffer of around 40,000 lines 
// may be necessary for large directories.

/*
tdf.opt.random = false;
tdf.opt.fontpath = false;
tdf.opt.info = true;
tdf.opt.pause = false;
tdf.opt.wrap = true;
tdf.display_all("Test Line");
pauseit();
*/




