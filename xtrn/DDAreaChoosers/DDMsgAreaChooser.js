/* This is a script that lets the user choose a message area,
 * with either a lightbar or traditional user interface.
 *
 * Date       Author          Description
 * ... Trimmed comments ...
 * 2019-08-22 Eric Oulashin   Version 1.18
 *                            Added message area searching.
 *                            Also, improved the time to display sub-boards
 *                            with the latest message date & time.
 * 2019-08-24 Eric Oulashin   Version 1.19
 *                            Fixed a bug with 'Next' search when returning from
 *                            sub-board choosing.
 * 2020-04-19 Eric Oulashin   Version 1.20
 *                            For lightbar mode, it now uses DDLightbarMenu
 *                            instead of using internal lightbar code.
 *                                  
*/

/* Command-line arguments:
   1 (argv[0]): Boolean - Whether or not to choose a message group first (default).  If
                false, the user will only be able to choose a different sub-board within
				their current message group.
   2 (argv[1]): Boolean - Whether or not to run the area chooser (if false,
                then this file will just provide the DDMsgAreaChooser class).
*/

var requireFnExists = (typeof(require) === "function");
if (requireFnExists)
{
	require("sbbsdefs.js", "K_NOCRLF");
	require("dd_lightbar_menu.js", "DDLightbarMenu");
}
else
{
	load("sbbsdefs.js");
	load("dd_lightbar_menu.js");
}

// This script requires Synchronet version 3.14 or higher.
// Exit if the Synchronet version is below the minimum.
if (system.version_num < 31400)
{
	var message = "\1n\1h\1y\1i* Warning:\1n\1h\1w Digital Distortion Message Area Chooser "
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
var DD_MSG_AREA_CHOOSER_VERSION = "1.20";
var DD_MSG_AREA_CHOOSER_VER_DATE = "2020-04-19";

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

// gIsSysop stores whether or not the user is a sysop.
var gIsSysop = user.compare_ars("SYSOP"); // Whether or not the user is a sysop

// 1st command-line argument: Whether or not to choose a message group first (if
// false, then only choose a sub-board within the user's current group).  This
// can be true or false.
var chooseMsgGrp = true;
if (typeof(argv[0]) == "boolean")
	chooseMsgGrp = argv[0];
else if (typeof(argv[0]) == "string")
	chooseMsgGrp = (argv[0].toLowerCase() == "true");

// 2nd command-line argument: Determine whether or not to execute the message listing
// code (true/false)
var executeThisScript = true;
if (typeof(argv[1]) == "boolean")
	executeThisScript = argv[1];
else if (typeof(argv[1]) == "string")
	executeThisScript = (argv[1].toLowerCase() == "true");

// If executeThisScript is true, then create a DDMsgAreaChooser object and use
// it to let the user choose a message area.
if (executeThisScript)
{
	var msgAreaChooser = new DDMsgAreaChooser();
	msgAreaChooser.SelectMsgArea(chooseMsgGrp);
}

// End of script execution

///////////////////////////////////////////////////////////////////////////////////
// DDMsgAreaChooser class stuff

function DDMsgAreaChooser()
{
	// this.colors will be an associative array of colors (indexed by their
	// usage) used for the message group/sub-board lists.
	// Colors for the file & message area lists
	this.colors = {
		areaNum: "\1n\1w\1h",
		desc: "\1n\1c",
		numItems: "\1b\1h",
		header: "\1n\1y\1h",
		subBoardHeader: "\1n\1g",
		areaMark: "\1g\1h",
		latestDate: "\1n\1g",
		latestTime: "\1n\1m",
		// Highlighted colors (for lightbar mode)
		bkgHighlight: "\1" + "4", // Blue background
		areaNumHighlight: "\1w\1h",
		descHighlight: "\1c",
		dateHighlight: "\1w\1h",
		timeHighlight: "\1w\1h",
		numItemsHighlight: "\1w\1h",
		// Lightbar help line colors
		lightbarHelpLineBkg: "\1" + "7",
		lightbarHelpLineGeneral: "\1b",
		lightbarHelpLineHotkey: "\1r",
		lightbarHelpLineParen: "\1m"
	};

	// showImportDates is a boolean to specify whether or not to display the
	// message import dates.  If false, the message written dates will be
	// displayed instead.
	this.showImportDates = true;

	// useLightbarInterface specifies whether or not to use the lightbar
	// interface.  The lightbar interface will still only be used if the
	// user's terminal supports ANSI.
	this.useLightbarInterface = true;

	// Filename base of a header to display above the area list
	this.areaChooserHdrFilenameBase = "msgAreaChgHeader";
	this.areaChooserHdrMaxLines = 5;

	// Whether or not to show the latest message date/time in the
	// sub-board list
	this.showDatesInSubBoardList = true;

	// Set the function pointers for the object
	this.ReadConfigFile = DDMsgAreaChooser_ReadConfigFile;
	this.WriteKeyHelpLine = DDMsgAreaChooser_writeKeyHelpLine;
	this.WriteGrpListHdrLine = DDMsgAreaChooser_writeGrpListTopHdrLine;
	this.WriteSubBrdListHdr1Line = DMsgAreaChooser_writeSubBrdListHdr1Line;
	this.SelectMsgArea = DDMsgAreaChooser_selectMsgArea;
	this.SelectMsgArea_Lightbar = DDMsgAreaChooser_selectMsgArea_Lightbar;
	this.CreateLightbarMsgGrpMenu = DDMsgAreaChooser_CreateLightbarMsgGrpMenu;
	this.CreateLightbarSubBoardMenu = DDMsgAreaChooser_CreateLightbarSubBoardMenu;
	this.SelectMsgArea_Traditional = DDMsgAreaChooser_selectMsgArea_Traditional;
	this.SelectSubBoard_Traditional = DDMsgAreaChooser_selectSubBoard_Traditional;
	this.ListMsgGrps = DDMsgAreaChooser_listMsgGrps_Traditional;
	this.ListSubBoardsInMsgGroup = DDMsgAreaChooser_listSubBoardsInMsgGroup_Traditional;
	// Lightbar-specific functions
	this.WriteMsgGroupLine = DDMsgAreaChooser_writeMsgGroupLine;
	this.updatePageNumInHeader = DDMsgAreaChooser_updatePageNumInHeader;
	this.GetMsgSubBrdLine = DDMsgAreaChooser_GetMsgSubBrdLine;
	// Help screen
	this.ShowHelpScreen = DDMsgAreaChooser_showHelpScreen;
	// Function to build the sub-board printf information for a message
	// group
	this.BuildSubBoardPrintfInfoForGrp = DDMsgAreaChooser_buildSubBoardPrintfInfoForGrp;
	this.DisplayAreaChgHdr = DDMsgAreaChooser_DisplayAreaChgHdr;
	this.WriteLightbarKeyHelpErrorMsg = DDMsgAreaChooser_WriteLightbarKeyHelpErrorMsg;

	// Read the settings from the config file.
	this.ReadConfigFile();
	
	// These variables store the lengths of the various columns displayed in
	// the message group/sub-board lists.
	// Sub-board info field lengths
	this.areaNumLen = 4;
	this.numItemsLen = 4;
	this.dateLen = 10; // i.e., YYYY-MM-DD
	this.timeLen = 8;  // i.e., HH:MM:SS
	// Sub-board name length - This should be 47 for an 80-column display.
	this.subBoardNameLen = console.screen_columns - this.areaNumLen - this.numItemsLen - 5;
	if (this.showDatesInSubBoardList)
		this.subBoardNameLen -= (this.dateLen + this.timeLen + 2);
	// Message group description length (67 chars on an 80-column screen)
	this.msgGrpDescLen = console.screen_columns - this.areaNumLen - this.numItemsLen - 5;

	// printf strings for various things
	// Message group information (printf strings)
	this.msgGrpListPrintfStr = "\1n " + this.colors.areaNum + "%" + this.areaNumLen
	                         + "d " + this.colors.desc + "%-"
	                         + this.msgGrpDescLen + "s " + this.colors.numItems
	                         + "%" + this.numItemsLen + "d";
	this.msgGrpListHilightPrintfStr = "\1n" + this.colors.bkgHighlight + " "
	                                + "\1n" + this.colors.bkgHighlight
	                                + this.colors.areaNumHighlight + "%" + this.areaNumLen
	                                + "d \1n" + this.colors.bkgHighlight
	                                + this.colors.descHighlight + "%-"
	                                + this.msgGrpDescLen + "s \1n" + this.colors.bkgHighlight
	                                + this.colors.numItemsHighlight + "%" + this.numItemsLen
	                                + "d";
	// Message group list header (printf string)
	this.msgGrpListHdrPrintfStr = this.colors.header + "%6s %-"
	                            + +(this.msgGrpDescLen-8) + "s %-12s";
	// Sub-board information header (printf string)
	this.subBoardListHdrPrintfStr = this.colors.header + " %5s %-"
	                              + +(this.subBoardNameLen-3) + "s %-7s";
	if (this.showDatesInSubBoardList)
		this.subBoardListHdrPrintfStr += " %-19s";
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
	// this.subBoardListPrintfInfo will be an array of printf strings
	// for the sub-boards in the message groups.  The index is the
	// message group index.  The sub-board printf information is created
	// on the fly the first time the user lists sub-boards for a message
	// group.
	this.subBoardListPrintfInfo = new Array();

	// areaChangeHdrLines is an array of text lines to use as a header to display
	// above the message area changer lists.
	this.areaChangeHdrLines = loadTextFileIntoArray(this.areaChooserHdrFilenameBase, this.areaChooserHdrMaxLines);
}

// For the DDMsgAreaChooser class: Writes the line of key help at the bottom
// row of the screen.
function DDMsgAreaChooser_writeKeyHelpLine()
{
	console.gotoxy(1, console.screen_rows);
	console.print(this.lightbarKeyHelpText);
}

// For the DDMsgAreaChooser class: Outputs the header line to appear above
// the list of message groups.
//
// Parameters:
//  pNumPages: The number of pages.  This is optional; if this is
//             not passed, then it won't be used.
//  pPageNum: The page number.  This is optional; if this is not passed,
//            then it won't be used.
function DDMsgAreaChooser_writeGrpListTopHdrLine(pNumPages, pPageNum)
{
	var descStr = "Description";
	if ((typeof(pPageNum) == "number") && (typeof(pNumPages) == "number"))
		descStr += "    (Page " + pPageNum + " of " + pNumPages + ")";
	else if ((typeof(pPageNum) == "number") && (typeof(pNumPages) != "number"))
		descStr += "    (Page " + pPageNum + ")";
	else if ((typeof(pPageNum) != "number") && (typeof(pNumPages) == "number"))
		descStr += "    (" + pNumPages + (pNumPages == 1 ? " page)" : " pages)");
	printf(this.msgGrpListHdrPrintfStr, "Group#", descStr, "# Sub-Boards");
	console.cleartoeol("\1n");
}

// For the DDMsgAreaChooser class: Outputs the first header line to appear
// above the sub-board list for a message group.
//
// Parameters:
//  pGrpIndex: The index of the message group (assumed to be valid)
//  pNumPages: The number of pages.  This is optional; if this is
//             not passed, then it won't be used.
//  pPageNum: The page number.  This is optional; if this is not passed,
//            then it won't be used.
function DMsgAreaChooser_writeSubBrdListHdr1Line(pGrpIndex, pNumPages, pPageNum)
{
	var descFormatStr = "\1n" + this.colors.subBoardHeader + "Sub-boards of \1h%-25s     \1n"
	                  + this.colors.subBoardHeader;
	if ((typeof(pPageNum) == "number") && (typeof(pNumPages) == "number"))
		descFormatStr += "(Page " + pPageNum + " of " + pNumPages + ")";
	else if ((typeof(pPageNum) == "number") && (typeof(pNumPages) != "number"))
		descFormatStr += "(Page " + pPageNum + ")";
	else if ((typeof(pPageNum) != "number") && (typeof(pNumPages) == "number"))
		descFormatStr += "(" + pNumPages + (pNumPages == 1 ? " page)" : " pages)");
	printf(descFormatStr, msg_area.grp_list[pGrpIndex].description.substr(0, 25));
	console.cleartoeol("\1n");
}

// For the DDMsgAreaChooser class: Lets the user choose a message group and
// sub-board via numeric input, using a lightbar interface (if enabled and
// if the user's terminal uses ANSI) or a traditional user interface.
//
// Parameters:
//  pChooseGroup: Boolean - Whether or not to choose the message group.  If false,
//                then this will allow choosing a sub-board within the user's
//                current message group.  This is optional; defaults to true.
function DDMsgAreaChooser_selectMsgArea(pChooseGroup)
{
	if (this.useLightbarInterface && console.term_supports(USER_ANSI))
		this.SelectMsgArea_Lightbar(pChooseGroup);
	else
		this.SelectMsgArea_Traditional(pChooseGroup);
}

// For the DDMsgAreaChooser class: Lets the user choose a message group and
// sub-board via numeric input, using a lightbar user interface.
//
// Parameters:
//  pChooseGroup: Boolean - Whether or not to choose the message group.  If false,
//                then this will allow choosing a sub-board within the user's
//                current message group.  This is optional; defaults to true.
//  pGrpIdx: Optional - The group index, if choosing a sub-board
function DDMsgAreaChooser_selectMsgArea_Lightbar(pChooseGroup, pGrpIdx)
{
	// If there are no message groups or sub-boards, then don't let the user
	// choose one.
	if (msg_area.grp_list.length == 0)
	{
		console.clear("\1n");
		console.print("\1y\1hThere are no message groups.\r\n\1p");
		return;
	}
	var chooseGroup = (typeof(pChooseGroup) == "boolean" ? pChooseGroup : true);
	if (!chooseGroup)
	{
		if (typeof(pGrpIdx) != "number")
			return;
		if (msg_area.grp_list[pGrpIdx].sub_list.length == 0)
		{
			console.clear("\1n");
			console.print("\1y\1hThere are no sub-boards in " + msg_area.grp_list[pGrpIdx].description + ".\r\n\1p");
			return;
		}
	}

	// Displays the header & header lines above the list
	function displayListHdrLines(pScreenRow, pChooseMsgGrp, pAreaChooser, pGrpIdx, pNumPages, pPageNum)
	{
		pAreaChooser.DisplayAreaChgHdr(1);
		console.gotoxy(1, pScreenRow);
		if (pChooseMsgGrp)
			pAreaChooser.WriteGrpListHdrLine(pNumPages, pPageNum);
		else
		{
			pAreaChooser.WriteSubBrdListHdr1Line(pGrpIdx);
			console.gotoxy(1, pScreenRow+2);
			if (pAreaChooser.showDatesInSubBoardList)
				printf(pAreaChooser.subBoardListHdrPrintfStr, "Sub #", "Name", "# Posts", "Latest date & time");
			else
				printf(pAreaChooser.subBoardListHdrPrintfStr, "Sub #", "Name", "# Posts");
		}
	}

	// Clear the screen, write the header, help line, and list header(s)
	console.clear("\1n");
	displayListHdrLines(this.areaChangeHdrLines.length, chooseGroup, this, pGrpIdx);
	this.WriteKeyHelpLine();

	// Create the menu and do the user input loop
	var msgAreaMenu = (chooseGroup ? this.CreateLightbarMsgGrpMenu() : this.CreateLightbarSubBoardMenu(pGrpIdx));
	var drawMenu = true;
	var lastSearchText = "";
	var lastSearchFoundIdx = -1;
	var chosenIdx = -1;
	var continueOn = true;
	// Let the user choose a group, and also respond to other user choices
	while (continueOn)
	{
		chosenIdx = -1;
		var returnedMenuIdx = msgAreaMenu.GetVal(drawMenu);
		drawMenu = true;
		var lastUserInputUpper = (typeof(msgAreaMenu.lastUserInput) == "string" ? msgAreaMenu.lastUserInput.toUpperCase() : msgAreaMenu.lastUserInput);
		if (typeof(returnedMenuIdx) == "number")
			chosenIdx = returnedMenuIdx;
		// If userChoice is not a number, then it should be null in this case,
		// and the user would have pressed one of the additional quit keys set
		// up for the menu.  So look at the menu's lastUserInput and do the
		// appropriate thing.
		else if ((lastUserInputUpper == "Q") || (lastUserInputUpper == KEY_ESC)) // Quit
			continueOn = false;
		else if ((lastUserInputUpper == "/") || (lastUserInputUpper == CTRL_F)) // Start of find
		{
			console.gotoxy(1, console.screen_rows);
			console.cleartoeol("\1n");
			console.gotoxy(1, console.screen_rows);
			var promptText = "Search: ";
			console.print(promptText);
			var searchText = getStrWithTimeout(K_UPPER|K_NOCRLF|K_GETSTR|K_NOSPIN|K_LINE, console.screen_columns - promptText.length - 1, SEARCH_TIMEOUT_MS);
			lastSearchText = searchText;
			// If the user entered text, then do the search, and if found,
			// found, go to the page and select the item indicated by the
			// search.
			if (searchText.length > 0)
			{
				var oldLastSearchFoundIdx = lastSearchFoundIdx;
				var oldSelectedItemIdx = msgAreaMenu.selectedItemIdx;
				var idx = -1;
				if (chooseGroup)
					idx = findMsgGrpIdxFromText(searchText, msgAreaMenu.selectedItemIdx);
				else
					idx = findSubBoardIdxFromText(pGrpIdx, searchText, msgAreaMenu.selectedItemIdx+1);
				lastSearchFoundIdx = idx;
				if (idx > -1)
				{
					// Set the currently selected item in the menu, and ensure it's
					// visible on the page
					msgAreaMenu.selectedItemIdx = idx;
					if (msgAreaMenu.selectedItemIdx >= msgAreaMenu.topItemIdx+msgAreaMenu.GetNumItemsPerPage())
						msgAreaMenu.topItemIdx = msgAreaMenu.selectedItemIdx - msgAreaMenu.GetNumItemsPerPage() + 1;
					else if (msgAreaMenu.selectedItemIdx < msgAreaMenu.topItemIdx)
						msgAreaMenu.topItemIdx = msgAreaMenu.selectedItemIdx;
					else
					{
						// If the current index and the last index are both on the same page on the
						// menu, then have the menu only redraw those items.
						msgAreaMenu.nextDrawOnlyItems = [msgAreaMenu.selectedItemIdx, oldLastSearchFoundIdx, oldSelectedItemIdx];
					}
				}
				else
				{
					if (chooseGroup)
						idx = findMsgGrpIdxFromText(searchText, 0);
					else
						idx = findSubBoardIdxFromText(pGrpIdx, searchText, 0);
					lastSearchFoundIdx = idx;
					if (idx > -1)
					{
						// Set the currently selected item in the menu, and ensure it's
						// visible on the page
						msgAreaMenu.selectedItemIdx = idx;
						if (msgAreaMenu.selectedItemIdx >= msgAreaMenu.topItemIdx+msgAreaMenu.GetNumItemsPerPage())
							msgAreaMenu.topItemIdx = msgAreaMenu.selectedItemIdx - msgAreaMenu.GetNumItemsPerPage() + 1;
						else if (msgAreaMenu.selectedItemIdx < msgAreaMenu.topItemIdx)
							msgAreaMenu.topItemIdx = msgAreaMenu.selectedItemIdx;
						else
						{
							// The current index and the last index are both on the same page on the
							// menu, so have the menu only redraw those items.
							msgAreaMenu.nextDrawOnlyItems = [msgAreaMenu.selectedItemIdx, oldLastSearchFoundIdx, oldSelectedItemIdx];
						}
					}
					else
					{
						this.WriteLightbarKeyHelpErrorMsg("Not found");
						drawMenu = false;
					}
				}
			}
			else
				drawMenu = false;
			this.WriteKeyHelpLine();
		}
		else if (lastUserInputUpper == "N") // Next search result (requires an existing search term)
		{
			// This works but seems a little strange sometimes.
			// - Should this always start from the selected index?
			// - If it wraps around to one of the items on the first page,
			//   should it always set the top index to 0?
			if ((lastSearchText.length > 0) && (lastSearchFoundIdx > -1))
			{
				var oldLastSearchFoundIdx = lastSearchFoundIdx;
				var oldSelectedItemIdx = msgAreaMenu.selectedItemIdx;
				// Do the search, and if found, go to the page and select the item
				// indicated by the search.
				var idx = 0;
				if (chooseGroup)
					idx = findMsgGrpIdxFromText(searchText, lastSearchFoundIdx+1);
				else
					idx = findSubBoardIdxFromText(pGrpIdx, searchText, lastSearchFoundIdx+1);
				if (idx > -1)
				{
					lastSearchFoundIdx = idx;
					// Set the currently selected item in the menu, and ensure it's
					// visible on the page
					msgAreaMenu.selectedItemIdx = idx;
					if (msgAreaMenu.selectedItemIdx >= msgAreaMenu.topItemIdx+msgAreaMenu.GetNumItemsPerPage())
					{
						msgAreaMenu.topItemIdx = msgAreaMenu.selectedItemIdx - msgAreaMenu.GetNumItemsPerPage() + 1;
						if (msgAreaMenu.topItemIdx < 0)
							msgAreaMenu.topItemIdx = 0;
					}
					else if (msgAreaMenu.selectedItemIdx < msgAreaMenu.topItemIdx)
						msgAreaMenu.topItemIdx = msgAreaMenu.selectedItemIdx;
					else
					{
						// The current index and the last index are both on the same page on the
						// menu, so have the menu only redraw those items.
						msgAreaMenu.nextDrawOnlyItems = [msgAreaMenu.selectedItemIdx, oldLastSearchFoundIdx, oldSelectedItemIdx];
					}
				}
				else
				{
					if (chooseGroup)
						idx = findMsgGrpIdxFromText(searchText, 0);
					else
						idx = findSubBoardIdxFromText(pGrpIdx, searchText, 0);
					lastSearchFoundIdx = idx;
					if (idx > -1)
					{
						// Set the currently selected item in the menu, and ensure it's
						// visible on the page
						msgAreaMenu.selectedItemIdx = idx;
						if (msgAreaMenu.selectedItemIdx >= msgAreaMenu.topItemIdx+msgAreaMenu.GetNumItemsPerPage())
						{
							msgAreaMenu.topItemIdx = msgAreaMenu.selectedItemIdx - msgAreaMenu.GetNumItemsPerPage() + 1;
							if (msgAreaMenu.topItemIdx < 0)
								msgAreaMenu.topItemIdx = 0;
						}
						else if (msgAreaMenu.selectedItemIdx < msgAreaMenu.topItemIdx)
							msgAreaMenu.topItemIdx = msgAreaMenu.selectedItemIdx;
						else
						{
							// The current index and the last index are both on the same page on the
							// menu, so have the menu only redraw those items.
							msgAreaMenu.nextDrawOnlyItems = [msgAreaMenu.selectedItemIdx, oldLastSearchFoundIdx, oldSelectedItemIdx];
						}
					}
					else
					{
						this.WriteLightbarKeyHelpErrorMsg("Not found");
						drawMenu = false;
						this.WriteKeyHelpLine();
					}
				}
			}
			else
			{
				this.WriteLightbarKeyHelpErrorMsg("There is no previous search", REFRESH_MSG_AREA_CHG_LIGHTBAR_HELP_LINE);
				drawMenu = false;
				this.WriteKeyHelpLine();
			}
		}
		else if (lastUserInputUpper == "?") // Show help
		{
			this.ShowHelpScreen(true, true);
			console.pause();
			// Refresh the screen
			console.clear("\1n");
			this.DisplayAreaChgHdr(1);
			displayListHdrLines(this.areaChangeHdrLines.length, chooseGroup, this, pGrpIdx);
			this.WriteKeyHelpLine();
		}
		// If the user entered a numeric digit, then treat it as
		// the start of the message group number.
		if (lastUserInputUpper.match(/[0-9]/))
		{
			// Put the user's input back in the input buffer to
			// be used for getting the rest of the message number.
			console.ungetstr(lastUserInputUpper);
			// Move the cursor to the bottom of the screen and
			// prompt the user for the message number.
			console.gotoxy(1, console.screen_rows);
			console.clearline("\1n");
			console.print("\1cChoose group #: \1h");
			var userInput = console.getnum(msg_area.grp_list.length);
			if (userInput > 0)
				chosenIdx = userInput - 1;
			else
			{
				// The user didn't make a selection.  So, we need to refresh
				// the screen due to everything being moved up one line.
				displayListHdrLines(this.areaChangeHdrLines.length, chooseGroup, this, pGrpIdx);
				this.WriteKeyHelpLine();
			}
		}

		// If a group/sub-board was chosen, then deal with it.
		if (chosenIdx > -1)
		{
			// If choosing a message group, then let the user choose a
			// sub-board within the group.  Otherwise, return the user's
			// chosen sub-board.
			if (chooseGroup)
			{
				// Show a "Loading..." text in case there are many sub-boards in
				// the chosen message group
				console.crlf();
				console.print("\1nLoading...");
				console.line_counter = 0; // To prevent a pause before the message list comes up
				// Ensure that the sub-board printf information is created for
				// the chosen message group.
				this.BuildSubBoardPrintfInfoForGrp(chosenIdx);
				var chosenSubBoardIdx = this.SelectMsgArea_Lightbar(false, chosenIdx);
				if (chosenSubBoardIdx > -1)
				{
					// Set the current sub-board
					bbs.cursub_code = msg_area.grp_list[chosenIdx].sub_list[chosenSubBoardIdx].code;
					continueOn = false;
				}
				else
				{
					// A sub-board was not chosen, so we'll have to re-draw
					// the header and list of message groups.
					displayListHdrLines(this.areaChangeHdrLines.length, chooseGroup, this, pGrpIdx);
				}
			}
			else
				return chosenIdx; // Return the chosen sub-board index
		}
	}
}

// For the DDMsgAreaChooser class: Creates the DDLightbarMenu for use with
// choosing a message group in lightbar mode.
//
// Return value: A DDLightbarMenu object for choosing a message group
function DDMsgAreaChooser_CreateLightbarMsgGrpMenu()
{
	// Start & end indexes for the various items in each mssage group list row
	// Selected mark, group#, description, # sub-boards
	var msgGrpListIdxes = {
		markCharStart: 0,
		markCharEnd: 1,
		grpNumStart: 1,
		grpNumEnd: 2 + (+this.areaNumLen)
	};
	msgGrpListIdxes.descStart = msgGrpListIdxes.grpNumEnd;
	msgGrpListIdxes.descEnd = msgGrpListIdxes.descStart + +this.msgGrpDescLen;
	msgGrpListIdxes.numItemsStart = msgGrpListIdxes.descEnd;
	// Set numItemsEnd to -1 to let the whole rest of the lines be colored
	msgGrpListIdxes.numItemsEnd = -1;
	var listStartRow = this.areaChangeHdrLines.length + 2;
	var msgGrpMenuHeight = console.screen_rows - listStartRow;
	var msgGrpMenu = new DDLightbarMenu(1, listStartRow, console.screen_columns, msgGrpMenuHeight);
	msgGrpMenu.scrollbarEnabled = true;
	msgGrpMenu.borderEnabled = false;
	msgGrpMenu.SetColors({
		itemColor: [{start: msgGrpListIdxes.markCharStart, end: msgGrpListIdxes.markCharEnd, attrs: this.colors.areaMark},
		            {start: msgGrpListIdxes.grpNumStart, end: msgGrpListIdxes.grpNumEnd, attrs: this.colors.areaNum},
		            {start: msgGrpListIdxes.descStart, end: msgGrpListIdxes.descEnd, attrs: this.colors.desc},
		            {start: msgGrpListIdxes.numItemsStart, end: msgGrpListIdxes.numItemsEnd, attrs: this.colors.numItems}],
		selectedItemColor: [{start: msgGrpListIdxes.markCharStart, end: msgGrpListIdxes.markCharEnd, attrs: this.colors.areaMark + this.colors.bkgHighlight},
		                    {start: msgGrpListIdxes.grpNumStart, end: msgGrpListIdxes.grpNumEnd, attrs: this.colors.areaNumHighlight + this.colors.bkgHighlight},
		                    {start: msgGrpListIdxes.descStart, end: msgGrpListIdxes.descEnd, attrs: this.colors.descHighlight + this.colors.bkgHighlight},
		                    {start: msgGrpListIdxes.numItemsStart, end: msgGrpListIdxes.numItemsEnd, attrs: this.colors.numItemsHighlight + this.colors.bkgHighlight}]
	});

	msgGrpMenu.multiSelect = false;
	msgGrpMenu.ampersandHotkeysInItems = false;
	msgGrpMenu.wrapNavigation = false;

	// Add additional keypresses for quitting the menu's input loop so we can
	// respond to these keys
	msgGrpMenu.AddAdditionalQuitKeys("nNqQ ?0123456789/" + CTRL_F);

	// Change the menu's NumItems() and GetItem() function to reference
	// the message list in this object rather than add the menu items
	// to the menu
	msgGrpMenu.areaChooser = this; // Add this object to the menu object
	msgGrpMenu.NumItems = function() {
		return msg_area.grp_list.length;
	};
	msgGrpMenu.GetItem = function(pGrpIndex) {
		var menuItemObj = this.MakeItemWithRetval(-1);
		if ((pGrpIndex >= 0) && (pGrpIndex < msg_area.grp_list.length))
		{
			menuItemObj.text = (((typeof(bbs.curgrp) == "number") && (pGrpIndex == msg_area.sub[bbs.cursub_code].grp_index)) ? "*" : " ");
			menuItemObj.text += format(this.areaChooser.msgGrpListPrintfStr, +(pGrpIndex+1),
			                           msg_area.grp_list[pGrpIndex].description.substr(0, this.areaChooser.msgGrpDescLen),
			                           msg_area.grp_list[pGrpIndex].sub_list.length);
			menuItemObj.text = strip_ctrl(menuItemObj.text);
			menuItemObj.retval = pGrpIndex;
		}

		return menuItemObj;
	};

	// Set the currently selected item to the current group
	msgGrpMenu.selectedItemIdx = msg_area.sub[bbs.cursub_code].grp_index;
	if (msgGrpMenu.selectedItemIdx >= msgGrpMenu.topItemIdx+msgGrpMenu.GetNumItemsPerPage())
		msgGrpMenu.topItemIdx = msgGrpMenu.selectedItemIdx - msgGrpMenu.GetNumItemsPerPage() + 1;

	return msgGrpMenu;
}

// For the DDMsgAreaChooser class: Creates the DDLightbarMenu for use with
// choosing a sub-board in lightbar mode.
//
// Parameters:
//  pGrpIdx: The index of the message group
//
// Return value: A DDLightbarMenu object for choosing a sub-board within
// the given message group
function DDMsgAreaChooser_CreateLightbarSubBoardMenu(pGrpIdx)
{
	// Start & end indexes for the various items in each mssage group list row
	// Selected mark, group#, description, # sub-boards
	var subBoardListIdxes = {
		markCharStart: 0,
		markCharEnd: 1,
		subNumStart: 1,
		subNumEnd: 3 + (+this.areaNumLen)
	};
	subBoardListIdxes.descStart = subBoardListIdxes.subNumEnd;
	subBoardListIdxes.descEnd = subBoardListIdxes.descStart + +this.subBoardNameLen + 1;
	subBoardListIdxes.numItemsStart = subBoardListIdxes.descEnd;
	subBoardListIdxes.numItemsEnd = subBoardListIdxes.numItemsStart + +this.numItemsLen + 1;
	subBoardListIdxes.dateStart = subBoardListIdxes.numItemsEnd;
	subBoardListIdxes.dateEnd = subBoardListIdxes.dateStart + +this.dateLen + 1;
	subBoardListIdxes.timeStart = subBoardListIdxes.dateEnd;
	// Set timeEnd to -1 to let the whole rest of the lines be colored
	subBoardListIdxes.timeEnd = -1;
	var listStartRow = this.areaChangeHdrLines.length + 3; // or + 2?
	var subBoardMenuHeight = console.screen_rows - listStartRow;
	var subBoardMenu = new DDLightbarMenu(1, listStartRow, console.screen_columns, subBoardMenuHeight);
	subBoardMenu.scrollbarEnabled = true;
	subBoardMenu.borderEnabled = false;
	subBoardMenu.SetColors({
		itemColor: [{start: subBoardListIdxes.markCharStart, end: subBoardListIdxes.markCharEnd, attrs: this.colors.areaMark},
		            {start: subBoardListIdxes.subNumStart, end: subBoardListIdxes.subNumEnd, attrs: this.colors.areaNum},
		            {start: subBoardListIdxes.descStart, end: subBoardListIdxes.descEnd, attrs: this.colors.desc},
		            {start: subBoardListIdxes.numItemsStart, end: subBoardListIdxes.numItemsEnd, attrs: this.colors.numItems},
		            {start: subBoardListIdxes.dateStart, end: subBoardListIdxes.dateEnd, attrs: this.colors.latestDate},
		            {start: subBoardListIdxes.timeStart, end: subBoardListIdxes.timeEnd, attrs: this.colors.latestTime}],
		selectedItemColor: [{start: subBoardListIdxes.markCharStart, end: subBoardListIdxes.markCharEnd, attrs: this.colors.areaMark + this.colors.bkgHighlight},
		                    {start: subBoardListIdxes.subNumStart, end: subBoardListIdxes.subNumEnd, attrs: this.colors.areaNumHighlight + this.colors.bkgHighlight},
		                    {start: subBoardListIdxes.descStart, end: subBoardListIdxes.descEnd, attrs: this.colors.descHighlight + this.colors.bkgHighlight},
		                    {start: subBoardListIdxes.numItemsStart, end: subBoardListIdxes.numItemsEnd, attrs: this.colors.numItemsHighlight + this.colors.bkgHighlight},
		                    {start: subBoardListIdxes.dateStart, end: subBoardListIdxes.dateEnd, attrs: this.colors.dateHighlight + this.colors.bkgHighlight},
		                    {start: subBoardListIdxes.timeStart, end: subBoardListIdxes.timeEnd, attrs: this.colors.timeHighlight + this.colors.bkgHighlight}]
	});

	subBoardMenu.multiSelect = false;
	subBoardMenu.ampersandHotkeysInItems = false;
	subBoardMenu.wrapNavigation = false;

	// Add additional keypresses for quitting the menu's input loop so we can
	// respond to these keys
	subBoardMenu.AddAdditionalQuitKeys("nNqQ ?0123456789/" + CTRL_F);

	// Change the menu's NumItems() and GetItem() function to reference
	// the message list in this object rather than add the menu items
	// to the menu
	subBoardMenu.areaChooser = this; // Add this object to the menu object
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
			menuItemObj.text = strip_ctrl(this.areaChooser.GetMsgSubBrdLine(this.grpIdx, pSubIdx, false));
			menuItemObj.retval = pSubIdx;
		}

		return menuItemObj;
	};

	// Set the currently selected item.  If the current sub-board is in this list,
	// then set the selected item to that; otherwise, the selected item should be
	// the first sub-board.
	if (msg_area.sub[bbs.cursub_code].grp_index == pGrpIdx)
	{
		subBoardMenu.selectedItemIdx = msg_area.sub[bbs.cursub_code].index;
		if (subBoardMenu.selectedItemIdx >= subBoardMenu.topItemIdx+subBoardMenu.GetNumItemsPerPage())
			subBoardMenu.topItemIdx = subBoardMenu.selectedItemIdx - subBoardMenu.GetNumItemsPerPage() + 1;
	}
	else
	{
		subBoardMenu.selectedItemIdx = 0;
		subBoardMenu.topItemIdx = 0;
	}

	return subBoardMenu;
}

// For the DDMsgAreaChooser class: Lets the user choose a message group and
// sub-board via numeric input, using a traditional user interface.
//
// Parameters:
//  pChooseGroup: Boolean - Whether or not to choose the message group.  If false,
//                then this will allow choosing a sub-board within the user's
//                current message group.  This is optional; defaults to true.
function DDMsgAreaChooser_selectMsgArea_Traditional(pChooseGroup)
{
	// If there are no message groups, then don't let the user
	// choose one.
	if (msg_area.grp_list.length == 0)
	{
		console.clear("\1n");
		console.print("\1y\1hThere are no message groups.\r\n\1p");
		return;
	}

	var chooseGroup = (typeof(pChooseGroup) == "boolean" ? pChooseGroup : true);
	if (chooseGroup)
	{
		// Show the message groups & sub-boards and let the user choose one.
		var selectedGrp = 0;      // The user's selected message group
		var selectedSubBoard = 0; // The user's selected sub-board
		var usersCurrentIdxVals = getGrpAndSubIdxesFromCode(bbs.cursub_code, true);
		var grpSearchText = "";
		var continueChoosingMsgArea = true;
		while (continueChoosingMsgArea)
		{
			// Clear the BBS command string to make sure there are no extra
			// commands in there that could cause weird things to happen.
			bbs.command_str = "";

			console.clear("\1n");
			this.DisplayAreaChgHdr(1);
			if (this.areaChangeHdrLines.length > 0)
				console.crlf();
			this.ListMsgGrps(grpSearchText);
			console.crlf();
			console.print("\1n\1b\1hþ \1n\1cWhich, \1hQ\1n\1cuit, \1hCTRL-F\1n\1c, \1h/\1n\1c, or [\1h" + +(usersCurrentIdxVals.grpIdx+1) + "\1n\1c]: \1h");
			// Accept Q (quit), / or CTRL_F (Search) or a file library number
			selectedGrp = console.getkeys("Q/" + CTRL_F, msg_area.grp_list.length);

			// If the user just pressed enter (selectedGrp would be blank),
			// default to the current group.
			if (selectedGrp.toString() == "")
				selectedGrp = usersCurrentIdxVals.grpIdx + 1;

			if (selectedGrp.toString() == "Q")
				continueChoosingMsgArea = false;
			else if ((selectedGrp.toString() == "/") || (selectedGrp.toString() == CTRL_F))
			{
				console.crlf();
				var searchPromptText = "\1n\1c\1hSearch\1g: \1n";
				console.print(searchPromptText);
				var searchText = console.getstr("", console.screen_columns-strip_ctrl(searchPromptText).length-1, K_UPPER|K_NOCRLF|K_GETSTR|K_NOSPIN|K_LINE);
				if (searchText.length > 0)
					grpSearchText = searchText;
			}
			else
			{
				grpSearchText = "";

				// If the user specified a message group number, then
				// set it and let the user choose a sub-board within
				// the group.
				if (selectedGrp > 0)
				{
					// Set the default sub-board #: The current sub-board, or if the
					// user chose a different group, then this should be set
					// to the first sub-board.
					var defaultSubBoard = usersCurrentIdxVals.subIdx + 1;
					if (selectedGrp-1 != usersCurrentIdxVals.grpIdx)
						defaultSubBoard = 1;

					console.clear("\1n");
					var selectSubRetVal = this.SelectSubBoard_Traditional(selectedGrp-1, defaultSubBoard-1);
					// If the user chose a directory, then set the user's
					// message sub-board and quit the message group loop.
					if (selectSubRetVal.subBoardCode != "")
					{
						continueChoosingMsgArea = false;
						bbs.cursub_code = selectSubRetVal.subBoardCode;
					}
				}
			}
		}
	}
	else
	{
		// Don't choose a group, just a sub-board within the user's current group.
		var idxVals = getGrpAndSubIdxesFromCode(bbs.cursub_code, true);
		var selectSubRetVal = this.SelectSubBoard_Traditional(idxVals.grpIdx, idxVals.subIdx);
		// If the user chose a directory, then set the user's sub-board.
		if (selectSubRetVal.subBoardCode != "")
			bbs.cursub_code = selectSubRetVal.subBoardCode;
	}
}

// For the DDMsgAreaChooser class: Allows the user to select a sub-board with the
// traditional user interface.
//
// Parameters:
//  pGrpIdx: The index of the message group to choose a sub-board for
//  pDefaultSubBoardIdx: The index of the default sub-board
//
// Return value: An object containing the following values:
//               subBoardChosen: Boolean - Whether or not a sub-board was chosen.
//               subBoardIndex: Numeric - The sub-board that was chosen (if any).
//                              Will be -1 if none chosen.
//               subBoardCode: The internal code of the chosen sub-board (or "" if none chosen)
function DDMsgAreaChooser_selectSubBoard_Traditional(pGrpIdx, pDefaultSubBoardIdx)
{
	var retObj = {
		subBoardChosen: false,
		subBoardIndex: 1,
		subBoardCode: ""
	}

	var searchText = "";
	var defaultSubBoardIdx = pDefaultSubBoardIdx;
	var continueOn = false;
	do
	{
		this.DisplayAreaChgHdr(1);
		if (this.areaChangeHdrLines.length > 0)
			console.crlf();
		this.ListSubBoardsInMsgGroup(pGrpIdx, defaultSubBoardIdx, searchText);
		console.crlf();
		if (defaultSubBoardIdx >= 0)
			console.print("\1n\1b\1hþ \1n\1cWhich, \1hQ\1n\1cuit, \1hCTRL-F\1n\1c, \1h/\1n\1c, or [\1h" + +(defaultSubBoardIdx+1) + "\1n\1c]: \1h");
		else
			console.print("\1n\1b\1hþ \1n\1cWhich, \1hQ\1n\1cuit, \1hCTRL-F\1n\1c, \1h/\1n\1c: \1h");
		// Accept Q (quit) or a sub-board number
		var selectedSubBoard = console.getkeys("Q/" + CTRL_F, msg_area.grp_list[pGrpIdx].sub_list.length);

		// If the user just pressed enter (selectedSubBoard would be blank),
		// default the selected directory.
		var selectedSubBoardStr = selectedSubBoard.toString();
		if (selectedSubBoardStr == "")
		{
			if (defaultSubBoardIdx >= 0)
			{
				selectedSubBoard = defaultSubBoardIdx + 1; // Make this 1-based
				continueOn = false;
			}
		}
		else if ((selectedSubBoardStr == "/") || (selectedSubBoardStr == CTRL_F))
		{
			// Search
			console.crlf();
			var searchPromptText = "\1n\1c\1hSearch\1g: \1n";
			console.print(searchPromptText);
			searchText = console.getstr("", console.screen_columns-strip_ctrl(searchPromptText).length-1, K_UPPER|K_NOCRLF|K_GETSTR|K_NOSPIN|K_LINE);
			console.print("\1n");
			console.crlf();
			if (searchText.length > 0)
				defaultSubBoardIdx = -1;
			else
				defaultSubBoardIdx = pDefaultSubBoardIdx;
			continueOn = true;
		}
		else if (selectedSubBoardStr == "Q")
			continueOn = false;

		// If a sub-board was chosen, then select it.
		if (selectedSubBoard > 0)
		{
			retObj.subBoardChosen = true;
			retObj.subBoardIndex = selectedSubBoard - 1;
			retObj.subBoardCode = msg_area.grp_list[pGrpIdx].sub_list[retObj.subBoardIndex].code;
			continueOn = false;
		}
	} while (continueOn);

	return retObj;
}

// For the DDMsgAreaChooser class: Lists all message groups (for the traditional
// user interface).
//
// Parameters:
//  pSearchText: Optional - Search text for the message groups
function DDMsgAreaChooser_listMsgGrps_Traditional(pSearchText)
{
	// Print the header
	this.WriteGrpListHdrLine();
	console.print("\1n");

	var searchText = (typeof(pSearchText) == "string" ? pSearchText.toUpperCase() : "");

	// List the message groups
	var printIt = true;
	for (var i = 0; i < msg_area.grp_list.length; ++i)
	{
		if (searchText.length > 0)
			printIt = ((msg_area.grp_list[i].name.toUpperCase().indexOf(searchText) >= 0) || (msg_area.grp_list[i].description.toUpperCase().indexOf(searchText) >= 0));
		else
			printIt = true;

		if (printIt)
		{
			console.crlf();
			this.WriteMsgGroupLine(i, false);
		}
	}
}

// For the DDMsgAreaChooser class: Lists the sub-boards in a message group,
// for the traditional user interface.
//
// Parameters:
//  pGrpIndex: The index of the message group (0-based)
//  pMarkIndex: An index of a message group to highlight.  This
//                   is optional; if left off, this will default to
//                   the current sub-board.
//  pSearchText: Optional - Search text for the sub-boards
//  pSortType: Optional - A string describing how to sort the list (if desired):
//             "none": Default behavior - Sort by sub-board #
//             "dateAsc": Sort by date, ascending
//             "dateDesc": Sort by date, descending
//             "description": Sort by description
function DDMsgAreaChooser_listSubBoardsInMsgGroup_Traditional(pGrpIndex, pMarkIndex, pSearchText, pSortType)
{
	// Default to the current message group & sub-board if pGrpIndex
	// and pMarkIndex aren't specified.
	var grpIndex = 0;
	if ((typeof(bbs.cursub_code) == "string") && (bbs.cursub_code != ""))
		grpIndex = msg_area.sub[bbs.cursub_code].grp_index;
	if ((pGrpIndex != null) && (typeof(pGrpIndex) == "number"))
		grpIndex = pGrpIndex;
	var highlightIndex = 0;
	if ((typeof(bbs.cursub_code) == "string") && (bbs.cursub_code != ""))
		highlightIndex = (pGrpIndex == msg_area.sub[bbs.cursub_code].index);
	if ((pMarkIndex != null) && (typeof(pMarkIndex) == "number"))
		highlightIndex = pMarkIndex;

	// Make sure grpIndex and highlightIndex are valid (they might not be for
	// brand-new users).
	if ((grpIndex == null) || (typeof(grpIndex) == "undefined"))
		grpIndex = 0;
	if ((highlightIndex == null) || (typeof(highlightIndex) == "undefined"))
		highlightIndex = 0;

	// Ensure that the sub-board printf information is created for
	// this message group.
	this.BuildSubBoardPrintfInfoForGrp(grpIndex);

	// Print the headers
	this.WriteSubBrdListHdr1Line(grpIndex);
	console.crlf();
	if (this.showDatesInSubBoardList)
		printf(this.subBoardListHdrPrintfStr, "Sub #", "Name", "# Posts", "Latest date & time");
	else
		printf(this.subBoardListHdrPrintfStr, "Sub #", "Name", "# Posts");
	console.print("\1n");

	// Make the search text uppercase for case-insensitive matching
	var searchTextUpper = (typeof(pSearchText) == "string" ? pSearchText.toUpperCase() : "");

	// List each sub-board in the message group.
	var subBoardArray = null;       // For sorting, if desired
	var newestDate = {}; // For storing the date of the newest post in a sub-board
	var msgBase = null;    // For opening the sub-boards with a MsgBase object
	var msgHeader = null;  // For getting the date & time of the newest post in a sub-board
	var subBoardNum = 0;   // 0-based sub-board number (because the array index is the number as a str)
	// If a sort type is specified, then add the sub-board information to
	// subBoardArray so that it can be sorted.
	if ((typeof(pSortType) == "string") && (pSortType != "") && (pSortType != "none"))
	{
		var addSubBoard = true;
		subBoardArray = [];
		var subBoardInfo = null;
		for (var arrSubBoardNum in msg_area.grp_list[grpIndex].sub_list)
		{
			// Open the current sub-board with the msgBase object.
			// If the search text is set, then use it to filter the sub-boards.
			addSubBoard = true;
			if (searchTextUpper.length > 0)
			{
				addSubBoard = ((msg_area.grp_list[grpIndex].sub_list[arrSubBoardNum].name.indexOf(searchTextUpper) >= 0) ||
				               (msg_area.grp_list[grpIndex].sub_list[arrSubBoardNum].description.indexOf(searchTextUpper) >= 0));
			}
			if (addSubBoard)
			{
				msgBase = new MsgBase(msg_area.grp_list[grpIndex].sub_list[arrSubBoardNum].code);
				if (msgBase.open())
				{
					subBoardInfo = new MsgSubBoardInfo();
					subBoardInfo.subBoardNum = +(arrSubBoardNum);
					subBoardInfo.subBoardIdx = msg_area.grp_list[grpIndex].sub_list[arrSubBoardNum].index;
					subBoardInfo.description = msg_area.grp_list[grpIndex].sub_list[arrSubBoardNum].description;
					//subBoardInfo.numPosts = numReadableMsgs(msgBase, msg_area.grp_list[grpIndex].sub_list[arrSubBoardNum].code);
					subBoardInfo.numPosts = msgBase.total_msgs;

					// Get the date & time when the last message was imported.
					if (this.showDatesInSubBoardList && (subBoardInfo.numPosts > 0))
					{
						//var msgHeader = msgBase.get_msg_header(true, msgBase.total_msgs-1, true);
						var msgHeader = getLatestMsgHdr(msg_area.grp_list[grpIndex].sub_list[arrSubBoardNum].code);
						if (msgHeader === null)
							msgHeader = getBogusMsgHdr();
						if (this.showImportDates)
							subBoardInfo.newestPostDate = msgHeader.when_imported_time
						else
						{
							var msgWrittenLocalBBSTime = msgWrittenTimeToLocalBBSTime(msgHeader);
							if (msgWrittenLocalBBSTime != -1)
								subBoardInfo.newestPostDate = msgWrittenLocalBBSTime;
							else
								subBoardInfo.newestPostDate = msgHeader.when_written_time;
						}
					}
				}
				msgBase.close();
				subBoardArray.push(subBoardInfo);
				delete msgBase; // Free some memory?
			}
		}

		// Sort sub-board list.
		if (pSortType == "dateAsc")
		{
			subBoardArray.sort(function(pA, pB)
			{
				// Return -1, 0, or 1, depending on whether pA's date comes
				// before, is equal to, or comes after pB's date.
				var returnValue = 0;
				if (pA.newestPostDate < pB.newestPostDate)
					returnValue = -1;
				else if (pA.newestPostDate > pB.newestPostDate)
					returnValue = 1;
				return returnValue;
			});
		}
		else if (pSortType == "dateDesc")
		{
			if (this.showDatesInSubBoardList)
			{
				subBoardArray.sort(function(pA, pB)
				{
					// Return -1, 0, or 1, depending on whether pA's date comes
					// after, is equal to, or comes before pB's date.
					var returnValue = 0;
					if (pA.newestPostDate > pB.newestPostDate)
						returnValue = -1;
					else if (pA.newestPostDate < pB.newestPostDate)
						returnValue = 1;
					return returnValue;
				});
			}
		}
		else if (pSortType == "description")
		{
			// Binary safe string comparison  
			// 
			// version: 909.322
			// discuss at: http://phpjs.org/functions/strcmp    // +   original by: Waldo Malqui Silva
			// +      input by: Steve Hilder
			// +   improved by: Kevin van Zonneveld (http://kevin.vanzonneveld.net)
			// +    revised by: gorthaur
			// *     example 1: strcmp( 'waldo', 'owald' );    // *     returns 1: 1
			// *     example 2: strcmp( 'owald', 'waldo' );
			// *     returns 2: -1
			subBoardArray.sort(function(pA, pB)
			{
				return ((pA.description == pB.description) ? 0 : ((pA.description > pB.description) ? 1 : -1));
			});
		}

		// Display the sub-board list.
		for (var i = 0; i < subBoardArray.length; ++i)
		{
			console.crlf();
			var showSubBoardMark = false;
			if ((typeof(bbs.cursub_code) == "string") && (bbs.cursub_code != ""))
			{
				if (subBoardArray[i].subBoardNum == highlightIndex)
					showSubBoardMark = ((grpIndex == msg_area.sub[bbs.cursub_code].grp_index) && (highlightIndex == subBoardArray[i].subBoardIdx));
			}
			console.print(showSubBoardMark ? "\1n" + this.colors.areaMark + "*" : " ");
			if (this.showDatesInSubBoardList)
			{
				printf(this.subBoardListPrintfInfo[grpIndex].printfStr, +(subBoardArray[i].subBoardNum+1),
				       subBoardArray[i].description.substr(0, this.subBoardNameLen),
				       subBoardArray[i].numPosts, strftime("%Y-%m-%d", subBoardArray[i].newestPostDate),
				       strftime("%H:%M:%S", subBoardArray[i].newestPostDate));
			}
			else
			{
				printf(this.subBoardListPrintfInfo[grpIndex].printfStr, +(subBoardArray[i].subBoardNum+1),
				       subBoardArray[i].description.substr(0, this.subBoardNameLen),
				       subBoardArray[i].numPosts, strftime("%Y-%m-%d", subBoardArray[i].newestPostDate));
			}
		}
	}
	// If no sort type is specified, then output the sub-board information in
	// order of sub-board number.
	else
	{
		var includeSubBoard = true;
		for (var arrSubBoardNum in msg_area.grp_list[grpIndex].sub_list)
		{
			// If the search text is set, then use it to filter the sub-board list.
			includeSubBoard = true;
			if (searchTextUpper.length > 0)
			{
				includeSubBoard = ((msg_area.grp_list[grpIndex].sub_list[arrSubBoardNum].name.toUpperCase().indexOf(searchTextUpper) >= 0) ||
				                   (msg_area.grp_list[grpIndex].sub_list[arrSubBoardNum].description.toUpperCase().indexOf(searchTextUpper) >= 0));
			}
			if (includeSubBoard)
			{
				// Open the current sub-board with the msgBase object.
				msgBase = new MsgBase(msg_area.grp_list[grpIndex].sub_list[arrSubBoardNum].code);
				if (msgBase.open())
				{
					// Get the date & time when the last message was imported.
					//var numMsgs = numReadableMsgs(msgBase, msg_area.grp_list[grpIndex].sub_list[arrSubBoardNum].code);
					var numMsgs = msgBase.total_msgs;
					if (numMsgs > 0)
					{
						//var msgHeader = msgBase.get_msg_header(true, msgBase.total_msgs-1, true);
						var msgHeader = getLatestMsgHdr(msg_area.grp_list[grpIndex].sub_list[arrSubBoardNum].code);
						if (msgHeader === null)
							msgHeader = getBogusMsgHdr();
						// Construct the date & time strings of the latest post
						if (this.showImportDates)
						{
							newestDate.date = strftime("%Y-%m-%d", msgHeader.when_imported_time);
							newestDate.time = strftime("%H:%M:%S", msgHeader.when_imported_time);
						}
						else
						{
							var msgWrittenLocalBBSTime = msgWrittenTimeToLocalBBSTime(msgHeader);
							if (msgWrittenLocalBBSTime != -1)
							{
								newestDate.date = strftime("%Y-%m-%d", msgWrittenLocalBBSTime);
								newestDate.time = strftime("%H:%M:%S", msgWrittenLocalBBSTime);
							}
							else
							{
								newestDate.date = strftime("%Y-%m-%d", msgHeader.when_written_time);
								newestDate.time = strftime("%H:%M:%S", msgHeader.when_written_time);
							}
						}
					}
					else
						newestDate.date = newestDate.time = "";

					// Print the sub-board information
					subBoardNum = +(arrSubBoardNum);
					console.crlf();
					console.print((subBoardNum == highlightIndex) ? "\1n" + this.colors.areaMark + "*" : " ");
					if (this.showDatesInSubBoardList)
					{
						printf(this.subBoardListPrintfInfo[grpIndex].printfStr, +(subBoardNum+1),
							   msg_area.grp_list[grpIndex].sub_list[arrSubBoardNum].description.substr(0, this.subBoardListPrintfInfo[grpIndex].nameLen),
							   numMsgs, newestDate.date, newestDate.time);
					}
					else
					{
						printf(this.subBoardListPrintfInfo[grpIndex].printfStr, +(subBoardNum+1),
							   msg_area.grp_list[grpIndex].sub_list[arrSubBoardNum].description.substr(0, this.subBoardListPrintfInfo[grpIndex].nameLen),
							   numMsgs);
					}

					msgBase.close();
				}

				// Free some memory?
				delete msgBase;
			}
		}
	}
}

//////////////////////////////////////////////
// Message group list stuff (lightbar mode) //
//////////////////////////////////////////////

// For the DDMsgAreaChooser class - Writes a message group information line.
//
// Parameters:
//  pGrpIndex: The index of the message group to write (assumed to be valid)
//  pHighlight: Boolean - Whether or not to write the line highlighted.
function DDMsgAreaChooser_writeMsgGroupLine(pGrpIndex, pHighlight)
{
	console.print("\1n");
	// Write the highlight background color if pHighlight is true.
	if (pHighlight)
		console.print(this.colors.bkgHighlight);

	// Write the message group information line
	var grpIsSelected = false;
	if ((typeof(bbs.cursub_code) == "string") && (bbs.cursub_code != ""))
		grpIsSelected = (pGrpIndex == msg_area.sub[bbs.cursub_code].grp_index);
	console.print(grpIsSelected ? this.colors.areaMark + "*" : " ");
	printf((pHighlight ? this.msgGrpListHilightPrintfStr : this.msgGrpListPrintfStr),
	       +(pGrpIndex+1),
	       msg_area.grp_list[pGrpIndex].description.substr(0, this.msgGrpDescLen),
	       msg_area.grp_list[pGrpIndex].sub_list.length);
	console.cleartoeol("\1n");
}

//////////////////////////////////////////////////
// Message sub-board list stuff (lightbar mode) //
//////////////////////////////////////////////////

// Updates the page number text in the group list header line on the screen.
//
// Parameters:
//  pPageNum: The page number
//  pNumPages: The total number of pages
//  pGroup: Boolean - Whether or not this is for the group header.  If so,
//          then this will go to the right location for the group page text
//          and use this.colors.header for the text.  Otherwise, this will
//          go to the right place for the sub-board page text and use the
//          sub-board header color.
//  pRestoreCurPos: Optional - Boolean - If true, then move the cursor back
//                  to the position where it was before this function was called
function DDMsgAreaChooser_updatePageNumInHeader(pPageNum, pNumPages, pGroup, pRestoreCurPos)
{
	var originalCurPos = null;
	if (pRestoreCurPos)
		originalCurPos = console.getxy();

	if (pGroup)
	{
		console.gotoxy(29, 1+this.areaChangeHdrLines.length);
		console.print("\1n" + this.colors.header + pPageNum + " of " + pNumPages + ")   ");
	}
	else
	{
		console.gotoxy(51, 1+this.areaChangeHdrLines.length);
		console.print("\1n" + this.colors.subBoardHeader + pPageNum + " of " + pNumPages + ")   ");
	}

	if (pRestoreCurPos)
		console.gotoxy(originalCurPos);
}

// For the DDMsgAreaChooser class: Writes a message sub-board information line.
//
// Parameters:
//  pGrpIndex: The index of the message group (assumed to be valid)
//  pSubIndex: The index of the sub-board within the message group to write (assumed to be valid)
//  pHighlight: Boolean - Whether or not to write the line highlighted.
function DDMsgAreaChooser_GetMsgSubBrdLine(pGrpIndex, pSubIndex, pHighlight)
{
	var subBoardLine = "\1n";
	// Write the highlight background color if pHighlight is true.
	if (pHighlight)
		subBoardLine += this.colors.bkgHighlight;

	// Determine if pGrpIndex and pSubIndex specify the user's
	// currently-selected group and sub-board.
	var currentSub = false;
	if ((typeof(bbs.cursub_code) == "string") && (bbs.cursub_code != ""))
		currentSub = ((pGrpIndex == msg_area.sub[bbs.cursub_code].grp_index) && (pSubIndex == msg_area.sub[bbs.cursub_code].index));

	// Open the current sub-board with the msgBase object (so that we can get
	// the date & time of the last imported message).
	var msgBase = new MsgBase(msg_area.grp_list[pGrpIndex].sub_list[pSubIndex].code);
	if (msgBase.open())
	{
		//var numMsgs = numReadableMsgs(msgBase, msg_area.grp_list[pGrpIndex].sub_list[pSubIndex].code);
		var numMsgs = msgBase.total_msgs;
		var newestDate = {}; // For storing the date of the newest post
		// Get the date & time when the last message was imported.
		var msgHeader = getLatestMsgHdr(msg_area.grp_list[pGrpIndex].sub_list[pSubIndex].code);
		if (msgHeader != null)
		{
			// Construct the date & time strings of the latest post
			if (this.showImportDates)
			{
				newestDate.date = strftime("%Y-%m-%d", msgHeader.when_imported_time);
				newestDate.time = strftime("%H:%M:%S", msgHeader.when_imported_time);
			}
			else
			{
				var msgWrittenLocalBBSTime = msgWrittenTimeToLocalBBSTime(msgHeader);
				if (msgWrittenLocalBBSTime != -1)
				{
					newestDate.date = strftime("%Y-%m-%d", msgWrittenLocalBBSTime);
					newestDate.time = strftime("%H:%M:%S", msgWrittenLocalBBSTime);
				}
				else
				{
					newestDate.date = strftime("%Y-%m-%d", msgHeader.when_written_time);
					newestDate.time = strftime("%H:%M:%S", msgHeader.when_written_time);
				}
			}
		}
		else
			newestDate.date = newestDate.time = "";

		// Print the sub-board information line.
		subBoardLine += (currentSub ? this.colors.areaMark + "*" : " ");
		if (this.showDatesInSubBoardList)
		{
			subBoardLine += format((pHighlight ? this.subBoardListPrintfInfo[pGrpIndex].highlightPrintfStr : this.subBoardListPrintfInfo[pGrpIndex].printfStr),
			       +(pSubIndex+1),
			       msg_area.grp_list[pGrpIndex].sub_list[pSubIndex].description.substr(0, this.subBoardListPrintfInfo[pGrpIndex].nameLen),
			       numMsgs, newestDate.date, newestDate.time);
		}
		else
		{
			subBoardLine += format((pHighlight ? this.subBoardListPrintfInfo[pGrpIndex].highlightPrintfStr : this.subBoardListPrintfInfo[pGrpIndex].printfStr),
			       +(pSubIndex+1),
			       msg_area.grp_list[pGrpIndex].sub_list[pSubIndex].description.substr(0, this.subBoardListPrintfInfo[pGrpIndex].nameLen),
			       numMsgs);
		}
		msgBase.close();

		// Free some memory?
		delete msgBase;
	}

	return subBoardLine;
}

///////////////////////////////////////////////
// Other functions for the msg. area chooser //
///////////////////////////////////////////////

// For the DDMsgAreaChooser class: Reads the configuration file.
function DDMsgAreaChooser_ReadConfigFile()
{
	// Determine the script's startup directory.
	// This code is a trick that was created by Deuce, suggested by Rob Swindell
	// as a way to detect which directory the script was executed in.  I've
	// shortened the code a little.
	var startup_path = '.';
	try { throw dig.dist(dist); } catch(e) { startup_path = e.fileName; }
	startup_path = backslash(startup_path.replace(/[\/\\][^\/\\]*$/,''));

	// Open the configuration file
	var cfgFile = new File(startup_path + "DDMsgAreaChooser.cfg");
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
					else if (settingUpper == "SHOWIMPORTDATES")
						this.showImportDates = (value.toUpperCase() == "TRUE");
					else if (settingUpper == "AREACHOOSERHDRFILENAMEBASE")
						this.areaChooserHdrFilenameBase = value;
					else if (settingUpper == "AREACHOOSERHDRMAXLINES")
					{
						var maxNumLines = +value;
						if (maxNumLines > 0)
							this.areaChooserHdrMaxLines = maxNumLines;
					}
					else if (settingUpper == "SHOWDATESINSUBBOARDLIST")
						this.showDatesInSubBoardList = (value.toUpperCase() == "TRUE");
				}
				else if (settingsMode == "colors")
					this.colors[setting] = value;
			}
		}

		cfgFile.close();
	}
}

// For the DDMsgAreaChooser class: Shows the help screen
//
// Parameters:
//  pLightbar: Boolean - Whether or not to show lightbar help.  If
//             false, then this function will show regular help.
//  pClearScreen: Boolean - Whether or not to clear the screen first
function DDMsgAreaChooser_showHelpScreen(pLightbar, pClearScreen)
{
	if (pClearScreen)
		console.clear("\1n");
	else
		console.print("\1n");
	console.center("\1c\1hDigital Distortion Message Area Chooser");
	console.center("\1kÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ");
	console.center("\1n\1cVersion \1g" + DD_MSG_AREA_CHOOSER_VERSION +
	               " \1w\1h(\1b" + DD_MSG_AREA_CHOOSER_VER_DATE + "\1w)");
	console.crlf();
	console.print("\1n\1cFirst, a listing of message groups is displayed.  One can be chosen by typing");
	console.crlf();
	console.print("its number.  Then, a listing of sub-boards within that message group will be");
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
		console.print("\1hENTER\1n\1c: Select the current group/sub-board");
		console.crlf();
		console.print("\1hHOME\1n\1c: Go to the first item on the screen");
		console.crlf();
		console.print("\1hEND\1n\1c: Go to the last item on the screen");
		console.crlf();
		console.print("\1hPageUp\1n\1c/\1hPageDown\1n\1c: Go to the previous/next page");
		console.crlf();
		console.print("\1hF\1n\1c/\1hL\1n\1c: Go to the first/last page");
		console.crlf();
		console.print("\1h/\1n\1c or \1hCtrl-F\1n\1c: Find by name/description");
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

// For the DDMsgAreaChooser class: Builds sub-board printf format information
// for a message group.  The widths of the description & # messages columns
// are calculated based on the greatest number of messages in a sub-board for
// the message group.
//
// Parameters:
//  pGrpIndex: The index of the message group
function DDMsgAreaChooser_buildSubBoardPrintfInfoForGrp(pGrpIndex)
{
	// If the array of sub-board printf strings doesn't contain the printf
	// strings for this message group, then figure out the largest number
	// of messages in the message group and add the printf strings.
	if (typeof(this.subBoardListPrintfInfo[pGrpIndex]) == "undefined")
	{
		var greatestNumMsgs = getGreatestNumMsgs(pGrpIndex);

		this.subBoardListPrintfInfo[pGrpIndex] = {};
		this.subBoardListPrintfInfo[pGrpIndex].numMsgsLen = greatestNumMsgs.toString().length;
		// Sub-board name length: With a # items length of 4, this should be
		// 47 for an 80-column display.
		this.subBoardListPrintfInfo[pGrpIndex].nameLen = console.screen_columns - this.areaNumLen - this.subBoardListPrintfInfo[pGrpIndex].numMsgsLen - 5;
		if (this.showDatesInSubBoardList)
			this.subBoardListPrintfInfo[pGrpIndex].nameLen -= (this.dateLen + this.timeLen + 2);
		// Create the printf strings
		this.subBoardListPrintfInfo[pGrpIndex].printfStr = " " + this.colors.areaNum
		                                                 + "%" + this.areaNumLen + "d "
		                                                 + this.colors.desc + "%-"
		                                                 + this.subBoardListPrintfInfo[pGrpIndex].nameLen + "s "
		                                                 + this.colors.numItems + "%"
		                                                 + this.subBoardListPrintfInfo[pGrpIndex].numMsgsLen + "d";
		if (this.showDatesInSubBoardList)
		{
			this.subBoardListPrintfInfo[pGrpIndex].printfStr += " "
			                                                 + this.colors.latestDate + "%" + this.dateLen + "s "
			                                                 + this.colors.latestTime + "%" + this.timeLen + "s";
		}
		this.subBoardListPrintfInfo[pGrpIndex].highlightPrintfStr = "\1n" + this.colors.bkgHighlight
		                                                          + " " + "\1n"
		                                                          + this.colors.bkgHighlight
		                                                          + this.colors.areaNumHighlight
		                                                          + "%" + this.areaNumLen + "d \1n"
		                                                          + this.colors.bkgHighlight
		                                                          + this.colors.descHighlight + "%-"
		                                                          + this.subBoardListPrintfInfo[pGrpIndex].nameLen + "s \1n"
		                                                          + this.colors.bkgHighlight
		                                                          + this.colors.numItemsHighlight + "%"
		                                                          + this.subBoardListPrintfInfo[pGrpIndex].numMsgsLen + "d";
		if (this.showDatesInSubBoardList)
		{
			this.subBoardListPrintfInfo[pGrpIndex].highlightPrintfStr += " \1n"
			                                                          + this.colors.bkgHighlight
			                                                          + this.colors.dateHighlight + "%" + this.dateLen + "s \1n"
			                                                          + this.colors.bkgHighlight
			                                                          + this.colors.timeHighlight + "%" + this.timeLen + "s\1n";
		}
	}
}

// For the DDMsgAreaChooser class: Displays the area chooser header
//
// Parameters:
//  pStartScreenRow: The row on the screen at which to start displaying the
//                   header information.  Will be used if the user's terminal
//                   supports ANSI.
//  pClearRowsFirst: Optional boolean - Whether or not to clear the rows first.
//                   Defaults to true.  Only valid if the user's terminal supports
//                   ANSI.
function DDMsgAreaChooser_DisplayAreaChgHdr(pStartScreenRow, pClearRowsFirst)
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

// For the DDMsgAreaChooser class: Writes a temporary error message at the key help line
// for lightbar mode.
//
// Parameters:
//  pErrorMsg: The error message to write
//  pRefreshHelpLine: Whether or not to re-draw the help line on the screen
function DDMsgAreaChooser_WriteLightbarKeyHelpErrorMsg(pErrorMsg, pRefreshHelpLine)
{
	console.gotoxy(1, console.screen_rows);
	console.cleartoeol("\1n");
	console.gotoxy(1, console.screen_rows);
	console.print("\1y\1h" + pErrorMsg + "\1n");
	mswait(ERROR_WAIT_MS);
	if (pRefreshHelpLine)
		this.WriteKeyHelpLine();
}

// Removes multiple, leading, and/or trailing spaces.
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

// Returns the greatest number of messages of all sub-boards within
// a message group.
//
// Parameters:
//  pGrpIndex: The index of the message group
//
// Returns: The greatest number of messages of all sub-boards within
//          the message group
function getGreatestNumMsgs(pGrpIndex)
{
	// Sanity checking
	if (typeof(pGrpIndex) != "number")
		return 0;
	if (typeof(msg_area.grp_list[pGrpIndex]) == "undefined")
		return 0;

	var greatestNumMsgs = 0;
	var msgBase = null;
	for (var subIndex = 0; subIndex < msg_area.grp_list[pGrpIndex].sub_list.length; ++subIndex)
	{
		msgBase = new MsgBase(msg_area.grp_list[pGrpIndex].sub_list[subIndex].code);
		if (msgBase == null) continue;
		if (msgBase.open())
		{
			if (msgBase.total_msgs > greatestNumMsgs)
				greatestNumMsgs = msgBase.total_msgs;
			msgBase.close();
		}
	}
	return greatestNumMsgs;
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
	if (userInput == KEY_ESC)
	{
		switch (console.inkey(K_NOECHO|K_NOSPIN, 2))
		{
			case '[':
				switch (console.inkey(K_NOECHO|K_NOSPIN, 2))
				{
					case 'V':
						userInput = KEY_PAGE_UP;
						break;
					case 'U':
						userInput = KEY_PAGE_DOWN;
						break;
				}
				break;
			case 'O':
				switch (console.inkey(K_NOECHO|K_NOSPIN, 2))
				{
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

// Adjusts a message's when-written time to the BBS's local time.
//
// Parameters:
//  pMsgHdr: A message header object
//
// Return value: The message's when_written_time adjusted to the BBS's local time.
//               If the message header doesn't have a when_written_time or
//               when_written_zone property, then this function will return -1.
function msgWrittenTimeToLocalBBSTime(pMsgHdr)
{
	if (!pMsgHdr.hasOwnProperty("when_written_time") || !pMsgHdr.hasOwnProperty("when_written_zone_offset") || !pMsgHdr.hasOwnProperty("when_imported_zone_offset"))
		return -1;

	var timeZoneDiffMinutes = pMsgHdr.when_imported_zone_offset - pMsgHdr.when_written_zone_offset;
	//var timeZoneDiffMinutes = pMsgHdr.when_written_zone - system.timezone;
	var timeZoneDiffSeconds = timeZoneDiffMinutes * 60;
	var msgWrittenTimeAdjusted = pMsgHdr.when_written_time + timeZoneDiffSeconds;
	return msgWrittenTimeAdjusted;
}

// Returns an object containing bare minimum properties necessary to
// display an invalid message header.  Additionally, an object returned
// by this function will have an extra property, isBogus, that will be
// a boolean set to true.
//
// Parameters:
//  pSubject: Optional - A string to use as the subject in the bogus message
//            header object
function getBogusMsgHdr(pSubject)
{
	var msgHdr = {
		subject: (typeof(pSubject) == "string" ? pSubject : ""),
		when_imported_time: 0,
		when_written_time: 0,
		when_written_zone: 0,
		date: "Fri, 1 Jan 1960 00:00:00 -0000",
		attr: 0,
		to: "Nobody",
		from: "Nobody",
		number: 0,
		offset: 0,
		isBogus: true
	};
	return msgHdr;
}

// Returns whether a message is readable to the user, based on its
// header and the sub-board code.
//
// Parameters:
//  pMsgHdr: The header object for the message
//  pSubBoardCode: The internal code for the sub-board the message is in
//
// Return value: Boolean - Whether or not the message is readable for the user
function isReadableMsgHdr(pMsgHdr, pSubBoardCode)
{
	if (pMsgHdr === null)
		return false;
	// Let the sysop see unvalidated messages and private messages but not other users.
	if (gIsSysop)
	{
		if (pSubBoardCode != "mail")
		{
			if ((msg_area.sub[pSubBoardCode].is_moderated && ((pMsgHdr.attr & MSG_VALIDATED) == 0)) ||
			    (((pMsgHdr.attr & MSG_PRIVATE) == MSG_PRIVATE) && !userHandleAliasNameMatch(pMsgHdr.to)))
			{
				return false;
			}
		}
	}
	// If the message is deleted, determine whether it should be viewable, based
	// on the system settings.
	if ((pMsgHdr.attr & MSG_DELETE) == MSG_DELETE)
	{
		// If the user is a sysop, check whether sysops can view deleted messages.
		// Otherwise, check whether users can view deleted messages.
		if (gIsSysop)
		{
			if ((system.settings & SYS_SYSVDELM) == 0)
				return false;
		}
		else
		{
			if ((system.settings & SYS_USRVDELM) == 0)
				return false;
		}
	}
	// The message voting and poll variables were added in sbbsdefs.js for
	// Synchronet 3.17.  Make sure they're defined before referencing them.
	if (typeof(MSG_UPVOTE) != "undefined")
	{
		if ((pMsgHdr.attr & MSG_UPVOTE) == MSG_UPVOTE)
			return false;
	}
	if (typeof(MSG_DOWNVOTE) != "undefined")
	{
		if ((pMsgHdr.attr & MSG_DOWNVOTE) == MSG_DOWNVOTE)
			return false;
	}
	// Don't include polls as being unreadable messages - They just need to have
	// their answer selections read from the header instead of the message body
	/*
	if (typeof(MSG_POLL) != "undefined")
	{
		if ((pMsgHdr.attr & MSG_POLL) == MSG_POLL)
			return false;
	}
	*/
	return true;
}

// Returns the number of readable messages in a sub-board.
//
// Parameters:
//  pMsgbase: The MsgBase object representing the sub-board
//  pSubBoardCode: The internal code of the sub-board
//
// Return value: The number of readable messages in the sub-board
function numReadableMsgs(pMsgbase, pSubBoardCode)
{
	if ((pMsgbase === null) || !pMsgbase.is_open)
		return 0;

	var numMsgs = 0;
	if (typeof(pMsgbase.get_all_msg_headers) === "function")
	{
		var msgHdrs = pMsgbase.get_all_msg_headers(true);
		for (var msgHdrsProp in msgHdrs)
		{
			if (msgHdrs[msgHdrsProp] == null)
				continue;
			else if (isReadableMsgHdr(msgHdrs[msgHdrsProp], pSubBoardCode))
				++numMsgs;
		}
	}
	else
	{
		var msgHeader;
		for (var i = 0; i < pMsgbase.total_msgs; ++i)
		{
			msgHeader = msgBase.get_msg_header(true, i, false);
			if (msgHeader == null)
				continue;
			else if (isReadableMsgHdr(msgHeader, pSubBoardCode))
				++numMsgs;
		}
	}
	return numMsgs;
}

// Returns whether a given name matches the logged-in user's handle, alias, or
// name.
//
// Parameters:
//  pName: A name to match against the logged-in user
//
// Return value: Boolean - Whether or not the given name matches the logged-in
//               user's handle, alias, or name
function userHandleAliasNameMatch(pName)
{
	if (typeof(pName) != "string")
		return false;

	var userMatch = false;
	var nameUpper = pName.toUpperCase();
	if (user.handle.length > 0)
		userMatch = (nameUpper.indexOf(user.handle.toUpperCase()) > -1);
	if (!userMatch && (user.alias.length > 0))
		userMatch = (nameUpper.indexOf(user.alias.toUpperCase()) > -1);
	if (!userMatch && (user.name.length > 0))
		userMatch = (nameUpper.indexOf(user.name.toUpperCase()) > -1);
	return userMatch;
}

// Gets a sub-board's group and sub-board indexes with a given sub-board code.
//
// Parameters:
//  pSubBoardCode: The internal code of the sub-board
//  pDefaultToZero: Whether or not to default the indexes to 0 instead of -1
//
// Return value: An object containing the following values:
//               grpIdx: The sub-board's group index.  Will be -1 if the sub-board code is invalid.
//               subIdx: The sub-board index.    Will be -1 if the sub-board code is invalid.
function getGrpAndSubIdxesFromCode(pSubBoardCode, pDefaultToZero)
{
	var retVal = {
		grpIdx: (pDefaultToZero ? 0 : -1),
		subIdx: (pDefaultToZero ? 0 : -1)
	};
	if ((typeof(pSubBoardCode) == "string") && (pSubBoardCode != ""))
	{
		retVal.grpIdx = msg_area.sub[pSubBoardCode].grp_index;
		retVal.subIdx = msg_area.sub[pSubBoardCode].index;
	}
	return retVal;
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
//  pSubBoard: Boolean - If true, search the sub-board list for the given group index.
//             If false, search the group list.
//  pStartItemIdx: The item index to start at
//  pGrpIdx: The index of the group to search in (only for doing a sub-board search)
//
// Return value: An object containing the following properties:
//               pageNum: The page number of the item (1-based; will be 0 if not found)
//               pageTopIdx: The index of the top item on the page (or -1 if not found)
//               itemIdx: The index of the item (or -1 if not found)
function getPageNumFromSearch(pText, pNumItemsPerPage, pSubBoard, pStartItemIdx, pGrpIdx)
{
	var retObj = {
		pageNum: 0,
		pageTopIdx: -1,
		itemIdx: -1
	};

	// Sanity checking
	if ((typeof(pText) != "string") || (typeof(pNumItemsPerPage) != "number") || (typeof(pSubBoard) != "boolean"))
		return retObj;

	// Convert the text to uppercase for case-insensitive searching
	var srchText = pText.toUpperCase();
	if (pSubBoard)
	{
		if ((typeof(pGrpIdx) == "number") && (pGrpIdx >= 0) && (pGrpIdx < msg_area.grp_list.length))
		{
			// Go through the sub-board list of the given group and
			// search for text in the descriptions
			for (var i = pStartItemIdx; i < msg_area.grp_list[pGrpIdx].sub_list.length; ++i)
			{
				if ((msg_area.grp_list[pGrpIdx].sub_list[i].description.toUpperCase().indexOf(srchText) > -1) ||
				    (msg_area.grp_list[pGrpIdx].sub_list[i].name.toUpperCase().indexOf(srchText) > -1))
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
		// Go through the message group list and look for a match
		for (var i = pStartItemIdx; i < msg_area.grp_list.length; ++i)
		{
			if ((msg_area.grp_list[i].name.toUpperCase().indexOf(srchText) > -1) ||
			    (msg_area.grp_list[i].description.toUpperCase().indexOf(srchText) > -1))
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

// Gets the header of the latest readable message in a sub-board,
// given a number of messages to look at.
//
// Paramters:
//  pSubCode: The internal code of the message sub-board
//  pNumMsgsToCheck: The number of messages to check at the end of the sub-board.
//                   This is optional and if omitted, all messages will be
//                   checked (from the last message) for the latest readable
//                   message header.
//
// Return value: The message header of the latest readable message.  If
//               none is found, this will be null.
function getLatestMsgHdr(pSubCode, pNumMsgsToCheck)
{
	var msgHdr = null;
	var numMsgsToCheck = (typeof(pNumMsgsToCheck) == "number" ? pNumMsgsToCheck : 0);
	var msgBase = new MsgBase(pSubCode);
	if (msgBase.open())
	{
		// Look through the last pNumMsgsToCheck headers to find the latest
		// readable message header
		var numMsgs = msgBase.total_msgs;
		var firstMsgIdx = 0;
		if (numMsgsToCheck >= 1)
			firstMsgIdx = numMsgs - numMsgsToCheck;
		else
			firstMsgIdx = 0;
		if (firstMsgIdx < 0)
			firstMsgIdx = 0;
		for (var i = numMsgs - 1; (i >= firstMsgIdx) && (msgHdr == null); --i)
		{
			var msgHeader = msgBase.get_msg_header(true, i, true);
			if (isReadableMsgHdr(msgHeader, pSubCode))
				msgHdr = msgHeader;
		}

		msgBase.close();
	}
	return msgHdr;
}

// Finds a message group index with search text, matching either the name or
// description, case-insensitive.
//
// Parameters:
//  pSearchText: The name/description text to look for
//  pStartItemIdx: The item index to start at.  Defaults to 0
//
// Return value: The index of the message group, or -1 if not found
function findMsgGrpIdxFromText(pSearchText, pStartItemIdx)
{
	if (typeof(pSearchText) != "string")
		return -1;

	var grpIdx = -1;

	var startIdx = (typeof(pStartItemIdx) == "number" ? pStartItemIdx : 0);
	if ((startIdx < 0) || (startIdx > msg_area.grp_list.length))
		startIdx = 0;

	// Go through the message group list and look for a match
	var searchTextUpper = pSearchText.toUpperCase();
	for (var i = startIdx; i < msg_area.grp_list.length; ++i)
	{
		if ((msg_area.grp_list[i].name.toUpperCase().indexOf(searchTextUpper) > -1) ||
		    (msg_area.grp_list[i].description.toUpperCase().indexOf(searchTextUpper) > -1))
		{
			grpIdx = i;
			break;
		}
	}

	return grpIdx;
}

// Finds a message group index with search text, matching either the name or
// description, case-insensitive.
//
// Parameters:
//  pGrpIdx: The index of the message group
//  pSearchText: The name/description text to look for
//  pStartItemIdx: The item index to start at.  Defaults to 0
//
// Return value: The index of the sub-board, or -1 if not found
function findSubBoardIdxFromText(pGrpIdx, pSearchText, pStartItemIdx)
{
	if (typeof(pGrpIdx) != "number")
		return -1;
	if (typeof(pSearchText) != "string")
		return -1;

	var subBoardIdx = -1;

	var startIdx = (typeof(pStartItemIdx) == "number" ? pStartItemIdx : 0);
	if ((startIdx < 0) || (startIdx > msg_area.grp_list[pGrpIdx].sub_list.length))
		startIdx = 0;

	// Go through the message group list and look for a match
	var searchTextUpper = pSearchText.toUpperCase();
	for (var i = startIdx; i < msg_area.grp_list[pGrpIdx].sub_list.length; ++i)
	{
		if ((msg_area.grp_list[pGrpIdx].sub_list[i].name.toUpperCase().indexOf(searchTextUpper) > -1) ||
		    (msg_area.grp_list[pGrpIdx].sub_list[i].description.toUpperCase().indexOf(searchTextUpper) > -1))
		{
			subBoardIdx = i;
			break;
		}
	}

	return subBoardIdx;
}
