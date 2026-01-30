// SlyEdit configurator: This is a menu-driven configuration program/script for SlyEdit.
// Any changes are saved to SlyEdit.cfg in sbbs/mods, so that custom changes don't get
// overridden with SlyEdit.cfg in sbbs/ctrl due to an update.

"use strict";


require("sbbsdefs.js", "P_NONE");
require("uifcdefs.js", "UIFC_INMSG");


if (!uifc.init("SlyEdit 1.92f Configurator"))
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

// Show the main menu and go from there.
// This is in a loop so that if the user aborts from confirming to save
// settings, they'll return to the main menu.
var anyOptionChanged = false;
var continueOn = true;
while (continueOn)
{
	anyOptionChanged = doMainMenu(anyOptionChanged) || anyOptionChanged;
	// If any configuration option changed, prompt the user & save if the user wants to
	if (anyOptionChanged)
	{
		var userChoice = promptYesNo("Save configuration?", true, WIN_ORG|WIN_MID|WIN_ACT|WIN_ESC);
		if (typeof(userChoice) === "boolean")
		{
			if (userChoice)
			{
				if (saveSlyEditCfgFile())
					uifc.msg("Changes were successfully saved (to mods dir)");
				else
					uifc.msg("Failed to save settings!");
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
	// Create a CTX to specify the current selected item index
	var ctx = uifc.list.CTX();
	var helpText = "Behavior: Behavior settings\r\nStrings: Text string configuration\r\nIce Colors: Ice-related color settings\r\nDCT Colors: DCT-related color settings";
	// Selection
	var winMode = WIN_ORG|WIN_MID|WIN_ACT|WIN_ESC;
	var menuTitle = "SlyEdit Configuration";
	var anyOptionChanged = false;
	var continueOn = true;
	while (continueOn && !js.terminated)
	{
		uifc.help_text = word_wrap(helpText, gHelpWrapWidth);
		var selection = uifc.list(winMode, menuTitle, ["Behavior", "Strings", "Ice Colors", "DCT Colors"], ctx);
		ctx.cur = selection; // Remember the current selected item
		switch (selection)
		{
			case -1: // ESC
				continueOn = false;
				break;
			case 0: // Behavior
				anyOptionChanged = doBehaviorMenu() || anyOptionChanged;
				break;
			case 1: // Strings
				anyOptionChanged = doStringsMenu() || anyOptionChanged;
				break;
			case 2: // Ice colors
				anyOptionChanged = doIceColors() || anyOptionChanged;
				break;
			case 3: // DCT colors
				anyOptionChanged = doDCTColors() || anyOptionChanged;
				break;
		}
	}
	return anyOptionChanged;
}

// Configuration menu for the [STRINGS] section
function doStringsMenu()
{
	// For menu item text formatting
	var itemTextMaxLen = 40;

	// Create a CTX to specify the current selected item index
	var ctx = uifc.list.CTX();
	var helpText = "Strings filename: The filename containing the configurable strings";
	// Selection
	var winMode = WIN_ORG|WIN_MID|WIN_ACT|WIN_ESC;
	var menuTitle = "String section configuration";
	var anyOptionChanged = false;
	var continueOn = true;
	while (continueOn && !js.terminated)
	{
		uifc.help_text = word_wrap(helpText, gHelpWrapWidth);
		var menuItems = [];
		menuItems.push(formatCfgMenuText(itemTextMaxLen, "Strings filename", gCfgInfo.cfgSections.STRINGS.stringsFilename));
		var selection = uifc.list(winMode, menuTitle, menuItems, ctx);
		ctx.cur = selection; // Remember the current selected item
		switch (selection)
		{
			case -1: // ESC
				continueOn = false;
				break;
			case 0: // Strings filename
				var userInput = uifc.input(WIN_MID, "Strings filename", gCfgInfo.cfgSections.STRINGS.stringsFilename, 60, K_EDIT);
				if (typeof(userInput) === "string" && userInput != gCfgInfo.cfgSections.STRINGS.stringsFilename)
				{
					gCfgInfo.cfgSections.STRINGS.stringsFilename = userInput;
					anyOptionChanged = true;
				}
				break;
		}
	}
	return anyOptionChanged;
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
		"ctrlQQuote",

		"enableTextReplacements",
		"tagLineFilename",
		"taglinePrefix",
		"dictionaryFilenames",
		// Meme settings
		"memeMaxTextLen",    // Number
		"memeDefaultWidth",  // Number
		"memeStyleRandom",   // Boolean
		"memeDefaultBorder", // String
		"memeDefaultColor",  // Number
		"memeJustify"        // String
	];
	// Menu item text for the options:
	var optionStrs = [
		// Toggle options:
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
		"Allow/enable spell check",
		"Ctrl-Q to quote (if not, use Ctrl-Y)",
		// Other options:
		"Enable text replacements",
		"Tagline filename",
		"Tagline prefix",
		"Dictionary filenames",
		"Maximum meme text length",
		"Default meme width",
		"Random meme style",
		"Meme default border style",
		"Meme default color number",
		"Meme justification"
	];
	// Build the array of items to be displayed on the menu
	var menuItems = [];
	// Toggle (on/off) settings
	const enableTextReplacementsIdx = 15;
	var optionIdx = 0
	for (; optionIdx < enableTextReplacementsIdx; ++optionIdx)
		menuItems.push(formatCfgMenuText(itemTextMaxLen, optionStrs[optionIdx], gCfgInfo.cfgSections.BEHAVIOR[cfgOptProps[optionIdx]]));
	// Text replacements can be a boolean true/false or "regex"
	menuItems.push(formatCfgMenuText(itemTextMaxLen, optionStrs[optionIdx++], getTxtReplacementsVal()));
	menuItems.push(formatCfgMenuText(itemTextMaxLen, optionStrs[optionIdx++], gCfgInfo.cfgSections.BEHAVIOR.tagLineFilename));
	menuItems.push(formatCfgMenuText(itemTextMaxLen, optionStrs[optionIdx++], gCfgInfo.cfgSections.BEHAVIOR.taglinePrefix));
	//menuItems.push(formatCfgMenuText(itemTextMaxLen, optionStrs[optionIdx++], gCfgInfo.cfgSections.BEHAVIOR.dictionaryFilenames.substr(0,30)));
	menuItems.push(formatCfgMenuText(itemTextMaxLen, optionStrs[optionIdx++], gCfgInfo.cfgSections.BEHAVIOR.dictionaryFilenames));
	menuItems.push(formatCfgMenuText(itemTextMaxLen, optionStrs[optionIdx++], gCfgInfo.cfgSections.BEHAVIOR.memeMaxTextLen));
	menuItems.push(formatCfgMenuText(itemTextMaxLen, optionStrs[optionIdx++], gCfgInfo.cfgSections.BEHAVIOR.memeDefaultWidth));
	menuItems.push(formatCfgMenuText(itemTextMaxLen, optionStrs[optionIdx++], gCfgInfo.cfgSections.BEHAVIOR.memeStyleRandom));
	menuItems.push(formatCfgMenuText(itemTextMaxLen, optionStrs[optionIdx++], gCfgInfo.cfgSections.BEHAVIOR.memeDefaultBorder));
	menuItems.push(formatCfgMenuText(itemTextMaxLen, optionStrs[optionIdx++], gCfgInfo.cfgSections.BEHAVIOR.memeDefaultColor));
	menuItems.push(formatCfgMenuText(itemTextMaxLen, optionStrs[optionIdx++], gCfgInfo.cfgSections.BEHAVIOR.memeJustify));

	// A dictionary of help text for each option, indexed by the option name from the configuration file
	if (doBehaviorMenu.optHelp == undefined)
		doBehaviorMenu.optHelp = getOptionHelpText();
	if (doBehaviorMenu.mainScreenHelp == undefined)
		doBehaviorMenu.mainScreenHelp = getBehaviorScreenHelp(doBehaviorMenu.optHelp, cfgOptProps);

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
		uifc.help_text = doBehaviorMenu.mainScreenHelp;
		var optionMenuSelection = uifc.list(winMode, menuTitle, menuItems, doBehaviorMenu.ctx);
		doBehaviorMenu.ctx.cur = optionMenuSelection; // Remember the current selected item
		if (optionMenuSelection == -1) // ESC
			continueOn = false;
		else
		{
			var optName = cfgOptProps[optionMenuSelection];
			var itemType = typeof(gCfgInfo.cfgSections.BEHAVIOR[optName]);
			uifc.help_text = doBehaviorMenu.optHelp[optName];
			if (optName == "enableTextReplacements")
			{
				// Enable text replacements - This is above boolean because this
				// could be mistaken for a boolean if it's true or false
				// Store the current value wo we can see if it changes
				var valBackup = gCfgInfo.cfgSections.BEHAVIOR.enableTextReplacements;
				// Prompt the user
				var ctx = uifc.list.CTX();
				if (typeof(gCfgInfo.cfgSections.BEHAVIOR.enableTextReplacements) === "boolean")
					ctx.cur = gCfgInfo.cfgSections.BEHAVIOR.enableTextReplacements ? 0 : 1;
				else if (gCfgInfo.cfgSections.BEHAVIOR.enableTextReplacements.toLowerCase() === "regex")
					ctx.cur = 2;
				var txtReplacementsSelection = uifc.list(winMode, optionStrs[optionMenuSelection], ["Yes", "No", "Regex"], ctx);
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
			}
			else if (optName == "memeDefaultBorder")
			{
				// Default border style for memes
				var valBackup = gCfgInfo.cfgSections.BEHAVIOR.memeDefaultBorder;
				// Prompt the user
				var possibleOptions = ["none", "single", "mixed1", "mixed2", "mixed3", "double", "ornate1", "ornate2", "ornate3"];
				var ctx = uifc.list.CTX();
				var currentSelectedOptIdx = possibleOptions.indexOf(gCfgInfo.cfgSections.BEHAVIOR.memeDefaultBorder);
				if (currentSelectedOptIdx > -1)
					ctx.cur = currentSelectedOptIdx;
				var borderStyleSelection = uifc.list(winMode, optionStrs[optionMenuSelection], possibleOptions, ctx);
				if (borderStyleSelection >= 0 && borderStyleSelection < possibleOptions.length)
					gCfgInfo.cfgSections.BEHAVIOR.memeDefaultBorder = possibleOptions[borderStyleSelection];
				if (gCfgInfo.cfgSections.BEHAVIOR.memeDefaultBorder != valBackup)
				{
					anyOptionChanged = true;
					menuItems[optionMenuSelection] = formatCfgMenuText(itemTextMaxLen, "Border style", possibleOptions[borderStyleSelection]);
				}
			}
			else if (optName == "memeJustify")
			{
				// Text justification for memes
				var valBackup = gCfgInfo.cfgSections.BEHAVIOR.memeJustify;
				// Prompt the user
				var possibleOptions = ["left", "center", "right"];
				var ctx = uifc.list.CTX();
				var currentSelectedOptIdx = possibleOptions.indexOf(gCfgInfo.cfgSections.BEHAVIOR.memeJustify);
				if (currentSelectedOptIdx > -1)
					ctx.cur = currentSelectedOptIdx;
				var memeTextJustifySelection = uifc.list(winMode, optionStrs[optionMenuSelection], possibleOptions, ctx);
				if (memeTextJustifySelection >= 0 && memeTextJustifySelection < possibleOptions.length)
					gCfgInfo.cfgSections.BEHAVIOR.memeJustify = possibleOptions[memeTextJustifySelection];
				if (gCfgInfo.cfgSections.BEHAVIOR.memeJustify != valBackup)
				{
					anyOptionChanged = true;
					menuItems[optionMenuSelection] = formatCfgMenuText(itemTextMaxLen, "Meme text justification", possibleOptions[memeTextJustifySelection]);
				}
			}
			else if (itemType === "boolean")
			{
				gCfgInfo.cfgSections.BEHAVIOR[optName] = !gCfgInfo.cfgSections.BEHAVIOR[optName];
				anyOptionChanged = true;
				menuItems[optionMenuSelection] = formatCfgMenuText(itemTextMaxLen, optionStrs[optionMenuSelection], gCfgInfo.cfgSections.BEHAVIOR[optName]);
			}
			else if (itemType === "number")
			{
				var promptStr = optionStrs[optionMenuSelection];
				var initialVal = gCfgInfo.cfgSections.BEHAVIOR[optName].toString();
				var minVal = 1;
				var userInput = uifc.input(WIN_MID, promptStr, initialVal, 0, K_NUMBER|K_EDIT);
				if (typeof(userInput) === "string" && userInput.length > 0)
				{
					var value = parseInt(userInput);
					if (!isNaN(value) && value >= minVal && gCfgInfo.cfgSections.BEHAVIOR[optName] != value)
					{
						gCfgInfo.cfgSections.BEHAVIOR[optName] = value;
						anyOptionChanged = true;
						menuItems[optionMenuSelection] = formatCfgMenuText(itemTextMaxLen, optionStrs[optionMenuSelection], gCfgInfo.cfgSections.BEHAVIOR[optName]);
					}
				}
			}
			else if (optName == "tagLineFilename")
			{
				// Tagline filename
				var userInput = uifc.input(WIN_MID, optionStrs[optionMenuSelection], gCfgInfo.cfgSections.BEHAVIOR.tagLineFilename, 60, K_EDIT);
				if (typeof(userInput) === "string" && userInput != gCfgInfo.cfgSections.BEHAVIOR.tagLineFilename)
				{
					gCfgInfo.cfgSections.BEHAVIOR.tagLineFilename = userInput;
					anyOptionChanged = true;
					menuItems[optionMenuSelection] = formatCfgMenuText(itemTextMaxLen, "Tagline filename", userInput);
				}
			}
			else if (optName == "taglinePrefix")
			{
				// Tagline prefix
				var userInput = uifc.input(WIN_MID, optionStrs[optionMenuSelection], gCfgInfo.cfgSections.BEHAVIOR.taglinePrefix, 0, K_EDIT);
				if (typeof(userInput) === "string" && userInput != gCfgInfo.cfgSections.BEHAVIOR.taglinePrefix)
				{
					gCfgInfo.cfgSections.BEHAVIOR.tagLineFilename = userInput;
					anyOptionChanged = true;
					menuItems[optionMenuSelection] = formatCfgMenuText(itemTextMaxLen, "Tagline prefix", userInput);
				}
			}
			else if (optName == "dictionaryFilenames")
			{
				// Dictionary filenames
				var userInput = promptDictionaries();
				anyOptionChanged = (userInput != gCfgInfo.cfgSections.BEHAVIOR.dictionaryFilenames);
				gCfgInfo.cfgSections.BEHAVIOR.dictionaryFilenames = userInput;
			}
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

// Returns a dictionary of help text, indexed by the option name from the configuration file
function getOptionHelpText()
{
	var optionHelpText = {};
	optionHelpText["displayEndInfoScreen"] = "Display end info screen: Whether or not to display editor info when exiting";

	optionHelpText["userInputTimeout"] = "User input timeout: Whether or not to enable user inactivity timeout";

	optionHelpText["reWrapQuoteLines"] = "Re-wrap quote lines: If true, quote lines will be re-wrapped so that they are complete ";
	optionHelpText["reWrapQuoteLines"] += "but still look good when quoted.  If this option is disabled, then quote lines will ";
	optionHelpText["reWrapQuoteLines"] += "simply be trimmed to fit into the message. Users can change this for themselves too.";

	optionHelpText["useQuoteLineInitials"] = "Use author initials in quoted lines: Whether or not to prefix the quote ";
	optionHelpText["useQuoteLineInitials"] += "lines with the last author's initials. Users can change this for themselves too.";

	optionHelpText["indentQuoteLinesWithInitials"] = "Indent quoted lines with author initials: When prefixing quote lines with the last author's initials, ";
	optionHelpText["indentQuoteLinesWithInitials"] += "whether or not to indent the quote lines with a space. Users can change this for themselves too.";

	optionHelpText["allowEditQuoteLines"] = "Allow editing quote lines: Whether or not to allow editing quote lines";

	optionHelpText["allowUserSettings"] = "Allow user settings: Whether or not to allow users to change their user settings.";

	optionHelpText["allowColorSelection"] = "Allow color selection: Whether or not to let the user change the text color";

	optionHelpText["saveColorsAsANSI"] = "Save colors as ANSI: Whether or not to save message color/attribute codes as ANSI ";
	optionHelpText["saveColorsAsANSI"] += "(if not, they will be saved as Synchronet attribute codes)";

	optionHelpText["allowCrossPosting"] = "Allow cross-posting: Whether or not to allow cross-posting to multiple sub-boards";

	optionHelpText["enableTaglines"] = "Enable taglines: Whether or not to enable the option to add a tagline.";

	optionHelpText["shuffleTaglines"] = "Shuffle taglines: Whether or not to shuffle (randomize) the list of taglines when they are ";
	optionHelpText["shuffleTaglines"] += "displayed for the user to choose from";

	optionHelpText["quoteTaglines"] = "Double-quotes around tag lines: Whether or not to add double-quotes around taglines";

	optionHelpText["allowSpellCheck"] = "Allow/enable spell check: Whether or not to allow spell check";

	optionHelpText["ctrlQQuote"] = "Ctrl-Q to quote: Use Ctrl-Q hotkey to quote. If not, Ctrl-Y will be used. This is a default ";
	optionHelpText["ctrlQQuote"] += "value for a user setting; users are allowed to toggle this according to their preference ";
	optionHelpText["ctrlQQuote"] += "(also dependent on what terminal they're using, since Ctrl-Q can be used to toggle ";
	optionHelpText["ctrlQQuote"] += "XON/XOFF flow control or disconnect.";

	optionHelpText["enableTextReplacements"] = "Enable text replacements: Whether or not to enable text replacements (AKA macros). Can be ";
	optionHelpText["enableTextReplacements"] += "true, false, or 'regex' to use regular expressions.";

	optionHelpText["tagLineFilename"] = "Tagline filename: The name of the file where tag lines are stored. This file is loaded ";
	optionHelpText["tagLineFilename"] += "from the sbbs ctrl directory.";

	optionHelpText["taglinePrefix"] = "Tagline prefix: Text to add to the front of a tagline when adding it to the message. This ";
	optionHelpText["taglinePrefix"] += "can be blank (nothing after the =) if no prefix is desired.";

	optionHelpText["dictionaryFilenames"] = "Dictionary filenames: These are dictionaries to use for spell check.  ";
	optionHelpText["dictionaryFilenames"] += "The dictionary filenames are in the format dictionary_<language>.txt, where ";
	optionHelpText["dictionaryFilenames"] += "<language> is the language name.  The dictionary files are located in either ";
	optionHelpText["dictionaryFilenames"] += "sbbs/mods, sbbs/ctrl, or the same directory as SlyEdit. Users can change ";
	optionHelpText["dictionaryFilenames"] += "this for themselves too.";

	optionHelpText["memeMaxTextLen"] = "Maximum meme text length: The maximum text length allowed for memes. A 'meme' for messages ";
	optionHelpText["memeMaxTextLen"] += "is a paragraph of text in a stylized box with an optional border and background color.";

	optionHelpText["memeDefaultWidth"] = "Default meme width: The default width of a meme box";

	optionHelpText["memeStyleRandom"] = "Random meme style: For meme input, whether to use an initially random meme style (color & border style).";

	optionHelpText["memeDefaultBorder"] = "Meme default border style: The default border style for meme input.";

	optionHelpText["memeDefaultColor"] = "Meme default color number: The default color (number) for meme input.";

	optionHelpText["memeJustify"] = "Meme justification: The text justification for meme input (left, center, right).";

	// Word-wrap the help text items
	for (var prop in optionHelpText)
		optionHelpText[prop] = word_wrap(optionHelpText[prop], gHelpWrapWidth);

	return optionHelpText;
}

// Returns help text for the behavior configuration screen
//
// Parameters:
//  pOptionHelpText: An object of help text for each option, indexed by the option name from the configuration file
//  pCfgOptProps: An array specifying the properties to include in the help text and their order
//
// Return value: Help text for the behavior options screen
function getBehaviorScreenHelp(pOptionHelpText, pCfgOptProps)
{
	if (getBehaviorScreenHelp.help == undefined)
	{
		getBehaviorScreenHelp.help = "This screen allows you to configure behavior options for SlyEdit.\r\n\r\n";
		for (var i = 0; i < pCfgOptProps.length; ++i)
		{
			var optName = pCfgOptProps[i];
			getBehaviorScreenHelp.help += pOptionHelpText[optName] + "\r\n\r\n";
		}
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

	if (!file_exists(retObj.cfgFilename) && file_exists(system.ctrl_dir + "SlyEdit.example.cfg"))
		retObj.cfgFilename = system.ctrl_dir + "SlyEdit.example.cfg";

	var cfgFile = new File(retObj.cfgFilename);
	if (cfgFile.open("r"))
	{
		var iniSectionNames = cfgFile.iniGetSections();
		for (var i = 0; i < iniSectionNames.length; ++i)
			retObj.cfgSections[iniSectionNames[i]] = cfgFile.iniGetObject(iniSectionNames[i]);
		cfgFile.close();

		print("");
		print("- Loaded config file: " + retObj.cfgFilename);
		print("")
	}
	// In case some settings weren't loaded, add defaults
	if (!retObj.cfgSections.hasOwnProperty("BEHAVIOR"))
		retObj.cfgSections.BEHAVIOR = {};
	if (!retObj.cfgSections.BEHAVIOR.hasOwnProperty("displayEndInfoScreen"))
		retObj.cfgSections.BEHAVIOR.displayEndInfoScreen = true;
	if (!retObj.cfgSections.BEHAVIOR.hasOwnProperty("userInputTimeout"))
		retObj.cfgSections.BEHAVIOR.userInputTimeout = true;
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
	if (!retObj.cfgSections.BEHAVIOR.hasOwnProperty("ctrlQQuote"))
		retObj.cfgSections.BEHAVIOR.ctrlQQuote = true;
	if (!retObj.cfgSections.BEHAVIOR.hasOwnProperty("dictionaryFilenames"))
		retObj.cfgSections.BEHAVIOR.dictionaryFilenames = "en,en-US-supplemental";
	if (!retObj.cfgSections.BEHAVIOR.hasOwnProperty("memeMaxTextLen"))
		retObj.cfgSections.BEHAVIOR.memeMaxTextLen = 500;
	if (!retObj.cfgSections.BEHAVIOR.hasOwnProperty("memeDefaultWidth"))
		retObj.cfgSections.BEHAVIOR.memeDefaultWidth = 39;
	if (!retObj.cfgSections.BEHAVIOR.hasOwnProperty("memeStyleRandom"))
		retObj.cfgSections.BEHAVIOR.memeStyleRandom = false;
	if (!retObj.cfgSections.BEHAVIOR.hasOwnProperty("memeDefaultBorder"))
		retObj.cfgSections.BEHAVIOR.memeDefaultBorder = "double";
	if (!retObj.cfgSections.BEHAVIOR.hasOwnProperty("memeDefaultColor"))
		retObj.cfgSections.BEHAVIOR.memeDefaultColor = 4;
	if (!retObj.cfgSections.BEHAVIOR.hasOwnProperty("memeJustify"))
		retObj.cfgSections.BEHAVIOR.memeJustify = "center";

	if (!retObj.cfgSections.hasOwnProperty("STRINGS"))
	{
		retObj.cfgSections.STRINGS = {
			stringsFilename: "SlyEditStrings_En.cfg"
		};
	}

	if (!retObj.cfgSections.hasOwnProperty("ICE_COLORS"))
		retObj.cfgSections.ICE_COLORS = {};
	if (!retObj.cfgSections.ICE_COLORS.hasOwnProperty("menuOptClassicColors"))
		retObj.cfgSections.ICE_COLORS.menuOptClassicColors = true;
	if (!retObj.cfgSections.ICE_COLORS.hasOwnProperty("ThemeFilename"))
		retObj.cfgSections.ICE_COLORS.ThemeFilename = "SlyIceColors_BlueIce.cfg";

	if (!retObj.cfgSections.hasOwnProperty("DCT_COLORS"))
		retObj.cfgSections.DCT_COLORS = {};
	if (!retObj.cfgSections.DCT_COLORS.hasOwnProperty("ThemeFilename"))
		retObj.cfgSections.DCT_COLORS.ThemeFilename = "SlyDCTColors_Default.cfg";
	return retObj;
}

// Saves the SlyEdit configuration file using the settings in gCfgInfo
//
// Return value: Boolean - Whether or not the save fully succeeded
function saveSlyEditCfgFile()
{
	// If SlyEdit.cfg doesn't exist in the sbbs/mods directory, then copy it
	// from sbbs/ctrl to sbbs/mods
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
	{
		if (!file_copy(originalCfgFilename, modsSlyEditCfgFilename))
			return false;
	}

	// Open the configuration file and save the current settings to it
	var saveSucceeded = false;
	var cfgFile = new File(modsSlyEditCfgFilename);
	if (cfgFile.open(cfgFile.exists ? "r+" : "w+")) // r+: Reading and writing (file must exist)
	{
		var behaviorSetSuccessful = cfgFile.iniSetObject("BEHAVIOR", gCfgInfo.cfgSections.BEHAVIOR);
		var stringsSetSuccessful = cfgFile.iniSetObject("STRINGS", gCfgInfo.cfgSections.STRINGS);
		var iceColorsSetSuccessful = cfgFile.iniSetObject("ICE_COLORS", gCfgInfo.cfgSections.ICE_COLORS);
		var dctColorsSetSuccessful = cfgFile.iniSetObject("DCT_COLORS", gCfgInfo.cfgSections.DCT_COLORS);

		cfgFile.close();
		saveSucceeded = behaviorSetSuccessful && stringsSetSuccessful && iceColorsSetSuccessful && dctColorsSetSuccessful;
	}

	return saveSucceeded;
}
