/* This file contains DCTEdit-specific functions for SlyEdit.
 *
 * Author: Eric Oulashin (AKA Nightfox)
 * BBS: Digital Distortion
 * BBS address: digdist.bbsindex.com
 */

"use strict";

if (typeof(require) === "function")
{
	require("sbbsdefs.js", "K_NOCRLF");
	require("slyedit_misc.js", "CTRL_A");
}
else
{
	load("sbbsdefs.js");
	load("slyedit_misc.js");
}

// DCTEdit menu item return values
var DCTMENU_FILE_SAVE = 0;
var DCTMENU_FILE_ABORT = 1;
var DCTMENU_FILE_EDIT = 2;
var DCTMENU_EDIT_INSERT_TOGGLE = 3;
var DCTMENU_EDIT_FIND_TEXT = 4;
var DCTMENU_EDIT_SPELL_CHECKER = 5;
var DCTMENU_EDIT_INSERT_MEME = 6;
var DCTMENU_EDIT_SETTINGS = 7;
var DCTMENU_SYSOP_IMPORT_FILE = 8;
var DCTMENU_SYSOP_EXPORT_FILE = 9;
var DCTMENU_HELP_COMMAND_LIST = 10;
var DCTMENU_GRAPHIC_CHAR = 11;
var DCTMENU_HELP_PROGRAM_INFO = 12;
var DCTMENU_CROSS_POST = 13;
var DCTMENU_LIST_TXT_REPLACEMENTS = 14;

// Read the color configuration file
readColorConfig(gConfigSettings.DCTColors.ThemeFilename);

///////////////////////////////////////////////////////////////////////////////////
// Functions

// This function reads the color configuration for DCT style.
//
// Parameters:
//  pFilename: The name of the color configuration file
function readColorConfig(pFilename)
{
	var themeFile = new File(pFilename);
	if (themeFile.open("r"))
	{
		var colorSettingsObj = themeFile.iniGetObject();
		themeFile.close();

		// DCT-specific colors
		for (var prop in gConfigSettings.DCTColors)
		{
			if (colorSettingsObj.hasOwnProperty(prop))
			{
				// Using toString() to ensure the color attributes are strings (in case the value is just a number)
				var value = colorSettingsObj[prop].toString();
				// Remove any instances of specifying the control character
				value = value.replace(/\\[xX]01/g, "").replace(/\\[xX]1/g, "").replace(/\\1/g, "");
				// Add actual control characters in the color setting
				gConfigSettings.DCTColors[prop] = attrCodeStr(value);
			}
		}
		// General colors
		for (var prop in gConfigSettings.genColors)
		{
			if (colorSettingsObj.hasOwnProperty(prop))
			{
				var value = colorSettingsObj[prop].toString();
				// Remove any instances of specifying the control character
				value = value.replace(/\\[xX]01/g, "").replace(/\\[xX]1/g, "").replace(/\\1/g, "");
				// Add actual control characters in the color setting
				gConfigSettings.genColors[prop] = attrCodeStr(value);
			}
		}
	}
}

// Sets up any global screen-related variables needed for DCT style
function globalScreenVarsSetup_DCTStyle()
{
	gSubjPos.x = 12;
	gSubjPos.y = 4;
	gSubjScreenLen = console.screen_columns - 15;
}

// Re-draws the screen, in the style of DCTEdit.
//
// Parameters:
//  pEditLeft: The leftmost column of the edit area
//  pEditRight: The rightmost column of the edit area
//  pEditTop: The topmost row of the edit area
//  pEditBottom: The bottommost row of the edit area
//  pEditColor: The edit color
//  pInsertMode: The insert mode ("INS" or "OVR")
//  pUseQuotes: Whether or not message quoting is enabled
//  pCtrlQQuote: Boolean - Whether or not we're using Ctrl-Q to quote (if not, using Ctrl-Y)
//  pEditLinesIndex: The index of the message line at the top of the edit area
//  pDisplayEditLines: The function that displays the edit lines
function redrawScreen_DCTStyle(pEditLeft, pEditRight, pEditTop, pEditBottom, pEditColor,
                               pInsertMode, pUseQuotes, pCtrlQQuote, pEditLinesIndex, pDisplayEditLines)
{
	// Top header
	// Generate & display the top border line (Note: Generate this
	// border only once, for efficiency).
	if (typeof(redrawScreen_DCTStyle.topBorder) == "undefined")
	{
		var innerWidth = console.screen_columns - 2;
		redrawScreen_DCTStyle.topBorder = CP437_BOX_DRAWINGS_UPPER_LEFT_SINGLE;
		for (var i = 0; i < innerWidth; ++i)
			redrawScreen_DCTStyle.topBorder += CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE;
		redrawScreen_DCTStyle.topBorder += CP437_BOX_DRAWINGS_UPPER_RIGHT_SINGLE;
		redrawScreen_DCTStyle.topBorder = randomTwoColorString(redrawScreen_DCTStyle.topBorder,
		                                                       gConfigSettings.DCTColors.TopBorderColor1,
		                                                       gConfigSettings.DCTColors.TopBorderColor2);
	}
	// Print the border line on the screen
	console.clear();
	console.print(redrawScreen_DCTStyle.topBorder);

	// Next line
	// From name
	var lineNum = 2;
	var fromNameAndAreaLineNum = lineNum;
	console.gotoxy(1, lineNum);
	// Calculate the width of the from name field: 28 characters, based
	// on an 80-column screen width.
	console.print(randomTwoColorString(CP437_BOX_DRAWINGS_LIGHT_VERTICAL, gConfigSettings.DCTColors.TopBorderColor1,
	                                   gConfigSettings.DCTColors.TopBorderColor2) +
				  " " + gConfigSettings.DCTColors.TopLabelColor + "From " +
				  gConfigSettings.DCTColors.TopLabelColonColor + ": " +
				  gConfigSettings.DCTColors.TopInfoBracketColor + "[" +
				  gConfigSettings.DCTColors.TopFromFillColor + CP437_BULLET_OPERATOR + 
				  gConfigSettings.DCTColors.TopFromColor);
	console.print(gConfigSettings.DCTColors.TopFromFillColor);
	var fieldWidth = Math.floor(console.screen_columns * (28/80)) - 2;
	for (var i = 0; i < fieldWidth; ++i)
		console.print(CP437_BULLET_OPERATOR);
	console.print(gConfigSettings.DCTColors.TopInfoBracketColor + "]");

	// Message area
	fieldWidth = Math.floor(console.screen_columns * (27/80));
	var startX = console.screen_columns - fieldWidth - 9;
	console.gotoxy(startX, lineNum);
	console.print(gConfigSettings.DCTColors.TopLabelColor + "Area" +
	              gConfigSettings.DCTColors.TopLabelColonColor + ": " +
	              gConfigSettings.DCTColors.TopInfoBracketColor + "[" +
	              gConfigSettings.DCTColors.TopAreaFillColor + CP437_BULLET_OPERATOR +
	              gConfigSettings.DCTColors.TopAreaColor);
	console.print(gConfigSettings.DCTColors.TopAreaFillColor);
	--fieldWidth;
	for (var i = 0; i < fieldWidth; ++i)
		console.print(CP437_BULLET_OPERATOR);
	console.print(gConfigSettings.DCTColors.TopInfoBracketColor + "] " +
	              randomTwoColorString(CP437_BOX_DRAWINGS_LIGHT_VERTICAL,
	                                   gConfigSettings.DCTColors.TopBorderColor1,
	                                   gConfigSettings.DCTColors.TopBorderColor2));
	var msgAreaX = startX + 8; // For later

	// Next line: To, Time, time Left
	var toNameLineNum = ++lineNum;
	console.gotoxy(1, lineNum);
	// To name
	fieldWidth = Math.floor(console.screen_columns * (28/80)) - 2;
	console.print(randomTwoColorString(CP437_BOX_DRAWINGS_LIGHT_VERTICAL, gConfigSettings.DCTColors.TopBorderColor1,
	                                   gConfigSettings.DCTColors.TopBorderColor2) +
				  " " + gConfigSettings.DCTColors.TopLabelColor + "To   " +
				  gConfigSettings.DCTColors.TopLabelColonColor + ": " +
				  gConfigSettings.DCTColors.TopInfoBracketColor + "[" +
				  gConfigSettings.DCTColors.TopToFillColor + CP437_BULLET_OPERATOR +
				  gConfigSettings.DCTColors.TopToColor);
	console.print(gConfigSettings.DCTColors.TopToFillColor);
	for (var i = 0; i < fieldWidth; ++i)
		console.print(CP437_BULLET_OPERATOR);
	console.print(gConfigSettings.DCTColors.TopInfoBracketColor + "]");

	// Current time
	console.gotoxy(startX, lineNum);
	console.print(gConfigSettings.DCTColors.TopLabelColor + "Time" +
	              gConfigSettings.DCTColors.TopLabelColonColor + ": " +
	              gConfigSettings.DCTColors.TopInfoBracketColor + "[" +
	              gConfigSettings.DCTColors.TopTimeFillColor + CP437_BULLET_OPERATOR);
	displayTime_DCTStyle();
	console.print(gConfigSettings.DCTColors.TopTimeFillColor + CP437_BULLET_OPERATOR +
	              gConfigSettings.DCTColors.TopInfoBracketColor + "]");

	// Time left
	fieldWidth = Math.floor(console.screen_columns * (7/80));
	startX = console.screen_columns - fieldWidth - 9;
	console.gotoxy(startX, lineNum);
	var timeStr = Math.floor(bbs.time_left / 60).toString().substr(0, fieldWidth);
	console.print(gConfigSettings.DCTColors.TopLabelColor + "Left" +
	              gConfigSettings.DCTColors.TopLabelColonColor + ": " +
	              gConfigSettings.DCTColors.TopInfoBracketColor + "[" +
	              gConfigSettings.DCTColors.TopTimeLeftFillColor + CP437_BULLET_OPERATOR +
	              gConfigSettings.DCTColors.TopTimeLeftColor + timeStr +
	              gConfigSettings.DCTColors.TopTimeLeftFillColor);
	fieldWidth -= (timeStr.length+1);
	for (var i = 0; i < fieldWidth; ++i)
		console.print(CP437_BULLET_OPERATOR);
	console.print(gConfigSettings.DCTColors.TopInfoBracketColor + "] " +
	              randomTwoColorString(CP437_BOX_DRAWINGS_LIGHT_VERTICAL,
	              gConfigSettings.DCTColors.TopBorderColor1,
	              gConfigSettings.DCTColors.TopBorderColor2));

	// Line 3: Subject
	var subjLineNum = ++lineNum;
	console.gotoxy(1, lineNum);
	fieldWidth = +(console.screen_columns - 15);
	console.print(randomTwoColorString(CP437_BOX_DRAWINGS_LOWER_LEFT_SINGLE, gConfigSettings.DCTColors.TopBorderColor1,
	                                   gConfigSettings.DCTColors.TopBorderColor1) +
	                                   " " + gConfigSettings.DCTColors.TopLabelColor + "Subj " +
	                                   gConfigSettings.DCTColors.TopLabelColonColor + ": " +
	                                   gConfigSettings.DCTColors.TopInfoBracketColor + "[" +
	                                   gConfigSettings.DCTColors.TopSubjFillColor + CP437_BULLET_OPERATOR +
	                                   gConfigSettings.DCTColors.TopSubjColor);
	console.print(gConfigSettings.DCTColors.TopSubjFillColor);
	for (var i = 0; i < fieldWidth; ++i)
		console.print(CP437_BULLET_OPERATOR);
	console.print(CP437_BULLET_OPERATOR);
	console.print(gConfigSettings.DCTColors.TopInfoBracketColor + "] " +
	              randomTwoColorString(CP437_BOX_DRAWINGS_LOWER_RIGHT_SINGLE,
	              gConfigSettings.DCTColors.TopBorderColor1,
	              gConfigSettings.DCTColors.TopBorderColor2));
	
	// Line 4: Top border for message area
	++lineNum;
	displayTextAreaTopBorder_DCTStyle(lineNum, pEditLeft, pEditRight);

	// Display the bottom message area border and help line
	DisplayTextAreaBottomBorder_DCTStyle(pEditBottom+1, null, pEditLeft, pEditRight, pInsertMode);
	DisplayBottomHelpLine_DCTStyle(console.screen_rows, pUseQuotes, pCtrlQQuote);

	// Display the message header information fields (From, To, Subj, Area). We use
	// gotoxy() and print these last in case they're UTF-8, to avoid header graphic
	// chars moving around.
	// TODO: Doing a substr() on UTF-8 strings seems to result in them being shorter
	// than intended if there are multi-byte characters.
	// Message area
	var msgAreaName = gMsgArea.substr(0, Math.floor(console.screen_columns * (27/80)));
	console.gotoxy(msgAreaX, fromNameAndAreaLineNum);
	console.print(msgAreaName, P_AUTO_UTF8);
	// From name
	var infoX = 12;
	var fromName = gFromName.substr(0, Math.floor(console.screen_columns * (28/80)));
	console.gotoxy(infoX, fromNameAndAreaLineNum);
	console.print(fromName, P_AUTO_UTF8);
	// To name
	var toName = gToName.substr(0, Math.floor(console.screen_columns * (28/80)));
	console.gotoxy(infoX, toNameLineNum);
	console.print(toName, P_AUTO_UTF8);
	// Subject
	console.gotoxy(infoX, subjLineNum);
	var subj = gMsgSubj.substr(0, +(console.screen_columns - 15));
	console.print(subj, P_AUTO_UTF8);
	
	// Go to the start of the edit area
	console.gotoxy(pEditLeft, pEditTop);

	// Write the message text that has been entered thus far.
	pDisplayEditLines(pEditTop, pEditLinesIndex);
	console.print(pEditColor);
}

function refreshSubjectOnScreen_DCTStyle(pX, pY, pLength, pText)
{
	console.print("\x01n" + gConfigSettings.DCTColors.TopSubjColor);
	console.gotoxy(pX, pY);
	//printf("%-" + pLength + "s", pText.substr(0, pLength));
	// Ensure the text is no longer than the field width
	var subj = pText.substr(0, pLength);
	console.print(subj);
	console.print("\x01n" + gConfigSettings.DCTColors.TopSubjFillColor);
	var fieldWidth = pLength - subj.length;
	for (var i = 0; i < fieldWidth; ++i)
		console.print(CP437_BULLET_OPERATOR);
}

// Displays the top border of the message area, in the style of DCTEdit.
//
// Parameters:
//  pLineNum: The line number on the screen at which to draw the border
//  pEditLeft: The leftmost edit area column on the screen
//  pEditRight: The rightmost edit area column on the screen
function displayTextAreaTopBorder_DCTStyle(pLineNum, pEditLeft, pEditRight)
{
	// The border will use random bright/normal colors.  The colors
	// should stay the same each time we draw it, so a "static"
	// variable is used for the border text.  If that variable has
	// not been defined yet, then build it.
	if (typeof(displayTextAreaTopBorder_DCTStyle.border) == "undefined")
	{
		var numHorizontalChars = pEditRight - pEditLeft - 1;

		displayTextAreaTopBorder_DCTStyle.border = CP437_BOX_DRAWINGS_UPPER_LEFT_SINGLE;
		for (var i = 0; i < numHorizontalChars; ++i)
			displayTextAreaTopBorder_DCTStyle.border += CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE;
		displayTextAreaTopBorder_DCTStyle.border += CP437_BOX_DRAWINGS_UPPER_RIGHT_SINGLE;
		displayTextAreaTopBorder_DCTStyle.border =
		randomTwoColorString(displayTextAreaTopBorder_DCTStyle.border,
		gConfigSettings.DCTColors.EditAreaBorderColor1,
		gConfigSettings.DCTColors.EditAreaBorderColor2);
	}

	// Draw the line on the screen
	//console.gotoxy((console.screen_columns >= 82 ? pEditLeft-1 : pEditLeft), pLineNum);
	console.gotoxy(pEditLeft, pLineNum);
	console.print(displayTextAreaTopBorder_DCTStyle.border);
}

// Displays the bottom border of the message area, in the style of DCTEdit.
//
// Parameters:
//  pLineNum: The line number on the screen at which to draw the border
//  pUseQuotes: This is not used; this is only here so that the signatures of
//              the IceEdit and DCTEdit versions match.
//  pEditLeft: The leftmost edit area column on the screen
//  pEditRight: The rightmost edit area column on the screen
//  pInsertMode: The insert mode ("INS" or "OVR")
//  pCanChgMsgColor: Whether or not changing the text color is allowed
function DisplayTextAreaBottomBorder_DCTStyle(pLineNum, pUseQuotes, pEditLeft, pEditRight,
                                               pInsertMode, pCanChgMsgColor)
{
	// The border will use random bright/normal colors.  The colors
	// should stay the same each time we draw it, so a "static"
	// variable is used for the border text.  If that variable has
	// not been defined yet, then build it.
	if (typeof(DisplayTextAreaBottomBorder_DCTStyle.border) == "undefined")
	{
		var innerWidth = pEditRight - pEditLeft - 1;

		DisplayTextAreaBottomBorder_DCTStyle.border = CP437_BOX_DRAWINGS_LOWER_LEFT_SINGLE;

		// This loop uses innerWidth-6 to make way for the insert mode
		// text.
		for (var i = 0; i < innerWidth-6; ++i)
			DisplayTextAreaBottomBorder_DCTStyle.border += CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE;
		DisplayTextAreaBottomBorder_DCTStyle.border =
		randomTwoColorString(DisplayTextAreaBottomBorder_DCTStyle.border,
		gConfigSettings.DCTColors.EditAreaBorderColor1,
		gConfigSettings.DCTColors.EditAreaBorderColor2);
		// Insert mode
		DisplayTextAreaBottomBorder_DCTStyle.border += gConfigSettings.DCTColors.EditModeBrackets
		                                            + "[" + gConfigSettings.DCTColors.EditMode
		                                            + pInsertMode
		                                            + gConfigSettings.DCTColors.EditModeBrackets
		                                            + "]";
		// The last 2 border characters
		DisplayTextAreaBottomBorder_DCTStyle.border +=
		randomTwoColorString(CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE + CP437_BOX_DRAWINGS_LOWER_RIGHT_SINGLE,
		gConfigSettings.DCTColors.EditAreaBorderColor1,
		gConfigSettings.DCTColors.EditAreaBorderColor2);
	}

	// Draw the border line on the screen.
	//console.gotoxy((console.screen_columns >= 82 ? pEditLeft-1 : pEditLeft), pLineNum);
	console.gotoxy(pEditLeft, pLineNum);
	console.print(DisplayTextAreaBottomBorder_DCTStyle.border);
}

// Displays the help line at the bottom of the screen, in the style
// of DCTEdit.
//
// Parameters:
//  pLineNum: The line number on the screen at which to draw the help line
//  pUsingQuotes: Boolean - Whether or not message quoting is enabled.
//  pCtrlQQuote: Boolean - Whether or not we're using Ctrl-Q to quote (if not, using Ctrl-Y)
function DisplayBottomHelpLine_DCTStyle(pLineNum, pUsingQuotes, pCtrlQQuote)
{
	var helpText = gConfigSettings.DCTColors.BottomHelpBrackets
	             + "[" + gConfigSettings.DCTColors.BottomHelpKeys + "CTRL"
	             + gConfigSettings.DCTColors.BottomHelpFill + CP437_BULLET_OPERATOR
	             + gConfigSettings.DCTColors.BottomHelpKeys + "Z"
	             + gConfigSettings.DCTColors.BottomHelpBrackets + "]\x01n "
	             + gConfigSettings.DCTColors.BottomHelpKeyDesc + "Save\x01n      "
	             + gConfigSettings.DCTColors.BottomHelpBrackets + "["
	             + gConfigSettings.DCTColors.BottomHelpKeys + "CTRL"
	             + gConfigSettings.DCTColors.BottomHelpFill + CP437_BULLET_OPERATOR
	             + gConfigSettings.DCTColors.BottomHelpKeys + "A"
	             + gConfigSettings.DCTColors.BottomHelpBrackets + "]\x01n "
	             + gConfigSettings.DCTColors.BottomHelpKeyDesc + "Abort";
	// If we can allow message quoting, then add a text to show Ctrl-Q (or Ctrl-Y) for
	// quoting.
	if (pUsingQuotes)
	{
		const quoteHotkeyChar = pCtrlQQuote ? "Q" : "Y";
		helpText += "\x01n      "
		         + gConfigSettings.DCTColors.BottomHelpBrackets + "["
		         + gConfigSettings.DCTColors.BottomHelpKeys + "CTRL"
		         + gConfigSettings.DCTColors.BottomHelpFill + CP437_BULLET_OPERATOR
		         + gConfigSettings.DCTColors.BottomHelpKeys + quoteHotkeyChar
		         + gConfigSettings.DCTColors.BottomHelpBrackets + "]\x01n "
		         + gConfigSettings.DCTColors.BottomHelpKeyDesc + "Quote";
	}
	helpText += "\x01n      "
	         + gConfigSettings.DCTColors.BottomHelpBrackets + "["
	         + gConfigSettings.DCTColors.BottomHelpKeys + "ESC"
	         + gConfigSettings.DCTColors.BottomHelpBrackets + "]\x01n "
	         + gConfigSettings.DCTColors.BottomHelpKeyDesc + "Menu";
	// Center the text by padding it in the front with spaces.  This is done instead
	// of using console.center() because console.center() will output a newline,
	// which would not be good on the last line of the screen.
	var numSpaces = Math.floor(console.screen_columns/2) - Math.floor(console.strlen(helpText)/2);
	for (var i = 0; i < numSpaces; ++i)
		helpText = " " + helpText;

	// Display the help line on the screen
	var lineNum = console.screen_rows;
	if ((typeof(pLineNum) != "undefined") && (pLineNum != null))
		lineNum = pLineNum;
	console.gotoxy(1, lineNum);
	console.print(helpText);
	console.print("\x01n");
	console.cleartoeol();
}

// Updates the insert mode displayd on the screen, for DCT Edit style.
//
// Parameters:
//  pEditRight: The rightmost column on the screen for the edit area
//  pEditBottom: The bottommost row on the screen for the edit area
//  pInsertMode: The insert mode ("INS" or "OVR")
function updateInsertModeOnScreen_DCTStyle(pEditRight, pEditBottom, pInsertMode)
{
   // If the number of columns on the screen is more than 80, the horizontal
   // position will be 1 more to the right due to the added vertical lines
   // around the edit area.
   if (console.screen_columns > 80)
      console.gotoxy(pEditRight-5, pEditBottom+1);
   else
      console.gotoxy(pEditRight-6, pEditBottom+1);
	console.print(gConfigSettings.DCTColors.EditModeBrackets + "[" +
	              gConfigSettings.DCTColors.EditMode + pInsertMode +
	              gConfigSettings.DCTColors.EditModeBrackets + "]");
}

// Draws the top border of the quote window, DCT Edit style.
//
// Parameters:
//  pQuoteWinHeight: The height of the quote window
//  pEditLeft: The leftmost column on the screen for the edit window
//  pEditRight: The rightmost column on the screen for the edit window
function DrawQuoteWindowTopBorder_DCTStyle(pQuoteWinHeight, pEditLeft, pEditRight)
{
	// Generate the top border vairable only once.
	if (typeof(DrawQuoteWindowTopBorder_DCTStyle.border) == "undefined")
	{
		DrawQuoteWindowTopBorder_DCTStyle.border = gConfigSettings.DCTColors.QuoteWinBorderColor
		                                         + CP437_BOX_DRAWINGS_UPPER_LEFT_SINGLE + CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE + " "
		                                         + gConfigSettings.DCTColors.QuoteWinBorderTextColor
		                                         + "Quote Window " + gConfigSettings.DCTColors.QuoteWinBorderColor;
		var curLength = console.strlen(DrawQuoteWindowTopBorder_DCTStyle.border);
		var borderWidth = pEditRight - pEditLeft;
		for (var i = curLength; i < borderWidth; ++i)
			DrawQuoteWindowTopBorder_DCTStyle.border += CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE;
		DrawQuoteWindowTopBorder_DCTStyle.border += CP437_BOX_DRAWINGS_UPPER_RIGHT_SINGLE;
	}

	// Draw the top border line
	var screenLine = console.screen_rows - pQuoteWinHeight + 1;
	console.gotoxy(pEditLeft, screenLine);
	console.print(DrawQuoteWindowTopBorder_DCTStyle.border);
}

// Draws the bottom border of the quote window, DCT Edit style.  Note:
// The cursor should be placed at the start of the line (leftmost screen
// column on the screen line where the border should be drawn).
//
// Parameters:
//  pEditLeft: The leftmost column of the edit area
//  pEditRight: The rightmost column of the edit area
function DrawQuoteWindowBottomBorder_DCTStyle(pEditLeft, pEditRight)
{
	// Generate the bottom border vairable only once.
	if (typeof(DrawQuoteWindowBottomBorder_DCTStyle.border) == "undefined")
	{
		// Create a string containing the quote help text.
		const quoteHotkeyChar = gUserSettings.ctrlQQuote ? "Q" : "Y";
		var quoteHelpText = gConfigSettings.DCTColors.QuoteWinBorderTextColor
		                 + "[Enter] Accept" + gConfigSettings.DCTColors.QuoteWinBorderColor
		                 + CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE + CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE + gConfigSettings.DCTColors.QuoteWinBorderTextColor
		                 + "[^" + quoteHotkeyChar + "/ESC] End" + gConfigSettings.DCTColors.QuoteWinBorderColor
		                 + CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE + CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE + gConfigSettings.DCTColors.QuoteWinBorderTextColor
		                 + "[" + UP_ARROW + "/" + DOWN_ARROW + "/PgUp/PgDn/Home/End] Scroll"
		                 + gConfigSettings.DCTColors.QuoteWinBorderColor + CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE
		                 + CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE + gConfigSettings.DCTColors.QuoteWinBorderTextColor;
		                 /*
		                 + "[" + UP_ARROW + "/" + DOWN_ARROW + "/PgUp/PgDn] Scroll"
		                 + gConfigSettings.DCTColors.QuoteWinBorderColor + CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE
		                 + CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE + gConfigSettings.DCTColors.QuoteWinBorderTextColor
		                 + "[F/L] First/last page";
		                 */
		var helpTextLen = console.strlen(quoteHelpText);

		// Figure out the starting horizontal position on the screen so that
		// the quote help text line can be centered.
		var helpTextStartX = Math.floor((console.screen_columns/2) - (helpTextLen/2));

		// Start creating DrawQuoteWindowBottomBorder_DCTStyle.border with the
		// bottom border lines, up until helpTextStartX.
		DrawQuoteWindowBottomBorder_DCTStyle.border = gConfigSettings.DCTColors.QuoteWinBorderColor + CP437_BOX_DRAWINGS_LOWER_LEFT_SINGLE;
		for (var XPos = pEditLeft+2; XPos < helpTextStartX; ++XPos)
			DrawQuoteWindowBottomBorder_DCTStyle.border += CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE;
		// Add the help text, then display the rest of the bottom border characters.
		DrawQuoteWindowBottomBorder_DCTStyle.border += quoteHelpText + gConfigSettings.DCTColors.QuoteWinBorderColor;
		// Previously, the rightmost column in this loop was pEditRight-2, but now it looks like that was resulting
		// in this line being 2 characters shorter than it should be.
		for (var XPos = helpTextStartX + helpTextLen; XPos <= pEditRight; ++XPos)
			DrawQuoteWindowBottomBorder_DCTStyle.border += CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE;
		DrawQuoteWindowBottomBorder_DCTStyle.border += CP437_BOX_DRAWINGS_LOWER_RIGHT_SINGLE;
	}

	// Print the border text on the screen
	console.print(DrawQuoteWindowBottomBorder_DCTStyle.border);
}

// Prompts the user for a yes/no question, DCTEdit-style.
//
// Parameters:
//  pQuestion: The question to ask, without the trailing "?"
//  pBoxTitle: The text to appear in the top border of the question box
//  pDefaultYes: Whether to default to "yes" (true/false).  If false, this
//               will default to "no".
//  pParamObj: An object containing the following members:
//   displayMessageRectangle: The function used for re-drawing the portion of the
//                            message on the screen when the yes/no box is no longer
//                            needed
//   editTop: The topmost row of the edit area
//   editBottom: The bottom row of the edit area
//   editWidth: The edit area width
//   editHeight: The edit area height
//   editLinesIndex: The current index into the edit lines array
// pAlwaysEraseBox: Boolean: erase the box regardless of a Yes or No answer.
function promptYesNo_DCTStyle(pQuestion, pBoxTitle, pDefaultYes, pParamObj, pAlwaysEraseBox)
{
	var userResponse = pDefaultYes;

	// Get the current cursor position, so that we can place the cursor
	// back there later.
	const originalCurpos = console.getxy();

	// Set up the abort confirmation box dimensions, and calculate
	// where on the screen to display it.
	//const boxWidth = 27;
	const boxWidth = pQuestion.length + 14;
	const boxHeight = 6;
	const boxX = Math.floor(console.screen_columns/2) - Math.floor(boxWidth/2);
	const boxY = pParamObj.editTop + Math.floor(pParamObj.editHeight/2) - Math.floor(boxHeight/2);
	const innerBoxWidth = boxWidth - 2;

	// Display the question box
	// Upper-left corner, 1 horizontal line, and "Abort" text
	console.gotoxy(boxX, boxY);
	console.print(gConfigSettings.DCTColors.TextBoxBorder + CP437_BOX_DRAWINGS_UPPER_LEFT_SINGLE +
	              CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE + " " + gConfigSettings.DCTColors.TextBoxBorderText +
	              pBoxTitle + gConfigSettings.DCTColors.TextBoxBorder + " ");
	// Remaining top box border
	for (var i = pBoxTitle.length + 5; i < boxWidth; ++i)
		console.print(CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE);
	console.print(CP437_BOX_DRAWINGS_UPPER_RIGHT_SINGLE);
	// Inner box: Blank
	var endScreenLine = boxY + boxHeight - 2;
	for (var screenLine = boxY+1; screenLine < endScreenLine; ++screenLine)
	{
		console.gotoxy(boxX, screenLine);
		console.print(CP437_BOX_DRAWINGS_LIGHT_VERTICAL);
		printf("%" + innerBoxWidth + "s", "");
		console.print(CP437_BOX_DRAWINGS_LIGHT_VERTICAL);
	}
	// Bottom box border
	console.gotoxy(boxX, screenLine);
	console.print(CP437_BOX_DRAWINGS_LOWER_LEFT_SINGLE);
	for (var i = 0; i < innerBoxWidth; ++i)
		console.print(CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE);
	console.print(CP437_BOX_DRAWINGS_LOWER_RIGHT_SINGLE);

	// Prompt the user whether or not to abort: Move the cursor to
	// the proper location on the screen, output the propmt text,
	// and get user input.
	console.gotoxy(boxX+3, boxY+2);
	console.print(gConfigSettings.DCTColors.TextBoxInnerText + pQuestion + "?  " +
	gConfigSettings.DCTColors.YesNoBoxBrackets + "[" +
	gConfigSettings.DCTColors.YesNoBoxYesNoText);
	// Default to yes/no, depending on the value of pDefaultYes.
	if (pDefaultYes)
	{
		console.print("Yes");
		userResponse = true;
	}
	else
	{
		console.print("No ");
		userResponse = false;
	}
	console.print(gConfigSettings.DCTColors.YesNoBoxBrackets + "]");

	// Input loop
	var userInput = "";
	var continueOn = true;
	while (continueOn)
	{
		// Move the cursor where it needs to be to write the "Yes"
		// or "No"
		console.gotoxy(boxX+20, boxY+2);
		// Get a key and take appropriate action.
		userInput = getUserKey(K_UPPER|K_NOECHO|K_NOCRLF|K_NOSPIN);
		if (userInput == KEY_ENTER)
			continueOn = false;
		else if (userInput == "Y")
		{
			userResponse = true;
			continueOn = false;
		}
		else if ((userInput == "N") || (userInput == KEY_ESC))
		{
			userResponse = false;
			continueOn = false;
		}
		// Arrow keys: Toggle back and forth
		else if ((userInput == KEY_UP) || (userInput == KEY_DOWN) ||
		         (userInput == KEY_LEFT) || (userInput == KEY_RIGHT))
		{
			// Toggle the userResponse variable
			userResponse = !userResponse;
			// Write "Yes" or "No", depending on the userResponse variable
			if (userResponse)
			{
				console.print(gConfigSettings.DCTColors.YesNoBoxYesNoText + "Yes" +
				gConfigSettings.DCTColors.YesNoBoxBrackets + "]");
			}
			else
			{
				console.print(gConfigSettings.DCTColors.YesNoBoxYesNoText + "No " +
				gConfigSettings.DCTColors.YesNoBoxBrackets + "]");
			}
		}
	}

	// If the user chose no, then erase the confirmation box and put the
	// cursor back where it originally was.
	if (!userResponse || pAlwaysEraseBox)
	{
		// Calculate the difference in the edit lines index we'll need to use
		// to erase the confirmation box.  This will be the difference between
		// the cursor position between boxY and the current cursor row.
		const editLinesIndexDiff = boxY - originalCurpos.y;
		pParamObj.displayMessageRectangle(boxX, boxY, boxWidth, boxHeight,
		                                  pParamObj.editLinesIndex + editLinesIndexDiff,
		                                  true);
		// Put the cursor back where it was
		console.gotoxy(originalCurpos);
	}

	return userResponse;
}

// Displays the time on the screen.
//
// Parameters:
//  pTimeStr: The time to display (optional).  If this is omitted,
//            then this funtion will get the current time.
function displayTime_DCTStyle(pTimeStr)
{
	// Calculate the horizontal location for the time string, using basically
	// the formula used for calculating the horizontal location of the message
	// area in redrawScreen_DCTStyle(), which the time lines up with.
	var fieldWidth = Math.floor(console.screen_columns * (27/80));
	//var curposX = console.screen_columns - fieldWidth - 9 + 8;
	var curposX = console.screen_columns - fieldWidth - 1;
	console.gotoxy(curposX, 3);

	if (pTimeStr == null)
		console.print("\x01n" + gConfigSettings.DCTColors.TopTimeColor + getCurrentTimeStr());
	else
		console.print("\x01n" + gConfigSettings.DCTColors.TopTimeColor + pTimeStr);
}

// Displays the number of minutes remaining on the screen.
function displayTimeRemaining_DCTStyle()
{
	var fieldWidth = Math.floor(console.screen_columns * (7/80));
	var startX = console.screen_columns - fieldWidth - 1;
	console.gotoxy(startX, 3);
	var timeStr = Math.floor(bbs.time_left / 60).toString().substr(0, fieldWidth);
	console.print(gConfigSettings.DCTColors.TopTimeLeftColor + timeStr +
	              gConfigSettings.DCTColors.TopTimeLeftFillColor);
	fieldWidth -= (timeStr.length+1);
	for (var i = 0; i < fieldWidth; ++i)
		console.print(CP437_BULLET_OPERATOR);
}

// Displays & handles the input loop for the DCT style ESC menu.
//
// Parameters:
//  pEditLeft: The leftmost column of the edit area
//  pEditRight: The rightmost column of the edit area
//  pEditTop: The topmost row of the edit area
//  pDisplayMessageRectangle: The function for drawing part of the
//                            edit text on the screen.  This is used
//                            for refreshing the screen after a menu
//                            box disappears.
//  pEditLinesIndex: The current index into the edit lines array.  This
//                   is is required to pass to the pDisplayMessageRectangle
//                   function.
//  pEditLineDiff: The difference between the current edit line and the top of
//                 the edit area.
//  pCanCrossPost: Whether or not cross-posting is allowed.
//
// Return value: The action code, based on the user's selection
function doDCTESCMenu(pEditLeft, pEditRight, pEditTop, pDisplayMessageRectangle,
                      pEditLinesIndex, pEditLineDiff, pCanCrossPost)
{
	// This function displays the top menu options, with a given one highlighted.
	//
	// Parameters:
	//  pItemPositions: An object containing the menu item positions.
	//  pHighlightedItemNum: The index (0-based) of the menu item to be highlighted.
	//  pMenus: An array containing the menus
	//
	// Return value: The user's last keypress during the menu's input loop.
	function displayTopMenuItems(pItemPositions, pHighlightedItemNum, pMenus)
	{
		// File
		console.gotoxy(pItemPositions.fileX, pItemPositions.mainMenuY);
		if (pHighlightedItemNum == 0)
		{
			console.print(gConfigSettings.DCTColors.SelectedMenuLabelBorders + CP437_RIGHT_HALF_BLOCK +
			              gConfigSettings.DCTColors.SelectedMenuLabelText + "File" +
			              gConfigSettings.DCTColors.SelectedMenuLabelBorders + CP437_LEFT_HALF_BLOCK);
		}
		else
			console.print("\x01n " + gConfigSettings.DCTColors.UnselectedMenuLabelText + "File\x01n ");
		// Edit
		console.gotoxy(pItemPositions.editX, pItemPositions.mainMenuY);
		if (pHighlightedItemNum == 1)
		{
			console.print(gConfigSettings.DCTColors.SelectedMenuLabelBorders + CP437_RIGHT_HALF_BLOCK +
			              gConfigSettings.DCTColors.SelectedMenuLabelText + "Edit" +
			              gConfigSettings.DCTColors.SelectedMenuLabelBorders + CP437_LEFT_HALF_BLOCK);
		}
		else
			console.print("\x01n " + gConfigSettings.DCTColors.UnselectedMenuLabelText + "Edit\x01n ");
		// SysOp
		if (user.is_sysop)
		{
			console.gotoxy(pItemPositions.sysopX, pItemPositions.mainMenuY);
			if (pHighlightedItemNum == 2)
			{
				console.print(gConfigSettings.DCTColors.SelectedMenuLabelBorders + CP437_RIGHT_HALF_BLOCK +
				              gConfigSettings.DCTColors.SelectedMenuLabelText + "SysOp" +
				              gConfigSettings.DCTColors.SelectedMenuLabelBorders + CP437_LEFT_HALF_BLOCK);
			}
			else
				console.print("\x01n " + gConfigSettings.DCTColors.UnselectedMenuLabelText + "SysOp\x01n ");
		}
		// Help
		console.gotoxy(pItemPositions.helpX, pItemPositions.mainMenuY);
		if (pHighlightedItemNum == 3)
		{
			console.print(gConfigSettings.DCTColors.SelectedMenuLabelBorders + CP437_RIGHT_HALF_BLOCK +
			              gConfigSettings.DCTColors.SelectedMenuLabelText + "Help" +
			              gConfigSettings.DCTColors.SelectedMenuLabelBorders + CP437_LEFT_HALF_BLOCK);
		}
		else
			console.print("\x01n " + gConfigSettings.DCTColors.UnselectedMenuLabelText + "Help\x01n ");

		// Display the menu (and capture the return object so that we can
		// also return it here).
		var returnObj = pMenus[pHighlightedItemNum].doInputLoop();

		// Refresh the part of the edit text on the screen where the menu was.
		pDisplayMessageRectangle(pMenus[pHighlightedItemNum].topLeftX,
		                         pMenus[pHighlightedItemNum].topLeftY,
		                         pMenus[pHighlightedItemNum].width,
		                         pMenus[pHighlightedItemNum].height,
		                         pEditLinesIndex-pEditLineDiff, true);

		return returnObj;
	}

	// Set up an object containing the menu item positions.
	// Only create this object once.
	if (typeof(doDCTESCMenu.mainMenuItemPositions) == "undefined")
	{
		doDCTESCMenu.mainMenuItemPositions = {
			// Vertical position on the screen for the main menu items
			mainMenuY: pEditTop - 1,
			// Horizontal position of the "File" text
			fileX: gEditLeft + 5,
			// Horizontal position of the "Edit" text
			editX: 0,
			// Horizontal position of the "SysOp" text
			sysopX: 0,
			helpX: 0
		};
		doDCTESCMenu.mainMenuItemPositions.editX = doDCTESCMenu.mainMenuItemPositions.fileX + 11;
		doDCTESCMenu.mainMenuItemPositions.sysopX = doDCTESCMenu.mainMenuItemPositions.editX + 11;
		// Horizontal position of of the "Help" text
		if (user.is_sysop)
			doDCTESCMenu.mainMenuItemPositions.helpX = doDCTESCMenu.mainMenuItemPositions.sysopX + 12;
		else
			doDCTESCMenu.mainMenuItemPositions.helpX = doDCTESCMenu.mainMenuItemPositions.sysopX;
	}

	// Variables for the menu numbers
	const fileMenuNum = 0;
	const editMenuNum = 1;
	const sysopMenuNum = 2;
	const helpMenuNum = 3;

	// Set up the menu objects.  Only create these objects once.
	if (typeof(doDCTESCMenu.allMenus) == "undefined")
	{
		doDCTESCMenu.allMenus = [];
		// File menu
		doDCTESCMenu.allMenus[fileMenuNum] = new DCTMenu(doDCTESCMenu.mainMenuItemPositions.fileX, doDCTESCMenu.mainMenuItemPositions.mainMenuY+1);
		doDCTESCMenu.allMenus[fileMenuNum].addItem("&Save    Ctrl-Z", DCTMENU_FILE_SAVE);
		doDCTESCMenu.allMenus[fileMenuNum].addItem("&Abort   Ctrl-A", DCTMENU_FILE_ABORT);
		if (pCanCrossPost)
			doDCTESCMenu.allMenus[fileMenuNum].addItem( "X-Post  Ctrl-&C", DCTMENU_CROSS_POST);
		doDCTESCMenu.allMenus[fileMenuNum].addItem("&Edit       ESC", DCTMENU_FILE_EDIT);
		doDCTESCMenu.allMenus[fileMenuNum].addExitLoopKey(CTRL_Z, DCTMENU_FILE_SAVE);
		doDCTESCMenu.allMenus[fileMenuNum].addExitLoopKey(CTRL_A, DCTMENU_FILE_ABORT);
		if (pCanCrossPost)
			doDCTESCMenu.allMenus[fileMenuNum].addExitLoopKey(CTRL_C, DCTMENU_CROSS_POST);
		doDCTESCMenu.allMenus[fileMenuNum].addExitLoopKey(KEY_ESC, DCTMENU_FILE_EDIT);

		// Edit menu
		doDCTESCMenu.allMenus[editMenuNum] = new DCTMenu(doDCTESCMenu.mainMenuItemPositions.editX, doDCTESCMenu.mainMenuItemPositions.mainMenuY+1);
		doDCTESCMenu.allMenus[editMenuNum].addItem("&Insert Mode    Ctrl-I", DCTMENU_EDIT_INSERT_TOGGLE);
		doDCTESCMenu.allMenus[editMenuNum].addItem("&Graphic char   Ctrl-G", DCTMENU_GRAPHIC_CHAR);
		doDCTESCMenu.allMenus[editMenuNum].addItem("&Word/txt find  Ctrl-W", DCTMENU_EDIT_FIND_TEXT);
		doDCTESCMenu.allMenus[editMenuNum].addItem("Spe&ll Checker  Ctrl-R", DCTMENU_EDIT_SPELL_CHECKER);
		doDCTESCMenu.allMenus[editMenuNum].addItem("Insert &Meme", DCTMENU_EDIT_INSERT_MEME);
		doDCTESCMenu.allMenus[editMenuNum].addItem("Setti&ngs       Ctrl-U", DCTMENU_EDIT_SETTINGS);
		doDCTESCMenu.allMenus[editMenuNum].addExitLoopKey(CTRL_I, DCTMENU_EDIT_INSERT_TOGGLE);
		doDCTESCMenu.allMenus[editMenuNum].addExitLoopKey(CTRL_N, DCTMENU_EDIT_FIND_TEXT);
		doDCTESCMenu.allMenus[editMenuNum].addExitLoopKey(CTRL_U, DCTMENU_EDIT_SETTINGS);

		// SysOp menu
		doDCTESCMenu.allMenus[sysopMenuNum] = new DCTMenu(doDCTESCMenu.mainMenuItemPositions.sysopX, doDCTESCMenu.mainMenuItemPositions.mainMenuY+1);
		doDCTESCMenu.allMenus[sysopMenuNum].addItem("&Import file      Ctrl-O", DCTMENU_SYSOP_IMPORT_FILE);
		doDCTESCMenu.allMenus[sysopMenuNum].addItem("E&xport to file   Ctrl-X", DCTMENU_SYSOP_EXPORT_FILE);
		doDCTESCMenu.allMenus[sysopMenuNum].addExitLoopKey(CTRL_O, DCTMENU_SYSOP_IMPORT_FILE);
		doDCTESCMenu.allMenus[sysopMenuNum].addExitLoopKey(CTRL_X, DCTMENU_SYSOP_EXPORT_FILE);

		// Help menu
		doDCTESCMenu.allMenus[helpMenuNum] = new DCTMenu(doDCTESCMenu.mainMenuItemPositions.helpX, doDCTESCMenu.mainMenuItemPositions.mainMenuY+1);
		doDCTESCMenu.allMenus[helpMenuNum].addItem("C&ommand List   Ctrl-L", DCTMENU_HELP_COMMAND_LIST);
		doDCTESCMenu.allMenus[helpMenuNum].addItem("&Program Info", DCTMENU_HELP_PROGRAM_INFO);
		if (gConfigSettings.enableTextReplacements)
		{
			doDCTESCMenu.allMenus[helpMenuNum].addItem("&Text replcmts  Ctrl-T", DCTMENU_LIST_TXT_REPLACEMENTS);
			doDCTESCMenu.allMenus[helpMenuNum].addExitLoopKey(CTRL_T, DCTMENU_LIST_TXT_REPLACEMENTS);
		}
		doDCTESCMenu.allMenus[helpMenuNum].addExitLoopKey(CTRL_P, DCTMENU_HELP_COMMAND_LIST);
		doDCTESCMenu.allMenus[helpMenuNum].addExitLoopKey(CTRL_G, DCTMENU_GRAPHIC_CHAR);
		doDCTESCMenu.allMenus[helpMenuNum].addExitLoopKey(CTRL_R, DCTMENU_HELP_PROGRAM_INFO);

		// For each menu, add KEY_LEFT, KEY_RIGHT, and ESC as loop-exit keys;
		// also, set the menu colors.
		for (var i = 0; i < doDCTESCMenu.allMenus.length; ++i)
		{
			doDCTESCMenu.allMenus[i].addExitLoopKey(KEY_LEFT);
			doDCTESCMenu.allMenus[i].addExitLoopKey(KEY_RIGHT);
			doDCTESCMenu.allMenus[i].addExitLoopKey(KEY_ESC);

			doDCTESCMenu.allMenus[i].colors.border = gConfigSettings.DCTColors.MenuBorders;
			doDCTESCMenu.allMenus[i].colors.selected = gConfigSettings.DCTColors.MenuSelectedItems;
			doDCTESCMenu.allMenus[i].colors.unselected = gConfigSettings.DCTColors.MenuUnselectedItems;
			doDCTESCMenu.allMenus[i].colors.hotkey = gConfigSettings.DCTColors.MenuHotkeys;
		}
	}

	// The chosen action will be returned from this function
	var chosenAction = ESC_MENU_EDIT_MESSAGE;

	// Boolean variables to keep track of which menus were displayed
	// (for refresh purposes later)
	var fileMenuDisplayed = false;
	var editMenuDisplayed = false;
	var sysopMenuDisplayed = false;
	var helpMenuDisplayed = false;

	// Display the top menu options with "File" highlighted.
	var menuNum = fileMenuNum; // 0: File, ..., 3: Help
	var subMenuItemNum = 0;
	var menuRetObj = displayTopMenuItems(doDCTESCMenu.mainMenuItemPositions, menuNum,
	                                     doDCTESCMenu.allMenus);
	// Input loop
	var userInput = "";
	var matchedMenuRetval = false; // Whether one of the menu return values was matched
	var continueOn = true;
	while (continueOn)
	{
		matchedMenuRetval = valMatchesMenuCode(menuRetObj.returnVal);
		// If a menu return value was matched, then set userInput to it.
		if (matchedMenuRetval)
			userInput = menuRetObj.returnVal;
		// If the user's input from the last menu was ESC, left, right, or one of the
		// characters from the menus, then set userInput to the last menu keypress.
		else if (inputMatchesMenuSelection(menuRetObj.userInput))
			userInput = menuRetObj.userInput;
		// If nothing from the menu was matched, then get a key from the user.
		else
			userInput = getUserKey(K_UPPER|K_NOECHO|K_NOCRLF|K_NOSPIN);
		menuRetObj.userInput = "";

		// If a menu return code was matched or userInput is blank (the
		// timeout was hit), then exit out of the loop.
		if (matchedMenuRetval || (userInput == ""))
			break;

		// Take appropriate action based on the user's input.
		switch (userInput)
		{
			case KEY_LEFT:
				if (menuNum == 0)
					menuNum = 3;
				else
					--menuNum;
				// Don't allow the sysop menu for non-sysops.
				if ((menuNum == sysopMenuNum) && !user.is_sysop)
					--menuNum;
				subMenuItemNum = 0;
				menuRetObj = displayTopMenuItems(doDCTESCMenu.mainMenuItemPositions, menuNum, doDCTESCMenu.allMenus);
				break;
			case KEY_RIGHT:
				if (menuNum == 3)
					menuNum = 0;
				else
					++menuNum;
				// Don't allow the sysop menu for non-sysops.
				if ((menuNum == sysopMenuNum) && !user.is_sysop)
					++menuNum;
				subMenuItemNum = 0;
				menuRetObj = displayTopMenuItems(doDCTESCMenu.mainMenuItemPositions, menuNum, doDCTESCMenu.allMenus);
				break;
			case KEY_UP:
			case KEY_DOWN:
				break;
			case KEY_ENTER: // Selected an item from the menu
				// Set userInput to the return code from the menu so that it will
				// be returned from this function.
				userInput = menuRetObj.returnVal;
			case "S":       // Save
			case CTRL_Z:    // Save
			case "A":       // Abort
			case CTRL_A:    // Abort
			case "I":       // Import file for sysop, or Insert/Overwrite toggle for non-sysop
			case CTRL_O:    // Import file (for sysop)
			case "X":       // Export file (for sysop)
			case CTRL_X:    // Export file (for sysop)
			case CTRL_V:    // Insert/overwrite toggle
			case "F":       // Find text
			case CTRL_F:    // Find text
			case "M":       // Insert meme
			case "N":       // User settings
			case CTRL_U:    // User settings
			case "O":       // Command List
			case "G":       // General help
			case "P":       // Program info
			case "E":       // Edit the message
			case KEY_ESC:   // Edit the message
				continueOn = false;
				break;
			case "C":       // Cross-post
			case CTRL_C:    // Cross-post
				if (pCanCrossPost)
					continueOn = false;
				break;
			case "T": // List text replacements
			case CTRL_T: // List text replacements
				if (gConfigSettings.enableTextReplacements)
					continueOn = false;
				break;
			case "L": // Spell checker
			case CTRL_W: // Spell checker
				continueOn = false;
				break;
			default:
				break;
		}
	}

	// We've exited the menu, so refresh the top menu border and the message text
	// on the screen.
	displayTextAreaTopBorder_DCTStyle(doDCTESCMenu.mainMenuItemPositions.mainMenuY, pEditLeft, pEditRight);

	// Set chosenAction based on the user's input
	// Save the message
	if ((userInput == "S") || (userInput == CTRL_Z) || (userInput == DCTMENU_FILE_SAVE))
		chosenAction = ESC_MENU_SAVE;
	// Abort
	else if ((userInput == "A") || (userInput == CTRL_A) || (userInput == DCTMENU_FILE_ABORT))
		chosenAction = ESC_MENU_ABORT;
	// Toggle insert/overwrite mode
	else if ((userInput == CTRL_V) || (userInput == DCTMENU_EDIT_INSERT_TOGGLE))
		chosenAction = ESC_MENU_INS_OVR_TOGGLE;
	// Import file (sysop only)
	else if (userInput == DCTMENU_SYSOP_IMPORT_FILE)
	{
		if (user.is_sysop)
			chosenAction = ESC_MENU_SYSOP_IMPORT_FILE;
	}
	// Import file for sysop, or Insert/Overwrite toggle for non-sysop
	else if (userInput == "I")
	{
		if (user.is_sysop)
			chosenAction = ESC_MENU_SYSOP_IMPORT_FILE;
		else
			chosenAction = ESC_MENU_INS_OVR_TOGGLE;
	}
	// Find text
	else if ((userInput == CTRL_F) || (userInput == "F") || (userInput == DCTMENU_EDIT_FIND_TEXT))
		chosenAction = ESC_MENU_FIND_TEXT;
	// Command List
	else if ((userInput == "O") || (userInput == DCTMENU_HELP_COMMAND_LIST))
		chosenAction = ESC_MENU_HELP_COMMAND_LIST;
	// General help
	else if ((userInput == "G") || (userInput == DCTMENU_GRAPHIC_CHAR))
		chosenAction = ESC_MENU_HELP_GRAPHIC_CHAR;
	// Program info
	else if ((userInput == "P") || (userInput == DCTMENU_HELP_PROGRAM_INFO))
		chosenAction = ESC_MENU_HELP_PROGRAM_INFO;
	// Export the message
	else if ((userInput == "X") || (userInput == DCTMENU_SYSOP_EXPORT_FILE))
	{
		if (user.is_sysop)
			chosenAction = ESC_MENU_SYSOP_EXPORT_FILE;
	}
	// Edit the message
	else if ((userInput == "E") || (userInput == KEY_ESC))
		chosenAction = ESC_MENU_EDIT_MESSAGE;
	// Cross-post
	else if ((userInput == CTRL_C) || (userInput == "C") || (userInput == DCTMENU_CROSS_POST))
	{
		if (pCanCrossPost)
			chosenAction = ESC_MENU_CROSS_POST_MESSAGE;
	}
	// List text replacements
	else if ((userInput == CTRL_T) || (userInput == "T") || (userInput == DCTMENU_LIST_TXT_REPLACEMENTS))
	{
		if (gConfigSettings.enableTextReplacements)
			chosenAction = ESC_MENU_LIST_TEXT_REPLACEMENTS;
	}
	// User settings
	else if ((userInput == CTRL_U) || (userInput == "N") || (userInput == DCTMENU_EDIT_SETTINGS))
		chosenAction = ESC_MENU_USER_SETTINGS;
	// Spell checker
	else if ((userInput == CTRL_W) || (userInput == "L") || (userInput == DCTMENU_EDIT_SPELL_CHECKER))
		chosenAction = ESC_MENU_SPELL_CHECK;
	// Insert meme
	else if ((userInput == "M") || (userInput == DCTMENU_EDIT_INSERT_MEME))
		chosenAction = ESC_MENU_INSERT_MEME;

	return chosenAction;
}

// This function returns whether a value matches any of the DCT Edit menu return
// values.  This is used in doDCTESCMenu()'s input loop;
//
// Parameters:
//  pVal: The value to test
//
// Return: Boolean - Whether or not the value matches a DCT Edit menu return value.
function valMatchesMenuCode(pVal)
{
	var valMatches = false;
	valMatches = ((pVal == DCTMENU_FILE_SAVE) || (pVal == DCTMENU_FILE_ABORT) ||
	              (pVal == DCTMENU_FILE_EDIT) || (pVal == DCTMENU_EDIT_INSERT_TOGGLE) ||
	              (pVal == DCTMENU_EDIT_FIND_TEXT) || (pVal == DCTMENU_HELP_COMMAND_LIST) ||
	              (pVal == DCTMENU_GRAPHIC_CHAR) || (pVal == DCTMENU_HELP_PROGRAM_INFO) ||
	              (pVal == DCTMENU_EDIT_SETTINGS) || (pVal == DCTMENU_EDIT_SPELL_CHECKER));
	if (gConfigSettings.enableTextReplacements)
		valMatches = (valMatches || (pVal == DCTMENU_LIST_TXT_REPLACEMENTS));
	if (user.is_sysop)
		valMatches = (valMatches || (pVal == DCTMENU_SYSOP_IMPORT_FILE) || (pVal == DCTMENU_SYSOP_EXPORT_FILE));
	return valMatches;
}

// This function returns whether a user input matches a selection from one of the
// menus.  This is used in doDCTESCMenu()'s input loop;
//
// Parameters:
//  pInput: The user input to test
function inputMatchesMenuSelection(pInput)
{
   return((pInput == KEY_ESC) || (pInput == KEY_LEFT) ||
           (pInput == KEY_RIGHT) || (pInput == KEY_ENTER) ||
           (pInput == "S") || (pInput == "A") || (pInput == "E") ||
           (pInput == "I") || (pInput == "M") || (user.is_sysop && (pInput == "X")) ||
           (pInput == "F") || (pInput == "C") || (pInput == "G") ||
           (pInput == "P") || (pInput == "T"));
}

//////////////////////////////////////////////////////////////////////////////////////////
// DCTMenu object functions

// Constructs a menu item for a DCT menu
function DCTMenuItem()
{
   this.text = "";        // The item text
   this.hotkeyIndex = -1; // Index of the hotkey in the text (-1 for no hotkey).
   this.hotkey = "";      // The shortcut key for the item (blank for no hotkey).
   this.returnVal = 0;    // Return value for the item
}

// DCTMenu constructor: Constructs a DCTEdit-style menu.
//
// Parameters:
//  pTopLeftX: The upper-left screen column
//  pTopLeftY: The upper-left screen row
function DCTMenu(pTopLeftX, pTopLeftY)
{
   this.colors = {
		// Unselected item colors
		unselected: "\x01n\x01" + "7\x01k",
		// Selected item colors
		selected: "\x01n\x01w",
		// Other colors
		hotkey: "\x01h\x01w",
		border: "\x01n\x01" + "+\x01k"
   };

   this.topLeftX = 1; // Upper-left screen column
   if ((pTopLeftX != null) && (pTopLeftX > 0) && (pTopLeftX <= console.screen_columns))
      this.topLeftX = pTopLeftX;
   this.topLeftY = 1; // Upper-left screen row
   if ((pTopLeftY != null) && (pTopLeftY > 0) && (pTopLeftY <= console.screen_rows))
      this.topLeftY = pTopLeftY;
   this.width = 0;
   this.height = 0;
   this.selectedItemIndex = 0;

   // this.menuItems will contain DCTMenuItem objects.
   this.menuItems = [];
   // hotkeyRetvals is an array, indexed by hotkey, that contains
   // the return values for each hotkey.
   this.hotkeyRetvals = [];

   // exitLoopKeys will contain keys that will exit the input loop
   // when pressed by the user, so that calling code can catch them.
   // It's indexed by the key, and the value is the return code that
   // should be returned for that key.
   this.exitLoopKeys = [];

   // Border style: "single" or "double"
   this.borderStyle = "single";

   // clearSpaceAroundMenu controls whether or not to clear one space
   // around the menu when it is drawn.
   this.clearSpaceAroundMenu = false;
   // clearSpaceColor controls which color to use when drawing the
   // clear space around the menu.
   this.clearSpaceColor = "\x01n";
   // clearSpaceTopText specifies text to display above the top of the
   // menu when clearing space around it.
   this.clearSpaceTopText = "";

   // Timeout (in milliseconds) for the input loop.  Default to 1 minute.
   this.timeoutMS = 60000;

   // Member functions
   this.addItem = DCTMenu_AddItem;
   this.addExitLoopKey = DCTMenu_AddExitLoopKey;
   this.displayItem = DCTMenu_DisplayItem;
   this.doInputLoop = DCTMenu_DoInputLoop;
   this.numItems = DCTMenu_NumItems;
   this.removeAllItems = DCTMenu_RemoveAllItems;
}
// Adds an item to the DCTMenu.
//
// Parameters:
//  pText: The text of the menu item.  Note that a & precedes a hotkey.
//  pReturnVal: The value to return upon selection of the item
function DCTMenu_AddItem(pText, pReturnVal)
{
   if (pText == "")
      return;

   var item = new DCTMenuItem();
   item.returnVal = pReturnVal;
   // Look for a & in pText, and if one is found, use the next character as
   // its hotkey.
   var ampersandIndex = pText.indexOf("&");
   if (ampersandIndex > -1)
   {
      // If pText has text after ampersandIndex, then set up
      // the next character as the hotkey in the item.
      if (pText.length > ampersandIndex+1)
      {
         item.hotkeyIndex = ampersandIndex;
         item.hotkey = pText.substr(ampersandIndex+1, 1);
         // Set the text of the item.  The text should not include
         // the ampersand.
         item.text = pText.substr(0, ampersandIndex) + pText.substr(ampersandIndex+1);
         // Add the hotkey & return value to this.hotkeyRetvals
         this.hotkeyRetvals[item.hotkey.toUpperCase()] = pReturnVal;

         // If the menu is not wide enough for this item's text, then
         // update this.width.
         if (this.width < item.text.length + 2)
            this.width = item.text.length + 2;
         // Also update this.height
         if (this.height == 0)
            this.height = 3;
         else
            ++this.height;
      }
      else
      {
         // pText does not have text after ampersandIndex.
         item.text = pText.substr(0, ampersandIndex);
      }
   }
   else
   {
      // No ampersand was found in pText.
      item.text = pText;
   }

   // Add the item to this.menuItems
   this.menuItems.push(item);
}
// Adds a key that will exit the input loop when pressed by the user.
//
// Parameters:
//  pKey: The key that should cause the input loop to exit
//  pReturnValue: The return value of the input loop when the key is pressed.
//                If this is not specified, a default value of -1 will be used.
function DCTMenu_AddExitLoopKey(pKey, pReturnValue)
{
   var val = -1;
   if (typeof(pReturnValue) == "number")
      val = pReturnValue;

   this.exitLoopKeys[pKey] = val;
}
// Displays an item on the menu.
//
// Parameters:
//  pItemIndex: The index of the item in the menuItems array
//  pPrintBorders: Boolean - Whether or not to display the horizontal
//                 borders on each side of the menu item text.
function DCTMenu_DisplayItem(pItemIndex, pPrintBorders)
{
   var printBorders = false;
   if (pPrintBorders != null)
      printBorders = pPrintBorders;

   // Determine whether to use the selected item color or unselected
   // item color.
   var itemColor = "";
   if (pItemIndex == this.selectedItemIndex)
      itemColor = this.colors.selected;
   else
      itemColor = this.colors.unselected;

   // Print the menu item text on the screen.
   if (printBorders)
   {
      console.gotoxy(this.topLeftX, this.topLeftY + pItemIndex + 1);
      if (this.borderStyle == "single")
         console.print(this.colors.border + CP437_BOX_DRAWINGS_LIGHT_VERTICAL);
      else if (this.borderStyle == "double")
         console.print(this.colors.border + CP437_BOX_DRAWINGS_DOUBLE_VERTICAL);
   }
   else
      console.gotoxy(this.topLeftX + 1, this.topLeftY + pItemIndex + 1);
   // If the menu item has a hotkey, then write the appropriate character
   // in the hotkey color.
   if (this.menuItems[pItemIndex].hotkeyIndex > -1)
   {
      console.print(itemColor +
                    this.menuItems[pItemIndex].text.substr(0, this.menuItems[pItemIndex].hotkeyIndex) +
                    this.colors.hotkey + this.menuItems[pItemIndex].hotkey + itemColor +
                    this.menuItems[pItemIndex].text.substr(this.menuItems[pItemIndex].hotkeyIndex + 1));
   }
   else
   {
      console.print(itemColor + this.menuItems[pItemIndex].text);
   }
   // If the item text isn't wide enough to fill the entire inner width, then
   // clear the line up until the right border.
   var innerWidth = this.width - 2;
   if (this.menuItems[pItemIndex].text.length < innerWidth)
   {
      for (var i = this.menuItems[pItemIndex].text.length; i < innerWidth; ++i)
         console.print(" ");
   }
   // Print the right border character if specified.
   if (printBorders)
   {
      if (this.borderStyle == "single")
         console.print(this.colors.border + CP437_BOX_DRAWINGS_LIGHT_VERTICAL);
      else if (this.borderStyle == "double")
         console.print(this.colors.border + CP437_BOX_DRAWINGS_DOUBLE_VERTICAL);
   }
}
// Displays the DCT menu and enters the input loop.
//
// Return value: An object containing the following properties:
//  returnVal: The return code of the item selected, or -1 if no
//             item was selected.
//  userInput: The last user input
function DCTMenu_DoInputLoop()
{
	var returnObj = {
		returnVal: -1,
		userInput: ""
	};

	// If clearSpaceAroundMenu is true, then draw a blank row
	// above the menu.
	if (this.clearSpaceAroundMenu && (this.topLeftY > 1))
	{
		// If there is room, output a space to the left, diagonal
		// from the top-left corner of the menu.
		if (this.topLeftX > 1)
		{
			console.gotoxy(this.topLeftX-1, this.topLeftY-1);
			console.print(this.clearSpaceColor + " ");
		}
		else
			console.gotoxy(this.topLeftX, this.topLeftY-1);

		// Output this.clearSpaceTopText
		console.print(this.clearSpaceTopText);
		// Output the rest of the blank space
		var textLen = console.strlen(this.clearSpaceTopText);
		if (textLen < this.width)
		{
			var numSpaces = this.width - textLen;
			if (this.topLeftX + this.width < console.screen_columns)
				++numSpaces;
			for (var i = 0; i < numSpaces; ++i)
				console.print(this.clearSpaceColor + " ");
		}
	}

	// Before drawing the top border, if clearSpaceAroundMenu is
	// true, put space before the border.
	if (this.clearSpaceAroundMenu && (this.topLeftY > 1))
	{
		console.gotoxy(this.topLeftX-1, this.topLeftY);
		console.print(this.clearSpaceColor + " ");
	}
	else
		console.gotoxy(this.topLeftX, this.topLeftY);
	// TODO:
	// 2025-05-06 - Kludge: For some reason, the menu isn't
	// being drawn now until a key is pressed. Clearing the
	// key buffer seems to help let the menu be drawn:
	console.clearkeybuffer();
	// This also helped get the menu to draw:
	//console.ungetstr(KEY_ENTER);
	//console.getkey(K_NOCRLF|K_NOECHO|K_NOSPIN);
	// Draw the top border
	var innerWidth = this.width - 2;
	if (this.borderStyle == "single")
	{
		console.print(this.colors.border + CP437_BOX_DRAWINGS_UPPER_LEFT_SINGLE);
		for (var i = 0; i < innerWidth; ++i)
			console.print(CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE);
		console.print(this.colors.border + CP437_BOX_DRAWINGS_UPPER_RIGHT_SINGLE);
	}
	else if (this.borderStyle == "double")
	{
		console.print(this.colors.border + CP437_BOX_DRAWINGS_UPPER_LEFT_DOUBLE);
		for (var i = 0; i < innerWidth; ++i)
			console.print(CP437_BOX_DRAWINGS_HORIZONTAL_DOUBLE);
		console.print(this.colors.border + CP437_BOX_DRAWINGS_UPPER_RIGHT_DOUBLE);
	}
	// If clearSpaceAroundMenu is true, then put a space after the border.
	if (this.clearSpaceAroundMenu && (this.topLeftX + this.width < console.screen_columns))
		console.print(this.clearSpaceColor + " ");

	// Print the menu items (and side spaces outside the menu if
	// clearSpaceAroundMenu is true).
	var itemColor = "";
	for (var i = 0; i < this.menuItems.length; ++i)
	{
		if (this.clearSpaceAroundMenu && (this.topLeftX > 1))
		{
			console.gotoxy(this.topLeftX-1, this.topLeftY + i + 1);
			console.print(this.clearSpaceColor + " ");
		}

		this.displayItem(i, true);

		if (this.clearSpaceAroundMenu && (this.topLeftX + this.width < console.screen_columns))
		{
			console.gotoxy(this.topLeftX + this.width, this.topLeftY + i + 1);
			console.print(this.clearSpaceColor + " ");
		}
	}

	// Before drawing the bottom border, if clearSpaceAroundMenu is
	// true, put space before the border.
	if (this.clearSpaceAroundMenu && (this.topLeftY > 1))
	{
		console.gotoxy(this.topLeftX - 1, this.topLeftY + this.height - 1);
		console.print(this.clearSpaceColor + " ");
	}
	else
		console.gotoxy(this.topLeftX, this.topLeftY + this.height - 1);
	// Draw the bottom border
	if (this.borderStyle == "single")
	{
		console.print(this.colors.border + CP437_BOX_DRAWINGS_LOWER_LEFT_SINGLE);
		for (var i = 0; i < innerWidth; ++i)
			console.print(CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE);
		console.print(CP437_BOX_DRAWINGS_LOWER_RIGHT_SINGLE);
	}
	else if (this.borderStyle == "double")
	{
		console.print(this.colors.border + CP437_BOX_DRAWINGS_LOWER_LEFT_DOUBLE);
		for (var i = 0; i < innerWidth; ++i)
			console.print(CP437_BOX_DRAWINGS_HORIZONTAL_DOUBLE);
		console.print(CP437_BOX_DRAWINGS_LOWER_RIGHT_DOUBLE);
	}
	// If clearSpaceAroundMenu is true, then put a space after the border.
	if (this.clearSpaceAroundMenu && (this.topLeftX + this.width < console.screen_columns))
		console.print(this.clearSpaceColor + " ");

	// If clearSpaceAroundMenu is true, then draw a blank row
	// below the menu.
	if (this.clearSpaceAroundMenu && (this.topLeftY + this.height < console.screen_rows))
	{
		var numSpaces = this.width + 2;
		if (this.topLeftX > 1)
			console.gotoxy(this.topLeftX-1, this.topLeftY + this.height);
		else
		{
			console.gotoxy(this.topLeftX, this.topLeftY + this.height);
			--numSpaces;
		}

		if (this.topLeftX + this.width >= console.screen_columns)
			--numSpaces;

		for (var i = 0; i < numSpaces; ++i)
			console.print(this.clearSpaceColor + " ");
	}

	// Place the cursor on the line of the selected item
	console.gotoxy(this.topLeftX + 1, this.topLeftY + this.selectedItemIndex + 1);

	// Keep track of the current cursor position
	var curpos = {
		x: this.topLeftX + 1,
		y: this.topLeftY + this.selectedItemIndex + 1
	};

	// Input loop
	const topItemLineNumber = this.topLeftY + 1;
	const bottomItemLineNumber = this.topLeftY + this.height - 1;
	var continueOn = true;
	while (continueOn)
	{
		// Get a key, (time out after the selected time), and take appropriate action.
		returnObj.userInput = getUserKey(K_UPPER|K_NOECHO|K_NOCRLF|K_NOSPIN);
		// If the user input is blank, then the input timed out, and we should quit.
		if (returnObj.userInput == "")
		{
			continueOn = false;
			break;
		}

		// Take appropriate action, depending on the user's keypress.
		switch (returnObj.userInput)
		{
			case KEY_ENTER:
				// Set returnObj.returnVal to the currently-selected item's returnVal,
				// and exit the input loop.
				returnObj.returnVal = this.menuItems[this.selectedItemIndex].returnVal;
				continueOn = false;
				break;
			case KEY_UP:
				// Go up one item
				if (this.menuItems.length > 1)
				{
					// If we're below the top menu item, then go up one item.  Otherwise,
					// go to the last menu item.
					var oldIndex = this.selectedItemIndex;
					if ((curpos.y > topItemLineNumber) && (this.selectedItemIndex > 0))
					{
						--curpos.y;
						--this.selectedItemIndex;
					}
					else
					{
						curpos.y = bottomItemLineNumber - 1;
						this.selectedItemIndex = this.menuItems.length - 1;
					}
					// Refresh the items on the screen so that the item colors
					// are updated.
					this.displayItem(oldIndex, false);
					this.displayItem(this.selectedItemIndex, false);
				}
				break;
			case KEY_DOWN:
				// Go down one item
				if (this.menuItems.length > 1)
				{
					// If we're above the bottom menu item, then go down one item.  Otherwise,
					// go to the first menu item.
					var oldIndex = this.selectedItemIndex;
					if ((curpos.y < bottomItemLineNumber) && (this.selectedItemIndex < this.menuItems.length - 1))
					{
						++curpos.y;
						++this.selectedItemIndex;
					}
					else
					{
						curpos.y = this.topLeftY + 1;
						this.selectedItemIndex = 0;
					}
					// Refresh the items on the screen so that the item colors
					// are updated.
					this.displayItem(oldIndex, false);
					this.displayItem(this.selectedItemIndex, false);
				}
				break;
			case KEY_ESC:
				continueOn = false;
				break;
			default:
				// If the user's input is one of the hotkeys, then stop the
				// input loop and return with the return code for the hotkey.
				if (typeof(this.hotkeyRetvals[returnObj.userInput]) != "undefined")
				{
					returnObj.returnVal = this.hotkeyRetvals[returnObj.userInput];
					continueOn = false;
				}
				// If the user's input is one of the loop-exit keys, then stop
				// the input loop.
				else if (typeof(this.exitLoopKeys[returnObj.userInput]) != "undefined")
				{
					returnObj.returnVal = this.exitLoopKeys[returnObj.userInput];
					continueOn = false;
				}
				break;
		}
	}

	return returnObj;
}

// Returns the number of items in the menu.
function DCTMenu_NumItems()
{
   return this.menuItems.length;
}
// Removes all items from a DCTMenu.
function DCTMenu_RemoveAllItems()
{
   this.width = 0;
   this.height = 0;
   this.selectedItemIndex = 0;
   this.menuItems = [];
   this.hotkeyRetvals = [];
   this.exitLoopKeys = [];
}
