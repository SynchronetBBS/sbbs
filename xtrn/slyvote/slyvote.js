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
 * 2017-02-26 Eric Oulashin     Version 0.17 Beta
 *                              Mostly complete; continuing to make tweaks
 * 2017-04-26 Eric Oulashin     Version 0.18 Beta
 *                              Fixed a coloring issue when outputting who voted
 *                              on a poll when more than 1 person voted on it.
 * 2017-08-03 Eric Oulashin     Version 0.19 Beta
 *                              Added a configuration option, useAllAvailableSubBoards,
 *                              which tells SlyVote to use all available sub-boards
 *                              where voting is enabled.  Defaults to true.  Also,
 *                              fixed an off-by-1 bug when displaying the list of
 *                              sub-boards to choose from.  Also, set the (new)
 *                              ampersandHotkeysInItems property in most of the
 *                              DDLightbarMenu objects to false so that it won't
 *                              interpret ampersands before a character in menu
 *                              items as hotkeys.
 * 2017-08-05 Eric Oulashin     Version 0.20 Beta
 *                              Fixed a bug in the stats display where it was assigning
 *                              the total_votes in the headers instead of comparing,
 *                              using an = instead of ==.  Also, updated the stats view
 *                              to display the stats in a scrollable frame rather than
 *                              simply dumping out the stats to the screen.
 * 2017-08-06 Eric Oulashin     Version 0.21 Beta
 *                              Updated the area chooser list to include a check mark
 *                              next to areas that have polls in them.
 * 2017-08-07 Eric Oulashin     Version 0.22 Beta
 *                              Updated to display "Loading areas..." when loading the
 *                              message areas & looking to see which ones have polls.
 *                              Also, changed "topic area" verbage to "voting area".
 * 2017-08-15 Eric Oulashin     Version 0.23 Beta
 *                              Started working on support for multi-answer polls.  Also,
 *                              changed the "topic" verbage to "poll".
 * 2017-08-16 Eric Oulashin     Version 0.24 Beta
 *                              Updated to show the number of choices a user can submit for
 *                              a poll, and restricted the user to choosing only that many.
 *                              Also, started working on adding an option to let the user
 *                              close a poll they created (while viewing poll results).
 * 2017-08-17 Eric Oulashin     Version 0.25 Beta
 *                              Finished the code for closing a poll
 * 2017-08-18 Eric Oulashin     Verison 0.26 Beta
 *                              Made some minor changes, such as the hotkey text for
 *                              submitting a vote in a poll
 * 2017-08-19 Eric Oulashin     Version 0.27 Beta
 *                              Used the new Msgbase.close_poll() method in the August
 *                              19, 2017 build of Synchronet 3.17
 * 2017-08-20 Eric Oulashin     Version 0.28 Beta
 *                              Updated the vote option input for traditional mode
 *                              (when voting from the 'view results' screen) so that
 *                              it only accepts numbers and a comma for input.  Also,
 *                              updated the close-poll behavior so that only the user
 *                              who created the poll can close it (removed the sysop).
 *                              Added a configuration file option, startupSubBoardCode,
 *                              which specifies which sub-board to automatically start
 *                              in if there are multiple sub-board codes configured.
 * 2017-08-27 Eric Oulashin     Version 0.29 beta
 *                              Checked for the user's UFLAG_V restriction (not
 *                              allowed to vote) in more places.  If the user is not
 *                              allowed to vote, they can still view poll results.
 *                              Also, updated the poll viewing & stats view to
 *                              say if a poll is closed.
 * 2017-09-08 Eric Oulashin     Version 0.30 beta
 *                              Implemented a user configuration file to store
 *                              last-read message numbers separately from
 *                              Synchronet's messagebase so that SlyVote won't
 *                              mess with users' actual last-read message numbers.
 *                              Also, updated so that when changing to another
 *                              area, if the top item index is on the last page
 *                              of the menu, then set the top item index to the
 *                              first item on the last page.
 * 2017-09-09 Eric Oulashin     Version 0.31 beta
 *                              Optimization: Updated to only build the area
 *                              selection menu once, the first time it's used.
 *                              Also refactored a section of code there to go
 *                              along with a bug fix in DDLIghtbarMenu.
 * 2017-09-24 Eric Oulashin     Version 0.32 beta
 *                              Bug fix: After deleting the last poll in a sub-board,
 *                              it now correctly goes to the main menu rather than
 *                              reporting an error.
 *                              Also, after creating or deleting a poll, the number
 *                              of polls in the sub-board displayed on the main menu
 *                              is now updated.
 * 2017-12-18 Eric Oulashin     Version 0.33 beta
 *                              Updated the definitions of the KEY_PAGE_UP and
 *                              KEY_PAGE_DOWN veriables to match what they are in
 *                              sbbsdefs.js (if defined) from December 18, 2017 so
 *                              that the PageUp and PageDown keys continue to work
 *                              properly.  This script should still also work with
 *                              older 3.17 builds of Synchronet.
 * 2018-01-28 Eric Oulashin     Version 0.34 beta
 *                              Updated to display avatars for the person who posted
 *                              a poll.
 * 2018-03-19 Eric Oulashin     Version 0.35 beta
 *                              Made a fix for non-lightbar voting input - It wasn't
 *                              accepting Q to quit out of voting (a blank input worked
 *                              though).
 * 2018-03-25 Eric Oulashin     Version 0.36 Beta
 *                              For non-lightbar voting, updated to use BallotHdr (791)
 *                              instead of SelectHdr (501).
 * 2018-04-23 Eric Oulashin     Version 0.37 Beta
 *                              When submitting a vote, the thread_id field is now
 *                              set to the message/poll's message ID, not message number.
 * 2018-12-23 Eric Oulashin     Version 0.38 Beta
 *                              Renamed DDLightbarMenu to dd_lightbar_menu.js
 * 2018-12-29 Eric Oulashin     Version 0.39 Beta
 *                              Made use of file_cfgname() when looking for and
 *                              loading the configuration file.
 * 2019-01-02 Eric Oulashin     Version 1.00
 *                              Changed the version to 1.00 after the official release
 *                              of Synchronet 3.17b
 * 2019-03-21 Eric Oulashin     Version 1.01 Beta
 *                              Changed "voting area" verbage to "sub-board".  Updated
 *                              the main screen to show the number of polls the user
 *                              has voted on & the number remaining.
 *                              Started working on updating to do sub-board selection
 *                              with group selection first.
 * 2019-03-22 Eric Oulashin     Version 1.01
 *                              Releasing this version
 * 2019-04-07 Eric Oulashin     
 *                              Updated to use the new get_index() messagebase function,
 *                              if available, for getting the message index objects
 *                              in subBoardHasPolls().  get_index() is faster than
 *                              iterating through all messages and calling
 *                              get_msg_index() for each message.
 * 2019-04-08 Eric Oulashin     Version 1.02
 *                              Further optimization: When checking a sub-board if it
 *                              has polls, check in reverse rather than forward.  Since
 *                              polls & voting is a relatively recent feature in
 *                              Synchronet, hopefully it should finish faster going
 *                              in reverse.  Also, updated CountPollsInSubBoard() to
 *                              use get_msg_index() if available so that it runs faster.
 * 2019-08-14 Eric Oulashin     Version 1.03
 *                              Made use of require() (if available) to load the .js
 *                              scripts.
 * 2020-02-09 Eric Oulashin     Version 1.04
 *                              Updated calls to AddAdditionalQuitKeys() in the
 *                              DDLightbarMenu class per the updated version
 *                              (requires the dd_lightbar_menu.js update).
 * 2020-03-29 Eric Oulashin     Version 1.05
 *                              Added a null check for the value returned by
 *                              msgbase.get_index() before using the value
 *                              wherever get_index() is called.
 * 2020-03-31 Eric Oulashin     Version 1.06
 *                              Enabled scrollbars in some menus where it would be
 *                              useful.  Requires the latest dd_scrollbar_menu.js.
 * 2020-04-04 Eric Oulashin     Version 1.07
 *                              Made use of the updated DDLightbarMenu, which allows
 *                              replacing the NumItems() and GetItem() functions to
 *                              let the menu use a differnet list of items, to avoid
 *                              adding/copying a bunch of items via DDLightbarMenu's Add()
 *                              function.
 * 2020-05-18 Eric Oulashin     Version 1.08
 *                              Fixed a typo when calling GetKeyWithESCChars()
 * 2020-05-19 Eric Oulashin     Verison 1.09
 *                              Now saves the user's last sub-board to the user config
 *                              file for SlyVote so SlyVote will start in that sub-board
 *                              the next time the user runs SlyVote.
 * 2020-05-22 Eric Oulashin     Version 1.10
 *                              Fixed a bug introduced in 1.07 where the
 *                              useAllAvailableSubBoards 'false' setting was being
 *                              ignored when it was set to false when changing
 *                              to another sub-board.
 * 2021-04-02 Eric Oulashin     Version 1.11
 *                              When configured to use all available sub-boards,
 *                              still don't allow choosing a sub-board that has
 *                              polls disabled.  This fixes an issue where SlyVote
 *                              was showing all available message groups but
 *                              some could be empty due to having no sub-boards
 *                              that allow polls.
 * 2022-06-21 Eric Oulashin     Version 1.12
 *                              Made an update to get vote stats to be displayed again.
 *                              Also, no longer has hard-coded CP437 characters, and
 *                              now uses "use strict" for better runtime checks of proper
 *                              code.
 * 2023-05-13 Eric Oulashin     Version 1.13
 *                              Fix for error when quitting/aborting out of choosing a
 *                              different sub-board. Refactored ReadConfigFile().
 * 2023-09-20 Eric Oulashin     Version 1.14
 *                              Fixed poll voting for single-answer polls
 * 2023-12-10 Eric Oulashin     Version 1.15
 *                              Code refactor; no difference in functionality.
 *                              Now uses js.exec_dir instead of the hack to find out
 *                              what directory the script is running in.
 *                              Now uses user.is_sysop rather than storing a global
 *                              variable with an ARS check for "SYSOP".
 * 2024-11-27 Eric Oulashin     Version 1.16
 *                              When showing a poll result message, for the user who posted the poll,
 *                              show the answers from the people who voted on it. This is to
 *                              basically mimic the fact that Synchronet shows who voted on your
 *                              poll and what they answered, but in the poll message itself.
 */

// TODO: Have a messsage group selection so that it doesn't have to display all
// sub-boards, which can potentially take a long time

"use strict";

const requireFnExists = (typeof(require) === "function");

if (requireFnExists)
{
	require("sbbsdefs.js", "K_UPPER");
	require("cp437_defs.js", "CP437_BOX_DRAWINGS_UPPER_LEFT_SINGLE");
}
else
{
	load("sbbsdefs.js");
	load("cp437_defs.js");
}

// This script requires Synchronet version 3.17 or higher.
// Exit if the Synchronet version is below the minimum.
if (system.version_num < 31700)
{
	var message = "\x01n\x01h\x01y\x01i* Warning:\x01n\x01h\x01w SlyVote requires version "
	             + "\x01g3.17\x01w or\r\nhigher of Synchronet.  This BBS is using "
	             + "version \x01g" + system.version + "\x01w.  Please notify the sysop.";
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
	console.print("\x01n\x01hSlyVote requires an ANSI client.");
	console.crlf();
	console.pause();
	exit();
}

// If the user is not allowed to vote (has the V restriction), show the appropriate
// error message and that the user can still view poll results.
load("text.js"); // For text.dat line definitions
if ((user.security.restrictions & UFLAG_V) == UFLAG_V)
{
	console.crlf();
	console.crlf();
	// Use the line from text.dat that says the user is not allowed to vote
	console.print("\x01n" + RemoveCRLFCodes(processBBSTextDatText(bbs.text(R_Voting))) + "\x01n");
	console.crlf();
	console.print("\x01cYou can still view poll results.\x01n");
	console.crlf();
	console.pause();
}

if (requireFnExists)
{
	require("frame.js", "Frame");
	require("scrollbar.js", "ScrollBar");
	require("dd_lightbar_menu.js", "DDLightbarMenu");
	require("smbdefs.js", "SMB_POLL_ANSWER");
}
else
{
	load("frame.js");
	load("scrollbar.js");
	load("dd_lightbar_menu.js");
	load("smbdefs.js");
}
// For avatar support
var gAvatar = load({}, "avatar_lib.js");

// Version information
var SLYVOTE_VERSION = "1.16";
var SLYVOTE_DATE = "2024-11-27";

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
};
var gAuxMsgAttrStrs = {
	MSG_FILEREQUEST: "Freq",
	MSG_FILEATTACH: "Attach",
	MSG_TRUNCFILE: "TruncFile",
	MSG_KILLFILE: "KillFile",
	MSG_RECEIPTREQ: "RctReq",
	MSG_CONFIRMREQ: "ConfReq",
	MSG_NODISP: "NoDisp"
};
var gNetMsgAttrStrs = {
	NETMSG_LOCAL: "FromLocal",
	NETMSG_INTRANSIT: "Transit",
	NETMSG_SENT: "Sent",
	NETMSG_KILLSENT: "KillSent",
	NETMSG_HOLD: "Hold",
	NETMSG_CRASH: "Crash",
	NETMSG_IMMEDIATE: "Now",
	NETMSG_DIRECT: "Direct"
};

// An amount of milliseconds to wait after displaying an error message, etc.
var ERROR_PAUSE_WAIT_MS = 1500;

var gBottomBorderRow = 23;
var gMessageRow = 3;

// Keyboard key codes for displaying on the screen
var UP_ARROW = ascii(24);
var DOWN_ARROW = ascii(25);
var LEFT_ARROW = ascii(17);
var RIGHT_ARROW = ascii(16);
// PageUp & PageDown keys - Synchronet 3.17 as of about December 18, 2017
// use CTRL-P and CTRL-N for PageUp and PageDown, respectively.  sbbsdefs.js
// defines them as KEY_PAGEUP and KEY_PAGEDN; I've used slightly different names
// in this script so that this script will work with Synchronet systems before
// and after the update containing those key definitions.
var KEY_PAGE_UP = "\x10"; // Ctrl-P
var KEY_PAGE_DOWN = "\x0e"; // Ctrl-N
// Ensure KEY_PAGE_UP and KEY_PAGE_DOWN are set to what's defined in sbbs.js
// for KEY_PAGEUP and KEY_PAGEDN in case they change
if (typeof(KEY_PAGEUP) === "string")
	KEY_PAGE_UP = KEY_PAGEUP;
if (typeof(KEY_PAGEDN) === "string")
	KEY_PAGE_DOWN = KEY_PAGEDN;

// An object containing keypresses for the vote results reader
var gReaderKeys = {
	goToFirst: "F",
	goToLast: "L",
	vote: "V",
	close: "C",
	validateMsg: "A", // Only for the sysop
	quit: "Q"
};


var gSlyVoteCfg = ReadConfigFile();
if (gSlyVoteCfg.cfgReadError.length > 0)
{
	log(LOG_ERR, "SlyVote: Error reading slyvote.cfg");
	bbs.log_str("SlyVote: Error reading slyvote.cfg");
	console.attributes = "N";
	console.crlf();
	console.print("\x01h\x01y* Error reading slyvote.cfg\x01n");
	console.crlf();
	console.pause();
	exit();
}
// Ensure there are sub-boards configured
var subBoardsConfigured = false;
if (Object.keys(gSlyVoteCfg.msgGroups).length > 0)
{
	for (var grpName in gSlyVoteCfg.msgGroups)
	{
		if (gSlyVoteCfg.msgGroups[grpName].length > 0)
		{
			subBoardsConfigured = true;
			break;
		}
	}
}
if (!subBoardsConfigured)
{
	console.attributes = "N";
	console.crlf();
	console.print("\x01cThere are no sub-boards configured.\x01n");
	console.crlf();
	console.pause();
	exit();
}


// Read the user settings file
// The name of the user's settings file for SlyVote
var gUserSettingsFilename = system.data_dir + "user/" + format("%04d", user.number) + ".slyvote.cfg";
var gUserSettings = ReadUserSettingsFile(gUserSettingsFilename, gSlyVoteCfg);

// Determine which sub-board to use - Prioritize the last sub-board in the user
// config file if there is one.  For the SlyVote config, if there is more than
// one, let the user choose.
var gSubBoardCode = "";
if (gUserSettings.lastSubCode.length > 0)
	gSubBoardCode = gUserSettings.lastSubCode;
else if (gSlyVoteCfg.numSubBoards == 1)
{
	for (var grpIdx in gSlyVoteCfg.msgGroups)
	{
		gSubBoardCode = gSlyVoteCfg.msgGroups[grpIdx][0];
		break;
	}
}
else
{
	// If the startup sub-board code is set, then automatically start
	// in that sub-board.
	if (gSlyVoteCfg.startupSubBoardCode.length > 0)
		gSubBoardCode = gSlyVoteCfg.startupSubBoardCode;
	// If the last sub-board from the user's last run of SlyVote is not blank,
	// then use it.
	else if (gUserSettings.lastSubCode.length > 0)
		gSubBoardCode = gUserSettings.lastSubCode;
	else // Let the user choose a sub-board
	{
		var chooseSubRetObj = ChooseVotingSubBoard(gSlyVoteCfg.msgGroups);
		if (typeof(chooseSubRetObj.subBoardChoice) === "string" && chooseSubRetObj.subBoardChoice.length > 0 && msg_area.sub.hasOwnProperty(chooseSubRetObj.subBoardChoice))
		{
			gSubBoardCode = chooseSubRetObj.subBoardChoice;
			console.gotoxy(1, chooseSubRetObj.menuPos.y + chooseSubRetObj.menuSize.height + 1);
		}
		// Exit if the user pressed ESC rather than choosing an area
		else
			exit(0);
	}
}
// Output a "loading..." text, in case it takes a while to count the polls in
// the current sub-board
console.attributes = "N";
console.crlf();
var subBoardName = msg_area.sub[gSubBoardCode].grp_name + ": " + msg_area.sub[gSubBoardCode].description;
console.print("\x01gLoading SlyVote (counting polls in \x01c" + subBoardName + "\x01g)...\x01n");
console.line_counter = 0;
var gSubBoardPollCountObj = CountPollsInSubBoard(gSubBoardCode);

// Program states
var MAIN_MENU = 0;
var VOTING_ON_A_POLL = 1;
var VOTE_ON_ALL_POLLS = 2;
var VIEWING_VOTE_RESULTS = 3;
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
		case VOTING_ON_A_POLL:
			gProgramState = ChooseVotePoll(true);
			break;
		case VOTE_ON_ALL_POLLS:
			gProgramState = ChooseVotePoll(false);
			break;
		case VIEWING_VOTE_RESULTS:
			gProgramState = ViewVoteResults(gSubBoardCode);
			break;
		case EXIT_SLYVOTE:
			gContinueSlyVote = false;
			break;
	}
}

// Before exiting, update the user settings file
if (!WriteUserSettingsFile(gUserSettings, gUserSettingsFilename))
{
	console.print("\x01n\x01y\x01hFailed to update your user settings file!\x01n");
	mswait(ERROR_PAUSE_WAIT_MS);
}




////////////////////////////////////////////////////////////
// Helper functions
////////////////////////////////////////////////////////////

// Lets the user choose a voting sub-board when there are multiple sub-boards
// configured.
//
// Parameters:
//  pMsgGrps: An object of message group indexes, where each item has an
//            array of sub-board codes
//
// Return value: An object containing the following properties:
//               subBoardChoice: The user's choice of sub-board (internal code), or null if the user aborted.
//               menuPos: The menu position
//               menuSize: The menu size
function ChooseVotingSubBoard(pMsgGrps)
{
	// Clear the screen & display the SlyVote stylized text
	console.clear("\x01n");
	DisplaySlyVoteText();

	// Draw columns to frame the voting menu
	var listTopRow = 8;
	var drawColRetObj = DrawVoteColumns(listTopRow, gBottomBorderRow-1, 17, 63);

	// If the message group and sub-board menus haven't been created yet, then create them
	if (typeof(ChooseVotingSubBoard.grpMenu) != "object")
		ChooseVotingSubBoard.grpMenu = CreateMsgGrpMenu(listTopRow, drawColRetObj, pMsgGrps);

	var chosenSubBoard = null;
	// If there is only one message group, then just let the user choose sub-boards
	// within that message group.  Otherwise, display the group menu and let the user
	// choose a message group and then a sub-board.
	var chosenGrpIdx = -1;
	var continueChoosingSubBoard = true;
	while (continueChoosingSubBoard)
	{
		if (ChooseVotingSubBoard.grpMenu.items.length == 1)
			chosenGrpIdx = ChooseVotingSubBoard.grpMenu.items[0].retval;
		else
		{
			// Display the "choose a group" text
			console.gotoxy(drawColRetObj.columnX1+10, listTopRow-1);
			console.print("\x01n\x01b\x01hChoose a group (\x01cESC\x01g/\x01cQ\x01g=\x01n\x01cExit\x01h\x01b)\x01n");
			chosenGrpIdx = ChooseVotingSubBoard.grpMenu.GetVal();
			// Note: If the user quits out of the menu without making a selection,
			// chosenGrpIdx will be null.
		}
		// Sub-board selection within a message group
		if (typeof(ChooseVotingSubBoard.subBoardMenus) != "object")
			ChooseVotingSubBoard.subBoardMenus = {};
		if ((chosenGrpIdx != null) && (chosenGrpIdx > -1))
		{
			// If the sub-board menu for the chosen group name doesn't exist, then create it
			if (!ChooseVotingSubBoard.subBoardMenus.hasOwnProperty(chosenGrpIdx))
			{
				// In case it takes a while to load the sub-board information, display
				// some text saying we're loading the sub-boards
				console.gotoxy(drawColRetObj.columnX1+2, listTopRow);
				console.print("\x01n\x01cLoading sub-boards\x01i...\x01n"); // In case loading sub-boards takes a while
				ChooseVotingSubBoard.subBoardMenus[chosenGrpIdx] = CreateSubBoardMenu(chosenGrpIdx, listTopRow, drawColRetObj, pMsgGrps);
			}

			// Display the message group
			var msgGrpText = "\x01n\x01b\x01hGroup: \x01w" + msg_area.grp_list[chosenGrpIdx].name + "\x01n";
			var txtDisplayLen = strip_ctrl(msgGrpText).length;
			var textX = (console.screen_columns/2) - (txtDisplayLen/2);
			console.gotoxy(textX, listTopRow-2);
			console.print(msgGrpText);
			// Display the "choose a sub-board" text
			console.gotoxy(drawColRetObj.columnX1+2, listTopRow-1);
			var chooseASubBoardText = "\x01n\x01b\x01hChoose a sub-board (\x01cESC\x01g/\x01cQ\x01g=\x01n\x01cExit\x01h\x01b)  \x01y\x01h" + CP437_CHECK_MARK + "\x01n\x01c=\x01b\x01hHas polls\x01n";
			console.print(chooseASubBoardText);
			// Let the user choose a sub-board
			chosenSubBoard = ChooseVotingSubBoard.subBoardMenus[chosenGrpIdx].GetVal();
			continueChoosingSubBoard = (chosenSubBoard == null) && (ChooseVotingSubBoard.grpMenu.items.length > 1);

			// If we will go back to the message group menu, then erase the message group
			// text and "choose a sub-board" text
			if (continueChoosingSubBoard)
			{
				// Erase the message group text
				textX = (console.screen_columns/2) - (txtDisplayLen/2);
				console.gotoxy(textX, listTopRow-2);
				console.print(format("\x01n%-" + txtDisplayLen + "s", ""));
				txtDisplayLen = strip_ctrl(chooseASubBoardText).length;
				// Erase the "choose a sub-board" text
				console.gotoxy(drawColRetObj.columnX1+2, listTopRow-1);
				console.print(format("\x01n%-" + txtDisplayLen + "s", ""));
			}
		}
		else
			continueChoosingSubBoard = false;
	}

	// Return an object with useful values
	return {
		subBoardChoice: (chosenSubBoard == null ? "" : chosenSubBoard),
		menuPos: ChooseVotingSubBoard.grpMenu.pos,
		menuSize: ChooseVotingSubBoard.grpMenu.size
	};
}
// Helper for ChooseVotingSubBoard().  Creates the message group menu.
//
// Parameters:
//  pListTopRow: The top row on the screen for the menu
//  pDrawColRetObj: The return object from DrawVoteColumns()
//  pMsgGrps: An object of message group indexes, where each item has an
//            array of sub-board codes
//
// Return value: A DDLightbarMenu object for the message group menu
function CreateMsgGrpMenu(pListTopRow, pDrawColRetObj, pMsgGrps)
{
	var grpMenu = new DDLightbarMenu(pDrawColRetObj.columnX1+pDrawColRetObj.colWidth-1, pListTopRow, pDrawColRetObj.textLen, pDrawColRetObj.colHeight);
	grpMenu.ampersandHotkeysInItems = false;
	grpMenu.scrollbarEnabled = true;
	grpMenu.AddAdditionalQuitKeys("qQ");
	var grpNameLen = pDrawColRetObj.textLen - 2;
	// Count the number of groups.  If it's more than the number per page in
	// the menu, then subtract 1 from grpNameLen to account for the scrollbar.
	var numGrps = 0;
	for (var grpIdx in pMsgGrps)
		++numGrps;
	if (numGrps > grpMenu.GetNumItemsPerPage())
		--grpNameLen;
	// Add the groups to the menu
	for (var grpIdx in pMsgGrps)
	{
		var grpName = msg_area.grp_list[grpIdx].name;
		var itemText = format("%-" + grpNameLen + "s", grpName.substr(0, grpNameLen));
		grpMenu.Add(itemText, grpIdx);
	}
	return grpMenu;
}
// Helper for ChooseVotingSubBoard().  Creates a sub-board menu.
//
// Parameters:
//  pGrpIdx: The index of the message group
//  pListTopRow: The top row on the screen for the menu
//  pDrawColRetObj: The return object from DrawVoteColumns()
//  pMsgGrps: An object of message group indexes, where each item has an
//            array of sub-board codes
//
// Return value: A DDLightbarMenu object for the message group menu
function CreateSubBoardMenu(pGrpIdx, pListTopRow, pDrawColRetObj, pMsgGrps)
{
	var topItemIndex = 0;
	var selectedItemIndex = 0;
	// Populate the sub-board menu for the group with its list of sub-boards
	var areaNameLen = pDrawColRetObj.textLen - 3;
	var subBoardMenu = new DDLightbarMenu(pDrawColRetObj.columnX1+pDrawColRetObj.colWidth-1, pListTopRow, pDrawColRetObj.textLen, pDrawColRetObj.colHeight);
	subBoardMenu.areaNameLen = areaNameLen;
	subBoardMenu.ampersandHotkeysInItems = false;
	subBoardMenu.scrollbarEnabled = true;
	subBoardMenu.AddAdditionalQuitKeys("qQ");
	subBoardMenu.msgGrp = pMsgGrps[pGrpIdx];
	subBoardMenu.numSubBoardsInChosenMsgGrp = pMsgGrps[pGrpIdx].length;
	subBoardMenu.subBoardCode = pMsgGrps[pGrpIdx][i];
	subBoardMenu.subBoardHasPolls = subBoardHasPolls;
	subBoardMenu.subBoardPollCounts = {};
	if (gSlyVoteCfg.useAllAvailableSubBoards)
	{
		subBoardMenu.NumItems = function() {
			return this.numSubBoardsInChosenMsgGrp;
		};
		subBoardMenu.GetItem = function(pItemIndex) {
			var subCode = this.msgGrp[pItemIndex];
			var pollCount = 0;
			if (this.subBoardPollCounts.hasOwnProperty(subCode))
				pollCount = this.subBoardPollCounts[subCode];
			else
			{
				pollCount = this.subBoardHasPolls(subCode);
				this.subBoardPollCounts[subCode] = pollCount;
			}
			var menuItemObj = this.MakeItemWithRetval(-1);
			var hasPollsChar = (pollCount ? "\x01y\x01h" + CP437_CHECK_MARK + "\x01n" : " ");
			menuItemObj.text = format("%-" + this.areaNameLen + "s %s", msg_area.sub[subCode].name.substr(0, this.areaNameLen), hasPollsChar);
			menuItemObj.retval = subCode;
			return menuItemObj;
		};
	}
	else
	{
		areaNameLen = pDrawColRetObj.textLen - 2;
		if (pMsgGrps[pGrpIdx].length > subBoardMenu.GetNumItemsPerPage())
			--areaNameLen;
		for (var i = 0; i < pMsgGrps[pGrpIdx].length; ++i)
		{
			var subCode = pMsgGrps[pGrpIdx][i];
			var hasPollsChar = (subBoardHasPolls(subCode) ? "\x01y\x01h" + CP437_CHECK_MARK + "\x01n" : " ");
			var itemText = format("%-" + areaNameLen + "s %s", msg_area.sub[subCode].name.substr(0, areaNameLen), hasPollsChar);
			subBoardMenu.Add(itemText, subCode);
			if (subCode == gSubBoardCode)
			{
				topItemIndex = i;
				selectedItemIndex = i;
			}
		}
	}

	for (var i = 0; i < pMsgGrps[pGrpIdx].length; ++i)
	{
		var subCode = pMsgGrps[pGrpIdx][i];
		if (subCode == gSubBoardCode)
		{
			topItemIndex = i;
			selectedItemIndex = i;
		}
	}
	// If the top item index is on the last page of the menu, then
	// set the top item index to the first item on the last page.
	if ((topItemIndex <= subBoardMenu.items.length - 1) && (topItemIndex >= subBoardMenu.GetTopItemIdxOfLastPage()))
		subBoardMenu.SetTopItemIdxToTopOfLastPage();
	else
		subBoardMenu.topItemIdx = topItemIndex;
	subBoardMenu.selectedItemIdx = selectedItemIndex;

	return subBoardMenu;
}

var gMainMenu = null;
function DoMainMenu()
{
	gProgramState = MAIN_MENU;
	var nextProgramState = MAIN_MENU;

	// Menu option return values
	var voteOnPollOpt = 0;
	var answerAllPollsOpt = 1;
	var createPollOpt = 2;
	var viewResultsOpt = 3;
	var viewStatsOpt = 4;
	var changeSubBoardOpt = 5;
	var quitToBBSOpt = 6;

	// Display the SlyVote screen and menu of choices
	console.clear("\x01n");
	var mainScrRetObj = DisplaySlyVoteMainVoteScreen(false);
	if (gMainMenu == null)
	{
		var mainMenuHeight = 6;
		if (gSlyVoteCfg.numSubBoards > 1)
			++mainMenuHeight;
		gMainMenu = new DDLightbarMenu(mainScrRetObj.optMenuX, mainScrRetObj.optMenuY, 17, mainMenuHeight);
		gMainMenu.hotkeyCaseSensitive = false;
		gMainMenu.Add("&Vote On A Poll", voteOnPollOpt, "1");
		gMainMenu.Add("&Answer All Polls", answerAllPollsOpt, "2");
		gMainMenu.Add("&Create A Poll", createPollOpt, "3");
		gMainMenu.Add("View &Results", viewResultsOpt, "4");
		gMainMenu.Add("View &Stats", viewStatsOpt, "5");
		if (gSlyVoteCfg.numSubBoards > 1)
		{
			gMainMenu.Add("Change Sub-Board", changeSubBoardOpt, "6");
			gMainMenu.Add("&Quit To BBS", quitToBBSOpt, "7");
		}
		else
			gMainMenu.Add("&Quit To BBS", quitToBBSOpt, "6");
		gMainMenu.colors.itemColor = "\x01n\x01w";
		gMainMenu.colors.selectedItemColor = "\x01n\x01" + "4\x01w\x01h";
	}
	// Get the user's choice and take appropriate action
	var userChoice = gMainMenu.GetVal(true);
	if (userChoice == voteOnPollOpt)
	{
		if (msg_area.sub[gSubBoardCode].can_post)
			nextProgramState = VOTING_ON_A_POLL;
		else
			DisplayErrorWithPause("\x01y\x01h" + RemoveCRLFCodes(processBBSTextDatText(bbs.text(CantPostOnSub))), gMessageRow, false);
	}
	else if (userChoice == answerAllPollsOpt)
	{
		if (msg_area.sub[gSubBoardCode].can_post)
			nextProgramState = VOTE_ON_ALL_POLLS;
		else
			DisplayErrorWithPause("\x01y\x01h" + RemoveCRLFCodes(processBBSTextDatText(bbs.text(CantPostOnSub))), gMessageRow, false);
	}
	else if (userChoice == createPollOpt)
	{
		// Only let the user create a poll if the user is allowed to post in
		// the sub-board.
		if (msg_area.sub[gSubBoardCode].can_post)
		{
			// Set the user's current sub-board to the chosen sub-board before
			// posting a poll
			var curSubCodeBackup = bbs.cursub_code;
			bbs.cursub_code = gSubBoardCode;
			// Let the user post a poll
			console.attributes = "N";
			console.gotoxy(1, console.screen_rows);
			bbs.exec("?postpoll.js");
			// Restore the user's sub-board
			bbs.cursub_code = curSubCodeBackup;
			// Assuming the poll was posted successfully, update the poll count
			// in the current sub-board.
			gSubBoardPollCountObj = CountPollsInSubBoard(gSubBoardCode);
		}
		else
			DisplayErrorWithPause("\x01y\x01h" + RemoveCRLFCodes(processBBSTextDatText(bbs.text(CantPostOnSub))), gMessageRow, false);
	}
	else if (userChoice == viewResultsOpt)
	{
		if (msg_area.sub[gSubBoardCode].can_read)
			nextProgramState = VIEWING_VOTE_RESULTS;
		else
		{
			var errorMsg = format(RemoveCRLFCodes(processBBSTextDatText(bbs.text(CantReadSub))), msg_area.sub[gSubBoardCode].grp_name, msg_area.sub[gSubBoardCode].name);
			if (errorMsg.substr(0, 2) == "\x01n")
				errorMsg = errorMsg.substr(2);
			DisplayErrorWithPause("\x01y\x01h" + errorMsg, gMessageRow, false);
		}
	}
	else if (userChoice == viewStatsOpt)
	{
		ViewStats(gSubBoardCode);
		nextProgramState = MAIN_MENU;
	}
	else if (userChoice == changeSubBoardOpt)
	{
		var chooseSubRetObj = ChooseVotingSubBoard(gSlyVoteCfg.msgGroups);
		var chosenSubBoardCode = chooseSubRetObj.subBoardChoice;
		// If the user chose a sub-board, then set gSubBoardCode.
		if (typeof(chosenSubBoardCode) === "string" && chosenSubBoardCode.length > 0 && msg_area.sub.hasOwnProperty(chosenSubBoardCode))
		{
			gSubBoardCode = chosenSubBoardCode;
			gSubBoardPollCountObj = CountPollsInSubBoard(gSubBoardCode);
		}
	}
	else if ((userChoice == quitToBBSOpt) || (userChoice == null))
		nextProgramState = EXIT_SLYVOTE;

	return nextProgramState;
}

// Lets the user choose a poll to vote on.
//
// Parameters:
//  pLetUserChoose: Boolean - Whether or not to let the user choose a poll.  Defaults
//                  to true.  If false, then all polls will be presented in order.
function ChooseVotePoll(pLetUserChoose)
{
	gProgramState = VOTING_ON_A_POLL;
	var nextProgramState = VOTING_ON_A_POLL;
	// Clear the screen between the top & bottom borders
	var formatStr = "%" + console.screen_columns + "s";
	console.attributes = "N";
	for (var posY = gMessageRow; posY < gBottomBorderRow; ++posY)
	{
		console.gotoxy(1, posY);
		printf(formatStr, "");
	}

	// Get the list of open polls for the selected sub-board
	var votePollInfo = GetPollHdrs(gSubBoardCode, true, true);
	if (votePollInfo.errorMsg.length > 0)
	{
		console.gotoxy(1, gMessageRow);
		console.print("\x01n\x01y\x01h" + votePollInfo.errorMsg + "\x01n");
		console.crlf();
		console.pause();
		return MAIN_MENU;
	}
	else if (votePollInfo.msgHdrs.length == 0)
	{
		console.gotoxy(1, gMessageRow);
		console.print("\x01n\x01g");
		if (votePollInfo.pollsExist)
			console.print("You have already voted on all polls in this sub-board, or all polls are closed");
		else
			console.print("There are no polls to vote on in this sub-board");
		console.attributes = "N";
		console.crlf();
		console.pause();
		return MAIN_MENU;
	}
	// If the user isn't allowed to vote, then print the appropriate error and return to the main menu.
	if ((user.security.restrictions & UFLAG_V) == UFLAG_V)
	{
		console.gotoxy(1, gMessageRow);
		console.print("\x01n" + RemoveCRLFCodes(processBBSTextDatText(bbs.text(R_Voting))) + "\x01n");
		console.crlf();
		console.pause();
		return MAIN_MENU;
	}

	// Draw the columns to frame the voting polls
	console.attributes = "N";
	var pleaseSelectTextRow = 6;
	var listTopRow = pleaseSelectTextRow + 2;
	var drawColRetObj = DrawVoteColumns(listTopRow);

	//var startCol = drawColRetObj.columnX1+2;
	var startCol = drawColRetObj.columnX1+drawColRetObj.colWidth-1;
	var menuHeight = gBottomBorderRow-listTopRow;

	// If we are to let the user choose a poll, then display the list of voting polls
	var letUserChoose = (typeof(pLetUserChoose) == "boolean" ? pLetUserChoose : true);
	if (letUserChoose)
	{
		// Create the menu containing voting polls and get the user's choic
		var pollsMenu = new DDLightbarMenu(startCol, listTopRow, drawColRetObj.textLen, menuHeight);
		pollsMenu.ampersandHotkeysInItems = false;
		pollsMenu.scrollbarEnabled = true;
		pollsMenu.votePollInfo = votePollInfo;
		pollsMenu.NumItems = function() {
			return this.votePollInfo.msgHdrs.length;
		};
		pollsMenu.GetItem = function(pItemIndex) {
			var menuItemObj = this.MakeItemWithRetval(-1);
			menuItemObj.text = votePollInfo.msgHdrs[pItemIndex].subject;
			menuItemObj.retval = votePollInfo.msgHdrs[pItemIndex].number;
			return menuItemObj;
		};
		var drawPollsMenu = true;
		while (nextProgramState == VOTING_ON_A_POLL)
		{
			var pleaseSectPollText = "\x01n\x01c\x01hP\x01n\x01clease select a poll to vote on (\x01hESC\x01n\x01g=\x01cReturn)\x01n";
			console.gotoxy(18, pleaseSelectTextRow);
			console.print(pleaseSectPollText);
			console.attributes = "N";
			var chosenMsgNum = pollsMenu.GetVal(drawPollsMenu);
			if (chosenMsgNum != null)
			{
				console.gotoxy(18, pleaseSelectTextRow);
				printf("%" + strip_ctrl(pleaseSectPollText).length + "s", "");
				var voteRetObj = DisplayPollOptionsAndVote(gSubBoardCode, chosenMsgNum, startCol, listTopRow, drawColRetObj.textLen, menuHeight);
				drawPollsMenu = true;
				if (voteRetObj.errorMsg.length > 0)
					DisplayErrorWithPause(voteRetObj.errorMsg, gMessageRow, voteRetObj.mnemonicsRequiredForErrorMsg);
			}
			else // The user chose to exit the polls menu
				nextProgramState = MAIN_MENU;
		}
	}
	else
	{
		// Vote on all polls
		var pollNumTextLen = 0; // To clear the poll # text later
		nextProgramState = MAIN_MENU;
		for (var i = 0; i < votePollInfo.msgHdrs.length; ++i)
		{
			// Display the poll number
			//var pollNumText = format("\x01n\x01c%3d of %-3d", +(i+1), votePollInfo.msgHdrs.length);
			var pollNumText = format("\x01n\x01c%3d/%-3d", +(i+1), votePollInfo.msgHdrs.length);
			console.gotoxy(1, console.screen_rows-4);
			console.print(pollNumText);
			pollNumTextLen = console.strlen(pollNumText); //strip_ctrl(pollNumText).length;
			// Let the user vote on the poll
			var voteRetObj = DisplayPollOptionsAndVote(gSubBoardCode, votePollInfo.msgHdrs[i].number, startCol, listTopRow, drawColRetObj.textLen, menuHeight);
			if (voteRetObj.userExited)
				break;
			if (voteRetObj.errorMsg.length > 0)
			{
				DisplayErrorWithPause(voteRetObj.errorMsg, gMessageRow, voteRetObj.mnemonicsRequiredForErrorMsg);
				console.gotoxy(1, console.screen_rows-4);
				printf("\x01n%" + pollNumTextLen + "s", "");
				break;
			}
		}
		// Erase the poll number text
		console.gotoxy(1, console.screen_rows-4);
		printf("\x01n%" + pollNumTextLen + "s", "");
	}

	return nextProgramState;
}

// Displays options for a poll and lets the user vote on it
//
// Parameters:
//  pSubBoardCode: The internal code of the sub-board
//  pMsgNum: The number of the message to vote on
//  pStartCol: The starting column on the screen for the option list
//  pStartRow: The starting row on the screen for the option list
//  pMenuWidth: The width to use for the poll option menu
//  pMenuHeight: The height to use for the poll option menu
//
// Return value: An object containing the following properties:
//               userExited: Boolean - Whether or not the user exited without voting
//               errorMsg: A string containing a message on error, or an empty string on success
//               mnemonicsRequiredForErrorMsg: Whether or not mnemonics is required to display the error message
function DisplayPollOptionsAndVote(pSubBoardCode, pMsgNum, pStartCol, pStartRow, pMenuWidth, pMenuHeight)
{
	var retObj = {
		userExited: false,
		errorMsg: "",
		mnemonicsRequiredForErrorMsg: false,
		nextProgramState: VOTING_ON_A_POLL
	};

	// Open the chosen sub-board
	var msgbase = new MsgBase(pSubBoardCode);
	if (msgbase.open())
	{
		if (!HasUserVotedOnMsg(pMsgNum, pSubBoardCode, msgbase, user))
		{
			// Display the ESC=Quit text
			var ESCQuitX = 1;
			//var ESCQuitY = console.screen_rows-3;
			//var ESCQuitX = 73;
			//var ESCQuitY = console.screen_rows-17;
			var ESCQuitY = console.screen_rows-10;
			console.gotoxy(ESCQuitX, ESCQuitY);
			console.print("\x01n\x01c\x01hESC\x01g=\x01n\x01cQuit\x01n");

			// Get the poll options and let the user choose one
			var msgHdr = msgbase.get_msg_header(false, pMsgNum, true);
			if ((msgHdr != null) && IsReadableMsgHdr(msgHdr, pSubBoardCode))
			{
				var pollTextAndOpts = GetPollTextAndOpts(msgHdr);
				// Print the poll question text centered on the screen
				var pollSubject = msgHdr.subject.substr(0, console.screen_columns);
				var pollSubjectStartCol = (console.screen_columns / 2) - (pollSubject.length / 2);
				var subjectRow = pStartRow-5;
				console.gotoxy(pollSubjectStartCol, subjectRow);
				console.print("\x01n\x01g\x01h" + pollSubject + "\x01n");
				// Write the maximum number of choices possible
				var numChoicesRow = subjectRow+1;
				var numChoicesPossibleText = "";
				if (msgHdr.votes > 1)
					numChoicesPossibleText = format("\x01n\x01gYou can submit up to \x01h%d\x01n\x01g choices. Spacebar=Choose, Enter=Submit", msgHdr.votes);
				else
					numChoicesPossibleText = format("\x01n\x01gYou can submit up to \x01h%d\x01n\x01g choice. Enter=Submit", msgHdr.votes);
				numChoicesPossibleText += ", ESC=Quit\x01n";
				var numChoicesPossibleTextLen = strip_ctrl(numChoicesPossibleText).length;
				var numChoicesCol = (console.screen_columns / 2) - (numChoicesPossibleTextLen / 2);
				console.gotoxy(numChoicesCol, numChoicesRow);
				console.print(numChoicesPossibleText);
				// Output up to the first 3 poll comment lines
				//console.attributes = "N";
				var i = 0;
				var commentStartRow = pStartRow - 3;
				for (var row = commentStartRow; (row < commentStartRow+3) && (i < pollTextAndOpts.commentLines.length); ++row)
				{
					console.gotoxy(1, row);
					console.print(pollTextAndOpts.commentLines[i++].substr(0, console.screen_columns));
				}
				// Create the poll options menu, and show it and let the user choose an option
				var optionsMenu = new DDLightbarMenu(pStartCol, pStartRow, pMenuWidth, pMenuHeight);
				optionsMenu.ampersandHotkeysInItems = false;
				optionsMenu.scrollbarEnabled = true;
				// If the poll allows multiple answers, enable the multi-choice
				// property for the menu.
				if (msgHdr.votes > 1)
				{
					optionsMenu.multiSelect = true;
					optionsMenu.maxNumSelections = msgHdr.votes;
				}
				else
					optionsMenu.multiSelect = false;
				for (i = 0; i < pollTextAndOpts.options.length; ++i)
					optionsMenu.Add(pollTextAndOpts.options[i], i+1);
				optionsMenu.colors.itemColor = "\x01c";
				optionsMenu.colors.selectedItemColor = "\x01b\x01" + "7";
				// Get the user's choice(s) on the poll
				var userChoice = optionsMenu.GetVal(true);
				// Erase the poll subject text so it doesn't look weird when the
				// success/error message is displayed
				console.gotoxy(pollSubjectStartCol, subjectRow);
				printf("%" + strip_ctrl(pollSubject).length + "s", "");
				// Submit the user's choice if they chose something
				var firstLineEraseLength = 0;
				if (userChoice != null)
				{
					// Allow multiple choices for userChoice, if it's a multi-choice poll.
					var voteRetObj;
					if (optionsMenu.multiSelect)
					{
						var userVotes = 0;
						for (var i = 0; i < userChoice.length; ++i)
							userVotes |= (1 << (userChoice[i]-1));
						voteRetObj = VoteOnPoll(pSubBoardCode, msgbase, msgHdr, user, userVotes, true);
					}
					else
						voteRetObj = VoteOnPoll(pSubBoardCode, msgbase, msgHdr, user, (1 << (userChoice-1)), true);
					// If there was an error, then show it.  Otherwise, show a success message.
					//var firstLineEraseLength = pollSubject.length;
					console.gotoxy(1, subjectRow);
					printf("\x01n%" + pollSubject.length + "s", "");
					console.gotoxy(1, subjectRow);
					if (voteRetObj.errorMsg.length > 0)
					{
						var voteErrMsg = voteRetObj.errorMsg.substr(0, console.screen_columns - 2);
						firstLineEraseLength = voteErrMsg.length;
						console.print("\x01y\x01h* " + voteErrMsg);
					}
					else
					{
						console.print("\x01gYour vote was successfully saved.");
						firstLineEraseLength = 33;
						IncrementNumPollsVotedForUser();
					}
					mswait(ERROR_PAUSE_WAIT_MS);
				}
				else
					retObj.userExited = true;

				// Before returning, erase the comment lines from the screen
				console.attributes = "N";
				console.gotoxy(1, subjectRow);
				printf("%" + firstLineEraseLength + "s", "");
				console.gotoxy(numChoicesCol, numChoicesRow);
				printf("%" + numChoicesPossibleTextLen + "s", "");
				i = 0;
				for (var row = commentStartRow; (row < commentStartRow+3) && (i < pollTextAndOpts.commentLines.length); ++row)
				{
					console.gotoxy(1, row);
					var pollCommentLine = pollTextAndOpts.commentLines[i++].substr(0, console.screen_columns);
					printf("%" + pollCommentLine.length + "s", "");
				}
			}
			else
				retObj.errorMsg = "Unable to retrieve the poll options";

			// Erase the ESC=Quit text
			console.gotoxy(ESCQuitX, ESCQuitY);
			printf("\x01n%8s", "");
		}
		else
		{
			// The user has already voted
			retObj.errorMsg = processBBSTextDatText(bbs.text(VotedAlready));
			retObj.errorMsg = RemoveCRLFCodes(retObj.errorMsg);
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

// Reads the configuration file (slyvote.cfg).  Returns an object with the following
// properties:
//  cfgReadError: A string which will contain a message if failed to read the configuration file
//  useAllAvailableSubBoards: Boolean - Whether or not to use all available sub-boards where
//                            voting is allowed
//  msgGroups: An object of message group indexes, and for each message group, an array of sub-board
//             codes within it
//  numSubBoards: The total number of sub-boards in the configuration
//  startupSubBoardCode: The sub-board to use on startup of SlyVote
//  showAvatars: Whether or not to show user avatars when showing the polls
function ReadConfigFile()
{
	var retObj = {
		cfgReadError: "",
		useAllAvailableSubBoards: true,
		msgGroups: {},
		numSubBoards: 0,
		startupSubBoardCode: "",
		showAvatars: true
	};

	// Open the main configuration file.  First look for it in the sbbs/mods
	// directory, then sbbs/ctrl, then in the same directory as this script.
	var filename = "slyvote.cfg";
	//var cfgFilename = system.mods_dir + filename;
	var cfgFilename = file_cfgname(system.mods_dir, filename);
	if (!file_exists(cfgFilename))
		cfgFilename = file_cfgname(system.ctrl_dir, filename);
	if (!file_exists(cfgFilename))
		cfgFilename = file_cfgname(js.exec_dir, filename);
	var cfgFile = new File(cfgFilename);
	if (cfgFile.open("r"))
	{
		var settingsObj = cfgFile.iniGetObject();
		cfgFile.close();

		if (typeof(settingsObj["showAvatars"]) === "boolean")
			retObj.showAvatars = settingsObj.showAvatars;
		if (typeof(settingsObj["useAllAvailableSubBoards"]) === "boolean")
			retObj.useAllAvailableSubBoards = settingsObj.useAllAvailableSubBoards;
		if (typeof(settingsObj["startupSubBoardCode"]) === "string")
		{
			if (msg_area.sub.hasOwnProperty(settingsObj.startupSubBoardCode))
				retObj.startupSubBoardCode = settingsObj.startupSubBoardCode;
		}
		if (typeof(settingsObj["subBoardCodes"]) === "string")
		{
			// Split the value on commas and add all sub-board codes to
			// the appropriate array in retObj (based on its group index), as
			// long as they're valid sub-board codes.
			var valueLower = settingsObj.subBoardCodes.toLowerCase();
			var subCodeArray = valueLower.split(",");
			for (var idx = 0; idx < subCodeArray.length; ++idx)
			{
				// If the sub-board code exists and voting is allowed in the sub-board, then add it.
				if (msg_area.sub.hasOwnProperty(subCodeArray[idx]))
				{
					if ((msg_area.sub[subCodeArray[idx]].settings & SUB_NOVOTING) == 0)
					{
						var groupIdx = msg_area.sub[subCodeArray[idx]].grp_index;
						if (!retObj.msgGroups.hasOwnProperty(groupIdx))
							retObj.msgGroups[groupIdx] = [];
						retObj.msgGroups[groupIdx].push(subCodeArray[idx]);
					}
				}
			}
		}
	}
	else // Unable to read the configuration file
		retObj.cfgReadError = "Unable to open the configuration file: slyvote.cfg";

	// If we're set to use all available sub-boards, then populate the array
	// of internal codes of all sub-boards where voting is enabled.
	if (retObj.useAllAvailableSubBoards && retObj.cfgReadError.length == 0)
	{
		retObj.msgGroups = {};
		for (var grp in msg_area.grp_list)
		{
			for (var sub in msg_area.grp_list[grp].sub_list)
			{
				if ((msg_area.grp_list[grp].sub_list[sub].settings & SUB_NOVOTING) == 0)
				{
					var groupIdx = msg_area.grp_list[grp].sub_list[sub].grp_index;
					if (!retObj.msgGroups.hasOwnProperty(groupIdx))
						retObj.msgGroups[groupIdx] = [];
					retObj.msgGroups[groupIdx].push(msg_area.grp_list[grp].sub_list[sub].code);
				}
			}
		}
	}

	// If there is only one sub-board configured, then copy it to
	// startupSubBoardCode (don't worry if it's different - It should be the same)
	retObj.numSubBoards = 0;
	for (var grpIdx in retObj.msgGroups)
		retObj.numSubBoards += retObj.msgGroups[grpIdx].length;
	if (retObj.numSubBoards == 1)
	{
		for (var grpIdx in retObj.msgGroups)
		{
			retObj.startupSubBoardCode = retObj.msgGroups[grpIdx][0];
			break;
		}
	}
	// If there are multiple sub-board codes, make sure the startup code is in
	// there.  Otherwise, set the startup sub-board code to a blank string.
	else if (retObj.numSubBoards > 1)
	{
		if (retObj.startupSubBoardCode.length > 0)
		{
			var sawStartupCode = false;
			var startupCodeUpper = retObj.startupSubBoardCode.toUpperCase(); // For case-insensitive match
			for (var grpIdx in retObj.msgGroups)
			{
				for (var i = 0; (i < retObj.msgGroups[grpIdx].length) && !sawStartupCode; ++i)
					sawStartupCode = (retObj.msgGroups[grpIdx][i].toUpperCase() == startupCodeUpper);
				if (sawStartupCode)
					break;
			}
			if (!sawStartupCode)
				retObj.startupSubBoardCode = "";
		}
	}
	else // There are no sub-board codes, so ensure the startup code is blank.
		retObj.startupSubBoardCode = "";

	return retObj;
}

// Reads the user settings file.
//
// Parameters:
//  pFilename: The name of the file to read
//  pSlyVoteCfg: The SlyVote configuration object.  For validation of values in
//               the user settings file.
//
// Return value: An object with the following properties:
//  lastRead: An object specifying last-read message numbers, indexed by sub-board code
//  lastSubCode: The internal code of the sub-board the user was in last time, or blank if none
function ReadUserSettingsFile(pFilename, pSlyVoteCfg)
{
	var userSettingsObj = {
		lastRead: { },
		lastSubCode: ""
	};

	var userCfgFile = new File(pFilename);
	if (userCfgFile.open("r"))
	{
		var fileLine = null;     // A line read from the file
		var equalsPos = 0;       // Position of a = in the line
		var commentPos = 0;      // Position of the start of a comment
		var setting = null;      // A setting name (string)
		var settingUpper = null; // Upper-case setting name
		var value = null;        // To store a value for a setting (string)
		while (!userCfgFile.eof)
		{
			// Read the next line from the config file.
			fileLine = userCfgFile.readln(2048);

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

				if (settingUpper == "LAST_SUB_CODE")
				{
					var subCodeLower = value.toLowerCase();
					if ((subCodeLower.length > 0) && subBoardCodeIsValid(subCodeLower))
						userSettingsObj.lastSubCode = subCodeLower;
				}
				// Last-read message numbers.  Lines starting with lastread_
				// should have a sub-board code after the "lastread_".  Add
				// the message number to userSettingsObj.lastRead if the value
				// is all digits and there is a sub-board code specified.
				else if ((settingUpper.indexOf("LASTREAD_") == 0) && /^[0-9]+$/.test(value))
				{
					var subBoardCode = setting.substr(9).toLowerCase();
					if (subBoardCode.length > 0)
						userSettingsObj.lastRead[subBoardCode] = +value; // + to make it a numeric type
				}
			}
		}

		userCfgFile.close();
	}

	// Validate the user's lastSubCode value.  If not using all available sub-boards
	// and lastSubCode is not in the list of available sub-boards, then set it
	// to a blank string so that the default initial sub-board will be used.
	if ((userSettingsObj.lastSubCode.length > 0) && !pSlyVoteCfg.useAllAvailableSubBoards)
	{
		var lastSubCodeLower = userSettingsObj.lastSubCode.toLowerCase();
		var lastSubCodeIsValid = false;
		for (var grpIdx = 0; (grpIdx < pSlyVoteCfg.msgGroups.length) && !lastSubCodeIsValid; ++grpIdx)
		{
			for (var subIdx = 0; (subIdx < pSlyVoteCfg.msgGroups[grpIdx].length) && !lastSubCodeIsValid; ++subIdx)
				lastSubCodeIsValid = (lastSubCodeLower == pSlyVoteCfg.msgGroups[grpIdx][subIdx]);
		}

		if (!lastSubCodeIsValid)
			userSettingsObj.lastSubCode = "";
	}

	return userSettingsObj;
}

// Writes the user settings file
//
// Parameters:
//  pUserCfg: The object containing the user's configuration settings
//  pFilename: The name of the file to write (should be gUserSettingsFilename)
function WriteUserSettingsFile(pUserCfg, pFilename)
{
	var writeSucceeded = false;
	var userCfgFile = new File(pFilename);
	if (userCfgFile.open("w"))
	{
		writeSucceeded = userCfgFile.writeln("last_sub_code=" + gSubBoardCode);
		if (writeSucceeded)
		{
			for (var subBoardCode in pUserCfg.lastRead)
			{
				writeSucceeded = userCfgFile.writeln("lastread_" + subBoardCode + "=" + pUserCfg.lastRead[subBoardCode]);
				if (!writeSucceeded)
					break;
			}
		}
		userCfgFile.close();
	}
	return writeSucceeded;
}

// Returns whether or not a string is a valid sub-board code
//
// Parameters:
//  pSubCode: A sub-board code
//
// Return value: Boolean - Whether or not pSubCode is a valid sub-board code
function subBoardCodeIsValid(pSubCode)
{
	if ((typeof(pSubCode) != "string") || (pSubCode.length == 0))
		return false;

	var subCodeLower = pSubCode.toLowerCase();
	var subCodeValid = false;
	for (var subCode in msg_area.sub)
	{
		if (subCode.toLowerCase() == subCodeLower)
		{
			subCodeValid = true;
			break;
		}
	}
	return subCodeValid;
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
			var unvotedOptionFormatStr = "\x01n\x01" + "0\x01c\x01h%2d\x01n\x01" + "0\x01c: \x01w\x01h%-" + voteOptDescLen + "s [%-4d %6.2f%]\x01n\x01" + "0";
			var votedOptionFormatStr = "\x01n\x01" + "0\x01c\x01h%2d\x01n\x01" + 0 + "\x01c: \x01" + "5\x01w\x01h%-" + voteOptDescLen + "s [%-4d %6.2f%]\x01n\x01" + "0";
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
						msgBody += " " + CP437_CHECK_MARK;
					msgBody += "\r\n";
					++tallyIdx;
				}
			}
			if (pollComment.length > 0)
				msgBody = pollComment + "\r\n" + msgBody;

			// If the poll is closed, append some text saying it's closed.  Otherwise,
			// if  voting is allowed in this sub-board and the current logged-in
			// user has not voted on this message, then append some text saying how to vote.
			if ((pMsgHdr.auxattr & POLL_CLOSED) == POLL_CLOSED) // If the poll is closed
				msgBody += "\x01n\x01y\x01hThis poll is closed for voting.\x01n\r\n";
			else
			{
				var votingAllowed = ((pSubBoardCode != "mail") &&
				                     (((msg_area.sub[pSubBoardCode].settings & SUB_NOVOTING) == 0)) &&
				                     ((user.security.restrictions & UFLAG_V) == 0)); // Would be UFLAG_V if the user isn't allowed to vote
				if (votingAllowed && !HasUserVotedOnMsg(pMsgHdr.number, pSubBoardCode, pMsgbase, pUser))
					msgBody += "\x01n\x01" + "0\r\n\x01gTo vote in this poll, press \x01w\x01h" + gReaderKeys.vote + "\x01n\x01" + "0\x01g now.\r\n";
			}

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
					//"\r\n\x01n\x01" + "0\x01hOn %s, in \x01c%s \x01n\x01" + "0\x01c%s\r\n\x01h\x01m%s voted in your poll: \x01n\x01" + "0\x01h%s\r\n" 787 PollVoteNotice
					// For some reason, the bright white text in the PollVoteNotice text here will actually
					// appear as bright black unless we replace \x01n\x01h with \x01n\x01w\x01h
					var userVotedInYourPollText = processBBSTextDatText(bbs.text(PollVoteNotice));

					// Pass true to get_all_msg_headers() to tell it to return vote messages
					// (the parameter was introduced in Synchronet 3.17+)
					var tmpHdrs = pMsgbase.get_all_msg_headers(true);
					for (var tmpProp in tmpHdrs)
					{
						if (tmpHdrs[tmpProp] == null)
							continue;
						// If this header's thread_back or reply_id matches the poll message
						// number, then append the 'user voted' string to the message body.
						if ((tmpHdrs[tmpProp].thread_back == pMsgHdr.number) || (tmpHdrs[tmpProp].reply_id == pMsgHdr.id))
						{
							var msgWrittenLocalTime = MsgWrittenTimeToLocalBBSTime(tmpHdrs[tmpProp]);
							var voteDate = strftime("%a %b %d %Y %H:%M:%S", msgWrittenLocalTime);
							var voterStr = format(userVotedInYourPollText, voteDate, pMsgbase.cfg.grp_name, pMsgbase.cfg.name,
							                      tmpHdrs[tmpProp].from, pMsgHdr.subject);
							var voterStrArray = lfexpand(word_wrap(voterStr, console.screen_columns-1, null, false)).split("\r\n");
							for (var voterStrI = 0; voterStrI < voterStrArray.length; ++voterStrI)
							{
								msgBody += voterStrArray[voterStrI];
								if (voterStrI < voterStrArray.length-1)
									msgBody += "\r\n";
							}
							// tmpHdrs[tmpProp].votes is a bitfield of which options they voted for
							if (!/\r\n$/.test(userVotedInYourPollText))
								msgBody += "\r\n";
							// MSG_POLL_MAX_ANSWERS
							var answerBitIdx = 0;
							for (var fieldI = 0; fieldI < pMsgHdr.field_list.length; ++fieldI)
							{
								if (pMsgHdr.field_list[fieldI].type == SMB_POLL_ANSWER)
								{
									var answerBit = (1 << answerBitIdx);
									if ((tmpHdrs[tmpProp].votes & answerBit) == answerBit)
									{
										var optionStrArray = lfexpand(word_wrap(pMsgHdr.field_list[fieldI].data, console.screen_columns-1, null, false)).split("\r\n");
										if (optionStrArray.length > 0)
										{
											if (optionStrArray[optionStrArray.length-1] == "")
												optionStrArray.pop();
											msgBody += format(" - %s\r\n", optionStrArray[0]);
											for (var optStrI = 1; optStrI < optionStrArray.length; ++optStrI)
												msgBody += format("%s\r\n", optionStrArray[optStrI]);
										}
									}
									++answerBitIdx;
								}
							}
						}
					}
				}
			}
		}
	}
	else
		msgBody = pMsgbase.get_msg_body(false, pMsgHdr.number);

	// Remove any Synchronet pause codes that might exist in the message
	msgBody = msgBody.replace(/\\[xX]01[pP]/g, "");

	// If the user is a sysop, this is a moderated message area, and the message
	// hasn't been validated, then prepend the message with a message to let the
	// sysop now know to validate it.
	if (pSubBoardCode != "mail")
	{
		if (user.is_sysop && msg_area.sub[pSubBoardCode].is_moderated && ((pMsgHdr.attr & MSG_VALIDATED) == 0))
		{
			var validateNotice = "\x01n\x01h\x01yThis is an unvalidated message in a moderated area.  Press "
							   + gReaderKeys.validateMsg + " to validate it.\r\n\x01g";
			for (var i = 0; i < 79; ++i)
				validateNotice += CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE;
			validateNotice += "\x01n\r\n";
			msgBody = validateNotice + msgBody;
		}
	}

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
//  pUserVoteBitfield: Optional - A bitfield of votes from choice(s) inputted by the user representing their vote.
//                   If this is not passed in or is null, etc., then this function will
//                   prompt the user for their vote with a traditional user interface.
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
function VoteOnPoll(pSubBoardCode, pMsgbase, pMsgHdr, pUser, pUserVoteBitfield, pRemoveNLsFromVoteText)
{
	var retObj = {
		BBSHasVoteFunction: (typeof(pMsgbase.vote_msg) === "function"),
		savedVote: false,
		userQuit: false,
		errorMsg: "",
		mnemonicsRequiredForErrorMsg: false,
		updatedHdr: null
	};

	// Don't allow voting for personal email
	if (pSubBoardCode == "mail")
	{
		retObj.errorMsg = "Can not vote on personal email";
		return retObj;
	}
	// If the user isn't allowed to vote, then return with an error message
	if ((user.security.restrictions & UFLAG_V) == UFLAG_V)
	{
		// Use the line from text.dat that says the user is not allowed to vote
		retObj.errorMsg = "\x01n" + RemoveCRLFCodes(processBBSTextDatText(bbs.text(R_Voting)));
		return retObj;
	}

	// If the message vote function is not defined in the running verison of Synchronet,
	// then just return.
	if (!retObj.BBSHasVoteFunction)
		return retObj;

	var removeNLsFromVoteText = (typeof(pRemoveNLsFromVoteText) === "boolean" ? pRemoveNLsFromVoteText : false);

	// See if voting is allowed in the current sub-board
	if ((msg_area.sub[pSubBoardCode].settings & SUB_NOVOTING) == SUB_NOVOTING)
	{
		retObj.errorMsg = processBBSTextDatText(bbs.text(VotingNotAllowed));
		if (removeNLsFromVoteText)
			retObj.errorMsg = RemoveCRLFCodes(retObj.errorMsg);
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
			// TODO: I'm not sure if this 'if' section is correct anymore for
			// the latest 3.17 build of Synchronet (August 14, 2017)
			// Digital Man said:
			// In a poll message, the "votes" property specifies the maximum number of
			// answers/votes per ballot (0 is the equivalent of 1).
			// Max votes testing? :
			// userVotedMaxVotes = (votes == pMsgHdr.votes);
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
			retObj.errorMsg = processBBSTextDatText(bbs.text(VotedAlready));
			if (removeNLsFromVoteText)
				retObj.errorMsg = RemoveCRLFCodes(retObj.errorMsg);
			retObj.mnemonicsRequiredForErrorMsg = true;
			return retObj;
		}
	}

	// If the user has voted on this message already, then set an error message and return.
	if (HasUserVotedOnMsg(pMsgHdr.number, pSubBoardCode, pMsgbase, pUser))
	{
		retObj.errorMsg = processBBSTextDatText(bbs.text(VotedAlready));
		if (removeNLsFromVoteText)
			retObj.errorMsg = RemoveCRLFCodes(retObj.errorMsg);
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
	var voteMsgHdr = {
		thread_back: pMsgHdr.number,
		reply_id: pMsgHdr.id,
		from: (pMsgbase.cfg.settings & SUB_NAME) == SUB_NAME ? user.name : user.alias
	};
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
			var voteResponse = 0;
			if (typeof(pUserVoteBitfield) != "number")
			{
				console.clear("\x01n");
				var selectHdr = processBBSTextDatText(bbs.text(BallotHdr));
				printf("\x01n" + selectHdr + "\x01n", pMsgHdr.subject);
				var optionFormatStr = "\x01n\x01c\x01h%2d\x01n\x01c: \x01h%s\x01n";
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
				// Get & process the selection from the user
				if (pMsgHdr.votes > 1)
				{
					// Support multiple answers from the user
					console.print("\x01n\x01gYour vote numbers, separated by commas, up to \x01h" + pMsgHdr.votes + "\x01n\x01g (Blank/Q=Quit):");
					console.crlf();
					console.print("\x01c\x01h");
					var userInput = consoleGetStrWithValidKeys("0123456789,Q", null, false);
					if ((userInput.length > 0) && (userInput.toUpperCase() != "Q"))
					{
						var userAnswers = userInput.split(",");
						if (userAnswers.length > 0)
						{
							// Generate confirmation text and an array of numbers
							// representing the user's choices, up to the number
							// of responses allowed
							var confirmText = "Vote ";
							var voteNumbers = [];
							for (var i = 0; (i < userAnswers.length) && (i < pMsgHdr.votes); ++i)
							{
								// Trim any whitespace from the user's response
								userAnswers[i] = trimSpaces(userAnswers[i], true, true, true);
								if (/^[0-9]+$/.test(userAnswers[i]))
								{
									voteNumbers.push(+userAnswers[i]);
									confirmText += userAnswers[i] + ",";
								}
							}
							// If the confirmation text has a trailing comma, remove it
							if (/,$/.test(confirmText))
								confirmText = confirmText.substr(0, confirmText.length-1);
							// Confirm from the user and submit their vote if they say yes
							if (voteNumbers.length > 0)
							{
								if (console.yesno(confirmText))
								{
									voteResponse = 0;
									for (var i = 0; i < voteNumbers.length; ++i)
										voteResponse |= (1 << (voteNumbers[i]-1));
								}
								else
									retObj.userQuit = true;
							}
						}
					}
					else
						retObj.userQuit = true;
				}
				else
				{
					// Get the selection prompt text from text.dat and replace the %u or %d with
					// the number 1 (default option)
					var selectPromptText = processBBSTextDatText(bbs.text(SelectItemWhich));
					selectPromptText = selectPromptText.replace(/%[uU]/, 1).replace(/%[dD]/, 1);
					console.mnemonics(selectPromptText);
					var maxNum = optionNum - 1;
					var userInputNum = console.getnum(maxNum);
					if (userInputNum == -1) // The user chose Q to quit
						retObj.userQuit = true;
					else
						voteResponse = (1 << (userInputNum-1));
					console.attributes = "N";
				}
			}
			else
				voteResponse = pUserVoteBitfield;
			//if (userInputNum == 0) // The user just pressed enter to choose the default
			//	userInputNum = 1;
			if (!retObj.userQuit)
			{
				voteMsgHdr.attr = MSG_VOTE;
				voteMsgHdr.votes = voteResponse;
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
		console.clear("\x01n");
	else
		console.attributes = "N";

	// Borders and stylized SlyVote text
	DisplayTopScreenBorder();
	DisplaySlyVoteText();
	DisplayVerAndRegBorders();
	console.gotoxy(1, gBottomBorderRow);
	DisplayBottomScreenBorder();
	// Write the current sub-board
	var subBoardText = msg_area.sub[gSubBoardCode].grp_name + " - " + msg_area.sub[gSubBoardCode].name;
	subBoardText = "\x01n\x01b\x01hCurrent sub-board: \x01w" + subBoardText.substr(0, console.scren_columns);
	var subBoardTextX = (console.screen_columns/2) - (strip_ctrl(subBoardText).length/2);
	console.gotoxy(subBoardTextX, 9);
	console.print(subBoardText);
	// Write the number of polls in the sub-board
	var numOpenPolls = gSubBoardPollCountObj.numPolls-gSubBoardPollCountObj.numClosedPolls;
	var numPollsText = format("\x01n\x01b\x01hThere are \x01w%d \x01bopen polls in this sub-board (\x01w%d\x01b total)",
	                          numOpenPolls,
							  gSubBoardPollCountObj.numPolls);
	var numPollsTextX = (console.screen_columns/2) - (strip_ctrl(numPollsText).length/2);
	console.gotoxy(numPollsTextX, 10);
	console.print(numPollsText);
	// Write the number of polls the user has voted on and not voted on
	var numPollsVotedOn = 0;
	if (gSubBoardPollCountObj.numPollsRemainingForUser == 0)
	{
		if (numOpenPolls > 0)
			numPollsVotedOn = "all";
	}
	else
		numPollsVotedOn = gSubBoardPollCountObj.numPollsUserVotedOn;
	numPollsText = format("\x01n\x01b\x01hYou have voted on \x01w%s \x01bpolls in this sub-board (\x01w%d\x01b remaining)",
	                      numPollsVotedOn, gSubBoardPollCountObj.numPollsRemainingForUser);
	var numPollsTextX = (console.screen_columns/2) - (strip_ctrl(numPollsText).length/2);
	console.gotoxy(numPollsTextX, 11);
	console.print(numPollsText);
	// Write the SlyVote version centered
	console.attributes = "N";
	var fieldWidth = 28;
	console.gotoxy(41, 14);
	console.print(CenterText("\x01n\x01hSlyVote v\x01c" + SLYVOTE_VERSION.replace(".", "\x01b.\x01c") + "\x01n", fieldWidth));
	// Write the "Registered to" text centered
	console.gotoxy(41, 17);
	console.print(CenterText("\x01n\x01h" + system.operator + "\x01n", fieldWidth));
	console.gotoxy(41, 18);
	console.print(CenterText("\x01n\x01h" + system.name + "\x01n", fieldWidth));
	// Write the menu of options
	var curPos = { x: 7, y: 13 };
	var retObj = { optMenuX: curPos.x+4, optMenuY: curPos.y }; // For the option menu to be used later
	var numMenuOptions = 6;
	if (gSlyVoteCfg.numSubBoards > 1)
		++numMenuOptions;
	for (var optNum = 1; optNum <= numMenuOptions; ++optNum)
	{
		console.gotoxy(curPos.x, curPos.y++);
		console.print("\x01" + "7\x01h\x01w" + CP437_LEFT_HALF_BLOCK + "\x01n\x01k\x01" + "7" + optNum + "\x01n\x01" + "7\x01k\x01h" + CP437_RIGHT_HALF_BLOCK + "\x01n");
	}

	return retObj;
}

function DisplayTopScreenBorder()
{
	if (typeof(DisplayTopScreenBorder.borderText) !== "string")
	{
		DisplayTopScreenBorder.borderText = strRepeat("\x01n\x01b" + CP437_LOWER_HALF_BLOCK + "\x01h" + CP437_BLACK_SQUARE + "\x01c" + CP437_UPPER_HALF_BLOCK + "\x01b" + CP437_BLACK_SQUARE, 19);
		DisplayTopScreenBorder.borderText += "\x01n\x01b" + CP437_LOWER_HALF_BLOCK;
	}

	console.attributes = "N";
	//console.crlf();
	console.print(DisplayTopScreenBorder.borderText);
	console.attributes = "N";
	//console.crlf();
}

function DisplaySlyVoteText()
{
	if (typeof(DisplaySlyVoteText.slyVoteTextLines) === "undefined")
	{
		var LOWER_CENTER_BLOCK_2 = strRepeat(CP437_LOWER_HALF_BLOCK, 2);
		var LOWER_CENTER_BLOCK_3 = strRepeat(CP437_LOWER_HALF_BLOCK, 3);
		var LOWER_CENTER_BLOCK_4 = strRepeat(CP437_LOWER_HALF_BLOCK, 4);
		var LOWER_CENTER_BLOCK_5 = strRepeat(CP437_LOWER_HALF_BLOCK, 5);
		var LOWER_CENTER_BLOCK_7 = strRepeat(CP437_LOWER_HALF_BLOCK, 7);
		var spaces11 = "           ";
		DisplaySlyVoteText.slyVoteTextLines = [
						"\x01" + spaces11 + CP437_MIDDLE_DOT,
						spaces11 + "\x01n\x01h\x01c" + LOWER_CENTER_BLOCK_7 + " \x01n\x01c" + LOWER_CENTER_BLOCK_3 + "  \x01h\x01b" + CP437_MIDDLE_DOT + "  \x01n\x01c" + LOWER_CENTER_BLOCK_3 + " " + LOWER_CENTER_BLOCK_3 + " \x01h" + LOWER_CENTER_BLOCK_3 + " " + LOWER_CENTER_BLOCK_3 + " \x01n\x01c" + LOWER_CENTER_BLOCK_7 + " " + LOWER_CENTER_BLOCK_7 + " " + LOWER_CENTER_BLOCK_7 + " \x01h" + CP437_MIDDLE_DOT,
						spaces11 + "\x01n\x01h\x01c" + CP437_FULL_BLOCK + " " + LOWER_CENTER_BLOCK_4 + CP437_FULL_BLOCK + " \x01n\x01c" + CP437_FULL_BLOCK + " " + CP437_FULL_BLOCK + "\x01      " + CP437_FULL_BLOCK + CP437_LOWER_HALF_BLOCK + CP437_UPPER_HALF_BLOCK + CP437_FULL_BLOCK + CP437_UPPER_HALF_BLOCK + CP437_LOWER_HALF_BLOCK + CP437_FULL_BLOCK + " \x01h" + CP437_FULL_BLOCK + " " + CP437_FULL_BLOCK + " " + CP437_FULL_BLOCK + " " + CP437_FULL_BLOCK + " \x01n\x01c" + CP437_FULL_BLOCK + " " + LOWER_CENTER_BLOCK_3 + " " + CP437_FULL_BLOCK + " " + CP437_FULL_BLOCK + LOWER_CENTER_BLOCK_2 + " " + LOWER_CENTER_BLOCK_2 + CP437_FULL_BLOCK + " " + CP437_FULL_BLOCK + " " + LOWER_CENTER_BLOCK_4 + CP437_FULL_BLOCK + "",
						" \x01h" + CP437_MIDDLE_DOT + "         " + CP437_FULL_BLOCK + strRepeat(CP437_LOWER_HALF_BLOCK, 4) + " " + CP437_FULL_BLOCK + " \x01n\x01c" + CP437_FULL_BLOCK + " " + CP437_FULL_BLOCK + LOWER_CENTER_BLOCK_4 + "  " + CP437_UPPER_HALF_BLOCK + CP437_FULL_BLOCK + " " + CP437_FULL_BLOCK + CP437_UPPER_HALF_BLOCK + "  \x01h" + CP437_FULL_BLOCK + CP437_LOWER_HALF_BLOCK + CP437_UPPER_HALF_BLOCK + CP437_FULL_BLOCK + CP437_UPPER_HALF_BLOCK + CP437_LOWER_HALF_BLOCK + CP437_FULL_BLOCK + " \x01n\x01c" + CP437_FULL_BLOCK + " " + CP437_FULL_BLOCK + CP437_LOWER_HALF_BLOCK + CP437_FULL_BLOCK + " " + CP437_FULL_BLOCK + "   " + CP437_FULL_BLOCK + " " + CP437_FULL_BLOCK + "   " + CP437_FULL_BLOCK + " " + LOWER_CENTER_BLOCK_3 + CP437_FULL_BLOCK + CP437_LOWER_HALF_BLOCK,
						"\x01n       \x01h\x01b" + CP437_MIDDLE_DOT + "   \x01c" + CP437_FULL_BLOCK + LOWER_CENTER_BLOCK_5 + CP437_FULL_BLOCK + " \x01n\x01c" + CP437_FULL_BLOCK + LOWER_CENTER_BLOCK_5 + CP437_FULL_BLOCK + " \x01h" + CP437_MIDDLE_DOT + " \x01n\x01c" + CP437_FULL_BLOCK + CP437_LOWER_HALF_BLOCK + CP437_FULL_BLOCK + "    \x01h" + CP437_UPPER_HALF_BLOCK + CP437_FULL_BLOCK + CP437_LOWER_HALF_BLOCK + CP437_FULL_BLOCK + CP437_UPPER_HALF_BLOCK + " \x01n\x01b" + CP437_MIDDLE_DOT + "\x01c" + CP437_FULL_BLOCK + LOWER_CENTER_BLOCK_5 + CP437_FULL_BLOCK + "   " + CP437_FULL_BLOCK + CP437_LOWER_HALF_BLOCK + CP437_FULL_BLOCK + " \x01h" + CP437_MIDDLE_DOT + " \x01n\x01c" + CP437_FULL_BLOCK + LOWER_CENTER_BLOCK_5 + CP437_FULL_BLOCK + "   \x01h\x01b" + CP437_MIDDLE_DOT,
						"\x01n                                          \x01b" + CP437_MIDDLE_DOT
					];
	}

	console.attributes = "N";
	for (var i = 0; i < DisplaySlyVoteText.slyVoteTextLines.length; ++i)
	{
		console.print(DisplaySlyVoteText.slyVoteTextLines[i]);
		console.crlf();
	}
	console.attributes = "N";
}

function DisplayVerAndRegBorders()
{
	if (typeof(DisplayVerAndRegBorders.borderLines) === "undefined")
	{
		var HORIZONTAL_SINGLE_2 = strRepeat(CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE, 2);
		var HORIZONTAL_SINGLE_3 = strRepeat(CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE, 3);
		DisplayVerAndRegBorders.borderLines = [
								"\x01b\x01h" + CP437_BOX_DRAWINGS_UPPER_LEFT_SINGLE + CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE + "\x01k" + strRepeat(CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE, 2) + "\x01b" + CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE + "\x01n\x01b" + HORIZONTAL_SINGLE_2 + "\x01h\x01k" + HORIZONTAL_SINGLE_3 + "\x01b" + CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE + "\x01n\x01b" + CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE + "\x01h" + HORIZONTAL_SINGLE_2 + "\x01n\x01b" + CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE + "\x01h" + CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE + "\x01k" + CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE + "\x01b" + HORIZONTAL_SINGLE_2 + "\x01n\x01b" + CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE + "\x01h\x01k" + CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE + "\x01n\x01b" + CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE + "\x01h" + CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE + "\x01k" + HORIZONTAL_SINGLE_2 + "\x01b" + CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE + "\x01n\x01b" + CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE + "\x01h\x01k" + CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE + "\x01n\x01b" + CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE + "\x01h" + CP437_BOX_DRAWINGS_UPPER_RIGHT_SINGLE,
								"\x01n\x01b" + CP437_BOX_DRAWINGS_LIGHT_VERTICAL + "\x01n                            \x01k\x01h" + CP437_BOX_DRAWINGS_LIGHT_VERTICAL,
								"\x01b" + CP437_BOX_DRAWINGS_LOWER_LEFT_SINGLE + HORIZONTAL_SINGLE_2 + "\x01k" + HORIZONTAL_SINGLE_2 + "\x01n\x01b" + HORIZONTAL_SINGLE_3 + "\x01h\x01k" + CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE + "\x01b" + CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE + "\x01k" + CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE + "\x01b" + CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE + "\x01n\x01b" + CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE + "\x01h" + HORIZONTAL_SINGLE_2 + "\x01k" + CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE + "\x01n\x01b" + CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE + "\x01h\x01k" + CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE + "\x01n\x01b" + CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE + "\x01h" + CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE + "\x01k" + CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE + "\x01b" + CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE + "\x01k" + HORIZONTAL_SINGLE_2 + "\x01b" + CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE + "\x01k" + CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE + "\x01b" + CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE + "\x01n\x01b" + CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE + "\x01h" + CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE + "\x01n\x01b" + CP437_BOX_DRAWINGS_LOWER_RIGHT_SINGLE,
								CP437_BOX_DRAWINGS_UPPER_LEFT_SINGLE + "\x01h\x01k" + CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE + "\x01n\x01b" + HORIZONTAL_SINGLE_3 + "\x01h\x01k" + CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE + "\x01b" + CP437_BOX_DRAWINGS_LIGHT_VERTICAL_AND_LEFT + " \x01n\x01bR\x01he\x01n\x01bgistere\x01h\x01kd To: \x01b" + CP437_BOX_DRAWINGS_LIGHT_LEFT_T + "\x01k" + CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE + "\x01n\x01b" + CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE + "\x01h" + CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE + "\x01k" + CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE + CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE + CP437_BOX_DRAWINGS_UPPER_RIGHT_SINGLE,
								"\x01n\x01b" + CP437_BOX_DRAWINGS_LIGHT_VERTICAL + "\x01n                            \x01b\x01h" + CP437_BOX_DRAWINGS_LIGHT_VERTICAL,
								"\x01n\x01k\x01h" + CP437_BOX_DRAWINGS_LIGHT_VERTICAL + "\x01n                            \x01b" + CP437_BOX_DRAWINGS_LIGHT_VERTICAL,
								"\x01h" + CP437_BOX_DRAWINGS_LOWER_LEFT_SINGLE + "\x01k" + CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE + "\x01n\x01b" + CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE + "\x01h\x01k" + CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE + "\x01n\x01b" + CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE + "\x01h" + HORIZONTAL_SINGLE_3 + "\x01k" + HORIZONTAL_SINGLE_2 + "\x01b" + CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE + "\x01n\x01b" + CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE + "\x01h" + HORIZONTAL_SINGLE_3 + "\x01k" + CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE + "\x01n\x01b" + CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE + "\x01h" + CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE + "\x01n\x01b" + CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE + "\x01h" + HORIZONTAL_SINGLE_2 + "\x01n\x01b" + CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE + "\x01h" + CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE + "\x01n\x01b" + CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE + "\x01h" + CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE + "\x01n\x01b" + CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE + "\x01h" + CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE + "\x01n\x01b" + CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE + "\x01h\x01k" + CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE + "\x01b" + CP437_BOX_DRAWINGS_LOWER_RIGHT_SINGLE
					];
	}

	var curPos = { x: 40, y: 13 };
	console.gotoxy(curPos.x, curPos.y++);
	console.attributes = "N";
	for (var i = 0; i < DisplayVerAndRegBorders.borderLines.length; ++i)
	{
		console.print(DisplayVerAndRegBorders.borderLines[i]);
		console.gotoxy(curPos.x, curPos.y++);
	}
	console.attributes = "N";
}

function DisplayBottomScreenBorder()
{
	if (typeof(DisplayBottomScreenBorder.borderText) !== "string")
	{
		DisplayBottomScreenBorder.borderText = " " + strRepeat("\x01n\x01b" + CP437_UPPER_HALF_BLOCK + "\x01h" + CP437_BLACK_SQUARE + "\x01c" + CP437_LOWER_HALF_BLOCK + "\x01b" + CP437_BLACK_SQUARE, 19);
		DisplayBottomScreenBorder.borderText += "\x01n\x01b" + CP437_UPPER_HALF_BLOCK;
	}
	console.attributes = "N";
	//console.crlf();
	console.print(DisplayBottomScreenBorder.borderText);
	console.attributes = "N";
	//console.crlf();
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

// Returns an array of message headers for polls
//
// Parameters:
//  pSubBoardCode: The internal code of the sub-board to open
//  pCheckIfUserVoted: Boolean - Whether or not to check whether the user has voted on the polls
//  pOnlyOpenPolls: Boolean - Whether or not to return only open polls.
//
// Return value: An object containing the following properties:
//               errorMsg: A string containing an error message on failure or a blank string on success
//               msgHdrs: An array containing message headers for voting polls
//               pollsExist: Boolean - Whether polls exist in the sub-board.  This is useful when
//                           this function doesn't return any headers because the the user voted
//                           on all of them but polls do exist.
function GetPollHdrs(pSubBoardCode, pCheckIfUserVoted, pOnlyOpenPolls)
{
	var retObj = {
		errorMsg: "",
		msgHdrs: [],
		pollsExist: false
	};

	// Open the chosen sub-board
	var msgbase = new MsgBase(pSubBoardCode);
	if (msgbase.open())
	{
		var msgHdrs = msgbase.get_all_msg_headers(true);
		for (var prop in msgHdrs)
		{
			// Skip deleted and unreadable messages
			if ((msgHdrs[prop].attr & MSG_DELETE) == MSG_DELETE)
				continue;
			if (!IsReadableMsgHdr(msgHdrs[prop], pSubBoardCode))
				continue;

			if ((msgHdrs[prop].type & MSG_TYPE_POLL) == MSG_TYPE_POLL)
			{
				var includeThisPoll = true;
				if (pOnlyOpenPolls)
					includeThisPoll = ((msgHdrs[prop].auxattr & POLL_CLOSED) == 0);
				if (includeThisPoll)
				{
					retObj.pollsExist = true;
					if (pCheckIfUserVoted)
					{
						if (!HasUserVotedOnMsg(msgHdrs[prop].number, pSubBoardCode, msgbase, user))
							retObj.msgHdrs.push(msgHdrs[prop]);
					}
					else
						retObj.msgHdrs.push(msgHdrs[prop]);
				}
			}
		}

		msgbase.close();
	}
	else
		retObj.errorMsg = "Failed to open the messagebase";

	return retObj;
}

// Lets the user view the voting results for a sub-board.
//
// Parameters:
//  pSubBoardCode: The internal code of the sub-board to view voting results for
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
			if (IsReadableMsgHdr(msgHdrs[prop], pSubBoardCode))
			{
				pollMsgHdrs.push(msgHdrs[prop]);
				// If the user settings has a last-read message number, then use it.
				// Otherwise, look in Synchronet's message area array for the user's
				// last-read message number.
				if (gUserSettings.lastRead.hasOwnProperty(pSubBoardCode))
				{
					if (msgHdrs[prop].number === gUserSettings.lastRead[pSubBoardCode])
						currentMsgIdx = pollMsgIdx;
				}
				else
				{
					if (msgHdrs[prop].number == msg_area.sub[pSubBoardCode].last_read)
						currentMsgIdx = pollMsgIdx;
				}
				// TODO: Remove - Older - Dealing with Synchronet last read pointers:
				/*
				if (msgHdrs[prop].number == msg_area.sub[pSubBoardCode].last_read)
					currentMsgIdx = pollMsgIdx;
				*/
				++pollMsgIdx;
			}
		}
		
		// If there are no polls, then just return
		if (pollMsgHdrs.length == 0)
		{
			msgbase.close();
			DisplayErrorWithPause("\x01n\x01y\x01hThere are no polls to view.\x01n", gMessageRow, false);
			return nextProgramState;
		}

		// Create the key help line to be displayed at the bottom of the screen
		var keyText = "\x01rLeft\x01b, \x01rRight\x01b, \x01rUp\x01b, \x01rDn\x01b, \x01rPgUp\x01b/\x01rDn\x01b, \x01rF\x01m)\x01birst, \x01rL\x01m)\x01bast, \x01r#\x01b, ";
		if (PollDeleteAllowed(msgbase, pSubBoardCode))
			keyText += "\x01rDEL\x01b, ";
		keyText += "\x01rC\x01m)\x01blose, \x01rQ\x01m)\x01buit, \x01r?";
		var keyHelpLine = "\x01" + "7" + CenterText(keyText, console.screen_columns-1);

		// Get the unmodified default header lines to be displayed
		var displayMsgHdrUnmodified = GetDefaultMsgDisplayHdr();

		// Calculate the height of the frame to use and create the frame & scrollbar objects
		var frameHeight = console.screen_rows - displayMsgHdrUnmodified.length - 1;
		var frameAndScrollbar = createFrameWithScrollbar(1, displayMsgHdrUnmodified.length + 1,
		                                                 console.screen_columns, frameHeight);

		// Prepare the screen and display the key help line on the last row of the screen
		console.clear("\x01n");
		console.gotoxy(1, console.screen_rows);
		console.print(keyHelpLine);

		// User input loop
		var drawMsg = true;
		var drawKeyHelpLine = false;
		var pollDeleted = false; // Will need to check if the poll was deleted later
		var continueOn = true;
		while (continueOn)
		{
			// Do garbage collection to ensure low memory usage
			js.gc(true);

			pollDeleted = false;

			// Display the key help line if specified to do so
			if (drawKeyHelpLine)
			{
				console.gotoxy(1, console.screen_rows);
				console.print("\x01n" + keyHelpLine);
			}

			// Get the message header lines to be displayed
			var dateTimeStr = pollMsgHdrs[currentMsgIdx].date.replace(/ [-+][0-9]+$/, "");
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
				// If the 'show avatars' setting is true, then draw the 'from'
				// user's avatar on the screen
				if (gSlyVoteCfg.showAvatars)
				{
					gAvatar.draw(pollMsgHdrs[currentMsgIdx].from_ext, pollMsgHdrs[currentMsgIdx].from, pollMsgHdrs[currentMsgIdx].from_net_addr, /* above: */true, /* right-justified: */true);
					console.attributes = 0;	// Clear the background attribute as the next line might scroll, filling with BG attribute
				}
				console.attributes = "N";
			}
			var msgBodyText = GetMsgBody(msgbase, pollMsgHdrs[currentMsgIdx], pSubBoardCode, user);
			if (msgBodyText == null)
				msgBodyText = "Unable to load poll";

			// Load the poll text into the Frame object and draw the frame
			frameAndScrollbar.frame.clear();
			frameAndScrollbar.frame.attr&=~HIGH;
			frameAndScrollbar.frame.putmsg(msgBodyText, "\x01n");
			frameAndScrollbar.frame.scrollTo(0, 0);
			if (drawMsg)
			{
				frameAndScrollbar.frame.invalidate();
				frameAndScrollbar.scrollbar.cycle();
				frameAndScrollbar.frame.cycle();
				frameAndScrollbar.frame.draw();
			}
			// Let the user scroll the message, and take appropriate action based
			// on the user input
			var scrollRetObj = ScrollFrame(frameAndScrollbar.frame, frameAndScrollbar.scrollbar, 0, "\x01n", false, 1, console.screen_rows);
			drawMsg = true;
			drawKeyHelpLine = false;
			if (scrollRetObj.lastKeypress == KEY_LEFT)
			{
				// Go back one poll poll
				if (currentMsgIdx > 0)
					--currentMsgIdx;
				else
					drawMsg = false;
			}
			else if (scrollRetObj.lastKeypress == KEY_RIGHT)
			{
				// Go to the next poll poll, if there is one
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
				// Let the user vote on the poll in interactive mode (this uses
				// traditional style interaction rather than using a lightbar).
				// TODO: Change this to use the lightbar menu at some point?
				// Using the traditional voting interface:
				var voteRetObj = VoteOnPoll(pSubBoardCode, msgbase, pollMsgHdrs[currentMsgIdx], user, null, true);
				drawKeyHelpLine = true;
				// If the user's vote was saved, then update the message header so that it includes
				// the user's vote information.
				if (voteRetObj.savedVote)
				{
					var msgHdrs = msgbase.get_all_msg_headers(true);
					pollMsgHdrs[currentMsgIdx] = msgHdrs[pollMsgHdrs[currentMsgIdx].number];
					IncrementNumPollsVotedForUser();
				}
				// If there is an error message, then display it at the bottom row.
				if (voteRetObj.errorMsg.length > 0)
				{
					console.gotoxy(1, console.screen_rows-1);
					printf("\x01n%" + +(console.screen_columns-1) + "s", "");
					console.gotoxy(1, console.screen_rows-1);
					if (voteRetObj.mnemonicsRequiredForErrorMsg)
					{
						console.mnemonics(voteRetObj.errorMsg.substr(0, console.screen_columns-1));
						mswait(ERROR_PAUSE_WAIT_MS);
					}
					else
					{
						console.print("\x01n\x01y\x01h" + voteRetObj.errorMsg.substr(0, console.screen_columns-1));
						mswait(ERROR_PAUSE_WAIT_MS);
						console.gotoxy(1, console.screen_rows-1);
						printf("\x01n%" + strip_ctrl(voteRetObj.errorMsg).length + "s", "");
					}
					drawMsg = true;
					drawKeyHelpLine = false;
				}
				// Lightbar voting:
				/*
				console.clear("\n");
				var listTopRow = 2;
				var drawColRetObj = DrawVoteColumns(listTopRow);
				var startCol = drawColRetObj.columnX1+drawColRetObj.colWidth-1;
				var menuHeight = gBottomBorderRow-listTopRow;
				var voteRetObj = DisplayPollOptionsAndVote(gSubBoardCode, pollMsgHdrs[currentMsgIdx].number, startCol, listTopRow, drawColRetObj.textLen, menuHeight);
				if (voteRetObj.errorMsg.length > 0)
					DisplayErrorWithPause(voteRetObj.errorMsg, gMessageRow, voteRetObj.mnemonicsRequiredForErrorMsg);
				*/
			}
			else if (scrollRetObj.lastKeypress == gReaderKeys.close)
			{
				// Only let the user close the poll if they created it
				var pollCloseMsg = "";
				if ((pollMsgHdrs[currentMsgIdx].type & MSG_TYPE_POLL) == MSG_TYPE_POLL)
				{
					if ((pollMsgHdrs[currentMsgIdx].auxattr & POLL_CLOSED) == 0)
					{
						if (userHandleAliasNameMatch(pollMsgHdrs[currentMsgIdx].from))
						{
							// Prompt to confirm whether the user wants to close the poll
							console.gotoxy(1, console.screen_rows-1);
							printf("\x01n%" + +(console.screen_columns-1) + "s", "");
							console.gotoxy(1, console.screen_rows-1);
							if (!console.noyes("Close poll"))
							{
								// Close the poll
								if (closePollWithOpenMsgbase(msgbase, pollMsgHdrs[currentMsgIdx].number))
								{
									pollMsgHdrs[currentMsgIdx].auxattr |= POLL_CLOSED;
									++gSubBoardPollCountObj.numClosedPolls;
									pollCloseMsg = "\x01n\x01cThis poll was successfully closed.";
								}
								else
									pollCloseMsg = "\x01n\x01r\x01h* Failed to close this poll!";
							}
						}
						else
							pollCloseMsg = "\x01n\x01y\x01hCan't close this poll because it's not yours";
					}
					else
						pollCloseMsg = "\x01n\x01y\x01hThis poll is already closed";
				}
				else
					pollCloseMsg = "\x01n\x01y\x01hThis message is not a poll";

				// Write the poll close success/failure/status message
				if (pollCloseMsg.length > 0)
				{
					console.gotoxy(1, console.screen_rows-1);
					printf("\x01n%" + +(console.screen_columns-1) + "s", "");
					console.gotoxy(1, console.screen_rows-1);
					console.print(pollCloseMsg + "\n");
					mswait(ERROR_PAUSE_WAIT_MS);
					console.gotoxy(1, console.screen_rows-1);
				}
				drawMsg = true; // Always draw the message, at lest to overwrite the yes/no confirmation
				drawKeyHelpLine = false;
			}
			else if (/[0-9]/.test(scrollRetObj.lastKeypress))
			{
				// The user started typing a number - Continue inputting the
				// poll number and go to that poll

				var originalCurpos = console.getxy();
				// Put the user's input back in the input buffer to
				// be used for getting the rest of the message number.
				console.ungetstr(scrollRetObj.lastKeypress);
				// Move the cursor to the last row of the screen and prompt the
				// user for the message number.
				console.gotoxy(1, console.screen_rows);
				printf("\x01n%" + +(console.screen_columns-1) + "s", ""); // Clear the last row
				console.gotoxy(1, console.screen_rows);
				// Prompt for the message number, and go to that message if it's
				// different from the current message
				var msgNumInput = PromptForMsgNum(pollMsgHdrs.length, {x: 1, y: console.screen_rows}, "\x01n\x01cPoll #\x01g\x01h: \x01c", false);
				if (msgNumInput-1 != currentMsgIdx)
					currentMsgIdx = msgNumInput-1;
				else
					drawMsg = false;
				drawKeyHelpLine = true;
			}
			else if (scrollRetObj.lastKeypress == KEY_DEL)
			{
				if (PollDeleteAllowed(msgbase, pSubBoardCode))
				{
					// Go to the last line and confirm whether to delete the message
					console.gotoxy(1, console.screen_rows);
					printf("\x01n%" + +(console.screen_columns-1) + "s", "");
					console.gotoxy(1, console.screen_rows);
					var deleteMsg = !console.noyes("\x01n\x01cDelete this poll\x01n");
					if (deleteMsg)
					{
						var delMsgRetObj = DeleteMessage(msgbase, pollMsgHdrs[currentMsgIdx].number, pSubBoardCode);
						// Show an error message if there was one
						var errorMsg = "";
						if (!delMsgRetObj.messageDeleted)
							errorMsg = "The message was not deleted";
						else if (!delMsgRetObj.allVoteMsgsDeleted)
							errorMsg = "Not all vote result messages were deleted";
						if (errorMsg.length > 0)
						{
							console.gotoxy(1, console.screen_rows);
							printf("\x01n%" + +(console.screen_columns-1) + "s", "");
							console.gotoxy(1, console.screen_rows);
							console.print("\x01n\x01y\x01h* " + errorMsg);
							mswait(ERROR_PAUSE_WAIT_MS);
							console.gotoxy(1, console.screen_rows);
							printf("\x01n%" + strip_ctrl(errorMsg).length + "s", "");
						}

						// If the message was deleted, remove it from the array and
						// adjust the current message index if necessary
						if (delMsgRetObj.messageDeleted)
						{
							pollDeleted = true;
							pollMsgHdrs.splice(currentMsgIdx, 1);
							// Update the poll count in the current sub-board
							--gSubBoardPollCountObj.numPolls;
							// Adjust the current message index
							if (pollMsgHdrs.length > 0)
							{
								if (currentMsgIdx >= pollMsgHdrs.length)
									currentMsgIdx = pollMsgHdrs.length - 1;
							}
							else
							{
								continueOn = false;
								console.gotoxy(1, console.screen_rows);
								console.attributes = "N";
								console.crlf();
								console.print("\x01nThere are no more polls.\x01n");
								console.crlf();
								console.pause();
							}
						}
					}
					// Note: Leave drawMsg as true because the yes/no prompt on the
					// last row would shift the message up one line
					//else
					//	drawMsg = false;
					drawKeyHelpLine = true;
				}
				else
					drawMsg = false;
			}
			else if (scrollRetObj.lastKeypress == "?")
			{
				DisplayViewingResultsHelpScr(msgbase, pSubBoardCode);
				drawKeyHelpLine = true;
				drawMsg = true;
			}
			else if (scrollRetObj.lastKeypress == gReaderKeys.validateMsg)
			{
				if (user.is_sysop && msg_area.sub[pSubBoardCode].is_moderated)
				{
					var validated = ValidateMsg(msgbase, pSubBoardCode, pollMsgHdrs[currentMsgIdx].number);
					console.gotoxy(1, console.screen_rows-2);
					console.print("\x01n\x01h\x01g");
					for (var i = 0; i < console.screen_columns-1; ++i)
						console.print("?"); // Horizontal single line
					console.gotoxy(1, console.screen_rows-1);
					printf("\x01n%" + +(console.screen_columns-1) + "s", "");
					console.gotoxy(1, console.screen_rows-1);
					if (validated)
					{
						console.print("\x01cMessage was validated successfully.\x01n");
						drawMsg = true;
						pollMsgHdrs[currentMsgIdx].attr |= MSG_VALIDATED;
					}
					else
					{
						console.print("\x01y\x01hMessage validation failed!\x01n");
						drawMsg = false;
					}
					mswait(ERROR_PAUSE_WAIT_MS);
				}
				else
					drawMsg = false;
			}
			else if (scrollRetObj.lastKeypress == gReaderKeys.quit)
				continueOn = false;

			// If the user didn't delete this message, update the user's last read
			// message number in their user settings
			if (!pollDeleted)
				gUserSettings.lastRead[pSubBoardCode] = pollMsgHdrs[currentMsgIdx].number;
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
	var hdrLine1 = "\x01n\x01h\x01c" + CP437_BOX_DRAWINGS_UPPER_LEFT_SINGLE + CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE + "\x01n\x01c"
	             + CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE + " \x01h@GRP-L";
	var numChars = msgGrpNameLen - 7;
	for (var i = 0; i < numChars; ++i)
		hdrLine1 += "#";
	hdrLine1 += "@ @SUB-L";
	numChars = subBoardNameLen - 7;
	for (var i = 0; i < numChars; ++i)
		hdrLine1 += "#";
	hdrLine1 += "@\x01k";
	numChars = console.screen_columns - console.strlen(hdrLine1) - 4;
	for (var i = 0; i < numChars; ++i)
		hdrLine1 += CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE;
	hdrLine1 += "\x01n\x01c" + CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE + CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE + "\x01h"
	         + CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE + CP437_BOX_DRAWINGS_UPPER_RIGHT_SINGLE;
	hdrDisplayLines.push(hdrLine1);
	var hdrLine2 = "\x01n\x01c" + CP437_BOX_DRAWINGS_LIGHT_VERTICAL + "\x01h\x01k" + CP437_LIGHT_SHADE + CP437_MEDIUM_SHADE
	             + CP437_DARK_SHADE + "\x01gM\x01n\x01gsg#\x01h\x01c: \x01b@MSG_NUM_AND_TOTAL-L";
	numChars = console.screen_columns - 32;
	for (var i = 0; i < numChars; ++i)
		hdrLine2 += "#";
	hdrLine2 += "@\x01n\x01c" + CP437_BOX_DRAWINGS_LIGHT_VERTICAL;
	hdrDisplayLines.push(hdrLine2);
	var hdrLine3 = "\x01n\x01h\x01k" + CP437_BOX_DRAWINGS_LIGHT_VERTICAL + CP437_LIGHT_SHADE + CP437_MEDIUM_SHADE + CP437_DARK_SHADE
				 + "\x01gF\x01n\x01grom\x01h\x01c: \x01b@MSG_FROM_AND_FROM_NET-L";
	numChars = console.screen_columns - 36;
	for (var i = 0; i < numChars; ++i)
		hdrLine3 += "#";
	hdrLine3 += "@\x01k" + CP437_BOX_DRAWINGS_LIGHT_VERTICAL;
	hdrDisplayLines.push(hdrLine3);
	var hdrLine4 = "\x01n\x01h\x01k" + CP437_BOX_DRAWINGS_LIGHT_VERTICAL + CP437_LIGHT_SHADE + CP437_MEDIUM_SHADE + CP437_DARK_SHADE
	             + "\x01gS\x01n\x01gubj\x01h\x01c: \x01b@MSG_SUBJECT-L";
	numChars = console.screen_columns - 26;
	for (var i = 0; i < numChars; ++i)
		hdrLine4 += "#";
	hdrLine4 += "@\x01k" + CP437_BOX_DRAWINGS_LIGHT_VERTICAL;
	hdrDisplayLines.push(hdrLine4);
	var hdrLine5 = "\x01n\x01c" + CP437_BOX_DRAWINGS_LIGHT_VERTICAL + "\x01h\x01k" + CP437_LIGHT_SHADE + CP437_MEDIUM_SHADE + CP437_DARK_SHADE
	             + "\x01gD\x01n\x01gate\x01h\x01c: \x01b@MSG_DATE-L";
	//numChars = console.screen_columns - 23;
	numChars = console.screen_columns - 67;
	for (var i = 0; i < numChars; ++i)
		hdrLine5 += "#";
	//hdrLine5 += "@\x01n\x01c" + CP437_BOX_DRAWINGS_LIGHT_VERTICAL;
	hdrLine5 += "@ @MSG_TIMEZONE@\x01n";
	for (var i = 0; i < 40; ++i)
		hdrLine5 += " ";
	hdrLine5 += "\x01n\x01c" + CP437_BOX_DRAWINGS_LIGHT_VERTICAL;
	hdrDisplayLines.push(hdrLine5);
	var hdrLine6 = "\x01n\x01h\x01c" + CP437_BOX_DRAWINGS_LIGHT_BOTTOM_T + CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE + "\x01n\x01c"
	             + CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE + CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE + "\x01h\x01k";
	numChars = console.screen_columns - 8;
	for (var i = 0; i < numChars; ++i)
		hdrLine6 += CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE;
	hdrLine6 += "\x01n\x01c" + CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE + CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE + "\x01h"
	         + CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE + CP437_BOX_DRAWINGS_LIGHT_BOTTOM_T;
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
	if (pAtCodeStr.indexOf("@MSG_FROM_AND_FROM_NET") > -1)
	{
		var fromWithNet = pMsgHdr.from + (typeof(pMsgHdr.from_net_addr) == "string" ? " (" + pMsgHdr.from_net_addr + ")" : "");
		replacementTxt = fromWithNet.substr(0, pSpecifiedLen);
	}
	else if (pAtCodeStr.indexOf("@MSG_FROM") > -1)
		replacementTxt = pMsgHdr.from.substr(0, pSpecifiedLen);
	else if (pAtCodeStr.indexOf("@MSG_FROM_EXT") > -1)
		replacementTxt = (typeof pMsgHdr.from_ext === "undefined" ? "" : pMsgHdr.from_ext.substr(0, pSpecifiedLen));
	else if ((pAtCodeStr.indexOf("@MSG_TO") > -1) || (pAtCodeStr.indexOf("@MSG_TO_NAME") > -1))
		replacementTxt = pMsgHdr.to.substr(0, pSpecifiedLen);
	else if (pAtCodeStr.indexOf("@MSG_TO_EXT") > -1)
		replacementTxt = (typeof pMsgHdr.to_ext === "undefined" ? "" : pMsgHdr.to_ext.substr(0, pSpecifiedLen));
	else if (pAtCodeStr.indexOf("@MSG_SUBJECT") > -1)
		replacementTxt = pMsgHdr.subject.substr(0, pSpecifiedLen);
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
		var messageNum = (typeof(pDisplayMsgNum) == "number" ? pDisplayMsgNum : pMsgHdr.offset+1);
		replacementTxt = (messageNum.toString() + "/" + pNumMsgs).substr(0, pSpecifiedLen); // "number" is also absolute number
	}
	else if (pAtCodeStr.indexOf("@MSG_NUM") > -1)
	{
		var messageNum = (typeof(pDisplayMsgNum) == "number" ? pDisplayMsgNum : pMsgHdr.offset+1);
		replacementTxt = messageNum.toString().substr(0, pSpecifiedLen); // "number" is also absolute number
	}
	else if (pAtCodeStr.indexOf("@MSG_ID") > -1)
		replacementTxt = (typeof pMsgHdr.id === "undefined" ? "" : pMsgHdr.id.substr(0, pSpecifiedLen));
	else if (pAtCodeStr.indexOf("@MSG_REPLY_ID") > -1)
		replacementTxt = (typeof pMsgHdr.reply_id === "undefined" ? "" : pMsgHdr.reply_id.substr(0, pSpecifiedLen));
	else if (pAtCodeStr.indexOf("@MSG_TIMEZONE") > -1)
	{
		if (pUseBBSLocalTimeZone)
			replacementTxt = system.zonestr(system.timezone).substr(0, pSpecifiedLen);
		else
			replacementTxt = system.zonestr(pMsgHdr.when_written_zone).substr(0, pSpecifiedLen);
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
	var messageNum = (typeof(pDisplayMsgNum) == "number" ? pDisplayMsgNum : pMsgHdr.offset+1);
	var msgUpvotes = 0;
	var msgDownvotes = 0;
	var msgVoteScore = 0;
	if (pMsgHdr.hasOwnProperty("total_votes") && pMsgHdr.hasOwnProperty("upvotes"))
	{
		msgUpvotes = pMsgHdr.upvotes;
		msgDownvotes = pMsgHdr.total_votes - pMsgHdr.upvotes;
		msgVoteScore = pMsgHdr.upvotes - msgDownvotes;
	}
	var newTxtLine = textLine.replace(/@MSG_SUBJECT@/gi, pMsgHdr.subject)
	                         .replace(/@MSG_FROM@/gi, pMsgHdr.from)
	                         .replace(/@MSG_FROM_AND_FROM_NET@/gi, pMsgHdr.from + (typeof(pMsgHdr.from_net_addr) == "string" ? " (" + pMsgHdr.from_net_addr + ")" : ""))
	                         .replace(/@MSG_FROM_EXT@/gi, (typeof(pMsgHdr.from_ext) == "string" ? pMsgHdr.from_ext : ""))
	                         .replace(/@MSG_TO@/gi, pMsgHdr.to)
	                         .replace(/@MSG_TO_NAME@/gi, pMsgHdr.to)
	                         .replace(/@MSG_TO_EXT@/gi, (typeof(pMsgHdr.to_ext) == "string" ? pMsgHdr.to_ext : ""))
	                         .replace(/@MSG_DATE@/gi, pDateTimeStr)
	                         .replace(/@MSG_ATTR@/gi, mainMsgAttrStr)
	                         .replace(/@MSG_AUXATTR@/gi, auxMsgAttrStr)
	                         .replace(/@MSG_NETATTR@/gi, netMsgAttrStr)
	                         .replace(/@MSG_ALLATTR@/gi, allMsgAttrStr)
	                         .replace(/@MSG_NUM_AND_TOTAL@/gi, messageNum.toString() + "/" + pNumMsgs)
	                         .replace(/@MSG_NUM@/gi, messageNum.toString())
	                         .replace(/@MSG_ID@/gi, (typeof(pMsgHdr.id) == "string" ? pMsgHdr.id : ""))
	                         .replace(/@MSG_REPLY_ID@/gi, (typeof(pMsgHdr.reply_id) == "string" ? pMsgHdr.reply_id : ""))
	                         .replace(/@MSG_FROM_NET@/gi, (typeof(pMsgHdr.from_net_addr) == "string" ? pMsgHdr.from_net_addr : ""))
	                         .replace(/@MSG_TO_NET@/gi, (typeof(pMsgHdr.to_net_addr) == "string" ? pMsgHdr.to_net_addr : ""))
	                         .replace(/@MSG_TIMEZONE@/gi, (useBBSLocalTimeZone ? system.zonestr(system.timezone) : system.zonestr(pMsgHdr.when_written_zone)))
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
// string "\x01PgUp" (KEY_PAGE_UP) or "\x01Pgdn" (KEY_PAGE_DOWN),
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

// Prompts the user for a message number and keeps repeating if they enter and
// invalid message number.  It is assumed that the cursor has already been
// placed where it needs to be for the prompt text, but a cursor position is
// one of the parameters to this function so that this function can write an
// error message if the message number inputted from the user is invalid.
//
// Parameters:
//  pMaxNum: The maximum number allowed
//  pCurPos: The starting position of the cursor (used for positioning the
//           cursor at the prompt text location to display an error message).
//           If not needed (i.e., in a traditional user interface or non-ANSI
//           terminal), this parameter can be null.
//  pPromptText: The text to use to prompt the user
//  pClearToEOLAfterPrompt: Whether or not to clear to the end of the line after
//                          writing the prompt text (boolean)
//
// Return value: The message number entered by the user.  If the user didn't
//               enter a message number, this will be 0.
function PromptForMsgNum(pMaxNum, pCurPos, pPromptText, pClearToEOLAfterPrompt)
{
	var curPosValid = ((pCurPos != null) && (typeof(pCurPos) == "object") &&
	                   pCurPos.hasOwnProperty("x") && (typeof(pCurPos.x) == "number") &&
	                   pCurPos.hasOwnProperty("y") && (typeof(pCurPos.y) == "number"));
	var useCurPos = (console.term_supports(USER_ANSI) && curPosValid);

	var msgNum = 0;
	var promptCount = 0;
	var lastErrorLen = 0; // The length of the last error message
	var promptTextLen = console.strlen(pPromptText);
	var continueAskingMsgNum = false;
	do
	{
		++promptCount;
		msgNum = 0;
		if (promptTextLen > 0)
			console.print(pPromptText);
		if (pClearToEOLAfterPrompt && useCurPos)
		{
			// The first time the user is being prompted, clear the line to the
			// end of the line.  For subsequent times, clear the line from the
			// prompt text length to the error text length;
			if (promptCount == 1)
			{
				var curPos = console.getxy();
				var clearLen = console.screen_columns - curPos.x + 1;
				if (curPos.y == console.screen_rows)
					--clearLen;
				printf("\x01n%" + clearLen + "s", "");
			}
			else
			{
				if (lastErrorLen > promptTextLen)
				{
					console.attributes = "N";
					console.gotoxy(pCurPos.x+promptTextLen, pCurPos.y);
					var clearLen = lastErrorLen - promptTextLen;
					printf("\x01n%" + clearLen + "s", "");
				}
			}
			console.gotoxy(pCurPos.x+promptTextLen, pCurPos.y);
		}
		msgNum = console.getnum(pMaxNum);
		continueAskingMsgNum = false;
	}
	while (continueAskingMsgNum);
	return msgNum;
}

// Deletes a message
//
// Parameters:
//  pMsgbase: The MessageBase object for the message sub-board
//  pMsgNum: The number of the message to delete
//  pSubBoardCode: The internal code of the sub-board
//
// Return value: An object containing the following values:
//               messageDeleted: Boolean - Whether or not the message was deleted
//               allVoteMsgsDeleted: Boolean - Whether or not all vote response messages were deleted
function DeleteMessage(pMsgbase, pMsgNum, pSubBoardCode)
{
	var retObj = {
		messageDeleted: false,
		allVoteMsgsDeleted: false
	};

	var msgHdr = pMsgbase.get_msg_header(false, pMsgNum, true);
	var msgID = (msgHdr == null ? "" : msgHdr.id);
	retObj.messageDeleted = pMsgbase.remove_msg(false, pMsgNum);
	if (retObj.messageDeleted)
	{
		// Delete any vote response messages for this message
		var voteDelRetObj = DeleteVoteMsgs(pMsgbase, pMsgNum, msgID, (pSubBoardCode == "mail"));
		retObj.allVoteMsgsDeleted = voteDelRetObj.allVoteMsgsDeleted;
	}

	return retObj;
}

// Deletes vote messages (messages that have voting response data for a message with
// a given message number).
//
// Parameters:
//  pMsgbase: A MessageBase object containing the messages to be deleted
//  pMsgNum: The number of the message for which vote messages should be deleted
//  pMsgID: The ID of the message for which vote messages should be deleted
//  pIsMailSub: Boolean - Whether or not it's the personal email area
//
// Return value: An object containing the following properties:
//               numVoteMsgs: The number of vote messages for the given message number
//               numVoteMsgsDeleted: The number of vote messages that were deleted
//               allVoteMsgsDeleted: Boolean - Whether or not all vote messages were deleted
function DeleteVoteMsgs(pMsgbase, pMsgNum, pMsgID, pIsEmailSub)
{
	var retObj = {
		numVoteMsgs: 0,
		numVoteMsgsDeleted: 0,
		allVoteMsgsDeleted: true
	};

	if ((pMsgbase === null) || !pMsgbase.is_open)
		return retObj;
	if (typeof(pMsgNum) != "number")
		return retObj;
	if (pIsEmailSub)
		return retObj;

	// This relies on get_all_msg_headers() returning vote messages.  The get_all_msg_headers()
	// function was added in Synchronet 3.16, and the 'true' parameter to get vote headers was
	// added in Synchronet 3.17.
	if (typeof(pMsgbase.get_all_msg_headers) === "function")
	{
		var msgHdrs = pMsgbase.get_all_msg_headers(true);
		for (var msgHdrsProp in msgHdrs)
		{
			if (msgHdrs[msgHdrsProp] == null)
				continue;
			// If this header is a vote header and its thread_back or reply_id matches the given message,
			// then we can delete this message.
			var isVoteMsg = (((msgHdrs[msgHdrsProp].attr & MSG_VOTE) == MSG_VOTE) || ((msgHdrs[msgHdrsProp].attr & MSG_UPVOTE) == MSG_UPVOTE) || ((msgHdrs[msgHdrsProp].attr & MSG_DOWNVOTE) == MSG_DOWNVOTE));
			if (isVoteMsg && (msgHdrs[msgHdrsProp].thread_back == pMsgNum) || (msgHdrs[msgHdrsProp].reply_id == pMsgID))
			{
				++retObj.numVoteMsgs;
				msgWasDeleted = pMsgbase.remove_msg(false, msgHdrs[msgHdrsProp].number);
				retObj.allVoteMsgsDeleted = (retObj.allVoteMsgsDeleted && msgWasDeleted);
				if (msgWasDeleted)
					++retObj.numVoteMsgsDeleted;
			}
		}
	}

	return retObj;
}

// Returns whether a message is readable to the user, based on its
// header and the sub-board code.
//
// Parameters:
//  pMsgHdr: The header object for the message
//  pSubBoardCode: The internal code for the sub-board the message is in
//
// Return value: Boolean - Whether or not the message is readable for the user
function IsReadableMsgHdr(pMsgHdr, pSubBoardCode)
{
	if (pMsgHdr === null)
		return false;
	if ((pMsgHdr.type & MSG_TYPE_POLL) != MSG_TYPE_POLL)
		return false;
	if (pSubBoardCode.toUpperCase() == "MAIL")
		return false;

	// Let the sysop see unvalidated messages and private messages but not other users.
	if (!user.is_sysop)
	{
		if ((msg_area.sub[pSubBoardCode].is_moderated && ((pMsgHdr.attr & MSG_VALIDATED) == 0)) ||
		    (((pMsgHdr.attr & MSG_PRIVATE) == MSG_PRIVATE) && !userHandleAliasNameMatch(pMsgHdr.to)))
		{
			return false;
		}
	}
	// If the message is deleted, determine whether it should be viewable, based
	// on the system settings.
	if ((pMsgHdr.attr & MSG_DELETE) == MSG_DELETE)
	{
		return false;
		/*
		// If the user is a sysop, check whether sysops can view deleted messages.
		// Otherwise, check whether users can view deleted messages.
		if (gIsSysop)
		{
			if ((system.settings & SYS_SYSVDELM) == 0)
				return false;
		}
		else
		{
			if ((system.settings & SYS_USRVDELM) == 0)
				return false;
		}
		*/
	}
	// The message voting and poll variables were added in sbbsdefs.js for
	// Synchronet 3.17.  Make sure they're defined before referencing them.
	if (typeof(MSG_UPVOTE) != "undefined")
	{
		if ((pMsgHdr.attr & MSG_UPVOTE) == MSG_UPVOTE)
			return false;
	}
	if (typeof(MSG_DOWNVOTE) != "undefined")
	{
		if ((pMsgHdr.attr & MSG_DOWNVOTE) == MSG_DOWNVOTE)
			return false;
	}
	// Don't include polls as being unreadable messages - They just need to have
	// their answer selections read from the header instead of the message body
	/*
	if (typeof(MSG_POLL) != "undefined")
	{
		if ((pMsgHdr.attr & MSG_POLL) == MSG_POLL)
			return false;
	}
	*/
	return true;
}

// Returns whether a given name matches the logged-in user's handle, alias, or
// name.
//
// Parameters:
//  pName: A name to match against the logged-in user
//
// Return value: Boolean - Whether or not the given name matches the logged-in
//               user's handle, alias, or name
function UserHandleAliasNameMatch(pName)
{
   if (typeof(pName) != "string")
      return false;

   var userMatch = false;
   var nameUpper = pName.toUpperCase();
   if (user.handle.length > 0)
      userMatch = (nameUpper.indexOf(user.handle.toUpperCase()) > -1);
   if (!userMatch && (user.alias.length > 0))
      userMatch = (nameUpper.indexOf(user.alias.toUpperCase()) > -1);
   if (!userMatch && (user.name.length > 0))
      userMatch = (nameUpper.indexOf(user.name.toUpperCase()) > -1);
   return userMatch;
}

// Draws the colums that will be on either side of the voting polls
// & choices.
//
// Parameters:
//  pTopRow: The row on the screen to use as the top row
//  pBottomRow: The row on the screen to use as the bottom row.  Optional; defaults
//              to one before gBottomBorderRow.
//  pColumnX1: The X (horizontal) coordinate of the start of the first column.  Optional;
//             defaults to 9.
//  pColumnX2: The X (horizontal) coordinate of the start of the 2nd column.  Optional;
//             defaults to 71.
//
// Return value: An object containing the following properties:
//               columnX1: The X coordinate of the first (left) column
//               columnX2: The X coordinate of the first (right) column
//               textLen: The length of text that can fit between the columns
//               colWidth: The width of the columns
//               colHeight: The height of the columns
function DrawVoteColumns(pTopRow, pBottomRow, pColumnX1, pColumnX2)
{
	var retObj = {
		columnX1: (typeof(pColumnX1) == "number" ? pColumnX1 : 9), // 17
		columnX2: (typeof(pColumnX2) == "number" ? pColumnX2 : 71), // 63
		textLen: 0,
		colWidth: 3,
		colHeight: 0
	};
	retObj.textLen = retObj.columnX2 - retObj.columnX1 - 2;

	// Draw the columns
	var bottomRow = (typeof(pBottomRow) == "number" ? pBottomRow : gBottomBorderRow-1);
	retObj.colHeight = bottomRow - pTopRow + 1;
	for (var posY = pTopRow; posY <= bottomRow; ++posY)
	{
		console.gotoxy(retObj.columnX1, posY);
		console.print("\x01" + "7\x01h\x01w" + CP437_LEFT_HALF_BLOCK + "\x01n\x01k\x01" + "7\x01n\x01" + "7\x01k\x01h" + CP437_RIGHT_HALF_BLOCK + "\x01n");
		console.gotoxy(retObj.columnX2, posY);
		console.print("\x01" + "7\x01h\x01w" + CP437_LEFT_HALF_BLOCK + "\x01n\x01k\x01" + "7\x01n\x01" + "7\x01k\x01h" + CP437_RIGHT_HALF_BLOCK + "\x01n");
	}

	return retObj;
}

// Returns whether deleting a poll (message) is allowed.
//
// Parameters:
//  pMsgbase: The MsgBase object representing the current open sub-board
//  pSubBoardCode: The internal code of the sub-board
//
// REturen value: Boolean - Whether deleting a poll is allowed
function PollDeleteAllowed(pMsgbase, pSubBoardCode)
{
	var canDelete = user.is_sysop || (pSubBoardCode == "mail");
	if ((pMsgbase != null) && pMsgbase.is_open && (pMsgbase.cfg != null))
		canDelete = canDelete || ((pMsgbase.cfg.settings & SUB_DEL) == SUB_DEL);
	return canDelete;
}

// Displays a help screen for viewing all poll results
//
// Parameters:
//  pMsgbase: A MessageBase object representing the sub-board
//  pSubBoardCode: The internal code of the sub-board
function DisplayViewingResultsHelpScr(pMsgbase, pSubBoardCode)
{
	console.clear("\n");
	DisplayProgramNameAndVerForHelpScreens();
	console.crlf();
	console.crlf();
	var subBoardGrpAndName = msg_area.sub[pSubBoardCode].grp_name + " - " + msg_area.sub[pSubBoardCode].name;
	console.print("\x01cCurrently viewing results in area \x01g" + subBoardGrpAndName);
	console.crlf();
	console.crlf();
	console.print("Keys");
	console.crlf();
	console.print("\x01k\x01h" + strRepeat(CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE, 4));
	console.crlf();
	var keyHelpLines = ["\x01h\x01cDown\x01g/\x01cup arrow    \x01g: \x01n\x01cScroll the text down\x01g/\x01cup",
	                    "\x01h\x01cLeft\x01g/\x01cright arrow \x01g: \x01n\x01cGo to the previous\x01g/\x01cnext poll",
	                    "\x01h\x01cPageUp\x01g/\x01cPageDown  \x01g: \x01n\x01cScroll the text up\x01g/\x01cdown a page",
	                    "\x01h\x01cHOME             \x01g: \x01n\x01cGo to the top of the text",
	                    "\x01h\x01cEND              \x01g: \x01n\x01cGo to the bottom of the text",
						"\x01h\x01cF                \x01g: \x01n\x01cGo to the first poll",
						"\x01h\x01cL                \x01g: \x01n\x01cGo to the last poll"];
	if (user.is_sysop)
		keyHelpLines.push("\x01h\x01cDEL              \x01g: \x01n\x01cDelete the current poll");
	else if (PollDeleteAllowed(pMsgbase, pSubBoardCode))
		keyHelpLines.push("\x01h\x01cDEL              \x01g: \x01n\x01cDelete the current poll (if it's yours)");
	keyHelpLines.push("\x01h\x01cQ                \x01g: \x01n\x01cQuit out of viewing poll results");
	for (var idx = 0; idx < keyHelpLines.length; ++idx)
	{
		console.print("\x01n" + keyHelpLines[idx]);
		console.crlf();
	}
	console.pause();
}

// Displays the program name & version for help screens
function DisplayProgramNameAndVerForHelpScreens()
{
	var progName = "SlyVote";
	var posX = (console.screen_columns/2) - (progName.length/2);
	console.gotoxy(posX, 1);
	console.print("\x01c\x01h" + progName);
	console.gotoxy(posX, 2);
	console.print("\x01k" + strRepeat(CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE, 7));
	var versionText = "\x01n\x01cVersion \x01g" + SLYVOTE_VERSION + "\x01w\x01h (\x01b" + SLYVOTE_DATE + "\x01w)\x01n";
	posX = (console.screen_columns/2) - (strip_ctrl(versionText).length/2);
	console.gotoxy(posX, 3);
	console.print(versionText);
}

// Validates a message if the sub-board requires message validation.
//
// Parameters:
//  pMsgbase: The MsgBase object representing the sub-board
//  pSubBoardCode: The internal code of the sub-board
//  pMsgHdrOrMsgNum: The message header or number
//
// Return value: Boolean - Whether or not validating the message was successful
function ValidateMsg(pMsgbase, pSubBoardCode, pMsgHdrOrMsgNum)
{
	if (!msg_area.sub[pSubBoardCode].is_moderated)
		return true;
	if ((pMsgbase == null) || !pMsgbase.is_open)
		return false;

	var msgNum = 0;
	if (typeof(pMsgHdrOrMsgNum) == "object")
		msgNum = pMsgHdrOrMsgNum.number;
	else if(typeof(pMsgHdrOrMsgNum) == "number")
		pMsgNum = pMsgHdrOrMsgNum;
	var validationSuccessful = false;
	var msgHdr = pMsgbase.get_msg_header(false, msgNum, false);
	if (msgHdr != null)
	{
		if ((msgHdr.attr & MSG_VALIDATED) == 0)
		{
			msgHdr.attr |= MSG_VALIDATED;
			validationSuccessful = pMsgbase.put_msg_header(false, msgHdr.number, msgHdr);
		}
		else
			validationSuccessful = true;
	}

	return validationSuccessful;
}

// Displays an error message on a specified row on the screen with the error delay.
//
// Parameters:
//  pErrorMsg: The error message to display
//  pMessageRow: The row on the screen to display the message on
//  pMnemonicsRequired: Boolean - Whether or not mnemonics is required to display the
//                      message string
function DisplayErrorWithPause(pErrorMsg, pMessageRow, pMnemonicsRequired)
{
	console.gotoxy(1, pMessageRow);
	if (pMnemonicsRequired)
	{
		var errorMessage = pErrorMsg.substr(0, console.screen_columns);
		console.mnemonics(errorMessage);
		mswait(ERROR_PAUSE_WAIT_MS);
		console.gotoxy(1, pMessageRow);
		//printf("\x01n%" + console.screen_columns + "s", "");
		printf("\x01n%" + errorMessage.length + "s", "");
	}
	else
	{
		var errorMessage = "\x01n" + pErrorMsg.substr(0, console.screen_columns);
		console.print("\x01n\x01y\x01h" + errorMessage);
		mswait(ERROR_PAUSE_WAIT_MS);
		console.gotoxy(1, pMessageRow);
		printf("\x01n%" + errorMessage.length + "s", "");
	}
}

// Lets the user view poll stats for a sub-board.
//
// Parameters:
//  pSubBoardCode: The internal code of the sub-board to view stats for
function ViewStats(pSubBoardCode)
{
	var pollRetObj = GetPollHdrs(pSubBoardCode, false, false);
	if (pollRetObj.errorMsg.length > 0)
	{
		console.gotoxy(1, gMessageRow);
		console.print("\x01n\x01h\x01y" + pollRetObj.errorMsg + "\x01n");
		mswait(ERROR_PAUSE_WAIT_MS);
		return;
	}
	if (pollRetObj.msgHdrs.length == 0)
	{
		console.gotoxy(1, gMessageRow);
		console.print("\x01n\x01r\x01yThere are no polls in this area.\x01n");
		mswait(ERROR_PAUSE_WAIT_MS);
		return;
	}

	// Sort the array by number of votes, with the highest first.
	pollRetObj.msgHdrs.sort(function(pA, pB) {
		if (pA.total_votes < pB.total_votes)
			return 1;
		else if (pA.total_votes == pB.total_votes)
			return 0;
		else if (pA.total_votes > pB.total_votes)
			return -1;
	});

	// Create a text message with the poll stats to display in a scrollable
	// frame
	var statsText = "\x01n\x01h\x01bSub-board: \x01c" + msg_area.sub[gSubBoardCode].grp_name + " - " + msg_area.sub[gSubBoardCode].name + "\x01n\r\n";
	statsText += "\x01w\x01hPolls ranked by number of votes (highest to lowest):\x01n\r\n\r\n";
	var labelColor = "\x01w\x01h";
	for (var i = 0; i < pollRetObj.msgHdrs.length; ++i)
	{
		statsText += "\x01n" + labelColor + "Poll " + +(i+1) + "/" + pollRetObj.msgHdrs.length + ": \x01n\x01g" + pollRetObj.msgHdrs[i].subject + "\r\n";
		statsText += "\x01n" + labelColor + "By: \x01n\x01r\x01h" + pollRetObj.msgHdrs[i].from + "\r\n";
		statsText += "\x01n" + labelColor + "Date: \x01n\x01b\x01h" + pollRetObj.msgHdrs[i].date + "\r\n";
		statsText += "\x01n" + labelColor + "Number of votes: \x01n\x01m\x01h" + pollRetObj.msgHdrs[i].total_votes + "\r\n";
		if ((pollRetObj.msgHdrs[i].auxattr & POLL_CLOSED) == POLL_CLOSED)
			statsText += "\x01n\x01y\x01hThis poll is closed for voting.\r\n";
		// Find the option(s) with the highest number(s) of votes
		var tallyIdx = 0;
		var answersAndNumVotes = [];
		for (var fieldI = 0; fieldI < pollRetObj.msgHdrs[i].field_list.length; ++fieldI)
		{
			if (pollRetObj.msgHdrs[i].field_list[fieldI].type == SMB_POLL_ANSWER)
			{
				var answerObj = {
					answerText: pollRetObj.msgHdrs[i].field_list[fieldI].data,
					numVotes: 0
				};
				if (pollRetObj.msgHdrs[i].hasOwnProperty("tally"))
				{
					if (tallyIdx < pollRetObj.msgHdrs[i].tally.length)
						answerObj.numVotes = pollRetObj.msgHdrs[i].tally[tallyIdx];
				}
				answersAndNumVotes.push(answerObj);
				++tallyIdx;
			}
		}
		answersAndNumVotes.sort(function(pA, pB) {
			if (pA.numVotes < pB.numVotes)
				return 1;
			else if (pA.numVotes == pB.numVotes)
				return 0;
			else if (pA.numVotes > pB.numVotes)
				return -1;
		});
		statsText += "\x01n" + labelColor + "Option(s) with highest number of votes:\x01n\r\n";
		for (var answerI = 0; answerI < answersAndNumVotes.length; ++answerI)
		{
			if (answersAndNumVotes[answerI].numVotes == 0)
				continue;
			statsText += "\x01n\x01b\x01h" + answersAndNumVotes[answerI].answerText + "\x01w: \x01m" + answersAndNumVotes[answerI].numVotes + "\x01n\r\n";
			if (answerI > 0)
			{
				if (answersAndNumVotes[answerI].numVotes != answersAndNumVotes[answerI-1].numVotes)
					break;
			}
		}
		statsText += "\r\n";
	}

	// Create a scrollable frame to display the stats in
	var frameAndScrollbar = createFrameWithScrollbar(1, 1, console.screen_columns, console.screen_rows - 1);
	// Put the stats text in the frame
	frameAndScrollbar.frame.putmsg(statsText, "\x01n");
	frameAndScrollbar.frame.scrollTo(0, 0);

	// Create the key help line to be displayed at the bottom of the screen
    var keyText = "\x01rUp\x01b, \x01rDn\x01b, \x01rPgUp\x01b/\x01rDn\x01b, \x01rHome\x01b, \x01rEnd\x01b, \x01rESC\x01b/\x01rQ\x01m)\x01buit";
	var keyHelpLine = "\x01" + "7" + CenterText(keyText, console.screen_columns-1);

	// Prepare the screen and display the key help line on the last row of the screen
	console.clear("\x01n");
	console.gotoxy(1, console.screen_rows);
	console.print(keyHelpLine);

	// User input loop
	var continueOn = true;
	while (continueOn)
	{
		// Do garbage collection to ensure low memory usage
		js.gc(true);

		// Draw the frame
		frameAndScrollbar.frame.invalidate();
		frameAndScrollbar.scrollbar.cycle();
		frameAndScrollbar.frame.cycle();
		frameAndScrollbar.frame.draw();

		// Let the user scroll the message, and take appropriate action based
		// on the user input
		var scrollRetObj = ScrollFrame(frameAndScrollbar.frame, frameAndScrollbar.scrollbar, 0, "\x01n", false, 1, console.screen_rows);
		if (scrollRetObj.lastKeypress == gReaderKeys.quit || scrollRetObj.lastKeypress == KEY_ESC)
			continueOn = false;
	}
}

// Removes newline codes (CR & LF) from a string.
//
// Parameters:
//  pText: The string to remove newline codes from
//
// Return value: The string with newline codes removed
function RemoveCRLFCodes(pText)
{
	return pText.replace("\r\n", "").replace("\n", "").replace("\N", "").replace("\r", "").replace("\R", "").replace("\R\n", "").replace("\r\N", "").replace("\R\N", "");
}

// Counts the number of polls in a sub-board.
//
// Parameters:
//  pSubBoardCode: The internal code of the sub-board to count polls in
//
// Return value: An object containing the following properties:
//               numPolls: The total number of polls in the sub-board
//               numClosedPolls: The number of polls in the sub-board that are closed
//               numPollsUserVotedOn: The number of polls the user has voted on
//               numPollsRemainingForUser: The number of polls remaining for the user
//                                         (numPolls - numPollsUserVotedOn - numClosedPolls, or 0)
function CountPollsInSubBoard(pSubBoardCode)
{
	var retObj = {
		numPolls: 0,
		numClosedPolls: 0,
		numPollsUserVotedOn: 0,
		numPollsRemainingForUser: 0
	};

	if (typeof(pSubBoardCode) !== "string" || pSubBoardCode.length == 0 || !msg_area.sub.hasOwnProperty(pSubBoardCode))
		return retObj;

	var msgbase = new MsgBase(pSubBoardCode);
	if (msgbase.open())
	{
		if (typeof(msgbase.get_index) === "function")
		{
			var msgIndexes = msgbase.get_index();
			if (msgIndexes != null)
			{
				for (var i = 0; i < msgIndexes.length; ++i)
				{
					// Note: IsReadableMsgHdr() checks the 'to' name for unvalidated messages
					// for the sysop if the sub-board requires validation, but when using get_index(),
					// the 'to' field seems to be numbers or undefined, so for now this won't
					// call IsReadableMsgHdr().
					if ((msgIndexes[i] == null) || ((msgIndexes[i].attr & MSG_DELETE) == MSG_DELETE) /*|| !IsReadableMsgHdr(msgHdr, pSubBoardCode)*/)
						continue;
					if ((msgIndexes[i].attr & MSG_POLL) == MSG_POLL)
					{
						++retObj.numPolls;
						if (HasUserVotedOnMsg(msgIndexes[i].number, pSubBoardCode, msgbase, user))
							++retObj.numPollsUserVotedOn;
					}
					var msgHdr = msgbase.get_msg_header(false, msgIndexes[i].number, true);
					if (msgHdr != null)
					{
						if ((msgIndexes[i].auxattr & POLL_CLOSED) == POLL_CLOSED)
							++retObj.numClosedPolls;
					}
				}
			}
		}
		else
		{
			var numMessages = msgbase.total_msgs;
			for (var i = 0; i < numMessages; ++i)
			{
				var msgHdr = msgbase.get_msg_header(true, i);
				if ((msgHdr == null) || ((msgHdr.attr & MSG_DELETE) == MSG_DELETE) || !IsReadableMsgHdr(msgHdr, pSubBoardCode))
					continue;
				if ((msgHdr.attr & MSG_POLL) == MSG_POLL)
				{
					++retObj.numPolls;
					if (HasUserVotedOnMsg(msgHdr.number, pSubBoardCode, msgbase, user))
						++retObj.numPollsUserVotedOn;
				}
				if ((msgHdr.auxattr & POLL_CLOSED) == POLL_CLOSED)
					++retObj.numClosedPolls;
			}
		}

		msgbase.close();

		retObj.numPollsRemainingForUser = retObj.numPolls - retObj.numPollsUserVotedOn - retObj.numClosedPolls;
		if (retObj.numPollsRemainingForUser < 0)
			retObj.numPollsRemainingForUser = 0;
	}
	return retObj;
}

// Increments the numPollsUserVotedOn member in gSubBoardPollCountObj by 1.
// Decrements numPollsRemainingForUser and if it goes below 0, sets it to 0.
function IncrementNumPollsVotedForUser()
{
	++gSubBoardPollCountObj.numPollsUserVotedOn;
	--gSubBoardPollCountObj.numPollsRemainingForUser;
	if (gSubBoardPollCountObj.numPollsRemainingForUser < 0)
		gSubBoardPollCountObj.numPollsRemainingForUser = 0;
}

// Returns whether or not a sub-board has at least one poll in it.
//
// Parameters:
//  pSubBoardCode: The internal code of the sub-board
//
// Return value: Boolean - Whether or not the sub-board has any poolls in it
function subBoardHasPolls(pSubBoardCode)
{
	var pollsExistInSubBoard = false;
	var msgbase = new MsgBase(pSubBoardCode);
	if (msgbase.open())
	{
		// If the new get_index function exists, then use it,
		// since it's faster than iterating through all messages
		// and calling msg_get_index() for each one.
		if (typeof(msgbase.get_index) === "function")
		{
			var msgIndexes = msgbase.get_index();
			if (msgIndexes != null)
			{
				for (var i = msgIndexes.length-1; !pollsExistInSubBoard && (i >= 0); --i)
				{
					if ((msgIndexes[i] == null) || ((msgIndexes[i].attr & MSG_DELETE) == MSG_DELETE))
						continue;
					if ((msgIndexes[i].attr & MSG_POLL) == MSG_POLL)
						pollsExistInSubBoard = true;
				}
			}
		}
		else
		{
			for (var i = msgbase.total_msgs-1; !pollsExistInSubBoard && (i >= 0); --i)
			{
				var msgIndex = msgbase.get_msg_index(true, i);
				if ((msgIndex == null) || ((msgIndex.attr & MSG_DELETE) == MSG_DELETE))
					continue;
				if ((msgIndex.attr & MSG_POLL) == MSG_POLL)
					pollsExistInSubBoard = true;
			}
		}
		
		msgbase.close();
	}
	return pollsExistInSubBoard;
}

// Returns whether a given name matches the logged-in user's handle, alias, or
// name.
//
// Parameters:
//  pName: A name to match against the logged-in user
//
// Return value: Boolean - Whether or not the given name matches the logged-in
//               user's handle, alias, or name
function userHandleAliasNameMatch(pName)
{
	if (typeof(pName) != "string")
		return false;

	var userMatch = false;
	var nameUpper = pName.toUpperCase();
	if (user.handle.length > 0)
		userMatch = (nameUpper.indexOf(user.handle.toUpperCase()) > -1);
	if (!userMatch && (user.alias.length > 0))
		userMatch = (nameUpper.indexOf(user.alias.toUpperCase()) > -1);
	if (!userMatch && (user.name.length > 0))
		userMatch = (nameUpper.indexOf(user.name.toUpperCase()) > -1);
	return userMatch;
}

// Closes a poll, using an existing MessageBase object.
//
// Parameters:
//  pMsgbase: A MessageBase object representing the current sub-board.  It
//            must be open.
//  pMsgNum: The message number (not the index)
//
// Return value: Boolean - Whether or not closing the poll succeeded
function closePollWithOpenMsgbase(pMsgbase, pMsgNum)
{
	var pollClosed = false;
	if ((pMsgbase !== null) && pMsgbase.is_open)
	{
		var userNameOrAlias = user.alias;
		// See if the poll was posted using the user's real name instead of
		// their alias
		var msgHdr = pMsgbase.get_msg_header(false, pMsgNum, false);
		if ((msgHdr != null) && ((msgHdr.attr & MSG_POLL) == MSG_POLL))
		{
			if (msgHdr.from.toUpperCase() == user.name.toUpperCase())
				userNameOrAlias = msgHdr.from;
		}
		// Close the poll (the close_poll() method was added in the Synchronet
		// 3.17 build on August 19, 2017)
		pollClosed = pMsgbase.close_poll(pMsgNum, userNameOrAlias);
	}
	return pollClosed;
}

// Closes a poll.
//
// Parameters:
//  pSubBoardCode: The internal code of the sub-board
//  pMsgNum: The message number (not the index)
//
// Return value: Boolean - Whether or not closing the poll succeeded
function closePoll(pSubBoardCode, pMsgNum)
{
	var pollClosed = false;
	var msgbase = new MsgBase(pSubBoardCode);
	if (msgbase.open())
	{
		pollClosed = closePollWithOpenMsgbase(msgbase, pMsgNum);
		msgbase.close();
	}
	return pollClosed;
}

// Inputs a string from the user, restricting their input to certain keys (optionally).
//
// Parameters:
//  pKeys: A string containing valid characters for input.  Optional
//  pMaxNumChars: The maximum number of characters to input.  Optional
//  pCaseSensitive: Boolean - Whether or not the input should be case-sensitive.  Optional.
//                  Defaults to true.  If false, then the user input will be uppercased.
//
// Return value: A string containing the user's input
function consoleGetStrWithValidKeys(pKeys, pMaxNumChars, pCaseSensitive)
{
	var maxNumChars = 0;
	if ((typeof(pMaxNumChars) == "number") && (pMaxNumChars > 0))
		maxNumChars = pMaxNumChars;

	var regexPattern = (typeof(pKeys) == "string" ? "[" + pKeys + "]" : ".");
	var caseSensitive = (typeof(pCaseSensitive) == "boolean" ? pCaseSensitive : true);
	var regex = new RegExp(regexPattern, (caseSensitive ? "" : "i"));

	var CTRL_H = "\x08";
	var BACKSPACE = CTRL_H;
	var CTRL_M = "\x0d";
	var KEY_ENTER = CTRL_M;

	var modeBits = (caseSensitive ? K_NONE : K_UPPER);
	var userInput = "";
	var continueOn = true;
	while (continueOn)
	{
		var userChar = console.getkey(K_NOECHO|modeBits);
		if (regex.test(userChar) && isPrintableChar(userChar))
		{
			var appendChar = true;
			if ((maxNumChars > 0) && (userInput.length >= maxNumChars))
				appendChar = false;
			if (appendChar)
			{
				userInput += userChar;
				if ((modeBits & K_NOECHO) == 0)
					console.print(userChar);
			}
		}
		else if (userChar == BACKSPACE)
		{
			if (userInput.length > 0)
			{
				if ((modeBits & K_NOECHO) == 0)
				{
					console.print(BACKSPACE);
					console.print(" ");
					console.print(BACKSPACE);
				}
				userInput = userInput.substr(0, userInput.length-1);
			}
		}
		else if (userChar == KEY_ENTER)
		{
			continueOn = false;
			if ((modeBits & K_NOCRLF) == 0)
				console.crlf();
		}
	}
	return userInput;
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
function createFrameWithScrollbar(pX, pY, pWidth, pHeight)
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

// Returns a string repeated a number of times
//
// Parameters:
//  pStr: The string to be repeated (string)
//  pNumTimes: The number of times to repeat the string (number)
//
// Return value: The string, repeated the number of times given
function strRepeat(pStr, pNumTimes)
{
	if (typeof(pStr) !== "string" || typeof(pNumTimes) !== "number")
		return "";
	if (pStr.length == 0 || pNumTimes < 1)
		return "";

	var repeatedStr = "";
	for (var i = 0; i < pNumTimes; ++i)
		repeatedStr += pStr;
	return repeatedStr;
}

// When using text strings from text.dat, for some reason, strings that use attribute
// normal followed by high, which should be white, appear as high black.  This function
// fixes that.
function processBBSTextDatText(pText)
{
	if (typeof(pText) !== "string")
		return "";

	return pText.replace(/\x01n\x01h/g, "\x01n\x01w\x01h");
}
