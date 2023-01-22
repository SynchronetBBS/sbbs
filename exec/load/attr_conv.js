// This file specifies some functions for converting attribute codes from
// other systems to Synchronet attribute codes.  The functions all take a
// string and return a string:
//
// function WWIVAttrsToSyncAttrs(pText)
// function PCBoardAttrsToSyncAttrs(pText)
// function wildcatAttrsToSyncAttrs(pText)
// function celerityAttrsToSyncAttrs(pText)
// function renegadeAttrsToSyncAttrs(pText)
// function ANSIAttrsToSyncAttrs(pText)
// function convertAttrsToSyncPerSysCfg(pText)
//
// Author: Eric Oulashin (AKA Nightfox)
// BBS: Digital Distortion
// BBS address: digitaldistortionbbs.com (or digdist.synchro.net)


"use strict";

require("sbbsdefs.js", "SYS_RENEGADE");

/////////////////////////////////////////////////////////////////////////
// Functions for converting other BBS color codes to Synchronet attribute codes

// Converts WWIV attribute codes to Synchronet attribute codes.
//
// Parameters:
//  pText: A string containing the text to convert
//
// Return value: The text with the color codes converted
function WWIVAttrsToSyncAttrs(pText)
{
	// First, see if the text has any WWIV-style attribute codes at
	// all.  We'll be performing a bunch of search & replace commands,
	// so we don't want to do all that work for nothing.. :)
	if (/\x03[0-9]/.test(pText))
	{
		var text = pText.replace(/\x030/g, "\x01n");			// Normal
		text = text.replace(/\x031/g, "\x01n\x01c\x01h");		// Bright cyan
		text = text.replace(/\x032/g, "\x01n\x01y\x01h");		// Bright yellow
		text = text.replace(/\x033/g, "\x01n\x01m");			// Magenta
		text = text.replace(/\x034/g, "\x01n\x01h\x01w\x01" + "4");	// Bright white on blue
		text = text.replace(/\x035/g, "\x01n\x01g");			// Green
		text = text.replace(/\x036/g, "\x01h\x01r\x01i");		// Bright red, blinking
		text = text.replace(/\x037/g, "\x01n\x01h\x01b");		// Bright blue
		text = text.replace(/\x038/g, "\x01n\x01b");			// Blue
		text = text.replace(/\x039/g, "\x01n\x01c");			// Cyan
		return text;
	}
	else
		return pText; // No WWIV-style color attribute found, so just return the text.
}

// Converts PCBoard attribute codes to Synchronet attribute codes.
//
// Parameters:
//  pText: A string containing the text to convert
//
// Return value: The text with the color codes converted
function PCBoardAttrsToSyncAttrs(pText)
{
	// First, see if the text has any PCBoard-style attribute codes at
	// all.  We'll be performing a bunch of search & replace commands,
	// so we don't want to do all that work for nothing.. :)
	if (/@[xX][0-9A-Fa-f]{2}/.test(pText))
	{
		// Black background
		var text = pText.replace(/@[xX]00/g, "\x01n\x01k\x01" + "0"); // Black on black
		text = text.replace(/@[xX]01/g, "\x01n\x01b\x01" + "0"); // Blue on black
		text = text.replace(/@[xX]02/g, "\x01n\x01g\x01" + "0"); // Green on black
		text = text.replace(/@[xX]03/g, "\x01n\x01c\x01" + "0"); // Cyan on black
		text = text.replace(/@[xX]04/g, "\x01n\x01r\x01" + "0"); // Red on black
		text = text.replace(/@[xX]05/g, "\x01n\x01m\x01" + "0"); // Magenta on black
		text = text.replace(/@[xX]06/g, "\x01n\x01y\x01" + "0"); // Yellow/brown on black
		text = text.replace(/@[xX]07/g, "\x01n\x01w\x01" + "0"); // White on black
		text = text.replace(/@[xX]08/g, "\x01n\x01w\x01" + "0"); // White on black
		text = text.replace(/@[xX]09/g, "\x01n\x01w\x01" + "0"); // White on black
		text = text.replace(/@[xX]08/g, "\x01h\x01k\x01" + "0"); // Bright black on black
		text = text.replace(/@[xX]09/g, "\x01h\x01b\x01" + "0"); // Bright blue on black
		text = text.replace(/@[xX]0[Aa]/g, "\x01h\x01g\x01" + "0"); // Bright green on black
		text = text.replace(/@[xX]0[Bb]/g, "\x01h\x01c\x01" + "0"); // Bright cyan on black
		text = text.replace(/@[xX]0[Cc]/g, "\x01h\x01r\x01" + "0"); // Bright red on black
		text = text.replace(/@[xX]0[Dd]/g, "\x01h\x01m\x01" + "0"); // Bright magenta on black
		text = text.replace(/@[xX]0[Ee]/g, "\x01h\x01y\x01" + "0"); // Bright yellow on black
		text = text.replace(/@[xX]0[Ff]/g, "\x01h\x01w\x01" + "0"); // Bright white on black
		// Blinking foreground

		// Blue background
		text = text.replace(/@[xX]10/g, "\x01n\x01k\x01" + "4"); // Black on blue
		text = text.replace(/@[xX]11/g, "\x01n\x01b\x01" + "4"); // Blue on blue
		text = text.replace(/@[xX]12/g, "\x01n\x01g\x01" + "4"); // Green on blue
		text = text.replace(/@[xX]13/g, "\x01n\x01c\x01" + "4"); // Cyan on blue
		text = text.replace(/@[xX]14/g, "\x01n\x01r\x01" + "4"); // Red on blue
		text = text.replace(/@[xX]15/g, "\x01n\x01m\x01" + "4"); // Magenta on blue
		text = text.replace(/@[xX]16/g, "\x01n\x01y\x01" + "4"); // Yellow/brown on blue
		text = text.replace(/@[xX]17/g, "\x01n\x01w\x01" + "4"); // White on blue
		text = text.replace(/@[xX]18/g, "\x01h\x01k\x01" + "4"); // Bright black on blue
		text = text.replace(/@[xX]19/g, "\x01h\x01b\x01" + "4"); // Bright blue on blue
		text = text.replace(/@[xX]1[Aa]/g, "\x01h\x01g\x01" + "4"); // Bright green on blue
		text = text.replace(/@[xX]1[Bb]/g, "\x01h\x01c\x01" + "4"); // Bright cyan on blue
		text = text.replace(/@[xX]1[Cc]/g, "\x01h\x01r\x01" + "4"); // Bright red on blue
		text = text.replace(/@[xX]1[Dd]/g, "\x01h\x01m\x01" + "4"); // Bright magenta on blue
		text = text.replace(/@[xX]1[Ee]/g, "\x01h\x01y\x01" + "4"); // Bright yellow on blue
		text = text.replace(/@[xX]1[Ff]/g, "\x01h\x01w\x01" + "4"); // Bright white on blue

		// Green background
		text = text.replace(/@[xX]20/g, "\x01n\x01k\x01" + "2"); // Black on green
		text = text.replace(/@[xX]21/g, "\x01n\x01b\x01" + "2"); // Blue on green
		text = text.replace(/@[xX]22/g, "\x01n\x01g\x01" + "2"); // Green on green
		text = text.replace(/@[xX]23/g, "\x01n\x01c\x01" + "2"); // Cyan on green
		text = text.replace(/@[xX]24/g, "\x01n\x01r\x01" + "2"); // Red on green
		text = text.replace(/@[xX]25/g, "\x01n\x01m\x01" + "2"); // Magenta on green
		text = text.replace(/@[xX]26/g, "\x01n\x01y\x01" + "2"); // Yellow/brown on green
		text = text.replace(/@[xX]27/g, "\x01n\x01w\x01" + "2"); // White on green
		text = text.replace(/@[xX]28/g, "\x01h\x01k\x01" + "2"); // Bright black on green
		text = text.replace(/@[xX]29/g, "\x01h\x01b\x01" + "2"); // Bright blue on green
		text = text.replace(/@[xX]2[Aa]/g, "\x01h\x01g\x01" + "2"); // Bright green on green
		text = text.replace(/@[xX]2[Bb]/g, "\x01h\x01c\x01" + "2"); // Bright cyan on green
		text = text.replace(/@[xX]2[Cc]/g, "\x01h\x01r\x01" + "2"); // Bright red on green
		text = text.replace(/@[xX]2[Dd]/g, "\x01h\x01m\x01" + "2"); // Bright magenta on green
		text = text.replace(/@[xX]2[Ee]/g, "\x01h\x01y\x01" + "2"); // Bright yellow on green
		text = text.replace(/@[xX]2[Ff]/g, "\x01h\x01w\x01" + "2"); // Bright white on green

		// Cyan background
		text = text.replace(/@[xX]30/g, "\x01n\x01k\x01" + "6"); // Black on cyan
		text = text.replace(/@[xX]31/g, "\x01n\x01b\x01" + "6"); // Blue on cyan
		text = text.replace(/@[xX]32/g, "\x01n\x01g\x01" + "6"); // Green on cyan
		text = text.replace(/@[xX]33/g, "\x01n\x01c\x01" + "6"); // Cyan on cyan
		text = text.replace(/@[xX]34/g, "\x01n\x01r\x01" + "6"); // Red on cyan
		text = text.replace(/@[xX]35/g, "\x01n\x01m\x01" + "6"); // Magenta on cyan
		text = text.replace(/@[xX]36/g, "\x01n\x01y\x01" + "6"); // Yellow/brown on cyan
		text = text.replace(/@[xX]37/g, "\x01n\x01w\x01" + "6"); // White on cyan
		text = text.replace(/@[xX]38/g, "\x01h\x01k\x01" + "6"); // Bright black on cyan
		text = text.replace(/@[xX]39/g, "\x01h\x01b\x01" + "6"); // Bright blue on cyan
		text = text.replace(/@[xX]3[Aa]/g, "\x01h\x01g\x01" + "6"); // Bright green on cyan
		text = text.replace(/@[xX]3[Bb]/g, "\x01h\x01c\x01" + "6"); // Bright cyan on cyan
		text = text.replace(/@[xX]3[Cc]/g, "\x01h\x01r\x01" + "6"); // Bright red on cyan
		text = text.replace(/@[xX]3[Dd]/g, "\x01h\x01m\x01" + "6"); // Bright magenta on cyan
		text = text.replace(/@[xX]3[Ee]/g, "\x01h\x01y\x01" + "6"); // Bright yellow on cyan
		text = text.replace(/@[xX]3[Ff]/g, "\x01h\x01w\x01" + "6"); // Bright white on cyan

		// Red background
		text = text.replace(/@[xX]40/g, "\x01n\x01k\x01" + "1"); // Black on red
		text = text.replace(/@[xX]41/g, "\x01n\x01b\x01" + "1"); // Blue on red
		text = text.replace(/@[xX]42/g, "\x01n\x01g\x01" + "1"); // Green on red
		text = text.replace(/@[xX]43/g, "\x01n\x01c\x01" + "1"); // Cyan on red
		text = text.replace(/@[xX]44/g, "\x01n\x01r\x01" + "1"); // Red on red
		text = text.replace(/@[xX]45/g, "\x01n\x01m\x01" + "1"); // Magenta on red
		text = text.replace(/@[xX]46/g, "\x01n\x01y\x01" + "1"); // Yellow/brown on red
		text = text.replace(/@[xX]47/g, "\x01n\x01w\x01" + "1"); // White on red
		text = text.replace(/@[xX]48/g, "\x01h\x01k\x01" + "1"); // Bright black on red
		text = text.replace(/@[xX]49/g, "\x01h\x01b\x01" + "1"); // Bright blue on red
		text = text.replace(/@[xX]4[Aa]/g, "\x01h\x01g\x01" + "1"); // Bright green on red
		text = text.replace(/@[xX]4[Bb]/g, "\x01h\x01c\x01" + "1"); // Bright cyan on red
		text = text.replace(/@[xX]4[Cc]/g, "\x01h\x01r\x01" + "1"); // Bright red on red
		text = text.replace(/@[xX]4[Dd]/g, "\x01h\x01m\x01" + "1"); // Bright magenta on red
		text = text.replace(/@[xX]4[Ee]/g, "\x01h\x01y\x01" + "1"); // Bright yellow on red
		text = text.replace(/@[xX]4[Ff]/g, "\x01h\x01w\x01" + "1"); // Bright white on red

		// Magenta background
		text = text.replace(/@[xX]50/g, "\x01n\x01k\x01" + "5"); // Black on magenta
		text = text.replace(/@[xX]51/g, "\x01n\x01b\x01" + "5"); // Blue on magenta
		text = text.replace(/@[xX]52/g, "\x01n\x01g\x01" + "5"); // Green on magenta
		text = text.replace(/@[xX]53/g, "\x01n\x01c\x01" + "5"); // Cyan on magenta
		text = text.replace(/@[xX]54/g, "\x01n\x01r\x01" + "5"); // Red on magenta
		text = text.replace(/@[xX]55/g, "\x01n\x01m\x01" + "5"); // Magenta on magenta
		text = text.replace(/@[xX]56/g, "\x01n\x01y\x01" + "5"); // Yellow/brown on magenta
		text = text.replace(/@[xX]57/g, "\x01n\x01w\x01" + "5"); // White on magenta
		text = text.replace(/@[xX]58/g, "\x01h\x01k\x01" + "5"); // Bright black on magenta
		text = text.replace(/@[xX]59/g, "\x01h\x01b\x01" + "5"); // Bright blue on magenta
		text = text.replace(/@[xX]5[Aa]/g, "\x01h\x01g\x01" + "5"); // Bright green on magenta
		text = text.replace(/@[xX]5[Bb]/g, "\x01h\x01c\x01" + "5"); // Bright cyan on magenta
		text = text.replace(/@[xX]5[Cc]/g, "\x01h\x01r\x01" + "5"); // Bright red on magenta
		text = text.replace(/@[xX]5[Dd]/g, "\x01h\x01m\x01" + "5"); // Bright magenta on magenta
		text = text.replace(/@[xX]5[Ee]/g, "\x01h\x01y\x01" + "5"); // Bright yellow on magenta
		text = text.replace(/@[xX]5[Ff]/g, "\x01h\x01w\x01" + "5"); // Bright white on magenta

		// Brown background
		text = text.replace(/@[xX]60/g, "\x01n\x01k\x01" + "3"); // Black on brown
		text = text.replace(/@[xX]61/g, "\x01n\x01b\x01" + "3"); // Blue on brown
		text = text.replace(/@[xX]62/g, "\x01n\x01g\x01" + "3"); // Green on brown
		text = text.replace(/@[xX]63/g, "\x01n\x01c\x01" + "3"); // Cyan on brown
		text = text.replace(/@[xX]64/g, "\x01n\x01r\x01" + "3"); // Red on brown
		text = text.replace(/@[xX]65/g, "\x01n\x01m\x01" + "3"); // Magenta on brown
		text = text.replace(/@[xX]66/g, "\x01n\x01y\x01" + "3"); // Yellow/brown on brown
		text = text.replace(/@[xX]67/g, "\x01n\x01w\x01" + "3"); // White on brown
		text = text.replace(/@[xX]68/g, "\x01h\x01k\x01" + "3"); // Bright black on brown
		text = text.replace(/@[xX]69/g, "\x01h\x01b\x01" + "3"); // Bright blue on brown
		text = text.replace(/@[xX]6[Aa]/g, "\x01h\x01g\x01" + "3"); // Bright breen on brown
		text = text.replace(/@[xX]6[Bb]/g, "\x01h\x01c\x01" + "3"); // Bright cyan on brown
		text = text.replace(/@[xX]6[Cc]/g, "\x01h\x01r\x01" + "3"); // Bright red on brown
		text = text.replace(/@[xX]6[Dd]/g, "\x01h\x01m\x01" + "3"); // Bright magenta on brown
		text = text.replace(/@[xX]6[Ee]/g, "\x01h\x01y\x01" + "3"); // Bright yellow on brown
		text = text.replace(/@[xX]6[Ff]/g, "\x01h\x01w\x01" + "3"); // Bright white on brown

		// White background
		text = text.replace(/@[xX]70/g, "\x01n\x01k\x01" + "7"); // Black on white
		text = text.replace(/@[xX]71/g, "\x01n\x01b\x01" + "7"); // Blue on white
		text = text.replace(/@[xX]72/g, "\x01n\x01g\x01" + "7"); // Green on white
		text = text.replace(/@[xX]73/g, "\x01n\x01c\x01" + "7"); // Cyan on white
		text = text.replace(/@[xX]74/g, "\x01n\x01r\x01" + "7"); // Red on white
		text = text.replace(/@[xX]75/g, "\x01n\x01m\x01" + "7"); // Magenta on white
		text = text.replace(/@[xX]76/g, "\x01n\x01y\x01" + "7"); // Yellow/brown on white
		text = text.replace(/@[xX]77/g, "\x01n\x01w\x01" + "7"); // White on white
		text = text.replace(/@[xX]78/g, "\x01h\x01k\x01" + "7"); // Bright black on white
		text = text.replace(/@[xX]79/g, "\x01h\x01b\x01" + "7"); // Bright blue on white
		text = text.replace(/@[xX]7[Aa]/g, "\x01h\x01g\x01" + "7"); // Bright green on white
		text = text.replace(/@[xX]7[Bb]/g, "\x01h\x01c\x01" + "7"); // Bright cyan on white
		text = text.replace(/@[xX]7[Cc]/g, "\x01h\x01r\x01" + "7"); // Bright red on white
		text = text.replace(/@[xX]7[Dd]/g, "\x01h\x01m\x01" + "7"); // Bright magenta on white
		text = text.replace(/@[xX]7[Ee]/g, "\x01h\x01y\x01" + "7"); // Bright yellow on white
		text = text.replace(/@[xX]7[Ff]/g, "\x01h\x01w\x01" + "7"); // Bright white on white

		// Black background, blinking foreground
		text = text.replace(/@[xX]80/g, "\x01n\x01k\x01" + "0\x01i"); // Blinking black on black
		text = text.replace(/@[xX]81/g, "\x01n\x01b\x01" + "0\x01i"); // Blinking blue on black
		text = text.replace(/@[xX]82/g, "\x01n\x01g\x01" + "0\x01i"); // Blinking green on black
		text = text.replace(/@[xX]83/g, "\x01n\x01c\x01" + "0\x01i"); // Blinking cyan on black
		text = text.replace(/@[xX]84/g, "\x01n\x01r\x01" + "0\x01i"); // Blinking red on black
		text = text.replace(/@[xX]85/g, "\x01n\x01m\x01" + "0\x01i"); // Blinking magenta on black
		text = text.replace(/@[xX]86/g, "\x01n\x01y\x01" + "0\x01i"); // Blinking yellow/brown on black
		text = text.replace(/@[xX]87/g, "\x01n\x01w\x01" + "0\x01i"); // Blinking white on black
		text = text.replace(/@[xX]88/g, "\x01h\x01k\x01" + "0\x01i"); // Blinking bright black on black
		text = text.replace(/@[xX]89/g, "\x01h\x01b\x01" + "0\x01i"); // Blinking bright blue on black
		text = text.replace(/@[xX]8[Aa]/g, "\x01h\x01g\x01" + "0\x01i"); // Blinking bright green on black
		text = text.replace(/@[xX]8[Bb]/g, "\x01h\x01c\x01" + "0\x01i"); // Blinking bright cyan on black
		text = text.replace(/@[xX]8[Cc]/g, "\x01h\x01r\x01" + "0\x01i"); // Blinking bright red on black
		text = text.replace(/@[xX]8[Dd]/g, "\x01h\x01m\x01" + "0\x01i"); // Blinking bright magenta on black
		text = text.replace(/@[xX]8[Ee]/g, "\x01h\x01y\x01" + "0\x01i"); // Blinking bright yellow on black
		text = text.replace(/@[xX]8[Ff]/g, "\x01h\x01w\x01" + "0\x01i"); // Blinking bright white on black

		// Blue background, blinking foreground
		text = text.replace(/@[xX]90/g, "\x01n\x01k\x01" + "4\x01i"); // Blinking black on blue
		text = text.replace(/@[xX]91/g, "\x01n\x01b\x01" + "4\x01i"); // Blinking blue on blue
		text = text.replace(/@[xX]92/g, "\x01n\x01g\x01" + "4\x01i"); // Blinking green on blue
		text = text.replace(/@[xX]93/g, "\x01n\x01c\x01" + "4\x01i"); // Blinking cyan on blue
		text = text.replace(/@[xX]94/g, "\x01n\x01r\x01" + "4\x01i"); // Blinking red on blue
		text = text.replace(/@[xX]95/g, "\x01n\x01m\x01" + "4\x01i"); // Blinking magenta on blue
		text = text.replace(/@[xX]96/g, "\x01n\x01y\x01" + "4\x01i"); // Blinking yellow/brown on blue
		text = text.replace(/@[xX]97/g, "\x01n\x01w\x01" + "4\x01i"); // Blinking white on blue
		text = text.replace(/@[xX]98/g, "\x01h\x01k\x01" + "4\x01i"); // Blinking bright black on blue
		text = text.replace(/@[xX]99/g, "\x01h\x01b\x01" + "4\x01i"); // Blinking bright blue on blue
		text = text.replace(/@[xX]9[Aa]/g, "\x01h\x01g\x01" + "4\x01i"); // Blinking bright green on blue
		text = text.replace(/@[xX]9[Bb]/g, "\x01h\x01c\x01" + "4\x01i"); // Blinking bright cyan on blue
		text = text.replace(/@[xX]9[Cc]/g, "\x01h\x01r\x01" + "4\x01i"); // Blinking bright red on blue
		text = text.replace(/@[xX]9[Dd]/g, "\x01h\x01m\x01" + "4\x01i"); // Blinking bright magenta on blue
		text = text.replace(/@[xX]9[Ee]/g, "\x01h\x01y\x01" + "4\x01i"); // Blinking bright yellow on blue
		text = text.replace(/@[xX]9[Ff]/g, "\x01h\x01w\x01" + "4\x01i"); // Blinking bright white on blue

		// Green background, blinking foreground
		text = text.replace(/@[xX][Aa]0/g, "\x01n\x01k\x01" + "2\x01i"); // Blinking black on green
		text = text.replace(/@[xX][Aa]1/g, "\x01n\x01b\x01" + "2\x01i"); // Blinking blue on green
		text = text.replace(/@[xX][Aa]2/g, "\x01n\x01g\x01" + "2\x01i"); // Blinking green on green
		text = text.replace(/@[xX][Aa]3/g, "\x01n\x01c\x01" + "2\x01i"); // Blinking cyan on green
		text = text.replace(/@[xX][Aa]4/g, "\x01n\x01r\x01" + "2\x01i"); // Blinking red on green
		text = text.replace(/@[xX][Aa]5/g, "\x01n\x01m\x01" + "2\x01i"); // Blinking magenta on green
		text = text.replace(/@[xX][Aa]6/g, "\x01n\x01y\x01" + "2\x01i"); // Blinking yellow/brown on green
		text = text.replace(/@[xX][Aa]7/g, "\x01n\x01w\x01" + "2\x01i"); // Blinking white on green
		text = text.replace(/@[xX][Aa]8/g, "\x01h\x01k\x01" + "2\x01i"); // Blinking bright black on green
		text = text.replace(/@[xX][Aa]9/g, "\x01h\x01b\x01" + "2\x01i"); // Blinking bright blue on green
		text = text.replace(/@[xX][Aa][Aa]/g, "\x01h\x01g\x01" + "2\x01i"); // Blinking bright green on green
		text = text.replace(/@[xX][Aa][Bb]/g, "\x01h\x01c\x01" + "2\x01i"); // Blinking bright cyan on green
		text = text.replace(/@[xX][Aa][Cc]/g, "\x01h\x01r\x01" + "2\x01i"); // Blinking bright red on green
		text = text.replace(/@[xX][Aa][Dd]/g, "\x01h\x01m\x01" + "2\x01i"); // Blinking bright magenta on green
		text = text.replace(/@[xX][Aa][Ee]/g, "\x01h\x01y\x01" + "2\x01i"); // Blinking bright yellow on green
		text = text.replace(/@[xX][Aa][Ff]/g, "\x01h\x01w\x01" + "2\x01i"); // Blinking bright white on green

		// Cyan background, blinking foreground
		text = text.replace(/@[xX][Bb]0/g, "\x01n\x01k\x01" + "6\x01i"); // Blinking black on cyan
		text = text.replace(/@[xX][Bb]1/g, "\x01n\x01b\x01" + "6\x01i"); // Blinking blue on cyan
		text = text.replace(/@[xX][Bb]2/g, "\x01n\x01g\x01" + "6\x01i"); // Blinking green on cyan
		text = text.replace(/@[xX][Bb]3/g, "\x01n\x01c\x01" + "6\x01i"); // Blinking cyan on cyan
		text = text.replace(/@[xX][Bb]4/g, "\x01n\x01r\x01" + "6\x01i"); // Blinking red on cyan
		text = text.replace(/@[xX][Bb]5/g, "\x01n\x01m\x01" + "6\x01i"); // Blinking magenta on cyan
		text = text.replace(/@[xX][Bb]6/g, "\x01n\x01y\x01" + "6\x01i"); // Blinking yellow/brown on cyan
		text = text.replace(/@[xX][Bb]7/g, "\x01n\x01w\x01" + "6\x01i"); // Blinking white on cyan
		text = text.replace(/@[xX][Bb]8/g, "\x01h\x01k\x01" + "6\x01i"); // Blinking bright black on cyan
		text = text.replace(/@[xX][Bb]9/g, "\x01h\x01b\x01" + "6\x01i"); // Blinking bright blue on cyan
		text = text.replace(/@[xX][Bb][Aa]/g, "\x01h\x01g\x01" + "6\x01i"); // Blinking bright green on cyan
		text = text.replace(/@[xX][Bb][Bb]/g, "\x01h\x01c\x01" + "6\x01i"); // Blinking bright cyan on cyan
		text = text.replace(/@[xX][Bb][Cc]/g, "\x01h\x01r\x01" + "6\x01i"); // Blinking bright red on cyan
		text = text.replace(/@[xX][Bb][Dd]/g, "\x01h\x01m\x01" + "6\x01i"); // Blinking bright magenta on cyan
		text = text.replace(/@[xX][Bb][Ee]/g, "\x01h\x01y\x01" + "6\x01i"); // Blinking bright yellow on cyan
		text = text.replace(/@[xX][Bb][Ff]/g, "\x01h\x01w\x01" + "6\x01i"); // Blinking bright white on cyan

		// Red background, blinking foreground
		text = text.replace(/@[xX][Cc]0/g, "\x01n\x01k\x01" + "1\x01i"); // Blinking black on red
		text = text.replace(/@[xX][Cc]1/g, "\x01n\x01b\x01" + "1\x01i"); // Blinking blue on red
		text = text.replace(/@[xX][Cc]2/g, "\x01n\x01g\x01" + "1\x01i"); // Blinking green on red
		text = text.replace(/@[xX][Cc]3/g, "\x01n\x01c\x01" + "1\x01i"); // Blinking cyan on red
		text = text.replace(/@[xX][Cc]4/g, "\x01n\x01r\x01" + "1\x01i"); // Blinking red on red
		text = text.replace(/@[xX][Cc]5/g, "\x01n\x01m\x01" + "1\x01i"); // Blinking magenta on red
		text = text.replace(/@[xX][Cc]6/g, "\x01n\x01y\x01" + "1\x01i"); // Blinking yellow/brown on red
		text = text.replace(/@[xX][Cc]7/g, "\x01n\x01w\x01" + "1\x01i"); // Blinking white on red
		text = text.replace(/@[xX][Cc]8/g, "\x01h\x01k\x01" + "1\x01i"); // Blinking bright black on red
		text = text.replace(/@[xX][Cc]9/g, "\x01h\x01b\x01" + "1\x01i"); // Blinking bright blue on red
		text = text.replace(/@[xX][Cc][Aa]/g, "\x01h\x01g\x01" + "1\x01i"); // Blinking bright green on red
		text = text.replace(/@[xX][Cc][Bb]/g, "\x01h\x01c\x01" + "1\x01i"); // Blinking bright cyan on red
		text = text.replace(/@[xX][Cc][Cc]/g, "\x01h\x01r\x01" + "1\x01i"); // Blinking bright red on red
		text = text.replace(/@[xX][Cc][Dd]/g, "\x01h\x01m\x01" + "1\x01i"); // Blinking bright magenta on red
		text = text.replace(/@[xX][Cc][Ee]/g, "\x01h\x01y\x01" + "1\x01i"); // Blinking bright yellow on red
		text = text.replace(/@[xX][Cc][Ff]/g, "\x01h\x01w\x01" + "1\x01i"); // Blinking bright white on red

		// Magenta background, blinking foreground
		text = text.replace(/@[xX][Dd]0/g, "\x01n\x01k\x01" + "5\x01i"); // Blinking black on magenta
		text = text.replace(/@[xX][Dd]1/g, "\x01n\x01b\x01" + "5\x01i"); // Blinking blue on magenta
		text = text.replace(/@[xX][Dd]2/g, "\x01n\x01g\x01" + "5\x01i"); // Blinking green on magenta
		text = text.replace(/@[xX][Dd]3/g, "\x01n\x01c\x01" + "5\x01i"); // Blinking cyan on magenta
		text = text.replace(/@[xX][Dd]4/g, "\x01n\x01r\x01" + "5\x01i"); // Blinking red on magenta
		text = text.replace(/@[xX][Dd]5/g, "\x01n\x01m\x01" + "5\x01i"); // Blinking magenta on magenta
		text = text.replace(/@[xX][Dd]6/g, "\x01n\x01y\x01" + "5\x01i"); // Blinking yellow/brown on magenta
		text = text.replace(/@[xX][Dd]7/g, "\x01n\x01w\x01" + "5\x01i"); // Blinking white on magenta
		text = text.replace(/@[xX][Dd]8/g, "\x01h\x01k\x01" + "5\x01i"); // Blinking bright black on magenta
		text = text.replace(/@[xX][Dd]9/g, "\x01h\x01b\x01" + "5\x01i"); // Blinking bright blue on magenta
		text = text.replace(/@[xX][Dd][Aa]/g, "\x01h\x01g\x01" + "5\x01i"); // Blinking bright green on magenta
		text = text.replace(/@[xX][Dd][Bb]/g, "\x01h\x01c\x01" + "5\x01i"); // Blinking bright cyan on magenta
		text = text.replace(/@[xX][Dd][Cc]/g, "\x01h\x01r\x01" + "5\x01i"); // Blinking bright red on magenta
		text = text.replace(/@[xX][Dd][Dd]/g, "\x01h\x01m\x01" + "5\x01i"); // Blinking bright magenta on magenta
		text = text.replace(/@[xX][Dd][Ee]/g, "\x01h\x01y\x01" + "5\x01i"); // Blinking bright yellow on magenta
		text = text.replace(/@[xX][Dd][Ff]/g, "\x01h\x01w\x01" + "5\x01i"); // Blinking bright white on magenta

		// Brown background, blinking foreground
		text = text.replace(/@[xX][Ee]0/g, "\x01n\x01k\x01" + "3\x01i"); // Blinking black on brown
		text = text.replace(/@[xX][Ee]1/g, "\x01n\x01b\x01" + "3\x01i"); // Blinking blue on brown
		text = text.replace(/@[xX][Ee]2/g, "\x01n\x01g\x01" + "3\x01i"); // Blinking green on brown
		text = text.replace(/@[xX][Ee]3/g, "\x01n\x01c\x01" + "3\x01i"); // Blinking cyan on brown
		text = text.replace(/@[xX][Ee]4/g, "\x01n\x01r\x01" + "3\x01i"); // Blinking red on brown
		text = text.replace(/@[xX][Ee]5/g, "\x01n\x01m\x01" + "3\x01i"); // Blinking magenta on brown
		text = text.replace(/@[xX][Ee]6/g, "\x01n\x01y\x01" + "3\x01i"); // Blinking yellow/brown on brown
		text = text.replace(/@[xX][Ee]7/g, "\x01n\x01w\x01" + "3\x01i"); // Blinking white on brown
		text = text.replace(/@[xX][Ee]8/g, "\x01h\x01k\x01" + "3\x01i"); // Blinking bright black on brown
		text = text.replace(/@[xX][Ee]9/g, "\x01h\x01b\x01" + "3\x01i"); // Blinking bright blue on brown
		text = text.replace(/@[xX][Ee][Aa]/g, "\x01h\x01g\x01" + "3\x01i"); // Blinking bright green on brown
		text = text.replace(/@[xX][Ee][Bb]/g, "\x01h\x01c\x01" + "3\x01i"); // Blinking bright cyan on brown
		text = text.replace(/@[xX][Ee][Cc]/g, "\x01h\x01r\x01" + "3\x01i"); // Blinking bright red on brown
		text = text.replace(/@[xX][Ee][Dd]/g, "\x01h\x01m\x01" + "3\x01i"); // Blinking bright magenta on brown
		text = text.replace(/@[xX][Ee][Ee]/g, "\x01h\x01y\x01" + "3\x01i"); // Blinking bright yellow on brown
		text = text.replace(/@[xX][Ee][Ff]/g, "\x01h\x01w\x01" + "3\x01i"); // Blinking bright white on brown

		// White background, blinking foreground
		text = text.replace(/@[xX][Ff]0/g, "\x01n\x01k\x01" + "7\x01i"); // Blinking black on white
		text = text.replace(/@[xX][Ff]1/g, "\x01n\x01b\x01" + "7\x01i"); // Blinking blue on white
		text = text.replace(/@[xX][Ff]2/g, "\x01n\x01g\x01" + "7\x01i"); // Blinking green on white
		text = text.replace(/@[xX][Ff]3/g, "\x01n\x01c\x01" + "7\x01i"); // Blinking cyan on white
		text = text.replace(/@[xX][Ff]4/g, "\x01n\x01r\x01" + "7\x01i"); // Blinking red on white
		text = text.replace(/@[xX][Ff]5/g, "\x01n\x01m\x01" + "7\x01i"); // Blinking magenta on white
		text = text.replace(/@[xX][Ff]6/g, "\x01n\x01y\x01" + "7\x01i"); // Blinking yellow/brown on white
		text = text.replace(/@[xX][Ff]7/g, "\x01n\x01w\x01" + "7\x01i"); // Blinking white on white
		text = text.replace(/@[xX][Ff]8/g, "\x01h\x01k\x01" + "7\x01i"); // Blinking bright black on white
		text = text.replace(/@[xX][Ff]9/g, "\x01h\x01b\x01" + "7\x01i"); // Blinking bright blue on white
		text = text.replace(/@[xX][Ff][Aa]/g, "\x01h\x01g\x01" + "7\x01i"); // Blinking bright green on white
		text = text.replace(/@[xX][Ff][Bb]/g, "\x01h\x01c\x01" + "7\x01i"); // Blinking bright cyan on white
		text = text.replace(/@[xX][Ff][Cc]/g, "\x01h\x01r\x01" + "7\x01i"); // Blinking bright red on white
		text = text.replace(/@[xX][Ff][Dd]/g, "\x01h\x01m\x01" + "7\x01i"); // Blinking bright magenta on white
		text = text.replace(/@[xX][Ff][Ee]/g, "\x01h\x01y\x01" + "7\x01i"); // Blinking bright yellow on white
		text = text.replace(/@[xX][Ff][Ff]/g, "\x01h\x01w\x01" + "7\x01i"); // Blinking bright white on white

		return text;
	}
	else
		return pText; // No PCBoard-style attribute codes found, so just return the text.
}

// Converts Wildcat attribute codes to Synchronet attribute codes.
//
// Parameters:
//  pText: A string containing the text to convert
//
// Return value: The text with the color codes converted
function wildcatAttrsToSyncAttrs(pText)
{
	// First, see if the text has any Wildcat-style attribute codes at
	// all.  We'll be performing a bunch of search & replace commands,
	// so we don't want to do all that work for nothing.. :)
	if (/@[0-9A-Fa-f]{2}@/.test(pText))
	{
		// Black background
		var text = pText.replace(/@00@/g, "\x01n\x01k\x01" + "0"); // Black on black
		text = text.replace(/@01@/g, "\x01n\x01b\x01" + "0"); // Blue on black
		text = text.replace(/@02@/g, "\x01n\x01g\x01" + "0"); // Green on black
		text = text.replace(/@03@/g, "\x01n\x01c\x01" + "0"); // Cyan on black
		text = text.replace(/@04@/g, "\x01n\x01r\x01" + "0"); // Red on black
		text = text.replace(/@05@/g, "\x01n\x01m\x01" + "0"); // Magenta on black
		text = text.replace(/@06@/g, "\x01n\x01y\x01" + "0"); // Yellow/brown on black
		text = text.replace(/@07@/g, "\x01n\x01w\x01" + "0"); // White on black
		text = text.replace(/@08@/g, "\x01n\x01w\x01" + "0"); // White on black
		text = text.replace(/@09@/g, "\x01n\x01w\x01" + "0"); // White on black
		text = text.replace(/@08@/g, "\x01h\x01k\x01" + "0"); // Bright black on black
		text = text.replace(/@09@/g, "\x01h\x01b\x01" + "0"); // Bright blue on black
		text = text.replace(/@0[Aa]@/g, "\x01h\x01g\x01" + "0"); // Bright green on black
		text = text.replace(/@0[Bb]@/g, "\x01h\x01c\x01" + "0"); // Bright cyan on black
		text = text.replace(/@0[Cc]@/g, "\x01h\x01r\x01" + "0"); // Bright red on black
		text = text.replace(/@0[Dd]@/g, "\x01h\x01m\x01" + "0"); // Bright magenta on black
		text = text.replace(/@0[Ee]@/g, "\x01h\x01y\x01" + "0"); // Bright yellow on black
		text = text.replace(/@0[Ff]@/g, "\x01h\x01w\x01" + "0"); // Bright white on black
		// Blinking foreground

		// Blue background
		text = text.replace(/@10@/g, "\x01n\x01k\x01" + "4"); // Black on blue
		text = text.replace(/@11@/g, "\x01n\x01b\x01" + "4"); // Blue on blue
		text = text.replace(/@12@/g, "\x01n\x01g\x01" + "4"); // Green on blue
		text = text.replace(/@13@/g, "\x01n\x01c\x01" + "4"); // Cyan on blue
		text = text.replace(/@14@/g, "\x01n\x01r\x01" + "4"); // Red on blue
		text = text.replace(/@15@/g, "\x01n\x01m\x01" + "4"); // Magenta on blue
		text = text.replace(/@16@/g, "\x01n\x01y\x01" + "4"); // Yellow/brown on blue
		text = text.replace(/@17@/g, "\x01n\x01w\x01" + "4"); // White on blue
		text = text.replace(/@18@/g, "\x01h\x01k\x01" + "4"); // Bright black on blue
		text = text.replace(/@19@/g, "\x01h\x01b\x01" + "4"); // Bright blue on blue
		text = text.replace(/@1[Aa]@/g, "\x01h\x01g\x01" + "4"); // Bright green on blue
		text = text.replace(/@1[Bb]@/g, "\x01h\x01c\x01" + "4"); // Bright cyan on blue
		text = text.replace(/@1[Cc]@/g, "\x01h\x01r\x01" + "4"); // Bright red on blue
		text = text.replace(/@1[Dd]@/g, "\x01h\x01m\x01" + "4"); // Bright magenta on blue
		text = text.replace(/@1[Ee]@/g, "\x01h\x01y\x01" + "4"); // Bright yellow on blue
		text = text.replace(/@1[Ff]@/g, "\x01h\x01w\x01" + "4"); // Bright white on blue

		// Green background
		text = text.replace(/@20@/g, "\x01n\x01k\x01" + "2"); // Black on green
		text = text.replace(/@21@/g, "\x01n\x01b\x01" + "2"); // Blue on green
		text = text.replace(/@22@/g, "\x01n\x01g\x01" + "2"); // Green on green
		text = text.replace(/@23@/g, "\x01n\x01c\x01" + "2"); // Cyan on green
		text = text.replace(/@24@/g, "\x01n\x01r\x01" + "2"); // Red on green
		text = text.replace(/@25@/g, "\x01n\x01m\x01" + "2"); // Magenta on green
		text = text.replace(/@26@/g, "\x01n\x01y\x01" + "2"); // Yellow/brown on green
		text = text.replace(/@27@/g, "\x01n\x01w\x01" + "2"); // White on green
		text = text.replace(/@28@/g, "\x01h\x01k\x01" + "2"); // Bright black on green
		text = text.replace(/@29@/g, "\x01h\x01b\x01" + "2"); // Bright blue on green
		text = text.replace(/@2[Aa]@/g, "\x01h\x01g\x01" + "2"); // Bright green on green
		text = text.replace(/@2[Bb]@/g, "\x01h\x01c\x01" + "2"); // Bright cyan on green
		text = text.replace(/@2[Cc]@/g, "\x01h\x01r\x01" + "2"); // Bright red on green
		text = text.replace(/@2[Dd]@/g, "\x01h\x01m\x01" + "2"); // Bright magenta on green
		text = text.replace(/@2[Ee]@/g, "\x01h\x01y\x01" + "2"); // Bright yellow on green
		text = text.replace(/@2[Ff]@/g, "\x01h\x01w\x01" + "2"); // Bright white on green

		// Cyan background
		text = text.replace(/@30@/g, "\x01n\x01k\x01" + "6"); // Black on cyan
		text = text.replace(/@31@/g, "\x01n\x01b\x01" + "6"); // Blue on cyan
		text = text.replace(/@32@/g, "\x01n\x01g\x01" + "6"); // Green on cyan
		text = text.replace(/@33@/g, "\x01n\x01c\x01" + "6"); // Cyan on cyan
		text = text.replace(/@34@/g, "\x01n\x01r\x01" + "6"); // Red on cyan
		text = text.replace(/@35@/g, "\x01n\x01m\x01" + "6"); // Magenta on cyan
		text = text.replace(/@36@/g, "\x01n\x01y\x01" + "6"); // Yellow/brown on cyan
		text = text.replace(/@37@/g, "\x01n\x01w\x01" + "6"); // White on cyan
		text = text.replace(/@38@/g, "\x01h\x01k\x01" + "6"); // Bright black on cyan
		text = text.replace(/@39@/g, "\x01h\x01b\x01" + "6"); // Bright blue on cyan
		text = text.replace(/@3[Aa]@/g, "\x01h\x01g\x01" + "6"); // Bright green on cyan
		text = text.replace(/@3[Bb]@/g, "\x01h\x01c\x01" + "6"); // Bright cyan on cyan
		text = text.replace(/@3[Cc]@/g, "\x01h\x01r\x01" + "6"); // Bright red on cyan
		text = text.replace(/@3[Dd]@/g, "\x01h\x01m\x01" + "6"); // Bright magenta on cyan
		text = text.replace(/@3[Ee]@/g, "\x01h\x01y\x01" + "6"); // Bright yellow on cyan
		text = text.replace(/@3[Ff]@/g, "\x01h\x01w\x01" + "6"); // Bright white on cyan

		// Red background
		text = text.replace(/@40@/g, "\x01n\x01k\x01" + "1"); // Black on red
		text = text.replace(/@41@/g, "\x01n\x01b\x01" + "1"); // Blue on red
		text = text.replace(/@42@/g, "\x01n\x01g\x01" + "1"); // Green on red
		text = text.replace(/@43@/g, "\x01n\x01c\x01" + "1"); // Cyan on red
		text = text.replace(/@44@/g, "\x01n\x01r\x01" + "1"); // Red on red
		text = text.replace(/@45@/g, "\x01n\x01m\x01" + "1"); // Magenta on red
		text = text.replace(/@46@/g, "\x01n\x01y\x01" + "1"); // Yellow/brown on red
		text = text.replace(/@47@/g, "\x01n\x01w\x01" + "1"); // White on red
		text = text.replace(/@48@/g, "\x01h\x01k\x01" + "1"); // Bright black on red
		text = text.replace(/@49@/g, "\x01h\x01b\x01" + "1"); // Bright blue on red
		text = text.replace(/@4[Aa]@/g, "\x01h\x01g\x01" + "1"); // Bright green on red
		text = text.replace(/@4[Bb]@/g, "\x01h\x01c\x01" + "1"); // Bright cyan on red
		text = text.replace(/@4[Cc]@/g, "\x01h\x01r\x01" + "1"); // Bright red on red
		text = text.replace(/@4[Dd]@/g, "\x01h\x01m\x01" + "1"); // Bright magenta on red
		text = text.replace(/@4[Ee]@/g, "\x01h\x01y\x01" + "1"); // Bright yellow on red
		text = text.replace(/@4[Ff]@/g, "\x01h\x01w\x01" + "1"); // Bright white on red

		// Magenta background
		text = text.replace(/@50@/g, "\x01n\x01k\x01" + "5"); // Black on magenta
		text = text.replace(/@51@/g, "\x01n\x01b\x01" + "5"); // Blue on magenta
		text = text.replace(/@52@/g, "\x01n\x01g\x01" + "5"); // Green on magenta
		text = text.replace(/@53@/g, "\x01n\x01c\x01" + "5"); // Cyan on magenta
		text = text.replace(/@54@/g, "\x01n\x01r\x01" + "5"); // Red on magenta
		text = text.replace(/@55@/g, "\x01n\x01m\x01" + "5"); // Magenta on magenta
		text = text.replace(/@56@/g, "\x01n\x01y\x01" + "5"); // Yellow/brown on magenta
		text = text.replace(/@57@/g, "\x01n\x01w\x01" + "5"); // White on magenta
		text = text.replace(/@58@/g, "\x01h\x01k\x01" + "5"); // Bright black on magenta
		text = text.replace(/@59@/g, "\x01h\x01b\x01" + "5"); // Bright blue on magenta
		text = text.replace(/@5[Aa]@/g, "\x01h\x01g\x01" + "5"); // Bright green on magenta
		text = text.replace(/@5[Bb]@/g, "\x01h\x01c\x01" + "5"); // Bright cyan on magenta
		text = text.replace(/@5[Cc]@/g, "\x01h\x01r\x01" + "5"); // Bright red on magenta
		text = text.replace(/@5[Dd]@/g, "\x01h\x01m\x01" + "5"); // Bright magenta on magenta
		text = text.replace(/@5[Ee]@/g, "\x01h\x01y\x01" + "5"); // Bright yellow on magenta
		text = text.replace(/@5[Ff]@/g, "\x01h\x01w\x01" + "5"); // Bright white on magenta

		// Brown background
		text = text.replace(/@60@/g, "\x01n\x01k\x01" + "3"); // Black on brown
		text = text.replace(/@61@/g, "\x01n\x01b\x01" + "3"); // Blue on brown
		text = text.replace(/@62@/g, "\x01n\x01g\x01" + "3"); // Green on brown
		text = text.replace(/@63@/g, "\x01n\x01c\x01" + "3"); // Cyan on brown
		text = text.replace(/@64@/g, "\x01n\x01r\x01" + "3"); // Red on brown
		text = text.replace(/@65@/g, "\x01n\x01m\x01" + "3"); // Magenta on brown
		text = text.replace(/@66@/g, "\x01n\x01y\x01" + "3"); // Yellow/brown on brown
		text = text.replace(/@67@/g, "\x01n\x01w\x01" + "3"); // White on brown
		text = text.replace(/@68@/g, "\x01h\x01k\x01" + "3"); // Bright black on brown
		text = text.replace(/@69@/g, "\x01h\x01b\x01" + "3"); // Bright blue on brown
		text = text.replace(/@6[Aa]@/g, "\x01h\x01g\x01" + "3"); // Bright breen on brown
		text = text.replace(/@6[Bb]@/g, "\x01h\x01c\x01" + "3"); // Bright cyan on brown
		text = text.replace(/@6[Cc]@/g, "\x01h\x01r\x01" + "3"); // Bright red on brown
		text = text.replace(/@6[Dd]@/g, "\x01h\x01m\x01" + "3"); // Bright magenta on brown
		text = text.replace(/@6[Ee]@/g, "\x01h\x01y\x01" + "3"); // Bright yellow on brown
		text = text.replace(/@6[Ff]@/g, "\x01h\x01w\x01" + "3"); // Bright white on brown

		// White background
		text = text.replace(/@70@/g, "\x01n\x01k\x01" + "7"); // Black on white
		text = text.replace(/@71@/g, "\x01n\x01b\x01" + "7"); // Blue on white
		text = text.replace(/@72@/g, "\x01n\x01g\x01" + "7"); // Green on white
		text = text.replace(/@73@/g, "\x01n\x01c\x01" + "7"); // Cyan on white
		text = text.replace(/@74@/g, "\x01n\x01r\x01" + "7"); // Red on white
		text = text.replace(/@75@/g, "\x01n\x01m\x01" + "7"); // Magenta on white
		text = text.replace(/@76@/g, "\x01n\x01y\x01" + "7"); // Yellow/brown on white
		text = text.replace(/@77@/g, "\x01n\x01w\x01" + "7"); // White on white
		text = text.replace(/@78@/g, "\x01h\x01k\x01" + "7"); // Bright black on white
		text = text.replace(/@79@/g, "\x01h\x01b\x01" + "7"); // Bright blue on white
		text = text.replace(/@7[Aa]@/g, "\x01h\x01g\x01" + "7"); // Bright green on white
		text = text.replace(/@7[Bb]@/g, "\x01h\x01c\x01" + "7"); // Bright cyan on white
		text = text.replace(/@7[Cc]@/g, "\x01h\x01r\x01" + "7"); // Bright red on white
		text = text.replace(/@7[Dd]@/g, "\x01h\x01m\x01" + "7"); // Bright magenta on white
		text = text.replace(/@7[Ee]@/g, "\x01h\x01y\x01" + "7"); // Bright yellow on white
		text = text.replace(/@7[Ff]@/g, "\x01h\x01w\x01" + "7"); // Bright white on white

		// Black background, blinking foreground
		text = text.replace(/@80@/g, "\x01n\x01k\x01" + "0\x01i"); // Blinking black on black
		text = text.replace(/@81@/g, "\x01n\x01b\x01" + "0\x01i"); // Blinking blue on black
		text = text.replace(/@82@/g, "\x01n\x01g\x01" + "0\x01i"); // Blinking green on black
		text = text.replace(/@83@/g, "\x01n\x01c\x01" + "0\x01i"); // Blinking cyan on black
		text = text.replace(/@84@/g, "\x01n\x01r\x01" + "0\x01i"); // Blinking red on black
		text = text.replace(/@85@/g, "\x01n\x01m\x01" + "0\x01i"); // Blinking magenta on black
		text = text.replace(/@86@/g, "\x01n\x01y\x01" + "0\x01i"); // Blinking yellow/brown on black
		text = text.replace(/@87@/g, "\x01n\x01w\x01" + "0\x01i"); // Blinking white on black
		text = text.replace(/@88@/g, "\x01h\x01k\x01" + "0\x01i"); // Blinking bright black on black
		text = text.replace(/@89@/g, "\x01h\x01b\x01" + "0\x01i"); // Blinking bright blue on black
		text = text.replace(/@8[Aa]@/g, "\x01h\x01g\x01" + "0\x01i"); // Blinking bright green on black
		text = text.replace(/@8[Bb]@/g, "\x01h\x01c\x01" + "0\x01i"); // Blinking bright cyan on black
		text = text.replace(/@8[Cc]@/g, "\x01h\x01r\x01" + "0\x01i"); // Blinking bright red on black
		text = text.replace(/@8[Dd]@/g, "\x01h\x01m\x01" + "0\x01i"); // Blinking bright magenta on black
		text = text.replace(/@8[Ee]@/g, "\x01h\x01y\x01" + "0\x01i"); // Blinking bright yellow on black
		text = text.replace(/@8[Ff]@/g, "\x01h\x01w\x01" + "0\x01i"); // Blinking bright white on black

		// Blue background, blinking foreground
		text = text.replace(/@90@/g, "\x01n\x01k\x01" + "4\x01i"); // Blinking black on blue
		text = text.replace(/@91@/g, "\x01n\x01b\x01" + "4\x01i"); // Blinking blue on blue
		text = text.replace(/@92@/g, "\x01n\x01g\x01" + "4\x01i"); // Blinking green on blue
		text = text.replace(/@93@/g, "\x01n\x01c\x01" + "4\x01i"); // Blinking cyan on blue
		text = text.replace(/@94@/g, "\x01n\x01r\x01" + "4\x01i"); // Blinking red on blue
		text = text.replace(/@95@/g, "\x01n\x01m\x01" + "4\x01i"); // Blinking magenta on blue
		text = text.replace(/@96@/g, "\x01n\x01y\x01" + "4\x01i"); // Blinking yellow/brown on blue
		text = text.replace(/@97@/g, "\x01n\x01w\x01" + "4\x01i"); // Blinking white on blue
		text = text.replace(/@98@/g, "\x01h\x01k\x01" + "4\x01i"); // Blinking bright black on blue
		text = text.replace(/@99@/g, "\x01h\x01b\x01" + "4\x01i"); // Blinking bright blue on blue
		text = text.replace(/@9[Aa]@/g, "\x01h\x01g\x01" + "4\x01i"); // Blinking bright green on blue
		text = text.replace(/@9[Bb]@/g, "\x01h\x01c\x01" + "4\x01i"); // Blinking bright cyan on blue
		text = text.replace(/@9[Cc]@/g, "\x01h\x01r\x01" + "4\x01i"); // Blinking bright red on blue
		text = text.replace(/@9[Dd]@/g, "\x01h\x01m\x01" + "4\x01i"); // Blinking bright magenta on blue
		text = text.replace(/@9[Ee]@/g, "\x01h\x01y\x01" + "4\x01i"); // Blinking bright yellow on blue
		text = text.replace(/@9[Ff]@/g, "\x01h\x01w\x01" + "4\x01i"); // Blinking bright white on blue

		// Green background, blinking foreground
		text = text.replace(/@[Aa]0@/g, "\x01n\x01k\x01" + "2\x01i"); // Blinking black on green
		text = text.replace(/@[Aa]1@/g, "\x01n\x01b\x01" + "2\x01i"); // Blinking blue on green
		text = text.replace(/@[Aa]2@/g, "\x01n\x01g\x01" + "2\x01i"); // Blinking green on green
		text = text.replace(/@[Aa]3@/g, "\x01n\x01c\x01" + "2\x01i"); // Blinking cyan on green
		text = text.replace(/@[Aa]4@/g, "\x01n\x01r\x01" + "2\x01i"); // Blinking red on green
		text = text.replace(/@[Aa]5@/g, "\x01n\x01m\x01" + "2\x01i"); // Blinking magenta on green
		text = text.replace(/@[Aa]6@/g, "\x01n\x01y\x01" + "2\x01i"); // Blinking yellow/brown on green
		text = text.replace(/@[Aa]7@/g, "\x01n\x01w\x01" + "2\x01i"); // Blinking white on green
		text = text.replace(/@[Aa]8@/g, "\x01h\x01k\x01" + "2\x01i"); // Blinking bright black on green
		text = text.replace(/@[Aa]9@/g, "\x01h\x01b\x01" + "2\x01i"); // Blinking bright blue on green
		text = text.replace(/@[Aa][Aa]@/g, "\x01h\x01g\x01" + "2\x01i"); // Blinking bright green on green
		text = text.replace(/@[Aa][Bb]@/g, "\x01h\x01c\x01" + "2\x01i"); // Blinking bright cyan on green
		text = text.replace(/@[Aa][Cc]@/g, "\x01h\x01r\x01" + "2\x01i"); // Blinking bright red on green
		text = text.replace(/@[Aa][Dd]@/g, "\x01h\x01m\x01" + "2\x01i"); // Blinking bright magenta on green
		text = text.replace(/@[Aa][Ee]@/g, "\x01h\x01y\x01" + "2\x01i"); // Blinking bright yellow on green
		text = text.replace(/@[Aa][Ff]@/g, "\x01h\x01w\x01" + "2\x01i"); // Blinking bright white on green

		// Cyan background, blinking foreground
		text = text.replace(/@[Bb]0@/g, "\x01n\x01k\x01" + "6\x01i"); // Blinking black on cyan
		text = text.replace(/@[Bb]1@/g, "\x01n\x01b\x01" + "6\x01i"); // Blinking blue on cyan
		text = text.replace(/@[Bb]2@/g, "\x01n\x01g\x01" + "6\x01i"); // Blinking green on cyan
		text = text.replace(/@[Bb]3@/g, "\x01n\x01c\x01" + "6\x01i"); // Blinking cyan on cyan
		text = text.replace(/@[Bb]4@/g, "\x01n\x01r\x01" + "6\x01i"); // Blinking red on cyan
		text = text.replace(/@[Bb]5@/g, "\x01n\x01m\x01" + "6\x01i"); // Blinking magenta on cyan
		text = text.replace(/@[Bb]6@/g, "\x01n\x01y\x01" + "6\x01i"); // Blinking yellow/brown on cyan
		text = text.replace(/@[Bb]7@/g, "\x01n\x01w\x01" + "6\x01i"); // Blinking white on cyan
		text = text.replace(/@[Bb]8@/g, "\x01h\x01k\x01" + "6\x01i"); // Blinking bright black on cyan
		text = text.replace(/@[Bb]9@/g, "\x01h\x01b\x01" + "6\x01i"); // Blinking bright blue on cyan
		text = text.replace(/@[Bb][Aa]@/g, "\x01h\x01g\x01" + "6\x01i"); // Blinking bright green on cyan
		text = text.replace(/@[Bb][Bb]@/g, "\x01h\x01c\x01" + "6\x01i"); // Blinking bright cyan on cyan
		text = text.replace(/@[Bb][Cc]@/g, "\x01h\x01r\x01" + "6\x01i"); // Blinking bright red on cyan
		text = text.replace(/@[Bb][Dd]@/g, "\x01h\x01m\x01" + "6\x01i"); // Blinking bright magenta on cyan
		text = text.replace(/@[Bb][Ee]@/g, "\x01h\x01y\x01" + "6\x01i"); // Blinking bright yellow on cyan
		text = text.replace(/@[Bb][Ff]@/g, "\x01h\x01w\x01" + "6\x01i"); // Blinking bright white on cyan

		// Red background, blinking foreground
		text = text.replace(/@[Cc]0@/g, "\x01n\x01k\x01" + "1\x01i"); // Blinking black on red
		text = text.replace(/@[Cc]1@/g, "\x01n\x01b\x01" + "1\x01i"); // Blinking blue on red
		text = text.replace(/@[Cc]2@/g, "\x01n\x01g\x01" + "1\x01i"); // Blinking green on red
		text = text.replace(/@[Cc]3@/g, "\x01n\x01c\x01" + "1\x01i"); // Blinking cyan on red
		text = text.replace(/@[Cc]4@/g, "\x01n\x01r\x01" + "1\x01i"); // Blinking red on red
		text = text.replace(/@[Cc]5@/g, "\x01n\x01m\x01" + "1\x01i"); // Blinking magenta on red
		text = text.replace(/@[Cc]6@/g, "\x01n\x01y\x01" + "1\x01i"); // Blinking yellow/brown on red
		text = text.replace(/@[Cc]7@/g, "\x01n\x01w\x01" + "1\x01i"); // Blinking white on red
		text = text.replace(/@[Cc]8@/g, "\x01h\x01k\x01" + "1\x01i"); // Blinking bright black on red
		text = text.replace(/@[Cc]9@/g, "\x01h\x01b\x01" + "1\x01i"); // Blinking bright blue on red
		text = text.replace(/@[Cc][Aa]@/g, "\x01h\x01g\x01" + "1\x01i"); // Blinking bright green on red
		text = text.replace(/@[Cc][Bb]@/g, "\x01h\x01c\x01" + "1\x01i"); // Blinking bright cyan on red
		text = text.replace(/@[Cc][Cc]@/g, "\x01h\x01r\x01" + "1\x01i"); // Blinking bright red on red
		text = text.replace(/@[Cc][Dd]@/g, "\x01h\x01m\x01" + "1\x01i"); // Blinking bright magenta on red
		text = text.replace(/@[Cc][Ee]@/g, "\x01h\x01y\x01" + "1\x01i"); // Blinking bright yellow on red
		text = text.replace(/@[Cc][Ff]@/g, "\x01h\x01w\x01" + "1\x01i"); // Blinking bright white on red

		// Magenta background, blinking foreground
		text = text.replace(/@[Dd]0@/g, "\x01n\x01k\x01" + "5\x01i"); // Blinking black on magenta
		text = text.replace(/@[Dd]1@/g, "\x01n\x01b\x01" + "5\x01i"); // Blinking blue on magenta
		text = text.replace(/@[Dd]2@/g, "\x01n\x01g\x01" + "5\x01i"); // Blinking green on magenta
		text = text.replace(/@[Dd]3@/g, "\x01n\x01c\x01" + "5\x01i"); // Blinking cyan on magenta
		text = text.replace(/@[Dd]4@/g, "\x01n\x01r\x01" + "5\x01i"); // Blinking red on magenta
		text = text.replace(/@[Dd]5@/g, "\x01n\x01m\x01" + "5\x01i"); // Blinking magenta on magenta
		text = text.replace(/@[Dd]6@/g, "\x01n\x01y\x01" + "5\x01i"); // Blinking yellow/brown on magenta
		text = text.replace(/@[Dd]7@/g, "\x01n\x01w\x01" + "5\x01i"); // Blinking white on magenta
		text = text.replace(/@[Dd]8@/g, "\x01h\x01k\x01" + "5\x01i"); // Blinking bright black on magenta
		text = text.replace(/@[Dd]9@/g, "\x01h\x01b\x01" + "5\x01i"); // Blinking bright blue on magenta
		text = text.replace(/@[Dd][Aa]@/g, "\x01h\x01g\x01" + "5\x01i"); // Blinking bright green on magenta
		text = text.replace(/@[Dd][Bb]@/g, "\x01h\x01c\x01" + "5\x01i"); // Blinking bright cyan on magenta
		text = text.replace(/@[Dd][Cc]@/g, "\x01h\x01r\x01" + "5\x01i"); // Blinking bright red on magenta
		text = text.replace(/@[Dd][Dd]@/g, "\x01h\x01m\x01" + "5\x01i"); // Blinking bright magenta on magenta
		text = text.replace(/@[Dd][Ee]@/g, "\x01h\x01y\x01" + "5\x01i"); // Blinking bright yellow on magenta
		text = text.replace(/@[Dd][Ff]@/g, "\x01h\x01w\x01" + "5\x01i"); // Blinking bright white on magenta

		// Brown background, blinking foreground
		text = text.replace(/@[Ee]0@/g, "\x01n\x01k\x01" + "3\x01i"); // Blinking black on brown
		text = text.replace(/@[Ee]1@/g, "\x01n\x01b\x01" + "3\x01i"); // Blinking blue on brown
		text = text.replace(/@[Ee]2@/g, "\x01n\x01g\x01" + "3\x01i"); // Blinking green on brown
		text = text.replace(/@[Ee]3@/g, "\x01n\x01c\x01" + "3\x01i"); // Blinking cyan on brown
		text = text.replace(/@[Ee]4@/g, "\x01n\x01r\x01" + "3\x01i"); // Blinking red on brown
		text = text.replace(/@[Ee]5@/g, "\x01n\x01m\x01" + "3\x01i"); // Blinking magenta on brown
		text = text.replace(/@[Ee]6@/g, "\x01n\x01y\x01" + "3\x01i"); // Blinking yellow/brown on brown
		text = text.replace(/@[Ee]7@/g, "\x01n\x01w\x01" + "3\x01i"); // Blinking white on brown
		text = text.replace(/@[Ee]8@/g, "\x01h\x01k\x01" + "3\x01i"); // Blinking bright black on brown
		text = text.replace(/@[Ee]9@/g, "\x01h\x01b\x01" + "3\x01i"); // Blinking bright blue on brown
		text = text.replace(/@[Ee][Aa]@/g, "\x01h\x01g\x01" + "3\x01i"); // Blinking bright green on brown
		text = text.replace(/@[Ee][Bb]@/g, "\x01h\x01c\x01" + "3\x01i"); // Blinking bright cyan on brown
		text = text.replace(/@[Ee][Cc]@/g, "\x01h\x01r\x01" + "3\x01i"); // Blinking bright red on brown
		text = text.replace(/@[Ee][Dd]@/g, "\x01h\x01m\x01" + "3\x01i"); // Blinking bright magenta on brown
		text = text.replace(/@[Ee][Ee]@/g, "\x01h\x01y\x01" + "3\x01i"); // Blinking bright yellow on brown
		text = text.replace(/@[Ee][Ff]@/g, "\x01h\x01w\x01" + "3\x01i"); // Blinking bright white on brown

		// White background, blinking foreground
		text = text.replace(/@[Ff]0@/g, "\x01n\x01k\x01" + "7\x01i"); // Blinking black on white
		text = text.replace(/@[Ff]1@/g, "\x01n\x01b\x01" + "7\x01i"); // Blinking blue on white
		text = text.replace(/@[Ff]2@/g, "\x01n\x01g\x01" + "7\x01i"); // Blinking green on white
		text = text.replace(/@[Ff]3@/g, "\x01n\x01c\x01" + "7\x01i"); // Blinking cyan on white
		text = text.replace(/@[Ff]4@/g, "\x01n\x01r\x01" + "7\x01i"); // Blinking red on white
		text = text.replace(/@[Ff]5@/g, "\x01n\x01m\x01" + "7\x01i"); // Blinking magenta on white
		text = text.replace(/@[Ff]6@/g, "\x01n\x01y\x01" + "7\x01i"); // Blinking yellow/brown on white
		text = text.replace(/@[Ff]7@/g, "\x01n\x01w\x01" + "7\x01i"); // Blinking white on white
		text = text.replace(/@[Ff]8@/g, "\x01h\x01k\x01" + "7\x01i"); // Blinking bright black on white
		text = text.replace(/@[Ff]9@/g, "\x01h\x01b\x01" + "7\x01i"); // Blinking bright blue on white
		text = text.replace(/@[Ff][Aa]@/g, "\x01h\x01g\x01" + "7\x01i"); // Blinking bright green on white
		text = text.replace(/@[Ff][Bb]@/g, "\x01h\x01c\x01" + "7\x01i"); // Blinking bright cyan on white
		text = text.replace(/@[Ff][Cc]@/g, "\x01h\x01r\x01" + "7\x01i"); // Blinking bright red on white
		text = text.replace(/@[Ff][Dd]@/g, "\x01h\x01m\x01" + "7\x01i"); // Blinking bright magenta on white
		text = text.replace(/@[Ff][Ee]@/g, "\x01h\x01y\x01" + "7\x01i"); // Blinking bright yellow on white
		text = text.replace(/@[Ff][Ff]@/g, "\x01h\x01w\x01" + "7\x01i"); // Blinking bright white on white

		return text;
	}
	else
		return pText; // No Wildcat-style attribute codes found, so just return the text.
}

// Converts Celerity attribute codes to Synchronet attribute codes.
//
// Parameters:
//  pText: A string containing the text to convert
//
// Return value: The text with the color codes converted
function celerityAttrsToSyncAttrs(pText)
{
	// First, see if the text has any Celerity-style attribute codes at
	// all.  We'll be performing a bunch of search & replace commands,
	// so we don't want to do all that work for nothing.. :)
	if (/\|[kbgcrmywdBGCRMYWS]/.test(pText))
	{
		// Using the \|S code (swap foreground & background)

		// Blue background
		var text = pText.replace(/\|b\|S\|k/g, "\x01n\x01k\x01" + "4"); // Black on blue
		text = text.replace(/\|b\|S\|b/g, "\x01n\x01b\x01" + "4"); // Blue on blue
		text = text.replace(/\|b\|S\|g/g, "\x01n\x01g\x01" + "4"); // Green on blue
		text = text.replace(/\|b\|S\|c/g, "\x01n\x01c\x01" + "4"); // Cyan on blue
		text = text.replace(/\|b\|S\|r/g, "\x01n\x01r\x01" + "4"); // Red on blue
		text = text.replace(/\|b\|S\|m/g, "\x01n\x01m\x01" + "4"); // Magenta on blue
		text = text.replace(/\|b\|S\|y/g, "\x01n\x01y\x01" + "4"); // Yellow/brown on blue
		text = text.replace(/\|b\|S\|w/g, "\x01n\x01w\x01" + "4"); // White on blue
		text = text.replace(/\|b\|S\|d/g, "\x01h\x01k\x01" + "4"); // Bright black on blue
		text = text.replace(/\|b\|S\|B/g, "\x01h\x01b\x01" + "4"); // Bright blue on blue
		text = text.replace(/\|b\|S\|G/g, "\x01h\x01g\x01" + "4"); // Bright green on blue
		text = text.replace(/\|b\|S\|C/g, "\x01h\x01c\x01" + "4"); // Bright cyan on blue
		text = text.replace(/\|b\|S\|R/g, "\x01h\x01r\x01" + "4"); // Bright red on blue
		text = text.replace(/\|b\|S\|M/g, "\x01h\x01m\x01" + "4"); // Bright magenta on blue
		text = text.replace(/\|b\|S\|Y/g, "\x01h\x01y\x01" + "4"); // Yellow on blue
		text = text.replace(/\|b\|S\|W/g, "\x01h\x01w\x01" + "4"); // Bright white on blue

		// Green background
		text = text.replace(/\|g\|S\|k/g, "\x01n\x01k\x01" + "2"); // Black on green
		text = text.replace(/\|g\|S\|b/g, "\x01n\x01b\x01" + "2"); // Blue on green
		text = text.replace(/\|g\|S\|g/g, "\x01n\x01g\x01" + "2"); // Green on green
		text = text.replace(/\|g\|S\|c/g, "\x01n\x01c\x01" + "2"); // Cyan on green
		text = text.replace(/\|g\|S\|r/g, "\x01n\x01r\x01" + "2"); // Red on green
		text = text.replace(/\|g\|S\|m/g, "\x01n\x01m\x01" + "2"); // Magenta on green
		text = text.replace(/\|g\|S\|y/g, "\x01n\x01y\x01" + "2"); // Yellow/brown on green
		text = text.replace(/\|g\|S\|w/g, "\x01n\x01w\x01" + "2"); // White on green
		text = text.replace(/\|g\|S\|d/g, "\x01h\x01k\x01" + "2"); // Bright black on green
		text = text.replace(/\|g\|S\|B/g, "\x01h\x01b\x01" + "2"); // Bright blue on green
		text = text.replace(/\|g\|S\|G/g, "\x01h\x01g\x01" + "2"); // Bright green on green
		text = text.replace(/\|g\|S\|C/g, "\x01h\x01c\x01" + "2"); // Bright cyan on green
		text = text.replace(/\|g\|S\|R/g, "\x01h\x01r\x01" + "2"); // Bright red on green
		text = text.replace(/\|g\|S\|M/g, "\x01h\x01m\x01" + "2"); // Bright magenta on green
		text = text.replace(/\|g\|S\|Y/g, "\x01h\x01y\x01" + "2"); // Yellow on green
		text = text.replace(/\|g\|S\|W/g, "\x01h\x01w\x01" + "2"); // Bright white on green

		// Cyan background
		text = text.replace(/\|c\|S\|k/g, "\x01n\x01k\x01" + "6"); // Black on cyan
		text = text.replace(/\|c\|S\|b/g, "\x01n\x01b\x01" + "6"); // Blue on cyan
		text = text.replace(/\|c\|S\|g/g, "\x01n\x01g\x01" + "6"); // Green on cyan
		text = text.replace(/\|c\|S\|c/g, "\x01n\x01c\x01" + "6"); // Cyan on cyan
		text = text.replace(/\|c\|S\|r/g, "\x01n\x01r\x01" + "6"); // Red on cyan
		text = text.replace(/\|c\|S\|m/g, "\x01n\x01m\x01" + "6"); // Magenta on cyan
		text = text.replace(/\|c\|S\|y/g, "\x01n\x01y\x01" + "6"); // Yellow/brown on cyan
		text = text.replace(/\|c\|S\|w/g, "\x01n\x01w\x01" + "6"); // White on cyan
		text = text.replace(/\|c\|S\|d/g, "\x01h\x01k\x01" + "6"); // Bright black on cyan
		text = text.replace(/\|c\|S\|B/g, "\x01h\x01b\x01" + "6"); // Bright blue on cyan
		text = text.replace(/\|c\|S\|G/g, "\x01h\x01g\x01" + "6"); // Bright green on cyan
		text = text.replace(/\|c\|S\|C/g, "\x01h\x01c\x01" + "6"); // Bright cyan on cyan
		text = text.replace(/\|c\|S\|R/g, "\x01h\x01r\x01" + "6"); // Bright red on cyan
		text = text.replace(/\|c\|S\|M/g, "\x01h\x01m\x01" + "6"); // Bright magenta on cyan
		text = text.replace(/\|c\|S\|Y/g, "\x01h\x01y\x01" + "6"); // Yellow on cyan
		text = text.replace(/\|c\|S\|W/g, "\x01h\x01w\x01" + "6"); // Bright white on cyan

		// Red background
		text = text.replace(/\|r\|S\|k/g, "\x01n\x01k\x01" + "1"); // Black on red
		text = text.replace(/\|r\|S\|b/g, "\x01n\x01b\x01" + "1"); // Blue on red
		text = text.replace(/\|r\|S\|g/g, "\x01n\x01g\x01" + "1"); // Green on red
		text = text.replace(/\|r\|S\|c/g, "\x01n\x01c\x01" + "1"); // Cyan on red
		text = text.replace(/\|r\|S\|r/g, "\x01n\x01r\x01" + "1"); // Red on red
		text = text.replace(/\|r\|S\|m/g, "\x01n\x01m\x01" + "1"); // Magenta on red
		text = text.replace(/\|r\|S\|y/g, "\x01n\x01y\x01" + "1"); // Yellow/brown on red
		text = text.replace(/\|r\|S\|w/g, "\x01n\x01w\x01" + "1"); // White on red
		text = text.replace(/\|r\|S\|d/g, "\x01h\x01k\x01" + "1"); // Bright black on red
		text = text.replace(/\|r\|S\|B/g, "\x01h\x01b\x01" + "1"); // Bright blue on red
		text = text.replace(/\|r\|S\|G/g, "\x01h\x01g\x01" + "1"); // Bright green on red
		text = text.replace(/\|r\|S\|C/g, "\x01h\x01c\x01" + "1"); // Bright cyan on red
		text = text.replace(/\|r\|S\|R/g, "\x01h\x01r\x01" + "1"); // Bright red on red
		text = text.replace(/\|r\|S\|M/g, "\x01h\x01m\x01" + "1"); // Bright magenta on red
		text = text.replace(/\|r\|S\|Y/g, "\x01h\x01y\x01" + "1"); // Yellow on red
		text = text.replace(/\|r\|S\|W/g, "\x01h\x01w\x01" + "1"); // Bright white on red

		// Magenta background
		text = text.replace(/\|m\|S\|k/g, "\x01n\x01k\x01" + "5"); // Black on magenta
		text = text.replace(/\|m\|S\|b/g, "\x01n\x01b\x01" + "5"); // Blue on magenta
		text = text.replace(/\|m\|S\|g/g, "\x01n\x01g\x01" + "5"); // Green on magenta
		text = text.replace(/\|m\|S\|c/g, "\x01n\x01c\x01" + "5"); // Cyan on magenta
		text = text.replace(/\|m\|S\|r/g, "\x01n\x01r\x01" + "5"); // Red on magenta
		text = text.replace(/\|m\|S\|m/g, "\x01n\x01m\x01" + "5"); // Magenta on magenta
		text = text.replace(/\|m\|S\|y/g, "\x01n\x01y\x01" + "5"); // Yellow/brown on magenta
		text = text.replace(/\|m\|S\|w/g, "\x01n\x01w\x01" + "5"); // White on magenta
		text = text.replace(/\|m\|S\|d/g, "\x01h\x01k\x01" + "5"); // Bright black on magenta
		text = text.replace(/\|m\|S\|B/g, "\x01h\x01b\x01" + "5"); // Bright blue on magenta
		text = text.replace(/\|m\|S\|G/g, "\x01h\x01g\x01" + "5"); // Bright green on magenta
		text = text.replace(/\|m\|S\|C/g, "\x01h\x01c\x01" + "5"); // Bright cyan on magenta
		text = text.replace(/\|m\|S\|R/g, "\x01h\x01r\x01" + "5"); // Bright red on magenta
		text = text.replace(/\|m\|S\|M/g, "\x01h\x01m\x01" + "5"); // Bright magenta on magenta
		text = text.replace(/\|m\|S\|Y/g, "\x01h\x01y\x01" + "5"); // Yellow on magenta
		text = text.replace(/\|m\|S\|W/g, "\x01h\x01w\x01" + "5"); // Bright white on magenta

		// Brown background
		text = text.replace(/\|y\|S\|k/g, "\x01n\x01k\x01" + "3"); // Black on brown
		text = text.replace(/\|y\|S\|b/g, "\x01n\x01b\x01" + "3"); // Blue on brown
		text = text.replace(/\|y\|S\|g/g, "\x01n\x01g\x01" + "3"); // Green on brown
		text = text.replace(/\|y\|S\|c/g, "\x01n\x01c\x01" + "3"); // Cyan on brown
		text = text.replace(/\|y\|S\|r/g, "\x01n\x01r\x01" + "3"); // Red on brown
		text = text.replace(/\|y\|S\|m/g, "\x01n\x01m\x01" + "3"); // Magenta on brown
		text = text.replace(/\|y\|S\|y/g, "\x01n\x01y\x01" + "3"); // Yellow/brown on brown
		text = text.replace(/\|y\|S\|w/g, "\x01n\x01w\x01" + "3"); // White on brown
		text = text.replace(/\|y\|S\|d/g, "\x01h\x01k\x01" + "3"); // Bright black on brown
		text = text.replace(/\|y\|S\|B/g, "\x01h\x01b\x01" + "3"); // Bright blue on brown
		text = text.replace(/\|y\|S\|G/g, "\x01h\x01g\x01" + "3"); // Bright green on brown
		text = text.replace(/\|y\|S\|C/g, "\x01h\x01c\x01" + "3"); // Bright cyan on brown
		text = text.replace(/\|y\|S\|R/g, "\x01h\x01r\x01" + "3"); // Bright red on brown
		text = text.replace(/\|y\|S\|M/g, "\x01h\x01m\x01" + "3"); // Bright magenta on brown
		text = text.replace(/\|y\|S\|Y/g, "\x01h\x01y\x01" + "3"); // Yellow on brown
		text = text.replace(/\|y\|S\|W/g, "\x01h\x01w\x01" + "3"); // Bright white on brown

		// White background
		text = text.replace(/\|w\|S\|k/g, "\x01n\x01k\x01" + "7"); // Black on white
		text = text.replace(/\|w\|S\|b/g, "\x01n\x01b\x01" + "7"); // Blue on white
		text = text.replace(/\|w\|S\|g/g, "\x01n\x01g\x01" + "7"); // Green on white
		text = text.replace(/\|w\|S\|c/g, "\x01n\x01c\x01" + "7"); // Cyan on white
		text = text.replace(/\|w\|S\|r/g, "\x01n\x01r\x01" + "7"); // Red on white
		text = text.replace(/\|w\|S\|m/g, "\x01n\x01m\x01" + "7"); // Magenta on white
		text = text.replace(/\|w\|S\|y/g, "\x01n\x01y\x01" + "7"); // Yellow/brown on white
		text = text.replace(/\|w\|S\|w/g, "\x01n\x01w\x01" + "7"); // White on white
		text = text.replace(/\|w\|S\|d/g, "\x01h\x01k\x01" + "7"); // Bright black on white
		text = text.replace(/\|w\|S\|B/g, "\x01h\x01b\x01" + "7"); // Bright blue on white
		text = text.replace(/\|w\|S\|G/g, "\x01h\x01g\x01" + "7"); // Bright green on white
		text = text.replace(/\|w\|S\|C/g, "\x01h\x01c\x01" + "7"); // Bright cyan on white
		text = text.replace(/\|w\|S\|R/g, "\x01h\x01r\x01" + "7"); // Bright red on white
		text = text.replace(/\|w\|S\|M/g, "\x01h\x01m\x01" + "7"); // Bright magenta on white
		text = text.replace(/\|w\|S\|Y/g, "\x01h\x01y\x01" + "7"); // Yellow on white
		text = text.replace(/\|w\|S\|W/g, "\x01h\x01w\x01" + "7"); // Bright white on white

		// Colors on black background
		text = text.replace(/\|k/g, "\x01n\x01k\x01" + "0");  // Black on black
		text = text.replace(/\|k\|S\|k/g, "\x01n\x01k\x01" + "0"); // Black on black
		text = text.replace(/\|b/g, "\x01n\x01b\x01" + "0");       // Blue on black
		text = text.replace(/\|k\|S\|b/g, "\x01n\x01b\x01" + "0"); // Blue on black
		text = text.replace(/\|g/g, "\x01n\x01g\x01" + "0");       // Green on black
		text = text.replace(/\|k\|S\|g/g, "\x01n\x01g\x01" + "0"); // Green on black
		text = text.replace(/\|c/g, "\x01n\x01c\x01" + "0");       // Cyan on black
		text = text.replace(/\|k\|S\|c/g, "\x01n\x01c\x01" + "0"); // Cyan on black
		text = text.replace(/\|r/g, "\x01n\x01r\x01" + "0");       // Red on black
		text = text.replace(/\|k\|S\|r/g, "\x01n\x01r\x01" + "0"); // Red on black
		text = text.replace(/\|m/g, "\x01n\x01m\x01" + "0");       // Magenta on black
		text = text.replace(/\|k\|S\|m/g, "\x01n\x01m\x01" + "0"); // Magenta on black
		text = text.replace(/\|y/g, "\x01n\x01y\x01" + "0");       // Yellow/brown on black
		text = text.replace(/\|k\|S\|y/g, "\x01n\x01y\x01" + "0"); // Yellow/brown on black
		text = text.replace(/\|w/g, "\x01n\x01w\x01" + "0");       // White on black
		text = text.replace(/\|k\|S\|w/g, "\x01n\x01w\x01" + "0"); // White on black
		text = text.replace(/\|d/g, "\x01h\x01k\x01" + "0");       // Bright black on black
		text = text.replace(/\|k\|S\|d/g, "\x01h\x01k\x01" + "0"); // Bright black on black
		text = text.replace(/\|B/g, "\x01h\x01b\x01" + "0");       // Bright blue on black
		text = text.replace(/\|k\|S\|B/g, "\x01h\x01b\x01" + "0"); // Bright blue on black
		text = text.replace(/\|G/g, "\x01h\x01g\x01" + "0");       // Bright green on black
		text = text.replace(/\|k\|S\|G/g, "\x01h\x01g\x01" + "0"); // Bright green on black
		text = text.replace(/\|C/g, "\x01h\x01c\x01" + "0");       // Bright cyan on black
		text = text.replace(/\|k\|S\|C/g, "\x01h\x01c\x01" + "0"); // Bright cyan on black
		text = text.replace(/\|R/g, "\x01h\x01r\x01" + "0");       // Bright red on black
		text = text.replace(/\|k\|S\|R/g, "\x01h\x01r\x01" + "0"); // Bright red on black
		text = text.replace(/\|M/g, "\x01h\x01m\x01" + "0");       // Bright magenta on black
		text = text.replace(/\|k\|S\|M/g, "\x01h\x01m\x01" + "0"); // Bright magenta on black
		text = text.replace(/\|Y/g, "\x01h\x01y\x01" + "0");       // Yellow on black
		text = text.replace(/\|k\|S\|Y/g, "\x01h\x01y\x01" + "0"); // Yellow on black
		text = text.replace(/\|W/g, "\x01h\x01w\x01" + "0");       // Bright white on black
		text = text.replace(/\|k\|S\|W/g, "\x01h\x01w\x01" + "0"); // Bright white on black

		return text;
	}
	else
		return pText; // No Celerity-style attribute codes found, so just return the text.
}

// Converts Renegade attribute (color) codes to Synchronet attribute codes.
//
// Parameters:
//  pText: A string containing the text to convert
//
// Return value: The text with the color codes converted
function renegadeAttrsToSyncAttrs(pText)
{
	// First, see if the text has any Renegade-style attribute codes at
	// all.  We'll be performing a bunch of search & replace commands,
	// so we don't want to do all that work for nothing.. :)
	if (/\|[0-3][0-9]/.test(pText))
	{
		var text = pText.replace(/\|00/g, "\x01n\x01k"); // Normal black
		text = text.replace(/\|01/g, "\x01n\x01b"); // Normal blue
		text = text.replace(/\|02/g, "\x01n\x01g"); // Normal green
		text = text.replace(/\|03/g, "\x01n\x01c"); // Normal cyan
		text = text.replace(/\|04/g, "\x01n\x01r"); // Normal red
		text = text.replace(/\|05/g, "\x01n\x01m"); // Normal magenta
		text = text.replace(/\|06/g, "\x01n\x01y"); // Normal brown
		text = text.replace(/\|07/g, "\x01n\x01w"); // Normal white
		text = text.replace(/\|08/g, "\x01n\x01k\x01h"); // High intensity black
		text = text.replace(/\|09/g, "\x01n\x01b\x01h"); // High intensity blue
		text = text.replace(/\|10/g, "\x01n\x01g\x01h"); // High intensity green
		text = text.replace(/\|11/g, "\x01n\x01c\x01h"); // High intensity cyan
		text = text.replace(/\|12/g, "\x01n\x01r\x01h"); // High intensity red
		text = text.replace(/\|13/g, "\x01n\x01m\x01h"); // High intensity magenta
		text = text.replace(/\|14/g, "\x01n\x01y\x01h"); // Yellow (high intensity brown)
		text = text.replace(/\|15/g, "\x01n\x01w\x01h"); // High intensity white
		text = text.replace(/\|16/g, "\x01" + "0"); // Background black
		text = text.replace(/\|17/g, "\x01" + "4"); // Background blue
		text = text.replace(/\|18/g, "\x01" + "2"); // Background green
		text = text.replace(/\|19/g, "\x01" + "6"); // Background cyan
		text = text.replace(/\|20/g, "\x01" + "1"); // Background red
		text = text.replace(/\|21/g, "\x01" + "5"); // Background magenta
		text = text.replace(/\|22/g, "\x01" + "3"); // Background brown
		text = text.replace(/\|23/g, "\x01" + "7"); // Background white
		text = text.replace(/\|24/g, "\x01i\x01w\x01" + "0"); // Blinking white on black
		text = text.replace(/\|25/g, "\x01i\x01w\x01" + "4"); // Blinking white on blue
		text = text.replace(/\|26/g, "\x01i\x01w\x01" + "2"); // Blinking white on green
		text = text.replace(/\|27/g, "\x01i\x01w\x01" + "6"); // Blinking white on cyan
		text = text.replace(/\|28/g, "\x01i\x01w\x01" + "1"); // Blinking white on red
		text = text.replace(/\|29/g, "\x01i\x01w\x01" + "5"); // Blinking white on magenta
		text = text.replace(/\|30/g, "\x01i\x01w\x01" + "3"); // Blinking white on yellow/brown
		text = text.replace(/\|31/g, "\x01i\x01w\x01" + "7"); // Blinking white on white
		return text;
	}
	else
		return pText; // No Renegade-style attribute codes found, so just return the text.
}

// Converts ANSI attribute codes to Synchronet attribute codes.  This
// is incomplete and doesn't convert all ANSI codes perfectly to Synchronet
// attribute codes.
//
// Parameters:
//  pText: A string containing the text to convert
//
// Return value: The text with ANSI codes converted to Synchronet attribute codes
function ANSIAttrsToSyncAttrs(pText)
{
	// TODO: Test & update this some more..  Not sure if this is working 100% right.

	// Web pages with ANSI code information:
	// http://pueblo.sourceforge.net/doc/manual/ansi_color_codes.html
	// http://ascii-table.com/ansi-escape-sequences.php
	// http://stackoverflow.com/questions/4842424/list-of-ansi-color-escape-sequences

	// If the string has ANSI codes in it, then go ahead and replace ANSI
	// codes with Synchronet attribute codes.
	if (textHasANSICodes(pText))
	{
		// Attributes
		var txt = pText.replace(/\033\[0[mM]/g, "\x01n"); // All attributes off
		txt = txt.replace(/\033\[1[mM]/g, "\x01h"); // Bold on (use high intensity)
		txt = txt.replace(/\033\[5[mM]/g, "\x01i"); // Blink on
		// Foreground colors
		txt = txt.replace(/\033\[30[mM]/g, "\x01k"); // Black foreground
		txt = txt.replace(/\033\[31[mM]/g, "\x01r"); // Red foreground
		txt = txt.replace(/\033\[32[mM]/g, "\x01g"); // Green foreground
		txt = txt.replace(/\033\[33[mM]/g, "\x01y"); // Yellow foreground
		txt = txt.replace(/\033\[34[mM]/g, "\x01b"); // Blue foreground
		txt = txt.replace(/\033\[35[mM]/g, "\x01m"); // Magenta foreground
		txt = txt.replace(/\033\[36[mM]/g, "\x01c"); // Cyan foreground
		txt = txt.replace(/\033\[37[mM]/g, "\x01w"); // White foreground
		// Background colors
		txt = txt.replace(/\033\[40[mM]/g, "\x01" + "0"); // Black background
		txt = txt.replace(/\033\[41[mM]/g, "\x01" + "1"); // Red background
		txt = txt.replace(/\033\[42[mM]/g, "\x01" + "2"); // Green background
		txt = txt.replace(/\033\[43[mM]/g, "\x01" + "3"); // Yellow background
		txt = txt.replace(/\033\[44[mM]/g, "\x01" + "4"); // Blue background
		txt = txt.replace(/\033\[45[mM]/g, "\x01" + "5"); // Magenta background
		txt = txt.replace(/\033\[46[mM]/g, "\x01" + "6"); // Cyan background
		txt = txt.replace(/\033\[47[mM]/g, "\x01" + "7"); // White background
		// Convert ;-delimited modes (such as \033[Value;...;Valuem)
		txt = ANSIMultiConvertToSyncCodes(txt);
		// Remove ANSI codes that are not wanted (such as moving the cursor, etc.)
		txt = txt.replace(/\033\[[0-9]+[aA]/g, ""); // Cursor up
		txt = txt.replace(/\033\[[0-9]+[bB]/g, ""); // Cursor down
		txt = txt.replace(/\033\[[0-9]+[cC]/g, ""); // Cursor forward
		txt = txt.replace(/\033\[[0-9]+[dD]/g, ""); // Cursor backward
		txt = txt.replace(/\033\[[0-9]+;[0-9]+[hH]/g, ""); // Cursor position
		txt = txt.replace(/\033\[[0-9]+;[0-9]+[fF]/g, ""); // Cursor position
		txt = txt.replace(/\033\[[sS]/g, ""); // Restore cursor position
		txt = txt.replace(/\033\[2[jJ]/g, ""); // Erase display
		txt = txt.replace(/\033\[[kK]/g, ""); // Erase line
		txt = txt.replace(/\033\[=[0-9]+[hH]/g, ""); // Set various screen modes
		txt = txt.replace(/\033\[=[0-9]+[lL]/g, ""); // Reset various screen modes
		return txt;
	}
	else
		return pText; // No ANSI codes found, so just return the text.
}

// Returns whether or not some text has any ANSI codes in it.
//
// Parameters:
//  pText: The text to test
//
// Return value: Boolean - Whether or not the text has ANSI codes in it
function textHasANSICodes(pText)
{
	
	return(/\033\[[0-9]+[mM]/.test(pText) || /\033\[[0-9]+(;[0-9]+)+[mM]/.test(pText) ||
	       /\033\[[0-9]+[aAbBcCdD]/.test(pText) || /\033\[[0-9]+;[0-9]+[hHfF]/.test(pText) ||
	       /\033\[[sSuUkK]/.test(pText) || /\033\[2[jJ]/.test(pText));
	/*
	var regex1 = new RegExp(ascii(27) + "\[[0-9]+[mM]");
	var regex2 = new RegExp(ascii(27) + "\[[0-9]+(;[0-9]+)+[mM]");
	var regex3 = new RegExp(ascii(27) + "\[[0-9]+[aAbBcCdD]");
	var regex4 = new RegExp(ascii(27) + "\[[0-9]+;[0-9]+[hHfF]");
	var regex5 = new RegExp(ascii(27) + "\[[sSuUkK]");
	var regex6 = new RegExp(ascii(27) + "\[2[jJ]");
	return(regex1.test(pText) || regex2.test(pText) || regex3.test(pText) ||
	       regex4.test(pText) || regex5.test(pText) || regex6.test(pText));
	*/
}

// Converts ANSI ;-delimited modes (such as [Value;...;Valuem) to Synchronet
// attribute codes
//
// Parameters:
//  pText: The text with ANSI ;-delimited modes to convert
//
// Return value: The text with ANSI ;-delimited codes converted to Synchronet attributes
function ANSIMultiConvertToSyncCodes(pText)
{
	var multiMatches = pText.match(/\033\[[0-9]+(;[0-9]+)+m/g);
	if (multiMatches == null)
		return pText;
	var updatedText = pText;
	for (var i = 0; i < multiMatches.length; ++i)
	{
		// Copy the string, with the \033[ removed from the beginning and the
		// trailing 'm' removed
		var text = multiMatches[i].substr(2);
		text = text.substr(0, text.length-1);
		var codes = text.split(";");
		var syncCodes = "";
		for (var idx = 0; idx < codes.length; ++idx)
		{
			if (codes[idx] == "0") // All attributes off
				syncCodes += "\x01n";
			else if (codes[idx] == "1") // Bold on (high intensity)
				syncCodes += "\x01h";
			else if (codes[idx] == "5") // Blink on
				syncCodes += "\x01i";
			else if (codes[idx] == "30") // Black foreground
				syncCodes += "\x01k";
			else if (codes[idx] == "31") // Red foreground
				syncCodes += "\x01r";
			else if (codes[idx] == "32") // Green foreground
				syncCodes += "\x01g";
			else if (codes[idx] == "33") // Yellow foreground
				syncCodes += "\x01y";
			else if (codes[idx] == "34") // Blue foreground
				syncCodes += "\x01b";
			else if (codes[idx] == "35") // Magenta foreground
				syncCodes += "\x01m";
			else if (codes[idx] == "36") // Cyan foreground
				syncCodes += "\x01c";
			else if (codes[idx] == "37") // White foreground
				syncCodes += "\x01w";
			else if (codes[idx] == "40") // Black background
				syncCodes += "\x01" + "0";
			else if (codes[idx] == "41") // Red background
				syncCodes += "\x01" + "1";
			else if (codes[idx] == "42") // Green background
				syncCodes += "\x01" + "2";
			else if (codes[idx] == "43") // Yellow background
				syncCodes += "\x01" + "3";
			else if (codes[idx] == "44") // Blue background
				syncCodes += "\x01" + "4";
			else if (codes[idx] == "45") // Magenta background
				syncCodes += "\x01" + "5";
			else if (codes[idx] == "46") // Cyan background
				syncCodes += "\x01" + "6";
			else if (codes[idx] == "47") // White background
				syncCodes += "\x01" + "7";
		}
		updatedText = updatedText.replace(multiMatches[i], syncCodes);
	}
	return updatedText;
}

// Converts Y-style MCI attribute codes to Synchronet attribute codes
//
// Parameters:
//  pText: A string containing the text to convert
//
// Return value: The text with Y-style MCI attribute codes converted to Synchronet attribute codes
function YStyleMCIAttrsToSyncAttrs(pText)
{
	if (/\x19[cz]/.test(pText))
	{
		return pText.replace(/\x19c0/g,    "\x01n\x01k")  // Normal Black
		            .replace(/\x19c1/g,    "\x01n\x01r")  // Normal Red
		            .replace(/\x19c2/g,    "\x01n\x01g")  // Normal Green
		            .replace(/\x19c3/g,    "\x01n\x01y")  // Brown
		            .replace(/\x19c4/g,    "\x01n\x01b")  // Normal Blue
		            .replace(/\x19c5/g,    "\x01n\x01m")  // Normal Magenta
		            .replace(/\x19c6/g,    "\x01n\x01c")  // Normal Cyan
		            .replace(/\x19c7/g,    "\x01n\x01w")  // Normal White
		            .replace(/\x19c8/g,    "\x01k\x01h")  // High Intensity Black
		            .replace(/\x19c9/g,    "\x01r\x01h")  // High Intensity Red
		            .replace(/\x19c[Aa]/g, "\x01g\x01h")  // High Intensity Green
		            .replace(/\x19c[Bb]/g, "\x01y\x01h")  // Yellow
		            .replace(/\x19c[Cc]/g, "\x01b\x01h")  // High Intensity Blue
		            .replace(/\x19c[Dd]/g, "\x01m\x01h")  // High Intensity Magenta
		            .replace(/\x19c[Ee]/g, "\x01c\x01h")  // High Intensity Cyan
		            .replace(/\x19c[Ff]/g, "\x01w\x01h")  // High Intensity White
		            .replace(/\x19z0/g,    "\x010")       // Background Black
		            .replace(/\x19z1/g,    "\x011")       // Background Red
		            .replace(/\x19z2/g,    "\x012")       // Background Green
		            .replace(/\x19z3/g,    "\x013")       // Background Brown
		            .replace(/\x19z4/g,    "\x014")       // Background Blue
		            .replace(/\x19z5/g,    "\x015")       // Background Magenta
		            .replace(/\x19z6/g,    "\x016")       // Background Cyan
		            .replace(/\x19z7/g,    "\x017");      // Background White
	}
	else
		return pText; // Just return the original string if no "Y" MCI codes found.
}

// Converts non-Synchronet attribute codes in text to Synchronet attribute
// codes according to the toggle options in SCFG > Message Options > Extra
// Attribute Codes
//
// Parameters:
//  pText: The text to be converted
//  pConvertANSI: Optional boolean - Whether or not to convert ANSI.  Defaults to true.
//
// Return value: The text with various other system attribute codes converted
//               to Synchronet attribute codes, or not, depending on the toggle
//               options in Extra Attribute Codes in SCFG
function convertAttrsToSyncPerSysCfg(pText, pConvertANSI)
{
	var convertedText = pText;
	var convertANSI = (typeof(pConvertANSI) === "boolean" ? pConvertANSI : true);
	if (convertANSI)
	{
		// Convert any ANSI codes to Synchronet attribute codes.
		// Then convert other BBS attribute codes to Synchronet attribute
		// codes according to the current system configuration.
		convertedText = ANSIAttrsToSyncAttrs(convertedText);
	}
	if ((system.settings & SYS_RENEGADE) == SYS_RENEGADE)
		convertedText = renegadeAttrsToSyncAttrs(convertedText);
	if ((system.settings & SYS_WWIV) == SYS_WWIV)
		convertedText = WWIVAttrsToSyncAttrs(convertedText);
	if ((system.settings & SYS_CELERITY) == SYS_CELERITY)
		convertedText = celerityAttrsToSyncAttrs(convertedText);
	if ((system.settings & SYS_PCBOARD) == SYS_PCBOARD)
		convertedText = PCBoardAttrsToSyncAttrs(convertedText);
	if ((system.settings & SYS_WILDCAT) == SYS_WILDCAT)
		convertedText = wildcatAttrsToSyncAttrs(convertedText);
	return convertedText;
}

// Converts Synchronet attribute codes to ANSI ;-delimited modes (such as \033[Value;...;Valuem)
//
// Parameters:
//  pText: The text with Synchronet codes to convert
//
// Return value: The text with Synchronet attributes converted to ANSI ;-delimited codes
function syncAttrCodesToANSI(pText)
{
	// First, see if the text has any Synchronet attribute codes at
	// all.  We'll be performing a bunch of search & replace commands,
	// so we don't want to do all that work for nothing.. :)
	if (hasSyncAttrCodes(pText))
	{
		var ANSIESCCodeStart = "\x1B["; // ESC[
		var newText = pText.replace(/\x01n/gi, ANSIESCCodeStart + "0m"); // Normal
		newText = newText.replace(/\x01-/gi, ANSIESCCodeStart + "0m"); // Normal
		newText = newText.replace(/\x01_/gi, ANSIESCCodeStart + "0m"); // Normal
		newText = newText.replace(/\x01h/gi, ANSIESCCodeStart + "1m"); // High intensity/bold
		newText = newText.replace(/\x01i/gi, ANSIESCCodeStart + "5m"); // Blinking on
		newText = newText.replace(/\x01f/gi, ANSIESCCodeStart + "5m"); // Blinking on
		newText = newText.replace(/\x01k/gi, ANSIESCCodeStart + "30m"); // Black foreground
		newText = newText.replace(/\x01r/gi, ANSIESCCodeStart + "31m"); // Red foreground
		newText = newText.replace(/\x01g/gi, ANSIESCCodeStart + "32m"); // Green foreground
		newText = newText.replace(/\x01y/gi, ANSIESCCodeStart + "33m"); // Yellow/brown foreground
		newText = newText.replace(/\x01b/gi, ANSIESCCodeStart + "34m"); // Blue foreground
		newText = newText.replace(/\x01m/gi, ANSIESCCodeStart + "35m"); // Magenta foreground
		newText = newText.replace(/\x01c/gi, ANSIESCCodeStart + "36m"); // Cyan foreground
		newText = newText.replace(/\x01w/gi, ANSIESCCodeStart + "37m"); // White foreground
		newText = newText.replace(/\x01[0]/gi, ANSIESCCodeStart + "40m"); // Black background
		newText = newText.replace(/\x01[1]/gi, ANSIESCCodeStart + "41m"); // Red background
		newText = newText.replace(/\x01[2]/gi, ANSIESCCodeStart + "42m"); // Green background
		newText = newText.replace(/\x01[3]/gi, ANSIESCCodeStart + "43m"); // Yellow/brown background
		newText = newText.replace(/\x01[4]/gi, ANSIESCCodeStart + "44m"); // Blue background
		newText = newText.replace(/\x01[5]/gi, ANSIESCCodeStart + "45m"); // Magenta background
		newText = newText.replace(/\x01[6]/gi, ANSIESCCodeStart + "46m"); // Cyan background
		newText = newText.replace(/\x01[7]/gi, ANSIESCCodeStart + "47m"); // White background
		return newText;
	}
	else
		return pText; // No Synchronet-style attribute codes found, so just return the text.
}

// Returns whether a string has any Synchronet attribute codes
//
// Parameters:
//  pStr: the string to check
//
// Return value: Boolean - Whether or not the string has any Synchronet attribute codes
function hasSyncAttrCodes(pStr)
{
	return (pStr.search(/\x01[krgybmcwhifn\-_01234567]/i) > -1);
}
