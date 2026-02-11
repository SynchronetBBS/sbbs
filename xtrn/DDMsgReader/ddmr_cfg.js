// Configurator for Digital Distortion Message Reader: This is a menu-driven configuration
// program/script for Digital Distortion Message reader.  If you're running DDMsgReader from
// xtrn/DDMsgReader (the standard location), then any changes are saved to DDMsgReader.cfg in
// sbbs/mods, so that custom changes don't get overridden due to an update.
// If you have DDMsgReader in a directory other than xtrn/DDMsgReader, then the changes to
// DDMsgReader.cfg will be saved in that directory (assuming you're running ddmr_cfg.js from
// that same directory).
//
// If you're running DDMsgReader from xtrn/DDMsgReader (the standard location) and you want
// to save the configuration file there (rather than sbbs/mods), you can use one of the
// following command-line options: noMods, -noMods, no_mods, or -no_mods

"use strict";


require("sbbsdefs.js", "P_NONE");
require("uifcdefs.js", "UIFC_INMSG");


if (!uifc.init("DigDist. Message Reader 1.97i Configurator"))
{
	print("Failed to initialize uifc");
	exit(1);
}
js.on_exit("uifc.bail()");


// DDMsgReader base configuration filename, and help text wrap width
const gDDMRCfgFileName = "DDMsgReader.cfg";
var gHelpWrapWidth = uifc.screen_width - 10;

// When saving the configuration file, always save it in the same directory
// as DDMsgReader (or this script); don't copy to mods
var gAlwaysSaveCfgInOriginalDir = true;

// Parse command-line arguments
parseCmdLineArgs();

// Read the DDMsgReader configuration file
var gCfgInfo = readDDMsgReaderCfgFile();

// Show the main menu and go from there.
// This is in a loop so that if the user aborts from confirming to save
// settings, they'll return to the main menu.
var anyOptionChanged = false;
var continueOn = true;
while (continueOn)
{
	anyOptionChanged = doMainMenu() || anyOptionChanged;
	// If any option changed, then let the user save the configuration if they want to
	if (anyOptionChanged)
	{
		var userChoice = promptYesNo("Save configuration?", true, WIN_ORG|WIN_MID|WIN_ACT|WIN_ESC);
		if (typeof(userChoice) === "boolean")
		{
			if (userChoice)
			{
				var saveRetObj = saveDDMsgReaderCfgFile();
				// Show an error if failed to save the settings.  If succeeded, only show
				// a message if settings were saved to the mods directory.
				if (!saveRetObj.saveSucceeded)
					uifc.msg("Failed to save settings!");
				else
				{
					var msg = "Changes were successfully saved";
					if (saveRetObj.savedToModsDir)
						msg += " (to the mods dir)";
					uifc.msg(msg);
				}
			}
			continueOn = false;
		}
	}
	else
		continueOn = false;
}



///////////////////////////////////////////////////////////
// Functions

function doMainMenu()
{
	// For menu item text formatting
	var itemTextMaxLen = 50;

	// Configuration option object properties
	// cfgOptProps must correspond exactly with optionStrs & menuItems
	var cfgOptProps = [
		"listInterfaceStyle", // String (Lightbar/Traditional)
		"reverseListOrder", // Boolean
		"readerInterfaceStyle", // String (Scrollable/Traditional)
		//"readerInterfaceStyleForANSIMessages", // String (Scrollable/Traditional)
		"displayBoardInfoInHeader", // Boolean
		"promptToContinueListingMessages", // Boolean
		"promptConfirmReadMessage", // Boolean
		"msgListDisplayTime", // String (written/imported)
		"msgAreaList_lastImportedMsg_time", // String (written/imported)
		"startMode", // String (Reader/List)
		"tabSpaces", // Number
		"pauseAfterNewMsgScan", // Boolean
		"readingPostOnSubBoardInsteadOfGoToNext", // Boolean
		"displayAvatars", // Boolean
		"rightJustifyAvatars", // Boolean
		"msgListSort", // String (Written/Received)
		"convertYStyleMCIAttrsToSync", // Boolean
		"prependFowardMsgSubject", // Boolean
		"enableIndexedModeMsgListCache", // Boolean
		"quickUserValSetIndex", // Number (can be -1)
		"saveAllHdrsWhenSavingMsgToBBSPC", // Boolean
		"msgSaveDir", // String
		"useIndexedModeForNewscan", // Boolean
		"displayIndexedModeMenuIfNoNewMessages", // Boolean
		"indexedModeMenuSnapToFirstWithNew", // Boolean
		"indexedModeMenuSnapToNextWithNewAftarMarkAllRead", // Boolean
		"newscanOnlyShowNewMsgs", // Boolean
		"promptDelPersonalEmailAfterReply", // Boolean
		"subBoardChangeSorting", // String: None, Alphabetical, LatestMsgDateOldestFirst, or LatestMsgDateNewestFirst
		"indexedModeNewscanOnlyShowSubsWithNewMsgs", // Boolean
		"showUserResponsesInTallyInfo", // Boolean
		"DDMsgAreaChooser", // String
		"themeFilename" // String
	];
	// Strings for the options to display on the menu
	var optionStrs = [
		"List Interface Style",
		"Reverse List Order",
		"Reader Interface Style",
		//"readerInterfaceStyleForANSIMessages",
		"Display Board Info In Header",
		"Prompt to Continue Listing Messages",
		"Prompt to Confirm Reading Message",
		"Message List Display Time",
		"Message Area List: Last Imported Message Time",
		"Start Mode",
		"Number of Spaces for Tabs",
		"Pause After New Message Scan",
		"Reading Post On Sub-Board Instead Of Go To Next",
		"Display Avatars",
		"Right-Justify Avatars",
		"Message List Sort",
		"Convert Y-Style MCI Attributes To Sync",
		"Prepend Forwarded Message Subject with \"Fwd\"",
		"Enable Indexed Mode Message List Cache",
		"Quick User Val Set Index",
		"Save All Headers When Saving Message To BBS PC",
		"Default directory to save messages to",
		"Use Indexed Mode For Newscan",
		"Display Indexed menu even with no new messages",
		"Index menu: Snap to sub-boards w/ new messages",
		"Index menu mark all read: Snap to subs w/ new msgs",
		"During a newscan, only show new messages",
		"Personal email: Prompt to delete after reply",
		"Sorting for sub-board change",
		"Index newscan: Only show subs w/ new msgs",
		"Include user responses in tally info",
		"DDMsgAreaChooser full path & filename",
		"Theme Filename"
	];
	// Build an array of formatted string to be displayed on the menu
	// (the value formatting will depend on the variable type)
	var menuItems = [];
	for (var i = 0; i < cfgOptProps.length; ++i)
	{
		var propName = cfgOptProps[i];
		menuItems.push(formatCfgMenuText(itemTextMaxLen, optionStrs[i], gCfgInfo.cfgOptions[propName]));
	}

	// Help text
	// A dictionary of help text for each option, indexed by the option name from the configuration file
	if (doMainMenu.optHelp == undefined)
		doMainMenu.optHelp = getOptionHelpText();
	if (doMainMenu.mainScreenHelp == undefined)
		doMainMenu.mainScreenHelp = getMainHelp(doMainMenu.optHelp, cfgOptProps);

	// Create a CTX to specify the current selected item index
	if (doMainMenu.ctx == undefined)
		doMainMenu.ctx = uifc.list.CTX();
	// Selection
	var winMode = WIN_ORG|WIN_MID|WIN_ACT|WIN_ESC;
	var menuTitle = "DD Message Reader Behavior Configuration";
	var anyOptionChanged = false;
	var continueOn = true;
	while (continueOn && !js.terminated)
	{
		uifc.help_text = doMainMenu.mainScreenHelp;
		var optionMenuSelection = uifc.list(winMode, menuTitle, menuItems, doMainMenu.ctx);
		doMainMenu.ctx.cur = optionMenuSelection; // Remember the current selected item
		if (optionMenuSelection == -1) // ESC
			continueOn = false;
		else
		{
			var optName = cfgOptProps[optionMenuSelection];
			var itemType = typeof(gCfgInfo.cfgOptions[optName]);
			uifc.help_text = doMainMenu.optHelp[optName];
			if (optName == "quickUserValSetIndex")
			{
				// User quick-validation set index
				var selectedValSetIdx = promptQuickValSet();
				if (selectedValSetIdx > -1)
				{
					gCfgInfo.cfgOptions.quickUserValSetIndex = selectedValSetIdx;
					anyOptionChanged = true;
					menuItems[optionMenuSelection] = formatCfgMenuText(itemTextMaxLen, optionStrs[optionMenuSelection], gCfgInfo.cfgOptions[optName]);
				}
			}
			else if (itemType === "boolean")
			{
				gCfgInfo.cfgOptions[optName] = !gCfgInfo.cfgOptions[optName];
				anyOptionChanged = true;
				menuItems[optionMenuSelection] = formatCfgMenuText(itemTextMaxLen, optionStrs[optionMenuSelection], gCfgInfo.cfgOptions[optName]);
			}
			else if (itemType === "number")
			{
				var promptStr = optionStrs[optionMenuSelection];
				var initialVal = gCfgInfo.cfgOptions[optName].toString();
				var minVal = 1;
				var userInput = uifc.input(WIN_MID, promptStr, initialVal, 0, K_NUMBER|K_EDIT);
				if (typeof(userInput) === "string" && userInput.length > 0)
				{
					var value = parseInt(userInput);
					if (!isNaN(value) && value >= minVal && gCfgInfo.cfgOptions[optName] != value)
					{
						gCfgInfo.cfgOptions[optName] = value;
						anyOptionChanged = true;
						menuItems[optionMenuSelection] = formatCfgMenuText(itemTextMaxLen, optionStrs[optionMenuSelection], gCfgInfo.cfgOptions[optName]);
					}
				}
			}
			else
			{
				if (optName == "DDMsgAreaChooser")
				{
					// DDMsgAreaChooser full path & filename
					var userInput = uifc.input(WIN_MID, "DDMsgReader full path & filename", gCfgInfo.cfgOptions[optName], 255, K_EDIT);
					if (typeof(userInput) === "string" && userInput != gCfgInfo.cfgOptions[optName])
					{
						gCfgInfo.cfgOptions[optName] = userInput;
						anyOptionChanged = true;
						menuItems[optionMenuSelection] = formatCfgMenuText(itemTextMaxLen, optionStrs[optionMenuSelection], gCfgInfo.cfgOptions[optName]);
					}
				}
				else if (optName == "themeFilename")
				{
					// Theme filename
					var userInput = promptThemeFilename();
					if (typeof(userInput) === "string" && userInput != gCfgInfo.cfgOptions[optName])
					{
						gCfgInfo.cfgOptions[optName] = userInput;
						anyOptionChanged = true;
						menuItems[optionMenuSelection] = formatCfgMenuText(itemTextMaxLen, optionStrs[optionMenuSelection], gCfgInfo.cfgOptions[optName]);
					}
				}
				else if (optName == "msgSaveDir")
				{
					var userInput = uifc.input(WIN_MID, "Message save dir", gCfgInfo.cfgOptions[optName], 0, K_EDIT);
					if (typeof(userInput) === "string" && userInput.length > 0)
					{
						if (file_isdir(userInput))
						{
							anyOptionChanged = (userInput != gCfgInfo.cfgOptions[optName]);
							if (anyOptionChanged)
							{
								gCfgInfo.cfgOptions[optName] = userInput;
								menuItems[optionMenuSelection] = formatCfgMenuText(itemTextMaxLen, optionStrs[optionMenuSelection], gCfgInfo.cfgOptions[optName]);
							}
						}
						else
							uifc.msg("That directory doesn't exist!");
					}
				}
				else if (optName == "subBoardChangeSorting")
				{
					// Multiple-choice
					var options = ["None", "Alphabetical", "Msg date: Oldest first", "Msg date: Newest first"];
					var promptStr = optionStrs[optionMenuSelection];
					var userChoice = promptMultipleChoice(promptStr, options, gCfgInfo.cfgOptions[optName]);
					//if (userChoice != null && userChoice != undefined)
					var userChoiceIdx = options.indexOf(userChoice);
					if (userChoiceIdx >= 0 && userChoiceIdx < options.length)
					{
						switch (userChoiceIdx)
						{
							case 0: // None
								gCfgInfo.cfgOptions[optName] = "None";
								break;
							case 1: // Alphabetical
								gCfgInfo.cfgOptions[optName] = "Alphabetical";
								break;
							case 2: // Msg date: Oldest first
								gCfgInfo.cfgOptions[optName] = "LatestMsgDateOldestFirst";
								break;
							case 3: // Msg date: Newest first
								gCfgInfo.cfgOptions[optName] = "LatestMsgDateNewestFirst";
								break;
						}
						anyOptionChanged = true;
						menuItems[optionMenuSelection] = formatCfgMenuText(itemTextMaxLen, optionStrs[optionMenuSelection], gCfgInfo.cfgOptions[optName]);
					}
				}
				else
				{
					// Multiple-choice
					var options = [];
					if (optName == "listInterfaceStyle")
						options = ["Lightbar", "Traditional"];
					else if (optName == "readerInterfaceStyle")
						options = ["Scrollable", "Traditional"];
					else if (optName == "msgListDisplayTime" || optName == "msgAreaList_lastImportedMsg_time")
						options = ["Written", "Imported"];
					else if (optName == "startMode")
						options = ["Reader", "List"];
					else if (optName == "msgListSort")
						options = ["Written", "Received"];
					var promptStr = optionStrs[optionMenuSelection];
					var userChoice = promptMultipleChoice(promptStr, options, gCfgInfo.cfgOptions[optName]);
					if (userChoice != null && userChoice != undefined && userChoice != gCfgInfo.cfgOptions[optName])
					{
						gCfgInfo.cfgOptions[optName] = userChoice;
						anyOptionChanged = true;
						menuItems[optionMenuSelection] = formatCfgMenuText(itemTextMaxLen, optionStrs[optionMenuSelection], gCfgInfo.cfgOptions[optName]);
					}
				}
			}
		}
	}

	return anyOptionChanged;
}

// Formats text for a behavior option
//
// Parameters:
//  pItemTextMaxLen: The maximum length for menu item text
//  pItemText: The text of the item to be displayed on the menu
//  pVal: The value of the option
//
// Return value: The formatted text for the menu item, with a Yes/No value indicating whether it's enabled
function formatCfgMenuText(pItemTextMaxLen, pItemText, pVal)
{
	if (formatCfgMenuText.formatStr == undefined)
		formatCfgMenuText.formatStr = "%-" + pItemTextMaxLen + "s %s";
	// Determine what should be displayed for the value
	var valType = typeof(pVal);
	var value = "";
	if (valType === "boolean")
		value = pVal ? "Yes" : "No";
	else
	{
		if (typeof(pVal) !== "undefined")
			value = pVal.toString();
	}
	return format(formatCfgMenuText.formatStr, pItemText.substr(0, pItemTextMaxLen), value);
}


// Prompts the user for which dictionaries to use (for spell check)
function promptThemeFilename()
{
	// Find theme filenames. There should be a DefaultTheme.cfg; also, look
	// for theme .cfg filenames starting with DDMR_Theme_
	var defaultThemeFilename = js.exec_dir + "DefaultTheme.cfg";
	var themeFilenames = [];
	if (file_exists(defaultThemeFilename))
		themeFilenames.push(defaultThemeFilename);
	themeFilenames = themeFilenames.concat(directory(js.exec_dir + "DDMR_Theme_*.cfg"));

	// Abbreviated theme file names: Get just the filename without the full path,
	// and remove the trailing .cfg
	var abbreviatedThemeFilenames = [];
	for (var i = 0; i < themeFilenames.length; ++i)
	{
		var themeFilename = file_getname(themeFilenames[i]).replace(/\.cfg$/, "");
		abbreviatedThemeFilenames.push(themeFilename);
	}
	// Add an option at the end to let the user type it themselves
	abbreviatedThemeFilenames.push("Type your own filename");

	// Create a context, and look for the current theme filename & set the
	// current index
	var ctx = uifc.list.CTX();
	if (gCfgInfo.cfgOptions.themeFilename.length > 0)
	{
		var themeFilenameUpper = gCfgInfo.cfgOptions.themeFilename.toUpperCase();
		for (var i = 0; i < themeFilenames.length; ++i)
		{
			if (themeFilenames[i].toUpperCase() == themeFilenameUpper)
			{
				ctx.cur = i;
				break;
			}
		}
	}
	// User input
	var chosenThemeFilename = null;
	var selection = uifc.list(WIN_MID, "Theme Filename", abbreviatedThemeFilenames, ctx);
	if (selection == -1)
	{
		// User quit/aborted; do nothing
	}
	// Last item: Let the user input the filename themselves
	else if (selection == abbreviatedThemeFilenames.length-1) 
	{
		var userInput = uifc.input(WIN_MID, "Theme filename", "", 0, K_EDIT);
		if (typeof(userInput) === "string")
			chosenThemeFilename = userInput;
	}
	else
		chosenThemeFilename = file_getname(themeFilenames[selection]);

	return chosenThemeFilename;
}

// Prompts the user to select one of multiple values for an option
//
// Parameters:
//  pPrompt: The prompt text
//  pChoices: An array of the choices
//  pCurrentVal: The current value (to set the index in the menu)
//
// Return value: The user's chosen value, or null if the user aborted
function promptMultipleChoice(pPrompt, pChoices, pCurrentVal)
{
	//uifc.help_text = pHelpText;
	// Create a context object with the current value index
	var currentValUpper = pCurrentVal.toUpperCase();
	var ctx = uifc.list.CTX();
	for (var i = 0; i < pChoices.length; ++i)
	{
		if (pChoices[i].toUpperCase() == currentValUpper)
		{
			ctx.cur = i;
			break;
		}
	}
	// Prompt the user and return their chosen value
	var chosenValue = null;
	//var winMode = WIN_ORG|WIN_MID|WIN_ACT|WIN_ESC;
	var winMode = WIN_MID;
	var userSelection = uifc.list(winMode, pPrompt, pChoices, ctx);
	if (userSelection >= 0 && userSelection < pChoices.length)
		chosenValue = pChoices[userSelection];
	return chosenValue;
}

// Prompts the user Yes/No for a boolean response
//
// Parameters:
//  pQuestion: The question/prompt for the user
//  pInitialVal: Boolean - Whether the initial selection in the menu should
//               be Yes (true) or No (false)
//  pWinMode: Optional window mode bits.  If not specified, WIN_MID will be used.
//
//  Return value: Boolean true (yes), false (no), or null if the user aborted
function promptYesNo(pQuestion, pInitialVal, pWinMode)
{
	var chosenVal = null;
	var winMode = typeof(pWinMode) === "number" ? pWinMode : WIN_MID;
	// Create a CTX to specify the current selected item index
	var ctx = uifc.list.CTX();
	ctx.cur = typeof(pInitialVal) === "boolean" && pInitialVal ? 0 : 1;
	switch (uifc.list(winMode, pQuestion, ["Yes", "No"], ctx))
	{
		case -1: // User quit/aborted - Leave chosenVal as null
			break;
		case 0: // User chose Yes
			chosenVal = true;
			break;
		case 1: // User chose No
			chosenVal = false;
			break;
		default:
			break;
	}
	return chosenVal;
}

// Prompts the user for a quick-validation set. Returns the index of the
// validation set, or -1 if none chosen.
function promptQuickValSet()
{
	var chosenQuickValSetIdx = -1;

	var promptStr = "Quick-Validation Values";
	// Create a context object with the current value index
	var ctx = uifc.list.CTX();
	if (gCfgInfo.cfgOptions.quickUserValSetIndex >= 0 && gCfgInfo.cfgOptions.quickUserValSetIndex < quickValSets.length)
		ctx.cur = gCfgInfo.cfgOptions.quickUserValSetIndex;

	// Choices: If main.ini exists (i.e., Synchronet 3.20+), then get the
	// quick validation sets from that. Otherwise, just make a list of indexes 0-9.
	var menuStrs = [];
	if (file_exists(system.ctrl_dir + "main.ini"))
	{
		var quickValSets = getQuickValidationVals();
		var formatStr = "%-d: SL: %-3d F1: %s";
		for (var i = 0; i < quickValSets.length; ++i)
			menuStrs.push(format(formatStr, i, quickValSets[i].level, getUserValFlagsStr(quickValSets[i].flags1)));
	}
	else
	{
		for (var i = 0; i < 10; ++i)
			menuStrs.push(i.toString());
	}

	// Prompt the user and return their chosen value
	var chosenValue = null;
	//var winMode = WIN_ORG|WIN_MID|WIN_ACT|WIN_ESC;
	var winMode = WIN_MID;
	var userSelection = uifc.list(winMode, promptStr, menuStrs, ctx);
	if (userSelection >= 0 && userSelection < quickValSets.length)
		chosenQuickValSetIdx = userSelection;
	return chosenQuickValSetIdx;
}


///////////////////////////////////////////////////
// Help text functions


// Returns a dictionary of help text, indexed by the option name from the configuration file
function getOptionHelpText()
{
	var optionHelpText = {};
	optionHelpText["listInterfaceStyle"] = "List Interface Style: Either Lightbar or Traditional";

	optionHelpText["reverseListOrder"] = "Reverse List Order: Whether or not message lists are to be shown in reverse order. This is a default for ";
	optionHelpText["reverseListOrder"] += "a user setting.";

	optionHelpText["readerInterfaceStyle"] = "Reader Interface Style: This can be either Scrollable (allowing scrolling up & down) or Traditional. ";
	optionHelpText["readerInterfaceStyle"] += "The scrollable interface only works if the user's terminal supports ANSI. The reader will fall back ";
	optionHelpText["readerInterfaceStyle"] += "to a traditional interface if the usser's terminal doesn't support ANSI.";

	//optionHelpText["readerInterfaceStyleForANSIMessages"] = "";

	optionHelpText["displayBoardInfoInHeader"] = "Display Board Info In Header: Whether or not to display the message group and sub-board lines in the ";
	optionHelpText["displayBoardInfoInHeader"] += "header at the top of the screen (an additional 2 lines).";

	optionHelpText["promptToContinueListingMessages"] = "Prompt to Continue Listing Messages: Whether or not to ask the user if they want to continue listing ";
	optionHelpText["promptToContinueListingMessages"] += "messages after they read a message";

	optionHelpText["promptConfirmReadMessage"] = "Prompt to Confirm Reading Message: Whether or not to prompt the user to confirm to read a message ";
	optionHelpText["promptConfirmReadMessage"] += "when a message is selected from the message list";

	optionHelpText["msgListDisplayTime"] = "Message List Display Time: Whether to display the message import time or the written time in the ";
	optionHelpText["msgListDisplayTime"] += "message lists.  Valid values are imported and written";

	optionHelpText["msgAreaList_lastImportedMsg_time"] = "Message Area List Last Imported Message Time: For the last message time in the message lists, ";
	optionHelpText["msgAreaList_lastImportedMsg_time"] += "whether to use message import time or written time. Valid values are imported and written";

	optionHelpText["startMode"] = "Start Mode: Specifies the default startup mode (Reader/Read or Lister/List)";

	optionHelpText["tabSpaces"] = "Number of Spaces for Tabs: This specifies how many spaces to use for tabs (if a message has tabs)";

	optionHelpText["pauseAfterNewMsgScan"] = "Pause After Nes Message Scan: Whether or not to pause at the end of a newscan";

	optionHelpText["readingPostOnSubBoardInsteadOfGoToNext"] = "Reading Post On Sub-Board Instead Of Go To Next: When reading messages (but not for a newscan, etc.): ";
	optionHelpText["readingPostOnSubBoardInsteadOfGoToNext"] += "Whether or not to ask the user whether to post on the sub-board in reader mode after reading the last ";
	optionHelpText["readingPostOnSubBoardInsteadOfGoToNext"] += "message instead of prompting to go to the next sub-board.  This is  like the stock Synchronet behavior.";

	optionHelpText["displayAvatars"] = "Display Avatars: Whether or not to display user avatars (the small user-specified ANSIs) when reading messages";

	optionHelpText["rightJustifyAvatars"] = "Right-Justify Avatars: Whether or not to display user avatars on the right. If false, they will be displayed ";
	optionHelpText["rightJustifyAvatars"] += "on the left.";

	optionHelpText["msgListSort"] = "Message List Sort: How to sort the message lists - Either Received (by date/time received, which is faster), ";
	optionHelpText["msgListSort"] += "or Written (by date/time written, which takes time due to sorting)";

	optionHelpText["convertYStyleMCIAttrsToSync"] = "Convert Y-Style MCI Attributes to Sync: Whether or not to convert Y-Style MCI attribute/color codes to ";
	optionHelpText["convertYStyleMCIAttrsToSync"] += "Synchronet attribute codes (if disabled, these codes will appear as-is rather than as the colors or ";
	optionHelpText["convertYStyleMCIAttrsToSync"] += "attributes they represent)";

	optionHelpText["prependFowardMsgSubject"] = "Prepend Forwarded Message Subject with \"Fwd\" Whether or not to prepend the subject for forwarded messages ";
	optionHelpText["prependFowardMsgSubject"] += "with \"Fwd: \"";

	optionHelpText["enableIndexedModeMsgListCache"] = "Enable Indexed Mode Message List Cache: For indexed reader mode, whether or not to enable caching the message ";
	optionHelpText["enableIndexedModeMsgListCache"] += "header lists for performance";

	optionHelpText["quickUserValSetIndex"] = "Quick User Val Set Index: An index of a quick-validation set from SCFG > System > Security Options > ";
	optionHelpText["quickUserValSetIndex"] += "Quick-Validation Values to be used by the sysop to quick-validate a local user who has posted a message while ";
	optionHelpText["quickUserValSetIndex"] += "reading the  message. This index is 0-based, as they appear in SCFG. Normally there are 10 quick-validation ";
	optionHelpText["quickUserValSetIndex"] += "values, so valid values for this index are 0 through 9. If you would rather DDMsgReader display a menu of ";
	optionHelpText["quickUserValSetIndex"] += "quick-validation sets, you can set this to an invalid index (such as -1).";

	optionHelpText["saveAllHdrsWhenSavingMsgToBBSPC"] = "Save All Headers When Saving Message to BBS PC: As the sysop, you can save messages to the BBS PC. This ";
	optionHelpText["saveAllHdrsWhenSavingMsgToBBSPC"] += "option specifies whether or not to save all the message headers along with the message. If disabled, ";
	optionHelpText["saveAllHdrsWhenSavingMsgToBBSPC"] += "only a few relevant headers will be saved (such as From, To, Subject, and message time).";

	optionHelpText["msgSaveDir"] = "Default directory to save messages to: The default directory on the BBS machine to save messages to (for the sysop). This can ";
	optionHelpText["msgSaveDir"] += "be blank, in which case, you would need to enter the full path every time when saving a message. If you specify ";
	optionHelpText["msgSaveDir"] += "a directory, it will save there if you only enter a filename, but you can still enter a fully-pathed ";
	optionHelpText["msgSaveDir"] += "filename to save a message in a different directory.";

	optionHelpText["useIndexedModeForNewscan"] = "Used Indexed Mode For Newscan: Whether or not to use indexed mode for message newscans (not for new-to-you ";
	optionHelpText["useIndexedModeForNewscan"] += "scans). This is a default for a user setting. When indexed mode is enabled for newscans, the reader displays ";
	optionHelpText["useIndexedModeForNewscan"] += "a menu showing each sub-board and the number of new messages and total messages in each. When disabled, ";
	optionHelpText["useIndexedModeForNewscan"] += "the reader will do a traditional newscan where it will scan through the sub-boards and go into reader ";
	optionHelpText["useIndexedModeForNewscan"] += "mode when there are new messages in a sub-board.";

	optionHelpText["displayIndexedModeMenuIfNoNewMessages"] = "Display Indexed menu even with no new messages: Whether or not to show the Indexed ";
	optionHelpText["displayIndexedModeMenuIfNoNewMessages"] += "newscan menu even when there are no new messages. This is a default for a user setting.";

	optionHelpText["indexedModeMenuSnapToFirstWithNew"] = "Index menu: Snap to sub-boards w/ new messages: For the indexed newscan sub-board ";
	optionHelpText["indexedModeMenuSnapToFirstWithNew"] += "menu in lightbar mode, whether or not to 'snap' the selected item to the next ";
	optionHelpText["indexedModeMenuSnapToFirstWithNew"] += "sub-board with new messages upon displaying or returning to the indexed newscan ";
	optionHelpText["indexedModeMenuSnapToFirstWithNew"] += "sub-board menu. This is a default for a user setting that users can toggle ";
	optionHelpText["indexedModeMenuSnapToFirstWithNew"] += "for themselves";

	optionHelpText["indexedModeMenuSnapToNextWithNewAftarMarkAllRead"] = "Index menu mark all read: Snap to subs w/ new msgs: For the indexed sub-board menu when doing a newscan, ";
	optionHelpText["indexedModeMenuSnapToNextWithNewAftarMarkAllRead"] += "whether or not to 'snap' the lightbar selected item to the next sub-board with ";
	optionHelpText["indexedModeMenuSnapToNextWithNewAftarMarkAllRead"] += "new messages when the user marks all messages as read in a sub-board on the menu";
	
	optionHelpText["newscanOnlyShowNewMsgs"] = "During a newscan, only show new messages (default for a user setting): Whether or not ";
	optionHelpText["newscanOnlyShowNewMsgs"] += "to only show new messages when the user is doing a newscan. Users can toggle this as ";
	optionHelpText["newscanOnlyShowNewMsgs"] += "they like.";

	optionHelpText["promptDelPersonalEmailAfterReply"] = "Personal email: Prompt to delete after reply: When reading personal email, ";
	optionHelpText["promptDelPersonalEmailAfterReply"] += "whether or not to propmt the user if they want to delete a message after ";
	optionHelpText["promptDelPersonalEmailAfterReply"] +=  "replying to it. This is a defafult for a user setting.";
	
	optionHelpText["subBoardChangeSorting"] = "Sorting for sub-board change: This is a sorting option for the sub-boards ";
	optionHelpText["subBoardChangeSorting"] += "when the user changes to another sub-board. The options are None (no sorting, ";
	optionHelpText["subBoardChangeSorting"] += "in the order defined in SCFG), Alphabetical, LatestMsgDateOldestFirst (by message ";
	optionHelpText["subBoardChangeSorting"] += "date, oldest first), or LatestMsgDateNewestFirst (by message date, newest first). ";
	optionHelpText["subBoardChangeSorting"] += "This is a default for a user setting, which users can change for themselves.";

	optionHelpText["indexedModeNewscanOnlyShowSubsWithNewMsgs"] = "Index newscan: Only show subs w/ new msgs: For indexed newscan mode, ";
	optionHelpText["indexedModeNewscanOnlyShowSubsWithNewMsgs"] += "whether to only show sub-boards with new messages. This is a ";
	optionHelpText["indexedModeNewscanOnlyShowSubsWithNewMsgs"] += "default for a user setting, which users can change for themselves.";

	optionHelpText["showUserResponsesInTallyInfo"] = "Include user responses in tally info: For poll messages or messages with ";
	optionHelpText["showUserResponsesInTallyInfo"] += "upvotes/downvotes, when showing vote tally info (a sysop function), whether or not ";
	optionHelpText["showUserResponsesInTallyInfo"] += "to show each users' responses. If false, it will just show the names of who voted on ";
	optionHelpText["showUserResponsesInTallyInfo"] += "the message (and when).";

	optionHelpText["DDMsgAreaChooser"] = "DDMsgAreaChooser full path & filename: The full path & filename of DDMsgAreaChooser.js, ";
	optionHelpText["DDMsgAreaChooser"] += "if it's not in sbbs/xtrn/DDAreaChoosers. If DDMsgAreaChooser.js is in ";
	optionHelpText["DDMsgAreaChooser"] += "sbbs/xtrn/DDAreaChoosers, this setting can be left empty. DDAreaChooser.js is used for ";
	optionHelpText["DDMsgAreaChooser"] += "allowing the user to change to a different message sub-board.";

	optionHelpText["themeFilename"] = "Theme filename: The name of a file for a color theme to use";

	// Word-wrap the help text items
	for (var prop in optionHelpText)
		optionHelpText[prop] = word_wrap(optionHelpText[prop], gHelpWrapWidth);

	return optionHelpText;
}

// Returns help text for the main configuration screen (behavior settings)
//
// Parameters:
//  pOptionHelpText: An object of help text for each option, indexed by the option name from the configuration file
//  pCfgOptProps: An array specifying the properties to include in the help text and their order
//
// Return value: Help text for the main configuration screen (behavior settings)
function getMainHelp(pOptionHelpText, pCfgOptProps)
{
	var helpText = "This screen allows you to configure behavior options for Digital Distortion Message Reader.\r\n\r\n";
	for (var i = 0; i < pCfgOptProps.length; ++i)
	{
		var optName = pCfgOptProps[i];
		//helpText += pOptionHelpText[optName] + "\r\n\r\n";
		helpText += pOptionHelpText[optName] + "\r\n";
	}
	return word_wrap(helpText, gHelpWrapWidth);
}


///////////////////////////////////////////////////
// Non-UI utility functions

// Returns an array of the quick-validation sets configured in
// SCFG > System > Security > Quick-Validation Values.  This reads
// from main.ini, which exists with Synchronet 3.20 and newer.
// In SCFG:
//
// Level                 60     |
// Flag Set #1                  |
// Flag Set #2                  |
// Flag Set #3                  |
// Flag Set #4                  |
// Exemptions                   |
// Restrictions                 |
// Extend Expiration     0 days |
// Additional Credits    0      |
//
// Each object in the returned array will have the following properties:
//  level (numeric)
//  expire
//  flags1
//  flags2
//  flags3
//  flags4
//  credits
//  exemptions
//  restrictions
function getQuickValidationVals()
{
	var validationValSets = [];
	// In SCFG > System > Security > Quick-Validation Values, there are 10 sets of
	// validation values.  These are in main.ini as [valset:0] through [valset:9]
	// This reads from main.ini, which exists with Synchronet 3.20 and newer.
	//system.version_num >= 32000
	var mainIniFile = new File(system.ctrl_dir + "main.ini");
	if (mainIniFile.open("r"))
	{
		for (var i = 0; i < 10; ++i)
		{
			// Flags:
			// AZ is 0x2000001
			// A is 1
			// Z is 0x2000000
			var valSection = mainIniFile.iniGetObject(format("valset:%d", i));
			if (valSection != null)
				validationValSets.push(valSection);
		}
		mainIniFile.close();
	}
	return validationValSets;
}
// Generates a string based on user quick-validation flags
//
// Parameters:
//  pFlags: A bitfield of user quick-validation flags
//
// Return value: A string representing the quick-validation flags (as would appear in SCFG)
function getUserValFlagsStr(pFlags)
{
	var flagLetters = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	var flagsStr = "";
	for (var i = 0; i < flagLetters.length; ++i)
		flagsStr += (Boolean((1 << i) & pFlags) ? flagLetters[i] : " ");
	return truncsp(flagsStr);
}

// Parses command-line arguments & sets any applicable values
function parseCmdLineArgs()
{
	for (var i = 0; i < argv.length; ++i)
	{
		var argUpper = argv[i].toUpperCase();
		if (argUpper == "-SAVE_TO_MODS")
			gAlwaysSaveCfgInOriginalDir = false;
	}
}

// Reads the DDMsgReader configuration file
//
// Return value: An object with the following properties:
//               cfgFilename: The full path & filename of the DDMsgReader configuration file that was read
//               cfgOptions: A dictionary of the configuration options and their values
function readDDMsgReaderCfgFile()
{
	var retObj = {
		cfgFilename: "",
		cfgOptions: {}
	};

	// Determine the location of the configuration file.  First look for it
	// in the sbbs/mods directory, then sbbs/ctrl, then in the same directory
	// as this script.
	var cfgFilename = file_cfgname(system.mods_dir, gDDMRCfgFileName);
	if (!file_exists(cfgFilename))
		cfgFilename = file_cfgname(system.ctrl_dir, gDDMRCfgFileName);
	if (!file_exists(cfgFilename))
		cfgFilename = file_cfgname(js.exec_dir, gDDMRCfgFileName);
	// If the configuration file hasn't been found, look to see if there's a DDMsgReader.example.cfg file
	// available in the same directory 
	if (!file_exists(cfgFilename))
	{
		var exampleFileName = file_cfgname(js.exec_dir, "DDMsgReader.example.cfg");
		if (file_exists(exampleFileName))
			cfgFilename = exampleFileName;
	}

	retObj.cfgFilename = cfgFilename;

	// Open and read the configuration file
	var cfgFile = new File(cfgFilename);
	if (cfgFile.open("r"))
	{
		retObj.cfgOptions = cfgFile.iniGetObject();
		cfgFile.close();
	}

	// In case some settings weren't loaded, add defaults
	if (!retObj.cfgOptions.hasOwnProperty("listInterfaceStyle"))
		retObj.cfgOptions.listInterfaceStyle = "Lightbar";
	if (!retObj.cfgOptions.hasOwnProperty("reverseListOrder"))
		retObj.cfgOptions.reverseListOrder = false;
	if (!retObj.cfgOptions.hasOwnProperty("readerInterfaceStyle"))
		retObj.cfgOptions.readerInterfaceStyle = "Scrollable";
	if (!retObj.cfgOptions.hasOwnProperty("readerInterfaceStyleForANSIMessages"))
		retObj.cfgOptions.readerInterfaceStyleForANSIMessages = "Scrollable";
	if (!retObj.cfgOptions.hasOwnProperty("displayBoardInfoInHeader"))
		retObj.cfgOptions.displayBoardInfoInHeader = false;
	if (!retObj.cfgOptions.hasOwnProperty("promptToContinueListingMessages"))
		retObj.cfgOptions.promptToContinueListingMessages = false;
	if (!retObj.cfgOptions.hasOwnProperty("promptConfirmReadMessage"))
		retObj.cfgOptions.promptConfirmReadMessage = false;
	if (!retObj.cfgOptions.hasOwnProperty("msgListDisplayTime"))
		retObj.cfgOptions.msgListDisplayTime = "written";
	if (!retObj.cfgOptions.hasOwnProperty("msgAreaList_lastImportedMsg_time"))
		retObj.cfgOptions.msgAreaList_lastImportedMsg_time = "written";
	if (!retObj.cfgOptions.hasOwnProperty("startMode"))
		retObj.cfgOptions.startMode = "Reader";
	if (!retObj.cfgOptions.hasOwnProperty("tabSpaces"))
		retObj.cfgOptions.tabSpaces = 3;
	if (!retObj.cfgOptions.hasOwnProperty("pauseAfterNewMsgScan"))
		retObj.cfgOptions.pauseAfterNewMsgScan = true;
	if (!retObj.cfgOptions.hasOwnProperty("readingPostOnSubBoardInsteadOfGoToNext"))
		retObj.cfgOptions.readingPostOnSubBoardInsteadOfGoToNext = false;
	if (!retObj.cfgOptions.hasOwnProperty("displayAvatars"))
		retObj.cfgOptions.displayAvatars = true;
	if (!retObj.cfgOptions.hasOwnProperty("rightJustifyAvatars"))
		retObj.cfgOptions.rightJustifyAvatars = true;
	if (!retObj.cfgOptions.hasOwnProperty("msgListSort"))
		retObj.cfgOptions.msgListSort = "Received";
	if (!retObj.cfgOptions.hasOwnProperty("convertYStyleMCIAttrsToSync"))
		retObj.cfgOptions.convertYStyleMCIAttrsToSync = true;
	if (!retObj.cfgOptions.hasOwnProperty("prependFowardMsgSubject"))
		retObj.cfgOptions.prependFowardMsgSubject = true;
	if (!retObj.cfgOptions.hasOwnProperty("enableIndexedModeMsgListCache"))
		retObj.cfgOptions.enableIndexedModeMsgListCache = true;
	if (!retObj.cfgOptions.hasOwnProperty("quickUserValSetIndex"))
		retObj.cfgOptions.quickUserValSetIndex = -1;
	if (!retObj.cfgOptions.hasOwnProperty("saveAllHdrsWhenSavingMsgToBBSPC"))
		retObj.cfgOptions.saveAllHdrsWhenSavingMsgToBBSPC = false;
	if (!retObj.cfgOptions.hasOwnProperty("msgSaveDir"))
		retObj.cfgOptions.msgSaveDir = "";
	if (!retObj.cfgOptions.hasOwnProperty("useIndexedModeForNewscan"))
		retObj.cfgOptions.useIndexedModeForNewscan = false;
	if (!retObj.cfgOptions.hasOwnProperty("displayIndexedModeMenuIfNoNewMessages"))
		retObj.cfgOptions.displayIndexedModeMenuIfNoNewMessages = false;
	if (!retObj.cfgOptions.hasOwnProperty("enableIndexedModeMsgListCache"))
		retObj.cfgOptions.enableIndexedModeMsgListCache = true;
	if (!retObj.cfgOptions.hasOwnProperty("newscanOnlyShowNewMsgs"))
		retObj.cfgOptions.newscanOnlyShowNewMsgs = true;
	if (!retObj.cfgOptions.hasOwnProperty("indexedModeMenuSnapToFirstWithNew"))
		retObj.cfgOptions.indexedModeMenuSnapToFirstWithNew = false;
	if (!retObj.cfgOptions.hasOwnProperty("indexedModeMenuSnapToNextWithNewAftarMarkAllRead"))
		retObj.cfgOptions.indexedModeMenuSnapToNextWithNewAftarMarkAllRead = true;
	if (!retObj.cfgOptions.hasOwnProperty("promptDelPersonalEmailAfterReply"))
		retObj.cfgOptions.promptDelPersonalEmailAfterReply = false;
	if (!retObj.cfgOptions.hasOwnProperty("subBoardChangeSorting"))
		retObj.cfgOptions.subBoardChangeSorting = "None";
	if (!retObj.cfgOptions.hasOwnProperty("indexedModeNewscanOnlyShowSubsWithNewMsgs"))
		retObj.cfgOptions.indexedModeNewscanOnlyShowSubsWithNewMsgs = false;
	if (!retObj.cfgOptions.hasOwnProperty("showUserResponsesInTallyInfo"))
		retObj.cfgOptions.showUserResponsesInTallyInfo = false;
	if (!retObj.cfgOptions.hasOwnProperty("DDMsgAreaChooser"))
		retObj.cfgOptions.DDMsgAreaChooser = "";
	if (!retObj.cfgOptions.hasOwnProperty("themeFilename"))
		retObj.cfgOptions.themeFilename = "DefaultTheme.cfg";

	return retObj;
}

// Saves the DDMsgReader configuration file using the settings in gCfgInfo
//
// Return value: An object with the following properties:
//               saveSucceeded: Boolean - Whether or not the save fully succeeded
//               savedToModsDir: Boolean - Whether or not the .cfg file was saved to the mods directory
function saveDDMsgReaderCfgFile()
{
	var retObj = {
		saveSucceeded: false,
		savedToModsDir: false
	};

	// If the configuration file was in the same directory as this configurator,
	// then deal with it appropriately.
	var cfgFilename = gCfgInfo.cfgFilename;
	if (gCfgInfo.cfgFilename.indexOf(js.exec_dir) > -1)
	{
		if (gAlwaysSaveCfgInOriginalDir) // Always save config in the original dir
		{
			// If the read configuration file was DDMsgReader.example.cfg, copy it
			// to DDMsgReader.cfg.
			if (cfgFilename.lastIndexOf("DDMsgReader.example.cfg") > -1)
			{
				var oldCfgFilename = gCfgInfo.cfgFilename;
				cfgFilename = gCfgInfo.cfgFilename.replace("DDMsgReader.example.cfg", "DDMsgReader.cfg");
				if (!file_copy(oldCfgFilename, cfgFilename))
					return retObj;
			}
		}
		else
		{
			// Copy to sbbs/mods
			cfgFilename = system.mods_dir + gDDMRCfgFileName;
			if (!file_copy(gCfgInfo.cfgFilename, cfgFilename))
				return retObj;
			retObj.savedToModsDir = true;
		}
	}

	var cfgFile = new File(cfgFilename);
	var openMode = (file_exists(cfgFilename) ? "r+" : "w");
	if (cfgFile.open(openMode))
	{
		for (var settingName in gCfgInfo.cfgOptions)
			cfgFile.iniSetValue(null, settingName, gCfgInfo.cfgOptions[settingName]);
		cfgFile.close();
		retObj.saveSucceeded = true;
	}

	return retObj;
}
