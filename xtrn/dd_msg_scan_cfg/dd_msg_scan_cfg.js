/* This is a script that allows the user to toggle unread/to-you newscan options for
 * message sub-boards on the BBS with a lightbar interface (or traditional/non-lightbar
 * interface).
 *
 * Author: Eric Oulashin (AKA Nightfox)
 * BBS: Digital Distortion
 * BBS address: digitaldistortionbbs.com (or digdist.bbsindex.com)
 *
 * Date       Author            Description
 * 2013-??-?? Eric Oulashin     Started
 * 2025-09-13 Eric Oulashin     Updates & refactoring for configuration.
 *                              Added an interfaceStyle configuration
 *                              option and moved colors to a separate
 *                              theme configuration file.
 * 2025-09-23 Eric Oulashin     Version 1.00
 *                              / and N search implemented.
 *                              Releasing this version.
 */

"use strict";

// 3.14 or higher for console.term_supports()
// This script requires Synchronet version 3.18 or higher (for mouse hotspot support).
// Exit if the Synchronet version is below the minimum.
if (system.version_num < 31800)
{
	var message = "\x01n\x01h\x01y\x01i* Warning:\x01n\x01h\x01w This "
	             + "requires version \x01g3.18\x01w or\r\n"
	             + "higher of Synchronet.  This BBS is using version \x01g" + system.version
	             + "\x01w.  Please notify the sysop.";
	console.crlf();
	console.print(message);
	console.crlf();
	console.pause();
	exit();
}


require("sbbsdefs.js", "K_UPPER");
require("dd_lightbar_menu.js", "DDLightbarMenu");
require("key_defs.js", "KEY_LEFT");


// Version information
var SCAN_CFG_VERSION = "1.00";
var SCAN_CFG_DATE = "2025-09-23";


// Characters for display
// Box-drawing/border characters: Single-line
var UPPER_LEFT_SINGLE = "\xDA";
var HORIZONTAL_SINGLE = "\xC4";
var UPPER_RIGHT_SINGLE = "\xBF";
var VERTICAL_SINGLE = "\xB3";
var LOWER_LEFT_SINGLE = "\xC0";
var LOWER_RIGHT_SINGLE = "\xD9";
var T_SINGLE = "\xC2";
var LEFT_T_SINGLE = "\xC3";
var RIGHT_T_SINGLE = "\xB4";
var BOTTOM_T_SINGLE = "\xC1";
var CROSS_SINGLE = "\xC5";
// Box-drawing/border characters: Double-line
var UPPER_LEFT_DOUBLE = "\xC9";
var HORIZONTAL_DOUBLE = "\xCD";
var UPPER_RIGHT_DOUBLE = "\xBB";
var VERTICAL_DOUBLE = "\xBA";
var LOWER_LEFT_DOUBLE = "\xC8";
var LOWER_RIGHT_DOUBLE = "\xBC";
var T_DOUBLE = "\xCB";
var LEFT_T_DOUBLE = "\xCC";
var RIGHT_T_DOUBLE = "\xB9";
var BOTTOM_T_DOUBLE = "\xCA";
var CROSS_DOUBLE = "\xCE";
// Box-drawing/border characters: Vertical single-line with horizontal double-line
var UPPER_LEFT_VSINGLE_HDOUBLE = "\xD5";
var UPPER_RIGHT_VSINGLE_HDOUBLE = "\xB8";
var LOWER_LEFT_VSINGLE_HDOUBLE = "\xD4";
var LOWER_RIGHT_VSINGLE_HDOUBLE = "\xBE";
// Other special characters
var DOT_CHAR = "\xF9";
var CHECK_CHAR = "\xFB";
var THIN_RECTANGLE_LEFT = "\xDD";
var THIN_RECTANGLE_RIGHT = "\xDE";

var BLOCK1 = "\xB0"; // Dimmest block
var BLOCK2 = "\xB1";
var BLOCK3 = "\xB2";
var BLOCK4 = "\xDB"; // Brightest block
var MID_BLOCK = "\xDC";
var TALL_UPPER_MID_BLOCK = "\xFE";
var UPPER_CENTER_BLOCK = "\xDF";
var LOWER_CENTER_BLOCK = "\xDC";

var UP_ARROW = "\x18"; // ascii(24);
var DOWN_ARROW = "\x19"; // ascii(25);
var LEFT_ARROW = "\x11"; // ascii(17);
var RIGHT_ARROW = "\x10"; // ascii(16);
var UP_DOWN_ARROWS = "\x12"; // ascii(18);
var LEFT_RIGHT_ARROWS = "\x1D"; // ascii(29);
var RIGHT_POINTING_TRIANGLE = "\x10"; // ascii(16);



// Sub-board sort options for changing to another sub-board.
const SUB_BOARD_SORT_NONE = 0;
const SUB_BOARD_SORT_ALPHABETICAL = 1;
const SUB_BOARD_SORT_LATEST_MSG_DATE_OLDEST_FIRST = 2;
const SUB_BOARD_SORT_LATEST_MSG_DATE_NEWEST_FIRST = 3;

// Timeout (in milliseconds) when waiting for search input
var SEARCH_TIMEOUT_MS = 10000;
const ERROR_PAUSE_WAIT_MS = 1500;


// Read the configuration flie
var gConfig = readConfigFile("dd_msg_scan_cfg.ini");


// Let the user choose a message group
var msgGrpListStartCol = 4;
var msgGrpListStartRow = 5;
var msgGrpMenuWidth = console.screen_columns - 5;
// For the menu height, leave 2 rows at the bottom of the screen for
// the group # prompt row and for the key help line at the bottom row
var msgGrpMenuHeight = console.screen_rows - msgGrpListStartRow - 4;
// If configured for a traditional user interface or the user's terminal doesn't support
// ANSI, then have the menu be wider to take up the full width of the screen (won't
// display the border)
if (!useLightbarInterface(gConfig))
{
	msgGrpListStartCol = 1;
	msgGrpMenuWidth = console.screen_columns - 1;
}
var continueOn = true;
while (continueOn)
{
	var msgGrpIdx = letUserChooseMsgGrp(gConfig, msgGrpListStartCol, msgGrpListStartRow, msgGrpMenuWidth, msgGrpMenuHeight);
	if (typeof(msgGrpIdx) === "number" && msgGrpIdx >= 0 && msgGrpIdx < msg_area.grp_list.length)
		letUserDoSubBoardConfigForMsgGrp(gConfig, msgGrpIdx, msgGrpListStartCol, msgGrpListStartRow, msgGrpMenuWidth, msgGrpMenuHeight);
	else
		continueOn = false;
}

////////////////////////////////////////////////////
// Functions

// Lets the user choose a message group
//
// Return value: The chosen index of the message group (0-based), or null if none is chosen
function letUserChooseMsgGrp(pConfig, msgGrpListStartCol, msgGrpListStartRow, msgGrpMenuWidth, msgGrpMenuHeight)
{
	console.clear("N");
	if (letUserChooseMsgGrp.menuInfo === undefined)
	{
		// Create the message group menu
		letUserChooseMsgGrp.menuInfo = createMsgGrpMenu(pConfig, msgGrpListStartCol, msgGrpListStartRow, msgGrpMenuWidth, msgGrpMenuHeight, 6);
	}
	var msgGrpMenu = letUserChooseMsgGrp.menuInfo.menu;

	// Row on the screen for prompting for a group number
	var promptRow = console.screen_rows - 1;
	// Text for prompting for a group number
	var groupNumPromptText = format("%sGroup #%s: %s", "\x01n" + pConfig.colors.lightbarNumInputPromptText,
	                                "\x01n" + pConfig.colors.lightbarNumInputPromptColon,
	                                "\x01n" + pConfig.colors.lightbarNumInputPromptUserInput);
	// Length for clearing the group # prompt text
	var groupNumPromptClearLen = console.strlen(groupNumPromptText) + msgGrpMenu.NumItems().toString().length;

	// Header line format string
	var hdrPrintfStr = "\x01n" + pConfig.colors.msgGrpHdr + "%" + letUserChooseMsgGrp.menuInfo.grpNumLen + "s %-"
	                 + letUserChooseMsgGrp.menuInfo.grpDescLen + "s %" + letUserChooseMsgGrp.menuInfo.numSubBoardsLen + "s\x01n";

	// For item searching
	var searchText = "";
	var itemSearchStartIdx = 0;
	// Override the message group's OnItemNav function to set the item
	// search index to the current menu item index. The OnItemNav function
	// isn't set in createMsgGrpMenu, so we can simply set it here.
	msgGrpMenu.OnItemNav = function(pOldItemIdx, pNewItemIdx) {
		itemSearchStartIdx = pNewItemIdx;
	};

	console.attributes = "N";
	// Show the message group menu & get the message group from the user
	var userChoice = null;
	var showHeaderLines = true;
	var redrawMenu = true;
	var writeKeyHelpLine = true;
	var continueOn = true;
	while (continueOn)
	{
		// Show the header lines if desired
		if (showHeaderLines)
		{
			if (useLightbarInterface(pConfig))
				console.gotoxy(1, 2);
			else
				console.crlf();
			console.center("\x01n" + pConfig.colors.msgGrpHdr + "Choose a message group to configure\x01n");
			if (useLightbarInterface(pConfig))
				console.gotoxy(msgGrpListStartCol, 3);
			printf(hdrPrintfStr, "#", "Description", "# Subs");
			if (useLightbarInterface(pConfig))
				console.gotoxy(msgGrpListStartCol, 4);
			else
				console.crlf();
			showHeaderLines = false;
		}
		// If we need to show the key help line at the bottom of the screen, then show it
		// if the user's terminal supports ANSI
		if (writeKeyHelpLine && useLightbarInterface(pConfig))
			showKeyHelpForChoosingMsgGrp(console.screen_rows);
		writeKeyHelpLine = false;
		// If we want to redraw the menu, then we need to draw our
		// custom border around it
		if (redrawMenu)
		{
			if (useLightbarInterface(pConfig))
				drawColoredBorder(msgGrpMenu.pos.x-1, msgGrpMenu.pos.y-1, msgGrpMenu.size.width+2, msgGrpMenu.size.height+2, pConfig.colors);
		}
		// Show the menu & get the user's choice
		userChoice = msgGrpMenu.GetVal(true, redrawMenu);
		var lastUserKeyUpper = msgGrpMenu.lastUserInput.toUpperCase();
		redrawMenu = false;
		if (userChoice != null) // User made a choice from the menu
			continueOn = false;
		else
		{
			// If the user entered a numeric digit, then treat it as
			// the start of the message group number.
			if (/^[0-9]$/.test(msgGrpMenu.lastUserInput))
			{
				// Put the user's input back in the input buffer to be
				// used for getting the rest of the message group number.
				console.ungetstr(msgGrpMenu.lastUserInput);
				if (useLightbarInterface(pConfig))
				{
					// Move the cursor to the bottom of the screen and
					// prompt the user for the message number.
					console.gotoxy(1, promptRow);
					console.clearline("\x01n");
					console.gotoxy(1, promptRow);
				}
				else
					console.crlf();
				console.print("\x01n" + groupNumPromptText);
				var userInput = console.getnum(msgGrpMenu.NumItems());
				console.attributes = "N";
				if (typeof(userInput) === "number" && userInput > 0 && userInput <= msgGrpMenu.NumItems())
				{
					//printf("\x01n\r\nHere 1: %s\r\n\x01p", userInput); // Temporary
					userChoice = userInput - 1;
					continueOn = false;
				}
				else
				{
					//console.print("\x01n\r\nHere 2\r\n\x01p"); // Temporary
					// The user didn't enter a group number, so keep showing the menu,
					// but don't redraw it.
					redrawMenu = false;
					writeKeyHelpLine = false;
					continueOn = true;
					// If using ANSI, erase the prompt text at the prompt row
					if (useLightbarInterface(pConfig))
					{
						console.gotoxy(1, promptRow);
						printf("\x01n%-*s", groupNumPromptClearLen, "");
					}
				}
				continue; // Continue to the next iteration of the while (continueOn) loop
			}
			// / key: Search; N: Next (if search text has already been entered
			if (msgGrpMenu.lastUserInput == "/" || lastUserKeyUpper == "N")
			{
				if (msgGrpMenu.lastUserInput == "/")
				{
					if (useLightbarInterface(pConfig))
					{
						console.gotoxy(1, console.screen_rows);
						console.cleartoeol("\x01n");
						console.gotoxy(1, console.screen_rows);
					}
					else
						console.print("\x01n\r\n");
					var promptText = "Search: ";
					console.print(promptText);
					var newSearchText = console.getstr(console.screen_columns - promptText.length - 1, K_UPPER|K_NOCRLF|K_GETSTR|K_NOSPIN|K_LINE);
					if (newSearchText != searchText)
						itemSearchStartIdx = 0;
					searchText = newSearchText;
				}
				// If the user entered text, then do the search, and if found,
				// found, go to the page and select the item indicated by the
				// search.
				if (searchText.length > 0)
				{
					var foundItem = false;
					var searchTextUpper = searchText.toUpperCase();
					var numItems = msgGrpMenu.NumItems();
					for (var i = itemSearchStartIdx; i < numItems; ++i)
					{
						var menuItem = msgGrpMenu.GetItem(i);
						if (menuItem != null)
						{
							var msgGrpNameUpper = menuItem.text.toUpperCase();
							if (msgGrpNameUpper.indexOf(searchTextUpper) > -1)
							{
								msgGrpMenu.SetSelectedItemIdx(i);
								foundItem = true;
								itemSearchStartIdx = i + 1;
								if (itemSearchStartIdx >= msgGrpMenu.NumItems())
									itemSearchStartIdx = 0;
								break;
							}
						}
					}
					redrawMenu = foundItem;
					if (!foundItem)
						displayErrorMsgAtBottomScreenRow(pConfig, "No result(s) found", false);
				}
				else
				{
					itemSearchStartIdx = 0;
					redrawMenu = false;
				}
				writeKeyHelpLine = true;
			}
			// Help screen
			else if (msgGrpMenu.lastUserInput == "?")
			{
				// Show help for choosing a message group
				console.clear("N");
				displayChooseMsgGroupHelp();
				console.clear("N");
				showHeaderLines = true;
				redrawMenu = true;
				writeKeyHelpLine = true;
			}
			else if (msgGrpMenu.lastUserInput == "q" || msgGrpMenu.lastUserInput == "Q")
			{
				// Quit: Do nothing; let userChoice be null
				continueOn = false;
			}
			else
				continueOn = false;
		}
	}
	return userChoice;
}

// Shows the key help line for choosing a message group
//
// Parameters:
//  pScreenRow: The row on the screen to draw the line at
function showKeyHelpForChoosingMsgGrp(pScreenRow)
{
	// Construct the help line text if it hasn't been constructed already
	if (showKeyHelpForChoosingMsgGrp.str == undefined)
	{
		showKeyHelpForChoosingMsgGrp.str = gConfig.colors.lightbarHelpLineHotkey + "Up"
		          + gConfig.colors.lightbarHelpLineGeneral + "/"
		          + gConfig.colors.lightbarHelpLineHotkey + "Dn"
		          + gConfig.colors.lightbarHelpLineGeneral + "/"
		          + gConfig.colors.lightbarHelpLineHotkey + "<-"
		          + gConfig.colors.lightbarHelpLineGeneral + "/"
		          + gConfig.colors.lightbarHelpLineHotkey + "->"
		          + gConfig.colors.lightbarHelpLineGeneral + "/"
		          + gConfig.colors.lightbarHelpLineHotkey + "PgUp"
		          + gConfig.colors.lightbarHelpLineGeneral + "/"
		          + gConfig.colors.lightbarHelpLineHotkey + "Dn"
		          + gConfig.colors.lightbarHelpLineGeneral + "/"
		          + gConfig.colors.lightbarHelpLineHotkey + "N"
		          + gConfig.colors.lightbarHelpLineGeneral + "/"
		          + gConfig.colors.lightbarHelpLineHotkey + "P"
		          + gConfig.colors.lightbarHelpLineGeneral + ", "
		          + gConfig.colors.lightbarHelpLineHotkey + "HOME"
		          + gConfig.colors.lightbarHelpLineGeneral + ", "
		          + gConfig.colors.lightbarHelpLineHotkey + "END"
		          + gConfig.colors.lightbarHelpLineGeneral + ", "
		          + gConfig.colors.lightbarHelpLineHotkey + "F"
		          + gConfig.colors.lightbarHelpLineParen + ")"
		          + gConfig.colors.lightbarHelpLineGeneral + "irst, "
		          + gConfig.colors.lightbarHelpLineHotkey + "L"
		          + gConfig.colors.lightbarHelpLineParen + ")"
		          + gConfig.colors.lightbarHelpLineGeneral + "ast, "
		          + gConfig.colors.lightbarHelpLineHotkey + "#"
		          + gConfig.colors.lightbarHelpLineGeneral + ", "
		          + gConfig.colors.lightbarHelpLineHotkey + LOWER_LEFT_SINGLE + HORIZONTAL_SINGLE + RIGHT_POINTING_TRIANGLE
				  + gConfig.colors.lightbarHelpLineGeneral + " Select, "
		          + gConfig.colors.lightbarHelpLineHotkey + "ESC"
		          + gConfig.colors.lightbarHelpLineGeneral + "/"
		          + gConfig.colors.lightbarHelpLineHotkey + "Q"
		          + gConfig.colors.lightbarHelpLineParen + ")"
		          + gConfig.colors.lightbarHelpLineGeneral + "uit, "
		          + gConfig.colors.lightbarHelpLineHotkey + "?";
		// Pad the help line so that the text is centered on the screen
		var maxWidth = console.screen_columns - 1;
		var numLeftSideChars = Math.floor((maxWidth - console.strlen(showKeyHelpForChoosingMsgGrp.str))/2);
		showKeyHelpForChoosingMsgGrp.str = format("%-*s", numLeftSideChars, "") + showKeyHelpForChoosingMsgGrp.str;
		var numRightSideChars = maxWidth - console.strlen(showKeyHelpForChoosingMsgGrp.str);
		showKeyHelpForChoosingMsgGrp.str += format("%-*s", numRightSideChars, "");
		showKeyHelpForChoosingMsgGrp.str = "\x01n" + gConfig.colors.lightbarHelpLineBkg + showKeyHelpForChoosingMsgGrp.str;
	}
	// Move the cursor to the desired location (if applicable) and write the help line
	if (useLightbarInterface(gConfig))
		console.gotoxy(1, pScreenRow);
	console.print(showKeyHelpForChoosingMsgGrp.str);
}

// Lets the user configure their scan pointers for a message group
//
// Parameters:
//  pConfig: The configuration object
//  pGrpIdx: The index of a message group
//  pStartCol: The starting column on the screen for the menus
//  pStartRow: The starting row on the screen for the menus
//  pTotalWidth: The total width for all the menus
//  pMenuHeight: The menu height for all menus
function letUserDoSubBoardConfigForMsgGrp(pConfig, pGrpIdx, pStartCol, pStartRow, pTotalWidth, pMenuHeight)
{
	if (msg_area.grp_list[pGrpIdx].sub_list.length == 0)
	{
		console.clear("N");
		console.center(gConfig.colors.msgGrpHdr + "There are no sub-boards for this group\x01n");
		console.crlf();
		console.pause();
		return;
	}

	console.clear("N");

	// For toggle-all unread and to-you options
	var unreadAllToggle = true;
	var toYouAllToggle = true;

	// Sub-board sorting option
	/*
	const SUB_BOARD_SORT_NONE = 0;
	const SUB_BOARD_SORT_ALPHABETICAL = 1;
	const SUB_BOARD_SORT_LATEST_MSG_DATE_OLDEST_FIRST = 2;
	const SUB_BOARD_SORT_LATEST_MSG_DATE_NEWEST_FIRST = 3;
	*/
	var subBoardSorting = SUB_BOARD_SORT_NONE;
	//subBoardSorting = SUB_BOARD_SORT_ALPHABETICAL; // Temporary

	// If lightbar is enabled and the user's terminal supports ANSI, use a lightbar 3-menu interface;
	// otherwise, use a traditional user interface.
	if (useLightbarInterface(pConfig))
	{
		// Create the 3 menus for the given message group
		var menuInfo = createSubBrdAndToggleMenus(gConfig, pGrpIdx, subBoardSorting, pStartCol, pStartRow, pTotalWidth, pMenuHeight);
		var subBoardMenu = menuInfo.subBoardMenu;
		var unreadToggleMenu = menuInfo.unreadToggleMenu;
		var toYouToggleMenu = menuInfo.toYouToggleMenu;
		// Menu header line header string
		var hdrPrintfStr = "\x01n" + gConfig.colors.msgGrpHdr + "%-" + subBoardMenu.size.width + "s %-"
		                 + unreadToggleMenu.size.width + "s %-" + toYouToggleMenu.size.width + "s\x01n";

		// We'll start user input on the Unread toggle menu. For now, set the To-You
		// toggle menu selected item color to be the same as the normal item color
		// so it appears that it doesn't have a selected item
		toYouToggleMenu.colors.selectedItemColor = toYouToggleMenu.colors.itemColor;

		// TODO: Color configuration for this prompt text
		var subBoardNumPromptText = "\x01n\x01cSub-board #\x01g\x01h: \x01c";
		var subBoardNumPromptTextLen = console.strlen(subBoardNumPromptText);
		// The current (toggle) menu should start out as the Unread toggle menu
		var currentToggleMenu = menuInfo.unreadToggleMenu;

		// For item searching
		var searchText = "";
		var itemSearchStartIdx = 0;
		// Override the OnItemNav functions of the unread & to-you toggle
		// menus to set the item search index to the current menu item index.
		// The OnItemNav functions are set in createSubBrdAndToggleMenus(),
		// so we need to make a copy of their existing OnItemNav functions
		// to call those as well as set the item search start index.
		unreadToggleMenu.OriginalOnItemNav = unreadToggleMenu.OnItemNav;
		unreadToggleMenu.OnItemNav = function(pOldItemIdx, pNewItemIdx) {
			this.OriginalOnItemNav(pOldItemIdx, pNewItemIdx);
			itemSearchStartIdx = pNewItemIdx;
		};
		toYouToggleMenu.OriginalOnItemNav = toYouToggleMenu.OnItemNav;
		toYouToggleMenu.OnItemNav = function(pOldItemIdx, pNewItemIdx) {
			this.OriginalOnItemNav(pOldItemIdx, pNewItemIdx);
			itemSearchStartIdx = pNewItemIdx;
		};

		// User input loop
		var showHeaderLines = true;
		var showMenus = true;
		var writeKeyHelpLine = true;
		var refreshToggleMenu = false;
		var continueOn = true;
		while (continueOn)
		{
			// If we want to, show the header lines
			if (showHeaderLines)
			{
				console.gotoxy(1, 1);
				// Message group description
				//console.center(gConfig.colors.msgGrpHdr + msg_area.grp_list[pGrpIdx].description + "\x01n");
				console.print(gConfig.colors.msgGrpHdr);
				console.center(gConfig.colors.msgGrpHdr + msg_area.grp_list[pGrpIdx].description);
				console.center("U: Toggle unread for all on/off; Y: Toggle to-you for all on/off\x01n");
				console.gotoxy(pStartCol, pStartRow-2);
				printf(hdrPrintfStr, "Sub-board", "Unread", "To-you");
				showHeaderLines = false;
			}
			// If we want to, show the key help on the bottom row of the screen
			if (writeKeyHelpLine)
			{
				showKeyHelpForScanToggle(console.screen_rows);
				writeKeyHelpLine = false;
			}
			if (showMenus)
			{
				subBoardMenu.Draw();
				unreadToggleMenu.Draw();
				toYouToggleMenu.Draw();
				drawColoredBorder(pStartCol-1, pStartRow-1, pTotalWidth+2, pMenuHeight+2, gConfig.colors);
				showMenus = false;
			}
			// Calling GetVal() to show the menu & allow user input, but not
			// capturing its return value because we're interested in the
			// user's last keypress. Any chosen value is irrelevant; we just
			// want to allow toggling the unread/to-you for the sub-boards.
			currentToggleMenu.GetVal(refreshToggleMenu);
			var lastUserKeyUpper = currentToggleMenu.lastUserInput.toUpperCase();
			refreshToggleMenu = false;
			var lastUserInputUpper = currentToggleMenu.lastUserInput.toUpperCase();
			if (lastUserInputUpper == KEY_ESC || lastUserInputUpper == "Q" || console.aborted)
				continueOn = false;
			else if (lastUserInputUpper == KEY_LEFT)
			{
				// Going to the Unread toggle menu. Swap the toggle menu and selected item color.
				currentToggleMenu.WriteItemAtItsLocation(currentToggleMenu.selectedItemIdx, false, false);
				currentToggleMenu = menuInfo.unreadToggleMenu;
				toYouToggleMenu.colors.selectedItemColor = toYouToggleMenu.colors.itemColor;
				unreadToggleMenu.colors.itemColor = "\x01n" + gConfig.colors.toggleMenuItemColor;
				unreadToggleMenu.colors.selectedItemColor = "\x01n" + gConfig.colors.toggleMenuSelectedItemColor;
				unreadToggleMenu.WriteItemAtItsLocation(unreadToggleMenu.selectedItemIdx, true, false);
			}
			else if (lastUserInputUpper == KEY_RIGHT)
			{
				// Going to the To-You toggle menu. Swap the toggle menu and selected item color
				currentToggleMenu.WriteItemAtItsLocation(currentToggleMenu.selectedItemIdx, false, false);
				currentToggleMenu = menuInfo.toYouToggleMenu;
				unreadToggleMenu.colors.selectedItemColor = unreadToggleMenu.colors.itemColor;
				toYouToggleMenu.colors.itemColor = "\x01n" + gConfig.colors.toggleMenuItemColor;
				toYouToggleMenu.colors.selectedItemColor = "\x01n" + gConfig.colors.toggleMenuSelectedItemColor;
				toYouToggleMenu.WriteItemAtItsLocation(toYouToggleMenu.selectedItemIdx, true, false);
			}
			else if (lastUserInputUpper == "U")
			{
				// Toggle unread for all
				unreadAllToggle = !unreadAllToggle;
				for (var subIdx = 0; subIdx < msg_area.grp_list[pGrpIdx].sub_list.length; ++subIdx)
					toggleSubBoardScanCfgOpt(pGrpIdx, subIdx, SCAN_CFG_NEW, unreadAllToggle);
				unreadToggleMenu.Draw();
				//refreshToggleMenu = false; // This is the default
			}
			else if (lastUserInputUpper == "Y")
			{
				// Toggle to-you for all
				toYouAllToggle = !toYouAllToggle;
				for (var subIdx = 0; subIdx < msg_area.grp_list[pGrpIdx].sub_list.length; ++subIdx)
					toggleSubBoardScanCfgOpt(pGrpIdx, subIdx, SCAN_CFG_TOYOU, toYouAllToggle);
				toYouToggleMenu.Draw();
				//refreshToggleMenu = false; // This is the default
			}
			// If the user entered a numeric digit, then treat it as
			// the start of the sub-board number.
			else if (/^[0-9]$/.test(lastUserInputUpper))
			{
				var promptRow = console.screen_rows - 1;
				// Put the user's input back in the input buffer to
				// be used for getting the rest of the message number.
				console.ungetstr(lastUserInputUpper);
				if (useLightbarInterface(pConfig))
				{
					// Move the cursor to the bottom of the screen and
					// prompt the user for the message number.
					console.gotoxy(1, promptRow);
					console.clearline("\x01n");
					console.gotoxy(1, promptRow);
				}
				else
					console.crlf();
				console.print("\x01n" + subBoardNumPromptText);
				var userInput = console.getnum(currentToggleMenu.NumItems());
				console.attributes = "N";
				if (typeof(userInput) === "number" && userInput > 0 && userInput <= currentToggleMenu.NumItems())
				{
					// TODO: Current selected item & current top item on all menus, and redraw if needed
					//refreshToggleMenu = false; // This is the default
				}
				else
				{
					// The user didn't enter a group number, so keep showing the menu,
					// but don't redraw it.
					//refreshToggleMenu = false; // This is the default
					// If using lightbar/ANSI, erase the prompt text at the prompt row
					if (useLightbarInterface(pConfig))
					{
						console.gotoxy(1, promptRow);
						printf("\x01n%-*s", subBoardNumPromptTextLen, "");
					}
				}
			}
			// / key: Search; N: Next (if search text has already been entered
			if (currentToggleMenu.lastUserInput == "/" || lastUserKeyUpper == "N")
			{
				if (currentToggleMenu.lastUserInput == "/")
				{
					if (useLightbarInterface(pConfig))
					{
						console.gotoxy(1, console.screen_rows);
						console.cleartoeol("\x01n");
						console.gotoxy(1, console.screen_rows);
					}
					else
						console.print("\x01n\r\n");
					var promptText = "Search: ";
					console.print(promptText);
					var newSearchText = console.getstr(console.screen_columns - promptText.length - 1, K_UPPER|K_NOCRLF|K_GETSTR|K_NOSPIN|K_LINE);
					if (newSearchText != searchText)
						itemSearchStartIdx = 0;
					searchText = newSearchText;
				}
				// If the user entered text, then do the search, and if found,
				// found, go to the page and select the item indicated by the
				// search.
				if (searchText.length > 0)
				{
					var foundItem = false;
					var numItems = subBoardMenu.NumItems();
					// The menus should all have the same number of items, but
					// check just in case
					if (numItems == unreadToggleMenu.NumItems() && toYouToggleMenu.NumItems())
					{
						var searchTextUpper = searchText.toUpperCase();
						for (var i = itemSearchStartIdx; i < numItems; ++i)
						{
							var menuItem = subBoardMenu.GetItem(i);
							if (menuItem != null)
							{
								var msgGrpNameUpper = menuItem.text.toUpperCase();
								if (msgGrpNameUpper.indexOf(searchTextUpper) > -1)
								{
									subBoardMenu.SetSelectedItemIdx(i);
									unreadToggleMenu.SetSelectedItemIdx(i);
									toYouToggleMenu.SetSelectedItemIdx(i);
									foundItem = true;
									itemSearchStartIdx = i + 1;
									if (itemSearchStartIdx >= subBoardMenu.NumItems())
										itemSearchStartIdx = 0;
									break;
								}
							}
						}
					}
					showMenus = foundItem;
					if (!foundItem)
						displayErrorMsgAtBottomScreenRow(pConfig, "No result(s) found", false);
				}
				else
				{
					itemSearchStartIdx = 0;
					showMenus = false;
				}
				writeKeyHelpLine = true;
			}
			// Help screen
			else if (currentToggleMenu.lastUserInput == "?")
			{
				// Show help for message scan configuration
				console.clear("N");
				displayScanConfigHelp(msg_area.grp_list[pGrpIdx].description);
				console.clear("N");
				showHeaderLines = true;
				showMenus = true;
				writeKeyHelpLine = true;
				refreshToggleMenu = false;
			}
		}
	}
	else
	{
		// Non-lightbar interface, without the 3-menu mechanism

		var colors = gConfig.colors;

		// Calculate widths for the various text columns
		var subNumWidth = msg_area.grp_list[pGrpIdx].sub_list.length.toString().length;
		var unreadColWidth = 6;
		var toYouColWidth = 6;
		var subBoardColWidth = console.screen_columns - subNumWidth - unreadColWidth - toYouColWidth - 1;

		// Header format string
		var subHdrPrintfStr = "\x01n" + colors.msgGrpHdr
		                    + "%" + subNumWidth + "s "
		                    + "%-" + (subBoardColWidth-subNumWidth-1) + "s %-"
		                    + unreadColWidth + "s %-" + toYouColWidth + "s\x01n\r\n";
		// Sub-board list format string
		var subInfoPrintfStr = "\x01n" + colors.subBoardMenuNumColor
		                     + "%" + subNumWidth + "s \x01n"
		                     + colors.subBoardMenuItemColor + "%-" + (subBoardColWidth-subNumWidth-1) + "s \x01n"
		                     + colors.toggleMenuItemColor + "%- " + unreadColWidth + "s %-"
		                     + toYouColWidth + "s\x01n\r\n";
		// Sub-board # prompt text
		var subNumPromptText = format("\x01n%sU: Toggle unread for all on/off; Y: Toggle to-you for all on/off,\r\n" +
		                              "or sub-board # (Q/Ctrl-C to quit)%s: %s", "\x01n" + colors.traditionalNumInputPromptText,
		                              "\x01n" + colors.traditionalNumInputPromptColon,
		                              "\x01n" + colors.traditionalNumInputPromptUserInput);
		// Sub-board toggle prompt text
		var subTogglePromptText = format("\x01n%sToggle (U)nread, To-(Y)ou, or (Q)uit\x01n%s: %s", colors.lightbarNumInputPromptText,
		                                 colors.lightbarNumInputPromptColon, colors.lightbarNumInputPromptUserInput);
		// User input loop
		var continueOn = true;
		while (continueOn)
		{
			// Print the headers
			// Message group description
			//console.center(colors.msgGrpHdr + msg_area.grp_list[pGrpIdx].description + "\x01n");
			console.print(colors.msgGrpHdr);
			console.center(colors.msgGrpHdr + msg_area.grp_list[pGrpIdx].description);
			console.center("U: Toggle unread for all on/off; Y: Toggle to-you for all on/off\x01n");
			// Sub-board list header
			printf(subHdrPrintfStr, "#", "Sub-board", "Unread", "To-you");
			// Print the sub-boards & allow the user to toggle unread/to-you scans
			for (var subIdx = 0; subIdx < msg_area.grp_list[pGrpIdx].sub_list.length; ++subIdx)
			{
				var newEnabled = Boolean(msg_area.grp_list[pGrpIdx].sub_list[subIdx].scan_cfg & SCAN_CFG_NEW);
				var toYouEnabled = Boolean(msg_area.grp_list[pGrpIdx].sub_list[subIdx].scan_cfg & SCAN_CFG_TOYOU);
				var subBoardDesc = msg_area.grp_list[pGrpIdx].sub_list[subIdx].description.substr(0, subBoardColWidth);
				printf(subInfoPrintfStr, subIdx+1, subBoardDesc, getToggleStr(newEnabled), getToggleStr(toYouEnabled));
			}
			// Prompt & toggle
			console.print(subNumPromptText);
			var userInput = console.getkeys("UYQ?/N" + KEY_ESC, msg_area.grp_list[pGrpIdx].sub_list.length, K_UPPER|K_NOCRLF|K_NOSPIN|K_CTRLKEYS);
			var userInputStr = userInput.toString();
			if (userInputStr == "Q" || userInputStr == KEY_ESC || console.aborted)
				continueOn = false;
			else if (userInputStr == "U")
			{
				// Toggle unread for all
				unreadAllToggle = !unreadAllToggle;
				for (var subIdx = 0; subIdx < msg_area.grp_list[pGrpIdx].sub_list.length; ++subIdx)
					toggleSubBoardScanCfgOpt(pGrpIdx, subIdx, SCAN_CFG_NEW, unreadAllToggle);
				console.crlf();
			}
			else if (userInputStr == "Y")
			{
				// Toggle to-you for all
				toYouAllToggle = !toYouAllToggle;
				for (var subIdx = 0; subIdx < msg_area.grp_list[pGrpIdx].sub_list.length; ++subIdx)
					toggleSubBoardScanCfgOpt(pGrpIdx, subIdx, SCAN_CFG_TOYOU, toYouAllToggle);
				console.crlf();
			}
			else if (userInputStr == "/")
			{
				// TODO: Search
			}
			else if (userInputStr == "N")
			{
				// TODO: Next search
			}
			else if (userInputStr == "?")
			{
				// Show help for message scan configuration
				console.clear("N");
				displayScanConfigHelp(msg_area.grp_list[pGrpIdx].description);
				console.clear("N");
			}
			else
			{
				var subBoardNum = userInput;
				if (subBoardNum <= 0 || console.aborted) // User backpaced and entered nothing, or entered Q or Ctrl-C
					continueOn = false;
				else
				{
					var subIdx = subBoardNum - 1;
					console.crlf();
					printf("\x01n" + colors.msgGrpHdr + "%s - %s\x01n", msg_area.grp_list[pGrpIdx].description,
						   msg_area.grp_list[pGrpIdx].sub_list[subIdx].description);
					console.crlf();
					console.print(subTogglePromptText);
					userInput = console.getkeys("UYQ" + KEY_ESC, -1, K_UPPER|K_NOCRLF|K_NOSPIN|K_CTRLKEYS);
					if (userInput == "U")
						toggleSubBoardScanCfgOpt(pGrpIdx, subIdx, SCAN_CFG_NEW);
					else if (userInput == "Y")
						toggleSubBoardScanCfgOpt(pGrpIdx, subIdx, SCAN_CFG_TOYOU);
					/*
					else if (userInput == "Q" || userInput == KEY_ESC || userInput == "")
						continueOn = false;
					*/
				}
			}
		}
	}
}


// Shows the key help line for choosing a message group
//
// Parameters:
//  pScreenRow: The row on the screen to draw the line at
function showKeyHelpForScanToggle(pScreenRow)
{
	// Construct the help line text if it hasn't been constructed already
	if (showKeyHelpForScanToggle.str == undefined)
	{
		showKeyHelpForScanToggle.str = gConfig.colors.lightbarHelpLineHotkey + "Up"
		          + gConfig.colors.lightbarHelpLineGeneral + "/"
		          + gConfig.colors.lightbarHelpLineHotkey + "Dn"
		          + gConfig.colors.lightbarHelpLineGeneral + "/"
		          + gConfig.colors.lightbarHelpLineHotkey + "<-"
		          + gConfig.colors.lightbarHelpLineGeneral + "/"
		          + gConfig.colors.lightbarHelpLineHotkey + "->"
		          + gConfig.colors.lightbarHelpLineGeneral + "/"
		          + gConfig.colors.lightbarHelpLineHotkey + "PgUp"
		          + gConfig.colors.lightbarHelpLineGeneral + "/"
		          + gConfig.colors.lightbarHelpLineHotkey + "Dn"
		          + gConfig.colors.lightbarHelpLineGeneral + ", "
		          + gConfig.colors.lightbarHelpLineHotkey + "HOME"
		          + gConfig.colors.lightbarHelpLineGeneral + ", "
		          + gConfig.colors.lightbarHelpLineHotkey + "END"
		          + gConfig.colors.lightbarHelpLineGeneral + ", "
		          + gConfig.colors.lightbarHelpLineHotkey + LOWER_LEFT_SINGLE + HORIZONTAL_SINGLE + RIGHT_POINTING_TRIANGLE
		          + gConfig.colors.lightbarHelpLineGeneral + " Toggle, "
		          + gConfig.colors.lightbarHelpLineHotkey + "ESC"
		          + gConfig.colors.lightbarHelpLineGeneral + "/"
		          + gConfig.colors.lightbarHelpLineHotkey + "Q"
		          + gConfig.colors.lightbarHelpLineParen + ")"
		          + gConfig.colors.lightbarHelpLineGeneral + "uit, "
		          + gConfig.colors.lightbarHelpLineHotkey + "?";
		// Pad the help line so that the text is centered on the screen
		var maxWidth = console.screen_columns - 1;
		var numLeftSideChars = Math.floor((maxWidth - console.strlen(showKeyHelpForScanToggle.str))/2);
		showKeyHelpForScanToggle.str = format("%-*s", numLeftSideChars, "") + showKeyHelpForScanToggle.str;
		var numRightSideChars = maxWidth - console.strlen(showKeyHelpForScanToggle.str);
		showKeyHelpForScanToggle.str += format("%-*s", numRightSideChars, "");
		showKeyHelpForScanToggle.str = "\x01n" + gConfig.colors.lightbarHelpLineBkg + showKeyHelpForScanToggle.str;
	}
	// Move the cursor to the desired location (if applicable) and write the help line
	if (useLightbarInterface(gConfig))
		console.gotoxy(1, pScreenRow);
	console.print(showKeyHelpForScanToggle.str);
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

// Creates the message group menu (a DDLightbarMenu object)
//
// Parameters:
//  pConfig: The configuration object containing the program settings & colors
//  pListStartCol: The starting screen column for the message group menu
//  pListStartRow: The starting screen row for the message group menu
//  pMenuWidth: The width to use for the menu
//  pMenuHeight: The height to use for the menu
//  pNumSubBoardsMinLen: The minimum length for the 'number of sub-boards' column
//                       (to fit the label)
//
// Return value: An object with the following properties:
//               menu: The DDLightbarMenu object representing the message group menu
//               grpNumLen: The length of the group number field
//               grpDescLen: The length of the group description field
//               numSubBoardsLen: The length of the '# sub-boards' field
function createMsgGrpMenu(pConfig, pListStartCol, pListStartRow, pMenuWidth, pMenuHeight, pNumSubBoardsMinLen)
{
	var msgGrpMenuWidth = typeof(pMenuWidth) === "number" ? pMenuWidth : console.screen_columns - 1;
	var msgGrpMenuHeight = 0;
	if (typeof(pMenuHeight) === "number")
		msgGrpMenuHeight = pMenuHeight;
	else
		msgGrpMenuHeight = console.screen_rows - pListStartRow;
	if (msg_area.grp_list.length < msgGrpMenuHeight)
		msgGrpMenuHeight = msg_area.grp_list.length;

	// The maximum length of the group # column
	var grpNumLength = msg_area.grp_list.length.toString().length;
	// Figure out the maximum length for the number of sub-boards in each message group
	var numSubBoardsLength = 0;
	for (var i = 0; i < msg_area.grp_list.length; ++i)
	{
		var numSubsLength = msg_area.grp_list[i].sub_list.length.toString().length;
		if (numSubsLength > numSubBoardsLength)
			numSubBoardsLength = numSubsLength;
	}
	if (typeof(pNumSubBoardsMinLen) === "number" && numSubBoardsLength < pNumSubBoardsMinLen)
		numSubBoardsLength = pNumSubBoardsMinLen;
	// The room leftover will be available for the group description
	var grpDescLength = msgGrpMenuWidth - grpNumLength - numSubBoardsLength - 2; // - 3
	// If the list of message groups is more than the menu height, then subtract 1 from
	// the group description length to allow for the scrollbar column in the menu
	if (msg_area.grp_list.length > msgGrpMenuHeight)
		--grpDescLength;

	// Start & end indexes for the various items in each mssage group list row
	// Description, # sub-boards
	var msgGrpListIdxes = {
		descStart: 0,
		descEnd: grpDescLength
	};
	msgGrpListIdxes.numItemsStart = msgGrpListIdxes.descEnd;
	msgGrpListIdxes.numItemsEnd = msgGrpListIdxes.numItemsStart + numSubBoardsLength;
	// Set numItemsEnd to -1 to let the whole rest of the lines be colored
	msgGrpListIdxes.numItemsEnd = -1;
	var listStartCol = typeof(pListStartCol) === "number" ? pListStartCol : 1;
	var listStartRow = typeof(pListStartRow) === "number" && pListStartRow >= 1 && pListStartRow <= console.screen_rows-10 ? pListStartRow : 3;
	var msgGrpMenu = new DDLightbarMenu(listStartCol, pListStartRow, msgGrpMenuWidth, msgGrpMenuHeight);
	msgGrpMenu.allowANSI = useLightbarInterface(pConfig);
	msgGrpMenu.multiSelect = false;
	msgGrpMenu.ampersandHotkeysInItems = false;
	msgGrpMenu.wrapNavigation = true;
	//scrollbarEnabled = false;
	msgGrpMenu.numberedMode = true;
	msgGrpMenu.colors.itemNumColor = "\x01n" + pConfig.colors.msgGrpMenuNumColor;
	msgGrpMenu.colors.highlightedItemNumColor = "\x01n" + pConfig.colors.msgGrpMenuNumHighlightColor;
	msgGrpMenu.scrollbarEnabled = true;
	msgGrpMenu.borderEnabled = false;
	msgGrpMenu.SetColors({
		itemColor: [{start: msgGrpListIdxes.descStart, end: msgGrpListIdxes.descEnd, attrs: "\x01n" + pConfig.colors.msgGrpMenuDescColor},
		            {start: msgGrpListIdxes.numItemsStart, end: msgGrpListIdxes.numItemsEnd, attrs: "\x01n" + pConfig.colors.msgGrpMenuNumItemsColor}],
		selectedItemColor: [{start: msgGrpListIdxes.descStart, end: msgGrpListIdxes.descEnd, attrs: "\x01n" + pConfig.colors.msgGrpMenuDescHighlightColor},
		                    {start: msgGrpListIdxes.numItemsStart, end: msgGrpListIdxes.numItemsEnd, attrs: "\x01n" + pConfig.colors.msgGrpMenuNumItemsHighlightColor}]
	});

	// Add additional keypresses for quitting the menu's input loop so we can
	// respond to these keys
	msgGrpMenu.AddAdditionalQuitKeys("qQ/nN?0123456789");
	// Additional page naviagation keys
	msgGrpMenu.AddAdditionalPageUpKeys("Pp"); // Previous page
	msgGrpMenu.AddAdditionalPageDownKeys("Nn"); // Next page
	msgGrpMenu.AddAdditionalFirstPageKeys("Ff"); // First page
	msgGrpMenu.AddAdditionalLastPageKeys("Ll"); // Last page

	// Add items for the message groups
	var itemPrintfStr = "%-" + grpDescLength + "s %" + numSubBoardsLength + "d";
	for (var i = 0; i < msg_area.grp_list.length; ++i)
	{
		//var grpText = format(itemPrintfStr, i+1, msg_area.grp_list[i].description.substr(0, grpDescLength), msg_area.grp_list[i].sub_list.length);
		var grpText = format(itemPrintfStr, msg_area.grp_list[i].description.substr(0, grpDescLength), msg_area.grp_list[i].sub_list.length);
		msgGrpMenu.Add(grpText, i);
	}

	// Set the currently selected item to the current group
	//msgGrpMenu.SetSelectedItemIdx(msg_area.sub[this.subBoardCode].grp_index);
	/*
	msgGrpMenu.selectedItemIdx = msg_area.sub[this.subBoardCode].grp_index;
	if (msgGrpMenu.selectedItemIdx >= msgGrpMenu.topItemIdx+msgGrpMenu.GetNumItemsPerPage())
		msgGrpMenu.topItemIdx = msgGrpMenu.selectedItemIdx - msgGrpMenu.GetNumItemsPerPage() + 1;
	*/

	return {
		menu: msgGrpMenu,
		grpNumLen: grpNumLength,
		grpDescLen: grpDescLength,
		numSubBoardsLen: numSubBoardsLength
	};
}

// Creates the 3 menus for sub-board scan toggle options: Sub-board list, unread, and to-you toggles
//
// Return value: An object with the following properties:
//               subBoardMenu: The sub-board menu for the message group
//               unreadToggleMenu: The menu for 'Unread' toggle options
//               toYouToggleMenu: The menu for 'To You' togle options
function createSubBrdAndToggleMenus(pConfig, pGrpIdx, pSubBoardSorting, pListStartCol, pListStartRow, pTotalWidth, pMenuHeight)
{
	// Get the length of the total number of sub-boards (for column header widths)
	//var numSubBoardsWidth = msg_area.grp_list[pGrpIdx].sub_list.length.toString().length;
	// Set up the menu widths
	var unreadMenuWidth = 6;
	var toYouMenuWidth = 6;
	var subBoardMenuWidth = pTotalWidth - unreadMenuWidth - toYouMenuWidth - 2; // 2 spaces

	var unreadMenuStartCol = pListStartCol + subBoardMenuWidth + 1;
	var toYouMenuStartCol = unreadMenuStartCol + unreadMenuWidth + 1;

	// Create an array of sub-board internal codes, which we might sort according
	// to pSubBoardSorting
	var subCodes = [];
	for (var subIdx = 0; subIdx < msg_area.grp_list[pGrpIdx].sub_list.length; ++subIdx)
		subCodes.push(msg_area.grp_list[pGrpIdx].sub_list[subIdx].code);
	// Sort the sub-boards if applicable
	//msg_area.sub
	//description
	//grp_index
	//index
	//grp_name
	switch (pSubBoardSorting)
	{
		case SUB_BOARD_SORT_NONE:
		default:
			break;
		case SUB_BOARD_SORT_ALPHABETICAL:
			subCodes.sort(function(pSubCodeA, pSubCodeB)
			{
				var subDescA = msg_area.sub[pSubCodeA].description.toUpperCase();
				var subDescB = msg_area.sub[pSubCodeB].description.toUpperCase();
				if (subDescA < subDescB)
					return -1;
				else if (subDescA == subDescB)
					return 0;
				if (subDescA > subDescB)
					return 1;
			});
			break;
		case SUB_BOARD_SORT_LATEST_MSG_DATE_OLDEST_FIRST:
			// TODO
			break;
		case SUB_BOARD_SORT_LATEST_MSG_DATE_NEWEST_FIRST:
			// TODO
			break;
	}

	// Create the menu of sub-board descriptions
	// TODO: Decide how to handle non-lightbar interface in this situation
	var subBoardMenu = new DDLightbarMenu(pListStartCol, pListStartRow, subBoardMenuWidth, pMenuHeight);
	subBoardMenu.subCodes = subCodes;
	subBoardMenu.allowANSI = useLightbarInterface(pConfig);
	subBoardMenu.wrapNavigation = true;
	subBoardMenu.scrollbarEnabled = true;
	subBoardMenu.colors.itemColor = "\x01n" + pConfig.colors.subBoardMenuItemColor;
	subBoardMenu.colors.selectedItemColor = "\x01n" + pConfig.colors.subBoardMenuSelectedItemColor;
	subBoardMenu.numberedMode = true;
	subBoardMenu.colors.itemNumColor = "\x01n" + gConfig.colors.subBoardMenuNumColor;
	subBoardMenu.colors.highlightedItemNumColor = "\x01n" + pConfig.colors.subBoardMenuSelectedItemColor;
	var maxSubBoardDescWidth = subBoardMenuWidth;
	// If the number of sub-boards is more than the menu height, then subtract 1 from
	// the maximum description width to allow for the scrollbar
	if (msg_area.grp_list[pGrpIdx].sub_list.length > pMenuHeight)
		--maxSubBoardDescWidth;
	for (var i = 0; i < subCodes.length; ++i)
	{
		var subCode = subCodes[i];
		var subDesc = msg_area.sub[subCode].description.substr(0, maxSubBoardDescWidth); // subBoardMenuWidth
		//subBoardMenu.Add(subDesc, i);
		subBoardMenu.Add(subDesc, msg_area.sub[subCode].index);
	}
	/*
	for (var subIdx = 0; subIdx < msg_area.grp_list[pGrpIdx].sub_list.length; ++subIdx)
	{
		var subDesc = msg_area.grp_list[pGrpIdx].sub_list[subIdx].description.substr(0, maxSubBoardDescWidth); // subBoardMenuWidth
		subBoardMenu.Add(subDesc, subIdx);
	}
	*/

	// Create the Unread toggle menu
	var unreadToggleMenu = new DDLightbarMenu(unreadMenuStartCol, pListStartRow, unreadMenuWidth, pMenuHeight);
	unreadToggleMenu.subCodes = subCodes;
	unreadToggleMenu.allowANSI = useLightbarInterface(pConfig);
	unreadToggleMenu.wrapNavigation = true;
	unreadToggleMenu.scrollbarEnabled = false;
	unreadToggleMenu.colors.itemColor = "\x01n" + pConfig.colors.toggleMenuItemColor;
	unreadToggleMenu.colors.selectedItemColor = "\x01n" + pConfig.colors.toggleMenuSelectedItemColor;
	// Change the menu's NumItems() and GetItem() function to reference
	// the message sub-board list in the given sub-board
	unreadToggleMenu.grpIdx = pGrpIdx;
	unreadToggleMenu.NumItems = function() {
		//return msg_area.grp_list[this.grpIdx].sub_list.length;
		return this.subCodes.length;
	};
	unreadToggleMenu.GetItem = function(pItemIndex) {
		//var enabled = Boolean(msg_area.grp_list[this.grpIdx].sub_list[pItemIndex].scan_cfg & SCAN_CFG_NEW);
		//var menuItemObj = this.MakeItemWithRetval(pItemIndex);
		var subCode = this.subCodes[pItemIndex];
		var enabled = Boolean(msg_area.sub[subCode].scan_cfg & SCAN_CFG_NEW);
		var menuItemObj = this.MakeItemWithRetval(subCode);
		//menuItemObj.text = format(" [%s]  ", enabled ? CHECK_CHAR : " ");
		menuItemObj.text = getToggleStr(enabled);
		return menuItemObj;
	};
	// Alternate: Adding items to the menu
	/*
	for (var subIdx = 0; subIdx < msg_area.grp_list[pGrpIdx].sub_list.length; ++subIdx)
	{
		var enabled = Boolean(msg_area.grp_list[this.grpIdx].sub_list[subIdx].scan_cfg & SCAN_CFG_NEW);
		unreadToggleMenu.Add(format(" [%s]  ", enabled ? CHECK_CHAR : " "), subIdx);
	}
	*/
	// Additional exit keys
	var additionalQuitKeysForToggleMenus = "uUyYqQ/nN?0123456789";
	unreadToggleMenu.AddAdditionalQuitKeys(additionalQuitKeysForToggleMenus + KEY_RIGHT); // Allow right arrow (to go to the to-you toggle menu)
	unreadToggleMenu.AddAdditionalQuitKeys(KEY_LEFT); // To disallow the left key to move the selected item up

	// Create the To You toggle menu
	var toYouToggleMenu = new DDLightbarMenu(toYouMenuStartCol, pListStartRow, toYouMenuWidth, pMenuHeight);
	toYouToggleMenu.subCodes = subCodes;
	toYouToggleMenu.allowANSI = useLightbarInterface(pConfig);
	toYouToggleMenu.wrapNavigation = true;
	toYouToggleMenu.scrollbarEnabled = false;
	toYouToggleMenu.colors.itemColor = "\x01n" + pConfig.colors.toggleMenuItemColor;
	toYouToggleMenu.colors.selectedItemColor = "\x01n" + pConfig.colors.toggleMenuSelectedItemColor;
	// Change the menu's NumItems() and GetItem() function to reference
	// the message sub-board list in the given sub-board
	// TODO: Allow sorting; copy the information into another array besides
	// just using msg_area.grp_list.sub_list
	//_Add(pText, pRetval, pHotkey, pSelectable, pIsUTF8)
	/*
	for (var subIdx = 0; subIdx < msg_area.grp_list[pGrpIdx].sub_list.length; ++subIdx)
	{
	}
	*/
	
	toYouToggleMenu.grpIdx = pGrpIdx;
	toYouToggleMenu.NumItems = function() {
		//return msg_area.grp_list[this.grpIdx].sub_list.length;
		return this.subCodes.length;
	};
	toYouToggleMenu.GetItem = function(pItemIndex) {
		//var enabled = Boolean(msg_area.grp_list[this.grpIdx].sub_list[pItemIndex].scan_cfg & SCAN_CFG_TOYOU);
		//var menuItemObj = this.MakeItemWithRetval(pItemIndex);
		var subCode = this.subCodes[pItemIndex];
		var enabled = Boolean(msg_area.sub[subCode].scan_cfg & SCAN_CFG_TOYOU);
		var menuItemObj = this.MakeItemWithRetval(subCode);
		menuItemObj.text = format(" [%s]  ", enabled ? CHECK_CHAR : " ");
		return menuItemObj;
	};
	
	// Alternate: Adding items to the menu
	/*
	for (var subIdx = 0; subIdx < msg_area.grp_list[pGrpIdx].sub_list.length; ++subIdx)
	{
		var enabled = Boolean(msg_area.grp_list[pGrpIdx].sub_list[subIdx].scan_cfg & SCAN_CFG_TOYOU);
		toYouToggleMenu.Add(format(" [%s]  ", enabled ? CHECK_CHAR : " "), subIdx);
	}
	*/
	// Additional exit keys
	toYouToggleMenu.AddAdditionalQuitKeys(additionalQuitKeysForToggleMenus + KEY_LEFT); // Allow left arrow (to go to the unread toggle menu)
	toYouToggleMenu.AddAdditionalQuitKeys(KEY_RIGHT); // To disallow the right key to move the selected item down

	// Set item navigation functions for the Unread & To-You toggle menus to select the
	// current item index for the other menus
	unreadToggleMenu.OnItemNav = function(pOldItemIdx, pNewItemIdx)
	{
		var otherToggleMenu = toYouToggleMenu;
		var needToggleMenuScroll = (otherToggleMenu.topItemIdx != this.topItemIdx);
		otherToggleMenu.selectedItemIdx = this.selectedItemIdx;
		otherToggleMenu.topItemIdx = this.topItemIdx;
		if (needToggleMenuScroll)
			otherToggleMenu.Draw({});
		// Sub-board menu
		var needSubBoardMenuScroll = (subBoardMenu.topItemIdx != this.topItemIdx);
		if (!needSubBoardMenuScroll)
		{
			subBoardMenu.WriteItemAtItsLocation(pOldItemIdx, false, false);
			subBoardMenu.WriteItemAtItsLocation(pNewItemIdx, true, false);
		}
		subBoardMenu.selectedItemIdx = this.selectedItemIdx;
		subBoardMenu.topItemIdx = this.topItemIdx;
		if (needSubBoardMenuScroll)
			subBoardMenu.Draw({});
	};
	toYouToggleMenu.OnItemNav = function(pOldItemIdx, pNewItemIdx)
	{
		var otherToggleMenu = unreadToggleMenu;
		var needToggleMenuScroll = (otherToggleMenu.topItemIdx != this.topItemIdx);
		otherToggleMenu.selectedItemIdx = this.selectedItemIdx;
		otherToggleMenu.topItemIdx = this.topItemIdx;
		if (needToggleMenuScroll)
			otherToggleMenu.Draw({});
		// Sub-board menu
		var needSubBoardMenuScroll = (subBoardMenu.topItemIdx != this.topItemIdx);
		if (!needSubBoardMenuScroll)
		{
			subBoardMenu.WriteItemAtItsLocation(pOldItemIdx, false, false);
			subBoardMenu.WriteItemAtItsLocation(pNewItemIdx, true, false);
		}
		subBoardMenu.selectedItemIdx = this.selectedItemIdx;
		subBoardMenu.topItemIdx = this.topItemIdx;
		if (needSubBoardMenuScroll)
			subBoardMenu.Draw({});
	};

	// Set the item select function on the toggle menus to toggle the
	// newscan/to-you scan selection for that sub-board
	unreadToggleMenu.OnItemSelect = function(pSubCode, pSelected)
	{
		toggleSubBoardScanCfgOpt(this.grpIdx, msg_area.sub[pSubCode].index, SCAN_CFG_NEW);
		// Refresh the item on the menu to show its current toggle status
		this.WriteItemAtItsLocation(this.selectedItemIdx, true, false);
	};
	toYouToggleMenu.OnItemSelect = function(pSubCode, pSelected)
	{
		toggleSubBoardScanCfgOpt(this.grpIdx, msg_area.sub[pSubCode].index, SCAN_CFG_TOYOU);
		// Refresh the item on the menu to show its current toggle status
		this.WriteItemAtItsLocation(this.selectedItemIdx, true, false);
	};

	return {
		subBoardMenu: subBoardMenu,
		unreadToggleMenu: unreadToggleMenu,
		toYouToggleMenu: toYouToggleMenu
	};
}
// Toggles a scan_cfg option in a sub-board
//
// Parameters:
//  pGrpIdx: The index of the message group
//  pSubIdx: The index of the sub-board within the message group
//  pScanCfgOpt: The scan configuration option to toggle
//  pEnableOverride: Optional - This can be a boolean that specifies whether to enable it
//                   or disable it.  If this is omitted, then this function will simply
//                   toggle it to the opposite of how it's currently set.
function toggleSubBoardScanCfgOpt(pGrpIdx, pSubIdx, pScanCfgOpt, pEnableOverride)
{
	if (typeof(pEnableOverride) === "boolean")
	{
		if (pEnableOverride)
			msg_area.grp_list[pGrpIdx].sub_list[pSubIdx].scan_cfg ^= pScanCfgOpt; // Enable
		else
			msg_area.grp_list[pGrpIdx].sub_list[pSubIdx].scan_cfg &= ~(pScanCfgOpt); // Disable
	}
	else
	{
		// Toggle it to the opposite of how it's currently set
		if (Boolean(msg_area.grp_list[pGrpIdx].sub_list[pSubIdx].scan_cfg & pScanCfgOpt))
			msg_area.grp_list[pGrpIdx].sub_list[pSubIdx].scan_cfg &= ~(pScanCfgOpt); // Disable
		else
			msg_area.grp_list[pGrpIdx].sub_list[pSubIdx].scan_cfg ^= pScanCfgOpt; // Enable
	}
}


// Draws a border with configured colors.
//
// Parameters:
//  pULX: The upper-left X coordinate
//  pULY: The upper-left Y coordinate
//  pWidth: The width of the border area
//  pHeight: Theight of the border area
//  pColors: An object containing the color configuration
function drawColoredBorder(pULX, pULY, pWidth, pHeight, pColors)
{
	if (typeof(pULX) !== "number" || typeof(pULY) !== "number" || typeof(pWidth) !== "number" || typeof(pHeight) !== "number")
		return;

	var colorCorner1 = "\x01n";
	var colorCorner2 = "\x01n";
	var colorCorner3 = "\x01n";
	var colorBorderNormal = "\x01n";
	if (typeof(pColors) === "object")
	{
		if (pColors.hasOwnProperty("msgGrpBorderColorCorner1"))
			colorCorner1 += pColors.msgGrpBorderColorCorner1;
		if (pColors.hasOwnProperty("msgGrpBorderColorCorner2"))
			colorCorner2 += pColors.msgGrpBorderColorCorner2;
		if (pColors.hasOwnProperty("msgGrpBorderColorCorner3"))
			colorCorner3 += pColors.msgGrpBorderColorCorner3;
		if (pColors.hasOwnProperty("msgGrpBorderColorNormal"))
			colorBorderNormal += pColors.msgGrpBorderColorNormal;
	}


	if (useLightbarInterface(gConfig))
		console.gotoxy(pULX, pULY);
	// Top border characters
	console.print(colorCorner1 + UPPER_LEFT_SINGLE + colorCorner2 + HORIZONTAL_SINGLE + colorCorner3 + HORIZONTAL_SINGLE);
	console.print(colorBorderNormal + charStr(HORIZONTAL_SINGLE, pWidth - 6));
	console.print(colorCorner3 + HORIZONTAL_SINGLE + colorCorner2 + HORIZONTAL_SINGLE + colorCorner1 + UPPER_RIGHT_SINGLE);
	// Side border characters
	if (useLightbarInterface(gConfig))
	{
		//var innerHeight = pHeight - 2;
		var rightmostCol = pULX + pWidth - 1;
		var startRow = pULY + 1;
		var nextRowAfterStart = startRow + 1;
		var onePastLastRow = pULY + pHeight - 1;
		var secondToLastRow = onePastLastRow - 2;
		var lastRow = onePastLastRow - 1;
		for (var screenRow = startRow; screenRow < onePastLastRow; ++screenRow)
		{
			var attrs = "";
			if (screenRow == startRow || screenRow == lastRow)
				attrs = colorCorner2;
			else if (screenRow == nextRowAfterStart || screenRow == secondToLastRow)
				attrs = colorCorner3;
			else
				attrs = colorBorderNormal;
			console.print(attrs);
			console.gotoxy(pULX, screenRow);
			console.print(VERTICAL_SINGLE);
			console.gotoxy(rightmostCol, screenRow);
			console.print(VERTICAL_SINGLE);
		}
		console.gotoxy(pULX, onePastLastRow);
	}
	else
	{
		// TODO
	}
	// Bottom border characters
	console.print(colorCorner1 + LOWER_LEFT_SINGLE + colorCorner2 + HORIZONTAL_SINGLE + colorCorner3 + HORIZONTAL_SINGLE);
	console.print(colorBorderNormal + charStr(HORIZONTAL_SINGLE, pWidth - 6));
	console.print(colorCorner3 + HORIZONTAL_SINGLE + colorCorner2 + HORIZONTAL_SINGLE + colorCorner1 + LOWER_RIGHT_SINGLE);
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

// Reads the configuration file
//
// Parameters:
//  pConfigFilenameWithoutLeadingPath: The configuration filename, without leading path
function readConfigFile(pConfigFilenameWithoutLeadingPath)
{
	var cfgObj = {
		useLightbarInterface: true,
		colors: {
			// Message group menu & related colors
			msgGrpHdr: "\x01y\x01h",
			msgGrpBorderColorCorner1: "\x01w\x01h",
			msgGrpBorderColorCorner2: "\x01c\x01h",
			msgGrpBorderColorCorner3: "\x01c",
			msgGrpBorderColorNormal: "\x01k\x01h",
			msgGrpMenuNumColor: "\x01w\x01h",
			msgGrpMenuDescColor: "\x01c",
			msgGrpMenuNumItemsColor: "\x01b\x01h",
			msgGrpMenuNumHighlightColor: "\x014\x01w\x01h",
			msgGrpMenuDescHighlightColor: "\x014\x01c\x01h",
			msgGrpMenuNumItemsHighlightColor: "\x014\x01w\x01h",
			// Lightbar help line colors
			lightbarHelpLineBkg: "\x017",
			lightbarHelpLineGeneral: "\x01b",
			lightbarHelpLineHotkey: "\x01r",
			lightbarHelpLineParen: "\x01m",
			// Sub-board menu
			subBoardMenuNumColor: "\x01w\x01h",
			// Number input prompt for the lightbar interface
			lightbarNumInputPromptText: "\x01c",
			lightbarNumInputPromptColon: "\x01g\x01h",
			lightbarNumInputPromptUserInput: "\x01c\x01h",
			// Number input prompt for the traditional/non-lightbar interface
			traditionalNumInputPromptText: "\x01w",
			traditionalNumInputPromptColon: "\x01g\x01h",
			traditionalNumInputPromptUserInput: "\x01c\x01h",
			// Sub-board list & toggle menu colors
			subBoardMenuItemColor: "\x01c",
			subBoardMenuSelectedItemColor: "\x01c\x01h",
			toggleMenuItemColor: "\x01c",
			toggleMenuSelectedItemColor: "\x01w\x01h\x014",
			// Help text
			helpText: "\x01c",
			// Error messages
			errorMsg: "\x01h\x01r"
		}
	};

	// Open the main configuration file.  First look for it in the sbbs/mods
	// directory, then sbbs/ctrl, then in the same directory as this script.
	var cfgFilename = file_cfgname(system.mods_dir, pConfigFilenameWithoutLeadingPath);
	if (!file_exists(cfgFilename))
		cfgFilename = file_cfgname(system.ctrl_dir, pConfigFilenameWithoutLeadingPath);
	if (!file_exists(cfgFilename))
		cfgFilename = file_cfgname(js.exec_dir, pConfigFilenameWithoutLeadingPath);
	// If the configuration file hasn't been found, look to see if there's a .example.ini file
	// available in the same directory 
	if (!file_exists(cfgFilename))
	{
		var defaultExampleCfgFilename = pConfigFilenameWithoutLeadingPath;
		var dotIdx = defaultExampleCfgFilename.lastIndexOf(".");
		if (dotIdx > -1)
		{
			defaultExampleCfgFilename = defaultExampleCfgFilename.substring(0, dotIdx) + ".example"
			                          + defaultExampleCfgFilename.substring(dotIdx);
			var exampleFileName = file_cfgname(js.exec_dir, defaultExampleCfgFilename);
			if (file_exists(exampleFileName))
				cfgFilename = exampleFileName;
		}
	}

	// Read the configuration file
	var themeFilename = "";
	var cfgFile = new File(cfgFilename);
	if (cfgFile.open("r"))
	{
		var behaviorSettings = cfgFile.iniGetObject("BEHAVIOR");
		var colorSettings = cfgFile.iniGetObject("COLORS");
		cfgFile.close();

		// Behavior
		if (typeof(behaviorSettings.interfaceStyle) === "string")
		{
			var valUpper = behaviorSettings.interfaceStyle.toUpperCase();
			cfgObj.useLightbarInterface = (valUpper == "LIGHTBAR");
		}

		// Color settings
		if (typeof(colorSettings.themeFilename) === "string" && colorSettings.themeFilename.length > 0)
		{
			var tmpFilename = file_cfgname(system.mods_dir, colorSettings.themeFilename);
			if (!file_exists(tmpFilename))
				tmpFilename = file_cfgname(system.ctrl_dir, colorSettings.themeFilename);
			if (!file_exists(tmpFilename))
				tmpFilename = file_cfgname(js.exec_dir, colorSettings.themeFilename);
			if (file_exists(tmpFilename))
				themeFilename = tmpFilename;
		}
	}

	// If we got a theme filename, then read the colors from that file
	if (themeFilename.length > 0)
	{
		var themeFile = new File(themeFilename);
		if (themeFile.open("r"))
		{
			var colorSettings = themeFile.iniGetObject("COLORS");
			themeFile.close();

			for (var prop in cfgObj.colors)
			{
				if (colorSettings.hasOwnProperty(prop))
					cfgObj.colors[prop] = attrCodeStr(strip_ctrl(colorSettings[prop]));
			}
		}
	}

	return cfgObj;
}

// Returns whether the lightbar interface is to be used (the configuration option is
// enabled and the user's terminal supports ANSI)
function useLightbarInterface(pCfgObj)
{
	var cfgUseLightbarInterface = false;
	if (typeof(pCfgObj) === "object" && typeof(pCfgObj.useLightbarInterface) === "boolean")
		cfgUseLightbarInterface = pCfgObj.useLightbarInterface;
	else
		cfgUseLightbarInterface = gConfig.useLightbarInterface;
	return (cfgUseLightbarInterface && console.term_supports(USER_ANSI));
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

// Returns a string to be shown in the toggle menus or sub-board list for
// whether a scan option is enabled or not
function getToggleStr(pEnabled)
{
	return format(" [%s]  ", pEnabled ? CHECK_CHAR : " ");
}

// Returns some text centered in a field width.
//
// Parameters:
//  pText: The text to be centered
//  pFieldWidth: The field width to center the text in. Defaults the console width.
//  pAppendPostSpacing: Whether or not to also append spacing at the end. Defaults to false.
function getCenteredText(pText, pFieldWidth, pAppendPostSpacing)
{
	var fieldWidth = (typeof(pFieldWidth) === "number" && pFieldWidth > 0 ? pFieldWidth : console.screen_columns);
	var appendPostSpacing = (typeof(pAppendPostSpacing) === "boolean" ? pAppendPostSpacing : false);

	var textLength = console.strlen(pText);
	var frontPaddingLen = 0;
	frontPaddingLen = Math.floor(fieldWidth/2) - Math.floor(textLength/2);
	var theText = format("%*s", frontPaddingLen, "") + pText;
	if (appendPostSpacing)
	{
		var remainingLen = fieldLen - console.strlen(theText);
		if (remainingLen > 0)
			theText += format("%*s", remainingLen, "");
	}

	return theText;
}

// Returns some text with a solid horizontal line on the next line.
//
// Parameters:
//  pText: The text to display
//  pCenter: Whether or not to center the text.  Optional; defaults
//           to false.
//  pFieldWidth: If centering, this is the field width to center the text in
//  pTextColor: The color to use for the text.  Optional; by default,
//              normal white will be used.
//  pLineColor: The color to use for the line underneath the text.
//              Optional; by default, bright black will be used.
function getTextWithLineBelow(pText, pCenter, pFieldWidth, pTextColor, pLineColor)
{
	var centerText = (typeof(pCenter) === "boolean" ? pCenter : false);
	var fieldWidth = (typeof(pFieldWidth) === "number" && pFieldWidth > 0 ? pFieldWidth : console.screen_columns);
	var textColor = (typeof(pTextColor) === "string" ? pTextColor : "\x01n\x01w");
	var lineColor = (typeof(pLineColor) === "string" ? pLineColor : "\x01n\x01k\x01h");

	var textLength = console.strlen(pText);

	// Create the text and a solid line on the next line
	var theText = "";
	if (centerText)
	{
		var frontPaddingLen = 0;
		if (fieldWidth > 0)
			frontPaddingLen = Math.floor(fieldWidth/2) - Math.floor(textLength/2);
		else
			frontPaddingLen = Math.floor(console.screen_columns/2) - Math.floor(textLength/2);
		var frontPadding = format("%*s", frontPaddingLen, "");
		theText += frontPadding + textColor + pText + "\x01n";
		theText += "\r\n";
		theText += frontPadding + lineColor;
		for (var i = 0; i < textLength; ++i)
			theText += HORIZONTAL_SINGLE;
		theText += "\x01n";
	}
	else
	{
		theText = textColor + pText + "\x01n\r\n";
		theText += lineColor;
		for (var i = 0; i < textLength; ++i)
			theText += HORIZONTAL_SINGLE;
		theText += "\x01n";
	}
	theText += "\r\n";
	return theText;
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
	console.print(getTextWithLineBelow(pText, pCenter, console.screen_columns, pTextColor, pLineColor));
}

// Gets & returns centered program version information.
//
// Parameters:
//  pFieldWidth: The width to center the text in
function getProgramInfoText(pFieldWidth)
{
	var versionText = "\x01n\x01cVersion \x01g" + SCAN_CFG_VERSION + " \x01w\x01h(\x01b" + SCAN_CFG_DATE + "\x01w)";
	var progInfoText = getTextWithLineBelow("Digital Distortion Scan Configurator", true, pFieldWidth, "\x01n\x01c\x01h", "\x01k\x01h");
	progInfoText += getCenteredText(versionText, pFieldWidth, false);
	progInfoText += "\x01n";
	return progInfoText;
}

// Displays the program information.
function displayProgramInfo()
{
	console.print(getProgramInfoText(console.screen_columns));
	console.crlf();
}

// Displays help for choosing a message group
function displayChooseMsgGroupHelp()
{
	var helpText = getProgramInfoText(console.screen_columns);
	helpText += "\r\n";
	helpText += "\r\n";
	helpText += "\x01n" + gConfig.colors.helpText;
	helpText += "On this screen, you can choose a message group. ";
	if (useLightbarInterface(gConfig))
	{
		helpText += "You can navigate the list with the up & down arrow keys, PageUp, PageDown, ";
		helpText += "HOME, and END keys. Also, you can press F to go to the first page or L to ";
		helpText += "go go the last page. You can press the Enter key to select a message group. ";
		helpText += "Alternately, you can type the number of the message group to select it. ";
	}
	else
	{
		helpText += "You can type the number of the message group to select it. ";
	}
	helpText += "To quit, you can press Q or ESC.\r\n";
	helpText += "\r\n";
	//helpText += "Hotkeys\r\n";
	helpText += "\x01n\x01w\x01hHotkeys:\x01n\r\n";
	var formatStr = "\x01n\x01c\x01h%-17s\x01g: \x01n\x01c%s\r\n";
	if (useLightbarInterface(gConfig))
	{
		helpText += format(formatStr, "Up arrow", "Move the cursor up");
		helpText += format(formatStr, "Down arrow", "Move the cursor down");
		helpText += format(formatStr, "PageUp", "Move up one page");
		helpText += format(formatStr, "PageDown", "Move down one page");
		helpText += format(formatStr, "Home", "Go to the top of the list");
		helpText += format(formatStr, "End", "Go to the end of the list");
		helpText += format(formatStr, "/", "Search for message group");
		helpText += format(formatStr, "N", "Next search result");
	}
	helpText += format(formatStr, "Q or ESC", "Quit");
	helpText += format(formatStr, "?", "Display help");
	console.print(lfexpand(word_wrap(helpText, console.screen_columns-1, false)));

	console.attributes = "N";
	console.pause();
	if (console.aborted)
		console.aborted = false; // To prevent issues that may arise from console.aborted being true
}

// Displays help for doing the scan configuration for a message group
function displayScanConfigHelp(pMsgGrpName)
{
	var helpText = getProgramInfoText(console.screen_columns);
	helpText += "\r\n";
	helpText += "\r\n";
	helpText += "\x01n" + gConfig.colors.helpText;
	helpText += "On this screen, you can configure your scan preferences for the sub-boards in ";
	helpText += "a message group. The current message group is " + pMsgGrpName + ". ";
	if (useLightbarInterface(gConfig))
	{
		helpText += "You can navigate the list with the up & down arrow keys, PageUp, PageDown, ";
		helpText += "HOME, and END keys. You can move between the Unread and To-you ";
		helpText += "columns with the left & right arrow keys, and you can toggle the ";
		helpText += "Unread and To-you option with the Enter key. ";
	}
	else
	{
	}
	helpText += "To quit, you can press Q or ESC.\r\n";
	helpText += "\r\n";
	//helpText += "Hotkeys\r\n";
	helpText += "\x01n\x01w\x01hHotkeys:\x01n\r\n";
	var formatStr = "\x01n\x01c\x01h%-17s\x01g: \x01n\x01c%s\r\n";
	if (useLightbarInterface(gConfig))
	{
		helpText += format(formatStr, "Up arrow", "Move the cursor up");
		helpText += format(formatStr, "Down arrow", "Move the cursor down");
		helpText += format(formatStr, "PageUp", "Move up one page");
		helpText += format(formatStr, "PageDown", "Move down one page");
		helpText += format(formatStr, "Home", "Go to the top of the list");
		helpText += format(formatStr, "End", "Go to the end of the list");
		helpText += format(formatStr, "/", "Search for sub-board");
		helpText += format(formatStr, "N", "Next search result");
	}
	helpText += format(formatStr, "Q or ESC", "Quit");
	helpText += format(formatStr, "?", "Display help");
	console.print(lfexpand(word_wrap(helpText, console.screen_columns-1, false)));

	console.attributes = "N";
	console.pause();
	if (console.aborted)
		console.aborted = false; // To prevent issues that may arise from console.aborted being true
}

// Displays an error message at the bottom row of the screen, pauses a moment, and then
// erases the error message.
//
// Parameters:
//  pConfig: The configuration object (which contains colors)
//  pErrorMsg: The error message to display
//  pClearRowAfterward: Optional boolean - Whether to clear the row afterward.  Defaults to true.
function displayErrorMsgAtBottomScreenRow(pConfig, pErrorMsg, pClearRowAfterward)
{
	console.gotoxy(1, console.screen_rows);
	console.cleartoeol("\x01n");
	console.gotoxy(1, console.screen_rows);
	console.print(pConfig.colors.errorMsg);
	console.print(pErrorMsg);
	mswait(ERROR_PAUSE_WAIT_MS);
	var clearRowAfterward = (typeof(pClearRowAfterward) === "boolean" ? pClearRowAfterward : true);
	if (clearRowAfterward)
	{
		console.gotoxy(1, console.screen_rows);
		console.cleartoeol("\x01n");
		console.gotoxy(1, console.screen_rows);
	}
	else
		console.attributes = "N";
}
