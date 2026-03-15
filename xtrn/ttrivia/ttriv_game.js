// ttriv_game.js - Tournament Trivia game logic for Synchronet BBS
// Handles game state, questions, scoring, and multi-node coordination

"use strict";

var dataDir = fullpath(js.exec_dir) + "data/";
var settingsFilename = dataDir + "settings.json";

var questions = [];
var lastMessageId = 0;
var myScore = 0;
var settings = null;

var lastDisplayedQuestionId = -1;
var lastDisplayedClueNum = -1;

var DEFAULT_SETTINGS = {
	maxClues: 3,
	questionFrequency: 50,
	listSysops: true,
	playerTimeout: 300,
	questionFiles: ["database.enc", "custom.txt"]
};

// -----------------------------------------------------------------------
// Initialization
// -----------------------------------------------------------------------

function initGame()
{
	// Create data directory if needed
	if (!file_isdir(dataDir))
		mkdir(dataDir);

	loadSettings(settingsFilename);
	loadQuestions();

	// Load player's current score
	if (acquireLock("players"))
	{
		var players = readPlayerData();
		for (var i = 0; i < players.records.length; ++i)
		{
			if (players.records[i].name === user.name)
			{
				myScore = players.records[i].score;
				break;
			}
		}
		releaseLock("players");
	}
}

// -----------------------------------------------------------------------
// File locking (lockfile-based with stale detection)
// -----------------------------------------------------------------------

function acquireLock(name)
{
	var lockPath = dataDir + name + ".lock";
	var maxAttempts = 50;

	for (var i = 0; i < maxAttempts; ++i)
	{
		// Stale lock detection (30 second timeout)
		if (file_exists(lockPath) && file_date(lockPath) < time() - 30)
			file_remove(lockPath);

		if (!file_exists(lockPath))
		{
			var f = new File(lockPath);
			if (f.open("w"))
			{
				f.writeln(bbs.node_num);
				f.close();
				return true;
			}
		}
		mswait(100);
	}
	return false;
}

function releaseLock(name)
{
	file_remove(dataDir + name + ".lock");
}

// -----------------------------------------------------------------------
// JSON file helpers
// -----------------------------------------------------------------------

function readJSON(path)
{
	if (!file_exists(path)) return null;
	var f = new File(path);
	if (!f.open("r")) return null;
	var text = f.read();
	f.close();
	try
	{
		return JSON.parse(text);
	}
	catch (e)
	{
		return null;
	}
}

function writeJSON(path, data)
{
	var f = new File(path);
	if (!f.open("w")) return false;
	f.write(JSON.stringify(data, null, 2));
	f.close();
	return true;
}

// -----------------------------------------------------------------------
// Settings
// -----------------------------------------------------------------------

function loadSettings(path)
{
	settings = readJSON(path);
	if (!settings)
	{
		settings = {};
		for (var k in DEFAULT_SETTINGS)
		{
			if (DEFAULT_SETTINGS[k] instanceof Array)
				settings[k] = DEFAULT_SETTINGS[k].slice();
			else
				settings[k] = DEFAULT_SETTINGS[k];
		}
		saveSettings(settingsFilename, settings);
	}
}

function saveSettings(pFilename, pSettings)
{
	writeJSON(pFilename, pSettings);
}

function clueFrequency()
{
	return Math.floor(settings.questionFrequency / (settings.maxClues + 1));
}

// -----------------------------------------------------------------------
// Question loading
// -----------------------------------------------------------------------

function decodeEncoded(text)
{
	var result = "";
	for (var n = 0; n < text.length; ++n)
	{
		var c = text.charCodeAt(n);
		if (n % 2 === 0)
			c -= 1 + (n % 4);
		else
			c -= 2;
		result += String.fromCharCode(c);
	}
	return result;
}

function loadQuestions()
{
	questions = [];
	var files = settings.questionFiles || DEFAULT_SETTINGS.questionFiles;

	for (var fi = 0; fi < files.length; ++fi)
	{
		if (!files[fi] || files[fi].length === 0) continue;

		var path = js.exec_dir + files[fi];
		var f = new File(path);
		if (!f.open("r")) continue;

		var isEncoded = (files[fi].toLowerCase().indexOf(".enc") >= 0);
		var lineNum = 0;

		while (!f.eof)
		{
			var qLine = f.readln();
			if (qLine === null) break;
			lineNum++;

			var aLine = f.readln();
			if (aLine === null) break;
			lineNum++;

			// Strip trailing CR from CRLF line endings
			qLine = qLine.replace(/\r$/, "");
			aLine = aLine.replace(/\r$/, "");

			if (qLine.length === 0 || aLine.length === 0) continue;

			if (isEncoded)
			{
				qLine = decodeEncoded(qLine);
				aLine = decodeEncoded(aLine);
			}

			// Strip trailing whitespace from answer
			aLine = aLine.replace(/\s+$/, "");

			if (qLine.length > 0 && aLine.length > 0)
			{
				questions.push({
					question: qLine.substring(0, 160),
					answer: aLine.substring(0, 80),
					fileIndex: fi,
					lineNumber: lineNum - 1
				});
			}
		}
		f.close();
	}
}

// -----------------------------------------------------------------------
// Game state management
// -----------------------------------------------------------------------

function readGameState()
{
	return readJSON(dataDir + "game_state.json");
}

function writeGameState(state)
{
	writeJSON(dataDir + "game_state.json", state);
}

function createEmptyState()
{
	return {
		question: "",
		answer: "",
		clue: "",
		clueNumber: 0,
		startTime: 0,
		lastClueTime: 0,
		skipRequests: [],
		activePlayers: {},
		trackFile: [-1, -1],
		trackLine: [-1, -1],
		usedQuestions: [],
		dbSize: 0,
		questionId: 0
	};
}

function createInitialClue(answer)
{
	var clue = "";
	for (var i = 0; i < answer.length; ++i)
	{
		var c = answer.charAt(i);
		if (/[a-zA-Z0-9]/.test(c))
			clue += ".";
		else
			clue += c;
	}
	return clue;
}

function selectNewQuestion(state)
{
	if (questions.length === 0) return false;

	var usedSet = {};
	if (state.usedQuestions)
	{
		for (var i = 0; i < state.usedQuestions.length; ++i)
			usedSet[state.usedQuestions[i]] = true;
	}

	var idx = -1;
	for (var tries = 0; tries < 5; ++tries)
	{
		idx = Math.floor(Math.random() * questions.length);
		if (!usedSet[idx]) break;
	}
	if (idx < 0) idx = 0;

	var q = questions[idx];

	// Track previous question info
	state.trackFile = [
		state.trackFile ? state.trackFile[1] : -1,
		q.fileIndex
	];
	state.trackLine = [
		state.trackLine ? state.trackLine[1] : -1,
		q.lineNumber
	];

	state.question = q.question;
	state.answer = q.answer;
	state.clue = createInitialClue(q.answer);
	state.clueNumber = 0;
	state.startTime = time();
	state.lastClueTime = time();
	state.skipRequests = [];
	state.dbIndex = idx;
	state.dbSize = questions.length;

	// Track used questions (keep last 50 to avoid immediate repeats)
	if (!state.usedQuestions) state.usedQuestions = [];
	state.usedQuestions.push(idx);
	if (state.usedQuestions.length > 50)
		state.usedQuestions = state.usedQuestions.slice(-50);

	return true;
}

// -----------------------------------------------------------------------
// Clue generation
// -----------------------------------------------------------------------

function generateClue(state)
{
	state.clueNumber++;

	if (state.clueNumber > settings.maxClues) return false;

	var answer = state.answer;
	var clue = state.clue;

	// Count unrevealed vs revealed alphanumeric characters
	var notRevealed = 0, alreadyRevealed = 0;
	for (var i = 0; i < clue.length; ++i)
	{
		if (clue.charAt(i) === ".")
			notRevealed++;
		else if (/[a-zA-Z0-9]/.test(clue.charAt(i)))
			alreadyRevealed++;
	}

	// Don't reveal if more than half already shown
	if (alreadyRevealed > notRevealed) return false;

	var lettersToReveal = state.clueNumber;
	if (lettersToReveal > Math.floor(notRevealed / 5))
		lettersToReveal = Math.floor(notRevealed / 5);
	if (lettersToReveal === 0 && alreadyRevealed === 0 && notRevealed > 1)
		lettersToReveal = 1;
	if (lettersToReveal === 0) return false;

	// Find word boundaries in the answer
	var words = [];
	var wordStart = -1;
	for (var i = 0; i <= answer.length; ++i)
	{
		if (i < answer.length && /[a-zA-Z0-9]/.test(answer.charAt(i)))
		{
			if (wordStart < 0) wordStart = i;
		}
		else
		{
			if (wordStart >= 0)
			{
				words.push({ start: wordStart, end: i });
				wordStart = -1;
			}
		}
	}

	if (words.length === 0) return false;

	// Reveal letters from random positions within random words
	var clueArr = clue.split("");
	var revealed = 0;
	var attempts = 0;
	while (revealed < lettersToReveal && attempts < 100)
	{
		var w = words[Math.floor(Math.random() * words.length)];
		// Pick a random starting position within the word
		var startPos = w.start + Math.floor(Math.random() * (w.end - w.start));
		// Scan forward from that position looking for unrevealed letters
		for (var i = startPos; i < w.end && revealed < lettersToReveal; ++i)
		{
			if (clueArr[i] === ".")
			{
				clueArr[i] = answer.charAt(i);
				revealed++;
			}
		}
		attempts++;
	}

	state.clue = clueArr.join("");
	state.lastClueTime = time();
	return true;
}

// -----------------------------------------------------------------------
// Answer checking
// -----------------------------------------------------------------------

function checkAnswer(attempt, state)
{
	// Case-insensitive exact match
	if (attempt.toLowerCase() === state.answer.toLowerCase())
		return true;

	// Reveal matching prefix letters in clue
	var maxChar = Math.min(attempt.length, state.answer.length);
	var clueArr = state.clue.split("");
	for (var n = 0; n < maxChar; ++n)
	{
		if (attempt.charAt(n).toLowerCase() === state.answer.charAt(n).toLowerCase())
			clueArr[n] = state.answer.charAt(n);
		else
			break;
	}
	state.clue = clueArr.join("");
	return false;
}

// -----------------------------------------------------------------------
// Scoring
// -----------------------------------------------------------------------

function pointValue(state)
{
	var pts = 3 - state.clueNumber;
	var playerCount = getActivePlayerCount(state);
	if (playerCount < 2 || pts < 1) return 1;
	return pts;
}

function getActivePlayerCount(state)
{
	var count = 0;
	if (state && state.activePlayers)
		for (var k in state.activePlayers) count++;
	return count;
}

// -----------------------------------------------------------------------
// Player data management
// -----------------------------------------------------------------------

function readPlayerData()
{
	var data = readJSON(dataDir + "players.json");
	if (!data)
	{
		var now = new Date();
		data = {
			month: now.getMonth() + 1,
			year: now.getFullYear(),
			previousWinner: "none",
			previousHighScore: 0,
			records: []
		};
	}
	return data;
}

function writePlayerData(data)
{
	writeJSON(dataDir + "players.json", data);
}

function checkMaintenance()
{
	if (!acquireLock("players")) return;
	var data = readPlayerData();
	var now = new Date();
	var curMonth = now.getMonth() + 1;
	var curYear = now.getFullYear();

	if (data.month !== curMonth || data.year !== curYear)
	{
		// Monthly maintenance: save winner, reset scores
		var topScore = 0;
		var topName = "none";
		for (var i = 0; i < data.records.length; ++i)
		{
			if (data.records[i].score > topScore)
			{
				topScore = data.records[i].score;
				topName = data.records[i].alias;
			}
		}
		data.previousWinner = topName;
		data.previousHighScore = topScore;
		data.records = [];
		data.month = curMonth;
		data.year = curYear;
		writePlayerData(data);
	}
	releaseLock("players");
}

function savePlayerScore()
{
	if (!acquireLock("players")) return;
	var data = readPlayerData();
	var found = false;
	for (var i = 0; i < data.records.length; ++i)
	{
		if (data.records[i].name === user.name)
		{
			data.records[i].score = myScore;
			data.records[i].alias = user.alias;
			data.records[i].isSysop = user.is_sysop;
			found = true;
			break;
		}
	}
	if (!found)
	{
		data.records.push({
			name: user.name,
			alias: user.alias,
			score: myScore,
			isSysop: user.is_sysop
		});
	}
	writePlayerData(data);
	releaseLock("players");
}

function getTopScores(count)
{
	if (!acquireLock("players"))
		return [];
	var data = readPlayerData();
	releaseLock("players");

	var records = data.records.slice();
	if (!settings.listSysops)
		records = records.filter(function (r) { return !r.isSysop; });

	records.sort(function (a, b) { return b.score - a.score; });
	return records.slice(0, count);
}

// -----------------------------------------------------------------------
// Active player registration
// -----------------------------------------------------------------------

function registerPlayer()
{
	if (!acquireLock("game")) return;
	var state = readGameState();
	if (!state) state = createEmptyState();
	if (!state.activePlayers) state.activePlayers = {};

	// Clean up stale entries for nodes that are no longer active
	cleanStalePlayers(state);

	state.activePlayers[bbs.node_num] = {
		alias: user.alias,
		name: user.name,
		isSysop: user.is_sysop,
		joinTime: time()
	};

	// If no active question, pick one
	if (!state.question || state.question.length === 0)
	{
		if (selectNewQuestion(state))
			state.questionId = (state.questionId || 0) + 1;
	}

	writeGameState(state);
	releaseLock("game");

	// Initialize display tracking
	var s = readGameState();
	if (s)
	{
		lastDisplayedQuestionId = s.questionId || 0;
		lastDisplayedClueNum = s.clueNumber || 0;
	}
}

function unregisterPlayer()
{
	if (!acquireLock("game")) return;
	var state = readGameState();
	if (state && state.activePlayers)
	{
		delete state.activePlayers[bbs.node_num];
		writeGameState(state);
	}
	releaseLock("game");
}

function cleanStalePlayers(state)
{
	if (!state || !state.activePlayers) return;
	for (var k in state.activePlayers)
	{
		var nodeIdx = parseInt(k) - 1;
		if (nodeIdx >= 0 && nodeIdx < system.node_list.length)
		{
			var nodeStatus = system.node_list[nodeIdx].status;
			// NODE_WFC=0 (waiting for caller), NODE_OFFLINE=5
			if (nodeStatus === 0 || nodeStatus === 5)
				delete state.activePlayers[k];
		}
	}
}

// -----------------------------------------------------------------------
// Broadcast message system
// -----------------------------------------------------------------------

function broadcastMessage(text, attr)
{
	if (!acquireLock("messages")) return;
	var msgs = readJSON(dataDir + "messages.json");
	if (!msgs) msgs = { nextId: 1, messages: [] };

	msgs.messages.push({
		id: msgs.nextId,
		time: time(),
		text: text,
		attr: attr || "\x01N\x01H\x01G"
	});
	msgs.nextId++;

	// Prune messages older than 60 seconds
	var cutoff = time() - 60;
	msgs.messages = msgs.messages.filter(function (m) { return m.time > cutoff; });

	writeJSON(dataDir + "messages.json", msgs);
	releaseLock("messages");
}

function getNewMessages()
{
	var msgs = readJSON(dataDir + "messages.json");
	if (!msgs) return [];

	var newMsgs = [];
	for (var i = 0; i < msgs.messages.length; ++i)
	{
		if (msgs.messages[i].id > lastMessageId)
		{
			newMsgs.push(msgs.messages[i]);
			lastMessageId = msgs.messages[i].id;
		}
	}
	return newMsgs;
}


// -----------------------------------------------------------------------
// Misc. functions
// -----------------------------------------------------------------------

// Parses arguments, where each argument in the given array is in the format
// -arg=val.  If the value is the string "true" or "false", then the value will
// be a boolean.  Otherwise, the value will be a string.
//
// Parameters:
//  argv: An array of strings containing values in the format -arg=val
//
// Return value: An object containing the argument values.  The index will be
//               the argument names, converted to lowercase.  The values will
//               be either the string argument values or boolean values, depending
//               on the formats of the arguments passed in.
function parseArgs(argv)
{
	var retObj = {
		settingsFilename: "settings.json"
	}

	for (var i = 0; i < argv.length; ++i)
	{
		// We're looking for strings that start with "-", except strings that are
		// only "-".
		if (typeof(argv[i]) !== "string" || argv[i].length == 0 || argv[i].charAt(0) != "-" || argv[i] == "-")
			continue;

		// Look for an = and if found, split the string on the =
		var equalsIdx = argv[i].indexOf("=");
		// If a = is found, then split on it and add the argument name & value
		// to the array.  Otherwise (if the = is not found), then treat the
		// argument as a boolean and set it to true (to enable an option).
		if (equalsIdx > -1)
		{
			var argName = argv[i].substring(1, equalsIdx).toLowerCase();
			var argVal = argv[i].substr(equalsIdx+1);

			if (argName == "settings" && argVal.length > 0)
				retObj.settingsFilename = argVal;
		}
		else
		{
			// = not found
		}
	}

	return retObj;
}
