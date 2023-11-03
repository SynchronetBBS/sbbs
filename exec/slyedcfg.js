// SlyEdit configurator: This is a menu-driven configuration program/script for SlyEdit.
// Any changes are saved to SlyEdit.cfg in sbbs/mods, so that custom changes don't get
// overridden with SlyEdit.cfg in sbbs/ctrl due to an update.
// Currently for SlyEdit 1.87.

"use strict";


require("sbbsdefs.js", "P_NONE");
require("uifcdefs.js", "UIFC_INMSG");


if (!uifc.init("SlyEdit 1.87 Configurator"))
{
	print("Failed to initialize uifc");
	exit(1);
}
js.on_exit("uifc.bail()");


// SlyEdit base configuration filename, and help text wrap width
var gSlyEdCfgFileName = "SlyEdit.cfg";
var gHelpWrapWidth = uifc.screen_width - 10;

// Read the SlyEdit configuration file
var gCfgInfo = readSlyEditCfgFile();

// Show the main menu and go from there
doMainMenu();



///////////////////////////////////////////////////////////
// Functions

function doMainMenu()
{
	// Create a CTX to specify the current selected item index
	if (doMainMenu.ctx == undefined)
		doMainMenu.ctx = uifc.list.CTX();
	var helpText = "Behavior: Behavior settings\r\nIce Colors: Ice-related color settings\r\nDCT Colors: DCT-related color settings";
	// Selection
	var winMode = WIN_ORG|WIN_MID|WIN_ACT|WIN_ESC;
	var menuTitle = "SlyEdit Configuration";
	var anyOptionChanged = false;
	var continueOn = true;
	while (continueOn && !js.terminated)
	{
		uifc.help_text = word_wrap(helpText, gHelpWrapWidth);
		var selection = uifc.list(winMode, menuTitle, ["Behavior", "Ice Colors", "DCT Colors"], doMainMenu.ctx);
		doMainMenu.ctx.cur = selection; // Remember the current selected item
		switch (selection)
		{
			case -1: // ESC
				continueOn = false;
				break;
			case 0: // Behavior
				anyOptionChanged = doBehaviorMenu() || anyOptionChanged;
				break;
			case 1: // Ice colors
				anyOptionChanged = doIceColors() || anyOptionChanged;
				break;
			case 2: // DCT colors
				anyOptionChanged = doDCTColors() || anyOptionChanged;
				break;
		}
	}
	
	// If any configuration option changed, prompt the user & save if the user wants to
	if (anyOptionChanged)
	{
		var userChoice = promptYesNo("Save configuration?", true, WIN_ORG|WIN_MID|WIN_ACT|WIN_ESC);
		if (typeof(userChoice) === "boolean" && userChoice)
		{
			if (saveSlyEditCfgFile())
				uifc.msg("Changes were successfully saved (to mods dir)");
			else
				uifc.msg("Failed to save settings!");
		}
	}
}

// Allows the user to change behavior settings.
// Returns whether any option changed (boolean).
function doBehaviorMenu()
{
	// For menu item text formatting
	var itemTextMaxLen = 40;

	// For options in the SlyEdit configuration, the order of
	// cfgOptProps must correspond exactly with menuItems
	// Configuration object properties (true/false properties are first):
	var cfgOptProps = [
		"displayEndInfoScreen",
		"userInputTimeout",
		"reWrapQuoteLines",
		"useQuoteLineInitials",
		"indentQuoteLinesWithInitials",
		"allowEditQuoteLines",
		"allowUserSettings",
		"allowColorSelection",
		"saveColorsAsANSI",
		"allowCrossPosting",
		"enableTaglines",
		"shuffleTaglines",
		"quoteTaglines",
		"allowSpellCheck",

		"inputTimeoutMS",
		"enableTextReplacements",
		"tagLineFilename",
		"taglinePrefix",
		"dictionaryFilenames"
	];
	// Menu item text for the toggle options:
	var toggleOptItems = [
		"Display end info screen",
		"Enable user input timeout",
		"Re-wrap quote lines",
		"Use author initials in quoted lines",
		"Indent quoted lines with author initials",
		"Allow editing quote lines",
		"Allow user settings",
		"Allow color selection",
		"Save colors as ANSI",
		"Allow cross-posting",
		"Enable taglines",
		"Shuffle taglines",
		"Double-quotes around tag lines",
		"Allow/enable spell check"
	];
	// Build the array of items to be displayed on the menu
	var menuItems = [];
	// Toggle (on/off) settings
	for (var i = 0; i < 14; ++i)
		menuItems.push(formatCfgMenuText(itemTextMaxLen, toggleOptItems[i], gCfgInfo.cfgSections.BEHAVIOR[cfgOptProps[i]]));
	// Text input settings, etc.
	menuItems.push(formatCfgMenuText(itemTextMaxLen, "User input timeout (MS)", gCfgInfo.cfgSections.BEHAVIOR.inputTimeoutMS));
	// Text replacements can be a boolean true/false or "regex"
	menuItems.push(formatCfgMenuText(itemTextMaxLen, "Enable text replacements", getTxtReplacementsVal()));
	menuItems.push(formatCfgMenuText(itemTextMaxLen, "Tagline filename", gCfgInfo.cfgSections.BEHAVIOR.tagLineFilename));
	menuItems.push(formatCfgMenuText(itemTextMaxLen, "Tagline prefix", gCfgInfo.cfgSections.BEHAVIOR.taglinePrefix));
	menuItems.push("Dictionary filenames"); // dictionaryFilenames

	// Create a CTX to specify the current selected item index
	if (doBehaviorMenu.ctx == undefined)
		doBehaviorMenu.ctx = uifc.list.CTX();
	// Selection
	var winMode = WIN_ORG|WIN_MID|WIN_ACT|WIN_ESC;
	var menuTitle = "SlyEdit Behavior Configuration";
	var anyOptionChanged = false;
	var continueOn = true;
	while (continueOn && !js.terminated)
	{
		uifc.help_text = getBehaviorScreenHelp();
		var optionMenuSelection = uifc.list(winMode, menuTitle, menuItems, doBehaviorMenu.ctx);
		doBehaviorMenu.ctx.cur = optionMenuSelection; // Remember the current selected item
		switch (optionMenuSelection)
		{
			case -1: // ESC
				continueOn = false;
				break;
			// 0-13 are boolean values
			case 0:
			case 1:
			case 2:
			case 3:
			case 4:
			case 5:
			case 6:
			case 7:
			case 8:
			case 9:
			case 10:
			case 11:
			case 12:
			case 13:
				gCfgInfo.cfgSections.BEHAVIOR[cfgOptProps[optionMenuSelection]] = !gCfgInfo.cfgSections.BEHAVIOR[cfgOptProps[optionMenuSelection]];
				anyOptionChanged = true;
				menuItems[optionMenuSelection] = formatCfgMenuText(itemTextMaxLen, toggleOptItems[optionMenuSelection], gCfgInfo.cfgSections.BEHAVIOR[cfgOptProps[optionMenuSelection]]);
				// With a separate window to prompt for toggling the item:
				/*
				if (inputCfgObjBoolean(cfgOptProps[optionMenuSelection], toggleOptItems[optionMenuSelection]))
				{
					anyOptionChanged = true;
					menuItems[optionMenuSelection] = formatCfgMenuText(itemTextMaxLen, toggleOptItems[optionMenuSelection], gCfgInfo.cfgSections.BEHAVIOR[cfgOptProps[optionMenuSelection]]);
				}
				*/
				break;
			case 14: // User input timeout (MS)
				uifc.help_text = "The user inactivity timeout, in milliseconds";
				var userInput = uifc.input(WIN_MID, "User input timeout (MS)", gCfgInfo.cfgSections.BEHAVIOR.inputTimeoutMS.toString(), 0, K_NUMBER|K_EDIT);
				if (typeof(userInput) === "string" && userInput.length > 0)
				{
					var value = parseInt(userInput);
					if (!isNaN(value) && value > 0 && gCfgInfo.cfgSections.BEHAVIOR.inputTimeoutMS != value)
					{
						gCfgInfo.cfgSections.BEHAVIOR.inputTimeoutMS = value;
						anyOptionChanged = true;
						menuItems[optionMenuSelection] = formatCfgMenuText(itemTextMaxLen, "User input timeout (MS)", value);
					}
				}
				break;
			case 15: // Enable text replacements (yes/no/regex)
				var helpText = "Whether or not to enable text replacements (AKA macros). Can be ";
				helpText += "true, false, or 'regex' to use regular expressions.";
				uifc.help_text = word_wrap(helpText, gHelpWrapWidth);
				// Store the current value wo we can see if it changes
				var valBackup = gCfgInfo.cfgSections.BEHAVIOR.enableTextReplacements;
				// Prompt the user
				var ctx = uifc.list.CTX();
				if (typeof(gCfgInfo.cfgSections.BEHAVIOR.enableTextReplacements) === "boolean")
					ctx.cur = gCfgInfo.cfgSections.BEHAVIOR.enableTextReplacements ? 0 : 1;
				else if (gCfgInfo.cfgSections.BEHAVIOR.enableTextReplacements.toLowerCase() === "regex")
					ctx.cur = 2;
				var txtReplacementsSelection = uifc.list(winMode, "Enable text replacements", ["Yes", "No", "Regex"], ctx);
				switch (txtReplacementsSelection)
				{
					case 0:
						gCfgInfo.cfgSections.BEHAVIOR.enableTextReplacements = true;
						break;
					case 1:
						gCfgInfo.cfgSections.BEHAVIOR.enableTextReplacements = false;
						break;
					case 2:
						gCfgInfo.cfgSections.BEHAVIOR.enableTextReplacements = "regex";
						break;
				}
				if (gCfgInfo.cfgSections.BEHAVIOR.enableTextReplacements != valBackup)
				{
					anyOptionChanged = true;
					menuItems[optionMenuSelection] = formatCfgMenuText(itemTextMaxLen, "Enable text replacements", getTxtReplacementsVal());
				}
				break;
			case 16: // Tagline filename
				var helpText = "The name of the file where tag lines are stored. This file is loaded from the sbbs ctrl directory.";
				uifc.help_text = word_wrap(helpText, gHelpWrapWidth);
				var userInput = uifc.input(WIN_MID, "Tagline filename", gCfgInfo.cfgSections.BEHAVIOR.tagLineFilename, 60, K_EDIT);
				if (typeof(userInput) === "string" && userInput != gCfgInfo.cfgSections.BEHAVIOR.tagLineFilename)
				{
					gCfgInfo.cfgSections.BEHAVIOR.tagLineFilename = userInput;
					anyOptionChanged = true;
					menuItems[optionMenuSelection] = formatCfgMenuText(itemTextMaxLen, "Tagline filename", userInput);
				}
				break;
			case 17: // Tagline prefix
				var helpText = "Text to add to the front of a tagline when adding it to the message. This ";
				helpText += "can be blank (nothing after the =) if no prefix is desired.";
				uifc.help_text = word_wrap(helpText, gHelpWrapWidth);
				var userInput = uifc.input(WIN_MID, "Tagline prefix", gCfgInfo.cfgSections.BEHAVIOR.taglinePrefix, 0, K_EDIT);
				if (typeof(userInput) === "string" && userInput != gCfgInfo.cfgSections.BEHAVIOR.taglinePrefix)
				{
					gCfgInfo.cfgSections.BEHAVIOR.tagLineFilename = userInput;
					anyOptionChanged = true;
					menuItems[optionMenuSelection] = formatCfgMenuText(itemTextMaxLen, "Tagline prefix", userInput);
				}
				break;
			case 18: // Dictionary filenames
				var userInput = promptDictionaries();
				anyOptionChanged = (userInput != gCfgInfo.cfgSections.BEHAVIOR.dictionaryFilenames);
				gCfgInfo.cfgSections.BEHAVIOR.dictionaryFilenames = userInput;
				break;
		}
	}

	return anyOptionChanged;
}

function doIceColors()
{
	// Create a CTX to specify the current selected item index for this
	// menu and the theme file menu
	if (doIceColors.ctx == undefined)
		doIceColors.ctx = uifc.list.CTX();
	if (doIceColors.theme_ctx == undefined)
		doIceColors.theme_ctx = uifc.list.CTX();
	// Format string for the menu items, and menu items
	var formatStr = "%-30s %s";
	var menuItems = [
		format(formatStr, "Menu option classic colors", gCfgInfo.cfgSections.ICE_COLORS.menuOptClassicColors ? "Yes" : "No"),
		format(formatStr, "Theme file", gCfgInfo.cfgSections.ICE_COLORS.ThemeFilename.replace(/\.cfg$/, ""))
	];

	var helpText = "Menu option classic colors: Whether to use 100% classic colors for menu options\r\n\r\n";
	helpText += "Theme file: The color theme configuration filename (can be in mods or ctrl)";

	// Selection
	var winMode = WIN_ORG|WIN_MID|WIN_ACT|WIN_ESC;
	var anyOptionChanged = false;
	var continueOn = true;
	while (continueOn && !js.terminated)
	{
		uifc.help_text = word_wrap(helpText, gHelpWrapWidth);
		var selection = uifc.list(winMode, "SlyEdit Ice Colors", menuItems, doIceColors.ctx);
		doIceColors.ctx.cur = selection; // Remember the current selected item
		switch (selection)
		{
			case -1: // ESC
				continueOn = false;
				break;
			case 0: // Toggle menu option classic colors
				gCfgInfo.cfgSections.ICE_COLORS.menuOptClassicColors = !gCfgInfo.cfgSections.ICE_COLORS.menuOptClassicColors;
				menuItems[0] = format(formatStr, "Menu option classic colors", gCfgInfo.cfgSections.ICE_COLORS.menuOptClassicColors ? "Yes" : "No");
				anyOptionChanged = true;
				break;
			case 1: // Theme file
				var dictFilenames = getAbbreviatedThemeFilenameList("Ice");
				var themeSelection = uifc.list(winMode, "SlyEdit Ice Theme", dictFilenames, doIceColors.theme_ctx);
				doIceColors.theme_ctx.cur = themeSelection; // Remember the current selected item
				if (themeSelection >= 0 && themeSelection < dictFilenames.length)
				{
					gCfgInfo.cfgSections.ICE_COLORS.ThemeFilename = dictFilenames[themeSelection] + ".cfg";
					menuItems[1] = format(formatStr, "Theme file", dictFilenames[themeSelection]);
					anyOptionChanged = true;
				}
				break;
		}
	}
	return anyOptionChanged;
}

function doDCTColors()
{
	// Create a CTX to specify the current selected item index for this
	// menu and the theme file menu
	if (doDCTColors.ctx == undefined)
		doDCTColors.ctx = uifc.list.CTX();
	if (doDCTColors.theme_ctx == undefined)
		doDCTColors.theme_ctx = uifc.list.CTX();
	// Format string for the menu items, and menu items
	var formatStr = "%-30s %s";
	var menuItems = [
		format(formatStr, "Theme file", gCfgInfo.cfgSections.DCT_COLORS.ThemeFilename.replace(/\.cfg$/, ""))
	];

	var helpText = "Theme file: The color theme configuration filename (can be in mods or ctrl)";

	// Selection
	var winMode = WIN_ORG|WIN_MID|WIN_ACT|WIN_ESC;
	var anyOptionChanged = false;
	var continueOn = true;
	while (continueOn && !js.terminated)
	{
		uifc.help_text = word_wrap(helpText, gHelpWrapWidth);
		var selection = uifc.list(winMode, "SlyEdit DCT Colors", menuItems, doDCTColors.ctx);
		doDCTColors.ctx.cur = selection; // Remember the current selected item
		switch (selection)
		{
			case -1: // ESC
				continueOn = false;
				break;
			case 0: // Theme file
				var dictFilenames = getAbbreviatedThemeFilenameList("DCT");
				var themeSelection = uifc.list(winMode, "SlyEdit DCT Theme", dictFilenames, doDCTColors.theme_ctx);
				doDCTColors.theme_ctx.cur = themeSelection; // Remember the current selected item
				if (themeSelection >= 0 && themeSelection < dictFilenames.length)
				{
					gCfgInfo.cfgSections.DCT_COLORS.ThemeFilename = dictFilenames[themeSelection] + ".cfg";
					menuItems[0] = format(formatStr, "Theme file", dictFilenames[themeSelection]);
					anyOptionChanged = true;
				}
				break;
		}
	}
	return anyOptionChanged;
}

// Gets a value for the text replacements option. This could be a boolean true/false
// or the string "regex".
function getTxtReplacementsVal()
{
	var txtReplacementsItemVal = "";
	if (gCfgInfo.cfgSections.BEHAVIOR.textReplacementsUseRegex)
		txtReplacementsItemVal = "Regex";
	else
		txtReplacementsItemVal = gCfgInfo.cfgSections.BEHAVIOR.enableTextReplacements;
	return txtReplacementsItemVal;
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
		value = pVal.toString();
	return format(formatCfgMenuText.formatStr, pItemText.substr(0, pItemTextMaxLen), value);
}

// For a boolean configuration option, this prompts the user
// for a yes/no response, with option display string
//
// Return value: Boolean - Whether or not the option changed from
//               the current configuration
function inputCfgObjBoolean(pCfgProp, pDisplayStr)
{
	var optChanged = false;
	var toggleOpt = promptYesNo(pDisplayStr, gCfgInfo.cfgSections.BEHAVIOR[pCfgProp]);
	if (typeof(toggleOpt) === "boolean")
	{
		optChanged = (gCfgInfo.cfgSections.BEHAVIOR[pCfgProp] != toggleOpt);
		gCfgInfo.cfgSections.BEHAVIOR[pCfgProp] = toggleOpt;
	}
	return optChanged;
}

// Prompts the user for which dictionaries to use (for spell check)
function promptDictionaries()
{
	// Find the dictionary filenames in sbbs/ctrl
	var dictFilenames = directory(system.ctrl_dir + "dictionary_*.txt");
	if (dictFilenames.length == 0)
	{
		uifc.msg("There are no dictionaries available in ctrl/");
		return "";
	}

	// Abbreviated dictionary names: Get just the filename without the full path,
	// and remove the trailing .txt and leading dictionary_
	var abbreviatedDictNames = [];
	for (var i = 0; i < dictFilenames.length; ++i)
	{
		var dictFilename = file_getname(dictFilenames[i]).replace(/\.txt$/, "");
		dictFilename = dictFilename.replace(/^dictionary_/, "");
		abbreviatedDictNames.push(dictFilename);
	}

	// Split the current list of dictionaries into an array
	var existingDictionaries = gCfgInfo.cfgSections.BEHAVIOR.dictionaryFilenames.split(",");
	// A map of abbreviated dictionary names and booleans for their enabled status
	var dictToggleVals = {};
	for (var i = 0; i < abbreviatedDictNames.length; ++i)
		dictToggleVals[abbreviatedDictNames[i]] = (existingDictionaries.indexOf(abbreviatedDictNames[i]) > -1);

	// Menu items: Abbreviated dictionary names with "Yes" or "No"
	var dictNameWidth = 30;
	var formatStr = "%-" + dictNameWidth + "s %s";
	var dictMenuItems = [];
	for (var i = 0; i < abbreviatedDictNames.length; ++i)
	{
		var enabledStr = dictToggleVals[abbreviatedDictNames[i]] ? "Yes" : "No";
		dictMenuItems.push(format(formatStr, abbreviatedDictNames[i], enabledStr));
	}

	// Create a CTX to keep track of the selected item index
	var ctx = uifc.list.CTX();
	// Help text
	var helpText = "Here, you can enable which dictionaries to use for spell check.  ";
	helpText += "The dictionary filenames are in the format dictionary_<language>.txt, where ";
	helpText += "<language> is the language name.  The dictionary files are located in either ";
	helpText += "sbbs/mods, sbbs/ctrl, or the same directory as SlyEdit.";
	uifc.help_text = word_wrap(helpText, gHelpWrapWidth);
	// User input loop
	var continueOn = true;
	while (continueOn)
	{
		var selection = uifc.list(WIN_MID, "Dictionaries", dictMenuItems, ctx);
		if (selection == -1) // User quit/aborted
			continueOn = false;
		else
		{
			var abbreviatedDictName = abbreviatedDictNames[selection];
			dictToggleVals[abbreviatedDictName] = !dictToggleVals[abbreviatedDictName];
			var enabledStr = dictToggleVals[abbreviatedDictName] ? "Yes" : "No";
			dictMenuItems[selection] = format(formatStr, abbreviatedDictName, enabledStr);
			ctx.cur = selection; // Remember the current selected item index
		}
	}

	// Build a comma-separated list of the Abbreviated dictionary names and return it
	var dictNames = "";
	for (var dictName in dictToggleVals)
	{
		if (dictToggleVals[dictName])
		{
			if (dictNames != "")
				dictNames += ",";
			dictNames += dictName;
		}
	}
	return dictNames;
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

// Returns help text for the behavior configuration screen
function getBehaviorScreenHelp()
{
	if (getBehaviorScreenHelp.help == undefined)
	{
		getBehaviorScreenHelp.help = "This screen allows you to configure behavior options for SlyEdit.\r\n\r\n";

		getBehaviorScreenHelp.help += "Display end info screen: Whether or not to display editor info when exiting\r\n\r\n";

		getBehaviorScreenHelp.help += "User input timeout: Whether or not to enable user inactivity timeout\r\n\r\n";

		getBehaviorScreenHelp.help += "Re-wrap quote lines: If true, quote lines will be re-wrapped so that they are complete ";
		getBehaviorScreenHelp.help += "but still look good when quoted.  If this option is disabled, then quote lines will ";
		getBehaviorScreenHelp.help += "simply be trimmed to fit into the message.\r\n\r\n";
		
		getBehaviorScreenHelp.help += "Use author initials in quoted lines: Whether or not to prefix the quote ";
		getBehaviorScreenHelp.help += "lines with the last author's initials. Users can change this for themselves too.\r\n\r\n";

		getBehaviorScreenHelp.help += "Indent quoted lines with author initials: When prefixing quote lines with the last author's initials, ";
		getBehaviorScreenHelp.help += "whether or not to indent the quote lines with a space. Users can change this for themselves too.\r\n\r\n";

		getBehaviorScreenHelp.help += "Allow editing quote lines: Whether or not to allow editing quote lines\r\n\r\n";

		getBehaviorScreenHelp.help += "Allow user settings: Whether or not to allow users to change their user settings.\r\n\r\n";

		getBehaviorScreenHelp.help += "Allow color selection: Whether or not to let the user change the text color\r\n\r\n";

		getBehaviorScreenHelp.help += "Save colors as ANSI: Whether or not to save message color/attribute codes as ANSI ";
		getBehaviorScreenHelp.help += "(if not, they will be saved as Synchronet attribute codes)\r\n\r\n";

		getBehaviorScreenHelp.help += "Allow cross-posting: Whether or not to allow cross-posting to multiple sub-boards\r\n\r\n";

		getBehaviorScreenHelp.help += "Enable taglines: Whether or not to enable the option to add a tagline.\r\n\r\n";

		getBehaviorScreenHelp.help += "Shuffle taglines: Whether or not to shuffle (randomize) the list of taglines when they are ";
		getBehaviorScreenHelp.help += "displayed for the user to choose from\r\n\r\n";

		getBehaviorScreenHelp.help += "Double-quotes around tag lines: Whether or not to add double-quotes around taglines\r\n\r\n";

		getBehaviorScreenHelp.help += "Allow/enable spell check: Whether or not to allow spell check\r\n\r\n";

		getBehaviorScreenHelp.help += "User input timeout (MS): The user inactivity timeout, in milliseconds\r\n\r\n";

		getBehaviorScreenHelp.help += "Enable text replacements: Whether or not to enable text replacements (AKA macros). Can be ";
		getBehaviorScreenHelp.help += "true, false, or 'regex' to use regular expressions.\r\n\r\n";

		getBehaviorScreenHelp.help += "Tagline filename: The name of the file where tag lines are stored. This file is loaded ";
		getBehaviorScreenHelp.help += "from the sbbs ctrl directory.\r\n\r\n";

		getBehaviorScreenHelp.help += "Tagline prefix: Text to add to the front of a tagline when adding it to the message. This ";
		getBehaviorScreenHelp.help += "can be blank (nothing after the =) if no prefix is desired.\r\n\r\n";

		getBehaviorScreenHelp.help +=  "Dictionary filenames: These are dictionaries to use for spell check.  ";
		getBehaviorScreenHelp.help += "The dictionary filenames are in the format dictionary_<language>.txt, where ";
		getBehaviorScreenHelp.help += "<language> is the language name.  The dictionary files are located in either ";
		getBehaviorScreenHelp.help += "sbbs/mods, sbbs/ctrl, or the same directory as SlyEdit. Users can change ";
		getBehaviorScreenHelp.help += "this for themselves too.";

		getBehaviorScreenHelp.help = word_wrap(getBehaviorScreenHelp.help, gHelpWrapWidth);
	}
	return getBehaviorScreenHelp.help;
}


///////////////////////////////////////////////////
// Non-UI utility functions


// Gets an array of abbreviated color configuration filenames.
// Assumes they're in the format Sly<OPT>Colors_*.cfg, where
// OPT is either Ice or DCT
function getAbbreviatedThemeFilenameList(pIceOrDCT)
{
	// The theme files may be in sbbs/ctrl or sbbs/mods.  Create a dictionary
	// of the filenames to store unique filenames from both directories.
	var dictFilenames = {};
	var dictFilenamesArray = directory(system.ctrl_dir + "Sly" + pIceOrDCT + "Colors_*.cfg");
	dictFilenamesArray = dictFilenamesArray.concat(directory(system.mods_dir + "Sly" + pIceOrDCT + "Colors_*.cfg"));
	for (var i = 0; i < dictFilenamesArray.length; ++i)
		dictFilenames[dictFilenamesArray[i]] = true;

	// For each dictionary filename, get just the filename without the
	// leading path and remove the trailing .cfg, and build a dictionary
	// of these abbreviated filenames.
	var abbreviatedDictFilenames = [];
	for (var filename in dictFilenames)
	{
		var abbreviatedFilename = file_getname(filename.replace(/\.cfg$/, ""));
		abbreviatedDictFilenames.push(abbreviatedFilename);
	}
	return abbreviatedDictFilenames;
}

// For configuration files, this function returns a fully-pathed filename.
// This function first checks to see if the file exists in the sbbs/mods
// directory, then the sbbs/ctrl directory, and if the file is not found there,
// this function defaults to the given default path.
//
// Parameters:
//  pFilename: The name of the file to look for
function genFullPathCfgFilename(pFilename)
{
	var fullyPathedFilename = system.mods_dir + pFilename;
	if (!file_exists(fullyPathedFilename))
		fullyPathedFilename = system.ctrl_dir + pFilename;
	if (!file_exists(fullyPathedFilename))
		fullyPathedFilename = js.exec_dir + pFilename;
	return fullyPathedFilename;
}

// Reads the SlyEdit configuration file
//
// Return value: An object with the following properties:
//               cfgFilename: The full path & filename of the SlyEdit configuration file that was read
//               cfgSections: A dictionary of the INI sections (BEHAVIOR, ICE_COLORS, DCT_COLORS)
function readSlyEditCfgFile()
{
	var retObj = {
		cfgFilename: genFullPathCfgFilename(gSlyEdCfgFileName),
		cfgSections: {}
	};

	var cfgFile = new File(retObj.cfgFilename);
	if (cfgFile.open("r"))
	{
		var iniSectionNames = cfgFile.iniGetSections();
		for (var i = 0; i < iniSectionNames.length; ++i)
			retObj.cfgSections[iniSectionNames[i]] = cfgFile.iniGetObject(iniSectionNames[i]);
		cfgFile.close();
	}
	// In case some settings weren't loaded, add defaults
	if (!retObj.cfgSections.hasOwnProperty("BEHAVIOR"))
		retObj.cfgSections.BEHAVIOR = {};
	if (!retObj.cfgSections.BEHAVIOR.hasOwnProperty("displayEndInfoScreen"))
		retObj.cfgSections.BEHAVIOR.displayEndInfoScreen = true;
	if (!retObj.cfgSections.BEHAVIOR.hasOwnProperty("userInputTimeout"))
		retObj.cfgSections.BEHAVIOR.userInputTimeout = true;
	if (!retObj.cfgSections.BEHAVIOR.hasOwnProperty("inputTimeoutMS"))
		retObj.cfgSections.BEHAVIOR.inputTimeoutMS = 30000;
	if (!retObj.cfgSections.BEHAVIOR.hasOwnProperty("reWrapQuoteLines"))
		retObj.cfgSections.BEHAVIOR.reWrapQuoteLines = true;
	if (!retObj.cfgSections.BEHAVIOR.hasOwnProperty("allowColorSelection"))
		retObj.cfgSections.BEHAVIOR.allowColorSelection = true;
	if (!retObj.cfgSections.BEHAVIOR.hasOwnProperty("saveColorsAsANSI"))
		retObj.cfgSections.BEHAVIOR.saveColorsAsANSI = false;
	if (!retObj.cfgSections.BEHAVIOR.hasOwnProperty("allowCrossPosting"))
		retObj.cfgSections.BEHAVIOR.allowCrossPosting = true;
	if (!retObj.cfgSections.BEHAVIOR.hasOwnProperty("enableTextReplacements"))
		retObj.cfgSections.BEHAVIOR.enableTextReplacements = false;
	if (!retObj.cfgSections.BEHAVIOR.hasOwnProperty("tagLineFilename"))
		retObj.cfgSections.BEHAVIOR.tagLineFilename = "SlyEdit_Taglines.txt";
	if (!retObj.cfgSections.BEHAVIOR.hasOwnProperty("taglinePrefix"))
		retObj.cfgSections.BEHAVIOR.taglinePrefix = "...";
	if (!retObj.cfgSections.BEHAVIOR.hasOwnProperty("quoteTaglines"))
		retObj.cfgSections.BEHAVIOR.quoteTaglines = false;
	if (!retObj.cfgSections.BEHAVIOR.hasOwnProperty("shuffleTaglines"))
		retObj.cfgSections.BEHAVIOR.shuffleTaglines = true;
	if (!retObj.cfgSections.BEHAVIOR.hasOwnProperty("allowUserSettings"))
		retObj.cfgSections.BEHAVIOR.allowUserSettings = true;
	if (!retObj.cfgSections.BEHAVIOR.hasOwnProperty("useQuoteLineInitials"))
		retObj.cfgSections.BEHAVIOR.useQuoteLineInitials = true;
	if (!retObj.cfgSections.BEHAVIOR.hasOwnProperty("indentQuoteLinesWithInitials"))
		retObj.cfgSections.BEHAVIOR.indentQuoteLinesWithInitials = true;
	if (!retObj.cfgSections.BEHAVIOR.hasOwnProperty("enableTaglines"))
		retObj.cfgSections.BEHAVIOR.enableTaglines = false;
	if (!retObj.cfgSections.BEHAVIOR.hasOwnProperty("allowEditQuoteLines"))
		retObj.cfgSections.BEHAVIOR.allowEditQuoteLines = true;
	if (!retObj.cfgSections.BEHAVIOR.hasOwnProperty("allowSpellCheck"))
		retObj.cfgSections.BEHAVIOR.allowSpellCheck = true;
	if (!retObj.cfgSections.BEHAVIOR.hasOwnProperty("dictionaryFilenames"))
		retObj.cfgSections.BEHAVIOR.dictionaryFilenames = "en,en-US-supplemental";

	if (!retObj.cfgSections.hasOwnProperty("ICE_COLORS"))
		retObj.cfgSections.ICE_COLORS = {};
	if (!retObj.cfgSections.ICE_COLORS.hasOwnProperty("menuOptClassicColors"))
		retObj.cfgSections.ICE_COLORS.menuOptClassicColors = true;
	if (!retObj.cfgSections.ICE_COLORS.hasOwnProperty("ThemeFilename"))
		retObj.cfgSections.ICE_COLORS.ThemeFilename = "SlyIceColors_BlueIce.cfg";

	if (!retObj.cfgSections.hasOwnProperty("DCT_COLORS"))
		retObj.cfgSections.DCT_COLORS = {};
	if (!retObj.cfgSections.ICE_COLORS.hasOwnProperty("ThemeFilename"))
		retObj.cfgSections.DCT_COLORS.ThemeFilename = "SlyDCTColors_Default.cfg";
	return retObj;
}

// Saves the SlyEdit configuration file using the settings in gCfgInfo
//
// Return value: Boolean - Whether or not the save fully succeeded
function saveSlyEditCfgFile()
{
	var saveSucceeded = false;

	// If SlyEdit.cfg doesn't exist in the sbbs/mods directory, then copy it
	// from sbbs/ctrl
	// gCfgInfo.cfgFilename contains the full path & filename of the configuration
	// file
	var originalCfgFilename = "";
	if (gCfgInfo.cfgFilename.length > 0)
		originalCfgFilename = gCfgInfo.cfgFilename;
	else
		originalCfgFilename = system.ctrl_dir + gSlyEdCfgFileName;
	var modsSlyEditCfgFilename = system.mods_dir + gSlyEdCfgFileName;
	var modsSlyEditCfgFileExists = file_exists(modsSlyEditCfgFilename);
	if (!modsSlyEditCfgFileExists && file_exists(originalCfgFilename))
		modsSlyEditCfgFileExists = file_copy(originalCfgFilename, modsSlyEditCfgFilename);

	var cfgFile = new File(modsSlyEditCfgFilename);
	if (modsSlyEditCfgFileExists)
	{
		if (cfgFile.open("r+")) // Reading and writing (file must exist)
		{
			for (var settingName in gCfgInfo.cfgSections.BEHAVIOR)
				cfgFile.iniSetValue("BEHAVIOR", settingName, gCfgInfo.cfgSections.BEHAVIOR[settingName]);
			for (var settingName in gCfgInfo.cfgSections.ICE_COLORS)
				cfgFile.iniSetValue("ICE_COLORS", settingName, gCfgInfo.cfgSections.ICE_COLORS[settingName]);
			for (var settingName in gCfgInfo.cfgSections.DCT_COLORS)
				cfgFile.iniSetValue("DCT_COLORS", settingName, gCfgInfo.cfgSections.DCT_COLORS[settingName]);

			cfgFile.close();
			saveSucceeded = true;
		}
	}
	else
	{
		// Creae a new SlyEdit.cfg in sbbs/mods
		if (cfgFile.open("w"))
		{
			saveSucceeded = true;
			// Behavior section
			if (!cfgFile.writeln("[BEHAVIOR]"))
				saveSucceeded = false;
			for (var settingName in gCfgInfo.cfgSections.BEHAVIOR)
			{
				// Write any comments for this setting
				var comments = getIniFileCommentsForOpt(settingName, "BEHAVIOR");
				for (var i = 0; i < comments.length; ++i)
				{
					if (!cfgFile.writeln(comments[i]))
						saveSucceeded = false;
				}
				// Write the setting
				var settingLine = settingName + "=" + gCfgInfo.cfgSections.BEHAVIOR[settingName];
				if (!cfgFile.writeln(settingLine))
					saveSucceeded = false;
			}
			// ICE_COLORS section
			if (!cfgFile.writeln("[ICE_COLORS]"))
				saveSucceeded = false;
			for (var settingName in gCfgInfo.cfgSections.ICE_COLORS)
			{
				// Write any comments for this setting
				var comments = getIniFileCommentsForOpt(settingName, "ICE_COLORS");
				for (var i = 0; i < comments.length; ++i)
				{
					if (!cfgFile.writeln(comments[i]))
						saveSucceeded = false;
				}
				// Write the setting
				var settingLine = settingName + "=" + gCfgInfo.cfgSections.ICE_COLORS[settingName];
				if (!cfgFile.writeln(settingLine))
					saveSucceeded = false;
			}
			// DCT_COLORS section
			if (!cfgFile.writeln("[DCT_COLORS]"))
				saveSucceeded = false;
			for (var settingName in gCfgInfo.cfgSections.DCT_COLORS)
			{
				// Write any comments for this setting
				var comments = getIniFileCommentsForOpt(settingName, "DCT_COLORS");
				for (var i = 0; i < comments.length; ++i)
				{
					if (!cfgFile.writeln(comments[i]))
						saveSucceeded = false;
				}
				// Write the setting
				var settingLine = settingName + "=" + gCfgInfo.cfgSections.DCT_COLORS[settingName];
				if (!cfgFile.writeln(settingLine))
					saveSucceeded = false;
			}

			cfgFile.close();
		}
	}

	return saveSucceeded;
}

// Returns an array of INI file comments for a particular option (and section name)
function getIniFileCommentsForOpt(pOptName, pSectionName)
{
	var commentLines = [];
	if (pSectionName == "BEHAVIOR")
	{
		if (pOptName == "reWrapQuoteLines")
		{
			commentLines.push("; If the reWrapQuoteLines option is set to true, quote lines will be re-wrapped");
			commentLines.push("; so that they are complete but still look good when quoted.  If this option is");
			commentLines.push("; disabled, then quote lines will simply be trimmed to fit into the message.");
		}
		else if (pOptName == "allowColorSelection")
			commentLines.push("; Whether or not to let the user change the text color");
		else if (pOptName == "saveColorsAsANSI")
		{
			commentLines.push("; Whether or not to save message color/attribute codes as ANSI (if not, they");
			commentLines.push("; will be saved as Synchronet attribute codes)");
		}
		else if (pOptName == "allowCrossPosting")
			commentLines.push("; Whether or not to allow cross-posting");
		else if (pOptName == "enableTextReplacements")
		{
			commentLines.push("; Whether or not to enable text replacements (AKA macros).");
			commentLines.push("; enableTextReplacements can have one of the following values:");
			commentLines.push("; false : Text replacement is disabled");
			commentLines.push("; true  : Text replacement is enabled and performed as literal search and replace");
			commentLines.push("; regex : Text replacement is enabled using regular expressions");
		}
		else if (pOptName == "tagLineFilename")
			commentLines.push("; The name of the file where tag lines are stored");
		else if (pOptName == "taglinePrefix")
		{
			commentLines.push("; Text to add to the front of a tagline when adding it to the message.");
			commentLines.push("; This can be blank (nothing after the =) if no prefix is desired.");
		}
		else if (pOptName == "quoteTaglines")
			commentLines.push("; Whether or not to add double-quotes around taglines");
		else if (pOptName == "shuffleTaglines")
		{
			commentLines.push("; Whether or not to shuffle (randomize) the list of taglines when they are");
			commentLines.push("; displayed for the user to choose from");
		}
		else if (pOptName == "allowUserSettings")
			commentLines.push("; Whether or not to allow users to change their user settings.");
		//; The following settings serve as defaults for the user settings, which
		//; each user can change for themselves:
		else if (pOptName == "useQuoteLineInitials")
		{
			commentLines.push("; Whether or not to prefix the quote lines with the last author's initials");
			commentLines.push("; This also has a user option that takes precedence over this setting.");
		}
		else if (pOptName == "indentQuoteLinesWithInitials")
		{
			commentLines.push("; When prefixing quote lines with the last author's initials, whether or not");
			commentLines.push("; to indent the quote lines with a space.");
			commentLines.push("; This also has a user option that takes precedence over this setting.");
		}
		else if (pOptName == "enableTaglines")
		{
			commentLines.push("; Whether or not to enable the option to add a tagline");
			commentLines.push("; This also has a user option that takes precedence over this setting.");
		}
		else if (pOptName == "allowEditQuoteLine")
		{
			commentLines.push("; Whether or not to allow editing quote lines");
			commentLines.push("; This also has a user option that takes precedence over this setting.");
		}
		else if (pOptName == "allowSpellCheck")
		{
			commentLines.push("; Whether or not to allow spell check");
			commentLines.push("; This also has a user option that takes precedence over this setting.");
		}
		else if (pOptName == "dictionaryFilenames")
		{
			commentLines.push("; Dictionary filenames (used for spell check): This is a comma-separated list of");
			commentLines.push("; dictionary filenames.  The dictionary filenames are in the format");
			commentLines.push("; dictionary_<language>.txt, where <language> is the language name.  In this");
			commentLines.push("; list, the filenames can be in that format, or just <language>.txt, or just");
			commentLines.push("; <language>.  Leave blank to use all dictionary files that exist on the");
			commentLines.push("; system.  The dictionary files are located in either sbbs/mods, sbbs/ctrl, or");
			commentLines.push("; the same directory as SlyEdit.");
			commentLines.push("; This also has a user option that takes precedence over this setting.");
		}
	}
	else if (pSectionName == "ICE_COLORS")
	{
		if (pOptName == "ThemeFilename")
			commentLines.push("; The filename of the theme file (no leading path necessary)");
		else if (pOptName == "menuOptClassicColors")
			commentLines.push("; Whether or not to use all classic IceEdit colors (true/false)");
	}
	else if (pSectionName == "DCT_COLORS")
	{
		if (pOptName == "ThemeFilename")
			commentLines.push("; The filename of the theme file (no leading path necessary)");
	}
	return commentLines;
}