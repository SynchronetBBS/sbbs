// This is a configurator for Groupie (newsgroup reader) that lets you (as the sysop)
// manage news server accounts for your users.

"use strict";


require("sbbsdefs.js", "P_NONE");
require("uifcdefs.js", "UIFC_INMSG");
require("nntp_client_lib.js", "NNTPConnectOptions");


if (!uifc.init("Groupie News Server Account Configurator"))
{
	print("Failed to initialize uifc");
	exit(1);
}
js.on_exit("uifc.bail()");


// Username sort options for listing users (i.e., when configuring
// user settings for the NNTP server)
const USER_SORT_USER_NUMBER = 0;
const USER_SORT_ALIAS = 1;
const USER_SORT_REAL_NAME = 2;

// Groupie base configuration filename, and help text wrap width
const gGroupieCfgFileName = "groupie.ini";
var gHelpWrapWidth = uifc.screen_width - 10;

// Read the Groupie configuration file
var gCfgInfo = readGroupieCfgFile(gGroupieCfgFileName);

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
				var saveRetObj = saveGroupieCfgFile();
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
		"groupie_cfg_user_sorting",             // Username sorting for groupie_cfg
		"default_server_password",              // String
		"use_lightbar_interface",               // Boolean
		"users_can_change_their_nttp_settings", // Boolean
		"receive_bufer_size_bytes",             // Numeric
		"receive_timeout_seconds",              // Numeric
		"hostname",                             // String
		"host_port",                            // Numeric
		//"prepend_foward_msg_subject",          // Boolean
		"default_max_num_msgs_from_newsgroup",  // Numeric
		"msg_save_dir",                         // String
		"newsgroup_list_hdr_filename_base",     // String
		"newsgroup_list_hdr_max_num_lines",     // Numeric
		"article_list_hdr_filename_base",       // String
		"article_list_hdr_max_num_lines",       // Numeric
		"theme_config_filename"                 // String
	];
	// Strings for the options to display on the menu
	var optionStrs = [
		"Username sorting for groupie_cfg",
		"Default server password for users",
		"Use Lightbar/Scrolling Interface",
		"Users can change their NNTP settings",
		"Receive buffer size (bytes)",
		"Receive timeout (in seconds)",
		"NNTP server hostname/IP",
		"NNTP server port",
		//"Prepend forwarded msgs w/ 'fwd'",
		"Default max # articles from newsgroup",
		"Message/article save dir (server PC)",
		"Newsgroup list hdr filename base",
		"Newsgroup list hdr max # lines",
		"Article list hdr filename base",
		"Article list hdr max # lines",
		"Theme configuration filename"
	];
	// Build an array of formatted string to be displayed on the menu
	// (the value formatting will depend on the variable type)
	var menuItems = [];
	menuItems.push("User account configuration");
	for (var i = 0; i < cfgOptProps.length; ++i)
	{
		var propName = cfgOptProps[i];
		menuItems.push(formatCfgMenuText(itemTextMaxLen, cfgOptProps[i], optionStrs[i], gCfgInfo.cfgOptions[propName]));
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
	var menuTitle = "Groupie Newsgroup Reader Configuration";
	var anyOptionChanged = false;
	var continueOn = true;
	while (continueOn && !js.terminated)
	{
		uifc.help_text = doMainMenu.mainScreenHelp;
		var optionMenuSelection = uifc.list(winMode, menuTitle, menuItems, doMainMenu.ctx);
		doMainMenu.ctx.cur = optionMenuSelection; // Remember the current selected item
		switch (optionMenuSelection)
		{
			case -1: // ESC
				continueOn = false;
				break;
			case 0: // User account configuration
				doUserAccountConfig();
				break;
			case 1: // Username sorting for groupie_cfg
				var propName = "groupie_cfg_user_sorting";
				var chooseSortRetObj = chooseGroupieCfgUsernameSortOpt(gCfgInfo.cfgOptions[propName]);
				if (!chooseSortRetObj.userAborted)
				{
					var optionArrayIdx = 1;
					var originalVal = gCfgInfo.cfgOptions[propName];
					gCfgInfo.cfgOptions[propName] = chooseSortRetObj.usernameSortOpt;
					menuItems[optionArrayIdx] = formatCfgMenuText(itemTextMaxLen, cfgOptProps[optionArrayIdx], optionStrs[optionArrayIdx], userSortOptValToStr(gCfgInfo.cfgOptions[propName]));
					anyOptionChanged = anyOptionChanged || (gCfgInfo.cfgOptions[propName] != originalVal);
				}
				break;
			default:
				var optionArrayIdx = optionMenuSelection - 1; // Because the 1st option is user account configuration
				var propName = cfgOptProps[optionArrayIdx];
				var originalVal = gCfgInfo.cfgOptions[propName];
				var optionType = typeof(gCfgInfo.cfgOptions[propName]);
				if (optionType === "boolean")
				{
					var userChoice = promptYesNo(optionStrs[optionArrayIdx], gCfgInfo.cfgOptions[propName], WIN_ORG|WIN_MID|WIN_ACT|WIN_ESC);
					if (typeof(userChoice) === "boolean")
					{
						gCfgInfo.cfgOptions[propName] = userChoice;
						menuItems[optionArrayIdx+1] = formatCfgMenuText(itemTextMaxLen, cfgOptProps[optionArrayIdx], optionStrs[optionArrayIdx], gCfgInfo.cfgOptions[propName]);
						anyOptionChanged = anyOptionChanged || (gCfgInfo.cfgOptions[propName] != originalVal);
					}
				}
				else if (optionType === "string")
				{
					gCfgInfo.cfgOptions[propName] = uifc.input(WIN_MID, optionStrs[optionArrayIdx], gCfgInfo.cfgOptions[propName], 0, K_EDIT);
					menuItems[optionArrayIdx+1] = formatCfgMenuText(itemTextMaxLen, cfgOptProps[optionArrayIdx], optionStrs[optionArrayIdx], gCfgInfo.cfgOptions[propName]);
					anyOptionChanged = anyOptionChanged || (gCfgInfo.cfgOptions[propName] != originalVal);
				}
				else if (optionType === "number")
				{
					var valStr = uifc.input(WIN_MID, optionStrs[optionArrayIdx], gCfgInfo.cfgOptions[propName].toString(), 0, K_NUMBER|K_EDIT);
					var valNum = parseInt(valStr);
					if (!isNaN(valNum))
					{
						gCfgInfo.cfgOptions[propName] = valNum;
						menuItems[optionArrayIdx+1] = formatCfgMenuText(itemTextMaxLen, cfgOptProps[optionArrayIdx], optionStrs[optionArrayIdx], gCfgInfo.cfgOptions[propName].toString());
						anyOptionChanged = anyOptionChanged || (gCfgInfo.cfgOptions[propName] != originalVal);
					}
				}
				break;
		}
	}

	return anyOptionChanged;
}

// Formats text for a behavior option
//
// Parameters:
//  pItemTextMaxLen: The maximum length for menu item text
//  pSettingName: The name of the configuration setting
//  pItemText: The text of the item to be displayed on the menu
//  pVal: The value of the option
//
// Return value: The formatted text for the menu item, with a Yes/No value indicating whether it's enabled
function formatCfgMenuText(pItemTextMaxLen, pSettingName, pItemText, pVal)
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
		{
			if (pSettingName == "groupie_cfg_user_sorting")
				value = userSortOptValToStr(pVal);
			else
				value = pVal.toString();
		}
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

// Performs the user account configuration for the NNTP server
function doUserAccountConfig()
{
	var menuOptStrs = [
		"Invididual user accounts",
		"Bulk change settings for existing NNTP accounts"
	];

	// Create a CTX to specify the current selected item index
	if (doUserAccountConfig.ctx == undefined)
		doUserAccountConfig.ctx = uifc.list.CTX();
	// Selection
	var winMode = WIN_ORG|WIN_MID|WIN_ACT|WIN_ESC;
	var menuTitle = "NNTP Server Account Management";
	var anyOptionChanged = false;
	var continueOn = true;
	while (continueOn && !js.terminated)
	{
		//uifc.help_text = doUserAccountConfig.mainScreenHelp;
		var optionMenuSelection = uifc.list(winMode, menuTitle, menuOptStrs, doUserAccountConfig.ctx);
		doUserAccountConfig.ctx.cur = optionMenuSelection; // Remember the current selected item
		switch (optionMenuSelection)
		{
			case -1: // ESC
				continueOn = false;
				break;
			case 0: // Individual NNTP user accounts
				editIndividualNNTPUserAccounts();
				break;
			case 1: // Bulk change settings for existing NNTP accounts
				bulkEditExistingNNTPAccountInformation();
				break;
		}
	}
}

// Performs editing of individual NNTP server user accounts
function editIndividualNNTPUserAccounts()
{
	// Get an array of the account names and dictionary of user names
	// to numbers
	var userInfo = getUserAcctNamesAndNumbers();

	// Create a CTX to specify the current selected item index
	if (editIndividualNNTPUserAccounts.ctx == undefined)
		editIndividualNNTPUserAccounts.ctx = uifc.list.CTX();
	// Selection
	var winMode = WIN_ORG|WIN_MID|WIN_ACT|WIN_ESC;
	var menuTitle = "Users";
	var anyOptionChanged = false;
	var continueOn = true;
	while (continueOn && !js.terminated)
	{
		//uifc.help_text = editIndividualNNTPUserAccounts.mainScreenHelp;
		var optionMenuSelection = uifc.list(winMode, menuTitle, userInfo.userAccountNames, editIndividualNNTPUserAccounts.ctx);
		editIndividualNNTPUserAccounts.ctx.cur = optionMenuSelection; // Remember the current selected item
		if (optionMenuSelection == -1) // ESC
			continueOn = false;
		else
		{
			var username = userInfo.userAccountNames[optionMenuSelection];
			editNNTPServerAcctInfoForUser(username, userInfo.userAccountNumsByUsername[username]);
		}
	}
}
// Helper for editIndividualNNTPUserAccounts(): Gets an array of
// user account names (possibly sorted) and an object mapping those
// names to account numbers
function getUserAcctNamesAndNumbers()
{
	var retObj = {
		userAccountNames: [],
		userAccountNumsByUsername: {}
	};

	// Build the array of the account names and dictionary of user names
	// to numbers
	for (var userNum = 1; userNum <= system.lastuser; ++userNum)
	{
		// Load the user record
		try
		{
			var theUser = new User(userNum);

			// If this user account is deleted, inactive, or the guest or a QWK account, then skip it
			if (Boolean(theUser.settings & USER_DELETED) || Boolean(theUser.settings & USER_INACTIVE) ||
				Boolean(theUser.security.restrictions & UFLAG_G) || theUser.compare_ars("REST Q"))
			{
				continue;
			}

			var username = getUserName(theUser);
			retObj.userAccountNames.push(username);
			retObj.userAccountNumsByUsername[username] = theUser.number;
		}
		catch (error)
		{
			log(LOG_ERR, format("Groupie config - Error loading user number %d: %s", userNum, error));
		}
	}
	// Sort the user list as configured, if applicable.
	// The names will be added as either the user's real name followed by
	// alias or alias followed by real name, depending on the sort option.
	// So if the sort option isn't user number (no specific sorting), we
	// just need to sort them.
	if (gCfgInfo.cfgOptions.groupie_cfg_user_sorting != USER_SORT_USER_NUMBER)
	{
		retObj.userAccountNames.sort(function(pA, pB) {
			var usernameAUpper = pA.toUpperCase();
			var usernameBUpper = pB.toUpperCase();
			if (usernameAUpper < usernameBUpper)
				return -1;
			else if (usernameAUpper == usernameBUpper)
				return 0;
			else if (usernameAUpper > usernameBUpper)
				return 1;
		});
	}

	return retObj;
}
// Builds a string containing a user's alias & name, with either their
// alias first or name first, depending on the configured sort order in
// the ettings
function getUserName(pUserAcct)
{
	// Depending on the sort option (which will be used right after this loop),
	// use either the user's real name first or the user's alias first
	var username = "";
	if (pUserAcct.alias.length > 0 && pUserAcct.name.length > 0)
	{
		if (gCfgInfo.cfgOptions.groupie_cfg_user_sorting == USER_SORT_REAL_NAME)
		{
			if (pUserAcct.name.toUpperCase() != pUserAcct.alias.toUpperCase())
				username = pUserAcct.name + " (" + pUserAcct.alias + ")";
			else
				username = pUserAcct.alias;
		}
		else
		{
			if (pUserAcct.name.toUpperCase() != pUserAcct.alias.toUpperCase())
				username = pUserAcct.alias + " (" + pUserAcct.name + ")";
			else
				username = pUserAcct.alias;
		}
	}
	else if (pUserAcct.alias.length > 0)
		username = pUserAcct.alias;
	else if (pUserAcct.name.length > 0)
		username = pUserAcct.name;

	return username;
}

// Allows configuring NNTP server settings for a user account
function editNNTPServerAcctInfoForUser(pUsername, pUserNum)
{
	// Put together an object with default server options for use with
	// loading the user's server settings
	var defaultServerOpts = {
		hostname: gCfgInfo.cfgOptions.hostname,
		port: gCfgInfo.cfgOptions.host_port,
		username: pUsername,
		password: ""
	};
	// Load the user's server configuration file: <user#>.groupie.ini from sbbs/data/user
	var userSettingsFilename = system.data_dir + "user/" + format("%04d", pUserNum) + ".groupie.ini";
	var userSettings = ReadUserSettings(userSettingsFilename, defaultServerOpts);
	if (!userSettings.successfullyRead)
	{
		// Set default values for the user's server settings
		userSettings.NNTPSettings.hostname = gCfgInfo.cfgOptions.hostname;
		userSettings.NNTPSettings.port = gCfgInfo.cfgOptions.host_port;
		try
		{
			var theUser = new User(pUserNum);
			userSettings.NNTPSettings.username = theUser.alias;
			userSettings.NNTPSettings.password = gCfgInfo.cfgOptions.default_server_password;
		}
		catch (error)
		{
			uifc.msg(format("* Failed to load user number %d: %s", pUserNum, error));
			return;
		}
	}
	if (userSettings.NNTPSettings.password.length == 0)
		userSettings.NNTPSettings.password = gCfgInfo.cfgOptions.default_server_password;

	// Menu options for the user's server settings
	var menuOpts = [
		"Hostname   " + userSettings.NNTPSettings.hostname,
		"Port       " + userSettings.NNTPSettings.port.toString(),
		"Username   " + userSettings.NNTPSettings.username,
		"Password   " + userSettings.NNTPSettings.password
	];

	// Create a CTX to specify the current selected item index
	if (editNNTPServerAcctInfoForUser.ctx == undefined)
		editNNTPServerAcctInfoForUser.ctx = uifc.list.CTX();
	// Selection
	var winMode = WIN_ORG|WIN_MID|WIN_ACT|WIN_ESC;
	var menuTitle = "Server settings for " + pUsername;
	var anyOptionChanged = false;
	var continueOn = true;
	while (continueOn && !js.terminated)
	{
		//uifc.help_text = editNNTPServerAcctInfoForUser.mainScreenHelp;
		var optionMenuSelection = uifc.list(winMode, menuTitle, menuOpts, editNNTPServerAcctInfoForUser.ctx);
		editNNTPServerAcctInfoForUser.ctx.cur = optionMenuSelection; // Remember the current selected item
		switch (optionMenuSelection)
		{
			case -1: // ESC
				continueOn = false;
				break;
			case 0: // Hostname
				var oldHostname = userSettings.NNTPSettings.hostname;
				userSettings.NNTPSettings.hostname = uifc.input(WIN_MID, "Hostname", userSettings.NNTPSettings.hostname, 0, K_EDIT);
				anyOptionChanged = anyOptionChanged || (userSettings.NNTPSettings.hostname != oldHostname);
				menuOpts[optionMenuSelection] = "Hostname   " + userSettings.NNTPSettings.hostname;
				break;
			case 1: // Port
				var oldPort = userSettings.NNTPSettings.port;
				var portStr = uifc.input(WIN_MID, "Port", userSettings.NNTPSettings.port.toString(), 0, K_NUMBER|K_EDIT);
				var port = parseInt(portStr);
				if (!isNaN(port))
				{
					userSettings.NNTPSettings.port = port;
					anyOptionChanged = anyOptionChanged || (userSettings.NNTPSettings.port != oldPort);
					menuOpts[optionMenuSelection] = "Port       " + userSettings.NNTPSettings.port.toString();
				}
				break;
			case 2: // Username
				var oldUsername = userSettings.NNTPSettings.username;
				userSettings.NNTPSettings.username = uifc.input(WIN_MID, "Hostname", userSettings.NNTPSettings.username, 0, K_EDIT);
				anyOptionChanged = anyOptionChanged || (userSettings.NNTPSettings.username != oldUsername);
				menuOpts[optionMenuSelection] = "Username   " + userSettings.NNTPSettings.username;
				break;
			case 3: // Password
				var oldPassword = userSettings.NNTPSettings.password;
				userSettings.NNTPSettings.password = uifc.input(WIN_MID, "Password", userSettings.NNTPSettings.password, 0, K_EDIT);
				anyOptionChanged = anyOptionChanged || (userSettings.NNTPSettings.password != oldPassword);
				menuOpts[optionMenuSelection] = "Password   " + userSettings.NNTPSettings.password;
				break;
		}
	}
	// If any options were changed, then save the user server configuration file
	if (anyOptionChanged)
	{
		var saveSucceed = SaveUserSettings(userSettingsFilename, userSettings.NNTPSettings);
		if (saveSucceed)
			uifc.msg("Successfully saved settings for " + pUsername);
		else
			uifc.msg("* Failed to save settings for " + pUsername + "!");
	}
}

// Allows changing NNTP server settings for existing NNTP accounts
function bulkEditExistingNNTPAccountInformation()
{
	// Get an array of <user#>.groupie.ini files from the sbbs/data/user directory,
	// as well as username & user #s for the users for those filenames
	var userInfo = getServerSettingFilenamesAndUserInfo();

	var cfgOptProps = [
		"hostname",                // String
		"host_port"                // Numeric
		//"default_server_password"  // String
	];
	var optionStrs = [
		"NNTP server hostname/IP",
		"NNTP server port"
		//"Default server password for users"
	];
	// For menu item text formatting
	var itemTextMaxLen = 40;
	// Build an array of formatted string to be displayed on the menu
	// (the value formatting will depend on the variable type)
	var menuItems = [];
	for (var i = 0; i < cfgOptProps.length; ++i)
	{
		var propName = cfgOptProps[i];
		menuItems.push(formatCfgMenuText(itemTextMaxLen, cfgOptProps[i], optionStrs[i], gCfgInfo.cfgOptions[propName]));
	}

	// Create a CTX to specify the current selected item index
	if (bulkEditExistingNNTPAccountInformation.ctx == undefined)
		bulkEditExistingNNTPAccountInformation.ctx = uifc.list.CTX();
	// Selection
	var winMode = WIN_ORG|WIN_MID|WIN_ACT|WIN_ESC;
	var menuTitle = format("Bulk account edit (%d files)", userInfo.serverSettingFilenames.length);
	var chosenHostname = gCfgInfo.cfgOptions.hostname;
	var chosenPort = gCfgInfo.cfgOptions.host_port;
	//var chosenPassword = gCfgInfo.cfgOptions.default_server_password;
	var continueOn = true;
	while (continueOn && !js.terminated)
	{
		//uifc.help_text = bulkEditExistingNNTPAccountInformation.mainScreenHelp;
		var optionMenuSelection = uifc.list(winMode, menuTitle, menuItems, bulkEditExistingNNTPAccountInformation.ctx);
		bulkEditExistingNNTPAccountInformation.ctx.cur = optionMenuSelection; // Remember the current selected item
		switch (optionMenuSelection)
		{
			case -1: // ESC
				continueOn = false;
				break;
			case 0: // Hostname
				var propName = cfgOptProps[optionMenuSelection];
				chosenHostname = uifc.input(WIN_MID, optionStrs[optionMenuSelection], gCfgInfo.cfgOptions[propName], 0, K_EDIT);
				menuItems[optionMenuSelection] = formatCfgMenuText(itemTextMaxLen, cfgOptProps[optionMenuSelection], optionStrs[optionMenuSelection], chosenHostname);
				break;
			case 1: // Port
				var propName = cfgOptProps[optionMenuSelection];
				var valStr = uifc.input(WIN_MID, optionStrs[optionMenuSelection], gCfgInfo.cfgOptions[propName].toString(), 0, K_NUMBER|K_EDIT);
				var valNum = parseInt(valStr);
				if (!isNaN(valNum))
				{
					chosenPort = valNum;
					menuItems[optionMenuSelection] = formatCfgMenuText(itemTextMaxLen, cfgOptProps[optionMenuSelection], optionStrs[optionMenuSelection], chosenPort.toString());
				}
				break;
			/*
			case 2: // Password
				var propName = cfgOptProps[optionMenuSelection];
				chosenPassword = uifc.input(WIN_MID, optionStrs[optionMenuSelection], gCfgInfo.cfgOptions[propName], 0, K_EDIT);
				menuItems[optionMenuSelection] = formatCfgMenuText(itemTextMaxLen, cfgOptProps[optionMenuSelection], optionStrs[optionMenuSelection], chosenPassword);
				break;
			*/
		}
	}

	// Prompt the user whether to apply/save settings, and if so, do it
	if (promptYesNo("Save configurations?", false, WIN_ORG|WIN_MID|WIN_ACT|WIN_ESC))
	{
		var usernamesForFailedSave = [];
		for (var i = 0; i < userInfo.serverSettingFilenames.length; ++i)
		{
			var userSettings = ReadUserSettings(userInfo.serverSettingFilenames[i], {});
			if (userSettings.successfullyRead)
			{
				userSettings.NNTPSettings.hostname = chosenHostname;
				userSettings.NNTPSettings.port = chosenPort;
				//userSettings.NNTPSettings.password = chosenPassword;
				//userSettings.NNTPSettings.username
				if (!SaveUserSettings(userInfo.serverSettingFilenames[i], userSettings.NNTPSettings))
					usernamesForFailedSave.push(userInfo.userAccountNames[i]);
			}
			else
				usernamesForFailedSave.push(userInfo.userAccountNames[i]);
		}

		// Show a success/fail message
		if (usernamesForFailedSave.length > 0)
		{
			winMode = WIN_ORG|WIN_MID|WIN_ACT|WIN_ESC;
			uifc.list(winMode, "Save failed for these users", usernamesForFailedSave);
		}
		else
			uifc.msg("All succeeded");
	}
}
// Helper for bulkEditExistingNNTPAccountInformation(): Gets an array of existing
// server setting filenames for users, and theur usernames & user numbers,
// possibly sorting the user names.
function getServerSettingFilenamesAndUserInfo()
{
	var retObj = {
		userAccountNames: [],
		serverSettingFilenames: [], // Same number and order as userAccountNames
		userAccountNumsByUsername: {},
		failedUserNums: []
	};

	var serverSettingFilenames = directory(system.data_dir + "user/*.groupie.ini");
	for (var i = 0; i < serverSettingFilenames.length; ++i)
	{
		var justFilename = file_getname(serverSettingFilenames[i]);
		var dotIdx = justFilename.indexOf(".");
		if (dotIdx == -1)
			continue;
		var userNum = parseInt(justFilename.substring(0, dotIdx));
		if (isNaN(userNum))
			continue;

		try
		{
			var theUser = new User(userNum);
			var username = getUserName(theUser);
			retObj.userAccountNames.push(username);
			retObj.serverSettingFilenames.push(serverSettingFilenames[i]);
			retObj.userAccountNumsByUsername[username] = userNum;
		}
		catch (error)
		{
			retObj.failedUserNums.push(userNum);
			log(LOG_ERR, format("Groupie config - Error loading user number %d: %s", userNum, error));
		}
	}

	// Sort the user list as configured, if applicable.
	// The names will be added as either the user's real name followed by
	// alias or alias followed by real name, depending on the sort option.
	// So if the sort option isn't user number (no specific sorting), we
	// just need to sort them.
	if (gCfgInfo.cfgOptions.groupie_cfg_user_sorting != USER_SORT_USER_NUMBER)
	{
		retObj.userAccountNames.sort(function(pA, pB) {
			var usernameAUpper = pA.toUpperCase();
			var usernameBUpper = pB.toUpperCase();
			if (usernameAUpper < usernameBUpper)
				return -1;
			else if (usernameAUpper == usernameBUpper)
				return 0;
			else if (usernameAUpper > usernameBUpper)
				return 1;
		});
	}

	return retObj;
}


// Lets the user (sysop) configure the username sorting for groupie_cfg
function chooseGroupieCfgUsernameSortOpt(pCurrentValNum)
{
	var retObj = {
		userAborted: false,
		usernameSortOpt: USER_SORT_USER_NUMBER
	}

	// Menu options for the username sorting options
	var menuOpts = [];
	var userNumSortOptIdx = menuOpts.push(userSortOptValToStr(USER_SORT_USER_NUMBER)) - 1;
	var aliasSortOptIdx = menuOpts.push(userSortOptValToStr(USER_SORT_ALIAS)) - 1;
	var realNameSortOptIdx = menuOpts.push(userSortOptValToStr(USER_SORT_REAL_NAME)) - 1;

	// Create a CTX to specify the current selected item index
	if (chooseGroupieCfgUsernameSortOpt.ctx == undefined)
		chooseGroupieCfgUsernameSortOpt.ctx = uifc.list.CTX();
	// Set the currently selected value based on pCurrentValNum
	switch (pCurrentValNum)
	{
		case USER_SORT_USER_NUMBER:
			chooseGroupieCfgUsernameSortOpt.ctx.cur = userNumSortOptIdx;
			break;
		case USER_SORT_ALIAS:
			chooseGroupieCfgUsernameSortOpt.ctx.cur = aliasSortOptIdx;
			break;
		case USER_SORT_REAL_NAME:
			chooseGroupieCfgUsernameSortOpt.ctx.cur = realNameSortOptIdx;
			break;
	}

	// Selection
	var winMode = WIN_ORG|WIN_MID|WIN_ACT|WIN_ESC;
	var menuTitle = "Username sorting for groupie_cfg";
	var anyOptionChanged = false;
	var continueOn = true;
	//while (continueOn && !js.terminated)
	{
		//uifc.help_text = chooseGroupieCfgUsernameSortOpt.mainScreenHelp;
		var optionMenuSelection = uifc.list(winMode, menuTitle, menuOpts, chooseGroupieCfgUsernameSortOpt.ctx);
		chooseGroupieCfgUsernameSortOpt.ctx.cur = optionMenuSelection; // Remember the current selected item
		switch (optionMenuSelection)
		{
			case -1: // ESC
				//continueOn = false;
				retObj.userAborted = true;
				break;
			case userNumSortOptIdx: // User number
				//usernameSortOpt = USER_SORT_USER_NUMBER;
				retObj.usernameSortOpt = USER_SORT_USER_NUMBER;
				break;
			case aliasSortOptIdx: // Alias
				//usernameSortOpt = USER_SORT_ALIAS;
				retObj.usernameSortOpt = USER_SORT_ALIAS;
				break;
			case realNameSortOptIdx: // Real name
				//usernameSortOpt = USER_SORT_REAL_NAME;
				retObj.usernameSortOpt = USER_SORT_REAL_NAME;
				break;
		}
	}

	//return usernameSortOpt;
	return retObj;
}


///////////////////////////////////////////////////
// Help text functions


// Returns a dictionary of help text, indexed by the option name from the configuration file
function getOptionHelpText()
{
	var optionHelpText = {};

	optionHelpText["groupie_cfg_user_sorting"] = "Username sorting for groupie_cfg: Username sorting for ";
	optionHelpText["groupie_cfg_user_sorting"] += "groupie_cfg (for instance, when configuring NNTP server ";
	optionHelpText["groupie_cfg_user_sorting"] += "credentials for users)";

	optionHelpText["use_lightbar_interface"] = "Use Lightbar/Scrolling Interface: Either Lightbar or Traditional";

	optionHelpText["users_can_change_their_nttp_settings"] = "Users can change their NNTP settings: Whether or not ";
	optionHelpText["users_can_change_their_nttp_settings"] += "users can change their NNTP settings";

	optionHelpText["receive_bufer_size_bytes"] = "Receive buffer size (bytes): The receive buffer size (in bytes) for use ";
	optionHelpText["receive_bufer_size_bytes"] += "with the news server";

	optionHelpText["receive_timeout_seconds"] = "Receive timeout (in seconds): The receive timeout (in seconds) for use ";
	optionHelpText["receive_timeout_seconds"] += "with the news server";

	optionHelpText["hostname"] = "NNTP server hostname/IP: The (default) hostname/IP address of the NNTP server";

	optionHelpText["host_port"] = "NNTP server port: The (default) port to use for the NNTP server";

	optionHelpText["default_server_password"] = "Default server password for users: The default server password ";
	optionHelpText["default_server_password"] += "to use for users' credentials";

	//optionHelpText["prepend_foward_msg_subject"] = "Prepend forwarded msgs w/ 'fwd': Whether or not to prepend ";
	//optionHelpText["prepend_foward_msg_subject"] += "subjects of forwarded messages with 'Fwd:'";

	optionHelpText["default_max_num_msgs_from_newsgroup"] = "Default max # articles from newsgroup: The default maximum ";
	optionHelpText["default_max_num_msgs_from_newsgroup"] += "number of articles to receive for a newsgroup (this is the ";
	optionHelpText["default_max_num_msgs_from_newsgroup"] += "default for a user setting)";

	optionHelpText["msg_save_dir"] = "Message/article save dir (server PC): The directory on the server PC where messages ";
	optionHelpText["msg_save_dir"] += "should be saved (sysop option";

	optionHelpText["newsgroup_list_hdr_filename_base"] = "Newsgroup list hdr filename base: Base filename (without .ans/.asc ";
	optionHelpText["newsgroup_list_hdr_filename_base"] += "extension) for an .ans/.asc to be displayed above the newsgroup list";

	optionHelpText["newsgroup_list_hdr_max_num_lines"] = "Newsgroup list hdr max # lines: The maximum number of lines from the ";
	optionHelpText["newsgroup_list_hdr_max_num_lines"] += "newsgroup list header file to use";

	optionHelpText["article_list_hdr_filename_base"] = "Article list hdr filename base: Base filename (without .ans/.asc ";
	optionHelpText["article_list_hdr_filename_base"] += "extension) for an .ans/.asc to be displayed above the newsgroup list";

	optionHelpText["article_list_hdr_max_num_lines"] = "Article list hdr max # lines: The maximum number of lines from the ";
	optionHelpText["article_list_hdr_max_num_lines"] += "article list header file to use";

	optionHelpText["theme_config_filename"] = "Theme configuration filename: The name of the theme configuration file to use";

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
	var helpText = "This screen allows you to configure behavior options for Groupie.\r\n\r\n";
	helpText += "User account configuration: Lets you configure NNTP accounts for your users.\r\n\r\n";
	for (var i = 0; i < pCfgOptProps.length; ++i)
	{
		var optName = pCfgOptProps[i];
		helpText += pOptionHelpText[optName] + "\r\n";
	}
	return word_wrap(helpText, gHelpWrapWidth);
}


///////////////////////////////////////////////////
// Non-UI utility functions

// Reads the Groupie configuration file
//
// Parameters:
//  pCfgFileName: The name of the configuration file (i.e., groupie.ini).
//                This will also use a ".example" configuration file if
//                it exists and the given configuration filename doesn't
//                exist
//
// Return value: An object with the following properties:
//               cfgFilename: The full path & filename of the Groupie configuration file that was read
//               cfgOptions: A dictionary of the configuration options and their values
function readGroupieCfgFile(pCfgFileName)
{
	var retObj = {
		cfgFilename: "",
		cfgOptions: {}
	};

	// Determine the location of the configuration file.  First look for it
	// in the sbbs/mods directory, then sbbs/ctrl, then in the same directory
	// as this script.
	var cfgFilename = file_cfgname(system.mods_dir, pCfgFileName);
	if (!file_exists(cfgFilename))
		cfgFilename = file_cfgname(system.ctrl_dir, pCfgFileName);
	if (!file_exists(cfgFilename))
		cfgFilename = file_cfgname(js.exec_dir, pCfgFileName);
	// If the configuration file hasn't been found, look to see if there's a .example file
	// available in the same directory 
	if (!file_exists(cfgFilename))
	{
		var defaultExampleCfgFilename = pCfgFileName;
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

	retObj.cfgFilename = cfgFilename;

	// Open and read the configuration file
	var cfgFile = new File(cfgFilename);
	if (cfgFile.open("r"))
	{
		retObj.cfgOptions = cfgFile.iniGetObject();
		cfgFile.close();
	}

	// In case some settings weren't loaded, add defaults
	if (typeof(retObj.cfgOptions.use_lightbar_interface) !== "boolean")
		retObj.cfgOptions.use_lightbar_interface = true;
	if (typeof(retObj.cfgOptions.users_can_change_their_nttp_setting) !== "boolean")
		retObj.cfgOptions.users_can_change_their_nttp_settings = false;
	if (typeof(retObj.cfgOptions.receive_bufer_size_bytes) !== "number")
		retObj.cfgOptions.receive_bufer_size_bytes = 1024;
	if (typeof(retObj.cfgOptions.receive_timeout_seconds) !== "number")
		retObj.cfgOptions.receive_timeout_seconds = 30;
	if (typeof(retObj.cfgOptions.hostname) !== "string")
		retObj.cfgOptions.hostname = "127.0.0.1";
	if (typeof(retObj.cfgOptions.host_port) !== "number")
		retObj.cfgOptions.host_port = 119;
	if (typeof(retObj.cfgOptions.default_server_password) !== "string")
		retObj.cfgOptions.default_server_password = "";
	if (typeof(retObj.cfgOptions.prepend_foward_msg_subject) !== "boolean")
		retObj.cfgOptions.prepend_foward_msg_subject = true;
	if (typeof(retObj.cfgOptions.default_max_num_msgs_from_newsgroup) !== "number")
		retObj.cfgOptions.default_max_num_msgs_from_newsgroup = 500;
	if (typeof(retObj.cfgOptions.msg_save_dir) !== "string")
		retObj.cfgOptions.msg_save_dir = "";
	if (typeof(retObj.cfgOptions.newsgroup_list_hdr_filename_base) !== "string")
		retObj.cfgOptions.newsgroup_list_hdr_filename_base = "newsgroups_Elite";
	if (typeof(retObj.cfgOptions.newsgroup_list_hdr_max_num_lines) !== "number")
		retObj.cfgOptions.newsgroup_list_hdr_max_num_lines = 8;
	if (typeof(retObj.cfgOptions.article_list_hdr_filename_base) !== "string")
		retObj.cfgOptions.article_list_hdr_filename_base = "articles_Elite";
	if (typeof(retObj.cfgOptions.article_list_hdr_max_num_lines) !== "number")
		retObj.cfgOptions.article_list_hdr_max_num_lines = 8;
	if (typeof(retObj.cfgOptions.theme_config_filename) !== "string")
		retObj.cfgOptions.theme_config_filename = "default_theme.ini";
	// groupie_cfg-specific stuff:
	if (typeof(retObj.cfgOptions.groupie_cfg_user_sorting) === "string")
		retObj.cfgOptions.groupie_cfg_user_sorting = userSortOptStrToVal(retObj.cfgOptions.groupie_cfg_user_sorting);
	else
		retObj.cfgOptions.groupie_cfg_user_sorting = USER_SORT_USER_NUMBER;


	return retObj;
}

// Converts a string to a user sort option for groupie_cfg
function userSortOptStrToVal(pSortOptStr)
{
	var sortOptStrUpper = pSortOptStr.toUpperCase();
	if (sortOptStrUpper == "ALIAS" || sortOptStrUpper == "USERNAME")
		return USER_SORT_ALIAS;
	else if (sortOptStrUpper == "REALNAME")
		return USER_SORT_REAL_NAME;
	else
		return USER_SORT_USER_NUMBER;
}

// Converts a username sorting value (for groupie_cfg) to a string
function userSortOptValToStr(pUsernameSortOpt)
{
	var optStr = "";
	switch (pUsernameSortOpt)
	{
		case USER_SORT_ALIAS:
			optStr = "Alias";
			break;
		case USER_SORT_REAL_NAME:
			optStr = "Real name";
			break;
		case USER_SORT_USER_NUMBER:
		default:
			optStr = "User number";
			break;
	}
	return optStr;
}

// Saves the Groupie configuration file using the settings in gCfgInfo
//
// Return value: An object with the following properties:
//               saveSucceeded: Boolean - Whether or not the save fully succeeded
//               savedToModsDir: Boolean - Whether or not the .cfg file was saved to the mods directory
function saveGroupieCfgFile()
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
		// See if the configuration file has ".example" in it. If so,
		// then save it in the same directory without the ".example"
		var saveInSameDir = false;
		var exampleIdx = gCfgInfo.cfgFilename.lastIndexOf(".example");
		if (exampleIdx > -1)
		{
			var originalCfgFilename = cfgFilename;
			var filenamePart1 = gCfgInfo.cfgFilename.substring(0, exampleIdx);
			var filenamePart2 = gCfgInfo.cfgFilename.substring(exampleIdx+8);
			cfgFilename = filenamePart1 + filenamePart2;
			if (!file_copy(originalCfgFilename, cfgFilename))
				return retObj;
		}
		/*
		else
		{
			// Copy to sbbs/mods
			cfgFilename = system.mods_dir + file_getname(gCfgInfo.cfgFilename);
			if (!file_copy(gCfgInfo.cfgFilename, cfgFilename))
				return retObj;
			retObj.savedToModsDir = true;
		}
		*/
	}

	var cfgFile = new File(cfgFilename);
	var openMode = (file_exists(cfgFilename) ? "r+" : "w");
	if (cfgFile.open(openMode))
	{
		for (var settingName in gCfgInfo.cfgOptions)
		{
			if (settingName == "groupie_cfg_user_sorting")
			{
				var settingVal = "";
				switch (gCfgInfo.cfgOptions[settingName])
				{
					case USER_SORT_USER_NUMBER:
						settingVal = "UserNumber";
						break;
					case USER_SORT_ALIAS:
						settingVal = "Alias";
						break;
					case USER_SORT_REAL_NAME:
						settingVal = "RealName";
						break;
				}
				cfgFile.iniSetValue(null, settingName, settingVal);
			}
			else
				cfgFile.iniSetValue(null, settingName, gCfgInfo.cfgOptions[settingName]);
		}
		cfgFile.close();
		retObj.saveSucceeded = true;
		retObj.savedToModsDir = (cfgFilename.indexOf(system.mods_dir) > -1);
	}

	return retObj;
}

// Reads the user's server settings
function ReadUserSettings(pUserSettingsFilename, pDefaultNNTPConnectOptions)
{
	var retObj = {
		successfullyRead: false,
		NNTPSettings: new NNTPConnectOptions()
	};

	// Copy the default NNTP server credentials to the user credentials object
	for (var prop in pDefaultNNTPConnectOptions)
		retObj.NNTPSettings[prop] = pDefaultNNTPConnectOptions[prop];

	// Read the user settings file, if possible
	var settingsFile = new File(pUserSettingsFilename);
	if (settingsFile.open("r"))
	{
		var userSettings = settingsFile.iniGetObject("NNTPSettings");
		settingsFile.close();
		retObj.successfullyRead = true;
		for (var prop in retObj.NNTPSettings)
		{
			if (userSettings.hasOwnProperty(prop))
			{
				// For the user's password loaded from the file,
				// base64-decode it. Otherwise, just read the
				// setting as-is.
				if (prop == "password")
					retObj.NNTPSettings[prop] = base64_decode(userSettings[prop]);
				else
					retObj.NNTPSettings[prop] = userSettings[prop];
			}
		}
	}

	return retObj;
}

// Saves the user's server settings
function SaveUserSettings(pUserSettingsFilename, pUserNNTPSettings)
{
	var succeeded = false;
	var settingsFile = new File(pUserSettingsFilename);
	if (settingsFile.open(settingsFile.exists ? "r+" : "w+"))
	{
		succeeded = true;
		for (var prop in pUserNNTPSettings)
		{
			var value = pUserNNTPSettings[prop];
			// Base64-encode the user's password so that it isn't just
			// stored as plaintext
			if (prop == "password")
				value = base64_encode(pUserNNTPSettings[prop]);
			if (!settingsFile.iniSetValue("NNTPSettings", prop, value))
				succeeded = false;
		}
		//settingsFile.flush();
		settingsFile.close();
	}
	return succeeded;
}
