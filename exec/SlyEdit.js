/* This is a text editor for Synchronet designed to mimic the look & feel of
 * DCTEdit and IceEdit, since neither of those editors have been developed
 * for quite a while and still exist only as 16-bit DOS applications.
 *
 * Author: Eric Oulashin (AKA Nightfox)
 * BBS: Digital Distortion
 * BBS address: digdist.bbsindex.com
 *
 * Date       Author            Description
 * 2009-05-11 Eric Oulashin     Started development
 * 2009-06-11 Eric Oulashin     Taking a break from development
 * 2009-08-09 Eric Oulashin     Started more development & testing
 * 2009-08-22 Eric Oulashin     Version 1.00
 *                              Initial public release
 * ....Removed some comments...
 * 2012-12-21 Eric Oulashin     Version 1.16
 *                              Updated to look for the .cfg files first in
 *                              the sbbs/ctrl directory, and if they're not
 *                              found there, assume they're in the same
 *                              directory as the .js files.
 * 2012-12-23 Eric Oulashin     Version 1.17 BETA
 *                              Started working on updating the quoting style,
 *                              to add the "To" user's initials or first 2
 *                              letters of their username to the quote prefix.
 * 2012-12-25 Eric Oulashin     When reading quote lines, if a quote line
 *                              contains only whitespace characters and/or >
 *                              characters, it will make the line blank before
 *                              adding it to gQuoteLines, to avoid weird
 *                              issues when prefixing the quote lines.
 * 2012-12-26 Eric Oulashin     Version 1.17
 *                              Updated printEditLine() to return the length of
 *                              text actually written.  Fixed a bug in
 *                              displayMessageRectangle() that was causing some
 *                              text lines not to be updated properly due to
 *                              incorrectly calculating text lengths, etc.
 * 2012-12-27 Eric Oulashin     Version 1.17
 *                              Bug fix: If the useQuoteLineInitials setting is
 *                              enabled but the reWrapQuoteLines setting was
 *                              disabled, it wasn't quoting lines; this has been
 *                              fixed.
 *                              Even though I had released v1.17 yesterday,
 *                              it appeared that nobody had downloaded it yet,
 *                              so I'm still calling this v1.17.
 * 2012-12-27 Eric Oulashin     Version 1.18 beta
 *                              Bug fix: When wrapping text lines while prefixing
 *                              the lines with initials, if wrapping a section
 *                              resulted in adding more lines, it wasn't properly
 *                              adding a > to the last line of the section to
 *                              indicate one more level of quoting.  This has
 *                              been fixed.
 * 2012-12-28 Eric Oulashin     Version 1.18 beta 3
 *                              Updated to use the new configuration setting
 *                              gConfigSettings.indentQuoteLinesWithInitials
 *                              to determine whether to prefix quote lines with
 *                              a space if using author's initials in quote lines.
 * 2012-12-30 Eric Oulashin     Version 1.18 beta 4
 *                              Updated so that when replying to a message
 *                              in a public sub-board, it will use the
 *                              message's "From" name, read from the message
 *                              database, rather than the given "To" name,
 *                              for the author's initials.  That way, if a
 *                              user changes the "To" name before replying to
 *                              a message, the correct initials will be used.
 * 2012-12-31 Eric Oulashin     Version 1.18 (final)
 *                              Releasing this version today, as testing has
 *                              shown that it's working well and as intended.
 * 2013-01-02 Eric Oulashin     Version 1.19
 *                              Fixed a bug where reading low-numbered messages
 *                              in a sub-board would result in getting the
 *                              incorrect original author name.  Updated
 *                              getFromNameForCurMsg() in SlyEdit_Misc.js
 *                              to just use the sub-board code and message
 *                              offset to get the header for the current
 *                              message being read.
 * 2013-01-13 Eric Oulashin     Version 1.20 beta
 *                              Started working on adding the ability to
 *                              cross-post to different message areas.
 *                              Started with the UI, selecting the message
 *                              group.
 * 2013-01-25 Eric Oulashin     Have still been making small changes to support
 *                              cross-posting.
 * 2013-01-31 Eric Oulashin     Version 1.20
 *                              Cross-posting & other udpates appear to be
 *                              working well and as expected, so releasing
 *                              this version now.
 * 2013-02-03 Eric Oulashin     Version 1.21
 *                              Bug fixes related to cross-posting:
 *                              - Posting during newscan: The message telling
 *                                which sub-board the message was posted in
 *                                was incorrect if different from the user's
 *                                current sub-board; this was fixed.
 *                              - Now includes the user's signature when cross-posting
 */

/* Command-line arguments:
 1 (argv[0]): Filename to read/edit
 2 (argv[1]): Editor mode ("DCT", "ICE", or "RANDOM")
*/

// Determine the location of this script (its startup directory).
// The code for figuring this out is a trick that was created by Deuce,
// suggested by Rob Swindell.  I've shortened the code a little.
var gStartupPath = '.';
try { throw dig.dist(dist); } catch(e) { gStartupPath = e.fileName; }
gStartupPath = backslash(gStartupPath.replace(/[\/\\][^\/\\]*$/,''));

// EDITOR_STYLE: Can be changed to mimic the look of DCT Edit or IceEdit.
// The following are supported:
//  "DCT": DCT Edit style
//  "ICE": IceEdit style
//  "RANDOM": Randomly choose a style
var EDITOR_STYLE = "DCT";
// The second command-line argument (argv[1]) can change this.
if (typeof(argv[1]) != "undefined")
{
	var styleUpper = argv[1].toUpperCase();
	// Make sure styleUpper is valid before setting EDITOR_STYLE.
	if (styleUpper == "DCT")
		EDITOR_STYLE = "DCT";
	else if (styleUpper == "ICE")
		EDITOR_STYLE = "ICE";
	else if (styleUpper == "RANDOM")
      EDITOR_STYLE = (Math.floor(Math.random()*2) == 0) ? "DCT" : "ICE";
}

// Load sbbsdefs.js and SlyEdit's misc. defs first
load("sbbsdefs.js");
load(gStartupPath + "SlyEdit_Misc.js");

// Load program settings from SlyEdit.cfg
var gConfigSettings = ReadSlyEditConfigFile();
// Load any specified 3rd-party startup scripts
for (var i = 0; i < gConfigSettings.thirdPartyLoadOnStart.length; ++i)
  load(gConfigSettings.thirdPartyLoadOnStart[i]);
// Execute any provided startup JavaScript commands
for (var i = 0; i < gConfigSettings.runJSOnStart.length; ++i)
  eval(gConfigSettings.runJSOnStart[i]);

const EDITOR_PROGRAM_NAME = "SlyEdit";

// This script requires Synchronet version 3.14 or higher.
// Exit if the Synchronet version is below the minimum.
if (system.version_num < 31400)
{
  console.print("n");
	console.crlf();
	console.print("nhyi* Warning:nhw " + EDITOR_PROGRAM_NAME);
	console.print(" " + "requires version g3.14w or");
	console.crlf();
	console.print("higher of Synchronet.  This BBS is using version g");
	console.print(system.version + "w.  Please notify the sysop.");
	console.crlf();
	console.pause();
	exit(1); // 1: Aborted
}
// If the user's terminal doesn't support ANSI, then exit.
if (!console.term_supports(USER_ANSI))
{
	console.print("n\r\nhyERROR: w" + EDITOR_PROGRAM_NAME +
	              " requires an ANSI terminal.n\r\np");
	exit(1); // 1: Aborted
}

// Constants
const EDITOR_VERSION = "1.21";
const EDITOR_VER_DATE = "2013-02-03";


// Program variables
var gIsSysop = user.compare_ars("SYSOP"); // Whether or not the user is a sysop
var gEditTop = 6;                         // The top line of the edit area
var gEditBottom = console.screen_rows-2;  // The last line of the edit area
// gEditLeft and gEditRight are the rightmost and leftmost columns of the edit
// area, respectively.  They default to an edit area 80 characters wide
// in the center of the screen, but for IceEdit mode, the edit area will
// be on the left side of the screen to match up with the screen header.
// gEditLeft and gEditRight are 1-based.
var gEditLeft = (console.screen_columns/2).toFixed(0) - 40 + 1;
var gEditRight = gEditLeft + 79; // Based on gEditLeft being 1-based
// If the screen has less than 80 columns, then use the whole screen.
if (console.screen_columns < 80)
{
	gEditLeft = 1;
	gEditRight = console.screen_columns;
}

// Colors
var gQuoteWinTextColor = "n7k";   // Normal text color for the quote window (DCT default)
var gQuoteLineHighlightColor = "nw"; // Highlighted text color for the quote window (DCT default)
var gTextAttrs = "nw";               // The text color for edit mode
var gQuoteLineColor = "nc";          // The text color for quote lines
var gUseTextAttribs = false;              // Will be set to true if text colors start to be used

// gQuotePrefix contains the text to prepend to quote lines.
// gQuotePrefix will later be updated to include the "To" user's
// initials or first 2 letters of their username.
var gQuotePrefix = " > ";

// gCrossPostMsgSubs will contain sub-boards for which the user wants to
// cross-post their message into.  Sub-board codes will be contained in
// objects whose name is the index to the message group in msg_area.grp_list
// to which the sub-board codes belong.
var gCrossPostMsgSubs = new Object();
// This function returns whether or not the a message sub-coard code exists in
// gCrossPostMsgSubs.
//
// Parameters:
//  pSubCode: The sub-code to look for
gCrossPostMsgSubs.subCodeExists = function(pSubCode) {
  if (typeof(pSubCode) != "string")
    return false;

  var grpIndex = msg_area.sub[pSubCode].grp_index;
  //displayDebugText(1, 3, "grpIndex: " + grpIndex, console.getxy(), true, false);
  var foundIt = false;
  if (this.hasOwnProperty(grpIndex))
    foundIt = this[grpIndex].hasOwnProperty(pSubCode);
  return foundIt;
};
// This function adds a sub-board code to gCrossPostMsgSubs.
//
// Parameters:
//  pSubCode: The sub-code to add
gCrossPostMsgSubs.add = function(pSubCode) {
  if (typeof(pSubCode) != "string")
    return;
  if (this.subCodeExists(pSubCode))
    return;

  var grpIndex = msg_area.sub[pSubCode].grp_index;
  if (!this.hasOwnProperty(grpIndex))
    this[grpIndex] = new Object();
  this[grpIndex][pSubCode] = true;
};
// This function removes a sub-board code from gCrossPostMsgSubs.
//
// Parameters:
//  pSubCode: The sub-code to remove
gCrossPostMsgSubs.remove = function(pSubCode) {
  if (typeof(pSubCode) != "string")
    return;

  var grpIndex = msg_area.sub[pSubCode].grp_index;
  if (this.hasOwnProperty(grpIndex))
  {
    delete this[grpIndex][pSubCode];
    if (numObjProperties(this[grpIndex]) == 0)
      delete this[grpIndex];
  }
};
// This function returns the number of message groups in
// gCrossPostMsgSubs.
gCrossPostMsgSubs.numMsgGrps = function() {
  var msgGrpCount = 0;
  for (var prop in this)
  {
    if ((prop == "add") || (prop == "remove") || (prop == "subCodeExists") ||
        (prop == "numMsgGrps"))
    {
      continue;
    }
    ++msgGrpCount;
  }
  return msgGrpCount;
};



// Set variables properly for the different editor styles.  Also, set up
// function pointers for functionality common to the IceEdit & DCTedit styles.
var fpDrawQuoteWindowTopBorder = null;
var fpDisplayTextAreaBottomBorder = null;
var fpDrawQuoteWindowBottomBorder = null;
var fpRedrawScreen = null;
var fpUpdateInsertModeOnScreen = null;
var fpDisplayBottomHelpLine = null;
var fpHandleESCMenu = null;
var fpDisplayTime = null;
if (EDITOR_STYLE == "DCT")
{
	gEditTop = 6;
	gQuoteWinTextColor = gConfigSettings.DCTColors.QuoteWinText;
	gQuoteLineHighlightColor = gConfigSettings.DCTColors.QuoteLineHighlightColor;
	gTextAttrs = gConfigSettings.DCTColors.TextEditColor;
	gQuoteLineColor = gConfigSettings.DCTColors.QuoteLineColor;

	// Function pointers for the DCTEdit-style screen update functions
	load(gStartupPath + "SlyEdit_DCTStuff.js");
	fpDrawQuoteWindowTopBorder = DrawQuoteWindowTopBorder_DCTStyle;
	fpDisplayTextAreaBottomBorder = DisplayTextAreaBottomBorder_DCTStyle;
	fpDrawQuoteWindowBottomBorder = DrawQuoteWindowBottomBorder_DCTStyle;
	fpRedrawScreen = redrawScreen_DCTStyle;
	fpUpdateInsertModeOnScreen = updateInsertModeOnScreen_DCTStyle;
	fpDisplayBottomHelpLine = DisplayBottomHelpLine_DCTStyle;
	fpHandleESCMenu = handleDCTESCMenu;
	fpDisplayTime = displayTime_DCTStyle;
}
else if (EDITOR_STYLE == "ICE")
{
	gEditTop = 5;
	gQuoteWinTextColor = gConfigSettings.iceColors.QuoteWinText;
	gQuoteLineHighlightColor = gConfigSettings.iceColors.QuoteLineHighlightColor;
	gTextAttrs = gConfigSettings.iceColors.TextEditColor;
	gQuoteLineColor = gConfigSettings.iceColors.QuoteLineColor;

	// Function pointers for the IceEdit-style screen update functions
	load(gStartupPath + "SlyEdit_IceStuff.js");
	fpDrawQuoteWindowTopBorder = DrawQuoteWindowTopBorder_IceStyle;
	fpDisplayTextAreaBottomBorder = DisplayTextAreaBottomBorder_IceStyle;
	fpDrawQuoteWindowBottomBorder = DrawQuoteWindowBottomBorder_IceStyle;
	fpRedrawScreen = redrawScreen_IceStyle;
	fpUpdateInsertModeOnScreen = updateInsertModeOnScreen_IceStyle;
	fpDisplayBottomHelpLine = DisplayBottomHelpLine_IceStyle;
	fpHandleESCMenu = handleIceESCMenu;
	fpDisplayTime = displayTime_IceStyle;
}

// Temporary (for testing): Make the edit area small
//gEditLeft = 25;
//gEditRight = 45;
//gEditBottom = gEditTop + 1;
// End Temporary

// Calculate the edit area width & height
const gEditWidth = gEditRight - gEditLeft + 1;
const gEditHeight = gEditBottom - gEditTop + 1;

// Message display & edit variables
var gInsertMode = "INS";       // Insert (INS) or overwrite (OVR) mode
var gQuoteLines = new Array(); // Array of quote lines loaded from file, if in quote mode
var gQuoteLinesTopIndex = 0;   // Index of the first displayed quote line
var gQuoteLinesIndex = 0;      // Index of the current quote line
// The gEditLines array will contain TextLine objects storing the line
// information.
var gEditLines = new Array();
var gEditLinesIndex = 0;      // Index into gEditLines for the line being edited
var gTextLineIndex = 0;       // Index into the current text line being edited
// Format strings used for printf() to display text in the edit area
const gFormatStr = "%-" + gEditWidth + "s";
const gFormatStrWithAttr = "%s%-" + gEditWidth + "s";

// gEditAreaBuffer will be an array of strings for the edit area, which
// will be checked by displayEditLines() before outputting text lines
// to optimize the update of message text on the screen. displayEditLines()
// will also update this array after writing a line of text to the screen.
// The indexes in this array are the absolute screen lines.
var gEditAreaBuffer = new Array();
function clearEditAreaBuffer()
{
   for (var lineNum = gEditTop; lineNum <= gEditBottom; ++lineNum)
      gEditAreaBuffer[lineNum] = "";
}
clearEditAreaBuffer();

// Set some stuff up for message editing
var gUseQuotes = true;
var gInputFilename = file_getcase(system.node_dir + "QUOTES.TXT");
if (gInputFilename == undefined)
{
	gUseQuotes = false;
	if ((argc > 0) && (gInputFilename == undefined))
		gInputFilename = argv[0];
}
else
{
	var all_files = directory(system.node_dir + "*");
	var newest_filedate = -Infinity;
	var newest_filename;
	for (var file in all_files)
	{
		if (all_files[file].search(/quotes.txt$/i) != -1)
		{
			var this_date = file_date(all_files[file]);
			if (this_date > newest_filedate)
			{
				newest_filename = all_files[file];
				newest_filedate = this_date;
			}
		}
	}
	if (newest_filename != undefined)
		gInputFilename = newest_filename;
}

var gOldStatus = bbs.sys_status;
bbs.sys_status &=~SS_PAUSEON;
bbs.sys_status |= SS_PAUSEOFF;
var gOldPassthru = console.ctrlkey_passthru;
console.ctrlkey_passthru = "+ACGKLOPQRTUVWXYZ_";
// Enable delete line in SyncTERM (Disabling ANSI Music in the process)
console.write("\033[=1M");
console.clear();

// Read the message from name, to name, and subject from the drop file
// (msginf in the node directory).
var gMsgAreaInfo = getCurMsgInfo(); // Contains info about the current sub-board
var gMsgSubj = "";
var gFromName = user.alias;
var gToName = gInputFilename;
var gMsgArea = "";
var dropFileTime = -Infinity;
var dropFileName = file_getcase(system.node_dir + "msginf");
if (dropFileName != undefined)
{
	if (file_date(dropFileName) >= dropFileTime)
	{
		var dropFile = new File(dropFileName);
		if (dropFile.exists && dropFile.open("r"))
		{
			dropFileTime = dropFile.date;
			info = dropFile.readAll();
			dropFile.close();

			gFromName = info[0];
			gToName = info[1];
			gMsgSubj = info[2];
			gMsgArea = info[4];

      if (gConfigSettings.useQuoteLineInitials)
      {
        // For the name to use for the initials, use the
        // current message's "from" name if there is one,
        // or the "To" name read from the message info
        // drop file.
        // Remove any leading, multiple, or trailing spaces
        // that it might have.
        var quotedName = trimSpaces(getFromNameForCurMsg(gMsgAreaInfo), true, true, true);
        if (quotedName.length == 0)
          quotedName = trimSpaces(gToName, true, true, true);
        // If configured to indent quote lines w/ initials with
        // a space, then do it.
        gQuotePrefix = "";
        if (gConfigSettings.indentQuoteLinesWithInitials)
          gQuotePrefix = " ";
				// Use the initials or first 2 characters from the
				// quoted name for gQuotePrefix.
				var spaceIndex = quotedName.indexOf(" ");
				if (spaceIndex > -1) // If a space exists, use the initials
				{
          gQuotePrefix += quotedName.charAt(0).toUpperCase();
					if (quotedName.length > spaceIndex+1)
						gQuotePrefix += quotedName.charAt(spaceIndex+1).toUpperCase();
					gQuotePrefix += "> ";
				}
				else // A space doesn't exist; use the first 2 letters
					gQuotePrefix += quotedName.substr(0, 2) + "> ";
      }
		}
	}
	file_remove(dropFileName);
}
// If the user is posting in a message sub-board, then add its information
// to gCrossPostMsgSubs.
if (postingInMsgSubBoard(gMsgArea))
  gCrossPostMsgSubs.add(gMsgAreaInfo.subBoardCode);
// Set a variable to store whether or not cross-posting can be done.
var gCanCrossPost = (gConfigSettings.allowCrossPosting && postingInMsgSubBoard(gMsgArea));
// If the message area name (gMsgArea) is different from what's in gMsgAreaInfo,
// then we probably want to use bbs.cursub_code instead of bbs.smb_sub_code, etc.
if (msg_area.sub[gMsgAreaInfo.subBoardCode].name.indexOf(gMsgArea) == -1)
{
  gMsgAreaInfo.lastMsg = -1;
  gMsgAreaInfo.curMsgNum = -1;
  gMsgAreaInfo.subBoardCode = bbs.cursub_code;
  gMsgAreaInfo.grpIndex = msg_area.sub[bbs.cursub_code].grp_index;
  var tmpMsgBaseObj = new MsgBase(bbs.cursub_code);
  if (tmpMsgBaseObj.open())
  {
    gMsgAreaInfo.totalNumMsgs = tmpMsgBaseObj.total_msgs;
    tmpMsgBaseObj.close();
  }
  else
    gMsgAreaInfo.totalNumMsgs = 0;
}

// Open the quote file / message file
var inputFile = new File(gInputFilename);
if (inputFile.open("r", false))
{
	// Read into the gQuoteLines or gEditLines array, depending on the value
	// of gUseQuotes.  Use a buffer size that should be long enough.
	if (gUseQuotes)
	{
      var textLine = null;  // Line of text read from the quotes file
      while (!inputFile.eof)
      {
        textLine = inputFile.readln(2048);
        // Only use textLine if it's actually a string.
        if (typeof(textLine) == "string")
        {
           textLine = strip_ctrl(textLine);
           // If the line has only whitespace and/or > characters,
           // then make the line blank before putting it into
           // gQuoteLines.
           if (/^[\s>]+$/.test(textLine))
              textLine = "";
           gQuoteLines.push(textLine);
        }
     }
     // If the setting to re-wrap quote lines is enabled, then do it.
     // wrapQuoteLines() will also prefix the quote lines with author's
     // initials if configured to do so.
     // If not configured to re-wrap quote lines, then if configured to
     // prefix quote lines with author's initials, then we need to
     // prefix them here with gQuotePrefix.
     if (gQuoteLines.length > 0)
     {
       if (gConfigSettings.reWrapQuoteLines)
         wrapQuoteLines(gConfigSettings.useQuoteLineInitials, gConfigSettings.indentQuoteLinesWithInitials);
       else if (gConfigSettings.useQuoteLineInitials)
       {
         var maxQuoteLineWidth = gEditWidth - gQuotePrefix.length;
         for (var i = 0; i < gQuoteLines.length; ++i)
           gQuoteLines[i] = quote_msg(gQuoteLines[i], maxQuoteLineWidth, gQuotePrefix);
       }
     }
	}
	else
	{
		var textLine = null;
		while (!inputFile.eof)
		{
			textLine = new TextLine();
			textLine.text = inputFile.readln(2048);
			if (typeof(textLine.text) == "string")
				textLine.text = strip_ctrl(textLine.text);
			else
				textLine.text = "";
			textLine.hardNewlineEnd = true;
			// If there would still be room on the line for at least
			// 1 more character, then add a space to the end of the
			// line.
			if (textLine.text.length < console.screen_columns-1)
				textLine.text += " ";
			gEditLines.push(textLine);
		}

		// If the last edit line is undefined (which is possible after reading the end
		// of the quotes file), then remove it from gEditLines.
		if (gEditLines.length > 0)
		{
			if (gEditLines.length > 0)
			{
				var lastQuoteLineIndex = gEditLines.length - 1;
				if (gEditLines[lastQuoteLineIndex].text == undefined)
					gEditLines.splice(lastQuoteLineIndex, 1);
			}
		}
	}
	inputFile.close();
}

// If the subject is blank, set it to something.
if (gMsgSubj == "")
	gMsgSubj = gToName.replace(/^.*[\\\/]/,'');
// Store a copy of the current subject (possibly allowing the user to
// change the subject in the future)
var gOldSubj = gMsgSubj;

// Now it's edit time.
var exitCode = doEditLoop();

// Remove any extra blank lines that may be at the end of
// the message (in gEditLines).
if ((exitCode == 0) && (gEditLines.length > 0))
{
   var lineIndex = gEditLines.length - 1;
   while ((lineIndex > 0) && (lineIndex < gEditLines.length) &&
           (gEditLines[lineIndex].length() == 0))
   {
      gEditLines.splice(lineIndex, 1);
      --lineIndex;
   }
}

// Clear the screen and display the end-of-program information (if the setting
// is enabled).
console.clear("n");
if (gConfigSettings.displayEndInfoScreen)
{
   displayProgramExitInfo(false);
   console.crlf();
}

// If the user wrote & saved a message, then output the message
// lines to a file with the passed-in input filename.
var savedTheMessage = false;
if ((exitCode == 0) && (gEditLines.length > 0))
{
  // If some message areas have been selected for cross-posting, then otuput
  // which areas will be cross-posted into, and do the cross-posting.
  var crossPosted = false;
  if (gCrossPostMsgSubs.numMsgGrps() > 0)
  {
    // Create a string containing the user's entire message
    var msgContents = "";
    // Append each line to msgContents.  Then,
    //  - If using Synchronet 3.15 or higher:
    //    Depending on whether the line has a hard newline
    //    or a soft newline, append a "\r\n" or a " \n", as
    //    per Synchronet's standard as of 3.15.
    //  - Otherwise (Synchronet 3.14 and below):
    //    Just append a "\r\n" to the line
    if (system.version_num >= 31500)
    {
      var useHardNewline = false;
      for (var i = 0; i < gEditLines.length; ++i)
      {
        // Use a hard newline if the current edit line has one or if this is
        // the last line of the message.
        useHardNewline = (gEditLines[i].hardNewlineEnd || (i == gEditLines.length-1));
        msgContents += gEditLines[i].text + (useHardNewline ? "\r\n" : " \n");
      }
    }
    else // Synchronet 3.14 and below
    {
      for (var i = 0; i < gEditLines.length; ++i)
        msgContents += gEditLines[i].text + "\r\n";
    }

    // If the user has a signature file, then read it and append it to
    // msgContents (with a blank line to separate the message & signature).
    // Note: msgContents already has a newline at the end, so we don't have
    // to append one here; just append the signature.
    var msgSigInfo = readUserSigFile();
    if (msgSigInfo.sigContents.length > 0)
      msgContents += msgSigInfo.sigContents + "\r\n";

    console.print("n");
    console.crlf();
    console.print("cYour message will be posted into the following area(s):");
    console.crlf();
    var postMsgErrStr = ""; // For storing errors related to saving the message
    for (var grpIndex in gCrossPostMsgSubs)
    {
      // Skip the function names (we only want the group indexes)
      if ((grpIndex == "add") || (grpIndex == "remove") || (grpIndex == "subCodeExists") || (grpIndex == "numMsgGrps"))
        continue;

      console.print("bh" + msg_area.grp_list[grpIndex].description + ":");
      console.crlf();
      for (var subCode in gCrossPostMsgSubs[grpIndex])
      {
        if (subCode == gMsgAreaInfo.subBoardCode)
        {
          printf("  ng%-48s", msg_area.sub[subCode].description.substr(0, 48));
          console.print(" c(original message area)");
        }
        // If subCode is not the user's current sub, then if the user is allowed
        // to post in that sub, then post the message there.
        else
        {
          printf("  ng%-73s", msg_area.sub[subCode].description.substr(0, 73));
          if (user.compare_ars(msg_area.sub[subCode].post_ars))
          {
             postMsgErrStr = postMsgToSubBoard(subCode, gToName, gMsgSubj, msgContents, user.number);
             if (postMsgErrStr.length == 0)
             {
               savedTheMessage = true;
               crossPosted = true;
               console.print("nhb[ng" + CHECK_CHAR + "nhb]n");
             }
             else
             {
               console.print("nhb[rXb]n");
               console.crlf();
               console.print("   nhr*n " + postMsgErrStr);
               console.crlf();
             }
          }
          else
          {
             // The user isn't allowed to post in the sub.  Output a message
             // saying so.
             console.print("nhb[rXb]n");
             console.crlf();
             console.print("   nhr*n You're not allowed to post in this area.");
             console.crlf();
          }
        }
        console.crlf();
      }
    }
    console.print("n");
    console.crlf();
  }


  // Determine whether or not to save the message to INPUT.MSG.  We want to
  // save it by default, but if the user is posting in a sub-board, whether we
  // want to save it will be determined by whether the user's current sub-board
  // code is in the list of cross-post areas.
  var saveMsgFile = true;
  if (postingInMsgSubBoard(gMsgArea))
  {
    if (!gCrossPostMsgSubs.subCodeExists(gMsgAreaInfo.subBoardCode))
    {
      saveMsgFile = false;
      // If the message was cross-posted to other message areas and not in the
      // user's current message area, output a message to say that Synchronet
      // will say the message was aborted, and that's normal.
      if (crossPosted)
      {
        console.print("nc* Note: Your message has been cross-posted in areas other than your currently-");
        console.crlf();
        console.print("selected message area.  Because your message was not saved for your currently-");
        console.crlf();
        console.print("selected message area, the BBS will say the message was aborted, even");
        console.crlf();
        console.print("though it was posted in those other areas.  This is normal.n");
        console.crlf();
        console.crlf();
      }
    }
  }
  if (saveMsgFile)
  {
    // Open the output filename.  If no arguments were passed, then use
    // INPUT.MSG in the node's temporary directory; otherwise, use the
    // first program argument.
    var msgFile = new File((argc == 0 ? system.temp_dir + "INPUT.MSG" : argv[0]));
    if (msgFile.open("w"))
    {
      // Write each line of the message to the file.  Note: The
      // "Expand Line Feeds to CRLF" option should be turned on
      // in SCFG for this to work properly for all platforms.
      for (var i = 0; i < gEditLines.length; ++i)
        msgFile.writeln(gEditLines[i].text);
      msgFile.close();
      savedTheMessage = true;
    }
    else
      console.print("nrh* Unable to save the message!n\r\n");
  }
}

/*
// Note: If we were using WWIV editor.inf/result.ed drop files, we
// could allow the user to change the subject and write the new
// subject in result.ed..
if (savedTheMessage)
{
  gMsgSubj = "New subject";
  if (gMsgSubj != gOldSubj)
  {
    var dropFile = new File(system.node_dir + "result.ed");
    if (dropFile.open("w"))
    {
      dropFile.writeln("0");
      dropFile.writeln(gMsgSubj);
      dropFile.close();
    }
  }
}
*/

// Set the original ctrlkey_passthru and sys_status settins back.
console.ctrlkey_passthru = gOldPassthru;
bbs.sys_status = gOldStatus;

// Set the end-of-program status message.
var endStatusMessage = "";
if (exitCode == 1)
   endStatusMessage = "nmhMessage aborted.";
else if (exitCode == 0)
{
   if (gEditLines.length > 0)
   {
      if (savedTheMessage)
         endStatusMessage = "nchThe message has been saved.";
      else
         endStatusMessage = "nmhMessage aborted.";
   }
   else
      endStatusMessage = "nmhEmpty message not sent.";
}
// We shouldn't hit this else case, but it's here just to be safe.
else
   endStatusMessage = "nmhPossible message error.";
console.print(endStatusMessage);
console.crlf();

// If the user's setting to pause after every screenful is disabled, then
// pause here so that they can see the exit information.
if (user.settings & USER_PAUSE == 0)
   mswait(1000);

// Load any specified 3rd-party exit scripts and execute any provided exit
// JavaScript commands.
for (var i = 0; i < gConfigSettings.thirdPartyLoadOnExit.length; ++i)
  load(gConfigSettings.thirdPartyLoadOnExit[i]);
for (var i = 0; i < gConfigSettings.runJSOnExit.length; ++i)
  eval(gConfigSettings.runJSOnExit[i]);

exit(exitCode);

// End of script execution


///////////////////////////////////////////////////////////////////////////////////
// Functions

// Edit mode & input loop
function doEditLoop()
{
	// Return codes:
	// 0: Success
	// 1: Aborted
	var returnCode = 0;

  // Set the shortcut keys.  Note: Avoid CTRL_H because that
  // backspace.
	const ABORT_KEY             = CTRL_A;
	const CROSSPOST_KEY         = CTRL_C;
	const DELETE_LINE_KEY       = CTRL_D;
	const GENERAL_HELP_KEY      = CTRL_G;
	const TOGGLE_INSERT_KEY     = CTRL_I;
	const CHANGE_COLOR_KEY      = CTRL_K;
	const FIND_TEXT_KEY         = CTRL_N;
	const IMPORT_FILE_KEY       = CTRL_O;
	const CMDLIST_HELP_KEY      = CTRL_P;
	const QUOTE_KEY             = CTRL_Q;
	const PROGRAM_INFO_HELP_KEY = CTRL_R;
	const PAGE_DOWN_KEY         = CTRL_S;
	const PAGE_UP_KEY           = CTRL_W;
	const EXPORT_FILE_KEY       = CTRL_X;
	const SAVE_KEY              = CTRL_Z;

   // Draw the screen.
   // Note: This is purposefully drawing the top of the message.  We
   // want to place the cursor at the first character on the top line,
   // too.  This is for the case where we're editin an existing message -
   // we want to start editigng it at the top.
	fpRedrawScreen(gEditLeft, gEditRight, gEditTop, gEditBottom, gTextAttrs,
		             gInsertMode, gUseQuotes, 0, displayEditLines);

	var curpos = new Object();
	curpos.x = gEditLeft;
	curpos.y = gEditTop;
	console.gotoxy(curpos);

	// Input loop
	var userInput = "";
	var currentWordLength = getWordLength(gEditLinesIndex, gTextLineIndex);
	var numKeysPressed = 0; // Used only to determine when to call updateTime()
	var continueOn = true;
	while (continueOn)
	{
		// Get a key, and time out after 5 minutes.
		// Get a keypress from the user.  If the setting for using the
		// input timeout is enabled and the user is not a sysop, then use
		// the input timeout specified in the config file.  Otherwise,
		// don't use a timeout.
		if (gConfigSettings.userInputTimeout && !gIsSysop)
			userInput = console.inkey(K_NOCRLF|K_NOSPIN, gConfigSettings.inputTimeoutMS);
		else
			userInput = console.getkey(K_NOCRLF|K_NOSPIN);
		// If userInput is blank, then the input timeout was probably
		// reached, so abort.
		if (userInput == "")
		{
			returnCode = 1; // Aborted
			continueOn = false;
			console.crlf();
			console.print("nhr" + EDITOR_PROGRAM_NAME + ": Input timeout reached.");
			continue;
		}

		// If we reach this code, the timeout wasn't reached.
		++numKeysPressed;

		// If gEditLines currently has 1 less line than we need,
		// then add a new line to gEditLines.
		if (gEditLines.length == gEditLinesIndex)
			gEditLines.push(new TextLine());

		// Take the appropriate action for the key pressed.
		switch (userInput)
		{
			case ABORT_KEY:
            // Before aborting, ask they user if they really want to abort.
            if (promptYesNo("Abort message", false, "Abort"))
            {
               returnCode = 1; // Aborted
               continueOn = false;
            }
            else
            {
               // Make sure the edit color attribute is set.
               //console.print("n" + gTextAttrs);
               console.print(chooseEditColor());
            }
				break;
			case SAVE_KEY:
				returnCode = 0; // Save
				continueOn = false;
				break;
			case CMDLIST_HELP_KEY:
				displayCommandList(true, true, true, gCanCrossPost, gIsSysop);
				clearEditAreaBuffer();
            fpRedrawScreen(gEditLeft, gEditRight, gEditTop, gEditBottom, gTextAttrs,
		                     gInsertMode, gUseQuotes, gEditLinesIndex-(curpos.y-gEditTop),
		                     displayEditLines);
				break;
			case GENERAL_HELP_KEY:
				displayGeneralHelp(true, true, true);
				clearEditAreaBuffer();
				fpRedrawScreen(gEditLeft, gEditRight, gEditTop, gEditBottom, gTextAttrs,
				               gInsertMode, gUseQuotes, gEditLinesIndex-(curpos.y-gEditTop),
				               displayEditLines);
				break;
			case PROGRAM_INFO_HELP_KEY:
				displayProgramInfo(true, true, true);
				clearEditAreaBuffer();
				fpRedrawScreen(gEditLeft, gEditRight, gEditTop, gEditBottom, gTextAttrs,
				               gInsertMode, gUseQuotes, gEditLinesIndex-(curpos.y-gEditTop),
				               displayEditLines);
            break;
			case QUOTE_KEY:
            // Let the user choose & insert quote lines into the message.
            if (gUseQuotes)
            {
               var retObject = doQuoteSelection(curpos, currentWordLength);
               curpos.x = retObject.x;
               curpos.y = retObject.y;
               currentWordLength = retObject.currentWordLength;
               // If user input timed out, then abort.
               if (retObject.timedOut)
               {
                  returnCode = 1; // Aborted
                  continueOn = false;
                  console.crlf();
                  console.print("nhr" + EDITOR_PROGRAM_NAME + ": Input timeout reached.");
                  continue;
               }
            }
            break;
         case CHANGE_COLOR_KEY:
				    /*
            // Let the user change the text color.
            if (gConfigSettings.allowColorSelection)
            {
               var retObject = doColorSelection(curpos, currentWordLength);
               curpos.x = retObject.x;
               curpos.y = retObject.y;
               currentWordLength = retObject.currentWordLength;
               // If user input timed out, then abort.
               if (retObject.timedOut)
               {
                  returnCode = 1; // Aborted
                  continueOn = false;
                  console.crlf();
                  console.print("nhr" + EDITOR_PROGRAM_NAME + ": Input timeout reached.");
                  continue;
               }
            }
            */
            break;
			case KEY_UP:
				// Move the cursor up one line.
				if (gEditLinesIndex > 0)
				{
					--gEditLinesIndex;

					// gTextLineIndex should containg the index in the text
					// line where the cursor would add text.  If the previous
					// line is shorter than the one we just left, then
					// gTextLineIndex and curpos.x need to be adjusted.
					if (gTextLineIndex > gEditLines[gEditLinesIndex].length())
					{
						gTextLineIndex = gEditLines[gEditLinesIndex].length();
						curpos.x = gEditLeft + gEditLines[gEditLinesIndex].length();
					}
					// Figure out the vertical coordinate of where the
					// cursor should be.
					// If the cursor is at the top of the edit area,
					// then scroll up through the message by 1 line.
					if (curpos.y == gEditTop)
						displayEditLines(gEditTop, gEditLinesIndex, gEditBottom, true, /*true*/false);
					else
						--curpos.y;

					console.gotoxy(curpos);
					currentWordLength = getWordLength(gEditLinesIndex, gTextLineIndex);
					console.print(chooseEditColor()); // Make sure the edit color is correct
				}
				break;
			case KEY_DOWN:
				// Move the cursor down one line.
				if (gEditLinesIndex < gEditLines.length-1)
				{
					++gEditLinesIndex;
					// gTextLineIndex should containg the index in the text
					// line where the cursor would add text.  If the next
					// line is shorter than the one we just left, then
					// gTextLineIndex and curpos.x need to be adjusted.
					if (gTextLineIndex > gEditLines[gEditLinesIndex].length())
					{
						gTextLineIndex = gEditLines[gEditLinesIndex].length();
						curpos.x = gEditLeft + gEditLines[gEditLinesIndex].length();
					}
					// Figure out the vertical coordinate of where the
					// cursor should be.
					// If the cursor is at the bottom of the edit area,
					// then scroll down through the message by 1 line.
					if (curpos.y == gEditBottom)
					{
						displayEditLines(gEditTop, gEditLinesIndex-(gEditBottom-gEditTop),
						                 gEditBottom, true, /*true*/false);
					}
					else
						++curpos.y;

					console.gotoxy(curpos);
					currentWordLength = getWordLength(gEditLinesIndex, gTextLineIndex);
					console.print(chooseEditColor()); // Make sure the edit color is correct
				}
				break;
			case KEY_LEFT:
				// If the horizontal cursor position is right of the
				// leftmost edit position, then let it move left.
				if (curpos.x > gEditLeft)
				{
					--curpos.x;
					console.gotoxy(curpos);
					if (gTextLineIndex > 0)
						--gTextLineIndex;
				}
				else
				{
					// The cursor is at the leftmost position in the
					// edit area.  If there are text lines above the
					// current line, then move the cursor to the end
					// of the previous line.
					if (gEditLinesIndex > 0)
					{
						--gEditLinesIndex;
						curpos.x = gEditLeft + gEditLines[gEditLinesIndex].length();
						// Move the cursor up or scroll up by one line
						if (curpos.y > 1)
							--curpos.y;
						else
							displayEditLines(gEditTop, gEditLinesIndex, gEditBottom, true, /*true*/false);
						gTextLineIndex = gEditLines[gEditLinesIndex].length();
						console.gotoxy(curpos);
					}
				}
				console.print(chooseEditColor());

				// Update the current word length.
				currentWordLength = getWordLength(gEditLinesIndex, gTextLineIndex);
				// Make sure the edit color is correct
				console.print(chooseEditColor());
				break;
			case KEY_RIGHT:
				// If the horizontal cursor position is left of the
				// rightmost edit position, then the cursor can move
				// to the right.
				if (curpos.x < gEditRight)
				{
					// The current line index must be within bounds
					// before we can move the cursor to the right.
					if (gTextLineIndex < gEditLines[gEditLinesIndex].length())
					{
						++curpos.x;
						console.gotoxy(curpos);
						++gTextLineIndex;
					}
					else
					{
						// The cursor is at the rightmost position on the
						// line.  If there are text lines below the current
						// line, then move the cursor to the start of the
						// next line.
						if (gEditLinesIndex < gEditLines.length-1)
						{
							++gEditLinesIndex;
							curpos.x = gEditLeft;
							// Move the cursor down or scroll down by one line
							if (curpos.y < gEditBottom)
								++curpos.y;
							else
								displayEditLines(gEditTop, gEditLinesIndex-(gEditBottom-gEditTop),
								                 gEditBottom, true, /*true*/false);
							gTextLineIndex = 0;
							console.gotoxy(curpos);
						}
					}
				}
				else
				{
					// The cursor is at the rightmost position in the
					// edit area.  If there are text lines below the
					// current line, then move the cursor to the start
					// of the next line.
					if (gEditLinesIndex < gEditLines.length-1)
					{
						++gEditLinesIndex;
						curpos.x = gEditLeft;
						// Move the cursor down or scroll down by one line
						if (curpos.y < gEditBottom)
							++curpos.y;
						else
							displayEditLines(gEditTop, gEditLinesIndex-(gEditBottom-gEditTop),
							                 gEditBottom, true, false);
						gTextLineIndex = 0;
						console.gotoxy(curpos);
					}
				}
				console.print(chooseEditColor());

				// Update the current word length.
				currentWordLength = getWordLength(gEditLinesIndex, gTextLineIndex);
				// Make sure the edit color is correct
				console.print(chooseEditColor());
				break;
			case KEY_HOME:
				// Go to the beginning of the line
				gTextLineIndex = 0;
				curpos.x = gEditLeft;
				console.gotoxy(curpos);
				// Update the current word length.
            currentWordLength = getWordLength(gEditLinesIndex, gTextLineIndex);
				break;
			case KEY_END:
				// Go to the end of the line
				if (gEditLinesIndex < gEditLines.length)
				{
					gTextLineIndex = gEditLines[gEditLinesIndex].length();
					curpos.x = gEditLeft + gTextLineIndex;
					// If the cursor position would be to the right of the edit
					// area, then place it at gEditRight.
					if (curpos.x > gEditRight)
					{
                  var difference = curpos.x - gEditRight;
                  curpos.x -= difference;
                  gTextLineIndex -= difference;
					}
					// Place the cursor where it should be.
					console.gotoxy(curpos);

					// Update the current word length.
               currentWordLength = getWordLength(gEditLinesIndex, gTextLineIndex);
				}
				break;
			case BACKSPACE:
				// Delete the previous character
				var retObject = doBackspace(curpos, currentWordLength);
				curpos.x = retObject.x;
				curpos.y = retObject.y;
				currentWordLength = retObject.currentWordLength;
				// Make sure the edit color is correct
				console.print(chooseEditColor());
				break;
			case KEY_DEL:
				// Delete the next character
				var retObject = doDeleteKey(curpos, currentWordLength);
				curpos.x = retObject.x;
				curpos.y = retObject.y;
				currentWordLength = retObject.currentWordLength;
				// Make sure the edit color is correct
				console.print(chooseEditColor());
				break;
			case KEY_ENTER:
				var retObject = doEnterKey(curpos, currentWordLength);
				curpos.x = retObject.x;
				curpos.y = retObject.y;
				currentWordLength = retObject.currentWordLength;
				returnCode = retObject.returnCode;
        continueOn = retObject.continueOn;
        // Check for whether we should do quote selection or
        // show the help screen (if the user entered /Q or /?)
        if (continueOn)
        {
           if (retObject.doQuoteSelection)
           {
              if (gUseQuotes)
              {
                 retObject = doQuoteSelection(curpos, currentWordLength);
                 curpos.x = retObject.x;
                 curpos.y = retObject.y;
                 currentWordLength = retObject.currentWordLength;
                 // If user input timed out, then abort.
                 if (retObject.timedOut)
                 {
                    returnCode = 1; // Aborted
                    continueOn = false;
                    console.crlf();
                    console.print("nhr" + EDITOR_PROGRAM_NAME + ": Input timeout reached.");
                    continue;
                 }
              }
           }
           else if (retObject.showHelp)
           {
              displayProgramInfo(true, true, false);
              displayCommandList(false, false, true, gCanCrossPost, gIsSysop);
              clearEditAreaBuffer();
              fpRedrawScreen(gEditLeft, gEditRight, gEditTop, gEditBottom, gTextAttrs,
                             gInsertMode, gUseQuotes, gEditLinesIndex-(curpos.y-gEditTop),
                             displayEditLines);
              console.gotoxy(curpos);
           }
           else if (retObject.doCrossPostSelection)
           {
              if (gCanCrossPost)
                 doCrossPosting();
           }
        }
        // Make sure the edit color is correct
				console.print(chooseEditColor());
				break;
         // Insert/overwrite mode toggle
         case KEY_INSERT:
         case TOGGLE_INSERT_KEY:
            toggleInsertMode(null);
            //console.print("n" + gTextAttrs);
            console.print(chooseEditColor());
            console.gotoxy(curpos);
            break;
			case KEY_ESC:
            // Do the ESC menu
            var retObj = fpHandleESCMenu(curpos, currentWordLength);
            returnCode = retObj.returnCode;
            continueOn = retObj.continueOn;
            curpos.x = retObj.x;
            curpos.y = retObj.y;
            currentWordLength = retObj.currentWordLength;
            // If we can continue on, put the cursor back
            // where it should be.
            if (continueOn)
            {
               //console.print("n" + gTextAttrs);
               console.print(chooseEditColor());
               console.gotoxy(curpos);
            }
				break;
         case FIND_TEXT_KEY:
            var retObj = findText(curpos);
            curpos.x = retObj.x;
            curpos.y = retObj.y;
            console.print(chooseEditColor()); // Make sure the edit color is correct
            break;
         case IMPORT_FILE_KEY:
            // Only let sysops import files.
            if (gIsSysop)
            {
               var retObj = importFile(gIsSysop, curpos);
               curpos.x = retObj.x;
               curpos.y = retObj.y;
               currentWordLength = retObj.currentWordLength;
               console.print(chooseEditColor()); // Make sure the edit color is correct
            }
            break;
         case EXPORT_FILE_KEY:
            // Only let sysops export files.
            if (gIsSysop)
            {
               exportToFile(gIsSysop);
               console.gotoxy(curpos);
            }
            break;
         case DELETE_LINE_KEY:
            var retObj = doDeleteLine(curpos);
            curpos.x = retObj.x;
            curpos.y = retObj.y;
            currentWordLength = retObj.currentWordLength;
            console.print(chooseEditColor()); // Make sure the edit color is correct
            break;
         case PAGE_UP_KEY: // Move 1 page up in the message
            // Calculate the index of the message line shown at the top
            // of the edit area.
            var topEditIndex = gEditLinesIndex-(curpos.y-gEditTop);
            // If topEditIndex is > 0, then we can page up.
            if (topEditIndex > 0)
            {
               // Calculate the new top edit line index.
               // If there is a screenful or more of lines above the top,
               // then set topEditIndex to what it would need to be for the
               // previous page.  Otherwise, set topEditIndex to 0.
               if (topEditIndex >= gEditHeight)
                  topEditIndex -= gEditHeight;
               else
                  topEditIndex = 0;
               // Refresh the edit area
               displayEditLines(gEditTop, topEditIndex, gEditBottom, true, /*true*/false);
               // Set the cursor to the last place on the last line.
               gEditLinesIndex = topEditIndex + gEditHeight - 1;
               gTextLineIndex = gEditLines[gEditLinesIndex].length();
               if ((gTextLineIndex > 0) && (gEditLines[gEditLinesIndex].length == gEditWidth))
                  --gTextLineIndex;
               curpos.x = gEditLeft + gTextLineIndex;
               curpos.y = gEditBottom;
               console.gotoxy(curpos);

               // Update the current word length.
               currentWordLength = getWordLength(gEditLinesIndex, gTextLineIndex);
            }
            else
            {
               // topEditIndex is 0.  If gEditLinesIndex is not already 0,
               // then make it 0 and place the cursor at the first line.
               if (gEditLinesIndex > 0)
               {
                  gEditLinesIndex = 0;
                  gTextLineIndex = 0;
                  curpos.x = gEditLeft;
                  curpos.y = gEditTop;
                  console.gotoxy(curpos);

                  // Update the current word length.
                  currentWordLength = getWordLength(gEditLinesIndex, gTextLineIndex);
               }
            }
            console.print(chooseEditColor()); // Make sure the edit color is correct
            break;
         case PAGE_DOWN_KEY: // Move 1 page down in the message
            // Calculate the index of the message line shown at the top
            // of the edit area, and the index of the line that would be
            // shown at the bottom of the edit area.
            var topEditIndex = gEditLinesIndex-(curpos.y-gEditTop);
            var bottomEditIndex = topEditIndex + gEditHeight - 1;
            // If bottomEditIndex is less than the last index, then we can
            // page down.
            var lastEditLineIndex = gEditLines.length-1;
            if (bottomEditIndex < lastEditLineIndex)
            {
               // Calculate the new top edit line index.
               // If there is a screenful or more of lines below the bottom,
               // then set topEditIndex to what it would need to be for the
               // next page.  Otherwise, set topEditIndex to the right
               // index to display the last full page.
               if (gEditLines.length - gEditHeight > bottomEditIndex)
                  topEditIndex += gEditHeight;
               else
                  topEditIndex = gEditLines.length - gEditHeight;
               // Refresh the edit area
               displayEditLines(gEditTop, topEditIndex, gEditBottom, true, /*true*/false);
               // Set the cursor to the first place on the first line.
               gEditLinesIndex = topEditIndex;
               gTextLineIndex = 0;
               curpos.x = gEditLeft;
               curpos.y = gEditTop;
               console.gotoxy(curpos);

               // Update the current word length.
               currentWordLength = getWordLength(gEditLinesIndex, gTextLineIndex);
            }
            else
            {
               // bottomEditIndex >= the last edit line index.
               // If gEditLinesIndex is not already equal to bottomEditIndex,
               // make it so and put the cursor at the end of the last line.
               if (gEditLinesIndex < bottomEditIndex)
               {
                  var oldEditLinesIndex = gEditLinesIndex;

                  // Make sure gEditLinesIndex is valid.  It should be set to the
                  // last edit line index.  It's possible that bottomEditIndex is
                  // beyond the last edit line index, so we need to be careful here.
                  if (bottomEditIndex == lastEditLineIndex)
                     gEditLinesIndex = bottomEditIndex;
                  else
                     gEditLinesIndex = lastEditLineIndex;
                  gTextLineIndex = gEditLines[gEditLinesIndex].length();
                  if ((gTextLineIndex > 0) && (gEditLines[gEditLinesIndex].length == gEditWidth))
                     --gTextLineIndex;
                  curpos.x = gEditLeft + gTextLineIndex;
                  curpos.y += (gEditLinesIndex-oldEditLinesIndex);
                  console.gotoxy(curpos);

                  // Update the current word length.
                  currentWordLength = getWordLength(gEditLinesIndex, gTextLineIndex);
               }
            }
            console.print(chooseEditColor()); // Make sure the edit color is correct
            break;
         case CROSSPOST_KEY:
            if (gCanCrossPost)
               doCrossPosting();
            break;
         default:
            // For the tab character, insert 3 spaces.  Otherwise,
            // if it's a printabel character, add the character.
            if (/\t/.test(userInput))
            {
               var retObject;
               for (var i = 0; i < 3; ++i)
               {
                  retObject = doPrintableChar(" ", curpos, currentWordLength);
                  curpos.x = retObject.x;
                  curpos.y = retObject.y;
                  currentWordLength = retObject.currentWordLength;
               }
            }
            else
            {
               if (isPrintableChar(userInput))
               {
                  var retObject = doPrintableChar(userInput, curpos, currentWordLength);
                  curpos.x = retObject.x;
                  curpos.y = retObject.y;
                  currentWordLength = retObject.currentWordLength;
               }
            }
            break;
		}

		// For every 5 keys pressed, dheck the current time and update
		// it on the screen if necessary.
		if (numKeysPressed % 5 == 0)
			updateTime();
	}

   // If gEditLines has only 1 line in it and it's blank, then
   // remove it so that we can test to see if the message is empty.
   if (gEditLines.length == 1)
   {
      if (gEditLines[0].length() == 0)
         gEditLines.splice(0, 1);
   }

	return returnCode;
}
// Helper function for doEditLoop(): Handles the backspace behavior.
//
// Parameters:
//  pCurpos: An object containing x and y values representing the
//           cursor position.
//  pCurrentWordLength: The length of the current word that has been typed
//
// Return value: An object containing x and y values representing the cursor
//               position and currentLength, the current word length.
function doBackspace(pCurpos, pCurrentWordLength)
{
   // Create the return object.
	var retObj = new Object();
	retObj.x = pCurpos.x;
	retObj.y = pCurpos.y;
	retObj.currentWordLength = pCurrentWordLength;

	var didBackspace = false;
	// For later, store a backup of the current edit line index and
	// cursor position.
	var originalCurrentLineIndex = gEditLinesIndex;
	var originalX = pCurpos.x;
	var originalY = pCurpos.y;
	var originallyOnLastLine = (gEditLinesIndex == gEditLines.length-1);

	// If the cursor is beyond the leftmost position in
	// the edit area, then we can simply remove the last
	// character in the current line and move the cursor
	// over to the left.
	if (retObj.x > gEditLeft)
	{
		if (gTextLineIndex > 0)
		{
			console.print(BACKSPACE);
			console.print(" ");
			--retObj.x;
			console.gotoxy(retObj.x, retObj.y);

			// Remove the previous character from the text line
			var textLineLength = gEditLines[gEditLinesIndex].length();
			if (textLineLength > 0)
			{
				var textLine = gEditLines[gEditLinesIndex].text.substr(0, gTextLineIndex-1)
				             + gEditLines[gEditLinesIndex].text.substr(gTextLineIndex);
				gEditLines[gEditLinesIndex].text = textLine;
				didBackspace = true;
				--gTextLineIndex;
			}
		}
	}
	else
	{
		// The cursor is at the leftmost position in the edit area.
		// If we are beyond the first text line, then move as much of
		// the current text line as possible up to the previous line,
		// if there's room (if not, don't do anything).
		if (gEditLinesIndex > 0)
		{
         var prevLineIndex = gEditLinesIndex - 1;
         if (gEditLines[gEditLinesIndex].length() > 0)
         {
            // Store the previous line's original length
            var originalPrevLineLen = gEditLines[prevLineIndex].length();

            // See how much space is at the end of the previous line
            var previousLineEndSpace = gEditWidth - gEditLines[prevLineIndex].length();
            if (previousLineEndSpace > 0)
            {
               var index = previousLineEndSpace - 1;
               // If that index is valid for the current line, then find the first
               // space in the current line so that the text would fit at the end
               // of the previous line.  Otherwise, set index to the length of the
               // current line so that we'll move the whole current line up to the
               // previous line.
               if (index < gEditLines[gEditLinesIndex].length())
               {
                  for (; index >= 0; --index)
                  {
                     if (gEditLines[gEditLinesIndex].text.charAt(index) == " ")
                        break;
                  }
               }
               else
                  index = gEditLines[gEditLinesIndex].length();
               // If we found a space, then move the part of the current line before
               // the space to the end of the previous line.
               if (index > 0)
               {
                  var linePart = gEditLines[gEditLinesIndex].text.substr(0, index);
                  gEditLines[gEditLinesIndex].text = gEditLines[gEditLinesIndex].text.substr(index);
                  gEditLines[prevLineIndex].text += linePart;
                  gEditLines[prevLineIndex].hardNewlineEnd = gEditLines[gEditLinesIndex].hardNewlineEnd;

                  // If the current line is now blank, then remove it from gEditLines.
                  if (gEditLines[gEditLinesIndex].length() == 0)
                     gEditLines.splice(gEditLinesIndex, 1);

                  // Update the global edit variables so that the cursor is placed
                  // on the previous line.
                  --gEditLinesIndex;
                  // Search for linePart in the line - If found, the cursor should
                  // be placed where it starts.  If it' snot found, place the cursor
                  // at the end of the line.
                  var linePartIndex = gEditLines[gEditLinesIndex].text.indexOf(linePart);
                  if (linePartIndex > -1)
                     gTextLineIndex = linePartIndex;
                  else
                     gTextLineIndex = gEditLines[gEditLinesIndex].length();

                  retObj.x = gEditLeft + gTextLineIndex;
                  if (retObj.y > gEditTop)
                     --retObj.y;

                  didBackspace = true;
               }
            }
         }
         else
         {
            // The current line's length is 0.
            // If there's enough room on the previous line, remove the
            // current line and place the cursor at the end of the
            // previous line.
            if (gEditLines[prevLineIndex].length() <= gEditWidth-1)
            {
               gEditLines.splice(gEditLinesIndex, 1);

               --gEditLinesIndex;
               gTextLineIndex = gEditLines[prevLineIndex].length();
               retObj.x = gEditLeft + gEditLines[prevLineIndex].length();
               if (retObj.y > gEditTop)
                  --retObj.y;

               didBackspace = true;
            }
         }
		}
	}

	// If the backspace was performed, then re-adjust the text lines
	// and refresh the screen.
	if (didBackspace)
	{
      // Store the previous line of text now so we can compare it later
      var prevTextline = "";
      if (gEditLinesIndex > 0)
         prevTextline = gEditLines[gEditLinesIndex-1].text;

      // Re-adjust the text lines
		reAdjustTextLines(gEditLines, gEditLinesIndex, gEditLines.length, gEditWidth);

      // If the previous line's length increased, that probably means that the
      // user backspaced to the beginning of the current line and the word was
      // moved to the end of the previous line.  If so, then move the cursor to
      // the end of the previous line.
      //var scrolled = false;
      if ((gEditLinesIndex > 0) &&
          (gEditLines[gEditLinesIndex-1].length() > prevTextline.length))
      {
         // Update the text index variables and cusor position variables.
         --gEditLinesIndex;
         gTextLineIndex = gEditLines[gEditLinesIndex].length();
         retObj.x = gEditLeft + gTextLineIndex;
         if (retObj.y > gEditTop)
            --retObj.y;
      }

      // If the cursor was at the leftmost position in the edit area,
      // update the edit lines from the currently-set screen line #.
      if (originalX == gEditLeft)
      {
         // Since the original X position was at the left edge of the edit area,
         // display the edit lines starting with the previous line if possible.
         if ((gEditLinesIndex > 0) && (retObj.y > gEditTop))
            displayEditLines(retObj.y-1, gEditLinesIndex-1, gEditBottom, true, true);
         else
            displayEditLines(retObj.y, gEditLinesIndex, gEditBottom, true, true);
      }
      // If the original horizontal cursor position was in the middle of
      // the line, and the line is the last line on the screen, then
      // only refresh that one line on the screen.
      else if ((originalX > gEditLeft) && (originalX < gEditLeft + gEditWidth - 1) && originallyOnLastLine)
         displayEditLines(originalY, originalCurrentLineIndex, originalY, false);
		// If scrolling was to be done, then refresh the entire
		// current message text on the screen from the top of the
		// edit area.  Otherwise, only refresh starting from the
		// original horizontal position and message line.
      else
      {
         // Display the edit lines starting with the previous line if possible.
         if ((gEditLinesIndex > 0) && (retObj.y > gEditTop))
            displayEditLines(retObj.y-1, gEditLinesIndex-1, gEditBottom, true, true);
         else
            displayEditLines(retObj.y, gEditLinesIndex, gEditBottom, true, true);
      }

      // Make sure the current word length is correct.
      retObj.currentWordLength = getWordLength(gEditLinesIndex, gTextLineIndex);
	}

   // Make sure the cursor is placed where it should be.
   console.gotoxy(retObj.x, retObj.y);
	return retObj;
}

// Helper function for doEditLoop(): Handles the delete key behavior.
//
// Parameters:
//  pCurpos: An object containing x and y values representing the
//           cursor position.
//  pCurrentWordLength: The length of the current word that has been typed
//
// Return value: An object containing x and y values representing the cursor
//               position and currentLength, the current word length.
function doDeleteKey(pCurpos, pCurrentWordLength)
{
   // Create the return object
  var returnObject = new Object();
	returnObject.x = pCurpos.x;
	returnObject.y = pCurpos.y;
	returnObject.currentWordLength = pCurrentWordLength;

  // Store the original line text (for testing to see if we should update the screen).
  var originalLineText = gEditLines[gEditLinesIndex].text;

  // If gEditLinesIndex is invalid, then return without doing anything.
  if ((gEditLinesIndex < 0) || (gEditLinesIndex >= gEditLines.length))
     return returnObject;

  // If the text line index is within bounds, then we can
  // delete the next character and refresh the screen.
  if (gTextLineIndex < gEditLines[gEditLinesIndex].length())
  {
     var lineText = gEditLines[gEditLinesIndex].text.substr(0, gTextLineIndex)
                   + gEditLines[gEditLinesIndex].text.substr(gTextLineIndex+1);
     gEditLines[gEditLinesIndex].text = lineText;
     // If the current character is a space, then reset the current word length.
     // to 0.  Otherwise, set it to the current word length.
     if (gTextLineIndex < gEditLines[gEditLinesIndex].length())
     {
        if (gEditLines[gEditLinesIndex].text.charAt(gTextLineIndex) == " ")
           returnObject.currentWordLength = 0;
        else
        {
           var spacePos = gEditLines[gEditLinesIndex].text.indexOf(" ", gTextLineIndex);
           if (spacePos > -1)
              returnObject.currentWordLength = spacePos - gTextLineIndex;
           else
              returnObject.currentWordLength = getWordLength(gEditLinesIndex, gTextLineIndex);
        }
     }

     // Re-adjust the line lengths and refresh the edit area.
     var textChanged = reAdjustTextLines(gEditLines, gEditLinesIndex, gEditLines.length,
                                         gEditWidth);

     // If the line text changed, then update the message area from the
     // current line on down.
     textChanged = textChanged || (gEditLines[gEditLinesIndex].text != originalLineText);
     if (textChanged)
     {
        // Calculate the bottommost edit area row to update, and then
        // refresh the edit area.
        var bottommostRow = calcBottomUpdateRow(returnObject.y, gEditLinesIndex);
        displayEditLines(returnObject.y, gEditLinesIndex, bottommostRow, true, true);
     }
  }
  else
  {
     // The textChanged variable will be used by this code to store whether or
     // not any text changed so we'll know if the screen needs to be refreshed.
     var textChanged = false;

     // The text line index is at the end of the line.
     // Set the current line's hardNewlineEnd property to false
     // so that we can bring up text from the next line,
     // if possible.
     gEditLines[gEditLinesIndex].hardNewlineEnd = false;
     // Also, temporarily set the line's isQuoteLine property to false
     // so that text from the next line can be brought up.  Store the
     // current isQuoteLine value so it can be restored later.
     var lineIsQuoteLine = gEditLines[gEditLinesIndex].isQuoteLine;
     gEditLines[gEditLinesIndex].isQuoteLine = false;

     // If the current line is blank and is not the last line, then remove it.
     if (gEditLines[gEditLinesIndex].length() == 0)
     {
        if (gEditLinesIndex < gEditLines.length-1)
        {
           gEditLines.splice(gEditLinesIndex, 1);
           textChanged = true;
        }
     }
     // If the next line is blank, then set its
     // hardNewlineEnd to false too, so that lower
     // text lines can be brought up.
     else if (gEditLinesIndex < gEditLines.length-1)
     {
        var nextLineIndex = gEditLinesIndex + 1;
        if (gEditLines[nextLineIndex].length() == 0)
           gEditLines[nextLineIndex].hardNewlineEnd = false;
     }

     // Re-adjust the text lines, update textChanged, restore the line's
     // isQuoteLine property, and set a few other things.
     textChanged = textChanged || reAdjustTextLines(gEditLines, gEditLinesIndex,
                                                    gEditLines.length, gEditWidth);
     gEditLines[gEditLinesIndex].isQuoteLine = lineIsQuoteLine;
     returnObject.currentWordLength = getWordLength(gEditLinesIndex, gTextLineIndex);
     var startRow = returnObject.y;
     var startEditLinesIndex = gEditLinesIndex;
     if (returnObject.y > gEditTop)
     {
       --startRow;
       --startEditLinesIndex;
     }

     // If text changed, then refresh the edit area.
     textChanged = textChanged || (gEditLines[gEditLinesIndex].text != originalLineText);
     if (textChanged)
     {
        // Calculate the bottommost edit area row to update, and then
        // refresh the edit area.
        var bottommostRow = calcBottomUpdateRow(startRow, startEditLinesIndex);
        displayEditLines(startRow, startEditLinesIndex, bottommostRow, true, true);
     }
  }

  // Move the cursor where it should be.
  console.gotoxy(returnObject.x, returnObject.y);

  return returnObject;
}

// Helper function for doEditLoop(): Handles printable characters.
//
// Parameters:
//  pUserInput: The user's input
//  pCurpos: An object containing x and y values representing the
//           cursor position.
//  pCurrentWordLength: The length of the current word that has been typed
//
// Return value: An object containing the following properties:
//               x: The horizontal component of the cursor position
//               y: The vertical component of the cursor position
//               currentLength: The length of the current word
function doPrintableChar(pUserInput, pCurpos, pCurrentWordLength)
{
   // Create the return object.
   var retObj = new Object();
   retObj.x = pCurpos.x;
	retObj.y = pCurpos.y;
	retObj.currentWordLength = pCurrentWordLength;

   // Note: gTextLineIndex is where the new character will appear in the line.
   // If gTextLineIndex is somehow past the end of the current line, then
   // fill it with spaces up to gTextLineIndex.
   if (gTextLineIndex > gEditLines[gEditLinesIndex].length())
   {
      var numSpaces = gTextLineIndex - gEditLines[gEditLinesIndex].length();
      if (numSpaces > 0)
         gEditLines[gEditLinesIndex].text += format("%" + numSpaces + "s", "");
      gEditLines[gEditLinesIndex].text += pUserInput;
   }
   // If gTextLineIndex is at the end of the line, then just append the char.
   else if (gTextLineIndex == gEditLines[gEditLinesIndex].length())
      gEditLines[gEditLinesIndex].text += pUserInput;
   else
   {
      // gTextLineIndex is at the beginning or in the middle of the line.
      if (inInsertMode())
      {
         gEditLines[gEditLinesIndex].text = spliceIntoStr(gEditLines[gEditLinesIndex].text,
                                                          gTextLineIndex, pUserInput);
      }
      else
      {
         gEditLines[gEditLinesIndex].text = gEditLines[gEditLinesIndex].text.substr(0, gTextLineIndex)
                                          + pUserInput + gEditLines[gEditLinesIndex].text.substr(gTextLineIndex+1);
      }
   }

   // Store a copy of the current line so that we can compare it later to see
   // if it was modified by reAdjustTextLines().
   var originalAfterCharApplied = gEditLines[gEditLinesIndex].text;

   // If the line is now too long to fit in the edit area, then we will have
   // to re-adjust the text lines.
   var reAdjusted = false;
   if (gEditLines[gEditLinesIndex].length() >= gEditWidth)
      reAdjusted = reAdjustTextLines(gEditLines, gEditLinesIndex, gEditLines.length, gEditWidth);

   // placeCursorAtEnd specifies whether or not to place the cursor at its
   // spot using console.gotoxy() at the end.  This is an optimization.
   var placeCursorAtEnd = true;

   // If the current text line is now different (modified by reAdjustTextLines()),
   // then we'll need to refresh multiple lines on the screen.
   if (reAdjusted && (gEditLines[gEditLinesIndex].text != originalAfterCharApplied))
   {
      // TODO: In case the user entered a whole line of text without any spaces,
      // the new character would appear on the next line, so we need to figure
      // out where the cursor location should be.

      // If gTextLineIndex is >= gEditLines[gEditLinesIndex].length(), then
      // we know the current word was wrapped to the next line.  Figure out what
      // retObj.x, retObj.currentWordLength, gEditLinesIndex, and gTextLineIndex
      // should be, and increment retObj.y.  Also figure out what lines on the
      // screen to update, and deal with scrolling if necessary.
      if (gTextLineIndex >= gEditLines[gEditLinesIndex].length())
      {
         // TODO: I changed this on 2010-02-14 to (hopefully) place the cursor
         // where it should be 
         // Old line (prior to 2010-02-14):
         //var numChars = gTextLineIndex - gEditLines[gEditLinesIndex].length();
         // New (2010-02-14):
         var numChars = 0;
         // Special case: If the current line's length is exactly the longest
         // edit with, then the # of chars should be 0 or 1, depending on whether the
         // entered character was a space or not.  Otherwise, calculate numChars
         // normally.
         if (gEditLines[gEditLinesIndex].length() == gEditWidth-1)
            numChars = ((pUserInput == " ") ? 0 : 1);
         else
            numChars = gTextLineIndex - gEditLines[gEditLinesIndex].length();
         retObj.x = gEditLeft + numChars;
         var originalEditLinesIndex = gEditLinesIndex++;
         gTextLineIndex = numChars;
         // The following line is now done at the end:
         //retObj.currentWordLength = getWordLength(gEditLinesIndex, gTextLineIndex);

         // Figure out which lines we need to update on the screen and whether
         // to do scrolling and what retObj.y should be.
         if (retObj.y < gEditBottom)
         {
            // We're above the last line on the screen, so we can go one
            // line down.
            var originalY = retObj.y++;
            // Update the lines on the screen.
            var bottommostRow = calcBottomUpdateRow(originalY, originalEditLinesIndex);
            displayEditLines(originalY, originalEditLinesIndex, bottommostRow, true, true);
         }
         else
         {
            // We're on the last line in the edit area, so we need to scroll
            // the text lines up on the screen.
            var editLinesTopIndex = gEditLinesIndex - (pCurpos.y - gEditTop);
            displayEditLines(gEditTop, editLinesTopIndex, gEditBottom, true, true);
         }
      }
      else
      {
         // gTextLineIndex is < the line's length.  Update the lines on the
         // screen from the current line down.  Increment retObj.x,
         // retObj.currentWordLength, and gTextLineIndex.
         var bottommostRow = calcBottomUpdateRow(retObj.y, gEditLinesIndex);
         displayEditLines(retObj.y, gEditLinesIndex, bottommostRow, true, true);
         if (pUserInput == " ")
            retObj.currentWordLength = 0;
         else
            ++retObj.currentWordLength;
         ++retObj.x;
         ++gTextLineIndex;
      }
   }
   else
   {
      // The text line wasn't changed by reAdjustTextLines.

      // If gTextLineIndex is not the last index of the line, then refresh the
      // entire line on the screen.  Otherwise, just output the character that
      // the user typed.
      if (gTextLineIndex < gEditLines[gEditLinesIndex].length()-1)
         displayEditLines(retObj.y, gEditLinesIndex, retObj.y, false, true);
      else
      {
         console.print(pUserInput);
         placeCursorAtEnd = false; // Since we just output the character
      }

      // Keep housekeeping variables up to date.
      ++retObj.x;
      ++gTextLineIndex;
      /* retObj.currentWordLength is now calculated at the end, but we could do this:
      if (pUserInput == " ")
         retObj.currentWordLength = 0;
      else
         ++retObj.currentWordLength;
      */
   }

   // Make sure the current word length is correct.
   retObj.currentWordLength = getWordLength(gEditLinesIndex, gTextLineIndex);

   // Make sure the cursor is placed where it should be.
   if (placeCursorAtEnd)
      console.gotoxy(retObj.x, retObj.y);

   return retObj;
}

// Helper function for doEditLoop(): Performs the action for when the user
// presses the enter key.
//
// Parameters:
//  pCurpos: An object containing x and y values representing the
//           cursor position.
//  pCurrentWordLength: The length of the current word that has been typed
//
// Return value: An object containing the following values:
//               x: The horizontal component of the cursor position
//               y: The vertical component of the cursor position
//               currentWordLength: The current word length
//               returnCode: The return code for the program (in case the
//                           user saves or aborts)
//               continueOn: Whether or not the edit loop should continue
//               doQuoteSelection: Whether or not the user typed the command
//                                 to do quote selection.
//               showHelp: Whether or not the user wants to show the help screen
function doEnterKey(pCurpos, pCurrentWordLength)
{
   // Create the return object
   var retObj = new Object();
	retObj.x = pCurpos.x;
	retObj.y = pCurpos.y;
	retObj.currentWordLength = pCurrentWordLength;
	retObj.returnCode = 0;
	retObj.continueOn = true;
	retObj.doQuoteSelection = false;
	retObj.doCrossPostSelection = false;
	retObj.showHelp = false;

   // Check for slash commands (/S, /A, /?).  If the user has
   // typed one of them by itself at the beginning of the line,
   // then save, abort, or show help, respectively.
   if (gEditLines[gEditLinesIndex].length() == 2)
   {
      var lineUpper = gEditLines[gEditLinesIndex].text.toUpperCase();
      // /S: Save
      if (lineUpper == "/S")
      {
         // If the current text line is the last one, remove it; otherwise,
         // blank it out.
         if (gEditLinesIndex == gEditLines.length-1)
            gEditLines.splice(gEditLinesIndex, 1);
         else
            gEditLines[gEditLinesIndex].text = "";

         retObj.continueOn = false;
         return(retObj);
      }
      // /A: Abort
      else if (lineUpper == "/A")
      {
         // Confirm with the user
         if (promptYesNo("Abort message", false, "Abort"))
         {
            retObj.returnCode = 1; // 1: Abort
            retObj.continueOn = false;
            return(retObj);
         }
         else
         {
            // Make sure the edit color attribute is set back.
            //console.print("n" + gTextAttrs);
            console.print(chooseEditColor());

            // Blank out the data in the text line, set the data in
            // retObj, and return it.
            gEditLines[gEditLinesIndex].text = "";
            retObj.currentWordLength = 0;
            gTextLineIndex = 0;
            retObj.x = gEditLeft;
            retObj.y = pCurpos.y;
            // Blank out the /A on the screen
            //console.print("n" + gTextAttrs);
            console.print(chooseEditColor());
            console.gotoxy(retObj.x, retObj.y);
            console.print("  ");
            // Put the cursor where it should be and return.
            console.gotoxy(retObj.x, retObj.y);
            return(retObj);
         }
      }
      // /Q: Do quote selection, and /?: Show help
      else if ((lineUpper == "/Q") || (lineUpper == "/?"))
      {
         retObj.doQuoteSelection = (lineUpper == "/Q");
         retObj.showHelp = (lineUpper == "/?");
         retObj.currentWordLength = 0;
         gTextLineIndex = 0;
         gEditLines[gEditLinesIndex].text = "";
         // Blank out the /? on the screen
         //console.print("n" + gTextAttrs);
         console.print(chooseEditColor());
         retObj.x = gEditLeft;
         console.gotoxy(retObj.x, retObj.y);
         console.print("  ");
         // Put the cursor where it should be and return.
         console.gotoxy(retObj.x, retObj.y);
         return(retObj);
      }
      else if (lineUpper == "/C")
      {
         retObj.doCrossPostSelection = true;

         // Blank out the data in the text line, set the data in
         // retObj, and return it.
         gEditLines[gEditLinesIndex].text = "";
         retObj.currentWordLength = 0;
         gTextLineIndex = 0;
         retObj.x = gEditLeft;
         retObj.y = pCurpos.y;
         // Blank out the /C on the screen
         //console.print("n" + gTextAttrs);
         console.print(chooseEditColor());
         retObj.x = gEditLeft;
         console.gotoxy(retObj.x, retObj.y);
         console.print("  ");
         // Put the cursor where it should be and return.
         console.gotoxy(retObj.x, retObj.y);
         return(retObj);
      }
   }

	// Store the current screen row position and gEditLines index.
	var initialScreenLine = pCurpos.y;
	var initialEditLinesIndex = gEditLinesIndex;

	// If we're currently on the last line, then we'll need to append
	// a new line.  Otherwise, we'll need to splice a new line into
	// gEditLines where appropriate.

	var appendLineToEnd = (gEditLinesIndex == gEditLines.length-1);
	var retObject = enterKey_InsertOrAppendNewLine(pCurpos, pCurrentWordLength, appendLineToEnd);
	retObj.x = retObject.x;
	retObj.y = retObject.y;
	retObj.currentWordLength = retObject.currentWordLength;

   // If a line was added to gEditLines, then set the hardNewlineEnd property
   // to true for both lines.
   if (retObject.addedATextLine)
   {
      gEditLines[initialEditLinesIndex].hardNewlineEnd = true;
      gEditLines[gEditLinesIndex].hardNewlineEnd = true;
   }

	// Refresh the message text on the screen if that wasn't done by
	// enterKey_InsertOrAppendNewLine().
	if (!retObject.displayedEditlines)
      displayEditLines(initialScreenLine, initialEditLinesIndex, gEditBottom, true, true);

	console.gotoxy(retObj.x, retObj.y);

	return retObj;
}

// Helper function for doEnterKey(): Appends/inserts a line to gEditLines
// and returns the position of where the cursor shoul dbe.
//
// Parameters:
//  pCurpos: An object containing x and y values representing the
//           cursor position.
//  pCurrentWordLength: The length of the current word that has been typed
//  pAppendLine: Whether or not to append the new line (true/false).  If false,
//               then the new line will be spliced into the middle of the array
//               where it belongs rather than appended to the end.
//
// Return value: An object containing the following values:
//               - x and y values representing the cursor position
//               - currentLength: The current word length
//               - displayedEditlines: Whether or not the edit lines were refreshed
//               - addedATextLine: Whether or not a line of text was added to gEditLines
//               - addedTextLineBelow: If addedATextLine is true, whether or not
//                 the line was added below the line
function enterKey_InsertOrAppendNewLine(pCurpos, pCurrentWordLength, pAppendLine)
{
   var returnObject = new Object();
   returnObject.displayedEditlines = false;
   returnObject.addedATextLine = false;
   returnObject.addedTextLineBelow = false;

	// If we're at the end of the line, then we can simply
	// add a new blank line & set the cursor there.
	// Otherwise, we need to split the current line, and
	// the text to the right of the cursor will go on the new line.
	if (gTextLineIndex == gEditLines[gEditLinesIndex].length())
	{
		if (pAppendLine)
		{
			// Add a new blank line to the end of the message, and set
			// the cursor there.
			gEditLines.push(new TextLine());
			++gEditLinesIndex;
			returnObject.addedATextLine = true;
         returnObject.addedTextLineBelow = true;
		}
		else
		{
			// Increment gEditLinesIndex and add a new line there.
			++gEditLinesIndex;
			gEditLines.splice(gEditLinesIndex, 0, new TextLine());
         returnObject.addedATextLine = true;
		}

		gTextLineIndex = 0;
		pCurrentWordLength = 0;
		pCurpos.x = gEditLeft;
		// Update the vertical cursor position.
		// If the cursor is at the bottom row, then we need
		// to scroll the message down by 1 line.  Otherwise,
		// we can simply increment pCurpos.y.
		if (pCurpos.y == gEditBottom)
		{
			displayEditLines(gEditTop, gEditLinesIndex-(gEditBottom-gEditTop),
			                 gEditBottom, true, true);
         returnObject.displayedEditlines = true;
      }
		else
			++pCurpos.y;
	}
	else
	{
		// We're in the middle of the line.
		// Get the text to the end of the current line.
		var lineEndText = gEditLines[gEditLinesIndex].text.substr(gTextLineIndex);
		// Remove that text from the current line.
		gEditLines[gEditLinesIndex].text = gEditLines[gEditLinesIndex].text.substr(0, gTextLineIndex);

		if (pAppendLine)
		{
			// Create a new line containing lineEndText and append it to
			// gEditLines.  Then place the cursor at the start of that line.
			var newTextLine = new TextLine();
			newTextLine.text = lineEndText;
			newTextLine.hardNewlineEnd = gEditLines[gEditLinesIndex].hardNewlineEnd;
			newTextLine.isQuoteLine = gEditLines[gEditLinesIndex].isQuoteLine;
			gEditLines.push(newTextLine);
			++gEditLinesIndex;
			returnObject.addedATextLine = true;
         returnObject.addedTextLineBelow = true;
		}
		else
		{
			// Create a new line containing lineEndText and splice it into
			// gEditLines on the next line.  Then place the cursor at the
			// start of that line.
			var oldIndex = gEditLinesIndex++;
			var newTextLine = new TextLine();
			newTextLine.text = lineEndText;
			newTextLine.hardNewlineEnd = gEditLines[oldIndex].hardNewlineEnd;
			newTextLine.isQuoteLine = gEditLines[oldIndex].isQuoteLine;
			// If the user pressed enter at the beginning of a line, then a new
			// blank line will be inserted above, so we want to make sure its
			// isQuoteLine property is set to false.
			if (gTextLineIndex == 0)
            gEditLines[oldIndex].isQuoteLine = false;
         // Splice the new text line into gEditLines at gEditLinesIndex.
			gEditLines.splice(gEditLinesIndex, 0, newTextLine);
         returnObject.addedATextLine = true;
		}

		gTextLineIndex = 0;
		pCurpos.x = gEditLeft;
		// Update the vertical cursor position.
		// If the cursor is at the bottom row, then we need
		// to scroll the message down by 1 line.  Otherwise,
		// we can simply increment pCurpos.y.
		if (pCurpos.y == gEditBottom)
		{
			displayEditLines(gEditTop, gEditLinesIndex-(gEditBottom-gEditTop),
			                 gEditBottom, true, true);
			returnObject.displayedEditlines = true;
      }
		else
			++pCurpos.y;
		// Figure out the current word length.
		// Look for a space in lineEndText.  If a space is found,
		// the word length is the length of the word up until the
		// space.  If a space is not found, then the word length
		// is the entire length of lineEndText.
		var spacePos = lineEndText.indexOf(" ");
		if (spacePos > -1)
			pCurrentWordLength = spacePos;
		else
			pCurrentWordLength = lineEndText.length;
	}

	// Set some stuff in the return object, and return it.
	returnObject.x = pCurpos.x;
	returnObject.y = pCurpos.y;
	returnObject.currentWordLength = pCurrentWordLength;
	return returnObject;
}

// This function handles quote selection and is called by doEditLoop().
//
// Parameters:
//  pCurpos: An object containing x and y values representing the
//           cursor position.
//  pCurrentWordLength: The length of the current word that has been typed
//
// Return value: An object containing the following properties:
//               x and y: The horizontal and vertical cursor position
//               timedOut: Whether or not the user input timed out (boolean)
//               currentWordLength: The length of the current word
function doQuoteSelection(pCurpos, pCurrentWordLength)
{
   // Create the return object
   var retObj = new Object();
   retObj.x = pCurpos.x;
   retObj.y = pCurpos.y;
   retObj.timedOut = false;
   retObj.currentWordLength = pCurrentWordLength;

   // Note: Quote lines are in the gQuoteLines array, where each element is
   // a string.

   // If gQuoteLines is empty, then we have nothing to do, so just return.
   if ((gQuoteLines.length == 0) || !gUseQuotes)
      return retObj;

   // Set up some variables
   var curpos = new Object();
   curpos.x = pCurpos.x;
   curpos.y = pCurpos.y;
   // Make the quote window's height about 42% of the edit area.
   const quoteWinHeight = Math.floor(gEditHeight * 0.42) + 1;
   // The first and last lines on the screen where quote lines are written
   const quoteTopScreenRow = console.screen_rows - quoteWinHeight + 2;
   const quoteBottomScreenRow = console.screen_rows - 2;
   // Quote window parameters
   const quoteWinTopScreenRow = quoteTopScreenRow-1;
   const quoteWinWidth = gEditRight - gEditLeft + 1;

   // Display the top border of the quote window.
   fpDrawQuoteWindowTopBorder(quoteWinHeight, gEditLeft, gEditRight);

   // Display the remainder of the quote window, with the quote lines in it.
   displayQuoteWindowLines(gQuoteLinesTopIndex, quoteWinHeight, quoteWinWidth, true, gQuoteLinesIndex);

   // Position the cursor at the currently-selected quote line.
   var screenLine = quoteTopScreenRow + (gQuoteLinesIndex - gQuoteLinesTopIndex);
   console.gotoxy(gEditLeft, screenLine);

   // User input loop
   var quoteLine = getQuoteTextLine(gQuoteLinesIndex, quoteWinWidth);
   retObj.timedOut = false;
   var userInput = null;
   var continueOn = true;
   while (continueOn)
   {
		// Get a key, and time out after 1 minute.
		userInput = console.inkey(0, 100000);
		if (userInput == "")
		{
			// The input timeout was reached.  Abort.
			retObj.timedOut = true;
			continueOn = false;
			break;
		}

		// If we got here, that means the user input didn't time out.
		switch (userInput)
		{
         case KEY_UP:
            // Go up 1 quote line
            if (gQuoteLinesIndex > 0)
            {
               // If the cursor is at the topmost position, then
               // we need to scroll up 1 line in gQuoteLines.
               if (screenLine == quoteTopScreenRow)
               {
                  --gQuoteLinesIndex;
                  --gQuoteLinesTopIndex;
                  quoteLine = getQuoteTextLine(gQuoteLinesIndex, quoteWinWidth);
                  // Redraw the quote lines in the quote window.
                  displayQuoteWindowLines(gQuoteLinesIndex, quoteWinHeight, quoteWinWidth,
                                          true, gQuoteLinesIndex);
                  // Put the cursor back where it should be.
                  console.gotoxy(gEditLeft, screenLine);
               }
               // If the cursor is below the topmost position, then
               // we can just go up 1 line.
               else if (screenLine > quoteTopScreenRow)
               {
                  // Write the current quote line using the normal color
                  // Note: This gets the quote line again using getQuoteTextLine()
                  // so that the color codes in the line will be correct.
                  quoteLine = getQuoteTextLine(gQuoteLinesIndex, quoteWinWidth);
                  console.gotoxy(gEditLeft, screenLine);
                  printf(gFormatStrWithAttr, gQuoteWinTextColor, quoteLine);

                  // Go up one line and display that quote line in the
                  // highlighted color.
                  --screenLine;
                  --gQuoteLinesIndex;
                  quoteLine = strip_ctrl(getQuoteTextLine(gQuoteLinesIndex, quoteWinWidth));
                  console.gotoxy(gEditLeft, screenLine);
                  printf(gFormatStrWithAttr, gQuoteLineHighlightColor, quoteLine);

                  // Make sure the cursor is where it should be.
                  console.gotoxy(gEditLeft, screenLine);
               }
            }
            break;
         case KEY_DOWN:
            // Go down 1 line in the quote window.
            var downRetObj = moveDownOneQuoteLine(gQuoteLinesIndex, screenLine,
                                                  quoteWinHeight, quoteWinWidth,
                                                  quoteBottomScreenRow);
            gQuoteLinesIndex = downRetObj.quoteLinesIndex;
            screenLine = downRetObj.screenLine;
            quoteLine = downRetObj.quoteLine;
            break;
         case KEY_ENTER:
            // numTimesToMoveDown specifies how many times to move the cursor
            // down after inserting the quote line into the message.
            var numTimesToMoveDown = 1;

            // Insert the quote line into gEditLines after the current gEditLines index.
            var insertedBelow = insertLineIntoMsg(gEditLinesIndex, quoteLine, true, true);
            if (insertedBelow)
            {
               // The cursor will need to be moved down 1 more line.
               // So, increment numTimesToMoveDown, and set curpos.x
               // and gTextLineIndex to the beginning of the line.
               ++numTimesToMoveDown;
               curpos.x = gEditLeft;
               gTextLineIndex = 0;
               retObj.currentWordLength = getWordLength(gEditLinesIndex, gTextLineIndex);
            }
            else
               retObj.currentWordLength = 0;

            // Refresh the part of the message that needs to be refreshed on the
            // screen (above the quote window).
            if (curpos.y < quoteTopScreenRow-1)
               displayEditLines(curpos.y, gEditLinesIndex, quoteTopScreenRow-2, false, true);

            gEditLinesIndex += numTimesToMoveDown;

            // Go down one line in the quote window.
            var tempReturnObj = moveDownOneQuoteLine(gQuoteLinesIndex, screenLine,
                                              quoteWinHeight, quoteWinWidth,
                                              quoteBottomScreenRow);
            gQuoteLinesIndex = tempReturnObj.quoteLinesIndex;
            screenLine = tempReturnObj.screenLine;
            quoteLine = tempReturnObj.quoteLine;

            // Move the cursor down as specified by numTimesToMoveDown.  If
            // the cursor is at the bottom of the edit area, then refresh
            // the message on the screen, scrolled down by one line.
            for (var i = 0; i < numTimesToMoveDown; ++i)
            {
               if (curpos.y == gEditBottom)
               {
                  // Refresh the message on the screen, scrolled down by
                  // one line, but only if this is the last time we're
                  // doing this (for efficiency).
                  if (i == numTimesToMoveDown-1)
                  {
                     displayEditLines(gEditTop, gEditLinesIndex-(gEditBottom-gEditTop),
                                      quoteTopScreenRow-2, false, true);
                  }
               }
               else
                  ++curpos.y;
            }
            break;
         // ESC or CTRL-Q: Stop quoting
         case KEY_ESC:
         case CTRL_Q:
            // Quit out of the input loop (get out of quote mode).
            continueOn = false;
            break;
		}
   }

   // We've exited quote mode.  Refresh the message text on the screen.  Note:
   // This will refresh only the quote window portion of the screen if the
   // cursor row is at or below the top of the quote window, and it will also
   // refresh the screen if the cursor row is above the quote window.
   displayEditLines(quoteWinTopScreenRow, gEditLinesIndex-(curpos.y-quoteWinTopScreenRow),
                    gEditBottom, true, true);

   // Draw the bottom edit border to erase the bottom border of the
   // quote window.
   fpDisplayTextAreaBottomBorder(gEditBottom+1, gUseQuotes, gEditLeft, gEditRight,
                                 gInsertMode, gConfigSettings.allowColorSelection);

   // Make sure the color is correct for editing.
   //console.print("n" + gTextAttrs);
   console.print(chooseEditColor());
   // Put the cursor where it should be.
   console.gotoxy(curpos);

   // Set the settings in the return object, and return it.
	retObj.x = curpos.x;
	retObj.y = curpos.y;
	return retObj;
}

// Helper for doQuoteSelection(): This function moves the quote selection
// down one line and updates the quote window.
//
// Parameters:
//  pQuoteLinesIndex: The index of the current line in gQuoteLines
//  pScreenLine: The vertical position of the cursor on the screen
//  pQuoteWinHeight: The height of the quote window
//  pQuoteWinWidth: The width of the quote window
//  pQuoteBottomScreenLine: The bottommost screen line where quote lines are displayed
function moveDownOneQuoteLine(pQuoteLinesIndex, pScreenLine, pQuoteWinHeight, pQuoteWinWidth,
                               pQuoteBottomScreenLine)
{
   // Create the return object
   var returnObj = new Object();
   returnObj.quoteLinesIndex = pQuoteLinesIndex;
   returnObj.screenLine = pScreenLine;
   returnObj.quoteLine = "";

   // If the current quote line is above the last one, then we can
   // move down one quote line.
   if (pQuoteLinesIndex < gQuoteLines.length-1)
   {
      // If the cursor is at the bottommost position, then
      // we need to scroll up 1 line in gQuoteLines.
      if (pScreenLine == pQuoteBottomScreenLine)
      {
         ++pQuoteLinesIndex;
         ++gQuoteLinesTopIndex;
         returnObj.quoteLine = getQuoteTextLine(pQuoteLinesIndex, pQuoteWinWidth);
         // Redraw the quote lines in the quote window.
         var topQuoteIndex = pQuoteLinesIndex - pQuoteWinHeight + 4;
         displayQuoteWindowLines(topQuoteIndex, pQuoteWinHeight, pQuoteWinWidth, true,
                                 pQuoteLinesIndex);
         // Put the cursor back where it should be.
         console.gotoxy(gEditLeft, pScreenLine);
      }
      // If the cursor is above the bottommost position, then
      // we can just go down 1 line.
      else if (pScreenLine < pQuoteBottomScreenLine)
      {
         // Write the current quote line using the normal color.
         // Note: This gets the quote line again using getQuoteTextLine()
         // so that the color codes in the line will be correct.
         console.gotoxy(gEditLeft, pScreenLine);
         returnObj.quoteLine = getQuoteTextLine(pQuoteLinesIndex, pQuoteWinWidth);
         printf(gFormatStrWithAttr, gQuoteWinTextColor, returnObj.quoteLine);

         // Go down one line and display that quote line in the
         // highlighted color.
         ++pScreenLine;
         ++pQuoteLinesIndex;
         returnObj.quoteLine = getQuoteTextLine(pQuoteLinesIndex, pQuoteWinWidth);
         console.gotoxy(gEditLeft, pScreenLine);
         printf(gFormatStrWithAttr, gQuoteLineHighlightColor, returnObj.quoteLine);

         // Put the cursor back where it should be.
         console.gotoxy(gEditLeft, pScreenLine);
      }
   }
   else // This else case is for when we're already on the last quote line.
      returnObj.quoteLine = getQuoteTextLine(pQuoteLinesIndex, pQuoteWinWidth);

   // Make sure the properties of returnObj have the correct
   // values (except quoteLine, which is already set), and
   // return the returnObj.
   returnObj.quoteLinesIndex = pQuoteLinesIndex;
   returnObj.screenLine = pScreenLine;
   return returnObj;
}

// Helper for doQuoteSelection(): This displays the quote window, except for its
// top border.
//
// Parameters:
//  pQuoteLinesIndex: The index into gQuoteLines to start at.  The quote line
//                    at this index will be displayed at the top of the quote
//                    window.
//  pQuoteWinHeight: The height of the quote window
//  pQuoteWinWidth: The width of the quote window
//  pDrawBottomBorder: Whether or not to draw the bottom border of the quote
//                     window.
//  pHighlightIndex: Optional - An index of a quote line to highlight.
function displayQuoteWindowLines(pQuoteLinesIndex, pQuoteWinHeight, pQuoteWinWidth, pDrawBottomBorder, pHighlightIndex)
{
   var quoteLinesIndex = pQuoteLinesIndex;
   var quoteLine = ""; // A line of text from gQuoteLines
   var screenLine = console.screen_rows - pQuoteWinHeight + 2;
   if (gQuoteLines.length > 0)
   {
      var color = "";     // The color to use when writing the text
      var lineLength = 0; // Length of a quote line
      while ((quoteLinesIndex < gQuoteLines.length) && (screenLine < console.screen_rows-1))
      {
         quoteLine = getQuoteTextLine(quoteLinesIndex, pQuoteWinWidth);
         // Go to the line on screen and display the quote line text.
         console.gotoxy(gEditLeft, screenLine);
         // If pHighlightIndex is valid, and if quoteLinesIndex matches
         // pHighlightIndex, then use the highlight color for this quote line.
         if ((pHighlightIndex != null) && (pHighlightIndex >= 0) && (pHighlightIndex < gQuoteLines.length))
         {
            if (quoteLinesIndex == pHighlightIndex)
            {
               color = gQuoteLineHighlightColor;
               quoteLine = quoteLine;
            }
            else
               color = gQuoteWinTextColor;
         }
         else
         {
            color = gQuoteWinTextColor;
            quoteLine = quoteLine;
         }
         // Write the quote line, and fill the rest of the line with spaces.
         printf(gFormatStrWithAttr, color, quoteLine);

         ++quoteLinesIndex;
         ++screenLine;
      }
   }
   // Fill the remainder of the quote window area
   for (; screenLine < console.screen_rows-1; ++screenLine)
   {
      console.gotoxy(gEditLeft, screenLine);
      printf(gFormatStrWithAttr, gQuoteWinTextColor, "");
   }

   // If pDrawBottomBorder is true, then display the bottom border of the
   // quote window.
   if (pDrawBottomBorder)
   {
      console.gotoxy(gEditLeft, screenLine);
      fpDrawQuoteWindowBottomBorder(gEditLeft, gEditRight);
   }
}

// This function returns a line of text from gQuoteLines, with "> "
// added  to the front if it's not blank.  Also, the text line will
// be limited in length by the screen width.
//
// Parameters:
//  pIndex: The index of the quote line to retrieve
//  pMaxWidth: The maximum width of the line
//
// Return value: The line of text from gQuoteLines
function getQuoteTextLine(pIndex, pMaxWidth)
{
   var textLine = "";
   if ((pIndex >= 0) && (pIndex < gQuoteLines.length))
   {
      if (gConfigSettings.useQuoteLineInitials)
      {
         if ((gQuoteLines[pIndex] != null) && (gQuoteLines[pIndex].length > 0))
          textLine = gQuoteLines[pIndex].substr(0, pMaxWidth-1);
      }
      else
      {
         if ((gQuoteLines[pIndex] != null) && (gQuoteLines[pIndex].length > 0))
            textLine = quote_msg(gQuoteLines[pIndex], pMaxWidth-1, gQuotePrefix);
      }
   }
   return textLine;
}

// This function deletes the current edit line.  This function is called
// by doEditLoop().
//
// Parameters:
//  pCurpos: An object containing the x and y cursor position.
//
// Return value: An object containing the following properties:
//               x: The horizontal component of the cursor location
//               y: The vertical component of the cursor location
//               currentWordLength: The length of the current word
function doDeleteLine(pCurpos)
{
   // Construct the object that we'll be returning
   var retObj = new Object();
   retObj.x = pCurpos.x;
   retObj.y = pCurpos.y;
   retObj.currentWordLength = 0;

   // Remove the current line from gEditLines.  If we're on the last line,
   // then we'll need to add a blank line to gEditLines.  We'll also need
   // to refresh the edit lines on the screen.
   if (gEditLinesIndex == gEditLines.length-1)
   {
      // We're on the last line.  Remove it & replace it with a new line.
      gEditLines.splice(gEditLinesIndex, 1, new TextLine());
      // Refresh (clear) the line on the screen
      displayEditLines(pCurpos.y, gEditLinesIndex, pCurpos.y, true, true);
      console.gotoxy(gEditLeft, pCurpos.y);
      printf(gFormatStr, "");
   }
   else
   {
      // We weren't on the last line.  Remove the current line and get the
      // word length, and then refresh the message on the screen.
      gEditLines.splice(gEditLinesIndex, 1);
      displayEditLines(pCurpos.y, gEditLinesIndex, gEditBottom, true, true);
      // Update the current word length
      retObj.currentWordLength = getWordLength(gEditLinesIndex, 0);

      // If there is a line above the current line, then set its hardNewlineEnd
      // to true.  This is for a scenario where the user deletes a line in the
      // middle of their message - If the user goes back up to the previous line
      // and starts typing in the middle and SlyEdit has to wrap it to the next
      // line, SlyEdit would basically remove a line without this change.
      if (gEditLinesIndex > 0)
         gEditLines[gEditLinesIndex-1].hardNewlineEnd = true;
   }

   // Adjust global message parameters, make sure the cursor position is
   // correct in retObj, and place the cursor where it's supposed to be.
   gTextLineIndex = 0;
   retObj.x = gEditLeft;
   console.gotoxy(retObj.x, retObj.y);

   return retObj;
}

// Toggles insert mode between insert and overwrite mode and updates it
// on the screen.  Insert/overwrite mode is signified by gInsertMode
// (either "INS" or "OVR");
//
// Parameters:
//  pCurpos: An object containing the cursor's position (X and Y coordinates).
//           The cursor will be returned here when finished.
function toggleInsertMode(pCurpos)
{
   // Change gInsertMode, and then refresh it on the screen.
   gInsertMode = inInsertMode() ? "OVR" : "INS";
   fpUpdateInsertModeOnScreen(gEditRight, gEditBottom, gInsertMode);
   if ((pCurpos != null) && (typeof(pCurpos) != "undefined"))
      console.gotoxy(pCurpos);
}

// Displays the contents of the gEditLines array, starting at a given
// line on the screen and index into the array.
//
// Parameters:
//  pStartScreenRow: The line on the screen at which to start printing the
//               message lines (1-based)
//  pArrayIndex: The starting index to use for the message lines array
//               (0-based)
//  pEndScreenRow: Optional.  This specifies the row on the screen to stop
//                 at.  If this is not specified, this function will stop
//                 at the edit area's bottom row (gEditBottom).
//  pClearRemainingScreenRows: Optional.  This is a boolean that specifies
//                             whether or not to clear the remaining lines
//                             on the screen between the end of the message
//                             text and the last row on the screen.
//  pIgnoreEditAreaBuffer: Optional.  This is a boolean that specifies whether
//                         to always write the edit text regardless of gEditAreaBuffer.
//                         By default, gEditAreaBuffer is always checked.
function displayEditLines(pStartScreenRow, pArrayIndex, pEndScreenRow, pClearRemainingScreenRows,
                           pIgnoreEditAreaBuffer)
{
   // Make sure the array has lines in it, the given array index is valid, and
   // that the given line # is valid.  If not, then just return.
   if ((gEditLines.length == 0) || (pArrayIndex < 0) || (pStartScreenRow < 1) || (pStartScreenRow > gEditBottom))
      return;

   // Choose which ending screen row to use for displaying text,
   // pEndScreenRow or gEditBottom.
   var endScreenRow = (pEndScreenRow != null ? pEndScreenRow : gEditBottom);

	// Display the message lines
	console.print("n" + gTextAttrs);
	var screenLine = pStartScreenRow;
	var arrayIndex = pArrayIndex;
	while ((screenLine <= endScreenRow) && (arrayIndex < gEditLines.length))
	{
		// Print the text from the current line in gEditLines.  Note: Lines starting
		// with " >" are assumed to be quote lines - Display those lines with cyan
		// color and the normal lines with gTextAttrs.
		var color = gTextAttrs;
		// Note: gEditAreaBuffer is also used in clearMsgAreaToBottom().
		if ((gEditAreaBuffer[screenLine] != gEditLines[arrayIndex].text) || pIgnoreEditAreaBuffer)
      {
         // Choose the quote line color or the normal color for the line, then
         // display the line on the screen.
         color = (isQuoteLine(gEditLines, arrayIndex) ? gQuoteLineColor : gTextAttrs);
         console.gotoxy(gEditLeft, screenLine);
         printf(gFormatStrWithAttr, color, gEditLines[arrayIndex].text);
         gEditAreaBuffer[screenLine] = gEditLines[arrayIndex].text;
      }

		++screenLine;
		++arrayIndex;
	}
	if (arrayIndex > 0)
		--arrayIndex;
	// incrementLineBeforeClearRemaining stores whether or not we
	// should increment screenLine before clearing the remaining
	// lines in the edit area.
	var incrementLineBeforeClearRemaining = true;
	// If the array index is valid, and if the current line is shorter
	// than the edit area width, then place the cursor after the last
	// character in the line.
	if ((arrayIndex >= 0) && (arrayIndex < gEditLines.length) &&
	    (gEditLines[arrayIndex] != undefined) && (gEditLines[arrayIndex].text != undefined))
	{
      var lineLength = gEditLines[arrayIndex].length();
      if (lineLength < gEditWidth)
      {
         --screenLine;
         console.gotoxy(gEditLeft + gEditLines[arrayIndex].length(), screenLine);
      }
      else if ((lineLength == gEditWidth) || (lineLength == 0))
         incrementLineBeforeClearRemaining = false;
	}
	else
      incrementLineBeforeClearRemaining = false;

	// Edge case: If the current screen line is below the last line, then
	// clear the lines up until that point.
	var clearRemainingScreenLines = (pClearRemainingScreenRows != null ? pClearRemainingScreenRows : true);
	if (clearRemainingScreenLines && (screenLine <= endScreenRow))
	{
		console.print("n" + gTextAttrs);
		var screenLineBackup = screenLine; // So we can move the cursor back
		clearMsgAreaToBottom(incrementLineBeforeClearRemaining ? screenLine+1 : screenLine,
		                     pIgnoreEditAreaBuffer);
		// Move the cursor back to the end of the current text line.
		if (typeof(gEditLines[arrayIndex]) != "undefined")
         console.gotoxy(gEditLeft + gEditLines[arrayIndex].length(), screenLineBackup);
      else
         console.gotoxy(gEditLeft, screenLineBackup);
	}

	// Make sure the correct color is set for the current line.
	console.print(chooseEditColor());
}

// Clears the lines in the message area from a given line to the bottom.
//
// Parameters:
//  pStartLine: The line number at which to start clearing.
//  pIgnoreEditAreaBuffer: Optional.  This is a boolean that specifies whether
//                         to always write the edit text regardless of gEditAreaBuffer.
//                         By default, gEditAreaBuffer is always checked.
function clearMsgAreaToBottom(pStartLine, pIgnoreEditAreaBuffer)
{
   for (var screenLine = pStartLine; screenLine <= gEditBottom; ++screenLine)
   {
		// Note: gEditAreaBuffer is also used in displayEditLines().
      if ((gEditAreaBuffer[screenLine].length > 0) || pIgnoreEditAreaBuffer)
      {
         console.gotoxy(gEditLeft, screenLine);
         printf(gFormatStr, "");
         gEditAreaBuffer[screenLine] = "";
      }
   }
}

// Returns whether or not the message is empty (gEditLines may have lines in
// it, and this tests to see if they are all empty).
function messageIsEmpty()
{
	var msgEmpty = true;
	
	for (var i = 0; i < gEditLines.length; ++i)
	{
		if (gEditLines[i].length() > 0)
		{
			msgEmpty = false;
			break;
		}
	}

	return msgEmpty;
}

// Displays a part of the message text in a rectangle on the screen.  This
// is useful for refreshing part of the message area that may have been
// written over (i.e., by a text dialog).
//
// Parameters:
//  pX: The upper-left X coordinate
//  pY: The upper-left Y coordinate
//  pWidth: The width of the rectangle
//  pHeight: The height of the rectangle
//  pEditLinesIndex: The starting index to use with gEditLines
//  pClearExtraWidth: Boolean - Optional.  If true, then space after the end of the line
//                    up to the specified width will be cleared.  Defaults to false.
function displayMessageRectangle(pX, pY, pWidth, pHeight, pEditLinesIndex, pClearExtraWidth)
{
   // If any of the parameters are out of bounds, then just return without
   // doing anything.
   if ((pX < gEditLeft) || (pY < gEditTop) || (pWidth < 0) || (pHeight < 0) || (pEditLinesIndex < 0))
      return;

   // If pWidth is too long with the given pX, then fix it.
   if (pWidth > (gEditRight - pX + 1))
      pWidth = gEditRight - pX + 1;
   // If pHeight is too much with the given pY, then fix it.
   if (pHeight > (gEditBottom - pY + 1))
      pHeight = gEditBottom - pY + 1;

   // Calculate the index into the edit line using pX and gEditLeft.  This
   // assumes that pX is within the edit area (and it should be).
   const editLineIndex = pX - gEditLeft;

   // Go to the given position on the screen and output the message text.
   var messageStr = ""; // Will contain a portion of the message text
   var screenY = pY;
   var editLinesIndex = pEditLinesIndex;
   var formatStr = "%-" + pWidth + "s";
   var actualLenWritten = 0; // Actual length of text written for each line
   for (var rectangleLine = 0; rectangleLine < pHeight; ++rectangleLine)
   {
      // Output the correct color for the line
      console.print("n" + (isQuoteLine(gEditLines, editLinesIndex) ? gQuoteLineColor : gTextAttrs));
      // Go to the position on the screen
      screenY = pY + rectangleLine;
      console.gotoxy(pX, screenY);
      // Display the message text.  If the current edit line is valid,
      // then print it; otherwise, just print spaces to blank out the line.
      if (typeof(gEditLines[editLinesIndex]) != "undefined")
      {
         actualLenWritten = printEditLine(editLinesIndex, false, editLineIndex, pWidth);
         // If pClearExtraWidth is true, then if the box width is longer than
         // the text that was written, then output spaces to clear the rest
         // of the line to erase the rest of the box line.
         if (pClearExtraWidth)
         {
            if (pWidth > actualLenWritten)
               printf("%" + +(pWidth-actualLenWritten) + "s", "");
         }
      }
      else
         printf(formatStr, "");

      ++editLinesIndex;
   }
}

// Displays the DCTEdit-style ESC menu and handles user input from that menu.
// This is used by the main input loop.
//
// Parameters:
//  pCurpos: The current cursor position
//  pEditLineDiff: The difference between the current edit line and the top of
//                 the edit area.
//  pCurrentWordLength: The length of the current word
//
// Return value: An object containing values to be used by the main input loop.
//               The object will contain these values:
//                 returnCode: The value to use as the editor's return code
//                 continueOn: Whether or not the input loop should continue
//                 x: The horizontal component of the cursor position
//                 y: The vertical component of the cursor position
//                 currentWordLength: The length of the current word
function handleDCTESCMenu(pCurpos, pCurrentWordLength)
{
   var returnObj = new Object();
   returnObj.returnCode = 0;
   returnObj.continueOn = true;
   returnObj.x = pCurpos.x;
   returnObj.y = pCurpos.y;
   returnObj.currentWordLength = pCurrentWordLength;

   // Call doDCTMenu() to display the DCT Edit menu and get the
   // user's choice.
   var editLineDiff = pCurpos.y - gEditTop;
   var menuChoice = doDCTMenu(gEditLeft, gEditRight, gEditTop,
                              displayMessageRectangle, gEditLinesIndex,
                              editLineDiff, gIsSysop, gCanCrossPost);
   // Take action according to the user's choice.
   // Save
   if ((menuChoice == "S") || (menuChoice == CTRL_Z) ||
       (menuChoice == DCTMENU_FILE_SAVE))
   {
      returnObj.returnCode = 0;
      returnObj.continueOn = false;
   }
   // Abort
   else if ((menuChoice == "A") || (menuChoice == CTRL_A) ||
             (menuChoice == DCTMENU_FILE_ABORT))
   {
      // Before aborting, ask they user if they really want to abort.
      if (promptYesNo("Abort message", false, "Abort"))
      {
         returnObj.returnCode = 1; // Aborted
         returnObj.continueOn = false;
      }
      else
      {
         // Make sure the edit color attribute is set back.
         //console.print("n" + gTextAttrs);
         console.print(chooseEditColor());
      }
   }
   // Toggle insert/overwrite mode
   else if ((menuChoice == CTRL_V) || (menuChoice == DCTMENU_EDIT_INSERT_TOGGLE))
      toggleInsertMode(pCurpos);
   // Import file (sysop only)
   else if (menuChoice == DCTMENU_SYSOP_IMPORT_FILE)
   {
      var retval = importFile(gIsSysop, pCurpos);
      returnObj.x = retval.x;
      returnObj.y = retval.y;
      returnObj.currentWordLength = retval.currentWordLength;
   }
   // Import file for sysop, or Insert/Overwrite toggle for non-sysop
   else if (menuChoice == "I")
   {
      if (gIsSysop)
      {
         var retval = importFile(gIsSysop, pCurpos);
         returnObj.x = retval.x;
         returnObj.y = retval.y;
         returnObj.currentWordLength = retval.currentWordLength;
      }
      else
         toggleInsertMode(pCurpos);
   }
   // Find text
   else if ((menuChoice == CTRL_F) || (menuChoice == "F") ||
             (menuChoice == DCTMENU_EDIT_FIND_TEXT))
   {
      var retval = findText(pCurpos);
      returnObj.x = retval.x;
      returnObj.y = retval.y;
   }
   // Command List
   else if ((menuChoice == "O") || (menuChoice == DCTMENU_HELP_COMMAND_LIST))
   {
      displayCommandList(true, true, true, gCanCrossPost, gIsSysop);
      clearEditAreaBuffer();
      fpRedrawScreen(gEditLeft, gEditRight, gEditTop, gEditBottom, gTextAttrs,
		               gInsertMode, gUseQuotes, gEditLinesIndex-(pCurpos.y-gEditTop),
		               displayEditLines);
   }
   // General help
   else if ((menuChoice == "G") || (menuChoice == DCTMENU_HELP_GENERAL))
   {
      displayGeneralHelp(true, true, true);
      clearEditAreaBuffer();
      fpRedrawScreen(gEditLeft, gEditRight, gEditTop, gEditBottom, gTextAttrs,
		               gInsertMode, gUseQuotes, gEditLinesIndex-(pCurpos.y-gEditTop),
		               displayEditLines);
   }
   // Program info
   else if ((menuChoice == "P") || (menuChoice == DCTMENU_HELP_PROGRAM_INFO))
   {
      displayProgramInfo(true, true, true);
      clearEditAreaBuffer();
      fpRedrawScreen(gEditLeft, gEditRight, gEditTop, gEditBottom, gTextAttrs,
		               gInsertMode, gUseQuotes, gEditLinesIndex-(pCurpos.y-gEditTop),
		               displayEditLines);
   }
   // Export the message
   else if ((menuChoice == "X") || (menuChoice == DCTMENU_SYSOP_EXPORT_FILE))
   {
      if (gIsSysop)
      {
         exportToFile(gIsSysop);
         console.gotoxy(returnObj.x, returnObj.y);
      }
   }
   // Edit the message
   else if ((menuChoice == "E") || (menuChoice == KEY_ESC))
   {
      // We don't need to do do anything in here.
   }
   // Cross-post
   else if ((menuChoice == CTRL_C) || (menuChoice == "C") ||
             (menuChoice == DCTMENU_CROSS_POST))
   {
      if (gCanCrossPost)
         doCrossPosting(pCurpos);
   }

   // Make sure the edit color attribute is set back.
   //console.print("n" + gTextAttrs);
   console.print(chooseEditColor());

   return returnObj;
}

// Displays the IceEdit-style ESC menu and handles user input from that menu.
// This is used by the main input loop.
//
// Parameters:
//  curpos: The current cursor position
//  pEditLineDiff: The difference between the current edit line and the top of
//                 the edit area.
//  pCurrentWordLength: The length of the current word
//
// Return value: An object containing values to be used by the main input loop.
//               The object will contain these values:
//                 returnCode: The value to use as the editor's return code
//                 continueOn: Whether or not the input loop should continue
//                 x: The horizontal component of the cursor position
//                 y: The vertical component of the cursor position
//                 currentWordLength: The length of the current word
function handleIceESCMenu(pCurpos, pCurrentWordLength)
{
   var returnObj = new Object();
   returnObj.returnCode = 0;
   returnObj.continueOn = true;
   returnObj.x = pCurpos.x;
   returnObj.y = pCurpos.y;
   returnObj.currentWordLength = pCurrentWordLength;

   // Call doIceESCMenu() to display the choices, and then take the
   // chosen action.
   var userChoice = doIceESCMenu(console.screen_rows, gCanCrossPost);
   switch (userChoice)
   {
      case ICE_ESC_MENU_SAVE:
         returnObj.returnCode = 0;
         returnObj.continueOn = false;
         break;
      case ICE_ESC_MENU_ABORT:
         // Before aborting, ask they user if they really want to abort.
         if (promptYesNo("Abort message", false, "Abort"))
         {
            returnObj.returnCode = 1; // Aborted
            returnObj.continueOn = false;
         }
         break;
      case ICE_ESC_MENU_EDIT:
         // Nothing needs to be done for this option.
         break;
      case ICE_ESC_MENU_HELP:
         displayProgramInfo(true, true, false);
         displayCommandList(false, false, true, gCanCrossPost, gIsSysop);
         clearEditAreaBuffer();
         fpRedrawScreen(gEditLeft, gEditRight, gEditTop, gEditBottom, gTextAttrs,
                        gInsertMode, gUseQuotes, gEditLinesIndex-(pCurpos.y-gEditTop),
                        displayEditLines);
         break;
      case ICE_ESC_MENU_CROSS_POST:
         if (gCanCrossPost)
            doCrossPosting(pCurpos);
         break;
   }

   // If the user didn't choose help, then we only need to refresh the bottom
   // row on the screen.
   if (userChoice != ICE_ESC_MENU_HELP)
      fpDisplayBottomHelpLine(console.screen_rows, gUseQuotes);

   // Make sure the edit color attribute is set back.
   //console.print("n" + gTextAttrs);
   console.print(chooseEditColor());

   return returnObj;
}

// Figures out and returns the length of a word in the message text,based on
// a given edit lines index and text line index.
//
// Parameters:
//  pEditLinesIndex: The index into the gEditLines array
//  pTextLineIndex: The index into the line's text
//
// Return value: The length of the word at the given indexes
function getWordLength(pEditLinesIndex, pTextLineIndex)
{
   // pEditLinesIndex and pTextLineIndex should be >= 0 before we can do
   // anything in this function.
   if ((pEditLinesIndex < 0) || (pTextLineIndex < 0))
      return 0;
   // Also, make sure gEditLines[pEditLinesIndex] is valid.
   if ((gEditLines[pEditLinesIndex] == null) || (typeof(gEditLines[pEditLinesIndex]) == "undefined"))
      return 0;

   // This function counts and returns the number of non-whitespace characters
   // before the current character.
   function countBeforeCurrentChar()
   {
      var charCount = 0;

      for (var i = pTextLineIndex-1; i >= 0; --i)
      {
         if (!/\s/.test(gEditLines[pEditLinesIndex].text.charAt(i)))
            ++charCount;
         else
            break;
      }

      return charCount;
   }

   var wordLen = 0;

   // If there are only characters to the left, or if the current
   // character is a space, then count before the current character.
   if ((pTextLineIndex == gEditLines[pEditLinesIndex].length()) ||
       (gEditLines[pEditLinesIndex].text.charAt(gTextLineIndex) == " "))
      wordLen = countBeforeCurrentChar();
   // If there are charactrs to the left and at the current line index,
   // then count to the left only if the current character is not whitespace.
   else if (pTextLineIndex == gEditLines[pEditLinesIndex].length()-1)
   {
      if (!/\s/.test(gEditLines[pEditLinesIndex].text.charAt(pTextLineIndex)))
         wordLen = countBeforeCurrentChar() + 1;
   }
   // If there are characters to the left and right, then count to the left
   // and right only if the current character is not whitespace.
   else if (pTextLineIndex < gEditLines[pEditLinesIndex].length()-1)
   {
      if (!/\s/.test(gEditLines[pEditLinesIndex].text.charAt(pTextLineIndex)))
      {
         // Count non-whitespace characters to the left, and include the current one.
         wordLen = countBeforeCurrentChar() + 1;
         // Count characters to the right.
         for (var i = pTextLineIndex+1; i < gEditLines[pEditLinesIndex].length(); ++i)
         {
            if (!/\s/.test(gEditLines[pEditLinesIndex].text.charAt(i)))
               ++wordLen;
            else
               break;
         }
      }
   }

   return wordLen;
}

// Inserts a string into gEditLines after a given index.
//
// Parameters:
//  pInsertLineIndex: The index for gEditLines at which to insert the string.
//  pString: The string to insert
//  pHardNewline: Whether or not to enable the hard newline flag for the line
//  pIsQuoteLine: Whether or not the line is a quote line
//
// Return value: Whether or not the line was inserted below the given index
//               (as opposed to above).
function insertLineIntoMsg(pInsertLineIndex, pString, pHardNewline, pIsQuoteLine)
{
   var insertedBelow = false;

   // Create the new text line
   var line = new TextLine();
   line.text = pString;
   line.hardNewlineEnd = false;
   if ((pHardNewline != null) && (typeof(pHardNewline) != "undefined"))
      line.hardNewlineEnd = pHardNewline;
   if ((pIsQuoteLine != null) && (typeof(pIsQuoteLine) != "undefined"))
      line.isQuoteLine = pIsQuoteLine;

   // If the current message line is empty, insert the quote line above
   // the current line.  Otherwise, insert the quote line below the
   // current line.
   if (typeof(gEditLines[pInsertLineIndex]) == "undefined")
      gEditLines.splice(pInsertLineIndex, 0, line);
   // Note: One time, I noticed an error with the following test:
   // gEditLines[pInsertLineIndex] has no properties
   // Thus, I added the above test to see if the edit line is valid.
   else if (gEditLines[pInsertLineIndex].length() == 0)
      gEditLines.splice(pInsertLineIndex, 0, line);
   else
   {
      // Insert the quote line below the given line index
      gEditLines.splice(pInsertLineIndex + 1, 0, line);
      // The current message line should have its hardNewlineEnd set
      // true so that the quote line won't get wrapped up.
      gEditLines[pInsertLineIndex].hardNewlineEnd = true;
      insertedBelow = true;
   }

   return insertedBelow;
}

// Prompts the user for a filename on the BBS computer and loads its contents
// into the message.  This is for sysops only!
//
// Parameters:
//  pIsSysop: Whether or not the user is the sysop
//  pCurpos: The current cursor position (with x and y properties)
//
// Return value: An object containing the following information:
//               x: The horizontal component of the cursor's location
//               y: The vertical component of the cursor's location
//               currentWordLength: The length of the current word
function importFile(pIsSysop, pCurpos)
{
   // Create the return object
   var retObj = new Object();
   retObj.x = pCurpos.x;
   retObj.y = pCurpos.y;
   retObj.currentWordLength = getWordLength(gEditLinesIndex, gTextLineIndex);

   // Don't let non-sysops do this.
   if (!pIsSysop)
      return retObj;

   var loadedAFile = false;
   // This loop continues to prompt the user until they enter a valid
   // filename or a blank string.
   var continueOn = true;
   while (continueOn)
   {
      // Go to the last row on the screen and prompt the user for a filename
      var promptText = "ncFile:h";
      var promptTextLen = strip_ctrl(promptText).length;
      console.gotoxy(1, console.screen_rows);
      console.cleartoeol("n");
      console.print(promptText);
      var filename = console.getstr(console.screen_columns-promptTextLen-1, K_NOCRLF);
      continueOn = (filename != "");
      if (continueOn)
      {
         filename = file_getcase(filename);
         if (filename != undefined)
         {
            // Open the file and insert its contents into the message.
            var inFile = new File(filename);
            if (inFile.exists && inFile.open("r"))
            {
               const maxLineLength = gEditWidth - 1; // Don't insert lines longer than this
               var fileLine;
               while (!inFile.eof)
               {
                  fileLine = inFile.readln(1024);
                  // fileLine should always be a string, but there seem to be
                  // situations where it isn't.  So if it's a string, we can
                  // insert text into gEditLines as normal.  If it's not a
                  // string, insert a blank line.
                  if (typeof(fileLine) == "string")
                  {
                     // Tab characters can cause problems, so replace tabs with 3 spaces.
                     fileLine = fileLine.replace(/\t/, "   ");
                     // Insert the line into the message, splitting up the line,
                     // if the line is longer than the edit area.
                     do
                     {
                        insertLineIntoMsg(gEditLinesIndex, fileLine.substr(0, maxLineLength),
                                          true, false);
                        fileLine = fileLine.substr(maxLineLength);
                        ++gEditLinesIndex;
                     } while (fileLine.length > maxLineLength);
                     // Edge case, if the line still has characters in it
                     if (fileLine.length > 0)
                     {
                        insertLineIntoMsg(gEditLinesIndex, fileLine, true, false);
                        ++gEditLinesIndex;
                     }
                  }
                  else
                  {
                     insertLineIntoMsg(gEditLinesIndex, "", true, false);
                     ++gEditLinesIndex;
                  }
               }
               inFile.close();

               // If the last text line is blank, then remove it.
               if (gEditLines[gEditLinesIndex].length() == 0)
               {
                  gEditLines.splice(gEditLinesIndex, 1);
                  --gEditLinesIndex;
               }

               loadedAFile = true;
               continueOn = false;
            }
            else // Unable to open the file
               writeWithPause(1, console.screen_rows, "yhUnable to open the file!", 1500);
         }
         else // Could not find the correct case for the file (it doesn't exist?)
            writeWithPause(1, console.screen_rows, "yhUnable to locate the file!", 1500);
      }
   }

   // Refresh the help line on the bottom of the screen
   fpDisplayBottomHelpLine(console.screen_rows, gUseQuotes);

   // If we loaded a file, then refresh the message text.
   if (loadedAFile)
   {
      // Insert a blank line into gEditLines so that the user ends up on a new
      // blank line.
      //displayEditLines(pScreenLine, pArrayIndex, pEndScreenRow, pClearRemainingScreenRows)
      // Figure out the index to start at in gEditLines
      var startIndex = 0;
      if (gEditLines.length > gEditHeight)
         startIndex = gEditLines.length - gEditHeight;
      // Refresh the message on the screen
      displayEditLines(gEditTop, startIndex, gEditBottom, true, true);

      // Set up the edit lines & text line index for the last line, and
      // place the cursor at the beginning of the last edit line.
      // If the last line is short enough, place the cursor at the end
      // of it.  Otherwise, append a new line and place the cursor there.
      if (gEditLines[gEditLinesIndex].length() < gEditWidth-1)
      {
         gEditLinesIndex = gEditLines.length - 1;
         gTextLineIndex = gEditLines[gEditLinesIndex].length();
         retObj.x = gEditLeft + gTextLineIndex;
         retObj.y = gEditBottom;
         if (gEditLines.length < gEditHeight)
            retObj.y = gEditTop + gEditLines.length - 1;
         retObj.currentWordLength = getWordLength(gEditLinesIndex, gTextLineIndex);
      }
      else
      {
         // Append a new line and place the cursor there
         gEditLines.push(new TextLine());
         gEditLinesIndex = gEditLines.length - 1;
         gTextLineIndex = 0;
         retObj.x = gEditLeft;
         retObj.y = gEditBottom;
         if (gEditLines.length < gEditHeight)
            retObj.y = gEditTop + gEditLines.length - 1;
         retObj.currentWordLength = 0;
      }
   }

   // Make sure the cursor is where it's supposed to be.
   console.gotoxy(retObj.x, retObj.y);

   return retObj;
}

// This function lets sysops export (save) the current message to
// a file.
//
// Parameters:
//  pIsSysop: Whether or not the user is the sysop
function exportToFile(pIsSysop)
{
   // Don't let non-sysops do this.
   if (!pIsSysop)
      return;

   // Go to the last row on the screen and prompt the user for a filename
   var promptText = "ncFile:h";
   var promptTextLen = strip_ctrl(promptText).length;
   console.gotoxy(1, console.screen_rows);
   console.cleartoeol("n");
   console.print(promptText);
   var filename = console.getstr(console.screen_columns-promptTextLen-1, K_NOCRLF);
   if (filename != "")
   {
      var outFile = new File(filename);
      if (outFile.open("w"))
      {
         const lastLineIndex = gEditLines.length - 1;
         for (var i = 0; i < gEditLines.length; ++i)
         {
            // Use writeln to write all lines with CRLF except the last line.
            if (i < lastLineIndex)
               outFile.writeln(gEditLines[i].text);
            else
               outFile.write(gEditLines[i].text);
         }
         outFile.close();
         writeWithPause(1, console.screen_rows, "mhMessage exported.", 1500);
      }
      else // Could not open the file for writing
         writeWithPause(1, console.screen_rows, "yhUnable to open the file for writing!", 1500);
   }
   else // No filename specified
      writeWithPause(1, console.screen_rows, "mhMessage not exported.", 1500);

   // Refresh the help line on the bottom of the screen
   fpDisplayBottomHelpLine(console.screen_rows, gUseQuotes);
}

// Performs a text search.
//
// Parameters:
//  pCurpos: The current cursor position (with x and y properties)
//
// Return value: An object containing the following properties:
//               x: The horizontal component of the cursor position
//               y: The vertical component of the cursor position
function findText(pCurpos)
{
   // Create the return object.
   var returnObj = new Object();
   returnObj.x = pCurpos.x;
   returnObj.y = pCurpos.y;

   // This function makes use of the following "static" variables:
   //  lastSearchText: The text searched for last
   //  searchStartIndex: The starting index for gEditLines that should
   //                    be used for the search
   if (typeof(findText.lastSearchText) == "undefined")
      findText.lastSearchText = "";
   if (typeof(findText.searchStartIndex) == "undefined")
      findText.searchStartIndex = 0;

   // Go to the last row on the screen and prompt the user for text to find
   var promptText = "ncText:h";
   var promptTextLen = strip_ctrl(promptText).length;
   console.gotoxy(1, console.screen_rows);
   console.cleartoeol("n");
   console.print(promptText);
   var searchText = console.getstr(console.screen_columns-promptTextLen-1, K_NOCRLF);

   // If the user's search is text is different from last time, then set the
   // starting gEditLines index to 0.  Also, update the last search text.
   if (searchText != findText.lastSearchText)
      findText.searchStartIndex = 0;
   findText.lastSearchText = searchText;

   // Search for the text.
   var caseSensitive = false; // Case-sensitive search?
   var textIndex = 0; // The index of the text in the edit lines
   if (searchText.length > 0)
   {
      // editLinesTopIndex is the index of the line currently displayed
      // at the top of the edit area, and also the line to be displayed
      // at the top of the edit area.
      var editLinesTopIndex = gEditLinesIndex - (pCurpos.y - gEditTop);

      // Look for the text in gEditLines
      var textFound = false;
      for (var i = findText.searchStartIndex; i < gEditLines.length; ++i)
      {
         if (caseSensitive)
            textIndex = gEditLines[i].text.indexOf(searchText);
         else
            textIndex = gEditLines[i].text.toUpperCase().indexOf(searchText.toUpperCase());
         // If the text was found in this line, then highlight it and
         // exit the search loop.
         if (textIndex > -1)
         {
            gTextLineIndex = textIndex;
            textFound = true;

            // If the line is above or below the edit area, then we'll need
            // to refresh the edit lines on the screen.  We also need to set
            // the cursor position to the proper place.
            returnObj.x = gEditLeft + gTextLineIndex;
            var refresh = false;
            if (i < editLinesTopIndex)
            {
               // The line is above the edit area.
               refresh = true;
               returnObj.y = gEditTop;
               editLinesTopIndex = i;
            }
            else if (i >= editLinesTopIndex + gEditHeight)
            {
               // The line is below the edit area.
               refresh = true;
               returnObj.y = gEditBottom;
               editLinesTopIndex = i - gEditHeight + 1;
            }
            else
            {
               // The line is inside the edit area.
               returnObj.y = pCurpos.y + (i - gEditLinesIndex);
            }

            gEditLinesIndex = i;

            if (refresh)
               displayEditLines(gEditTop, editLinesTopIndex, gEditBottom, true, true);

            // Highlight the found text on the line by briefly displaying it in a
            // different color.
            var highlightText = gEditLines[i].text.substr(textIndex, searchText.length);
            console.gotoxy(returnObj.x, returnObj.y);
            console.print("nk4" + highlightText);
            mswait(1500);
            console.gotoxy(returnObj.x, returnObj.y);
            //console.print(gTextAttrs + highlightText);
            console.print(chooseEditColor() + highlightText);

            // The next time the user searches with the same text, we'll want
            // to start searching at the next line.  Wrap around if necessary.
            findText.searchStartIndex = i + 1;
            if (findText.searchStartIndex >= gEditLines.length)
               findText.searchStartIndex = 0;

            break;
         }
      }

      // If the text wasn't found, tell the user.  Also, make sure searchStartIndex
      // is reset to 0.
      if (!textFound)
      {
         console.gotoxy(1, console.screen_rows);
         console.cleartoeol("n");
         console.print("yhThe text wasn't found!");
         mswait(1500);

         findText.searchStartIndex = 0;
      }
   }

   // Refresh the help line on the bottom of the screen
   fpDisplayBottomHelpLine(console.screen_rows, gUseQuotes);

   // Make sure the cursor is positioned where it should be.
   console.gotoxy(returnObj.x, returnObj.y);

   return returnObj;
}

// Returns whether we're in insert mode (if not, we're in overwrite mode).
function inInsertMode()
{
   return (gInsertMode == "INS");
}

// Returns either the normal edit color (gTextAttrs) or the quote line
// color (gQuoteLineColor), depending on whether the current edit line
// is a normal line or a quote line.
function chooseEditColor()
{
   return ("n" + (isQuoteLine(gEditLines, gEditLinesIndex) ? gQuoteLineColor : gTextAttrs));
}

// This function calculates the row on the screen to stop updating the
// message text.
//
// Parameters:
//  pY: The topmost row at which we'll start writing
//  pTopIndex: The topmost index in gEditLines
//
// Return value: The row on the screen to stop updating the
//               message text.
function calcBottomUpdateRow(pY, pTopIndex)
{
   var bottomScreenRow = gEditBottom;
   // Note: This is designed to return the screen row #
   // below the last message line.  To return the exact
   // bottommost screen row, subtract 1 from gEditLines.length-pTopIndex.
   var bottommost = (pY + (gEditLines.length-pTopIndex));
   if (bottomScreenRow > bottommost)
      bottomScreenRow = bottommost;
   return bottomScreenRow;
}

// This function updates the time on the screen and puts
// the cursor back to where it was.
function updateTime()
{
	if (typeof(updateTime.timeStr) == "undefined")
		updateTime.timeStr = getCurrentTimeStr();

	// If the current time has changed since the last time this
	// function was called, then update the time on the screen.
	var currentTime = getCurrentTimeStr();
	if (currentTime != updateTime.timeStr)
	{
		// Get the current cursor position so we can move
		// the cursor back there when we're done.
		var curpos = console.getxy();
		// Display the current time on the screen
		fpDisplayTime(currentTime);
		// Make sure the edit color attribute is set.
		console.print("n" + gTextAttrs);
		// Move the cursor back to where it was
		console.gotoxy(curpos);
		// Update this function's time variable
		updateTime.timeStr = currentTime;
	}
}

// This function lets the user change the text color and is called by doEditLoop().
//
// Parameters:
//  pCurpos: An object containing x and y values representing the
//           cursor position.
//  pCurrentWordLength: The length of the current word that has been typed
//
// Return value: An object containing the following properties:
//               x and y: The horizontal and vertical cursor position
//               timedOut: Whether or not the user input timed out (boolean)
//               currentWordLength: The length of the current word
function doColorSelection(pCurpos, pCurrentWordLength)
{
   // Create the return object
   var retObj = new Object();
	retObj.x = pCurpos.x;
	retObj.y = pCurpos.y;
	retObj.timedOut = false;
	retObj.currentWordLength = pCurrentWordLength;

   // Note: The current text color is stored in gTextAttrs

   var colorSelTopLine = console.screen_rows - 2;
   var curpos = new Object();
   curpos.x = 1;
   curpos.y = colorSelTopLine;
   console.gotoxy(curpos);
   console.print("nForeground: whK:nkBlack whR:nrRed whG:ngGreen whY:nyYellow whB:nbBlue whM:nmMagenta whC:ncCyan whW:nwWhite");
   console.cleartoeol("n");
   console.crlf();
   console.print("nBackground: wh0:n" + gTextAttrs + "0Blackn wh1:n" + gTextAttrs + "1Redn wh2:n" + gTextAttrs + "2Greenn wh3:n" + gTextAttrs + "3Yellown wh4:n" + gTextAttrs + "4Bluen wh5:n" + gTextAttrs + "5Magentan wh6:n" + gTextAttrs + "6Cyann wh7:n" + gTextAttrs + "7White");
   console.cleartoeol("n");
   console.crlf();
   console.clearline("n");
   console.print("Special: whH:n" + gTextAttrs + "hHigh Intensity wI:n" + gTextAttrs + "iBlinking nwhN:nNormal cþ nChoose Color: ");
   var attr = FORE_ATTR;
   var toggle = true;
   //var key = console.getkeys("KRGYBMCW01234567HIN").toString(); // Outputs a CR..  bad
   var key = console.getkey(K_UPPER|K_NOCRLF);
   switch (key)
   {
      // Foreground colors:
      case 'K': // Black
      case 'R': // Red
      case 'G': // Green
      case 'Y': // Yellow
      case 'B': // Blue
      case 'M': // Magenta
      case 'C': // Cyan
      case 'W': // White
         attr = FORE_ATTR;
         break;
      // Background colors:
      case '0': // Black
      case '1': // Red
      case '2': // Green
      case '3': // Yellow
      case '4': // Blue
      case '5': // Magenta
      case '6': // Cyan
      case '7': // White
         attr = BKG_ATTR;
         break;
      // Special attributes:
      case 'H': // High intensity
      case 'I': // Blinking
         attr = SPECIAL_ATTR;
         break;
      case 'N': // Normal
         gTextAttrs = "N";
         toggle = false;
         break;
      default:
         toggle = false;
         break;
   }
   if (key != "Q")
   {
      if (toggle)
      {
         gTextAttrs = toggleAttr(attr, gTextAttrs, key);
         // TODO: Set the attribute in the current text line
      }
   }


   // Display the parts of the screen text that we covered up with the
   // color selection: Message edit lines, bottom border, and bottom help line.
   displayEditLines(colorSelTopLine, gEditLinesIndex-(gEditBottom-colorSelTopLine),
                    gEditBottom, true, true);
   fpDisplayTextAreaBottomBorder(gEditBottom+1, gUseQuotes, gEditLeft, gEditRight,
                                 gInsertMode, gConfigSettings.allowColorSelection);
   fpDisplayBottomHelpLine(console.screen_rows, gUseQuotes);

   console.print(gTextAttrs);

   // Move the cursor to where it should be before returning
   curpos.x = pCurpos.x;
   curpos.y = pCurpos.y;
   console.gotoxy(curpos);

   // This code was copied from doQuoteSelection().
/*
   // Set up some variables
   var curpos = new Object();
   curpos.x = pCurpos.x;
   curpos.y = pCurpos.y;
   const quoteWinHeight = 8;
   // The first and last lines on the screen where quote lines are written
   const quoteTopScreenRow = console.screen_rows - quoteWinHeight + 2;
   const quoteBottomScreenRow = console.screen_rows - 2;
   // Quote window parameters
   const quoteWinTopScreenRow = quoteTopScreenRow-1;
   const quoteWinWidth = gEditRight - gEditLeft + 1;

   // Display the top border of the quote window.
   fpDrawQuoteWindowTopBorder(quoteWinHeight, gEditLeft, gEditRight);

   // Display the remainder of the quote window, with the quote lines in it.
   displayQuoteWindowLines(gQuoteLinesTopIndex, quoteWinHeight, quoteWinWidth, true, gQuoteLinesIndex);

   // Position the cursor at the currently-selected quote line.
   var screenLine = quoteTopScreenRow + (gQuoteLinesIndex - gQuoteLinesTopIndex);
   console.gotoxy(gEditLeft, screenLine);

   // User input loop
   var quoteLine = getQuoteTextLine(gQuoteLinesIndex, quoteWinWidth);
   retObj.timedOut = false;
   var userInput = null;
   var continueOn = true;
   while (continueOn)
   {
		// Get a key, and time out after 1 minute.
		userInput = console.inkey(0, 100000);
		if (userInput == "")
		{
			// The input timeout was reached.  Abort.
			retObj.timedOut = true;
			continueOn = false;
			break;
		}

		// If we got here, that means the user input didn't time out.
		switch (userInput)
		{
         case KEY_UP:
            // Go up 1 quote line
            if (gQuoteLinesIndex > 0)
            {
               // If the cursor is at the topmost position, then
               // we need to scroll up 1 line in gQuoteLines.
               if (screenLine == quoteTopScreenRow)
               {
                  --gQuoteLinesIndex;
                  --gQuoteLinesTopIndex;
                  quoteLine = getQuoteTextLine(gQuoteLinesIndex, quoteWinWidth);
                  // Redraw the quote lines in the quote window.
                  displayQuoteWindowLines(gQuoteLinesIndex, quoteWinHeight, quoteWinWidth,
                                          true, gQuoteLinesIndex);
                  // Put the cursor back where it should be.
                  console.gotoxy(gEditLeft, screenLine);
               }
               // If the cursor is below the topmost position, then
               // we can just go up 1 line.
               else if (screenLine > quoteTopScreenRow)
               {
                  // Write the current quote line using the normal color
                  // Note: This gets the quote line again using getQuoteTextLine()
                  // so that the color codes in the line will be correct.
                  quoteLine = getQuoteTextLine(gQuoteLinesIndex, quoteWinWidth);
                  console.gotoxy(gEditLeft, screenLine);
                  printf(gFormatStrWithAttr, gQuoteWinTextColor, quoteLine);

                  // Go up one line and display that quote line in the
                  // highlighted color.
                  --screenLine;
                  --gQuoteLinesIndex;
                  quoteLine = strip_ctrl(getQuoteTextLine(gQuoteLinesIndex, quoteWinWidth));
                  console.gotoxy(gEditLeft, screenLine);
                  printf(gFormatStrWithAttr, gQuoteLineHighlightColor, quoteLine);

                  // Make sure the cursor is where it should be.
                  console.gotoxy(gEditLeft, screenLine);
               }
            }
            break;
         case KEY_DOWN:
            // Go down 1 line in the quote window.
            var downRetObj = moveDownOneQuoteLine(gQuoteLinesIndex, screenLine,
                                                  quoteWinHeight, quoteWinWidth,
                                                  quoteBottomScreenRow);
            gQuoteLinesIndex = downRetObj.quoteLinesIndex;
            screenLine = downRetObj.screenLine;
            quoteLine = downRetObj.quoteLine;
            break;
         case KEY_ENTER:
            // numTimesToMoveDown specifies how many times to move the cursor
            // down after inserting the quote line into the message.
            var numTimesToMoveDown = 1;

            // Insert the quote line into gEditLines after the current gEditLines index.
            var insertedBelow = insertLineIntoMsg(gEditLinesIndex, quoteLine, true, true);
            if (insertedBelow)
            {
               // The cursor will need to be moved down 1 more line.
               // So, increment numTimesToMoveDown, and set curpos.x
               // and gTextLineIndex to the beginning of the line.
               ++numTimesToMoveDown;
               curpos.x = gEditLeft;
               gTextLineIndex = 0;
               retObj.currentWordLength = getWordLength(gEditLinesIndex, gTextLineIndex);
            }
            else
               retObj.currentWordLength = 0;

            // Refresh the part of the message that needs to be refreshed on the
            // screen (above the quote window).
            if (curpos.y < quoteTopScreenRow-1)
               displayEditLines(curpos.y, gEditLinesIndex, quoteTopScreenRow-2, false, true);

            gEditLinesIndex += numTimesToMoveDown;

            // Go down one line in the quote window.
            var tempReturnObj = moveDownOneQuoteLine(gQuoteLinesIndex, screenLine,
                                              quoteWinHeight, quoteWinWidth,
                                              quoteBottomScreenRow);
            gQuoteLinesIndex = tempReturnObj.quoteLinesIndex;
            screenLine = tempReturnObj.screenLine;
            quoteLine = tempReturnObj.quoteLine;

            // Move the cursor down as specified by numTimesToMoveDown.  If
            // the cursor is at the bottom of the edit area, then refresh
            // the message on the screen, scrolled down by one line.
            for (var i = 0; i < numTimesToMoveDown; ++i)
            {
               if (curpos.y == gEditBottom)
               {
                  // Refresh the message on the screen, scrolled down by
                  // one line, but only if this is the last time we're
                  // doing this (for efficiency).
                  if (i == numTimesToMoveDown-1)
                  {
                     displayEditLines(gEditTop, gEditLinesIndex-(gEditBottom-gEditTop),
                                      quoteTopScreenRow-2, false, true);
                  }
               }
               else
                  ++curpos.y;
            }
            break;
         // ESC or CTRL-Q: Stop quoting
         case KEY_ESC:
         case CTRL_Q:
            // Quit out of the input loop (get out of quote mode).
            continueOn = false;
            break;
		}
   }

   // We've exited quote mode.  Refresh the message text on the screen.  Note:
   // This will refresh only the quote window portion of the screen if the
   // cursor row is at or below the top of the quote window, and it will also
   // refresh the screen if the cursor row is above the quote window.
   displayEditLines(quoteWinTopScreenRow, gEditLinesIndex-(curpos.y-quoteWinTopScreenRow),
                    gEditBottom, true, true);

   // Draw the bottom edit border to erase the bottom border of the
   // quote window.
   fpDisplayTextAreaBottomBorder(gEditBottom+1, gUseQuotes, gEditLeft, gEditRight,
                                 gInsertMode, gConfigSettings.allowColorSelection);

   // Make sure the color is correct for editing.
   //console.print("n" + gTextAttrs);
   console.print(chooseEditColor());
   // Put the cursor where it should be.
   console.gotoxy(curpos);
*/
   // Set the settings in the return object, and return it.
	retObj.x = curpos.x;
	retObj.y = curpos.y;
	return retObj;
}

// For the cross-posting UI: Draws the initial top border of
// the selection box
//
// Parameters:
//  pTopLeft: The coordinates of the top-left corner of the box
//            (must have x and y properties)
//  pWidth: The width of the box
//  pBorderColor: The color to use for the border
//  pTextColor: The color to use for the border text
function drawInitialCrossPostSelBoxTopBorder(pTopLeft, pWidth, pBorderColor, pTextColor)
{
  console.gotoxy(pTopLeft);
  console.print(pBorderColor + UPPER_LEFT_SINGLE + RIGHT_T_SINGLE +
                pTextColor + "Cross-posting: Choose group" +
                pBorderColor + LEFT_T_SINGLE);
  var len = pWidth - 31;
  for (var i = 0; i < len; ++i)
    console.print(HORIZONTAL_SINGLE);
}
// For the cross-posting UI: Draws the initial bottom border of
// the selection box
//
// Parameters:
//  pBottomLeft: The coordinates of the bottom-left corner of the box
//               (must have x and y properties)
//  pWidth: The width of the box
//  pBorderColor: The color to use for the border
//  pMsgSubs: Boolean - Whether or not this is being used for the message sub-boards.
//            If true, then this will output "Toggle" for the Enter action.  Otherwise
//            (for message groups), this will output "Select" for the Enter action.
function drawInitialCrossPostSelBoxBottomBorder(pBottomLeft, pWidth, pBorderColor,
                                                 pMsgSubs)
{
  console.gotoxy(pBottomLeft);
  console.print(pBorderColor + LOWER_LEFT_SINGLE + RIGHT_T_SINGLE +
                "nhcb, cb, cNy)bext, cPy)brev, cFy)birst, cLy)bast, cEntery=b"
                + (pMsgSubs ? "Toggle" : "Select") + ", cCtrl-Cnc/hQy=bEnd, c?y=bHelpn"
                + pBorderColor + LEFT_T_SINGLE);
  len = pWidth - 73;
  for (var i = 0; i < len; ++i)
    console.print(HORIZONTAL_SINGLE);
  console.print(LOWER_RIGHT_SINGLE);
}
// Displays help text for cross-posting, for use in cross-post selection mode.
//
// Parameters:
//  selBoxUpperLeft: An object containg x and y coordinates for the upper-left
//                   corner of the selection box
//  selBoxLowerRight: An object containg x and y coordinates for the lower-right
//                    corner of the selection box
function displayCrossPostHelp(selBoxUpperLeft, selBoxLowerRight)
{
   // Create an array of help text, but do it only once, the
   // first time this function is called.
   if (typeof(displayCrossPostHelp.helpLines) == "undefined")
   {
      displayCrossPostHelp.helpLines = new Array();
      displayCrossPostHelp.helpLines.push("ncCross-posing allows you to post a message in more than one message");
      displayCrossPostHelp.helpLines.push("area.  To select areas for cross-posting, do the following:");
      displayCrossPostHelp.helpLines.push(" h1. ncChoose a message group from the list with the Enter key.");
      displayCrossPostHelp.helpLines.push("    Alternately, you may type the number of the message group.");
      displayCrossPostHelp.helpLines.push(" h2. ncIn the list of message sub-boards that appears, toggle individual");
      displayCrossPostHelp.helpLines.push("    sub-boards with the Enter key.  Alternately, you may type the");
      displayCrossPostHelp.helpLines.push("    number of the message sub-board.");
      displayCrossPostHelp.helpLines.push("Message sub-boards that are toggled for cross-posting will include a");
      displayCrossPostHelp.helpLines.push("check mark (" + gConfigSettings.genColors.crossPostChk + CHECK_CHAR + "nc) in the sub-board list.  Initially, your current message");
      displayCrossPostHelp.helpLines.push("sub-board is enabled by default.  Also, your current message group is");
      displayCrossPostHelp.helpLines.push("marked with an asterisk (" + gConfigSettings.genColors.crossPostMsgGrpMark + "*nc).");
      displayCrossPostHelp.helpLines.push("To navigate the list, you may use the up & down arrow keys, N to go to");
      displayCrossPostHelp.helpLines.push("the next page, P to go to the previous page, F to go to the first page,");
      displayCrossPostHelp.helpLines.push("and L to go to the last page.  Ctrl-C, Q, and ESC end the selection.");
   }

   // Display the help text
   var selBoxInnerWidth = selBoxLowerRight.x - selBoxUpperLeft.x - 1;
   var selBoxInnerHeight = selBoxLowerRight.y - selBoxUpperLeft.y - 1;
   var lineLen = 0;
   var screenRow = selBoxUpperLeft.y+1;
   console.print("n");
   for (var i = 0; (i < displayCrossPostHelp.helpLines.length) && (screenRow < selBoxLowerRight.y); ++i)
   {
      console.gotoxy(selBoxUpperLeft.x+1, screenRow++);
      console.print(displayCrossPostHelp.helpLines[i]);
      // If the text line is shorter than the inner width of the box, then
      // blank the rest of the line.
      lineLen = strip_ctrl(displayCrossPostHelp.helpLines[i]).length;
      if (lineLen < selBoxInnerWidth)
      {
         var numSpaces = selBoxInnerWidth - lineLen;
         for (var ii = 0; ii < numSpaces; ++ii)
            console.print(" ");
      }
   }
   // If screenRow is below the bottommost inner row in the selection box, then
   // blank the rest of the lines in the selection box.
   if (screenRow < selBoxLowerRight.y)
   {
      var printfStr = "%-" + selBoxInnerWidth + "s";
      console.print("n");
      for (; screenRow < selBoxLowerRight.y; ++screenRow)
      {
         console.gotoxy(selBoxUpperLeft.x+1, screenRow);
         printf(printfStr, "");
      }
   }
}
// Handles the cross-posting functionality.  Displays a menu of
// the message areas and allows the user to select other areas
// into which to cross-post their message.
//
// Parameters:
//  pOriginalCurpos: Optional - The correct position of the cursor
//                   when this function is called.  If this parameter
//                   is not passed, then this function will call
//                   console.getxy() to get the position of the cursor.
//                   This is used for refreshing the message area to
//                   erase the cross-post selection box when the user
//                   quits out of cross-post selection.
function doCrossPosting(pOriginalCurpos)
{
  // If cross-posting is not allowed, then just return.
  if (!gCanCrossPost)
    return;

  // This function returns the index of the bottommost message group that
  // can be displayed on the screen.
  //
  // Parameters:
  //  pTopGrpIndex: The index of the topmost message group displayed on screen
  //  pNumItemsPerPage: The number of items per page
  function getBottommostGrpIndex(pTopGrpIndex, pNumItemsPerPage)
  {
    var bottomGrpIndex = pTopGrpIndex + pNumItemsPerPage - 1;
    // If bottomGrpIndex is beyond the last index, then adjust it.
    if (bottomGrpIndex >= msg_area.grp_list.length)
       bottomGrpIndex = msg_area.grp_list.length - 1;
    return bottomGrpIndex;
  }

  // Re-writes the "Choose group" text in the top border of the selection
  // box.  For use when returning from the sub-board list.
  //
  // Parameters:
  //  pSelBoxUpperLeft: An object containing x and y values for the upper-left
  //                    corner of the selection box
  //  pSelBoxInnerWidth: The inner width (inside the left & right borders) of the
  //                     selection box
  //  pGrpIndex: The index of message group tht was chosen
  function reWriteInitialTopBorderText(pSelBoxUpperLeft, pSelBoxInnerWidth, pGrpIndex)
  {
    // Position the cursor after the "Cross-posting: " text in the border and
    // write the "Choose group" text
    console.gotoxy(pSelBoxUpperLeft.x+17, pSelBoxUpperLeft.y);
    console.print("n" + gConfigSettings.genColors.crossPostBorderTxt + "Choose group");
    //Choose group
    //Areas in <xxxxx>
    // Re-write the border characters to overwrite the message group name
    grpDesc = msg_area.grp_list[pGrpIndex].description.substr(0, pSelBoxInnerWidth-25);
    // Write the updated border character(s)
    console.print("n" + gConfigSettings.genColors.crossPostBorder + LEFT_T_SINGLE);
    if (grpDesc.length > 3)
    {
      var numChars = grpDesc.length - 3;
      for (var i = 0; i < numChars; ++i)
        console.print(HORIZONTAL_SINGLE);
    }
  }

  // Store the position of the cursor when we started so that we
  // can return the cursor back to this position at the end
  var origStartingCurpos = null;
  if ((pOriginalCurpos != null) && (typeof(pOriginalCurpos) != "undefined"))
     origStartingCurpos = pOriginalCurpos;
  else
     origStartingCurpos = console.getxy();

  // Construct objects to represent the screen locations of the upper-left
  // and lower-right corners of the selection box.  Initially, let the box
  // borders be 1 character into the edit area on all sides.
  var selBoxUpperLeft = new Object();
  selBoxUpperLeft.x = gEditLeft + 3;
  selBoxUpperLeft.y = gEditTop + 1;
  var selBoxLowerRight = new Object();
  selBoxLowerRight.x = gEditRight - 3;
  selBoxLowerRight.y = gEditBottom - 1;
  // Total and inner text width & height of the selection box
  var selBoxWidth = selBoxLowerRight.x - selBoxUpperLeft.x + 1;
  var selBoxHeight = selBoxLowerRight.y - selBoxUpperLeft.y + 1;
  // Don't let the box's height be more than 17 characters.
  if (selBoxHeight > 17)
  {
    selBoxLowerRight.y = selBoxUpperLeft.y + 16; // For a height of 17 characters
    selBoxHeight = selBoxLowerRight.y - selBoxUpperLeft.y + 1;
  }
  // Inner size of the box (for text)
  var selBoxInnerWidth = selBoxWidth - 2;
  var selBoxInnerHeight = selBoxHeight - 2;

  // Calculate the index of the message line at the top of the edit area, which
  // which is where the message area list box will start.  We need to store
  // this so that we can erase the selection box when the user is done
  // selecting a message area.  We'll erase the box by re-writing the message
  // text.
  var editLineIndexAtSelBoxTopRow = gEditLinesIndex - (origStartingCurpos.y-selBoxUpperLeft.y);

  // Variables for keeping track of the message group/area list
  var topMsgGrpIndex = 0;    // The index of the message group at the top of the list
  // Figure out the index of the last message group to appear on the screen.
  var bottomMsgGrpIndex = getBottommostGrpIndex(topMsgGrpIndex, selBoxInnerHeight);
  var numPages = Math.ceil(msg_area.grp_list.length / selBoxInnerHeight);
  var numItemsPerPage = selBoxInnerHeight;
  var topIndexForLastPage = (selBoxInnerHeight * numPages) - selBoxInnerHeight;
  // msgGrpFieldLen will store the length to use for the message group numbers
  // in the list.  It should be able to accommodate the highest message group
  // number on the system.
  var msgGrpFieldLen = msg_area.grp_list.length.toString().length;

  var selectedGrpIndex = 0; // The currently-selected group index

  // Draw the selection box borders
  // Top border
  drawInitialCrossPostSelBoxTopBorder(selBoxUpperLeft, selBoxWidth,
                                      gConfigSettings.genColors.crossPostBorder,
                                      gConfigSettings.genColors.crossPostBorderTxt);
  // Side borders
  console.print(UPPER_RIGHT_SINGLE);
  for (var row = selBoxUpperLeft.y+1; row < selBoxLowerRight.y; ++row)
  {
    console.gotoxy(selBoxUpperLeft.x, row);
    console.print(VERTICAL_SINGLE);
    console.gotoxy(selBoxLowerRight.x, row);
    console.print(VERTICAL_SINGLE);
  }
  // Bottom border
  drawInitialCrossPostSelBoxBottomBorder({ x: selBoxUpperLeft.x, y: selBoxLowerRight.y },
                                         selBoxWidth, gConfigSettings.genColors.crossPostBorder,
                                         false);

  // Write the message groups
  var pageNum = 1;
  ListScreenfulOfMsgGrps(topMsgGrpIndex, selectedGrpIndex, selBoxUpperLeft.y+1,
                         selBoxUpperLeft.x+1, selBoxLowerRight.y-1, selBoxLowerRight.x-1,
                         msgGrpFieldLen, true);
  // Move the cursor to the inner upper-left corner of the selection box
  var curpos = new Object(); // Current cursor position
  curpos.x = selBoxUpperLeft.x+1;
  curpos.y = selBoxUpperLeft.y+1;
  console.gotoxy(curpos);

  // User input loop
  var userInput = null;
  var continueChoosingMsgArea = true;
  while (continueChoosingMsgArea)
  {
    pageNum = calcPageNum(topMsgGrpIndex, selBoxInnerHeight);

    // Get a key from the user (upper-case) and take action based upon it.
    userInput = console.getkey(K_UPPER | K_NOCRLF);
    switch (userInput)
    {
      case KEY_UP: // Move up one message group in the list
         if (selectedGrpIndex > 0)
         {
            // If the previous group index is on the previous page, then
            // display the previous page.
            var previousGrpIndex = selectedGrpIndex - 1;
            if (previousGrpIndex < topMsgGrpIndex)
            {
               // Adjust topMsgGrpIndex and bottomMsgGrpIndex, and
               // refresh the list on the screen.
               topMsgGrpIndex -= numItemsPerPage;
               bottomMsgGrpIndex = getBottommostGrpIndex(topMsgGrpIndex, numItemsPerPage);
               ListScreenfulOfMsgGrps(topMsgGrpIndex, previousGrpIndex, selBoxUpperLeft.y+1,
                         selBoxUpperLeft.x+1, selBoxLowerRight.y-1, selBoxLowerRight.x-1,
                         msgGrpFieldLen, true);
               // We'll want to move the cursor to the leftmost character
               // of the selected line.
               curpos.x = selBoxUpperLeft.x+1;
               curpos.y = selBoxUpperLeft.y+selBoxInnerHeight;
            }
            else
            {
               // Display the current line un-highlighted
               console.gotoxy(selBoxUpperLeft.x+1, curpos.y);
               writeMsgGroupLine(selectedGrpIndex, selBoxInnerWidth, msgGrpFieldLen, false);
               // Display the previous line highlighted
               curpos.x = selBoxUpperLeft.x+1;
               --curpos.y;
               console.gotoxy(curpos);
               writeMsgGroupLine(previousGrpIndex, selBoxInnerWidth, msgGrpFieldLen, true);
            }
            selectedGrpIndex = previousGrpIndex;
            console.gotoxy(curpos); // Move the cursor into place where it should be
         }
         break;
      case KEY_DOWN: // Move down one message group in the list
         if (selectedGrpIndex < msg_area.grp_list.length - 1)
         {
            // If the next group index is on the next page, then display
            // the next page.
            var nextGrpIndex = selectedGrpIndex + 1;
            if (nextGrpIndex > bottomMsgGrpIndex)
            {
               // Adjust topMsgGrpIndex and bottomMsgGrpIndex, and
               // refresh the list on the screen.
               topMsgGrpIndex += numItemsPerPage;
               bottomMsgGrpIndex = getBottommostGrpIndex(topMsgGrpIndex, numItemsPerPage);
               ListScreenfulOfMsgGrps(topMsgGrpIndex, nextGrpIndex, selBoxUpperLeft.y+1,
                         selBoxUpperLeft.x+1, selBoxLowerRight.y-1, selBoxLowerRight.x-1,
                         msgGrpFieldLen, true);
               // We'll want to move the cursor to the leftmost character
               // of the selected line.
               curpos.x = selBoxUpperLeft.x+1;
               curpos.y = selBoxUpperLeft.y+1;
            }
            else
            {
               // Display the current line un-highlighted
               console.gotoxy(selBoxUpperLeft.x+1, curpos.y);
               writeMsgGroupLine(selectedGrpIndex, selBoxInnerWidth, msgGrpFieldLen, false);
               // Display the next line highlighted
               curpos.x = selBoxUpperLeft.x+1;
               ++curpos.y;
               console.gotoxy(curpos);
               writeMsgGroupLine(nextGrpIndex, selBoxInnerWidth, msgGrpFieldLen, true);
            }
            selectedGrpIndex = nextGrpIndex;
            console.gotoxy(curpos); // Move the cursor into place where it should be
         }
         break;
      case KEY_HOME: // Go to the top message group on the screen
         if (selectedGrpIndex > topMsgGrpIndex)
         {
            // Display the current line un-highlighted, adjust
            // selectedGrpIndex, then display the new line
            // highlighted.
            console.gotoxy(selBoxUpperLeft.x+1, curpos.y);
            writeMsgGroupLine(selectedGrpIndex, selBoxInnerWidth, msgGrpFieldLen, false);
            selectedGrpIndex = topMsgGrpIndex;
            curpos = { x: selBoxUpperLeft.x+1, y: selBoxUpperLeft.y+1 };
            console.gotoxy(curpos);
            writeMsgGroupLine(selectedGrpIndex, selBoxInnerWidth, msgGrpFieldLen, true);
            console.gotoxy(curpos);
         }
         break;
      case KEY_END: // Go to the bottom message group on the screen
         if (selectedGrpIndex < bottomMsgGrpIndex)
         {
            // Display the current line un-highlighted, adjust
            // selectedGrpIndex, then display the new line
            // highlighted.
            console.gotoxy(selBoxUpperLeft.x+1, curpos.y);
            writeMsgGroupLine(selectedGrpIndex, selBoxInnerWidth, msgGrpFieldLen, false);
            selectedGrpIndex = bottomMsgGrpIndex;
            curpos.x = selBoxUpperLeft.x + 1;
            curpos.y = selBoxUpperLeft.y + (bottomMsgGrpIndex-topMsgGrpIndex+1);
            console.gotoxy(curpos);
            writeMsgGroupLine(selectedGrpIndex, selBoxInnerWidth, msgGrpFieldLen, true);
            console.gotoxy(curpos);
         }
         break;
      case 'N': // Go to the next page
         var nextPageTopIndex = topMsgGrpIndex + numItemsPerPage;
         if (nextPageTopIndex < msg_area.grp_list.length)
         {
            // Adjust topMsgGrpIndex and bottomMsgGrpIndex, and
            // refresh the list on the screen.
            topMsgGrpIndex = nextPageTopIndex;
            pageNum = calcPageNum(topMsgGrpIndex, numItemsPerPage);
            bottomMsgGrpIndex = getBottommostGrpIndex(topMsgGrpIndex, numItemsPerPage);
            selectedGrpIndex = topMsgGrpIndex;
            ListScreenfulOfMsgGrps(topMsgGrpIndex, selectedGrpIndex, selBoxUpperLeft.y+1,
                         selBoxUpperLeft.x+1, selBoxLowerRight.y-1, selBoxLowerRight.x-1,
                         msgGrpFieldLen, true);
            // Put the cursor at the beginning of the topmost row of message groups
            curpos = { x: selBoxUpperLeft.x+1, y: selBoxUpperLeft.y+1 };
            console.gotoxy(curpos);
         }
         break;
      case 'P': // Go to the previous page
         var prevPageTopIndex = topMsgGrpIndex - numItemsPerPage;
         if (prevPageTopIndex >= 0)
         {
            // Adjust topMsgGrpIndex and bottomMsgGrpIndex, and
            // refresh the list on the screen.
            topMsgGrpIndex = prevPageTopIndex;
            pageNum = calcPageNum(topMsgGrpIndex, numItemsPerPage);
            bottomMsgGrpIndex = getBottommostGrpIndex(topMsgGrpIndex, numItemsPerPage);
            selectedGrpIndex = topMsgGrpIndex;
            ListScreenfulOfMsgGrps(topMsgGrpIndex, selectedGrpIndex, selBoxUpperLeft.y+1,
                         selBoxUpperLeft.x+1, selBoxLowerRight.y-1, selBoxLowerRight.x-1,
                         msgGrpFieldLen, true);
            // Put the cursor at the beginning of the topmost row of message groups
            curpos = { x: selBoxUpperLeft.x+1, y: selBoxUpperLeft.y+1 };
            console.gotoxy(curpos);
         }
         break;
      case 'F': // Go to the first page
         if (topMsgGrpIndex > 0)
         {
            topMsgGrpIndex = 0;
            pageNum = calcPageNum(topMsgGrpIndex, numItemsPerPage);
            bottomMsgGrpIndex = getBottommostGrpIndex(topMsgGrpIndex, numItemsPerPage);
            selectedGrpIndex = 0;
            ListScreenfulOfMsgGrps(topMsgGrpIndex, selectedGrpIndex, selBoxUpperLeft.y+1,
                         selBoxUpperLeft.x+1, selBoxLowerRight.y-1, selBoxLowerRight.x-1,
                         msgGrpFieldLen, true);
            // Put the cursor at the beginning of the topmost row of message groups
            curpos = { x: selBoxUpperLeft.x+1, y: selBoxUpperLeft.y+1 };
            console.gotoxy(curpos);
         }
         break;
      case 'L': // Go to the last page
         if (topMsgGrpIndex < topIndexForLastPage)
         {
            topMsgGrpIndex = topIndexForLastPage;
            pageNum = calcPageNum(topMsgGrpIndex, numItemsPerPage);
            bottomMsgGrpIndex = getBottommostGrpIndex(topMsgGrpIndex, numItemsPerPage);
            selectedGrpIndex = topIndexForLastPage;
            ListScreenfulOfMsgGrps(topMsgGrpIndex, selectedGrpIndex, selBoxUpperLeft.y+1,
                         selBoxUpperLeft.x+1, selBoxLowerRight.y-1, selBoxLowerRight.x-1,
                         msgGrpFieldLen, true);
            // Put the cursor at the beginning of the topmost row of message groups
            curpos = { x: selBoxUpperLeft.x+1, y: selBoxUpperLeft.y+1 };
            console.gotoxy(curpos);
         }
         break;
      case CTRL_C:  // Quit (Ctrl-C is the cross-post hotkey)
      case KEY_ESC: // Quit
      case "Q":     // Quit
         continueChoosingMsgArea = false;
         break;
      case KEY_ENTER: // Select the currently-highlighted message group
         // Store the current cursor position for later, then show the
         // sub-boards in the chosen message group and let the user
         // toggle ones for cross-posting.
         var selectCurrentGrp_originalCurpos = curpos;
         var selectMsgAreaRetObj = crossPosting_selectSubBoardInGrp(selectedGrpIndex,
                                               selBoxUpperLeft, selBoxLowerRight, selBoxWidth,
                                               selBoxHeight, selBoxInnerWidth, selBoxInnerHeight);
         // If the user toggled some sub-boards...
         if (selectMsgAreaRetObj.subBoardsToggled)
         {
            // TODO: Does anything need to be done here?
         }

         // Update the Enter action text in the bottom border to say "Select"
         // (instead of "Toggle").
         console.gotoxy(selBoxUpperLeft.x+43, selBoxLowerRight.y);
         console.print("nhbSelect");
         // Refresh the top border of the selection box, refresh the list of
         // message groups in the box, and move the cursor back to its original
         // position.
         reWriteInitialTopBorderText(selBoxUpperLeft, selBoxInnerWidth, selectedGrpIndex);
         ListScreenfulOfMsgGrps(topMsgGrpIndex, selectedGrpIndex, selBoxUpperLeft.y+1,
                                selBoxUpperLeft.x+1, selBoxLowerRight.y-1,
                                selBoxLowerRight.x-1, msgGrpFieldLen, true);
         console.gotoxy(selectCurrentGrp_originalCurpos);
         break;
      case '?': // Display cross-post help
         displayCrossPostHelp(selBoxUpperLeft, selBoxLowerRight);
         console.gotoxy(selBoxUpperLeft.x+1, selBoxLowerRight.y-1);
         console.pause();
         ListScreenfulOfMsgGrps(topMsgGrpIndex, selectedGrpIndex, selBoxUpperLeft.y+1,
                                selBoxUpperLeft.x+1, selBoxLowerRight.y-1,
                                selBoxLowerRight.x-1, msgGrpFieldLen, true);
         console.gotoxy(curpos);
         break;
      default:
         // If the user entered a numeric digit, then treat it as
         // the start of the message group number.
         if (userInput.match(/[0-9]/))
         {
            var originalCurpos = curpos;
            // Put the user's input back in the input buffer to
            // be used for getting the rest of the message number.
            console.ungetstr(userInput);
            // We want to write the prompt text only if the first digit entered
            // by the user is an ambiguous message group number (i.e., if
            // the first digit is 2 and there's a message group # 2 and 20).
            var writePromptText = (msg_area.grp_list.length >= +userInput * 10);
            if (writePromptText)
            {
              console.gotoxy(selBoxUpperLeft.x+1, selBoxLowerRight.y);
              printf("ncChoose group #:%" + +(selBoxInnerWidth-15) + "s", "");
              console.gotoxy(selBoxUpperLeft.x+17, selBoxLowerRight.y);
              console.print("h");
            }
            else
              console.gotoxy(selBoxUpperLeft.x+1, selBoxLowerRight.y);
            userInput = console.getnum(msg_area.grp_list.length);

            // Re-draw the bottom border of the selection box
            if (writePromptText)
            {
              drawInitialCrossPostSelBoxBottomBorder({ x: selBoxUpperLeft.x, y: selBoxLowerRight.y },
                                                     selBoxWidth, gConfigSettings.genColors.crossPostBorder,
                                                     false);
            }
            else
            {
              console.gotoxy(selBoxUpperLeft.x+1, selBoxLowerRight.y);
              console.print(gConfigSettings.genColors.crossPostBorder + RIGHT_T_SINGLE);
            }

            // If the user made a selection, then let them choose a
            // sub-board from the group.
            if (userInput > 0)
            {
               // Show the sub-boards in the chosen message group and
               // let the user toggle ones for cross-posting.
               // userInput-1 is the group index
               var chosenGrpIndex = userInput - 1;
               var selectMsgAreaRetObj = crossPosting_selectSubBoardInGrp(chosenGrpIndex,
                                               selBoxUpperLeft, selBoxLowerRight, selBoxWidth,
                                               selBoxHeight, selBoxInnerWidth, selBoxInnerHeight);
               // If the user chose a sub-board, then set bbs.curgrp and
               // bbs.cursub, and don't continue the input loop anymore.
               if (selectMsgAreaRetObj.subBoardsToggled)
               {
                  // TODO: Does anything need to be done here?
               }
               // Update the Enter action text in the bottom border to say "Select"
               // (instead of "Toggle").
               console.gotoxy(selBoxUpperLeft.x+43, selBoxLowerRight.y);
               console.print("nhbSelect");
               // Refresh the top border of the selection box
               reWriteInitialTopBorderText(selBoxUpperLeft, selBoxInnerWidth, chosenGrpIndex);
            }

            // Refresh the list of message groups in the box and move the
            // cursor back to its original position.
            ListScreenfulOfMsgGrps(topMsgGrpIndex, selectedGrpIndex, selBoxUpperLeft.y+1,
                                   selBoxUpperLeft.x+1, selBoxLowerRight.y-1,
                                   selBoxLowerRight.x-1, msgGrpFieldLen, true);
            console.gotoxy(originalCurpos);
         }
         break;
    }
  }

  // We're done selecting message areas for cross-posting.
  // Erase the message area selection rectangle by re-drawing the message text.
  // Then, move the cursor back to where it was when we started the message
  // area selection.
  displayMessageRectangle(selBoxUpperLeft.x, selBoxUpperLeft.y, selBoxWidth,
                          selBoxHeight, editLineIndexAtSelBoxTopRow, true);
  console.gotoxy(origStartingCurpos);
}
// Displays a screenful of message groups, for the cross-posting
// interface.
//
// Parameters:
//  pStartIndex: The message group index to start at (0-based)
//  pSelectedIndex: The index of the currently-selected message group
//  pStartScreenRow: The row on the screen to start at (1-based)
//  pStartScreenCol: The column on the screen to start at (1-based)
//  pEndScreenRow: The row on the screen to end at (1-based)
//  pEndScreenCol: The column on the screen to end at (1-based)
//  pMsgGrpFieldLen: The length to use for the group number field
//  pBlankToEndRow: Boolean - Whether or not to write blank lines to the end
//                  screen row if there aren't enough message groups to fill
//                  the screen.
function ListScreenfulOfMsgGrps(pStartIndex, pSelectedIndex, pStartScreenRow,
                                 pStartScreenCol, pEndScreenRow, pEndScreenCol,
                                 pMsgGrpFieldLen, pBlankToEndRow)
{
   // If the parameters are invalid, then just return.
   if ((typeof(pStartIndex) != "number") || (typeof(pSelectedIndex) != "number") ||
       (typeof(pStartScreenRow) != "number") || (typeof(pStartScreenCol) != "number") ||
       (typeof(pEndScreenRow) != "number") || (typeof(pEndScreenCol) != "number"))
   {
      return;
   }
   if ((pStartIndex < 0) || (pStartIndex >= msg_area.grp_list.length))
      return;
   if ((pStartScreenRow < 1) || (pStartScreenRow > console.screen_rows))
      return;
   if ((pEndScreenRow < 1) || (pEndScreenRow > console.screen_rows))
      return;
   if ((pStartScreenCol < 1) || (pStartScreenCol > console.screen_columns))
      return;
   if ((pEndScreenCol < 1) || (pEndScreenCol > console.screen_columns))
      return;

   // If pStartScreenRow is greater than pEndScreenRow, then swap them.
   // Do the same with pStartScreenCol and pEndScreenCol.
   if (pStartScreenRow > pEndScreenRow)
   {
      var temp = pStartScreenRow;
      pStartScreenRow = pEndScreenRow;
      pEndScreenRow = temp;
   }
   if (pStartScreenCol > pEndScreenCol)
   {
      var temp = pStartScreenCol;
      pStartScreenCol = pEndScreenCol;
      pEndScreenCol = temp;
   }

   // Calculate the ending index to use for the message groups array.
   var endIndex = pStartIndex + (pEndScreenRow-pStartScreenRow);
   if (endIndex >= msg_area.grp_list.length)
      endIndex = msg_area.grp_list.length - 1;
   var onePastEndIndex = endIndex + 1;

   // Go to the specified screen row, and display the message group information.
   var textWidth = pEndScreenCol - pStartScreenCol + 1;
   var row = pStartScreenRow;
   var grpIndex = pStartIndex;
   for (; grpIndex < onePastEndIndex; ++grpIndex)
   {
      console.gotoxy(pStartScreenCol, row++);
      // The 4th parameter to writeMsgGroupLine() is whether or not to
      // write the message group with highlight colors.
      writeMsgGroupLine(grpIndex, textWidth, pMsgGrpFieldLen, (grpIndex == pSelectedIndex));
   }

   // If pBlankToEndRow is true and we're not at the end row yet, then
   // write blank lines to the end row.
   if (pBlankToEndRow)
   {
      var screenRow = pStartScreenRow + (endIndex - pStartIndex) + 1;
      if (screenRow <= pEndScreenRow)
      {
         console.print("n");
         var areaWidth = pEndScreenCol - pStartScreenCol + 1;
         var formatStr = "%-" + areaWidth + "s";
         for (; screenRow <= pEndScreenRow; ++screenRow)
         {
            console.gotoxy(pStartScreenCol, screenRow)
            printf(formatStr, "");
         }
      }
   }
}
// Writes a message group information line (for choosing a message group
// for cross-posing).
//
// Parameters:
//  pGrpIndex: The index of the message group to write (assumed to be valid)
//  pTextWidth: The maximum text width
//  pMsgGrpFieldLen: The length to use for the group number field
//  pHighlight: Boolean - Whether or not to write the line highlighted.
function writeMsgGroupLine(pGrpIndex, pTextWidth, pMsgGrpFieldLen, pHighlight)
{
   if ((typeof(pGrpIndex) != "number") || (typeof(pTextWidth) != "number"))
      return;

   // Build a printf format string
   var grpDescLen = pTextWidth - pMsgGrpFieldLen - 2;
   var printfStr = "n";
   if (pHighlight)
   {
     printfStr += gConfigSettings.genColors.crossPostMsgGrpMarkHighlight + "%1s"
               + gConfigSettings.genColors.crossPostMsgAreaNumHighlight + "%" + pMsgGrpFieldLen
               + "d " + gConfigSettings.genColors.crossPostMsgAreaDescHighlight + "%-"
               + grpDescLen + "s";
   }
   else
   {
     printfStr += gConfigSettings.genColors.crossPostMsgGrpMark + "%1s"
               + gConfigSettings.genColors.crossPostMsgAreaNum + "%" + pMsgGrpFieldLen + "d "
               + gConfigSettings.genColors.crossPostMsgAreaDesc + "%-" + grpDescLen + "s";
   }

   // Write the message group information line
   var markChar = (pGrpIndex == gMsgAreaInfo.grpIndex ? "*" : " ");
   printf(printfStr, markChar, +(pGrpIndex+1), msg_area.grp_list[pGrpIndex].description.substr(0, grpDescLen));
}
// For cross-posting: Lets the user choose a sub-board within a message group
//
// Parameters:
//  pGrpIndex: The index of the message group to choose from.
//  pSelBoxUpperLeft: An object containing the following values:
//                    x: The horizontal coordinate (column) of the selection box's
//                       upper-left corner
//                    y: The vertical coordinate (row) of the selection box's
//                       upper-left corner
//  pSelBoxLowerRight: An object containing the following values:
//                     x: The horizontal coordinate (column) of the selection box's
//                        lower-right corner
//                     y: The vertical coordinate (row) of the selection box's
//                        lower-right corner
//  pSelBoxWidth: The width of the selection box
//  pSelBoxHeight: The height of the selection box
//  pSelBoxInnerWidth: The inner width of the selection box (inside the borders)
//  pSelBoxInnerHeight: The inner height of the selection box (inside the borders)
//
// Return value: An object containing the following values:
//               subBoardsToggled: Boolean - Whether or not any sub-boards were toggled
function crossPosting_selectSubBoardInGrp(pGrpIndex, pSelBoxUpperLeft, pSelBoxLowerRight,
                                           pSelBoxWidth, pSelBoxHeight, pSelBoxInnerWidth,
                                           pSelBoxInnerHeight)
{
  // Create the return object with default values
  var retObj = new Object();
  retObj.subBoardsToggled = false;

  // Check the parameters and return if any are invalid
  if ((typeof(pGrpIndex) != "number") || (typeof(pSelBoxWidth) != "number") ||
      (typeof(pSelBoxHeight) != "number") || (typeof(pSelBoxInnerWidth) != "number") ||
      (typeof(pSelBoxInnerHeight) != "number") || (typeof(pSelBoxUpperLeft) != "object") ||
      (typeof(pSelBoxLowerRight) != "object") || (typeof(pSelBoxUpperLeft.x) != "number") ||
      (typeof(pSelBoxUpperLeft.y) != "number") || (typeof(pSelBoxLowerRight.x) != "number") ||
      (typeof(pSelBoxLowerRight.y) != "number"))
  {
    return retObj;
  }


  // Clear the text inside the selection box
  console.print("n");
  var printfStr = "%" + pSelBoxInnerWidth + "s";
  for (var rowOffset = 1; rowOffset <= pSelBoxInnerHeight; ++rowOffset)
  {
    console.gotoxy(pSelBoxUpperLeft.x+1, pSelBoxUpperLeft.y+rowOffset);
    printf(printfStr, "");
  }

  // If there are no sub-boards in the given message group, then show
  // an error and return.
  if (msg_area.grp_list[pGrpIndex].sub_list.length == 0)
  {
    console.gotoxy(pSelBoxUpperLeft.x+1, pSelBoxUpperLeft.y+1);
    console.print("yhThere are no message areas in the chosen group.");
    console.gotoxy(pSelBoxUpperLeft.x+1, pSelBoxUpperLeft.y+2);
    console.pause();
    return retObj;
  }

  // This function returns the index of the bottommost message sub-board that
  // can be displayed on the screen.
  //
  // Parameters:
  //  pTopSubIndex: The index of the topmost message sub-board displayed on screen
  //  pNumItemsPerPage: The number of items per page
  function getBottommostSubBoardIndex(pTopSubIndex, pNumItemsPerPage)
  {
    var bottomSubIndex = pTopSubIndex + pNumItemsPerPage - 1;
    // If bottomSubIndex is beyond the last index, then adjust it.
    if (bottomSubIndex >= msg_area.grp_list[pGrpIndex].sub_list.length)
       bottomSubIndex = msg_area.grp_list[pGrpIndex].sub_list.length - 1;
    return bottomSubIndex;
  }

  // This function writes the error message that the user can't post in a
  // message sub-board, pauses, then refreshes the bottom border of the
  // selection box.
  //
  // Parameters:
  //  pX: The column of the lower-right corner of the selection box
  //  pY: The row of the lower-right corner of the selection box
  //  pSubBoardNum: The number of the sub-board (1-based)
  //  pSelBoxWidth: The width of the selection box
  //  pSelBoxInnerWidth: The width of the selection box inside the left & right borders
  //  pPauseMS: The number of millisecons to pause after displaying the error message
  //  pCurpos: The position of the cursor before calling this function
  function writeCantPostErrMsg(pX, pY, pSubBoardNum, pSelBoxWidth, pSelBoxInnerWidth, pPauseMS, pCurpos)
  {
    var cantPostErrMsg = "You're not allowed to post in area " + pSubBoardNum + ".";
    console.gotoxy(pX+1, pY);
    printf("nhy%-" + pSelBoxInnerWidth + "s", cantPostErrMsg);
    console.gotoxy(pX+cantPostErrMsg.length+1, pY);
    mswait(pPauseMS);
    // Refresh the bottom border of the selection box
    drawInitialCrossPostSelBoxBottomBorder({ x: pX, y: pY }, pSelBoxWidth,
                                           gConfigSettings.genColors.crossPostBorder, true);
    console.gotoxy(pCurpos);
  }

  // Update the text in the top border of the selection box to include
  // the message area description
  //¦Cross-posting: Choose group
  //¦Cross-posting: Areas in 
  console.gotoxy(pSelBoxUpperLeft.x+17, pSelBoxUpperLeft.y);
  var grpDesc = msg_area.grp_list[pGrpIndex].description.substr(0, pSelBoxInnerWidth-25);
  console.print("n" + gConfigSettings.genColors.crossPostBorderTxt + "Areas in " +
                grpDesc);
  // Write the updated border character(s)
  console.print("n" + gConfigSettings.genColors.crossPostBorder);
  // If the length of the group description is shorter than the remaining text
  // the selection box border, then draw horizontal lines to fill in the gap.
  if (grpDesc.length < 3)
  {
    
    var numChars = 3 - grpDesc.length;
    for (var i = 0; i < numChars; ++i)
      console.print(HORIZONTAL_SINGLE);
  }
  console.print(LEFT_T_SINGLE);

  // Update the Enter action text in the bottom border to say "Toggle"
  // (instead of "Select").
  console.gotoxy(pSelBoxUpperLeft.x+43, pSelBoxLowerRight.y);
  console.print("nhbToggle");

  // Variables for keeping track of the message group/area list
  var topMsgSubIndex = 0;    // The index of the message sub-board at the top of the list
  // Figure out the index of the last message group to appear on the screen.
  var bottomMsgSubIndex = getBottommostSubBoardIndex(topMsgSubIndex, pSelBoxInnerHeight);
  var numPages = Math.ceil(msg_area.grp_list[pGrpIndex].sub_list.length / pSelBoxInnerHeight);
  var numItemsPerPage = pSelBoxInnerHeight;
  var topIndexForLastPage = (pSelBoxInnerHeight * numPages) - pSelBoxInnerHeight;
  var selectedMsgSubIndex = 0; // The currently-selected message sub-board index
  // subNumFieldLen will store the length to use for the sub-board numbers in the list.
  // It should be able to accommodate the highest message sub-board number in the
  // group.
  var subNumFieldLen = msg_area.grp_list[pGrpIndex].sub_list.length.toString().length;
  // The number of milliseconds to pause after displaying the error message
  // that the user isn't allowed to post in a sub-board (due to ARS).
  var cantPostErrPauseMS = 2000;

  // Write the sub-boards
  var pageNum = 1;
  ListScreenfulOfMsgSubs(pGrpIndex, topMsgSubIndex, selectedMsgSubIndex, pSelBoxUpperLeft.y+1,
                         pSelBoxUpperLeft.x+1, pSelBoxLowerRight.y-1, pSelBoxLowerRight.x-1,
                         subNumFieldLen, true);
  // Move the cursor to the inner upper-left corner of the selection box
  var curpos = new Object(); // Current cursor position
  curpos.x = pSelBoxUpperLeft.x+1;
  curpos.y = pSelBoxUpperLeft.y+1;
  console.gotoxy(curpos);

  // User input loop
  var userInput = null;
  var continueChoosingMsgSubBoard = true;
  while (continueChoosingMsgSubBoard)
  {
    pageNum = calcPageNum(topMsgSubIndex, pSelBoxInnerHeight);

    // Get a key from the user (upper-case) and take action based upon it.
    userInput = console.getkey(K_UPPER | K_NOCRLF);
    switch (userInput)
    {
      case KEY_UP: // Move up one message sub-board in the list
         if (selectedMsgSubIndex > 0)
         {
            // If the previous group index is on the previous page, then
            // display the previous page.
            var previousMsgSubIndex = selectedMsgSubIndex - 1;
            if (previousMsgSubIndex < topMsgSubIndex)
            {
               // Adjust topMsgSubIndex and bottomMsgGrpIndex, and
               // refresh the list on the screen.
               topMsgSubIndex -= numItemsPerPage;
               bottomMsgSubIndex = getBottommostSubBoardIndex(topMsgSubIndex, numItemsPerPage);
               ListScreenfulOfMsgSubs(pGrpIndex, topMsgSubIndex, previousMsgSubIndex,
                         pSelBoxUpperLeft.y+1, pSelBoxUpperLeft.x+1, pSelBoxLowerRight.y-1,
                         pSelBoxLowerRight.x-1, subNumFieldLen, true);
               // We'll want to move the cursor to the leftmost character
               // of the selected line.
               curpos.x = pSelBoxUpperLeft.x+1;
               curpos.y = pSelBoxUpperLeft.y+pSelBoxInnerHeight;
            }
            else
            {
               // Display the current line un-highlighted
               console.gotoxy(pSelBoxUpperLeft.x+1, curpos.y);
               writeMsgSubLine(pGrpIndex, selectedMsgSubIndex, pSelBoxInnerWidth,
                               subNumFieldLen, false);
               // Display the previous line highlighted
               curpos.x = pSelBoxUpperLeft.x+1;
               --curpos.y;
               console.gotoxy(curpos);
               writeMsgSubLine(pGrpIndex, previousMsgSubIndex, pSelBoxInnerWidth,
                               subNumFieldLen, true);
            }
            selectedMsgSubIndex = previousMsgSubIndex;
            console.gotoxy(curpos); // Move the cursor into place where it should be
         }
         break;
      case KEY_DOWN: // Move down one message sub-board in the list
         if (selectedMsgSubIndex < msg_area.grp_list[pGrpIndex].sub_list.length - 1)
         {
            // If the next sub-board index is on the next page, then display
            // the next page.
            var nextMsgSubIndex = selectedMsgSubIndex + 1;
            if (nextMsgSubIndex > bottomMsgSubIndex)
            {
               // Adjust topMsgGrpIndex and bottomMsgGrpIndex, and
               // refresh the list on the screen.
               topMsgSubIndex += numItemsPerPage;
               bottomMsgSubIndex = getBottommostSubBoardIndex(topMsgSubIndex, numItemsPerPage);
               ListScreenfulOfMsgSubs(pGrpIndex, topMsgSubIndex, nextMsgSubIndex,
                         pSelBoxUpperLeft.y+1, pSelBoxUpperLeft.x+1, pSelBoxLowerRight.y-1,
                         pSelBoxLowerRight.x-1, subNumFieldLen, true);
               // We'll want to move the cursor to the leftmost character
               // of the selected line.
               curpos.x = pSelBoxUpperLeft.x+1;
               curpos.y = pSelBoxUpperLeft.y+1;
            }
            else
            {
               // Display the current line un-highlighted
               console.gotoxy(pSelBoxUpperLeft.x+1, curpos.y);
               writeMsgSubLine(pGrpIndex, selectedMsgSubIndex, pSelBoxInnerWidth,
                               subNumFieldLen, false);
               // Display the next line highlighted
               curpos.x = pSelBoxUpperLeft.x+1;
               ++curpos.y;
               console.gotoxy(curpos);
               writeMsgSubLine(pGrpIndex, nextMsgSubIndex, pSelBoxInnerWidth,
                               subNumFieldLen, true);
            }
            selectedMsgSubIndex = nextMsgSubIndex;
            console.gotoxy(curpos); // Move the cursor into place where it should be
         }
         break;
      case KEY_HOME: // Go to the top message sub-board on the screen
         if (selectedMsgSubIndex > topMsgSubIndex)
         {
            // Display the current line un-highlighted, adjust
            // selectedMsgSubIndex, then display the new line
            // highlighted.
            console.gotoxy(pSelBoxUpperLeft.x+1, curpos.y);
            writeMsgSubLine(pGrpIndex, selectedMsgSubIndex, pSelBoxInnerWidth,
                            subNumFieldLen, false);
            selectedMsgSubIndex = topMsgSubIndex;
            curpos = { x: pSelBoxUpperLeft.x+1, y: pSelBoxUpperLeft.y+1 };
            console.gotoxy(curpos);
            writeMsgSubLine(pGrpIndex, selectedMsgSubIndex, pSelBoxInnerWidth,
                            subNumFieldLen, true);
            console.gotoxy(curpos);
         }
         break;
      case KEY_END: // Go to the bottom message sub-board on the screen
         if (selectedMsgSubIndex < bottomMsgSubIndex)
         {
            // Display the current line un-highlighted, adjust
            // selectedGrpIndex, then display the new line
            // highlighted.
            console.gotoxy(pSelBoxUpperLeft.x+1, curpos.y);
            writeMsgSubLine(pGrpIndex, selectedMsgSubIndex, pSelBoxInnerWidth,
                            subNumFieldLen, false);
            selectedMsgSubIndex = bottomMsgSubIndex;
            curpos.x = pSelBoxUpperLeft.x + 1;
            curpos.y = pSelBoxUpperLeft.y + (bottomMsgSubIndex-topMsgSubIndex+1);
            console.gotoxy(curpos);
            writeMsgSubLine(pGrpIndex, selectedMsgSubIndex, pSelBoxInnerWidth,
                            subNumFieldLen, true);
            console.gotoxy(curpos);
         }
         break;
      case 'N': // Go to the next page
         var nextPageTopIndex = topMsgSubIndex + numItemsPerPage;
         if (nextPageTopIndex < msg_area.grp_list[pGrpIndex].sub_list.length)
         {
            // Adjust the top and bottom indexes, and refresh the list on the
            // screen.
            topMsgSubIndex = nextPageTopIndex;
            pageNum = calcPageNum(topMsgSubIndex, numItemsPerPage);
            bottomMsgSubIndex = getBottommostSubBoardIndex(topMsgSubIndex, numItemsPerPage);
            selectedMsgSubIndex = topMsgSubIndex;
            ListScreenfulOfMsgSubs(pGrpIndex, topMsgSubIndex, selectedMsgSubIndex,
                         pSelBoxUpperLeft.y+1, pSelBoxUpperLeft.x+1, pSelBoxLowerRight.y-1,
                         pSelBoxLowerRight.x-1, subNumFieldLen, true);
            // Put the cursor at the beginning of the topmost row of message groups
            curpos = { x: pSelBoxUpperLeft.x+1, y: pSelBoxUpperLeft.y+1 };
            console.gotoxy(curpos);
         }
         break;
      case 'P': // Go to the previous page
         var prevPageTopIndex = topMsgSubIndex - numItemsPerPage;
         if (prevPageTopIndex >= 0)
         {
            // Adjust the top and bottom indexes, and refresh the list on the
            // screen.
            topMsgSubIndex = prevPageTopIndex;
            pageNum = calcPageNum(topMsgSubIndex, numItemsPerPage);
            bottomMsgSubIndex = getBottommostSubBoardIndex(topMsgSubIndex, numItemsPerPage);
            selectedMsgSubIndex = topMsgSubIndex;
            ListScreenfulOfMsgSubs(pGrpIndex, topMsgSubIndex, selectedMsgSubIndex,
                         pSelBoxUpperLeft.y+1, pSelBoxUpperLeft.x+1, pSelBoxLowerRight.y-1,
                         pSelBoxLowerRight.x-1, subNumFieldLen, true);
            // Put the cursor at the beginning of the topmost row of message groups
            curpos = { x: pSelBoxUpperLeft.x+1, y: pSelBoxUpperLeft.y+1 };
            console.gotoxy(curpos);
         }
         break;
      case 'F': // Go to the first page
         if (topMsgSubIndex > 0)
         {
            topMsgSubIndex = 0;
            pageNum = calcPageNum(topMsgSubIndex, numItemsPerPage);
            bottomMsgSubIndex = getBottommostSubBoardIndex(topMsgSubIndex, numItemsPerPage);
            selectedMsgSubIndex = 0;
            ListScreenfulOfMsgSubs(pGrpIndex, topMsgSubIndex, selectedMsgSubIndex,
                         pSelBoxUpperLeft.y+1, pSelBoxUpperLeft.x+1, pSelBoxLowerRight.y-1,
                         pSelBoxLowerRight.x-1, subNumFieldLen, true);
            // Put the cursor at the beginning of the topmost row of message groups
            curpos = { x: pSelBoxUpperLeft.x+1, y: pSelBoxUpperLeft.y+1 };
            console.gotoxy(curpos);
         }
         break;
      case 'L': // Go to the last page
         if (topMsgSubIndex < topIndexForLastPage)
         {
            topMsgSubIndex = topIndexForLastPage;
            pageNum = calcPageNum(topMsgSubIndex, numItemsPerPage);
            bottomMsgSubIndex = getBottommostSubBoardIndex(topMsgSubIndex, numItemsPerPage);
            selectedMsgSubIndex = topIndexForLastPage;
            ListScreenfulOfMsgSubs(pGrpIndex, topMsgSubIndex, selectedMsgSubIndex,
                         pSelBoxUpperLeft.y+1, pSelBoxUpperLeft.x+1, pSelBoxLowerRight.y-1,
                         pSelBoxLowerRight.x-1, subNumFieldLen, true);
            // Put the cursor at the beginning of the topmost row of message groups
            curpos = { x: pSelBoxUpperLeft.x+1, y: pSelBoxUpperLeft.y+1 };
            console.gotoxy(curpos);
         }
         break;
      case CTRL_C:  // Quit (Ctrl-C is the cross-post hotkey)
      case "Q":     // Quit
      case KEY_ESC: // Quit
         continueChoosingMsgSubBoard = false;
         break;
      case KEY_ENTER: // Select the currently-highlighted message sub-board
         // If the sub-board code is toggled on, then toggle it off, and vice-versa.
         var msgSubCode = msg_area.grp_list[pGrpIndex].sub_list[selectedMsgSubIndex].code;
         if (gCrossPostMsgSubs.subCodeExists(msgSubCode))
         {
            // Remove it from gCrossPostMsgSubs and refresh the line on the screen
            gCrossPostMsgSubs.remove(msgSubCode);
            console.gotoxy(pSelBoxUpperLeft.x+1, curpos.y);
            // Write a blank space using highlight colors
            console.print(gConfigSettings.genColors.crossPostChkHighlight + " ");
            console.gotoxy(pSelBoxUpperLeft.x+1, curpos.y);
         }
         else
         {
            // If the user is allowed to post in the selected sub, then add it
            // to gCrossPostMsgSubs and refresh the line on the screen;
            // otherwise, show an error message.
            if (user.compare_ars(msg_area.sub[msgSubCode].post_ars))
            {
              gCrossPostMsgSubs.add(msgSubCode);
              console.gotoxy(pSelBoxUpperLeft.x+1, curpos.y);
              // Write a checkmark using highlight colors
              console.print(gConfigSettings.genColors.crossPostChkHighlight + CHECK_CHAR);
              console.gotoxy(pSelBoxUpperLeft.x+1, curpos.y);
           }
           else
           {
              // Go to the bottom row of the selection box and display an error that
              // the user can't post in the selected sub-board and pause for a moment.
              writeCantPostErrMsg(pSelBoxUpperLeft.x, pSelBoxLowerRight.y, selectedMsgSubIndex+1,
                                  pSelBoxWidth, pSelBoxInnerWidth, cantPostErrPauseMS, curpos);
           }
         }
         break;
      case '?': // Display cross-post help
         displayCrossPostHelp(pSelBoxUpperLeft, pSelBoxLowerRight);
         console.gotoxy(pSelBoxUpperLeft.x+1, pSelBoxLowerRight.y);
         console.pause();
         ListScreenfulOfMsgSubs(pGrpIndex, topMsgSubIndex, selectedMsgSubIndex,
                         pSelBoxUpperLeft.y+1, pSelBoxUpperLeft.x+1, pSelBoxLowerRight.y-1,
                         pSelBoxLowerRight.x-1, subNumFieldLen, true);
         console.gotoxy(curpos);
         break;
      default:
         // If the user entered a numeric digit, then treat it as
         // the start of the message sub-board number.
         if (userInput.match(/[0-9]/))
         {
            var originalCurpos = curpos;
            // Put the user's input back in the input buffer to
            // be used for getting the rest of the message number.
            console.ungetstr(userInput);
            // We want to write the prompt text only if the first digit entered
            // by the user is an ambiguous message sub-board number (i.e., if
            // the first digit is 2 and there's a message group # 2 and 20).
            var writePromptText = (msg_area.grp_list[pGrpIndex].sub_list.length >= +userInput * 10);
            if (writePromptText)
            {
              // Move the cursor to the bottom border of the selection box and
              // prompt the user for the message number.
              console.gotoxy(pSelBoxUpperLeft.x+1, pSelBoxLowerRight.y);
              printf("ncToggle sub-board #:%" + +(pSelBoxInnerWidth-19) + "s", "");
              console.gotoxy(pSelBoxUpperLeft.x+21, pSelBoxLowerRight.y);
              console.print("h");
            }
            else
              console.gotoxy(pSelBoxUpperLeft.x+1, pSelBoxLowerRight.y);
            userInput = console.getnum(msg_area.grp_list[pGrpIndex].sub_list.length);
            var chosenMsgSubIndex = userInput - 1;

            // Re-draw the bottom border of the selection box
            if (writePromptText)
            {
              drawInitialCrossPostSelBoxBottomBorder({ x: pSelBoxUpperLeft.x, y: pSelBoxLowerRight.y },
                                                     pSelBoxWidth, gConfigSettings.genColors.crossPostBorder,
                                                     true);
            }
            else
            {
              console.gotoxy(pSelBoxUpperLeft.x+1, pSelBoxLowerRight.y);
              console.print(gConfigSettings.genColors.crossPostBorder + RIGHT_T_SINGLE);
            }

            // If the user made a selection, then toggle it on/off.
            if (userInput > 0)
            {
               var msgSubCode = msg_area.grp_list[pGrpIndex].sub_list[chosenMsgSubIndex].code;
               if (gCrossPostMsgSubs.subCodeExists(msgSubCode))
               {
                  // Remove it from gCrossPostMsgSubs and refresh the line on the screen
                  gCrossPostMsgSubs.remove(msgSubCode);
                  if ((chosenMsgSubIndex >= topMsgSubIndex) && (chosenMsgSubIndex <= bottomMsgSubIndex))
                  {
                    var screenRow = pSelBoxUpperLeft.y + (chosenMsgSubIndex - topMsgSubIndex + 1);
                    console.gotoxy(pSelBoxUpperLeft.x+1, screenRow);
                    // Write a blank space
                    var color = (chosenMsgSubIndex == selectedMsgSubIndex ?
                                 gConfigSettings.genColors.crossPostChkHighlight :
                                 gConfigSettings.genColors.crossPostChk);
                    console.print(color + " ");
                  }
               }
               else
               {
                  // If the user is allowed to post in the selected sub, then add it
                  // to gCrossPostMsgSubs and refresh the line on the screen;
                  // otherwise, show an error message.
                  if (user.compare_ars(msg_area.sub[msgSubCode].post_ars))
                  {
                     gCrossPostMsgSubs.add(msgSubCode);
                     if ((chosenMsgSubIndex >= topMsgSubIndex) && (chosenMsgSubIndex <= bottomMsgSubIndex))
                     {
                       var screenRow = pSelBoxUpperLeft.y + (chosenMsgSubIndex - topMsgSubIndex + 1);
                       console.gotoxy(pSelBoxUpperLeft.x+1, screenRow);
                       // Write a checkmark
                       var color = (chosenMsgSubIndex == selectedMsgSubIndex ?
                                    gConfigSettings.genColors.crossPostChkHighlight :
                                    gConfigSettings.genColors.crossPostChk);
                       console.print(color + CHECK_CHAR);
                     }
                  }
                  else
                  {
                     // Go to the bottom row of the selection box and display an error that
                     // the user can't post in the selected sub-board and pause for a moment.
                     writeCantPostErrMsg(pSelBoxUpperLeft.x, pSelBoxLowerRight.y, chosenMsgSubIndex+1,
                                         pSelBoxWidth, pSelBoxInnerWidth, cantPostErrPauseMS, curpos);
                  }
               }
            }

            console.gotoxy(originalCurpos);
         }
         break;
    }
  }

  return retObj;
}
// Displays a screenful of message groups, for the cross-posting
// interface.
//
// Parameters:
//  pGrpIndex: The message group index
//  pStartIndex: The message group index to start at (0-based)
//  pSelectedIndex: The index of the currently-selected message sub
//  pStartScreenRow: The row on the screen to start at (1-based)
//  pStartScreenCol: The column on the screen to start at (1-based)
//  pEndScreenRow: The row on the screen to end at (1-based)
//  pEndScreenCol: The column on the screen to end at (1-based)
//  pSubNumFieldLen: The length to use for the sub-board number field
//  pBlankToEndRow: Boolean - Whether or not to write blank lines to the end
//                  screen row if there aren't enough message groups to fill
//                  the screen.
function ListScreenfulOfMsgSubs(pGrpIndex, pStartIndex, pSelectedIndex, pStartScreenRow,
                                 pStartScreenCol, pEndScreenRow, pEndScreenCol,
                                 pSubNumFieldLen, pBlankToEndRow)
{
   // If the parameters are invalid, then just return.
   if ((typeof(pGrpIndex) != "number") || (typeof(pStartIndex) != "number") ||
       (typeof(pSelectedIndex) != "number") || (typeof(pStartScreenRow) != "number") ||
       (typeof(pStartScreenCol) != "number") || (typeof(pEndScreenRow) != "number") ||
       (typeof(pEndScreenCol) != "number"))
   {
      return;
   }
   if ((pGrpIndex < 0) || (pGrpIndex >= msg_area.grp_list.length))
      return;
   if ((pStartIndex < 0) || (pStartIndex >= msg_area.grp_list[pGrpIndex].sub_list.length))
      return;
   if ((pStartScreenRow < 1) || (pStartScreenRow > console.screen_rows))
      return;
   if ((pEndScreenRow < 1) || (pEndScreenRow > console.screen_rows))
      return;
   if ((pStartScreenCol < 1) || (pStartScreenCol > console.screen_columns))
      return;
   if ((pEndScreenCol < 1) || (pEndScreenCol > console.screen_columns))
      return;

   // If pStartScreenRow is greater than pEndScreenRow, then swap them.
   // Do the same with pStartScreenCol and pEndScreenCol.
   if (pStartScreenRow > pEndScreenRow)
   {
      var temp = pStartScreenRow;
      pStartScreenRow = pEndScreenRow;
      pEndScreenRow = temp;
   }
   if (pStartScreenCol > pEndScreenCol)
   {
      var temp = pStartScreenCol;
      pStartScreenCol = pEndScreenCol;
      pEndScreenCol = temp;
   }

   // Calculate the ending index to use for the message groups array.
   var endIndex = pStartIndex + (pEndScreenRow-pStartScreenRow);
   if (endIndex >= msg_area.grp_list[pGrpIndex].sub_list.length)
      endIndex = msg_area.grp_list[pGrpIndex].sub_list.length - 1;
   var onePastEndIndex = endIndex + 1;

   // Go to the specified screen row, and display the message group information.
   var textWidth = pEndScreenCol - pStartScreenCol + 1;
   var row = pStartScreenRow;
   var msgSubIndex = pStartIndex;
   for (; msgSubIndex < onePastEndIndex; ++msgSubIndex)
   {
      console.gotoxy(pStartScreenCol, row++);
      // The 5th parameter to writeMsgSubLine() is whether or not to
      // write the message sub information with highlight colors.
      writeMsgSubLine(pGrpIndex, msgSubIndex, textWidth, pSubNumFieldLen,
                      (msgSubIndex == pSelectedIndex));
   }

   // If pBlankToEndRow is true and we're not at the end row yet, then
   // write blank lines to the end row.
   if (pBlankToEndRow)
   {
      var screenRow = pStartScreenRow + (endIndex - pStartIndex) + 1;
      if (screenRow <= pEndScreenRow)
      {
         console.print("n");
         var areaWidth = pEndScreenCol - pStartScreenCol + 1;
         var formatStr = "%-" + areaWidth + "s";
         for (; screenRow <= pEndScreenRow; ++screenRow)
         {
            console.gotoxy(pStartScreenCol, screenRow)
            printf(formatStr, "");
         }
      }
   }
}
// Writes a message sub-board information line (for choosing a message sub-board
// for cross-posing).
//
// Parameters:
//  pGrpIndex: The index of the message group (assumed to be valid)
//  pSubIndex: The index of the message sub-board within the group (assumed to be valid)
//  pSubCode: The sub-board code (assumed to be valid)
//  pTextWidth: The maximum text width
//  pSubNumFieldLen: The length to use for the sub-board number field
//  pHighlight: Boolean - Whether or not to write the line highlighted.
function writeMsgSubLine(pGrpIndex, pSubIndex, pTextWidth, pSubNumFieldLen, pHighlight)
{
   if ((typeof(pGrpIndex) != "number") || (typeof(pSubIndex) != "number") ||
       (typeof(pTextWidth) != "number"))
   {
      return;
   }

   // Put together the printf format string
   var msgSubDescLen = pTextWidth - pSubNumFieldLen - 2;
   var printfStr = "n";
   if (pHighlight)
   {
     printfStr += gConfigSettings.genColors.crossPostChkHighlight + "%1s"
               + gConfigSettings.genColors.crossPostMsgAreaNumHighlight + "%" + pSubNumFieldLen + "d "
               + gConfigSettings.genColors.crossPostMsgAreaDescHighlight + "%-" + msgSubDescLen + "s";
   }
   else
   {
     printfStr += gConfigSettings.genColors.crossPostChk + "%1s"
               + gConfigSettings.genColors.crossPostMsgAreaNum + "%" + pSubNumFieldLen + "d "
               + gConfigSettings.genColors.crossPostMsgAreaDesc + "%-" + msgSubDescLen + "s";
   }

   // Write the message group information line
   var subCode = msg_area.grp_list[pGrpIndex].sub_list[pSubIndex].code;
   printf(printfStr, (gCrossPostMsgSubs.subCodeExists(subCode) ? CHECK_CHAR : " "), +(pSubIndex+1),
          msg_area.grp_list[pGrpIndex].sub_list[pSubIndex].description.substr(0, msgSubDescLen));
}

// Writes a line in the edit lines array
//
// Parameters:
//  pIndex: Integer - The index of the line to write. Required.
//  pUseColors: Boolean - Whether or not to use the line's colors.
//              Optional.  If omitted, the colors will be used.
//  pStart: Integer - The index in the line of where to start.
//          Optional.  If omitted, 0 will be used.
//  pLength: Integer - The length to write.  Optional.  If
//           omitted, the entire line will be written.  <= 0 can be
//           passed to write the entire string.
//
// Return value: The actual length of text written
function printEditLine(pIndex, pUseColors, pStart, pLength)
{
   if (typeof(pIndex) != "number")
      return 0;
   var useColors = true;
   var start = 0;
   var length = -1;
   if (typeof(pUseColors) == "boolean")
      useColors = pUseColors;
   if (typeof(pStart) == "number")
      start = pStart;
   if (typeof(pLength) == "number")
      length = pLength;
   // Validation of variable values
   if (pIndex < 0)
      pIndex = 0;
   else if (pIndex >= gEditLines.length)
   {
      // Before returning, write spaces for the length specified so
      // that the screen is updated correctly
      for (var i = 0; i < length; ++i)
         console.print(" ");
      return length;
   }
   if (start < 0)
      start = 0;
   else if (start >= gEditLines[pIndex].text.length)
   {
      // Before returning, write spaces for the length specified so
      // that the screen is updated correctly
      for (var i = 0; i < length; ++i)
         console.print(" ");
      return length;
   }
   //if (length > (gEditLines[pIndex].text.length - start))
   //   length = gEditLines[pIndex].text.length - start;

   var lengthWritten = 0;
   if (useColors)
   {
   }
   else
   {
      // Don't use the line colors
      // Cases where the start index is at the beginning of the line
      if (start == 0)
      {
         // Simplest case: start is 0 and length is negative -
         // Just print the entire line.
         lengthWritten = gEditLines[pIndex].text.length;
         if (length <= 0)
            console.print(gEditLines[pIndex].text);
         else
         {
            var textToWrite = gEditLines[pIndex].text.substr(start, length);
            console.print(textToWrite);
            lengthWritten = textToWrite.length;
         }
      }
      else
      {
         // Start is > 0
         var textToWrite = "";
         if (length <= 0)
            textToWrite = gEditLines[pIndex].text.substr(start);
         else
            textToWrite = gEditLines[pIndex].text.substr(start, length);
         console.print(textToWrite);
         lengthWritten = textToWrite.length;
      }
   }
   return lengthWritten;
}