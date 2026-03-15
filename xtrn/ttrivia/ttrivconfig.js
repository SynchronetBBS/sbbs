// Configurator for Tournament Trivia - Menu-driven configuration for
// Tournament Trivia settings.  Settings are read from and saved to
// data/settings.json in the game directory.

"use strict";

require("sbbsdefs.js", "P_NONE");
require("uifcdefs.js", "UIFC_INMSG");
require(js.exec_dir + "ttriv_game.js", "parseArgs");


if (!uifc.init("Tournament Trivia Configurator"))
{
	print("Failed to initialize uifc");
	exit(1);
}
js.on_exit("uifc.bail()");


// Game and data directories
var gDataDir = fullpath(js.exec_dir) + "data/";
var gSettingsFilename = gDataDir + "settings.json";

// Parse command-line arguments for the settings filename
var argsObj = parseArgs(argv);
if (argsObj.settingsFilename.length > 0)
{
	// If no full path is specified, then assume the filename is in the data dir
	var justFilename = file_getname(argsObj.settingsFilename);
	if (argsObj.settingsFilename.indexOf(justFilename) == 0)
		gSettingsFilename = gDataDir + justFilename;
	else
		gSettingsFilename = argsObj.settingsFilename;
}

if (!file_exists(gSettingsFilename))
{
	saveSettings(gSettingsFilename, DEFAULT_SETTINGS);
	uifc.msg(format("Created new settings file: %s", file_getname(gSettingsFilename)));
}

// Help text wrap width
var gHelpWrapWidth = uifc.screen_width - 10;

// Default settings (must match ttriv_game.js DEFAULT_SETTINGS)
var gDefaultSettings = {
	maxClues: 3,
	questionFrequency: 50,
	listSysops: true,
	playerTimeout: 300,
	questionFiles: ["database.enc", "custom.txt"]
};

// Read the settings file
var gSettings = readSettings();

// Show the main menu and go from there.
// This is in a loop so that if the user aborts from confirming to save
// settings, they'll return to the main menu.
var anyOptionChanged = false;
var continueOn = true;
while (continueOn)
{
	anyOptionChanged = doMainMenu() || anyOptionChanged;
	if (anyOptionChanged)
	{
		var userChoice = promptYesNo("Save configuration?", true, WIN_ORG|WIN_MID|WIN_ACT|WIN_ESC);
		if (typeof(userChoice) === "boolean")
		{
			if (userChoice)
			{
				if (saveSettings())
					uifc.msg("Settings saved successfully.");
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
	// For menu item text formatting
	var itemTextMaxLen = 40;

	// Configuration option properties (parallel arrays with optionStrs)
	var cfgOptProps = [
		"questionFrequency",
		"maxClues",
		"playerTimeout",
		"listSysops",
		"questionFiles"
	];
	// Display strings for the options
	var optionStrs = [
		"Question Frequency (seconds)",
		"Max Clues Per Question",
		"Player Timeout (seconds)",
		"Show Sysops on Score List",
		"Question Files"
	];

	// Build the menu items array
	var menuItems = buildMenuItems(itemTextMaxLen, cfgOptProps, optionStrs);

	// Help text
	if (doMainMenu.optHelp == undefined)
		doMainMenu.optHelp = getOptionHelpText();
	if (doMainMenu.mainScreenHelp == undefined)
		doMainMenu.mainScreenHelp = getMainHelp(doMainMenu.optHelp, cfgOptProps);

	// Create a CTX to track the current selected item index
	if (doMainMenu.ctx == undefined)
		doMainMenu.ctx = uifc.list.CTX();

	var winMode = WIN_ORG|WIN_MID|WIN_ACT|WIN_ESC;
	var menuTitle = "Tournament Trivia Configuration";
	var anyOptionChanged = false;
	var continueOn = true;
	while (continueOn && !js.terminated)
	{
		uifc.help_text = doMainMenu.mainScreenHelp;
		var selection = uifc.list(winMode, menuTitle, menuItems, doMainMenu.ctx);
		doMainMenu.ctx.cur = selection;
		if (selection == -1)
			continueOn = false;
		else
		{
			var optName = cfgOptProps[selection];
			uifc.help_text = doMainMenu.optHelp[optName];

			if (optName == "questionFrequency")
			{
				var userInput = uifc.input(WIN_MID, "Question Frequency (25-75 sec)", gSettings.questionFrequency.toString(), 0, K_NUMBER|K_EDIT);
				if (typeof(userInput) === "string" && userInput.length > 0)
				{
					var val = parseInt(userInput);
					if (!isNaN(val) && val >= 25 && val <= 75 && val != gSettings.questionFrequency)
					{
						gSettings.questionFrequency = val;
						anyOptionChanged = true;
						menuItems = buildMenuItems(itemTextMaxLen, cfgOptProps, optionStrs);
					}
					else if (!isNaN(val) && (val < 25 || val > 75))
						uifc.msg("Value must be between 25 and 75.");
				}
			}
			else if (optName == "maxClues")
			{
				var userInput = uifc.input(WIN_MID, "Max Clues (0-4)", gSettings.maxClues.toString(), 0, K_NUMBER|K_EDIT);
				if (typeof(userInput) === "string" && userInput.length > 0)
				{
					var val = parseInt(userInput);
					if (!isNaN(val) && val >= 0 && val <= 4 && val != gSettings.maxClues)
					{
						gSettings.maxClues = val;
						anyOptionChanged = true;
						menuItems = buildMenuItems(itemTextMaxLen, cfgOptProps, optionStrs);
					}
					else if (!isNaN(val) && (val < 0 || val > 4))
						uifc.msg("Value must be between 0 and 4.");
				}
			}
			else if (optName == "playerTimeout")
			{
				var userInput = uifc.input(WIN_MID, "Player Timeout (60-600 sec)", gSettings.playerTimeout.toString(), 0, K_NUMBER|K_EDIT);
				if (typeof(userInput) === "string" && userInput.length > 0)
				{
					var val = parseInt(userInput);
					if (!isNaN(val) && val >= 60 && val <= 600 && val != gSettings.playerTimeout)
					{
						gSettings.playerTimeout = val;
						anyOptionChanged = true;
						menuItems = buildMenuItems(itemTextMaxLen, cfgOptProps, optionStrs);
					}
					else if (!isNaN(val) && (val < 60 || val > 600))
						uifc.msg("Value must be between 60 and 600.");
				}
			}
			else if (optName == "listSysops")
			{
				gSettings.listSysops = !gSettings.listSysops;
				anyOptionChanged = true;
				menuItems = buildMenuItems(itemTextMaxLen, cfgOptProps, optionStrs);
			}
			else if (optName == "questionFiles")
			{
				var filesChanged = doQuestionFilesMenu();
				if (filesChanged)
				{
					anyOptionChanged = true;
					menuItems = buildMenuItems(itemTextMaxLen, cfgOptProps, optionStrs);
				}
			}
		}
	}

	return anyOptionChanged;
}

// Sub-menu for editing question files
function doQuestionFilesMenu()
{
	var anyChanged = false;
	var continueOn = true;

	while (continueOn && !js.terminated)
	{
		// Build menu showing current question files
		var menuItems = [];
		for (var i = 0; i < 10; ++i)
		{
			var fname = (i < gSettings.questionFiles.length && gSettings.questionFiles[i])
						? gSettings.questionFiles[i] : "(none)";
			menuItems.push(format("File #%-2d  %s", i + 1, fname));
		}

		uifc.help_text = word_wrap("Select a question file slot to edit. Each slot can hold a filename "
			+ "for a question database file. Files ending in .enc are treated as encoded; "
			+ "plain text files should have one question per two lines (question on the first "
			+ "line, answer on the second).", gHelpWrapWidth);

		var selection = uifc.list(WIN_ORG|WIN_MID|WIN_ACT|WIN_ESC, "Question Files", menuItems);
		if (selection == -1)
			continueOn = false;
		else
		{
			var currentVal = (selection < gSettings.questionFiles.length && gSettings.questionFiles[selection])
							 ? gSettings.questionFiles[selection] : "";
			var userInput = uifc.input(WIN_MID, "Filename (blank for none)", currentVal, 40, K_EDIT);
			if (typeof(userInput) === "string" && userInput != currentVal)
			{
				// Ensure the array is long enough
				while (gSettings.questionFiles.length <= selection)
					gSettings.questionFiles.push("");
				gSettings.questionFiles[selection] = userInput;
				anyChanged = true;
			}
		}
	}

	return anyChanged;
}

// Builds the array of formatted menu item strings
function buildMenuItems(itemTextMaxLen, cfgOptProps, optionStrs)
{
	var menuItems = [];
	for (var i = 0; i < cfgOptProps.length; ++i)
	{
		var propName = cfgOptProps[i];
		menuItems.push(formatCfgMenuText(itemTextMaxLen, optionStrs[i], gSettings[propName]));
	}
	return menuItems;
}

// Formats text for a configuration menu option
function formatCfgMenuText(pItemTextMaxLen, pItemText, pVal)
{
	if (formatCfgMenuText.formatStr == undefined)
		formatCfgMenuText.formatStr = "%-" + pItemTextMaxLen + "s %s";
	var value = "";
	var valType = typeof(pVal);
	if (valType === "boolean")
		value = pVal ? "Yes" : "No";
	else if (pVal instanceof Array)
	{
		// Show non-empty filenames joined by comma
		var names = [];
		for (var i = 0; i < pVal.length; ++i)
		{
			if (pVal[i] && pVal[i].length > 0)
				names.push(pVal[i]);
		}
		value = names.length > 0 ? names.join(", ") : "(none)";
	}
	else if (typeof(pVal) !== "undefined")
		value = pVal.toString();
	return format(formatCfgMenuText.formatStr, pItemText.substr(0, pItemTextMaxLen), value);
}

// Prompts the user Yes/No for a boolean response
function promptYesNo(pQuestion, pInitialVal, pWinMode)
{
	var chosenVal = null;
	var winMode = typeof(pWinMode) === "number" ? pWinMode : WIN_MID;
	var ctx = uifc.list.CTX();
	ctx.cur = typeof(pInitialVal) === "boolean" && pInitialVal ? 0 : 1;
	switch (uifc.list(winMode, pQuestion, ["Yes", "No"], ctx))
	{
		case -1:
			break;
		case 0:
			chosenVal = true;
			break;
		case 1:
			chosenVal = false;
			break;
		default:
			break;
	}
	return chosenVal;
}


///////////////////////////////////////////////////////////
// Help text functions

function getOptionHelpText()
{
	var optionHelpText = {};

	optionHelpText["questionFrequency"] = "Question Frequency: How many seconds between automatic question changes. "
		+ "Valid range is 25 to 75 seconds. The default is 50 seconds. Clue timing is "
		+ "derived from this value divided by the number of clues plus one.";

	optionHelpText["maxClues"] = "Max Clues Per Question: The maximum number of clues given per question "
		+ "before time runs out. Valid range is 0 to 4. The default is 3. More clues "
		+ "make the game easier; fewer clues make it harder and reward players who answer "
		+ "quickly with more points.";

	optionHelpText["playerTimeout"] = "Player Timeout: How many seconds of inactivity before a player is "
		+ "considered stale and removed from the active player list. Valid range is 60 to "
		+ "600 seconds. The default is 300 seconds (5 minutes).";

	optionHelpText["listSysops"] = "Show Sysops on Score List: Whether or not sysop players appear on the "
		+ "top 10 high scores list. Toggle this option to change between Yes and No. "
		+ "The default is Yes.";

	optionHelpText["questionFiles"] = "Question Files: Up to 10 question database files can be configured. "
		+ "Files ending in .enc are treated as encoded question databases. Plain text "
		+ "files (e.g. custom.txt) should contain one question per two lines: the question "
		+ "on the first line and the answer on the second. Files are loaded from the game directory.";

	// Word-wrap the help text items
	for (var prop in optionHelpText)
		optionHelpText[prop] = word_wrap(optionHelpText[prop], gHelpWrapWidth);

	return optionHelpText;
}

function getMainHelp(pOptionHelpText, pCfgOptProps)
{
	var helpText = "This screen allows you to configure settings for Tournament Trivia.\r\n\r\n";
	for (var i = 0; i < pCfgOptProps.length; ++i)
		helpText += pOptionHelpText[pCfgOptProps[i]] + "\r\n";
	return word_wrap(helpText, gHelpWrapWidth);
}


///////////////////////////////////////////////////////////
// Settings file I/O

function readSettings()
{
	var settings = null;

	if (file_exists(gSettingsFilename))
	{
		var f = new File(gSettingsFilename);
		if (f.open("r"))
		{
			var text = f.read();
			f.close();
			try
			{
				settings = JSON.parse(text);
			}
			catch (e)
			{
				settings = null;
			}
		}
	}

	// Apply defaults for any missing properties
	if (!settings)
		settings = {};
	if (typeof(settings.maxClues) !== "number")
		settings.maxClues = gDefaultSettings.maxClues;
	if (typeof(settings.questionFrequency) !== "number")
		settings.questionFrequency = gDefaultSettings.questionFrequency;
	if (typeof(settings.listSysops) !== "boolean")
		settings.listSysops = gDefaultSettings.listSysops;
	if (typeof(settings.playerTimeout) !== "number")
		settings.playerTimeout = gDefaultSettings.playerTimeout;
	if (!(settings.questionFiles instanceof Array))
		settings.questionFiles = gDefaultSettings.questionFiles.slice();

	return settings;
}

function saveSettings()
{
	// Create data directory if needed
	if (!file_isdir(gDataDir))
		mkdir(gDataDir);

	var f = new File(gSettingsFilename);
	if (!f.open("w"))
		return false;
	f.write(JSON.stringify(gSettings, null, 2));
	f.close();
	return true;
}
