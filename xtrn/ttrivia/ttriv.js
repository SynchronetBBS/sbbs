// ttriv.js - Tournament Trivia for Synchronet BBS
// Ported from the C++ BBS door game by Evan Elias
//
// A multi-node trivia game where questions are presented automatically,
// players type their answers, and points are awarded based on how few
// clues were needed. Supports multiple simultaneous players across nodes.

"use strict";

require("sbbsdefs.js", "K_NONE");
require(js.exec_dir + "ttriv_ui.js", "printColor");
require(js.exec_dir + "ttriv_game.js", "initGame");

var VERSION = "1.0js";
var exitGame = false;
var lastTimerCheck = 0;
var lastScoreSave = 0;

// -----------------------------------------------------------------------
// Main entry point
// -----------------------------------------------------------------------

function main()
{
	var argsObj = parseArgs(argv);
	if (argsObj.settingsFilename.length == 0)
	{
		printColor("ERROR: An empty settings filename was specified!", "\x01N\x01H\x01R");
		console.attributes = "N";
		console.pause();
		return;
	}

	// dataDir and settingsFilename is declared in triv_game.js
	// If no full path is specified, then assume the settings filename is in the data dir
	var justFilename = file_getname(argsObj.settingsFilename);
	if (argsObj.settingsFilename.indexOf(justFilename) == 0)
		settingsFilename = dataDir + justFilename;
	else
		settingsFilename = argsObj.settingsFilename;

	// If the settings file doesn't exist, then create it
	if (!file_exists(settingsFilename))
		saveSettings(settingsFilename, DEFAULT_SETTINGS);

	initGame();
	checkMaintenance();

	if (questions.length === 0)
	{
		printColor("ERROR: No question files found!", "\x01N\x01H\x01R");
		printColor("Place database.enc and/or custom.txt in the game directory.", "\x01N\x01H\x01Y");
		printColor("Game directory: " + js.exec_dir, "\x01N\x01W");
		console.pause();
		return;
	}

	registerPlayer();

	splashScreen();

	broadcastMessage(">>> " + user.alias + " has entered the game!", "\x01N\x01H\x01G");
	// Advance past our own join message
	getNewMessages();

	showMenu();
	listPlayers();
	displayCurrentQuestion();

	// Refresh question timing so the first clue doesn't fire immediately
	// (time spent on splash/menu screens would otherwise count against it)
	if (acquireLock("game"))
	{
		var state = readGameState();
		if (state && state.clueNumber === 0)
		{
			state.startTime = time();
			state.lastClueTime = time();
			writeGameState(state);
		}
		releaseLock("game");
	}

	gameLoop();

	broadcastMessage(">>> " + user.alias + " has left the game.", "\x01N\x01H\x01G");
}

// -----------------------------------------------------------------------
// Splash screen
// -----------------------------------------------------------------------

function splashScreen()
{
	console.line_counter = 0;
	console.clear("N");
	console.crlf();
	textBox("TOURNAMENT TRIVIA v" + VERSION, "\x01N\x01H\x01W", "\x01N\x01H\x01C", true);
	centerText("(c) 2003 Evan Elias   Synchronet JS port", "\x01N\x01W");
	centerText(questions.length + " questions available in the database", "\x01N\x01H\x01Y");
	console.crlf();

	// Show previous month's winner
	if (acquireLock("players"))
	{
		var data = readPlayerData();
		releaseLock("players");
		if (data.previousWinner && data.previousWinner !== "none")
		{
			centerText("Last month's winner was " + data.previousWinner +
					   " with " + data.previousHighScore + " points!", "\x01N\x01H\x01C");
		}
	}

	// Show top scores
	showScores();

	console.crlf();

	if (myScore === 0)
	{
		// New player - offer instructions
		if (console.yesno("\x01n\x01h\x01yWould you like instructions on how to play"))
			showInstructions();
	}
	else
		console.pause();
}

// -----------------------------------------------------------------------
// Instructions
// -----------------------------------------------------------------------

function showInstructions()
{
	console.crlf();
	printColor("HOW TO PLAY", "\x01N\x01H\x01Y");
	printColor("===========", "\x01N\x01H\x01Y");
	console.crlf();
	printColor("Tournament Trivia is a multi-node trivia game. Questions are", "\x01N\x01W");
	printColor("displayed automatically, and you simply type your answer and", "\x01N\x01W");
	printColor("press Enter. If your answer is wrong, matching letters from", "\x01N\x01W");
	printColor("the beginning of the answer will be revealed in the clue.", "\x01N\x01W");
	console.crlf();
	printColor("Clues are given periodically to help you guess the answer.", "\x01N\x01W");
	printColor("The fewer clues needed, the more points you earn:", "\x01N\x01W");
	printColor("  0 clues: 3 points  |  1 clue: 2 points  |  2+ clues: 1 point", "\x01N\x01H\x01C");
	printColor("  (Single player mode always awards 1 point)", "\x01N\x01H\x01K");
	console.crlf();
	printColor("Type MENU or ? for a list of available commands.", "\x01N\x01H\x01Y");
	console.crlf();
	console.pause();
}

// -----------------------------------------------------------------------
// Menu display
// -----------------------------------------------------------------------

function showMenu()
{
	console.clear("N");
	console.line_counter = 0;
	console.crlf();
	textBox("TOURNAMENT TRIVIA v" + VERSION, "\x01N\x01H\x01W", "\x01N\x01H\x01C", true);
	console.crlf();

	menuOption("?", "Display this menu", "\x01N\x01H\x01W", "\x01N\x01H\x01C", "\x01N\x01W");
	menuOption("S", "View top 10 scores this month", "\x01N\x01H\x01W", "\x01N\x01H\x01C", "\x01N\x01W");
	menuOption("P", "List online players", "\x01N\x01H\x01W", "\x01N\x01H\x01C", "\x01N\x01W");
	menuOption("N", "Skip to next question (vote)", "\x01N\x01H\x01W", "\x01N\x01H\x01C", "\x01N\x01W");
	menuOption("T", "Send a message to another player", "\x01N\x01H\x01W", "\x01N\x01H\x01C", "\x01N\x01W");
	menuOption("C", "Report a correction", "\x01N\x01H\x01W", "\x01N\x01H\x01C", "\x01N\x01W");
	menuOption("H", "Help / instructions", "\x01N\x01H\x01W", "\x01N\x01H\x01C", "\x01N\x01W");
	if (user.is_sysop)
		menuOption("!", "Sysop configuration", "\x01N\x01H\x01W", "\x01N\x01H\x01C", "\x01N\x01W");
	menuOption("X", "Exit the game", "\x01N\x01H\x01W", "\x01N\x01H\x01C", "\x01N\x01W");
	console.crlf();
	printColor("Or just type your answer to the current question!", "\x01N\x01H\x01Y");
	console.crlf();
}

// -----------------------------------------------------------------------
// Score display
// -----------------------------------------------------------------------

function showScores()
{
	console.crlf();
	underlineText("THIS MONTH'S HIGH SCORES", CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE, "\x01N\x01H\x01C", "\x01N\x01B", true);

	var scores = getTopScores(10);

	if (scores.length === 0)
	{
		centerText("No scores yet this month.", "\x01N\x01H\x01K");
		return;
	}

	var oneColumn = (scores.length <= 5);

	for (var n = 0; n < 5; ++n)
	{
		var left;
		if (n < scores.length)
		{
			var alias = scores[n].alias;
			if (alias.length > 18) alias = alias.substring(0, 18);
			left = format(" %d. %-18s [%4d points]", n + 1, alias, scores[n].score);
		}
		else
			left = format(" %d. %-18s [     points]", n + 1, "");

		if (oneColumn)
		{
			centerText(left, "\x01N\x01C");
			continue;
		}

		printColor(left + "    ", "\x01N\x01C", 0);
		printColor(CP437_BOX_DRAWINGS_LIGHT_VERTICAL + " ", "\x01N\x01B", 0);

		var ri = n + 5;
		var right;
		if (ri < scores.length)
		{
			var alias2 = scores[ri].alias;
			if (alias2.length > 18) alias2 = alias2.substring(0, 18);
			right = format("%2d. %-18s [%4d points] ", ri + 1, alias2, scores[ri].score);
		}
		else
			right = format("%2d. %-18s [     points] ", ri + 1, "");
		printColor(right, "\x01N\x01C");
	}
}

// -----------------------------------------------------------------------
// Player list
// -----------------------------------------------------------------------

function listPlayers()
{
	var state = readGameState();
	if (!state || !state.activePlayers) return;

	var names = [];
	for (var k in state.activePlayers)
		names.push(state.activePlayers[k].alias);

	if (names.length > 0)
		printColor("Online players: " + names.join(", "), "\x01N\x01H\x01G");
}

// -----------------------------------------------------------------------
// Question display
// -----------------------------------------------------------------------

function displayCurrentQuestion()
{
	var state = readGameState();
	if (!state || !state.question || state.question.length === 0)
	{
		printColor("Waiting for a question...", "\x01N\x01H\x01K");
		return;
	}
	displayQuestion(state, false);
}

function displayQuestion(state, isNew)
{
	console.crlf();
	if (isNew)
		centerText("* * *  NEW QUESTION  * * *", "\x01N\x01H\x01W");
	console.crlf();

	// Word-wrap question text with indent
	var indent = "Question:       ";
	var padStr = "                "; // same width as indent
	var maxWidth = 79 - indent.length;
	var words = state.question.split(" ");
	var line = "";
	var firstLine = true;

	for (var i = 0; i < words.length; ++i)
	{
		if (line.length > 0 && line.length + 1 + words[i].length > maxWidth)
		{
			if (firstLine)
			{
				printColor(indent, "\x01N\x01H\x01C", 0);
				firstLine = false;
			}
			else
				printColor(padStr, "\x01N\x01W", 0);
			printColor(line, "\x01N\x01H\x01W");
			line = words[i];
		}
		else
		{
			if (line.length > 0) line += " ";
			line += words[i];
		}
	}
	if (line.length > 0)
	{
		if (firstLine)
			printColor(indent, "\x01N\x01H\x01C", 0);
		else
			printColor(padStr, "\x01N\x01W", 0);
		printColor(line, "\x01N\x01H\x01W");
	}

	displayClue(state);
}

function displayClue(state)
{
	var header;
	if (state.clueNumber === 0)
		header = "Answer:         ";
	else if (state.clueNumber >= settings.maxClues)
		header = "Last Clue:      ";
	else
	{
		var names = ["", "First Clue:     ", "Second Clue:    ", "Third Clue:     "];
		header = names[state.clueNumber] || "Clue:           ";
	}

	printColor(header, "\x01N\x01H\x01C", 0);
	printColor(state.clue, "\x01N\x01H\x01B");
}

// -----------------------------------------------------------------------
// Main game loop
// -----------------------------------------------------------------------

function gameLoop()
{
	var inputBuf = "";
	var promptShown = false;

	lastTimerCheck = time();
	lastScoreSave = time();

	while (!exitGame && bbs.online && !js.terminated)
	{
		// Show prompt if not shown
		if (!promptShown)
		{
			console.line_counter = 0;
			console.crlf();
			console.print("\x01N\x01H\x01K[Score: " + myScore + "] ");
			printColor("Your answer? ", "\x01N\x01H\x01C", 0);
			console.print("\x01N\x01H\x01W");
			promptShown = true;
		}

		// Non-blocking key input with 500ms timeout
		var key = console.inkey(K_NONE, 500);

		// Check for broadcast messages from other players
		var msgs = getNewMessages();
		if (msgs.length > 0)
		{
			// Clear current line, show messages, then redisplay prompt
			console.print("\r");
			console.cleartoeol("N");

			for (var i = 0; i < msgs.length; ++i)
			{
				console.print(msgs[i].attr + msgs[i].text);
				console.crlf();
			}

			// Redisplay prompt and partial input
			console.print("\x01N\x01H\x01K[Score: " + myScore + "] ");
			printColor("Your answer? ", "\x01N\x01H\x01C", 0);
			console.print("\x01N\x01H\x01W" + inputBuf);
		}

		// Check timers (question expiry, clue generation)
		var timerResult = checkTimers();
		if (timerResult)
		{
			console.print("\r");
			console.cleartoeol("N");

			if (timerResult.type === "question")
				displayQuestion(timerResult.state, true);
			else if (timerResult.type === "clue")
			{
				displayClue(timerResult.state);
				console.crlf();
			}

			// Redisplay prompt and partial input
			console.print("\x01N\x01H\x01K[Score: " + myScore + "] ");
			printColor("Your answer? ", "\x01N\x01H\x01C", 0);
			console.print("\x01N\x01H\x01W" + inputBuf);
		}

		// Save score periodically
		if (time() - lastScoreSave >= 60)
		{
			savePlayerScore();
			lastScoreSave = time();
		}

		if (!key) continue;

		var code = ascii(key);

		// Enter key: process input
		if (key === "\r" || key === "\n")
		{
			console.crlf();
			if (inputBuf.length > 0)
				processInput(inputBuf);
			else
			{
				// Blank input: redisplay current question
				displayCurrentQuestion();
			}
			inputBuf = "";
			promptShown = false;
			continue;
		}

		// Backspace/Delete
		if (code === 8 || code === 127)
		{
			if (inputBuf.length > 0)
			{
				inputBuf = inputBuf.substring(0, inputBuf.length - 1);
				console.print("\b \b");
			}
			continue;
		}

		// Printable character
		if (code >= 32 && inputBuf.length < 80)
		{
			inputBuf += key;
			console.print(key);
		}
	}
}

// -----------------------------------------------------------------------
// Timer checking (question expiry, clue generation, stale player cleanup)
// -----------------------------------------------------------------------

function checkTimers()
{
	if (time() - lastTimerCheck < 1) return null;
	lastTimerCheck = time();

	if (!acquireLock("game")) return null;
	var state = readGameState();
	if (!state || !state.startTime || state.startTime === 0)
	{
		releaseLock("game");
		return null;
	}

	var now = time();
	var advanced = false;

	// Try to advance question timer
	if (now - state.startTime >= settings.questionFrequency)
	{
		if (selectNewQuestion(state))
		{
			state.questionId = (state.questionId || 0) + 1;
			advanced = true;
		}
	}
	// Try to advance clue timer
	else if (now - state.lastClueTime >= clueFrequency() &&
			 state.clueNumber < settings.maxClues)
	{
		if (generateClue(state))
			advanced = true;
	}

	// Periodically clean stale players
	cleanStalePlayers(state);

	if (advanced) writeGameState(state);
	releaseLock("game");

	// Detect display changes (whether we advanced or another node did)
	var qid = state.questionId || 0;
	var cnum = state.clueNumber || 0;

	if (qid !== lastDisplayedQuestionId)
	{
		lastDisplayedQuestionId = qid;
		lastDisplayedClueNum = cnum;
		return { type: "question", state: state };
	}
	if (cnum !== lastDisplayedClueNum)
	{
		lastDisplayedClueNum = cnum;
		return { type: "clue", state: state };
	}

	return null;
}

// -----------------------------------------------------------------------
// Input processing
// -----------------------------------------------------------------------

function processInput(text)
{
	var trimmed = text.trim();
	if (trimmed.length === 0) return;

	// Display command
	if (trimmed === ".d" || trimmed.toLowerCase() === "display")
	{
		displayCurrentQuestion();
		return;
	}

	// Try as answer first (matches original game behavior)
	if (!acquireLock("game"))
	{
		printColor("Game busy, please try again.", "\x01N\x01H\x01R");
		return;
	}
	var state = readGameState();
	if (!state || !state.answer)
	{
		releaseLock("game");
		return;
	}

	// Check if the question changed while we were typing
	var qid = state.questionId || 0;
	if (qid !== lastDisplayedQuestionId)
	{
		releaseLock("game");
		lastDisplayedQuestionId = qid;
		lastDisplayedClueNum = state.clueNumber || 0;
		displayQuestion(state, true);
		printColor("(The question changed while you were typing.)", "\x01N\x01H\x01K");
		return;
	}

	if (checkAnswer(trimmed, state))
	{
		// CORRECT ANSWER!
		var pts = pointValue(state);
		var correctAnswer = state.answer; // Save before overwriting
		myScore += pts;

		// Pick next question immediately
		selectNewQuestion(state);
		state.questionId = (state.questionId || 0) + 1;
		writeGameState(state);
		releaseLock("game");

		lastDisplayedQuestionId = state.questionId;
		lastDisplayedClueNum = 0;

		broadcastMessage(">>> " + user.alias + " got the correct answer: " +
						 correctAnswer.toUpperCase() + "!", "\x01N\x01H\x01G");
		// Advance past our own broadcast
		getNewMessages();

		printColor("*** CORRECT! You earned " + pts + " point" +
				   (pts !== 1 ? "s" : "") + "! (Total: " + myScore + ") ***", "\x01N\x01H\x01G");

		displayQuestion(state, true);
		savePlayerScore();
		return;
	}

	// Wrong answer - save updated clue (checkAnswer reveals matching prefix)
	writeGameState(state);
	releaseLock("game");

	// Try as command
	if (tryCommand(trimmed)) return;

	// Not a command and not a correct answer - broadcast as chat and show clue
	broadcastMessage("<" + user.alias + "> " + trimmed, "\x01N\x01W");
	getNewMessages(); // advance past our own chat message
	displayClue(state);
}

// -----------------------------------------------------------------------
// Command dispatch
// -----------------------------------------------------------------------

function tryCommand(text)
{
	var parts = text.split(/\s+/);
	var cmd = parts[0].toUpperCase();
	var args = parts.slice(1).join(" ");

	switch (cmd)
	{
		case "WHO":
		case "PLAYERS":
		case "PL":
		case "P":
			cmdWho();
			return true;
		case "SKIP":
		case "NEXT":
		case "N":
			cmdSkip();
			return true;
		case "HELP":
		case "H":
			showInstructions();
			return true;
		case "WHISPER":
		case "TELL":
		case "T":
			cmdTell(args);
			return true;
		case "SUBMIT":
		case "SUB":
			cmdSubmit();
			return true;
		case "CORRECTION":
		case "CORRECT":
		case "C":
			cmdCorrection();
			return true;
		case "SCORES":
		case "SC":
		case "TOPTEN":
		case "TOP":
		case "S":
			showScores();
			return true;
		case "EXIT":
		case "QUIT":
		case "X":
			cmdExit();
			return true;
		case "SYSOP":
		case "CONFIG":
		case "CONFIGURE":
		case "!":
			cmdSysopConfig();
			return true;
		case "MENU":
		case "?":
			showMenu();
			displayCurrentQuestion();
			return true;
	}
	return false;
}

// -----------------------------------------------------------------------
// Commands
// -----------------------------------------------------------------------

function cmdWho()
{
	var state = readGameState();
	if (!state || !state.activePlayers)
	{
		printColor("No players online.", "\x01N\x01H\x01K");
		return;
	}

	console.crlf();
	underlineText("PLAYERS ONLINE", CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE, "\x01N\x01H\x01C", "\x01N\x01B", false);

	for (var k in state.activePlayers)
	{
		var p = state.activePlayers[k];
		var marker = (parseInt(k) === bbs.node_num) ? " (you)" : "";
		printColor("  " + p.alias + marker, "\x01N\x01C");
	}
	console.crlf();
}

function cmdSkip()
{
	if (!acquireLock("game")) return;
	var state = readGameState();
	if (!state) { releaseLock("game"); return; }

	// Check if already requested
	if (state.skipRequests && state.skipRequests.indexOf(bbs.node_num) >= 0)
	{
		releaseLock("game");
		printColor("You already requested that this question be skipped!", "\x01N\x01H\x01R");
		return;
	}

	if (!state.skipRequests) state.skipRequests = [];
	state.skipRequests.push(bbs.node_num);

	var playerCount = getActivePlayerCount(state);

	if (playerCount <= 1)
	{
		// Single player: accelerate question timer
		state.startTime -= Math.floor(settings.questionFrequency / 2);
		state.clueNumber = Math.floor((time() - state.startTime) / clueFrequency());
		writeGameState(state);
		releaseLock("game");
		printColor("Speeding up the question timer...", "\x01N\x01H\x01Y");
		return;
	}

	if (state.skipRequests.length >= playerCount)
	{
		// All players agreed to skip
		selectNewQuestion(state);
		state.questionId = (state.questionId || 0) + 1;
		writeGameState(state);
		releaseLock("game");

		lastDisplayedQuestionId = state.questionId;
		lastDisplayedClueNum = 0;

		broadcastMessage(">>> Question skipped by vote!", "\x01N\x01H\x01Y");
		getNewMessages();
		displayQuestion(state, true);
		return;
	}

	writeGameState(state);
	releaseLock("game");
	broadcastMessage(">>> " + user.alias + " has requested that this question be skipped.", "\x01N\x01H\x01Y");
	getNewMessages();
	printColor("Skip request noted. (" + state.skipRequests.length + "/" +
			   playerCount + " votes)", "\x01N\x01H\x01Y");
}

function cmdTell(args)
{
	if (!args || args.length === 0)
	{
		printColor("Syntax:   TELL <user> <message>", "\x01N\x01H\x01Y");
		return;
	}

	var state = readGameState();
	if (!state || !state.activePlayers)
	{
		printColor("No other players online.", "\x01N\x01H\x01R");
		return;
	}

	var parts = args.split(/\s+/);
	var targetNode = -1;
	var targetAlias = "";
	var msgStartIdx = 1;

	// Find target player by name prefix match
	for (var k in state.activePlayers)
	{
		var p = state.activePlayers[k];
		if (parseInt(k) === bbs.node_num) continue;

		if (p.alias.toLowerCase().indexOf(parts[0].toLowerCase()) === 0)
		{
			targetNode = parseInt(k);
			targetAlias = p.alias;
			break;
		}
	}

	if (targetNode < 0)
	{
		printColor("Player not found.", "\x01N\x01H\x01R");
		return;
	}

	var msg = parts.slice(msgStartIdx).join(" ");
	if (msg.length === 0)
	{
		printColor("Syntax:   TELL <user> <message>", "\x01N\x01H\x01Y");
		return;
	}

	// Send private message via Synchronet node messaging
	system.put_node_message(targetNode,
		"\x01n\x01h\x01g" + user.alias + " tells you: \x01w" + msg);
	printColor("Message sent to " + targetAlias + ".", "\x01N\x01H\x01G");
}

function cmdSubmit()
{
	console.crlf();
	printColor("QUESTION SUBMISSIONS", "\x01N\x01H\x01Y");
	printColor("====================", "\x01N\x01H\x01Y");
	console.crlf();
	printColor("To submit new trivia questions, contact your sysop.", "\x01N\x01W");
	printColor("Questions should be placed in custom.txt (one question", "\x01N\x01W");
	printColor("per two lines: question on first line, answer on second).", "\x01N\x01W");
	console.crlf();
}

function cmdCorrection()
{
	var state = readGameState();
	if (!state) return;

	var files = settings.questionFiles || DEFAULT_SETTINGS.questionFiles;

	console.crlf();
	underlineText("CORRECTION REPORT", CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE, "\x01N\x01H\x01C", "\x01N\x01B", false);

	if (state.trackLine && state.trackLine[0] >= 0 && state.trackFile)
	{
		printColor("Previous question:", "\x01N\x01H\x01Y");
		var fi = state.trackFile[0];
		printColor("  In file " + (files[fi] || "unknown") +
				   ", question " + state.trackLine[0], "\x01N\x01W");
	}

	if (state.trackLine && state.trackLine[1] >= 0 && state.trackFile)
	{
		printColor("Current question:", "\x01N\x01H\x01Y");
		var fi = state.trackFile[1];
		printColor("  In file " + (files[fi] || "unknown") +
				   ", question " + state.trackLine[1], "\x01N\x01W");
	}

	console.crlf();
	printColor("Please report corrections to your sysop with the above info.", "\x01N\x01W");
	console.crlf();
}

function cmdExit()
{
	if (console.yesno("\x01n\x01h\x01yAre you sure you want to exit"))
		exitGame = true;
}

// -----------------------------------------------------------------------
// Sysop configuration
// -----------------------------------------------------------------------

function cmdSysopConfig()
{
	if (!user.is_sysop)
	{
		printColor("This command is for sysops only.", "\x01N\x01H\x01R");
		return;
	}

	var configLoop = true;
	while (configLoop && bbs.online && !js.terminated)
	{
		console.line_counter = 0;
		console.crlf();
		underlineText("SYSOP CONFIGURATION", CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE, "\x01N\x01H\x01C", "\x01N\x01B", false);
		console.crlf();

		menuOption("F", "Question Frequency: " + settings.questionFrequency + " seconds",
				   "\x01N\x01H\x01W", "\x01N\x01H\x01C", "\x01N\x01W");
		menuOption("M", "Max Clues: " + settings.maxClues, "\x01N\x01H\x01W", "\x01N\x01H\x01C", "\x01N\x01W");
		menuOption("S", "Show Sysops on Score List: " + (settings.listSysops ? "Yes" : "No"),
				   "\x01N\x01H\x01W", "\x01N\x01H\x01C", "\x01N\x01W");
		menuOption("T", "Player Timeout: " + settings.playerTimeout + " seconds",
				   "\x01N\x01H\x01W", "\x01N\x01H\x01C", "\x01N\x01W");

		console.crlf();
		var files = settings.questionFiles || [];
		for (var i = 0; i < 10; ++i)
		{
			var label = ((i + 1) % 10).toString();
			var fname = (i < files.length && files[i]) ? files[i] : "(none)";
			menuOption(label, "Question File #" + (i + 1) + ": " + fname,
					   "\x01N\x01H\x01W", "\x01N\x01H\x01C", "\x01N\x01W");
		}

		console.crlf();
		menuOption("X", "Exit configuration", "\x01N\x01H\x01W", "\x01N\x01H\x01C", "\x01N\x01W");
		console.crlf();

		printColor("Choice: ", "\x01N\x01H\x01C", 0);
		var key = console.getkey(K_UPPER);
		console.crlf();

		switch (key)
		{
			case "F":
				printColor("Question frequency (25-75 seconds): ", "\x01N\x01H\x01Y", 0);
				var val = console.getnum(75);
				if (val >= 25 && val <= 75)
				{
					settings.questionFrequency = val;
					saveSettings(settingsFilename, settings);
					printColor("Question frequency set to " + val + " seconds.", "\x01N\x01H\x01G");
				}
				else
					printColor("Value must be between 25 and 75.", "\x01N\x01H\x01R");
				break;
			case "M":
				printColor("Max clues per question (0-4): ", "\x01N\x01H\x01Y", 0);
				var val = console.getnum(4);
				if (val >= 0 && val <= 4)
				{
					settings.maxClues = val;
					saveSettings(settingsFilename, settings);
					printColor("Max clues set to " + val + ".", "\x01N\x01H\x01G");
				}
				else
					printColor("Value must be between 0 and 4.", "\x01N\x01H\x01R");
				break;
			case "S":
				settings.listSysops = !settings.listSysops;
				saveSettings(settingsFilename, settings);
				printColor("Sysops " + (settings.listSysops ? "will" : "will not") +
						   " be shown on score list.", "\x01N\x01H\x01Y");
				break;
			case "T":
				printColor("Player timeout (60-600 seconds): ", "\x01N\x01H\x01Y", 0);
				var val = console.getnum(600);
				if (val >= 60 && val <= 600)
				{
					settings.playerTimeout = val;
					saveSettings(settingsFilename, settings);
					printColor("Player timeout set to " + val + " seconds.", "\x01N\x01H\x01G");
				}
				else
					printColor("Value must be between 60 and 600.", "\x01N\x01H\x01R");
				break;
			case "1": case "2": case "3": case "4": case "5":
			case "6": case "7": case "8": case "9": case "0":
				var idx = (key === "0") ? 9 : parseInt(key) - 1;
				printColor("Enter filename (or blank for none): ", "\x01N\x01H\x01Y", 0);
				var fname = console.getstr("", 20, K_EDIT | K_LINE);
				if (!settings.questionFiles)
					settings.questionFiles = DEFAULT_SETTINGS.questionFiles.slice();
				while (settings.questionFiles.length <= idx)
					settings.questionFiles.push("");
				settings.questionFiles[idx] = fname || "";
				saveSettings(settingsFilename, settings);
				// Reload questions with new file list
				loadQuestions();
				printColor(questions.length + " questions now available.", "\x01N\x01H\x01Y");
				break;
			case "X":
				configLoop = false;
				break;
		}
	}

	printColor("Configuration saved.", "\x01N\x01H\x01G");
	showMenu();
	displayCurrentQuestion();
}

// -----------------------------------------------------------------------
// Run the game with cleanup
// -----------------------------------------------------------------------

try
{
	main();
}
catch (e)
{
	log("Tournament Trivia error: " + e);
	printColor("An error occurred: " + e, "\x01N\x01H\x01R");
	console.pause();
}
finally
{
	savePlayerScore();
	unregisterPlayer();
}
