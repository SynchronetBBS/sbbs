/* This is a script that lets the user choose a file area,
 * with either a lightbar or traditional user interface.
 *
 * Date       Author          Description
 * ... Trimmed comments ...
 * 2019-08-22 Eric Oulashin   Version 1.18
 *                            Added file area searching
 * 2019-08-24 Eric Oulashin   Version 1.19
 *                            Fixed a bug with 'Next' search when returning from
 *                            returning from file directory choosing
 * 2020-04-19 Eric Oulashin   Version 1.20
 *                            For lightbar mode, it now uses DDLightbarMenu
 *                            instead of using internal lightbar code.
 * 2022-01-15 Eric Oulashin   Version 1.21
 *                            Added directory name collapsing
 * 2022-02-12 Eric Oulashin   Version 1.22
 *                            Fixed lightbar file directory choosing issue when
 *                            using name collapsing (was using the wrong data
 *                            structure)
 * 2022-03-18 Eric Oulashin   Version 1.23
 *                            For directory collapsing, if there's only one subdirectory,
 *                            then it won't be collapsed.
 * 2022-05-17 Eric Oulashin   Version 1.24
 *                            Fixes for searching and search error reporting (probably
 *                            due to mistaken copy & paste in an earlier commit)
 * 2022-06-12 Eric Oulashin   Version 1.26
 *                            Running this scipt with the "false command-line parameter
 *                            works again, allowing the user to choose the file directory
 *                            within their file library.
 * 2022-07-03 Eric Oulashin   Version 1.27
 *                            When listing libraries without first listing directories
 *                            within the user's current library, it wouldn't display
 *                            the libraries the first time because the library information
 *                            wasn't built when using directory collapsing.  This has been
 *                            fixed.
 * 2022-07-06 Eric Oulashin   Version 1.28
 *                            Fix for not actually moving to the user's selected directory
 *                            when directly choosing a directory in their library in lightbar mode.
 * 2022-07-23 Eric Oulashin   Version 1.29
 *                            Re-arranged the help text for lightbar mode to be more consistent with my message reader
 *                            (and message area chooser).
 * 2022-08-19 Eric Oulashin   Version 1.30
 *                            Set the control key pass-thru so that some hotkeys (such as Ctrl-P for PageUp) only
 *                            get caught by this script.
 * 2023-03-19 Eric Oulashin   Verison 1.33
 *                            Fix for inputting the library/dir # in lightbar mode. Updated wording for that
 *                            as well. Changed the version to match the message area chooser.
 * 2023-04-15 Eric Oulashin   Version 1.34
 *                            Fix: For lightbar mode with directory collapsing, now sets the selected item based
 *                            on the user's current directory. Also, color settings no longer need the control
 *                            character (they can just be a list of the attribute characters).
 * 2023-05-14 Eric Oulashin   Version 1.35
 *                            Refactored the configuration reading code.
 *                            Fix: Displays correct file counts in directories when using directory name collapsing
 * 2023-07-21 Eric Oulashin   Version 1.36
 *                            Fix for directory collapsing mode with the lightbar interface: It now exits
 *                            when the user chooses their same file directory instead of continuing the
 *                            menu input loop.
 * 2023-09-12 Eric Oulashin   Version 1.37 Beta
 *                            Area change header line bug fix
 * 2023-09-16 Eric Oulashin   Version 1.37
 *                            Releasing this version
 * 2023-09-17 Eric Oulashin   Version 1.38
 *                            Bug fix: Searching was stuck if using directory name collapsing
 * 2023-10-24 Eric Oulashin   Version 1.40
 *                            Directory name collapsing: Fix for incorrect subdir assignment.
 *                            Also, won't collapse if the name before the separator is the same as
 *                            the file library description.
 * 2024-03-13 Eric Oulashin   Version 1.41
 *                            Fix for the directory item counts
 * 2024-11-08 Eric Oulashin   Version 1.42 Beta
 *                            Started working on a change to directory collapsing to allow an
 *                            arbitrary amount of separators in the library/directory names to
 *                            create multiple levels of categories
 * 2025-03-17 Eric Oulashin   Version 1.42
 *                            Releasing this version
 * 2025-04-10 Eric Oulashin   Version 1.42b
 *                            Fix: altName wasn't added to items if name collapsing disabled.
 *                            Also, start of name collapsing enhancement (no empty names).
 * 2025-04-21 Eric Oulashin   Version 1.42c
 *                            F & L keys working again on the light bar menu (first & last page).
 *                            Fix to ensure the header lines are written in the proper place after
 *                            showing help.
 * 2025-05-03 Eric Oulashin   Version 1.42d
 *                            Fix: Displays the configured area change header again
 * 2025-05-27 Eric Oulashin   Version 1.42e
 *                            Fix: Name collapsing for group names w/ more than 1 instance
 * 2025-05-28 Eric Oulashin   Version 1.42f
 *                            Name collapsing now works at the top level. Also,
 *                            support for a double separator to not collapse (and
 *                            display just one of those characters).
 * 2025-06-01 Eric Oulashin   Version 1.43
 *                            If DDFileAreaChooser.cfg doesn't exist, read DDFileAreaChooser.example.cfg
 *                            (in the same directory as DDFileAreaChooser.js) if it exists.
 *                            Also did an internal refactor, moving some common functionality
 *                            out into DDAreaChooserCommon.js to make development a bit simpler.
 * 2025-06-05 Eric Oulashin   Version 1.44
 *                            Version update to match the message area chooser (no functional update here)
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
 * 2025-12-26 Eric Oulashin   Version 1.48 Beta
 *                            Started working on having the area chooser save the user's
 *                            last chosen directory for each file library, for when the
 *                            user switches between libraries (toggaleable w/ a user setting)
 * 2025-12-31 Eric Oulashin   Version 1.48
 *                            Releasing this version
 * 2026-02-27 Eric Oulashin   Version 1.49
 *                            Fix: When a file directory has 10,000+ files, the description
 *                            column color no longer extends into the # items column.  The
 *                            per-library descFieldLen (which accounts for numFilesLen) is
 *                            now used for color regions when displaying directories.
 *                            Fix: # items column now right-aligns correctly.  numFilesLen and
 *                            numDirsLen are now dynamic (consider subdir counts and lib dir counts).
 */

// TODO: Failing silently when 1st argument is true

/* Command-line arguments:
   1 (argv[0]): Boolean - Whether or not to choose a file library first (default).  If
                false, the user will only be able to choose a different directory within
				their current file library.
   2 (argv[1]): Boolean - Whether or not to run the area chooser (if false,
                then this file will just provide the DDFileAreaChooser class).
*/

if (typeof(require) === "function")
{
	require("sbbsdefs.js", "K_NOCRLF");
	require("dd_lightbar_menu.js", "DDLightbarMenu");
	require("DDAreaChooserCommon.js", "getAreaHeirarchy");
	require("choice_scroll_box.js", "ChoiceScrollbox");
	require("cp437_defs.js", "CP437_BOX_DRAWINGS_UPPER_LEFT_SINGLE");
}
else
{
	load("sbbsdefs.js");
	load("dd_lightbar_menu.js");
	load("DDAreaChooserCommon.js");
	load("choice_scroll_box.js");
	load("cp437_defs.js");
}

// This script requires Synchronet version 3.14 or higher.
// Exit if the Synchronet version is below the minimum.
if (system.version_num < 31400)
{
	var message = "\x01n\x01h\x01y\x01i* Warning:\x01n\x01h\x01w Digital Distortion Message Lister "
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
var DD_FILE_AREA_CHOOSER_VERSION = "1.49";
var DD_FILE_AREA_CHOOSER_VER_DATE = "2026-02-27";

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

// File directory sort options
const FILE_DIR_SORT_NONE = 0;
const FILE_DIR_SORT_ALPHABETICAL = 1;

// Misc. defines
var ERROR_WAIT_MS = 1500;
var SEARCH_TIMEOUT_MS = 10000;

// User settings file name
var gUserSettingsFilename = system.data_dir + "user/" + format("%04d", user.number) + ".DDFileAreaChooser_Settings";


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
	// When exiting this script, make sure to set the ctrl key pasthru back to what it was originally
	js.on_exit("console.ctrlkey_passthru = " + console.ctrlkey_passthru);
	console.ctrlkey_passthru = "+ACGKLOPQRTUVWXYZ_"; // So that control key combinations only get caught by this script

	var fileAreaChooser = new DDFileAreaChooser();
	fileAreaChooser.SelectFileArea(chooseFileLib);
}

// End of script execution

///////////////////////////////////////////////////////////////////////////////////
// DDFileAreaChooser class stuff

function DDFileAreaChooser()
{
	// Colors
	this.colors = {
		areaNum: "\x01n\x01w\x01h",
		desc: "\x01n\x01c",
		numItems: "\x01b\x01h",
		header: "\x01n\x01y\x01h",
		fileAreaHdr: "\x01n\x01g",
		areaMark: "\x01g\x01h",
		// Highlighted colors (for lightbar mode)
		bkgHighlight: "\x01" + "4", // Blue background
		areaNumHighlight: "\x01w\x01h",
		descHighlight: "\x01c",
		numItemsHighlight: "\x01w\x01h",
		// Lightbar help line colors
		lightbarHelpLineBkg: "\x01" + "7",
		lightbarHelpLineGeneral: "\x01b",
		lightbarHelpLineHotkey: "\x01r",
		lightbarHelpLineParen: "\x01m"
	};

	// useLightbarInterface specifies whether or not to use the lightbar
	// interface.  The lightbar interface will still only be used if the
	// user's terminal supports ANSI.
	this.useLightbarInterface = true;

	this.areaNumLen = 4; // TODO: Make dynamic?
	this.descFieldLen = 67; // Description field length
	this.numDirsLen = 4; // TODO: Make dynamic

	// Filename base of a header to display above the area list
	this.areaChooserHdrFilenameBase = "fileAreaChgHeader";
	this.areaChooserHdrMaxLines = 5;

	// Whether or not to enable directory collapsing.  For
	// example, for directories in a library starting with
	// common text and a separator (specified below), the
	// common text will be the only one displayed, and when
	// the user selects it, a 3rd tier with the directories
	// after the separator will be shown
	this.useDirCollapsing = true;
	// The separator character to use for directory collapsing
	this.dirCollapseSeparator = ":";

	// User settings
	this.userSettings = {
		// Area change sorting for changing to another sub-board: None, Alphabetical, or LatestMsgDate
		areaChangeSorting: FILE_DIR_SORT_NONE,
		// When changing to a different file library, whether to remember/use
		// the last directory in each file library as the currently selected
		// directory
		rememberLastDirWhenChangingLib: false
	};
	// The user's last chosen directories for each file library. The key is
	// the library name and the value is the internal code for the user's last
	// chosen sub-board for that library.
	this.lastChosenDirsPerLibForUser = {};

	// Set the functions for the object
	this.ReadConfigFile = DDFileAreaChooser_ReadConfigFile;
	this.ReadUserSettingsFile = DDFileAreaChooser_ReadUserSettingsFile;
	this.WriteUserSettingsFile = DDFileAreaChooser_WriteUserSettingsFile;
	this.SelectFileArea = DDFileAreaChooser_SelectFileArea;
	this.WriteLibListHdrLine = DDFileAreaChooser_WriteLibListHdrLine;
	this.WriteDirListHdr1Line = DDFileAreaChooser_WriteDirListHdr1Line;
	// Lightbar-specific functions
	this.CreateLightbarMenu = DDFileAreaChooser_CreateLightbarMenu;
	this.GetColorIndexInfoForLightbarMenu = DDFileAreaChooser_GetColorIndexInfoForLightbarMenu;
	this.DisplayMenuHdrWithNumItems = DDFileAreaChooser_DisplayMenuHdrWithNumItems;
	this.WriteKeyHelpLine = DDFileAreaChooser_writeKeyHelpLine;
	// Help screen
	this.ShowHelpScreen = DDFileAreaChooser_ShowHelpScreen;
	// Misc. functions
	// Function to build the directory printf information for a file lib
	this.BuildFileDirPrintfInfoForLib = DDFileAreaChooser_buildFileDirPrintfInfoForLib;
	this.GetPrintfStrForDirWithoutAreaNum = DDFileAreaChooser_GetPrintfStrForDirWithoutAreaNum;
	// Function to display the header above the area list
	this.DisplayAreaChgHdr = DDFileAreaChooser_DisplayAreaChgHdr;
	this.WriteLightbarKeyHelpErrorMsg = DDFileAreaChooser_WriteLightbarKeyHelpErrorMsg;
	this.FindFileAreaIdxFromText = DDFileAreaChooser_FindFileAreaIdxFromText;
	this.GetGreatestNumFiles = DDFileAreaChooser_GetGreatestNumFiles;
	this.getMaxItemsCountInHierarchy = DDFileAreaChooser_getMaxItemsCountInHierarchy;
	this.DoUserSettings_Scrollable = DDFileAreaChooser_DoUserSettings_Scrollable;
	this.DoUserSettings_Traditional = DDFileAreaChooser_DoUserSettings_Traditional;

	// Read the settings from the config file.
	this.ReadConfigFile();
	// Read the user settings
	this.ReadUserSettingsFile();
    
    // lib_list will be set up with a file library/directory structure for
    // the chooser to use to let the user choose a file lib & directory. It
    // will be set up with the same basic format regardless of whether
    // directory collapsing is to be used or not.
	this.lib_list = getAreaHeirarchy(DDAC_FILE_AREAS, this.useDirCollapsing, this.dirCollapseSeparator);

	// Make numDirsLen dynamic so the # column stays right-aligned when a library has 1000+ directories
	var maxDirsInAnyLib = 0;
	for (var i = 0; i < this.lib_list.length; ++i)
	{
		if (this.lib_list[i].hasOwnProperty("items") && this.lib_list[i].items.length > maxDirsInAnyLib)
			maxDirsInAnyLib = this.lib_list[i].items.length;
	}
	this.numDirsLen = Math.max(4, Math.max(1, maxDirsInAnyLib.toString().length));
	this.descFieldLen = console.screen_columns - this.areaNumLen - this.numDirsLen - 5;

	// printf strings used for outputting the file libraries
	this.fileLibPrintfStr = " " + this.colors.areaNum + "%" + this.areaNumLen + "d "
	                      + this.colors.desc + "%-" + this.descFieldLen
	                      + "s " + this.colors.numItems + "%" + this.numDirsLen + "d";
	this.fileLibHighlightPrintfStr = "\x01n" + this.colors.bkgHighlight + " "
	                               + this.colors.areaNumHighlight + "%" + this.areaNumLen + "d "
	                               + this.colors.descHighlight + "%-" + this.descFieldLen
	                               + "s " + this.colors.numItemsHighlight + "%" + this.numDirsLen + "d";
	this.fileLibListHdrPrintfStr = this.colors.header + " %5s %-"
	                             + +(this.descFieldLen-2) + "s %6s";
	this.fileDirHdrPrintfStr = this.colors.header + " %5s %-"
	                         + +(this.descFieldLen-3) + "s %-7s";
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

	// this.fileDirListPrintfInfo will be an array of printf strings
	// for the file directories in the file libraries.  The index is the
	// file library index.  The file directory printf information is
	// created on the fly the first time the user lists directories for
	// a file library.
	this.fileDirListPrintfInfo = [];
	// Build printf information for each file library
	for (var libIdx = 0; libIdx < file_area.lib_list.length; ++libIdx)
		this.BuildFileDirPrintfInfoForLib(libIdx);

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
function DDFileAreaChooser_SelectFileArea(pChooseLib)
{
	var chooseLib = (typeof(pChooseLib) === "boolean" ? pChooseLib : true);

	// Start with this.lib_list, which is the topmost file lib/dir structure.
	var fileLibStructure = this.lib_list;
	var libName = "";
	if (!chooseLib)
	{
		for (var i = 0; i < this.lib_list.length; ++i)
		{
			//if (fileDirStructureHasCurrentUserFileDir(this.lib_list[i], dirCodeOverride, lastChosenDirsObj))
			if (fileDirStructureHasCurrentUserFileDir(this.lib_list[i], null, null))
			{
				if (this.lib_list[i].hasOwnProperty("items"))
					fileLibStructure = this.lib_list[i].items;
				break;
			}
		}

		// Set libName
		// This code isn't tested because SelectFileArea() isn't called with pChooseLib
		// false anymore.
		if (Array.isArray(fileLibStructure) && fileLibStructure.length > 0)
		{
			if (this.lib_list[fileLibStructure[0].topLevelIdx].hasOwnProperty("name"))
				libName = this.lib_list[fileLibStructure[0].topLevelIdx].name;
		}
	}

	var previousFileLibStructures = []; // Will be used like a stack
	var selectedItemIndexes = [];       // Will be used like a stack
	var selectedItemIdx = null;
	var chosenLibOrSubdirName = ""; // Will have the name of the user's chosen library/subdir
	var previousChosenLibOrSubdirNames = []; // Will be used like a stack
	var directoriesLabelLen = 15; // The length of the "Directories of " label
	var nameSep = " - ";          // A string to use to separate lib/subdirectory names for the top header line
	var numItemsWidth = 0;        // Width of the header column for # of items (not including the space), for creating the menu
	var sortingChanged = false;   // Will be set true when the user changes sorting so that we can update appropriately
	if (!this.useLightbarInterface || !console.term_supports(USER_ANSI))
		numItemsWidth = 5;
	// Main loop
	var writeHdrLines = false;
	var writeKeyHelpLine = true;
	var selectionLoopContinueOn = true;
	while (selectionLoopContinueOn)
	{
		console.clear("\x01n");

		// Write the header ANSI/ASC if there is one
		if (this.areaChangeHdrLines.length > 0)
		{
			this.DisplayAreaChgHdr(1, false);
			console.attributes = "N";
			console.crlf();
		}

		// If we're displaying the file libraries (top level), then we'll output 1
		// header line; otherwise, we'll output 2 header line; adjut the top line
		// of the menu accordingly.
		var menuTopRow = 0;
		if (fileLibStructure == this.lib_list || chosenLibOrSubdirName.length == 0)
			menuTopRow = this.areaChangeHdrLines.length + 2;
		else
		{
			menuTopRow = this.areaChangeHdrLines.length + 3;
			printf("\x01n%sDirectories of \x01h%s\x01n", this.colors.fileAreaHdr, chosenLibOrSubdirName.substr(0, console.screen_columns - directoriesLabelLen - 1));
			console.crlf();
		}
		var createMenuRet = this.CreateLightbarMenu(libName, fileLibStructure, previousFileLibStructures.length+1, menuTopRow, selectedItemIdx, numItemsWidth);
		if (this.useLightbarInterface && console.term_supports(USER_ANSI))
			numItemsWidth = createMenuRet.itemNumWidth;
		// If sorting has changed, ensure the menu's selected item is the user's
		// current sub-board
		if (sortingChanged)
		{
			//var screenPosBackup = console.getxy();
			setMenuIdxWithSelectedFileDir(createMenuRet.menuObj, fileLibStructure, null, this.lastChosenDirsPerLibForUser);
			// TODO: It seems the key help line at the bottom  could disappear in
			// this situation, so make sure it's still showing
			writeKeyHelpLine = true;
			// If we're back at the root, set sortingChanged back to false
			if (fileLibStructure == this.lib_list)
				sortingChanged = false;
		}
		var menu = createMenuRet.menuObj;
		// Write the header lines, & write the key help line at the bottom of the screen
		var numItemsColLabel = createMenuRet.allDirs ? "Files" : "Items";
		//this.DisplayMenuHdrWithNumItems(createMenuRet.itemNumWidth, createMenuRet.descWidth-3, createMenuRet.numItemsWidth, numItemsColLabel);
		if (this.useLightbarInterface && console.term_supports(USER_ANSI))
			this.DisplayMenuHdrWithNumItems(numItemsWidth, createMenuRet.descWidth-3, createMenuRet.numItemsWidth, numItemsColLabel);
		else
			this.DisplayMenuHdrWithNumItems(numItemsWidth, createMenuRet.descWidth-2, createMenuRet.numItemsWidth, numItemsColLabel); // TODO
		if (!this.useLightbarInterface || !console.term_supports(USER_ANSI))
			console.crlf(); // Not drawing the hotkey help line (this.WriteKeyHelpLine();)

		// Show the menu in a loop and get user input
		var lastSearchText = "";
		var lastSearchFoundIdx = -1;
		var drawMenu = true;
		// Menu input loop
		var menuContinueOn = true;
		while (menuContinueOn)
		{
			// Draw the header lines and key help line if needed
			if (writeHdrLines)
			{
				//this.DisplayMenuHdrWithNumItems(createMenuRet.itemNumWidth, createMenuRet.descWidth-3, createMenuRet.numItemsWidth, numItemsColLabel);
				if (this.useLightbarInterface && console.term_supports(USER_ANSI))
					this.DisplayMenuHdrWithNumItems(numItemsWidth, createMenuRet.descWidth-3, createMenuRet.numItemsWidth, numItemsColLabel);
				else
					this.DisplayMenuHdrWithNumItems(numItemsWidth, createMenuRet.descWidth-3, createMenuRet.numItemsWidth, numItemsColLabel); // TODO
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
					var itemPromptWord = createMenuRet.allDirs ? "item" : "directory";
					printf("\x01cChoose %s #: \x01h", itemPromptWord);
					var userInput = console.getnum(menu.NumItems());
					if (userInput > 0)
					{
						selectedMenuIdx = userInput - 1;
						selectedItemIndexes.push(selectedMenuIdx);
						previousChosenLibOrSubdirNames.push(chosenLibOrSubdirName);
						if (previousChosenLibOrSubdirNames.length > 1)
						{
							chosenLibOrSubdirName = previousChosenLibOrSubdirNames[previousChosenLibOrSubdirNames.length-1];
							// If chosenLibOrSubdirName is now too long, remove some of the previous labels
							while (chosenLibOrSubdirName.length > console.screen_columns - 1)
							{
								var sepIdx = chosenLibOrSubdirName.indexOf(nameSep);
								if (sepIdx > -1)
									chosenLibOrSubdirName = chosenLibOrSubdirName.substr(sepIdx + nameSep.length);
							}
						}
						else
							chosenLibOrSubdirName = fileLibStructure[selectedMenuIdx].name;
					}
					else
					{
						// The user didn't make a selection.  So, we need to refresh the
						// screen (including the header, due to things being moved down one line).
						if (this.useLightbarInterface && console.term_supports(USER_ANSI))
							console.gotoxy(1, 1);
						this.DisplayMenuHdrWithNumItems(createMenuRet.itemNumWidth, createMenuRet.descWidth-3, createMenuRet.numItemsWidth, numItemsColLabel);
						if (this.useLightbarInterface && console.term_supports(USER_ANSI))
							this.WriteKeyHelpLine();
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
				// The objects in this.lib_list have a 'name' property and either
                // an 'items' property if it has sub-items or a 'subItemObj' property
				// if it's a file directory
				selectedItemIndexes.push(selectedMenuIdx);
				var selectingTopLevelLib = (previousChosenLibOrSubdirNames.length == 0);
				previousChosenLibOrSubdirNames.push(chosenLibOrSubdirName);
				if (fileLibStructure[selectedMenuIdx].hasOwnProperty("items"))
				{
					previousFileLibStructures.push(fileLibStructure);
					if (previousChosenLibOrSubdirNames.length > 1)
					{
						chosenLibOrSubdirName = previousChosenLibOrSubdirNames[previousChosenLibOrSubdirNames.length-1] + nameSep + fileLibStructure[selectedMenuIdx].name;
						// If chosenLibOrSubdirName is now too long, remove some of the previous labels
						while (chosenLibOrSubdirName.length > console.screen_columns - 1)
						{
							var sepIdx = chosenLibOrSubdirName.indexOf(nameSep);
							if (sepIdx > -1)
								chosenLibOrSubdirName = chosenLibOrSubdirName.substr(sepIdx + nameSep.length);
						}
					}
					else
						chosenLibOrSubdirName = fileLibStructure[selectedMenuIdx].name;
					// Set libName if the user has chosen a file library (at the top level)
					if (selectingTopLevelLib)
					{
						//libName = chosenLibOrSubdirName;
						libName = fileLibStructure[selectedMenuIdx].shortName;
					}
					fileLibStructure = fileLibStructure[selectedMenuIdx].items;
					menuContinueOn = false;
				}
				else if (fileLibStructure[selectedMenuIdx].hasOwnProperty("subItemObj"))
				{
					// The user has selected a file directory
					bbs.curdir_code = fileLibStructure[selectedMenuIdx].subItemObj.code;
					// Add to the user's last-used sub-boards dictionary & save the user's settings
					this.lastChosenDirsPerLibForUser[file_area.dir[bbs.curdir_code].lib_name] = bbs.curdir_code;
					this.WriteUserSettingsFile();
					// Don't continue the loops
					menuContinueOn = false;
					selectionLoopContinueOn = false;
				}

				// The key help line will need to be written again
				writeKeyHelpLine = true;
			}
			else if ((lastUserInputUpper == "/") || (lastUserInputUpper == CTRL_F)) // Start of find
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
					lastSearchText = searchText;
					// If the user entered text, then do the search, and if found,
					// found, go to the page and select the item indicated by the
					// search.
					if (searchText.length > 0)
					{
						var oldLastSearchFoundIdx = lastSearchFoundIdx;
						var oldSelectedItemIdx = menu.selectedItemIdx;
						var idx = this.FindFileAreaIdxFromText(fileLibStructure, searchText, menu.selectedItemIdx);
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
							var idx = this.FindFileAreaIdxFromText(fileLibStructure, searchText, 0);
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
					// TODO: Ensure this is correct for file areas
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
						var idx = this.FindFileAreaIdxFromText(fileLibStructure, searchText, 0);
						//lastSearchFoundIdx = idx; // Don't need to set lastSearchFoundIdx
						var newMsgAreaStructure = [];
						while (idx > -1)
						{
							newMsgAreaStructure.push(fileLibStructure[idx]);

							// Find the next one
							idx = this.FindFileAreaIdxFromText(fileLibStructure, searchText, idx+1);
						}
						if (newMsgAreaStructure.length > 0)
						{
							selectedItemIdx = selectedItemIndexes.push(selectedItemIdx);
							fileLibStructure = previousFileLibStructures.push(fileLibStructure);
							previousChosenLibOrSubdirNames.push("");
							fileLibStructure = newMsgAreaStructure;
							// TODO: libName
							createMenuRet = this.CreateLightbarMenu(libName, newMsgAreaStructure, previousFileLibStructures.length+1, menuTopRow, 0, numItemsWidth);
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
					var idx = this.FindFileAreaIdxFromText(fileLibStructure, searchText, lastSearchFoundIdx+1);
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
						idx = this.FindFileAreaIdxFromText(fileLibStructure, searchText, 0);
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
					sortHeirarchyRecursive(this.lib_list, this.userSettings.areaChangeSorting);
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
			else if (lastUserInputUpper == "?")
			{
				var usingLightbar = this.useLightbarInterface && console.term_supports(USER_ANSI);
				this.ShowHelpScreen(usingLightbar, true);
				menuContinueOn = true;
				selectionLoopContinueOn = true;
				drawMenu = true;
				writeHdrLines = true;
				writeKeyHelpLine = true;
				console.clear("\x01n");
			}
			// Quit - Note: This check should be last
			else if (lastUserInputUpper == "Q" || lastUserInputUpper == KEY_ESC || selectedMenuIdx == null)
			{
				// Cancel/Quit
				// Quit this menu loop and go back to the previous file lib/dir structure
				menuContinueOn = false;
				selectedItemIdx = selectedItemIndexes.pop();
				if (previousFileLibStructures.length == 0)
				{
					// The user was at the first level in the lib/dir structure; fully quit out from here
					selectionLoopContinueOn = false;
				}
				else // Go to the previous file lib/dir structure
				{
					fileLibStructure = previousFileLibStructures.pop();
					if (fileLibStructure == this.lib_list)
					{
						chosenLibOrSubdirName = "";
						previousChosenLibOrSubdirNames = [];
						selectedItemIndexes = [];
					}
					else
						chosenLibOrSubdirName = previousChosenLibOrSubdirNames.pop();
				}

				// The key help line will need to be written again
				writeKeyHelpLine = true;
			}
		}
	}
}

// For the DDFileAreaChooser class: Outputs the header line to appear above
// the list of file libraries.
//
// Parameters:
//  pNumPages: The number of pages (a number).  This is optional; if this is
//             not passed, then it won't be used.
//  pPageNum: The page number.  This is optional; if this is not passed,
//            then it won't be used.
function DDFileAreaChooser_WriteLibListHdrLine(pNumPages, pPageNum)
{
	var descStr = "Description";
	if ((typeof(pPageNum) == "number") && (typeof(pNumPages) == "number"))
		descStr += "    (Page " + pPageNum + " of " + pNumPages + ")";
	else if ((typeof(pPageNum) == "number") && (typeof(pNumPages) != "number"))
		descStr += "    (Page " + pPageNum + ")";
	else if ((typeof(pPageNum) != "number") && (typeof(pNumPages) == "number"))
		descStr += "    (" + pNumPages + (pNumPages == 1 ? " page)" : " pages)");
	printf(this.fileLibListHdrPrintfStr, "Lib #", descStr, "# Dirs");
	console.cleartoeol("\x01n");
}

// For the DDFileAreaChooser class: Outputs the first header line to appear
// above the directory list for a file library.
//
// Parameters:
//  pLibIdx: The index of the file library (assumed to be valid)
//  pDirIdx: The directory index, if directory collapsing is enabled and
//           we need to write the header line for subdirectories within a
//           directory.  This can be negative if not needed.
//  pNumPages: The number of pages (a number).  This is optional; if this is
//             not passed, then it won't be used.
//  pPageNum: The page number.  This is optional; if this is not passed,
//            then it won't be used.
function DDFileAreaChooser_WriteDirListHdr1Line(pLibIdx, pDirIdx, pNumPages, pPageNum)
{
	var descLen = 40;
	var descFormatStr = this.colors.fileAreaHdr + "Directories of \x01h%-" + descLen + "s     \x01n"
	                  + this.colors.fileAreaHdr;
	if ((typeof(pPageNum) == "number") && (typeof(pNumPages) == "number"))
		descFormatStr += "(Page " + pPageNum + " of " + pNumPages + ")";
	else if ((typeof(pPageNum) == "number") && (typeof(pNumPages) != "number"))
		descFormatStr += "(Page " + pPageNum + ")";
	else if ((typeof(pPageNum) != "number") && (typeof(pNumPages) == "number"))
		descFormatStr += "(" + pNumPages + (pNumPages == 1 ? " page)" : " pages)");
	// If using subdirectory collapsing, then build the description as needed.  Otherwise,
	// just use the subdirectory description.
	var desc = "";
	// The description should be the library's description.  Also, if pDirIdx
	// is a number, then append the directory description to the description.
	desc = this.lib_list[pLibIdx].description;
	if ((typeof(pDirIdx) === "number") && (this.lib_list[pLibIdx].dir_list[pDirIdx].subdir_list.length > 0))
		desc += this.dirCollapseSeparator + " " + this.lib_list[pLibIdx].dir_list[pDirIdx].description;
	printf(descFormatStr, desc.substr(0, descLen));
	console.cleartoeol("\x01n");
}

// For the DDFileAreaChooser class: Creates a lightbar menu to choose a library/directory.
//
// Parameters:
//  pLibName: The name of the file library (or an empty string if there is none yet)
//  pDirHeirarchyObj: An object from this.lib_list, which is
//                    set up with a 'name' property and either
//                    an 'items' property if it has sub-items
//                    or a 'subItemObj' property if it's a file
//                    directory
//  pHeirarchyLevel: The level we're at in the heirarchy (1-based)
//  pMenuTopRow: The screen row to use for the menu's top row
//  pSelectedItemIdx: Optional - The index to use for the selected item. If not
//                    specified, the item with the user's current selected file
//                    directory will be used, if available.
//  pNumItemsWidth: The character width of the column label for the number of items; mainly for the traditional (non-lightbar) UI
//
// Return value: An object with the following properties:
//               menuObj: The menu object
//               allDirs: Whether or not all the items in the menu
//                        are file directiries. If not, some or all
//                        are other lists of items
//               itemNumWidth: The width of the item numbers column
//               descWidth: The width of the description column
//               numItemsWidth: The width of the # of items column
function DDFileAreaChooser_CreateLightbarMenu(pLibName, pDirHeirarchyObj, pHeirarchyLevel, pMenuTopRow, pSelectedItemIdx, pNumItemsWidth)
{
	var retObj = {
		menuObj: null,
		allDirs: true,
		itemNumWidth: 0,
		descWidth: 0,
		numItemsWidth: 0
	};

	// Determine the correct desc width for color regions.  When showing directories
	// within a library, use the per-library descFieldLen (which accounts for the
	// library's numFilesLen - e.g. 5 digits for 10000+ files).  Using the global
	// this.descFieldLen when a library has 10000+ files would make the description
	// color extend into the # items column.
	var descWidthForColors = this.descFieldLen;
	if (pDirHeirarchyObj !== this.lib_list && Array.isArray(pDirHeirarchyObj) && pDirHeirarchyObj.length > 0
	    && pDirHeirarchyObj[0].hasOwnProperty("topLevelIdx"))
	{
		var topLevelIdx = pDirHeirarchyObj[0].topLevelIdx;
		if (typeof(this.fileDirListPrintfInfo[topLevelIdx]) !== "undefined"
		    && this.fileDirListPrintfInfo[topLevelIdx].hasOwnProperty("descFieldLen"))
			descWidthForColors = this.fileDirListPrintfInfo[topLevelIdx].descFieldLen;
	}
	// Get color index information for the menu
	var colorIdxInfo = this.GetColorIndexInfoForLightbarMenu(pDirHeirarchyObj, null, descWidthForColors);
	// Calculate column widths for the return object
	retObj.itemNumWidth = colorIdxInfo.fileDirListIdxes.itemNumEnd - 1;
	//retObj.descWidth = colorIdxInfo.fileDirListIdxes.descEnd - colorIdxInfo.fileDirListIdxes.descStart;
	retObj.descWidth = descWidthForColors;
	retObj.numItemsWidth = console.screen_columns - colorIdxInfo.fileDirListIdxes.numItemsStart;
	// Create and set up the menu
	var fileDirMenuHeight = console.screen_rows - pMenuTopRow;
	var fileDirMenu = new DDLightbarMenu(1, pMenuTopRow, console.screen_columns, fileDirMenuHeight);
	// Add additional keypresses for quitting the menu's input loop so we can
	// respond to these keys
	fileDirMenu.AddAdditionalQuitKeys("qQ?/" + CTRL_F + CTRL_U);
	fileDirMenu.AddAdditionalFirstPageKeys("fF");
	fileDirMenu.AddAdditionalLastPageKeys("lL");
	if (this.useLightbarInterface && console.term_supports(USER_ANSI))
	{
		fileDirMenu.allowANSI = true;
		// Additional quit keys for ANSI mode which we can respond to
		// N: Next (search) and numbers for item input
		fileDirMenu.AddAdditionalQuitKeys(" nN0123456789" + CTRL_C);
	}
	// If not using the lightbar interface (and ANSI behavior is not to be allowed), fileDirMenu.numberedMode
	// will be set to true by default.  Also, in that situation, set the menu's item number color
	else
	{
		fileDirMenu.allowANSI = false;
		fileDirMenu.colors.itemNumColor = this.colors.areaNum;
		//retObj.numItemsWidth = maxNumItemsWidthInHeirarchy(pDirHeirarchyObj);
		if (typeof(pNumItemsWidth) === "number")
			retObj.itemNumWidth = pNumItemsWidth;
		else
			retObj.itemNumWidth = pDirHeirarchyObj.length.toString().length;
		retObj.descWidth = console.screen_columns - retObj.itemNumWidth - retObj.numItemsWidth - 1;
	}
	// If not using the lightbar interface (and ANSI behavior is not to be allowed), fileDirMenu.numberedMode
	// will be set to true by default.  Also, in that situation, set the menu's item number color
	if (!fileDirMenu.allowANSI)
		fileDirMenu.colors.itemNumColor = this.colors.areaNum;
	fileDirMenu.scrollbarEnabled = true;
	fileDirMenu.borderEnabled = false;
	// Set the color arrays in the menu
	fileDirMenu.SetColors({
		itemColor: colorIdxInfo.itemColor,
		selectedItemColor: colorIdxInfo.selectedItemColor
	});

	fileDirMenu.multiSelect = false;
	fileDirMenu.ampersandHotkeysInItems = false;
	fileDirMenu.wrapNavigation = false;
	// Menu prompt text for non-ANSI mode
	fileDirMenu.nonANSIPromptText = "\x01n\x01b\x01h" + TALL_UPPER_MID_BLOCK + " \x01n\x01cWhich, \x01hQ\x01n\x01cuit, \x01hCTRL-F\x01n\x01c, \x01h/\x01n\x01c: \x01h";

	// Build the file directory info for the given file library
	fileDirMenu.dirHeirarchyObj = pDirHeirarchyObj;
	fileDirMenu.areaChooser = this;
	fileDirMenu.allDirs = true; // Whether the menu has only directories (no file libraries)
	if (Array.isArray(pDirHeirarchyObj))
	{
		// See if any of the items in the array aren't directories, and set retObj.allDirs.
		// Also, see which one has the user's current chosen directory so we can set the
		// current menu item index - And save that index in the menu object for its
		// reference later.
		var lastChosenDirsObj = null;
		var dirCodeOverride = null;
		if (pHeirarchyLevel > 1 && this.userSettings.rememberLastDirWhenChangingLib)
		{
			lastChosenDirsObj = this.lastChosenDirsPerLibForUser;
			if (this.lastChosenDirsPerLibForUser.hasOwnProperty(pLibName))
				dirCodeOverride = this.lastChosenDirsPerLibForUser[pLibName];
		}
		var tmpRetObj = setMenuIdxWithSelectedFileDir(fileDirMenu, pDirHeirarchyObj, dirCodeOverride, lastChosenDirsObj);
		retObj.allDirs = tmpRetObj.allDirs;

		// Replace the menu's NumItems() function to return the correct number of items
		fileDirMenu.NumItems = function() {
			return this.dirHeirarchyObj.length;
		};
		fileDirMenu.numItemsLen = fileDirMenu.NumItems().toString().length;
		// Replace the menu's GetItem() function to create & return an item for the menu
		fileDirMenu.descFieldLen = descWidthForColors; // Mainly for lightbar mode
		if (!fileDirMenu.allowANSI)
			fileDirMenu.descFieldLen += 3;
		fileDirMenu.GetItem = function(pItemIdx) {
			var menuItemObj = this.MakeItemWithRetval(-1);
			//var showDirMark = fileDirStructureHasCurrentUserFileDir(this.dirHeirarchyObj[pItemIdx], null, this.lastChosenDirsPerLibForUser);
			var showDirMark = (pItemIdx == this.idxWithUserSelectedDir);
			var areaDesc = this.dirHeirarchyObj[pItemIdx].name;
			var numItems = 0;
			var adjustDescLen = false;
			if (this.dirHeirarchyObj[pItemIdx].hasOwnProperty("items"))
			{
				numItems = this.dirHeirarchyObj[pItemIdx].items.length;
				// If this isn't the top level (libraries), then add "<subdirs>" to the description
				if (this.dirHeirarchyObj != this.areaChooser.lib_list)
				{
					areaDesc += "  <subdirs>";
					//adjustDescLen = true;
				}
			}
			else if (this.dirHeirarchyObj[pItemIdx].hasOwnProperty("subItemObj"))
			{
				if (numItems = this.dirHeirarchyObj[pItemIdx].subItemObj.hasOwnProperty("files"))
					numItems = this.dirHeirarchyObj[pItemIdx].subItemObj.files; // Added in Synchronet 3.18c
				else
				{
					var dirIdx = this.dirHeirarchyObj[pItemIdx].subItemObj.index;
					numItems = this.areaChooser.fileDirListPrintfInfo[this.dirHeirarchyObj[pItemIdx].topLevelIdx].fileCounts[dirIdx];
				}
			}

			// Menu item text
			menuItemObj.text = (showDirMark ? "*" : " ");
			if (this.allowANSI)
			{
				menuItemObj.text += format(this.areaChooser.fileDirListPrintfInfo[this.dirHeirarchyObj[pItemIdx].topLevelIdx].printfStr, pItemIdx+1,
										   areaDesc.substr(0, this.descFieldLen), numItems);
			}
			else
			{
				// No ANSI - Numbered mode
				var numSpaces = retObj.itemNumWidth - this.numItemsLen - console.strlen(menuItemObj.text);
				if (numSpaces > 0)
					areaDesc = format("%*s", numSpaces, "") + areaDesc;
				// TODO: In a library with mixed items that are a subdir or have more items (such
				// as my BBS files), the ones with the items (subdirs) didn't have the correct length
				// - The description length for the ones with subdirs is 1 too long, but the ones with
				// items are good. But it seems fixed now with adjustDesclen not being set to true..?
				var descWidth = console.screen_columns - this.numItemsLen - retObj.numItemsWidth - 2;
				if (numSpaces > 0)
				{
					//descWidth -= (retObj.itemNumWidth - numSpaces);
					descWidth -= numSpaces;
				}
				if (adjustDescLen || pHeirarchyLevel >= 3 || this.numItemsLen == 1) // Not sure why the heirarchy level matters here
					++descWidth;
				var formatStr = this.areaChooser.GetPrintfStrForDirWithoutAreaNum(descWidth, retObj.numItemsWidth);
				menuItemObj.text += format(formatStr, areaDesc.substr(0, descWidth), numItems);

				// TODO: Adjust/set colors for the menu item?
				/*
				//var itemColorInfo = this.areaChooser.GetColorIndexInfoForLightbarMenu(this.dirHeirarchyObj, null, this.descFieldLen+1);
				var itemColorInfo = this.areaChooser.GetColorIndexInfoForLightbarMenu(this.dirHeirarchyObj, retObj.numItemsWidth, retObj.descWidth+1);
				menuItemObj.itemColor = itemColorInfo.itemColor;
				menuItemObj.itemSelectedColor = itemColorInfo.selectedItemColor;
				*/
			}
			menuItemObj.text = strip_ctrl(menuItemObj.text);
			menuItemObj.retval = pItemIdx;
			return menuItemObj;
		};
		
		// Set the currently selected item
		var selectedIdx = fileDirMenu.idxWithUserSelectedDir;
		if (typeof(pSelectedItemIdx) === "number" && pSelectedItemIdx >= 0 && pSelectedItemIdx < fileDirMenu.NumItems())
			selectedIdx = pSelectedItemIdx;
		if (selectedIdx >= 0 && selectedIdx < fileDirMenu.NumItems())
		{
			fileDirMenu.selectedItemIdx = selectedIdx;
			if (fileDirMenu.selectedItemIdx >= fileDirMenu.topItemIdx+fileDirMenu.GetNumItemsPerPage())
				fileDirMenu.topItemIdx = fileDirMenu.selectedItemIdx - fileDirMenu.GetNumItemsPerPage() + 1;
		}
	}

	retObj.menuObj = fileDirMenu;
	return retObj;
}
// Helper for DDFileAreaChooser_CreateLightbarMenu(): Returns arrays of objects with start, end, and attrs properties
// for the lightbar menu to add colors to the menu items.
//
// Parameters:
//  pDirHeirarchyObj: An object from this.lib_list, which is set up with a
//                    'name' property and either an 'items' property if it
//                    has sub-items or a 'subItemObj' property if it's a file
//                    directory
//  pNumItemsWidthOverride: Optional - An override for the width of the # items column. Mainly for the traditional (non-lightbar) UI
//                          If this is not specified/null, then this.areaNumLen will be used.
//  pDescWidthOverride: Optional - Description width override
function DDFileAreaChooser_GetColorIndexInfoForLightbarMenu(pDirHeirarchyObj, pNumItemsWidthOverride, pDescWidthOverride)
{
	var retObj = {
		fileDirListIdxes: {}, // Start & end indexes for the various items in each item list row
		itemColor: [],        // Normal item color array
		selectedItemColor: [] // Selected item color array
	};

	var areaNumLen = this.areaNumLen;
	if (typeof(pNumItemsWidthOverride) === "number" && pNumItemsWidthOverride >= 0)
		areaNumLen = pNumItemsWidthOverride;
	var descLen = this.descFieldLen;
	if (typeof(pDescWidthOverride) === "number" && pDescWidthOverride >= 0)
		descLen = pDescWidthOverride;

	if (this.useLightbarInterface && console.term_supports(USER_ANSI))
	{
		retObj.fileDirListIdxes = {
			markCharStart: 0,
			markCharEnd: 1,
			itemNumStart: 1,
			itemNumEnd: 3 + areaNumLen
		};
		retObj.fileDirListIdxes.descStart = retObj.fileDirListIdxes.itemNumEnd;
		retObj.fileDirListIdxes.descEnd = retObj.fileDirListIdxes.descStart + descLen + 1;
		retObj.fileDirListIdxes.numItemsStart = retObj.fileDirListIdxes.descEnd;
		// Set numItemsEnd to -1 to let the whole rest of the lines be colored
		retObj.fileDirListIdxes.numItemsEnd = -1;
		// Item color arrays
		retObj.itemColor = [{start: retObj.fileDirListIdxes.markCharStart, end: retObj.fileDirListIdxes.markCharEnd, attrs: this.colors.areaMark},
		                    {start: retObj.fileDirListIdxes.itemNumStart, end: retObj.fileDirListIdxes.itemNumEnd, attrs: this.colors.areaNum},
		                    {start: retObj.fileDirListIdxes.descStart, end: retObj.fileDirListIdxes.descEnd, attrs: this.colors.desc},
		                    {start: retObj.fileDirListIdxes.numItemsStart, end: retObj.fileDirListIdxes.numItemsEnd, attrs: this.colors.numItems}],
		retObj.selectedItemColor = [{start: retObj.fileDirListIdxes.markCharStart, end: retObj.fileDirListIdxes.markCharEnd, attrs: this.colors.areaMark + this.colors.bkgHighlight},
		                            {start: retObj.fileDirListIdxes.itemNumStart, end: retObj.fileDirListIdxes.itemNumEnd, attrs: this.colors.areaNumHighlight + this.colors.bkgHighlight},
		                            {start: retObj.fileDirListIdxes.descStart, end: retObj.fileDirListIdxes.descEnd, attrs: this.colors.descHighlight + this.colors.bkgHighlight},
		                            {start: retObj.fileDirListIdxes.numItemsStart, end: retObj.fileDirListIdxes.numItemsEnd, attrs: this.colors.numItemsHighlight + this.colors.bkgHighlight}]
	}
	else
	{
		retObj.fileDirListIdxes = {
			markCharStart: 0,
			markCharEnd: 1,
			descStart: 1,
		};
		retObj.fileDirListIdxes.descEnd = 2 + descLen;
		retObj.fileDirListIdxes.numItemsStart = retObj.fileDirListIdxes.descEnd;
		// Set numItemsEnd to -1 to let the whole rest of the lines be colored
		retObj.fileDirListIdxes.numItemsEnd = -1;
		// Item color arrays
		retObj.itemColor = [{start: retObj.fileDirListIdxes.markCharStart, end: retObj.fileDirListIdxes.markCharEnd, attrs: this.colors.areaMark},
		                    {start: retObj.fileDirListIdxes.descStart, end: retObj.fileDirListIdxes.descEnd, attrs: this.colors.desc},
		                    {start: retObj.fileDirListIdxes.numItemsStart, end: retObj.fileDirListIdxes.numItemsEnd, attrs: this.colors.numItems}],
		retObj.selectedItemColor = [{start: retObj.fileDirListIdxes.markCharStart, end: retObj.fileDirListIdxes.markCharEnd, attrs: this.colors.areaMark + this.colors.bkgHighlight},
		                            {start: retObj.fileDirListIdxes.descStart, end: retObj.fileDirListIdxes.descEnd, attrs: this.colors.descHighlight + this.colors.bkgHighlight},
		                            {start: retObj.fileDirListIdxes.numItemsStart, end: retObj.fileDirListIdxes.numItemsEnd, attrs: this.colors.numItemsHighlight + this.colors.bkgHighlight}]
	}

	return retObj;
}

// For the DDFileAreaChooser class: Displays a header line for use above the menu for items that
// contain a number of items (sub-boards or files)
function DDFileAreaChooser_DisplayMenuHdrWithNumItems(pItemNumLen, pDescLen, pNumItemsLen, pItemsColumnLabel)
{
	console.attributes = 0;
	var itemNumLabel = (pItemNumLen >= 5 ? "Item#" : "#");
	var formatStr = "%s%" + pItemNumLen + "s %-" + pDescLen + "s %" + pNumItemsLen + "s";
	printf(formatStr, this.colors.header, itemNumLabel, "Description", "# " + pItemsColumnLabel);
}

// For the DDFileAreaChooser class: Displays the key help line at the bottom of the screen
function DDFileAreaChooser_writeKeyHelpLine()
{
	console.gotoxy(1, console.screen_rows);
	//console.print(this.lightbarKeyHelpText);
	console.putmsg(this.lightbarKeyHelpText);
}

// For the DDFileAreaChooser class: Reads the configuration file.
function DDFileAreaChooser_ReadConfigFile()
{
	// Use "DDFileAreaChooser.cfg" if that exists; otherwise, use
	// "DDFileAreaChooser.example.cfg" (the stock config file)
	var cfgFilenameBase = "DDFileAreaChooser.cfg";
	var cfgFilename = file_cfgname(system.mods_dir, cfgFilenameBase);
	if (!file_exists(cfgFilename))
		cfgFilename = file_cfgname(system.ctrl_dir, cfgFilenameBase);
	if (!file_exists(cfgFilename))
		cfgFilename = file_cfgname(js.exec_dir, cfgFilenameBase);
	// If the configuration file hasn't been found, look to see if there's a DDFileAreaChooser.example.cfg file
	// available in the same directory 
	if (!file_exists(cfgFilename))
	{
		var exampleFileName = file_cfgname(js.exec_dir, "DDFileAreaChooser.example.cfg");
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
		if (typeof(behaviorSettings["areaChooserHdrFilenameBase"]) === "string")
			this.areaChooserHdrFilenameBase = behaviorSettings.areaChooserHdrFilenameBase;
		if (typeof(behaviorSettings["useDirCollapsing"]) === "boolean")
			this.useDirCollapsing = behaviorSettings.useDirCollapsing;
		if (typeof(behaviorSettings["dirCollapseSeparator"]) === "string" && behaviorSettings["dirCollapseSeparator"].length > 0)
			this.dirCollapseSeparator = behaviorSettings.dirCollapseSeparator;
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

// For the DDFileAreaChooser class: Reads the user settings file
function DDFileAreaChooser_ReadUserSettingsFile()
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
		// Last chosen directories
		var lastDirs = userSettingsFile.iniGetObject("LAST_DIRECTORIES");
		if (lastDirs != null)
			this.lastChosenDirsPerLibForUser = lastDirs;

		userSettingsFile.close();
	}
}

// For the DDFileAreaChooser class: Writes the user settings file
function DDFileAreaChooser_WriteUserSettingsFile()
{
	var writeSucceeded = false;
	// Open the user settings file, if it exists
	var userSettingsFile = new File(gUserSettingsFilename);
	if (userSettingsFile.open(userSettingsFile.exists ? "r+" : "w+"))
	{
		// Variables in this.userSettings are initialized in the DDFileAreaChooserconstructor. For each
		// user setting (except for twitlist), save the setting in the user's settings file. The user's
		// twit list is an array that is saved to a separate file.
		for (var settingName in this.userSettings)
		{
			userSettingsFile.iniSetValue("BEHAVIOR", settingName, this.userSettings[settingName]);
		}
		// Last chosen directories
		for (var dirName in this.lastChosenDirsPerLibForUser)
		{
			userSettingsFile.iniSetValue("LAST_DIRECTORIES", dirName, this.lastChosenDirsPerLibForUser[dirName]);
		}
		userSettingsFile.close();
		writeSucceeded = true;
	}
	return writeSucceeded;
}

// Misc. functions

// For the DDFileAreaChooser class: Shows the help screen
//
// Parameters:
//  pLightbar: Boolean - Whether or not to show lightbar help.  If
//             false, then this function will show regular help.
//  pClearScreen: Boolean - Whether or not to clear the screen first
function DDFileAreaChooser_ShowHelpScreen(pLightbar, pClearScreen)
{
	if (pClearScreen)
		console.clear("\x01n");
	else
		console.attributes = "N";
	console.center("\x01c\x01hDigital Distortion File Area Chooser");
	var lineStr = "";
	for (var i = 0; i < 36; ++i)
		lineStr += HORIZONTAL_SINGLE;
	console.attributes = "HK";
	console.center(lineStr);
	console.center("\x01n\x01cVersion \x01g" + DD_FILE_AREA_CHOOSER_VERSION +
	               " \x01w\x01h(\x01b" + DD_FILE_AREA_CHOOSER_VER_DATE + "\x01w)");
	console.crlf();
	console.print("\x01n\x01cFirst, a listing of file libraries is displayed.  One can be chosen by typing");
	console.crlf();
	console.print("its number.  Then, a listing of directories within that library will be");
	console.crlf();
	console.print("shown, and one can be chosen by typing its number.");
	console.crlf();

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
		console.print("\x01hENTER\x01n\x01c: Select the current library/dir");
		console.crlf();
		console.print("\x01hHOME\x01n\x01c: Go to the first item on the screen");
		console.crlf();
		console.print("\x01hEND\x01n\x01c: Go to the last item on the screen");
		console.crlf();
		console.print("\x01hPageUp\x01n\x01c/\x01hPageDown\x01n\x01c: Go to the previous/next page");
		console.crlf();
		console.print("\x01hF\x01n\x01c/\x01hL\x01n\x01c: Go to the first/last page");
		console.crlf();
		console.print("\x01h/\x01n\x01c or \x01hCTRL-F\x01n\x01c: Find by name/description");
		console.crlf();
		console.print("\x01hN\x01n\x01c: Next search result (after a find)");
		console.crlf();
		console.print("\x01hCTRL-U\x01n\x01c: User settings (i.e., sorting)");
		console.crlf();
	}

	console.crlf();
	console.print("Additional keyboard commands:");
	console.crlf();
	for (var i = 0; i < 29; ++i)
			console.print(HORIZONTAL_SINGLE);
	console.attributes = "HK";
	console.crlf();
	console.print("\x01n\x01c\x01h?\x01n\x01c: Show this help screen");
	console.crlf();
	console.print("\x01hQ\x01n\x01c: Quit");
	console.crlf();
}

// Returns the number of files in a directory for one of the file libraries.
// Note that returns the number of files in the directory on the hard drive,
// which isn't necessarily the number of files in the database for the directory.
//
// Paramters:
//  pLibIdx: The file library index (0-based)
//  pDirIdx: The file directory index (0-based)
//
// Returns: The number of files in the directory
function numFilesInDir(pLibIdx, pDirIdx)
{
	var numFiles = 0;

	// file_area.lib_list[pLibIdx].dir_list[pDirIdx].files was added in Synchronet 3.18c
	if (file_area.lib_list[pLibIdx].dir_list[pDirIdx].hasOwnProperty("files"))
		numFiles = file_area.lib_list[pLibIdx].dir_list[pDirIdx].files;
	else
	{
		// Count the files in the directory.  If it's not a directory, then
		// increment numFiles.
		var files = directory(file_area.lib_list[pLibIdx].dir_list[pDirIdx].path + "*.*");
		numFiles = files.length;
		// Make sure directories aren't counted: Go through the files array, and
		// for each directory, decrement numFiles.
		for (var i in files)
		{
			if (file_isdir(files[i]))
				--numFiles;
		}
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
		this.fileDirListPrintfInfo[pLibIndex] = {
			numFilesLen: 4
		};
		// Get information about the number of files in each directory
		// and the greatest number of files and set up the according
		// information in the file directory list object
		var fileDirInfo = this.GetGreatestNumFiles(pLibIndex);
		var greatestNum = 0;
		if (fileDirInfo != null)
		{
			greatestNum = fileDirInfo.greatestNumFiles;
			this.fileDirListPrintfInfo[pLibIndex].fileCounts = fileDirInfo.fileCounts.slice(0);
			this.fileDirListPrintfInfo[pLibIndex].fileCountsByCode = fileDirInfo.fileCountsByCode;
		}
		else
		{
			// fileDirInfo is null.  We still want to create
			// the fileCounts array in the file directory object
			// so that it's valid.
			if (this.useDirCollapsing)
			{
				this.fileDirListPrintfInfo[pLibIndex].fileCounts = [];
				this.fileDirListPrintfInfo[pLibIndex].fileCountsByCode = {};
				for (var dirIdx = 0; dirIdx < this.lib_list[pLibIndex].dir_list.length; ++dirIdx)
				{
					if (this.lib_list[pLibIndex].dir_list.subdir_list.length > 0)
						this.fileDirListPrintfInfo[pLibIndex].fileCounts[dirIdx] = this.lib_list[pLibIndex].dir_list.subdir_list.length;
					else
						this.fileDirListPrintfInfo[pLibIndex].fileCounts[dirIdx] = 0;
				}
				for (var dirIdx = 0; i < file_area.lib_list[pLibIndex].dir_list.length; ++dirIdx)
				{
					var dirCode = file_area.lib_list[pLibIndex].dir_list[dirIdx].code;
					this.fileDirListPrintfInfo[pLibIndex].fileCountsByCode[dirCode] = 0;
				}
			}
			else
			{
				this.fileDirListPrintfInfo[pLibIndex].fileCounts = new Array(file_area.lib_list[pLibIndex].dir_list.length);
				for (var dirIdx = 0; i < file_area.lib_list[pLibIndex].dir_list.length; ++dirIdx)
					this.fileDirListPrintfInfo[pLibIndex].fileCounts[dirIdx] == 0;
			}
		}
		// Also consider items.length for collapsed subdirs (e.g. 1000+ subdirs in a group)
		// so the # column stays right-aligned
		if (typeof(this.lib_list[pLibIndex]) !== "undefined" && this.lib_list[pLibIndex].hasOwnProperty("items"))
		{
			var maxFromHierarchy = this.getMaxItemsCountInHierarchy(this.lib_list[pLibIndex], fileDirInfo);
			if (maxFromHierarchy > greatestNum)
				greatestNum = maxFromHierarchy;
		}
		this.fileDirListPrintfInfo[pLibIndex].numFilesLen = Math.max(1, greatestNum.toString().length);

		// Set the description field length and printf strings for
		// this file library
		this.fileDirListPrintfInfo[pLibIndex].descFieldLen = console.screen_columns - this.areaNumLen - this.fileDirListPrintfInfo[pLibIndex].numFilesLen - 5;
		this.fileDirListPrintfInfo[pLibIndex].printfStr =
		                        this.colors.areaNum + " %" + this.areaNumLen + "d "
		                        + this.colors.desc + "%-"
		                        + this.fileDirListPrintfInfo[pLibIndex].descFieldLen
		                        + "s " + this.colors.numItems + "%"
		                        + this.fileDirListPrintfInfo[pLibIndex].numFilesLen + "d";
		                        this.fileDirListPrintfInfo[pLibIndex].highlightPrintfStr =
		                        "\x01n" + this.colors.bkgHighlight
		                        + this.colors.areaNumHighlight + " %" + this.areaNumLen
		                        + "d " + this.colors.descHighlight + "%-"
		                        + this.fileDirListPrintfInfo[pLibIndex].descFieldLen
		                        + "s " + this.colors.numItemsHighlight + "%"
		                        + this.fileDirListPrintfInfo[pLibIndex].numFilesLen +"d\x01n";
		this.fileDirListPrintfInfo[pLibIndex].printfStrWithoutAreaNum =
		                        this.colors.desc + "%-"
		                        + (this.fileDirListPrintfInfo[pLibIndex].descFieldLen+2)
		                        + "s " + this.colors.numItems + "%"
		                        + this.fileDirListPrintfInfo[pLibIndex].numFilesLen + "d";
		this.fileDirListPrintfInfo[pLibIndex].highlightPrintfStr =
		                        "\x01n" + this.colors.bkgHighlight
		                        + this.colors.areaNumHighlight + " %" + this.areaNumLen
		                        + "d " + this.colors.descHighlight + "%-"
		                        + this.fileDirListPrintfInfo[pLibIndex].descFieldLen
		                        + "s " + this.colors.numItemsHighlight + "%"
		                        + this.fileDirListPrintfInfo[pLibIndex].numFilesLen +"d\x01n";
	}
}

 // Returns a printf string for an area item without an area number; mainly for
 // the traditional/non-lightbar interface, which uses the menu in numbered mode
 //
 // Parameters:
 //  pDescLen: The length to use for the description
 //  pNumItemsLen: The lenggth of the field for the number of items in the area (right side)
 //
 // Return value: The printf string for the item
function DDFileAreaChooser_GetPrintfStrForDirWithoutAreaNum(pDescLen, pNumItemsLen)
{
	var printfStr = "\x01n" + this.colors.desc + "%-" + pDescLen + "s " + this.colors.numItems + "%"
	              + pNumItemsLen + "d";
	return printfStr;
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

// For the DDFileAreaChooser class: Writes a temporary error message at the key help line
// for lightbar mode.
//
// Parameters:
//  pErrorMsg: The error message to write
//  pRefreshHelpLine: Whether or not to re-draw the help line on the screen
function DDFileAreaChooser_WriteLightbarKeyHelpErrorMsg(pErrorMsg, pRefreshHelpLine)
{
	console.gotoxy(1, console.screen_rows);
	console.cleartoeol("\x01n");
	console.gotoxy(1, console.screen_rows);
	console.print("\x01y\x01h" + pErrorMsg + "\x01n");
	mswait(ERROR_WAIT_MS);
	if (pRefreshHelpLine && this.useLightbarInterface && console.term_supports(USER_ANSI))
		this.WriteKeyHelpLine();
}

// Finds a file area index with search text, matching either the name or
// description, case-insensitive.
//
// Parameters:
//  pFileAreaStructure: The file area structure from this.msgArea_list to search through
//  pSearchText: The name/description text to look for
//  pStartItemIdx: The item index to start at.  Defaults to 0
//
// Return value: The index of the file area, or -1 if not found
function DDFileAreaChooser_FindFileAreaIdxFromText(pFileAreaStructure, pSearchText, pStartItemIdx)
{
	if (typeof(pSearchText) != "string")
		return -1;

	var areaIdx = -1;

	var startIdx = (typeof(pStartItemIdx) === "number" ? pStartItemIdx : 0);
	if ((startIdx < 0) || (startIdx > pFileAreaStructure.length))
		startIdx = 0;

	// Go through the message area list and look for a match
	var searchTextUpper = pSearchText.toUpperCase();
	for (var i = startIdx; i < pFileAreaStructure.length; ++i)
	{
		if (pFileAreaStructure[i].name.toUpperCase().indexOf(searchTextUpper) > -1 || pFileAreaStructure[i].altName.toUpperCase().indexOf(searchTextUpper) > -1)
		{
			areaIdx = i;
			break;
		}
	}

	return areaIdx;
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

// For the DDFileAreaChooser class: Recursively finds the maximum count displayed
// in the # column (items.length for groups, file count for dirs) in the hierarchy.
// Used to ensure numFilesLen accommodates all displayed values for right-alignment.
function DDFileAreaChooser_getMaxItemsCountInHierarchy(pNode, pFileDirInfo)
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
			var subMax = this.getMaxItemsCountInHierarchy(pNode.items[i], pFileDirInfo);
			if (subMax > max)
				max = subMax;
		}
	}
	else if (pNode.hasOwnProperty("subItemObj") && pFileDirInfo != null && pFileDirInfo.fileCounts != null)
	{
		var dirIdx = pNode.subItemObj.index;
		if (dirIdx >= 0 && dirIdx < pFileDirInfo.fileCounts.length)
		{
			var cnt = pFileDirInfo.fileCounts[dirIdx];
			if (cnt > max)
				max = cnt;
		}
	}
	return max;
}

// For the DDFileAreaChooser class: For a given file library index, returns an
// object containing the greatest number of files of all directories within a
// file library and an array containing the number of files in each directory.
// If the given library index is invalid, this function will return null.
// If directory collapsing is enabled, this will account for the number of
// subdirectories in the directories that have them.
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
//          fileCountsByCode: A dictionary indexed by internal code of the
//                            file directories, and each value is the number
//                            of files in the directory
function DDFileAreaChooser_GetGreatestNumFiles(pLibIndex)
{
	// Sanity checking
	if (typeof(pLibIndex) != "number")
		return null;
	if (typeof(file_area.lib_list[pLibIndex]) == "undefined")
		return null;

	var retObj = {
		greatestNumFiles: 0,
		fileCounts: null, // Will be an array
		fileCountsByCode: {}
	}

	retObj.fileCounts = new Array(file_area.lib_list[pLibIndex].dir_list.length);
	for (var dirIndex = 0; dirIndex < file_area.lib_list[pLibIndex].dir_list.length; ++dirIndex)
	{
		retObj.fileCounts[dirIndex] = numFilesInDir(pLibIndex, dirIndex);
		if (retObj.fileCounts[dirIndex] > retObj.greatestNumFiles)
			retObj.greatestNumFiles = retObj.fileCounts[dirIndex];
	}

	// Populate the fileCountsByCode dictionary for the given file library
	for (var dirIndex = 0; dirIndex < file_area.lib_list[pLibIndex].dir_list.length; ++dirIndex)
	{
		var dirCode = file_area.lib_list[pLibIndex].dir_list[dirIndex].code;
		retObj.fileCountsByCode[dirCode] = numFilesInDir(pLibIndex, dirIndex);
	}

	return retObj;
}

// For the DDFileAreaChooser class:  Lets the user manage their preferences/settings (scrollable/ANSI user interface).
//
// Return value: An object containing the following properties:
//               needWholeScreenRefresh: Boolean - Whether or not the whole screen needs to be
//                                       refreshed (i.e., when the user has edited their twitlist)
//               optionBoxTopLeftX: The top-left screen column of the option box
//               optionBoxTopLeftY: The top-left screen row of the option box
//               optionBoxWidth: The width of the option box
//               optionBoxHeight: The height of the option box
function DDFileAreaChooser_DoUserSettings_Scrollable()
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


	// When changing to a different file library, whether to remember/use
	// the last directory in each file library as the currently selected
	// directory
	const CHG_LIB_REMEMBER_DIRECTORY_OPT_INDEX = optionBox.addTextItem(format(optionFormatStr, "Remember directory when changing libraries"));
	if (this.userSettings.rememberLastDirWhenChangingLib)
		optionBox.chgCharInTextItem(CHG_LIB_REMEMBER_DIRECTORY_OPT_INDEX, checkIdx, CP437_CHECK_MARK);

	// Create an object containing toggle values (true/false) for each option index
	var optionToggles = {};
	optionToggles[CHG_LIB_REMEMBER_DIRECTORY_OPT_INDEX] = this.userSettings.rememberLastDirWhenChangingLib;

	// Other options
	// Sorting option
	var FILE_DIR_CHANGE_SORTING_OPT_INDEX = optionBox.addTextItem("Sorting");

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
					case CHG_LIB_REMEMBER_DIRECTORY_OPT_INDEX:
						this.areaChooserObj.userSettings.rememberLastDirWhenChangingLib = !this.areaChooserObj.userSettings.rememberLastDirWhenChangingLib;
						break;
					default:
						break;
				}
			}
			else
			{
				switch (itemIndex)
				{
					case FILE_DIR_CHANGE_SORTING_OPT_INDEX:
						var sortOptMenu = CreateFileDirChangeSortOptMenu(optBoxStartX, optBoxTopRow, optBoxWidth, optBoxHeight, this.areaChooserObj.userSettings.areaChangeSorting);
						var chosenSortOpt = sortOptMenu.GetVal();
						console.attributes = "N";
						if (typeof(chosenSortOpt) === "number")
							this.areaChooserObj.userSettings.areaChangeSorting = chosenSortOpt;
						retObj.needWholeScreenRefresh = false;
						this.drawBorder();
						this.drawInnerMenu(FILE_DIR_CHANGE_SORTING_OPT_INDEX);
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

// For the DDFileAreaChooser class: Lets the user change their settings (traditional user interface)
function DDFileAreaChooser_DoUserSettings_Traditional()
{
	var optNum = 1;
	var FILE_DIR_CHANGE_SORTING_OPT_NUM = optNum++;
	var HIGHEST_CHOICE_NUM = FILE_DIR_CHANGE_SORTING_OPT_NUM; // Highest choice number

	console.crlf();
	var wordFirstCharAttrs = "\x01c\x01h";
	var wordRemainingAttrs = "\x01c";
	console.print(colorFirstCharAndRemainingCharsInWords("User Settings", wordFirstCharAttrs, wordRemainingAttrs) + "\r\n");
	printTradUserSettingOption(FILE_DIR_CHANGE_SORTING_OPT_NUM, "Sorting", wordFirstCharAttrs, wordRemainingAttrs);
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
		case FILE_DIR_CHANGE_SORTING_OPT_NUM:
			console.attributes = "N";
			console.crlf();
			console.print("\x01cChoose a sorting option (\x01hQ\x01n\x01c to quit)");
			console.crlf();
			var sortOptMenu = CreateFileDirChangeSortOptMenu(1, 1, console.screen_columns, console.screen_rows, this.userSettings.areaChangeSorting);
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
					case FILE_DIR_SORT_NONE:
						console.print("None");
						break;
					case FILE_DIR_SORT_ALPHABETICAL:
						console.print("Alphabetical");
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

// Helper function for user settings: Creates the menu object to let the user choose a
// file area change sorting option, and returns the object
function CreateFileDirChangeSortOptMenu(pX, pY, pWidth, pHeight, pCurrentSortSetting)
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
	sortOptMenu.topBorderText = "File area change sorting";
	sortOptMenu.Add("None", FILE_DIR_SORT_NONE);
	sortOptMenu.Add("Alphabetical", FILE_DIR_SORT_ALPHABETICAL);
	switch (pCurrentSortSetting)
	{
		case FILE_DIR_SORT_NONE:
			sortOptMenu.selectedItemIdx = 0;
			break;
		case FILE_DIR_SORT_ALPHABETICAL:
			sortOptMenu.selectedItemIdx = 1;
			break;
	}

	sortOptMenu.colors.itemColor = "\x01n\x01c\x01h";

	// For use in numbered mode for the traditional UI:
	sortOptMenu.colors.itemNumColor = "\x01n\x01c";
	sortOptMenu.colors.highlightedItemNumColor = "\x01n\x01g\x01h";

	return sortOptMenu;
}

///////////////////////////////
// Misc. functions

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
//  pFileDir: Boolean - If true, search the file directory list for the given library index.
//            If false, search the library list.
//  pStartItemIdx: The item index to start at
//  pLibIdx: The index of the library to search in (only for doing a directory search)
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

// Finds a file library index with search text, matching either the name or
// description, case-insensitive.
//
// Parameters:
//  pSearchText: The name/description text to look for
//  pStartItemIdx: The item index to start at.  Defaults to 0
//
// Return value: The index of the file library, or -1 if not found
function findFileLibIdxFromText(pSearchText, pStartItemIdx)
{
	if (typeof(pSearchText) != "string")
		return -1;

	var libIdx = -1;

	var startIdx = (typeof(pStartItemIdx) == "number" ? pStartItemIdx : 0);
	if ((startIdx < 0) || (startIdx > file_area.lib_list.length))
		startIdx = 0;

	// Go through the file library list and look for a match
	var searchTextUpper = pSearchText.toUpperCase();
	for (var i = startIdx; i < file_area.lib_list.length; ++i)
	{
		if ((file_area.lib_list[i].name.toUpperCase().indexOf(searchTextUpper) > -1) ||
		    (file_area.lib_list[i].description.toUpperCase().indexOf(searchTextUpper) > -1))
		{
			libIdx = i;
			break;
		}
	}

	return libIdx;
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

// Finds the index of a file library AFTER the given library index which contains
// directories.  If there are none, this will return -1.
//
// Parameters:
//  pLibIdx: An index of a file library; this function will start searching AFTER this one
//
// Return value: An index of a file library after the given group index that contains directories,
//               or -1 if there are none
function findNextLibIdxWithDirs(pLibIdx)
{
	if (typeof(pLibIdx) !== "number")
		return -1;
	var nextLibIdx = -1;
	//file_area.lib_list[pLibIdx].dir_list
	if (pLibIdx < file_area.lib_list.length - 1)
	{
		for (var i = pLibIdx + 1; i < file_area.lib_list.length && nextLibIdx == -1; ++i)
		{
			if (file_area.lib_list[i].dir_list.length > 0)
				nextLibIdx = i;
		}
	}
	if (nextLibIdx == -1 && pLibIdx > 0)
	{
		for (var i = 0; i < pLibIdx && nextLibIdx == -1; ++i)
		{
			if (file_area.lib_list[i].dir_list.length > 0)
				nextLibIdx = i;
		}
	}
	return nextLibIdx;
}

// Given a file lib/directory heirarchy object built by this module, this
// function returns whether it contains the user's currently selected file
// directory.
//
// Parameters:
//  pDirHeirarchyObj: An object from this.lib_list, which is
//                    set up with a 'name' property and either
//                    an 'items' property if it has sub-items
//                    or a 'subItemObj' property if it's a file
//                    directory
//  pDirCodeMatchOverride: Optional - If known, this is an internal code of a
//                         file directory to match (other than bbs.curdir_code)
//  pLastChosenDirsPerLibForUser: Optional - An object where the keys are the
//                                file library names and the values are the
//                                user's last chosen directories for each library.
//
// Return value: Whether or not the given structure has the user's currently selected file directory
function fileDirStructureHasCurrentUserFileDir(pDirHeirarchyObj, pDirCodeMatchOverride, pLastChosenDirsPerLibForUser)
{
	var currentUserFileDirFound = false;
	if (Array.isArray(pDirHeirarchyObj))
	{
		// This could be the top-level array or one of the 'items' properties, which is an array.
		// Go through the array and call this function again recursively; this function will
		// return when we get to an actual file directory that is the user's current selection.
		for (var i = 0; i < pDirHeirarchyObj.length && !currentUserFileDirFound; ++i)
			currentUserFileDirFound = fileDirStructureHasCurrentUserFileDir(pDirHeirarchyObj[i], pDirCodeMatchOverride, pLastChosenDirsPerLibForUser);
	}
	else
	{
		// This is one of the objects with 'name' and an 'items' or 'subItemObj'
		if (pDirHeirarchyObj.hasOwnProperty("subItemObj"))
		{
			//currentUserFileDirFound = (bbs.curdir_code == pDirHeirarchyObj.subItemObj.code);
			var dirCodeToLookFor = bbs.curdir_code;
			if (typeof(pDirCodeMatchOverride) === "string" && pDirCodeMatchOverride.length > 0)
				dirCodeToLookFor = pDirCodeMatchOverride;
			currentUserFileDirFound = (dirCodeToLookFor == pDirHeirarchyObj.subItemObj.code);
		}
		else if (pDirHeirarchyObj.hasOwnProperty("items"))
		{
			var dirCodeToLookFor = null;
			if (typeof(pDirCodeMatchOverride) === "string" && pDirCodeMatchOverride.length > 0)
				dirCodeToLookFor = pDirCodeMatchOverride;
			else if (pLastChosenDirsPerLibForUser != null && typeof(pLastChosenDirsPerLibForUser) === "object" && pDirHeirarchyObj.hasOwnProperty("name"))
			{
				if (pLastChosenDirsPerLibForUser.hasOwnProperty(pDirHeirarchyObj.name))
					dirCodeToLookFor = pLastChosenDirsPerLibForUser[pDirHeirarchyObj.name];
			}
			currentUserFileDirFound = fileDirStructureHasCurrentUserFileDir(pDirHeirarchyObj.items, dirCodeToLookFor, pLastChosenDirsPerLibForUser);
		}
	}
	return currentUserFileDirFound;
}

// Given an array of file directory heirarchy objects, this returns the length of the
// longest number of items in the heirarchy - if there are any with an "items" array.
// If there are no items with their own "items" array, this will return 0.
//
// Parameters:
//  pDirHeirarchyObj: An object from this.lib_list, which is
//                    set up with a 'name' property and either
//                    an 'items' property if it has sub-items
//                    or a 'subItemObj' property if it's a file
//                    directory
//
// Return value: The length of the longest number of items in the heirarchy
function maxNumItemsWidthInHeirarchy(pDirHeirarchyObj)
{
	var maxNumItemsWidth = 0;
	for (var i = 0; i < pDirHeirarchyObj.length; ++i)
	{
		if (pDirHeirarchyObj[i].hasOwnProperty("items"))
		{
			var numItemsLen = pDirHeirarchyObj[i].items.length.toString().length;
			if (numItemsLen > maxNumItemsWidth)
				maxNumItemsWidth = numItemsLen;
		}
	}
	return maxNumItemsWidth;
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
		case FILE_DIR_SORT_NONE:
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
		case FILE_DIR_SORT_ALPHABETICAL:
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

// With a DDLightbarMenu object for file areas, checks to see if any of the items in
// the array aren't directories.
// Also, see which one has the user's current chosen directory so we can set the
// current menu item index - And save that index in the menu object for its
// reference later.
//
// Parameters:
//  pMenuObj: The DDLightbarMenu object representing the menu
//  pDirHeirarchyObj: An object from this.lib_list, which is set
//                    up with a 'name' property and either an
//                    'items' property if it has sub-items or a
//                    'subItemObj' property if it's a file directory
//  pDirCodeMatchOverride: Optional - If known, this is an internal code of a
//                         file directory to match (other than bbs.curdir_code)
//  pLastChosenDirsPerLibForUser: Optional - An object where the keys are the
//                                file library names and the values are the
//                                user's last chosen directory for each library.
function setMenuIdxWithSelectedFileDir(pMenuObj, pDirHeirarchyObj, pDirCodeMatchOverride, pLastChosenDirsPerLibForUser)
{
	var retObj = {
		allDirs: true
	};

	pMenuObj.idxWithUserSelectedDir = -1;
	for (var i = 0; i < pDirHeirarchyObj.length; ++i)
	{
		// Each object will have either an "items" or a "subItemObj"
		if (!pDirHeirarchyObj[i].hasOwnProperty("subItemObj"))
		{
			retObj.allDirs = false;
			pMenuObj.allDirs = false;
		}
		// See if this one has the user's selected file directory
		if (fileDirStructureHasCurrentUserFileDir(pDirHeirarchyObj[i], pDirCodeMatchOverride, pLastChosenDirsPerLibForUser))
			pMenuObj.idxWithUserSelectedDir = i;
		// If we've found all we need, then stop going through the array
		if (!retObj.allDirs && pMenuObj.idxWithUserSelectedDir > -1)
			break;
	}
	if (pMenuObj.idxWithUserSelectedDir >= 0 && pMenuObj.idxWithUserSelectedDir < pMenuObj.NumItems())
	{
		pMenuObj.selectedItemIdx = pMenuObj.idxWithUserSelectedDir;
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
