// $Id: SlyEdit_IceStuff.js,v 1.33 2019/08/15 04:43:33 nightfox Exp $

/* This contains IceEdit-specific functions for SlyEdit.
 *
 * Author: Eric Oulashin (AKA Nightfox)
 * BBS: Digital Distortion
 * BBS address: digdist.bbsindex.com
 *
 * Date       User              Description
 * 2009-06-06 Eric Oulashin     Started development
 * 2009-08-09 Eric Oulashin     More development & testing
 * 2009-08-22 Eric Oulashin     Version 1.00
 *                              Initial public release
 * ... Removed comments ...
 * 2019-05-04 Eric Oulashin     Updated to use require() instead of load() if possible.
 */

if (typeof(require) === "function")
{
	require("sbbsdefs.js", "K_NOCRLF");
	require(getScriptDir() + "SlyEdit_Misc.js", "UPPER_LEFT_SINGLE");
}
else
{
	load("sbbsdefs.js");
	load(getScriptDir() + "SlyEdit_Misc.js");
}

// Read the color configuration file
readColorConfig(gConfigSettings.iceColors.ThemeFilename);


///////////////////////////////////////////////////////////////////////////////////
// Functions

// This function reads the color configuration for Ice style.
//
// Parameters:
//  pFilename: The name of the color configuration file
function readColorConfig(pFilename)
{
   var colors = readValueSettingConfigFile(pFilename, 512);
   if (colors != null)
   {
      // Make a backup of the menuOptClassicColors setting so we can set it
      // back in the Ice color settings object after setting the colors.
      var useClassicColorsBackup = gConfigSettings.iceColors.menuOptClassicColors;
      gConfigSettings.iceColors = colors;
      // Move the general color settings into gConfigSettings.genColors.*
      if (EDITOR_STYLE == "ICE")
         moveGenColorsToGenSettings(gConfigSettings.iceColors, gConfigSettings);
      gConfigSettings.iceColors.menuOptClassicColors = useClassicColorsBackup;
   }
}

// Sets up any global screen-related variables needed for Ice style
function globalScreenVarsSetup_IceStyle()
{
	gSubjPos.x = 12;
	gSubjPos.y = 3;
	gSubjScreenLen = +(console.screen_columns * (54/80)).toFixed(0);
}

// Re-draws the screen, in the style of IceEdit.
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
function redrawScreen_IceStyle(pEditLeft, pEditRight, pEditTop, pEditBottom, pEditColor,
                               pInsertMode, pUseQuotes, pEditLinesIndex, pDisplayEditLines)
{
	// Top header
	// Generate & display the top border line (Note: Generate this
	// border line only once, for efficiency).
	if (typeof(redrawScreen_IceStyle.topBorder) == "undefined")
	{
      redrawScreen_IceStyle.topBorder = UPPER_LEFT_SINGLE;
      var innerWidth = console.screen_columns - 2;
      for (var i = 0; i < innerWidth; ++i)
         redrawScreen_IceStyle.topBorder += HORIZONTAL_SINGLE;
      redrawScreen_IceStyle.topBorder += UPPER_RIGHT_SINGLE;
      redrawScreen_IceStyle.topBorder = randomTwoColorString(redrawScreen_IceStyle.topBorder,
                                                             gConfigSettings.iceColors.BorderColor1,
                                                             gConfigSettings.iceColors.BorderColor2);
   }
   // Print the border line on the screen
   console.clear();
   console.print(redrawScreen_IceStyle.topBorder);

	// Next line
	// To name
	var lineNum = 2;
	console.gotoxy(1, lineNum);
	// Calculate the width of the user alias field: 28 characters, based
	// on an 80-column screen width.
	var fieldWidth = (console.screen_columns * (29/80)).toFixed(0);
	var screenText = gToName.substr(0, fieldWidth);
	console.print("\1n" + randomTwoColorString(VERTICAL_SINGLE,
	                                            gConfigSettings.iceColors.BorderColor1,
	                                            gConfigSettings.iceColors.BorderColor2) +
				  gConfigSettings.iceColors.TopInfoBkgColor + " " +
				  gConfigSettings.iceColors.TopLabelColor + "TO" +
				  gConfigSettings.iceColors.TopLabelColonColor + ": " +
				  gConfigSettings.iceColors.TopToColor + screenText);
	fieldWidth -= screenText.length;
	for (var i = 0; i < fieldWidth; ++i)
		console.print(" ");

	// From name
	fieldWidth = (console.screen_columns * (29/80)).toFixed(0);
	screenText = gFromName.substr(0, fieldWidth);
	console.print(" " + gConfigSettings.iceColors.TopLabelColor + "FROM" +
	              gConfigSettings.iceColors.TopLabelColonColor + ": " +
	              gConfigSettings.iceColors.TopFromColor + screenText);
	fieldWidth -= screenText.length;
	for (var i = 0; i < fieldWidth; ++i)
		console.print(" ");
	// More spaces until the time location
	var curpos = console.getxy();
	var startX = console.screen_columns - 8;
	while (curpos.x < startX)
	{
		console.print(" ");
		++curpos.x;
	}

	// Time
	console.print(" ");
	displayTime_IceStyle();
	console.print(" " + randomTwoColorString(VERTICAL_SINGLE, gConfigSettings.iceColors.BorderColor1,
	                                         gConfigSettings.iceColors.BorderColor2));

	// Next line: Subject, time left, insert/overwrite mode
	++lineNum;
	console.gotoxy(1, lineNum);
	// Subject
	fieldWidth = (console.screen_columns * (54/80)).toFixed(0);
	screenText = gMsgSubj.substr(0, fieldWidth);
	console.print("\1n" + randomTwoColorString(VERTICAL_SINGLE,
	                                           gConfigSettings.iceColors.BorderColor1,
	                                           gConfigSettings.iceColors.BorderColor2) +
	                                           gConfigSettings.iceColors.TopInfoBkgColor + " " +
	                                           gConfigSettings.iceColors.TopLabelColor + "SUBJECT" +
	                                           gConfigSettings.iceColors.TopLabelColonColor + ": " +
	                                           gConfigSettings.iceColors.TopSubjectColor + screenText);
	fieldWidth -= screenText.length;
	for (var i = 0; i < fieldWidth; ++i)
		console.print(" ");

	// Time left
	fieldWidth = (console.screen_columns * (4/80)).toFixed(0);
	screenText = Math.floor(bbs.time_left / 60).toString().substr(0, fieldWidth);
	startX = console.screen_columns - fieldWidth - 10;
	// Before outputting the time left, write more spaces until the starting
	// horizontal location.
	curpos = console.getxy();
	while (curpos.x < startX)
	{
		console.print(" ");
		++curpos.x;
	}
	console.print(" " + gConfigSettings.iceColors.TopLabelColor + "TL" +
	              gConfigSettings.iceColors.TopLabelColonColor + ": " +
	              gConfigSettings.iceColors.TopTimeLeftColor, screenText);
	fieldWidth -= screenText.length;
	for (var i = 0; i < fieldWidth; ++i)
		console.print(" ");

	// Insert/overwrite mode
	console.print(" " + gConfigSettings.iceColors.EditMode + pInsertMode + " n" +
	              randomTwoColorString(VERTICAL_SINGLE, gConfigSettings.iceColors.BorderColor1,
	                                                    gConfigSettings.iceColors.BorderColor2));
	
	// Next line: Top border for the message area and also includes the user #,
	// message area, and node #.
	// Generate this border line only once, for efficiency.
	if (typeof(redrawScreen_IceStyle.msgAreaBorder) == "undefined")
	{
      redrawScreen_IceStyle.msgAreaBorder = "n"
                         + randomTwoColorString(LEFT_T_SINGLE + HORIZONTAL_SINGLE,
                                                gConfigSettings.iceColors.BorderColor1,
                                                gConfigSettings.iceColors.BorderColor2)
      // User #, padded with high-black dim block characters, 5 characters for a screen
      // that's 80 characters wide.
                       + "h" + THIN_RECTANGLE_LEFT + "#k";
      fieldWidth = (console.screen_columns * (5/80)).toFixed(0) - user.number.toString().length;
      for (var i = 0; i < fieldWidth; ++i)
         redrawScreen_IceStyle.msgAreaBorder += BLOCK1;
      redrawScreen_IceStyle.msgAreaBorder += "c" + user.number
                                           + gConfigSettings.iceColors.BorderColor1
                                           + THIN_RECTANGLE_RIGHT;

      // The message area name should be centered on the line.  So, based on its
      // length (up to 35 characters), figure out its starting position before
      // printing it.
      //var msgAreaName = gMsgArea.substr(0, 20); // Used to be 20 characters
      var msgAreaName = gMsgArea.substr(0, 35);
      // 2 is subtracted from the starting position to leave room for the
      // block character and the space.
      var startPos = (console.screen_columns/2).toFixed(0) - (msgAreaName.length/2).toFixed(0) - 2;
      // Write border characters up to the message area name start position
      screenText = "";
      for (var i = strip_ctrl(redrawScreen_IceStyle.msgAreaBorder).length; i < startPos; ++i)
         screenText += HORIZONTAL_SINGLE;
      redrawScreen_IceStyle.msgAreaBorder += randomTwoColorString(screenText,
                                                             gConfigSettings.iceColors.BorderColor1,
                                                             gConfigSettings.iceColors.BorderColor2);

      // Write the message area name
      redrawScreen_IceStyle.msgAreaBorder += "h" + gConfigSettings.iceColors.BorderColor1
                  + THIN_RECTANGLE_LEFT + " " + iceText(msgAreaName, "w") + " h"
                  + gConfigSettings.iceColors.BorderColor1 + THIN_RECTANGLE_RIGHT;

      // Calculate the field width for the node number field.
      // For the node # field, use 3 characters for a screen 80 characters wide.
      fieldWidth = (console.screen_columns * (3/80)).toFixed(0);
      // Calculate the horizontal starting position for the node field.
      var nodeFieldStartPos = console.screen_columns - fieldWidth - 9;

      // Write horizontal border characters up until the point where we'll output
      // the node number.
      screenText = "";
      for (var posX = strip_ctrl(redrawScreen_IceStyle.msgAreaBorder).length; posX < nodeFieldStartPos; ++posX)
         screenText += HORIZONTAL_SINGLE;
      redrawScreen_IceStyle.msgAreaBorder += randomTwoColorString(screenText,
                                                            gConfigSettings.iceColors.BorderColor1,
                                                            gConfigSettings.iceColors.BorderColor2);

      // Output the node # field
      redrawScreen_IceStyle.msgAreaBorder += "h" + gConfigSettings.iceColors.BorderColor1
                         + THIN_RECTANGLE_LEFT + iceText("Node", "w") + "nb:hk";
      fieldWidth -= bbs.node_num.toString().length;
      for (var i = 0; i < fieldWidth; ++i)
         redrawScreen_IceStyle.msgAreaBorder += BLOCK1;
      redrawScreen_IceStyle.msgAreaBorder += "c" + bbs.node_num
                         + gConfigSettings.iceColors.BorderColor1 + THIN_RECTANGLE_RIGHT;

      // Write the last 2 characters of top border
      redrawScreen_IceStyle.msgAreaBorder += randomTwoColorString(HORIZONTAL_SINGLE + RIGHT_T_SINGLE,
                                                               gConfigSettings.iceColors.BorderColor1,
                                                               gConfigSettings.iceColors.BorderColor2);
	}
	// Draw the border line on the screen
	++lineNum;
	console.gotoxy(1, lineNum);
	console.print(redrawScreen_IceStyle.msgAreaBorder);
	
	// Display the bottom message area border and help line
	DisplayTextAreaBottomBorder_IceStyle(pEditBottom + 1, pUseQuotes);
	DisplayBottomHelpLine_IceStyle(console.screen_rows, pUseQuotes);

	// Go to the start of the edit area
	console.gotoxy(pEditLeft, pEditTop);

	// Write the message text that has been entered thus far.
	pDisplayEditLines(pEditTop, pEditLinesIndex);
	console.print(pEditColor);
}

function refreshSubjectOnScreen_IceStyle(pX, pY, pLength, pText)
{
	console.print("\1n" + gConfigSettings.iceColors.TopInfoBkgColor + gConfigSettings.iceColors.TopSubjectColor);
	console.gotoxy(pX, pY);
	printf("%-" + pLength + "s", pText.substr(0, pLength));
}

// Displays the first help line for the bottom of the screen, in the style
// of IceEdit.
//
// Parameters:
//  pLineNum: The line number on the screen where the text should be placed
//  pUseQuotes: Whether or not message quoting is enabled
//  pCanChgMsgColor: Whether or not changing the text color is allowed
// The following parameters are not used; this is here to match the function signature of DCTEdit version.
//  pEditLeft
//  pEditRight
//  pInsertMode
function DisplayTextAreaBottomBorder_IceStyle(pLineNum, pUseQuotes, pEditLeft, pEditRight,
                                               pInsertMode, pCanChgMsgColor)
{
   // The border will use random bright/normal colors.  The colors
   // should stay the same each time we draw it, so a "static"
   // variable is used for the border text.  If that variable has
   // not been defined yet, then build it.
   if (typeof(DisplayTextAreaBottomBorder_IceStyle.border) == "undefined")
   {
      // Build the string of CTRL key combinations that will be displayed
      var ctrlKeyHelp = "n" + gConfigSettings.iceColors.KeyInfoLabelColor
                      + "CTRL nhb(nwAhb)n"
                      + gConfigSettings.iceColors.KeyInfoLabelColor + "Abort";
      if (pUseQuotes)
      {
         ctrlKeyHelp += " nhb(nwQhb)n"
                      + gConfigSettings.iceColors.KeyInfoLabelColor + "Quote";
      }
      ctrlKeyHelp += " nhb(nwZhb)n"
                   + gConfigSettings.iceColors.KeyInfoLabelColor + "Save";

      // Start the border text with the first 2 border characters
      // The beginning of this line shows that SlyEdit is registered
      // to the sysop. :)
      DisplayTextAreaBottomBorder_IceStyle.border =
               randomTwoColorString(LOWER_LEFT_SINGLE + HORIZONTAL_SINGLE,
                                    gConfigSettings.iceColors.BorderColor1,
                                    gConfigSettings.iceColors.BorderColor2)
             + "h" + gConfigSettings.iceColors.BorderColor1 + THIN_RECTANGLE_LEFT
             + iceText("Registered To: " + system.operator.substr(0, 20), "w")
             + "h" + gConfigSettings.iceColors.BorderColor1 + THIN_RECTANGLE_RIGHT;
      // Append border characters up until the point we'll have to write the CTRL key
      // help text.
      var screenText = "";
      var endPos = console.screen_columns - strip_ctrl(ctrlKeyHelp).length - 3;
      var textLen = strip_ctrl(DisplayTextAreaBottomBorder_IceStyle.border).length;
      for (var i = textLen+1; i < endPos; ++i)
         screenText += HORIZONTAL_SINGLE;
      DisplayTextAreaBottomBorder_IceStyle.border += randomTwoColorString(screenText,
                                                                gConfigSettings.iceColors.BorderColor1,
                                                                gConfigSettings.iceColors.BorderColor2);

      // CTRL key help and the remaining 2 characters in the border.
      DisplayTextAreaBottomBorder_IceStyle.border += "h" + gConfigSettings.iceColors.BorderColor1
                  + THIN_RECTANGLE_LEFT + ctrlKeyHelp + gConfigSettings.iceColors.BorderColor1
                  + THIN_RECTANGLE_RIGHT
                  + randomTwoColorString(HORIZONTAL_SINGLE + LOWER_RIGHT_SINGLE,
                                         gConfigSettings.iceColors.BorderColor1,
                                         gConfigSettings.iceColors.BorderColor2);
   }

   // Display the border line on the screen
   // If pLineNum is not specified, then default to the 2nd to the last
	// line on the screen.
	var lineNum = console.screen_rows-1;
	if ((typeof(pLineNum) != "undefined") && (pLineNum != null))
		lineNum = pLineNum;
   console.gotoxy(1, lineNum);
   console.print(DisplayTextAreaBottomBorder_IceStyle.border);
}

// Displays the second (lower) help line for the bottom of the screen,
// in the style of IceEdit.
//
// Parameters:
//  pLineNum: The line number on the screen where the text should be placed
//  The following are not used and are only here to match the DCT-style function:
//   pUsingQuotes: Boolean - Whether or not message quoting is enabled.
function DisplayBottomHelpLine_IceStyle(pLineNum, pUsingQuotes)
{
	// Construct the help text only once
	if (typeof(DisplayBottomHelpLine_IceStyle.helpText) == "undefined")
	{
		// This line contains the copyright mesage & ESC key help
		var screenText = iceText(EDITOR_PROGRAM_NAME + " v", "\1w") + "\1c\1h"
		               + EDITOR_VERSION.toString() + "   "
		               + iceText("Copyright", "\1w") + " \1c\1h" + COPYRIGHT_YEAR + " "
		               + iceText("Eric Oulashin", "\1w") + " \1n\1b" + DOT_CHAR + " "
		               + iceText("Press ESCape For Help", "\1w");
		// Calculate the starting position to center the help text, and front-pad
		// DisplayBottomHelpLine_IceStyle.helpText with that many spaces.
		var xPos = (console.screen_columns / 2).toFixed(0)
		         - (strip_ctrl(screenText).length / 2).toFixed(0);
		DisplayBottomHelpLine_IceStyle.helpText = "";
		for (var i = 0; i < xPos; ++i)
			DisplayBottomHelpLine_IceStyle.helpText += " ";
		DisplayBottomHelpLine_IceStyle.helpText += screenText;
	}

	// If pLineNum is not specified, then default to the last line
	// on the screen.
	var lineNum = console.screen_rows;
	if ((typeof(pLineNum) != "undefined") && (pLineNum != null))
		lineNum = pLineNum;
	// Display the help text on the screen
	console.gotoxy(1, lineNum);
	console.print(DisplayBottomHelpLine_IceStyle.helpText);
	console.print("\1n");
	console.cleartoeol();
}

// Updates the insert mode displayd on the screen, for Ice Edit style.
//
// Parameters:
//  pEditRight: Not used; this is only here to match the signature of the DCTEdit version.
//  pEditBottom: Not used; this is only here to match the signature of the DCTEdit version.
//  pInsertMode: The insert mode ("INS" or "OVR")
function updateInsertModeOnScreen_IceStyle(pEditRight, pEditBottom, pInsertMode)
{
	console.gotoxy(console.screen_columns-4, 3);
	console.print(gConfigSettings.iceColors.TopInfoBkgColor + gConfigSettings.iceColors.EditMode
	              + pInsertMode);
}

// Draws the top border of the quote window, IceEdit style.
//
// Parameters:
//  pQuoteWinHeight: The height of the quote window
//  pEditLeft: The leftmost column of the edit area
//  pEditRight: The rightmost column of the edit area
function DrawQuoteWindowTopBorder_IceStyle(pQuoteWinHeight, pEditLeft, pEditRight)
{
   // Top border of the quote window
   // The border will use random bright/normal colors.  The colors
   // should stay the same each time we draw it, so a "static"
   // variable is used for the border text.  If that variable has
   // not been defined yet, then build it.
   if (typeof(DrawQuoteWindowTopBorder_IceStyle.border) == "undefined")
   {
      DrawQuoteWindowTopBorder_IceStyle.border = randomTwoColorString(UPPER_LEFT_VSINGLE_HDOUBLE,
                                                              gConfigSettings.iceColors.BorderColor1,
                                                              gConfigSettings.iceColors.BorderColor2)
                + gConfigSettings.iceColors.BorderColor2 + THIN_RECTANGLE_LEFT
                + gConfigSettings.iceColors.QuoteWinBorderTextColor + "Quote Window"
                + gConfigSettings.iceColors.BorderColor2
                + THIN_RECTANGLE_RIGHT;
      // The border from here to the end of the line: Random high/low blue
      var screenText = "";
      for (var posX = pEditLeft+16; posX <= pEditRight; ++posX)
         screenText += HORIZONTAL_DOUBLE;
      screenText += UPPER_RIGHT_VSINGLE_HDOUBLE;
      DrawQuoteWindowTopBorder_IceStyle.border += randomTwoColorString(screenText,
                                                           gConfigSettings.iceColors.BorderColor1,
                                                           gConfigSettings.iceColors.BorderColor2);
   }

   // Draw the border line on the screen
   console.gotoxy(pEditLeft, console.screen_rows - pQuoteWinHeight + 1);
   console.print(DrawQuoteWindowTopBorder_IceStyle.border);
}

// Draws the bottom border of the quote window, IceEdit style.  Note:
// The cursor should be placed at the start of the line (leftmost screen
// column on the screen line where the border should be drawn).
//
// Parameters:
//  pEditLeft: The leftmost column of the edit area
//  pEditRight: The rightmost column of the edit area
function DrawQuoteWindowBottomBorder_IceStyle(pEditLeft, pEditRight)
{
   // The border will use random bright/normal colors.  The colors
   // should stay the same each time we draw it, so a "static"
   // variable is used for the border text.  If that variable has
   // not been defined yet, then build it.
   if (typeof(DrawQuoteWindowBottomBorder_IceStyle.border) == "undefined")
   {
      DrawQuoteWindowBottomBorder_IceStyle.border = randomTwoColorString(LOWER_LEFT_VSINGLE_HDOUBLE,
                                                             gConfigSettings.iceColors.BorderColor1,
                                                             gConfigSettings.iceColors.BorderColor2)
                + gConfigSettings.iceColors.BorderColor2 + THIN_RECTANGLE_LEFT
                + gConfigSettings.iceColors.QuoteWinBorderTextColor + "^Q/ESC=End"
                + gConfigSettings.iceColors.BorderColor2 + THIN_RECTANGLE_RIGHT
                + gConfigSettings.iceColors.BorderColor1 + HORIZONTAL_DOUBLE
                + gConfigSettings.iceColors.BorderColor2  + THIN_RECTANGLE_LEFT
                + gConfigSettings.iceColors.QuoteWinBorderTextColor + "CR=Accept"
                + gConfigSettings.iceColors.BorderColor2 + THIN_RECTANGLE_RIGHT
                + gConfigSettings.iceColors.BorderColor1 + HORIZONTAL_DOUBLE
                + gConfigSettings.iceColors.BorderColor2 + THIN_RECTANGLE_LEFT
                + gConfigSettings.iceColors.QuoteWinBorderTextColor + "Up/Down/PgUp/PgDn=Scroll"
                + gConfigSettings.iceColors.BorderColor2 + THIN_RECTANGLE_RIGHT
                + gConfigSettings.iceColors.BorderColor1 + HORIZONTAL_DOUBLE
                + gConfigSettings.iceColors.BorderColor2 + THIN_RECTANGLE_LEFT
                + gConfigSettings.iceColors.QuoteWinBorderTextColor + "F/L=First/Last pg"
                + gConfigSettings.iceColors.BorderColor2 + THIN_RECTANGLE_RIGHT;
      // The border from here to the end of the line: Random high/low blue
      var screenText = "";
      for (var posX = pEditLeft + 73; posX <= pEditRight; ++posX)
         screenText += HORIZONTAL_DOUBLE;
      screenText += LOWER_RIGHT_VSINGLE_HDOUBLE;
      DrawQuoteWindowBottomBorder_IceStyle.border += randomTwoColorString(screenText,
                                                          gConfigSettings.iceColors.BorderColor1,
                                                          gConfigSettings.iceColors.BorderColor2);
   }

   // Draw the border line on the screen
   console.print(DrawQuoteWindowBottomBorder_IceStyle.border);
}

// Prompts the user for a yes/no question, IceEdit-style.  Note that
// the cursor should be position where it needs to be before calling
// this function.
//
// Parameters:
//  pQuestion: The question to ask, without the trailing "?"
//  pDefaultYes: Whether to default to "yes" (true/false).  If false, this
//               will default to "no".
function promptYesNo_IceStyle(pQuestion, pDefaultYes)
{
   var userResponse = pDefaultYes;

   // Print the question, and highlight "yes" or "no", depending on
   // the value of pDefaultYes.
   console.print(iceText(pQuestion + "? ", "w"));
   displayIceYesNoText(pDefaultYes);

   // yesNoX contains the horizontal position for the "Yes" & "No" text.
   const yesNoX = strip_ctrl(pQuestion).length + 3;

   // Input loop
   var userInput = "";
   var continueOn = true;
   while (continueOn)
   {
      // Move the cursor to the start of the "Yes" or "No" text (whichever
      // one is currently selected).
      console.gotoxy(userResponse ? yesNoX : yesNoX+7, console.screen_rows);
      // Get a keypress from the user and take appropriate action.
		userInput = getUserKey(K_UPPER|K_NOECHO|K_NOCRLF|K_NOSPIN, gConfigSettings);
		// If userInput is blank, then the timeout was hit, so exit the loop.
		// Also exit the loop of the user pressed enter.
		if ((userInput == "") || (userInput == KEY_ENTER))
         continueOn = false;
      else if (userInput == "Y")
      {
         userResponse = true;
         continueOn = false;
      }
      else if (userInput == "N")
      {
         userResponse = false;
         continueOn = false;
      }
      // Left or right arrow key: Toggle userResponse and update the
      // yes/no text with the appropriate colors
      else if ((userInput == KEY_LEFT) || (userInput == KEY_RIGHT))
      {
         // Move the cursor to the start of the "Yes" and "No" text, and
         // update the text depending on the value of userResponse.
         console.gotoxy(yesNoX, console.screen_rows);
         userResponse = !userResponse;
         displayIceYesNoText(userResponse);
      }
   }

   return userResponse;
}

// Displays the time on the screen.
//
// Parameters:
//  pTimeStr: The time to display (optional).  If this is omitted,
//            then this funtion will get the current time.
function displayTime_IceStyle(pTimeStr)
{
	console.gotoxy(console.screen_columns-7, 2);
	if (pTimeStr == null)
		console.print("n" + gConfigSettings.iceColors.TopInfoBkgColor + gConfigSettings.iceColors.TopTimeColor + getCurrentTimeStr());
	else
		console.print("n" + gConfigSettings.iceColors.TopInfoBkgColor + gConfigSettings.iceColors.TopTimeColor + pTimeStr);
}

// Displays the number of minutes remaining on the screen.
function displayTimeRemaining_IceStyle()
{
   var fieldWidth = (console.screen_columns * (4/80)).toFixed(0);
	var formatStr = "n" + gConfigSettings.iceColors.TopInfoBkgColor
	              + gConfigSettings.iceColors.TopTimeLeftColor + "%-"
	              + fieldWidth + "s";
	var startX = console.screen_columns - fieldWidth - 5;
	console.gotoxy(startX, 3);
	var timeStr = Math.floor(bbs.time_left / 60).toString().substr(0, fieldWidth);
	printf(formatStr, timeStr);
}

// For IceEdit mode: This function takes a string and returns a copy
// of the string with uppercase letters dim and the rest bright for
// a given color.
//
// Parameters:
//  pString: The string to convert
//  pColor: The Synchronet color code to use as the base text color.
//  pBkgColor: Optional - The background color (a Synchronet color code).
//             Defaults to normal white.
function iceText(pString, pColor, pBkgColor)
{
	// Return if an invalid string is passed in.
	if (typeof(pString) == "undefined")
		return "";
	if (pString == null)
		return "";

	// Set the color.  Default to blue.
	var color = "b";
	if ((typeof(pColor) != "undefined") && (pColor != null))
      color = pColor;

   // Set the background color.  Default to normal black.
   var bkgColor = "nk";
   if ((typeof(pBkgColor) != "undefined") && (pBkgColor != null))
      bkgColor = pBkgColor;

	// Create a copy of the string without any control characters,
	// and then add our coloring to it.
	pString = strip_ctrl(pString);
	var returnString = "n" + bkgColor + color;
	var lastColor = "n" + color;
	var character = "";
	for (var i = 0; i < pString.length; ++i)
	{
		character = pString.charAt(i);

		// Upper-case letters: Make it dim with the passed-in color.
		if (character.match(/[A-Z]/) != null)
		{
         // If the last color was not the normal color, then append
         // the normal color to returnString.
         if (lastColor != "n" + color)
            returnString += "n" + bkgColor + color;
			lastColor = "n" + color;
		}
		// Lower-case letter: Make it bright with the passed-in color.
		else if (character.match(/[a-z]/) != null)
		{
			// If this is the first character or if the last color was
			// not the bright color, then append the bright color to
			// returnString.
			if ((i == 0) || (lastColor != ("h" + color)))
            returnString += "h" + color;
			lastColor = "h" + color;
		}
		// Number: Make it bright cyan
		else if (character.match(/[0-9]/) != null)
		{
         // If the last color was not bright cyan, then append
         // bright cyan to returnString.
         if (lastColor != "hc")
            returnString += "hc";
			lastColor = "hc";
		}
		// All else: Make it bright blue
		else
		{
         // If the last color was not bright cyan, then append
         // bright cyan to returnString.
         if (lastColor != "hb")
            returnString += "hb";
			lastColor = "hb";
		}

		// Append the character from pString.
		returnString += character;
	}

	return returnString;
}

// Displays & handles the input loop for the IceEdit menu.
//
// Parameters:
//  pY: The line number on the screen for the menu
//  pCanCrossPost: Whether or not cross-posting is allowed
//
// Return value: One of the ESC_MENU values, based on the
//               user's response.
function doIceESCMenu(pY, pCanCrossPost)
{
	// The chosen action will be returned from this function
	var chosenAction = ESC_MENU_EDIT_MESSAGE;

	// IceEdit ESC menu item return values
	var ICE_ESC_MENU_SAVE = 0;
	var ICE_ESC_MENU_ABORT = 1;
	var ICE_ESC_MENU_EDIT = 2;
	var ICE_ESC_MENU_SETTINGS = 3;
	var ICE_ESC_MENU_HELP = 4;
	var ICE_ESC_MENU_SPELL_CHECK = 5;
	var ICE_ESC_MENU_CROSS_POST = 6;

	var promptText = "Select An Option:  ";

	console.gotoxy(1, pY);
	console.print(iceText(promptText, "\1w"));
	console.cleartoeol("\1n");
	// Input loop
	var lastMenuItem = (pCanCrossPost ? ICE_ESC_MENU_CROSS_POST : ICE_ESC_MENU_SPELL_CHECK);
	var userChoice = ICE_ESC_MENU_SAVE;
	var userInput;
	var continueOn = true;
	while (continueOn)
	{
		console.gotoxy(promptText.length, pY);

		// Display the options, with the correct one highlighted.
		switch (userChoice)
		{
			case ICE_ESC_MENU_SAVE:
				console.print(iceStyledPromptText("Save", true) + "\1n ");
				console.print(iceStyledPromptText("Abort", false) + "\1n ");
				console.print(iceStyledPromptText("Edit", false) + "\1n ");
				console.print(iceStyledPromptText("Settings", false) + "\1n ");
				console.print(iceStyledPromptText("Help", false) + "\1n ");
				console.print(iceStyledPromptText("sPlchk", false));
				if (pCanCrossPost)
				console.print("\1n " + iceStyledPromptText("Cross-post", false));
				break;
			case ICE_ESC_MENU_ABORT:
				console.print(iceStyledPromptText("Save", false) + "\1n ");
				console.print(iceStyledPromptText("Abort", true) + "\1n ");
				console.print(iceStyledPromptText("Edit", false) + "\1n ");
				console.print(iceStyledPromptText("Settings", false) + "\1n ");
				console.print(iceStyledPromptText("Help", false) + "\1n ");
				console.print(iceStyledPromptText("sPlchk", false));
				if (pCanCrossPost)
					console.print("\1n " + iceStyledPromptText("Cross-post", false));
				break;
			case ICE_ESC_MENU_EDIT:
				console.print(iceStyledPromptText("Save", false) + "\1n ");
				console.print(iceStyledPromptText("Abort", false) + "\1n ");
				console.print(iceStyledPromptText("Edit", true) + "\1n ");
				console.print(iceStyledPromptText("Settings", false) + "\1n ");
				console.print(iceStyledPromptText("Help", false) + "\1n ");
				console.print(iceStyledPromptText("sPlchk", false));
				if (pCanCrossPost)
					console.print("\1n " + iceStyledPromptText("Cross-post", false));
				break;
			case ICE_ESC_MENU_SETTINGS:
				console.print(iceStyledPromptText("Save", false) + "\1n ");
				console.print(iceStyledPromptText("Abort", false) + "\1n ");
				console.print(iceStyledPromptText("Edit", false) + "\1n ");
				console.print(iceStyledPromptText("Settings", true) + "\1n ");
				console.print(iceStyledPromptText("Help", false) + "\1n ");
				console.print(iceStyledPromptText("sPlchk", false));
				if (pCanCrossPost)
					console.print("\1n " + iceStyledPromptText("Cross-post", false));
				break;
			case ICE_ESC_MENU_HELP:
				console.print(iceStyledPromptText("Save", false) + "\1n ");
				console.print(iceStyledPromptText("Abort", false) + "\1n ");
				console.print(iceStyledPromptText("Edit", false) + "\1n ");
				console.print(iceStyledPromptText("Settings", false) + "\1n ");
				console.print(iceStyledPromptText("Help", true) + "\1n ");
				console.print(iceStyledPromptText("sPlchk", false));
				if (pCanCrossPost)
					console.print("\1n " + iceStyledPromptText("Cross-post", false));
				break;
			case ICE_ESC_MENU_SPELL_CHECK:
				console.print(iceStyledPromptText("Save", false) + "\1n ");
				console.print(iceStyledPromptText("Abort", false) + "\1n ");
				console.print(iceStyledPromptText("Edit", false) + "\1n ");
				console.print(iceStyledPromptText("Settings", false) + "\1n ");
				console.print(iceStyledPromptText("Help", false) + "\1n ");
				console.print(iceStyledPromptText("sPlchk", true));
				if (pCanCrossPost)
					console.print("\1n " + iceStyledPromptText("Cross-post", false));
				break;
				break;
			case ICE_ESC_MENU_CROSS_POST:
				console.print(iceStyledPromptText("Save", false) + "\1n ");
				console.print(iceStyledPromptText("Abort", false) + "\1n ");
				console.print(iceStyledPromptText("Edit", false) + "\1n ");
				console.print(iceStyledPromptText("Settings", false) + "\1n ");
				console.print(iceStyledPromptText("Help", false) + "\1n ");
				console.print(iceStyledPromptText("sPlchk", false) + "\1n ");
				console.print(iceStyledPromptText("Cross-post", true));
				break;
		}

		// Get the user's choice
		userInput = getUserKey(K_UPPER|K_NOECHO|K_NOCRLF|K_NOSPIN, gConfigSettings);
		switch (userInput)
		{
			case KEY_UP:
			case KEY_LEFT:
				--userChoice;
				if (userChoice < ICE_ESC_MENU_SAVE)
					userChoice = lastMenuItem;
				break;
			case KEY_DOWN:
			case KEY_RIGHT:
				++userChoice;
				if (userChoice > lastMenuItem)
					userChoice = ICE_ESC_MENU_SAVE;
				break;
			case "S": // Save
				userChoice = ICE_ESC_MENU_SAVE;
				continueOn = false;
				break;
			case "A": // Abort
				userChoice = ICE_ESC_MENU_ABORT;
				continueOn = false;
				break;
			case KEY_ESC: // Go back to editing the message
			case "E":     // Go back to editing the message
				userChoice = ICE_ESC_MENU_EDIT;
				continueOn = false;
				break;
			case "H": // Help
				userChoice = ICE_ESC_MENU_HELP;
				continueOn = false;
				break;
			case "C": // Cross-post
				if (pCanCrossPost)
				{
					userChoice = ICE_ESC_MENU_CROSS_POST;
					continueOn = false;
				}
				break;
			case "P": // Spell check
				userChoice = ICE_ESC_MENU_SPELL_CHECK;
				continueOn = false;
				break;
			case KEY_ENTER: // Accept the current choice
				continueOn = false;
				break;
			default:
				break;
		}
	}

	// Make sure special text attributes are cleared.
	console.print("\1n");

	// Now that the user has made a choice, set userAction
	switch (userChoice)
	{
		case ICE_ESC_MENU_SAVE:
			chosenAction = ESC_MENU_SAVE;
			break;
		case ICE_ESC_MENU_ABORT:
			chosenAction = ESC_MENU_ABORT;
			break;
		case ICE_ESC_MENU_EDIT:
			chosenAction = ESC_MENU_EDIT_MESSAGE;
			break;
		case ICE_ESC_MENU_SETTINGS:
			chosenAction = ESC_MENU_USER_SETTINGS;
			break;
		case ICE_ESC_MENU_HELP:
			chosenAction = ESC_MENU_HELP_COMMAND_LIST;
			break;
		case ICE_ESC_MENU_SPELL_CHECK:
			chosenAction = ESC_MENU_SPELL_CHECK;
			break;
		case ICE_ESC_MENU_CROSS_POST:
			chosenAction = ESC_MENU_CROSS_POST_MESSAGE;
			break;
	}

	return chosenAction;
}

// Returns text to be used in prompts, such as the ESC menu, etc.
//
// Parameters:
//  pText: The text to display
//  pHighlight: Whether or not to use highlight colors
//
// Return value: The styled text
function iceStyledPromptText(pText, pHighlight)
{
   var styledText;
   if (gConfigSettings.iceColors.menuOptClassicColors)
   {
      if (pHighlight)
         styledText = gConfigSettings.iceColors.SelectedOptionBorderColor + THIN_RECTANGLE_LEFT
                    + gConfigSettings.iceColors.SelectedOptionTextColor + pText
                    + gConfigSettings.iceColors.SelectedOptionBorderColor + THIN_RECTANGLE_RIGHT;
      else
         styledText = gConfigSettings.iceColors.UnselectedOptionBorderColor + THIN_RECTANGLE_LEFT
                    + iceText(pText, "w") + gConfigSettings.iceColors.UnselectedOptionBorderColor
                    + THIN_RECTANGLE_RIGHT;
   }
   else
   {
      if (pHighlight)
         styledText = gConfigSettings.iceColors.SelectedOptionBorderColor + THIN_RECTANGLE_LEFT
                    + "n4hy" + pText.substr(0, 1) + "c" + pText.substr(1) + "n"
                    + gConfigSettings.iceColors.SelectedOptionBorderColor + THIN_RECTANGLE_RIGHT;
      else
         styledText = gConfigSettings.iceColors.UnselectedOptionBorderColor + THIN_RECTANGLE_LEFT
                    + iceText(pText, "c") + gConfigSettings.iceColors.UnselectedOptionBorderColor
                    + THIN_RECTANGLE_RIGHT;
   }
   return styledText;
}

// Displays yes/no options in Ice style.
//
// Parameters:
//  pYesSelected: Whether or not the "yes" option is to be selected.
function displayIceYesNoText(pYesSelected)
{
   if (pYesSelected)
   {
      if (gConfigSettings.iceColors.menuOptClassicColors)
      {
         console.print(gConfigSettings.iceColors.SelectedOptionBorderColor + THIN_RECTANGLE_LEFT +
                       gConfigSettings.iceColors.SelectedOptionTextColor + "YES" +
                       gConfigSettings.iceColors.SelectedOptionBorderColor +
                       THIN_RECTANGLE_RIGHT + gConfigSettings.iceColors.UnselectedOptionBorderColor +
                       "  " + THIN_RECTANGLE_LEFT + gConfigSettings.iceColors.UnselectedOptionTextColor +
                       "NO" + gConfigSettings.iceColors.UnselectedOptionBorderColor +
                       THIN_RECTANGLE_RIGHT + "n");
      }
      else
      {
         console.print(gConfigSettings.iceColors.SelectedOptionBorderColor + THIN_RECTANGLE_LEFT +
                       "n4hyYcesn" + gConfigSettings.iceColors.SelectedOptionBorderColor +
                       THIN_RECTANGLE_RIGHT + gConfigSettings.iceColors.UnselectedOptionBorderColor +
                       "  " + THIN_RECTANGLE_LEFT + "nhcNnco" +
                       gConfigSettings.iceColors.UnselectedOptionBorderColor + THIN_RECTANGLE_RIGHT +
                       "n");
      }
   }
   else
   {
      if (gConfigSettings.iceColors.menuOptClassicColors)
      {
         console.print(gConfigSettings.iceColors.UnselectedOptionBorderColor + THIN_RECTANGLE_LEFT +
                       gConfigSettings.iceColors.UnselectedOptionTextColor + "YES" +
                       gConfigSettings.iceColors.UnselectedOptionBorderColor + THIN_RECTANGLE_RIGHT +
                       "  " + gConfigSettings.iceColors.SelectedOptionBorderColor + THIN_RECTANGLE_LEFT +
                       gConfigSettings.iceColors.SelectedOptionTextColor + "NO" +
                       gConfigSettings.iceColors.SelectedOptionBorderColor + THIN_RECTANGLE_RIGHT +
                       "n");
      }
      else
      {
         console.print(gConfigSettings.iceColors.UnselectedOptionBorderColor + THIN_RECTANGLE_LEFT +
                       "nhcYnces" + gConfigSettings.iceColors.UnselectedOptionBorderColor +
                       THIN_RECTANGLE_RIGHT + "  " + gConfigSettings.iceColors.SelectedOptionBorderColor +
                       THIN_RECTANGLE_LEFT + "n4hyNcon" +
                       gConfigSettings.iceColors.SelectedOptionBorderColor + THIN_RECTANGLE_RIGHT +
                       "n");
      }
   }
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