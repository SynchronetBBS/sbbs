/* This is a news reader door door for Synchronet which connects to an external
 * news (NNTP) server.  This assumes the news server has an account with credentials
 * matching the BBS user's username & password, though the credentials to use
 * with the news server can be overridden in the settings file.
 *
 *
 * Date       Author            Description
 * 2024-09-13 Eric Oulashin     Started working on this
 * 2025-09-19 Eric Oulashin     Version 0.90 Beta
 *                              Progress made
 * 2025-09-25 Eric Oulashin     Version 1.00
 *                              Releasing this version
 * 2025-09-27 Eric Oulashin     Version 1.01
 *                              New configuration option for toggling whether
 *                              users can change their NNTP settings.
 *                              New configurator that can be run with jsexec
 *                              which can also edit users' NNTP settings.
 * 2025-09-29 Eric Oulashin     Version 1.02
 *                              New configuration option, append_bbs_domain_to_usernames,
 *                              which specifies whether to append the BBS's domain
 *                              (as @domain) to usernames when authenticating with
 *                              the news server
 * 2025-09-30 Eric Oulashin     Version 1.03
 *                              When getting the list of newsgroups, if using the
 *                              newsgroup cache file for a server, then also request
 *                              a list of new newsgroups since the cached file's
 *                              timestamp
 * 2025-09-30 Eric Oulashin     Version 1.04
 *                              Mechanism to subscribe to newsgroups
 */

"use strict";

// Requires Synchronet version 3.20 or higher (.ini configuration files, updated console.editfile(), and
// bbs.expand_atcodes())
if (system.version_num < 32000)
{
	var message = "\x01n\x01h\x01y\x01i* Warning: \x01n\x01h\x01wGroupie "
	            + "requires version \x01g3.20\x01w or\r\n"
	            + "higher of Synchronet.  This BBS is using version \x01g" + system.version
	            + "\x01w.  Please notify the sysop.";
	console.crlf();
	console.print(message);
	console.crlf();
	console.pause();
	exit(0);
}


// Note: key_defs.js is loaded by sbbsdefs.js
require("sbbsdefs.js", "K_NOCRLF");
require("nntp_client_lib.js", "NNTPClient");
require("key_defs.js", "CTRL_H");
require("cp437_defs.js", "CP437_BOX_DRAWINGS_UPPER_LEFT_SINGLE");
require("choice_scroll_box.js", "ChoiceScrollbox");
require("dd_lightbar_menu.js", "DDLightbarMenu");
require("frame.js", "Frame");
require("scrollbar.js", "ScrollBar");
require("graphic.js", 'Graphic');
require("text.js", "Aborted");
var ansiterm = require("ansiterm_lib.js", 'expand_ctrl_a');
var hexdump = load('hexdump_lib.js');


// Program and version information
var PROGRAM_NAME = "Groupie";
var PROGRAM_VERSION = "1.04";
var PROGRAM_DATE = "2025-10-22";


///////////////////////////////////

// Key codes
var BACKSPACE = CTRL_H;
// Keyboard key codes for displaying on the screen
var UP_ARROW = ascii(24);
var DOWN_ARROW = ascii(25);
var LEFT_ARROW = ascii(17);
var RIGHT_ARROW = ascii(16);

// Action codes for this reader
const ACTION_NONE = 0;
const ACTION_GO_TO_FIRST_MSG = 1;
const ACTION_GO_TO_LAST_MSG = 2;
const ACTION_GO_TO_PREVIOUS_MSG = 3;
const ACTION_GO_TO_NEXT_MSG = 4;
const ACTION_SHOW_MSG_LIST = 5;
const ACTION_QUIT_READER = 6;

// Program states for this reader
const STATE_CHOOSING_NEWSGROUP = 0;
const STATE_LISTING_ARTICLES_IN_NEWSGROUP = 1;
const STATE_POSTING = 2;
const STATE_LISTING_READING_ARTICLE = 3;
const STATE_EXIT = 4;

// Timeout (in milliseconds) when waiting for search input
var SEARCH_TIMEOUT_MS = 10000;
const ERROR_PAUSE_WAIT_MS = 1500;


///////////////////////////////////

// When exiting, make sure we don't leave a QUOTES.TXT behind in the node directory
js.on_exit("file_remove(\"" + system.node_dir + "QUOTES.TXT" + "\");");

// Update the ctrl key passthru to support what this reader needs (only while this reader is running)
js.on_exit("console.ctrlkey_passthru = " + console.ctrlkey_passthru);
console.ctrlkey_passthru = "U";


console.line_counter = 0; // To prevent a pause before the message list comes up
console.clear("N");
console.center(ColorFirstCharAndRemainingCharsInWords(PROGRAM_NAME, "\x01n\x01c\x01h", "\x01n\x01c"));
console.attributes = "N";
console.crlf();
console.crlf();


// User number padded to 4 digits with leading 0s - For user data files
const gPaddedUserNum = format("%04d", user.number);

// Read the settings. groupie.ini is the default configuration filename;
// there could be a groupie.example.ini or perhaps a groupie.ini in the
// same directory or in sbbs/mods etc..
var gSettings = ReadSettings("groupie.ini");
// Read the user's usage information
const gUserUsageInfoFilename = system.data_dir + "user/" + gPaddedUserNum + ".groupie_usage.json";
var gUserUsageInfo = ReadUserUsageInfo(gUserUsageInfoFilename);

// Look for the header filenames as configured
var hdrSubdirName = "list_headers";
var newsgrpHdrFilename = findANSOrASCVersionOfFile(gSettings.newsgroupListHdrFilenameBase, hdrSubdirName);
var articleListHdrFilename = findANSOrASCVersionOfFile(gSettings.articleListHdrFilenameBase, hdrSubdirName);
// Read the header files into arrays
var gNewsgroupListHdrLines = loadAnsOrAscFileIntoArray(newsgrpHdrFilename, gSettings.newsgroupListHdrMaxNumLines);
var gArticleListHdrLines = loadAnsOrAscFileIntoArray(articleListHdrFilename, gSettings.articleListHdrMaxNumLines);

// User settings filename (and store whether the user's settings file existed; if not, this is the
// first time the user has run this reader)
var gUserSettingsFilename = system.data_dir + "user/" + gPaddedUserNum + ".groupie.ini";
var gUserSettingsFileExistedOnStart = file_exists(gUserSettingsFilename);
// Read the user's settings.  If the user settings file didn't exist, then prompt the user for settings.
var gUserSettings = ReadUserSettings(gUserSettingsFilename, gSettings.server);
// If configured to append the BBS's domain to the user's username,
// then append it as @domain
if (gSettings.appendBBSDomainToUsernames)
	gUserSettings.NNTPSettings.username = gUserSettings.NNTPSettings.username + "@" + system.inet_addr;
// If the user's settings couldn't be successfully read, then save the user's
// default server settings and try to read them again.
if (!gUserSettings.successfullyRead)
{
	if (SaveUserSettings(gUserSettingsFilename, true))
		gUserSettings = ReadUserSettings(gUserSettingsFilename, gSettings.server);
}
/*
// If not successfully read, then if we can let the user configure their server settings,
// do so.
if (!gUserSettings.successfullyRead)
{
	if (gSettings.usersCanChangeTheirNNTPSettings)
	{
		console.print("Settings file does not exist.  Configuration settings are needed.\r\n");
		LetUserUpdateSettings(gUserSettingsFilename);
		console.crlf();
	}
	else
	{
		var errorMsg = "You don't have any server settings. Please ask your sysop to configure that for you.";
		console.print(lfexpand(word_wrap(errorMsg, console.screen_columns-1, false)));
		console.crlf();
		console.pause();
		exit(0);
	}
}
*/

// TODO
//gUserSettings.subscribedNewsgroups

// Create the news reader client object and connect.
// This is in a loop in case connection/authentication fails and the user has
// to update their settings/credentials.
var nntpClient;
var continueConnectLoop = true;
while (continueConnectLoop)
{
	printf("Connecting to the server (%s)...\r\n", gUserSettings.NNTPSettings.hostname);
	nntpClient = new NNTPClient(gUserSettings.NNTPSettings, 0, gSettings.recvBufSizeBytes, gSettings.recvTimeoutSeconds);
	nntpClient.lowercaseHeaderFieldNames = true;
	nntpClient.addHeaderToIfMissing = true;
	var connectRetObj = null;
	var connectContinueOn = true;
	while (connectContinueOn)
	{
		connectRetObj = nntpClient.Connect();
		if (connectRetObj.connected)
		{
			//console.print(connectRetObj.connectStr + "\r\n");
			console.print("Connected to the server.\r\n");
			connectContinueOn = false;
			break;
		}
		else
		{
			nntpClient.Disconnect();
			printf("!Error %d connecting to %s port %d\r\n", nntpClient.GetLastError(), nntpClient.connectOptions.hostname, nntpClient.connectOptions.port);
			//exit(1);
			// Let the user change their server configuration, if configured to do so
			if (gSettings.usersCanChangeTheirNNTPSettings && console.yesno("Change configuration settings"))
				LetUserUpdateSettings(gUserSettingsFilename);
			else
			{
				console.print("Please ask the sysop to change your server settings.\r\n\x01p");
				connectContinueOn = false;
				break;
			}
		}
	}
	if (typeof(connectRetObj) !== "object" || !connectRetObj.connected)
	{
		console.print("\x01n\x01w\x01hFailed to connect.\x01n\r\n");
		if (!gSettings.usersCanChangeTheirNNTPSettings)
			console.print("Please ask the sysop to change your server settings.\r\n");
		console.crlf();
		exit(1);
	}


	

	if (nntpClient.Authenticate())
	{
		continueConnectLoop = false;
		console.print("Authentication was successful.\r\n");
	}
	else
	{
		nntpClient.Disconnect();
		console.print("Authentication failed!\r\n");
		// Let the user change their server configuration
		if (gSettings.usersCanChangeTheirNNTPSettings && console.yesno("Change configuration settings"))
		{
			LetUserUpdateSettings(gUserSettingsFilename);
			console.crlf();
		}
		else
		{
			if (!gSettings.usersCanChangeTheirNNTPSettings)
				console.print("Please ask the sysop to change your server settings.\r\n");
			nntpClient.Disconnect();
			continueConnectLoop = false;
			exit(2);
		}
	}
}


// If the user's settings file didn't exist on startup, then this is the first
// time the user has run this reader, so show an initial welcome/help screen.
if (!gUserSettingsFileExistedOnStart)
{
	console.line_counter = 0;
	console.clear("N");
	DisplayFirstTimeUsageHelp();
}

// Get the list of newsgroups & create the newsgroup menu
// Filename for the newsgroup list cache for the server
//var gNewsgroupListFilename = js.exec_dir + gSettings.server.hostname + ".grpListCache";
var gNewsgroupListFilename = js.exec_dir + gUserSettings.NNTPSettings.hostname + ".grpListCache";
// If the user has subscribed to some newsgroups, ask if they want to see only
// their subscribed newsgroups or all newsgroups
var gShowOnlySubscribedNewsgroups = false;
if (gUserSettings.subscribedNewsgroups.length > 0)
{
	var promptTextColor = "\x01n\x01c";
	//var promptParenColor = "\x01g\x01h";
	//var promptInsideParenTextColor = "\x01c"; // Will have high from previous color
	//var promptInsideParenEqualsColor = "\x01y"; // Will have high from previous color
	var promptInsideParenTextColor = "\x01n\x01c"
	var promptParenColor = "\x01g\x01h";
	var promptInsideParenEqualsColor = "\x01h\x01y";
	var promptText = format("%sShow only subscribed groups %s(%sno%s=%sshow all%s)%s", promptTextColor, promptParenColor, promptInsideParenTextColor,
	                         promptInsideParenEqualsColor, promptInsideParenTextColor, promptParenColor, promptTextColor);
	gShowOnlySubscribedNewsgroups = console.yesno(promptText);
}
else
	console.print("Getting list of newsgroups...\r\n");
// Create the newsgroup menu, with the newsgroups
var gCreateNewsgrpMenuRetObj = CreateNewsgroupMenu_ExitOnError(nntpClient, gUserSettings.subscribedNewsgroups, gShowOnlySubscribedNewsgroups);


// The main program logic is here:

// This is a loop that basically uses state machine logic to control what to do
// next.
// Start in a 'choosing newsgroup' state, and depending on the user's chosen
// action, change state accordingly until the user chooses to exit the reader.
// The various functions called here return a state value for what should be
// done next. The state for 'listing articles in newsgroup' also includes
// potentially reading messages in the newsgroup, posting, and replying.
var gCurrentState = STATE_CHOOSING_NEWSGROUP;
var gCurrentActionRetObj = null;
var gChosenNewsgroupName = "";
var gSelectNGRetObj = null;
var gSelectedArticleHdr = null;
var gSelectedArticleBody = null;
while (gCurrentState != STATE_EXIT)
{
	switch (gCurrentState)
	{
		case STATE_CHOOSING_NEWSGROUP:
			gChosenNewsgroupName = "";
			gSelectNGRetObj = null;
			gSelectedArticleHdr = null;
			gSelectedArticleBody = null;

			// Let the user start by choosing a newsgroup. From there, the user can choose an
			// article and then read it.
			gCurrentActionRetObj = ChooseNewsgroup(gCreateNewsgrpMenuRetObj.newsgrpMenu, gCreateNewsgrpMenuRetObj.newsgroupItemFormatStr, gCreateNewsgrpMenuRetObj.hdrFormatStr, gShowOnlySubscribedNewsgroups);
			gCurrentState = gCurrentActionRetObj.nextState;
			gChosenNewsgroupName = gCurrentActionRetObj.chosenNewsgroup;
			gSelectNGRetObj = gCurrentActionRetObj.selectNGRetObj;
			break;
		case STATE_LISTING_ARTICLES_IN_NEWSGROUP:
			gSelectedArticleHdr = null;
			gSelectedArticleBody = null;

			if (gChosenNewsgroupName != "" && gSelectNGRetObj != null)
			{
				gCurrentActionRetObj = ListArticlesInNewsgroup(nntpClient, gChosenNewsgroupName, gSelectNGRetObj);
				if (typeof(gCurrentActionRetObj) === "object" && gCurrentActionRetObj.hasOwnProperty("nextState"))
					gCurrentState = gCurrentActionRetObj.nextState;
				else
					gCurrentState = STATE_CHOOSING_NEWSGROUP;
			}
			else
				gCurrentState = STATE_CHOOSING_NEWSGROUP;
			break;
		// With the current logic, these states are normally handled within
		// listing articles in a newsgroup, but just in case, if these are seen
		// here, go on to choosing a newsgroup.
		case STATE_POSTING:
		case STATE_LISTING_READING_ARTICLE:
			gCurrentState = STATE_CHOOSING_NEWSGROUP;
			gChosenNewsgroupName = "";
			gSelectNGRetObj = null;
			gCurrentActionRetObj = null;
			break;
	}
}

// When we reach this point, the user has chosen to exit.

// Disconnect from the NNTP server
nntpClient.Disconnect();

// Save the user usage information to the JSON file
SaveUserNewsgroupUsageInfoToFile();




/////////////////////////////////////////
// Functions

function ReadSettings(pDefaultCfgFilename)
{
	var settings = {
		server: new NNTPConnectOptions(),
		appendBBSDomainToUsernames: false,
		serverPort: 119, // Default server port
		defaultPasswordForUsers: "",
		recvBufSizeBytes: 1024, // Default receive size
		recvTimeoutSeconds: 30, // Default receive timeout
		usersHaveSeparateAccount: false,
		prependFowardMsgSubject: true,
		defaultMaxNumMsgsFromNewsgroup: 500,
		msgSaveDir: "",
		useLightbarInterface: true,
		usersCanChangeTheirNNTPSettings: false,
		newsgroupListHdrFilenameBase: "",
		newsgroupListHdrMaxNumLines: 8,
		articleListHdrFilenameBase: "",
		articleListHdrMaxNumLines: 8,
		colors: {
			// Newsgroup list
			grpListHdrColor: "\x01n\x01w\x01h",
			grpListNameColor: "\x01n\x01c",
			grpListDescColor: "\x01n\x01c",
			grpListNameSelectedColor: "\x01n\x01h\x01c\x014",
			grpListDescSelectedColor: "\x01n\x01h\x01c\x014",
			// Newsgroup list help line
			lightbarNewsgroupListHelpLineBkgColor: "\x017", 	// Background
			lightbarNewsgroupListHelpLineGeneralColor: "\x01b",
			lightbarNewsgroupListHelpLineHotkeyColor: "\x01r",
			lightbarNewsgroupListHelpLineParenColor: "\x01m",
			// Article list
			articleListHdrColor: "\x01n\x01w\x01h",
			articleNumColor: "\x01n\x01y\x01h",
			articleFromColor: "\x01n\x01c",
			articleToColor: "\x01n\x01c",
			articleSubjColor: "\x01n\x01c",
			articleDateColor: "\x01n\x01b\x01h",
			articleTimeColor: "\x01n\x01b\x01h",
			// Article list help line (for lightbar mode) - Only used for ANSI terminals
			lightbarArticleListHelpLineBkgColor: "\x017", 	// Background
			lightbarArticleListHelpLineGeneralColor: "\x01b",
			lightbarArticleListHelpLineHotkeyColor: "\x01r",
			lightbarArticleListHelpLineParenColor: "\x01m",
			// Article list - Highlighted item
			articleNumHighlightedColor: "\x01n\x01y\x01h\x014",
			articleFromHighlightedColor: "\x01n\x01c\x01h\x014",
			articleToHighlightedColor: "\x01n\x01c\x01h\x014",
			articleSubjHighlightedColor: "\x01n\x01c\x01h\x014",
			articleDateHighlightedColor: "\x01n\x01w\x01h\x014",
			articleTimeHighlightedColor: "\x01n\x01w\x01h\x014",
			// Scrollable reader mode help line (for lightbar mode) - Only used for ANSI terminals
			scrollableReadHelpLineBkgColor: "\x017", 	// Background
			scrollableReadHelpLineGeneralColor: "\x01b",
			scrollableReadHelpLineHotkeyColor: "\x01r",
			scrollableReadHelpLineParenColor: "\x01m",
			// Help text
			helpText: "\x01c",
			// Colors for the default message header displayed above message
			msgHdrMsgNumColor: "\x01n\x01b\x01h", // Message #
			msgHdrFromColor: "\x01n\x01b\x01h",   // From username
			msgHdrToColor: "\x01n\x01b\x01h",     // To username
			msgHdrToUserColor: "\x01n\x01g\x01h", // To username when it's to the current user
			msgHdrSubjColor: "\x01n\x01b\x01h",   // Message subject
			msgHdrDateColor: "\x01n\x01b\x01h",   // Message date
			// Error messages
			errorMsg: "\x01r\x01h"
		}
	};
	settings.server.hostname = "127.0.0.1";
	settings.server.port = 119;
	settings.server.username = user.alias;
	//settings.server.password = user.security.password; // Don't use this; use the configured default server password

	// Figure out where the configuration file is
	var cfgFilename = file_cfgname(system.mods_dir, pDefaultCfgFilename);
	if (!file_exists(cfgFilename))
		cfgFilename = file_cfgname(system.ctrl_dir, pDefaultCfgFilename);
	if (!file_exists(cfgFilename))
		cfgFilename = file_cfgname(js.exec_dir, pDefaultCfgFilename);
	// If the configuration file hasn't been found, look to see if there's a .example.ini file
	// available in the same directory 
	if (!file_exists(cfgFilename))
	{
		var defaultExampleCfgFilename = pDefaultCfgFilename;
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

	// Read the configuration settings, if possible
	var themeFilename = "";
	var cfgFile = new File(cfgFilename);
	if (cfgFile.open("r"))
	{
		this.cfgFileSuccessfullyRead = true;
		var settingsObj = cfgFile.iniGetObject();
		cfgFile.close();
		// Newsgroup server settings
		if (typeof(settingsObj.hostname) === "string")
			settings.server.hostname = settingsObj.hostname;
		if (typeof(settingsObj.host_port) === "number")
		{
			settings.serverPort = settingsObj.host_port;
			settings.server.port = settingsObj.host_port;
		}
		if (typeof(settingsObj.default_server_password) === "string")
		{
			settings.defaultPasswordForUsers = settingsObj.default_server_password;
			settings.server.password = settingsObj.default_server_password;
		}
		if (typeof(settingsObj.receive_bufer_size_bytes) === "number" && settingsObj.receive_bufer_size_bytes > 0)
		{
			settings.recvBufSizeBytes = settingsObj.receive_bufer_size_bytes;
			settings.server.recvBufSizeBytes = settingsObj.receive_bufer_size_bytes;
		}
		if (typeof(settingsObj.receive_timeout_seconds) === "number" && settingsObj.receive_timeout_seconds > 0)
		{
			settings.recvTimeoutSeconds = settingsObj.receive_timeout_seconds;
			settings.server.recvTimeoutSeconds = settingsObj.receive_timeout_seconds;
		}
		if (typeof(settingsObj.append_bbs_domain_to_usernames) === "boolean")
			settings.appendBBSDomainToUsernames = settingsObj.append_bbs_domain_to_usernames;
		// Other settings
		if (typeof(settingsObj.prepend_foward_msg_subject) === "boolean")
			settings.prependFowardMsgSubject = settingsObj.prepend_foward_msg_subject;
		if (typeof(settingsObj.default_max_num_msgs_from_newsgroup) === "number" && settingsObj.default_max_num_msgs_from_newsgroup > 0)
			settings.defaultMaxNumMsgsFromNewsgroup = settingsObj.default_max_num_msgs_from_newsgroup;
		if (typeof(settingsObj.msg_save_dir) === "string" && settingsObj.msg_save_dir.length > 0)
			settings.msgSaveDir = settingsObj.msg_save_dir;
		if (typeof(settingsObj.use_lightbar_interface) === "boolean")
			settings.useLightbarInterface = settingsObj.use_lightbar_interface;
		if (typeof(settingsObj.users_can_change_their_nttp_settings) === "boolean")
			settings.usersCanChangeTheirNNTPSettings = settingsObj.users_can_change_their_nttp_settings;
		if (typeof(settingsObj.newsgroup_list_hdr_filename_base) === "string" && settingsObj.newsgroup_list_hdr_filename_base.length > 0)
			settings.newsgroupListHdrFilenameBase = settingsObj.newsgroup_list_hdr_filename_base;
		if (typeof(settingsObj.newsgroup_list_hdr_max_num_lines) === "number")
			settings.newsgroupListHdrMaxNumLines = settingsObj.newsgroup_list_hdr_max_num_lines;
		if (typeof(settingsObj.article_list_hdr_filename_base) === "string" && settingsObj.article_list_hdr_filename_base.length > 0)
			settings.articleListHdrFilenameBase = settingsObj.article_list_hdr_filename_base;
		if (typeof(settingsObj.article_list_hdr_max_num_lines) === "number")
			settings.articleListHdrMaxNumLines = settingsObj.article_list_hdr_max_num_lines;
		if (typeof(settingsObj.theme_config_filename) === "string" && settingsObj.theme_config_filename.length > 0)
			themeFilename = settingsObj.theme_config_filename;
	}
	// Colors - Read the color settings from the configured color theme file, if there is one
	if (themeFilename.length > 0)
	{
		var themeFilename = file_cfgname(system.mods_dir, themeFilename);
		if (!file_exists(themeFilename))
			themeFilename = file_cfgname(system.ctrl_dir, themeFilename);
		if (!file_exists(themeFilename))
			themeFilename = file_cfgname(js.exec_dir, themeFilename);
		// If the configuration file hasn't been found, look to see if there's a .example.ini file
		// available in the same directory 
		if (!file_exists(themeFilename))
		{
			var exampleFileName = "";
			var dotIdx = themeFilename.lastIndexOf(".");
			if (dotIdx > -1)
			{
				exampleFileName = themeFilename.substring(0, dotIdx);
				exampleFileName += ".example" + themeFilename.substring(dotIdx);
			}
			else
				exampleFileName = themeFilename + ".example.ini";
			if (file_exists(exampleFileName))
				themeFilename = exampleFileName;
		}
		// If the theme file exists, read the color settings from it
		if (file_exists(themeFilename))
		{
			var themeFile = new File(themeFilename);
			if (themeFile.open("r"))
			{
				var colorSettings = themeFile.iniGetObject("COLORS");
				themeFile.close();
				for (var prop in settings.colors)
				{
					if (colorSettings.hasOwnProperty(prop) && typeof(colorSettings[prop]) === "string")
						settings.colors[prop] = attrCodeStr(colorSettings[prop]);
				}
			}
		}
	}

	return settings;
}

// Reads the user's settings
function ReadUserSettings(pUserSettingsFilename, pDefaultNNTPConnectOptions)
{
	var retObj = {
		successfullyRead: false,
		NNTPSettings: new NNTPConnectOptions(),
		subscribedNewsgroups: []
	};

	// Copy the default NNTP server credentials to the user credentials object
	for (var prop in pDefaultNNTPConnectOptions)
		retObj.NNTPSettings[prop] = pDefaultNNTPConnectOptions[prop];

	// Read the user settings file, if possible
	var settingsFile = new File(pUserSettingsFilename);
	if (settingsFile.open("r"))
	{
		var userSettings = settingsFile.iniGetObject("NNTPSettings");
		var subscribedNewsgroups = settingsFile.iniGetValue("SUBSCRIBED_NEWSGROUPS", "newsgroups_list", "");
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
		if (subscribedNewsgroups.length > 0)
			retObj.subscribedNewsgroups = subscribedNewsgroups.split(",");
	}

	return retObj;
}

// Saves the user's settings
function SaveUserSettings(pUserSettingsFilename, pSaveNNTPSettings)
{
	var succeeded = false;
	var settingsFile = new File(pUserSettingsFilename);
	if (settingsFile.open(settingsFile.exists ? "r+" : "w+"))
	{
		succeeded = true;
		if (pSaveNNTPSettings)
		{
			for (var prop in gUserSettings.NNTPSettings)
			{
				var value = gUserSettings.NNTPSettings[prop];
				// Base64-encode the user's password so that it isn't just
				// stored as plaintext
				if (prop == "password")
					value = base64_encode(gUserSettings.NNTPSettings[prop]);
				if (!settingsFile.iniSetValue("NNTPSettings", prop, value))
					succeeded = false;
			}
		}
		if (!settingsFile.iniSetValue("SUBSCRIBED_NEWSGROUPS", "newsgroups_list", gUserSettings.subscribedNewsgroups.join(",")))
			succeeded = false;
		//settingsFile.flush();
		settingsFile.close();
	}
	else
		log(LOG_ERR, format("* %s: Faled to save user settings file: %s", PROGRAM_NAME, pUserSettingsFilename));
	return succeeded;
}

// Gets the user's external editor configuration from xtrn.ini
//
// Return value: An object with the following properties:
//               userHasExternalEditor: Boolean - Whether or not the user is using an external editor
//               editorIsQuickBBSStyle: Boolean - Whether or not the user's external editor is QuickBBS
//                                      MSGINF/MSGTMP style (true), or WWIF EDITOR.INF/RESULT.ED style (false)
function GetUserExternalEditorCfg()
{
	// Internal editor: 
	var retObj = {
		userHasExternalEditor: false,
		editorIsQuickBBSStyle: false
	};

	if (user.editor == "")
		return retObj;

	retObj.userHasExternalEditor = true;
	var xtrnIni = new File(system.ctrl_dir + "xtrn.ini");
	if (xtrnIni.open("r"))
	{
		var editorSettings = xtrnIni.iniGetObject("editor:" + user.editor.toUpperCase());
		xtrnIni.close();
		retObj.editorIsQuickBBSStyle = Boolean(editorSettings.settings & XTRN_QUICKBBS);
	}
	else
		log(LOG_ERR, format("* %s: Failed to read %s (to get external editor configuration)",
		                    PROGRAM_NAME, system.ctrl_dir + "xtrn.ini"));

	return retObj;
}

// Reads the current user's usage information file for DDMsgReader
function ReadUserUsageInfo(pUserUsageInfoFilename)
{
	var userUsageInfo = {
		servers: {}
	};

	var userUsageFile = new File(pUserUsageInfoFilename);
	if (userUsageFile.open("r"))
	{
		var usageFileContents = userUsageFile.read(userUsageFile.length);
		userUsageFile.close();
		try
		{
			userUsageInfo = JSON.parse(usageFileContents);
		}
		catch (error)
		{
			log(LOG_ERR, format("* %s: Failed to load user usage information from %s; line number: %d: %s",
			                    PROGRAM_NAME, pUserUsageInfoFilename, error.lineNumber, error));
		}
	}

	return userUsageInfo;
}

// Gets a default object for saving a user's usage information for a newsgroup server
function GetDefaultUserUsageServerInfoObj()
{
	return {
		newsgroups: {} // Keyed by newsgroup name
	};
}

// Gets a default object for saving a user's usage information for a newsgroup
function GetDefaultUserUsageNewsgroupInfoObj()
{
	return {
		estimatedNumArticles: 0,
		preferredMaxNumArticles: 0
	};
}

// Lets the user update their settings
function LetUserUpdateSettings(pUserSettingsFilename)
{
	console.attributes = "N";
	console.print("\x01cServer hostname\x01g\x01h: \x01n");
	//var userInput = console.getstr(console.screen_columns - 17, K_LINE | K_TRIM | K_NOSPIN);
	var userInput = console.getstr(gUserSettings.NNTPSettings.hostname, console.screen_columns - 17, K_EDIT | K_LINE | K_TRIM | K_NOSPIN);
	if (userInput.length > 0)
		gUserSettings.NNTPSettings.hostname = userInput;
	console.print("\x01cServer port (default=119)\x01g\x01h: \x01n");
	//var port = parseInt(console.getstr(console.screen_columns - 27, K_LINE | K_TRIM | K_NOSPIN | K_NUMBER));
	var port = parseInt(console.getstr(gUserSettings.NNTPSettings.port.toString(), console.screen_columns - 27, K_EDIT | K_LINE | K_TRIM | K_NOSPIN | K_NUMBER));
	if (!isNaN(port))
		gUserSettings.NNTPSettings.port = port;
	else
		gUserSettings.NNTPSettings.port = 119;
	console.print("\x01cUsername\x01g\x01h: \x01n");
	gUserSettings.NNTPSettings.username = console.getstr(gUserSettings.NNTPSettings.username, console.screen_columns - 10, K_EDIT | K_LINE | K_TRIM | K_NOSPIN);
	console.print("\x01cPassword\x01g\x01h: \x01n");
	gUserSettings.NNTPSettings.password = console.getstr(console.screen_columns - 10, K_LINE | K_TRIM | K_NOSPIN | K_NOECHO);
	if (!SaveUserSettings(pUserSettingsFilename, true))
	{
		log(LOG_ERR, format("* %s: Failed to save user settings to file!", PROGRAM_NAME));
		console.print("\x01n\x01y\x01hFailed to save your settings!\x01n\r\n\x01p");
	}
}

// Returns whether the lightbar interface is enabled and supported by the user's terminal
function LightbarIsSupported()
{
	return (gSettings.useLightbarInterface && console.term_supports(USER_ANSI));
}

// Returns a string where for each word, the first letter will have one set of Synchronet attributes
// applied and the remainder of the word will have another set of Synchronet attributes applied
function ColorFirstCharAndRemainingCharsInWords(pStr, pWordFirstCharAttrs, pWordRemainderAttrs)
{
	if (typeof(pStr) !== "string" || pStr.length == 0)
		return "";
	if (typeof(pWordFirstCharAttrs) !== "string" || typeof(pWordRemainderAttrs) !== "string")
		return pStr;

	var wordsArray = pStr.split(" ");
	for (var i = 0; i < wordsArray.length; ++i)
	{
		if (wordsArray[i] != " ")
			wordsArray[i] = "\x01n" + pWordFirstCharAttrs + wordsArray[i].substr(0, 1) + "\x01n" + pWordRemainderAttrs + wordsArray[i].substr(1);
	}
	return wordsArray.join(" ");
}

// Returns whether or not a character is printable.
//
// Parameters:
//  pChar: A character to test
//
// Return value: Boolean - Whether or not the character is printable
function isPrintableChar(pChar)
{
	// Make sure pChar is valid and is a string.
	if (typeof(pChar) != "string")
		return false;
	if (pChar.length == 0)
		return false;

	// Make sure the character is a printable ASCII character in the range of 32 to 254,
	// except for 127 (delete).
	var charCode = pChar.charCodeAt(0);
	return ((charCode > 31) && (charCode < 255) && (charCode != 127));
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
	for (var i = 0; i < pAttrCodeCharStr.length; ++i)
		str += "\x01" + pAttrCodeCharStr[i];
	return str;
}

// Creates the newsgroup menu, with the newsgroups in it.
//
// Parameters:
//  pNNTPClient: The NNTPClient object to communicate with the news server
//  pSubscribedNewsgroups: An array of names of subscribed newsgroups
//  pShowOnlySubscribedNewsgroups: Boolean - Whether or not to show only the subscribed
//                                 newsgroups (if there are any)
//
// Return value: An object with the following properties:
//               newsgrpMenu: The DDLightbarMenu object representing the menu. Will be null on error or if there are no newsgroups.
//               newsgroupItemFormatStr: printf format string for newsgroups & newsgroup menu header
//               errorMsg: An empty string on success, or will contain an error string on failure
//               numNewsgroups: The number of newsgroups retrieved
function CreateNewsgroupMenu(pNNTPClient, pSubscribedNewsgroups, pShowOnlySubscribedNewsgroups)
{
	var newsgroupNameWidth = Math.floor(console.screen_columns / 2) - 1;
	var newsgroupDescWidth = console.screen_columns - newsgroupNameWidth - 1;

	var retObj = {
		newsgrpMenu: null,
		newsgroupItemFormatStr: "%-" + newsgroupNameWidth + "s %-" + newsgroupDescWidth + "s",
		hdrFormatStr: "",
		errorMsg: "",
		numNewsgroups: 0
	};

	var usingSubscribedNewsgroups = (pShowOnlySubscribedNewsgroups && pSubscribedNewsgroups.length > 0);
	// If we are to only show subscribed newsgroups (and there are some), then
	// just use those.  Otherwise, request the full list of newsgroups from
	// the server.
	var getNewsgrpsRetObj = null;
	if (usingSubscribedNewsgroups)
	{
		getNewsgrpsRetObj = {
			succeeded: true,
			responseLine: "",
			newsgroupArray: []
		};
		for (var i = 0; i < pSubscribedNewsgroups.length; ++i)
		{
			getNewsgrpsRetObj.newsgroupArray.push({
				name: pSubscribedNewsgroups[i],
				desc: pSubscribedNewsgroups[i]
			});
		}
		retObj.numNewsgroups = getNewsgrpsRetObj.newsgroupArray.length;
	}
	else
	{
		var timeoutOverride = 15;
		if (!file_exists(gNewsgroupListFilename))
		{
			getNewsgrpsRetObj = pNNTPClient.GetNewsgroups(timeoutOverride);
			if (getNewsgrpsRetObj.succeeded)
			{
				retObj.numNewsgroups = getNewsgrpsRetObj.newsgroupArray.length;
				if (getNewsgrpsRetObj.newsgroupArray.length > 0)
				{
					// Write the newsgroup list to gNewsgroupListFilename
					var outFile = new File(gNewsgroupListFilename);
					if (outFile.open("w"))
					{
						for (var i = 0; i < getNewsgrpsRetObj.newsgroupArray.length; ++i)
						{
							outFile.writeln(getNewsgrpsRetObj.newsgroupArray[i].name);
							outFile.writeln(getNewsgrpsRetObj.newsgroupArray[i].desc);
							outFile.writeln("");
						}
						outFile.close();
					}
				}
				else // There are no newsgroups
					return retObj;
			}
			else
			{
				retObj.errorMsg = pNNTPClient.GetLastError();
				if (retObj.errorMsg.length == 0)
					retObj.errorMsg = "Failed to retrieve newsgroups";
				return retObj;
			}
		}
		else
		{
			// gNewsgroupListFilename exists
			// Set up getNewsgrpsRetObj like it is from GetNewsgroups()
			getNewsgrpsRetObj = {
				succeeded: false,
				responseLine: "",
				newsgroupArray: []
			};

			var count = 1; // 1: Newsgroup name; 2: Newsgroup description
			var newsgrpName = "";
			var inFile = new File(gNewsgroupListFilename);
			var grpCacheFileTimestamp = inFile.date;
			if (inFile.open("r"))
			{
				while (!inFile.eof)
				{
					var fileLine = inFile.readln(4096);
					// fileLine should be a string, but I've seen some cases
					// where for some reason it isn't.  If it's not a string,
					// then continue onto the next line.
					if (typeof(fileLine) !== "string")
						continue;

					switch (count)
					{
						case 0: // Ignore the line (blank line expected here)
							count = 1;
							break;
						case 1: // Newsgroup name
							newsgrpName = fileLine;
							count = 2;
							break;
						case 2: // Newsgroup description
							if (newsgrpName != "")
							{
								getNewsgrpsRetObj.newsgroupArray.push({
									name: newsgrpName,
									desc: fileLine
								});
							}
							count = 0; // Ignore the next line
							break;
					}
				}
				inFile.close();

				// Also, get any new newsgroups since the cache file was written
				var ngRet = pNNTPClient.GetNewNewsgroupsSinceLocalTimestamp(grpCacheFileTimestamp, timeoutOverride);
				if (ngRet.succeeded)
				{
					if (ngRet.newsgroupArray.length > 0)
					{
						for (var i = 0; i < ngRet.newsgroupArray.length; ++i)
						{
							getNewsgrpsRetObj.newsgroupArray.push({
								name: ngRet.newsgroupArray[i].name,
								desc: ngRet.newsgroupArray[i].name
							});
						}

						// Add the new newsgroups to the newsgroup cache file
						var outFile = new File(gNewsgroupListFilename);
						if (outFile.open("a"))
						{
							for (var i = 0; i < ngRet.newsgroupArray.length; ++i)
							{
								outFile.writeln(ngRet.newsgroupArray[i].name);
								outFile.writeln(ngRet.newsgroupArray[i].name);
								outFile.writeln("");
							}
							outFile.close();
						}
					}
				}
				else
				{
					var fileTimestampStr = strftime("%Y-%m-%d %H:%M:%S", grpCacheFileTimestamp);
					if (ngRet.responseLine.length > 0)
						retObj.errorMsg = format("Failed to retrieve new newsgroups since %s: %s", fileTimestampStr, ngRet.responseLine);
					else
						retObj.errorMsg = format("Failed to retrieve new newsgroups since %s", fileTimestampStr);
					return retObj;
				}

				retObj.numNewsgroups = getNewsgrpsRetObj.newsgroupArray.length;
			}
		}
	}

	// Create the header format string. If lightbar is not enabled/supported, it will
	// need an additional column for the item number (for numbered mode).
	if (LightbarIsSupported())
		retObj.hdrFormatStr = "%-" + newsgroupNameWidth + "s %-" + newsgroupDescWidth + "s";
	else
	{
		var itemNumLen = retObj.numNewsgroups.toString().length;
		retObj.hdrFormatStr = "%" + itemNumLen + "s %-" + newsgroupNameWidth + "s %-" + newsgroupDescWidth + "s";
	}

	// Sort the newsgroups array
	getNewsgrpsRetObj.newsgroupArray.sort(function(pA, pB) {
		/*
		// Temporary
		for (var prop in pA)
			console.print(prop + ": " + pA[prop] + "\r\n");
		// End Temporary
		*/
		var nameA = pA.name.toUpperCase();
		var nameB = pB.name.toUpperCase();
		if (nameA < nameB)
			return -1;
		else if (nameA == nameB)
			return 0;
		else
			return 1;
	});

	var nameWidth = Math.floor(console.screen_columns / 2) - 1;
	var descWidth = console.screen_columns - nameWidth - 1;
	if (!LightbarIsSupported())
		nameWidth -= getNewsgrpsRetObj.newsgroupArray.length.toString().length;
	// Start & end indexes for the newsgroups
	var newsgrpIdxes = {
		grpNameStart: 0,
		grpNameEnd: nameWidth,
		grpDescStart: nameWidth,
		grpDescEnd: nameWidth + descWidth
	};

	var menuStartRow = gNewsgroupListHdrLines.length + 3;
	var menuHeight = console.screen_rows - gNewsgroupListHdrLines.length - 3;
	retObj.newsgrpMenu = new DDLightbarMenu(1, menuStartRow, console.screen_columns, menuHeight);
	retObj.newsgrpMenu.wrapNavigation = false;
	retObj.newsgrpMenu.scrollbarEnabled = true;
	retObj.newsgrpMenu.borderEnabled = false;
	retObj.newsgrpMenu.SetColors({
		itemColor: [{start: newsgrpIdxes.grpNameStart, end: newsgrpIdxes.grpNameEnd, attrs: "\x01n" + gSettings.colors.grpListNameColor},
		            {start: newsgrpIdxes.grpDescStart, end: newsgrpIdxes.grpDescEnd, attrs: "\x01n" + gSettings.colors.grpListDescColor}],
		selectedItemColor: [{start: newsgrpIdxes.grpNameStart, end: newsgrpIdxes.grpNameEnd, attrs: "\x01n" + gSettings.colors.grpListNameSelectedColor},
		                    {start: newsgrpIdxes.grpDescStart, end: newsgrpIdxes.grpDescEnd, attrs: "\x01n" + gSettings.colors.grpListDescSelectedColor}]
	});

	retObj.newsgrpMenu.allowANSI = LightbarIsSupported();
	retObj.newsgrpMenu.colors.itemNumColor = gSettings.colors.articleNumColor; // For numbered mode
	//retObj.newsgrpMenu.multiSelect = false;
	retObj.newsgrpMenu.multiSelect = true;
	retObj.newsgrpMenu.ampersandHotkeysInItems = false;

	var additionalQuitKeys = "Qq/nN?" + CTRL_Q;
	if (usingSubscribedNewsgroups)
		additionalQuitKeys += KEY_DEL;
	retObj.newsgrpMenu.AddAdditionalQuitKeys(additionalQuitKeys);


	// Add the newsgroups to the menu
	retObj.newsgrpMenu.fullDescs = []; // Full descriptions for the newsgroups
	for (var i = 0; i < getNewsgrpsRetObj.newsgroupArray.length; ++i)
	{
		var item = format(retObj.newsgroupItemFormatStr, getNewsgrpsRetObj.newsgroupArray[i].name.substr(0, newsgroupNameWidth), getNewsgrpsRetObj.newsgroupArray[i].desc.substr(0, newsgroupDescWidth));
		retObj.newsgrpMenu.Add(item, getNewsgrpsRetObj.newsgroupArray[i].name);
		retObj.newsgrpMenu.fullDescs.push(getNewsgrpsRetObj.newsgroupArray[i].desc);
	}

	return retObj;
}

// UI facilitator: Creates the newsgroup menu and exits if there's an error
function CreateNewsgroupMenu_ExitOnError(pNNTPClient, pSubscribedNewsgroups, pShowOnlySubscribedNewsgroups)
{
	var createNewsgrpMenuRetObj = CreateNewsgroupMenu(pNNTPClient, pSubscribedNewsgroups, pShowOnlySubscribedNewsgroups);
	if (createNewsgrpMenuRetObj.errorMsg.length > 0)
	{
		nntpClient.Disconnect();
		printf("Error getting newsgroup list: %s\n", createNewsgrpMenuRetObj.errorMsg);
		exit(2);
	}
	else if (createNewsgrpMenuRetObj.numNewsgroups == 0)
	{
		console.print("\x01nThere are no newsgroups.\r\n\x01p");
		exit(0);
	}
	return createNewsgrpMenuRetObj;
}

// Generates an object with the newsgroup heirarchy. Newsgroup names are split on dots and
// a heirarchy is built from there.  For instance, a newsgroup name of Local.General would
// be split so that Local is a group and General is a newsgroup within it.  Similarly,
// alt.bbs.ads would be split so that:
// alt
//   bbs
//     ads
//     and so on
//
// Parameters:
//  pNewsgroupArray: An array of newsgroup information as returned by GetNewsgroups() in
//                   nntp_client_lib.js. Each newsgroup object has 'name' and 'desc'
//                   properties.
//
// Return value: An object indexed by a name, where for each name, there is an object with
//               the following properties:
//               isNewsgroup: If true, the object represents a newsgroup on the server. If false,
//                            it's a group with similar objects below it
//               desc: If isNewsgroup is true, this property will exist and contains the description
//                     of the newsgroup
function MakeNewsgrpHeirarchy(pNewsgroupArray)
{
	var retObj = {};

	for (var i = 0; i < pNewsgroupArray.length; ++i)
	{
		//pNewsgroupArray[i].name
		//pNewsgroupArray[i].desc
		var nameParts = pNewsgroupArray[i].name.split(".");
	}

	return retObj;
}

// Creates a menu for articles in a newsgroup, with the article header information in it.
// This assumes the newsgroup has already been selected on the server.
//
// Parameters:
//  pSelectNGRetObj: The return object from NNTPClient.SelectNewsgroup()
//  pNewsgroupName: The name of the newsgroup to get articles for
//  pMaxNumArticles: The maximum number of articles to retrieve - The last articles
//                   will be retrieved, up to this many
//
// Return value: An object with the following properties:
//               articleMenu: The DDLightbarMenu object representing the menu. Will be null on error or if there are no newsgroups.
//               errorMsg: An empty string on success, or will contain an error string on failure
//               hdrFormatStr: A printf format string for displaying the column headers
function CreateArticleMenuForNewsgroup(pNNTPClient, pSelectNGRetObj, pMaxNumArticles)
{
	var retObj = {
		articleMenu: null,
		errorMsg: "",
		hdrFormatStr: ""
	};

	// Create the menu
	var menuStartRow = gArticleListHdrLines.length + 3;
	var menuHeight = console.screen_rows - gArticleListHdrLines.length - 3;
	retObj.articleMenu = new DDLightbarMenu(1, menuStartRow, console.screen_columns, menuHeight);
	retObj.articleMenu.wrapNavigation = false;
	retObj.articleMenu.scrollbarEnabled = true;
	retObj.articleMenu.borderEnabled = false;
	retObj.articleMenu.allowANSI = LightbarIsSupported();
	retObj.articleMenu.colors.itemNumColor = gSettings.colors.articleNumColor; // For numbered mode
	retObj.articleMenu.multiSelect = false;
	retObj.articleMenu.ampersandHotkeysInItems = false;

	var additionalQuitKeys = "QqPp/Nn0123456789?" + CTRL_U + CTRL_Q;
	retObj.articleMenu.AddAdditionalQuitKeys(additionalQuitKeys);


	// Add the articles and article numebrs to arrays in the menu object
	//pSelectNGRetObj.estNumArticles
	//pSelectNGRetObj.firstArticleNum
	//pSelectNGRetObj.lastArticleNum
	retObj.articleMenu.articleHdrs = [];
	retObj.articleMenu.articleNums = []; // Article numbers mapped to item indexes in the menu

	// Get the articles
	if (typeof(pMaxNumArticles) === "number" && pMaxNumArticles > 0)
	{
		// Calculate how many blocks to display for the progress bar.
		// There will be 12 characters for the status & percentage, and
		// leave one more for a space between that & the progress bar.
		var numStatusBlocks = console.screen_columns - 13; // 12

		if (!LightbarIsSupported())
			console.print("\r\nFiltering articles\r\n");
		else
		{
			// Clear the bottom row to make room to write the progress of retrieving articles
			console.gotoxy(1, console.screen_rows);
			console.cleartoeol("N");
			console.gotoxy(1, console.screen_rows);
		}
		var numArticlesRetrieved = 0;
		var articles = []; // Will contain objects with articleNum and article
		for (var articleNum = pSelectNGRetObj.lastArticleNum; articleNum >= pSelectNGRetObj.firstArticleNum && numArticlesRetrieved < pMaxNumArticles; --articleNum)
		{
			var hdrRetObj = pNNTPClient.GetArticleHeader(articleNum);
			if (hdrRetObj.succeeded)
			{
				++numArticlesRetrieved;
				articles.push({
					articleNum: articleNum,
					article: hdrRetObj.articleHdr
				});
				// Print the number of articles retrieved
				if (numArticlesRetrieved % 4 == 0 || numArticlesRetrieved >= pMaxNumArticles)
				{
					// Display the progress, along with a progress bar
					var numArticlesFraction = numArticlesRetrieved / pMaxNumArticles;
					var numArticlesPercentage = numArticlesFraction * 100.0;
					//printf("\r1/2 %6.2f%% (%d)", numArticlesPercentage, pMaxNumArticles);
					var numProgressBlocks = Math.floor(numArticlesFraction * numStatusBlocks);
					var numRemainingBlocks = numStatusBlocks - numProgressBlocks;
					printf("\r1/2 %6.2f%% ", numArticlesPercentage);
					// TODO: Could this be more efficient?
					for (var j = 0; j < numProgressBlocks; ++j)
						console.print(CP437_FULL_BLOCK);
					for (var j = 0; j < numRemainingBlocks; ++j)
						console.print(CP437_LIGHT_SHADE);
				}
			}
		}
		// Add the articles to the menu. Go through the articles array in
		// reverse order, since we aded them in reverse order
		if (!LightbarIsSupported())
			console.print("\r\nPopulating articles\r\n");
		for (var i = articles.length - 1; i >= 0; --i)
		{
			retObj.articleMenu.articleHdrs.push(articles[i].article);
			retObj.articleMenu.articleNums.push(articles[i].articleNum);
			// Print the number of articles retrieved
			if (retObj.articleMenu.articleHdrs.length % 4 == 0)
			{
				// Display the progress, along with a progress bar
				var numArticlesFraction = retObj.articleMenu.articleHdrs.length / articles.length;
				var numArticlesPercentage = numArticlesFraction * 100.0;
				//printf("\r2/2 %6.2f%% (%d)", numArticlesPercentage, articles.length);
				printf("\r2/2 %6.2f%% ", numArticlesPercentage);
				var numProgressBlocks = Math.floor(numArticlesFraction * numStatusBlocks);
				var numRemainingBlocks = numStatusBlocks - numProgressBlocks;
				// TODO: Could this be more efficient?
				for (var j = 0; j < numProgressBlocks; ++j)
					console.print(CP437_FULL_BLOCK);
				for (var j = 0; j < numRemainingBlocks; ++j)
					console.print(CP437_LIGHT_SHADE);
			}
		}
		console.line_counter = 0; // To prevent pausing after showing the progress
	}
	else
	{
		if (!LightbarIsSupported())
			console.crlf();
		// Get all articles
		for (var articleNum = pSelectNGRetObj.firstArticleNum; articleNum <= pSelectNGRetObj.lastArticleNum; ++articleNum)
		{
			var hdrRetObj = pNNTPClient.GetArticleHeader(articleNum);
			if (hdrRetObj.succeeded)
			{
				retObj.articleMenu.articleHdrs.push(hdrRetObj.articleHdr);
				retObj.articleMenu.articleNums.push(articleNum);
				// Print the number of articles retrieved
				if (retObj.articleMenu.articleHdrs.length % 4 == 0 || retObj.articleMenu.articleHdrs.length >= pSelectNGRetObj.estNumArticles)
				{
					var numArticlesPercentage = retObj.articleMenu.articleHdrs.length / pSelectNGRetObj.estNumArticles * 100;
					printf("\r%.2f%% (%d)   ", numArticlesPercentage, retObj.articleMenu.articleHdrs.length);
				}
			}
		}
		console.line_counter = 0; // To prevent pausing after showing the progress
	}

	// Now that we've retrieved all the headers and we know how many there are, generate
	// the lengths & colors.
	var setMenuLengthsRetObj = SetLengthsAndPositionsForArticleMenu(retObj.articleMenu);
	retObj.hdrFormatStr = setMenuLengthsRetObj.hdrFormatStr;

	return retObj;
}

function SetLengthsAndPositionsForArticleMenu(pMenu)
{
	var retObj = {
		hdrFormatStr: ""
	};

	// Now that we've retrieved all the headers and we know how many there are, generate
	// the lengths & colors.
	var msgNumLen = pMenu.articleHdrs.length.toString().length;
	var dateLen = 10; // i.e., YYYY-MM-DD
	var timeLen = 8;  // i.e., HH:MM:SS
	var fromLen = Math.floor((console.screen_columns * (15/console.screen_columns)));
	var toLen = Math.floor(console.screen_columns * (15/console.screen_columns));
	//var colsLeftForSubject = console.screen_columns-msgNumLen-dateLen-timeLen-fromLen-toLen-6; // 6 to account for the spaces
	//var subjLen = Math.floor(console.screen_columns * (colsLeftForSubject/console.screen_columns));
	var subjLen = 0;
	if (LightbarIsSupported())
		subjLen = console.screen_columns - msgNumLen - dateLen - timeLen - fromLen - toLen - 6; // 6 to account for the spaces (was previously 8)
	else
	{
		subjLen = console.screen_columns - msgNumLen - dateLen - timeLen - fromLen - toLen - 6; // 6 to account for the spaces (was previously 8)
	}
	// If the menu scrollbar won't be used (not needed), then add 1 to the subject length. Also, this would have to
	// be adjusted if the menu has borders enabled.
	// TODO: This doesn't seem to be working - If the scrollbar isn't needed, the last character of the time isn't colored
	if (pMenu.articleHdrs.length <= pMenu.size.height)
		++subjLen;

	// Format string for the menu items
	var formatStr = "";
	if (LightbarIsSupported())
		formatStr = "%" + msgNumLen + "d %-" + fromLen + "s %-" + toLen + "s %-" + subjLen + "s %-" + dateLen + "s %-" + timeLen + "s";
	else
		formatStr = "%-" + fromLen + "s %-" + toLen + "s %-" + subjLen + "s %-" + dateLen + "s %-" + timeLen + "s";

	retObj.hdrFormatStr = formatStr;
	// If using ANSI (and displaying numbers as part of the items), then for the header format string, change the first
	// 'd' to an 's' (to display a string in the header)
	if (LightbarIsSupported())
	{
		var spaceIdx = retObj.hdrFormatStr.indexOf(" ");
		if (spaceIdx > -1)
			retObj.hdrFormatStr = "%" + msgNumLen + "s " + retObj.hdrFormatStr.substr(spaceIdx+1);
	}
	// Header format string for non-lightbar mode
	else
		retObj.hdrFormatStr = "%" + msgNumLen + "s %-" + fromLen + "s %-" + toLen + "s %-" + subjLen + "s %-" + dateLen + "s %-" + timeLen + "s";

	/*
	if (!LightbarIsSupported())
		nameWidth -= getNewsgrpsRetObj.newsgroupArray.length.toString().length;
	*/
	// Start & end indexes for the articles
	var articleListIdxes = {};
	if (LightbarIsSupported())
	{
		articleListIdxes.msgNumStart = 0;
		articleListIdxes.msgNumEnd = msgNumLen + 1;
		articleListIdxes.fromNameStart = articleListIdxes.msgNumEnd;
	}
	else
		articleListIdxes.fromNameStart = 0;
	articleListIdxes.fromNameEnd = articleListIdxes.fromNameStart + fromLen + 1;
	articleListIdxes.toNameStart = articleListIdxes.fromNameEnd;
	articleListIdxes.toNameEnd = articleListIdxes.toNameStart + toLen + 1;
	articleListIdxes.subjStart = articleListIdxes.toNameEnd;
	articleListIdxes.subjEnd = articleListIdxes.subjStart + +subjLen + 1;
	articleListIdxes.dateStart = articleListIdxes.subjEnd;
	articleListIdxes.dateEnd = articleListIdxes.dateStart + dateLen + 1;
	articleListIdxes.timeStart = articleListIdxes.dateEnd;
	articleListIdxes.timeEnd = console.screen_columns - 1; // articleListIdxes.timeStart + +this.TIME_LEN + 1;
	// Colors
	var itemColorArray = [];
	var selectedItemColorArray = [];
	if (LightbarIsSupported())
	{
		itemColorArray.push({start: articleListIdxes.msgNumStart, end: articleListIdxes.msgNumEnd, attrs: "\x01n" + gSettings.colors.articleNumColor});
		selectedItemColorArray.push({start: articleListIdxes.msgNumStart, end: articleListIdxes.msgNumEnd, attrs: "\x01n" + gSettings.colors.articleNumHighlightedColor});
	}
	itemColorArray.push({start: articleListIdxes.fromNameStart, end: articleListIdxes.fromNameEnd, attrs: "\x01n" + gSettings.colors.articleFromColor});
	itemColorArray.push({start: articleListIdxes.toNameStart, end: articleListIdxes.toNameEnd, attrs: "\x01n" + gSettings.colors.articleToColor});
	itemColorArray.push({start: articleListIdxes.subjStart, end: articleListIdxes.subjEnd, attrs: "\x01n" + gSettings.colors.articleSubjColor});
	itemColorArray.push({start: articleListIdxes.dateStart, end: articleListIdxes.dateEnd, attrs: "\x01n" + gSettings.colors.articleDateColor});
	itemColorArray.push({start: articleListIdxes.timeStart, end: articleListIdxes.timeEnd, attrs: "\x01n" + gSettings.colors.articleTimeColor});
	selectedItemColorArray.push({start: articleListIdxes.fromNameStart, end: articleListIdxes.fromNameEnd, attrs: "\x01n" + gSettings.colors.articleFromHighlightedColor});
	selectedItemColorArray.push({start: articleListIdxes.toNameStart, end: articleListIdxes.toNameEnd, attrs: "\x01n" + gSettings.colors.articleToHighlightedColor});
	selectedItemColorArray.push({start: articleListIdxes.subjStart, end: articleListIdxes.subjEnd, attrs: "\x01n" + gSettings.colors.articleSubjHighlightedColor});
	selectedItemColorArray.push({start: articleListIdxes.dateStart, end: articleListIdxes.dateEnd, attrs: "\x01n" + gSettings.colors.articleDateHighlightedColor});
	selectedItemColorArray.push({start: articleListIdxes.timeStart, end: articleListIdxes.timeEnd, attrs: "\x01n" + gSettings.colors.articleTimeHighlightedColor});
	pMenu.SetColors({
		itemColor: itemColorArray,
		selectedItemColor: selectedItemColorArray
	});

	// Add the item texts to the menu.
	// articleHdrs and articleNums in the menu object should be the same length.
	for (var idx = 0; idx < pMenu.articleHdrs.length; ++idx)
	{
		var hdr = pMenu.articleHdrs[idx];
		var fromName = "";
		// from example: "Nightfox" <nightfox@digitaldistortionbbs.com>
		var fromComponents = hdr.from.match(/".*"/);
		if (Array.isArray(fromComponents))
			fromName = fromComponents[0].substr(1, fromComponents[0].length-2);
		else
			fromName = hdr.from;
		var toName = (hdr.hasOwnProperty("to") ? hdr.to : "All");
		var subject = (hdr.hasOwnProperty("subject") ? hdr.subject : "No Subject");
		// Parse the date & time
		var dateParseRetObj = parseHdrDate(hdr);
		var dateStr = format("%04d-%02d-%02d", dateParseRetObj.year, dateParseRetObj.monthNum, dateParseRetObj.dayOfMonthNum);
		var timeStr = format("%02d:%02d:%02d", dateParseRetObj.hour, dateParseRetObj.min, dateParseRetObj.second);
		
		var itemText = "";
		if (LightbarIsSupported())
			itemText = format(formatStr, idx+1, fromName.substr(0, fromLen), toName.substr(0, toLen), subject.substr(0, subjLen), dateStr.substr(0, dateLen), timeStr.substr(0, timeLen));
		else
			itemText = format(formatStr, fromName.substr(0, fromLen), toName.substr(0, toLen), subject.substr(0, subjLen), dateStr.substr(0, dateLen), timeStr.substr(0, timeLen));
		pMenu.Add(itemText, pMenu.articleNums[idx]);
	}
	
	/*
	pMenu.GetItem = function(pItemIndex) {
		var menuItemObj = this.MakeItemWithRetval(pMenu.articleNums[pItemIndex]);
		//menuItemObj.text = strip_ctrl(this.msgReader.PrintMessageInfo(msgHdr, false, itemIdx+1, true));
	}
	*/

	// Add a custom method to the menu to get the message header for the current selected (highlighted)
	// item (or null if none selected); also, similar for the current selected article number (or -1 if
	// none selected)
	pMenu.GetSelectedMsgHdr = function() {
		if (this.selectedItemIdx >= 0 && this.selectedItemIdx < this.articleHdrs.length)
			return this.articleHdrs[this.selectedItemIdx];
		else
			return null;
	};
	pMenu.GetSelectedArticleNum = function() {
		if (this.selectedItemIdx >= 0 && this.selectedItemIdx < this.articleNums.length)
			return this.articleNums[this.selectedItemIdx];
		else
			return -1;
	};

	return retObj;
}

function shortMonthNameToNum(pMonthName)
{
	if (pMonthName == "Jan")
		return 1;
	else if (pMonthName == "Feb")
		return 2;
	else if (pMonthName == "Mar")
		return 3;
	else if (pMonthName == "Apr")
		return 4;
	else if (pMonthName == "May")
		return 5;
	else if (pMonthName == "Jun")
		return 6;
	else if (pMonthName == "Jul")
		return 7;
	else if (pMonthName == "Aug")
		return 8;
	else if (pMonthName == "Sep")
		return 9;
	else if (pMonthName == "Oct")
		return 10
	else if (pMonthName == "Nov")
		return 11;
	else if (pMonthName == "Dec")
		return 12;
	else
		return 1;
}

// Lets the user choose a newsgroup (with a newsgroup menu that's already created),
// then choose an article & potentially read it
//
// Parameters:
//  pNewsgroupMenu: The newsgroup menu object (a DDLightbarMenu)
//  pNewsgroupFormatStr: The format string for displaying newsgroups
//  pListHdrFormatStr: The format string for the list header
//  pShowingOnlySubscribedNewsgroups: Boolean - Whether or not the user is seeing
//                                    only subscribed newsgroups
//
// Return value: An object with the following properties:
//               nextState: The next state that the reader should be in
//               selectNGRetObj: An object returned from the user selecting a newsgroup.
//                               If none selected, this will be null.
//               chosenNewsgroup: The name of the chosen newsgroup (if the user chose one)
function ChooseNewsgroup(pNewsgroupMenu, pNewsgroupFormatStr, pListHdrFormatStr, pShowingOnlySubscribedNewsgroups)
{
	var retObj = {
		nextState: STATE_CHOOSING_NEWSGROUP,
		selectNGRetObj: null,
		chosenNewsgroup: "",
	};

	// Create the key help line
	ChooseNewsgroup.keyHelpLine = CreateNewsgrpListHelpLine(pShowingOnlySubscribedNewsgroups);

	console.line_counter = 0; // To prevent a pause before the message list comes up
	console.clear("N");

	// If the user's terminal supports ANSI, write the key help line at the bottom of the screen
	if (LightbarIsSupported())
	{
		console.attributes = "N";
		console.gotoxy(1, console.screen_rows);
		console.putmsg(ChooseNewsgroup.keyHelpLine);
	}


	var itemSearchStartIdx = 0;
	// User input loop
	var continueOn = true;
	var writeTopHdrs = true;
	var drawMenu = true;
	var writeKeyHelpLine = false;
	var searchText = "";
	while (continueOn)
	{
		if (writeTopHdrs)
		{
			DisplayHdrLines(gNewsgroupListHdrLines, 1);
			if (LightbarIsSupported())
				console.gotoxy(1, gNewsgroupListHdrLines.length+1);
			else
				console.crlf();
			console.center(gSettings.colors.grpListHdrColor + "Choose a newsgroup");
			//console.crlf();
			if (LightbarIsSupported())
				printf(gSettings.colors.grpListHdrColor + pListHdrFormatStr, "Newsgroup", "Description");
			else
			{
				printf(gSettings.colors.grpListHdrColor + pListHdrFormatStr, "#", "Newsgroup", "Description");
				console.crlf();
			}
			writeTopHdrs = false;
		}
		if (writeKeyHelpLine && LightbarIsSupported())
		{
			console.attributes = "N";
			console.gotoxy(1, console.screen_rows);
			console.putmsg(ChooseNewsgroup.keyHelpLine);
		}
		console.gotoxy(pNewsgroupMenu.pos.x, pNewsgroupMenu.pos.y);
		var selectedNG = pNewsgroupMenu.GetVal(drawMenu);
		var lastUserKeyUpper = pNewsgroupMenu.lastUserInput.toUpperCase();
		drawMenu = true;
		console.attributes = "N";
		console.print("\r");
		if (pNewsgroupMenu.multiSelect)
		{
			// Multi-select is enabled in the menu

			// If the GetVal() return value (selectedNG) is null, then that means
			// the user didn't multi-select any newsgroups. In that case, if the
			// user pressed Enter, we'll want to get the newsgroup at the current
			// menu item. Or if the user pressed one of the quit keys, then we'll
			// want to quit.
			if (selectedNG == null)
			{
				//console.print("Last keypress enter? - " + (pNewsgroupMenu.lastUserInput == KEY_ENTER) + "\r\n");
				if (pNewsgroupMenu.lastUserInput == KEY_ENTER)
				{
					//printf("Selected item idx: %d\r\n", pNewsgroupMenu.selectedItemIdx);
					var currentMenuItem = pNewsgroupMenu.GetItem(pNewsgroupMenu.selectedItemIdx);
					if (currentMenuItem != null)
					{
						// Newsgroup name:
						/*
						if (LightbarIsSupported())
							console.gotoxy(1, pNewsgroupMenu.pos.y + pNewsgroupMenu.size.height + 1);
						else
							console.crlf();
						*/
						if (LightbarIsSupported())
							console.gotoxy(1, pNewsgroupMenu.pos.y + pNewsgroupMenu.size.height + 1);
						//console.crlf();
						// TODO
						var selectNGRetObj = nntpClient.SelectNewsgroup(currentMenuItem.retval);
						if (selectNGRetObj.succeeded)
						{
							/*
							console.print("Newsgroup selection succeeded\r\n");
							console.print("Estimated # articles: " + selectNGRetObj.estNumArticles + "\r\n");
							console.print("First article #: " + selectNGRetObj.firstArticleNum + "\r\n");
							console.print("Last article #: " + selectNGRetObj.lastArticleNum + "\r\n");
							*/

							// Next state: Let the user list articles in the newsgroup
							retObj.nextState = STATE_LISTING_ARTICLES_IN_NEWSGROUP;
							retObj.chosenNewsgroup = currentMenuItem.retval;
							retObj.selectNGRetObj = selectNGRetObj;
							continueOn = false;
						}
						else
							console.print("* Newsgroup selection failed!\r\n\x01p");
					}
				}
				else if (pNewsgroupMenu.lastUserInput == CTRL_Q || pNewsgroupMenu.lastUserInput == KEY_ESC || lastUserKeyUpper == "Q")
				{
					//console.crlf();
					//console.print("Aborted\r\n");
					retObj.nextState = STATE_EXIT;
					retObj.chosenNewsgroup = "";
					retObj.selectNGRetObj = null;
					continueOn = false;
				}
				// / key: Search; N: Next (if search text has already been entered
				else if (pNewsgroupMenu.lastUserInput == "/" || lastUserKeyUpper == "N")
				{
					if (pNewsgroupMenu.lastUserInput == "/")
					{
						if (LightbarIsSupported())
						{
							console.gotoxy(1, console.screen_rows);
							console.cleartoeol("N");
							console.gotoxy(1, console.screen_rows);
						}
						else
							console.print("\x01n\r\n");
						var promptText = "Search: ";
						console.print(promptText);
						searchText = console.getstr(console.screen_columns - promptText.length - 1, K_UPPER|K_NOCRLF|K_GETSTR|K_NOSPIN|K_LINE);
					}
					// If the user entered text, then do the search, and if found,
					// found, go to the page and select the item indicated by the
					// search.
					if (searchText.length > 0)
					{
						var foundItem = false;
						var searchTextUpper = searchText.toUpperCase();
						var numItems = pNewsgroupMenu.NumItems();
						for (var i = itemSearchStartIdx; i < numItems; ++i)
						{
							var menuItem = pNewsgroupMenu.GetItem(i);
							if (menuItem != null)
							{
								// In the item, retval is the newsgroup name
								var newsgroupNameUpper = menuItem.retval.toUpperCase();
								var newsgroupDescUpper = pNewsgroupMenu.fullDescs[i].toUpperCase();
								if (newsgroupNameUpper.indexOf(searchTextUpper) > -1 || newsgroupDescUpper.indexOf(searchTextUpper) > -1)
								{
									pNewsgroupMenu.SetSelectedItemIdx(i);
									foundItem = true;
									itemSearchStartIdx = i + 1;
									if (itemSearchStartIdx >= pNewsgroupMenu.NumItems())
										itemSearchStartIdx = 0;
									break;
								}
							}
						}
						drawMenu = foundItem;
						if (!foundItem)
							displayErrorMsgAtBottomScreenRow("No result(s) found", false);
					}
					else
					{
						itemSearchStartIdx = 0;
						drawMenu = false;
					}
					writeKeyHelpLine = true;
				}
				else if (pNewsgroupMenu.lastUserInput == "?")
				{
					console.line_counter = 0; // To prevent a pause before the message list comes up
					console.clear("N");
					DisplayChooseNewsgroupHelp();
					drawMenu = true;
					writeTopHdrs = true;
					writeKeyHelpLine = true;
					console.line_counter = 0; // To prevent a pause before the message list comes up
					console.clear("N");
				}
				// DEL: Clear subscribed newsgroups
				else if (pNewsgroupMenu.lastUserInput == KEY_DEL)
				{
					if (pShowingOnlySubscribedNewsgroups)
					{
						if (LightbarIsSupported())
						{
							console.gotoxy(1, console.screen_rows);
							console.cleartoeol("N");
							console.gotoxy(1, console.screen_rows);
						}
						else
							console.print("\x01n\r\n");
						if (!console.noyes("Clear subscribed newsgroups"))
						{
							gCreateNewsgrpMenuRetObj = CreateNewsgroupMenu_ExitOnError(nntpClient, gUserSettings.subscribedNewsgroups, false);
							gUserSettings.subscribedNewsgroups = [];
							if (SaveUserSettings(gUserSettingsFilename, false))
							{
								console.print("Your subscribed newsgroups have been cleared.\r\n\x01p");
							}
							else
							{
								log(LOG_ERR, format("* %s: Failed to save user's subscribed newsgroups!", PROGRAM_NAME));
								console.print("\x01n\x01y\x01hFailed to save your subscribed newsgroups!\x01n\r\n\x01p");
							}
							gShowOnlySubscribedNewsgroups = false;
							retObj.nextState = STATE_CHOOSING_NEWSGROUP;
							continueOn = false;
							drawMenu = false;
							writeTopHdrs = false;
							writeKeyHelpLine = false;
						}
						else
						{
							drawMenu = true;
							writeTopHdrs = true;
							writeKeyHelpLine = true;
							console.line_counter = 0; // To prevent a pause before the message list comes up
							console.clear("N");
						}
					}
				}
			}
			else if (Array.isArray(selectedNG))
			{
				/*
				console.clear("N");
				console.print("Subscribing to the following newsgroups:\r\n");
				for (var i = 0; i < selectedNG.length; ++i)
				{
					printf("%s\r\n", selectedNG[i]);
				}
				console.pause();
				*/
				// Open the user's settings file and add the newsgroups to the SUBSCRIBED_NEWSGROUPS section
				var settingsFile = new File(gUserSettingsFilename);
				var openMode = (file_exists(gUserSettingsFilename) ? "r+" : "w");
				if (settingsFile.open(openMode))
				{
					/*
					var subscribedNewsgroups = settingsFile.iniGetObject("SUBSCRIBED_NEWSGROUPS");
					if (subscribedNewsgroups != null)
					{
						//retObj.sortOption = userSettingsFile.iniGetValue("BEHAVIOR", "areaChangeSorting", SUB_BOARD_SORT_NONE);
						
					}
					else
					{
					}
					*/
					var settingsSectionName = "SUBSCRIBED_NEWSGROUPS";
					var settingName = "newsgroups_list";
					var subscribedNewsgroupsList = settingsFile.iniGetValue(settingsSectionName, settingName, "");
					for (var i = 0; i < selectedNG.length; ++i)
					{
						if (subscribedNewsgroupsList.indexOf(selectedNG[i]) > -1)
							continue;
						if (subscribedNewsgroupsList.length > 0)
							subscribedNewsgroupsList += ",";
						subscribedNewsgroupsList += selectedNG[i];
					}
					settingsFile.iniSetValue(settingsSectionName, settingName, subscribedNewsgroupsList);
					settingsFile.close();

					// Display to the user that their subscribed newsgroups have been updated
					var statusMsg = "Updated your subscribed newsgroups";
					if (LightbarIsSupported())
					{
						displayStatusMsgAtBottomScreenRow(statusMsg, "GH", false);
						writeTopHdrs = false;
						drawMenu = false;
						writeKeyHelpLine = true;
					}
					else
					{
						printf("\x01n\r\n\x01g\x01h%s\x01n\r\n\x01p", statusMsg);
						console.aborted = false;
						writeTopHdrs = true;
						drawMenu = true;
						writeKeyHelpLine = true;
					}
				}
				else
				{
					var errorMsg = "Failed to update your subscribed newsgroups!";
					if (LightbarIsSupported())
					{
						displayErrorMsgAtBottomScreenRow(errorMsg, false);
						writeTopHdrs = false;
						drawMenu = false;
						writeKeyHelpLine = false;
					}
					else
					{
						printf("\x01n\r\n\x01y\x01h%s\x01n\r\n\x01p", errorMsg);
						console.aborted = false;
						writeTopHdrs = true;
						drawMenu = true;
						writeKeyHelpLine = true;
					}
				}
			}
		}
		else
		{
			// Multi-select is disabled in the menu

			// Quit totally out of Groupie
			if (pNewsgroupMenu.lastUserInput == CTRL_Q)
			{
				retObj.nextState = STATE_EXIT;
				continueOn = false;
			}
			// / key: Search; N: Next (if search text has already been entered
			else if (pNewsgroupMenu.lastUserInput == "/" || lastUserKeyUpper == "N")
			{
				if (pNewsgroupMenu.lastUserInput == "/")
				{
					if (LightbarIsSupported())
					{
						console.gotoxy(1, console.screen_rows);
						console.cleartoeol("N");
						console.gotoxy(1, console.screen_rows);
					}
					else
						console.print("\x01n\r\n");
					var promptText = "Search: ";
					console.print(promptText);
					searchText = console.getstr(console.screen_columns - promptText.length - 1, K_UPPER|K_NOCRLF|K_GETSTR|K_NOSPIN|K_LINE);
				}
				// If the user entered text, then do the search, and if found,
				// found, go to the page and select the item indicated by the
				// search.
				if (searchText.length > 0)
				{
					var foundItem = false;
					var searchTextUpper = searchText.toUpperCase();
					var numItems = pNewsgroupMenu.NumItems();
					for (var i = itemSearchStartIdx; i < numItems; ++i)
					{
						var menuItem = pNewsgroupMenu.GetItem(i);
						if (menuItem != null)
						{
							// In the item, retval is the newsgroup name
							var newsgroupNameUpper = menuItem.retval.toUpperCase();
							var newsgroupDescUpper = pNewsgroupMenu.fullDescs[i].toUpperCase();
							if (newsgroupNameUpper.indexOf(searchTextUpper) > -1 || newsgroupDescUpper.indexOf(searchTextUpper) > -1)
							{
								pNewsgroupMenu.SetSelectedItemIdx(i);
								foundItem = true;
								itemSearchStartIdx = i + 1;
								if (itemSearchStartIdx >= pNewsgroupMenu.NumItems())
									itemSearchStartIdx = 0;
								break;
							}
						}
					}
					drawMenu = foundItem;
					if (!foundItem)
						displayErrorMsgAtBottomScreenRow("No result(s) found", false);
				}
				else
				{
					itemSearchStartIdx = 0;
					drawMenu = false;
				}
				writeKeyHelpLine = true;
			}
			else if (pNewsgroupMenu.lastUserInput == "?")
			{
				console.line_counter = 0; // To prevent a pause before the message list comes up
				console.clear("N");
				DisplayChooseNewsgroupHelp();
				drawMenu = true;
				writeTopHdrs = true;
				writeKeyHelpLine = true;
				console.line_counter = 0; // To prevent a pause before the message list comes up
				console.clear("N");
			}
			else if (selectedNG == null) // The user didn't choose a newsgroup
			{
				//console.crlf();
				//console.print("Aborted\r\n");
				retObj.nextState = STATE_EXIT;
				retObj.chosenNewsgroup = "";
				retObj.selectNGRetObj = null;
				continueOn = false;
			}
			else
			{
				/*
				if (LightbarIsSupported())
					console.gotoxy(1, pNewsgroupMenu.pos.y + pNewsgroupMenu.size.height + 1);
				else
					console.crlf();
				*/
				if (LightbarIsSupported())
					console.gotoxy(1, pNewsgroupMenu.pos.y + pNewsgroupMenu.size.height + 1);
				//console.crlf();
				// TODO
				var selectNGRetObj = nntpClient.SelectNewsgroup(selectedNG);
				if (selectNGRetObj.succeeded)
				{
					/*
					console.print("Newsgroup selection succeeded\r\n");
					console.print("Estimated # articles: " + selectNGRetObj.estNumArticles + "\r\n");
					console.print("First article #: " + selectNGRetObj.firstArticleNum + "\r\n");
					console.print("Last article #: " + selectNGRetObj.lastArticleNum + "\r\n");
					*/

					// Next state: Let the user list articles in the newsgroup
					retObj.nextState = STATE_LISTING_ARTICLES_IN_NEWSGROUP;
					retObj.chosenNewsgroup = selectedNG;
					retObj.selectNGRetObj = selectNGRetObj;
					continueOn = false;
				}
				else
					console.print("* Newsgroup selection failed!\r\n\x01p");
			}
		}
	}

	return retObj;
}

// Lets the user choose an article and read it
function ListArticlesInNewsgroup(pNNTPClient, pNewsgroupName, pSelectNGRetObj)
{
	var retObj = {
		nextState: STATE_CHOOSING_NEWSGROUP
	};

	// Whether or not the code here has changed the next state to a
	// non-default next state to be returned
	var nextStateNonDefaultOnExit = false;

	// Update the user's usage information (gUserUsageInfo)
	/*
	gUserUsageInfo = {
		servers: {
			"myNewsServer": {
				"aNewsgroup": {
					estimatedNumArticles: 0,
					preferredMaxNumArticles: 0
				}
			}
		}
	};
	*/
	// If the newsgroup doesn't exist in the user's usage information, then we'll
	// want to prompt the user for the maximum number of articles to download
	var promptForMaxNumArticles = false;
	if (!gUserUsageInfo.servers.hasOwnProperty(pNNTPClient.connectOptions.hostname))
		gUserUsageInfo.servers[pNNTPClient.connectOptions.hostname] = GetDefaultUserUsageServerInfoObj();
	if (!gUserUsageInfo.servers[pNNTPClient.connectOptions.hostname].newsgroups.hasOwnProperty(pNewsgroupName))
	{
		gUserUsageInfo.servers[pNNTPClient.connectOptions.hostname].newsgroups[pNewsgroupName] = GetDefaultUserUsageNewsgroupInfoObj();
		promptForMaxNumArticles = true;
	}
	gUserUsageInfo.servers[pNNTPClient.connectOptions.hostname].newsgroups[pNewsgroupName].estimatedNumArticles = pSelectNGRetObj.estNumArticles;
	if (!gUserUsageInfo.servers[pNNTPClient.connectOptions.hostname].newsgroups[pNewsgroupName].hasOwnProperty("preferredMaxNumArticles") || gUserUsageInfo.servers[pNNTPClient.connectOptions.hostname].newsgroups[pNewsgroupName].preferredMaxNumArticles == 0)
		promptForMaxNumArticles = true;

	// If we need to prompt the user for the maximum number of articles to download, then
	// do so.
	var userPreferredMaxNumArticles = gUserUsageInfo.servers[pNNTPClient.connectOptions.hostname].newsgroups[pNewsgroupName].preferredMaxNumArticles;
	if (promptForMaxNumArticles)
	{
		console.attributes = "N";
		var promptText = format("\x01c\x01hMaximum number of articles to download\x01g: \x01n");
		if (LightbarIsSupported())
			console.gotoxy(1, console.screen_rows);
		console.print(promptText);
		var numStr = console.getstr(gSettings.defaultMaxNumMsgsFromNewsgroup.toString(), console.screen_columns - console.strlen(promptText) - 1, K_EDIT | K_LINE | K_TRIM | K_NOSPIN | K_NUMBER | K_NOCRLF);
		// If the user aborted (i.e., with Ctrl-C), then return now.  Otherwise,
		// use their inputted preferred maximum number of articles
		if (console.aborted)
			return retObj;
		else
		{
			console.attributes = "N";
			var numMsgs = parseInt(numStr);
			if (!isNaN(numMsgs) && numMsgs > 0)
			{
				userPreferredMaxNumArticles = numMsgs;
				gUserUsageInfo.servers[pNNTPClient.connectOptions.hostname].newsgroups[pNewsgroupName].preferredMaxNumArticles = numMsgs;
			}
		}
	}

	// If the chosen number of articles to download is 0, use the default configured value
	if (userPreferredMaxNumArticles == 0)
		userPreferredMaxNumArticles = gSettings.defaultMaxNumMsgsFromNewsgroup;


	// Create the key help line if it hasn't been created yet
	if (ListArticlesInNewsgroup.keyHelpLine === undefined)
		ListArticlesInNewsgroup.keyHelpLine = CreateArticleListHelpLine();

	console.line_counter = 0; // To prevent a pause before the message list comes up

	// Create the menu of articles for the selected newsgroup
	var createArticleMenuRetObj = CreateArticleMenuForNewsgroup(pNNTPClient, pSelectNGRetObj, userPreferredMaxNumArticles);
	if (createArticleMenuRetObj.errorMsg.length > 0)
	{
		console.clear("N");
		console.crlf();
		console.print("Article retrieval failed: " + createArticleMenuRetObj.errorMsg + "\r\n\x01p");
		return retObj;
	}
	else if (createArticleMenuRetObj.articleMenu.NumItems() == 0)
	{
		console.clear("N");
		console.crlf();
		console.print("There are no articles in the chosen newsgroup\r\n\x01p");
		return retObj;
	}

	console.clear("N");

	// If the user's terminal supports ANSI, write the key help line at the bottom of the screen
	if (LightbarIsSupported())
	{
		console.attributes = "N";
		console.gotoxy(1, console.screen_rows);
		console.putmsg(ListArticlesInNewsgroup.keyHelpLine);
	}

	// User input loop: Let the user choose an article
	var continueOn = true;
	var writeTopHdrs = true;
	var writeKeyHelpLine = false;
	var programmaticArticleNum = null;
	var drawMenu = true;
	var searchText = "";
	var itemSearchStartIdx = 0;
	while (continueOn)
	{
		if (writeTopHdrs)
		{
			console.attributes = "N";
			DisplayHdrLines(gArticleListHdrLines, 1);
			if (LightbarIsSupported())
				console.gotoxy(1, gArticleListHdrLines.length+1);
			else
				console.crlf();
			console.center(gSettings.colors.articleListHdrColor + "Articles for " + pNewsgroupName);
			printf(createArticleMenuRetObj.hdrFormatStr, "#", "From", "To", "Subject", "Date", "Time");
			if (!LightbarIsSupported())
				console.crlf();
		}
		writeTopHdrs = false;
		if (writeKeyHelpLine && LightbarIsSupported())
		{
			console.attributes = "N";
			console.gotoxy(1, console.screen_rows);
			console.putmsg(ListArticlesInNewsgroup.keyHelpLine);
		}
		writeKeyHelpLine = false;
		console.attributes = "N";
		var selectedArticleNum = 0;
		if (programmaticArticleNum != null && typeof(programmaticArticleNum) === "number")
		{
			selectedArticleNum = programmaticArticleNum;
			programmaticArticleNum = null;
		}
		else
			selectedArticleNum = createArticleMenuRetObj.articleMenu.GetVal(drawMenu);
		console.attributes = "N";
		var lastKeypressUpper = createArticleMenuRetObj.articleMenu.lastUserInput.toUpperCase();
		drawMenu = true;
		console.print("\r");
		// Quit totally out of Groupie
		if (createArticleMenuRetObj.articleMenu.lastUserInput == CTRL_Q)
		{
			retObj.nextState = STATE_EXIT;
			continueOn = false;
		}
		// P: Let the user post a message
		else if (lastKeypressUpper == "P")
		{
			gCurrentState = STATE_POSTING;
			// Prompt the user for a subject
			if (LightbarIsSupported())
				console.gotoxy(1, console.screen_rows);
			console.crlf();
			var newsgroupDesc1 = "";
			var newsgroupDesc2 = "";
			var dotIdx = pNewsgroupName.indexOf(".");
			if (dotIdx > -1)
			{
				newsgroupDesc1 = pNewsgroupName.substr(0, dotIdx);
				newsgroupDesc2 = pNewsgroupName.substr(dotIdx+1);
			}
			else
				newsgroupDesc1 = pNewsgroupName
			printf("\x01n" + bbs.text(bbs.text.Posting), newsgroupDesc1, newsgroupDesc2);
			console.attributes = "N";
			console.print("\x01y\x01hSubject: \x01n");
			// TODO: If the user has posted a message already, for some reason the
			// prompt for this getstr() will be populated with the previous subject
			// the user used. clearkeybuffer() doesn't seem to help.
			console.clearkeybuffer();
			var subject = console.getstr(subject, console.screen_columns - 7, K_EDIT|K_LINE|K_TRIM|K_NOSPIN);
			if (subject != "")
			{
				// Let the user edit a message to be posted. If successful, refresh
				// the message headers with the new message.
				if (EditAndPostMsg("All", subject, pNewsgroupName, null, null))
				{
					// Refresh the message headers with the new message
					createArticleMenuRetObj.articleMenu.articleHdrs.push({
						to: "All",
						from: user.handle, // TODO: Setting for this to be handle or real name
						subject: subject,
						date: system.timestr() // This should be a string, not an integer such as returned by time()
					});
					var menuLengthsRetObj = SetLengthsAndPositionsForArticleMenu(createArticleMenuRetObj.articleMenu);
					createArticleMenuRetObj.hdrFormatStr = menuLengthsRetObj.hdrFormatStr;
					writeTopHdrs = true;
				}
			}
			else
				console.mnemonics(bbs.text(bbs.text.Aborted));
			console.clear("N");
			writeTopHdrs = true;
			writeKeyHelpLine = true;
			gCurrentState = STATE_LISTING_ARTICLES_IN_NEWSGROUP;
		}
		// / key: Search; N: Next (if search text has already been entered
		else if (createArticleMenuRetObj.articleMenu.lastUserInput == "/" || lastKeypressUpper == "N")
		{
			// TODO: Finish updating this to work for this menu/section
			if (createArticleMenuRetObj.articleMenu.lastUserInput == "/")
			{
				if (LightbarIsSupported())
				{
					console.gotoxy(1, console.screen_rows);
					console.cleartoeol("N");
					console.gotoxy(1, console.screen_rows);
				}
				else
					console.print("\x01n\r\n");
				var promptText = "Search: ";
				console.print(promptText);
				searchText = console.getstr(console.screen_columns - promptText.length - 1, K_UPPER|K_NOCRLF|K_GETSTR|K_NOSPIN|K_LINE);
			}
			// If the user entered text, then do the search, and if found,
			// found, go to the page and select the item indicated by the
			// search.
			if (searchText.length > 0)
			{
				var foundItem = false;
				var searchTextUpper = searchText.toUpperCase();
				for (var i = itemSearchStartIdx; i < createArticleMenuRetObj.articleMenu.articleHdrs.length; ++i)
				{
					var toUpper = "";
					if (createArticleMenuRetObj.articleMenu.articleHdrs[i].hasOwnProperty("to"))
						toUpper = createArticleMenuRetObj.articleMenu.articleHdrs[i].to.toUpperCase();
					var fromUpper = "";
					if (createArticleMenuRetObj.articleMenu.articleHdrs[i].hasOwnProperty("from"))
						fromUpper = createArticleMenuRetObj.articleMenu.articleHdrs[i].from.toUpperCase();
					var subjUpper = "";
					if (createArticleMenuRetObj.articleMenu.articleHdrs[i].hasOwnProperty("subject"))
						subjUpper = createArticleMenuRetObj.articleMenu.articleHdrs[i].subject.toUpperCase();
					if (toUpper.indexOf(searchTextUpper) > -1 || fromUpper.indexOf(searchTextUpper) > -1 || subjUpper.indexOf(searchTextUpper) > -1)
					{
						createArticleMenuRetObj.articleMenu.SetSelectedItemIdx(i);
						foundItem = true;
						itemSearchStartIdx = i + 1;
						if (itemSearchStartIdx >= createArticleMenuRetObj.articleMenu.NumItems())
							itemSearchStartIdx = 0;
						break;
					}
				}
				drawMenu = foundItem;
				if (!foundItem)
					displayErrorMsgAtBottomScreenRow("No result(s) found", false);
			}
			else
			{
				itemSearchStartIdx = 0;
				drawMenu = false;
			}
			writeKeyHelpLine = true;
		}
		// Numeric digit: The start of an article number from the menu
		else if (createArticleMenuRetObj.articleMenu.lastUserInput.match(/[0-9]/))
		{
			// Default values for screen refresh control variables
			drawMenu = true;
			writeTopHdrs = true;
			writeKeyHelpLine = true;

			// Put the user's input back in the input buffer to
			// be used for getting the rest of the message number.
			console.ungetstr(createArticleMenuRetObj.articleMenu.lastUserInput);
			// Move the cursor to the bottom of the screen and
			// prompt the user for an article number.
			console.gotoxy(1, console.screen_rows);
			console.cleartoeol("N");
			console.gotoxy(1, console.screen_rows);
			var numArticles = createArticleMenuRetObj.articleMenu.NumItems();
			printf("\x01n\x01cArticle # (1-%d)\x01g\x01h: \x01c", numArticles);
			var articleNumFromMenu = console.getnum(numArticles);
			console.attributes = "N";
			// If the user entered a valid article number, then let them read it.
			if (articleNumFromMenu > 0)
			{
				var menuItemIdx = articleNumFromMenu-1;
				var menuItem = createArticleMenuRetObj.articleMenu.GetItem(menuItemIdx);
				if (menuItem != null)
				{
					selectedArticleNum = menuItem.retval;
					createArticleMenuRetObj.articleMenu.selectedItemIdx = menuItemIdx; // Because some logic uses this
					// Get the article and let the user read it
					var articleReadRetObj = GetArticleAndLetUserReadIt(programmaticArticleNum, selectedArticleNum, pNewsgroupName,
					                                                   createArticleMenuRetObj.articleMenu, pNNTPClient);
					programmaticArticleNum = articleReadRetObj.programmaticArticleNum;
					if (articleReadRetObj.nextState == STATE_EXIT)
					{
						retObj.nextState = STATE_EXIT;
						continueOn = false;
					}
					//writeTopHdrs = articleReadRetObj.writeTopHdrs;
					//writeKeyHelpLine = articleReadRetObj.writeKeyHelpLine;
					//continueOn = articleReadRetObj.continueOn;
				}
			}
			if (continueOn)
				console.clear("N");
		}
		// User settings
		else if (createArticleMenuRetObj.articleMenu.lastUserInput == CTRL_U)
		{
			// Store the user's current preferred maximum number of messages for this newsgroup
			// so we can compare it later
			var previousMaxNumMsgs = gUserUsageInfo.servers[pNNTPClient.connectOptions.hostname].newsgroups[pNewsgroupName].preferredMaxNumArticles;
			// Show the user settings dialog & let the user make changes
			var userSettingsMenuTopRow = createArticleMenuRetObj.articleMenu.pos.y;
			var userSettingsRetObj = DoUserSettings(pNNTPClient.connectOptions.hostname, pNewsgroupName,
			                                        userSettingsMenuTopRow, ListArticlesInNewsgroup.keyHelpLine);
			// If the user changed their preferred maximum number of messages in this
			// newsgroup and we may need to load more, then load up to that maximum
			// number of messages
			if (userSettingsRetObj.maxNumArticlesForNewsgroupChanged && createArticleMenuRetObj.articleMenu.NumItems() >= previousMaxNumMsgs)
			{
				gChosenNewsgroupName = pNewsgroupName;
				retObj.nextState = STATE_LISTING_ARTICLES_IN_NEWSGROUP;
				nextStateNonDefaultOnExit = true;
				continueOn = false;
			}
			else if (userSettingsRetObj.needWholeScreenRefresh)
			{
				drawMenu = true;
				writeTopHdrs = true;
				writeKeyHelpLine = true;
				console.clear("N");
			}
			else
			{
				drawMenu = false;
				writeTopHdrs = false;
				writeKeyHelpLine = userSettingsRetObj.refreshKeyHelpLine;
				createArticleMenuRetObj.articleMenu.DrawPartialAbs(userSettingsRetObj.optionBoxTopLeftX, userSettingsRetObj.optionBoxTopLeftY, userSettingsRetObj.optionBoxWidth, userSettingsRetObj.optionBoxHeight);
			}
		}
		// Show help
		else if (createArticleMenuRetObj.articleMenu.lastUserInput == "?")
		{
			console.clear("N");
			DisplayListingArticleHelp(pNewsgroupName);
			if (console.aborted)
				console.aborted = false;
			drawMenu = true;
			writeTopHdrs = true;
			writeKeyHelpLine = true;
			console.clear("N");
		}
		else if (selectedArticleNum == null) // The user quit
			continueOn = false;
		// Selected an article to read
		else if (typeof(selectedArticleNum) === "number")
		{
			// Default values for screen refresh control variables
			drawMenu = true;
			writeTopHdrs = true;
			writeKeyHelpLine = true;

			// Get the article and let the user read it
			var articleReadRetObj = GetArticleAndLetUserReadIt(programmaticArticleNum, selectedArticleNum, pNewsgroupName,
			                                                   createArticleMenuRetObj.articleMenu, pNNTPClient);
			programmaticArticleNum = articleReadRetObj.programmaticArticleNum;
			if (articleReadRetObj.nextState == STATE_EXIT)
			{
				retObj.nextState = STATE_EXIT;
				drawMenu = false;
				writeTopHdrs = false;
				writeKeyHelpLine = false;
				continueOn = false;
			}
			//writeTopHdrs = articleReadRetObj.writeTopHdrs;
			//writeKeyHelpLine = articleReadRetObj.writeKeyHelpLine;
			//continueOn = articleReadRetObj.continueOn;
			if (continueOn)
				console.clear("N");
		}
	}

	// Unless the code here has changed the next state to something
	// non-standard, then set the next state to choosing a newsgroup
	// (unless the next state should be exit)
	if (!nextStateNonDefaultOnExit)
	{
		if (retObj.nextState != STATE_EXIT)
			retObj.nextState = STATE_CHOOSING_NEWSGROUP;
	}

	return retObj;
}

// Helper for ListArticlesInNewsgroup(): Gets an article and lets the user read it
function GetArticleAndLetUserReadIt(pProgrammaticArticleNum, pSelectedArticleNum, pNewsgroupName, pArticleMenu, pNNTPClient)
{
	var retObj = {
		programmaticArticleNum: pProgrammaticArticleNum,
		writeTopHdrs: true,
		writeKeyHelpLine: true,
		continueOn: true,
		nextState: ACTION_NONE
	};

	// Get the article
	var getArticleSucceeded = true;
	var articleBody = "";
	if (pProgrammaticArticleNum != null && typeof(pProgrammaticArticleNum) === "number")
	{
		var articleBodyRetObj = pNNTPClient.GetArticleBody(pProgrammaticArticleNum);
		getArticleSucceeded = articleBodyRetObj.succeeded;
		articleBody = articleBodyRetObj.body;
		retObj.programmaticArticleNum = null;
	}
	else
	{
		var articleBodyRetObj = pNNTPClient.GetArticleBody(pSelectedArticleNum);
		getArticleSucceeded = articleBodyRetObj.succeeded;
		articleBody = articleBodyRetObj.body;
	}
	if (getArticleSucceeded)
	{
		var hdr = pArticleMenu.GetSelectedMsgHdr();
		var readRetObj = ReadMessage(pArticleMenu.selectedItemIdx,
		                             pArticleMenu.NumItems(), hdr,
		                             articleBody, pNewsgroupName);
		gCurrentState = STATE_LISTING_ARTICLES_IN_NEWSGROUP; // Default
		switch (readRetObj.nextAction)
		{
			case ACTION_NONE:
				console.clear("N");
				retObj.writeTopHdrs = true;
				retObj.writeKeyHelpLine = true;
				break;
			case ACTION_GO_TO_FIRST_MSG:
				pArticleMenu.selectedItemIdx = 0;
				pArticleMenu.topItemIdx = 0;
				retObj.programmaticArticleNum = pArticleMenu.GetItem(0).retval;
				break;
			case ACTION_GO_TO_LAST_MSG:
				pArticleMenu.CalcAndSetTopItemIdxToTopOfLastPage();
				pArticleMenu.selectedItemIdx = pArticleMenu.NumItems()-1;
				var item = pArticleMenu.GetItem(pArticleMenu.selectedItemIdx);
				retObj.programmaticArticleNum = item.retval;
				pNNTPClient.Last();
				break;
			case ACTION_GO_TO_PREVIOUS_MSG:
				if (pArticleMenu.selectedItemIdx > 0)
				{
					pArticleMenu.SetSelectedItemIdx(pArticleMenu.selectedItemIdx - 1);
					var item = pArticleMenu.GetItem(pArticleMenu.selectedItemIdx);
					retObj.programmaticArticleNum = item.retval;
				}
				else
					retObj.programmaticArticleNum = pSelectedArticleNum;
				retObj.nextState = ACTION_NONE;
				break;
			case ACTION_GO_TO_NEXT_MSG:
				var highestMsgIdx = pArticleMenu.NumItems() - 1;
				if (pArticleMenu.selectedItemIdx < highestMsgIdx)
				{
					var nextMsgIdx = pArticleMenu.selectedItemIdx + 1;
					pArticleMenu.SetSelectedItemIdx(nextMsgIdx);
					var item = pArticleMenu.GetItem(pArticleMenu.selectedItemIdx);
					retObj.programmaticArticleNum = item.retval;
					if (nextMsgIdx == pArticleMenu.NumItems())
						pNNTPClient.Last();
				}
				else
					retObj.programmaticArticleNum = pSelectedArticleNum;
				retObj.nextState = ACTION_NONE;
				break;
			case ACTION_QUIT_READER:
				retObj.nextState = STATE_EXIT;
				retObj.continueOn = false;
				break;
		}
	}
	else
	{
		if (LightbarIsSupported())
		{
			displayErrorMsgAtBottomScreenRow("Failed to get the article!", false);
			retObj.writeTopHdrs = false;
			retObj.writeKeyHelpLine = true;
		}
		else
			console.print("Failed to get the article!\r\n\x01p");
	}

	return retObj;
}

// Lets the user read a message. This calls the appropriate version for
// scrolling or traditional as appropriate.
function ReadMessage(pMsgIdx, pNumMsgs, pHdr, pBody, pNewsgroupName)
{
	var retObj = null;
	if (LightbarIsSupported())
		retObj = ReadMessage_Scrollable(pMsgIdx, pNumMsgs, pHdr, pBody, pNewsgroupName);
	else
		retObj = ReadMessage_Traditional(pMsgIdx, pNumMsgs, pHdr, pBody, pNewsgroupName);
	return retObj;
}

// Lets the user read a message with a scrolling (ANSI) user interface.
//
// Parameters:
//  pMsgIdx: The message index. Mainly for range checking for next/previous msg navigation.
//  pNumMsgs: The total number of messages. Mainly for range checking for next/previous msg navigation.
//  pHdr: The article header object
//  pBody: The article body
//  pNewsgroupName: The newsgroup name
//
// Return value: An obhect with the following properties:
//               nextAction: The ID of the next action the reader should take
function ReadMessage_Scrollable(pMsgIdx, pNumMsgs, pHdr, pBody, pNewsgroupName)
{
	gCurrentState = STATE_LISTING_READING_ARTICLE;

	var retObj = {
		nextAction: ACTION_NONE
	};

	if (ReadMessage_Scrollable.keyHelpLine === undefined)
		ReadMessage_Scrollable.keyHelpLine = CreateScrollableReaderHelpLine();

	console.line_counter = 0; // To prevent a pause before the message list comes up
	console.clear("N");
	// If the user's terminal supports ANSI, write the key help line at the bottom of the screen
	if (LightbarIsSupported())
	{
		console.attributes = "N";
		console.gotoxy(1, console.screen_rows);
		console.putmsg(ReadMessage_Scrollable.keyHelpLine);
		console.gotoxy(1, 1);
	}
	// Display the message header & get the number of lines displayed
	var numHdrLines = GetAndDisplayMsgHdr(pHdr);
	var frameStartY = (numHdrLines > 0 ? numHdrLines + 1 : 1);
	var frameHeight = console.screen_rows - frameStartY;
	var frameAndScrollbar = CreateFrameWithScrollbar(1, frameStartY, console.screen_columns, frameHeight);
	// If the message has ANSI content, then use a Graphic object to help make
	// the message look good.  Also, remove any ANSI clear screen codes from the
	// message text.
	var msgAreaWidth = console.screen_columns;
	var messageText = pBody;
	var msgHasANSICodes = messageText.indexOf("\x1b[") >= 0;
	if (msgHasANSICodes)
	{
		messageText = messageText.replace(/\u001b\[[012]J/gi, "");
		//var graphic = new Graphic(msgAreaWidth, frameHeight-1);
		// To help ensure ANSI messages look good, it seems the Graphic object should have
		// its with later set to 1 less than the width used to create it.
		var graphicWidth = (msgAreaWidth < console.screen_columns ? msgAreaWidth+1 : console.screen_columns);
		var graphic = new Graphic(graphicWidth, frameHeight-1);
		graphic.auto_extend = true;
		graphic.ANSI = ansiterm.expand_ctrl_a(messageText);
		//graphic.normalize();
		graphic.width = graphicWidth - 1;
		//messageText = graphic.MSG.split('\n');
		messageText = graphic.MSG;
	}
	// Add the message text to the frame, and draw it
	frameAndScrollbar.frame.putmsg(messageText, "\x01n");
	frameAndScrollbar.frame.scrollTo(0, 0);
	frameAndScrollbar.frame.invalidate();
	frameAndScrollbar.scrollbar.cycle();
	frameAndScrollbar.frame.cycle();
	frameAndScrollbar.frame.draw();
	// User input loop
	var displayMsgHdr = false;
	var writeKeyHelpLine = false;
	var continueOn = true;
	while (continueOn && bbs.online && !js.terminated)
	{
		if (displayMsgHdr)
		{
			if (LightbarIsSupported())
				console.gotoxy(1, 1);
			GetAndDisplayMsgHdr(pHdr);
			displayMsgHdr = false;
		}
		if (writeKeyHelpLine && LightbarIsSupported())
		{
			console.gotoxy(1, console.screen_rows);
			console.putmsg(ReadMessage_Scrollable.keyHelpLine);
			writeKeyHelpLine = false;
		}
		var scrollRetObj = ScrollFrame(frameAndScrollbar.frame, frameAndScrollbar.scrollbar, 0, "\x01n", true, 1, frameStartY);
		if (scrollRetObj.lastKeypress == "M") // Message list
		{
			retObj.nextAction = ACTION_SHOW_MSG_LIST;
			continueOn = false;
		}
		else if (scrollRetObj.lastKeypress == "Q" || scrollRetObj.lastKeypress == KEY_ESC)
		{
			retObj.nextAction = ACTION_SHOW_MSG_LIST;
			continueOn = false;
		}
		else if (scrollRetObj.lastKeypress == CTRL_Q)
		{
			retObj.nextAction = ACTION_QUIT_READER;
			continueOn = false;
		}
		else if (scrollRetObj.lastKeypress == KEY_LEFT)
		{
			// Go to the previous message
			if (pMsgIdx > 0)
			{
				retObj.nextAction = ACTION_GO_TO_PREVIOUS_MSG;
				continueOn = false;
			}
		}
		else if (scrollRetObj.lastKeypress == KEY_RIGHT)
		{
			// Go to the next message
			if (pMsgIdx < pNumMsgs-1)
			{
				retObj.nextAction = ACTION_GO_TO_NEXT_MSG;
				continueOn = false;
			}
		}
		else if (scrollRetObj.lastKeypress == "F")
		{
			// Go to the first message
			retObj.nextAction = ACTION_GO_TO_FIRST_MSG;
			continueOn = false;
		}
		else if (scrollRetObj.lastKeypress == "L")
		{
			// Go to the last message
			retObj.nextAction = ACTION_GO_TO_LAST_MSG;
			continueOn = false;
		}
		else if (scrollRetObj.lastKeypress == "R")
		{
			// Prompt the user to edit the 'to' name and subject
			var to = (pHdr.hasOwnProperty("from") ? pHdr.from : "All");
			var subject = (pHdr.hasOwnProperty("subject") ? pHdr.subject : "(No Subject)");
			if (LightbarIsSupported())
				console.gotoxy(1, console.screen_rows);
			console.crlf();
			console.print("\x01n\x01y\x01hPost to: \x01n");
			to = console.getstr(to, console.screen_columns - 9, K_EDIT | K_LINE | K_TRIM | K_NOSPIN);
			console.print("\x01n\x01y\x01hSubject: \x01n");
			subject = console.getstr(subject, console.screen_columns - 7, K_EDIT | K_LINE | K_TRIM | K_NOSPIN);
			EditAndPostMsg(to, subject, pNewsgroupName, pHdr, pBody);
			console.clear("N");
			displayMsgHdr = true;
			writeKeyHelpLine = true;
		}
		else if (scrollRetObj.lastKeypress == "H")
		{
			// View headers (sysop only)
			if (user.is_sysop)
			{
				ViewMsgHdrs_Scrolling(pHdr, 1, frameStartY, console.screen_columns, frameHeight);
			}
			displayMsgHdr = false;
			writeKeyHelpLine = false;
		}
		else if (scrollRetObj.lastKeypress == "O") // Forward message
		{
			if (LightbarIsSupported())
				console.gotoxy(1, console.screen_rows);
			console.crlf();
			var fwdRetStr = ForwardMessage(pHdr, pBody, pNewsgroupName);
			if (fwdRetStr.length > 0)
			{
				console.attributes = "N";
				console.print(gSettings.colors.errorMsg);
				console.print(fwdRetStr);
				console.attributes = "N";
				console.crlf();
				console.pause();
			}
			console.clear("N");

			displayMsgHdr = true;
			writeKeyHelpLine = true;
		}
		else if (scrollRetObj.lastKeypress == "X") // Show message hex dump
		{
			//console.print("\r\nThis feature is not yet implemented    \r\n\x01p"); // Temporary
			if (user.is_sysop)
			{
				ShowMsgHexDump_Scrollable(frameStartY, frameHeight, pBody);
			}
		}
		else if (scrollRetObj.lastKeypress == CTRL_S) // Save message to file on the BBS machine
		{
			if (user.is_sysop)
			{
				console.gotoxy(1, console.screen_rows);
				console.cleartoeol("N");
				console.gotoxy(1, console.screen_rows);
				console.print("\x01cFile\x01g\x01h: \x01n");
				var filename = console.getstr("", console.screen_columns - 7, K_LINE|K_NOSPIN|K_NOCRLF);
				if (filename != "")
				{
					if (SaveMessageToFile(pHdr, pBody, pNewsgroupName, filename))
					{
						console.gotoxy(1, console.screen_rows);
						console.cleartoeol("N");
						console.gotoxy(1, console.screen_rows);
						console.print("\x01cThe message has been saved.\x01n");
						mswait(ERROR_PAUSE_WAIT_MS);
					}
					else
						displayErrorMsgAtBottomScreenRow("Failed to save the file", false);
				}
				else
					displayErrorMsgAtBottomScreenRow("Message not saved", false);
				writeKeyHelpLine = true;
			}
		}
		else if (scrollRetObj.lastKeypress == CTRL_X) // Save message hex dump to file on the BBS machine
		{
			if (user.is_sysop)
			{
				console.gotoxy(1, console.screen_rows);
				console.cleartoeol("N");
				console.gotoxy(1, console.screen_rows);
				console.print("\x01cFile\x01g\x01h: \x01n");
				var filename = console.getstr("", console.screen_columns - 7, K_LINE|K_NOSPIN|K_NOCRLF);
				if (filename != "")
				{
					var saveHexRetObj = SaveMsgHexDumpToFile(pHdr, pBody, pNewsgroupName, filename);
					if (saveHexRetObj.saveSucceeded)
					{
						console.gotoxy(1, console.screen_rows);
						console.cleartoeol("N");
						console.gotoxy(1, console.screen_rows);
						console.print("\x01cThe hex dump has been saved.\x01n");
						mswait(ERROR_PAUSE_WAIT_MS);
					}
					else if (saveHexRetObj.errorMsg != "")
						displayErrorMsgAtBottomScreenRow(saveHexRetObj.errorMsg, false);
					else
						displayErrorMsgAtBottomScreenRow("Failed to save the hex dump", false);
				}
				else
					displayErrorMsgAtBottomScreenRow("Message not saved", false);
				writeKeyHelpLine = true;
			}
		}
		else if (scrollRetObj.lastKeypress == "?")
		{
			// Reader help
			displayReaderHelp(pNewsgroupName, true);
			console.clear("N");
			displayMsgHdr = true;
			writeKeyHelpLine = true;
		}
	}

	return retObj;
}
// Helper for ReadMessage_Scrollable: Shows a message hex dump in a scrolling window
function ShowMsgHexDump_Scrollable(pFrameStartY, pFrameHeight, pMsgBody)
{
	if (!user.is_sysop)
		return;

	var frameAndScrollbar = CreateFrameWithScrollbar(1, pFrameStartY, console.screen_columns, pFrameHeight);
	// Add the message hex dump to the frame, and draw it
	var hexLines = hexdump.generate(undefined, pMsgBody, /* ASCII: */true, /* offsets: */true);
	var hexText = "";
	for (var i = 0; i < hexLines.length; ++i)
	{
		hexText += hexLines[i];
		if (i < hexLines.length-1)
			hexText += "\r\n";
	}
	frameAndScrollbar.frame.putmsg(hexText, "\x01n");
	frameAndScrollbar.frame.scrollTo(0, 0);
	frameAndScrollbar.frame.invalidate();
	frameAndScrollbar.scrollbar.cycle();
	frameAndScrollbar.frame.cycle();
	frameAndScrollbar.frame.draw();
	// Display it
	//ScrollFrame(frameAndScrollbar.frame, frameAndScrollbar.scrollbar, 0, 
	var continueOn = true;
	while (continueOn && bbs.online && !js.terminated)
	{
		var scrollRetObj = ScrollFrame(frameAndScrollbar.frame, frameAndScrollbar.scrollbar, 0, "\x01n", true, 1, pFrameStartY);
		var lastKeyIsQuit = (scrollRetObj.lastKeypress == KEY_ENTER || scrollRetObj.lastKeypress == "Q" || scrollRetObj.lastKeypress == KEY_ESC);
		if (lastKeyIsQuit || bbs.online == 0 || js.terminated)
			continueOn = false;
	}
}

// Lets the user read a message with the traditional user interface.
//
// Parameters:
//  pMsgIdx: The message index. Mainly for range checking for next/previous msg navigation.
//  pNumMsgs: The total number of messages. Mainly for range checking for next/previous msg navigation.
//  pHdr: The article header object
//  pBody: The article body
function ReadMessage_Traditional(pMsgIdx, pNumMsgs, pHdr, pBody, pNewsgroupName)
{
	gCurrentState = STATE_LISTING_READING_ARTICLE;

	var retObj = {
		nextAction: ACTION_NONE
	};

	// Generate the key help text etc.
	var allowedKeys = "FLROQ" + KEY_LEFT + KEY_RIGHT + KEY_ENTER + KEY_ESC + CTRL_Q;
	var keyHelpText = "\x01n\x01c\x01h<-\x01n\x01g/\x01c\x01h->\x01n\x01g, ";
	keyHelpText += "\x01c(\x01hF\x01n\x01c)\x01girst, \x01c(\x01hL\x01n\x01c)\x01gast, \x01c(\x01hR\x01n\x01c)\x01geply";
	if (user.is_sysop)
	{
		allowedKeys += "HX" + CTRL_S + CTRL_X;
		keyHelpText += ", \x01c(\x01hH\x01n\x01c)\x01gdrs, \x01c(\x01hX\x01n\x01c)\x01g hex, \x01c\x01hCtrl-X";
	}
	keyHelpText += "\x01g, \x01n\x01c(\x01hQ\x01n\x01c)\x01guit, \x01h\x01c?\x01n\x01g: ";

	// Reading the message & input loop
	var msgLines = pBody.split("\r\n");
	var writeMessage = true;
	var continueOn = true;
	while (continueOn && bbs.online && !js.terminated)
	{
		console.clear("N");
		GetAndDisplayMsgHdr(pHdr);
		console.crlf();
		// Show the message
		if (writeMessage)
		{
			//console.print(pBody);
			for (var i = 0; i < msgLines.length; ++i)
				console.print(msgLines[i] + "\r\n");
			// Write the prompt text & get a command from the user
			console.print(keyHelpText);
			writeMessage = false;
		}
		var userInput = console.getkeys(allowedKeys, -1, K_UPPER|K_NOECHO|K_NOSPIN|K_NOCRLF);
		// Quit out of reading the message
		if (userInput == "Q" || userInput == KEY_ESC || !bbs.online || js.terminated)
		{
			retObj.nextAction = ACTION_SHOW_MSG_LIST;
			continueOn = false;
		}
		// Totally quit out of Groupie
		else if (userInput == CTRL_Q)
		{
			retObj.nextAction = ACTION_QUIT_READER;
			continueOn = false;
		}
		// Previous message
		else if (userInput == KEY_LEFT)
		{
			// Go to the previous message
			if (pMsgIdx > 0)
			{
				retObj.nextAction = ACTION_GO_TO_PREVIOUS_MSG;
				continueOn = false;
			}
		}
		// Next message
		else if (userInput == KEY_RIGHT)
		{
			// Go to the next message
			if (pMsgIdx < pNumMsgs-1)
			{
				retObj.nextAction = ACTION_GO_TO_NEXT_MSG;
				continueOn = false;
			}
		}
		// First message
		else if (userInput == "F")
		{
			// Go to the first message
			retObj.nextAction = ACTION_GO_TO_FIRST_MSG;
			continueOn = false;
		}
		// Last message
		else if (userInput == "L")
		{
			// Go to the last message
			retObj.nextAction = ACTION_GO_TO_LAST_MSG;
			continueOn = false;
		}
		// Reply
		else if (userInput == "R")
		{
			// Prompt the user to edit the 'to' name and subject
			var to = (pHdr.hasOwnProperty("from") ? pHdr.from : "All");
			var subject = (pHdr.hasOwnProperty("subject") ? pHdr.subject : "(No Subject)");
			console.attributes = "N";
			console.crlf();
			console.print("\x01n\x01y\x01hPost to: \x01n");
			to = console.getstr(to, console.screen_columns - 9, K_EDIT | K_LINE | K_TRIM | K_NOSPIN);
			console.print("\x01n\x01y\x01hSubject: \x01n");
			subject = console.getstr(subject, console.screen_columns - 7, K_EDIT | K_LINE | K_TRIM | K_NOSPIN);
			EditAndPostMsg(to, subject, pNewsgroupName, pHdr, pBody);
			writeMessage = true;
			console.clear("N");
		}
		// Show message headers (sysop only)
		else if (userInput == "H")
		{
			if (user.is_sysop)
			{
				console.print("\x01c\x01hMessage Headers\r\n");
				console.print("\x01g---------------\r\n");
				console.attributes = "NW";
				for (var prop in pHdr)
				{
					var propCapitalized = prop[0].toUpperCase() + prop.substr(1);
					printf("%s: %s\r\n", propCapitalized, pHdr[prop]);
				}
				console.pause();
				writeMessage = true;
			}
		}
		// Forward message
		else if (userInput == "O")
		{
			console.crlf();
			var fwdRetStr = ForwardMessage(pHdr, pBody, pNewsgroupName);
			if (fwdRetStr.length > 0)
			{
				console.attributes = "N";
				console.print(gSettings.colors.errorMsg);
				console.print(fwdRetStr);
				console.attributes = "N";
				console.crlf();
				console.pause();
			}
			writeMessage = true;
		}
		// Show hex dump (sysop only)
		else if (userInput == "X")
		{
			if (user.is_sysop)
			{
				console.attributes = "N";
				console.crlf();
				var hexLines = hexdump.generate(undefined, pBody, /* ASCII: */true, /* offsets: */true);
				for (var i = 0; i < hexLines.length; ++i)
					printf("%s\r\n", hexLines[i]);
				console.pause();
				writeMessage = true;
			}
		}
		// Save message on the BBS machine (sysop only)
		else if (userInput == CTRL_S)
		{
			if (user.is_sysop)
			{
				console.attributes = "N";
				console.crlf();
				console.print("- Save message to file on the BBS machine\r\n");
				console.print("\x01cFile\x01g\x01h: \x01n");
				var filename = console.getstr("", console.screen_columns - 7, K_LINE|K_NOSPIN|K_NOCRLF);
				if (filename != "")
				{
					if (SaveMessageToFile(pHdr, pBody, pNewsgroupName, filename))
					{
						console.crlf();
						console.print("\x01cThe message has been saved.\x01n\r\n");
					}
					else
						console.print("\x01n" + gSettings.colors.errorMsg + gSettings.colors.errorMsg + "\x01n\r\n");
				}
				else
					console.print("\x01n" + gSettings.colors.errorMsg + "Message not saved\x01n\r\n");
				console.pause();
				writeMessage = true;
			}
		}
		// Dump hex (sysop only)
		else if (userInput == CTRL_X)
		{
			if (user.is_sysop)
			{
				console.attributes = "N";
				console.crlf();
				console.print("- Dump message hex to file on the BBS machine\r\n");
				console.print("\x01cFile\x01g\x01h: \x01n");
				var filename = console.getstr("", console.screen_columns - 7, K_LINE|K_NOSPIN|K_NOCRLF);
				if (filename != "")
				{
					var saveHexRetObj = SaveMsgHexDumpToFile(pHdr, pBody, pNewsgroupName, filename);
					if (saveHexRetObj.saveSucceeded)
					{
						console.crlf();
						console.print("\x01cThe hex dump has been saved.\x01n\r\n");
						mswait(ERROR_PAUSE_WAIT_MS);
					}
					else if (saveHexRetObj.errorMsg != "")
						console.print("\x01n" + gSettings.colors.errorMsg + saveHexRetObj.errorMsg + "\x01n\r\n");
					else
						console.print("\x01n" + gSettings.colors.errorMsg + "Failed to save the hex dump\x01n\r\n");
				}
				else
					console.print("\x01n" + gSettings.colors.errorMsg + "Hex dump not saved\x01n\r\n");
				console.pause();
				writeMessage = true;
			}
		}
		// Message list
		else if (userInput == "M")
		{
			retObj.nextAction = ACTION_SHOW_MSG_LIST;
			continueOn = false;
		}
		// Show help
		else if (userInput == "?")
		{
			displayReaderHelp(pNewsgroupName, false);
			console.clear("N");
			writeMessage = true;
		}
	}

	return retObj;
}

function ViewMsgHdrs_Scrolling(pMsgHdr, pFrameStartX, pFrameStartY, pFrameWidth, pFrameHeight)
{
	var hdrsFrameAndScrollbar = CreateFrameWithScrollbar(pFrameStartX, pFrameStartY, pFrameWidth, pFrameHeight);
	// Add the header information to the frame, and draw it
	// Note: The \x01w seems to be necessary to make the header field lines visible in the frame
	var hdrsInfoText = "\x01c\x01hMessage Headers\r\n\x01g---------------\x01n\x01w\r\n";
	for (var prop in pMsgHdr)
	{
		var propCapitalized = prop[0].toUpperCase() + prop.substr(1);
		hdrsInfoText += propCapitalized + ": " + pMsgHdr[prop] + "\r\n";
	}
	hdrsFrameAndScrollbar.frame.putmsg(hdrsInfoText, "\x01n");
	hdrsFrameAndScrollbar.frame.scrollTo(0, 0);
	hdrsFrameAndScrollbar.frame.invalidate();
	hdrsFrameAndScrollbar.scrollbar.cycle();
	hdrsFrameAndScrollbar.frame.cycle();
	hdrsFrameAndScrollbar.frame.draw();

	var continueOn = true;
	while (continueOn && bbs.online && !js.terminated)
	{
		var scrollRetObj = ScrollFrame(hdrsFrameAndScrollbar.frame, hdrsFrameAndScrollbar.scrollbar, 0, "\x01n", true, 1, pFrameStartY);
		var lastKeyIsQuit = (scrollRetObj.lastKeypress == KEY_ENTER || scrollRetObj.lastKeypress == "Q" || scrollRetObj.lastKeypress == KEY_ESC);
		if (lastKeyIsQuit || bbs.online == 0 || js.terminated)
			continueOn = false;
	}
}

function ViewMsgHdrs_Traditional(pMsgHdr)
{
	// TODO
}

// Returns the name of the msghdr file in the
// sbbs/text/menu directory. If the user's terminal supports ANSI, this first
// checks to see if an .ans version exists.  Otherwise, checks to see if an
// .asc version exists.  If neither are found, this function will return an
// empty string.
function GetMsgHdrFilenameFull()
{
	// If the user's terminal supports ANSI and msghdr.ans exists
	// in the text/menu directory, then use that one.  Otherwise,
	// if msghdr.asc exists, then use that one.
	var ansiFileName = "menu/msghdr.ans";
	var asciiFileName = "menu/msghdr.asc";
	var msgHdrFilename = "";
	if (LightbarIsSupported() && file_exists(system.text_dir + ansiFileName))
		msgHdrFilename = system.text_dir + ansiFileName;
	else if (file_exists(system.text_dir + asciiFileName))
		msgHdrFilename = system.text_dir + asciiFileName;
	return msgHdrFilename;
}

// Loads and displays the Synchronet message header file
//
// Parameters:
//  pArticleHdr: The article header object
//
// Return value: The number of lines displayed
function GetAndDisplayMsgHdr(pArticleHdr)
{
	var numLinesDisplayed = 0;
	var hdrLines = [];
	var msgHdrFilename = GetMsgHdrFilenameFull();
	if (msgHdrFilename.length > 0)
	{
		var msgHdrFile = new File(msgHdrFilename);
		if (msgHdrFile.open("r"))
		{
			var fileLine = null; // To store a line read from the file
			while (!msgHdrFile.eof)
			{
				// Read the next line from the header file
				fileLine = msgHdrFile.readln(2048);

				// fileLine should be a string, but I've seen some cases
				// where it isn't, so check its type.
				if (typeof(fileLine) != "string")
					continue;

				hdrLines.push(fileLine);
			}
			msgHdrFile.close();
		}
	}
	else
	{
		// There is no msghdr file on the system
		if (GetAndDisplayMsgHdr.hdrLines === undefined)
			GetAndDisplayMsgHdr.hdrLines = GetDefaultDisplayHdrLines();
		hdrLines = GetAndDisplayMsgHdr.hdrLines;
	}

	for (var i = 0; i < hdrLines.length; ++i)
	{
		// Since we're displaying the message information ouside of Synchronet's
		// message read prompt, this script now has to parse & replace some of
		// the @-codes in the message header line, since Synchronet doesn't know
		// that the user is reading a message.
		var hdrLine = ParseMsgAtCodes(hdrLines[i], pArticleHdr, null, pArticleHdr.date, false, true);
		//var hdrLine = bbs.expand_atcodes(hdrLines[i], MakeSyncCompatibleMsgHdr(pArticleHdr));
		hdrLine = hdrLine.replace(/[\r\n]+$/, "");
		hdrLine = hdrLine.replace(/\r\n$/, "").replace(/\r$/, "").replace(/\r$/, "");
		hdrLine = skipsp(truncsp(hdrLine));
		if (console.strlen(hdrLine) > 0 && !/^\s+$/.test(hdrLine))
		{
			//console.putmsg(hdrLine);
			console.print(hdrLine);
			//console.print(hdrLine.substr(0, console.screen_columns - 8) + ": " + hdrLine.length.toString());
			//console.print(hdrLine.substr(0, console.screen_columns - 1));
			// Note: substrWithAttrCodes() is defined in dd_lightbar_menu.js
			//substrWithAttrCodes(pStr, pStartIdx, pLen)
			console.crlf();
			++numLinesDisplayed;
		}
	}

	return numLinesDisplayed;
}

function GetDefaultDisplayHdrLines()
{
	var hdrLines = [
		" Msg. #: @MSG_NUM@",
		"   From: @MSG_FROM@",
		"     To: @MSG_TO@",
		"Subject: @MSG_SUBJECT@",
		"   Date: @MSG_DATE@"
	];
	hdrLines[0] = "\x01n" + gSettings.colors.msgHdrMsgNumColor + hdrLines[0];
	hdrLines[1] = "\x01n" + gSettings.colors.msgHdrFromColor + hdrLines[1];
	hdrLines[2] = "\x01n" + gSettings.colors.msgHdrToColor + hdrLines[2];
	hdrLines[3] = "\x01n" + gSettings.colors.msgHdrSubjColor + hdrLines[3];
	hdrLines[4] = "\x01n" + gSettings.colors.msgHdrDateColor + hdrLines[4];
	hdrLines.push("\x01n\x01c\x01h");
	for (var i = 0; i < console.screen_columns; ++i)
		hdrLines[5] += CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE;
	hdrLines[5] += "\x01n";
	return hdrLines;
}

// Generates an internal enhanced reader header line for the 'To' user.
//
// Parameters:
//  pColors: A JSON object containing the color settings read from the
//           theme configuration file.  This function will use the
//           'msgHdrToColor' or 'msgHdrToUserColor' property, depending
//           on the pToReadingUser property.
//  pToReadingUser: Boolean - Whether or not to generate the line with
//                  the color/attribute for the reading user
//
// Return value: A string containing the internal enhanced reader header
//               line specifying the 'to' user
function genEnhHdrToUserLine(pColors, pToReadingUser)
{
	var toHdrLine = "\x01n\x01h\x01k" + CP437_BOX_DRAWINGS_LIGHT_VERTICAL + CP437_LIGHT_SHADE + CP437_MEDIUM_SHADE + CP437_DARK_SHADE
	              + "\x01gT\x01n\x01go  \x01h\x01c: " +
	              (pToReadingUser ? pColors.msgHdrToUserColor : pColors.msgHdrToColor) +
	              "@MSG_TO-L";
	var numChars = console.screen_columns - 21;
	for (var i = 0; i < numChars; ++i)
		toHdrLine += "#";
	toHdrLine += "@\x01k" + CP437_BOX_DRAWINGS_LIGHT_VERTICAL;
	return toHdrLine;
}

// Looks for complex @-code strings in a text line and
// parses & replaces them appropriately with the appropriate info from the message header
// object and/or message base object.  This is more complex than simple text substitution
// because message subject @-codes could include something like @MSG_SUBJECT-L######@
// or @MSG_SUBJECT-R######@ or @MSG_SUBJECT-L20@ or @MSG_SUBJECT-R20@.
//
// Parameters:
//  pTextLine: The line of text to search
//  pMsgHdr: The message header object
//  pDisplayMsgNum: The message number, if different from the number in the header
//                  object (i.e., if doing a message search).  This parameter can
//                  be null, in which case the number in the header object will be
//                  used.
//  pDateTimeStr: Optional formatted string containing the date & time.  If this is
//                not provided, the current date & time will be used.
//  pBBSLocalTimeZone: Optional boolean - Whether or not pDateTimeStr is in the BBS's
//                     local time zone.  Defaults to false.
//  pAllowCLS: Optional boolean - Whether or not to allow the @CLS@ code.
//             Defaults to false.
//
// Return value: A string with the complex @-codes substituted in the line with the
// appropriate message header information.
function ParseMsgAtCodes(pTextLine, pMsgHdr, pDisplayMsgNum, pDateTimeStr,
                         pBBSLocalTimeZone, pAllowCLS)
{
	var textLine = pTextLine;
	var dateTimeStr = "";
	var useBBSLocalTimeZone = false;
	if (typeof(pDateTimeStr) == "string")
	{
		dateTimeStr = pDateTimeStr;
		if (typeof(pBBSLocalTimeZone) == "boolean")
			useBBSLocalTimeZone = pBBSLocalTimeZone;
	}
	else
		dateTimeStr = strftime("%Y-%m-%d %H:%M:%S", pMsgHdr.when_written_date);
	// Message attribute strings
	// TODO: Not sure if these are needed
	/*
	var allMsgAttrStr = makeAllMsgAttrStr(pMsgHdr);
	var mainMsgAttrStr = makeMainMsgAttrStr(pMsgHdr.attr);
	var auxMsgAttrStr = makeAuxMsgAttrStr(pMsgHdr.auxattr);
	var netMsgAttrStr = makeNetMsgAttrStr(pMsgHdr.netattr);
	*/
	// An array of @-code strings without the trailing @, to be used for constructing
	// regular expressions to look for versions with justification & length specifiers.
	// The order of the strings in this array matters.  For instance, @MSG_NUM_AND_TOTAL
	// needs to come before @MSG_NUM so that it gets processed properly, since they
	// both start out with the same text.
	var atCodeStrBases = ["@MSG_FROM", "@MSG_FROM_AND_FROM_NET", "@MSG_FROM_EXT", "@MSG_TO", "@MSG_TO_NAME", "@MSG_TO_EXT",
	                      "@MSG_SUBJECT", "@MSG_DATE", "@MSG_ATTR", "@MSG_AUXATTR", "@MSG_NETATTR",
	                      "@MSG_ALLATTR", "@MSG_NUM_AND_TOTAL", "@MSG_NUM", "@MSG_ID",
	                      "@MSG_REPLY_ID", "@MSG_TIMEZONE", "@GRP", "@GRPL", "@SUB", "@SUBL",
						  "@BBS", "@BOARDNAME", "@ALIAS", "@SYSOP", "@CONF", "@DATE", "@DIR", "@DIRL",
						  "@USER", "@NAME"];
	// For each @-code, look for a version with justification & length specified and
	// replace accordingly.
	for (var atCodeStrBaseIdx in atCodeStrBases)
	{
		var atCodeStrBase = atCodeStrBases[atCodeStrBaseIdx];
		// Synchronet @-codes can specify justification with -L or -R and width using a series
		// of non-numeric non-space characters (i.e., @MSG_SUBJECT-L#####@ or @MSG_SUBJECT-R######@).
		// So look for these types of format specifiers for the message subject and if found,
		// parse and replace appropriately.
		var multipleCharLenRegex = new RegExp(atCodeStrBase + "-[LR][^0-9 ]+@", "gi");
		var atCodeMatchArray = textLine.match(multipleCharLenRegex);
		if ((atCodeMatchArray != null) && (atCodeMatchArray.length > 0))
		{
			for (var idx in atCodeMatchArray)
			{
				// In this case, the subject length is the length of the whole format specifier.
				var substLen = atCodeMatchArray[idx].length;
				textLine = this.ReplaceMsgAtCodeFormatStr(pMsgHdr, pDisplayMsgNum, textLine, substLen,
				                                     atCodeMatchArray[idx], pDateTimeStr, useBBSLocalTimeZone,
				                                     //mainMsgAttrStr, auxMsgAttrStr, netMsgAttrStr, allMsgAttrStr);
				                                     "", "", "", "");
			}
		}
		// Now, look for subject formatters with the length specified (i.e., @MSG_SUBJECT-L20@ or @MSG_SUBJECT-R20@)
		var numericLenSearchRegex = new RegExp(atCodeStrBase + "-[LR][0-9]+@", "gi");
		atCodeMatchArray = textLine.match(numericLenSearchRegex);
		if ((atCodeMatchArray != null) && (atCodeMatchArray.length > 0))
		{
			for (var idx in atCodeMatchArray)
			{
				// Extract the length specified between the -L or -R and the final @.
				var dashJustifyIndex = findDashJustifyIndex(atCodeMatchArray[idx]);
				var substLen = atCodeMatchArray[idx].substring(dashJustifyIndex+2, atCodeMatchArray[idx].length-1);
				textLine = this.ReplaceMsgAtCodeFormatStr(pMsgHdr, pDisplayMsgNum, textLine, substLen, atCodeMatchArray[idx],
				                                     pDateTimeStr, useBBSLocalTimeZone, mainMsgAttrStr, mainMsgAttrStr,
				                                     auxMsgAttrStr, netMsgAttrStr, allMsgAttrStr, dashJustifyIndex);
			}
		}
	}

	// In case there weren't any complex @-codes, do replacements for the basic
	// @-codes.  Set the group & sub-board information as Personal Mail or the
	// sub-board currently being read.
	var grpIdx = -1;
	var groupName = "";
	var groupDesc = "";
	var subName = "";
	var subDesc = "";
	var msgConf = "";
	var fileDir = "";
	var fileDirLong = "";
	// TODO
	if ((typeof(bbs.curlib) == "number") && (typeof(bbs.curdir) == "number"))
	{
		if ((typeof(file_area.lib_list[bbs.curlib]) == "object") && (typeof(file_area.lib_list[bbs.curlib].dir_list[bbs.curdir]) == "object"))
		{
			fileDir = file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].name;
			fileDirLong = file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].description;
		}
	}
	// Declared earlier
	// TODO: Not sure if these are needed
	/*
	var allMsgAttrStr = makeAllMsgAttrStr(pMsgHdr);
	var mainMsgAttrStr = makeMainMsgAttrStr(pMsgHdr.attr);
	var auxMsgAttrStr = makeAuxMsgAttrStr(pMsgHdr.auxattr);
	var netMsgAttrStr = makeNetMsgAttrStr(pMsgHdr.netattr);
	*/
	var messageNum = (typeof(pDisplayMsgNum) == "number" ? pDisplayMsgNum : 0);
	// Get the fields we need from the header, and if the header is UTF-8 and the user's terminal doesn't support
	// UTF-8, convert the text
	var fromName = pMsgHdr.from;
	var toName = pMsgHdr.to;
	var fromNetAddr = (typeof(pMsgHdr.from_net_addr) == "string" ? " (" + pMsgHdr.from_net_addr + ")" : "");
	var from_ext = (typeof pMsgHdr.from_ext === "undefined" ? "" : pMsgHdr.from_ext);
	var to_ext = (typeof pMsgHdr.to_ext === "undefined" ? "" : pMsgHdr.to_ext);
	var subject = pMsgHdr.subject;
	var id = (typeof(pMsgHdr.id) === "string" ? pMsgHdr.id : "");
	var reply_id = (typeof(pMsgHdr.reply_id) === "string" ? pMsgHdr.reply_id : "");
	var from_net_addr = (typeof(pMsgHdr.from_net_addr) === "string" ? pMsgHdr.from_net_addr : "");
	var to_net_addr = (typeof(pMsgHdr.to_net_addr) === "string" ? pMsgHdr.to_net_addr : "");
	if (pMsgHdr.hasOwnProperty("is_utf8") && pMsgHdr.is_utf8)
	{
		var userConsoleSupportsUTF8 = false;
		if (typeof(USER_UTF8) != "undefined")
			userConsoleSupportsUTF8 = console.term_supports(USER_UTF8);
		if (!userConsoleSupportsUTF8)
		{
			fromName = utf8_cp437(fromName);
			toName = utf8_cp437(toName);
			fromNetAddr = utf8_cp437(fromNetAddr);
			from_ext = utf8_cp437(from_ext);
			to_ext = utf8_cp437(to_ext);
			subject = utf8_cp437(subject);
			id = utf8_cp437(id);
			reply_id = utf8_cp437(reply_id);
			from_net_addr = utf8_cp437(from_net_addr);
			to_net_addr = utf8_cp437(to_net_addr);
		}
	}
	// This list has some custom @-codes:
	var newTxtLine = textLine.replace(/@MSG_SUBJECT@/gi, subject)
	                         .replace(/@MSG_TO@/gi, pMsgHdr["to"])
	                         .replace(/@MSG_TO_NAME@/gi, pMsgHdr["to"])
	                         .replace(/@MSG_TO_EXT@/gi, to_ext)
	                         .replace(/@MSG_DATE@/gi, pDateTimeStr)
	                         //.replace(/@MSG_ATTR@/gi, mainMsgAttrStr)
	                         //.replace(/@MSG_AUXATTR@/gi, auxMsgAttrStr)
	                         //.replace(/@MSG_NETATTR@/gi, netMsgAttrStr)
	                         //.replace(/@MSG_ALLATTR@/gi, allMsgAttrStr)
	                         //.replace(/@MSG_NUM_AND_TOTAL@/gi, messageNum.toString() + "/" + this.NumMessages()) // TODO
	                         .replace(/@MSG_NUM@/gi, messageNum.toString())
	                         .replace(/@MSG_ID@/gi, id)
	                         .replace(/@MSG_REPLY_ID@/gi, reply_id)
	                         .replace(/@MSG_FROM_NET@/gi, from_net_addr)
	                         .replace(/@MSG_TO_NET@/gi, to_net_addr)
	                         //.replace(/@MSG_TIMEZONE@/gi, (useBBSLocalTimeZone ? system.zonestr(system.timezone) : system.zonestr(pMsgHdr["when_written_zone"]))) // TODO
	                         .replace(/@GRP@/gi, groupName) // TODO
	                         .replace(/@GRPL@/gi, groupDesc) // TODO
	                         .replace(/@SUB@/gi, subName) // TODO
	                         .replace(/@SUBL@/gi, subDesc) // TODO
	                         .replace(/@CONF@/gi, msgConf) // TODO
	                         .replace(/@SYSOP@/gi, system.operator)
	                         .replace(/@DATE@/gi, system.datestr())
	                         .replace(/@LOCATION@/gi, system.location)
	                         .replace(/@DIR@/gi, fileDir)
	                         .replace(/@DIRL@/gi, fileDirLong)
	                         .replace(/@ALIAS@/gi, user.alias)
	                         .replace(/@NAME@/gi, (user.name.length > 0 ? user.name : user.alias))
	                         .replace(/@USER@/gi, user.alias)
	                         .replace(/@MSG_UPVOTES@/gi, 0)
	                         .replace(/@MSG_DOWNVOTES@/gi, 0)
	                         .replace(/@MSG_SCORE@/gi, 0);
	// If the user is not the sysop and the message was posted anonymously,
	// then replace the from name @-codes with "Anonymous".  Otherwise,
	// replace the from name @-codes with the actual from name.
	if (!user.is_sysop && ((pMsgHdr.attr & MSG_ANONYMOUS) == MSG_ANONYMOUS))
	{
		newTxtLine = newTxtLine.replace(/@MSG_FROM@/gi, "Anonymous")
		                       .replace(/@MSG_FROM_AND_FROM_NET@/gi, "Anonymous")
		                       .replace(/@MSG_FROM_EXT@/gi, "Anonymous");
	}
	else
	{
		newTxtLine = newTxtLine.replace(/@MSG_FROM@/gi, pMsgHdr["from"])
		                       .replace(/@MSG_FROM_AND_FROM_NET@/gi, fromName + (fromNetAddr.length > 0 ? " (" + fromNetAddr + ")" : ""))
		                       .replace(/@MSG_FROM_EXT@/gi, from_ext);
	}
	if (!pAllowCLS)
		newTxtLine = newTxtLine.replace(/@CLS@/gi, "");
	newTxtLine = replaceAtCodesInStr(newTxtLine);
	
	/*
	// New: Let Synchronet replace known @-codes
	var startIdx = 0;
	var atIdx = pTextLine.indexOf("@", startIdx);
	while (atIdx > -1)
	{
		var nextAtIdx = pTextLine.indexOf("@", atIdx+1);
		if (nextAtIdx > startIdx)
			newTxtLine += pTextLine.substring(startIdx, nextAtIdx);
		if (nextAtIdx > atIdx)
		{
			newTxtLine += bbs.atcode(pTextLine.substring(atIdx, nextAtIdx));
			startIdx = nextAtIdx + 1;
		}
		else
			startIdx = atIdx + 1;
		atIdx = pTextLine.indexOf("@", startIdx);
	}
	if (startIdx < pTextLine.length)
		newTxtLine += pTextLine.substr(startIdx);
	// End New
	*/

	return newTxtLine;
}
// Helper for ParseMsgAtCodes(): Replaces a
// given @-code format string in a text line with the appropriate message header
// info or BBS system info.
//
// Parameters:
//  pMsgHdr: The object containing the message header information
//  pDisplayMsgNum: The message number, if different from the number in the header
//                  object (i.e., if doing a message search).  This parameter can
//                  be null, in which case the number in the header object will be
//                  used.
//  pTextLine: The text line in which to perform the replacement
//  pSpecifiedLen: The length extracted from the @-code format string
//  pAtCodeStr: The @-code format string, which will be replaced with the actual message info
//  pDateTimeStr: Formatted string containing the date & time
//  pUseBBSLocalTimeZone: Boolean - Whether or not pDateTimeStr is in the BBS's local time zone.
//  pMsgMainAttrStr: A string describing the main message attributes ('attr' property of header)
//  pMsgAuxAttrStr: A string describing the auxiliary message attributes ('auxattr' property of header)
//  pMsgNetAttrStr: A string describing the network message attributes ('netattr' property of header)
//  pMsgAllAttrStr: A string describing all message attributes
//  pDashJustifyIdx: Optional - The index of the -L or -R in the @-code string
function ReplaceMsgAtCodeFormatStr(pMsgHdr, pDisplayMsgNum, pTextLine, pSpecifiedLen,
                                  pAtCodeStr, pDateTimeStr, pUseBBSLocalTimeZone, pMsgMainAttrStr, pMsgAuxAttrStr,
								  pMsgNetAttrStr, pMsgAllAttrStr, pDashJustifyIdx)
{
	// Get the fields we need from the header, and if the header is UTF-8 and the user's terminal doesn't support
	// UTF-8, convert the text
	var fromName = pMsgHdr.from;
	var toName = pMsgHdr.hasOwnProperty("to") ? pMsgHdr.to : "";
	var fromNetAddr = (typeof(pMsgHdr.from_net_addr) == "string" ? " (" + pMsgHdr.from_net_addr + ")" : "");
	var fromExt = (typeof pMsgHdr.from_ext === "undefined" ? "" : pMsgHdr.from_ext);
	var toExt = (typeof pMsgHdr.to_ext === "undefined" ? "" : pMsgHdr.to_ext);
	var subject = pMsgHdr.subject;
	var id = (pMsgHdr.hasOwnProperty("id") ? pMsgHdr.id : "");
	var reply_id = (pMsgHdr.hasOwnProperty("id") ? pMsgHdr.reply_id : "");
	if (pMsgHdr.hasOwnProperty("is_utf8") && pMsgHdr.is_utf8)
	{
		var userConsoleSupportsUTF8 = false;
		if (typeof(USER_UTF8) != "undefined")
			userConsoleSupportsUTF8 = console.term_supports(USER_UTF8);
		if (!userConsoleSupportsUTF8)
		{
			fromName = utf8_cp437(fromName);
			toName = utf8_cp437(toName);
			fromNetAddr = utf8_cp437(fromNetAddr);
			fromExt = utf8_cp437(fromExt);
			toExt = utf8_cp437(toExt);
			subject = utf8_cp437(subject);
			id = utf8_cp437(id);
			reply_id = utf8_cp437(reply_id);
		}
	}

	if (typeof(pDashJustifyIdx) != "number")
		pDashJustifyIdx = findDashJustifyIndex(pAtCodeStr);
	// Specify the format string with left or right justification based on the justification
	// character (either L or R).
	var formatStr = ((/L/i).test(pAtCodeStr.charAt(pDashJustifyIdx+1)) ? "%-" : "%") + pSpecifiedLen + "s";
	// Specify the replacement text depending on the @-code string
	var replacementTxt = "";
	if (pAtCodeStr.indexOf("@MSG_FROM_AND_FROM_NET") > -1)
	{
		// If the user is not the sysop and the message was posted anonymously,
		// then replace the from name @-codes with "Anonymous".  Otherwise,
		// replace the from name @-codes with the actual from name.
		if (!user.is_sysop && ((pMsgHdr.attr & MSG_ANONYMOUS) == MSG_ANONYMOUS))
			replacementTxt = "Anonymous".substr(0, pSpecifiedLen);
		else
		{
			var fromWithNet = fromName + (typeof(fromNetAddr) == "string" ? " (" + fromNetAddr + ")" : "");
			replacementTxt = fromWithNet.substr(0, pSpecifiedLen);
		}
	}
	else if (pAtCodeStr.indexOf("@MSG_FROM") > -1)
	{
		// If the user is not the sysop and the message was posted anonymously,
		// then replace the from name @-codes with "Anonymous".  Otherwise,
		// replace the from name @-codes with the actual from name.
		if (!user.is_sysop && ((pMsgHdr.attr & MSG_ANONYMOUS) == MSG_ANONYMOUS))
			replacementTxt = "Anonymous".substr(0, pSpecifiedLen);
		else
			replacementTxt = pMsgHdr.from.substr(0, pSpecifiedLen);
	}
	else if (pAtCodeStr.indexOf("@MSG_FROM_EXT") > -1)
	{
		// If the user is not the sysop and the message was posted anonymously,
		// then replace the from name @-codes with "Anonymous".  Otherwise,
		// replace the from name @-codes with the actual from name.
		if (!user.is_sysop && ((pMsgHdr.attr & MSG_ANONYMOUS) == MSG_ANONYMOUS))
			replacementTxt = "Anonymous".substr(0, pSpecifiedLen);
		else
			replacementTxt = (typeof fromExt === "undefined" ? "" : fromExt.substr(0, pSpecifiedLen));
	}
	else if ((pAtCodeStr.indexOf("@MSG_TO") > -1) || (pAtCodeStr.indexOf("@MSG_TO_NAME") > -1))
		replacementTxt = toName.substr(0, pSpecifiedLen);
	else if (pAtCodeStr.indexOf("@MSG_TO_EXT") > -1)
		replacementTxt = (typeof toExt === "undefined" ? "" : toExt.substr(0, pSpecifiedLen));
	else if (pAtCodeStr.indexOf("@MSG_SUBJECT") > -1)
		replacementTxt = subject.substr(0, pSpecifiedLen);
	else if (pAtCodeStr.indexOf("@MSG_DATE") > -1)
		replacementTxt = pDateTimeStr.substr(0, pSpecifiedLen);
	else if (pAtCodeStr.indexOf("@MSG_ATTR") > -1)
		replacementTxt = pMsgMainAttrStr.substr(0, pSpecifiedLen);
	else if (pAtCodeStr.indexOf("@MSG_AUXATTR") > -1)
		replacementTxt = pMsgAuxAttrStr.substr(0, pSpecifiedLen);
	else if (pAtCodeStr.indexOf("@MSG_NETATTR") > -1)
		replacementTxt = pMsgNetAttrStr.substr(0, pSpecifiedLen);
	else if (pAtCodeStr.indexOf("@MSG_ALLATTR") > -1)
		replacementTxt = pMsgAllAttrStr.substr(0, pSpecifiedLen);
	else if (pAtCodeStr.indexOf("@MSG_NUM_AND_TOTAL") > -1)
	{
		var messageNum = (typeof(pDisplayMsgNum) == "number" ? pDisplayMsgNum : 0);
		replacementTxt = (messageNum.toString() + "/" + this.NumMessages()).substr(0, pSpecifiedLen); // "number" is also absolute number
	}
	else if (pAtCodeStr.indexOf("@MSG_NUM") > -1)
	{
		var messageNum = (typeof(pDisplayMsgNum) == "number" ? pDisplayMsgNum : 0);
		replacementTxt = messageNum.toString().substr(0, pSpecifiedLen); // "number" is also absolute number
	}
	else if (pAtCodeStr.indexOf("@MSG_ID") > -1)
		replacementTxt = id.substr(0, pSpecifiedLen);
	else if (pAtCodeStr.indexOf("@MSG_REPLY_ID") > -1)
		replacementTxt = reply_id.substr(0, pSpecifiedLen);
	else if (pAtCodeStr.indexOf("@MSG_TIMEZONE") > -1)
	{
		if (pUseBBSLocalTimeZone)
			replacementTxt = system.zonestr(system.timezone).substr(0, pSpecifiedLen);
		else
		{
			// TODO: The newsgroup headers don't seem to have when_written_zone, but it might
			// be a different property(?)
			//replacementTxt = system.zonestr(pMsgHdr.when_written_zone).substr(0, pSpecifiedLen);
		}
	}
	else if (pAtCodeStr.indexOf("@GRP") > -1)
	{
		// TODO
		/*
		if (this.readingPersonalEmail)
			replacementTxt = "Personal mail".substr(0, pSpecifiedLen);
		else
			replacementTxt = msg_area.sub[this.subBoardCode].grp_name.substr(0, pSpecifiedLen);
		*/
	}
	else if (pAtCodeStr.indexOf("@GRPL") > -1)
	{
		// TODO
		/*
		if (this.readingPersonalEmail)
			replacementTxt = "Personal mail".substr(0, pSpecifiedLen);
		else
		{
			var grpIdx = msg_area.sub[this.subBoardCode].grp_index;
			replacementTxt = msg_area.grp_list[grpIdx].description.substr(0, pSpecifiedLen);
		}
		*/
	}
	else if (pAtCodeStr.indexOf("@SUB") > -1)
	{
		// TODO
		/*
		if (this.readingPersonalEmail)
			replacementTxt = "Personal mail".substr(0, pSpecifiedLen);
		else
			replacementTxt = msg_area.sub[this.subBoardCode].name.substr(0, pSpecifiedLen);
		*/
	}
	else if (pAtCodeStr.indexOf("@SUBL") > -1)
	{
		// TODO
		/*
		if (this.readingPersonalEmail)
			replacementTxt = "Personal mail".substr(0, pSpecifiedLen);
		else
			replacementTxt = msg_area.sub[this.subBoardCode].description.substr(0, pSpecifiedLen);
		*/
	}
	else if ((pAtCodeStr.indexOf("@BBS") > -1) || (pAtCodeStr.indexOf("@BOARDNAME") > -1))
		replacementTxt = system.name.substr(0, pSpecifiedLen);
	else if (pAtCodeStr.indexOf("@SYSOP") > -1)
		replacementTxt = system.operator.substr(0, pSpecifiedLen);
	else if (pAtCodeStr.indexOf("@CONF") > -1)
	{
		// TODO
		/*
		var msgConfDesc = msg_area.grp_list[msg_area.sub[this.subBoardCode].grp_index].name + " "
		                + msg_area.sub[this.subBoardCode].name;
		replacementTxt = msgConfDesc.substr(0, pSpecifiedLen);
		*/
	}
	else if (pAtCodeStr.indexOf("@DATE") > -1)
		replacementTxt = system.datestr().substr(0, pSpecifiedLen);
	else if (pAtCodeStr.indexOf("@DIR") > -1)
	{
		if ((typeof(bbs.curlib) == "number") && (typeof(bbs.curdir) == "number"))
			replacementTxt = file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].name.substr(0, pSpecifiedLen);
		else
			replacementTxt = "";
	}
	else if (pAtCodeStr.indexOf("@DIRL") > -1)
	{
		if ((typeof(bbs.curlib) == "number") && (typeof(bbs.curdir) == "number"))
			replacementTxt = file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].description.substr(0, pSpecifiedLen);
		else
			replacementTxt = "";
	}
	else if ((pAtCodeStr.indexOf("@ALIAS") > -1) || (pAtCodeStr.indexOf("@USER") > -1))
		replacementTxt = user.alias.substr(0, pSpecifiedLen);
	else if (pAtCodeStr.indexOf("@NAME") > -1) // User name or alias
	{
		var userNameOrAlias = (user.name.length > 0 ? user.name : user.alias);
		replacementTxt = userNameOrAlias.substr(0, pSpecifiedLen);
	}

	// Do the text replacement (escape special characters in the @-code string so we can do a literal search)
	var searchRegex = new RegExp(pAtCodeStr.replace(/[-[\]{}()*+?.,\\^$|#\s]/g, "\\$&"), "gi");
	return pTextLine.replace(searchRegex, format(formatStr, replacementTxt));
}
// Helper for ParseMsgAtCodes() and
// DigDistMsgReader_ReplaceMsgAtCodeFormatStr(): Returns the index of the -L or -R in
// one of the @-code strings.
//
// Parameters:
//  pAtCodeStr: The @-code string to search
//
// Return value: The index of the -L or -R, or -1 if not found
function findDashJustifyIndex(pAtCodeStr)
{
	var strIndex = pAtCodeStr.indexOf("-");
	if (strIndex > -1)
	{
		// If this part of the string is not -L or -R, then set strIndex to -1
		// to signify that it was not found.
		var checkStr = pAtCodeStr.substr(strIndex, 2).toUpperCase();
		if ((checkStr != "-L") && (checkStr != "-R"))
			strIndex = -1;
	}
	return strIndex;
}

// TODO: Not sure if these are needed
/*
// Returns a string describing all message attributes (main, auxiliary, and net).
//
// Parameters:
//  pMsgHdr: A message header object.  
//
// Return value: A string describing all of the message attributes
function makeAllMsgAttrStr(pMsgHdr)
{
	if ((pMsgHdr == null) || (typeof(pMsgHdr) != "object"))
		return "";

	var msgAttrStr = makeMainMsgAttrStr(pMsgHdr.attr);
	var auxAttrStr = makeAuxMsgAttrStr(pMsgHdr.auxattr);
	if (auxAttrStr.length > 0)
	{
		if (msgAttrStr.length > 0)
			msgAttrStr += ", ";
		msgAttrStr += auxAttrStr;
	}
	var netAttrStr = makeNetMsgAttrStr(pMsgHdr.netattr);
	if (netAttrStr.length > 0)
	{
		if (msgAttrStr.length > 0)
			msgAttrStr += ", ";
		msgAttrStr += netAttrStr;
	}
	return msgAttrStr;
}

// Returns a string describing the main message attributes.
//
// Parameters:
//  pMainMsgAttrs: The bit field for the main message attributes
//                 (normally, the 'attr' property of a header object)
//  pIfEmptyString: Optional - A string to use if there are no attributes set
//
// Return value: A string describing the main message attributes
function makeMainMsgAttrStr(pMainMsgAttrs, pIfEmptyString)
{
	if (makeMainMsgAttrStr.attrStrs === undefined)
	{
		makeMainMsgAttrStr.attrStrs = [
			{ attr: MSG_DELETE, str: "Del" },
			{ attr: MSG_PRIVATE, str: "Priv" },
			{ attr: MSG_READ, str: "Read" },
			{ attr: MSG_PERMANENT, str: "Perm" },
			{ attr: MSG_LOCKED, str: "Lock" },
			{ attr: MSG_ANONYMOUS, str: "Anon" },
			{ attr: MSG_KILLREAD, str: "Killread" },
			{ attr: MSG_MODERATED, str: "Mod" },
			{ attr: MSG_VALIDATED, str: "Valid" },
			{ attr: MSG_REPLIED, str: "Repl" },
			{ attr: MSG_NOREPLY, str: "NoRepl" }
		];
	}

	var msgAttrStr = "";
	if (typeof(pMainMsgAttrs) == "number")
	{
		for (var i = 0; i < makeMainMsgAttrStr.attrStrs.length; ++i)
		{
			if (Boolean(pMainMsgAttrs & makeMainMsgAttrStr.attrStrs[i].attr))
			{
				if (msgAttrStr.length > 0)
					msgAttrStr += ", ";
				msgAttrStr += makeMainMsgAttrStr.attrStrs[i].str;
			}
		}
	}
	if ((msgAttrStr.length == 0) && (typeof(pIfEmptyString) == "string"))
		msgAttrStr = pIfEmptyString;
	return msgAttrStr;
}

// Returns a string describing auxiliary message attributes.
//
// Parameters:
//  pAuxMsgAttrs: The bit field for the auxiliary message attributes
//                (normally, the 'auxattr' property of a header object)
//  pIfEmptyString: Optional - A string to use if there are no attributes set
//
// Return value: A string describing the auxiliary message attributes
function makeAuxMsgAttrStr(pAuxMsgAttrs, pIfEmptyString)
{
	if (makeAuxMsgAttrStr.attrStrs === undefined)
	{
		makeAuxMsgAttrStr.attrStrs = [
			{ attr: MSG_FILEREQUEST, str: "Freq" },
			{ attr: MSG_FILEATTACH, str: "Attach" },
			{ attr: MSG_KILLFILE, str: "KillFile" },
			{ attr: MSG_RECEIPTREQ, str: "RctReq" },
			{ attr: MSG_CONFIRMREQ, str: "ConfReq" },
			{ attr: MSG_NODISP, str: "NoDisp" }
		];
		if (typeof(MSG_TRUNCFILE) === "number")
			makeAuxMsgAttrStr.attrStrs.push({ attr: MSG_TRUNCFILE, str: "TruncFile" });
	}

	var msgAttrStr = "";
	if (typeof(pAuxMsgAttrs) == "number")
	{
		for (var i = 0; i < makeAuxMsgAttrStr.attrStrs.length; ++i)
		{
			if (Boolean(pAuxMsgAttrs & makeAuxMsgAttrStr.attrStrs[i].attr))
			{
				if (msgAttrStr.length > 0)
					msgAttrStr += ", ";
				msgAttrStr += makeAuxMsgAttrStr.attrStrs[i].str;
			}
		}
	}
	if ((msgAttrStr.length == 0) && (typeof(pIfEmptyString) == "string"))
		msgAttrStr = pIfEmptyString;
	return msgAttrStr;
}

// Returns a string describing network message attributes.
//
// Parameters:
//  pNetMsgAttrs: The bit field for the network message attributes
//                (normally, the 'netattr' property of a header object)
//  pIfEmptyString: Optional - A string to use if there are no attributes set
//
// Return value: A string describing the network message attributes
function makeNetMsgAttrStr(pNetMsgAttrs, pIfEmptyString)
{
	if (makeNetMsgAttrStr.attrStrs === undefined)
	{
		makeNetMsgAttrStr.attrStrs = [
			{ attr: MSG_LOCAL, str: "FromLocal" },
			{ attr: MSG_INTRANSIT, str: "Transit" },
			{ attr: MSG_SENT, str: "Sent" },
			{ attr: MSG_KILLSENT, str: "KillSent" },
			{ attr: MSG_ARCHIVESENT, str: "ArcSent" },
			{ attr: MSG_HOLD, str: "Hold" },
			{ attr: MSG_CRASH, str: "Crash" },
			{ attr: MSG_IMMEDIATE, str: "Now" },
			{ attr: MSG_DIRECT, str: "Direct" }
		];
		if (typeof(MSG_GATE) === "number")
			makeNetMsgAttrStr.attrStrs.push({ attr: MSG_GATE, str: "Gate" });
		if (typeof(MSG_ORPHAN) === "number")
			makeNetMsgAttrStr.attrStrs.push({ attr: MSG_ORPHAN, str: "Orphan" });
		if (typeof(MSG_FPU) === "number")
			makeNetMsgAttrStr.attrStrs.push({ attr: MSG_FPU, str: "FPU" });
		if (typeof(MSG_TYPELOCAL) === "number")
			makeNetMsgAttrStr.attrStrs.push({ attr: MSG_TYPELOCAL, str: "ForLocal" });
		if (typeof(MSG_TYPEECHO) === "number")
			makeNetMsgAttrStr.attrStrs.push({ attr: MSG_TYPEECHO, str: "ForEcho" });
		if (typeof(MSG_TYPENET) === "number")
			makeNetMsgAttrStr.attrStrs.push({ attr: MSG_TYPENET, str: "ForNetmail" });
		if (typeof(MSG_MIMEATTACH) === "number")
			makeNetMsgAttrStr.attrStrs.push({ attr: MSG_MIMEATTACH, str: "MimeAttach" });
	}

	var msgAttrStr = "";
	if (typeof(pNetMsgAttrs) == "number")
	{
		for (var i = 0; i < makeNetMsgAttrStr.attrStrs.length; ++i)
		{
			if (Boolean(pNetMsgAttrs & makeNetMsgAttrStr.attrStrs[i].attr))
			{
				if (msgAttrStr.length > 0)
					msgAttrStr += ", ";
				msgAttrStr += makeNetMsgAttrStr.attrStrs[i].str;
			}
		}
	}
	if ((msgAttrStr.length == 0) && (typeof(pIfEmptyString) == "string"))
		msgAttrStr = pIfEmptyString;
	return msgAttrStr;
}
*/

// Replaces @-codes in a string and returns the new string.
//
// Parameters:
//  pStr: A string in which to replace @-codes
//
// Return value: A version of the string with @-codes interpreted
function replaceAtCodesInStr(pStr)
{
	if (typeof(pStr) != "string")
		return "";

	// This code was originally written by Deuce.  I updated it to check whether
	// the string returned by bbs.atcode() is null, and if so, just return
	// the original string.
	return pStr.replace(/@([^@]+)@/g, function(m, code) {
		var decoded = bbs.atcode(code);
		return (decoded != null ? decoded : "@" + code + "@");
	});
}

// Creates the (string) key help line to show at the bottom of the screen for the newsgroup list
//
// Parameters:
//  pShowingOnlySubscribedNewsgroups: Boolean - Whether or not the user is seeing
//                                    only subscribed newsgroups
//
// Return value: The key help line for showing the newsgroup list
function CreateNewsgrpListHelpLine(pShowingOnlySubscribedNewsgroups)
{
	var newsgrpListLightbarModeHelpLine = gSettings.colors.lightbarNewsgroupListHelpLineHotkeyColor + "@CLEAR_HOT@@`Up`" + KEY_UP + "@"
	                           + gSettings.colors.lightbarNewsgroupListHelpLineGeneralColor + "/"
	                           //+ gSettings.colors.lightbarNewsgroupListHelpLineHotkeyColor + "@`Dn`" + KEY_DOWN + "@"
	                           + gSettings.colors.lightbarNewsgroupListHelpLineHotkeyColor + "@`Dn`\\n@"
	                           + gSettings.colors.lightbarNewsgroupListHelpLineGeneralColor + "/"
	                           //+ gSettings.colors.lightbarNewsgroupListHelpLineHotkeyColor + "@`PgUp`" + KEY_PAGEUP + "@"
	                           + gSettings.colors.lightbarNewsgroupListHelpLineHotkeyColor + "@`PgUp`" + "\x1b[V" + "@"
	                           + gSettings.colors.lightbarNewsgroupListHelpLineGeneralColor + "/"
	                           + gSettings.colors.lightbarNewsgroupListHelpLineHotkeyColor + "@`Dn`" + KEY_PAGEDN + "@"
	                           + gSettings.colors.lightbarNewsgroupListHelpLineGeneralColor + "/"
	                           //+ gSettings.colors.lightbarNewsgroupListHelpLineHotkeyColor + "@`ENTER`" + KEY_ENTER + "@"
	                           + gSettings.colors.lightbarNewsgroupListHelpLineHotkeyColor + "@`ENTER`\\r\\n@"
	                           + gSettings.colors.lightbarNewsgroupListHelpLineGeneralColor + "/"
	                           + gSettings.colors.lightbarNewsgroupListHelpLineHotkeyColor + "@`HOME`" + KEY_HOME + "@"
	                           + gSettings.colors.lightbarNewsgroupListHelpLineGeneralColor + "/"
	                           + gSettings.colors.lightbarNewsgroupListHelpLineHotkeyColor + "@`END`" + KEY_END + "@";
	var helpLineLen = 28;
	newsgrpListLightbarModeHelpLine += gSettings.colors.lightbarNewsgroupListHelpLineGeneralColor
	                             + ", " + gSettings.colors.lightbarNewsgroupListHelpLineHotkeyColor
	                             + gSettings.colors.lightbarNewsgroupListHelpLineHotkeyColor + "@`/`/@"
	                             + gSettings.colors.lightbarNewsgroupListHelpLineGeneralColor + ", ";
	helpLineLen += 6; // TODO: Should be 5?
	// If the user is seeing only subscribed newsgroups, then add DEL to allow them to clear their
	// subscribed newsgroups
	if (pShowingOnlySubscribedNewsgroups)
	{
		newsgrpListLightbarModeHelpLine += gSettings.colors.lightbarNewsgroupListHelpLineHotkeyColor + "@`DEL`" + KEY_DEL + "@"
		                           + gSettings.colors.lightbarNewsgroupListHelpLineGeneralColor + ", ";
		helpLineLen += 5;
	}
	newsgrpListLightbarModeHelpLine += gSettings.colors.lightbarNewsgroupListHelpLineHotkeyColor + "@`Q`Q@"
	                             + gSettings.colors.lightbarNewsgroupListHelpLineParenColor + ")"
	                             + gSettings.colors.lightbarNewsgroupListHelpLineGeneralColor + "uit, "
	                             + gSettings.colors.lightbarNewsgroupListHelpLineHotkeyColor + "@`?`?@";
	helpLineLen += 8;

	// Pad the help line on the left & right with spaces
	var numPadding = Math.floor((console.screen_columns - helpLineLen) / 2);
	var paddingStr = "";
	for (var i = 0; i < numPadding; ++i)
		paddingStr += " ";
	newsgrpListLightbarModeHelpLine = gSettings.colors.lightbarNewsgroupListHelpLineBkgColor + paddingStr + newsgrpListLightbarModeHelpLine + paddingStr;

	return newsgrpListLightbarModeHelpLine;
}

// Creates the (string) key help line to show at the bottom of the screen for the article list
function CreateArticleListHelpLine()
{
	var articleListLightbarModeHelpLine = gSettings.colors.lightbarArticleListHelpLineHotkeyColor + "@CLEAR_HOT@@`Up`" + KEY_UP + "@"
	                           + gSettings.colors.lightbarArticleListHelpLineGeneralColor + "/"
	                           + gSettings.colors.lightbarArticleListHelpLineHotkeyColor + "@`Dn`\\n@"
	                           + gSettings.colors.lightbarArticleListHelpLineGeneralColor + "/"
	                           //+ gSettings.colors.lightbarNewsgroupListHelpLineHotkeyColor + "@`PgUp`" + KEY_PAGEUP + "@"
	                           + gSettings.colors.lightbarArticleListHelpLineHotkeyColor + "@`PgUp`" + "\x1b[V" + "@"
	                           + gSettings.colors.lightbarArticleListHelpLineGeneralColor + "/"
	                           + gSettings.colors.lightbarArticleListHelpLineHotkeyColor + "@`Dn`" + KEY_PAGEDN + "@"
	                           + gSettings.colors.lightbarArticleListHelpLineGeneralColor + ", "
	                           + gSettings.colors.lightbarArticleListHelpLineHotkeyColor + "@`ENTER`\\r\\n@"
	                           + gSettings.colors.lightbarArticleListHelpLineGeneralColor + ", "
	                           + gSettings.colors.lightbarArticleListHelpLineHotkeyColor + "@`HOME`" + KEY_HOME + "@"
	                           + gSettings.colors.lightbarArticleListHelpLineGeneralColor + ", "
	                           + gSettings.colors.lightbarArticleListHelpLineHotkeyColor + "@`END`" + KEY_END + "@";
	var helpLineLen = 31;
	articleListLightbarModeHelpLine += gSettings.colors.lightbarArticleListHelpLineGeneralColor
	                             + ", " + gSettings.colors.lightbarArticleListHelpLineHotkeyColor
	                             + "#" + gSettings.colors.lightbarArticleListHelpLineGeneralColor + ", ";
	helpLineLen += 5;
	articleListLightbarModeHelpLine += gSettings.colors.lightbarArticleListHelpLineHotkeyColor + "@`P`p@"
	                             + gSettings.colors.lightbarArticleListHelpLineParenColor + ")"
	                             + gSettings.colors.lightbarArticleListHelpLineGeneralColor + "ost, "
	                             + gSettings.colors.lightbarArticleListHelpLineHotkeyColor + "@`Q`q@"
	                             + gSettings.colors.lightbarArticleListHelpLineParenColor + ")"
	                             + gSettings.colors.lightbarArticleListHelpLineGeneralColor + "uit, "
	                             + gSettings.colors.lightbarArticleListHelpLineHotkeyColor + "@`?`?@  ";
	helpLineLen += 18; // TODO: Seems maybe one off

	// Pad the help line with spaces on each side
	var numPadding = Math.floor((console.screen_columns - helpLineLen) / 2);
	var paddingStr = "";
	for (var i = 0; i < numPadding; ++i)
		paddingStr += " ";
	articleListLightbarModeHelpLine = gSettings.colors.lightbarArticleListHelpLineBkgColor + paddingStr + articleListLightbarModeHelpLine + paddingStr;

	return articleListLightbarModeHelpLine;
}

// Creates the (string) key help line to show at the bottom of the screen for scrollable reader mode
function CreateScrollableReaderHelpLine()
{
	var scrollableReadHelpLine = gSettings.colors.scrollableReadHelpLineHotkeyColor + "@CLEAR_HOT@@`Up`" + KEY_UP + "@"
	                           + gSettings.colors.scrollableReadHelpLineGeneralColor + "/"
	                           + gSettings.colors.scrollableReadHelpLineHotkeyColor + "@`Dn`\\n@"
	                           + gSettings.colors.scrollableReadHelpLineGeneralColor + "/"
	                           + gSettings.colors.scrollableReadHelpLineHotkeyColor + "@`<-`" + KEY_LEFT + "@"
	                           + gSettings.colors.scrollableReadHelpLineGeneralColor + "/"
	                           + gSettings.colors.scrollableReadHelpLineHotkeyColor + "@`->`" + KEY_RIGHT + "@"
	                           + gSettings.colors.scrollableReadHelpLineGeneralColor + "/"
	                           + gSettings.colors.scrollableReadHelpLineHotkeyColor + "@`PgUp`" + "\x1b[V" + "@"
	                           + gSettings.colors.scrollableReadHelpLineGeneralColor + "/"
	                           + gSettings.colors.scrollableReadHelpLineHotkeyColor + "@`Dn`" + KEY_PAGEDN + "@"
	                           + gSettings.colors.scrollableReadHelpLineGeneralColor + "/"
	                           + gSettings.colors.scrollableReadHelpLineHotkeyColor + "@`HOME`" + KEY_HOME + "@"
	                           + gSettings.colors.scrollableReadHelpLineGeneralColor + "/"
	                           + gSettings.colors.scrollableReadHelpLineHotkeyColor + "@`END`" + KEY_END + "@";
	var helpLineLen = 28;
	scrollableReadHelpLine += gSettings.colors.scrollableReadHelpLineGeneralColor + ", "
	                        + gSettings.colors.scrollableReadHelpLineHotkeyColor + "@`F`Q@"
	                        + gSettings.colors.scrollableReadHelpLineParenColor + ")"
	                        + gSettings.colors.scrollableReadHelpLineGeneralColor + "irst, "
	                        + gSettings.colors.scrollableReadHelpLineHotkeyColor + "@`L`Q@"
	                        + gSettings.colors.scrollableReadHelpLineParenColor + ")"
	                        + gSettings.colors.scrollableReadHelpLineGeneralColor + "ast, "
	                        + gSettings.colors.scrollableReadHelpLineHotkeyColor + "@`R`Q@"
	                        + gSettings.colors.scrollableReadHelpLineParenColor + ")"
	                        + gSettings.colors.scrollableReadHelpLineGeneralColor + "eply, "

	                        + gSettings.colors.scrollableReadHelpLineHotkeyColor + "@`M`M@"
	                        + gSettings.colors.scrollableReadHelpLineParenColor + ")"
	                        + gSettings.colors.scrollableReadHelpLineGeneralColor + "sgs, "

	                        + gSettings.colors.scrollableReadHelpLineHotkeyColor + "@`Q`Q@"
	                        + gSettings.colors.scrollableReadHelpLineParenColor + ")"
	                        + gSettings.colors.scrollableReadHelpLineGeneralColor + "uit, "
	                        + gSettings.colors.scrollableReadHelpLineHotkeyColor + "@`?`?@";
	helpLineLen += 40;

	// Pad the help line with spaces on each side
	var leftPadding = Math.floor((console.screen_columns - helpLineLen) / 2);
	var rightPadding = leftPadding;
	var maxPaddingLen = (console.screen_columns - 1) - helpLineLen;
	var totalPadding = leftPadding + rightPadding;
	if (totalPadding > maxPaddingLen)
		rightPadding += (maxPaddingLen - totalPadding);
	var rightPaddingStr = "";
	for (var i = 0; i < rightPadding; ++i)
		rightPaddingStr += " ";
	var leftPaddingStr = "";
	for (var i = 0; i < leftPadding; ++i)
		leftPaddingStr += " ";
	scrollableReadHelpLine = gSettings.colors.scrollableReadHelpLineBkgColor + leftPaddingStr + scrollableReadHelpLine + rightPaddingStr;

	return scrollableReadHelpLine;
}

// Creates a scrollable Frame object with a scrollbar
//
// Parameters:
//  pX: The X (horizontal) component of the upper-left of the frame
//  pY: The Y (vertical) component of the upper-left of the frame
//  pWidth: The width of the frame
//  pHeight: The height of the frame
//
// Return value: An object with the following properties:
//               frame: The Frame object
//               scrollbar: The ScrollBar object
function CreateFrameWithScrollbar(pX, pY, pWidth, pHeight)
{
	var retObj = {
		frame: null,
		scrollbar: null
	};
	retObj.frame = new Frame(pX, pY, pWidth, pHeight, BG_BLACK);
	retObj.frame.attr &=~ HIGH;
	retObj.frame.v_scroll = true;
	retObj.frame.h_scroll = false;
	retObj.frame.scrollbars = true;
	retObj.scrollbar = new ScrollBar(retObj.frame, {bg: BG_BLACK, fg: LIGHTGRAY, orientation: "vertical", autohide: false});
	return retObj;
}


// Displays a Frame on the screen and allows scrolling through it with the up &
// down arrow keys, PageUp, PageDown, HOME, and END.
//
// Parameters:
//  pFrame: A Frame object to display & scroll through
//  pScrollbar: A ScrollBar object associated with the Frame object
//  pTopLineIdx: The index of the text line to display at the top
//  pTxtAttrib: The attribute(s) to apply to the text lines
//  pWriteTxtLines: Boolean - Whether or not to write the text lines (in addition
//                  to doing the message loop).  If false, this will only do the
//                  the message loop.  This parameter is intended as a screen
//                  refresh optimization.
//  pPostWriteCurX: The X location for the cursor after writing the message
//                  lines
//  pPostWriteCurY: The Y location for the cursor after writing the message
//                  lines
//
// Return value: An object with the following properties:
//               lastKeypress: The last key pressed by the user (a string)
//               topLineIdx: The new top line index of the text lines, in case of scrolling
function ScrollFrame(pFrame, pScrollbar, pTopLineIdx, pTxtAttrib, pWriteTxtLines, pPostWriteCurX,
                     pPostWriteCurY)
{
	// Variables for the top line index for the last page, scrolling, etc.
	var topLineIdxForLastPage = pFrame.data_height - pFrame.height;
	if (topLineIdxForLastPage < 0)
		topLineIdxForLastPage = 0;

	var retObj = {
		lastKeypress: "",
		topLineIdx: pTopLineIdx
	};

	if (pTopLineIdx > 0)
		pFrame.scrollTo(0, pTopLineIdx);

	var writeTxtLines = pWriteTxtLines;
	if (writeTxtLines)
	{
		pFrame.invalidate(); // Force drawing on the next call to draw() or cycle()
		pFrame.cycle();
		//pFrame.draw();
	}

	// Frame scroll cycle/user input loop
	var cycleFrame = false;
	var continueOn = true;
	while (continueOn)
	{
		if (cycleFrame)
		{
			// Invalidate the frame to force it to redraw everything, as a
			// workaround to clear the background before writing again
			pFrame.invalidate();
			// Cycle the scrollbar & frame to get them to scroll
			if (pScrollbar != null)
				pScrollbar.cycle();
			pFrame.cycle();
		}

		writeTxtLines = false;
		cycleFrame = false;

		// Get a keypress from the user and take action based on it
		console.gotoxy(pPostWriteCurX, pPostWriteCurY);
		retObj.lastKeypress = GetKeyWithESCChars(K_UPPER|K_NOCRLF|K_NOECHO|K_NOSPIN);
		switch (retObj.lastKeypress)
		{
			case KEY_UP:
				if (retObj.topLineIdx > 0)
				{
					pFrame.scroll(0, -1);
					--retObj.topLineIdx;
					cycleFrame = true;
					writeTxtLines = true;
				}
				break;
			case KEY_DOWN:
				if (retObj.topLineIdx < topLineIdxForLastPage)
				{
					pFrame.scroll(0, 1);
					cycleFrame = true;
					++retObj.topLineIdx;
					writeTxtLines = true;
				}
				break;
			case KEY_PAGEDN: // Next page
				if (retObj.topLineIdx < topLineIdxForLastPage)
				{
					//pFrame.scroll(0, pFrame.height);
					retObj.topLineIdx += pFrame.height;
					if (retObj.topLineIdx > topLineIdxForLastPage)
						retObj.topLineIdx = topLineIdxForLastPage;
					pFrame.scrollTo(0, retObj.topLineIdx);
					cycleFrame = true;
					writeTxtLines = true;
				}
				break;
			case KEY_PAGEUP: // Previous page
				if (retObj.topLineIdx > 0)
				{
					//pFrame.scroll(0, -(pFrame.height));
					retObj.topLineIdx -= pFrame.height;
					if (retObj.topLineIdx < 0)
						retObj.topLineIdx = 0;
					pFrame.scrollTo(0, retObj.topLineIdx);
					cycleFrame = true;
					writeTxtLines = true;
				}
				break;
			case KEY_HOME: // First page
				//pFrame.home();
				pFrame.scrollTo(0, 0);
				cycleFrame = true;
				retObj.topLineIdx = 0;
				break;
			case KEY_END: // Last page
				//pFrame.end();
				pFrame.scrollTo(0, topLineIdxForLastPage);
				cycleFrame = true;
				retObj.topLineIdx = topLineIdxForLastPage;
				break;
			default:
				continueOn = false;
				break;
		}
	}

	return retObj;
}

// Inputs a keypress from the user and handles some ESC-based
// characters such as PageUp, PageDown, and ESC.  If PageUp
// or PageDown are pressed, this function will return the
// string "\x01PgUp" (KEY_PAGEUP) or "\x01Pgdn" (KEY_PAGEDN),
// respectively.  Also, F1-F5 will be returned as "\x01F1"
// through "\x01F5", respectively.
// Thanks goes to Psi-Jack for the original impementation
// of this function.
//
// Parameters:
//  pGetKeyMode: Optional - The mode bits for console.getkey().
//               If not specified, K_NONE will be used.
//
// Return value: The user's keypress
function GetKeyWithESCChars(pGetKeyMode)
{
	var getKeyMode = K_NONE;
	if (typeof(pGetKeyMode) == "number")
		getKeyMode = pGetKeyMode;

	var userInput = console.getkey(getKeyMode);
	if (userInput == KEY_ESC) {
		switch (console.inkey(K_NOECHO|K_NOSPIN, 2)) {
			case '[':
				switch (console.inkey(K_NOECHO|K_NOSPIN, 2)) {
					case 'V':
						userInput = KEY_PAGEUP;
						break;
					case 'U':
						userInput = KEY_PAGEDN;
						break;
				}
				break;
			case 'O':
				switch (console.inkey(K_NOECHO|K_NOSPIN, 2)) {
					case 'P':
						userInput = "\x01F1";
						break;
					case 'Q':
						userInput = "\x01F2";
						break;
					case 'R':
						userInput = "\x01F3";
						break;
					case 'S':
						userInput = "\x01F4";
						break;
					case 't':
						userInput = "\x01F5";
						break;
				}
				break;
			default:
				break;
		}
	}

	return userInput;
}

// Lets the user edit a message, and posts it on the NNTP server
//
// Parameters:
//  pToName: The 'to' name (could be different from the 'to' in the header)
//  pSubject: The subject (could be different from the subject in the header)
//  pNewsgroupName: The name of the newsgroup
//  pMsgHdr: If this is a reply, this parameter is the header of the message being replied to.
//           If this is to be a new message, this parameter can be null/omitted.
//  pMsgBody: If this is to be a reply, this parameter is the message body (will be written
//            to a temporary file for the editor to read for quoting). If this is to be a
//            new message, this parameter can be null/omitted.
//
// Return value: Boolean - Whether or not the message was successfully edited
function EditAndPostMsg(pToName, pSubject, pNewsgroupName, pMsgHdr, pMsgBody)
{
	var editorDropfileName = "";

	/*
	if (pMsgHdr != undefined && pMsgHdr != null && typeof(pMsgHdr) === "object")
	{
		// Get the user's external editor configuration, if there is one, and write
		// the appropriate drop file(s)
		var xtrnEditorCfg = GetUserExternalEditorCfg();
		// For WWIV, just write a EDITOR.INF.  (RESULT.ED is written by the editor upon saving)
		var msgtmpFilename = system.node_dir + "MSGTMP";
		if (xtrnEditorCfg.userHasExternalEditor)
		{
			if (xtrnEditorCfg.editorIsQuickBBSStyle)
			{
				// Write MSGINF
				var msginfRet = writeMsgInf(pMsgHdr, pSubject, pNewsgroupName);
				if (msginfRet.success)
					editorDropfileName = msginfRet.filename;
				// Write the message to MSGTMP
				if (typeof(pMsgBody) === "string")
				{
					var msginfFile = new File(msgtmpFilename);
					if (msginfFile.open("w"))
					{
						msginfFile.writeln(pMsgBody);
						msginfFile.close();
					}
				}
			}
			else
			{
				// WWIV-style: Write EDITOR.INF
				var editorInfRet = writeEditorInf(pMsgHdr, pSubject);
				if (editorInfRet.success)
					editorDropfileName = editorInfRet.filename;
			}
		}
	}
	*/

	// Write the message body to QUOTES.TXT in the node directory
	var quotesFilename = system.node_dir + "QUOTES.TXT";
	if (typeof(pMsgBody) === "string")
	{
		var quotesFile = new File(quotesFilename);
		if (quotesFile.open("w"))
		{
			quotesFile.writeln(pMsgBody);
			quotesFile.close();
		}
	}


	var newsgroupName = (typeof(pNewsgroupName) === "string" ? pNewsgroupName : "");

	// TODO: See if there's a way to get the 'from' name from the message
	// so that we can get the 'from' initials for quote lines.

	// Let the user edit a temporary file, and post it.
	// Note: The last parameter ('false') for console.editfile() specifies to not delete QUOTES.TXT
	var msgFilename = system.node_dir + "groupie_msg.txt";
	var successfullyEditedFile = console.editfile(msgFilename, pToName, user.alias, pSubject, newsgroupName, false);
	file_remove(quotesFilename);
	//file_remove(msgtmpFilename);
	if (editorDropfileName != "")
		file_remove(editorDropfileName);

	// In case there.s a RESULT.ED, read it and get the subject from it, in case the subject changed.
	// Also, we could get the editor name from RESULT.ED too.
	var subject = pSubject;
	var msgEditorName = "";
	var resultEdFilename = file_getcase(system.node_dir + "RESULT.ED");
	if (resultEdFilename != undefined && typeof(resultEdFilename) === "string")
	{
		var resultEdFile = new File(resultEdFilename);
		if (resultEdFile.open("r"))
		{
			// Line 1: Always 0
			// Line 2: Subject
			// Line 3: Editor name
			var resultEdLines = resultEdFile.readAll();
			resultEdFile.close();
			file_remove(resultEdFilename);
			if (resultEdLines.length >= 2)
				subject = resultEdLines[1];
			if (resultEdLines.length >= 3 && resultEdLines[2].length > 0)
				msgEditorName = resultEdLines[2];
		}
	}

	if (successfullyEditedFile)
	{
		var msgBody = "";
		var msgFile = new File(msgFilename);
		if (msgFile.open("r"))
		{
			var fileLines = msgFile.readAll(4096);
			msgFile.close();
			for (var i = 0; i < fileLines.length; ++i)
				msgBody += fileLines[i] + "\r\n";

			if (msgBody != "")
			{
				var postRetObj = nntpClient.PostArticle(pToName, subject, msgBody);
				console.print("\x01n\r\n");
				if (postRetObj.succeeded)
					console.print("\x01cPost \x01hsucceeded\x01n\r\n");
				else
					console.print("\x01y\x01hPost failed!\x01n\r\n\x01p");
			}
			else
				console.print("\x01n\x01y\x01hNot posting empty message\x01n\r\n\x01p");
		}

		if (!file_remove(msgFilename))
			log(LOG_ERR, format("* %s: Failed to delete %s", PROGRAM_NAME, msgFilename));
	}

	return successfullyEditedFile;
}

// https://wiki.synchro.net/ref:msginf
function writeMsgInf(pMsgHdr, pSubject, pNewsgroupName)
{
	var retObj = {
		success: false,
		filename: system.node_dir + "MSGINF"
	};
	/*
	Line	Description
	1		From name or ���������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������Anonymous���������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������
	2		To name or ���������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������All���������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������
	3		Subject
	4		Message number (always ���������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������1���������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������)
	5		Message area name
	6		Private: ���������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������YES��������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������� or ���������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������NO���������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������
	7		Optional tag-line file path/name (Synchronet extension)
	*/
	var dropFile = new File(retObj.filename);
	if (dropFile.open("w"))
	{
		dropFile.writeln(user.name);
		dropFile.writeln(pMsgHdr.hasOwnProperty("from") ? pMsgHdr.from : "All");
		dropFile.writeln(pSubject);
		dropFile.writeln("1");
		dropFile.writeln(pNewsgroupName);
		dropFile.writeln("NO");
		dropFile.close();
		retObj.success = true;
	}
	return retObj;
}

// https://wiki.synchro.net/ref:editor.inf
function writeEditorInf(pMsgHdr, pSubject)
{
	var retObj = {
		success: false,
		filename: system.node_dir + "EDITOR.INF"
	};
	/*
	Line	Descriptipon			WWIV					Synchronet
	1		Message Title			up to 60 characters		up to 70 characters
	2		Message destination		Sub-board name			���������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������to��������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������� user name or ���������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������All���������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������
	3		Author's user number		
	4		Author's name/alias		up to 30 characters		up to 25 characters or ���������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������Anonymous���������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������
	5		Author's real name		up to 20 characters		up to 25 characters
	6		Author's security level	0-255					0-99
	7		flags (number)									not used
	8		"TopLine"				unsure					not used
	9		Language										not used
	*/
	var dropFile = new File(retObj.filename);
	if (dropFile.open("w"))
	{
		dropFile.writeln(pSubject);
		dropFile.writeln(pMsgHdr.hasOwnProperty("from") ? pMsgHdr.from : "All");
		dropFile.writeln(user.number);
		dropFile.writeln(user.alias.substr(0, 25));
		dropFile.writeln(user.name.substr(0, 25));
		dropFile.writeln(user.security.level);
		dropFile.writeln(""); // Flags
		dropFile.writeln(""); // "TopLine"
		dropFile.writeln(""); // Language
		dropFile.close();
		retObj.success = true;
	}
	return retObj;
}

function SaveMessageToFile(pHdr, pBody, pNewsgroupName, pFilename)
{
	var saveSucceeded = false;
	var outFilename = genPathedFilename(gSettings.msgSaveDir, pFilename);
	var outFile = new File(outFilename);
	if (outFile.open("w"))
	{
		outFile.writeln("Newsgroup name: " + pNewsgroupName);
		if (pHdr.hasOwnProperty("from"))
			outFile.writeln("From: " + pHdr.from);
		if (pHdr.hasOwnProperty("to"))
			outFile.writeln("To: " + pHdr.to);
		if (pHdr.hasOwnProperty("subject"))
			outFile.writeln("Subject: " + pHdr.subject);
		if (pHdr.hasOwnProperty("date"))
			outFile.writeln("Date: " + pHdr.date);
		outFile.writeln("-----------------------------");
		outFile.write(pBody);
		outFile.close();
		saveSucceeded = true;
	}
	return saveSucceeded;
}

// For the DDMsgReader class: Saves a message hex dump to a file on the BBS machine with the given filename
//
// Parameters:
//  pMsgHdr: The header of the message
//  pMsgBody: The message body
//  pNewsgroupName: The name of the newsgroup that the message is from
//  pOutFilename: The full path & filename of the file to save the hex dump to
//
// Return value: An object with the following properties:
//               saveSucceeded: Boolean - Whether or not the save succeeded
//               errorMsg: A string containing an error on failure
function SaveMsgHexDumpToFile(pMsgHdr, pMsgBody, pNewsgroupName, pOutFilename)
{
	var retObj = {
		saveSucceeded: false,
		errorMsg: ""
	};

	if (typeof(pMsgHdr) !== "object" || typeof(pOutFilename) !== "string")
	{
		retObj.errorMsg = "Invalid parameter given";
		return retObj;
	}

	var hexLines = hexdump.generate(undefined, pMsgBody, /* ASCII: */true, /* offsets: */true);
	if (Array.isArray(hexLines) && hexLines.length > 0)
	{
		var outFilename = genPathedFilename(gSettings.msgSaveDir, pOutFilename);
		var outFile = new File(outFilename);
		if (outFile.open("w"))
		{
			// Write the message header lines
			outFile.writeln("Newsgroup: " + pNewsgroupName);
			outFile.writeln("From: " + pMsgHdr.from);
			outFile.writeln("To: " + pMsgHdr.to);
			outFile.writeln("Subject: " + pMsgHdr.subject);
			outFile.writeln("Date: " + pMsgHdr.date);
			outFile.writeln("=================================");
			// Write the hex dump
			for (var hexI = 0; hexI < hexLines.length; ++hexI)
				outFile.writeln(hexLines[hexI]);
			outFile.close();
			retObj.saveSucceeded = true;
		}
		else
			retObj.errorMsg = "File write failed";
	}
	else
		errorMsg = "Hex dump is not available";

	return retObj;
}

// For a given filename, if it has a / or is a good-looking Windows path, then use it as the full path.
// Otherwise, if the defautl directory (pDefaultDir) is not blank and exists, then use it & append
// pFilename; otherwise, treat pFilename as fully-pathed.
//
// Parameters:
//  pDefaultDir: A default directory to use to fully-path the filename
//  pFilename: A filename by itself or an absolute Path
//
// Return value: The fully-pathed filename, using either the default path or fully-pathed as specified by pFilename
function genPathedFilename(pDefaultDir, pFilename)
{
	var outFilename = "";
	const runningInWindows = /^WIN/.test(system.platform.toUpperCase());
	if (pFilename.indexOf("/") > -1 || (runningInWindows && testForWindowsPath(pFilename)))
		outFilename = pFilename;
	else if (pDefaultDir.length > 0 && file_isdir(pDefaultDir))
		outFilename = backslash(pDefaultDir) + pFilename;
	else
		outFilename = pFilename;
	return outFilename;
}

// Displays the program information.
function DisplayProgramInfo()
{
	displayTextWithLineBelow(PROGRAM_NAME, true, "\x01n\x01c\x01h", "\x01k\x01h")
	console.center("\x01n\x01cVersion \x01g" + PROGRAM_VERSION + " \x01w\x01h(\x01b" + PROGRAM_DATE + "\x01w)");
	console.crlf();

	console.pause();

	console.aborted = false;
}

// Displays help for reading a message
//
// Parameters:
//  pNewsgroupName: The name of the newsgroup currently being read
//  pScrollable: Boolean - Whether or not we're using the scrollable interface
function displayReaderHelp(pNewsgroupName, pScrollable)
{
	// TODO: Finish this
	console.clear("N");
	DisplayProgramInfo();

	console.attributes = "N";
	console.crlf();
	console.print("\x01cCurrently reading: \x01h" + pNewsgroupName);
	console.attributes = "NC";
	console.crlf();
	console.print("\x01n\x01cHotkeys");
	console.crlf();
	console.print("\x01h\x01k");
	for (var i = 0; i < 25; ++i)
		console.print(CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE);
	console.crlf();
	var keyHelpLines = [];
	if (pScrollable)
	{
		keyHelpLines.push("\x01h\x01cDown\x01g/\x01cup arrow    \x01g: \x01n\x01cScroll down\x01g/\x01cup in the message");
		keyHelpLines.push("\x01h\x01cPageUp\x01g/\x01cPageDown  \x01g: \x01n\x01cScroll up\x01g/\x01cdown a page in the message");
		keyHelpLines.push("\x01h\x01cHOME             \x01g: \x01n\x01cGo to the top of the message");
		keyHelpLines.push("\x01h\x01cEND              \x01g: \x01n\x01cGo to the bottom of the message");
	}
	keyHelpLines.push("\x01h\x01cLeft\x01g/\x01cright arrow \x01g: \x01n\x01cGo to the previous\x01g/\x01cnext message");
	keyHelpLines.push("\x01h\x01cEnter            \x01g: \x01n\x01cGo to the next message");
	//keyHelpLines.push("\x01h\x01cCtrl-U           \x01g: \x01n\x01cChange your user settings");
	if (user.is_sysop)
	{
		keyHelpLines.push("\x01h\x01cCtrl-S           \x01g: \x01n\x01cSave the message (to the BBS machine)");
		//keyHelpLines.push("\x01h\x01cCtrl-O           \x01g: \x01n\x01cShow operator menu");
		keyHelpLines.push("\x01h\x01cX                \x01g: \x01n\x01cShow message hex dump");
		keyHelpLines.push("\x01h\x01cCtrl-X           \x01g: \x01n\x01cSave message hex dump to a file");
	}
	//keyHelpLines.push("\x01h\x01cCtrl-A           \x01g: \x01n\x01cDownload attachments");
	//keyHelpLines.push("\x01h\x01c" + this.enhReaderKeys.quit + "                \x01g: \x01n\x01cQuit back to the BBS");
	keyHelpLines.push("\x01h\x01cM                \x01g: \x01n\x01cGo back to the message list");
	keyHelpLines.push("\x01h\x01cQ                \x01g: \x01n\x01cQuit to article list");
	keyHelpLines.push("\x01h\x01cCtrl-Q           \x01g: \x01n\x01cQuit out of Groupie completely");

	// Display the help text
	for (var idx = 0; idx < keyHelpLines.length; ++idx)
	{
		console.print("\x01n" + keyHelpLines[idx]);
		console.crlf();
	}

	// Pause and let the user press a key to continue.  Note: For some reason,
	// with console.pause(), not all of the message on the screen would get
	// refreshed.  So instead, we display the system's pause text and input a
	// key from the user.  Calling getKeyWithESCChars() to input a key from the
	// user to allow for multi-key sequence inputs like PageUp, PageDown, F1,
	// etc. without printing extra characters on the screen.
	//console.print("\x01n" + this.pausePromptText);
	//getKeyWithESCChars(K_NOSPIN|K_NOCRLF|K_NOECHO);
	// I'm not sure the above is needed anymore.  Should be able to use
	// console.pause(), which easily supports custom pause scripts being loaded.
	console.pause();

	console.aborted = false;
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
			theText += CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE;
		theText += "\x01n";
	}
	else
	{
		theText = textColor + pText + "\x01n\r\n";
		theText += lineColor;
		for (var i = 0; i < textLength; ++i)
			theText += CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE;
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

// Allows the user to forward a message (article) to an email address or
// another user.  This function is interactive with the user.
//
// Parameters:
//  pMsgHeader: The header of the message being forwarded
//  pMsgBody: The body text of the message
//  pNewsgroupName: The name of the newsgroup the message is from
//
// Return value: A blank string on success or a string containing a
//               message upon failure.
function ForwardMessage(pMsgHdr, pMsgBody, pNewsgroupName)
{
	if (typeof(pMsgHdr) !== "object")
		return "Invalid message header given";
	if (typeof(pMsgBody) !== "string" || pMsgBody.length == 0)
		return "Empty message body given";

	var retStr = "";

	console.attributes = "N";
	//console.crlf();
	console.print("\x01cUser name/number/email address\x01h:\x01n");
	console.crlf();
	var msgDest = console.getstr(console.screen_columns - 1, K_LINE);
	console.attributes = "N";
	console.crlf();
	if (msgDest.length > 0)
	{
		// Let the user change the subject if they want (prepend it with "Fwd: " for forwarded
		// and let the user edit the subject
		var subjPromptText = bbs.text(SubjectPrompt);
		console.putmsg(subjPromptText);
		var initialMsgSubject = (gSettings.prependFowardMsgSubject ? "Fwd: " + pMsgHdr.subject : pMsgHdr.subject);
		var msgSubject = console.getstr(initialMsgSubject, console.screen_columns - console.strlen(subjPromptText) - 1, K_LINE | K_EDIT);

		var tmpMsgbase = new MsgBase("mail");
		if (tmpMsgbase.open())
		{
			// Prepend some lines to the message body to describe where
			// the message came from originally.
			var newMsgBody = "This is a forwarded message from " + system.name + "\n";
			newMsgBody += "Forwarded by: " + user.alias;
			if (user.alias != user.name)
				newMsgBody += " (" + user.name + ")";
			newMsgBody += "\n";
			newMsgBody += "From newsgroup: " + pNewsgroupName + "\n";
			newMsgBody += "From: " + pMsgHdr.from + "\n";
			newMsgBody += "To: " + pMsgHdr.to + "\n";
			if (msgSubject == pMsgHdr.subject)
				newMsgBody += "Subject: " + pMsgHdr.subject + "\n";
			else
			{
				newMsgBody += "Subject: " + msgSubject + "\n";
				newMsgBody += "(Original subject: " + pMsgHdr.subject + ")\n";
			}
			newMsgBody += "==================================\n\n";
			newMsgBody += pMsgBody;

			// Ask whether to edit the message before forwarding it,
			// and use console.editfile(filename) to edit it.
			if (!console.noyes("Edit the message before sending"))
			{
				// TODO: Let the user edit the message, then read it
				// and set newMsgBody to it
				var tmpMsgFilename = system.temp_dir + "/GroupieMessage.txt";
				// Write the current message to the file
				var wroteMsgToTmpFile = false;
				var outFile = new File(tmpMsgFilename);
				if (outFile.open("w"))
				{
					wroteMsgToTmpFile = outFile.write(newMsgBody, newMsgBody.length);
					outFile.close();
				}
				if (wroteMsgToTmpFile)
				{
					// Let the user edit the file, and if successful,
					// read it in to newMsgBody
					if (console.editfile(tmpMsgFilename))
					{
						var inFile = new File(tmpMsgFilename);
						if (inFile.open("r"))
						{
							newMsgBody = inFile.read(inFile.length);
							inFile.close();
						}
					}
				}
				else
				{
					console.print("\x01n\x01cFailed to write message to a file for editing\x01n");
					console.crlf();
					console.pause();
				}
			}
			// End New (editing message)

			// Create part of a header object which will be used when saving/sending
			// the message.  The destination ("to" informatoin) will be filled in
			// according to the destination type.
			var destMsgHdr = { to_net_type: NET_NONE, from: user.name,
							   replyto: user.name, subject: msgSubject }; // pMsgHdr.subject
			if (user.netmail.length > 0)
				destMsgHdr.replyto_net_addr = user.netmail;
			else
				destMsgHdr.replyto_net_addr = user.email;
			//destMsgHdr.when_written_time = 
			//destMsgHdr.when_written_zone = system.timezone;
			//destMsgHdr.when_written_zone_offset = 

			var confirmedForwardMsg = true;

			// If the destination is in the format anything@anything, then
			// accept it as the message destination.  It could be an Internet
			// address (someone@somewhere.com), FidoNet address (sysop@1:327/4),
			// or a QWK address (someone@HOST).
			// We could specifically use gEmailRegex and gFTNEmailRegex to test
			// msgDest, but just using those would be too restrictive.
			if (/^.*@.*$/.test(msgDest))
			{
				confirmedForwardMsg = console.yesno("Forward via email to " + msgDest);
				if (confirmedForwardMsg)
				{
					console.print("\x01n\x01cForwarding via email to " + msgDest + "\x01n");
					console.crlf();
					destMsgHdr.to = msgDest;
					destMsgHdr.to_net_addr = msgDest;
					destMsgHdr.to_net_type = netaddr_type(msgDest);
				}
			}
			else
			{
				// See if what the user entered is a user number/name/alias
				var userNum = 0;
				if (/^[0-9]+$/.test(msgDest))
				{
					userNum = +msgDest;
					// Determine if the user entered a valid user number
					var lastUserNum = (system.lastuser == undefined ? system.stats.total_users : system.lastuser + 1);
					if ((userNum < 1) || (userNum >= lastUserNum))
					{
						userNum = 0;
						console.print("\x01h\x01y* Invalid user number (" + msgDest + ")\x01n");
						console.crlf();
					}
				}
				else // Try to get a user number assuming msgDest is a username/alias
					userNum = system.matchuser(msgDest, true);
				// If we have a valid user number, then we can forward the message.
				if (userNum > 0)
				{
					var destUser = new User(userNum);
					confirmedForwardMsg = console.yesno("Forward to " + destUser.alias + " (user " + destUser.number + ")");
					if (confirmedForwardMsg)
					{
						destMsgHdr.to = destUser.alias;
						// If the destination user has an Internet email address,
						// ask the user if they want to send to the destination
						// user's Internet email address
						var sendToNetEmail = false;
						if (destUser.netmail.length > 0)
						{
							sendToNetEmail = !console.noyes("Send to the user's Internet email (" + destUser.netmail + ")");
							if (sendToNetEmail)
							{
								console.print("\x01n\x01cForwarding to " + destUser.netmail + "\x01n");
								console.crlf();
								destMsgHdr.to = destUser.name;
								destMsgHdr.to_net_addr = destUser.netmail;
								destMsgHdr.to_net_type = NET_INTERNET;
							}
						}
						if (!sendToNetEmail)
						{
							console.print("\x01n\x01cForwarding to " + destUser.alias + "\x01n");
							console.crlf();
							destMsgHdr.to_ext = destUser.number;
							destMsgHdr.to_net_type = NET_NONE;
						}
					}
				}
				else
				{
					confirmedForwardMsg = false;
					console.print("\x01h\x01y* Unknown destination\x01n");
					console.crlf();
				}
			}
			var savedMsg = true;
			if (confirmedForwardMsg)
				savedMsg = tmpMsgbase.save_msg(destMsgHdr, newMsgBody);
			else
			{
				console.print("\x01n\x01cCanceled\x01n");
				console.crlf();
			}
			tmpMsgbase.close();

			if (!savedMsg)
			{
				console.print("\x01h\x01y* Failed to send the message!\x01n");
				console.crlf();
			}

			// Pause for user input so the user can see the messages written
			console.pause();
		}
		else
			retStr = "Failed to open email messagebase";
	}
	else
	{
		console.print("\x01n\x01cCanceled\x01n");
		console.crlf();
		console.pause();
	}

	return retStr;
}

// Displays a status message at the bottom row of the screen, pauses a moment, and then
// erases the status message.
//
// Parameters:
//  pStatusMsg: The status message to display
//  pAttributes: The Synchronet attribute codes (without SOH character(s))
//  pClearRowAfterward: Optional boolean - Whether to clear the row afterward.  Defaults to true.
function displayStatusMsgAtBottomScreenRow(pStatusMsg, pAttributes, pClearRowAfterward)
{
	console.gotoxy(1, console.screen_rows);
	console.cleartoeol("N");
	console.gotoxy(1, console.screen_rows);
	if (typeof(pAttributes) === "string")
		console.attributes = pAttributes;
	console.print(pStatusMsg);
	mswait(ERROR_PAUSE_WAIT_MS);
	var clearRowAfterward = (typeof(pClearRowAfterward) === "boolean" ? pClearRowAfterward : true);
	if (clearRowAfterward)
	{
		console.gotoxy(1, console.screen_rows);
		console.cleartoeol("N");
		console.gotoxy(1, console.screen_rows);
	}
	else
		console.attributes = "N";
}

// Displays an error message at the bottom row of the screen, pauses a moment, and then
// erases the error message.
//
// Parameters:
//  pErrorMsg: The error message to display
//  pClearRowAfterward: Optional boolean - Whether to clear the row afterward.  Defaults to true.
function displayErrorMsgAtBottomScreenRow(pErrorMsg, pClearRowAfterward)
{
	console.gotoxy(1, console.screen_rows);
	console.cleartoeol("N");
	console.gotoxy(1, console.screen_rows);
	console.print(gSettings.colors.errorMsg);
	console.print(pErrorMsg);
	mswait(ERROR_PAUSE_WAIT_MS);
	var clearRowAfterward = (typeof(pClearRowAfterward) === "boolean" ? pClearRowAfterward : true);
	if (clearRowAfterward)
	{
		console.gotoxy(1, console.screen_rows);
		console.cleartoeol("N");
		console.gotoxy(1, console.screen_rows);
	}
	else
		console.attributes = "N";
}

// Gets & returns centered program version information.
//
// Parameters:
//  pFieldWidth: The width to center the text in
function GetProgramInfoText(pFieldWidth)
{
	var versionText = "\x01n\x01cVersion \x01g" + PROGRAM_VERSION + " \x01w\x01h(\x01b" + PROGRAM_DATE + "\x01w)";
	var progInfoText = getTextWithLineBelow(PROGRAM_NAME, true, pFieldWidth, "\x01n\x01c\x01h", "\x01k\x01h");
	progInfoText += getCenteredText(versionText, pFieldWidth, false);
	progInfoText += "\x01n";
	return progInfoText;
}

// Displays the program information.
function DisplayProgramInfo()
{
	console.print(GetProgramInfoText(console.screen_columns));
	console.crlf();
}

// Displays initial welcome/help screen for first-time usage
function DisplayFirstTimeUsageHelp()
{
	DisplayProgramInfo();
	console.crlf();
	var helpText = format("Welcome to %s.  This is a news reader that connects directly to a news server.", PROGRAM_NAME);
	helpText += "At the screen to choose a newsgroup, you can press enter on a newsgroup to select it and read ";
	helpText += "articles in the newsgroup.  Also, on the screen to choose a newsgroup, you can select multiple ";
	helpText += "multiple newsgroups to subscribe to ";
	if (LightbarIsSupported())
		helpText += "by pressing the spacebar on each newsgroup, then pressing Enter to add/update your subscribed newsgroups.";
	else
		helpText += "by choosing more than one newsgroup.";
	helpText += "\r\n";
	helpText += "If you have newsgroups you have subscribed to, then the next time you run this reader, you ";
	helpText += "will be asked if you want to only see your subscribed newsgroups.  When viewing only your ";
	helpText += "subscribed newsgroups, you can press DEL to clear your list of subscribed newsgroups.";
	console.attributes = "C";
	console.print(lfexpand(word_wrap(helpText, console.screen_columns-1, false)));
	console.attributes = "N";
	console.pause();
	if (console.aborted)
		console.aborted = false; // To prevent issues that may arise from console.aborted being true
}

// Displays help for choosing a newsgroup
function DisplayChooseNewsgroupHelp()
{
	var helpText = GetProgramInfoText(console.screen_columns);
	helpText += "\r\n";
	helpText += "\r\n";
	helpText += "\x01n" + gSettings.colors.helpText;
	helpText += "On this screen, you can choose a newsgroup. You can choose one newsgroup from ";
	helpText += "the list to read articles or subscribe to newsgroups ";
	if (LightbarIsSupported())
		helpText += "by pressing the spacebar on each newsgroup, then pressing Enter to add/update your subscribed newsgroups.  ";
	else
		helpText += "by choosing more than one newsgroup.  ";
	helpText += "You can press Q or ESC to quit.\r\n";
	helpText += "\r\n";
	//helpText += "Hotkeys\r\n";
	helpText += "\x01n\x01w\x01hHotkeys:\x01n\r\n";
	var formatStr = "\x01n\x01c\x01h%-17s\x01g: \x01n\x01c%s\r\n";
	if (LightbarIsSupported())
	{
		helpText += format(formatStr, "Up arrow", "Move the cursor up");
		helpText += format(formatStr, "Down arrow", "Move the cursor down");
		helpText += format(formatStr, "PageUp", "Move up one page");
		helpText += format(formatStr, "PageDown", "Move down one page");
		helpText += format(formatStr, "Home", "Go to the top of the list");
		helpText += format(formatStr, "End", "Go to the end of the list");
		helpText += format(formatStr, "Spacebar", "Select newsgroup (and press Enter when done to subscribe)");
	}
	helpText += format(formatStr, "/", "Search for newsgroup");
	helpText += format(formatStr, "ESC, Q, or Ctrl-Q", "Quit");
	helpText += format(formatStr, "?", "Display help");
	console.print(lfexpand(word_wrap(helpText, console.screen_columns-1, false)));

	console.attributes = "N";
	console.pause();
	if (console.aborted)
		console.aborted = false; // To prevent issues that may arise from console.aborted being true
}

// Displays help for listing articles
function DisplayListingArticleHelp(pNewsgroupName)
{
	var helpText = GetProgramInfoText(console.screen_columns);
	helpText += "\r\n";
	helpText += "\r\n";
	helpText += "\x01n" + gSettings.colors.helpText;
	helpText += "This screen lists the articles. You can choose an article from the list or ";
	helpText += "press Q or ESC to quit.\r\n";
	helpText += format("Currently listing articles in %s\r\n", pNewsgroupName);
	helpText += "\r\n";
	//helpText += "Hotkeys\r\n";
	helpText += "\x01n\x01w\x01hHotkeys:\x01n\r\n";
	var formatStr = "\x01n\x01c\x01h%-17s\x01g: \x01n\x01c%s\r\n";
	if (LightbarIsSupported())
	{
		helpText += format(formatStr, "Up arrow", "Move the cursor up");
		helpText += format(formatStr, "Down arrow", "Move the cursor down");
		helpText += format(formatStr, "PageUp", "Move up one page");
		helpText += format(formatStr, "PageDown", "Move down one page");
		helpText += format(formatStr, "Home", "Go to the top of the list");
		helpText += format(formatStr, "End", "Go to the end of the list");
	}
	helpText += format(formatStr, "Number", "Read an article by number");
	helpText += format(formatStr, "P", "Post a message");
	helpText += format(formatStr, "Ctrl-U", "User stetings (i.e., max # articles)");
	helpText += format(formatStr, "Q or ESC", "Quit back to newsgroup list");
	helpText += format(formatStr, "Ctrl-Q", "Quit out of Groupie completely");
	helpText += format(formatStr, "?", "Display help");
	console.print(lfexpand(word_wrap(helpText, console.screen_columns-1, false)));

	console.attributes = "N";
	console.pause();
	if (console.aborted)
		console.aborted = false; // To prevent issues that may arise from console.aborted being true
}

// Converts a program state to a name (string)
function ProgramStateToName(pState)
{
	var stateName = "";
	switch (pState)
	{
		case STATE_CHOOSING_NEWSGROUP:
			stateName = "Choosing a newsgroup";
			break;
		case STATE_LISTING_ARTICLES_IN_NEWSGROUP:
			stateName = "Listing articles in newsgroup";
			break;
		case STATE_POSTING:
			stateName = "Posting a message in a newsgroup";
			break;
		case STATE_EXIT:
			stateName = "Exit";
			break;
		default:
			stateName = "Unknown";
			break;
	}
	return stateName;
}

// Given a filename without extension, this looks for an .ans or .asc version of the file.
// Also, this will first look for versions of the file with a filename that has "-"
// followed by terminal width in the name, so that you can use files that match various
// terminal widths. For example, given a filename base of "ListHdr", if the user's terminal
// width is 132, this will first look for "ListHdr-132.ans" or "ListHdr-132.asc"; if not
// found, this will just look for ListHdr.ans or ListHdr.asc.
//
// Parameters:
//  pDirName: The directory to look in
//  pFilenameBase: The filename without extension
//
// Return value: The fully-pathed filename, if found, or an empty string if not.
function findANSOrAscVersionOfFileInDirectory(pDirName, pFilenameBase)
{
	if (typeof(pDirName) !== "string" || pDirName.length == 0 || typeof(pFilenameBase) !== "string" || pFilenameBase.length == 0)
		return "";

	var fullyPathedFilename = "";

	var txtFilenameBaseFullPath = backslash(pDirName) + pFilenameBase;
	if (file_exists(txtFilenameBaseFullPath + "-" + console.screen_columns + ".ans"))
		fullyPathedFilename = txtFilenameBaseFullPath + "-" + console.screen_columns + ".ans";
	else if (file_exists(txtFilenameBaseFullPath + "-" + console.screen_columns + ".asc"))
		fullyPathedFilename = txtFilenameBaseFullPath + "-" + console.screen_columns + ".asc";
	else if (file_exists(txtFilenameBaseFullPath + ".ans"))
		fullyPathedFilename = txtFilenameBaseFullPath + ".ans";
	else if (file_exists(txtFilenameBaseFullPath + ".asc"))
		fullyPathedFilename = txtFilenameBaseFullPath + ".asc";

	return fullyPathedFilename;
}

// This looks for an .ans or .asc version of a file in a subdirectory of this script's
// directory.  This will first look for versions of the file with a filename that has "-"
// followed by terminal width in the name, so that you can use files that match various
// terminal widths. For example, given a filename base of "ListHdr", if the user's terminal
// width is 132, this will first look for "ListHdr-132.ans" or "ListHdr-132.asc"; if not
// found, this will just look for ListHdr.ans or ListHdr.asc.
//
// If the file isn't found in the subdirectory with the given name, this will then look
// in the same directory as this script.
//
// Parameters:
//  pFilenameBase: The filename without extension
//  pSubdirectoryNameInScriptDir: The name of a subdirectory in this script's directory to look in
//
// Return value: The fully-pathed filename, if found, or an empty string if not.
function findANSOrASCVersionOfFile(pFilenameBase, pSubdirectoryNameInScriptDir)
{
	if (typeof(pFilenameBase) !== "string" || pFilenameBase.length == 0)
		return "";

	var fullyPathedFilename = "";

	// First, look in the subdirectory in the script directory specified by
	// pSubdirectoryNameInScriptDir (if exists)
	if (pSubdirectoryNameInScriptDir.length > 0)
		fullyPathedFilename = findANSOrAscVersionOfFileInDirectory(js.exec_dir + pSubdirectoryNameInScriptDir, pFilenameBase);

	// Look in the same directory as the script
	if (fullyPathedFilename.length == 0)
		fullyPathedFilename = findANSOrAscVersionOfFileInDirectory(js.exec_dir, pFilenameBase);

	return fullyPathedFilename;
}

// Loads a text file (an .ans or .asc) into an array.
//
// Parameters:
//  pFullyPathedFilename: The fully-pathed filename to load
//  pMaxNumLines: Optional - The maximum number of lines to load from the text file
//
// Return value: An array containing the lines from the text file
function loadAnsOrAscFileIntoArray(pFullyPathedFilename, pMaxNumLines)
{
	if (typeof(pFullyPathedFilename) !== "string")
		return [];
	if (!file_exists(pFullyPathedFilename))
		return [];

	var maxNumLines = (typeof(pMaxNumLines) === "number" ? pMaxNumLines : -1);

	var filename = pFullyPathedFilename;

	// If the user's console doesn't support ANSI and the header file is ANSI,
	// then convert it to Synchronet attribute codes and read that file instead.
	if (!console.term_supports(USER_ANSI) && getStrAfterPeriod(pFullyPathedFilename).toUpperCase() == "ANS")
	{
		var dotIdx = pFullyPathedFilename.lastIndexOf(".");
		if (dotIdx >= 0)
		{
			filename = system.temp_dir + file_getname(pFullyPathedFilename) + "_converted.asc";
			var filenameBase = pFullyPathedFilename.substr(0, dotIdx);
			var cmdLine = system.exec_dir + "ans2asc \"" + pFullyPathedFilename + "\" \""
						+ filename + "\"";
			// Note: Both system.exec(cmdLine) and
			// bbs.exec(cmdLine, EX_NATIVE, js.exec_dir) could be used to
			// execute the command, but system.exec() seems noticeably faster.
			system.exec(cmdLine);
		}
		else
			filename = "";
	}

	// Read the file
	var txtFileLines = [];
	var textFile = new File(filename);
	if (textFile.open("r"))
	{
		var fileLine = null;
		while (!textFile.eof)
		{
			// Read the next line from the header file.
			fileLine = textFile.readln(2048);
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
			if (maxNumLines > -1 && txtFileLines.length == maxNumLines)
				break;
		}
		textFile.close();
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

// Displays header lines
//
// Parameters:
//  pHdrLineArray: The array of header lines to display
//  pScreenRow: The starting screen row to display the header lines at
//
function DisplayHdrLines(pHdrLineArray, pScreenRow)
{
	var screenRow = (pScreenRow >= 1 && pScreenRow <= console.screen_rows ? pScreenRow : 1);

	console.attributes = "N";
	for (var i = 0; i < pHdrLineArray.length; ++i)
	{
		console.gotoxy(1, screenRow+i);
		console.putmsg(pHdrLineArray[i]);
	}
	console.attributes = "N";
}

// Lets the user edit their user Settings
//
// Parameters:
//  pServerName: The name of the newsgroup server that the user/client is connected to
//  pNewsgroupName: The name of the newsgroup being read (if applicable)
//  pTopRowOverride: Optional - The row on the screen for the top line of the settings dialog.
//                   If not specified, then 1 below the top of the message area (for reader mode)
//                   will be used.
//  pLightbarKeyHelpLine: For lightbar mode, this is the key help line currently being displayed
//                        at the bottom of the screen, to be refreshed when necessary
function DoUserSettings(pServerName, pNewsgroupName, pTopRowOverride, pLightbarKeyHelpLine)
{
	var retObj = null;
	if (LightbarIsSupported())
		retObj = DoUserSettings_Scrollable(pServerName, pNewsgroupName, pTopRowOverride, pLightbarKeyHelpLine);
	else
		retObj = DoUserSettings_Traditional(pServerName, pNewsgroupName);
	return retObj;
}

// Lets the user edit their user Settings (scrollable/ANSI/lightbar version)
//
// Parameters:
//  pServerName: The name of the newsgroup server that the user/client is connected to
//  pNewsgroupName: The name of the newsgroup being read (if applicable)
//  pTopRowOverride: Optional - The row on the screen for the top line of the settings dialog.
//                   If not specified, then 1 below the top of the message area (for reader mode)
//                   will be used.
//  pLightbarKeyHelpLine: For lightbar mode, this is the key help line currently being displayed
//                        at the bottom of the screen, to be refreshed when necessary
//
// Return value: An object containing the following properties:
//               needWholeScreenRefresh: Boolean - Whether or not the whole screen needs to be
//                                       refreshed (i.e., when the user has edited their twitlist)
//               optionBoxTopLeftX: The top-left screen column of the option box
//               optionBoxTopLeftY: The top-left screen row of the option box
//               optionBoxWidth: The width of the option box
//               optionBoxHeight: The height of the option box
function DoUserSettings_Scrollable(pServerName, pNewsgroupName, pTopRowOverride, pLightbarKeyHelpLine)
{
	var retObj = GetDoUserSettingsDefaultRetObj();

	if (!LightbarIsSupported())
	{
		DoUserSettings_Scrollable(pServerName, pNewsgroupName, pTopRowOverride);
		return retObj;
	}

	// Ensure gUserUsageInfo has properties for the current server & newsgroup
	if (!gUserUsageInfo.servers.hasOwnProperty(pServerName))
	{
		gUserUsageInfo.servers[pServerName] = {
			"newsgroups": {}
		};
	}
	if (!gUserUsageInfo.servers[pServerName].newsgroups.hasOwnProperty(pNewsgroupName))
	{
		gUserUsageInfo.servers[pServerName].newsgroups[pNewsgroupName] = GetDefaultUserUsageNewsgroupInfoObj();
	}

	// Save the user's current settings so that we can check them later to see if any
	// of them changed, in order to determine whether to save the user's settings file.
	var originalSettings = copyObj(gUserSettings);
	var originalNewsgroupUsageInfo = copyObj(gUserUsageInfo);
	var newsgroupUsageSettingsChanged = false;

	// Create the user settings box
	var optBoxTitle = "Setting                                      Enabled";
	var optBoxWidth = ChoiceScrollbox_MinWidth();
	var optBoxHeight = 4;
	var optBoxTopRow = 1;
	if (typeof(pTopRowOverride) === "number" && pTopRowOverride >= 1 && pTopRowOverride <= console.screen_rows - optBoxHeight + 1)
		optBoxTopRow = pTopRowOverride;
	var optBoxStartX = Math.floor((console.screen_columns/2) - (optBoxWidth/2));
	if (optBoxStartX < 0)
		optBoxStartX = 0;
	var optionBox = new ChoiceScrollbox(optBoxStartX, optBoxTopRow, optBoxWidth, optBoxHeight, optBoxTitle,
	                                    null, false, true);
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

	// User settings:

	// Create an object containing toggle values (true/false) for each option index
	var optionToggles = {};
	//optionToggles[MAX_NUM_MSGS_FOR_THIS_NEWSGROUP_OPT_INDEX] = gUserSettings.option;

	// Other actions
	var MAX_NUM_MSGS_FOR_THIS_NEWSGROUP_OPT_INDEX  = optionBox.addTextItem("Maximum # msgs for this newsgorup");

	// Set up the enter key in the box to toggle the selected item.
	optionBox.readerObj = this;
	optionBox.setEnterKeyOverrideFn(function(pBox) {
		var itemIndex = pBox.getChosenTextItemIndex();
		if (itemIndex > -1)
		{
			// If there's an option for the chosen item, then update the text on the
			// screen depending on whether the option is enabled or not.
			if (optionToggles.hasOwnProperty(itemIndex))
			{
				/*
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
					case MSG_LIST_SELECT_MSG_MOVES_TO_NEXT_MSG_OPT_INDEX:
						this.readerObj.userSettings.selectInMsgListMovesToNext = !this.readerObj.userSettings.selectInMsgListMovesToNext;
						break;
					default:
						break;
				}
				*/
			}
			// For options that aren't on/off toggle options, take the appropriate action.
			else
			{
				switch (itemIndex)
				{
					case MAX_NUM_MSGS_FOR_THIS_NEWSGROUP_OPT_INDEX:
						// Move the cursor to the bottom of the screen and
						// prompt the user for the maximum number of articles
						// to download in this newsgroup
						console.gotoxy(1, console.screen_rows);
						console.cleartoeol("N");
						console.gotoxy(1, console.screen_rows);
						// We only have one setting in here, so the logic for
						// newsgroupUsageSettingsChanged is okay:
						newsgroupUsageSettingsChanged = PromptForAndSetPreferredMaxNumArticlesForNewsgroup(pServerName, pNewsgroupName);
						// Refresh the key help line at the bottom of the screen if applicable
						if (typeof(pLightbarKeyHelpLine) === "string" && pLightbarKeyHelpLine.length > 0 && LightbarIsSupported())
						{
							console.attributes = "N";
							console.gotoxy(1, console.screen_rows);
							console.putmsg(pLightbarKeyHelpLine);
							console.attributes = "N";
						}

						retObj.refreshKeyHelpLine = false;
						break;
					default:
						break;
				}
			}
		}
	}); // Option box enter key override function

	// Display the option box and have it do its input loop
	var boxRetObj = optionBox.doInputLoop(true);
	if (console.aborted)
		console.aborted = false;
	console.line_counter = 0; // To prevent a pause before the message list comes up

	// If the user changed any of their settings, then save the user settings.
	// If the save fails, then output an error message.
	var userSettingsChanged = false;
	for (var prop in gUserSettings)
	{
		if (originalSettings.hasOwnProperty(prop))
		{
			userSettingsChanged = userSettingsChanged || (originalSettings[prop] != gUserSettings[prop]);
			if (userSettingsChanged)
				break;
		}
	}

	// If any of the user's newsgroup usage settings changed, then save the user's newsgroup settings JSON file
	if (newsgroupUsageSettingsChanged)
		SaveUserNewsgroupUsageInfoToFile();

	// Prepare return object values and return
	retObj.optionBoxTopLeftX = optionBox.dimensions.topLeftX;
	retObj.optionBoxTopLeftY = optionBox.dimensions.topLeftY;
	retObj.optionBoxWidth = optionBox.dimensions.width;
	retObj.optionBoxHeight = optionBox.dimensions.height;
	return retObj;
}

// Lets the user edit their user Settings (traditional/non-lightbar version)
//
// Parameters:
//  pServerName: The name of the newsgroup server that the user/client is connected to
//  pNewsgroupName: The name of the newsgroup being read (if applicable)
function DoUserSettings_Traditional(pServerName, pNewsgroupName)
{
	var retObj = GetDoUserSettingsDefaultRetObj();

	var optNum = 1;
	var MAX_NUM_MSGS_FOR_THIS_NEWSGROUP_OPT_NUM = optNum++;
	var HIGHEST_CHOICE_NUM = MAX_NUM_MSGS_FOR_THIS_NEWSGROUP_OPT_NUM; // Highest choice number

	console.crlf();
	var wordFirstCharAttrs = "\x01c\x01h";
	var wordRemainingAttrs = "\x01c";
	console.print(colorFirstCharAndRemainingCharsInWords("User Settings", wordFirstCharAttrs, wordRemainingAttrs) + "\r\n");
	printTradUserSettingOption(MAX_NUM_MSGS_FOR_THIS_NEWSGROUP_OPT_NUM, "Maximum number of messages for this newsgroup", wordFirstCharAttrs, wordRemainingAttrs);
	console.crlf();
	console.print("\x01cYour choice (\x01hQ\x01n\x01c: Quit)\x01h: \x01g");
	var userChoiceNum = console.getnum(HIGHEST_CHOICE_NUM);
	console.attributes = "N";
	var userChoiceStr = userChoiceNum.toString().toUpperCase();
	if (userChoiceStr.length == 0 || userChoiceStr == "Q")
	{
		if (console.aborted)
			console.aborted = false;
		return retObj;
	}

	var userSettingsChanged = false;
	switch (userChoiceNum)
	{
		case MAX_NUM_MSGS_FOR_THIS_NEWSGROUP_OPT_NUM:
			var previousMaxNumMsgs = gUserUsageInfo.servers[pServerName].newsgroups[pNewsgroupName].preferredMaxNumArticles;
			// For now, there's only one setting here, so this logic for userSettingsChanged
			// is okay:
			userSettingsChanged = PromptForAndSetPreferredMaxNumArticlesForNewsgroup(pServerName, pNewsgroupName);
			break;
	}

	// If any user settings changed, then save the user's newsgroup settings JSON file
	if (userSettingsChanged)
		SaveUserNewsgroupUsageInfoToFile();

	retObj.needWholeScreenRefresh = true;
	return retObj;
}
// Helper for DigDistMsgReader_DoUserSettings_Traditional: Returns a string where for each word,
// the first letter will have one set of Synchronet attributes applied and the remainder of the word
// will have another set of Synchronet attributes applied
function colorFirstCharAndRemainingCharsInWords(pStr, pWordFirstCharAttrs, pWordRemainderAttrs)
{
	if (typeof(pStr) !== "string" || pStr.length == 0)
		return "";
	if (typeof(pWordFirstCharAttrs) !== "string" || typeof(pWordRemainderAttrs) !== "string")
		return pStr;

	var wordsArray = pStr.split(" ");
	for (var i = 0; i < wordsArray.length; ++i)
	{
		if (wordsArray[i] != " ")
			wordsArray[i] = "\x01n" + pWordFirstCharAttrs + wordsArray[i].substr(0, 1) + "\x01n" + pWordRemainderAttrs + wordsArray[i].substr(1);
	}
	return wordsArray.join(" ");
}
// Helper for DigDistMsgReader_DoUserSettings_Traditional: Returns a string where for each word,
// the first letter will have one set of Synchronet attributes applied and the remainder of the word
// will have another set of Synchronet attributes applied
function printTradUserSettingOption(pOptNum, pStr, pWordFirstCharAttrs, pWordRemainderAttrs)
{
	printf("\x01c\x01h%d\x01g: %s\r\n", pOptNum, colorFirstCharAndRemainingCharsInWords(pStr, pWordFirstCharAttrs, pWordRemainderAttrs));
}

// Prompts the user for a preferred maximum number of messages for a particular newsgroup
//
// Parameters:
//  pServerName: The server name/IP address
//  pNewsgroupName: The newsgroup name
//
// Return value: Boolean - Whether or not the user's maxinum number of articles for the newsgroup changed
function PromptForAndSetPreferredMaxNumArticlesForNewsgroup(pServerName, pNewsgroupName)
{
	var maxNumArticlesChanged = false;

	console.attributes = "N";
	var promptText = format("\x01c\x01hMaximum number of articles to download\x01g: \x01n");
	console.print(promptText);
	var defaultInputVal = 0;
	if (gUserUsageInfo.servers.hasOwnProperty(pServerName) &&
		gUserUsageInfo.servers[pServerName].hasOwnProperty("newsgroups") &&
		gUserUsageInfo.servers[pServerName].newsgroups.hasOwnProperty(pNewsgroupName) &&
		gUserUsageInfo.servers[pServerName].newsgroups[pNewsgroupName].hasOwnProperty("preferredMaxNumArticles"))
	{
		defaultInputVal = gUserUsageInfo.servers[pServerName].newsgroups[pNewsgroupName].preferredMaxNumArticles;
	}
	else
		defaultInputVal = gSettings.defaultMaxNumMsgsFromNewsgroup;
	var numStr = console.getstr(defaultInputVal.toString(), console.screen_columns - console.strlen(promptText) - 1, K_EDIT | K_LINE | K_TRIM | K_NOSPIN | K_NUMBER | K_NOCRLF);
	console.attributes = "N";
	var numMsgs = parseInt(numStr);
	if (!isNaN(numMsgs) && numMsgs > 0)
	{
		maxNumArticlesChanged = (gUserUsageInfo.servers[pServerName].newsgroups[pNewsgroupName].preferredMaxNumArticles != numMsgs);
		gUserUsageInfo.servers[pServerName].newsgroups[pNewsgroupName].preferredMaxNumArticles = numMsgs;
	}

	return maxNumArticlesChanged;
}

// Saves the user's newsgroup usage information to their JSON file
function SaveUserNewsgroupUsageInfoToFile()
{
	var saveSucceeded = false;

	var userUsageFile = new File(gUserUsageInfoFilename);
	if (userUsageFile.open("w"))
	{
		userUsageFile.write(JSON.stringify(gUserUsageInfo));
		userUsageFile.close();
		saveSucceeded = true;
	}
	else
		log(LOG_ERR, format("* %s: Failed to save user usage information to %s", PROGRAM_NAME, gUserUsageInfoFilename));

	return saveSucceeded;
}

// Gets a default return object for the DoUserSettings() functions
function GetDoUserSettingsDefaultRetObj()
{
	return {
		needWholeScreenRefresh: false,
		refreshKeyHelpLine: false,
		maxNumArticlesForNewsgroupChanged: false,
		optionBoxTopLeftX: 1,
		optionBoxTopLeftY: 1,
		optionBoxWidth: 0,
		optionBoxHeight: 0
	};
}

// Deep-copies an object
function copyObj(pSrcObj)
{
	var destObj = {};
	if (destObj && pSrcObj)
	{
		for (var prop in pSrcObj)
		{
			if (pSrcObj.hasOwnProperty(prop))
			{
				if (destObj[prop] && typeof pSrcObj[prop] === 'destObject')
					destObj[prop] = setVals(destObj[prop], pSrcObj[prop]);
				else
					destObj[prop] = pSrcObj[prop];
			}
		}
	}
	return destObj;
}
