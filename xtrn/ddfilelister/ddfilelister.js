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
 *                              the hotkeys.
*/

if (typeof(require) === "function")
{
	require("sbbsdefs.js", "K_UPPER");
	require("text.js", "Email"); // Text string definitions (referencing text.dat)
	require("dd_lightbar_menu.js", "DDLightbarMenu");
	require("frame.js", "Frame");
	require("scrollbar.js", "ScrollBar");
	require("mouse_getkey.js", "mouse_getkey");
}
else
{
	load("sbbsdefs.js");
	load("text.js"); // Text string definitions (referencing text.dat)
	load("dd_lightbar_menu.js");
	load("frame.js");
	load("scrollbar.js");
	load("mouse_getkey.js");
}


// If the user's terminal doesn't support ANSI, then just call the standard Synchronet
// file list function and exit now
// TODO: Create a traditional user interface?
if (!console.term_supports(USER_ANSI))
{
	bbs.list_files();
	exit();
}


// Store whether the user is a sysop
var gUserIsSysop = user.compare_ars("SYSOP");


// This script requires Synchronet version 3.19 or higher.
// If the Synchronet version is below the minimum, then just call the standard
// Synchronet file list and exit.
if (system.version_num < 31900)
{
	if (gUserIsSysop)
	{
		var message = "\1n\1h\1y\1i* Warning:\1n\1h\1w Digital Distortion File Lister "
		            + "requires version \1g3.19\1w or\r\n"
		            + "higher of Synchronet.  This BBS is using version \1g" + system.version
		            + "\1w.\1n";
		console.crlf();
		console.print(message);
		console.crlf();
		console.pause();
	}
	bbs.list_files();
	exit();
}

// Lister version information
var LISTER_VERSION = "2.01";
var LISTER_DATE = "2022-02-07";


///////////////////////////////////////////////////////////////////////////////
// Global variables

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
// For terminals that are at least 100 characters wide, allow 10 more characters
// for the filename.  This will also give more space for the description.
if (console.screen_columns >= 100)
	gListIdxes.filenameEnd += 10;
gListIdxes.fileSizeStart = gListIdxes.filenameEnd;
gListIdxes.fileSizeEnd = gListIdxes.fileSizeStart + 7;
gListIdxes.descriptionStart = gListIdxes.fileSizeEnd;
gListIdxes.descriptionEnd = console.screen_columns - 1; // Leave 1 character remaining on the screen
// Colors
var gColors = {
	filename: "\1n\1b\1h",
	fileSize: "\1n\1m\1h",
	desc: "\1n\1w",
	bkgHighlight: "\1n\1" + "4",
	filenameHighlight: "\1c\1h",
	fileSizeHighlight: "\1c\1h",
	descHighlight: "\1c\1h",
	fileTimestamp: "\1g\1h",
	fileInfoWindowBorder: "\1r",
	fileInfoWindowTitle: "\1g",
	errorBoxBorder: "\1g\1h",
	errorMessage: "\1y\1h",
	successMessage: "\1c",

	batchDLInfoWindowBorder: "\1r",
	batchDLInfoWindowTitle: "\1g",
	confirmFileActionWindowBorder: "\1r",
	confirmFileActionWindowWindowTitle: "\1g",

	fileAreaMenuBorder: "\1b",
	fileNormalBkg: "\1" + "4",
	fileAreaNum: "\1w",
	fileAreaDesc: "\1w",
	fileAreaNumItems: "\1w",

	fileAreaMenuHighlightBkg: "\1" + "7",
	fileAreaNumHighlight: "\1b",
	fileAreaDescHighlight: "\1b",
	fileAreaNumItemsHighlight: "\1b"
};


// Actions
var FILE_VIEW_INFO = 1;
var FILE_VIEW = 2;
var FILE_ADD_TO_BATCH_DL = 3;
var HELP = 4;
var QUIT = 5;
var FILE_MOVE = 6;   // Sysop action
var FILE_DELETE = 7; // Sysop action



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


///////////////////////////////////////////////////////////////////////////////
// Script execution code

var gFilebase = new FileBase(bbs.curdir_code);
if (!gFilebase.open())
{
	console.crlf();
	console.print("\1n\1h\1yUnable to open \1w" + file_area.dir[bbs.curdir_code].description + "\1n");
	console.crlf();
	console.pause();
	exit(1);
}

// If we got here, the gFilebase successfully opened.
// If there are no files in the filebase, then say so and exit now.
if (gFilebase.files == 0)
{
	var libIdx = file_area.dir[bbs.curdir_code].lib_index;
	console.crlf();
	console.print("\1n\1cThere are no files in \1h" + file_area.lib_list[libIdx].description + "\1n\1c - \1h" +
	              file_area.dir[bbs.curdir_code].description + "\1n");
	console.crlf();
	console.pause();
	exit();
}

// The sort order to use for the file list
var gFileSortOrder = FileBase.SORT.NATURAL; // Natural sort order, same as DATE_A (import date ascending)

// Read the configuration file and set the settings
readConfigFile();

// To check a user's file basic/extended detail information setting:
// if ((user.settings & USER_EXTDESC) == USER_EXTDESC)

// Get a list of file data with normal detail (without extended info).  When the user
// selects a file to view extended info, we'll get metadata about the file with extended detail.
//var gFileList = gFilebase.get_list("*", FileBase.DETAIL.NORM); // FileBase.DETAIL.EXTENDED
var gFileList = gFilebase.get_list("*", FileBase.DETAIL.NORM, 0, true, gFileSortOrder); // FileBase.DETAIL.EXTENDED

// Clear the screen and display the header lines
console.clear("\1n");
displayFileLibAndDirHeader(bbs.curdir_code);
// Construct and display the menu/command bar at the bottom of the screen
var fileMenuBar = new DDFileMenuBar({ x: 1, y: console.screen_rows });
fileMenuBar.writePromptLine();
// Create the file list menu
var gFileListMenu = createFileListMenu(fileMenuBar.getAllActionKeysStr(true, true) + KEY_LEFT + KEY_RIGHT);
// In a loop, show the file list menu, allowing the user to scroll the file list,
// and respond to user input until the user decides to quit.
gFileListMenu.Draw({});
var continueDoingFileList = true;
var drawFileListMenu = false; // For screen refresh optimization
while (continueDoingFileList)
{
	// Clear the menu's selected item indexes so it's 'fresh' for this round
	for (var prop in gFileListMenu.selectedItemIndexes)
		delete gFileListMenu.selectedItemIndexes[prop];
	var actionRetObj = null;
	var userChoice = gFileListMenu.GetVal(drawFileListMenu, gFileListMenu.selectedItemIndexes);
	drawFileListMenu = false; // For screen refresh optimization
	var lastUserInputUpper = gFileListMenu.lastUserInput != null ? gFileListMenu.lastUserInput.toUpperCase() : null;
	if (lastUserInputUpper == null || lastUserInputUpper == "Q")
		continueDoingFileList = false;
	else if (lastUserInputUpper == KEY_LEFT)
		fileMenuBar.decrementMenuItemAndRefresh();
	else if (lastUserInputUpper == KEY_RIGHT)
		fileMenuBar.incrementMenuItemAndRefresh();
	else if (lastUserInputUpper == KEY_ENTER)
	{
		var currentActionVal = fileMenuBar.getCurrentSelectedAction();
		fileMenuBar.setCurrentActionCode(currentActionVal);
		actionRetObj = doAction(currentActionVal, bbs.curdir_code, gFilebase, gFileList, gFileListMenu);
	}
	else
	{
		var currentActionVal = fileMenuBar.getActionFromChar(lastUserInputUpper, false);
		fileMenuBar.setCurrentActionCode(currentActionVal, true);
		actionRetObj = doAction(currentActionVal, bbs.curdir_code, gFilebase, gFileList, gFileListMenu);
	}
	// If an action was done (actionRetObj is not null), then look at actionRetObj and
	// do what's needed.  Note that quit (for the Q key) is already handled.
	if (actionRetObj != null)
	{
		if (actionRetObj.exitNow)
			continueDoingFileList = false;
		else
		{
			if (actionRetObj.reDrawListerHeader)
			{
				console.print("\1n");
				console.gotoxy(1, 1);
				displayFileLibAndDirHeader(bbs.curdir_code);
			}
			if (actionRetObj.reDrawCmdBar) // Could call fileMenuBar.constructPromptText(); if needed
				fileMenuBar.writePromptLine();
			var redrewPartOfFileListMenu = false;
			if (actionRetObj.fileListPartialRedrawInfo != null)
			{
				drawFileListMenu = false;
				var startX = actionRetObj.fileListPartialRedrawInfo.startX;
				var startY = actionRetObj.fileListPartialRedrawInfo.startY;
				var width = actionRetObj.fileListPartialRedrawInfo.width;
				var height = actionRetObj.fileListPartialRedrawInfo.height;
				gFileListMenu.DrawPartial(startX, startY, width, height, {});
				redrewPartOfFileListMenu = true;
			}
			else
			{
				continueDoingFileList = actionRetObj.continueFileLister;
				drawFileListMenu = actionRetObj.reDrawFileListMenu;
			}
			// If we're not redrawing the whole file list menu, then remove
			// checkmarks from any selected files
			if (!drawFileListMenu && gFileListMenu.numSelectedItemIndexes() > 0)
			{
				var lastItemIdxOnScreen = gFileListMenu.topItemIdx + gFileListMenu.size.height - 1;
				var listItemStartRow = gFileListMenu.pos.y;
				var redrawStartX = gFileListMenu.pos.x + gFileListMenu.size.width - 1;
				var redrawWidth = 1;
				if (gFileListMenu.borderEnabled) // Shouldn't have this enabled
				{
					--lastItemIdxOnScreen;
					++listItemStartRow;
					--redrawStartX;
				}
				if (gFileListMenu.scrollbarEnabled && !gFileListMenu.CanShowAllItemsInWindow())
				{
					--redrawStartX;
					++redrawWidth;
				}
				for (var idx in gFileListMenu.selectedItemIndexes)
				{
					var idxNum = +idx;
					if (idxNum >= gFileListMenu.topItemIdx && idxNum <= lastItemIdxOnScreen)
					{
						gFileListMenu.DrawPartialAbs(redrawStartX, listItemStartRow+idxNum, redrawWidth, 1, {});
						redrewPartOfFileListMenu = true;
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

gFilebase.close();



///////////////////////////////////////////////////////////////////////////////
// Functions: File actions

// Performs a specified file action based on an action code.
//
// Parameters:
//  pActionCode: A code specifying an action to do.  Must be one of the global
//               action codes.
//  pDirCode: The internal code of the file directory
//  pFilebase: A Filebase object representing the downloadable file directory.  This
//             is assumed to be open.
//  pFileList: The list of file metadata objects, as retrieved from the filebase
//  pFileListMenu: The file list menu
//
// Return value: An object with values to indicate status & screen refresh actions; see
//               getDefaultActionRetObj() for details.
function doAction(pActionCode, pDirCode, pFilebase, pFileList, pFileListMenu)
{
	if (typeof(pActionCode) !== "number")
		return getDefaultActionRetObj();
	if (pFilebase == null || typeof(pFilebase) !== "object")
		return getDefaultActionRetObj();

	var retObj = null;
	switch (pActionCode)
	{
		case FILE_VIEW_INFO:
			retObj = showFileInfo(pFilebase, pFileList, pFileListMenu);
			break;
		case FILE_VIEW:
			retObj = viewFile(pFilebase, pFileList, pFileListMenu);
			break;
		case FILE_ADD_TO_BATCH_DL:
			retObj = addSelectedFilesToBatchDLQueue(pDirCode, pFilebase, pFileList, pFileListMenu);
			break;
		case HELP:
			retObj = displayHelpScreen(pDirCode, pFilebase);
			break;
		case QUIT:
			retObj = getDefaultActionRetObj();
			retObj.continueFileLister = false;
			break;
		case FILE_MOVE: // Sysop action
			if (gUserIsSysop)
				retObj = chooseFilebaseAndMoveFileToOtherFilebase_Lightbar(pDirCode, pFilebase, pFileList, pFileListMenu);
			break;
		case FILE_DELETE: // Sysop action
			if (gUserIsSysop)
				retObj = removeFileFromFilebase(pDirCode, pFilebase, pFileList, pFileListMenu);
			break;
	}

	return retObj;
}

// Returns an object for use for returning from performing a file action,
// with default values.
//
// Return value: An object with the following properties:
//               continueFileLister: Boolean - Whether or not the file lister should continue, or exit
//               reDrawFileListMenu: Boolean - Whether or not to re-draw the whole file list
//               reDrawListerHeader: Boolean - Whether or not to re-draw the header at the top of the screen
//               reDrawCmdBar: Boolean - Whether or not to re-draw the command bar at the bottom of the screen
//               fileListPartialRedrawInfo: If part of the file list menu needs to be re-drawn,
//                                          this will be an object that includes the following properties:
//                                          startX: The starting X coordinate for where to re-draw
//                                          startY: The starting Y coordinate for where to re-draw
//                                          width: The width to re-draw
//                                          height: The height to re-draw
//               exitNow: Exit the file lister now (boolean)
//               If no part of the file list menu needs to be re-drawn, this will be null.
function getDefaultActionRetObj()
{
	return {
		continueFileLister: true,
		reDrawFileListMenu: false,
		reDrawListerHeader: false,
		reDrawCmdBar: false,
		fileListPartialRedrawInfo: null,
		exitNow: false
	};
}

// Shows extended information about a file to the user.
//
// Parameters:
//  pFilebase: A Filebase object representing the downloadable file directory.  This
//             is assumed to be open.
//  pFileList: The list of file metadata objects, as retrieved from the filebase
//  pFileListMenu: The file list menu
//
// Return value: An object with values to indicate status & screen refresh actions; see
//               getDefaultActionRetObj() for details.
function showFileInfo(pFilebase, pFileList, pFileListMenu)
{
	var retObj = getDefaultActionRetObj();

	// The width of the frame to display the file info (including borders).  This
	// is declared early so that it can be used for string length adjustment.
	var frameWidth = pFileListMenu.size.width - 4;

	// pFileList[pFileListMenu.selectedItemIdx] has a file metadata object without
	// extended information.  Get a metadata object with extended information so we
	// can display the extended description.
	var extdFileInfo = pFilebase.get(pFileList[pFileListMenu.selectedItemIdx], FileBase.DETAIL.EXTENDED);
	// Build a string with the file information
	var fileTime = pFilebase.get_time(extdFileInfo.name);
	// Make sure the displayed filename isn't too crazy long
	var adjustedFilename = shortenFilename(extdFileInfo.name, frameWidth-2, false);
	var fileInfoStr = "\1n\1wFilename";
	if (adjustedFilename.length < extdFileInfo.name.length)
		fileInfoStr += " (shortened)";
	fileInfoStr += ":\r\n";
	fileInfoStr += gColors.filename + adjustedFilename +  "\1n\1w\r\n";
	// Note: File size can also be retrieved by calling pFilebase.get_size(extdFileInfo.name)
	// TODO: Shouldn't need the max length here
	fileInfoStr += "Size: " + gColors.fileSize + getFileSizeStr(extdFileInfo.size, 99999) + "\1n\1w\r\n";
	fileInfoStr += "Timestamp: " + gColors.fileTimestamp + strftime("%Y-%m-%d %H:%M:%S", fileTime) + "\1n\1w\r\n"
	fileInfoStr += "\r\n";
	fileInfoStr += gColors.desc;
	// extdFileInfo should have extdDesc, but check just in case
	var fileDesc = "";
	if (extdFileInfo.hasOwnProperty("extdesc") && extdFileInfo.extdesc.length > 0)
		fileDesc = extdFileInfo.extdesc;
	else
		fileDesc = extdFileInfo.desc;
	// It's possible for fileDesc to be undefined (due to extDesc or desc being undefined),
	// so make sure it's a string
	if (typeof(fileDesc) !== "string")
		fileDesc = "";
	if (fileDesc.length > 0)
		fileInfoStr += "Description:\r\n" + fileDesc;
	else
		fileInfoStr += "No description available";
	fileInfoStr += "\1n\1w";

	// Construct & draw a frame with the file information & do the input loop
	// for the frame until the user closes the frame.
	var frameUpperLeftX = pFileListMenu.pos.x + 2;
	var frameUpperLeftY = pFileListMenu.pos.y + 2;
	// Note: frameWidth is declared earlier
	var frameHeight = 10;
	var frameTitle = "File Info";
	displayBorderedFrameAndDoInputLoop(frameUpperLeftX, frameUpperLeftY, frameWidth, frameHeight,
	                                   gColors.fileInfoWindowBorder, frameTitle,
	                                   gColors.fileInfoWindowTitle, fileInfoStr);

	// Construct the file list redraw info.  Note that the X and Y are relative
	// to the file list menu, not absolute screen coordinates.
	retObj.fileListPartialRedrawInfo = {
		startX: 2,
		startY: 2,
		width: frameWidth+1,
		height: frameHeight
	};

	return retObj;
}

// Lets the user view a file.
//
// Parameters:
//  pFilebase: A Filebase object representing the downloadable file directory.  This
//             is assumed to be open.
//  pFileList: The list of file metadata objects, as retrieved from the filebase
//  pFileListMenu: The file list menu
//
// Return value: An object with values to indicate status & screen refresh actions; see
//               getDefaultActionRetObj() for details.
function viewFile(pFilebase, pFileList, pFileListMenu)
{
	var retObj = getDefaultActionRetObj();

	var fullyPathedFilename = pFilebase.get_path(pFileList[pFileListMenu.selectedItemIdx]);
	console.gotoxy(1, console.screen_rows);
	console.print("\1n");
	console.crlf();
	var successfullyViewed = bbs.view_file(fullyPathedFilename);
	if (!successfullyViewed)
		console.pause();

	retObj.reDrawListerHeader = true;
	retObj.reDrawFileListMenu = true;
	retObj.reDrawCmdBar = true;
	return retObj;
}

// Allows the user to add their selected file to their batch downloaded queue
//
// Parameters:
//  pDirCode: The internal code of the file directory
//  pFilebase: The FileBase object representing the file directory (assumed open)
//  pFileList: The list of file metadata objects from the file directory
//  pFileListMenu: The menu object for the file diretory
//
// Return value: An object with values to indicate status & screen refresh actions; see
//               getDefaultActionRetObj() for details.
function addSelectedFilesToBatchDLQueue(pDirCode, pFilebase, pFileList, pFileListMenu)
{
	var retObj = getDefaultActionRetObj();

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
		filenames.push(pFileList[pFileListMenu.selectedItemIdx].name);
		metadataObjects.push(pFileList[pFileListMenu.selectedItemIdx]);
	}
	// Note that confirmFileActionWithUser() will re-draw the parts of the file
	// list menu that are necessary.
	var addFilesConfirmed = confirmFileActionWithUser(filenames, "Batch DL add", false);
	if (addFilesConfirmed)
	{
		var batchDLQueueStats = getUserDLQueueStats();
		var filenamesFailed = []; // To store filenames that failed to get added to the queue
		var batchDLFilename = backslash(system.data_dir + "user") + format("%04d", user.number) + ".dnload";
		var batchDLFile = new File(batchDLFilename);
		if (batchDLFile.open(batchDLFile.exists ? "r+" : "w+"))
		{
			displayMsg("Adding file(s) to batch DL queue..", false, false);
			for (var i = 0; i < metadataObjects.length; ++i)
			{
				// If the file isn't in the user's batch DL queue already, then add it.
				var fileAlreadyInQueue = false;
				for (var fIdx = 0; fIdx < batchDLQueueStats.filenames.length && !fileAlreadyInQueue; ++fIdx)
					exists = (batchDLQueueStats.filenames[fIdx].filename == metadataObjects[i].name);
				if (!fileAlreadyInQueue)
				{
					var addToQueueSuccessful = true;
					batchDLFile.writeln("");

					// Add the required "dir" and "desc" properties to the user's batch download
					// queue file.  The section is the filename.
					addToQueueSuccessful = batchDLFile.iniSetValue(metadataObjects[i].name, "dir", pDirCode);
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

		// Frame location & size for batch DL queue stats or filenames that failed
		var frameUpperLeftX = gFileListMenu.pos.x + 2;
		var frameUpperLeftY = gFileListMenu.pos.y + 2;
		var frameWidth = gFileListMenu.size.width - 4;
		var frameInnerWidth = frameWidth - 2; // Without borders
		var frameHeight = 8;
		// To make the list refresh info to return to the main script loop
		function makeBatchRefreshInfoObj(pFrameWidth, pFrameHeight)
		{
			return {
				startX: 3,
				startY: 3,
				width: pFrameWidth+1,
				height: pFrameHeight
			};
		}

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
				console.print("\1n");
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
				// \1cFiles: \1h1 \1n\1c(\1h100 \1n\1cMax)  Credits: 0  Bytes: \1h2,228,254 \1n\1c Time: 00:09:40
				//var fileSize = gFilebase.get_size(gFileList[pIdx].name);
				// Note: The maximum number of allowed files in the batch download queue doesn't seem to
				// be available to JavaScript.
				var totalQueueSize = batchDLQueueStats.totalSize + pFileList[pFileListMenu.selectedItemIdx].size;
				var totalQueueCost = batchDLQueueStats.totalCost + pFileList[pFileListMenu.selectedItemIdx].cost;
				var queueStats = "\1n\1cFiles: \1h" + batchDLQueueStats.numFilesInQueue + "  \1n\1cCredits: \1h"
				               + totalQueueCost + "\1n\1c  Bytes: \1h" + numWithCommas(totalQueueSize) + "\1n\1w\r\n";
				for (var i = 0; i < batchDLQueueStats.filenames.length; ++i)
				{
					queueStats += shortenFilename(batchDLQueueStats.filenames[i].filename, frameInnerWidth, false) + "\r\n";
					queueStats += batchDLQueueStats.filenames[i].desc.substr(0, frameInnerWidth) + "\r\n";
					if (i < batchDLQueueStats.filenames.length-1)
						queueStats += "\r\n";
				}
				var additionalQuitKeys = "yYnN";
				var lastUserInput = displayBorderedFrameAndDoInputLoop(frameUpperLeftX, frameUpperLeftY, frameWidth,
				                                                       frameHeight, gColors.batchDLInfoWindowBorder,
				                                                       frameTitle, gColors.batchDLInfoWindowTitle,
				                                                       queueStats, additionalQuitKeys);
				if (lastUserInput.toUpperCase() == "Y")
				{
					retObj.reDrawFileListMenu = true;
					retObj.reDrawListerHeader = true;
					retObj.reDrawCmdBar = true;
					console.print("\1n");
					console.gotoxy(1, console.screen_rows);
					console.crlf();
					bbs.batch_download();
				}
				else
				{
					retObj.reDrawFileListMenu = true;
					// Construct the file list redraw info.  Note that the X and Y are relative
					// to the file list menu, not absolute screen coordinates.
					//retObj.fileListPartialRedrawInfo = makeBatchRefreshInfoObj(frameWidth, frameHeight);
				}
			}
		}
		else
		{
			eraseMsgBoxScreenArea();
			// Build a frame object to show the names of the files that failed to be added to the
			// user's batch DL queue
			var frameTitle = "Failed to add these files to batch DL queue";
			var fileListStr = "\1n\1w";
			for (var i = 0; i < filenamesFailed.length; ++i)
				fileListStr += shortenFilename(filenamesFailed[i], frameInnerWidth, false) + "\r\n";
			var lastUserInput = displayBorderedFrameAndDoInputLoop(frameUpperLeftX, frameUpperLeftY, frameWidth,
																   frameHeight, gColors.batchDLInfoWindowBorder,
																   frameTitle, gColors.batchDLInfoWindowTitle,
																   fileListStr, "");
			// Construct the file list redraw info.  Note that the X and Y are relative
			// to the file list menu, not absolute screen coordinates.
			retObj.fileListPartialRedrawInfo = makeBatchRefreshInfoObj(frameWidth, frameHeight);
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

	var batchDLFilename = backslash(system.data_dir + "user") + format("%04d", user.number) + ".dnload";
	var batchDLFile = new File(batchDLFilename);
	if (batchDLFile.open(batchDLFile.exists ? "r+" : "w+"))
	{
		// See if a section exists for the filename
		//File.iniGetAllObjects([name_property] [,prefix=none] [,lowercase=false] [,blanks=false])
		var allIniObjs = batchDLFile.iniGetAllObjects();
		console.print("\1n\r\n");
		for (var i = 0; i < allIniObjs.length; ++i)
		{
			if (typeof(allIniObjs[i]) === "object")
			{
				++(retObj.numFilesInQueue);
				//allIniObjs[i].name
				//allIniObjs[i].dir
				//allIniObjs[i].desc
				retObj.filenames.push({ filename: allIniObjs[i].name, desc: allIniObjs[i].desc });
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

		/*
		var sections = batchDLFile.iniGetSections();
		retObj.numFilesInQueue = sections.length;
		for (var i = 0; i < sections.length; ++i)
		{
			//var desc = 
			//retObj.filenames.push({ filename: sections[i], desc:  });
			// Get the dir code from the section, then get the size and cost for
			// the file from the filebase and add them to the totals in retObj
			var dirCode = batchDLFile.iniGetValue(sections[i], "dir", "");
			if (dirCode.length > 0)
			{
				var filebase = new FileBase(dirCode);
				if (filebase.open())
				{
					var fileInfo = filebase.get(sections[i]);
					if (typeof(fileInfo) === "object")
					{
						retObj.totalSize += +(fileInfo.size);
						retObj.totalCost += +(fileInfo.cost);
					}
					filebase.close();
				}
			}
		}
		*/
		batchDLFile.close();
	}

	return retObj;
}

// Displays the help screen.
//
// Parameters:
//  pDirCode: The internal code of the file directory being used
//  pFilebase: A Filebase object representing the downloadable file directory.  This
//             is assumed to be open.
function displayHelpScreen(pDirCode, pFilebase)
{
	var retObj = getDefaultActionRetObj();

	console.clear("\1n");
	// Display program information
	displayTextWithLineBelow("Digital Distortion File Lister", true, "\1n\1c\1h", "\1k\1h")
	console.center("\1n\1cVersion \1g" + LISTER_VERSION + " \1w\1h(\1b" + LISTER_DATE + "\1w)");
	console.crlf();

	// Display information about the current file directory
	var libIdx = file_area.dir[pDirCode].lib_index;
	var dirIdx = file_area.dir[pDirCode].index;
	console.print("\1n\1cCurrent file library: \1g" + file_area.lib_list[libIdx].description);
	console.crlf();
	console.print("\1cCurrent file directory: \1g" + file_area.dir[pDirCode].description);
	console.crlf();
	console.print("\1cThere are \1g" + pFilebase.files + " \1cfiles in this directory.");
	console.crlf();
	console.crlf();

	// Display information about the lister
	var helpStr = "This lists files in your current file directory with a lightbar interface (for an ANSI terminal).  ";
	helpStr += "The file list can be navigated using the up & down arrow keys, PageUp, PageDown, Home, and End keys.  "
	helpStr += "The currently highlighted file in the menu is used by default for the various actions.  For batch download "
	helpStr += "selection, ";
	if (gUserIsSysop)
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
	var printfStr = "\1n\1c\1h%-" + commandStrWidth + "s\1g: \1n\1c%s\r\n";
	printf(printfStr, "I", "Display extended file information");
	printf(printfStr, "V", "View the file");
	printf(printfStr, "B", "Flag the file(s) for batch download");
	if (gUserIsSysop)
	{
		printf(printfStr, "M", "Move the file(s) to another directory");
		printf(printfStr, "D", "Delete the file(s)");
	}
	printf(printfStr, "?", "Show this help screen");
	printf(printfStr, "Q", "Quit back to the BBS");
	console.print("\1n");
	console.crlf();
	//console.pause();

	retObj.reDrawListerHeader = true;
	retObj.reDrawFileListMenu = true;
	retObj.reDrawCmdBar = true;
	return retObj;
}

// Allows the user to move the selected file to another filebase.  Only for sysops!
//
// Parameters:
//  pDirCode: The internal code of the original file directory
//  pFilebase: The FileBase object representing the file directory (assumed open)
//  pFileList: The list of file metadata objects from the file directory
//  pFileListMenu: The menu object for the file diretory
//
// Return value: An object with values to indicate status & screen refresh actions; see
//               getDefaultActionRetObj() for details.
function chooseFilebaseAndMoveFileToOtherFilebase_Lightbar(pDirCode, pFilebase, pFileList, pFileListMenu)
{
	var retObj = getDefaultActionRetObj();

	// Confirm with the user to move the file(s).  If they don't want to,
	// then just return now.
	var filenames = [];
	if (gFileListMenu.numSelectedItemIndexes() > 0)
	{
		for (var idx in gFileListMenu.selectedItemIndexes)
			filenames.push(pFileList[+idx].name);
	}
	else
		filenames.push(pFileList[pFileListMenu.selectedItemIdx].name);
	// Note that confirmFileActionWithUser() will re-draw the parts of the file
	// list menu that are necessary.
	var moveFilesConfirmed = confirmFileActionWithUser(filenames, "Move", false);
	if (!moveFilesConfirmed)
		return retObj;


	retObj.reDrawFileListMenu = true;
	// Prompt the user which directory to move the file to
	var chosenDirCode = null;
	var fileLibMenu = createFileLibMenu();
	console.gotoxy(fileLibMenu.pos.x, fileLibMenu.pos.y-1);
	printf("\1n\1c\1h|\1n\1c%-" + +(fileLibMenu.size.width-1) + "s\1n", "Choose a destination area");
	var continueOn = true;
	while (continueOn)
	{
		var chosenLibIdx = fileLibMenu.GetVal();
		if (typeof(chosenLibIdx) === "number")
		{
			var fileDirMenu = createFileDirMenu(chosenLibIdx);
			chosenDirCode = fileDirMenu.GetVal();
			if (typeof(chosenDirCode) === "string")
			{
				if (chosenDirCode != pDirCode)
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
		// Build an array of file indexes and sort the array
		var fileIndexes = [];
		if (gFileListMenu.numSelectedItemIndexes() > 0)
		{
			for (var idx in gFileListMenu.selectedItemIndexes)
				fileIndexes.push(+idx);
		}
		else
			fileIndexes.push(+(pFileListMenu.selectedItemIdx));
		fileIndexes.sort();

		// Go through the list of files and move each of them
		var moveAllSucceeded = true;
		for (var i = 0; i < fileIndexes.length; ++i)
		{
			var fileIdx = fileIndexes[i];
			var moveRetObj = moveFileToOtherFilebase(pFilebase, pFileList[fileIdx], chosenDirCode);
			if (moveRetObj.moveSucceeded)
			{
				// Remove the file info object from the file list array
				pFileList.splice(fileIdx, 1);
				// Subtract 1 from the remaining indexes in the fileIndexes array
				for (var j = i+1; j < fileIndexes.length; ++j)
					fileIndexes[j] = fileIndexes[j] - 1;
			}
			else
			{
				moveAllSucceeded = false;
				displayMsg(pFileList[fileIdx].name, true);
				displayMsg(moveRetObj.failReason, true);
			}
		}
		if (moveAllSucceeded)
		{
			var libIdx = file_area.dir[chosenDirCode].lib_index;
			var msg = "Successfully moved the file(s) to "
			        + file_area.lib_list[libIdx].description + " - "
			        + file_area.dir[chosenDirCode].description
			displayMsg(msg, false);
		}
		// After moving the files, if the file directory is empty, say so
		if (pFilebase.files == 0)
		{
			displayMsg("The directory now has no files.", false);
			retObj.exitNow = true;
		}
	}

	return retObj;
}

// Allows the user to remove the selected file from the filebase.  Only for sysops!
//
// Parameters:
//  pDirCode: The internal code of the original file directory
//  pFilebase: The FileBase object representing the file directory (assumed open)
//  pFileList: The list of file metadata objects from the file directory
//  pFileListMenu: The menu object for the file diretory
//
// Return value: An object with values to indicate status & screen refresh actions; see
//               getDefaultActionRetObj() for details.
function removeFileFromFilebase(pDirCode, pFilebase, pFileList, pFileListMenu)
{
	var retObj = getDefaultActionRetObj();

	// Confirm the action with the user.  If the user confirms, then remove the file(s).
	// If there are multiple selected files, then prompt to remove each of them.
	// Otherwise, prompt for the one selected file.
	var filenames = [];
	if (gFileListMenu.numSelectedItemIndexes() > 0)
	{
		for (var idx in gFileListMenu.selectedItemIndexes)
			filenames.push(pFileList[+idx].name);
	}
	else
		filenames.push(pFileList[pFileListMenu.selectedItemIdx].name);
	// Note that confirmFileActionWithUser() will re-draw the parts of the file list menu
	// that are necessary.
	var removeFilesConfirmed = confirmFileActionWithUser(filenames, "Remove", false);
	if (removeFilesConfirmed)
	{
		// FileBase.remove(filename [,delete=false])
		var succeeded = pFilebase.remove(pFileList[pFileListMenu.selectedItemIdx].name, true);
		if (succeeded)
		{
			var messages = [ "Successfully removed the file(s)." ];
			// Remove the file info object from the file list array
			pFileList.splice(pFileListMenu.selectedItemIdx, 1);
			// Adjust the file list menu's current selected index
			--pFileListMenu.selectedItemIdx;
			if (pFileListMenu.selectedItemIdx < 0)
				pFileListMenu.selectedItemIdx = 0;
			if (pFileListMenu.topItemIdx > pFileListMenu.selectedItemIdx)
				pFileListMenu.topItemIdx = pFileListMenu.selectedItemIdx;
			// If the file directory still has files in it, have the menu redraw
			// itself to refresh with the missing entry.  Otherwise (no files left),
			// say so and have the lister exit now.
			if (pFilebase.files > 0)
				retObj.reDrawFileListMenu = true;
			else
			{
				messages.push("The directory now has no files.");
				retObj.exitNow = true;
			}
			displayMsgs(messages, false);
		}
		else
			displayMsg("Failed to remove the file!", true); // console.print("\1y\1hFailed to remove the file!\1n");
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
	if (gUserIsSysop)
	{
		this.cmdArray.push(new DDFileMenuBarItem("Move", 0, FILE_MOVE));
		this.cmdArray.push(new DDFileMenuBarItem("Del", 0, FILE_DELETE));
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
	this.promptText = "\1n\1w" + BLOCK1 + BLOCK2 + BLOCK3 + BLOCK4;
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
	this.promptText += "\1w" + THIN_RECTANGLE_RIGHT;
	for (var i = 0; i < numSolidBlocksPerSide; ++i)
		this.promptText += BLOCK4;
	this.promptText += BLOCK3 + BLOCK2 + BLOCK1 + "\1n";
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
	console.gotoxy(this.cmdArray[this.currentCommandIdx].pos, this.pos.y);
	console.print("\1n" + this.getDDFileMenuBarItemText(itemText, false, false));
	// Draw the new item text with selected colors
	itemText = this.getItemTextFromIdx(pCmdIdx);
	console.gotoxy(this.cmdArray[pCmdIdx].pos, this.pos.y);
	console.print("\1n" + this.getDDFileMenuBarItemText(itemText, true, false));
	console.gotoxy(this.pos.x+strip_ctrl(this.promptText).length-1, this.pos.y);

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
	var itemText = "\1n";
	if (selected)
		itemText += "\1" + "1\1r\1h" + firstChar + "\1n\1" + "1\1k" + restOfText;
	else
		itemText += "\1" + "6\1c\1h" + firstChar + "\1n\1" + "6\1k" + restOfText;
	itemText += "\1n";
	if (withTrailingBlock)
		itemText += "\1w" + THIN_RECTANGLE_RIGHT + THIN_RECTANGLE_LEFT + "\1n";
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
			if (this.cmdArray[i].itemText.length > 0 && this.cmdArray[i].itemText.charAt(0) == pChar)
				retCode = this.cmdArray[i].retCode;
		}
	}
	else
	{
		// Not case sensitive
		var charUpper = pChar.toUpperCase();
		for (var i = 0; i < this.cmdArray.length && retCode == -1; ++i)
		{
			if (this.cmdArray[i].itemText.length > 0 && this.cmdArray[i].itemText.charAt(0).toUpperCase() == charUpper)
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
function DDFileMenuBarItem(pItemText, pPos, pRetCode)
{
	this.itemText = pItemText;
	this.pos = pPos;
	this.retCode = pRetCode;
}


///////////////////////////////////////////////////////////////////////////////
// Helper functions

// Moves a file from one filebase to another
//
// Parameters:
//  pSrcFilebase: A FileBase object representing the source filebase.  This is assumed to be open.
//  pSrcFileMetadata: Metadata for the source file.  This is assumed to contain 'normal' detail (not extended)
//  pDestDirCode: The internal code of the destination filebase to move to the file to
//
// Return value: An object containing the following properties:
//               moveSucceeded: Boolean - Whether or not the move succeeded
//               failReason: If the move failed, this is a string that specifies why it failed
function moveFileToOtherFilebase(pSrcFilebase, pSrcFileMetadata, pDestDirCode)
{
	var retObj = {
		moveSucceeded: false,
		failReason: ""
	};

	// pSrcFileMetadata is assumed to be a basic file metadata object, without extended
	// information.  Get a metadata object with maximum information so we have all
	// metadata available.
	var extdFileInfo = pSrcFilebase.get(pSrcFileMetadata, FileBase.DETAIL.MAX);
	// Move the file over, remove it from the original filebase, and add it to the new filebase
	var srcFilenameFull = pSrcFilebase.get_path(pSrcFileMetadata);
	var destFilenameFull = file_area.dir[pDestDirCode].path + pSrcFileMetadata.name;
	if (file_rename(srcFilenameFull, destFilenameFull))
	{
		if (pSrcFilebase.remove(pSrcFileMetadata.name, false))
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
					moveBackSucceeded = pSrcFilebase.add(extdFileInfo);
				if (!moveBackSucceeded)
					retObj.failReason += " & moving the file back failed";
			}
		}
		else
			retObj.failReason = "Failed to remove the file from the source directory";
	}
	else
		retObj.failReason = "Failed to move the file to the new filebase directory";

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
	var keyHelpStr = "\1n\1c\1hQ\1b/\1cEnter\1b/\1cESC\1y: \1gClose\1b";
	var scrollLoopNavHelp = "\1c\1hUp\1b/\1cDn\1b/\1cHome\1b/\1cEnd\1b/\1cPgup\1b/\1cPgDn\1y: \1gNav";
	if (console.screen_columns >= 80)
		keyHelpStr += ", " + scrollLoopNavHelp;
	var borderColor = (typeof(pBorderColor) === "string" ? pBorderColor : "\1r");
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
	frameObj.putmsg(pFrameContents, "\1n");
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
//  pDirCode: The internal code of the file directory to use
function displayFileLibAndDirHeader(pDirCode)
{
	if (typeof(pDirCode) !== "string")
		return;
	if (typeof(file_area.dir[pDirCode]) === "undefined")
		return;

	var libIdx = file_area.dir[pDirCode].lib_index;
	var dirIdx = file_area.dir[pDirCode].index;
	var libDesc = file_area.lib_list[libIdx].description;
	var dirDesc =  file_area.dir[pDirCode].description;

	var hdrTextWidth = console.screen_columns - 21;
	var descWidth = hdrTextWidth - 11;

	// Library line
	console.print("\1n\1w" + BLOCK1 + BLOCK2 + BLOCK3 + BLOCK4 + THIN_RECTANGLE_LEFT);
	printf("\1cLib \1w\1h#\1b%4d\1c: \1n\1c%-" + descWidth + "s\1n", +(libIdx+1), libDesc.substr(0, descWidth));
	console.print("\1w" + THIN_RECTANGLE_RIGHT + "\1k\1h" + BLOCK4 + "\1n\1w" + THIN_RECTANGLE_LEFT +
	              "\1g\1hDD File\1n\1w");
	console.print(THIN_RECTANGLE_RIGHT + BLOCK4 + BLOCK3 + BLOCK2 + BLOCK1);
	console.crlf();
	// Directory line
	console.print("\1n\1w" + BLOCK1 + BLOCK2 + BLOCK3 + BLOCK4 + THIN_RECTANGLE_LEFT);
	printf("\1cDir \1w\1h#\1b%4d\1c: \1n\1c%-" + descWidth + "s\1n", +(dirIdx+1), dirDesc.substr(0, descWidth));
	console.print("\1w" + THIN_RECTANGLE_RIGHT + "\1k\1h" + BLOCK4 + "\1n\1w" + THIN_RECTANGLE_LEFT +
	              "\1g\1hLister \1n\1w");
	console.print(THIN_RECTANGLE_RIGHT + BLOCK4 + BLOCK3 + BLOCK2 + BLOCK1);
	console.print("\1n");
	gNumHeaderLinesDisplayed = 2;

	// List header
	console.crlf();
	displayListHdrLine(false);
	++gNumHeaderLinesDisplayed;

	gErrorMsgBoxULY = gNumHeaderLinesDisplayed; // Note: console.screen_rows is 1-based
}
// Displays the header line with the column headers for the file list
//
// Parameters:
//  pMoveToLocationFirst: Boolean - Whether to move the cursor to the required location first.
function displayListHdrLine(pMoveToLocationFirst)
{
	if (pMoveToLocationFirst && console.term_supports(USER_ANSI))
		console.gotoxy(1, 3);
	var filenameLen = gListIdxes.filenameEnd - gListIdxes.filenameStart;
	var fileSizeLen = gListIdxes.fileSizeEnd - gListIdxes.fileSizeStart -1;
	var shortDescLen = gListIdxes.descriptionEnd - gListIdxes.descriptionStart + 1;
	var formatStr = "\1n\1w\1h%-" + filenameLen + "s %" + fileSizeLen + "s %-"
	              + +(shortDescLen-7) + "s\1n\1w%5s\1n";
	var listHdrEndText = THIN_RECTANGLE_RIGHT + BLOCK4 + BLOCK3 + BLOCK2 + BLOCK1;
	printf(formatStr, "Filename", "Size", "Description", listHdrEndText);
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
	var fileListMenu = new DDLightbarMenu(1, startRow, console.screen_columns - 1, console.screen_rows - (startRow-1) - 1);
	fileListMenu.scrollbarEnabled = true;
	fileListMenu.borderEnabled = false;
	fileListMenu.multiSelect = true;
	fileListMenu.ampersandHotkeysInItems = false;
	fileListMenu.wrapNavigation = false;

	// Add additional keypresses for quitting the menu's input loop so we can
	// respond to these keys.
	if (typeof(pQuitKeys) === "string")
		fileListMenu.AddAdditionalQuitKeys(pQuitKeys);

	fileListMenu.SetColors({
		itemColor: [{start: gListIdxes.filenameStart, end: gListIdxes.filenameEnd, attrs: gColors.filename},
		            {start: gListIdxes.fileSizeStart, end: gListIdxes.fileSizeEnd, attrs: gColors.fileSize},
		            {start: gListIdxes.descriptionStart, end: gListIdxes.descriptionEnd, attrs: gColors.desc}],
		selectedItemColor: [{start: gListIdxes.filenameStart, end: gListIdxes.filenameEnd, attrs: gColors.bkgHighlight + gColors.filenameHighlight},
		                    {start: gListIdxes.fileSizeStart, end: gListIdxes.fileSizeEnd, attrs: gColors.bkgHighlight + gColors.fileSizeHighlight},
		                    {start: gListIdxes.descriptionStart, end: gListIdxes.descriptionEnd, attrs: gColors.bkgHighlight + gColors.descHighlight}]
	});

	fileListMenu.filenameLen = gListIdxes.filenameEnd - gListIdxes.filenameStart;
	fileListMenu.fileSizeLen = gListIdxes.fileSizeEnd - gListIdxes.fileSizeStart -1;
	fileListMenu.shortDescLen = gListIdxes.descriptionEnd - gListIdxes.descriptionStart + 1;
	fileListMenu.fileFormatStr = "%-" + fileListMenu.filenameLen
	                           + "s %" + fileListMenu.fileSizeLen
	                           + "s %-" + fileListMenu.shortDescLen + "s";

	// Define the menu functions for getting the number of items and getting an item
	fileListMenu.NumItems = function() {
		// could also return gFilebase.files
		return gFileList.length;
	};
	fileListMenu.GetItem = function(pIdx) {
		var menuItemObj = this.MakeItemWithRetval(pIdx);
		var filename = shortenFilename(gFileList[pIdx].name, this.filenameLen, true);
		// Note: The file size is in bytes
		var fileSize = gFilebase.get_size(gFileList[pIdx].name);
		var desc = (typeof(gFileList[pIdx].desc) === "string" ? gFileList[pIdx].desc : "");
		menuItemObj.text = format(this.fileFormatStr,
		                          filename,//gFileList[pIdx].name.substr(0, this.filenameLen),
								  getFileSizeStr(fileSize, this.fileSizeLen),
		                          desc.substr(0, this.shortDescLen));
		return menuItemObj;
	}

	fileListMenu.selectedItemIndexes = {};
	fileListMenu.numSelectedItemIndexes = function() {
		return Object.keys(this.selectedItemIndexes).length;
	};
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
		console.print("\1n\1y\1hThere are no file libraries available\1n");
		console.crlf();
		console.pause();
		return;
	}

	//DDLightbarMenu(pX, pY, pWidth, pHeight)
	// Create the menu object
	var startRow = gNumHeaderLinesDisplayed + 4;
	var fileLibMenu = new DDLightbarMenu(5, startRow, console.screen_columns - 10, console.screen_rows - startRow - 5);
	fileLibMenu.scrollbarEnabled = true;
	fileLibMenu.borderEnabled = true;
	fileLibMenu.multiSelect = false;
	fileLibMenu.ampersandHotkeysInItems = false;
	fileLibMenu.wrapNavigation = false;

	// Add additional keypresses for quitting the menu's input loop.
	// Q: Quit
	var additionalQuitKeys = "qQ";
	fileLibMenu.AddAdditionalQuitKeys(additionalQuitKeys);

	// Construct a format string for the file libraries
	var largestNumDirs = getLargestNumDirsWithinFileLibs();
	fileLibMenu.libNumLen = file_area.lib_list.length.toString().length;
	fileLibMenu.numDirsLen = largestNumDirs.toString().length;
	var menuInnerWidth = fileLibMenu.size.width - 2; // Menu width excluding borders
	// Allow 2 for spaces
	fileLibMenu.libDescLen = menuInnerWidth - fileLibMenu.libNumLen - fileLibMenu.numDirsLen - 2;
	fileLibMenu.libFormatStr = "%" + fileLibMenu.libNumLen + "d %-" + fileLibMenu.libDescLen + "s %" + fileLibMenu.numDirsLen + "d";

	// Colors and their indexes
	fileLibMenu.borderColor = gColors.fileAreaMenuBorder;
	var libNumStart = 0;
	var libNumEnd = fileLibMenu.libNumLen;
	var descStart = libNumEnd;
	var descEnd = descStart + fileLibMenu.libDescLen;
	var numDirsStart = descEnd;
	//var numDirsEnd = numDirsStart + fileLibMenu.numDirsLen;
	fileLibMenu.SetColors({
		itemColor: [{start: libNumStart, end: libNumEnd, attrs: "\1n" + gColors.fileNormalBkg + gColors.fileAreaNum},
		            {start: descStart, end:descEnd, attrs: "\1n" + gColors.fileNormalBkg + gColors.fileAreaDesc},
		            {start: numDirsStart, end: -1, attrs: "\1n" + gColors.fileNormalBkg + gColors.fileAreaNumItems}],
		selectedItemColor: [{start: libNumStart, end: libNumEnd, attrs: "\1n" + gColors.fileAreaMenuHighlightBkg + gColors.fileAreaNumHighlight},
		                    {start: descStart, end:descEnd, attrs: "\1n" + gColors.fileAreaMenuHighlightBkg + gColors.fileAreaDescHighlight},
		                    {start: numDirsStart, end: -1, attrs: "\1n" + gColors.fileAreaMenuHighlightBkg + gColors.fileAreaNumItemsHighlight}]
	});

	fileLibMenu.topBorderText = "\1y\1hFile Libraries";
	// Define the menu functions for getting the number of items and getting an item
	fileLibMenu.NumItems = function() {
		return file_area.lib_list.length;
	};
	fileLibMenu.GetItem = function(pIdx) {
		var menuItemObj = this.MakeItemWithRetval(pIdx);
		menuItemObj.text = format(this.libFormatStr,
		                          pIdx + 1,//file_area.lib_list[pIdx].number + 1,
								  file_area.lib_list[pIdx].description.substr(0, this.libDescLen),
								  file_area.lib_list[pIdx].dir_list.length);
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
		// TODO: Better error display
		console.gotoxy(5, startRow);
		console.print("\1n\1y\1hThere are no directories in this file library  \1n");
		console.crlf();
		console.pause();
		return null;
	}

	//DDLightbarMenu(pX, pY, pWidth, pHeight)
	// Create the menu object
	var fileDirMenu = new DDLightbarMenu(5, startRow, console.screen_columns - 10, console.screen_rows - startRow - 5);
	fileDirMenu.scrollbarEnabled = true;
	fileDirMenu.borderEnabled = true;
	fileDirMenu.multiSelect = false;
	fileDirMenu.ampersandHotkeysInItems = false;
	fileDirMenu.wrapNavigation = false;

	// Add additional keypresses for quitting the menu's input loop.
	// Q: Quit
	var additionalQuitKeys = "qQ";
	fileDirMenu.AddAdditionalQuitKeys(additionalQuitKeys);

	fileDirMenu.libIdx = pLibIdx;
	// Construct a format string for the file libraries
	var largestNumFiles = getLargestNumFilesInLibDirs(pLibIdx);
	fileDirMenu.dirNumLen = file_area.lib_list[pLibIdx].dir_list.length.toString().length;
	fileDirMenu.numFilesLen = largestNumFiles.toString().length;
	var menuInnerWidth = fileDirMenu.size.width - 2; // Menu width excluding borders
	// Allow 2 for spaces
	fileDirMenu.dirDescLen = menuInnerWidth - fileDirMenu.dirNumLen - fileDirMenu.numFilesLen - 2;
	fileDirMenu.dirFormatStr = "%" + fileDirMenu.dirNumLen + "d %-" + fileDirMenu.dirDescLen + "s %" + fileDirMenu.numFilesLen + "d";

	// Colors and their indexes
	fileDirMenu.borderColor = gColors.fileAreaMenuBorder;
	var dirNumStart = 0;
	var dirNumEnd = fileDirMenu.dirNumLen;
	var descStart = dirNumEnd;
	var descEnd = descStart + fileDirMenu.dirDescLen;
	var numDirsStart = descEnd;
	//var numDirsEnd = numDirsStart + fileDirMenu.numDirsLen;
	fileDirMenu.SetColors({
		itemColor: [{start: dirNumStart, end: dirNumEnd, attrs: "\1n" + gColors.fileNormalBkg + gColors.fileAreaNum},
		            {start: descStart, end:descEnd, attrs: "\1n" + gColors.fileNormalBkg + gColors.fileAreaDesc},
		            {start: numDirsStart, end: -1, attrs: "\1n" + gColors.fileNormalBkg + gColors.fileAreaNumItems}],
		selectedItemColor: [{start: dirNumStart, end: dirNumEnd, attrs: "\1n" + gColors.fileAreaMenuHighlightBkg + gColors.fileAreaNumHighlight},
		                    {start: descStart, end:descEnd, attrs: "\1n" + gColors.fileAreaMenuHighlightBkg + gColors.fileAreaDescHighlight},
		                    {start: numDirsStart, end: -1, attrs: "\1n" + gColors.fileAreaMenuHighlightBkg + gColors.fileAreaNumItemsHighlight}]
	});

	fileDirMenu.topBorderText = "\1y\1h" + ("File directories of " + file_area.lib_list[pLibIdx].description).substr(0, fileDirMenu.size.width-2);
	// Define the menu functions for getting the number of items and getting an item
	fileDirMenu.NumItems = function() {
		return file_area.lib_list[this.libIdx].dir_list.length;
	};
	fileDirMenu.GetItem = function(pIdx) {
		// Return the internal code for the directory for the item
		var menuItemObj = this.MakeItemWithRetval(file_area.lib_list[this.libIdx].dir_list[pIdx].code);
		menuItemObj.text = format(this.dirFormatStr,
		                          pIdx + 1,//file_area.lib_list[this.libIdx].dir_list[pIdx].number + 1,
								  file_area.lib_list[this.libIdx].dir_list[pIdx].description.substr(0, this.dirDescLen),
								  getNumFilesInDir(this.libIdx, pIdx));
		return menuItemObj;
	}

	return fileDirMenu;
}
// Returns the number of files in a file directory
//
// Parameters:
//  pLibIdx: The library index
//  pDirIdx: The directory index within the file library
//
// Return value: The number of files in the file directory
function getNumFilesInDir(pLibIdx, pDirIdx)
{
	if (typeof(pLibIdx) !== "number" || typeof(pDirIdx) !== "number")
		return 0;
	if (pLibIdx < 0 || pLibIdx >= file_area.lib_list.length)
		return 0;
	if (pDirIdx < 0 || pDirIdx >= file_area.lib_list[pLibIdx].dir_list.length)
		return 0;

	var numFiles = 0;
	var filebase = new FileBase(file_area.lib_list[pLibIdx].dir_list[pDirIdx].code);
	if (filebase.open())
	{
		numFiles = filebase.files;
		filebase.close();
	}
	return numFiles;
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
		var numFilesInDir = getNumFilesInDir(pLibIdx, dirIdx);
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
	var textColor = (typeof(pTextColor) == "string" ? pTextColor : "\1n\1w");
	var lineColor = (typeof(pLineColor) == "string" ? pLineColor : "\1n\1k\1h");

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

	// Draw the box border, then write the messages
	var title = pIsError ? "Error" : "Message";
	var titleColor = pIsError ? gColors.errorMessage : gColors.successMessage;
	drawBorder(gErrorMsgBoxULX, gErrorMsgBoxULY, gErrorMsgBoxWidth, gErrorMsgBoxHeight,
	           gColors.errorBoxBorder, "single", title, titleColor, "");
	var msgColor = "\1n" + (pIsError ? gColors.errorMessage : gColors.successMessage);
	var innerWidth = gErrorMsgBoxWidth - 2;
	var msgFormatStr = msgColor + "%-" + innerWidth + "s\1n";
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
	displayListHdrLine(true);
	gFileListMenu.DrawPartialAbs(gErrorMsgBoxULX, gErrorMsgBoxULY+1, gErrorMsgBoxWidth, gErrorMsgBoxHeight-2);
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
	console.print("\1n" + pColor);
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
			titleText = "\1n" + pTitleColor + titleTextWithoutAttrs;
		console.print(borderChars.preText + "\1n" + substrWithAttrCodes(titleText, 0, titleLen) +
		              "\1n" + pColor + borderChars.postText);
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
		console.print(borderChars.preText + "\1n" + substrWithAttrCodes(pBottomBorderText, 0, textLen) +
		              "\1n" + pColor + borderChars.postText);
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
	console.print("\1n\1g\1h");
	for (var i = 0; i < width; ++i)
		console.print(HORIZONTAL_SINGLE);
	console.print("\1n");
}

// Confirms with the user to perform an action with a file or set of files
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
		drawSeparatorLine(1, console.screen_rows-2, console.screen_columns-1);
		console.gotoxy(1, console.screen_rows-1);
		console.cleartoeol("\1n");
		console.gotoxy(1, console.screen_rows-1);
		var shortFilename = shortenFilename(filename, Math.floor(console.screen_columns/2), false);
		if (pDefaultYes)
			actionConfirmed = console.yesno(pActionName + " " + shortFilename);
		else
			actionConfirmed = !console.noyes(pActionName + " " + shortFilename);
		gFileListMenu.DrawPartialAbs(1, console.screen_rows-2, console.screen_columns, 2, {});
	}
	else
	{
		// Construct & draw a frame with the file list & display the frame to confirm with the
		// user to delete the files
		var frameUpperLeftX = gFileListMenu.pos.x + 2;
		var frameUpperLeftY = gFileListMenu.pos.y + 2;
		var frameWidth = gFileListMenu.size.width - 4;
		var frameHeight = 10;
		var frameTitle = pActionName + " files? (Y/N)";
		var additionalQuitKeys = "yYnN";
		var frameInnerWidth = frameWidth - 2; // Without borders; for filename lengths
		var fileListStr = "\1n\1w";
		for (var i = 0; i < pFilenames.length; ++i)
			fileListStr += shortenFilename(pFilenames[i], frameInnerWidth, false) + "\r\n";
		var lastUserInput = displayBorderedFrameAndDoInputLoop(frameUpperLeftX, frameUpperLeftY, frameWidth,
		                                                       frameHeight, gColors.confirmFileActionWindowBorder,
		                                                       frameTitle, gColors.confirmFileActionWindowWindowTitle,
		                                                       fileListStr, additionalQuitKeys);
		actionConfirmed = (lastUserInput.toUpperCase() == "Y");
		gFileListMenu.DrawPartialAbs(frameUpperLeftX, frameUpperLeftY, frameWidth, frameHeight, {});
	}

	return actionConfirmed;
}


// Reads the configuration file and sets the settings accordingly
function readConfigFile()
{
	this.cfgFileSuccessfullyRead = false;

	var themeFilename = ""; // In case a theme filename is specified

	// Determine the script's startup directory.
	// This code is a trick that was created by Deuce, suggested by Rob Swindell
	// as a way to detect which directory the script was executed in.  I've
	// shortened the code a little.
	var startupPath = '.';
	try { throw dig.dist(dist); } catch(e) { startupPath = e.fileName; }
	startupPath = backslash(startupPath.replace(/[\/\\][^\/\\]*$/,''));

	// Open the main configuration file.  First look for it in the sbbs/mods
	// directory, then sbbs/ctrl, then in the same directory as this script.
	var cfgFilename = "ddfilelister.cfg";
	var cfgFilenameFullPath = file_cfgname(system.mods_dir, cfgFilename);
	if (!file_exists(cfgFilenameFullPath))
		cfgFilenameFullPath = file_cfgname(system.ctrl_dir, cfgFilename);
	if (!file_exists(cfgFilenameFullPath))
		cfgFilenameFullPath = file_cfgname(startupPath, cfgFilename);
	var cfgFile = new File(cfgFilenameFullPath);
	if (cfgFile.open("r"))
	{
		this.cfgFileSuccessfullyRead = true;

		var fileLine = null;     // A line read from the file
		var equalsPos = 0;       // Position of a = in the line
		var commentPos = 0;      // Position of the start of a comment
		var setting = null;      // A setting name (string)
		var settingUpper = null; // Upper-case setting name
		var value = null;        // To store a value for a setting (string)
		var valueUpper = null;   // Upper-cased value for a setting (string)
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

				// Set the appropriate valueUpper in the settings object.
				if (settingUpper == "SORTORDER")
				{
					// FileBase.SORT properties
					// Name		Type	Description
					// NATURAL	number	Natural sort order (same as DATE_A)
					// NAME_AI	number	Filename ascending, case insensitive sort order
					// NAME_DI	number	Filename descending, case insensitive sort order
					// NAME_AS	number	Filename ascending, case sensitive sort order
					// NAME_DS	number	Filename descending, case sensitive sort order
					// DATE_A	number	Import date/time ascending sort order
					// DATE_D	number	Import date/time descending sort order
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
					else // Default
						gFileSortOrder = FileBase.SORT.NATURAL;
				}
				else if (settingUpper == "THEMEFILENAME")
				{
					// First look for the theme config file in the sbbs/mods
					// directory, then sbbs/ctrl, then the same directory as
					// this script.
					themeFilename = system.mods_dir + value;
					if (!file_exists(themeFilename))
						themeFilename = system.ctrl_dir + value;
					if (!file_exists(themeFilename))
						themeFilename = startupPath + value;
				}
			}
		}

		cfgFile.close();
	}
	else
	{
		// Was unable to read the configuration file.  Output a warning to the user
		// that defaults will be used and to notify the sysop.
		console.print("\1n");
		console.crlf();
		console.print("\1w\1hUnable to open the configuration file: \1y" + cfgFilename);
		console.crlf();
		console.print("\1wDefault settings will be used.  Please notify the sysop.");
		mswait(2000);
	}
	
	// If a theme filename was specified, then read the colors & strings
	// from it.
	if (themeFilename.length > 0)
	{
		var themeFile = new File(themeFilename);
		if (themeFile.open("r"))
		{
			var fileLine = null;     // A line read from the file
			var equalsPos = 0;       // Position of a = in the line
			var commentPos = 0;      // Position of the start of a comment
			var setting = null;      // A setting name (string)
			var value = null;        // To store a value for a setting (string)
			while (!themeFile.eof)
			{
				// Read the next line from the config file.
				fileLine = themeFile.readln(2048);

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
					// Read the setting (without leading/trailing spaces) & value
					setting = trimSpaces(fileLine.substr(0, equalsPos), true, false, true);
					value = fileLine.substr(equalsPos+1);

					if (gColors.hasOwnProperty(setting))
					{
						// Trim leading & trailing spaces from the value when
						// setting a color.  Also, replace any instances of "\1"
						// with the Synchronet attribute control character.
						gColors[setting] = trimSpaces(value, true, false, true).replace(/\\1/g, "\1");
					}
				}
			}

			themeFile.close();
		}
		else
		{
			// Was unable to read the theme file.  Output a warning to the user
			// that defaults will be used and to notify the sysop.
			this.cfgFileSuccessfullyRead = false;
			console.print("\1n");
			console.crlf();
			console.print("\1w\1hUnable to open the theme file: \1y" + themeFilename);
			console.crlf();
			console.print("\1wDefault colors will be used.  Please notify the sysop.");
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

	var filenameExt = file_getext(pFilename);
	var filenameWithoutExt = file_getname(pFilename);
	var extIdx = filenameWithoutExt.indexOf(filenameExt);
	if (extIdx >= 0)
		filenameWithoutExt = filenameWithoutExt.substr(0, extIdx);

	var maxWithoutExtLen = pMaxLen - filenameExt.length;
	if (filenameWithoutExt.length > maxWithoutExtLen)
		filenameWithoutExt = filenameWithoutExt.substr(0, maxWithoutExtLen);

	var fillWidth = (typeof(pFillWidth) === "boolean" ? pFillWidth : false);
	var adjustedFilename = "";
	if (fillWidth)
		adjustedFilename = format("%-" + maxWithoutExtLen + "s%s", filenameWithoutExt, filenameExt);
	else
		adjustedFilename = filenameWithoutExt + filenameExt;

	return adjustedFilename;
}
