/* This is a script that lets the user choose a file area,
 * with either a lightbar or traditional user interface.
 *
 * Date       User          Version Description
 * 2010-03-06 Eric Oulashin 0.90    Started (based on DDFileAreaChooser.js)
 * 2010-03-08 Eric Oulashin         Continued work
 * 2010-03-09 Eric Oulashin         Continued work
 * 2010-03-11 Eric Oulashin         Continued work
 * 2010-03-12 Eric Oulashin         Fixed a bug related to choosing a file area
 *                                  by typing a number, in lightbar mode.
 * 2010-03-13 Eric Oulashin 1.00    Fixed a bug where it was incorrectly displaying
 *                                  the "currently selected" marker next to some
 *                                  directories in some file libraries.
 *                                  Updated to read settings from a configuration
 *                                  file.
 * 2012-10-06 Eric Oulashin 1.02    For lightbar mode, updated to display the
 *                                  page number in the header at the top (in
 *                                  addition to the total number of pages,
 *                                  which it was already displaying).
 * 2012-11-30 Eric Oulashin 1.03    Bug fix: After leaving the help screen
 *                                  from the directory list, the top line
 *                                  is now correctly written with the page
 *                                  information as "Page # of #".
 * 2013-05-04 Eric Oulashin 1.04    Updated to dynamically adjust the length
 *                                  of the # files column based on the
 *                                  greatest number of files of all
 *                                  file dirs within a file library so
 *                                  that the formatting still looks good.
 * 2013-05-10 Eric Oulashin 1.05    Bug fix: In
 *                                  DDFileAreaChooser_listDirsInFileLib_Traditional,
 *                                  updated a couple lines of code to use
 *                                  libIndex rather than pLibIndex, since
 *                                  pLibIndex can sometimes be invalid.
 *                                  Also updated that function to prepare
 *                                  the associative array of directory
 *                                  information for the file library.
 * 2014-09-14 Eric Oulashin 1.06    Bug fix: Updated the highlight (lightbar)
 *                                  format string in the
 *                                  DDFileAreaChooser_buildFileDirPrintfInfoForLib()
 *                                  function to include a normal attribute at
 *                                  the end to avoid color issues when clearing
 *                                  the screen, etc.  Bug reported by Psi-Jack.
 * 2014-12-22 Eric Oulashin 1.07    Updated the verison number to match the
 *                                  message area chooser, which had a couple of
 *                                  bugs fixed.
 * 2015-04-19 Eric Oulashin 1.08    Added color settings for the lightbar help text
 *                                  at the bottom of the screen.  Also, added the
 *									ability to use the PageUp & PageDown keys instead
 *                                  of P and N in the lightbar lists.
 * 2016-01-17 Eric Oulashin 1.09    Updated to allow choosing only the directory within
 *                                  the user's current file library.  The first command-
 *                                  line argument now specifies whether or not to
 *                                  allow choosing the library, and it defaults
 *                                  to true.
 * 2016-02-14 Eric Oulashin 1.10Beta Started updating to allow a header ANSI/ASC
 *                                  to be displayed above the area list
 * 2016-02-15 Eric Oulashin 1.10    Releasing this version
 * 2016-02-19 Eric Oulashin 1.11    Bug fix: The page number wasn't being updated
 *                                  properly when changing pages in the file
 *                                  libraries
 * 2016-11-22 Eric Oulashin 1.12    Updated the version to match the message area
 *                                  chooser, which has a bug fix for working
 *                                  with null message headers, which seem to
 *                                  be more common with Synchronet 3.17+ (the
 *                                  message voting feature was introduced in 3.17).
 * 2016-12-11 Eric Oulashin 1.13    Updated the version to match the message area chooser
 * 2017-12-18 Eric Oulashin 1.15    Updated the definitions of the KEY_PAGE_UP and KEY_PAGE_DOWN
 *                                  variables to match what they are in sbbsdefs.js (if defined)
 *                                  from December 18, 2017 so that the PageUp and PageDown keys
 *                                  continue to work properly.  This script should still also work
 *                                  with older builds of Synchronet.
 * 2018-03-07 Eric Oulashin 1.16    Bug fix for off-by-one when a file directory has no libraries.
 * 2018-06-25 Eric Oulashin 1.17    Updated the version number to match my message area chooser.
 * 2019-08-18 Eric Oulashin 1.18 Beta Started working on file area searching.
 * 2019-08-22 Eric Oulashin 1.18    Releasing this version
 * 2019-08-24 Eric Oulashin 1.19    Fixed a bug with 'Next' search when
 *                                  returning from sub-board choosing
 */

/* Command-line arguments:
   1 (argv[0]): Boolean - Whether or not to choose a file library first (default).  If
                false, the user will only be able to choose a different directory within
				their current file library.
   2 (argv[1]): Boolean - Whether or not to run the area chooser (if false,
                then this file will just provide the DDFileAreaChooser class).
*/

var requireFnExists = (typeof(require) === "function");
if (requireFnExists)
	require("sbbsdefs.js", "K_NOCRLF");
else
	load("sbbsdefs.js");

// This script requires Synchronet version 3.14 or higher.
// Exit if the Synchronet version is below the minimum.
if (system.version_num < 31400)
{
	var message = "\1n\1h\1y\1i* Warning:\1n\1h\1w Digital Distortion Message Lister "
	             + "requires version \1g3.14\1w or\r\n"
	             + "higher of Synchronet.  This BBS is using version \1g" + system.version
	             + "\1w.  Please notify the sysop.";
	console.crlf();
	console.print(message);
	console.crlf();
	console.pause();
	exit();
}

// Version & date variables
var DD_FILE_AREA_CHOOSER_VERSION = "1.19";
var DD_FILE_AREA_CHOOSER_VER_DATE = "2018-08-24";

// Keyboard input key codes
var CTRL_H = "\x08";
var CTRL_M = "\x0d";
var KEY_ENTER = CTRL_M;
var BACKSPACE = CTRL_H;
var CTRL_F = "\x06";
var KEY_ESC = ascii(27);
// PageUp & PageDown keys - Synchronet 3.17 as of about December 18, 2017
// use CTRL-P and CTRL-N for PageUp and PageDown, respectively.  sbbsdefs.js
// defines them as KEY_PAGEUP and KEY_PAGEDN; I've used slightly different names
// in this script so that this script will work with Synchronet systems before
// and after the update containing those key definitions.
var KEY_PAGE_UP = "\x10"; // Ctrl-P
var KEY_PAGE_DOWN = "\x0e"; // Ctrl-N
// Ensure KEY_PAGE_UP and KEY_PAGE_DOWN are set to what's defined in sbbs.js
// for KEY_PAGEUP and KEY_PAGEDN in case they change
if (typeof(KEY_PAGEUP) === "string")
	KEY_PAGE_UP = KEY_PAGEUP;
if (typeof(KEY_PAGEDN) === "string")
	KEY_PAGE_DOWN = KEY_PAGEDN;

// Key codes for display
var UP_ARROW = ascii(24);
var DOWN_ARROW = ascii(25);

// Misc. defines
var ERROR_WAIT_MS = 1500;
var SEARCH_TIMEOUT_MS = 10000;

// Determine the script's startup directory.
// This code is a trick that was created by Deuce, suggested by Rob Swindell
// as a way to detect which directory the script was executed in.  I've
// shortened the code a little.
var gStartupPath = '.';
try { throw dig.dist(dist); } catch(e) { gStartupPath = e.fileName; }
gStartupPath = backslash(gStartupPath.replace(/[\/\\][^\/\\]*$/,''));

// 1st command-line argument: Whether or not to choose a file library first (if
// false, then only choose a directory within the user's current library).  This
// can be true or false.
var chooseFileLib = true;
if (typeof(argv[0]) == "boolean")
	chooseFileLib = argv[0];
else if (typeof(argv[0]) == "string")
	chooseFileLib = (argv[0].toLowerCase() == "true");

// 2nd command-line argument: Determine whether or not to execute the message listing
// code (true/false)
var executeThisScript = true;
if (typeof(argv[1]) == "boolean")
	executeThisScript = argv[1];
else if (typeof(argv[1]) == "string")
	executeThisScript = (argv[1].toLowerCase() == "true");

// If executeThisScript is true, then create a DDFileAreaChooser object and use
// it to let the user choose a message area.
if (executeThisScript)
{
	var fileAreaChooser = new DDFileAreaChooser();
	fileAreaChooser.SelectFileArea(chooseFileLib);
}

// End of script execution

///////////////////////////////////////////////////////////////////////////////////
// DDFileAreaChooser class stuff

function DDFileAreaChooser()
{
	// Colors
	this.colors = new Object();
	this.colors.areaNum = "\1n\1w\1h";
	this.colors.desc = "\1n\1c";
	this.colors.numItems = "\1b\1h";
	this.colors.header = "\1n\1y\1h";
	this.colors.fileAreaHdr = "\1n\1g";
	this.colors.areaMark = "\1g\1h";
	// Highlighted colors (for lightbar mode)
	this.colors.bkgHighlight = "\1" + "4"; // Blue background
	this.colors.areaNumHighlight = "\1w\1h";
	this.colors.descHighlight = "\1c";
	this.colors.numItemsHighlight = "\1w\1h";
	// Lightbar help line colors
	this.colors.lightbarHelpLineBkg = "\1" + "7";
	this.colors.lightbarHelpLineGeneral = "\1b";
	this.colors.lightbarHelpLineHotkey = "\1r";
	this.colors.lightbarHelpLineParen = "\1m";

	// useLightbarInterface specifies whether or not to use the lightbar
	// interface.  The lightbar interface will still only be used if the
	// user's terminal supports ANSI.
	this.useLightbarInterface = true;

	this.areaNumLen = 4;
	this.descFieldLen = 67; // Description field length

	// Filename base of a header to display above the area list
	this.areaChooserHdrFilenameBase = "fileAreaChgHeader";
	this.areaChooserHdrMaxLines = 5;

	// Set the function pointers for the object
	this.ReadConfigFile = DDFileAreaChooser_ReadConfigFile;
	this.SelectFileArea = DDFileAreaChooser_selectFileArea;
	this.SelectFileArea_Traditional = DDFileAreaChooser_selectFileArea_Traditional;
	this.SelectDirWithinFileLib_Traditional = DDFileAreaChooser_selectDirWithinFileLib_Traditional;
	this.ListFileLibs = DDFileAreaChooser_listFileLibs_Traditional;
	this.ListDirsInFileLib_Traditional = DDFileAreaChooser_listDirsInFileLib_Traditional;
	this.WriteLibListHdrLine = DDFileAreaChooser_writeLibListTopHdrLine;
	this.WriteDirListHdr1Line = DDFileAreaChooser_writeDirListHdr1Line;
	// Lightbar-specific functions
	this.SelectFileArea_Lightbar = DDFileAreaChooser_selectFileArea_Lightbar;
	this.SelectDirWithinFileLib_Lightbar = DDFileAreaChooser_selectDirWithinFileLib_Lightbar;
	this.WriteKeyHelpLine = DDFileAreaChooser_writeKeyHelpLine;
	this.ListScreenfulOfFileLibs = DDFileAreaChooser_listScreenfulOfFileLibs;
	this.WriteFileLibLine = DDFileAreaChooser_writeFileLibLine;
	this.WriteFileLibDirLine = DDFileAreaChooser_writeFileLibDirLine;
	this.updatePageNumInHeader = DDFileAreaChooser_updatePageNumInHeader;
	this.ListScreenfulOfDirs = DDMsgAreaChooser_listScreenfulOfFileDirs;
	// Help screen
	this.ShowHelpScreen = DDFileAreaChooser_showHelpScreen;
	// Misc. functions
	this.NumFilesInDir = DDFileAreaChooser_NumFilesInDir;
	// Function to build the directory printf information for a file lib
	this.BuildFileDirPrintfInfoForLib = DDFileAreaChooser_buildFileDirPrintfInfoForLib;
	// Function to display the header above the area list
	this.DisplayAreaChgHdr = DDFileAreaChooser_DisplayAreaChgHdr;
	this.WriteLightbarKeyHelpErrorMsg = DDFileAreaChooser_WriteLightbarKeyHelpErrorMsg;

	// Read the settings from the config file.
	this.ReadConfigFile();

	// printf strings used for outputting the file libraries
	this.fileLibPrintfStr = " " + this.colors.areaNum + "%4d "
	                      + this.colors.desc + "%-" + this.descFieldLen
	                      + "s " + this.colors.numItems + "%4d";
	this.fileLibHighlightPrintfStr = "\1n" + this.colors.bkgHighlight + " "
	                               + this.colors.areaNumHighlight + "%4d "
	                               + this.colors.descHighlight + "%-" + this.descFieldLen
	                               + "s " + this.colors.numItemsHighlight + "%4d";
	this.fileLibListHdrPrintfStr = this.colors.header + " %5s %-"
	                             + +(this.descFieldLen-2) + "s %6s";
	this.fileDirHdrPrintfStr = this.colors.header + " %5s %-"
	                         + +(this.descFieldLen-3) + "s %-7s";
	// Lightbar mode key help line
	this.lightbarKeyHelpText = "\1n" + this.colors.lightbarHelpLineHotkey
	              + this.colors.lightbarHelpLineBkg + UP_ARROW
				  + "\1n" + this.colors.lightbarHelpLineGeneral
				  + this.colors.lightbarHelpLineBkg + ", "
				  + "\1n" + this.colors.lightbarHelpLineHotkey
				  + this.colors.lightbarHelpLineBkg + DOWN_ARROW
				  + "\1n" + this.colors.lightbarHelpLineGeneral
				  + this.colors.lightbarHelpLineBkg + ", "
				  + "\1n" + this.colors.lightbarHelpLineHotkey
				  + this.colors.lightbarHelpLineBkg + "HOME"
				  + "\1n" + this.colors.lightbarHelpLineGeneral
				  + this.colors.lightbarHelpLineBkg + ", "
				  + "\1n" + this.colors.lightbarHelpLineHotkey
				  + this.colors.lightbarHelpLineBkg + "END"
				  + "\1n" + this.colors.lightbarHelpLineGeneral
				  + this.colors.lightbarHelpLineBkg + ", "
				  + "\1n" + this.colors.lightbarHelpLineHotkey
				  + this.colors.lightbarHelpLineBkg + "#"
				  + "\1n" + this.colors.lightbarHelpLineGeneral
				  + this.colors.lightbarHelpLineBkg + ", "
				  + "\1n" + this.colors.lightbarHelpLineHotkey
				  + this.colors.lightbarHelpLineBkg + "PgUp"
				  + "\1n" + this.colors.lightbarHelpLineGeneral
				  + this.colors.lightbarHelpLineBkg + "/"
				  + "\1n" + this.colors.lightbarHelpLineHotkey
				  + this.colors.lightbarHelpLineBkg + "Dn"
				  + "\1n" + this.colors.lightbarHelpLineGeneral
				  + this.colors.lightbarHelpLineBkg + ", "
				  + "\1n" + this.colors.lightbarHelpLineHotkey
				  + this.colors.lightbarHelpLineBkg + "F"
				  + "\1n" + this.colors.lightbarHelpLineParen
				  + this.colors.lightbarHelpLineBkg + ")"
				  + "\1n" + this.colors.lightbarHelpLineGeneral
				  + this.colors.lightbarHelpLineBkg + "irst pg, "
				  + "\1n" + this.colors.lightbarHelpLineHotkey
				  + this.colors.lightbarHelpLineBkg + "L"
				  + "\1n" + this.colors.lightbarHelpLineParen
				  + this.colors.lightbarHelpLineBkg + ")"
				  + "\1n" + this.colors.lightbarHelpLineGeneral
				  + this.colors.lightbarHelpLineBkg + "ast pg, "
	              + "\1n" + this.colors.lightbarHelpLineHotkey
	              + this.colors.lightbarHelpLineBkg + "CTRL-F"
	              + "\1n" + this.colors.lightbarHelpLineGeneral
	              + this.colors.lightbarHelpLineBkg + ", "
	              + "\1n" + this.colors.lightbarHelpLineHotkey
	              + this.colors.lightbarHelpLineBkg + "/"
	              + "\1n" + this.colors.lightbarHelpLineGeneral
	              + this.colors.lightbarHelpLineBkg + ", "
	              + "\1n" + this.colors.lightbarHelpLineHotkey
	              + this.colors.lightbarHelpLineBkg + "N"
	              + "\1n" + this.colors.lightbarHelpLineGeneral
	              + this.colors.lightbarHelpLineBkg + ", "
	              + "\1n" + this.colors.lightbarHelpLineHotkey
				  + this.colors.lightbarHelpLineBkg + "Q"
				  + "\1n" + this.colors.lightbarHelpLineParen
				  + this.colors.lightbarHelpLineBkg + ")"
				  + "\1n" + this.colors.lightbarHelpLineGeneral
				  + this.colors.lightbarHelpLineBkg + "uit, "
				  + "\1n" + this.colors.lightbarHelpLineHotkey
				  + this.colors.lightbarHelpLineBkg + "?";
	// Pad the lightbar key help text on either side to center it on the screen
	// (but leave off the last character to avoid screen drawing issues)
	var helpTextLen = console.strlen(this.lightbarKeyHelpText);
	var helpTextStartCol = (console.screen_columns/2) - (helpTextLen/2);
	this.lightbarKeyHelpText = "\1n" + this.colors.lightbarHelpLineBkg
	                         + format("%" + +(helpTextStartCol) + "s", "")
							 + this.lightbarKeyHelpText + "\1n"
							 + this.colors.lightbarHelpLineBkg;
	var numTrailingChars = console.screen_columns - (helpTextStartCol+helpTextLen) - 1;
	this.lightbarKeyHelpText += format("%" + +(numTrailingChars) + "s", "") + "\1n";

	// this.fileDirListPrintfInfo will be an array of printf strings
	// for the file directories in the file libraries.  The index is the
	// file library index.  The file directory printf information is
	// created on the fly the first time the user lists directories for
	// a file library.
	this.fileDirListPrintfInfo = new Array();

	// areaChangeHdrLines is an array of text lines to use as a header to display
	// above the message area changer lists.
	this.areaChangeHdrLines = loadTextFileIntoArray(this.areaChooserHdrFilenameBase, this.areaChooserHdrMaxLines);
}

// For the DDFileAreaChooser class: Lets the user choose a file area.
//
// Parameters:
//  pChooseLib: Boolean - Whether or not to choose the file library.  If false,
//              then this will allow choosing a directory within the user's
//              current file library.  This is optional; defaults to true.
function DDFileAreaChooser_selectFileArea(pChooseLib)
{
	if (this.useLightbarInterface && console.term_supports(USER_ANSI))
		this.SelectFileArea_Lightbar(pChooseLib);
	else
		this.SelectFileArea_Traditional(pChooseLib);
}

// For the DDFileAreaChooser class: Traditional user interface for
// letting the user choose a file area
//
// Parameters:
//  pChooseLib: Boolean - Whether or not to choose the file library.  If false,
//              then this will allow choosing a directory within the user's
//              current file library.  This is optional; defaults to true.
function DDFileAreaChooser_selectFileArea_Traditional(pChooseLib)
{
	// If there are no file libraries, then don't let the user
	// choose one.
	if (file_area.lib_list.length == 0)
	{
		console.clear("\1n");
		console.print("\1y\1hThere are no file libraries.\r\n\1p");
		return;
	}

	var curLibIdx = 0;
	var curDirIdx = 0;
	if (typeof(bbs.curdir_code) == "string")
	{
		curLibIdx = file_area.dir[bbs.curdir_code].lib_index;
		curDirIdx = file_area.dir[bbs.curdir_code].index;
	}

	var chooseLib = (typeof(pChooseLib) == "boolean" ? pChooseLib : true);
	if (chooseLib)
	{
		// Show the file libraries & directories and let the user choose one.
		var selectedLib = 0; // The user's selected file library
		var selectedDir = 0; // The user's selected file directory
		var libSearchText = "";
		var continueChooseFileLib = true;
		while (continueChooseFileLib)
		{
			// Clear the BBS command string to make sure there are no extra
			// commands in there that could cause weird things to happen.
			bbs.command_str = "";

			console.clear("\1n");
			this.DisplayAreaChgHdr(1);
			if (this.areaChangeHdrLines.length > 0)
				console.crlf();
			this.ListFileLibs(libSearchText);
			console.print("\1n\1b\1hþ \1n\1cWhich, \1hQ\1n\1cuit, \1hCTRL-F\1n\1c, \1h/\1n\1c, or [\1h" + +(curLibIdx+1) + "\1n\1c]:\1h ");
			// Accept Q (quit) or a file library number
			selectedLib = console.getkeys("QN/" + CTRL_F, file_area.lib_list.length);

			// If the user just pressed enter (selectedLib would be blank),
			// default to the current library.
			if (selectedLib.toString() == "")
				selectedLib = curLibIdx + 1;

			// If the user chose to quit, then set continueChooseFileLib to
			// false so we'll exit the loop.  Otherwise, let the user chose
			// a dir within the library.
			if (selectedLib.toString() == "Q")
				continueChooseFileLib = false;
			else if ((selectedLib.toString() == "/") || (selectedLib.toString() == CTRL_F))
			{
				console.crlf();
				var searchPromptText = "\1n\1c\1hSearch\1g: \1n";
				console.print(searchPromptText);
				var searchText = console.getstr("", console.screen_columns-strip_ctrl(searchPromptText).length-1, K_UPPER|K_NOCRLF|K_GETSTR|K_NOSPIN|K_LINE);
				if (searchText.length > 0)
					libSearchText = searchText;
			}
			else
			{
				libSearchText = "";

				if (selectedLib-1 == curLibIdx)
					selectedDir = curDirIdx + 1;
				else
					selectedDir = 1;
				continueChooseFileLib = !this.SelectDirWithinFileLib_Traditional(selectedLib, selectedDir);
			}
		}
	}
	else
	{
		// Don't choose a library, just a directory within the user's current library.
		this.SelectDirWithinFileLib_Traditional(curLibIdx, curDirIdx);
	}
}

// For the DDFileAreaChooser class: Lets the user select a file area (directory)
// within a specified file library - Traditional user interface.
//
// Parameters:
//  pLibNumber: The file library number
//  pSelectedDir: The currently-selected file directory
//
// Return value: Boolean - Whether or not the user chose a file area.
function DDFileAreaChooser_selectDirWithinFileLib_Traditional(pLibNumber, pSelectedDir)
{
	// If pLibNumber is invalid, then just return false.
	if (pLibNumber <= 0)
		return false;

	var userChoseAnArea = false;
	var libIdx = pLibNumber - 1;

	// If there are no file directories in the given file libraary, then show
	// an error and return.
	if (file_area.lib_list[libIdx].dir_list.length == 0)
	{
		console.clear("\1n");
		console.print("\1y\1hThere are no directories in this library.\r\n\1p");
		return false;
	}

	// Ensure that the file directory printf information is created for
	// this file library.
	this.BuildFileDirPrintfInfoForLib(libIdx);

	// Set the default directory #: The current directory, or if the
	// user chose a different file library, then this should be set
	// to the first directory.
	function getDefaultDir(pDir)
	{
		var defaultDir = 0;
		if (typeof(pDir) == "number")
			defaultDir = pDir;
		else if (typeof(bbs.curdir_code) == "string")
		{
			if (libIdx != file_area.dir[bbs.curdir_code].lib_index)
				defaultDir = 1;
			else
				defaultDir = file_area.dir[bbs.curdir_code].index;
		}
		return defaultDir;
	}

	var defaultDir = getDefaultDir(pSelectedDir);
	var searchText = "";
	var numDirsListed = 0;
	var continueOn = false;
	do
	{
		console.clear("\1n");
		this.DisplayAreaChgHdr(1);
		if (this.areaChangeHdrLines.length > 0)
			console.crlf();
		numDirsListed = this.ListDirsInFileLib_Traditional(libIdx, defaultDir - 1, searchText);
		if ((numDirsListed > 0) && (typeof(pSelectedDir) == "number") && (pSelectedDir >= 1) && (pSelectedDir <= numDirsListed))
			defaultDir = getDefaultDir(pSelectedDir);
		if (defaultDir >= 1)
			console.print("\1n\1b\1hþ \1n\1cWhich, \1hQ\1n\1cuit, \1hCTRL-F\1n\1c, \1h/\1n\1c, or [\1h" + defaultDir + "\1n\1c]: \1h");
		else
			console.print("\1n\1b\1hþ \1n\1cWhich, \1hQ\1n\1cuit, \1hCTRL-F\1n\1c, \1h/\1n\1c: \1h");
		// Accept Q (quit), / or CTRL_F to search, or a file directory number
		var selectedDir = console.getkeys("Q/" + CTRL_F, file_area.lib_list[libIdx].dir_list.length);

		// If the user just pressed enter (selectedDir would be blank),
		// default the selected directory.
		if (selectedDir.toString() == "Q")
			continueOn = false;
		else if (selectedDir.toString() == "")
			selectedDir = defaultDir;
		else if ((selectedDir == "/") || (selectedDir == CTRL_F))
		{
			// Search
			console.crlf();
			var searchPromptText = "\1n\1c\1hSearch\1g: \1n";
			console.print(searchPromptText);
			searchText = console.getstr("", console.screen_columns-strip_ctrl(searchPromptText).length-1, K_UPPER|K_NOCRLF|K_GETSTR|K_NOSPIN|K_LINE);
			console.print("\1n");
			console.crlf();
			if (searchText.length > 0)
				defaultDir = -1;
			else
				defaultDir = getDefaultDir(pSelectedDir);
			continueOn = true;
			console.line_counter = 0; // To avoid pausing before the clear screen
		}

		// If the user chose a directory, then set the user's file directory.
		if (selectedDir > 0)
		{
			continueOn = false;
			bbs.curdir_code = file_area.lib_list[libIdx].dir_list[selectedDir-1].code;
			userChoseAnArea = true;
		}
	} while (continueOn);

	return userChoseAnArea;
}

// For the DDFileAreaChooser class: Traditional user interface for listing
// the file libraries
//
// Parameters:
//  pSearchText: Text to search for in the file library names/descriptions.
//               If blank or not a string, all will be displayed.
//
// Return value: The number of directories listed
function DDFileAreaChooser_listFileLibs_Traditional(pSearchText)
{
	var searchText = (typeof(pSearchText) == "string" ? pSearchText.toUpperCase() : "");

	// Print the list header
	printf(this.fileLibListHdrPrintfStr, "Lib #", "Description", "# Dirs");
	console.crlf();
	console.print("\1n");
	// Print the information for each file library
	var numDirsListed = 0;
	var printIt = true;
	var currentDir = false;
	for (var i = 0; i < file_area.lib_list.length; ++i)
	{
		if (searchText.length > 0)
			printIt = ((file_area.lib_list[i].name.toUpperCase().indexOf(searchText) >= 0) || (file_area.lib_list[i].description.toUpperCase().indexOf(searchText) >= 0));
		else
			printIt = true;

		if (printIt)
		{
			++numDirsListed;
			// Print the library information.
			var curLibIdx = 0;
			if (typeof(bbs.curdir_code) == "string")
				curLibIdx = file_area.dir[bbs.curdir_code].lib_index;
			console.print(i == curLibIdx ? this.colors.areaMark + "*" : " ");
			printf(this.fileLibPrintfStr, +(i+1),
			file_area.lib_list[i].description.substr(0, this.descFieldLen),
			file_area.lib_list[i].dir_list.length);
			console.crlf();
		}
	}
	return numDirsListed;
}

// For the DDFileAreaChooser class: Traditional user interface for listing
// the directories in a file library
//
// Parameters:
//  pLibIndex: The index of the file library (0-based)
//  pMarkIndex: An index of a file library to display the "current" mark
//              next to.  This is optional.
//  pSearchText: Text to search for in the file directories (blank or none to list all)
//
// Return value: The number of file directories listed
function DDFileAreaChooser_listDirsInFileLib_Traditional(pLibIndex, pMarkIndex, pSearchText)
{
	// Set libIndex, the library index
	var libIndex = 0;
	if (typeof(pLibIndex) == "number")
		libIndex = pLibIndex;
	else if (typeof(bbs.curdir_code) == "string")
		libIndex = file_area.dir[bbs.curdir_code].lib_index;

	// Set markIndex, the index of the item to highlight
	var markIndex = -1;
	if (typeof(pMarkIndex) == "number")
	{
		if ((pMarkIndex >= 0) && (pMarkIndex < file_area.lib_list[libIndex].dir_list.length))
			markIndex = pMarkIndex;
	}

	var searchText = (typeof(pSearchText) == "string" ? pSearchText.toUpperCase() : "");

	// Ensure that the file directory printf information is created for
	// this file library.
	this.BuildFileDirPrintfInfoForLib(libIndex);

	// Print the header lines
	console.print(this.colors.fileAreaHdr + "Directories of \1h" +
	              file_area.lib_list[libIndex].description);
	console.crlf();
	printf(this.fileDirHdrPrintfStr, "Dir #", "Description", "# Files");
	console.crlf();
	console.print("\1n");
	var numDirsListed = 0;
	var printIt = true;
	for (var i = 0; i < file_area.lib_list[libIndex].dir_list.length; ++i)
	{
		if (searchText.length > 0)
			printIt = ((file_area.lib_list[libIndex].dir_list[i].name.toUpperCase().indexOf(searchText) >= 0) || (file_area.lib_list[libIndex].dir_list[i].description.toUpperCase().indexOf(searchText) >= 0));
		else
			printIt = true;
		if (printIt)
		{
			++numDirsListed;
			// See if this is the currently-selected directory.
			console.print(markIndex > -1 && i == markIndex ? this.colors.areaMark + "*" : " ");
			printf(this.fileDirListPrintfInfo[libIndex].printfStr, +(i+1),
			       file_area.lib_list[libIndex].dir_list[i].description.substr(0, this.descFieldLen),
			       this.fileDirListPrintfInfo[libIndex].fileCounts[i]);
			console.crlf();
		}
	}
	return numDirsListed;
}

// For the DDFileAreaChooser class: Outputs the header line to appear above
// the list of file libraries.
//
// Parameters:
//  pNumPages: The number of pages (a number).  This is optional; if this is
//             not passed, then it won't be used.
//  pPageNum: The page number.  This is optional; if this is not passed,
//            then it won't be used.
function DDFileAreaChooser_writeLibListTopHdrLine(pNumPages, pPageNum)
{
	var descStr = "Description";
	if ((typeof(pPageNum) == "number") && (typeof(pNumPages) == "number"))
		descStr += "    (Page " + pPageNum + " of " + pNumPages + ")";
	else if ((typeof(pPageNum) == "number") && (typeof(pNumPages) != "number"))
		descStr += "    (Page " + pPageNum + ")";
	else if ((typeof(pPageNum) != "number") && (typeof(pNumPages) == "number"))
		descStr += "    (" + pNumPages + (pNumPages == 1 ? " page)" : " pages)");
	printf(this.fileLibListHdrPrintfStr, "Lib #", descStr, "# Dirs");
	console.cleartoeol("\1n");
}

// For the DDFileAreaChooser class: Outputs the first header line to appear
// above the directory list for a file library.
//
// Parameters:
//  pLibIndex: The index of the file library (assumed to be valid)
//  pNumPages: The number of pages (a number).  This is optional; if this is
//             not passed, then it won't be used.
//  pPageNum: The page number.  This is optional; if this is not passed,
//            then it won't be used.
function DDFileAreaChooser_writeDirListHdr1Line(pLibIndex, pNumPages, pPageNum)
{
  var descLen = 40;
  var descFormatStr = this.colors.fileAreaHdr + "Directories of \1h%-" + descLen + "s     \1n"
                    + this.colors.fileAreaHdr;
  if ((typeof(pPageNum) == "number") && (typeof(pNumPages) == "number"))
    descFormatStr += "(Page " + pPageNum + " of " + pNumPages + ")";
  else if ((typeof(pPageNum) == "number") && (typeof(pNumPages) != "number"))
    descFormatStr += "(Page " + pPageNum + ")";
  else if ((typeof(pPageNum) != "number") && (typeof(pNumPages) == "number"))
    descFormatStr += "(" + pNumPages + (pNumPages == 1 ? " page)" : " pages)");
  printf(descFormatStr, file_area.lib_list[pLibIndex].description.substr(0, descLen));
  console.cleartoeol("\1n");
}

// Lightbar functions

// For the DDFileAreaChooser class: Lightbar interface for letting the user
// choose a file library and directory
//
// Parameters:
//  pChooseLib: Boolean - Whether or not to choose the file library.  If false,
//              then this will allow choosing a directory within the user's
//              current file library.  This is optional; defaults to true.
function DDFileAreaChooser_selectFileArea_Lightbar(pChooseLib)
{
	// If there are file libraries, then don't let the user
	// choose one.
	if (file_area.lib_list.length == 0)
	{
		console.clear("\1n");
		console.print("\1y\1hThere are no file libraries.\r\n\1p");
		return;
	}

	var chooseLib = (typeof(pChooseLib) == "boolean" ? pChooseLib : true);
	if (chooseLib)
	{
		// Returns the index of the bottommost file library that can be displayed
		// on the screen.
		//
		// Parameters:
		//  pTopLibIndex: The index of the topmost file library displayed on screen
		//  pNumItemsPerPage: The number of items per page
		function getBottommostLibIndex(pTopLibIndex, pNumItemsPerPage)
		{
			var bottomLibIndex = pTopLibIndex + pNumItemsPerPage - 1;
			// If bottomLibIndex is beyond the last index, then adjust it.
			if (bottomLibIndex >= file_area.lib_list.length)
				bottomLibIndex = file_area.lib_list.length - 1;
			return bottomLibIndex;
		}
		
		// For doing the "next" search result
		function nextLibSearchFoundItem(srchObj, listStartRow, listEndRow, selectedLibIndex, topFileLibIndex, chooserObj)
		{
			var retObj = {
				differentPage: false,
				topFileLibIndex: srchObj.pageTopIdx,
				selectedLibIndex: srchObj.itemIdx
			};

			// For screen refresh optimization, don't redraw the whole
			// list if the result is on the same page
			if (srchObj.pageTopIdx != topFileLibIndex)
			{
				retObj.differentPage = true;
				chooserObj.updatePageNumInHeader(srchObj.pageNum, numPages, true, false);
				chooserObj.ListScreenfulOfFileLibs(retObj.topFileLibIndex, listStartRow, listEndRow, false, true, retObj.selectedLibIndex);
			}
			else
			{
				if (srchObj.itemIdx != selectedLibIndex)
				{
					var screenY = listStartRow + (selectedLibIndex - topFileLibIndex);
					console.gotoxy(1, screenY);
					chooserObj.WriteFileLibLine(selectedLibIndex, false);
					retObj.selectedLibIndex = srchObj.itemIdx;
					screenY = listStartRow + (retObj.selectedLibIndex - topFileLibIndex);
					console.gotoxy(1, screenY);
					chooserObj.WriteFileLibLine(retObj.selectedLibIndex, true);
				}
			}

			return retObj;
		}


		// Figure out the index of the user's currently-selected file library
		var selectedLibIndex = 0;
		if (typeof(bbs.curdir_code) == "string")
			selectedLibIndex = file_area.dir[bbs.curdir_code].lib_index;

		var listStartRow = 2+this.areaChangeHdrLines.length; // The row on the screen where the list will start
		var listEndRow = console.screen_rows - 1; // Row on screen where list will end
		var topFileLibIndex = 0;    // The index of the file library at the top of the list

		// Figure out the index of the last file library to appear on the screen.
		var numItemsPerPage = listEndRow - listStartRow + 1;
		var bottomFileLibIndex = getBottommostLibIndex(topFileLibIndex, numItemsPerPage);
		// Figure out how many pages are needed to list all the file areas.
		var numPages = Math.ceil(file_area.lib_list.length / numItemsPerPage);
		var pageNum = 1;
		// Figure out the top index for the last page.
		var topIndexForLastPage = (numItemsPerPage * numPages) - numItemsPerPage;

		// If the highlighted row is beyond the current screen, then
		// go to the appropriate page.
		if (selectedLibIndex > bottomFileLibIndex)
		{
			var nextPageTopIndex = 0;
			while (selectedLibIndex > bottomFileLibIndex)
			{
				nextPageTopIndex = topFileLibIndex + numItemsPerPage;
				if (nextPageTopIndex < file_area.lib_list.length)
				{
					// Adjust topFileLibIndex and bottomFileLibIndex, and
					// refresh the list on the screen.
					topFileLibIndex = nextPageTopIndex;
					pageNum = calcPageNum(topFileLibIndex, numItemsPerPage);
					bottomFileLibIndex = getBottommostLibIndex(topFileLibIndex, numItemsPerPage);
				}
				else
					break;
			}

			// If we didn't find the correct page for some reason, then set the
			// variables to display page 1 and select the first file library.
			var foundCorrectPage = ((topFileLibIndex < file_area.lib_list.length) &&
			                        (selectedLibIndex >= topFileLibIndex) && (selectedLibIndex <= bottomFileLibIndex));
			if (!foundCorrectPage)
			{
				topFileLibIndex = 0;
				pageNum = calcPageNum(topFileLibIndex, numItemsPerPage);
				bottomFileLibIndex = getBottommostLibIndex(topFileLibIndex, numItemsPerPage);
				selectedLibIndex = 0;
			}
		}

		// Clear the screen, write the header, help line, and library list header, and output
		// a screenful of file libraries.
		console.clear("\1n");
		this.DisplayAreaChgHdr(1);
		this.WriteKeyHelpLine();

		var curpos = new Object();
		curpos.x = 1;
		curpos.y = 1+this.areaChangeHdrLines.length;
		console.gotoxy(curpos);
		this.WriteLibListHdrLine(numPages, pageNum);
		this.ListScreenfulOfFileLibs(topFileLibIndex, listStartRow, listEndRow, false, false);
		// Start of the input loop.
		var highlightScrenRow = 0; // The row on the screen for the highlighted library
		var userInput = "";        // Will store a keypress from the user
		var retObj = null;        // To store the return value of choosing a file area
		var lastSearchText = "";
		var lastSearchFoundIdx = -1;
		var continueChoosingFileArea = true;
		while (continueChoosingFileArea)
		{
			// Highlight the currently-selected file library
			highlightScrenRow = listStartRow + (selectedLibIndex - topFileLibIndex);
			curpos.y = highlightScrenRow;
			if ((highlightScrenRow > 0) && (highlightScrenRow < console.screen_rows))
			{
				console.gotoxy(1, highlightScrenRow);
				this.WriteFileLibLine(selectedLibIndex, true);
			}

			// Get a key from the user (upper-case) and take action based upon it.
			userInput = getKeyWithESCChars(K_UPPER | K_NOCRLF);
			switch (userInput)
			{
				case KEY_UP: // Move up one file library in the list
					if (selectedLibIndex > 0)
					{
						// If the previous library index is on the previous page, then
						// display the previous page.
						var previousGrpIndex = selectedLibIndex - 1;
						if (previousGrpIndex < topFileLibIndex)
						{
							// Adjust topFileLibIndex and bottomFileLibIndex, and
							// refresh the list on the screen.
							topFileLibIndex -= numItemsPerPage;
							pageNum = calcPageNum(topFileLibIndex, numItemsPerPage);
							bottomFileLibIndex = getBottommostLibIndex(topFileLibIndex, numItemsPerPage);
							this.updatePageNumInHeader(pageNum, numPages, true, false);
							this.ListScreenfulOfFileLibs(topFileLibIndex, listStartRow, listEndRow, false, true);
						}
						else
						{
							// Display the current line un-highlighted.
							console.gotoxy(1, curpos.y);
							this.WriteFileLibLine(selectedLibIndex, false);
						}
						selectedLibIndex = previousGrpIndex;
					}
					break;
				case KEY_DOWN: // Move down one file library in the list
					if (selectedLibIndex < file_area.lib_list.length - 1)
					{
						// If the next library index is on the next page, then display
						// the next page.
						var nextGrpIndex = selectedLibIndex + 1;
						if (nextGrpIndex > bottomFileLibIndex)
						{
							// Adjust topFileLibIndex and bottomFileLibIndex, and
							// refresh the list on the screen.
							topFileLibIndex += numItemsPerPage;
							pageNum = calcPageNum(topFileLibIndex, numItemsPerPage);
							bottomFileLibIndex = getBottommostLibIndex(topFileLibIndex, numItemsPerPage);
							this.updatePageNumInHeader(pageNum, numPages, true, false);
							this.ListScreenfulOfFileLibs(topFileLibIndex, listStartRow, listEndRow, false, true);
						}
						else
						{
							// Display the current line un-highlighted.
							console.gotoxy(1, curpos.y);
							this.WriteFileLibLine(selectedLibIndex, false);
						}
						selectedLibIndex = nextGrpIndex;
					}
					break;
				case KEY_HOME: // Go to the top library on the screen
					if (selectedLibIndex > topFileLibIndex)
					{
						// Display the current line un-highlighted, then adjust
						// selectedLibIndex.
						console.gotoxy(1, curpos.y);
						this.WriteFileLibLine(selectedLibIndex, false);
						selectedLibIndex = topFileLibIndex;
						// Note: curpos.y is set at the start of the while loop.
					}
					break;
				case KEY_END: // Go to the bottom message library on the screen
					if (selectedLibIndex < bottomFileLibIndex)
					{
						// Display the current line un-highlighted, then adjust
						// selectedLibIndex.
						console.gotoxy(1, curpos.y);
						this.WriteFileLibLine(selectedLibIndex, false);
						selectedLibIndex = bottomFileLibIndex;
						// Note: curpos.y is set at the start of the while loop.
					}
					break;
				case KEY_ENTER: // Select the currently-highlighted file library
					retObj = this.SelectDirWithinFileLib_Lightbar(selectedLibIndex);
					// If the user chose an area, then set the user's file directory,
					// and don't continue the input loop anymore.
					if (retObj.fileDirChosen)
					{
						bbs.curdir_code = retObj.fileDirCode;
						continueChoosingFileArea = false;
					}
					else
					{
						// An area was not chosen, so we'll have to re-draw
						// the header and list of file libraries.
						console.gotoxy(1, 1+this.areaChangeHdrLines.length);
						this.WriteLibListHdrLine(numPages, pageNum);
						this.ListScreenfulOfFileLibs(topFileLibIndex, listStartRow, listEndRow, false, true);
					}
					break;
				case KEY_PAGE_DOWN: // Go to the next page
					var nextPageTopIndex = topFileLibIndex + numItemsPerPage;
					if (nextPageTopIndex < file_area.lib_list.length)
					{
						// Adjust topFileLibIndex and bottomFileLibIndex, and
						// refresh the list on the screen.
						topFileLibIndex = nextPageTopIndex;
						pageNum = calcPageNum(topFileLibIndex, numItemsPerPage);
						bottomFileLibIndex = getBottommostLibIndex(topFileLibIndex, numItemsPerPage);
						this.updatePageNumInHeader(pageNum, numPages, true, false);
						this.ListScreenfulOfFileLibs(topFileLibIndex, listStartRow, listEndRow, false, true);
						selectedLibIndex = topFileLibIndex;
					}
					break;
				case KEY_PAGE_UP: // Go to the previous page
					var prevPageTopIndex = topFileLibIndex - numItemsPerPage;
					if (prevPageTopIndex >= 0)
					{
						// Adjust topFileLibIndex and bottomFileLibIndex, and
						// refresh the list on the screen.
						topFileLibIndex = prevPageTopIndex;
						pageNum = calcPageNum(topFileLibIndex, numItemsPerPage);
						bottomFileLibIndex = getBottommostLibIndex(topFileLibIndex, numItemsPerPage);
						this.updatePageNumInHeader(pageNum, numPages, true, false);
						this.ListScreenfulOfFileLibs(topFileLibIndex, listStartRow, listEndRow, false, true);
						selectedLibIndex = topFileLibIndex;
					}
					break;
				case 'F': // Go to the first page
					if (topFileLibIndex > 0)
					{
						topFileLibIndex = 0;
						pageNum = calcPageNum(topFileLibIndex, numItemsPerPage);
						bottomFileLibIndex = getBottommostLibIndex(topFileLibIndex, numItemsPerPage);
						this.updatePageNumInHeader(pageNum, numPages, true, false);
						this.ListScreenfulOfFileLibs(topFileLibIndex, listStartRow, listEndRow, false, true);
						selectedLibIndex = 0;
					}
					break;
				case 'L': // Go to the last page
					if (topFileLibIndex < topIndexForLastPage)
					{
						topFileLibIndex = topIndexForLastPage;
						pageNum = calcPageNum(topFileLibIndex, numItemsPerPage);
						bottomFileLibIndex = getBottommostLibIndex(topFileLibIndex, numItemsPerPage);
						this.updatePageNumInHeader(pageNum, numPages, true, false);
						this.ListScreenfulOfFileLibs(topFileLibIndex, listStartRow, listEndRow, false, true);
						selectedLibIndex = topIndexForLastPage;
					}
					break;
				case KEY_ESC: // Quit
				case 'Q': // Quit
					continueChoosingFileArea = false;
					break;
				case '?': // Show help
					this.ShowHelpScreen(true, true);
					console.pause();
					// Refresh the screen
					this.DisplayAreaChgHdr(1);
					this.WriteKeyHelpLine();
					console.gotoxy(1, 1+this.areaChangeHdrLines.length);
					this.WriteLibListHdrLine(numPages, pageNum);
					this.ListScreenfulOfFileLibs(topFileLibIndex, listStartRow, listEndRow, false, true);
					break;
				case '/': // Start of find (search)
				case CTRL_F: // Start of find
					console.gotoxy(1, console.screen_rows);
					console.cleartoeol("\1n");
					console.gotoxy(1, console.screen_rows);
					var promptText = "Search: ";
					console.print(promptText);
					var searchText = getStrWithTimeout(K_UPPER|K_NOCRLF|K_GETSTR|K_NOSPIN|K_LINE, console.screen_columns - promptText.length - 1, SEARCH_TIMEOUT_MS);
					// If the user entered text, then do the search, and if found,
					// found, go to the page and select the item indicated by the
					// search.
					if (searchText.length > 0)
					{
						var srchObj = getPageNumFromSearch(searchText, numItemsPerPage, false, 0);
						if (srchObj.pageNum > 0)
						{
							lastSearchText = searchText;
							lastSearchFoundIdx = srchObj.itemIdx;

							// For screen refresh optimization, don't redraw the whole
							// list if the result is on the same page
							if (srchObj.pageTopIdx != topFileLibIndex)
							{
								topFileLibIndex = srchObj.pageTopIdx;
								selectedLibIndex = srchObj.itemIdx;
								pageNum = srchObj.pageNum;
								this.updatePageNumInHeader(pageNum, numPages, true, false);
								this.ListScreenfulOfFileLibs(topFileLibIndex, listStartRow, listEndRow, false, true);
							}
							else
							{
								if (srchObj.itemIdx != selectedLibIndex)
								{
									var screenY = listStartRow + (selectedLibIndex - topFileLibIndex);
									console.gotoxy(1, screenY);
									this.WriteFileLibLine(selectedLibIndex, false);
									selectedLibIndex = srchObj.itemIdx;
									screenY = listStartRow + (selectedLibIndex - topFileLibIndex);
									console.gotoxy(1, screenY);
									this.WriteFileLibLine(selectedLibIndex, true);
								}
							}
						}
						else
							this.WriteLightbarKeyHelpErrorMsg("Not found", false);
					}
					this.WriteKeyHelpLine();
					break;
				case 'N': // Next search result (requires an existing search term)
					if ((lastSearchText.length > 0) && (lastSearchFoundIdx > -1))
					{
						// Do the search, and if found, go to the page and select the item
						// indicated by the search.
						var srchObj = getPageNumFromSearch(lastSearchText, numItemsPerPage, false, lastSearchFoundIdx+1);
						lastSearchFoundIdx = srchObj.itemIdx;
						if (srchObj.pageNum > 0)
						{
							var foundItemRetObj = nextLibSearchFoundItem(srchObj, listStartRow, listEndRow, selectedLibIndex, topFileLibIndex, this);
							if (foundItemRetObj.differentPage)
							{
								pageNum = srchObj.pageNum;
								topFileLibIndex = foundItemRetObj.topFileLibIndex;
								bottomFileLibIndex = getBottommostLibIndex(topFileLibIndex, numItemsPerPage);
							}
							selectedLibIndex = foundItemRetObj.selectedLibIndex;
						}
						else
						{
							// Not found - Wrap around and start at 0 again
							var srchObj = getPageNumFromSearch(lastSearchText, numItemsPerPage, false, 0);
							lastSearchFoundIdx = srchObj.itemIdx;
							var foundItemRetObj = nextLibSearchFoundItem(srchObj, listStartRow, listEndRow, selectedLibIndex, topFileLibIndex, this);
							if (foundItemRetObj.selectedLibIndex != selectedLibIndex)
							{
								if (foundItemRetObj.differentPage)
								{
									pageNum = srchObj.pageNum;
									topFileLibIndex = foundItemRetObj.topFileLibIndex;
									bottomFileLibIndex = getBottommostLibIndex(topFileLibIndex, numItemsPerPage);
								}
								selectedLibIndex = foundItemRetObj.selectedLibIndex;
							}
							else
								this.WriteLightbarKeyHelpErrorMsg("No others found", true);
						}
					}
					else
						this.WriteLightbarKeyHelpErrorMsg("There is no previous search", true);
					break;
				default:
					// If the user entered a numeric digit, then treat it as
					// the start of the file library number.
					if (userInput.match(/[0-9]/))
					{
						var originalCurpos = curpos;

						// Put the user's input back in the input buffer to
						// be used for getting the rest of the message number.
						console.ungetstr(userInput);
						// Move the cursor to the bottom of the screen and
						// prompt the user for the message number.
						console.gotoxy(1, console.screen_rows);
						console.clearline("\1n");
						console.print("\1cChoose library #: \1h");
						userInput = console.getnum(file_area.lib_list.length);
						// If the user made a selection, then let them choose a
						// directory from the file library.
						if (userInput > 0)
						{
							var selectedLibIndex = userInput - 1;
							var retObj = this.SelectDirWithinFileLib_Lightbar(selectedLibIndex);
							// If the user chose a sub-board, then set the user's
							// file directory, and don't continue the input loop anymore.
							if (retObj.fileDirChosen)
							{
								bbs.curdir_code = retObj.fileDirCode;
								continueChoosingFileArea = false;
							}
							else
							{
								// A sub-board was not chosen, so we'll have to re-draw
								// the header and list of file libraries.
								console.gotoxy(1, 1+this.areaChangeHdrLines.length);
								this.WriteLibListHdrLine(numPages, pageNum);
								this.ListScreenfulOfFileLibs(topFileLibIndex, listStartRow, listEndRow, false, true);
							}
						}
						else
						{
							// The user didn't make a selection.  So, we need to refresh
							// the screen due to everything being moved up one line.
							this.WriteKeyHelpLine();
							console.gotoxy(1, 1+this.areaChangeHdrLines.length);
							this.WriteLibListHdrLine(numPages, pageNum);
							this.ListScreenfulOfFileLibs(topFileLibIndex, listStartRow, listEndRow, false, true);
						}
					}
					break;
			}
		}
	}
	else
	{
		// Don't choose a library, just let the user choose a directory within
		// their current library.
		retObj = this.SelectDirWithinFileLib_Lightbar(selectedLibIndex);
		// If the user chose an area, then set the user's file directory
		if (retObj.fileDirChosen)
			bbs.curdir_code = retObj.fileDirCode;
	}
}

// For the DDFileAreaChooser class: Lightbar interface for letting the user
// choose a directory within a file library.
//
// Parameters:
//  pLibIndex: The index of the file library to choose from.  This is
//             optional; if not specified, the user's current directory will be used.
//  pHighlightIndex: An index of a file directory to highlight.  This
//                   is optional; if left off, this will default to
//                   the current sub-board.
//
// Return value: An object containing the following values:
//               fileDirChosen: Boolean - Whether or not a sub-board was chosen.
//               fileDirIdx: Numeric - The index of the file dir that was chosen (if any).
//                           Will be -1 if none chosen.
//               fileDirCode: The internal code of the file directory chosen ("" if none chosen)
function DDFileAreaChooser_selectDirWithinFileLib_Lightbar(pLibIndex, pHighlightIndex)
{
	// Create the return object.
	var retObj = {
		fileDirChosen: false,
		fileDirIdx: -1,
		fileDirCode: ""
	};

	var libIndex = 0;
	if (typeof(pLibIndex) == "number")
		libIndex = pLibIndex;
	else if (typeof(bbs.curdir_code) == "string")
		libIndex = file_area.dir[bbs.curdir_code].lib_index;
	// Double-check libIndex
	if (libIndex < 0)
		libIndex = 0;
	else if (libIndex >= file_area.lib_list.length)
		libIndex = file_area.lib_list.length - 1;

	// If there are no sub-boards in the given file library, then show
	// an error and return.
	if (file_area.lib_list[libIndex].dir_list.length == 0)
	{
		console.clear("\1n");
		console.print("\1y\1hThere are no directories in the chosen library.\r\n\1p");
		return retObj;
	}
	
	var highlightIndex = 0;
	if ((pHighlightIndex != null) && (typeof(pHighlightIndex) == "number"))
		highlightIndex = pHighlightIndex;
	else if (typeof(bbs.curdir_code) == "string")
		highlightIndex = file_area.dir[bbs.curdir_code].index;
	// Sanity checking for highlightIndex
	if (typeof(bbs.curdir_code) == "string")
	{
		if (libIndex != file_area.dir[bbs.curdir_code].lib_index)
			highlightIndex = 0;
	}
	if (highlightIndex < 0)
		highlightIndex = 0;
	else if (highlightIndex >= file_area.lib_list[libIndex].dir_list.length)
		highlightIndex = file_area.lib_list[libIndex].dir_list.length - 1;

	// Ensure that the file directory printf information is created for
	// this file library.
	this.BuildFileDirPrintfInfoForLib(libIndex);

	// Returns the index of the bottommost directory that can be displayed on
	// the screen.
	//
	// Parameters:
	//  pTopDirIndex: The index of the topmost directory displayed on screen
	//  pNumItemsPerPage: The number of items per page
	function getBottommostDirIndex(pTopDirIndex, pNumItemsPerPage)
	{
		var bottomDirIndex = pTopDirIndex + pNumItemsPerPage - 1;
		// If bottomDirIndex is beyond the last index, then adjust it.
		if (bottomDirIndex >= file_area.lib_list[libIndex].dir_list.length)
			bottomDirIndex = file_area.lib_list[libIndex].dir_list.length - 1;
		return bottomDirIndex;
	}
	
	// For doing the "next" search result
	function nextDirSearchFoundItem(srchObj, numPages, pLibIndex, listStartRow, listEndRow, selectedDirIndex, topFileDirIndex, chooserObj)
	{
		var retObj = {
			differentPage: false,
			topFileDirIndex: srchObj.pageTopIdx,
			selectedDirIndex: srchObj.itemIdx
		};

		// For screen refresh optimization, don't redraw the whole
		// list if the result is on the same page
		if (srchObj.pageTopIdx != topFileDirIndex)
		{
			retObj.differentPage = true;
			chooserObj.updatePageNumInHeader(srchObj.pageNum, numPages, false, false);
			chooserObj.ListScreenfulOfDirs(pLibIndex, srchObj.pageTopIdx, listStartRow, listEndRow, false, true, srchObj.itemIdx);
		}
		else
		{
			if (srchObj.itemIdx != selectedDirIndex)
			{
				var screenY = listStartRow + (selectedDirIndex - topFileDirIndex);
				console.gotoxy(1, screenY);
				chooserObj.WriteFileLibDirLine(pLibIndex, selectedDirIndex, false);
				retObj.selectedDirIndex = srchObj.itemIdx;
				screenY = listStartRow + (retObj.selectedDirIndex - topFileDirIndex);
				console.gotoxy(1, screenY);
				chooserObj.WriteFileLibDirLine(pLibIndex, selectedDirIndex, true);
			}
		}

		return retObj;
	}


	// Figure out the index of the user's currently-selected sub-board.
	var selectedDirIndex = 0;
	if (typeof(bbs.curdir_code) == "string")
	{
		selectedDirIndex = file_area.dir[bbs.curdir_code].index;
		if (selectedDirIndex < 0)
			selectedDirIndex = 0;
		else if (selectedDirIndex >= file_area.lib_list[libIndex].dir_list.length)
			selectedDirIndex = file_area.lib_list[libIndex].dir_list.length - 1;
		// Sanity check
		if (libIndex != file_area.dir[bbs.curdir_code].lib_index)
			selectedDirIndex = 0;
	}

	var listStartRow = 3+this.areaChangeHdrLines.length;      // The row on the screen where the list will start
	var listEndRow = console.screen_rows - 1; // Row on screen where list will end
	var topDirIndex = 0;      // The index of the file directory at the top of the list
	// Figure out the index of the last file directory to appear on the screen.
	var numItemsPerPage = listEndRow - listStartRow + 1;
	var bottomDirIndex = getBottommostDirIndex(topDirIndex, numItemsPerPage);
	// Figure out how many pages are needed to list all the sub-boards.
	var numPages = Math.ceil(file_area.lib_list[libIndex].dir_list.length / numItemsPerPage);
	var pageNum = calcPageNum(topDirIndex, numItemsPerPage);
	// Figure out the top index for the last page.
	var topIndexForLastPage = (numItemsPerPage * numPages) - numItemsPerPage;

	// If the highlighted row is beyond the current screen, then
	// go to the appropriate page.
	if (selectedDirIndex > bottomDirIndex)
	{
		var nextPageTopIndex = 0;
		while (selectedDirIndex > bottomDirIndex)
		{
			nextPageTopIndex = topDirIndex + numItemsPerPage;
			if (nextPageTopIndex < file_area.lib_list[libIndex].dir_list.length)
			{
				// Adjust topDirIndex and bottomDirIndex, and
				// refresh the list on the screen.
				topDirIndex = nextPageTopIndex;
				pageNum = calcPageNum(topDirIndex, numItemsPerPage);
				bottomDirIndex = getBottommostDirIndex(topDirIndex, numItemsPerPage);
			}
			else
				break;
		}

		// If we didn't find the correct page for some reason, then set the
		// variables to display page 1 and select the first file directory
		var foundCorrectPage = ((topDirIndex < file_area.lib_list[libIndex].dir_list.length) &&
		                        (selectedDirIndex >= topDirIndex) && (selectedDirIndex <= bottomDirIndex));
		if (!foundCorrectPage)
		{
			topDirIndex = 0;
			pageNum = calcPageNum(topDirIndex, numItemsPerPage);
			bottomDirIndex = getBottommostDirIndex(topDirIndex, numItemsPerPage);
			selectedDirIndex = 0;
		}
	}

	// Clear the screen, write the header, help line, and library list header, and output
	// a screenful of file directories.
	console.clear("\1n");
	this.DisplayAreaChgHdr(1);
	if (this.areaChangeHdrLines.length > 0)
		console.crlf();
	this.WriteDirListHdr1Line(libIndex, numPages, pageNum);
	this.WriteKeyHelpLine();

	var curpos = new Object();
	curpos.x = 1;
	curpos.y = 2+this.areaChangeHdrLines.length;
	console.gotoxy(curpos);
	printf(this.fileDirHdrPrintfStr, "Dir #", "Description", "# Files");
	this.ListScreenfulOfDirs(libIndex, topDirIndex, listStartRow, listEndRow, false, false);
	// Start of the input loop.
	var highlightScrenRow = 0; // The row on the screen for the highlighted directory
	var userInput = "";        // Will store a keypress from the user
	var lastSearchText = "";
	var lastSearchFoundIdx = -1;
	var continueChoosingFileDir = true;
	while (continueChoosingFileDir)
	{
		// Highlight the currently-selected message directory
		highlightScrenRow = listStartRow + (selectedDirIndex - topDirIndex);
		curpos.y = highlightScrenRow;
		if ((highlightScrenRow > 0) && (highlightScrenRow < console.screen_rows))
		{
			console.gotoxy(1, highlightScrenRow);
			this.WriteFileLibDirLine(libIndex, selectedDirIndex, true);
		}

		// Get a key from the user (upper-case) and take action based upon it.
		userInput = getKeyWithESCChars(K_UPPER | K_NOCRLF);
		switch (userInput)
		{
			case KEY_UP: // Move up one file directory in the list
				if (selectedDirIndex > 0)
				{
					// If the previous directory index is on the previous page, then
					// display the previous page.
					var previousDirIndex = selectedDirIndex - 1;
					if (previousDirIndex < topDirIndex)
					{
						//this.WriteLightbarKeyHelpErrorMsg("Here 1", true); // Temporary
						// Adjust topDirIndex and bottomDirIndex, and
						// refresh the list on the screen.
						topDirIndex -= numItemsPerPage;
						pageNum = calcPageNum(topDirIndex, numItemsPerPage);
						bottomDirIndex = getBottommostDirIndex(topDirIndex, numItemsPerPage);
						this.updatePageNumInHeader(pageNum, numPages, false, false);
						this.ListScreenfulOfDirs(libIndex, topDirIndex, listStartRow,
						                         listEndRow, false, true);
					}
					else
					{
						//this.WriteLightbarKeyHelpErrorMsg("Here 2", true); // Temporary
						// Display the current line un-highlighted.
						console.gotoxy(1, curpos.y);
						this.WriteFileLibDirLine(libIndex, selectedDirIndex, false);
					}
					selectedDirIndex = previousDirIndex;
				}
				break;
			case KEY_DOWN: // Move down one file directory in the list
				if (selectedDirIndex < file_area.lib_list[libIndex].dir_list.length - 1)
				{
					// If the next directory index is on the next page, then display
					// the next page.
					var nextDirIndex = selectedDirIndex + 1;
					if (nextDirIndex > bottomDirIndex)
					{
						//this.WriteLightbarKeyHelpErrorMsg("Here 3", true); // Temporary
						// Adjust topDirIndex and bottomDirIndex, and
						// refresh the list on the screen.
						topDirIndex += numItemsPerPage;
						pageNum = calcPageNum(topDirIndex, numItemsPerPage);
						bottomDirIndex = getBottommostDirIndex(topDirIndex, numItemsPerPage);
						this.updatePageNumInHeader(pageNum, numPages, false, false);
						this.ListScreenfulOfDirs(libIndex, topDirIndex, listStartRow,
						                         listEndRow, false, true);
					}
					else
					{
						//this.WriteLightbarKeyHelpErrorMsg("Here 4", true); // Temporary
						// Display the current line un-highlighted.
						console.gotoxy(1, curpos.y);
						this.WriteFileLibDirLine(libIndex, selectedDirIndex, false);
					}
					selectedDirIndex = nextDirIndex;
				}
				break;
			case KEY_HOME: // Go to the top file directory on the screen
				if (selectedDirIndex > topDirIndex)
				{
					// Display the current line un-highlighted, then adjust
					// selectedDirIndex.
					console.gotoxy(1, curpos.y);
					this.WriteFileLibDirLine(libIndex, selectedDirIndex, false);
					selectedDirIndex = topDirIndex;
					// Note: curpos.y is set at the start of the while loop.
				}
				break;
			case KEY_END: // Go to the bottom file directory on the screen
				if (selectedDirIndex < bottomDirIndex)
				{
					// Display the current line un-highlighted, then adjust
					// selectedDirIndex.
					console.gotoxy(1, curpos.y);
					this.WriteFileLibDirLine(libIndex, selectedDirIndex, false);
					selectedDirIndex = bottomDirIndex;
					// Note: curpos.y is set at the start of the while loop.
				}
				break;
			case KEY_ENTER: // Select the currently-highlighted sub-board; and we're done.
				continueChoosingFileDir = false;
				retObj.fileDirChosen = true;
				retObj.fileDirIdx = selectedDirIndex;
				retObj.fileDirCode = file_area.lib_list[libIndex].dir_list[selectedDirIndex].code;
				break;
			case KEY_PAGE_DOWN: // Go to the next page
				var nextPageTopIndex = topDirIndex + numItemsPerPage;
				if (nextPageTopIndex < file_area.lib_list[libIndex].dir_list.length)
				{
					// Adjust topDirIndex and bottomDirIndex, and
					// refresh the list on the screen.
					topDirIndex = nextPageTopIndex;
					pageNum = calcPageNum(topDirIndex, numItemsPerPage);
					bottomDirIndex = getBottommostDirIndex(topDirIndex, numItemsPerPage);
					this.updatePageNumInHeader(pageNum, numPages, false, false);
					this.ListScreenfulOfDirs(libIndex, topDirIndex, listStartRow,
					                         listEndRow, false, true);
					selectedDirIndex = topDirIndex;
				}
				break;
			case KEY_PAGE_UP: // Go to the previous page
				var prevPageTopIndex = topDirIndex - numItemsPerPage;
				if (prevPageTopIndex >= 0)
				{
					// Adjust topDirIndex and bottomDirIndex, and
					// refresh the list on the screen.
					topDirIndex = prevPageTopIndex;
					pageNum = calcPageNum(topDirIndex, numItemsPerPage);
					bottomDirIndex = getBottommostDirIndex(topDirIndex, numItemsPerPage);
					this.updatePageNumInHeader(pageNum, numPages, false, false);
					this.ListScreenfulOfDirs(libIndex, topDirIndex, listStartRow,
					                         listEndRow, false, true);
					selectedDirIndex = topDirIndex;
				}
				break;
			case 'F': // Go to the first page
				if (topDirIndex > 0)
				{
					topDirIndex = 0;
					pageNum = calcPageNum(topDirIndex, numItemsPerPage);
					bottomDirIndex = getBottommostDirIndex(topDirIndex, numItemsPerPage);
					this.updatePageNumInHeader(pageNum, numPages, false, false);
					this.ListScreenfulOfDirs(libIndex, topDirIndex, listStartRow,
					                         listEndRow, false, true);
					selectedDirIndex = 0;
				}
				break;
			case 'L': // Go to the last page
				if (topDirIndex < topIndexForLastPage)
				{
					topDirIndex = topIndexForLastPage;
					pageNum = calcPageNum(topDirIndex, numItemsPerPage);
					bottomDirIndex = getBottommostDirIndex(topDirIndex, numItemsPerPage);
					this.updatePageNumInHeader(pageNum, numPages, false, false);
					this.ListScreenfulOfDirs(libIndex, topDirIndex, listStartRow,
					                         listEndRow, false, true);
					selectedDirIndex = topIndexForLastPage;
				}
				break;
			case '/': // Start of find (search)
			case CTRL_F: // Start of find
				console.gotoxy(1, console.screen_rows);
				console.cleartoeol("\1n");
				console.gotoxy(1, console.screen_rows);
				var promptText = "Search: ";
				console.print(promptText);
				var searchText = getStrWithTimeout(K_UPPER|K_NOCRLF|K_GETSTR|K_NOSPIN|K_LINE, console.screen_columns - promptText.length - 1, SEARCH_TIMEOUT_MS);
				// If the user entered text, then do the search, and if found,
				// found, go to the page and select the item indicated by the
				// search.
				if (searchText.length > 0)
				{
					var srchObj = getPageNumFromSearch(searchText, numItemsPerPage, true, 0, pLibIndex);
					if (srchObj.pageNum > 0)
					{
						lastSearchText = searchText;
						lastSearchFoundIdx = srchObj.itemIdx;

						// For screen refresh optimization, don't redraw the whole
						// list if the result is on the same page
						if (srchObj.pageTopIdx != topDirIndex)
						{
							topDirIndex = srchObj.pageTopIdx;
							selectedDirIndex = srchObj.itemIdx;
							pageNum = srchObj.pageNum;
							this.updatePageNumInHeader(pageNum, numPages, false, false);
							this.ListScreenfulOfDirs(pLibIndex, topDirIndex, listStartRow, listEndRow, false, true);
						}
						else
						{
							if (srchObj.itemIdx != selectedDirIndex)
							{
								var screenY = listStartRow + (selectedDirIndex - topDirIndex);
								console.gotoxy(1, screenY);
								this.WriteFileLibDirLine(pLibIndex, selectedDirIndex, false);
								selectedDirIndex = srchObj.itemIdx;
								screenY = listStartRow + (selectedDirIndex - topDirIndex);
								console.gotoxy(1, screenY);
								this.WriteFileLibDirLine(pLibIndex, selectedDirIndex, true);
							}
						}
					}
					else
						this.WriteLightbarKeyHelpErrorMsg("Not found", false);
				}
				this.WriteKeyHelpLine();
				break;
			case 'N': // Next search result (requires an existing search term)
				if ((lastSearchText.length > 0) && (lastSearchFoundIdx > -1))
				{
					// Do the search, and if found, go to the page and select the item
					// indicated by the search.
					var srchObj = getPageNumFromSearch(searchText, numItemsPerPage, true, lastSearchFoundIdx+1, pLibIndex);
					lastSearchFoundIdx = srchObj.itemIdx;
					if (srchObj.pageNum > 0)
					{
						var foundItemRetObj = nextDirSearchFoundItem(srchObj, numPages, pLibIndex, listStartRow, listEndRow, selectedDirIndex, topDirIndex, this);
						if (foundItemRetObj.differentPage)
						{
							topDirIndex = foundItemRetObj.topFileDirIndex;
							pageNum = srchObj.pageNum;
							bottomDirIndex = getBottommostDirIndex(topDirIndex, numItemsPerPage);
						}
						selectedDirIndex = foundItemRetObj.selectedDirIndex;
					}
					else
					{
						// Not found - Wrap around and start at 0 again
						var srchObj = getPageNumFromSearch(searchText, numItemsPerPage, true, 0, pLibIndex);
						lastSearchFoundIdx = srchObj.itemIdx;
						var foundItemRetObj = nextDirSearchFoundItem(srchObj, numPages, pLibIndex, listStartRow, listEndRow, selectedDirIndex, topDirIndex, this);
						if (foundItemRetObj.selectedDirIndex != selectedDirIndex)
						{
							if (foundItemRetObj.differentPage)
							{
								pageNum = srchObj.pageNum;
								topDirIndex = foundItemRetObj.topFileDirIndex;
								bottomDirIndex = getBottommostDirIndex(topDirIndex, numItemsPerPage);
							}
							selectedDirIndex = foundItemRetObj.selectedDirIndex;
						}
						else
							this.WriteLightbarKeyHelpErrorMsg("No others found", true);
					}
				}
				else
					this.WriteLightbarKeyHelpErrorMsg("There is no previous search", true);
				break;
			case KEY_ESC: // Quit
			case 'Q': // Quit
				continueChoosingFileDir = false;
				break;
			case '?': // Show help
				this.ShowHelpScreen(true, true);
				console.pause();
				// Refresh the screen
				this.DisplayAreaChgHdr(1);
				console.gotoxy(1, 1+this.areaChangeHdrLines.length);
				this.WriteDirListHdr1Line(libIndex, numPages, pageNum);
				console.cleartoeol("\1n");
				this.WriteKeyHelpLine();
				console.gotoxy(1, 2+this.areaChangeHdrLines.length);
				printf(this.fileDirHdrPrintfStr, "Dir #", "Description", "# Files");
				this.ListScreenfulOfDirs(libIndex, topDirIndex, listStartRow, listEndRow, false, true);
				break;
			default:
				// If the user entered a numeric digit, then treat it as
				// the start of the file directory number.
				if (userInput.match(/[0-9]/))
				{
					var originalCurpos = curpos;

					// Put the user's input back in the input buffer to
					// be used for getting the rest of the message number.
					console.ungetstr(userInput);
					// Move the cursor to the bottom of the screen and
					// prompt the user for the message number.
					console.gotoxy(1, console.screen_rows);
					console.clearline("\1n");
					console.print("\1cDir #: \1h");
					userInput = console.getnum(file_area.lib_list[libIndex].dir_list.length);
					// If the user made a selection, then set it in the
					// return object and don't continue the input loop.
					if (userInput > 0)
					{
						continueChoosingFileDir = false;
						retObj.fileDirChosen = true;
						retObj.fileDirIdx = userInput - 1;
						retObj.fileDirCode = file_area.lib_list[libIndex].dir_list[retObj.fileDirIdx].code;
					}
					else
					{
						// The user didn't enter a selection.  Now we need to
						// re-draw the screen due to everything being moved
						// up one line.
						console.gotoxy(1, 1);
						this.WriteDirListHdr1Line(libIndex, numPages, pageNum);
						console.cleartoeol("\1n");
						this.WriteKeyHelpLine();
						console.gotoxy(1, 2);
						printf(this.fileDirHdrPrintfStr, "Dir #", "Description", "# Files");
						this.ListScreenfulOfDirs(libIndex, topDirIndex, listStartRow,
						                         listEndRow, false, true);
					}
				}
				break;
		}
	}

	return retObj;
}

// Displays a screenful of file libraries (for the lightbar interface).
//
// Parameters:
//  pStartIndex: The file library index to start at (0-based)
//  pStartScreenRow: The row on the screen to start at (1-based)
//  pEndScreenRow: The row on the screen to end at (1-based)
//  pClearScreenFirst: Boolean - Whether or not to clear the screen first
//  pBlankToEndRow: Boolean - Whether or not to write blank lines to the end
//                  screen row if there aren't file libraries to fill
//                  the screen.
//  pHighlightIndex: Optional - The index of the library to highlight
function DDFileAreaChooser_listScreenfulOfFileLibs(pStartIndex, pStartScreenRow,
                                                   pEndScreenRow, pClearScreenFirst,
                                                   pBlankToEndRow, pHighlightIndex)
{
	// Check the parameters; If they're bad, then just return.
	if ((typeof(pStartIndex) != "number") ||
	    (typeof(pStartScreenRow) != "number") ||
	    (typeof(pEndScreenRow) != "number"))
	{
		return;
	}
	if ((pStartIndex < 0) || (pStartIndex >= file_area.lib_list.length))
		return;
	if ((pStartScreenRow < 1) || (pStartScreenRow > console.screen_rows))
		return;
	if ((pEndScreenRow < 1) || (pEndScreenRow > console.screen_rows))
		return;

	// If pStartScreenRow is greather than pEndScreenRow, then swap them.
	if (pStartScreenRow > pEndScreenRow)
	{
		var temp = pStartScreenRow;
		pStartScreenRow = pEndScreenRow;
		pEndScreenRow = temp;
	}

	// Calculate the ending index to use for the file libraries array.
	var endIndex = pStartIndex + (pEndScreenRow-pStartScreenRow);
	if (endIndex >= file_area.lib_list.length)
		endIndex = file_area.lib_list.length - 1;
	var onePastEndIndex = endIndex + 1;

	// Clear the screen, go to the specified screen row, and display the file
	// library information.
	var highlightIdx = (typeof(pHighlightIndex) == "number" ? pHighlightIndex : -1);
	if (pClearScreenFirst)
		console.clear("\1n");
	console.gotoxy(1, pStartScreenRow);
	var highlight = false;
	var libIndex = pStartIndex;
	for (; libIndex < onePastEndIndex; ++libIndex)
	{
		highlight = (libIndex == highlightIdx);
		this.WriteFileLibLine(libIndex, highlight);
		if (libIndex < endIndex)
			console.crlf();
	}

	// If pBlankToEndRow is true and we're not at the end row yet, then
	// write blank lines to the end row.
	if (pBlankToEndRow)
	{
		var screenRow = pStartScreenRow + (endIndex - pStartIndex) + 1;
		if (screenRow <= pEndScreenRow)
		{
			for (; screenRow <= pEndScreenRow; ++screenRow)
			{
				console.gotoxy(1, screenRow);
				console.clearline("\1n");
			}
		}
	}
}

// For the DDFileAreaChooser class - Writes a file library information line.
//
// Parameters:
//  pLibIndex: The index of the file library to write (assumed to be valid)
//  pHighlight: Boolean - Whether or not to write the line highlighted.
function DDFileAreaChooser_writeFileLibLine(pLibIndex, pHighlight)
{
	console.print("\1n");
	// Write the highlight background color if pHighlight is true.
	if (pHighlight)
		console.print(this.colors.bkgHighlight);

	// Write the file library information line
	curLibIdx = 0;
	if (typeof(bbs.curdir_code) == "string")
		curLibIdx = file_area.dir[bbs.curdir_code].lib_index;
	console.print(pLibIndex == curLibIdx ? this.colors.areaMark + "*" : " ");
	printf((pHighlight ? this.fileLibHighlightPrintfStr : this.fileLibPrintfStr),
	       +(pLibIndex+1),
	       file_area.lib_list[pLibIndex].description.substr(0, this.descFieldLen),
	       file_area.lib_list[pLibIndex].dir_list.length);
	console.cleartoeol("\1n");
}

// For the DDFileAreaChooser class: Writes a file directory information line.
//
// Parameters:
//  pLibIndex: The index of the file library (assumed to be valid)
//  pDirIndex: The index of the directory within the file library to write (assumed to be valid)
//  pHighlight: Boolean - Whether or not to write the line highlighted.
function DDFileAreaChooser_writeFileLibDirLine(pLibIndex, pDirIndex, pHighlight)
{
	console.print("\1n");
	// Write the highlight background color if pHighlight is true.
	if (pHighlight)
		console.print(this.colors.bkgHighlight);

	// Determine if pLibIndex and pDirIndex specify the user's
	// currently-selected file library & directory.
	var currentDir = false;
	if (typeof(bbs.curdir_code) == "string")
		currentDir = ((pLibIndex == file_area.dir[bbs.curdir_code].lib_index) && (pDirIndex == file_area.dir[bbs.curdir_code].index));

	// Print the directory information line
	console.print(currentDir ? this.colors.areaMark + "*" : " ");
	printf((pHighlight ? this.fileDirListPrintfInfo[pLibIndex].highlightPrintfStr : this.fileDirListPrintfInfo[pLibIndex].printfStr),
	       +(pDirIndex+1),
	       file_area.lib_list[pLibIndex].dir_list[pDirIndex].description.substr(0, this.descFieldLen),
	this.fileDirListPrintfInfo[pLibIndex].fileCounts[pDirIndex]);
}

// Updates the page number text in the file library/area list header line on the screen.
//
// Parameters:
//  pPageNum: The page number
//  pNumPages: The total number of pages
//  pFileLib: Boolean - Whether or not this is for the file library header.  If so,
//            then this will go to the right location for the file library page text
//            and use this.colors.header for the text.  Otherwise, this will
//            go to the right place for the file area page text and use the
//            file area header color.
//  pRestoreCurPos: Optional - Boolean - If true, then move the cursor back
//                  to the position where it was before this function was called
function DDFileAreaChooser_updatePageNumInHeader(pPageNum, pNumPages, pFileLib, pRestoreCurPos)
{
  var originalCurPos = null;
  if (pRestoreCurPos)
    originalCurPos = console.getxy();

  if (pFileLib)
  {
    console.gotoxy(29, 1+this.areaChangeHdrLines.length);
    console.print("\1n" + this.colors.header + pPageNum + " of " + pNumPages + ")   ");
  }
  else
  {
    console.gotoxy(67, 1+this.areaChangeHdrLines.length);
    console.print("\1n" + this.colors.fileAreaHdr + pPageNum + " of " + pNumPages + ")   ");
  }

  if (pRestoreCurPos)
    console.gotoxy(originalCurPos);
}

// Displays a screenful of file directories, for the lightbar interface.
//
// Parameters:
//  pLibIndex: The index of the file library (0-based)
//  pStartDirIndex: The file directory index to start at (0-based)
//  pStartScreenRow: The row on the screen to start at (1-based)
//  pEndScreenRow: The row on the screen to end at (1-based)
//  pClearScreenFirst: Boolean - Whether or not to clear the screen first
//  pBlankToEndRow: Boolean - Whether or not to write blank lines to the end
//                  screen row if there aren't enough file directories to fill
//                  the screen.
//  pSelectedDirIdx: Optional - The index of the selected dir
function DDMsgAreaChooser_listScreenfulOfFileDirs(pLibIndex, pStartDirIndex,
                                                  pStartScreenRow, pEndScreenRow,
                                                  pClearScreenFirst, pBlankToEndRow,
												  pSelectedDirIdx)
{
	// Check the parameters; If they're bad, then just return.
	if ((typeof(pLibIndex) != "number") ||
	    (typeof(pStartDirIndex) != "number") ||
	    (typeof(pStartScreenRow) != "number") ||
	    (typeof(pEndScreenRow) != "number"))
	{
		return;
	}
	if ((pLibIndex < 0) || (pLibIndex >= file_area.lib_list.length))
		return;
	if ((pStartDirIndex < 0) ||
			(pStartDirIndex >= file_area.lib_list[pLibIndex].dir_list.length))
	{
		return;
	}
	if ((pStartScreenRow < 1) || (pStartScreenRow > console.screen_rows))
		return;
	if ((pEndScreenRow < 1) || (pEndScreenRow > console.screen_rows))
		return;
	// If pStartScreenRow is greather than pEndScreenRow, then swap them.
	if (pStartScreenRow > pEndScreenRow)
	{
		var temp = pStartScreenRow;
		pStartScreenRow = pEndScreenRow;
		pEndScreenRow = temp;
	}

	// Calculate the ending index to use for the sub-board array.
	var endIndex = pStartDirIndex + (pEndScreenRow-pStartScreenRow);
	if (endIndex >= file_area.lib_list[pLibIndex].dir_list.length)
		endIndex = file_area.lib_list[pLibIndex].dir_list.length - 1;
	var onePastEndIndex = endIndex + 1;

	// Clear the screen and go to the specified screen row.
	if (pClearScreenFirst)
		console.clear("\1n");
	console.gotoxy(1, pStartScreenRow);

	// Start listing the file  directories.

	var dirIndex = pStartDirIndex;
	var highlightIdx = (typeof(pSelectedDirIdx) == "number" ? pSelectedDirIdx : -1);
	for (; dirIndex < onePastEndIndex; ++dirIndex)
	{
		var highlight = (dirIndex == highlightIdx);
		this.WriteFileLibDirLine(pLibIndex, dirIndex, highlight);
		if (dirIndex < endIndex)
			console.crlf();
	}

	// If pBlankToEndRow is true and we're not at the end row yet, then
	// write blank lines to the end row.
	if (pBlankToEndRow)
	{
		var screenRow = pStartScreenRow + (endIndex - pStartDirIndex) + 1;
		if (screenRow <= pEndScreenRow)
		{
			for (; screenRow <= pEndScreenRow; ++screenRow)
			{
				console.gotoxy(1, screenRow);
				console.clearline("\1n");
			}
		}
	}
}

function DDFileAreaChooser_writeKeyHelpLine()
{
	console.gotoxy(1, console.screen_rows);
	console.print(this.lightbarKeyHelpText);
}

// For the DDFileAreaChooser class: Reads the configuration file.
function DDFileAreaChooser_ReadConfigFile()
{
	// Open the configuration file
	var cfgFile = new File(gStartupPath + "DDFileAreaChooser.cfg");
	if (cfgFile.open("r"))
	{
		var settingsMode = "behavior";
		var fileLine = null;     // A line read from the file
		var equalsPos = 0;       // Position of a = in the line
		var commentPos = 0;      // Position of the start of a comment
		var setting = null;      // A setting name (string)
		var settingUpper = null; // Upper-case setting name
		var value = null;        // A value for a setting (string)
		while (!cfgFile.eof)
		{
			// Read the next line from the config file.
			fileLine = cfgFile.readln(2048);

			// fileLine should be a string, but I've seen some cases
			// where it isn't, so check its type.
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
			else if (fileLine.toUpperCase() == "[COLORS]")
			{
				settingsMode = "colors";
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

				if (settingsMode == "behavior")
				{
					// Set the appropriate value in the settings object.
					if (settingUpper == "USELIGHTBARINTERFACE")
						this.useLightbarInterface = (value.toUpperCase() == "TRUE");
					else if (settingUpper == "AREACHOOSERHDRFILENAMEBASE")
						this.areaChooserHdrFilenameBase = value;
					else if (settingUpper == "AREACHOOSERHDRMAXLINES")
					{
						var maxNumLines = +value;
						if (maxNumLines > 0)
							this.areaChooserHdrMaxLines = maxNumLines;
					}
				}
				else if (settingsMode == "colors")
					this.colors[setting] = value;
			}
		}

		cfgFile.close();
	}
}

// Misc. functions

// For the DDFileAreaChooser class: Shows the help screen
//
// Parameters:
//  pLightbar: Boolean - Whether or not to show lightbar help.  If
//             false, then this function will show regular help.
//  pClearScreen: Boolean - Whether or not to clear the screen first
function DDFileAreaChooser_showHelpScreen(pLightbar, pClearScreen)
{
	if (pClearScreen)
		console.clear("\1n");
	else
		console.print("\1n");
	console.center("\1c\1hDigital Distortion File Area Chooser");
	console.center("\1kÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ");
	console.center("\1n\1cVersion \1g" + DD_FILE_AREA_CHOOSER_VERSION +
	               " \1w\1h(\1b" + DD_FILE_AREA_CHOOSER_VER_DATE + "\1w)");
	console.crlf();
	console.print("\1n\1cFirst, a listing of file libraries is displayed.  One can be chosen by typing");
	console.crlf();
	console.print("its number.  Then, a listing of directories within that library will be");
	console.crlf();
	console.print("shown, and one can be chosen by typing its number.");
	console.crlf();

	if (pLightbar)
	{
		console.crlf();
		console.print("\1n\1cThe lightbar interface also allows up & down navigation through the lists:");
		console.crlf();
		console.print("\1k\1hÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ");
		console.crlf();
		console.print("\1n\1c\1hUp arrow\1n\1c: Move the cursor up one line");
		console.crlf();
		console.print("\1hDown arrow\1n\1c: Move the cursor down one line");
		console.crlf();
		console.print("\1hENTER\1n\1c: Select the current library/dir");
		console.crlf();
		console.print("\1hHOME\1n\1c: Go to the first item on the screen");
		console.crlf();
		console.print("\1hEND\1n\1c: Go to the last item on the screen");
		console.crlf();
		console.print("\1hPageUp\1n\1c/\1hPageDown\1n\1c: Go to the previous/next page");
		console.crlf();
		console.print("\1hF\1n\1c/\1hL\1n\1c: Go to the first/last page");
		console.crlf();
		console.print("\1h/\1n\1c or \1hCTRL-F\1n\1c: Find by name/description");
		console.crlf();
		console.print("\1hN\1n\1c: Next search result (after a find)");
		console.crlf();
	}

	console.crlf();
	console.print("Additional keyboard commands:");
	console.crlf();
	console.print("\1k\1hÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ");
	console.crlf();
	console.print("\1n\1c\1h?\1n\1c: Show this help screen");
	console.crlf();
	console.print("\1hQ\1n\1c: Quit");
	console.crlf();
}

// Returns the number of files in a directory for one of the file libraries.
// Note that returns the number of files in the directory on the hard drive,
// which isn't necessarily the number of files in the database for the directory.
//
// Paramters:
//  pLibNum: The file library number (0-based)
//  pDirNum: The file directory number (0-based)
//
// Returns: The number of files in the directory
function DDFileAreaChooser_NumFilesInDir(pLibNum, pDirNum)
{
   var numFiles = 0;

   // Count the files in the directory.  If it's not a directory, then
   // increment numFiles.
   var files = directory(file_area.lib_list[pLibNum].dir_list[pDirNum].path + "*.*");
   numFiles = files.length;
   // Make sure directories aren't counted: Go through the files array, and
   // for each directory, decrement numFiles.
   for (var i in files)
   {
      if (file_isdir(files[i]))
         --numFiles;
   }

   return numFiles;
}

// Builds file directory printf format information for a file library.
// The widths of the description & # files columns are calculated
// based on the greatest number of files in a directory for the
// file library.
//
// Parameters:
//  pLibIndex: The index of the file library
function DDFileAreaChooser_buildFileDirPrintfInfoForLib(pLibIndex)
{
  if (typeof(this.fileDirListPrintfInfo[pLibIndex]) == "undefined")
  {
    // Create the file directory listing object and set some defaults
    this.fileDirListPrintfInfo[pLibIndex] = new Object();
    this.fileDirListPrintfInfo[pLibIndex].numFilesLen = 4;
    // Get information about the number of files in each directory
    // and the greatest number of files and set up the according
    // information in the file directory list object
    var fileDirInfo = getGreatestNumFiles(pLibIndex);
    if (fileDirInfo != null)
    {
      this.fileDirListPrintfInfo[pLibIndex].numFilesLen = fileDirInfo.greatestNumFiles.toString().length;
      this.fileDirListPrintfInfo[pLibIndex].fileCounts = fileDirInfo.fileCounts.slice(0);
    }
    else
    {
      // fileDirInfo is null.  We still want to create
      // the fileCounts array in the file directory object
      // so that it's valid.
      this.fileDirListPrintfInfo[pLibIndex].fileCounts = new Array(file_area.lib_list[pLibIndex].length);
      for (var i = 0; i < file_area.lib_list[pLibIndex].length; ++i)
        this.fileDirListPrintfInfo[pLibIndex].fileCounts[i] == 0;
    }

    // Set the description field length and printf strings for
    // this file library
    this.fileDirListPrintfInfo[pLibIndex].descFieldLen =
                        console.screen_columns - this.areaNumLen
                        - this.fileDirListPrintfInfo[pLibIndex].numFilesLen - 5;
    this.fileDirListPrintfInfo[pLibIndex].printfStr =
                        this.colors.areaNum + " %" + this.areaNumLen + "d "
                        + this.colors.desc + "%-"
                        + this.fileDirListPrintfInfo[pLibIndex].descFieldLen
                        + "s " + this.colors.numItems + "%"
                        + this.fileDirListPrintfInfo[pLibIndex].numFilesLen + "d";
    this.fileDirListPrintfInfo[pLibIndex].highlightPrintfStr =
                        "\1n" + this.colors.bkgHighlight
                        + this.colors.areaNumHighlight + " %" + this.areaNumLen
                        + "d " + this.colors.descHighlight + "%-"
                        + this.fileDirListPrintfInfo[pLibIndex].descFieldLen
                        + "s " + this.colors.numItemsHighlight + "%"
                        + this.fileDirListPrintfInfo[pLibIndex].numFilesLen +"d\1n";
  }
}

// For the DDFileAreaChooser class: Displays the area chooser header
//
// Parameters:
//  pStartScreenRow: The row on the screen at which to start displaying the
//                   header information.  Will be used if the user's terminal
//                   supports ANSI.
//  pClearRowsFirst: Optional boolean - Whether or not to clear the rows first.
//                   Defaults to true.  Only valid if the user's terminal supports
//                   ANSI.
function DDFileAreaChooser_DisplayAreaChgHdr(pStartScreenRow, pClearRowsFirst)
{
	if (this.areaChangeHdrLines == null)
		return;
	if (this.areaChangeHdrLines.length == 0)
		return;

	// If the user's terminal supports ANSI and pStartScreenRow is a number, then
	// we can move the cursor and display the header where specified.
	if (console.term_supports(USER_ANSI) && (typeof(pStartScreenRow) == "number"))
	{
		// If specified to clear the rows first, then do so.
		var screenX = 1;
		var screenY = pStartScreenRow;
		var clearRowsFirst = (typeof(pClearRowsFirst) == "boolean" ? pClearRowsFirst : true);
		if (clearRowsFirst)
		{
			console.print("\1n");
			for (var hdrFileIdx = 0; hdrFileIdx < this.areaChangeHdrLines.length; ++hdrFileIdx)
			{
				console.gotoxy(screenX, screenY++);
				console.cleartoeol();
			}
		}
		// Display the header starting on the first column and the given screen row.
		screenX = 1;
		screenY = pStartScreenRow;
		for (var hdrFileIdx = 0; hdrFileIdx < this.areaChangeHdrLines.length; ++hdrFileIdx)
		{
			console.gotoxy(screenX, screenY++);
			console.print(this.areaChangeHdrLines[hdrFileIdx]);
			//console.putmsg(this.areaChangeHdrLines[hdrFileIdx]);
			//console.cleartoeol("\1n"); // Shouldn't do this, as it resets color attributes
		}
	}
	else
	{
		// The user's terminal doesn't support ANSI or pStartScreenRow is not a
		// number - So just output the header lines.
		for (var hdrFileIdx = 0; hdrFileIdx < this.areaChangeHdrLines.length; ++hdrFileIdx)
		{
			console.print(this.areaChangeHdrLines[hdrFileIdx]);
			//console.putmsg(this.areaChangeHdrLines[hdrFileIdx]);
			//console.cleartoeol("\1n"); // Shouldn't do this, as it resets color attributes
			console.crlf();
		}
	}
}

// For the DDFileAreaChooser class: Writes a temporary error message at the key help line
// for lightbar mode.
//
// Parameters:
//  pErrorMsg: The error message to write
//  pRefreshHelpLine: Whether or not to re-draw the help line on the screen
function DDFileAreaChooser_WriteLightbarKeyHelpErrorMsg(pErrorMsg, pRefreshHelpLine)
{
	console.gotoxy(1, console.screen_rows);
	console.cleartoeol("\1n");
	console.gotoxy(1, console.screen_rows);
	console.print("\1y\1h" + pErrorMsg + "\1n");
	mswait(ERROR_WAIT_MS);
	if (pRefreshHelpLine)
		this.WriteKeyHelpLine();
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
function trimSpaces(pString, pLeading, pMultiple, pTrailing)
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

	return pString;
}

// Calculates & returns a page number.
//
// Parameters:
//  pTopIndex: The index (0-based) of the topmost item on the page
//  pNumPerPage: The number of items per page
//
// Return value: The page number
function calcPageNum(pTopIndex, pNumPerPage)
{
  return ((pTopIndex / pNumPerPage) + 1);
}

// For a given file library index, returns an object containing
// the greatest number of files of all directories within a file
// library and an array containing the number of files in each
// directory.  If the given library index is invalid, this
// function will return null.
//
// Parameters:
//  pLibIndex: The index of the file library
//
// Returns: An object containing the following properties:
//          greatestNumFiles: The greatest number of files of all
//                            directories within the file library
//          fileCounts: An array, indexed by directory index,
//                      containing the number of files in each
//                      directory within the file library
function getGreatestNumFiles(pLibIndex)
{
  // Sanity checking
  if (typeof(pLibIndex) != "number")
    return null;
  if (typeof(file_area.lib_list[pLibIndex]) == "undefined")
    return null;

  var retObj = new Object();
  retObj.greatestNumFiles = 0;
  retObj.fileCounts = new Array(file_area.lib_list[pLibIndex].dir_list.length);
  for (var dirIndex = 0; dirIndex < file_area.lib_list[pLibIndex].dir_list.length; ++dirIndex)
  {
    retObj.fileCounts[dirIndex] = DDFileAreaChooser_NumFilesInDir(pLibIndex, dirIndex);
    if (retObj.fileCounts[dirIndex] > retObj.greatestNumFiles)
      retObj.greatestNumFiles = retObj.fileCounts[dirIndex];
  }
  return retObj;
}

// Inputs a keypress from the user and handles some ESC-based
// characters such as PageUp, PageDown, and ESC.  If PageUp
// or PageDown are pressed, this function will return the
// string "\1PgUp" (KEY_PAGE_UP) or "\1Pgdn" (KEY_PAGE_DOWN),
// respectively.  Also, F1-F5 will be returned as "\1F1"
// through "\1F5", respectively.
// Thanks goes to Psi-Jack for the original impementation
// of this function.
//
// Parameters:
//  pGetKeyMode: Optional - The mode bits for console.getkey().
//               If not specified, K_NONE will be used.
//
// Return value: The user's keypress
function getKeyWithESCChars(pGetKeyMode)
{
   var getKeyMode = K_NONE;
   if (typeof(pGetKeyMode) == "number")
      getKeyMode = pGetKeyMode;

   var userInput = console.getkey(getKeyMode);
   if (userInput == KEY_ESC) {
      switch (console.inkey(K_NOECHO|K_NOSPIN, 2)) {
         case '[':
            switch (console.inkey(K_NOECHO|K_NOSPIN, 2)) {
               case 'V':
                  userInput = KEY_PAGE_UP;
                  break;
               case 'U':
                  userInput = KEY_PAGE_DOWN;
                  break;
           }
           break;
         case 'O':
           switch (console.inkey(K_NOECHO|K_NOSPIN, 2)) {
              case 'P':
                 userInput = "\1F1";
                 break;
              case 'Q':
                 userInput = "\1F2";
                 break;
              case 'R':
                 userInput = "\1F3";
                 break;
              case 'S':
                 userInput = "\1F4";
                 break;
              case 't':
                 userInput = "\1F5";
                 break;
           }
         default:
           break;
      }
   }

   return userInput;
}

// Loads a text file (an .ans or .asc) into an array.  This will first look for
// an .ans version, and if exists, convert to Synchronet colors before loading
// it.  If an .ans doesn't exist, this will look for an .asc version.
//
// Parameters:
//  pFilenameBase: The filename without the extension
//  pMaxNumLines: Optional - The maximum number of lines to load from the text file
//
// Return value: An array containing the lines from the text file
function loadTextFileIntoArray(pFilenameBase, pMaxNumLines)
{
	if (typeof(pFilenameBase) != "string")
		return new Array();

	var maxNumLines = (typeof(pMaxNumLines) == "number" ? pMaxNumLines : -1);

	var txtFileLines = new Array();
	// See if there is a header file that is made for the user's terminal
	// width (areaChgHeader-<width>.ans/asc).  If not, then just go with
	// msgHeader.ans/asc.
	var txtFileExists = true;
	var txtFilenameFullPath = gStartupPath + pFilenameBase;
	var txtFileFilename = "";
	if (file_exists(txtFilenameFullPath + "-" + console.screen_columns + ".ans"))
		txtFileFilename = txtFilenameFullPath + "-" + console.screen_columns + ".ans";
	else if (file_exists(txtFilenameFullPath + "-" + console.screen_columns + ".asc"))
		txtFileFilename = txtFilenameFullPath + "-" + console.screen_columns + ".asc";
	else if (file_exists(txtFilenameFullPath + ".ans"))
		txtFileFilename = txtFilenameFullPath + ".ans";
	else if (file_exists(txtFilenameFullPath + ".asc"))
		txtFileFilename = txtFilenameFullPath + ".asc";
	else
		txtFileExists = false;
	if (txtFileExists)
	{
		var syncConvertedHdrFilename = txtFileFilename;
		// If the user's console doesn't support ANSI and the header file is ANSI,
		// then convert it to Synchronet attribute codes and read that file instead.
		if (!console.term_supports(USER_ANSI) && (getStrAfterPeriod(txtFileFilename).toUpperCase() == "ANS"))
		{
			syncConvertedHdrFilename = txtFilenameFullPath + "_converted.asc";
			if (!file_exists(syncConvertedHdrFilename))
			{
				if (getStrAfterPeriod(txtFileFilename).toUpperCase() == "ANS")
				{
					var filenameBase = txtFileFilename.substr(0, dotIdx);
					var cmdLine = system.exec_dir + "ans2asc \"" + txtFileFilename + "\" \""
								+ syncConvertedHdrFilename + "\"";
					// Note: Both system.exec(cmdLine) and
					// bbs.exec(cmdLine, EX_NATIVE, gStartupPath) could be used to
					// execute the command, but system.exec() seems noticeably faster.
					system.exec(cmdLine);
				}
				else
					syncConvertedHdrFilename = txtFileFilename;
			}
		}
		/*
		// If the header file is ANSI, then convert it to Synchronet attribute
		// codes and read that file instead.  This is done so that this script can
		// accurately get the file line lengths using console.strlen().
		var syncConvertedHdrFilename = txtFilenameFullPath + "_converted.asc";
		if (!file_exists(syncConvertedHdrFilename))
		{
			if (getStrAfterPeriod(txtFileFilename).toUpperCase() == "ANS")
			{
				var filenameBase = txtFileFilename.substr(0, dotIdx);
				var cmdLine = system.exec_dir + "ans2asc \"" + txtFileFilename + "\" \""
				            + syncConvertedHdrFilename + "\"";
				// Note: Both system.exec(cmdLine) and
				// bbs.exec(cmdLine, EX_NATIVE, gStartupPath) could be used to
				// execute the command, but system.exec() seems noticeably faster.
				system.exec(cmdLine);
			}
			else
				syncConvertedHdrFilename = txtFileFilename;
		}
		*/
		// Read the header file into txtFileLines
		var hdrFile = new File(syncConvertedHdrFilename);
		if (hdrFile.open("r"))
		{
			var fileLine = null;
			while (!hdrFile.eof)
			{
				// Read the next line from the header file.
				fileLine = hdrFile.readln(2048);
				// fileLine should be a string, but I've seen some cases
				// where it isn't, so check its type.
				if (typeof(fileLine) != "string")
					continue;

				// Make sure the line isn't longer than the user's terminal
				//if (fileLine.length > console.screen_columns)
				//   fileLine = fileLine.substr(0, console.screen_columns);
				txtFileLines.push(fileLine);

				// If the header array now has the maximum number of lines, then
				// stop reading the header file.
				if (txtFileLines.length == maxNumLines)
					break;
			}
			hdrFile.close();
		}
	}
	return txtFileLines;
}

// Returns the portion (if any) of a string after the period.
//
// Parameters:
//  pStr: The string to extract from
//
// Return value: The portion of the string after the dot, if there is one.  If
//               not, then an empty string will be returned.
function getStrAfterPeriod(pStr)
{
	var strAfterPeriod = "";
	var dotIdx = pStr.lastIndexOf(".");
	if (dotIdx > -1)
		strAfterPeriod = pStr.substr(dotIdx+1);
	return strAfterPeriod;
}

// Inputs a string from the user, with a timeout
//
// Parameters:
//  pMode: The mode bits to use for the input (i.e., defined in sbbsdefs.js)
//  pMaxLength: The maximum length of the string (0 or less for no limit)
//  pTimeout: The timeout (in milliseconds).  When the timeout is reached,
//            input stops and the user's input is returned.
//
// Return value: The user's input (string)
function getStrWithTimeout(pMode, pMaxLength, pTimeout)
{
	var inputStr = "";

	var mode = K_NONE;
	if (typeof(pMode) == "number")
		mode = pMode;
	var maxWidth = 0;
	if (typeof(pMaxLength) == "number")
			maxWidth = pMaxLength;
	var timeout = 0;
	if (typeof(pTimeout) == "number")
		timeout = pTimeout;

	var setNormalAttrAtEnd = false;
	if (((mode & K_LINE) == K_LINE) && (maxWidth > 0) && console.term_supports(USER_ANSI))
	{
		var curPos = console.getxy();
		printf("\1n\1w\1h\1" + "4%" + maxWidth + "s", "");
		console.gotoxy(curPos);
		setNormalAttrAtEnd = true;
	}

	var curPos = console.getxy();
	var userKey = "";
	do
	{
		userKey = console.inkey(mode, timeout);
		if ((userKey.length > 0) && isPrintableChar(userKey))
		{
			var allowAppendChar = true;
			if ((maxWidth > 0) && (inputStr.length >= maxWidth))
				allowAppendChar = false;
			if (allowAppendChar)
			{
				inputStr += userKey;
				console.print(userKey);
				++curPos.x;
			}
		}
		else if (userKey == BACKSPACE)
		{
			if (inputStr.length > 0)
			{
				inputStr = inputStr.substr(0, inputStr.length-1);
				console.gotoxy(curPos.x-1, curPos.y);
				console.print(" ");
				console.gotoxy(curPos.x-1, curPos.y);
				--curPos.x;
			}
		}
		else if (userKey == KEY_ENTER)
			userKey = "";
	} while(userKey.length > 0);

	if (setNormalAttrAtEnd)
		console.print("\1n");

	return inputStr;
}

// Returns whether or not a character is printable.
function isPrintableChar(pText)
{
   // Make sure pText is valid and is a string.
   if (typeof(pText) != "string")
      return false;
   if (pText.length == 0)
      return false;

   // Make sure the character is a printable ASCII character in the range of 32 to 254,
   // except for 127 (delete).
   var charCode = pText.charCodeAt(0);
   return ((charCode > 31) && (charCode < 255) && (charCode != 127));
}

// Finds the page number of an item, given some text to search for in the item.
//
// Parameters:
//  pText: The text to search for in the items
//  pNumItemsPerPage: The number of items per page
//  pFileDir: Boolean - If true, search the file directory list for the given library index.
//            If false, search the library list.
//  pStartItemIdx: The item index to start at
//  pLibIdx: The index of the library to search in (only for doing a sub-board search)
//
// Return value: An object containing the following properties:
//               pageNum: The page number of the item (1-based; will be 0 if not found)
//               pageTopIdx: The index of the top item on the page (or -1 if not found)
//               itemIdx: The index of the item (or -1 if not found)
function getPageNumFromSearch(pText, pNumItemsPerPage, pFileDir, pStartItemIdx, pLibIdx)
{
	var retObj = {
		pageNum: 0,
		pageTopIdx: -1,
		itemIdx: -1
	};

	// Sanity checking
	if ((typeof(pText) != "string") || (typeof(pNumItemsPerPage) != "number") || (typeof(pFileDir) != "boolean"))
		return retObj;

	// Convert the text to uppercase for case-insensitive searching
	var srchText = pText.toUpperCase();
	if (pFileDir)
	{
		if ((typeof(pLibIdx) == "number") && (pLibIdx >= 0) && (pLibIdx < file_area.lib_list.length))
		{
			// Go through the file directory list of the given library and
			// search for text in the descriptions
			for (var i = pStartItemIdx; i < file_area.lib_list[pLibIdx].dir_list.length; ++i)
			{
				if ((file_area.lib_list[pLibIdx].dir_list[i].description.toUpperCase().indexOf(srchText) > -1) ||
				    (file_area.lib_list[pLibIdx].dir_list[i].name.toUpperCase().indexOf(srchText) > -1))
				{
					retObj.itemIdx = i;
					// Figure out the page number and top index for the page
					var pageObj = calcPageNumAndTopPageIdx(i, pNumItemsPerPage);
					if ((pageObj.pageNum > 0) && (pageObj.pageTopIdx > -1))
					{
						retObj.pageNum = pageObj.pageNum;
						retObj.pageTopIdx = pageObj.pageTopIdx;
					}
					break;
				}
			}
		}
	}
	else
	{
		// Go through the file library list and look for a match
		for (var i = pStartItemIdx; i < file_area.lib_list.length; ++i)
		{
			if ((file_area.lib_list[i].name.toUpperCase().indexOf(srchText) > -1) ||
			    (file_area.lib_list[i].description.toUpperCase().indexOf(srchText) > -1))
			{
				retObj.itemIdx = i;
				// Figure out the page number and top index for the page
				var pageObj = calcPageNumAndTopPageIdx(i, pNumItemsPerPage);
				if ((pageObj.pageNum > 0) && (pageObj.pageTopIdx > -1))
				{
					retObj.pageNum = pageObj.pageNum;
					retObj.pageTopIdx = pageObj.pageTopIdx;
				}
				break;
			}
		}
	}

	return retObj;
}

// Calculates the page number (1-based) and top index for the page (0-based),
// given an item index.
//
// Parameters:
//  pItemIdx: The index of the item
//  pNumItemsPerPage: The number of items per page
//
// Return value: An object containing the following properties:
//               pageNum: The page number of the item (1-based; will be 0 if not found)
//               pageTopIdx: The index of the top item on the page (or -1 if not found)
function calcPageNumAndTopPageIdx(pItemIdx, pNumItemsPerPage)
{
	var retObj = {
		pageNum: 0,
		pageTopIdx: -1
	};

	var pageNum = 1;
	var topIdx = 0;
	var continueOn = true;
	do
	{
		var endIdx = topIdx + pNumItemsPerPage;
		if ((pItemIdx >= topIdx) && (pItemIdx < endIdx))
		{
			continueOn = false;
			retObj.pageNum = pageNum;
			retObj.pageTopIdx = topIdx;
		}
		else
		{
			++pageNum;
			topIdx = endIdx;
		}
	} while (continueOn);

	return retObj;
}