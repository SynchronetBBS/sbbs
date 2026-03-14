// ttriv_ui.js - Tournament Trivia UI helpers for Synchronet BBS
// Display functions for CP437 box-drawing and text formatting

"use strict";

require("cp437_defs.js", "CP437_MEDIUM_SHADE");

// Print text with Synchronet Ctrl-A color prefix and optional newlines
// newlines: 0=none, 1=one CRLF (default), 2=double CRLF
function printColor(text, ctrlA, newlines)
{
	if (newlines === undefined)
		newlines = 1;
	console.print(ctrlA + text);
	for (var i = 0; i < newlines; ++i)
		console.crlf();
}

// Print centered text with color
function centerText(text, ctrlA)
{
	var len = text.length;
	if (len >= 79)
	{
		printColor(text, ctrlA);
		return;
	}
	var pad = 40 - Math.floor(len / 2);
	var spaces = "";
	for (var i = 0; i < pad; ++i) spaces += " ";
	console.print("\x01N\x01W" + spaces);
	printColor(text, ctrlA);
}

// Display text in a double-line CP437 box
function textBox(title, textCtrlA, boxCtrlA, centered)
{
	if (title.length < 1 || title.length > 75)
		return;

	var padding = "";
	if (centered)
	{
		var padLen = 38 - Math.floor(title.length / 2);
		for (var i = 0; i < padLen; ++i) padding += " ";
	}

	// Build horizontal bar
	var hbar = "";
	for (var i = 0; i < title.length + 2; ++i)
		hbar += CP437_BOX_DRAWINGS_HORIZONTAL_DOUBLE;

	// Top: corner + bar + corner
	if (centered) console.print("\x01N\x01W" + padding);
	printColor(CP437_BOX_DRAWINGS_UPPER_LEFT_DOUBLE + hbar + CP437_BOX_DRAWINGS_UPPER_RIGHT_DOUBLE, boxCtrlA);

	// Middle: side + title + side
	if (centered) console.print("\x01N\x01W" + padding);
	printColor(CP437_BOX_DRAWINGS_DOUBLE_VERTICAL + " ", boxCtrlA, 0);
	printColor(title, textCtrlA, 0);
	printColor(" " + CP437_BOX_DRAWINGS_DOUBLE_VERTICAL, boxCtrlA);

	// Bottom: corner + bar + corner
	if (centered)
		console.print("\x01N\x01W" + padding);
	printColor(CP437_BOX_DRAWINGS_LOWER_LEFT_DOUBLE + hbar + CP437_BOX_DRAWINGS_LOWER_RIGHT_DOUBLE, boxCtrlA, 2);
}

// Display text with an underline character repeated to match width
function underlineText(text, ulChar, textCtrlA, ulCtrlA, centered)
{
	var ul = "";
	while (ul.length < text.length)
		ul += ulChar;
	ul = ul.substring(0, text.length);

	if (centered)
	{
		centerText(text, textCtrlA);
		centerText(ul, ulCtrlA);
	}
	else
	{
		printColor(text, textCtrlA);
		printColor(ul, ulCtrlA);
	}
}

// Display a menu option: " K > text"
function menuOption(key, text, keyCtrlA, arrowCtrlA, textCtrlA)
{
	printColor(" " + key, keyCtrlA, 0);
	printColor(" " + CP437_BLACK_RIGHT_POINTING_POINTER + " ", arrowCtrlA, 0);
	printColor(text, textCtrlA);
}
