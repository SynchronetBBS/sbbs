/* This is a voting door for Synchronet.  It requires Synchronet 3.17 or higher, since
 * it makes use of the new voting features added to the message bases in Synchronet
 * 3.17.  Also, this requires an ANSI client.
 *
 * Author: Eric Oulashin (AKA Nightfox)
 * BBS: Digital Distortion
 * BBS address: digitaldistortionbbs.com (or digdist.bbsindex.com)
 *
 * Date       Author            Description
 * 2016-12-29 Eric Oulashin     Version 0.01 Beta
 *                              Started
 */
 
 // TODO:
 // - Answer all topics
 // - In the result viewer, allow users to type in a topic number and jump to it
 // - Allow sysops to delete polls
 // - Add a help screen?


load("sbbsdefs.js");

// This script requires Synchronet version 3.17 or higher.
// Exit if the Synchronet version is below the minimum.
if (system.version_num < 31700)
{
	var message = "\1n\1h\1y\1i* Warning:\1n\1h\1w SlyVote requires version "
	             + "\1g3.17\1w or\r\nhigher of Synchronet.  This BBS is using "
	             + "version \1g" + system.version + "\1w.  Please notify the sysop.";
	console.crlf();
	console.print(message);
	console.crlf();
	console.pause();
	exit();
}

// Exit if the user's client doesn't support ANSI
if (!console.term_supports(USER_ANSI))
{
	console.crlf();
	console.print("\1n\1hSlyVote requires an ANSI client.");
	console.crlf();
	console.pause();
	exit();
}

// Exit if the user has the V restriction (not allowed to vote)
if ((user.security.restrictions & UFLAG_V) == UFLAG_V)
{
	console.crlf();
	// Use the line from text.dat that says the user is not allowed to vote
	console.print("\1n" + bbs.text(R_Voting));
	console.pause();
	
}

load("frame.js");
load("scrollbar.js");
load("DDLightbarMenu.js");

// Version information
var SLYVOTE_VERSION = "0.04 Beta";
var SLYVOTE_DATE = "2017-01-12";

// Determine the script's startup directory.
// This code is a trick that was created by Deuce, suggested by Rob Swindell
// as a way to detect which directory the script was executed in.  I've
// shortened the code a little.
var gStartupPath = '.';
try { throw dig.dist(dist); } catch(e) { gStartupPath = e.fileName; }
gStartupPath = backslash(gStartupPath.replace(/[\/\\][^\/\\]*$/,''));

// Characters for display
// Box-drawing/border characters: Single-line
var UPPER_LEFT_SINGLE = "Ú";
var HORIZONTAL_SINGLE = "Ä";
var UPPER_RIGHT_SINGLE = "¿";
var VERTICAL_SINGLE = "³";
var LOWER_LEFT_SINGLE = "À";
var LOWER_RIGHT_SINGLE = "Ù";
var T_SINGLE = "Â";
var LEFT_T_SINGLE = "Ã";
var RIGHT_T_SINGLE = "´";
var BOTTOM_T_SINGLE = "Á";
var CROSS_SINGLE = "Å";
// Box-drawing/border characters: Double-line
var UPPER_LEFT_DOUBLE = "É";
var HORIZONTAL_DOUBLE = "Í";
var UPPER_RIGHT_DOUBLE = "»";
var VERTICAL_DOUBLE = "º";
var LOWER_LEFT_DOUBLE = "È";
var LOWER_RIGHT_DOUBLE = "¼";
var T_DOUBLE = "Ë";
var LEFT_T_DOUBLE = "Ì";
var RIGHT_T_DOUBLE = "¹";
var BOTTOM_T_DOUBLE = "Ê";
var CROSS_DOUBLE = "Î";
// Box-drawing/border characters: Vertical single-line with horizontal double-line
var UPPER_LEFT_VSINGLE_HDOUBLE = "Õ";
var UPPER_RIGHT_VSINGLE_HDOUBLE = "¸";
var LOWER_LEFT_VSINGLE_HDOUBLE = "Ô";
var LOWER_RIGHT_VSINGLE_HDOUBLE = "¾";
// Other special characters
var DOT_CHAR = "ú";
var CHECK_CHAR = "û";
var THIN_RECTANGLE_LEFT = "Ý";
var THIN_RECTANGLE_RIGHT = "Þ";
var BLOCK1 = "°"; // Dimmest block
var BLOCK2 = "±";
var BLOCK3 = "²";
var BLOCK4 = "Û"; // Brightest block

// Strings for the various message attributes (used by makeAllAttrStr(),
// makeMainMsgAttrStr(), makeAuxMsgAttrStr(), and makeNetMsgAttrStr())
var gMainMsgAttrStrs = {
	MSG_DELETE: "Del",
	MSG_PRIVATE: "Priv",
	MSG_READ: "Read",
	MSG_PERMANENT: "Perm",
	MSG_LOCKED: "Lock",
	MSG_ANONYMOUS: "Anon",
	MSG_KILLREAD: "Killread",
	MSG_MODERATED: "Mod",
	MSG_VALIDATED: "Valid",
	MSG_REPLIED: "Repl",
	MSG_NOREPLY: "NoRepl"
}
var gAuxMsgAttrStrs = {
	MSG_FILEREQUEST: "Freq",
	MSG_FILEATTACH: "Attach",
	MSG_TRUNCFILE: "TruncFile",
	MSG_KILLFILE: "KillFile",
	MSG_RECEIPTREQ: "RctReq",
	MSG_CONFIRMREQ: "ConfReq",
	MSG_NODISP: "NoDisp"
}
var gNetMsgAttrStrs = {
	MSG_LOCAL: "FromLocal",
	MSG_INTRANSIT: "Transit",
	MSG_SENT: "Sent",
	MSG_KILLSENT: "KillSent",
	MSG_ARCHIVESENT: "ArcSent",
	MSG_HOLD: "Hold",
	MSG_CRASH: "Crash",
	MSG_IMMEDIATE: "Now",
	MSG_DIRECT: "Direct",
	MSG_GATE: "Gate",
	MSG_ORPHAN: "Orphan",
	MSG_FPU: "FPU",
	MSG_TYPELOCAL: "ForLocal",
	MSG_TYPEECHO: "ForEcho",
	MSG_TYPENET: "ForNetmail"
}

// An amount of milliseconds to wait after displaying an error message, etc.
var ERROR_PAUSE_WAIT_MS = 1500;

var gBottomBorderRow = 23;
var gMessageRow = 3;

// Keyboard key codes for displaying on the screen
var UP_ARROW = ascii(24);
var DOWN_ARROW = ascii(25);
var LEFT_ARROW = ascii(17);
var RIGHT_ARROW = ascii(16);
// PageUp & PageDown keys - Not real key codes, but codes defined
// to be used & recognized in this script
var KEY_PAGE_UP = "\1PgUp";
var KEY_PAGE_DOWN = "\1PgDn";

// An object containing keypresses for the vote results reader
var gReaderKeys = {
	goToFirst: "F",
	goToLast: "L",
	vote: "V",
	quit: "Q"
}


//  cfgReadError: A string which will contain a message if failed to read the configuration file
//  subBoardCodes: An array containing sub-board codes read from the configuration file
var slyVoteCfg = ReadConfigFile();
if (slyVoteCfg.cfgReadError.length > 0)
{
	log(LOG_ERR, "SlyVote: Error reading SlyVote.cfg");
	bbs.log_str("SlyVote: Error reading SlyVote.cfg");
	console.print("\1n");
	console.crlf();
	console.print("\1h\1y* Error reading SlyVote.cfg\1n");
	console.crlf();
	console.pause();
	exit();
}
if (slyVoteCfg.subBoardCodes.length == 0)
{
	console.print("\1n");
	console.crlf();
	console.print("\1cThere are no voting areas configured.\1n");
	console.crlf();
	console.pause();
	exit();
}

// Determine which sub-board to use - If there is more than one, let the user choose.
var gSubBoardCode = "";
if (slyVoteCfg.subBoardCodes.length == 1)
	gSubBoardCode = slyVoteCfg.subBoardCodes[0];
else
{
	console.gotoxy(2, 1);
	console.print("\1n\1cChoose a sub-board:");
	var subBoardsLB = new DDLightbarMenu(2, 2, 45, 10);
	for (var idx = 0; idx < slyVoteCfg.subBoardCodes.length; ++idx)
	{
		var name = msg_area.sub[slyVoteCfg.subBoardCodes[idx]].name;
		var desc = msg_area.sub[slyVoteCfg.subBoardCodes[idx]].description;
		subBoardsLB.Add(desc, slyVoteCfg.subBoardCodes[idx]);
	}
	gSubBoardCode = subBoardsLB.GetVal();
	if (gSubBoardCode == null)
		exit(0);
	console.gotoxy(1, subBoardsLB.pos.y+subBoardsLB.size.height+1);
}

// Program states
var MAIN_MENU = 0;
var VOTING_ON_A_TOPIC = 1;
var VIEWING_VOTE_RESULTS = 2;
var EXIT_SLYVOTE = 999;

var gProgramState = MAIN_MENU;

// Program action loop
var gContinueSlyVote = true;
while (gContinueSlyVote)
{
	switch (gProgramState)
	{
		case MAIN_MENU:
			gProgramState = DoMainMenu();
			break;
		case VOTING_ON_A_TOPIC:
			gProgramState = ChooseVoteTopic();
			break;
		case VIEWING_VOTE_RESULTS:
			gProgramState = ViewVoteResults(gSubBoardCode);
			break;
		case EXIT_SLYVOTE:
			gContinueSlyVote = false;
			break;
	}
}






////////////////////////////////////////////////////////////
// Helper functions
////////////////////////////////////////////////////////////

var gMainMenu = null;
function DoMainMenu()
{
	gProgramState = MAIN_MENU;
	var nextProgramState = MAIN_MENU;
	// Display the SlyVote screen and menu of choices
	console.clear("\1n");
	var mainScrRetObj = DisplaySlyVoteMainVoteScreen(false);
	if (gMainMenu == null)
	{
		gMainMenu = new DDLightbarMenu(mainScrRetObj.optMenuX, mainScrRetObj.optMenuY, 17, 5);
		gMainMenu.Add("Vote On A Topic", "vote", "1");
		gMainMenu.Add("Answer All Topics", "answerAll", "2");
		gMainMenu.Add("Create A Topic", "create", "3");
		gMainMenu.Add("View Results", "viewResults", "4");
		gMainMenu.Add("Quit To BBS", "quit", "5");
		gMainMenu.colors.itemColor = "\1n\1w";
		gMainMenu.colors.selectedItemColor = "\1n\1" + "4\1w\1h";
	}
	// Get the user's choice and take appropriate action
	var userChoice = gMainMenu.GetVal(true);
	if (userChoice == "vote")
		nextProgramState = VOTING_ON_A_TOPIC;
	else if (userChoice == "answerAll")
	{
		// TODO
	}
	else if (userChoice == "create")
	{
		// Set the user's current sub-board to the chosen sub-board before
		// posting a poll
		var curSubCodeBackup = bbs.cursub_code;
		bbs.cursub_code = gSubBoardCode;
		// Let the user post a poll
		console.print("\1n");
		console.gotoxy(1, console.screen_rows);
		bbs.exec("?postpoll.js");
		// Restore the user's sub-board
		bbs.cursub_code = curSubCodeBackup;
	}
	else if (userChoice == "viewResults")
		nextProgramState = VIEWING_VOTE_RESULTS;
	else if ((userChoice == "quit") || (userChoice == null))
		nextProgramState = EXIT_SLYVOTE;

	return nextProgramState;
}

// Lets the user choose a voting topic (poll) to vote on
function ChooseVoteTopic()
{
	gProgramState = VOTING_ON_A_TOPIC;
	var nextProgramState = VOTING_ON_A_TOPIC;
	// Clear the screen between the top & bottom borders
	var formatStr = "%" + console.screen_columns + "s";
	console.print("\1n");
	for (var posY = gMessageRow; posY < gBottomBorderRow; ++posY)
	{
		console.gotoxy(1, posY);
		printf(formatStr, "");
	}

	// Get the list of polls for the selected sub-board
	var voteTopicInfo = GetVoteTopicHdrs(gSubBoardCode, true);
	if (voteTopicInfo.errorMsg.length > 0)
	{
		console.gotoxy(1, gMessageRow);
		console.print("\1n\1y\1h" + voteTopicInfo.errorMsg + "\1n");
		console.crlf();
		console.pause();
		return MAIN_MENU;
	}
	else if (voteTopicInfo.msgHdrs.length == 0)
	{
		console.gotoxy(1, gMessageRow);
		console.print("\1n\1cThere are no polls to vote on in this section\1n");
		console.crlf();
		console.pause();
		return MAIN_MENU;
	}

	// Display the list of voting topics
	console.print("\1n");
	var pleaseSelectTextRow = 6;
	// Draw the columns to frame the voting topics
	var columnX1 = 17;
	var columnX2 = 63;
	var textLen = columnX2 - columnX1 - 2;
	var listTopRow = pleaseSelectTextRow + 1;
	for (var posY = listTopRow; posY < gBottomBorderRow; ++posY)
	{
		console.gotoxy(columnX1, posY);
		console.print("\1" + "7\1h\1wÝ\1n\1k\1" + "7\1n\1" + "7\1k\1hÞ\1n");
		console.gotoxy(columnX2, posY);
		console.print("\1" + "7\1h\1wÝ\1n\1k\1" + "7\1n\1" + "7\1k\1hÞ\1n");
	}
	// Create the menu containing voting topics and get the user's choice
	var startCol = columnX1+2;
	var menuHeight = gBottomBorderRow-listTopRow-1;
	var topicsMenu = new DDLightbarMenu(startCol, listTopRow, textLen, menuHeight);
	for (var i = 0; i < voteTopicInfo.msgHdrs.length; ++i)
		topicsMenu.Add(voteTopicInfo.msgHdrs[i].subject, voteTopicInfo.msgHdrs[i].number);
	var drawTopicsMenu = true;
	while (nextProgramState == VOTING_ON_A_TOPIC)
	{
		var pleaseSectTopicText = "\1n\1c\1hP\1n\1clease select a topic to vote on (\1hESC\1n\1g=\1cReturn)\1n";
		console.gotoxy(18, pleaseSelectTextRow);
		console.print(pleaseSectTopicText);
		console.print("\1n");
		var userChoice = topicsMenu.GetVal(drawTopicsMenu);
		if (userChoice != null)
		{
			//topicsMenu.Erase();
			console.gotoxy(18, pleaseSelectTextRow);
			printf("%" + strip_ctrl(pleaseSectTopicText).length + "s", "");
			var voteRetObj = DisplayTopicOptionsAndVote(gSubBoardCode, userChoice, startCol, listTopRow, textLen, menuHeight);
			drawTopicsMenu = true;
			if (voteRetObj.errorMsg.length > 0)
			{
				console.gotoxy(1, gMessageRow);
				if (voteRetObj.mnemonicsRequiredForErrorMsg)
				{
					console.gotoxy(1, gMessageRow);
					console.mnemonics(voteRetObj.errorMsg);
					mswait(ERROR_PAUSE_WAIT_MS);
					console.gotoxy(1, gMessageRow);
					printf("\1n%" + console.screen_columns + "s", "");
				}
				else
				{
					console.print("\1n\1y\1h" + voteRetObj.errorMsg);
					mswait(ERROR_PAUSE_WAIT_MS);
					console.gotoxy(1, gMessageRow);
					printf("\1n%" + voteRetObj.errorMsg.length + "s", "");
				}
			}
		}
		else // The user chose to exit the topics menu
			nextProgramState = MAIN_MENU;
	}

	return nextProgramState;
}

// Displays options for a topic and lets the user vote on it
//
// Parameters:
//  pSubBoardCode: The internal code of the sub-board
//  pMsgNum: The number of the message to vote on
//  pStartCol: The starting column on the screen for the option list
//  pStartRow: The starting row on the screen for the option list
//  pMenuWidth: The width to use for the topic option menu
//  pMenuHeight: The height to use for the topic option menu
//
// Return value: An object containing the following properties:
//               errorMsg: A string containing a message on error, or an empty string on success
//               mnemonicsRequiredForErrorMsg: Whether or not mnemonics is required to display the error message
function DisplayTopicOptionsAndVote(pSubBoardCode, pMsgNum, pStartCol, pStartRow, pMenuWidth, pMenuHeight)
{
	var retObj = {
		errorMsg: "",
		mnemonicsRequiredForErrorMsg: false,
		nextProgramState: VOTING_ON_A_TOPIC
	};

	// Open the chosen sub-board
	var msgbase = new MsgBase(pSubBoardCode);
	if (msgbase.open())
	{
		if (!HasUserVotedOnMsg(pMsgNum, pSubBoardCode, msgbase, user))
		{
			var msgHdr = msgbase.get_msg_header(false, pMsgNum, true);
			if (msgHdr != null)
			{
				var pollTextAndOpts = GetPollTextAndOpts(msgHdr);
				// Print the poll question text
				console.gotoxy(1, pStartRow-4);
				var pollSubject = msgHdr.subject.substr(0, console.screen_columns);
				console.print("\1n\1c" + pollSubject);
				// Output up to the first 3 poll comment lines
				//console.print("\1n");
				var i = 0;
				var commentStartRow = pStartRow - 3;
				for (var row = commentStartRow; (row < commentStartRow+3) && (i < pollTextAndOpts.commentLines.length); ++row)
				{
					console.gotoxy(1, row);
					console.print(pollTextAndOpts.commentLines[i++].substr(0, console.screen_columns));
				}
				// Create the poll options menu, and show it and let the user choose an option
				var optionsMenu = new DDLightbarMenu(pStartCol, pStartRow, pMenuWidth, pMenuHeight);
				for (i = 0; i < pollTextAndOpts.options.length; ++i)
					optionsMenu.Add(pollTextAndOpts.options[i], i);
				optionsMenu.colors.itemColor = "\1c";
				optionsMenu.colors.selectedItemColor = "\1b\1" + "7";
				// Get the user's choice of voting option and submit it for voting
				var userChoice = optionsMenu.GetVal(true);
				if (userChoice != null)
				{
					var voteRetObj = VoteOnTopic(pSubBoardCode, msgbase, msgHdr, user, userChoice, true);
					// If there was an error, then show it.  Otherwise, show a success message.
					var firstLineEraseLength = pollSubject.length;
					console.gotoxy(1, pStartRow-4);
					printf("\1n%" + pollSubject.length + "s", "");
					console.gotoxy(1, pStartRow-4);
					if (voteRetObj.errorMsg.length > 0)
					{
						var voteErrMsg = voteRetObj.errorMsg.substr(0, console.screen_columns - 2);
						firstLineEraseLength = voteErrMsg.length;
						console.print("\1y\1h* " + voteErrMsg);
						mswait(ERROR_PAUSE_WAIT_MS);
					}
					else
					{
						console.print("\1cYour vote was successfully saved.");
						mswait(ERROR_PAUSE_WAIT_MS);
					}
				}

				// Before returning, erase the poll question text and comment lines from the screen
				console.print("\1n");
				console.gotoxy(1, pStartRow-4);
				printf("%" + firstLineEraseLength + "s", "");
				i = 0;
				for (var row = commentStartRow; (row < commentStartRow+3) && (i < pollTextAndOpts.commentLines.length); ++row)
				{
					console.gotoxy(1, row);
					var pollCommentLine = pollTextAndOpts.commentLines[i++].substr(0, console.screen_columns);
					printf("%" + pollCommentLine.length + "s", "");
				}
			}
			else
				retObj.errorMsg = "Unable to retrieve the topic options";
		}
		else
		{
			// The user has already voted
			retObj.errorMsg = bbs.text(typeof(VotedAlready) != "undefined" ? VotedAlready : 780);
			retObj.errorMsg = retObj.errorMsg.replace("\r\n", "").replace("\n", "").replace("\N", "").replace("\r", "").replace("\R", "").replace("\R\n", "").replace("\r\N", "").replace("\R\N", "");
			retObj.mnemonicsRequiredForErrorMsg = true;
		}

		msgbase.close();
	}
	else
		retObj.errorMsg = "Failed to open the messagebase";

	return retObj;
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
	if(typeof(pLeading) != "undefined")
		leading = pLeading;
	if(typeof(pMultiple) != "undefined")
		multiple = pMultiple;
	if(typeof(pTrailing) != "undefined")
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

// Reads the configuration file (SlyVote.cfg).  Returns an object with the following
// properties:
//  cfgReadError: A string which will contain a message if failed to read the configuration file
//  subBoardCodes: An array containing sub-board codes read from the configuration file
function ReadConfigFile()
{
	var retObj = {
		cfgReadError: "",
		subBoardCodes: []
	};

	// Open the main configuration file.  First look for it in the sbbs/mods
	// directory, then sbbs/ctrl, then in the same directory as this script.
	var filename = "SlyVote.cfg";
	var cfgFilename = system.mods_dir + filename;
	if (!file_exists(cfgFilename))
		cfgFilename = system.ctrl_dir + filename;
	if (!file_exists(cfgFilename))
		cfgFilename = gStartupPath + filename;
	var cfgFile = new File(cfgFilename);
	if (cfgFile.open("r"))
	{
		var fileLine = null;     // A line read from the file
		var equalsPos = 0;       // Position of a = in the line
		var commentPos = 0;      // Position of the start of a comment
		var setting = null;      // A setting name (string)
		var settingUpper = null; // Upper-case setting name
		var value = null;        // To store a value for a setting (string)
		while (!cfgFile.eof)
		{
			// Read the next line from the config file.
			fileLine = cfgFile.readln(2048);

			// fileLine should be a string, but I've seen some cases
			// where it isn't, so check its type.
			if (typeof(fileLine) != "string")
				continue;

			// If the line starts with with a semicolon (the comment
			// character) or is blank, then skip it.
			if ((fileLine.substr(0, 1) == ";") || (fileLine.length == 0))
				continue;

			// If the line has a semicolon anywhere in it, then remove
			// everything from the semicolon onward.
			commentPos = fileLine.indexOf(";");
			if (commentPos > -1)
				fileLine = fileLine.substr(0, commentPos);

			// Look for an equals sign, and if found, separate the line
			// into the setting name (before the =) and the value (after the
			// equals sign).
			equalsPos = fileLine.indexOf("=");
			if (equalsPos > 0)
			{
				// Read the setting & value, and trim leading & trailing spaces.
				setting = trimSpaces(fileLine.substr(0, equalsPos), true, false, true);
				settingUpper = setting.toUpperCase();
				value = trimSpaces(fileLine.substr(equalsPos+1), true, false, true);

				// Set the appropriate value in the settings object.
				if (settingUpper == "SUBBOARDCODES")
				{
					// Split the value on commas and add all sub-board codes to
					// retObj.subBoardCodes, as long as they're valid sub-board codes.
					var valueLower = value.toLowerCase();
					var subCodeArray = valueLower.split(",");
					for (var idx = 0; idx < subCodeArray.length; ++idx)
					{
						// If the sub-board code exists and voting is allowed in the sub-board, then add it.
						if (msg_area.sub.hasOwnProperty(subCodeArray[idx]))
						{
							if ((msg_area.sub[subCodeArray[idx]].settings & SUB_NOVOTING) == 0)
								retObj.subBoardCodes.push(subCodeArray[idx]);
						}
					}
				}
			}
		}

		cfgFile.close();
	}
	else // Unable to read the configuration file
		retObj.cfgReadError = "Unable to open the configuration file: SlyVote.cfg";

	return retObj;
}

// Checks to see whether a user has voted on a message.
//
// Parameters:
//  pMsgNum: The message number
//  pSubBoardCode: The internal code of the sub-board
//  pMsgbase: The MessageBase object for the sub-board
//  pUser: Optional - A user account to check.  If omitted, the current logged-in
//         user will be used.
function HasUserVotedOnMsg(pMsgNum, pSubBoardCode, pMsgbase, pUser)
{
	// Don't do this for personal email
	if (pSubBoardCode == "mail")
		return false;

	// Thanks to echicken for explaining how to check this.  To check a user's
	// vote, use MsgBase.how_user_voted().
	/*
	The return value will be:
	0 - user hasn't voted
	1 - upvoted
	2 - downvoted
	Or, if the message was a poll, it's a bitfield:
	if (votes&(1<<2)) {
	 // User selected answer 2
	}
	*/
	var userHasVotedOnMsg = false;
	if ((pMsgbase !== null) && pMsgbase.is_open)
	{
		if (typeof(pMsgbase.how_user_voted) === "function")
		{
			var votes = 0;
			if (typeof(pUser) == "object")
				votes = pMsgbase.how_user_voted(pMsgNum, (pMsgbase.cfg.settings & SUB_NAME) == SUB_NAME ? pUser.name : pUser.alias);
			else
				votes = pMsgbase.how_user_voted(pMsgNum, (pMsgbase.cfg.settings & SUB_NAME) == SUB_NAME ? user.name : user.alias);
			userHasVotedOnMsg = (votes > 0);
			/*
			if (votes > 0)
			{
				var userVotedMaxVotes = false;
				if ((votes == 0) || (votes == 1))
					userVotedMaxVotes = (votes == 3); // (1 << 0) | (1 << 1);
				else
				{
					var msgHdr = pMsgbase.get_msg_header(false, pMsgNum);
					if (msgHdr != null)
					{
						userVotedMaxVotes = true;
						for (var voteIdx = 0; voteIdx <= numVotes; ++voteIdx)
						{
							if (votes && (1 << voteIdx) == 0)
							{
								userVotedMaxVotes = false;
								break;
							}
						}
					}
				}
				userHasVotedOnMsg = userVotedMaxVotes;
			}
			*/
		}
	}
	return userHasVotedOnMsg;
}

// Gets the body (text) of a message.  If it's
// a poll, this method will format the message body with poll results.  Otherwise,
// this method will simply get the message body.
//
// Parameters:
//  pMsgbase: The MessageBase object for the sub-board
//  pMsgHeader: The message header
//  pSubBoardCode: The internal code of the sub-board
//  pUser: Optional - A user account to check.  If omitted, the current logged-in
//         user will be used.
//
// Return value: The poll results, colorized.  If the message is not a
//               poll message, then an empty string will be returned.
function GetMsgBody(pMsgbase, pMsgHdr, pSubBoardCode, pUser)
{
	var msgBody = "";
	if ((typeof(MSG_TYPE_POLL) != "undefined") && (pMsgHdr.type & MSG_TYPE_POLL) == MSG_TYPE_POLL)
	{
		// A poll is intended to be parsed (and displayed) using on the header data. The
		// (optional) comments are stored in the hdr.field_list[] with type values of
		// SMB_COMMENT (now defined in sbbsdefs.js) and the available answers are in the
		// field_list[] with type values of SMB_POLL_ANSWER.

		// The 'comments' and 'answers' are also a part of the message header, so you can
		// grab them separately, then format and display them however you want.  You can
		// find them in the header.field_list array; each element in that array should be
		// an object with a 'type' and a 'data' property.  Relevant types here are
		// SMB_COMMENT and SMB_POLL_ANSWER.  (This is what I'm doing on the web, and I
		// just ignore the message body for poll messages.)

		if (pMsgHdr.hasOwnProperty("field_list"))
		{
			//var voteOptDescLen = 27;
			// Figure out the longest length of the poll choices, with
			// a maximum of 22 characters less than the terminal width.
			// Use a minimum of 27 characters.
			// That length will be used for the formatting strings for
			// the poll results.
			var voteOptDescLen = 0;
			for (var fieldI = 0; fieldI < pMsgHdr.field_list.length; ++fieldI)
			{
				if (pMsgHdr.field_list[fieldI].type == SMB_POLL_ANSWER)
				{
					if (pMsgHdr.field_list[fieldI].data.length > voteOptDescLen)
						voteOptDescLen = pMsgHdr.field_list[fieldI].data.length;
				}
			}
			if (voteOptDescLen > console.screen_columns - 22)
				voteOptDescLen = console.screen_columns - 22;
			else if (voteOptDescLen < 27)
				voteOptDescLen = 27;

			// Format strings for outputting the voting option lines
			var unvotedOptionFormatStr = "\1n\1" + "0\1c\1h%2d\1n\1" + "0\1c: \1w\1h%-" + voteOptDescLen + "s [%-4d %6.2f%]\1n\1" + "0";
			var votedOptionFormatStr = "\1n\1" + "0\1c\1h%2d\1n\1" + 0 + "\1c: \1" + "5\1w\1h%-" + voteOptDescLen + "s [%-4d %6.2f%]\1n\1" + "0";
			// Add up the total number of votes so that we can
			// calculate vote percentages.
			var totalNumVotes = 0;
			if (pMsgHdr.hasOwnProperty("tally"))
			{
				for (var tallyI = 0; tallyI < pMsgHdr.tally.length; ++tallyI)
					totalNumVotes += pMsgHdr.tally[tallyI];
			}
			// Go through field_list and append the voting options and stats to
			// msgBody
			var pollComment = "";
			var optionNum = 1;
			var numVotes = 0;
			var votePercentage = 0;
			var tallyIdx = 0;
			for (var fieldI = 0; fieldI < pMsgHdr.field_list.length; ++fieldI)
			{
				if (pMsgHdr.field_list[fieldI].type == SMB_COMMENT)
					pollComment += pMsgHdr.field_list[fieldI].data + "\r\n";
				else if (pMsgHdr.field_list[fieldI].type == SMB_POLL_ANSWER)
				{
					// Figure out the number of votes on this option and the
					// vote percentage
					if (pMsgHdr.hasOwnProperty("tally"))
					{
						if (tallyIdx < pMsgHdr.tally.length)
						{
							numVotes = pMsgHdr.tally[tallyIdx];
							votePercentage = (numVotes / totalNumVotes) * 100;
						}
					}
					// Append to the message text
					msgBody += format(numVotes == 0 ? unvotedOptionFormatStr : votedOptionFormatStr,
					                  optionNum++, pMsgHdr.field_list[fieldI].data.substr(0, voteOptDescLen),
					                  numVotes, votePercentage);
					if (numVotes > 0)
						msgBody += " " + CHECK_CHAR;
					msgBody += "\r\n";
					++tallyIdx;
				}
			}
			if (pollComment.length > 0)
				msgBody = pollComment + "\r\n" + msgBody;

			// If voting is allowed in this sub-board and the current logged-in
			// user has not voted on this message, then append some text saying
			// how to vote.
			var votingAllowed = ((pSubBoardCode != "mail") && (((msg_area.sub[pSubBoardCode].settings & SUB_NOVOTING) == 0)));
			if (votingAllowed && !HasUserVotedOnMsg(pMsgHdr.number, pSubBoardCode, pMsgbase, pUser))
				msgBody += "\1n\1" + "0\r\n\1gTo vote in this poll, press \1w\1h" + gReaderKeys.vote + "\1n\1" + "0\1g now.";

			// If the current logged-in user created this poll, then show the
			// users who have voted on it so far.
			var msgFromUpper = pMsgHdr.from.toUpperCase();
			if ((msgFromUpper == user.name.toUpperCase()) || (msgFromUpper == user.handle.toUpperCase()))
			{
				// Check all the messages in the messagebase after the current one
				// to find poll response messages
				if (typeof(pMsgbase.get_all_msg_headers) === "function")
				{
					// Get the line from text.dat for writing who voted & when.  It
					// is a format string and should look something like this:
					//"\r\n\1n\1" + "0\1hOn %s, in \1c%s \1n\1" + "0\1c%s\r\n\1h\1m%s voted in your poll: \1n\1" + "0\1h%s\r\n" 787 PollVoteNotice
					var userVotedInYourPollText = bbs.text(typeof(PollVoteNotice) != "undefined" ? PollVoteNotice : 787);

					// Pass true to get_all_msg_headers() to tell it to return vote messages
					// (the parameter was introduced in Synchronet 3.17+)
					var tmpHdrs = pMsgbase.get_all_msg_headers(true);
					for (var tmpProp in tmpHdrs)
					{
						if (tmpHdrs[tmpProp] == null)
							continue;
						// If this header's thread_back or reply_id matches the poll message
						// number, then append the 'user voted' string to the message body.
						if ((tmpHdrs[tmpProp].thread_back == pMsgHdr.number) || (tmpHdrs[tmpProp].reply_id == pMsgHdr.number))
						{
							var msgWrittenLocalTime = MsgWrittenTimeToLocalBBSTime(tmpHdrs[tmpProp]);
							var voteDate = strftime("%a %b %d %Y %H:%M:%S", msgWrittenLocalTime);
							msgBody += format(userVotedInYourPollText, voteDate, pMsgbase.cfg.grp_name, pMsgbase.cfg.name,
							                  tmpHdrs[tmpProp].from, pMsgHdr.subject);
						}
					}
				}
			}
		}
	}
	else
		msgBody = pMsgbase.get_msg_body(false, pMsgHdr.number);
	return msgBody;
}

// Gets a poll message's text and options
//
// Parameters:
//  pMsgHdr: The header object of the message to get the text & options from
//
// Return value: An object with the following properties:
//               commentLines: An array containing the comment lines
//               options: An array containing the poll options (text)
function GetPollTextAndOpts(pMsgHdr)
{
	var retObj = {
		commentLines: [],
		options: []
	};
	
	if ((typeof(MSG_TYPE_POLL) != "undefined") && (pMsgHdr.type & MSG_TYPE_POLL) == MSG_TYPE_POLL)
	{
		// A poll is intended to be parsed (and displayed) using on the header data. The
		// (optional) comments are stored in the hdr.field_list[] with type values of
		// SMB_COMMENT (now defined in sbbsdefs.js) and the available answers are in the
		// field_list[] with type values of SMB_POLL_ANSWER.

		// The 'comments' and 'answers' are also a part of the message header, so you can
		// grab them separately, then format and display them however you want.  You can
		// find them in the header.field_list array; each element in that array should be
		// an object with a 'type' and a 'data' property.  Relevant types here are
		// SMB_COMMENT and SMB_POLL_ANSWER.  (This is what I'm doing on the web, and I
		// just ignore the message body for poll messages.)

		if (pMsgHdr.hasOwnProperty("field_list"))
		{
			// Go through field_list and append the voting options and stats to
			// msgBody
			var pollComment = "";
			var optionNum = 1;
			var numVotes = 0;
			var votePercentage = 0;
			var tallyIdx = 0;
			for (var fieldI = 0; fieldI < pMsgHdr.field_list.length; ++fieldI)
			{
				if (pMsgHdr.field_list[fieldI].type == SMB_COMMENT)
					retObj.commentLines.push(pMsgHdr.field_list[fieldI].data);
				else if (pMsgHdr.field_list[fieldI].type == SMB_POLL_ANSWER)
					retObj.options.push(pMsgHdr.field_list[fieldI].data);
			}
		}
	}

	return retObj;
}

// Lets the user vote on a message
//
// Parameters:
//  pSubBoardCode: The internal code of the sub-board being used for voting
//  pMsgbase: The MessageBase object for the sub-board being used for voting
//  pMsgHdr: The header of the mesasge being voted on
//  pUser: The user object representing the user voting on the poll
//  pUserVoteNumber: Optional - A number inputted by the user representing their vote.
//                   If this is not passed in or is null, etc., then this function will
//                   prompt the user for their vote.
//  pRemoveNLsFromVoteText: Optional boolean - Whether or not to remove newlines
//                          (and carriage returns) from the voting text from
//                          text.dat.  Defaults to false.
//
// Return value: An object with the following properties:
//               BBSHasVoteFunction: Boolean - Whether or not the system has
//                                   the vote_msg function
//               savedVote: Boolean - Whether or not the vote was saved
//               userQuit: Boolean - Whether or not the user quit and didn't vote
//               errorMsg: String - An error message, if something went wrong
//               mnemonicsRequiredForErrorMsg: Boolean - Whether or not mnemonics is required to print the error message
//               updatedHdr: The updated message header containing vote information.
//                           If something went wrong, this will be null.
function VoteOnTopic(pSubBoardCode, pMsgbase, pMsgHdr, pUser, pUserVoteNumber, pRemoveNLsFromVoteText)
{
	var retObj = new Object();
	retObj.BBSHasVoteFunction = (typeof(pMsgbase.vote_msg) === "function");
	retObj.savedVote = false;
	retObj.userQuit = false;
	retObj.errorMsg = "";
	retObj.mnemonicsRequiredForErrorMsg = false;
	retObj.updatedHdr = null;

	// Don't allow voting for personal email
	if (pSubBoardCode == "mail")
	{
		retObj.errorMsg = "Can not vote on personal email";
		return retObj;
	}

	// If the message vote function is not defined in the running verison of Synchronet,
	// then just return.
	if (!retObj.BBSHasVoteFunction)
		return retObj;

	var removeNLsFromVoteText = (typeof(pRemoveNLsFromVoteText) === "boolean" ? pRemoveNLsFromVoteText : false)

	// See if voting is allowed in the current sub-board
	if ((msg_area.sub[pSubBoardCode].settings & SUB_NOVOTING) == SUB_NOVOTING)
	{
		retObj.errorMsg = bbs.text(VotingNotAllowed);
		if (removeNLsFromVoteText)
			retObj.errorMsg = retObj.errorMsg.replace("\r\n", "").replace("\n", "").replace("\N", "").replace("\r", "").replace("\R", "").replace("\R\n", "").replace("\r\N", "").replace("\R\N", "");
		retObj.mnemonicsRequiredForErrorMsg = true;
		return retObj;
	}

	// If the message is a poll question and has the maximum number of votes
	// already or is closed for voting, then don't let the user vote on it.
	if ((pMsgHdr.attr & MSG_POLL) == MSG_POLL)
	{
		var userVotedMaxVotes = false;
		var numVotes = (pMsgHdr.hasOwnProperty("votes") ? pMsgHdr.votes : 0);
		if (typeof(pMsgbase.how_user_voted) === "function")
		{
			var votes = pMsgbase.how_user_voted(pMsgHdr.number, (pMsgbase.cfg.settings & SUB_NAME) == SUB_NAME ? user.name : user.alias);
			if (votes >= 0)
			{
				if ((votes == 0) || (votes == 1))
					userVotedMaxVotes = (votes == 3); // (1 << 0) | (1 << 1);
				else
				{
					userVotedMaxVotes = true;
					for (var voteIdx = 0; voteIdx <= numVotes; ++voteIdx)
					{
						if (votes && (1 << voteIdx) == 0)
						{
							userVotedMaxVotes = false;
							break;
						}
					}
				}
			}
		}
		var pollIsClosed = ((pMsgHdr.auxattr & POLL_CLOSED) == POLL_CLOSED);
		if (pollIsClosed)
		{
			retObj.errorMsg = "This poll is closed";
			return retObj;
		}
		else if (userVotedMaxVotes)
		{
			retObj.errorMsg = bbs.text(VotedAlready);
			if (removeNLsFromVoteText)
				retObj.errorMsg = retObj.errorMsg.replace("\r\n", "").replace("\n", "").replace("\N", "").replace("\r", "").replace("\R", "").replace("\R\n", "").replace("\r\N", "").replace("\R\N", "");
			retObj.mnemonicsRequiredForErrorMsg = true;
			return retObj;
		}
	}

	// If the user has voted on this message already, then set an error message and return.
	if (HasUserVotedOnMsg(pMsgHdr.number, pSubBoardCode, pMsgbase, pUser))
	{
		retObj.errorMsg = bbs.text(VotedAlready);
		if (removeNLsFromVoteText)
			retObj.errorMsg = retObj.errorMsg.replace("\r\n", "").replace("\n", "").replace("\N", "").replace("\r", "").replace("\R", "").replace("\R\n", "").replace("\r\N", "").replace("\R\N", "");
		retObj.mnemonicsRequiredForErrorMsg = true;
		return retObj;
	}

	// New MsgBase method: vote_msg(). it takes a message header object
	// (like save_msg), except you only need a few properties, in order of
	// importarnce:
	// attr: you need to have this set to MSG_UPVOTE, MSG_DOWNVOTE, or MSG_VOTE
	// thread_back or reply_id: either of these must be set to indicate msg to vote on
	// from: name of voter
	// from_net_type and from_net_addr: if applicable

	// Do some initial setup of the header for the vote message to be
	// saved to the messagebase
	var voteMsgHdr = new Object();
	voteMsgHdr.thread_back = pMsgHdr.number;
	voteMsgHdr.reply_id = pMsgHdr.number;
	voteMsgHdr.from = (pMsgbase.cfg.settings & SUB_NAME) == SUB_NAME ? user.name : user.alias;
	if (pMsgHdr.from.hasOwnProperty("from_net_type"))
	{
		voteMsgHdr.from_net_type = pMsgHdr.from_net_type;
		if (pMsgHdr.from_net_type != NET_NONE)
			voteMsgHdr.from_net_addr = user.email;
	}

	// Input vote options from the user differently depending on whether
	// the message is a poll or not
	if ((pMsgHdr.attr & MSG_POLL) == MSG_POLL)
	{
		if (pMsgHdr.hasOwnProperty("field_list"))
		{
			var userInputNum = 0;
			if (typeof(pUserVoteNumber) != "number")
			{
				console.clear("\1n");
				var selectHdr = bbs.text(typeof(SelectItemHdr) != "undefined" ? SelectItemHdr : 501);
				printf("\1n" + selectHdr + "\1n", pMsgHdr.subject);
				var optionFormatStr = "\1n\1c\1h%2d\1n\1c: \1h%s\1n";
				var optionNum = 1;
				for (var fieldI = 0; fieldI < pMsgHdr.field_list.length; ++fieldI)
				{
					if (pMsgHdr.field_list[fieldI].type == SMB_POLL_ANSWER)
					{
						printf(optionFormatStr, optionNum++, pMsgHdr.field_list[fieldI].data);
						console.crlf();
					}
				}
				console.crlf();
				// Get the selection prompt text from text.dat and replace the %u or %d with
				// the number 1 (default option)
				var selectPromptText = bbs.text(typeof(SelectItemWhich) != "undefined" ? SelectItemWhich : 503);
				selectPromptText = selectPromptText.replace(/%[uU]/, 1).replace(/%[dD]/, 1);
				console.mnemonics(selectPromptText);
				// Get & process the selection from the user
				var maxNum = optionNum - 1;
				// TODO: Update to support multiple answers from the user
				userInputNum = console.getnum(maxNum);
				console.print("\1n");
			}
			else
				userInputNum = pUserVoteNumber;
			//if (userInputNum == 0) // The user just pressed enter to choose the default
			//	userInputNum = 1;
			if (userInputNum == -1) // The user chose Q to quit
				retObj.userQuit = true;
			else
			{
				// The user's answer is 0-based, so if userInputNum is positive,
				// subtract 1 from it (if it's already 0, that means the user
				// chose to keep the default first answer).
				if (userInputNum > 0)
					--userInputNum;
				var votes = (1 << userInputNum);
				voteMsgHdr.attr = MSG_VOTE;
				voteMsgHdr.votes = votes;
			}
		}
	}

	// If the user hasn't quit and there is no error message, then save the vote
	// message header
	if (!retObj.userQuit && (retObj.errorMsg.length == 0))
	{
		retObj.savedVote = pMsgbase.vote_msg(voteMsgHdr);
		if (!retObj.savedVote)
			retObj.errorMsg = "Failed to save your vote";
	}

	return retObj;
}

// Displays the SlyVote screen.
//
// Parameters:
//  pClearScr - Optional - Whether or not to clear the screen first.  Defaults to true.
//
// Return value: An object with the following parameters:
//               optMenuX: The column of the upper-left corner to use for the option menu
//               optMenuY: The row of the upper-left corner to use for the option menu
function DisplaySlyVoteMainVoteScreen(pClearScr)
{
	var clearScr = (typeof(pClearScr) == "boolean" ? pClearScr : true);
	if (clearScr)
		console.clear("\1n");
	else
		console.print("\1n");
	//bbs.menu("../" + gStartupPath + "slyvote");
	DisplayTopScreenBorder();
	DisplaySlyVoteText();
	DisplayVerAndRegBorders();
	console.gotoxy(1, gBottomBorderRow);
	DisplayBottomScreenBorder();
	console.print("\1n");
	// Write the SlyVote version centered
	var fieldWidth = 28;
	console.gotoxy(41, 14);
	console.print(CenterText("\1n\1hSlyVote v\1c" + SLYVOTE_VERSION.replace(".", "\1b.\1c") + "\1n", fieldWidth));
	// Write the "Registered to" text centered
	console.gotoxy(41, 17);
	console.print(CenterText("\1n\1h" + system.operator + "\1n", fieldWidth));
	console.gotoxy(41, 18);
	console.print(CenterText("\1n\1h" + system.name + "\1n", fieldWidth));
	// Write the option numbers
	var curPos = { x: 7, y: 12 };
	var retObj = { optMenuX: curPos.x+4, optMenuY: curPos.y }; // For the option menu to be used later
	for (var optNum = 1; optNum <= 5; ++optNum)
	{
		console.gotoxy(curPos.x, curPos.y++);
		console.print("\1" + "7\1h\1wÝ\1n\1k\1" + "7" + optNum + "\1n\1" + "7\1k\1hÞ\1n");
	}

	return retObj;
}

function DisplayTopScreenBorder()
{
	console.print("0");
	console.crlf();
	console.print("nh nbÜhþcßbþnbÜhþcßbþnbÜhþcßbþnbÜhþcßbþnbÜhþcßbþnbÜhþcßbþnbÜhþcßbþnbÜhþcßbþnbÜhþcßbþnbÜhþcßbþnbÜhþcßbþnbÜhþcßbþnbÜhþcßbþnbÜhþcßbþnbÜhþcßbþnbÜhþcßbþnbÜhþcßbþnbÜhþcßbþnbÜhþcßbþnbÜ");
	console.crlf();
}

function DisplaySlyVoteText()
{
	console.print("Ÿú");
	console.crlf();
	console.print("ŠhcÜÜÜÜÜÜÜ ncÜÜÜ  hbú  ncÜÜÜ ÜÜÜ hÜÜÜ ÜÜÜ ncÜÜÜÜÜÜÜ ÜÜÜÜÜÜÜ ÜÜÜÜÜÜÜ hú");
	console.crlf();
	console.print("ŠÛ ÜÜÜÜÛ ncÛ Û„ÛÜßÛßÜÛ hÛ Û Û Û ncÛ ÜÜÜ Û ÛÜÜ ÜÜÛ Û ÜÜÜÜÛ");
	console.crlf();
	console.print("  hú‡ÛÜÜÜÜ Û ncÛ ÛÜÜÜÜ  ßÛ Ûß  hÛÜßÛßÜÛ ncÛ ÛÜÛ Û   Û Û   Û ÜÜÜÛÜ");
	console.crlf();
	console.print("†hbú   cÛÜÜÜÜÜÛ ncÛÜÜÜÜÜÛ hú ncÛÜÛ    hßÛÜÛß nbúcÛÜÜÜÜÜÛ   ÛÜÛ hú ncÛÜÜÜÜÜÛ   hbú");
	console.crlf();
	console.print("©nbú");
	console.crlf();
}

function DisplayVerAndRegBorders()
{
	var curPos = { x: 40, y: 13 };
	console.gotoxy(curPos.x, curPos.y++);
	console.print("hÚÄkÄÄbÄnbÄÄhkÄÄÄbÄnbÄhÄÄnbÄhÄkÄbÄÄnbÄhkÄnbÄhÄkÄÄbÄnbÄhkÄnbÄh¿");
	console.gotoxy(curPos.x, curPos.y++);
	console.print("nb³†hw         c b c  ‡k³");
	console.gotoxy(curPos.x, curPos.y++);
	console.print("bÀÄÄkÄÄnbÄÄÄhkÄbÄkÄbÄnbÄhÄÄkÄnbÄhkÄnbÄhÄkÄbÄkÄÄbÄkÄbÄnbÄhÄnbÙ");
	console.gotoxy(curPos.x, curPos.y++);
	console.print("ÚhkÄnbÄÄÄhkÄb´ nbRhenbgisterehkd To: bÃkÄnbÄhÄkÄÄ¿");
	console.gotoxy(curPos.x, curPos.y++);
	console.print("nb³›h³");
	console.gotoxy(curPos.x, curPos.y++);
	console.print("k³›nb³");
	console.gotoxy(curPos.x, curPos.y++);
	console.print("hÀkÄnbÄhkÄnbÄhÄÄÄkÄÄbÄnbÄhÄÄÄkÄnbÄhÄnbÄhÄÄnbÄhÄnbÄhÄnbÄhÄnbÄhkÄbÙ");
}

function DisplayBottomScreenBorder()
{
	console.print(" nbßhþcÜbþnbßhþcÜbþnbßhþcÜbþnbßhþcÜbþnbßhþcÜbþnbßhþcÜbþnbßhþcÜbþnbßhþcÜbþnbßhþcÜbþnbßhþcÜbþnbßhþcÜbþnbßhþcÜbþnbßhþcÜbþnbßhþcÜbþnbßhþcÜbþnbßhþcÜbþnbßhþcÜbþnbßhþcÜbþnbßhþcÜbþnbßn");
}

// Centers some text within a specified field length.
//
// Parameters:
//  pText: The text to center
//  pFieldLen: The field length
function CenterText(pText, pFieldLen)
{
	var centeredText = "";

	var textLen = strip_ctrl(pText).length;
	// If pFieldLen is less than the text length, then truncate the string.
	if (pFieldLen < textLen)
		centeredText = pText.substring(0, pFieldLen);
	else
	{
		// pFieldLen is at least equal to the text length, so we can
		// center the text.
		// Calculate the number of spaces needed to center the text.
		var numSpaces = pFieldLen - textLen;
		if (numSpaces > 0)
		{
			var rightSpaces = (numSpaces/2).toFixed(0);
			var leftSpaces = numSpaces - rightSpaces;
			// Build centeredText
			for (var i = 0; i < leftSpaces; ++i)
				centeredText += " ";
			centeredText += pText;
			for (var i = 0; i < rightSpaces; ++i)
				centeredText += " ";
		}
		else // pFieldLength is the same length as the text.
			centeredText = pText;
	}

	return centeredText;
}

// Returns an array of message headers for voting topics
//
// Parameters:
//  pSubBoardCode: The internal code of the sub-board to open
//  pCheckIfUserVoted: Boolean - Whether or not to check whether the user has voted on the topics
//
// Return value: An object containing the following properties:
//               errorMsg: A string containing an error message on failure or a blank string on success
//               msgHdrs: An array containing message headers for voting topics
function GetVoteTopicHdrs(pSubBoardCode, pCheckIfUserVoted)
{
	var retObj = {
		errorMsg: "",
		msgHdrs: []
	};

	// Open the chosen sub-board
	var msgbase = new MsgBase(pSubBoardCode);
	if (msgbase.open())
	{
		var msgHdrs = msgbase.get_all_msg_headers(true);
		for (var prop in msgHdrs)
		{
			if ((msgHdrs[prop].type & MSG_TYPE_POLL) == MSG_TYPE_POLL)
			{
				if (pCheckIfUserVoted)
				{
					if (!HasUserVotedOnMsg(msgHdrs[prop].number, pSubBoardCode, msgbase, user))
						retObj.msgHdrs.push(msgHdrs[prop]);
				}
				else
					retObj.msgHdrs.push(msgHdrs[prop]);
			}
		}

		msgbase.close();
	}
	else
		retObj.errorMsg = "Failed to open the messagebase";

	return retObj;
}

function ViewVoteResults(pSubBoardCode)
{
	var nextProgramState = MAIN_MENU;

	var msgbase = new MsgBase(pSubBoardCode);
	if (msgbase.open())
	{
		// Fill an array of message headers for the poll messages.
		// Also, determine the index of the message to read based on the
		// user's last-read pointer.
		var currentMsgIdx = 0;
		var pollMsgIdx = 0;
		var pollMsgHdrs = [];
		var msgHdrs = msgbase.get_all_msg_headers(true);
		for (var prop in msgHdrs)
		{
			if ((msgHdrs[prop].type & MSG_TYPE_POLL) == MSG_TYPE_POLL)
			{
				pollMsgHdrs.push(msgHdrs[prop]);
				if (msgHdrs[prop].number == msg_area.sub[pSubBoardCode].last_read)
					currentMsgIdx = pollMsgIdx;
				++pollMsgIdx;
			}
		}
		delete msgHdrs; // Free some memory
		
		// If there are no polls, then just return
		if (pollMsgHdrs.length == 0)
		{
			msgbase.close();
			console.gotoxy(1, gMessageRow);
			console.print("\1n\1y\1hThere are no topics to view.\1n");
			mswait(ERROR_PAUSE_WAIT_MS);
			console.gotoxy(1, gMessageRow);
			printf("%28s", ""); // Erase the error message
			return nextProgramState;
		}

		// Create the key help line to be displayed at the bottom of the screen
		var keyHelpLine = "\1" + "7" + CenterText("\1rLeft\1b, \1rRight\1b, \1rUp\1b, \1rDn\1b, \1rPgUp\1b/\1rDn\1b, \1rF\1m)\1birst, \1rL\1m)\1bast, \1r#\1b, \1rQ\1m)\1buit", console.screen_columns-1);

		// Get the unmodified default header lines to be displayed
		var displayMsgHdrUnmodified = GetDefaultMsgDisplayHdr();

		// Calculate the height of the frame to use
		var frameHeight = console.screen_rows - displayMsgHdrUnmodified.length - 1;
		// Create the frame object to use for displaying the message
		// TODO: The scrollbar scroll amount seems to change as the message lengths change
		var displayFrame = new Frame(1, // x: Horizontal coordinate of top left
		                             displayMsgHdrUnmodified.length + 1, // y: Vertical coordinate of top left
		                             console.screen_columns, // Width
		                             frameHeight, // Height
		                             BG_BLACK);
		displayFrame.v_scroll = true;
		displayFrame.h_scroll = false;
		displayFrame.scrollbars = true;
		var displayFrameScrollbar = new ScrollBar(displayFrame, {bg: BG_BLACK, fg: LIGHTGRAY, orientation: "vertical", autohide: false});

		// Prepare the screen and display the key help line on the last row of the screen
		console.clear("\1n");
		console.gotoxy(1, console.screen_rows);
		console.print(keyHelpLine);

		// User input loop
		var drawMsg = true;
		var drawKeyHelpLine = false;
		var continueOn = true;
		while (continueOn)
		{
			// Do garbage collection to ensure low memory usage
			js.gc(true);

			// Display the key help line if specified to do so
			if (drawKeyHelpLine)
			{
				console.gotoxy(1, console.screen_rows);
				console.print("\1n" + keyHelpLine);
			}

			// Get the message header lines to be displayed
			var dateTimeStr = pollMsgHdrs[currentMsgIdx]["date"].replace(/ [-+][0-9]+$/, "");
			var displayMsgHdr = GetDisplayMsgHdrForMsg(pollMsgHdrs[currentMsgIdx], displayMsgHdrUnmodified, pSubBoardCode, pollMsgHdrs.length, currentMsgIdx+1, dateTimeStr, false, false);
			// Display the message header on the screen
			if (drawMsg)
			{
				var curPos = { x: 1, y: 1 };
				for (var i = 0; i < displayMsgHdr.length; ++i)
				{
					console.gotoxy(curPos.x, curPos.y++);
					console.print(displayMsgHdr[i]);
				}
				console.print("\1n");
			}
			var msgBodyText = GetMsgBody(msgbase, pollMsgHdrs[currentMsgIdx], pSubBoardCode, user);
			if (msgBodyText == null)
				msgBodyText = "Unable to load poll";

			// Load the poll text into the Frame object and draw the frame
			displayFrame.clear();
			displayFrame.attr&=~HIGH;
			displayFrame.putmsg(msgBodyText, "\1n");
			displayFrame.scrollTo(0, 0);
			if (drawMsg)
				displayFrame.draw();
			var scrollRetObj = ScrollFrame(displayFrame, displayFrameScrollbar, 0, "\1n", false, 1, console.screen_rows);
			drawMsg = true;
			drawKeyHelpLine = false;
			if (scrollRetObj.lastKeypress == KEY_LEFT)
			{
				// Go back one poll topic
				if (currentMsgIdx > 0)
					--currentMsgIdx;
				else
					drawMsg = false;
			}
			else if (scrollRetObj.lastKeypress == KEY_RIGHT)
			{
				// Go to the next poll topic, if there is one
				if (currentMsgIdx < pollMsgHdrs.length-1)
					++currentMsgIdx;
				else
					drawMsg = false;
			}
			else if (scrollRetObj.lastKeypress == gReaderKeys.goToFirst)
			{
				// Go to the first poll
				if (currentMsgIdx > 0)
					currentMsgIdx = 0;
				else
					drawMsg = false;
			}
			else if (scrollRetObj.lastKeypress == gReaderKeys.goToLast)
			{
				// Go to the last poll
				if (currentMsgIdx < pollMsgHdrs.length-1)
					currentMsgIdx = pollMsgHdrs.length-1;
				else
					drawMsg = false;
			}
			else if (scrollRetObj.lastKeypress == gReaderKeys.vote)
			{
				// Let the user vote on the topic in interactive mode (this uses
				// traditional style interaction rather than usinga lightbar).
				var voteRetObj = VoteOnTopic(pSubBoardCode, msgbase, pollMsgHdrs[currentMsgIdx], user, null, true);
				drawKeyHelpLine = true;
				// If the user's vote was saved, then update the message header so that it includes
				// the user's vote information.
				if (voteRetObj.savedVote)
				{
					var msgHdrs = msgbase.get_all_msg_headers(true);
					pollMsgHdrs[currentMsgIdx] = msgHdrs[pollMsgHdrs[currentMsgIdx].number];
					delete msgHdrs;
				}
				// If there is an error message, then display it.
				if (voteRetObj.errorMsg.length > 0)
				{
					console.gotoxy(1, gMessageRow);
					if (voteRetObj.mnemonicsRequiredForErrorMsg)
					{
						console.mnemonics(voteRetObj.errorMsg);
						mswait(ERROR_PAUSE_WAIT_MS);
						console.gotoxy(1, gMessageRow);
						printf("\1n%" + console.screen_columns + "s", "");
					}
					else
					{
						console.print("\1n\1y\1h" + voteRetObj.errorMsg);
						mswait(ERROR_PAUSE_WAIT_MS);
						console.gotoxy(1, gMessageRow);
						printf("\1n%" + strip_ctrl(voteRetObj.errorMsg).length + "s", "");
					}
				}
			}
			else if (/[0-9]/.test(scrollRetObj.lastKeypress))
			{
				// The user started typing a number - Continue inputting the
				// poll number and go to that poll
				// TODO
			}
			else if (scrollRetObj.lastKeypress == gReaderKeys.quit)
				continueOn = false;

			// Update the user's scan pointer and last read message pointer
			if (pSubBoardCode != "mail") // && !this.SearchTypePopulatesSearchResults()
			{
				// What if newest_message_header.number is invalid  (e.g. NaN or 0xffffffff or >
				// msgbase.last_msg)?
				if (pollMsgHdrs[currentMsgIdx].number > msg_area.sub[pSubBoardCode].scan_ptr)
					msg_area.sub[pSubBoardCode].scan_ptr = pollMsgHdrs[currentMsgIdx].number;
				msg_area.sub[pSubBoardCode].last_read = pollMsgHdrs[currentMsgIdx].number;
			}
		}

		msgbase.close();
	}

	return nextProgramState;
}

// Returns an array with lines to display on the screen for a default message header
function GetDefaultMsgDisplayHdr()
{
	var hdrDisplayLines = [];

	// Group name: 20% of console width
	// Sub-board name: 34% of console width
	var msgGrpNameLen = Math.floor(console.screen_columns * 0.2);
	var subBoardNameLen = Math.floor(console.screen_columns * 0.34);
	var hdrLine1 = "\1n\1h\1c" + UPPER_LEFT_SINGLE + HORIZONTAL_SINGLE + "\1n\1c"
	             + HORIZONTAL_SINGLE + " \1h@GRP-L";
	var numChars = msgGrpNameLen - 7;
	for (var i = 0; i < numChars; ++i)
		hdrLine1 += "#";
	hdrLine1 += "@ @SUB-L";
	numChars = subBoardNameLen - 7;
	for (var i = 0; i < numChars; ++i)
		hdrLine1 += "#";
	hdrLine1 += "@\1k";
	numChars = console.screen_columns - console.strlen(hdrLine1) - 4;
	for (var i = 0; i < numChars; ++i)
		hdrLine1 += HORIZONTAL_SINGLE;
	hdrLine1 += "\1n\1c" + HORIZONTAL_SINGLE + HORIZONTAL_SINGLE + "\1h"
	         + HORIZONTAL_SINGLE + UPPER_RIGHT_SINGLE;
	hdrDisplayLines.push(hdrLine1);
	var hdrLine2 = "\1n\1c" + VERTICAL_SINGLE + "\1h\1k" + BLOCK1 + BLOCK2
	             + BLOCK3 + "\1gM\1n\1gsg#\1h\1c: \1b@MSG_NUM_AND_TOTAL-L";
	numChars = console.screen_columns - 32;
	for (var i = 0; i < numChars; ++i)
		hdrLine2 += "#";
	hdrLine2 += "@\1n\1c" + VERTICAL_SINGLE;
	hdrDisplayLines.push(hdrLine2);
	var hdrLine3 = "\1n\1h\1k" + VERTICAL_SINGLE + BLOCK1 + BLOCK2 + BLOCK3
				 + "\1gF\1n\1grom\1h\1c: \1b@MSG_FROM-L";
	numChars = console.screen_columns - 23;
	for (var i = 0; i < numChars; ++i)
		hdrLine3 += "#";
	hdrLine3 += "@\1k" + VERTICAL_SINGLE;
	hdrDisplayLines.push(hdrLine3);
	var hdrLine4 = "\1n\1h\1k" + VERTICAL_SINGLE + BLOCK1 + BLOCK2 + BLOCK3
	             + "\1gS\1n\1gubj\1h\1c: \1b@MSG_SUBJECT-L";
	numChars = console.screen_columns - 26;
	for (var i = 0; i < numChars; ++i)
		hdrLine4 += "#";
	hdrLine4 += "@\1k" + VERTICAL_SINGLE;
	hdrDisplayLines.push(hdrLine4);
	var hdrLine5 = "\1n\1c" + VERTICAL_SINGLE + "\1h\1k" + BLOCK1 + BLOCK2 + BLOCK3
	             + "\1gD\1n\1gate\1h\1c: \1b@MSG_DATE-L";
	//numChars = console.screen_columns - 23;
	numChars = console.screen_columns - 67;
	for (var i = 0; i < numChars; ++i)
		hdrLine5 += "#";
	//hdrLine5 += "@\1n\1c" + VERTICAL_SINGLE;
	hdrLine5 += "@ @MSG_TIMEZONE@\1n";
	for (var i = 0; i < 40; ++i)
		hdrLine5 += " ";
	hdrLine5 += "\1n\1c" + VERTICAL_SINGLE;
	hdrDisplayLines.push(hdrLine5);
	var hdrLine6 = "\1n\1h\1c" + BOTTOM_T_SINGLE + HORIZONTAL_SINGLE + "\1n\1c"
	             + HORIZONTAL_SINGLE + HORIZONTAL_SINGLE + "\1h\1k";
	numChars = console.screen_columns - 8;
	for (var i = 0; i < numChars; ++i)
		hdrLine6 += HORIZONTAL_SINGLE;
	hdrLine6 += "\1n\1c" + HORIZONTAL_SINGLE + HORIZONTAL_SINGLE + "\1h"
	         + HORIZONTAL_SINGLE + BOTTOM_T_SINGLE;
	hdrDisplayLines.push(hdrLine6);

	return hdrDisplayLines;
}

function GetDisplayMsgHdrForMsg(pMsgHdr, pOriginalDisplayHdrLines, pSubBoardCode, pNumMsgs, pDisplayMsgNum, pDateTimeStr, pBBSLocalTimeZone, pAllowCLS)
{
	var displayHdrForMsg = [];
	for (var i = 0; i < pOriginalDisplayHdrLines.length; ++i)
	{
		var hdrLine = ParseMsgAtCodes(pOriginalDisplayHdrLines[i], pMsgHdr, pSubBoardCode, pNumMsgs, pDisplayMsgNum, pDateTimeStr, pBBSLocalTimeZone, pAllowCLS);
		displayHdrForMsg.push(hdrLine);
	}
	return displayHdrForMsg;
}

// Replaces a given @-code format string in a text line with the appropriate
// message header info or BBS system info.
//
// Parameters:
//  pMsgHdr: The object containing the message header information
//  pSubBoardCode: The internal code of the sub-board that the message header is from
//  pNumMsgs: The total number of messages available
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
function ReplaceMsgAtCodeFormatStr(pMsgHdr, pSubBoardCode, pNumMsgs, pDisplayMsgNum, pTextLine, pSpecifiedLen,
                                   pAtCodeStr, pDateTimeStr, pUseBBSLocalTimeZone, pMsgMainAttrStr, pMsgAuxAttrStr,
								   pMsgNetAttrStr, pMsgAllAttrStr, pDashJustifyIdx)
{
	var readingPersonalEmail = (pSubBoardCode == "mail");

	if (typeof(pDashJustifyIdx) != "number")
		pDashJustifyIdx = findDashJustifyIndex(pAtCodeStr);
	// Specify the format string with left or right justification based on the justification
	// character (either L or R).
	var formatStr = ((/L/i).test(pAtCodeStr.charAt(pDashJustifyIdx+1)) ? "%-" : "%") + pSpecifiedLen + "s";
	// Specify the replacement text depending on the @-code string
	var replacementTxt = "";
	if (pAtCodeStr.indexOf("@MSG_FROM") > -1)
		replacementTxt = pMsgHdr["from"].substr(0, pSpecifiedLen);
	else if (pAtCodeStr.indexOf("@MSG_FROM_EXT") > -1)
		replacementTxt = (typeof pMsgHdr["from_ext"] === "undefined" ? "" : pMsgHdr["from_ext"].substr(0, pSpecifiedLen));
	else if ((pAtCodeStr.indexOf("@MSG_TO") > -1) || (pAtCodeStr.indexOf("@MSG_TO_NAME") > -1))
		replacementTxt = pMsgHdr["to"].substr(0, pSpecifiedLen);
	else if (pAtCodeStr.indexOf("@MSG_TO_EXT") > -1)
		replacementTxt = (typeof pMsgHdr["to_ext"] === "undefined" ? "" : pMsgHdr["to_ext"].substr(0, pSpecifiedLen));
	else if (pAtCodeStr.indexOf("@MSG_SUBJECT") > -1)
		replacementTxt = pMsgHdr["subject"].substr(0, pSpecifiedLen);
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
		var messageNum = (typeof(pDisplayMsgNum) == "number" ? pDisplayMsgNum : pMsgHdr["offset"]+1);
		replacementTxt = (messageNum.toString() + "/" + pNumMsgs).substr(0, pSpecifiedLen); // "number" is also absolute number
	}
	else if (pAtCodeStr.indexOf("@MSG_NUM") > -1)
	{
		var messageNum = (typeof(pDisplayMsgNum) == "number" ? pDisplayMsgNum : pMsgHdr["offset"]+1);
		replacementTxt = messageNum.toString().substr(0, pSpecifiedLen); // "number" is also absolute number
	}
	else if (pAtCodeStr.indexOf("@MSG_ID") > -1)
		replacementTxt = (typeof pMsgHdr["id"] === "undefined" ? "" : pMsgHdr["id"].substr(0, pSpecifiedLen));
	else if (pAtCodeStr.indexOf("@MSG_REPLY_ID") > -1)
		replacementTxt = (typeof pMsgHdr["reply_id"] === "undefined" ? "" : pMsgHdr["reply_id"].substr(0, pSpecifiedLen));
	else if (pAtCodeStr.indexOf("@MSG_TIMEZONE") > -1)
	{
		if (pUseBBSLocalTimeZone)
			replacementTxt = system.zonestr(system.timezone).substr(0, pSpecifiedLen);
		else
			replacementTxt = system.zonestr(pMsgHdr["when_written_zone"]).substr(0, pSpecifiedLen);
	}
	else if (pAtCodeStr.indexOf("@GRP") > -1)
	{
		if (readingPersonalEmail)
			replacementTxt = "Personal mail".substr(0, pSpecifiedLen);
		else
			replacementTxt = msg_area.sub[pSubBoardCode].grp_name.substr(0, pSpecifiedLen);
			
	}
	else if (pAtCodeStr.indexOf("@GRPL") > -1)
	{
		if (readingPersonalEmail)
			replacementTxt = "Personal mail".substr(0, pSpecifiedLen);
		else
		{
			var grpIdx = msg_area.sub[pSubBoardCode].grp_index;
			replacementTxt = msg_area.grp_list[grpIdx].description.substr(0, pSpecifiedLen);
		}
	}
	else if (pAtCodeStr.indexOf("@SUB") > -1)
	{
		if (readingPersonalEmail)
			replacementTxt = "Personal mail".substr(0, pSpecifiedLen);
		else

			replacementTxt = msg_area.sub[pSubBoardCode].name.substr(0, pSpecifiedLen);
	}
	else if (pAtCodeStr.indexOf("@SUBL") > -1)
	{
		if (readingPersonalEmail)
			replacementTxt = "Personal mail".substr(0, pSpecifiedLen);
		else
			replacementTxt = msg_area.sub[pSubBoardCode].description.substr(0, pSpecifiedLen);
	}
	else if ((pAtCodeStr.indexOf("@BBS") > -1) || (pAtCodeStr.indexOf("@BOARDNAME") > -1))
		replacementTxt = system.name.substr(0, pSpecifiedLen);
	else if (pAtCodeStr.indexOf("@SYSOP") > -1)
		replacementTxt = system.operator.substr(0, pSpecifiedLen);
	else if (pAtCodeStr.indexOf("@CONF") > -1)
	{
		var msgConfDesc = msg_area.grp_list[msg_area.sub[pSubBoardCode].grp_index].name + " "
		                + msg_area.sub[pSubBoardCode].name;
		replacementTxt = msgConfDesc.substr(0, pSpecifiedLen);
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

// Returns the index of the -L or -R in one of the @-code strings.
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

// Looks for complex @-code strings in a text line and parses & replaces them
// appropriately with the appropriate info from the message header object and/or
// message base object.  This is more complex than simple text substitution
// because message subject @-codes could include something like @MSG_SUBJECT-L######@
// or @MSG_SUBJECT-R######@ or @MSG_SUBJECT-L20@ or @MSG_SUBJECT-R20@.
//
// Parameters:
//  pTextLine: The line of text to search
//  pMsgHdr: The message header object
//  pSubBoardCode: The internal code of the sub-board
//  pNumMsgs: The total number of messages available
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
function ParseMsgAtCodes(pTextLine, pMsgHdr, pSubBoardCode, pNumMsgs, pDisplayMsgNum, pDateTimeStr, pBBSLocalTimeZone, pAllowCLS)
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
	var allMsgAttrStr = makeAllMsgAttrStr(pMsgHdr);
	var mainMsgAttrStr = makeMainMsgAttrStr(pMsgHdr.attr);
	var auxMsgAttrStr = makeAuxMsgAttrStr(pMsgHdr.auxattr);
	var netMsgAttrStr = makeNetMsgAttrStr(pMsgHdr.netattr);
	// An array of @-code strings without the trailing @, to be used for constructing
	// regular expressions to look for versions with justification & length specifiers.
	// The order of the strings in this array matters.  For instance, @MSG_NUM_AND_TOTAL
	// needs to come before @MSG_NUM so that it gets processed properly, since they
	// both start out with the same text.
	var atCodeStrBases = ["@MSG_FROM", "@MSG_FROM_EXT", "@MSG_TO", "@MSG_TO_NAME", "@MSG_TO_EXT",
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
				textLine = ReplaceMsgAtCodeFormatStr(pMsgHdr, pSubBoardCode, pNumMsgs, pDisplayMsgNum, textLine, substLen,
				                                     atCodeMatchArray[idx], pDateTimeStr, useBBSLocalTimeZone,
				                                     mainMsgAttrStr, auxMsgAttrStr, netMsgAttrStr, allMsgAttrStr);
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
				textLine = ReplaceMsgAtCodeFormatStr(pMsgHdr, pSubBoardCode, pNumMsgs, pDisplayMsgNum, textLine, substLen, atCodeMatchArray[idx],
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
	if (pSubBoardCode == "mail")
	{
		var subName = "Personal mail";
		var subDesc = "Personal mail";
	}
	else
	{
		grpIdx = msg_area.sub[pSubBoardCode].grp_index;
		groupName = msg_area.sub[pSubBoardCode].grp_name;
		groupDesc = msg_area.grp_list[grpIdx].description;
		subName = msg_area.sub[pSubBoardCode].name;
		subDesc = msg_area.sub[pSubBoardCode].description;
		msgConf = msg_area.grp_list[msg_area.sub[pSubBoardCode].grp_index].name + " "
		        + msg_area.sub[pSubBoardCode].name;
	}
	if ((typeof(bbs.curlib) == "number") && (typeof(bbs.curdir) == "number"))
	{
		if ((typeof(file_area.lib_list[bbs.curlib]) == "object") && (typeof(file_area.lib_list[bbs.curlib].dir_list[bbs.curdir]) == "object"))
		{
			fileDir = file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].name;
			fileDirLong = file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].description;
		}
	}
	var messageNum = (typeof(pDisplayMsgNum) == "number" ? pDisplayMsgNum : pMsgHdr["offset"]+1);
	var msgUpvotes = 0;
	var msgDownvotes = 0;
	var msgVoteScore = 0;
	if (pMsgHdr.hasOwnProperty("total_votes") && pMsgHdr.hasOwnProperty("upvotes"))
	{
		msgUpvotes = pMsgHdr.upvotes;
		msgDownvotes = pMsgHdr.total_votes - pMsgHdr.upvotes;
		msgVoteScore = pMsgHdr.upvotes - msgDownvotes;
	}
	var newTxtLine = textLine.replace(/@MSG_SUBJECT@/gi, pMsgHdr["subject"])
	                         .replace(/@MSG_FROM@/gi, pMsgHdr["from"])
	                         .replace(/@MSG_FROM_EXT@/gi, (typeof(pMsgHdr["from_ext"]) == "string" ? pMsgHdr["from_ext"] : ""))
	                         .replace(/@MSG_TO@/gi, pMsgHdr["to"])
	                         .replace(/@MSG_TO_NAME@/gi, pMsgHdr["to"])
	                         .replace(/@MSG_TO_EXT@/gi, (typeof(pMsgHdr["to_ext"]) == "string" ? pMsgHdr["to_ext"] : ""))
	                         .replace(/@MSG_DATE@/gi, pDateTimeStr)
	                         .replace(/@MSG_ATTR@/gi, mainMsgAttrStr)
	                         .replace(/@MSG_AUXATTR@/gi, auxMsgAttrStr)
	                         .replace(/@MSG_NETATTR@/gi, netMsgAttrStr)
	                         .replace(/@MSG_ALLATTR@/gi, allMsgAttrStr)
	                         .replace(/@MSG_NUM_AND_TOTAL@/gi, messageNum.toString() + "/" + pNumMsgs)
	                         .replace(/@MSG_NUM@/gi, messageNum.toString())
	                         .replace(/@MSG_ID@/gi, (typeof(pMsgHdr["id"]) == "string" ? pMsgHdr["id"] : ""))
	                         .replace(/@MSG_REPLY_ID@/gi, (typeof(pMsgHdr["reply_id"]) == "string" ? pMsgHdr["reply_id"] : ""))
	                         .replace(/@MSG_FROM_NET@/gi, (typeof(pMsgHdr["from_net_addr"]) == "string" ? pMsgHdr["from_net_addr"] : ""))
	                         .replace(/@MSG_TO_NET@/gi, (typeof(pMsgHdr["to_net_addr"]) == "string" ? pMsgHdr["to_net_addr"] : ""))
	                         .replace(/@MSG_TIMEZONE@/gi, (useBBSLocalTimeZone ? system.zonestr(system.timezone) : system.zonestr(pMsgHdr["when_written_zone"])))
	                         .replace(/@GRP@/gi, groupName)
	                         .replace(/@GRPL@/gi, groupDesc)
	                         .replace(/@SUB@/gi, subName)
	                         .replace(/@SUBL@/gi, subDesc)
							 .replace(/@CONF@/gi, msgConf)
							 .replace(/@BBS@/gi, system.name)
							 .replace(/@BOARDNAME@/gi, system.name)
							 .replace(/@SYSOP@/gi, system.operator)
							 .replace(/@DATE@/gi, system.datestr())
							 .replace(/@LOCATION@/gi, system.location)
							 .replace(/@DIR@/gi, fileDir)
							 .replace(/@DIRL@/gi, fileDirLong)
							 .replace(/@ALIAS@/gi, user.alias)
							 .replace(/@NAME@/gi, (user.name.length > 0 ? user.name : user.alias))
							 .replace(/@USER@/gi, user.alias)
							 .replace(/@MSG_UPVOTES@/gi, msgUpvotes)
							 .replace(/@MSG_DOWNVOTES@/gi, msgDownvotes)
							 .replace(/@MSG_SCORE@/gi, msgVoteScore);
	if (!pAllowCLS)
		newTxtLine = newTxtLine.replace(/@CLS@/gi, "");
	return newTxtLine;
}

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

// Returns a string describing the main message attributes.  Makes use of the
// gMainMsgAttrStrs object for the main message attributes and description
// strings.
//
// Parameters:
//  pMainMsgAttrs: The bit field for the main message attributes
//                 (normally, the 'attr' property of a header object)
//  pIfEmptyString: Optional - A string to use if there are no attributes set
//
// Return value: A string describing the main message attributes
function makeMainMsgAttrStr(pMainMsgAttrs, pIfEmptyString)
{
   var msgAttrStr = "";
   if (typeof(pMainMsgAttrs) == "number")
   {
      for (var prop in gMainMsgAttrStrs)
      {
         if ((pMainMsgAttrs & prop) == prop)
         {
            if (msgAttrStr.length > 0)
               msgAttrStr += ", ";
            msgAttrStr += gMainMsgAttrStrs[prop];
         }
      }
   }
   if ((msgAttrStr.length == 0) && (typeof(pIfEmptyString) == "string"))
	   msgAttrStr = pIfEmptyString;
   return msgAttrStr;
}

// Returns a string describing auxiliary message attributes.  Makes use of the
// gAuxMsgAttrStrs object for the auxiliary message attributes and description
// strings.
//
// Parameters:
//  pAuxMsgAttrs: The bit field for the auxiliary message attributes
//                (normally, the 'auxattr' property of a header object)
//  pIfEmptyString: Optional - A string to use if there are no attributes set
//
// Return value: A string describing the auxiliary message attributes
function makeAuxMsgAttrStr(pAuxMsgAttrs, pIfEmptyString)
{
   var msgAttrStr = "";
   if (typeof(pAuxMsgAttrs) == "number")
   {
      for (var prop in gAuxMsgAttrStrs)
      {
         if ((pAuxMsgAttrs & prop) == prop)
         {
            if (msgAttrStr.length > 0)
               msgAttrStr += ", ";
            msgAttrStr += gAuxMsgAttrStrs[prop];
         }
      }
   }
   if ((msgAttrStr.length == 0) && (typeof(pIfEmptyString) == "string"))
	   msgAttrStr = pIfEmptyString;
   return msgAttrStr;
}

// Returns a string describing network message attributes.  Makes use of the
// gNetMsgAttrStrs object for the network message attributes and description
// strings.
//
// Parameters:
//  pNetMsgAttrs: The bit field for the network message attributes
//                (normally, the 'netattr' property of a header object)
//  pIfEmptyString: Optional - A string to use if there are no attributes set
//
// Return value: A string describing the network message attributes
function makeNetMsgAttrStr(pNetMsgAttrs, pIfEmptyString)
{
   var msgAttrStr = "";
   if (typeof(pNetMsgAttrs) == "number")
   {
      for (var prop in gNetMsgAttrStrs)
      {
         if ((pNetMsgAttrs & prop) == prop)
         {
            if (msgAttrStr.length > 0)
               msgAttrStr += ", ";
            msgAttrStr += gNetMsgAttrStrs[prop];
         }
      }
   }
   if ((msgAttrStr.length == 0) && (typeof(pIfEmptyString) == "string"))
	   msgAttrStr = pIfEmptyString;
   return msgAttrStr;
}

// Adjusts a message's when-written time to the BBS's local time.
//
// Parameters:
//  pMsgHdr: A message header object
//
// Return value: The message's when_written_time adjusted to the BBS's local time.
//               If the message header doesn't have a when_written_time or
//               when_written_zone property, then this function will return -1.
function MsgWrittenTimeToLocalBBSTime(pMsgHdr)
{
	if (!pMsgHdr.hasOwnProperty("when_written_time") || !pMsgHdr.hasOwnProperty("when_written_zone_offset") || !pMsgHdr.hasOwnProperty("when_imported_zone_offset"))
		return -1;

	var timeZoneDiffMinutes = pMsgHdr.when_imported_zone_offset - pMsgHdr.when_written_zone_offset;
	//var timeZoneDiffMinutes = pMsgHdr.when_written_zone - system.timezone;
	var timeZoneDiffSeconds = timeZoneDiffMinutes * 60;
	var msgWrittenTimeAdjusted = pMsgHdr.when_written_time + timeZoneDiffSeconds;
	return msgWrittenTimeAdjusted;
}

// Inputs a keypress from the user and handles some ESC-based
// characters such as PageUp, PageDown, and ESC.  If PageUp
// or PageDown are pressed, this function will return the
// string "\1PgUp" (KEY_PAGE_UP) or "\1Pgdn" (KEY_PAGE_DOWN),
// respectively.  Also, F1-F5 will be returned as "\1F1"
// through "\1F5", respectively.
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
					userInput = KEY_PAGE_UP;
					break;
				case 'U':
					userInput = KEY_PAGE_DOWN;
					break;
				}
				break;
			case 'O':
				switch (console.inkey(K_NOECHO|K_NOSPIN, 2)) {
				case 'P':
					userInput = "\1F1";
					break;
				case 'Q':
					userInput = "\1F2";
					break;
				case 'R':
					userInput = "\1F3";
					break;
				case 'S':
					userInput = "\1F4";
					break;
				case 't':
					userInput = "\1F5";
					break;
				}
			default:
				break;
		}
	}

	return userInput;
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

	var retObj = new Object();
	retObj.lastKeypress = "";
	retObj.topLineIdx = pTopLineIdx;

	if (pTopLineIdx > 0)
		pFrame.scrollTo(0, pTopLineIdx);

	var writeTxtLines = pWriteTxtLines;
	if (writeTxtLines)
	{
		pFrame.invalidate(); // Force drawing on the next call to draw() or cycle()
		pFrame.cycle();
		//pFrame.draw();
	}

	// Note: It seems that in order for the frame's scrollbar to be accurate,
	// the frame & scrollbar must be cycled at least once initially.
	var cycleFrame = true;
	var continueOn = true;
	while (continueOn)
	{
		// If we are to write the text lines, then draw the frame.
		// TODO: Do we really need this?  Will this be different from
		// scrollTextLines()?
		//if (writeTxtLines)
		//	pFrame.draw();

		if (cycleFrame)
		{
			// Invalidate the frame to force it to redraw everything, as a
			// workaround to clear the background before writing again
			// TODO: I might want to remove this invalidate() later when
			// Frame is fixed to redraw better on scrolling.
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
		retObj.lastKeypress = getKeyWithESCChars(K_UPPER|K_NOCRLF|K_NOECHO|K_NOSPIN);
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
			case KEY_PAGE_DOWN: // Next page
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
			case KEY_PAGE_UP: // Previous page
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