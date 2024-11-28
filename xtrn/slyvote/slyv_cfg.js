// This is a menu-driven configuration program/script for SlyVote.  If you're running SlyVote from
// xtrn/slyvote (the standard location), then any changes are saved to slyvote.cfg in
// sbbs/mods, so that custom changes don't get overridden due to an update.
// If you have SlyVote in a directory other than xtrn/slyvote, then the changes to
// slyvote.cfg will be saved in that directory (assuming you're running slyv_cfg.js from
// that same directory).
//
// If you're running SlyVote from xtrn/slyvote (the standard location) and you want
// to save the configuration file there (rather than sbbs/mods), you can use one of the
// following command-line options: noMods, -noMods, no_mods, or -no_mods

require("sbbsdefs.js", "P_NONE");
require("uifcdefs.js", "UIFC_INMSG");


if (!uifc.init("SlyVote 1.16 Configurator"))
{
	print("Failed to initialize uifc");
	exit(1);
}
js.on_exit("uifc.bail()");


var gCfgFilename = "slyvote.cfg";

// Read the configuration file
var gCfgInfo = readCfgFile();
if (!gCfgInfo.successfullyOpened)
{
	writeln("");
	writeln("* Could not open the configuration file!");
	writeln("");
	exit(1);
}

var gHelpWrapWidth = uifc.screen_width - 10;
// A string to use in the main menu for enabled sub-boards when there are sub-boards configured
var gAllowedSubsCfgMenuText = "<Subs set>";

// When saving the configuration file, always save it in the same directory
// as SlyVote (or this script); don't copy to mods
var gAlwaysSaveCfgInOriginalDir = false;

// Parse command-line arguments
parseCmdLineArgs();

// Read the SlyVote configuration file
var gCfgInfo = readCfgFile();


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
				var saveRetObj = saveCfgFile();
				// Show an error if failed to save the settings.  If succeeded, only show
				// a message if settings were saved to the mods directory.
				if (!saveRetObj.saveSucceeded)
					uifc.msg("Failed to save settings!");
				else
				{
					if (saveRetObj.savedToModsDir)
						uifc.msg("Changes were successfully saved (to the mods dir)");
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

// Parses command-line arguments & sets any applicable values
function parseCmdLineArgs()
{
	for (var i = 0; i < argv.length; ++i)
	{
		var argUpper = argv[i].toUpperCase();
		if (argUpper == "NOMODS" || argUpper == "NO_MODS" || argUpper == "-NOMODS" || argUpper == "-NO_MODS")
			gAlwaysSaveCfgInOriginalDir = true;
	}
}

function doMainMenu()
{
	// For menu item text formatting
	var itemTextMaxLen = 50;

	// Configuration option object properties
	// cfgOptProps must correspond exactly with optionStrs & menuItems
	var cfgOptProps = [
		"showAvatars", // Boolean
		"useAllAvailableSubBoards", // Boolean
		"startupSubBoardCode", // String
		"subBoardCodes" // String (comma-separated list)
	];
	// Strings for the options to display on the menu
	var optionStrs = [
		"Show avatars in poll messages",
		"Use all available sub-boards",
		"Startup sub-board",
		"List of sub-boards to allow"
	];
	// Build an array of formatted string to be displayed on the menu
	// (the value formatting will depend on the variable type)
	var menuItems = [];
	for (var i = 0; i < cfgOptProps.length; ++i)
	{
		var propName = cfgOptProps[i];
		if (propName == "subBoardCodes")
			menuItems.push(formatCfgMenuText(itemTextMaxLen, optionStrs[i], gCfgInfo.cfgOptions.subBoardCodes != "" ? gAllowedSubsCfgMenuText : ""));
		else
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
	var menuTitle = "SlyVote Configuration";
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
			if (optName == "startupSubBoardCode")
			{
				// Startup sub-board code
				// displayMsgGrpsAndChooseSubBoard() will return undefined if the user aborted
				var subCode = displayMsgGrpsAndChooseSubBoard();
				if (typeof(subCode) === "string")
				{
					if (subCode != "" && subCode != gCfgInfo.cfgOptions.startupSubBoardCode)
					{
						anyOptionChanged = true;
						gCfgInfo.cfgOptions.startupSubBoardCode = subCode;
						menuItems[optionMenuSelection] = formatCfgMenuText(itemTextMaxLen, optionStrs[optionMenuSelection], gCfgInfo.cfgOptions[optName]);
					}
				}
				else
				{
					if (gCfgInfo.cfgOptions.startupSubBoardCode != "")
					{
						var userChoice = promptYesNo("Clear the startup sub-board?", true, WIN_ORG|WIN_MID|WIN_ACT|WIN_ESC);
						if (typeof(userChoice) === "boolean" && userChoice)
						{
							anyOptionChanged = true;
							gCfgInfo.cfgOptions.startupSubBoardCode = "";
							menuItems[optionMenuSelection] = formatCfgMenuText(itemTextMaxLen, optionStrs[optionMenuSelection], gCfgInfo.cfgOptions[optName]);
						}
					}
				}
			}
			else if (optName == "subBoardCodes")
			{
				var subCodes = displayMsgGrpsAndToggleSubBoards(gCfgInfo.cfgOptions.subBoardCodes);
				if (typeof(subCodes) === "string" && subCodes != gCfgInfo.cfgOptions.subBoardCodes)
				{
					anyOptionChanged = true;
					gCfgInfo.cfgOptions.subBoardCodes = subCodes;
					menuItems[optionMenuSelection] = formatCfgMenuText(itemTextMaxLen, optionStrs[optionMenuSelection], subCodes != "" ? gAllowedSubsCfgMenuText : "")
				}
			}
			else if (itemType === "boolean")
			{
				anyOptionChanged = true;
				gCfgInfo.cfgOptions[optName] = !gCfgInfo.cfgOptions[optName];
				menuItems[optionMenuSelection] = formatCfgMenuText(itemTextMaxLen, optionStrs[optionMenuSelection], gCfgInfo.cfgOptions[optName]);
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

// Starting with the message groups, allows the user to choose a single sub-board
//
// Return value: The internal code of the chosen sub-board, or undefined if none was chosen
function displayMsgGrpsAndChooseSubBoard()
{
	var subCode = undefined;

	// Generate a list of message groups and the number of sub-boards they each have
	var msgGroups = [];
	var grpNameLen = 35;
	var numSubBoardsLen = 10;
	var formatStr = "%-" + grpNameLen + "s %" + numSubBoardsLen + "d";
	for (var grpIdx = 0; grpIdx < msg_area.grp_list.length; ++grpIdx)
		msgGroups.push(format(formatStr, msg_area.grp_list[grpIdx].description, msg_area.grp_list[grpIdx].sub_list.length));

	// Create a CTX to specify the current selected item index
	if (displayMsgGrpsAndChooseSubBoard.ctx == undefined)
		displayMsgGrpsAndChooseSubBoard.ctx = uifc.list.CTX();
	// Selection
	var winMode = WIN_ORG|WIN_MID|WIN_ACT|WIN_ESC;
	var menuTitle = format("%-" + grpNameLen + "s %" + numSubBoardsLen + "s", "Message Groups", "Sub-boards");
	var continueOn = true;
	while (continueOn && !js.terminated)
	{
		//uifc.help_text = displayMsgGrpsAndChooseSubBoard.mainScreenHelp;
		var selectedMsgGrpIdx = uifc.list(winMode, menuTitle, msgGroups, displayMsgGrpsAndChooseSubBoard.ctx);
		if (selectedMsgGrpIdx == -1) // ESC
		{
			subCode = undefined;
			continueOn = false;
		}
		else
		{
			var chosenSubCode = chooseSubBoardFromMsgGroup(selectedMsgGrpIdx);
			if (typeof(chosenSubCode) === "string" && chosenSubCode != "")
			{
				// chooseSubBoardFromMsgGroup() will confirm with the user
				subCode = chosenSubCode;
				continueOn = false;
				/*
				var userChoice = promptYesNo(msg_area.grp_list[grpIdx].name + ": Are you sure?", true, WIN_ORG|WIN_MID|WIN_ACT|WIN_ESC);
				if (typeof(userChoice) === "boolean" && userChoice)
				{
					subCode = chosenSubCode;
					continueOn = false;
				}
				*/
			}
		}
	}

	return subCode;
}
// Lets the user choose a sub-board of a message group
//
// Parameters:
//  pGrpIdx: The index of the message group
//
// Return value: The internal code of the chosen sub-board (string), or undefined if none was chosen
function chooseSubBoardFromMsgGroup(pGrpIdx)
{
	var subCode = undefined;

	// Generate a list of the sub-boards in the message group and the number of sub-boards they each have
	var maxSubDescLen = 50;
	var subBoards = [];
	for (var subIdx = 0; subIdx < msg_area.grp_list[pGrpIdx].sub_list.length; ++subIdx)
		subBoards.push(msg_area.grp_list[pGrpIdx].sub_list[subIdx].description.substr(0, maxSubDescLen));

	// Create a CTX to specify the current selected item index
	if (chooseSubBoardFromMsgGroup.ctx == undefined)
		chooseSubBoardFromMsgGroup.ctx = uifc.list.CTX();
	// Selection
	var winMode = WIN_ORG|WIN_MID|WIN_ACT|WIN_ESC;
	var menuTitle = format("%s Sub-boards (%d)", msg_area.grp_list[pGrpIdx].name, msg_area.grp_list[pGrpIdx].sub_list.length);
	var continueOn = true;
	while (continueOn && !js.terminated)
	{
		//uifc.help_text = chooseSubBoardFromMsgGroup.mainScreenHelp;
		var selectedMsgGrpIdx = uifc.list(winMode, menuTitle, subBoards, chooseSubBoardFromMsgGroup.ctx);
		if (selectedMsgGrpIdx == -1) // ESC
		{
			subCode = undefined;
			continueOn = false;
		}
		else
		{
			var chosenSubCode = msg_area.grp_list[pGrpIdx].sub_list[selectedMsgGrpIdx].code;
			var userChoice = promptYesNo(msg_area.grp_list[pGrpIdx].sub_list[selectedMsgGrpIdx].name + ": Are you sure?", true, WIN_ORG|WIN_MID|WIN_ACT|WIN_ESC);
			if (typeof(userChoice) === "boolean" && userChoice)
			{
				subCode = chosenSubCode;
				continueOn = false;
			}
		}
	}

	return subCode;
}

// Starting with the message groups, allows the user to toggle multiple sub-boards on/off
//
// Parameters:
//  pCurrentSubCodeList: String - A comma-separated list of all current enabled sub-board codes
//
// Return value: A comma-separated list of internal codes of all enabled sub-boards, or an empty string if there are none
function displayMsgGrpsAndToggleSubBoards(pCurrentSubCodeList)
{
	var subCodes = typeof(pCurrentSubCodeList) === "string" ? pCurrentSubCodeList : "";

	// Generate a list of message groups and the number of sub-boards they each have
	var msgGroups = [];
	var grpNameLen = 35;
	var numSubBoardsLen = 10;
	var formatStr = "%-" + grpNameLen + "s %" + numSubBoardsLen + "d";
	for (var grpIdx = 0; grpIdx < msg_area.grp_list.length; ++grpIdx)
		msgGroups.push(format(formatStr, msg_area.grp_list[grpIdx].description, msg_area.grp_list[grpIdx].sub_list.length));

	// Create a CTX to specify the current selected item index
	if (displayMsgGrpsAndChooseSubBoard.ctx == undefined)
		displayMsgGrpsAndChooseSubBoard.ctx = uifc.list.CTX();
	// Selection
	var winMode = WIN_ORG|WIN_MID|WIN_ACT|WIN_ESC;
	var menuTitle = format("%-" + grpNameLen + "s %" + numSubBoardsLen + "s", "Message Groups", "Sub-boards");
	var continueOn = true;
	while (continueOn && !js.terminated)
	{
		//uifc.help_text = displayMsgGrpsAndChooseSubBoard.mainScreenHelp;
		var selectedMsgGrpIdx = uifc.list(winMode, menuTitle, msgGroups, displayMsgGrpsAndChooseSubBoard.ctx);
		if (selectedMsgGrpIdx == -1) // ESC
			continueOn = false;
		else
		{
			var toggledSubCodes = chooseMultipleSubBoardsFromMsgGroup(selectedMsgGrpIdx, subCodes);
			if (typeof(toggledSubCodes) === "string")
				subCodes = toggledSubCodes;
		}
	}

	return subCodes;
}
// Lets the user choose multiple sub-boards from a message group
//
// Parameters:
//  pGrpIdx: The index of the message group
//  pCurrentSubCodeList: String - A comma-separated list of all current enabled sub-board codes
//
// Return value: A comma-separated list of internal codes of all enabled sub-boards, or an empty string if there are none
function chooseMultipleSubBoardsFromMsgGroup(pGrpIdx, pCurrentSubCodeList)
{
	// Split pCurrentSubCodeList on commas to make an array of sub-board codes
	var currentEnabledSubCodes = {};
	if (typeof(pCurrentSubCodeList) === "string")
	{
		var enabledSubCodes = pCurrentSubCodeList.split(",");
		for (var i = 0; i < enabledSubCodes.length; ++i)
		{
			var subCode = enabledSubCodes[i];
			currentEnabledSubCodes[subCode] = true;
		}
	}

	// Generate a list of the sub-boards in the message group and the number of sub-boards they each have
	var subDescLen = 30;
	var enabledLen = 7;
	var formatStr = "%-" + subDescLen + "s %" + enabledLen + "s";
	var subBoards = [];
	for (var subIdx = 0; subIdx < msg_area.grp_list[pGrpIdx].sub_list.length; ++subIdx)
	{
		var subIsEnabled = currentEnabledSubCodes.hasOwnProperty(msg_area.grp_list[pGrpIdx].sub_list[subIdx].code);
		subBoards.push(format(formatStr, msg_area.grp_list[pGrpIdx].sub_list[subIdx].description.substr(0, subDescLen), subIsEnabled ? "Yes" : "No"));
	}

	// Create a CTX to specify the current selected item index
	if (chooseSubBoardFromMsgGroup.ctx == undefined)
		chooseSubBoardFromMsgGroup.ctx = uifc.list.CTX();
	// Selection
	var winMode = WIN_ORG|WIN_MID|WIN_ACT|WIN_ESC;
	var subBoardInfoStr = format("%s Sub-boards (%d)", msg_area.grp_list[pGrpIdx].name, msg_area.grp_list[pGrpIdx].sub_list.length);
	var menuTitle = format(formatStr, subBoardInfoStr.substr(0, subDescLen), "Enabled");
	var continueOn = true;
	while (continueOn && !js.terminated)
	{
		//uifc.help_text = chooseSubBoardFromMsgGroup.mainScreenHelp;
		var selectedSubIdx = uifc.list(winMode, menuTitle, subBoards, chooseSubBoardFromMsgGroup.ctx);
		if (selectedSubIdx == -1) // ESC
			continueOn = false;
		else
		{
			// Toggle the sub-board
			if (currentEnabledSubCodes.hasOwnProperty(msg_area.grp_list[pGrpIdx].sub_list[selectedSubIdx].code))
			{
				delete currentEnabledSubCodes[msg_area.grp_list[pGrpIdx].sub_list[selectedSubIdx].code];
				subBoards[selectedSubIdx] = format(formatStr, msg_area.grp_list[pGrpIdx].sub_list[selectedSubIdx].description, "No");
			}
			else
			{
				currentEnabledSubCodes[msg_area.grp_list[pGrpIdx].sub_list[selectedSubIdx].code] = true;
				subBoards[selectedSubIdx] = format(formatStr, msg_area.grp_list[pGrpIdx].sub_list[selectedSubIdx].description, "Yes");
			}
		}
	}

	// Convert the object of all enabled sub-boards back to a comma-separated list (string) and return it
	var subCodes = "";
	for (var subCode in currentEnabledSubCodes)
		subCodes += subCode + ",";
	subCodes = subCodes.replace(/,$/, ""); // Remove trailing comma
	subCodes = subCodes.replace(/^,/, ""); // Remove initial comma if there is one
	return subCodes;
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


// Reads the configuration file
function readCfgFile()
{
	var retObj = {
		cfgFilename: "",
		successfullyOpened: false,
		cfgOptions: {
			showAvatars: true,
			useAllAvailableSubBoards: true,
			startupSubBoardCode: "",
			subBoardCodes: ""
		}
	};

	// Determine the location of the configuration file.  First look for it
	// in the sbbs/mods directory, then sbbs/ctrl, then in the same directory
	// as this script.
	retObj.cfgFilename = file_cfgname(system.mods_dir, gCfgFilename);
	if (!file_exists(retObj.cfgFilename))
		retObj.cfgFilename = file_cfgname(system.ctrl_dir, gCfgFilename);
	if (!file_exists(retObj.cfgFilename))
		retObj.cfgFilename = file_cfgname(js.exec_dir, gCfgFilename);
	var cfgFile = new File(retObj.cfgFilename);
	if (cfgFile.open("r"))
	{
		var settingsObj = cfgFile.iniGetObject();
		cfgFile.close();
		retObj.successfullyOpened = true;

		// Copy the values into retObj.cfgOptions
		if (settingsObj.hasOwnProperty("showAvatars") && typeof(settingsObj.showAvatars) === "boolean")
			retObj.cfgOptions.showAvatars = settingsObj.showAvatars;
		if (settingsObj.hasOwnProperty("useAllAvailableSubBoards") && typeof(settingsObj.useAllAvailableSubBoards) === "boolean")
			retObj.cfgOptions.useAllAvailableSubBoards = settingsObj.useAllAvailableSubBoards;
		if (settingsObj.hasOwnProperty("startupSubBoardCode") && typeof(settingsObj.startupSubBoardCode) === "string")
			retObj.cfgOptions.startupSubBoardCode = settingsObj.startupSubBoardCode;
		if (settingsObj.hasOwnProperty("subBoardCodes") && typeof(settingsObj.subBoardCodes) === "string")
			retObj.cfgOptions.subBoardCodes = settingsObj.subBoardCodes;
	}
	return retObj;
}

// Saves the configuration file using the settings in gCfgInfo
//
// Return value: An object with the following properties:
//               saveSucceeded: Boolean - Whether or not the save fully succeeded
//               savedToModsDir: Boolean - Whether or not the .cfg file was saved to the mods directory
function saveCfgFile()
{
	var retObj = {
		saveSucceeded: false,
		savedToModsDir: false
	};

	// If the configuration file was loaded from the standard location in
	// the Git repository (xtrn/slyvote), and the option to always save
	// in the original directory is not set, then the configuration file
	// should be copied to sbbs/mods to avoid custom settings being overwritten
	// with an update.
	var defaultDirRE = new RegExp("xtrn[\\\\/]slyvote[\\\\/]" + gCfgFilename + "$");
	var cfgFilename = gCfgInfo.cfgFilename;
	if (defaultDirRE.test(cfgFilename) && !gAlwaysSaveCfgInOriginalDir)
	{
		cfgFilename = system.mods_dir + gCfgFilename;
		if (!file_copy(gCfgInfo.cfgFilename, cfgFilename))
			return false;
		retObj.savedToModsDir = true;
	}

	var cfgFile = new File(cfgFilename);
	if (cfgFile.open("r+")) // Reading and writing (file must exist)
	{
		for (var settingName in gCfgInfo.cfgOptions)
			cfgFile.iniSetValue(null, settingName, gCfgInfo.cfgOptions[settingName]);
		cfgFile.close();
		retObj.saveSucceeded = true;
	}

	return retObj;
}



///////////////////////////////////////////////////
// Help text functions


// Returns a dictionary of help text, indexed by the option name from the configuration file
function getOptionHelpText()
{
	var optionHelpText = {};
	optionHelpText["showAvatars"] = "Show avatars in poll messages: Whether or not to show user avatar ANSIs ";
	optionHelpText["showAvatars"] += "in the header when reading/viewing polls";

	optionHelpText["useAllAvailableSubBoards"] = "Use all available sub-boards: Whether or not to let the user access all sub-boards ";
	optionHelpText["useAllAvailableSubBoards"] += "from SlyVote. For instance, you might have only certain sub-boards that allow voting, ";
	optionHelpText["useAllAvailableSubBoards"] += "or you might set up only one designated sub-board for polls, if desired";

	optionHelpText["startupSubBoardCode"] = "Startup sub-board: The initial sub-board to use on startup. This is saved in the ";
	optionHelpText["startupSubBoardCode"] += "configuration file as the internal sub-board code.";

	optionHelpText["subBoardCodes"] = "List of sub-boards to allow: If not using all sub-boards, this is a set of sub-boards ";
	optionHelpText["subBoardCodes"] += "to give users access to in SlyVote. For instance, you might have only certain sub-boards ";
	optionHelpText["subBoardCodes"] += "that allow voting, or you might set up only one designated sub-board for polls, if desired. ";
	optionHelpText["subBoardCodes"] += "This is saved in the configuration file as a comma-separated list of internal sub-board ";
	optionHelpText["subBoardCodes"] += "codes";

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
	var helpText = "This screen allows you to configure the options for SlyVote.\r\n\r\n";
	for (var i = 0; i < pCfgOptProps.length; ++i)
	{
		var optName = pCfgOptProps[i];
		//helpText += pOptionHelpText[optName] + "\r\n\r\n";
		helpText += pOptionHelpText[optName] + "\r\n";
	}
	return word_wrap(helpText, gHelpWrapWidth);
}