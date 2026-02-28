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
 * 2020-11-01 Eric Oulashin   Version 1.21 Beta
 *                            Working on sub-board collapsing
 * 2022-01-15 Eric Oulashin   Version 1.21
 *                            Finished sub-board collapsing (finally) and releasing
 *                            this version.
 * 2022-02-12 Eric Oulashin   Version 1.22
 *                            Updated the version to match the file area chooser
 * 2022-03-18 Eric Oulashin   Version 1.23
 *                            For sub-board collapsing, if there's only one sub-group,
 *                            then it won't be collapsed.
 *                            Also fixed an issue: Using Q to quit out of the 2nd level
 *                            (sub-board/sub-group) for lightbar mode no longer quits
 *                            out of the chooser altogether.
 * 2022-05-17 Eric Oulashin   Version 1.24
 *                            Fix for search error reporting (probably due to
 *                            mistaken copy & paste in an earlier commit)
 * 2022-06-06 Eric Oulashin   Version 1.25
 *                            Fix for miscolored digit(s) in # messages column in
 *                            the sub-board list when using the lightbar menu
 * 2022-06-11 Eric Oulashin   Version 1.26
 *                            Updated to try to prevent the error "this.subBoardListPrintfInfo[pGrpIdx] is undefined"
 *                            when only choosing a sub-board within the user's current message group.
 * 2022-07-23 Eric Oulashin   Version 1.29
 *                            Re-arranged the help text for lightbar mode to be more consistent with my message reader.
 * 2022-08-19 Eric Oulashin   Version 1.30
 *                            Set the control key pass-thru so that some hotkeys (such as Ctrl-P for PageUp) only
 *                            get caught by this script.
 * 2022-11-04 Eric Oulashin   Version 1.31
 *                            Made use of the 'posts' property in msg_area.sub[sub-code] (or msg_area.grp_list.sub_list)
 *                            for the number of posts without votes
 * 2022-11-07 Eric Oulashin   Version 1.32
 *                            Bug fix for numeric input when choosing a sub-board.  Bug fix for getting the number of
 *                            posts with the traditional user interface.
 * 2023-03-19 Eric Oulashin   Version 1.33
 *                            Updated wording for inputting the library/dir # in lightbar mode
 * 2023-04-15 Eric Oulashin   Version 1.34
 *                            Fix: For lightbar mode with sub-board collapsing, now sets the selected item based
 *                            on the user's current sub-board. Also, color settings no longer need the control
 *                            character (they can just be a list of the attribute characters).
 * 2023-05-14 Eric Oulashin   Version 1.35
 *                            Refactored the code for reading the configuration file
 * 2023-07-16 Eric Oulashin   Version 1.36
 *                            Possible fix for not allowing to change sub-board if the first group is empty
 * 2023-09-11 Eric Oulashin   Version 1.37 Beta
 *                            Area change header display bug fix
 * 2023-09-16 Eric Oulashin   Version 1.37
 *                            Releasing this version
 * 2023-10-07 Eric Oulashin   Version 1.38
 *                            Fix for name collapsing mode with the lightbar interface: No longer gets stuck
 *                            in a loop when choosing a sub-board.
 * 2023-10-17 Eric Oulashin   Version 1.39
 *                            Faster searching for the latest readable message header
 *                            (for getting the latest message timesamp), using
 *                            get_msg_index() rather than get_msg_header().
 * 2023-10-24 Eric Oulashin   Version 1.40
 *                            Sub-board name collapsing: Fix for incorrect sub-subboard assignment.
 *                            Also, won't collapse if the name before the separator is the same as
 *                            the message group description.
 * 2023-10-27 Eric Oulashin   Version 1.41
 *                            Lightbar mode: When using name collapisng, ensure the menu item for
 *                            the appropriate subgroup is selected.
 * 2024-11-12 Eric Oulashin   Version 1.42 Beta
 *                            Started working on a change to sub-board collapsing to allow an
 *                            arbitrary amount of separators in the group/sub names to
 *                            create multiple levels of categories
 * 2025-03-17 Eric Oulashin   Version 1.42
 *                            Releasing this version
 * 2025-04-04 Eric Oulashin   Version 1.42a
 *                            Fix for 'undefined' error (using the wrong object) when typing a sub-board
 *                            number to choose it. Reported by Keyop.
 * 2025-04-10 Eric Oulashin   Version 1.42b
 *                            Fix: altName wasn't added to items if name collapsing disabled.
 *                            Also, start of name collapsing enhancement (no empty names).
 * 2025-04-21 Eric Oulashin   Version 1.42c
 *                            F & L keys working again on the light bar menu (first & last page).
 *                            Fix for "this.DisplayMenuHdrWithNumItems is not a function".
 * 2025-05-27 Eric Oulashin   Version 1.42e
 *                            Fix: Name collapsing for group names w/ more than 1 instance
 * 2025-05-28 Eric Oulashin   Version 1.42f
 *                            Name collapsing now works at the top level. Also,
 *                            support for a double separator to not collapse (and
 *                            display just one of those characters).
 * 2025-06-01 Eric Oulashin   Version 1.43
 *                            If DDMsgAreaChooser.cfg doesn't exist, read DDMsgAreaChooser.example.cfg
 *                            (in the same directory as DDMsgAreaChooser.js) if it exists.
 *                            Also did an internal refactor, moving some common functionality
 *                            out into DDAreaChooserCommon.js to make development a bit simpler.
 * 2025-06-04 Eric Oulashin   Version 1.44
 *                            Fix: Item colors for sub-boards properly aligned again
 * 2025-06-18 Eric Oulashin   Version 1.45
 *                            Added sorting with a user config option
 * 2025-08-26 Eric Oulashin   Version 1.46
 *                            Replaced arrow keys in the key help line since some terminals
 *                            can't display them.
 * 2025-10-03 Eric Oulashin   Version 1.47
 *                            Mouse click hotspots in the key help lines. Some of the
 *                            click hotspots have an issue that causes the chooser to
 *                            exit out though - the full sequence for the key for those
 *                            clicks (ESC + ...) might not be captured.
 * 2025-12-21 Eric Oulashin   Version 1.48 Beta
 *                            Started working on having the area chooser save the user's
 *                            last chosen sub-board for each message group, for when the
 *                            user switches between groups (toggaleable w/ a user setting)
 * 2025-12-31 Eric Oulashin   Version 1.48
 *                            Releasing this version
 * 2026-02-27 Eric Oulashin   Version 1.49
 *                            Fix: When a sub-board has 10,000+ messages, the description
 *                            column color no longer extends into the # messages column.
 *                            Use dynamic subBoardDescLen based on numMsgsLen for color regions.
 *                            Fix: # items column now right-aligns correctly.  numItemsLen and
 *                            numMsgsLen are now dynamic (consider sub-group counts).
 */

/* Command-line arguments:
   1 (argv[0]): Boolean - Whether or not to choose a message group first (default).  If
                false, the user will only be able to choose a different sub-board within
				their current message group.
   2 (argv[1]): Boolean - Whether or not to run the area chooser (if false,
                then this file will just provide the DDMsgAreaChooser class).
*/

if (typeof(require) === "function")
{
	require("sbbsdefs.js", "K_NOCRLF");
	require("dd_lightbar_menu.js", "DDLightbarMenu");
	require("DDAreaChooserCommon.js", "getAreaHeirarchy");
	require("choice_scroll_box.js", "ChoiceScrollbox");
}
else
{
	load("sbbsdefs.js");
	load("dd_lightbar_menu.js");
	load("DDAreaChooserCommon.js");
	load("choice_scroll_box.js");
}

// This script requires Synchronet version 3.14 or higher.
// Exit if the Synchronet version is below the minimum.
if (system.version_num < 31400)
{
	var message = "\x01n\x01h\x01y\x01i* Warning:\x01n\x01h\x01w Digital Distortion Message Area Chooser "
	             + "requires version \x01g3.14\x01w or\r\n"
	             + "higher of Synchronet.  This BBS is using version \x01g" + system.version
	             + "\x01w.  Please notify the sysop.";
	console.crlf();
	console.print(message);
	console.crlf();
	console.pause();
	exit();
}

// Version & date variables
var DD_MSG_AREA_CHOOSER_VERSION = "1.49";
var DD_MSG_AREA_CHOOSER_VER_DATE = "2026-02-27";

// Keyboard input key codes
var CTRL_H = "\x08";
var CTRL_M = "\x0d";
var KEY_ENTER = CTRL_M;
var BACKSPACE = CTRL_H;
var CTRL_F = "\x06";
var KEY_ESC = ascii(27);

// Characters for display
var UP_ARROW = ascii(24);
var DOWN_ARROW = ascii(25);
var BLOCK1 = "\xB0"; // Dimmest block
var BLOCK2 = "\xB1";
var BLOCK3 = "\xB2";
var BLOCK4 = "\xDB"; // Brightest block
var MID_BLOCK = ascii(254);
var TALL_UPPER_MID_BLOCK = "\xFE";
var UPPER_CENTER_BLOCK = "\xDF";
var LOWER_CENTER_BLOCK = "\xDC";

// Characters for display
var HORIZONTAL_SINGLE = "\xC4";

// Sub-board sort options for changing to another sub-board.
// Note: These sort option values must match the values for how they're
// defined in DDMsgReader.
const SUB_BOARD_SORT_NONE = 0;
const SUB_BOARD_SORT_ALPHABETICAL = 1;
const SUB_BOARD_SORT_LATEST_MSG_DATE_OLDEST_FIRST = 2;
const SUB_BOARD_SORT_LATEST_MSG_DATE_NEWEST_FIRST = 3;

// Misc. defines
var ERROR_WAIT_MS = 1500;
var SEARCH_TIMEOUT_MS = 10000;


// User settings file name
var gUserSettingsFilename = system.data_dir + "user/" + format("%04d", user.number) + ".DDMsgAreaChooser_Settings";

// Parse command-line arguments
// 1st command-line argument: Whether or not to choose a message group first (if
// false, then only choose a sub-board within the user's current group).  This
// can be true or false.

// 2nd command-line argument: Determine whether or not to execute the message listing
// code (true/false)
var cmdLineParseRetobj = parseCmdLineArgs(argv);


// If executeThisScript is true, then create a DDMsgAreaChooser object and use
// it to let the user choose a message area.
if (cmdLineParseRetobj.executeThisScript)
{
	// Starting with the user's current messsage group, find the first message group with sub-boards.
	// If there are none, output an error message and exit.
	//var firstGrpIdxWithSubBoards = findNextGrpIdxWithSubBoards(0);
	var firstGrpIdxWithSubBoards = findNextGrpIdxWithSubBoards(-1);
	if (firstGrpIdxWithSubBoards < 0)
	{
		console.clear("\x01n");
		console.print("\x01y\x01hThere are no message sub-boards available.\r\n\x01p");
		exit(0);
	}

	// When exiting this script, make sure to set the ctrl key pasthru back to what it was originally
	js.on_exit("console.ctrlkey_passthru = " + console.ctrlkey_passthru);
	console.ctrlkey_passthru = "+ACGKLOPQRTUVWXYZ_"; // So that control key combinations only get caught by this script

	var msgAreaChooser = new DDMsgAreaChooser();
	// If we are to let the user choose a sub-board within
	// their current group (and not choose a message group
	// first), then we need to capture the chosen sub-board
	// here just in case, and change the user's message area
	// here.  Otherwise, if choosing the message group first,
	// SelectMsgArea() will change the user's sub-board.
	//var msgGroupIdx = (cmdLineParseRetobj.chooseMsgGrpOnStartup ? firstGrpIdxWithSubBoards/*null*/ : +bbs.curgrp);
	var msgGroupIdx = +bbs.curgrp; // Default to the user's current message group
	if (!cmdLineParseRetobj.chooseMsgGrpOnStartup)
		msgAreaChooser.BuildSubBoardPrintfInfoForGrp(msgGroupIdx);
	var chosenIdx = msgAreaChooser.SelectMsgArea(cmdLineParseRetobj.chooseMsgGrpOnStartup, msgGroupIdx);
	if (!cmdLineParseRetobj.chooseMsgGrpOnStartup && (typeof(chosenIdx) === "number"))
		bbs.cursub_code = msg_area.grp_list[bbs.curgrp].sub_list[chosenIdx].code;
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
		areaNum: "\x01n\x01w\x01h",
		desc: "\x01n\x01c",
		numItems: "\x01b\x01h",
		header: "\x01n\x01y\x01h",
		subBoardHeader: "\x01n\x01g",
		areaMark: "\x01g\x01h",
		latestDate: "\x01n\x01g",
		latestTime: "\x01n\x01m",
		// Highlighted colors (for lightbar mode)
		bkgHighlight: "\x01" + "4", // Blue background
		areaNumHighlight: "\x01w\x01h",
		descHighlight: "\x01c",
		dateHighlight: "\x01w\x01h",
		timeHighlight: "\x01w\x01h",
		numItemsHighlight: "\x01w\x01h",
		// Lightbar help line colors
		lightbarHelpLineBkg: "\x017",
		lightbarHelpLineGeneral: "\x01b",
		lightbarHelpLineHotkey: "\x01r",
		lightbarHelpLineParen: "\x01m"
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

	// Whether or not to enable sub-board collapsing.  For
	// example, for sub-board in a group starting with
	// common text and a separator (specified below), the
	// common text will be the only one displayed, and when
	// the user selects it, a 3rd tier with the sub-board
	// after the separator will be shown
	this.useSubCollapsing = true;
	// The separator character to use for sub-board collapsing
	this.subCollapseSeparator = ":";
	// If userSubCollapsing is true, then group_list will be populated
	// with some information from Synchronet's msg_area.grp_list,
	// including a dir_list for each group.  The dir_list arrays
	// could have one that was collapsed from multiple sub-board
	// set up in the BBS - The dir_list within that one would then
	// contain multiple sub-board split based on the dir collapse
	// separator.
	//this.msgArea_list = [];

	// User settings
	this.userSettings = {
		// Area change sorting for changing to another sub-board: None, Alphabetical, or LatestMsgDate
		areaChangeSorting: SUB_BOARD_SORT_NONE,
		// When changing to a different message group, whether to remember/use
		// the last sub-board in each message group as the currently selected
		// sub-board
		rememberLastSubBoardWhenChangingGrp: false
	};
	// The user's last chosen sub-boards for each message group. The key is
	// the group name and the value is the internal code for the user's last
	// chosen sub-board for that group.
	this.lastChosenSubBoardsPerGrpForUser = {};

	// Set the function pointers for the object
	this.ReadConfigFile = DDMsgAreaChooser_ReadConfigFile;
	this.ReadUserSettingsFile = DDMsgAreaChooser_ReadUserSettingsFile;
	this.WriteUserSettingsFile = DDMsgAreaChooser_WriteUserSettingsFile;
	this.WriteKeyHelpLine = DDMsgAreaChooser_WriteKeyHelpLine;
	this.WriteGrpListHdrLine = DDMsgAreaChooser_WriteGrpListHdrLine;
	this.WriteSubBrdListHdr1Line = DMsgAreaChooser_WriteSubBrdListHdr1Line;
	this.SelectMsgArea = DDMsgAreaChooser_SelectMsgArea;
	this.CreateLightbarMenu = DDMsgAreaChooser_CreateLightbarMenu;
	this.GetColorIndexInfoForLightbarMenu = DDMsgAreaChooser_GetColorIndexInfoForLightbarMenu;
	this.GetSubBoardColorIndexInfoAndFormatStrForMenuItem = DDMsgAreaChooser_GetSubBoardColorIndexInfoAndFormatStrForMenuItem;
	// Help screen
	this.ShowHelpScreen = DDMsgAreaChooser_ShowHelpScreen;
	// Function to build the sub-board printf information for a message
	// group
	this.BuildSubBoardPrintfInfoForGrp = DDMsgAreaChooser_BuildSubBoardPrintfInfoForGrp;
	this.getMaxItemsCountInMsgHierarchy = DDMsgAreaChooser_getMaxItemsCountInMsgHierarchy;
	this.DisplayAreaChgHdr = DDMsgAreaChooser_DisplayAreaChgHdr;
	this.DisplayListHdrLines = DDMsgAreaChooser_DisplayListHdrLines;
	this.WriteLightbarKeyHelpErrorMsg = DDMsgAreaChooser_WriteLightbarKeyHelpErrorMsg;
	this.SetUpGrpListWithCollapsedSubBoards = DDMsgAreaChooser_SetUpGrpListWithCollapsedSubBoards;
	this.FindMsgAreaIdxFromText = DDMsgAreaChooser_FindMsgAreaIdxFromText;
	this.GetSubNameLenAndNumMsgsLen = DDMsgAreaChooser_GetSubNameLenAndNumMsgsLen;
	this.DoUserSettings_Scrollable = DDMsgAreaChooser_DoUserSettings_Scrollable;
	this.DoUserSettings_Traditional = DDMsgAreaChooser_DoUserSettings_Traditional;

	// Read the settings from the config file.
	this.ReadConfigFile();
	// Read the user settings
	this.ReadUserSettingsFile();

	// Populate msgArea_list (should be done after reading the configuration file
	// so that useSubCollapsing and subCollapseSeparator are set according to settings
	this.msgArea_list = getAreaHeirarchy(DDAC_MSG_AREAS, this.useSubCollapsing, this.subCollapseSeparator);
	// Sort according to the user's configured sort option.
	sortHeirarchyRecursive(this.msgArea_list, this.userSettings.areaChangeSorting);

	// Make numItemsLen dynamic so the # column stays right-aligned when a group has 1000+ items
	var maxItemsInAnyGrp = 0;
	for (var i = 0; i < this.msgArea_list.length; ++i)
	{
		var grpMax = this.getMaxItemsCountInMsgHierarchy(this.msgArea_list[i]);
		if (grpMax > maxItemsInAnyGrp)
			maxItemsInAnyGrp = grpMax;
	}
	this.numItemsLen = Math.max(4, Math.max(1, maxItemsInAnyGrp.toString().length));
	
	// These variables store default lengths of the various columns displayed in
	// the message group/sub-board lists.
	// Sub-board info field lengths
	this.areaNumLen = 4;
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
	this.msgGrpListPrintfStr = "\x01n" + this.colors.areaNum + "%" + this.areaNumLen
	                         + "d " + this.colors.desc + "%-"
	                         + this.msgGrpDescLen + "s " + this.colors.numItems
	                         + "%" + this.numItemsLen + "d";
	this.msgGrpListPrintfStrWithoutAreaNum = "\x01n" + this.colors.desc + "%-"
	                         + this.msgGrpDescLen + "s " + this.colors.numItems
	                         + "%" + this.numItemsLen + "d";
	this.msgGrpListHilightPrintfStr = "\x01n" + this.colors.bkgHighlight + " "
	                                + "\x01n" + this.colors.bkgHighlight
	                                + this.colors.areaNumHighlight + "%" + this.areaNumLen
	                                + "d \x01n" + this.colors.bkgHighlight
	                                + this.colors.descHighlight + "%-"
	                                + this.msgGrpDescLen + "s \x01n" + this.colors.bkgHighlight
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
	this.lightbarKeyHelpText = "\x01n" + this.colors.lightbarHelpLineHotkey
	              + this.colors.lightbarHelpLineBkg + "@CLEAR_HOT@@`Up`" + KEY_UP + "@"
	              + "\x01n" + this.colors.lightbarHelpLineGeneral
	              + this.colors.lightbarHelpLineBkg + ", "
	              + "\x01n" + this.colors.lightbarHelpLineHotkey
	              + this.colors.lightbarHelpLineBkg + "@`Dn`\\n@"
	              + "\x01n" + this.colors.lightbarHelpLineGeneral
	              + this.colors.lightbarHelpLineBkg + ", "
	              + "\x01n" + this.colors.lightbarHelpLineHotkey
	              + this.colors.lightbarHelpLineBkg + "@`PgUp`" + "\x1b[V" + "@"
	              + "\x01n" + this.colors.lightbarHelpLineGeneral
	              + this.colors.lightbarHelpLineBkg + "/"
	              + "\x01n" + this.colors.lightbarHelpLineHotkey
	              + this.colors.lightbarHelpLineBkg + "@`Dn`" + KEY_PAGEDN + "@"
	              + "\x01n" + this.colors.lightbarHelpLineGeneral
	              + this.colors.lightbarHelpLineBkg + ", "
	              + "\x01n" + this.colors.lightbarHelpLineHotkey
	              + this.colors.lightbarHelpLineBkg + "@`HOME`" + KEY_HOME + "@"
	              + "\x01n" + this.colors.lightbarHelpLineGeneral
	              + this.colors.lightbarHelpLineBkg + ", "
	              + "\x01n" + this.colors.lightbarHelpLineHotkey
	              + this.colors.lightbarHelpLineBkg + "@`END`" + KEY_END + "@"
	              + "\x01n" + this.colors.lightbarHelpLineGeneral
	              + this.colors.lightbarHelpLineBkg + ", "
	              + "\x01n" + this.colors.lightbarHelpLineHotkey
	              + this.colors.lightbarHelpLineBkg + "@`F`F@"
	              + "\x01n" + this.colors.lightbarHelpLineParen
	              + this.colors.lightbarHelpLineBkg + ")"
	              + "\x01n" + this.colors.lightbarHelpLineGeneral
	              + this.colors.lightbarHelpLineBkg + "irst pg, "
	              + "\x01n" + this.colors.lightbarHelpLineHotkey
	              + this.colors.lightbarHelpLineBkg + "@`L`L@"
	              + "\x01n" + this.colors.lightbarHelpLineParen
	              + this.colors.lightbarHelpLineBkg + ")"
	              + "\x01n" + this.colors.lightbarHelpLineGeneral
	              + this.colors.lightbarHelpLineBkg + "ast pg, "
	              + "\x01n" + this.colors.lightbarHelpLineHotkey
	              + this.colors.lightbarHelpLineBkg + "@`#`#@"
	              + "\x01n" + this.colors.lightbarHelpLineGeneral
	              + this.colors.lightbarHelpLineBkg + ", "
	              + "\x01n" + this.colors.lightbarHelpLineHotkey
	              + this.colors.lightbarHelpLineBkg + "@`CTRL-F`" + CTRL_F + "@"
	              + "\x01n" + this.colors.lightbarHelpLineGeneral
	              + this.colors.lightbarHelpLineBkg + ", "
	              + "\x01n" + this.colors.lightbarHelpLineHotkey
	              + this.colors.lightbarHelpLineBkg + "@`/`/@"
	              + "\x01n" + this.colors.lightbarHelpLineGeneral
	              + this.colors.lightbarHelpLineBkg + ", "
	              + "\x01n" + this.colors.lightbarHelpLineHotkey
	              + this.colors.lightbarHelpLineBkg + "@`N`N@"
	              + "\x01n" + this.colors.lightbarHelpLineGeneral
	              + this.colors.lightbarHelpLineBkg + ", "
	              + "\x01n" + this.colors.lightbarHelpLineHotkey
				  + this.colors.lightbarHelpLineBkg + "@`Q`Q@"
				  + "\x01n" + this.colors.lightbarHelpLineParen
				  + this.colors.lightbarHelpLineBkg + ")"
				  + "\x01n" + this.colors.lightbarHelpLineGeneral
				  + this.colors.lightbarHelpLineBkg + "uit, "
				  + "\x01n" + this.colors.lightbarHelpLineHotkey
				  + this.colors.lightbarHelpLineBkg + "@`?`?@";
	// Pad the lightbar key help text on either side to center it on the screen
	// (but leave off the last character to avoid screen drawing issues)
	//var helpTextLen = console.strlen(this.lightbarKeyHelpText);
	var helpTextLen = 74;
	var helpTextStartCol = (console.screen_columns/2) - (helpTextLen/2);
	this.lightbarKeyHelpText = "\x01n" + this.colors.lightbarHelpLineBkg
	                         + format("%" + +(helpTextStartCol) + "s", "")
							 + this.lightbarKeyHelpText + "\x01n"
							 + this.colors.lightbarHelpLineBkg;
	var numTrailingChars = console.screen_columns - (helpTextStartCol+helpTextLen) - 1;
	this.lightbarKeyHelpText += format("%" + +(numTrailingChars) + "s", "") + "\x01n";
	// this.subBoardListPrintfInfo will be an array of printf strings
	// for the sub-boards in the message groups.  The index is the
	// message group index.  The sub-board printf information is created
	// on the fly the first time the user lists sub-boards for a message
	// group.
	this.subBoardListPrintfInfo = [];

	// areaChangeHdrLines is an array of text lines to use as a header to display
	// above the message area changer lists.
	this.areaChangeHdrLines = loadTextFileIntoArray(this.areaChooserHdrFilenameBase, this.areaChooserHdrMaxLines);
}

// For the DDMsgAreaChooser class: Writes the line of key help at the bottom
// row of the screen.
function DDMsgAreaChooser_WriteKeyHelpLine()
{
	console.gotoxy(1, console.screen_rows);
	//console.print(this.lightbarKeyHelpText);
	console.putmsg(this.lightbarKeyHelpText);
}

// For the DDMsgAreaChooser class: Outputs the header line to appear above
// the list of message groups.
//
// Parameters:
//  pNumPages: The number of pages.  This is optional; if this is
//             not passed, then it won't be used.
//  pPageNum: The page number.  This is optional; if this is not passed,
//            then it won't be used.
function DDMsgAreaChooser_WriteGrpListHdrLine(pNumPages, pPageNum)
{
	var descStr = "Description";
	if ((typeof(pPageNum) === "number") && (typeof(pNumPages) === "number"))
		descStr += "    (Page " + pPageNum + " of " + pNumPages + ")";
	else if ((typeof(pPageNum) === "number") && (typeof(pNumPages) !== "number"))
		descStr += "    (Page " + pPageNum + ")";
	else if ((typeof(pPageNum) !== "number") && (typeof(pNumPages) === "number"))
		descStr += "    (" + pNumPages + (pNumPages == 1 ? " page)" : " pages)");
	printf(this.msgGrpListHdrPrintfStr, "Group#", descStr, "# Sub-Boards");
	console.cleartoeol("\x01n");
}

// For the DDMsgAreaChooser class: Outputs the first header line to appear
// above the sub-board list for a message group.
//
// Parameters:
//  pMsgAreaHeirarchyObj: An object from this.msgArea_list, which is
//                        set up with a 'name' property and either
//                        an 'items' property if it has sub-items
//                        or a 'subItemObj' property if it's a sub-board
//  pGrpIndex: The index of the message group (assumed to be valid)
//  pSubIndex: Optional - The index of a sub-board within the message group (if using sub-board
//             collapsing, we might want to append the sub-board name to the description)
//  pNumPages: The number of pages.  This is optional; if this is
//             not passed, then it won't be used.
//  pPageNum: The page number.  This is optional; if this is not passed,
//            then it won't be used.
function DMsgAreaChooser_WriteSubBrdListHdr1Line(pMsgAreaHeirarchyObj, pGrpIndex, pNumPages, pPageNum)
{
	var descLen = 25;
	var descFormatStr = "\x01n" + this.colors.subBoardHeader + "Sub-boards of \x01h%-" + descLen + "s     \x01n"
	                  + this.colors.subBoardHeader;
	if ((typeof(pPageNum) === "number") && (typeof(pNumPages) === "number"))
		descFormatStr += "(Page " + pPageNum + " of " + pNumPages + ")";
	else if ((typeof(pPageNum) === "number") && (typeof(pNumPages) !== "number"))
		descFormatStr += "(Page " + pPageNum + ")";
	else if ((typeof(pPageNum) !== "number") && (typeof(pNumPages) === "number"))
		descFormatStr += "(" + pNumPages + (pNumPages == 1 ? " page)" : " pages)");

	// Ensure this.msgArea_list is set up
	this.SetUpGrpListWithCollapsedSubBoards();

	var desc = "";
	if (Array.isArray(pMsgAreaHeirarchyObj) && pGrpIndex >= 0 && pGrpIndex < pMsgAreaHeirarchyObj.length)
	{
		if (pMsgAreaHeirarchyObj[pGrpIndex].hasOwnProperty("name"))
			desc = pMsgAreaHeirarchyObj[pGrpIndex].name;
	}
	else if (pMsgAreaHeirarchyObj.hasOwnProperty("items") && pGrpIndex >= 0 && pGrpIndex < pMsgAreaHeirarchyObj.items.length)
	{
		if (pMsgAreaHeirarchyObj.items[pGrpIndex].hasOwnProperty("name"))
			desc = pMsgAreaHeirarchyObj.items[pGrpIndex].name;
	}
	printf(descFormatStr, desc.substr(0, descLen));
	console.cleartoeol("\x01n");
}

// For the DDMsgAreaChooser class: Lets the user choose a message group and
// sub-board via numeric input, using a lightbar interface (if enabled and
// if the user's terminal uses ANSI) or a traditional user interface.
//
// Parameters:
//  pChooseGroup: Boolean - Whether or not to choose the message group.  If false,
//                then this will allow choosing a sub-board within the user's
//                current message group.  This is optional; defaults to true.
//  pGrpIdx: The group index (can be null) - This is for the lightbar
//           interface logic; the traditional interface doesn't use this
//           for SelectMsgArea().
function DDMsgAreaChooser_SelectMsgArea(pChooseGroup, pGrpIdx)
{
	var chooseGrp = (typeof(pChooseGroup) === "boolean" ? pChooseGroup : true);

	// Start with this.lib_list, which is the topmost file lib/dir structure
	var msgAreaStructure = this.msgArea_list;
	var msgAreaStructureWithItems = this.msgArea_list;
	var selectedGrpIdx = null;
	var previousMsgAreaStructure = null;
	if (!chooseGrp)
	{
		if (typeof(pGrpIdx) === "number")
		{
			if (pGrpIdx >= 0 && pGrpIdx < this.msgArea_list.length)
			{
				if (this.msgArea_list[pGrpIdx].hasOwnProperty("items"))
				{
					msgAreaStructure = this.msgArea_list[pGrpIdx].items;
					msgAreaStructureWithItems = this.msgArea_list[pGrpIdx];
					selectedGrpIdx = pGrpIdx;
					previousMsgAreaStructure = this.msgArea_list;
				}
			}
		}
		else
		{
			//var lastChosenSubsObj = this.userSettings.rememberLastSubBoardWhenChangingGrp ? this.lastChosenSubBoardsPerGrpForUser : null;
			var lastChosenSubsObj = null;
			/*
			var lastChosenSubsObj = null;
			if (pHeirarchyLevel > 1 && this.userSettings.rememberLastSubBoardWhenChangingGrp)
				lastChosenSubsObj = this.lastChosenSubBoardsPerGrpForUser;
			*/
			for (var i = 0; i < this.msgArea_list.length; ++i)
			{
				if (msgAreaStructureHasCurrentUserSubBoard(this.msgArea_list[i], null, lastChosenSubsObj))
				{
					if (this.msgArea_list[i].hasOwnProperty("items"))
					{
						msgAreaStructure = this.msgArea_list[i].items;
						msgAreaStructureWithItems = this.msgArea_list[i];
						selectedGrpIdx = i;
						previousMsgAreaStructure = this.msgArea_list;
					}
					break;
				}
			}
		}
	}
	var selectedItemIndexes = [];       // Will be used like a stack
	var selectedItemIdx = null;
	var chosenGroupOrSubBoardName = ""; // Will have the name of the user's chosen library/subdir
	var previousMsgAreaStructures = []; // Will be used like a stack
	var previousMsgAreaStructuresWithItems = [];
	var previousChosenLibOrSubdirNames = []; // Will be used like a stack
	var subBoardsLabelLen = 14; // The length of the "Sub-boards of " label
	var nameSep = " - ";          // A string to use to separate group/sub-board names for the top header line
	var numItemsWidth = 0;        // Width of the header column for # of items (not including the space), for creating the menu
	var sortingChanged = false;   // Will be set true when the user changes sorting so that we can update appropriately
	// Main loop
	var selectionLoopContinueOn = true;
	while (selectionLoopContinueOn)
	{
		console.clear("\x01n");

		// If we're displaying the file libraries (top level), then we'll output 1
		// header line; otherwise, we'll output 2 header line; adjut the top line
		// of the menu accordingly.
		var menuTopRow = 0;
		var choosingGroup = chooseGrp && (msgAreaStructure == this.msgArea_list || chosenGroupOrSubBoardName.length == 0);
		//if (msgAreaStructure == this.msgArea_list || chosenGroupOrSubBoardName.length == 0)
		if (choosingGroup)
		{
			menuTopRow = this.areaChangeHdrLines.length + 2;
			numItemsWidth = 6; // "Group#"
		}
		else
		{
			menuTopRow = this.areaChangeHdrLines.length + 3;
			numItemsWidth = 6; // " Sub #"
			// TODO: Remove?
			/*
			printf("\x01n%sSub-boards of \x01h%s\x01n", this.colors.header, chosenGroupOrSubBoardName.substr(0, console.screen_columns - subBoardsLabelLen - 1));
			console.crlf();
			*/
		}
		//var previousMsgAreaStructure = (previousMsgAreaStructures.length > 0 ? previousMsgAreaStructures[previousMsgAreaStructures.length-1] : null);
		if (chooseGrp)
			previousMsgAreaStructure = (previousMsgAreaStructures.length > 0 ? previousMsgAreaStructures[previousMsgAreaStructures.length-1] : null);
		else if (previousMsgAreaStructures.length > 0)
			previousMsgAreaStructure = previousMsgAreaStructures[previousMsgAreaStructures.length-1];
		var grpName = "";
		if (typeof(selectedGrpIdx) === "number" && selectedGrpIdx >= 0 && selectedGrpIdx < this.msgArea_list.length)
		{
			if (this.msgArea_list[selectedGrpIdx].hasOwnProperty("name"))
				grpName = this.msgArea_list[selectedGrpIdx].name;
		}
		var createMenuRet = this.CreateLightbarMenu(grpName, msgAreaStructure, previousMsgAreaStructures.length+1, menuTopRow, selectedItemIdx, numItemsWidth);
		// If sorting has changed, ensure the menu's selected item is the user's
		// current sub-board
		if (sortingChanged)
		{
			var lastChosenSubsObj = this.userSettings.rememberLastSubBoardWhenChangingGrp ? this.lastChosenSubBoardsPerGrpForUser : null;
			setMenuIdxWithSelectedSubBoard(createMenuRet.menuObj, msgAreaStructure, null, lastChosenSubsObj);
			// If we're back at the root, set sortingChanged back to false
			if (msgAreaStructure == this.msgArea_list)
				sortingChanged = false;
		}
		// createMenuRet.allSubs
		// createMenuRet.allOnlyOtherItems
		var menu = createMenuRet.menuObj;
		// Write the header lines, & write the key help line at the bottom of the screen
		var numItemsColLabel = createMenuRet.allSubs ? "Sub-boards" : "Items";
		//var previousMsgAreaStructure = (previousMsgAreaStructures.length > 0 ? previousMsgAreaStructures[previousMsgAreaStructures.length-1] : null);
		//this.DisplayListHdrLines(this.areaChangeHdrLines.length+1, choosingGroup, previousMsgAreaStructure, selectedItemIdx); // Old
		this.DisplayListHdrLines(this.areaChangeHdrLines.length+1, choosingGroup, previousMsgAreaStructure, selectedGrpIdx);
		if (!this.useLightbarInterface || !console.term_supports(USER_ANSI))
			console.crlf(); // Not drawing the hotkey help line (this.WriteKeyHelpLine();)

		// Show the menu in a loop and get user input
		var lastSearchText = "";
		var lastSearchFoundIdx = -1;
		var drawMenu = true;
		var writeHdrLines = false; // Already displayed above
		var writeKeyHelpLine = true;
		// Menu input loop
		var menuContinueOn = true;
		while (menuContinueOn)
		{
			// Draw the header lines and key help line if needed
			if (writeHdrLines)
			{
				this.DisplayListHdrLines(this.areaChangeHdrLines.length+1, choosingGroup, previousMsgAreaStructure, selectedGrpIdx);
				if (!this.useLightbarInterface || !console.term_supports(USER_ANSI))
					console.crlf(); // Not drawing the hotkey help line (this.WriteKeyHelpLine();)
				writeHdrLines = false;
			}
			if (writeKeyHelpLine && this.useLightbarInterface && console.term_supports(USER_ANSI))
			{
				this.WriteKeyHelpLine();
				writeKeyHelpLine = false;
			}

			// Show the menu and get user input
			var selectedMenuIdx = menu.GetVal(drawMenu);
			drawMenu = true;
			var lastUserInputUpper = (typeof(menu.lastUserInput) == "string" ? menu.lastUserInput.toUpperCase() : "");
			// Applicable for ANSI/lightbar mode: If the user typed a number, the menu input loop will
			// exit with the return value being null and the user's input in lastUserInput.  Test to see
			// if the user typed a number (it will be a single number), and if it's within the number of
			// menu items, set selectedMenuIdx to the menu item index.
			if (this.useLightbarInterface && console.term_supports(USER_ANSI) && lastUserInputUpper.match(/[0-9]/))
			{
				var userInputNum = parseInt(lastUserInputUpper);
				if (!isNaN(userInputNum) && userInputNum >= 1 && userInputNum <= menu.NumItems())
				{
					// Put the user's input back in the input buffer to
					// be used for getting the rest of the message number.
					console.ungetstr(lastUserInputUpper);
					// Go to the last row on the screen and prompt for the full item number
					console.gotoxy(1, console.screen_rows);
					console.clearline("\x01n");
					console.gotoxy(1, console.screen_rows);
					var itemPromptWord = createMenuRet.allSubs ? "item" : "sub-board";
					printf("\x01cChoose %s #: \x01h", itemPromptWord);
					var userInput = console.getnum(menu.NumItems());
					if (userInput > 0)
					{
						selectedMenuIdx = userInput - 1;
						selectedItemIndexes.push(selectedMenuIdx);
						previousChosenLibOrSubdirNames.push(chosenGroupOrSubBoardName);
						if (previousChosenLibOrSubdirNames.length > 1)
						{
							chosenGroupOrSubBoardName = previousChosenLibOrSubdirNames[previousChosenLibOrSubdirNames.length-1] + nameSep + msgAreaStructure[selectedMenuIdx].name;
							// If chosenGroupOrSubBoardName is now too long, remove some of the previous labels
							while (chosenGroupOrSubBoardName.length > console.screen_columns - 1)
							{
								var sepIdx = chosenGroupOrSubBoardName.indexOf(nameSep);
								if (sepIdx > -1)
									chosenGroupOrSubBoardName = chosenGroupOrSubBoardName.substr(sepIdx + nameSep.length);
							}
						}
						else
							chosenGroupOrSubBoardName = msgAreaStructure[selectedMenuIdx].name;
					}
					else
					{
						// The user didn't make a selection.  So, we need to refresh the
						// screen (including the header, due to things being moved down one line).
						if (this.useLightbarInterface && console.term_supports(USER_ANSI))
							console.gotoxy(1, 1);
						writeHdrLines = true;
						writeKeyHelpLine = true;
						continue; // Continue to display the menu again and get the user's choice
					}
				}
			}

			// The code block above will set selectedMenuIdx if the user typed a valid entry

			// Check for aborted & other uesr input and take appropriate action. Note the first check
			// here is 'if' and not 'else if'; that's intentional.
			if (console.aborted || lastUserInputUpper == CTRL_C)
			{
				// Fully quit out (note: This check/block must be before the test for Q/ESC/null return value)
				menuContinueOn = false;
				selectionLoopContinueOn = false;
			}
			else if (typeof(selectedMenuIdx) === "number")
			{
				// The user chose a valid item (the return value is the menu item index)
				// The objects in this.msgArea_list have a 'name' property and either
                // an 'items' property if it has sub-items or a 'subItemObj' property
				// if it's a sub-board
				selectedItemIdx = null;
				selectedGrpIdx = selectedMenuIdx;
				selectedItemIndexes.push(selectedMenuIdx);
				previousChosenLibOrSubdirNames.push(msgAreaStructure[selectedMenuIdx].name);
				if (msgAreaStructure[selectedMenuIdx].hasOwnProperty("items"))
				{
					previousMsgAreaStructures.push(msgAreaStructure);
					previousMsgAreaStructuresWithItems.push(msgAreaStructureWithItems);
					if (previousChosenLibOrSubdirNames.length > 1)
					{
						chosenGroupOrSubBoardName = previousChosenLibOrSubdirNames[previousChosenLibOrSubdirNames.length-1] + nameSep + msgAreaStructure[selectedMenuIdx].name;
						// If chosenGroupOrSubBoardName is now too long, remove some of the previous labels
						while (chosenGroupOrSubBoardName.length > console.screen_columns - 1)
						{
							var sepIdx = chosenGroupOrSubBoardName.indexOf(nameSep);
							if (sepIdx > -1)
								chosenGroupOrSubBoardName = chosenGroupOrSubBoardName.substr(sepIdx + nameSep.length);
						}
					}
					else
						chosenGroupOrSubBoardName = msgAreaStructure[selectedMenuIdx].name;
					msgAreaStructure = msgAreaStructure[selectedMenuIdx].items;
					msgAreaStructureWithItems = msgAreaStructure[selectedMenuIdx];
					menuContinueOn = false;
				}
				else if (msgAreaStructure[selectedMenuIdx].hasOwnProperty("subItemObj"))
				{
					// The user has selected a sub-board
					bbs.cursub_code = msgAreaStructure[selectedMenuIdx].subItemObj.code;
					// Add to the user's last-used sub-boards dictionary & save the user's settings
					this.lastChosenSubBoardsPerGrpForUser[msg_area.sub[bbs.cursub_code].grp_name] = bbs.cursub_code;
					this.WriteUserSettingsFile();
					// Don't continue the loops
					menuContinueOn = false;
					selectionLoopContinueOn = false;
				}
			}
			else if (lastUserInputUpper == "?")
			{
				var usingLightbar = this.useLightbarInterface && console.term_supports(USER_ANSI);
				this.ShowHelpScreen(usingLightbar, true);
				menuContinueOn = true;
				selectionLoopContinueOn = true;
				drawMenu = true;
				writeHdrLines = true;
				writeKeyHelpLine = true;
			}
			else if (lastUserInputUpper == "/" || lastUserInputUpper == CTRL_F) // Start of find
			{
				// Lightbar/ANSI mode
				if (this.useLightbarInterface && console.term_supports(USER_ANSI))
				{
					console.gotoxy(1, console.screen_rows);
					console.cleartoeol("\x01n");
					console.gotoxy(1, console.screen_rows);
					var promptText = "Search: ";
					console.print(promptText);
					//var searchText = getStrWithTimeout(K_UPPER|K_NOCRLF|K_GETSTR|K_NOSPIN|K_LINE, console.screen_columns - promptText.length - 1, SEARCH_TIMEOUT_MS);
					var searchText = console.getstr(lastSearchText, console.screen_columns - promptText.length - 1, K_UPPER|K_NOCRLF|K_GETSTR|K_NOSPIN|K_LINE);
					var searchTextIsStr = (typeof(searchText) === "string");
					if (searchTextIsStr)
						lastSearchText = searchText;
					// If the user entered text, then do the search, and if found,
					// found, go to the page and select the item indicated by the
					// search.
					if (searchTextIsStr && searchText.length > 0)
					{
						var oldLastSearchFoundIdx = lastSearchFoundIdx;
						var oldSelectedItemIdx = menu.selectedItemIdx;
						var idx = this.FindMsgAreaIdxFromText(msgAreaStructure, searchText, menu.selectedItemIdx);
						lastSearchFoundIdx = idx;
						if (idx > -1)
						{
							// Set the currently selected item in the menu, and ensure it's
							// visible on the page
							menu.selectedItemIdx = idx;
							if (menu.selectedItemIdx >= menu.topItemIdx+menu.GetNumItemsPerPage())
								menu.topItemIdx = menu.selectedItemIdx - menu.GetNumItemsPerPage() + 1;
							else if (menu.selectedItemIdx < menu.topItemIdx)
								menu.topItemIdx = menu.selectedItemIdx;
							else
							{
								// If the current index and the last index are both on the same page on the
								// menu, then have the menu only redraw those items.
								menu.nextDrawOnlyItems = [menu.selectedItemIdx, oldLastSearchFoundIdx, oldSelectedItemIdx];
							}
						}
						else
						{
							var idx = this.FindMsgAreaIdxFromText(msgAreaStructure, searchText, 0);
							lastSearchFoundIdx = idx;
							if (idx > -1)
							{
								// Set the currently selected item in the menu, and ensure it's
								// visible on the page
								menu.selectedItemIdx = idx;
								if (menu.selectedItemIdx >= menu.topItemIdx+menu.GetNumItemsPerPage())
									menu.topItemIdx = menu.selectedItemIdx - menu.GetNumItemsPerPage() + 1;
								else if (menu.selectedItemIdx < menu.topItemIdx)
									menu.topItemIdx = menu.selectedItemIdx;
								else
								{
									// The current index and the last index are both on the same page on the
									// menu, so have the menu only redraw those items.
									menu.nextDrawOnlyItems = [menu.selectedItemIdx, oldLastSearchFoundIdx, oldSelectedItemIdx];
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
					writeKeyHelpLine = true;
				}
				else
				{
					// Traditional/non-ANSI interface (menu will be using numbered mode)
					console.attributes = "N";
					console.crlf();
					var promptText = "Search: ";
					console.print(promptText);
					var searchText = console.getstr(lastSearchText, console.screen_columns - promptText.length - 1, K_UPPER|K_NOCRLF|K_GETSTR|K_NOSPIN|K_LINE);
					var searchTextIsStr = (typeof(searchText) === "string");
					console.attributes = "N";
					console.crlf();
					// Don't need to set lastSearchFoundIdx
					//if (searchTextIsStr)
					//	lastSearchText = searchText;
					// If the user entered text, then do the search, and if found,
					// found, go to the page and select the item indicated by the
					// search.
					if (searchTextIsStr && searchText.length > 0)
					{
						var oldLastSearchFoundIdx = lastSearchFoundIdx;
						var oldSelectedItemIdx = menu.selectedItemIdx;
						var idx = this.FindMsgAreaIdxFromText(msgAreaStructure, searchText, 0);
						//lastSearchFoundIdx = idx; // Don't need to set lastSearchFoundIdx
						var newMsgAreaStructure = [];
						while (idx > -1)
						{
							newMsgAreaStructure.push(msgAreaStructure[idx]);

							// Find the next one
							idx = this.FindMsgAreaIdxFromText(msgAreaStructure, searchText, idx+1);
						}
						if (newMsgAreaStructure.length > 0)
						{
							selectedItemIdx = selectedItemIndexes.push(selectedItemIdx);
							msgAreaStructure = previousMsgAreaStructures.push(msgAreaStructure);
							msgAreaStructureWithItems = previousMsgAreaStructuresWithItems.push(msgAreaStructureWithItems);
							previousChosenLibOrSubdirNames.push("");
							msgAreaStructure = newMsgAreaStructure;
							grpName = "";
							if (typeof(selectedItemIdx) === "number" && selectedItemIdx >= 0 && selectedItemIdx < this.msgArea_list.length)
							{
								if (this.msgArea_list[selectedItemIdx].hasOwnProperty("name"))
									grpName = this.msgArea_list[selectedItemIdx].name;
							}
							createMenuRet = this.CreateLightbarMenu(grpName, newMsgAreaStructure, previousMsgAreaStructures.length+1, menuTopRow, 0, numItemsWidth);
							menu = createMenuRet.menuObj;
						}
						else
							console.print("Not found\r\n\x01p");
					}

					console.line_counter = 0;
					console.clear("\x01n");
					writeHdrLines = true;
					drawMenu = true;
					writeKeyHelpLine = false;
				}
			}
			else if (lastUserInputUpper == "N") // Next search result (requires an existing search term)
			{
				// Lightbar/ANSI mode
				if (this.useLightbarInterface && console.term_supports(USER_ANSI))
				{
					// This works but seems a little strange sometimes.
					// - Should this always start from the selected index?
					// - If it wraps around to one of the items on the first page,
					//   should it always set the top index to 0?
					if ((lastSearchText.length > 0) && (lastSearchFoundIdx > -1))
					{
						var oldLastSearchFoundIdx = lastSearchFoundIdx;
						var oldSelectedItemIdx = menu.selectedItemIdx;
						// Do the search, and if found, go to the page and select the item
						// indicated by the search.
						var idx = this.FindMsgAreaIdxFromText(msgAreaStructure, searchText, lastSearchFoundIdx+1);
						if (idx > -1)
						{
							lastSearchFoundIdx = idx;
							// Set the currently selected item in the menu, and ensure it's
							// visible on the page
							menu.selectedItemIdx = idx;
							if (menu.selectedItemIdx >= menu.topItemIdx+menu.GetNumItemsPerPage())
							{
								menu.topItemIdx = menu.selectedItemIdx - menu.GetNumItemsPerPage() + 1;
								if (menu.topItemIdx < 0)
									menu.topItemIdx = 0;
							}
							else if (menu.selectedItemIdx < menu.topItemIdx)
								menu.topItemIdx = menu.selectedItemIdx;
							else
							{
								// The current index and the last index are both on the same page on the
								// menu, so have the menu only redraw those items.
								menu.nextDrawOnlyItems = [menu.selectedItemIdx, oldLastSearchFoundIdx, oldSelectedItemIdx];
							}
						}
						else
						{
							idx = this.FindMsgAreaIdxFromText(msgAreaStructure, searchText, 0);
							lastSearchFoundIdx = idx;
							if (idx > -1)
							{
								// Set the currently selected item in the menu, and ensure it's
								// visible on the page
								menu.selectedItemIdx = idx;
								if (menu.selectedItemIdx >= menu.topItemIdx+menu.GetNumItemsPerPage())
								{
									menu.topItemIdx = menu.selectedItemIdx - menu.GetNumItemsPerPage() + 1;
									if (menu.topItemIdx < 0)
										menu.topItemIdx = 0;
								}
								else if (menu.selectedItemIdx < menu.topItemIdx)
									menu.topItemIdx = menu.selectedItemIdx;
								else
								{
									// The current index and the last index are both on the same page on the
									// menu, so have the menu only redraw those items.
									menu.nextDrawOnlyItems = [menu.selectedItemIdx, oldLastSearchFoundIdx, oldSelectedItemIdx];
								}
							}
							else
							{
								this.WriteLightbarKeyHelpErrorMsg("Not found");
								drawMenu = false;
								writeKeyHelpLine = true;
							}
						}
					}
					else
					{
						this.WriteLightbarKeyHelpErrorMsg("There is no previous search", true);
						drawMenu = false;
						writeKeyHelpLine = true;
					}
				}
				else
				{
					// Traditional/non-ANSI interface (menu will be using numbered mode)
					// TODO
					console.attributes = "N";
					console.crlf();
					console.print("TODO\r\n\x01p");
					console.clear("\x01n");
					writeHdrLines = true;
					drawMenu = true;
					writeKeyHelpLine = false;
				}
			}
			else if (lastUserInputUpper == CTRL_U)
			{
				var usingANSI = this.useLightbarInterface && console.term_supports(USER_ANSI);
				// Make a backup of the user's sort setting so we can compare it later
				var previousSortSetting = this.userSettings.areaChangeSorting;
				var userSettingsRetObj;
				if (usingANSI)
					userSettingsRetObj = this.DoUserSettings_Scrollable();
				else
					this.DoUserSettings_Traditional();

				// If the user's sort setting changed, set menuContinueOn to false.
				// That will re-create the menu and sort it accordingly and re-draw.
				if (this.userSettings.areaChangeSorting != previousSortSetting)
				{
					menuContinueOn = false;
					sortingChanged = true;
					sortHeirarchyRecursive(this.msgArea_list, this.userSettings.areaChangeSorting);
				}
				else
				{
					// If using the ANSI/scrolling interface, refresh the screen
					if (usingANSI)
					{
						if (userSettingsRetObj.needWholeScreenRefresh)
						{
							writeHdrLines = true;
							drawMenu = true;
							writeKeyHelpLine = true;
						}
						else
						{
							//var menu = createMenuRet.menuObj;
							menu.DrawPartialAbs(userSettingsRetObj.optionBoxTopLeftX,
												userSettingsRetObj.optionBoxTopLeftY,
												userSettingsRetObj.optionBoxWidth,
												userSettingsRetObj.optionBoxHeight);
							writeHdrLines = false;
							drawMenu = false;
							writeKeyHelpLine = false;
						}
					}
				}
			}
			// Quit - Note: This check should be last
			else if (lastUserInputUpper == "Q" || lastUserInputUpper == KEY_ESC || selectedMenuIdx == null)
			{
				// Cancel/Quit
				// Quit this menu loop and go back to the previous file lib/dir structure
				menuContinueOn = false;
				selectedItemIdx = selectedItemIndexes.pop();
				// TODO: I don't remember why I had the following 2 lines:
				/*
				if (selectedItemIndexes.length > 0)
					selectedItemIdx = selectedItemIndexes.pop();
				*/
				selectedGrpIdx = selectedItemIdx;
				if (previousMsgAreaStructures.length == 0)
				{
					// The user was at the first level in the lib/dir structure; fully quit out from here
					selectionLoopContinueOn = false;
				}
				else // Go to the previous file lib/dir structure
				{
					msgAreaStructure = previousMsgAreaStructures.pop();
					msgAreaStructureWithItems = previousMsgAreaStructuresWithItems.pop();
					if (msgAreaStructure == this.msgArea_list)
					{
						chosenGroupOrSubBoardName = "";
						previousChosenLibOrSubdirNames = [];
						selectedItemIndexes = [];
					}
					else
						chosenGroupOrSubBoardName = previousChosenLibOrSubdirNames.pop();
				}
			}
		}
	}
}

// For the DDMsgAreaChooser class: Displays the header & header lines above the list.
//
// Parameters:
//  pScreenRow: The row on the screen to write the lines at.  If no cursor movements are desired, this can be null.
//  pChooseGroup: Boolean - Whether or not the user is choosing a message group
//  pMsgAreaHeirarchyObj: An object from this.msgArea_list, which is
//                        set up with a 'name' property and either
//                        an 'items' property if it has sub-items
//                        or a 'subItemObj' property if it's a sub-board
//  pGrpIndex: The index of the message group (assumed to be valid)
//  pGrpIdx: The index of the message group being used
//  pNumPages: Optional - The number of pages of items
//  pPageNum: Optional - The current page number for the items
function DDMsgAreaChooser_DisplayListHdrLines(pScreenRow, pChooseGroup, pMsgAreaHeirarchyObj, pGrpIdx, pNumPages, pPageNum)
{
	this.DisplayAreaChgHdr(1);
	if (typeof(pScreenRow) === "number")
		console.gotoxy(1, pScreenRow);
	if (pChooseGroup)
		this.WriteGrpListHdrLine(pNumPages, pPageNum);
	else
	{
		// See if the items in the directory heirarchy only contain a "subItemObj" or an "items"
		var allItemsAreSubBoards = true;
		var allItemsContainOnlyOtherItems = true;
		if (typeof(pMsgAreaHeirarchyObj[pGrpIdx]) === "object" && !Array.isArray(pMsgAreaHeirarchyObj[pGrpIdx]) && pMsgAreaHeirarchyObj[pGrpIdx].hasOwnProperty("items"))
		{
			for (var i = 0; i < pMsgAreaHeirarchyObj[pGrpIdx].items.length; ++i)
			{
				if (!pMsgAreaHeirarchyObj[pGrpIdx].items[i].hasOwnProperty("subItemObj") || pMsgAreaHeirarchyObj[pGrpIdx].items[i].hasOwnProperty("items"))
					allItemsAreSubBoards = false;
				if (pMsgAreaHeirarchyObj[pGrpIdx].items[i].hasOwnProperty("subItemObj") || !pMsgAreaHeirarchyObj[pGrpIdx].items[i].hasOwnProperty("items"))
					allItemsContainOnlyOtherItems = false;
			}
		}

		// Write the list header lines
		this.WriteSubBrdListHdr1Line(pMsgAreaHeirarchyObj, pGrpIdx);
		if (typeof(pScreenRow) === "number")
			console.gotoxy(1, pScreenRow+1);
		else
			console.crlf();
		if (allItemsContainOnlyOtherItems)
			this.WriteGrpListHdrLine(pNumPages, pPageNum);
		else
		{
			var numItemsText = "Items";
			if (allItemsAreSubBoards)
				numItemsText = "Posts";
			else if (allItemsContainOnlyOtherItems)
				numItemsText = "Items";
			if (this.showDatesInSubBoardList)
				printf(this.subBoardListHdrPrintfStr, "Sub #", "Name", "# " + numItemsText, "Latest date & time");
			else
				printf(this.subBoardListHdrPrintfStr, "Sub #", "Name", "# " + numItemsText);
		}
	}
}

// For the DDMsgAreaChooser class: Creates a lightbar menu to choose a message group/sub-board.
//
// Parameters:
//  pGrpName: The name of the message group (or an empty string if there is none yet)
//  pMsgAreaHeirarchyObj: An object from this.msgArea_list, which is
//                        set up with a 'name' property and either
//                        an 'items' property if it has sub-items
//                        or a 'subItemObj' property if it's a sub-board
//  pHeirarchyLevel: The level we're at in the heirarchy (1-based)
//  pMenuTopRow: The screen row to use for the menu's top row
//  pSelectedItemIdx: The index to use for the selected item. If not
//                    specified, the item with the user's current selected file
//                    directory will be used, if available.
//  pItemNumWidth: The character width of the column label for the item number;
//                 mainly for the traditional (non-lightbar) UI but is used for lightbar
//                 mode as well for level > 2 (not sure why needed though)
//
// Return value: An object with the following properties:
//               menuObj: The menu object
//               allSubs: Whether or not all the items in the menu
//                        are sub-boards. If not, some or all are
//                        other lists of items. If this is true,
//                        allOnlyOtherItems should be false.
//               allOnlyOtherItems: Whether or not all the items in the menu
//                                  only have arrays of other items. This is
//                                  mutually exclusive with allSubs. If this
//                                  is true, allSubs should be false.
//               itemNumWidth: The width of the item numbers column
//               descWidth: The width of the description column
//               numItemsWidth: The width of the # of items column
function DDMsgAreaChooser_CreateLightbarMenu(pGrpName, pMsgAreaHeirarchyObj, pHeirarchyLevel, pMenuTopRow, pSelectedItemIdx, pItemNumWidth)
{
	var retObj = {
		menuObj: null,
		allSubs: true,
		allOnlyOtherItems: true,
		itemNumWidth: 0,
		descWidth: 0,
		numItemsWidth: 0
	};

	// Get color index information for the menu
	var colorIdxInfo = this.GetColorIndexInfoForLightbarMenu(pMsgAreaHeirarchyObj);

	// Calculate column widths for the return object
	retObj.itemNumWidth = colorIdxInfo.subBoardListIdxes.subNumend - 1;
	//retObj.descWidth = msgGrpListIdxes.descEnd - msgGrpListIdxes.descStart;
	retObj.descWidth = this.subBoardNameLen;
	retObj.numItemsWidth = console.screen_columns - colorIdxInfo.msgGrpListIdxes.numItemsStart;

	// Create the menu object
	var fileDirMenuHeight = console.screen_rows - pMenuTopRow;
	var msgAreaMenu = new DDLightbarMenu(1, pMenuTopRow, console.screen_columns, fileDirMenuHeight);
	/*
	if (typeof(console.mouse_mode) === "number")
		msgAreaMenu.mouseEnabled = !Boolean(console.mouse_mode & MOUSE_MODE_OFF);
	*/
	// Add additional keypresses for quitting the menu's input loop so we can
	// respond to these keys
	msgAreaMenu.AddAdditionalQuitKeys("qQ?/" + CTRL_F + CTRL_U);
	msgAreaMenu.AddAdditionalFirstPageKeys("fF");
	msgAreaMenu.AddAdditionalLastPageKeys("lL");
	if (this.useLightbarInterface && console.term_supports(USER_ANSI))
	{
		msgAreaMenu.allowANSI = true;
		// Additional quit keys for ANSI mode which we can respond to
		// N: Next (search) and numbers for item input
		msgAreaMenu.AddAdditionalQuitKeys(" nN0123456789" + CTRL_C);
	}
	// If not using the lightbar interface (and ANSI behavior is not to be allowed), msgAreaMenu.numberedMode
	// will be set to true by default.  Also, in that situation, set the menu's item number color
	else
	{
		msgAreaMenu.allowANSI = false;
		msgAreaMenu.colors.itemNumColor = this.colors.areaNum;
		retObj.itemNumWidth = pMsgAreaHeirarchyObj.length.toString().length;
		//retObj.descWidth -= 3;
	}
	msgAreaMenu.scrollbarEnabled = true;
	msgAreaMenu.borderEnabled = false;
	// Menu prompt text for non-ANSI mode
	msgAreaMenu.nonANSIPromptText = "\x01n\x01b\x01h" + TALL_UPPER_MID_BLOCK + " \x01n\x01cWhich, \x01hQ\x01n\x01cuit, \x01hCTRL-F\x01n\x01c, \x01h/\x01n\x01c: \x01h";

	// Menu item colors
	msgAreaMenu.itemColorForSubBoard = colorIdxInfo.itemColorForSubBoard;
	msgAreaMenu.selectedItemColorForSubBoard = colorIdxInfo.selectedItemColorForSubBoard;
	msgAreaMenu.itemColorForItemWithSubItems = colorIdxInfo.itemColorForItemWithSubItems;
	msgAreaMenu.selectedItemColorForItemWithSubItems = colorIdxInfo.selectedItemColorForItemWithSubItems;

	// See if all the items in pMsgAreaHeirarchyObj are sub-boards.
	// Also, see which one has the user's current chosen sub-board so we can set the
	// current menu item index in the menu
	msgAreaMenu.idxWithUserSelectedSubBoard = -1;
	if (Array.isArray(pMsgAreaHeirarchyObj))
	{
		//msgAreaMenu.idxWithUserSelectedSubBoard = -1;
		for (var i = 0; i < pMsgAreaHeirarchyObj.length; ++i)
		{
			// Each object will have either an "items" or a "subItemObj"
			if (!pMsgAreaHeirarchyObj[i].hasOwnProperty("subItemObj"))
				retObj.allSubs = false;
			// See if this one has the user's selected message sub-board
			var lastChosenSubsObj = null;
			if (pHeirarchyLevel > 1 && this.userSettings.rememberLastSubBoardWhenChangingGrp)
				lastChosenSubsObj = this.lastChosenSubBoardsPerGrpForUser;
			if (msgAreaStructureHasCurrentUserSubBoard(pMsgAreaHeirarchyObj[i], null, lastChosenSubsObj))
				msgAreaMenu.idxWithUserSelectedSubBoard = i;
			// If we've found all we need, then stop going through the array
			if (!retObj.allSubs && msgAreaMenu.idxWithUserSelectedSubBoard > -1)
				break;
		}
	}

	msgAreaMenu.multiSelect = false;
	msgAreaMenu.ampersandHotkeysInItems = false;
	msgAreaMenu.wrapNavigation = false;

	// Build the sub-board info for the given message group
	msgAreaMenu.msgAreaHeirarchyObj = pMsgAreaHeirarchyObj;
	msgAreaMenu.areaChooser = this;
	msgAreaMenu.allSubs = true; // Whether the menu has only sub-boards
	if (Array.isArray(pMsgAreaHeirarchyObj))
	{
		// See if any of the items in the array aren't directories, and set retObj.allSubs.
		// Also, see which one has the user's current chosen sub-board so we can set the
		// current menu item index - And save that index in the menu object for its
		// reference later.
		var lastChosenSubsObj = null;
		var subCodeOverride = null;
		if (pHeirarchyLevel > 1 && this.userSettings.rememberLastSubBoardWhenChangingGrp)
		{
			lastChosenSubsObj = this.lastChosenSubBoardsPerGrpForUser;
			if (this.lastChosenSubBoardsPerGrpForUser.hasOwnProperty(pGrpName))
				subCodeOverride = this.lastChosenSubBoardsPerGrpForUser[pGrpName];
		}
		var tmpRetObj = setMenuIdxWithSelectedSubBoard(msgAreaMenu, pMsgAreaHeirarchyObj, subCodeOverride, lastChosenSubsObj);
		retObj.allSubs = tmpRetObj.allSubs;
		retObj.allOnlyOtherItems = tmpRetObj.allOnlyOtherItems;

		// Replace the menu's NumItems() function to return the correct number of items
		msgAreaMenu.NumItems = function() {
			return this.msgAreaHeirarchyObj.length;
		};
		msgAreaMenu.numItemsLen = msgAreaMenu.NumItems().toString().length;
		// Replace the menu's GetItem() function to create & return an item for the menu
		msgAreaMenu.GetItem = function(pItemIdx) {
			var menuItemObj = this.MakeItemWithRetval(-1);
			/*
			var lastChosenSubsObj = this.userSettings.rememberLastSubBoardWhenChangingGrp ? this.lastChosenSubBoardsPerGrpForUser : null;
			var showDirMark = msgAreaStructureHasCurrentUserSubBoard(this.msgAreaHeirarchyObj[pItemIdx], null, lastChosenSubsObj);
			*/
			var showDirMark = (pItemIdx == this.idxWithUserSelectedSubBoard);
			var areaDesc = this.msgAreaHeirarchyObj[pItemIdx].name;
			var numItems = 0;
			if (this.msgAreaHeirarchyObj[pItemIdx].hasOwnProperty("items"))
			{
				numItems = this.msgAreaHeirarchyObj[pItemIdx].items.length;
				// If this isn't the top level (libraries), then add "<subdirs>" to the description
				if (this.msgAreaHeirarchyObj != this.areaChooser.lib_list)
					areaDesc += "  <subsubs>";

                // Menu item color arrays
                menuItemObj.itemColor = this.itemColorForItemWithSubItems;
                menuItemObj.itemSelectedColor = this.selectedItemColorForItemWithSubItems;
				
				// Menu item text
				menuItemObj.text = (showDirMark ? "*" : " ");
				if (this.allowANSI)
				{
					menuItemObj.text += " " + format(this.areaChooser.msgGrpListPrintfStr, +(pItemIdx+1),
					                                 this.msgAreaHeirarchyObj[pItemIdx].name.substr(0, this.areaChooser.msgGrpDescLen),
					                                 this.msgAreaHeirarchyObj[pItemIdx].items.length);
					menuItemObj.text = strip_ctrl(menuItemObj.text);
				}
				else
				{
					// Traditional UI or no ANSI - Numbered mode
					// Number of spaces before the group name
					var numSpaces = pItemNumWidth - this.numItemsLen - console.strlen(menuItemObj.text);
					if (numSpaces > 0)
						menuItemObj.text += format("%*s", numSpaces, "");
					menuItemObj.text += format(this.areaChooser.msgGrpListPrintfStrWithoutAreaNum,
					                           this.msgAreaHeirarchyObj[pItemIdx].name.substr(0, this.areaChooser.msgGrpDescLen),
					                           this.msgAreaHeirarchyObj[pItemIdx].items.length);
					menuItemObj.text = strip_ctrl(menuItemObj.text);
				}
			}
			else if (this.msgAreaHeirarchyObj[pItemIdx].hasOwnProperty("subItemObj"))
			{
				var numPostsLabelWidth = 7; // Width of the "# Posts" label

				if (this.msgAreaHeirarchyObj[pItemIdx].subItemObj.hasOwnProperty("posts"))
					numItems = this.msgAreaHeirarchyObj[pItemIdx].subItemObj.posts; // Added in Synchronet 3.18c
				else
				{
					// Get information from the messagebase
					var msgBase = new MsgBase(this.msgAreaHeirarchyObj[pItemIdx].subItemObj.code);
					if (msgBase.open())
					{
						numItems = msgBase.total_msgs;
						msgBase.close();
					}
				}

				/*
				// If this menu contaons only sub-boards, then set the
				// item color arrays here
				if (this.allSubs)
				{
					// Menu item color arrays
					menuItemObj.itemColor = msgAreaMenu.itemColorForSubBoard;
					menuItemObj.itemSelectedColor = msgAreaMenu.selectedItemColorForSubBoard;
				}
				*/

				var grpIdx = this.msgAreaHeirarchyObj[pItemIdx].subItemObj.grp_index;
				// Ensure the subBoardListPrintfInfo object is built for the given group
				this.areaChooser.BuildSubBoardPrintfInfoForGrp(grpIdx);
				// Menu item text
				menuItemObj.text = (showDirMark ? "*" : " ");
				if (this.allowANSI)
				{
					// Menu item color arrays for the item
					var itemColorIdxInfo = this.areaChooser.GetSubBoardColorIndexInfoAndFormatStrForMenuItem(pItemNumWidth, numPostsLabelWidth);
					menuItemObj.itemColor = itemColorIdxInfo.itemColorInfo;
					menuItemObj.itemSelectedColor = itemColorIdxInfo.selectedItemColorInfo;
					var formatStr = itemColorIdxInfo.formatStr;
					// Note: this.areaChooser.subBoardNameLen and this.areaChooser.subBoardListPrintfInfo[grpIdx].nameLen
					// are probably the same.
					// Get the timestamp of the last message, if configured to do so
					if (this.areaChooser.showDatesInSubBoardList)
					{
						var lastMsgPostTimestamp = getLatestMsgTime(this.msgAreaHeirarchyObj[pItemIdx].subItemObj.code);
						//this.areaChooser.subBoardListPrintfInfo[grpIdx].printfStr
						menuItemObj.text += format(formatStr, pItemIdx+1,
												   this.msgAreaHeirarchyObj[pItemIdx].name.substr(0, this.areaChooser.subBoardNameLen),
												   numItems,
						                           strftime("%Y-%m-%d", lastMsgPostTimestamp),
						                           strftime("%H:%M:%S", lastMsgPostTimestamp));
					}
					else
					{
						//this.areaChooser.subBoardListPrintfInfo[grpIdx].printfStr
						menuItemObj.text += format(formatStr, pItemIdx+1,
												   this.msgAreaHeirarchyObj[pItemIdx].name.substr(0, this.areaChooser.subBoardNameLen), numItems);
					}
				}
				else
				{
					// No ANSI - Numbered mode

					// Number of spaces before the group name
					var numSpaces = pItemNumWidth - this.numItemsLen - console.strlen(menuItemObj.text);
					if (numSpaces > 0)
						menuItemObj.text += format("%*s", numSpaces, " ");

					// Menu item coloring & format string
					var itemColorIdxInfo = this.areaChooser.GetSubBoardColorIndexInfoAndFormatStrForMenuItem(pItemNumWidth, numPostsLabelWidth, numSpaces);
					menuItemObj.itemColor = itemColorIdxInfo.itemColorInfo;
					menuItemObj.itemSelectedColor = itemColorIdxInfo.selectedItemColorInfo;
					var formatStr = itemColorIdxInfo.formatStr;
					// Generate the item text
					if (this.areaChooser.showDatesInSubBoardList)
					{
						var lastMsgPostTimestamp = getLatestMsgTime(this.msgAreaHeirarchyObj[pItemIdx].subItemObj.code);
						//this.areaChooser.subBoardListPrintfInfo[grpIdx].printfStrWithoutAreaNum
						menuItemObj.text += format(formatStr,
												  this.msgAreaHeirarchyObj[pItemIdx].name.substr(0, this.areaChooser.subBoardNameLen), numItems,
						                          strftime("%Y-%m-%d", lastMsgPostTimestamp),
						                          strftime("%H:%M:%S", lastMsgPostTimestamp));
					}
					else
					{
						//this.areaChooser.subBoardListPrintfInfo[grpIdx].printfStrWithoutAreaNum
						menuItemObj.text += format(formatStr,
						                          this.msgAreaHeirarchyObj[pItemIdx].name.substr(0, this.areaChooser.subBoardNameLen), numItems);
					}
				}
			}

			menuItemObj.text = strip_ctrl(menuItemObj.text);
			menuItemObj.retval = pItemIdx;
			return menuItemObj;
		};

		// Set the currently selected item
		var selectedIdx = msgAreaMenu.idxWithUserSelectedSubBoard;
		//if (user.is_sysop) printf("\x01nselectedIdx: %d   \r\n\x01p", selectedIdx); // Temporary
		// TODO: The first 'if' block here is new, for last sub-board functionality
		if (pGrpName.length > 0 && this.userSettings.rememberLastSubBoardWhenChangingGrp && this.lastChosenSubBoardsPerGrpForUser.hasOwnProperty(pGrpName))
		{
			for (var i = 0; i < pMsgAreaHeirarchyObj.length; ++i)
			{
				if (pMsgAreaHeirarchyObj[i].hasOwnProperty("subItemObj") && pMsgAreaHeirarchyObj[i].code == this.lastChosenSubBoardsPerGrpForUser[pGrpName])
				{
					selectedIdx = i;
					break;
				}
			}
		}
		else if (pSelectedItemIdx != null && typeof(pSelectedItemIdx) === "number" && pSelectedItemIdx >= 0 && pSelectedItemIdx < msgAreaMenu.NumItems())
			selectedIdx = pSelectedItemIdx;
		if (selectedIdx >= 0 && selectedIdx < msgAreaMenu.NumItems())
		{
			msgAreaMenu.selectedItemIdx = selectedIdx;
			if (msgAreaMenu.selectedItemIdx >= msgAreaMenu.topItemIdx+msgAreaMenu.GetNumItemsPerPage())
				msgAreaMenu.topItemIdx = msgAreaMenu.selectedItemIdx - msgAreaMenu.GetNumItemsPerPage() + 1;
		}
	}

	retObj.menuObj = msgAreaMenu;
	return retObj;
}
// Helper for DDMsgAreaChooser_CreateLightbarMenu(): Returns arrays of objects with start, end, and attrs properties
// for the lightbar menu to add colors to the menu items.
//
// Parameters:
//  pMsgAreaHeirarchyObj: An object from this.msgArea_list, which is
//                        set up with a 'name' property and either
//                        an 'items' property if it has sub-items
//                        or a 'subItemObj' property if it's a sub-board
//  pAreaNumWidthOverride: Optional - An override for the width of the area # column. Mainly for the traditional (non-lightbar) UI
//                         If this is not specified/null, then this.areaNumLen will be used.
//  pColorIdxOffset: Optional - An offset to adjust the color indexes by in the color arrays.
//                   Must be positive.
function DDMsgAreaChooser_GetColorIndexInfoForLightbarMenu(pMsgAreaHeirarchyObj, pAreaNumWidthOverride, pColorIdxOffset)
{
	var retObj = {
		msgGrpListIdxes: {},
		subBoardListIdxes: {},
		itemColorForSubBoard: [],
		selectedItemColorForSubBoard: [],
		itemColorForItemWithSubItems: [],
		selectedItemColorForItemWithSubItems: []
	};

	var usingLightbarInterface = this.useLightbarInterface && console.term_supports(USER_ANSI);
	//var areaNumLen = usingLightbarInterface ? this.areaNumLen : pMsgAreaHeirarchyObj.length.toString().length;
	var areaNumLen = this.areaNumLen;
	if (typeof(pAreaNumWidthOverride) === "number" && pAreaNumWidthOverride >= 0)
		areaNumLen = pAreaNumWidthOverride;

	// Find the length of the highest number of messages of sub-boards in pMsgAreaHeirarchyObj
	var highestNumMsgs = 0;
	for (var i = 0; i < pMsgAreaHeirarchyObj.length; ++i)
	{
		if (pMsgAreaHeirarchyObj[i].hasOwnProperty("subItemObj"))
		{
			// The 'posts' property was added in Synchronet 3.18c
			if (msg_area.sub[pMsgAreaHeirarchyObj[i].subItemObj.code].hasOwnProperty("posts"))
			{
				if (msg_area.sub[pMsgAreaHeirarchyObj[i].subItemObj.code].posts > highestNumMsgs)
					highestNumMsgs = msg_area.sub[pMsgAreaHeirarchyObj[i].subItemObj.code].posts;
			}
			else
			{
				// Get the total number of messages in the sub-board.  It isn't accurate, but it's fast.
				var msgBase = new MsgBase(pMsgAreaHeirarchyObj[i].subItemObj.code);
				if (msgBase.open())
				{
					if (msgBase.total_msgs > highestNumMsgs)
						highestNumMsgs = msgBase.total_msgs;
					msgBase.close();
				}
			}
		}
	}
	var numMsgsLen = Math.max(1, highestNumMsgs.toString().length);

	// Start & end indexes for the various items in each item list row
	// subBoardDescLen: description column width for sub-board list, accounts for numMsgsLen
	// (e.g. 5 digits for 10000+ messages) so the description color doesn't extend into the # column
	var subBoardDescLen = console.screen_columns - areaNumLen - numMsgsLen - 5;
	if (this.showDatesInSubBoardList)
		subBoardDescLen -= (this.dateLen + this.timeLen + 2);
	// nameLen used for non-lightbar mode
	var nameLen = console.screen_columns - areaNumLen - numMsgsLen - this.dateLen - 14; // Was - 5
	if (usingLightbarInterface)
	{
		retObj.msgGrpListIdxes = {
			markCharStart: 0,
			markCharEnd: 1,
			grpNumStart: 1,
			//grpNumEnd: 2 + (+this.areaNumLen)
			grpNumEnd: 2 + areaNumLen
		};
		retObj.msgGrpListIdxes.descStart = retObj.msgGrpListIdxes.grpNumEnd;
		retObj.msgGrpListIdxes.descEnd = retObj.msgGrpListIdxes.descStart + +this.msgGrpDescLen;

		retObj.subBoardListIdxes = {
			markCharStart: 0,
			markCharEnd: 1,
			subNumStart: 1,
			//subNumEnd: 3 + (+this.areaNumLen)
			subNumEnd: 3 + areaNumLen
		};
		retObj.subBoardListIdxes.descStart = retObj.subBoardListIdxes.subNumEnd;
	}
	else
	{
		retObj.msgGrpListIdxes = {
			markCharStart: 0,
			markCharEnd: 1,
			descStart: 1
		};
		retObj.subBoardListIdxes = {
			markCharStart: 0,
			markCharEnd: 1,
			descStart: 1
		};
	}
	// Remainder of the message group colors
	retObj.msgGrpListIdxes.descEnd = 2 + (+this.msgGrpDescLen);
	retObj.msgGrpListIdxes.numItemsStart = retObj.msgGrpListIdxes.descEnd;
	// Set numItemsEnd to -1 to let the whole rest of the lines be colored
	retObj.msgGrpListIdxes.numItemsEnd = -1;
	// Remainder of sub-board colors
	// Use subBoardDescLen (which accounts for numMsgsLen) so the description color
	// doesn't extend into the # messages column when a sub-board has 10000+ messages
	//retObj.subBoardListIdxes.descEnd = retObj.subBoardListIdxes.descStart + nameLen;
	retObj.subBoardListIdxes.descEnd = retObj.subBoardListIdxes.descStart + subBoardDescLen;
	// For the sub-board list, if not using the lightbar interface, we still need
	// to account for the length of the item numbers, which will be displayed by
	// the menu object rather than by us
	if (!usingLightbarInterface)
	{
		//retObj.subBoardListIdxes.descEnd += this.areaNumLen;
		retObj.subBoardListIdxes.descEnd += areaNumLen;
	}
	retObj.subBoardListIdxes.numItemsStart = retObj.subBoardListIdxes.descEnd;
	retObj.subBoardListIdxes.numItemsEnd = retObj.subBoardListIdxes.numItemsStart + numMsgsLen + 1;
	//retObj.subBoardListIdxes.numItemsEnd = retObj.subBoardListIdxes.numItemsStart + numMsgsLen+2 + 1;
	if (this.showDatesInSubBoardList)
	{
		retObj.subBoardListIdxes.dateStart = retObj.subBoardListIdxes.numItemsEnd;
		retObj.subBoardListIdxes.dateEnd = retObj.subBoardListIdxes.dateStart + +this.dateLen + 1;
		retObj.subBoardListIdxes.timeStart = retObj.subBoardListIdxes.dateEnd;
		// Set timeEnd to -1 to let the whole rest of the lines be colored
		retObj.subBoardListIdxes.timeEnd = -1;
	}

	// Menu item colors
	if (usingLightbarInterface)
	{
		// Colors for items that are sub-boards
		if (this.showDatesInSubBoardList)
		{
			retObj.itemColorForSubBoard = [{start: retObj.subBoardListIdxes.markCharStart, end: retObj.subBoardListIdxes.markCharEnd, attrs: this.colors.areaMark},
			                               {start: retObj.subBoardListIdxes.subNumStart, end: retObj.subBoardListIdxes.subNumEnd, attrs: this.colors.areaNum},
			                               {start: retObj.subBoardListIdxes.descStart, end: retObj.subBoardListIdxes.descEnd, attrs: this.colors.desc},
			                               {start: retObj.subBoardListIdxes.numItemsStart, end: retObj.subBoardListIdxes.numItemsEnd, attrs: this.colors.numItems},
			                               {start: retObj.subBoardListIdxes.dateStart, end: retObj.subBoardListIdxes.dateEnd, attrs: this.colors.latestDate},
			                               {start: retObj.subBoardListIdxes.timeStart, end: retObj.subBoardListIdxes.timeEnd, attrs: this.colors.latestTime}];
			retObj.selectedItemColorForSubBoard = [{start: retObj.subBoardListIdxes.markCharStart, end: retObj.subBoardListIdxes.markCharEnd, attrs: this.colors.areaMark + this.colors.bkgHighlight},
			                                       {start: retObj.subBoardListIdxes.subNumStart, end: retObj.subBoardListIdxes.subNumEnd, attrs: this.colors.areaNumHighlight + this.colors.bkgHighlight},
			                                       {start: retObj.subBoardListIdxes.descStart, end: retObj.subBoardListIdxes.descEnd, attrs: this.colors.descHighlight + this.colors.bkgHighlight},
			                                       {start: retObj.subBoardListIdxes.numItemsStart, end: retObj.subBoardListIdxes.numItemsEnd, attrs: this.colors.numItemsHighlight + this.colors.bkgHighlight},
			                                       {start: retObj.subBoardListIdxes.dateStart, end: retObj.subBoardListIdxes.dateEnd, attrs: this.colors.dateHighlight + this.colors.bkgHighlight},
			                                       {start: retObj.subBoardListIdxes.timeStart, end: retObj.subBoardListIdxes.timeEnd, attrs: this.colors.timeHighlight + this.colors.bkgHighlight}];
		}
		else
		{
			retObj.itemColorForSubBoard = [{start: retObj.subBoardListIdxes.markCharStart, end: retObj.subBoardListIdxes.markCharEnd, attrs: this.colors.areaMark},
			                               {start: retObj.subBoardListIdxes.subNumStart, end: retObj.subBoardListIdxes.subNumEnd, attrs: this.colors.areaNum},
			                               {start: retObj.subBoardListIdxes.descStart, end: retObj.subBoardListIdxes.descEnd, attrs: this.colors.desc},
			                               {start: retObj.subBoardListIdxes.numItemsStart, end: retObj.subBoardListIdxes.numItemsEnd, attrs: this.colors.numItems}];
			retObj.selectedItemColorForSubBoard = [{start: retObj.subBoardListIdxes.markCharStart, end: retObj.subBoardListIdxes.markCharEnd, attrs: this.colors.areaMark + this.colors.bkgHighlight},
			                                       {start: retObj.subBoardListIdxes.subNumStart, end: retObj.subBoardListIdxes.subNumEnd, attrs: this.colors.areaNumHighlight + this.colors.bkgHighlight},
			                                       {start: retObj.subBoardListIdxes.descStart, end: retObj.subBoardListIdxes.descEnd, attrs: this.colors.descHighlight + this.colors.bkgHighlight},
			                                       {start: retObj.subBoardListIdxes.numItemsStart, end: retObj.subBoardListIdxes.numItemsEnd, attrs: this.colors.numItemsHighlight + this.colors.bkgHighlight}];
		}
		// Colors for items that have another list of items
		retObj.itemColorForItemWithSubItems = [{start: retObj.msgGrpListIdxes.markCharStart, end: retObj.msgGrpListIdxes.markCharEnd, attrs: this.colors.areaMark},
		                                       {start: retObj.msgGrpListIdxes.grpNumStart, end: retObj.msgGrpListIdxes.grpNumEnd, attrs: this.colors.areaNum},
		                                       {start: retObj.msgGrpListIdxes.descStart, end: retObj.msgGrpListIdxes.descEnd, attrs: this.colors.desc},
		                                       {start: retObj.msgGrpListIdxes.numItemsStart, end: retObj.msgGrpListIdxes.numItemsEnd, attrs: this.colors.numItems}];
		retObj.selectedItemColorForItemWithSubItems = [{start: retObj.msgGrpListIdxes.markCharStart, end: retObj.msgGrpListIdxes.markCharEnd, attrs: this.colors.areaMark + this.colors.bkgHighlight},
		                                               {start: retObj.msgGrpListIdxes.grpNumStart, end: retObj.msgGrpListIdxes.grpNumEnd, attrs: this.colors.areaNumHighlight + this.colors.bkgHighlight},
		                                               {start: retObj.msgGrpListIdxes.descStart, end: retObj.msgGrpListIdxes.descEnd, attrs: this.colors.descHighlight + this.colors.bkgHighlight},
		                                               {start: retObj.msgGrpListIdxes.numItemsStart, end: retObj.msgGrpListIdxes.numItemsEnd, attrs: this.colors.numItemsHighlight + this.colors.bkgHighlight}];
	}
	else
	{
		// Colors for items that are sub-boards
		if (this.showDatesInSubBoardList)
		{
			retObj.itemColorForSubBoard = [{start: retObj.subBoardListIdxes.markCharStart, end: retObj.subBoardListIdxes.markCharEnd, attrs: this.colors.areaMark},
			                               {start: retObj.subBoardListIdxes.descStart, end: retObj.subBoardListIdxes.descEnd, attrs: this.colors.desc},
			                               {start: retObj.subBoardListIdxes.numItemsStart, end: retObj.subBoardListIdxes.numItemsEnd, attrs: this.colors.numItems},
			                               {start: retObj.subBoardListIdxes.dateStart, end: retObj.subBoardListIdxes.dateEnd, attrs: this.colors.latestDate},
			                               {start: retObj.subBoardListIdxes.timeStart, end: retObj.subBoardListIdxes.timeEnd, attrs: this.colors.latestTime}];
			retObj.selectedItemColorForSubBoard = [{start: retObj.subBoardListIdxes.markCharStart, end: retObj.subBoardListIdxes.markCharEnd, attrs: this.colors.areaMark + this.colors.bkgHighlight},
			                                       {start: retObj.subBoardListIdxes.descStart, end: retObj.subBoardListIdxes.descEnd, attrs: this.colors.descHighlight + this.colors.bkgHighlight},
			                                       {start: retObj.subBoardListIdxes.numItemsStart, end: retObj.subBoardListIdxes.numItemsEnd, attrs: this.colors.numItemsHighlight + this.colors.bkgHighlight},
			                                       {start: retObj.subBoardListIdxes.dateStart, end: retObj.subBoardListIdxes.dateEnd, attrs: this.colors.dateHighlight + this.colors.bkgHighlight},
			                                       {start: retObj.subBoardListIdxes.timeStart, end: retObj.subBoardListIdxes.timeEnd, attrs: this.colors.timeHighlight + this.colors.bkgHighlight}];
		}
		else
		{
			retObj.itemColorForSubBoard = [{start: retObj.subBoardListIdxes.markCharStart, end: retObj.subBoardListIdxes.markCharEnd, attrs: this.colors.areaMark},
			                               {start: retObj.subBoardListIdxes.descStart, end: retObj.subBoardListIdxes.descEnd, attrs: this.colors.desc},
			                               {start: retObj.subBoardListIdxes.numItemsStart, end: retObj.subBoardListIdxes.numItemsEnd, attrs: this.colors.numItems}];
			retObj.selectedItemColorForSubBoard = [{start: retObj.subBoardListIdxes.markCharStart, end: retObj.subBoardListIdxes.markCharEnd, attrs: this.colors.areaMark + this.colors.bkgHighlight},
			                                       {start: retObj.subBoardListIdxes.descStart, end: retObj.subBoardListIdxes.descEnd, attrs: this.colors.descHighlight + this.colors.bkgHighlight},
			                                       {start: retObj.subBoardListIdxes.numItemsStart, end: retObj.subBoardListIdxes.numItemsEnd, attrs: this.colors.numItemsHighlight + this.colors.bkgHighlight}];
		}
		// Colors for items that have another list of items
		retObj.itemColorForItemWithSubItems = [{start: retObj.msgGrpListIdxes.markCharStart, end: retObj.msgGrpListIdxes.markCharEnd, attrs: this.colors.areaMark},
		                                            {start: retObj.msgGrpListIdxes.descStart, end: retObj.msgGrpListIdxes.descEnd, attrs: this.colors.desc},
		                                            {start: retObj.msgGrpListIdxes.numItemsStart, end: retObj.msgGrpListIdxes.numItemsEnd, attrs: this.colors.numItems}];
		retObj.selectedItemColorForItemWithSubItems = [{start: retObj.msgGrpListIdxes.markCharStart, end: retObj.msgGrpListIdxes.markCharEnd, attrs: this.colors.areaMark + this.colors.bkgHighlight},
		                                                    {start: retObj.msgGrpListIdxes.descStart, end: retObj.msgGrpListIdxes.descEnd, attrs: this.colors.descHighlight + this.colors.bkgHighlight},
		                                                    {start: retObj.msgGrpListIdxes.numItemsStart, end: retObj.msgGrpListIdxes.numItemsEnd, attrs: this.colors.numItemsHighlight + this.colors.bkgHighlight}];
	}

	// If a positive color index offset was given, then adjust the indexes in the
	// arrays
	if (typeof(pColorIdxOffset) === "number" && pColorIdxOffset > 0)
	{
		retObj.itemColorForSubBoard = AdjustMenuAttrArrayIndexes(retObj.itemColorForSubBoard, pColorIdxOffset);
		retObj.selectedItemColorForSubBoard = AdjustMenuAttrArrayIndexes(retObj.selectedItemColorForSubBoard, pColorIdxOffset);
	}

	return retObj;
}

// Bulids and returns arrays of coloring information and a printf format string for
// a sub-board for the lightbar menu
//
// Parameters:
//  pItemNumHdrWidth: The width being used for the "Item #" column header
//  pNumItemsHdrWidth: The width being used for the "# Items" column header
//  pDescriptionPaddingLen: Optional - An amount of padding used in front of the description for
//                          the traditional interface, which will be used between the mark character
//                          and the description text. This won't be added to the format string but
//                          will be used for the color/attribute indexes if provided.
//
// Return value: An object with the following parameters:
//  itemColorInfo: An array of objects with 'start', 'end', and 'attrs' properties with colors for the menu item
//  selectedItemColorInfo: An array of objects with 'start', 'end', and 'attrs' properties with selected item
//                         colors for the menu item
//  formatStr: A printf format string for the menu item
function DDMsgAreaChooser_GetSubBoardColorIndexInfoAndFormatStrForMenuItem(pItemNumHdrWidth, pNumItemsHdrWidth, pDescriptionPaddingLen)
{
	var retObj = {
		itemColorInfo: [],
		selectedItemColorInfo: [],
		formatStr: ""
	};

	var itemNumHdrWidth = pItemNumHdrWidth - 1; // Leave room for the selected mark character

	var usingLightbarInterface = this.useLightbarInterface && console.term_supports(USER_ANSI);
	if (usingLightbarInterface)
	{
		var markStart = 0;
		var markEnd = 1;
		if (this.showDatesInSubBoardList)
		{
			var dateWidth = 10;
			var timeWidth = 8;
			var dateAndTimeWidth = dateWidth + timeWidth + 1;
			var descWidth = console.screen_columns - itemNumHdrWidth - pNumItemsHdrWidth - dateWidth - timeWidth - 6;
			//if (this.scrollbarEnabled && !this.CanShowAllItemsInWindow())
			retObj.formatStr = "%" + itemNumHdrWidth + "d %-" + descWidth + "s %" + pNumItemsHdrWidth + "d %-" + dateWidth + "s %-" + timeWidth + "s";

			var itemNumStart = markEnd;
			var itemNumEnd = itemNumStart + itemNumHdrWidth + 1;
			var descStart = itemNumEnd;
			var descEnd = descStart + descWidth + 1;
			var numItemsStart = descEnd;
			var numItemsEnd = numItemsStart + pNumItemsHdrWidth + 1;
			var dateStart = numItemsEnd;
			var dateEnd = dateStart + dateWidth;
			var timeStart = dateEnd;
			var timeEnd = -1;
			retObj.itemColorInfo = [{start: markStart, end: markEnd, attrs: this.colors.areaMark},
			                        {start: itemNumStart, end: itemNumEnd, attrs: this.colors.areaNum},
			                        {start: descStart, end: descEnd, attrs: this.colors.desc},
			                        {start: numItemsStart, end: numItemsEnd, attrs: this.colors.numItems},
			                        {start: dateStart, end: dateEnd, attrs: this.colors.latestDate},
			                        {start: timeStart, end: timeEnd, attrs: this.colors.latestTime}];
			retObj.selectedItemColorInfo = [{start: markStart, end: markEnd, attrs: this.colors.areaMark + this.colors.bkgHighlight},
			                                {start: itemNumStart, end: itemNumEnd, attrs: this.colors.areaNumHighlight + this.colors.bkgHighlight},
			                                {start: descStart, end: descEnd, attrs: this.colors.descHighlight + this.colors.bkgHighlight},
			                                {start: numItemsStart, end: numItemsEnd, attrs: this.colors.numItemsHighlight + this.colors.bkgHighlight},
			                                {start: dateStart, end: dateEnd, attrs: this.colors.dateHighlight + this.colors.bkgHighlight},
			                                {start: timeStart, end: timeEnd, attrs: this.colors.timeHighlight + this.colors.bkgHighlight}];
		}
		else
		{
			// No dates in the sub-board list
			var itemNumStart = markEnd;
			var itemNumEnd = itemNumStart + itemNumHdrWidth + 1;
			var descStart = itemNumEnd;
			var descWidth = console.screen_columns - itemNumHdrWidth - pNumItemsHdrWidth - 4;
			var descEnd = descStart + descWidth + 1;
			var numItemsStart = descEnd;
			var numItemsEnd = -1;
			//if (this.scrollbarEnabled && !this.CanShowAllItemsInWindow())
			retObj.formatStr = "%" + itemNumHdrWidth + "d %-" + descWidth + "s %" + pNumItemsHdrWidth + "d";
			retObj.itemColorInfo = [{start: markStart, end: markEnd, attrs: this.colors.areaMark},
			                        {start: itemNumStart, end: itemNumEnd, attrs: this.colors.areaNum},
			                        {start: descStart, end: descEnd, attrs: this.colors.desc},
			                        {start: numItemsStart, end: numItemsEnd, attrs: this.colors.numItems}];
			retObj.selectedItemColorInfo = [{start: markStart, end: markEnd, attrs: this.colors.areaMark + this.colors.bkgHighlight},
			                                {start: itemNumStart, end: itemNumEnd, attrs: this.colors.areaNumHighlight + this.colors.bkgHighlight},
			                                {start: descStart, end: descEnd, attrs: this.colors.descHighlight + this.colors.bkgHighlight},
			                                {start: numItemsStart, end: numItemsEnd, attrs: this.colors.numItemsHighlight + this.colors.bkgHighlight}];
		}
	}
	else
	{
		// TODO
		// Using the traditional/non-lightbar interface
		var markStart = 0;
		var markEnd = 1;
		if (this.showDatesInSubBoardList)
		{
			var dateWidth = 10;
			var timeWidth = 8;
			var descWidth = console.screen_columns - itemNumHdrWidth - pNumItemsHdrWidth - dateWidth - timeWidth - 6;
			//if (this.scrollbarEnabled && !this.CanShowAllItemsInWindow())
			retObj.formatStr = "%-" + descWidth + "s %" + pNumItemsHdrWidth + "d %-" + dateWidth + "s %-" + timeWidth + "s";

			var descStart = markEnd;
			if (typeof(pDescriptionPaddingLen) === "number" && pDescriptionPaddingLen > 0)
			{
				descStart += pDescriptionPaddingLen;
				// It seems descWidth doesn't need to be adjusted here
			}
			var descEnd = descStart + descWidth + 1;
			var numItemsStart = descEnd;
			var numItemsEnd = numItemsStart + pNumItemsHdrWidth + 1;
			var dateStart = numItemsEnd;
			var dateEnd = dateStart + dateWidth;
			var timeStart = dateEnd;
			var timeEnd = -1;
			retObj.itemColorInfo = [{start: markStart, end: markEnd, attrs: this.colors.areaMark},
			                        {start: descStart, end: descEnd, attrs: this.colors.desc},
			                        {start: numItemsStart, end: numItemsEnd, attrs: this.colors.numItems},
			                        {start: dateStart, end: dateEnd, attrs: this.colors.latestDate},
			                        {start: timeStart, end: timeEnd, attrs: this.colors.latestTime}];
			retObj.selectedItemColorInfo = [{start: markStart, end: markEnd, attrs: this.colors.areaMark + this.colors.bkgHighlight},
			                                {start: descStart, end: descEnd, attrs: this.colors.descHighlight + this.colors.bkgHighlight},
			                                {start: numItemsStart, end: numItemsEnd, attrs: this.colors.numItemsHighlight + this.colors.bkgHighlight},
			                                {start: dateStart, end: dateEnd, attrs: this.colors.dateHighlight + this.colors.bkgHighlight},
			                                {start: timeStart, end: timeEnd, attrs: this.colors.timeHighlight + this.colors.bkgHighlight}];
		}
		else
		{
			// No dates in the sub-board list
			var descStart = markEnd;
			var descWidth = console.screen_columns - itemNumHdrWidth - pNumItemsHdrWidth - 4;
			var descEnd = descStart + descWidth + 1;
			var numItemsStart = descEnd;
			var numItemsEnd = -1;
			//if (this.scrollbarEnabled && !this.CanShowAllItemsInWindow())
			//retObj.formatStr = "%" + itemNumHdrWidth + "d %-" + descWidth + "s %" + pNumItemsHdrWidth + "d";
			retObj.formatStr = "%-" + descWidth + "s %" + pNumItemsHdrWidth + "d";
			// Generate the color info arrays
			if (typeof(pDescriptionPaddingLen) === "number" && pDescriptionPaddingLen > 0)
			{
				descStart += pDescriptionPaddingLen;
				// It seems descWidth doesn't need to be adjusted here
			}
			retObj.itemColorInfo = [{start: markStart, end: markEnd, attrs: this.colors.areaMark},
			                        {start: descStart, end: descEnd, attrs: this.colors.desc},
			                        {start: numItemsStart, end: numItemsEnd, attrs: this.colors.numItems}];
			retObj.selectedItemColorInfo = [{start: markStart, end: markEnd, attrs: this.colors.areaMark + this.colors.bkgHighlight},
			                                {start: descStart, end: descEnd, attrs: this.colors.descHighlight + this.colors.bkgHighlight},
			                                {start: numItemsStart, end: numItemsEnd, attrs: this.colors.numItemsHighlight + this.colors.bkgHighlight}];
		}
	}

	return retObj;
}

// Adjusts the attribute indexes in an attribute array to be used with a DDLightbarMenu
//
// Parameters:
//  pMenuAttrIdxArray: The attribute array to be adjusted
//  pAttrIdxOffset: The offset to adjust the indexes by. Must be positive.
//
// Return value: The adjusted array
function AdjustMenuAttrArrayIndexes(pMenuAttrIdxArray, pAttrIdxOffset)
{
	if (pAttrIdxOffset <= 0)
		return pMenuAttrIdxArray;

	var newArray = pMenuAttrIdxArray;
	for (var i = 0; i < newArray.length; ++i)
	{
		newArray[i].start += pAttrIdxOffset;
		if (newArray[i].end > -1)
			newArray[i].end += pAttrIdxOffset;
	}
	// To make DDLightbarMenu happy, ensure the first object in the array
	// goes from index 0 to the next one
	if (newArray.length > 0)
	{
		newArray.splice(0, 0, {
			start: 0,
			end: 0,
			attrs: "\x01n"
		});
		newArray[0].end = newArray[1].start;
	}
	return newArray;
}

///////////////////////////////////////////////
// Other functions for the msg. area chooser //
///////////////////////////////////////////////

// For the DDMsgAreaChooser class: Reads the configuration file.
function DDMsgAreaChooser_ReadConfigFile()
{
	// Use "DDMsgAreaChooser.cfg" if that exists; otherwise, use
	// "DDMsgAreaChooser.example.cfg" (the stock config file)
	var cfgFilenameBase = "DDMsgAreaChooser.cfg";
	var cfgFilename = file_cfgname(system.mods_dir, cfgFilenameBase);
	if (!file_exists(cfgFilename))
		cfgFilename = file_cfgname(system.ctrl_dir, cfgFilenameBase);
	if (!file_exists(cfgFilename))
		cfgFilename = file_cfgname(js.exec_dir, cfgFilenameBase);
	// If the configuration file hasn't been found, look to see if there's a DDMsgAreaChooser.example.cfg file
	// available in the same directory 
	if (!file_exists(cfgFilename))
	{
		var exampleFileName = file_cfgname(js.exec_dir, "DDMsgAreaChooser.example.cfg");
		if (file_exists(exampleFileName))
			cfgFilename = exampleFileName;
	}
	var cfgFile = new File(cfgFilename);
	if (cfgFile.open("r"))
	{
		var behaviorSettings = cfgFile.iniGetObject("BEHAVIOR");
		var colorSettings = cfgFile.iniGetObject("COLORS");
		cfgFile.close();
		// Behavior settings
		var hdrMaxNumLines = parseInt(behaviorSettings["areaChooserHdrMaxLines"]);
		if (!isNaN(hdrMaxNumLines) && hdrMaxNumLines > 0)
			this.areaChooserHdrMaxLines = hdrMaxNumLines;
		if (typeof(behaviorSettings["useLightbarInterface"]) === "boolean")
			this.useLightbarInterface = behaviorSettings.useLightbarInterface;
		if (typeof(behaviorSettings["showImportDates"]) === "boolean")
			this.showImportDates = behaviorSettings.showImportDates;
		if (typeof(behaviorSettings["areaChooserHdrFilenameBase"]) === "string")
			this.areaChooserHdrFilenameBase = behaviorSettings.areaChooserHdrFilenameBase;
		if (typeof(behaviorSettings["useSubCollapsing"]) === "boolean")
			this.useSubCollapsing = behaviorSettings.useSubCollapsing;
		if (typeof(behaviorSettings["subCollapseSeparator"]) === "string" && behaviorSettings["subCollapseSeparator"].length > 0)
			this.subCollapseSeparator = behaviorSettings.subCollapseSeparator;
		if (typeof(behaviorSettings["showDatesInSubBoardList"]) === "boolean")
			this.showDatesInSubBoardList = behaviorSettings.showDatesInSubBoardList;
		// Color settings
		var onlySyncAttrsRegexWholeWord = new RegExp("^[\x01krgybmcw01234567hinq,;\.dtlasz]+$", 'i');
		for (var prop in this.colors)
		{
			if (colorSettings.hasOwnProperty(prop))
			{
				// Make sure the value is a string (for attrCodeStr() etc; in some cases, such as a background attribute of 4, it will be a number)
				var value = colorSettings[prop].toString();
				// If the value doesn't have any control characters, then add the control character
				// before attribute characters
				if (!/\x01/.test(value))
					value = attrCodeStr(value);
				if (onlySyncAttrsRegexWholeWord.test(value))
					this.colors[prop] = value;
			}
		}
	}
}

// For the DDMsgAreaChooser class: Reads the user settings file
function DDMsgAreaChooser_ReadUserSettingsFile()
{
	// Open and read the user settings file, if it exists
	var userSettingsFile = new File(gUserSettingsFilename);
	if (userSettingsFile.open("r"))
	{
		// Behavior settings
		for (var settingName in this.userSettings)
		{
			this.userSettings[settingName] = userSettingsFile.iniGetValue("BEHAVIOR", settingName, this.userSettings[settingName]);
		}
		// Last chosen sub-boards
		var lastSubBoards = userSettingsFile.iniGetObject("LAST_SUBBOARDS");

		userSettingsFile.close();

		if (lastSubBoards != null)
			this.lastChosenSubBoardsPerGrpForUser = lastSubBoards;
	}
}

// For the DDMsgAreaChooser class: Writes the user settings file
function DDMsgAreaChooser_WriteUserSettingsFile()
{
	var writeSucceeded = false;
	// Open the user settings file, if it exists
	var userSettingsFile = new File(gUserSettingsFilename);
	if (userSettingsFile.open(userSettingsFile.exists ? "r+" : "w+"))
	{
		// Variables in this.userSettings are initialized in the DDMsgAreaChooser constructor. For each
		// user setting (except for twitlist), save the setting in the user's settings file. The user's
		// twit list is an array that is saved to a separate file.
		for (var settingName in this.userSettings)
		{
			userSettingsFile.iniSetValue("BEHAVIOR", settingName, this.userSettings[settingName]);
		}
		// Last chosen sub-boards
		for (var grpName in this.lastChosenSubBoardsPerGrpForUser)
		{
			userSettingsFile.iniSetValue("LAST_SUBBOARDS", grpName, this.lastChosenSubBoardsPerGrpForUser[grpName]);
		}
		userSettingsFile.close();
		writeSucceeded = true;
	}
	if (!writeSucceeded)
		log(LOG_ERR, "Failed to save user settings file: " + gUserSettingsFilename);
	return writeSucceeded;
}

// For the DDMsgAreaChooser class: Shows the help screen
//
// Parameters:
//  pLightbar: Boolean - Whether or not to show lightbar help.  If
//             false, then this function will show regular help.
//  pClearScreen: Boolean - Whether or not to clear the screen first
function DDMsgAreaChooser_ShowHelpScreen(pLightbar, pClearScreen)
{
	if (pClearScreen)
		console.clear("\x01n");
	else
		console.attributes = "N";
	console.center("\x01c\x01hDigital Distortion Message Area Chooser");
	var lineStr = "";
	for (var i = 0; i < 39; ++i)
		lineStr += HORIZONTAL_SINGLE;
	console.attributes = "HK";
	console.center(lineStr);
	console.center("\x01n\x01cVersion \x01g" + DD_MSG_AREA_CHOOSER_VERSION +
	               " \x01w\x01h(\x01b" + DD_MSG_AREA_CHOOSER_VER_DATE + "\x01w)");
	console.crlf();
	var helpText = "First, a listing of message groups is displayed.  One can be chosen by typing ";
	helpText += "its number.  Then, a listing of sub-boards within that message group will be ";
	helpText += "shown, and one can be chosen by typing its number.";
	printf("\x01n\x01c%s", lfexpand(word_wrap(helpText, console.screen_columns-1, null, false)));

	if (pLightbar)
	{
		console.crlf();
		console.print("\x01n\x01cThe lightbar interface also allows up & down navigation through the lists:");
		console.crlf();
		console.attributes = "HK";
		for (var i = 0; i < 74; ++i)
			console.print(HORIZONTAL_SINGLE);
		console.crlf();
		console.print("\x01n\x01c\x01hUp arrow\x01n\x01c: Move the cursor up one line");
		console.crlf();
		console.print("\x01hDown arrow\x01n\x01c: Move the cursor down one line");
		console.crlf();
		console.print("\x01hENTER\x01n\x01c: Select the current group/sub-board");
		console.crlf();
		console.print("\x01hHOME\x01n\x01c: Go to the first item on the screen");
		console.crlf();
		console.print("\x01hEND\x01n\x01c: Go to the last item on the screen");
		console.crlf();
		console.print("\x01hPageUp\x01n\x01c/\x01hPageDown\x01n\x01c: Go to the previous/next page");
		console.crlf();
		console.print("\x01hF\x01n\x01c/\x01hL\x01n\x01c: Go to the first/last page");
		console.crlf();
		console.print("\x01h/\x01n\x01c or \x01hCtrl-F\x01n\x01c: Find by name/description");
		console.crlf();
		console.print("\x01hN\x01n\x01c: Next search result (after a find)");
		console.crlf();
		console.print("\x01hCTRL-U\x01n\x01c: User settings (i.e., sorting)");
		console.crlf();
	}

	console.crlf();
	console.print("Additional keyboard commands:");
	console.crlf();
	console.attributes = "HK";
	for (var i = 0; i < 29; ++i)
		console.print(HORIZONTAL_SINGLE);
	console.crlf();
	console.print("\x01n\x01c\x01h?\x01n\x01c: Show this help screen");
	console.crlf();
	console.print("\x01hQ\x01n\x01c: Quit");
	console.crlf();
}

// For the DDMsgAreaChooser class: Builds sub-board printf format information
// for a message group.  The widths of the description & # messages columns
// are calculated based on the greatest number of messages in a sub-board for
// the message group.
//
// Parameters:
//  pGrpIndex: The index of the message group
function DDMsgAreaChooser_BuildSubBoardPrintfInfoForGrp(pGrpIndex)
{
	// If the array of sub-board printf strings doesn't contain the printf
	// strings for this message group, then figure out the largest number
	// of messages in the message group and add the printf strings.
	if (typeof(this.subBoardListPrintfInfo[pGrpIndex]) == "undefined")
	{
		var greatestNumMsgs = getGreatestNumMsgs(pGrpIndex);
		// Also consider items.length for collapsed sub-groups (e.g. 1000+ sub-boards in a group)
		// so the # column stays right-aligned
		if (this.msgArea_list[pGrpIndex].hasOwnProperty("items"))
		{
			var maxFromHierarchy = this.getMaxItemsCountInMsgHierarchy(this.msgArea_list[pGrpIndex]);
			if (maxFromHierarchy > greatestNumMsgs)
				greatestNumMsgs = maxFromHierarchy;
		}

		this.subBoardListPrintfInfo[pGrpIndex] = {};
		this.subBoardListPrintfInfo[pGrpIndex].numMsgsLen = Math.max(1, greatestNumMsgs.toString().length);
		var numMsgsLen = this.subBoardListPrintfInfo[pGrpIndex].numMsgsLen;
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
		this.subBoardListPrintfInfo[pGrpIndex].printfStrWithoutAreaNum = this.colors.desc + "%-"
		                                                 + this.subBoardListPrintfInfo[pGrpIndex].nameLen + "s "
		                                                 + this.colors.numItems + "%"
		                                                 + this.subBoardListPrintfInfo[pGrpIndex].numMsgsLen + "d";
		if (this.showDatesInSubBoardList)
		{
			this.subBoardListPrintfInfo[pGrpIndex].printfStr += " "
			                                                 + this.colors.latestDate + "%" + this.dateLen + "s "
			                                                 + this.colors.latestTime + "%" + this.timeLen + "s";
			this.subBoardListPrintfInfo[pGrpIndex].printfStrWithoutAreaNum += " "
			                                                 + this.colors.latestDate + "%" + this.dateLen + "s "
			                                                 + this.colors.latestTime + "%" + this.timeLen + "s";
		}
		this.subBoardListPrintfInfo[pGrpIndex].highlightPrintfStr = "\x01n" + this.colors.bkgHighlight
		                                                          + " " + "\x01n"
		                                                          + this.colors.bkgHighlight
		                                                          + this.colors.areaNumHighlight
		                                                          + "%" + this.areaNumLen + "d \x01n"
		                                                          + this.colors.bkgHighlight
		                                                          + this.colors.descHighlight + "%-"
		                                                          + this.subBoardListPrintfInfo[pGrpIndex].nameLen + "s \x01n"
		                                                          + this.colors.bkgHighlight
		                                                          + this.colors.numItemsHighlight + "%"
		                                                          + this.subBoardListPrintfInfo[pGrpIndex].numMsgsLen + "d";
		this.subBoardListPrintfInfo[pGrpIndex].highlightPrintfStrWithoutAreaNum = "\x01n" + this.colors.bkgHighlight
		                                                          + this.colors.descHighlight + "%-"
		                                                          + this.subBoardListPrintfInfo[pGrpIndex].nameLen + "s \x01n"
		                                                          + this.colors.bkgHighlight
		                                                          + this.colors.numItemsHighlight + "%"
		                                                          + this.subBoardListPrintfInfo[pGrpIndex].numMsgsLen + "d";
		if (this.showDatesInSubBoardList)
		{
			this.subBoardListPrintfInfo[pGrpIndex].highlightPrintfStr += " \x01n"
			                                                          + this.colors.bkgHighlight
			                                                          + this.colors.dateHighlight + "%" + this.dateLen + "s \x01n"
			                                                          + this.colors.bkgHighlight
			                                                          + this.colors.timeHighlight + "%" + this.timeLen + "s\x01n";
			this.subBoardListPrintfInfo[pGrpIndex].highlightPrintfStrWithoutAreaNum += " \x01n"
			                                                          + this.colors.bkgHighlight
			                                                          + this.colors.dateHighlight + "%" + this.dateLen + "s \x01n"
			                                                          + this.colors.bkgHighlight
			                                                          + this.colors.timeHighlight + "%" + this.timeLen + "s\x01n";
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
	if (console.term_supports(USER_ANSI) && (typeof(pStartScreenRow) === "number"))
	{
		// If specified to clear the rows first, then do so.
		var screenX = 1;
		var screenY = pStartScreenRow;
		var clearRowsFirst = (typeof(pClearRowsFirst) == "boolean" ? pClearRowsFirst : true);
		if (clearRowsFirst)
		{
			console.attributes = "N";
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
			//console.cleartoeol("\x01n"); // Shouldn't do this, as it resets color attributes
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
			//console.cleartoeol("\x01n"); // Shouldn't do this, as it resets color attributes
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
	console.cleartoeol("\x01n");
	console.gotoxy(1, console.screen_rows);
	console.print("\x01y\x01h" + pErrorMsg + "\x01n");
	mswait(ERROR_WAIT_MS);
	if (pRefreshHelpLine)
		this.WriteKeyHelpLine();
}

// For the DDMsgAreaChooser class: Sets up the group_list array according to
// the sub-board collapse separator
function DDMsgAreaChooser_SetUpGrpListWithCollapsedSubBoards()
{
	if (this.msgArea_list.length == 0)
	{
		// Copy some of the information from msg_area.grp_list
		for (var grpIdx = 0; grpIdx < msg_area.grp_list.length; ++grpIdx)
		{
			// Go through sub_list in the curent message group, and for
			// any that have the collapse separator, add only one copy
			// of the name to the sub_list property for this group.
			// First, we'll have to see if multiple sub-boards with
			// the collapse separator have the same prefix.
			// dirDescs is an object indexed by sub-board description,
			// and the value will be how many times it was seen.
			// First, count the number of sub-boards that have the separator.
			// If all of the group's sub-boards have the separator, then we
			// won't collapse the sub-boards.
			var numSubBoardsWithSeparator = 0;
			for (var subIdx = 0; subIdx < msg_area.grp_list[grpIdx].sub_list.length; ++subIdx)
			{
				// Check to see if the name before the separator is the same as the
				// message group name, and only count it if it's not.
				var sepIdx = msg_area.grp_list[grpIdx].sub_list[subIdx].description.indexOf(this.subCollapseSeparator);
				if (sepIdx > -1)
				{
					var subBoardDescBeforeSep = msg_area.grp_list[grpIdx].sub_list[subIdx].description.substr(0, sepIdx);
					if (subBoardDescBeforeSep != msg_area.grp_list[grpIdx].name)
						++numSubBoardsWithSeparator;
				}
			}
			// Whether or not to use sub-board collapsing for this group
			var collapseThisGroup = (numSubBoardsWithSeparator > 0 && numSubBoardsWithSeparator < msg_area.grp_list[grpIdx].sub_list.length);
			// Go through and build  the group list
			var subDescs = {};
			for (var subIdx = 0; subIdx < msg_area.grp_list[grpIdx].sub_list.length; ++subIdx)
			{
				var subBoardDesc = msg_area.grp_list[grpIdx].sub_list[subIdx].description;
				if (collapseThisGroup)
				{
					var sepIdx = msg_area.grp_list[grpIdx].sub_list[subIdx].description.indexOf(this.subCollapseSeparator);
					if (sepIdx > -1)
						subBoardDesc = truncsp(subBoardDesc.substr(0, sepIdx));  // Remove trailing whitespace
				}
				if (subDescs.hasOwnProperty(subBoardDesc))
					subDescs[subBoardDesc] += 1;
				else
					subDescs[subBoardDesc] = 1;
			}

			// Create an initial file group object for this file group (except
			// for sub_list, which we will build ourselves for this group)
			var msgGrpObj = defaultMsgGrpObj();
			for (var prop in msgGrpObj)
			{
				if (prop != "sub_list")
					msgGrpObj[prop] = msg_area.grp_list[grpIdx][prop];
			}

			// Go through the dirs in this group again.  For each sub-board:
			// If its whole description exists in subDescs, then just add it to
			// the sub_list array.  Otherwise:
			// If a collapse seprator exists, then split on that and see if the
			// prefix was seen more than once.  If so, then add the separate
			// subdirs as their own items in the sub_list array.  Otherwise,
			// add the single sub-board to the sub_list array with the whole
			// description.
			var addedPrefixDescriptionDirs = {};
			for (var subIdx = 0; subIdx < msg_area.grp_list[grpIdx].sub_list.length; ++subIdx)
			{
				var subBoardDesc = msg_area.grp_list[grpIdx].sub_list[subIdx].description;
				if (subDescs.hasOwnProperty(subBoardDesc))
					msgGrpObj.sub_list.push(subBoardObjFromOfficialSubBoard(grpIdx, subIdx));
				else
				{
					var sepIdx = msg_area.grp_list[grpIdx].sub_list[subIdx].description.indexOf(this.subCollapseSeparator);
					if (sepIdx > -1)
					{
						// subBoardDesc here will now be the shortened middle one before the separator character
						subBoardDesc = truncsp(msg_area.grp_list[grpIdx].sub_list[subIdx].description.substr(0, sepIdx));  // Remove trailing whitespace
						// If the sub-board description before the name separator character is different
						// from the message group name, then add it with sub-subboards
						if (subBoardDesc != msg_area.grp_list[grpIdx].name)
						{
							// If it has been seen more than once, then the description should
							// be the prefix description
							if (subDescs[subBoardDesc] > 1)
							{
								// Find the sub-board with a description matching subBoardDesc
								var addedSubIdx = -1; // msgGrpObj.sub_list.length - 1;
								for (var i = 0; i < msgGrpObj.sub_list.length && addedSubIdx == -1; ++i)
								{
									if (msgGrpObj.sub_list[i].description == subBoardDesc)
										addedSubIdx = i;
								}
								if (!addedPrefixDescriptionDirs.hasOwnProperty(subBoardDesc))
								{
									// Add it to sub_list
									msgGrpObj.sub_list.push(subBoardObjFromOfficialSubBoard(grpIdx, subIdx));
									addedSubIdx = msgGrpObj.sub_list.length - 1;
									msgGrpObj.sub_list[addedSubIdx].description = subBoardDesc;
									addedPrefixDescriptionDirs[subBoardDesc] = true;
								}
								// Add the sub-subboard to the sub-board's sub-subboard list
								// Using skipsp() to strip leading whitespace
								var subSubBoardDesc = skipsp(msg_area.grp_list[grpIdx].sub_list[subIdx].description.substr(sepIdx+1));
								msgGrpObj.sub_list[addedSubIdx].sub_subboard_list.push({
									description: subSubBoardDesc,
									code: msg_area.grp_list[grpIdx].sub_list[subIdx].code,
									index: msg_area.grp_list[grpIdx].sub_list[subIdx].index,
									grp_index: msg_area.grp_list[grpIdx].sub_list[subIdx].grp_index
								});
							}
							else // Add it with the full description
								msgGrpObj.sub_list.push(subBoardObjFromOfficialSubBoard(grpIdx, subIdx));
						}
						else
						{
							// The sub-board name before the name separator character is the same as the message
							// group name. Add it as its own single sub-board.
							msgGrpObj.sub_list.push(subBoardObjFromOfficialSubBoard(grpIdx, subIdx));
						}
					}
					if (subDescs.hasOwnProperty(subBoardDesc))
						subDescs[subDescs] += 1;
					else
						subDescs[subDescs] = 1;
				}
			}

			this.msgArea_list.push(msgGrpObj);
		}
	}
}

// For the DDMsgAreaChooser class:  Lets the user manage their preferences/settings (scrollable/ANSI user interface).
//
// Return value: An object containing the following properties:
//               needWholeScreenRefresh: Boolean - Whether or not the whole screen needs to be
//                                       refreshed (i.e., when the user has edited their twitlist)
//               optionBoxTopLeftX: The top-left screen column of the option box
//               optionBoxTopLeftY: The top-left screen row of the option box
//               optionBoxWidth: The width of the option box
//               optionBoxHeight: The height of the option box
function DDMsgAreaChooser_DoUserSettings_Scrollable()
{
	var retObj = {
		needWholeScreenRefresh: false,
		optionBoxTopLeftX: 1,
		optionBoxTopLeftY: 1,
		optionBoxWidth: 0,
		optionBoxHeight: 0
	};

	if (!this.useLightbarInterface || !console.term_supports(USER_ANSI))
	{
		this.DoUserSettings_Traditional();
		return retObj;
	}

	// Save the user's current settings so that we can check them later to see if any
	// of them changed, in order to determine whether to save the user's settings file.
	var originalSettings = {};
	for (var prop in this.userSettings)
	{
		if (this.userSettings.hasOwnProperty(prop))
			originalSettings[prop] = this.userSettings[prop];
	}


	// Create the user settings box
	var optBoxTitle = "Setting                                      Enabled";
	var optBoxWidth = ChoiceScrollbox_MinWidth();
	var optBoxHeight = 6;
	var optBoxTopRow = 3;
	var optBoxStartX = Math.floor((console.screen_columns/2) - (optBoxWidth/2));
	++optBoxStartX; // Looks better
	var optionBox = new ChoiceScrollbox(optBoxStartX, optBoxTopRow, optBoxWidth, optBoxHeight, optBoxTitle,
										null/*gConfigSettings*/, false, true);
	optionBox.addInputLoopExitKey(CTRL_U);
	// Update the bottom help text to be more specific to the user settings box
	var bottomBorderText = "\x01n\x01h\x01cUp\x01b, \x01cDn\x01b, \x01cEnter\x01y=\x01bSelect\x01n\x01c/\x01h\x01btoggle, "
	                     + "\x01cESC\x01n\x01c/\x01hQ\x01n\x01c/\x01hCtrl-U\x01y=\x01bClose";
	// This one contains the page navigation keys..  Don't really need to show those,
	// since the settings box only has one page right now:
	/*var bottomBorderText = "\x01n\x01h\x01cUp\x01b, \x01cDn\x01b, \x01cN\x01y)\x01bext, \x01cP\x01y)\x01brev, "
						   + "\x01cF\x01y)\x01birst, \x01cL\x01y)\x01bast, \x01cEnter\x01y=\x01bSelect, "
						   + "\x01cESC\x01n\x01c/\x01hQ\x01n\x01c/\x01hCtrl-U\x01y=\x01bClose";*/

	optionBox.setBottomBorderText(bottomBorderText, true, false);

	// Add the options to the option box
	const checkIdx = 48;
	const optionFormatStr = "%-" + (checkIdx-1) + "s[ ]";

	// When changing to a different message group, whether to remember/use
	// the last sub-board in each message group as the currently selected
	// sub-board
	const CHG_GRP_REMEMBER_SUB_BOARD_OPT_INDEX = optionBox.addTextItem(format(optionFormatStr, "Remember sub-boards when changing groups"));
	if (this.userSettings.rememberLastSubBoardWhenChangingGrp)
		optionBox.chgCharInTextItem(CHG_GRP_REMEMBER_SUB_BOARD_OPT_INDEX, checkIdx, CP437_CHECK_MARK);

	// Create an object containing toggle values (true/false) for each option index
	var optionToggles = {};
	optionToggles[CHG_GRP_REMEMBER_SUB_BOARD_OPT_INDEX] = this.userSettings.rememberLastSubBoardWhenChangingGrp;

	// Other options
	// Sorting option
	var SUB_BOARD_CHANGE_SORTING_OPT_INDEX = optionBox.addTextItem("Sorting");

	// Set up the enter key in the box to toggle the selected item.
	optionBox.areaChooserObj = this;
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
					case CHG_GRP_REMEMBER_SUB_BOARD_OPT_INDEX:
						this.areaChooserObj.userSettings.rememberLastSubBoardWhenChangingGrp = !this.areaChooserObj.userSettings.rememberLastSubBoardWhenChangingGrp;
						break;
					default:
						break;
				}
			}
			else
			{
				switch (itemIndex)
				{
					case SUB_BOARD_CHANGE_SORTING_OPT_INDEX:
						var sortOptMenu = CreateSubBoardChangeSortOptMenu(optBoxStartX, optBoxTopRow, optBoxWidth, optBoxHeight, this.areaChooserObj.userSettings.areaChangeSorting);
						var chosenSortOpt = sortOptMenu.GetVal();
						console.attributes = "N";
						if (typeof(chosenSortOpt) === "number")
							this.areaChooserObj.userSettings.areaChangeSorting = chosenSortOpt;
						retObj.needWholeScreenRefresh = false;
						this.drawBorder();
						this.drawInnerMenu(SUB_BOARD_CHANGE_SORTING_OPT_INDEX);
						break;
					default:
						break;
				}
			}
		}
	}); // Option box enter key override function

	// Display the option box and have it do its input loop
	var boxRetObj = optionBox.doInputLoop(true);

	// If the user changed any of their settings, then save the user settings.
	// If the save fails, then output an error message.
	var settingsChanged = false;
	for (var prop in this.userSettings)
	{
		if (this.userSettings.hasOwnProperty(prop))
		{
			settingsChanged = settingsChanged || (originalSettings[prop] != this.userSettings[prop]);
			if (settingsChanged)
				break;
		}
	}
	if (settingsChanged)
	{
		if (!this.WriteUserSettingsFile())
		{
			writeWithPause(1, console.screen_rows, "\x01n\x01y\x01hFailed to save settings!\x01n", ERROR_PAUSE_WAIT_MS, "\x01n", true);
			// Refresh the bottom row help line
			this.WriteKeyHelpLine();
		}
	}

	optionBox.addInputLoopExitKey(CTRL_U);

	// Prepare return object values and return
	retObj.optionBoxTopLeftX = optionBox.dimensions.topLeftX;
	retObj.optionBoxTopLeftY = optionBox.dimensions.topLeftY;
	retObj.optionBoxWidth = optionBox.dimensions.width;
	retObj.optionBoxHeight = optionBox.dimensions.height;
	return retObj;
}

// For the DDMsgAreaChooser class: Lets the user change their settings (traditional user interface)
function DDMsgAreaChooser_DoUserSettings_Traditional()
{
	var optNum = 1;
	// When changing to a different message group, whether to remember/use
	// the last sub-board in each message group as the currently selected
	// sub-board
	var CHG_GRP_REMEMBER_SUB_BOARD_OPT_INDEX = optNum++;
	// Sorting for sub-boards
	var SUB_BOARD_CHANGE_SORTING_OPT_NUM = optNum++;
	var HIGHEST_CHOICE_NUM = SUB_BOARD_CHANGE_SORTING_OPT_NUM; // Highest choice number

	console.crlf();
	var wordFirstCharAttrs = "\x01c\x01h";
	var wordRemainingAttrs = "\x01c";
	console.print(colorFirstCharAndRemainingCharsInWords("User Settings", wordFirstCharAttrs, wordRemainingAttrs) + "\r\n");
	printTradUserSettingOption(CHG_GRP_REMEMBER_SUB_BOARD_OPT_INDEX, "Remember sub-boards when changing groups", wordFirstCharAttrs, wordRemainingAttrs);
	printTradUserSettingOption(SUB_BOARD_CHANGE_SORTING_OPT_NUM, "Sorting", wordFirstCharAttrs, wordRemainingAttrs);
	console.crlf();
	console.print("\x01cYour choice (\x01hQ\x01n\x01c: Quit)\x01h: \x01g");
	var userChoiceNum = console.getnum(HIGHEST_CHOICE_NUM);
	console.attributes = "N";
	var userChoiceStr = userChoiceNum.toString().toUpperCase();
	if (userChoiceStr.length == 0 || userChoiceStr == "Q")
		return retObj;

	var userSettingsChanged = false;
	switch (userChoiceNum)
	{
		case CHG_GRP_REMEMBER_SUB_BOARD_OPT_INDEX:
			var oldChgGrpRememberSubBoardSetting = this.userSettings.rememberLastSubBoardWhenChangingGrp;
			this.userSettings.rememberLastSubBoardWhenChangingGrp = !console.noyes("Remember sub-boards when changing groups");
			userSettingsChanged = (this.userSettings.rememberLastSubBoardWhenChangingGrp != oldChgGrpRememberSubBoardSetting);
			break;
		case SUB_BOARD_CHANGE_SORTING_OPT_NUM:
			console.attributes = "N";
			console.crlf();
			console.print("\x01cChoose a sorting option (\x01hQ\x01n\x01c to quit)");
			console.crlf();
			var sortOptMenu = CreateSubBoardChangeSortOptMenu(1, 1, console.screen_columns, console.screen_rows, this.userSettings.areaChangeSorting);
			sortOptMenu.numberedMode = true;
			sortOptMenu.allowANSI = false;
			var chosenSortOpt = sortOptMenu.GetVal();
			if (typeof(chosenSortOpt) === "number")
			{
				this.userSettings.areaChangeSorting = chosenSortOpt;
				userSettingsChanged = true;
				console.print("\x01n\x01cYou chose\x01g\x01h: \x01c");
				switch (chosenSortOpt)
				{
					case SUB_BOARD_SORT_NONE:
						console.print("None");
						break;
					case SUB_BOARD_SORT_ALPHABETICAL:
						console.print("Alphabetical");
						break;
					case SUB_BOARD_SORT_LATEST_MSG_DATE_OLDEST_FIRST:
						console.print("Message date (oldest first)");
						break;
					case SUB_BOARD_SORT_LATEST_MSG_DATE_NEWEST_FIRST:
						console.print("Message date (newest first)");
						break;
				}
				console.crlf();
			}
			else
			{
				console.putmsg(bbs.text(Aborted), P_SAVEATR);
				console.pause();
			}
			console.attributes = "N";
			break;
	}

	// If any user settings changed, then write them to the user settings file
	if (userSettingsChanged)
	{
		if (!this.WriteUserSettingsFile())
		{
			console.print("\x01n\r\n\x01y\x01hFailed to save settings!\x01n");
			console.crlf();
			console.pause();
		}
	}
}

// Helper function for user settings: Creates the menu object to let the user choose
// a sub-board change sorting option, and returns the object
function CreateSubBoardChangeSortOptMenu(pX, pY, pWidth, pHeight, pCurrentSortSetting)
{
	var sortOptMenu = new DDLightbarMenu(pX, pY, pWidth, pHeight);
	sortOptMenu.AddAdditionalQuitKeys("qQ");
	sortOptMenu.borderEnabled = true;
	sortOptMenu.colors.borderColor = "\x01n\x01b";
	sortOptMenu.borderChars = {
		upperLeft: CP437_BOX_DRAWINGS_UPPER_LEFT_DOUBLE,
		upperRight: CP437_BOX_DRAWINGS_UPPER_RIGHT_DOUBLE,
		lowerLeft: CP437_BOX_DRAWINGS_LOWER_LEFT_DOUBLE,
		lowerRight: CP437_BOX_DRAWINGS_LOWER_RIGHT_DOUBLE,
		top: CP437_BOX_DRAWINGS_HORIZONTAL_DOUBLE,
		bottom: CP437_BOX_DRAWINGS_HORIZONTAL_DOUBLE,
		left: CP437_BOX_DRAWINGS_DOUBLE_VERTICAL,
		right: CP437_BOX_DRAWINGS_DOUBLE_VERTICAL
	};
	sortOptMenu.topBorderText = "Sub-board change sorting";
	sortOptMenu.Add("None", SUB_BOARD_SORT_NONE);
	sortOptMenu.Add("Alphabetical", SUB_BOARD_SORT_ALPHABETICAL);
	sortOptMenu.Add("Msg date: Oldest first", SUB_BOARD_SORT_LATEST_MSG_DATE_OLDEST_FIRST);
	sortOptMenu.Add("Msg date: Newest first", SUB_BOARD_SORT_LATEST_MSG_DATE_NEWEST_FIRST);
	switch (pCurrentSortSetting)
	{
		case SUB_BOARD_SORT_NONE:
			sortOptMenu.selectedItemIdx = 0;
			break;
		case SUB_BOARD_SORT_ALPHABETICAL:
			sortOptMenu.selectedItemIdx = 1;
			break;
		case SUB_BOARD_SORT_LATEST_MSG_DATE_OLDEST_FIRST:
			sortOptMenu.selectedItemIdx = 2;
			break;
		case SUB_BOARD_SORT_LATEST_MSG_DATE_NEWEST_FIRST:
			sortOptMenu.selectedItemIdx = 3;
			break;
	}

	sortOptMenu.colors.itemColor = "\x01n\x01c\x01h";

	// For use in numbered mode for the traditional UI:
	sortOptMenu.colors.itemNumColor = "\x01n\x01c";
	sortOptMenu.colors.highlightedItemNumColor = "\x01n\x01g\x01h";

	return sortOptMenu;
}

/////////////////////////////
// Misc. functions

// Returns a default object for a message group
function defaultMsgGrpObj()
{
	return {
		index: 0,
		number: 0,
		name: "",
		description: "",
		ars: 0,
		sub_list: []
	};
}

// Returns a default object for a message sub-board.  It can potentially
// contain its own list of sub-boards within the original subboard.
function defaultSubBoardObj()
{
	return {
		index: 0,
		number: 0,
		grp_index: 0,
		grp_number: 0,
		grp_name: 0,
		code: "",
		name: "",
		grp_name: "",
		description: "",
		ars: 0,
		settings: 0,
		sub_subboard_list: []
	};
}
function subBoardObjFromOfficialSubBoard(pGrpIdx, pSubIdx)
{
	var subItemObj = defaultSubBoardObj();
	for (var prop in subItemObj)
	{
		if (prop != "sub_subboard_list" && msg_area.grp_list[pGrpIdx].sub_list[pSubIdx].hasOwnProperty(prop)) // Doesn't exist in the official dir objects
			subItemObj[prop] = msg_area.grp_list[pGrpIdx].sub_list[pSubIdx][prop];
	}
	return subItemObj;
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
// For the DDMsgAreaChooser class: Recursively finds the maximum items.length
// in the message area hierarchy (for collapsed sub-groups).  Used so numMsgsLen
// accommodates all displayed values for right-alignment.
function DDMsgAreaChooser_getMaxItemsCountInMsgHierarchy(pNode)
{
	if (!pNode)
		return 0;
	var max = 0;
	if (pNode.hasOwnProperty("items"))
	{
		if (pNode.items.length > max)
			max = pNode.items.length;
		for (var i = 0; i < pNode.items.length; ++i)
		{
			var subMax = this.getMaxItemsCountInMsgHierarchy(pNode.items[i]);
			if (subMax > max)
				max = subMax;
		}
	}
	return max;
}

// Parameters:
//  pGrpIndex: The index of the message group
//
// Returns: The greatest number of messages of all sub-boards within
//          the message group
function getGreatestNumMsgs(pGrpIndex)
{
	// Sanity checking
	if (typeof(pGrpIndex) !== "number")
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
			var numMsgs = numReadableMsgs(msgBase, msg_area.grp_list[pGrpIndex].sub_list[subIndex].code);
			if (numMsgs > greatestNumMsgs)
				greatestNumMsgs = numMsgs;
			msgBase.close();
		}
	}
	return greatestNumMsgs;
}

// Returns the number of messages in a sub-board
//
// Parameters:
//  pSubCode: The internal code of the sub-board
//
// Return value: The number of messages in the sub-board, or 0 if can't be opened
function getNumMsgsInSubBoard(pSubCode)
{
	if (typeof(pSubCode) !== "string")
		return 0;

	var numMsgs = 0;
	var msgBase = new MsgBase(pSubCode);
	if (msgBase != null && msgBase.open())
	{
		numMsgs = msgBase.total_msgs;
		msgBase.close();
	}
	delete msgBase; // Free some memory?
	return numMsgs;
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
		return [];

	var maxNumLines = (typeof(pMaxNumLines) === "number" ? pMaxNumLines : -1);

	var txtFileLines = [];
	// See if there is a header file that is made for the user's terminal
	// width (areaChgHeader-<width>.ans/asc).  If not, then just go with
	// msgHeader.ans/asc.
	var txtFileExists = true;
	var txtFilenameFullPath = js.exec_dir + pFilenameBase;
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
					// bbs.exec(cmdLine, EX_NATIVE, js.exec_dir) could be used to
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
				// bbs.exec(cmdLine, EX_NATIVE, js.exec_dir) could be used to
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
// header and the sub-board code. This also checks pMsgHdrOrIdx for
// null; if it's null, this function will return false.
//
// Parameters:
//  pMsgHdrOrIdx: The header or index object for the message
//  pSubBoardCode: The internal code for the sub-board the message is in
//
// Return value: Boolean - Whether or not the message is readable for the user
function isReadableMsgHdr(pMsgHdrOrIdx, pSubBoardCode)
{
	if (pMsgHdrOrIdx === null)
		return false;
	// Let the sysop see unvalidated messages and private messages but not other users.
	if (!user.is_sysop)
	{
		if (pSubBoardCode != "mail")
		{
			if ((msg_area.sub[pSubBoardCode].is_moderated && ((pMsgHdrOrIdx.attr & MSG_VALIDATED) == 0)) ||
			    (((pMsgHdrOrIdx.attr & MSG_PRIVATE) == MSG_PRIVATE) && !userHandleAliasNameMatch(pMsgHdrOrIdx.to)))
			{
				return false;
			}
		}
	}
	// If the message is deleted, determine whether it should be viewable, based
	// on the system settings.
	if (((pMsgHdrOrIdx.attr & MSG_DELETE) == MSG_DELETE) && !canViewDeletedMsgs())
		return false;
	// The message voting and poll variables were added in sbbsdefs.js for
	// Synchronet 3.17.  Make sure they're defined before referencing them.
	if (typeof(MSG_UPVOTE) != "undefined")
	{
		if ((pMsgHdrOrIdx.attr & MSG_UPVOTE) == MSG_UPVOTE)
			return false;
	}
	if (typeof(MSG_DOWNVOTE) != "undefined")
	{
		if ((pMsgHdrOrIdx.attr & MSG_DOWNVOTE) == MSG_DOWNVOTE)
			return false;
	}
	// Don't include polls as being unreadable messages - They just need to have
	// their answer selections read from the header instead of the message body
	/*
	if (typeof(MSG_POLL) != "undefined")
	{
		if ((pMsgHdrOrIdx.attr & MSG_POLL) == MSG_POLL)
			return false;
	}
	*/
	return true;
}

// Returns whether the logged-in user can view deleted messages.
function canViewDeletedMsgs()
{
	var usersVDM = ((system.settings & SYS_USRVDELM) == SYS_USRVDELM);
	var sysopVDM = ((system.settings & SYS_SYSVDELM) == SYS_SYSVDELM);
	return (usersVDM || (user.is_sysop && sysopVDM));
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
	// The posts property in msg_area.sub[sub_code] and msg_area.grp_list.sub_list is the number
	// of posts excluding vote posts
	if (typeof(msg_area.sub[pSubBoardCode].posts) === "number")
		return msg_area.sub[pSubBoardCode].posts;
	else if ((pMsgbase !== null) && pMsgbase.is_open)
	{
		// Just return the total number of messages..  This isn't accurate, but it's fast.
		return pMsgbase.total_msgs;
	}
	else if (pMsgbase === null)
	{
		var numMsgs = 0;
		var msgBase = new MsgBase(pSubBoardCode);
		if (msgBase.open())
		{
			// Just return the total number of messages..  This isn't accurate, but it's fast.
			numMsgs = msgBase.total_msgs;
			msgBase.close();
		}
		return numMsgs;
	}
	else
		return 0;

	// Older code, used before Synchronet 3.18c when the 'posts' property was added - This is slow:
	/*
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
	*/
}

// Returns whether a given name or CRC16 value matches the logged-in user's
// handle, alias, or name.
//
// Parameters:
//  pNameOrCRC16: A name (string) to match against the logged-in user, or a CRC16 (number)
//                to match against the logged-in user
//
// Return value: Boolean - Whether or not the given name matches the logged-in
//               user's handle, alias, or name
function userHandleAliasNameMatch(pNameOrCRC16)
{
	var checkByCRC16 = (typeof(pNameOrCRC16) === "number");
	if (!checkByCRC16 && typeof(pNameOrCRC16) !== "string")
		return false;

	var userMatch = false;
	if (checkByCRC16)
	{
		if (user.handle.length > 0)
		{
			if (userHandleAliasNameMatch.userHandleCRC16 === undefined)
				userHandleAliasNameMatch.userHandleCRC16 = crc16_calc(user.handle.toLowerCase());
			userMatch = (userHandleAliasNameMatch.userHandleCRC16 == pNameOrCRC16);
		}
		if (!userMatch && (user.alias.length > 0))
		{
			if (userHandleAliasNameMatch.userAliasCRC16 === undefined)
				userHandleAliasNameMatch.userAliasCRC16 = crc16_calc(user.alias.toLowerCase());
			userMatch = (userHandleAliasNameMatch.userAliasCRC16 == pNameOrCRC16);
		}
		if (!userMatch && (user.name.length > 0))
		{
			if (userHandleAliasNameMatch.userNameCRC16 === undefined)
				userHandleAliasNameMatch.userNameCRC16 = crc16_calc(user.name.toLowerCase());
			userMatch = (userHandleAliasNameMatch.userNameCRC16 == pNameOrCRC16);
		}
	}
	else
	{
		if (pNameOrCRC16 != "")
		{
			var nameUpper = pNameOrCRC16.toUpperCase();
			// If the name starts & ends with the same quote character, then remove the
			// quote characters.
			var firstChar = nameUpper.charAt(0);
			var lastChar = nameUpper.charAt(nameUpper.length-1);
			if ((firstChar == "\"" && lastChar == "\"") ||(firstChar == "'" && lastChar == "'"))
				nameUpper = nameUpper.substring(1, nameUpper.length-1);
			
			if (user.handle.length > 0)
			{
				if (userHandleAliasNameMatch.userHandleUpper === undefined)
					userHandleAliasNameMatch.userHandleUpper = user.handle.toUpperCase();
				userMatch = (nameUpper.indexOf(userHandleAliasNameMatch.userHandleUpper) > -1);
			}
			if (!userMatch && (user.alias.length > 0))
			{
				if (userHandleAliasNameMatch.userAliasUpper === undefined)
					userHandleAliasNameMatch.userAliasUpper = user.alias.toUpperCase();
				userMatch = (nameUpper.indexOf(userHandleAliasNameMatch.userAliasUpper) > -1);
			}
			if (!userMatch && (user.name.length > 0))
			{
				if (userHandleAliasNameMatch.userNameUpper === undefined)
					userHandleAliasNameMatch.userNameUpper = user.name.toUpperCase();
				userMatch = (nameUpper.indexOf(userHandleAliasNameMatch.userNameUpper) > -1);
			}
		}
	}
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
	if (typeof(pMode) === "number")
		mode = pMode;
	var maxWidth = 0;
	if (typeof(pMaxLength) === "number")
			maxWidth = pMaxLength;
	var timeout = 0;
	if (typeof(pTimeout) === "number")
		timeout = pTimeout;

	var setNormalAttrAtEnd = false;
	if (((mode & K_LINE) == K_LINE) && (maxWidth > 0) && console.term_supports(USER_ANSI))
	{
		var curPos = console.getxy();
		printf("\x01n\x01w\x01h\x01" + "4%" + maxWidth + "s", "");
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
		console.attributes = "N";

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
	if ((typeof(pText) != "string") || (typeof(pNumItemsPerPage) !== "number") || (typeof(pSubBoard) != "boolean"))
		return retObj;

	// Convert the text to uppercase for case-insensitive searching
	var srchText = pText.toUpperCase();
	if (pSubBoard)
	{
		if ((typeof(pGrpIdx) === "number") && (pGrpIdx >= 0) && (pGrpIdx < msg_area.grp_list.length))
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
//
// Return value: The message header of the latest readable message.  If
//               none is found, this will be null.
function getLatestMsgHdr(pSubCode)
{
	var msgHdr = null;
	var msgBase = new MsgBase(pSubCode);
	if (msgBase.open())
	{
		msgHdr = getLatestMsgHdrWithMsgbase(msgBase, pSubCode);
		msgBase.close();
	}
	delete msgBase; // Free some memory?
	return msgHdr;
}
// Gets the header of the latest readable message in a sub-board,
// given a number of messages to look at.
//
// Paramters:
//  pMsgbase: A MsgBase object for the sub-board, already opened
//  pSubCode: The internal code of the sub-board
//
// Return value: The message header of the latest readable message.  If
//               none is found, this will be null.
function getLatestMsgHdrWithMsgbase(pMsgbase, pSubCode)
{
	if (typeof(pMsgbase) !== "object")
		return null;
	if (!pMsgbase.is_open)
		return null;

	// Look through the message headers to find the latest readable one
	var msgHdrToReturn = null;
	var msgIdx = pMsgbase.total_msgs-1;
	var msgHeader = pMsgbase.get_msg_index(true, msgIdx, false);
	while (!isReadableMsgHdr(msgHeader, pSubCode) && (msgIdx >= 0))
		msgHeader = pMsgbase.get_msg_index(true, --msgIdx, true);
	if (msgHeader != null)
		msgHdrToReturn = pMsgbase.get_msg_header(true, msgIdx, false);
	return msgHdrToReturn;
}

// Gets the time of the latest post in a sub-board.
//
// Parameters:
//  pSubCode: The internal code of the sub-board
//
// Return value: The time of the latest post in the sub-board.  If pSubCode is invalid, this will be 0.
function getLatestMsgTime(pSubCode)
{
	if (typeof(pSubCode) !== "string")
		return 0;

	var latestPostTime = 0;

	var msgBase = new MsgBase(pSubCode);
	if (msgBase.open())
	{
		var msgIdx = msgBase.total_msgs-1;
		var msgHeader = msgBase.get_msg_index(true, msgIdx, false);
		while (!isReadableMsgHdr(msgHeader, pSubCode) && (msgIdx >= 0))
			msgHeader = msgBase.get_msg_index(true, --msgIdx, true);
		if (msgHeader != null)
		{
			msgHeader = msgBase.get_msg_header(true, msgIdx, false);
			var msgWrittenLocalBBSTime = msgWrittenTimeToLocalBBSTime(msgHeader);
			latestPostTime = msgWrittenLocalBBSTime != -1 ? msgWrittenLocalBBSTime : msgHeader.when_written_time;
		}

		msgBase.close();
	}

	return latestPostTime;
}

// Finds a message area index with search text, matching either the name or
// description, case-insensitive.
//
// Parameters:
//  pMsgAreaStructure: The message area structure from this.msgArea_list to search through
//  pSearchText: The name/description text to look for
//  pStartItemIdx: The item index to start at.  Defaults to 0
//
// Return value: The index of the message area, or -1 if not found
function DDMsgAreaChooser_FindMsgAreaIdxFromText(pMsgAreaStructure, pSearchText, pStartItemIdx)
{
	if (typeof(pSearchText) != "string")
		return -1;

	var areaIdx = -1;

	var startIdx = (typeof(pStartItemIdx) === "number" ? pStartItemIdx : 0);
	if ((startIdx < 0) || (startIdx > pMsgAreaStructure.length))
		startIdx = 0;

	// Go through the message area list and look for a match
	var searchTextUpper = pSearchText.toUpperCase();
	for (var i = startIdx; i < pMsgAreaStructure.length; ++i)
	{
		if (pMsgAreaStructure[i].name.toUpperCase().indexOf(searchTextUpper) > -1 || pMsgAreaStructure[i].altName.toUpperCase().indexOf(searchTextUpper) > -1)
		{
			areaIdx = i;
			break;
		}
	}

	return areaIdx;
}

// For the DDMsgAreaChooser class: Gets the lengths for the sub-board column and # of messages
// column for a sub-board.  Gets defaults if that information isn't available.
//
// Parameters:
//  pGrpIdx: The message group index
//
// Return value: An object containing the following properties:
//                      nameLen: The name field lengh
//                      numMsgsLen: The # messages field length
function DDMsgAreaChooser_GetSubNameLenAndNumMsgsLen(pGrpIdx)
{
	var retObj = {
		nameLen: console.screen_columns - this.areaNumLen - this.numItemsLen - 5,
		numMsgsLen: this.numItemsLen
	};

	if (typeof(pGrpIdx) === "number" && pGrpIdx >= 0 && pGrpIdx < msg_area.grp_list.length)
	{
		if (typeof(this.subBoardListPrintfInfo[pGrpIdx]) === "object")
		{
			if (this.subBoardListPrintfInfo[pGrpIdx].hasOwnProperty("nameLen"))
				retObj.nameLen = +(this.subBoardListPrintfInfo[pGrpIdx].nameLen);
			if (this.subBoardListPrintfInfo[pGrpIdx].hasOwnProperty("numMsgsLen"))
				retObj.numMsgsLen = +(this.subBoardListPrintfInfo[pGrpIdx].numMsgsLen);
		}
	}
	return retObj;
}


////////////////////////////////////////////////////////////////////////////
// Helper functions

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

// Finds the index of a message group AFTER the given group index which contains
// sub-boards.  If there are none, this will return -1.
//
// Parameters:
//  pGrpIdx: An index of a message group; this function will start searching AFTER this one
//
// Return value: An index of a message group after the given group index that contains sub-boards,
//               or -1 if there are none
function findNextGrpIdxWithSubBoards(pGrpIdx)
{
	if (typeof(pGrpIdx) !== "number")
		return -1;
	var nextGrpIdx = -1;
	if (pGrpIdx < msg_area.grp_list.length - 1)
	{
		for (var i = pGrpIdx + 1; i < msg_area.grp_list.length && nextGrpIdx == -1; ++i)
		{
			if (msg_area.grp_list[i].sub_list.length > 0)
				nextGrpIdx = i;
		}
	}
	if (nextGrpIdx == -1 && pGrpIdx > 0)
	{
		for (var i = 0; i < pGrpIdx && nextGrpIdx == -1; ++i)
		{
			if (msg_area.grp_list[i].sub_list.length > 0)
				nextGrpIdx = i;
		}
	}
	return nextGrpIdx;
}

// Given a message group/sub-board heirarchy object built by this module, this
// function returns whether it contains the user's currently selected message
// sub-board.
//
// Parameters:
//  pMsgSubHeirarchyObj: An object from this.msgArea_list, which is
//                       set up with a 'name' property and either
//                       an 'items' property if it has sub-items
//                       or a 'subItemObj' property if it's a message
//                       sub-board
//  pSubCodeMatchOverride: Optional - If known, this is an internal code of a
//                         sub-board to match (other than bbs.cursub_code)
//  pLastChosenSubBoardsPerGrpForUser: Optional - An object where the keys are the
//                                     message group names and the values are the
//                                     user's last chosen sub-board for each group.
//
// Return value: Whether or not the given structure has the user's currently selected message sub-board
function msgAreaStructureHasCurrentUserSubBoard(pMsgSubHeirarchyObj, pSubCodeMatchOverride, pLastChosenSubBoardsPerGrpForUser)
{
	var currentUserSubBoardFound = false;
	if (Array.isArray(pMsgSubHeirarchyObj))
	{
		// This could be the top-level array or one of the 'items' properties, which is an array.
		// Go through the array and call this function again recursively; this function will
		// return when we get to an actual sub-board that is the user's current selection.
		for (var i = 0; i < pMsgSubHeirarchyObj.length && !currentUserSubBoardFound; ++i)
			currentUserSubBoardFound = msgAreaStructureHasCurrentUserSubBoard(pMsgSubHeirarchyObj[i], pSubCodeMatchOverride, pLastChosenSubBoardsPerGrpForUser);
	}
	else
	{
		// This is one of the objects with 'name' and an 'items' or 'subItemObj'
		if (pMsgSubHeirarchyObj.hasOwnProperty("subItemObj"))
		{
			//currentUserSubBoardFound = (bbs.cursub_code == pMsgSubHeirarchyObj.subItemObj.code);
			var subCodeToLookFor = bbs.cursub_code;
			if (typeof(pSubCodeMatchOverride) === "string" && pSubCodeMatchOverride.length > 0)
				subCodeToLookFor = pSubCodeMatchOverride;
			currentUserSubBoardFound = (subCodeToLookFor == pMsgSubHeirarchyObj.subItemObj.code);
		}
		else if (pMsgSubHeirarchyObj.hasOwnProperty("items"))
		{
			var subCodeToLookFor = null;
			if (typeof(pSubCodeMatchOverride) === "string" && pSubCodeMatchOverride.length > 0)
				subCodeToLookFor = pSubCodeMatchOverride;
			else if (pLastChosenSubBoardsPerGrpForUser != null && typeof(pLastChosenSubBoardsPerGrpForUser) === "object" && pMsgSubHeirarchyObj.hasOwnProperty("name"))
			{
				if (pLastChosenSubBoardsPerGrpForUser.hasOwnProperty(pMsgSubHeirarchyObj.name))
					subCodeToLookFor = pLastChosenSubBoardsPerGrpForUser[pMsgSubHeirarchyObj.name];
			}
			currentUserSubBoardFound = msgAreaStructureHasCurrentUserSubBoard(pMsgSubHeirarchyObj.items, subCodeToLookFor, pLastChosenSubBoardsPerGrpForUser);
		}
	}
	return currentUserSubBoardFound;
}

// Parses command-line arguments
function parseCmdLineArgs(argv)
{
	var retObj = {
		chooseMsgGrpOnStartup: true,
		executeThisScript: true
	};

	// 1st command-line argument: Whether or not to choose a message group first (if
	// false, then only choose a sub-board within the user's current group).  This
	// can be true or false.
	if (typeof(argv[0]) == "boolean")
		retObj.chooseMsgGrpOnStartup = argv[0];
	else if (typeof(argv[0]) == "string" && argv[0].length > 0 && argv[0][0] != "-")
		retObj.chooseMsgGrpOnStartup = (argv[0].toLowerCase() == "true");

	// 2nd command-line argument: Determine whether or not to execute the message listing
	// code (true/false)
	if (typeof(argv[1]) == "boolean")
		retObj.executeThisScript = argv[1];
	else if (typeof(argv[1]) == "string" && argv[1].length > 0 && argv[1][0] != "-")
		retObj.executeThisScript = (argv[1].toLowerCase() == "true");

	return retObj;
}

// Converts a short sort option string to its numeric sort option value
function sortOptToValue(pSortOptStr)
{
	var sortOptVal = SUB_BOARD_SORT_NONE;
	const optValStr = pSortOptStr.toLowerCase();
	if (optValStr == "none")
		sortOptVal = SUB_BOARD_SORT_NONE;
	else if (optValStr == "alphabetical")
		sortOptVal = SUB_BOARD_SORT_ALPHABETICAL;
	else if (optValStr == "msg_date_oldest_first")
		sortOptVal = SUB_BOARD_SORT_LATEST_MSG_DATE_OLDEST_FIRST;
	else if (optValStr == "msg_date_newest_first")
		sortOptVal = SUB_BOARD_SORT_LATEST_MSG_DATE_NEWEST_FIRST;
	return sortOptVal;
}

// Converts one of the sub-board sort options to a descriptive string
function subBoardSortOptionToDescriptiveStr(pSortOption)
{
	var optionStr = "None (as configured in the system)";
	switch (pSortOption)
	{
		case SUB_BOARD_SORT_NONE:
			optionStr = "None (as configured in the system)";
			break;
		case SUB_BOARD_SORT_ALPHABETICAL:
			optionStr = "Alphabetical";
			break;
		case SUB_BOARD_SORT_LATEST_MSG_DATE_OLDEST_FIRST:
			optionStr = "Latest message date (oldest first)";
			break;
		case SUB_BOARD_SORT_LATEST_MSG_DATE_NEWEST_FIRST:
			optionStr = "Latest message date (newest first)";
			break;
	}
	return optionStr;
}

// Sorts the heirarchy recursively, including None, Alphabetical,
// latest message date oldest first, and latest message date
// newest first.
//
// Parameters:
//  pHeirarchyArray: One of the arrays of items from the heirarchy
//  pSortOption: The option specifying which way to sort
function sortHeirarchyRecursive(pHeirarchyArray, pSortOption)
{
	switch (pSortOption)
	{
		case SUB_BOARD_SORT_NONE:
			// Sort by the uniqueNumber property that was added when creating the
			// structure initially
			pHeirarchyArray.sort(function(pA, pB)
			{
				if (pA.uniqueNumber < pB.uniqueNumber)
					return -1;
				else if (pA.uniqueNumber == pB.uniqueNumber)
					return 0;
				else if (pA.uniqueNumber > pB.uniqueNumber)
					return 1;
			});
			break;
		case SUB_BOARD_SORT_ALPHABETICAL:
			pHeirarchyArray.sort(function(pA, pB)
			{
				if (pA.name < pB.name)
					return -1;
				else if (pA.name == pB.name)
					return 0;
				else if (pA.name > pB.name)
					return 1;
			});
			break;
		case SUB_BOARD_SORT_LATEST_MSG_DATE_OLDEST_FIRST:
			pHeirarchyArray.sort(function(pA, pB)
			{
				var sortVal = 0;
				if (pA.hasOwnProperty("subItemObj") && pB.hasOwnProperty("subItemObj"))
				{
					var latestMsgTimeA = getLatestMsgTime(pA.subItemObj.code);
					var latestMsgTimeB = getLatestMsgTime(pB.subItemObj.code);
					if (latestMsgTimeA < latestMsgTimeB)
						sortVal = -1;
					else if (latestMsgTimeA == latestMsgTimeB)
						sortVal = 0;
					else if (latestMsgTimeA > latestMsgTimeB)
						sortVal = 1;
				}
				return sortVal;
			});
			break;
		case SUB_BOARD_SORT_LATEST_MSG_DATE_NEWEST_FIRST:
			pHeirarchyArray.sort(function(pA, pB)
			{
				var sortVal = 0;
				if (pA.hasOwnProperty("subItemObj") && pB.hasOwnProperty("subItemObj"))
				{
					var latestMsgTimeA = getLatestMsgTime(pA.subItemObj.code);
					var latestMsgTimeB = getLatestMsgTime(pB.subItemObj.code);
					if (latestMsgTimeA < latestMsgTimeB)
						sortVal = 1;
					else if (latestMsgTimeA == latestMsgTimeB)
						sortVal = 0;
					else if (latestMsgTimeA > latestMsgTimeB)
						sortVal = -1;
				}
				return sortVal;
			});
			break;
	}
	pHeirarchyArray.sorted = true;
	// Sort recursively
	for (var i = 0; i < pHeirarchyArray.length; ++i)
	{
		if (pHeirarchyArray[i].hasOwnProperty("items"))
			sortHeirarchyRecursive(pHeirarchyArray[i].items, pSortOption);
		pHeirarchyArray[i].sorted = true;
	}
}

// With a DDLightbarMenu object for message areas, checks to see if any of the items in
// the array aren't sub-boards.
// Also, see which one has the user's current chosen sub-board so we can set the
// current menu item index - And save that index in the menu object for its
// reference later.
//
// Parameters:
//  pMenuObj: The DDLightbarMenu object representing the menu
//  pMsgAreaHeirarchyObj: An object from this.msgArea_list, which is
//                        set up with a 'name' property and either
//                        an 'items' property if it has sub-items
//                        or a 'subItemObj' property if it's a message
//                        sub-board
//  pSubCodeMatchOverride: Optional - If known, this is an internal code of a
//                         sub-board to match (other than bbs.cursub_code)
//  pLastChosenSubBoardsPerGrpForUser: Optional - An object where the keys are the
//                                     message group names and the values are the
//                                     user's last chosen sub-board for each group.
function setMenuIdxWithSelectedSubBoard(pMenuObj, pMsgAreaHeirarchyObj, pSubCodeMatchOverride, pLastChosenSubBoardsPerGrpForUser)
{
	var retObj = {
		allSubs: true,
		allOnlyOtherItems: true,
	};

	pMenuObj.idxWithUserSelectedSubBoard = -1;
	for (var i = 0; i < pMsgAreaHeirarchyObj.length; ++i)
	{
		// Each object will have either an "items" or a "subItemObj"
		if (!pMsgAreaHeirarchyObj[i].hasOwnProperty("subItemObj"))
		{
			retObj.allSubs = false;
			pMenuObj.allSubs = false;
		}
		if (!pMsgAreaHeirarchyObj[i].hasOwnProperty("items"))
			retObj.allOnlyOtherItems = false
		// See if this one has the user's selected message sub-board
		if (msgAreaStructureHasCurrentUserSubBoard(pMsgAreaHeirarchyObj[i], pSubCodeMatchOverride, pLastChosenSubBoardsPerGrpForUser))
			pMenuObj.idxWithUserSelectedSubBoard = i;
		// If we've found all we need, then stop going through the array
		if (!retObj.allSubs && pMenuObj.idxWithUserSelectedSubBoard > -1)
			break;
	}
	if (pMenuObj.idxWithUserSelectedSubBoard >= 0 && pMenuObj.idxWithUserSelectedSubBoard < pMenuObj.NumItems())
	{
		pMenuObj.selectedItemIdx = pMenuObj.idxWithUserSelectedSubBoard;
		if (pMenuObj.selectedItemIdx >= pMenuObj.topItemIdx+pMenuObj.GetNumItemsPerPage())
			pMenuObj.topItemIdx = pMenuObj.selectedItemIdx - pMenuObj.GetNumItemsPerPage() + 1;
	}

	return retObj;
}

/*
// Removes empty strings from an array - Given an array,
// makes a new array with only the non-empty strings and returns it.
function removeEmptyStrsFromArray(pArray)
{
	var newArray = [];
	for (var i = 0; i < pArray.length; ++i)
	{
		if (pArray[i].length > 0 && !/^\s+$/.test(pArray[i]))
			newArray.push(pArray[i]);
	}
	return newArray;
}
*/
