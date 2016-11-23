/* This is a script that lets the user choose a message area,
 * with either a lightbar or traditional user interface.
 *
 * Date       User          Version Description
 * 2010-02-05 Eric Oulashin 0.90    Started
 * 2010-02-18 to
 * 2010-02-27 Eric Oulashin         Continued work.
 *                                  Added the first & lasg page functionality
 *                                  to the lightbar interface.
 * 2010-03-13 Eric Oulashin 1.00    Added the ability to load settings from a
 *                                  configuration file.
 * 2011-04-22 Eric Oulashin 1.01    Fixed the wording when choosing a message
 *                                  group - It now says "group #" instead
 *                                  of "sub-board #".
 * 2012-10-06 Eric Oulashin 1.02    For lightbar mode, updated to display the
 *                                  page number in the header at the top (in
 *                                  addition to the total number of pages,
 *                                  which it was already displaying).
 * 2012-11-30 Eric Oulashin 1.03    Bug fix: After leaving the help screen
 *                                  from the sub-board list, the top line is
 *                                  now correctly written with the page
 *                                  information as "Page # of #".
 * 2013-05-04 Eric Oulashin 1.04    Updated to dynamically adjust the length
 *                                  of the # messages column based on the
 *                                  greatest number of messages of all
 *                                  sub-boards within a message group so
 *                                  that the formatting still looks good.
 * 2013-05-10 Eric Oulashin 1.05    Updated the version to match the
 *                                  version in DDFileAreaChooser (a bug
 *                                  was fixed there, but DDMsgAreaChooser
 *                                  didn't have the corresponding bug).
 * 2014-09-14 Eric Oulashin 1.06    Bug fix: Updated the highlight (lightbar)
 *                                  format string in the
 *                                  DDMsgAreaChooser_buildSubBoardPrintfInfoForGrp()
 *                                  function to include a normal attribute at
 *                                  the end to avoid color issues when clearing
 *                                  the screen, etc.  Bug reported by Psi-Jack.
 * 2014-12-22 Eric Oulashin 1.07    Bug fix: Made this.colors.subBoardHeader apply
 *                                  to the whole line rather than just the page
 *                                  number.
 *                                  Bug fix: The initial display of the page number
 *                                  is now correct (previously, it would start out
 *                                  saying page 1, even if on another page).
 * 2015-04-19 Eric Oulashin 1.08    Added color settings for the lightbar help text
 *                                  at the bottom of the screen.  Also, added the
 *									ability to use the PageUp & PageDown keys instead
 *                                  of P and N in the lightbar lists.
 * 2016-01-17 Eric Oulashin 1.09    Updated to allow choosing only the sub-board within
 *                                  the user's current message group.  The first command-
 *                                  line argument now specifies whether or not to
 *                                  allow choosing the message group, and it defaults
 *                                  to true.
 * 2016-02-12 Eric Oulashin 1.10Beta Started working on adding the ability to display
 *                                  a header ANSI/ASCII file above the list.
 * 2016-02-15 Eric Oulashin 1.10    Releasing this version
 * 2016-02-19 Eric Oulashin 1.11    Bug fix: The page number wasn't being updated
 *                                  when changing pages in the message groups
 *                                  when using the arrow keys to scroll between
 *                                  pages
 * 2016-11-20 Eric Oulashin 1.12    Started working on updating to handle null
 *                                  message headers, which could happen with the
 *                                  new voting feature in Synchronet 3.17.
 * 2016-11-22 Eric Oulashin 1.12    Releasing this version
*/

/* Command-line arguments:
   1 (argv[0]): Boolean - Whether or not to choose a message group first (default).  If
                false, the user will only be able to choose a different sub-board within
				their current message group.
   2 (argv[1]): Boolean - Whether or not to run the area chooser (if false,
                then this file will just provide the DDMsgAreaChooser class).
*/

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
var DD_MSG_AREA_CHOOSER_VERSION = "1.12";
var DD_MSG_AREA_CHOOSER_VER_DATE = "2016-11-22";

// Keyboard input key codes
var CTRL_M = "\x0d";
var KEY_ENTER = CTRL_M;
var KEY_ESC = ascii(27);
// PageUp & PageDown keys - Not real key codes, but codes defined
// to be used & recognized in this script
var KEY_PAGE_UP = "\1PgUp";
var KEY_PAGE_DOWN = "\1PgDn";

// Key codes for display
var UP_ARROW = ascii(24);
var DOWN_ARROW = ascii(25);

// Determine the script's startup directory.
// This code is a trick that was created by Deuce, suggested by Rob Swindell
// as a way to detect which directory the script was executed in.  I've
// shortened the code a little.
var gStartupPath = '.';
try { throw dig.dist(dist); } catch(e) { gStartupPath = e.fileName; }
gStartupPath = backslash(gStartupPath.replace(/[\/\\][^\/\\]*$/,''));

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
	this.colors = new Object();
	this.colors.areaNum = "\1n\1w\1h";
	this.colors.desc = "\1n\1c";
	this.colors.numItems = "\1b\1h";
	this.colors.header = "\1n\1y\1h";
	this.colors.subBoardHeader = "\1n\1g";
	this.colors.areaMark = "\1g\1h";
	this.colors.latestDate = "\1n\1g";
	this.colors.latestTime = "\1n\1m";
	// Highlighted colors (for lightbar mode)
	this.colors.bkgHighlight = "\1" + "4"; // Blue background
	this.colors.areaNumHighlight = "\1w\1h";
	this.colors.descHighlight = "\1c";
	this.colors.dateHighlight = "\1w\1h";
	this.colors.timeHighlight = "\1w\1h";
	this.colors.numItemsHighlight = "\1w\1h";
	// Lightbar help line colors
	this.colors.lightbarHelpLineBkg = "\1" + "7";
	this.colors.lightbarHelpLineGeneral = "\1b";
	this.colors.lightbarHelpLineHotkey = "\1r";
	this.colors.lightbarHelpLineParen = "\1m";

	// showImportDates is a boolean to specify whether or not to display the
	// message import dates.  If false, the message written dates will be
	// displayed instead.
	this.showImportDates = true;

	// useLightbarInterface specifies whether or not to use the lightbar
	// interface.  The lightbar interface will still only be used if the
	// user's terminal supports ANSI.
	this.useLightbarInterface = true;

	// These variables store the lengths of the various columns displayed in
	// the message group/sub-board lists.
	// Sub-board info field lengths
	this.areaNumLen = 4;
	this.numItemsLen = 4;
	this.dateLen = 10; // i.e., YYYY-MM-DD
	this.timeLen = 8;  // i.e., HH:MM:SS
	// Sub-board name length - This should be 47 for an 80-column display.
	this.subBoardNameLen = console.screen_columns - this.areaNumLen -
	this.numItemsLen - this.dateLen - this.timeLen - 7;
	// Message group description length (67 chars on an 80-column screen)
	this.msgGrpDescLen = console.screen_columns - this.areaNumLen -
	this.numItemsLen - 5;

	// Filename base of a header to display above the area list
	this.areaChooserHdrFilenameBase = "msgAreaChgHeader";
	this.areaChooserHdrMaxLines = 5;

	// Set the function pointers for the object
	this.ReadConfigFile = DDMsgAreaChooser_ReadConfigFile;
	this.WriteKeyHelpLine = DDMsgAreaChooser_writeKeyHelpLine;
	this.WriteGrpListHdrLine = DDMsgAreaChooser_writeGrpListTopHdrLine;
	this.WriteSubBrdListHdr1Line = DMsgAreaChooser_writeSubBrdListHdr1Line;
	this.SelectMsgArea = DDMsgAreaChooser_selectMsgArea;
	this.SelectMsgArea_Lightbar = DDMsgAreaChooser_selectMsgArea_Lightbar;
	this.SelectSubBoard_Lightbar = DDMsgAreaChooser_selectSubBoard_Lightbar;
	this.SelectMsgArea_Traditional = DDMsgAreaChooser_selectMsgArea_Traditional;
	this.SelectSubBoard_Traditional = DDMsgAreaChooser_selectSubBoard_Traditional;
	this.ListMsgGrps = DDMsgAreaChooser_listMsgGrps_Traditional;
	this.ListSubBoardsInMsgGroup = DDMsgAreaChooser_listSubBoardsInMsgGroup_Traditional;
	// Lightbar-specific functions
	this.ListScreenfulOfMsgGrps = DDMsgAreaChooser_listScreenfulOfMsgGrps;
	this.WriteMsgGroupLine = DDMsgAreaChooser_writeMsgGroupLine;
	this.updatePageNumInHeader = DDMsgAreaChooser_updatePageNumInHeader;
	this.ListScreenfulOfSubBrds = DDMsgAreaChooser_listScreenfulOfSubBrds;
	this.WriteMsgSubBoardLine = DDMsgAreaChooser_writeMsgSubBrdLine;
	// Help screen
	this.ShowHelpScreen = DDMsgAreaChooser_showHelpScreen;
	// Function to build the sub-board printf information for a message
	// group
	this.BuildSubBoardPrintfInfoForGrp = DDMsgAreaChooser_buildSubBoardPrintfInfoForGrp;
	this.DisplayAreaChgHdr = DDMsgAreaChooser_DisplayAreaChgHdr;

	// Read the settings from the config file.
	this.ReadConfigFile();

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
	                              + +(this.subBoardNameLen-3) + "s %-7s %-19s";
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
function DDMsgAreaChooser_selectMsgArea_Lightbar(pChooseGroup)
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
		// Returns the index of the bottommost message group that can be displayed
		// on the screen.
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


		// Figure out the index of the user's currently-selected message group
		var selectedGrpIndex = 0;
		if ((bbs.curgrp != null) && (typeof(bbs.curgrp) == "number"))
			selectedGrpIndex = bbs.curgrp;

		var listStartRow = 2 + this.areaChangeHdrLines.length; // The row on the screen where the list will start
		var listEndRow = console.screen_rows - 1; // Row on screen where list will end
		var topMsgGrpIndex = 0;    // The index of the message group at the top of the list

		// Figure out the index of the last message group to appear on the screen.
		var numItemsPerPage = listEndRow - listStartRow + 1;
		var bottomMsgGrpIndex = getBottommostGrpIndex(topMsgGrpIndex, numItemsPerPage);
		// Figure out how many pages are needed to list all the sub-boards.
		var numPages = Math.ceil(msg_area.grp_list.length / numItemsPerPage);
		// Figure out the top index for the last page.
		var topIndexForLastPage = (numItemsPerPage * numPages) - numItemsPerPage;

		// If the highlighted row is beyond the current screen, then
		// go to the appropriate page.
		if (selectedGrpIndex > bottomMsgGrpIndex)
		{
			var nextPageTopIndex = 0;
			while (selectedGrpIndex > bottomMsgGrpIndex)
			{
				nextPageTopIndex = topMsgGrpIndex + numItemsPerPage;
				if (nextPageTopIndex < msg_area.grp_list.length)
				{
					// Adjust topMsgGrpIndex and bottomMsgGrpIndex, and
					// refresh the list on the screen.
					topMsgGrpIndex = nextPageTopIndex;
					bottomMsgGrpIndex = getBottommostGrpIndex(topMsgGrpIndex, numItemsPerPage);
				}
				else
					break;
			}

			// If we didn't find the correct page for some reason, then set the
			// variables to display page 1 and select the first message group.
			var foundCorrectPage = ((topMsgGrpIndex < msg_area.grp_list.length) &&
									(selectedGrpIndex >= topMsgGrpIndex) && (selectedGrpIndex <= bottomMsgGrpIndex));
			if (!foundCorrectPage)
			{
				topMsgGrpIndex = 0;
				bottomMsgGrpIndex = getBottommostGrpIndex(topMsgGrpIndex, numItemsPerPage);
				selectedGrpIndex = 0;
			}
		}

		// Clear the screen, write the header, help line, and group list header, and output
		// a screenful of message groups.
		console.clear("\1n");
		this.DisplayAreaChgHdr(1);
		this.WriteKeyHelpLine();

		var curpos = new Object();
		curpos.x = 1;
		curpos.y = 1 + this.areaChangeHdrLines.length;
		console.gotoxy(curpos);
		var pageNum = calcPageNum(topMsgGrpIndex, numItemsPerPage);
		this.WriteGrpListHdrLine(numPages, pageNum);
		this.ListScreenfulOfMsgGrps(topMsgGrpIndex, listStartRow, listEndRow, false, false);
		// Start of the input loop.
		var highlightScrenRow = 0; // The row on the screen for the highlighted group
		var userInput = "";        // Will store a keypress from the user
		var retObj = null;        // To store the return value of choosing a sub-board
		var continueChoosingMsgArea = true;
		while (continueChoosingMsgArea)
		{
			// Highlight the currently-selected message group
			highlightScrenRow = listStartRow + (selectedGrpIndex - topMsgGrpIndex);
			curpos.y = highlightScrenRow;
			if ((highlightScrenRow > 0) && (highlightScrenRow < console.screen_rows))
			{
				console.gotoxy(1, highlightScrenRow);
				this.WriteMsgGroupLine(selectedGrpIndex, true);
			}

			// Get a key from the user (upper-case) and take action based upon it.
			userInput = getKeyWithESCChars(K_UPPER | K_NOCRLF);
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
						this.updatePageNumInHeader(pageNum, numPages, true, false);
						this.ListScreenfulOfMsgGrps(topMsgGrpIndex, listStartRow, listEndRow, false, true);
					}
					else
					{
						// Display the current line un-highlighted.
						console.gotoxy(1, curpos.y);
						this.WriteMsgGroupLine(selectedGrpIndex, false);
					}
					selectedGrpIndex = previousGrpIndex;
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
						this.updatePageNumInHeader(pageNum+1, numPages, true, false);
						this.ListScreenfulOfMsgGrps(topMsgGrpIndex, listStartRow,
													listEndRow, false, true);
					}
					else
					{
						// Display the current line un-highlighted.
						console.gotoxy(1, curpos.y);
						this.WriteMsgGroupLine(selectedGrpIndex, false);
					}
					selectedGrpIndex = nextGrpIndex;
				}
				break;
			case KEY_HOME: // Go to the top message group on the screen
				if (selectedGrpIndex > topMsgGrpIndex)
				{
					// Display the current line un-highlighted, then adjust
					// selectedGrpIndex.
					console.gotoxy(1, curpos.y);
					this.WriteMsgGroupLine(selectedGrpIndex, false);
					selectedGrpIndex = topMsgGrpIndex;
					// Note: curpos.y is set at the start of the while loop.
				}
				break;
			case KEY_END: // Go to the bottom message group on the screen
				if (selectedGrpIndex < bottomMsgGrpIndex)
				{
					// Display the current line un-highlighted, then adjust
					// selectedGrpIndex.
					console.gotoxy(1, curpos.y);
					this.WriteMsgGroupLine(selectedGrpIndex, false);
					selectedGrpIndex = bottomMsgGrpIndex;
					// Note: curpos.y is set at the start of the while loop.
				}
				break;
			case KEY_ENTER: // Select the currently-highlighted message group
				retObj = this.SelectSubBoard_Lightbar(selectedGrpIndex);
				// If the user chose a sub-board, then set bbs.curgrp and
				// bbs.cursub, and don't continue the input loop anymore.
				if (retObj.subBoardChosen)
				{
					bbs.curgrp = selectedGrpIndex;
					bbs.cursub = retObj.subBoardIndex;
					continueChoosingMsgArea = false;
				}
				else
				{
					// A sub-board was not chosen, so we'll have to re-draw
					// the header and list of message groups.
					this.DisplayAreaChgHdr(1);
					console.gotoxy(1, 1+this.areaChangeHdrLines.length);
					this.WriteGrpListHdrLine(numPages, pageNum);
					this.ListScreenfulOfMsgGrps(topMsgGrpIndex, listStartRow, listEndRow, false, true);
				}
				break;
			case KEY_PAGE_DOWN: // Go to the next page
				var nextPageTopIndex = topMsgGrpIndex + numItemsPerPage;
				if (nextPageTopIndex < msg_area.grp_list.length)
				{
					// Adjust topMsgGrpIndex and bottomMsgGrpIndex, and
					// refresh the list on the screen.
					topMsgGrpIndex = nextPageTopIndex;
					pageNum = calcPageNum(topMsgGrpIndex, numItemsPerPage);
					bottomMsgGrpIndex = getBottommostGrpIndex(topMsgGrpIndex, numItemsPerPage);
					this.updatePageNumInHeader(pageNum, numPages, true, false);
					this.ListScreenfulOfMsgGrps(topMsgGrpIndex, listStartRow, listEndRow, false, true);
					selectedGrpIndex = topMsgGrpIndex;
				}
				break;
			case KEY_PAGE_UP: // Go to the previous page
				var prevPageTopIndex = topMsgGrpIndex - numItemsPerPage;
				if (prevPageTopIndex >= 0)
				{
					// Adjust topMsgGrpIndex and bottomMsgGrpIndex, and
					// refresh the list on the screen.
					topMsgGrpIndex = prevPageTopIndex;
					pageNum = calcPageNum(topMsgGrpIndex, numItemsPerPage);
					bottomMsgGrpIndex = getBottommostGrpIndex(topMsgGrpIndex, numItemsPerPage);
					this.updatePageNumInHeader(pageNum, numPages, true, false);
					this.ListScreenfulOfMsgGrps(topMsgGrpIndex, listStartRow, listEndRow, false, true);
					selectedGrpIndex = topMsgGrpIndex;
				}
				break;
			case 'F': // Go to the first page
				if (topMsgGrpIndex > 0)
				{
					topMsgGrpIndex = 0;
					pageNum = calcPageNum(topMsgGrpIndex, numItemsPerPage);
					bottomMsgGrpIndex = getBottommostGrpIndex(topMsgGrpIndex, numItemsPerPage);
					this.updatePageNumInHeader(pageNum, numPages, true, false);
					this.ListScreenfulOfMsgGrps(topMsgGrpIndex, listStartRow, listEndRow, false, true);
					selectedGrpIndex = 0;
				}
				break;
			case 'L': // Go to the last page
				if (topMsgGrpIndex < topIndexForLastPage)
				{
					topMsgGrpIndex = topIndexForLastPage;
					pageNum = calcPageNum(topMsgGrpIndex, numItemsPerPage);
					bottomMsgGrpIndex = getBottommostGrpIndex(topMsgGrpIndex, numItemsPerPage);
					this.updatePageNumInHeader(pageNum, numPages, true, false);
					this.ListScreenfulOfMsgGrps(topMsgGrpIndex, listStartRow, listEndRow, false, true);
					selectedGrpIndex = topIndexForLastPage;
				}
				break;
			case 'Q': // Quit
				continueChoosingMsgArea = false;
				break;
			case '?': // Show help
				this.ShowHelpScreen(true, true);
				console.pause();
				// Refresh the screen
				this.WriteKeyHelpLine();
				this.DisplayAreaChgHdr(1);
				console.gotoxy(1, 1+this.areaChangeHdrLines.length);
				this.WriteGrpListHdrLine(numPages, pageNum);
				this.ListScreenfulOfMsgGrps(topMsgGrpIndex, listStartRow, listEndRow, false, true);
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
					// Move the cursor to the bottom of the screen and
					// prompt the user for the message number.
					console.gotoxy(1, console.screen_rows);
					console.clearline("\1n");
					console.print("\1cChoose group #: \1h");
					userInput = console.getnum(msg_area.grp_list.length);
					// If the user made a selection, then let them choose a
					// sub-board from the group.
					if (userInput > 0)
					{
						var msgGroupIndex = userInput - 1;
						retObj = this.SelectSubBoard_Lightbar(msgGroupIndex);
						// If the user chose a sub-board, then set bbs.curgrp and
						// bbs.cursub, and don't continue the input loop anymore.
						if (retObj.subBoardChosen)
						{
							bbs.curgrp = msgGroupIndex;
							bbs.cursub = retObj.subBoardIndex;
							continueChoosingMsgArea = false;
						}
						else
						{
							// A sub-board was not chosen, so we'll have to re-draw
							// the header and list of message groups.
							console.gotoxy(1, 1);
							this.WriteGrpListHdrLine(numPages, pageNum);
							this.ListScreenfulOfMsgGrps(topMsgGrpIndex, listStartRow, listEndRow, false, true);
						}
					}
					else
					{
						// The user didn't make a selection.  So, we need to refresh
						// the screen due to everything being moved up one line.
						this.WriteKeyHelpLine();
						console.gotoxy(1, 1);
						this.WriteGrpListHdrLine(numPages, pageNum);
						this.ListScreenfulOfMsgGrps(topMsgGrpIndex, listStartRow, listEndRow, false, true);
					}
				}
				break;
			}
		}
	}
	else
	{
		// Don't choose a group, just a sub-board within the user's current group.
		retObj = this.SelectSubBoard_Lightbar(bbs.curgrp);
		// If the user chose a sub-board, then set bbs.cursub
		if (retObj.subBoardChosen)
			bbs.cursub = retObj.subBoardIndex;
	}
}

// For the DDMsgAreaChooser class: Lets the user choose a sub-board within a
// message group, with a lightbar interface.  Does not set bbs.cursub.
//
// Parameters:
//  pGrpIndex: The index of the message group to choose from.  This is
//             optional; if not specified, bbs.curgrp will be used.
//  pMarkIndex: An index of a message group to display the "current" mark
//              next to.  This is optional; if left off, this will default to
//              the current sub-board.
//
// Return value: An object containing the following values:
//               subBoardChosen: Boolean - Whether or not a sub-board was chosen.
//               subBoardIndex: Numeric - The sub-board that was chosen (if any).
//                              Will be -1 if none chosen.
function DDMsgAreaChooser_selectSubBoard_Lightbar(pGrpIndex, pMarkIndex)
{
	// Create the return object.
	var retObj = new Object();
	retObj.subBoardChosen = false;
	retObj.subBoardIndex = -1;

	var grpIndex = 0;
	if (typeof(pGrpIndex) == "number")
		grpIndex = pGrpIndex;
	else if ((bbs.curgrp != null) && (typeof(bbs.curgrp) == "number"))
		grpIndex = bbs.curgrp;
	// Double-check grpIndex
	if (grpIndex < 0)
		grpIndex = 0;
	else if (grpIndex >= msg_area.grp_list.length)
		grpIndex = msg_area.grp_list.length - 1;

	var markIndex = 0;
	if ((pMarkIndex != null) && (typeof(pMarkIndex) == "number"))
		markIndex = pMarkIndex;
	else if ((bbs.cursub != null) && (typeof(bbs.cursub) == "number") && (bbs.curgrp == pGrpIndex))
		markIndex = bbs.cursub;
	// Double-check markIndex
	if (markIndex < 0)
		markIndex = 0;
	else if (markIndex >= msg_area.grp_list[grpIndex].sub_list.length)
		markIndex = msg_area.grp_list[grpIndex].sub_list.length - 1;


	// Ensure that the sub-board printf information is created for
	// this message group.
	this.BuildSubBoardPrintfInfoForGrp(grpIndex);


	// If there are no sub-boards in the given message group, then show
	// an error and return.
	if (msg_area.grp_list[grpIndex].sub_list.length == 0)
	{
		console.clear("\1n");
		console.print("\1y\1hThere are no sub-boards in the chosen group.\r\n\1p");
		return retObj;
	}

	// Returns the index of the bottommost sub-board that can be displayed on
	// the screen.
	//
	// Parameters:
	//  pTopSubIndex: The index of the topmost sub-board displayed on screen
	//  pNumItemsPerPage: The number of items per page
	function getBottommostSubIndex(pTopSubIndex, pNumItemsPerPage)
	{
		var bottomGrpIndex = topSubIndex + pNumItemsPerPage - 1;
		// If bottomGrpIndex is beyond the last index, then adjust it.
		if (bottomGrpIndex >= msg_area.grp_list[grpIndex].sub_list.length)
			bottomGrpIndex = msg_area.grp_list[grpIndex].sub_list.length - 1;
		return bottomGrpIndex;
	}


	// Figure out the index of the user's currently-selected sub-board.
	var selectedSubIndex = 0;
	if ((bbs.cursub != null) && (typeof(bbs.cursub) == "number"))
	{
		if ((bbs.curgrp != null) && (typeof(bbs.curgrp) == "number") && (bbs.curgrp == pGrpIndex))
			selectedSubIndex = bbs.cursub;
	}

	var listStartRow = 3+this.areaChangeHdrLines.length; // The row on the screen where the list will start
	var listEndRow = console.screen_rows - 1; // Row on screen where list will end
	var topSubIndex = 0;      // The index of the message group at the top of the list
	// Figure out the index of the last message group to appear on the screen.
	var numItemsPerPage = listEndRow - listStartRow + 1;
	var bottomSubIndex = getBottommostSubIndex(topSubIndex, numItemsPerPage);
	// Figure out how many pages are needed to list all the sub-boards.
	var numPages = Math.ceil(msg_area.grp_list[grpIndex].sub_list.length / numItemsPerPage);
	// Figure out the top index for the last page.
	var topIndexForLastPage = (numItemsPerPage * numPages) - numItemsPerPage;

	// If the highlighted row is beyond the current screen, then
	// go to the appropriate page.
	if (selectedSubIndex > bottomSubIndex)
	{
		var nextPageTopIndex = 0;
		while (selectedSubIndex > bottomSubIndex)
		{
			nextPageTopIndex = topSubIndex + numItemsPerPage;
			if (nextPageTopIndex < msg_area.grp_list[grpIndex].sub_list.length)
			{
				// Adjust topSubIndex and bottomSubIndex, and
				// refresh the list on the screen.
				topSubIndex = nextPageTopIndex;
				bottomSubIndex = getBottommostSubIndex(topSubIndex, numItemsPerPage);
			}
			else
				break;
		}

		// If we didn't find the correct page for some reason, then set the
		// variables to display page 1 and select the first message group.
		var foundCorrectPage = ((topSubIndex < msg_area.grp_list[grpIndex].sub_list.length) &&
		                       (selectedSubIndex >= topSubIndex) && (selectedSubIndex <= bottomSubIndex));
		if (!foundCorrectPage)
		{
			topSubIndex = 0;
			bottomSubIndex = getBottommostSubIndex(topSubIndex, numItemsPerPage);
			selectedSubIndex = 0;
		}
	}

	// Clear the screen, write the header, help line, and group list header, and output
	// a screenful of message groups.
	console.clear("\1n");
	this.DisplayAreaChgHdr(1); // Don't need to since it should already be drawn
	if (this.areaChangeHdrLines.length > 0)
		console.crlf();
	var pageNum = calcPageNum(topSubIndex, numItemsPerPage);
	this.WriteSubBrdListHdr1Line(grpIndex, numPages, pageNum);
	this.WriteKeyHelpLine();

	var curpos = new Object();
	curpos.x = 1;
	curpos.y = 2+this.areaChangeHdrLines.length;
	console.gotoxy(curpos);
	printf(this.subBoardListHdrPrintfStr, "Sub #", "Name", "# Posts", "Latest date & time");
	this.ListScreenfulOfSubBrds(grpIndex, topSubIndex, listStartRow, listEndRow, false, false);
	// Start of the input loop.
	var highlightScrenRow = 0; // The row on the screen for the highlighted group
	var userInput = "";        // Will store a keypress from the user
	var continueChoosingSubBrd = true;
	while (continueChoosingSubBrd)
	{
		// Highlight the currently-selected message group
		highlightScrenRow = listStartRow + (selectedSubIndex - topSubIndex);
		curpos.y = highlightScrenRow;
		if ((highlightScrenRow > 0) && (highlightScrenRow < console.screen_rows))
		{
			console.gotoxy(1, highlightScrenRow);
			this.WriteMsgSubBoardLine(grpIndex, selectedSubIndex, true);
		}

		// Get a key from the user (upper-case) and take action based upon it.
		userInput = getKeyWithESCChars(K_UPPER | K_NOCRLF);
		switch (userInput)
		{
			case KEY_UP: // Move up one message group in the list
				if (selectedSubIndex > 0)
				{
					// If the previous group index is on the previous page, then
					// display the previous page.
					var previousSubIndex = selectedSubIndex - 1;
					if (previousSubIndex < topSubIndex)
					{
						// Adjust topSubIndex and bottomSubIndex, and
						// refresh the list on the screen.
						topSubIndex -= numItemsPerPage;
						bottomSubIndex = getBottommostSubIndex(topSubIndex, numItemsPerPage);
						pageNum = calcPageNum(topSubIndex, numItemsPerPage);
						this.updatePageNumInHeader(pageNum, numPages, false, false);
						this.ListScreenfulOfSubBrds(grpIndex, topSubIndex, listStartRow, listEndRow, false, true);
					}
					else
					{
						// Display the current line un-highlighted.
						console.gotoxy(1, curpos.y);
						this.WriteMsgSubBoardLine(grpIndex, selectedSubIndex, false);
					}
					selectedSubIndex = previousSubIndex;
				}
				break;
			case KEY_DOWN: // Move down one message group in the list
				if (selectedSubIndex < msg_area.grp_list[grpIndex].sub_list.length - 1)
				{
					// If the next group index is on the next page, then display
					// the next page.
					var nextGrpIndex = selectedSubIndex + 1;
					if (nextGrpIndex > bottomSubIndex)
					{
						// Adjust topSubIndex and bottomSubIndex, and
						// refresh the list on the screen.
						topSubIndex += numItemsPerPage;
						bottomSubIndex = getBottommostSubIndex(topSubIndex, numItemsPerPage);
						pageNum = calcPageNum(topSubIndex, numItemsPerPage);
						this.updatePageNumInHeader(pageNum, numPages, false, false);
						this.ListScreenfulOfSubBrds(grpIndex, topSubIndex, listStartRow, listEndRow, false, true);
					}
					else
					{
						// Display the current line un-highlighted.
						console.gotoxy(1, curpos.y);
						this.WriteMsgSubBoardLine(grpIndex, selectedSubIndex, false);
					}
					selectedSubIndex = nextGrpIndex;
				}
				break;
			case KEY_HOME: // Go to the top message group on the screen
				if (selectedSubIndex > topSubIndex)
				{
					// Display the current line un-highlighted, then adjust
					// selectedSubIndex.
					console.gotoxy(1, curpos.y);
					this.WriteMsgSubBoardLine(grpIndex, selectedSubIndex, false);
					selectedSubIndex = topSubIndex;
					// Note: curpos.y is set at the start of the while loop.
				}
				break;
			case KEY_END: // Go to the bottom message group on the screen
				if (selectedSubIndex < bottomSubIndex)
				{
					// Display the current line un-highlighted, then adjust
					// selectedSubIndex.
					console.gotoxy(1, curpos.y);
					this.WriteMsgSubBoardLine(grpIndex, selectedSubIndex, false);
					selectedSubIndex = bottomSubIndex;
					// Note: curpos.y is set at the start of the while loop.
				}
				break;
			case KEY_ENTER: // Select the currently-highlighted sub-board; and we're done.
				continueChoosingSubBrd = false;
				retObj.subBoardChosen = true;
				retObj.subBoardIndex = selectedSubIndex;
				break;
			case KEY_PAGE_DOWN: // Go to the next page
				var nextPageTopIndex = topSubIndex + numItemsPerPage;
				if (nextPageTopIndex < msg_area.grp_list[grpIndex].sub_list.length)
				{
					// Adjust topSubIndex and bottomSubIndex, and
					// refresh the list on the screen.
					topSubIndex = nextPageTopIndex;
					pageNum = calcPageNum(topSubIndex, numItemsPerPage);
					bottomSubIndex = getBottommostSubIndex(topSubIndex, numItemsPerPage);
					this.updatePageNumInHeader(pageNum, numPages, false, false);
					this.ListScreenfulOfSubBrds(grpIndex, topSubIndex, listStartRow, listEndRow, false, true);
					selectedSubIndex = topSubIndex;
				}
				break;
			case KEY_PAGE_UP: // Go to the previous page
				var prevPageTopIndex = topSubIndex - numItemsPerPage;
				if (prevPageTopIndex >= 0)
				{
					// Adjust topSubIndex and bottomSubIndex, and
					// refresh the list on the screen.
					topSubIndex = prevPageTopIndex;
					pageNum = calcPageNum(topSubIndex, numItemsPerPage);
					bottomSubIndex = getBottommostSubIndex(topSubIndex, numItemsPerPage);
					this.updatePageNumInHeader(pageNum, numPages, false, false);
					this.ListScreenfulOfSubBrds(grpIndex, topSubIndex, listStartRow, listEndRow, false, true);
					selectedSubIndex = topSubIndex;
				}
				break;
			case 'F': // Go to the first page
				if (topSubIndex > 0)
				{
					topSubIndex = 0;
					pageNum = calcPageNum(topSubIndex, numItemsPerPage);
					bottomSubIndex = getBottommostSubIndex(topSubIndex, numItemsPerPage);
					this.updatePageNumInHeader(pageNum, numPages, false, false);
					this.ListScreenfulOfSubBrds(grpIndex, topSubIndex, listStartRow, listEndRow, false, true);
					selectedSubIndex = 0;
				}
				break;
			case 'L': // Go to the last page
				if (topSubIndex < topIndexForLastPage)
				{
					topSubIndex = topIndexForLastPage;
					pageNum = calcPageNum(topSubIndex, numItemsPerPage);
					bottomSubIndex = getBottommostSubIndex(topSubIndex, numItemsPerPage);
					this.updatePageNumInHeader(pageNum, numPages, false, false);
					this.ListScreenfulOfSubBrds(grpIndex, topSubIndex, listStartRow, listEndRow, false, true);
					selectedSubIndex = topIndexForLastPage;
				}
				break;
			case 'Q': // Quit
				continueChoosingSubBrd = false;
				break;
			case '?': // Show help
				this.ShowHelpScreen(true, true);
				console.pause();
				// Refresh the screen
				this.DisplayAreaChgHdr(1);
				console.gotoxy(1, 1+this.areaChangeHdrLines.length);
				this.WriteSubBrdListHdr1Line(grpIndex, numPages, pageNum);
				console.cleartoeol("\1n");
				this.WriteKeyHelpLine();
				console.gotoxy(1, 2+this.areaChangeHdrLines.length);
				printf(this.subBoardListHdrPrintfStr, "Sub #", "Name", "# Posts", "Latest date & time");
				this.ListScreenfulOfSubBrds(grpIndex, topSubIndex, listStartRow, listEndRow, false, true);
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
					// Move the cursor to the bottom of the screen and
					// prompt the user for the message number.
					console.gotoxy(1, console.screen_rows);
					console.clearline("\1n");
					console.print("\1cSub-board #: \1h");
					userInput = console.getnum(msg_area.grp_list[grpIndex].sub_list.length);
					// If the user made a selection, then set it in the
					// return object and don't continue the input loop.
					if (userInput > 0)
					{
						continueChoosingSubBrd = false;
						retObj.subBoardChosen = true;
						retObj.subBoardIndex = userInput - 1;
					}
					else
					{
						// The user didn't enter a selection.  Now we need to
						// re-draw the screen due to everything being moved
						// up one line.
						console.gotoxy(1, 1);
						this.WriteSubBrdListHdr1Line(grpIndex, numPages, pageNum);
						console.cleartoeol("\1n");
						this.WriteKeyHelpLine();
						console.gotoxy(1, 2);
						printf(this.subBoardListHdrPrintfStr, "Sub #", "Name", "# Posts", "Latest date & time");
						this.ListScreenfulOfSubBrds(grpIndex, topSubIndex, listStartRow, listEndRow, false, true);
					}
				}
				break;
		}
	}

	return retObj;
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
			this.ListMsgGrps();
			console.crlf();
			console.print("\1n\1b\1hþ \1n\1cWhich, \1hQ\1n\1cuit, or [\1h" + +(bbs.curgrp+1) + "\1n\1c]: \1h");
			// Accept Q (quit) or a file library number
			selectedGrp = console.getkeys("Q", msg_area.grp_list.length);

			// If the user just pressed enter (selectedGrp would be blank),
			// default to the current group.
			if (selectedGrp.toString() == "")
				selectedGrp = bbs.curgrp + 1;

			if (selectedGrp.toString() == "Q")
				continueChoosingMsgArea = false;
			else
			{
				// If the user specified a message group number, then
				// set it and let the user choose a sub-board within
				// the group.
				if (selectedGrp > 0)
				{
					// Set the default sub-board #: The current sub-board, or if the
					// user chose a different group, then this should be set
					// to the first sub-board.
					var defaultSubBoard = bbs.cursub + 1;
					if (selectedGrp-1 != bbs.curgrp)
						defaultSubBoard = 1;

					console.clear("\1n");
					var selectSubRetVal = this.SelectSubBoard_Traditional(selectedGrp-1, defaultSubBoard-1);
					// If the user chose a directory, then set bbs.curlib &
					// bbs.curdir and quit the file library loop.
					if ((selectedGrp.toString() != "Q") && (selectSubRetVal.subBoardIndex > -1))
					{
						bbs.curgrp = selectedGrp - 1;
						bbs.cursub = selectSubRetVal.subBoardIndex;
						continueChoosingMsgArea = false;
					}
				}
			}
		}
	}
	else
	{
		// Don't choose a group, just a sub-board within the user's current group.
		var selectSubRetVal = this.SelectSubBoard_Traditional(bbs.curgrp, bbs.cursub);
		// If the user chose a directory, then set bbs.curlib &
		// bbs.curdir and quit the file library loop.
		if (selectSubRetVal.subBoardIndex > -1)
			bbs.cursub = selectSubRetVal.subBoardIndex;
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
function DDMsgAreaChooser_selectSubBoard_Traditional(pGrpIdx, pDefaultSubBoardIdx)
{
	var retObj = new Object();
	retObj.subBoardChosen = false;
	retObj.subBoardIndex = -1;

	this.DisplayAreaChgHdr(1);
	if (this.areaChangeHdrLines.length > 0)
		console.crlf();
	this.ListSubBoardsInMsgGroup(pGrpIdx, pDefaultSubBoardIdx);
	console.crlf();
	console.print("\1n\1b\1hþ \1n\1cWhich, \1hQ\1n\1cuit, or [\1h" + +(pDefaultSubBoardIdx+1) + "\1n\1c]: \1h");
	// Accept Q (quit) or a sub-board number
	var selectedSubBoard = console.getkeys("Q", msg_area.grp_list[pGrpIdx].sub_list.length);

	// If the user just pressed enter (selectedSubBoard would be blank),
	// default the selected directory.
	var selectedSubBoardStr = selectedSubBoard.toString();
	if (selectedSubBoardStr == "")
		selectedSubBoard = pDefaultSubBoardIdx + 1; // Make this 1-based

	if (selectedSubBoard > 0)
	{
		retObj.subBoardChosen = true;
		retObj.subBoardIndex = selectedSubBoard - 1;
	}
	return retObj;
}

// For the DDMsgAreaChooser class: Lists all message groups (for the traditional
// user interface).
function DDMsgAreaChooser_listMsgGrps_Traditional()
{
	// Print the header
	this.WriteGrpListHdrLine();
	console.print("\1n");
	// List the message groups
	for (var i = 0; i < msg_area.grp_list.length; ++i)
	{
		console.crlf();
		this.WriteMsgGroupLine(i, false);
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
//  pSortType: Optional - A string describing how to sort the list (if desired):
//             "none": Default behavior - Sort by sub-board #
//             "dateAsc": Sort by date, ascending
//             "dateDesc": Sort by date, descending
//             "description": Sort by description
function DDMsgAreaChooser_listSubBoardsInMsgGroup_Traditional(pGrpIndex, pMarkIndex, pSortType)
{
	// Default to the current message group & sub-board if pGrpIndex
	// and pMarkIndex aren't specified.
	var grpIndex = bbs.curgrp;
	if ((pGrpIndex != null) && (typeof(pGrpIndex) == "number"))
		grpIndex = pGrpIndex;
	var highlightIndex = bbs.cursub;
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
	printf(this.subBoardListHdrPrintfStr, "Sub #", "Name", "# Posts", "Latest date & time");
	console.print("\1n");

	// List each sub-board in the message group.
	var subBoardArray = null;       // For sorting, if desired
	var newestDate = new Object(); // For storing the date of the newest post in a sub-board
	var msgBase = null;    // For opening the sub-boards with a MsgBase object
	var msgHeader = null;  // For getting the date & time of the newest post in a sub-board
	var subBoardNum = 0;   // 0-based sub-board number (because the array index is the number as a str)
	// If a sort type is specified, then add the sub-board information to
	// subBoardArray so that it can be sorted.
	if ((typeof(pSortType) == "string") && (pSortType != "") && (pSortType != "none"))
	{
		subBoardArray = new Array();
		var subBoardInfo = null;
		for (var arrSubBoardNum in msg_area.grp_list[grpIndex].sub_list)
		{
			// Open the current sub-board with the msgBase object.
			msgBase = new MsgBase(msg_area.grp_list[grpIndex].sub_list[arrSubBoardNum].code);
			if (msgBase.open())
			{
				subBoardInfo = new MsgSubBoardInfo();
				subBoardInfo.subBoardNum = +(arrSubBoardNum);
				subBoardInfo.description = msg_area.grp_list[grpIndex].sub_list[arrSubBoardNum].description;
				subBoardInfo.numPosts = msgBase.total_msgs;

				// Get the date & time when the last message was imported.
				if (msgBase.total_msgs > 0)
				{
					console.print("Here 1!\r\n\1p"); // Temporary
					//var msgHeader = msgBase.get_msg_header(true, msgBase.total_msgs-1, true);
					var msgHeader = null;
					var msgIdx = msgBase.total_msgs-1;
					while (msgHeader === null)
					{
						msgHeader = msgBase.get_msg_header(true, msgIdx, true);
						--msgIdx;
					}
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
		}
		// Free some memory?
		delete msgBase;

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
			console.print((subBoardArray[i].subBoardNum == highlightIndex) ? "\1n" + this.colors.areaMark + "*" : " ");
			printf(this.subBoardListPrintfInfo[grpIndex].printfStr, +(subBoardArray[i].subBoardNum+1),
			subBoardArray[i].description.substr(0, this.subBoardNameLen),
			subBoardArray[i].numPosts, strftime("%Y-%m-%d", subBoardArray[i].newestPostDate),
			strftime("%H:%M:%S", subBoardArray[i].newestPostDate));
		}
	}
	// If no sort type is specified, then output the sub-board information in
	// order of sub-board number.
	else
	{
		for (var arrSubBoardNum in msg_area.grp_list[grpIndex].sub_list)
		{
			// Open the current sub-board with the msgBase object.
			msgBase = new MsgBase(msg_area.grp_list[grpIndex].sub_list[arrSubBoardNum].code);
			if (msgBase.open())
			{
				// Get the date & time when the last message was imported.
				if (msgBase.total_msgs > 0)
				{
					//var msgHeader = msgBase.get_msg_header(true, msgBase.total_msgs-1, true);
					var msgHeader = null;
					var msgIdx = msgBase.total_msgs-1;
					while (msgHeader === null)
					{
						msgHeader = msgBase.get_msg_header(true, msgIdx, true);
						--msgIdx;
					}
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
				printf(this.subBoardListPrintfInfo[grpIndex].printfStr, +(subBoardNum+1),
				msg_area.grp_list[grpIndex].sub_list[arrSubBoardNum].description.substr(0, this.subBoardListPrintfInfo[grpIndex].nameLen),
				msgBase.total_msgs, newestDate.date, newestDate.time);

				msgBase.close();
			}

			// Free some memory?
			delete msgBase;
		}
	}
}

//////////////////////////////////////////////
// Message group list stuff (lightbar mode) //
//////////////////////////////////////////////

// Displays a screenful of message groups, for the lightbar interface.
//
// Parameters:
//  pStartIndex: The message group index to start at (0-based)
//  pStartScreenRow: The row on the screen to start at (1-based)
//  pEndScreenRow: The row on the screen to end at (1-based)
//  pClearScreenFirst: Boolean - Whether or not to clear the screen first
//  pBlankToEndRow: Boolean - Whether or not to write blank lines to the end
//                  screen row if there aren't enough message groups to fill
//                  the screen.
function DDMsgAreaChooser_listScreenfulOfMsgGrps(pStartIndex, pStartScreenRow,
                                                  pEndScreenRow, pClearScreenFirst,
                                                  pBlankToEndRow)
{
   // Check the parameters; If they're bad, then just return.
   if ((typeof(pStartIndex) != "number") ||
       (typeof(pStartScreenRow) != "number") ||
       (typeof(pEndScreenRow) != "number"))
   {
      return;
   }
   if ((pStartIndex < 0) || (pStartIndex >= msg_area.grp_list.length))
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

   // Calculate the ending index to use for the message groups array.
   var endIndex = pStartIndex + (pEndScreenRow-pStartScreenRow);
   if (endIndex >= msg_area.grp_list.length)
      endIndex = msg_area.grp_list.length - 1;
   var onePastEndIndex = endIndex + 1;

   // Check to make sure bbs.curgrp is valid (it might not be for brand-new users).
   var curgrpValid = ((bbs.curgrp != null) && (typeof(bbs.curgrp) != "undefined"));

   // Clear the screen, go to the specified screen row, and display the message
   // group information.
   if (pClearScreenFirst)
      console.clear("\1n");
   console.gotoxy(1, pStartScreenRow);
   var grpIndex = pStartIndex;
   for (; grpIndex < onePastEndIndex; ++grpIndex)
   {
      this.WriteMsgGroupLine(grpIndex, false);
      if (grpIndex < endIndex)
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
   console.print(((typeof(bbs.curgrp) == "number") && (pGrpIndex == bbs.curgrp)) ? this.colors.areaMark + "*" : " ");
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

// Displays a screenful of message sub-boards, for the lightbar interface.
//
// Parameters:
//  pGrpIndex: The index of the message group (0-based)
//  pStartSubIndex: The message sub-board index to start at (0-based)
//  pStartScreenRow: The row on the screen to start at (1-based)
//  pEndScreenRow: The row on the screen to end at (1-based)
//  pClearScreenFirst: Boolean - Whether or not to clear the screen first
//  pBlankToEndRow: Boolean - Whether or not to write blank lines to the end
//                  screen row if there aren't enough message groups to fill
//                  the screen.
function DDMsgAreaChooser_listScreenfulOfSubBrds(pGrpIndex, pStartSubIndex,
                                                  pStartScreenRow, pEndScreenRow,
                                                  pClearScreenFirst, pBlankToEndRow)
{
	// Check the parameters; If they're bad, then just return.
	if ((typeof(pGrpIndex) != "number") ||
	    (typeof(pStartSubIndex) != "number") ||
	    (typeof(pStartScreenRow) != "number") ||
	    (typeof(pEndScreenRow) != "number"))
	{
		return;
	}
	if ((pGrpIndex < 0) || (pGrpIndex >= msg_area.grp_list.length))
		return;
	if ((pStartSubIndex < 0) ||
	    (pStartSubIndex >= msg_area.grp_list[pGrpIndex].sub_list.length))
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
	var endIndex = pStartSubIndex + (pEndScreenRow-pStartScreenRow);
	if (endIndex >= msg_area.grp_list[pGrpIndex].sub_list.length)
		endIndex = msg_area.grp_list[pGrpIndex].sub_list.length - 1;
	var onePastEndIndex = endIndex + 1;

	// Clear the screen and go to the specified screen row.
	if (pClearScreenFirst)
		console.clear("\1n");
	console.gotoxy(1, pStartScreenRow);

	// Start listing the sub-boards.

	var subIndex = pStartSubIndex;
	for (; subIndex < onePastEndIndex; ++subIndex)
	{
		this.WriteMsgSubBoardLine(pGrpIndex, subIndex, false);
		if (subIndex < endIndex)
		console.crlf();
	}

	// If pBlankToEndRow is true and we're not at the end row yet, then
	// write blank lines to the end row.
	if (pBlankToEndRow)
	{
		var screenRow = pStartScreenRow + (endIndex - pStartSubIndex) + 1;
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

// For the DDMsgAreaChooser class: Writes a message sub-board information line.
//
// Parameters:
//  pGrpIndex: The index of the message group (assumed to be valid)
//  pSubIndex: The index of the sub-board within the message group to write (assumed to be valid)
//  pHighlight: Boolean - Whether or not to write the line highlighted.
function DDMsgAreaChooser_writeMsgSubBrdLine(pGrpIndex, pSubIndex, pHighlight)
{
	console.print("\1n");
	// Write the highlight background color if pHighlight is true.
	if (pHighlight)
		console.print(this.colors.bkgHighlight);

	// Determine if pGrpIndex and pSubIndex specify the user's
	// currently-selected group and sub-board.
	var currentSub = false;
	if ((typeof(bbs.curgrp) == "number") && (typeof(bbs.cursub) == "number"))
		currentSub = ((pGrpIndex == bbs.curgrp) && (pSubIndex == bbs.cursub));

	// Open the current sub-board with the msgBase object (so that we can get
	// the date & time of the last imporeted message).
	var msgBase = new MsgBase(msg_area.grp_list[pGrpIndex].sub_list[pSubIndex].code);
	if (msgBase.open())
	{
		var newestDate = new Object(); // For storing the date of the newest post
		// Get the date & time when the last message was imported.
		if (msgBase.total_msgs > 0)
		{
			//var msgHeader = msgBase.get_msg_header(true, msgBase.total_msgs-1, true);
			var msgHeader = null;
			var msgIdx = msgBase.total_msgs-1;
			while (msgHeader === null)
			{
				msgHeader = msgBase.get_msg_header(true, msgIdx, true);
				--msgIdx;
			}
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

		// Print the sub-board information line.
		console.print(currentSub ? this.colors.areaMark + "*" : " ");
		printf((pHighlight ? this.subBoardListPrintfInfo[pGrpIndex].highlightPrintfStr : this.subBoardListPrintfInfo[pGrpIndex].printfStr),
		       +(pSubIndex+1),
		msg_area.grp_list[pGrpIndex].sub_list[pSubIndex].description.substr(0, this.subBoardListPrintfInfo[pGrpIndex].nameLen),
		msgBase.total_msgs, newestDate.date, newestDate.time);
		msgBase.close();

		// Free some memory?
		delete msgBase;
	}
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

      this.subBoardListPrintfInfo[pGrpIndex] = new Object();
      this.subBoardListPrintfInfo[pGrpIndex].numMsgsLen = greatestNumMsgs.toString().length;
      // Sub-board name length: With a # items length of 4, this should be
      // 47 for an 80-column display.
      this.subBoardListPrintfInfo[pGrpIndex].nameLen = console.screen_columns -
                                   this.areaNumLen -
                                   this.subBoardListPrintfInfo[pGrpIndex].numMsgsLen -
                                   this.dateLen - this.timeLen - 7;
      // Create the printf strings
      this.subBoardListPrintfInfo[pGrpIndex].printfStr =
               " " + this.colors.areaNum
               + "%" + this.areaNumLen + "d "
               + this.colors.desc + "%-"
               + this.subBoardListPrintfInfo[pGrpIndex].nameLen + "s "
               + this.colors.numItems + "%"
               + this.subBoardListPrintfInfo[pGrpIndex].numMsgsLen + "d "
               + this.colors.latestDate + "%" + this.dateLen + "s "
               + this.colors.latestTime + "%" + this.timeLen + "s";
      this.subBoardListPrintfInfo[pGrpIndex].highlightPrintfStr =
                              "\1n" + this.colors.bkgHighlight + " "
                              + "\1n" + this.colors.bkgHighlight
                              + this.colors.areaNumHighlight
                              + "%" + this.areaNumLen + "d \1n"
                              + this.colors.bkgHighlight
                              + this.colors.descHighlight + "%-"
                              + this.subBoardListPrintfInfo[pGrpIndex].nameLen + "s \1n"
                              + this.colors.bkgHighlight
                              + this.colors.numItemsHighlight + "%"
                              + this.subBoardListPrintfInfo[pGrpIndex].numMsgsLen + "d \1n"
                              + this.colors.bkgHighlight
                              + this.colors.dateHighlight + "%" + this.dateLen + "s \1n"
                              + this.colors.bkgHighlight
                              + this.colors.timeHighlight + "%" + this.timeLen + "s\1n";
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
	var msgHdr = new Object();
	msgHdr.subject = (typeof(pSubject) == "string" ? pSubject : "");
	msgHdr.when_imported_time = 0;
	msgHdr.when_written_time = 0;
	msgHdr.when_written_zone = 0;
	msgHdr.date = "Fri, 1 Jan 1960 00:00:00 -0000";
	msgHdr.attr = 0;
	msgHdr.to = "Nobody";
	msgHdr.from = "Nobody";
	msgHdr.number = 0;
	msgHdr.offset = 0;
	msgHdr.isBogus = true;
	return msgHdr;
}