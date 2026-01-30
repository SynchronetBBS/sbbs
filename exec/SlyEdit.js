/* This is a text editor for Synchronet designed to mimic the look & feel of
 * DCTEdit and IceEdit, since neither of those editors have been developed
 * for quite a while and still exist only as 16-bit DOS applications.
 *
 * Author: Eric Oulashin (AKA Nightfox)
 * BBS: Digital Distortion
 * BBS address: digdist.bbsindex.com
 *
 */

"use strict";

/* Command-line arguments:
 1 (argv[0]): Filename to read/edit
 2 (argv[1]): Editor mode ("DCT", "ICE", or "RANDOM")
*/

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

// Load required JavaScript libraries
var requireFnExists = (typeof(require) === "function");
if (requireFnExists)
{
	require("sbbsdefs.js", "K_NOCRLF");
	require("attr_conv.js", "syncAttrCodesToANSI");
	require("text.js", "AreYouThere");
	require("frame.js", "Frame");
	require("scrollbar.js", "ScrollBar");
	require("slyedit_misc.js", "gUserSettingsFilename");
	require("choice_scroll_box.js", "ChoiceScrollbox");
}
else
{
	load("sbbsdefs.js");
	load("attr_conv.js");
	load("text.js");
	load("frame.js");
	load("scrollbar.js");
	load("slyedit_misc.js");
	load("choice_scroll_box.js");
}

// Load program settings from SlyEdit.cfg, and load the user configuratio nsettings
var gConfigSettings = ReadSlyEditConfigFile();
var gUserSettings = ReadUserSettingsFile(gConfigSettings);
// Load any specified 3rd-party startup scripts
for (var i = 0; i < gConfigSettings.thirdPartyLoadOnStart.length; ++i)
	load(gConfigSettings.thirdPartyLoadOnStart[i]);
// Execute any provided startup JavaScript commands
for (var i = 0; i < gConfigSettings.runJSOnStart.length; ++i)
	eval(gConfigSettings.runJSOnStart[i]);

const EDITOR_PROGRAM_NAME = "SlyEdit";
const ERRORMSG_PAUSE_MS = 1500;
const TEXT_SEARCH_PAUSE_MS = 1500;
const SPELL_CHECK_PAUSE_MS = 1000;

// This script requires Synchronet version 3.14 or higher.
// Exit if the Synchronet version is below the minimum.
if (system.version_num < 31500) // 3.15 for user.is_sysop
{
	console.attributes = "N";
	console.crlf();
	console.print("\x01n\x01h\x01y\x01i* Warning:\x01n\x01h\x01w " + EDITOR_PROGRAM_NAME);
	console.print(" " + "requires version \x01g3.15\x01w or");
	console.crlf();
	console.print("higher of Synchronet.  This BBS is using version \x01g");
	console.print(system.version + "\x01w.  Please notify the sysop.");
	console.crlf();
	console.pause();
	exit(1); // 1: Aborted
}
// If the user's terminal doesn't support ANSI, or if their terminal is less
// than 80 characters wide, then exit.
if (!console.term_supports(USER_ANSI))
{
	console.print("\x01n\r\n\x01h\x01yERROR: \x01w" + EDITOR_PROGRAM_NAME +
	              " requires an ANSI terminal.\x01n\r\n\x01p");
	exit(1); // 1: Aborted
}
if (console.screen_columns < 80)
{
	console.print("\x01n\r\n\x01h\x01w" + EDITOR_PROGRAM_NAME + " requires a terminal width of at least 80 characters.\x01n\r\n\x01p");
	exit(1); // 1: Aborted
}

// Version information
var EDITOR_VERSION = "1.92f";
var EDITOR_VER_DATE = "2026-01-30";


// Program variables
var gEditTop = 6;                         // The top line of the edit area
var gEditBottom = console.screen_rows-2;  // The last line of the edit area
// gEditLeft and gEditRight are the rightmost and leftmost columns of the edit
// area, respectively.  They default to an edit area 80 characters wide
// in the center of the screen, but for IceEdit mode, the edit area will
// be on the left side of the screen to match up with the screen header.
// gEditLeft and gEditRight are 1-based.
//var gEditLeft = (console.screen_columns/2).toFixed(0) - 40 + 1;
//var gEditRight = gEditLeft + 79; // Based on gEditLeft being 1-based
var gEditLeft = 1;
var gEditRight = gEditLeft + console.screen_columns - 1; // Based on gEditLeft being 1-based
/*
// If the screen has less than 80 columns, then use the whole screen.
if (console.screen_columns < 80)
{
   gEditLeft = 1;
   gEditRight = console.screen_columns;
}
*/

// Temporary (for testing): Make the edit area small
//gEditLeft = 25;
//gEditRight = 45;
//gEditBottom = gEditTop + 1;
// End Temporary

// Calculate the edit area width & height
var gEditWidth = gEditRight - gEditLeft + 1;
var gEditHeight = gEditBottom - gEditTop + 1;


// Even though SlyEdit is always editing a file, this
// stores returns whether or not we're simply editing
// a file (as opposed to posting a message).
var gJustEditingAFile = false;

// Whether the user can change the subject
var gCanChangeSubject = true;

// Colors
var gQuoteWinTextColor = "\x01n\x01" + "7\x01k";   // Normal text color for the quote window (DCT default)
var gQuoteLineHighlightColor = "\x01n\x01w"; // Highlighted text color for the quote window (DCT default)
var gTextAttrs = "\x01n\x01w";               // The text color for edit mode
var gQuoteLineColor = "\x01n\x01c";          // The text color for quote lines

// When using color attributes and inserting text into a line, there are situations where we might want to
// start at index+1 when shifting attribute codes to the right, and other times we want to start at index.
// This defaults to true.
var gTextInsertColorShiftIndexPlusOne = true;

// gQuotePrefix contains the text to prepend to quote lines.
// gQuotePrefix will later be updated to include the message sender's
// initials or first 2 letters of their username.
var gQuotePrefix = " > ";

// gCrossPostMsgSubs will contain sub-boards for which the user wants to
// cross-post their message into.  Sub-board codes will be contained in
// objects whose name is the index to the message group in msg_area.grp_list
// to which the sub-board codes belong.
var gCrossPostMsgSubs = {};
// This function returns whether or not a property of the gCrossPostMsgSubs
// object is one of its member functions (i.e., something to skip when looking
// only for the message groups).
//
// Parameters:
//  pPropName: The name of a property
//
// Return value: Boolean - Whether or not the property is the name of one of the
//               functions in the gCrossPostMsgSubs object
gCrossPostMsgSubs.propIsFuncName = function(pPropName) {
   return((pPropName == "propIsFuncName") || (pPropName == "add") || (pPropName == "remove") ||
           (pPropName == "subCodeExists") || (pPropName == "numMsgGrps") ||
           (pPropName == "numSubBoards"));
};
// This function returns whether or not the a message sub-coard code exists in
// gCrossPostMsgSubs.
//
// Parameters:
//  pSubCode: The sub-code to look for
gCrossPostMsgSubs.subCodeExists = function(pSubCode) {
	if (typeof(pSubCode) != "string")
		return false;
	if (pSubCode === "")
		return false;

	var foundIt = false;
	if (typeof(msg_area.sub[pSubCode]) === "object")
	{
		var grpIndex = msg_area.sub[pSubCode].grp_index;
		if (this.hasOwnProperty(grpIndex))
			foundIt = this[grpIndex].hasOwnProperty(pSubCode);
	}
	return foundIt;
};
// This function adds a sub-board code to gCrossPostMsgSubs.
//
// Parameters:
//  pSubCode: The sub-code to add
gCrossPostMsgSubs.add = function(pSubCode) {
	if (typeof(pSubCode) != "string")
		return;
	if (pSubCode === "")
		return;
	if (this.subCodeExists(pSubCode))
		return;

	var grpIndex = msg_area.sub[pSubCode].grp_index;
	if (!this.hasOwnProperty(grpIndex))
		this[grpIndex] = {};
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
		if (!this.propIsFuncName(prop))
			++msgGrpCount;
	}
	return msgGrpCount;
};
// This function returns the number of sub-boards the user has chosen to post
// the message into.
gCrossPostMsgSubs.numSubBoards = function () {
	var numMsgSubs = 0;
	for (var grpIndex in this)
	{
		if (!this.propIsFuncName(grpIndex))
		{
			for (var subCode in gCrossPostMsgSubs[grpIndex])
				++numMsgSubs;
		}
	}
	return numMsgSubs;
};



// Set up some standard function names for various screen/UI functionality
// that are common to the Ice & DCT styles.
var fpDrawQuoteWindowTopBorder = null;
var fpDisplayTextAreaBottomBorder = null;
var fpDrawQuoteWindowBottomBorder = null;
var fpRedrawScreen = null;
var fpUpdateInsertModeOnScreen = null;
var fpDisplayBottomHelpLine = null;
var fpDisplayTime = null;
var fpDisplayTimeRemaining = null;
var fpCallESCMenu = null;
var fpRefreshSubjectOnScreen = null;
var fpGlobalScreenVarsSetup = null;
// Subject screen position & length (for changing the subject).  Note: These
// will be set in the IceStuff or DCTStuff scripts, depending on which theme
// is being used.
var gSubjPos = {
	x: 0,
	y: 0
};
var gSubjScreenLen = 0;
if (EDITOR_STYLE == "DCT")
{
	if (requireFnExists)
		require("slyedit_dct_stuff.js", "DrawQuoteWindowTopBorder_DCTStyle");
	else
		load("slyedit_dct_stuff.js");
	gEditTop = 6;
	gQuoteWinTextColor = gConfigSettings.DCTColors.QuoteWinText;
	gQuoteLineHighlightColor = gConfigSettings.DCTColors.QuoteLineHighlightColor;
	gTextAttrs = "\x01n";
	gQuoteLineColor = gConfigSettings.DCTColors.QuoteLineColor;

	// Function pointers for the DCTEdit-style screen update functions
	fpDrawQuoteWindowTopBorder = DrawQuoteWindowTopBorder_DCTStyle;
	fpDisplayTextAreaBottomBorder = DisplayTextAreaBottomBorder_DCTStyle;
	fpDrawQuoteWindowBottomBorder = DrawQuoteWindowBottomBorder_DCTStyle;
	fpRedrawScreen = redrawScreen_DCTStyle;
	fpUpdateInsertModeOnScreen = updateInsertModeOnScreen_DCTStyle;
	fpDisplayBottomHelpLine = DisplayBottomHelpLine_DCTStyle;
	fpDisplayTime = displayTime_DCTStyle;
	fpDisplayTimeRemaining = displayTimeRemaining_DCTStyle;
	fpCallESCMenu = callDCTESCMenu;
	fpGlobalScreenVarsSetup = globalScreenVarsSetup_DCTStyle;

	// Note: gSubjScreenLen is set in redrawScreen_DCTStyle()
	fpRefreshSubjectOnScreen = refreshSubjectOnScreen_DCTStyle;
}
else if (EDITOR_STYLE == "ICE")
{
	if (requireFnExists)
		require("slyedit_ice_stuff.js", "DrawQuoteWindowTopBorder_IceStyle");
	else
		load("slyedit_ice_stuff.js");
	gEditTop = 5;
	gQuoteWinTextColor = gConfigSettings.iceColors.QuoteWinText;
	gQuoteLineHighlightColor = gConfigSettings.iceColors.QuoteLineHighlightColor;
	gTextAttrs = "\x01n";
	gQuoteLineColor = gConfigSettings.iceColors.QuoteLineColor;

	// Function pointers for the IceEdit-style screen update functions
	fpDrawQuoteWindowTopBorder = DrawQuoteWindowTopBorder_IceStyle;
	fpDisplayTextAreaBottomBorder = DisplayTextAreaBottomBorder_IceStyle;
	fpDrawQuoteWindowBottomBorder = DrawQuoteWindowBottomBorder_IceStyle;
	fpRedrawScreen = redrawScreen_IceStyle;
	fpUpdateInsertModeOnScreen = updateInsertModeOnScreen_IceStyle;
	fpDisplayBottomHelpLine = DisplayBottomHelpLine_IceStyle;
	fpDisplayTime = displayTime_IceStyle;
	fpDisplayTimeRemaining = displayTimeRemaining_IceStyle;
	fpCallESCMenu = callIceESCMenu;
	fpGlobalScreenVarsSetup = globalScreenVarsSetup_IceStyle;

	// Note: gSubjScreenLen is set in redrawScreen_IceStyle()
	fpRefreshSubjectOnScreen = refreshSubjectOnScreen_IceStyle;
}

// Set up any required global screen variables
fpGlobalScreenVarsSetup();

// Message display & edit variables
var gInsertMode = "INS";       // Insert (INS) or overwrite (OVR) mode
var gQuoteLines = [];          // Array of quote lines loaded from file, if in quote mode
// Some boolean values for whether any of the quote lines had certain color codes in them.
var gUserHasOpenedQuoteWindow = false; // Whether or not the user has opened the quote line selection window
var gQuoteLinesTopIndex = 0;   // Index of the first displayed quote line
var gQuoteLinesIndex = 0;      // Index of the current quote line
// The gEditLines array will contain TextLine objects storing the line
// information.
var gEditLines = [];
var gEditLinesIndex = 0;      // Index into gEditLines for the line being edited
var gTextLineIndex = 0;       // Index into the current text line being edited
// Format strings used for printf() to display text in the edit area
const gFormatStr = "%-" + gEditWidth + "s";
const gFormatStrWithAttr = "%s%-" + gEditWidth + "s";
// Will contain valid word characters, for spell checking
var gValidWordChars = "";

// Whether or not a message file was uploaded (if so, then SlyEdit won't
// do the line re-wrapping at the end before saving the message).
var gUploadedMessageFile = false;

// Definitions for actions to take after the Enter key is pressed
const ENTER_ACTION_NONE = 0;
const ENTER_ACTION_DO_QUOTE_SELECTION = 1;
const ENTER_ACTION_DO_CROSS_POST_SELECTION = 2;
const ENTER_ACTION_DO_MEME_INPUT = 3;
const ENTER_ACTION_SHOW_HELP = 4;

// gEditAreaBuffer will be an array of strings for the edit area, which
// will be checked by displayEditLines() before outputting text lines
// to optimize the update of message text on the screen. displayEditLines()
// will also update this array after writing a line of text to the screen.
// The indexes in this array are the absolute screen lines.
var gEditAreaBuffer = [];
function clearEditAreaBuffer()
{
	for (var lineNum = gEditTop; lineNum <= gEditBottom; ++lineNum)
		gEditAreaBuffer[lineNum] = "";
}
clearEditAreaBuffer();
// gTxtreplacements will be an associative array that stores words (in upper
// case) to be replaced with other words.
var gNumTxtReplacements = 0;
var gTxtReplacements = {};
if (gConfigSettings.enableTextReplacements)
	gNumTxtReplacements = populateTxtReplacements(gTxtReplacements, gConfigSettings.textReplacementsUseRegex, gConfigSettings.allowColorSelection);

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

// When exiting this script, make sure to set the bbs.sys_status & ctrl key pasthru values
// back to what they were originally
js.on_exit("bbs.sys_status = " + bbs.sys_status);
js.on_exit("console.ctrlkey_passthru = " + console.ctrlkey_passthru);

// Data for an "Are you there?" timeout warning handler
js.global.slyEditData = {
	bottomHelpLineRow: console.screen_rows,
	useQuotes: gUseQuotes,
	displayBottomhelpLine: fpDisplayBottomHelpLine,
	replaceAtCodesInStr: replaceAtCodesInStr
};
Object.defineProperty(js.global, "SlyEdit_areYouThereProp", {
	configurable: true, // Allows this property to be deleted and re-defined as needed
	get: function()
	{
		// blank out the AreYouThere timeout warning string (used by
		// console.getkey()), which would interfere with full-screen display and scrolling display
		// functionality.
		bbs.replace_text(AreYouThere, "");

		var originalCurPos = console.getxy();
		var originalAtrs = console.attributes;

		console.beep();
		var warningTxt = js.global.slyEditData.replaceAtCodesInStr(gConfigSettings.strings.areYouThere);
		var numSpaces = Math.floor(console.screen_columns / 2) - Math.floor(console.strlen(warningTxt) / 2);
		if (numSpaces > 0)
			warningTxt = format("%*s", numSpaces, "") + warningTxt;
		if (console.strlen(warningTxt) >= console.screen_columns)
			warningTxt = warningTxt.substr(0, console.screen_columns-1);
		warningTxt = "\x01n" + warningTxt;
		console.gotoxy(1, js.global.slyEditData.bottomHelpLineRow);
		console.print(warningTxt);
		console.cleartoeol("\x01n");
		mswait(1500);
		js.global.slyEditData.displayBottomhelpLine(js.global.bottomHelpLineRow, js.global.useQuotes);

		// Put a key into the input buffer so that Synchronet isn't waiting for
		// a keypress after the "Are you there" warning
		console.ungetstr(KEY_ENTER);

		console.gotoxy(originalCurPos);
		console.attributes = originalAtrs;

		return "";
	}
});
// Replace the system's AreYouThere text line with a @JS to show the
// global SlyEdit_areYouThereProp property that has been set up
// with a custom getter function to display "Are you there?" at a
// good place on the screen temporarily and then refresh the screen.
bbs.replace_text(AreYouThere, "@JS:SlyEdit_areYouThereProp@");
// On script exit, delete the global SlyEdit_areYouThereProp property we have created
js.on_exit("delete SlyEdit_areYouThereProp;");
js.on_exit("delete js.global.slyEditData;");
js.on_exit("bbs.revert_text(AreYouThere);");

// Update the user's status on the BBS
bbs.sys_status &=~SS_PAUSEON;
bbs.sys_status |= SS_PAUSEOFF;
// Update the ctrl key passthru to support what SlyEdit needs (only while SlyEdit is running)
console.ctrlkey_passthru = "+ACGKLOPQRTUVWXYZ_";
// Enable delete line in SyncTERM (Disabling ANSI Music in the process)
console.write("\x1B[=1M"); // 1B (27): ESC
console.clear();

// Read information from the drop file (msginf), if it exists
var gMsgAreaInfo = null; // Will store the value returned by getCurMsgInfo().
var setMsgAreaInfoObj = false;
var gMsgSubj = "";
var gFromName = user.alias;
var gToName = "";
var gMsgArea = "";
var gTaglineFilename = "";
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
			var info = dropFile.readAll();
			dropFile.close();

			//https://wiki.synchro.net/ref:msginf

			// If the msginf file has at least 8 lines, the 8th line
			// specifies the character set (CP437 or UTF-8)
			if (info.length >= 8)
				gConfiguredCharset = info[7].toUpperCase();
			
			gFromName = info[0];
			gToName = info[1];
			gMsgSubj = info[2];
			gMsgArea = info[4];

			// Now that we know the name of the message area
			// that the message is being posted in, call
			// getCurMsgInfo() to set gMsgAreaInfo.
			gMsgAreaInfo = getCurMsgInfo(gMsgArea);
			setMsgAreaInfoObj = true;
			// If there is no sub-board code, that means we're editing a
			// regular file:
			// - Disable quote selection
			// - Make the 'to' name just the filename without the full
			// path
			// - Don't allow changing the subject
			// - Enable color selection regardless of the configuration
			//   (we could be editing an .asc file for the BBS, for instance)
			if (gMsgAreaInfo.subBoardCode.length == 0)
			{
				gUseQuotes = false;
				// It's possible that a script could call console.editfile()
				// with a 'to', 'from', subject, and message area to provide
				// that metadata when editing a file (i.e., maybe the caller
				// is a news reader that wants to post a message), but if
				// not, the 'to' name will be the filename (and the 'from'
				// name and subject will be blank). The file could be a
				// new file, so it might not exist yet.
				// There's also a case where Synchronet will use an editor to
				// edit things like an extended file description. In that case,
				// just the filename will be in the subject. The way this is
				// coded now, it won't set gJustEditingAFile to true, and
				// that might actually be desirable when editing file descriptions
				// & such, due to handling of spaces at the ends of lines.
				if (gToName.length > 0 && gFromName.length == 0 && gMsgSubj.length == 0)
				{
					gJustEditingAFile = true;
					//gToName = file_getname(gToName);
					//gMsgSubj = "Editing a file";
					gMsgSubj = file_getname(gToName);
					gToName = "";
					gCanChangeSubject = false;
					gConfigSettings.allowColorSelection = true;
					js.global.slyEditData.useQuotes = false;
					// Should we also disable tag lines here?
				}
				else if (gToName.length == 0 && gFromName.length == 0 && gMsgSubj.length > 0)
				{
					// This is the case where we might be editing a file description
					// (not sure what other cases might result in just the subject being
					// populated). In this case, we probably shouldn't allow changing
					// the subject.
					gCanChangeSubject = false;
					gConfigSettings.allowColorSelection = true;
					js.global.slyEditData.useQuotes = false;
					// Should we also disable tag lines here?
				}
			}

			// If the msginf file has at least 7 lines, then the 7th line is the full
			// path & filename of the tagline file, where we can write the
			// user's chosen tag line (if the user has that option enabled)
			// for the BBS software to read the tagline & insert it at the
			// proper place in the message.
			if (info.length >= 7)
				gTaglineFilename = info[6];
		}
	}
	file_remove(dropFileName);
}
else
{
	// There is no msginf - Try editor.inf
	dropFileName = file_getcase(system.node_dir + "editor.inf");
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

				gMsgArea = "";
				if (userEditorCfgHasUTF8Enabled())
				{
					gFromName = utf8_decode(info[3]);
					gToName = utf8_decode(info[1]);
					gMsgSubj = utf8_decode(info[0]);
				}
				else
				{
					gFromName = info[3];
					gToName = info[1];
					gMsgSubj = info[0];
				}

				// TODO: If we can know the name of the message
				// area name, we can call getCurMsgInfo().  But
				// editor.inf doesn't have the message area name.
				// Now that we know the name of the message area
				// that the message is being posted in, call
				// getCurMsgInfo() to set gMsgAreaInfo.
				//gMsgAreaInfo = getCurMsgInfo(gMsgArea);
				//setMsgAreaInfoObj = true;
			}
		}
		file_remove(dropFileName);
	}
}
// If gMsgAreaInfo hasn't been set yet, then set it.
if (!setMsgAreaInfoObj)
{
	gMsgAreaInfo = getCurMsgInfo(gMsgArea);
	setMsgAreaInfoObj = true;
}

// Set a variable to store whether or not cross-posting can be done.
var gCanCrossPost = (gMsgAreaInfo.subBoardCode.length > 0 && gConfigSettings.allowCrossPosting && postingInMsgSubBoard(gMsgArea));
// If the user is posting in a message sub-board, then add its information
// to gCrossPostMsgSubs.
if (gMsgAreaInfo.subBoardCode.length > 0 && postingInMsgSubBoard(gMsgArea))
	gCrossPostMsgSubs.add(gMsgAreaInfo.subBoardCode);

// Open the quote file / message file
readQuoteOrMessageFile();

// Store a copy of the current subject (possibly allowing the user to
// change the subject in the future)
var gOldSubj = gMsgSubj;

// Make sure the console has the normal attribute
// before starting the editor, in case any background
// color etc. was set
console.attributes = "N";

// Now it's edit time.
var exitCode = doEditLoop();

// Clear the screen and display the end-of-program information (if the setting
// is enabled).
console.clear("\x01n");
if (gConfigSettings.displayEndInfoScreen)
{
   displayProgramExitInfo(false);
   console.crlf();
}

// If the user wrote & saved a message, do post work:
// - If the user changed the subject, then write a RESULT.ED file with
//   the new subject.
// - Remove any extra blank lines that may be at the end of the message (in
// gEditLines), and output the message lines to a file with the passed-in
// input filename.
// - Do cross-posting.
var savedTheMessage = false;
if ((exitCode == 0) && (gEditLines.length > 0))
{
	// Write a RESULT.ED containing the subject and editor name.
	// Note: Normally, this is only supported for WWIV editors supporting
	// result.inf/result.ed.  However, with Synchronet 3.17c development
	// builds from July 21, 2019 onward, result.ed is supported even for
	// editors configured for QuickBBS MSGINF/MSGTMP.
	// RESULT.ED specs:
	// Line 1: Anonymous (1 for true, 0 for false) - Unused by Synchronet
	// Line 2: Message subject
	// Line 3: Editor details (e.g. full name, version/revision, date of build or release)
	// Note: When cross-posting, gMsgSubj will be used, which has the correct current
	// subject.
	// gMsgSubj is the current subject, and gOldSubj is the original subject
	var dropFile = new File(system.node_dir + "RESULT.ED");
	if (dropFile.open("w"))
	{
		dropFile.writeln("0");
		dropFile.writeln(gMsgSubj);
		dropFile.writeln(EDITOR_PROGRAM_NAME + " " + EDITOR_VERSION + " (" + EDITOR_VER_DATE + ") (" + EDITOR_STYLE + " style)");
		dropFile.close();
	}

	// Remove any extra blank lines at the end of the message
	var lineIndex = gEditLines.length - 1;
	while ((lineIndex > 0) && (lineIndex < gEditLines.length) && (gEditLines[lineIndex].screenLength() == 0))
	{
		gEditLines.splice(lineIndex, 1);
		--lineIndex;
	}

	var msgHasAttrCodes = messageLinesHaveAttrs();
	// If the user didn't upload a message from a file, then for paragraphs of
	// non-quote lines, make the paragraph all one line.  Copy to another array
	// of edit lines, and then set gEditLines to that array.
	if (!gUploadedMessageFile)
	{
		var newEditLines = [];
		// Go through each text block (which were identified earlier).  For quote blocks, add the lines
		// as-is without attribute/color codes.  For non-quote blocks, conbine the lines without hard newlines
		// (soft wraps) into one line each.
		var blockIdxes = findQuoteAndNonQuoteBlockIndexes(gEditLines);
		for (var blockIdx = 0; blockIdx < blockIdxes.allBlocks.length; ++blockIdx)
		{
			// If this block is a quote block, then add the lines from the block as-is to the new edit lines array.
			if (blockIdxes.allBlocks[blockIdx].isQuoteBlock)
			{
				for (var i = blockIdxes.allBlocks[blockIdx].start; i < blockIdxes.allBlocks[blockIdx].end; ++i)
				{
					var newTextLine = new TextLine(gEditLines[i].text, gEditLines[i].hardNewlineEnd, gEditLines[i].isQuoteLine);
					newTextLine.attrs = gEditLines[i].attrs;
					// If the message has attribute codes, ensure quote blocks start with a normal attribute
					if (msgHasAttrCodes && i == blockIdxes.allBlocks[blockIdx].start)
						newTextLine.attrs[0] = "\x01n";
					newEditLines.push(newTextLine);
					delete gEditLines[i]; // Save some memory
				}
			}
			else
			{
				// This is not a quote block.  For each paragraph/set of lines that don't have hard newline endings,
				// combine them into a single text line.  When we reach a line with a hard newline end, stop
				// combining into a text line & add that text line to the new edit lines array, then continue
				// on the same way with the rest of the block of text.
				var i = blockIdxes.allBlocks[blockIdx].start;
				while ((i < blockIdxes.allBlocks[blockIdx].end) && (i < gEditLines.length))
				{
					var textLine = ""; // We're starting a new text line
					var textAttrs = {};
					var attrIdxOffset = 0;  // For copying color/attribute codes to the new message line
					var addedSpace = false; // For color/attribute code offset correction if needed
					var continueOn = true;
					while (continueOn)
					{
						addedSpace = false;
						// New (2025-04-23): If the sub-board code is not empty, then
						// we're posting in the messagebase, so we should add a space
						// after the line. Otherwise, the user is editing a file, and
						// we don't want to do that.
						if (gMsgAreaInfo.subBoardCode.length > 0)
						{
							if (textLine.length > 0)
							{
								textLine += " ";
								addedSpace = true;
							}
						}
						else
						{
							// Editing a file (not posting a message for a sub-board or email)
							if (i > blockIdxes.allBlocks[blockIdx].start)
							{
								var prevLineIdx = i - 1;
								// TODO: This isn't working properly
								// Temporary
								//console.print("\x01n" + prevLineIdx + ", " + typeof(gEditLines[prevLineIdx]) + "\r\n"); // Temporary
								//console.print(i + ", " + typeof(gEditLines[i]) + "\r\n"); // Temporary
								// End Temporary
								// TODO: This doesn't seem to be adding the space as expected
								/*
								if (!gEditLines[prevLineIdx].hardNewlineEnd)
								{
									var maxLineLenForScreen = console.screen_columns - 1;
									var prevLineLen = console.strlen(gEditLines[prevLineIdx].text);
									// Temporary
									printf("\x01n%d - Max len: %d, prev line len: %d\r\n", i, maxLineLenForScreen, prevLineLen);
									printf("%s:\r\n\r\n", gEditLines[prevLineIdx].text); // Temporary
									// End Temporary
									if (prevLineLen < maxLineLenForScreen)
									{
										var numSpaces = maxLineLenForScreen - prevLineLen;
										//printf("\x01n# spaces: %d\r\n\r\n", numSpaces); // Temporary
										//textLine += format("%*s", numSpaces, "");
									}
								}
								*/
							}
						}
						if (addedSpace)
							++attrIdxOffset;
						textLine += gEditLines[i].text;
						for (var attrTxtIdx in gEditLines[i].attrs)
						{
							var newAttrTxtIdx = (+attrTxtIdx) + attrIdxOffset;
							textAttrs[newAttrTxtIdx] = gEditLines[i].attrs[attrTxtIdx];
						}
						attrIdxOffset = textLine.length;
						// When we reach a line with a hard newline end, or the end of the text block or edit lines,
						// stop accumulating the text into textLine.
						continueOn = (!gEditLines[i].hardNewlineEnd) && (i < blockIdxes.allBlocks[blockIdx].end) && (i+1 < gEditLines.length);
						// Save some memory
						if (gMsgAreaInfo.subBoardCode.length > 0) // If editing a message for a sub-board or mail
							delete gEditLines[i];
						else // Editing a file
						{
							if (typeof(gEditLines[i-1]) === "object")
								delete gEditLines[i-1];
						}
						++i;
					}
					var newTextLine = new TextLine(textLine, true, false);
					newTextLine.attrs = textAttrs;
					newEditLines.push(newTextLine);
				}
			}
		}
		gEditLines = newEditLines;
	}

	// Store whether the user is still posting the message in the original sub-board
	// and whether that's the only sub-board they're posting in.
	var postingInOriginalSubBoard = gCrossPostMsgSubs.subCodeExists(gMsgAreaInfo.subBoardCode);
	var postingOnlyInOriginalSubBoard = (postingInOriginalSubBoard && (gCrossPostMsgSubs.numSubBoards() == 1));

	// If some message sub-boards have been selected for cross-posting, then otuput
	// which sub-boards will be cross-posted into, and do the cross-posting.
	// TODO: If the user changed the subject, make sure the subject is correct when cross-posting.
	var crossPosted = false;
	if (gCrossPostMsgSubs.numMsgGrps() > 0)
	{
		// Read the user's signature, in case they have one
		var msgSigInfo = readUserSigFile();

		// If the will cross-post into other sub-boards, then create a string containing
		// the user's entire message.
		var msgContents = "";
		if (!postingOnlyInOriginalSubBoard)
		{
			// If using UTF-8, then build msgContents in a way that it will look as it should.
			if (gPrintMode & P_UTF8)
			{
				for (var i = 0; i < gEditLines.length; ++i)
				{
					var msgTxtLine = gEditLines[i].getText(true);
					for (var txtLineI = 0; txtLineI < msgTxtLine.length; ++txtLineI)
					{
						// Credit to Deuce for this code (this was seen in fseditor.js)
						var encoded = utf8_encode(msgTxtLine[txtLineI].charCodeAt(0));
						for (var encodedI = 0; encodedI < encoded.length; ++encodedI)
							msgContents += encoded[encodedI];
					}
					// Use a hard newline if the current edit line has one or if this is
					// the last line of the message.
					var useHardNewline = (gEditLines[i].hardNewlineEnd || (i == gEditLines.length-1));
					msgContents += (useHardNewline ? "\r\n" : " \n");
				}
			}
			else
			{
				// Not using UTF-8. Build msgContents directly from the message lines.
				for (var i = 0; i < gEditLines.length; ++i)
				{
					var msgTxtLine = gEditLines[i].getText(true);
					// Use a hard newline if the current edit line has one or if this is
					// the last line of the message.
					var useHardNewline = (gEditLines[i].hardNewlineEnd || (i == gEditLines.length-1));
					msgContents += gEditLines[i].getText(true) + (useHardNewline ? "\r\n" : " \n");
				}
			}

			// If the user has not chosen to auto-sign messages and we're posting a message in a
			// sub-board or personal email, then also append their signature to the message now.
			if (!gUserSettings.autoSignMessages && gMsgAreaInfo.subBoardCode.length > 0)
			{
				// Append a blank line to separate the message & signature.
				// Note: msgContents already has a newline at the end, so we don't have
				// to append one here; just append the signature.
				if (msgSigInfo.sigContents.length > 0)
				{
					if (messageLinesHaveAttrs())
						msgContents += "\x01n";
					msgContents += msgSigInfo.sigContents + "\r\n";
				}
			}
		}
		// If the message has any attribute codes, then append a code to set attributes back to normal
		if (msgHasAttrCodes)
		{
			msgContents += "\x01n";
			// If the option to save color codes as ANSI is enabled, then convert any Synchronet color codes
			// to ANSI in the message
			if (gConfigSettings.saveColorsAsANSI)
				msgContents = syncAttrCodesToANSI(msgContents);
		}

		console.attributes = "N";
		console.crlf();
		console.print("\x01n" + gConfigSettings.genColors.msgWillBePostedHdr + "Your message will be posted into the following area(s):");
		console.crlf();
		// If the user is posting in the originally-chosen sub-board and other sub-boards,
		// then make a log in the BBS log that the user is posting a message (for
		// cross-post logging).
		if (postingInOriginalSubBoard && !postingOnlyInOriginalSubBoard)
		{
			log(LOG_INFO, EDITOR_PROGRAM_NAME + ": " + user.alias + " is posting a message in " + msg_area.sub[gMsgAreaInfo.subBoardCode].grp_name +
			    " " + msg_area.sub[gMsgAreaInfo.subBoardCode].description + " (" + gMsgSubj + ")");
			bbs.log_str(EDITOR_PROGRAM_NAME + ": " + user.alias + " is posting a message in " + msg_area.sub[gMsgAreaInfo.subBoardCode].grp_name +
			            " " + msg_area.sub[gMsgAreaInfo.subBoardCode].description + " (" + gMsgSubj + ")");
		}
		var postMsgErrStr = ""; // For storing errors related to saving the message
		for (var grpIndex in gCrossPostMsgSubs)
		{
			// Skip the function names (we only want the group indexes)
			if (gCrossPostMsgSubs.propIsFuncName(grpIndex))
				continue;

			console.print("\x01n" + gConfigSettings.genColors.msgPostedGrpHdr + msg_area.grp_list[grpIndex].description + ":");
			console.crlf();
			for (var subCode in gCrossPostMsgSubs[grpIndex])
			{
				if (subCode == gMsgAreaInfo.subBoardCode)
				{
					printf("\x01n  " + gConfigSettings.genColors.msgPostedSubBoardName + "%-48s", msg_area.sub[subCode].description.substr(0, 48));
					console.print("\x01n " + gConfigSettings.genColors.msgPostedOriginalAreaText + "(original message area)");
				}
				// If subCode is not the user's current sub, then if the user is allowed
				// to post in that sub, then post the message there.
				else
				{
					// Write a log in the BBS log about which message area the user is
					// cross-posting into.
					log(LOG_INFO, EDITOR_PROGRAM_NAME + ": " + user.alias + " is cross-posting a message in " + msg_area.sub[subCode].grp_name +
					    " " + msg_area.sub[subCode].description + " (" + gMsgSubj + ")");
					bbs.log_str(EDITOR_PROGRAM_NAME + ": " + user.alias + " is cross-posting a message in " + msg_area.sub[subCode].grp_name +
					            " " + msg_area.sub[subCode].description + " (" + gMsgSubj + ")");

					// Write the cross-posting message area on the user's screen.
					printf("\x01n  " + gConfigSettings.genColors.msgPostedSubBoardName + "%-73s", msg_area.sub[subCode].description.substr(0, 73));
					if (msg_area.sub[subCode].can_post)
					{
						// If the user's auto-sign setting is enabled and we're posting in a
						// sub-board/personal email (not editing a file), then auto-sign the
						// message and append their signature afterward.  Otherwise, don't
						// auto-sign, and their signature has already been appended (if posting
						// a message).
						if (gUserSettings.autoSignMessages && gMsgAreaInfo.subBoardCode.length > 0)
						{
							var msgContents2 = msgContents + "\r\n";
							var userSignName = getSignName(subCode, gUserSettings.autoSignRealNameOnlyFirst, gUserSettings.autoSignEmailsRealName);
							// If the message has colors, then for some reason we need to add another \r.
							// Also, if the option to save the message as ANSI is enabled, then convert any
							// Synchronet color codes in the user's sign name to ANSI before appending it
							// to the message.
							if (msgHasAttrCodes)
							{
								msgContents2 += "\r";
								if (gConfigSettings.saveColorsAsANSI)
									userSignName = syncAttrCodesToANSI(userSignName);
							}
							msgContents2 += userSignName;
							msgContents2 += "\r\n\r\n";
							// If the user has a signature and we're posting a message (not editing a file), then
							// append the user's signature
							if (msgSigInfo.sigContents.length > 0 && gMsgAreaInfo.subBoardCode.length > 0)
							{
								if (messageLinesHaveAttrs())
									msgContents2 += "\x01n";
								msgContents2 += "\r\n" + msgSigInfo.sigContents + "\r\n";
							}
							postMsgErrStr = postMsgToSubBoard(subCode, gFromName, gToName, gMsgSubj, msgContents2, user.number);
						}
						else
							postMsgErrStr = postMsgToSubBoard(subCode, gFromName, gToName, gMsgSubj, msgContents, user.number);
						if (postMsgErrStr.length == 0)
						{
							savedTheMessage = true;
							crossPosted = true;
							console.print("\x01n\x01h\x01b[\x01n\x01g" + CP437_CHECK_MARK + "\x01n\x01h\x01b]\x01n");
						}
						else
						{
							console.print("\x01n\x01h\x01b[\x01rX\x01b]\x01n");
							console.crlf();
							console.print("   \x01n\x01h\x01r*\x01n " + postMsgErrStr);
							console.crlf();
						}
					}
					else
					{
						// The user isn't allowed to post in the sub.  Output a message
						// saying so.
						console.print("\x01n\x01h\x01b[\x01rX\x01b]\x01n");
						console.crlf();
						console.print("   \x01n\x01h\x01r*\x01n You're not allowed to post in this area.");
						console.crlf();
					}
				}
				console.crlf();
			}
		}
		console.attributes = "N";
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
				console.print("\x01n\x01c* Note: Your message has been cross-posted in areas other than your currently-");
				console.crlf();
				console.print("selected message area.  Because your message was not saved for your currently-");
				console.crlf();
				console.print("selected message area, the BBS will say the message was aborted, even");
				console.crlf();
				console.print("though it was posted in those other areas.  This is normal.\x01n");
				console.crlf();
				console.crlf();
			}
		}
	}
	if (saveMsgFile)
		saveMessageToFile(); // Save the message for being posted by Synchronet
}

// Saves the message to the file.  If no command-line arguments were passed to SlyEdit, then the
// filename will be INPUT.MSG in the system's temporary directory; otherwise, the first command-line
// argument will be used.
function saveMessageToFile()
{
	// If no command-line arguments were passed, then use INPUT.MSG in the system temporary directory; otherwise,
	// use the first command-line argument.
	var outputFilename = (argc == 0 ? system.temp_dir + "INPUT.MSG" : argv[0]);
	var msgFile = new File(outputFilename);
	//if (msgFile.open("w"))
	if (msgFile.open("wb")) // Open in binary mode to suppress end-of-line translations
	{
		// If the message has any attribute codes in it, then append a normal attribute code
		// to the end of the last line of the message. This is a workaround to ensure colors
		// are reset back to normal.
		var msgHasAttrCodes = messageLinesHaveAttrs();
		if (msgHasAttrCodes)
			gEditLines[gEditLines.length-1].text += "\x01n";
		// Write each line of the message to the file.
		// 2025-04-28: It used to be that "Expand Line Feeds to CRLF" option should
		// be turned on in SCFG for this to work properly for all platforms.
		// However, that probably isn't a problem now, and that option should now be
		// disabled so that line endings don't change in case we're editing a
		// file (rather than posting a messge).
		var lastLineIdx = gEditLines.length - 1;
		for (var i = 0; i < gEditLines.length; ++i)
		{
			//msgFile.writeln(gEditLines[i].getText(gConfigSettings.allowColorSelection));
			var msgTxtLine = gEditLines[i].getText(gConfigSettings.allowColorSelection);
			if (msgHasAttrCodes && gConfigSettings.saveColorsAsANSI)
				msgTxtLine = syncAttrCodesToANSI(msgTxtLine);
			// If UTF-8 is enabled, then write each character properly.  Otherwise, write the entire
			// line to the file as we'd normally do, with write() or writeln()
			if (gPrintMode & P_UTF8)
			{
				for (var txtLineI = 0; txtLineI < msgTxtLine.length; ++txtLineI)
				{
					// Credit to Deuce for this code (this was seen in fseditor.js)
					var encoded = utf8_encode(msgTxtLine[txtLineI].charCodeAt(0));
					for (var encodedI = 0; encodedI < encoded.length; ++encodedI)
						msgFile.writeBin(ascii(encoded[encodedI]), 1);
				}
				// Write an end-line
				// Note: The editor setting "Expand Line Feeds to CRLF" will cause
				// line endings to be CRLF (they'd normally be just LF in *nix)
				msgFile.writeln("");
			}
			else
			{
				// Note: The editor setting "Expand Line Feeds to CRLF" will cause
				// line endings to be CRLF (they'd normally be just LF in *nix)
				msgFile.writeln(msgTxtLine);
			}
		}
		// Auto-sign the message if the user's setting to do so is enabled and
		// we're posting a message (not editing a file)
		if (gUserSettings.autoSignMessages && gMsgAreaInfo.subBoardCode.length > 0)
		{
			msgFile.writeln("");
			var subCode = (postingInMsgSubBoard(gMsgArea) ? gMsgAreaInfo.subBoardCode : "mail");
			msgFile.writeln(getSignName(subCode, gUserSettings.autoSignRealNameOnlyFirst, gUserSettings.autoSignEmailsRealName));
		}
		msgFile.close();
		savedTheMessage = true;
	}
	else
		console.print("\x01n\x01r\x01h* Unable to save the message!\x01n\r\n");
}


// Set the end-of-program status message.
var editObjName = (gMsgAreaInfo.subBoardCode.length > 0 ? "message" : "file");
var operationName = (gMsgAreaInfo.subBoardCode.length > 0 ? "Message" : "Edit");
var endStatusMessage = "";
if (exitCode == 1)
	endStatusMessage = format("%s%s aborted.", gConfigSettings.genColors.msgAbortedText, operationName);
else if (exitCode == 0)
{
	if (gEditLines.length > 0)
	{
		if (savedTheMessage)
			endStatusMessage = format("%sThe %s has been saved.", gConfigSettings.genColors.msgHasBeenSavedText, editObjName);
		else
			endStatusMessage = format("%s%s aborted.", gConfigSettings.genColors.msgAbortedText, operationName);
	}
	else
		endStatusMessage = format("%sEmpty %s not saved.", gConfigSettings.genColors.emptyMsgNotSentText, editObjName);
}
// We shouldn't hit this else case, but it's here just to be safe.
else
	endStatusMessage = gConfigSettings.genColors.genMsgErrorText + "Possible message error.";
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

// Reads the quote/message file (must be done on startup)
function readQuoteOrMessageFile()
{
	var inputFile = new File(gInputFilename);
	// Open the quote file in "binary" mode (only applicable on Windows) - If
	// the quote file contains a Ctrl-Z (ASCII 26) char, it would be truncated
	// at that point. Some UTF-8 messages that include a "right arrow" unicode
	// code point are truncated to ASCII Ctrl-Z (ASCII 26) char, which is interpreted
	// by Windows as an "EOF" (end of file) marker for files open in text mode and the
	// file won't be read beyond that char.
	if (inputFile.open("rb", false))
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
				if (typeof(textLine) === "string")
				{
					// Convert any color/attribute codes from other BBS software to Synchronet attribute codes
					textLine = convertAttrsToSyncPerSysCfg(textLine, true);
					// TODO: I'd like to comment this out & leave attribute codes in the quote lines,
					// but that's currently causing wrapTextLinesForQuoting() to not find prefixes
					// properly & not wrap properly
					//textLine = strip_ctrl(textLine);
					// If the line has only whitespace and/or > characters,
					// then make the line blank before putting it into
					// gQuoteLines.
					if (/^[\s>]+$/.test(textLine))
						textLine = "";
					// Add the quote line to gQuotelines
					gQuoteLines.push(textLine);
					/*
					// If the quote line length is within the user's terminal width, then add it as-is.
					//if (textLine.length <= console.screen_columns-1)
					if (console.strlen(textLine) <= console.screen_columns-1)
						gQuoteLines.push(textLine);
					else
					{
						// Word-wrap the quote line to ensure the quote lines are no wider than the user's
						// terminal width (minus 1 character)
						var wrappedLine = word_wrap(textLine, console.screen_columns-1, textLine.length, false);
						var wrappedLines = wrappedLine.split("\n");
						// If splitting results in an empty line at the end of the array (due to a newline at the
						// end of the last line), then remove that line. Then add the wrapped lines to the quote lines.
						if (wrappedLines.length > 0 && wrappedLines[wrappedLines.length-1].length == 0)
							wrappedLines.splice(-1);
						for (var i = 0; i < wrappedLines.length; ++i)
							gQuoteLines.push(wrappedLines[i]);
					}
					*/
				}
			}
		}
		else
		{
			while (!inputFile.eof)
			{
				//gMsgAreaInfo.subBoardCode
				// If the line is too long to fit on the screen, then
				// split it (console.screen_columns-1)
				var textLineFromFile = inputFile.readln(8192);
				if (typeof(textLineFromFile) !== "string") // Shouldn't be true, but I've seen it before
					continue;
				var maxLineLen = console.screen_columns - 1;
				if (textLineFromFile.length > maxLineLen)
				{
					do
					{
						// Note: substrWithAttrCodes() is defined in dd_lightbar_menu.js
						var textLine = new TextLine();
						textLine.setText(substrWithAttrCodes(textLineFromFile, 0, maxLineLen));
						var textLineLen = console.strlen(textLineFromFile);
						textLine.hardNewlineEnd = (textLineLen <= maxLineLen);
						gEditLines.push(textLine);
						if (textLineLen > maxLineLen)
							textLineFromFile = substrWithAttrCodes(textLineFromFile, maxLineLen);
						else
							textLineFromFile = "";
					} while (console.strlen(textLineFromFile) > 0);
				}
				else
				{
					var textLine = new TextLine();
					textLine.setText(textLineFromFile);
					textLine.hardNewlineEnd = true;
					gEditLines.push(textLine);
				}
				// Old code - Does not split lines depending on terminal width:
				/*
				var textLine = new TextLine();
				var textLineFromFile = inputFile.readln(2048);
				if (typeof(textLineFromFile) == "string")
					textLine.setText(strip_ctrl(textLineFromFile));
				else
					textLine.setText("");
				textLine.hardNewlineEnd = true;
				// If there would still be room on the line for at least
				// 1 more character, then add a space to the end of the
				// line.
				if (textLine.screenLength() < console.screen_columns-1)
					textLine.text += " ";
				gEditLines.push(textLine);
				*/
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
}

// Edit mode & input loop
function doEditLoop()
{
	// Return codes:
	// 0: Success
	// 1: Aborted
	var returnCode = 0;

	// Set the shortcut keys.  Note - Avoid the following:
	// CTRL_B: Home key
	// CTRL_E: End key
	// CTRL_F: Right arrow key
	// CTRL_H: Backspace
	// CTRL_J: Down arrow key
	// CTRL_M: Enter key
	// CTRL_N: PageDown
	// CTRL_P: PageUp
	// CTRL_V: Insert key
	const ABORT_KEY                 = CTRL_A;
	const CROSSPOST_KEY             = CTRL_C;
	const DELETE_LINE_KEY           = CTRL_D;
	//const GENERAL_HELP_KEY          = CTRL_G;
	const GRAPHICS_CHAR_KEY         = CTRL_G;
	const TOGGLE_INSERT_KEY         = CTRL_I;
	const CHANGE_COLOR_KEY          = CTRL_K;
	const CMDLIST_HELP_KEY          = CTRL_L;
	const CMDLIST_HELP_KEY_2        = KEY_F1;
	const IMPORT_FILE_KEY           = CTRL_O;
	var QUOTE_KEY                   = gUserSettings.ctrlQQuote ? CTRL_Q : CTRL_Y;
	const SPELL_CHECK_KEY           = CTRL_R;
	const CHANGE_SUBJECT_KEY        = CTRL_S;
	const LIST_TXT_REPLACEMENTS_KEY = CTRL_T;
	const USER_SETTINGS_KEY         = CTRL_U;
	const SEARCH_TEXT_KEY           = CTRL_W;
	const EXPORT_TO_FILE_KEY        = CTRL_X;
	const SAVE_KEY                  = CTRL_Z;
	const INSERT_MEME_KEY_COMBO_1    = "/m";
	const INSERT_MEME_KEY_COMBO_2    = "/M";

	// Draw the screen.
	// Note: This is purposefully drawing the top of the message.  We
	// want to place the cursor at the first character on the top line,
	// too.  This is for the case where we're editing an existing message -
	// we want to start editigng it at the top.
	fpRedrawScreen(gEditLeft, gEditRight, gEditTop, gEditBottom, gTextAttrs, gInsertMode, gUseQuotes,
	               gUserSettings.ctrlQQuote, 0, displayEditLines);

	var curpos = {
		x: gEditLeft,
		y: gEditTop
	};
	console.gotoxy(curpos);

	// initialTimeLeft and updateTimeLeft will be used to keep track of the user's
	// time remaining so that we can update the user's time left on the screen.
	var initialTimeLeft = bbs.time_left;
	var updateTimeLeft = false;

	// Input loop
	var userInput = "";
	var currentWordLength = getWordLength(gEditLinesIndex, gTextLineIndex);
	var numKeysPressed = 0; // Used only to determine when to call updateTime()
	gTextInsertColorShiftIndexPlusOne = true; // Normally, shift attributes right starting at index+1 when inserting text
	var continueOn = true;
	while (continueOn)
	{
		// Get a key from the user
		userInput = console.getkey(K_NOCRLF|K_NOSPIN|K_NUL);

		// Replace the system's AreYouThere text line with a @JS to show the
		// global SlyEdit_areYouThereProp property that has been set up
		// with a custom getter function to display "Are you there?" at a
		// good place on the screen temporarily and then refresh the screen.
		// This is done here in the loop because the custom get function will
		// replace AreYouThere with "" to prevent the screen from getting
		// messy due to internal getkey() logic in Synchronet.
		bbs.replace_text(AreYouThere, "@JS:SlyEdit_areYouThereProp@");

		// If the cursor is at the end of the last line and the user
		// pressed the DEL key, then treat it as a backspace.  Some
		// terminals send a delete for backspace, particularly with
		// keyboards that have a delete key but no backspace key.
		var atEndOfLastLine = ((gEditLinesIndex == gEditLines.length - 1) && (gTextLineIndex == gEditLines[gEditLinesIndex].text.length));
		if (atEndOfLastLine && (userInput == KEY_DEL))
			userInput = BACKSPACE;

		if (!bbs.online)
		{
			var logStr = EDITOR_PROGRAM_NAME + ": User is no longer online (" + user.alias + " on node " + bbs.node_num + ")";
			bbs.log_str(logStr);
			log(LOG_INFO, logStr);
			continueOn = false;
			returnCode = 1; // Aborted
			saveMessageToFile();
		}
		// If userInput is null, then the input timeout was probably
		// reached, so abort.
		else if (userInput == null)
		{
			returnCode = 1; // Aborted
			continueOn = false;
			console.crlf();
			console.print("\x01n\x01h\x01r" + EDITOR_PROGRAM_NAME + ": Input timeout reached.");
			continue;
		}

		// If we get here, that means the timeout wasn't reached.
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
				console.attributes = "N"; // To avoid problems with background colors
				var editObjName = (gMsgAreaInfo.subBoardCode.length > 0 ? "message" : "edit");
				if (promptYesNo("Abort " + editObjName, false, "Abort", false, false))
				{
					returnCode = 1; // Aborted
					continueOn = false;
				}
				else
				{
					// Make sure the edit color attribute is set.
					//console.print("\x01n" + gTextAttrs);
					console.print(chooseEditColor());
				}
				break;
			case SAVE_KEY:
				returnCode = 0; // Save
				continueOn = false;
				break;
			case CMDLIST_HELP_KEY:
			case CMDLIST_HELP_KEY_2:
				displayCommandList(true, true, true, gCanCrossPost,
				                   gConfigSettings.enableTextReplacements, gConfigSettings.allowUserSettings,
				                   gConfigSettings.allowSpellCheck, gConfigSettings.allowColorSelection, gCanChangeSubject);
				clearEditAreaBuffer();
				fpRedrawScreen(gEditLeft, gEditRight, gEditTop, gEditBottom, gTextAttrs, gInsertMode,
				               gUseQuotes, gUserSettings.ctrlQQuote, gEditLinesIndex-(curpos.y-gEditTop),
				               displayEditLines);
				break;
			/*
			case GENERAL_HELP_KEY:
				displayGeneralHelp(true, true, true);
				clearEditAreaBuffer();
				fpRedrawScreen(gEditLeft, gEditRight, gEditTop, gEditBottom, gTextAttrs, gInsertMode,
				               gUseQuotes, gUserSettings.ctrlQQuote, gEditLinesIndex-(curpos.y-gEditTop),
				               displayEditLines);
				break;
			*/
			case GRAPHICS_CHAR_KEY:
				var graphicChar = promptForGraphicsChar(curpos);
				fpDisplayBottomHelpLine(console.screen_rows, gUseQuotes, gUserSettings.ctrlQQuote);
				console.gotoxy(curpos);
				if (graphicChar != null && typeof(graphicChar) === "string" && graphicChar.length > 0)
				{
					var retObject = doPrintableChar(graphicChar, curpos, currentWordLength);
					curpos.x = retObject.x;
					curpos.y = retObject.y;
					currentWordLength = retObject.currentWordLength;
				}
				break;
			case QUOTE_KEY:
				// Let the user choose & insert quote lines into the message.
				if (gUseQuotes)
				{
					var quoteRetObj = doQuoteSelection(curpos, currentWordLength, QUOTE_KEY);
					curpos.x = quoteRetObj.x;
					curpos.y = quoteRetObj.y;
					currentWordLength = quoteRetObj.currentWordLength;
					// If user input timed out, then abort.
					if (quoteRetObj.timedOut)
					{
						returnCode = 1; // Aborted
						continueOn = false;
						console.crlf();
						console.print("\x01n\x01h\x01r" + EDITOR_PROGRAM_NAME + ": Input timeout reached.");
						continue;
					}
				}
				break;
			case CHANGE_COLOR_KEY:
				// Let the user change the text color, if the current line is not a quote line.
				if (gConfigSettings.allowColorSelection/* && !isQuoteLine(gEditLines, gEditLinesIndex)*/)
				{
					if (!isQuoteLine(gEditLines, gEditLinesIndex))
					{
						var chgColorRetobj = doColorSelection(gTextAttrs, curpos, currentWordLength);
						if (!chgColorRetobj.timedOut)
						{
							// Note: DoColorSelection() will prefix the color with the normal
							// attribute.
							gTextAttrs = chgColorRetobj.txtAttrs;
							gEditLines[gEditLinesIndex].attrs[gTextLineIndex] = chgColorRetobj.txtAttrs;
							console.print(gTextAttrs);
							curpos.x = chgColorRetobj.x;
							curpos.y = chgColorRetobj.y;
							currentWordLength = chgColorRetobj.currentWordLength;
							// For attribute code right-shifting, start at index+1 if in the middle of the line
							gTextInsertColorShiftIndexPlusOne = (gTextLineIndex > 0);
							// Refresh the text lines on the screen so that the colors on the screen are up to date, in appropriate conditions
							if (gEditLinesIndex < gEditLines.length-1 || gTextLineIndex < gEditLines[gEditLinesIndex].screenLength()-1)
							{
								displayEditLines(curpos.y, gEditLinesIndex, gEditBottom, false, true);
								console.gotoxy(curpos.x, curpos.y);
							}
						}
						else
						{
							// User input timed out, so abort.
							chgColorRetobj.returnCode = 1; // Aborted
							continueOn = false;
							console.crlf();
							console.print("\x01n\x01h\x01r" + EDITOR_PROGRAM_NAME + ": Input timeout reached.");
							continue;
						}
					}
					else
					{
						writeWithPause(1, console.screen_rows, "\x01n\x01y\x01hCan't change quote line colors\x01n", ERRORMSG_PAUSE_MS);
						// Refresh the help line on the bottom of the screen
						fpDisplayBottomHelpLine(console.screen_rows, gUseQuotes, gUserSettings.ctrlQQuote);
						console.gotoxy(curpos.x, curpos.y);
						console.print(gTextAttrs);
					}
				}
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
					if (gTextLineIndex > gEditLines[gEditLinesIndex].screenLength())
					{
						gTextLineIndex = gEditLines[gEditLinesIndex].screenLength();
						curpos.x = gEditLeft + gEditLines[gEditLinesIndex].screenLength();
					}
					// Figure out the vertical coordinate of where the
					// cursor should be.
					// If the cursor is at the top of the edit area,
					// then scroll up through the message by 1 line.
					if (curpos.y == gEditTop)
						displayEditLines(gEditTop, gEditLinesIndex, gEditBottom, true, /*true*/false);
					else
						--curpos.y;

					// For attribute code right-shifting, start at index+1 if in the middle of the line or index at the beginning
					gTextInsertColorShiftIndexPlusOne = (gTextLineIndex > 0);

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
					if (gTextLineIndex > gEditLines[gEditLinesIndex].screenLength())
					{
						gTextLineIndex = gEditLines[gEditLinesIndex].screenLength();
						curpos.x = gEditLeft + gEditLines[gEditLinesIndex].screenLength();
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

					// For attribute code right-shifting, start at index+1 if in the middle of the line or index at the beginning
					gTextInsertColorShiftIndexPlusOne = (gTextLineIndex > 0);

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
						curpos.x = gEditLeft + gEditLines[gEditLinesIndex].screenLength();
						// Move the cursor up or scroll up by one line
						if (curpos.y > 1)
							--curpos.y;
						else
							displayEditLines(gEditTop, gEditLinesIndex, gEditBottom, true, /*true*/false);
						gTextLineIndex = gEditLines[gEditLinesIndex].screenLength();
						console.gotoxy(curpos);
					}
				}

				// For attribute code right-shifting, start at index+1 if in the middle of the line or index at the beginning
				gTextInsertColorShiftIndexPlusOne = (gTextLineIndex > 0);

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
					if (gTextLineIndex < gEditLines[gEditLinesIndex].screenLength())
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
							displayEditLines(gEditTop, gEditLinesIndex-(gEditBottom-gEditTop), gEditBottom, true, false);
						gTextLineIndex = 0;
						console.gotoxy(curpos);
					}
				}

				// For attribute code right-shifting, start at index+1 if in the middle of the line or index at the beginning
				gTextInsertColorShiftIndexPlusOne = (gTextLineIndex > 0);

				// Update the current word length.
				currentWordLength = getWordLength(gEditLinesIndex, gTextLineIndex);
				// Make sure the edit color is correct
				console.print(chooseEditColor());
				break;
			case KEY_HOME:
				if (gTextLineIndex > 0)
				{
					// Go to the beginning of the line
					gTextLineIndex = 0;
					// For attribute code right-shifting, start at index (not index+1)
					gTextInsertColorShiftIndexPlusOne = false;
					curpos.x = gEditLeft;
					console.gotoxy(curpos);
					// Update the displayed text color.
					gTextAttrs = chooseEditColor();
					console.print(gTextAttrs);
					// Update the current word length.
					currentWordLength = getWordLength(gEditLinesIndex, gTextLineIndex);
				}
				break;
			case KEY_END:
				// Go to the end of the line
				if (gEditLinesIndex < gEditLines.length)
				{
					gTextLineIndex = gEditLines[gEditLinesIndex].screenLength();
					// For attribute code right-shifting, start at index+1
					gTextInsertColorShiftIndexPlusOne = true;
					curpos.x = gEditLeft + gTextLineIndex;
					// If the cursor position would be to the right of the edit
					// area, then place it at gEditRight.
					if (curpos.x > gEditRight)
					{
						var difference = curpos.x - gEditRight;
						curpos.x -= difference;
						gTextLineIndex -= difference;
					}
					// Place the cursor where it should be, and update the displayed text color.
					console.gotoxy(curpos);
					gTextAttrs = chooseEditColor();
					console.print(gTextAttrs);

					// Update the current word length.
					currentWordLength = getWordLength(gEditLinesIndex, gTextLineIndex);
				}
				break;
			case BACKSPACE:
				// Delete the previous character
				if (textLineIsEditable(gEditLinesIndex))
				{
					var backspRetObj = doBackspace(curpos, currentWordLength);
					// For attribute code right-shifting, start at index+1 if in the middle of the line or index at the beginning
					gTextInsertColorShiftIndexPlusOne = (gTextLineIndex > 0);
					curpos.x = backspRetObj.x;
					curpos.y = backspRetObj.y;
					currentWordLength = backspRetObj.currentWordLength;
					// Make sure the edit color is correct
					console.print(chooseEditColor());
				}
				break;
			case KEY_DEL:
				// Delete the next character
				if (textLineIsEditable(gEditLinesIndex))
				{
					var delRetObj = doDeleteKey(curpos, currentWordLength);
					// For attribute code right-shifting, start at index+1 if in the middle of the line or index at the beginning
					gTextInsertColorShiftIndexPlusOne = (gTextLineIndex > 0);
					curpos.x = delRetObj.x;
					curpos.y = delRetObj.y;
					currentWordLength = delRetObj.currentWordLength;
					// Make sure the edit color is correct
					console.print(chooseEditColor());
				}
				break;
			case KEY_ENTER:
				var cursorAtBeginningOrEnd = ((curpos.x == 1) || (curpos.x >= gEditLines[gEditLinesIndex].screenLength()));
				var letUserEditLine = (cursorAtBeginningOrEnd ? true : textLineIsEditable(gEditLinesIndex));
				if (letUserEditLine)
				{
					var enterRetObj = doEnterKey(curpos, currentWordLength);
					curpos.x = enterRetObj.x;
					curpos.y = enterRetObj.y;
					currentWordLength = enterRetObj.currentWordLength;
					returnCode = enterRetObj.returnCode;
					continueOn = enterRetObj.continueOn;
					// Do what the next action should be after the enter key was pressed
					if (continueOn)
					{
						switch (enterRetObj.nextAction)
						{
							case ENTER_ACTION_DO_QUOTE_SELECTION:
								if (gUseQuotes)
								{
									enterRetObj = doQuoteSelection(curpos, currentWordLength, QUOTE_KEY);
									curpos.x = enterRetObj.x;
									curpos.y = enterRetObj.y;
									currentWordLength = enterRetObj.currentWordLength;
									// If user input timed out, then abort.
									if (enterRetObj.timedOut)
									{
										returnCode = 1; // Aborted
										continueOn = false;
										console.crlf();
										console.print("\x01n\x01h\x01r" + EDITOR_PROGRAM_NAME + ": Input timeout reached.");
										continue;
									}
								}
								break;
							case ENTER_ACTION_DO_CROSS_POST_SELECTION:
								if (gCanCrossPost)
									doCrossPosting();
								break;
							case ENTER_ACTION_DO_MEME_INPUT:
								// Input a meme from the user
								var memeInputRetObj = doMemeInput();
								// If a meme was added and we're able, move the
								// cursor below the meme that was addded
								if (memeInputRetObj.numMemeLines > 0)
								{
									var newY = curpos.y + memeInputRetObj.numMemeLines;
									if (newY <= gEditBottom)
										curpos.y = newY;
									else
										curpos.y = gEditBottom;
								}
								if (memeInputRetObj.refreshScreen)
								{
									// Refresh the screen
									clearEditAreaBuffer();
									fpRedrawScreen(gEditLeft, gEditRight, gEditTop, gEditBottom, gTextAttrs,
									               gInsertMode, gUseQuotes, gUserSettings.ctrlQQuote,
									               gEditLinesIndex-(curpos.y-gEditTop), displayEditLines);
								}
								console.gotoxy(curpos);
								break;
							case ENTER_ACTION_SHOW_HELP:
								displayProgramInfo(true, false);
								displayCommandList(false, false, true, gCanCrossPost,
								                   gConfigSettings.enableTextReplacements, gConfigSettings.allowUserSettings,
								                   gConfigSettings.allowSpellCheck, gConfigSettings.allowColorSelection, gCanChangeSubject);
								clearEditAreaBuffer();
								fpRedrawScreen(gEditLeft, gEditRight, gEditTop, gEditBottom, gTextAttrs,
								               gInsertMode, gUseQuotes, gUserSettings.ctrlQQuote,
								               gEditLinesIndex-(curpos.y-gEditTop), displayEditLines);
								console.gotoxy(curpos);
								break;
							default:
								break;
						}
					}
					// For attribute code right-shifting, start at index+1 if in the middle of the line or index at the beginning
					gTextInsertColorShiftIndexPlusOne = (gTextLineIndex > 0);
					// Make sure the edit color is correct
					console.print(chooseEditColor());
				}
				break;
				// Insert/overwrite mode toggle
			case KEY_INSERT:
			case TOGGLE_INSERT_KEY:
				toggleInsertMode(null);
				gTextAttrs = chooseEditColor();
				console.print(gTextAttrs);
				console.gotoxy(curpos);
				break;
			case KEY_ESC:
				// Do the ESC menu
				var escRetObj = doESCMenu(curpos, currentWordLength);
				returnCode = escRetObj.returnCode;
				continueOn = escRetObj.continueOn;
				curpos.x = escRetObj.x;
				curpos.y = escRetObj.y;
				currentWordLength = escRetObj.currentWordLength;
				// If we can continue on, put the cursor back
				// where it should be.
				if (continueOn)
				{
					console.print(chooseEditColor()); // Make sure the edit color is correct
					console.gotoxy(curpos);
				}
				break;
			case SEARCH_TEXT_KEY:
				var searchRetObj = findText(curpos);
				curpos.x = searchRetObj.x;
				curpos.y = searchRetObj.y;
				console.print(chooseEditColor()); // Make sure the edit color is correct
				break;
			case SPELL_CHECK_KEY:
				if (gConfigSettings.allowSpellCheck)
				{
					var spellCheckRetObj = doSpellCheck(curpos, true);
					curpos.x = spellCheckRetObj.x;
					curpos.y = spellCheckRetObj.y;
					currentWordLength = spellCheckRetObj.currentWordLength;
					console.print(chooseEditColor()); // Make sure the edit color is correct
				}
				break;
			case IMPORT_FILE_KEY:
				// Only let sysops import files.
				if (user.is_sysop)
				{
					var importRetObj = importFile(curpos);
					curpos.x = importRetObj.x;
					curpos.y = importRetObj.y;
					currentWordLength = importRetObj.currentWordLength;
					continueOn = !importRetObj.sendImmediately;
					if (continueOn)
						console.print(chooseEditColor()); // Make sure the edit color is correct
				}
				break;
			case EXPORT_TO_FILE_KEY:
				// Only let sysops export/save the message to a file.
				if (user.is_sysop)
				{
					exportToFile();
					console.print(chooseEditColor()); // Make sure the edit color is correct
					console.gotoxy(curpos);
				}
				break;
			case DELETE_LINE_KEY:
				var delRetObj = doDeleteLine(curpos);
				// For attribute code right-shifting, start at index+1 if in the middle of the line or index at the beginning
				gTextInsertColorShiftIndexPlusOne = (gTextLineIndex > 0);
				curpos.x = delRetObj.x;
				curpos.y = delRetObj.y;
				currentWordLength = delRetObj.currentWordLength;
				// Make sure the edit color is correct
				gTextAttrs = chooseEditColor();
				console.print(gTextAttrs);
				break;
			case KEY_PAGEUP: // Move 1 page up in the message
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
					gTextLineIndex = gEditLines[gEditLinesIndex].screenLength();
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
				// For attribute code right-shifting, start at index+1 if in the middle of the line or index at the beginning
				gTextInsertColorShiftIndexPlusOne = (gTextLineIndex > 0);
				// Make sure the edit color is correct
				gTextAttrs = chooseEditColor();
				console.print(gTextAttrs);
				break;
			case KEY_PAGEDN: // Move 1 page down in the message
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
						gTextLineIndex = gEditLines[gEditLinesIndex].screenLength();
						if ((gTextLineIndex > 0) && (gEditLines[gEditLinesIndex].length == gEditWidth))
							--gTextLineIndex;
						curpos.x = gEditLeft + gTextLineIndex;
						curpos.y += (gEditLinesIndex-oldEditLinesIndex);
						console.gotoxy(curpos);

						// Update the current word length.
						currentWordLength = getWordLength(gEditLinesIndex, gTextLineIndex);
					}
				}
				// For attribute code right-shifting, start at index+1 if in the middle of the line or index at the beginning
				gTextInsertColorShiftIndexPlusOne = (gTextLineIndex > 0);
				// Make sure the edit color is correct
				gTextAttrs = chooseEditColor();
				console.print(gTextAttrs);
				break;
			case CROSSPOST_KEY:
				if (gCanCrossPost)
					doCrossPosting();
				break;
			case LIST_TXT_REPLACEMENTS_KEY:
				if (gConfigSettings.enableTextReplacements)
					listTextReplacements();
				break;
			case USER_SETTINGS_KEY:
				var userSettingsRetObj = doUserSettings(curpos, true);
				// If the user changed their option for using Ctrl-Q as the quote hotkey,
				// then change it
				if (userSettingsRetObj.ctrlQQuoteOptChanged)
				{
					QUOTE_KEY = gUserSettings.ctrlQQuote ? CTRL_Q : CTRL_Y;
					// If we have quote lines and the UI style is DCT, then refresh the key
					// help line at the bottom of the screen, which includes the quote hotkey
					// in this case
					if (gUseQuotes && EDITOR_STYLE == "DCT")
					{
						fpDisplayBottomHelpLine(console.screen_rows, gUseQuotes, gUserSettings.ctrlQQuote);
						console.gotoxy(curpos);
					}
				}
				break;
			case CHANGE_SUBJECT_KEY:
				if (gCanChangeSubject)
				{
					console.attributes = "N";
					console.gotoxy(gSubjPos.x, gSubjPos.y);
					var subj = console.getstr(gSubjScreenLen, K_LINE|K_NOCRLF|K_NOSPIN|K_TRIM);
					if (subj.length > 0)
						gMsgSubj = subj;
					// Refresh the subject line on the screen with the proper colors etc.
					fpRefreshSubjectOnScreen(gSubjPos.x, gSubjPos.y, gSubjScreenLen, gMsgSubj);

					// Restore the edit color and cursor position
					console.print(chooseEditColor());
					console.gotoxy(curpos);
				}
				break;
			default:
				// For the tab character, insert 3 spaces.  Otherwise,
				// if it's a printable character, add the character.
				if (textLineIsEditable(gEditLinesIndex))
				{
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
				}
				break;
		}

		// Update the time strings on the screen
		updateTimeLeft = (initialTimeLeft - bbs.time_left >= 60);
		// For every 5 keys pressed, check the current time and update
		// it on the screen if necessary.  updateTime() is also being
		// called when the first key is pressed so that the function's
		// time string variable gets initially set.
		// Note: The 2nd parameter to updateTime() is whether or not to move the
		// cursor back to the original location after updating the time on the
		// screen.  For optimization, we don't want to do that if we'll also be
		// updating the time left on the screen.
		if ((numKeysPressed == 1) || (numKeysPressed % 5 == 0))
			updateTime(curpos, !updateTimeLeft);
		// If the user's time left has gone down by at least 60 seconds, then
		// update the time & user's time left on the screen.
		if (updateTimeLeft)
		{
			fpDisplayTimeRemaining();
			// Change back to the edit color and place the cursor back
			// where it needs to be.
			console.print(chooseEditColor());
			console.gotoxy(curpos); // Place the cursor back where it needs to be

			initialTimeLeft = bbs.time_left;
		}
	}


	// If the user has not aborted the message, then if spell check is allowed,
	// prompt for spell check and adding a tagline if those options are enabled
	// in the user settings
	if (returnCode == 0)
	{
		if (gConfigSettings.allowSpellCheck && gUserSettings.promptSpellCheckOnSave)
			doSpellCheck(curpos, true);

		// If taglines is enabled in the user settings and the user is not editing their signature & is not editing an existing message, then
		// prompt the user for a tag line to be appended to the message.
		var isEditingSignature = (gMsgSubj == format("%04d.sig", user.number));
		var isEditingExistingMsg = !isEditingSignature && ((gMsgSubj == "MSGTMP") || (gMsgSubj == "DDMsgReader_message.txt") || (gMsgSubj == "DDMsgLister_message.txt"));
		if (gUserSettings.enableTaglines && !isEditingSignature && !isEditingExistingMsg && txtFileContainsLines(gConfigSettings.tagLineFilename))
		{
			if (promptYesNo("Add a tagline", true, "Add tagline", true, true))
			{
				var taglineRetObj = doTaglineSelection();
				if (taglineRetObj.taglineWasSelected && taglineRetObj.tagline.length > 0)
				{
					// If the tagline filename was specified in the MSGINF drop file,
					// then write the tag line to that file (Synchronet will read that
					// and append its contents after the user's signature).  Otherwise,
					// append the tagline to the message directly.
					if (gTaglineFilename.length > 0)
						writeTaglineToMsgTaglineFile(taglineRetObj.tagline, gTaglineFilename);
					else
					{
						// Append a blank line and then append the tagline to the message
						gEditLines.push(new TextLine());
						var newLine = new TextLine(taglineRetObj.tagline);
						gEditLines.push(newLine);
						reAdjustTextLines(gEditLines, gEditLines.length-1, gEditLines.length, gEditWidth, gConfigSettings.allowColorSelection, gJustEditingAFile); // gMsgAreaInfo.subBoardCode.length == 0
					}
				}
			}
		}
	}

	// If gEditLines has only 1 line in it and it's blank, then
	// remove it so that we can test to see if the message is empty.
	if (gEditLines.length == 1)
	{
		if (gEditLines[0].screenLength() == 0)
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
	var retObj = {
		x: pCurpos.x,
		y: pCurpos.y,
		currentWordLength: pCurrentWordLength
	};

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
			/*
			var printMode = gUserConsoleSupportsUTF8 ? P_UTF8 : P_NONE;
			console.print(BACKSPACE, printMode);
			console.print(" ", printMode);
			//console.print(BACKSPACE);
			//console.print(" ");
			*/

			// Remove the previous character from the text line
			var textLineLength = gEditLines[gEditLinesIndex].screenLength();
			if (textLineLength > 0)
			{
				var printMode = gUserConsoleSupportsUTF8 ? P_UTF8 : P_NONE;
				console.print(BACKSPACE, printMode);
				console.print(" ", printMode);
				//console.print(BACKSPACE);
				//console.print(" ");

				// TODO: After backspacing over a UTF-8 character in the middle of a text line, we can't backspace anymore

				gEditLines[gEditLinesIndex].removeCharAt(--gTextLineIndex);
				didBackspace = true;
				//--gTextLineIndex;
			}

			--retObj.x;
			console.gotoxy(retObj.x, retObj.y);
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
			if (gEditLines[gEditLinesIndex].screenLength() > 0)
			{
				// Store the previous line's original length
				var originalPrevLineLen = gEditLines[prevLineIndex].screenLength();

				// See how much space is at the end of the previous line
				var previousLineEndSpace = gEditWidth - gEditLines[prevLineIndex].screenLength();
				if (previousLineEndSpace > 0)
				{
					var index = previousLineEndSpace - 1;
					// If that index is valid for the current line, then find the first
					// space in the current line so that the text would fit at the end
					// of the previous line.  Otherwise, set index to the length of the
					// current line so that we'll move the whole current line up to the
					// previous line.
					if (index < gEditLines[gEditLinesIndex].screenLength())
					{
						for (; index >= 0; --index)
						{
							if (gEditLines[gEditLinesIndex].text.charAt(index) == " ")
								break;
						}
					}
					else
						index = gEditLines[gEditLinesIndex].screenLength();
					// If we found a space, then move the part of the current line before
					// the space to the end of the previous line.
					if (index > 0)
					{
						var linePart = gEditLines[gEditLinesIndex].text.substr(0, index);
						gEditLines[gEditLinesIndex].text = gEditLines[gEditLinesIndex].text.substr(index);
						var prevLineOriginalLen = gEditLines[prevLineIndex].text.length;
						gEditLines[prevLineIndex].text += linePart;
						gEditLines[prevLineIndex].hardNewlineEnd = gEditLines[gEditLinesIndex].hardNewlineEnd;
						// Also move any attribute codes to the previous line
						var frontAttrs = gEditLines[prevLineIndex].popAttrsFromFront(index);
						for (var attrTextIdx in frontAttrs)
						{
							var prevLineAttrIdx = prevLineOriginalLen + (+attrTextIdx);
							gEditLines[prevLineIndex].attrs[prevLineAttrIdx] = gEditLines[gEditLinesIndex].attrs[textIdx];
						}

						// If the current line is now blank, then remove it from gEditLines.
						if (gEditLines[gEditLinesIndex].screenLength() == 0)
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
							gTextLineIndex = gEditLines[gEditLinesIndex].screenLength();

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
					// Copy the current line's "hard newline end" setting to the
					// previous line (so that if there's a blank line below the
					// current line, the blank line will be preserved), then remove
					// the current edit line.
					gEditLines[gEditLinesIndex-1].hardNewlineEnd = gEditLines[gEditLinesIndex].hardNewlineEnd;
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
		reAdjustTextLines(gEditLines, gEditLinesIndex, gEditLines.length, gEditWidth, gConfigSettings.allowColorSelection, gJustEditingAFile); // gMsgAreaInfo.subBoardCode.length == 0

		// If the previous line's length increased, that probably means that the
		// user backspaced to the beginning of the current line and the word was
		// moved to the end of the previous line.  If so, then move the cursor to
		// the end of the previous line.
		//var scrolled = false;
		if ((gEditLinesIndex > 0) && (gEditLines[gEditLinesIndex-1].length() > prevTextline.length))
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
	var retObj = {
		x: pCurpos.x,
		y: pCurpos.y,
		currentWordLength: pCurrentWordLength
	};

	// If gEditLinesIndex is invalid, then return without doing anything.
	if ((gEditLinesIndex < 0) || (gEditLinesIndex >= gEditLines.length))
		return retObj;

	// Store the original line text (for testing to see if we should update the screen).
	var originalLineText = gEditLines[gEditLinesIndex].getText(true);

	// If the text line index is within bounds, then we can
	// delete the next character and refresh the screen.
	if (gTextLineIndex < gEditLines[gEditLinesIndex].length())
	{
		gEditLines[gEditLinesIndex].removeCharAt(gTextLineIndex);
		// If the current character is a space, then reset the current word length.
		// to 0.  Otherwise, set it to the current word length.
		if (gTextLineIndex < gEditLines[gEditLinesIndex].screenLength())
		{
			if (gEditLines[gEditLinesIndex].text.charAt(gTextLineIndex) == " ")
				retObj.currentWordLength = 0;
			else
			{
				var spacePos = gEditLines[gEditLinesIndex].text.indexOf(" ", gTextLineIndex);
				if (spacePos > -1)
					retObj.currentWordLength = spacePos - gTextLineIndex;
				else
					retObj.currentWordLength = getWordLength(gEditLinesIndex, gTextLineIndex);
			}
		}

		// Re-adjust the line lengths and refresh the edit area.
		var reAdjustRetObj = reAdjustTextLines(gEditLines, gEditLinesIndex, gEditLines.length, gEditWidth, gConfigSettings.allowColorSelection, gJustEditingAFile); // gMsgAreaInfo.subBoardCode.length == 0
		var textChanged = reAdjustRetObj.textChanged;

		// If the line text changed, then update the message area from the
		// current line on down.
		textChanged = textChanged || (gEditLines[gEditLinesIndex].text != originalLineText);
		if (textChanged)
		{
			// Calculate the bottommost edit area row to update, and then
			// refresh the edit area.
			var bottommostRow = calcBottomUpdateRow(retObj.y, gEditLinesIndex);
			displayEditLines(retObj.y, gEditLinesIndex, bottommostRow, true, true);
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

		// If the current line is blank and is not the last line, then remove it.
		if (gEditLines[gEditLinesIndex].screenLength() == 0)
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
			if (gEditLines[nextLineIndex].screenLength() == 0)
				gEditLines[nextLineIndex].hardNewlineEnd = false;
		}

		// Re-adjust the text lines, update textChanged & set a few other things
		var reAdjustRetObj = reAdjustTextLines(gEditLines, gEditLinesIndex, gEditLines.length, gEditWidth, gConfigSettings.allowColorSelection, gJustEditingAFile); // gMsgAreaInfo.subBoardCode.length == 0
		textChanged = textChanged || reAdjustRetObj.textChanged;
		retObj.currentWordLength = getWordLength(gEditLinesIndex, gTextLineIndex);
		var startRow = retObj.y;
		var startEditLinesIndex = gEditLinesIndex;
		if (retObj.y > gEditTop)
		{
			--startRow;
			--startEditLinesIndex;
		}

		// If text changed, then refresh the edit area.
		textChanged = textChanged || (gEditLines[gEditLinesIndex].getText(true) != originalLineText);
		if (textChanged)
		{
			// Calculate the bottommost edit area row to update, and then
			// refresh the edit area.
			var bottommostRow = calcBottomUpdateRow(startRow, startEditLinesIndex);
			displayEditLines(startRow, startEditLinesIndex, bottommostRow, true, true);
		}
	}

	// Move the cursor where it should be.
	console.gotoxy(retObj.x, retObj.y);

	return retObj;
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
	var retObj = {
		x: pCurpos.x,
		y: pCurpos.y,
		currentWordLength: pCurrentWordLength
	};

	// Note: gTextLineIndex is where the new character will appear in the line.
	// If gTextLineIndex is somehow past the end of the current line, then
	// fill it with spaces up to gTextLineIndex.
	var idxIsAtEndOfTextLine = false;
	if (gTextLineIndex > gEditLines[gEditLinesIndex].screenLength())
	{
		var numSpaces = gTextLineIndex - gEditLines[gEditLinesIndex].screenLength();
		if (numSpaces > 0)
			gEditLines[gEditLinesIndex].text += format("%" + numSpaces + "s", "");
		gEditLines[gEditLinesIndex].text += pUserInput;
	}
	// If gTextLineIndex is at the end of the line, then just append the char.
	else if (gTextLineIndex == gEditLines[gEditLinesIndex].screenLength())
	{
		gEditLines[gEditLinesIndex].text += pUserInput;
		idxIsAtEndOfTextLine = true;
	}
	else
	{
		// gTextLineIndex is at the beginning or in the middle of the line.
		if (inInsertMode())
			gEditLines[gEditLinesIndex].insertIntoText(gTextLineIndex, pUserInput, gTextInsertColorShiftIndexPlusOne);
		else
		{
			// Replace the character at gTextlineIndex
			gEditLines[gEditLinesIndex].text = gEditLines[gEditLinesIndex].text.substr(0, gTextLineIndex)
			                                 + pUserInput + gEditLines[gEditLinesIndex].text.substr(gTextLineIndex+1);
		}
	}

	// Handle text replacement (AKA macros).
	var madeTxtReplacement = false; // For screen refresh purposes
	var afterTxtReplacementAttrs = ""; // To ensure the right color is in use
	if (gConfigSettings.enableTextReplacements && (pUserInput == " "))
	{
		var priorTxtLineAttrs = getAllEditLineAttrsUntilLineIdx(gEditLinesIndex);
		var txtReplaceObj = gEditLines[gEditLinesIndex].doMacroTxtReplacement(gTxtReplacements, gTextLineIndex,
		                                                                      gConfigSettings.textReplacementsUseRegex, priorTxtLineAttrs);
		madeTxtReplacement = txtReplaceObj.madeTxtReplacement;
		if (madeTxtReplacement)
		{
			retObj.x += txtReplaceObj.wordLenDiff;
			gTextLineIndex += txtReplaceObj.wordLenDiff;
			afterTxtReplacementAttrs = txtReplaceObj.priorTxtAttrs;
		}
	}

	// Store a copy of the current line so that we can compare it later to see
	// if it was modified by reAdjustTextLines().
	var originalAfterCharApplied = gEditLines[gEditLinesIndex].text;

	// If the line is now too long to fit in the edit area, then we will have
	// to re-adjust the text lines.
	var reAdjusted = false;
	var addedSpaceAtSplitPointDuringReAdjust = false;
	if (gEditLines[gEditLinesIndex].screenLength() >= gEditWidth)
	{
		var reAdjustRetObj = reAdjustTextLines(gEditLines, gEditLinesIndex, gEditLines.length, gEditWidth, gConfigSettings.allowColorSelection, gJustEditingAFile); // gMsgAreaInfo.subBoardCode.length == 0
		reAdjusted = reAdjustRetObj.textChanged;
		addedSpaceAtSplitPointDuringReAdjust = reAdjustRetObj.addedSpaceAtSplitPoint;
	}

	// placeCursorAtEnd specifies whether or not to place the cursor at its
	// spot using console.gotoxy() at the end.  This is an optimization.
	var placeCursorAtEnd = true;

	// If the current text line is now different (modified by reAdjustTextLines())
	// or text replacements were made, then we'll need to refresh multiple lines
	// on the screen.
	if ((reAdjusted && (gEditLines[gEditLinesIndex].text != originalAfterCharApplied)) || madeTxtReplacement)
	{
		// If gTextLineIndex is >= gEditLines[gEditLinesIndex].length(), then
		// we know the current word was wrapped to the next line.  Figure out what
		// retObj.x, retObj.currentWordLength, gEditLinesIndex, and gTextLineIndex
		// should be, and increment retObj.y.  Also figure out what lines on the
		// screen to update, and deal with scrolling if necessary.
		if (gTextLineIndex >= gEditLines[gEditLinesIndex].screenLength())
		{
			// I changed this on 2010-02-14 to (hopefully) place the cursor where
			// it should be
			// Old line (prior to 2010-02-14):
			//var numChars = gTextLineIndex - gEditLines[gEditLinesIndex].length();
			// New (2010-02-14):
			var numChars = 0;
			// Special case: If the current line's length is exactly the longest
			// edit with, then the # of chars should be 0 or 1, depending on whether the
			// entered character was a space or not.  Otherwise, calculate numChars
			// normally.
			if (gEditLines[gEditLinesIndex].screenLength() == gEditWidth-1)
				numChars = ((pUserInput == " ") ? 0 : 1);
			else
			{
				numChars = gTextLineIndex - gEditLines[gEditLinesIndex].screenLength();
				// New (2025-05-04) - If editing a regular file (not a message)
				// and a space was added where the line was split, increment
				// numChars
				if (gJustEditingAFile && addedSpaceAtSplitPointDuringReAdjust) // gMsgAreaInfo.subBoardCode.length == 0
					++numChars;
			}
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
		if (gTextLineIndex < gEditLines[gEditLinesIndex].screenLength()-1)
			displayEditLines(retObj.y, gEditLinesIndex, retObj.y, false, true);
		else
		{
			printStrConsideringUTF8(pUserInput, gPrintMode);
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

	// Ensure the attributes for displaying text is what they should be
	if (madeTxtReplacement)
	{
		gTextAttrs = afterTxtReplacementAttrs;
		if (gTextAttrs.length == 0)
			gTextAttrs = "\x01n";
		console.print(gTextAttrs);
	}
	else
		console.print(chooseEditColor());

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
//               nextAction: Will have one of the ENTER_ACTION_* values to specify
//                           what action to take based on the user's input on the
//                           current line. Defaults to ENTER_ACTION_NONE, for no
//                           special action.
function doEnterKey(pCurpos, pCurrentWordLength)
{
	// Create the return object
	var retObj = {
		x: pCurpos.x,
		y: pCurpos.y,
		currentWordLength: pCurrentWordLength,
		returnCode: 0,
		continueOn: true,
		nextAction: ENTER_ACTION_NONE
	};

	// Store the current screen row position and gEditLines index.
	var initialScreenLine = pCurpos.y;
	var initialEditLinesIndex = gEditLinesIndex;

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
			var editObjName = (gMsgAreaInfo.subBoardCode.length > 0 ? "message" : "edit");
			if (promptYesNo("Abort " + editObjName, false, "Abort", false, false))
			{
				retObj.returnCode = 1; // 1: Abort
				retObj.continueOn = false;
				return(retObj);
			}
			else
			{
				// Make sure the current text color attribute is set back.
				console.print(chooseEditColor());

				// Blank out the data in the text line, set the data in
				// retObj, and return it.
				gEditLines[gEditLinesIndex].text = "";
				retObj.currentWordLength = 0;
				gTextLineIndex = 0;
				retObj.x = gEditLeft;
				retObj.y = pCurpos.y;
				// Blank out the /A on the screen
				console.print(chooseEditColor());
				console.gotoxy(retObj.x, retObj.y);
				console.print("  ");
				// Put the cursor where it should be
				console.gotoxy(retObj.x, retObj.y);
				return(retObj);
			}
		}
		// /Q: Do quote selection or /?: Show help
		else if ((lineUpper == "/Q") || (lineUpper == "/?"))
		{
			if (lineUpper == "/Q")
				retObj.nextAction = ENTER_ACTION_DO_QUOTE_SELECTION;
			else if (lineUpper == "/?")
				retObj.nextAction = ENTER_ACTION_SHOW_HELP;
			retObj.currentWordLength = 0;
			gTextLineIndex = 0;
			gEditLines[gEditLinesIndex].text = "";
			// Blank out the /? on the screen
			console.print(chooseEditColor());
			retObj.x = gEditLeft;
			console.gotoxy(retObj.x, retObj.y);
			console.print("  ");
			// Put the cursor where it should be
			console.gotoxy(retObj.x, retObj.y);
			return(retObj);
		}
		// /M: Input & insert a meme
		else if (lineUpper == "/M")
		{
			retObj.nextAction = ENTER_ACTION_DO_MEME_INPUT;
			retObj.currentWordLength = 0;
			gTextLineIndex = 0;
			gEditLines[gEditLinesIndex].text = "";
			// Blank out the /M on the screen
			console.print(chooseEditColor());
			retObj.x = gEditLeft;
			console.gotoxy(retObj.x, retObj.y);
			console.print("  ");
			// Put the cursor where it should be
			console.gotoxy(retObj.x, retObj.y);
			return(retObj);
		}
		// /C: Cross-post
		else if (lineUpper == "/C")
		{
			retObj.nextAction = ENTER_ACTION_DO_CROSS_POST_SELECTION;

			// Blank out the data in the text line, set the data in
			// retObj, and return it.
			gEditLines[gEditLinesIndex].text = "";
			retObj.currentWordLength = 0;
			gTextLineIndex = 0;
			retObj.x = gEditLeft;
			retObj.y = pCurpos.y;
			// Blank out the /C on the screen
			console.print(chooseEditColor());
			retObj.x = gEditLeft;
			console.gotoxy(retObj.x, retObj.y);
			console.print("  ");
			// Put the cursor where it should be
			console.gotoxy(retObj.x, retObj.y);
			return(retObj);
		}
		// /T: List text replacements (do that here)
		else if (lineUpper == "/T")
		{
			if (gConfigSettings.enableTextReplacements)
				listTextReplacements();
			// Blank out the data in the text line, set the data in
			// retObj, and return it.
			gEditLines[gEditLinesIndex].text = "";
			retObj.currentWordLength = 0;
			gTextLineIndex = 0;
			retObj.x = gEditLeft;
			retObj.y = pCurpos.y;
			// Blank out the /T on the screen
			console.print(chooseEditColor());
			retObj.x = gEditLeft;
			console.gotoxy(retObj.x, retObj.y);
			console.print("  ");
			// Put the cursor where it should be
			console.gotoxy(retObj.x, retObj.y);
			return(retObj);
		}
		// /U: User settings (do that here)
		else if (lineUpper == "/U")
		{
			var currentCursorPos = {
				x: retObj.x,
				y: retObj.y
			};
			doUserSettings(currentCursorPos, false);
			// Blank out the data in the text line, set the data in
			// retObj, and return it.
			gEditLines[gEditLinesIndex].text = "";
			retObj.currentWordLength = 0;
			gTextLineIndex = 0;
			retObj.x = gEditLeft;
			retObj.y = pCurpos.y;
			// Blank out the /U on the screen
			console.print(chooseEditColor());
			retObj.x = gEditLeft;
			console.gotoxy(retObj.x, retObj.y);
			console.print("  ");
			// Put the cursor where it should be
			console.gotoxy(retObj.x, retObj.y);
			return(retObj);
		}
	}
	// /UL or /UPLOAD: Upload a file containing a message to post
	// instead of any text entered in the editor
	else if (((gEditLines[gEditLinesIndex].length() == 3) && (gEditLines[gEditLinesIndex].text.toUpperCase() == "/UL")) || ((gEditLines[gEditLinesIndex].length() == 7) && (gEditLines[gEditLinesIndex].text.toUpperCase() == "/UPLOAD")))
	{
		if (letUserUploadMessageFile())
		{
			retObj.continueOn = false;
			return(retObj);
		}
		else
		{
			// Blank out the /ul or /upload on the screen
			console.print(chooseEditColor());
			retObj.x = gEditLeft;
			console.gotoxy(retObj.x, retObj.y);
			printf("%" + gEditLines[gEditLinesIndex].length() + "s", "");
			gEditLines[gEditLinesIndex].text = "";
			retObj.currentWordLength = 0;
			gTextLineIndex = 0;
			// Put the cursor where it should be
			console.gotoxy(retObj.x, retObj.y);
			return(retObj);
		}
	}

	// Handle text replacement (AKA macros).
	var reAdjustedTxtLines = false; // For screen refresh purposes
	var madeTxtReplacement = false; // To help ensure we're using the correct edit color
	var afterTxtReplacementAttrs = ""; // To ensure the right color is in use
	// cursorHorizDiff will be set if the replaced word is too long to fit on
	// the end of the line - In that case, the cursor and line index will have
	// to be adjusted since the new word will be moved to the next line.
	var cursorHorizDiff = 0;
	if (gConfigSettings.enableTextReplacements)
	{
		var priorTxtLineAttrs = getAllEditLineAttrsUntilLineIdx(gEditLinesIndex);
		var txtReplaceObj = gEditLines[gEditLinesIndex].doMacroTxtReplacement(gTxtReplacements, gTextLineIndex-1,
		                                                                      gConfigSettings.textReplacementsUseRegex, priorTxtLineAttrs);
		madeTxtReplacement = txtReplaceObj.madeTxtReplacement;
		if (txtReplaceObj.madeTxtReplacement)
		{
			gTextLineIndex += txtReplaceObj.wordLenDiff;
			retObj.x += txtReplaceObj.wordLenDiff;
			retObj.currentWordLength += txtReplaceObj.wordLenDiff;
			afterTxtReplacementAttrs = txtReplaceObj.priorTxtAttrs;

			// If the replaced text on the line is too long to print on the screen,then
			// then we'll need to wrap the line.
			// If the logical screen column of the last character of the last word
			// is beyond the rightmost colum of the edit area, then wrap the line.
			if (gEditLeft + txtReplaceObj.newTextEndIdx - 1 >= gEditRight - 1)
			{
				// If the replaced text contains at least one space, then look for
				// the last space that can appear within the edit area on the screen.
				if (gEditLines[gEditLinesIndex].text.indexOf(" ", txtReplaceObj.wordStartIdx) > -1)
				{
					var spaceIdx = gEditLines[gEditLinesIndex].text.lastIndexOf(" ", txtReplaceObj.textLineIndex);
					while ((spaceIdx > -1) && (spaceIdx > gEditWidth-2)) // To split lines at the 79th column
						spaceIdx = gEditLines[gEditLinesIndex].text.lastIndexOf(" ", spaceIdx-1);
					// If a space was found after the start of the new text, then
					// set gTextLineIndex to the first character of the word we want
					// to split the line at, and the horizontal cursor offset based on
					// the difference.
					if (spaceIdx > txtReplaceObj.wordStartIdx)
					{
						gTextLineIndex = spaceIdx + 1;
						cursorHorizDiff = txtReplaceObj.newTextEndIdx - spaceIdx;
					}
					else
					{
						gTextLineIndex = txtReplaceObj.wordStartIdx;
						cursorHorizDiff = txtReplaceObj.newTextLen;
					}
				}
				else
				{
					// The new text doesn't contain a space, so set gTextLineIndex
					// to the start of the new word so that
					// enterKey_InsertOrAppendNewLine() will split the line there.
					gTextLineIndex = txtReplaceObj.wordStartIdx;
					cursorHorizDiff = txtReplaceObj.newTextLen;
				}
			}
			if (gConfigSettings.allowColorSelection)
			{
				//console.print(getAllEditLineAttrs(gEditLinesIndex, gTextLineIndex));
				console.print(txtReplaceObj.priorTxtAttrs);
			}
		}
	}

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
	// In case text replacements with attribute codes were done, ensure the attributes for displaying text is what they should be
	if (madeTxtReplacement)
	{
		gTextAttrs = afterTxtReplacementAttrs;
		if (gTextAttrs.length == 0)
			gTextAttrs = "\x01n";
		console.print(gTextAttrs);
	}

	// Note: cursorHorizDiff is set if a word was replaced and the line was
	// wrapped because the new word wastoo long to fit on the end of the line.
	retObj.x += cursorHorizDiff;
	gTextLineIndex += cursorHorizDiff;

	console.gotoxy(retObj.x, retObj.y);

	return retObj;
}

// Helper function for doEnterKey(): Appends/inserts a line to gEditLines
// and returns the position of where the cursor should be.
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
	var returnObject = {
		displayedEditlines: false,
		addedATextLine: false,
		addedTextLineBelow: false
	};

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
			displayEditLines(gEditTop, gEditLinesIndex-(gEditBottom-gEditTop), gEditBottom, true, true);
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
		// Remove that text from the current line.  Also get any attribute codes that there might be in
		// the line from that index onward.
		var endAttrs = gEditLines[gEditLinesIndex].trimEnd(gTextLineIndex);

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
			// Set the attribute codes from the end of the current line at the beginning text
			// in the new line
			for (var txtIdx in endAttrs)
			{
				var newTxtIdx = (+txtIdx) - gTextLineIndex;
				newTextLine.attrs[(+txtIdx) - gTextLineIndex] = endAttrs[txtIdx];
			}
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
			// Set the attribute codes from the end of the current line at the beginning text
			// in the new line
			for (var txtIdx in endAttrs)
			{
				var newTxtIdx = (+txtIdx) - gTextLineIndex;
				newTextLine.attrs[(+txtIdx) - gTextLineIndex] = endAttrs[txtIdx];
			}
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
			displayEditLines(gEditTop, gEditLinesIndex-(gEditBottom-gEditTop), gEditBottom, true, true);
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

// Helper function for doEditLoop(): Returns whether a text line is editable
// (for instance, quote lines might not be editable).
//
// Parameters:
//  pLineIdx: The index of the text line
//
// Return value: Boolean - Whether or not the text line is editable
function textLineIsEditable(pLineIdx)
{
	if (typeof(pLineIdx) != "number")
		return true;
	if ((pLineIdx < 0) || (pLineIdx >= gEditLines.length))
		return true;

	// The main concern is whether quote lines are editable.
	var lineIsEditable = true;
	if (!gConfigSettings.allowEditQuoteLines)
		lineIsEditable = !(gEditLines[pLineIdx].isQuoteLine);
	return lineIsEditable;
}

// This function handles quote selection and is called by doEditLoop().
//
// Parameters:
//  pCurpos: An object containing x and y values representing the
//           cursor position.
//  pCurrentWordLength: The length of the current word that has been typed
//  pQuoteKey: The hotkey to use to open/close the quote window (could be Ctrl-Q
//             or Ctrl-Y, depending on the SlyEdit configuration)
//
// Return value: An object containing the following properties:
//               x and y: The horizontal and vertical cursor position
//               timedOut: Whether or not the user input timed out (boolean)
//               currentWordLength: The length of the current word
function doQuoteSelection(pCurpos, pCurrentWordLength, pQuoteKey)
{
	// Create the return object
	var retObj = {
		x: pCurpos.x,
		y: pCurpos.y,
		timedOut: false,
		currentWordLength: pCurrentWordLength
	};

	// Note: Quote lines are in the gQuoteLines array, where each element is
	// a string.

	// If gQuoteLines is empty, then we have nothing to do, so just return.
	if ((gQuoteLines.length == 0) || !gUseQuotes)
		return retObj;

	// The first time this function runs, save the user's settings for using initials
	// in quote lines and whether or not to indent quote lines containing initials.
	// These will be checked against the user's current settings to see if we need
	// to wrap the quote lines again in case the user changed these settings.
	if (typeof(doQuoteSelection.useQuoteLineInitials) == "undefined")
		doQuoteSelection.useQuoteLineInitials = gUserSettings.useQuoteLineInitials;
	if (typeof(doQuoteSelection.indentQuoteLinesWithInitials) == "undefined")
		doQuoteSelection.indentQuoteLinesWithInitials = gUserSettings.indentQuoteLinesWithInitials;
	if (typeof(doQuoteSelection.trimSpacesFromQuoteLines) == "undefined")
		doQuoteSelection.trimSpacesFromQuoteLines = gUserSettings.trimSpacesFromQuoteLines;
	if (typeof(doQuoteSelection.wrapQuoteLines) == "undefined")
		doQuoteSelection.wrapQuoteLines = gUserSettings.wrapQuoteLines;
	if (typeof(doQuoteSelection.joinQuoteLinesWhenWrapping) == "undefined")
		doQuoteSelection.joinQuoteLinesWhenWrapping = gUserSettings.joinQuoteLinesWhenWrapping;
	
	// If the setting to re-wrap quote lines is enabled, then do it.
	// We're re-wrapping the quote lines here in case the user changes their
	// setting for prefixing quote lines with author initials.
	// The quote prefix (optionally with author's initials) are passed to
	// wrapTextLinesForQuoting().
	// If not configured to re-wrap quote lines, then if configured to
	// prefix quote lines with author's initials, then we need to
	// prefix them here with gQuotePrefix.
	if (gQuoteLines.length > 0)
	{
		// The first time this function runs, create a backup array of the original
		// quote lines
		if (typeof(doQuoteSelection.backupQuoteLines) === "undefined")
		{
			doQuoteSelection.backupQuoteLines = []; // Array
			for (var i = 0; i < gQuoteLines.length; ++i)
				doQuoteSelection.backupQuoteLines.push(gQuoteLines[i]);
		}

		// If this is the first time the user has opened the quote window or if the
		// user has changed their settings for using author's initials in quote lines,
		// then re-copy the original quote lines into gQuoteLines and re-wrap them.
		var quoteLineInitialsSettingChanged = (doQuoteSelection.useQuoteLineInitials != gUserSettings.useQuoteLineInitials);
		var indentQuoteLinesWithInitialsSettingChanged = (doQuoteSelection.indentQuoteLinesWithInitials != gUserSettings.indentQuoteLinesWithInitials);
		var trimSpacesFromQuoteLinesSettingChanged = (doQuoteSelection.trimSpacesFromQuoteLines != gUserSettings.trimSpacesFromQuoteLines);
		var wrapQuoteLinesChanged = (doQuoteSelection.wrapQuoteLines != gUserSettings.wrapQuoteLines);
		var joinWrappedQuoteLinesChanged = (doQuoteSelection.joinQuoteLinesWhenWrapping != gUserSettings.joinQuoteLinesWhenWrapping);
		if (!gUserHasOpenedQuoteWindow || quoteLineInitialsSettingChanged || indentQuoteLinesWithInitialsSettingChanged ||
		    trimSpacesFromQuoteLinesSettingChanged || wrapQuoteLinesChanged || joinWrappedQuoteLinesChanged)
		{
			doQuoteSelection.useQuoteLineInitials = gUserSettings.useQuoteLineInitials;
			doQuoteSelection.indentQuoteLinesWithInitials = gUserSettings.indentQuoteLinesWithInitials;
			doQuoteSelection.trimSpacesFromQuoteLines = gUserSettings.trimSpacesFromQuoteLines;
			doQuoteSelection.wrapQuoteLines = gUserSettings.wrapQuoteLines;

			// If the user changed the setting for trimming spaces from quote lines,
			// then re-populate gQuoteLines with the original quote lines.
			if (trimSpacesFromQuoteLinesSettingChanged)
			{
				//readQuoteOrMessageFile();
				gQuoteLines = [];
				for (var i = 0; i < doQuoteSelection.backupQuoteLines.length; ++i)
					gQuoteLines.push(doQuoteSelection.backupQuoteLines[i]);
			}

			// If the user has opened the quote window before, then empty gQuoteLines
			// and re-copy the original quote lines back into it.
			if (gUserHasOpenedQuoteWindow)
			{
				gQuoteLines.length = 0;
				for (var i = 0; i < doQuoteSelection.backupQuoteLines.length; ++i)
					gQuoteLines.push(doQuoteSelection.backupQuoteLines[i]);
			}

			// Reset the selected quote line index & top displayed quote line index
			// back to 0 to ensure valid screen display & scrolling behavior
			gQuoteLinesIndex = 0;
			gQuoteLinesTopIndex = 0;

			// If configured to do so, and the message is not a poll, ensure the
			// quote lines are wrapped according to the editor configuration, or
			// by default to 79 columns
			var maxQuoteLineLength = 79;
			setQuotePrefix();
			// gConfigSettings.reWrapQuoteLines
			if (gUserSettings.wrapQuoteLines && !curMsgIsPoll(gMsgAreaInfo))
			{
				// If the settings for the user's configured editor have quote wrapping
				// enabled with a number of columns, then use that number of columns.
				// The wrapTextLinesForQuoting() function is what prepends the quote
				// prefix onto the quote lines. It also does wrapping, so (at least for
				// now) we need some value for the maximum quote line length, and we need
				// to call that function to prepend the quote prefix onto the quote lines
				var quoteWrapCfg = getEditorQuoteWrapCfgFromSCFG();
				if (quoteWrapCfg.quoteWrapEnabled && quoteWrapCfg.quoteWrapCols > 0)
					maxQuoteLineLength = quoteWrapCfg.quoteWrapCols;
				gQuoteLines = wrapTextLinesForQuoting(gQuoteLines, gQuotePrefix, gUserSettings.indentQuoteLinesWithInitials, gUserSettings.trimSpacesFromQuoteLines, maxQuoteLineLength, gUserSettings.joinQuoteLinesWhenWrapping);
			}
			else if (gUserSettings.useQuoteLineInitials)
			{
				var maxQuoteLineWidth = maxQuoteLineLength - gQuotePrefix.length;
				for (var i = 0; i < gQuoteLines.length; ++i)
					gQuoteLines[i] = quote_msg(gQuoteLines[i], maxQuoteLineWidth, gQuotePrefix);
			}
			else
			{
				// Quote with the default quote prefix (no initials)
				for (var i = 0; i < gQuoteLines.length; ++i)
					gQuoteLines[i] = quote_msg(gQuoteLines[i], maxQuoteLineLength);
			}
		}
	}

	// Set up some variables
	var curpos = {
		x: pCurpos.x,
		y: pCurpos.y
	};

	// Make the quote window's height about 42% of the edit area.
	const quoteWinHeight = Math.floor(gEditHeight * 0.42) + 1;
	// The first line on the screen where quote lines are written
	const quoteTopScreenRow = console.screen_rows - quoteWinHeight + 2;

	// Display the top border of the quote window.
	fpDrawQuoteWindowTopBorder(quoteWinHeight, gEditLeft, gEditRight);
	console.gotoxy(gEditLeft, gEditBottom+1);
	fpDrawQuoteWindowBottomBorder(gEditLeft, gEditRight);

	var quoteLineMenu = createQuoteLineMenu(quoteTopScreenRow, pQuoteKey);
	var insertedQuoteLines = false;
	// Customize the menu's OnItemSelect function to add the selected quote
	// line to the message.  Note that the menu's exitOnItemSelect is set
	// to false in createQuoteLineMenu() so that its input loop won't
	// exit when the user selects a quote line.
	quoteLineMenu.OnItemSelect = function(pQuoteLineIdx, pSelected)
	{
		if (!pSelected) return; // pSelected should always be true for this, but just in case..

		// Keep gQuoteLinesIndex up to date
		gQuoteLinesIndex = pQuoteLineIdx;

		// numTimesToMoveDown specifies how many times to move the cursor
		// down after inserting the quote line into the message.
		var numTimesToMoveDown = 1;

		// Insert the quote line into gEditLines after the current gEditLines index.
		var quoteLine = getQuoteTextLine(pQuoteLineIdx, quoteLineMenu.size.width);
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

		insertedQuoteLines = true;

		// Refresh the part of the message that needs to be refreshed on the
		// screen (above the quote window).
		if (curpos.y < quoteTopScreenRow-1)
			displayEditLines(curpos.y, gEditLinesIndex, quoteTopScreenRow-2, false, true);

		gEditLinesIndex += numTimesToMoveDown;

		// Go down one line in the quote window
		quoteLineMenu.DoKeyDown();

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
					displayEditLines(gEditTop, gEditLinesIndex-(gEditBottom-gEditTop), quoteTopScreenRow-2, false, true);
			}
			else
				++curpos.y;
		}
	};
	// Set the current & top item indexes back into the menu, if we know them already
	if (typeof(doQuoteSelection.selectedQuoteLineIdx) !== "undefined" && typeof(doQuoteSelection.topQuoteLineIdx) !== "undefined")
	{
		quoteLineMenu.selectedItemIdx = doQuoteSelection.selectedQuoteLineIdx;
		quoteLineMenu.topItemIdx = doQuoteSelection.topQuoteLineIdx;
	}
	// Now, display the quote line menu & run its input loop.  Also, save the quote line menu's
	// selected & top item indexes for next time the user opens the quote window.
	quoteLineMenu.GetVal();
	doQuoteSelection.selectedQuoteLineIdx = quoteLineMenu.selectedItemIdx;
	doQuoteSelection.topQuoteLineIdx = quoteLineMenu.topItemIdx;

	// We've exited quote mode.  Refresh the message text on the screen.  Note:
	// This will refresh only the quote window portion of the screen if the
	// cursor row is at or below the top of the quote window, and it will also
	// refresh the screen if the cursor row is above the quote window.
	const quoteWinTopScreenRow = quoteTopScreenRow-1;
	displayEditLines(quoteWinTopScreenRow, gEditLinesIndex-(curpos.y-quoteWinTopScreenRow),
	                 gEditBottom, true, true);

	// Draw the bottom edit border to erase the bottom border of the
	// quote window.
	fpDisplayTextAreaBottomBorder(gEditBottom+1, gUseQuotes, gEditLeft, gEditRight,
	                              gInsertMode, gConfigSettings.allowColorSelection);

	// Make sure the color is correct for editing.
	console.print(chooseEditColor());
	// Put the cursor where it should be.
	console.gotoxy(curpos);

	// Ensure we're using the correct text color
	if (insertedQuoteLines)
	{
		gTextAttrs = "\x01n";
		console.print(gTextAttrs);
		// It might be good to ensure the next (non-quote) edit line starts with a normal attribute, but
		// the edit line is undefined at this point, and doesn't really need to, since SlyEdit will prepend
		// quote blocks with a normal attribute when saving anyway.
		//if (messageLinesHaveAttrs())
		//	gEditLines[gEditLinesIndex].attrs[0] = "\x01n";
	}

	gUserHasOpenedQuoteWindow = true;

	// Set the settings in the return object, and return it.
	retObj.x = curpos.x;
	retObj.y = curpos.y;
	return retObj;
}
// Creates the DDLightbarMenu for selecting a quote line
//
// Parameters:
//  pQuoteTopScreenRow: The first line on the screen where quote lines are written
//  pQuoteKey: The hotkey to use to open/close the quote window (could be Ctrl-Q
//             or Ctrl-Y, depending on the SlyEdit configuration)
function createQuoteLineMenu(pQuoteTopScreenRow, pQuoteKey)
{
	// Quote window parameters
	const quoteBottomScreenRow = console.screen_rows - 2;
	const quoteWinTopScreenRow = pQuoteTopScreenRow-1;
	const quoteWinWidth = gEditRight - gEditLeft + 1;
	var quoteWinInnerHeight = quoteBottomScreenRow - pQuoteTopScreenRow + 1; // # of quote lines in the quote window

	// Create the quote line window
	var quoteLineMenu = new DDLightbarMenu(1, pQuoteTopScreenRow, console.screen_columns, quoteWinInnerHeight);
	quoteLineMenu.scrollbarEnabled = false;
	quoteLineMenu.borderEnabled = false;
	quoteLineMenu.multiSelect = false;
	quoteLineMenu.ampersandHotkeysInItems = false;
	quoteLineMenu.wrapNavigation = false;
	// Have the menu persist on screen when items are selected (with the Enter key)
	quoteLineMenu.exitOnItemSelect = false;
	quoteLineMenu.colors.itemColor = gQuoteWinTextColor;
	quoteLineMenu.colors.selectedItemColor = gQuoteLineHighlightColor;

	// Add additional keypresses for quitting the menu's input loop
	quoteLineMenu.AddAdditionalQuitKeys(pQuoteKey);

	// Change the menu's NumItems() and GetItem() function to reference
	// the message list in this object rather than add the menu items
	// to the menu
	quoteLineMenu.NumItems = function() {
		return gQuoteLines.length;
	};
	quoteLineMenu.GetItem = function(pQuoteLineIdx) {
		var menuItemObj = this.MakeItemWithRetval(-1);
		if (pQuoteLineIdx >= 0 && pQuoteLineIdx < gQuoteLines.length)
		{
			// Note: this refers to the quote line menu object (DDLightbarMenu)
			menuItemObj.text = getQuoteTextLine(pQuoteLineIdx, this.size.width);
			menuItemObj.retval = pQuoteLineIdx;
			menuItemObj.textIsUTF8 = !str_is_ascii(menuItemObj.text);
		}
		return menuItemObj;
	};

	// Set the quote menu's currently selected index to the next one available
	quoteLineMenu.SetSelectedItemIdx(gQuoteLinesIndex == 0 ? 0 : gQuoteLinesIndex+1);

	return quoteLineMenu;
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
		//if (gUserSettings.useQuoteLineInitials)
		if (gUserSettings.wrapQuoteLines) // gConfigSettings.reWrapQuoteLines
		{
			// Note: substrWithAttrCodes() is defined in dd_lightbar_menu.js
			if ((gQuoteLines[pIndex] != null) && (gQuoteLines[pIndex].length > 0))
				textLine = substrWithAttrCodes(gQuoteLines[pIndex], 0, pMaxWidth-1);
		}
		else
		{
			if ((gQuoteLines[pIndex] != null) && (gQuoteLines[pIndex].length > 0))
			{
				//textLine = quote_msg(gQuoteLines[pIndex], pMaxWidth-1, gQuotePrefix);
				textLine = gQuoteLines[pIndex];
			}
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
	var retObj = {
		x: pCurpos.x,
		y: pCurpos.y,
		currentWordLength: 0
	};

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
		// First, get the attributes up to & including the current line so that we
		// can ensure the attributes are preserved for this point onward.
		var originalAttrs = "";
		if (gEditLines[gEditLinesIndex].hasAttrs())
			originalAttrs = getAllEditLineAttrsUntilLineIdx(gEditLinesIndex, true);
		// Remove the current line and put the attributes at its start
		gEditLines.splice(gEditLinesIndex, 1);
		if (originalAttrs.length > 0)
		{
			if (gEditLines[gEditLinesIndex].attrs.hasOwnProperty(0))
			{
				gEditLines[gEditLinesIndex].attrs[0] = gEditLines[gEditLinesIndex].attrs[0] + originalAttrs;
				var normalAttrIdx = gEditLines[gEditLinesIndex].attrs[0].lastIndexOf("\x01n");
				if (normalAttrIdx < 0)
					normalAttrIdx = gEditLines[gEditLinesIndex].attrs[0].lastIndexOf("\x01N");
				if (normalAttrIdx > -1)
					gEditLines[gEditLinesIndex].attrs[0] = gEditLines[gEditLinesIndex].attrs[0].substr(normalAttrIdx);
			}
			else
				gEditLines[gEditLinesIndex].attrs[0] = originalAttrs;
		}
		// Refresh the message lines on the screen
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

	// Apply anny attribute codes until the given start array index
	var currentAttrCodes = getAllEditLineAttrsUntilLineIdx(pArrayIndex);
	console.print("\x01n" + currentAttrCodes);
	// Display the message lines
	var screenLine = pStartScreenRow;
	var arrayIndex = pArrayIndex;
	while ((screenLine <= endScreenRow) && (arrayIndex < gEditLines.length))
	{
		var textLine = gEditLines[arrayIndex].getText(true);
		if ((gEditAreaBuffer[screenLine] != textLine) || pIgnoreEditAreaBuffer)
		{
			var lineIsQuoteLine = isQuoteLine(gEditLines, arrayIndex);
			// Make sure the text line doesn't exceed the edit width (unlikely)
			if (console.strlen(textLine) > gEditWidth)
				textLine = shortenStrWithAttrCodes(textLine, gEditWidth, true);
			// If the line is a quote line, then apply the quote line color (and strip other
			// attribute codes from the line)
			if (lineIsQuoteLine)
				textLine = "\x01n" + gQuoteLineColor + strip_ctrl(textLine);
			// If this line is not a quote line but the last line was, include a normal attribute at the
			// beginning to remove the quote line color.
			else if (arrayIndex > 0 && isQuoteLine(gEditLines, arrayIndex-1))
				textLine = "\x01n" + textLine;
			console.gotoxy(gEditLeft, screenLine);
			// TODO: For some reason, neither the end line print() function nor printStrConsideringUTF8()
			// are able to correctly print UTF-8 characters for quote lines. So for quote lines, print
			// using console.print() with a mode bit if needed.  We strip color from quote lines, so
			// this should be fine.
			if (lineIsQuoteLine)
				console.print(textLine, gPrintMode);
			else
			{
				gEditLines[arrayIndex].print(true, gEditWidth, true);
				//printStrConsideringUTF8(textLine, gPrintMode);
			}

			gEditAreaBuffer[screenLine] = textLine;
			// Clear to the end of the line, to erase any previously written text.
			console.cleartoeol("\x01n");
			// Ensure the correct attributes are being used after the cleartoeol()
			currentAttrCodes += gEditLines[arrayIndex].getAttrs(null, true);
			var normalAttrIdx = currentAttrCodes.lastIndexOf("\x01n");
			if (normalAttrIdx < 0)
				normalAttrIdx = currentAttrCodes.lastIndexOf("\x01N");
			if (normalAttrIdx > -1)
				currentAttrCodes = currentAttrCodes.substr(normalAttrIdx);
			console.print(currentAttrCodes, gPrintMode);
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
		var lineLength = gEditLines[arrayIndex].screenLength();
		if (lineLength < gEditWidth)
		{
			--screenLine;
			console.gotoxy(gEditLeft + gEditLines[arrayIndex].screenLength(), screenLine);
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
		console.attributes = "N";
		var screenLineBackup = screenLine; // So we can move the cursor back
		clearMsgAreaToBottom(incrementLineBeforeClearRemaining ? screenLine+1 : screenLine, pIgnoreEditAreaBuffer);
		// Move the cursor back to the end of the current text line.
		if (typeof(gEditLines[arrayIndex]) != "undefined")
			console.gotoxy(gEditLeft + gEditLines[arrayIndex].screenLength(), screenLineBackup);
		else
			console.gotoxy(gEditLeft, screenLineBackup);
	}
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
	for (var i = 0; i < gEditLines.length && msgEmpty; ++i)
		msgEmpty = msgEmpty && (gEditLines[i].screenLength() == 0);
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
//  pClearExtraWidth: Boolean - Optional.  If true, then any space after the end of the line
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
	//var formatStr = "%-" + pWidth + "s";
	var editColor = chooseEditColor();
	var formatStr = "\x01n%-" + pWidth + "s" + editColor;
	var actualLenWritten = 0; // Actual length of text written for each line
	for (var rectangleLine = 0; rectangleLine < pHeight; ++rectangleLine)
	{
		// Output the correct color for the line
		//console.print("\x01n" + (isQuoteLine(gEditLines, editLinesIndex) ? gQuoteLineColor : editColor));
		// If the current line is a quote line, then output the quote line color attribute to set the line color.
		if (isQuoteLine(gEditLines, editLinesIndex))
			console.print("\x01n" + gQuoteLineColor);
		// If the previous line was a quote line, then output the appropriate attribute codes for this line
		else if (editLinesIndex > 0 && isQuoteLine(gEditLines, editLinesIndex-1))
			console.attributes = "N";
		// Otherwise, get all attribute codes from the lines before this one and output them
		else
			console.print("\x01n" + getAllEditLineAttrsUntilLineIdx(editLinesIndex));
		// Go to the position on the screen
		screenY = pY + rectangleLine;
		console.gotoxy(pX, screenY);
		// Display the message text.  If the current edit line is valid,
		// then print it; otherwise, just print spaces to blank out the line.
		if (typeof(gEditLines[editLinesIndex]) != "undefined")
		{
			actualLenWritten = printEditLine(editLinesIndex, gConfigSettings.allowColorSelection, editLineIndex, pWidth);
			// If pClearExtraWidth is true, then if the box width is longer than
			// the text that was written, then output spaces to clear the rest
			// of the line to erase the rest of the box line.
			if (pClearExtraWidth)
			{
				if (pWidth > actualLenWritten)
					printf("%*s", pWidth-actualLenWritten, "");
			}
		}
		else
			printf(formatStr, "");

		++editLinesIndex;
	}
	console.print(editColor);
}

// Functions to call the appropriate ESC menu function for the UI style
//
// Parameters:
//  pCurpos: The current cursor position
//
// Return value: The ESC menu action value representing the uesr's choice
function callIceESCMenu(pCurpos)
{
	var chosenAction = doIceESCMenu(console.screen_rows, gCanCrossPost);
	// If the user didn't choose help, then we need to refresh the bottom row
	// on the screen.
	if (chosenAction != ESC_MENU_HELP_COMMAND_LIST)
		fpDisplayBottomHelpLine(console.screen_rows, gUseQuotes, gUserSettings.ctrlQQuote);
	return chosenAction;
}
function callDCTESCMenu(pCurpos)
{
	var editLineDiff = pCurpos.y - gEditTop;
	var chosenAction = doDCTESCMenu(gEditLeft, gEditRight, gEditTop,
	                                displayMessageRectangle, gEditLinesIndex,
	                                editLineDiff, gCanCrossPost);
	return chosenAction;
}
// Shows the ESC menu (different, depending on the UI style) and takes action
// according to the user's choice from the menu.
//
// Parameters:
//  pCurpos: The current cursor position
//  pCurrentWordLength: The length of the current word
//
// Return value: An object containing values to be used by the main input loop.
//               The object will contain these values:
//                 returnCode: The value to use as the editor's return code
//                 continueOn: Whether or not the input loop should continue
//                 x: The horizontal component of the cursor position
//                 y: The vertical component of the cursor position
//                 currentWordLength: The length of the current word
//                 sendImmediately: Boolean - Whether or not to send immediately (i.e., if importing a file)
function doESCMenu(pCurpos, pCurrentWordLength)
{
	var returnObj = {
		returnCode: 0,
		continueOn: true,
		x: pCurpos.x,
		y: pCurpos.y,
		currentWordLength: pCurrentWordLength
	};

	switch (fpCallESCMenu(pCurpos))
	{
		case ESC_MENU_SAVE:
			returnObj.returnCode = 0;
			returnObj.continueOn = false;
			break;
		case ESC_MENU_ABORT:
			// Before aborting, ask they user if they really want to abort.
			var editObjName = (gMsgAreaInfo.subBoardCode.length > 0 ? "message" : "edit");
			if (promptYesNo("Abort " + editObjName, false, "Abort", false, false))
			{
				returnObj.returnCode = 1; // Aborted
				returnObj.continueOn = false;
			}
			break;
		case ESC_MENU_EDIT_MESSAGE:
			// Nothing needs to be done for this menu option
			break;
		case ESC_MENU_INS_OVR_TOGGLE: // Insert/overwrite mode toggle
			toggleInsertMode(pCurpos);
			break;
		case ESC_MENU_SYSOP_IMPORT_FILE:
			if (user.is_sysop)
			{
				var retval = importFile(pCurpos);
				returnObj.x = retval.x;
				returnObj.y = retval.y;
				returnObj.currentWordLength = retval.currentWordLength;
				returnObj.continueOn = !retval.sendImmediately;
			}
			break;
		case ESC_MENU_SYSOP_EXPORT_FILE:
			if (user.is_sysop)
			{
				exportToFile();
				console.gotoxy(returnObj.x, returnObj.y);
			}
			break;
		case ESC_MENU_FIND_TEXT:
			var retval = findText(pCurpos);
			returnObj.x = retval.x;
			returnObj.y = retval.y;
			break;
		case ESC_MENU_HELP_COMMAND_LIST:
			displayCommandList(true, true, true, gCanCrossPost,
			                   gConfigSettings.enableTextReplacements, gConfigSettings.allowUserSettings,
			                   gConfigSettings.allowSpellCheck, gConfigSettings.allowColorSelection, gCanChangeSubject);
			clearEditAreaBuffer();
			fpRedrawScreen(gEditLeft, gEditRight, gEditTop, gEditBottom, gTextAttrs, gInsertMode,
			               gUseQuotes, gUserSettings.ctrlQQuote, gEditLinesIndex-(pCurpos.y-gEditTop),
			               displayEditLines);
			break;
		case ESC_MENU_HELP_GRAPHIC_CHAR:
			var graphicChar = promptForGraphicsChar(pCurpos);
			fpDisplayBottomHelpLine(console.screen_rows, gUseQuotes, gUserSettings.ctrlQQuote);
			console.gotoxy(pCurpos);
			if (graphicChar != null && typeof(graphicChar) === "string" && graphicChar.length > 0)
			{
				var retObject = doPrintableChar(graphicChar, pCurpos, pCurrentWordLength);
				returnObj.x = retObject.x;
				returnObj.y = retObject.y;
				returnObj.currentWordLength = retObject.currentWordLength;
			}
			break;
		case ESC_MENU_HELP_PROGRAM_INFO:
			displayProgramInfoBox(pCurpos);
			break;
		case ESC_MENU_CROSS_POST_MESSAGE:
			if (gCanCrossPost)
				doCrossPosting(pCurpos);
			break;
		case ESC_MENU_LIST_TEXT_REPLACEMENTS:
			if (gConfigSettings.enableTextReplacements)
				listTextReplacements();
			break;
		case ESC_MENU_USER_SETTINGS:
			if (gConfigSettings.allowUserSettings)
				doUserSettings(pCurpos, true);
			break;
		case ESC_MENU_SPELL_CHECK:
			var spellCheckRetObj = doSpellCheck(pCurpos, false);
			returnObj.x = spellCheckRetObj.x;
			returnObj.y = spellCheckRetObj.y;
			returnObj.currentWordLength = spellCheckRetObj.currentWordLength;
			break;
		case ESC_MENU_INSERT_MEME:
			// Input a meme from the user
			var memeInputRetObj = doMemeInput();
			// If a meme was added and we're able, move the
			// cursor below the meme that was addded
			if (memeInputRetObj.numMemeLines > 0)
			{
				var newY = returnObj.y + memeInputRetObj.numMemeLines;
				if (newY <= gEditBottom)
					returnObj.y = newY;
				else
					returnObj.y = gEditBottom;
			}
			if (memeInputRetObj.refreshScreen)
			{
				// Refresh the screen
				clearEditAreaBuffer();
				fpRedrawScreen(gEditLeft, gEditRight, gEditTop, gEditBottom, gTextAttrs,
				               gInsertMode, gUseQuotes, gUserSettings.ctrlQQuote,
				               gEditLinesIndex-(returnObj.y-gEditTop), displayEditLines);
			}
			break;
	}

	// Make sure the edit color attribute is set back.
	console.print(chooseEditColor());

	return returnObj;
}

// Displays SlyEdit program information in a box in the edit area.  Waits for
// a keypress, and then erases the box.
//
// Parameters:
//  pCurpos: The current cursor position (with x and y properties)
function displayProgramInfoBox(pCurpos)
{
	var boxWidth = 47;
	var boxHeight = 12;
	var boxTopLeftX = Math.floor(console.screen_columns / 2) - Math.floor(boxWidth/2);
	var boxTopLeftY = gEditTop + Math.floor(gEditHeight / 2) - Math.floor(boxHeight/2);
	var borderBGColor = "\x01b\x01h\x01" + "4"; // High blue with blue background
	// Draw the box border
	console.gotoxy(boxTopLeftX, boxTopLeftY);
	console.print("\x01n" + borderBGColor);
	console.print(CP437_BOX_DRAWINGS_UPPER_LEFT_SINGLE);
	var innerWidth = boxWidth - 2;
	for (var i = 0; i < innerWidth; ++i)
		console.print(CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE);
	console.print(CP437_BOX_DRAWINGS_UPPER_RIGHT_SINGLE);
	var innerHeight = boxHeight - 2;
	for (var i = 0; i < innerHeight; ++i)
	{
		console.gotoxy(boxTopLeftX, boxTopLeftY+i+1);
		console.print(CP437_BOX_DRAWINGS_LIGHT_VERTICAL);
		console.gotoxy(boxTopLeftX+boxWidth-1, boxTopLeftY+i+1);
		console.print(CP437_BOX_DRAWINGS_LIGHT_VERTICAL);
	}
	console.gotoxy(boxTopLeftX, boxTopLeftY+boxHeight-1);
	console.print(CP437_BOX_DRAWINGS_LOWER_LEFT_SINGLE);
	for (var i = 0; i < innerWidth; ++i)
		console.print(CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE);
	console.print(CP437_BOX_DRAWINGS_LOWER_RIGHT_SINGLE);
	console.gotoxy(boxTopLeftX+1, boxTopLeftY+1);
	var boxWidthFillFormatStr = "%" + innerWidth + "s";
	printf(boxWidthFillFormatStr, "");
	console.print("\x01w");
	var textLine = centeredText(innerWidth, EDITOR_PROGRAM_NAME + " " + EDITOR_VERSION + " (" + EDITOR_VER_DATE + ")");
	console.gotoxy(boxTopLeftX+1, boxTopLeftY+2);
	console.print(textLine);
	console.gotoxy(boxTopLeftX+1, boxTopLeftY+3);
	console.print(centeredText(innerWidth, "Copyright (C) 2009-" + COPYRIGHT_YEAR + " Eric Oulashin"));
	console.gotoxy(boxTopLeftX+1, boxTopLeftY+4);
	printf(boxWidthFillFormatStr, "");
	console.gotoxy(boxTopLeftX+1, boxTopLeftY+5);
	console.print("\x01y");
	console.print(centeredText(innerWidth, "BBS tools web page:"));
	console.print("\x01m");
	console.gotoxy(boxTopLeftX+1, boxTopLeftY+6);
	console.print(centeredText(innerWidth, "http://digdist.synchro.net/DDBBSStuff.html"));
	console.gotoxy(boxTopLeftX+1, boxTopLeftY+7);
	printf(boxWidthFillFormatStr, "");
	console.gotoxy(boxTopLeftX+1, boxTopLeftY+8);
	printf(boxWidthFillFormatStr, "");
	console.gotoxy(boxTopLeftX+1, boxTopLeftY+9);
	console.print("\x01c");
	console.print(centeredText(innerWidth, "Thanks for using " + EDITOR_PROGRAM_NAME + "!"));
	console.gotoxy(boxTopLeftX+1, boxTopLeftY+10);
	printf(boxWidthFillFormatStr, "");

	console.attributes = "N";
	// Wait for a keypress
	console.getkey(K_NOCRLF|K_NOSPIN);
	// Erase the info box
	//var topEditIndex = gEditLinesIndex-(curpos.y-gEditTop);
	var editLineIndexAtSelBoxTopRow = gEditLinesIndex - (pCurpos.y-boxTopLeftY);
	displayMessageRectangle(boxTopLeftX, boxTopLeftY, boxWidth, boxHeight, editLineIndexAtSelBoxTopRow, true);
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
	if ((pTextLineIndex == gEditLines[pEditLinesIndex].length()) || (gEditLines[pEditLinesIndex].text.charAt(gTextLineIndex) == " "))
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
	var line = new TextLine(pString, pHardNewline, pIsQuoteLine);
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
//
// Return value: An object containing the following information:
//               x: The horizontal component of the cursor's location
//               y: The vertical component of the cursor's location
//               currentWordLength: The length of the current word
//               sendImmediately: Boolean - Whether or not to send the message immediately
function importFile(pCurpos)
{
	// Create the return object
	var retObj = {
		x: pCurpos.x,
		y: pCurpos.y,
		currentWordLength: getWordLength(gEditLinesIndex, gTextLineIndex),
		sendImmediately: false
	};

	// Don't let non-sysops do this.
	if (!user.is_sysop)
		return retObj;

	var loadedAFile = false;
	// This loop continues to prompt the user until they enter a valid
	// filename or a blank string.
	var continueOn = true;
	while (continueOn)
	{
		// Go to the last row on the screen and prompt the user for a filename
		var promptText = "\x01n\x01cFile:\x01h";
		var promptTextLen = console.strlen(promptText);
		console.gotoxy(1, console.screen_rows);
		console.cleartoeol("\x01n");
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
					// Prompt if the user wants to send the file as-is or import into the edit area
					//promptYesNo(pQuestion, pDefaultYes, pBoxTitle, pIceRefreshForBothAnswers, pAlwaysEraseBox)
					retObj.sendImmediately = promptYesNo("Send immediately", true, "Send", true, true);
					var fileLine = "";
					if (retObj.sendImmediately)
					{
						while (!inFile.eof)
						{
							// Read the next line from the file
							fileLine = inFile.readln(2048);
							// fileLine should be a string, but I've seen some cases
							// where for some reason it isn't.  If it's not a string,
							// then continue onto the next line.
							if (typeof(fileLine) != "string")
								continue;
							// Add the line to gEditLines
							// TODO: It seems this isn't populating gEditLines and
							// it's sending an empty message.
							gEditLines.push(new TextLine(fileLine, true, false));
						}
					}
					else
					{
						const maxLineLength = gEditWidth - 1; // Don't insert lines longer than this
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
					writeWithPause(1, console.screen_rows, "\x01y\x01hUnable to open the file!", ERRORMSG_PAUSE_MS);
			}
			else // Could not find the correct case for the file (it doesn't exist?)
				writeWithPause(1, console.screen_rows, "\x01y\x01hUnable to locate the file!", ERRORMSG_PAUSE_MS);
		}
	}

	// Refresh the help line on the bottom of the screen
	fpDisplayBottomHelpLine(console.screen_rows, gUseQuotes, gUserSettings.ctrlQQuote);

	// If not sending immediately and we loaded a file, then refresh the message text.
	if (!retObj.sendImmediately && loadedAFile)
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
function exportToFile()
{
   // Don't let non-sysops do this.
   if (!user.is_sysop)
      return;

   // Go to the last row on the screen and prompt the user for a filename
   var promptText = "\x01n\x01cFile:\x01h";
   var promptTextLen = console.strlen(promptText);
   console.gotoxy(1, console.screen_rows);
   console.cleartoeol("\x01n");
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
         writeWithPause(1, console.screen_rows, "\x01m\x01hMessage exported.", ERRORMSG_PAUSE_MS);
      }
      else // Could not open the file for writing
         writeWithPause(1, console.screen_rows, "\x01y\x01hUnable to open the file for writing!", ERRORMSG_PAUSE_MS);
   }
   else // No filename specified
      writeWithPause(1, console.screen_rows, "\x01m\x01hMessage not exported.", ERRORMSG_PAUSE_MS);

   // Refresh the help line on the bottom of the screen
   fpDisplayBottomHelpLine(console.screen_rows, gUseQuotes, gUserSettings.ctrlQQuote);
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
	var retObj = {
		x: pCurpos.x,
		y: pCurpos.y
	};

	// This function makes use of the following "static" variables:
	//  lastSearchText: The text searched for last
	//  searchStartIndex: The starting index for gEditLines that should
	//                    be used for the search
	if (typeof(findText.lastSearchText) == "undefined")
		findText.lastSearchText = "";
	if (typeof(findText.searchStartIndex) == "undefined")
		findText.searchStartIndex = 0;

	// Go to the last row on the screen and prompt the user for text to find
	var promptText = "\x01n\x01cText:\x01h";
	var promptTextLen = console.strlen(promptText);
	console.gotoxy(1, console.screen_rows);
	console.cleartoeol("\x01n");
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
				retObj.x = gEditLeft + gTextLineIndex;
				var refresh = false;
				if (i < editLinesTopIndex)
				{
					// The line is above the edit area.
					refresh = true;
					retObj.y = gEditTop;
					editLinesTopIndex = i;
				}
				else if (i >= editLinesTopIndex + gEditHeight)
				{
					// The line is below the edit area.
					refresh = true;
					retObj.y = gEditBottom;
					editLinesTopIndex = i - gEditHeight + 1;
				}
				else
				{
					// The line is inside the edit area.
					retObj.y = pCurpos.y + (i - gEditLinesIndex);
				}

				gEditLinesIndex = i;

				if (refresh)
					displayEditLines(gEditTop, editLinesTopIndex, gEditBottom, true, true);

				// Highlight the found text on the line by briefly displaying it in a
				// different color.
				var highlightText = gEditLines[i].text.substr(textIndex, searchText.length);
				console.gotoxy(retObj.x, retObj.y);
				console.print("\x01n\x01k\x01" + "4" + highlightText);
				mswait(TEXT_SEARCH_PAUSE_MS);
				console.gotoxy(retObj.x, retObj.y);
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
			console.cleartoeol("\x01n");
			console.print("\x01y\x01hThe text wasn't found!");
			mswait(ERRORMSG_PAUSE_MS);

			findText.searchStartIndex = 0;
		}
	}

	// Refresh the help line on the bottom of the screen
	fpDisplayBottomHelpLine(console.screen_rows, gUseQuotes, gUserSettings.ctrlQQuote);

	// Make sure the cursor is positioned where it should be.
	console.gotoxy(retObj.x, retObj.y);

	return retObj;
}

// Performs spell checking.
//
// Parameters:
//  pCurpos: The current cursor position (with x and y properties)
//  pConfirmSpellcheck: Whether or not to confirm with the user that they
//                      really want to run spellcheck.  Optional; defaults
//                      to true.
//
// Return value: An object containing the following properties:
//               x: The horizontal component of the cursor position
//               y: The vertical component of the cursor position
//               currentWordLength: The length of the current word
function doSpellCheck(pCurpos, pConfirmSpellcheck)
{
	// Create the return object.
	var retObj = {
		x: pCurpos.x,
		y: pCurpos.y,
		currentWordLength: 0
	};

	var confirmSpellcheck = (typeof(pConfirmSpellcheck) == "boolean" ? pConfirmSpellcheck : true);
	if (confirmSpellcheck)
	{
		// Confirm if the user wants to do spellcheck
		if (!promptYesNo("Run spell check", true, "Spell check", false, true))
		{
			console.gotoxy(pCurpos.x, pCurpos.y);
			return retObj;
		}
	}

	// If there are no dictionaries specifies, then output an error message
	// and return.  The user settings should have dictionary filenames specified,
	// but if not, default to the SlyEdit dictionary configuration.
	var dictFilenames = null;
	if (gUserSettings.hasOwnProperty("dictionaryFilenames"))
		dictFilenames = gUserSettings.dictionaryFilenames;
	else
		dictFilenames = gConfigSettings.dictionaryFilenames;
	if (dictFilenames.length == 0)
	{
		writeWithPause(1, console.screen_rows, "\x01y\x01hThere are no dictionaries configured!\x01n", ERRORMSG_PAUSE_MS);
		// Refresh the help line on the bottom of the screen
		fpDisplayBottomHelpLine(console.screen_rows, gUseQuotes, gUserSettings.ctrlQQuote);
		console.gotoxy(pCurpos.x, pCurpos.y);
		return retObj;
	}

	// Load the dictionary file(s)
	console.gotoxy(1, console.screen_rows);
	console.cleartoeol("\x01n");
	if (dictFilenames.length > 1)
		console.print("Reading dictionaries...");
	else
		console.print("Reading dictionary...");
	var dictionaries = [];
	for (var i = 0; i < dictFilenames.length; ++i)
	{
		var dictionary = readDictionaryFile(dictFilenames[i], true);
		if (dictionary.length > 0)
			dictionaries.push(dictionary);
	}
	// If we were unable to load any of the dictionary files, then output an error and
	// return.
	if (dictionaries.length == 0)
	{
		writeWithPause(1, console.screen_rows, "\x01y\x01hUnable to load the dictionary file(s)!\x01n", ERRORMSG_PAUSE_MS);
		// Refresh the help line on the bottom of the screen
		fpDisplayBottomHelpLine(console.screen_rows, gUseQuotes, gUserSettings.ctrlQQuote);
		console.gotoxy(pCurpos.x, pCurpos.y);
		return retObj;
	}

	// Write "Spell check in progress" in place of the "Reading dictionary..." text
	console.gotoxy(1, console.screen_rows);
	console.print("Spell check in progress");

	// Look at all the words in the message and check to see if they're valid
	// words.  If not, then let the user correct them.
	var textIndex = 0; // The index of the text in the edit lines
	// editLinesTopIndex is the index of the line currently displayed
	// at the top of the edit area, and also the line to be displayed
	// at the top of the edit area.
	var editLinesTopIndex = gEditLinesIndex - (pCurpos.y - gEditTop);

	var fixedAnyWords = false;
	var continueOn = true;
	var oldPreviousLine = ""; // To store the previous uncorrected line, for screen update purposes
	for (var editLineIdx = 0; (editLineIdx < gEditLines.length) && continueOn; ++editLineIdx)
	{
		// Skip quote lines
		if (gEditLines[editLineIdx].isQuoteLine)
			continue;

		oldPreviousLine = gEditLines[editLineIdx].text;

		// The index in the line for where to start searching for the word
		// in order to highlight it if necessary
		var searchStartIdx = 0;
		var searchStartIdxInOldLine = 0;
		var lineWords = gEditLines[editLineIdx].text.split(" ");
		for (var wordIdx = 0; (wordIdx < lineWords.length) && continueOn; ++wordIdx)
		{
			// Spell check the current word in the line
			var spellChkRetObj = spellCheckWordInLine(dictionaries, editLineIdx, lineWords, wordIdx,
			                                          searchStartIdx, oldPreviousLine, searchStartIdxInOldLine,
			                                          editLinesTopIndex);
			continueOn = spellChkRetObj.continueSpellCheck;
			if (continueOn)
			{
				searchStartIdx = spellChkRetObj.searchStartIdx;
				searchStartIdxInOldLine = spellChkRetObj.searchStartIdxInOldLine;
				editLinesTopIndex = spellChkRetObj.editLinesTopIdx;
				if (spellChkRetObj.fixedWord)
					fixedAnyWords = true;
			}
		}
	}

	// If any words were fixed, then refresh the message text on the screen.
	if (fixedAnyWords)
	{
		// Re-wrap the message text to ensure it fits on the screen
		var nonQuoteBlockIdxes = findNonQuoteBlockIndexes(gEditLines);
		for (var i = 0; i < nonQuoteBlockIdxes.length; ++i)
		{
			// Create a string with the new section of text after fixes and line-wrap it.
			var nonQuoteBlockText = "";
			for (var lineIdx = nonQuoteBlockIdxes[i].start; lineIdx < nonQuoteBlockIdxes[i].end; ++lineIdx)
				nonQuoteBlockText += gEditLines[lineIdx].text + "\n";
			var textLines = lfexpand(word_wrap(nonQuoteBlockText, console.screen_columns-1, null, false)).split("\r\n");
			textLines.pop(); // Remove the last empty line which was added due to the split as done above
			var numLinesDiff = textLines.length - (nonQuoteBlockIdxes[i].end-nonQuoteBlockIdxes[i].start);
			// If lines were added, then splice in text lines into gEditLines where
			// appropriate.  If lines were removed, then remove lines from gEditLines.
			if (numLinesDiff > 0)
			{
				for (var counter = 0; counter < numLinesDiff; ++counter)
					gEditLines.splice(nonQuoteBlockIdxes[i].end, 0, new TextLine("", false, false));
			}
			else if (numLinesDiff < 0)
				gEditLines.splice(nonQuoteBlockIdxes[i].end, -numLinesDiff); // Make numLinesDiff a positive
			var endLineIdx = nonQuoteBlockIdxes[i].end + numLinesDiff;
			var wrappedLineIdx = 0;
			for (var lineIdx = nonQuoteBlockIdxes[i].start; lineIdx < endLineIdx; ++lineIdx)
				gEditLines[lineIdx].setText(textLines[wrappedLineIdx++]);
		}
	}
	// Scroll to the end of the message and put the cursor at the end of the last
	// line on the screen.  Also, ensure all the message tracking variables are
	// updated properly.
	editLinesTopIndex = gEditLines.length - gEditHeight;
	if (editLinesTopIndex < 0)
		editLinesTopIndex = 0;
	var height = gEditBottom - gEditTop + 1;
	displayMessageRectangle(1, gEditTop, console.screen_columns, height, editLinesTopIndex, true);
	var numMessageLinesOnScreen = gEditLines.length - editLinesTopIndex;
	gEditLinesIndex = editLinesTopIndex + numMessageLinesOnScreen - 1;
	retObj.x = gEditLeft + gEditLines[gEditLinesIndex].text.length;
	retObj.y = gEditTop + numMessageLinesOnScreen - 1;
	gTextLineIndex = gEditLines[gEditLinesIndex].text.length;
	retObj.currentWordLength = getWordLength(gEditLinesIndex, gTextLineIndex);

	// Refresh the help line on the bottom of the screen
	fpDisplayBottomHelpLine(console.screen_rows, gUseQuotes, gUserSettings.ctrlQQuote);

	// Make sure the cursor is positioned where it should be.
	console.gotoxy(retObj.x, retObj.y);

	return retObj;
}

// Helper for doSpellCheck(): Checks a word from an array of words (split from an edit line)
// and inputs a correction from the user if needed.
//
// Parameters:
//  pDictionaries: The array of dictionary arrays to use for word verification
//  pEditLineIdx: The index into gEditLines of the current word array being passed in
//  pWordArray: The array of words to check
//  pWordIdx: The index into pWordArray of the word to check
//  pSearchStartIdx: The current search start index in the current edit line
//  pOldEditLine: The "old" (original) edit line before any fixes
//  pSearchStartIdxInOldLine: The current search start index in the old edit line
//  pEditLinesTopIdx: The current value of the edit lines top index
//
// Return value: An object with the following properties:
//               skipped: Whether or not this word was skipped
//               searchStartIdx: The current search start index in the current edit line
//               searchStartIdxInOldLine: The current search start index in the old edit line
//               fixedWord: Whether or not the word was fixed by the user
//               continueSpellCheck: Boolean - Whether or not to continue spell check
//               editLinesTopIdx: The current value of the edit lines top index
function spellCheckWordInLine(pDictionaries, pEditLineIdx, pWordArray, pWordIdx,
                               pSearchStartIdx, pOldEditLine, pSearchStartIdxInOldLine,
                               pEditLinesTopIdx)
{
	var retObj = {
		skipped: false,
		searchStartIdx: 0,
		searchStartIdxInOldLine: 0,
		fixedWord: false,
		continueSpellCheck: true,
		editLinesTopIdx: pEditLinesTopIdx
	};
	// Ensure the word to test is all lowercase for case-insensitive matching
	var currentWord = pWordArray[pWordIdx].toLowerCase();
	// Ensure the word we're checking only has letters and/or an apostrophe.
	var currentWord = removeNonWordCharsFromStr(currentWord);
	// Now, ensure the word has only certain characters: Letters, apostrophe.  Skip it if not.
	if (!strIsAllWordChars(currentWord))
	{
		retObj.skipped = true;
		return retObj;
	}

	// If K_UTF8 exists, then SlyEdit supports UTF-8 strings
	var textMode = (g_K_UTF8Exists && gUserConsoleSupportsUTF8 ? P_UTF8 : P_NONE);

	// Ensure the word doesn't have any whitespace and isn't just whitespace
	currentWord = trimSpaces(currentWord, true, true, true);
	if (currentWord.length > 0)
	{
		// If the word isn't a valid word, then highlight it and let the user
		// correct it.
		if (!wordExists_MultipleDictionaries(pDictionaries, currentWord))
		{
			// Find the word's index in the line
			var wordIdxInLine = gEditLines[pEditLineIdx].text.toLowerCase().indexOf(currentWord, pSearchStartIdx);
			var wordIdxInOldLine = pOldEditLine.toLowerCase().indexOf(currentWord, pSearchStartIdxInOldLine);
			retObj.searchStartIdx = wordIdxInLine + 1;
			retObj.searchStartIdxInOldLine = wordIdxInOldLine + 1;
			// If the text was found in this line, then highlight it
			// and let the user change it
			if (wordIdxInLine > -1)
			{
				gTextLineIndex = wordIdxInLine;

				// If the line is above or below the edit area, then we'll need
				// to refresh the edit lines on the screen.  We also need to set
				// the cursor position to the proper place.
				retObj.x = gEditLeft + gTextLineIndex;
				var oldLineX = gEditLeft + wordIdxInOldLine;
				var refresh = false;
				if (pEditLineIdx < pEditLinesTopIdx)
				{
					// The line is above the edit area.
					refresh = true;
					retObj.y = gEditTop;
					retObj.editLinesTopIdx = pEditLineIdx;
				}
				else if (pEditLineIdx >= pEditLinesTopIdx + gEditHeight)
				{
					// The line is below the edit area.
					refresh = true;
					retObj.y = gEditBottom;
					retObj.editLinesTopIdx = pEditLineIdx - gEditHeight + 1;
				}
				else
				{
					// The line is inside the edit area.
					var indexDiff = pEditLineIdx - pEditLinesTopIdx;
					retObj.y = gEditTop + indexDiff;
				}

				gEditLinesIndex = pEditLineIdx;

				if (refresh)
					displayEditLines(gEditTop, retObj.editLinesTopIdx, gEditBottom, true, true);

				// Highlight the found text on the line by briefly displaying it in a
				// different color.
				var highlightText = gEditLines[pEditLineIdx].text.substr(wordIdxInLine, currentWord.length);
				//console.gotoxy(retObj.x, retObj.y); // Updated line position
				console.gotoxy(oldLineX, retObj.y);   // Old line position
				console.print("\x01n\x01k\x014");
				printStrConsideringUTF8(highlightText, gPrintMode);
				mswait(SPELL_CHECK_PAUSE_MS);
				//console.gotoxy(retObj.x, retObj.y); // Updated line position
				console.gotoxy(oldLineX, retObj.y);   // Old line position
				printStrConsideringUTF8(gEditLines[pEditLineIdx].substr(true, wordIdxInLine, currentWord.length), gPrintMode);
				retObj.x = wordIdxInLine + console.strlen(pWordArray[pWordIdx], textMode) + 1;
				// Prompt the user for a corrected word.  If they enter
				// a new word, then fix it in the text line.
				var wordCorrectRetObj = inputWordCorrection(currentWord, { x: retObj.x, y: retObj.y }, pEditLineIdx);
				if (wordCorrectRetObj.aborted)
					retObj.continueSpellCheck = false;
				else if ((wordCorrectRetObj.newWord.length > 0) && (wordCorrectRetObj.newWord != currentWord))
				{
					var firstLinePart = gEditLines[pEditLineIdx].text.substr(0, wordIdxInLine);
					var endLinePart = gEditLines[pEditLineIdx].text.substr(wordIdxInLine+currentWord.length);
					gEditLines[pEditLineIdx].setText(firstLinePart + wordCorrectRetObj.newWord + endLinePart);
					retObj.fixedWord = true;
				}
			}
		}
	}

	return retObj;
}
// Helper for spellCheckWordInLine(): Returns whether a string contains only word characters
// (letters
function strIsAllWordChars(pStr)
{
	if (typeof(pStr) !== "string" || pStr.length == 0)
		return false;

	if (gValidWordChars.length == 0)
		gValidWordChars = getStrOfWordChars();

	var onlyWordCharsFound = true;
	for (var i = 0; i < pStr.length && onlyWordCharsFound; ++i)
		onlyWordCharsFound = (gValidWordChars.indexOf(pStr.charAt(i)) > -1);
	return onlyWordCharsFound;
}
// Helper for spellCheckWordInLine(): Removes non-word characters from a string
function removeNonWordCharsFromStr(pStr)
{
	if (typeof(pStr) !== "string" || pStr.length == 0)
		return "";

	if (gValidWordChars.length == 0)
		gValidWordChars = getStrOfWordChars();

	var filteredStr = pStr;
	var continueOn = true;
	while (continueOn)
	{
		var startIdx = -1;
		var endIdx = -1;
		for (var i = 0; i < filteredStr.length; ++i)
		{
			if (gValidWordChars.indexOf(filteredStr.charAt(i)) == -1)
			{
				if (startIdx == -1)
					startIdx = i;
				else
					endIdx = i;
				break;
			}
		}
		if (startIdx > -1 && endIdx >= startIdx)
			filteredStr = filteredStr.substring(0, startIdx) + filteredStr.substring(endIdx+1);
		else if (startIdx > -1 && endIdx == -1)
			filteredStr = filteredStr.substring(0, startIdx) + filteredStr.substring(startIdx+1);
		else if (startIdx == -1 && endIdx == -1)
			continueOn = false;
	}
	return filteredStr;
}
// Builds & returns a string of valid word characters
function getStrOfWordChars()
{
	// First valid character is an apostrophe
	var validChars = "'";
	// a-z
	for (var i = 97; i <= 122; ++i)
		validChars += ascii(i);
	// A-Z
	for (var i = 65; i <= 90; ++i)
		validChars += ascii(i);
	var validCharsRegexStr = "^[a-zA-Z";
	var additionalASCIIVals = [129,154,130,131,132,133,134,135,136,137,138,139,140,141,142,143,144,147,148,149,150,151,152,153,160,161,162,163,164,165,225];
	for (var i = 0; i < additionalASCIIVals.length; ++i)
		validChars += ascii(additionalASCIIVals[i]);
	return validChars;
}
// Helper for doSpellCheck(): Displays a text area on the screen with a misspelled
// word in the title of the box and a text input to correct it
//
// Parameters:
//  pMisspelledWord: The misspelled word
//  pCurpos: The position of the cursor before calling this function
//  pEditLineIdx: The index of the current edit line
//
// Return value: An object containing the following properties:
//               newWord: The user's new word, or a blank string if they didn't
//                        enter a corrected word
//               aborted: Boolean - Whether or not the user wants to abort spell check
function inputWordCorrection(pMisspelledWord, pCurpos, pEditLineIdx)
{
	var retObj = {
		newWord: "",
		aborted: false
	};

	var originalCurpos = pCurpos;

	// If K_UTF8 exists, then SlyEdit supports UTF-8 strings
	var textMode = (g_K_UTF8Exists && gUserConsoleSupportsUTF8 ? P_UTF8 : P_NONE);

	// Create and display a text area with the misspelled word as the title
	// and get user input for the corrected word
	// For the 'text box', ensure the width is at most 80 characters
	var txtBoxWidth = 62; // Was 80
	var txtBoxHeight = 3;
	var txtBoxX = Math.floor(console.screen_columns / 2) - Math.floor(txtBoxWidth/2);
	var txtBoxY = Math.floor(console.screen_rows / 2) - 1;
	var maxInputLen = txtBoxWidth - 2;

	// Draw the top border of the input box
	var borderLine = "\x01n\x01g" + CP437_BOX_DRAWINGS_UPPER_LEFT_SINGLE + CP437_BOX_DRAWINGS_LIGHT_VERTICAL_AND_LEFT;
	borderLine += "\x01b\x01h" + pMisspelledWord.substr(0, txtBoxWidth-4);
	borderLine += "\x01n\x01g" + CP437_BOX_DRAWINGS_LIGHT_LEFT_T;
	var remainingWidth = txtBoxWidth - console.strlen(borderLine) - 1;
	for (var i = 0; i < remainingWidth; ++i)
		borderLine += CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE;
	borderLine += CP437_BOX_DRAWINGS_UPPER_RIGHT_SINGLE + "\x01n";
	console.gotoxy(txtBoxX, txtBoxY);
	console.print(borderLine);
	// Draw the bottom border of the input box
	borderLine = "\x01g" + CP437_BOX_DRAWINGS_LOWER_LEFT_SINGLE + CP437_BOX_DRAWINGS_LIGHT_VERTICAL_AND_LEFT;
	borderLine += "\x01c\x01hEnter\x01y=\x01bNo change\x01n\x01g" + CP437_BOX_DRAWINGS_LIGHT_LEFT_T + CP437_BOX_DRAWINGS_LIGHT_VERTICAL_AND_LEFT + "\x01H\x01cCtrl-C\x01n\x01c/\x01hESC\x01y=\x01bEnd\x01n\x01g" + CP437_BOX_DRAWINGS_LIGHT_LEFT_T;
	var remainingWidth = txtBoxWidth - console.strlen(borderLine) - 1;
	for (var i = 0; i < remainingWidth; ++i)
		borderLine += CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE;
	borderLine += CP437_BOX_DRAWINGS_LOWER_RIGHT_SINGLE + "\x01n";
	console.gotoxy(txtBoxX, txtBoxY+2);
	console.print(borderLine);
	// Draw the side borders
	console.print("\x01n\x01g");
	console.gotoxy(txtBoxX, txtBoxY+1);
	console.print(CP437_BOX_DRAWINGS_LIGHT_VERTICAL);
	console.gotoxy(txtBoxX+txtBoxWidth-1, txtBoxY+1);
	console.print(CP437_BOX_DRAWINGS_LIGHT_VERTICAL);
	console.attributes = "N";

	// Go to the middle row for user input
	var inputX = txtBoxX + 1;
	console.gotoxy(inputX, txtBoxY+1);
	printf("%" + maxInputLen + "s", "");
	console.gotoxy(inputX, txtBoxY+1);
	//retObj.newWord = console.getstr(maxInputLen, K_NOCRLF);
	var continueOn = true;
	while(continueOn)
	{
		var userInputChar = console.getkey(K_NOCRLF|K_NOSPIN);
		switch (userInputChar)
		{
			case CTRL_C:
			case KEY_ESC:
				retObj.newWord = "";
				retObj.aborted = true;
			case KEY_ENTER:
				continueOn = false;
				break;
			case BACKSPACE:
				if (retObj.newWord.length > 0)
				{
					retObj.newWord = retObj.newWord.substr(0, retObj.newWord.length - 1);
					console.gotoxy(inputX+retObj.newWord.length, txtBoxY+1);
					console.print(" ");
					console.gotoxy(inputX+retObj.newWord.length, txtBoxY+1);
				}
				break;
			default:
				// Append the character to the new word if the word is less than
				// the maximum input length
				if ((console.strlen(retObj.newWord, textMode) < maxInputLen) && isPrintableChar(userInputChar))
				{
					retObj.newWord += userInputChar;
					console.print(userInputChar);
				}
				break;
		}
	}

	// Erase the input area
	// Calculate the difference in the edit lines index we'll need to use
	// to erase the confirmation box.  This will be the difference between
	// the cursor position between boxY and the current cursor row.
	var editLinesIndexDiff = txtBoxY - originalCurpos.y;
	displayMessageRectangle(txtBoxX, txtBoxY, txtBoxWidth, txtBoxHeight, pEditLineIdx + editLinesIndexDiff, true);

	return retObj;
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
	var color = "\x01n";
	if (isQuoteLine(gEditLines, gEditLinesIndex))
		color = gQuoteLineColor;
	else
	{
		if (gConfigSettings.allowColorSelection)
			gTextAttrs = getAllEditLineAttrs(gEditLinesIndex, gTextLineIndex);
		if (gTextAttrs.length == 0)
			gTextAttrs = "\x01n";
		color = gTextAttrs;
	}
	return color;
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
//
// Parameters:
//  pCurpos: An object containg the X and Y coordinates of the cursor position
//  pMoveCursorBack: Boolean - Whether or not to move the cursor back after updating
//                   the time on the screen
function updateTime(pCurpos, pMoveCursorBack)
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
		var curpos = (typeof(pCurpos) == "object" ? pCurpos : console.getxy());
		// Display the current time on the screen
		fpDisplayTime(currentTime);
		// Make sure the edit color is correct
		console.print(chooseEditColor());
		// Move the cursor back to where it was
		if (pMoveCursorBack)
			console.gotoxy(curpos);
		// Update this function's time variable
		updateTime.timeStr = currentTime;
	}
}

// This function lets the user change the text color and is called by doEditLoop().
//
// Parameters:
//  pTxtAttrs: The current text color & attributes
//  pCurpos: An object containing x and y values representing the
//           cursor position.
//  pCurrentWordLength: The length of the current word that has been typed
//
// Return value: An object containing the following properties:
//               txtAttrs: The chosen text color & attributes
//               x and y: The horizontal and vertical cursor position
//               timedOut: Whether or not the user input timed out (boolean)
//               currentWordLength: The length of the current word
function doColorSelection(pTxtAttrs, pCurpos, pCurrentWordLength)
{
	// Create the return object
	var retObj = {
		txtAttrs: pTxtAttrs,
		x: pCurpos.x,
		y: pCurpos.y,
		timedOut: false,
		currentWordLength: pCurrentWordLength
	};

	var originalScreenY = pCurpos.y; // For screen refreshing

	// Display the 3 rows of color/attribute options and the prompt for the
	// user
	var colorSelTopLine = console.screen_rows - 2;
	var curpos = {
		x: 1,
		y: colorSelTopLine
	};
	console.gotoxy(curpos);
	console.print("\x01n\x01cForeground: \x01w\x01hK:\x01n\x01kBlack \x01w\x01hR:\x01n\x01rRed \x01w\x01hG:\x01n\x01gGreen \x01w\x01hY:\x01n\x01yYellow \x01w\x01hB:\x01n\x01bBlue \x01w\x01hM:\x01n\x01mMagenta \x01w\x01hC:\x01n\x01cCyan \x01w\x01hW:\x01n\x01wWhite");
	console.cleartoeol("\x01n");
	console.crlf();
	console.print("\x01n\x01cBackground: \x01w\x01h0:\x01n\x01" + "0Black\x01n \x01w\x01h1:\x01n\x01" + "1Red\x01n \x01w\x01h2:\x01n\x01" + "2\x01kGreen\x01n \x01w\x01h3:\x01" + "3Yellow\x01n \x01w\x01h4:\x01n\x01" + "4Blue\x01n \x01w\x01h5:\x01n\x01" + "5Magenta\x01n \x01w\x01h6:\x01n\x01" + "6\x01kCyan\x01n \x01w\x01h7:\x01n\x01" + "7\x01kWhite");
	console.cleartoeol("\x01n");
	console.crlf();
	console.clearline("\x01n");
	console.print("\x01cSpecial: \x01w\x01hH:\x01n\x01hHigh Intensity \x01wI:\x01n\x01iBlinking \x01n\x01w\x01hN:\x01nNormal \x01h\x01g" + CP437_BLACK_SQUARE + " \x01n\x01cChoose colors/attributes\x01h\x01g: \x01c");
	// Get the attribute codes from the user.  Ideally, we'd use console.getkeys(),
	// but that outputs a CR at the end, which is undesirable.  So instead, we call
	// getUserInputWithSetOfInputStrs (defined in slyedit_misc.js).
	//var key = console.getkeys("KRGYBMCW01234567HIN").toString(); // Outputs a CR..  bad
	var validKeys = ["KRGYBMCW", // Foreground color codes
	                 "01234567", // Background color codes
	                 "HIN"];     // Special color codes
	var attrCodeKeys = getUserInputWithSetOfInputStrs(K_UPPER|K_NOCRLF|K_NOSPIN, validKeys, gConfigSettings);
	// If the user entered some attributes, then set them in retObj.txtAttrs.
	if (attrCodeKeys.length > 0)
	{
		retObj.txtAttrs = (attrCodeKeys.charAt(0) == "N" ? "" : "\x01n");
		for (var i = 0; i < attrCodeKeys.length; ++i)
			retObj.txtAttrs += "\x01" + attrCodeKeys.charAt(i);
	}

	// Display the parts of the screen text that we covered up with the
	// color selection: Message edit lines, bottom border, and bottom help line.
	var screenYDiff = colorSelTopLine - originalScreenY;
	displayEditLines(colorSelTopLine, gEditLinesIndex + screenYDiff, gEditBottom, true, true);
	fpDisplayTextAreaBottomBorder(gEditBottom+1, gUseQuotes, gEditLeft, gEditRight,
	                              gInsertMode, gConfigSettings.allowColorSelection);
	fpDisplayBottomHelpLine(console.screen_rows, gUseQuotes, gUserSettings.ctrlQQuote);

	// Move the cursor to where it should be before returning
	curpos.x = pCurpos.x;
	curpos.y = pCurpos.y;
	console.gotoxy(curpos);

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
  console.print(pBorderColor + CP437_BOX_DRAWINGS_UPPER_LEFT_SINGLE + CP437_BOX_DRAWINGS_LIGHT_VERTICAL_AND_LEFT +
                pTextColor + "Cross-posting: Choose group" +
                pBorderColor + CP437_BOX_DRAWINGS_LIGHT_LEFT_T);
  var len = pWidth - 31;
  for (var i = 0; i < len; ++i)
    console.print(CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE);
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
function drawInitialCrossPostSelBoxBottomBorder(pBottomLeft, pWidth, pBorderColor, pMsgSubs)
{
	const maxWidth = pWidth - 2; // - 2 for the corner characters
	console.gotoxy(pBottomLeft);
	var border = CP437_BOX_DRAWINGS_LIGHT_VERTICAL_AND_LEFT + "\x01n\x01h\x01cUp\x01b/\x01cDn\x01b/\x01cPgUp\x01b/\x01cPgDn\x01b/\x01cHome\x01b/\x01cEnd \x01c";
	if (pMsgSubs)
		border += "Space\x01y=\x01bToggle \x01cEnter\x01y=\x01bConfirm";
	else
		border += "Enter\x01y=\x01bSelect";
	border += " \x01cCtrl-C\x01n\x01c/\x01hQ\x01y=\x01bAbort \x01c?";
	if (!pMsgSubs)
		border += "\x01y=\x01bHelp";
	border += "\x01n" + pBorderColor + CP437_BOX_DRAWINGS_LIGHT_LEFT_T;
	var numCharsToAdd = Math.floor(maxWidth / 2) - Math.floor(console.strlen(border)/2);
	for (var i = 0; i < numCharsToAdd; ++i)
		border = CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE + border;
	numCharsToAdd = maxWidth - console.strlen(border);
	for (var i = 0; i < numCharsToAdd; ++i)
		border += CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE;
	border = pBorderColor + CP437_BOX_DRAWINGS_LOWER_LEFT_SINGLE + border + CP437_BOX_DRAWINGS_LOWER_RIGHT_SINGLE;
	console.print(border);
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
      displayCrossPostHelp.helpLines = [];
      displayCrossPostHelp.helpLines.push("\x01n\x01cCross-posing allows you to post a message in more than one message");
      displayCrossPostHelp.helpLines.push("area.  To select areas for cross-posting, do the following:");
      displayCrossPostHelp.helpLines.push(" \x01h1. \x01n\x01cChoose a message group from the list with the Enter key.");
      displayCrossPostHelp.helpLines.push("    Alternately, you may type the number of the message group.");
      displayCrossPostHelp.helpLines.push(" \x01h2. \x01n\x01cIn the list of message sub-boards that appears, toggle individual");
      displayCrossPostHelp.helpLines.push("    sub-boards with the Enter key.  Alternately, you may type the");
      displayCrossPostHelp.helpLines.push("    number of the message sub-board.");
      displayCrossPostHelp.helpLines.push("Message sub-boards that are toggled for cross-posting will include a");
      displayCrossPostHelp.helpLines.push("check mark (" + gConfigSettings.genColors.crossPostChk + CP437_CHECK_MARK + "\x01n\x01c) in the sub-board list.  Initially, your current message");
      displayCrossPostHelp.helpLines.push("sub-board is enabled by default.  Also, your current message group is");
      displayCrossPostHelp.helpLines.push("marked with an asterisk (" + gConfigSettings.genColors.crossPostMsgGrpMark + "*\x01n\x01c).");
      displayCrossPostHelp.helpLines.push("To navigate the list, you may use the up & down arrow keys, PageUp and");
      displayCrossPostHelp.helpLines.push("PageDown to go to the previous & next page, Home to go to the first");
      displayCrossPostHelp.helpLines.push("page, and End to go to the last page. To confirm sub-board selection,");
	  displayCrossPostHelp.helpLines.push("press Enter. To abort: Ctrl-C, Q, or ESC.");
   }

   // Display the help text
   var selBoxInnerWidth = selBoxLowerRight.x - selBoxUpperLeft.x - 1;
   var selBoxInnerHeight = selBoxLowerRight.y - selBoxUpperLeft.y - 1;
   var lineLen = 0;
   var screenRow = selBoxUpperLeft.y+1;
   console.attributes = "N";
   for (var i = 0; (i < displayCrossPostHelp.helpLines.length) && (screenRow < selBoxLowerRight.y); ++i)
   {
      console.gotoxy(selBoxUpperLeft.x+1, screenRow++);
      console.print(displayCrossPostHelp.helpLines[i]);
      // If the text line is shorter than the inner width of the box, then
      // blank the rest of the line.
      lineLen = console.strlen(displayCrossPostHelp.helpLines[i]);
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
      console.attributes = "N";
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
		console.print("\x01n" + gConfigSettings.genColors.listBoxBorderText + "Choose group");
		// Re-write the border characters to overwrite the message group name
		grpDesc = msg_area.grp_list[pGrpIndex].description.substr(0, pSelBoxInnerWidth-25);
		// Write the updated border character(s)
		console.print("\x01n" + gConfigSettings.genColors.listBoxBorder + CP437_BOX_DRAWINGS_LIGHT_LEFT_T);
		if (grpDesc.length > 3)
		{
			var numChars = grpDesc.length - 3;
			for (var i = 0; i < numChars; ++i)
				console.print(CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE);
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
	var selBoxUpperLeft = {
		x: gEditLeft + 3,
		y: gEditTop + 1
	};
	var selBoxLowerRight = {
		x: gEditRight - 3,
		y: gEditBottom - 1
	};
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
	gConfigSettings.genColors.listBoxBorder,
	gConfigSettings.genColors.listBoxBorderText);
	// Side borders
	console.print(CP437_BOX_DRAWINGS_UPPER_RIGHT_SINGLE);
	for (var row = selBoxUpperLeft.y+1; row < selBoxLowerRight.y; ++row)
	{
		console.gotoxy(selBoxUpperLeft.x, row);
		console.print(CP437_BOX_DRAWINGS_LIGHT_VERTICAL);
		console.gotoxy(selBoxLowerRight.x, row);
		console.print(CP437_BOX_DRAWINGS_LIGHT_VERTICAL);
	}
	// Bottom border
	drawInitialCrossPostSelBoxBottomBorder({ x: selBoxUpperLeft.x, y: selBoxLowerRight.y },
	                                       selBoxWidth, gConfigSettings.genColors.listBoxBorder,
	                                       false);

	// Input loop - Prompting for sub-boards
	// Note: promptUserForCrossPostSubBoardCodes() will add the user's
	// selected sub-board codes to gCrossPostMsgSubs.
	var promptRetObj = promptUserForCrossPostSubBoardCodes(selBoxUpperLeft, selBoxLowerRight);

	// We're done selecting message areas for cross-posting.
	// Erase the message area selection rectangle by re-drawing the message text.
	// Then, move the cursor back to where it was when we started the message
	// area selection.
	displayMessageRectangle(selBoxUpperLeft.x, selBoxUpperLeft.y, selBoxWidth,
	                        selBoxHeight, editLineIndexAtSelBoxTopRow, true);
	console.gotoxy(origStartingCurpos);
}
// Performs the input loop using DDLightbarMenu objects to get sub-board codes
// for cross-posting from the user.
//
// Parameters:
//  pSelBoxUpperLeft: An object containing x & y coordinates for the upper-left of the selection box with borders
//  pSelBoxLowerRight: An object containing x & y coordinates for the lower-right of the selection box with borders
//
// Return value: An object containing the following properties:
//               lastUserInput: The last input from the user
function promptUserForCrossPostSubBoardCodes(pSelBoxUpperLeft, pSelBoxLowerRight)
{
	var retObj = {
		lastUserInput: ""
	};

	// Set up an object containing the colors to use for the items in the group/sub-board lists
	var menuListColors = {
		areaMark: "\x01g\x01h",
		areaNum: "\x01n\x01w\x01h",
		desc: "\x01n\x01c",
		areaNumHighlight: "\x01w\x01h",
		descHighlight: "\x01c",
		bkgHighlight: "\x01" + "4"
	};
	
	// Calculate the selection box width & height, with borders
	var selBoxWidth = pSelBoxLowerRight.x - pSelBoxUpperLeft.x + 1;
	var selBoxHeight = pSelBoxLowerRight.y - pSelBoxUpperLeft.y + 1;
	// Don't let the box's height be more than 17 characters.
	if (selBoxHeight > 17)
	{
	  pSelBoxLowerRight.y = pSelBoxUpperLeft.y + 16; // For a height of 17 characters
	  selBoxHeight = pSelBoxLowerRight.y - pSelBoxUpperLeft.y + 1;
	}
	// Inner size of the box (for the text menu items)
	var selBoxInnerWidth = selBoxWidth - 2;
	var selBoxInnerHeight = selBoxHeight - 2;

	// The starting column & row for the text items in the menu
	var listStartCol = pSelBoxUpperLeft.x+1;
	var listStartRow = pSelBoxUpperLeft.y+1;

	// The number of milliseconds to pause after displaying the error message
	// that the user isn't allowed to post in a sub-board (due to ARS).
	var cantPostErrPauseMS = 2000;

	// Create & display the message group menu for the user to choose from
	var msgGrpMenu = createCrossPostMsgGrpMenu(listStartCol, listStartRow, selBoxInnerWidth, selBoxInnerHeight, menuListColors);
	var continueOn = true;
	while (continueOn)
	{
		// Show the message group menu and get the chosen item from the user
		var grpIdx = msgGrpMenu.GetVal();
		retObj.lastUserInput = msgGrpMenu.lastUserInput;
		if (msgGrpMenu.lastUserInput == "?")
		{
			displayCrossPostHelp(pSelBoxUpperLeft, pSelBoxLowerRight);
			console.gotoxy(listStartCol, listStartRow);
			consolePauseWithoutText();
		}
		else if (msgGrpMenu.lastUserInput == "Q" || msgGrpMenu.lastUserInput == CTRL_C)
			continueOn = false;
		else if (typeof(grpIdx) == "number")
		{
			// Change the bottom box text to say Space=Toggle (instead of Enter=Select)
			drawInitialCrossPostSelBoxBottomBorder({ x: pSelBoxUpperLeft.x, y: pSelBoxLowerRight.y },
													selBoxWidth, gConfigSettings.genColors.listBoxBorder,
													true);
			// Get an object of selected sub-board indexes that we can pass to
			// the sub-board menu.
			var selectedItemIndexes = {};
			for (var subIdx = 0; subIdx < msg_area.grp_list[grpIdx].sub_list.length; ++subIdx)
			{
				if (gCrossPostMsgSubs.subCodeExists(msg_area.grp_list[grpIdx].sub_list[subIdx].code))
					selectedItemIndexes[subIdx] = true;
			}
			

			// Create the sub-board menu and let the user make a selection.
			var subBoardMenu = createCrossPostSubBoardMenu(grpIdx, listStartCol, listStartRow, selBoxInnerWidth, selBoxInnerHeight, menuListColors);
			// Get the selection of sub-boards from the user
			var selectedSubBoards = subBoardMenu.GetVal(true, selectedItemIndexes);
			retObj.lastUserInput = subBoardMenu.lastUserInput;
			if (subBoardMenu.lastUserInput == "?")
			{
				displayCrossPostHelp(pSelBoxUpperLeft, pSelBoxLowerRight);
				console.gotoxy(listStartCol, listStartRow);
				consolePauseWithoutText();
			}
			else if (subBoardMenu.lastUserInput == "Q" || subBoardMenu.lastUserInput == CTRL_C)
			{
				// Do nothing
			}
			// If the user selected some sub-boards, then add them to gCrossPostMsgSubs
			if (selectedSubBoards != null && selectedSubBoards.length > 0)
			{
				// We want to ensure we only use the sub-boards the user selected.  In case the
				// user has gone into the same menu multiple times and selected and un-selected
				// some, this loop goes through all sub-boards in the current message group and
				// removes them from the selection, then adds the ones the user just selected.
				for (var subIdx in msg_area.grp_list[grpIdx].sub_list)
					gCrossPostMsgSubs.remove(msg_area.grp_list[grpIdx].sub_list[subIdx].code);
				for (var idx in selectedSubBoards)
					gCrossPostMsgSubs.add(selectedSubBoards[idx]);
			}
			// Change the bottom box text back to say Enter=Select (instead of Space=Toggle)
			drawInitialCrossPostSelBoxBottomBorder({ x: pSelBoxUpperLeft.x, y: pSelBoxLowerRight.y },
													selBoxWidth, gConfigSettings.genColors.listBoxBorder,
													false);
		}
		else
			continueOn = false;
	}

	return retObj;
}
// Creates the DDLightbarMenu for use with choosing a message group for
// cross-posting.  Does not use borders, so that the border display can be
// customized by the caller.
//
// Parameters:
//  pListStartCol: The column on the screen where the list thould start
//  pListStartRow: The row on the screen where the list should start
//       pListStartRow should be this.areaChangeHdrLines.length + 2
//  pListWidth: The width of the list on the screen (without borders; effectively the item text width)
//  pListHeight: The width of the list on the screen (without borders)
//  pColorObj: Optional - An object containing color/attribute codes for the various parts of
//             the message group items in the menu.  Needs the following properties:
//             areaMark: For the selected group
//             areaNum: For the group number
//             desc: For the group description
//             areaNumHighlight: For the group number for the selected item
//             descHighlight: For the group descripton for the selected item
//             bkgHighlight: The background color/attribute for the selected item
//
// Return value: A DDLightbarMenu object for choosing a message group
function createCrossPostMsgGrpMenu(pListStartCol, pListStartRow, pListWidth, pListHeight, pColorObj)
{
	var numGrpsLength = msg_area.grp_list.length.toString().length;

	var msgGrpMenu = new DDLightbarMenu(pListStartCol, pListStartRow, pListWidth, pListHeight);
	msgGrpMenu.scrollbarEnabled = true;
	msgGrpMenu.borderEnabled = false;
	msgGrpMenu.multiSelect = false;
	msgGrpMenu.ampersandHotkeysInItems = false;
	msgGrpMenu.wrapNavigation = false;
	// Add additional keypresses for quitting the menu's input loop so we can
	// respond to these keys
	// ? = Help
	// Q: Exit
	// Ctrl-C: Exit
	msgGrpMenu.AddAdditionalQuitKeys("?Qq" + CTRL_C);

	// Description length (for the color indexes & printf string)
	var descLen = pListWidth - numGrpsLength - 2; // -2 for the possible * and the space
	// If we actually need a scrollbar, then decrease descLen by 1 more
	if (msg_area.grp_list.length > pListHeight)
		--descLen;
	// Colors
	if (pColorObj != null)
	{
		// Start & end indexes for the various items in each mssage group list row
		// Selected mark, group#, description, # sub-boards
		var msgGrpListIdxes = {
			markCharStart: 0,
			markCharEnd: 1,
			grpNumStart: 1,
			grpNumEnd: 2 + (+numGrpsLength)
		};
		msgGrpListIdxes.descStart = msgGrpListIdxes.grpNumEnd;
		msgGrpListIdxes.descEnd = msgGrpListIdxes.descStart + descLen;

		msgGrpMenu.SetColors({
			itemColor: [{start: msgGrpListIdxes.markCharStart, end: msgGrpListIdxes.markCharEnd, attrs: pColorObj.areaMark},
						{start: msgGrpListIdxes.grpNumStart, end: msgGrpListIdxes.grpNumEnd, attrs: pColorObj.areaNum},
						{start: msgGrpListIdxes.descStart, end: msgGrpListIdxes.descEnd, attrs: pColorObj.desc}],
			selectedItemColor: [{start: msgGrpListIdxes.markCharStart, end: msgGrpListIdxes.markCharEnd, attrs: pColorObj.areaMark + pColorObj.bkgHighlight},
								{start: msgGrpListIdxes.grpNumStart, end: msgGrpListIdxes.grpNumEnd, attrs: pColorObj.areaNumHighlight + pColorObj.bkgHighlight},
								{start: msgGrpListIdxes.descStart, end: msgGrpListIdxes.descEnd, attrs: pColorObj.descHighlight + pColorObj.bkgHighlight}]
		});
	}

	// Generate a printf string for the message group items
	// *[# ] Description
	var printfStr = "%s%" + +numGrpsLength + "d %-" + +descLen + "s";

	// Change the menu's NumItems() and GetItem() function to reference
	// the message list in this object rather than add the menu items
	// to the menu
	msgGrpMenu.NumItems = function() {
		return msg_area.grp_list.length;
	};
	msgGrpMenu.GetItem = function(pGrpIndex) {
		var menuItemObj = this.MakeItemWithRetval(-1);
		if ((pGrpIndex >= 0) && (pGrpIndex < msg_area.grp_list.length))
		{
			var markColText = (((typeof(bbs.curgrp) == "number") && (pGrpIndex == msg_area.sub[bbs.cursub_code].grp_index)) ? "*" : " ");
			var grpDesc = msg_area.grp_list[pGrpIndex].description.substr(0, descLen);
			menuItemObj.text = format(printfStr, markColText, +(pGrpIndex+1), grpDesc);
			menuItemObj.text = strip_ctrl(menuItemObj.text);
			menuItemObj.retval = pGrpIndex;
		}

		return menuItemObj;
	};

	// Set the currently selected item to the current group
	msgGrpMenu.SetSelectedItemIdx(msg_area.sub[bbs.cursub_code].grp_index);

	return msgGrpMenu;
}
// Creates the DDLightbarMenu for use with choosing a sub-board for
// cross-posting
//
// Parameters:
//  pGrpIdx: The index of the message group
//  pListStartCol: The column on the screen where the list thould start
//  pListStartRow: The row on the screen where the list should start
//     pListStartRow should be this.areaChangeHdrLines.length + 2
//  pListWidth: The width of the list on the screen (without borders; effectively the item text width)
//  pListHeight: The width of the list on the screen (without borders)
//  pColorObj: Optional - An object containing color/attribute codes for the various parts of
//             the message sub-board items in the menu.  Needs the following properties:
//             areaMark: For the selected sub-board
//             areaNum: For the sub-board number
//             desc: For the sub-board description
//             areaNumHighlight: For the sub-board number for the selected item
//             descHighlight: For the sub-board descripton for the selected item
//             bkgHighlight: The background color/attribute for the selected item
//
// Return value: A DDLightbarMenu object for choosing a sub-board within
// the given message group
function createCrossPostSubBoardMenu(pGrpIdx, pListStartCol, pListStartRow, pListWidth, pListHeight, pColorObj)
{
	var numSubsLength = msg_area.grp_list[pGrpIdx].sub_list.length.toString().length;

	var subBoardMenu = new DDLightbarMenu(pListStartCol, pListStartRow, pListWidth, pListHeight);
	subBoardMenu.scrollbarEnabled = true;
	subBoardMenu.borderEnabled = false;
	subBoardMenu.multiSelect = true;
	subBoardMenu.enterAndSelectKeysAddsMultiSelectItem = false; // Ensure Enter doesn't add to multi-select items
	subBoardMenu.ampersandHotkeysInItems = false;
	subBoardMenu.wrapNavigation = false;
	// Add additional keypresses for quitting the menu's input loop so we can
	// respond to these keys
	// ? = Help
	// Q and Ctrl-C: Quit
	subBoardMenu.AddAdditionalQuitKeys("?qQ" + CTRL_C);

	// Description length (for the color indexes & printf string)
	var descLen = pListWidth - numSubsLength - 2; // -2 for the possible * and the space
	// If we actually need a scrollbar, then decrease descLen by 1 more
	if (msg_area.grp_list.length > pListHeight)
		--descLen;
	// Colors
	if (pColorObj != null)
	{
		// Start & end indexes for the various items in each mssage group list row
		// Selected mark, group#, description, # sub-boards
		var subBoardListIdxes = {
			markCharStart: 0,
			markCharEnd: 1,
			subNumStart: 1,
			subNumEnd: 2 + numSubsLength
		};
		subBoardListIdxes.descStart = subBoardListIdxes.subNumEnd;
		subBoardListIdxes.descEnd = subBoardListIdxes.descStart + descLen;

		subBoardMenu.SetColors({
			itemColor: [{start: subBoardListIdxes.markCharStart, end: subBoardListIdxes.markCharEnd, attrs: pColorObj.areaMark},
						{start: subBoardListIdxes.subNumStart, end: subBoardListIdxes.subNumEnd, attrs: pColorObj.areaNum},
						{start: subBoardListIdxes.descStart, end: subBoardListIdxes.descEnd, attrs: pColorObj.desc}],
			selectedItemColor: [{start: subBoardListIdxes.markCharStart, end: subBoardListIdxes.markCharEnd, attrs: pColorObj.areaMark + pColorObj.bkgHighlight},
								{start: subBoardListIdxes.subNumStart, end: subBoardListIdxes.subNumEnd, attrs: pColorObj.areaNumHighlight + pColorObj.bkgHighlight},
								{start: subBoardListIdxes.descStart, end: subBoardListIdxes.descEnd, attrs: pColorObj.descHighlight + pColorObj.bkgHighlight}]
		});
	}

	// Generate a printf string for the sub-boards
	// *[# ] Description
	var printfStr = "%s%" + +numSubsLength + "d %-" + +descLen + "s";

	// Change the menu's NumItems() and GetItem() function to reference
	// the message list in this object rather than add the menu items
	// to the menu
	subBoardMenu.grpIdx = pGrpIdx;
	subBoardMenu.NumItems = function() {
		return msg_area.grp_list[this.grpIdx].sub_list.length;
	};
	subBoardMenu.GetItem = function(pSubIdx) {
		var menuItemObj = this.MakeItemWithRetval(-1);
		if ((pSubIdx >= 0) && (pSubIdx < msg_area.grp_list[this.grpIdx].sub_list.length))
		{
			var showSubBoardMark = false;
			if ((typeof(bbs.cursub_code) == "string") && (bbs.cursub_code != ""))
				showSubBoardMark = ((this.grpIdx == msg_area.sub[bbs.cursub_code].grp_index) && (pSubIdx == msg_area.sub[bbs.cursub_code].index));
			var markColText = showSubBoardMark ? "*" : " ";
			var subDesc = msg_area.grp_list[this.grpIdx].sub_list[pSubIdx].description.substr(0, descLen);
			menuItemObj.text = format(printfStr, markColText, +(pSubIdx+1), subDesc);
			//menuItemObj.retval = pSubIdx;
			// Have the selected item be the sub-board code
			menuItemObj.retval = msg_area.grp_list[this.grpIdx].sub_list[pSubIdx].code;
		}

		return menuItemObj;
	};

	// Set the currently selected item.  If the current sub-board is in this list,
	// then set the selected item to that; otherwise, the selected item should be
	// the first sub-board.
	if (msg_area.sub[bbs.cursub_code].grp_index == pGrpIdx)
		subBoardMenu.SetSelectedItemIdx(msg_area.sub[bbs.cursub_code].index);
	else
		subBoardMenu.SetSelectedItemIdx(0); // This should also set its topItemIdx to 0

	// Sub-board selection validation function
	subBoardMenu.ValidateSelectItem = function(pSubCode) {
		if (msg_area.sub[pSubCode].can_post)
			return true;
		else
		{
			// Go to the bottom row of the selection box and display an error that
			// the user can't post in the selected sub-board and pause for a moment.
			writeCantPostErrMsg(pListStartCol-1, pListStartRow+pListHeight, subBoardMenu.selectedItemIdx+1,
								pListWidth+2, pListWidth, 2000/*cantPostErrPauseMS*/, console.getxy());
		}
	};

	return subBoardMenu;
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
	printf("\x01n\x01h\x01y%-" + pSelBoxInnerWidth + "s", cantPostErrMsg);
	console.gotoxy(pX+cantPostErrMsg.length+1, pY);
	mswait(pPauseMS);
	// Refresh the bottom border of the selection box
	drawInitialCrossPostSelBoxBottomBorder({ x: pX, y: pY }, pSelBoxWidth, gConfigSettings.genColors.listBoxBorder, true);
	console.gotoxy(pCurpos);
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
	var useColors = (typeof(pUseColors) == "boolean" ? pUseColors : true);
	var start = (typeof(pStart) == "number" ? pStart : 0);
	var length = (typeof(pLength) == "number" ? pLength : -1);
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
		// Before returning, write spaces for the length specified so that the
		// screen is updated correctly.  Also, output a normal attribute first
		// to ensure there isn't a background color for the spaces.
		console.attributes = "N";
		for (var i = 0; i < length; ++i)
			console.print(" ");
		printf(gTextAttrs);
		return length;
	}
	//if (length > (gEditLines[pIndex].text.length - start))
	//	length = gEditLines[pIndex].text.length - start;

	var lengthWritten = 0;
	if (useColors)
	{
		var lineLengthToGet = (length > -1 ? length : gEditLines[pIndex].screenLength()-start);
		var lineText = "";
		if (gEditLines[pIndex].isAQuoteLine())
		{
			// The line is a quote line. Print the line with the configured quote color.
			lineText = "\x01n" + gQuoteLineColor + gEditLines[pIndex].text.substr(start, lineLengthToGet);
		}
		else
		{
			// The line isn't a quote line
			// Note: substrWithAttrCodes() is defined in dd_lightbar_menu.js
			//var lineText = substrWithAttrCodes(gEditLines[pIndex].getText(true), start, lineLengthToGet);
			// The line's substr() will include the necessary attribute codes
			//lineText = gEditLines[pIndex].substr(true, start, lineLengthToGet);
			lineText = substrWithAttrCodes(gEditLines[pIndex].getText(true), start, lineLengthToGet);
		}
		lengthWritten = console.strlen(lineText, str_is_utf8(strip_ctrl(lineText)) ? P_UTF8 : P_NONE);
		printStrConsideringUTF8(lineText, gPrintMode);
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
				gEditLines[pIndex].print(false);
			else
			{
				var textToWrite = gEditLines[pIndex].text.substr(start, length);
				printStrConsideringUTF8(textToWrite, gPrintMode);
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
			printStrConsideringUTF8(textToWrite, gPrintMode);
			lengthWritten = console.strlen(textToWrite, str_is_ascii(textToWrite) ? P_NONE :P_UTF8);
		}
	}
	return lengthWritten;
}

// Lists the text replacements configured in SlyEdit using a scrollable list box.
function listTextReplacements()
{
	if (gNumTxtReplacements == 0)
	{
		var originalCurpos = console.getxy();
		writeMsgOntBtmHelpLineWithPause("\x01n\x01h\x01yThere are no text replacements.", ERRORMSG_PAUSE_MS);
		console.print(chooseEditColor()); // Make sure the edit color is correct
		console.gotoxy(originalCurpos);
		return;
	}

	// Calculate the text width for each column, which will then be used to
	// calculate the width of the box.  For the width of the box, we need to
	// subtract at least 3 from the edit area with to accomodate the box's side
	// borders and the space between the text columns.
	var txtWidth = Math.floor((gEditWidth - 10)/2);

	// In order to be able to navigate forward and backwards through the text
	// replacements, we need to copy them into an array, since gTxtReplacements
	// is an object and not navigable both ways.  This will also allow us to easily
	// know how many text replacements there are (using the .length property of
	// the array).
	// For speed, create this only once.
	if (typeof(listTextReplacements.txtReplacementArr) == "undefined")
	{
		listTextReplacements.txtReplacementArr = [];
		for (var prop in gTxtReplacements)
		{
			var txtReplacementObj = {
				originalText: prop,
				replacement: gTxtReplacements[prop]
			};
			listTextReplacements.txtReplacementArr.push(txtReplacementObj);
		}
	}

	// We'll want to have an object with the box dimensions.
	var boxInfo = {};

	// Construct the top & bottom border strings if they don't exist already.
	if (typeof(listTextReplacements.topBorder) == "undefined")
	{
		listTextReplacements.topBorder = "\x01n" + gConfigSettings.genColors.listBoxBorder
		                               + CP437_BOX_DRAWINGS_UPPER_LEFT_SINGLE + "\x01n" + gConfigSettings.genColors.listBoxBorderText + "Text"
		                               + "\x01n" + gConfigSettings.genColors.listBoxBorder;
		for (var i = 0; i < (txtWidth-3); ++i)
			listTextReplacements.topBorder += CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE;
		listTextReplacements.topBorder += "\x01n" + gConfigSettings.genColors.listBoxBorderText
		                               + "Replacement" + "\x01n" + gConfigSettings.genColors.listBoxBorder;
		for (var i = 0; i < (txtWidth-11); ++i)
			listTextReplacements.topBorder += CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE;
		listTextReplacements.topBorder += CP437_BOX_DRAWINGS_UPPER_RIGHT_SINGLE;
	}
	boxInfo.width = console.strlen(listTextReplacements.topBorder);
	if (typeof(listTextReplacements.bottomBorder) == "undefined")
	{
		var numReplacementsStr = "Total: " + listTextReplacements.txtReplacementArr.length;
		listTextReplacements.bottomBorder = "\x01n" + gConfigSettings.genColors.listBoxBorder
		                                  + CP437_BOX_DRAWINGS_LOWER_LEFT_SINGLE + "\x01n" + gConfigSettings.genColors.listBoxBorderText
		                                  + UP_ARROW + ", " + DOWN_ARROW + ", ESC/Ctrl-T/C=Close" + "\x01n"
		                                  + gConfigSettings.genColors.listBoxBorder;
		var maxNumChars = boxInfo.width - numReplacementsStr.length - 28;
		for (var i = 0; i < maxNumChars; ++i)
			listTextReplacements.bottomBorder += CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE;
		listTextReplacements.bottomBorder += CP437_BOX_DRAWINGS_LIGHT_VERTICAL_AND_LEFT + "\x01n"
		                                  + gConfigSettings.genColors.listBoxBorderText + numReplacementsStr + "\x01n"
		                                  + gConfigSettings.genColors.listBoxBorder + CP437_BOX_DRAWINGS_LIGHT_LEFT_T;
		listTextReplacements.bottomBorder += CP437_BOX_DRAWINGS_LOWER_RIGHT_SINGLE;
	}
	// printf format strings for the list
	if (typeof(listTextReplacements.listFormatStr) == "undefined")
	{
		listTextReplacements.listFormatStr = "\x01n" + gConfigSettings.genColors.listBoxItemText
		                                   + "%-" + txtWidth + "s %-" + txtWidth + "s";
	}
	if (typeof(listTextReplacements.listFormatStrNormalAttr) == "undefined")
		listTextReplacements.listFormatStrNormalAttr = "\x01n%-" + txtWidth + "s %-" + txtWidth + "s";

	// Limit the box height to up to 12 lines.
	boxInfo.height = gNumTxtReplacements + 2;
	if (boxInfo.height > 12)
		boxInfo.height = 12;
	boxInfo.topLeftX = gEditLeft + Math.floor((gEditWidth/2) - (boxInfo.width/2));
	boxInfo.topLeftY = gEditTop + Math.floor((gEditHeight/2) - (boxInfo.height/2));

	// Draw the top & bottom box borders for the list of text replacements
	var originalCurpos = console.getxy();
	console.gotoxy(boxInfo.topLeftX, boxInfo.topLeftY);
	console.print(listTextReplacements.topBorder);
	console.gotoxy(boxInfo.topLeftX, boxInfo.topLeftY+boxInfo.height-1);
	console.print(listTextReplacements.bottomBorder);
	// Draw the side borders
	console.print("\x01n" + gConfigSettings.genColors.listBoxBorder);
	for (var i = 0; i < boxInfo.height-2; ++i)
	{
		console.gotoxy(boxInfo.topLeftX, boxInfo.topLeftY+i+1);
		console.print(CP437_BOX_DRAWINGS_LIGHT_VERTICAL);
		console.gotoxy(boxInfo.topLeftX+boxInfo.width-1, boxInfo.topLeftY+i+1);
		console.print(CP437_BOX_DRAWINGS_LIGHT_VERTICAL);
	}

	// Set up some variables for the user input loop
	const numItemsPerPage = boxInfo.height - 2;
	const numPages = Math.ceil(listTextReplacements.txtReplacementArr.length / numItemsPerPage);
	// For the horizontal location of the page number text for the box border:
	// Based on the fact that there can be up to 9999 text replacements and 10
	// per page, there will be up to 1000 pages of replacements.  To write the
	// text, we'll want to be 20 characters to the left of the end of the border
	// of the box.
	const pageNumTxtStartX = boxInfo.topLeftX + boxInfo.width - 20;
	var pageNum = 0;
	var startArrIndex = 0;
	var endArrIndex = 0; // One past the last array item
	var screenY = 0;
	// User input loop (also drawing the list of items)
	var continueOn = true;
	var refreshList = true; // For screen redraw optimizations
	while (continueOn)
	{
		if (refreshList)
		{
			// Write the list of items for the current page
			startArrIndex = pageNum * numItemsPerPage;
			endArrIndex = startArrIndex + numItemsPerPage;
			if (endArrIndex > listTextReplacements.txtReplacementArr.length)
				endArrIndex = listTextReplacements.txtReplacementArr.length;
			screenY = boxInfo.topLeftY + 1;
			for (var i = startArrIndex; i < endArrIndex; ++i)
			{
				console.gotoxy(boxInfo.topLeftX+1, screenY);
				printf(listTextReplacements.listFormatStr,
				       listTextReplacements.txtReplacementArr[i].originalText.substr(0, txtWidth),
				       listTextReplacements.txtReplacementArr[i].replacement.substr(0, txtWidth));
				++screenY;
			}
			// If the current screen row is below the bottom row inside the box,
			// continue and write blank lines to the bottom of the inside of the box
			// to blank out any text that might still be there.
			while (screenY < boxInfo.topLeftY+boxInfo.height-1)
			{
				console.gotoxy(boxInfo.topLeftX+1, screenY);
				printf(listTextReplacements.listFormatStrNormalAttr, "", "");
				++screenY;
			}

			// Update the page number in the top border of the box.
			console.gotoxy(pageNumTxtStartX, boxInfo.topLeftY);
			console.print("\x01n" + gConfigSettings.genColors.listBoxBorder + CP437_BOX_DRAWINGS_LIGHT_VERTICAL_AND_LEFT);
			printf("\x01n" + gConfigSettings.genColors.listBoxBorderText + "Page %4d of %4d", pageNum+1, numPages);
			console.print("\x01n" + gConfigSettings.genColors.listBoxBorder + CP437_BOX_DRAWINGS_LIGHT_LEFT_T);

			// Just for sane appearance: Move the cursor to the first character of
			// the first row and make it the color for the text replacements.
			console.gotoxy(boxInfo.topLeftX+1, boxInfo.topLeftY+1);
			console.print(gConfigSettings.genColors.listBoxItemText);
		}

		// Get a key from the user (upper-case) and take action based upon it.
		var userInput = getUserKey(K_UPPER|K_NOCRLF|K_NOSPIN);
		switch (userInput)
		{
			case KEY_UP:
				// Go up one page
				refreshList = (pageNum > 0);
				if (refreshList)
					--pageNum;
				break;
			case KEY_DOWN:
				// Go down one page
				refreshList = (pageNum < numPages-1);
				if (refreshList)
					++pageNum;
				break;
				// Quit for ESC, Ctrl-T, Ctrl-A, and 'C' (close).
			case KEY_ESC:
			case CTRL_T:
			case CTRL_A:
			case 'C':
				refreshList = false;
				continueOn = false;
				break;
			default:
				// Unrecognized command.  Don't refresh the list of the screen.
				refreshList = false;
				break;
		}
	}

	// We're done listing the text replacements.
	// Erase the list box rectangle by re-drawing the message text.  Then, move
	// the cursor back to where it was originally.
	var editLineIndexAtSelBoxTopRow = gEditLinesIndex - (originalCurpos.y-boxInfo.topLeftY);
	displayMessageRectangle(boxInfo.topLeftX, boxInfo.topLeftY, boxInfo.width,
	                        boxInfo.height, editLineIndexAtSelBoxTopRow, true);
	console.gotoxy(originalCurpos);
	console.print(chooseEditColor());
}

// Lets the user manage their preferences/settings.
//
// Parameters:
//  pCurpos: The position of the cursor on the screen when this function is called.
//           This is optional.  If not specified, this function will call console.getxy()
//           to get the cursor position.
//  pReturnCursorToOriginalPos: Optional, boolean - Whether or not to return the cursor
//                              to its original position when done.
function doUserSettings(pCurpos, pReturnCursorToOriginalPos)
{
	var retObj = {
		ctrlQQuoteOptChanged: false
	};

	if (!gConfigSettings.allowUserSettings)
		return retObj;

	const originalCurpos = (typeof(pCurpos) == "object" ? pCurpos : console.getxy());
	var returnCursorWhenDone = true;
	if (typeof(pReturnCursorToOriginalPos) == "boolean")
		returnCursorWhenDone = pReturnCursorToOriginalPos;

	// Save the user's current settings so that we can check them later to see if any
	// of them changed, in order to determine whether to save the user's settings file.
	var originalSettings = {};
	for (var prop in gUserSettings)
	{
		if (gUserSettings.hasOwnProperty(prop))
			originalSettings[prop] = gUserSettings[prop];
	}

	// Load the dictionary filenames - If there are more than 1, then we'll add
	// an option for the user to choose a dictionary.
	var dictionaryFilenames = getDictionaryFilenames(js.exec_dir);

	// Copy the SlyEdit configuration color settings to ChoiceScrollbox
	// color settings
	var choiceBoxSettings = {
		colors: {
			listBoxBorder: gConfigSettings.genColors.listBoxBorder,
			listBoxBorderText: gConfigSettings.genColors.listBoxBorderText,
			listBoxItemText: gConfigSettings.genColors.listBoxItemText,
			listBoxItemHighlight: gConfigSettings.genColors.listBoxItemHighlight
		}
	};

	// Create the user settings box
	var optBoxTitle = "Setting                                      Enabled";
	var optBoxWidth = ChoiceScrollbox_MinWidth();
	var optBoxHeight = (dictionaryFilenames.length > 1 ? 14 : 13);
	var optBoxStartX = gEditLeft + Math.floor((gEditWidth/2) - (optBoxWidth/2));
	if (optBoxStartX < gEditLeft)
		optBoxStartX = gEditLeft;
	var optionBox = new ChoiceScrollbox(optBoxStartX, gEditTop+1, optBoxWidth, optBoxHeight, optBoxTitle,
	                                    choiceBoxSettings, false, true);
	optionBox.addInputLoopExitKey(CTRL_U);
	optionBox.addInputLoopExitKey("?");
	// Update the bottom help text to be more specific to the user settings box
	/*
	var bottomBorderText = "\x01n\x01h\x01c"+ UP_ARROW + "\x01b, \x01c"+ DOWN_ARROW + "\x01b, \x01cEnter\x01y=\x01bSelect\x01n\x01c/\x01h\x01btoggle, "
	                      + "\x01cESC\x01n\x01c/\x01hQ\x01n\x01c/\x01hCtrl-U\x01y=\x01bClose";
	*/
	var bottomBorderText = "\x01n\x01h\x01cUp\x01b, \x01cDn\x01b, \x01cEnter\x01y=\x01bSelect\x01n\x01c/\x01h\x01btoggle, "
	                     + "\x01c?\x01y=\x01bHelp, "
	                     + "\x01cESC\x01n\x01c/\x01hQ\x01n\x01c/\x01hCtrl-U\x01y=\x01bClose";
	// This one contains the page navigation keys..  Don't really need to show those,
	// since the settings box only has one page right now:
	/*var bottomBorderText = "\x01n\x01h\x01c"+ UP_ARROW + "\x01b, \x01c"+ DOWN_ARROW + "\x01b, \x01cN\x01y)\x01bext, \x01cP\x01y)\x01brev, "
	                       + "\x01cF\x01y)\x01birst, \x01cL\x01y)\x01bast, \x01cEnter\x01y=\x01bSelect, "
	                       + "\x01cESC\x01n\x01c/\x01hQ\x01n\x01c/\x01hCtrl-U\x01y=\x01bClose";*/

	optionBox.setBottomBorderText(bottomBorderText, true, false, true);

	// Add the options to the option box
	const optFormatStr = "%-46s [ ]";
	const checkIdx = 48;
	const CTRL_Q_QUOTE_OPT_INDEX = optionBox.addTextItem(format(optFormatStr, "Ctrl-Q to quote (if not, then Ctrl-Y)"));
	const TAGLINE_OPT_INDEX = optionBox.addTextItem(format(optFormatStr, "Taglines"));
	var SPELLCHECK_ON_SAVE_OPT_INDEX = -1;
	if (gConfigSettings.allowSpellCheck)
		SPELLCHECK_ON_SAVE_OPT_INDEX = optionBox.addTextItem(format(optFormatStr, "Prompt for spell checker on save"));
	const QUOTE_WRAP_OPT_INDEX = optionBox.addTextItem(format(optFormatStr, "Wrap quote lines to terminal width"));
	const JOIN_WRAPPED_QUOTE_LINES_OPT_INDEX = optionBox.addTextItem(format(optFormatStr, "Join quote lines when wrapping"));
	const QUOTE_INITIALS_OPT_INDEX = optionBox.addTextItem(format(optFormatStr, "Quote with author's initials"));
	const QUOTE_INITIALS_INDENT_OPT_INDEX = optionBox.addTextItem(format(optFormatStr, "Indent quote lines containing initials"));
	const TRIM_QUOTE_SPACES_OPT_INDEX = optionBox.addTextItem(format(optFormatStr, "Trim spaces from quote lines"));
	const AUTO_SIGN_OPT_INDEX = optionBox.addTextItem(format(optFormatStr, "Auto-sign messages"));
	const SIGN_REAL_ONLY_FIRST_NAME_OPT_INDEX = optionBox.addTextItem(format(optFormatStr, "  When using real name, use only first name"));
	const SIGN_EMAILS_REAL_NAME_OPT_INDEX = optionBox.addTextItem(format(optFormatStr, "  Sign emails with real name"));
	if (gUserSettings.ctrlQQuote)
		optionBox.chgCharInTextItem(CTRL_Q_QUOTE_OPT_INDEX, checkIdx, CP437_CHECK_MARK);
	var DICTIONARY_OPT_INDEX = -1;
	if (dictionaryFilenames.length > 1)
		DICTIONARY_OPT_INDEX = optionBox.addTextItem("Spell-check dictionary/dictionaries");
	if (gUserSettings.wrapQuoteLines)
		optionBox.chgCharInTextItem(QUOTE_WRAP_OPT_INDEX, checkIdx, CP437_CHECK_MARK);
	if (gUserSettings.joinQuoteLinesWhenWrapping)
		optionBox.chgCharInTextItem(JOIN_WRAPPED_QUOTE_LINES_OPT_INDEX, checkIdx, CP437_CHECK_MARK);
	if (gUserSettings.enableTaglines)
		optionBox.chgCharInTextItem(TAGLINE_OPT_INDEX, checkIdx, CP437_CHECK_MARK);
	if (gConfigSettings.allowSpellCheck && gUserSettings.promptSpellCheckOnSave)
		optionBox.chgCharInTextItem(SPELLCHECK_ON_SAVE_OPT_INDEX, checkIdx, CP437_CHECK_MARK);
	if (gUserSettings.useQuoteLineInitials)
		optionBox.chgCharInTextItem(QUOTE_INITIALS_OPT_INDEX, checkIdx, CP437_CHECK_MARK);
	if (gUserSettings.indentQuoteLinesWithInitials)
		optionBox.chgCharInTextItem(QUOTE_INITIALS_INDENT_OPT_INDEX, checkIdx, CP437_CHECK_MARK);
	if (gUserSettings.trimSpacesFromQuoteLines)
		optionBox.chgCharInTextItem(TRIM_QUOTE_SPACES_OPT_INDEX, checkIdx, CP437_CHECK_MARK);
	if (gUserSettings.autoSignMessages)
		optionBox.chgCharInTextItem(AUTO_SIGN_OPT_INDEX, checkIdx, CP437_CHECK_MARK);
	if (gUserSettings.autoSignRealNameOnlyFirst)
		optionBox.chgCharInTextItem(SIGN_REAL_ONLY_FIRST_NAME_OPT_INDEX, checkIdx, CP437_CHECK_MARK);
	if (gUserSettings.autoSignEmailsRealName)
		optionBox.chgCharInTextItem(SIGN_EMAILS_REAL_NAME_OPT_INDEX, checkIdx, CP437_CHECK_MARK);

	// Create an object containing toggle values (true/false) for each option index
	var optionToggles = {};
	optionToggles[CTRL_Q_QUOTE_OPT_INDEX] = gUserSettings.ctrlQQuote;
	optionToggles[QUOTE_WRAP_OPT_INDEX] = gUserSettings.wrapQuoteLines;
	optionToggles[JOIN_WRAPPED_QUOTE_LINES_OPT_INDEX] = gUserSettings.joinQuoteLinesWhenWrapping;
	optionToggles[TAGLINE_OPT_INDEX] = gUserSettings.enableTaglines;
	if (gConfigSettings.allowSpellCheck)
		optionToggles[SPELLCHECK_ON_SAVE_OPT_INDEX] = gUserSettings.promptSpellCheckOnSave;
	optionToggles[QUOTE_INITIALS_OPT_INDEX] = gUserSettings.useQuoteLineInitials;
	optionToggles[QUOTE_INITIALS_INDENT_OPT_INDEX] = gUserSettings.indentQuoteLinesWithInitials;
	optionToggles[TRIM_QUOTE_SPACES_OPT_INDEX] = gUserSettings.trimSpacesFromQuoteLines;
	optionToggles[AUTO_SIGN_OPT_INDEX] = gUserSettings.autoSignMessages;
	optionToggles[SIGN_REAL_ONLY_FIRST_NAME_OPT_INDEX] = gUserSettings.autoSignRealNameOnlyFirst;
	optionToggles[SIGN_EMAILS_REAL_NAME_OPT_INDEX] = gUserSettings.autoSignEmailsRealName;

	// Set up the enter key in the box to toggle the selected item.
	optionBox.setEnterKeyOverrideFn(function(pBox) {
		var itemIndex = pBox.getChosenTextItemIndex();
		if (itemIndex > -1)
		{
			// If there's an option for the chosen item, then update the text on the
			// screen depending on whether the option is enabled or not.
			if (optionToggles.hasOwnProperty(itemIndex))
			{
				// Toggle the option and refresh it on the screen
				optionToggles[itemIndex] = !optionToggles[itemIndex];
				if (optionToggles[itemIndex])
					optionBox.chgCharInTextItem(itemIndex, checkIdx, CP437_CHECK_MARK);
				else
					optionBox.chgCharInTextItem(itemIndex, checkIdx, " ");
				optionBox.refreshItemCharOnScreen(itemIndex, checkIdx);

				// Toggle the setting for the user in global user setting object.
				switch (itemIndex)
				{
					case CTRL_Q_QUOTE_OPT_INDEX:
						gUserSettings.ctrlQQuote = !gUserSettings.ctrlQQuote;
						break;
					case QUOTE_WRAP_OPT_INDEX:
						gUserSettings.wrapQuoteLines = !gUserSettings.wrapQuoteLines;
						break;
					case JOIN_WRAPPED_QUOTE_LINES_OPT_INDEX:
						gUserSettings.joinQuoteLinesWhenWrapping = !gUserSettings.joinQuoteLinesWhenWrapping;
						break;
					case TAGLINE_OPT_INDEX:
						gUserSettings.enableTaglines = !gUserSettings.enableTaglines;
						break;
					case SPELLCHECK_ON_SAVE_OPT_INDEX:
						gUserSettings.promptSpellCheckOnSave = !gUserSettings.promptSpellCheckOnSave;
						break;
					case QUOTE_INITIALS_OPT_INDEX:
						gUserSettings.useQuoteLineInitials = !gUserSettings.useQuoteLineInitials;
						break;
					case QUOTE_INITIALS_INDENT_OPT_INDEX:
						gUserSettings.indentQuoteLinesWithInitials = !gUserSettings.indentQuoteLinesWithInitials;
						break;
					case TRIM_QUOTE_SPACES_OPT_INDEX:
						gUserSettings.trimSpacesFromQuoteLines = !gUserSettings.trimSpacesFromQuoteLines;
						break;
					case AUTO_SIGN_OPT_INDEX:
						gUserSettings.autoSignMessages = !gUserSettings.autoSignMessages;
						break;
					case SIGN_REAL_ONLY_FIRST_NAME_OPT_INDEX:
						gUserSettings.autoSignRealNameOnlyFirst = !gUserSettings.autoSignRealNameOnlyFirst;
						break;
					case SIGN_EMAILS_REAL_NAME_OPT_INDEX:
						gUserSettings.autoSignEmailsRealName = !gUserSettings.autoSignEmailsRealName;
						break;
					default:
						break;
				}
			}
			// For options that aren't on/off toggle options, take the appropriate action.
			else
			{
				switch (itemIndex)
				{
					case DICTIONARY_OPT_INDEX:
						// Let the user choose a dictionary file
						// Find all the dictionary filenames (matching the filename
						// mask dictionary_*.txt"), and then make a menu of the
						// language names for the user to choose from
						if (dictionaryFilenames.length > 0)
						{
							doUserDictionaryLanguageSelection(optBoxStartX, gEditTop+1, optBoxWidth, optBoxHeight, dictionaryFilenames);
							// Refresh the user option box on the screen
							optionBox.refreshOnScreen(optionBox.chosenTextItemIndex);
						}
						else
							writeWithPause(1, console.screen_rows, "\x01y\x01hThere are no dictionaries!\x01n", ERRORMSG_PAUSE_MS);
						break;
					default:
						break;
				}
			}
		}
	}); // Option box enter key override function

	// Display the option box and have it do its input loop
	var continueOn = true;
	while (continueOn)
	{
		var boxRetObj = optionBox.doInputLoop(true);
		if (boxRetObj.lastKeypress == "?")
		{
			displayUserSettingsHelp(optionBox.dimensions.topLeftX+1, optionBox.dimensions.topLeftY+1, optionBox.dimensions.width-2,
			                        optionBox.dimensions.height-2);
		}
		else if (boxRetObj.userQuit)
			continueOn = false;
	}

	// If the user changed any of their settings, then save the user settings.
	// If the save fails, then output an error message.
	var settingsChanged = false;
	for (var prop in gUserSettings)
	{
		if (gUserSettings.hasOwnProperty(prop))
		{
			settingsChanged = (originalSettings[prop] != gUserSettings[prop]);
			if (settingsChanged)
				break;
		}
	}
	if (settingsChanged)
	{
		if (!WriteUserSettingsFile(gUserSettings))
			writeMsgOntBtmHelpLineWithPause("\x01n\x01y\x01hFailed to save settings!\x01n", ERRORMSG_PAUSE_MS);
	}

	// Now that we're done, erase the option box by re-drawing/writing the appropriate part of the message area.
	var editLineIndexAtSelBoxTopRow = gEditLinesIndex - (originalCurpos.y-optionBox.dimensions.topLeftY);
	displayMessageRectangle(optionBox.dimensions.topLeftX, optionBox.dimensions.topLeftY,
	                        optionBox.dimensions.width, optionBox.dimensions.height,
	                        editLineIndexAtSelBoxTopRow, true);

	if (returnCursorWhenDone)
		console.gotoxy(originalCurpos);

	// See if the user changed their option for using Ctrl-Q to quote
	if (originalSettings.hasOwnProperty("ctrlQQuote") && typeof(originalSettings.ctrlQQuote) === "boolean")
		retObj.ctrlQQuoteOptChanged = (gUserSettings.ctrlQQuote != originalSettings.ctrlQQuote);

	return retObj;
}
// Helper for doUserSettings(): Does the dictionary language selection
//
// Parameters:
//  pBoxTopLeftX: The upper-left X coordinate for the option box
//  pBoxTopLeftY: The upper-left Y coordinate for the option box
//  pBoxWidth: The width for the option box
//  pBoxHeight: The height for the option box\
//  pDictionaryFilenames: An array of the dictionary filenames
function doUserDictionaryLanguageSelection(pBoxTopLeftX, pBoxTopLeftY, pBoxWidth, pBoxHeight, pDictionaryFilenames)
{
	// First, sort the languages by name, and ensure the ones with (General) are before
	// the other, localized/supplimental ones.
	var languageNamesAndDictionaries = [];
	for (var i = 0; i < pDictionaryFilenames.length; ++i)
	{
		var languageName = getLanguageNameFromDictFilename(pDictionaryFilenames[i]);
		languageNamesAndDictionaries.push({ name: languageName, filename: pDictionaryFilenames[i] });
	}
	languageNamesAndDictionaries.sort(languageNameDictFilenameSort);
	// Add the language/dictionary options to the menu
	var dictMenu = new DDLightbarMenu(pBoxTopLeftX, pBoxTopLeftY, pBoxWidth, pBoxHeight);
	var selectedItemIndexes = { };
	for (var i = 0; i < languageNamesAndDictionaries.length; ++i)
	{
		dictMenu.Add(languageNamesAndDictionaries[i].name, languageNamesAndDictionaries[i].filename);
		// See if this dictionary is in the user's configured
		// dictionaries, and if so, add this dictionary to the
		// selected items list.
		if (languageIsSelectedInUserSettings(gUserSettings, languageNamesAndDictionaries[i].filename))
			selectedItemIndexes[i] = true;
	}
	dictMenu.ampersandHotkeysInItems = false;
	dictMenu.AddAdditionalQuitKeys(["q", "Q", KEY_ESC, CTRL_C]);
	dictMenu.multiSelect = true;
	dictMenu.borderEnabled = true;
	dictMenu.SetBorderChars({
		upperLeft: CP437_BOX_DRAWINGS_UPPER_LEFT_SINGLE,
		upperRight: CP437_BOX_DRAWINGS_UPPER_RIGHT_SINGLE,
		lowerLeft: CP437_BOX_DRAWINGS_LOWER_LEFT_SINGLE,
		lowerRight: CP437_BOX_DRAWINGS_LOWER_RIGHT_SINGLE,
		top: CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE,
		bottom: CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE,
		left: CP437_BOX_DRAWINGS_LIGHT_VERTICAL,
		right: CP437_BOX_DRAWINGS_LIGHT_VERTICAL
	});
	dictMenu.scrollbarEnabled = true;
	dictMenu.colors.borderColor = "\x01g";
	dictMenu.topBorderText = "\x01b\x01hSelect dictionary languages (\x01cEnter\x01g: \x01bChoose, \x01cSpacebar\x01g: \x01bMulti-select\x01b)\x01n";
	dictMenu.bottomBorderText = "\x01c\x01hESC\x01y/\x01cQ\x01g: \x01bQuit\x01n";
	var userChoice = dictMenu.GetVal(true, selectedItemIndexes);
	if (userChoice != null)
	{
		gConfigSettings.pDictionaryFilenames = [];
		gUserSettings.pDictionaryFilenames = [];
		for (var i = 0; i < userChoice.length; ++i)
		{
			gConfigSettings.pDictionaryFilenames.push(userChoice[i]);
			gUserSettings.pDictionaryFilenames.push(userChoice[i]);
		}
	}
}

// Helper for doUserSettings(): Displays help for user settings in a scrollable box
function displayUserSettingsHelp(pTopLeftX, pTopLeftY, pWidth, pHeight)
{
	// Draw a border around the frame object
	console.gotoxy(pTopLeftX-1, pTopLeftY-1);
	console.attributes = "HB";
	console.print(CP437_BOX_DRAWINGS_UPPER_LEFT_DOUBLE);
	for (var i = 0; i < pWidth; ++i)
		console.print(CP437_BOX_DRAWINGS_HORIZONTAL_DOUBLE);
	console.print(CP437_BOX_DRAWINGS_UPPER_RIGHT_DOUBLE);
	for (var lineIdx = 0; lineIdx < pHeight; ++lineIdx)
	{
		console.gotoxy(pTopLeftX-1, pTopLeftY+lineIdx);
		console.print(CP437_BOX_DRAWINGS_DOUBLE_VERTICAL);
		console.gotoxy(pTopLeftX+pWidth, pTopLeftY+lineIdx);
		console.print(CP437_BOX_DRAWINGS_DOUBLE_VERTICAL);
	}
	console.gotoxy(pTopLeftX-1, pTopLeftY+pHeight);
	console.print(CP437_BOX_DRAWINGS_LOWER_LEFT_DOUBLE);
	for (var i = 0; i < pWidth; ++i)
		console.print(CP437_BOX_DRAWINGS_HORIZONTAL_DOUBLE);
	console.print(CP437_BOX_DRAWINGS_LOWER_RIGHT_DOUBLE);

	// Create the frame object
	var frameObj = new Frame(pTopLeftX, pTopLeftY, pWidth, pHeight, BG_BLACK);
	frameObj.attr &=~ HIGH;
	frameObj.v_scroll = true;
	frameObj.h_scroll = false;
	frameObj.scrollbars = true;
	var scrollbarObj = new ScrollBar(frameObj, {bg: BG_BLACK, fg: LIGHTGRAY, orientation: "vertical", autohide: false});
	// Add the help text, then start the user input loop for the frame
	var helpText = getUserSettingsHelpText(pWidth);
	frameObj.putmsg(helpText, "\x01n");
	var additionalQuitKeys = "";
	var lastUserInput = doFrameInputLoop(frameObj, scrollbarObj, helpText, additionalQuitKeys);
}

// Returns text for the user settings help
function getUserSettingsHelpText(pMaxTextWidth)
{
	var helpHeaderText = "SlyEdit User Settings Help";
	var numSpaces = Math.floor(pMaxTextWidth/2) - Math.floor(helpHeaderText.length/2);
	var helpText = "\x01n\x01c\x01h" + format("%*s", numSpaces, "") + helpHeaderText + "\x01n\r\n\r\n";
	helpText += "\x01cPress any key to exit this help.\x01n\r\n\r\n";
	helpText += getHelpTextWithLabel("Taglines", "Toggles whether you want to be prompted to add a tag line when you save a message", pMaxTextWidth);
	helpText += "\r\n";
	helpText += getHelpTextWithLabel("Prompt for spell checker on save", "Whether to use the spell checker when saving a message/file", pMaxTextWidth);
	helpText += "\r\n";
	helpText += getHelpTextWithLabel("Wrap quote lines to terminal width", "Whether or not to wrap quoted lines to your terminal width, to preserve all of their text. If not, quote lines will be truncated due to the quote prefix.", pMaxTextWidth);
	helpText += "\r\n";
	helpText += getHelpTextWithLabel("Join quote lines when wrapping", "Whether or not quoted lines should be joined together after wrapping. If not, quote lines will always start on their own line, even when wrapping is enabled. Turning this off can help preserve some of the formatting of the quoted lines.", pMaxTextWidth);
	helpText += "\r\n";
	helpText += getHelpTextWithLabel("Quote with author's initials", "Whether or not to include the author's initials in the quote prefix when quoting the message you're replying to", pMaxTextWidth);
	helpText += "\r\n";
	helpText += getHelpTextWithLabel("Indent quote lines containing initials", "When quoting with author's initials, whether or not to indent quote lines with a space", pMaxTextWidth);
	helpText += "\r\n";
	helpText += getHelpTextWithLabel("Trim spaces from quote lines", "Whether or not to remove leading and trailing spaces from quote lines. If you want to preserve formatting, you should turn this off before quoting the message.", pMaxTextWidth);
	helpText += "\r\n";
	helpText += getHelpTextWithLabel("Auto-sign messages", "Whether or not to automatically sign your name at the end of a message. Your handle will be used in sub-boards where handles are allowed; otherwise, your real name will be used.", pMaxTextWidth);
	helpText += "\r\n";
	helpText += getHelpTextWithLabel("  When using real name, use only first name", "When auto-signing with your real name, whether to use only your first name", pMaxTextWidth);
	helpText += "\r\n";
	helpText += getHelpTextWithLabel("  Sign emails with real name", "When auto-signing messages, whether or not to always sign emails with your real name", pMaxTextWidth);
	helpText += "\r\n";
	helpText += getHelpTextWithLabel("Spell-check dictionary/dictionaries", "Here, you can specify which spell-check dictionaries are used", pMaxTextWidth);
	helpText += "\x01n";
	return helpText;
}
// Helper for getUserSettingsHelpText()
function getHelpTextWithLabel(pLabel, pHelpText, pMaxWidth)
{
	var helpText = format("\x01n\x01c\x01h%s\x01g: \x01n\x01c%s", pLabel, pHelpText);
	return lfexpand(word_wrap(helpText, pMaxWidth, null, false));
}

// Allows the user to select a tagline.  Returns an object with the following
// properties:
//  taglineWasSelected: Boolean - Whether or not a tag line was selected
//  tagline: String - The tag line that was selected
function doTaglineSelection()
{
	var retObj = {
		taglineWasSelected: false,
		tagline: ""
	};

	// Read the tagline file
	var taglines = readTxtFileIntoArray(gConfigSettings.tagLineFilename, true, true, 5000);
	if (taglines.length == 0)
		return retObj;

	// If the configuration option to shuffle the taglines is enabled, then
	// shuffle them.
	if (gConfigSettings.shuffleTaglines)
		shuffleArray(taglines);

	// Create the list box for the taglines.  Make the box up to 14 lines tall.
	var boxHeight = (taglines.length > 12 ? 14 : taglines.length+2);
	var boxTopRow = gEditTop + Math.floor((gEditHeight/2) - (boxHeight/2));

	// Set up the menu
	var taglineMenu = new DDLightbarMenu(gEditLeft, boxTopRow, gEditWidth, boxHeight);
	taglineMenu.colors.borderColor = "\x01g";
	taglineMenu.colors.itemColor = "\x01n\x01c";
	taglineMenu.colors.selectedItemColor = "\x01n\x01w\x01h\x01" + "4";
	taglineMenu.borderEnabled = true;
	taglineMenu.SetBorderChars({
		upperLeft: CP437_BOX_DRAWINGS_UPPER_LEFT_SINGLE,
		upperRight: CP437_BOX_DRAWINGS_UPPER_RIGHT_SINGLE,
		lowerLeft: CP437_BOX_DRAWINGS_LOWER_LEFT_SINGLE,
		lowerRight: CP437_BOX_DRAWINGS_LOWER_RIGHT_SINGLE,
		top: CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE,
		bottom: CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE,
		left: CP437_BOX_DRAWINGS_LIGHT_VERTICAL,
		right: CP437_BOX_DRAWINGS_LIGHT_VERTICAL
	});
	taglineMenu.topBorderText = "\x01n\x01g" + CP437_BOX_DRAWINGS_LIGHT_VERTICAL_AND_LEFT + "\x01b\x01hTaglines\x01n\x01g" + CP437_BOX_DRAWINGS_LIGHT_LEFT_T;
	taglineMenu.bottomBorderText = "\x01n\x01h\x01c"+ UP_ARROW + "\x01b, \x01c"+ DOWN_ARROW + "\x01b, \x01cPgUp\x01b/\x01cPgDn\x01b, "
	                     + "\x01cF\x01y)\x01birst, \x01cL\x01y)\x01bast, \x01cHOME\x01b, \x01cEND\x01b, \x01cEnter\x01y=\x01bSelect, "
	                     + "\x01cR\x01y)\x01bandom, \x01cESC\x01n\x01c/\x01h\x01cQ\x01y=\x01bEnd";
	taglineMenu.additionalQuitKeys = "RrQqFfLl";
	taglineMenu.wrapNavigation = true;
	taglineMenu.ampersandHotkeysInItems = false;
	taglineMenu.scrollbarEnabled = true;
	taglineMenu.multiSelect = false;
	// Add the tag lines to the menu
	for (var i = 0; i < taglines.length; ++i)
		taglineMenu.Add(taglines[i], taglines[i]);
	// Input loop for displaying the menu and letting the user make a selection.
	// This input loop is here in order to respond to the F and L keys, to go
	// to the first & last pages of the menu.
	var continueOn = true;
	while (continueOn)
	{
		var chosenTagline = taglineMenu.GetVal(true);
		var lastUserInputUpper = taglineMenu.lastUserInput.toUpperCase();
		// If the user pressed R, then choose a random tagline.
		if (lastUserInputUpper == "R")
		{
			retObj.tagline = taglines[random(taglines.length)];
			retObj.taglineWasSelected = true;
			continueOn = false;
		}
		// First page
		else if (lastUserInputUpper == "F")
			tagLineMenu.SetSelectedItemIdx(0); // This should also set its topItemIdx to 0
		// Last page
		else if (lastUserInputUpper == "L")
		{
			var lastPageTopItemIdx = taglineMenu.GetTopItemIdxOfLastPage();
			taglineMenu.selectedItemIdx = lastPageTopItemIdx;
			taglineMenu.topItemIdx = lastPageTopItemIdx;
		}
		// If the user chose a tagline, then use it.
		else if (typeof(chosenTagline) == "string")
		{
			retObj.tagline = chosenTagline;
			retObj.taglineWasSelected = true;
			continueOn = false;
		}
		// If the user exited, then exit this loop.
		else if (chosenTagline == null)
			continueOn = false;
	}

	// If a tagline was selected, then add the tagline prefix in front of it, and
	// also quote the tagline if the option to do so is enabled.
	if (retObj.taglineWasSelected)
	{
		if (gConfigSettings.taglinePrefix.length > 0)
			retObj.tagline = gConfigSettings.taglinePrefix + retObj.tagline;
		// If the option to quote taglines is enabled, then do it.
		if (gConfigSettings.quoteTaglines)
			retObj.tagline = "\"" + retObj.tagline + "\"";
	}

	return retObj;
}

// Sets the quote prefix, gQuotePrefix (the text to use for prefixing quote lines).
function setQuotePrefix()
{
	// To minimize disk reads in case the user changes their preference for author
	// initials in quote lines, get the from name of the message being replied to
	// and store it in a persistent variable.  This is done at the beginning of
	// this function whether or not the user wants author initials in quote lines
	// to ensure we get the correct name in case anything in the message base
	// changes after this function is first called.
	if (!setQuotePrefix.curMsgFromName)
		setQuotePrefix.curMsgFromName = getFromNameForCurMsg(gMsgAreaInfo);

	gQuotePrefix = " > "; // The default quote prefix
	// If we're configured to use poster's initials in the
	// quote lines, then do it.
	if (gUserSettings.useQuoteLineInitials)
	{
		// For the name to use for quote line initials:
		// If posting in a message sub-board, get the author's name from the
		// header of the current message being read in the sub-board (in
		// case the user changes the "To" name).  Otherwise (if not posting in
		// a message sub-board), use the gToName value read from the drop file.
		// Remove any leading, multiple, or trailing spaces.
		var quotedName = "";
		if (postingInMsgSubBoard(gMsgArea))
		{
			quotedName = trimSpaces(setQuotePrefix.curMsgFromName, true, true, true);
			if (quotedName.length == 0)
				quotedName = trimSpaces(gToName, true, true, true);
		}
		else
			quotedName = trimSpaces(gToName, true, true, true);
		// If configured to indent quote lines w/ initials with
		// a space, then do it.
		gQuotePrefix = "";
		if (gUserSettings.indentQuoteLinesWithInitials)
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

// Writes an array of strings to the message tagline file (editor.tag in the node's
// temp directory).
//
// Parameters:
//  pTagline: The tagline to write to the file (a string)
//  pTaglineFilename: The full path & filename of the tagline file
//
// Return value: Boolean - Whether or not the tagline file was successfully written.
//               If pTaglineFilename is empty or is not a string, or if pTagline is
//               not a string, this will return false.  If pTagline is empty, then
//               this will return true.
function writeTaglineToMsgTaglineFile(pTagline, pTaglineFilename)
{
	// Sanity checking
	if ((typeof(pTagline) != "string") || (typeof(pTaglineFilename) != "string"))
		return false;
	if (pTaglineFilename.length == 0)
		return false;
	if (pTagline.length == 0)
		return true;

	var wroteTaglineFile = false;
	var taglineFile = new File(pTaglineFilename);
	if (taglineFile.open("w"))
	{
		// Write the tagline to the file, with a blank line before it for spacing.
		taglineFile.writeln("");
		taglineFile.writeln(pTagline);
		taglineFile.close();
		wroteTaglineFile = true;
	}
	return wroteTaglineFile;
}

// Writes some text over the bottom help line, with a pause before erasing the
// text and refreshing the bottom help line.
//
// Parameters:
//  pMsg: The text to write
//  pPauseMS: The pause (in milliseconds) to wait while displaying the message
function writeMsgOntBtmHelpLineWithPause(pMsg, pPauseMS)
{
   // Write the message with the pause, then refresh the help line on the
   // bottom of the screen.
   writeWithPause(1, console.screen_rows, pMsg, pPauseMS);
   fpDisplayBottomHelpLine(console.screen_rows, gUseQuotes, gUserSettings.ctrlQQuote);
}

// Gets the user's alias/name to use for auto-signing the message.
//
// Parameters:
//  pSubCode: The sub-board code ("mail" for email)
//  pRealNameOnlyFirst: Whether or not to use the user's first name only when using their real name
//  pRealNameForEmail: Whether or not to use the user's real name for email
//
// Return value: The user's name to use for auto-signing
function getSignName(pSubCode, pRealNameOnlyFirst, pRealNameForEmail)
{
	var useRealName = false;
	if (typeof(pSubCode) === "string" && pSubCode != "")
	{
		if (pSubCode == "mail")
			useRealName = pRealNameForEmail;
		else
			useRealName = Boolean(msg_area.sub[pSubCode].settings & SUB_NAME);
	}
	var signName = "";
	if (useRealName)
	{
		signName = trimSpaces(user.name, true, false, true);
		if (pRealNameOnlyFirst)
		{
			var spacePos = signName.indexOf(" ");
			if (spacePos > -1)
				signName = signName.substr(0, spacePos);
		}
	}
	else
		signName = trimSpaces(user.alias, true, false, true);
	return signName;
}

// Lets the user upload a message in a text file, replacing any message
// that might be written in the editor.
//
// Parameters:
//  pCurpos: The console's current position (with x and y members)
//
// Return value: Boolean - True if successfully uploaded, false if not
function letUserUploadMessageFile(pCurpos)
{
	gUploadedMessageFile = false;

	var originalCurpos;
	if ((typeof(pCurpos) == "object") && pCurpos.hasOwnProperty("x") && pCurpos.hasOwnProperty("y"))
		originalCurpos = pCurpos;
	else
		originalCurpos = console.getxy();

	var uploadedMessage = false;
	if (promptYesNo("Upload a message", true, "Upload message", true, true))
	{
		console.attributes = "N";
		console.gotoxy(1, console.screen_rows);
		console.crlf();
		var msgFilename = system.node_dir + "uploadedMsg.txt";
		if (bbs.receive_file(msgFilename))
		{
			console.print("Upload succeeded.\r\n");
			// Read the file and populate gEditLines
			var msgFile = new File(msgFilename);
			if (msgFile.open("r"))
			{
				uploadedMessage = true;
				gUploadedMessageFile = true;

				// Free up memory already in gEditLiens, and then
				// re-populate gEditLines with the new message from the file
				for (var i = 0; i < gEditLines.length; ++i)
					delete gEditLines[i];
				gEditLines = [];

				var fileLine;
				while (!msgFile.eof)
				{
					// Read the next line from the file
					fileLine = msgFile.readln(2048);

					// fileLine should be a string, but I've seen some cases
					// where for some reason it isn't.  If it's not a string,
					// then continue onto the next line.
					if (typeof(fileLine) != "string")
						continue;
					// Add the line to gEditLines
					// TODO: It seems this isn't populating gEditLines and
					// it's sending an empty message.
					gEditLines.push(new TextLine(fileLine, true, false));
				}
				
				msgFile.close();
				file_remove(msgFilename);
			}
			else
			{
				console.print("\x01y\x01hFailed to read the message file!\x01n\r\n\x01p");
				fpRedrawScreen(gEditLeft, gEditRight, gEditTop, gEditBottom, gTextAttrs, gInsertMode, gUseQuotes,
				               gUserSettings.ctrlQQuote, 0, displayEditLines);
			}
		}
		else
		{
			console.print("\x01y\x01hUpload failed!\x01n\r\n\x01p");
			fpRedrawScreen(gEditLeft, gEditRight, gEditTop, gEditBottom, gTextAttrs, gInsertMode, gUseQuotes,
			               gUserSettings.ctrlQQuote, 0, displayEditLines);
		}
	}

	console.gotoxy(originalCurpos);
	return uploadedMessage;
}

// Returns whether any of the message lines have attribute codes
function messageLinesHaveAttrs()
{
	var msgHasAttrCodes = false;
	for (var i = 0; i < gEditLines.length && !msgHasAttrCodes; ++i)
		msgHasAttrCodes = msgHasAttrCodes || gEditLines[i].hasAttrs();
	return msgHasAttrCodes;
}

// Gets all the attribute codes from the edit lines until a certain index.
//
// Parameters:
//  pEndArrayIdx: One past the last edit line index to get attributes for
//  pIncludeEndArrayIdxAttrs: Optional boolean: Whether or not to include the attributes for the line at the end array index.
//                            Defaults to false.
//
// Return value: A string containing the relevant attribute codes to apply up to the given line index (non-inclusive).
function getAllEditLineAttrsUntilLineIdx(pEndArrayIdx, pIncludeEndArrayIdxAttrs)
{
	if (typeof(pEndArrayIdx) !== "number" || pEndArrayIdx < 0)
		return "";

	var includeEndArrayIdxAttrs = (typeof(pIncludeEndArrayIdxAttrs) === "boolean" ? pIncludeEndArrayIdxAttrs : false);

	var attributesStr = "";
	var onePastLastIdx = (includeEndArrayIdxAttrs ? pEndArrayIdx + 1 : pEndArrayIdx);
	for (var i = 0; i < onePastLastIdx; ++i)
	{
		if (typeof(gEditLines[i]) !== "object") continue;
		if (isQuoteLine(gEditLines, i))
		{
			attributesStr = "\x01n"; // Attributes should be reset at the beginning of quote blocks
			continue;
		}
		else
			attributesStr += gEditLines[i].getAttrs();
	}
	// If there is a normal attribute code in the middle of the string, remove anything before it
	var normalAttrIdx = attributesStr.lastIndexOf("\x01n");
	if (normalAttrIdx < 0)
		normalAttrIdx = attributesStr.lastIndexOf("\x01N");
	if (normalAttrIdx > -1)
		attributesStr = attributesStr.substr(normalAttrIdx/*+2*/);
	return attributesStr;
}
// Gets all the attribute codes from the edit lines until an index within one of the edit lines.
//
// Parameters:
//  pEndArrayIdx: The last edit line index to get attributes for
//  pLineEditIdx: The index within the current line to get the last attributes for
//
// Return value: A string containing the relevant attribute codes to apply up to the given line indexes
function getAllEditLineAttrs(pEndArrayIdx, pLineEditIdx)
{
	var attributesStr = "";
	if (!isQuoteLine(gEditLines, pEndArrayIdx))
	{
		attributesStr = getAllEditLineAttrsUntilLineIdx(pEndArrayIdx);
		if (typeof(gEditLines[pEndArrayIdx]) === "object")
			attributesStr += gEditLines[pEndArrayIdx].getAttrs(pLineEditIdx);
		// If there is a normal attribute code in the middle of the string, remove anything before it
		var normalAttrIdx = attributesStr.lastIndexOf("\x01n");
		if (normalAttrIdx < 0)
			normalAttrIdx = attributesStr.lastIndexOf("\x01N");
		if (normalAttrIdx > -1)
			attributesStr = attributesStr.substr(normalAttrIdx/*+2*/);
	}
	return attributesStr;
}

// Prompts the user to enter a graphical character (codes 129-155).
// Credit to Deuce - Taken from FSEditor (get_graphic()) and modified a bit.
//
// Parameters:
//  pCurPos: The current cursor position
//
//  Return value: A graphic character if one is selected, or null if none was selected
function promptForGraphicsChar(pCurPos)
{
	console.gotoxy(1, console.screen_rows);
	console.cleartoeol("\x01n");
	console.gotoxy(1, console.screen_rows);
	console.putmsg("Enter Graphics Code (\x01H?\x01N for a list, \x01HCTRL-C\x01N to cancel): ");
	// Write a few spaces with a background color to indicate input text length
	console.attributes = 31;
	console.write(format("%3.3s",""));
	var inp = ""; // The user's inputted string
	console.gotoxy(55 + inp.length, console.screen_rows);
	var continueOn = true;
	while (continueOn)
	{
		var ch = getUserKey(K_UPPER|K_NOCRLF|K_NOSPIN);
		switch(ch)
		{
			case '':
				break;
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				switch(inp.length)
				{
					case 0:
						if (ch != '1' && ch != '2')
						{
							console.beep();
							ch = '';
						}
						break;
					case 1:
						if (inp.substr(0,1) == '1' && ch < '2')
						{
							console.beep();
							ch = '';
						}
						if (inp.substr(0,1) == '2' && ch > '5')
						{
							console.beep();
							ch = '';
						}
						break;
					case 2:
						if (inp.substr(0,2) == '25' && ch > '5')
						{
							console.beep();
							ch = '';
						}
						if (inp.substr(0,2) == '12' && ch < '8')
						{
							console.beep();
							ch = '';
						}
						break;
					default:
						console.beep();
						ch = '';
						break;
				}
				console.print(ch);
				inp += ch;
				break;
			case '\x08':	// Backspace
				if (inp.length > 0)
				{
					inp = inp.substr(0, inp.length-1);
					var curPos = console.getxy();
					console.gotoxy(curPos.x-1, curPos.y);
					console.print(" ");
					console.gotoxy(curPos.x-1, curPos.y);
				}
				break;
			case '\x0d':	// CR
				if (inp.length >= 3)
				{
					// Return the chosen graphic char here
					console.attributes = "N";
					var intVal = parseInt(inp);
					if (gUserConsoleSupportsUTF8) // In fseditor, if the print mode contains P_UTF8
						return CP437CodeToUTF8Code(intVal);
					else
						return ascii(intVal);
				}
				else if (inp.length > 0)
				{
					console.beep();
					break;
				}
				else
				{
					// Abort
					continueOn = false;
					break;
				}
			case '\x03':	// CTRL-C
				continueOn = false;
				break;
			case '?':
				// Draws a graphics reference box then waits for a keypress.
				var refBoxStartRow = gEditTop + 3;
				// Blank out the edit area at the top & bottom of where the reference box will be drawn
				for (var row = gEditTop; row <= refBoxStartRow; ++row)
				{
					console.gotoxy(1, row);
					console.cleartoeol("\x01n");
				}
				for (var row = gEditBottom-2; row <= gEditBottom; ++row)
				{
					console.gotoxy(1, row);
					console.cleartoeol("\x01n");
				}

				console.gotoxy(1, refBoxStartRow);
				console.attributes = 7;
				for (var x = 128; x < 256; ++x)
				{
					printf(" %s: %-3d", ascii(x), x);
					if (x == 137 || x == 147 || x == 157 || x == 167 || x == 177 || x == 187 || x == 197 || x == 207 || x == 217 || x == 227 || x == 237 || x == 247)
						console.crlf();
				}
				console.cleartoeol();
				console.gotoxy(1, refBoxStartRow + 13);
				console.print("PRESS ANY KEY TO CONTINUE", gPrintMode);
				if (gUserConsoleSupportsUTF8)
					console.print(" (Results may be different for a UTF-8 terminal)", gPrintMode);
				console.cleartoeol();
				console.getkey();

				// Refresh the edit lines on the screen
				var editLinesTopIndex = gEditLinesIndex - (pCurPos.y - gEditTop);
				displayMessageRectangle(1, gEditTop, gEditWidth, gEditHeight, editLinesTopIndex, true);
				
				// Return to the char # prompt
				console.gotoxy(55 + inp.length, console.screen_rows);
				// Write a few spaces with a background color to indicate input text length
				console.attributes = 31;
				console.write(format("%3.3s",""));
				console.gotoxy(55 + inp.length, console.screen_rows);
				break;
			default:
				console.beep();
				break;
		}
	}

	console.attributes = "N";
}

// Inputs a meme from the user and inserts it into the message
//
// Return value: An object with the following properties:
//               refreshScreen: Whether or not the whole screen should be refreshed (boolean)
//               numMemeLines: The number of lines in the meme
function doMemeInput()
{
	var retObj = {
		refreshScreen: true,
		numMemeLines: 0
	};


	console.attributes = "N";
	console.gotoxy(1, console.screen_rows);
	console.crlf();
	console.print("\x01g\x01h- \x01n\x01cMeme input \x01g-\x01n\r\n");
	// Allow the user to change the meme width if they want to
	//console.print("\x01cMeme width\x01g\x01h: \x01n");
	//var editWidth = console.screen_columns - 13;
	var promptText = format("\x01cMeme width (up to \x01h%d\x01n\x01c)\x01g\x01h: \x01n", console.screen_columns-1);
	console.print(promptText);
	//var editWidth = console.screen_columns - console.strlen(promptText) - 1;
	var editWidth = (console.screen_columns-1).toString().length + 1;
	var inputtedMemeWidth = console.getstr(gConfigSettings.memeSettings.memeDefaultWidth.toString(), editWidth, K_EDIT|K_LINE|K_NOSPIN|K_NUMBER);
	var memeWidthNum = parseInt(inputtedMemeWidth, 10);
	if (!(!isNaN(memeWidthNum) && memeWidthNum > 0 && memeWidthNum < console.screen_columns))
	{
		memeWidthNum = gConfigSettings.memeSettings.memeDefaultWidth;
		var errorMsg = format("\x01y\x01hInvalid width (\x01wmust be between 1 and %d (1 less than terminal width)\x01y). Defaulting to %d.\x01n",
		                      console.screen_columns-1, gConfigSettings.memeSettings.memeDefaultWidth);
		console.print(lfexpand(word_wrap(errorMsg, console.screen_columns-1)));
		console.crlf();
	}
	// Input the meme text from the user
	console.print("\x01cWhat do you want to say? \x01g\x01h(\x01cENTER\x01g=\x01n\x01cAbort\x01g\x01h)\x01n\r\n");
	var text = console.getstr(gConfigSettings.memeSettings.memeMaxTextLen, K_LINEWRAP);
	if (typeof(text) !== "string" || text.length == 0)
		return retObj;
	var memeOptions = {
		random: gConfigSettings.memeSettings.random,
		border: gConfigSettings.memeSettings.memeDefaultBorderIdx,
		color: gConfigSettings.memeSettings.memeDefaultColorIdx,
		justify: gConfigSettings.memeSettings.justify,
		width: memeWidthNum
	};
	var msg = load("meme_chooser.js", text, memeOptions);
	var memeContent = (typeof(msg) === "string" ? msg : "");

	// Split the meme content into lines
	var memeLines = memeContent.split("\r\n");
	// The last meme line will probably be an empty line due to
	// the trailing \r\n, so remove it
	if (memeLines.length > 0 && memeLines[memeLines.length-1].length == 0)
		memeLines.pop();
	retObj.numMemeLines = memeLines.length;
	// If the user entered a meme, then add it to the message
	if (memeLines.length > 0)
	{
		// The previous edit line should have a hard newline ending
		if (gEditLinesIndex > 0)
			gEditLines[gEditLinesIndex-1].hardNewlineEnd = true;

		// Remove the current edit line; the meme will start on this line
		gEditLines.splice(gEditLinesIndex, 1);

		// Ensure the meme lines are inserted at the correct place
		// in gEditLines, depending on the edit lines array index
		if (gEditLinesIndex == gEditLines.length-1)
		{
			// Append the meme to the edit lines
			for (var i = 0; i < memeLines.length; ++i)
				gEditLines.push(new TextLine(memeLines[i], true, false));
			gEditLines.push(new TextLine("", true, false));
			gEditLinesIndex = gEditLines.length - 1;
			gTextLineIndex = 0;
			retObj.currentWordLength = 0;
		}
		else
		{
			// Currently in the middle of the message
			for (var i = 0; i < memeLines.length; ++i)
				gEditLines.splice(gEditLinesIndex++, 0, new TextLine(memeLines[i], true, false));
		}
		/*
		retObj.currentWordLength = 0;
		gTextLineIndex = 0;
		gEditLines[gEditLinesIndex].text = "";
		// Blank out the /M on the screen
		console.print(chooseEditColor());
		retObj.x = gEditLeft;
		console.gotoxy(retObj.x, retObj.y);
		*/
	}

	return retObj;
}

// Displays a Frame object and handles the input loop for navigation until
// the user presses Q, Enter, or ESC To quit the input loop
//
// Parameters:
//  pFrame: The Frame object
//  pScrollbar: The Scrollbar object for the Frame
//  pFrameContentStr: The string content that was added to the Frame
//  pAdditionalQuitKeys: Optional - A string containing additional keys to quit the
//                       input loop.  This is case-sensitive.
//
// Return value: The last keypress/input from the user
function doFrameInputLoop(pFrame, pScrollbar, pFrameContentStr, pAdditionalQuitKeys)
{
	var checkAdditionalQuitKeys = (typeof(pAdditionalQuitKeys) === "string" && pAdditionalQuitKeys.length > 0);

	// Input loop for the frame to let the user scroll it
	var frameContentTopYOffset = 0;
	//var maxFrameYOffset = pFrameContentStr.split("\r\n").length - pFrame.height;
	var maxFrameYOffset = countOccurrencesInStr(pFrameContentStr, "\r\n") - pFrame.height;
	if (maxFrameYOffset < 0) maxFrameYOffset = 0;
	var userInput = "";
	var continueOn = true;
	do
	{
		pFrame.scrollTo(0, frameContentTopYOffset);
		pFrame.invalidate();
		pScrollbar.cycle();
		pFrame.cycle();
		pFrame.draw();
		// Note: getKeyWithESCChars() is defined in dd_lightbar_menu.js.
		userInput = getKeyWithESCChars(K_NOECHO|K_NOSPIN|K_NOCRLF, 30000).toUpperCase();
		if (userInput == KEY_UP)
		{
			if (frameContentTopYOffset > 0)
				--frameContentTopYOffset;
		}
		else if (userInput == KEY_DOWN)
		{
			if (frameContentTopYOffset < maxFrameYOffset)
				++frameContentTopYOffset;
		}
		else if (userInput == KEY_PAGEUP)
		{
			frameContentTopYOffset -= pFrame.height;
			if (frameContentTopYOffset < 0)
				frameContentTopYOffset = 0;
		}
		else if (userInput == KEY_PAGEDN)
		{
			frameContentTopYOffset += pFrame.height;
			if (frameContentTopYOffset > maxFrameYOffset)
				frameContentTopYOffset = maxFrameYOffset;
		}
		else if (userInput == KEY_HOME)
			frameContentTopYOffset = 0;
		else if (userInput == KEY_END)
			frameContentTopYOffset = maxFrameYOffset;

		// Check for whether to continue the input loop
		continueOn = (userInput != "Q" && userInput != KEY_ENTER && userInput != KEY_ESC);
		// If the additional quit keys does not contain the user's keypress, then continue
		// the input loop.
		// In other words, if the additional quit keys includes the user's keypress, then
		// don't continue.
		if (continueOn && checkAdditionalQuitKeys)
			continueOn = (pAdditionalQuitKeys.indexOf(userInput) < 0);
	} while (continueOn);

	return userInput;
}
