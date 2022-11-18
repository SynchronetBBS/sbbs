/*
This is a trivia game for Synchronet.  It currently has a basic Q&A format, and supports multiple
trivia categories.  User's high scores and trivia category stats (total score, last time played)
are saved to a file.

Date       Author            Description
2022-11-17 Eric Oulashin     Version 1.00
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
var GAME_VERSION = "1.00";
var GAME_VER_DATE = "2022-11-17";

// Load required .js libraries
var requireFnExists = (typeof(require) === "function");
if (requireFnExists)
	require("sbbsdefs.js", "P_NONE");
else
	load("sbbsdefs.js");


// Determine the location of this script (its startup directory).
// The code for figuring this out is a trick that was created by Deuce,
// suggested by Rob Swindell.  I've shortened the code a little.
var gStartupPath = '.';
var gThisScriptFilename = "";
try { throw dig.dist(dist); } catch(e) {
	gStartupPath = backslash(e.fileName.replace(/[\/\\][^\/\\]*$/,''));
	gThisScriptFilename = file_getname(e.fileName);
}

// Characters for display
// Box-drawing/border characters: Single-line
var UPPER_LEFT_SINGLE = "\xDA";
var HORIZONTAL_SINGLE = "\xC4";
var UPPER_RIGHT_SINGLE = "\xBF";
var VERTICAL_SINGLE = "\xB3";
var LOWER_LEFT_SINGLE = "\xC0";
var LOWER_RIGHT_SINGLE = "\xD9";
var T_SINGLE = "\xC2";
var LEFT_T_SINGLE = "\xC3";
var RIGHT_T_SINGLE = "\xB4";
var BOTTOM_T_SINGLE = "\xC1";
var CROSS_SINGLE = "\xC5";
// Box-drawing/border characters: Double-line
var UPPER_LEFT_DOUBLE = "\xC9";
var HORIZONTAL_DOUBLE = "\xCD";
var UPPER_RIGHT_DOUBLE = "\xBB";
var VERTICAL_DOUBLE = "\xBA";
var LOWER_LEFT_DOUBLE = "\xC8";
var LOWER_RIGHT_DOUBLE = "\xBC";
var T_DOUBLE = "\xCB";
var LEFT_T_DOUBLE = "\xCC";
var RIGHT_T_DOUBLE = "\xB9";
var BOTTOM_T_DOUBLE = "\xCA";
var CROSS_DOUBLE = "\xCE";
// Box-drawing/border characters: Vertical single-line with horizontal double-line
var UPPER_LEFT_VSINGLE_HDOUBLE = "\xD5";
var UPPER_RIGHT_VSINGLE_HDOUBLE = "\xB8";
var LOWER_LEFT_VSINGLE_HDOUBLE = "\xD4";
var LOWER_RIGHT_VSINGLE_HDOUBLE = "\xBE";
// Other special characters
var DOT_CHAR = "\xF9";
var CHECK_CHAR = "\xFB";
var THIN_RECTANGLE_LEFT = "\xDD";
var THIN_RECTANGLE_RIGHT = "\xDE";
var BLOCK1 = "\xB0"; // Dimmest block
var BLOCK2 = "\xB1";
var BLOCK3 = "\xB2";
var BLOCK4 = "\xDB"; // Brightest block
var MID_BLOCK = "\xDC";
var TALL_UPPER_MID_BLOCK = "\xFE";
var UPPER_CENTER_BLOCK = "\xDF";
var LOWER_CENTER_BLOCK = "\xDC";



// Maximum Levenshtein distance (inclusive) to consisder an answer matching (when appropriate)
var MAX_LEVENSHTEIN_DISTANCE = 2;
// Scores filename
var SCORES_FILENAME = gStartupPath + "scores.json";
// Semaphore filename to use when saving the user's score to try to prevent multiple instances
// from overwriting the score on each other
var SCORES_SEMAPHORE_FILENAME = gStartupPath + "SCORES_SEMAPHORE.tmp";
// Main menu actions
var ACTION_PLAY = 0;
var ACTION_SHOW_HELP_SCREEN = 1;
var ACTION_SHOW_HIGH_SCORES = 2;
var ACTION_QUIT = 3;
var ACTION_SYSOP_CLEAR_SCORES = 4;


// Upon exit for any reason, make sure the scores semaphore filename doesn't exist so future instances don't get frozen
//js.on_exit("if (file_exists(\"" + SCORES_SEMAPHORE_FILENAME + "\")) file_remove(\"" + SCORES_SEMAPHORE_FILENAME + "\");");


// Display the program logo
displayProgramLogo(true, false);

// Load the settings from the .ini file
var gSettings = loadSettings(gStartupPath);


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
			showScores(false);
			break;
		case ACTION_SYSOP_CLEAR_SCORES:
			if (user.is_sysop)
			{
				console.print("\x01n");
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
			break;
		case ACTION_QUIT:
		default:
			continueOn = false;
			break;
	}
}


console.print("\x01n\x01cReturning to \x01y\x01h" + system.name + "\x01n\x01c...\x01n");
mswait(1000);
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
		console.print("\x01n" + gSettings.colors.error + "There are no trivia sections available!\x01n");
		console.crlf();
		console.pause();
		return 2;
	}
	// If there's only 1 trivia category, then choose it automatically.  Otherwise,
	// let the user choose a trivia category.
	var chosenSectionIdx = 0;
	if (qaFilenameInfo.length > 1)
	{
		console.print("\x01n" + gSettings.colors.triviaCategoryHdr + "Choose a trivia section:\x01n");
		console.crlf();
		for (var i = 0; i < qaFilenameInfo.length; ++i)
		{
			var categoryNum = i + 1;
			var categoryNumStr = format(gSettings.colors.triviaCategoryListNumbers + "%3d", i + 1);
			var categorySep = gSettings.colors.triviaCategoryListSeparator + ": ";
			var categoryName = gSettings.colors.triviaCategoryName + qaFilenameInfo[i].sectionName;
			console.print("\x01n" + categoryNumStr + "\x01n" + categorySep + "\x01n" + categoryName);
			console.crlf();
		}
		console.print("\x01n" + gSettings.colors.categoryNumPrompt + "Choose\x01n" + gSettings.colors.categoryNumPromptSeparator + ": \x01n" + gSettings.colors.categoryNumInput);
		var chosenSectionNum = +(console.getnum(qaFilenameInfo.length));
		if (chosenSectionNum < 1 || chosenSectionNum > qaFilenameInfo.length)
		{
			//console.print("\x01n" + gSettings.colors.error + "That is not a valid section number\x01n");
			//console.print("\x01n\x01cReturning to \x01y\x01h" + system.name + "\x01n\x01c...\x01n");
			//mswait(1500);
			return 0;
		}
		chosenSectionIdx = chosenSectionNum - 1;
	}

	console.print("\x01n" + gSettings.colors.triviaCategoryHdr + "Playing " + gSettings.colors.triviaCategoryName + qaFilenameInfo[chosenSectionIdx].sectionName + "\x01n");
	console.crlf();

	// Load and parse the section filename into questions, answers, and points
	var QAArray = parseQAFilename(qaFilenameInfo[chosenSectionIdx].filename);
	shuffle(QAArray);
	console.print("There are " + QAArray.length + " questions in total.");
	console.crlf();
	// Each element in QAArray is an object with the following properties:
	// question
	// answer
	// numPoints
	console.print("\x01n\x01gWill ask \x01h" + gSettings.behavior.numQuestionsPerPlay + "\x01n\x01g questions.\x01n");
	console.crlf();
	console.print("\x01n\x01gYou can answer \x01hQ\x01n\x01g at any time to quit.\x01n");
	console.crlf();
	var userPoints = 0;
	var maxNumQuestions = gSettings.behavior.numQuestionsPerPlay;
	if (QAArray.length < maxNumQuestions)
		maxNumQuestions = QAArray.length;
	var continueQuestioning = true;
	for (var i = 0; i < maxNumQuestions && continueQuestioning; ++i)
	{
		var questionNumTxt = format("\x01n%sQuestion \x01n%s%d \x01n%sof \x01n%s%d\x01n%s:\x01n",
		   gSettings.colors.questionHdr, gSettings.colors.questionHdrNum, (i+1),
		   gSettings.colors.questionHdr, gSettings.colors.questionHdrNum,
		   maxNumQuestions, gSettings.colors.questionHdr);

		// Show the question and prompt the user for a response.  Let the user
		// try multiple times; when wrong, display a hint.
		var gotAnswerCorrect = false;
		for (var tryI = 0; tryI < gSettings.behavior.numTriesPerQuestion && !gotAnswerCorrect && continueQuestioning; ++tryI)
		{
			// Print the question
			console.print(questionNumTxt);
			console.crlf();
			printWithWordWrap(gSettings.colors.question, QAArray[i].question);
			console.print("\x01n" + gSettings.colors.questionHdr + "# points: \x01n" +
			              gSettings.colors.questionHdrNum + QAArray[i].numPoints + "\x01n\r\n");
			// If this is after the first try, then give a clue
			if (tryI > 0)
			{
				console.print("\x01n" + gSettings.colors.clueHdr + "Clue:\x01n");
				console.crlf();
				console.print(gSettings.colors.clue + partiallyHiddenStr(QAArray[i].answer, tryI-1) + "\x01n");
				console.crlf();
				
			}
			// Prompt for an answer
			console.print(gSettings.colors.answerPrompt + "Answer (try " + +(tryI+1) + " of " + gSettings.behavior.numTriesPerQuestion +
			              ")\x01n" + gSettings.colors.answerPromptSep + ": \x01n"/* + gSettings.colors.answerInput*/);
			console.crlf();
			console.print(gSettings.colors.answerInput);
			var userResponse = console.getstr();
			console.print("\x01n");
			var responseCheckRetObj = checkUserResponse(QAArray[i].answer, userResponse);
			if (responseCheckRetObj.userChoseQuit)
			{
				continueQuestioning = false;
				break;
			}
			else if (responseCheckRetObj.userInputMatchedAnswer)
			{
				gotAnswerCorrect = true;
				userPoints += QAArray[i].numPoints;
				console.print("\x01n" + gSettings.colors.questionHdr + "Correct!\x01n");
				console.crlf();
			}
			else
			{
				console.print("\x01n" + gSettings.colors.questionHdr + "* Incorrect!");
				console.crlf();
				if (tryI < gSettings.behavior.numTriesPerQuestion)
					console.crlf();
			}
		}
		// If the user didn't get the answer correct, output the correct answer
		if (!gotAnswerCorrect)
		{
			console.print("\x01n" + gSettings.colors.questionHdr + "The answer was:");
			console.crlf();
			printWithWordWrap(gSettings.colors.answerAfterIncorrect, QAArray[i].answer);
			console.print("\x01n");
		}

		// Print the user's score so far
		console.crlf();
		console.print("\x01n" + gSettings.colors.scoreSoFarText + "Your score so far: \x01n" + gSettings.colors.userScore + userPoints + "\x01n");
		console.crlf();

		console.crlf();
	}
	console.print("\x01n" + gSettings.colors.questionHdr + "Your score: \x01n" + gSettings.colors.userScore + userPoints + "\x01n");
	console.crlf();
	console.print("\x01n\x01b\x01hUpdating the scores file...");
	console.crlf();
	updateScoresFile(userPoints, qaFilenameInfo[chosenSectionIdx].sectionName);
	console.print("Done.\x01n");
	console.crlf();
	return 0;
}


// Loads settings from the .ini file
//
// Parameters:
//  gStartupPath: The path to the directory where the .ini file is located
//
// Return value: An object with 'behavior' and 'color' sections with the settings loaded from the .ini file
function loadSettings(pStartupPath)
{
	var settings = {};
	var cfgFileName = genFullPathCfgFilename("gttrivia.ini", pStartupPath);
	var iniFile = new File(cfgFileName);
	if (iniFile.open("r"))
	{
		settings.behavior = iniFile.iniGetObject("BEHAVIOR");
		settings.colors = iniFile.iniGetObject("COLORS");
		settings.category_ars = iniFile.iniGetObject("CATEGORY_ARS");

		// Ensure the actual expected setting name & color names exist in the settings
		if (typeof(settings.behavior) !== "object")
			settings.behavior = {};
		if (typeof(settings.colors) !== "object")
			settings.colors = {};
		if (typeof(settings.category_ars) !== "object")
			settings.category_ars = {};

		if (typeof(settings.behavior.numQuestionsPerPlay) !== "number")
			settings.behavior.numQuestionsPerPlay = 10;
		if (typeof(settings.behavior.numTriesPerQuestion) !== "number")
			settings.behavior.numTriesPerQuestion = 4;
		if (typeof(settings.behavior.maxNumPlayerScoresToDisplay) !== "number")
			settings.behavior.maxNumPlayerScoresToDisplay = 10;

		if (typeof(settings.colors.error) !== "string")
			settings.colors.error = "\x01y\x01h";
		if (typeof(settings.colors.triviaCategoryHdr) !== "string")
			settings.colors.triviaCategoryHdr = "\x01m\x01h";
		if (typeof(settings.colors.triviaCategoryListNumbers) !== "string")
			settings.colors.triviaCategoryListNumbers = "\x01c\x01h";
		if (typeof(settings.colors.triviaCategoryListSeparator) !== "string")
			settings.colors.triviaCategoryListSeparator = "\x01g\x01h";
		if (typeof(settings.colors.triviaCategoryName) !== "string")
			settings.colors.triviaCategoryName = "\x01c";
		if (typeof(settings.colors.categoryNumPrompt) !== "string")
			settings.colors.categoryNumPrompt = "\x01c";
		if (typeof(settings.colors.categoryNumPromptSeparator) !== "string")
			settings.colors.categoryNumPromptSeparator = "\x01g\x01h";
		if (typeof(settings.colors.categoryNumInput) !== "string")
			settings.colors.categoryNumInput = "\x01c\x01h";
		if (typeof(settings.colors.questionHdr) !== "string")
			settings.colors.questionHdr = "\x01m";
		if (typeof(settings.colors.questionHdrNum) !== "string")
			settings.colors.questionHdrNum = "\x01c\x01h";
		if (typeof(settings.colors.question) !== "string")
			settings.colors.question = "\x01b\x01h";
		if (typeof(settings.colors.answerPrompt) !== "string")
			settings.colors.answerPrompt = "\x01c";
		if (typeof(settings.colors.answerPromptSep) !== "string")
			settings.colors.answerPromptSep = "\x01g\x01h";
		if (typeof(settings.colors.answerInput) !== "string")
			settings.colors.answerInput = "\x01c\x01h";
		if (typeof(settings.colors.userScore) !== "string")
			settings.colors.userScore = "\x01c\x01h";
		if (typeof(settings.colors.scoreSoFarText) !== "string")
			settings.colors.scoreSoFarText = "\x01c";
		if (typeof(settings.colors.clueHdr) !== "string")
			settings.colors.clueHdr = "\x01r\x01h";
		if (typeof(settings.colors.clue) !== "string")
			settings.colors.clue = "\x01g\x01h";
		if (typeof(settings.colors.answerAfterIncorrect) !== "string")
			settings.colors.answerAfterIncorrect = "\x01g";
		
		// Sanity checking
		if (settings.behavior.numQuestionsPerPlay <= 0)
			settings.behavior.numQuestionsPerPlay = 10;
		if (settings.behavior.numTriesPerQuestion <= 0)
			settings.behavior.numTriesPerQuestion = 3;
		if (settings.behavior.maxNumPlayerScoresToDisplay <= 0)
			settings.behavior.maxNumPlayerScoresToDisplay = 10;

		// For each color, replace any instances of specifying the control character in substWord with the actual control character
		for (var prop in settings.colors)
			settings.colors[prop] = settings.colors[prop].replace(/\\[xX]01/g, "\x01").replace(/\\[xX]1/g, "\x01").replace(/\\1/g, "\x01");
		iniFile.close();
	}
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
			var defaultPath = backslash(pDefaultPath);
			fullyPathedFilename = defaultPath + pFilename;
		}
		else
			fullyPathedFilename = pFilename;
	}
	return fullyPathedFilename;
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

	console.printfile(gStartupPath + "gttrivia.asc", P_NONE, 80);

	if (typeof(pPauseAfter) === "boolean" && pPauseAfter)
		console.pause();
}

// Shows the main menu, prompts the user for a choice, and returns the user's choice as
// one of the following values:
// ACTION_PLAY
// ACTION_SHOW_HIGH_SCORES
// ACTION_SHOW_HELP_SCREEN
// ACTION_QUIT
function doMainMenu()
{
	console.print("\x01n\x01y\x01hMain Menu\x01n");
	console.crlf();
	console.print("\x01b");
	for (var i = 0; i < console.screen_columns-1; ++i)
		console.print(HORIZONTAL_DOUBLE);
	console.crlf();
	console.print("\x01c1\x01y\x01h) \x01bPlay \x01n");
	console.print("\x01c2\x01y\x01h) \x01bHelp \x01n");
	console.print("\x01c3\x01y\x01h) \x01bShow high scores \x01n");
	if (user.is_sysop)
		console.print("\x01c9\x01y\x01h) \x01bClear high scores \x01n"); // Option 9
	console.print("\x01cQ\x01y\x01h)\x01buit");
	console.crlf();
	console.print("\x01n");
	console.print("\x01b");
	for (var i = 0; i < console.screen_columns-1; ++i)
		console.print(HORIZONTAL_DOUBLE);
	console.crlf();
	console.print("\x01cYour choice\x01g\x01h: \x01c");
	var menuAction = ACTION_PLAY;
	//var userChoice = console.getnum(3);
	var validKeys = "123qQ";
	if (user.is_sysop)
		validKeys += "9"; // Clear scores
	var userChoice = console.getkeys(validKeys, -1, K_UPPER).toString();
	console.print("\x01n");
	if (userChoice.length == 0 || userChoice == "Q")
		menuAction = ACTION_QUIT;
	else if (userChoice == "1")
		menuAction = ACTION_PLAY;
	else if (userChoice == "2")
		menuAction = ACTION_SHOW_HELP_SCREEN;
	else if (userChoice == "3")
		menuAction = ACTION_SHOW_HIGH_SCORES;
	else if (userChoice == "9" && user.is_sysop)
		menuAction = ACTION_SYSOP_CLEAR_SCORES;
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
	var QAFilenames = directory(gStartupPath + "qa/*.qa");
	for (var i = 0; i < QAFilenames.length; ++i)
	{
		// Get the section name - Start by removing the .qa filename extension
		var filenameExtension = file_getext(QAFilenames[i]);
		var sectionName = file_getname(QAFilenames[i]);
		var charIdx = sectionName.lastIndexOf(".");
		if (charIdx > -1)
			sectionName = sectionName.substring(0, charIdx);
		// Currently, sectionName is the filename without the extension.
		// See if there is an ARS string for this in the configuration, and if so,
		// only add it if the ARS string passes for the user.
		var addThisSection = true;
		if (gSettings.category_ars.hasOwnProperty(sectionName))
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

// Parses a Q&A filename
//
// Return value: An array of objects containing the following properties:
//               question: The trivia question
//               answer: The answer to the trivia question
//               numPoints: The number of points to award for the correct answer
function parseQAFilename(pQAFilenameFullPath)
{
	if (!file_exists(pQAFilenameFullPath))
		return [];

	// Open the file and parse it.  Each line has:
	// question,answer,points
	var QA_Array = [];
	var QAFile = new File(pQAFilenameFullPath);
	if (QAFile.open("r"))
	{
		var theQuestion = "";
		var theAnswer = "";
		var theNumPoints = -1;
		while (!QAFile.eof)
		{
			var fileLine = QAFile.readln(2048);
			// I've seen some cases where readln() doesn't return a string
			if (typeof(fileLine) !== "string")
				continue;
			fileLine = fileLine.trim();
			if (fileLine.length == 0)
				continue;

			if (theQuestion.length > 0 && theAnswer.length > 0 && theNumPoints > -1)
			{
				QA_Array.push(new QA(theQuestion, theAnswer, +theNumPoints));
				theQuestion = "";
				theAnswer = "";
				theNumPoints = -1;
			}

			if (theQuestion.length == 0)
				theQuestion = fileLine;
			else if (theAnswer.length == 0)
				theAnswer = fileLine;
			else if (theNumPoints < 1)
			{
				theNumPoints = +(fileLine);
				if (theNumPoints < 1)
					theNumPoints = 10;
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
			QA_Array.push(new QA(theQuestion, theAnswer, +theNumPoints));
	}
	return QA_Array;
}
// QA object constructor
function QA(pQuestion, pAnswer, pNumPoints)
{
	this.question = pQuestion;
	this.answer = pAnswer;
	this.numPoints = pNumPoints;
	if (this.numPoints < 1)
		this.numPoints = 10;
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
// Return value: An object with the following properties:
//               userChoseQuit: Boolean: Whether or not the user chose to quit
//               userInputMatchedAnswer: Boolean: Whether or not the user's answer matches the given answer to the question
function checkUserResponse(pAnswer, pUserInput)
{
	var retObj = {
		userChoseQuit: false,
		userInputMatchedAnswer: false
	};

	if (typeof(pAnswer) !== "string" || typeof(pUserInput) !== "string")
		return retObj;
	if (pUserInput.length == 0)
		return retObj;

	// Convert both to uppercase for case-insensitive matching
	var answerUpper = pAnswer.toUpperCase();
	var userInputUpper = pUserInput.toUpperCase();
	
	if (userInputUpper == "Q")
	{
		retObj.userChoseQuit = true;
		return retObj;
	}

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
function updateScoresFile(pUserCurrentGameScore, pLastSectionName)
{
	if (typeof(pUserCurrentGameScore) !== "number")
		return false;

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
			var scoreFileArray = scoresFile.readAll();
			scoresFile.close();
			var scoreFileContents = "";
			for (var i = 0; i < scoreFileArray.length; ++i)
				scoreFileContents += (scoreFileArray[i] + "\n");
			try
			{
				scoresObj = JSON.parse(scoreFileContents);
			}
			catch (error)
			{
				log(LOG_ERR, GAME_NAME + " - Loading scores: " + error);
				bbs.log_str(GAME_NAME + " - Loading scores: " + error);
			}
		}
	}
	if (typeof(scoresObj) !== "object")
		scoresObj = {};

	// Add/update the user's score, and save the scores file
	try
	{
		// Score object example (note: last_time is a UNIX time):
		// Username:
		//   category_stats:
		//     category_1:
		//       last_time: 166000
		//       total_score: 20
		//     category_2:
		//       last_time: 146000
		//       total_score: 80
		//  total_score: 100
		//  last_score: 20
		//  last_trivia_category: category_1
		//  last_time: 166000
		var currentTime = time();
		var userCategoryStatsPropName = "category_stats";
		// Ensure the score object has an object for the current user
		if (!scoresObj.hasOwnProperty(user.alias))
			scoresObj[user.alias] = {};
		// Ensure the user object in the scores object has a category_total_scores object
		if (!scoresObj[user.alias].hasOwnProperty(userCategoryStatsPropName))
			scoresObj[user.alias][userCategoryStatsPropName] = {};
		// Add/update the user's score for the category in their category_total_scores object
		if (!scoresObj[user.alias][userCategoryStatsPropName].hasOwnProperty(pLastSectionName))
			scoresObj[user.alias][userCategoryStatsPropName][pLastSectionName] = {};
		scoresObj[user.alias][userCategoryStatsPropName][pLastSectionName].last_time = currentTime;
		if (!scoresObj[user.alias][userCategoryStatsPropName][pLastSectionName].hasOwnProperty("total_score"))
			scoresObj[user.alias][userCategoryStatsPropName][pLastSectionName].total_score = pUserCurrentGameScore;
		else
			scoresObj[user.alias][userCategoryStatsPropName][pLastSectionName].total_score += pUserCurrentGameScore;
		// Update the user's grand total score value
		if (scoresObj[user.alias].hasOwnProperty("total_score"))
			scoresObj[user.alias].total_score += pUserCurrentGameScore;
		else
			scoresObj[user.alias].total_score = pUserCurrentGameScore;
		scoresObj[user.alias].last_score = pUserCurrentGameScore;
		scoresObj[user.alias].last_trivia_category = lastSectionName;
		scoresObj[user.alias].last_time = currentTime;
	}
	catch (error)
	{
		console.print("* " + error);
		console.crlf();
		log(LOG_ERR, GAME_NAME + " - Updating trivia score object: " + error);
		bbs.log_str(GAME_NAME + " - Updating trivia score object: " + error);
	}
	scoresFile = new File(SCORES_FILENAME);
	if (scoresFile.open("w"))
	{
		scoresFile.write(JSON.stringify(scoresObj));
		scoresFile.close();
	}

	// Delete the semaphore file
	file_remove(SCORES_SEMAPHORE_FILENAME);
}

// Shows the saved scores
//
// Parameters:
//  pPauseAtEnd: Boolean - Whether or not to pause at the end.  The default is false.
function showScores(pPauseAtEnd)
{
	console.print("\x01n");
	console.crlf();
	var sortedScores = [];
	// Read the scores file to see if the user has an existing score in there already
	if (file_exists(SCORES_FILENAME))
	{
		var scoresFile = new File(SCORES_FILENAME);
		if (scoresFile.open("r"))
		{
			var scoreFileArray = scoresFile.readAll();
			scoresFile.close();
			var scoreFileContents = "";
			for (var i = 0; i < scoreFileArray.length; ++i)
				scoreFileContents += (scoreFileArray[i] + "\n");
			var scoresObj = JSON.parse(scoreFileContents);
			for (var prop in scoresObj)
			{
				sortedScores.push({
					name: prop,
					total_score: scoresObj[prop].total_score,
					last_score: scoresObj[prop].last_score,
					last_trivia_category: scoresObj[prop].last_trivia_category,
					last_time: scoresObj[prop].last_time
				});
			}
		}
		// Sort the array: High total score first
		sortedScores.sort(function(objA, objB) {
			if (objA.total_score > objB.total_score)
				return -1;
			else if (objA.total_score < objB.total_score)
				return 1;
			else
				return 0;
		});
	}
	// Print the scores if there are any
	if (sortedScores.length > 0)
	{
		// Make the format string for printf()
		var scoreWidth = 6;
		var dateWidth = 10;
		var categoryWidth = 15;
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
		// Print the scores
		console.center("\x01g\x01hHigh Scores\x01n");
		console.crlf();
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
			console.print(HORIZONTAL_DOUBLE);
		console.crlf();
		// Print the list of high scores
		console.print("\x01g");
		if (console.screen_columns >= 80)
		{
			for (var i = 0; i < sortedScores.length && i < gSettings.behavior.maxNumPlayerScoresToDisplay; ++i)
			{
				var playerName = sortedScores[i].name.substr(0, nameWidth);
				var lastDate = strftime("%Y-%m-%d", sortedScores[i].last_time);
				var sectionName = sortedScores[i].last_trivia_category.substr(0, categoryWidth);
				printf(formatStr, playerName, lastDate, sectionName, sortedScores[i].total_score, sortedScores[i].last_score);
				console.crlf();
			}
		}
		else
		{
			for (var i = 0; i < sortedScores.length && i < gSettings.behavior.maxNumPlayerScoresToDisplay; ++i)
			{
				printf(formatStr, sortedScores[i].name.substr(0, nameWidth), sortedScores[i].total_score, sortedScores[i].last_score);
				console.crlf();
			}
		}
		console.print("\x01n\x01b");
		for (var i = 0; i < console.screen_columns-1; ++i)
			console.print(HORIZONTAL_DOUBLE);
		console.crlf();
	}
	else
		console.print("\x01gThere are no saved scores yet.\x01n");
	console.crlf();
	if (typeof(pPauseAtEnd) === "boolean" && pPauseAtEnd)
		console.pause();
}

// Displays the game help to the user
function showHelpScreen()
{
	// Construct the header text lines only once.
	if (typeof(showHelpScreen.headerLines) === "undefined")
	{
		showHelpScreen.headerLines = [];

		var headerText = "\x01n\x01c" + GAME_NAME + " Help\x01n";
		var headerTextLen = strip_ctrl(headerText).length;

		// Top border
		var headerTextStr = "\x01h\x01c" + UPPER_LEFT_SINGLE;
		for (var i = 0; i < headerTextLen + 2; ++i)
			headerTextStr += HORIZONTAL_SINGLE;
		headerTextStr += UPPER_RIGHT_SINGLE;
		showHelpScreen.headerLines.push(headerTextStr);

		// Middle line: Header text string
		headerTextStr = VERTICAL_SINGLE + "\x01n " + headerText + " \x01n\x01h\x01c" + VERTICAL_SINGLE;
		showHelpScreen.headerLines.push(headerTextStr);

		// Lower border
		headerTextStr = LOWER_LEFT_SINGLE;
		for (var i = 0; i < headerTextLen + 2; ++i)
			headerTextStr += HORIZONTAL_SINGLE;
		headerTextStr += LOWER_RIGHT_SINGLE;
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
	var helpText = "\x01n\x01c" + GAME_NAME + " is a trivia game with a freeform answer format.  The game "
	             + "starts with a main menu, allowing you to play, show high scores, or quit.";
	printWithWordWrap(null, helpText);
	console.crlf();
	console.print("\x01n\x01c\x01hGameplay:");
	console.crlf();
	console.print("\x01n\x01g");
	for (var i = 0; i < 9; ++i)
		console.print(HORIZONTAL_SINGLE);
	console.print("\x01n");
	console.crlf();
	helpText = "\x01n\x01cWhen starting a game, there can be potentially multiple trivia categories to "
	         + "choose from.  If there is only one category, the game will automatically start with that category.";
	printWithWordWrap(null, helpText);
	helpText = "\x01n\x01cDuring a game, you will be asked up to " + gSettings.behavior.numQuestionsPerPlay
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
	printWithWordWrap(null, helpText);
	console.print("\x01n");
	console.crlf();
	console.pause();
}

// Returns a version of a string that is masked, possibly with some of its characters unmasked
//
// Parameters:
//  pStr: A string to mask
//  pNumLettersUncovered: The number of letters to be uncovered/unmasked
//  pMaskChar: Optional - The mask character. Defaults to "*".
function partiallyHiddenStr(pStr, pNumLettersUncovered, pMaskChar)
{
	if (typeof(pStr) !== "string")
		return "";
	var maskChar = (typeof(pMaskChar) === "string" && pMaskChar.length > 0 ? pMaskChar.substr(0, 1) : "*");
	var numLetersUncovered = (typeof(pNumLettersUncovered) === "number" && pNumLettersUncovered > 0 ? pNumLettersUncovered : 0);
	var str = "";
	if (numLetersUncovered >= pStr.length)
		str = pStr;
	else
	{
		var i = 0;
		for (i = 0; i < pStr.length; ++i)
		{
			if (i < numLetersUncovered)
				str += pStr.charAt(i);
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
	/*
	var str = word_wrap(pStr, console.screen_columns-1, console.strlen(pStr), false);
	console.print(str);
	console.print("\r");
	*/
	console.putmsg(attrs + pStr, P_WORDWRAP);
	var applyNormalAttr = (typeof(pPrintNormalAttrAfterward) === "boolean" ? pPrintNormalAttrAfterward : true);
	if (applyNormalAttr)
		console.print("\x01n");
}