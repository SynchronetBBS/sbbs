/*
This is a trivia game for Synchronet.  It currently has a basic Q&A format, and supports multiple
trivia categories.  User's high scores and trivia category stats (total score, last time played)
are saved to a file.

Date       Author            Description
2022-11-18 Eric Oulashin     Version 1.00
2022-11-25 Eric Oulashin     Version 1.01
                             Added the ability to store & retrieve scores to/from a server,
                             so that scores from multiple BBSes can be displayed.  There are
                             also sysop functions to remove players and users from the hosted
                             inter-BBS scores. Also, answer clues now don't mask spaces in the
                             answer.
2022-12-08 Eric Oulashin     Version 1.02
                             The game can now post scores in (networked) message sub-boards as
                             a backup to using a JSON DB server in case the server can't be
                             contacted.
2023-01-03 Eric Oulashin     Version 1.03 beta
                             Started working on allowing Q&A files to have a section of JSON
                             metadata, and also for its answers to possibly be a section of
                             JSON containing multiple possible answers. JSON metadata in
                             a QA file may have the following properties (all optional):
                             category_name: The name of the category
                             ARS: An ARS string that can restrict usage of the category
                             "-- Answer metadata begin"/"-- Answer metadata end" sections
                             need to have an "answers" property, which is an array of
                             acceptable answers (as strings).  It can also optionally have an
                             "answerFact" property, to specify an interesting fact about
                             the answer.
                             Fixed a bug in reading local scores and parsing them, which
                             affected saving local scores and showing local scores.
2023-01-14 Eric Oulashin     Version 1.03
                             Releasing this version
2024-03-26 Eric Oulashin     Version 1.04
                             Formatting fix for sysop menu when the server scores file is missing.
                             Allow showing help when playing a game by entering ?
2024-03-29 Eric Oulashin     Version 1.05
                             Ensure the correct script startup directory is always used (make
							 a copy of js.exec_dir on startup, since js.exec_dir could change)
*/

"use strict";


// Game name - For logging, etc.
var GAME_NAME = "Good Time Trivia";

// This script requires Synchronet version 3.15 or higher (for user.is_sysop).
// Exit if the Synchronet version is below the minimum.
if (system.version_num < 31500)
{
	console.print("\x01n");
	console.crlf();
	console.print("\x01n\x01h\x01y\x01i* Warning:\x01n\x01h\x01w " + GAME_NAME);
	console.print(" " + "requires version \x01g3.15\x01w or");
	console.crlf();
	console.print("higher of Synchronet.  This BBS is using version \x01g");
	console.print(system.version + "\x01w.  Please notify the sysop.");
	console.crlf();
	console.pause();
	exit(1);
}

// Version information
var GAME_VERSION = "1.05";
var GAME_VER_DATE = "2024-03-29";

// Load required .js libraries
var requireFnExists = (typeof(require) === "function");
if (requireFnExists)
{
	require("sbbsdefs.js", "P_NONE");
	require("cp437_defs.js", "CP437_BOX_DRAWINGS_UPPER_LEFT_SINGLE");
	require("json-client.js", "JSONClient");
	require(js.exec_dir + "lib.js", "getJSONSvcPortFromServicesIni");
}
else
{
	load("sbbsdefs.js");
	load("cp437_defs.js");
	load("json-client.js");
	load(js.exec_dir + "lib.js");
}

// Make a copy of js.exec_dir.  js.exec_dir could be changed indirectly
// due to another JS script running (for instance, via console.yesno(),
// which would run yesnobar.js in sbbs/exec).
const gScriptExecDir = js.exec_dir;




// Maximum Levenshtein distance (inclusive) to consisder an answer matching (when appropriate)
var MAX_LEVENSHTEIN_DISTANCE = 2;
// Scores filename
var SCORES_FILENAME = gScriptExecDir + "scores.json";
// Semaphore filename to use when saving the user's score to try to prevent multiple instances
// from overwriting the score on each other
var SCORES_SEMAPHORE_FILENAME = gScriptExecDir + "SCORES_SEMAPHORE.tmp";
// Main menu actions
var ACTION_PLAY = 0;
var ACTION_SHOW_HELP_SCREEN = 1;
var ACTION_SHOW_HIGH_SCORES = 2;
var ACTION_QUIT = 3;
var ACTION_SYSOP_MENU = 4;


// Values for JSON DB reading and writing
var JSON_DB_LOCK_READ = 1;
var JSON_DB_LOCK_WRITE = 2;
var JSON_DB_LOCK_UNLOCK = -1;


// Upon exit for any reason, make sure the scores semaphore filename doesn't exist so future instances don't get frozen
//js.on_exit("if (file_exists(\"" + SCORES_SEMAPHORE_FILENAME + "\")) file_remove(\"" + SCORES_SEMAPHORE_FILENAME + "\");");



// Load the settings from the .ini file
var gSettings = loadSettings();

// Parse command-line arguments
var gCmdLineArgs = parseCmdLineArgs(argv);

// If the command-line argument was specified to post or read scores in the configured message
// sub-board, then do so and exit.
if (gCmdLineArgs.postScoresToSubBoard)
{
	if (gSettings.behavior.scoresMsgSubBoardsForPosting.length == 0)
	{
		log(LOG_ERR, format("%s - Post scores to sub-boards specified, but scoresMsgSubBoardsForPosting is not set", GAME_NAME));
		exit(2);
	}

	var exitCode = 0;
	for (var i = 0; i < gSettings.behavior.scoresMsgSubBoardsForPosting.length; ++i)
	{
		var subCode = gSettings.behavior.scoresMsgSubBoardsForPosting[i];
		if (!msg_area.sub.hasOwnProperty(subCode))
		{
			log(LOG_ERR, format("%s - Sub-board-code %s does not exist (specified in %s)", GAME_NAME, subCode, "scoresMsgSubBoardsForPosting"));
			exitCode = 3;
			continue;
		}

		var postSuccessful = postGTTriviaScoresToSubBoard(subCode);
		// For logging
		var subBoardInfoStr = msg_area.sub[subCode].name + " - " + msg_area.sub[subCode].description;
		// Write the status to the log
		var logMsg = "";
		var logLevel = LOG_INFO;
		if (postSuccessful)
			logMsg = format("%s - Successfully posted local scores to sub-board %s (%s)", GAME_NAME, subCode, subBoardInfoStr);
		else
		{
			logLevel = LOG_ERR;
			logMsg = format("%s - Posting scores to sub-board %s (%s) failed!", GAME_NAME, subCode, subBoardInfoStr);
			exitCode = 3;
		}
		log(logLevel, logMsg);
	}
	exit(exitCode);
}
else if (gCmdLineArgs.readScoresFromSubBoard)
{
	if (gSettings.server.scoresMsgSubBoardsForReading.length == 0)
	{
		log(LOG_ERR, format("%s - Read scores from sub-boards specified, but scoresMsgSubBoardsForReading is not set", GAME_NAME));
		exit(2);
	}

	var exitCode = 0;
	for (var i = 0; i < gSettings.server.scoresMsgSubBoardsForReading.length; ++i)
	{
		var subCode = gSettings.server.scoresMsgSubBoardsForReading[i];
		if (subCode.length == 0 || !msg_area.sub.hasOwnProperty(subCode))
		{
			log(LOG_ERR, format("%s - Invalid sub-board code specified in scoresMsgSubBoardsForReading: %s", GAME_NAME, subCode));
			continue;
		}

		var readSuccessful = readGTTriviaScoresFromSubBoard(subCode);
		// For logging
		var subBoardInfoStr = msg_area.sub[subCode].name + " - " + msg_area.sub[subCode].description;
		// Write the status to the log
		var logMsg = "";
		var logLevel = LOG_INFO;
		if (readSuccessful)
			logMsg = format("%s - Successfully read scores from sub-board %s (%s)", GAME_NAME, subCode, subBoardInfoStr);
		else
		{
			logLevel = LOG_ERR;
			logMsg = format("%s - Reading scores from sub-board %s (%s) failed!", GAME_NAME, subCode, subBoardInfoStr);
			exitCode = 4;
		}
		log(logLevel, logMsg);
	}
	exit(exitCode);
}


// Display the program logo
displayProgramLogo(true, false);

//console.clear("\x01n");

// In a loop until the user chooses to exit, show the main menu and let the user make
// a choice of what to do
var continueOn = true;
while (continueOn)
{
	switch (doMainMenu())
	{
		case ACTION_PLAY:
			playTrivia();
			break;
		case ACTION_SHOW_HELP_SCREEN:
			showHelpScreen();
			break;
		case ACTION_SHOW_HIGH_SCORES:
			showScores();
			break;
		case ACTION_SYSOP_MENU:
			if (user.is_sysop)
				doSysopMenu();
			break;
		case ACTION_QUIT:
		default:
			continueOn = false;
			break;
	}
}


console.print("\x01n\x01cReturning to \x01y\x01h" + system.name + "\x01n\x01c...\x01n");
mswait(500);
// End of script execution.



////////////////////////////////////////////////////
// Functions

// Lets the user play trivia (choosing a category if there is more than one)
function playTrivia()
{
	// Load the Q&A filenames and let the user choose a section to play
	var qaFilenameInfo = getQACategoriesAndFilenames();
	if (qaFilenameInfo.length == 0)
	{
		console.attributes = "N" + gSettings.colors.error;
		console.print("There are no trivia sections available!\x01n");
		console.crlf();
		console.pause();
		return 2;
	}
	// If there's only 1 trivia category, then choose it automatically.  Otherwise,
	// let the user choose a trivia category.
	var chosenSectionIdx = 0;
	if (qaFilenameInfo.length > 1)
	{
		console.attributes = "N" + gSettings.colors.triviaCategoryHdr;
		console.print("Choose a trivia section:\x01n");
		console.crlf();
		for (var i = 0; i < qaFilenameInfo.length; ++i)
		{
			var categoryNum = i + 1;
			var categoryNumStr = format(attrCodeStr(gSettings.colors.triviaCategoryListNumbers) + "%3d", i + 1);
			var categorySep = attrCodeStr(gSettings.colors.triviaCategoryListSeparator) + ": ";
			var categoryName = attrCodeStr(gSettings.colors.triviaCategoryName) + qaFilenameInfo[i].sectionName;
			console.print("\x01n" + categoryNumStr + "\x01n" + categorySep + "\x01n" + categoryName);
			console.crlf();
		}
		console.attributes = "N" + gSettings.colors.categoryNumPrompt;
		console.print("Choose\x01n");
		console.attributes = gSettings.colors.categoryNumPromptSeparator;
		console.print(": \x01n");
		console.attributes = gSettings.colors.categoryNumInput;
		var chosenSectionNum = +(console.getnum(qaFilenameInfo.length));
		if (chosenSectionNum < 1 || chosenSectionNum > qaFilenameInfo.length)
		{
			//console.print("\x01n" + attrCodeStr(gSettings.colors.error) + "That is not a valid section number\x01n");
			//console.print("\x01n\x01cReturning to \x01y\x01h" + system.name + "\x01n\x01c...\x01n");
			//mswait(1500);
			return 0;
		}
		chosenSectionIdx = chosenSectionNum - 1;
	}

	console.attributes = "N" + gSettings.colors.triviaCategoryHdr;
	console.print("Playing ");
	console.attributes = gSettings.colors.triviaCategoryName;
	console.print(qaFilenameInfo[chosenSectionIdx].sectionName + "\x01n");
	console.crlf();

	// Load and parse the section filename into questions, answers, and points
	var QAArray = parseQAFile(qaFilenameInfo[chosenSectionIdx].filename);
	shuffle(QAArray);
	console.print("There are " + add_commas(QAArray.length, 0) + " questions in total.");
	console.crlf();
	// Each element in QAArray is an object with the following properties:
	// question
	// answer
	// numPoints
	console.print("\x01n\x01gWill ask up to \x01h" + gSettings.behavior.numQuestionsPerPlay + "\x01n\x01g questions.\x01n");
	console.crlf();
	console.print("\x01n\x01gYou can answer \x01hQ\x01n\x01g at any time to quit.\x01n");
	console.crlf();
	var userPoints = 0;
	var maxNumQuestions = gSettings.behavior.numQuestionsPerPlay;
	if (QAArray.length < maxNumQuestions)
		maxNumQuestions = QAArray.length;
	var continueQuestioning = true;
	var questionColor = attrCodeStr(gSettings.colors.question);
	var questionHdrColor = attrCodeStr(gSettings.colors.questionHdr);
	var questionHdrNumColor = attrCodeStr(gSettings.colors.questionHdrNum);
	var answerWhenIncorrectColor = attrCodeStr(gSettings.colors.answerAfterIncorrect);
	for (var i = 0; i < maxNumQuestions && continueQuestioning; ++i)
	{
		var questionNumTxt = format("\x01n%sQuestion \x01n%s%d \x01n%sof \x01n%s%d\x01n%s:\x01n",
		                       questionHdrColor, questionHdrNumColor, (i+1), questionHdrColor, questionHdrNumColor,
		                       maxNumQuestions, questionHdrColor);

		// Show the question and prompt the user for a response.  Let the user
		// try multiple times; when wrong, display a hint.
		var gotAnswerCorrect = false;
		for (var tryI = 0; tryI < gSettings.behavior.numTriesPerQuestion && !gotAnswerCorrect && continueQuestioning; ++tryI)
		{
			// Print the question
			console.print(questionNumTxt);
			console.crlf();
			printWithWordWrap(questionColor, QAArray[i].question);
			console.attributes = "N" + gSettings.colors.questionHdr;
			console.print("# points: ");
			console.attributes = "N" + gSettings.colors.questionHdrNum;
			console.print(QAArray[i].numPoints + "\x01n");
			console.crlf();
			// If this is after the first try, then give a clue
			if (tryI > 0)
			{
				console.attributes = "N" + gSettings.colors.clueHdr;
				console.print("Clue:");
				console.crlf();
				console.attributes = "N" + gSettings.colors.clue;
				var clueAnswer = "";
				if (typeof(QAArray[i].answer) === "string")
					clueAnswer = QAArray[i].answer;
				else if (Array.isArray(QAArray[i].answer))
					clueAnswer = QAArray[i].answer[0];
				console.print(partiallyHiddenStr(clueAnswer, tryI-1) + "\x01n");
				console.crlf();
			}
			// Prompt for an answer
			console.attributes = "N" + gSettings.colors.answerPrompt;
			console.print("Answer (try " + +(tryI+1) + " of " + gSettings.behavior.numTriesPerQuestion + ")");
			console.attributes = "N" + gSettings.colors.answerPromptSep;
			console.print(": ");
			console.crlf();
			console.attributes = "N" + gSettings.colors.answerInput;
			var userResponse = console.getstr();
			console.print("\x01n");
			var responseCheckRetObj = checkUserResponse(QAArray[i].answer, userResponse, qaFilenameInfo[chosenSectionIdx].sectionName);
			if (responseCheckRetObj.userChoseQuit)
			{
				continueQuestioning = false;
				break;
			}
			else if (responseCheckRetObj.userInputMatchedAnswer)
			{
				gotAnswerCorrect = true;
				userPoints += QAArray[i].numPoints;
				console.attributes = "N" + gSettings.colors.questionHdr;
				console.print("Correct!\x01n");
				console.crlf();
			}
			// If the user is extempt from an incorrect answer, then decrement the try
			// counter and continue to the next iteration (to not count against the user)
			else if (responseCheckRetObj.incorrectExempt)
			{
				--tryI;
				continue;
			}
			else
			{
				console.attributes = "N" + gSettings.colors.questionHdr;
				console.print("* Incorrect!");
				console.crlf();
				if (tryI < gSettings.behavior.numTriesPerQuestion)
					console.crlf();
			}
		}
		// If the user didn't get the answer correct, output the correct answer
		if (!gotAnswerCorrect)
		{
			console.attributes = "N" + gSettings.colors.questionHdr;
			console.print("The answer was:");
			console.crlf();
			console.attributes = "N";
			var theCorrectAnswer = "";
			if (typeof(QAArray[i].answer) === "string")
				theCorrectAnswer = QAArray[i].answer;
			else if (Array.isArray(QAArray[i].answer))
				theCorrectAnswer = QAArray[i].answer[0];
			printWithWordWrap(answerWhenIncorrectColor, theCorrectAnswer);
			console.attributes = "N";
		}
		if (QAArray[i].hasOwnProperty("answerFact") && typeof(QAArray[i].answerFact) === "string" && QAArray[i].answerFact.length > 0)
		{
			console.crlf();
			console.attributes = "N" + gSettings.colors.questionHdr;
			console.print("Fact:");
			console.crlf();
			printWithWordWrap(attrCodeStr("N" + gSettings.colors.answerFact), QAArray[i].answerFact, true);
			console.crlf();
		}

		// Print the user's score so far
		console.crlf();
		console.attributes = "N" +  gSettings.colors.scoreSoFarText;
		console.print("Your score so far: ");
		console.attributes = "N" + gSettings.colors.userScore;
		console.print(userPoints);
		console.attributes = "N";
		console.crlf();

		console.crlf();
	}
	console.attributes = "N" + gSettings.colors.questionHdr;
	console.print("Your score: ");
	console.attributes = "N" + gSettings.colors.userScore;
	console.print(userPoints);
	console.attributes = "N";
	console.crlf();
	console.print("\x01b\x01hUpdating the scores file...");
	console.crlf();
	// Update the local scores file
	var updateLocalScoresRetObj = updateScoresFile(userPoints, qaFilenameInfo[chosenSectionIdx].sectionName);
	if (updateLocalScoresRetObj.succeeded)
	{
		// If there is a server configured, then send the user's score to the server too.
		// If there are no server settings configured, or posting scores to the server fails,
		// then if there's a sub-board configured, then write the user scores to the sub-board
		var writeUserScoresToSubBoard = false;
		if (gSettings.hasValidServerSettings())
		{
			writeUserScoresToSubBoard = !updateScoresOnServer(user.alias, updateLocalScoresRetObj.userScoresObj);
			if (writeUserScoresToSubBoard)
			{
				var errorMsg = "\x01n" + attrCodeStr(gSettings.colors.error) + "Failed to update scores on the remote server.";
				if (gSettings.behavior.scoresMsgSubBoardsForPosting.length > 0)
					errorMsg += " Will post server scores in message area(s); server scores will be delayed.\x01n";
				console.putmsg(errorMsg, P_WORDWRAP|P_NOATCODES);
				console.attributes = "BH"; // As before
			}
		}
		else
			writeUserScoresToSubBoard = true;
		if (writeUserScoresToSubBoard)
		{
			// If there are any message sub-boards configured, post the scores in there
			for (var i = 0; i < gSettings.behavior.scoresMsgSubBoardsForPosting.length; ++i)
			{
				var subCode = gSettings.behavior.scoresMsgSubBoardsForPosting[i];
				if (msg_area.sub.hasOwnProperty(subCode))
				{
					if (postGTTriviaScoresToSubBoard(subCode))
						log(LOG_INFO, format("%s - Successfully posted scores in the sub-board", GAME_NAME));
					else
						log(LOG_INFO, format("%s - Posting scores in the sub-board failed!", GAME_NAME));
				}
			}
		}
		console.print("Done.\x01n");
	}
	else
	{
		console.attributes = "N" + gSettings.colors.error;
		console.print("Failed to save the scores!\x01n");
	}
	console.crlf();
	return 0;
}


// Loads settings from the .ini file
//
// Return value: An object with 'behavior' and 'color' sections with the settings loaded from the .ini file
function loadSettings()
{
	var settings = {
		colors: {
			error: "YH",
			triviaCategoryHdr: "MH",
			triviaCategoryListNumbers: "CH",
			triviaCategoryListSeparator: "GH",
			triviaCategoryName: "C",
			categoryNumPrompt: "C",
			categoryNumPromptSeparator: "GH",
			categoryNumInput: "CH",
			questionHdr: "M",
			questionHdrNum: "CH",
			question: "BH",
			answerPrompt: "C",
			answerPromptSep: "GH",
			answerInput: "CH",
			userScore: "CH",
			scoreSoFarText: "C",
			clueHdr: "RH",
			clue: "GH",
			answerAfterIncorrect: "G",
			answerFact: "G"
		}
	};
	var cfgFileName = genFullPathCfgFilename("gttrivia.ini", gScriptExecDir);
	var iniFile = new File(cfgFileName);
	if (iniFile.open("r"))
	{
		settings.behavior = iniFile.iniGetObject("BEHAVIOR");
		var colorSettingsObj = iniFile.iniGetObject("COLORS");
		settings.category_ars = iniFile.iniGetObject("CATEGORY_ARS");
		settings.remoteServer = iniFile.iniGetObject("REMOTE_SERVER");
		settings.server = iniFile.iniGetObject("SERVER");

		// Ensure the actual expected setting name & color names exist in the settings
		if (typeof(settings.behavior) !== "object")
			settings.behavior = {};
		if (typeof(settings.colors) !== "object")
			settings.colors = {};
		if (typeof(settings.category_ars) !== "object")
			settings.category_ars = {};
		if (typeof(settings.remoteServer) !== "object")
			settings.remoteServer = {};
		if (typeof(settings.server) !== "object")
			settings.server = {};

		if (typeof(settings.behavior.numQuestionsPerPlay) !== "number")
			settings.behavior.numQuestionsPerPlay = 10;
		if (typeof(settings.behavior.numTriesPerQuestion) !== "number")
			settings.behavior.numTriesPerQuestion = 4;
		if (typeof(settings.behavior.maxNumPlayerScoresToDisplay) !== "number")
			settings.behavior.maxNumPlayerScoresToDisplay = 10;

		// Colors - For any setting that matches one in settings.colors, replace it.
		var onlySyncAttrCharsRegexWholeWord = new RegExp("^[krgybmcw01234567hinq,;\.dtlasz]+$", 'i');
		for (var prop in settings.colors)
		{
			if (colorSettingsObj.hasOwnProperty(prop))
			{
				// Trim spaces from the color value.  Using toString() to ensure the color attributes
				// are strings (in case the value is just a number)
				var value = trimSpaces(colorSettingsObj[prop].toString(), true, true, true);
				value = value.replace(/\\x01/g, "\x01"); // Replace "\x01" with control character
				if (onlySyncAttrCharsRegexWholeWord.test(value))
					settings.colors[prop] = value;
			}
		}

		settings.behavior.scoresMsgSubBoardsForPosting = splitAndVerifyMsgSubCodes(settings.behavior.scoresMsgSubBoardsForPosting, "scoresMsgSubBoardsForPosting");
		settings.server.scoresMsgSubBoardsForReading = splitAndVerifyMsgSubCodes(settings.server.scoresMsgSubBoardsForReading, "scoresMsgSubBoardsForReading");

		// Sanity checking
		if (settings.behavior.numQuestionsPerPlay <= 0)
			settings.behavior.numQuestionsPerPlay = 10;
		if (settings.behavior.numTriesPerQuestion <= 0)
			settings.behavior.numTriesPerQuestion = 3;
		if (settings.behavior.maxNumPlayerScoresToDisplay <= 0)
			settings.behavior.maxNumPlayerScoresToDisplay = 10;
		if (!/^[0-9]+$/.test(settings.remoteServer.port))
			settings.remoteServer.port = 0;

		// No need to do this:
		// For each color, replace any instances of specifying the control character in substWord with the actual control character
		//for (var prop in settings.colors)
		//	settings.colors[prop] = settings.colors[prop].replace(/\\[xX]01/g, "\x01").replace(/\\[xX]1/g, "\x01").replace(/\\1/g, "\x01");
		
		iniFile.close();
	}

	// Other settings - Not read from the configuration file, but things we want to use in multiple places
	// in this script
	// JSON scope and JSON location for scores on the server (if a server is to be used)
	settings.remoteServer.gtTriviaScope = "GTTRIVIA";
	// JSON location: For the BBS name, use the QWK ID if available, but if not, use the system name and replace spaces
	// with underscores (since spaces may cause issues in JSON property names)
	var BBS_ID = getBBSIDForJSON();
	settings.remoteServer.scoresJSONLocation = "SCORES";
	settings.remoteServer.BBSJSONLocation = settings.remoteServer.scoresJSONLocation + ".systems." + BBS_ID;
	settings.remoteServer.userScoresJSONLocationWithoutUsername = settings.remoteServer.BBSJSONLocation + ".user_scores";
	settings.hasValidServerSettings = function() {
		return (this.remoteServer.hasOwnProperty("server") && this.remoteServer.hasOwnProperty("port") && this.remoteServer.server.length > 0);
	};

	return settings;
}
// For configuration files, this function returns a fully-pathed filename.
// This function first checks to see if the file exists in the sbbs/mods
// directory, then the sbbs/ctrl directory, and if the file is not found there,
// this function defaults to the given default path.
//
// Parameters:
//  pFilename: The name of the file to look for
//  pDefaultPath: The default directory (must have a trailing separator character)
function genFullPathCfgFilename(pFilename, pDefaultPath)
{
	var fullyPathedFilename = system.mods_dir + pFilename;
	if (!file_exists(fullyPathedFilename))
		fullyPathedFilename = system.ctrl_dir + pFilename;
	if (!file_exists(fullyPathedFilename))
	{
		if (typeof(pDefaultPath) == "string")
		{
			// Make sure the default path has a trailing path separator
			var defaultPath = pDefaultPath;
			if (defaultPath.length > 0 && defaultPath.charAt(defaultPath.length-1) != "/" && defaultPath.charAt(defaultPath.length-1) != "\\")
				defaultPath += "/";
			fullyPathedFilename = defaultPath + pFilename;
		}
		else
			fullyPathedFilename = pFilename;
	}
	return fullyPathedFilename;
}
// Takes a comma-separated list of internal sub-board codes and splits them into an array,
// and also verifies they exist; the returned array will contain only ones that exist.
// Also ensures there are no duplicates in the array.
//
// Parameters:
//  pSubCodeList: A comma-separated list of message sub-board codes (string)
//  pSettingName: Optional string representing the configuration setting name, for logging
//                invalid sub-board codes. If this is missing/null or empty, no logging will be done.
function splitAndVerifyMsgSubCodes(pSubCodeList, pSettingName)
{
	if (typeof(pSubCodeList) !== "string")
		return [];

	var settingName = (typeof(pSettingName) === "string" ? pSettingName : "");

	var subCodes = [];
	var subCodesFromList = pSubCodeList.split(",");
	for (var i = 0; i < subCodesFromList.length; ++i)
	{
		if (msg_area.sub.hasOwnProperty(subCodesFromList[i]))
		{
			if (!subCodes.indexOf(subCodesFromList[i]) > -1)
				subCodes.push(subCodesFromList[i]);
		}
		else if (settingName.length > 0)
		{
			var errMsg = format("%s - For configuration setting %s, %s is an invalid sub-board code", GAME_NAME, settingName, subCodesFromList[i]);
			log(LOG_ERR, errMsg);
		}
	}
	//!msg_area.sub.hasOwnProperty(
	return subCodes;
}

// Displays the program logo
//
// Parameters:
//  pClearScreenFirst: Boolean - Whether or not to clear the screen first
//  pPauseAfter: Boolean - Whether or not to have a keypress pause afterward
function displayProgramLogo(pClearScreenFirst, pPauseAfter)
{
	if (typeof(pClearScreenFirst) === "boolean" && pClearScreenFirst)
		console.clear("\x01n");

	console.printfile(gScriptExecDir + "gttrivia.asc", P_NONE, 80);

	if (typeof(pPauseAfter) === "boolean" && pPauseAfter)
		console.pause();
}

// Shows the main menu, prompts the user for a choice, and returns the user's choice as
// one of the following values:
// ACTION_PLAY
// ACTION_SHOW_HIGH_SCORES
// ACTION_SHOW_HELP_SCREEN
// ACTION_SYSOP_MENU
// ACTION_QUIT
function doMainMenu()
{
	console.print("\x01n\x01y\x01hMain Menu\x01n");
	console.crlf();
	console.print("\x01b");
	for (var i = 0; i < console.screen_columns-1; ++i)
		console.print(CP437_BOX_DRAWINGS_HORIZONTAL_DOUBLE);
	console.crlf();
	console.print("\x01c1\x01y\x01h) \x01bPlay \x01n");
	console.print("\x01c2\x01y\x01h) \x01bHelp \x01n");
	console.print("\x01c3\x01y\x01h) \x01bShow high scores \x01n");
	if (user.is_sysop)
		console.print("\x01c9\x01y\x01h) \x01bSysop menu \x01n"); // Option 9
	console.print("\x01cQ\x01y\x01h)\x01buit");
	console.crlf();
	console.print("\x01n");
	console.print("\x01b");
	for (var i = 0; i < console.screen_columns-1; ++i)
		console.print(CP437_BOX_DRAWINGS_HORIZONTAL_DOUBLE);
	console.crlf();
	console.print("\x01cYour choice\x01g\x01h: \x01c");
	var menuAction = ACTION_PLAY;
	//var userChoice = console.getnum(3);
	var validKeys = "123qQ";
	if (user.is_sysop)
		validKeys += "9"; // Clear scores
	var userChoice = console.getkeys(validKeys, -1, K_UPPER).toString();
	console.attributes = "N";
	if (userChoice.length == 0 || userChoice == "Q")
		menuAction = ACTION_QUIT;
	else if (userChoice == "1")
		menuAction = ACTION_PLAY;
	else if (userChoice == "2")
		menuAction = ACTION_SHOW_HELP_SCREEN;
	else if (userChoice == "3")
		menuAction = ACTION_SHOW_HIGH_SCORES;
	else if (userChoice == "9" && user.is_sysop)
		menuAction = ACTION_SYSOP_MENU;
	return menuAction;
}

// Gets the list of the Q&A files in the qa subdirectory in the .js script's directory.
//
// Return value: An array of objects with the following properties:
//               sectionName: The name of the trivia section
//               filename: The fully-pathed filename of the file containing the questions, answers, and # points for the trivia section
function getQACategoriesAndFilenames()
{
	var sectionsAndFilenames = [];
	var QAFilenames = directory(gScriptExecDir + "qa/*.qa");
	for (var i = 0; i < QAFilenames.length; ++i)
	{
		// Get the section name - Start by removing the .qa filename extension
		var filenameExtension = file_getext(QAFilenames[i]);
		// sectionName is the filename without the extension, but the section name can also be specified by
		// JSON metadata in the Q&A file
		var sectionName = file_getname(QAFilenames[i]);
		var charIdx = sectionName.lastIndexOf(".");
		if (charIdx > -1)
			sectionName = sectionName.substring(0, charIdx);

		// Open the file to see if it has a JSON metadata section specifying a section name, etc.
		// Note: If its metadata has an "ars" setting, then we'll use that instead of
		// any ARS setting that may be in gttrivia.ini for this section.
		var sectionARS = null;
		var QAFile = new File(QAFilenames[i]);
		if (QAFile.open("r"))
		{
			var fileMetadataStr = "";
			var readingFileMetadata = false;
			var haveSeenAllFileMetadata = false;
			while (!QAFile.eof && !haveSeenAllFileMetadata)
			{
				var fileLine = QAFile.readln(2048);
				// I've seen some cases where readln() doesn't return a string
				if (typeof(fileLine) !== "string")
					continue;
				fileLine = fileLine.trim();
				if (fileLine.length == 0)
					continue;

				//-- QA metadata begin" and "-- QA metadata end"
				if (fileLine === "-- QA metadata begin")
				{
					fileMetadataStr = "";
					readingFileMetadata = true;
					continue;
				}
				else if (fileLine === "-- QA metadata end")
				{
					readingFileMetadata = false;
					haveSeenAllFileMetadata = true;
					continue;
				}
				else if (readingFileMetadata)
					fileMetadataStr += fileLine + " ";
			}
			QAFile.close();

			// If we've read all the file metadata lines, then parse it & use the metadata.
			if (haveSeenAllFileMetadata && fileMetadataStr.length > 0)
			{
				try
				{
					var fileMetadataObj = JSON.parse(fileMetadataStr);
					if (typeof(fileMetadataObj) === "object")
					{
						if (fileMetadataObj.hasOwnProperty("category_name") && typeof(fileMetadataObj.category_name) === "string")
							sectionName = fileMetadataObj.category_name;
						if (fileMetadataObj.hasOwnProperty("ARS") && typeof(fileMetadataObj.ARS) === "string")
							sectionARS = fileMetadataObj.ARS;
					}
				}
				catch (error) {}
			}
		}

		// See if there is an ARS string for this in the configuration, and if so,
		// only add it if the ARS string passes for the user.
		var addThisSection = true;
		if (typeof(sectionARS) === "string")
			addThisSection = bbs.compare_ars(sectionARS);
		else if (gSettings.category_ars.hasOwnProperty(sectionName))
			addThisSection = bbs.compare_ars(gSettings.category_ars[sectionName]);
		// Add this section/category, if allowed
		if (addThisSection)
		{
			// Capitalize the first letters of the words in the section name, and replace underscores with spaces
			var wordsArr = sectionName.split("_");
			for (var wordI = 0; wordI < wordsArr.length; ++wordI)
				wordsArr[wordI] = wordsArr[wordI].charAt(0).toUpperCase() + wordsArr[wordI].substr(1);
			sectionName = wordsArr.join(" ");
			sectionsAndFilenames.push({
				sectionName: sectionName,
				filename: QAFilenames[i]
			});
		}
	}
	return sectionsAndFilenames;
}

// TODO: Put the next 3 functions into a common JS library for use with this and
// qa_edit.js?
// Parses a Q&A file with questions and answers
//
// Parameters:
//  pQAFilenameFullPath: The full path & filename of the trivia Q&A file to read
//
// Return value: An array of objects containing the following properties:
//               question: The trivia question
//               answer: The answer to the trivia question
//               numPoints: The number of points to award for the correct answer
function parseQAFile(pQAFilenameFullPath)
{
	if (!file_exists(pQAFilenameFullPath))
		return [];

	// Open the file and parse it.  Each line has:
	// question,answer,points
	var QA_Array = [];
	var QAFile = new File(pQAFilenameFullPath);
	if (QAFile.open("r"))
	{
		var inFileMetadata = false; // Whether or not we're in the file metadata question (we'll skip all of this for the questions & answers)
		var theQuestion = "";
		var theAnswer = "";
		var theNumPoints = -1;
		var readingAnswerMetadata = false;
		var doneReadintAnswerMetadata = false; // For immediately after done reading answer JSON
		var answerIsJSON = false;
		var component = "Q"; // Q/A/P for question, answer, points
		while (!QAFile.eof)
		{
			var fileLine = QAFile.readln(2048);
			// I've seen some cases where readln() doesn't return a string
			if (typeof(fileLine) !== "string")
				continue;
			fileLine = fileLine.trim();
			if (fileLine.length == 0)
				continue;

			// Skip any file metadata lines (those are read in getQACategoriesAndFilenames())
			if (fileLine === "-- QA metadata begin")
			{
				inFileMetadata = true;
				continue;
			}
			else if (fileLine === "-- QA metadata end")
			{
				inFileMetadata = false;
				continue;
			}
			if (inFileMetadata)
				continue;

			if (theQuestion.length > 0 && theAnswer.length > 0 && theNumPoints > -1)
			{
				addQAToArray(QA_Array, theQuestion, theAnswer, theNumPoints, answerIsJSON);
				theQuestion = "";
				theAnswer = "";
				theNumPoints = -1;
				readingAnswerMetadata = false;
				doneReadintAnswerMetadata = false;
				answerIsJSON = false;
				component = "Q";
			}

			if (component === "Q")
			{
				theQuestion = fileLine;
				component = "A"; // Next, set answer
			}
			else if (component === "A")
			{
				// Possible JSON for multiple answers
				if (fileLine === "-- Answer metadata begin")
				{
					readingAnswerMetadata = true;
					answerIsJSON = true;
					theAnswer = "";
					continue;
				}
				else if (fileLine === "-- Answer metadata end")
				{
					readingAnswerMetadata = false;
					doneReadintAnswerMetadata = true;
				}
				if (readingAnswerMetadata)
					theAnswer += fileLine + " ";
				else
				{
					if (doneReadintAnswerMetadata)
						doneReadintAnswerMetadata = false;
					else
						theAnswer = fileLine;
					component = "P"; // Next, set points
				}
			}
			else if (component === "P")
			{
				theNumPoints = +(fileLine);
				if (theNumPoints < 1)
					theNumPoints = 10;
				component = "Q"; // Next, set question
			}

			// Older: Each line in the format question,answer,numPoints
			/*
			// Questions might have commas in them, so we can't just blindly split on commas like this:
			//var lineParts = fileLine.split(",");
			// Find the first & last indexes of a comma in the line
			var lastCommaIdx = fileLine.lastIndexOf(",");
			var firstCommaIdx = (lastCommaIdx > -1 ? fileLine.lastIndexOf(",", lastCommaIdx-1) : -1);
			if (firstCommaIdx > -1 && lastCommaIdx > -1)
			{
				QA_Array.push({
					question: fileLine.substring(0, firstCommaIdx),
					answer: fileLine.substring(firstCommaIdx+1, lastCommaIdx),
					numPoints: +(fileLine.substring(lastCommaIdx+1))
				});
			}
			*/
		}
		QAFile.close();
		// Ensure we've added the last question & answer, if there is one
		if (theQuestion.length > 0 && theAnswer.length > 0 && theNumPoints > -1)
			addQAToArray(QA_Array, theQuestion, theAnswer, theNumPoints, answerIsJSON);
	}
	return QA_Array;
}
// QA object constructor
function QA(pQuestion, pAnswer, pNumPoints, pAnswerFact)
{
	this.question = pQuestion;
	this.answer = pAnswer;
	this.numPoints = pNumPoints;
	if (this.numPoints < 1)
		this.numPoints = 10;
	if (typeof(pAnswerFact) === "string" && pAnswerFact.length > 0)
		this.answerFact = pAnswerFact;
}
// Helper for parseQAFile(): Adds a question, answer, and # points to the Q&A array
//
// Parameters:
//  QA_Array (INOUT): The array to add the Q/A/Point sets to
//  theQuestion: The question (string)
//  theAnswer: The answer (string)
//  theNumPoints: The number of points to award (number)
//  answerIsJSON: Boolean - Whether or not theAnswer is in JSON format or not (if JSON, it contains
//                multiple possible answers)
function addQAToArray(QA_Array, theQuestion, theAnswer, theNumPoints, answerIsJSON)
{
	// If the answer is a JSON object, then there may be multiple acceptable answers specified
	var addAnswer = true;
	var answerFact = null;
	if (answerIsJSON)
	{
		try
		{
			var answerObj = JSON.parse(theAnswer);
			if (typeof(answerObj) === "object")
			{
				if (answerObj.hasOwnProperty("answers") && Array.isArray(answerObj.answers) && answerObj.answers.length > 0)
				{
					// Make sure all answers in the array are non-zero length
					theAnswer = [];
					for (var i = 0; i < answerObj.answers.length; ++i)
					{
						if (answerObj.answers[i].length > 0)
							theAnswer.push(answerObj.answers[i]);
					}
					// theAnswer is an array
					addAnswer = (theAnswer.length > 0);
				}
				else if (answerObj.hasOwnProperty("answer") && typeof(answerObj.answer) === "string" && answerObj.answer.length > 0)
					theAnswer = answerObj.answer;
				else
					addAnswer = false;
				if (answerObj.hasOwnProperty("answerFact") && answerObj.answerFact.length > 0)
					answerFact = answerObj.answerFact;
			}
			else
				addAnswer = false;
		}
		catch (error)
		{
			addAnswer = false;
		}
	}
	if (addAnswer) // Note: theAnswer is converted to an object if it's JSON
		QA_Array.push(new QA(theQuestion, theAnswer, +theNumPoints, answerFact));
}

// Shuffles an array
//
// Parameters:
//  pArray: The array to shuffle
function shuffle(pArray)
{
	var currentIndex = pArray.length;
	var randomIndex = 0;

	// While there remain elements to shuffle...
	while (currentIndex != 0)
	{
		// Pick a remaining element
		randomIndex = Math.floor(Math.random() * currentIndex);
		--currentIndex;

		// And swap it with the current element
		[pArray[currentIndex], pArray[randomIndex]] = [
			pArray[randomIndex], pArray[currentIndex]];
	}

	return pArray;
}

// Returns whether or not a user's answer matches the answer to a question (or is close enough).
// Match is case-insensitive.  If it's a 1-word answer, then it should match exactly.  Otherwise,
// a Levenshtein distance is used.
//
// Properties:
//  pAnswer: The answer to the question. This can either be a string (for a single answer) or an array of
//           strings (if multiple answers are acceptable)
//  pUserInput: The user's response to the question (string)
//  pCategoryName: The name of the category being played.  Only needed for display on the help screen.
//
// Return value: An object with the following properties:
//               userChoseQuit: Boolean: Whether or not the user chose to quit
//               userInputMatchedAnswer: Boolean: Whether or not the user's answer matches the given answer to the question
//               incorrectExempt: Whether or not the user is exempt from a wrong answer (i.e., they chose to view help)
function checkUserResponse(pAnswer, pUserInput, pCategoryName)
{
	var retObj = {
		userChoseQuit: false,
		userInputMatchedAnswer: false,
		incorrectExempt: false
	};

	// pAnswer should be a string or an array, and pUserInput should be a string
	if (!(typeof(pAnswer) === "string" || Array.isArray(pAnswer))|| typeof(pUserInput) !== "string")
		return retObj;
	if (pUserInput.length == 0)
		return retObj;

	var userInputUpper = pUserInput.toUpperCase(); // For case-insensitive matching
	
	if (userInputUpper == "Q")
	{
		retObj.userChoseQuit = true;
		return retObj;
	}
	else if (pUserInput == "?")
	{
		showHelpScreen(pCategoryName);
		retObj.incorrectExempt = true;
		return retObj;
	}

	// In case there are multiple acceptable answers, make an array (or copy it) so
	// we can check the user's response against all acceptable answers
	var acceptableAnswers = null;
	if (typeof(pAnswer) === "string")
		acceptableAnswers = [ pAnswer.toUpperCase() ];
	else if (Array.isArray(pAnswer))
	{
		acceptableAnswers = [];
		for (var i = 0; i < pAnswer.length; ++i)
			acceptableAnswers.push(pAnswer[i].toUpperCase());
	}
	else
		return retObj; // pAnswer isn't valid here, so just return with a 'false' response
	// Check the user's response against the acceptable answers
	for (var i = 0; i < acceptableAnswers.length && !retObj.userInputMatchedAnswer; ++i)
	{
		var answerUpper = acceptableAnswers[i].toUpperCase();
		// If there are spaces in the answer, then do a Levenshtein comparison.  Otherwise,
		// do an exact match.
		if (answerUpper.indexOf(" ") > -1)
		{
			var levDist = levenshteinDistance(answerUpper, userInputUpper);
			retObj.userInputMatchedAnswer = (levDist <= MAX_LEVENSHTEIN_DISTANCE);
		}
		else
		{
			// There are no spaces in the answer.  If the answer is 12 or shorter, use an exact match;
			// otherwise, use a Levenshtein distance.
			if (answerUpper.length <= 12)
				retObj.userInputMatchedAnswer = (userInputUpper == answerUpper);
			else
			{
				var levDist = levenshteinDistance(answerUpper, userInputUpper);
				retObj.userInputMatchedAnswer = (levDist <= MAX_LEVENSHTEIN_DISTANCE);
			}
		}
	}

	return retObj;
}

// Calculates the Levenshtein distance between 2 strings
//
// Parameters:
//  pStr1: The first string to compare
//  pStr2: The second string to compare
//
// Return value: The Levenshtein distance between the two strings (numeric)
function levenshteinDistance(pStr1, pStr2)
{
	if (typeof(pStr1) !== "string" || typeof(pStr2) !== "string")
		return 0;

	//https://www.tutorialspoint.com/levenshtein-distance-in-javascript
	var track = new Array(pStr2.length + 1);
	for (var i = 0; i < pStr2.length + 1; ++i)
	{
		track[i] = new Array(pStr1.length + 1);
		for (var j = 0; j < pStr1.length + 1; ++j)
			track[i][j] = null;
	}

	for (var i = 0; i <= pStr1.length; i += 1)
		track[0][i] = i;
	for (var j = 0; j <= pStr2.length; j += 1)
		track[j][0] = j;
	for (var j = 1; j <= pStr2.length; j += 1)
	{
		for (var i = 1; i <= pStr1.length; i += 1)
		{
			var indicator = pStr1[i - 1] === pStr2[j - 1] ? 0 : 1;
			track[j][i] = Math.min(
				track[j][i - 1] + 1, // deletion
				track[j - 1][i] + 1, // insertion
				track[j - 1][i - 1] + indicator // substitution
			);
		}
	}
	return track[pStr2.length][pStr1.length];
}

// Adds/updates the user's game score in the scores file
//
// Parameters:
//  pUserCurrentGameScore: The user's score for their current game
//  pLastSectionName: The name of the last trivia section the user played
//
// Return value: An object with the following properties:
//               succeeded: Boolean: Whether or not saving the scores to the file succeeded
//               userScoresObj: An object containing information on the user's scores
function updateScoresFile(pUserCurrentGameScore, pLastSectionName)
{
	var retObj = {
		succeeded: false,
		userScoresObj: {}
	};

	if (typeof(pUserCurrentGameScore) !== "number")
		return retObj;

	var lastSectionName = (typeof(pLastSectionName) === "string" ? pLastSectionName : "");

	// If the semaphore file exists, then wait for it to be deleted before writing
	// the user's score to the file
	var semaphoreWaitCount = 0;
	while (file_exists(SCORES_SEMAPHORE_FILENAME) && semaphoreWaitCount < 20)
	{
		mswait(50);
		++semaphoreWaitCount;
	}
	// Write the semaphore file (to try to prevent simultaneous writes that might stomp on each other)
	file_touch(SCORES_SEMAPHORE_FILENAME);

	var scoresObj = {};
	// Read the scores file to see if the user has an existing score in there already
	var scoresFile = new File(SCORES_FILENAME);
	if (file_exists(SCORES_FILENAME))
	{
		if (scoresFile.open("r"))
		{
			var scoreFileContents = scoresFile.read(scoresFile.length);
			scoresFile.close();
			try
			{
				scoresObj = JSON.parse(scoreFileContents);
			}
			catch (error)
			{
				log(LOG_ERR, GAME_NAME + " - Loading scores: Line " + error.lineNumber + ": " + error);
				bbs.log_str(GAME_NAME + " - Loading scores: Line " + error.lineNumber + ": " + error);
			}
		}
	}
	if (typeof(scoresObj) !== "object")
		scoresObj = {};

	retObj.succeeded = true;
	// Add/update the user's score, and save the scores file
	try
	{
		// Score object example (note: last_time is a UNIX time):
		// Username:
		//   category_stats:
		//     0:
		//       category_name: General
		//       last_time: 166000
		//       last_score: 20
		//     1:
		//       category_name: Misc
		//       last_time: 146000
		//       last_score: 80
		//  total_score: 100
		//  last_score: 20
		//  last_trivia_category: category_1
		//  last_time: 166000
		var currentTime = time();
		var userCategoryStatsPropName = "category_stats";
		// Ensure the score object has an object for the current user
		if (!scoresObj.hasOwnProperty(user.alias))
			scoresObj[user.alias] = {};
		// Ensure the user object in the scores object has a category_stats array
		if (!scoresObj[user.alias].hasOwnProperty(userCategoryStatsPropName))
			scoresObj[user.alias][userCategoryStatsPropName] = [];
		else
		{
			// In case version 1.00 of this door has been run before, the category stats would be an object
			// with category names as the properties..  Convert that to an array
			if (!Array.isArray(scoresObj[user.alias][userCategoryStatsPropName]))
			{
				var userCatStats = [];
				for (var categoryName in scoresObj[user.alias][userCategoryStatsPropName])
				{
					var statsObj = {
						category_name: categoryName,
						last_score: 0,
						last_time: scoresObj[user.alias][userCategoryStatsPropName].last_time
					};
					// Version 1.00 has a total_score in the category sections
					if (scoresObj[user.alias][userCategoryStatsPropName].hasOwnProperty("total_score"))
						statsObj.last_score = scoresObj[user.alias][userCategoryStatsPropName].total_score;
					else if (scoresObj[user.alias][userCategoryStatsPropName].hasOwnProperty("last_score"))
						statsObj.last_score = scoresObj[user.alias][userCategoryStatsPropName].last_score;
					userCatStats.push(statsObj);
				}
				scoresObj[user.alias][userCategoryStatsPropName] = userCatStats;
			}
		}
		// See if the category stats already has an element with the last section name
		var catStatsElementIdx = -1;
		for (var i = 0; i < scoresObj[user.alias][userCategoryStatsPropName].length && catStatsElementIdx == -1; ++i)
		{
			if (scoresObj[user.alias][userCategoryStatsPropName][i].category_name == pLastSectionName)
				catStatsElementIdx = i;
		}
		if (catStatsElementIdx == -1)
		{
			// The last section info doesn't exist in the array
			scoresObj[user.alias][userCategoryStatsPropName].push({
				category_name: pLastSectionName,
				last_score: pUserCurrentGameScore,
				last_time: currentTime
			});
		}
		else // The last section info already exists in the array
		{
			scoresObj[user.alias][userCategoryStatsPropName][catStatsElementIdx].last_score = pUserCurrentGameScore;
			scoresObj[user.alias][userCategoryStatsPropName][catStatsElementIdx].last_time = currentTime;
		}
		// Update the user's grand total score value
		if (scoresObj[user.alias].hasOwnProperty("total_score"))
			scoresObj[user.alias].total_score += pUserCurrentGameScore;
		else
			scoresObj[user.alias].total_score = pUserCurrentGameScore;
		scoresObj[user.alias].last_score = pUserCurrentGameScore;
		scoresObj[user.alias].last_trivia_category = lastSectionName;
		scoresObj[user.alias].last_time = currentTime;
		retObj.userScoresObj = scoresObj[user.alias];
	}
	catch (error)
	{
		retObj.succeeded = false;
		console.print("* Line " + error.lineNumber + ": " + error);
		console.crlf();
		log(LOG_ERR, GAME_NAME + " - Updating trivia score object: Line " + error.lineNumber + ": " + error);
		bbs.log_str(GAME_NAME + " - Updating trivia score object: Line " + error.lineNumber + ": " + error);
	}
	scoresFile = new File(SCORES_FILENAME);
	if (scoresFile.open("w"))
	{
		scoresFile.write(JSON.stringify(scoresObj));
		scoresFile.close();
	}
	else
		retObj.succeeded = false;

	// Delete the semaphore file
	file_remove(SCORES_SEMAPHORE_FILENAME);

	return retObj;
}

// Updates user scores on the server (if there is one configured)
//
// Parameters:
//  pUserNameForScores: The user's name as used for the scores
//  pUserScoreInfo: An object containing user scores, as created by updateScoresFile()
//
// Return value: Boolean: Whether or not the update was successful
function updateScoresOnServer(pUserNameForScores, pUserScoreInfo)
{
	// Make sure the settings have valid server settings and the user score info object is valid
	if (!gSettings.hasValidServerSettings())
		return false;
	if (typeof(pUserNameForScores) !== "string" || pUserNameForScores.length == 0 || typeof(pUserScoreInfo) !== "object")
		return false;

	var updateSuccessful = true;
	try
	{
		// You could lock for each individual write like this:
		//
		// var JSONLocation = gSettings.remoteServer.BBSJSONLocation + ".bbs_name";
		// jsonClient.write(gSettings.remoteServer.gtTriviaScope, JSONLocation, system.name, JSON_DB_LOCK_WRITE);
		//
		// You can also call lock() to lock the JSON location you want to use, do your reads & writes, and then
		// unlock at the end.  The code here locks on the BBS ID JSON location and does its writes, so that
		// readGTTriviaScoresFromSubBoard() can also lock on the same location to do its writes when importing
		// scores from the messagebase.

		var jsonClient = new JSONClient(gSettings.remoteServer.server, gSettings.remoteServer.port);
		jsonClient.lock(gSettings.remoteServer.gtTriviaScope, gSettings.remoteServer.BBSJSONLocation, JSON_DB_LOCK_WRITE);
		// Ensure the BBS name on the server has been set
		var JSONLocation = gSettings.remoteServer.BBSJSONLocation + ".bbs_name";
		jsonClient.write(gSettings.remoteServer.gtTriviaScope, JSONLocation, system.name);
		// Write the scores on the server
		JSONLocation = gSettings.remoteServer.userScoresJSONLocationWithoutUsername + "." + pUserNameForScores;
		jsonClient.write(gSettings.remoteServer.gtTriviaScope, JSONLocation, pUserScoreInfo);
		// Write the client & version information in the user scores too
		var gameInfo = format("%s version %s (%s)", GAME_NAME, GAME_VERSION, GAME_VER_DATE);
		JSONLocation += ".game_client";
		jsonClient.write(gSettings.remoteServer.gtTriviaScope, JSONLocation, gameInfo);
		// Now that we're done, unlock and disconnect
		jsonClient.unlock(gSettings.remoteServer.gtTriviaScope, gSettings.remoteServer.BBSJSONLocation);
		jsonClient.disconnect();
	}
	catch (error)
	{
		updateSuccessful = false;
		console.print("* Line " + error.lineNumber + ": " + error);
		console.crlf();
		log(LOG_ERR, GAME_NAME + " - Updating scores on server: Line " + error.lineNumber + ": " + error);
		bbs.log_str(GAME_NAME + " - Updating scores on server: Line " + error.lineNumber + ": " + error);
	}
	return updateSuccessful;
}

// Shows the saved scores - First the locally saved scores, and then if there is a
// server configured, shows the remote saved scores (after prompting the user whether
// to show those)
function showScores()
{
	// Show local scores.  Then, if there is a server configured, prompt the user if they
	// want to see server scores, and if so, show those.
	showLocalScores();

	if (gSettings.hasValidServerSettings())
	{
		var showServerScoresConfirm = console.yesno("\x01n\x01b\x01hShow multi-BBS scores");
		console.attributes = "N";
		if (showServerScoresConfirm)
			showServerScores();
	}
}

// Shows the locally saved scores (on the current BBS)
function showLocalScores()
{
	console.attributes = "N";
	console.crlf();
	var sortedScores = [];
	// Read the scores file to see if the user has an existing score in there already
	if (file_exists(SCORES_FILENAME))
	{
		var scoresFile = new File(SCORES_FILENAME);
		if (scoresFile.open("r"))
		{
			var scoreFileContents = scoresFile.read(scoresFile.length);
			scoresFile.close();
			try
			{
				var scoresObj = JSON.parse(scoreFileContents);
				for (var prop in scoresObj)
				{
					sortedScores.push(new UserScoreObj(prop, scoresObj[prop].total_score, scoresObj[prop].last_score,
												  scoresObj[prop].last_trivia_category, scoresObj[prop].last_time));
				}
			}
			catch (error)
			{
				log(LOG_ERR, GAME_NAME + " - Parsing local scores: Line " + error.lineNumber + ": " + error);
				bbs.log_str(GAME_NAME + " - Parsing local scores scores: Line " + error.lineNumber + ": " + error);
				console.attributes = "N" + gSettings.colors.error;
				console.print("* Line: " + error.lineNumber + ": " + error);
				console.crlf();
			}
		}
		// Sort the array: High total score first
		sortedScores.sort(userScoresSortTotalScoreHighest);
	}
	// Print the scores if there are any
	if (sortedScores.length > 0)
		showUserScoresArray(sortedScores);
	else
		console.print("\x01gThere are no saved scores yet.\x01n");
	console.crlf();
}

// Shows the scores from the server (multi-BBS scores)
function showServerScores()
{
	if (!gSettings.hasValidServerSettings())
		return;

	// Use a JSONClient to get the scores JSON from the server
	try
	{
		var jsonClient = new JSONClient(gSettings.remoteServer.server, gSettings.remoteServer.port);
		var data = jsonClient.read(gSettings.remoteServer.gtTriviaScope, "SCORES", JSON_DB_LOCK_READ);
		jsonClient.disconnect();
		// Example of scores from the server (as of data version 1.01):
		/*
		SCORES:
		 systems:
		  DIGDIST:
		   bbs_name: Digital Distortion
		   user_scores:
		    Nightfox:
			 category_stats:
			  0:
			   category_name: General
			   last_score: 20
			   last_time: 2022-11-24
			 total_score: 60
			 last_score: 20
			 last_trivia_category: General
			 last_time: 2022-11-24
			 game_client: Good Time Trivia version 1.01 Beta (2022-11-24)
		*/
		/*
		if (typeof(data) === "string")
			data = JSON.parse(data);
		*/
		// Sanity checking: Make sure the data is an object and has a "systems" property
		if (typeof(data) !== "object")
		{
			console.attributes = "N" + gSettings.colors.error;
			console.print("Invalid scores data was received from the server\x01n");
			console.crlf();
			if (user.is_sysop)
			{
				console.print("Expected an object.  It's actually: " + typeof(data) + "\r\n");
				//gSettings.remoteServer.server, gSettings.remoteServer.port
				console.print("    Server: " + gSettings.remoteServer.server + "\r\n");
				console.print("      Port: " + gSettings.remoteServer.port + "\r\n");
				console.print("JSON scope: " + gSettings.remoteServer.gtTriviaScope + "\r\n");
			}
			return;
		}
		if (!data.hasOwnProperty("systems"))
		{
			console.attributes = "N" + gSettings.colors.error;
			console.print("Invalid scores data was received from the server (2)\x01n");
			console.crlf();
			if (user.is_sysop)
				console.print("The JSON data doesn't have a 'systems' element.\r\n");
			return;
		}

		// For each BBS in the scores, sort the player scores and then show the scores for
		// the BBS
		for (var BBS_ID in data.systems)
		{
			if (!data.systems[BBS_ID].hasOwnProperty("user_scores"))
				continue;
			var sortedScores = [];
			for (var playerName in data.systems[BBS_ID].user_scores)
			{
				// Player category stats are also available (as an array):
				//data.systems[BBS_ID].user_scores[playerName].category_stats
				var scoreObj = new UserScoreObj(playerName, data.systems[BBS_ID].user_scores[playerName].total_score, data.systems[BBS_ID].user_scores[playerName].last_score,
												data.systems[BBS_ID].user_scores[playerName].last_trivia_category, data.systems[BBS_ID].user_scores[playerName].last_time);
				sortedScores.push(scoreObj);
			}
			// Sort the array: High total score first.  Then display them.
			sortedScores.sort(userScoresSortTotalScoreHighest);
			if (sortedScores.length > 0)
			{
				showUserScoresArray(sortedScores, data.systems[BBS_ID].bbs_name);
				// If debugging is enabled, then also show the game_client property (game_client stores the name
				// & version of the game that wrote the user score data for this player)
				if (gCmdLineArgs.debug)
				{
					if (data.systems[BBS_ID].user_scores[playerName].hasOwnProperty("game_client"))
					{
						console.print("\x01n\x01cGame client: \x01h" + data.systems[BBS_ID].user_scores[playerName].game_client + "\x01n");
						console.crlf();
					}
				}
				console.crlf();
			}
		}
	}
	catch (error)
	{
		log(LOG_ERR, GAME_NAME + " - Getting server scores: Line " + error.lineNumber + ": " + error);
		bbs.log_str(GAME_NAME + " - Getting server scores: Line " + error.lineNumber + ": " + error);
		console.attributes = "N" + gSettings.colors.error;
		console.print("* Line: " + error.lineNumber + ": " + error);
		console.crlf();
	}
}

// Shows an array of user scores
//
// Parameters:
//  pUserScoresArray: An array of objects with the following properties:
//                    name: Player's name
//                    total_score: Player's total score from all games they've played
//                    last_score: The player's score from the last game they played
//                    last_trivia_category: The name of the last trivia category the player played
//                    last_time: The UNIX timestamp when the player's scores were saved
//  pBBSName: Optional - The name of a BBS where the scores are coming from (if applicable)
function showUserScoresArray(pUserScoresArray, pBBSName)
{
	if (typeof(pUserScoresArray) !== "object" || pUserScoresArray.length == 0)
		return;

	// Make the format string for printf()
	var scoreWidth = 6;
	var dateWidth = 10;
	var categoryWidth = 25; //15;
	var nameWidth = 0;
	var formatStr = "";
	if (console.screen_columns >= 80)
	{
		nameWidth = console.screen_columns - dateWidth - categoryWidth - (scoreWidth * 2) - 5;
		formatStr = "%-" + nameWidth + "s %-" + dateWidth + "s %-" + categoryWidth + "s %" + scoreWidth + "d %" + scoreWidth + "d";
	}
	else
	{
		nameWidth = console.screen_columns - (scoreWidth * 2) - 3;
		formatStr = "%-" + nameWidth + "s %" + scoreWidth + "d %" + scoreWidth + "d";
	}
	// Show the "High scores" header
	/*
	if (typeof(pBBSName) === "string" && pBBSName.length > 0)
		console.center("\x01g\x01hHigh Scores: \x01c" + pBBSName + "\x01n");
	else
		console.center("\x01g\x01hHigh Scores\x01n");
	*/
	console.center("\x01n\x01g\x01hHigh Scores\x01n");
	if (typeof(pBBSName) === "string" && pBBSName.length > 0)
		console.center("\x01n\x01gBBS\x01h: \x01c" + pBBSName);
	console.crlf();
	// Print the scores
	if (console.screen_columns >= 80)
	{
		printf("\x01w\x01h%-" + nameWidth + "s %-" + dateWidth + "s %-" + categoryWidth + "s %" + scoreWidth + "s %" + scoreWidth + "s",
			   "Player", "Last date", "Last category", "Total", "Last");
	}
	else
		printf("\x01w\x01h%-" + nameWidth + "s %" + scoreWidth + "s %" + scoreWidth + "s\x01n", "Player", "Total", "Last");
	console.crlf();
	console.print("\x01n\x01b");
	for (var i = 0; i < console.screen_columns-1; ++i)
		console.print(CP437_BOX_DRAWINGS_HORIZONTAL_DOUBLE);
	console.crlf();
	// Print the list of high scores
	console.print("\x01g");
	if (console.screen_columns >= 80)
	{
		for (var i = 0; i < pUserScoresArray.length && i < gSettings.behavior.maxNumPlayerScoresToDisplay; ++i)
		{
			var playerName = pUserScoresArray[i].name.substr(0, nameWidth);
			var lastDate = strftime("%Y-%m-%d", pUserScoresArray[i].last_time);
			var sectionName = pUserScoresArray[i].last_trivia_category.substr(0, categoryWidth);
			printf(formatStr, playerName, lastDate, sectionName, pUserScoresArray[i].total_score, pUserScoresArray[i].last_score);
			console.crlf();
		}
	}
	else
	{
		for (var i = 0; i < pUserScoresArray.length && i < gSettings.behavior.maxNumPlayerScoresToDisplay; ++i)
		{
			printf(formatStr, pUserScoresArray[i].name.substr(0, nameWidth), pUserScoresArray[i].total_score, pUserScoresArray[i].last_score);
			console.crlf();
		}
	}
	console.print("\x01n\x01b");
	for (var i = 0; i < console.screen_columns-1; ++i)
		console.print(CP437_BOX_DRAWINGS_HORIZONTAL_DOUBLE);
	console.crlf();
}

// Creates a user score object for display in the high scores
function UserScoreObj(pPlayerName, pTotalScore, pLastScore, pLastCategory, pLastTime)
{
	this.name = pPlayerName;
	this.total_score = pTotalScore;
	this.last_score = pLastScore;
	this.last_trivia_category = pLastCategory;
	this.last_time = pLastTime;
}

// An array sorting function for UserScoreObj objects to sort the array by
// highest total_score first
function userScoresSortTotalScoreHighest(pPlayerA, pPlayerB)
{
	if (pPlayerA.total_score > pPlayerB.total_score)
		return -1;
	else if (pPlayerA.total_score < pPlayerB.total_score)
		return 1;
	else
		return 0;
}

// Displays the game help to the user
//
// Parameters:
//  pCategoryName: Optional - A category name, if the user is currently playing a game
function showHelpScreen(pCategoryName)
{
	// Construct the header text lines only once.
	if (typeof(showHelpScreen.headerLines) === "undefined")
	{
		showHelpScreen.headerLines = [];

		var headerText = "\x01n\x01c" + GAME_NAME + " Help\x01n";
		var headerTextLen = strip_ctrl(headerText).length;

		// Top border
		var headerTextStr = "\x01h\x01c" + CP437_BOX_DRAWINGS_UPPER_LEFT_SINGLE;
		for (var i = 0; i < headerTextLen + 2; ++i)
			headerTextStr += CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE;
		headerTextStr += CP437_BOX_DRAWINGS_UPPER_RIGHT_SINGLE;
		showHelpScreen.headerLines.push(headerTextStr);

		// Middle line: Header text string
		headerTextStr = CP437_BOX_DRAWINGS_LIGHT_VERTICAL + "\x01n " + headerText + " \x01n\x01h\x01c" + CP437_BOX_DRAWINGS_LIGHT_VERTICAL;
		showHelpScreen.headerLines.push(headerTextStr);

		// Lower border
		headerTextStr = CP437_BOX_DRAWINGS_LOWER_LEFT_SINGLE;
		for (var i = 0; i < headerTextLen + 2; ++i)
			headerTextStr += CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE;
		headerTextStr += CP437_BOX_DRAWINGS_LOWER_RIGHT_SINGLE;
		showHelpScreen.headerLines.push(headerTextStr);
	}

	console.clear("\x01n");
	// Print the header strings
	for (var index in showHelpScreen.headerLines)
		console.center(showHelpScreen.headerLines[index]);

	// Show help text
	console.center("\x01n\x01cVersion \x01h" + GAME_VERSION);
	console.center("\x01n\x01c\x01h" + GAME_VER_DATE + "\x01n");
	console.crlf();
	if (typeof(pCategoryName) === "string" && pCategoryName.length > 0)
	{
		var catHelpStr = "\x01n\x01gYou are currently playing a game:\x01n "
		               + attrCodeStr(gSettings.colors.triviaCategoryName) + pCategoryName + "\x01n";
		console.putmsg(catHelpStr, P_WORDWRAP);
		console.crlf();
	}
	var helpText = GAME_NAME + " is a trivia game with a freeform answer format.  The game "
	             + "starts with a main menu, allowing you to play, show high scores, or quit.";
	printWithWordWrap("\x01n\x01c", helpText);
	console.crlf();
	console.print("\x01n\x01c\x01hGameplay:");
	console.crlf();
	console.print("\x01n\x01g");
	for (var i = 0; i < 9; ++i)
		console.print(CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE);
	console.print("\x01n");
	console.crlf();
	helpText = "When starting a game, there can be potentially multiple trivia categories to "
	         + "choose from.  If there is only one category, the game will automatically start with that category.";
	printWithWordWrap("\x01n\x01c", helpText);
	helpText = "During a game, you will be asked up to " + gSettings.behavior.numQuestionsPerPlay
	         + " questions per play.  For each question, you are given ";
	if (gSettings.behavior.numTriesPerQuestion > 1)
		helpText += gSettings.behavior.numTriesPerQuestion + " chances ";
	else
		helpText += "1 chance ";
	helpText += "to answer the question correctly.  At any time, you an answer Q to quit out of the "
	         + "question set.  ";
	if (gSettings.behavior.numTriesPerQuestion > 1)
	{
		helpText += "Each time you answer incorrectly, a clue will be given.  The first clue is a "
		         + "fully-masked answer, which reveals the length of the answer; each additional "
		         + "clue will have one character revealed in the answer.  ";
	}
	helpText += "When the questions have completed, your score will be shown.  When you're done playing, "
	         + "your current play score, category, and total running score will be saved to the high scores file.";
	printWithWordWrap("\x01n\x01c", helpText);
	console.print("\x01n");
	console.crlf();
	console.pause();
}

// Returns a version of a string that is masked, possibly with some of its characters unmasked.
// Spaces will not be included.
//
// Parameters:
//  pStr: A string to mask
//  pNumLettersUncovered: The number of letters to be uncovered/unmasked
//  pMaskChar: Optional - The mask character. Defaults to "*".
function partiallyHiddenStr(pStr, pNumLettersUncovered, pMaskChar)
{
	if (typeof(pStr) !== "string" || pStr.length == 0)
		return "";

	// Count the number of spaces in the string
	var numSpaces = 0;
	for (var i = 0; i < pStr.length; ++i)
	{
		if (pStr.charAt(i) == " ")
			++numSpaces;
	}

	var maskChar = (typeof(pMaskChar) === "string" && pMaskChar.length > 0 ? pMaskChar.substr(0, 1) : "*");
	var numLetersUncovered = (typeof(pNumLettersUncovered) === "number" && pNumLettersUncovered > 0 ? pNumLettersUncovered : 0);
	var str = "";
	if (numLetersUncovered >= pStr.length - numSpaces) //if (numLetersUncovered >= pStr.length)
		str = pStr;
	else
	{
		var i = 0;
		var charCount = 0;
		for (i = 0; i < pStr.length; ++i)
		{
			var currentChar = pStr.charAt(i);
			if (currentChar == " ")
			{
				str += " ";
				continue;
			}
			if (charCount++ < numLetersUncovered)
				str += currentChar;
			else
				str += maskChar;
		}
	}
	return str;
}

// Prints a string with word wrapping for the terminal width.  Uses word_wrap() and prints a carriage return
// so that the cursor is at the beginning of the next line.
//
// Parameters:
//  pAttributes: Any attributes to be applied for the text.  Can be null or an empty string for no attributes.
//  pStr: The string to print word-wrapped
//  pPrintNormalAttrAfterward: Optional boolean - Whether or not to apply the normal attribute afterward.  Defaults to true.
function printWithWordWrap(pAttributes, pStr, pPrintNormalAttrAfterward)
{
	if (typeof(pStr) !== "string" || pStr.length == 0)
		return;
	var attrs = "";
	if (typeof(pAttributes) === "string" && pAttributes.length > 0)
	{
		//console.print("\x01n" + pAttributes);
		attrs = "\x01n" + pAttributes;
	}
	//console.print(word_wrap(pStr, console.screen_columns-1, console.strlen(pStr), false));
	//console.print("\r");
	console.putmsg(attrs + pStr, P_WORDWRAP|P_NOATCODES);
	var applyNormalAttr = (typeof(pPrintNormalAttrAfterward) === "boolean" ? pPrintNormalAttrAfterward : true);
	if (applyNormalAttr)
		console.print("\x01n");
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

// Sysop menu: Provides the sysop with some maintenance options
function doSysopMenu()
{
	if (!user.is_sysop)
		return;

	var continueOn = true;
	while (continueOn)
	{
		console.attributes = "N";
		console.print("\x01c\x01hSysop menu");
		console.crlf();
		console.attributes = "NB";
		for (var i = 0; i < console.screen_columns-1; ++i)
			console.print(CP437_BOX_DRAWINGS_HORIZONTAL_DOUBLE);
		console.crlf();
		var validKeys = "1Q"; // Clear high scores, Quit
		console.print("\x01c1\x01y\x01h) \x01bClear high scores\x01n");
		console.print("     \x01cQ\x01y\x01h)\x01buit\x01n");
		// If there is an inter-BBS scores JSON file, then add some options to manage that
		if (file_exists(gScriptExecDir + "server/" + "gttrivia.json"))
		{
			validKeys += "23";
			console.crlf();
			console.print("\x01gInter-BBS scores\x01n");
			console.crlf();
			console.attributes = "KH";
			for (var i = 0; i < 16; ++i)
				console.print(CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE);
			console.attributes = "N";
			console.crlf();
			console.print("\x01c2\x01y\x01h) \x01bDelete user (from all systems)\x01n");
			console.crlf();
			console.print("\x01c3\x01y\x01h) \x01bDelete BBS scores\x01n");
			console.crlf();
		}
		else
			console.crlf();
		console.attributes = "NB";
		for (var i = 0; i < console.screen_columns-1; ++i)
			console.print(CP437_BOX_DRAWINGS_HORIZONTAL_DOUBLE);
		console.attributes = "N";
		console.crlf();
		console.print("\x01cYour choice\x01g\x01h: \x01c");
		var userChoice = console.getkeys(validKeys, -1, K_UPPER).toString();
		console.attributes = "N";
		if (userChoice.length == 0 || userChoice == "Q")
			continueOn = false;
		else if (userChoice == "1")
		{
			console.crlf();
			if (file_exists(SCORES_FILENAME))
			{
				if (!console.noyes("\x01y\x01hAre you SURE you want to clear the scores\x01b"))
				{
					file_remove(SCORES_FILENAME);
					console.print("\x01n\x01c\x01hThe score file has been deleted.");
				}
			}
			else
				console.print("\x01n\x01c\x01hThere is no score file yet.");
			console.print("\x01n");
			console.crlf();
		}
		else if (userChoice == "2")
		{
			// Delete user from all systems from server scores
			console.print("\x01cPlayer name\x01g\x01h: \x01c");
			var playerName = console.getstr("", -1, K_UPRLWR);
			if (playerName.length > 0)
			{
				if (!console.noyes("\x01y\x01hAre you SURE you want to remove \x01g" + playerName + "\x01b"))
				{
					var localJSONServicePort = getJSONSvcPortFromServicesIni();
					var jsonClient = new JSONClient("127.0.0.1", localJSONServicePort);
					try
					{
						var data = jsonClient.read(gSettings.remoteServer.gtTriviaScope, "SCORES", JSON_DB_LOCK_READ);
						if (typeof(data) === "object" && data.hasOwnProperty("systems"))
						{
							for (var BBS_ID in data.systems)
							{
								if (data.systems[BBS_ID].hasOwnProperty("user_scores"))
								{
									var playerNameUpper = playerName.toUpperCase();
									for (var playerName in data.systems[BBS_ID].user_scores)
									{
										if (playerName.toUpperCase() == playerNameUpper)
										{
											var JSONLocation = format("SCORES.systems.%s.user_scores.%s", BBS_ID, playerName);
											jsonClient.remove(gSettings.remoteServer.gtTriviaScope, JSONLocation, JSON_DB_LOCK_WRITE);
										}
									}
								}
							}
						}
					}
					catch (error)
					{
						console.print("* " + error + "\r\n");
						log(LOG_ERR, GAME_NAME + " - Deleting user from server scores: Line " + error.lineNumber + ": " + error);
						bbs.log_str(GAME_NAME + " - Deleting user from server scores: Line " + error.lineNumber + ": " + error);
					}
					jsonClient.disconnect();
				}
			}
		}
		else if (userChoice == "3")
		{
			// Delete BBS from server scores
			console.print("\x01cBBS name\x01g\x01h: \x01c");
			var BBSName = console.getstr("", -1, K_UPRLWR);
			if (BBSName.length > 0)
			{
				if (!console.noyes("\x01y\x01hAre you SURE you want to remove \x01g" + BBSName + "\x01b"))
				{
					var localJSONServicePort = getJSONSvcPortFromServicesIni();
					var jsonClient = new JSONClient("127.0.0.1", localJSONServicePort);
					try
					{
						var data = jsonClient.read(gSettings.remoteServer.gtTriviaScope, "SCORES", JSON_DB_LOCK_READ);
						if (typeof(data) === "object" && data.hasOwnProperty("systems"))
						{
							var BBSNameUpper = BBSName.toUpperCase();
							for (var BBS_ID in data.systems)
							{
								if (data.systems[BBS_ID].hasOwnProperty("bbs_name") && data.systems[BBS_ID].bbs_name.toUpperCase() == BBSNameUpper)
								{
									var JSONLocation = format("SCORES.systems.%s", BBS_ID);
									jsonClient.remove(gSettings.remoteServer.gtTriviaScope, JSONLocation, JSON_DB_LOCK_WRITE);
								}
							}
						}
					}
					catch (error)
					{
						console.print("* " + error + "\r\n");
						log(LOG_ERR, GAME_NAME + " - Deleting user from server scores: Line " + error.lineNumber + ": " + error);
						bbs.log_str(GAME_NAME + " - Deleting user from server scores: Line " + error.lineNumber + ": " + error);
					}
					jsonClient.disconnect();
				}
			}
		}
	}
}

// Returns a BBS ID to use for JSON (the QWK ID if existing; otherwise, the BBS name with
// spaces converted to underscores)
function getBBSIDForJSON()
{
	var BBS_ID = "";
	if (system.qwk_id.length > 0)
		BBS_ID = system.qwk_id;
	else
		BBS_ID = system.name.replace(/ /g, "_");
	return BBS_ID;
}

// Posts all users' scores from the local scores file to a message sub-board
//
// Parameters:
//  pSubCode: The internal code of the sub-board to post the scores to
function postGTTriviaScoresToSubBoard(pSubCode)
{
	if (typeof(pSubCode) !== "string" || !msg_area.sub.hasOwnProperty(pSubCode))
		return false;

	// Prepare the user scores for posting in the message sub-board
	// JSON location: For the BBS name, use the QWK ID if available, but if not, use the system name and replace spaces
	// with underscores (since spaces may cause issues in JSON property names)
	var BBS_ID = getBBSIDForJSON();
	var scoresForThisBBS = {};
	scoresForThisBBS[BBS_ID] = {};
	scoresForThisBBS[BBS_ID].bbs_name = system.name;
	scoresForThisBBS[BBS_ID].user_scores = {};

	// Read the scores file to see if the user has an existing score in there already
	var scoresFile = new File(SCORES_FILENAME);
	if (file_exists(SCORES_FILENAME))
	{
		if (scoresFile.open("r"))
		{
			var scoreFileContents = scoresFile.read(scoresFile.length);
			scoresFile.close();
			try
			{
				scoresForThisBBS[BBS_ID].user_scores = JSON.parse(scoreFileContents);
			}
			catch (error)
			{
				scoresForThisBBS[BBS_ID].user_scores = {};
				log(LOG_ERR, GAME_NAME + " - Loading scores: Line " + error.lineNumber + ": " + error);
				bbs.log_str(GAME_NAME + " - Loading scores: Line " + error.lineNumber + ": " + error);
			}
		}
	}
	if (typeof(scoresForThisBBS[BBS_ID]) !== "object")
		scoresForThisBBS[BBS_ID].user_scores = {};

	if (Object.keys(scoresForThisBBS[BBS_ID].user_scores).length === 0)
		return false;

	var postSuccessful = false;
	var dataMsgbase = new MsgBase(pSubCode);
	if (dataMsgbase.open())
	{
		// Create the message header, and send the message.
		var header = {
			to: GAME_NAME, // "Good Time Trivia"
			from: system.username(1),
			from_ext: 1,
			subject: system.name
			//from_net_type: NET_NONE,
			//to_net_type: NET_NONE
		};
		/*
		if ((dataMsgbase.settings & SUB_QNET) == SUB_QNET)
		{
			header.from_net_type = NET_QWK;
			header.to_net_type = NET_QWK;
		}
		else if ((dataMsgbase.settings & SUB_PNET) == SUB_PNET)
		{
			header.from_net_type = NET_POSTLINK;
			header.to_net_type = NET_POSTLINK;
		}
		else if ((dataMsgbase.settings & SUB_FIDO) == SUB_FIDO)
		{
			header.from_net_type = NET_FIDO;
			header.to_net_type = NET_FIDO;
		}
		else if ((dataMsgbase.settings & SUB_INET) == SUB_INET)
		{
			header.from_net_type = NET_INTERNET;
			header.to_net_type = NET_INTERNET;
		}
		*/

		//postSuccessful = dataMsgbase.save_msg(header, JSON.stringify(scoresForThisBBS));
		var message = lfexpand(JSON.stringify(scoresForThisBBS, null, 1));
		message += " --- " + GAME_NAME + " " + GAME_VERSION + " (" + GAME_VER_DATE + ")";
		postSuccessful = dataMsgbase.save_msg(header, message);

		dataMsgbase.close();
	}
	return postSuccessful;
}

// Reads trivia scores from a sub-board and posts on the host system (if configured)
//
// Parameters:
//  pSubCode: An internal code of a sub-board to read the game scores from
//
// Return value: Boolean - Whether or not the score update succeeded
function readGTTriviaScoresFromSubBoard(pSubCode)
{
	if (typeof(pSubCode) !== "string" || !msg_area.sub.hasOwnProperty(pSubCode))
		return false;

	// For logging
	var subBoardInfoStr = msg_area.sub[subCode].name + " - " + msg_area.sub[subCode].description;
	log(LOG_INFO, format("%s - Reading score posts from sub-board %s (%s)", GAME_NAME, pSubCode, subBoardInfoStr));

	// For posting to the local JSON server, get the configured JSON service port number
	var localJSONServicePort = getJSONSvcPortFromServicesIni();
	if (localJSONServicePort <= 0)
	{
		log(LOG_ERR, format("%s - Local JSON service port is invalid (%d)", GAME_NAME, localJSONServicePort));
		return false;
	}

	var scoreUpdateSucceeded = true;
	var dataMsgbase = new MsgBase(pSubCode);
	if (dataMsgbase.open())
	{
		try
		{
			// Create the JSON Client object for updating the scores on the local JSON DB server.
			// For each user score in the JSON object, if their last time is after the current last
			// time in the server's JSON, then post the user's score.
			var jsonClient = new JSONClient("127.0.0.1", localJSONServicePort);

			var to_crc = crc16_calc(GAME_NAME.toLowerCase());
			var index = dataMsgbase.get_index();
			for (var i = 0; index && i < index.length; i++)
			{
				var idx = index[i];
				if ((idx.attr & MSG_DELETE) == MSG_DELETE || idx.to != to_crc)
					continue;

				var msgHdr = dataMsgbase.get_msg_header(true, idx.offset);
				if (!msgHdr)
					continue;
				if (/*!msgHdr.from_net_type ||*/ msgHdr.to != GAME_NAME)
					continue;

				var msgBody = dataMsgbase.get_msg_body(msgHdr, false, false, false);
				if (msgBody == null || msgBody.length == 0) //if (!msgBody)
					continue;

				log(LOG_INFO, "Scores message imported at " + strftime("%Y-%m-%d %H:%M:%S", msgHdr.when_imported_time));
				// Clean up the message body so that it only has JSON
				var txtIdx = msgBody.indexOf(" --- " + GAME_NAME);
				if (txtIdx > 0)
					msgBody = msgBody.substr(0, txtIdx);
				// Parse the JSON from the message, and then go through the BBSes and users
				// in it.  For any user scores that are more recent than what's on the server,
				// post those to the server.
				try
				{
					var scoresObjFromMsg = JSON.parse(msgBody);
					// For each user score, if their last time is after the current last time in the
					// server's JSON, then post the user's score.
					for (var BBS_ID in scoresObjFromMsg)
					{
						// Lock on the BBS name location in the JSON, do the writes, and unlock when we're done
						jsonClient.lock(gSettings.remoteServer.gtTriviaScope, gSettings.remoteServer.BBSJSONLocation, JSON_DB_LOCK_WRITE);
						// Ensure the BBS name on the server has been set
						var JSONLocation = gSettings.remoteServer.scoresJSONLocation + ".systems." + BBS_ID + ".bbs_name";
						jsonClient.write(gSettings.remoteServer.gtTriviaScope, JSONLocation, system.name);
						for (var userID in scoresObjFromMsg[BBS_ID].user_scores)
						{
							// For logging
							var msgLastTimeFormatted = strftime("%Y-%m-%d %H:%M:%S", scoresObjFromMsg[BBS_ID].user_scores[userID].last_time);
							var serverLastTimeFormatted = "";

							// Read the current user's scores from the server and compare the user's last_time
							// from the message in the sub-board with the one from the server, and only update
							// if newer.
							JSONLocation = gSettings.remoteServer.scoresJSONLocation + ".systems." + BBS_ID + ".user_scores." + userID;
							var serverUserScoreData = jsonClient.read(gSettings.remoteServer.gtTriviaScope, JSONLocation);
							var postUserScoresToServer = false;
							if (typeof(serverUserScoreData) === "object")
							{
								postUserScoresToServer = (scoresObjFromMsg[BBS_ID].user_scores[userID].last_time > serverUserScoreData.last_time);
								serverLastTimeFormatted = strftime("%Y-%m-%d %H:%M:%S", serverUserScoreData.last_time)
							}
							else
							{
								postUserScoresToServer = true;
								serverLastTimeFormatted = "N/A";
							}

							// Log the user, BBS, and date of the scores seen
							var logMsg = format("%s - Saw scores for %s on %s; in message: %s, on server: %s; will update server scores: %s",
												GAME_NAME, userID, scoresObjFromMsg[BBS_ID].bbs_name, msgLastTimeFormatted, serverLastTimeFormatted,
												postUserScoresToServer);
							log(LOG_INFO, logMsg);

							// If the scores from the message are newer, write the scores on the server
							if (postUserScoresToServer)
							{
								JSONLocation = gSettings.remoteServer.scoresJSONLocation + ".systems." + BBS_ID + ".user_scores." + userID;
								jsonClient.write(gSettings.remoteServer.gtTriviaScope, JSONLocation, scoresObjFromMsg[BBS_ID].user_scores[userID]);
							}
							// Now that we've written the user scores, unlock this BBS in the JSON
							jsonClient.unlock(gSettings.remoteServer.gtTriviaScope, gSettings.remoteServer.BBSJSONLocation);
						}
					}
				}
				catch (error)
				{
					scoreUpdateSucceeded = false;
					console.print("* Line " + error.lineNumber + ": " + error);
					console.crlf();
					log(LOG_ERR, GAME_NAME + " - Updating scores on server: Line " + error.lineNumber + ": " + error);
					bbs.log_str(GAME_NAME + " - Updating scores on server: Line " + error.lineNumber + ": " + error);
				}
			}

			jsonClient.disconnect();
		}
		catch (error)
		{
			scoreUpdateSucceeded = false;
			console.print("* Line " + error.lineNumber + ": " + error);
			console.crlf();
			log(LOG_ERR, GAME_NAME + " - Connecting to JSON DB server (for scores update): Line " + error.lineNumber + ": " + error);
			bbs.log_str(GAME_NAME + " - Connecting to JSON DB server (for scores update): Line " + error.lineNumber + ": " + error);
		}

		dataMsgbase.close();
	}
	else
	{
		var errMsg = format("%s - Unable to open sub-board %s (%s)", GAME_NAME, pSubCode, subBoardInfoStr);
		log(LOG_ERR, errMsg);
	}

	log(LOG_INFO, format("%s - End of reading score posts from sub-board %s (%s).  All succeeded: %s", GAME_NAME, pSubCode,
	                     subBoardInfoStr, scoreUpdateSucceeded));

	return scoreUpdateSucceeded;
}

function add_commas(val, pad)
{
	var s = val.toString();
	s = s.replace(/([0-9]+)([0-9]{3})$/,"$1,$2");
	while (s.search(/[0-9]{4}/)!=-1)
		s = s.replace(/([0-9]+)([0-9]{3}),/g,"$1,$2,");
	while (s.length < pad)
		s = " " + s;
	return(s);
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
//
// Return value: The string with whitespace trimmed
function trimSpaces(pString, pLeading, pMultiple, pTrailing)
{
	var leading = true;
	var multiple = true;
	var trailing = true;
	if (typeof(pLeading) != "undefined")
		leading = pLeading;
	if (typeof(pMultiple) != "undefined")
		multiple = pMultiple;
	if (typeof(pTrailing) != "undefined")
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

// Parses command-line arguments.  Returns an object with settings/actions specified.
//
// Parameters:
//  argv: The array of command-line arguments
//
// Return value: An object with the following properties:
//  debug: Boolean: Whether or not to enable debugging
//  postScoresToSubBoard: Boolean: Whether or not to post scores in the configured sub-board. If this is
//                        enabled, scores are to be posted and then the script should exit.
//  readScoresFromSubBoard: Boolean: Whether or not to read scores from the configured sub-board.
//                          If this is enabled, scores are to be posted and then the script should exit.
function parseCmdLineArgs(argv)
{
	var retObj = {
		debug: false,
		postScoresToSubBoard: false,
		readScoresFromSubBoard: false
	};

	if (!Array.isArray(argv))
		return retObj;

	var postScoresToSubOpt = "POST_SCORES_TO_SUBBOARD";
	var readScoresFromSubOpt = "READ_SCORES_FROM_SUBBOARD";
	for (var i = 0; i < argv.length; ++i)
	{
		var argUpper = argv[i].toUpperCase();
		if (argUpper == "DEBUG" || argUpper == "-DEBUG" || argUpper == "--DEBUG")
			retObj.debug = true;
		else if (argUpper == postScoresToSubOpt || argUpper == "-" + postScoresToSubOpt || argUpper == "--" + postScoresToSubOpt)
			retObj.postScoresToSubBoard = true;
		else if (argUpper == readScoresFromSubOpt || argUpper == "-" + readScoresFromSubOpt || argUpper == "--" + readScoresFromSubOpt)
			retObj.readScoresFromSubBoard = true;
	}

	return retObj;
}

