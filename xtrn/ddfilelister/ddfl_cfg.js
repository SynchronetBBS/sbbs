// This is a menu-driven configurator for Digital Distortion File Lister.  If you're running
// DDFileLister xtrn/ddfilelister (the standard location), then any changes are saved to
// ddfilelister.cfg in sbbs/mods, so that custom changes don't get overridden due to an update.
// If you have ddfilelister in a directory other than xtrn/ddfilelister, then the changes to
// ddfilelister.cfg will be saved in that directory (assuming you're running ddmr_cfg.js from
// that same directory).
// Currently for ddfilelister 2.29.
//
// If you're running ddfilelister from xtrn/ddfilelister (the standard location) and you want
// to save the configuration file there (rather than sbbs/mods), you can use one of the
// following command-line options: noMods, -noMods, no_mods, or -no_mods

"use strict";


require("sbbsdefs.js", "P_NONE");
require("uifcdefs.js", "UIFC_INMSG");


if (!uifc.init("DigDist. File Lister 2.29 Configurator"))
{
	print("Failed to initialize uifc");
	exit(1);
}
js.on_exit("uifc.bail()");

// DDFileLister base configuration filename, and help text wrap width
var gCfgFileName = "ddfilelister.cfg";
var gHelpWrapWidth = uifc.screen_width - 10;

// When saving the configuration file, always save it in the same directory
// as DDFileLister (or this script); don't copy to mods
var gAlwaysSaveCfgInOriginalDir = false;

// Parse command-line arguments
parseCmdLineArgs();

// Read the DDFileLister configuration file
var gCfgInfo = readDDFileListerCfgFile();

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
				var saveRetObj = saveDDFileListerCfgFile();
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

function doMainMenu()
{
	// For menu item text formatting
	var itemTextMaxLen = 50;

	// Configuration option object properties
	// cfgOptProps must correspond exactly with optionStrs & menuItems
	var cfgOptProps = [
		"interfaceStyle", // String (lightbar/traditional)
		"traditionalUseSyncStock", // Boolean
		"sortOrder", // String (PER_DIR_CFG/NATURAL/NAME_AI/NAME_DI/NAME_AS/NAME_DS/DATE_A/DATE_D/ULTIME/DLTIME)
		"displayUserAvatars", // Boolean
		"displayNumFilesInHeader", // Boolean
		"useFilenameIfShortDescriptionEmpty", // Boolean
		"filenameInExtendedDescription", // String (always/ifDescEmpty/never)
		"pauseAfterViewingFile", // Boolean
		"blankNFilesListedStrIfLoadableModule", // Boolean
		"themeFilename" // String
	];
	// Strings for the options to display on the menu
	var optionStrs = [
		"User Interface Style",
		"Traditional interface: Use stock implementation",
		"Sort order of file list",
		"Display user avatars",
		"Display number of files in header",
		"Short descs: Use filename if empty",
		"Use filename in extended description",
		"Pause after viewing a file",
		"Blank \"# Files Listed\" as loadable module",
		"Theme Filename"
	];
	// Build an array of formatted strings to be displayed on the menu
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
	var menuTitle = "DD File Lister Configuration";
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
			if (optName == "sortOrder")
			{
				// Sort order
				var optionsForCfgFile = ["PER_DIR_CFG", "NATURAL", "NAME_AI", "NAME_DI", "NAME_AS", "NAME_DS", "DATE_A", "DATE_D", "ULTIME", "DLTIME"];
				var optionsForDisplay = ["Per dir config (PER_DIR_CFG)",
				                         "Natural sort order (import date/time ascending) (NATURAL)",
										 "Filename ascending, case insensitive (NAME_AI)",
										 "Filename descending, case insensitive (NAME_DI)",
										 "Filename ascending, case sensitive (NAME_AS)",
										 "Filename descending, case sensitive (NAME_DS)",
										 "Import date/time ascending sort order (DATE_A)",
										 "Import date/time descending (DATE_D)",
										 "Upload time (ULTIME)",
										 "Download time (DLTIME)"];
				var promptStr = optionStrs[optionMenuSelection];
				// Return value: An object with the following properties:
				//               chosenIdx: The index of the user's chosen value, or -1 if the user aborted
				//               chosenValue: The user's chosen value, or null if the user aborted
				var choiceRetObj = promptMultipleChoice(promptStr, optionsForDisplay, gCfgInfo.cfgOptions[optName]);
				if (choiceRetObj.chosenIdx >= 0 && choiceRetObj.chosenValue != undefined && optionsForCfgFile[choiceRetObj.chosenIdx] != gCfgInfo.cfgOptions[optName])
				{
					gCfgInfo.cfgOptions[optName] = optionsForCfgFile[choiceRetObj.chosenIdx];
					anyOptionChanged = true;
					menuItems[optionMenuSelection] = formatCfgMenuText(itemTextMaxLen, optionStrs[optionMenuSelection], gCfgInfo.cfgOptions[optName]);
				}
			}
			else if (optName == "filenameInExtendedDescription")
			{
				// Use of the filename in extended descriptions
				// always, ifDescEmpty, never
				var optionsForCfgFile = ["always", "ifDescEmpty", "never"];
				var optionsForDisplay = ["Always",
				                         "If description is empty",
										 "Never"];
				var promptStr = optionStrs[optionMenuSelection];
				// Return value: An object with the following properties:
				//               chosenIdx: The index of the user's chosen value, or -1 if the user aborted
				//               chosenValue: The user's chosen value, or null if the user aborted
				var choiceRetObj = promptMultipleChoice(promptStr, optionsForDisplay, gCfgInfo.cfgOptions[optName]);
				if (choiceRetObj.chosenIdx >= 0 && choiceRetObj.chosenValue != undefined && optionsForCfgFile[choiceRetObj.chosenIdx] != gCfgInfo.cfgOptions[optName])
				{
					gCfgInfo.cfgOptions[optName] = optionsForCfgFile[choiceRetObj.chosenIdx];
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
				if (optName == "themeFilename")
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
				else
				{
					// Generic multiple-choice
					var options = [];
					if (optName == "interfaceStyle")
						options = ["Lightbar", "Traditional"];
					var promptStr = optionStrs[optionMenuSelection];
					var choiceRetObj = promptMultipleChoice(promptStr, options, gCfgInfo.cfgOptions[optName]);
					// Return value: An object with the following properties:
					//               chosenIdx: The index of the user's chosen value, or -1 if the user aborted
					//               chosenValue: The user's chosen value, or null if the user aborted
					if (choiceRetObj.chosenValue != null && choiceRetObj.chosenValue != undefined && choiceRetObj.chosenValue != gCfgInfo.cfgOptions[optName])
					{
						gCfgInfo.cfgOptions[optName] = choiceRetObj.chosenValue;
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
// Return value: An object with the following properties:
//               chosenIdx: The index of the user's chosen value, or -1 if the user aborted
//               chosenValue: The user's chosen value, or null if the user aborted
function promptMultipleChoice(pPrompt, pChoices, pCurrentVal)
{
	var retObj = {
		chosenIdx: -1,
		chosenValue: null
	};

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
	//var winMode = WIN_ORG|WIN_MID|WIN_ACT|WIN_ESC;
	var winMode = WIN_MID;
	var userSelection = uifc.list(winMode, pPrompt, pChoices, ctx);
	if (userSelection >= 0 && userSelection < pChoices.length)
	{
		retObj.chosenIdx = userSelection;
		retObj.chosenValue = pChoices[userSelection];
	}
	return retObj;
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

///////////////////////////////////////////////////
// Help text functions


// Returns a dictionary of help text, indexed by the option name from the configuration file
function getOptionHelpText()
{
	var optionHelpText = {};
	optionHelpText["interfaceStyle"] = "Interface Style: Either Lightbar or Traditional";

	optionHelpText["traditionalUseSyncStock"] = "Traditional interface use stock implementation: When using the traditional ";
	optionHelpText["traditionalUseSyncStock"] += "interface, use the Synchronet stock file lister (don't use ddfilelister at all). If this ";
	optionHelpText["traditionalUseSyncStock"] += "is false, ddfilelister's traditional interface will be used if interfaceStyle ";
	optionHelpText["traditionalUseSyncStock"] += "is traditional.";

	optionHelpText["sortOrder"] = "Sort order of file list: The sort order of the file list. Valid options are PER_DIR_CFG ";
	optionHelpText["sortOrder"] += "(according to the directory configuration in SCFG > File Areas > library > File Directories > ";
	optionHelpText["sortOrder"] += "dir > Advanced Options > Sort Value and Direction); NATURAL (natural sort order - same as ";
	optionHelpText["sortOrder"] += "DATE_A); NAME_AI (filename ascending, case insensitive); FILENAME_DI (filename descending, ";
	optionHelpText["sortOrder"] += "case insensitive); NAME__AS (filename ascending, case sensitive); NAME_DS (filename descending, ";
	optionHelpText["sortOrder"] += "case sensitive); DATE_A (import date/time ascending); DATE_D (import date/time descending); ULTIME ";
	optionHelpText["sortOrder"] += "(upload time); (DLTIME (download time)";

	optionHelpText["displayUserAvatars"] = "Display user avatars: Whether or not to display user avatars for the uploader in extended ";
	optionHelpText["displayUserAvatars"] += "file descriptions";

	optionHelpText["displayNumFilesInHeader"] = "Display number of files in header: Whether or not to display the number of files in ";
	optionHelpText["displayNumFilesInHeader"] += "the header at the top of the screen";

	optionHelpText["useFilenameIfShortDescriptionEmpty"] = "Short descs - Use filename if empty: For short descriptions, if the ";
	optionHelpText["useFilenameIfShortDescriptionEmpty"] += "description is empty, whether to use the filename instead";

	optionHelpText["filenameInExtendedDescription"] = "Use filename in extended description: How to use the filename in the extended ";
	optionHelpText["filenameInExtendedDescription"] += "description. Valid options are always (always use the filename in the description); ";
	optionHelpText["filenameInExtendedDescription"] += "ifDescEmpty (only if the description is empty - also if the filename is too short to ";
	optionHelpText["filenameInExtendedDescription"] += "fully be shown in the menu, the full filename will appear in the description); ";
	optionHelpText["filenameInExtendedDescription"] += "never (never use the filename in the description)";

	optionHelpText["pauseAfterViewingFile"] = "Pause after viewing a file: Whether nor not to pause & wait for user input after viewing a file";

	optionHelpText["blankNFilesListedStrIfLoadableModule"] = "Blank \"# Files Listed\" as loadable module: When used as a loadable module, ";
	optionHelpText["blankNFilesListedStrIfLoadableModule"] += "whether or not to blank out the \"# Files Listed\" string (from text.dat) so ";
	optionHelpText["blankNFilesListedStrIfLoadableModule"] += "that Synchronet won't display it after the lister exits";

	optionHelpText["themeFilename"] = "Theme Filename: The name of the color theme file";

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
	var helpText = "This screen allows you to configure behavior options for Digital Distortion File Lister.\r\n\r\n";
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

// Reads the DDFileLister configuration file
//
// Return value: An object with the following properties:
//               cfgFilename: The full path & filename of the DDFileLister configuration file that was read
//               cfgOptions: A dictionary of the configuration options and their values
function readDDFileListerCfgFile()
{
	var retObj = {
		cfgFilename: "",
		cfgOptions: {}
	};

	// Determine the location of the configuration file.  First look for it
	// in the sbbs/mods directory, then sbbs/ctrl, then in the same directory
	// as this script.
	var cfgFilename = file_cfgname(system.mods_dir, gCfgFileName);
	if (!file_exists(cfgFilename))
		cfgFilename = file_cfgname(system.ctrl_dir, gCfgFileName);
	if (!file_exists(cfgFilename))
		cfgFilename = file_cfgname(js.exec_dir, gCfgFileName);
	retObj.cfgFilename = cfgFilename;

	// Open and read the configuration file
	var cfgFile = new File(retObj.cfgFilename);
	if (cfgFile.open("r"))
	{
		retObj.cfgOptions = cfgFile.iniGetObject();
		cfgFile.close();
	}

	// In case some settings weren't loaded, add defaults
	if (!retObj.cfgOptions.hasOwnProperty("interfaceStyle"))
		retObj.cfgOptions.interfaceStyle = "Lightbar";
	if (!retObj.cfgOptions.hasOwnProperty("traditionalUseSyncStock"))
		retObj.cfgOptions.traditionalUseSyncStock = true;
	if (!retObj.cfgOptions.hasOwnProperty("sortOrder"))
		retObj.cfgOptions.sortOrder = "PER_DIR_CFG";
	if (!retObj.cfgOptions.hasOwnProperty("pauseAfterViewingFile"))
		retObj.cfgOptions.pauseAfterViewingFile = false;
	if (!retObj.cfgOptions.hasOwnProperty("blankNFilesListedStrIfLoadableModule"))
		retObj.cfgOptions.blankNFilesListedStrIfLoadableModule = true;
	if (!retObj.cfgOptions.hasOwnProperty("displayUserAvatars"))
		retObj.cfgOptions.displayUserAvatars = true;
	if (!retObj.cfgOptions.hasOwnProperty("useFilenameIfShortDescriptionEmpty"))
		retObj.cfgOptions.useFilenameIfShortDescriptionEmpty = true;
	if (!retObj.cfgOptions.hasOwnProperty("filenameInExtendedDescription"))
		retObj.cfgOptions.filenameInExtendedDescription = "ifDescEmpty";
	if (!retObj.cfgOptions.hasOwnProperty("displayNumFilesInHeader"))
		retObj.cfgOptions.displayNumFilesInHeader = true;
	if (!retObj.cfgOptions.hasOwnProperty("themeFilename"))
		retObj.cfgOptions.themeFilename = "defaultTheme.cfg";

	return retObj;
}

// Saves the DDFileLister configuration file using the settings in gCfgInfo
//
// Return value: An object with the following properties:
//               saveSucceeded: Boolean - Whether or not the save fully succeeded
//               savedToModsDir: Boolean - Whether or not the .cfg file was saved to the mods directory
function saveDDFileListerCfgFile()
{
	var retObj = {
		saveSucceeded: false,
		savedToModsDir: false
	};

	// If the configuration file was loaded from the standard location in
	// the Git repository (xtrn/ddfilelister), and the option to always save
	// in the original directory is not set, then the configuration file
	// should be copied to sbbs/mods to avoid custom settings being overwritten
	// with an update.
	var defaultDirRE = new RegExp("xtrn[\\\\/]ddfilelister[\\\\/]" + gCfgFileName + "$");
	var cfgFilename = gCfgInfo.cfgFilename;
	if (defaultDirRE.test(cfgFilename) && !gAlwaysSaveCfgInOriginalDir)
	{
		cfgFilename = system.mods_dir + gCfgFileName;
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
