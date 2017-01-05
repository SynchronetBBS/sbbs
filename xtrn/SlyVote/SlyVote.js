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

// Version information
var SLYVOTE_VERSION = "0.01 Beta";
var SLYVOTE_DATE = "2017-01-04";

// Determine the script's startup directory.
// This code is a trick that was created by Deuce, suggested by Rob Swindell
// as a way to detect which directory the script was executed in.  I've
// shortened the code a little.
var gStartupPath = '.';
try { throw dig.dist(dist); } catch(e) { gStartupPath = e.fileName; }
gStartupPath = backslash(gStartupPath.replace(/[\/\\][^\/\\]*$/,''));

var ERROR_PAUSE_WAIT_MS = 1500;

var gBottomBorderRow = 23;

load("frame.js");
load("DDLightbarMenu.js");

// See if frame.js and scrollbar.js exist in sbbs/exec/load on the BBS machine.
// If so, load them.  They will be used for displaying messages with ANSI content
// with a scrollable user interface.
var gFrameJSAvailable = file_exists(backslash(system.exec_dir) + "load/frame.js");
if (gFrameJSAvailable)
	load("frame.js");
var gScrollbarJSAvailable = file_exists(backslash(system.exec_dir) + "load/scrollbar.js");
if (gScrollbarJSAvailable)
	load("scrollbar.js");


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
	console.gotoxy(1, subBoardsLB.pos.y+subBoardsLB.size.height+1);
}

//bbs.exec("?postpoll.js");

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
		case EXIT_SLYVOTE:
			gContinueSlyVote = false;
			break;
	}
}






////////////////////////////////////////////////////////////
// Helper functions
////////////////////////////////////////////////////////////

function DoMainMenu()
{
	gProgramState = MAIN_MENU;
	var nextProgramState = MAIN_MENU;
	// Display the SlyVote screen and menu of choices
	console.clear("\1n");
	var mainScrRetObj = DisplaySlyVoteMainVoteScreen(false);
	var voteOptsMenu = new DDLightbarMenu(mainScrRetObj.optMenuX, mainScrRetObj.optMenuY, 17, 5);
	voteOptsMenu.Add("Vote On A Booth", "vote", "1");
	voteOptsMenu.Add("Answer All Booths", "answerAll", "2");
	voteOptsMenu.Add("Create A Booth", "create", "3");
	voteOptsMenu.Add("View Results", "viewResults", "4");
	voteOptsMenu.Add("Quit To BBS", "quit", "5");
	voteOptsMenu.colors.itemColor = "\1n\1w";
	voteOptsMenu.colors.selectedItemColor = "\1n\1" + "4\1w\1h";
	// Get the user's choice and take appropriate action
	var userChoice = voteOptsMenu.GetVal(true);
	if (userChoice == "vote")
		nextProgramState = VOTING_ON_A_TOPIC;
	else if (userChoice == "answerAll")
	{
		
	}
	else if (userChoice == "create")
	{
		
	}
	else if (userChoice == "viewResults")
	{
		
	}
	else if (userChoice == "quit")
		nextProgramState = EXIT_SLYVOTE;

	return nextProgramState;
}

function ChooseVoteTopic()
{
	gProgramState = VOTING_ON_A_TOPIC;
	var nextProgramState = VOTING_ON_A_TOPIC;
	// Clear the screen between the top & bottom borders
	var formatStr = "%" + console.screen_columns + "s";
	console.print("\1n");
	for (var posY = 3; posY < gBottomBorderRow; ++posY)
	{
		console.gotoxy(1, posY);
		printf(formatStr, "");
	}

	// Get the list of polls for the selected sub-board
	var voteTopicInfo = GetVoteTopicHdrs(gSubBoardCode);
	if (voteTopicInfo.errorMsg.length > 0)
	{
		console.gotoxy(1, 3);
		console.print("\1n\1y\1h" + voteTopicInfo.errorMsg + "\1n");
		console.crlf();
		console.pause();
		return;
	}
	else if (voteTopicInfo.msgHdrs.length == 0)
	{
		console.gotoxy(1, 3);
		console.print("\1n\1cThere are no polls to vote on in this section\1n");
		console.crlf();
		console.pause();
		return;
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
			var voteRetObj = VoteOnTopic(gSubBoardCode, userChoice, startCol, listTopRow, textLen, menuHeight);
			// TODO: Check for voteRetObj.errorMsg, etc.
			drawTopicsMenu = true;
		}
		else // The user chose to exit the topics menu
			nextProgramState = MAIN_MENU;
	}

	return nextProgramState;
}

// Lets the user vote on a topic
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
function VoteOnTopic(pSubBoardCode, pMsgNum, pStartCol, pStartRow, pMenuWidth, pMenuHeight)
{
	var retObj = {
		errorMsg: "",
		nextProgramState: VOTING_ON_A_TOPIC
	};

	// Open the chosen sub-board
	var msgbase = new MsgBase(pSubBoardCode);
	if (msgbase.open())
	{
		var msgHdr = msgbase.get_msg_header(false, pMsgNum, true);
		if (msgHdr != null)
		{
			var pollTextAndOpts = GetPollTextAndOpts(msgHdr);
			// Print the poll question text
			console.gotoxy(1, pStartRow-4);
			//printf("\1n\1c%-" + console.screen_columns + "s", msgHdr.subject.substr(0, console.screen_columns));
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
			// TODO: If the poll has already been voted on by the user, then show it before inputting
			// the user's choice.
			var userChoice = optionsMenu.GetVal(true);
			// Return value: An object with the following properties:
			//               BBSHasVoteFunction: Boolean - Whether or not the system has
			//                                   the vote_msg function
			//               savedVote: Boolean - Whether or not the vote was saved
			//               userQuit: Boolean - Whether or not the user quit and didn't vote
			//               errorMsg: String - An error message, if something went wrong
			//               mnemonicsRequiredForErrorMsg: Boolean - Whether or not mnemonics is required to print the error message
			//               updatedHdr: The updated message header containing vote information.
			//                           If something went wrong, this will be null.
			var voteRetObj = VoteOnMessage(pSubBoardCode, msgbase, msgHdr, user, userChoice, true);
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
		this.cfgFileSuccessfullyRead = true;

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
			var unvotedOptionFormatStr = "\1n\1c\1h%2d\1n\1c: \1w\1h%-" + voteOptDescLen + "s [%-4d %6.2f%]\1n";
			var votedOptionFormatStr = "\1n\1c\1h%2d\1n\1c: \1" + "5\1w\1h%-" + voteOptDescLen + "s [%-4d %6.2f%]\1n";
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
			{
				msgBody += "\1n\r\n\1gTo vote in this poll, press \1w\1h"
				        + this.enhReaderKeys.vote + "\1n\1g now.";
			}

			// If the current logged-in user created this poll, then show the
			// users who have voted on it so far.
			var msgFromUpper = pMsgHdr.from.toUpperCase();
			if ((msgFromUpper == user.name.toUpperCase()) || (msgFromUpper == user.handle.toUpperCase()))
			{
				// Check all the messages in the messagebase after the current one
				// to find poll response messages
				if (typeof(this.msgbase.get_all_msg_headers) === "function")
				{
					// Get the line from text.dat for writing who voted & when.  It
					// is a format string and should look something like this:
					//"\r\n\1n\1hOn %s, in \1c%s \1n\1c%s\r\n\1h\1m%s voted in your poll: \1n\1h%s\r\n" 787 PollVoteNotice
					var userVotedInYourPollText = bbs.text(typeof(PollVoteNotice) != "undefined" ? PollVoteNotice : 787);

					// Pass true to get_all_msg_headers() to tell it to return vote messages
					// (the parameter was introduced in Synchronet 3.17+)
					var tmpHdrs = this.msgbase.get_all_msg_headers(true);
					for (var tmpProp in tmpHdrs)
					{
						if (tmpHdrs[tmpProp] == null)
							continue;
						// If this header's thread_back or reply_id matches the poll message
						// number, then append the 'user voted' string to the message body.
						if ((tmpHdrs[tmpProp].thread_back == pMsgHdr.number) || (tmpHdrs[tmpProp].reply_id == pMsgHdr.number))
						{
							var msgWrittenLocalTime = msgWrittenTimeToLocalBBSTime(tmpHdrs[tmpProp]);
							var voteDate = strftime("%a %b %d %Y %H:%M:%S", msgWrittenLocalTime);
							msgBody += format(userVotedInYourPollText, voteDate, this.msgbase.cfg.grp_name, this.msgbase.cfg.name,
							                  tmpHdrs[tmpProp].from, pMsgHdr.subject);
						}
					}
				}
			}
		}
	}
	else
		msgBody = this.msgbase.get_msg_body(false, pMsgHdr.number);
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
function VoteOnMessage(pSubBoardCode, pMsgbase, pMsgHdr, pUser, pUserVoteNumber, pRemoveNLsFromVoteText)
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
		retObj.errorMsg = bbs.text(typeof(VotingNotAllowed) != "undefined" ? VotingNotAllowed : 779);
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
			retObj.errorMsg = bbs.text(typeof(VotedAlready) != "undefined" ? VotedAlready : 780);
			if (removeNLsFromVoteText)
				retObj.errorMsg = retObj.errorMsg.replace("\r\n", "").replace("\n", "").replace("\N", "").replace("\r", "").replace("\R", "").replace("\R\n", "").replace("\r\N", "").replace("\R\N", "");
			retObj.mnemonicsRequiredForErrorMsg = true;
			return retObj;
		}
	}

	// If the user has voted on this message already, then set an error message and return.
	if (HasUserVotedOnMsg(pMsgHdr.number, pSubBoardCode, pMsgbase, pUser))
	{
		retObj.errorMsg = bbs.text(typeof(VotedAlready) != "undefined" ? VotedAlready : 780);
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
//
// Return value: An object containing the following properties:
//               errorMsg: A string containing an error message on failure or a blank string on success
//               msgHdrs: An array containing message headers for voting topics
function GetVoteTopicHdrs(pSubBoardCode)
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
				// TODO: See if the logged-in user has already voted on this?
				retObj.msgHdrs.push(msgHdrs[prop]);
			}
		}

		msgbase.close();
	}
	else
		retObj.errorMsg = "Failed to open the messagebase";

	return retObj;
}