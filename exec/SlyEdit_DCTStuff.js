/* This file contains DCTEdit-specific functions for SlyEdit.
 *
 * Author: Eric Oulashin (AKA Nightfox)
 * BBS: Digital Distortion
 * BBS address: digdist.bbsindex.com
 *
 * Date       User              Description
 * 2009-05-11 Eric Oulashin     Started development
 * 2009-08-09 Eric Oulashin     More development & testing
 * 2009-08-22 Eric Oulashin     Version 1.00
 *                              Initial public release
 * 2009-12-03 Eric Oulashin     Added support for color schemes.
 *                              Added readColorConfig().
 * 2009-12-31 Eric Oulashin     Updated promptYesNo_DCTStyle()
 *                              so that the return variable,
 *                              userResponse, defaults to the
 *                              value of pDefaultYes.
 * 2010-01-02 Eric Oulashin     Removed abortConfirm_DCTStyle(),
 *                              since it's no longer used anymore.
 * 2010-11-19 Eric Oulashin     Updated doDCTMenu() so that when the
 *                              pDisplayMessageRectangle() function is
 *                              called, it passes true at the end to
 *                              tell it to clear extra spaces between
 *                              the end of the text lines up to the
 *                              width given.
 * 2010-11-21 Eric Oulashin     Updated promptYesNo_DCTStyle() so that when the
 *                              pDisplayMessageRectangle() function is
 *                              called, it passes true at the end to
 *                              tell it to clear extra spaces between
 *                              the end of the text lines up to the
 *                              width given.
 * 2011-02-02 Eric Oulashin     Moved the time displaying code into
 *                              a new function, displayTime_DCTStyle().
 * 2012-12-21 Eric Oulashin     Removed gStartupPath from the beginning
 *                              of the theme filename, since the path is
 *                              now set in ReadSlyEditConfigFile() in
 *                              SlyEdit_Misc.js.
 * 2013-01-19 Eric Oulashin     Updated readColorConfig() to move the
 *                              general color settings to gConfigSettings.genColors.*
 * 2013-01-24 Eric Oulashin     Updated doDCTMenu() to include an option
 *                              for cross-posting on the File menu.
 * 2013-08-23 Eric Oulashin     Updated readColorConfig() with the new general color
 *                              configuration settings.
 * 2013-08-28 Eric Oulashin     Simplified readColorConfig() by having it call
 *                              moveGenColorsToGenSettings() (defined in
 *                              SlyEdit_Misc.js) to move the general colors
 *                              into the genColors array in the configuration
 *                              object.
 */

load("sbbsdefs.js");
load(getScriptDir() + "SlyEdit_Misc.js");

// DCTEdit menu item return values
var DCTMENU_FILE_SAVE = 0;
var DCTMENU_FILE_ABORT = 1;
var DCTMENU_FILE_EDIT = 2;
var DCTMENU_EDIT_INSERT_TOGGLE = 3;
var DCTMENU_EDIT_FIND_TEXT = 4;
//var DCTMENU_EDIT_SPELL_CHECKER = 5;
//var DCTMENU_EDIT_SETUP = 6;
var DCTMENU_SYSOP_IMPORT_FILE = 7;
var DCTMENU_SYSOP_EXPORT_FILE = 11;
var DCTMENU_HELP_COMMAND_LIST = 8;
var DCTMENU_HELP_GENERAL = 9;
var DCTMENU_HELP_PROGRAM_INFO = 10;
var DCTMENU_CROSS_POST = 12;
var DCTMENU_LIST_TXT_REPLACEMENTS = 13;

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
   var colors = readValueSettingConfigFile(pFilename, 512);
   if (colors != null)
   {
      gConfigSettings.DCTColors = colors;
      // Move the general color settings into gConfigSettings.genColors.*
      if (EDITOR_STYLE == "DCT")
        moveGenColorsToGenSettings(gConfigSettings.DCTColors, gConfigSettings);
   }
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
//  pEditLinesIndex: The index of the message line at the top of the edit area
//  pDisplayEditLines: The function that displays the edit lines
function redrawScreen_DCTStyle(pEditLeft, pEditRight, pEditTop, pEditBottom, pEditColor,
                                pInsertMode, pUseQuotes, pEditLinesIndex, pDisplayEditLines)
{
	// Top header
	// Generate & display the top border line (Note: Generate this
	// border only once, for efficiency).
	if (typeof(redrawScreen_DCTStyle.topBorder) == "undefined")
	{
      var innerWidth = console.screen_columns - 2;
      redrawScreen_DCTStyle.topBorder = UPPER_LEFT_SINGLE;
      for (var i = 0; i < innerWidth; ++i)
         redrawScreen_DCTStyle.topBorder += HORIZONTAL_SINGLE;
      redrawScreen_DCTStyle.topBorder += UPPER_RIGHT_SINGLE;
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
	console.gotoxy(1, lineNum);
	// Calculate the width of the from name field: 28 characters, based
	// on an 80-column screen width.
	var fieldWidth = (console.screen_columns * (28/80)).toFixed(0);
	screenText = gFromName.substr(0, fieldWidth);
	console.print(randomTwoColorString(VERTICAL_SINGLE, gConfigSettings.DCTColors.TopBorderColor1,
	                                   gConfigSettings.DCTColors.TopBorderColor2) +
				  " " + gConfigSettings.DCTColors.TopLabelColor + "From " +
				  gConfigSettings.DCTColors.TopLabelColonColor + ": " +
				  gConfigSettings.DCTColors.TopInfoBracketColor + "[" +
				  gConfigSettings.DCTColors.TopFromFillColor + DOT_CHAR + 
				  gConfigSettings.DCTColors.TopFromColor + screenText +
				  gConfigSettings.DCTColors.TopFromFillColor);
	fieldWidth -= (screenText.length+1);
	for (var i = 0; i < fieldWidth; ++i)
		console.print(DOT_CHAR);
	console.print(gConfigSettings.DCTColors.TopInfoBracketColor + "]");

	// Message area
	fieldWidth = (console.screen_columns * (27/80)).toFixed(0);
	screenText = gMsgArea.substr(0, fieldWidth);
	var startX = console.screen_columns - fieldWidth - 9;
	console.gotoxy(startX, lineNum);
	console.print(gConfigSettings.DCTColors.TopLabelColor + "Area" +
	              gConfigSettings.DCTColors.TopLabelColonColor + ": " +
	              gConfigSettings.DCTColors.TopInfoBracketColor + "[" +
	              gConfigSettings.DCTColors.TopAreaFillColor + DOT_CHAR +
	              gConfigSettings.DCTColors.TopAreaColor + screenText +
	              gConfigSettings.DCTColors.TopAreaFillColor);
	fieldWidth -= (screenText.length+1);
	for (var i = 0; i < fieldWidth; ++i)
		console.print(DOT_CHAR);
	console.print(gConfigSettings.DCTColors.TopInfoBracketColor + "] " +
	              randomTwoColorString(VERTICAL_SINGLE,
	                                   gConfigSettings.DCTColors.TopBorderColor1,
	                                   gConfigSettings.DCTColors.TopBorderColor2));

	// Next line: To, Time, time Left
	++lineNum;
	console.gotoxy(1, lineNum);
	// To name
	fieldWidth = (console.screen_columns * (28/80)).toFixed(0);
	screenText = gToName.substr(0, fieldWidth);
	console.print(randomTwoColorString(VERTICAL_SINGLE, gConfigSettings.DCTColors.TopBorderColor1,
	                                   gConfigSettings.DCTColors.TopBorderColor2) +
				  " " + gConfigSettings.DCTColors.TopLabelColor + "To   " +
				  gConfigSettings.DCTColors.TopLabelColonColor + ": " +
				  gConfigSettings.DCTColors.TopInfoBracketColor + "[" +
				  gConfigSettings.DCTColors.TopToFillColor + DOT_CHAR +
				  gConfigSettings.DCTColors.TopToColor + screenText +
				  gConfigSettings.DCTColors.TopToFillColor);
	fieldWidth -= (screenText.length+1);
	for (var i = 0; i < fieldWidth; ++i)
		console.print(DOT_CHAR);
	console.print(gConfigSettings.DCTColors.TopInfoBracketColor + "]");

	// Current time
	console.gotoxy(startX, lineNum);
	console.print(gConfigSettings.DCTColors.TopLabelColor + "Time" +
	              gConfigSettings.DCTColors.TopLabelColonColor + ": " +
	              gConfigSettings.DCTColors.TopInfoBracketColor + "[" +
	              gConfigSettings.DCTColors.TopTimeFillColor + DOT_CHAR);
	displayTime_DCTStyle();
	console.print(gConfigSettings.DCTColors.TopTimeFillColor + DOT_CHAR +
	              gConfigSettings.DCTColors.TopInfoBracketColor + "]");

	// Time left
	fieldWidth = (console.screen_columns * (7/80)).toFixed(0);
	startX = console.screen_columns - fieldWidth - 9;
	console.gotoxy(startX, lineNum);
	var timeStr = Math.floor(bbs.time_left / 60).toString();
	console.print(gConfigSettings.DCTColors.TopLabelColor + "Left" +
	              gConfigSettings.DCTColors.TopLabelColonColor + ": " +
	              gConfigSettings.DCTColors.TopInfoBracketColor + "[" +
	              gConfigSettings.DCTColors.TopTimeLeftFillColor + DOT_CHAR +
	              gConfigSettings.DCTColors.TopTimeLeftColor + timeStr +
	              gConfigSettings.DCTColors.TopTimeLeftFillColor);
	fieldWidth -= (timeStr.length+1);
	for (var i = 0; i < fieldWidth; ++i)
		console.print(DOT_CHAR);
	console.print(gConfigSettings.DCTColors.TopInfoBracketColor + "] " +
	              randomTwoColorString(VERTICAL_SINGLE,
	              gConfigSettings.DCTColors.TopBorderColor1,
	              gConfigSettings.DCTColors.TopBorderColor2));

	// Line 3: Subject
	++lineNum;
	console.gotoxy(1, lineNum);
	//fieldWidth = (console.screen_columns * (66/80)).toFixed(0);
	fieldWidth = console.screen_columns - 13;
	screenText = gMsgSubj.substr(0, fieldWidth);
	console.print(randomTwoColorString(LOWER_LEFT_SINGLE, gConfigSettings.DCTColors.TopBorderColor1,
	                                   gConfigSettings.DCTColors.TopBorderColor1) +
                 " " + gConfigSettings.DCTColors.TopLabelColor + "Subj " +
                 gConfigSettings.DCTColors.TopLabelColonColor + ": " +
				     gConfigSettings.DCTColors.TopInfoBracketColor + "[" +
				     gConfigSettings.DCTColors.TopSubjFillColor + DOT_CHAR +
				     gConfigSettings.DCTColors.TopSubjColor + screenText +
				     gConfigSettings.DCTColors.TopSubjFillColor);
	fieldWidth -= (screenText.length+1);
	for (var i = 0; i < fieldWidth; ++i)
		console.print(DOT_CHAR);
	console.print(gConfigSettings.DCTColors.TopInfoBracketColor + "] " +
	              randomTwoColorString(LOWER_RIGHT_SINGLE,
	              gConfigSettings.DCTColors.TopBorderColor1,
	              gConfigSettings.DCTColors.TopBorderColor2));
	
	// Line 4: Top border for message area
	++lineNum;
	displayTextAreaTopBorder_DCTStyle(lineNum, pEditLeft, pEditRight);
	// If the screen is at least 82 characters wide, display horizontal
	// lines around the message editing area.
	if (console.screen_columns >= 82)
	{
		for (lineNum = pEditTop; lineNum <= pEditBottom; ++lineNum)
		{
			console.gotoxy(pEditLeft-1, lineNum);
			console.print(randomTwoColorString(VERTICAL_SINGLE,
			                                   gConfigSettings.DCTColors.EditAreaBorderColor1,
			                                   gConfigSettings.DCTColors.EditAreaBorderColor2));
			console.gotoxy(pEditRight+1, lineNum);
			console.print(randomTwoColorString(VERTICAL_SINGLE,
			                                   gConfigSettings.DCTColors.EditAreaBorderColor1,
			                                   gConfigSettings.DCTColors.EditAreaBorderColor2));
		}
	}

	// Display the bottom message area border and help line
	DisplayTextAreaBottomBorder_DCTStyle(pEditBottom+1, null, pEditLeft, pEditRight, pInsertMode);
	DisplayBottomHelpLine_DCTStyle(console.screen_rows, pUseQuotes);
	
	// Go to the start of the edit area
	console.gotoxy(pEditLeft, pEditTop);

	// Write the message text that has been entered thus far.
	pDisplayEditLines(pEditTop, pEditLinesIndex);
	console.print(pEditColor);
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
      if (console.screen_columns >= 82)
         numHorizontalChars += 2;

      displayTextAreaTopBorder_DCTStyle.border = UPPER_LEFT_SINGLE;
      for (var i = 0; i < numHorizontalChars; ++i)
         displayTextAreaTopBorder_DCTStyle.border += HORIZONTAL_SINGLE;
      displayTextAreaTopBorder_DCTStyle.border += UPPER_RIGHT_SINGLE;
      displayTextAreaTopBorder_DCTStyle.border =
               randomTwoColorString(displayTextAreaTopBorder_DCTStyle.border,
                                    gConfigSettings.DCTColors.EditAreaBorderColor1,
                                    gConfigSettings.DCTColors.EditAreaBorderColor2);
   }

	// Draw the line on the screen
	console.gotoxy((console.screen_columns >= 82 ? pEditLeft-1 : pEditLeft), pLineNum);
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
      // If the screen is at least 82 characters wide, add 2 to innerWidth
      // to make room for the vertical lines around the text area.
      if (console.screen_columns >= 82)
         innerWidth += 2;

      DisplayTextAreaBottomBorder_DCTStyle.border = LOWER_LEFT_SINGLE;

      // This loop uses innerWidth-6 to make way for the insert mode
      // text.
      for (var i = 0; i < innerWidth-6; ++i)
         DisplayTextAreaBottomBorder_DCTStyle.border += HORIZONTAL_SINGLE;
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
                     randomTwoColorString(HORIZONTAL_SINGLE + LOWER_RIGHT_SINGLE,
                                          gConfigSettings.DCTColors.EditAreaBorderColor1,
                                          gConfigSettings.DCTColors.EditAreaBorderColor2);
   }

   // Draw the border line on the screen.
   console.gotoxy((console.screen_columns >= 82 ? pEditLeft-1 : pEditLeft), pLineNum);
   console.print(DisplayTextAreaBottomBorder_DCTStyle.border);
}

// Displays the help line at the bottom of the screen, in the style
// of DCTEdit.
//
// Parameters:
//  pLineNum: The line number on the screen at which to draw the help line
//  pUsingQuotes: Boolean - Whether or not message quoting is enabled.
function DisplayBottomHelpLine_DCTStyle(pLineNum, pUsingQuotes)
{
   // For efficiency, define the help line variable only once.
   if (typeof(DisplayBottomHelpLine_DCTStyle.helpText) == "undefined")
   {
      DisplayBottomHelpLine_DCTStyle.helpText = gConfigSettings.DCTColors.BottomHelpBrackets +
                     "[" + gConfigSettings.DCTColors.BottomHelpKeys + "CTRL" +
                     gConfigSettings.DCTColors.BottomHelpFill + DOT_CHAR +
                     gConfigSettings.DCTColors.BottomHelpKeys + "Z" +
                     gConfigSettings.DCTColors.BottomHelpBrackets + "]n " +
                     gConfigSettings.DCTColors.BottomHelpKeyDesc + "Saven      " +
                     gConfigSettings.DCTColors.BottomHelpBrackets + "[" +
                     gConfigSettings.DCTColors.BottomHelpKeys + "CTRL" +
                     gConfigSettings.DCTColors.BottomHelpFill + DOT_CHAR +
                     gConfigSettings.DCTColors.BottomHelpKeys + "A" +
                     gConfigSettings.DCTColors.BottomHelpBrackets + "]n " +
                     gConfigSettings.DCTColors.BottomHelpKeyDesc + "Abort";
      // If we can allow message quoting, then add a text to show Ctrl-Q for
      // quoting.
      if (pUsingQuotes)
         DisplayBottomHelpLine_DCTStyle.helpText += "n      " +
                          gConfigSettings.DCTColors.BottomHelpBrackets + "[" +
                          gConfigSettings.DCTColors.BottomHelpKeys + "CTRL" +
                          gConfigSettings.DCTColors.BottomHelpFill + DOT_CHAR +
                          gConfigSettings.DCTColors.BottomHelpKeys + "Q" +
                          gConfigSettings.DCTColors.BottomHelpBrackets + "]n " +
                          gConfigSettings.DCTColors.BottomHelpKeyDesc + "Quote";
      DisplayBottomHelpLine_DCTStyle.helpText += "n      " +
                     gConfigSettings.DCTColors.BottomHelpBrackets + "[" +
                     gConfigSettings.DCTColors.BottomHelpKeys + "ESC" +
                     gConfigSettings.DCTColors.BottomHelpBrackets + "]n " +
                     gConfigSettings.DCTColors.BottomHelpKeyDesc + "Menu";
      // Center the text by padding it in the front with spaces.  This is done instead
      // of using console.center() because console.center() will output a newline,
      // which would not be good on the last line of the screen.
      var numSpaces = (console.screen_columns/2).toFixed(0)
                     - (strip_ctrl(DisplayBottomHelpLine_DCTStyle.helpText).length/2).toFixed(0);
      for (var i = 0; i < numSpaces; ++i)
         DisplayBottomHelpLine_DCTStyle.helpText = " " + DisplayBottomHelpLine_DCTStyle.helpText;
   }

   // Display the help line on the screen
   var lineNum = console.screen_rows;
	if ((typeof(pLineNum) != "undefined") && (pLineNum != null))
		lineNum = pLineNum;
   console.gotoxy(1, lineNum);
	console.print(DisplayBottomHelpLine_DCTStyle.helpText);
}

// Updates the insert mode displayd on the screen, for DCT Edit style.
//
// Parameters:
//  pEditRight: The rightmost column on the screen for the edit area
//  pEditBottom: The bottommost row on the screen for the edit area
//  pInsertMode: The insert mode ("INS" or "OVR")
function updateInsertModeOnScreen_DCTStyle(pEditRight, pEditBottom, pInsertMode)
{
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
                                + UPPER_LEFT_SINGLE + HORIZONTAL_SINGLE + " "
                                + gConfigSettings.DCTColors.QuoteWinBorderTextColor
                                + "Quote Window " + gConfigSettings.DCTColors.QuoteWinBorderColor;
      var curLength = strip_ctrl(DrawQuoteWindowTopBorder_DCTStyle.border).length;
      var endCol = console.screen_columns-1;
      for (var i = curLength; i < endCol; ++i)
         DrawQuoteWindowTopBorder_DCTStyle.border += HORIZONTAL_SINGLE;
      DrawQuoteWindowTopBorder_DCTStyle.border += UPPER_RIGHT_SINGLE;
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
      var quoteHelpText = " " + gConfigSettings.DCTColors.QuoteWinBorderTextColor
                         + "[Enter] Quote Line " + gConfigSettings.DCTColors.QuoteWinBorderColor
                         + " ";
      for (var i = 0; i < 3; ++i)
         quoteHelpText += HORIZONTAL_SINGLE;
      quoteHelpText += " " +  gConfigSettings.DCTColors.QuoteWinBorderTextColor
                     + "[ESC] Stop Quoting " + gConfigSettings.DCTColors.QuoteWinBorderColor
                     + " ";
      for (var i = 0; i < 3; ++i)
         quoteHelpText += HORIZONTAL_SINGLE;
      quoteHelpText += " " + gConfigSettings.DCTColors.QuoteWinBorderTextColor
                     + "[Up/Down] Scroll Lines ";

      // Figure out the starting horizontal position on the screen so that
      // the quote help text line can be centered.
      var helpTextStartX = ((console.screen_columns/2) - (strip_ctrl(quoteHelpText).length/2)).toFixed(0);

      // Start creating DrawQuoteWindowBottomBorder_DCTStyle.border with the
      // bottom border lines, up until helpTextStartX.
      DrawQuoteWindowBottomBorder_DCTStyle.border = gConfigSettings.DCTColors.QuoteWinBorderColor
                                                  + LOWER_LEFT_SINGLE;
      for (var XPos = pEditLeft+2; XPos < helpTextStartX; ++XPos)
         DrawQuoteWindowBottomBorder_DCTStyle.border += HORIZONTAL_SINGLE;
      // Add the help text, then display the rest of the bottom border characters.
      DrawQuoteWindowBottomBorder_DCTStyle.border += quoteHelpText
                                                   + gConfigSettings.DCTColors.QuoteWinBorderColor;
      for (var XPos = pEditLeft+2+strip_ctrl(quoteHelpText).length; XPos <= pEditRight-1; ++XPos)
         DrawQuoteWindowBottomBorder_DCTStyle.border += HORIZONTAL_SINGLE;
      DrawQuoteWindowBottomBorder_DCTStyle.border += LOWER_RIGHT_SINGLE;
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
function promptYesNo_DCTStyle(pQuestion, pBoxTitle, pDefaultYes, pParamObj)
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
   const boxX = (console.screen_columns/2).toFixed(0) - (boxWidth/2).toFixed(0);
   const boxY = +pParamObj.editTop + +(pParamObj.editHeight/2).toFixed(0) - +(boxHeight/2).toFixed(0);
   const innerBoxWidth = boxWidth - 2;

   // Display the question box
   // Upper-left corner, 1 horizontal line, and "Abort" text
   console.gotoxy(boxX, boxY);
   console.print(gConfigSettings.DCTColors.TextBoxBorder + UPPER_LEFT_SINGLE +
                 HORIZONTAL_SINGLE + " " + gConfigSettings.DCTColors.TextBoxBorderText +
                 pBoxTitle + gConfigSettings.DCTColors.TextBoxBorder + " ");
   // Remaining top box border
   for (var i = pBoxTitle.length + 5; i < boxWidth; ++i)
      console.print(HORIZONTAL_SINGLE);
   console.print(UPPER_RIGHT_SINGLE);
   // Inner box: Blank
   var endScreenLine = boxY + boxHeight - 2;
   for (var screenLine = boxY+1; screenLine < endScreenLine; ++screenLine)
   {
      console.gotoxy(boxX, screenLine);
      console.print(VERTICAL_SINGLE);
      for (var i = 0; i < innerBoxWidth; ++i)
         console.print(" ");
      console.print(VERTICAL_SINGLE);
   }
   // Bottom box border
   console.gotoxy(boxX, screenLine);
   console.print(LOWER_LEFT_SINGLE);
   for (var i = 0; i < innerBoxWidth; ++i)
      console.print(HORIZONTAL_SINGLE);
   console.print(LOWER_RIGHT_SINGLE);

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
		userInput = getUserKey(K_UPPER|K_NOECHO|K_NOCRLF|K_NOSPIN, gConfigSettings);
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

   // If the user chose not to abort, then erase the confirmation box and
   // put the cursor back where it originally was.
   if (!userResponse)
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
	console.gotoxy(52, 3);
	if (pTimeStr == null)
		console.print(gConfigSettings.DCTColors.TopTimeColor + getCurrentTimeStr());
	else
		console.print(gConfigSettings.DCTColors.TopTimeColor + pTimeStr);
}

// Displays & handles the input loop for the DCT Edit menu.
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
//  pIsSysop: Whether or not the user is a sysop.
//  pCanCrossPost: Whether or not cross-posting is allowed.
//
// Return value: An object containing the following properties:
//               userInput: The user's input from the menu loop.
//               returnVal: The return code from the menu.
function doDCTMenu(pEditLeft, pEditRight, pEditTop, pDisplayMessageRectangle,
                    pEditLinesIndex, pEditLineDiff, pIsSysop, pCanCrossPost)
{
   // This function displays the top menu options, with a given one highlighted.
   //
   // Parameters:
   //  pItemPositions: An object containing the menu item positions.
   //  pHighlightedItemNum: The index (0-based) of the menu item to be highlighted.
   //  pMenus: An array containing the menus
   //  pIsSysop: Whether or not the user is the sysop.
   //
   // Return value: The user's last keypress during the menu's input loop.
   function displayTopMenuItems(pItemPositions, pHighlightedItemNum, pMenus, pIsSysop)
   {
      // File
      console.gotoxy(pItemPositions.fileX, pItemPositions.mainMenuY);
      if (pHighlightedItemNum == 0)
      {
         console.print(gConfigSettings.DCTColors.SelectedMenuLabelBorders + THIN_RECTANGLE_RIGHT +
                       gConfigSettings.DCTColors.SelectedMenuLabelText + "File" +
                       gConfigSettings.DCTColors.SelectedMenuLabelBorders + THIN_RECTANGLE_LEFT);
      }
      else
      {
         console.print("n " + gConfigSettings.DCTColors.UnselectedMenuLabelText + "File" +
                       "n ");
      }
      // Edit
      console.gotoxy(pItemPositions.editX, pItemPositions.mainMenuY);
      if (pHighlightedItemNum == 1)
      {
         console.print(gConfigSettings.DCTColors.SelectedMenuLabelBorders + THIN_RECTANGLE_RIGHT +
                       gConfigSettings.DCTColors.SelectedMenuLabelText + "Edit" +
                       gConfigSettings.DCTColors.SelectedMenuLabelBorders + THIN_RECTANGLE_LEFT);
      }
      else
      {
         console.print("n " + gConfigSettings.DCTColors.UnselectedMenuLabelText + "Edit" +
                       "n ");
      }
      // SysOp
      if (pIsSysop)
      {
         console.gotoxy(pItemPositions.sysopX, pItemPositions.mainMenuY);
         if (pHighlightedItemNum == 2)
         {
            console.print(gConfigSettings.DCTColors.SelectedMenuLabelBorders + THIN_RECTANGLE_RIGHT +
                          gConfigSettings.DCTColors.SelectedMenuLabelText + "SysOp" +
                          gConfigSettings.DCTColors.SelectedMenuLabelBorders + THIN_RECTANGLE_LEFT);
         }
         else
         {
            console.print("n " + gConfigSettings.DCTColors.UnselectedMenuLabelText + "SysOp" +
                          "n ");
         }
      }
      // Help
      console.gotoxy(pItemPositions.helpX, pItemPositions.mainMenuY);
      if (pHighlightedItemNum == 3)
      {
         console.print(gConfigSettings.DCTColors.SelectedMenuLabelBorders + THIN_RECTANGLE_RIGHT +
                       gConfigSettings.DCTColors.SelectedMenuLabelText + "Help" +
                       gConfigSettings.DCTColors.SelectedMenuLabelBorders + THIN_RECTANGLE_LEFT);
      }
      else
      {
         console.print("n " + gConfigSettings.DCTColors.UnselectedMenuLabelText + "Help" +
                       "n ");
      }

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
   if (typeof(doDCTMenu.mainMenuItemPositions) == "undefined")
   {
      doDCTMenu.mainMenuItemPositions = new Object();
      // Vertical position on the screen for the main menu items
      doDCTMenu.mainMenuItemPositions.mainMenuY = pEditTop - 1;
      // Horizontal position of the "File" text
      doDCTMenu.mainMenuItemPositions.fileX = gEditLeft + 5;
      // Horizontal position of the "Edit" text
      doDCTMenu.mainMenuItemPositions.editX = doDCTMenu.mainMenuItemPositions.fileX + 11;
      // Horizontal position of the "SysOp" text
      doDCTMenu.mainMenuItemPositions.sysopX = doDCTMenu.mainMenuItemPositions.editX + 11;
      // Horizontal position of of the "Help" text
      if (pIsSysop)
         doDCTMenu.mainMenuItemPositions.helpX = doDCTMenu.mainMenuItemPositions.sysopX + 12;
      else
         doDCTMenu.mainMenuItemPositions.helpX = doDCTMenu.mainMenuItemPositions.sysopX;
   }

   // Variables for the menu numbers
   const fileMenuNum = 0;
   const editMenuNum = 1;
   const sysopMenuNum = 2;
   const helpMenuNum = 3;

   // Set up the menu objects.  Only create these objects once.
   if (typeof(doDCTMenu.allMenus) == "undefined")
   {
      //function displayDebugText(pDebugX, pDebugY, pText, pOriginalPos, pClearDebugLineFirst, pPauseAfter)
      doDCTMenu.allMenus = new Array();
      // File menu
      doDCTMenu.allMenus[fileMenuNum] = new DCTMenu(doDCTMenu.mainMenuItemPositions.fileX, doDCTMenu.mainMenuItemPositions.mainMenuY+1);
      doDCTMenu.allMenus[fileMenuNum].addItem("&Save    Ctrl-Z", DCTMENU_FILE_SAVE);
      doDCTMenu.allMenus[fileMenuNum].addItem("&Abort   Ctrl-A", DCTMENU_FILE_ABORT);
      if (pCanCrossPost)
         doDCTMenu.allMenus[fileMenuNum].addItem( "X-Post  Ctrl-&C", DCTMENU_CROSS_POST);
      doDCTMenu.allMenus[fileMenuNum].addItem("&Edit       ESC", DCTMENU_FILE_EDIT);
      doDCTMenu.allMenus[fileMenuNum].addExitLoopKey(CTRL_Z, DCTMENU_FILE_SAVE);
      doDCTMenu.allMenus[fileMenuNum].addExitLoopKey(CTRL_A, DCTMENU_FILE_ABORT);
      if (pCanCrossPost)
         doDCTMenu.allMenus[fileMenuNum].addExitLoopKey(CTRL_C, DCTMENU_CROSS_POST);
      doDCTMenu.allMenus[fileMenuNum].addExitLoopKey(KEY_ESC, DCTMENU_FILE_EDIT);

      // Edit menu
      doDCTMenu.allMenus[editMenuNum] = new DCTMenu(doDCTMenu.mainMenuItemPositions.editX, doDCTMenu.mainMenuItemPositions.mainMenuY+1);
      doDCTMenu.allMenus[editMenuNum].addItem("&Insert Mode    Ctrl-I", DCTMENU_EDIT_INSERT_TOGGLE);
      doDCTMenu.allMenus[editMenuNum].addItem("&Find Text      Ctrl-N", DCTMENU_EDIT_FIND_TEXT);
      //doDCTMenu.allMenus[editMenuNum].addItem("Spell &Checker  Ctrl-W", DCTMENU_EDIT_SPELL_CHECKER);
      //doDCTMenu.allMenus[editMenuNum].addItem("&Setup          Ctrl-U", DCTMENU_EDIT_SETUP);
      doDCTMenu.allMenus[editMenuNum].addExitLoopKey(CTRL_I, DCTMENU_EDIT_INSERT_TOGGLE);
      doDCTMenu.allMenus[editMenuNum].addExitLoopKey(CTRL_N, DCTMENU_EDIT_FIND_TEXT);

      // SysOp menu
      doDCTMenu.allMenus[sysopMenuNum] = new DCTMenu(doDCTMenu.mainMenuItemPositions.sysopX, doDCTMenu.mainMenuItemPositions.mainMenuY+1);
      doDCTMenu.allMenus[sysopMenuNum].addItem("&Import file      Ctrl-O", DCTMENU_SYSOP_IMPORT_FILE);
      doDCTMenu.allMenus[sysopMenuNum].addItem("E&xport to file   Ctrl-X", DCTMENU_SYSOP_EXPORT_FILE);
      doDCTMenu.allMenus[sysopMenuNum].addExitLoopKey(CTRL_O, DCTMENU_SYSOP_IMPORT_FILE);
      doDCTMenu.allMenus[sysopMenuNum].addExitLoopKey(CTRL_X, DCTMENU_SYSOP_EXPORT_FILE);

      // Help menu
      doDCTMenu.allMenus[helpMenuNum] = new DCTMenu(doDCTMenu.mainMenuItemPositions.helpX, doDCTMenu.mainMenuItemPositions.mainMenuY+1);
      doDCTMenu.allMenus[helpMenuNum].addItem("C&ommand List   Ctrl-P", DCTMENU_HELP_COMMAND_LIST);
      doDCTMenu.allMenus[helpMenuNum].addItem("&General Help   Ctrl-G", DCTMENU_HELP_GENERAL);
      doDCTMenu.allMenus[helpMenuNum].addItem("&Program Info   Ctrl-R", DCTMENU_HELP_PROGRAM_INFO);
      if (gConfigSettings.enableTextReplacements)
      {
         // For some reason, Ctrl-T isn't working properly in this context - It
         // exits the help menu but doesn't exit the menu overall.  So for now,
         // I'm not showing or allowing Ctrl-T in this context.
         //doDCTMenu.allMenus[helpMenuNum].addItem("&Text replcmts  Ctrl-T", DCTMENU_LIST_TXT_REPLACEMENTS);
         //doDCTMenu.allMenus[helpMenuNum].addExitLoopKey(CTRL_T, DCTMENU_LIST_TXT_REPLACEMENTS);
         doDCTMenu.allMenus[helpMenuNum].addItem("&Text replcmts        ", DCTMENU_LIST_TXT_REPLACEMENTS);
      }
      doDCTMenu.allMenus[helpMenuNum].addExitLoopKey(CTRL_P, DCTMENU_HELP_COMMAND_LIST);
      doDCTMenu.allMenus[helpMenuNum].addExitLoopKey(CTRL_G, DCTMENU_HELP_GENERAL);
      doDCTMenu.allMenus[helpMenuNum].addExitLoopKey(CTRL_R, DCTMENU_HELP_PROGRAM_INFO);

      // For each menu, add KEY_LEFT, KEY_RIGHT, and ESC as loop-exit keys;
      // also, set the menu colors.
      for (var i = 0; i < doDCTMenu.allMenus.length; ++i)
      {
         doDCTMenu.allMenus[i].addExitLoopKey(KEY_LEFT);
         doDCTMenu.allMenus[i].addExitLoopKey(KEY_RIGHT);
         doDCTMenu.allMenus[i].addExitLoopKey(KEY_ESC);

         doDCTMenu.allMenus[i].colors.border = gConfigSettings.DCTColors.MenuBorders;
         doDCTMenu.allMenus[i].colors.selected = gConfigSettings.DCTColors.MenuSelectedItems;
         doDCTMenu.allMenus[i].colors.unselected = gConfigSettings.DCTColors.MenuUnselectedItems;
         doDCTMenu.allMenus[i].colors.hotkey = gConfigSettings.DCTColors.MenuHotkeys;
      }
   }

   // Boolean variables to keep track of which menus were displayed
   // (for refresh purposes later)
   var fileMenuDisplayed = false;
   var editMenuDisplayed = false;
   var sysopMenuDisplayed = false;
   var helpMenuDisplayed = false;

   // Display the top menu options with "File" highlighted.
   var menuNum = fileMenuNum; // 0: File, ..., 3: Help
   var subMenuItemNum = 0;
   var menuRetObj = displayTopMenuItems(doDCTMenu.mainMenuItemPositions, menuNum,
                                        doDCTMenu.allMenus, pIsSysop);
   // Input loop
   var userInput = "";
   var matchedMenuRetval = false; // Whether one of the menu return values was matched
   var continueOn = true;
   while (continueOn)
   {
      matchedMenuRetval = valMatchesMenuCode(menuRetObj.returnVal, pIsSysop);
      // If a menu return value was matched, then set userInput to it.
      if (matchedMenuRetval)
         userInput = menuRetObj.returnVal;
      // If the user's input from the last menu was ESC, left, right, or one of the
      // characters from the menus, then set userInput to the last menu keypress.
      else if (inputMatchesMenuSelection(menuRetObj.userInput, pIsSysop))
         userInput = menuRetObj.userInput;
      // If nothing from the menu was matched, then get a key from the user.
      else
         userInput = getUserKey(K_UPPER|K_NOECHO|K_NOCRLF|K_NOSPIN, gConfigSettings);
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
            if ((menuNum == sysopMenuNum) && !pIsSysop)
               --menuNum;
            subMenuItemNum = 0;
            menuRetObj = displayTopMenuItems(doDCTMenu.mainMenuItemPositions, menuNum, doDCTMenu.allMenus, pIsSysop);
            break;
         case KEY_RIGHT:
            if (menuNum == 3)
               menuNum = 0;
            else
               ++menuNum;
            // Don't allow the sysop menu for non-sysops.
            if ((menuNum == sysopMenuNum) && !pIsSysop)
               ++menuNum;
            subMenuItemNum = 0;
            menuRetObj = displayTopMenuItems(doDCTMenu.mainMenuItemPositions, menuNum, doDCTMenu.allMenus, pIsSysop);
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
         //case CTRL_T: // List text replacements
            if (gConfigSettings.enableTextReplacements)
               continueOn = false;
            break;
         default:
            break;
      }
   }

   // We've exited the menu, so refresh the top menu border and the message text
   // on the screen.
   displayTextAreaTopBorder_DCTStyle(doDCTMenu.mainMenuItemPositions.mainMenuY, pEditLeft, pEditRight);

   // Return the user's input from the menu loop.
   return userInput;
}

// This function returns whether a value matches any of the DCT Edit menu return
// values.  This is used in doDCTMenu()'s input loop;
//
// Parameters:
//  pVal: The value to test
//  pIsSysop: Whether or not the user is a sysop
//
// Return: Boolean - Whether or not the value matches a DCT Edit menu return value.
function valMatchesMenuCode(pVal, pIsSysop)
{
   var valMatches = false;
   if (pIsSysop)
   {
      valMatches = ((pVal == DCTMENU_FILE_SAVE) || (pVal == DCTMENU_FILE_ABORT) ||
                   (pVal == DCTMENU_FILE_EDIT) || (pVal == DCTMENU_EDIT_INSERT_TOGGLE) ||
                   (pVal == DCTMENU_EDIT_FIND_TEXT) || (pVal == DCTMENU_SYSOP_IMPORT_FILE) ||
                   (pVal == DCTMENU_SYSOP_EXPORT_FILE) || (pVal == DCTMENU_HELP_COMMAND_LIST) ||
                   (pVal == DCTMENU_HELP_GENERAL) || (pVal == DCTMENU_HELP_PROGRAM_INFO));
   }
   else
   {
      valMatches = ((pVal == DCTMENU_FILE_SAVE) || (pVal == DCTMENU_FILE_ABORT) ||
                   (pVal == DCTMENU_FILE_EDIT) || (pVal == DCTMENU_EDIT_INSERT_TOGGLE) ||
                   (pVal == DCTMENU_EDIT_FIND_TEXT) || (pVal == DCTMENU_HELP_COMMAND_LIST) ||
                   (pVal == DCTMENU_HELP_GENERAL) || (pVal == DCTMENU_HELP_PROGRAM_INFO));
   }
   if (gConfigSettings.enableTextReplacements)
      valMatch = (valMatches || DCTMENU_LIST_TXT_REPLACEMENTS);
   return valMatches;
}

// This function returns whether a user input matches a selection from one of the
// menus.  This is used in doDCTMenu()'s input loop;
//
// Parameters:
//  pInput: The user input to test
//  pIsSysop: Whether or not the user is a sysop
function inputMatchesMenuSelection(pInput, pIsSysop)
{
   return((pInput == KEY_ESC) || (pInput == KEY_LEFT) ||
           (pInput == KEY_RIGHT) || (pInput == KEY_ENTER) ||
           (pInput == "S") || (pInput == "A") || (pInput == "E") ||
           (pInput == "I") || (pIsSysop && (pInput == "X")) ||
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
   this.colors = new Object();
   // Unselected item colors
   this.colors.unselected = "n7k";
   // Selected item colors
   this.colors.selected = "nw";
   // Other colors
   this.colors.hotkey = "hw";
   this.colors.border = "n7k";

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
   this.menuItems = new Array();
   // hotkeyRetvals is an array, indexed by hotkey, that contains
   // the return values for each hotkey.
   this.hotkeyRetvals = new Array();

   // exitLoopKeys will contain keys that will exit the input loop
   // when pressed by the user, so that calling code can catch them.
   // It's indexed by the key, and the value is the return code that
   // should be returned for that key.
   this.exitLoopKeys = new Array();

   // Border style: "single" or "double"
   this.borderStyle = "single";

   // clearSpaceAroundMenu controls whether or not to clear one space
   // around the menu when it is drawn.
   this.clearSpaceAroundMenu = false;
   // clearSpaceColor controls which color to use when drawing the
   // clear space around the menu.
   this.clearSpaceColor = "n";
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
   if ((pReturnValue != null) && (typeof(pReturnValue) != "undefined"))
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
         console.print(this.colors.border + VERTICAL_SINGLE);
      else if (this.borderStyle == "double")
         console.print(this.colors.border + VERTICAL_DOUBLE);
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
         console.print(this.colors.border + VERTICAL_SINGLE);
      else if (this.borderStyle == "double")
         console.print(this.colors.border + VERTICAL_DOUBLE);
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
   var returnObj = new Object();
   returnObj.returnVal = -1;
   returnObj.userInput = "";

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
      var textLen = strip_ctrl(this.clearSpaceTopText).length;
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
   // Draw the top border
   var innerWidth = this.width - 2;
   if (this.borderStyle == "single")
   {
      console.print(this.colors.border + UPPER_LEFT_SINGLE);
      for (var i = 0; i < innerWidth; ++i)
         console.print(HORIZONTAL_SINGLE);
      console.print(this.colors.border + UPPER_RIGHT_SINGLE);
   }
   else if (this.borderStyle == "double")
   {
      console.print(this.colors.border + UPPER_LEFT_DOUBLE);
      for (var i = 0; i < innerWidth; ++i)
         console.print(HORIZONTAL_DOUBLE);
      console.print(this.colors.border + UPPER_RIGHT_DOUBLE);
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
      console.print(this.colors.border + LOWER_LEFT_SINGLE);
      for (var i = 0; i < innerWidth; ++i)
         console.print(HORIZONTAL_SINGLE);
      console.print(LOWER_RIGHT_SINGLE);
   }
   else if (this.borderStyle == "double")
   {
      console.print(this.colors.border + LOWER_LEFT_DOUBLE);
      for (var i = 0; i < innerWidth; ++i)
         console.print(HORIZONTAL_DOUBLE);
      console.print(LOWER_RIGHT_DOUBLE);
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
   var curpos = new Object();
   curpos.x = this.topLeftX + 1;
   curpos.y = this.topLeftY + this.selectedItemIndex + 1;

   // Input loop
   const topItemLineNumber = this.topLeftY + 1;
   const bottomItemLineNumber = this.topLeftY + this.height - 1;
   var continueOn = true;
   while (continueOn)
   {
      // Get a key, (time out after the selected time), and take appropriate action.
		returnObj.userInput = getUserKey(K_UPPER|K_NOECHO|K_NOCRLF|K_NOSPIN, gConfigSettings);
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
   this.menuItems = new Array();
   this.hotkeyRetvals = new Array();
   this.exitLoopKeys = new Array();
}

// Returns the the script's execution directory.
// The code in this function is a trick that was created by Deuce, suggested
// by Rob Swindell as a way to detect which directory the script was executed
// in.  I've shortened the code a little.
function getScriptDir()
{
   var startup_path = '.';
   try { throw dig.dist(dist); } catch(e) { startup_path = e.fileName; }
   return(backslash(startup_path.replace(/[\/\\][^\/\\]*$/,'')));
}