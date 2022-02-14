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
if (!console.term_supports(USER_ANSI))
{
	bbs.list_files();
	exit();
}



// This script requires Synchronet version 3.19 or higher.
// If the Synchronet version is below the minimum, then just call the standard
// Synchronet file list and exit.
if (system.version_num < 31900)
{
	if (user.is_sysop)
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
var LISTER_VERSION = "2.02";
var LISTER_DATE = "2022-02-13";


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

// Search/list modes
var MODE_LIST_CURDIR = 1;
var MODE_SEARCH_FILENAME = 2;
var MODE_SEARCH_DESCRIPTION = 3;
var MODE_NEW_FILE_SEARCH = 4;

// The searc/list mode for the current run
var gScriptMode = MODE_LIST_CURDIR; // Default



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

///////////////////////////////////////////////////////////////////////////////
// Script execution code

// The sort order to use for the file list
var gFileSortOrder = FileBase.SORT.NATURAL; // Natural sort order, same as DATE_A (import date ascending)

var gSearchVerbose = false;

// Read the configuration file and set the settings
readConfigFile();

// Parse command-line arguments (which sets program options)
parseArgs(argv);

// This array will contain file metadata objects
var gFileList = [];

// Populate the file list based on the script mode (list/search)
var listPopRetObj = populateFileList(gScriptMode);
if (listPopRetObj.exitNow)
	exit(listPopRetObj.exitCode);

// If there are no files, then say so and exit.
if (gFileList.length == 0)
{
	console.crlf();
	console.print("\1n\1c");
	if (gScriptMode == MODE_LIST_CURDIR)
		console.print("There are no files in the current directory.");
	else
		console.print("No files were found.");
	console.print("\1n");
	console.crlf();
	console.pause();
	exit(0);
}


// Clear the screen and display the header lines
console.clear("\1n");
displayFileLibAndDirHeader();
// Construct and display the menu/command bar at the bottom of the screen
var fileMenuBar = new DDFileMenuBar({ x: 1, y: console.screen_rows });
fileMenuBar.writePromptLine();
// Create the file list menu
var gFileListMenu = createFileListMenu(fileMenuBar.getAllActionKeysStr(true, true) + KEY_LEFT + KEY_RIGHT + KEY_DEL);
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
		actionRetObj = doAction(currentActionVal, gFileList, gFileListMenu);
	}
	// Allow the delete key as a special key for sysops to delete the selected file(s)
	else if (lastUserInputUpper == KEY_DEL)
	{
		if (user.is_sysop)
		{
			fileMenuBar.setCurrentActionCode(FILE_DELETE, true);
			actionRetObj = doAction(FILE_DELETE, gFileList, gFileListMenu);
		}
	}
	else
	{
		var currentActionVal = fileMenuBar.getActionFromChar(lastUserInputUpper, false);
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
			if (actionRetObj.reDrawHeaderTextOnly)
			{
				console.print("\1n");
				displayFileLibAndDirHeader(true); // Will move the cursor where it needs to be
			}
			else if (actionRetObj.reDrawListerHeader)
			{
				console.print("\1n");
				console.gotoxy(1, 1);
				displayFileLibAndDirHeader();
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




///////////////////////////////////////////////////////////////////////////////
// Functions: File actions

// Performs a specified file action based on an action code.
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

	var retObj = null;
	switch (pActionCode)
	{
		case FILE_VIEW_INFO:
			retObj = showFileInfo(pFileList, pFileListMenu);
			break;
		case FILE_VIEW:
			retObj = viewFile(pFileList, pFileListMenu);
			break;
		case FILE_ADD_TO_BATCH_DL:
			retObj = addSelectedFilesToBatchDLQueue(pFileList, pFileListMenu);
			break;
		case HELP:
			retObj = displayHelpScreen();
			break;
		case QUIT:
			retObj = getDefaultActionRetObj();
			retObj.continueFileLister = false;
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

// Returns an object for use for returning from performing a file action,
// with default values.
//
// Return value: An object with the following properties:
//               continueFileLister: Boolean - Whether or not the file lister should continue, or exit
//               reDrawFileListMenu: Boolean - Whether or not to re-draw the whole file list
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
//               exitNow: Exit the file lister now (boolean)
//               If no part of the file list menu needs to be re-drawn, this will be null.
function getDefaultActionRetObj()
{
	return {
		continueFileLister: true,
		reDrawFileListMenu: false,
		reDrawListerHeader: false,
		reDrawHeaderTextOnly: false,
		reDrawCmdBar: false,
		fileListPartialRedrawInfo: null,
		exitNow: false
	};
}

// Shows extended information about a file to the user.
//
// Parameters:
//  pFileList: The list of file metadata objects, as retrieved from the filebase
//  pFileListMenu: The file list menu
//
// Return value: An object with values to indicate status & screen refresh actions; see
//               getDefaultActionRetObj() for details.
function showFileInfo(pFileList, pFileListMenu)
{
	var retObj = getDefaultActionRetObj();

	// The width of the frame to display the file info (including borders).  This
	// is declared early so that it can be used for string length adjustment.
	var frameWidth = pFileListMenu.size.width - 4;
	var frameInnerWidth = frameWidth - 2; // Without borders

	// pFileList[pFileListMenu.selectedItemIdx] has a file metadata object without
	// extended information.  Get a metadata object with extended information so we
	// can display the extended description.
	// The metadata object in pFileList should have a dirCode added by this script.
	// If not, assume the user's current directory.
	var dirCode = bbs.curdir_code;
	if (pFileList[pFileListMenu.selectedItemIdx].hasOwnProperty("dirCode"))
		dirCode = pFileList[pFileListMenu.selectedItemIdx].dirCode;
	var fileInfoObj = getFileInfoFromFilebase(dirCode, pFileList[pFileListMenu.selectedItemIdx].name, FileBase.DETAIL.EXTENDED);
	var extdFileInfo = fileInfoObj.fileMetadataObj;
	if (typeof(extdFileInfo) !== "object")
	{
		displayMsg("Unable to get file info!", true, true);
		return;
	}
	var fileTime = fileInfoObj.fileTime;
	// Build a string with the file information
	// Make sure the displayed filename isn't too crazy long
	var adjustedFilename = shortenFilename(extdFileInfo.name, frameInnerWidth, false);
	var fileInfoStr = "\1n\1wFilename";
	if (adjustedFilename.length < extdFileInfo.name.length)
		fileInfoStr += " (shortened)";
	fileInfoStr += ":\r\n";
	fileInfoStr += gColors.filename + adjustedFilename +  "\1n\1w\r\n";
	// Note: File size can also be retrieved by calling a FileBase's get_size(extdFileInfo.name)
	// TODO: Shouldn't need the max length here
	fileInfoStr += "Size: " + gColors.fileSize + getFileSizeStr(extdFileInfo.size, 99999) + "\1n\1w\r\n";
	fileInfoStr += "Timestamp: " + gColors.fileTimestamp + strftime("%Y-%m-%d %H:%M:%S", fileTime) + "\1n\1w\r\n"
	fileInfoStr += "\r\n";

	// File library/directory information
	var libIdx = file_area.dir[dirCode].lib_index;
	var dirIdx = file_area.dir[dirCode].index;
	var libDesc = file_area.lib_list[libIdx].description;
	var dirDesc =  file_area.dir[dirCode].description;
	fileInfoStr += "\1c\1hLib\1g: \1n\1c" + libDesc.substr(0, frameInnerWidth-5) + "\1n\1w\r\n";
	fileInfoStr += "\1c\1hDir\1g: \1n\1c" + dirDesc.substr(0, frameInnerWidth-5) + "\1n\1w\r\n";
	fileInfoStr += "\r\n";

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
	fileInfoStr += gColors.desc;
	if (fileDesc.length > 0)
		fileInfoStr += "Description:\r\n" + fileDesc;
	else
		fileInfoStr += "No description available";
	if (user.is_sysop)
	{
		var sysopFields = [ "from", "cost", "added"];
		for (var sI = 0; sI < sysopFields.length; ++sI)
		{
			var prop = sysopFields[sI];
			if (extdFileInfo.hasOwnProperty(prop))
			{
				if (typeof(extdFileInfo[prop]) === "string" && extdFileInfo[prop].length == 0)
					continue;
				var propName = prop.charAt(0).toUpperCase() + prop.substr(1);
				fileInfoStr += "\r\n\1n\1c\1h" + propName + "\1g:\1n\1c ";
				if (prop == "added")
					fileInfoStr += strftime("%Y-%m-%d %H:%M:%S", extdFileInfo.added);
				else
					fileInfoStr += extdFileInfo[prop].toString().substr(0, frameInnerWidth);
				fileInfoStr += "\1n\1w";
			}
		}
	}
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
//  pFileList: The list of file metadata objects, as retrieved from the filebase
//  pFileListMenu: The file list menu
//
// Return value: An object with values to indicate status & screen refresh actions; see
//               getDefaultActionRetObj() for details.
function viewFile(pFileList, pFileListMenu)
{
	var retObj = getDefaultActionRetObj();

	// Open the filebase & get the fully pathed filename
	var fullyPathedFilename = "";
	var filebase = new FileBase(pFileList[pFileListMenu.selectedItemIdx].dirCode);
	if (filebase.open())
	{
		fullyPathedFilename = filebase.get_path(pFileList[pFileListMenu.selectedItemIdx]);
		filebase.close();
	}
	else
	{
		displayMsg("Failed to open the filebase!", true, true);
		return retObj;
	}

	// View the file
	console.gotoxy(1, console.screen_rows);
	console.print("\1n");
	console.crlf();
	var successfullyViewed = bbs.view_file(fullyPathedFilename);
	console.print("\1n");
	if (gPauseAfterViewingFile || !successfullyViewed)
		console.pause();

	retObj.reDrawListerHeader = true;
	retObj.reDrawFileListMenu = true;
	retObj.reDrawCmdBar = true;
	return retObj;
}

// Allows the user to add their selected file to their batch downloaded queue
//
// Parameters:
//  pFileList: The list of file metadata objects from the file directory
//  pFileListMenu: The menu object for the file diretory
//
// Return value: An object with values to indicate status & screen refresh actions; see
//               getDefaultActionRetObj() for details.
function addSelectedFilesToBatchDLQueue(pFileList, pFileListMenu)
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
function displayHelpScreen()
{
	var retObj = getDefaultActionRetObj();

	console.clear("\1n");
	// Display program information
	displayTextWithLineBelow("Digital Distortion File Lister", true, "\1n\1c\1h", "\1k\1h")
	console.center("\1n\1cVersion \1g" + LISTER_VERSION + " \1w\1h(\1b" + LISTER_DATE + "\1w)");
	console.crlf();

	// If listing files in a directory, display information about the current file directory.
	if (gScriptMode == MODE_LIST_CURDIR)
	{
		var libIdx = file_area.dir[bbs.curdir_code].lib_index;
		var dirIdx = file_area.dir[bbs.curdir_code].index;
		console.print("\1n\1cCurrent file library: \1g" + file_area.lib_list[libIdx].description);
		console.crlf();
		console.print("\1cCurrent file directory: \1g" + file_area.dir[bbs.curdir_code].description);
		console.crlf();
		console.print("\1cThere are \1g" + file_area.dir[bbs.curdir_code].files + " \1cfiles in this directory.");
	}
	else if (gScriptMode == MODE_SEARCH_FILENAME)
		console.print("\1n\1cCurrently performing a filename search");
	else if (gScriptMode == MODE_SEARCH_DESCRIPTION)
		console.print("\1n\1cCurrently performing a description search");
	else if (gScriptMode == MODE_NEW_FILE_SEARCH)
		console.print("\1n\1cCurrently performing a new file search");
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
	var printfStr = "\1n\1c\1h%-" + commandStrWidth + "s\1g: \1n\1c%s\r\n";
	printf(printfStr, "I", "Display extended file information");
	printf(printfStr, "V", "View the file");
	printf(printfStr, "B", "Flag the file(s) for batch download");
	if (user.is_sysop)
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
//  pFileList: The list of file metadata objects from the file directory
//  pFileListMenu: The menu object for the file diretory
//
// Return value: An object with values to indicate status & screen refresh actions; see
//               getDefaultActionRetObj() for details.
function chooseFilebaseAndMoveFileToOtherFilebase(pFileList, pFileListMenu)
{
	var retObj = getDefaultActionRetObj();

	// Confirm with the user to move the file(s).  If they don't want to,
	// then just return now.
	var filenames = [];
	if (pFileListMenu.numSelectedItemIndexes() > 0)
	{
		for (var idx in pFileListMenu.selectedItemIndexes)
			filenames.push(pFileList[+idx].name);
	}
	else
		filenames.push(pFileList[pFileListMenu.selectedItemIdx].name);
	// Note that confirmFileActionWithUser() will re-draw the parts of the file
	// list menu that are necessary.
	var moveFilesConfirmed = confirmFileActionWithUser(filenames, "Move", false);
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
		width: fileLibMenu.size.width + 1,
		height: fileLibMenu.size.height + 1 // + 1 because of the label above the menu
	};
	console.gotoxy(fileLibMenu.pos.x, fileLibMenu.pos.y-1);
	printf("\1n\1c\1h|\1n\1c%-" + +(fileLibMenu.size.width-1) + "s\1n", "Choose a destination area");
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
			chosenDirCode = fileDirMenu.GetVal();
			if (typeof(chosenDirCode) === "string")
			{
				if (chosenDirCode != pFileList[pFileListMenu.selectedItemIdx].dirCode)
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
		var libIdx = file_area.dir[chosenDirCode].lib_index;
		var dirIdx = file_area.dir[chosenDirCode].index;
		var libDesc = file_area.lib_list[libIdx].description;
		var dirDesc =  file_area.dir[chosenDirCode].description;
		var destLibAndDirDesc = libDesc + " - " + dirDesc;

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
		fileIndexes.sort(function(a, b) { return a - b});

		// Go through the list of files and move each of them
		var moveAllSucceeded = true;
		for (var i = 0; i < fileIndexes.length; ++i)
		{
			var fileIdx = fileIndexes[i];
			// For logging
			libIdx = file_area.dir[pFileList[fileIdx].dirCode].lib_index;
			dirIdx = file_area.dir[pFileList[fileIdx].dirCode].index;
			libDesc = file_area.lib_list[libIdx].description;
			dirDesc =  file_area.dir[pFileList[fileIdx].dirCode].description;
			var srcLibAndDirDesc = libDesc + " - " + dirDesc;

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
				if (gScriptMode == MODE_LIST_CURDIR)
				{
					pFileList.splice(fileIdx, 1);
					// Subtract 1 from the remaining indexes in the fileIndexes array
					for (var j = i+1; j < fileIndexes.length; ++j)
						fileIndexes[j] = fileIndexes[j] - 1;
					// Have the file list menu set up its description width, colors, and format
					// string again in case it no longer needs to use its scrollbar
					pFileListMenu.SetItemWidthsColorsAndFormatStr();
					retObj.reDrawFileListMenu = true;
				}
				else
				{
					// Note: getFileInfoFromFilebase() will add dirCode to the metadata object
					var fileDataObj = getFileInfoFromFilebase(chosenDirCode, pFileList[fileIdx].name, FileBase.DETAIL.NORM);
					pFileList[fileIdx] = fileDataObj.fileMetadataObj;
					/*
					// If all files were in the same directory, then we'll need to update the header
					// lines at the top of the file list.  If there's only one file in the list,
					// the header lines will need to display the correct directory.  Otherwise,
					// set allSameDir to false so the header lines will now say "various".
					// However, if not all files were in the same directory, check to see if they
					// are now, and if so, we'll need to re-draw the header lines.
					if (typeof(pFileList.allSameDir) == "boolean")
					{
						if (pFileList.allSameDir)
						{
							if (pFileList.length > 1)
								pFileList.allSameDir = false;
							//retObj.reDrawListerHeader = true;
							retObj.reDrawHeaderTextOnly = true;
						}
						else
						{
							pFileList.allSameDir = true; // Until we find it's not true
							for (var fileListIdx = 1; fileListIdx < pFileList.length && pFileList.allSameDir; ++fileListIdx)
								pFileList.allSameDir = (pFileList[fileListIdx].dirCode == pFileList[0].dirCode);
							//retObj.reDrawListerHeader =  pFileList.allSameDir;
							retObj.reDrawHeaderTextOnly =  pFileList.allSameDir;
						}
					}
					*/
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
		if (gScriptMode != MODE_LIST_CURDIR && typeof(pFileList.allSameDir) == "boolean")
		{
			if (pFileList.allSameDir)
			{
				if (pFileList.length > 1)
					pFileList.allSameDir = false;
				//retObj.reDrawListerHeader = true;
				retObj.reDrawHeaderTextOnly = true;
			}
			else
			{
				pFileList.allSameDir = true; // Until we find it's not true
				for (var fileListIdx = 1; fileListIdx < pFileList.length && pFileList.allSameDir; ++fileListIdx)
					pFileList.allSameDir = (pFileList[fileListIdx].dirCode == pFileList[0].dirCode);
				//retObj.reDrawListerHeader =  pFileList.allSameDir;
				retObj.reDrawHeaderTextOnly =  pFileList.allSameDir;
			}
		}
		// Display a success/fail message
		if (moveAllSucceeded)
		{
			var msg = "Successfully moved the file(s) to " + destLibAndDirDesc;
			displayMsg(msg, false, true);
		}
		else
		{
			displayMsg("Failed to move the file(s)!", true, true);
		}
		// After moving the files, if there are no more files (in the directory or otherwise),
		// say so and exit now.
		if (gScriptMode == MODE_LIST_CURDIR && file_area.dir[bbs.curdir_code].files == 0)
		{
			displayMsg("There are no more files in the directory.", false);
			retObj.exitNow = true;
		}
		else if (pFileList.length == 0)
		{
			displayMsg("There are no more files.", false);
			retObj.exitNow = true;
		}
		// If not exiting now, we'll want to re-draw part of the file list to erase the
		// area chooser menu.
		if (!retObj.exitNow)
			retObj.fileListPartialRedrawInfo = fileListPartialRedrawInfo;
	}
	else
	{
		// The user has canceled out of the area selection.
		// We'll want to re-draw part of the file list to erase the area chooser menu.
		retObj.fileListPartialRedrawInfo = fileListPartialRedrawInfo;
	}

	return retObj;
}

// Allows the user to remove the selected file(s) from the filebase.  Only for sysops!
//
// Parameters:
//  pFileList: The list of file metadata objects from the file directory
//  pFileListMenu: The menu object for the file diretory
//
// Return value: An object with values to indicate status & screen refresh actions; see
//               getDefaultActionRetObj() for details.
function confirmAndRemoveFilesFromFilebase(pFileList, pFileListMenu)
{
	var retObj = getDefaultActionRetObj();

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
	if (removeFilesConfirmed)
	{
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
				// FileBase.remove(filename [,delete=false])
				removeFileSucceeded = filebase.remove(pFileList[fileIdx].name, true);
				if (gScriptMode == MODE_LIST_CURDIR)
					numFilesRemaining = filebase.files;
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
			retObj.reDrawFileListMenu = true;
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
			if (gScriptMode == MODE_LIST_CURDIR)
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
	if (user.is_sysop)
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
	var retObj = {
		fileMetadataObj: null,
		fileTime: 0
	};

	if (typeof(pDirCode) !== "string" || pDirCode.length == 0 || typeof(pFilename) !== "string" || pFilename.length == 0)
		return retObj;

	var filebase = new FileBase(pDirCode);
	if (filebase.open())
	{
		// Just in case the file has the full path, get just the filename from it.
		var filename = file_getname(pFilename);
		var fileDetail = (typeof(pDetail) === "number" ? pDetail : FileBase.DETAIL.NORM);
		retObj.fileMetadataObj = filebase.get(filename, fileDetail);
		retObj.fileMetadataObj.dirCode = pDirCode;
		//retObj.fileMetadataObj.size = filebase.get_size(filename);
		retObj.fileTime = filebase.get_time(filename);
		filebase.close();
	}

	return retObj;
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
//  pTextOnly: Only draw the library & directory text (no decoration or other text).
//             This is optional & defaults to false.
function displayFileLibAndDirHeader(pTextOnly)
{
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
	if (gScriptMode == MODE_LIST_CURDIR)
		dirCode = bbs.curdir_code;
	else if (typeof(gFileList.allSameDir) == "boolean" && gFileList.allSameDir && gFileList.length > 0)
		dirCode = gFileList[0].dirCode;
	if (dirCode.length > 0)
	{
		libIdx = file_area.dir[dirCode].lib_index;
		dirIdx = file_area.dir[dirCode].index;
		libDesc = file_area.lib_list[libIdx].description;
		dirDesc =  file_area.dir[dirCode].description;
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
	var libText = format("\1cLib \1w\1h#\1b%4d\1c: \1n\1c%-" + descWidth + "s\1n", +(libIdx+1), libDesc.substr(0, descWidth));
	var dirText = format("\1cDir \1w\1h#\1b%4d\1c: \1n\1c%-" + descWidth + "s\1n", +(dirIdx+1), dirDesc.substr(0, descWidth));

	// Library line
	if (textOnly)
	{
		console.gotoxy(6, 1);
		console.print("\1n" + libText);
		console.gotoxy(6, 2);
		console.print("\1n" + dirText);
	}
	else
	{
		console.print("\1n\1w" + BLOCK1 + BLOCK2 + BLOCK3 + BLOCK4 + THIN_RECTANGLE_LEFT);
		console.print(libText);
		console.print("\1w" + THIN_RECTANGLE_RIGHT + "\1k\1h" + BLOCK4 + "\1n\1w" + THIN_RECTANGLE_LEFT +
					  "\1g\1hDD File\1n\1w");
		console.print(THIN_RECTANGLE_RIGHT + BLOCK4 + BLOCK3 + BLOCK2 + BLOCK1);
		console.crlf();
		// Directory line
		console.print("\1n\1w" + BLOCK1 + BLOCK2 + BLOCK3 + BLOCK4 + THIN_RECTANGLE_LEFT);
		console.print(dirText);
		console.print("\1w" + THIN_RECTANGLE_RIGHT + "\1k\1h" + BLOCK4 + "\1n\1w" + THIN_RECTANGLE_LEFT +
					  "\1g\1hLister \1n\1w");
		console.print(THIN_RECTANGLE_RIGHT + BLOCK4 + BLOCK3 + BLOCK2 + BLOCK1);
		console.print("\1n");

		// List header
		console.crlf();
		displayListHdrLine(false);

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
function displayListHdrLine(pMoveToLocationFirst)
{
	if (pMoveToLocationFirst && console.term_supports(USER_ANSI))
		console.gotoxy(1, 3);
	var filenameLen = gListIdxes.filenameEnd - gListIdxes.filenameStart;
	var fileSizeLen = gListIdxes.fileSizeEnd - gListIdxes.fileSizeStart -1;
	//var shortDescLen = gListIdxes.descriptionEnd - gListIdxes.descriptionStart + 1;
	// shortDescLen here should always be the same (for the last blocks to always be in the same
	// position), whereas descriptionEnd might change based on whether the menu is using its scrollbar
	var shortDescLen = 60;
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
		this.fileFormatStr = "%-" + this.filenameLen + "s %" + this.fileSizeLen
		                   + "s %-" + this.shortDescLen + "s";
		return widthChanged;
	};
	// Set up the menu's description width, colors, and format string
	fileListMenu.SetItemWidthsColorsAndFormatStr();

	// Define the menu function for getting an item
	fileListMenu.GetItem = function(pIdx) {
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
	// Define the menu function for getting an item
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
		displayMsg("There are no directories in this file library", true, true);
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
	// Define the menu function for ggetting an item
	fileDirMenu.GetItem = function(pIdx) {
		// Return the internal code for the directory for the item
		var menuItemObj = this.MakeItemWithRetval(file_area.lib_list[this.libIdx].dir_list[pIdx].code);
		menuItemObj.text = format(this.dirFormatStr,
		                          pIdx + 1,//file_area.lib_list[this.libIdx].dir_list[pIdx].number + 1,
								  file_area.lib_list[this.libIdx].dir_list[pIdx].description.substr(0, this.dirDescLen),
								  file_area.lib_list[this.libIdx].dir_list[pIdx].files);
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
				var valueUpper = value.toUpperCase();

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
				else if (settingUpper == "PAUSEAFTERVIEWINGFILE")
					gPauseAfterViewingFile = (value.toUpperCase() == "TRUE");
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


// Parses command-line arguments, where each argument in the given array is in
// the format -arg=val.  If the value is the string "true" or "false", then the
// value will be a boolean.  Otherwise, the value will be a string.
//
// Parameters:
//  pArgArr: An array of strings containing values in the format -arg=val
function parseArgs(pArgArr)
{
	// Default program options
	gScriptMode = MODE_LIST_CURDIR;

	// Sanity checking for pArgArr - Make sure it's an array
	if ((typeof(pArgArr) != "object") || (typeof(pArgArr.length) != "number"))
		return;

	// Go through pArgArr looking for strings in the format -arg=val and parse them
	// into objects in the argVals array.
	var equalsIdx = 0;
	var argName = "";
	var argVal = "";
	var argValUpper = ""; // For case-insensitive matching
	for (var i = 0; i < pArgArr.length; ++i)
	{
		// We're looking for strings that start with "-", except strings that are
		// only "-".
		if ((typeof(pArgArr[i]) != "string") || (pArgArr[i].length == 0) ||
		    (pArgArr[i].charAt(0) != "-") || (pArgArr[i] == "-"))
		{
			continue;
		}

		// Look for an = and if found, split the string on the =
		equalsIdx = pArgArr[i].indexOf("=");
		// If a = is found, then split on it and add the argument name & value
		// to the array.  Otherwise (if the = is not found), then treat the
		// argument as a boolean and set it to true (to enable an option).
		if (equalsIdx > -1)
		{
			argName = pArgArr[i].substring(1, equalsIdx).toUpperCase();
			argVal = pArgArr[i].substr(equalsIdx+1);
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
					gScriptMode = MODE_LIST_CURDIR;
			}
		}
		else // An equals sign (=) was not found.  Add as a boolean set to true to enable the option.
		{
			// Nothing to be done here for this script
		}
	}
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
	if (pSearchMode == MODE_LIST_CURDIR) // This is the default
	{
		var filebase = new FileBase(bbs.curdir_code);
		if (filebase.open())
		{
			// If there are no files in the filebase, then say so and exit now.
			if (filebase.files == 0)
			{
				filebase.close();
				var libIdx = file_area.dir[bbs.curdir_code].lib_index;
				console.crlf();
				console.print("\1n\1cThere are no files in \1h" + file_area.lib_list[libIdx].description + "\1n\1c - \1h" +
							  file_area.dir[bbs.curdir_code].description + "\1n");
				console.crlf();
				console.pause();
				retObj.exitNow = true;
				retObj.exitCode = 0;
				return retObj;
			}

			// To check a user's file basic/extended detail information setting:
			// if ((user.settings & USER_EXTDESC) == USER_EXTDESC)

			// Get a list of file data with normal detail (without extended info).  When the user
			// selects a file to view extended info, we'll get metadata about the file with extended detail.
			gFileList = filebase.get_list("*", FileBase.DETAIL.NORM, 0, true, gFileSortOrder); // FileBase.DETAIL.EXTENDED
			filebase.close();
			// Add a dirCode property to the file metadata objects (for consistency,
			// as file search results may contain files from multiple directories).
			for (var i = 0; i < gFileList.length; ++i)
				gFileList[i].dirCode = bbs.curdir_code;
		}
		else
		{
			console.crlf();
			console.print("\1n\1h\1yUnable to open \1w" + file_area.dir[bbs.curdir_code].description + "\1n");
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

		// Prompt the user for directory, library, or all
		console.print("\1n");
		console.crlf();
		console.mnemonics(bbs.text(DirLibOrAll));
		var validInputOptions = "DLA";
		var userInputDLA = console.getkeys(validInputOptions, -1, K_UPPER);
		var userFilespec = "";
		if (userInputDLA.length > 0 && validInputOptions.indexOf(userInputDLA) > -1)
		{
			// Prompt the user for a filespec to search for
			console.mnemonics(bbs.text(FileSpecStarDotStar));
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

		// Prompt the user for directory, library, or all
		console.print("\1n");
		console.crlf();
		//console.print("\r\n\1c\1hFind Text in File Descriptions (no wildcards)\1n\r\n");
		console.mnemonics(bbs.text(DirLibOrAll));
		console.print("\1n");
		var validInputOptions = "DLA";
		var userInputDLA = console.getkeys(validInputOptions, -1, K_UPPER);
		var searchDescription = "";
		if (userInputDLA.length > 0 && validInputOptions.indexOf(userInputDLA) > -1)
		{
			// Prompt the user for a description to search for
			console.mnemonics(bbs.text(SearchStringPrompt));
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

		// Prompt the user for directory, library, or all
		console.print("\1n");
		console.crlf();
		console.mnemonics(bbs.text(DirLibOrAll));
		var validInputOptions = "DLA";
		var userInputDLA = console.getkeys(validInputOptions, -1, K_UPPER);
		console.print("\1n");
		console.crlf();
		if (userInputDLA == "D" || userInputDLA == "L" || userInputDLA == "A")
		{
			console.print("\1n\1cSearching for files uploaded after \1h" + system.timestr(bbs.new_file_time) + "\1n");
			console.crlf();
		}
		var searchRetObj = searchDirGroupOrAll(userInputDLA, function(pDirCode) {
			return searchDirNewFiles(pDirCode, bbs.new_file_time);
		});
		// Now bbs.last_new_file_time needs to be updated with the current time
		bbs.last_new_file_time = time();
		// user.new_file_time should be updated with the value of bbs.last_new_file_time
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

	if (pSearchMode != MODE_LIST_CURDIR)
		gFileList.allSameDir = allSameDir;

	if (dirErrors.length > 0)
	{
		console.print("\1n\1y\1h");
		for (var i = 0; i < dirErrors.length; ++i)
		{
			console.print(dirErrors[i]);
			console.crlf();
		}
		console.print("\1n");
		console.pause();
		retObj.exitNow = true;
		retObj.exitCode = 1;
		return retObj;
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
		var fileList = filebase.get_list(pFilespec, FileBase.DETAIL.NORM, 0, true, gFileSortOrder); // Or EXTENDED
		retObj.foundFiles = (fileList.length > 0);
		filebase.close();
		for (var i = 0; i < fileList.length; ++i)
		{
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
		var fileList = filebase.get_list("*", FileBase.DETAIL.EXTENDED, 0, true, gFileSortOrder);
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
		var fileList = filebase.get_list("*", FileBase.DETAIL.NORM, 0, true, gFileSortOrder);
		filebase.close();
		for (var i = 0; i < fileList.length; ++i)
		{
			if (fileList[i].added >= pSinceTime)
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
		var searchRetObj = pDirSearchFn(bbs.curdir_code);
		retObj.allSameDir = true;
		for (var i = 0; i < searchRetObj.dirErrors.length; ++i)
			retObj.errors.push(searchRetObj.dirErrors[i]);
	}
	else if (searchOptionUpper == "L")
	{
		// Current file library
		var libIdx = file_area.dir[bbs.curdir_code].lib_index;
		for (var dirIdx = 0; dirIdx < file_area.lib_list[libIdx].dir_list.length; ++dirIdx)
		{
			if (file_area.lib_list[libIdx].dir_list[dirIdx].can_access) // And can_download?
			{
				if (gSearchVerbose)
				{
					console.print(" " + file_area.lib_list[libIdx].dir_list[dirIdx].description + "..");
					console.crlf();
				}
				lastDirCode = (gFileList.length > 0 ? gFileList[gFileList.length-1].dirCode : "");
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
					lastDirCode = (gFileList.length > 0 ? gFileList[gFileList.length-1].dirCode : "");
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
		retObj.errors.push("Invalid search option" + (pSearchOption.length > 0 ? ": " + pSearchOption : ""));
	}

	return retObj;
}