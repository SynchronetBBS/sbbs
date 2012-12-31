/* This file declares some general helper functions and variables
 * that are used by SlyEdit.
 *
 * Author: Eric Oulashin (AKA Nightfox)
 * BBS: Digital Distortion
 * BBS address: digdist.bbsindex.com
 *
 * Date       User              Description
 * 2009-06-06 Eric Oulashin     Started development
 * 2009-06-11 Eric Oulashin     Taking a break from development
 * 2009-08-09 Eric Oulashin     Started more development & testing
 * 2009-08-22 Eric Oulashin     Version 1.00
 *                              Initial public release
 * ....Removed some comments...
 * 2010-01-19 Eric Oulashin     Updated reAdjustTextLines() to return a boolean
 *                              to signify whether any text had been changed.
 *                              Moved isQuoteLine() to here from SlyEdit.js.
 *                              Updated isQuoteLine() to just use the isQuoteLine
 *                              property of the line and not check to see if the
 *                              line starts with a >.
 *                              Updated displayProgramInfo(): Removed my BBS
 *                              name & URL, as well as my handle.
 * 2010-02-14 Eric Oulashin     Updated reAdjustTextLines() so that it won't
 *                              assume it's splitting around a space: If a space
 *                              is not found in the line, it won't drop a
 *                              character from the line.
 * 2010-04-03 Eric Oulashin     Started working on Ctrl-A color support.
 * 2010-04-06 Eric Oulashin     Updated ReadSlyEditConfigFile() to read the
 *                              allowColorSelection value from the config file.
 * 2010-04-10 Eric Oulashin     Added toggleAttr().
 * 2012-02-17 Eric Oulashin     Added rewrapTextLines(). Changed the configuration
 *                              setting "splitLongQuoteLines" to "reWrapQuoteLines".
 * 2012-03-31 Eric Oulashin     Added the following configuration options:
 *                              add3rdPartyStartupScript
 *                              addJSOnStart
 *                              add3rdPartyExitScript
 *                              addJSOnExit
 * 2012-04-11 Eric Oulashin     Fixed a bug with quote line wrapping where it
 *                              was incorrectly dealing with quote lines that
 *                              were blank after the quote text.
 * 2012-12-21 Eric Oulashin     Updated to check for the .cfg files in the
 *                              sbbs/ctrl directory first, and if they aren't
 *                              there, assume they're in the same directory as
 *                              the .js file.
 * 2012-12-23 Eric Oulashin     Worked on updating wrapQuoteLines() and
 *                              firstNonQuoteTxtIndex() to support putting the
 *                              "To" user's initials before the > in quote lines.
 * 2012-12-25 Eric Oulashin     Updated wrapQuoteLines() to insert a > in quote
 *                              lines right after the leading quote characters
 *                              (if any), without a space afteward, to indicate
 *                              an additional level of quoting.
 * 2012-12-27 Eric Oulashin     Bug fix in wrapQuoteLines(): When prefixing
 *                              quote lines with author initials, if wrapping
 *                              resulted in additional quote lines, it wasn't
 *                              adding a > character to the additional line(s).
 * 2012-12-28 Eric Oulashin     Updated firstNonQuoteTxtIndex() to more
 *                              intelligently find the first non-quote index
 *                              when using author initials in quote lines.
 *                              I.e., dealing with multiply-quoted lines that
 *                              start like this:
 *                              > AD> 
 *                              > AD>>
 *                              etc..
 * 2012-12-30 Eric Oulashin     Added the readCurMsgNum() and
 *                              getFromNameForCurMsg() functions.
 *                              readCurMsgNum() reads DDML_SyncSMBInfo.txt,
 *                              which is written to the node directory by
 *                              the Digital Distortion Message Lister and
 *                              contains information about the current
 *                              message being read so that SlyEdit can
 *                              read it for getting the sender name from
 *                              the message header.  That was necessary
 *                              because the information about that in the
 *                              bbs object (provided by Synchronet) can't
 *                              be modified.
 */

// Note: These variables are declared with "var" instead of "const" to avoid
// multiple declaration errors when this file is loaded more than once.

// Values for attribute types (for text attribute substitution)
var FORE_ATTR = 1; // Foreground color attribute
var BKG_ATTR = 2;  // Background color attribute
var SPECIAL_ATTR = 3; // Special attribute

// Box-drawing/border characters: Single-line
var UPPER_LEFT_SINGLE = "Ú";
var HORIZONTAL_SINGLE = "Ä";
var UPPER_RIGHT_SINGLE = "¿";
var VERTICAL_SINGLE = "³";
var LOWER_LEFT_SINGLE = "À";
var LOWER_RIGHT_SINGLE = "Ù";
var T_SINGLE = "Â";
var LEFT_T_SINGLE = "Ã";
var RIGHT_T_SINGLE = "´";
var BOTTOM_T_SINGLE = "Á";
var CROSS_SINGLE = "Å";
// Box-drawing/border characters: Double-line
var UPPER_LEFT_DOUBLE = "É";
var HORIZONTAL_DOUBLE = "Í";
var UPPER_RIGHT_DOUBLE = "»";
var VERTICAL_DOUBLE = "º";
var LOWER_LEFT_DOUBLE = "È";
var LOWER_RIGHT_DOUBLE = "¼";
var T_DOUBLE = "Ë";
var LEFT_T_DOUBLE = "Ì";
var RIGHT_T_DOUBLE = "¹";
var BOTTOM_T_DOUBLE = "Ê";
var CROSS_DOUBLE = "Î";
// Box-drawing/border characters: Vertical single-line with horizontal double-line
var UPPER_LEFT_VSINGLE_HDOUBLE = "Õ";
var UPPER_RIGHT_VSINGLE_HDOUBLE = "¸";
var LOWER_LEFT_VSINGLE_HDOUBLE = "Ô";
var LOWER_RIGHT_VSINGLE_HDOUBLE = "¾";
// Other special characters
var DOT_CHAR = "ú";
var THIN_RECTANGLE_LEFT = "Ý";
var THIN_RECTANGLE_RIGHT = "Þ";
var BLOCK1 = "°"; // Dimmest block
var BLOCK2 = "±";
var BLOCK3 = "²";
var BLOCK4 = "Û"; // Brightest block

// Navigational keys
var UP_ARROW = "";
var DOWN_ARROW = "";
// CTRL keys
var CTRL_A = "\x01";
var CTRL_B = "\x02";
//var KEY_HOME = CTRL_B;
var CTRL_C = "\x03";
var CTRL_D = "\x04";
var CTRL_E = "\x05";
//var KEY_END = CTRL_E;
var CTRL_F = "\x06";
//var KEY_RIGHT = CTRL_F;
var CTRL_G = "\x07";
var BEEP = CTRL_G;
var CTRL_H = "\x08";
var BACKSPACE = CTRL_H;
var CTRL_I = "\x09";
var TAB = CTRL_I;
var CTRL_J = "\x0a";
//var KEY_DOWN = CTRL_J;
var CTRL_K = "\x0b";
var CTRL_L = "\x0c";
var INSERT_LINE = CTRL_L;
var CTRL_M = "\x0d";
var CR = CTRL_M;
var KEY_ENTER = CTRL_M;
var CTRL_N = "\x0e";
var CTRL_O = "\x0f";
var CTRL_P = "\x10";
var CTRL_Q = "\x11";
var XOFF = CTRL_Q;
var CTRL_R = "\x12";
var CTRL_S = "\x13";
var XON = CTRL_S;
var CTRL_T = "\x14";
var CTRL_U = "\x15";
var CTRL_V = "\x16";
var KEY_INSERT = CTRL_V;
var CTRL_W = "\x17";
var CTRL_X = "\x18";
var CTRL_Y = "\x19";
var CTRL_Z = "\x1a";
var KEY_ESC = "\x1b";

///////////////////////////////////////////////////////////////////////////////////
// Object/class stuff

// TextLine object constructor: This is used to keep track of a text line,
// and whether it has a hard newline at the end (i.e., if the user pressed
// enter to break the line).
//
// Parameters (all optional):
//  pText: The text for the line
//  pHardNewlineEnd: Whether or not the line has a "hard newline" - What
//                   this means is that text below it won't be wrapped up
//                   to this line when re-adjusting the text lines.
//  pIsQuoteLine: Whether or not the line is a quote line.
function TextLine(pText, pHardNewlineEnd, pIsQuoteLine)
{
	this.text = "";               // The line text
	this.hardNewlineEnd = false; // Whether or not the line has a hard newline at the end
	this.isQuoteLine = false;    // Whether or not this is a quote line
   // Copy the parameters if they are valid.
   if ((pText != null) && (typeof(pText) == "string"))
      this.text = pText;
   if ((pHardNewlineEnd != null) && (typeof(pHardNewlineEnd) == "boolean"))
      this.hardNewlineEnd = pHardNewlineEnd;
   if ((pIsQuoteLine != null) && (typeof(pIsQuoteLine) == "boolean"))
      this.isQuoteLine = pIsQuoteLine;

	// NEW & EXPERIMENTAL:
   // For color support
   this.attrs = new Array(); // An array of attributes for the line
   // Functions
   this.length = TextLine_Length;
   this.print = TextLine_Print;
}
// For the TextLine class: Returns the length of the text.
function TextLine_Length()
{
   return this.text.length;
}
// For  the TextLine class: Prints the text line, using its text attributes.
//
// Parameters:
//  pClearToEOL: Boolean - Whether or not to clear to the end of the line
function TextLine_Print(pClearToEOL)
{
   console.print(this.text);

   if (pClearToEOL)
      console.cleartoeol();
}

// AbortConfirmFuncParams constructor: This object contains parameters used by
// the abort confirmation function (actually, there are separate ones for
// IceEdit and DCT Edit styles).
function AbortConfirmFuncParams()
{
   this.editTop = gEditTop;
   this.editBottom = gEditBottom;
   this.editWidth = gEditWidth;
   this.editHeight = gEditHeight;
   this.editLinesIndex = gEditLinesIndex;
   this.displayMessageRectangle = displayMessageRectangle;
}


///////////////////////////////////////////////////////////////////////////////////
// Functions

// This function takes a string and returns a copy of the string
// with a color randomly alternating between dim & bright versions.
//
// Parameters:
//  pString: The string to convert
//  pColor: The color to use (Synchronet color code)
function randomDimBrightString(pString, pColor)
{
	// Return if an invalid string is passed in.
	if (pString == null)
		return "";
	if (typeof(pString) != "string")
		return "";

   // Set the color.  Default to green.
	var color = "g";
	if ((pColor != null) && (typeof(pColor) != "undefined"))
      color = pColor;

   return(randomTwoColorString(pString, "n" + color, "nh" + color));
}

// This function takes a string and returns a copy of the string
// with colors randomly alternating between two given colors.
//
// Parameters:
//  pString: The string to convert
//  pColor11: The first color to use (Synchronet color code)
//  pColor12: The second color to use (Synchronet color code)
function randomTwoColorString(pString, pColor1, pColor2)
{
	// Return if an invalid string is passed in.
	if (pString == null)
		return "";
	if (typeof(pString) != "string")
		return "";

	// Set the colors.  Default to green.
	var color1 = "ng";
	if ((pColor1 != null) && (typeof(pColor1) != "undefined"))
      color1 = pColor1;
   var color2 = "ngh";
	if ((pColor2 != null) && (typeof(pColor2) != "undefined"))
      color2 = pColor2;

	// Create a copy of the string without any control characters,
	// and then add our coloring to it.
	pString = strip_ctrl(pString);
	var returnString = color1;
	var useColor1 = false;     // Whether or not to use the useColor1 version of the color1
	var oldUseColor1 = useColor1; // The value of useColor1 from the last pass
	for (var i = 0; i < pString.length; ++i)
	{
		// Determine if this character should be useColor1
		useColor1 = (Math.floor(Math.random()*2) == 1);
		if (useColor1 != oldUseColor1)
         returnString += (useColor1 ? color1 : color2);

		// Append the character from pString.
		returnString += pString.charAt(i);

		oldUseColor1 = useColor1;
	}

	return returnString;
}

// Returns the current time as a string, to be displayed on the screen.
function getCurrentTimeStr()
{
	var timeStr = strftime("%I:%M%p", time());
	timeStr = timeStr.replace("AM", "a");
	timeStr = timeStr.replace("PM", "p");
	
	return timeStr;
}

// Returns whether or not a character is printable.
function isPrintableChar(pText)
{
   // Make sure pText is valid and is a string.
   if ((pText == null) || (pText == undefined))
      return false;
   if (typeof(pText) != "string")
      return false;
   if (pText.length == 0)
      return false;

   // Make sure the character is a printable ASCII character in the range of 32 to 254,
   // except for 127 (delete).
   var charCode = pText.charCodeAt(0);
   return ((charCode > 31) && (charCode < 255) && (charCode != 127));
}

// Removes multiple, leading, and/or trailing spaces
// The search & replace regular expressions used in this
// function came from the following URL:
//  http://qodo.co.uk/blog/javascript-trim-leading-and-trailing-spaces
//
// Parameters:
//  pString: The string to trim
//  pLeading: Whether or not to trim leading spaces (optional, defaults to true)
//  pMultiple: Whether or not to trim multiple spaces (optional, defaults to true)
//  pTrailing: Whether or not to trim trailing spaces (optional, defaults to true)
//
// Return value: The trimmed string
function trimSpaces(pString, pLeading, pMultiple, pTrailing)
{
   // Make sure pString is a string.
   if (typeof(pString) == "string")
   {
      var leading = true;
      var multiple = true;
      var trailing = true;
      if(typeof(pLeading) != "undefined")
         leading = pLeading;
      if(typeof(pMultiple) != "undefined")
         multiple = pMultiple;
      if(typeof(pTrailing) != "undefined")
         trailing = pTrailing;

      // To remove both leading & trailing spaces:
      //pString = pString.replace(/(^\s*)|(\s*$)/gi,"");

      if (leading)
         pString = pString.replace(/(^\s*)/gi,"");
      if (multiple)
         pString = pString.replace(/[ ]{2,}/gi," ");
      if (trailing)
         pString = pString.replace(/(\s*$)/gi,"");
   }

   return pString;
}

// Displays the text to display above help screens.
function displayHelpHeader()
{
   // Construct the header text lines only once.
   if (typeof(displayHelpHeader.headerLines) == "undefined")
   {
      displayHelpHeader.headerLines = new Array();

      var headerText = EDITOR_PROGRAM_NAME + " Help w(y"
                      + (EDITOR_STYLE == "DCT" ? "DCT" : "Ice")
                      + " modew)";
      var headerTextLen = strip_ctrl(headerText).length;

      // Top border
      var headerTextStr = "nhc" + UPPER_LEFT_SINGLE;
      for (var i = 0; i < headerTextLen + 2; ++i)
         headerTextStr += HORIZONTAL_SINGLE;
      headerTextStr += UPPER_RIGHT_SINGLE;
      displayHelpHeader.headerLines.push(headerTextStr);

      // Middle line: Header text string
      headerTextStr = VERTICAL_SINGLE + "4y " + headerText + " nhc"
                    + VERTICAL_SINGLE;
      displayHelpHeader.headerLines.push(headerTextStr);

      // Lower border
      headerTextStr = LOWER_LEFT_SINGLE;
      for (var i = 0; i < headerTextLen + 2; ++i)
         headerTextStr += HORIZONTAL_SINGLE;
      headerTextStr += LOWER_RIGHT_SINGLE;
      displayHelpHeader.headerLines.push(headerTextStr);
   }

   // Print the header strings
   for (var index in displayHelpHeader.headerLines)
      console.center(displayHelpHeader.headerLines[index]);
}

// Displays the command help.
//
// Parameters:
//  pDisplayHeader: Whether or not to display the help header.
//  pClear: Whether or not to clear the screen first
//  pPause: Whether or not to pause at the end
//  pIsSysop: Whether or not the user is the sysop.
function displayCommandList(pDisplayHeader, pClear, pPause, pIsSysop)
{
   if (pClear)
      console.clear("n");
   if (pDisplayHeader)
   {
      displayHelpHeader();
      console.crlf();
   }

   var isSysop = false;
   if (pIsSysop != null)
      isSysop = pIsSysop;
   else
      isSysop = user.compare_ars("SYSOP");

   // This function displays a key and its description with formatting & colors.
   //
   // Parameters:
   //  pKey: The key description
   //  pDesc: The description of the key's function
   //  pCR: Whether or not to display a carriage return (boolean).  Optional;
   //       if not specified, this function won't display a CR.
   function displayCmdKeyFormatted(pKey, pDesc, pCR)
   {
      printf("ch%-13sg: nc%s", pKey, pDesc);
      if (pCR)
         console.crlf();
   }
   // This function does the same, but outputs 2 on the same line.
   function displayCmdKeyFormattedDouble(pKey, pDesc, pKey2, pDesc2, pCR)
   {
      printf("ch%-13sg: nc%-28s kh" + VERTICAL_SINGLE +
             " ch%-7sg: nc%s", pKey, pDesc, pKey2, pDesc2);
      if (pCR)
         console.crlf();
   }

   // Help keys and slash commands
   printf("ng%-44s  %-33s\r\n", "Help keys", "Slash commands (on blank line)");
   printf("kh%-44s  %-33s\r\n", "ÄÄÄÄÄÄÄÄÄ", "ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ");
   displayCmdKeyFormattedDouble("Ctrl-G", "General help", "/A", "Abort", true);
   displayCmdKeyFormattedDouble("Ctrl-P", "Command key help", "/S", "Save", true);
   displayCmdKeyFormattedDouble("Ctrl-R", "Program information", "/Q", "Quote message", true);
   printf(" ch%-7sg  nc%s", "", "", "/?", "Show help");
   console.crlf();
   // Command/edit keys
   console.print("ngCommand/edit keys\r\nkhÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ\r\n");
   displayCmdKeyFormattedDouble("Ctrl-A", "Abort message", "Ctrl-W", "Page up", true);
   displayCmdKeyFormattedDouble("Ctrl-Z", "Save message", "Ctrl-S", "Page down", true);
   displayCmdKeyFormattedDouble("Ctrl-Q", "Quote message", "Ctrl-N", "Find text", true);
   displayCmdKeyFormattedDouble("Insert/Ctrl-I", "Toggle insert/overwrite mode",
                                "ESC", "Command menu", true);
   if (isSysop)
      displayCmdKeyFormattedDouble("Ctrl-O", "Import a file", "Ctrl-X", "Export to file", true);
   displayCmdKeyFormatted("Ctrl-D", "Delete line", true);

   if (pPause)
      console.pause();
}

// Displays the general help screen.
//
// Parameters:
//  pDisplayHeader: Whether or not to display the help header.
//  pClear: Whether or not to clear the screen first
//  pPause: Whether or not to pause at the end
function displayGeneralHelp(pDisplayHeader, pClear, pPause)
{
   if (pClear)
      console.clear("n");
   if (pDisplayHeader)
      displayHelpHeader();

   console.print("ncSlyEdit is a full-screen message editor that mimics the look & feel of\r\n");
   console.print("IceEdit or DCT Edit, two popular editors.  The editor is currently in " +
                 (EDITOR_STYLE == "DCT" ? "DCT" : "Ice") + "\r\nmode.\r\n");
   console.print("At the top of the screen, information about the message being written (or\r\n");
   console.print("file being edited) is displayed.  The middle section is the edit area,\r\n");
   console.print("where the message/file is edited.  Finally, the bottom section displays\r\n");
   console.print("some of the most common keys and/or status.");
   console.crlf();
   if (pPause)
      console.pause();
}

// Displays the text to display above program info screens.
function displayProgInfoHeader()
{
   // Construct the header text lines only once.
   if (typeof(displayProgInfoHeader.headerLines) == "undefined")
   {
      displayProgInfoHeader.headerLines = new Array();

      var progNameLen = strip_ctrl(EDITOR_PROGRAM_NAME).length;

      // Top border
      var headerTextStr = "nhc" + UPPER_LEFT_SINGLE;
      for (var i = 0; i < progNameLen + 2; ++i)
         headerTextStr += HORIZONTAL_SINGLE;
      headerTextStr += UPPER_RIGHT_SINGLE;
      displayProgInfoHeader.headerLines.push(headerTextStr);

      // Middle line: Header text string
      headerTextStr = VERTICAL_SINGLE + "4y " + EDITOR_PROGRAM_NAME + " nhc"
                    + VERTICAL_SINGLE;
      displayProgInfoHeader.headerLines.push(headerTextStr);

      // Lower border
      headerTextStr = LOWER_LEFT_SINGLE;
      for (var i = 0; i < progNameLen + 2; ++i)
         headerTextStr += HORIZONTAL_SINGLE;
      headerTextStr += LOWER_RIGHT_SINGLE;
      displayProgInfoHeader.headerLines.push(headerTextStr);
   }

   // Print the header strings
   for (var index in displayProgInfoHeader.headerLines)
      console.center(displayProgInfoHeader.headerLines[index]);
}

// Displays program information.
//
// Parameters:
//  pDisplayHeader: Whether or not to display the help header.
//  pClear: Whether or not to clear the screen first
//  pPause: Whether or not to pause at the end
function displayProgramInfo(pDisplayHeader, pClear, pPause)
{
   if (pClear)
      console.clear("n");
   if (pDisplayHeader)
      displayProgInfoHeader();

   // Print the program information
   console.center("ncVersion g" + EDITOR_VERSION + " wh(b" +
                  EDITOR_VER_DATE + "w)");
   console.center("ncby Eric Oulashin");
   console.crlf();
   console.print("ncSlyEdit is a full-screen message editor written for Synchronet that mimics\r\n");
   console.print("the look & feel of IceEdit or DCT Edit.");
   console.crlf();
   if (pPause)
      console.pause();
}

// Displays the informational screen for the program exit.
//
// Parameters:
//  pClearScreen: Whether or not to clear the screen.
function displayProgramExitInfo(pClearScreen)
{
	if (pClearScreen)
		console.clear("n");

   console.print("ncYou have been using:\r\n");
   console.print("hkÛ7ßßßßßßßßßßßßßßßßßßßßßßßßßßßßßßßßßß0Û\r\n");
   console.print("Û7 nb7Üßßßß Û       Ûßßßß    Û Ü       hk0Û\r\n");
   console.print("Û7 nb7ßÜÜÜ  Û Ü   Ü ÛÜÜÜ   ÜÜÛ Ü ÜÜÛÜÜ hk0Û\r\n");
   console.print("Û7     nb7Û Û Û   Û Û     Û  Û Û   Û   hk0Û\r\n");
   console.print("Û7 nb7ßßßß  ß  ßÜß  ßßßßß  ßßß ß   ßßß hk0Û\r\n");
   console.print("Û7         nb7Üß                       hk0Û\r\n");
   console.print("Û7        nb7ß                         hk0Û\r\n");
   console.print("ßßßßßßßßßßßßßßßßßßßßßßßßßßßßßßßßßßßß\r\n");
   console.print("ngVersion hy" + EDITOR_VERSION + " nm(" +
	              EDITOR_VER_DATE + ")");
   console.crlf();
	console.print("nbhby Eric Oulashin nwof chDncigital hDncistortion hBncBS");
	console.crlf();
	console.crlf();
	console.print("ncAcknowledgements for look & feel go to the following people:");
	console.crlf();
	console.print("Dan Traczynski: Creator of DCT Edit");
	console.crlf();
	console.print("Jeremy Landvoigt: Original creator of IceEdit");
	console.crlf();
}

// Writes some text on the screen at a given location with a given pause.
//
// Parameters:
//  pX: The column number on the screen at which to write the message
//  pY: The row number on the screen at which to write the message
//  pText: The text to write
//  pPauseMS: The pause time, in milliseconds
//  pClearLineAttrib: Optional - The color/attribute to clear the line with.
//                    If not specified, defaults to normal attribute.
function writeWithPause(pX, pY, pText, pPauseMS, pClearLineAttrib)
{
   var clearLineAttrib = "n";
   if ((pClearLineAttrib != null) && (typeof(pClearLineAttrib) == "string"))
      clearLineAttrib = pClearLineAttrib;
   console.gotoxy(pX, pY);
   console.cleartoeol(clearLineAttrib);
   console.print(pText);
   mswait(pPauseMS);
}

// Prompts the user for a yes/no question.
//
// Parameters:
//  pQuestion: The question to ask the user
//  pDefaultYes: Boolean - Whether or not the default should be Yes.
//               For false, the default will be No.
//  pBoxTitle: For DCT mode, this specifies the title to use for the
//             prompt box.  This is optional; if this is left out,
//             the prompt box title will default to "Prompt".
//
// Return value: Boolean - true for a "Yes" answer, false for "No"
function promptYesNo(pQuestion, pDefaultYes, pBoxTitle)
{
   var userResponse = pDefaultYes;

   if (EDITOR_STYLE == "DCT")
   {
      // We need to create an object of parameters to pass to the DCT-style
      // Yes/No function.
      var paramObj = new AbortConfirmFuncParams();
      paramObj.editLinesIndex = gEditLinesIndex;
      if (typeof(pBoxTitle) == "string")
         userResponse = promptYesNo_DCTStyle(pQuestion, pBoxTitle, pDefaultYes, paramObj);
      else
         userResponse = promptYesNo_DCTStyle(pQuestion, "Prompt", pDefaultYes, paramObj);
   }
   else if (EDITOR_STYLE == "ICE")
   {
      const originalCurpos = console.getxy();
      // Go to the bottom line on the screen and prompt the user
      console.gotoxy(1, console.screen_rows);
      console.cleartoeol();
      console.gotoxy(1, console.screen_rows);
      userResponse = promptYesNo_IceStyle(pQuestion, pDefaultYes);
      // If the user chose "No", then re-display the bottom help line and
      // move the cursor back to its original position.
      if (!userResponse)
      {
         fpDisplayBottomHelpLine(console.screen_rows, gUseQuotes);
         console.gotoxy(originalCurpos);
      }
   }

   return userResponse;
}

// Reads the SlyEdit configuration settings from SlyEdit.cfg.
//
// Return value: An object containing the settings as properties.
function ReadSlyEditConfigFile()
{
   var cfgObj = new Object(); // Configuration object

   // Default settings
   cfgObj.thirdPartyLoadOnStart = new Array();
   cfgObj.runJSOnStart = new Array();
   cfgObj.thirdPartyLoadOnExit = new Array();
   cfgObj.runJSOnExit = new Array();
   cfgObj.displayEndInfoScreen = true;
   cfgObj.userInputTimeout = true;
   cfgObj.inputTimeoutMS = 300000;
   cfgObj.reWrapQuoteLines = true;
   cfgObj.allowColorSelection = true;
   cfgObj.useQuoteLineInitials = true;
   // The next setting specifies whether or not quote lines
   // shoudl be prefixed with a space when using author
   // initials.
   cfgObj.indentQuoteLinesWithInitials = false;

   // Default Ice-style colors
   cfgObj.iceColors = new Object();
   // Ice color theme file
   cfgObj.iceColors.ThemeFilename = system.ctrl_dir + "SlyIceColors_BlueIce.cfg";
   if (!file_exists(cfgObj.iceColors.ThemeFilename))
      cfgObj.iceColors.ThemeFilename = gStartupPath + "SlyIceColors_BlueIce.cfg";
   // Text edit color
   cfgObj.iceColors.TextEditColor = "nw";
   // Quote line color
   cfgObj.iceColors.QuoteLineColor = "nc";
   // Ice colors for the quote window
   cfgObj.iceColors.QuoteWinText = "nhw";            // White
   cfgObj.iceColors.QuoteLineHighlightColor = "4hc"; // High cyan on blue background
   cfgObj.iceColors.QuoteWinBorderTextColor = "nch"; // Bright cyan
   cfgObj.iceColors.BorderColor1 = "nb";              // Blue
   cfgObj.iceColors.BorderColor2 = "nbh";          // Bright blue
   // Ice colors for multi-choice prompts
   cfgObj.iceColors.SelectedOptionBorderColor = "nbh4";
   cfgObj.iceColors.SelectedOptionTextColor = "nch4"
   cfgObj.iceColors.UnselectedOptionBorderColor = "nb";
   cfgObj.iceColors.UnselectedOptionTextColor = "nw";
   // Ice colors for the top info area
   cfgObj.iceColors.TopInfoBkgColor = "4";
   cfgObj.iceColors.TopLabelColor = "ch";
   cfgObj.iceColors.TopLabelColonColor = "bh";
   cfgObj.iceColors.TopToColor = "wh";
   cfgObj.iceColors.TopFromColor = "wh";
   cfgObj.iceColors.TopSubjectColor = "wh";
   cfgObj.iceColors.TopTimeColor = "gh";
   cfgObj.iceColors.TopTimeLeftColor = "gh";
   cfgObj.iceColors.EditMode = "ch";
   cfgObj.iceColors.KeyInfoLabelColor = "ch";

   // Default DCT-style colors
   cfgObj.DCTColors = new Object();
   // DCT color theme file
   cfgObj.DCTColors.ThemeFilename = system.ctrl_dir + "SlyDCTColors_Default.cfg";
   if (!file_exists(cfgObj.DCTColors.ThemeFilename))
      cfgObj.DCTColors.ThemeFilename = gStartupPath + "SlyDCTColors_Default.cfg";
   // Text edit color
   cfgObj.DCTColors.TextEditColor = "nw";
   // Quote line color
   cfgObj.DCTColors.QuoteLineColor = "nc";
   // DCT colors for the border stuff
   cfgObj.DCTColors.TopBorderColor1 = "nr";
   cfgObj.DCTColors.TopBorderColor2 = "nrh";
   cfgObj.DCTColors.EditAreaBorderColor1 = "ng";
   cfgObj.DCTColors.EditAreaBorderColor2 = "ngh";
   cfgObj.DCTColors.EditModeBrackets = "nkh";
   cfgObj.DCTColors.EditMode = "nw";
   // DCT colors for the top informational area
   cfgObj.DCTColors.TopLabelColor = "nbh";
   cfgObj.DCTColors.TopLabelColonColor = "nb";
   cfgObj.DCTColors.TopFromColor = "nch";
   cfgObj.DCTColors.TopFromFillColor = "nc";
   cfgObj.DCTColors.TopToColor = "nch";
   cfgObj.DCTColors.TopToFillColor = "nc";
   cfgObj.DCTColors.TopSubjColor = "nwh";
   cfgObj.DCTColors.TopSubjFillColor = "nw";
   cfgObj.DCTColors.TopAreaColor = "ngh";
   cfgObj.DCTColors.TopAreaFillColor = "ng";
   cfgObj.DCTColors.TopTimeColor = "nyh";
   cfgObj.DCTColors.TopTimeFillColor = "nr";
   cfgObj.DCTColors.TopTimeLeftColor = "nyh";
   cfgObj.DCTColors.TopTimeLeftFillColor = "nr";
   cfgObj.DCTColors.TopInfoBracketColor = "nm";
   // DCT colors for the quote window
   cfgObj.DCTColors.QuoteWinText = "n7k";
   cfgObj.DCTColors.QuoteLineHighlightColor = "nw";
   cfgObj.DCTColors.QuoteWinBorderTextColor = "n7r";
   cfgObj.DCTColors.QuoteWinBorderColor = "nk7";
   // DCT colors for the quote window
   cfgObj.DCTColors.QuoteWinText = "n7b";
   cfgObj.DCTColors.QuoteLineHighlightColor = "nw";
   cfgObj.DCTColors.QuoteWinBorderTextColor = "n7r";
   cfgObj.DCTColors.QuoteWinBorderColor = "nk7";
   // DCT colors for the bottom row help text
   cfgObj.DCTColors.BottomHelpBrackets = "nkh";
   cfgObj.DCTColors.BottomHelpKeys = "nrh";
   cfgObj.DCTColors.BottomHelpFill = "nr";
   cfgObj.DCTColors.BottomHelpKeyDesc = "nc";
   // DCT colors for text boxes
   cfgObj.DCTColors.TextBoxBorder = "nk7";
   cfgObj.DCTColors.TextBoxBorderText = "nr7";
   cfgObj.DCTColors.TextBoxInnerText = "nb7";
   cfgObj.DCTColors.YesNoBoxBrackets = "nk7";
   cfgObj.DCTColors.YesNoBoxYesNoText = "nwh7";
   // DCT colors for the menus
   cfgObj.DCTColors.SelectedMenuLabelBorders = "nw";
   cfgObj.DCTColors.SelectedMenuLabelText = "nk7";
   cfgObj.DCTColors.UnselectedMenuLabelText = "nwh";
   cfgObj.DCTColors.MenuBorders = "nk7";
   cfgObj.DCTColors.MenuSelectedItems = "nw";
   cfgObj.DCTColors.MenuUnselectedItems = "nk7";
   cfgObj.DCTColors.MenuHotkeys = "nwh7";

   // Open the configuration file
   var ctrlCfgFileName = system.ctrl_dir + "SlyEdit.cfg";
   var cfgFile = new File(file_exists(ctrlCfgFileName) ? ctrlCfgFileName : gStartupPath + "SlyEdit.cfg");
   if (cfgFile.open("r"))
   {
      var settingsMode = "behavior";
      var fileLine = null;     // A line read from the file
      var equalsPos = 0;       // Position of a = in the line
      var commentPos = 0;      // Position of the start of a comment
      var setting = null;      // A setting name (string)
      var settingUpper = null; // Upper-case setting name
      var value = null;        // A value for a setting (string)
      var valueUpper = null;   // Upper-cased value
      while (!cfgFile.eof)
      {
         // Read the next line from the config file.
         fileLine = cfgFile.readln(2048);

         // fileLine should be a string, but I've seen some cases
         // where for some reason it isn't.  If it's not a string,
         // then continue onto the next line.
         if (typeof(fileLine) != "string")
            continue;

         // If the line starts with with a semicolon (the comment
         // character) or is blank, then skip it.
         if ((fileLine.substr(0, 1) == ";") || (fileLine.length == 0))
            continue;

         // If in the "behavior" section, then set the behavior-related variables.
         if (fileLine.toUpperCase() == "[BEHAVIOR]")
         {
            settingsMode = "behavior";
            continue;
         }
         else if (fileLine.toUpperCase() == "[ICE_COLORS]")
         {
            settingsMode = "ICEColors";
            continue;
         }
         else if (fileLine.toUpperCase() == "[DCT_COLORS]")
         {
            settingsMode = "DCTColors";
            continue;
         }

         // If the line has a semicolon anywhere in it, then remove
         // everything from the semicolon onward.
         commentPos = fileLine.indexOf(";");
         if (commentPos > -1)
            fileLine = fileLine.substr(0, commentPos);

         // Look for an equals sign, and if found, separate the line
         // into the setting name (before the =) and the value (after the
         // equals sign).
         equalsPos = fileLine.indexOf("=");
         if (equalsPos > 0)
         {
            // Read the setting & value, and trim leading & trailing spaces.
            setting = trimSpaces(fileLine.substr(0, equalsPos), true, false, true);
            settingUpper = setting.toUpperCase();
            value = trimSpaces(fileLine.substr(equalsPos+1), true, false, true);
            valueUpper = value.toUpperCase();

            if (settingsMode == "behavior")
            {
               if (settingUpper == "DISPLAYENDINFOSCREEN")
                  cfgObj.displayEndInfoScreen = (valueUpper == "TRUE");
               else if (settingUpper == "USERINPUTTIMEOUT")
                  cfgObj.userInputTimeout = (valueUpper == "TRUE");
               else if (settingUpper == "INPUTTIMEOUTMS")
                  cfgObj.inputTimeoutMS = +value;
               else if (settingUpper == "REWRAPQUOTELINES")
                  cfgObj.reWrapQuoteLines = (valueUpper == "TRUE");
               else if (settingUpper == "ALLOWCOLORSELECTION")
                  cfgObj.allowColorSelection = (valueUpper == "TRUE");
               else if (settingUpper == "USEQUOTELINEINITIALS")
                  cfgObj.useQuoteLineInitials = (valueUpper == "TRUE");
               else if (settingUpper == "INDENTQUOTELINESWITHINITIALS")
                  cfgObj.indentQuoteLinesWithInitials = (valueUpper == "TRUE");
               else if (settingUpper == "ADD3RDPARTYSTARTUPSCRIPT")
                  cfgObj.thirdPartyLoadOnStart.push(value);
               else if (settingUpper == "ADD3RDPARTYEXITSCRIPT")
                  cfgObj.thirdPartyLoadOnExit.push(value);
               else if (settingUpper == "ADDJSONSTART")
                  cfgObj.runJSOnStart.push(value);
               else if (settingUpper == "ADDJSONEXIT")
                  cfgObj.runJSOnExit.push(value);
            }
            else if (settingsMode == "ICEColors")
            {
               if (settingUpper == "THEMEFILENAME")
               {
                  //system.ctrl_dir
                  //gStartupPath
                  cfgObj.iceColors.ThemeFilename = system.ctrl_dir + value;
                  if (!file_exists(cfgObj.iceColors.ThemeFilename))
                     cfgObj.iceColors.ThemeFilename = gStartupPath + value;
               }
            }
            else if (settingsMode == "DCTColors")
            {
               if (settingUpper == "THEMEFILENAME")
               {
                  cfgObj.DCTColors.ThemeFilename = system.ctrl_dir + value;
                  if (!file_exists(cfgObj.DCTColors.ThemeFilename))
                     cfgObj.DCTColors.ThemeFilename = gStartupPath + value;
               }
            }
         }
      }

      cfgFile.close();

      // Validate the settings
      if (cfgObj.inputTimeoutMS < 1000)
         cfgObj.inputTimeoutMS = 300000;
   }

   return cfgObj;
}

// This function reads a configuration file containing
// setting=value pairs and returns the settings in
// an Object.
//
// Parameters:
//  pFilename: The name of the configuration file.
//  pLineReadLen: The maximum number of characters to read from each
//                line.  This is optional; if not specified, then up
//                to 512 characters will be read from each line.
//
// Return value: An Object containing the value=setting pairs.  If the
//               file can't be opened or no settings can be read, then
//               this function will return null.
function readValueSettingConfigFile(pFilename, pLineReadLen)
{
   var retObj = null;

   var cfgFile = new File(pFilename);
   if (cfgFile.open("r"))
   {
      // Set the number of characters to read per line.
      var numCharsPerLine = 512;
      if (pLineReadLen != null)
         numCharsPerLine = pLineReadLen;

      var fileLine = null;     // A line read from the file
      var equalsPos = 0;       // Position of a = in the line
      var commentPos = 0;      // Position of the start of a comment
      var setting = null;      // A setting name (string)
      var settingUpper = null; // Upper-case setting name
      var value = null;        // A value for a setting (string)
      var valueUpper = null;   // Upper-cased value
      while (!cfgFile.eof)
      {
         // Read the next line from the config file.
         fileLine = cfgFile.readln(numCharsPerLine);

         // fileLine should be a string, but I've seen some cases
         // where it isn't, so check its type.
         if (typeof(fileLine) != "string")
            continue;

         // If the line starts with with a semicolon (the comment
         // character) or is blank, then skip it.
         if ((fileLine.substr(0, 1) == ";") || (fileLine.length == 0))
            continue;

         // If the line has a semicolon anywhere in it, then remove
         // everything from the semicolon onward.
         commentPos = fileLine.indexOf(";");
         if (commentPos > -1)
            fileLine = fileLine.substr(0, commentPos);

         // Look for an equals sign, and if found, separate the line
         // into the setting name (before the =) and the value (after the
         // equals sign).
         equalsPos = fileLine.indexOf("=");
         if (equalsPos > 0)
         {
            // If retObj hasn't been created yet, then create it.
            if (retObj == null)
               retObj = new Object();

            // Read the setting & value, and trim leading & trailing spaces.  Then
            // set the value in retObj.
            setting = trimSpaces(fileLine.substr(0, equalsPos), true, false, true);
            value = trimSpaces(fileLine.substr(equalsPos+1), true, false, true);
            retObj[setting] = value;
         }
      }

      cfgFile.close();
   }

   return retObj;
}

// Splits a string up by a maximum length, preserving whole words.
//
// Parameters:
//  pStr: The string to split
//  pMaxLen: The maximum length for the strings (strings longer than this
//           will be split)
//
// Return value: An array of strings resulting from the string split
function splitStrStable(pStr, pMaxLen)
{
   var strings = new Array();

   // Error checking
   if (typeof(pStr) != "string")
   {
      console.print("1 - pStr not a string!\r\n");
      return strings;
   }

   // If the string's length is less than or equal to pMaxLen, then
   // just insert it into the strings array.  Otherwise, we'll
   // need to split it.
   if (pStr.length <= pMaxLen)
      strings.push(pStr);
   else
   {
      // Make a copy of pStr so that we don't modify it.
      var theStr = pStr;

      var tempStr = "";
      var splitIndex = 0; // Index of a space in a string
      while (theStr.length > pMaxLen)
      {
         // If there isn't a space at the pMaxLen location in theStr,
         // then assume there's a word there and look for a space
         // before it.
         splitIndex = pMaxLen;
         if (theStr.charAt(splitIndex) != " ")
         {
            splitIndex = theStr.lastIndexOf(" ", splitIndex);
            // If a space was not found, then we should split at
            // pMaxLen.
            if (splitIndex == -1)
               splitIndex = pMaxLen;
         }

         // Extract the first part of theStr up to splitIndex into
         // tempStr, and then remove that part from theStr.
         tempStr = theStr.substr(0, splitIndex);
         theStr = theStr.substr(splitIndex+1);

         // If tempStr is not blank, then insert it into the strings
         // array.
         if (tempStr.length > 0)
            strings.push(tempStr);
      }
      // Edge case: If theStr is not blank, then insert it into the
      // strings array.
      if (theStr.length > 0)
         strings.push(theStr);
   }

   return strings;
}

// Inserts a string inside another string.
//
// Parameters:
//  pStr: The string inside which to insert the other string
//  pIndex: The index of pStr at which to insert the other string
//  pStr2: The string to insert into the first string
//
// Return value: The spliced string
function spliceIntoStr(pStr, pIndex, pStr2)
{
   // Error checking
   var typeofPStr = typeof(pStr);
   var typeofPStr2 = typeof(pStr2);
   if ((typeofPStr != "string") && (typeofPStr2 != "string"))
      return "";
   else if ((typeofPStr == "string") && (typeofPStr2 != "string"))
      return pStr;
   else if ((typeofPStr != "string") && (typeofPStr2 == "string"))
      return pStr2;
   // If pIndex is beyond the last index of pStr, then just return the
   // two strings concatenated.
   if (pIndex >= pStr.length)
      return (pStr + pStr2);
   // If pIndex is below 0, then just return pStr2 + pStr.
   else if (pIndex < 0)
      return (pStr2 + pStr);

   return (pStr.substr(0, pIndex) + pStr2 + pStr.substr(pIndex));
}

// Fixes the text lines in the gEditLines array so that they all
// have a maximum width to fit within the edit area.
//
// Parameters:
//  pTextLineArray: An array of TextLine objects to adjust
//  pStartIndex: The index of the line in the array to start at.
//  pEndIndex: One past the last index of the line in the array to end at.
//  pEditWidth: The width of the edit area (AKA the maximum line length + 1)
//
// Return value: Boolean - Whether or not any text was changed.
function reAdjustTextLines(pTextLineArray, pStartIndex, pEndIndex, pEditWidth)
{
   // Returns without doing anything if any of the parameters are not
   // what they should be. (Note: Not checking pTextLineArray for now..)
   if (typeof(pStartIndex) != "number")
      return false;
   if (typeof(pEndIndex) != "number")
      return false;
   if (typeof(pEditWidth) != "number")
      return false;
   // Range checking
   if ((pStartIndex < 0) || (pStartIndex >= pTextLineArray.length))
      return false;
   if ((pEndIndex <= pStartIndex) || (pEndIndex < 0))
      return false;
   if (pEndIndex > pTextLineArray.length)
      pEndIndex = pTextLineArray.length;
   if (pEditWidth <= 5)
      return false;

   var textChanged = false; // We'll return this upon function exit.

   var nextLineIndex = 0;
   var charsToRemove = 0;
   var splitIndex = 0;
   var spaceFound = false;      // Whether or not a space was found in a text line
   var splitIndexOriginal = 0;
   var tempText = null;
   var appendedNewLine = false; // If we appended another line
   for (var i = pStartIndex; i < pEndIndex; ++i)
   {
      // As an extra precaution, check to make sure this array element is defined.
      if (pTextLineArray[i] == undefined)
         continue;

      nextLineIndex = i + 1;
      // If the line's text is longer or equal to the edit width, then if
      // possible, move the last word to the beginning of the next line.
      if (pTextLineArray[i].text.length >= pEditWidth)
      {
         charsToRemove = pTextLineArray[i].text.length - pEditWidth + 1;
         splitIndex = pTextLineArray[i].text.length - charsToRemove;
         splitIndexOriginal = splitIndex;
         // If the character in the text line at splitIndex is not a space,
         // then look for a space before splitIndex.
         spaceFound = (pTextLineArray[i].text.charAt(splitIndex) == " ");
         if (!spaceFound)
         {
            splitIndex = pTextLineArray[i].text.lastIndexOf(" ", splitIndex-1);
            spaceFound = (splitIndex > -1);
            if (!spaceFound)
               splitIndex = splitIndexOriginal;
         }
         tempText = pTextLineArray[i].text.substr(spaceFound ? splitIndex+1 : splitIndex);
         pTextLineArray[i].text = pTextLineArray[i].text.substr(0, splitIndex);
         textChanged = true;
         // If we're on the last line, or if the current line has a hard
         // newline or is a quote line, then append a new line below.
         appendedNewLine = false;
         if ((nextLineIndex == pTextLineArray.length) || pTextLineArray[i].hardNewlineEnd ||
             isQuoteLine(pTextLineArray, i))
         {
            pTextLineArray.splice(nextLineIndex, 0, new TextLine());
            pTextLineArray[nextLineIndex].hardNewlineEnd = pTextLineArray[i].hardNewlineEnd;
            pTextLineArray[i].hardNewlineEnd = false;
            pTextLineArray[nextLineIndex].isQuoteLine = pTextLineArray[i].isQuoteLine;
            appendedNewLine = true;
         }

         // Move the text around and adjust the line properties.
         if (appendedNewLine)
            pTextLineArray[nextLineIndex].text = tempText;
         else
         {
            // If we're in insert mode, then insert the text at the beginning of
            // the next line.  Otherwise, overwrite the text in the next line.
            if (inInsertMode())
               pTextLineArray[nextLineIndex].text = tempText + " " + pTextLineArray[nextLineIndex].text;
            else
            {
               // We're in overwrite mode, so overwite the first part of the next
               // line with tempText.
               if (pTextLineArray[nextLineIndex].text.length < tempText.length)
                  pTextLineArray[nextLineIndex].text = tempText;
               else
               {
                  pTextLineArray[nextLineIndex].text = tempText
                                           + pTextLineArray[nextLineIndex].text.substr(tempText.length);
               }
            }
         }
      }
      else
      {
         // pTextLineArray[i].text.length is < pEditWidth, so try to bring up text
         // from the next line.

         // Only do it if the line doesn't have a hard newline and it's not a
         // quote line and there is a next line.
         if (!pTextLineArray[i].hardNewlineEnd && !isQuoteLine(pTextLineArray, i) &&
             (i < pTextLineArray.length-1))
         {
            if (pTextLineArray[nextLineIndex].text.length > 0)
            {
               splitIndex = pEditWidth - pTextLineArray[i].text.length - 2;
               // If splitIndex is negative, that means the entire next line
               // can fit on the current line.
               if ((splitIndex < 0) || (splitIndex > pTextLineArray[nextLineIndex].text.length))
                  splitIndex = pTextLineArray[nextLineIndex].text.length;
               else
               {
                  // If the character in the next line at splitIndex is not a
                  // space, then look for a space before it.
                  if (pTextLineArray[nextLineIndex].text.charAt(splitIndex) != " ")
                     splitIndex = pTextLineArray[nextLineIndex].text.lastIndexOf(" ", splitIndex);
                  // If no space was found, then skip to the next line (we don't
                  // want to break up words from the next line).
                  if (splitIndex == -1)
                     continue;
               }

               // Get the text to bring up to the current line.
               // If the current line does not end with a space and the next line
               // does not start with a space, then add a space between this line
               // and the next line's text.  This is done to avoid joining words
               // accidentally.
               tempText = "";
               if ((pTextLineArray[i].text.charAt(pTextLineArray[i].text.length-1) != " ") &&
                   (pTextLineArray[nextLineIndex].text.substr(0, 1) != " "))
               {
                  tempText = " ";
               }
               tempText += pTextLineArray[nextLineIndex].text.substr(0, splitIndex);
               // Move the text from the next line to the current line, if the current
               // line has room for it.
               if (pTextLineArray[i].text.length + tempText.length < pEditWidth)
               {
                  pTextLineArray[i].text += tempText;
                  pTextLineArray[nextLineIndex].text = pTextLineArray[nextLineIndex].text.substr(splitIndex+1);
                  textChanged = true;

                  // If the next line is now blank, then remove it.
                  if (pTextLineArray[nextLineIndex].text.length == 0)
                  {
                     // The current line should take on the next line's
                     // hardnewlineEnd property before removing the next line.
                     pTextLineArray[i].hardNewlineEnd = pTextLineArray[nextLineIndex].hardNewlineEnd;
                     pTextLineArray.splice(nextLineIndex, 1);
                  }
               }
            }
            else
            {
               // The next line's text string is blank.  If its hardNewlineEnd
               // property is false, then remove the line.
               if (!pTextLineArray[nextLineIndex].hardNewlineEnd)
               {
                  pTextLineArray.splice(nextLineIndex, 1);
                  textChanged = true;
               }
            }
         }
      }
   }

   return textChanged;
}

// Returns indexes of the first unquoted text line and the next
// quoted text line in an array of text lines.
//
// Parameters:
//  pTextLineArray: An array of TextLine objects
//  pStartIndex: The index of where to start looking in the array
//  pQuotePrefix: The quote line prefix (string)
//
// Return value: An object containing the following properties:
//               noQuoteLineIndex: The index of the next non-quoted line.
//                                 Will be -1 if none are found.
//               nextQuoteLineIndex: The index of the next quoted line.
//                                   Will be -1 if none are found.
function quotedLineIndexes(pTextLineArray, pStartIndex, pQuotePrefix)
{
   var retObj = new Object();
   retObj.noQuoteLineIndex = -1;
   retObj.nextQuoteLineIndex = -1;

   if (pTextLineArray.length == 0)
      return retObj;
   if (typeof(pStartIndex) != "number")
      return retObj;
   if (pStartIndex >= pTextLineArray.length)
      return retObj;

   var startIndex = (pStartIndex > -1 ? pStartIndex : 0);

   // Look for the first non-quoted line in the array.
   retObj.noQuoteLineIndex = startIndex;
   for (; retObj.noQuoteLineIndex < pTextLineArray.length; ++retObj.noQuoteLineIndex)
   {
      if (pTextLineArray[retObj.noQuoteLineIndex].text.indexOf(pQuotePrefix) == -1)
         break;
   }
   // If the index is pTextLineArray.length, then what we're looking for wasn't
   // found, so set the index to -1.
   if (retObj.noQuoteLineIndex == pTextLineArray.length)
      retObj.noQuoteLineIndex = -1;

   // Look for the next quoted line in the array.
   // If we found a non-quoted line, then use that index; otherwise,
   // start at the first line.
   if (retObj.noQuoteLineIndex > -1)
      retObj.nextQuoteLineIndex = retObj.noQuoteLineIndex;
   else
      retObj.nextQuoteLineIndex = 0;
   for (; retObj.nextQuoteLineIndex < pTextLineArray.length; ++retObj.nextQuoteLineIndex)
   {
      if (pTextLineArray[retObj.nextQuoteLineIndex].text.indexOf(pQuotePrefix) == 0)
         break;
   }
   // If the index is pTextLineArray.length, then what we're looking for wasn't
   // found, so set the index to -1.
   if (retObj.nextQuoteLineIndex == pTextLineArray.length)
      retObj.nextQuoteLineIndex = -1;

   return retObj;
}

// Returns whether a line in an array of TextLine objects is a quote line.
// This is true if the line's isQuoteLine property is true or the line's text
// starts with > (preceded by any # of spaces).
//
// Parameters:
//  pLineArray: An array of TextLine objects
//  pLineIndex: The index of the line in gEditLines
function isQuoteLine(pLineArray, pLineIndex)
{
   if (typeof(pLineArray) == "undefined")
      return false;
   if (typeof(pLineIndex) != "number")
      return false;

   var lineIsQuoteLine = false;
   if (typeof(pLineArray[pLineIndex]) != "undefined")
   {
      /*
      lineIsQuoteLine = ((pLineArray[pLineIndex].isQuoteLine) ||
                     (/^ *>/.test(pLineArray[pLineIndex].text)));
      */
      lineIsQuoteLine = (pLineArray[pLineIndex].isQuoteLine);
   }
   return lineIsQuoteLine;
}

// Replaces an attribute in a text attribute string.
//
// Parameters:
//  pAttrType: Numeric:
//             FORE_ATTR: Foreground attribute
//             BKG_ATTR: Background attribute
//             3: Special attribute
//  pAttrs: The attribute string to change
//  pNewAttr: The new attribute to put into the attribute string (without the
//            control character)
function toggleAttr(pAttrType, pAttrs, pNewAttr)
{
   // Removes an attribute from an attribute string, if it
   // exists.  Returns the new attribute string.
   function removeAttrIfExists(pAttrs, pNewAttr)
   {
      var index = pAttrs.search(pNewAttr);
      if (index > -1)
         pAttrs = pAttrs.replace(pNewAttr, "");
      return pAttrs;
   }

   // Convert pAttrs and pNewAttr to all uppercase for ease of searching
   pAttrs = pAttrs.toUpperCase();
   pNewAttr = pNewAttr.toUpperCase();

   // If pAttrs starts with the normal attribute, then
   // remove it (we'll put it back on later).
   var normalAtStart = false;
   if (pAttrs.search(/^N/) == 0)
   {
      normalAtStart = true;
      pAttrs = pAttrs.substr(2);
   }

   // Prepend the attribute control character to the new attribute
   var newAttr = "" + pNewAttr;

   // Set a regex for searching & replacing
   var regex = "";
   switch (pAttrType)
   {
      case FORE_ATTR: // Foreground attribute
         regex = /K|R|G|Y|B|M|C|W/g;
         break;
      case BKG_ATTR: // Background attribute
         regex = /0|1|2|3|4|5|6|7/g;
         break;
      case SPECIAL_ATTR: // Special attribute
         //regex = /H|I|N/g;
         index = pAttrs.search(newAttr);
         if (index > -1)
            pAttrs = pAttrs.replace(newAttr, "");
         else
            pAttrs += newAttr;
         break;
      default:
         break;
   }

   // If regex is not blank, then search & replace on it in
   // pAttrs.
   if (regex != "")
   {
      pAttrs = removeAttrIfExists(pAttrs, newAttr);
      // If the regex is found, then replace it.  Otherwise,
      // add pNewAttr to the attribute string.
      if (pAttrs.search(regex) > -1)
         pAttrs = pAttrs.replace(regex, "" + pNewAttr);
      else
         pAttrs += "" + pNewAttr;
   }

   // If pAttrs started with the normal attribute, then
   // put it back on.
   if (normalAtStart)
      pAttrs = "N" + pAttrs;

   return pAttrs;
}

// This function wraps an array of strings based on a line width.
//
// Parameters:
//  pLineArr: An array of strings
//  pStartLineIndex: The index of the text line in the array to start at
//  pStopIndex: The index of where to stop in the array.  This is one past
//              the last line in the array.  For example, to end at the
//              last line in the array, use the array's .length property
//              for this parameter.
//  pLineWidth: The maximum width of each line
//
// Return value: The number of strings in lineArr
function wrapTextLines(pLineArr, pStartLineIndex, pStopIndex, pLineWidth)
{
  // Validate parameters
  if (pLineArr == null)
    return 0;
  if ((pStartLineIndex == null) || (typeof(pStartLineIndex) != "number") || (pStartLineIndex < 0))
    pStartLineIndex = 0;
  if (pStartLineIndex >= pLineArr.length)
    return pLineArr.length;
  if ((typeof(pStopIndex) != "number") || (pStopIndex == null) || (pStopIndex > pLineArr.length))
    pStopIndex = pLineArr.length;

  // Now for the actual code:
  var trimLen = 0;   // The number of characters to trim from the end of a string
  var trimIndex = 0; // The index of where to start trimming
  for (var i = pStartLineIndex; i < pStopIndex; ++i)
  {
    // If the object in pLineArr is not a string for some reason, then skip it.
    if (typeof(pLineArr[i]) != "string")
      continue;

    if (pLineArr[i].length > pLineWidth)
    {
      trimLen = pLineArr[i].length - pLineWidth;
      trimIndex = pLineArr[i].lastIndexOf(" ", pLineArr[i].length - trimLen);
      if (trimIndex == -1)
        trimIndex = pLineArr[i].length - trimLen;
      // Trim the text, and remove leading spaces from it too.
      trimmedText = pLineArr[i].substr(trimIndex).replace(/^ +/, "");
      pLineArr[i] = pLineArr[i].substr(0, trimIndex);
      if (i < pLineArr.length - 1)
      {
        // If the next line is blank, then append another blank
        // line there to preserve the message's formatting.
        if (pLineArr[i+1].length == 0)
          pLineArr.splice(i+1, 0, "");
        else
        {
          // Since the next line is not blank, then append a space
          // to the end of the trimmed text if it doesn't have one.
          if (trimmedText.charAt(trimmedText.length-1) != " ")
            trimmedText += " "
        }
        // Prepend the trimmed text to the next line.
        pLineArr[i+1] = trimmedText + pLineArr[i+1];
      }
      else
        pLineArr.push(trimmedText);
    }
  }
  return pLineArr.length;
}

// Returns an object containing default quote string information.
//
// Return value: An object containing the following properties:
//               startIndex: The index of the first non-quote character in the string.
//                           Defaults to -1.
//               quoteLevel: The number of > characters at the start of the string
//               begOfLine: Normally, the quote text at the beginng of the line.
//                          This defaults to a blank string.
function getDefaultQuoteStrObj()
{
  var retObj = new Object();
  retObj.startIndex = -1;
  retObj.quoteLevel = 0;
  retObj.begOfLine = ""; // Will store the beginning of the line, before the >
  return retObj;
}

// Returns the index of a string for the first non-quote character.
//
// Parameters:
//  pStr: A string to check
//  pUseAuthorInitials: Whether or not SlyEdit is configured to prefix
//                      quote lines with author's initials
//
// Return value: An object containing the following properties:
//               startIndex: The index of the first non-quote character in the string.
//                           If pStr is an invalid string, or if a non-quote character
//                           is not found, this will be -1.
//               quoteLevel: The number of > characters at the start of the string
//               begOfLine: The quote text at the beginng of the line
function firstNonQuoteTxtIndex(pStr, pUseAuthorInitials, pIndentQuoteLinesWithInitials)
{
  // Create the return object with initial values.
  var retObj = getDefaultQuoteStrObj();  

  // If pStr is not a valid positive-length string, then just return.
  if ((pStr == null) || (typeof(pStr) != "string") || (pStr.length == 0))
    return retObj;

  // Look for quote lines that begin with 1 or 2 initials followed by a > (i.e.,
  // "EO>" or "E>" at the start of the line.  If found, set an index to look for
  // & count the > characters from the >.
  var searchStartIndex = 0;
  // Regex notes:
  //  \w: Matches any alphanumeric character (word characters) including underscore (short for [a-zA-Z0-9_])
  //  ?: Supposed to match 0 or 1 occurance, but seems to match 1 or 2
  // First, look for spaces then 1 or 2 initials followed by a non-space followed
  // by a >.  If not found, then look for ">>".  If that isn't found, then look
  // for just 2 characters followed by a >.
  var lineStartsWithQuoteText = /^ *\w?[^ ]>/.test(pStr);
  if (pUseAuthorInitials)
  {
    if (!lineStartsWithQuoteText)
      lineStartsWithQuoteText = (pStr.lastIndexOf(">>") > -1);
    if (!lineStartsWithQuoteText)
      lineStartsWithQuoteText = /\w{2}>/.test(pStr);
  }
  if (lineStartsWithQuoteText)
  {
    if (pUseAuthorInitials)
    {
      // First, look for ">> " starting from the beginning of the line
      // (this would be a line that has been quoted at least twice).
      // If found, then increment searchStartIndex by 2 to get past the
      // >> characters.  Otherwise, look for the last instance of 2
      // letters, numbers, or underscores (a user's handle could have
      // these characters) followed by >.  (It's possible that someone's
      // username has a number in it.)
      searchStartIndex = pStr.lastIndexOf(">> ");
      if (searchStartIndex > -1)
        searchStartIndex += 2;
      else
      {
        // If pStr is at least 3 characters long, then starting with the
        // last 3 characters in pStr, look for an instance of 2 letters
        // or numbers or underscores followed by a >.  Keep moving back
        // 1 character at a time until found or until the beginning of
        // the string is reached.
        if (pStr.length >= 3)
        {
          // Regex notes:
          //  \w: Matches any alphanumeric character (word characters) including underscore (short for [a-zA-Z0-9_])
          var substrStartIndex = pStr.length - 3;
          for (; (substrStartIndex >= 0) && (searchStartIndex < 0); --substrStartIndex)
            searchStartIndex = pStr.substr(substrStartIndex, 3).search(/\w{2}>/);
          ++substrStartIndex; // To fix off-by-one
          if (searchStartIndex > -1)
            searchStartIndex += substrStartIndex + 3; // To get past the "..>"
                                                      // Note: I originally had + 4 here..
          if (searchStartIndex < 0)
          {
            searchStartIndex = pStr.indexOf(">");
            if (searchStartIndex < 0)
              searchStartIndex = 0;
          }
        }
        else
        {
          searchStartIndex = pStr.indexOf(">");
          if (searchStartIndex < 0)
            searchStartIndex = 0;
        }
      }
    }
    else
    {
      // SlyEdit is not prefixing quote lines with author's initials.
      searchStartIndex = pStr.indexOf(">");
      if (searchStartIndex < 0)
        searchStartIndex = 0;
    }
  }

  // Find the quote level and the beginning of the line.
  // Look for the first non-quote text and quote level in the string.
  var strChar = "";
  var j = 0;
  var GTIndex = -1; // Index of a > character in the string
  for (var i = searchStartIndex; i < pStr.length; ++i)
  {
    strChar = pStr.charAt(i);
    if ((strChar != " ") && (strChar != ">"))
    {
      // We've found the first non-quote character.
      retObj.startIndex = i;
      // Count the number of times the > character appears at the start of
      // the line, and set quoteLevel to that.
      if (i >= 0)
      {
        for (j = 0; j < i; ++j)
        {
          if (pStr.charAt(j) == ">")
            ++retObj.quoteLevel;
        }
      }
      // Store the beginning of the line in retObj.begOfLine.  And if
      // SlyEdit is configured to indent quote lines with author initials,
      // and if the beginning of the line doesn't begin with a space,
      // then add a space to the beginning of it.
      retObj.begOfLine = pStr.substr(0, retObj.startIndex);
      if (pUseAuthorInitials && pIndentQuoteLinesWithInitials && (retObj.begOfLine.length > 0) && (retObj.begOfLine.charAt(0) != " "))
        retObj.begOfLine = " " + retObj.begOfLine;
      break;
    }
  }

  // If we haven't found non-quote text but the line starts with quote text,
  // then set the starting index & quote level in retObj.
  //displayDebugText(1, 2, "Search start index: " + searchStartIndex, console.getxy(), true, true);
  if (lineStartsWithQuoteText && ((retObj.startIndex == -1) || (retObj.quoteLevel == 0)))
  {
    retObj.startIndex = pStr.indexOf(">") + 1;
    retObj.quoteLevel = 1;
  }

  return retObj;
}

// Performs text wrapping on the quote lines.
//
// Parameters:
//  pUseAuthorInitials: Whether or not to prefix quote lines with the last author's
//                      initials
// pIndentQuoteLinesWithInitials: If prefixing the quote lines with the
//                                last author's initials, this parameter specifies
//                                whether or not to also prefix the quote lines with
//                                a space.
function wrapQuoteLines(pUseAuthorInitials, pIndentQuoteLinesWithInitials)
{
  if (gQuoteLines.length == 0)
    return;

  var useAuthorInitials = true;
  var indentQuoteLinesWithInitials = false;
  if (typeof(pUseAuthorInitials) != "undefined")
    useAuthorInitials = pUseAuthorInitials;
  if (typeof(pIndentQuoteLinesWithInitials) != "undefined")
    indentQuoteLinesWithInitials = pIndentQuoteLinesWithInitials;

  // This function checks if a string has only > characters separated by
  // whitespace and returns a version where the > characters are only separated
  // by one space each.  If the line starts with " >", the leading space will
  // be removed.
  function normalizeGTChars(pStr)
  {
    if (/^\s*>\s*$/.test(pStr))
      pStr = ">";
    else
    {
      pStr = pStr.replace(/>\s*>/g, "> >")
                 .replace(/^\s>/, ">")
                 .replace(/^\s*$/, "");
    }
    return pStr;
  }

  // Note: gQuotePrefix is declared in SlyEdit.js.
  // Make another copy of it without its leading space for searching the
  // quote lines later.
  var quotePrefixWithoutLeadingSpace = gQuotePrefix.replace(/^ /, "");

  // Create an array for line information objects, and append the
  // line info for all quote lines into it.  Also, store the first
  // line's quote level in the lastQuoteLevel variable.
  var lineInfos = new Array();
  for (var quoteLineIndex = 0; quoteLineIndex < gQuoteLines.length; ++quoteLineIndex)
    lineInfos.push(firstNonQuoteTxtIndex(gQuoteLines[quoteLineIndex], pUseAuthorInitials, pIndentQuoteLinesWithInitials));
  var lastQuoteLevel = lineInfos[0].quoteLevel;

  // Loop through the array starting at the 2nd line and wrap the lines
  var startArrIndex = 0;
  var endArrIndex = 0;
  var quotePrefix = "";
  var quoteLevel = 0;
  var quoteLineInfoObj = null;
  var i = 0; // Index variable
  var maxBegOfLineLen = 0; // For storing the length of the longest beginning of line that was removed
  for (var quoteLineIndex = 1; quoteLineIndex < gQuoteLines.length; ++quoteLineIndex)
  {
    quoteLineInfoObj = lineInfos[quoteLineIndex];
    if (quoteLineInfoObj.quoteLevel != lastQuoteLevel)
    {
      maxBegOfLineLen = 0;
      endArrIndex = quoteLineIndex;
      // Remove the quote strings from the lines we're about to wrap
      for (i = startArrIndex; i < endArrIndex; ++i)
      {
        // lineInfos[i] is checked for null to avoid the error "!JavaScript
        // TypeError: lineInfos[i] is undefined".  But I'm not sure why it
        // would be null..
        if (lineInfos[i] != null)
        {
          if (lineInfos[i].startIndex > -1)
            gQuoteLines[i] = gQuoteLines[i].substr(lineInfos[i].startIndex);
          else
            gQuoteLines[i] = normalizeGTChars(gQuoteLines[i]);
          // If the quote line now only consists of spaces after removing the quote
          // characters, then make it blank.
          if (/^ +$/.test(gQuoteLines[i]))
            gQuoteLines[i] = "";
          // Change multiple spaces to single spaces in the beginning-of-line
          // string.  Also, if not prefixing quote lines w/ initials with a
          // space, then also trim leading spaces.
          if (useAuthorInitials && indentQuoteLinesWithInitials)
            lineInfos[i].begOfLine = trimSpaces(lineInfos[i].begOfLine, false, true, false);
          else
            lineInfos[i].begOfLine = trimSpaces(lineInfos[i].begOfLine, true, true, false);

          // See if we need to update maxBegOfLineLen, and if so, do it.
          if (lineInfos[i].begOfLine.length > maxBegOfLineLen)
            maxBegOfLineLen = lineInfos[i].begOfLine.length;
        }
      }
      // If maxBegOfLineLen is positive, then add 1 more to it because
      // we'll be adding a > character to the quote lines to signify one
      // more level of quoting.
      if (maxBegOfLineLen > 0)
        ++maxBegOfLineLen;
      // Add gQuotePrefix's length to maxBegOfLineLen to account for that
      // for wrapping the text. Note: In future versions, if we don't want
      // to add the previous author's initials to all lines, then we might
      // not automatically want to add this to every line.
      maxBegOfLineLen += gQuotePrefix.length;

      var numLinesBefore = gQuoteLines.length;

      // Wrap the text lines in the range we've seen
      // Note: 79 is assumed as the maximum line length because that seems to
      // be a commonly-accepted message width for BBSs.  So, we need to
      // subtract the maximum "beginning of line" length from 79 and use that
      // as the wrapping length.
      // If using author initials in the quote lines, use maxBegOfLineLen as
      // the basis of where to wrap.  Otherwise (for older style without
      // author initials), calculate the width based on the quote level and
      // number of " > " strings we'll insert.
      if (useAuthorInitials)
        wrapTextLines(gQuoteLines, startArrIndex, endArrIndex, 79 - maxBegOfLineLen);
      else
        wrapTextLines(gQuoteLines, startArrIndex, endArrIndex, 79 - (2*(lastQuoteLevel+1) + gQuotePrefix.length));
      // If quote lines were added as a result of wrapping, then
      // determine the number of lines added, and update endArrIndex
      // and quoteLineIndex accordingly.
      var numLinesAdded = 0; // Will store the number of lines added after wrapping
      if (gQuoteLines.length > numLinesBefore)
      {
        numLinesAdded = gQuoteLines.length - numLinesBefore;
        endArrIndex += numLinesAdded;
        //quoteLineIndex += (numLinesAdded-1); // - 1 because quoteLineIndex will be incremented by the for loop
        quoteLineIndex += numLinesAdded;

        // Splice in a quote line info object for each new line added
        // by the wrapping process.
        var insertEndIndex = endArrIndex + numLinesAdded - 1;
        for (var insertIndex = endArrIndex-1; insertIndex < insertEndIndex; ++insertIndex)
        {
          lineInfos.splice(insertIndex, 0, getDefaultQuoteStrObj());
          lineInfos[insertIndex].startIndex = lineInfos[startArrIndex].startIndex;
          lineInfos[insertIndex].quoteLevel = lineInfos[startArrIndex].quoteLevel;
          lineInfos[insertIndex].begOfLine = lineInfos[startArrIndex].begOfLine;
        }
      }
      // Put the beginnings of the wrapped lines back on them.
      if ((quoteLineIndex > 0) && (lastQuoteLevel > 0))
      {
        // If using the author's initials in the quote lines, then
        // do it the new way.  Otherwise, do it the old way where
        // we just insert "> " back in the beginning of the quote
        // lines.
        if (useAuthorInitials)
        {
          for (i = startArrIndex; i < endArrIndex; ++i)
          {
            if (lineInfos[i] != null)
            {
              // If the beginning of the line has a non-zero length,
              // then add a > at the end to signify that this line is
              // being quoted again.
              var begOfLineLen = lineInfos[i].begOfLine.length;
              if (begOfLineLen > 0)
              {
                if (lineInfos[i].begOfLine.charAt(begOfLineLen-1) == " ")
                  lineInfos[i].begOfLine = lineInfos[i].begOfLine.substr(0, begOfLineLen-1) + "> ";
                else
                  lineInfos[i].begOfLine += ">";
              }
              // Re-assemble the quote line
              gQuoteLines[i] = lineInfos[i].begOfLine + gQuoteLines[i];
            }
            else
            {
              // Old style: Put quote strings ("> ") back into the lines
              // we just wrapped.
              quotePrefix = "";
              for (i = 0; i < lastQuoteLevel; ++i)
                quotePrefix += "> ";
              gQuoteLines[i] = quotePrefix + gQuoteLines[i].replace(/^\s*>/, ">");
            }
          }
        }
        else
        {
          // Not using author initials in the quote lines.
          // Old style: Put quote strings ("> ") back into the lines
          // we just wrapped.
          quotePrefix = "";
          for (i = 0; i < lastQuoteLevel; ++i)
            quotePrefix += "> ";
          for (i = startArrIndex; i < endArrIndex; ++i)
            gQuoteLines[i] = quotePrefix + gQuoteLines[i].replace(/^\s*>/, ">");
        }
      }

      lastQuoteLevel = quoteLineInfoObj.quoteLevel;
      // We want to go onto the next block of quote lines to wrap, so
      // set startArrIndex to the next line where we want to start wrapping.
      //startArrIndex = quoteLineIndex + numLinesAdded;
      startArrIndex = quoteLineIndex;

      if (useAuthorInitials)
      {
        // For quoting only the last author's lines: Insert gQuotePrefix
        // to the front of the quote lines.  gQuotePrefix contains the
        // last message author's initials.
        if (quoteLineInfoObj.quoteLevel == 0)
        {
          if ((gQuoteLines[i].length > 0) && (gQuoteLines[i].indexOf(gQuotePrefix) != 0) && (gQuoteLines[i].indexOf(quotePrefixWithoutLeadingSpace) != 0))
            gQuoteLines[i] = gQuotePrefix + gQuoteLines[i];
        }
      }
    }
  }

  // Wrap the last block of lines: This is the block that contains
  // (some of) the last message's author's reply to the quoted lines
  // above it.
  // Then, go through the quote lines again, and for ones that start with " >",
  // remove the leading whitespace.  This is because the quote string is " > ",
  // so it would insert an extra space before the first > in the quote line.
  // Also, if using author initials, quote the last author's lines by inserting
  // gQuotePrefix to the front of the quote lines.  gQuotePrefix contains the
  // last message author's initials.
  if (useAuthorInitials)
  {
    wrapTextLines(gQuoteLines, startArrIndex, gQuoteLines.length, 79 - gQuotePrefix.length);
    for (i = 0; i < gQuoteLines.length; ++i)
    {
      // If not prefixing the quote lines with a space, then remove leading
      // whitespace from the quote line if it starts with a >.
      if (!indentQuoteLinesWithInitials)
        gQuoteLines[i] = gQuoteLines[i].replace(/^\s*>/, ">");
      // Quote the last author's lines with gQuotePrefix
      if ((lineInfos[i] != null) && (lineInfos[i].quoteLevel == 0))
      {
        if ((gQuoteLines[i].length > 0) && (gQuoteLines[i].indexOf(gQuotePrefix) != 0) && (gQuoteLines[i].indexOf(quotePrefixWithoutLeadingSpace) != 0))
          gQuoteLines[i] = gQuotePrefix + gQuoteLines[i];
      }
    }
  }
  else
  {
    wrapTextLines(gQuoteLines, startArrIndex, gQuoteLines.length, 79 - (2*(lastQuoteLevel+1) + gQuotePrefix.length));
    for (i = 0; i < gQuoteLines.length; ++i)
      gQuoteLines[i] = gQuoteLines[i].replace(/^\s*>/, ">");
  }
}

// Returns an object containing the following values:
// - lastMsg: The last message in the sub-board (i.e., bbs.smb_last_msg)
// - totalNumMsgs: The total number of messages in the sub-board (i.e., bbs.smb_total_msgs)
// - curMsgNum: The number of the current message being read (i.e., bbs.smb_curmsg)
// - subBoardCode: The current sub-board code (i.e., bbs.smb_sub_code)
// This function First tries to read the values from the file
// DDML_SyncSMBInfo.txt in the node directory (written by the Digital
// Distortion Message Lister v1.31 and higher).  If that file can't be read,
// the values will default to the values of bbs.smb_last_msg,
// bbs.smb_total_msgs, and bbs.smb_curmsg.
function readCurMsgNum()
{
  var retObj = new Object();
  retObj.lastMsg = bbs.smb_last_msg;
  retObj.totalNumMsgs = bbs.smb_total_msgs;
  retObj.curMsgNum = bbs.smb_curmsg;
  retObj.subBoardCode = bbs.smb_sub_code;

  var SMBInfoFile = new File(system.node_dir + "DDML_SyncSMBInfo.txt");
  if (SMBInfoFile.open("r"))
  {
    var fileLine = null; // A line read from the file
    var lineNum = 0; // Will be incremented at the start of the while loop, to start at 1.
    while (!SMBInfoFile.eof)
    {
       ++lineNum;

       // Read the next line from the config file.
       fileLine = SMBInfoFile.readln(2048);

       // fileLine should be a string, but I've seen some cases
       // where for some reason it isn't.  If it's not a string,
       // then continue onto the next line.
       if (typeof(fileLine) != "string")
          continue;

        // Depending on the line number, set the appropriate value
        // in retObj.
        switch (lineNum)
        {
          case 1:
            retObj.lastMsg = +fileLine;
            break;
          case 2:
            retObj.totalNumMsgs = +fileLine;
            break;
          case 3:
            retObj.curMsgNum = +fileLine;
            break;
          case 4:
            retObj.subBoardCode = fileLine;
            break;
          default:
            break;
        }
     }
     SMBInfoFile.close();
  }

  return retObj;
}

// Gets the "From" name of the current message being replied to.
// Only for use when replying to a message in a public sub-board.
// The message information is retrieved from DDML_SyncSMBInfo.txt
// in the node dir if it exists or from the bbs object's properties.
// On error, the string returned will be blank.
function getFromNameForCurMsg()
{
  var fromName = "";

  // Get the information about the current message from
  // DDML_SyncSMBInfo.txt in the node dir if it exists or from
  // the bbs object's properties.  Then open the message header
  // and get the 'from' name from it.
  // Thanks goes to echicken (sysop of Electronic Chicken) for
  // the idea behind this code.
  var msgInfo = readCurMsgNum();
  var msgNum = msgInfo.lastMsg - msgInfo.totalNumMsgs + msgInfo.curMsgNum + 1;

  if (msgInfo.subBoardCode.length > 0)
  {
    var msgBase = new MsgBase(msgInfo.subBoardCode);
    if (msgBase != null)
    {
      msgBase.open();
      var hdr = msgBase.get_msg_header(msgNum);
      if (hdr != null)
        fromName = hdr.from;
      msgBase.close();
    }
  }

  return fromName;
}

// This function displays debug text at a given location on the screen, then
// moves the cursor back to a given location.
//
// Parameters:
//  pDebugX: The X lcoation of where to write the debug text
//  pDebugY: The Y lcoation of where to write the debug text
//  pText: The text to write at the debug location
//  pOriginalPos: An object with x and y properties containing the original cursor position
//  pClearDebugLineFirst: Whether or not to clear the debug line before writing the text
//  pPauseAfter: Whether or not to pause after displaying the text
function displayDebugText(pDebugX, pDebugY, pText, pOriginalPos, pClearDebugLineFirst, pPauseAfter)
{
	console.gotoxy(pDebugX, pDebugY);
	if (pClearDebugLineFirst)
		console.clearline();
	// Output the text
	console.print(pText);
	if (pPauseAfter)
      console.pause();
	if ((typeof(pOriginalPos) != "undefined") && (pOriginalPos != null))
		console.gotoxy(pOriginalPos);
}