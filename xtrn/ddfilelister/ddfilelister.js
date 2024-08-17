/* This is a file lister door for Synchronet.
 *
 * Author: Eric Oulashin (AKA Nightfox)
 * BBS: Digital Distortion
 * BBS address: digitaldistortionbbs.com (or digdist.synchro.net)
 *
 * Date       Author            Description
 * 2022-01-17 Eric Oulashin     Version 0.01
 *                              Started work on this script
 * 2022-02-06 Eric Oulashin     Version 2.00
 *                              Functionality implemented (for lightbar/ANSI terminal).
 *                              Seems to work as expected.  Releasing this version.
 *                              I'm calling this version 2.00 because I had already
 *                              released a file lister mod years ago (modding the stock
 *                              Synchronet file list interface).
 * 2022-02-07 Eric Oulashin     Version 2.01
 *                              Fixed file description being undefined when viewing
 *                              file info.  Fixed command bar refreshing when pressing
 *                              the hotkeys.  Added an option to pause after viewing a
 *                              file (defaults to true).
 * 2022-02-13 Eric Oulashin     Version 2.02
 *                              Things overall look good. Releasing this version.  Added
 *                              the ability to do searching via filespec, description, and
 *                              new file search (started working on this 2022-02-08).
 * 2022-02-27 Eric Oulashin     Version 2.03
 *                              For terminals over 25 rows tall, the file info window will
 *                              now be up to 45 rows tall.  Also, fixed the display of the
 *                              trailing blocks for the list header for wide terminals (over
 *                              80 columns).
 * 2022-03-09 Eric Oulashin     Version 2.04
 *                              Bug fix: Now successfully formats filenames without extensions
 *                              when listing files.
 * 2022-03-12 Eric Oulashin     Version 2.05
 *                              Now makes use of the user's extended file description setting:
 *                              If the user's extended file description setting is enabled,
 *                              the lister will now show extended file descriptions on the
 *                              main screen in a split format, with the lightbar file list
 *                              on the left and the extended file description for the
 *                              highlighted file on the right.  Also, made the file info
 *                              window taller for terminals within 25 lines high.
 *                              I had started work on this on March 9, 2022.
 * 2022-03-13 Eric Oulashin     Version 2.05a
 *                              Fix for "fileDesc is not defined" error when displaying
 *                              the file description on the main screen.  Also made a
 *                              small refactor to the main screen refresh function.
 * 2022-04-13 Eric Oulashin     Version 2.06
 *                              When extended file descriptions are enabled, the file
 *                              date is now shown with the file description on the last
 *                              line.
 * 2022-12-02 Eric Oulashin     Version 2.07
 *                              In a file's extended description, added the number of times
 *                              downloaded and date/time last downloaded.  Also, fixed a bug
 *                              where some descriptions were blank in the Frame object because
 *                              of a leading normal attribute (the fix may be a kludge though).
 * 2023-01-18 Eric Oulashin     Version 2.08
 *                              When doing a file search in multiple directories, the file
 *                              library & directory is now shown in the header as the user
 *                              scrolls through the file list/search results.  Also,
 *                              used lfexpand() to ensure the extended description has
 *                              CRLF endings, useful for splitting it into multiple lines properly.
 * 2023-02-25 Eric Oulashin     Version 2.09
 *                              Now supports being used as a loadable module for
 *                              Scan Dirs and List Files
 * 2023-02-27 Eric Oulashin     Version 2.10
 *                              Now allows downloading a single selected file with the D key.
 *                              Also, ddfilelister now checks whether the user has permission to
 *                              download before allowing adding files to their batch download queue
 *                              (and downloading a single file as well).
 * 2023-05-14 Eric Oulashin     Version 2.11
 *                              Refactored the function that reads the configuration file. Also,
 *                              the theme configuration file can now just contain the attribute
 *                              characters, without the control character.
 *
 *                              Future work: Actual support for a traditional/non-lightbar user interface
 * 2023-07-29 Eric Oulashin     Version 2.12 Beta
 *                              Started working on implementing a traditional/non-lightbar UI
 * 2023-08-12 Eric Oulashin     Version 2.12
 *                              Releasing this version
 * 2023-08-13 Eric Oulashin     Version 2.13
 *                              Refactor for printing file info for traditional UI. Fixes for
 *                              quitting certain actions for traditional UI. Prints selected action
 *                              for traditional UI.
 * 2023-09-02 Eric Oulashin     Version 2.14
 *                              Fix for the lightbar interface: When erasing the file info window,
 *                              the file date is not shown on a duplicate line if the file date is
 *                              already showing in the description area (i.e., for a 1-line file
 *                              description)
 * 2023-09-16 Eric Oulashin     Version 2.15
 *                              Fix for "Empty directory" message after quitting (the lister must
 *                              exit with the number of files listed).  Also, updates for filename
 *                              searching, and help screen now should always pause.
 * 2023-09-17 Eric Oulashin     New configuration option: blankNFilesListedStrIfLoadableModule,
 *                              If true (default), then when started as a loadable module, replace the
 *                              "# Files Listed" text with an empty string so that it won't be displayed
 *                              after exit
 * 2023-11-11 Eric Oulashin     Version 2.15a
 *                              On start, if console.aborted is true (due to the user pressing Ctrl-C, etc.),
 *                              then return -1 to stop a file scan in progress.
 * 2024-02-02 Eric Oulashin     Version 2.15b
 *                              More checks for pFileList[pIdx] and the 'desc' property when getting the description
 * 2024-02-10 Eric Oulashin     Version 2.16
 *                              New sort option in the config file: PER_DIR_CFG, which has Synchronet sort
 *                              the file list according to the file directory's configuration (SCFG >
 *                              File Areas > library > File Directories > dir > Advanced Options > Sort Value and Direction)
 * 2024-02-28 Eric Oulashin     Version 2.17
 *                              Fix for possibly no file description when adding to the batch DL queue.
 *                              Also, fix for file description screen refresh (off by one column) for extended
 *                              descriptions
 * 2024-03-08 Eric Oulashin     Version 2.18
 *                              Bug fix: Got description search working when used as a loadable module.
 *                              Added Ctrl-C to the help screen to mention it can be used to abort.
 * 2024-03-11 Eric Oulashin     Version 2.19
 *                              Screen refresh fix: When printing the empty lines after an extended
 *                              description, ensure the whole line width is used (the last character
 *                              was being left there when it should have been written over with a space)
 * 2024-03-22 Eric Oulashin     Version 2.20
 *                              (Hopefully) Fix for descLines being undefined in getFileInfoLineArrayForTraditionalUI()
 * 2024-04-08 Eric Oulashin     Version 2.21 Beta
 *                              Fix: Searching by file date as a loadable module now does the new file search
 * 2024-04-09 Eric Oulashin     Version 2.21
 *                              Releasing this version
 * 2024-08-14 Eric Oulashin     Version 2.22 Beta
 *                              Started working on adding the ability to edit file details/information (for the sysop)
 * 2024-08-16 Eric Oulashin     Version 2.22
 *                              Releasing this version
*/

"use strict";

//js.on_exit("console.ctrlkey_passthru = " + console.ctrlkey_passthru);
//console.ctrlkey_passthru |= (1<<3); // Ctrl-C
//console.ctrlkey_passthru = "+C";
//console.ctrlkey_passthru = "-C";

// If the search action has been aborted, then return -1
if (console.aborted)
	exit(-1);

// This script requires Synchronet version 3.20 or newer (only for bbs.Text.<value>);
// Synchronet 3.19 added filebase support in JS.
// If the Synchronet version is below the minimum, then exit.
if (system.version_num < 32000)
{
	if (user.is_sysop)
	{
		var message = "\x01n\x01h\x01y\x01i* Warning:\x01n\x01h\x01w Digital Distortion File Lister "
		            + "requires version \x01g3.20\x01w or\r\n"
		            + "newer of Synchronet.  This BBS is using version \x01g" + system.version
		            + "\x01w.\x01n";
		console.crlf();
		console.print(message);
		console.crlf();
		console.pause();
	}
	exit(0);
}


require("sbbsdefs.js", "K_UPPER");
require("sbbsdefs.js", "K_UPPER");
require('key_defs.js', 'KEY_UP');
require("text.js", "Email"); // Text string definitions (referencing text.dat)
require("dd_lightbar_menu.js", "DDLightbarMenu");
require("frame.js", "Frame");
require("scrollbar.js", "ScrollBar");
require("mouse_getkey.js", "mouse_getkey");
require("attr_conv.js", "convertAttrsToSyncPerSysCfg");


// Lister version information
var LISTER_VERSION = "2.22";
var LISTER_DATE = "2024-08-16";


///////////////////////////////////////////////////////////////////////////////
// Global variables

var KEY_BACKSPACE = CTRL_H;

// Block characters
var BLOCK1 = "\xB0"; // Dimmest block
var BLOCK2 = "\xB1";
var BLOCK3 = "\xB2";
var BLOCK4 = "\xDB"; // Brightest block
var THIN_RECTANGLE_LEFT = "\xDD";
var THIN_RECTANGLE_RIGHT = "\xDE";
var RIGHT_T_HDOUBLE_VSINGLE = "\xB5";
var LEFT_T_HDOUBLE_VSINGLE = "\xcC6";

// For file sizes
//var BYTES_PER_TB = 1099511627776; // Seems to be too big for JS
var BYTES_PER_GB = 1073741824;
var BYTES_PER_MB = 1048576;
var BYTES_PER_KB = 1024;

// File list column indexes (0-based).  The end indexes are one past the last index.
// These defaults assume an 80-character wide terminal.
var gListIdxes = {
	filenameStart: 0
};
// The end index of each column includes the trailing space so that
// highlight colors will highlight the whole field
gListIdxes.filenameEnd = gListIdxes.filenameStart + 13;
gListIdxes.fileSizeStart = gListIdxes.filenameEnd;
gListIdxes.fileSizeEnd = gListIdxes.fileSizeStart + 7;
gListIdxes.descriptionStart = gListIdxes.fileSizeEnd;
gListIdxes.descriptionEnd = console.screen_columns - 1; // Leave 1 character remaining on the screen
// Colors
var gColors = {
	filename: "\x01n\x01b\x01h",
	fileSize: "\x01n\x01m\x01h",
	desc: "\x01n\x01w",
	bkgHighlight: "\x01n\x014",
	filenameHighlight: "\x01c\x01h",
	fileSizeHighlight: "\x01c\x01h",
	descHighlight: "\x01c\x01h",
	fileTimestamp: "\x01g\x01h",
	fileInfoWindowBorder: "\x01r",
	fileInfoWindowTitle: "\x01g",
	errorBoxBorder: "\x01g\x01h",
	errorMessage: "\x01y\x01h",
	successMessage: "\x01c",

	batchDLInfoWindowBorder: "\x01r",
	batchDLInfoWindowTitle: "\x01g",
	confirmFileActionWindowBorder: "\x01r",
	confirmFileActionWindowWindowTitle: "\x01g",

	fileAreaMenuBorder: "\x01b", // File move menu for lightbar interface
	fileNormalBkg: "\x01" + "4",
	fileAreaNum: "\x01w",
	fileAreaDesc: "\x01w",
	fileAreaNumItems: "\x01w",

	fileAreaMenuHighlightBkg: "\x017",
	fileAreaNumHighlight: "\x01b",
	fileAreaDescHighlight: "\x01b",
	fileAreaNumItemsHighlight: "\x01b",

	fileAreaMenuBorderTrad: "\x01b", // File move menu for traditional interface
	fileNormalBkgTrad: "\x01n\x01w",
	listNumTrad: "\x01g\x01h",
	fileAreaDescTrad: "\x01c",
	fileAreaNumItemsTrad: "\x01b\x01h"
};


// Actions
var FILE_VIEW_INFO = 1;
var FILE_VIEW = 2;
var FILE_ADD_TO_BATCH_DL = 3;
var FILE_DOWNLOAD_SINGLE = 4;
var FILE_EDIT = 5;
var HELP = 6;
var QUIT = 7;
var FILE_MOVE = 8;   // Sysop action
var FILE_DELETE = 9; // Sysop action

var NEXT_PAGE = 10;
var PREV_PAGE = 11;
var FIRST_PAGE = 12;
var LAST_PAGE = 13;

// Search/list modes
var MODE_LIST_DIR = 1;
var MODE_SEARCH_FILENAME = 2;
var MODE_SEARCH_DESCRIPTION = 3;
var MODE_NEW_FILE_SEARCH = 4;

// Sort orders (not included in FileBase.SORT)
var SORT_PER_DIR_CFG = 50; // Sort according to the file directory configuration
var SORT_FL_ULTIME = 51;   // Sort by upload time
var SORT_FL_DLTIME = 52;   // Sort by download time

// The searc/list mode for the current run
var gScriptMode = MODE_LIST_DIR; // Default
var gListBehavior = FL_NONE; // From sbbsdefs.js

// The directory internal code to list
var gDirCode = bbs.curdir_code;



// This will store the number of header lines that were displayed.  This will control
// the starting row of the file list menu.
var gNumHeaderLinesDisplayed = 0;

// The number of milliseconds to wait after displaying an error message
var gErrorMsgWaitMS = 1500;
// The upper-left position, width, & size of the error message box
var gErrorMsgBoxULX = 2;
var gErrorMsgBoxULY = 4;
var gErrorMsgBoxWidth = console.screen_columns - 2;
var gErrorMsgBoxHeight = 3;

// Whether or not to pause after viewing a file
var gPauseAfterViewingFile = true;

// Whether or not to use the lightbar interface (this could be specified as false
// in the configuration file, to use the traditional interface even if the user's
// terminal supports ANSI)
var gUseLightbarInterface = true;

// If using the traditional interface, whether to use Synchronet's stock
// file lister instead of ddfilelister
var gTraditionalUseSyncStock = false;

///////////////////////////////////////////////////////////////////////////////
// Script execution code

// The filename pattern to match
var gFilespec = "*";

// Description keyword to match (empty for no keyword search)
var gDescKeyword = "";

// The sort order to use for the file list
var gFileSortOrder = SORT_PER_DIR_CFG; // Use the file directory's configured sort order option

var gSearchVerbose = false;

// When called as a lodable module, one of the options is to scan all dirs
var gScanAllDirs = false;

// Setting from the configuration file: When used as a loadable module, whether
// or not to blank out the "# Files Listed" string (from text.dat) so that
// Synchronet won't display it after the lister exits
var gBlankNFilesListedStrIfLoadableModule = true;

// Read the configuration file and set the settings
readConfigFile();

// Parse command-line arguments (which sets program options)
var gRunningAsLoadableModule = parseArgs(argv);

// If set to use the traditional (non-lightbar) UI and if set to use the Synchronet
// stock file lister, then do so instead of using ddfilelister's traditional UI
if ((!gUseLightbarInterface || !console.term_supports(USER_ANSI)) && gTraditionalUseSyncStock)
{
	var exitCode = 0;
	if (gScriptMode == MODE_SEARCH_FILENAME || gScriptMode == MODE_SEARCH_DESCRIPTION || gScriptMode == MODE_NEW_FILE_SEARCH)
		bbs.scan_dirs(gListBehavior, gScanAllDirs);
	else
		exitCode = bbs.list_files(gDirCode, gFilespec, gListBehavior);
	exit(exitCode);
}

// This array will contain file metadata objects
var gFileList = [];

// Populate the file list based on the script mode (list/search).
// It's important that this is called before createFileListMenu(),
// since this adjusts gListIdxes.filenameEnd based on the longest
// filename length and terminal width.
var listPopRetObj = populateFileList(gScriptMode);
if (listPopRetObj.exitNow)
	exit(0); // listPopRetObj.exitCode

// If there are no files, then say so (if not running as a loadable module) and exit.
console.line_counter = 0;
if (gFileList.length == 0)
{
	if (!gRunningAsLoadableModule)
	{
		console.crlf();
		console.attributes = "NC";
		if (gScriptMode == MODE_LIST_DIR)
		{
			if (gFilespec == "*" || gFilespec == "*.*")
				console.print("There are no files in the directory.");
			else
				console.print("No files in the directory were found matching " + gFilespec);
		}
		else
			console.print("No files were found.");
		console.attributes = "N";
		console.crlf();
		//console.pause();
	}
	exit(0);
}

// Construct and display the menu/command bar at the bottom of the screen
var fileMenuBar = new DDFileMenuBar({ x: 1, y: console.screen_rows });
// Clear the screen and display the header lines
console.clear("\x01n");
if ((gListBehavior & FL_NO_HDR) != FL_NO_HDR)
	displayFileLibAndDirHeader(false, null, !gUseLightbarInterface || !console.term_supports(USER_ANSI));
// Create the file list menu (must be done after displayFileLibAndDirHeader() when using ANSI and lightbar)
var gFileListMenu = createFileListMenu(fileMenuBar.getAllActionKeysStr(true, true) + KEY_LEFT + KEY_RIGHT + KEY_DEL /*+ CTRL_C*/);
if (gUseLightbarInterface && console.term_supports(USER_ANSI))
{
	fileMenuBar.writePromptLine();
	// In a loop, show the file list menu, allowing the user to scroll the file list,
	// and respond to user input until the user decides to quit.
	gFileListMenu.Draw({});
	// If using extended descriptions, write the first file's description on the screen
	if (extendedDescEnabled())
		displayFileExtDescOnMainScreen(0);
	var continueDoingFileList = true;
	var drawFileListMenu = false; // For screen refresh optimization
	while (continueDoingFileList)
	{
		// Clear the menu's selected item indexes so it's 'fresh' for this round
		for (var prop in gFileListMenu.selectedItemIndexes)
			delete gFileListMenu.selectedItemIndexes[prop];
		var actionRetObj = null;
		var currentActionVal = null;
		var userChoice = gFileListMenu.GetVal(drawFileListMenu, gFileListMenu.selectedItemIndexes);
		drawFileListMenu = false; // For screen refresh optimization
		var lastUserInputUpper = gFileListMenu.lastUserInput != null ? gFileListMenu.lastUserInput.toUpperCase() : null;
		if (lastUserInputUpper == null || lastUserInputUpper == "Q" || console.aborted)
			continueDoingFileList = false;
		else if (lastUserInputUpper == KEY_LEFT)
			fileMenuBar.decrementMenuItemAndRefresh();
		else if (lastUserInputUpper == KEY_RIGHT)
			fileMenuBar.incrementMenuItemAndRefresh();
		else if (lastUserInputUpper == KEY_ENTER)
		{
			currentActionVal = fileMenuBar.getCurrentSelectedAction();
			fileMenuBar.setCurrentActionCode(currentActionVal);
			actionRetObj = doAction(currentActionVal, gFileList, gFileListMenu);
		}
		// Allow the delete key as a special key for sysops to delete the selected file(s). Also allow backspace
		// due to some terminals returning backspace for delete.
		else if (lastUserInputUpper == KEY_DEL || lastUserInputUpper == KEY_BACKSPACE)
		{
			if (user.is_sysop)
			{
				fileMenuBar.setCurrentActionCode(FILE_DELETE, true);
				actionRetObj = doAction(FILE_DELETE, gFileList, gFileListMenu);
				currentActionVal = FILE_DELETE;
			}
		}
		else
		{
			currentActionVal = fileMenuBar.getActionFromChar(lastUserInputUpper, false);
			fileMenuBar.setCurrentActionCode(currentActionVal, true);
			actionRetObj = doAction(currentActionVal, gFileList, gFileListMenu);
		}
		// If an action was done (actionRetObj is not null), then look at actionRetObj and
		// do what's needed.  Note that quit (for the Q key) is already handled.
		if (actionRetObj != null)
		{
			if (actionRetObj.exitNow)
				continueDoingFileList = false;
			else
			{
				if ((gListBehavior & FL_NO_HDR) != FL_NO_HDR)
				{
					if (actionRetObj.reDrawHeaderTextOnly)
					{
						console.attributes = "N";
						displayFileLibAndDirHeader(true, null, gFileListMenu.numberedMode); // Will move the cursor where it needs to be
					}
					else if (actionRetObj.reDrawListerHeader)
					{
						console.attributes = "N";
						console.gotoxy(1, 1);
						displayFileLibAndDirHeader(false, null, gFileListMenu.numberedMode);
					}
				}
				if (actionRetObj.reDrawCmdBar) // Could call fileMenuBar.constructPromptText(); if needed
					fileMenuBar.writePromptLine();
				var redrewPartOfFileListMenu = false;
				// If we are to re-draw the main screen content, then
				// enable the flag to draw the file list menu on the next
				// GetVal(); also, if extended descriptions are being shown,
				// write the current file's extended description too.
				if (actionRetObj.reDrawMainScreenContent)
				{
					drawFileListMenu = true;
					if (extendedDescEnabled())
						displayFileExtDescOnMainScreen(gFileListMenu.selectedItemIdx);
				}
				else
				{
					// If there is partial redraw information available, then use it
					// to re-draw that part of the main screen
					if (actionRetObj.fileListPartialRedrawInfo != null)
					{
						drawFileListMenu = false;
						var startX = actionRetObj.fileListPartialRedrawInfo.absStartX;
						var startY = actionRetObj.fileListPartialRedrawInfo.absStartY;
						var width = actionRetObj.fileListPartialRedrawInfo.width;
						var height = actionRetObj.fileListPartialRedrawInfo.height;
						refreshScreenMainContent(startX, startY, width, height, true);
						actionRetObj.refreshedSelectedFilesAlready = true;
						redrewPartOfFileListMenu = true;
					}
					else
					{
						// Partial screen re-draw information was not returned.
						continueDoingFileList = actionRetObj.continueFileLister;
						drawFileListMenu = actionRetObj.reDrawMainScreenContent;
						// If displaying extended descriptions and the user deleted some files, then
						// refresh the file description area to erase the delete confirmation text
						if (extendedDescEnabled()/* && currentActionVal == FILE_DELETE*/)
						{
							if (actionRetObj.hasOwnProperty("filesDeleted") && actionRetObj.filesDeleted)
							{
								var numFiles = gFileListMenu.NumItems();
								if (numFiles > 0 && gFileListMenu.selectedItemIdx >= 0 && gFileListMenu.selectedItemIdx < numFiles)
									displayFileExtDescOnMainScreen(gFileListMenu.selectedItemIdx);
							}
							else
							{
								var firstLine = startY + gFileListMenu.pos.y;
								var lastLine = console.screen_rows - 1;
								var width = console.screen_columns - gFileListMenu.size.width - 1;
								displayFileExtDescOnMainScreen(gFileListMenu.selectedItemIdx, firstLine, lastLine, width);
							}
						}
					}
				}
				// Remove checkmarks from any selected files in the file menu.
				// For efficiency, we'd probably only do this if not re-drawing the wohle
				// menu, but that's not working for now.
				if (!actionRetObj.refreshedSelectedFilesAlready && /*!drawFileListMenu &&*/ gFileListMenu.numSelectedItemIndexes() > 0)
				{
					var bottomItemIdx = gFileListMenu.GetBottomItemIdx();
					var redrawTopY = -1;
					var redrawBottomY = -1;
					if (actionRetObj.fileListPartialRedrawInfo != null)
					{
						redrawTopY = actionRetObj.fileListPartialRedrawInfo.absStartY;
						redrawBottomY = actionRetObj.fileListPartialRedrawInfo.height + height - 1;
					}
					for (var idx in gFileListMenu.selectedItemIndexes)
					{
						var idxNum = +idx;
						if (idxNum >= gFileListMenu.topItemIdx && idxNum <= bottomItemIdx)
						{
							var drawItem = true;
							if (redrawTopY > -1 && redrawBottomY > redrawTopY)
							{
								var screenRowForItem = gFileListMenu.ScreenRowForItem(idxNum);
								drawItem = (screenRowForItem < redrawTopY || screenRowForItem > redrawBottomY)
							}
							if (drawItem)
							{
								var isSelected = (idxNum == gFileListMenu.selectedItemIdx);
								gFileListMenu.WriteItemAtItsLocation(idxNum, isSelected, false);
							}
							else
								console.print("\x01n\r\nNot drawing idx " + idxNum + "\r\n\x01p");
						}
					}
				}
				// If part of the file list menu was re-drawn (partially, not completely), move the cursor
				// to the lower-right corner of the screen so that it's out of the way
				if (redrewPartOfFileListMenu)
					console.gotoxy(console.screen_columns-1, console.screen_rows);
			}
		}
	}

	// Move the cursor to the last line on the screen and do a CRLF, because when used as a loadable
	// module, Synchronet will output a CRLF and output the number of files listed.
	console.attributes = "N";
	console.gotoxy(1, console.screen_rows);
	//console.crlf();
}
else
{
	// Traditional UI
	console.crlf();

	// An array containing text descriptions for all files, which may include
	// multiple lines for files with an extended description (if enabled)
	var formatInfo = getTraditionalFileInfoFormatInfo();
	var allFileInfoLines = [];
	for (var i = 0; i < gFileList.length; ++i)
		allFileInfoLines = allFileInfoLines.concat(getFileInfoLineArrayForTraditionalUI(gFileList, i, formatInfo));

	// Number of files per page, assuming 1-line descriptions; 3 lines for top
	// header and 1 line for bottom key help line
	var numLinesPerPage = console.screen_rows - 4;
	var topItemIdx = 0;
	var topItemIndexForLastPage = allFileInfoLines.length - numLinesPerPage;

	// Allowed keys for user input
	var validOptionKeys = "IVBDEMNPFLQ?" + KEY_PAGEDN + KEY_PAGEUP + KEY_HOME + KEY_END;
	if (user.is_sysop)
		validOptionKeys += KEY_DEL + KEY_BACKSPACE;
	// If the user's terminal supports ANSI, then also allow left, right, and enter (option navigation & selection)
	if (console.term_supports(USER_ANSI))
		validOptionKeys += KEY_LEFT + KEY_RIGHT + KEY_UP + KEY_DOWN + KEY_ENTER;

	// User input loop
	var continueOn = true;
	var drawDirHeaderLines = false;
	var drawMenu = true;
	var refreshWholePromptLine = true;
	while (continueOn)
	{
		if (drawDirHeaderLines && (gListBehavior & FL_NO_HDR) != FL_NO_HDR)
		{
			if (console.term_supports(USER_ANSI))
				console.clear("\x01n");
			displayFileLibAndDirHeader(false, null, true);
			console.crlf();
		}
		if (drawMenu)
		{
			// Draw the current page of items
			console.line_counter = 0;
			var lastItemIdx = topItemIdx + numLinesPerPage - 1;
			for (var i = topItemIdx; i <= lastItemIdx; ++i)
			{
				if (i < allFileInfoLines.length)
					console.print(allFileInfoLines[i]);
				//else
				//	printf("%-*s", console.screen_columns, ""); // Ensure the line is blank
				console.crlf();
			}
		}

		if (refreshWholePromptLine || drawMenu)
			fileMenuBar.pos = console.getxy();
		if (refreshWholePromptLine)
			fileMenuBar.writePromptLine();
		var userInput = console.getkeys(validOptionKeys, -1, K_UPPER|K_NOECHO|K_NOSPIN|K_NOCRLF).toString();
		// If the user pressed the enter key, change userInput to the key
		// corresponding to what we'd expect for that option
		if (userInput == KEY_ENTER)
		{
			//currentActionVal = fileMenuBar.getCurrentSelectedAction();
			switch (fileMenuBar.getCurrentSelectedAction())
			{
				case NEXT_PAGE:
					userInput = KEY_PAGEDN;
					break;
				case PREV_PAGE:
					userInput = KEY_PAGEUP;
					break;
				case FIRST_PAGE:
					userInput = "F";
					break;
				case LAST_PAGE:
					userInput = "L";
					break;
				case QUIT:
					continueOn = false;
					break;
			}
		}
		// Check action based on the user's last input
		if (userInput == KEY_LEFT)
		{
			fileMenuBar.decrementMenuItemAndRefresh();
			drawDirHeaderLines = false;
			drawMenu = false;
			refreshWholePromptLine = false;
		}
		else if (userInput == KEY_RIGHT)
		{
			fileMenuBar.incrementMenuItemAndRefresh();
			drawDirHeaderLines = false;
			drawMenu = false;
			refreshWholePromptLine = false;
		}
		else if (userInput == KEY_ENTER)
		{
			drawDirHeaderLines = true;
			drawMenu = true;
			refreshWholePromptLine = true;
			currentActionVal = fileMenuBar.getCurrentSelectedAction();
			fileMenuBar.setCurrentActionCode(currentActionVal);
			actionRetObj = doAction(currentActionVal, gFileList, gFileListMenu);
		}
		// Allow the delete key as a special key for sysops to delete the selected file(s). Also allow backspace
		// due to some terminals returning backspace for delete.
		else if (userInput == KEY_DEL || userInput == KEY_BACKSPACE)
		{
			if (user.is_sysop)
			{
				drawDirHeaderLines = true;
				drawMenu = true;
				refreshWholePromptLine = true;
				fileMenuBar.setCurrentActionCode(FILE_DELETE, true);
				actionRetObj = doAction(FILE_DELETE, gFileList, gFileListMenu);
				currentActionVal = FILE_DELETE;
			}
			else
			{
				drawDirHeaderLines = false;
				drawMenu = false;
				refreshWholePromptLine = false;
			}
		}
		else if (userInput == "Q" || console.aborted)
			continueOn = false;
		else if (userInput == KEY_UP)
		{
			// One line up
			if (topItemIdx > 0)
			{
				--topItemIdx;
				drawDirHeaderLines = true;
				drawMenu = true;
				refreshWholePromptLine = true;
			}
			else
			{
				drawDirHeaderLines = false;
				drawMenu = false;
				refreshWholePromptLine = false;
			}
		}
		else if (userInput == KEY_DOWN)
		{
			// One line down
			if (topItemIdx < topItemIndexForLastPage)
			{
				++topItemIdx;
				drawDirHeaderLines = true;
				drawMenu = true;
				refreshWholePromptLine = true;
			}
			else
			{
				drawDirHeaderLines = false;
				drawMenu = false;
				refreshWholePromptLine = false;
			}
		}
		else if (userInput == "N" || userInput == KEY_PAGEDN)
		{
			// Next page
			if (topItemIdx < topItemIndexForLastPage)
			{
				topItemIdx += numLinesPerPage;
				if (topItemIdx > topItemIndexForLastPage)
					topItemIdx = topItemIndexForLastPage;
				drawDirHeaderLines = true;
				drawMenu = true;
				refreshWholePromptLine = true;
			}
			else
			{
				drawDirHeaderLines = false;
				drawMenu = false;
				refreshWholePromptLine = false;
			}
			currentActionVal = NEXT_PAGE;
			//fileMenuBar.setCurrentActionCode(NEXT_PAGE, !refreshWholePromptLine);
			fileMenuBar.setCurrentActionCode(NEXT_PAGE, true);
		}
		else if (userInput == "P" || userInput == KEY_PAGEUP)
		{
			// Previous page
			if (topItemIdx > 0)
			{
				topItemIdx -= numLinesPerPage;
				if (topItemIdx < 0)
					topItemIdx = 0;
				drawDirHeaderLines = true;
				drawMenu = true;
				refreshWholePromptLine = true;
			}
			else
			{
				drawDirHeaderLines = false;
				drawMenu = false;
				refreshWholePromptLine = false;
			}
			currentActionVal = PREV_PAGE;
			//fileMenuBar.setCurrentActionCode(PREV_PAGE, !refreshWholePromptLine);
			fileMenuBar.setCurrentActionCode(PREV_PAGE, true);
		}
		else if (userInput == "F" || userInput == KEY_HOME)
		{
			// First page
			if (topItemIdx > 0)
			{
				topItemIdx = 0;
				drawDirHeaderLines = true;
				drawMenu = true;
				refreshWholePromptLine = true;
			}
			else
			{
				drawDirHeaderLines = false;
				drawMenu = false;
				refreshWholePromptLine = false;
			}
			currentActionVal = FIRST_PAGE;
			//fileMenuBar.setCurrentActionCode(FIRST_PAGE, !refreshWholePromptLine);
			fileMenuBar.setCurrentActionCode(FIRST_PAGE, true);
		}
		else if (userInput == "L" || userInput == KEY_END)
		{
			// Last page
			if (topItemIdx < topItemIndexForLastPage)
			{
				topItemIdx = topItemIndexForLastPage;
				drawDirHeaderLines = true;
				drawMenu = true;
				refreshWholePromptLine = true;
			}
			else
			{
				drawDirHeaderLines = false;
				drawMenu = false;
				refreshWholePromptLine = false;
			}
			currentActionVal = LAST_PAGE;
			//fileMenuBar.setCurrentActionCode(LAST_PAGE, !refreshWholePromptLine);
			fileMenuBar.setCurrentActionCode(LAST_PAGE, true);
		}
		else
		{
			drawDirHeaderLines = true;
			drawMenu = true;
			refreshWholePromptLine = true;
			currentActionVal = fileMenuBar.getActionFromChar(userInput, false);
			fileMenuBar.setCurrentActionCode(currentActionVal, true);
			actionRetObj = doAction(currentActionVal, gFileList, gFileListMenu);
		}
	}
}

// The exit code needs to be the number of files listed (this is important if this
// script is used as a loadable module).
exit(gFileList.length);


///////////////////////////////////////////////////////////////////////////////
// Functions: File actions

// Performs a specified file action based on an action code.
// This works for both the lightbar/ANSI and traditional/non-lightbar UI
//
// Parameters:
//  pActionCode: A code specifying an action to do.  Must be one of the global
//               action codes.
//  pFileList: The list of file metadata objects, as retrieved from the filebase
//  pFileListMenu: The file list menu
//
// Return value: An object with values to indicate status & screen refresh actions; see
//               getDefaultActionRetObj() for details.
function doAction(pActionCode, pFileList, pFileListMenu)
{
	if (typeof(pActionCode) !== "number")
		return getDefaultActionRetObj();
	if (pActionCode < 0)
		return getDefaultActionRetObj();

	// If not using the ANSI interface, then prompt the user for the file number
	// (for options that need one)
	var useANSIInterface = gUseLightbarInterface && console.term_supports(USER_ANSI);
	var fileIdx = pFileListMenu.selectedItemIdx;
	if (!useANSIInterface && pActionCode != QUIT && pActionCode != HELP && pActionCode != NEXT_PAGE && pActionCode != PREV_PAGE)
	{
		console.crlf();
		console.print(getActionStr(pActionCode) + "\x01n");
		console.crlf();
		var numMenuItems = pFileListMenu.NumItems();
		console.attributes = "N";
		console.crlf();
		console.print("\x01cFile # (\x01h1-" + numMenuItems + "\x01n\x01c)\x01h\x01g: \x01g");
		var userInput = console.getnum(numMenuItems);
		console.attributes = "N";
		if (userInput >= 1 && userInput <= numMenuItems)
		{
			fileIdx = userInput - 1;
			pFileListMenu.selectedItemIdx = fileIdx;
		}
		else // User chose to quit (via Q, 0, or not entering a number)
			return getDefaultActionRetObj();
	}

	var fileMetadata = pFileList[fileIdx];

	var retObj = null;
	switch (pActionCode)
	{
		case FILE_VIEW_INFO:
			retObj = useANSIInterface ? showFileInfo_ANSI(fileMetadata) : showFileInfo_noANSI(fileMetadata);
			break;
		case FILE_VIEW:
			retObj = viewFile(fileMetadata);
			break;
		case FILE_ADD_TO_BATCH_DL:
			if (userCanDownloadFromFileArea_ShowErrorIfNot(fileMetadata.dirCode))
				retObj = addSelectedFilesToBatchDLQueue(fileMetadata, pFileList);
			else
			{
				retObj = getDefaultActionRetObj();
				retObj.reDrawListerHeader = true;
				retObj.reDrawHeaderTextOnly = false;
				retObj.reDrawMainScreenContent = true;
				retObj.reDrawCmdBar = true;
			}
			break;
		case FILE_DOWNLOAD_SINGLE:
			if (userCanDownloadFromFileArea_ShowErrorIfNot(fileMetadata.dirCode) && pFileListMenu.selectedItemIdx >= 0 && pFileListMenu.selectedItemIdx < pFileListMenu.NumItems())
				retObj = letUserDownloadSelectedFile(fileMetadata);
			else
			{
				retObj = getDefaultActionRetObj();
				retObj.reDrawListerHeader = true;
				retObj.reDrawHeaderTextOnly = false;
				retObj.reDrawMainScreenContent = true;
				retObj.reDrawCmdBar = true;
			}
			break;
		case HELP:
			retObj = displayHelpScreen();
			break;
		case QUIT:
			retObj = getDefaultActionRetObj();
			retObj.continueFileLister = false;
			break;
		case FILE_EDIT:
			retObj = editFileInfo(pFileList, pFileListMenu);
			break;
		case FILE_MOVE: // Sysop action
			if (user.is_sysop)
				retObj = chooseFilebaseAndMoveFileToOtherFilebase(pFileList, pFileListMenu);
			break;
		case FILE_DELETE: // Sysop action
			if (user.is_sysop)
				retObj = confirmAndRemoveFilesFromFilebase(pFileList, pFileListMenu);
			break;
	}

	return retObj;
}

// Returns a string representing an action code, for relevant actions (not necessarily all actions)
//
// Parameters:
//  pActionCode: A code specifying an action to do.  Must be one of the global
//               action codes.
//
// Return value: A string representing the action code
function getActionStr(pActionCode)
{
	var actionCodeStr = "";
	switch (pActionCode)
	{
		case FILE_VIEW_INFO:
			actionCodeStr = "View File Info";
			break;
		case FILE_VIEW:
			actionCodeStr = "View File";
			break;
		case FILE_ADD_TO_BATCH_DL:
			actionCodeStr = "Add File To Batch DL";
			break;
		case FILE_DOWNLOAD_SINGLE:
			actionCodeStr = "Download File";
			break;
		case FILE_EDIT:
			actionCodeStr = "Edit File Info";
			break;
		case FILE_MOVE:
			actionCodeStr = "Move File";
			break;
		case FILE_DELETE:
			actionCodeStr = "Remove File";
			break;
	}
	return applyColorsToWords(actionCodeStr, "\x01c\x01h", "\x01c");
}
// Given a string, applies one color to the first character in each word and another color to
// the rest of the letters in each word. Returns the modified string.
function applyColorsToWords(pStr, pFirstCharColor, pNextCharsColor)
{
	if (typeof(pStr) !== "string")
		return "";
	if (typeof(pFirstCharColor) !== "string" || typeof(pNextCharsColor) !== "string")
		return pStr;
	if (pStr.length == 0)
		return "";

	var updatedStr = "";
	var words = pStr.split(" ");
	for (var i = 0; i < words.length; ++i)
	{
		if (words[i].length > 0)
		{
			updatedStr += "\x01n" + pFirstCharColor + words[i].charAt(0);
			if (words[i].length > 1)
				updatedStr += "\x01n" + pNextCharsColor + words[i].substr(1);
			updatedStr += " ";
		}
		else
			updatedStr += " ";
	}
	return updatedStr;
}

// Returns an object for use for returning from performing a file action,
// with default values.
//
// Return value: An object with the following properties:
//               continueFileLister: Boolean - Whether or not the file lister should continue, or exit
//               reDrawMainScreenContent: Boolean - Whether or not to re-draw the main screen content
//                                        (file list, and extended description area if applicable)
//               reDrawListerHeader: Boolean - Whether or not to re-draw the header at the top of the screen
//               reDrawHeaderTextOnly: Boolean - Whether or not to re-draw the header text only.  This should
//                                     take precedence over reDrawListerHeader.
//               reDrawCmdBar: Boolean - Whether or not to re-draw the command bar at the bottom of the screen
//               fileListPartialRedrawInfo: If part of the file list menu needs to be re-drawn,
//                                          this will be an object that includes the following properties:
//                                          startX: The starting X coordinate for where to re-draw
//                                          startY: The starting Y coordinate for where to re-draw
//                                          width: The width to re-draw
//                                          height: The height to re-draw
//               refreshedSelectedFilesAlready: Whether or not selected file checkmark items
//                                              have already been refreshed (boolean)
//               exitNow: Exit the file lister now (boolean)
//               If no part of the file list menu needs to be re-drawn, this will be null.
function getDefaultActionRetObj()
{
	return {
		continueFileLister: true,
		reDrawMainScreenContent: false,
		reDrawListerHeader: false,
		reDrawHeaderTextOnly: false,
		reDrawCmdBar: false,
		fileListPartialRedrawInfo: null,
		refreshedSelectedFilesAlready: false,
		exitNow: false
	};
}

// Shows extended information about a file to the user (ANSI version).
//
// Parameters:
//  pFileMetadata: The file metadata object for the file to view information about
//
// Return value: An object with values to indicate status & screen refresh actions; see
//               getDefaultActionRetObj() for details.
function showFileInfo_ANSI(pFileMetadata)
{
	var retObj = getDefaultActionRetObj();
	if (pFileMetadata == undefined || typeof(pFileMetadata) !== "object")
		return retObj;

	// The width of the frame to display the file info (including borders).  This
	// is declared early so that it can be used for string length adjustment.
	//var frameWidth = pFileListMenu.size.width - 4; // TODO: Remove?
	var frameWidth = console.screen_columns - 4;

	// pFileList[pFileListMenu.selectedItemIdx] has a file metadata object without
	// extended information.  Get a metadata object with extended information so we
	// can display the extended description.
	// The metadata object in pFileList should have a dirCode added by this script.
	var dirCode = gDirCode;
	if (pFileMetadata.hasOwnProperty("dirCode"))
		dirCode = pFileMetadata.dirCode;
	var fileMetadata = null;
	if (extendedDescEnabled())
		fileMetadata = pFileMetadata;
	else
		fileMetadata = getFileInfoFromFilebase(dirCode, pFileMetadata.name, FileBase.DETAIL.EXTENDED);
	// Build a string with the file information
	// Make sure the displayed filename isn't too crazy long
	var frameInnerWidth = frameWidth - 2; // Without borders
	var adjustedFilename = shortenFilename(fileMetadata.name, frameInnerWidth, false);
	var fileInfoStr = "\x01n\x01wFilename";
	if (adjustedFilename.length < fileMetadata.name.length)
		fileInfoStr += " (shortened)";
	fileInfoStr += ":\r\n";
	fileInfoStr += gColors.filename + adjustedFilename +  "\x01n\x01w\r\n";
	// Note: File size can also be retrieved by calling a FileBase's get_size(fileMetadata.name)
	// TODO: Shouldn't need the max length here
	fileInfoStr += "Size: " + gColors.fileSize + getFileSizeStr(fileMetadata.size, 99999) + "\x01n\x01w\r\n";
	fileInfoStr += "Timestamp: " + gColors.fileTimestamp + strftime("%Y-%m-%d %H:%M:%S", fileMetadata.time) + "\x01n\x01w\r\n";
	fileInfoStr += "\r\n";

	// File library/directory information
	var libIdx = file_area.dir[dirCode].lib_index;
	var dirIdx = file_area.dir[dirCode].index;
	var libDesc = file_area.lib_list[libIdx].description;
	var dirDesc =  file_area.dir[dirCode].description;
	fileInfoStr += format("\x01c\x01h%s\x01g: \x01n\x01c%s\x01n\x01w\r\n", "Lib", libDesc.substr(0, frameInnerWidth-5));
	fileInfoStr += format("\x01c\x01h%s\x01g: \x01n\x01c%s\x01n\x01w\r\n", "Dir", dirDesc.substr(0, frameInnerWidth-5));
	fileInfoStr += "\r\n";

	// fileMetadata should have extdDesc, but check just in case
	var fileDesc = "";
	if (fileMetadata.hasOwnProperty("extdesc") && fileMetadata.extdesc.length > 0)
		fileDesc = fileMetadata.extdesc;
	else
		fileDesc = fileMetadata.desc;
	// It's possible for fileDesc to be undefined (due to extDesc or desc being undefined),
	// so make sure it's a string.
	// Also, if it's a string, reformat certain types of strings that don't look good in a
	// Frame object
	if (typeof(fileDesc) === "string")
	{
		// Check to see if it starts with a normal attribute and remove if so,
		// since that seems to cause problems with displaying the description in a Frame object.  This
		// may be a kludge, and perhaps there's a better solution..
		fileDesc = fileDesc.replace(/^\x01[nN]/, "");
		// Fix line endings if necessary
		fileDesc = lfexpand(fileDesc);
	}
	else
		fileDesc = "";
	// This might be overkill, but just in case, convert any non-Synchronet
	// attribute codes to Synchronet attribute codes in the description.
	if (!fileMetadata.hasOwnProperty("attrsConverted"))
	{
		fileDesc = convertAttrsToSyncPerSysCfg(fileDesc);
		fileMetadata.attrsConverted = true;
		if (fileMetadata.hasOwnProperty("extdesc"))
			fileMetadata.extdesc = fileDesc;
		else
			fileMetadata.desc = fileDesc;
	}

	fileInfoStr += gColors.desc;
	if (fileDesc.length > 0)
		fileInfoStr += "Description:\r\n" + fileDesc; // Don't want to use strip_ctrl(fileDesc)
	else
		fileInfoStr += "No description available";
	fileInfoStr += "\r\n";
	// # of times downloaded and last downloaded date/time
	var fieldFormatStr = "\r\n\x01n\x01c\x01h%s\x01g:\x01n\x01c %s";
	var timesDownloaded = fileMetadata.hasOwnProperty("times_downloaded") ? fileMetadata.times_downloaded : 0;
	fileInfoStr += format(fieldFormatStr, "Times downloaded", timesDownloaded);
	if (fileMetadata.hasOwnProperty("last_downloaded"))
		fileInfoStr += format(fieldFormatStr, "Last downloaded", strftime("%Y-%m-%d %H:%M", fileMetadata.last_downloaded));
	// Some more fields for the sysop
	if (user.is_sysop)
	{
		var sysopFields = [ "from", "cost", "added" ];
		for (var sI = 0; sI < sysopFields.length; ++sI)
		{
			var prop = sysopFields[sI];
			if (fileMetadata.hasOwnProperty(prop))
			{
				if (typeof(fileMetadata[prop]) === "string" && fileMetadata[prop].length == 0)
					continue;
				var propName = prop.charAt(0).toUpperCase() + prop.substr(1);
				var infoValue = "";
				if (prop == "added")
					infoValue = strftime("%Y-%m-%d %H:%M:%S", fileMetadata.added);
				else
					infoValue = fileMetadata[prop].toString().substr(0, frameInnerWidth);
				fileInfoStr += format(fieldFormatStr, propName, infoValue);
				//fileInfoStr += "\x01n\x01w";
			}
		}
	}
	// Append a final CR & LF so that all lines can be split on those
	fileInfoStr += "\r\n";
	//fileInfoStr += "\x01n\x01w";

	// Construct & draw a frame with the file information & do the input loop
	// for the frame until the user closes the frame.
	var frameUpperLeftX = 3;
	var frameUpperLeftY = gNumHeaderLinesDisplayed + 3;
	// Note: frameWidth is declared earlier
	var frameHeight = console.screen_rows - 4 - frameUpperLeftY;
	// If the user's console is more than 25 rows high, then make the info window
	// taller so that its bottom row is 10 from the bottom, but only up to 45 rows tall.
	if (console.screen_rows > 25)
	{
		var frameBottomRow = console.screen_rows - 4;
		frameHeight = frameBottomRow - frameUpperLeftY + 1;
		if (frameHeight > 45)
			frameHeight = 45;
	}
	var frameTitle = "File Info";
	displayBorderedFrameAndDoInputLoop(frameUpperLeftX, frameUpperLeftY, frameWidth, frameHeight,
	                                   gColors.fileInfoWindowBorder, frameTitle,
	                                   gColors.fileInfoWindowTitle, fileInfoStr);

	// Construct the file list redraw info.  Note that the X and Y are relative
	// to the file list menu, not absolute screen coordinates.
	retObj.fileListPartialRedrawInfo = {
		startX: frameUpperLeftX - gFileListMenu.pos.x + 1, // Relative to the file menu
		startY: frameUpperLeftY - gFileListMenu.pos.y + 1, // Relative to the file menu
		absStartX: frameUpperLeftX,
		absStartY: frameUpperLeftY,
		width: frameWidth,
		height: frameHeight
	};

	return retObj;
}
// Shows extended information about a file to the user (non-ANSI/traditional version).
//
// Parameters:
//  pFileMetadata: The file metadata object for the file to view information about
//
// Return value: An object with values to indicate status & screen refresh actions; see
//               getDefaultActionRetObj() for details.
function showFileInfo_noANSI(pFileMetadata)
{
	var retObj = getDefaultActionRetObj();
	if (pFileMetadata == undefined || typeof(pFileMetadata) !== "object")
		return retObj;

	// pFileList[pFileListMenu.selectedItemIdx] has a file metadata object without
	// extended information.  Get a metadata object with extended information so we
	// can display the extended description.
	// The metadata object in pFileList should have a dirCode added by this script.
	var dirCode = gDirCode;
	if (pFileMetadata.hasOwnProperty("dirCode"))
		dirCode = pFileMetadata.dirCode;
	var fileMetadata = null;
	if (extendedDescEnabled())
		fileMetadata = pFileMetadata;
	else
		fileMetadata = getFileInfoFromFilebase(dirCode, pFileMetadata.name, FileBase.DETAIL.EXTENDED);

	console.print("\x01n\x01wFilename:\r\n");
	console.print(gColors.filename + fileMetadata.name +  "\x01n\x01w\r\n");
	console.print("Size: " + gColors.fileSize + getFileSizeStr(fileMetadata.size, 99999) + "\x01n\x01w\r\n");
	console.print("Timestamp: " + gColors.fileTimestamp + strftime("%Y-%m-%d %H:%M:%S", fileMetadata.time) + "\x01n\x01w\r\n");
	console.crlf();

	// File library/directory information
	var libIdx = file_area.dir[dirCode].lib_index;
	var dirIdx = file_area.dir[dirCode].index;
	var libDesc = file_area.lib_list[libIdx].description;
	var dirDesc =  file_area.dir[dirCode].description;
	console.print("\x01c\x01hLib\x01g: \x01n\x01c" + libDesc + "\x01n\x01w\r\n");
	console.print("\x01c\x01hDir\x01g: \x01n\x01c" + dirDesc + "\x01n\x01w\r\n");
	console.crlf();

	// fileMetadata should have extdDesc, but check just in case
	var fileDesc = "";
	if (fileMetadata.hasOwnProperty("extdesc") && fileMetadata.extdesc.length > 0)
		fileDesc = fileMetadata.extdesc;
	else
		fileDesc = fileMetadata.desc;
	// It's possible for fileDesc to be undefined (due to extDesc or desc being undefined),
	// so make sure it's a string.
	// Also, if it's a string, reformat certain types of strings that don't look good in a
	// Frame object
	if (typeof(fileDesc) === "string")
	{
		// Check to see if it starts with a normal attribute and remove if so,
		// since that seems to cause problems with displaying the description in a Frame object.  This
		// may be a kludge, and perhaps there's a better solution..
		fileDesc = fileDesc.replace(/^\x01[nN]/, "");
		// Fix line endings if necessary
		fileDesc = lfexpand(fileDesc);
	}
	else
		fileDesc = "";
	// This might be overkill, but just in case, convert any non-Synchronet
	// attribute codes to Synchronet attribute codes in the description.
	if (!fileMetadata.hasOwnProperty("attrsConverted"))
	{
		fileDesc = convertAttrsToSyncPerSysCfg(fileDesc);
		fileMetadata.attrsConverted = true;
		if (fileMetadata.hasOwnProperty("extdesc"))
			fileMetadata.extdesc = fileDesc;
		else
			fileMetadata.desc = fileDesc;
	}

	console.print(gColors.desc);
	if (fileDesc.length > 0)
		console.print("Description:\r\n" + fileDesc); // Don't want to use strip_ctrl(fileDesc)
	else
		console.print("No description available");
	console.crlf();
	// # of times downloaded and last downloaded date/time
	var fieldFormatStr = "\x01n\x01c\x01h%s\x01g:\x01n\x01c %s\x01n\r\n";
	var timesDownloaded = fileMetadata.hasOwnProperty("times_downloaded") ? fileMetadata.times_downloaded : 0;
	printf(fieldFormatStr, "Times downloaded", timesDownloaded);
	if (fileMetadata.hasOwnProperty("last_downloaded"))
		printf(fieldFormatStr, "Last downloaded", strftime("%Y-%m-%d %H:%M", fileMetadata.last_downloaded));
	// Some more fields for the sysop
	if (user.is_sysop)
	{
		var sysopFields = [ "from", "cost", "added" ];
		for (var sI = 0; sI < sysopFields.length; ++sI)
		{
			var prop = sysopFields[sI];
			if (fileMetadata.hasOwnProperty(prop))
			{
				if (typeof(fileMetadata[prop]) === "string" && fileMetadata[prop].length == 0)
					continue;
				var propName = prop.charAt(0).toUpperCase() + prop.substr(1);
				var infoValue = "";
				if (prop == "added")
					infoValue = strftime("%Y-%m-%d %H:%M:%S", fileMetadata.added);
				else
					infoValue = fileMetadata[prop].toString();
				printf(fieldFormatStr, propName, infoValue);
				console.attributes = "NW";
			}
		}
	}
	console.attributes = "NW";
	console.crlf();

	// Construct the file list redraw info.  Note that the X and Y are relative
	// to the file list menu, not absolute screen coordinates.
	retObj.fileListPartialRedrawInfo = {
		startX: 1,
		startY: 1,
		absStartX: 1,
		absStartY: 1,
		width: 0,
		height: 0
	};

	return retObj;
}
// Splits a string on a given string and then re-combines the string with \r\n (carriage return & newline)
// at the end of each line
//
// Parameters:
//  pStr: The string to split & recombine
//  pSplitStr: The string to split the first string on
//
// Return value: The string split on pSplitStr and re-combined with \r\n at the end of each line
function splitStrAndCombineWithRN(pStr, pSplitStr)
{
	if (typeof(pStr) !== "string")
		return "";
	if (typeof(pSplitStr) !== "string")
		return pStr;

	var newStr = "";
	var strs = pStr.split(pSplitStr);
	if (strs.length > 0)
	{
		for (var i = 0; i < strs.length; ++i)
			newStr += strs[i] + "\r\n";
		newStr = newStr.replace(/\r\n$/, "");
	}
	else
		newStr = pStr;
	return newStr;
}

// Lets the user view a file.  Note that this function works for both the ANSI/lightbar
// and traditional user interface.
//
// Parameters:
//  pFileMetadata: The file metadata object for the file to view
//
// Return value: An object with values to indicate status & screen refresh actions; see
//               getDefaultActionRetObj() for details.
function viewFile(pFileMetadata)
{
	var retObj = getDefaultActionRetObj();
	if (pFileMetadata == undefined || typeof(pFileMetadata) !== "object")
		return retObj;

	// Open the filebase & get the fully pathed filename
	var fullyPathedFilename = "";
	var filebase = new FileBase(pFileMetadata.dirCode);
	if (filebase.open())
	{
		fullyPathedFilename = filebase.get_path(pFileMetadata);
		filebase.close();
	}
	else
	{
		if (gUseLightbarInterface && console.term_supports(USER_ANSI))
			displayMsg("Failed to open the filebase!", true, true);
		else
			console.print("\x01n" + gColors.errorMessage + "Failed to open the filebase!\x01n\r\n\x01p");
		return retObj;
	}

	// View the file
	console.gotoxy(1, console.screen_rows);
	console.attributes = "N";
	console.crlf();
	var successfullyViewed = bbs.view_file(fullyPathedFilename);
	console.attributes = "N";
	if (gPauseAfterViewingFile || !successfullyViewed)
		console.pause();

	retObj.reDrawListerHeader = true;
	retObj.reDrawMainScreenContent = true;
	retObj.reDrawCmdBar = true;
	return retObj;
}

// Allows the user to add their selected file to their batch downloaded queue. Note that this can work
// with both the ANSI/lightbar interface or traditional interface.
//
// Parameters:
//  pFileMetadata: The file metadata object for the file
//  pFileList: The list of file metadata objects from the file directory
//
// Return value: An object with values to indicate status & screen refresh actions; see
//               getDefaultActionRetObj() for details.
function addSelectedFilesToBatchDLQueue(pFileMetadata, pFileList)
{
	var retObj = getDefaultActionRetObj();
	if (!userCanDownloadFromFileArea_ShowErrorIfNot(pFileMetadata.dirCode))
		return retObj;

	// Confirm with the user to add the file(s) to their batch queue.  If they don't want to,
	// then just return now.
	var filenames = [];
	var metadataObjects = [];
	if (gFileListMenu.numSelectedItemIndexes() > 0)
	{
		for (var idx in gFileListMenu.selectedItemIndexes)
		{
			var idxNum = +idx;
			filenames.push(pFileList[idxNum].name);
			metadataObjects.push(pFileList[idxNum]);
		}
	}
	else
	{
		filenames.push(pFileMetadata.name);
		metadataObjects.push(pFileMetadata);
	}
	// Note that confirmFileActionWithUser() will re-draw the parts of the file
	// list menu that are necessary.
	var addFilesConfirmed = addFilesConfirmed = confirmFileActionWithUser(filenames, "Batch DL add", false);
	retObj.refreshedSelectedFilesAlready = true;
	if (addFilesConfirmed)
	{
		var batchDLQueueStats = getUserDLQueueStats();
		var filenamesFailed = []; // To store filenames that failed to get added to the queue
		var batchDLFilename = system.data_dir + "user/" + format("%04d", user.number) + ".dnload";
		var batchDLFile = new File(batchDLFilename);
		if (batchDLFile.open(batchDLFile.exists ? "r+" : "w+"))
		{
			if (gUseLightbarInterface && console.term_supports(USER_ANSI))
				displayMsg("Adding file(s) to batch DL queue..", false, false);
			else
				console.print("\x01n\x01cAdding file(s) to batch DL queue..\x01n\r\n");
			for (var i = 0; i < metadataObjects.length; ++i)
			{
				// If the file isn't in the user's batch DL queue already, then add it.
				var fileAlreadyInQueue = false;
				for (var fIdx = 0; fIdx < batchDLQueueStats.filenames.length && !fileAlreadyInQueue; ++fIdx)
					fileAlreadyInQueue = (batchDLQueueStats.filenames[fIdx].filename == metadataObjects[i].name);
				if (!fileAlreadyInQueue)
				{
					var addToQueueSuccessful = true;
					batchDLFile.writeln("");

					// Add the required "dir" and "desc" properties to the user's batch download
					// queue file.  The section is the filename.  Also, this script should add a
					// dirCode property to each metadata object in the list.
					addToQueueSuccessful = batchDLFile.iniSetValue(metadataObjects[i].name, "dir", metadataObjects[i].dirCode);
					if (addToQueueSuccessful)
					{
						addToQueueSuccessful = batchDLFile.iniSetValue(metadataObjects[i].name, "desc", metadataObjects[i].desc);
						// Update the batch DL queue stats object
						++(batchDLQueueStats.numFilesInQueue);
						batchDLQueueStats.filenames.push({ filename: metadataObjects[i].name, desc: metadataObjects[i].desc });
						batchDLQueueStats.totalSize += +(metadataObjects[i].size);
						batchDLQueueStats.totalCost += +(metadataObjects[i].cost);
					}

					if (!addToQueueSuccessful)
						filenamesFailed.push(metadataObjects[i].name);
				}
			}

			batchDLFile.close();
		}

		// For ANSI/lightbar: Show a message box
		if (gUseLightbarInterface && console.term_supports(USER_ANSI))
		{
			// Frame location & size for batch DL queue stats or filenames that failed
			var frameUpperLeftX = gFileListMenu.pos.x + 2;
			var frameUpperLeftY = gFileListMenu.pos.y + 2;
			var frameWidth = console.screen_columns - 4; // Used to be gFileListMenu.size.width - 4;
			var frameInnerWidth = frameWidth - 2; // Without borders
			var frameHeight = 8;

			// If there were no failures, then show a success message & prompt the user if they
			// want to download their batch queue.  Otherwise, show the filenames that failed to
			// get added.
			if (filenamesFailed.length == 0)
			{
				displayMsg("Your batch DL queue was sucessfully updated", false, true);
				// Prompt if the user wants to download their batch queue
				if (bbs.batch_dnload_total > 0)
				{
					// Clear most of the screen area so the user has focus on the batch DL queue stats
					var fullLineFormatStr = "%" + console.screen_columns + "s";
					var leftFormatStr = "%" + frameUpperLeftX + "s";
					var rightFormatStr = "%" + +(frameUpperLeftX+frameWidth-1) + "s";
					var lastFrameRow = frameUpperLeftY + frameHeight - 1;
					var lastRow = console.screen_rows - 1;
					console.attributes = "N";
					for (var screenRow = gNumHeaderLinesDisplayed+1; screenRow <= lastRow; ++screenRow)
					{
						console.gotoxy(1, screenRow);
						if (screenRow < frameUpperLeftY || screenRow > lastFrameRow)
							printf(fullLineFormatStr, "");
						else
						{
							printf(leftFormatStr, "");
							console.gotoxy(frameUpperLeftX+frameWidth, screenRow);
							printf(rightFormatStr, "");
						}
					}

					// Build a frame with batch DL queue stats and prompt the user if they want to
					// download their batch DL queue
					var frameTitle = "Download your batch queue (Y/N)?";
					// \x01cFiles: \x01h1 \x01n\x01c(\x01h100 \x01n\x01cMax)  Credits: 0  Bytes: \x01h2,228,254 \x01n\x01c Time: 00:09:40
					// Note: The maximum number of allowed files in the batch download queue doesn't seem to
					// be available to JavaScript.
					var totalQueueSize = batchDLQueueStats.totalSize + pFileMetadata.size;
					var totalQueueCost = batchDLQueueStats.totalCost + pFileMetadata.cost;
					var queueStats = "\x01n\x01cFiles: \x01h" + batchDLQueueStats.numFilesInQueue + "  \x01n\x01cCredits: \x01h"
								   + totalQueueCost + "\x01n\x01c  Bytes: \x01h" + numWithCommas(totalQueueSize) + "\x01n\x01w\r\n";
					for (var i = 0; i < batchDLQueueStats.filenames.length; ++i)
					{
						queueStats += shortenFilename(batchDLQueueStats.filenames[i].filename, frameInnerWidth, false) + "\r\n";
						if (typeof(batchDLQueueStats.filenames[i].desc) === "string")
							queueStats += batchDLQueueStats.filenames[i].desc.substr(0, frameInnerWidth) + "\r\n";
						if (i < batchDLQueueStats.filenames.length-1)
							queueStats += "\r\n";
					}
					var additionalQuitKeys = "yYnN";
					var lastUserInput = displayBorderedFrameAndDoInputLoop(frameUpperLeftX, frameUpperLeftY, frameWidth,
																		   frameHeight, gColors.batchDLInfoWindowBorder,
																		   frameTitle, gColors.batchDLInfoWindowTitle,
																		   queueStats, additionalQuitKeys);
					// The main screen content (file list & extended description if applicable)
					// will need to be redrawn after this.
					retObj.reDrawMainScreenContent = true;
					// If the user chose to download their file queue, then send it to the user.
					// And the lister headers will need to be re-drawn as well.
					if (lastUserInput.toUpperCase() == "Y")
					{
						retObj.reDrawListerHeader = true;
						retObj.reDrawCmdBar = true;
						console.attributes = "N";
						console.gotoxy(1, console.screen_rows);
						console.crlf();
						bbs.batch_download();
						// If the user is still online (chose not to hang up after transfer),
						// then pause so that the user can see the batch download status
						if (bbs.online > 0)
							console.pause();
					}
				}
			}
			else
			{
				eraseMsgBoxScreenArea();
				// Build a frame object to show the names of the files that failed to be added to the
				// user's batch DL queue
				var frameTitle = "Failed to add these files to batch DL queue";
				var fileListStr = "\x01n\x01w";
				for (var i = 0; i < filenamesFailed.length; ++i)
					fileListStr += shortenFilename(filenamesFailed[i], frameInnerWidth, false) + "\r\n";
				var lastUserInput = displayBorderedFrameAndDoInputLoop(frameUpperLeftX, frameUpperLeftY, frameWidth,
																	   frameHeight, gColors.batchDLInfoWindowBorder,
																	   frameTitle, gColors.batchDLInfoWindowTitle,
																	   fileListStr, "");
				// Add the file list redraw info.  Note that the X and Y are relative
				// to the file list menu, not absolute screen coordinates.
				// To make the list refresh info to return to the main script loop
				retObj.fileListPartialRedrawInfo = {
					startX: 3,
					startY: 3,
					absStartX: gFileListMenu.pos.x + 3 - 1, // 1-based
					absStartY: gFileListMenu.pos.y + 3 - 1, // 1-based
					width: frameWidth + 1,
					height: frameHeight
				};
			}
		}
		else
		{
			// Traditional UI
			retObj.fileListPartialRedrawInfo = null;
			// If there were no failures, then show a success message & prompt the user if they
			// want to download their batch queue.  Otherwise, show the filenames that failed to
			// get added.
			if (filenamesFailed.length == 0)
			{
				console.attributes = "N";
				console.print("Your batch DL queue was sucessfully updated");
				console.crlf();
				// Prompt if the user wants to download their batch queue
				if (bbs.batch_dnload_total > 0)
				{
					// Output the user's file queue
					// \x01cFiles: \x01h1 \x01n\x01c(\x01h100 \x01n\x01cMax)  Credits: 0  Bytes: \x01h2,228,254 \x01n\x01c Time: 00:09:40
					// Note: The maximum number of allowed files in the batch download queue doesn't seem to
					// be available to JavaScript.
					var totalQueueSize = batchDLQueueStats.totalSize + pFileMetadata.size;
					var totalQueueCost = batchDLQueueStats.totalCost + pFileMetadata.cost;
					console.print("\x01cFiles: \x01h" + batchDLQueueStats.numFilesInQueue + "  \x01n\x01cCredits: \x01h"
					               + totalQueueCost + "\x01n\x01c  Bytes: \x01h" + numWithCommas(totalQueueSize) + "\x01n");
					console.crlf();
					for (var i = 0; i < batchDLQueueStats.filenames.length; ++i)
					{
						console.print(batchDLQueueStats.filenames[i].filename + "\r\n");
						if (typeof(batchDLQueueStats.filenames[i].desc) === "string")
							console.print(batchDLQueueStats.filenames[i].desc + "\r\n");
						if (i < batchDLQueueStats.filenames.length-1)
							console.crlf();
					}
					// Prompt the user whether they want to download their file queue; send it to the user
					// if they choose to do so.
					if (console.yesno("Download your batch queue (Y/N)"))
					{
						retObj.reDrawListerHeader = true;
						retObj.reDrawCmdBar = true;
						console.attributes = "N";
						bbs.batch_download();
						// If the user is still online (chose not to hang up after transfer),
						// then pause so that the user can see the batch download status
						if (bbs.online > 0)
							console.pause();
					}
				}
			}
			else
			{
				// Show the names of the files that failed to be added to the user's batch DL queue
				console.attributes = "N";
				console.print(gColors.errorMessage + "Failed to add these files to batch DL queue:\x01n\r\n");
				console.attributes = "NW";
				for (var i = 0; i < filenamesFailed.length; ++i)
					console.print(filenamesFailed[i] + "\r\n");
				console.pause();
				console.line_counter = 0;
			}
		}
	}

	return retObj;
}
// Gets stats about the user's batch download queue.
//
// Return value: An object containing the following properties:
//               numFilesInQueue: The number of files already in the queue
//               totalSize: The total size of the files in the queue
//               totalCost: The total cost of the files in the queue
//               filenames: An array of objects, each containing the filename and
//                          descriptions (desc) of the files in the download queue
function getUserDLQueueStats()
{
	var retObj = {
		numFilesInQueue: 0,
		totalSize: 0,
		totalCost: 0,
		filenames: []
	};

	var batchDLFilename = system.data_dir + "user/" + format("%04d", user.number) + ".dnload";
	var batchDLFile = new File(batchDLFilename);
	if (batchDLFile.open(batchDLFile.exists ? "r+" : "w+"))
	{
		// See if a section exists for the filename
		//File.iniGetAllObjects([name_property] [,prefix=none] [,lowercase=false] [,blanks=false])
		var allIniObjs = batchDLFile.iniGetAllObjects();
		console.attributes = "N";
		console.crlf();
		for (var i = 0; i < allIniObjs.length; ++i)
		{
			if (typeof(allIniObjs[i]) === "object")
			{
				++(retObj.numFilesInQueue);
				//allIniObjs[i].name
				//allIniObjs[i].dir
				//allIniObjs[i].desc
				retObj.filenames.push({
					filename: allIniObjs[i].name,
					desc: typeof(allIniObjs[i].desc) === "string" ? allIniObjs[i].desc : ""
				});
				// dir is the internal directory code
				if (allIniObjs[i].dir.length > 0)
				{
					var filebase = new FileBase(allIniObjs[i].dir);
					if (filebase.open())
					{
						var fileInfo = filebase.get(allIniObjs[i].name);
						if (typeof(fileInfo) === "object")
						{
							retObj.totalSize += +(fileInfo.size);
							retObj.totalCost += +(fileInfo.cost);
						}
						filebase.close();
					}
				}
			}
		}
		batchDLFile.close();
	}

	return retObj;
}

// Lets the user download the currently selected file on the file list. This works with both the
// ANSi/lightbar UI and traditional UI.
//
// Parameters:
//  pFileMetadata: The file metadata object for the file to download
//
// Return value: An object with values to indicate status & screen refresh actions; see
//               getDefaultActionRetObj() for details.
function letUserDownloadSelectedFile(pFileMetadata)
{
	var retObj = getDefaultActionRetObj();
	console.attributes = "N";
	console.crlf();
	console.crlf();
	// If the user has the security level to download the file, let them do so
	if (userCanDownloadFromFileArea_ShowErrorIfNot(pFileMetadata.dirCode))
	{
		console.print("\x01cDownloading \x01h" + pFileMetadata.name + "\x01n");
		console.crlf();
		var selectedFilanmeFullPath = file_area.dir[pFileMetadata.dirCode].path + pFileMetadata.name;
		bbs.send_file(selectedFilanmeFullPath);
	}

	retObj.reDrawListerHeader = true;
	retObj.reDrawHeaderTextOnly = false;
	retObj.reDrawMainScreenContent = true;
	retObj.reDrawCmdBar = true;

	return retObj;
}

// Displays the help screen.
function displayHelpScreen()
{
	var retObj = getDefaultActionRetObj();

	console.clear("\x01n");
	// Display program information
	displayTextWithLineBelow("Digital Distortion File Lister", true, "\x01n\x01c\x01h", "\x01k\x01h")
	console.center("\x01n\x01cVersion \x01g" + LISTER_VERSION + " \x01w\x01h(\x01b" + LISTER_DATE + "\x01w)");
	console.crlf();

	// If listing files in a directory, display information about the current file directory.
	if (gScriptMode == MODE_LIST_DIR)
	{
		var libIdx = file_area.dir[gDirCode].lib_index;
		var dirIdx = file_area.dir[gDirCode].index;
		console.print("\x01n\x01cCurrent file library: \x01g" + file_area.lib_list[libIdx].description);
		console.crlf();
		console.print("\x01cCurrent file directory: \x01g" + file_area.dir[gDirCode].description);
		console.crlf();
		console.print("\x01cThere are \x01g" + file_area.dir[gDirCode].files + " \x01cfiles in this directory.");
	}
	else if (gScriptMode == MODE_SEARCH_FILENAME)
		console.print("\x01n\x01cCurrently performing a filename search");
	else if (gScriptMode == MODE_SEARCH_DESCRIPTION)
		console.print("\x01n\x01cCurrently performing a description search");
	else if (gScriptMode == MODE_NEW_FILE_SEARCH)
		console.print("\x01n\x01cCurrently performing a new file search");
	console.crlf();
	console.crlf();

	// Display information about the lister
	var helpStr = "This lists files in your current file directory with a lightbar interface (for an ANSI terminal).  ";
	helpStr += "The file list can be navigated using the up & down arrow keys, PageUp, PageDown, Home, and End keys.  "
	helpStr += "The currently highlighted file in the menu is used by default for the various actions.  For batch download "
	helpStr += "selection, ";
	if (user.is_sysop)
		helpStr += "moving, and deleting, ";
	helpStr += "you can select multiple files by using the spacebar.  ";
	helpStr += "There is also a command bar accross the bottom of the screen - You can select an action on the ";
	helpStr += "action bar by using the left & right arrow keys and pressing enter to choose an action.  Alternately, ";
	helpStr += "you can press the first character of the action word to perform the action.";
	helpStr += "  Also, the following actions are available:";
	// Wrap the help string to the user's terminal width, and replace all instances of
	// newlines with carriage return + newline, then display the help text.
	helpStr = word_wrap(helpStr, console.screen_columns - 1).replace(/\n/g, "\r\n");
	console.print(helpStr);
	// Display the commands available
	var commandStrWidth = 8;
	var printfStr = "\x01n\x01c\x01h%-" + commandStrWidth + "s\x01g: \x01n\x01c%s\r\n";
	printf(printfStr, "I", "Display extended file information");
	printf(printfStr, "V", "View the file");
	printf(printfStr, "B", "Flag the selected file(s) for batch download");
	printf(printfStr, "D", "Download the highlighted (selected) file");
	printf(printfStr, "E", "Edit the file information");
	if (!gUseLightbarInterface)
	{
		printf(printfStr, "N/PageDn", "Show the next page of files");
		printf(printfStr, "P/Pageup", "Show the previous page of files");
		printf(printfStr, "F/Home", "Show the first page of files");
		printf(printfStr, "L/End", "Show the last page of files");
	}
	if (user.is_sysop)
	{
		printf(printfStr, "M", "Move the file(s) to another directory");
		printf(printfStr, "DEL", "Delete the file(s)");
	}
	printf(printfStr, "?", "Show this help screen");
	// Ctrl-C for aborting (wanted to use isDoingFileSearch() but it seems even for searching/scanning,
	// the mode is LIST_DIR rather than a search/scan
	printf(printfStr, "Ctrl-C", "Abort (can quit out of searching, etc.)");
	printf(printfStr, "Q", "Quit back to the BBS");
	console.attributes = "N";
	console.crlf();
	console.pause();

	retObj.reDrawListerHeader = true;
	retObj.reDrawMainScreenContent = true;
	retObj.reDrawCmdBar = true;
	return retObj;
}

// Allows the user to edit a file's metadata.  Only for sysops!
//
// Parameters:
//  pFileList: The list of file metadata objects from the file directory
//  pFileListMenu: The menu object for the file diretory
//
// Return value: An object with values to indicate status & screen refresh actions; see
//               getDefaultActionRetObj() for details.
function editFileInfo(pFileList, pFileListMenu)
{
	var retObj = getDefaultActionRetObj();

	var fileMetadata = pFileList[pFileListMenu.selectedItemIdx];

	// If the user isn't the sysop or didn't upload this file, then don't let
	// them edit it
	if (!(user.is_sysop || userNameOrAliasMatchCaseIns(fileMetadata.from)))
	{
		if (gUseLightbarInterface && console.term_supports(USER_ANSI))
		{
			var errorMsg = "You don't have permission to edit this file";
			displayMsg(errorMsg, true, true);
		}
		else
		{
			var errorMsg = format("\x01n\r\n\x01y\x01hYou don't have permission to edit \x01c%s \x01y(in \x01g%s\x01y)\x01n", fileMetadata.name, getLibAndDirDesc(fileMetadata.dirCode));
			errorMsg = lfexpand(word_wrap(errorMsg, console.screen_columns-1));
			console.print(errorMsg + "\r\n\x01p");
			retObj.reDrawMainScreenContent = true;
			retObj.reDrawListerHeader = true;
			retObj.reDrawCmdBar = true;
			retObj.refreshedSelectedFilesAlready = true;
		}
		return retObj;
	}

	// Set return values. After this function, the screen will need to be re-drawn.
	retObj.reDrawMainScreenContent = true;
	retObj.reDrawListerHeader = true;
	retObj.reDrawCmdBar = true;
	retObj.refreshedSelectedFilesAlready = true;

	console.print("\x01n\r\n\r\n");
	var msg = format("\x01cEditing file\x01g\x01h: \x01c%s\x01n \x01c(in \x01h%s\x01n\x01c)\x01n", fileMetadata.name, getLibAndDirDesc(fileMetadata.dirCode));
	console.print(lfexpand(word_wrap(msg), console.screen_columns-1));
	var newMetadata = {};
	for (var prop in fileMetadata)
		newMetadata[prop] = fileMetadata[prop];
	// Description
	var promptText = bbs.text(bbs.text.EditDescription);
	var editWidth = console.screen_columns - console.strlen(promptText) - 1;
	console.mnemonics(promptText);
	newMetadata.desc = console.getstr(fileMetadata.desc, editWidth, K_EDIT|K_LINE|K_NOSPIN); // K_NOCRLF
	if (console.aborted)
		return retObj;
	// Tags
	promptText = bbs.text(bbs.text.TagFilePrompt);
	editWidth = console.screen_columns - console.strlen(promptText) - 1;
	console.mnemonics(promptText);
	var tags = console.getstr(newMetadata.hasOwnProperty("tags") ? newMetadata.tags : "", editWidth, K_EDIT|K_LINE|K_NOSPIN); // K_NOCRLF
	if (console.aborted)
		return retObj;
	if (tags.length > 0)
		newMetadata.tags = tags;
	// Extended description
	if (!console.noyes(bbs.text(bbs.text.EditExtDescriptionQ)))
	{
		// Get the extended metadata object and check to see if it has an existing extended definition
		var extdMetadata = getFileInfoFromFilebase(fileMetadata.dirCode, fileMetadata.name, FileBase.DETAIL.EXTENDED);
		// Let the user edit the extended description with their configured editor
		var descFilename = system.node_dir + "extdDescTemp.txt";
		var outFile = new File(descFilename);
		if (outFile.open("w"))
		{
			// An extended file description is usually up to about 45 characters long
			var descWrapped = word_wrap(extdMetadata.extdesc, 45, null, false).split("\r\n");
			for (var lineIdx = 0; lineIdx < descWrapped.length; ++lineIdx)
				outFile.writeln(descWrapped[lineIdx]);
			outFile.close();
			if (console.editfile(descFilename, "", "", fileMetadata.name, "", false))
			{
				if (outFile.open("r"))
				{
					newMetadata.extdesc = outFile.readAll(console.screen_columns).join("\r\n");
					outFile.close();
				}
			}
			if (!file_remove(descFilename))
				log(LOG_ERR, "Failed to delete temporary file " + descFilename);
		}
	}
	// Only let the sysop edit the uploader, credit value, upload time, etc.
	if (user.is_sysop)
	{
		// Uploader
		promptText = bbs.text(bbs.text.EditUploader);
		editWidth = console.screen_columns - console.strlen(promptText) - 1;
		console.mnemonics(promptText);
		newMetadata.from = console.getstr(fileMetadata.from, editWidth, K_EDIT|K_LINE|K_NOSPIN);
		if (console.aborted)
			return retObj;
		// Credit value
		promptText = bbs.text(bbs.text.EditCreditValue);
		editWidth = console.screen_columns - console.strlen(promptText) - 1;
		console.mnemonics(promptText);
		newMetadata.cost = console.getstr(fileMetadata.cost.toString(), editWidth, K_EDIT|K_LINE|K_NUMBER|K_NOSPIN);
		if (console.aborted)
			return retObj;
		// # times downloaded
		promptText = bbs.text(bbs.text.EditTimesDownloaded);
		editWidth = console.screen_columns - console.strlen(promptText) - 1;
		console.mnemonics(promptText);
		newMetadata.times_downloaded = console.getstr(fileMetadata.times_downloaded.toString(), editWidth, K_EDIT|K_LINE|K_NUMBER|K_NOSPIN);
		if (console.aborted)
			return retObj;
		// Current new-scan date/time
		console.mnemonics(bbs.text(bbs.text.NScanDate));
		//console.print(bbs.text(bbs.text.NScanDate));
		console.mnemonics(system.timestr(fileMetadata.time));
		var yearStr = strftime("%Y", fileMetadata.time);
		var monthNumStr = strftime("%m", fileMetadata.time);
		var dayNumStr = strftime("%d", fileMetadata.time);
		var hourStr = strftime("%H", fileMetadata.time);
		var minuteStr = strftime("%M", fileMetadata.time);
		console.crlf();
		console.mnemonics(bbs.text(bbs.text.NScanYear));
		var scanYearConsoleAttrs = console.attributes; // To restore for subsequent prompts since console.getstr() seems to apply normal attribute
		yearStr = console.getstr(yearStr, yearStr.length+1, K_EDIT|K_LINE|K_NUMBER|K_NOSPIN|K_NOCRLF);
		if (console.aborted)
			return retObj;
		console.attributes = scanYearConsoleAttrs; // Restore year prompt console attributes
		console.mnemonics(bbs.text(bbs.text.NScanMonth));
		monthNumStr = console.getstr(monthNumStr, 2, K_EDIT|K_LINE|K_NUMBER|K_NOSPIN|K_NOCRLF);
		if (console.aborted)
			return retObj;
		console.attributes = scanYearConsoleAttrs; // Restore year prompt console attributes
		console.mnemonics(bbs.text(bbs.text.NScanDay));
		dayNumStr = console.getstr(dayNumStr, 2, K_EDIT|K_LINE|K_NUMBER|K_NOSPIN|K_NOCRLF);
		if (console.aborted)
			return retObj;
		console.attributes = scanYearConsoleAttrs; // Restore year prompt console attributes
		console.mnemonics(bbs.text(bbs.text.NScanHour));
		hourStr = console.getstr(hourStr, 2, K_EDIT|K_LINE|K_NUMBER|K_NOSPIN|K_NOCRLF);
		if (console.aborted)
			return retObj;
		var hourNum = parseInt(hourStr, 10);
		console.attributes = scanYearConsoleAttrs; // Restore year prompt console attributes
		console.mnemonics(bbs.text(bbs.text.NScanMinute));
		minuteStr = console.getstr(minuteStr, 2, K_EDIT|K_LINE|K_NUMBER|K_NOSPIN|K_NOCRLF);
		if (console.aborted)
			return retObj;
		if (!isNaN(hourNum) && hourNum <= 12)
		{
			console.attributes = scanYearConsoleAttrs; // Restore year prompt console attributes
			if (console.yesno(bbs.text(bbs.text.NScanPmQ)))
				hourNum += 12;
			
		}
		// Do some validation before setting the newscan time
		var yearNum = parseInt(yearStr, 10);
		var monthNum = parseInt(monthNumStr, 10);
		var dayNum = parseInt(dayNumStr, 10);
		var minuteNum = parseInt(minuteStr, 10);
		var allAreNums = !isNaN(yearNum) && !isNaN(monthNum) && !isNaN(dayNum) && !isNaN(hourNum) && !isNaN(minuteNum);
		if (allAreNums && monthNum >= 1 && monthNum <= 12 && dayNum >= 1 && dayNum <= 31 && hourNum <= 23 && minuteNum <= 59)
			newMetadata.time = getTimeTFFromDate(yearNum, monthNum, dayNum, hourNum, minuteNum);
		else
			console.print("\x01nDate/time invalid; not updating.\r\n");
	}

	// Update the file information in the filebase
	var filebase = new FileBase(fileMetadata.dirCode);
	if (filebase.open())
	{
		var succeeded = filebase.update(fileMetadata.name, newMetadata);
		filebase.close();
		if (succeeded)
		{
			pFileList[pFileListMenu.selectedItemIdx] = newMetadata;
			log(LOG_INFO, format("Successfully updated information update for %s in %s", fileMetadata.name, getLibAndDirDesc(fileMetadata.dirCode)));
		}
		else
		{
			log(LOG_ERR, format("Failed to save information update for %s in %s", fileMetadata.name, getLibAndDirDesc(fileMetadata.dirCode)));
			console.print("\r\nFailed to save the changes!\r\n\x01p");
		}
	}

	
	return retObj;
}

// Allows the user to move the selected file to another filebase.  Only for sysops!
//
// Parameters:
//  pFileList: The list of file metadata objects from the file directory
//  pFileListMenu: The menu object for the file diretory
//
// Return value: An object with values to indicate status & screen refresh actions; see
//               getDefaultActionRetObj() for details.
function chooseFilebaseAndMoveFileToOtherFilebase(pFileList, pFileListMenu)
{
	var retObj = getDefaultActionRetObj();

	var fileMetadata = pFileList[pFileListMenu.selectedItemIdx];

	// Confirm with the user to move the file(s).  If they don't want to,
	// then just return now.
	var filenames = [];
	if (pFileListMenu.numSelectedItemIndexes() > 0)
	{
		for (var idx in pFileListMenu.selectedItemIndexes)
			filenames.push(pFileList[+idx].name);
	}
	else
		filenames.push(fileMetadata.name);
	// Note that confirmFileActionWithUser() will re-draw the parts of the file
	// list menu that are necessary.
	var moveFilesConfirmed = confirmFileActionWithUser(filenames, "Move", false);
	retObj.refreshedSelectedFilesAlready = true;
	if (!moveFilesConfirmed)
		return retObj;

	// Create a file library menu for the user to choose a file library (and then directory)
	var fileLibMenu = createFileLibMenu();
	// For screen refresh purposes, construct the file list redraw info.  Note that the X and Y are relative
	// to the file list menu, not absolute screen coordinates.
	var topYForRefresh = fileLibMenu.pos.y - 1; // - 1 because of the label above the menu
	var fileListPartialRedrawInfo = {
		startX: fileLibMenu.pos.x - pFileListMenu.pos.x + 1,
		startY: topYForRefresh - pFileListMenu.pos.y + 1,
		absStartX: fileLibMenu.pos.x,
		absStartY: topYForRefresh,
		width: fileLibMenu.size.width + 1,
		height: fileLibMenu.size.height + 1 // + 1 because of the label above the menu
	};
	var chooseAreaText = "Choose a destination area";
	if (gUseLightbarInterface && console.term_supports(USER_ANSI))
	{
		console.gotoxy(fileLibMenu.pos.x, fileLibMenu.pos.y-1);
		printf("\x01n\x01c\x01h|\x01n\x01c%-" + +(fileLibMenu.size.width-1) + "s\x01n", chooseAreaText);
	}
	else
		printf("\x01n\x01c%-" + +(fileLibMenu.size.width-1) + "s\x01n\r\n", chooseAreaText);
	// Prompt the user which directory to move the file to
	var chosenDirCode = null;
	var continueOn = true;
	while (continueOn)
	{
		var chosenLibIdx = fileLibMenu.GetVal();
		if (typeof(chosenLibIdx) === "number")
		{
			// The file dir menu will be created at the same position & with the same size
			// as the file library menu
			var fileDirMenu = createFileDirMenu(chosenLibIdx);
			if (!gUseLightbarInterface)
				printf("\x01n%sDirectories of %s:\r\n", gColors.fileAreaDescTrad, file_area.lib_list[chosenLibIdx].description);
			chosenDirCode = fileDirMenu.GetVal();
			if (typeof(chosenDirCode) === "string")
			{
				if (chosenDirCode != fileMetadata.dirCode)
					continueOn = false;
				else
				{
					chosenDirCode = "";
					displayMsg("Can't move to the same directory", true);
				}
			}
		}
		else
			continueOn = false;
	}

	// If the user chose a directory, then move the file there.
	if (typeof(chosenDirCode) === "string" && chosenDirCode.length > 0)
	{
		// For logging
		var destLibAndDirDesc = getLibAndDirDesc(chosenDirCode);

		// Build an array of file indexes and sort the array
		var fileIndexes = [];
		if (pFileListMenu.numSelectedItemIndexes() > 0)
		{
			for (var idx in pFileListMenu.selectedItemIndexes)
				fileIndexes.push(+idx);
		}
		else
			fileIndexes.push(+(pFileListMenu.selectedItemIdx));
		// Ensure the file indexes are sorted in numerical order
		fileIndexes.sort(function(a, b) { return a - b });

		// Go through the list of files and move each of them
		var moveAllSucceeded = true;
		for (var i = 0; i < fileIndexes.length; ++i)
		{
			var fileIdx = fileIndexes[i];
			// For logging
			var srcLibAndDirDesc = getLibAndDirDesc(pFileList[fileIdx].dirCode);

			var moveRetObj = moveFileToOtherFilebase(pFileList[fileIdx], chosenDirCode);
			var logMsg = "";
			var logLevel = LOG_INFO;
			if (moveRetObj.moveSucceeded)
			{
				logMsg = "Digital Distotion File Lister: Successfully moved " + pFileList[fileIdx].name
				       + " from " + srcLibAndDirDesc + " to " + destLibAndDirDesc;

				// If we're listing files in the user's current directory, then remove
				// the file info object from the file list array.  Otherwise, update
				// the metadata object in the list.
				if (gScriptMode == MODE_LIST_DIR)
				{
					pFileList.splice(fileIdx, 1);
					// Subtract 1 from the remaining indexes in the fileIndexes array
					for (var j = i+1; j < fileIndexes.length; ++j)
						fileIndexes[j] = fileIndexes[j] - 1;
					// Have the file list menu set up its description width, colors, and format
					// string again in case it no longer needs to use its scrollbar
					pFileListMenu.SetItemWidthsColorsAndFormatStr();
					retObj.reDrawMainScreenContent = true;
				}
				else
				{
					// Note: getFileInfoFromFilebase() will add dirCode to the metadata object
					var fileDetail = (extendedDescEnabled() ? FileBase.DETAIL.EXTENDED : FileBase.DETAIL.NORM);
					pFileList[fileIdx] = getFileInfoFromFilebase(chosenDirCode, pFileList[fileIdx].name, fileDetail);
				}
			}
			else
			{
				moveAllSucceeded = false;
				logLevel = LOG_ERR;
				logMsg = "Digital Distotion File Lister: Failed to move " + pFileList[fileIdx].name
				       + " from " + srcLibAndDirDesc + " to " + destLibAndDirDesc;
			}
			log(logLevel, logMsg);
			bbs.log_str(logMsg);
		}
		// Adjust the selected item index in the file list menu if necesary
		if (pFileListMenu.NumItems() == 0)
			pFileListMenu.selectedItemIdx = 0;
		else if (pFileListMenu.selectedItemIdx >= pFileListMenu.NumItems() - 1)
			pFileListMenu.selectedItemIdx = pFileListMenu.NumItems() - 1;
		// If doing a search (not listing files in the user's current directory), then
		// if all files were in the same directory, then we'll need to update the header
		// lines at the top of the file list.  If there's only one file in the list,
		// the header lines will need to display the correct directory.  Otherwise,
		// set allSameDir to false so the header lines will now say "various".
		// However, if not all files were in the same directory, check to see if they
		// are now, and if so, we'll need to re-draw the header lines.
		if (gScriptMode != MODE_LIST_DIR && typeof(pFileList.allSameDir) == "boolean")
		{
			if (pFileList.allSameDir)
			{
				if (pFileList.length > 1)
					pFileList.allSameDir = false;
				retObj.reDrawHeaderTextOnly = true;
			}
			else
			{
				pFileList.allSameDir = true; // Until we find it's not true
				for (var fileListIdx = 1; fileListIdx < pFileList.length && pFileList.allSameDir; ++fileListIdx)
					pFileList.allSameDir = (pFileList[fileListIdx].dirCode == pFileList[0].dirCode);
				retObj.reDrawHeaderTextOnly =  pFileList.allSameDir;
			}
		}
		// Display a success/fail message
		//displayMsgs(pMsgArray, pIsError, pWaitAndErase)
		if (moveAllSucceeded)
			displayMsg("Successfully moved the file(s) to " + destLibAndDirDesc, false, true);
		else
			displayMsg("Failed to move the file(s)!", true, true);
		// After moving the files, if there are no more files (in the directory or otherwise),
		// say so and exit now.
		if (gScriptMode == MODE_LIST_DIR && file_area.dir[gDirCode].files == 0)
		{
			displayMsg("There are no more files in the directory.", false);
			retObj.exitNow = true;
		}
		else if (pFileList.length == 0)
		{
			displayMsg("There are no more files.", false);
			retObj.exitNow = true;
		}
	}

	// If not exiting now, we'll want to re-draw part of the file list to erase the
	// area chooser menu.
	if (!retObj.exitNow)
		retObj.fileListPartialRedrawInfo = fileListPartialRedrawInfo;

	return retObj;
}

// Allows the user to remove the selected file(s) from the filebase.  Only for sysops!
// This works with both the ANSI/Lightbar UI and the traditional UI.
//
// Parameters:
//  pFileList: The list of file metadata objects from the file directory
//  pFileListMenu: The menu object for the file diretory
//
// Return value: An object with values to indicate status & screen refresh actions; see
//               getDefaultActionRetObj() for details.  For this function, the object
//               returned will have the following additional properties:
//               filesDeleted: Boolean - Whether or not files were actually deleted (after
//                             confirmation)
function confirmAndRemoveFilesFromFilebase(pFileList, pFileListMenu)
{
	var retObj = getDefaultActionRetObj();
	retObj.filesDeleted = false;

	// Confirm the action with the user.  If the user confirms, then remove the file(s).
	// If there are multiple selected files, then prompt to remove each of them.
	// Otherwise, prompt for the one selected file.
	var filenames = [];
	if (pFileListMenu.numSelectedItemIndexes() > 0)
	{
		for (var idx in pFileListMenu.selectedItemIndexes)
			filenames.push(pFileList[+idx].name);
	}
	else
		filenames.push(pFileList[pFileListMenu.selectedItemIdx].name);
	// Note that confirmFileActionWithUser() will re-draw the parts of the file list menu
	// that are necessary.
	var removeFilesConfirmed = confirmFileActionWithUser(filenames, "Remove", false);
	retObj.refreshedSelectedFilesAlready = true;
	if (removeFilesConfirmed)
	{
		retObj.filesDeleted = true; // Assume true even if some deletions may fail

		var fileIndexes = [];
		if (pFileListMenu.numSelectedItemIndexes() > 0)
		{
			for (var idx in pFileListMenu.selectedItemIndexes)
				fileIndexes.push(+idx);
		}
		else
			fileIndexes.push(+(pFileListMenu.selectedItemIdx));
		// Ensure the file indexes are sorted in numerical order
		fileIndexes.sort(function(a, b) { return a - b});

		// Go through all the selected files and remove them.
		// Note: Going through the list of indexes in reverse order so that
		// removing each one from pFileList (gFileList) is simpler.
		var removeAllSucceeded = true;
		//for (var i = 0; i < fileIndexes.length; ++i)
		for (var i = fileIndexes.length-1; i >= 0; --i)
		{
			var fileIdx = fileIndexes[i];
			if (typeof(pFileList[fileIdx]) === "undefined")
			{
				removeAllSucceeded = false;
				continue;
			}
			// For logging
			var libIdx = file_area.dir[pFileList[fileIdx].dirCode].lib_index;
			var dirIdx = file_area.dir[pFileList[fileIdx].dirCode].index;
			var libDesc = file_area.lib_list[libIdx].description;
			var dirDesc =  file_area.dir[pFileList[fileIdx].dirCode].description;
			var libAndDirDesc = libDesc + " - " + dirDesc;

			// Open the filebase and remove the file
			var removeFileSucceeded = false;
			var numFilesRemaining = 0;
			var filebase = new FileBase(pFileList[fileIdx].dirCode);
			if (filebase.open())
			{
				var filenameFullPath = filebase.get_path(pFileList[fileIdx].name); // For logging
				try
				{
					removeFileSucceeded = filebase.remove(pFileList[fileIdx].name, true);
				}
				catch (error)
				{
					removeFileSucceeded = false;
					// Make an entry in the BBS log that deleting the file failed
					var logMsg = "ddfilelister: " + error;
					log(LOG_ERR, logMsg);
					bbs.log_str(logMsg);
				}
				// If the remove failed with deleting the file, then try without deleting the file
				if (!removeFileSucceeded)
				{
					removeFileSucceeded = filebase.remove(pFileList[fileIdx].name, false);
					if (removeFileSucceeded)
					{
						var logMsg = "ddfilelister: Removed " + filenameFullPath + " from the "
						           + "filebase but couldn't actually delete the file";
						log(LOG_INFO, logMsg);
						bbs.log_str(logMsg);
					}
				}
				if (removeFileSucceeded)
				{
					if (gScriptMode == MODE_LIST_DIR)
						numFilesRemaining = filebase.files;
				}
				filebase.close();
			}
			else
				removeAllSucceeded = false;

			// Log a success/error message
			var logMsg = "";
			var logLevel = LOG_INFO;
			if (removeFileSucceeded)
			{
				logMsg = "Digital Distortion File Lister: Successfully removed " + pFileList[fileIdx].name
				       + " from " + libAndDirDesc + " (by " + user.alias + ")";
				// Remove the file info object from the file list array
				pFileList.splice(fileIdx, 1);
				// If we were going through the list in forward order, we'd have to
				// subtract 1 from the remaining indexes:
				/*
				// Subtract 1 from the remaining indexes in the fileIndexes array
				for (var j = i+1; j < fileIndexes.length; ++j)
					fileIndexes[j] = fileIndexes[j] - 1;
				*/
			}
			else
			{
				removeAllSucceeded = false;
				logMsg = "Digital Distortion File Lister: Failed to remove " + pFileList[fileIdx].name
				       + " from " + libAndDirDesc + " (by " + user.alias + ")";
				logLevel = LOG_ERR;
			}
			log(logLevel, logMsg);
			bbs.log_str(logMsg);
		}
		// Display a success/failure message
		if (removeAllSucceeded)
			displayMsg("Successfully removed the file(s)", false, true);
		else
			displayMsg("Failed to remove 1 or more files", true, true);
		// Adjust the selected item index in the file list menu if necesary
		if (pFileListMenu.NumItems() == 0)
			pFileListMenu.selectedItemIdx = 0;
		else if (pFileListMenu.selectedItemIdx >= pFileListMenu.NumItems() - 1)
			pFileListMenu.selectedItemIdx = pFileListMenu.NumItems() - 1;
		// If the file list still has files in it, have the menu redraw
		// itself to refresh with the missing entry.  Otherwise (no files left),
		// say so and have the lister exit now.
		numFilesRemaining = pFileList.length;
		if (numFilesRemaining > 0)
		{
			// Have the file list menu set up its description width, colors, and format
			// string again in case it no longer needs to use its scrollbar
			pFileListMenu.SetItemWidthsColorsAndFormatStr();
			retObj.reDrawMainScreenContent = true;
			// If all files were not in the same directory, then check to see if all
			// remaining files are now.  If so, we'll need to update the header lines
			// at the top of the file list.
			if (typeof(pFileList.allSameDir) == "boolean")
			{
				if (!pFileList.allSameDir)
				{
					pFileList.allSameDir = true; // Until we find it's not true
					for (var i = 1; i < pFileList.length && pFileList.allSameDir; ++i)
						pFileList.allSameDir = (pFileList[i].dirCode == pFileList[0].dirCode);
					//retObj.reDrawListerHeader =  pFileList.allSameDir;
					retObj.reDrawHeaderTextOnly =  pFileList.allSameDir;
				}
			}
			// Also, if the file list menu can now show all its items on one
			// page (not needing the scrollbar), set its top item index to 0.
			if (pFileListMenu.CanShowAllItemsInWindow())
				pFileListMenu.topItemIdx = 0;
		}
		else
		{
			if (gScriptMode == MODE_LIST_DIR)
				displayMsg("The directory now has no files.", false, true);
			else
				displayMsg("There are no more files to show.", false, true);
			retObj.exitNow = true;
		}
	}

	return retObj;
}

///////////////////////////////////////////////////////////////////////////////
// DDFileMenuBar stuff

function DDFileMenuBar(pPos)
{
	// Member functions
	this.constructPromptText = DDFileMenuBar_constructPromptText;
	this.getItemTextFromIdx = DDFileMenuBar_getItemTextFromIdx;
	this.writePromptLine = DDFileMenuBar_writePromptLine;
	this.refreshWithNewAction = DDFileMenuBar_refreshWithNewAction;
	this.getDDFileMenuBarItemText = DDFileMenuBar_getDDFileMenuBarItemText;
	this.incrementMenuItemAndRefresh = DDFileMenuBar_incrementMenuItemAndRefresh;
	this.decrementMenuItemAndRefresh = DDFileMenuBar_decrementMenuItemAndRefresh;
	this.getCurrentSelectedAction = DDFileMenuBar_getCurrentSelectedAction;
	this.getActionFromChar = DDFileMenuBar_getActionFromChar;
	this.setCurrentActionCode = DDFileMenuBar_setCurrentActionCode;
	this.getAllActionKeysStr = DDFileMenuBar_getAllActionKeysStr;

	// Member variables
	this.pos = {
		x: 1,
		y: 1
	};
	if (typeof(pPos) === "object" && pPos.hasOwnProperty("x") && pPos.hasOwnProperty("y") && typeof(pPos.x) === "number" && typeof(pPos.y) === "number")
	{
		if (pPos.x >= 1 && pPos.x <= console.screen_columns)
			this.pos.x = pPos.x;
		if (pPos.y >= 1 && pPos.y <= console.screen_rows)
			this.pos.y = pPos.y;
	}

	this.currentCommandIdx = 0; // The index of the current command for the menu array
	this.lastCommandIdx = 0;    // To keep track of the previous command index

	// Construct this.cmdArray: An array of the options
	this.cmdArray = [];
	this.cmdArray.push(new DDFileMenuBarItem("Info", 0, FILE_VIEW_INFO));
	this.cmdArray.push(new DDFileMenuBarItem("View", 0, FILE_VIEW));
	this.cmdArray.push(new DDFileMenuBarItem("Batch", 0, FILE_ADD_TO_BATCH_DL));
	this.cmdArray.push(new DDFileMenuBarItem("DL", 0, FILE_DOWNLOAD_SINGLE));
	if (user.is_sysop)
	{
		// For the Edit command, the sysop will have too many things in the menu
		// if using the traditional (non-lightbar) interface; if using the lightbar
		// interface, there is still enough room.  For othe lightbar interface,
		// use the full word "Edit"; otherwise, use "E".
		if (gUseLightbarInterface && console.term_supports(USER_ANSI))
			this.cmdArray.push(new DDFileMenuBarItem("Edit", 0, FILE_EDIT));
		else
			this.cmdArray.push(new DDFileMenuBarItem("E", 0, FILE_EDIT));
		this.cmdArray.push(new DDFileMenuBarItem("Move", 0, FILE_MOVE));
		//this.cmdArray.push(new DDFileMenuBarItem("Del", 0, FILE_DELETE));
		this.cmdArray.push(new DDFileMenuBarItem("DEL", 0, FILE_DELETE, KEY_DEL));
	}
	else
	{
		// The user is not a sysop; there is room for the Edit comand
		this.cmdArray.push(new DDFileMenuBarItem("Edit", 0, FILE_EDIT));
	}
	//DDFileMenuBarItem(pItemText, pPos, pRetCode, pHotkeyOverride)
	if (!gUseLightbarInterface || !console.term_supports(USER_ANSI))
	{
		this.cmdArray.push(new DDFileMenuBarItem("Next", 0, NEXT_PAGE, KEY_PAGEDN));
		this.cmdArray.push(new DDFileMenuBarItem("Prev", 0, PREV_PAGE, KEY_PAGEUP));
		this.cmdArray.push(new DDFileMenuBarItem("First", 0, FIRST_PAGE));
		this.cmdArray.push(new DDFileMenuBarItem("Last", 0, LAST_PAGE));
	}
	this.cmdArray.push(new DDFileMenuBarItem("?", 0, HELP));
	this.cmdArray.push(new DDFileMenuBarItem("Quit", 0, QUIT));

	// Construct the prompt text (this must happen after this.cmdArray is built)
	this.promptText = "";
	this.constructPromptText(); // this.promptText will be constructed here
}
// For the DDFileMenuBar class: Constructs the prompt text.  This must be called after
// this.cmdArray is built.
//
// Return value: The number of additional solid blocks used to fill the whole screen row
function DDFileMenuBar_constructPromptText()
{
	var totalItemTextLen = 0;
	for (var i = 0; i < this.cmdArray.length; ++i)
		totalItemTextLen += this.cmdArray[i].itemText.length;
	// The number of inner characters (without the outer solid blocks) is the total text
	// length of all the items + 2 characters for each item except the last one
	var numInnerChars = totalItemTextLen + (2 * (this.cmdArray.length-1));
	// The number of solid blocks: Subtracting 11 because there will be 5 block characters on each side,
	// and subtract 1 extra so it doesn't fill the last character on the screen
	var numSolidBlocks = console.screen_columns - numInnerChars - 11;
	var numSolidBlocksPerSide = Math.floor(numSolidBlocks / 2);
	// Build the prompt text: Start with the left blocks
	this.promptText = "\x01n\x01w" + BLOCK1 + BLOCK2 + BLOCK3 + BLOCK4;
	for (var i = 0; i < numSolidBlocksPerSide; ++i)
		this.promptText += BLOCK4;
	this.promptText += THIN_RECTANGLE_LEFT;
	// Add the menu item text & block characters
	var menuItemXPos = 6 + numSolidBlocksPerSide; // The X position of the start of item text for each item
	for (var i = 0; i < this.cmdArray.length; ++i)
	{
		this.cmdArray[i].pos = menuItemXPos;
		var numTrailingBlockChars = 0;
		var selected = (i == this.currentCommandIdx);
		var withTrailingBlock = false;
		if (i < this.cmdArray.length-1)
		{
			withTrailingBlock = true;
			numTrailingBlockChars = 2;
		}
		menuItemXPos += this.cmdArray[i].itemText.length + numTrailingBlockChars;
		this.promptText += this.getDDFileMenuBarItemText(this.cmdArray[i].itemText, selected, withTrailingBlock);
	}
	// Add the right-side blocks
	this.promptText += "\x01w" + THIN_RECTANGLE_RIGHT;
	for (var i = 0; i < numSolidBlocksPerSide; ++i)
		this.promptText += BLOCK4;
	this.promptText += BLOCK3 + BLOCK2 + BLOCK1 + "\x01n";
}
// For the DDFileMenuBar class: Gets the text for a prompt item based on its index
function DDFileMenuBar_getItemTextFromIdx(pIdx)
{
	if (typeof(pIdx) !== "number" || pIdx < 0 || pIdx >= this.cmdArray.length)
		return "";
	return this.cmdArray[pIdx].itemText;
}
// For the DDFileMenuBar class: Writes the prompt text at the defined location
function DDFileMenuBar_writePromptLine()
{
	// Place the cursor at the defined location, then write the prompt text
	if (gUseLightbarInterface && console.term_supports(USER_ANSI))
		console.gotoxy(this.pos.x, this.pos.y);
	console.print(this.promptText);
}
// For the DDFileMenuBar class: Refreshes 2 items in the command bar text line
//
// Parameters:
//  pCmdIdx: The index of the new/current command
function DDFileMenuBar_refreshWithNewAction(pCmdIdx)
{
	if (typeof(pCmdIdx) !== "number")
		return;
	if (pCmdIdx == this.currentCommandIdx)
		return;

	// Refresh the prompt area for the previous index with regular colors
	// Re-draw the last item text with regular colors
	var itemText = this.getItemTextFromIdx(this.currentCommandIdx);
	if (console.term_supports(USER_ANSI))
	{
		console.gotoxy(this.cmdArray[this.currentCommandIdx].pos, this.pos.y);
		console.print("\x01n" + this.getDDFileMenuBarItemText(itemText, false, false));
		// Draw the new item text with selected colors
		itemText = this.getItemTextFromIdx(pCmdIdx);
		console.gotoxy(this.cmdArray[pCmdIdx].pos, this.pos.y);
		console.print("\x01n" + this.getDDFileMenuBarItemText(itemText, true, false));
		console.gotoxy(this.pos.x+strip_ctrl(this.promptText).length-1, this.pos.y);
	}

	this.lastCommandIdx = this.currentCommandIdx;
	this.currentCommandIdx = pCmdIdx;

	// Re-construct the bar text to make sure it's up to date with the selected action
	this.constructPromptText();
}
// For the DDFileMenuBar class: Returns a string containing a piece of text for the
// menu bar text with its color attributes.
//
// Parameters:
//  pText: The text for the item
//  pSelected: Boolean - Whether or not the item is selected
//  pWithTrailingBlock: Boolean - Whether or not to include the trailing block
//
// Return value: A string containing the item text for the action bar
function DDFileMenuBar_getDDFileMenuBarItemText(pText, pSelected, pWithTrailingBlock)
{
	if (typeof(pText) !== "string" || pText.length == 0)
		return "";

	var selected = (typeof(pSelected) === "boolean" ? pSelected : false);
	var withTrailingBlock = (typeof(pWithTrailingBlock) === "boolean" ? pWithTrailingBlock : false);

	// Separate the first character from the rest of the text
	var firstChar = pText.length > 0 ? pText.charAt(0) : "";
	var restOfText = pText.length > 1 ? pText.substr(1, pText.length - 1) : "";
	// Build the item text and return it
	var itemText = "\x01n";
	if (selected)
		itemText += "\x01" + "1\x01r\x01h" + firstChar + "\x01n\x01" + "1\x01k" + restOfText;
	else
		itemText += "\x01" + "6\x01c\x01h" + firstChar + "\x01n\x01" + "6\x01k" + restOfText;
	itemText += "\x01n";
	if (withTrailingBlock)
		itemText += "\x01w" + THIN_RECTANGLE_RIGHT + THIN_RECTANGLE_LEFT + "\x01n";
	return itemText;
}
// For the DDFileMenuBar class: Increments to the next menu item and refreshes the
// menu bar on the screen
function DDFileMenuBar_incrementMenuItemAndRefresh()
{
	var newCmdIdx = this.currentCommandIdx + 1;
	if (newCmdIdx >= this.cmdArray.length)
		newCmdIdx = 0;
	// Will set this.currentCommandIdx
	this.refreshWithNewAction(newCmdIdx);
}
// For the DDFileMenuBar class: Decrements to the previous menu item and refreshes the
// menu bar on the screen
function DDFileMenuBar_decrementMenuItemAndRefresh()
{
	var newCmdIdx = this.currentCommandIdx - 1;
	if (newCmdIdx < 0)
		newCmdIdx = this.cmdArray.length - 1;
	// Will set this.currentCommandIdx
	this.refreshWithNewAction(newCmdIdx);
}
// For the DDFileMenuBar class: Gets the return code for the currently selected action
function DDFileMenuBar_getCurrentSelectedAction()
{
	return this.cmdArray[this.currentCommandIdx].retCode;
}
// For the DDFileMenuBar class: Gets the return code matching a given character.
// If there is no match, this will return -1.
//
// Parameters:
//  pChar: The character to match
//  pCaseSensitive: Optional - Boolean - Whether or not to do a case-sensitive match.
//                  This defaults to false.
function DDFileMenuBar_getActionFromChar(pChar, pCaseSensitive)
{
	if (typeof(pChar) !== "string" || pChar.length == 0)
		return -1;

	var caseSensitive = (typeof(pCaseSensitive) === "boolean" ? pCaseSensitive : false);

	var retCode = -1;
	if (caseSensitive)
	{
		for (var i = 0; i < this.cmdArray.length && retCode == -1; ++i)
		{
			if (this.cmdArray[i].hotkeyOverride != null && typeof(this.cmdArray[i].hotkeyOverride) !== "undefined")
			{
				if (pChar == this.cmdArray[i].hotkeyOverride)
					retCode = this.cmdArray[i].retCode;
			}
			else if (this.cmdArray[i].itemText.length > 0 && this.cmdArray[i].itemText.charAt(0) == pChar)
				retCode = this.cmdArray[i].retCode;
		}
	}
	else
	{
		// Not case sensitive
		var charUpper = pChar.toUpperCase();
		for (var i = 0; i < this.cmdArray.length && retCode == -1; ++i)
		{
			if (this.cmdArray[i].hotkeyOverride != null && typeof(this.cmdArray[i].hotkeyOverride) !== "undefined")
			{
				if (pChar == this.cmdArray[i].hotkeyOverride)
					retCode = this.cmdArray[i].retCode;
			}
			else if (this.cmdArray[i].itemText.length > 0 && this.cmdArray[i].itemText.charAt(0).toUpperCase() == charUpper)
				retCode = this.cmdArray[i].retCode;
		}
	}
	return retCode;
}
// For the DDFileMenuBar class: Sets the current command item in the menu bar based on its
// action code
//
// Parameters:
//  pActionCode: The code of the action
//  pRefreshOnScreen: Optional boolean - Whether or not to refresh the command bar on the screen.
//                    Defaults to false.
function DDFileMenuBar_setCurrentActionCode(pActionCode, pRefreshOnScreen)
{
	if (typeof(pActionCode) !== "number")
		return;

	var refreshOnScreen = (typeof(pRefreshOnScreen) === "boolean" ? pRefreshOnScreen : false);

	for (var i = 0; i < this.cmdArray.length; ++i)
	{
		if (this.cmdArray[i].retCode == pActionCode)
		{
			if (refreshOnScreen)
				this.refreshWithNewAction(i);
			else
			{
				this.currentCommandIdx = i;
				this.lastCommandIdx = i;
			}
			break;
		}
	}
}
// For the DDFileMenuBar: Gets all the action hotkeys as a string
//
// Parameters:
//  pLowercase: Boolean - Whether or not to include letters as lowercase
//  pUppercase: Boolean - Whether or not to include letters as uppercase
//
// Return value: All the action hotkeys as a string
function DDFileMenuBar_getAllActionKeysStr(pLowercase, pUppercase)
{
	var hotkeysStr = "";
	for (var i = 0; i < this.cmdArray.length; ++i)
	{
		if (this.cmdArray[i].itemText.length > 0)
			hotkeysStr += this.cmdArray[i].itemText.charAt(0);
	}
	var hotkeysToReturn = "";
	if (pLowercase)
		hotkeysToReturn += hotkeysStr.toLowerCase();
	if (pUppercase)
		hotkeysToReturn += hotkeysStr.toUpperCase();
	return hotkeysToReturn;
}

// Consctructor for an DDFileMenuBarItem
//
// Parameters:
//  pItemText: The text of the item
//  pPos: Horizontal (or vertical) starting location in the bar
//  pRetCode: The item's return code
//  pHotkeyOverride: Optional: A key to use for the action instead of the first character in pItemText
function DDFileMenuBarItem(pItemText, pPos, pRetCode, pHotkeyOverride)
{
	this.itemText = pItemText;
	this.pos = pPos;
	this.retCode = pRetCode;
	this.hotkeyOverride = null;
	if (pHotkeyOverride != null && typeof(pHotkeyOverride) !== "undefined")
		this.hotkeyOverride = pHotkeyOverride;
}


///////////////////////////////////////////////////////////////////////////////
// Helper functions

// Gets file metadata & its file time from a filebase.
//
// Parameters:
//  pDirCode: The internal code of the directory the file is in
//  pFilename: The name of the file (without the full path)
//  pDetail: The detail level of the metadata object to get - See FileBase.DETAIL in JS docs
//
// Return value: An object containing the following properties:
//               fileMetadataObj: An object with extended file metadata from the filebase.
//                                This will have a dirCode property added.
//               fileTime: The timestamp of the file
function getFileInfoFromFilebase(pDirCode, pFilename, pDetail)
{
	if (typeof(pDirCode) !== "string" || pDirCode.length == 0 || typeof(pFilename) !== "string" || pFilename.length == 0)
		return null;

	var fileMetadataObj = null;
	var filebase = new FileBase(pDirCode);
	if (filebase.open())
	{
		// Just in case the file has the full path, get just the filename from it.
		var filename = file_getname(pFilename);
		var fileDetail = (typeof(pDetail) === "number" ? pDetail : FileBase.DETAIL.NORM);
		if (typeof(pDetail) === "number")
			fileDetail = pDetail;
		else
			fileDetail = (extendedDescEnabled() ? FileBase.DETAIL.EXTENDED : FileBase.DETAIL.NORM);
		fileMetadataObj = filebase.get(filename, fileDetail);
		fileMetadataObj.dirCode = pDirCode;
		//fileMetadataObj.size = filebase.get_size(filename);
		if (!fileMetadataObj.hasOwnProperty("time"))
			fileMetadataObj.time = filebase.get_time(filename);
		filebase.close();
	}

	return fileMetadataObj;
}

// Moves a file from one filebase to another
//
// Parameters:
//  pSrcFileMetadata: Metadata for the source file.  This is assumed to contain 'normal' detail (not extended)
//  pDestDirCode: The internal code of the destination filebase to move to the file to
//
// Return value: An object containing the following properties:
//               moveSucceeded: Boolean - Whether or not the move succeeded
//               failReason: If the move failed, this is a string that specifies why it failed
function moveFileToOtherFilebase(pSrcFileMetadata, pDestDirCode)
{
	var retObj = {
		moveSucceeded: false,
		failReason: ""
	};

	// pSrcFileMetadata is assumed to be a basic file metadata object, without extended
	// information.  Get a metadata object with maximum information so we have all
	// metadata available.
	var srcFilebase = new FileBase(pSrcFileMetadata.dirCode);
	if (srcFilebase.open())
	{
		var extdFileInfo = srcFilebase.get(pSrcFileMetadata, FileBase.DETAIL.MAX);
		// Move the file over, remove it from the original filebase, and add it to the new filebase
		var srcFilenameFull = srcFilebase.get_path(pSrcFileMetadata);
		var destFilenameFull = file_area.dir[pDestDirCode].path + pSrcFileMetadata.name;
		if (file_rename(srcFilenameFull, destFilenameFull))
		{
			if (srcFilebase.remove(pSrcFileMetadata.name, false))
			{
				// Add the file to the other directory
				var destFilebase = new FileBase(pDestDirCode);
				if (destFilebase.open())
				{
					retObj.moveSucceeded = destFilebase.add(extdFileInfo);
					destFilebase.close();
				}
				else
				{
					retObj.failReason = "Failed to open the destination filebase";
					// Try to add the file back to the source filebase
					var moveBackSucceeded = false;
					if (file_rename(destFilenameFull, srcFilenameFull))
						moveBackSucceeded = srcFilebase.add(extdFileInfo);
					if (!moveBackSucceeded)
						retObj.failReason += " & moving the file back failed";
				}
			}
			else
				retObj.failReason = "Failed to remove the file from the source directory";
		}
		else
			retObj.failReason = "Failed to move the file to the new filebase directory";

		srcFilebase.close();
	}
	else
	{
		var libIdx = file_area.dir[pSrcFileMetadata.dirCode].lib_index;
		var dirIdx = file_area.dir[pSrcFileMetadata.dirCode].index;
		var libDesc = file_area.lib_list[libIdx].description;
		var dirDesc =  file_area.dir[pSrcFileMetadata.dirCode].description;
		retObj.failreason = "Failed to open filebase: " + libDesc + " - " + dirDesc;
	}

	return retObj;
}

// Counts the number of occurrences of a substring within a string
//
// Parameters:
//  pStr: The string to count occurences in
//  pSubstr: The string to look for within pStr
//
// Return value: The number of occurrences of pSubstr found in pStr
function countOccurrencesInStr(pStr, pSubstr)
{
	if (typeof(pStr) !== "string" || typeof(pSubstr) !== "string") return 0;
	if (pStr.length == 0 || pSubstr.length == 0) return 0;

	var count = 0;
	var strIdx = pStr.indexOf(pSubstr);
	while (strIdx > -1 && strIdx < pStr.length)
	{
		++count;
		strIdx = pStr.indexOf(pSubstr, strIdx+1);
	}
	return count;
}

// Constructs & displays a frame with a border around it, and performs a user input loop
// until the user quits out of the input loop.
//
// Parameters:
//  pFrameX: The X coordinate of the upper-left corner of the frame (including border)
//  pFrameY: The Y coordinate of the upper-left corner of the frame (including border)
//  pFrameWidth: The width of the frame (including border)
//  pFrameHeight: The height of the frame (including border)
//  pBorderColor: The attribute codes for the border color
//  pFrameTitle: The title (text) to use in the frame border
//  pTitleColor: Optional string - The attribute codes for the color to use for the frame title
//  pFrameContents: The contents to display in the frame
//  pAdditionalQuitKeys: Optional - A string containing additional keys to quit the
//                       input loop.  This is case-sensitive.
//
// Return value: The last keypress/input from the user
function displayBorderedFrameAndDoInputLoop(pFrameX, pFrameY, pFrameWidth, pFrameHeight, pBorderColor, pFrameTitle, pTitleColor, pFrameContents, pAdditionalQuitKeys)
{
	if (typeof(pFrameX) !== "number" || typeof(pFrameY) !== "number" || typeof(pFrameWidth) !== "number" || typeof(pFrameHeight) !== "number")
		return;

	// Display the border for the frame
	var keyHelpStr = "\x01n\x01c\x01hQ\x01b/\x01cEnter\x01b/\x01cESC\x01y: \x01gClose\x01b";
	var scrollLoopNavHelp = "\x01c\x01hUp\x01b/\x01cDn\x01b/\x01cHome\x01b/\x01cEnd\x01b/\x01cPgup\x01b/\x01cPgDn\x01y: \x01gNav";
	if (console.screen_columns >= 80)
		keyHelpStr += ", " + scrollLoopNavHelp;
	var borderColor = (typeof(pBorderColor) === "string" ? pBorderColor : "\x01r");
	drawBorder(pFrameX, pFrameY, pFrameWidth, pFrameHeight, borderColor, "double", pFrameTitle, pTitleColor, keyHelpStr);

	// Construct the frame window for the file info
	// Create a Frame here with the full filename, extended description, etc.
	var frameX = pFrameX + 1;
	var frameY = pFrameY + 1;
	var frameWidth = pFrameWidth - 2;
	var frameHeight = pFrameHeight - 2;
	var frameObj = new Frame(frameX, frameY, frameWidth, frameHeight, BG_BLACK);
	frameObj.attr &=~ HIGH;
	frameObj.v_scroll = true;
	frameObj.h_scroll = false;
	frameObj.scrollbars = true;
	var scrollbarObj = new ScrollBar(frameObj, {bg: BG_BLACK, fg: LIGHTGRAY, orientation: "vertical", autohide: false});
	// Put the file info string in the frame window, then start the
	// user input loop for the frame
	frameObj.putmsg(pFrameContents, "\x01n");
	var lastUserInput = doFrameInputLoop(frameObj, scrollbarObj, pFrameContents, pAdditionalQuitKeys);
	//infoFrame.bottom();

	return lastUserInput;
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

// Displays the header lines for showing above the file list
//
// Parameters:
//  pTextOnly: Only draw the library & directory text (no decoration or other text).
//             This is optional & defaults to false.
//  pDirCodeOverride: Optional string: If this is valid, this will be used for the library & directory name
//  pNumberedMode: Boolean - Whether or not the menu/list has numbers in front of the file info items
function displayFileLibAndDirHeader(pTextOnly, pDirCodeOverride, pNumberedMode)
{
	// If the behavior flags include no header, then just return immediately
	if ((gListBehavior & FL_NO_HDR) == FL_NO_HDR)
		return;

	var textOnly = (typeof(pTextOnly) === "boolean" ? pTextOnly : false);

	// Determine if this is the first time this function has been called.  If so,
	// we'll want to update gNumHeaderLinesDisplayed at the end.
	var dispHdrFirstRun = false;
	if (typeof(displayFileLibAndDirHeader.firstRun) == "undefined")
	{
		dispHdrFirstRun = true;
		displayFileLibAndDirHeader.firstRun = true;
	}

	// For the library & directory text to display, if we're just listing the user's
	// current directory, use that.  Otherwise, if all search results are in the same
	// directory, then use a directory code from the file list.
	var libIdx = 0;
	var dirIdx = 0;
	var libDesc = "";
	var dirDesc =  "";
	var dirCode = "";
	if (gScriptMode == MODE_LIST_DIR)
		dirCode = gDirCode;
	else if (typeof(gFileList.allSameDir) === "boolean" && gFileList.allSameDir && gFileList.length > 0)
		dirCode = gFileList[0].dirCode;
	if (dirCode.length > 0)
	{
		libIdx = file_area.dir[dirCode].lib_index;
		dirIdx = file_area.dir[dirCode].index;
		libDesc = file_area.lib_list[libIdx].description;
		dirDesc =  file_area.dir[dirCode].description;
	}
	else if (typeof(pDirCodeOverride) === "string" && file_area.dir.hasOwnProperty(pDirCodeOverride))
	{
		libIdx = file_area.dir[pDirCodeOverride].lib_index;
		dirIdx = file_area.dir[pDirCodeOverride].index;
		libDesc = file_area.lib_list[libIdx].description;
		dirDesc =  file_area.dir[pDirCodeOverride].description;
	}
	else
	{
		libIdx = -1;
		dirIdx = -1;
		libDesc = "Various";
		dirDesc = "Various";
	}

	var hdrTextWidth = console.screen_columns - 21;
	var descWidth = hdrTextWidth - 11;
	var libText = format("\x01cLib \x01w\x01h#\x01b%4d\x01c: \x01n\x01c%-" + descWidth + "s\x01n", +(libIdx+1), libDesc.substr(0, descWidth));
	var dirText = format("\x01cDir \x01w\x01h#\x01b%4d\x01c: \x01n\x01c%-" + descWidth + "s\x01n", +(dirIdx+1), dirDesc.substr(0, descWidth));

	// Library line
	if (textOnly)
	{
		if (console.term_supports(USER_ANSI))
		{
			console.gotoxy(6, 1);
			console.print("\x01n" + libText);
			console.gotoxy(6, 2);
			console.print("\x01n" + dirText);
		}
	}
	else
	{
		console.print("\x01n\x01w" + BLOCK1 + BLOCK2 + BLOCK3 + BLOCK4 + THIN_RECTANGLE_LEFT);
		console.print(libText);
		console.print("\x01w" + THIN_RECTANGLE_RIGHT + "\x01k\x01h" + BLOCK4 + "\x01n\x01w" + THIN_RECTANGLE_LEFT +
					  "\x01g\x01hDD File\x01n\x01w");
		console.print(THIN_RECTANGLE_RIGHT + BLOCK4 + BLOCK3 + BLOCK2 + BLOCK1);
		console.crlf();
		// Directory line
		console.print("\x01n\x01w" + BLOCK1 + BLOCK2 + BLOCK3 + BLOCK4 + THIN_RECTANGLE_LEFT);
		console.print(dirText);
		console.print("\x01w" + THIN_RECTANGLE_RIGHT + "\x01k\x01h" + BLOCK4 + "\x01n\x01w" + THIN_RECTANGLE_LEFT +
					  "\x01g\x01hLister \x01n\x01w");
		console.print(THIN_RECTANGLE_RIGHT + BLOCK4 + BLOCK3 + BLOCK2 + BLOCK1);
		console.attributes = "N";

		// List header
		console.crlf();
		displayListHdrLine(false, pNumberedMode);

		if (dispHdrFirstRun)
		{
			gNumHeaderLinesDisplayed = 3;
			gErrorMsgBoxULY = gNumHeaderLinesDisplayed; // Note: console.screen_rows is 1-based
		}
	}
}
// Displays the header line with the column headers for the file list
//
// Parameters:
//  pMoveToLocationFirst: Boolean - Whether to move the cursor to the required location first.
//  pNumberedMode: Boolean - Whether or not the menu/list has numbers in front of the file info items
function displayListHdrLine(pMoveToLocationFirst, pNumberedMode)
{
	// Make the format string if it hasn't been made already
	if (displayListHdrLine.formatStr == undefined)
	{
		var filenameLen = gListIdxes.filenameEnd - gListIdxes.filenameStart;
		var fileSizeLen = gListIdxes.fileSizeEnd - gListIdxes.fileSizeStart -1;
		var descLen = gListIdxes.descriptionEnd - gListIdxes.descriptionStart + 1;
		var numItemsLen = gFileList.length.toString().length;
		if (pNumberedMode)
			descLen -= (numItemsLen+1);
		displayListHdrLine.formatStr = "\x01n\x01w\x01h";
		if (pNumberedMode)
			displayListHdrLine.formatStr += format("%" + numItemsLen + "s ", "#");
		displayListHdrLine.formatStr += "%-" + filenameLen + "s %" + fileSizeLen + "s %-"
		                             + +(descLen-7) + "s\x01n\x01w%5s\x01n";
	}

	if (pMoveToLocationFirst && console.term_supports(USER_ANSI))
		console.gotoxy(1, 3);

	var listHdrEndText = THIN_RECTANGLE_RIGHT + BLOCK4 + BLOCK3 + BLOCK2 + BLOCK1;
	printf(displayListHdrLine.formatStr, "Filename", "Size", "Description", listHdrEndText);
}

// Creates the menu for displaying the file list
//
// Parameters:
//  pQuitKeys: A string containing hotkeys to use as the menu's quit keys
//
// Return value: The DDLightbarMenu object for the file list in the file directory
function createFileListMenu(pQuitKeys)
{
	//DDLightbarMenu(pX, pY, pWidth, pHeight)
	// Create the menu object.  Place it below the header lines (which should have been written
	// before this), and also leave 1 row at the bottom for the prompt line
	var startRow = gNumHeaderLinesDisplayed > 0 ? gNumHeaderLinesDisplayed + 1 : 1;
	// If we'll be displaying short (one-line) file descriptions, then use the whole width
	// of the terminal (minus 1) for the menu width.  But if the user has extended (multi-line)
	// file descriptions enabled, then set the menu width only up through the file size, since
	// the extended file description will be displayed to the right of the menu.
	var menuWidth = console.screen_columns - 1;
	if (extendedDescEnabled())
		menuWidth = gListIdxes.fileSizeEnd + 1;
	var menuHeight = console.screen_rows - (startRow-1) - 1;
	var fileListMenu = new DDLightbarMenu(1, startRow, menuWidth, menuHeight);
	fileListMenu.borderEnabled = false;
	fileListMenu.ampersandHotkeysInItems = false;
	fileListMenu.wrapNavigation = false;
	fileListMenu.colors.itemNumColor = gColors.listNumTrad; // For numbered mode, if non-lightbar
	if (gUseLightbarInterface && console.term_supports(USER_ANSI))
	{
		fileListMenu.allowANSI = true;
		fileListMenu.scrollbarEnabled = true;
		fileListMenu.multiSelect = true;
		fileListMenu.numberedMode = false;
	}
	else
	{
		fileListMenu.allowANSI = false;
		fileListMenu.scrollbarEnabled = false;
		fileListMenu.multiSelect = false;
		fileListMenu.numberedMode = true;
	}

	fileListMenu.extdDescEnabled = extendedDescEnabled();

	// Add additional keypresses for quitting the menu's input loop so we can
	// respond to these keys.
	if (typeof(pQuitKeys) === "string")
		fileListMenu.AddAdditionalQuitKeys(pQuitKeys);

	// Define the menu's function to get the number of items.  This must be done here
	// in order for the menu's CanShowAllItemsInWindow() to work propertly.
	fileListMenu.NumItems = function() {
		return gFileList.length;
	};

	// Define a function for setting the description width, colors, and format string
	// based on whether the menu's scrollbar will be used.
	//
	// Return value: Boolean: Whether or not the width changed
	fileListMenu.SetItemWidthsColorsAndFormatStr = function() {
		var widthChanged = false;
		var oldDescriptionEnd = gListIdxes.descriptionEnd;
		gListIdxes.descriptionEnd = console.screen_columns - 1; // Leave 1 character remaining on the screen
		// If the file list menu will use its scrollbar, then reduce the description width by 1
		if (this.scrollbarEnabled && !this.CanShowAllItemsInWindow())
			gListIdxes.descriptionEnd -= 1;
		widthChanged = (gListIdxes.descriptionEnd != oldDescriptionEnd);
		this.SetColors({
			itemColor: [{start: gListIdxes.filenameStart, end: gListIdxes.filenameEnd, attrs: gColors.filename},
			            {start: gListIdxes.fileSizeStart, end: gListIdxes.fileSizeEnd, attrs: gColors.fileSize},
			            {start: gListIdxes.descriptionStart, end: gListIdxes.descriptionEnd, attrs: gColors.desc}],
			selectedItemColor: [{start: gListIdxes.filenameStart, end: gListIdxes.filenameEnd, attrs: gColors.bkgHighlight + gColors.filenameHighlight},
			                    {start: gListIdxes.fileSizeStart, end: gListIdxes.fileSizeEnd, attrs: gColors.bkgHighlight + gColors.fileSizeHighlight},
			                    {start: gListIdxes.descriptionStart, end: gListIdxes.descriptionEnd, attrs: gColors.bkgHighlight + gColors.descHighlight}]
		});

		this.filenameLen = gListIdxes.filenameEnd - gListIdxes.filenameStart;
		this.fileSizeLen = gListIdxes.fileSizeEnd - gListIdxes.fileSizeStart -1;
		this.shortDescLen = gListIdxes.descriptionEnd - gListIdxes.descriptionStart + 1;
		// If extended descriptions are enabled, then we won't be writing a description here
		if (this.extdDescEnabled)
			this.fileFormatStr = "%-" + this.filenameLen + "s %" + this.fileSizeLen + "s";
		else
		{
			this.fileFormatStr = "%-" + this.filenameLen + "s %" + this.fileSizeLen
			                   + "s %-" + this.shortDescLen + "s";
		}
		return widthChanged;
	};
	// Set up the menu's description width, colors, and format string
	fileListMenu.SetItemWidthsColorsAndFormatStr();

	fileListMenu.lastFileDirCode = "";
	// Define the menu function for getting an item
	fileListMenu.GetItem = function(pIdx) {
		// In here, 'this' refers to the fileListMenu object
		// If doing a file search, then update the header with the file library & directory
		// name of the currently selected file (instead of displaying "Various"). This seems
		// like a bit of a hack, but it works.
		var allSameDir = (typeof(gFileList.allSameDir) === "boolean" ? gFileList.allSameDir : false);
		if (isDoingFileSearch() && !allSameDir && gFileList[pIdx].dirCode != this.lastFileDirCode)
		{
			if ((gListBehavior & FL_NO_HDR) != FL_NO_HDR)
			{
				var originalCurPos = console.getxy();
				displayFileLibAndDirHeader(true, gFileList[pIdx].dirCode, gFileListMenu.numberedMode);
				console.gotoxy(originalCurPos);
			}
		}
		this.lastFileDirCode = gFileList[pIdx].dirCode;

		var menuItemObj = this.MakeItemWithRetval(pIdx);
		var filename = shortenFilename(gFileList[pIdx].name, this.filenameLen, true);
		// FileBase.format_name() could also be called to format the filename for display:
		/*
		var filebase = new FileBase(gFileList[pIdx].dirCode);
		if (filebase.open())
		{
			filename = filebase.format_name(gFileList[pIdx].name, this.filenameLen, true);
			filebase.close();
		}
		*/
		var desc = (typeof(gFileList[pIdx].desc) === "string" ? gFileList[pIdx].desc : "");
		menuItemObj.text = format(this.fileFormatStr,
		                          filename,
								  getFileSizeStr(gFileList[pIdx].size, this.fileSizeLen),
		                          desc.substr(0, this.shortDescLen));
		return menuItemObj;
	}

	fileListMenu.selectedItemIndexes = {};
	fileListMenu.numSelectedItemIndexes = function() {
		return Object.keys(this.selectedItemIndexes).length;
	};

	// If extended file descriptions are enabled, set the OnItemNav function for
	// when the user navigates to a new item, to display the file description
	// next to the file menu.
	if (extendedDescEnabled())
	{
		fileListMenu.OnItemNav = function(pOldItemIdx, pNewItemIdx) {
			displayFileExtDescOnMainScreen(pNewItemIdx);
		};
	}

	return fileListMenu;
}

// Creates the menu for choosing a file library (for moving a message to another message area).
// The return value of the chosen item is the file library index.
//
// Return value: The DDLightbarMenu object for choosing a file library
function createFileLibMenu()
{
	// This probably shouldn't happen, but check to make sure there are file libraries
	if (file_area.lib_list.length == 0)
	{
		console.crlf();
		console.print("\x01n\x01y\x01hThere are no file libraries available\x01n");
		console.crlf();
		console.pause();
		return;
	}

	//DDLightbarMenu(pX, pY, pWidth, pHeight)
	// Create the menu object
	var startRow = gNumHeaderLinesDisplayed + 4;
	var fileLibMenu = new DDLightbarMenu(5, startRow, console.screen_columns - 10, console.screen_rows - startRow - 5);
	fileLibMenu.scrollbarEnabled = gUseLightbarInterface && console.term_supports(USER_ANSI);
	fileLibMenu.borderEnabled = true;
	fileLibMenu.multiSelect = false;
	fileLibMenu.ampersandHotkeysInItems = false;
	fileLibMenu.wrapNavigation = false;
	fileLibMenu.allowANSI = gUseLightbarInterface && console.term_supports(USER_ANSI);

	// Add additional keypresses for quitting the menu's input loop.
	// Q: Quit
	var additionalQuitKeys = "qQ";
	fileLibMenu.AddAdditionalQuitKeys(additionalQuitKeys);

	// Re-define the menu's function for getting the number of items.  This is necessary
	// here in order for the menu's CanShowAllItemsInWindow() to work properly.
	fileLibMenu.NumItems = function() {
		return file_area.lib_list.length;
	};

	// Construct a format string for the file libraries
	var largestNumDirs = getLargestNumDirsWithinFileLibs();
	fileLibMenu.libNumLen = file_area.lib_list.length.toString().length;
	fileLibMenu.numDirsLen = largestNumDirs.toString().length;
	var menuInnerWidth = fileLibMenu.size.width - 2; // Menu width excluding borders
	// If the scrollbar will be showing, reduce the inner width by 1
	if (fileLibMenu.scrollbarEnabled && !fileLibMenu.CanShowAllItemsInWindow())
		--menuInnerWidth;
	// Allow 2 for spaces
	fileLibMenu.libDescLen = menuInnerWidth - fileLibMenu.libNumLen - fileLibMenu.numDirsLen - 2;
	if (gUseLightbarInterface && console.term_supports(USER_ANSI))
		fileLibMenu.libFormatStr = "%" + fileLibMenu.libNumLen + "d %-" + fileLibMenu.libDescLen + "s %" + fileLibMenu.numDirsLen + "d";
	else
		fileLibMenu.libFormatStr = "%-" + fileLibMenu.libDescLen + "s %" + fileLibMenu.numDirsLen + "d";

	// Colors and their indexes
	fileLibMenu.borderColor = gColors.fileAreaMenuBorder;
	var libNumStart = 0;
	var libNumEnd = fileLibMenu.libNumLen;
	var descStart = libNumEnd;
	var descEnd = descStart + fileLibMenu.libDescLen;
	var numDirsStart = descEnd;
	//var numDirsEnd = numDirsStart + fileLibMenu.numDirsLen;
	// Selected colors (for lightbar interface)
	fileLibMenu.SetColors({
		selectedItemColor: [{start: libNumStart, end: libNumEnd, attrs: "\x01n" + gColors.fileAreaMenuHighlightBkg + gColors.fileAreaNumHighlight},
		                    {start: descStart, end:descEnd, attrs: "\x01n" + gColors.fileAreaMenuHighlightBkg + gColors.fileAreaDescHighlight},
		                    {start: numDirsStart, end: -1, attrs: "\x01n" + gColors.fileAreaMenuHighlightBkg + gColors.fileAreaNumItemsHighlight}]
	});
	// Non-selected item colors
	if (gUseLightbarInterface && console.term_supports(USER_ANSI))
	{
		fileLibMenu.SetColors({
			itemColor: [{start: libNumStart, end: libNumEnd, attrs: "\x01n" + gColors.fileNormalBkg + gColors.fileAreaNum},
						{start: descStart, end:descEnd, attrs: "\x01n" + gColors.fileNormalBkg + gColors.fileAreaDesc},
						{start: numDirsStart, end: -1, attrs: "\x01n" + gColors.fileNormalBkg + gColors.fileAreaNumItems}]
		});
	}
	else
	{
		fileLibMenu.colors.itemNumColor = gColors.listNumTrad;
		descStart = 0;
		descEnd = descStart + fileLibMenu.libDescLen;
		numDirsStart = descEnd;
		fileLibMenu.SetColors({
			itemColor: [{start: descStart, end:descEnd, attrs: "\x01n" + gColors.fileNormalBkgTrad + gColors.fileAreaDescTrad},
						{start: numDirsStart, end: -1, attrs: "\x01n" + gColors.fileNormalBkgTrad + gColors.fileAreaNumItemsTrad}]
		});
	}

	fileLibMenu.topBorderText = "\x01y\x01hFile Libraries";
	// Define the menu function for getting an item
	fileLibMenu.GetItem = function(pIdx) {
		var menuItemObj = this.MakeItemWithRetval(pIdx);
		if (gUseLightbarInterface && console.term_supports(USER_ANSI))
		{
			menuItemObj.text = format(this.libFormatStr,
			                          pIdx + 1,//file_area.lib_list[pIdx].number + 1,
			                          file_area.lib_list[pIdx].description.substr(0, this.libDescLen),
			                          file_area.lib_list[pIdx].dir_list.length);
		}
		else
		{
			menuItemObj.text = format(this.libFormatStr,
			                          file_area.lib_list[pIdx].description.substr(0, this.libDescLen),
			                          file_area.lib_list[pIdx].dir_list.length);
		}
		return menuItemObj;
	}

	return fileLibMenu;
}
// Helper for createFileLibMenu(): Returns the largest number of directories within the file libraries
function getLargestNumDirsWithinFileLibs()
{
	var largestNumDirs = 0;
	for (var libIdx = 0; libIdx < file_area.lib_list.length; ++libIdx)
	{
		if (file_area.lib_list[libIdx].dir_list.length > largestNumDirs)
			largestNumDirs = file_area.lib_list[libIdx].dir_list.length;
	}
	return largestNumDirs;
}

// Creates the menu for choosing a file directory within a file library (for moving
// a message to another message area).
// The return value of the chosen item is the internal code for the file directory.
//
// Parameters:
//  pLibIdx: The file directory index
//
// Return value: The DDLightbarMenu object for choosing a file directory within the
// file library at the given index
function createFileDirMenu(pLibIdx)
{
	if (typeof(pLibIdx) !== "number")
		return null;

	var startRow = gNumHeaderLinesDisplayed + 4;
	// Make sure there are directories in this library
	if (file_area.lib_list[pLibIdx].dir_list.length == 0)
	{
		displayMsg("There are no directories in this file library", true, true);
		return null;
	}

	//DDLightbarMenu(pX, pY, pWidth, pHeight)
	// Create the menu object
	var fileDirMenu = new DDLightbarMenu(5, startRow, console.screen_columns - 10, console.screen_rows - startRow - 5);
	fileDirMenu.scrollbarEnabled = gUseLightbarInterface && console.term_supports(USER_ANSI);
	fileDirMenu.borderEnabled = true;
	fileDirMenu.multiSelect = false;
	fileDirMenu.ampersandHotkeysInItems = false;
	fileDirMenu.wrapNavigation = false;
	fileDirMenu.allowANSI = gUseLightbarInterface && console.term_supports(USER_ANSI);

	// Add additional keypresses for quitting the menu's input loop.
	// Q: Quit
	var additionalQuitKeys = "qQ";
	fileDirMenu.AddAdditionalQuitKeys(additionalQuitKeys);

	// Re-define the menu's function for getting the number of items.  This is necessary
	// here in order for the menu's CanShowAllItemsInWindow() to work properly.
	fileDirMenu.NumItems = function() {
		return file_area.lib_list[this.libIdx].dir_list.length;
	};

	fileDirMenu.libIdx = pLibIdx;
	// Construct a format string for the file libraries
	var largestNumFiles = getLargestNumFilesInLibDirs(pLibIdx);
	fileDirMenu.dirNumLen = file_area.lib_list[pLibIdx].dir_list.length.toString().length;
	fileDirMenu.numFilesLen = largestNumFiles.toString().length;
	var menuInnerWidth = fileDirMenu.size.width - 2; // Menu width excluding borders
	// If the scrollbar will be showing, reduce the inner width by 1
	if (fileDirMenu.scrollbarEnabled && !fileDirMenu.CanShowAllItemsInWindow())
		--menuInnerWidth;
	// Allow 2 for spaces
	fileDirMenu.dirDescLen = menuInnerWidth - fileDirMenu.dirNumLen - fileDirMenu.numFilesLen - 2;
	if (gUseLightbarInterface && console.term_supports(USER_ANSI))
		fileDirMenu.dirFormatStr = "%" + fileDirMenu.dirNumLen + "d %-" + fileDirMenu.dirDescLen + "s %" + fileDirMenu.numFilesLen + "d";
	else
		fileDirMenu.dirFormatStr = "%-" + fileDirMenu.dirDescLen + "s %" + fileDirMenu.numFilesLen + "d";

	// Colors and their indexes
	fileDirMenu.borderColor = gColors.fileAreaMenuBorder;
	var dirNumStart = 0;
	var dirNumEnd = fileDirMenu.dirNumLen;
	var descStart = dirNumEnd;
	var descEnd = descStart + fileDirMenu.dirDescLen;
	var numDirsStart = descEnd;
	//var numDirsEnd = numDirsStart + fileDirMenu.numDirsLen;
	// Selected colors (for lightbar interface)
	fileDirMenu.SetColors({
		selectedItemColor: [{start: dirNumStart, end: dirNumEnd, attrs: "\x01n" + gColors.fileAreaMenuHighlightBkg + gColors.fileAreaNumHighlight},
		                    {start: descStart, end:descEnd, attrs: "\x01n" + gColors.fileAreaMenuHighlightBkg + gColors.fileAreaDescHighlight},
		                    {start: numDirsStart, end: -1, attrs: "\x01n" + gColors.fileAreaMenuHighlightBkg + gColors.fileAreaNumItemsHighlight}]
	});
	// Non-selected item colors
	if (gUseLightbarInterface && console.term_supports(USER_ANSI))
	{
		fileDirMenu.SetColors({
			itemColor: [{start: dirNumStart, end: dirNumEnd, attrs: "\x01n" + gColors.fileNormalBkg + gColors.fileAreaNum},
						{start: descStart, end:descEnd, attrs: "\x01n" + gColors.fileNormalBkg + gColors.fileAreaDesc},
						{start: numDirsStart, end: -1, attrs: "\x01n" + gColors.fileNormalBkg + gColors.fileAreaNumItems}]
		});
	}
	else
	{
		fileDirMenu.colors.itemNumColor = gColors.listNumTrad;
		descStart = 0;
		descEnd = descStart + fileDirMenu.dirDescLen;
		numDirsStart = descEnd;
		fileDirMenu.SetColors({
			itemColor: [{start: descStart, end:descEnd, attrs: "\x01n" + gColors.fileNormalBkgTrad + gColors.fileAreaDescTrad},
						{start: numDirsStart, end: -1, attrs: "\x01n" + gColors.fileNormalBkgTrad + gColors.fileAreaNumItemsTrad}]
		});
	}

	fileDirMenu.topBorderText = "\x01y\x01h" + ("File directories of " + file_area.lib_list[pLibIdx].description).substr(0, fileDirMenu.size.width-2);
	// Define the menu function for ggetting an item
	fileDirMenu.GetItem = function(pIdx) {
		// Return the internal code for the directory for the item
		var menuItemObj = this.MakeItemWithRetval(file_area.lib_list[this.libIdx].dir_list[pIdx].code);
		if (gUseLightbarInterface && console.term_supports(USER_ANSI))
		{
			menuItemObj.text = format(this.dirFormatStr,
			                          pIdx + 1,//file_area.lib_list[this.libIdx].dir_list[pIdx].number + 1,
			                          file_area.lib_list[this.libIdx].dir_list[pIdx].description.substr(0, this.dirDescLen),
			                          file_area.lib_list[this.libIdx].dir_list[pIdx].files);
		}
		else
		{
			menuItemObj.text = format(this.dirFormatStr,
			                          file_area.lib_list[this.libIdx].dir_list[pIdx].description.substr(0, this.dirDescLen),
			                          file_area.lib_list[this.libIdx].dir_list[pIdx].files);

		}
		return menuItemObj;
	}

	return fileDirMenu;
}

// Returns the largest number of files in all directories in a file library
//
// Parameters:
//  pLibIdx: The index of a file library
//
// Return value: The largest number of files of all the directories in the file library
function getLargestNumFilesInLibDirs(pLibIdx)
{
	var largestNumFiles = 0;
	for (var dirIdx = 0; dirIdx < file_area.lib_list[pLibIdx].dir_list.length; ++dirIdx)
	{
		var numFilesInDir = file_area.lib_list[pLibIdx].dir_list[dirIdx].files;
		if (numFilesInDir > largestNumFiles)
			largestNumFiles = numFilesInDir;
	}
	return largestNumFiles;
}

// Returns a formatted string representation of a file size.  Tries
// to put a size designation at the end if possible.
//
// Parameters:
//  pFileSize: The size of the file in bytes
//  pMaxLen: Optional - The maximum length of the string
//
// Return value: A formatted string representation of the file size
function getFileSizeStr(pFileSize, pMaxLen)
{
	var fileSizeStr = "?";
	if (typeof(pFileSize) !== "number" || pFileSize < 0)
		return fileSizeStr;

	// TODO: Improve
	if (pFileSize >= BYTES_PER_GB) // Gigabytes
	{
		fileSizeStr = format("%.02fG", +(pFileSize / BYTES_PER_GB));
		if (typeof(pMaxLen) === "number" && pMaxLen > 0 && fileSizeStr.length > pMaxLen)
		{
			fileSizeStr = format("%.1fG", +(pFileSize / BYTES_PER_GB));
			if (fileSizeStr.length > pMaxLen)
			{
				// If there's a decimal point, then put the size designation after it
				var dotIdx = fileSizeStr.lastIndexOf(".");
				if (dotIdx > 0)
				{
					if (/\.$/.test(fileSizeStr))
						fileSizeStr = fileSizeStr.substr(0, dotIdx) + "G";
					else
						fileSizeStr = fileSizeStr.substr(0, fileSizeStr.length-1) + "G";
				}
			}
			fileSizeStr = fileSizeStr.substr(0, pMaxLen);
		}
	}
	else if (pFileSize >= BYTES_PER_MB) // Megabytes
	{
		fileSizeStr = format("%.02fM", +(pFileSize / BYTES_PER_MB));
		if (typeof(pMaxLen) === "number" && pMaxLen > 0 && fileSizeStr.length > pMaxLen)
		{
			fileSizeStr = format("%.1fM", +(pFileSize / BYTES_PER_MB));
			if (fileSizeStr.length > pMaxLen)
			{
				// If there's a decimal point, then put the size designation after it
				var dotIdx = fileSizeStr.lastIndexOf(".");
				if (dotIdx > 0)
				{
					if (/\.$/.test(fileSizeStr))
						fileSizeStr = fileSizeStr.substr(0, dotIdx) + "M";
					else
						fileSizeStr = fileSizeStr.substr(0, fileSizeStr.length-1) + "M";
				}
			}
			fileSizeStr = fileSizeStr.substr(0, pMaxLen);
		}
	}
	else if (pFileSize >= BYTES_PER_KB) // Kilobytes
	{
		fileSizeStr = format("%.02fK", +(pFileSize / BYTES_PER_KB));
		if (typeof(pMaxLen) === "number" && pMaxLen > 0 && fileSizeStr.length > pMaxLen)
		{
			fileSizeStr = format("%.1fK", +(pFileSize / BYTES_PER_KB));
			if (fileSizeStr.length > pMaxLen)
			{
				// If there's a decimal point, then put the size designation after it
				var dotIdx = fileSizeStr.lastIndexOf(".");
				if (dotIdx > 0)
				{
					if (/\.$/.test(fileSizeStr))
						fileSizeStr = fileSizeStr.substr(0, dotIdx) + "K";
					else
						fileSizeStr = fileSizeStr.substr(0, fileSizeStr.length-1) + "K";
				}
			}
			fileSizeStr = fileSizeStr.substr(0, pMaxLen);
		}
	}
	else
	{
		fileSizeStr = pFileSize.toString();
		if (typeof(pMaxLen) === "number" && pMaxLen > 0 && fileSizeStr.length > pMaxLen)
			fileSizeStr = fileSizeStr.substr(0, pMaxLen);
	}

	return fileSizeStr;
}

// Displays some text with a solid horizontal line on the next line.
//
// Parameters:
//  pText: The text to display
//  pCenter: Whether or not to center the text.  Optional; defaults
//           to false.
//  pTextColor: The color to use for the text.  Optional; by default,
//              normal white will be used.
//  pLineColor: The color to use for the line underneath the text.
//              Optional; by default, bright black will be used.
function displayTextWithLineBelow(pText, pCenter, pTextColor, pLineColor)
{
	var centerText = (typeof(pCenter) == "boolean" ? pCenter : false);
	var textColor = (typeof(pTextColor) == "string" ? pTextColor : "\x01n\x01w");
	var lineColor = (typeof(pLineColor) == "string" ? pLineColor : "\x01n\x01k\x01h");

	// Output the text and a solid line on the next line.
	if (centerText)
	{
		console.center(textColor + pText);
		var solidLine = "";
		var textLength = console.strlen(pText);
		for (var i = 0; i < textLength; ++i)
			solidLine += HORIZONTAL_SINGLE;
		console.center(lineColor + solidLine);
	}
	else
	{
		console.print(textColor + pText);
		console.crlf();
		console.print(lineColor);
		var textLength = console.strlen(pText);
		for (var i = 0; i < textLength; ++i)
			console.print(HORIZONTAL_SINGLE);
		console.crlf();
	}
}

// Returns a string for a number with commas added every 3 places
//
// Parameters:
//  pNum: The number to format
//
// Return value: A string with the number formatted with commas every 3 places
function numWithCommas(pNum)
{
	var numStr = "";
	if (typeof(pNum) === "number")
		numStr = pNum.toString();
	else if (typeof(pNum) === "string")
		numStr = pNum;
	else
		return "";

	// Check for a decimal point in the number
	var afterDotSuffix = "";
	var dotIdx = numStr.lastIndexOf(".");
	if (dotIdx > -1)
	{
		afterDotSuffix = numStr.substr(dotIdx+1);
		numStr = numStr.substr(0, dotIdx);
	}
	// First, build an array containing sections of the number containing
	// 3 digits each (the last may contain less than 3)
	var numParts = [];
	var i = numStr.length - 1;
	var continueOn = true;
	while (continueOn/*i >= 0*/)
	{
		if (i >= 3)
		{
			numParts.push(numStr.substr(i-2, 3));
			i -= 3;
		}
		else
		{
			numParts.push(numStr.substr(0, i+1));
			i -= i;
			continueOn = false;
		}
	}
	// Reverse the array so the sections of digits are in forward order
	numParts.reverse();
	// Re-build the number string with commas
	numStr = "";
	for (var i = 0; i < numParts.length; ++i)
		numStr += numParts[i] + ",";
	if (/,$/.test(numStr))
		numStr = numStr.substr(0, numStr.length-1);
	// Append back the value after the decimal place if there was one
	if (afterDotSuffix.length > 0)
		numStr += "." + afterDotSuffix;
	return numStr;
}

// Displays a set of messages at the status location on the screen, along with a border.  Then
// refreshes the area of the screen to erase the message box.
//
// Parameters:
//  pMsgArray: An array containing the messages to display
//  pIsError: Boolean - Whether or not the messages are an error
//  pWaitAndErase: Optional boolean - Whether or not to automatically wait a moment and then
//                 erase the message box after drawing.  Defaults to true.
function displayMsgs(pMsgArray, pIsError, pWaitAndErase)
{
	if (typeof(pMsgArray) !== "object" || pMsgArray.length == 0)
		return;

	var waitAndErase = (typeof(pWaitAndErase) === "boolean" ? pWaitAndErase : true);

	if (gUseLightbarInterface)
	{
		// Draw the box border, then write the messages
		var title = pIsError ? "Error" : "Message";
		var titleColor = pIsError ? gColors.errorMessage : gColors.successMessage;
		drawBorder(gErrorMsgBoxULX, gErrorMsgBoxULY, gErrorMsgBoxWidth, gErrorMsgBoxHeight,
				   gColors.errorBoxBorder, "single", title, titleColor, "");
		var msgColor = "\x01n" + (pIsError ? gColors.errorMessage : gColors.successMessage);
		var innerWidth = gErrorMsgBoxWidth - 2;
		var msgFormatStr = msgColor + "%-" + innerWidth + "s\x01n";
		for (var i = 0; i < pMsgArray.length; ++i)
		{
			console.gotoxy(gErrorMsgBoxULX+1, gErrorMsgBoxULY+1);
			printf(msgFormatStr, pMsgArray[i].substr(0, innerWidth));
			if (waitAndErase)
			{
				// Wait for the error wait duration
				mswait(gErrorMsgWaitMS);
			}
		}
		if (waitAndErase)
			eraseMsgBoxScreenArea();
	}
	else
	{
		console.attributes = "N";
		var msgColor = "\x01n" + (pIsError ? gColors.errorMessage : gColors.successMessage);
		for (var i = 0; i < pMsgArray.length; ++i)
			console.print(msgColor + pMsgArray[i] + "\r\n");
		if (waitAndErase)
		{
			// Wait for the error wait duration
			mswait(gErrorMsgWaitMS);
		}
	}
}
function displayMsg(pMsg, pIsError, pWaitAndErase)
{
	if (typeof(pMsg) !== "string")
		return;
	displayMsgs([ pMsg ], pIsError, pWaitAndErase);
}
// Erases the message box screen area by re-drawing the necessary components
function eraseMsgBoxScreenArea()
{
	// Refresh the list header line and have the file list menu refresh itself over
	// the error message window
	displayListHdrLine(true, gFileListMenu.numberedMode);
	// This used to call gFileListMenu.DrawPartialAbs
	refreshScreenMainContent(gErrorMsgBoxULX, gErrorMsgBoxULY+1, gErrorMsgBoxWidth, gErrorMsgBoxHeight-2);
}

// Draws a border
//
// Parameters:
//  pX: The X location of the upper left corner
//  pY: The Y location of the upper left corner
//  pWidth: The width of the box
//  pHeight: The height of the box
//  pColor: A string containing color/attribute codes for the border characters
//  pLineStyle: A string specifying the border character style, either "single" or "double"
//  pTitle: Optional - A string specifying title text for the top border
//  pTitleColor: Optional - Attribute codes for the color to use for the title text
//  pBottomBorderText: Optional - A string specifying text to include in the bottom border
function drawBorder(pX, pY, pWidth, pHeight, pColor, pLineStyle, pTitle, pTitleColor, pBottomBorderText)
{
	if (typeof(pX) !== "number" || typeof(pY) !== "number" || typeof(pWidth) !== "number" || typeof(pHeight) !== "number")
		return;
	if (typeof(pColor) !== "string")
		return;

	var borderChars = {
		UL: UPPER_LEFT_SINGLE,
		UR: UPPER_RIGHT_SINGLE,
		LL: LOWER_LEFT_SINGLE,
		LR: LOWER_RIGHT_SINGLE,
		preText: RIGHT_T_SINGLE,
		postText: LEFT_T_SINGLE,
		horiz: HORIZONTAL_SINGLE,
		vert: VERTICAL_SINGLE
	};
	if (typeof(pLineStyle) === "string" && pLineStyle.toUpperCase() == "DOUBLE")
	{
		borderChars.UL = UPPER_LEFT_DOUBLE;
		borderChars.UR = UPPER_RIGHT_DOUBLE;
		borderChars.LL = LOWER_LEFT_DOUBLE;
		borderChars.LR = LOWER_RIGHT_DOUBLE;
		borderChars.preText = RIGHT_T_DOUBLE;
		borderChars.postText = LEFT_T_DOUBLE
		borderChars.horiz = HORIZONTAL_DOUBLE;
		borderChars.vert = VERTICAL_DOUBLE;
	}

	// Top border
	console.gotoxy(pX, pY);
	console.print("\x01n" + pColor);
	console.print(borderChars.UL);
	var innerWidth = pWidth - 2;
	// Include the title text in the top border, if there is any specified
	var titleTextWithoutAttrs = strip_ctrl(pTitle); // Possibly used twice, so only call strip_ctrl() once
	if (typeof(pTitle) === "string" && titleTextWithoutAttrs.length > 0)
	{
		var titleLen = strip_ctrl(pTitle).length;
		if (titleLen > pWidth - 4)
			titleLen = pWidth - 4;
		innerWidth -= titleLen;
		innerWidth -= 2; // ?? Correctional
		// Note: substrWithAttrCodes() is defined in dd_lightbar_menu.js
		var titleText = pTitle;
		if (typeof(pTitleColor) === "string")
			titleText = "\x01n" + pTitleColor + titleTextWithoutAttrs;
		console.print(borderChars.preText + "\x01n" + substrWithAttrCodes(titleText, 0, titleLen) +
		              "\x01n" + pColor + borderChars.postText);
		if (innerWidth > 0)
			console.print(pColor);
	}
	for (var i = 0; i < innerWidth; ++i)
		console.print(borderChars.horiz);
	console.print(borderChars.UR);
	// Side borders
	var rightCol = pX + pWidth - 1;
	var endScreenRow = pY + pHeight - 1;
	for (var screenRow = pY + 1; screenRow < endScreenRow; ++screenRow)
	{
		console.gotoxy(pX, screenRow);
		console.print(borderChars.vert);
		console.gotoxy(rightCol, screenRow);
		console.print(borderChars.vert);
	}
	// Bottom border
	console.gotoxy(pX, endScreenRow);
	console.print(borderChars.LL);
	innerWidth = pWidth - 2;
	// Include the bottom border text in the top border, if there is any specified
	if (typeof(pBottomBorderText) === "string" && pBottomBorderText.length > 0)
	{
		var textLen = strip_ctrl(pBottomBorderText).length;
		if (textLen > pWidth - 4)
			textLen = pWidth - 4;
		innerWidth -= textLen;
		innerWidth -= 2; // ?? Correctional
		// Note: substrWithAttrCodes() is defined in dd_lightbar_menu.js
		console.print(borderChars.preText + "\x01n" + substrWithAttrCodes(pBottomBorderText, 0, textLen) +
		              "\x01n" + pColor + borderChars.postText);
		if (innerWidth > 0)
			console.print(pColor);
	}
	for (var i = 0; i < innerWidth; ++i)
		console.print(borderChars.horiz);
	console.print(borderChars.LR);
}

// Draws a horizontal separator line on the screen, in high green
//
// Parameters:
//  pX: The X coordinate to start at
//  pY: The Y coordinate to start at
//  pWidth: The width of the line to draw
function drawSeparatorLine(pX, pY, pWidth)
{
	if (typeof(pX) !== "number" || typeof(pY) !== "number" || typeof(pWidth) !== "number")
		return;
	if (pX < 1 || pX > console.screen_columns)
		return;
	if (pY < 1 || pY > console.screen_rows)
		return;
	if (pWidth < 1)
		return;

	var width = pWidth;
	var maxWidth = console.screen_columns - pX + 1;
	if (width > maxWidth)
		width = maxWidth;

	console.gotoxy(pX, pY);
	console.print("\x01n\x01g\x01h");
	for (var i = 0; i < width; ++i)
		console.print(HORIZONTAL_SINGLE);
	console.attributes = "N";
}

// Confirms with the user to perform an action with a file or set of files. Note that this
// can work with both the ANSI/lightbar or traditional UI.
//
// Parameters:
//  pFilenames: An array of filenames (as strings), or a string containing a filename
//  pActionName: String - The name of the action to confirm
//  pDefaultYes: Boolean - True if the default is to be yes, or false if no
//
// Return value: Boolean - True if the user confirmed, or false if not
function confirmFileActionWithUser(pFilenames, pActionName, pDefaultYes)
{
	if (typeof(pFilenames) !== "object" && typeof(pFilenames) !== "string")
		return false;
	if (typeof(pActionName) !== "string")
		return false;

	var actionConfirmed = false;

	var numFilenames = 1;
	if (typeof(pFilenames) === "object")
		numFilenames = pFilenames.length;
	if (numFilenames < 1)
		return false;
	// If there is only 1 filename, then prompt the user near the bottom of the screen
	else if (numFilenames == 1)
	{
		var filename = (typeof(pFilenames) === "string" ? pFilenames : pFilenames[0]);
		if (gUseLightbarInterface && console.term_supports(USER_ANSI))
		{
			drawSeparatorLine(1, console.screen_rows-2, console.screen_columns-1);
			console.gotoxy(1, console.screen_rows-1);
			console.cleartoeol("\x01n");
			console.gotoxy(1, console.screen_rows-1);
		}
		var shortFilename = shortenFilename(filename, Math.floor(console.screen_columns/2), false);
		if (pDefaultYes)
			actionConfirmed = console.yesno(pActionName + " " + shortFilename);
		else
			actionConfirmed = !console.noyes(pActionName + " " + shortFilename);
		if (gUseLightbarInterface && console.term_supports(USER_ANSI))
		{
			// Refresh the main screen content, to erase the confirmation prompt
			refreshScreenMainContent(1, console.screen_rows-2, console.screen_columns, 2, true);
		}
	}
	else
	{
		if (gUseLightbarInterface && console.term_supports(USER_ANSI))
		{
			// Construct & draw a frame with the file list & display the frame to confirm with the
			// user
			var frameUpperLeftX = gFileListMenu.pos.x + 2;
			var frameUpperLeftY = gFileListMenu.pos.y + 2;
			//var frameWidth = gFileListMenu.size.width - 4;
			var frameWidth = console.screen_columns - 4;
			var frameHeight = 10;
			var frameTitle = pActionName + " files? (Y/N)";
			var additionalQuitKeys = "yYnN";
			var frameInnerWidth = frameWidth - 2; // Without borders; for filename lengths
			var fileListStr = "\x01n\x01w";
			for (var i = 0; i < pFilenames.length; ++i)
				fileListStr += shortenFilename(pFilenames[i], frameInnerWidth, false) + "\r\n";
			var lastUserInput = displayBorderedFrameAndDoInputLoop(frameUpperLeftX, frameUpperLeftY, frameWidth,
																   frameHeight, gColors.confirmFileActionWindowBorder,
																   frameTitle, gColors.confirmFileActionWindowWindowTitle,
																   fileListStr, additionalQuitKeys);
			actionConfirmed = (lastUserInput.toUpperCase() == "Y");
			// Refresh the main screen content, to erase the confirmation window
			refreshScreenMainContent(frameUpperLeftX, frameUpperLeftY, frameWidth, frameHeight, true);
		}
		else
		{
			// Traditional UI
			// Write the file list
			console.attributes = "NW";
			for (var i = 0; i < pFilenames.length; ++i)
				console.print(pFilenames[i] + "\r\n");
			// Confirm with the user
			if (pDefaultYes)
				actionConfirmed = console.yesno(pActionName + " files");
			else
				actionConfirmed = !console.noyes(pActionName + " files");
		}
	}

	return actionConfirmed;
}


// Reads the configuration file and sets the settings accordingly
function readConfigFile()
{
	var themeFilename = ""; // In case a theme filename is specified

	// Open the main configuration file.  First look for it in the sbbs/mods
	// directory, then sbbs/ctrl, then in the same directory as this script.
	var cfgFilename = "ddfilelister.cfg";
	var cfgFilenameFullPath = file_cfgname(system.mods_dir, cfgFilename);
	if (!file_exists(cfgFilenameFullPath))
		cfgFilenameFullPath = file_cfgname(system.ctrl_dir, cfgFilename);
	if (!file_exists(cfgFilenameFullPath))
		cfgFilenameFullPath = file_cfgname(js.exec_dir, cfgFilename);
	var cfgFile = new File(cfgFilenameFullPath);
	if (cfgFile.open("r"))
	{
		var settingsObj = cfgFile.iniGetObject();
		cfgFile.close();

		for (var prop in settingsObj)
		{
			var propUpper = prop.toUpperCase();
			if (propUpper == "INTERFACESTYLE")
			{
				if (typeof(settingsObj[prop]) === "string")
					gUseLightbarInterface = (settingsObj[prop].toUpperCase() == "LIGHTBAR");
			}
			else if (propUpper == "TRADITIONALUSESYNCSTOCK")
			{
				typeof(settingsObj[prop]) === "boolean"
					gTraditionalUseSyncStock = settingsObj[prop];
			}
			else if (propUpper == "SORTORDER")
			{
				if (typeof(settingsObj[prop]) === "string")
				{
					var valueUpper = settingsObj[prop].toUpperCase();
					if (valueUpper == "NATURAL")
						gFileSortOrder = FileBase.SORT.NATURAL;
					else if (valueUpper == "NAME_AI")
						gFileSortOrder = FileBase.SORT.NAME_AI;
					else if (valueUpper == "NAME_DI")
						gFileSortOrder = FileBase.SORT.NAME_DI;
					else if (valueUpper == "NAME_AS")
						gFileSortOrder = FileBase.SORT.NAME_AS;
					else if (valueUpper == "NAME_DS")
						gFileSortOrder = FileBase.SORT.NAME_DS;
					else if (valueUpper == "DATE_A")
						gFileSortOrder = FileBase.SORT.DATE_A;
					else if (valueUpper == "DATE_D")
						gFileSortOrder = FileBase.SORT.DATE_D;
					else if (valueUpper == "PER_DIR_CFG")
						gFileSortOrder = SORT_PER_DIR_CFG;
					else if (valueUpper == "ULTIME")
						gFileSortOrder = SORT_FL_ULTIME;
					else if (valueUpper == "DLTIME")
						gFileSortOrder = SORT_FL_DLTIME;
					else // Default
						gFileSortOrder = FileBase.SORT.NATURAL;
				}
			}
			else if (propUpper == "PAUSEAFTERVIEWINGFILE")
			{
				if (typeof(settingsObj[prop]) === "boolean")
					gPauseAfterViewingFile = settingsObj[prop];
			}
			else if (propUpper == "BLANKNFILESLISTEDSTRIFLOADABLEMODULE")
			{
				if (typeof(settingsObj[prop]) === "boolean")
					gBlankNFilesListedStrIfLoadableModule = settingsObj[prop];
			}
			else if (propUpper == "THEMEFILENAME")
			{
				if (typeof(settingsObj[prop]) === "string")
				{
					// First look for the theme config file in the sbbs/mods
					// directory, then sbbs/ctrl, then the same directory as
					// this script.
					themeFilename = system.mods_dir + settingsObj[prop];
					if (!file_exists(themeFilename))
						themeFilename = system.ctrl_dir + settingsObj[prop];
					if (!file_exists(themeFilename))
						themeFilename = js.exec_dir + settingsObj[prop];
				}
			}
		}
	}
	else
	{
		// Was unable to read the configuration file.  Output a warning to the user
		// that defaults will be used and to notify the sysop.
		console.attributes = "N";
		console.crlf();
		console.print("\x01w\x01hUnable to open the configuration file: \x01y" + cfgFilename);
		console.crlf();
		console.print("\x01wDefault settings will be used.  Please notify the sysop.");
		mswait(2000);
	}
	
	// If a theme filename was specified, then read the colors & strings
	// from it.
	if (themeFilename.length > 0)
	{
		var onlySyncAttrsRegexWholeWord = new RegExp("^[\x01krgybmcw01234567hinq,;\.dtlasz]+$", 'i');
		var themeFile = new File(themeFilename);
		if (themeFile.open("r"))
		{
			var themeSettingsObj = themeFile.iniGetObject();
			themeFile.close();

			// Set any color values specified
			for (var prop in gColors)
			{
				if (themeSettingsObj.hasOwnProperty(prop))
				{
					// Trim leading & trailing spaces from the value when
					// setting a color.  Also, replace any instances of "\x01" or "\1"
					// with the Synchronet attribute control character.
					// Using toString() to ensure the color attributes are strings (in case the value is just a number)
					var value = trimSpaces(themeSettingsObj[prop].toString(), true, false, true).replace(/\\[xX]01/g, "\x01").replace(/\\1/g, "\x01");
					// If the value doesn't have any control characters, then add the control character
					// before attribute characters
					if (!/\x01/.test(value))
						value = attrCodeStr(value);
					if (onlySyncAttrsRegexWholeWord.test(value))
						gColors[prop] = value;
				}
			}
		}
		else
		{
			// Was unable to read the theme file.  Output a warning to the user
			// that defaults will be used and to notify the sysop.
			console.attributes = "N";
			console.crlf();
			console.print("\x01w\x01hUnable to open the theme file: \x01y" + themeFilename);
			console.crlf();
			console.print("\x01wDefault colors will be used.  Please notify the sysop.");
			mswait(2000);
		}
	}
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
// Return value: The string with whitespace trimmed
function trimSpaces(pString, pLeading, pMultiple, pTrailing)
{
	if (typeof(pString) !== "string")
		return "";

	var leading = (typeof(pLeading) === "boolean" ? pLeading : true);
	var multiple = (typeof(pMultiple) === "boolean" ? pMultiple : true);
	var trailing = (typeof(pTrailing) === "boolean" ? pTrailing : true);

	// To remove both leading & trailing spaces:
	//pString = pString.replace(/(^\s*)|(\s*$)/gi,"");

	var string = pString;
	if (leading)
		string = string.replace(/(^\s*)/gi,"");
	if (multiple)
		string = string.replace(/[ ]{2,}/gi," ");
	if (trailing)
		string = string.replace(/(\s*$)/gi,"");

	return string;
}

// Given a filename, this returns a filename shortened to a maximum length, preserving the
// filename extension.
//
// Parameters:
//  pFilename: The filename to shorten
//  pMaxLen: The maximum length of the shortened filename
//  pFillWidth: Boolean - Whether or not to fill the width specified by pMaxLen
//
// Return value: A string with the shortened filename
function shortenFilename(pFilename, pMaxLen, pFillWidth)
{
	if (typeof(pFilename) !== "string")
		return "";
	if (typeof(pMaxLen) !== "number" || pMaxLen < 1)
		return "";

	var shortenedFilename = ""; // Will contain the shortened filename
	// Get the filename extension.  And the way we shorten the filename
	// will depend on whether the filename actually has an extension or not.
	var filenameExt = file_getext(pFilename);
	if (typeof(filenameExt) === "string")
	{
		var filenameWithoutExt = file_getname(pFilename);
		var extIdx = filenameWithoutExt.indexOf(filenameExt);
		if (extIdx >= 0)
			filenameWithoutExt = filenameWithoutExt.substr(0, extIdx);

		var maxWithoutExtLen = pMaxLen - filenameExt.length;
		if (filenameWithoutExt.length > maxWithoutExtLen)
			filenameWithoutExt = filenameWithoutExt.substr(0, maxWithoutExtLen);

		var fillWidth = (typeof(pFillWidth) === "boolean" ? pFillWidth : false);
		if (fillWidth)
			shortenedFilename = format("%-" + maxWithoutExtLen + "s%s", filenameWithoutExt, filenameExt);
		else
			shortenedFilename = filenameWithoutExt + filenameExt;
	}
	else
		shortenedFilename = pFilename.substr(0, pMaxLen);
	return shortenedFilename;
}


// Parses command-line arguments, where each argument in the given array is in
// the format -arg=val.  If the value is the string "true" or "false", then the
// value will be a boolean.  Otherwise, the value will be a string.
//
// Parameters:
//  argv: An array of strings containing values in the format -arg=val
//
// Return value: Boolean: Whether or not this script was run as a loadable
//                        module (depending on the arguments used).
function parseArgs(argv)
{
	// Default program options
	gScriptMode = MODE_LIST_DIR;

	// Sanity checking for argv - Make sure it's an array
	if ((typeof(argv) != "object") || (typeof(argv.length) != "number"))
		return false;

	// This script accepts arguments in -arg=val format; See if there are any of those.
	// If so, process arguments with that assumption; otherwise, we'll check for args
	// passed when Synchronet runs this as a loadable module.
	var scriptRanAsLoadableModule = false;
	var anySettingEqualsVal = false;
	for (var i = 0; i < argv.length && !anySettingEqualsVal; ++i)
		anySettingEqualsVal = (typeof(argv[i]) === "string" && argv[i].length > 0 && argv[i].charAt(0) == "-" && argv[i].indexOf("=") > 1);
	if (anySettingEqualsVal)
	{
		// Go through argv looking for strings in the format -arg=val and parse them
		// into objects in the argVals array.
		var equalsIdx = 0;
		var argName = "";
		var argVal = "";
		var argValUpper = ""; // For case-insensitive matching
		for (var i = 0; i < argv.length; ++i)
		{
			// We're looking for strings that start with "-", except strings that are
			// only "-".
			if (typeof(argv[i]) !== "string" || argv[i].length == 0 || argv[i].charAt(0) != "-" || argv[i] == "-")
				continue;

			// Look for an = and if found, split the string on the =
			equalsIdx = argv[i].indexOf("=");
			// If a = is found, then split on it and add the argument name & value
			// to the array.  Otherwise (if the = is not found), then treat the
			// argument as a boolean and set it to true (to enable an option).
			if (equalsIdx > -1)
			{
				argName = argv[i].substring(1, equalsIdx).toUpperCase();
				argVal = argv[i].substr(equalsIdx+1);
				argValUpper = argVal.toUpperCase();
				if (argName === "MODE")
				{
					if (argValUpper === "SEARCH_FILENAME")
						gScriptMode = MODE_SEARCH_FILENAME;
					else if (argValUpper === "SEARCH_DESCRIPTION")
						gScriptMode = MODE_SEARCH_DESCRIPTION;
					else if (argValUpper === "NEW_FILE_SEARCH")
						gScriptMode = MODE_NEW_FILE_SEARCH;
					else if (argValUpper === "LIST_CURDIR")
						gScriptMode = MODE_LIST_DIR;
				}
			}
			// Nothing to do if an = was not found
		}
	}
	else
	{
		// Check for arguments as if this was run by Synchronet as a loadable module
		// (for either Scan Dirs or List Files)
		//The 'S' option calls bbs.scan_dirs() which is expected to do the prompting for the string (filename/pattern) and then call filelist mod for each directory

		/*
		// Bits in mode for bbs.scan_dirs()
		// bbs.list_files() & bbs.list_file_info()
		//********************************************
		var FL_NONE			=0;			// No special behavior
		var FL_ULTIME		=(1<<0);	// List files by upload time
		var FL_DLTIME		=(1<<1);	// List files by download time
		var FL_NO_HDR		=(1<<2);	// Don't list directory header
		var FL_FINDDESC		=(1<<3);	// Find text in description
		var FL_EXFIND		=(1<<4);	// Find text in description - extended info
		var FL_VIEW			=(1<<5);	// View ZIP/ARC/GIF etc. info
		*/

		// There must be either 2 or 3 arguments
		if (argv.length < 2)
			return false;
		// If gBlankNFilesListedStrIfLoadableModule is true, replace the "# Files Listed" text with an
		// empty string so that it won't be displayed after exit
		if (gBlankNFilesListedStrIfLoadableModule)
		{
			bbs.replace_text(NFilesListed, "");
			js.on_exit("bbs.revert_text(NFilesListed);");
		}
		// The 2nd argument is the mode/behavior bits in either case
		var FLBehavior = parseInt(argv[1]);
		if (isNaN(FLBehavior))
			return false;
		else
			gListBehavior = FLBehavior;
		// If the 'no header' option was passed, then disable that
		if (Boolean(gListBehavior & FL_NO_HDR))
			gListBehavior &= ~FL_NO_HDR;
		scriptRanAsLoadableModule = true;

		// Default gScriptmode to MODE_LIST_DIR; for FLBehavior as FL_NONE, no special behavior
		gScriptMode = MODE_LIST_DIR;
		
		// 2 args - Scanning/searching
		if (argv.length == 2)
		{
			// When used as the Scan Dirs lodable module (no longer recommended):
			if (argv[0] == "0" || argv[0] == "1")
			{
				// - 0: Bool (scanning all directories): 0/1
				// - 1: FL_ mode value
				gScanAllDirs = (argv[0] == "1");
			}
			// When used as the List Files loadable module: First arg is a directory internal code
			// and 2nd arg is list mode (FL_ mode value; see comments above starting with FL_NONE)
			else if (/^[^\s]+$/.test(argv[0]))
			{
				// - 0: Directory internal code
				if (typeof(file_area.dir[argv[0]]) === "object")
					gDirCode = argv[0];
			}

			// If FL_ULTIME is set, then set the script mode to new file search
			if (Boolean(FLBehavior & FL_ULTIME))
				gScriptMode = MODE_NEW_FILE_SEARCH; // List files by upload time
			// If FL_FINDDESC is set, then set the script mode to search by description
			else if (Boolean(FLBehavior & FL_FINDDESC) || Boolean(FLBehavior & FL_EXFIND))
				gScriptMode = MODE_SEARCH_DESCRIPTION;
			if (Boolean(FLBehavior & FL_VIEW))
			{
				// View ZIP/ARC/GIF etc. info
				// TODO: Not sure what to do with this
			}
		}
		// 3 args - Internal code, mode, filespec/description keyword
		else if (argv.length >= 3) //==3
		{
			// - 0: Directory internal code
			// - 1: FL_ mode value
			// - 2: Filespec (i.e., *, *.zip, etc.) or keyword for description search
			// Filename search: os2xferp,4,*.zip:
			if (!file_area.dir.hasOwnProperty(argv[0]))
				return false;
			gDirCode = argv[0];
			if (Boolean(FLBehavior & FL_FINDDESC) || Boolean(FLBehavior & FL_EXFIND))
			{
				gScriptMode = MODE_SEARCH_DESCRIPTION;
				// The keyword could have spaces in it, so look at argv[2] and the remainder of the
				// command-line arguments and combine those into gDescKeyword
				gDescKeyword = "";
				for (var i = 2; i < argv.length; ++i)
				{
					if (argv[i].length > 0)
					{
						if (gDescKeyword.length > 0)
							gDescKeyword += " ";
						gDescKeyword += argv[i];
					}
				}
			}
			else if (Boolean(FLBehavior & FL_NO_HDR))
			{
				// Filename search
				gFilespec = argv[2];
			}
			else if (Boolean(FLBehavior & FL_VIEW))
			{
				// View ZIP/ARC/GIF etc. info
				// TODO: Not sure what to do with this
			}
		}

		// Options that apply to both searching and listing
		if (Boolean(FLBehavior & FL_ULTIME))
			gFileSortOrder = SORT_FL_ULTIME;
		else if (Boolean(FLBehavior & FL_DLTIME))
			gFileSortOrder = SORT_FL_DLTIME;

	}

	return scriptRanAsLoadableModule;
}

// Populates the file list (gFileList).
//
// Parameters:
//  pSearchMode: The search mode
//
// Return value: An obmect with the following properties:
//               exitNow: Boolean: Whether or not this script should exit after calling this function
//               exitCode: The exit code to use if needing to exit after calling this function
function populateFileList(pSearchMode)
{
	var retObj = {
		exitNow: false,
		exitCode: 0
	};
	
	var dirErrors = [];
	var allSameDir = true;

	// Do the things for list or search, depending on the specified mode
	if (pSearchMode == MODE_LIST_DIR) // This is the default
	{
		var filebase = new FileBase(gDirCode);
		if (filebase.open())
		{
			// If there are no files in the filebase, then say so and exit now.
			if (filebase.files == 0)
			{
				filebase.close();
				var libIdx = file_area.dir[gDirCode].lib_index;
				console.crlf();
				console.print("\x01n\x01cThere are no files in \x01h" + file_area.lib_list[libIdx].description + "\x01n\x01c - \x01h" +
							  file_area.dir[gDirCode].description + "\x01n");
				console.crlf();
				console.pause();
				retObj.exitNow = true;
				retObj.exitCode = 0;
				return retObj;
			}

			// Get a list of file data
			var fileDetail = (extendedDescEnabled() ? FileBase.DETAIL.EXTENDED : FileBase.DETAIL.NORM);
			if (gFileSortOrder == SORT_FL_ULTIME || gFileSortOrder == SORT_FL_DLTIME)
			{
				gFileList = filebase.get_list(gFilespec, fileDetail, 0, true, FileBase.SORT.NATURAL);
				if (gFileSortOrder == SORT_FL_ULTIME)
					gFileList.sort(fileInfoSortULTime);
				else if (gFileSortOrder == SORT_FL_DLTIME)
					gFileList.sort(fileInfoSortDLTime);
			}
			else if (gFileSortOrder == SORT_PER_DIR_CFG)
				gFileList = filebase.get_list(gFilespec, fileDetail, 0, true, file_area.dir[gDirCode].sort);
			else
				gFileList = filebase.get_list(gFilespec, fileDetail, 0, true, gFileSortOrder);
			filebase.close();
			// Add a dirCode property to the file metadata objects (for consistency,
			// as file search results may contain files from multiple directories).
			// Also, if the metadata objects have an extdesc, remove any trailing CRLF
			// from the end.
			for (var i = 0; i < gFileList.length; ++i)
			{
				gFileList[i].dirCode = gDirCode;
				if (gFileList[i].hasOwnProperty("extdesc") && /\r\n$/.test(gFileList[i].extdesc))
				{
					gFileList[i].extdesc = gFileList[i].extdesc.substr(0, gFileList[i].extdesc.length-2);
					// Fix line endings if necessary
					gFileList[i].extdesc = lfexpand(gFileList[i].extdesc);
				}
			}
		}
		else
		{
			console.crlf();
			console.print("\x01n\x01h\x01yUnable to open \x01w" + file_area.dir[gDirCode].description + "\x01n");
			console.crlf();
			console.pause();
			retObj.exitNow = true;
			retObj.exitCode = 1;
			return retObj;
		}
	}
	else if (pSearchMode == MODE_SEARCH_FILENAME)
	{
		var lastDirCode = "";

		// If not searching all already, prompt the user for directory, library, or all
		var validInputOptions = "DLA";
		var userInputDLA = "";
		if (gScanAllDirs)
			userInputDLA = "A";
		else
		{
			console.attributes = "N";
			console.crlf();
			console.mnemonics(bbs.text(bbs.text.DirLibOrAll));
			userInputDLA = console.getkeys(validInputOptions, -1, K_UPPER);
		}
		var userFilespec = "";
		if (userInputDLA.length > 0 && validInputOptions.indexOf(userInputDLA) > -1)
		{
			// Prompt the user for a filespec to search for
			console.mnemonics(bbs.text(bbs.text.FileSpecStarDotStar));
			userFilespec = console.getstr();
			if (userFilespec.length == 0)
				userFilespec = "*"; // Should this be *.*?
			console.print("Searching....");
		}
		var searchRetObj = searchDirGroupOrAll(userInputDLA, function(pDirCode) {
			return searchDirWithFilespec(pDirCode, userFilespec);
		});
		allSameDir = searchRetObj.allSameDir;
		for (var i = 0; i < searchRetObj.errors.length; ++i)
			dirErrors.push(searchRetObj.errors[i]);
	}
	else if (pSearchMode == MODE_SEARCH_DESCRIPTION)
	{
		var lastDirCode = "";

		// If gDescKeyword hasn't been specified (i.e., via use as a loadable module), then if
		// not saerching all already, prompt the user for directory, library, or all
		if (gDescKeyword == "")
		{
			var validInputOptions = "DLA";
			var userInputDLA = "";
			if (gScanAllDirs)
				userInputDLA = "A";
			else
			{
				console.attributes = "N";
				console.crlf();
				//console.print("\r\n\x01c\x01hFind Text in File Descriptions (no wildcards)\x01n\r\n");
				console.mnemonics(bbs.text(bbs.text.DirLibOrAll));
				console.attributes = "N";
				userInputDLA = console.getkeys(validInputOptions, -1, K_UPPER);
			}
			var searchDescription = "";
			if (userInputDLA.length > 0 && validInputOptions.indexOf(userInputDLA) > -1)
			{
				// Prompt the user for a description to search for
				console.mnemonics(bbs.text(bbs.text.SearchStringPrompt));
				searchDescription = console.getstr(40, K_LINE|K_UPPER);
				if (searchDescription.length > 0)
					console.print("Searching....");
				else
				{
					retObj.exitNow = true;
					retObj.exitCode = 0;
					return retObj;
				}
			}
			var searchRetObj = searchDirGroupOrAll(userInputDLA, function(pDirCode) {
				return searchDirWithDescUpper(pDirCode, searchDescription);
			});
			allSameDir = searchRetObj.allSameDir;
			for (var i = 0; i < searchRetObj.errors.length; ++i)
				dirErrors.push(searchRetObj.errors[i]);
		}
		else
		{
			if (gDirCode.length > 0 && gDescKeyword.length > 0)
			{
				var searchRetObj = searchDirWithDescUpper(gDirCode, gDescKeyword.toUpperCase());
				//searchRetObj.foundFiles
				for (var i = 0; i < searchRetObj.dirErrors.length; ++i)
					dirErrors.push(searchRetObj.dirErrors[i]);
			}
		}
	}
	else if (pSearchMode == MODE_NEW_FILE_SEARCH)
	{
		// New file search
		var lastDirCode = "";

		// 2022-02-011 - Digital Man said:
		/*
		Upon logon to the terminal server, bbs.new_file_time and bbs.last_new_file_time
		are set to the current user.new_file_time value.

		A new file scan displays files uploaded (added/imported) since the current
		bbs.new_file_time value. The bbs.new_file_time value can be manipulated by the
		user, e.g. they want to review files that have been uploaded over the past
		month or whatever.

		When the user executes a new file scan, bbs.last_new_file_time is updated to
		the current time (this happens automatically when using the built-in file
		listing logic). The bbs.last_new_file_time isn't actually used for anything
		until the user logs-off and its value is then copied to the user.new_file_time
		to be used for the next logon (this copy/sync of the last_new_file_time ->
		user.new_file_time is built-in to the BBS's logout logci and nothing a script
		would need to do).

		This scheme insures that a user will never "miss" the display of new files on
		the BBS and that they would not normally see the same/repeat "new" files
		between successive sessions.
		*/

		// If not searching all dirs already, prompt the user for directory, library, or all
		var userInputDLA = "";
		if (gScanAllDirs)
			userInputDLA = "A";
		// If running as a loadable module, the sub-board would be specified
		else if (gRunningAsLoadableModule)
			userInputDLA = "D";
		else
		{
			console.attributes = "N";
			console.crlf();
			console.mnemonics(bbs.text(bbs.text.DirLibOrAll));
			var validInputOptions = "DLA";
			userInputDLA = console.getkeys(validInputOptions, -1, K_UPPER);
			console.attributes = "N";
			console.crlf();
		}
		// Synchronet itself seems to print the "Searching for files uploaded after..." string, so
		// we don't have to print it here
		/*
		if (userInputDLA == "D" || userInputDLA == "L" || userInputDLA == "A")
		{
			console.print("\x01n\x01cSearching for files uploaded after \x01h" + system.timestr(bbs.new_file_time) + "\x01n");
			console.crlf();
		}
		*/
		var searchRetObj = searchDirGroupOrAll(userInputDLA, function(pDirCode) {
			var searchDirNewFilesRetObj = searchDirNewFiles(pDirCode, bbs.new_file_time);
			// If searching a single directory and no files were found, then say so
			if (userInputDLA == "D" && !searchDirNewFilesRetObj.foundFiles)
			{
				var areaFullDesc = file_area.dir[pDirCode].lib_name + ": " + file_area.dir[pDirCode].description;
				printf("\x01n\x01cNo new files found in \x01h%s\x01n\r\n", areaFullDesc);
			}
			return searchDirNewFilesRetObj;
		});
		// Now bbs.last_new_file_time needs to be updated with the current time
		bbs.last_new_file_time = time();
		// user.new_file_time will be updated with the value of bbs.last_new_file_time
		// when the user logs off.
		allSameDir = searchRetObj.allSameDir;
		for (var i = 0; i < searchRetObj.errors.length; ++i)
			dirErrors.push(searchRetObj.errors[i]);
	}
	else
	{
		retObj.exitNow = true;
		retObj.exitCode = 1;
		return retObj;
	}

	if (pSearchMode != MODE_LIST_DIR)
		gFileList.allSameDir = allSameDir;

	if (dirErrors.length > 0)
	{
		console.print("\x01n\x01y\x01h");
		for (var i = 0; i < dirErrors.length; ++i)
		{
			console.print(dirErrors[i]);
			console.crlf();
		}
		console.attributes = "N";
		console.pause();
		retObj.exitNow = true;
		retObj.exitCode = 1;
		return retObj;
	}

	// Figure out the longest filename in the list.
	var longestFilenameLen = 0;
	for (var i = 0; i < gFileList.length; ++i)
	{
		if (gFileList[i].name.length > longestFilenameLen)
			longestFilenameLen = gFileList[i].name.length;
	}
	var displayFilenameLen = gListIdxes.filenameEnd - gListIdxes.filenameStart + 1;
	// If the user has extended descriptions enabled, then allow 47 characters for the
	// description and adjust the filename length accordingly
	gListIdxes.descriptionEnd = console.screen_columns - 1; // Leave 1 character remaining on the screen
	if (extendedDescEnabled())
	{
		gListIdxes.descriptionStart = gListIdxes.descriptionEnd - 47 + 1;
		gListIdxes.fileSizeEnd = gListIdxes.descriptionStart;
		gListIdxes.fileSizeStart = gListIdxes.fileSizeEnd - 7;
		gListIdxes.filenameEnd = gListIdxes.fileSizeStart;
	}
	// If not displaying extended descriptions, then if the longest filename
	// is longer than the current display filename length and the user's
	// terminal is at least 100 columns wide, then increase the filename length
	// for the list by 20;
	else if (longestFilenameLen > displayFilenameLen && console.screen_columns >= 100)
	{
		gListIdxes.filenameEnd += 20;
		gListIdxes.fileSizeStart = gListIdxes.filenameEnd;
		gListIdxes.fileSizeEnd = gListIdxes.fileSizeStart + 7;
		gListIdxes.descriptionStart = gListIdxes.fileSizeEnd;
	}

	return retObj;
}

// Searches files in a directory by filespec.
//
// Parameters:
//  pDirCode: The internal code of a directory to search
//  pFilespec: A filespec describing files to search for
//
// Return value: An object with the following properties:
//               foundFiles: Boolean - Whether or not any files were found
//               dirErrors: An array of strings that will be populated with any errors that occur
function searchDirWithFilespec(pDirCode, pFilespec)
{
	var retObj = {
		foundFiles: false,
		dirErrors: []
	};
	if (typeof(pDirCode) !== "string" || pDirCode.length == 0 || typeof(pFilespec) !== "string")
		return retObj;

	var filebase = new FileBase(pDirCode);
	if (filebase.open())
	{
		var fileDetail = (extendedDescEnabled() ? FileBase.DETAIL.EXTENDED : FileBase.DETAIL.NORM);
		var fileList;
		if (gFileSortOrder == SORT_FL_ULTIME || gFileSortOrder == SORT_FL_DLTIME)
		{
			fileList = filebase.get_list(pFilespec, fileDetail, 0, true, FileBase.SORT.NATURAL);
			if (gFileSortOrder == SORT_FL_ULTIME)
				fileList.sort(fileInfoSortULTime);
			else if (gFileSortOrder == SORT_FL_DLTIME)
				fileList.sort(fileInfoSortDLTime);
		}
		else if (gFileSortOrder == SORT_PER_DIR_CFG)
			fileList = filebase.get_list(pFilespec, fileDetail, 0, true, file_area.dir[pDirCode].sort);
		else
			fileList = filebase.get_list(pFilespec, fileDetail, 0, true, gFileSortOrder);
		retObj.foundFiles = (fileList.length > 0);
		filebase.close();
		for (var i = 0; i < fileList.length; ++i)
		{
			// Fix line endings in extdesc if necessary
			if (fileList[i].hasOwnProperty("extdesc") && /\r\n$/.test(fileList[i].extdesc))
				fileList[i].extdesc = lfexpand(fileList[i].extdesc);
			fileList[i].dirCode = pDirCode;
			gFileList.push(fileList[i]);
		}
	}
	else
	{
		var libAndDirDesc = file_area.lib_list[libIdx].description + " - "
						  + file_area.lib_list[libIdx].dir_list[dirIdx].description + " ("
						  + file_area.lib_list[libIdx].dir_list[dirIdx].code + ")";
		retObj.dirErrors.push("Failed to open " + libAndDirDesc);
	}
	return retObj;
}

// Searches files in a directory by description.
//
// Parameters:
//  pDirCode: The internal code of a directory to search
//  pDescUpper: The description to search for.  This is assumed to be uppercase.
//
// Return value: An object with the following properties:
//               foundFiles: Boolean - Whether or not any files were found
//               dirErrors: An array of strings that will be populated with any errors that occur
function searchDirWithDescUpper(pDirCode, pDescUpper)
{
	var retObj = {
		foundFiles: false,
		dirErrors: []
	};
	if (typeof(pDirCode) !== "string" || pDirCode.length == 0 || typeof(pDescUpper) !== "string" || pDescUpper.length == 0)
		return retObj;

	var filebase = new FileBase(pDirCode);
	if (filebase.open())
	{
		//var fileList = filebase.get_list(gFilespec, FileBase.DETAIL.EXTENDED, 0, true, gFileSortOrder);
		var fileList;
		if (gFileSortOrder == SORT_FL_ULTIME || gFileSortOrder == SORT_FL_DLTIME)
		{
			fileList = filebase.get_list(gFilespec, FileBase.DETAIL.EXTENDED, 0, true, FileBase.SORT.NATURAL);
			if (gFileSortOrder == SORT_FL_ULTIME)
				fileList.sort(fileInfoSortULTime);
			else if (gFileSortOrder == SORT_FL_DLTIME)
				fileList.sort(fileInfoSortDLTime);
		}
		else if (gFileSortOrder == SORT_PER_DIR_CFG)
			fileList = filebase.get_list(gFilespec, FileBase.DETAIL.EXTENDED, 0, true, file_area.dir[pDirCode].sort);
		else
			fileList = filebase.get_list(gFilespec, FileBase.DETAIL.EXTENDED, 0, true, gFileSortOrder);
		filebase.close();
		for (var i = 0; i < fileList.length; ++i)
		{
			// Search both 'desc' and 'extdesc', if available in the file metadata object
			var fileIsMatch = false;
			if (fileList[i].hasOwnProperty("desc"))
			{
				var fileDesc = strip_ctrl(fileList[i].desc).toUpperCase();
				fileIsMatch = (fileDesc.indexOf(pDescUpper) > -1);
			}
			if (!fileIsMatch && fileList[i].hasOwnProperty("extdesc"))
			{
				// Fix line endings if necessary
				fileList[i].extdesc = lfexpand(fileList[i].extdesc);
				var descLines = fileList[i].extdesc.split("\r\n");
				var fileDesc = strip_ctrl(descLines.join(" ")).toUpperCase();
				fileIsMatch = (fileDesc.indexOf(pDescUpper) > -1);
			}
			if (fileIsMatch)
			{
				retObj.foundFiles = true;
				fileList[i].dirCode = pDirCode;
				gFileList.push(fileList[i]);
			}
		}
	}
	else
	{
		var libAndDirDesc = file_area.lib_list[libIdx].description + " - "
						  + file_area.lib_list[libIdx].dir_list[dirIdx].description + " ("
						  + file_area.lib_list[libIdx].dir_list[dirIdx].code + ")";
		retObj.dirErrors.push("Failed to open " + libAndDirDesc);
	}
	return retObj;
}

// Searches files in a directory that were added later than a given time.
//
// Parameters:
//  pDirCode: The internal code of a directory to search
//  pSinceTime: The time after which to look for newer files
//
// Return value: An object with the following properties:
//               foundFiles: Boolean - Whether or not any files were found
//               dirErrors: An array of strings that will be populated with any errors that occur
function searchDirNewFiles(pDirCode, pSinceTime)
{
	var retObj = {
		foundFiles: false,
		dirErrors: []
	};
	if (typeof(pDirCode) !== "string" || pDirCode.length == 0 || typeof(pSinceTime) !== "number")
		return retObj;

	var filebase = new FileBase(pDirCode);
	if (filebase.open())
	{
		var fileDetail = (extendedDescEnabled() ? FileBase.DETAIL.EXTENDED : FileBase.DETAIL.NORM);
		var fileList;
		if (gFileSortOrder == SORT_FL_ULTIME || gFileSortOrder == SORT_FL_DLTIME)
		{
			fileList = filebase.get_list(gFilespec, fileDetail, 0, true, FileBase.SORT.NATURAL);
			if (gFileSortOrder == SORT_FL_ULTIME)
				fileList.sort(fileInfoSortULTime);
			else if (gFileSortOrder == SORT_FL_DLTIME)
				fileList.sort(fileInfoSortDLTime);
		}
		else if (gFileSortOrder == SORT_PER_DIR_CFG)
			fileList = filebase.get_list(gFilespec, fileDetail, 0, true, file_area.dir[pDirCode].sort);
		else
			fileList = filebase.get_list(gFilespec, fileDetail, 0, true, gFileSortOrder);
		filebase.close();
		for (var i = 0; i < fileList.length; ++i)
		{
			if (fileList[i].added >= pSinceTime)
			{
				retObj.foundFiles = true;
				// Fix line endings in extdesc if necessary
				if (fileList[i].hasOwnProperty("extdesc"))
					fileList[i].extdesc = lfexpand(fileList[i].extdesc);
				fileList[i].dirCode = pDirCode;
				gFileList.push(fileList[i]);
			}
		}
	}
	else
	{
		var libAndDirDesc = file_area.lib_list[libIdx].description + " - "
						  + file_area.lib_list[libIdx].dir_list[dirIdx].description + " ("
						  + file_area.lib_list[libIdx].dir_list[dirIdx].code + ")";
		retObj.dirErrors.push("Failed to open " + libAndDirDesc);
	}
	return retObj;
}

// Searches either the user's current directory, library, or all dirs in all
// libraries, using a search function that populages gFileList.
//
// Parameters:
//  pSearchOption: A string that specifies "D" for directory, "L" for library, or "A" for all
//  pDirSearchFn: A search function that searches a file directory for criteria and populates gFileList.
//                This function must return an object with the following properties:
//                foundFiles: Boolean - Whether any files were found
//                dirErrors: An array containing strings for any errors found
//
// Return value: An object with the following properties:
//               allSameDir: Boolean - Whethre or not all files found are in the same directory
//               errors: An array containing strings for any errors found
function searchDirGroupOrAll(pSearchOption, pDirSearchFn)
{
	var retObj = {
		allSameDir: true,
		errors: []
	};

	if (typeof(pSearchOption) !== "string")
	{
		retObj.errors.push("Invalid search option");
		return retObj;
	}
	if (typeof(pDirSearchFn) !== "function")
	{
		retObj.errors.push("No search function");
		return retObj;
	}

	var searchOptionUpper = pSearchOption.toUpperCase();
	if (searchOptionUpper == "D")
	{
		// Current directory
		var searchRetObj = pDirSearchFn(gDirCode);
		retObj.allSameDir = true;
		for (var i = 0; i < searchRetObj.dirErrors.length; ++i)
			retObj.errors.push(searchRetObj.dirErrors[i]);
	}
	else if (searchOptionUpper == "L")
	{
		// Current file library
		var libIdx = file_area.dir[gDirCode].lib_index;
		for (var dirIdx = 0; dirIdx < file_area.lib_list[libIdx].dir_list.length; ++dirIdx)
		{
			if (file_area.lib_list[libIdx].dir_list[dirIdx].can_access) // And can_download?
			{
				if (gSearchVerbose)
				{
					console.print(" " + file_area.lib_list[libIdx].dir_list[dirIdx].description + "..");
					console.crlf();
				}
				var lastDirCode = (gFileList.length > 0 ? gFileList[gFileList.length-1].dirCode : "");
				var dirCode = file_area.lib_list[libIdx].dir_list[dirIdx].code;
				var searchRetObj = pDirSearchFn(dirCode);
				if (retObj.allSameDir && searchRetObj.foundFiles && lastDirCode.length > 0)
					retObj.allSameDir = (dirCode == lastDirCode);
				for (var i = 0; i < searchRetObj.dirErrors.length; ++i)
					retObj.errors.push(searchRetObj.dirErrors[i]);
			}
		}
	}
	else if (searchOptionUpper == "A")
	{
		// All file libraries & directories
		for (var libIdx = 0; libIdx < file_area.lib_list.length; ++libIdx)
		{
			if (gSearchVerbose)
			{
				console.print("Searching " + file_area.lib_list[libIdx].description + "..");
				console.crlf();
			}
			for (var dirIdx = 0; dirIdx < file_area.lib_list[libIdx].dir_list.length; ++dirIdx)
			{
				if (file_area.lib_list[libIdx].dir_list[dirIdx].can_access) // And can_download?
				{
					if (gSearchVerbose)
					{
						console.print(" " + file_area.lib_list[libIdx].dir_list[dirIdx].description + "..");
						console.crlf();
					}
					var lastDirCode = (gFileList.length > 0 ? gFileList[gFileList.length-1].dirCode : "");
					var dirCode = file_area.lib_list[libIdx].dir_list[dirIdx].code;
					var searchRetObj = pDirSearchFn(dirCode);
					if (retObj.allSameDir && searchRetObj.foundFiles && lastDirCode.length > 0)
						retObj.allSameDir = (dirCode == lastDirCode);
					for (var i = 0; i < searchRetObj.dirErrors.length; ++i)
						retObj.errors.push(searchRetObj.dirErrors[i]);
				}
			}
		}
	}
	else
	{
		//retObj.errors.push("Invalid search option" + (pSearchOption.length > 0 ? ": " + pSearchOption : ""));
		retObj.errors.push("Aborted");
	}

	return retObj;
}

// Returns whether the user has their extended file description setting enabled
// and if it can be supported in the user's terminal mode.  Extended descriptions
// will be displayed in the main screen if the user has that option enabled and
// the user's terminal is at least 80 columns wide.
function extendedDescEnabled()
{
	var userExtDescEnabled = ((user.settings & USER_EXTDESC) == USER_EXTDESC);
	// TODO: If the traditional (non-lightbar) is enabled and/or the user's terminal doesn't support ANSI, or
	// the user's terminal width is less than 80, this will cause the lister to not use extended file descriptions
	return userExtDescEnabled && console.screen_columns >= 80 && gUseLightbarInterface && console.term_supports(USER_ANSI);
}

// Displays a file's extended description on the main screen, next to the
// file list menu.  This is to be used when the user's extended file description
// option is enabled (where the menu would take up about the left half of
// the screen).
//
// Parameters:
//  pFileIdx: The index of the file metadata object in gFileList to use
//  pStartScreenRow: Optional - The screen row number to start printing, for partial
//                   screen refreshing (can be in the middle of the extended description)
//  pEndScreenRow: Optional - The screen row number to stop printing, for partial
//                   screen refreshing (can be in the middle of the extended description)
//  pMaxWidth: Optional - The maximum width to use for printing the description lines
function displayFileExtDescOnMainScreen(pFileIdx, pStartScreenRow, pEndScreenRow, pMaxWidth)
{
	if (typeof(pFileIdx) !== "number")
		return;
	if (pFileIdx < 0 || pFileIdx >= gFileList.length)
		return;

	// Get the file description from its metadata object
	var fileMetadata = gFileList[pFileIdx];
	var fileDesc = "";
	if (fileMetadata.hasOwnProperty("extdesc") && fileMetadata.extdesc.length > 0)
		fileDesc = fileMetadata.extdesc;
	else
		fileDesc = fileMetadata.desc;
	if (typeof(fileDesc) != "string")
		fileDesc = "";

	// This might be overkill, but just in case, convert any non-Synchronet
	// attribute codes to Synchronet attribute codes in the description.
	// This will help simplify getting substrings for formatting.  Then for
	// efficiency, put the converted description back into the metadata
	// object in the array so that it doesn't have to be converted again.
	if (!fileMetadata.hasOwnProperty("attrsConverted"))
	{
		fileDesc = convertAttrsToSyncPerSysCfg(fileDesc);
		fileMetadata.attrsConverted = true;
		if (fileMetadata.hasOwnProperty("extdesc"))
			fileMetadata.extdesc = fileDesc;
		else
			fileMetadata.desc = fileDesc;
	}

	// Calculate where to write the description on the screen
	//var startX = gFileListMenu.size.width + 1; // Assuming the file menu starts at the leftmost column
	var startX = gFileListMenu.pos.x + gFileListMenu.size.width;
	var maxDescLen = console.screen_columns - startX;
	if (typeof(pMaxWidth) === "number" && pMaxWidth >= 0 && pMaxWidth < maxDescLen)
		maxDescLen = pMaxWidth;
	// Go to the location on the screen and write the file description
	var formatStr = "%-" + maxDescLen + "s";
	// firstScreenRow is the first row on the screen where the extended description
	// should start at.  lastScreenRow is the last row (inclusive) to use for
	// printing the extended description
	var firstScreenRow = gNumHeaderLinesDisplayed + 1;
	var lastScreenRow = console.screen_rows - 1; // This is inclusive
	// screenRowForPrinting will be used for the actual screen row we're at while
	// printing the extended description lines
	var screenRowForPrinting = firstScreenRow;
	// If pStartScreenRow or pEndScreenRow are specified, then use
	// them to specify the start & end screen rows to actually print
	if (typeof(pStartScreenRow) === "number" && pStartScreenRow >= firstScreenRow && pStartScreenRow <= lastScreenRow)
		screenRowForPrinting = pStartScreenRow;
	if (typeof(pEndScreenRow) === "number" && pEndScreenRow > firstScreenRow && pStartScreenRow <= lastScreenRow)
		lastScreenRow = pEndScreenRow;
	var fileDescArray = fileDesc.split("\r\n");
	console.attributes = "N";
	// screenRowNum is to keep track of the row on the screen where the
	// description line would be placed, in case the start row is after that
	var screenRowNum = firstScreenRow;
	for (var i = 0; i < fileDescArray.length; ++i)
	{
		if (screenRowForPrinting > screenRowNum++)
			continue;
		// Note: substrWithAttrCodes() is defined in dd_lightbar_menu.js
		// Normally it would be handy to use printf() to print the text line:
		//printf(formatStr, substrWithAttrCodes(fileDescArray[i], 0, maxDescLen));
		// However, printf() doesn't account for attribute codes and thus may not
		// fill the rest of the width.  So, we do that manually.
		var descLine = substrWithAttrCodes(fileDescArray[i], 0, maxDescLen);
		var lineTextLength = console.strlen(descLine, P_AUTO_UTF8);
		if (lineTextLength > 0)
		{
			console.gotoxy(startX, screenRowForPrinting++);
			console.print(descLine);
			//var remainingLen = maxDescLen - lineTextLength + 1;
			var remainingLen = maxDescLen - lineTextLength;
			if (remainingLen > 0)
				printf("%-*s", remainingLen, "");
		}
		// Stop printing the description lines when we reach the last line on
		// the screen where we want to print.
		if (screenRowForPrinting > lastScreenRow)
			break;
	}
	// TODO: We shouldn't have to do this
	// For the remainder of the description area, it seems maxDescLen has to be 1 longer in order
	// to fill the whole text width
	/*
	if (maxDescLen == console.screen_columns - startX)
	{
		++maxDescLen;
		formatStr = "%-" + maxDescLen + "s";
	}
	*/
	// If there is room (and we're not below the file date already), show the file date on the next line.
	var belowDescriptionDate = (pStartScreenRow > gFileListMenu.pos.y + fileDescArray.length);
	if (!belowDescriptionDate && screenRowForPrinting <= lastScreenRow && fileMetadata.hasOwnProperty("time"))
	{
		console.attributes = "N";
		console.gotoxy(startX, screenRowForPrinting++);
		var dateStr = "Date: " + strftime("%Y-%m-%d", fileMetadata.time);
		//printf("%-" + maxDescLen + "s", dateStr.substr(0, maxDescLen));
		printf(formatStr, dateStr.substr(0, maxDescLen));
	}
	// Clear the rest of the lines to the bottom of the list area
	console.attributes = "N";
	while (screenRowForPrinting <= lastScreenRow)
	{
		console.gotoxy(startX, screenRowForPrinting++);
		printf(formatStr, "");
		//printf(formatStr, "!");
		//printf("%-*s", maxDescLen, "");
		//for (var i = 0; i < maxDescLen; ++i)
		//	console.print("!");
	}
}

// Refreshes (re-draws) the main content of the screen (file list menu,
// and extended description area if enabled).  The coordinates are absolute
// screen coordinates.
//
// Parameters:
//  pUpperLeftX: The X coordinate of the upper-left corner of the area to re-draw
//  pUpperLeftY: The Y coordinate of the upper-left corner of the area to re-draw
//  pWidth: The width of the area to re-draw
//  pHeight: The height of the area to re-draw
//  pSelectedItemIdxes: Optional: An object with selected item indexes for the file menu.
//                                If not passed, an empty object will be used.
//                      This can also be a boolean, and if true, will refresh the
//                      selected items on the file menu (with checkmarks) outside the
//                      given top & bottom screen rows.
function refreshScreenMainContent(pUpperLeftX, pUpperLeftY, pWidth, pHeight, pSelectedItemIdxes)
{
	// Have the file list menu partially re-draw itself if necessary
	var startXWithinFileList = (pUpperLeftX >= gFileListMenu.pos.x && pUpperLeftX < gFileListMenu.pos.x + gFileListMenu.size.width);
	var startYWithinFileList = (pUpperLeftY >= gFileListMenu.pos.y && pUpperLeftY < gFileListMenu.pos.y + gFileListMenu.size.height);
	if (startXWithinFileList && startYWithinFileList)
	{
		var selectedItemIdxesIsValid = (typeof(pSelectedItemIdxes) === "object");
		var selectedItemIdxes = (selectedItemIdxesIsValid ? pSelectedItemIdxes : {});
		gFileListMenu.DrawPartialAbs(pUpperLeftX, pUpperLeftY, pWidth, pHeight, selectedItemIdxes);
	}
	// If pSelectedItemIdxes is a bool instead of an object and is true,
	// refresh the selected items (with checkmarks) outside the top & bottom
	// lines on the file menu
	if (!selectedItemIdxesIsValid && typeof(pSelectedItemIdxes) === "boolean" && pSelectedItemIdxes && gFileListMenu.numSelectedItemIndexes() > 0)
	{
		var bottomScreenRow = pUpperLeftY + pHeight - 1;
		for (var idx in gFileListMenu.selectedItemIndexes)
		{
			var idxNum = +idx;
			var itemScreenRow = gFileListMenu.ScreenRowForItem(idxNum);
			if (itemScreenRow == -1)
				continue;
			if (itemScreenRow < pUpperLeftY || itemScreenRow > bottomScreenRow)
			{
				var isSelected = (idxNum == gFileListMenu.selectedItemIdx);
				gFileListMenu.WriteItemAtItsLocation(idxNum, isSelected, false);
			}
		}
	}
	// If the user has extended descriptions enabled, then the file menu
	// is only taking up about half the screen on the left, and we'll also
	// have to refresh the description area.
	if (extendedDescEnabled())
	{
		var fileMenuRightX = gFileListMenu.pos.x + gFileListMenu.size.width - 1;
		//var width = pWidth - (fileMenuRightX - pUpperLeftX + 1);
		var width = pWidth - (fileMenuRightX - pUpperLeftX);
		if (width > 0)
		{
			var firstRow = pUpperLeftY;
			// The last row is inclusive.  It seems like there might be an off-by-1
			// problem here?  I thought 1 would need to be subtracted from lastRow
			var lastRow = pUpperLeftY + pHeight;
			// We don't want to overwrite the last row on the screen, since that's
			// used for the command bar
			if (lastRow == console.screen_rows)
				--lastRow;
			displayFileExtDescOnMainScreen(gFileListMenu.selectedItemIdx, firstRow, lastRow, width);
		}
	}
}

// Returns whether or not the lister is doing a file search
function isDoingFileSearch()
{
	return (gScriptMode == MODE_SEARCH_FILENAME || gScriptMode == MODE_SEARCH_DESCRIPTION || gScriptMode == MODE_NEW_FILE_SEARCH);
}

// Custom file info sort function for upload time
function fileInfoSortULTime(pA, pB)
{
	if (pA.hasOwnProperty("added") && pB.hasOwnProperty("added"))
	{
		if (pA.added < pB.added)
			return -1;
		else if (pA.added > pB.added)
			return 1;
		else
			return 0;
	}
	else
	{
		if (pA.time < pB.time)
			return -1;
		else if (pA.time > pB.time)
			return 1;
		else
			return 0;
	}
}

// Custom file info sort function for download time
function fileInfoSortDLTime(pA, pB)
{
	if (pA.hasOwnProperty("last_downloaded") && pB.hasOwnProperty("last_downloaded"))
	{
		if (pA.last_downloaded < pB.last_downloaded)
			return -1;
		else if (pA.last_downloaded > pB.last_downloaded)
			return 1;
		else
			return 0;
	}
	else
		return 0;
}

// Returns whether or not a user can download from a file directory, and writes an error if not.
//
// Parameters:
//  pDirCode: The internal directory code of a file directory
//
// Return value: Boolean - Whether or not the user can download from the file directory given
function userCanDownloadFromFileArea_ShowErrorIfNot(pDirCode)
{
	var userCanDownload = file_area.dir[pDirCode].can_download;
	if (!userCanDownload)
	{
		// The user doesn't have permission to download from this directory
		//file_area.dir[pDirCode].name
		var areaFullDesc = file_area.dir[pDirCode].lib_name + ": " + file_area.dir[pDirCode].description;
		areaFullDesc = word_wrap(areaFullDesc, console.screen_columns-1, areaFullDesc.length).replace(/\r|\n/g, "\r\n");
		while (areaFullDesc.lastIndexOf("\r\n") == areaFullDesc.length-2)
			areaFullDesc = areaFullDesc.substr(0, areaFullDesc.length-2);
		console.crlf();
		console.print(areaFullDesc);
		console.crlf();
		console.mnemonics(bbs.text(bbs.text.CantDownloadFromDir));
		console.crlf();
		console.pause();
	}
	return userCanDownload;
}

// Given a string of attribute characters, this function inserts the control code
// in front of each attribute character and returns the new string.
//
// Parameters:
//  pAttrCodeCharStr: A string of attribute characters (i.e., "YH" for yellow high)
//
// Return value: A string with the control character inserted in front of the attribute characters
function attrCodeStr(pAttrCodeCharStr)
{
	if (typeof(pAttrCodeCharStr) !== "string")
		return "";

	var str = "";
	// See this page for Synchronet color attribute codes:
	// http://wiki.synchro.net/custom:ctrl-a_codes
	for (var i = 0; i < pAttrCodeCharStr.length; ++i)
	{
		var currentChar = pAttrCodeCharStr.charAt(i);
		if (/[krgybmcwKRGYBMCWHhIiEeFfNn01234567]/.test(currentChar))
			str += "\x01" + currentChar;
	}
	return str;
}

// Returns the number of lines to be displayed for all file information
// (accounting for multi-line descriptions if multi-line descriptions are enabled)
//
// Parameters: 
//  pFileList: An array of file metadata objects
//
// Return value: The total number of lines to be displayed for all files (accounting for
//               possible multi-line descriptions if enabled)
function numFileInfoLines(pFileList)
{
	if ((user.settings & USER_EXTDESC) == USER_EXTDESC)
	{
		var totalNumLines = 0;
		for (var i = 0; i < pFileList; ++i)
			totalNumLines += getExtdFileDescArray(pFileList, i).length;
		return totalNumLines;
	}
	else
		return pFileList.length;
}

// Gets an array of formatted strings with file information for the traditional UI,
// with a number before the file information (for a numbered list). If extended
// descriptions are not enabled, there will only be 1 string; if extended descriptions
// are enabled, there could be more than 1 string in the array.
//
// Parameters: 
//  pFileList: An array of file metadata objects
//  pIdx: The index of the file information to display
//  pFormatInfo: An object containing format information returned by getTraditionalFileInfoFormatInfo()
function getFileInfoLineArrayForTraditionalUI(pFileList, pIdx, pFormatInfo)
{
	if (!Array.isArray(pFileList) || typeof(pIdx) !== "number" || pIdx < 0 || pIdx >= pFileList.length)
		return [];
	if (pFileList[pIdx] == undefined)
		return [];

	var userExtDescEnabled = ((user.settings & USER_EXTDESC) == USER_EXTDESC);
	var descLines;
	if (userExtDescEnabled)
		descLines = getExtdFileDescArray(pFileList, pIdx);
	else
	{
		if (pFileList[pIdx].hasOwnProperty("desc") && typeof(pFileList[pIdx].desc) === "string")
			descLines = [ pFileList[pIdx].desc.replace(/\r$/, "").replace(/\n$/, "").replace(/\r\n$/, "") ];
	}
	if (!Array.isArray(descLines))
		descLines = [];
	if (descLines.length == 0)
		descLines.push("");

	var filename = shortenFilename(pFileList[pIdx].name, pFormatInfo.filenameLen, true);
	// Note: substrWithAttrCodes() is defined in dd_lightbar_menu.js
	var fileInfoLines = [];
	fileInfoLines.push(format(pFormatInfo.formatStr, pIdx+1, filename, getFileSizeStr(pFileList[pIdx].size, pFormatInfo.fileSizeLen), substrWithAttrCodes(descLines[0], 0, pFormatInfo.descLen)));
	if (userExtDescEnabled)
	{
		for (var i = 1; i < descLines.length; ++i)
			fileInfoLines.push(format(pFormatInfo.formatStrExtdDescLines, substrWithAttrCodes(descLines[i], 0, pFormatInfo.descLen)));
	}
	return fileInfoLines;
}
// Helper for getFileInfoLineArrayForTraditionalUI(): Returns an object containing formatting information
// for displaying information about files for the traditional UI
function getTraditionalFileInfoFormatInfo()
{
	var fileInfoFormatInfo = {
		filenameLen: gListIdxes.filenameEnd - gListIdxes.filenameStart,
		fileSizeLen: gListIdxes.fileSizeEnd - gListIdxes.fileSizeStart -1,
		numItemsLen: gFileList.length.toString().length
	};
	fileInfoFormatInfo.descLen = gListIdxes.descriptionEnd - gListIdxes.descriptionStart - (fileInfoFormatInfo.numItemsLen+2);
	fileInfoFormatInfo.formatStr = "\x01n" + gColors.listNumTrad + "%" + fileInfoFormatInfo.numItemsLen + "d \x01n" + gColors.filename + "%-" + fileInfoFormatInfo.filenameLen + "s \x01n"
				  + gColors.fileSize + "%" + fileInfoFormatInfo.fileSizeLen + "s \x01n" + gColors.desc + "%-" + fileInfoFormatInfo.descLen + "s\x01n";
	var leadingSpaceLen = fileInfoFormatInfo.numItemsLen + fileInfoFormatInfo.filenameLen + fileInfoFormatInfo.fileSizeLen + 3;
	fileInfoFormatInfo.formatStrExtdDescLines = "\x01n" + charStr(" ", leadingSpaceLen) + "%-" + fileInfoFormatInfo.descLen + "s\x01n";
	return fileInfoFormatInfo;
}

// Gets an array of text lines with a file's extended description. To be used only if
// the user has extended file descriptions enabled.
//
// Parameters: 
//  pFileList: An array of file metadata objects
//  pIdx: The index of the file information to display
//
// Return value: An array of strings containing the file's extended description, if available;
//               if not available, the array will containin the non-extended description.
function getExtdFileDescArray(pFileList, pIdx)
{
	if (!Array.isArray(pFileList) || typeof(pIdx) !== "number" || pIdx < 0 || pIdx >= pFileList.length)
		return [];
	if (pFileList[pIdx] == undefined)
		return [];

	var extdDesc = "";
	if (pFileList[pIdx].hasOwnProperty("extdesc"))
		extdDesc = pFileList[pIdx].extdesc;
	else
	{
		var dirCode = (pFileList[pIdx].hasOwnProperty("dirCode") ? pFileList[pIdx].dirCode : gDirCode);
		var fileMetadata = getFileInfoFromFilebase(dirCode, pFileList[pIdx].name, FileBase.DETAIL.EXTENDED);
		if (fileMetadata.hasOwnProperty("extdesc"))
			extdDesc = fileMetadata.extdesc;
	}
	if (extdDesc.length == 0)
	{
		if (pFileList[pIdx].hasOwnProperty("desc") && typeof(pFileList[pIdx].desc) === "string")
			extdDesc = pFileList[pIdx].desc;
	}
	var descLines = lfexpand(extdDesc).split("\r\n");
	// Splitting as above can result in an extra empty last line
	if (descLines[descLines.length-1].length == 0)
		descLines.pop();
	return descLines;
}

// Returns a string with a character repeated a given number of times
//
// Parameters:
//  pChar: The character to repeat in the string
//  pNumTimes: The number of times to repeat the character
//
// Return value: A string with the given character repeated the given number of times
function charStr(pChar, pNumTimes)
{
	if (typeof(pChar) !== "string" || pChar.length == 0 || typeof(pNumTimes) !== "number" || pNumTimes < 1)
		return "";

	var str = "";
	for (var i = 0; i < pNumTimes; ++i)
		str += pChar;
	return str;
}

// Given a directory internal-code, this returns the library description and directory
// description separated by a dash.
//
// Parameters:
//  pDirCode: The internal directory code
//
// Return value: The library description and directory description separated by a dash
function getLibAndDirDesc(pDirCode)
{
	var libIdx = file_area.dir[pDirCode].lib_index;
	var dirIdx = file_area.dir[pDirCode].index;
	var libDesc = file_area.lib_list[libIdx].description;
	var dirDesc =  file_area.dir[pDirCode].description;
	return libDesc + " - " + dirDesc;
}

// Returns a time_t timestamp with a given year, month, day, hour, minute, and second.
//
// Parameters:
//  pYear: The year
//  pMonthNum: The month (1-12)
//  pDayNum: The day number (1-31)
//  pHour: The hour of day (0-23)
//  pMinute: The minute of the hour (0-59)
//  pSecond: The second of the minute (0-59). This is optional.
//
// Return: A time_t value representing the given date/time
function getTimeTFFromDate(pYear, pMonthNum, pDayNum, pHour, pMinute, pSecond)
{
	var seconds = (typeof(pSecond) === "number" ? pSecond : 0);
	var dateStr = format("%d-%02d-%02dT%02d:%02d:%02d", pYear, pMonthNum, pDayNum, pHour, pMinute, seconds);
	var date = new Date(dateStr);
	return date.getTime() / 1000;
}

// Returns whether a name matches the user's name or alias, case-insensitively
//
// Parameters:
//  pName: A name to match
//
// Return value: Whether the name matches the user's name or alias
function userNameOrAliasMatchCaseIns(pName)
{
	if (typeof(pName) !== "string")
		return false;

	var nameUpper = pName.toUpperCase();
	return nameUpper == user.alias.toUpperCase() || nameUpper == user.name.toUpperCase() || nameUpper == user.handle.toUpperCase();
}
