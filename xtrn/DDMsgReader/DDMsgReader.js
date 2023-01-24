/* This is a message reader/lister door for Synchronet.  Features include:
 * - Listing messages in the user's current message area with the ability to
 *   navigate forwards & backwards through the list (and for ANSI users, a
 *   lightbar interface will be used, or optionally can be set to use a more
 *   traditional interface for ANSI users)
 * - The user can select a message from the list to read and optionally reply to
 * - For ANSI users, reading messages is done with an enhanced user interface,
 *   with the ability to scroll up & down through the message, move to the next
 *   or previous message using the right & left arrow keys, display the message
 *   list to choose another message to read, etc.
 * - The ability to start up with the message list or reading messages in the
 *   user's current message area (AKA sub-board)
 * - Message searching
 *
 * Author: Eric Oulashin (AKA Nightfox)
 * BBS: Digital Distortion
 * BBS address: digitaldistortionbbs.com (or digdist.bbsindex.com)
 *
 * Date       Author            Description
 * 2014-09-13 Eric Oulashin     Started (based on my message lister script)
 * ... Comments trimmed ...
 * 2022-03-14 Eric Oulashin     Version 1.47
 *                              Updated to make DDMsgReader can be called directly as a
 *                              loadable module by Synchronet (work started on March 8).
 *                              Also, refactored to use attr_conv.js and removed the
 *                              attribute conversion functions from this script.
 * 2022-03-23 Eric Oulashin     Version 1.47a
 *                              Now calls bbs.edit_msg() to edit an existing message (if
 *                              that function exists - It was added in Synchronet 3.18).
 * 2022-06-12 Eric Oulashin     Version 1.48
 *                              Improved display of ANSI messages via the use of the Graphic object
 * 2022-06-13 Eric Oulashin     Version 1.49
 *                              Refactor: Simplified saving a message to BBS machine for sysop
 *                              (as-is, less processing); removed attachment stuff for pre-Synchronet
 *                              3.17; moved hasSyncAttrCodes() to attr_conv.js because that's where it
 *                              needs to be.
 * 2022-06-13 Eric Oulashin     Version 1.50
 *                              When doing a text search, it now ignores the user scan configuration for
 *                              sub-boards, to ensure it will show any results of the text search.
 * 2022-07-05 Eric Oulashin     Version 1.51
 *                              Graphic is now only used when using the scrollable interface. Also,
 *                              when creating the Graphic, now subtracting 1 from the reading area height
 *                              to avoid making the Graphic one line too tall to avoid unnecessary scrolling.
 *                              When saving messages with ANSI codes, Graphic is only used if the message has
 *                              any ASCII drawing characters. (not sure if this really matters much though).
 *                              Also, applied "use strict" and made some changes as necessary.
 * 2022-07-09 Eric Oulashin     Version 1.52
 *                              Mouse click support for the bottom help lines in scrollable mode
 *                              (thanks to help from Nelgin)
 * 2022-07-18 Eric Oulashin     Version 1.53
 *                              Deleted messages can now be un-marked for deletion from the message
 *                              list with the U key (if the user has delete permissions). Also, the reader now
 *                              honors the system setting for whether users can view deleted messages.
 * 2022-08-06 Eric Oulashin     Version 1.54
 *                              Users now have a personal twit list (configurable via Ctrl-U, user settings).
 * 2022-09-23 Eric Oulashin     Version 1.55
 *                              Refactored how email replies are done (passing the header to the appropriate
 *                              functions, not using ungetstr() when prompting for the message subject)
 * 2022-11-25 Eric Oulashin     Version 1.56
 *                              Fixed bug startup mode for scanning all groups for un-read messages to you where
 *                              the reader was bringing up personal email instead.
 * 2022-12-02 Eric Oulashin     Version 1.57
 *                              @-codes were only expanded when reading personal mail; now, DDMsgReader
 *                              also checks to make sure the sender is a sysop.  Also, used putmsg() in
 *                              place of this script's own @-message parsing when displaying some of the
 *                              configured text strings.
 * 2022-12-12 Eric Oulashin     Fix for "assignment to undeclared variable" error in GetMsgSubBrdLine();
 *                              appeared when changing to a different message area from the reader
 * 2012-12-14 Eric Oulashin     Version 1.58
 *                              When writing QUOTES.TXT, quote lines are now wrapped if the user's
 *                              external editor configuration is configured to do so.
 * 2022-12-29 Eric Oulashin     Version 1.59
 *                              For Synchronet above 3.20, read the external editor quote wrap setting
 *                              from xtrn.ini.  Below version 3.20, read it from xtrn.cnf.
 *                              Also, there's a new user setting to toggle whether or not to use the scrollbar
 *                              in the scrolling reader. Currently there is no alternate progress displayed
 *                              if not using the scrollbar, but that is planned for a future update.
 * 2023-01-20 Eric Oulashin     Version 1.60
 *                              DDMsgReader can now optionally convert Y-style MCI attribute codes to
 *                              to Synchronet attribute codes, with the new configuration setting
 *                              convertYStyleMCIAttrsToSync (true/false). Requires the updated attr_conv.js
 *                              in sbbs/exec/load.
 * 2023-01-22 Eric Oulashin     Version 1.61
 *                              Fix: When replying to an email with an unknown sender (empty),
 *                              no longer gives the error "Invalid user field: 0"; also, if the sender is
 *                              unknown, prompts the user for a user name/number/email address to send
 *                              the reply to.
 */

"use strict";

// TODO: In the message list, add the ability to search with / similar to my area chooser

/* Command-line arguments (in -arg=val format, or -arg format to enable an
   option):
   -search: A search type.  Available options:
            keyword_search: Do a keyword search in message subject/body text (current message area)
            from_name_search: 'From' name search (current message area)
            to_name_search: 'To' name search (current message area)
            to_user_search: To user search (current message area)
            new_msg_scan: New message scan (prompt for current sub-board, current
                          group, or all)
            new_msg_scan_all: New message scan (all sub-boards)
			new_msg_scan_cur_grp: New message scan (current message group only)
            new_msg_scan_cur_sub: New message scan (current sub-board only).  This
			                      can (optionally) be used with the -subBoard
								  command-line parameter, which specifies an internal
								  code for a sub-board, which may be different from
								  the user's currently selected sub-board.
			to_user_new_scan: Scan for new (unread) messages to the user (prompt
			                  for current sub-board, current group, or all)
			to_user_new_scan_all: Scan for new (unread) messages to the user
			                      (all sub-boards)
			to_user_new_scan_cur_grp: Scan for new (unread) messages to the user
			                          (current group)
            to_user_new_scan_cur_sub: Scan for new (unread) messages to the user
			                          (current sub-board)
			to_user_all_scan: Scan for all messages to the user (prompt for current
			                  sub-board, current group, or all)
            prompt: Prompt the user for one of several search/scan options to
                    choose from
        Note that if the -personalEmail option is specified (to read personal
        email), the only valid search types are keyword_search and
        from_name_search.
   -suppressSearchTypeText: Disable the search type text that would appear
                            above searches or scans (such as "New To You
                            Message Scan", etc.)
   -startMode: Startup mode.  Available options:
               list (or lister): Message list mode
               read (or reader): Message read mode
   -configFilename: Specifies the name of the configuration file to use.
                    Defaults to DDMsgReader.cfg.
   -personalEmail: Read personal email to the user.  This is a true/false value.
                   It doesn't need to explicitly have a =true or =false afterward;
                   simply including -personalEmail will enable it.  If this is specified,
				   the -chooseAreaFirst and -subBoard options will be ignored.
   -personalEmailSent: Read personal email to the user.  This is a true/false
                       value.  It doesn't need to explicitly have a =true or =false
                       afterward; simply including -personalEmailSent will enable it.
   -allPersonalEmail: Read all personal email (to/from all)
   -userNum: Specify a user number (for the personal email options)
   -chooseAreaFirst: Display the message area chooser before reading/listing
                     messages.  This is a true/false value.  It doesn't need
                     to explicitly have a =true or =false afterward; simply
                     including -chooseAreaFirst will enable it.  If -personalEmail
					 or -subBoard is specified, then this option won't have any
                     effect.
	-subBoard: The sub-board (internal code or number) to read, other than the user's
               current sub-board. If this is specified, the -chooseAreaFirst option
			   will be ignored.
	-verboseLogging: Enable logging to the system log & node log.  Currently, there
	                 isn't much that will be logged, but more log messages could be
					 added in the future.
*/

// - For pageUp & pageDown, enable alternate keys:
//  - When reading a message - scrollTextLines()
//  - When listing messages
//  - When listing message groups & sub-boards for sub-board selection
// - For sub-board area search:
//  - Enable searching in traditional interface
//  - Update the keys in the lightbar help line and traditional interface

// This script requires Synchronet version 3.18 or higher (for mouse hotspot support).
// Exit if the Synchronet version is below the minimum.
if (system.version_num < 31800)
{
	var message = "\x01n\x01h\x01y\x01i* Warning:\x01n\x01h\x01w Digital Distortion Message Reader "
	             + "requires version \x01g3.18\x01w or\r\n"
	             + "higher of Synchronet.  This BBS is using version \x01g" + system.version
	             + "\x01w.  Please notify the sysop.";
	console.crlf();
	console.print(message);
	console.crlf();
	console.pause();
	exit();
}

require("sbbsdefs.js", "K_UPPER");
require("text.js", "Email"); // Text string definitions (referencing text.dat)
require("utf8_cp437.js", "utf8_cp437");
require("userdefs.js", "USER_UTF8");
require("dd_lightbar_menu.js", "DDLightbarMenu");
require("html2asc.js", 'html2asc');
require("attr_conv.js", "convertAttrsToSyncPerSysCfg");
require("graphic.js", 'Graphic');
load('822header.js');
var ansiterm = require("ansiterm_lib.js", 'expand_ctrl_a');


// Reader version information
var READER_VERSION = "1.61";
var READER_DATE = "2023-01-22";

// Keyboard key codes for displaying on the screen
var UP_ARROW = ascii(24);
var DOWN_ARROW = ascii(25);
var LEFT_ARROW = ascii(17);
var RIGHT_ARROW = ascii(16);
// Ctrl keys for input
var CTRL_A = "\x01";
var CTRL_B = "\x02";
var CTRL_C = "\x03";
var CTRL_D = "\x04";
var CTRL_E = "\x05";
var CTRL_F = "\x06";
var CTRL_G = "\x07";
var BEEP = CTRL_G;
var CTRL_H = "\x08";
var BACKSPACE = CTRL_H;
var CTRL_I = "\x09";
var TAB = CTRL_I;
var CTRL_J = "\x0a";
var CTRL_K = "\x0b";
var CTRL_L = "\x0c";
var CTRL_M = "\x0d";
var CTRL_N = "\x0e";
var CTRL_O = "\x0f";
var CTRL_P = "\x10";
var CTRL_Q = "\x11";
var XOFF = CTRL_Q;
var CTRL_R = "\x12";
var CTRL_S = "\x13";
var XON = CTRL_S;
var CTRL_T = "\x14";
var CTRL_U = "\x15";
var CTRL_V = "\x16";
var KEY_INSERT = CTRL_V;
var CTRL_W = "\x17";
var CTRL_X = "\x18";
var CTRL_Y = "\x19";
var CTRL_Z = "\x1a";
//var KEY_ESC = "\x1b";
var KEY_ESC = ascii(27);
var KEY_ENTER = CTRL_M;
// PageUp & PageDown keys - Synchronet 3.17 as of about December 18, 2017
// use CTRL-P and CTRL-N for PageUp and PageDown, respectively.  sbbsdefs.js
// defines them as KEY_PAGEUP and KEY_PAGEDN; I've used slightly different names
// in this script so that this script will work with Synchronet systems before
// and after the update containing those key definitions.
var KEY_PAGE_UP = CTRL_P;
var KEY_PAGE_DOWN = CTRL_N;
// Ensure KEY_PAGE_UP and KEY_PAGE_DOWN are set to what's defined in sbbs.js
// for KEY_PAGEUP and KEY_PAGEDN in case they change
if (typeof(KEY_PAGEUP) === "string")
	KEY_PAGE_UP = KEY_PAGEUP;
if (typeof(KEY_PAGEDN) === "string")
	KEY_PAGE_DOWN = KEY_PAGEDN;
	
// These are defined in sbbsdefs.js:
//var	  KEY_UP		='\x1e';	// ctrl-^ (up arrow)
//var	  KEY_DOWN		='\x0a';	// ctrl-j (dn arrow)
//var   KEY_RIGHT		='\x06';	// ctrl-f (rt arrow)
//var	  KEY_LEFT		='\x1d';	// ctrl-] (lf arrow)
//var	  KEY_HOME		='\x02';	// ctrl-b (home)
//var   KEY_END       ='\x05';	// ctrl-e (end)
//var   KEY_DEL       ='\x7f';    // (del)
// These were added to sbbsdef.js around December 17, 2017:
//var		KEY_PAGEUP	='\x10';	/* ctrl-p (Page Up)							*/
//var		KEY_PAGEDN	='\x0e';	/* ctrl-n (Page Down)						*/

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


const ERROR_PAUSE_WAIT_MS = 1500;

// Reader mode definitions:
const READER_MODE_LIST = 0;
const READER_MODE_READ = 1;
// Search types
const SEARCH_NONE = -1;
const SEARCH_KEYWORD = 2;
const SEARCH_FROM_NAME = 3;
const SEARCH_TO_NAME_CUR_MSG_AREA = 4;
const SEARCH_TO_USER_CUR_MSG_AREA = 5;
const SEARCH_MSG_NEWSCAN = 6;
const SEARCH_MSG_NEWSCAN_CUR_SUB = 7;
const SEARCH_MSG_NEWSCAN_CUR_GRP = 8;
const SEARCH_MSG_NEWSCAN_ALL = 9;
const SEARCH_TO_USER_NEW_SCAN = 10;
const SEARCH_TO_USER_NEW_SCAN_CUR_SUB = 11;
const SEARCH_TO_USER_NEW_SCAN_CUR_GRP = 12;
const SEARCH_TO_USER_NEW_SCAN_ALL = 13;
const SEARCH_ALL_TO_USER_SCAN = 14;

// Message threading types
const THREAD_BY_ID = 15;
const THREAD_BY_TITLE = 16;
const THREAD_BY_AUTHOR = 17;
const THREAD_BY_TO_USER = 18;

// Reader mode - Actions
const ACTION_NONE = 19;
const ACTION_GO_NEXT_MSG = 20;
const ACTION_GO_PREVIOUS_MSG = 21;
const ACTION_GO_SPECIFIC_MSG = 22;
const ACTION_GO_FIRST_MSG = 23;
const ACTION_GO_LAST_MSG = 24;
const ACTION_DISPLAY_MSG_LIST = 25;
const ACTION_CHG_MSG_AREA = 26;
const ACTION_GO_PREV_MSG_AREA = 27;
const ACTION_GO_NEXT_MSG_AREA = 28;
const ACTION_QUIT = 29;

// Definitions for help line refresh parameters for error functions
const REFRESH_MSG_AREA_CHG_LIGHTBAR_HELP_LINE = 0;

// Message list sort types
const MSG_LIST_SORT_DATETIME_RECEIVED = 0;
const MSG_LIST_SORT_DATETIME_WRITTEN = 1;

// Misc. defines
var ERROR_WAIT_MS = 1500;
var SEARCH_TIMEOUT_MS = 10000;

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
	MSG_KILLFILE: "KillFile",
	MSG_RECEIPTREQ: "RctReq",
	MSG_CONFIRMREQ: "ConfReq",
	MSG_NODISP: "NoDisp"
};
if (typeof(MSG_TRUNCFILE) != "undefined")
	gAuxMsgAttrStrs.MSG_TRUNCFILE = "TruncFile";
var gNetMsgAttrStrs = {
	MSG_LOCAL: "FromLocal",
	MSG_INTRANSIT: "Transit",
	MSG_SENT: "Sent",
	MSG_KILLSENT: "KillSent",
	MSG_ARCHIVESENT: "ArcSent",
	MSG_HOLD: "Hold",
	MSG_CRASH: "Crash",
	MSG_IMMEDIATE: "Now",
	MSG_DIRECT: "Direct"
};
if (typeof(MSG_GATE) != "undefined")
	gNetMsgAttrStrs.MSG_GATE = "Gate";
if (typeof(MSG_ORPHAN) != "undefined")
	gNetMsgAttrStrs.MSG_ORPHAN = "Orphan";
if (typeof(MSG_FPU) != "undefined")
	gNetMsgAttrStrs.MSG_FPU = "FPU";
if (typeof(MSG_TYPELOCAL) != "undefined")
	gNetMsgAttrStrs.MSG_TYPELOCAL = "ForLocal";
if (typeof(MSG_TYPEECHO) != "undefined")
	gNetMsgAttrStrs.MSG_TYPEECHO = "ForEcho";
if (typeof(MSG_TYPENET) != "undefined")
	gNetMsgAttrStrs.MSG_TYPENET = "ForNetmail";
if (typeof(MSG_MIMEATTACH) != "undefined")
	gNetMsgAttrStrs.MSG_MIMEATTACH = "MimeAttach";

// A regular expression to check whether a string is an email address
var gEmailRegex = /^(([^<>()\[\]\\.,;:\s@"]+(\.[^<>()\[\]\\.,;:\s@"]+)*)|(".+"))@((\[[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}])|(([a-zA-Z\-0-9]+\.)+[a-zA-Z]{2,}))$/;
// A regular expression to check whether a string is a FidoNet email address
var gFTNEmailRegex = /^.*@[0-9]+:[0-9]+\/[0-9]+$/;
// An array of regular expressions for checking for ANSI codes (globally in a string & ignore case)
var gANSIRegexes = [ new RegExp(ascii(27) + "\[[0-9]+[mM]", "gi"),
                     new RegExp(ascii(27) + "\[[0-9]+(;[0-9]+)+[mM]", "gi"),
                     new RegExp(ascii(27) + "\[[0-9]+[aAbBcCdD]", "gi"),
                     new RegExp(ascii(27) + "\[[0-9]+;[0-9]+[hHfF]", "gi"),
                     new RegExp(ascii(27) + "\[[sSuUkK]", "gi"),
                     new RegExp(ascii(27) + "\[2[jJ]", "gi") ];

// Determine the script's startup directory.
// This code is a trick that was created by Deuce, suggested by Rob Swindell
// as a way to detect which directory the script was executed in.  I've
// shortened the code a little.
var gStartupPath = '.';
try { throw dig.dist(dist); } catch(e) { gStartupPath = e.fileName; }
gStartupPath = backslash(gStartupPath.replace(/[\/\\][^\/\\]*$/,''));

// See if we're running in Windows or not.  Until early 2015, the word_wrap()
// function seemed to have a bug where the wrapping length in Linux was one
// less than what it uses in Windows).  That seemed to be fixed in one of the
// Synchronet 3.16 builds in early 2015.
var gRunningInWindows = /^WIN/.test(system.platform.toUpperCase());

// Temporary directory (in the logged-in user's node directory) to store
// file attachments, etc.
var gFileAttachDir = backslash(system.node_dir + "DDMsgReader_Attachments");
// If the temporary attachments directory exists, then delete it (in case the last
// user hung up while running this script, etc.)
if (file_exists(gFileAttachDir))
	deltree(gFileAttachDir);

// See if the avatar support files are available, and load them if so
var gAvatar = null;
if (file_exists(backslash(system.exec_dir) + "load/smbdefs.js") && file_exists(backslash(system.exec_dir) + "load/avatar_lib.js"))
{
	require("smbdefs.js", "SMB_POLL_ANSWER");
	gAvatar = load({}, "avatar_lib.js");
}

// User twitlist filename (and settings filename)
var gUserTwitListFilename = backslash(system.data_dir + "user") + format("%04d", user.number) + ".DDMsgReader_twitlist";
var gUserSettingsFilename = backslash(system.data_dir + "user") + format("%04d", user.number) + ".DDMsgReader_Settings";

/////////////////////////////////////////////
// Script execution code

// Parse the command-line arguments

var gCmdLineArgVals = parseArgs(argv);
if (gCmdLineArgVals.exitNow)
	exit(0);
var gAllPersonalEmailOptSpecified = (gCmdLineArgVals.hasOwnProperty("allpersonalemail") && gCmdLineArgVals.allpersonalemail);
// Check to see if the command-line argument for reading personal email is enabled
var gListPersonalEmailCmdLineOpt = ((gCmdLineArgVals.hasOwnProperty("personalemail") && gCmdLineArgVals.personalemail) ||
                                    (gCmdLineArgVals.hasOwnProperty("personalemailsent") && gCmdLineArgVals.personalemailsent) ||
									gAllPersonalEmailOptSpecified);
// If the command-line parameter "search" is specified as "prompt", then
// prompt the user for the type of search to perform.
var gDoDDMR = true; // If the user doesn't choose a search type, this will be set to false
if (gCmdLineArgVals.hasOwnProperty("search") && (gCmdLineArgVals["search"].toLowerCase() == "prompt"))
{
	console.print("\x01n");
	console.crlf();
	console.print("\x01cMessage search:");
	console.crlf();
	var allowedKeys = "";
	if (!gListPersonalEmailCmdLineOpt)
	{
		allowedKeys = "ANKFTYUS";
		console.print(" \x01g\x01hN\x01y = \x01n\x01cNew message scan");
		console.crlf();
		console.print(" \x01g\x01hK\x01y = \x01n\x01cKeyword");
		console.crlf();
		console.print(" \x01h\x01gF\x01y = \x01n\x01cFrom name");
		console.crlf();
		console.print(" \x01h\x01gT\x01y = \x01n\x01cTo name");
		console.crlf();
		console.print(" \x01h\x01gY\x01y = \x01n\x01cTo you");
		console.crlf();
		console.print(" \x01h\x01gU\x01y = \x01n\x01cUnread (new) messages to you");
		console.crlf();
		console.print(" \x01h\x01gS\x01y = \x01n\x01cScan for msgs to you");
		console.crlf();
	}
	else
	{
		// Reading personal email - Allow fewer choices
		allowedKeys = "KF";
		console.print(" \x01g\x01hK\x01y = \x01n\x01cKeyword");
		console.crlf();
		console.print(" \x01h\x01gF\x01y = \x01n\x01cFrom name");
		console.crlf();
	}
	console.print(" \x01h\x01gA\x01y = \x01n\x01cAbort");
	console.crlf();
	console.print("\x01n\x01cMake a selection\x01g\x01h: \x01c");
	// TODO: Check to see if keyword & from name search work when reading
	// personal email
	switch (console.getkeys(allowedKeys))
	{
		case "N":
			gCmdLineArgVals["search"] = "new_msg_scan";
			break;
		case "K":
			gCmdLineArgVals["search"] = "keyword_search";
			break;
		case "F":
			gCmdLineArgVals["search"] = "from_name_search";
			break;
		case "T":
			gCmdLineArgVals["search"] = "to_name_search";
			break;
		case "Y":
			gCmdLineArgVals["search"] = "to_user_search";
			break;
		case "U":
			gCmdLineArgVals["search"] = "to_user_new_scan";
			break;
		case "S":
			gCmdLineArgVals["search"] = "to_user_all_scan";
			break;
		case "A": // Abort
		default:
			gDoDDMR = false;
			console.print("\x01n\x01h\x01y\x01iAborted\x01n");
			console.crlf();
			console.pause();
			break;
	}
}

if (gDoDDMR)
{
	// Write the user's default twitlist if it doesn't already exist
	writeDefaultUserTwitListIfNotExist();

	// When exiting this script, make sure to set the ctrl key pasthru back to what it was originally
	js.on_exit("console.ctrlkey_passthru = " + console.ctrlkey_passthru);
	// Set a control key pass-thru so we can capture certain control keys that we normally wouldn't be able to
	var gOldCtrlKeyPassthru = console.ctrlkey_passthru; // Backup to be restored later
	console.ctrlkey_passthru = "+ACGKLOPQRTUVWXYZ_";

	// Create an instance of the DigDistMsgReader class and use it to read/list the
	// messages in the user's current sub-board.  Pass the parsed command-line
	// argument values object to its constructor.
	var readerSubCode = (gListPersonalEmailCmdLineOpt ? "mail" : bbs.cursub_code);
	// If the -subBoard option was specified and the "read personal email" option was
	// not specified, then use the sub-board specified by the -subBoard command-line
	// option.
	if (gCmdLineArgVals.hasOwnProperty("subboard") && !gListPersonalEmailCmdLineOpt)
	{
		// If the specified sub-board option is all digits, then treat it as the
		// sub-board number.  Otherwise, treat it as an internal sub-board code.
		if (/^[0-9]+$/.test(gCmdLineArgVals["subboard"]))
			readerSubCode = getSubBoardCodeFromNum(Number(gCmdLineArgVals["subboard"]));
		else
			readerSubCode = gCmdLineArgVals["subboard"];
	}
	var msgReader = new DigDistMsgReader(readerSubCode, gCmdLineArgVals);
	if (gCmdLineArgVals.indexedmode)
	{
		msgReader.DoIndexedMode();
	}
	else
	{
		// If the option to choose a message area first was enabled on the command-line
		// (and neither the -subBoard nor the -personalEmail options were specified),
		// then let the user choose a sub-board now.
		if (gCmdLineArgVals.hasOwnProperty("chooseareafirst") && gCmdLineArgVals["chooseareafirst"] && !gCmdLineArgVals.hasOwnProperty("subboard") && !gListPersonalEmailCmdLineOpt)
			msgReader.SelectMsgArea();
		// Back up the user's current sub-board so that we can change back
		// to it after searching is done, if a search is done.
		var originalMsgGrpIdx = bbs.curgrp;
		var originalSubBoardIdx = bbs.cursub;
		var restoreOriginalSubCode = true;
		// Based on the reader's start mode/search type, do the appropriate thing.
		switch (msgReader.searchType)
		{
			case SEARCH_NONE:
				restoreOriginalSubCode = false;
				if (msgReader.subBoardCode != "mail")
				{
					console.print("\x01n");
					console.crlf();
					console.print("Loading " + subBoardGrpAndName(msgReader.subBoardCode) + "....");
					console.line_counter = 0; // To prevent a pause before the message list comes up
				}
				msgReader.ReadOrListSubBoard();
				break;
			case SEARCH_KEYWORD:
				var txtToSearch = (gCmdLineArgVals.hasOwnProperty("searchtext") ? gCmdLineArgVals.searchtext : null);
				var subBoardCode = (gCmdLineArgVals.hasOwnProperty("subboard") ? gCmdLineArgVals.subboard : null);
				msgReader.SearchMsgScan("keyword_search", txtToSearch, subBoardCode);
				break;
			case SEARCH_FROM_NAME:
				msgReader.SearchMessages("from_name_search");
				break;
			case SEARCH_TO_NAME_CUR_MSG_AREA:
				msgReader.SearchMessages("to_name_search");
				break;
			case SEARCH_TO_USER_CUR_MSG_AREA:
				msgReader.SearchMessages("to_user_search");
				break;
			case SEARCH_MSG_NEWSCAN:
				if (!gCmdLineArgVals.suppresssearchtypetext)
				{
					console.crlf();
					console.putmsg(msgReader.text.newMsgScanText);
				}
				msgReader.MessageAreaScan(SCAN_CFG_NEW, SCAN_NEW);
				break;
			case SEARCH_MSG_NEWSCAN_CUR_SUB:
				msgReader.MessageAreaScan(SCAN_CFG_NEW, SCAN_NEW, "S");
				break;
			case SEARCH_MSG_NEWSCAN_CUR_GRP:
				msgReader.MessageAreaScan(SCAN_CFG_NEW, SCAN_NEW, "G");
				break;
			case SEARCH_MSG_NEWSCAN_ALL:
				msgReader.MessageAreaScan(SCAN_CFG_NEW, SCAN_NEW, "A");
				break;
			case SEARCH_TO_USER_NEW_SCAN:
				if (!gCmdLineArgVals.suppresssearchtypetext)
				{
					console.crlf();
					console.putmsg(msgReader.text.newToYouMsgScanText);
				}
				msgReader.MessageAreaScan(SCAN_CFG_TOYOU/*SCAN_CFG_YONLY*/, SCAN_UNREAD);
				break;
			case SEARCH_TO_USER_NEW_SCAN_CUR_SUB:
				msgReader.MessageAreaScan(SCAN_CFG_TOYOU/*SCAN_CFG_YONLY*/, SCAN_UNREAD, "S");
				break;
			case SEARCH_TO_USER_NEW_SCAN_CUR_GRP:
				msgReader.MessageAreaScan(SCAN_CFG_TOYOU/*SCAN_CFG_YONLY*/, SCAN_UNREAD, "G");
				break;
			case SEARCH_TO_USER_NEW_SCAN_ALL:
				msgReader.MessageAreaScan(SCAN_CFG_TOYOU/*SCAN_CFG_YONLY*/, SCAN_UNREAD, "A");
				break;
			case SEARCH_ALL_TO_USER_SCAN:
				if (!gCmdLineArgVals.suppresssearchtypetext)
				{
					console.crlf();
					console.putmsg(msgReader.text.allToYouMsgScanText);
				}
				msgReader.MessageAreaScan(SCAN_CFG_TOYOU, SCAN_TOYOU);
				break;
		}

		// If we should restore the user's original message area, then do so.
		if (restoreOriginalSubCode)
		{
			bbs.cursub = 0;
			bbs.curgrp = originalMsgGrpIdx;
			bbs.cursub = originalSubBoardIdx;
		}
	}

	// Remove the temporary attachments & ANSI temp directories if they exists
	deltree(gFileAttachDir);
	deltree(backslash(system.node_dir + "DDMsgReaderANSIMsgTemp"));

	// Before this script finishes, make sure the terminal attributes are set back
	// to normal (in case there are any attributes left on, such as background,
	// blink, etc.)
	console.attributes = "N";
}

// End of script execution.  Functions below:

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
	var toHdrLine = "\x01n\x01h\x01k" + VERTICAL_SINGLE + BLOCK1 + BLOCK2 + BLOCK3
		          + "\x01gT\x01n\x01go  \x01h\x01c: " +
		          (pToReadingUser ? pColors.msgHdrToUserColor : pColors.msgHdrToColor) +
		          "@MSG_TO-L";
	var numChars = console.screen_columns - 21;
	for (var i = 0; i < numChars; ++i)
		toHdrLine += "#";
	toHdrLine += "@\x01k" + VERTICAL_SINGLE;
	return toHdrLine;
}

///////////////////////////////////////////////////////////////////////////////////
// DigDistMsgReader class stuff

// DigDistMsgReader class constructor: Constructs a
// DigDistMsgReader object, to be used for listing messages
// in a message area.
//
// Parameters:
//  pSubBoardCode: Optional - The Synchronet sub-board code, or "mail"
//                 for personal email.
//  pScriptArgs: Optional - An object containing key/value pairs representing
//               the command-line arguments & values, as returned by parseArgs().
function DigDistMsgReader(pSubBoardCode, pScriptArgs)
{
	// Set the methods for the object
	this.setSubBoardCode = DigDistMsgReader_SetSubBoardCode;
	this.RecalcMsgListWidthsAndFormatStrs = DigDistMsgReader_RecalcMsgListWidthsAndFormatStrs;
	this.NumMessages = DigDistMsgReader_NumMessages;
	this.SearchingAndResultObjsDefinedForCurSub = DigDistMsgReader_SearchingAndResultObjsDefinedForCurSub;
	this.PopulateHdrsForCurrentSubBoard = DigDistMsgReader_PopulateHdrsForCurrentSubBoard;
	this.FilterMsgHdrsIntoHdrsForCurrentSubBoard = DigDistMsgReader_FilterMsgHdrsIntoHdrsForCurrentSubBoard;
	this.GetMsgIdx = DigDistMsgReader_GetMsgIdx;
	this.RefreshSearchResultMsgHdr = DigDistMsgReader_RefreshSearchResultMsgHdr;   // Refreshes a message header in the search results
	this.SearchMessages = DigDistMsgReader_SearchMessages; // Prompts the user for search text, then lists/reads messages, performing the search
	this.SearchMsgScan = DigDistMsgReader_SearchMsgScan;
	this.RefreshHdrInSubBoardHdrs = DigDistMsgReader_RefreshHdrInSubBoardHdrs;
	this.RefreshHdrInSavedArrays = DigDistMsgReader_RefreshHdrInSavedArrays;
	this.ReadMessages = DigDistMsgReader_ReadMessages;
	this.DisplayEnhancedMsgReadHelpLine = DigDistMsgReader_DisplayEnhancedMsgReadHelpLine;
	this.GoToPrevSubBoardForEnhReader = DigDistMsgReader_GoToPrevSubBoardForEnhReader;
	this.GoToNextSubBoardForEnhReader = DigDistMsgReader_GoToNextSubBoardForEnhReader;
	this.SetUpTraditionalMsgListVars = DigDistMsgReader_SetUpTraditionalMsgListVars;
	this.SetUpLightbarMsgListVars = DigDistMsgReader_SetUpLightbarMsgListVars;
	this.ListMessages = DigDistMsgReader_ListMessages;
	this.ListMessages_Traditional = DigDistMsgReader_ListMessages_Traditional;
	this.ListMessages_Lightbar = DigDistMsgReader_ListMessages_Lightbar;
	this.CreateLightbarMsgListMenu = DigDistMsgReader_CreateLightbarMsgListMenu;
	this.CreateLightbarMsgGrpMenu = DigDistMsgReader_CreateLightbarMsgGrpMenu;
	this.CreateLightbarSubBoardMenu = DigDistMsgReader_CreateLightbarSubBoardMenu;
	this.AdjustLightbarMsgListMenuIdxes = DigDistMsgReader_AdjustLightbarMsgListMenuIdxes;
	this.ClearSearchData = DigDistMsgReader_ClearSearchData;
	this.ReadOrListSubBoard = DigDistMsgReader_ReadOrListSubBoard;
	this.PopulateHdrsIfSearch_DispErrorIfNoMsgs = DigDistMsgReader_PopulateHdrsIfSearch_DispErrorIfNoMsgs;
	this.SearchTypePopulatesSearchResults = DigDistMsgReader_SearchTypePopulatesSearchResults;
	this.SearchTypeRequiresSearchText = DigDistMsgReader_SearchTypeRequiresSearchText;
	this.MessageAreaScan = DigDistMsgReader_MessageAreaScan;
	this.PromptContinueOrReadMsg = DigDistMsgReader_PromptContinueOrReadMsg;
	this.WriteMsgListScreenTopHeader = DigDistMsgReader_WriteMsgListScreenTopHeader;
	this.ReadMessageEnhanced = DigDistMsgReader_ReadMessageEnhanced;
	this.ReadMessageEnhanced_Scrollable = DigDistMsgReader_ReadMessageEnhanced_Scrollable;
	this.ScrollableReaderNextReadableMessage = DigDistMsgReader_ScrollableReaderNextReadableMessage;
	this.ScrollReaderDetermineClickCoordAction = DigDistMsgReader_ScrollReaderDetermineClickCoordAction;
	this.ReadMessageEnhanced_Traditional = DigDistMsgReader_ReadMessageEnhanced_Traditional;
	this.EnhReaderPrepLast2LinesForPrompt = DigDistMsgReader_EnhReaderPrepLast2LinesForPrompt;
	this.LookForNextOrPriorNonDeletedMsg = DigDistMsgReader_LookForNextOrPriorNonDeletedMsg;
	this.PrintMessageInfo = DigDistMsgReader_PrintMessageInfo;
	this.ListScreenfulOfMessages = DigDistMsgReader_ListScreenfulOfMessages;
	this.DisplayMsgListHelp = DigDistMsgReader_DisplayMsgListHelp;
	this.DisplayTraditionalMsgListHelp = DigDistMsgReader_DisplayTraditionalMsgListHelp;
	this.DisplayLightbarMsgListHelp = DigDistMsgReader_DisplayLightbarMsgListHelp;
	this.DisplayMessageListNotesHelp = DigDistMsgReader_DisplayMessageListNotesHelp;
	this.SetMsgListPauseTextAndLightbarHelpLine = DigDistMsgReader_SetMsgListPauseTextAndLightbarHelpLine;
	this.SetEnhancedReaderHelpLine = DigDistMsgReader_SetEnhancedReaderHelpLine;
	this.EditExistingMsg = DigDistMsgReader_EditExistingMsg;
	this.EditExistingMessageOldWay = DigDistMsgReader_EditExistingMessageOldWay;
	this.CanDelete = DigDistMsgReader_CanDelete;
	this.CanDeleteLastMsg = DigDistMsgReader_CanDeleteLastMsg;
	this.CanEdit = DigDistMsgReader_CanEdit;
	this.CanQuote = DigDistMsgReader_CanQuote;
	this.ReadConfigFile = DigDistMsgReader_ReadConfigFile;
	this.ReadUserSettingsFile = DigDistMsgReader_ReadUserSettingsFile;
	this.WriteUserSettingsFile = DigDistMsgReader_WriteUserSettingsFile;
	// TODO: Is this.DisplaySyncMsgHeader even needed anymore?  Looks like it's not being called.
	this.DisplaySyncMsgHeader = DigDistMsgReader_DisplaySyncMsgHeader;
	this.GetMsgHdrFilenameFull = DigDistMsgReader_GetMsgHdrFilenameFull;
	this.GetMsgHdrByIdx = DigDistMsgReader_GetMsgHdrByIdx;
	this.GetMsgHdrByMsgNum = DigDistMsgReader_GetMsgHdrByMsgNum;
	this.GetMsgHdrByAbsoluteNum = DigDistMsgReader_GetMsgHdrByAbsoluteNum;
	this.AbsMsgNumToIdx = DigDistMsgReader_AbsMsgNumToIdx;
	this.IdxToAbsMsgNum = DigDistMsgReader_IdxToAbsMsgNum;
	this.NonDeletedMessagesExist = DigDistMsgReader_NonDeletedMessagesExist;
	this.HighestMessageNum = DigDistMsgReader_HighestMessageNum;
	this.IsValidMessageNum = DigDistMsgReader_IsValidMessageNum;
	this.PromptForMsgNum = DigDistMsgReader_PromptForMsgNum;
	this.ParseMsgAtCodes = DigDistMsgReader_ParseMsgAtCodes;
	this.ReplaceMsgAtCodeFormatStr = DigDistMsgReader_ReplaceMsgAtCodeFormatStr;
	this.FindNextNonDeletedMsgIdx = DigDistMsgReader_FindNextNonDeletedMsgIdx;
	this.ChangeSubBoard = DigDistMsgReader_ChangeSubBoard;
	this.EnhancedReaderChangeSubBoard = DigDistMsgReader_EnhancedReaderChangeSubBoard;
	this.ReplyToMsg = DigDistMsgReader_ReplyToMsg;
	this.DoPrivateReply = DigDistMsgReader_DoPrivateReply;
	this.DisplayEnhancedReaderHelp = DigDistMsgReader_DisplayEnhancedReaderHelp;
	this.DisplayEnhancedMsgHdr = DigDistMsgReader_DisplayEnhancedMsgHdr;
	this.DisplayAreaChgHdr = DigDistMsgReader_DisplayAreaChgHdr;
	this.DisplayEnhancedReaderWholeScrollbar = DigDistMsgReader_DisplayEnhancedReaderWholeScrollbar;
	this.UpdateEnhancedReaderScollbar = DigDistMsgReader_UpdateEnhancedReaderScollbar;
	this.MessageIsDeleted = DigDistMsgReader_MessageIsDeleted;
	this.MessageIsLastFromUser = DigDistMsgReader_MessageIsLastFromUser;
	this.DisplayEnhReaderError = DigDistMsgReader_DisplayEnhReaderError;
	this.EnhReaderPromptYesNo = DigDistMsgReader_EnhReaderPromptYesNo;
	this.PromptAndDeleteOrUndeleteMessage = DigDistMsgReader_PromptAndDeleteOrUndeleteMessage;
	this.PromptAndDeleteOrUndeleteSelectedMessages = DigDistMsgReader_PromptAndDeleteOrUndeleteSelectedMessages;
	this.GetExtdMsgHdrInfo = DigDistMsgReader_GetExtdMsgHdrInfo;
	this.GetMsgInfoForEnhancedReader = DigDistMsgReader_GetMsgInfoForEnhancedReader;
	this.GetLastReadMsgIdxAndNum = DigDistMsgReader_GetLastReadMsgIdxAndNum;
	this.GetScanPtrMsgIdx = DigDistMsgReader_GetScanPtrMsgIdx;
	this.RemoveFromSearchResults = DigDistMsgReader_RemoveFromSearchResults;
	this.FindThreadNextOffset = DigDistMsgReader_FindThreadNextOffset;
	this.FindThreadPrevOffset = DigDistMsgReader_FindThreadPrevOffset;
	this.SaveMsgToFile = DigDistMsgReader_SaveMsgToFile;
	this.ToggleSelectedMessage = DigDistMsgReader_ToggleSelectedMessage;
	this.MessageIsSelected = DigDistMsgReader_MessageIsSelected;
	this.AllSelectedMessagesCanBeDeleted = DigDistMsgReader_AllSelectedMessagesCanBeDeleted;
	this.DeleteOrUndeleteSelectedMessages = DigDistMsgReader_DeleteOrUndeleteSelectedMessages;
	this.NumSelectedMessages = DigDistMsgReader_NumSelectedMessages;
	this.ForwardMessage = DigDistMsgReader_ForwardMessage;
	this.VoteOnMessage = DigDistMsgReader_VoteOnMessage;
	this.HasUserVotedOnMsg = DigDistMsgReader_HasUserVotedOnMsg;
	this.GetUpvoteAndDownvoteInfo = DigDistMsgReader_GetUpvoteAndDownvoteInfo;
	this.GetMsgBody = DigDistMsgReader_GetMsgBody;
	this.RefreshMsgHdrInArrays = DigDistMsgReader_RefreshMsgHdrInArrays;
	this.WriteLightbarKeyHelpErrorMsg = DigDistMsgReader_WriteLightbarKeyHelpErrorMsg;

	// startMode specifies the mode for the reader to start in - List mode
	// or reader mode, etc.  This is a setting that is read from the configuration
	// file.  The configuration file can be either READER_MODE_READ or
	// READER_MODE_LIST, but the optional "mode" parameter in the command-line
	// arguments can specify another mode.
	this.startMode = READER_MODE_LIST;

	// hdrsForCurrentSubBoard is an array that will be populated with the
	// message headers for the current sub-board.
	this.hdrsForCurrentSubBoard = [];
	// hdrsForCurrentSubBoardByMsgNum is an object that maps absolute message numbers
	// to their index to hdrsForCurrentSubBoard
	this.hdrsForCurrentSubBoardByMsgNum = {};

	// msgSearchHdrs is an object containing message headers found via searching.
	// It is indexed by internal message area code.  Each internal code index
	// will specify an object containing the following properties:
	//  indexed: A standard 0-based array containing message headers
	this.msgSearchHdrs = {};
	this.searchString = ""; // To be used for message searching
	// this.searchType will specify the type of search:
	//  SEARCH_NONE (-1): No search
	//  SEARCH_KEYWORD: Keyword search in message subject & body
	//  SEARCH_FROM_NAME: Search by 'from' name
	//  SEARCH_TO_NAME_CUR_MSG_AREA: Search by 'to' name
	//  SEARCH_TO_USER_CUR_MSG_AREA: Search by 'to' name, to the current user
	//  SEARCH_MSG_NEWSCAN: New (unread) message scan (prompt the user for sub, group, or all)
	//  SEARCH_MSG_NEWSCAN_CUR_SUB: New (unread) message scan (current sub-board)
	//  SEARCH_MSG_NEWSCAN_CUR_GRP: New (unread) message scan (current message group)
	//  SEARCH_MSG_NEWSCAN_ALL: New (unread) message scan (all message sub-boards)
	//  SEARCH_TO_USER_NEW_SCAN: New (unread) messages to the current user (prompt the user for sub, group, or all)
	//  SEARCH_TO_USER_NEW_SCAN_CUR_SUB: New (unread) messages to the current user (current sub-board)
	//  SEARCH_TO_USER_NEW_SCAN_CUR_GRP: New (unread) messages to the current user (current group)
	//  SEARCH_TO_USER_NEW_SCAN_ALL: New (unread) messages to the current user (all sub-board)
	//  SEARCH_ALL_TO_USER_SCAN: All messages to the current user
	this.searchType = SEARCH_NONE;
	this.doingMsgScan = false; // Set to true in MessageAreaScan()

	this.subBoardCode = bbs.cursub_code; // The message sub-board code
	this.readingPersonalEmail = false;

	// this.colors will be an array of colors to use in the message list
	this.colors = getDefaultColors();
	this.readingPersonalEmailFromUser = false;
	if (typeof(pSubBoardCode) == "string")
	{
		var subCodeLowerCase = pSubBoardCode.toLowerCase();
		if (subBoardCodeIsValid(subCodeLowerCase))
		{
			this.setSubBoardCode(subCodeLowerCase);
			if (gCmdLineArgVals.hasOwnProperty("personalemailsent") && gCmdLineArgVals.personalemailsent)
				this.readingPersonalEmailFromUser = true;
		}
	}

	// This property controls whether or not the user will be prompted to
	// continue listing messages after selecting a message to read.  Only for
	// regular reading, not for newscans etc.
	this.promptToContinueListingMessages = false;
	// Whether or not to prompt the user to confirm to read a message
	this.promptToReadMessage = false;
	// For enhanced reader mode (reading only, not for newscan, etc.): Whether or
	// not to ask the user whether to post on the sub-board in reader mode after
	// reading the last message instead of prompting to go to the next sub-board.
	// This is like the stock Synchronet behavior.
	this.readingPostOnSubBoardInsteadOfGoToNext = false;

	// String lengths for the columns to write
	// Fixed field widths: Message number, date, and time
	// TODO: It might be good to figure out the longest message number for a
	// sub-board and set the message number length dynamically.  It would have
	// to change whenever the user changes to a different sub-board, and the
	// message list format string would have to change too.
	//this.MSGNUM_LEN = 4;
	this.MSGNUM_LEN = 5;
	this.DATE_LEN = 10; // i.e., YYYY-MM-DD
	this.TIME_LEN = 8;  // i.e., HH:MM:SS
	// Variable field widths: From, to, and subject
	this.FROM_LEN = (console.screen_columns * (15/console.screen_columns)).toFixed(0);
	this.TO_LEN = (console.screen_columns * (15/console.screen_columns)).toFixed(0);
	//var colsLeftForSubject = console.screen_columns-this.MSGNUM_LEN-this.DATE_LEN-this.TIME_LEN-this.FROM_LEN-this.TO_LEN-6; // 6 to account for the spaces
	//this.SUBJ_LEN = (console.screen_columns * (colsLeftForSubject/console.screen_columns)).toFixed(0);
	this.SUBJ_LEN = console.screen_columns-this.MSGNUM_LEN-this.DATE_LEN-this.TIME_LEN-this.FROM_LEN-this.TO_LEN-6; // 6 to account for the spaces
	// For showing message scores in the message list
	this.SCORE_LEN = 4;
	// Whether or not to show message scores in the message list: Only if the terminal
	// is at least 86 characters wide and if vote functions exist in the running build
	// of Synchronet
	this.showScoresInMsgList = ((console.screen_columns >= 86) && (typeof((new MsgBase("mail")).vote_msg) === "function"));
	if (this.showScoresInMsgList)
		this.SUBJ_LEN -= (this.SCORE_LEN + 1); // + 1 to account for a space

	// Whether or not the user chose to read a message
	this.readAMessage = false;
	// Whether or not the user denied confirmation to read a message
	this.deniedReadingMessage = false;

	// msgListUseLightbarListInterface specifies whether or not to use the lightbar
	// interface for the message list.  The lightbar interface will only be used if
	// the user's terminal supports ANSI.
	this.msgListUseLightbarListInterface = true;

	// Whether or not to use the scrolling interface when reading a message
	// (will only be used for ANSI terminals).
	this.scrollingReaderInterface = true;

	// reverseListOrder stores whether or not to arrange the message list descending
	// by date.
	this.reverseListOrder = false;

	// displayBoardInfoInHeader specifies whether or not to display
	// the message group and sub-board lines in the header at the
	// top of the screen (an additional 2 lines).
	this.displayBoardInfoInHeader = false;

	// msgList_displayMessageDateImported specifies whether or not to use the
	// message import date as the date displayed in the message list.  If false,
	// the message written date will be displayed.
	this.msgList_displayMessageDateImported = true;

	// The number of spaces to use for tab characters - Used in the
	// extended read mode
	this.numTabSpaces = 3;

	// this.text is an object containing text used for various prompts & functions.
	this.text = {
		scrollbarBGChar: BLOCK1,
		scrollbarScrollBlockChar: BLOCK2,
		goToPrevMsgAreaPromptText: "\x01n\x01c\x01hGo to the previous message area",
		goToNextMsgAreaPromptText: "\x01n\x01c\x01hGo to the next message area",
		newMsgScanText: "\x01c\x01hN\x01n\x01cew \x01hM\x01n\x01cessage \x01hS\x01n\x01ccan",
		newToYouMsgScanText: "\x01c\x01hN\x01n\x01cew \x01hT\x01n\x01co \x01hY\x01n\x01cou \x01hM\x01n\x01cessage \x01hS\x01n\x01ccan",
		allToYouMsgScanText: "\x01c\x01hA\x01n\x01cll \x01hM\x01n\x01cessages \x01hT\x01n\x01co \x01hY\x01n\x01cou \x01hS\x01n\x01ccan",
		goToMsgNumPromptText: "\x01n\x01cGo to message # (or \x01hENTER\x01n\x01c to cancel)\x01g\x01h: \x01c",
		msgScanAbortedText: "\x01n\x01h\x01cM\x01n\x01cessage scan \x01h\x01y\x01iaborted\x01n",
		deleteMsgNumPromptText: "\x01n\x01cNumber of the message to be deleted (or \x01hENTER\x01n\x01c to cancel)\x01g\x01h: \x01c",
		editMsgNumPromptText: "\x01n\x01cNumber of the message to be edited (or \x01hENTER\x01n\x01c to cancel)\x01g\x01h: \x01c",
		searchingSubBoardAbovePromptText: "\x01n\x01cSearching (current sub-board: \x01b\x01h%s\x01n\x01c)",
		searchingSubBoardText: "\x01n\x01cSearching \x01h%s\x01n\x01c...",
		noMessagesInSubBoardText: "\x01n\x01h\x01bThere are no messages in the area \x01w%s\x01b.",
		noSearchResultsInSubBoardText: "\x01n\x01h\x01bNo messages were found in the area \x01w%s\x01b with the given search criteria.",
		msgScanCompleteText: "\x01n\x01h\x01cM\x01n\x01cessage scan complete\x01h\x01g.\x01n",
		invalidMsgNumText: "\x01n\x01y\x01hInvalid message number: %d",
		readMsgNumPromptText: "\x01n\x01g\x01h\x01i* \x01n\x01cRead message #: \x01h",
		msgHasBeenDeletedText: "\x01n\x01h\x01g* \x01yMessage #\x01w%d \x01yhas been deleted.",
		noKludgeLinesForThisMsgText: "\x01n\x01h\x01yThere are no kludge lines for this message.",
		searchingPersonalMailText: "\x01w\x01hSearching personal mail\x01n",
		searchTextPromptText: "\x01cEnter the search text\x01g\x01h:\x01n\x01c ",
		fromNamePromptText: "\x01cEnter the 'from' name to search for\x01g\x01h:\x01n\x01c ",
		toNamePromptText: "\x01cEnter the 'to' name to search for\x01g\x01h:\x01n\x01c ",
		abortedText: "\x01n\x01y\x01h\x01iAborted\x01n",
		loadingPersonalMailText: "\x01n\x01cLoading %s...",
		msgDelConfirmText: "\x01n\x01h\x01yDelete\x01n\x01c message #\x01h%d\x01n\x01c: Are you sure",
		msgUndelConfirmText: "\x01n\x01h\x01yUndelete\x01n\x01c message #\x01h%d\x01n\x01c: Are you sure",
		delSelectedMsgsConfirmText: "\x01n\x01h\x01yDelete selected messages: Are you sure",
		undelSelectedMsgsConfirmText: "\x01n\x01h\x01yUndelete selected messages: Are you sure",
		msgDeletedText: "\x01n\x01cMessage #\x01h%d\x01n\x01c has been marked for deletion.",
		msgUndeletedText: "\x01n\x01cMessage #\x01h%d\x01n\x01c has been unmarked for deletion.",
		selectedMsgsDeletedText: "\x01n\x01cSelected messages have been marked for deletion.",
		selectedMsgsUndeletedText: "\x01n\x01cSelected messages have been unmarked for deletion.",
		cannotDeleteMsgText_notYoursNotASysop: "\x01n\x01h\x01wCannot delete message #\x01y%d \x01wbecause it's not yours or you're not a sysop.",
		cannotDeleteMsgText_notLastPostedMsg: "\x01n\x01h\x01g* \x01yCannot delete message #%d. You can only delete your last message in this area.\x01n",
		cannotDeleteAllSelectedMsgsText: "\x01n\x01y\x01h* Cannot delete all selected messages",
		msgEditConfirmText: "\x01n\x01cEdit message #\x01h%d\x01n\x01c: Are you sure",
		noPersonalEmailText: "\x01n\x01cYou have no messages.",
		postOnSubBoard: "\x01n\x01gPost on %s %s"
	};


	// These two variables keep track of whether we're doing a message scan that spans
	// multiple sub-boards so that the enhanced reader function can enable use of
	// the > key to go to the next sub-board.
	this.doingMultiSubBoardScan = false;

	// An option for using the scrollable interface for messages with ANSI
	// content - The sysop can set this to false if the sysop thinks the
	// scrolling ANSI interface (using frame.js and scrollbar.js) doesn't
	// look good enough
	this.useScrollingInterfaceForANSIMessages = true;

	// Whether or not to pause (with a message) after doing a new message scan
	this.pauseAfterNewMsgScan = true;

	// For the message area chooser header filename & maximum number of
	// area chooser header lines to display
	this.areaChooserHdrFilenameBase = "areaChgHeader";
	this.areaChooserHdrMaxLines = 5;
	
	// Some key bindings for enhanced reader mode
	this.enhReaderKeys = {
		reply: "R",
		privateReply: "I",
		editMsg: "E",
		showHelp: "?",
		postMsg: "P",
		nextMsg: KEY_RIGHT,
		previousMsg: KEY_LEFT,
		firstMsg: "F",
		lastMsg: "L",
		showKludgeLines: "K",
		showHdrInfo: "H",
		showMsgList: "M",
		chgMsgArea: "C",
		userEdit: "U",
		quit: "Q",
		prevMsgByTitle: "<",
		nextMsgByTitle: ">",
		prevMsgByAuthor: "{",
		nextMsgByAuthor: "}",
		prevMsgByToUser: "[",
		nextMsgByToUser: "]",
		prevMsgByThreadID: "(",
		nextMsgByThreadID: ")",
		prevSubBoard: "-",
		nextSubBoard: "+",
		downloadAttachments: CTRL_A,
		saveToBBSMachine: CTRL_S,
		deleteMessage: KEY_DEL,
		selectMessage: " ",
		batchDelete: CTRL_D,
		forwardMsg: "O",
		vote: "V",
		showVotes: "T",
		closePoll: "!",
		bypassSubBoardInNewScan: "B",
		userSettings: CTRL_U,
		threadView: "*" // TODO: Implement this
	};
	if (user.is_sysop)
		this.enhReaderKeys.validateMsg = "A";
	// Some key bindings for the message list (not necessarily all of them)
	this.msgListKeys = {
		deleteMessage: KEY_DEL,
		undeleteMessage: "U",
		batchDelete: CTRL_D,
		editMsg: "E",
		goToMsg: "G",
		chgMsgArea: "C",
		userSettings: CTRL_U,
		quit: "Q",
		showHelp: "?"
	};

	// Whether or not to display avatars
	this.displayAvatars = true;
	this.rightJustifyAvatar = true;

	// Message list sort option
	this.msgListSort = MSG_LIST_SORT_DATETIME_RECEIVED;

	// Whether or not to convert Y-style MCI attribute codes to Synchronet attribute codes
	this.convertYStyleMCIAttrsToSync = false;

	// Whether or not to use the scrollbar in the enhanced message reader
	this.useEnhReaderScrollbar = true;

	this.cfgFilename = "DDMsgReader.cfg";
	// Check the command-line arguments for a custom configuration file name
	// before reading the configuration file.
	var scriptArgsIsValid = (typeof(pScriptArgs) == "object");
	if (scriptArgsIsValid && pScriptArgs.hasOwnProperty("configfilename"))
		this.cfgFilename = pScriptArgs["configfilename"];
	// Read the settings from the config file
	this.cfgFileSuccessfullyRead = false;
	this.ReadConfigFile();
	this.userSettings = {
		twitList: []
	};
	this.ReadUserSettingsFile(false);
	// Set any other values specified by the command-line parameters
	// Reader start mode - Read or list mode
	if (scriptArgsIsValid)
	{
		if (pScriptArgs.hasOwnProperty("startmode"))
		{
			var readerStartMode = readerModeStrToVal(pScriptArgs["startmode"]);
			if (readerStartMode != -1)
				this.startMode = readerStartMode;
		}
		// Search mode
		if (pScriptArgs.hasOwnProperty("search"))
		{
			var searchType = searchTypeStrToVal(pScriptArgs["search"]);
			if (searchType != SEARCH_NONE)
				this.searchType = searchType;
		}
	}
	// Color value adjusting (must be done after reading the config file in case
	// the color settings were changed from defaults)
	// Message list highlight colors: For each (except for the background),
	// prepend the normal attribute and append the background attribute to the end.
	// This is to ensure that high attributes don't affect the rest of the line and
	// the background attribute stays for the rest of the line.
	this.colors.msgListMsgNumHighlightColor = "\x01n" + this.colors.msgListMsgNumHighlightColor + this.colors.msgListHighlightBkgColor;
	this.colors.msgListFromHighlightColor = "\x01n" + this.colors.msgListFromHighlightColor + this.colors.msgListHighlightBkgColor;
	this.colors.msgListToHighlightColor = "\x01n" + this.colors.msgListToHighlightColor + this.colors.msgListHighlightBkgColor;
	this.colors.msgListSubjHighlightColor = "\x01n" + this.colors.msgListSubjHighlightColor + this.colors.msgListHighlightBkgColor;
	this.colors.msgListDateHighlightColor = "\x01n" + this.colors.msgListDateHighlightColor + this.colors.msgListHighlightBkgColor;
	this.colors.msgListTimeHighlightColor = "\x01n" + this.colors.msgListTimeHighlightColor + this.colors.msgListHighlightBkgColor;
	// Similar for the area chooser lightbar highlight colors
	this.colors.areaChooserMsgAreaNumHighlightColor = "\x01n" + this.colors.areaChooserMsgAreaNumHighlightColor + this.colors.areaChooserMsgAreaBkgHighlightColor;
	this.colors.areaChooserMsgAreaDescHighlightColor = "\x01n" + this.colors.areaChooserMsgAreaDescHighlightColor + this.colors.areaChooserMsgAreaBkgHighlightColor;
	this.colors.areaChooserMsgAreaDateHighlightColor = "\x01n" + this.colors.areaChooserMsgAreaDateHighlightColor + this.colors.areaChooserMsgAreaBkgHighlightColor;
	this.colors.areaChooserMsgAreaTimeHighlightColor = "\x01n" + this.colors.areaChooserMsgAreaTimeHighlightColor + this.colors.areaChooserMsgAreaBkgHighlightColor;
	this.colors.areaChooserMsgAreaNumItemsHighlightColor = "\x01n" + this.colors.areaChooserMsgAreaNumItemsHighlightColor + this.colors.areaChooserMsgAreaBkgHighlightColor;
	// Similar for the enhanced reader help line colors
	this.colors.enhReaderHelpLineGeneralColor = "\x01n" + this.colors.enhReaderHelpLineGeneralColor + this.colors.enhReaderHelpLineBkgColor;
	this.colors.enhReaderHelpLineHotkeyColor = "\x01n" + this.colors.enhReaderHelpLineHotkeyColor + this.colors.enhReaderHelpLineBkgColor;
	this.colors.enhReaderHelpLineParenColor = "\x01n" + this.colors.enhReaderHelpLineParenColor + this.colors.enhReaderHelpLineBkgColor;
	// Similar for the lightbar message list help line colors
	this.colors.lightbarMsgListHelpLineGeneralColor = "\x01n" + this.colors.lightbarMsgListHelpLineGeneralColor + this.colors.lightbarMsgListHelpLineBkgColor;
	this.colors.lightbarMsgListHelpLineHotkeyColor = "\x01n" + this.colors.lightbarMsgListHelpLineHotkeyColor + this.colors.lightbarMsgListHelpLineBkgColor;
	this.colors.lightbarMsgListHelpLineParenColor = "\x01n" + this.colors.lightbarMsgListHelpLineParenColor + this.colors.lightbarMsgListHelpLineBkgColor;
	// Similar for the lightbar area chooser help line colors
	this.colors.lightbarAreaChooserHelpLineGeneralColor = "\x01n" + this.colors.lightbarAreaChooserHelpLineGeneralColor + this.colors.lightbarAreaChooserHelpLineBkgColor;
	this.colors.lightbarAreaChooserHelpLineHotkeyColor = "\x01n" + this.colors.lightbarAreaChooserHelpLineHotkeyColor + this.colors.lightbarAreaChooserHelpLineBkgColor;
	this.colors.lightbarAreaChooserHelpLineParenColor = "\x01n" + this.colors.lightbarAreaChooserHelpLineParenColor + this.colors.lightbarAreaChooserHelpLineBkgColor;
	// Prepend most of the text strings with the normal attribute (if they don't
	// have it already) to make sure the correct colors are used.
	for (var prop in this.text)
	{
		if ((prop != "scrollbarBGChar") && (prop != "scrollbarScrollBlockChar"))
		{
			if ((this.text[prop].length > 0) && (this.text[prop].charAt(0) != "\x01n"))
				this.text[prop] = "\x01n" + this.text[prop];
		}
	}

	// this.tabReplacementText will be the text that tabs will be replaced
	// with in enhanced reader mode
	this.tabReplacementText = format("%" + this.numTabSpaces + "s", "");

	// Calculate the message list widths and format strings based on the current
	// sub-board code and color settings.  Start with a message # field length
	// of 4 characters.  This will be re-calculated later after message headers
	// are loaded.
	this.RecalcMsgListWidthsAndFormatStrs(4);

	// If the user's terminal doesn't support ANSI, then append a newline to
	// the end of the format string (we won't be able to move the cursor).
	if (!canDoHighASCIIAndANSI())
	{
		this.sMsgInfoFormatStr += "\r\n";
		this.sMsgInfoToUserFormatStr += "\r\n";
		this.sMsgInfoFromUserFormatStr += "\r\n";
		this.sMsgInfoFormatHighlightStr += "\r\n";
	}
	// Enhanced reader help line (will be set up in
	// DigDistMsgReader_SetEnhancedReaderHelpLine())
	this.enhReadHelpLine = "";

	// Read the enhanced message header file and populate this.enhMsgHeaderLines,
	// the header text for enhanced reader mode.  The enhanced reader header file
	// name will start with 'enhMsgHeader', and there can be multiple versions for
	// different terminal widths (i.e., msgHeader_80.ans for an 80-column console
	// and msgHeader_132 for a 132-column console).
	this.enhMsgHeaderLines = loadTextFileIntoArray("enhMsgHeader", 10);
	// this.enhMsgHeaderLinesToReadingUser will be a copy of this.endMsgReaderLines
	// but with the 'To' user line changed to highlight the name for messages to
	// the logged-on reading user
	this.enhMsgHeaderLinesToReadingUser = [];
	// If the header file didn't exist, then populate the enhanced reader header
	// array with default lines.
	this.usingInternalEnhMsgHdr = (this.enhMsgHeaderLines.length == 0);
	if (this.usingInternalEnhMsgHdr)
	{
		// Group name: 20% of console width
		// Sub-board name: 34% of console width
		var msgGrpNameLen = Math.floor(console.screen_columns * 0.2);
		var subBoardNameLen = Math.floor(console.screen_columns * 0.34);
		var hdrLine1 = "\x01n\x01h\x01c" + UPPER_LEFT_SINGLE + HORIZONTAL_SINGLE + "\x01n\x01c"
		             + HORIZONTAL_SINGLE + " \x01h@GRP-L";
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
			hdrLine1 += HORIZONTAL_SINGLE;
		hdrLine1 += "\x01n\x01c" + HORIZONTAL_SINGLE + HORIZONTAL_SINGLE + "\x01h"
		         + HORIZONTAL_SINGLE + UPPER_RIGHT_SINGLE;
		this.enhMsgHeaderLines.push(hdrLine1);
		this.enhMsgHeaderLinesToReadingUser.push(hdrLine1);
		var hdrLine2 = "\x01n\x01c" + VERTICAL_SINGLE + "\x01h\x01k" + BLOCK1 + BLOCK2
		             + BLOCK3 + "\x01gM\x01n\x01gsg#\x01h\x01c: " + this.colors.msgHdrMsgNumColor + "@MSG_NUM_AND_TOTAL-L";
		numChars = console.screen_columns - 32;
		for (var i = 0; i < numChars; ++i)
			hdrLine2 += "#";
		hdrLine2 += "@\x01n\x01c" + VERTICAL_SINGLE;
		this.enhMsgHeaderLines.push(hdrLine2);
		this.enhMsgHeaderLinesToReadingUser.push(hdrLine2);
		var hdrLine3 = "\x01n\x01h\x01k" + VERTICAL_SINGLE + BLOCK1 + BLOCK2 + BLOCK3
					 + "\x01gF\x01n\x01grom\x01h\x01c: " + this.colors.msgHdrFromColor + "@MSG_FROM_AND_FROM_NET-L";
		numChars = console.screen_columns - 36;
		for (var i = 0; i < numChars; ++i)
			hdrLine3 += "#";
		hdrLine3 += "@\x01k" + VERTICAL_SINGLE;
		this.enhMsgHeaderLines.push(hdrLine3);
		this.enhMsgHeaderLinesToReadingUser.push(hdrLine3);
		this.enhMsgHeaderLines.push(genEnhHdrToUserLine(this.colors, false));
		this.enhMsgHeaderLinesToReadingUser.push(genEnhHdrToUserLine(this.colors, true));
		var hdrLine5 = "\x01n\x01h\x01k" + VERTICAL_SINGLE + BLOCK1 + BLOCK2 + BLOCK3
		             + "\x01gS\x01n\x01gubj\x01h\x01c: " + this.colors.msgHdrSubjColor + "@MSG_SUBJECT-L";
		numChars = console.screen_columns - 26;
		for (var i = 0; i < numChars; ++i)
			hdrLine5 += "#";
		hdrLine5 += "@\x01k" + VERTICAL_SINGLE;
		this.enhMsgHeaderLines.push(hdrLine5);
		this.enhMsgHeaderLinesToReadingUser.push(hdrLine5);
		var hdrLine6 = "\x01n\x01c" + VERTICAL_SINGLE + "\x01h\x01k" + BLOCK1 + BLOCK2 + BLOCK3
		             + "\x01gD\x01n\x01gate\x01h\x01c: " + this.colors.msgHdrDateColor + "@MSG_DATE-L";
		//numChars = console.screen_columns - 23;
		numChars = console.screen_columns - 67;
		for (var i = 0; i < numChars; ++i)
			hdrLine6 += "#";
		//hdrLine6 += "@\x01n\x01c" + VERTICAL_SINGLE;
		hdrLine6 += "@ @MSG_TIMEZONE@\x01n";
		for (var i = 0; i < 40; ++i)
			hdrLine6 += " ";
		hdrLine6 += "\x01n\x01c" + VERTICAL_SINGLE;
		this.enhMsgHeaderLines.push(hdrLine6);
		this.enhMsgHeaderLinesToReadingUser.push(hdrLine6);
		var hdrLine7 = "\x01n\x01h\x01c" + BOTTOM_T_SINGLE + HORIZONTAL_SINGLE + "\x01n\x01c"
		             + HORIZONTAL_SINGLE + HORIZONTAL_SINGLE + "\x01h\x01k";
		numChars = console.screen_columns - 8;
		for (var i = 0; i < numChars; ++i)
			hdrLine7 += HORIZONTAL_SINGLE;
		hdrLine7 += "\x01n\x01c" + HORIZONTAL_SINGLE + HORIZONTAL_SINGLE + "\x01h"
		         + HORIZONTAL_SINGLE + BOTTOM_T_SINGLE;
		this.enhMsgHeaderLines.push(hdrLine7);
		this.enhMsgHeaderLinesToReadingUser.push(hdrLine7);
	}
	else
	{
		// We loaded the enhanced message header lines from a custom file.
		// Copy from this.enhMsgHeaderLines to this.enhMsgHeaderLinesToReadingUser
		// but change any 'To:' line to highlight the 'to' username.
		this.enhMsgHeaderLinesToReadingUser = this.enhMsgHeaderLines.slice();
		// Go through the header lines and ensure the 'To' line has a different
		// color
		for (var lineIdx = 0; lineIdx < this.enhMsgHeaderLinesToReadingUser.length; ++lineIdx)
			this.enhMsgHeaderLinesToReadingUser[lineIdx] = syncAttrCodesToANSI(strWithToUserColor(this.enhMsgHeaderLinesToReadingUser[lineIdx], this.colors.msgHdrToUserColor));
	}
	// Save the enhanced reader header width.  This will be the length of the longest
	// line in the header.
	this.enhMsgHeaderWidth = 0;
	if (this.enhMsgHeaderLines.length > 0)
	{
		var lineLen = 0;
		for (var i = 0; i < this.enhMsgHeaderLines.length; ++i)
		{
			lineLen = console.strlen(this.enhMsgHeaderLines[i]);
			if (lineLen > this.enhMsgHeaderWidth)
				this.enhMsgHeaderWidth = lineLen;
		}
	}

	// Message display area information
	this.msgAreaTop = this.enhMsgHeaderLines.length + 1;
	this.msgAreaBottom = console.screen_rows-1;  // The last line of the message area
	// msgAreaLeft and msgAreaRight are the rightmost and leftmost columns of the
	// message area, respectively.  These are 1-based.  1 is subtracted from
	// msgAreaRight to leave room for the scrollbar in enhanced reader mode.
	this.msgAreaLeft = 1;
	this.msgAreaRight = console.screen_columns - 1;
	this.msgAreaWidth = this.msgAreaRight - this.msgAreaLeft + 1;
	this.msgAreaHeight = this.msgAreaBottom - this.msgAreaTop + 1;

	//////////////////////////////////////////////
	// Things related to changing to a different message group & sub-board

	// In the message area lists (for changing to another message area), the
	// date & time of the last-imported message will be shown.
	// msgAreaList_lastImportedMsg_showImportTime is a boolean to specify
	// whether or not to use the import time for the last-imported message.
	// If false, the message written time will be used.
	this.msgAreaList_lastImportedMsg_showImportTime = true;

	// These variables store the lengths of the various columns displayed in
	// the message group/sub-board lists.
	// Sub-board info field lengths
	this.areaNumLen = 4;
	this.numItemsLen = 4;
	this.dateLen = 10; // i.e., YYYY-MM-DD
	this.timeLen = 8;  // i.e., HH:MM:SS
	// Sub-board name length - This should be 47 for an 80-column display.
	this.subBoardNameLen = console.screen_columns - this.areaNumLen - this.numItemsLen - this.dateLen - this.timeLen - 7;
	// Message group description length (67 chars on an 80-column screen)
	this.msgGrpDescLen = console.screen_columns - this.areaNumLen - this.numItemsLen - 5;

	// Some methods for choosing the message area
	this.WriteChgMsgAreaKeysHelpLine = DigDistMsgReader_WriteLightbarChgMsgAreaKeysHelpLine;
	this.WriteGrpListHdrLine1 = DigDistMsgReader_WriteGrpListTopHdrLine1;
	this.WriteSubBrdListHdrLine = DigDistMsgReader_WriteSubBrdListHdrLine;
	this.SelectMsgArea = DigDistMsgReader_SelectMsgArea;
	this.SelectMsgArea_Lightbar = DigDistMsgReader_SelectMsgArea_Lightbar;
	this.SelectMsgArea_Traditional = DigDistMsgReader_SelectMsgArea_Traditional;
	this.ListMsgGrps = DigDistMsgReader_ListMsgGrps_Traditional;
	this.ListSubBoardsInMsgGroup = DigDistMsgReader_ListSubBoardsInMsgGroup_Traditional;
	// Lightbar-specific methods
	this.WriteMsgGroupLine = DigDistMsgReader_writeMsgGroupLine;
	this.UpdateMsgAreaPageNumInHeader = DigDistMsgReader_updateMsgAreaPageNumInHeader;
	this.GetMsgSubBoardLine = DigDistMsgReader_GetMsgSubBrdLine;
	// Choose Message Area help screen
	this.ShowChooseMsgAreaHelpScreen = DigDistMsgReader_showChooseMsgAreaHelpScreen;
	// Method to build the sub-board printf information for a message
	// group
	this.BuildSubBoardPrintfInfoForGrp = DigDistMsgReader_BuildSubBoardPrintfInfoForGrp;
	// Methods for calculating a page number for a message list item
	this.CalcTraditionalMsgListTopIdx = DigDistMsgReader_CalcTraditionalMsgListTopIdx;
	this.CalcLightbarMsgListTopIdx = DigDistMsgReader_CalcLightbarMsgListTopIdx;
	this.CalcMsgListScreenIdxVarsFromMsgNum = DigDistMsgReader_CalcMsgListScreenIdxVarsFromMsgNum;
	// A method for validating a user's choice of message area
	this.ValidateMsgAreaChoice = DigDistMsgReader_ValidateMsgAreaChoice;
	this.ValidateMsg = DigDistMsgReader_ValidateMsg;
	this.GetGroupNameAndDesc = DigDistMsgReader_GetGroupNameAndDesc;
	this.DoUserSettings_Scrollable = DigDistMsgReader_DoUserSettings_Scrollable;
	this.DoUserSettings_Traditional = DigDistMsgReader_DoUserSettings_Traditional;
	this.RefreshMsgAreaRectangle = DigDistMsgReader_RefreshMsgAreaRectangle;
	this.MsgHdrFromOrToInUserTwitlist = DigDistMsgReader_MsgHdrFromOrToInUserTwitlist;
	// For indexed mode
	this.DoIndexedMode = DigDistMsgReader_DoIndexedMode;
	this.DoIndexedModeLightbar = DigDistMsgReader_DoIndexedModeLightbar;
	this.DoIndexedModeTraditional = DigDistMsgReader_DoIndexedModeTraditional;
	this.MakeLightbarIndexedModeMenu = DigDistMsgReader_MakeLightbarIndexedModeMenu;

	// printf strings for message group/sub-board lists
	// Message group information (printf strings)
	this.msgGrpListPrintfStr = "\x01n " + this.colors.areaChooserMsgAreaNumColor + "%" + this.areaNumLen
	                         + "d " + this.colors.areaChooserMsgAreaDescColor + "%-"
	                         + this.msgGrpDescLen + "s " + this.colors.areaChooserMsgAreaNumItemsColor
	                         + "%" + this.numItemsLen + "d";
	this.msgGrpListHilightPrintfStr = "\x01n" + this.colors.areaChooserMsgAreaBkgHighlightColor + " "
	                                + "\x01n" + this.colors.areaChooserMsgAreaBkgHighlightColor
	                                + this.colors.areaChooserMsgAreaNumHighlightColor + "%" + this.areaNumLen
	                                + "d \x01n" + this.colors.areaChooserMsgAreaBkgHighlightColor
	                                + this.colors.areaChooserMsgAreaDescHighlightColor + "%-"
	                                + this.msgGrpDescLen + "s \x01n" + this.colors.areaChooserMsgAreaBkgHighlightColor
	                                + this.colors.areaChooserMsgAreaNumItemsHighlightColor + "%" + this.numItemsLen
	                                + "d";
	// Message group list header (printf string)
	this.msgGrpListHdrPrintfStr = this.colors.areaChooserMsgAreaHeaderColor + "%6s %-"
	                            + +(this.msgGrpDescLen-8) + "s %-12s";
	// Sub-board information header (printf string)
	this.subBoardListHdrPrintfStr = this.colors.areaChooserMsgAreaHeaderColor + " %5s %-"
	                              + +(this.subBoardNameLen-3) + "s %-7s %-19s";
	// Lightbar area chooser help line text
	// For PageUp, normally I'd think KEY_PAGEUP should work, but that triggers sending a telegram instead.  \x1b[V seems to work though.
	this.lightbarAreaChooserHelpLine = "\x01n"
	                          + this.colors.lightbarAreaChooserHelpLineHotkeyColor + "@CLEAR_HOT@@`" + UP_ARROW + "`" + KEY_UP + "@"
	                          + this.colors.lightbarAreaChooserHelpLineGeneralColor + ", "
	                          + this.colors.lightbarAreaChooserHelpLineHotkeyColor + "@`" + DOWN_ARROW + "`" + KEY_DOWN + "@"
	                          + this.colors.lightbarAreaChooserHelpLineGeneralColor + ", "
	                          + this.colors.lightbarAreaChooserHelpLineHotkeyColor + "@`HOME`" + KEY_HOME + "@"
	                          + this.colors.lightbarAreaChooserHelpLineGeneralColor + ", "
	                          + this.colors.lightbarAreaChooserHelpLineHotkeyColor + "@`END`" + KEY_END + "@"
	                          + this.colors.lightbarAreaChooserHelpLineGeneralColor + ", "
	                          + this.colors.lightbarAreaChooserHelpLineHotkeyColor + "#"
	                          + this.colors.lightbarAreaChooserHelpLineGeneralColor + ", "
	                          + this.colors.lightbarAreaChooserHelpLineHotkeyColor + "@`PgUp`" + "\x1b[V" + "@"
	                          + this.colors.lightbarAreaChooserHelpLineGeneralColor + "/"
	                          + this.colors.lightbarAreaChooserHelpLineHotkeyColor + "@`Dn`" + KEY_PAGEDN + "@"
	                          + this.colors.lightbarAreaChooserHelpLineGeneralColor + ", "
	                          + this.colors.lightbarAreaChooserHelpLineHotkeyColor + "@`F`F@"
	                          + this.colors.lightbarAreaChooserHelpLineParenColor + ")"
	                          + this.colors.lightbarAreaChooserHelpLineGeneralColor + "irst pg, "
	                          + this.colors.lightbarAreaChooserHelpLineHotkeyColor + "@`L`L@"
	                          + this.colors.lightbarAreaChooserHelpLineParenColor + ")"
	                          + this.colors.lightbarAreaChooserHelpLineGeneralColor + "ast pg, "
	                          + this.colors.lightbarAreaChooserHelpLineHotkeyColor + "@`CTRL-F`" + CTRL_F + "@"
	                          + this.colors.lightbarAreaChooserHelpLineGeneralColor + ", "
	                          + this.colors.lightbarAreaChooserHelpLineHotkeyColor + "@`/`/@"
	                          + this.colors.lightbarAreaChooserHelpLineGeneralColor + ", "
	                          + this.colors.lightbarAreaChooserHelpLineHotkeyColor + "@`N`N@"
	                          + this.colors.lightbarAreaChooserHelpLineGeneralColor + ", "
	                          + this.colors.lightbarAreaChooserHelpLineHotkeyColor + "@`Q`Q@"
	                          + this.colors.lightbarAreaChooserHelpLineParenColor + ")"
	                          + this.colors.lightbarAreaChooserHelpLineGeneralColor + "uit, "
	                          + this.colors.lightbarAreaChooserHelpLineHotkeyColor + "@`?`?@";
	var lbAreaChooserHelpLineLen = 72;
	// Pad the lightbar key help text on either side to center it on the screen
	// (but leave off the last character to avoid screen drawing issues)
	var padLen = console.screen_columns - lbAreaChooserHelpLineLen - 1;
	var leftPadLen = Math.floor(padLen/2);
	var rightPadLen = padLen - leftPadLen;
	this.lightbarAreaChooserHelpLine = this.colors.lightbarAreaChooserHelpLineGeneralColor
	                                 + format("%" + leftPadLen + "s", "")
	                                 + this.lightbarAreaChooserHelpLine
	                                 + this.colors.lightbarAreaChooserHelpLineGeneralColor
	                                 + format("%" + rightPadLen + "s", "") + "\x01n";

	// this.subBoardListPrintfInfo will be an array of printf strings
	// for the sub-boards in the message groups.  The index is the
	// message group index.  The sub-board printf information is created
	// on the fly the first time the user lists sub-boards for a message
	// group.
	this.subBoardListPrintfInfo = [];

	// Variables to save the top message index for the traditional & lightbar
	// message lists.  Initialize them to -1 to mean the message list hasn't been
	// displayed yet - In that case, the lister will use the user's last
	// read pointer.
	this.tradListTopMsgIdx = -1;
	this.tradMsgListNumLines = console.screen_rows-3;
	if (this.displayBoardInfoInHeader)
		this.tradMsgListNumLines -= 2;
	this.lightbarListTopMsgIdx = -1;
	this.lightbarMsgListNumLines = console.screen_rows-2;
	this.lightbarMsgListStartScreenRow = 2; // The first line number on the screen for the message list
	// If we will be displaying the message group and sub-board in the
	// header at the top of the screen (an additional 2 lines), then
	// update this.lightbarMsgListNumLines and this.lightbarMsgListStartScreenRow to
	// account for this.
	if (this.displayBoardInfoInHeader)
	{
		this.lightbarMsgListNumLines -= 2;
		this.lightbarMsgListStartScreenRow += 2;
	}
	// The selected message index for the lightbar message list (initially -1, will
	// be set in the lightbar list method)
	this.lightbarListSelectedMsgIdx = -1;
	// The selected message cursor position for the lightbar message list (initially
	// null, will be set in the lightbar list message)
	this.lightbarListCurPos = null;

	// selectedMessages will be an object (indexed by sub-board internal code)
	// containing objects that contain message indexes (as properties) for the
	// sub-boards.  Messages can be selected by the user for doing things such
	// as a batch delete, etc.
	this.selectedMessages = {};

	// areaChangeHdrLines is an array of text lines to use as a header to display
	// above the message area changer lists.
	this.areaChangeHdrLines = loadTextFileIntoArray(this.areaChooserHdrFilenameBase, this.areaChooserHdrMaxLines);

	// pausePromptText is the text that will be used for some of the pause
	// prompts.  It's loaded from text.dat, but in case that text contains
	// "@EXEC:" (to execute a script), this script will default to a "press
	// a key" message.
	this.pausePromptText = bbs.text(Pause);
	if (this.pausePromptText.toUpperCase().indexOf("@EXEC:") > -1)
		this.pausePromptText = "\x01n\x01c[ Press a key ] ";
}

// For the DigDistMsgReader class: Sets the subBoardCode property and also
// sets the readingPersonalEmail property, a boolean for whether or not
// personal email is being read (whether the sub-board code is "mail")
//
// Parameters:
//  pSubCode: The sub-board code to set in the object
function DigDistMsgReader_SetSubBoardCode(pSubCode)
{
	this.subBoardCode = pSubCode;
	this.readingPersonalEmail = (this.subBoardCode.toLowerCase() == "mail");
}

// For the DigDistMsgReader class: Populates the hdrsForCurrentSubBoard
// array with message headers from the current sub-board.  Filters out
// messages that are deleted, unvalidated, private, and voting messages.
function DigDistMsgReader_PopulateHdrsForCurrentSubBoard()
{
	if (this.subBoardCode == "mail")
	{
		this.hdrsForCurrentSubBoard = [];
		this.hdrsForCurrentSubBoardByMsgNum = {};
		return;
	}

	var tmpHdrs = null;
	var msgbase = new MsgBase(this.subBoardCode);
	if (msgbase.open())
	{
		// First get all headers in a temporary array, then filter them into
		// this.hdrsForCurrentSubBoard.
		// Note: get_all_msg_headers() was added in Synchronet 3.16.  DDMsgReader requires a minimum
		// of 3.18, so we're okay to use it.
		// The first parameter is whether to include votes (the parameter was introduced in Synchronet 3.17+).
		// We used to pass false here.
		tmpHdrs = msgbase.get_all_msg_headers(true);
		msgbase.close();
	}

	// Filter the headers into this.hdrsForCurrentSubBoard
	this.FilterMsgHdrsIntoHdrsForCurrentSubBoard(tmpHdrs, true);
}

// For the DigDistMsgReader class: Takes an array of message headers in the current
// sub-board and filters them into this.hdrsForCurrentSubBoard and
// this.hdrsForCurrentSubBoardByMsgNum based on which messages are readable to the
// user.
//
// Parameters:
//  pMsgHdrs: An array/object of message header objects
//  pClearFirst: Boolean - Whether or not to empty this.hdrsForCurrentSubBoard
//               and this.hdrsForCurrentSubBoardByMsgNum first.
function DigDistMsgReader_FilterMsgHdrsIntoHdrsForCurrentSubBoard(pMsgHdrs, pClearFirst)
{
	if (pClearFirst)
	{
		this.hdrsForCurrentSubBoard = [];
		this.hdrsForCurrentSubBoardByMsgNum = {};
	}
	if (pMsgHdrs == null)
		return;

	for (var prop in pMsgHdrs)
	{
		// Only add the message header if the message is readable to the user
		// and the from & to name isn't in the user's personal twitlist.
		// this.hdrsForCurrentSubBoardByMsgNum also has to be populated, but
		// that's done later in this function, in case this.hdrsForCurrentSubBoard
		// needs to be sorted.
		if (isReadableMsgHdr(pMsgHdrs[prop], this.subBoardCode) && !this.MsgHdrFromOrToInUserTwitlist(pMsgHdrs[prop]))
		{
			this.hdrsForCurrentSubBoard.push(pMsgHdrs[prop]);
			// This isn't done right here anymore due to the possibility of
			// this.hdrsForCurrentSubBoard being sorted
			//this.hdrsForCurrentSubBoardByMsgNum[pMsgHdrs[prop].number] = this.hdrsForCurrentSubBoard.length - 1;
		}
	}

	// If the sort type is date/time written, then sort the message header
	// array as such
	if (this.msgListSort == MSG_LIST_SORT_DATETIME_WRITTEN)
		this.hdrsForCurrentSubBoard.sort(sortMessageHdrsByDateTime);

	// Populate this.hdrsForCurrentSubBoardByMsgNum (this needs to be done here
	// based on the order of this.hdrsForCurrentSubBoard)
	for (var idx = 0; idx < this.hdrsForCurrentSubBoard.length; ++idx)
		this.hdrsForCurrentSubBoardByMsgNum[this.hdrsForCurrentSubBoard[idx].number] = idx;
}

// For the DigDistMsgReader class: Gets the message offset (index) for a message, given
// a message header.  Returns -1 on failure.  The returned index is for the object's
// message header array(s), if populated, in the priority of search headers, then
// hdrsForCurrentSubBoard.  If neither of those are populated, the offset of the header
// in the messagebase will be returned.
//
// Parameters:
//  pHdrOrMsgNum: Can either be a message header object or a message number.
//
// Return value: The message index (or offset in the messagebase)
function DigDistMsgReader_GetMsgIdx(pHdrOrMsgNum)
{
	var msgNum = -1;
	if (typeof(pHdrOrMsgNum) == "object")
		msgNum = pHdrOrMsgNum.number;
	else if (typeof(pHdrOrMsgNum) == "number")
		msgNum = pHdrOrMsgNum;
	else
		return -1;

	if (typeof(msgNum) != "number")
		return -1;

	var msgIdx = 0;
	if (this.msgSearchHdrs.hasOwnProperty(this.subBoardCode) &&
	    (this.msgSearchHdrs[this.subBoardCode].indexed.length > 0))
	{
		for (var i = 0; i < this.msgSearchHdrs[this.subBoardCode].indexed.length; ++i)
		{
			if (this.msgSearchHdrs[this.subBoardCode].indexed[i].number == msgNum)
			{
				msgIdx = i;
				break;
			}
		}
	}
	else if (this.hdrsForCurrentSubBoard.length > 0)
	{
		if (this.hdrsForCurrentSubBoardByMsgNum.hasOwnProperty(msgNum))
			msgIdx = this.hdrsForCurrentSubBoardByMsgNum[msgNum];
		else
		{
			msgIdx = msgNumToIdxFromMsgbase(this.subBoardCode, msgNum);
			if (msgIdx != -1)
				this.hdrsForCurrentSubBoardByMsgNum[msgNum] = msgIdx;
		}
	}
	else
		msgIdx = msgNumToIdxFromMsgbase(this.subBoardCode, msgNum);
	return msgIdx;
}

// Given a sub-board code and message number, this function gets the index
// of that message from the Synchronet messagebase.  Returns -1 if not found.
//
// Parameters:
//  pSubCode: The sub-board code
//  pMsgNum: The message number
//
// Return value: The index of the message, or -1 if not found.
function msgNumToIdxFromMsgbase(pSubCode, pMsgNum)
{
	var msgIdx = -1;

	var msgbase = new MsgBase(pSubCode);
	if (msgbase.open())
	{
		var msgHdr =  msgbase.get_msg_header(false, pMsgNum, false);
		if (msgHdr != null)
			msgIdx = msgHdr.offset;
		msgbase.close();
	}

	return msgIdx;
}

// For the DigDistMsgReader class: Refreshes a message header in the message header
// arrays in this.msgSearchHdrs.
//
// Parameters:
//  pMsgIndex: The index (0-based) of the message header
//  pAttrib: Optional - An attribute to apply.  If this is is not specified,
//           then the message header will be retrieved from the message base.
//  pApply: Optional boolean - Whether or not to apply the attribute or remove it. Defaults to true.
//  pSubBoardCode: Optional - An internal sub-board code.  If not specified, then
//                 this method will default to this.subBoardCode.
function DigDistMsgReader_RefreshSearchResultMsgHdr(pMsgIndex, pAttrib, pApply, pSubBoardCode)
{
	if (typeof(pMsgIndex) != "number")
		return;

	var applyAttr = (typeof(pApply) === "boolean" ? pApply : true);
	var subCode = (typeof(pSubBoardCode) == "string" ? pSubBoardCode : this.subBoardCode);
	var msgbase = new MsgBase(subCode);
	if (msgbase.open())
	{
		if (this.msgSearchHdrs.hasOwnProperty(subCode))
		{
			var msgNum = pMsgIndex + 1;
			if (typeof(pAttrib) != "undefined")
			{
				if (this.msgSearchHdrs[this.subBoardCode].indexed.hasOwnProperty(pMsgIndex))
				{
					if (applyAttr)
						this.msgSearchHdrs[this.subBoardCode].indexed[pMsgIndex].attr = this.msgSearchHdrs[this.subBoardCode].indexed[pMsgIndex].attr | pAttrib;
					else
						this.msgSearchHdrs[this.subBoardCode].indexed[pMsgIndex].attr = this.msgSearchHdrs[this.subBoardCode].indexed[pMsgIndex].attr ^ pAttrib;
					var msgOffsetFromHdr = this.msgSearchHdrs[this.subBoardCode].indexed[pMsgIndex].offset;
					msgbase.put_msg_header(true, msgOffsetFromHdr, this.msgSearchHdrs[this.subBoardCode].indexed[pMsgIndex]);
				}
			}
			else
			{
				var msgHeader = this.GetMsgHdrByIdx(pMsgIndex);
				if (this.msgSearchHdrs[this.subBoardCode].indexed.hasOwnProperty(pMsgIndex))
				{
					this.msgSearchHdrs[this.subBoardCode].indexed[pMsgIndex] = msgHeader;
					msgbase.put_msg_header(true, msgHeader.offset, msgHeader);
				}
			}
		}
		msgbase.close();
	}
}

// For the DigDistMsgReader class: Refreshes a message header in the message header
// array for the current sub-board.
//
// Parameters:
//  pMsgIndex: The index (0-based) of the message header
//  pAttrib: Optional - An attribute to apply.  If this is is not specified,
//           then the message header will be retrieved from the message base.
//  pApply: Optional boolean - Whether or not to apply the attribute or remove it. Defaults to true.
function DigDistMsgReader_RefreshHdrInSubBoardHdrs(pMsgIndex, pAttrib, pApply)
{
	if (typeof(pMsgIndex) != "number")
		return;

	if ((pMsgIndex >= 0) && (pMsgIndex < this.hdrsForCurrentSubBoard.length))
	{
		var applyAttr = (typeof(pApply) === "boolean" ? pApply : true);
		if (applyAttr)
			this.hdrsForCurrentSubBoard[pMsgIndex].attr = this.hdrsForCurrentSubBoard[pMsgIndex].attr | pAttrib;
		else
			this.hdrsForCurrentSubBoard[pMsgIndex].attr = this.hdrsForCurrentSubBoard[pMsgIndex].attr ^ pAttrib;
	}
}

// For the DigDistMsgReader class: Refreshes a message header in the saved message
// header arrays.
//
// Parameters:
//  pMsgIndex: The index (0-based) of the message header
//  pAttrib: Optional - An attribute to apply.  If this is is not specified,
//           then the message header will be retrieved from the message base.
//  pApply: Optional boolean - Whether or not to apply the attribute or remove it. Defaults to true.
//  pSubBoardCode: Optional - An internal sub-board code.  If not specified, then
//                 this method will default to this.subBoardCode.
function DigDistMsgReader_RefreshHdrInSavedArrays(pMsgIndex, pAttrib, pApply, pSubBoardCode)
{
	var applyAttr = (typeof(pApply) === "boolean" ? pApply : true);
	this.RefreshSearchResultMsgHdr(pMsgIndex, pAttrib, applyAttr, pSubBoardCode);
	this.RefreshHdrInSubBoardHdrs(pMsgIndex, pAttrib, applyAttr);
}

// For the DigDistMsgReader class: Inputs search text from the user, then reads/lists
// messages, which will perform the search.
//
// Paramters:
//  pSearchModeStr: A string to specify the lister mode to use - This can
//                  be one of the search modes to specify how to search:
//                  "keyword_search": Search the message subjects & bodies by keyword
//                  "from_name_search": Search messages by from name
//                  "to_name_search": Search messages by to name
//                  "to_user_search": Search messages by to name, to the logged-in user
//  pSubBoardCode: Optional - The Synchronet sub-board code, or "mail"
//                 for personal email.  Or, this can be the a boolean false for scan
//                 search mode to scan through sub-boards while searching each of them.
//  pScanScopeChar: Optional string with a character specifying "A" to scan all sub-boards,
//                  "G" for the current message group, or "S" for the user's current sub-board.
//                  If this is not specified, the current sub-board will be used.
//  pTxtToSearch: Optional - Text to search for (if specified, this won't prompt the user for search text)
//  pSkipSubBoardScanCfgCheck: Optional boolean - Whether or not to skip the sub-board scan config check for
//                                                  each sub-board.  Defaults to false.
function DigDistMsgReader_SearchMessages(pSearchModeStr, pSubBoardCode, pScanScopeChar, pTxtToSearch, pSkipSubBoardScanCfgCheck)
{
	var searchTextProvided = (typeof(pTxtToSearch) === "string" && pTxtToSearch != "");
	var skipSubBoardScanCfgCheck = (typeof(pSkipSubBoardScanCfgCheck) === "boolean" ? pSkipSubBoardScanCfgCheck : false);
	// Convert the search mode string to an integer representing the search
	// mode.  If we get back -1, that means the search mode string was invalid.
	// If that's the case, simply list messages.  Otherwise, do the search.
	this.searchType = searchTypeStrToVal(pSearchModeStr);
	if (this.searchType == SEARCH_NONE) // No search; search mode string was invalid
	{
		// Clear the search information and read/list messages.
		this.ClearSearchData();
		this.ReadOrListSubBoard(pSubBoardCode);
	}
	else
	{
		// The search mode string was valid, so go ahead and search.
		console.print("\x01n");
		console.crlf();
		var subCode = "";
		if (typeof(pScanScopeChar) !== "string")
		{
			subCode = (typeof(pSubBoardCode) === "string" ? pSubBoardCode : this.subBoardCode);
			if (subCode == "mail")
			{
				//console.print("\x01n" + replaceAtCodesInStr(this.text.searchingPersonalMailText));
				console.putmsg("\x01n" + this.text.searchingPersonalMailText);
			}
			else
			{
				var formattedText = format(this.text.searchingSubBoardAbovePromptText, subBoardGrpAndName(bbs.cursub_code));
				//console.print("\x01n" + replaceAtCodesInStr(formattedText) + "\x01n");
				console.putmsg("\x01n" + formattedText + "\x01n");
			}
			console.crlf();
		}
		// Output the prompt text to the user (for modes where a prompt is needed)
		switch (this.searchType)
		{
			case SEARCH_KEYWORD:
				if (!searchTextProvided)
				{
					//console.print("\x01n" + replaceAtCodesInStr(this.text.searchTextPromptText));
					console.putmsg("\x01n" + this.text.searchTextPromptText);
				}
				else
					console.print("\x01n\x01gSearching for: \x01c" + pTxtToSearch + "\x01n\r\n");
				break;
			case SEARCH_FROM_NAME:
				if (!searchTextProvided)
				{
					//console.print("\x01n" + replaceAtCodesInStr(this.text.fromNamePromptText));
					console.putmsg("\x01n" + this.text.fromNamePromptText);
				}
				else
					console.print("\x01n\x01gSearching for: \x01c" + pTxtToSearch + "\x01n\r\n");
				break;
			case SEARCH_TO_NAME_CUR_MSG_AREA:
				if (!searchTextProvided)
				{
					//console.print("\x01n" + replaceAtCodesInStr(this.text.toNamePromptText));
					console.putmsg("\x01n" + this.text.toNamePromptText);
				}
				else
					console.print("\x01n\x01gSearching for: \x01c" + pTxtToSearch + "\x01n\r\n");
				break;
			case SEARCH_TO_USER_CUR_MSG_AREA:
				// Note: No prompt needed for this - Will search for the user's name/handle
				console.line_counter = 0; // To prevent a pause before the message list comes up
				break;
			default:
				break;
		}
		//var promptUserForText = this.SearchTypePopulatesSearchResults();
		var promptUserForText = this.SearchTypeRequiresSearchText();
		// Get the search text from the user
		if (promptUserForText)
		{
			if (searchTextProvided)
				this.searchString = pTxtToSearch;
			else
				this.searchString = console.getstr(512, K_UPPER);
		}
		// If the user was prompted for search text but no search text was entered,
		// then show an abort message and don't do anything.  Otherwise, go ahead
		// and list/read messages.
		if (promptUserForText && (this.searchString.length == 0))
		{
			this.ClearSearchData();
			//console.print("\x01n" + replaceAtCodesInStr(this.text.abortedText));
			//console.crlf();
			console.putmsg("\x01n" + this.text.abortedText);
			console.pause();
			return;
		}
		else
		{
			// If pScanScopeChar is a string, then do scan search mode.  Otherwise,
			// scan/search the current sub-board.
			if (typeof(pScanScopeChar) === "string" && (pScanScopeChar === "S" || pScanScopeChar === "G" || pScanScopeChar === "A"))
			{
				var subBoardCodeBackup = this.subBoardCode;
				var subBoardsToScan = getSubBoardsToScanArray(pScanScopeChar);
				this.doingMsgScan = true;
				var continueScan = true;
				var userAborted = false;
				this.doingMultiSubBoardScan = (subBoardsToScan.length > 1);
				// If the sub-board's access requirements allows the user to read it
				// and it's enabled in the user's message scan configuration, then go
				// ahead with this sub-board.
				// Note: Used to use this to determine whether the user could access the
				// sub-board:
				//user.compare_ars(msg_area.grp_list[grpIndex].sub_list[subIndex].ars)
				// Now using the can_read property.
				for (var subCodeIdx = 0; (subCodeIdx < subBoardsToScan.length) && continueScan; ++subCodeIdx)
				{
					subCode = subBoardsToScan[subCodeIdx];
					if (skipSubBoardScanCfgCheck || (msg_area.sub[subCode].can_read && ((msg_area.sub[subCode].scan_cfg & SCAN_CFG_NEW) == SCAN_CFG_NEW)))
					{
						// Force garbage collection to ensure enough memory is available to continue
						js.gc(true);
						// Set the console line counter to 0 to prevent screen pausing
						// when the "Searching ..." and "No messages were found" text is
						// displayed repeatedly
						console.line_counter = 0;
						// let the user read the sub-board (and toggle betweeen reading and
						// listing)
						var readOrListRetObj = this.ReadOrListSubBoard(subCode, null, true, true, false, true, READER_MODE_READ);
						console.print("\x01n");
						console.crlf();
						//if (this.SearchTypePopulatesSearchResults())
						//	console.print("\x01n\r\nSearching...");
						console.line_counter = 0;
						if (readOrListRetObj.stoppedReading)
							break;
					}
				}
				this.subBoardCode = subBoardCodeBackup;
				console.pause();
			}
			else
				this.ReadOrListSubBoard(subCode);
			// Clear the search data so that subsequent listing or reading sessions
			// don't repeat the same search
			this.ClearSearchData();
		}
	}
}

// For the DigDistMsgReader class: Performs a message search scan through sub-boards.
// Prompts the user for Sub-board/Group/All, then inputs search text from the user, then
// reads/lists messages through the sub-boards, performing the search in each sub-board.
//
// Paramters:
//  pSearchModeStr: A string to specify the lister mode to use - This can
//                  be one of the search modes to specify how to search:
//                  "keyword_search": Search the message subjects & bodies by keyword
//                  "from_name_search": Search messages by from name
//                  "to_name_search": Search messages by to name
//                  "to_user_search": Search messages by to name, to the logged-in user
//  pTxtToSearch: Optional - Text to search for (if specified, this won't prompt the user for search text)
//  pSubCode: Optional - An internal code of a sub-board if scanning just one sub-board
function DigDistMsgReader_SearchMsgScan(pSearchModeStr, pTxtToSearch, pSubCode)
{
	if (typeof(pSearchModeStr) !== "string" || pSearchModeStr.length == 0)
		return;

	// If the given sub-board code is valid, then use that and scan only in that
	// sub-board.  Otherwise, prompt the user for sub-board, group, or all, then
	// call SearchMessages to do the search.
	var scanScopeChar = "";
	var previousSubBoardCode = null;
	if (typeof(pSubCode) === "string" && subBoardCodeIsValid(pSubCode))
	{
		var previousSubBoardCode = this.subBoardCode;
		this.subBoardCode = pSubCode;
		scanScopeChar = "S";
	}
	else
	{
		console.mnemonics(bbs.text(SubGroupOrAll));
		scanScopeChar = console.getkeys("SGAC").toString();
	}
	if (scanScopeChar.length > 0)
		this.SearchMessages(pSearchModeStr, null, scanScopeChar, pTxtToSearch, true); // Skip/ignore scan config checks
	else
	{
		console.crlf();
		//console.print(replaceAtCodesInStr(this.text.msgScanAbortedText));
		//console.crlf();
		console.putmsg(this.text.msgScanAbortedText);
		console.pause();
	}
	// Restore this.subBoardCode if necessary
	if (typeof(previousSubBoardCode) === "string")
		this.subBoardCode = previousSubBoardCode;
}

// This function clears the search data from the object.
function DigDistMsgReader_ClearSearchData()
{
   this.searchType = SEARCH_NONE;
   this.searchString == "";
   if (this.msgSearchHdrs != null)
   {
		for (var subCode in this.msgSearchHdrs)
		{
			delete this.msgSearchHdrs[subCode].indexed;
			delete this.msgSearchHdrs[subCode];
		}
		delete this.msgSearchHdrs;
		this.msgSearchHdrs = {};
   }
}

// For the DigDistMsgReader class: Performs message reading/listing.
// Depending on the value of this.startMode, starts in either reader
// mode or lister mode.  Uses an input loop to let the user switch
// between the two modes.
//
// Parameters:
//  pSubBoardCode: Optional - The internal code of a sub-board to read.
//                 If not specified, the internal sub-board code specified
//                 when creating the object will be used.
//  pStartingMsgOffset: Optional - The offset of a message to start at
//  pAllowChgArea: Optional boolean - Whether or not to allow changing the
//                 message area
//  pReturnOnNextAreaNav: Optional boolean - Whether or not this method should
//                        return when it would move to the next message area due
//                        navigation from the user (i.e., with the right arrow key)
//  pPauseOnNoMsgSrchResults: Optional boolean - Whether or not to pause when
//                            a message search doesn't find any search results
//                            in the current sub-board.  Defaults to true.
//  pPromptToGoNextIfNoResults: Optional boolean - Whether or not to prompt the user
//                         to go onto the next/previous sub-board if there are no
//                         search results in the current sub-board.  Defaults to true.
//  pInitialModeOverride: Optional (numeric) to override the initial mode in this
//                        function (READER_MODE_READ or READER_MODE_LIST).  If not
//                        specified, defaults to this.startMode.
//
// Return value: An object with the following properties:
//               stoppedReading: Boolean - Whether or not the user stopped reading.
//                               This can also be true if there is an error.
function DigDistMsgReader_ReadOrListSubBoard(pSubBoardCode, pStartingMsgOffset,
                                             pAllowChgArea, pReturnOnNextAreaNav,
                                             pPauseOnNoMsgSrchResults,
                                             pPromptToGoNextIfNoResults,
                                             pInitialModeOverride)
{
	var retObj = {
		stoppedReading: false
	};

	// Set the sub-board code if applicable
	var previousSubBoardCode = this.subBoardCode;
	if (typeof(pSubBoardCode) == "string")
	{
		if (subBoardCodeIsValid(pSubBoardCode))
			this.setSubBoardCode(pSubBoardCode);
		else
		{
			console.print("\x01n\x01h\x01yWarning: \x01wThe Message Reader connot continue because an invalid");
			console.crlf();
			console.print("sub-board code was specified (" + pSubBoardCode + "). Please notify the sysop.");
			console.crlf();
			console.pause();
			retObj.stoppedReading = true;
			return retObj;
		}
	}

	// If the user doesn't have permission to read the current sub-board, then
	// don't allow the user to read it.
	if (this.subBoardCode != "mail")
	{
		if (!msg_area.sub[this.subBoardCode].can_read)
		{
			var errorMsg = format(bbs.text(CantReadSub), msg_area.sub[this.subBoardCode].grp_name, msg_area.sub[this.subBoardCode].name);
			console.print("\x01n" + errorMsg);
			console.pause();
			retObj.stoppedReading = true;
			return retObj;
		}
	}

	// Populate this.msgSearchHdrs for the current sub-board if there is a search
	// specified.  If there are no messages to read in the current sub-board, then
	// just return.
	var pauseOnNoSearchResults = (typeof(pPauseOnNoMsgSrchResults) == "boolean" ? pPauseOnNoMsgSrchResults : true);
	if (!this.PopulateHdrsIfSearch_DispErrorIfNoMsgs(true, true, pauseOnNoSearchResults))
	{
		retObj.stoppedReading = false;
		return retObj;
	}
	// If not searching, then populate the array of all readable headers for the
	// current sub-board.
	if (!this.SearchingAndResultObjsDefinedForCurSub())
		this.PopulateHdrsForCurrentSubBoard();

	// Check the pAllowChgArea parameter.  If it's a boolean, then use it.  If
	// not, then check to see if we're reading personal mail - If not, then allow
	// the user to change to a different message area.
	var allowChgMsgArea = true;
	if (typeof(pAllowChgArea) == "boolean")
		allowChgMsgArea = pAllowChgArea;
	else
		allowChgMsgArea = (this.subBoardCode != "mail");
	// If reading personal email and messages haven't been collected (searched)
	// yet, then do so now.
	if (this.readingPersonalEmail && (!this.msgSearchHdrs.hasOwnProperty(this.subBoardCode)))
		this.msgSearchHdrs[this.subBoardCode] = searchMsgbase(this.subBoardCode, this.searchType, this.searchString, this.readingPersonalEmailFromUser);

	// Determine whether to start in list or reader mode, depending
	// on the value of this.startMode.
	var readerMode = this.startMode;
	// If an initial mode override was specified and is valid, then use it.
	if (typeof(pInitialModeOverride) === "number" && (pInitialModeOverride == READER_MODE_READ || pInitialModeOverride == READER_MODE_LIST))
		readerMode = pInitialModeOverride;
	// User input loop
	var selectedMessageOffset = 0;
	if (typeof(pStartingMsgOffset) == "number")
		selectedMessageOffset = pStartingMsgOffset;
	else if (this.SearchingAndResultObjsDefinedForCurSub())
	{
		// If reading personal mail, start at the first unread message index
		// (or the last message, if all messages have been read)
		if (this.readingPersonalEmail)
		{
			selectedMessageOffset = this.GetLastReadMsgIdxAndNum(false).lastReadMsgIdx; // Used to be true
			if ((selectedMessageOffset > -1) && (selectedMessageOffset < this.NumMessages() - 1))
				++selectedMessageOffset;
		}
		else
			selectedMessageOffset = 0;
	}
	else if (this.hdrsForCurrentSubBoard.length > 0)
	{
		selectedMessageOffset = this.GetMsgIdx(GetScanPtrOrLastMsgNum(this.subBoardCode));
		if (selectedMessageOffset < 0)
			selectedMessageOffset = 0;
		else if (selectedMessageOffset >= this.hdrsForCurrentSubBoard.length)
			selectedMessageOffset = this.hdrsForCurrentSubBoard.length - 1;
	}
	else
		selectedMessageOffset = -1;
	var otherRetObj = null;
	var continueOn = true;
	while (continueOn)
	{
		switch (readerMode)
		{
			case READER_MODE_READ:
				// Call the ReadMessages method - DOn't change the sub-board,
				// and pass the selected index of the message to read.  If that
				// index is -1, the ReadMessages method will use the user's
				// last-read message index.
				otherRetObj = this.ReadMessages(null, selectedMessageOffset, true, allowChgMsgArea,
				                                pReturnOnNextAreaNav, pPromptToGoNextIfNoResults);
				// If the user wants to quit or if there was an error, then stop
				// the input loop.
				if (otherRetObj.stoppedReading)
				{
					retObj.stoppedReading = true;
					continueOn = false;
				}
				// If we're set to return on navigation to the next message area and
				// the user's last keypress was the right arrow key or next action
				// was to go to the next message area, then don't continue the input
				// loop, and also say that the user didn't stop reading.
				else if (pReturnOnNextAreaNav &&
				         ((otherRetObj.lastUserInput == KEY_RIGHT) || (otherRetObj.lastUserInput == KEY_ENTER) || (otherRetObj.lastAction == ACTION_GO_NEXT_MSG_AREA)))
				{
					retObj.stoppedReading = false;
					continueOn = false;
				}
				else if (otherRetObj.messageListReturn)
					readerMode = READER_MODE_LIST;
				break;
			case READER_MODE_LIST:
				// Note: Doing the message list is also handled in this.ReadMessages().
				// This code is here in case the reader is configured to start up
				// in list mode first.
				// List messages
				otherRetObj = this.ListMessages(null, pAllowChgArea);
				// If the user wants to quit, set continueOn to false to get out
				// of the loop.  Otherwise, set the selected message offset to
				// what the user chose from the list.
				if (otherRetObj.lastUserInput == "Q")
				{
					retObj.stoppedReading = true;
					continueOn = false;
				}
				else
				{
					selectedMessageOffset = otherRetObj.selectedMsgOffset;
					readerMode = READER_MODE_READ;
				}
				break;
			default:
				break;
		}
	}

	console.clear("\x01n");

	return retObj;
}
// Helper for DigDistMsgReader_ReadOrListSubBoard(): Populates this.msgSearchHdrs
// if an applicable search type is specified; also, if there are no messages in
// the current sub-board, outputs an error to the user.
//
// Parameters:
//  pCloseMsgbaseAndSetNullIfNoMsgs: Optional boolean - Whether or not to close the message
//                         base if there are no messages.  Defaults to true.
//  pOutputMessages: Boolean - Whether or not to output messages to the screen.
//                   Defaults to true.
//  pPauseOnNoMsgError: Optional boolean - Whether or not to pause for a keypress
//                      after displaying the "no messages" error.  Defaults to true.
//
// Return value: Boolean - Whether or not there are messages to read in the current
//               sub-board
function DigDistMsgReader_PopulateHdrsIfSearch_DispErrorIfNoMsgs(pCloseMsgbaseAndSetNullIfNoMsgs,
                                                 pOutputMessages, pPauseOnNoMsgError)
{
	var thereAreMessagesToRead = true;

	var outputMessages = (typeof(pOutputMessages) == "boolean" ? pOutputMessages : true);

	// If a search is is specified that would populate the search results, then
	// perform the message search for the current sub-board.
	if (this.SearchTypePopulatesSearchResults())
	{
		if (!this.msgSearchHdrs.hasOwnProperty(this.subBoardCode))
		{
			// TODO: In case new messages were posted in this sub-board, it might help
			// to check the current number of messages vs. the previous number of messages
			// and search the new messages if there are more.
			if (outputMessages)
			{
				console.crlf();
				var formattedText = "";
				if (this.readingPersonalEmail)
					formattedText = format(this.text.loadingPersonalMailText, subBoardGrpAndName(this.subBoardCode));
				else
					formattedText = format(this.text.searchingSubBoardText, subBoardGrpAndName(this.subBoardCode));
				//console.print("\x01n" + replaceAtCodesInStr(formattedText) + "\x01n");
				console.putmsg("\x01n" + formattedText + "\x01n");
			}
			this.msgSearchHdrs[this.subBoardCode] = searchMsgbase(this.subBoardCode, this.searchType, this.searchString, this.readingPersonalEmailFromUser);
		}
	}
	else
	{
		// There is no search is specified, so clear the search results for the
		// current sub-board to help ensure that there are messages to read.
		if (this.msgSearchHdrs.hasOwnProperty(this.subBoardCode))
		{
			delete this.msgSearchHdrs[this.subBoardCode].indexed;
			delete this.msgSearchHdrs[this.subBoardCode];
		}
	}

	// If there are no messages to display in the current sub-board, then set the
	// return value and let the user know (if outputMessages is true).
	if (this.NumMessages() == 0)
	{
		thereAreMessagesToRead = false;
		if (outputMessages)
		{
			console.print("\x01n");
			console.crlf();
			if (this.readingPersonalEmail)
			{
				//console.print(replaceAtCodesInStr(this.text.noPersonalEmailText));
				console.putmsg(this.text.noPersonalEmailText);
			}
			else
			{
				var formattedText = "";
				if (this.msgSearchHdrs.hasOwnProperty(this.subBoardCode))
					formattedText = format(this.text.noSearchResultsInSubBoardText, subBoardGrpAndName(this.subBoardCode));
				else
					formattedText = format(this.text.noMessagesInSubBoardText, subBoardGrpAndName(this.subBoardCode));
				//console.print(replaceAtCodesInStr(formattedText));
				console.putmsg(formattedText);
			}
			console.crlf();
			var pauseOnNoMsgsError = (typeof(pPauseOnNoMsgError) == "boolean" ? pPauseOnNoMsgError : true);
			if (pauseOnNoMsgsError)
				console.pause();
		}
	}

	return thereAreMessagesToRead;
}

// For the DigDistMsgReader class: Returns whether the search type is a type
// that would result in the search results structure being populated.  Search
// types where that wouldn't happen are SEARCH_NONE (no search) and any of the
// message scan search types.
function DigDistMsgReader_SearchTypePopulatesSearchResults()
{
	return (this.readingPersonalEmail || searchTypePopulatesSearchResults(this.searchType));
}

// For the DigDistMsgReader class: Returns whether the search type is a type
// that requires search text.  Search types that require search text are the
// keyword search, from name search, and to name search.  Search types that
// don't require search text are SEARCH_NONE (no search) & the message scan search
// types.
function DigDistMsgReader_SearchTypeRequiresSearchText()
{
	return searchTypeRequiresSearchText(this.searchType);
}

// Returns whether a search type value would populate search results.
//
// Parameters:
//  pSearchType: A search type integer value
//
// Return value: Boolean - Whether or not the search type would populate search
//               results
function searchTypePopulatesSearchResults(pSearchType)
{
	return ((pSearchType == SEARCH_KEYWORD) ||
	        (pSearchType == SEARCH_FROM_NAME) ||
	        (pSearchType == SEARCH_TO_NAME_CUR_MSG_AREA) ||
	        (pSearchType == SEARCH_TO_USER_CUR_MSG_AREA) ||
	        (pSearchType == SEARCH_TO_USER_NEW_SCAN) ||
			(pSearchType == SEARCH_TO_USER_NEW_SCAN_CUR_SUB) ||
	        (pSearchType == SEARCH_TO_USER_NEW_SCAN_CUR_GRP) ||
	        (pSearchType == SEARCH_TO_USER_NEW_SCAN_ALL) ||
	        (pSearchType == SEARCH_ALL_TO_USER_SCAN));
}

// Returns whether a search type value requires search text.
//
// Parameters:
//  pSearchType: A search type integer value
//
// Return value: Boolean - Whether or not the search type requires search text
function searchTypeRequiresSearchText(pSearchType)
{
	return ((pSearchType == SEARCH_KEYWORD) ||
	         (pSearchType == SEARCH_FROM_NAME) ||
	         (pSearchType == SEARCH_TO_NAME_CUR_MSG_AREA));
}

// For the DigDistMsgReader class: Scans the message area(s) for new messages,
// unread messages to the user, or all messages to the user.
//
// Parameters:
//  pScanCfgOpt: The scan configuration option to check for in the sub-boards
//               (from sbbsdefs.js). Supported values are SCAN_CFG_NEW (new
//               message scan), and SCAN_CFG_TOYOU (messages to the user)
//  pScanMode: The scan mode (from sbbsdefs.js).  Supported values are SCAN_NEW
//             (new message scan), SCAN_TOYOU (scan for all messages to the
//             user), and SCAN_UNREAD (scan for new messages to the user).
//  pScanScopeChar: Optional - A character (as a string) representing the scan
//                  scope: "S" for sub-board, "G" for group, or "A" for all.
//                  If this is not specified, the user will be prompted for the
//                  scan scope.
function DigDistMsgReader_MessageAreaScan(pScanCfgOpt, pScanMode, pScanScopeChar)
{
	var scanScopeChar = "";
	if ((typeof(pScanScopeChar) == "string") && /^[SGA]$/.test(pScanScopeChar))
		scanScopeChar = pScanScopeChar;
	else
	{
		// Prompt the user to scan in the current sub-board, the current message group,
		// or all.  Default to all.
		console.print("\x01n");
		console.mnemonics(bbs.text(SubGroupOrAll));
		scanScopeChar = console.getkeys("SGAC").toString();
		// If the user just pressed Enter without choosing anything, then abort and return.
		if (scanScopeChar.length == 0)
		{
			console.crlf();
			//console.print(replaceAtCodesInStr(this.text.msgScanAbortedText));
			//console.crlf();
			console.putmsg(this.text.msgScanAbortedText);
			console.pause();
			return;
		}
	}

	// Do some logging if verbose logging is enabled
	if (gCmdLineArgVals.verboselogging)
	{
		var logMessage = "Doing a message area scan (";
		if (pScanCfgOpt == SCAN_CFG_NEW)
		{
			// The only valid value for pScanMode in this case is SCAN_NEW, so no
			// need to check pScanMode to append more to the log message.
			logMessage += "new";
		}
		else if (pScanCfgOpt == SCAN_CFG_TOYOU)
		{
			// Valid values for pScanMode in this case are SCAN_UNREAD and SCAN_TOYOU.
			if (pScanMode == SCAN_UNREAD)
				logMessage += "unread messages to the user";
			else if (pScanMode == SCAN_TOYOU)
				logMessage += "all messages to the user";
		}
		if (scanScopeChar == "A") // All sub-boards
			logMessage += ", all sub-boards";
		else if (scanScopeChar == "G") // Current message group
		{
			logMessage += ", current message group (" +
			              msg_area.grp_list[bbs.curgrp].description + ")";
		}
		else if (scanScopeChar == "S") // Current sub-board
		{
			logMessage += ", current sub-board (" +
			              msg_area.grp_list[bbs.curgrp].description + " - " +
						  msg_area.grp_list[bbs.curgrp].sub_list[bbs.cursub].description + ")";
		}
		logMessage += ")";
		writeToSysAndNodeLog(logMessage);
	}

	// Save the original search type, sub-board code, searched message headers,
	// etc. to be restored later
	var originalSearchType = this.searchType;
	var originalSubBoardCode = this.subBoardCode;
	var originalBBSCurGrp = bbs.curgrp;
	var originalBBSCurSub = bbs.cursub;
	var originalMsgSrchHdrs = this.msgSearchHdrs;

	// Make sure there is no search data
	this.ClearSearchData();

	// Create an array of internal codes of sub-boards to scan
	var subBoardsToScan = getSubBoardsToScanArray(scanScopeChar);
	// Scan through the sub-boards
	this.doingMsgScan = true;
	var continueNewScan = true;
	var userAborted = false;
	this.doingMultiSubBoardScan = (subBoardsToScan.length > 1);
	for (var subCodeIdx = 0; (subCodeIdx < subBoardsToScan.length) && continueNewScan; ++subCodeIdx)
	{
		// Force garbage collection to ensure enough memory is available to continue
		js.gc(true);
		// Set the console line counter to 0 to prevent screen pausing
		// when the "Searching ..." and "No messages were found" text is
		// displayed repeatedly
		console.line_counter = 0;
		// If the sub-board's access requirements allows the user to read it
		// and it's enabled in the user's message scan configuration, then go
		// ahead with this sub-board.
		// Note: Used to use this to determine whether the user could access the
		// sub-board:
		//user.compare_ars(msg_area.grp_list[grpIndex].sub_list[subIndex].ars)
		// Now using the can_read property.
		this.setSubBoardCode(subBoardsToScan[subCodeIdx]); // Needs to be set before getting the last read/scan pointer index
		if (msg_area.sub[this.subBoardCode].can_read && ((msg_area.sub[this.subBoardCode].scan_cfg & pScanCfgOpt) == pScanCfgOpt))
		{
			var grpIndex = msg_area.sub[this.subBoardCode].grp_index;
			var subIndex = msg_area.sub[this.subBoardCode].index;
			// Sub-board description: msg_area.grp_list[grpIndex].sub_list[subIndex].description
			// Open the sub-board and check for unread messages.  If there are any, then let
			// the user read the messages in the sub-board.
			//var msgbase = new MsgBasesubBoardsToScan[subCodeIdx]);
			var msgbase = new MsgBase(this.subBoardCode);
			if (msgbase.open())
			{
				// Get a filtered list of messages for this sub-board
				this.PopulateHdrsForCurrentSubBoard();

				//this.setSubBoardCode(subBoardsToScan[subCodeIdx]); // Needs to be set before getting the last read/scan pointer index

				// If the current sub-board contains only deleted messages,
				// or if the user has already read the last message in this
				// sub-board, then skip it.
				var scanPtrMsgIdx = this.GetScanPtrMsgIdx();
				var nonDeletedMsgsExist = (this.FindNextNonDeletedMsgIdx(scanPtrMsgIdx-1, true) > -1);
				var userHasReadLastMessage = false;
				if (this.subBoardCode != "mail")
				{
					// What if newest_message_header.number is invalid  (e.g. NaN or 0xffffffff or >
					// msgbase.last_msg)?
					if (this.hdrsForCurrentSubBoard.length > 0)
					{
						if ((msg_area.sub[this.subBoardCode].last_read == this.hdrsForCurrentSubBoard[this.hdrsForCurrentSubBoard.length-1].number) ||
						    (scanPtrMsgIdx == this.hdrsForCurrentSubBoard.length-1))
						{
							userHasReadLastMessage = true;
						}
					}
				}
				if (!nonDeletedMsgsExist || userHasReadLastMessage)
				{
					if (msgbase != null)
						msgbase.close();
					continue;
				}

				// In the switch cases below, bbs.curgrp and bbs.cursub are
				// temporarily changed the user's sub-board to the current
				// sub-board so that certain @-codes (such as @GRP-L@, etc.)
				// are displayed by Synchronet correctly.

				// We might want the starting message index to be different
				// depending on the scan mode.
				switch (pScanMode)
				{
					case SCAN_NEW:
						// Make sure the sub-board has some messages.  Let the user read it if
						// the scan pointer index is -1 (one unread message) or if it points to
						// a message within the number of messages in the sub-board.
						var totalNumMsgs = msgbase.total_msgs;
						if ((totalNumMsgs > 0) && ((scanPtrMsgIdx == -1) || (scanPtrMsgIdx < totalNumMsgs-1)))
						{
							bbs.curgrp = grpIndex;
							bbs.cursub = subIndex;
							// Start at the scan pointer
							var startMsgIdx = scanPtrMsgIdx;
							// If the message has already been read, then start at the next message
							var tmpMsgHdr = this.GetMsgHdrByIdx(startMsgIdx);
							if ((tmpMsgHdr != null) && (msg_area.sub[this.subBoardCode].last_read == tmpMsgHdr.number) && (startMsgIdx < this.NumMessages(true) - 1))
								++startMsgIdx;
							// Allow the user to read messages in this sub-board.  Don't allow
							// the user to change to a different message area, don't pause
							// when there's no search results in a sub-board, and return
							// instead of going to the next sub-board via navigation.
							var readRetObj = this.ReadOrListSubBoard(null, startMsgIdx, false, true, false);
							// If the user stopped reading & decided to quit, then exit the
							// message scan loops.
							if (readRetObj.stoppedReading)
							{
								continueNewScan = false;
								userAborted = true;
							}
						}
						break;
					case SCAN_TOYOU: // All messages to the user
						bbs.curgrp = grpIndex;
						bbs.cursub = subIndex;
						// Search for messages to the user in the current sub-board
						// and let the user read the sub-board if messages are
						// found.  Don't allow the user to change to a different
						// message area, don't pause when there's no search results
						// in a sub-board, and return instead of going to the next
						// sub-board via navigation.
						this.searchType = SEARCH_TO_USER_CUR_MSG_AREA;
						var readRetObj = this.ReadOrListSubBoard(null, 0, false, true, false);
						// If the user stopped reading & decided to quit, then exit the
						// message scan loops.
						if (readRetObj.stoppedReading)
						{
							continueNewScan = false;
							userAborted = true;
						}
						break;
					case SCAN_UNREAD: // New (unread) messages to the user
						bbs.curgrp = grpIndex;
						bbs.cursub = subIndex;
						// Search for messages to the user in the current sub-board
						// and let the user read the sub-board if messages are
						// found.  Don't allow the user to change to a different
						// message area, don't pause when there's no search results
						// in a sub-board, and return instead of going to the next
						// sub-board via navigation.
						this.searchType = SEARCH_TO_USER_NEW_SCAN;
						var readRetObj = this.ReadOrListSubBoard(null, 0, false, true, false);
						// If the user stopped reading & decided to quit, then exit the
						// message scan loops.
						if (readRetObj.stoppedReading)
						{
							continueNewScan = false;
							userAborted = true;
						}
						break;
					default:
						break;
				}

				if (msgbase != null)
					msgbase.close();
			}
		}
		// Pause for a short moment to avoid causing CPU usage going to 99%
		mswait(10);
	}
	this.doingMultiSubBoardScan = false;


	// Restore the original sub-board code, searched message headers, etc.
	this.searchType = originalSearchType;
	this.setSubBoardCode(originalSubBoardCode);
	this.msgSearchHdrs = originalMsgSrchHdrs;
	bbs.curgrp = originalBBSCurGrp;
	bbs.cursub = originalBBSCurSub;
	if ((msgbase != null) && msgbase.is_open)
		msgbase.close();
	this.doingMultiSubBoardScan = false;
	this.doingMsgScan = false;

	if (this.pauseAfterNewMsgScan)
	{
		console.crlf();
		if (userAborted)
		{
			//console.print("\x01n" + replaceAtCodesInStr(this.text.msgScanAbortedText) + "\x01n");
			console.putmsg("\x01n" + this.text.msgScanAbortedText + "\x01n");
		}
		else
		{
			//console.print("\x01n" + replaceAtCodesInStr(this.text.msgScanCompleteText) + "\x01n");
			console.putmsg("\x01n" + this.text.msgScanCompleteText + "\x01n");
		}
		console.crlf();
		console.pause();
	}
}

// For the DigDistMsgReader class: Performs the message reading activity.
//
// Parameters:
//  pSubBoardCode: Optional - The internal code of a sub-board to read.
//                 If not specified, the internal sub-board code specified
//                 when creating the object will be used.
//  pStartingMsgOffset: Optional - The offset of a message to start at
//  pReturnOnMessageList: Optional boolean - Whether or not to quit when the
//                      user wants to list messages (used when this method
//                      is called from ReadOrListSubBoard()).
//  pAllowChgArea: Optional boolean - Whether or not to allow changing the
//                 message area
//  pReturnOnNextAreaNav: Optional boolean - Whether or not this method should
//                        return when it would move to the next message area due
//                        navigation from the user (i.e., with the right arrow
//                        key or with < (go to previous message area) or > (go
//                        to next message area))
//  pPromptToGoToNextAreaIfNoSearchResults: Optional boolean - Whether or not to
//                          prompt the user to go to the next/previous sub-board
//                          when there are no search results
//
// Return value: An object that has the following properties:
//               lastUserInput: The user's last keypress/input
//               lastAction: The last action chosen by the user based on their
//                           last keypress, etc.
//               stoppedReading: Boolean - Whether reading has stopped
//                               (due to user quitting, error, or otherwise)
//               messageListReturn: Boolean - Whether this method is returning for
//                                  the caller to display the message list.  This
//                                  will only be true when the pReturnOnMessageList
//                                  parameter is true and the user wants to list
//                                  messages.
function DigDistMsgReader_ReadMessages(pSubBoardCode, pStartingMsgOffset, pReturnOnMessageList,
                                       pAllowChgArea, pReturnOnNextAreaNav, pPromptToGoToNextAreaIfNoSearchResults)
{
	var retObj = {
		lastUserInput: "",
		lastAction: ACTION_NONE,
		stoppedReading: false,
		messageListReturn: false
	};

	// If the passed-in sub-board code was different than what was set in the object before,
	// then open the new message sub-board.
	var previousSubBoardCode = this.subBoardCode;
	if (typeof(pSubBoardCode) == "string")
	{
		if (subBoardCodeIsValid(pSubBoardCode))
			this.setSubBoardCode(pSubBoardCode);
		else
		{
			console.print("\x01n\x01h\x01yWarning: \x01wThe Message Reader connot continue because an invalid");
			console.crlf();
			console.print("sub-board code was specified (" + pSubBoardCode + "). Please notify the sysop.");
			console.crlf();
			console.pause();
			retObj.stoppedReading = true;
			return retObj;
		}
	}
	if (this.subBoardCode.length == 0)
	{
		console.print("\x01n\x01h\x01yWarning: \x01wThe Message Reader connot continue because no message");
		console.crlf();
		console.print("sub-board was specified. Please notify the sysop.");
		console.crlf();
		console.pause();
		retObj.stoppedReading = true;
		return retObj;
	}

	// If the user doesn't have permission to read the current sub-board, then
	// don't allow the user to read it.
	if (this.subBoardCode != "mail")
	{
		if (!msg_area.sub[this.subBoardCode].can_read)
		{
			var errorMsg = format(bbs.text(CantReadSub), msg_area.sub[this.subBoardCode].grp_name, msg_area.sub[this.subBoardCode].name);
			console.print("\x01n" + errorMsg);
			console.pause();
			retObj.stoppedReading = true;
			return retObj;
		}
	}

	var msgbase = new MsgBase(this.subBoardCode);
	// If the message base was not opened, then output an error and return.
	if (!msgbase.open())
	{
		console.print("\x01n");
		console.crlf();
		console.print("\x01h\x01y* \x01wUnable to open message sub-board:");
		console.crlf();
		console.print(subBoardGrpAndName(this.subBoardCode));
		console.crlf();
		console.pause();
		retObj.stoppedReading = true;
		return retObj;
	}

	// If there are no messages to display in the current sub-board, then let the
	// user know and exit.
	var numOfMessages = this.NumMessages(msgbase);
	msgbase.close();
	if (numOfMessages == 0)
	{
		console.clear("\x01n");
		console.center("\x01n\x01h\x01yThere are no messages to display.");
		console.crlf();
		console.pause();
		retObj.stoppedReading = true;
		return retObj;
	}

	// Check the pAllowChgArea parameter.  If it's a boolean, then use it.  If
	// not, then check to see if we're reading personal mail - If not, then allow
	// the user to change to a different message area.
	var allowChgMsgArea = true;
	if (typeof(pAllowChgArea) == "boolean")
		allowChgMsgArea = pAllowChgArea;
	else
		allowChgMsgArea = (this.subBoardCode != "mail");
	// If reading personal email and messages haven't been collected (searched)
	// yet, then do so now.
	if (this.readingPersonalEmail && (!this.msgSearchHdrs.hasOwnProperty(this.subBoardCode)))
		this.msgSearchHdrs[this.subBoardCode] = searchMsgbase(this.subBoardCode, this.searchType, this.searchString, this.readingPersonalEmailFromUser);

	// Determine the index of the message to start at.  This will be
	// pStartingMsgOffset if pStartingMsgOffset is valid, or the index
	// of the user's last-read message in this sub-board.
	var msgIndex = 0;
	if ((typeof(pStartingMsgOffset) == "number") && (pStartingMsgOffset >= 0) && (pStartingMsgOffset < this.NumMessages()))
		msgIndex = pStartingMsgOffset;
	else if (this.SearchingAndResultObjsDefinedForCurSub())
		msgIndex = 0;
	else
	{
		msgIndex = this.GetLastReadMsgIdxAndNum().lastReadMsgIdx;
		if (msgIndex == -1)
			msgIndex = 0;
	}

	// If the current message index is for a message that has been
	// deleted, then find the next non-deleted message.
	var testMsgHdr = this.GetMsgHdrByIdx(msgIndex);
	if ((testMsgHdr == null) || ((testMsgHdr.attr & MSG_DELETE) == MSG_DELETE))
	{
		// First try going forward
		var nonDeletedMsgIdx = this.FindNextNonDeletedMsgIdx(msgIndex, true);
		// If a non-deleted message was not found, then try going backward.
		if (nonDeletedMsgIdx == -1)
			nonDeletedMsgIdx = this.FindNextNonDeletedMsgIdx(msgIndex, false);
		// If a non-deleted message was found, then set msgIndex to it.
		// Otherwise, tell the user there are no messages in this sub-board
		// and return.
		if (nonDeletedMsgIdx > -1)
			msgIndex = nonDeletedMsgIdx;
		else
		{
			console.clear("\x01n");
			console.center("\x01h\x01yThere are no messages to display.");
			console.crlf();
			console.pause();
			retObj.stoppedReading = true;
			return retObj;
		}
	}

	// Construct the hotkey help line (needs to be done after the message
	// base is open so that the delete & edit keys can be added correctly).
	this.SetEnhancedReaderHelpLine();

	// Get the screen ready for reading messages - First, clear the screen.
	console.clear("\x01n");
	// Display the help line at the bottom of the screen
	if (this.scrollingReaderInterface && console.term_supports(USER_ANSI))
		this.DisplayEnhancedMsgReadHelpLine(console.screen_rows, allowChgMsgArea);
	// Input loop
	var msgHdr = null;
	var dateTimeStr = null;
	var screenY = 1; // For screen updates requiring more than one line
	var continueOn = true;
	var readMsgRetObj = null;
	// previousNextAction will store the next action from the previous iteration.
	// It is useful for some checks, such as when the current message is deleted,
	// we'll want to see if the user wanted to go to the previous message/area
	// for navigation purposes.
	var previousNextAction = ACTION_NONE;
	while (continueOn && (msgIndex >= 0) && (msgIndex < this.NumMessages()))
	{
		// Display the message with the enhanced read method
		readMsgRetObj = this.ReadMessageEnhanced(msgIndex, allowChgMsgArea);
		retObj.lastUserInput = readMsgRetObj.lastKeypress;
		retObj.lastAction = readMsgRetObj.nextAction;
		// If we should refresh the enhanced reader help line on the screen (and
		// the returned message offset is valid and the user's terminal supports ANSI),
		// then refresh the help line.
		if (readMsgRetObj.refreshEnhancedRdrHelpLine && readMsgRetObj.offsetValid && console.term_supports(USER_ANSI))
			this.DisplayEnhancedMsgReadHelpLine(console.screen_rows, allowChgMsgArea);
		// If the returned message offset is invalid, then quit.
		if (!readMsgRetObj.offsetValid)
		{
			continueOn = false;
			retObj.stoppedReading = true;
			break;
		}
		// If the message is not readable to the user, then go to the
		// next/previous message
		else if (readMsgRetObj.msgNotReadable)
		{
			// If the user's next action in the last iteration was to go to the
			// previous message, then go backwards; otherwise, go forward.
			if (previousNextAction == ACTION_GO_PREVIOUS_MSG)
				msgIndex = this.FindNextNonDeletedMsgIdx(msgIndex, false);
			else
				msgIndex = this.FindNextNonDeletedMsgIdx(msgIndex, true);
			continueOn = ((msgIndex >= 0) && (msgIndex < this.NumMessages()));
		}
		else if (readMsgRetObj.nextAction == ACTION_QUIT) // Quit
		{
			// Quit
			continueOn = false;
			retObj.stoppedReading = true;
			break;
		}
		else if (readMsgRetObj.lastKeypress == "R")
		{
			// Replying to the message is handled in ReadMessageEnhanced().
			// The help line at the bottom of the screen needs to be redrawn though,
			// for ANSI users.
			if (this.scrollingReaderInterface && console.term_supports(USER_ANSI))
				this.DisplayEnhancedMsgReadHelpLine(console.screen_rows, allowChgMsgArea);
		}
		else if (readMsgRetObj.nextAction == ACTION_GO_PREVIOUS_MSG) // Go to previous message/area
		{
			// TODO: There is some opportunity for screen redraw optimization - If
			// already at the first readable sub-board, this would redraw the
			// screen unnecessarily.  Similar for the right arrow key too.

			// The newMsgOffset value will be 0 or more if a prior non-deleted
			// message was found.  If it's -1, then allow going to the previous
			// message sub-board/group.
			if (readMsgRetObj.newMsgOffset > -1)
				msgIndex = readMsgRetObj.newMsgOffset;
			else
			{
				// The user is at the beginning of the current sub-board.
				if (allowChgMsgArea)
				{
					if (this.SearchTypePopulatesSearchResults())
						console.print("\x01n\r\nLoading messages...");
					var goToPrevRetval = this.GoToPrevSubBoardForEnhReader(allowChgMsgArea, pPromptToGoToNextAreaIfNoSearchResults);
					retObj.stoppedReading = goToPrevRetval.shouldStopReading;
					// If we're going to stop reading, then 
					if (retObj.stoppedReading)
						msgIndex = 0;
					else if (goToPrevRetval.changedMsgArea)
						msgIndex = goToPrevRetval.msgIndex;
				}

				// If the caller wants this method to return instead of going to the next
				// sub-board with messages, then do so.
				if (pReturnOnNextAreaNav)
					return retObj;
			}
		}
		// Go to next message action - This can happen with the right arrow key or
		// if the user deletes the message in the ReadMessageEnhanced() method.
		else if (readMsgRetObj.nextAction == ACTION_GO_NEXT_MSG)
		{
			// The newMsgOffset value will be 0 or more if a later non-deleted
			// message was found.  If it's -1, then allow going to the next
			// message sub-board/group.
			if (readMsgRetObj.newMsgOffset > -1)
				msgIndex = readMsgRetObj.newMsgOffset;
			else
			{
				// The user is at the end of the current sub-board.
				if (allowChgMsgArea && !pReturnOnNextAreaNav)
				{
					if (this.SearchTypePopulatesSearchResults())
						console.print("\x01n\r\nLoading messages...");
					var goToNextRetval = this.GoToNextSubBoardForEnhReader(allowChgMsgArea, pPromptToGoToNextAreaIfNoSearchResults);
					retObj.stoppedReading = goToNextRetval.shouldStopReading;
					// If we're going to stop reading, then 
					if (retObj.stoppedReading)
						msgIndex = 0;
					else if (goToNextRetval.changedMsgArea)
						msgIndex = goToNextRetval.msgIndex;
				}
				// If the caller wants this method to return instead of going to the next
				// sub-board with messages, then do so.
				if (pReturnOnNextAreaNav)
					return retObj;
			}
		}
		else if (readMsgRetObj.nextAction == ACTION_GO_FIRST_MSG) // Go to the first message
		{
			// Go to the first message that's not marked as deleted.  This passes -1 as the
			// starting message index because FindNextNonDeletedMsgIdx() will increment it
			// before searching in order to find the "next" message.
			msgIndex = this.FindNextNonDeletedMsgIdx(-1, true);
		}
		else if (readMsgRetObj.nextAction == ACTION_GO_LAST_MSG) // Go to the last message
		{
			// Go to the last message that's not marked as deleted
			msgIndex = this.FindNextNonDeletedMsgIdx(this.NumMessages(), false);
		}
		else if (readMsgRetObj.nextAction == ACTION_CHG_MSG_AREA) // Change message area, if allowed
		{
			if (allowChgMsgArea)
			{
				// Change message sub-board.  If a different sub-board was
				// chosen, then change some variables to use the new
				// chosen sub-board.
				var oldSubBoardCode = this.subBoardCode;
				this.SelectMsgArea();
				if (this.subBoardCode != oldSubBoardCode)
					this.PopulateHdrsForCurrentSubBoard();
				var chgSubBoardRetObj = this.EnhancedReaderChangeSubBoard(bbs.cursub_code);
				if (chgSubBoardRetObj.succeeded)
				{
					// Set the message index, etc.
					// If there are search results, then set msgIndex to the first
					// message.  Otherwise (if there is no search specified), then
					// set the message index to the user's last read message.
					if (this.SearchingAndResultObjsDefinedForCurSub())
						msgIndex = 0;
					else
						msgIndex = chgSubBoardRetObj.lastReadMsgIdx;
					// If the current message index is for a message that has been
					// deleted, then find the next non-deleted message.
					testMsgHdr = this.GetMsgHdrByIdx(msgIndex);
					if ((testMsgHdr == null) || ((testMsgHdr.attr & MSG_DELETE) == MSG_DELETE))
					{
						// First try going forward
						var nonDeletedMsgIdx = this.FindNextNonDeletedMsgIdx(msgIndex, true);
						// If a non-deleted message was not found, then try going backward.
						if (nonDeletedMsgIdx == -1)
							nonDeletedMsgIdx = this.FindNextNonDeletedMsgIdx(msgIndex, false);
						// If a non-deleted message was found, then set msgIndex to it.
						// Otherwise, return.
						// Note: If there are no messages in the chosen sub-board at all,
						// then the error would have already been shown.
						if (nonDeletedMsgIdx > -1)
							msgIndex = nonDeletedMsgIdx;
						else
						{
							if (this.NumMessages() != 0)
							{
								// There are messages, but none that are not deleted.
								console.clear("\x01n");
								console.center("\x01h\x01yThere are no messages to display.");
								console.crlf();
								console.pause();
							}
							retObj.stoppedReading = true;
							return retObj;
						}
					}
					// Set the hotkey help line again, since the new sub-board might have
					// different settings for whether messages can be edited or deleted,
					// then refresh it on the screen.
					var oldHotkeyHelpLine = this.enhReadHelpLine;
					this.SetEnhancedReaderHelpLine();
					if ((oldHotkeyHelpLine != this.enhReadHelpLine) && this.scrollingReaderInterface && console.term_supports(USER_ANSI))
						this.DisplayEnhancedMsgReadHelpLine(console.screen_rows, allowChgMsgArea);
				}
				else
				{
					retObj.stoppedReading = false;
					return retObj;
				}
			}
		}
		else if (readMsgRetObj.nextAction == ACTION_GO_PREV_MSG_AREA) // Go to the previous message area
		{
			// The user is at the beginning of the current sub-board.
			if (allowChgMsgArea)
			{
				var goToPrevRetval = this.GoToPrevSubBoardForEnhReader(allowChgMsgArea, pPromptToGoToNextAreaIfNoSearchResults);
				retObj.stoppedReading = goToPrevRetval.shouldStopReading;
				if (retObj.stoppedReading)
					msgIndex = 0;
				else if (goToPrevRetval.changedMsgArea)
					msgIndex = goToPrevRetval.msgIndex;
			}
			// If the caller wants this method to return instead of going to the next
			// sub-board with messages, then do so.
			if (pReturnOnNextAreaNav)
				return retObj;
		}
		else if (readMsgRetObj.nextAction == ACTION_GO_NEXT_MSG_AREA) // Go to the next message area
		{
			if (allowChgMsgArea && !pReturnOnNextAreaNav)
			{
				var goToNextRetval = this.GoToNextSubBoardForEnhReader(allowChgMsgArea, pPromptToGoToNextAreaIfNoSearchResults);
				retObj.stoppedReading = goToNextRetval.shouldStopReading;
				if (retObj.stoppedReading)
					msgIndex = 0;
				else if (goToNextRetval.changedMsgArea)
					msgIndex = goToNextRetval.msgIndex;
			}
			// If the caller wants this method to return instead of going to the next
			// sub-board with messages, then do so.
			if (pReturnOnNextAreaNav)
				return retObj;
		}
		else if (readMsgRetObj.nextAction == ACTION_DISPLAY_MSG_LIST) // Display message list
		{
			// If we need to return to the caller for this, then do so.
			if (pReturnOnMessageList)
			{
				retObj.messageListReturn = true;
				return retObj;
			}
			else
			{
				// If this.reverseListOrder is the string "ASK", the user will be prompted
				// on the last line of the screen for whether they want to list the
				// messages in reverse order.  So, erase the help line on the bottom of
				// the screen.
				if ((typeof(this.reverseListOrder) == "string") && (this.reverseListOrder.toUpperCase() == "ASK"))
				{
					if (this.scrollingReaderInterface && console.term_supports(USER_ANSI))
					{
						console.gotoxy(1, console.screen_rows);
						console.cleartoeol("\x01n");
					}
				}

				// List messages
				var listRetObj = this.ListMessages(null, pAllowChgArea);
				// If the user wants to quit, then stop the input loop.
				if (listRetObj.lastUserInput == "Q")
				{
					continueOn = false;
					retObj.stoppedReading = true;
				}
				// If the user chose a different message, then set the message index
				else if ((listRetObj.selectedMsgOffset > -1) && (listRetObj.selectedMsgOffset < this.NumMessages()))
					msgIndex = listRetObj.selectedMsgOffset;
			}
		}
		// Go to specific message & new message offset is valid: Read the new
		// message
		else if ((readMsgRetObj.nextAction == ACTION_GO_SPECIFIC_MSG) && (readMsgRetObj.newMsgOffset > -1))
			msgIndex = readMsgRetObj.newMsgOffset;

		// Save this iteration's next action for the "previous" next action for the next iteration
		previousNextAction = readMsgRetObj.nextAction;
	}

	return retObj;
}

// For the DigDistMsgReader class: Performs the message listing, given a
// sub-board code.
//
// Paramters:
//  pSubBoardCode: Optional - The internal sub-board code, or "mail"
//                 for personal email.
//  pAllowChgSubBoard: Optional - A boolean to specify whether or not to allow
//                     changing to another sub-board.  Defaults to true.
// Return value: An object containing the following properties:
//               lastUserInput: The user's last keypress/input
//               selectedMsgOffset: The index of the message selected to read,
//                                  if one was selected.  If none was selected,
//                                  this will be -1.
function DigDistMsgReader_ListMessages(pSubBoardCode, pAllowChgSubBoard)
{
	var retObj = {
		lastUserInput: "",
		selectedMsgOffset: -1
	};

	// If the passed-in sub-board code was different than what was set in the object before,
	// then open the new message sub-board.
	var previousSubBoardCode = this.subBoardCode;
	if (typeof(pSubBoardCode) == "string")
	{
		if (subBoardCodeIsValid(pSubBoardCode))
			this.setSubBoardCode(pSubBoardCode);
		else
		{
			console.print("\x01n\x01h\x01yWarning: \x01wThe Message Reader connot continue because an invalid");
			console.crlf();
			console.print("sub-board code was specified (" + pSubBoardCode + "). Please notify the sysop.");
			console.crlf();
			console.pause();
			return retObj;
		}
	}
	if (this.subBoardCode.length == 0)
	{
		console.print("\x01n\x01h\x01yWarning: \x01wThe Message Reader connot continue because no message\r\n");
		console.print("sub-board was specified. Please notify the sysop.\r\n\x01p");
		return retObj;
	}

	// If the user doesn't have permission to read the current sub-board, then
	// don't allow the user to read it.
	if (this.subBoardCode != "mail")
	{
		if (!msg_area.sub[this.subBoardCode].can_read)
		{
			var errorMsg = format(bbs.text(CantReadSub), msg_area.sub[this.subBoardCode].grp_name, msg_area.sub[this.subBoardCode].name);
			console.print("\x01n" + errorMsg);
			console.pause();
			return retObj;
		}
	}

	// If there are no messages to display in the current sub-board, then let the
	// user know and exit.
	if (this.NumMessages() == 0)
	{
		console.clear("\x01n");
		console.center("\x01n\x01h\x01yThere are no messages to display.\r\n\x01p");
		return retObj;
	}

	// Construct the traditional UI pause text and the line of help text for lightbar
	// mode.  This adds the delete and edit keys if the user is allowed to delete & edit
	// messages.
	this.SetMsgListPauseTextAndLightbarHelpLine();

	// If this.reverseListOrder is the string "ASK", prompt the user for whether
	// they want to list the messages in reverse order.
	if ((typeof(this.reverseListOrder) == "string") && (this.reverseListOrder.toUpperCase() == "ASK"))
	{
		if (numMessages(bbs.cursub_code) > 0)
			this.reverseListOrder = !console.noyes("\x01n\x01cList in reverse (newest on top)");
	}

	// List the messages using the lightbar or traditional interface, depending on
	// what this.msgListUseLightbarListInterface is set to.  The lightbar interface requires ANSI.
	if (this.msgListUseLightbarListInterface && canDoHighASCIIAndANSI())
		retObj = this.ListMessages_Lightbar(pAllowChgSubBoard);
	else
		retObj = this.ListMessages_Traditional(pAllowChgSubBoard);
	return retObj;
}
// For the DigDistMsgReader class: Performs the message listing, given a
// sub-board code.  This version uses a traditional user interface, prompting
// the user at the end of each page to continue, quit, or read a message.
//
// Parameters:
//  pAllowChgSubBoard: Optional - A boolean to specify whether or not to allow
//                     changing to another sub-board.  Defaults to true.
//
// Return value: An object containing the following properties:
//               lastUserInput: The user's last keypress/input
//               selectedMsgOffset: The index of the message selected to read,
//                                  if one was selected.  If none was selected,
//                                  this will be -1.
function DigDistMsgReader_ListMessages_Traditional(pAllowChgSubBoard)
{
	var retObj = {
		lastUserInput: "",
		selectedMsgOffset: -1
	};

	// If the user doesn't have permission to read the current sub-board, then
	// don't allow the user to read it.
	if (this.subBoardCode != "mail")
	{
		if (!msg_area.sub[this.subBoardCode].can_read)
		{
			var errorMsg = format(bbs.text(CantReadSub), msg_area.sub[this.subBoardCode].grp_name, msg_area.sub[this.subBoardCode].name);
			console.print("\x01n" + errorMsg);
			console.pause();
			return retObj;
		}
	}

	// Reset this.readAMessage and deniedReadingmessage to false, in case the
	// message listing has previously ended with them set to true.
	this.readAMessage = false;
	this.deniedReadingMessage = false;

	var msgbase = new MsgBase(this.subBoardCode);
	if (!msgbase.open())
	{
		console.center("\x01n\x01h\x01yError: \x01wUnable to open the sub-board.\r\n\x01p");
		return retObj;
	}

	var allowChgSubBoard = (typeof(pAllowChgSubBoard) == "boolean" ? pAllowChgSubBoard : true);

	// this.tradMsgListNumLines stores the maximum number of lines to write.  It's the number
	// of rows on the user's screen - 3 to make room for the header line
	// at the top, the question line at the bottom, and 1 extra line at
	// the bottom of the screen so that displaying carriage returns
	// doesn't mess up the position of the header lines at the top.
	this.tradMsgListNumLines = console.screen_rows-3;
	var nListStartLine = 2; // The first line number on the screen for the message list
	// If we will be displaying the message group and sub-board in the
	// header at the top of the screen (an additional 2 lines), then
	// update this.tradMsgListNumLines and nListStartLine to account for this.
	if (this.displayBoardInfoInHeader)
	{
		this.tradMsgListNumLines -= 2;
		nListStartLine += 2;
	}

	// If the user's terminal doesn't support ANSI, then re-calculate
	// this.tradMsgListNumLines - we won't be keeping the headers at the top of the
	// screen.
	if (!canDoHighASCIIAndANSI()) // Could also be !console.term_supports(USER_ANSI)
		this.tradMsgListNumLines = console.screen_rows - 2;

	this.RecalcMsgListWidthsAndFormatStrs();

	// Clear the screen and write the header at the top
	console.clear("\x01n");
	this.WriteMsgListScreenTopHeader();

	// If this.tradListTopMsgIdx hasn't been set yet, then get the index of the user's
	// last read message and figure out which page it's on and set the top message
	// index accordingly.
	if (this.tradListTopMsgIdx == -1)
		this.SetUpTraditionalMsgListVars();
	// Write the message list
	var continueOn = true;
	var retvalObj = null;
	var curpos = null; // Current character position
	var lastScreen = false;
	while (continueOn)
	{
		// Go to the top and write the current page of message information,
		// then update curpos.
		console.gotoxy(1, nListStartLine);
		lastScreen = this.ListScreenfulOfMessages(this.tradListTopMsgIdx, this.tradMsgListNumLines);
		curpos = console.getxy();
		clearToEOS(curpos.y);
		console.gotoxy(curpos);
		// Prompt the user whether or not to continue or to read a message
		// (by message number).
		if (this.reverseListOrder)
			retvalObj = this.PromptContinueOrReadMsg((this.tradListTopMsgIdx == this.NumMessages()-1), lastScreen, allowChgSubBoard);
		else
			retvalObj = this.PromptContinueOrReadMsg((this.tradListTopMsgIdx == 0), lastScreen, allowChgSubBoard);
		retObj.lastUserInput = retvalObj.userInput;
		retObj.selectedMsgOffset = retvalObj.selectedMsgOffset;

		continueOn = retvalObj.continueOn;
		// TODO: Update this to use PageUp & PageDown keys for paging?  It would
		// require updating PromptContinueOrReadMsg(), which would be non-trivial
		// because that method uses console.getkeys() with a list of allowed keys
		// and a message number limit.
		if (continueOn)
		{
			// If the user chose to go to the previous page of listings,
			// then subtract the appropriate number of messages from
			// this.tradListTopMsgIdx in order to do so.
			if (retvalObj.userInput == "P")
			{
				if (this.reverseListOrder)
				{
					this.tradListTopMsgIdx += this.tradMsgListNumLines;
					// If we go past the beginning, then we need to reset
					// msgNum so we'll be at the beginning of the list.
					var totalNumMessages = this.NumMessages();
					if (this.tradListTopMsgIdx >= totalNumMessages)
						this.tradListTopMsgIdx = totalNumMessages - 1;
				}
				else
				{
					this.tradListTopMsgIdx -= this.tradMsgListNumLines;
					// If we go past the beginning, then we need to reset
					// msgNum so we'll be at the beginning of the list.
					if (this.tradListTopMsgIdx < 0)
						this.tradListTopMsgIdx = 0;
				}
			}
			// If the user chose to go to the next page, update
			// this.tradListTopMsgIdx appropriately.
			else if (retvalObj.userInput == "N")
			{
				if (this.reverseListOrder)
					this.tradListTopMsgIdx -= this.tradMsgListNumLines;
				else
					this.tradListTopMsgIdx += this.tradMsgListNumLines;
			}
			// First page
			else if (retvalObj.userInput == "F")
			{
				if (this.reverseListOrder)
					this.tradListTopMsgIdx = this.NumMessages() - 1;
				else
					this.tradListTopMsgIdx = 0;
			}
			// Last page
			else if (retvalObj.userInput == "L")
			{
				if (this.reverseListOrder)
				{
					this.tradListTopMsgIdx = (this.NumMessages() % this.tradMsgListNumLines) - 1;
					// If this.tradListTopMsgIdx is now invalid (below 0), then adjust it
					// to properly display the last page of messages.
					if (this.tradListTopMsgIdx < 0)
						this.tradListTopMsgIdx = this.tradMsgListNumLines - 1;
				}
				else
				{
					var totalNumMessages = this.NumMessages();
					this.tradListTopMsgIdx = totalNumMessages - (totalNumMessages % this.tradMsgListNumLines);
					if (this.tradListTopMsgIdx >= totalNumMessages)
						this.tradListTopMsgIdx = totalNumMessages - this.tradMsgListNumLines;
				}
			}
			// D: Delete a message
			else if (retvalObj.userInput == "D" || retvalObj.userInput == this.msgListKeys.deleteMessage) // KEY_DEL
			{
				if (this.CanDelete() || this.CanDeleteLastMsg())
				{
					var msgNum = this.PromptForMsgNum({ x: curpos.x, y: curpos.y+1 }, replaceAtCodesInStr(this.text.deleteMsgNumPromptText), false, ERROR_PAUSE_WAIT_MS, false);
					// If the user enters a valid message number, then call the
					// DeleteMessage() method, which will prompt the user for
					// confirmation and delete the message if confirmed.
					if (msgNum > 0)
						this.PromptAndDeleteOrUndeleteMessage(msgNum-1, null, true);

					// Refresh the top header on the screen for continuing to list
					// messages.
					console.clear("\x01n");
					this.WriteMsgListScreenTopHeader();
				}
			}
			// E: Edit a message
			else if (retvalObj.userInput == this.msgListKeys.editMsg) // "E"
			{
				if (this.CanEdit())
				{
					var msgNum = this.PromptForMsgNum({ x: curpos.x, y: curpos.y+1 }, replaceAtCodesInStr(this.text.editMsgNumPromptText), false, ERROR_PAUSE_WAIT_MS, false);
					// If the user entered a valid message number, then let the
					// user edit the message.
					if (msgNum > 0)
					{
						// See if the current message header has our "isBogus" property and it's true.
						// Only let the user edit the message if it's not a bogus message header.
						// The message header could have the "isBogus" property, for instance, if
						// it's a vote message (introduced in Synchronet 3.17).
						var tmpMsgHdr = this.GetMsgHdrByIdx(msgNum-1);
						var hdrIsBogus = (tmpMsgHdr.hasOwnProperty("isBogus") ? tmpMsgHdr.isBogus : false);
						if (!hdrIsBogus)
							var returnObj = this.EditExistingMsg(msgNum-1);
						else
						{
							console.print("\x01n\r\n\x01h\x01yThat message isn't editable.\n");
							console.crlf();
							console.pause();
						}
					}

					// Refresh the top header on the screen for continuing to list
					// messages.
					console.clear("\x01n");
					this.WriteMsgListScreenTopHeader();
				}
			}
			// G: Go to a specific message by # (place that message on the top)
			else if (retvalObj.userInput == "G")
			{
				var msgNum = this.PromptForMsgNum(curpos, "\x01n" + replaceAtCodesInStr(this.text.goToMsgNumPromptText), false, ERROR_PAUSE_WAIT_MS, false);
				if (msgNum > 0)
					this.tradListTopMsgIdx = msgNum - 1;

				// Refresh the top header on the screen for continuing to list
				// messages.
				console.clear("\x01n");
				this.WriteMsgListScreenTopHeader();
			}
			// ?: Display help
			else if (retvalObj.userInput == "?")
			{
				console.clear("\x01n");
				this.DisplayMsgListHelp(allowChgSubBoard, true);
				console.clear("\x01n");
				this.WriteMsgListScreenTopHeader();
			}
			// C: Change to another message area (sub-board)
			else if (retvalObj.userInput == this.msgListKeys.chgMsgArea) // "C"
			{
				if (allowChgSubBoard && (this.subBoardCode != "mail"))
				{
					// Store the current sub-board code so we can see if it changed
					var oldSubCode = bbs.cursub_code;
					// Let the user choose another message area.  If they chose
					// a different message area, then set up the message base
					// object accordingly.
					this.SelectMsgArea();
					if (bbs.cursub_code != oldSubCode)
					{
						var chgSubRetval = this.ChangeSubBoard(bbs.cursub_code);
						continueOn = chgSubRetval.succeeded;
					}
					// Update the traditional list variables and refresh the screen
					if (continueOn)
					{
						this.SetUpTraditionalMsgListVars();
						console.clear("\x01n");
						this.WriteMsgListScreenTopHeader();
					}
				}
			}
			// S: Select message(s)
			else if (retvalObj.userInput == "S")
			{
				// Input the message number list from the user
				console.print("\x01n\x01cNumber(s) of message(s) to select, (\x01hA\x01n\x01c=All, \x01hN\x01n\x01c=None, \x01hENTER\x01n\x01c=cancel)\x01g\x01h: \x01c");
				var userNumberList = console.getstr(128, K_UPPER);
				// If the user entered A or N, then select/un-select all messages.
				// Otherwise, select only the messages that the user entered.
				if ((userNumberList == "A") || (userNumberList == "N"))
				{
					var messageSelectToggle = (userNumberList == "A");
					var totalNumMessages = this.NumMessages();
					for (var msgIdx = 0; msgIdx < totalNumMessages; ++msgIdx)
						this.ToggleSelectedMessage(this.subBoardCode, msgIdx, messageSelectToggle);
				}
				else
				{
					if (userNumberList.length > 0)
					{
						var numArray = parseNumberList(userNumberList);
						for (var numIdx = 0; numIdx < numArray.length; ++numIdx)
							this.ToggleSelectedMessage(this.subBoardCode, numArray[numIdx]-1);
					}
				}
				// Refresh the top header on the screen for continuing to list
				// messages.
				console.clear("\x01n");
				this.WriteMsgListScreenTopHeader();
			}
			// Ctrl-D: Batch delete (for selected messages)
			else if (retvalObj.userInput == this.msgListKeys.batchDelete) // CTRL_D
			{
				console.print("\x01n");
				console.crlf();
				if (this.NumSelectedMessages() > 0)
				{
					// The PromptAndDeleteOrUndeleteSelectedMessages() method will prompt the user for confirmation
					// to delete the message and then delete it if confirmed.
					this.PromptAndDeleteOrUndeleteSelectedMessages(null, true);

					// In case all messages were deleted, if the user can't view deleted messages,
					// show an appropriate message and don't continue listing messages.
					//if (this.NumMessages(true) == 0)
					if (!this.NonDeletedMessagesExist() && !canViewDeletedMsgs())
					{
						continueOn = false;
						// Note: The following doesn't seem to be necessary, since
						// the ReadOrListSubBoard() method will show a message saying
						// there are no messages to read and then will quit out.
						
						//msgbase.close();
						//msgbase = null;
						//console.clear("\x01n");
						//console.center("\x01n\x01h\x01yThere are no messages to display.");
						//console.crlf();
						//console.pause();
						
					}
					else
					{
						// There are still messages to list, so refresh the top
						// header on the screen for continuing to list messages.
						console.clear("\x01n");
						this.WriteMsgListScreenTopHeader();
					}
				}
				else
				{
					// There are no selected messages
					console.print("\x01n\x01h\x01yThere are no selected messages.");
					mswait(ERROR_PAUSE_WAIT_MS);
					// Refresh the top header on the screen for continuing to list messages.
					console.clear("\x01n");
					this.WriteMsgListScreenTopHeader();
				}
			}
			// User settings
			else if (retvalObj.userInput == this.msgListKeys.userSettings)
			{
				/*
				var continueOn = true;
				var retvalObj = null;
				var curpos = null; // Current character position
				var lastScreen = false;

				this.RecalcMsgListWidthsAndFormatStrs();
				if (this.tradListTopMsgIdx == -1)
					this.SetUpTraditionalMsgListVars();
				this.WriteMsgListScreenTopHeader();
				*/
				var userSettingsRetObj = this.DoUserSettings_Traditional();
				retvalObj.userInput = "";
				//drawMenu = userSettingsRetObj.needWholeScreenRefresh;
				// In case the user changed their twitlist, re-filter the messages for this sub-board
				if (userSettingsRetObj.userTwitListChanged)
				{
					console.gotoxy(1, console.screen_rows);
					console.crlf();
					console.print("\x01nTwitlist changed; re-filtering..");
					var tmpMsgbase = new MsgBase(this.subBoardCode);
					if (tmpMsgbase.open())
					{
						var tmpAllMsgHdrs = tmpMsgbase.get_all_msg_headers(true);
						tmpMsgbase.close();
						this.FilterMsgHdrsIntoHdrsForCurrentSubBoard(tmpAllMsgHdrs, true);
					}
					else
						console.print("\x01y\x01hFailed to open the messagbase!\x01\r\n\x01p");
					this.RecalcMsgListWidthsAndFormatStrs();
					if (this.tradListTopMsgIdx == -1)
						this.SetUpTraditionalMsgListVars();
					// If there are still messages in this sub-board, and the message offset is beyond the last
					// message, then adjust the top message index as necessary.
					if (this.hdrsForCurrentSubBoard.length > 0)
					{
						if (this.tradListTopMsgIdx > this.hdrsForCurrentSubBoard.length)
							this.tradListTopMsgIdx = this.hdrsForCurrentSubBoard.length - this.tradMsgListNumLines;
					}
					else
					{
						continueOn = false;
						retObj.selectedMsgOffset = -1;
					}
				}
				if (userSettingsRetObj.needWholeScreenRefresh)
					this.WriteMsgListScreenTopHeader();
			}
			else
			{
				// If a message has been selected, exit out of this input loop
				// so we can return from this method - The calling method will
				// call the enhanced reader method.
				if (retObj.selectedMsgOffset >= 0)
					continueOn = false;
			}
		}
	}

	msgbase.close();

	return retObj;
}
// For the DigDistMsgReader class: Performs the message listing, given a
// sub-board code.  This verison uses a lightbar interface for message
// navigation.
//
// Parameters:
//  pAllowChgSubBoard: Optional - A boolean to specify whether or not to allow
//                     changing to another sub-board.  Defaults to true.
//
// Return value: An object containing the following properties:
//               lastUserInput: The user's last keypress/input
//               selectedMsgOffset: The index of the message selected to read,
//                                  if one was selected.  If none was selected,
//                                  this will be -1.
function DigDistMsgReader_ListMessages_Lightbar(pAllowChgSubBoard)
{
	var retObj = {
		lastUserInput: "",
		selectedMsgOffset: -1
	};

	// If the user doesn't have permission to read the current sub-board, then
	// don't allow the user to read it.
	if (this.subBoardCode != "mail")
	{
		if (!msg_area.sub[this.subBoardCode].can_read)
		{
			var errorMsg = format(bbs.text(CantReadSub), msg_area.sub[this.subBoardCode].grp_name, msg_area.sub[this.subBoardCode].name);
			console.print("\x01n" + errorMsg);
			console.pause();
			return retObj;
		}
	}

	// This method is only supported if the user's terminal supports
	// ANSI.
	if (!canDoHighASCIIAndANSI()) // Could also be !console.term_supports(USER_ANSI)
	{
		console.print("\r\n\x01h\x01ySorry, an ANSI terminal is required for this operation.\x01n\x01w\r\n");
		console.pause();
		return retObj;
	}

	// Reset this.readAMessage and deniedReadingMessage to false, in case the
	// message listing has previously ended with them set to true.
	this.readAMessage = false;
	this.deniedReadingMessage = false;

	this.RecalcMsgListWidthsAndFormatStrs();

	var allowChgSubBoard = (typeof(pAllowChgSubBoard) == "boolean" ? pAllowChgSubBoard : true);

	// This function will be used for displaying the help line at
	// the bottom of the screen.
	function DisplayHelpLine(pHelpLineText)
	{
		console.gotoxy(1, console.screen_rows);
		// Mouse: console.print replaced with console.putmsg for mouse click hotspots
		//console.print(pHelpLineText);
		console.putmsg(pHelpLineText); // console.putmsg() can process @-codes, which we use for mouse click tracking
		console.cleartoeol("\x01n");
	}

	// Clear the screen and write the header at the top
	console.clear("\x01n");
	this.WriteMsgListScreenTopHeader();
	DisplayHelpLine(this.msgListLightbarModeHelpLine);

	// If the lightbar message list index & cursor position variables haven't been
	// set yet, then set them.
	if ((this.lightbarListTopMsgIdx == -1) || (this.lightbarListSelectedMsgIdx == -1) ||
	    (this.lightbarListCurPos == null))
	{
		this.SetUpLightbarMsgListVars();
	}

	// Create a DDLightbarMenu for the message list and list messages
	// and let the user choose one
	var msgListMenu = this.CreateLightbarMsgListMenu();
	var msgHeader = null;
	var drawMenu = true;
	var continueOn = true;
	while (continueOn)
	{
		var userChoice = msgListMenu.GetVal(drawMenu);
		drawMenu = true;
		var lastUserInputUpper = (typeof(msgListMenu.lastUserInput) == "string" ? msgListMenu.lastUserInput.toUpperCase() : msgListMenu.lastUserInput);
		// If the user's last input is null, then something bad/weird must have
		// happened, so don't continue the input loop.
		if (lastUserInputUpper == null)
		{
			continueOn = false;
			break;
		}
		this.lightbarListSelectedMsgIdx = msgListMenu.selectedItemIdx;
		// If userChoice is a number, then it will be a message number for a message to read
		if (typeof(userChoice) == "number")
		{
			// The user choice a message to read
			this.lightbarListSelectedMsgIdx = msgListMenu.selectedItemIdx;
			msgHeader = this.GetMsgHdrByIdx(this.lightbarListSelectedMsgIdx, this.showScoresInMsgList);
			this.PrintMessageInfo(msgHeader, true, this.lightbarListSelectedMsgIdx+1);
			console.gotoxy(this.lightbarListCurPos); // Make sure the cursor is still in the right place
			var hdrIsBogus = (msgHeader.hasOwnProperty("isBogus") ? msgHeader.isBogus : false);
			if (!hdrIsBogus)
			{
				// Allow the user to read the current message.
				var readMsg = true;
				if (this.promptToReadMessage)
				{
					// Confirm with the user whether to read the message.
					var sReadMsgConfirmText = this.colors.readMsgConfirmColor
											+ "Read message "
											+ this.colors.readMsgConfirmNumberColor
											+ +(this.GetMsgIdx(msgHeader.number) + 1)
											+ this.colors.readMsgConfirmColor
											+ ": Are you sure";
					console.gotoxy(1, console.screen_rows);
					console.print("\x01n");
					console.clearline();
					readMsg = console.yesno(sReadMsgConfirmText);
				}
				if (readMsg)
				{
					// If there is a search specified and the search result objects are
					// set up for the current sub-board, then the selected message offset
					// should be the search result array index.  Otherwise (if not
					// searching), the message offset should be the actual message offset
					// in the message base.
					if (this.SearchingAndResultObjsDefinedForCurSub())
						retObj.selectedMsgOffset = this.lightbarListSelectedMsgIdx;
					else
					{
						//retObj.selectedMsgOffset = msgHeader.offset;
						retObj.selectedMsgOffset = this.GetMsgIdx(msgHeader.number);
						if (retObj.selectedMsgOffset < 0)
							retObj.selectedMsgOffset = 0;
					}
					// Return from here so that the calling function can switch into
					// reader mode.
					continueOn = false;
					return retObj;
				}
				else
					this.deniedReadingMessage = true;

				// Ask the user if  they want to continue reading messages
				if (this.promptToContinueListingMessages)
					continueOn = console.yesno(this.colors["afterReadMsg_ListMorePromptColor"] + "Continue listing messages");
				// If the user chose to continue reading messages, then refresh
				// the screen.  Even if the user chooses not to read the message,
				// the screen needs to be re-drawn so it appears properly.
				if (continueOn)
				{
					this.WriteMsgListScreenTopHeader();
					DisplayHelpLine(this.msgListLightbarModeHelpLine);
				}
			}
		}
		// If userChoice is not a number, then it should be null in this case,
		// and the user would have pressed one of the additional quit keys set
		// up for the menu.  So look at the menu's lastUserInput and do the
		// appropriate thing.
		else if ((lastUserInputUpper == this.msgListKeys.quit) || (lastUserInputUpper == KEY_ESC)) // Quit
		{
			continueOn = false;
			retObj.lastUserInput = "Q"; // So the reader will quit out
		}
		// Numeric digit: The start of a number of a message to read
		else if (lastUserInputUpper.match(/[0-9]/))
		{
			// Put the user's input back in the input buffer to
			// be used for getting the rest of the message number.
			console.ungetstr(lastUserInputUpper);
			// Move the cursor to the bottom of the screen and
			// prompt the user for the message number.
			console.gotoxy(1, console.screen_rows);
			var userInput = this.PromptForMsgNum({ x: 1, y: console.screen_rows }, replaceAtCodesInStr(this.text.readMsgNumPromptText), true, ERROR_PAUSE_WAIT_MS, false);
			if (userInput > 0)
			{
				// See if the current message header has our "isBogus" property and it's true.
				// Only let the user read the message if it's not a bogus message header.
				// The message header could have the "isBogus" property, for instance, if
				// it's a vote message (introduced in Synchronet 3.17).
				//GetMsgHdrByIdx(pMsgIdx, pExpandFields)
				var tmpMsgHdr = this.GetMsgHdrByIdx(+(userInput-1), false);
				var hdrIsBogus = (tmpMsgHdr.hasOwnProperty("isBogus") ? tmpMsgHdr.isBogus : false);
				if (!hdrIsBogus)
				{
					// Confirm with the user whether to read the message
					var readMsg = true;
					if (this.promptToReadMessage)
					{
						var sReadMsgConfirmText = this.colors.readMsgConfirmColor
												+ "Read message "
												+ this.colors.readMsgConfirmNumberColor
												+ userInput + this.colors.readMsgConfirmColor
												+ ": Are you sure";
												readMsg = console.yesno(sReadMsgConfirmText);
					}
					if (readMsg)
					{
						// Update the message list screen variables
						this.CalcMsgListScreenIdxVarsFromMsgNum(+userInput);
						retObj.selectedMsgOffset = userInput - 1;
						// Return from here so that the calling function can switch
						// into reader mode.
						return retObj;
					}
					else
						this.deniedReadingMessage = true;

					// Prompt the user whether or not to continue listing
					// messages.
					if (this.promptToContinueListingMessages)
						continueOn = console.yesno(this.colors.afterReadMsg_ListMorePromptColor + "Continue listing messages");
				}
				else
					writeWithPause(1, console.screen_rows, "\x01n\x01h\x01yThat's not a readable message.", ERROR_PAUSE_WAIT_MS, "\x01n", true);
			}

			// If the user chose to continue listing messages, then re-draw
			// the screen.
			if (continueOn)
			{
				this.WriteMsgListScreenTopHeader();
				DisplayHelpLine(this.msgListLightbarModeHelpLine);
			}
		}
		// DEL key: Delete a message
		else if (lastUserInputUpper == this.msgListKeys.deleteMessage)
		{
			if (this.CanDelete() || this.CanDeleteLastMsg())
			{
				console.gotoxy(1, console.screen_rows);
				console.print("\x01n");
				console.clearline();

				// The PromptAndDeleteOrUndeleteMessage() method will prompt the user for confirmation
				// to delete the message and then delete it if confirmed.
				this.PromptAndDeleteOrUndeleteMessage(this.lightbarListSelectedMsgIdx, { x: 1, y: console.screen_rows}, true);

				// In case all messages were deleted, if the user can't view deleted messages,
				// show an appropriate message and don't continue listing messages.
				//if (this.NumMessages(true) == 0)
				if (!this.NonDeletedMessagesExist() && !canViewDeletedMsgs())
					continueOn = false;
				else
				{
					// There are still some messages to show, so refresh the screen.
					// Refresh the header & help line.
					this.WriteMsgListScreenTopHeader();
					DisplayHelpLine(this.msgListLightbarModeHelpLine);
				}
			}
		}
		// E: Edit a message
		else if (lastUserInputUpper == this.msgListKeys.editMsg)
		{
			if (this.CanEdit())
			{
				// See if the current message header has our "isBogus" property and it's true.
				// Only let the user edit the message if it's not a bogus message header.
				// The message header could have the "isBogus" property, for instance, if
				// it's a vote message (introduced in Synchronet 3.17).
				var tmpMsgHdr = this.GetMsgHdrByIdx(this.lightbarListSelectedMsgIdx, false);
				var hdrIsBogus = (tmpMsgHdr.hasOwnProperty("isBogus") ? tmpMsgHdr.isBogus : false);
				if (!hdrIsBogus)
				{
					// Ask the user if they really want to edit the message
					console.gotoxy(1, console.screen_rows);
					console.print("\x01n");
					console.clearline();
					// Let the user edit the message
					//var returnObj = this.EditExistingMsg(tmpMsgHdr.offset);
					var returnObj = this.EditExistingMsg(this.lightbarListSelectedMsgIdx);
					// Refresh the header & help line
					this.WriteMsgListScreenTopHeader();
					DisplayHelpLine(this.msgListLightbarModeHelpLine);
				}
			}
			else
				drawMenu = false; // No need to re-draw the menu
		}
		// G: Go to a specific message by # (highlight or place that message on the top)
		else if (lastUserInputUpper == this.msgListKeys.goToMsg)
		{
			// Move the cursor to the bottom of the screen and
			// prompt the user for a message number.
			console.gotoxy(1, console.screen_rows);
			var userMsgNum = this.PromptForMsgNum({ x: 1, y: console.screen_rows }, "\n" + replaceAtCodesInStr(this.text.goToMsgNumPromptText), true, ERROR_PAUSE_WAIT_MS, false);
			if (userMsgNum > 0)
			{
				// Make sure the message number is for a valid message (i.e., it
				// could be an invalid message number if there is a search, where
				// not all message numbers are consecutive).
				if (this.GetMsgHdrByMsgNum(userMsgNum) != null)
				{
					// If the message is on the current page, then just go to and
					// highlight it.  Otherwise, set the user's selected message on the
					// top of the page.  We also have to make sure that this.lightbarListCurPos.y and
					// originalCurpos.y are set correctly.  Also, account for search
					// results if there are any (we'll need to have the correct array
					// index for the search results).
					var chosenMsgIndex = userMsgNum - 1;
					msgListMenu.selectedItemIdx = chosenMsgIndex;
					if ((chosenMsgIndex < msgListMenu.NumItems()) && (chosenMsgIndex >= this.lightbarListTopMsgIdx))
					{
						this.lightbarListSelectedMsgIdx = chosenMsgIndex;
						msgListMenu.selectedItemIdx = this.lightbarListSelectedMsgIdx;
					}
					else
					{
						this.lightbarListTopMsgIdx = this.lightbarListSelectedMsgIdx = chosenMsgIndex;
						msgListMenu.topItemIdx = this.lightbarListTopMsgIdx;
					}
				}
				else
				{
					// The user entered an invalid message number
					console.print("\x01n" + replaceAtCodesInStr(format(this.text.invalidMsgNumText, userMsgNum)) + "\x01n");
					console.inkey(K_NONE, ERROR_PAUSE_WAIT_MS);
				}
			}

			// Refresh the header & help lines
			this.WriteMsgListScreenTopHeader();
			DisplayHelpLine(this.msgListLightbarModeHelpLine);
		}
		// C: Change to another message area (sub-board)
		else if (lastUserInputUpper == this.msgListKeys.chgMsgArea)
		{
			if (allowChgSubBoard && (this.subBoardCode != "mail"))
			{
				// Store the current sub-board code so we can see if it changed
				var oldSubCode = bbs.cursub_code;
				// Let the user choose another message area.  If they chose
				// a different message area, then set up the message base
				// object accordingly.
				this.SelectMsgArea();
				if (bbs.cursub_code != oldSubCode)
				{
					var chgSubRetval = this.ChangeSubBoard(bbs.cursub_code);
					continueOn = chgSubRetval.succeeded;
					if (chgSubRetval.succeeded)
					{
						console.print("\x01n");
						console.gotoxy(1, console.screen_rows);
						console.cleartoeol("\x01n");
						console.gotoxy(1, console.screen_rows);
						console.print("Loading...");
						this.PopulateHdrsForCurrentSubBoard();
						this.SetUpLightbarMsgListVars();
					}
				}
				// Update the lightbar list variables and refresh the header & help lines
				if (continueOn)
				{
					console.clear("\x01n");
					// Adjust the menu indexes to ensure they're correct for the current sub-board
					this.AdjustLightbarMsgListMenuIdxes(msgListMenu);
					this.WriteMsgListScreenTopHeader();
					DisplayHelpLine(this.msgListLightbarModeHelpLine);
				}
			}
			else
				drawMenu = false; // No need to re-draw the menu
		}
		else if (lastUserInputUpper == this.msgListKeys.showHelp) // Show help
		{
			console.clear("\x01n");
			this.DisplayMsgListHelp(allowChgSubBoard, true);
			// Re-draw the message list header & help line before
			// the menu is re-drawn
			this.WriteMsgListScreenTopHeader();
			DisplayHelpLine(this.msgListLightbarModeHelpLine);
		}
		// Spacebar: Select a message for batch operations (such as batch
		// delete, etc.)
		else if (lastUserInputUpper == " ")
		{
			this.ToggleSelectedMessage(this.subBoardCode, this.lightbarListSelectedMsgIdx);
			// Have the menu draw only the check character column in the
			// next iteration
			msgListMenu.nextDrawOnlyItemSubstr = { start: this.MSGNUM_LEN, end: this.MSGNUM_LEN+1 };
		}
		// Ctrl-A: Select/de-select all messages
		else if (lastUserInputUpper == CTRL_A)
		{
			console.gotoxy(1, console.screen_rows);
			console.print("\x01n");
			console.clearline();
			console.gotoxy(1, console.screen_rows);

			// Prompt the user to select All, None (un-select all), or Cancel
			console.print("\x01n\x01gSelect \x01c(\x01hA\x01n\x01c)\x01gll, \x01c(\x01hN\x01n\x01c)\x01gone, or \x01c(\x01hC\x01n\x01c)\x01gancel: \x01h\x01g");
			var userChoice = getAllowedKeyWithMode("ANC", K_UPPER | K_NOCRLF);
			if ((userChoice == "A") || (userChoice == "N"))
			{
				// Toggle all the messages
				var messageSelectToggle = (userChoice == "A");
				var totalNumMessages = this.NumMessages();
				var messageIndex = 0;
				for (messageIndex = 0; messageIndex < totalNumMessages; ++messageIndex)
					this.ToggleSelectedMessage(this.subBoardCode, messageIndex, messageSelectToggle);
				// Have the menu draw only the check character column in the
				// next iteration
				msgListMenu.nextDrawOnlyItemSubstr = { start: this.MSGNUM_LEN, end: this.MSGNUM_LEN+1 };
			}
			else
				drawMenu = false; // No need to re-draw the menu

			// Refresh the help line
			DisplayHelpLine(this.msgListLightbarModeHelpLine);
		}
		// Ctrl-D: Batch delete (for selected messages)
		else if (lastUserInputUpper == CTRL_D)
		{
			if (this.CanDelete() || this.CanDeleteLastMsg())
			{
				if (this.NumSelectedMessages() > 0)
				{
					console.gotoxy(1, console.screen_rows);
					console.print("\x01n");
					console.clearline();

					// The PromptAndDeleteOrUndeleteSelectedMessages() method will prompt the user for confirmation
					// to delete the message and then delete it if confirmed.
					this.PromptAndDeleteOrUndeleteSelectedMessages({ x: 1, y: console.screen_rows}, true);

					// In case all messages were deleted, if the user can't view deleted messages,
					// show an appropriate message and don't continue listing messages.
					//if (this.NumMessages(true) == 0)
					if (!this.NonDeletedMessagesExist() && !canViewDeletedMsgs())
						continueOn = false;
					else
					{
						// There are still messages to list, so refresh the header & help lines
						this.WriteMsgListScreenTopHeader();
						DisplayHelpLine(this.msgListLightbarModeHelpLine);
					}
				}
				else
				{
					// There are no selected messages
					writeWithPause(1, console.screen_rows, "\x01n\x01h\x01yThere are no selected messages.", ERROR_PAUSE_WAIT_MS, "\x01n", true);
					// Refresh the help line
					DisplayHelpLine(this.msgListLightbarModeHelpLine);
				}
			}
		}
		// U: Undelete message(s)
		else if (lastUserInputUpper == this.msgListKeys.undeleteMessage)
		{
			if (this.CanDelete() || this.CanDeleteLastMsg())
			{
				console.gotoxy(1, console.screen_rows);
				console.print("\x01n");
				console.clearline();
				if (this.NumSelectedMessages() > 0)
				{
					// Multi-message undelete
					this.PromptAndDeleteOrUndeleteSelectedMessages({ x: 1, y: console.screen_rows}, false);
				}
				else
				{
					// Single-message undelete
					this.PromptAndDeleteOrUndeleteMessage(this.lightbarListSelectedMsgIdx, { x: 1, y: console.screen_rows}, false);
				}

				// Refresh the header & help line.
				this.WriteMsgListScreenTopHeader();
				DisplayHelpLine(this.msgListLightbarModeHelpLine);
			}
		}
		else if (lastUserInputUpper == this.msgListKeys.userSettings)
		{
			var userSettingsRetObj = this.DoUserSettings_Scrollable(function(pReader) { DisplayHelpLine(pReader.msgListLightbarModeHelpLine); });
			lastUserInputUpper = "";
			drawMenu = userSettingsRetObj.needWholeScreenRefresh;
			// In case the user changed their twitlist, re-filter the messages for this sub-board
			if (userSettingsRetObj.userTwitListChanged)
			{
				console.gotoxy(1, console.screen_rows);
				console.crlf();
				console.print("\x01nTwitlist changed; re-filtering..");
				var tmpMsgbase = new MsgBase(this.subBoardCode);
				if (tmpMsgbase.open())
				{
					var tmpAllMsgHdrs = tmpMsgbase.get_all_msg_headers(true);
					tmpMsgbase.close();
					this.FilterMsgHdrsIntoHdrsForCurrentSubBoard(tmpAllMsgHdrs, true);
				}
				else
					console.print("\x01y\x01hFailed to open the messagbase!\x01\r\n\x01p");
				this.SetUpLightbarMsgListVars();
				msgListMenu = this.CreateLightbarMsgListMenu();
				drawMenu = true;
			}
			if (userSettingsRetObj.needWholeScreenRefresh)
			{
				this.WriteMsgListScreenTopHeader();
				DisplayHelpLine(this.msgListLightbarModeHelpLine);
			}
			else
				msgListMenu.DrawPartialAbs(userSettingsRetObj.optionBoxTopLeftX, userSettingsRetObj.optionBoxTopLeftY, userSettingsRetObj.optionBoxWidth, userSettingsRetObj.optionBoxHeight);
		}
		// S: Sorting options
		// TODO
		else if (lastUserInputUpper == "S")
		{
			// Refresh the help line
			DisplayHelpLine(this.msgListLightbarModeHelpLine);
		}
	}
	this.lightbarListSelectedMsgIdx = msgListMenu.selectedItemIdx;
	this.lightbarListTopMsgIdx = msgListMenu.topItemIdx;

	return retObj;
}
// For the DigDistMsgLister class: Creates & returns a DDLightbarMenu for
// performing the lightbar message list.
function DigDistMsgReader_CreateLightbarMsgListMenu()
{
	// Start & end indexes for the various items in each message list row
	var msgListIdxes = {
		msgNumStart: 0,
		msgNumEnd: this.MSGNUM_LEN,
		selectMarkStart: this.MSGNUM_LEN,
		selectMarkEnd: this.MSGNUM_LEN+1,
	};
	msgListIdxes.fromNameStart = this.MSGNUM_LEN + 1;
	msgListIdxes.fromNameEnd = msgListIdxes.fromNameStart + +this.FROM_LEN + 1;
	msgListIdxes.toNameStart = msgListIdxes.fromNameEnd;
	msgListIdxes.toNameEnd = msgListIdxes.toNameStart + +this.TO_LEN + 1;
	msgListIdxes.subjStart = msgListIdxes.toNameEnd;
	msgListIdxes.subjEnd = msgListIdxes.subjStart + +this.SUBJ_LEN + 1;
	if (this.showScoresInMsgList)
	{
		msgListIdxes.scoreStart = msgListIdxes.subjEnd;
		msgListIdxes.scoreEnd = msgListIdxes.scoreStart + +this.SCORE_LEN + 1;
		msgListIdxes.dateStart = msgListIdxes.scoreEnd;
	}
	else
		msgListIdxes.dateStart = msgListIdxes.subjEnd;
	msgListIdxes.dateEnd = msgListIdxes.dateStart + +this.DATE_LEN + 1;
	msgListIdxes.timeStart = msgListIdxes.dateEnd;
	msgListIdxes.timeEnd = console.screen_columns - 1; // msgListIdxes.timeStart + +this.TIME_LEN + 1;
	var msgListMenuHeight = console.screen_rows - this.lightbarMsgListStartScreenRow;
	var msgListMenu = new DDLightbarMenu(1, this.lightbarMsgListStartScreenRow, console.screen_columns, msgListMenuHeight);
	msgListMenu.scrollbarEnabled = true;
	msgListMenu.borderEnabled = false;
	msgListMenu.SetColors({
		itemColor: [{start: msgListIdxes.msgNumStart, end: msgListIdxes.msgNumEnd, attrs: this.colors.msgListMsgNumColor},
		            {start: msgListIdxes.selectMarkStart, end: msgListIdxes.selectMarkEnd, attrs: this.colors.selectedMsgMarkColor},
		            {start: msgListIdxes.fromNameStart, end: msgListIdxes.fromNameEnd, attrs: this.colors.msgListFromColor},
		            {start: msgListIdxes.toNameStart, end: msgListIdxes.toNameEnd, attrs: this.colors.msgListToColor},
		            {start: msgListIdxes.subjStart, end: msgListIdxes.subjEnd, attrs: this.colors.msgListSubjectColor},
		            {start: msgListIdxes.dateStart, end: msgListIdxes.dateEnd, attrs: this.colors.msgListDateColor},
		            {start: msgListIdxes.timeStart, end: msgListIdxes.timeEnd, attrs: this.colors.msgListTimeColor}],
		altItemColor: [{start: msgListIdxes.msgNumStart, end: msgListIdxes.msgNumEnd, attrs: this.colors.msgListToUserMsgNumColor},
		               {start: msgListIdxes.selectMarkStart, end: msgListIdxes.selectMarkEnd, attrs: this.colors.selectedMsgMarkColor},
		               {start: msgListIdxes.fromNameStart, end: msgListIdxes.fromNameEnd, attrs: this.colors.msgListToUserFromColor},
		               {start: msgListIdxes.toNameStart, end: msgListIdxes.toNameEnd, attrs: this.colors.msgListToUserToColor},
		               {start: msgListIdxes.subjStart, end: msgListIdxes.subjEnd, attrs: this.colors.msgListToUserSubjectColor},
		               {start: msgListIdxes.dateStart, end: msgListIdxes.dateEnd, attrs: this.colors.msgListToUserDateColor},
		               {start: msgListIdxes.timeStart, end: msgListIdxes.timeEnd, attrs: this.colors.msgListToUserTimeColor}],
		selectedItemColor: [{start: msgListIdxes.msgNumStart, end: msgListIdxes.msgNumEnd, attrs: this.colors.msgListMsgNumHighlightColor},
		                    {start: msgListIdxes.selectMarkStart, end: msgListIdxes.selectMarkEnd, attrs: this.colors.selectedMsgMarkColor + this.colors.msgListHighlightBkgColor},
		                    {start: msgListIdxes.fromNameStart, end: msgListIdxes.fromNameEnd, attrs: this.colors.msgListFromHighlightColor},
		                    {start: msgListIdxes.toNameStart, end: msgListIdxes.toNameEnd, attrs: this.colors.msgListToHighlightColor},
		                    {start: msgListIdxes.subjStart, end: msgListIdxes.subjEnd, attrs: this.colors.msgListSubjHighlightColor},
		                    {start: msgListIdxes.dateStart, end: msgListIdxes.dateEnd, attrs: this.colors.msgListDateHighlightColor},
		                    {start: msgListIdxes.timeStart, end: msgListIdxes.timeEnd, attrs: this.colors.msgListTimeHighlightColor}],
		altSelectedItemColor: [{start: msgListIdxes.msgNumStart, end: msgListIdxes.msgNumEnd, attrs: this.colors.msgListMsgNumHighlightColor},
		                       {start: msgListIdxes.selectMarkStart, end: msgListIdxes.selectMarkEnd, attrs: this.colors.selectedMsgMarkColor + this.colors.msgListHighlightBkgColor},
		                       {start: msgListIdxes.fromNameStart, end: msgListIdxes.fromNameEnd, attrs: this.colors.msgListFromHighlightColor},
		                       {start: msgListIdxes.toNameStart, end: msgListIdxes.toNameEnd, attrs: this.colors.msgListToHighlightColor},
		                       {start: msgListIdxes.subjStart, end: msgListIdxes.subjEnd, attrs: this.colors.msgListSubjHighlightColor},
		                       {start: msgListIdxes.dateStart, end: msgListIdxes.dateEnd, attrs: this.colors.msgListDateHighlightColor},
		                       {start: msgListIdxes.timeStart, end: msgListIdxes.timeEnd, attrs: this.colors.msgListTimeHighlightColor}]
	});
	// If we are to show message vote scores in the list (i.e., if the
	// user's terminal is wide enough), then splice in color specifiers
	// for the score column.
	if (this.showScoresInMsgList)
	{
		msgListMenu.colors.itemColor.splice(5, 0, {start: msgListIdxes.scoreStart, end: msgListIdxes.scoreEnd, attrs: this.colors.msgListScoreColor});
		msgListMenu.colors.altItemColor.splice(5, 0, {start: msgListIdxes.scoreStart, end: msgListIdxes.scoreEnd, attrs: this.colors.msgListToUserScoreColor});
		msgListMenu.colors.selectedItemColor.splice(5, 0, {start: msgListIdxes.scoreStart, end: msgListIdxes.scoreEnd, attrs: this.colors.msgListScoreHighlightColor + this.colors.msgListHighlightBkgColor});
		msgListMenu.colors.altSelectedItemColor.splice(5, 0, {start: msgListIdxes.scoreStart, end: msgListIdxes.scoreEnd, attrs: this.colors.msgListScoreHighlightColor + this.colors.msgListHighlightBkgColor});
	}

	msgListMenu.multiSelect = false;
	msgListMenu.ampersandHotkeysInItems = false;
	msgListMenu.wrapNavigation = false;

	// Add additional keypresses for quitting the menu's input loop so we can
	// respond to these keys
	// Ctrl-A: Select all messages
	var additionalQuitKeys = "EeqQgGcCsS ?0123456789" + CTRL_A + this.msgListKeys.batchDelete + this.msgListKeys.userSettings;
	if (this.CanDelete() || this.CanDeleteLastMsg())
		additionalQuitKeys += this.msgListKeys.deleteMessage + this.msgListKeys.undeleteMessage.toLowerCase() + this.msgListKeys.undeleteMessage.toUpperCase();
	if (this.CanEdit())
		additionalQuitKeys += this.msgListKeys.editMsg;
	msgListMenu.AddAdditionalQuitKeys(additionalQuitKeys);

	// Change the menu's NumItems() and GetItem() function to reference
	// the message list in this object rather than add the menu items
	// to the menu
	msgListMenu.msgReader = this; // Add this object to the menu object
	msgListMenu.NumItems = function() {
		return this.msgReader.NumMessages();
	};
	msgListMenu.GetItem = function(pItemIndex) {
		var menuItemObj = this.MakeItemWithRetval(-1);
		var itemIdx = (this.msgReader.reverseListOrder ? this.msgReader.NumMessages() - pItemIndex - 1 : pItemIndex);
		// In order to get vote score information (displayed if the user's terminal is wide
		// enough), the 2nd parameter to GetMsgHdrByIdx() should be true.
		var msgHdr = this.msgReader.GetMsgHdrByIdx(itemIdx, this.msgReader.showScoresInMsgList);
		if (msgHdr != null)
		{
			// When setting the item text, call PrintMessageInfo with true as
			// the last parameter to return the string instead
			menuItemObj.text = strip_ctrl(this.msgReader.PrintMessageInfo(msgHdr, false, itemIdx+1, true));
			menuItemObj.retval = msgHdr.number;
			if (this.msgReader.subBoardCode != "mail")
				menuItemObj.useAltColors = userHandleAliasNameMatch(msgHdr.to);
			// If the message is marked as deleted, ensure the correct color is used
			// for the mark character in the menu
			if ((msgHdr.attr & MSG_DELETE) == MSG_DELETE)
			{
				var fromColor = this.msgReader.colors.msgListFromColor;
				var toColor = this.msgReader.colors.msgListToColor;
				var subjColor = this.msgReader.colors.msgListSubjectColor;
				if ((this.msgReader.subBoardCode != "mail") && (userHandleAliasNameMatch(msgHdr.to)))
				{
					fromColor = this.msgReader.colors.msgListToUserFromColor;
					toColor = this.msgReader.colors.msgListToUserToColor;
					subjColor = this.msgReader.colors.msgListToUserSubjectColor;
				}
				menuItemObj.itemColor = [{start: msgListIdxes.msgNumStart, end: msgListIdxes.msgNumEnd, attrs: this.msgReader.colors.msgListMsgNumColor},
				                         {start: msgListIdxes.selectMarkStart, end: msgListIdxes.selectMarkEnd, attrs: "\x01r\x01h\x01i"},
				                         {start: msgListIdxes.fromNameStart, end: msgListIdxes.fromNameEnd, attrs: fromColor},
				                         {start: msgListIdxes.toNameStart, end: msgListIdxes.toNameEnd, attrs: toColor},
				                         {start: msgListIdxes.subjStart, end: msgListIdxes.subjEnd, attrs: subjColor},
				                         {start: msgListIdxes.dateStart, end: msgListIdxes.dateEnd, attrs: this.msgReader.colors.msgListDateColor},
				                         {start: msgListIdxes.timeStart, end: msgListIdxes.timeEnd, attrs: this.msgReader.colors.msgListTimeColor}];
				menuItemObj.itemSelectedColor = [{start: msgListIdxes.msgNumStart, end: msgListIdxes.msgNumEnd, attrs: this.msgReader.colors.msgListMsgNumHighlightColor},
				                                 {start: msgListIdxes.selectMarkStart, end: msgListIdxes.selectMarkEnd, attrs: "\x01r\x01h\x01i" + this.msgReader.colors.msgListHighlightBkgColor},
				                                 {start: msgListIdxes.fromNameStart, end: msgListIdxes.fromNameEnd, attrs: this.msgReader.colors.msgListFromHighlightColor},
				                                 {start: msgListIdxes.toNameStart, end: msgListIdxes.toNameEnd, attrs: this.msgReader.colors.msgListToHighlightColor},
				                                 {start: msgListIdxes.subjStart, end: msgListIdxes.subjEnd, attrs: this.msgReader.colors.msgListSubjHighlightColor},
				                                 {start: msgListIdxes.dateStart, end: msgListIdxes.dateEnd, attrs: this.msgReader.colors.msgListDateHighlightColor},
				                                 {start: msgListIdxes.timeStart, end: msgListIdxes.timeEnd, attrs: this.msgReader.colors.msgListTimeHighlightColor}];
			}
		}
		return menuItemObj;
	};

	// Adjust the menu indexes to ensure they're correct for the current sub-board
	this.AdjustLightbarMsgListMenuIdxes(msgListMenu);

	return msgListMenu;
}
// For the DigDistMsgLister class: Creates a DDLightbarMenu object for the user to choose
// a message group.
//
// Return value: A DDLightbarMenu object set up to let the user choose a message group
function DigDistMsgReader_CreateLightbarMsgGrpMenu()
{
	// Start & end indexes for the various items in each mssage group list row
	// Selected mark, group#, description, # sub-boards
	var msgGrpListIdxes = {
		markCharStart: 0,
		markCharEnd: 1,
		grpNumStart: 1,
		grpNumEnd: 2 + (+this.areaNumLen)
	};
	msgGrpListIdxes.descStart = msgGrpListIdxes.grpNumEnd;
	msgGrpListIdxes.descEnd = msgGrpListIdxes.descStart + +this.msgGrpDescLen;
	msgGrpListIdxes.numItemsStart = msgGrpListIdxes.descEnd;
	msgGrpListIdxes.numItemsEnd = msgGrpListIdxes.numItemsStart + +this.numItemsLen;
	// Set numItemsEnd to -1 to let the whole rest of the lines be colored
	msgGrpListIdxes.numItemsEnd = -1;
	var listStartRow = this.areaChangeHdrLines.length + 2;
	var msgGrpMenuHeight = console.screen_rows - listStartRow;
	var msgGrpMenu = new DDLightbarMenu(1, listStartRow, console.screen_columns, msgGrpMenuHeight);
	msgGrpMenu.scrollbarEnabled = true;
	msgGrpMenu.borderEnabled = false;
	msgGrpMenu.SetColors({
		itemColor: [{start: msgGrpListIdxes.markCharStart, end: msgGrpListIdxes.markCharEnd, attrs: this.colors.areaChooserMsgAreaMarkColor},
		            {start: msgGrpListIdxes.grpNumStart, end: msgGrpListIdxes.grpNumEnd, attrs: this.colors.areaChooserMsgAreaNumColor},
		            {start: msgGrpListIdxes.descStart, end: msgGrpListIdxes.descEnd, attrs: this.colors.areaChooserMsgAreaDescColor},
		            {start: msgGrpListIdxes.numItemsStart, end: msgGrpListIdxes.numItemsEnd, attrs: this.colors.areaChooserMsgAreaNumItemsColor}],
		selectedItemColor: [{start: msgGrpListIdxes.markCharStart, end: msgGrpListIdxes.markCharEnd, attrs: this.colors.areaChooserMsgAreaMarkColor + this.colors.areaChooserMsgAreaBkgHighlightColor},
		                    {start: msgGrpListIdxes.grpNumStart, end: msgGrpListIdxes.grpNumEnd, attrs: this.colors.areaChooserMsgAreaNumHighlightColor},
		                    {start: msgGrpListIdxes.descStart, end: msgGrpListIdxes.descEnd, attrs: this.colors.areaChooserMsgAreaDescHighlightColor},
		                    {start: msgGrpListIdxes.numItemsStart, end: msgGrpListIdxes.numItemsEnd, attrs: this.colors.areaChooserMsgAreaNumItemsHighlightColor}]
	});

	msgGrpMenu.multiSelect = false;
	msgGrpMenu.ampersandHotkeysInItems = false;
	msgGrpMenu.wrapNavigation = false;

	// Add additional keypresses for quitting the menu's input loop so we can
	// respond to these keys
	msgGrpMenu.AddAdditionalQuitKeys("nNqQ ?0123456789/" + CTRL_F);

	// Change the menu's NumItems() and GetItem() function to reference
	// the message list in this object rather than add the menu items
	// to the menu
	msgGrpMenu.msgReader = this; // Add this object to the menu object
	msgGrpMenu.NumItems = function() {
		return msg_area.grp_list.length;
	};
	msgGrpMenu.GetItem = function(pGrpIndex) {
		var menuItemObj = this.MakeItemWithRetval(-1);
		if ((pGrpIndex >= 0) && (pGrpIndex < msg_area.grp_list.length))
		{
			menuItemObj.text = format(((typeof(bbs.curgrp) == "number") && (pGrpIndex == msg_area.sub[this.msgReader.subBoardCode].grp_index)) ? "*" : " ");
			menuItemObj.text += format(this.msgReader.msgGrpListPrintfStr, +(pGrpIndex+1),
			                           msg_area.grp_list[pGrpIndex].description.substr(0, this.msgReader.msgGrpDescLen),
			                           msg_area.grp_list[pGrpIndex].sub_list.length);
			menuItemObj.text = strip_ctrl(menuItemObj.text);
			menuItemObj.retval = pGrpIndex;
		}

		return menuItemObj;
	};

	// Set the currently selected item to the current group
	msgGrpMenu.selectedItemIdx = msg_area.sub[this.subBoardCode].grp_index;
	if (msgGrpMenu.selectedItemIdx >= msgGrpMenu.topItemIdx+msgGrpMenu.GetNumItemsPerPage())
		msgGrpMenu.topItemIdx = msgGrpMenu.selectedItemIdx - msgGrpMenu.GetNumItemsPerPage() + 1;

	return msgGrpMenu;
}
// For the DigDistMsgLister class: Creates a DDLightbarMenu object for the user to choose
// a sub-board within a message group.
//
// Parameters:
//  pGrpIdx: The index of the group to list sub-boards for
//
// Return value: A DDLightbarMenu object set up to let the user choose a sub-board within the
//               given message group
function DigDistMsgReader_CreateLightbarSubBoardMenu(pGrpIdx)
{
	// Start & end indexes for the various items in each sub-board list row
	// Selected mark, group#, description, # sub-boards
	var subBrdListIdxes = {
		markCharStart: 0,
		markCharEnd: 1,
		subNumStart: 1,
		subNumEnd: 2 + (+this.areaNumLen)
	};
	subBrdListIdxes.descStart = subBrdListIdxes.subNumEnd;
	subBrdListIdxes.descEnd = subBrdListIdxes.descStart + +(this.subBoardListPrintfInfo[pGrpIdx].nameLen) + 1;
	subBrdListIdxes.numItemsStart = subBrdListIdxes.descEnd;
	subBrdListIdxes.numItemsEnd = subBrdListIdxes.numItemsStart + +(this.subBoardListPrintfInfo[pGrpIdx].numMsgsLen) + 1;
	subBrdListIdxes.dateStart = subBrdListIdxes.numItemsEnd;
	subBrdListIdxes.dateEnd = subBrdListIdxes.dateStart + +this.dateLen + 1;
	subBrdListIdxes.timeStart = subBrdListIdxes.dateEnd;
	// Set timeEnd to -1 to let the whole rest of the lines be colored
	subBrdListIdxes.timeEnd = -1;
	var listStartRow = this.areaChangeHdrLines.length + 3;
	var subBrdMenuHeight = console.screen_rows - listStartRow;
	var subBoardMenu = new DDLightbarMenu(1, listStartRow, console.screen_columns, subBrdMenuHeight);
	subBoardMenu.scrollbarEnabled = true;
	subBoardMenu.borderEnabled = false;
	subBoardMenu.SetColors({
		itemColor: [{start: subBrdListIdxes.markCharStart, end: subBrdListIdxes.markCharEnd, attrs: this.colors.areaChooserMsgAreaMarkColor},
		            {start: subBrdListIdxes.subNumStart, end: subBrdListIdxes.subNumEnd, attrs: this.colors.areaChooserMsgAreaNumColor},
		            {start: subBrdListIdxes.descStart, end: subBrdListIdxes.descEnd, attrs: this.colors.areaChooserMsgAreaDescColor},
		            {start: subBrdListIdxes.numItemsStart, end: subBrdListIdxes.numItemsEnd, attrs: this.colors.areaChooserMsgAreaNumItemsColor},
		            {start: subBrdListIdxes.dateStart, end: subBrdListIdxes.dateEnd, attrs: this.colors.areaChooserMsgAreaLatestDateColor},
		            {start: subBrdListIdxes.timeStart, end: subBrdListIdxes.timeEnd, attrs: this.colors.areaChooserMsgAreaLatestTimeColor}],
		selectedItemColor: [{start: subBrdListIdxes.markCharStart, end: subBrdListIdxes.markCharEnd, attrs: this.colors.areaChooserMsgAreaMarkColor + this.colors.areaChooserMsgAreaBkgHighlightColor},
		                    {start: subBrdListIdxes.subNumStart, end: subBrdListIdxes.subNumEnd, attrs: this.colors.areaChooserMsgAreaNumHighlightColor},
		                    {start: subBrdListIdxes.descStart, end: subBrdListIdxes.descEnd, attrs: this.colors.areaChooserMsgAreaDescHighlightColor},
		                    {start: subBrdListIdxes.numItemsStart, end: subBrdListIdxes.numItemsEnd, attrs: this.colors.areaChooserMsgAreaNumItemsHighlightColor},
		                    {start: subBrdListIdxes.dateStart, end: subBrdListIdxes.dateEnd, attrs: this.colors.areaChooserMsgAreaDateHighlightColor},
		                    {start: subBrdListIdxes.timeStart, end: subBrdListIdxes.timeEnd, attrs: this.colors.areaChooserMsgAreaTimeHighlightColor}]
	});

	subBoardMenu.multiSelect = false;
	subBoardMenu.ampersandHotkeysInItems = false;
	subBoardMenu.wrapNavigation = false;

	// Add additional keypresses for quitting the menu's input loop so we can
	// respond to these keys
	subBoardMenu.AddAdditionalQuitKeys("nNqQ ?0123456789/" + CTRL_F);

	// Change the menu's NumItems() and GetItem() function to reference
	// the message list in this object rather than add the menu items
	// to the menu
	subBoardMenu.msgReader = this; // Add this object to the menu object
	subBoardMenu.grpIdx = pGrpIdx;
	subBoardMenu.NumItems = function() {
		return msg_area.grp_list[pGrpIdx].sub_list.length;
	};
	subBoardMenu.GetItem = function(pSubIdx) {
		var menuItemObj = this.MakeItemWithRetval(-1);
		if ((pSubIdx >= 0) && (pSubIdx < msg_area.grp_list[this.grpIdx].sub_list.length))
		{
			//var highlight = (msg_area.grp_list[this.grpIdx].sub_list[pSubIdx].code.toUpperCase() == this.msgReader.subBoardCode.toUpperCase());
			menuItemObj.text = this.msgReader.GetMsgSubBoardLine(this.grpIdx, pSubIdx, false);
			menuItemObj.text = strip_ctrl(menuItemObj.text);
			menuItemObj.retval = pSubIdx;
		}

		return menuItemObj;
	};

	// Set the currently selected item to the current group
	if (msg_area.sub[this.subBoardCode].grp_index == pGrpIdx)
	{
		subBoardMenu.selectedItemIdx = msg_area.sub[this.subBoardCode].index;
		if (subBoardMenu.selectedItemIdx >= subBoardMenu.topItemIdx+subBoardMenu.GetNumItemsPerPage())
			subBoardMenu.topItemIdx = subBoardMenu.selectedItemIdx - subBoardMenu.GetNumItemsPerPage() + 1;
	}
	else
	{
		subBoardMenu.selectedItemIdx = 0;
		subBoardMenu.topItemIdx = 0;
	}

	return subBoardMenu;
}
// For the DigDistMsgLister class: Adjusts lightbar menu indexes for a message list menu
function DigDistMsgReader_AdjustLightbarMsgListMenuIdxes(pMsgListMenu)
{
	pMsgListMenu.selectedItemIdx = this.lightbarListSelectedMsgIdx;
	pMsgListMenu.topItemIdx = this.lightbarListTopMsgIdx;

	// In the DDLightbarMenu class, the top index on the last page should
	// allow for displaying a full page of items.  So if
	// this.lightbarListTopMsgIdx is beyond the top index for the last
	// page in the menu object, then adjust this.lightbarListTopMsgIdx.
	var menuTopItemIdxOnLastPage = pMsgListMenu.GetTopItemIdxOfLastPage();
	if (pMsgListMenu.topItemIdx > menuTopItemIdxOnLastPage)
	{
		pMsgListMenu.topItemIdx = menuTopItemIdxOnLastPage;
		this.lightbarListTopMsgIdx = menuTopItemIdxOnLastPage;
	}
	// TODO: Ensure this.lightbarListTopMsgIdx is always correct for the last page
}
// For the DigDistMsgListerClass: Prints a line of information about
// a message.
//
// Parameters:
//  pMsgHeader: The message header object, returned by MsgBase.get_msg_header().
//  pHighlight: Optional boolean - Whether or not to highlight the line (true) or
//              use the standard colors (false).
//  pMsgNum: Optional - A number to use for the message instead of the number/offset
//           in the message header
//  pReturnStrInstead: Optional boolean - Whether or not to return a formatted string
//                     instead of printing to the console.  Defaults to false.
function DigDistMsgReader_PrintMessageInfo(pMsgHeader, pHighlight, pMsgNum, pReturnStrInstead)
{
	// pMsgHeader must be a valid object.
	if (typeof(pMsgHeader) == "undefined")
		return;
	if (pMsgHeader == null)
		return;

	var highlight = false;
	if (typeof(pHighlight) == "boolean")
		highlight = pHighlight;

	// Get the message's import date & time as strings.  If
	// this.msgList_displayMessageDateImported is true, use the message import date.
	// Otherwise, use the message written date.
	var sDate;
	var sTime;
	if (this.msgList_displayMessageDateImported)
	{
		sDate = strftime("%Y-%m-%d", pMsgHeader.when_imported_time);
		sTime = strftime("%H:%M:%S", pMsgHeader.when_imported_time);
	}
	else
	{
		//sDate = strftime("%Y-%m-%d", pMsgHeader.when_written_time);
		//sTime = strftime("%H:%M:%S", pMsgHeader.when_written_time);
		var msgWrittenLocalTime = msgWrittenTimeToLocalBBSTime(pMsgHeader);
		if (msgWrittenLocalTime != -1)
		{
			sDate = strftime("%Y-%m-%d", msgWrittenLocalTime);
			sTime = strftime("%H:%M:%S", msgWrittenLocalTime);
		}
		else
		{
			sDate = strftime("%Y-%m-%d", pMsgHeader.when_written_time);
			sTime = strftime("%H:%M:%S", pMsgHeader.when_written_time);
		}
	}

	//var msgNum = (typeof(pMsgNum) == "number" ? pMsgNum : pMsgHeader.offset+1);
	var msgNum = (typeof(pMsgNum) == "number" ? pMsgNum : this.GetMsgIdx(pMsgHeader.number)+1);
	if (msgNum == 0) // In case GetMsgIdx() returns -1 for failure
		msgNum = 1;

	// Determine if the message has been deleted.
	var msgDeleted = ((pMsgHeader.attr & MSG_DELETE) == MSG_DELETE);

	// msgIndicatorChar will contain (possibly) a character to display after
	// the message number to indicate whether it has been deleted, selected,
	// etc.  If not, then it will just be a space.
	var msgIndicatorChar = " ";

	// Get the message score value
	var msgVoteInfo = getMsgUpDownvotesAndScore(pMsgHeader);
	// Ensure the score number can fit within 4 digits
	if (msgVoteInfo.voteScore > 9999)
		msgVoteInfo.voteScore = 9999;
	else if (msgVoteInfo.voteScore < -999)
		msgVoteInfo.voteScore = -999;

	// Generate the string with the message header information.
	var msgHdrStr = "";
	// Note: The message header has the following fields:
	// 'number': The message number
	// 'offset': The message offset
	// 'to': Who the message is directed to (string)
	// 'from' Who wrote the message (string)
	// 'subject': The message subject (string)
	// 'date': The date - Full text (string)
	// To access one of these, use brackets; i.e., msgHeader['to']
	if (highlight)
	{
		// For any indicator character next to the message, prioritize selected, then deleted, then attachments
		if (this.MessageIsSelected(this.subBoardCode, msgNum-1))
			msgIndicatorChar = "\x01n" + this.colors.selectedMsgMarkColor + this.colors.msgListHighlightBkgColor + CHECK_CHAR + "\x01n";
		else if (msgDeleted)
			msgIndicatorChar = "\x01n\x01r\x01h\x01i" + this.colors.msgListHighlightBkgColor + "*\x01n";
		else if (msgHdrHasAttachmentFlag(pMsgHeader))
			msgIndicatorChar = "\x01n" + this.colors.selectedMsgMarkColor + this.colors.msgListHighlightBkgColor + "A\x01n";
		var fromName = pMsgHeader.from;
		// If the message was posted anonymously and the logged-in user is
		// not the sysop, then show "Anonymous" for the 'from' name.
		if (!user.is_sysop && ((pMsgHeader.attr & MSG_ANONYMOUS) == MSG_ANONYMOUS))
			fromName = "Anonymous";
		if (this.showScoresInMsgList)
		{
			msgHdrStr += format(this.sMsgInfoFormatHighlightStr, msgNum, msgIndicatorChar,
			       fromName.substr(0, this.FROM_LEN),
			       pMsgHeader.to.substr(0, this.TO_LEN),
			       pMsgHeader.subject.substr(0, this.SUBJ_LEN),
			       msgVoteInfo.voteScore, sDate, sTime);
		}
		else
		{
			msgHdrStr += format(this.sMsgInfoFormatHighlightStr, msgNum, msgIndicatorChar,
			       fromName.substr(0, this.FROM_LEN),
			       pMsgHeader.to.substr(0, this.TO_LEN),
			       pMsgHeader.subject.substr(0, this.SUBJ_LEN),
			       sDate, sTime);
		}
	}
	else
	{
		// For any indicator character next to the message, prioritize selected, then deleted, then attachments
		if (this.MessageIsSelected(this.subBoardCode, msgNum-1))
			msgIndicatorChar = "\x01n" +  this.colors.selectedMsgMarkColor + CHECK_CHAR + "\x01n";
		else if (msgDeleted)
			msgIndicatorChar = "\x01n\x01r\x01h\x01i*\x01n";
		else if (msgHdrHasAttachmentFlag(pMsgHeader))
			msgIndicatorChar = "\x01n" +  this.colors.selectedMsgMarkColor + "A\x01n";

		// Determine whether to use the normal, "to-user", or "from-user" format string.
		// The differences are the colors.  Then, output the message information line.
		var toNameUpper = pMsgHeader.to.toUpperCase();
		var msgToUser = ((toNameUpper == user.alias.toUpperCase()) || (toNameUpper == user.name.toUpperCase()) || (toNameUpper == user.handle.toUpperCase()));
		var fromNameUpper = pMsgHeader.from.toUpperCase();
		var msgIsFromUser = ((fromNameUpper == user.alias.toUpperCase()) || (fromNameUpper == user.name.toUpperCase()) || (fromNameUpper == user.handle.toUpperCase()));
		var formatStr = ""; // Format string for printing the message information
		if (this.readingPersonalEmail)
			formatStr = this.sMsgInfoFormatStr;
		else
			formatStr = (msgToUser ? this.sMsgInfoToUserFormatStr : (msgIsFromUser ? this.sMsgInfoFromUserFormatStr : this.sMsgInfoFormatStr));
		var fromName = pMsgHeader.from;
		// If the message was posted anonymously and the logged-in user is
		// not the sysop, then show "Anonymous" for the 'from' name.
		if (!user.is_sysop && ((pMsgHeader.attr & MSG_ANONYMOUS) == MSG_ANONYMOUS))
			fromName = "Anonymous";
		if (this.showScoresInMsgList)
		{
			msgHdrStr += format(formatStr, msgNum, msgIndicatorChar, fromName.substr(0, this.FROM_LEN),
			       pMsgHeader.to.substr(0, this.TO_LEN), pMsgHeader.subject.substr(0, this.SUBJ_LEN),
			       msgVoteInfo.voteScore, sDate, sTime);
		}
		else
		{
			msgHdrStr += format(formatStr, msgNum, msgIndicatorChar, fromName.substr(0, this.FROM_LEN),
			       pMsgHeader.to.substr(0, this.TO_LEN), pMsgHeader.subject.substr(0, this.SUBJ_LEN),
			       sDate, sTime);
		}
	}

	var returnStrInstead = (typeof(pReturnStrInstead) == "boolean" ? pReturnStrInstead : false);
	if (!returnStrInstead)
	{
		console.print(msgHdrStr);
		console.cleartoeol("\x01n"); // To clear away any extra text that may have been entered by the user
	}
	return msgHdrStr;
}
// For the traditional interface of DigDistMsgListerClass: Prompts the user to
// continue or read a message (by number).
//
// Parameters:
//  pStart: Whether or not we're on the first page (true or false)
//  pEnd: Whether or not we're at the last page (true or false)
//  pAllowChgSubBoard: Optional - A boolean to specify whether or not to allow
//                     changing to another sub-board.  Defaults to true.
//
// Return value: An object with the following properties:
//               continueOn: Boolean, whether or not the user wants to continue
//                           listing the messages
//               userInput: The user's input
//               selectedMsgOffset: The offset of the message selected to read,
//                                  if one was selected.  If a message was not
//                                  selected, this will be -1.
function DigDistMsgReader_PromptContinueOrReadMsg(pStart, pEnd, pAllowChgSubBoard)
{
	// Create the return object and set some initial default values
	var retObj = {
		continueOn: true,
		userInput: "",
		selectedMsgOffset: -1
	};

	var allowChgSubBoard = (typeof(pAllowChgSubBoard) == "boolean" ? pAllowChgSubBoard : true);

	var continueOn = true;
	// Prompt the user whether or not to continue or to read a message
	// (by message number).  Make use of the different prompt texts,
	// depending whether we're at the beginning, in the middle, or at
	// the end of the message list.
	var userInput = "";
	var allowedKeys = "?GS"; // ? = help, G = Go to message #, S = Select message(s), Ctrl-D: Batch delete
	allowedKeys += this.msgListKeys.userSettings;
	if (allowChgSubBoard)
		allowedKeys += "C"; // Change to another message area
	if (this.CanDelete() || this.CanDeleteLastMsg())
		allowedKeys += "D"; // Delete
	if (this.CanEdit())
		allowedKeys += "E"; // Edit
	if (pStart && pEnd)
	{
		// This is the only page.
		console.print(this.msgListOnlyOnePageContinuePrompt);
		// Get input from the user.  Allow only Q (quit).
		allowedKeys += "Q";
	}
	else if (pStart)
	{
		// We're on the first page.
		console.print(this.sStartContinuePrompt);
		// Get input from the user.  Allow only L (last), N (next), or Q (quit).
		allowedKeys += "LNQ";
	}
	else if (pEnd)
	{
		// We're on the last page.
		console.print(this.sEndContinuePrompt);
		// Get input from the user.  Allow only F (first), P (previous), or Q (quit).
		allowedKeys += "FPQ";
	}
	else
	{
		// We're neither on the first nor last page.  Allow F (first), L (last),
		// N (next), P (previous), or Q (quit).
		console.print(this.sContinuePrompt);
		allowedKeys += "FLNPQ";
	}
	// Get the user's input.  Allow CTRL-D (batch delete) without echoing it.
	// If the user didn't press CTRL-L, allow the keys in allowedKeys or a number from 1
	// to the highest message number.
	userInput = console.getkey(K_NOECHO);
	if (userInput != CTRL_D)
	{
		console.ungetstr(userInput);
		userInput = console.getkeys(allowedKeys, this.HighestMessageNum()).toString();
	}
	if (userInput == "Q")
		continueOn = false;

	// If the user has typed all numbers, then read that message.
	if ((userInput != "") && /^[0-9]+$/.test(userInput))
	{
		// If the user entered a valid message number, then let the user read the message.
		// The message number might be invalid if there are search results that
		// have non-continuous message numbers.
		if (this.IsValidMessageNum(userInput))
		{
			// See if the current message header has our "isBogus" property and it's true.
			// Only let the user read the message if it's not a bogus message header.
			// The message header could have the "isBogus" property, for instance, if
			// it's a vote message (introduced in Synchronet 3.17).
			var tmpMsgHdr = this.GetMsgHdrByIdx(+(userInput-1), false);
			var hdrIsBogus = (tmpMsgHdr.hasOwnProperty("isBogus") ? tmpMsgHdr.isBogus : false);
			if (!hdrIsBogus)
			{
				// Confirm with the user whether to read the message
				var readMsg = true;
				if (this.promptToReadMessage)
				{
					var sReadMsgConfirmText = this.colors["readMsgConfirmColor"]
											+ "Read message "
											+ this.colors["readMsgConfirmNumberColor"]
											+ userInput + this.colors["readMsgConfirmColor"]
											+ ": Are you sure";
					readMsg = console.yesno(sReadMsgConfirmText);
				}
				if (readMsg)
				{
					// Update the message list screen variables
					this.CalcMsgListScreenIdxVarsFromMsgNum(+userInput);
					// Return from here so that the calling function can switch
					// into reader mode.
					retObj.continueOn = continueOn;
					retObj.userInput = userInput;
					retObj.selectedMsgOffset = userInput-1;
					return retObj;
				}
				else
					this.deniedReadingMessage = true;

				// Prompt the user whether or not to continue listing
				// messages.
				if (this.promptToContinueListingMessages)
				{
					continueOn = console.yesno(this.colors["afterReadMsg_ListMorePromptColor"] +
											   "Continue listing messages");
				}
			}
			else
			{
				console.print("\x01n\x01h\x01yThat's not a readable message.\x01n");
				console.crlf();
				console.pause();
			}
		}
		else
		{
			// The user entered an invalid message number.
			console.print("\x01n\x01h\x01w" + userInput + " \x01y is not a valid message number.\x01n");
			console.crlf();
			console.pause();
			continueOn = true;
		}
	}

	// Make sure color highlighting is turned off
	console.print("\x01n");

	// Fill the return object with the required values, and return it.
	retObj.continueOn = continueOn;
	retObj.userInput = userInput;
	return retObj;
}
// For the DigDistMsgReader Class: Given a message number of a message in the
// current message area, lets the user read a message and allows the user to
// respond, etc.  This is an enhanced version that allows scrolling up & down
// the message with the up & down arrow keys, and the left & right arrow keys
// will return from the function to allow calling code to navigate back & forth
// through the message sub-board.
//
// Parameters:
//  pOffset: The offset of the message to be read
//  pAllowChgArea: Optional boolean - Whether or not to allow changing the
//                 message area
//
// Return value: And object with the following properties:
//               offsetValid: Boolean - Whether or not the passed-in offset was valid
//               msgDeleted: Boolean - Whether or not the message is marked as deleted
//                           (not deleted by the user in the reader)
//               userReplied: Boolean - Whether or not the user replied to the message.
//               lastKeypress: The last keypress from the user - For navigation purposes
//               newMsgOffset: The offset of another message to read, if the user
//                             input another message number.  If the user did not
//                             input another message number, this will be -1.
//               nextAction: The next action for the caller to take.  This will be
//                           one of the values specified by the ACTION_* constants.
//                           This defaults to ACTION_NONE on error.
//               refreshEnhancedRdrHelpLine: Boolean - Whether or not to refresh the
//                                           enhanced reader help line on the screen
//                                           (for instance, if switched to the traditional
//                                            non-scrolling interface to read the message)
function DigDistMsgReader_ReadMessageEnhanced(pOffset, pAllowChgArea)
{
	var retObj = {
		offsetValid: true,
		msgNotReadable: false,
		userReplied: false,
		lastKeypress: "",
		newMsgOffset: -1,
		nextAction: ACTION_NONE,
		refreshEnhancedRdrHelpLine: false
	};
	
	// Get the message header.  Don't expand fields since we may need to save
	// the header later with the MSG_READ attribute.
	//var msgHeader = this.GetMsgHdrByIdx(pOffset, false);
	// Get the message header.  Get expanded fields so that we can show any
	// voting stats/responses that may be included with the message.
	var msgHeader = this.GetMsgHdrByIdx(pOffset, true);
	if (msgHeader == null)
	{
		//console.print("\x01n" + replaceAtCodesInStr(format(this.text.invalidMsgNumText, +(pOffset+1))) + "\x01n");
		//console.crlf();
		console.putmsg("\x01n" + format(this.text.invalidMsgNumText, +(pOffset+1)) + "\x01n");
		console.inkey(K_NONE, ERROR_PAUSE_WAIT_MS);
		retObj.offsetValid = false;
		return retObj;
	}

	// If this message is not readable for the user (it's marked as deleted and
	// the system is set to not show deleted messages, etc.), then don't let the
	// user read it, and just silently return.
	// TODO: If the message is not readable, this will end up causing an infinite loop.
	retObj.msgNotReadable = !isReadableMsgHdr(msgHeader, this.subBoardCode);
	if (retObj.msgNotReadable)
		return retObj;

	// Update the message list index variables so that the message list is in
	// the right spot for the message currently being read
	this.CalcMsgListScreenIdxVarsFromMsgNum(pOffset+1);

	// Check the pAllowChgArea parameter.  If it's a boolean, then use it.  If
	// not, then check to see if we're reading personal mail - If not, then allow
	// the user to change to a different message area.
	var allowChgMsgArea = true;
	if (typeof(pAllowChgArea) == "boolean")
		allowChgMsgArea = pAllowChgArea;
	else
		allowChgMsgArea = (this.subBoardCode != "mail");

	// Get the message text and see if it has any ANSI codes.  Remove any pause
	// codes it might have.  If it has ANSI codes, then don't use the scrolling
	// interface so that the ANSI gets displayed properly.
	var messageText = this.GetMsgBody(msgHeader);

	if (msgHdrHasAttachmentFlag(msgHeader))
	{
		messageText = "\x01n\x01g\x01h- This message contains one or more attachments. Press CTRL-A to download.\x01n\r\n"
		            + "\x01n\x01g\x01h--------------------------------------------------------------------------\x01n\r\n"
		            + messageText;
	}
	var useScrollingInterface = this.scrollingReaderInterface && console.term_supports(USER_ANSI);
	// If we switch to the non-scrolling interface here, then the calling method should
	// refresh the enhanced reader help line on the screen.
	retObj.refreshEnhancedRdrHelpLine = (this.scrollingReaderInterface && !useScrollingInterface);
	// If the current message is new to the user, update the number of posts read this session.
	if (pOffset > this.GetScanPtrMsgIdx())
		++bbs.posts_read;
	// Use the scrollable reader interface if the setting is enabled & the user's
	// terminal supports ANSI.  Otherwise, use a more traditional user interface.
	if (useScrollingInterface)
		retObj = this.ReadMessageEnhanced_Scrollable(msgHeader, allowChgMsgArea, messageText, pOffset);
	else
		retObj = this.ReadMessageEnhanced_Traditional(msgHeader, allowChgMsgArea, messageText, pOffset);

	// Mark the message as read if it was written to the current user
	if (userNameHandleAliasMatch(msgHeader.to) && ((msgHeader.attr & MSG_READ) == 0))
	{
		// Using applyAttrsInMsgHdrInMessagbase(), which loads the header without
		// expanded fields and saves the attributes with that header.
		var saveRetObj = applyAttrsInMsgHdrInMessagbase(this.subBoardCode, msgHeader.number, MSG_READ);
		if (this.SearchTypePopulatesSearchResults() && saveRetObj.saveSucceeded)
			this.RefreshHdrInSavedArrays(pOffset, MSG_READ, true);
	}

	// If not reading personal email and not doing a search, then update the
	// scan & last read message pointers.
	if ((this.subBoardCode != "mail") && (this.searchType == SEARCH_NONE))
	{
		if (msgHeader.number > GetScanPtrOrLastMsgNum(this.subBoardCode))
			msg_area.sub[this.subBoardCode].scan_ptr = msgHeader.number;
		msg_area.sub[this.subBoardCode].last_read = msgHeader.number;
	}

	return retObj;
}
// Helper method for ReadMessageEnhanced() - Does the scrollable reader interface
function DigDistMsgReader_ReadMessageEnhanced_Scrollable(msgHeader, allowChgMsgArea, messageText, pOffset)
{
	var retObj = {
		offsetValid: true,
		msgNotReadable: false,
		userReplied: false,
		lastKeypress: "",
		newMsgOffset: -1,
		nextAction: ACTION_NONE,
		refreshEnhancedRdrHelpLine: false
	};

	// This is a scrollbar update function for use when viewing the message.
	function msgScrollbarUpdateFn(pFractionToLastPage)
	{
		// Update the scrollbar position for the message, depending on the
		// value of pFractionToLastMessage.
		fractionToLastPage = pFractionToLastPage;
		solidBlockStartRow = msgReaderObj.msgAreaTop + Math.floor(numNonSolidScrollBlocks * pFractionToLastPage);
		if (solidBlockStartRow != solidBlockLastStartRow)
			msgReaderObj.UpdateEnhancedReaderScollbar(solidBlockStartRow, solidBlockLastStartRow, numSolidScrollBlocks);
		solidBlockLastStartRow = solidBlockStartRow;
		console.gotoxy(1, console.screen_rows);
	}
	// This is a scrollbar update function for use when viewing the header info/kludge lines.
	function msgInfoScrollbarUpdateFn(pFractionToLastPage)
	{
		var infoSolidBlockStartRow = msgReaderObj.msgAreaTop + Math.floor(numNonSolidInfoScrollBlocks * pFractionToLastPage);
		if (infoSolidBlockStartRow != lastInfoSolidBlockStartRow)
			msgReaderObj.UpdateEnhancedReaderScollbar(infoSolidBlockStartRow, lastInfoSolidBlockStartRow, numInfoSolidScrollBlocks);
		lastInfoSolidBlockStartRow = infoSolidBlockStartRow;
		console.gotoxy(1, console.screen_rows);
	}

	var msgAreaWidth = this.useEnhReaderScrollbar ? this.msgAreaWidth : this.msgAreaWidth + 1;

	// We could word-wrap the message to ensure words aren't split across lines, but
	// doing so could make some messages look bad (i.e., messages with drawing characters),
	// and word_wrap also might not handle ANSI or other color/attribute codes..
	//if (!textHasDrawingChars(messageText))
	//	messageText = word_wrap(messageText, msgAreaWidth);

	// If the message has ANSI content, then use a Graphic object to help make
	// the message look good.  Also, remove any ANSI clear screen codes from the
	// message text.
	var msgHasANSICodes = messageText.indexOf("\x1b[") >= 0;
	if (msgHasANSICodes)
	{
		messageText = messageText.replace(/\u001b\[[012]J/gi, "");
		var graphic = new Graphic(msgAreaWidth, this.msgAreaHeight-1);
		graphic.auto_extend = true;
		graphic.ANSI = ansiterm.expand_ctrl_a(messageText);
		//graphic.normalize();
		//graphic.width = msgAreaWidth;
		//messageText = graphic.MSG.split('\n');
		messageText = graphic.MSG;
	}

	// Show the message header
	this.DisplayEnhancedMsgHdr(msgHeader, pOffset+1, 1);

	// Get the message text, interpret any @-codes in it, replace tabs with spaces
	// to prevent weirdness when displaying the message lines, and word-wrap the
	// text so that it looks good on the screen,
	var msgInfo = this.GetMsgInfoForEnhancedReader(msgHeader, true, true, true, messageText);

	var topMsgLineIdxForLastPage = msgInfo.topMsgLineIdxForLastPage;
	var numSolidScrollBlocks = msgInfo.numSolidScrollBlocks;
	var numNonSolidScrollBlocks = msgInfo.numNonSolidScrollBlocks;
	var solidBlockStartRow = msgInfo.solidBlockStartRow;
	var solidBlockLastStartRow = solidBlockStartRow;
	var topMsgLineIdx = 0;
	var fractionToLastPage = 0;
	if (topMsgLineIdxForLastPage != 0)
		fractionToLastPage = topMsgLineIdx / topMsgLineIdxForLastPage;

	// If use of the scrollbar is enabled, draw an initial scrollbar on the rightmost
	// column of the message area showing the fraction of the message shown and what
	// part of the message is currently being shown.  The scrollbar will be updated
	// minimally in the input loop to minimize screen redraws.
	if (this.useEnhReaderScrollbar)
		this.DisplayEnhancedReaderWholeScrollbar(solidBlockStartRow, numSolidScrollBlocks);

	// Input loop (for scrolling the message up & down)
	var msgLineFormatStr = "%-" + msgAreaWidth + "s";
	var writeMessage = true;
	// msgAreaHeight, msgReaderObj, and scrollbarUpdateFunction are for use
	// with scrollTextLines().
	var msgAreaHeight = this.msgAreaBottom - this.msgAreaTop + 1;
	var msgReaderObj = this;

	var msgHasAttachments = msgHdrHasAttachmentFlag(msgHeader);
	// User input loop
	var continueOn = true;
	while (continueOn)
	{
		// Display the message lines (depending on the value of writeMessage)
		// and handle scroll keys via scrollTextLines().  Handle other keypresses
		// here.
		var scrollbarInfoObj = {
			solidBlockLastStartRow: 0,
			numSolidScrollBlocks: 0
		};
		scrollbarInfoObj.solidBlockLastStartRow = solidBlockLastStartRow;
		scrollbarInfoObj.numSolidScrollBlocks = numSolidScrollBlocks;
		var scrollRetObj = scrollTextLines(msgInfo.messageLines, topMsgLineIdx,
									   this.colors.msgBodyColor, writeMessage,
									   this.msgAreaLeft, this.msgAreaTop, msgAreaWidth,
									   msgAreaHeight, 1, console.screen_rows, this.useEnhReaderScrollbar,
									   msgScrollbarUpdateFn, scrollbarInfoObj);
		topMsgLineIdx = scrollRetObj.topLineIdx;
		retObj.lastKeypress = scrollRetObj.lastKeypress;
		switch (retObj.lastKeypress)
		{
			case this.enhReaderKeys.deleteMessage: // Delete message
				var originalCurpos = console.getxy();
				// The 2nd to last row of the screen is where the user will
				// be prompted for confirmation to delete the message.
				// Ideally, I'd like to put the cursor on the last row of
				// the screen for this, but console.noyes() lets the enter
				// key shift everything on screen up one row, and there's
				// no way to avoid that.  So, to optimize screen refreshing,
				// the cursor is placed on the 2nd to the last row on the
				// screen to prompt for confirmation.
				var promptPos = this.EnhReaderPrepLast2LinesForPrompt();

				// Prompt the user for confirmation to delete the message.
				// Note: this.PromptAndDeleteOrUndeleteMessage() will check to see if the user
				// is a sysop or the message was posted by the user.
				// If the message was deleted and the user can't view deleted messages,
				// then exit this read method and return KEY_RIGHT as the last keypress
				// so that the calling method will go to the next message/sub-board.
				// Otherwise (if the message was not deleted), refresh the
				// last 2 lines of the message on the screen.
				var msgWasDeleted = this.PromptAndDeleteOrUndeleteMessage(pOffset, promptPos, true, true, msgAreaWidth, true);
				if (msgWasDeleted && !canViewDeletedMsgs())
				{
					var msgSearchObj = this.LookForNextOrPriorNonDeletedMsg(pOffset);
					continueOn = msgSearchObj.continueInputLoop;
					retObj.newMsgOffset = msgSearchObj.newMsgOffset;
					retObj.nextAction = msgSearchObj.nextAction;
					if (msgSearchObj.promptGoToNextArea)
					{
						if (this.EnhReaderPromptYesNo(replaceAtCodesInStr(this.text.goToNextMsgAreaPromptText), msgInfo.messageLines, topMsgLineIdx, msgLineFormatStr, solidBlockStartRow, numSolidScrollBlocks))
						{
							// Let this method exit and let the caller go to the next sub-board
							continueOn = false;
							retObj.nextAction = ACTION_GO_NEXT_MSG;
						}
						else
							writeMessage = false; // No need to refresh the message
					}
				}
				else
				{
					this.DisplayEnhReaderError("", msgInfo.messageLines, topMsgLineIdx, msgLineFormatStr);
					// Move the cursor back to its original position
					console.gotoxy(originalCurpos);
					writeMessage = false;
				}
				break;
			case this.enhReaderKeys.selectMessage: // Select message (for batch delete, etc.)
				var originalCurpos = console.getxy();
				var promptPos = this.EnhReaderPrepLast2LinesForPrompt();
				if (this.EnhReaderPromptYesNo("Select this message", msgInfo.messageLines, topMsgLineIdx, msgLineFormatStr, solidBlockStartRow, numSolidScrollBlocks, true))
					this.ToggleSelectedMessage(this.subBoardCode, pOffset, true);
				else
					this.ToggleSelectedMessage(this.subBoardCode, pOffset, false);
				writeMessage = false; // No need to refresh the message
				break;
			case this.enhReaderKeys.batchDelete:
				// TODO: Write this?  Not sure yet if it makes much sense to
				// have batch delete in the reader interface.
				// Prompt the user for confirmation, and use
				// this.DeleteOrUndeleteSelectedMessages() to mark the selected messages
				// as deleted.
				// Returns an object with the following properties:
				//  deletedAll: Boolean - Whether or not all messages were successfully marked
				//              for deletion
				//  failureList: An object containing indexes of messages that failed to get
				//               marked for deletion, indexed by internal sub-board code, then
				//               containing messages indexes as properties.  Reasons for failing
				//               to mark messages deleted can include the user not having permission
				//               to delete in a sub-board, failure to open the sub-board, etc.
				writeMessage = false; // No need to refresh the message
				break;
			case this.enhReaderKeys.editMsg: // Edit the messaage
				if (this.CanEdit())
				{
					// Move the cursor to the last line in the message area so
					// the edit confirmation prompt will appear there.  Not using
					// the last line on the screen because the yes/no prompt will
					// output a carriage return and move everything on the screen
					// up one line, which is not ideal in case the user says No.
					var promptPos = this.EnhReaderPrepLast2LinesForPrompt();
					// Let the user edit the message if they want to
					var editReturnObj = this.EditExistingMsg(pOffset);
					// If the user didn't confirm, then we only have to refresh the bottom
					// help line.  Otherwise, we need to refresh everything on the screen.
					if (!editReturnObj.userConfirmed)
					{
						// For some reason, the yes/no prompt erases the last character
						// of the scrollbar - So, figure out which block was there and
						// refresh it.
						//var scrollBarBlock = "\x01n\x01h\x01k" + BLOCK1; // Dim block
						// Dim block
						if (this.useEnhReaderScrollbar)
						{
							var scrollBarBlock = this.colors.scrollbarBGColor + this.text.scrollbarBGChar;
							if (solidBlockStartRow + numSolidScrollBlocks - 1 == this.msgAreaBottom)
							{
								//scrollBarBlock = "\x01w" + BLOCK2; // Bright block
								// Bright block
								scrollBarBlock = this.colors.scrollbarScrollBlockColor + this.text.scrollbarScrollBlockChar;
							}
							else
							{
								// TODO
							}
							console.gotoxy(this.msgAreaRight+1, this.msgAreaBottom);
							console.print(scrollBarBlock);
						}
						// Refresh the last 2 message lines on the screen, then display
						// the key help line
						this.DisplayEnhReaderError("", msgInfo.messageLines, topMsgLineIdx, msgLineFormatStr);
						this.DisplayEnhancedMsgReadHelpLine(console.screen_rows, allowChgMsgArea);
						writeMessage = false;
					}
					else
					{
						// If the message was edited, then refresh the text lines
						// array and update the other message-related variables.
						if (editReturnObj.msgEdited && (editReturnObj.newMsgIdx > -1))
						{
							// When the message is edited, the old message will be
							// deleted and the edited message will be posted as a new
							// message.  So we should return to the caller and have it
							// go directly to that new message.
							this.DisplayEnhancedMsgReadHelpLine(console.screen_rows, allowChgMsgArea);
							continueOn = false;
							retObj.newMsgOffset = editReturnObj.newMsgIdx;
						}
						else
						{
							// The message was not edited.  Refresh everything on the screen.
							// If the enhanced message header width is less than the console
							// width, then clear the screen to remove anything that might be
							// left on the screen by the message editor.
							if (this.enhMsgHeaderWidth < console.screen_columns)
								console.clear("\x01n");
							// Display the message header and key help line
							this.DisplayEnhancedMsgHdr(msgHeader, pOffset+1, 1);
							this.DisplayEnhancedMsgReadHelpLine(console.screen_rows, allowChgMsgArea);
							// Display the scrollbar again, and ensure it's in the correct position
							if (this.useEnhReaderScrollbar)
							{
								solidBlockStartRow = this.msgAreaTop + Math.floor(numNonSolidScrollBlocks * fractionToLastPage);
								this.DisplayEnhancedReaderWholeScrollbar(solidBlockStartRow, numSolidScrollBlocks);
							}
							else
							{
								// TODO
							}
							writeMessage = true; // We want to refresh the message on the screen
						}
					}
				}
				else
					writeMessage = false; // Don't write the current message again
				break;
			case this.enhReaderKeys.showHelp: // Show the help screen
				this.DisplayEnhancedReaderHelp(allowChgMsgArea, msgHasAttachments);
				// If the enhanced message header width is less than the console
				// width, then clear the screen to remove anything left on the
				// screen from the help screen.
				if (this.enhMsgHeaderWidth < console.screen_columns)
					console.clear("\x01n");
				// Display the message header and key help line
				this.DisplayEnhancedMsgHdr(msgHeader, pOffset+1, 1);
				this.DisplayEnhancedMsgReadHelpLine(console.screen_rows, allowChgMsgArea);
				// Display the scrollbar again, and ensure it's in the correct position
				if (this.useEnhReaderScrollbar)
				{
					solidBlockStartRow = this.msgAreaTop + Math.floor(numNonSolidScrollBlocks * fractionToLastPage);
					this.DisplayEnhancedReaderWholeScrollbar(solidBlockStartRow, numSolidScrollBlocks);
				}
				else
				{
					// TODO
				}
				writeMessage = true; // We want to refresh the message on the screen
				break;
			case this.enhReaderKeys.reply: // Reply to the message
			case this.enhReaderKeys.privateReply: // Private message reply
				// If the user pressed the private reply key while reading private
				// mail, then do nothing (allow only the regular reply key to reply).
				var privateReply = (retObj.lastKeypress == this.enhReaderKeys.privateReply);
				if (privateReply && this.readingPersonalEmail)
					writeMessage = false; // Don't re-write the current message again
				else
				{
					// Get the message header with fields expanded so we can get the most info possible.
					//var extdMsgHdr = this.GetMsgHdrByAbsoluteNum(msgHeader.number, true);
					var msgbase = new MsgBase(this.subBoardCode);
					if (msgbase.open())
					{
						var extdMsgHdr = msgbase.get_msg_header(false, msgHeader.number, true);
						msgbase.close();
						// Let the user reply to the message.
						var replyRetObj = this.ReplyToMsg(extdMsgHdr, messageText, privateReply, pOffset);
						retObj.userReplied = replyRetObj.postSucceeded;
						//retObj.msgNotReadable = replyRetObj.msgWasDeleted;
						var msgWasDeleted = replyRetObj.msgWasDeleted;
						//if (retObj.msgNotReadable)
						if (msgWasDeleted && !canViewDeletedMsgs())
						{
							var msgSearchObj = this.LookForNextOrPriorNonDeletedMsg(pOffset);
							continueOn = msgSearchObj.continueInputLoop;
							retObj.newMsgOffset = msgSearchObj.newMsgOffset;
							retObj.nextAction = msgSearchObj.nextAction;
							if (msgSearchObj.promptGoToNextArea)
							{
								if (this.EnhReaderPromptYesNo(replaceAtCodesInStr(this.text.goToNextMsgAreaPromptText), msgInfo.messageLines, topMsgLineIdx, msgLineFormatStr, solidBlockStartRow, numSolidScrollBlocks))
								{
									// Let this method exit and let the caller go to the next sub-board
									continueOn = false;
									retObj.nextAction = ACTION_GO_NEXT_MSG;
								}
								else
									writeMessage = true; // We want to refresh the message on the screen
							}
						}
						else
						{
							// If the enhanced message header width is less than the console
							// width, then clear the screen to remove anything left on the
							// screen by the message editor.
							if (this.enhMsgHeaderWidth < console.screen_columns)
								console.clear("\x01n");
							// Display the message header and key help line again
							this.DisplayEnhancedMsgHdr(msgHeader, pOffset+1, 1);
							this.DisplayEnhancedMsgReadHelpLine(console.screen_rows, allowChgMsgArea);
							// Display the scrollbar again to refresh it on the screen
							if (this.useEnhReaderScrollbar)
							{
								solidBlockStartRow = this.msgAreaTop + Math.floor(numNonSolidScrollBlocks * fractionToLastPage);
								this.DisplayEnhancedReaderWholeScrollbar(solidBlockStartRow, numSolidScrollBlocks);
							}
							else
							{
								// TODO
							}
							writeMessage = true; // We want to refresh the message on the screen
						}
					}
				}
				break;
			case this.enhReaderKeys.postMsg: // Post a message
				if (!this.readingPersonalEmail)
				{
					// Let the user post a message.
					if (bbs.post_msg(this.subBoardCode))
					{
						if (searchTypePopulatesSearchResults(this.searchType))
						{
							// TODO: If the user is doing a search, it might be
							// useful to search their new message and add it to
							// the search results if it's a match..  but maybe
							// not?
						}
						console.pause();
					}

					// Refresh things on the screen
					// Display the message header and key help line again
					this.DisplayEnhancedMsgHdr(msgHeader, pOffset+1, 1);
					this.DisplayEnhancedMsgReadHelpLine(console.screen_rows, allowChgMsgArea);
					// Display the scrollbar again to refresh it on the screen
					if (this.useEnhReaderScrollbar)
					{
						solidBlockStartRow = this.msgAreaTop + Math.floor(numNonSolidScrollBlocks * fractionToLastPage);
						this.DisplayEnhancedReaderWholeScrollbar(solidBlockStartRow, numSolidScrollBlocks);
					}
					else
					{
						// TODO
					}
					writeMessage = true; // We want to refresh the message on the screen
				}
				else
					writeMessage = false; // Don't re-write the current message again
				break;
			// Numeric digit: The start of a number of a message to read
			case "0":
			case "1":
			case "2":
			case "3":
			case "4":
			case "5":
			case "6":
			case "7":
			case "8":
			case "9":
				var originalCurpos = console.getxy();
				// Put the user's input back in the input buffer to
				// be used for getting the rest of the message number.
				console.ungetstr(retObj.lastKeypress);
				// Move the cursor to the 2nd to last row of the screen and
				// prompt the user for the message number.  Ideally, I'd like
				// to put the cursor on the last row of the screen for this, but
				// console.getnum() lets the enter key shift everything on screen
				// up one row, and there's no way to avoid that.  So, to optimize
				// screen refreshing, the cursor is placed on the 2nd to the last
				// row on the screen to prompt for the message number.
				var promptPos = this.EnhReaderPrepLast2LinesForPrompt();
				// Prompt for the message number
				var msgNumInput = this.PromptForMsgNum(promptPos, replaceAtCodesInStr(this.text.readMsgNumPromptText), false, ERROR_PAUSE_WAIT_MS, false);
				// Only allow reading the message if the message number is valid
				// and it's not the same message number that was passed in.
				if ((msgNumInput > 0) && (msgNumInput-1 != pOffset))
				{
					// If the message is marked as deleted, then output an error
					if (this.MessageIsDeleted(msgNumInput-1))
					{
						writeWithPause(this.msgAreaLeft, console.screen_rows-1,
									   "\x01n" + replaceAtCodesInStr(format(this.text.msgHasBeenDeletedText, msgNumInput)) + "\x01n",
									   ERROR_PAUSE_WAIT_MS, "\x01n", true);
					}
					else
					{
						// Confirm with the user whether to read the message
						var readMsg = true;
						if (this.promptToReadMessage)
						{
							var sReadMsgConfirmText = this.colors["readMsgConfirmColor"]
													+ "Read message "
													+ this.colors["readMsgConfirmNumberColor"]
													+ msgNumInput + this.colors["readMsgConfirmColor"]
													+ ": Are you sure";
							console.gotoxy(promptPos);
							console.print("\x01n");
							readMsg = console.yesno(sReadMsgConfirmText);
						}
						if (readMsg)
						{
							continueOn = false;
							retObj.newMsgOffset = msgNumInput - 1;
							retObj.nextAction = ACTION_GO_SPECIFIC_MSG;
						}
						else
							writeMessage = false; // Don't re-write the current message again
					}
				}
				else // Message number invalid or the same as what was passed in
					writeMessage = false; // Don't re-write the current message again

				// If the user chose to continue reading messages, then refresh
				// the last 2 message lines in the last part of the message area
				// and then put the cursor back to its original position.
				if (continueOn)
				{
					this.DisplayEnhReaderError("", msgInfo.messageLines, topMsgLineIdx, msgLineFormatStr);
					// Move the cursor back to its original position
					console.gotoxy(originalCurpos);
				}
				break;
			case this.enhReaderKeys.prevMsgByTitle: // Previous message by title
			case this.enhReaderKeys.prevMsgByAuthor: // Previous message by author
			case this.enhReaderKeys.prevMsgByToUser: // Previous message by 'to user'
			case this.enhReaderKeys.prevMsgByThreadID: // Previous message by thread ID
				// Only allow this if we aren't doing a message search.
				if (!this.SearchingAndResultObjsDefinedForCurSub())
				{
					var threadPrevMsgOffset = this.FindThreadPrevOffset(msgHeader,
					                                                    keypressToThreadType(retObj.lastKeypress, this.enhReaderKeys),
					                                                    true);
					if (threadPrevMsgOffset > -1)
					{
						retObj.newMsgOffset = threadPrevMsgOffset;
						retObj.nextAction = ACTION_GO_SPECIFIC_MSG;
						continueOn = false;
					}
					else
					{
						// Refresh the help line at the bottom of the screen
						//this.DisplayEnhancedMsgReadHelpLine(console.screen_rows, allowChgMsgArea);
						writeMessage = false; // Don't re-write the current message again
					}
					// Make sure the help line on the bottom of the screen is
					// drawn.
					this.DisplayEnhancedMsgReadHelpLine(console.screen_rows, allowChgMsgArea);
				}
				else
					writeMessage = false; // Don't re-write the current message again
				break;
			case this.enhReaderKeys.nextMsgByTitle: // Next message by title (subject)
			case this.enhReaderKeys.nextMsgByAuthor: // Next message by author
			case this.enhReaderKeys.nextMsgByToUser: // Next message by 'to user'
			case this.enhReaderKeys.nextMsgByThreadID: // Next message by thread ID
				// Only allow this if we aren't doing a message search.
				if (!this.SearchingAndResultObjsDefinedForCurSub())
				{
					var threadPrevMsgOffset = this.FindThreadNextOffset(msgHeader,
					                                                    keypressToThreadType(retObj.lastKeypress, this.enhReaderKeys),
					                                                    true);
					if (threadPrevMsgOffset > -1)
					{
						retObj.newMsgOffset = threadPrevMsgOffset;
						retObj.nextAction = ACTION_GO_SPECIFIC_MSG;
						continueOn = false;
					}
					else
						writeMessage = false; // Don't re-write the current message again
					// Make sure the help line on the bottom of the screen is
					// drawn.
					this.DisplayEnhancedMsgReadHelpLine(console.screen_rows, allowChgMsgArea);
				}
				else
					writeMessage = false; // Don't re-write the current message again
				break;
			case this.enhReaderKeys.previousMsg: // Previous message
				// Look for a prior message that isn't marked for deletion.  Even
				// if we don't find one, we'll still want to return from this
				// function (with message index -1) so that this script can go
				// onto the previous message sub-board/group.
				retObj.newMsgOffset = this.FindNextNonDeletedMsgIdx(pOffset, false);
				// As a screen redraw optimization: Only return if there is a valid new
				// message offset or the user is allowed to change to a different sub-board.
				// Otherwise, don't return, and don't refresh the message on the screen.
				var goToPrevMessage = false;
				if ((retObj.newMsgOffset > -1) || allowChgMsgArea)
				{
					if (retObj.newMsgOffset == -1 && !curMsgSubBoardIsLast())
					{
						goToPrevMessage = this.EnhReaderPromptYesNo(replaceAtCodesInStr(this.text.goToPrevMsgAreaPromptText),
																	msgInfo.messageLines, topMsgLineIdx,
																	msgLineFormatStr, solidBlockStartRow,
																	numSolidScrollBlocks);
					}
					else
					{
						// We're not at the beginning of the sub-board, so it's okay to exit this
						// method and go to the previous message.
						goToPrevMessage = true;
					}
				}
				if (goToPrevMessage)
				{
					continueOn = false;
					retObj.nextAction = ACTION_GO_PREVIOUS_MSG;
				}
				else
					writeMessage = false; // No need to refresh the message
				break;
			case this.enhReaderKeys.nextMsg: // Next message
			case KEY_ENTER:
				// Look for a later message that isn't marked for deletion.  Even
				// if we don't find one, we'll still want to return from this
				// function (with message index -1) so that this script can go
				// onto the next message sub-board/group.
				var findNextMsgRetObj = this.ScrollableReaderNextReadableMessage(pOffset, msgInfo, topMsgLineIdx, msgLineFormatStr, solidBlockStartRow, numSolidScrollBlocks);
				if (findNextMsgRetObj.newMsgOffset > -1)
					retObj.newMsgOffset = findNextMsgRetObj.newMsgOffset;
				writeMessage = findNextMsgRetObj.writeMessage;
				continueOn = findNextMsgRetObj.continueOn;
				retObj.nextAction = findNextMsgRetObj.nextAction;
				break;
				// First & last message: Quit out of this input loop and let the
				// calling function, this.ReadMessages(), handle the action.
			case this.enhReaderKeys.firstMsg: // First message
				// Only leave this function if we aren't already on the first message.
				if (pOffset > 0)
				{
					continueOn = false;
					retObj.nextAction = ACTION_GO_FIRST_MSG;
				}
				else
					writeMessage = false; // Don't re-write the current message again
				break;
			case this.enhReaderKeys.lastMsg: // Last message
				// Only leave this function if we aren't already on the last message.
				if (pOffset < this.NumMessages() - 1)
				{
					continueOn = false;
					retObj.nextAction = ACTION_GO_LAST_MSG;
				}
				else
					writeMessage = false; // Don't re-write the current message again
				break;
			case this.enhReaderKeys.prevSubBoard: // Go to the previous message area
				if (allowChgMsgArea)
				{
					continueOn = false;
					retObj.nextAction = ACTION_GO_PREV_MSG_AREA;
				}
				else
					writeMessage = false; // Don't re-write the current message again
				break;
			case this.enhReaderKeys.nextSubBoard: // Go to the next message area
				if (allowChgMsgArea || this.doingMultiSubBoardScan)
				{
					continueOn = false;
					retObj.nextAction = ACTION_GO_NEXT_MSG_AREA;
				}
				else
					writeMessage = false; // Don't re-write the current message again
				break;
				// H and K: Display the extended message header info/kludge lines
				// (for the sysop)
			case this.enhReaderKeys.showHdrInfo:
			case this.enhReaderKeys.showKludgeLines:
				if (user.is_sysop)
				{
					// Save the original cursor position
					var originalCurPos = console.getxy();

					// Get an array of the extended header info/kludge lines and then
					// allow the user to scroll through them.
					var extdHdrInfoLines = this.GetExtdMsgHdrInfo(msgHeader, (retObj.lastKeypress == this.enhReaderKeys.showKludgeLines));
					if (extdHdrInfoLines.length > 0)
					{
						if (this.useEnhReaderScrollbar)
						{
							// Calculate information for the scrollbar for the kludge lines
							var infoFractionShown = this.msgAreaHeight / extdHdrInfoLines.length;
							if (infoFractionShown > 1)
								infoFractionShown = 1.0;
							var numInfoSolidScrollBlocks = Math.floor(this.msgAreaHeight * infoFractionShown);
							if (numInfoSolidScrollBlocks == 0)
								numInfoSolidScrollBlocks = 1;
							var numNonSolidInfoScrollBlocks = this.msgAreaHeight - numInfoSolidScrollBlocks;
							var lastInfoSolidBlockStartRow = this.msgAreaTop;
							// Display the kludge lines and let the user scroll through them
							this.DisplayEnhancedReaderWholeScrollbar(this.msgAreaTop, numInfoSolidScrollBlocks);
						}
						scrollTextLines(extdHdrInfoLines, 0, this.colors["msgBodyColor"], true, this.msgAreaLeft,
						                this.msgAreaTop, msgAreaWidth, msgAreaHeight, 1, console.screen_rows,
						                this.useEnhReaderScrollbar, msgInfoScrollbarUpdateFn);
						// Display the scrollbar for the message to refresh it on the screen
						if (this.useEnhReaderScrollbar)
						{
							solidBlockStartRow = this.msgAreaTop + Math.floor(numNonSolidScrollBlocks * fractionToLastPage);
							this.DisplayEnhancedReaderWholeScrollbar(solidBlockStartRow, numSolidScrollBlocks);
						}
						else
						{
							// TODO
						}
						writeMessage = true; // We want to refresh the message on the screen
					}
					else
					{
						// There are no kludge lines for this message
						this.DisplayEnhReaderError(replaceAtCodesInStr(this.text.noKludgeLinesForThisMsgText), msgInfo.messageLines, topMsgLineIdx, msgLineFormatStr);
						console.gotoxy(originalCurPos);
						writeMessage = false;
					}
				}
				else // The user is not a sysop
					writeMessage = false;
				break;
				// Message list, change message area: Quit out of this input loop
				// and let the calling function, this.ReadMessages(), handle the
				// action.
			case this.enhReaderKeys.showMsgList: // Message list
				console.print("\x01n");
				console.crlf();
				console.print("Loading...");
				retObj.nextAction = ACTION_DISPLAY_MSG_LIST;
				continueOn = false;
				break;
			case this.enhReaderKeys.chgMsgArea: // Change message area, if allowed
				if (allowChgMsgArea)
				{
					retObj.nextAction = ACTION_CHG_MSG_AREA;
					continueOn = false;
				}
				else
					writeMessage = false; // No need to refresh the message
				break;
			case this.enhReaderKeys.downloadAttachments: // Download attachments
				if (msgHasAttachments)
				{
					console.print("\x01n");
					console.gotoxy(1, console.screen_rows);
					console.crlf();
					allowUserToDownloadMessage_NewInterface(msgHeader, this.subBoardCode);

					// Refresh things on the screen
					console.clear("\x01n");
					// Display the message header and key help line again
					this.DisplayEnhancedMsgHdr(msgHeader, pOffset+1, 1);
					this.DisplayEnhancedMsgReadHelpLine(console.screen_rows, allowChgMsgArea);
					// Display the scrollbar again to refresh it on the screen
					if (this.useEnhReaderScrollbar)
					{
						solidBlockStartRow = this.msgAreaTop + Math.floor(numNonSolidScrollBlocks * fractionToLastPage);
						this.DisplayEnhancedReaderWholeScrollbar(solidBlockStartRow, numSolidScrollBlocks);
					}
					else
					{
						// TODO
					}
					writeMessage = true; // We want to refresh the message on the screen
				}
				else
					writeMessage = false;
				break;
			case this.enhReaderKeys.saveToBBSMachine:
				// Save the message to the BBS machine - Only allow this
				// if the user is a sysop.
				if (user.is_sysop)
				{
					// Prompt the user for a filename to save the message to the
					// BBS machine
					var promptPos = this.EnhReaderPrepLast2LinesForPrompt();
					console.print("\x01n\x01cFilename:\x01h");
					var inputLen = console.screen_columns - 10; // 10 = "Filename:" length + 1
					var filename = console.getstr(inputLen, K_NOCRLF);
					console.print("\x01n");
					if (filename.length > 0)
					{
						var saveMsgRetObj = this.SaveMsgToFile(msgHeader, filename);
						console.gotoxy(promptPos);
						console.cleartoeol("\x01n");
						console.gotoxy(promptPos);
						if (saveMsgRetObj.succeeded)
						{
							var statusMsg = "\x01n\x01cThe message has been saved.";
							if (msgHdrHasAttachmentFlag(msgHeader))
								statusMsg += " Attachments not saved.";
							statusMsg += "\x01n";
							console.print(statusMsg);
						}
						else
							console.print("\x01n\x01y\x01hFailed: " + saveMsgRetObj.errorMsg + "\x01n");
						mswait(ERROR_PAUSE_WAIT_MS);
					}
					else
					{
						console.gotoxy(promptPos);
						console.print("\x01n\x01y\x01hMessage not exported\x01n");
						mswait(ERROR_PAUSE_WAIT_MS);
					}
					// Refresh the last 2 lines of the message on the screen to overwrite
					// the file save prompt
					this.DisplayEnhReaderError("", msgInfo.messageLines, topMsgLineIdx, msgLineFormatStr);
				}
				writeMessage = false; // Don't write the whole message again
				break;
			case this.enhReaderKeys.userEdit: // Edit the user who wrote the message
				if (user.is_sysop)
				{
					console.print("\x01n");
					console.crlf();
					console.print("- Edit user " + msgHeader.from);
					console.crlf();
					var editObj = editUser(msgHeader.from);
					if (editObj.errorMsg.length != 0)
					{
						console.print("\x01n");
						console.crlf();
						console.print("\x01y\x01h" + editObj.errorMsg + "\x01n");
						console.crlf();
						console.pause();
					}

					// Refresh things on the screen
					console.clear("\x01n");
					// Display the message header and key help line again
					this.DisplayEnhancedMsgHdr(msgHeader, pOffset+1, 1);
					this.DisplayEnhancedMsgReadHelpLine(console.screen_rows, allowChgMsgArea);
					// Display the scrollbar again to refresh it on the screen
					if (this.useEnhReaderScrollbar)
					{
						solidBlockStartRow = this.msgAreaTop + Math.floor(numNonSolidScrollBlocks * fractionToLastPage);
						this.DisplayEnhancedReaderWholeScrollbar(solidBlockStartRow, numSolidScrollBlocks);
					}
					else
					{
						// TODO
					}
					writeMessage = true; // We want to refresh the message on the screen
				}
				else // The user is not a sysop
					writeMessage = false;
				break;
			case this.enhReaderKeys.forwardMsg: // Forward the message
				console.print("\x01n");
				console.crlf();
				console.print("\x01c- Forward message\x01n");
				console.crlf();
				var retStr = this.ForwardMessage(msgHeader, messageText);
				if (retStr.length > 0)
				{
					console.print("\x01n\x01h\x01y* " + retStr + "\x01n");
					console.crlf();
					console.pause();
				}

				// Refresh things on the screen
				console.clear("\x01n");
				// Display the message header and key help line again
				this.DisplayEnhancedMsgHdr(msgHeader, pOffset+1, 1);
				this.DisplayEnhancedMsgReadHelpLine(console.screen_rows, allowChgMsgArea);
				// Display the scrollbar again to refresh it on the screen
				if (this.useEnhReaderScrollbar)
				{
					solidBlockStartRow = this.msgAreaTop + Math.floor(numNonSolidScrollBlocks * fractionToLastPage);
					this.DisplayEnhancedReaderWholeScrollbar(solidBlockStartRow, numSolidScrollBlocks);
				}
				else
				{
					// TODO
				}
				writeMessage = true; // We want to refresh the message on the screen
				break;
			case this.enhReaderKeys.vote: // Vote on the message
				// Move the cursor to the last line in the message area so the
				// vote question text prompt will appear there.
				var promptPos = this.EnhReaderPrepLast2LinesForPrompt();
				console.gotoxy(1, console.screen_rows-1);
				// Let the user vote on the message
				var voteRetObj = this.VoteOnMessage(msgHeader, true);
				if (voteRetObj.BBSHasVoteFunction)
				{
					var msgIsPollVote = ((typeof(MSG_TYPE_POLL) != "undefined") && (msgHeader.type & MSG_TYPE_POLL) == MSG_TYPE_POLL);
					if (!voteRetObj.userQuit)
					{
						// If the message is a poll vote, then output any error
						// message on its own line and refresh the whole screen.
						// Otherwise, use the last 2 rows for an error message
						// and only refresh what's necessary.
						if (msgIsPollVote)
						{
							console.print("\x01n");
							console.gotoxy(1, console.screen_rows-1);
							if (voteRetObj.errorMsg.length > 0)
							{
								if (voteRetObj.mnemonicsRequiredForErrorMsg)
								{
									console.mnemonics(voteRetObj.errorMsg);
									console.print("\x01n");
								}
								else
									console.print("\x01y\x01h* " + voteRetObj.errorMsg + "\x01n");
								mswait(ERROR_PAUSE_WAIT_MS);
							}
							else if (!voteRetObj.savedVote)
							{
								console.print("\x01y\x01h* Failed to save the vote\x01n");
								mswait(ERROR_PAUSE_WAIT_MS);
							}
						}
						else
						{
							// Not a poll vote - Just an up/down vote
							if ((voteRetObj.errorMsg.length > 0) || (!voteRetObj.savedVote))
							{
								console.print("\x01n");
								console.gotoxy(1, console.screen_rows-1);
								if (voteRetObj.errorMsg.length > 0)
								{
									if (voteRetObj.mnemonicsRequiredForErrorMsg)
									{
										console.mnemonics(voteRetObj.errorMsg);
										console.print("\x01n");
									}
									else
										console.print("\x01y\x01h* " + voteRetObj.errorMsg + "\x01n");
								}
								else if (!voteRetObj.savedVote)
									console.print("\x01y\x01h* Failed to save the vote\x01n");
							}
							else
								msgHeader = voteRetObj.updatedHdr; // To get updated vote information
							mswait(ERROR_PAUSE_WAIT_MS);

							writeMessage = false;
						}
						// Exit out of the reader and come back to read
						// the same message again so that the voting results
						// are re-loaded and displayed on the screen.
						retObj.newMsgOffset = pOffset;
						retObj.nextAction = ACTION_GO_SPECIFIC_MSG;
						continueOn = false;
						this.DisplayEnhancedMsgReadHelpLine(console.screen_rows, allowChgMsgArea);
					}
					else
					{
						// The user quit out of voting.  Refresh the screen.
						// Exit out of the reader and come back to read
						// the same message again so that the screen is refreshed.
						retObj.newMsgOffset = pOffset;
						retObj.nextAction = ACTION_GO_SPECIFIC_MSG;
						continueOn = false;
						this.DisplayEnhancedMsgReadHelpLine(console.screen_rows, allowChgMsgArea);
					}
				}
				else
					writeMessage = false;
				break;
			case this.enhReaderKeys.showVotes: // Show votes
				// Save the original cursor position
				var originalCurPos = console.getxy();
				if (msgHeader.hasOwnProperty("total_votes") && msgHeader.hasOwnProperty("upvotes"))
				{
					var voteInfo = this.GetUpvoteAndDownvoteInfo(msgHeader);
					// Display the vote info and let the user scroll through them
					// (the console height should be enough, but do this just in case)
					// Calculate information for the scrollbar for the vote info lines
					if (this.useEnhReaderScrollbar)
					{
						var infoFractionShown = this.msgAreaHeight / voteInfo.length;
						if (infoFractionShown > 1)
							infoFractionShown = 1.0;
						var numInfoSolidScrollBlocks = Math.floor(this.msgAreaHeight * infoFractionShown);
						if (numInfoSolidScrollBlocks == 0)
							numInfoSolidScrollBlocks = 1;
						var numNonSolidInfoScrollBlocks = this.msgAreaHeight - numInfoSolidScrollBlocks;
						var lastInfoSolidBlockStartRow = this.msgAreaTop;
						// Display the vote info lines and let the user scroll through them
						this.DisplayEnhancedReaderWholeScrollbar(this.msgAreaTop, numInfoSolidScrollBlocks);
					}
					else
					{
						// TODO
					}
					scrollTextLines(voteInfo, 0, this.colors["msgBodyColor"], true, this.msgAreaLeft, this.msgAreaTop, msgAreaWidth,
					                msgAreaHeight, 1, console.screen_rows, this.useEnhReaderScrollbar, msgInfoScrollbarUpdateFn);
					// Display the scrollbar for the message to refresh it on the screen
					if (this.useEnhReaderScrollbar)
					{
						solidBlockStartRow = this.msgAreaTop + Math.floor(numNonSolidScrollBlocks * fractionToLastPage);
						this.DisplayEnhancedReaderWholeScrollbar(solidBlockStartRow, numSolidScrollBlocks);
					}
					else
					{
						// TODO
					}
					writeMessage = true; // We want to refresh the message on the screen
				}
				else
				{
					this.DisplayEnhReaderError("There is no voting information for this message", msgInfo.messageLines, topMsgLineIdx, msgLineFormatStr);
					console.gotoxy(originalCurPos);
					writeMessage = false;
				}
				break;
			case this.enhReaderKeys.closePoll: // Close a poll message
				// Save the original cursor position
				var originalCurPos = console.getxy();
				var pollCloseMsg = "";
				// If this message is a poll, then allow closing it.
				if ((typeof(MSG_TYPE_POLL) != "undefined") && (msgHeader.type & MSG_TYPE_POLL) == MSG_TYPE_POLL)
				{
					if ((msgHeader.auxattr & POLL_CLOSED) == 0)
					{
						// Only let the user close the poll if they created it
						if (userHandleAliasNameMatch(msgHeader.from))
						{
							// Prompt to confirm whether the user wants to close the poll
							console.gotoxy(1, console.screen_rows-1);
							printf("\x01n%" + +(console.screen_columns-1) + "s", "");
							console.gotoxy(1, console.screen_rows-1);
							if (!console.noyes("Close poll"))
							{
								// Close the poll (open the sub-board first)
								var msgbase = new MsgBase(this.subBoardCode);
								if (msgbase.open())
								{
									if (closePollWithOpenMsgbase(msgbase, msgHeader.number))
									{
										msgHeader.auxattr |= POLL_CLOSED;
										pollCloseMsg = "\x01n\x01cThis poll was successfully closed.";
									}
									else
										pollCloseMsg = "\x01n\x01r\x01h* Failed to close this poll!";
									msgbase.close();
								}
								else
									pollCloseMsg = "\x01n\x01y\x01hUnable to open sub-board to close the poll";
							}
						}
						else
							pollCloseMsg = "\x01n\x01y\x01hCan't close this poll because it's not yours";
					}
					else
						pollCloseMsg = "\x01n\x01y\x01hThis poll is already closed";
				}
				else
					pollCloseMsg = "This message is not a poll";

				// Display the poll closing status message
				this.DisplayEnhReaderError(pollCloseMsg, msgInfo.messageLines, topMsgLineIdx, msgLineFormatStr);
				console.gotoxy(originalCurPos);
				writeMessage = false;
				break;
			case this.enhReaderKeys.validateMsg: // Validate the message
				if (user.is_sysop && (this.subBoardCode != "mail") && msg_area.sub[this.subBoardCode].is_moderated)
				{
					var message = "";
					if (this.ValidateMsg(this.subBoardCode, msgHeader.number))
					{
						message = "\x01n\x01cMessage validation successful";
						// Refresh the message header in the arrays
						this.RefreshMsgHdrInArrays(msgHeader.number);
						// Exit out of the reader and come back to read
						// the same message again so that the voting results
						// are re-loaded and displayed on the screen.
						retObj.newMsgOffset = pOffset;
						retObj.nextAction = ACTION_GO_SPECIFIC_MSG;
						continueOn = false;
						this.DisplayEnhancedMsgReadHelpLine(console.screen_rows, allowChgMsgArea);
					}
					else
						message = "\x01n\x01y\x01hMessage validation failed!";
					this.DisplayEnhReaderError(message, msgInfo.messageLines, topMsgLineIdx, msgLineFormatStr);
				}
				else
					writeMessage = false;
				break;
			case this.enhReaderKeys.bypassSubBoardInNewScan:
				writeMessage = false; // TODO: Finish
				/*
				if (this.doingMsgScan)
				{
					console.print("\x01n");
					var originalCurpos = console.getxy();
					// The 2nd to last row of the screen is where the user will
					// be prompted for confirmation to delete the message.
					// Ideally, I'd like to put the cursor on the last row of
					// the screen for this, but console.noyes() lets the enter
					// key shift everything on screen up one row, and there's
					// no way to avoid that.  So, to optimize screen refreshing,
					// the cursor is placed on the 2nd to the last row on the
					// screen to prompt for confirmation.
					var promptPos = this.EnhReaderPrepLast2LinesForPrompt();
					if (!console.noyes("Bypass this sub-board in newscans"))
					{
						continueOn = false;
						msg_area.sub[this.subBoardCode].scan_cfg &= SCAN_CFG_NEW;
					}
					else
					{
						this.DisplayEnhReaderError("", msgInfo.messageLines, topMsgLineIdx, msgLineFormatStr);
						// Move the cursor back to its original position
						console.gotoxy(originalCurpos);
						this.DisplayEnhancedMsgReadHelpLine(console.screen_rows, allowChgMsgArea);
					}
				}
				else
					writeMessage = false;
				*/
				break;
			case this.enhReaderKeys.userSettings:
				// Make a backup copy of the this.useEnhReaderScrollbar setting in case it changes, so we can tell if we need to refresh the scrollbar
				var oldUseEnhReaderScrollbar = this.useEnhReaderScrollbar;
				var userSettingsRetObj = this.DoUserSettings_Scrollable(function(pReader) { pReader.DisplayEnhancedMsgReadHelpLine(console.screen_rows, allowChgMsgArea); });
				retObj.lastKeypress = "";
				writeMessage = userSettingsRetObj.needWholeScreenRefresh;
				// In case the user changed their twitlist, re-filter the messages for this sub-board
				if (userSettingsRetObj.userTwitListChanged)
				{
					console.gotoxy(1, console.screen_rows);
					console.crlf();
					console.print("\x01nTwitlist changed; re-filtering..");
					var tmpMsgbase = new MsgBase(this.subBoardCode);
					if (tmpMsgbase.open())
					{
						continueOn = false;
						writeMessage = false;
						var tmpAllMsgHdrs = tmpMsgbase.get_all_msg_headers(true);
						tmpMsgbase.close();
						this.FilterMsgHdrsIntoHdrsForCurrentSubBoard(tmpAllMsgHdrs, true);
						// If the user is currently reading a message a message by someone who is now
						// in their twit list, change the message currently being viewed.
						if (this.MsgHdrFromOrToInUserTwitlist(msgHeader))
						{
							var findNextMsgRetObj = this.ScrollableReaderNextReadableMessage(pOffset, msgInfo, topMsgLineIdx, msgLineFormatStr, solidBlockStartRow, numSolidScrollBlocks);
							if (findNextMsgRetObj.newMsgOffset > -1)
							{
								retObj.newMsgOffset = findNextMsgRetObj.newMsgOffset;
								retObj.nextAction = ACTION_GO_SPECIFIC_MSG;
							}
							else
								retObj.nextAction = ACTION_GO_NEXT_MSG_AREA;
						}
						else
						{
							// If there are still messages in this sub-board, and the message offset is beyond the last
							// message, then show the last message in the sub-board.  Otherwise, go to the next message area.
							if (this.hdrsForCurrentSubBoard.length > 0)
							{
								if (pOffset > this.hdrsForCurrentSubBoard.length)
								{
									//this.hdrsForCurrentSubBoard[this.hdrsForCurrentSubBoard.length-1].number
									retObj.newMsgOffset = this.hdrsForCurrentSubBoard.length - 1;
									retObj.nextAction = ACTION_GO_SPECIFIC_MSG;
								}
							}
							else
								retObj.nextAction = ACTION_GO_NEXT_MSG_AREA;
						}
					}
					else
						console.print("\x01y\x01hFailed to open the messagbase!\x01\r\n\x01p");
					this.SetUpLightbarMsgListVars();
					writeMessage = true;
				}
				if (userSettingsRetObj.needWholeScreenRefresh)
				{
					this.DisplayEnhancedMsgHdr(msgHeader, pOffset+1, 1);
					if (this.useEnhReaderScrollbar)
						this.DisplayEnhancedReaderWholeScrollbar(solidBlockStartRow, numSolidScrollBlocks);
					else
					{
						// TODO
					}
					this.DisplayEnhancedMsgReadHelpLine(console.screen_rows, allowChgMsgArea);
				}
				else
				{
					// If the message scrollbar was toggled, then draw/erase the scrollbar
					if (this.useEnhReaderScrollbar != oldUseEnhReaderScrollbar)
					{
						msgAreaWidth = this.useEnhReaderScrollbar ? this.msgAreaWidth : this.msgAreaWidth + 1;
						// If the message is ANSI, then re-create the Graphic object to account for the
						// new width
						if (msgHasANSICodes)
						{
							var graphic = new Graphic(msgAreaWidth, this.msgAreaHeight-1);
							graphic.auto_extend = true;
							graphic.ANSI = ansiterm.expand_ctrl_a(messageText);
							graphic.width = msgAreaWidth;
							messageText = graphic.MSG;
						}
						// Display or erase the scrollbar
						if (this.useEnhReaderScrollbar)
						{
							solidBlockStartRow = this.msgAreaTop + Math.floor(numNonSolidScrollBlocks * fractionToLastPage);
							this.DisplayEnhancedReaderWholeScrollbar(solidBlockStartRow, numSolidScrollBlocks);
						}
						else
						{
							// Erase the scrollbar
							console.attributes = "N";
							for (var screenY = this.msgAreaTop; screenY <= this.msgAreaBottom; ++screenY)
							{
								console.gotoxy(this.msgAreaRight+1, screenY);
								console.print(" ");
							}
						}
					}
					this.RefreshMsgAreaRectangle(msgInfo.messageLines, topMsgLineIdx, userSettingsRetObj.optionBoxTopLeftX, userSettingsRetObj.optionBoxTopLeftY, userSettingsRetObj.optionBoxWidth, userSettingsRetObj.optionBoxHeight);
				}
				break;
			case this.enhReaderKeys.quit: // Quit
				retObj.nextAction = ACTION_QUIT;
				continueOn = false;
				break;
			default:
				writeMessage = false;
				break;
		}
	}

	return retObj;
}
// Helper method for ReadMessageEnhanced_Scrollable(): Determines if there is a readable message after the
// one at the given offset.
//
// Parameters:
//  pOfset: The offset of the current message
//  pMsgInfo: An object contaiining message information
//  pTopMsgLineIdx: The index of the message line at the top of the reader area
//  pMsgLineFormatStr: A format string for the message line
//  pSolidBlockStartRow: The screen row of where the solid blocks start for the scrollbar
//  pNumSolidScrollBlocks: The number of solid blocks in the scrollbar
//
// Return value: An object containing the following properties:
//               newMsgOffset: The offset of the next readable message, if available.  If not available, this will be -1.
//               writeMessage: Boolean - Whether or not to write the whole message again (for the scrollable interface)
//               continueOn: Boolean - Whether or not to continue with the reader input loop
//               nextAction: A value indicating the next action for the reader to take after leaving the reader function
function DigDistMsgReader_ScrollableReaderNextReadableMessage(pOffset, pMsgInfo, pTopMsgLineIdx, pMsgLineFormatStr, pSolidBlockStartRow, pNumSolidScrollBlocks)
{
	var retObj = {
		newMsgOffset: -1,
		writeMessage: true,
		continueOn: true,
		nextAction: ACTION_NONE
	};

	// Look for a later message that isn't marked for deletion.  Even
	// if we don't find one, we'll still want to return from this
	// function (with message index -1) so that this script can go
	// onto the next message sub-board/group.
	retObj.newMsgOffset = this.FindNextNonDeletedMsgIdx(pOffset, true);
	// Note: Unlike the left arrow key, we want to exit this method when
	// navigating to the next message, regardless of whether or not the
	// user is allowed to change to a different sub-board, so that processes
	// that require continuation (such as new message scan) can continue.
	// Still, if there are no more readable messages in the current sub-board
	// (and thus the user would go onto the next message area), prompt the
	// user whether they want to continue onto the next message area.
	if (retObj.newMsgOffset == -1 && !curMsgSubBoardIsLast())
	{
		// For personal mail, don't do anything, and don't refresh the
		// message.  In a sub-board, ask the user if they want to go
		// to the next one.
		if (this.readingPersonalEmail)
			retObj.writeMessage = false;
		else
		{
			// If configured to allow the user to post in the sub-board
			// instead of going to the next message area and we're not
			// scanning, then do so.
			if (this.readingPostOnSubBoardInsteadOfGoToNext && !this.doingMsgScan)
			{
				console.print("\x01n");
				console.crlf();
				// Ask the user if they want to post on the sub-board.
				// If they say yes, then do so before exiting.
				var grpNameAndDesc = this.GetGroupNameAndDesc();
				if (!console.noyes(replaceAtCodesInStr(format(this.text.postOnSubBoard, grpNameAndDesc.grpName, grpNameAndDesc.grpDesc))))
					bbs.post_msg(this.subBoardCode);
				retObj.continueOn = false;
				retObj.nextAction = ACTION_QUIT;
			}
			else
			{
				// Prompt the user whether they want to go to the next message area
				if (this.EnhReaderPromptYesNo(replaceAtCodesInStr(this.text.goToNextMsgAreaPromptText), pMsgInfo.messageLines, pTopMsgLineIdx, pMsgLineFormatStr, pSolidBlockStartRow, pNumSolidScrollBlocks))
				{
					// Let this method exit and let the caller go to the next sub-board
					retObj.continueOn = false;
					retObj.nextAction = ACTION_GO_NEXT_MSG;
				}
				else
					retObj.writeMessage = false; // No need to refresh the message
			}
		}
	}
	else
	{
		// We're not at the end of the sub-board, so it's okay to exit this
		// method and go to the next message.
		retObj.continueOn = false;
		retObj.nextAction = ACTION_GO_NEXT_MSG;
	}

	return retObj;
}
// Helper method for ReadMessageEnhanced() - Determines the next keypress for a click
// coordinate outside the scroll area.
//
// Parameters:
//  pScrollRetObj: The return object of the message scroll function
//  pEnhReadHelpLineClickCoords: An array of click coordinates & action strings
//
// Return value: An object containing the following properties:
//               actionStr: A string containing the next action for the enhanced reader,
//                          or an empty string if there was no valid action found.
function DigDistMsgReader_ScrollReaderDetermineClickCoordAction(pScrollRetObj, pEnhReadHelpLineClickCoords)
{
	var retObj = {
		actionStr: ""
	};

	for (var coordIdx = 0; coordIdx < pEnhReadHelpLineClickCoords.length; ++coordIdx)
	{
		if ((pScrollRetObj.mouse.x == pEnhReadHelpLineClickCoords[coordIdx].x) && (pScrollRetObj.mouse.y == pEnhReadHelpLineClickCoords[coordIdx].y))
		{
			// The up arrow, down arrow, PageUp, PageDown, Home, and End aren't handled
			// here - Those are handled in scrollTextlines().
			if (pEnhReadHelpLineClickCoords[coordIdx].actionStr == LEFT_ARROW)
				retObj.actionStr = this.enhReaderKeys.previousMsg;
			else if (pEnhReadHelpLineClickCoords[coordIdx].actionStr == RIGHT_ARROW)
				retObj.actionStr = this.enhReaderKeys.nextMsg;
			else if (pEnhReadHelpLineClickCoords[coordIdx].actionStr.indexOf("DEL") == 0)
				retObj.actionStr = this.enhReaderKeys.deleteMessage;
			else if (pEnhReadHelpLineClickCoords[coordIdx].actionStr.indexOf("E)") == 0)
				retObj.actionStr = this.enhReaderKeys.editMsg;
			else if (pEnhReadHelpLineClickCoords[coordIdx].actionStr.indexOf("F") == 0)
				retObj.actionStr = this.enhReaderKeys.firstMsg;
			else if (pEnhReadHelpLineClickCoords[coordIdx].actionStr.indexOf("L") == 0)
				retObj.actionStr = this.enhReaderKeys.lastMsg;
			else if (pEnhReadHelpLineClickCoords[coordIdx].actionStr.indexOf("R") == 0)
				retObj.actionStr = this.enhReaderKeys.reply;
			else if (pEnhReadHelpLineClickCoords[coordIdx].actionStr.indexOf("C") == 0)
				retObj.actionStr = this.enhReaderKeys.chgMsgArea;
			else if (pEnhReadHelpLineClickCoords[coordIdx].actionStr.indexOf("Q") == 0)
				retObj.actionStr = this.enhReaderKeys.quit;
			else if (pEnhReadHelpLineClickCoords[coordIdx].actionStr.indexOf("?") == 0)
				retObj.actionStr = this.enhReaderKeys.showHelp;
			break;
		}
	}
	return retObj;
}
// Helper method for ReadMessageEnhanced() - Does the traditional (non-scrollable) reader interface
function DigDistMsgReader_ReadMessageEnhanced_Traditional(msgHeader, allowChgMsgArea, messageText, pOffset)
{
	var retObj = {
		offsetValid: true,
		msgNotReadable: false,
		userReplied: false,
		lastKeypress: "",
		newMsgOffset: -1,
		nextAction: ACTION_NONE,
		refreshEnhancedRdrHelpLine: false
	};

	// We could word-wrap the message to ensure words aren't split across lines, but
	// doing so could make some messages look bad (i.e., messages with drawing characters),
	// and word_wrap also might not handle ANSI or other color/attribute codes..
	//if (!textHasDrawingChars(messageText))
	//	messageText = word_wrap(messageText, this.msgAreaWidth);

	var msgHasAttachments = msgHdrHasAttachmentFlag(msgHeader);

	// Only interpret @-codes if the user is reading personal email and the sender is a sysop.  There
	// are many @-codes that do some action such as move the cursor, execute a
	// script, etc., and I don't want users on message networks to do anything
	// malicious to users on other BBSes.
	if (this.readingPersonalEmail && msgSenderIsASysop(msgHeader))
		messageText = replaceAtCodesInStr(messageText); // Or this.ParseMsgAtCodes(messageText, msgHeader) to replace only some @ codes
	var msgHasANSICodes = messageText.indexOf("\x1b[") >= 0;
	var msgTextWrapped = (msgHasANSICodes ? messageText : word_wrap(messageText, console.screen_columns-1));

	// Generate the key help text
	var keyHelpText = "\x01n\x01c\x01h#\x01n\x01b, \x01c\x01hLeft\x01n\x01b, \x01c\x01hRight\x01n\x01b, ";
	if (this.CanDelete() || this.CanDeleteLastMsg())
		keyHelpText += "\x01c\x01hDEL\x01b, ";
	if (this.CanEdit())
		keyHelpText += "\x01c\x01hE\x01y)\x01n\x01cdit\x01b, ";
	keyHelpText += "\x01c\x01hF\x01y)\x01n\x01cirst\x01b, \x01c\x01hL\x01y)\x01n\x01cast\x01b, \x01c\x01hR\x01y)\x01n\x01ceply\x01b, ";
	// If the user is allowed to change to a different message area, then
	// include that option.
	if (allowChgMsgArea)
	{
		// If there's room for the private reply option, then include that
		// before the change area option.
		if (console.screen_columns >= 89)
			keyHelpText += "\x01c\x01hP\x01y)\x01n\x01crivate reply\x01b, ";
		keyHelpText += "\x01c\x01hC\x01y)\x01n\x01chg area\x01b, ";
	}
	else
	{
		// The user isn't allowed to change to a different message area.
		// Go ahead and include the private reply option.
		keyHelpText += "\x01c\x01hP\x01y)\x01n\x01crivate reply\x01b, ";
	}
	keyHelpText += "\x01c\x01hQ\x01y)\x01n\x01cuit\x01b, \x01c\x01h?\x01g: \x01c";

	// User input loop
	var writeMessage = true;
	var writePromptText = true;
	var continueOn = true;
	while (continueOn)
	{
		if (writeMessage)
		{
			if (console.term_supports(USER_ANSI))
				console.clear("\x01n");
			// Write the message header & message body to the screen
			this.DisplayEnhancedMsgHdr(msgHeader, pOffset+1, 1);
			console.print("\x01n" + this.colors["msgBodyColor"]);
			console.putmsg(msgTextWrapped, P_NOATCODES);
		}
		// Write the prompt text
		if (writePromptText)
			console.print(keyHelpText);
		// Default the writing of the message & input prompt to true for the
		// next iteration.
		writeMessage = true;
		writePromptText = true;
		// Input a key from the user and take action based on the keypress.
		//retObj.lastKeypress = getKeyWithESCChars(K_UPPER|K_NOCRLF|K_NOECHO|K_NOSPIN);
		retObj.lastKeypress = getKeyWithESCChars(K_UPPER);
		switch (retObj.lastKeypress)
		{
			case this.enhReaderKeys.deleteMessage: // Delete message
				console.crlf();
				// Prompt the user for confirmation to delete the message.
				// Note: this.PromptAndDeleteOrUndeleteMessage() will check to see if the user
				// is a sysop or the message was posted by the user.
				// If the message was deleted, then exit this read method
				// and return KEY_RIGHT as the last keypress so that the
				// calling method will go to the next message/sub-board.
				// Otherwise (if the message was not deleted), refresh the
				// last 2 lines of the message on the screen.
				var msgWasDeleted = this.PromptAndDeleteOrUndeleteMessage(pOffset, null, true);
				if (msgWasDeleted && !canViewDeletedMsgs())
				{
					var msgSearchObj = this.LookForNextOrPriorNonDeletedMsg(pOffset);
					continueOn = msgSearchObj.continueInputLoop;
					retObj.newMsgOffset = msgSearchObj.newMsgOffset;
					retObj.nextAction = msgSearchObj.nextAction;
					if (msgSearchObj.promptGoToNextArea)
					{
						if (console.yesno(replaceAtCodesInStr(this.text.goToNextMsgAreaPromptText)))
						{
							// Let this method exit and let the caller go to the next sub-board
							continueOn = false;
							retObj.nextAction = ACTION_GO_NEXT_MSG;
						}
						else
							writeMessage = false; // No need to refresh the message
					}
				}
				break;
			case this.enhReaderKeys.selectMessage: // Select message (for batch delete, etc.)
				console.crlf();
				var selectMessage = !console.noyes("Select this message");
				this.ToggleSelectedMessage(this.subBoardCode, pOffset, selectMessage);
				break;
			case this.enhReaderKeys.batchDelete:
				// TODO: Write this?  Not sure yet if it makes much sense to
				// have batch delete in the reader interface.
				// Prompt the user for confirmation, and use
				// this.DeleteOrUndeleteSelectedMessages() to mark the selected messages
				// as deleted.
				// Returns an object with the following properties:
				//  deletedAll: Boolean - Whether or not all messages were successfully marked
				//              for deletion
				//  failureList: An object containing indexes of messages that failed to get
				//               marked for deletion, indexed by internal sub-board code, then
				//               containing messages indexes as properties.  Reasons for failing
				//               to mark messages deleted can include the user not having permission
				//               to delete in a sub-board, failure to open the sub-board, etc.
				writeMessage = false; // No need to refresh the message
				break;
			case this.enhReaderKeys.editMsg: // Edit the message
				if (this.CanEdit())
				{
					console.crlf();
					// Let the user edit the message if they want to
					var editReturnObj = this.EditExistingMsg(pOffset);
					// If the user confirmed editing the message, then see if the
					// message was edited and refresh the screen accordingly.
					if (editReturnObj.userConfirmed)
					{
						// If the message was edited, then refresh the text lines
						// array and update the other message-related variables.
						if (editReturnObj.msgEdited && (editReturnObj.newMsgIdx > -1))
						{
							// When the message is edited, the old message will be
							// deleted and the edited message will be posted as a new
							// message.  So we should return to the caller and have it
							// go directly to that new message.
							continueOn = false;
							retObj.newMsgOffset = editReturnObj.newMsgIdx;
						}
					}
				}
				else
				{
					writeMessage = false;
					writePromptText = false;
				}
				break;
			case this.enhReaderKeys.showHelp: // Show help
				if (!console.term_supports(USER_ANSI))
				{
					console.crlf();
					console.crlf();
				}
				this.DisplayEnhancedReaderHelp(allowChgMsgArea, msgHasAttachments);
				if (!console.term_supports(USER_ANSI))
				{
					console.crlf();
					console.crlf();
				}
				break;
			case this.enhReaderKeys.reply: // Reply to the message
			case this.enhReaderKeys.privateReply: // Private reply
				// If the user pressed the private reply key while reading private
				// mail, then do nothing (allow only the regular reply key to reply).
				// If not reading personal email, go ahead and let the user reply
				// with either the reply or private reply keypress.
				var privateReply = (retObj.lastKeypress == this.enhReaderKeys.privateReply);
				if (privateReply && this.readingPersonalEmail)
				{
					writeMessage = false; // Don't re-write the current message again
					writePromptText = false; // Don't write the prompt text again
				}
				else
				{
					console.crlf();
					// Get the message header with fields expanded so we can get the most info possible.
					//var extdMsgHdr = this.GetMsgHdrByAbsoluteNum(msgHeader.number, true);
					var msgbase = new MsgBase(this.subBoardCode);
					if (msgbase.open())
					{
						var extdMsgHdr = msgbase.get_msg_header(false, msgHeader.number, true);
						msgbase.close();
						// Let the user reply to the message
						var replyRetObj = this.ReplyToMsg(extdMsgHdr, messageText, privateReply, pOffset);
						retObj.userReplied = replyRetObj.postSucceeded;
						//retObj.msgNotReadable = replyRetObj.msgWasDeleted;
						var msgWasDeleted = replyRetObj.msgWasDeleted;
						if (msgWasDeleted && !canViewDeletedMsgs())
						{
							var msgSearchObj = this.LookForNextOrPriorNonDeletedMsg(pOffset);
							continueOn = msgSearchObj.continueInputLoop;
							retObj.newMsgOffset = msgSearchObj.newMsgOffset;
							retObj.nextAction = msgSearchObj.nextAction;
							if (msgSearchObj.promptGoToNextArea)
							{
								if (console.yesno(replaceAtCodesInStr(this.text.goToNextMsgAreaPromptText)))
								{
									// Let this method exit and let the caller go to the next sub-board
									continueOn = false;
									retObj.nextAction = ACTION_GO_NEXT_MSG;
								}
								else
									writeMessage = true; // We want to refresh the message on the screen
							}
						}
					}
					else // msgbase failed to open
					{
						console.print("\x01n");
						console.crlf();
						console.print("\x01h\x01yFailed to open the sub-board.  Aborting.\x01n");
						mswait(ERROR_PAUSE_WAIT_MS);
					}
				}
				break;
			case this.enhReaderKeys.postMsg: // Post a message
				if (!this.readingPersonalEmail)
				{
					// Let the user post a message.
					if (bbs.post_msg(this.subBoardCode))
					{
						// TODO: If the user is doing a search, it might be
						// useful to search their new message and add it to
						// the search results if it's a match..  but maybe
						// not?
					}

					console.pause();

					// We'll want to refresh the message & prompt text on the screen
					writeMessage = true;
					writePromptText = true;
				}
				else
				{
					// Don't write the current message or prompt text in the next iteration
					writeMessage = false;
					writePromptText = false;
				}
				break;
			// Numeric digit: The start of a number of a message to read
			case "0":
			case "1":
			case "2":
			case "3":
			case "4":
			case "5":
			case "6":
			case "7":
			case "8":
			case "9":
				console.crlf();
				// Put the user's input back in the input buffer to
				// be used for getting the rest of the message number.
				console.ungetstr(retObj.lastKeypress);
				// Prompt for the message number
				var msgNumInput = this.PromptForMsgNum(null, replaceAtCodesInStr(this.text.readMsgNumPromptText), false, ERROR_PAUSE_WAIT_MS, false);
				// Only allow reading the message if the message number is valid
				// and it's not the same message number that was passed in.
				if ((msgNumInput > 0) && (msgNumInput-1 != pOffset))
				{
					// If the message is marked as deleted, then output an error
					if (this.MessageIsDeleted(msgNumInput-1))
					{
						console.crlf();
						console.print("\x01n" + replaceAtCodesInStr(format(this.text.msgHasBeenDeletedText, msgNumInput)) + "\x01n");
						console.crlf();
						console.pause();
					}
					else
					{
						// Confirm with the user whether to read the message
						var readMsg = true;
						if (this.promptToReadMessage)
						{
							readMsg = console.yesno("\x01n" + this.colors["readMsgConfirmColor"]
													+ "Read message "
													+ this.colors["readMsgConfirmNumberColor"]
													+ msgNumInput + this.colors["readMsgConfirmColor"]
													+ ": Are you sure");
						}
						if (readMsg)
						{
							continueOn = false;
							retObj.newMsgOffset = msgNumInput - 1;
							retObj.nextAction = ACTION_GO_SPECIFIC_MSG;
						}
					}
				}
				break;
			case this.enhReaderKeys.prevMsgByTitle: // Previous message by title
			case this.enhReaderKeys.prevMsgByAuthor: // Previous message by author
			case this.enhReaderKeys.prevMsgByToUser: // Previous message by 'to user'
			case this.enhReaderKeys.prevMsgByThreadID: // Previous message by thread ID
				// Only allow this if we aren't doing a message search.
				if (!this.SearchingAndResultObjsDefinedForCurSub())
				{
					console.crlf(); // For the "Searching..." text
					var threadPrevMsgOffset = this.FindThreadPrevOffset(msgHeader,
																		keypressToThreadType(retObj.lastKeypress, this.enhReaderKeys),
																		false);
					if (threadPrevMsgOffset > -1)
					{
						retObj.newMsgOffset = threadPrevMsgOffset;
						retObj.nextAction = ACTION_GO_SPECIFIC_MSG;
						continueOn = false;
					}
				}
				else
				{
					writeMessage = false;
					writePromptText = false;
				}
				break;
			case this.enhReaderKeys.nextMsgByTitle: // Next message by title (subject)
			case this.enhReaderKeys.nextMsgByAuthor: // Next message by author
			case this.enhReaderKeys.nextMsgByToUser: // Next message by 'to user'
			case this.enhReaderKeys.nextMsgByThreadID: // Next message by thread ID
				// Only allow this if we aren't doing a message search.
				if (!this.SearchingAndResultObjsDefinedForCurSub())
				{
					console.crlf(); // For the "Searching..." text
					var threadNextMsgOffset = this.FindThreadNextOffset(msgHeader,
																		keypressToThreadType(retObj.lastKeypress, this.enhReaderKeys),
																		false);
					if (threadNextMsgOffset > -1)
					{
						retObj.newMsgOffset = threadNextMsgOffset;
						retObj.nextAction = ACTION_GO_SPECIFIC_MSG;
						continueOn = false;
					}
				}
				else
				{
					writeMessage = false;
					writePromptText = false;
				}
				break;
			case this.enhReaderKeys.previousMsg: // Previous message
				// TODO: Change the key for this?
				// Look for a prior message that isn't marked for deletion.  Even
				// if we don't find one, we'll still want to return from this
				// function (with message index -1) so that this script can go
				// onto the previous message sub-board/group.
				retObj.newMsgOffset = this.FindNextNonDeletedMsgIdx(pOffset, false);
				var goToPrevMessage = false;
				if ((retObj.newMsgOffset > -1) || allowChgMsgArea)
				{
					if (retObj.newMsgOffset == -1 && !curMsgSubBoardIsLast())
					{
						console.crlf();
						goToPrevMessage = console.yesno(replaceAtCodesInStr(this.text.goToPrevMsgAreaPromptText));
					}
					else
					{
						// We're not at the beginning of the sub-board, so it's okay to exit this
						// method and go to the previous message.
						goToPrevMessage = true;
					}
				}
				if (goToPrevMessage)
				{
					continueOn = false;
					retObj.nextAction = ACTION_GO_PREVIOUS_MSG;
				}
				break;
			case this.enhReaderKeys.nextMsg: // Next message
			case KEY_ENTER:
				// Look for a later message that isn't marked for deletion.  Even
				// if we don't find one, we'll still want to return from this
				// function (with message index -1) so that this script can go
				// onto the next message sub-board/group.
				retObj.newMsgOffset = this.FindNextNonDeletedMsgIdx(pOffset, true);
				// Note: Unlike the left arrow key, we want to exit this method when
				// navigating to the next message, regardless of whether or not the
				// user is allowed to change to a different sub-board, so that processes
				// that require continuation (such as new message scan) can continue.
				// Still, if there are no more readable messages in the current sub-board
				// (and thus the user would go onto the next message area), prompt the
				// user whether they want to continue onto the next message area.
				if (retObj.newMsgOffset == -1 && !curMsgSubBoardIsLast())
				{
					console.print("\x01n");
					console.crlf();
					// If configured to allow the user to post in the sub-board
					// instead of going to the next message area and we're not
					// scanning, then do so.
					if (this.readingPostOnSubBoardInsteadOfGoToNext && !this.doingMsgScan)
					{
						// Ask the user if they want to post on the sub-board.
						// If they say yes, then do so before exiting.
						var grpNameAndDesc = this.GetGroupNameAndDesc();
						if (!console.noyes(replaceAtCodesInStr(format(this.text.postOnSubBoard, grpNameAndDesc.grpName, grpNameAndDesc.grpDesc))))
							bbs.post_msg(this.subBoardCode);
						continueOn = false;
						retObj.nextAction = ACTION_QUIT;
					}
					else
					{
						if (console.yesno(replaceAtCodesInStr(this.text.goToNextMsgAreaPromptText)))
						{
							// Let this method exit and let the caller go to the next sub-board
							continueOn = false;
							retObj.nextAction = ACTION_GO_NEXT_MSG;
						}
					}
				}
				else
				{
					// We're not at the end of the sub-board, so it's okay to exit this
					// method and go to the next message.
					continueOn = false;
					retObj.nextAction = ACTION_GO_NEXT_MSG;
				}
				break;
			case this.enhReaderKeys.firstMsg: // First message
				// Only leave this function if we aren't already on the first message.
				if (pOffset > 0)
				{
					continueOn = false;
					retObj.nextAction = ACTION_GO_FIRST_MSG;
				}
				else
				{
					writeMessage = false;
					writePromptText = false;
				}
				break;
			case this.enhReaderKeys.lastMsg: // Last message
				// Only leave this function if we aren't already on the last message.
				if (pOffset < this.NumMessages() - 1)
				{
					continueOn = false;
					retObj.nextAction = ACTION_GO_LAST_MSG;
				}
				else
				{
					writeMessage = false;
					writePromptText = false;
				}
				break;
			case "-": // Go to the previous message area
				if (allowChgMsgArea)
				{
					continueOn = false;
					retObj.nextAction = ACTION_GO_PREV_MSG_AREA;
				}
				else
				{
					writeMessage = false;
					writePromptText = false;
				}
				break;
			case "+": // Go to the next message area
				if (allowChgMsgArea || this.doingMultiSubBoardScan)
				{
					continueOn = false;
					retObj.nextAction = ACTION_GO_NEXT_MSG_AREA;
				}
				else
				{
					writeMessage = false;
					writePromptText = false;
				}
				break;
			// H and K: Display the extended message header info/kludge lines
			// (for the sysop)
			case this.enhReaderKeys.showHdrInfo:
			case this.enhReaderKeys.showKludgeLines:
				if (user.is_sysop)
				{
					console.crlf();
					// Get an array of the extended header info/kludge lines and then
					// display them.
					var extdHdrInfoLines = this.GetExtdMsgHdrInfo(msgHeader, (retObj.lastKeypress == this.enhReaderKeys.showKludgeLines));
					if (extdHdrInfoLines.length > 0)
					{
						console.crlf();
						for (var infoIter = 0; infoIter < extdHdrInfoLines.length; ++infoIter)
						{
							console.print(extdHdrInfoLines[infoIter]);
							console.crlf();
						}
						console.pause();
					}
					else
					{
						// There are no kludge lines for this message
						console.print(replaceAtCodesInStr(this.text.noKludgeLinesForThisMsgText));
						console.crlf();
						console.pause();
					}
				}
				else // The user is not a sysop
				{
					writeMessage = false;
					writePromptText = false;
				}
				break;
				// Message list, change message area: Quit out of this input loop
				// and let the calling function, this.ReadMessages(), handle the
				// action.
			case this.enhReaderKeys.showMsgList: // Message list
				console.print("\x01n");
				console.crlf();
				console.print("Loading...");
				retObj.nextAction = ACTION_DISPLAY_MSG_LIST;
				continueOn = false;
				break;
			case this.enhReaderKeys.chgMsgArea: // Change message area, if allowed
				if (allowChgMsgArea)
				{
					retObj.nextAction = ACTION_CHG_MSG_AREA;
					continueOn = false;
				}
				else
				{
					writeMessage = false;
					writePromptText = false;
				}
				break;
			case this.enhReaderKeys.downloadAttachments: // Download attachments
				if (msgHasAttachments)
				{
					console.print("\x01n");
					console.crlf();
					console.print("\x01c- Download Attached Files -\x01n");
					allowUserToDownloadMessage_NewInterface(msgHeader, this.subBoardCode);

					// Ensure the message is refreshed on the screen
					writeMessage = true;
					writePromptText = true;
				}
				else
				{
					writeMessage = false;
					writePromptText = false;
				}
				break;
			case this.enhReaderKeys.saveToBBSMachine:
				// Save the message to the BBS machine - Only allow this
				// if the user is a sysop.
				if (user.is_sysop)
				{
					console.crlf();
					console.print("\x01n\x01cFilename:\x01h");
					var inputLen = console.screen_columns - 10; // 10 = "Filename:" length + 1
					var filename = console.getstr(inputLen, K_NOCRLF);
					console.print("\x01n");
					console.crlf();
					if (filename.length > 0)
					{
						var saveMsgRetObj = this.SaveMsgToFile(msgHeader, filename);
						if (saveMsgRetObj.succeeded)
						{
							console.print("\x01n\x01cThe message has been saved.\x01n");
							if (msgHdrHasAttachmentFlag(msgHeader))
								console.print(" Attachments not saved.");
							console.print("\x01n");
						}
						else
							console.print("\x01n\x01y\x01hFailed: " + saveMsgRetObj.errorMsg + "\x01n");
						mswait(ERROR_PAUSE_WAIT_MS);
					}
					else
					{
						console.print("\x01n\x01y\x01hMessage not exported\x01n");
						mswait(ERROR_PAUSE_WAIT_MS);
					}
					writeMessage = true;
				}
				else
					writeMessage = false;
				break;
			case this.enhReaderKeys.userEdit: // Edit the user who wrote the message
				if (user.is_sysop)
				{
					console.print("\x01n");
					console.crlf();
					console.print("- Edit user " + msgHeader.from);
					console.crlf();
					var editObj = editUser(msgHeader.from);
					if (editObj.errorMsg.length != 0)
					{
						console.print("\x01n");
						console.crlf();
						console.print("\x01y\x01h" + editObj.errorMsg + "\x01n");
						console.crlf();
						console.pause();
					}
				}
				writeMessage = true;
				break;
			case this.enhReaderKeys.forwardMsg: // Forward the message
				console.print("\x01n");
				console.crlf();
				console.print("\x01c- Forward message\x01n");
				console.crlf();
				var retStr = this.ForwardMessage(msgHeader, messageText);
				if (retStr.length > 0)
				{
					console.print("\x01n\x01h\x01y* " + retStr + "\x01n");
					console.crlf();
					console.pause();
				}
				writeMessage = true;
				break;
			case this.enhReaderKeys.vote: // Vote on the message
				var voteRetObj = this.VoteOnMessage(msgHeader);
				if (voteRetObj.BBSHasVoteFunction)
				{
					if (!voteRetObj.userQuit)
					{
						if ((voteRetObj.errorMsg.length > 0) || (!voteRetObj.savedVote))
						{
							console.print("\x01n");
							console.crlf();
							if (voteRetObj.errorMsg.length > 0)
							{
								if (voteRetObj.mnemonicsRequiredForErrorMsg)
								{
									console.mnemonics(voteRetObj.errorMsg);
									console.print("\x01n");
								}
								else
									console.print("\x01y\x01h* " + voteRetObj.errorMsg + "\x01n");
							}
							else if (!voteRetObj.savedVote)
								console.print("\x01y\x01h* Failed to save the vote\x01n");
							console.crlf();
							console.pause();
						}
						else
							msgHeader = voteRetObj.updatedHdr; // To get updated vote information
					}

					// If this message is a poll, then exit out of the reader
					// and come back to read the same message again so that the
					// voting results are re-loaded and displayed on the screen.
					if ((typeof(MSG_TYPE_POLL) != "undefined") && (msgHeader.type & MSG_TYPE_POLL) == MSG_TYPE_POLL)
					{
						retObj.newMsgOffset = pOffset;
						retObj.nextAction = ACTION_GO_SPECIFIC_MSG;
						continueOn = false;
					}
					else
						writeMessage = true; // We want to refresh the message on the screen
				}
				else
					writeMessage = false;
				break;
			case this.enhReaderKeys.showVotes: // Show votes
				if (msgHeader.hasOwnProperty("total_votes") && msgHeader.hasOwnProperty("upvotes"))
				{
					console.print("\x01n");
					console.crlf();
					var voteInfo = this.GetUpvoteAndDownvoteInfo(msgHeader);
					for (var voteInfoIdx = 0; voteInfoIdx < voteInfo.length; ++voteInfoIdx)
					{
						console.print(voteInfo[voteInfoIdx]);
						console.crlf();
					}
				}
				else
				{
					console.print("\x01n\x01h\x01yThere is no voting information for this message\x01n");
					console.crlf();
				}
				console.pause();
				writeMessage = true;
				break;
			case this.enhReaderKeys.closePoll: // Close a poll message
				var pollCloseMsg = "";
				console.print("\x01n");
				console.crlf();
				// If this message is a poll, then allow closing it.
				if ((typeof(MSG_TYPE_POLL) != "undefined") && (msgHeader.type & MSG_TYPE_POLL) == MSG_TYPE_POLL)
				{
					if ((msgHeader.auxattr & POLL_CLOSED) == 0)
					{
						// Only let the user close the poll if they created it
						if (userHandleAliasNameMatch(msgHeader.from))
						{
							// Prompt to confirm whether the user wants to close the poll
							if (!console.noyes("Close poll"))
							{
								// Close the poll (open the sub-board first)
								var msgbase = new MsgBase(this.subBoardCode);
								if (msgbase.open())
								{
									if (closePollWithOpenMsgbase(msgbase, msgHeader.number))
									{
										msgHeader.auxattr |= POLL_CLOSED;
										pollCloseMsg = "\x01n\x01cThis poll was successfully closed.";
									}
									else
										pollCloseMsg = "\x01n\x01r\x01h* Failed to close this poll!";
									msgbase.close();
								}
								else
									pollCloseMsg = "\x01n\x01r\x01h* Failed to open the sub-board!";
							}
						}
						else
							pollCloseMsg = "\x01n\x01y\x01hCan't close this poll because it's not yours";
					}
					else
						pollCloseMsg = "\x01n\x01y\x01hThis poll is already closed";
				}
				else
					pollCloseMsg = "This message is not a poll";

				// Display the poll closing status message
				if (strip_ctrl(pollCloseMsg).length > 0)
				{
					console.print("\x01n" + pollCloseMsg + "\x01n");
					console.crlf();
					console.pause();
				}
				writeMessage = true;
				break;
			case this.enhReaderKeys.validateMsg: // Validate the message
				if (user.is_sysop && (this.subBoardCode != "mail") && msg_area.sub[this.subBoardCode].is_moderated)
				{
					var message = "";
					if (this.ValidateMsg(this.subBoardCode, msgHeader.number))
					{
						message = "\x01n\x01cMessage validation successful";
						// Refresh the message header in the arrays
						this.RefreshMsgHdrInArrays(msgHeader.number);
						// Exit out of the reader and come back to read
						// the same message again so that the voting results
						// are re-loaded and displayed on the screen.
						retObj.newMsgOffset = pOffset;
						retObj.nextAction = ACTION_GO_SPECIFIC_MSG;
						continueOn = false;
					}
					else
					{
						message = "\x01n\x01y\x01hMessage validation failed!";
						writeMessage = true;
					}
					console.crlf();
					console.print(message);
					console.print("\x01n");
					console.crlf();
					console.pause();
				}
				else
					writeMessage = false;
				break;
			case this.enhReaderKeys.bypassSubBoardInNewScan:
				// TODO: Finish
				writeMessage = false;
				/*
				if (this.doingMsgScan)
				{
					console.print("\x01n");
					console.crlf();
					if (!console.noyes("Bypass this sub-board in newscans"))
					{
						continueOn = false;
						msg_area.sub[this.subBoardCode].scan_cfg &= SCAN_CFG_NEW;
					}
					else
						writeMessage = true;
				}
				else
					writeMessage = false;
				*/
				break;
			case this.enhReaderKeys.userSettings:
				var userSettingsRetObj = this.DoUserSettings_Traditional();
				// In case the user changed their twitlist, re-filter the messages for this sub-board
				if (userSettingsRetObj.userTwitListChanged)
				{
					console.crlf();
					console.print("\x01nTwitlist changed; re-filtering..");
					var tmpMsgbase = new MsgBase(this.subBoardCode);
					if (tmpMsgbase.open())
					{
						continueOn = false;
						writeMessage = false;
						var tmpAllMsgHdrs = tmpMsgbase.get_all_msg_headers(true);
						tmpMsgbase.close();
						this.FilterMsgHdrsIntoHdrsForCurrentSubBoard(tmpAllMsgHdrs, true);
						// If the user is currently reading a message a message by someone who is now
						// in their twit list, change the message currently being viewed.
						if (this.MsgHdrFromOrToInUserTwitlist(msgHeader))
						{
							var newReadableMsgOffset = this.FindNextNonDeletedMsgIdx(pOffset, true);
							if (newReadableMsgOffset > -1)
							{
								retObj.newMsgOffset = newReadableMsgOffset;
								retObj.offsetValid = true;
								retObj.nextAction = ACTION_GO_SPECIFIC_MSG;
							}
							else
								retObj.nextAction = ACTION_GO_NEXT_MSG_AREA;
						}
						else
						{
							// If there are still messages in this sub-board, and the message offset is beyond the last
							// message, then show the last message in the sub-board.  Otherwise, go to the next message area.
							if (this.hdrsForCurrentSubBoard.length > 0)
							{
								if (pOffset > this.hdrsForCurrentSubBoard.length)
								{
									//this.hdrsForCurrentSubBoard[this.hdrsForCurrentSubBoard.length-1].number
									retObj.newMsgOffset = this.hdrsForCurrentSubBoard.length - 1;
									retObj.offsetValid = true;
									retObj.nextAction = ACTION_GO_SPECIFIC_MSG;
								}
							}
							else
								retObj.nextAction = ACTION_GO_NEXT_MSG_AREA;
						}
					}
					else
						console.print("\x01y\x01hFailed to open the messagbase!\x01\r\n\x01p");
					this.SetUpLightbarMsgListVars();
					writeMessage = true;
				}
				break;
			case this.enhReaderKeys.quit: // Quit
				retObj.nextAction = ACTION_QUIT;
				continueOn = false;
				break;
			default:
				// No need to do anything
				writeMessage = false;
				writePromptText = false;
				break;
		}
	}

	return retObj;
}

// For the ReadMessageEnhanced methods: This function converts a thread navigation
// key character to its corresponding thread type value
function keypressToThreadType(pKeypress, pEnhReaderKeys)
{
	var threadType = THREAD_BY_ID;
	switch (pKeypress)
	{
		case pEnhReaderKeys.prevMsgByTitle:
		case pEnhReaderKeys.nextMsgByTitle:
			threadType = THREAD_BY_TITLE;
			break;
		case pEnhReaderKeys.prevMsgByAuthor:
		case pEnhReaderKeys.nextMsgByAuthor:
			threadType = THREAD_BY_AUTHOR;
			break;
		case pEnhReaderKeys.prevMsgByToUser:
		case pEnhReaderKeys.nextMsgByToUser:
			threadType = THREAD_BY_TO_USER;
			break;
		case pEnhReaderKeys.prevMsgByThreadID:
		case pEnhReaderKeys.nextMsgByThreadID:
		default:
			threadType = THREAD_BY_ID;
			break;
	}
	return threadType;
}

// For the DigDistMsgReader class: For the enhanced reader method - Prepares the
// last 2 lines on the screen for propmting the user for something.
//
// Return value: An object containing x and y values representing the cursor
//               position, ready to prompt the user.
function DigDistMsgReader_EnhReaderPrepLast2LinesForPrompt()
{
	var promptPos = { x: this.msgAreaLeft, y: this.msgAreaBottom };
	// Write a line of characters above where the prompt will be placed,
	// to help get the user's attention.
	console.gotoxy(promptPos.x, promptPos.y-1);
	console.print("\x01n" + this.colors.enhReaderPromptSepLineColor);
	for (var lineCounter = 0; lineCounter < this.msgAreaWidth; ++lineCounter)
		console.print(HORIZONTAL_SINGLE);
	// Clear the inside of the message area, so as not to overwrite
	// the scrollbar character
	console.print("\x01n");
	console.gotoxy(promptPos);
	for (var lineCounter = 0; lineCounter < this.msgAreaWidth; ++lineCounter)
		console.print(" ");
	// Position the cursor at the prompt location
	console.gotoxy(promptPos);
	
	return promptPos;
}

// For the DigDistMsgReader class: For the enhanced reader method - Looks for a
// later method that isn't marked for deletion.  If none is found, looks for a
// prior message that isn't marked for deletion.
//
// Parameters:
//  pOffset: The offset of the message to start at
//
// Return value: An object with the following properties:
//               newMsgOffset: The offset of the next readable message
//               nextAction: The next action (code) for the enhanced reader
//               continueInputLoop: Boolean - Whether or not to continue the input loop
//               promptGoToNextArea: Boolean - Whether or not to prompt the user to go
//                                   to the next message area
function DigDistMsgReader_LookForNextOrPriorNonDeletedMsg(pOffset)
{
	var retObj = {
		newMsgOffset: 0,
		nextAction: ACTION_NONE,
		continueInputLoop: true,
		promptGoToNextArea: false
	};

	// Look for a later message that isn't marked for deletion.
	// If none is found, then look for a prior message that isn't
	// marked for deletion.
	retObj.newMsgOffset = this.FindNextNonDeletedMsgIdx(pOffset, true);
	if (retObj.newMsgOffset > -1)
	{
		retObj.continueInputLoop = false;
		retObj.nextAction = ACTION_GO_NEXT_MSG;
	}
	else
	{
		// No later message found, so look for a prior message.
		retObj.newMsgOffset = this.FindNextNonDeletedMsgIdx(pOffset, false);
		if (retObj.newMsgOffset > -1)
		{
			retObj.continueInputLoop = false;
			retObj.nextAction = ACTION_GO_PREVIOUS_MSG;
		}
		else
		{
			// No prior message found.  We'll want to return from the enhanced
			// reader function (with message index -1) so that this script can
			// go onto the next message sub-board/group.  Also, set the next
			// action such that the calling method will go on to the next
			// message/sub-board.
			if (!curMsgSubBoardIsLast())
			{
				if (this.readingPersonalEmail)
				{
					retObj.continueInputLoop = false;
					retObj.nextAction = ACTION_QUIT;
				}
				else
					retObj.promptGoToNextArea =  true;
			}
			else
			{
				// We're not at the end of the sub-board or the current sub-board
				// is the last, so go ahead and exit.
				retObj.continueInputLoop = false;
				retObj.nextAction = ACTION_GO_NEXT_MSG;
			}
		}
	}
	return retObj;
}

// For the DigDistMsgReader class: Writes the help line for enhanced reader
// mode.
//
// Parameters:
//  pScreenRow: Optional - The screen row to write the help line on.  If not
//              specified, the last row on the screen will be used.
//  pDisplayChgAreaOpt: Optional boolean - Whether or not to show the "change area" option.
//                      Defaults to true.
function DigDistMsgReader_DisplayEnhancedMsgReadHelpLine(pScreenRow, pDisplayChgAreaOpt)
{
	var displayChgAreaOpt = (typeof(pDisplayChgAreaOpt) == "boolean" ? pDisplayChgAreaOpt : true);
	// Move the cursor to the desired location on the screen and display the help line
	console.gotoxy(1, typeof(pScreenRow) == "number" ? pScreenRow : console.screen_rows);
	// TODO: Mouse: console.print replaced with console.putmsg for mouse click hotspots
	//console.print(displayChgAreaOpt ? this.enhReadHelpLine : this.enhReadHelpLineWithoutChgArea);
	// console.putmsg() handles @-codes, which we use for mouse click tracking
	console.putmsg(displayChgAreaOpt ? this.enhReadHelpLine : this.enhReadHelpLineWithoutChgArea);
}

// For the DigDistMsgReader class: Goes back to the prior readable sub-board
// (accounting for search results, etc.).  Changes the object's subBoardCode,
// msgbase object, etc.
//
// Parameters:
//  pAllowChgMsgArea: Boolean - Whether or not the user is allowed to change
//                    to another message area
//  pPromptPrevIfNoResults: Optional boolean - Whether or not to prompt the user to
//                          go to the previous area if there are no search results.
//
// Return value: An object with the following properties:
//               changedMsgArea: Boolean - Whether or not this method successfully
//                               changed to a prior message area
//               msgIndex: The message index for the new sub-board.  Will be -1
//                         if there is no new sub-board or otherwise invalid
//                         scenario.
//               shouldStopReading: Whether or not the script should stop letting
//                                  the user read messages
function DigDistMsgReader_GoToPrevSubBoardForEnhReader(pAllowChgMsgArea, pPromptPrevIfNoResults)
{
	var retObj = {
		changedMsgArea: false,
		msgIndex: -1,
		shouldStopReading: false
	};

	// Only allow this if pAllowChgMsgArea is true and we're not reading personal
	// email.  If we're reading personal email, then msg_area.sub is unavailable
	// for the "mail" internal code.
	if (pAllowChgMsgArea && (this.subBoardCode != "mail"))
	{
		// continueGoingToPrevSubBoard specifies whether or not to continue
		// going to the previous sub-boards in case there is search text
		// specified.
		var continueGoingToPrevSubBoard = true;
		while (continueGoingToPrevSubBoard)
		{
			// Allow going to the previous message sub-board/group.
			var msgGrpIdx = msg_area.sub[this.subBoardCode].grp_index;
			var subBoardIdx = msg_area.sub[this.subBoardCode].index;
			var readMsgRetObj = findNextOrPrevNonEmptySubBoard(msgGrpIdx, subBoardIdx, false);
			// If a different sub-board was found, then go to that sub-board.
			if (readMsgRetObj.foundSubBoard && readMsgRetObj.subChanged)
			{
				bbs.cursub = 0;
				bbs.curgrp = readMsgRetObj.grpIdx;
				bbs.cursub = readMsgRetObj.subIdx;
				this.setSubBoardCode(readMsgRetObj.subCode);
				if (this.searchType == SEARCH_NONE || !this.SearchingAndResultObjsDefinedForCurSub())
				{
					continueGoingToPrevSubBoard = false; // No search results, so don't keep going to the previous sub-board.
					// Go to the user's last read message.  If the message index ends up
					// below 0, then go to the last message not marked as deleted.
					// We probably shouldn't use GetMsgIdx() yet because the arrays of
					// message headers have not been populated for the next area yet
					retObj.msgIndex = this.AbsMsgNumToIdx(msg_area.sub[this.subBoardCode].last_read);
					//retObj.msgIndex = this.GetMsgIdx(msg_area.sub[this.subBoardCode].last_read);
					if (retObj.msgIndex >= 0)
						retObj.changedMsgArea = true;
					else
					{
						// Look for the last message not marked as deleted
						var nonDeletedMsgIdx = this.FindNextNonDeletedMsgIdx(this.NumMessages(), false);
						// If a non-deleted message was found, then set retObj.msgIndex to it.
						// Otherwise, tell the user there are no messages in this sub-board
						// and return.
						if (nonDeletedMsgIdx > -1)
							retObj.msgIndex = nonDeletedMsgIdx;
						else
							retObj.msgIndex = this.NumMessages() - 1; // Shouldn't get here
						var newLastRead = this.IdxToAbsMsgNum(retObj.msgIndex);
						if (newLastRead > -1)
							msg_area.sub[this.subBoardCode].last_read = newLastRead;
					}
				}
				// Set the hotkey help line again, as this sub-board might have
				// different settings for whether messages can be edited or deleted,
				// then refresh it on the screen.
				var oldHotkeyHelpLine = this.enhReadHelpLine;
				this.SetEnhancedReaderHelpLine();
				// If a search is is specified that would populate the search
				// results, then populate this.msgSearchHdrs for the current
				// sub-board if there is search text specified.  If there
				// are no search results, then ask the user if they want
				// to continue searching the message areas.
				if (this.SearchTypePopulatesSearchResults())
				{
					if (this.PopulateHdrsIfSearch_DispErrorIfNoMsgs(false, true, false))
					{
						retObj.changedMsgArea = true;
						continueGoingToPrevSubBoard = false;
						retObj.msgIndex = this.NumMessages() - 1;
						if (this.scrollingReaderInterface && console.term_supports(USER_ANSI))
							this.DisplayEnhancedMsgReadHelpLine(console.screen_rows, pAllowChgMsgArea);
					}
					else // No search results in this sub-board
					{
						var promptPrevIfNoResults = (typeof(pPromptPrevIfNoResults) === "boolean" ? pPromptPrevIfNoResults : true);
						if (promptPrevIfNoResults)
							continueGoingToPrevSubBoard = !console.noyes("Continue searching");
						if (!continueGoingToPrevSubBoard)
						{
							retObj.shouldStopReading = true;
							return retObj;
						}
					}
				}
				else
				{
					retObj.changedMsgArea = true;
					this.PopulateHdrsForCurrentSubBoard();
					if ((oldHotkeyHelpLine != this.enhReadHelpLine) && this.scrollingReaderInterface && console.term_supports(USER_ANSI))
						this.DisplayEnhancedMsgReadHelpLine(console.screen_rows, pAllowChgMsgArea);
				}
			}
			else
			{
				// Didn't find a prior sub-board with readable messages.
				// We could stop and exit the script here by doing the following,
				// but I'd rather let the user exit when they want to.

				continueGoingToPrevSubBoard = false;
				// Show a message telling the user that there are no prior
				// messages or sub-boards.  Then, refresh the hotkey help line.
				writeWithPause(this.msgAreaLeft, console.screen_rows,
									"\x01n\x01h\x01y* No prior messages or no message in prior message areas.",
									ERROR_PAUSE_WAIT_MS, "\x01n", true);
				if (this.scrollingReaderInterface && console.term_supports(USER_ANSI))
					this.DisplayEnhancedMsgReadHelpLine(console.screen_rows, pAllowChgMsgArea);
			}
		}
	}

	return retObj;
}

// For the DigDistMsgReader class: Goes to the next readable sub-board
// (accounting for search results, etc.).  Changes the object's subBoardCode,
// msgbase object, etc.
//
// Parameters:
//  pAllowChgMsgArea: Boolean - Whether or not the user is allowed to change
//                    to another message area
//  pPromptNextIfNoResults: Optional boolean - Whether or not to prompt the user to
//                          go to the next area if there are no search results.
//                          Defaults to true.
//
// Return value: An object with the following properties:
//               changedMsgArea: Boolean - Whether or not this method successfully
//                               changed to a prior message area
//               msgIndex: The message index for the new sub-board.  Will be -1
//                         if there is no new sub-board or otherwise invalid
//                         scenario.
//               shouldStopReading: Whether or not the script should stop letting
//                                  the user read messages
function DigDistMsgReader_GoToNextSubBoardForEnhReader(pAllowChgMsgArea, pPromptNextIfNoResults)
{
	var retObj = {
		changedMsgArea: false,
		msgIndex: -1,
		shouldStopReading: false
	};

	// Only allow this if pAllowChgMsgArea is true and we're not reading personal
	// email.  If we're reading personal email, then msg_area.sub is unavailable
	// for the "mail" internal code.
	if (pAllowChgMsgArea && (this.subBoardCode != "mail"))
	{
		// continueGoingToNextSubBoard specifies whether or not to continue
		// advancing to the next sub-boards in case there is search text
		// specified.
		var continueGoingToNextSubBoard = true;
		while (continueGoingToNextSubBoard)
		{
			// Allow going to the next message sub-board/group.
			var msgGrpIdx = msg_area.sub[this.subBoardCode].grp_index;
			var subBoardIdx = msg_area.sub[this.subBoardCode].index;
			var readMsgRetObj = findNextOrPrevNonEmptySubBoard(msgGrpIdx, subBoardIdx, true);
			// If a different sub-board was found, then go to that sub-board.
			if (readMsgRetObj.foundSubBoard && readMsgRetObj.subChanged)
			{
				retObj.msgIndex = 0;
				bbs.cursub = 0;
				bbs.curgrp = readMsgRetObj.grpIdx;
				bbs.cursub = readMsgRetObj.subIdx;
				this.setSubBoardCode(readMsgRetObj.subCode);
				if ((this.searchType == SEARCH_NONE) || !this.SearchingAndResultObjsDefinedForCurSub())
				{
					continueGoingToNextSubBoard = false; // No search results, so don't keep going to the next sub-board.
					// Go to the user's last read message.  If the message index ends up
					// below 0, then go to the first message not marked as deleted.
					retObj.msgIndex = this.AbsMsgNumToIdx(msg_area.sub[this.subBoardCode].last_read);
					// We probably shouldn't use GetMsgIdx() yet because the arrays of
					// message headers have not been populated for the next area yet
					//retObj.msgIndex = this.GetMsgIdx(msg_area.sub[this.subBoardCode].last_read);
					if (retObj.msgIndex >= 0)
						retObj.changedMsgArea = true;
					else
					{
						// Set the index of the message to display - Look for the
						// first message not marked as deleted
						var nonDeletedMsgIdx = this.FindNextNonDeletedMsgIdx(this.NumMessages()-1, true);
						// If a non-deleted message was found, then set retObj.msgIndex to it.
						// Otherwise, tell the user there are no messages in this sub-board
						// and return.
						if (nonDeletedMsgIdx > -1)
						{
							retObj.msgIndex = nonDeletedMsgIdx;
							retObj.changedMsgArea = true;
							var newLastRead = this.IdxToAbsMsgNum(nonDeletedMsgIdx);
							if (newLastRead > -1)
								msg_area.sub[this.subBoardCode].last_read = newLastRead;
						}
					}
				}
				// Set the hotkey help line again, as this sub-board might have
				// different settings for whether messages can be edited or deleted,
				// then refresh it on the screen.
				var oldHotkeyHelpLine = this.enhReadHelpLine;
				this.SetEnhancedReaderHelpLine();
				// If a search is is specified that would populate the search
				// results, then populate this.msgSearchHdrs for the current
				// sub-board if there is search text specified.  If there
				// are no search results, then ask the user if they want
				// to continue searching the message areas.
				if (this.SearchTypePopulatesSearchResults())
				{
					if (this.PopulateHdrsIfSearch_DispErrorIfNoMsgs(false, true, false))
					{
						retObj.changedMsgArea = true;
						continueGoingToNextSubBoard = false;
						this.PopulateHdrsForCurrentSubBoard();
						retObj.msgIndex = 0;
						if (this.scrollingReaderInterface && console.term_supports(USER_ANSI))
							this.DisplayEnhancedMsgReadHelpLine(console.screen_rows, pAllowChgMsgArea);
					}
					else // No search results in this sub-board
					{
						var promptNextIfNoresults = (typeof(pPromptNextIfNoResults) === "boolean" ? pPromptNextIfNoResults : true);
						if (promptNextIfNoresults)
							continueGoingToNextSubBoard = !console.noyes("Continue searching");
						if (!continueGoingToNextSubBoard)
						{
							retObj.shouldStopReading = true;
							return retObj;
						}
					}
				}
				else
				{
					// There is no search.  Populate the arrays of all headers
					// for this sub-board
					this.PopulateHdrsForCurrentSubBoard();
					retObj.msgIndex = this.GetMsgIdx(msg_area.sub[this.subBoardCode].last_read);
					if (retObj.msgIndex == -1)
						retObj.msgIndex = 0;
				}
			}
			else
			{
				// Didn't find later sub-board with readable messages.
				// We could stop and exit the script here by doing the following,
				// but I'd rather let the user exit when they want to.
				//retObj.shouldStopReading = true;
				//return retObj;

				continueGoingToNextSubBoard = false;
				// Show a message telling the user that there are no more
				// messages or sub-boards.  Then, refresh the hotkey help line.
				writeWithPause(this.msgAreaLeft, console.screen_rows,
				               "\x01n\x01h\x01y* No more messages or message areas.",
				               ERROR_PAUSE_WAIT_MS, "\x01n", true);
				if (this.scrollingReaderInterface && console.term_supports(USER_ANSI))
					this.DisplayEnhancedMsgReadHelpLine(console.screen_rows, pAllowChgMsgArea);
			}
		}
	}

	return retObj;
}

// For the DigDistMsgReader Class: Prepares the variables that keep track of the
// traditional-interface message list position, current messsage number, etc.
function DigDistMsgReader_SetUpTraditionalMsgListVars()
{
	// If a search is specified, then just start at the first message.
	// If no search is specified, then get the index of the user's last read
	// message.  Then, figure out which page it's on and set the lightbar list
	// index & cursor position variables accordingly.
	var lastReadMsgIdx = 0;
	if (!this.SearchingAndResultObjsDefinedForCurSub())
	{
		lastReadMsgIdx = this.GetLastReadMsgIdxAndNum().lastReadMsgIdx;
		if (lastReadMsgIdx == -1)
			lastReadMsgIdx = 0;
	}
	var pageNum = findPageNumOfItemNum(lastReadMsgIdx+1, this.tradMsgListNumLines, this.NumMessages(),
									   this.reverseListOrder);
	this.CalcTraditionalMsgListTopIdx(pageNum);
	if (!this.reverseListOrder && (this.tradListTopMsgIdx > lastReadMsgIdx))
		this.tradListTopMsgIdx -= this.tradMsgListNumLines;
}

// For the DigDistMsgReader Class: Prepares the variables that keep track of the
// lightbar message list position, current messsage number, etc.
function DigDistMsgReader_SetUpLightbarMsgListVars()
{
	// If no search is specified or if reading personal email, then get the index
	// of the user's last read message.  Then, figure out which page it's on and
	// set the lightbar list index & cursor position variables accordingly.
	var lastReadMsgIdx = 0;
	if (!this.SearchingAndResultObjsDefinedForCurSub() || this.readingPersonalEmail)
	{
		lastReadMsgIdx = this.GetLastReadMsgIdxAndNum().lastReadMsgIdx;
		if (lastReadMsgIdx == -1)
			lastReadMsgIdx = 0;
	}
	else
	{
		// A search was specified.  If reading personal email, then set the
		// message index to the last read message.
		if (this.readingPersonalEmail)
		{
			lastReadMsgIdx = this.GetLastReadMsgIdxAndNum().lastReadMsgIdx;
			if (lastReadMsgIdx == -1)
				lastReadMsgIdx = 0;
		}
	}
	var pageNum = findPageNumOfItemNum(lastReadMsgIdx+1, this.lightbarMsgListNumLines, this.NumMessages(),
	                                   this.reverseListOrder);
	this.CalcLightbarMsgListTopIdx(pageNum);
	var initialCursorRow = 0;
	if (this.reverseListOrder)
		initialCursorRow = this.lightbarMsgListStartScreenRow+(this.lightbarListTopMsgIdx-lastReadMsgIdx);
	else
	{
		if (this.lightbarListTopMsgIdx > lastReadMsgIdx)
			this.lightbarListTopMsgIdx -= this.lightbarMsgListNumLines;
		initialCursorRow = this.lightbarMsgListStartScreenRow+(lastReadMsgIdx-this.lightbarListTopMsgIdx);
	}
	this.lightbarListSelectedMsgIdx = lastReadMsgIdx;
	this.lightbarListCurPos = { x: 1, y: initialCursorRow };
}

// For the DigDistMsgReader Class: Writes the message list column headers at the
// top of the screen.
function DigDistMsgReader_WriteMsgListScreenTopHeader()
{
	console.home();

	// If we will be displaying the message group and sub-board in the
	// header at the top of the screen (an additional 2 lines), then
	// update nMaxLines and nListStartLine to account for this.
	if (this.displayBoardInfoInHeader && canDoHighASCIIAndANSI()) // console.term_supports(USER_ANSI)
	{
		var curpos = console.getxy();
		// Figure out the message group name & sub-board name
		// For the message group name, we can also use msgbase.cfg.grp_name in
		// Synchronet 3.12 and higher.
		var msgbase = new MsgBase(this.subBoardCode);
		var msgGroupName = "";
		if (this.subBoardCode == "mail")
			msgGroupName = "Mail";
		else
			msgGroupName = msg_area.grp_list[msgbase.cfg.grp_number].description;
		var subBoardName = "Unspecified";
		if (msgbase.open())
		{
			if (msgbase.cfg != null)
				subBoardName = msgbase.cfg.description;
			else if ((msgbase.subnum == -1) || (msgbase.subnum == 65535))
				subBoardName = "Electronic Mail";
			else
				subBoardName = "Unspecified";
			msgbase.close();
		}

		// Display the message group name
		console.print(this.colors["msgListHeaderMsgGroupTextColor"] + "Msg group: " +
		this.colors["msgListHeaderMsgGroupNameColor"] + msgGroupName);
		console.cleartoeol(); // Fill to the end of the line with the current colors
		// Display the sub-board name on the next line
		++curpos.y;
		console.gotoxy(curpos);
		console.print(this.colors["msgListHeaderSubBoardTextColor"] + "Sub-board: " +
		this.colors["msgListHeaderMsgSubBoardName"] + subBoardName);
		console.cleartoeol(); // Fill to the end of the line with the current colors
		++curpos.y;
		console.gotoxy(curpos);
	}

	// Write the message listing column headers
	if (this.showScoresInMsgList)
		printf(this.colors.msgListColHeader + this.sMsgListHdrFormatStr, "Msg#", "From", "To", "Subject", "+/-", "Date", "Time");
	else
		printf(this.colors.msgListColHeader + this.sMsgListHdrFormatStr, "Msg#", "From", "To", "Subject", "Date", "Time");

	// Set the normal text attribute
	console.print("\x01n");
}
// For the DigDistMsgReader Class: Lists a screenful of message header information.
//
// Parameters:
//  pTopIndex: The index (offset) of the top message
//  pMaxLines: The maximum number of lines to output to the screen
//
// Return value: Boolean, whether or not the last message output to the
//               screen is the last message in the sub-board.
function DigDistMsgReader_ListScreenfulOfMessages(pTopIndex, pMaxLines)
{
	var atLastPage = false;

	var curpos = console.getxy();
	var msgIndex = 0;
	if (this.reverseListOrder)
	{
		var endIndex = pTopIndex - pMaxLines + 1; // The index of the last message to display
		for (msgIndex = pTopIndex; (msgIndex >= 0) && (msgIndex >= endIndex); --msgIndex)
		{
			// The following line which sets console.line_counter to 0 is a
			// kludge to disable Synchronet's automatic pausing after a
			// screenful of text, so that this script can have more control
			// over screen pausing.
			console.line_counter = 0;

			// Get the message header (it will be a MsgHeader object) and
			// display it.
			msgHeader = this.GetMsgHdrByIdx(msgIndex, this.showScoresInMsgList);
			if (msgHeader == null)
				continue;

			// Display the message info
			this.PrintMessageInfo(msgHeader, false, msgIndex+1);
			if (console.term_supports(USER_ANSI))
			{
				++curpos.y;
				console.gotoxy(curpos);
			}
			else
				console.crlf();
		}

		atLastPage = (msgIndex < 0);
	}
	else
	{
		var endIndex = pTopIndex + pMaxLines; // One past the last message index to display
		for (msgIndex = pTopIndex; (msgIndex < this.NumMessages()) && (msgIndex < endIndex); ++msgIndex)
		{
			// The following line which sets console.line_counter to 0 is a
			// kludge to disable Synchronet's automatic pausing after a
			// screenful of text, so that this script can have more control
			// over screen pausing.
			console.line_counter = 0;

			// Get the message header (it will be a MsgHeader object) and
			// display it.
			var msgHeader = this.GetMsgHdrByIdx(msgIndex, this.showScoresInMsgList);
			if (msgHeader == null)
				continue;

			// Display the message info
			this.PrintMessageInfo(msgHeader, false, msgIndex+1);
			if (console.term_supports(USER_ANSI))
			{
				++curpos.y;
				console.gotoxy(curpos);
			}
			else
				console.crlf();
		}

		atLastPage = (msgIndex == this.NumMessages());
	}

	return atLastPage;
}
// For the DigDistMsgReader Class: Displays the help screen for the message list.
//
// Parameters:
//  pChgSubBoardAllowed: Whether or not changing to another sub-board is allowed
//  pPauseAtEnd: Boolean, whether or not to pause at the end.
function DigDistMsgReader_DisplayMsgListHelp(pChgSubBoardAllowed, pPauseAtEnd)
{
	DisplayProgramInfo();

	// Display help specific to which interface is being used.
	if (this.msgListUseLightbarListInterface)
		this.DisplayLightbarMsgListHelp(false, pChgSubBoardAllowed);
	else
		this.DisplayTraditionalMsgListHelp(false, pChgSubBoardAllowed);

	// If pPauseAtEnd is true, then output a newline and
	// prompt the user whether or not to continue.
	if (pPauseAtEnd)
		console.pause();
}
// For the DigDistMsgReader Class: Displays help for the traditional-interface
// message list
//
// Parameters:
//  pDisplayHeader: Whether or not to display a help header at the beginning
//  pChgSubBoardAllowed: Whether or not changing to another sub-board is allowed
//  pPauseAtEnd: Boolean, whether or not to pause at the end.
function DigDistMsgReader_DisplayTraditionalMsgListHelp(pDisplayHeader, pChgSubBoardAllowed, pPauseAtEnd)
{
	// If pDisplayHeader is true, then display the program information.
	if (pDisplayHeader)
		DisplayProgramInfo();

	// Display information about the current sub-board and search results.
	console.print("\x01n\x01cCurrently reading \x01g" + subBoardGrpAndName(this.subBoardCode));
	console.crlf();
	// If the user isn't reading personal messages (i.e., is reading a sub-board),
	// then output the total number of messages in the sub-board.  We probably
	// shouldn't output the total number of messages in the "mail" area, because
	// that includes more than the current user's email.
	if (this.subBoardCode != "mail")
	{
		var numOfMessages = 0;
		var msgbase = new MsgBase(this.subBoardCode);
		if (msgbase.open())
		{
			numOfMessages = msgbase.total_msgs;
			msgbase.close();
		}
		console.print("\x01n\x01cThere are a total of \x01g" + numOfMessages + " \x01cmessages in the current area.");
		console.crlf();
	}
	// If there is currently a search (which also includes personal messages),
	// then output the number of search results/personal messages.
	if (this.SearchingAndResultObjsDefinedForCurSub())
	{
		var numSearchResults = this.NumMessages();
		var resultsWord = (numSearchResults > 1 ? "results" : "result");
		console.print("\x01n\x01c");
		if (this.readingPersonalEmail)
			console.print("You have \x01g" + numSearchResults + " \x01c" + (numSearchResults == 1 ? "message" : "messages") + ".");
		else
		{
			if (numSearchResults == 1)
				console.print("There is \x01g1 \x01csearch result.");
			else
				console.print("There are \x01g" + numSearchResults + " \x01csearch results.");
		}
		console.crlf();
	}
	console.crlf();

	console.print("\x01n" + this.colors["tradInterfaceHelpScreenColor"]);
	displayTextWithLineBelow("Page navigation and message selection", false,
	                         this.colors["tradInterfaceHelpScreenColor"], "\x01k\x01h");
	console.print(this.colors["tradInterfaceHelpScreenColor"]);
	console.print("The message lister will display a page of message header information.  At\r\n");
	console.print("the end of each page, a prompt is displayed, allowing you to navigate to\r\n");
	console.print("the next page, previous page, first page, or the last page.  If you would\r\n");
	console.print("like to read a message, you may type the message number, followed by\r\n");
	console.print("the enter key if the message number is short.  To quit the listing, press\r\n");
	console.print("the Q key.\r\n\r\n");
	this.DisplayMessageListNotesHelp();
	console.crlf();
	console.crlf();
	displayTextWithLineBelow("Summary of the keyboard commands:", false,
	                         this.colors["tradInterfaceHelpScreenColor"], "\x01k\x01h");
	console.print(this.colors["tradInterfaceHelpScreenColor"]);
	console.print("\x01n\x01h\x01cN" + this.colors["tradInterfaceHelpScreenColor"] + ": Go to the next page\r\n");
	console.print("\x01n\x01h\x01cP" + this.colors["tradInterfaceHelpScreenColor"] + ": Go to the previous page\r\n");
	console.print("\x01n\x01h\x01cF" + this.colors["tradInterfaceHelpScreenColor"] + ": Go to the first page\r\n");
	console.print("\x01n\x01h\x01cL" + this.colors["tradInterfaceHelpScreenColor"] + ": Go to the last page\r\n");
	console.print("\x01n\x01h\x01cG" + this.colors["tradInterfaceHelpScreenColor"] + ": Go to a specific message by number (the message will appear at the top\r\n" +
	              "   of the list)\r\n");
	console.print("\x01n\x01h\x01cNumber" + this.colors["tradInterfaceHelpScreenColor"] + ": Read the message corresponding with that number\r\n");
	//console.print("The following commands are available only if you have permission to do so:\r\n");
	if (this.CanDelete() || this.CanDeleteLastMsg())
		console.print("\x01n\x01h\x01cD" + this.colors["tradInterfaceHelpScreenColor"] + ": Mark a message for deletion\r\n");
	if (this.CanEdit())
		console.print("\x01n\x01h\x01cE" + this.colors["tradInterfaceHelpScreenColor"] + ": Edit an existing message\r\n");
	if (pChgSubBoardAllowed)
		console.print("\x01n\x01h\x01cC" + this.colors["tradInterfaceHelpScreenColor"] + ": Change to another message sub-board\r\n");
	console.print("\x01n\x01h\x01cS" + this.colors["tradInterfaceHelpScreenColor"] + ": Select messages (for batch delete, etc.)\r\n");
	console.print("\x01n" + this.colors["tradInterfaceHelpScreenColor"] + "  A message number or multiple numbers can be entered separated by commas or\r\n");
	console.print("\x01n" + this.colors["tradInterfaceHelpScreenColor"] + "  spaces.  Additionally, a range of numbers (separated by a dash) can be used.\r\n");
	console.print("\x01n" + this.colors["tradInterfaceHelpScreenColor"] + "  Examples:\r\n");
	console.print("\x01n" + this.colors["tradInterfaceHelpScreenColor"] + "  125\r\n");
	console.print("\x01n" + this.colors["tradInterfaceHelpScreenColor"] + "  1,2,3\r\n");
	console.print("\x01n" + this.colors["tradInterfaceHelpScreenColor"] + "  1 2 3\r\n");
	console.print("\x01n" + this.colors["tradInterfaceHelpScreenColor"] + "  1,2,10-20\r\n");
	console.print("\x01n\x01h\x01cCTRL-D" + this.colors["tradInterfaceHelpScreenColor"] + ": Batch delete selected messages\r\n");
	console.print("\x01n\x01h\x01cQ" + this.colors["tradInterfaceHelpScreenColor"] + ": Quit\r\n");
	console.print("\x01n\x01h\x01c?" + this.colors["tradInterfaceHelpScreenColor"] + ": Show this help screen\r\n\r\n");

	// If pPauseAtEnd is true, then output a newline and
	// prompt the user whether or not to continue.
	if (pPauseAtEnd)
		console.pause();
}
// For the DigDistMsgReader Class: Displays help for the lightbar message list
//
// Parameters:
//  pDisplayHeader: Whether or not to display a help header at the beginning
//  pChgSubBoardAllowed: Whether or not changing to another sub-board is allowed
//  pPauseAtEnd: Boolean, whether or not to pause at the end.
function DigDistMsgReader_DisplayLightbarMsgListHelp(pDisplayHeader, pChgSubBoardAllowed, pPauseAtEnd)
{
	// If pDisplayHeader is true, then display the program information.
	if (pDisplayHeader)
		DisplayProgramInfo();

	// Display information about the current sub-board and search results.
	console.print("\x01n\x01cCurrently reading \x01g" + subBoardGrpAndName(this.subBoardCode));
	console.crlf();
	// If the user isn't reading personal messages (i.e., is reading a sub-board),
	// then output the total number of messages in the sub-board.  We probably
	// shouldn't output the total number of messages in the "mail" area, because
	// that includes more than the current user's email.
	if (this.subBoardCode != "mail")
	{
		var numOfMessages = 0;
		var msgbase = new MsgBase(this.subBoardCode);
		if (msgbase.open())
		{
			numOfMessages = msgbase.total_msgs;
			msgbase.close();
		}
		console.print("\x01n\x01cThere are a total of \x01g" + numOfMessages + " \x01cmessages in the current area.");
		console.crlf();
	}
	// If there is currently a search (which also includes personal messages),
	// then output the number of search results/personal messages.
	if (this.SearchingAndResultObjsDefinedForCurSub())
	{
		var numSearchResults = this.NumMessages();
		var resultsWord = (numSearchResults > 1 ? "results" : "result");
		console.print("\x01n\x01c");
		if (this.readingPersonalEmail)
			console.print("You have \x01g" + numSearchResults + " \x01c" + (numSearchResults == 1 ? "message" : "messages") + ".");
		else
		{
			if (numSearchResults == 1)
				console.print("There is \x01g1 \x01csearch result.");
			else
				console.print("There are \x01g" + numSearchResults + " \x01csearch results.");
		}
		console.crlf();
	}
	console.crlf();

	displayTextWithLineBelow("Lightbar interface: Page navigation and message selection",
	                         false, this.colors["tradInterfaceHelpScreenColor"], "\x01k\x01h");
	console.print(this.colors["tradInterfaceHelpScreenColor"]);
	console.print("The message lister will display a page of message header information.  You\r\n");
	console.print("may use the up and down arrows to navigate the list of messages.  The\r\n");
	console.print("currently-selected message will be highlighted as you navigate through\r\n");
	console.print("the list.  To read a message, navigate to the desired message and press\r\n");
	console.print("the enter key.  You can also read a message by typing its message number.\r\n");
	console.print("To quit out of the message list, press the Q key.\r\n\r\n");
	this.DisplayMessageListNotesHelp();
	console.crlf();
	console.crlf();
	displayTextWithLineBelow("Summary of the keyboard commands:", false, this.colors["tradInterfaceHelpScreenColor"], "\x01k\x01h");
	console.print(this.colors["tradInterfaceHelpScreenColor"]);
	console.print("\x01n\x01h\x01cDown arrow" + this.colors["tradInterfaceHelpScreenColor"] + ": Move the cursor down/select the next message\r\n");
	console.print("\x01n\x01h\x01cUp arrow" + this.colors["tradInterfaceHelpScreenColor"] + ": Move the cursor up/select the previous message\r\n");
	console.print("\x01n\x01h\x01cN" + this.colors["tradInterfaceHelpScreenColor"] + ": Go to the next page\r\n");
	console.print("\x01n\x01h\x01cP" + this.colors["tradInterfaceHelpScreenColor"] + ": Go to the previous page\r\n");
	console.print("\x01n\x01h\x01cF" + this.colors["tradInterfaceHelpScreenColor"] + ": Go to the first page\r\n");
	console.print("\x01n\x01h\x01cL" + this.colors["tradInterfaceHelpScreenColor"] + ": Go to the last page\r\n");
	console.print("\x01n\x01h\x01cG" + this.colors["tradInterfaceHelpScreenColor"] + ": Go to a specific message by number (the message will be highlighted and\r\n" +
	              "   may appear at the top of the list)\r\n");
	console.print("\x01n\x01h\x01cENTER" + this.colors["tradInterfaceHelpScreenColor"] + ": Read the selected message\r\n");
	console.print("\x01n\x01h\x01cNumber" + this.colors["tradInterfaceHelpScreenColor"] + ": Read the message corresponding with that number\r\n");
	if (this.CanDelete() || this.CanDeleteLastMsg())
		console.print("\x01n\x01h\x01cDEL" + this.colors["tradInterfaceHelpScreenColor"] + ": Mark the selected message for deletion\r\n");
	if (this.CanEdit())
		console.print("\x01n\x01h\x01cE" + this.colors["tradInterfaceHelpScreenColor"] + ": Edit the selected message\r\n");
	if (pChgSubBoardAllowed)
		console.print("\x01n\x01h\x01cC" + this.colors["tradInterfaceHelpScreenColor"] + ": Change to another message sub-board\r\n");
	console.print("\x01n\x01h\x01cSpacebar" + this.colors["tradInterfaceHelpScreenColor"] + ": Select message (for batch delete, etc.)\r\n");
	console.print("\x01n\x01h\x01cCTRL-A" + this.colors["tradInterfaceHelpScreenColor"] + ": Select/de-select all messages\r\n");
	console.print("\x01n\x01h\x01cCTRL-D" + this.colors["tradInterfaceHelpScreenColor"] + ": Batch delete selected messages\r\n");
	console.print("\x01n\x01h\x01cQ" + this.colors["tradInterfaceHelpScreenColor"] + ": Quit\r\n");
	console.print("\x01n\x01h\x01c?" + this.colors["tradInterfaceHelpScreenColor"] + ": Show this help screen\r\n");

	// If pPauseAtEnd is true, then pause.
	if (pPauseAtEnd)
		console.pause();
}
// For the DigDistMsgReader class: Displays the message list notes for the
// help screens.
function DigDistMsgReader_DisplayMessageListNotesHelp()
{
	displayTextWithLineBelow("Notes about the message list:", false,
	                         this.colors["tradInterfaceHelpScreenColor"], "\x01n\x01k\x01h")
	console.print(this.colors["tradInterfaceHelpScreenColor"]);
	console.print("If a message has been marked for deletion, it will appear with a blinking\r\n");
	console.print("red asterisk (\x01n\x01h\x01r\x01i*" + "\x01n" + this.colors["tradInterfaceHelpScreenColor"] + ") in");
	console.print(" after the message number in the message list.");
}
// For the DigDistMsgReader Class: Sets the traditional UI pause prompt text
// strings, sLightbarModeHelpLine, the text string for the lightbar help line,
// for the message lister interface.  This checks with the MsgBase object to determine
// if the user is allowed to delete or edit messages, and if so, adds the
// appropriate keys to the prompt & help text.
function DigDistMsgReader_SetMsgListPauseTextAndLightbarHelpLine()
{


	// Set the traditional UI pause prompt text.
	// If the user can delete messages, then append D as a valid key.
	// If the user can edit messages, then append E as a valid key.
	this.sStartContinuePrompt = "\x01n\x01c(" + this.colors["tradInterfaceContPromptHotkeyColor"] + "N\x01n\x01c)"
	                          + this.colors["tradInterfaceContPromptMainColor"]
	                          + "ext, \x01n\x01c(" + this.colors["tradInterfaceContPromptHotkeyColor"] + "L\x01n\x01c)"
	                          + this.colors["tradInterfaceContPromptMainColor"]
	                          + "ast, \x01n\x01c(" + this.colors["tradInterfaceContPromptHotkeyColor"] + "G\x01n\x01c)"
	                          + this.colors["tradInterfaceContPromptMainColor"] + "o, ";
	if (this.CanDelete() || this.CanDeleteLastMsg())
	{
		this.sStartContinuePrompt += "\x01n\x01c(" + this.colors["tradInterfaceContPromptHotkeyColor"]
		                          + "D\x01n\x01c)" + this.colors["tradInterfaceContPromptMainColor"] + "el, ";
	}
	if (this.CanEdit())
	{
		this.sStartContinuePrompt += "\x01n\x01c(" + this.colors["tradInterfaceContPromptHotkeyColor"]
		                          + "E\x01n\x01c)" + this.colors["tradInterfaceContPromptMainColor"] + "dit, ";
	}
	this.sStartContinuePrompt += "\x01n\x01c(" + this.colors["tradInterfaceContPromptHotkeyColor"] + "S\x01n\x01c)"
	                     + this.colors["tradInterfaceContPromptMainColor"]
	                     + "el, ";
	this.sStartContinuePrompt += "\x01n\x01c(" + this.colors["tradInterfaceContPromptHotkeyColor"] + "Q\x01n\x01c)"
	                          + this.colors["tradInterfaceContPromptMainColor"]
	                          + "uit, msg" + this.colors["tradInterfaceContPromptHotkeyColor"] + "#" +
	                          this.colors["tradInterfaceContPromptMainColor"] + ", " + this.colors["tradInterfaceContPromptHotkeyColor"]
	                          + "?" + this.colors["tradInterfaceContPromptMainColor"] + ": "
	                          		+ this.colors["tradInterfaceContPromptUserInputColor"];

	this.sContinuePrompt = "\x01n\x01c(" + this.colors["tradInterfaceContPromptHotkeyColor"] + "N\x01n\x01c)"
	                     + this.colors["tradInterfaceContPromptMainColor"]
	                     + "ext, \x01c(" + this.colors["tradInterfaceContPromptHotkeyColor"] + "P\x01n\x01c)"
	                     + this.colors["tradInterfaceContPromptMainColor"]
	                     + "rev, \x01n\x01c(" + this.colors["tradInterfaceContPromptHotkeyColor"] + "F\x01n\x01c)"
	                     + this.colors["tradInterfaceContPromptMainColor"]
	                     + "irst, \x01n\x01c(" + this.colors["tradInterfaceContPromptHotkeyColor"] + "L\x01n\x01c)"
	                     + this.colors["tradInterfaceContPromptMainColor"]
	                     + "ast, \x01n\x01c(" + this.colors["tradInterfaceContPromptHotkeyColor"] + "G\x01n\x01c)"
	                     + this.colors["tradInterfaceContPromptMainColor"] + "o, ";
	if (this.CanDelete() || this.CanDeleteLastMsg())
	{
		this.sContinuePrompt += "\x01n\x01c(" + this.colors["tradInterfaceContPromptHotkeyColor"]
		                     + "D\x01n\x01c)" + this.colors["tradInterfaceContPromptMainColor"] + "el, ";
	}
	if (this.CanEdit())
	{
		this.sContinuePrompt += "\x01n\x01c(" + this.colors["tradInterfaceContPromptHotkeyColor"]
		                     + "E\x01n\x01c)" + this.colors["tradInterfaceContPromptMainColor"] + "dit, ";
	}
	this.sContinuePrompt += "\x01n\x01c(" + this.colors["tradInterfaceContPromptHotkeyColor"] + "S\x01n\x01c)"
	                     + this.colors["tradInterfaceContPromptMainColor"]
	                     + "el, ";
	this.sContinuePrompt += "\x01n\x01c(" + this.colors["tradInterfaceContPromptHotkeyColor"] + "Q\x01n\x01c)"
	                     + this.colors["tradInterfaceContPromptMainColor"]
	                     + "uit, msg" + this.colors["tradInterfaceContPromptHotkeyColor"] + "#"
	                     + this.colors["tradInterfaceContPromptMainColor"] + ", " + this.colors["tradInterfaceContPromptHotkeyColor"]
	                     + "?" + this.colors["tradInterfaceContPromptMainColor"] + ": "
	                     + this.colors["tradInterfaceContPromptUserInputColor"];

	this.sEndContinuePrompt = "\x01n\x01c(" + this.colors["tradInterfaceContPromptHotkeyColor"] + "P\x01n\x01c)"
	                        + this.colors["tradInterfaceContPromptMainColor"]
	                        + "rev, \x01n\x01c(" + this.colors["tradInterfaceContPromptHotkeyColor"] + "F\x01n\x01c)"
	                        + this.colors["tradInterfaceContPromptMainColor"]
	                        + "irst, \x01n\x01c(" + this.colors["tradInterfaceContPromptHotkeyColor"] + "G\x01n\x01c)"
	                        + this.colors["tradInterfaceContPromptMainColor"] + "o, ";
	if (this.CanDelete() || this.CanDeleteLastMsg())
	{
		this.sEndContinuePrompt += "\x01n\x01c(" + this.colors["tradInterfaceContPromptHotkeyColor"]
		                        + "D\x01n\x01c)" + this.colors["tradInterfaceContPromptMainColor"] + "el, ";
	}
	if (this.CanEdit())
	{
		this.sEndContinuePrompt += "\x01n\x01c(" + this.colors["tradInterfaceContPromptHotkeyColor"]
		                        + "E\x01n\x01c)" + this.colors["tradInterfaceContPromptMainColor"] + "dit, ";
	}
	this.sEndContinuePrompt += "\x01n\x01c(" + this.colors["tradInterfaceContPromptHotkeyColor"] + "S\x01n\x01c)"
	                        + this.colors["tradInterfaceContPromptMainColor"]
	                        + "el, ";
	this.sEndContinuePrompt += "\x01n\x01c(" + this.colors["tradInterfaceContPromptHotkeyColor"] + "Q\x01n\x01c)"
	                        + this.colors["tradInterfaceContPromptMainColor"]
	                        + "uit, msg" + this.colors["tradInterfaceContPromptHotkeyColor"] + "#"
	                        + this.colors["tradInterfaceContPromptMainColor"] + ", " + this.colors["tradInterfaceContPromptHotkeyColor"]
	                        + "?" + this.colors["tradInterfaceContPromptMainColor"] + ": "
	                        + this.colors["tradInterfaceContPromptUserInputColor"];

	this.msgListOnlyOnePageContinuePrompt = "\x01n\x01c(" + this.colors["tradInterfaceContPromptHotkeyColor"] + "G\x01n\x01c)"
	                                + this.colors["tradInterfaceContPromptMainColor"] + "o, ";
	if (this.CanDelete() || this.CanDeleteLastMsg())
	{
		this.msgListOnlyOnePageContinuePrompt += "\x01n\x01c(" + this.colors["tradInterfaceContPromptHotkeyColor"]
		                                + "D\x01n\x01c)" + this.colors["tradInterfaceContPromptMainColor"] + "el, ";
	}
	if (this.CanEdit())
	{
		this.msgListOnlyOnePageContinuePrompt += "\x01n\x01c(" + this.colors["tradInterfaceContPromptHotkeyColor"]
		                                + "E\x01n\x01c)" + this.colors["tradInterfaceContPromptMainColor"] + "dit, ";
	}
	this.msgListOnlyOnePageContinuePrompt += "\x01n\x01c(" + this.colors["tradInterfaceContPromptHotkeyColor"] + "S\x01n\x01c)"
	                     + this.colors["tradInterfaceContPromptMainColor"]
	                     + "el, ";
	this.msgListOnlyOnePageContinuePrompt += "\x01n\x01c(" + this.colors["tradInterfaceContPromptHotkeyColor"] + "Q\x01n\x01c)"
	                                + this.colors["tradInterfaceContPromptMainColor"]
	                                + "uit, msg" + this.colors["tradInterfaceContPromptHotkeyColor"] + "#"
	                                + this.colors["tradInterfaceContPromptMainColor"] + ", " + this.colors["tradInterfaceContPromptHotkeyColor"]
	                                + "?" + this.colors["tradInterfaceContPromptMainColor"] + ": "
	                                + this.colors["tradInterfaceContPromptUserInputColor"];

	// Set the lightbar help text for message listing.  The @-codes are for mouse click tracking.
	// For PageUp, normally I'd think KEY_PAGEUP should work, but that triggers sending a telegram instead.  \x1b[V seems to work though.
	this.msgListLightbarModeHelpLine = this.colors.lightbarMsgListHelpLineHotkeyColor + "@CLEAR_HOT@@`" + UP_ARROW + "`" + KEY_UP + "@"
	                           + this.colors.lightbarMsgListHelpLineGeneralColor + ", "
							   + this.colors.lightbarMsgListHelpLineHotkeyColor + "@`" + DOWN_ARROW + "`" + KEY_DOWN + "@"
	                           + this.colors.lightbarMsgListHelpLineGeneralColor + ", "
							   + this.colors.lightbarMsgListHelpLineHotkeyColor + "@`PgUp`" + "\x1b[V" + "@"
	                           + this.colors.lightbarMsgListHelpLineGeneralColor + "/"
							   + this.colors.lightbarMsgListHelpLineHotkeyColor + "@`Dn`" + KEY_PAGEDN + "@"
	                           + this.colors.lightbarMsgListHelpLineGeneralColor + ", "
							   + this.colors.lightbarMsgListHelpLineHotkeyColor + "@`ENTER`" + KEY_ENTER + "@"
	                           + this.colors.lightbarMsgListHelpLineGeneralColor + ", "
							   + this.colors.lightbarMsgListHelpLineHotkeyColor + "@`HOME`" + KEY_HOME + "@"
	                           + this.colors.lightbarMsgListHelpLineGeneralColor + ", "
							   + this.colors.lightbarMsgListHelpLineHotkeyColor + "@`END`" + KEY_END + "@";
	var lbHelpLineLen = 31;
	// If the user can delete messages, then append DEL as a valid key.
	if (this.CanDelete() || this.CanDeleteLastMsg())
	{
		this.msgListLightbarModeHelpLine += this.colors.lightbarMsgListHelpLineGeneralColor + ", "
		                           + this.colors.lightbarMsgListHelpLineHotkeyColor + "@`DEL`" + KEY_DEL + "@";
		lbHelpLineLen += 5;
	}
	this.msgListLightbarModeHelpLine += this.colors.lightbarMsgListHelpLineGeneralColor
	                                 + ", " + this.colors.lightbarMsgListHelpLineHotkeyColor
	                                 + "#" + this.colors.lightbarMsgListHelpLineGeneralColor + ", ";
	lbHelpLineLen += 5;
	// If the user can edit messages, then append E as a valid key.
	if (this.CanEdit())
	{
		this.msgListLightbarModeHelpLine += this.colors.lightbarMsgListHelpLineHotkeyColor
		                           + "@`E`E@" + this.colors.lightbarMsgListHelpLineParenColor
								   + ")" + this.colors.lightbarMsgListHelpLineGeneralColor
								   + "dit, ";
		lbHelpLineLen += 7;
	}
	this.msgListLightbarModeHelpLine += this.colors.lightbarMsgListHelpLineHotkeyColor + "@`G`G@"
							   + this.colors.lightbarMsgListHelpLineParenColor + ")"
	                           + this.colors.lightbarMsgListHelpLineGeneralColor + "o, "
	                           + this.colors.lightbarMsgListHelpLineHotkeyColor + "@`Q`Q@"
							   + this.colors.lightbarMsgListHelpLineParenColor + ")"
	                           + this.colors.lightbarMsgListHelpLineGeneralColor + "uit, "
							   + this.colors.lightbarMsgListHelpLineHotkeyColor + "@`?`?@  ";
	lbHelpLineLen += 15;

	// Add spaces to the end of sLightbarModeHelpLine up until one char
	// less than the width of the screen.
	//var lbHelpLineLen = console.strlen(this.msgListLightbarModeHelpLine);
	var numChars = console.screen_columns - lbHelpLineLen - 1;
	if (numChars > 0)
	{
		// Add characters on the left and right of the line so that the
		// text is centered.
		var numLeft = Math.floor(numChars / 2);
		var numRight = numChars - numLeft;
		for (var i = 0; i < numLeft; ++i)
			this.msgListLightbarModeHelpLine = " " + this.msgListLightbarModeHelpLine;
		this.msgListLightbarModeHelpLine = "\x01n"
		                             + this.colors.lightbarMsgListHelpLineBkgColor
		                             + this.msgListLightbarModeHelpLine;
		this.msgListLightbarModeHelpLine += "\x01n" + this.colors.lightbarMsgListHelpLineBkgColor;
		for (var i = 0; i < numRight; ++i)
			this.msgListLightbarModeHelpLine += ' ';
	}
}
// For the DigDistMsgReader Class: Sets the hotkey help line for the enhanced
// reader mode
function DigDistMsgReader_SetEnhancedReaderHelpLine()
{
	// For PageUp, normally I'd think KEY_PAGEUP should work, but that triggers sending a telegram instead.  \x1b[V seems to work though.
	this.enhReadHelpLine = this.colors.enhReaderHelpLineHotkeyColor + "@CLEAR_HOT@@`" + UP_ARROW + "`" + KEY_UP + "@"
						 + this.colors.enhReaderHelpLineGeneralColor + ", "
						 + this.colors.enhReaderHelpLineHotkeyColor + "@`" + DOWN_ARROW + "`" + KEY_DOWN + "@"
						 + this.colors.enhReaderHelpLineGeneralColor + ", "
						 + this.colors.enhReaderHelpLineHotkeyColor + "@`" + LEFT_ARROW + "`" + KEY_LEFT + "@"
						 + this.colors.enhReaderHelpLineGeneralColor +", "
						 + this.colors.enhReaderHelpLineHotkeyColor + "@`" + RIGHT_ARROW + "`" + KEY_RIGHT + "@"
						 + this.colors.enhReaderHelpLineGeneralColor + ", "
						 + this.colors.enhReaderHelpLineHotkeyColor + "@`PgUp`" + "\x1b[V" + "@"
						 + this.colors.enhReaderHelpLineGeneralColor + "/"
						 + this.colors.enhReaderHelpLineHotkeyColor + "@`Dn`" + KEY_PAGEDN + "@"
						 + this.colors.enhReaderHelpLineGeneralColor + ", "
						 + this.colors.enhReaderHelpLineHotkeyColor + "@`HOME`" + KEY_HOME + "@"
						 + this.colors.enhReaderHelpLineGeneralColor + ", "
						 + this.colors.enhReaderHelpLineHotkeyColor + "@`END`" + KEY_END + "@"
						 + this.colors.enhReaderHelpLineGeneralColor + ", "
						 + this.colors.enhReaderHelpLineHotkeyColor;
	if (this.CanDelete() || this.CanDeleteLastMsg())
		this.enhReadHelpLine += "@`DEL`" + KEY_DEL + "@" + this.colors.enhReaderHelpLineGeneralColor + ", " + this.colors.enhReaderHelpLineHotkeyColor;
	if (this.CanEdit() && (console.screen_columns > 87))
		this.enhReadHelpLine += "@`E`E@" + this.colors.enhReaderHelpLineParenColor + ")" + this.colors.enhReaderHelpLineGeneralColor + "dit, " + this.colors.enhReaderHelpLineHotkeyColor;
	this.enhReadHelpLine += "@`F`F@" + this.colors.enhReaderHelpLineParenColor + ")"
						 + this.colors.enhReaderHelpLineGeneralColor + "irst, "
						 + this.colors.enhReaderHelpLineHotkeyColor + "@`L`L@" 
						 + this.colors.enhReaderHelpLineParenColor + ")"
						 + this.colors.enhReaderHelpLineGeneralColor + "ast, "
						 + this.colors.enhReaderHelpLineHotkeyColor + "@`R`R@"
						 + this.colors.enhReaderHelpLineParenColor + ")"
						 + this.colors.enhReaderHelpLineGeneralColor + "eply, "
						 + this.colors.enhReaderHelpLineHotkeyColor + "@`C`C@"
						 + this.colors.enhReaderHelpLineParenColor + ")"
						 + this.colors.enhReaderHelpLineGeneralColor + "hg area, "
						 + this.colors.enhReaderHelpLineHotkeyColor + "@`Q`Q@"
						 + this.colors.enhReaderHelpLineParenColor + ")"
						 + this.colors.enhReaderHelpLineGeneralColor + "uit, "
						 + this.colors.enhReaderHelpLineHotkeyColor + "@`?`?@";

	// Center the help text based on the console width
	var numCharsRemaining = console.screen_columns - console.strlen(this.enhReadHelpLine) - 1;
	var numCharsOnEachSide = Math.floor(numCharsRemaining/2);
	// Left side
	for (var i = 0; i < numCharsOnEachSide; ++i)
		this.enhReadHelpLine = " " + this.enhReadHelpLine;
	this.enhReadHelpLine = "\x01n" + this.colors.enhReaderHelpLineBkgColor + this.enhReadHelpLine;
	// Right side
	for (var i = 0; i < numCharsOnEachSide; ++i)
		this.enhReadHelpLine += " ";
	//numCharsRemaining = console.screen_columns - console.strlen(this.enhReadHelpLine) - 5;
	numCharsRemaining = console.screen_columns - 1 - console.strlen(this.enhReadHelpLine);
	if (numCharsRemaining > 0)
	{
		for (var i = 0; i < numCharsRemaining; ++i)
		this.enhReadHelpLine += " ";
	}

	// Create a version without the change area option
	// For PageUp, normally I'd think KEY_PAGEUP should work, but that triggers sending a telegram instead.  \x1b[V seems to work though.
	this.enhReadHelpLineWithoutChgArea = this.colors.enhReaderHelpLineHotkeyColor + "@CLEAR_HOT@  @`" + UP_ARROW + "`" + KEY_UP + "@"
									   + this.colors.enhReaderHelpLineGeneralColor + ", "
									   + this.colors.enhReaderHelpLineHotkeyColor + "@`" + DOWN_ARROW + "`" + KEY_DOWN + "@"
									   + this.colors.enhReaderHelpLineGeneralColor + ", "
									   + this.colors.enhReaderHelpLineHotkeyColor + "@`" + LEFT_ARROW + "`" + KEY_LEFT + "@"
									   + this.colors.enhReaderHelpLineGeneralColor + ", "
									   + this.colors.enhReaderHelpLineHotkeyColor + "@`" + RIGHT_ARROW + "`" + KEY_RIGHT + "@"
									   + this.colors.enhReaderHelpLineGeneralColor + ", "
									   + this.colors.enhReaderHelpLineHotkeyColor + "@`PgUp`" + "\x1b[V" + "@"
									   + this.colors.enhReaderHelpLineGeneralColor + "/"
									   + this.colors.enhReaderHelpLineHotkeyColor + "@`Dn`" + KEY_PAGEDN + "@"
									   + this.colors.enhReaderHelpLineGeneralColor + ", "
									   + this.colors.enhReaderHelpLineHotkeyColor + "@`HOME`" + KEY_HOME + "@"
									   + this.colors.enhReaderHelpLineGeneralColor + ", "
									   + this.colors.enhReaderHelpLineHotkeyColor + "@`END`" + KEY_END + "@"
									   + this.colors.enhReaderHelpLineGeneralColor + ", "
									   + this.colors.enhReaderHelpLineHotkeyColor;
	if (this.CanDelete() || this.CanDeleteLastMsg())
		this.enhReadHelpLineWithoutChgArea += "@`DEL`" + KEY_DEL + "@" + this.colors.enhReaderHelpLineGeneralColor + ", " + this.colors.enhReaderHelpLineHotkeyColor;
	if (this.CanEdit())
		this.enhReadHelpLineWithoutChgArea += "@`E`E@" + this.colors.enhReaderHelpLineParenColor + ")" + this.colors.enhReaderHelpLineGeneralColor + "dit, " + this.colors.enhReaderHelpLineHotkeyColor;
	this.enhReadHelpLineWithoutChgArea += "@`F`F@" + this.colors.enhReaderHelpLineParenColor + ")"
									   + this.colors.enhReaderHelpLineGeneralColor + "irst, "
									   + this.colors.enhReaderHelpLineHotkeyColor + "@`L`L@"
									   + this.colors.enhReaderHelpLineParenColor + ")"
									   + this.colors.enhReaderHelpLineGeneralColor + "ast, "
									   + this.colors.enhReaderHelpLineHotkeyColor + "@`R`R@"
									   + this.colors.enhReaderHelpLineParenColor + ")"
									   + this.colors.enhReaderHelpLineGeneralColor + "eply, "
									   + this.colors.enhReaderHelpLineHotkeyColor + "@`Q`Q@"
									   + this.colors.enhReaderHelpLineParenColor + ")"
									   + this.colors.enhReaderHelpLineGeneralColor + "uit, "
									   + this.colors.enhReaderHelpLineHotkeyColor + "@`?`?@  ";

	// Center the help text based on the console width
	numCharsRemaining = console.screen_columns - console.strlen(this.enhReadHelpLineWithoutChgArea) - 2;
	numCharsOnEachSide = Math.floor(numCharsRemaining/2);
	// Left side
	for (var i = 0; i < numCharsOnEachSide; ++i)
		this.enhReadHelpLineWithoutChgArea = " " + this.enhReadHelpLineWithoutChgArea;
	this.enhReadHelpLineWithoutChgArea = "\x01n" + this.colors.enhReaderHelpLineBkgColor + this.enhReadHelpLineWithoutChgArea;
	// Right side
	for (var i = 0; i < numCharsOnEachSide; ++i)
		this.enhReadHelpLineWithoutChgArea += " ";
	numCharsRemaining = console.screen_columns - console.strlen(this.enhReadHelpLineWithoutChgArea) - 1;
	if (numCharsRemaining > 0)
	{
		for (var i = 0; i < numCharsRemaining; ++i)
		this.enhReadHelpLineWithoutChgArea += " ";
	}
}
function stripCtrlFromEnhReadHelpLine_ReplaceArrowChars(pHelpLine)
{
	var helpLineNoAttrs = strip_ctrl(pHelpLine);
	var charsToPutBack = [UP_ARROW, DOWN_ARROW, LEFT_ARROW, RIGHT_ARROW];
	var helpLineIdx = -1;
	for (var i = 0; i < charsToPutBack.length; ++i)
	{
		helpLineIdx = helpLineNoAttrs.indexOf(",", helpLineIdx+1);
		if (helpLineIdx > -1)
		{
			helpLineNoAttrs = helpLineNoAttrs.substr(0, helpLineIdx) + charsToPutBack[i] + helpLineNoAttrs.substr(helpLineIdx);
			++helpLineIdx;
		}
	}
	return helpLineNoAttrs;
}
// For the DigDistMsgReader class: Reads the configuration file (by default,
// DDMsgReader.cfg) and sets the object properties
// accordingly.
function DigDistMsgReader_ReadConfigFile()
{
	this.cfgFileSuccessfullyRead = false;

	var themeFilename = ""; // In case a theme filename is specified

	// Open the main configuration file.  First look for it in the sbbs/mods
	// directory, then sbbs/ctrl, then in the same directory as this script.
	var cfgFilename = file_cfgname(system.mods_dir, this.cfgFilename);
	if (!file_exists(cfgFilename))
		cfgFilename = file_cfgname(system.ctrl_dir, this.cfgFilename);
	if (!file_exists(cfgFilename))
		cfgFilename = file_cfgname(gStartupPath, this.cfgFilename);
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
		var valueUpper = null;   // Upper-cased value for a setting (string)
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
				valueUpper = value.toUpperCase();

				// Set the appropriate valueUpper in the settings object.
				if (settingUpper == "LISTINTERFACESTYLE")
					this.msgListUseLightbarListInterface = (valueUpper == "LIGHTBAR");
				else if (settingUpper == "READERINTERFACESTYLE")
					this.scrollingReaderInterface = (valueUpper == "SCROLLABLE");
				else if (settingUpper == "READERINTERFACESTYLEFORANSIMESSAGES")
					this.useScrollingInterfaceForANSIMessages = (valueUpper == "SCROLLABLE");
				else if (settingUpper == "DISPLAYBOARDINFOINHEADER")
					this.displayBoardInfoInHeader = (valueUpper == "TRUE");
				// Note: this.reverseListOrder can be true, false, or "ASK"
				else if (settingUpper == "REVERSELISTORDER")
					this.reverseListOrder = (valueUpper == "ASK" ? "ASK" : (valueUpper == "TRUE"));
				else if (settingUpper == "PROMPTTOCONTINUELISTINGMESSAGES")
					this.promptToContinueListingMessages = (valueUpper == "TRUE");
				else if (settingUpper == "PROMPTCONFIRMREADMESSAGE")
					this.promptToReadMessage = (valueUpper == "TRUE");
				else if (settingUpper == "MSGLISTDISPLAYTIME")
					this.msgList_displayMessageDateImported = (valueUpper == "IMPORTED");
				else if (settingUpper == "MSGAREALIST_LASTIMPORTEDMSG_TIME")
					this.msgAreaList_lastImportedMsg_showImportTime = (valueUpper == "IMPORTED");
				else if (settingUpper == "STARTMODE")
				{
					if ((valueUpper == "READER") || (valueUpper == "READ"))
						this.startMode = READER_MODE_READ;
					else if ((valueUpper == "LISTER") || (valueUpper == "LIST"))
						this.startMode = READER_MODE_LIST;
				}
				else if (settingUpper == "TABSPACES")
				{
					var numSpaces = +value;
					// If greater than 0, then set this.numTabSpaces
					if (numSpaces > 0)
						this.numTabSpaces = numSpaces;
				}
				else if (settingUpper == "PAUSEAFTERNEWMSGSCAN")
					this.pauseAfterNewMsgScan = (valueUpper == "TRUE");
				else if (settingUpper == "READINGPOSTONSUBBOARDINSTEADOFGOTONEXT")
					this.readingPostOnSubBoardInsteadOfGoToNext = (valueUpper == "TRUE");
				else if (settingUpper == "AREACHOOSERHDRFILENAMEBASE")
					this.areaChooserHdrFilenameBase = value;
				else if (settingUpper == "AREACHOOSERHDRMAXLINES")
				{
					var maxNumLines = +value;
					if (maxNumLines > 0)
						this.areaChooserHdrMaxLines = maxNumLines;
				}
				else if (settingUpper == "THEMEFILENAME")
				{
					// First look for the theme config file in the sbbs/mods
					// directory, then sbbs/ctrl, then the same directory as
					// this script.
					themeFilename = system.mods_dir + value;
					if (!file_exists(themeFilename))
						themeFilename = system.ctrl_dir + value;
					if (!file_exists(themeFilename))
						themeFilename = gStartupPath + value;
				}
				else if (settingUpper == "DISPLAYAVATARS")
					this.displayAvatars = (valueUpper == "TRUE");
				else if (settingUpper == "RIGHTJUSTIFYAVATARS")
					this.rightJustifyAvatar = (valueUpper == "TRUE");
				else if (settingUpper == "MSGLISTSORT")
				{
					if (valueUpper == "WRITTEN")
						this.msgListSort = MSG_LIST_SORT_DATETIME_WRITTEN;
				}
				else if (settingUpper == "CONVERTYSTYLEMCIATTRSTOSYNC")
					this.convertYStyleMCIAttrsToSync = (valueUpper == "TRUE");
			}
		}

		cfgFile.close();
	}
	else
	{
		// Was unable to read the configuration file.  Output a warning to the user
		// that defaults will be used and to notify the sysop.
		console.print("\x01n");
		console.crlf();
		console.print("\x01w\x01hUnable to open the configuration file: \x01y" + this.cfgFilename);
		console.crlf();
		console.print("\x01wDefault settings will be used.  Please notify the sysop.");
		mswait(2000);
	}
	
	// If a theme filename was specified, then read the colors & strings
	// from it.
	if (themeFilename.length > 0)
	{
		var themeFile = new File(themeFilename);
		if (themeFile.open("r"))
		{
			var fileLine = null;     // A line read from the file
			var equalsPos = 0;       // Position of a = in the line
			var commentPos = 0;      // Position of the start of a comment
			var setting = null;      // A setting name (string)
			var value = null;        // To store a value for a setting (string)
			var onlySyncAttrsRegexWholeWord = new RegExp("^(\x01[krgybmcw01234567hinpq,;\.dtl<>\[\]asz])+$", 'i');
			while (!themeFile.eof)
			{
				// Read the next line from the config file.
				fileLine = themeFile.readln(2048);

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
					// Read the setting (without leading/trailing spaces) & value
					setting = trimSpaces(fileLine.substr(0, equalsPos), true, false, true);
					value = fileLine.substr(equalsPos+1);

					// Colors
					if ((setting == "msgListHeaderMsgGroupTextColor") || (setting == "msgListHeaderMsgGroupNameColor") ||
					    (setting == "msgListHeaderSubBoardTextColor") || (setting == "msgListHeaderMsgSubBoardName") ||
					    (setting == "msgListColHeader") ||
					    (setting == "msgListMsgNumColor") || (setting == "msgListFromColor") ||
						(setting == "msgListToColor") || (setting == "msgListSubjectColor") ||
						(setting == "msgListDateColor") || (setting == "msgListTimeColor") ||
					    (setting == "msgListToUserMsgNumColor") || (setting == "msgListToUserFromColor") ||
					    (setting == "msgListToUserToColor") || (setting == "msgListToUserSubjectColor") ||
					    (setting == "msgListToUserDateColor") || (setting == "msgListToUserTimeColor") ||
					    (setting == "msgListFromUserMsgNumColor") || (setting == "msgListFromUserFromColor") ||
					    (setting == "msgListFromUserToColor") || (setting == "msgListFromUserSubjectColor") ||
					    (setting == "msgListFromUserDateColor") || (setting == "msgListFromUserTimeColor") ||
					    (setting == "msgListHighlightBkgColor") || (setting == "msgListMsgNumHighlightColor") ||
					    (setting == "msgListFromHighlightColor") || (setting == "msgListToHighlightColor") ||
					    (setting == "msgListSubjHighlightColor") || (setting == "msgListDateHighlightColor") ||
					    (setting == "msgListTimeHighlightColor") || (setting == "lightbarMsgListHelpLineBkgColor") ||
					    (setting == "lightbarMsgListHelpLineGeneralColor") || (setting == "lightbarMsgListHelpLineHotkeyColor") ||
					    (setting == "lightbarMsgListHelpLineParenColor") || (setting == "tradInterfaceContPromptMainColor") ||
					    (setting == "tradInterfaceContPromptHotkeyColor") || (setting == "tradInterfaceContPromptUserInputColor") ||
					    (setting == "msgBodyColor") || (setting == "readMsgConfirmColor") ||
					    (setting == "readMsgConfirmNumberColor") ||
					    (setting == "afterReadMsg_ListMorePromptColor") ||
					    (setting == "tradInterfaceHelpScreenColor") || (setting == "areaChooserMsgAreaNumColor") ||
					    (setting == "areaChooserMsgAreaDescColor") || (setting == "areaChooserMsgAreaNumItemsColor") ||
					    (setting == "areaChooserMsgAreaHeaderColor") || (setting == "areaChooserSubBoardHeaderColor") ||
					    (setting == "areaChooserMsgAreaMarkColor") || (setting == "areaChooserMsgAreaLatestDateColor") ||
					    (setting == "areaChooserMsgAreaLatestTimeColor") || (setting == "areaChooserMsgAreaBkgHighlightColor") ||
					    (setting == "areaChooserMsgAreaNumHighlightColor") || (setting == "areaChooserMsgAreaDescHighlightColor") ||
					    (setting == "areaChooserMsgAreaDateHighlightColor") || (setting == "areaChooserMsgAreaTimeHighlightColor") ||
					    (setting == "areaChooserMsgAreaNumItemsHighlightColor") || (setting == "lightbarAreaChooserHelpLineBkgColor") ||
					    (setting == "lightbarAreaChooserHelpLineGeneralColor") || (setting == "lightbarAreaChooserHelpLineHotkeyColor") ||
					    (setting == "lightbarAreaChooserHelpLineParenColor") || (setting == "scrollbarBGColor") ||
					    (setting == "scrollbarScrollBlockColor") || (setting == "enhReaderPromptSepLineColor") ||
					    (setting == "enhReaderHelpLineBkgColor") || (setting == "enhReaderHelpLineGeneralColor") ||
					    (setting == "enhReaderHelpLineHotkeyColor") || (setting == "enhReaderHelpLineParenColor") ||
					    (setting == "hdrLineLabelColor") || (setting == "hdrLineValueColor") ||
					    (setting == "selectedMsgMarkColor") || (setting ==  "msgListScoreColor") ||
					    (setting == "msgListToUserScoreColor") || (setting == "msgListFromUserScoreColor") ||
					    (setting == "msgListScoreHighlightColor") || (setting == "msgHdrMsgNumColor") ||
					    (setting == "msgHdrFromColor") || (setting == "msgHdrToColor") ||
					    (setting == "msgHdrToUserColor") || (setting == "msgHdrSubjColor") ||
					    (setting == "msgHdrDateColor"))
					{
						// Trim leading & trailing spaces from the value when
						// setting a color.  Also, replace any instances of "\x01"
						// with the Synchronet attribute control character.
						if (onlySyncAttrsRegexWholeWord.test(value))
							this.colors[setting] = trimSpaces(value, true, false, true).replace(/\\x01/g, "\x01");
					}
					// Text values
					else if ((setting == "scrollbarBGChar") ||
					         (setting == "scrollbarScrollBlockChar") ||
					         (setting == "goToPrevMsgAreaPromptText") ||
					         (setting == "goToNextMsgAreaPromptText") ||
					         (setting == "newMsgScanText") ||
					         (setting == "newToYouMsgScanText") ||
					         (setting == "allToYouMsgScanText") ||
					         (setting == "goToMsgNumPromptText") ||
					         (setting == "msgScanCompleteText") ||
					         (setting == "msgScanAbortedText") ||
					         (setting == "deleteMsgNumPromptText") ||
					         (setting == "editMsgNumPromptText") ||
					         (setting == "noMessagesInSubBoardText") ||
					         (setting == "noSearchResultsInSubBoardText") ||
					         (setting == "invalidMsgNumText") ||
					         (setting == "readMsgNumPromptText") ||
							 (setting == "msgHasBeenDeletedText") ||
					         (setting == "noKludgeLinesForThisMsgText") ||
							 (setting == "searchingPersonalMailText") ||
					         (setting == "searchingSubBoardAbovePromptText") ||
					         (setting == "searchingSubBoardText") ||
							 (setting == "searchTextPromptText") ||
							 (setting == "fromNamePromptText") ||
							 (setting == "toNamePromptText") ||
							 (setting == "abortedText") ||
							 (setting == "loadingPersonalMailText") ||
							 (setting == "msgDelConfirmText") ||
							 (setting == "msgUndelConfirmText") ||
							 (setting == "selectedMsgsUndeletedText") ||
							 (setting == "msgDeletedText") ||
							 (setting == "msgUndeletedText") ||
							 (setting == "cannotDeleteMsgText_notYoursNotASysop") ||
							 (setting == "cannotDeleteMsgText_notLastPostedMsg") ||
							 (setting == "msgEditConfirmText") ||
							 (setting == "noPersonalEmailText") ||
							 (setting == "postOnSubBoard"))
					{
						// Replace any instances of "\x01" with the Synchronet
						// attribute control character
						this.text[setting] = value.replace(/\\x01/g, "\x01");
					}
				}
			}

			themeFile.close();

			// Ensure that scrollbarBGChar and scrollbarScrollBlockChar are
			// only one character.  If they're longer, use only the first
			// character.
			if (this.text.scrollbarBGChar.length > 1)
				this.text.scrollbarBGChar = this.text.scrollbarBGChar.substr(0, 1);
			if (this.text.scrollbarScrollBlockChar.length > 1)
				this.text.scrollbarScrollBlockChar = this.text.scrollbarScrollBlockChar.substr(0, 1);
		}
		else
		{
			// Was unable to read the theme file.  Output a warning to the user
			// that defaults will be used and to notify the sysop.
			this.cfgFileSuccessfullyRead = false;
			console.print("\x01n");
			console.crlf();
			console.print("\x01w\x01hUnable to open the theme file: \x01y" + themeFilename);
			console.crlf();
			console.print("\x01wDefault settings will be used.  Please notify the sysop.");
			mswait(2000);
		}
	}
}
// For the DigDistMsgReader class: Reads the user settings file
//
// Parameters:
//  pOnlyTwitlist: Optional boolean - Whether or not to only read the user's twitlist. Defaults to false.
function DigDistMsgReader_ReadUserSettingsFile(pOnlyTwitlist)
{
	var onlyTwitList = (typeof(pOnlyTwitlist) === "boolean" ? pOnlyTwitlist : false);
	// Open the user's personal twit list file, if it exists
	var userTwitlistFile = new File(gUserTwitListFilename);
	if (userTwitlistFile.open("r"))
	{
		while (!userTwitlistFile.eof)
		{
			// Read the next line from the config file.
			var fileLine = userTwitlistFile.readln(2048);

			// fileLine should be a string, but I've seen some cases
			// where for some reason it isn't.  If it's not a string,
			// then continue onto the next line.
			if (typeof(fileLine) != "string")
				continue;

			// If the line starts with with a semicolon (the comment
			// character) or is blank, then skip it.
			if ((fileLine.substr(0, 1) == ";") || (fileLine.length == 0))
				continue;

			// In case there are any commas, split on commas. Add all names to the user's twitlist
			var names = fileLine.split(",");
			for (var i = 0; i < names.length; ++i)
			{
				var twitListNameEntry = names[i].trim().toLowerCase(); // Lowercase for case-insensitive comparisons
				if (twitListNameEntry.length > 0)
					this.userSettings.twitList.push(twitListNameEntry);
			}
		}
		userTwitlistFile.close();
	}

	if (!onlyTwitList)
	{
		// Open the user settings file, if it exists
		var userSettingsFile = new File(gUserSettingsFilename);
		if (userSettingsFile.open("r"))
		{
			//var behavior = userSettingsFile.iniGetObject("BEHAVIOR");
			this.useEnhReaderScrollbar = userSettingsFile.iniGetValue("BEHAVIOR", "useEnhReaderScrollbar", true);
			userSettingsFile.close();
		}
	}
}

// For the DigDistMessageReader class: Writes the user settings file.
//
// Return value: Boolean - Whether or not the write succeeded
function DigDistMsgReader_WriteUserSettingsFile()
{
	var writeSucceeded = false;
	// Open the user settings file, if it exists
	var userSettingsFile = new File(gUserSettingsFilename);
	if (userSettingsFile.open(userSettingsFile.exists ? "r+" : "w+"))
	{
		userSettingsFile.iniSetValue("BEHAVIOR", "useEnhReaderScrollbar", this.useEnhReaderScrollbar);
		userSettingsFile.close();
		writeSucceeded = true;
	}
	return writeSucceeded;
}

// For the DigDistMsgReader class: Lets the user edit an existing message.
//
// Parameters:
//  pMsgIndex: The index of the message to edit
//
// Return value: An object with the following parameters:
//               userCannotEdit: Boolean - True if the user can't edit, false if they can
//               userConfirmed: Boolean - Whether or not the user confirmed editing
//               msgEdited: Boolean - Whether or not the message was edited
//               newMsgIdx: The index (offset) of the new (edited) message that was saved.
//                          If the message wasn't edited/saved, this will be -1.
function DigDistMsgReader_EditExistingMsg(pMsgIndex)
{
	var returnObj = {
		userCannotEdit: false,
		userConfirmed: false,
		msgEdited: false,
		newMsgIdx: -1
	};

	// Open the sub-board
	var msgbase = new MsgBase(this.subBoardCode);
	if (!msgbase.open())
	{
		console.print("\x01n\x01h\x01wCan't open the sub-board\x01n");
		console.crlf();
		console.pause();
		return returnObj;
	}

	// Only let the user edit the message if they're a sysop or
	// if they wrote the message.
	var msgHeader = this.GetMsgHdrByIdx(pMsgIndex, false, msgbase);
	if (!user.is_sysop && (msgHeader.from != user.name) && (msgHeader.from != user.alias) && (msgHeader.from != user.handle))
	{
		console.print("\x01n\x01h\x01wCannot edit message #\x01y" + +(pMsgIndex+1) +
		              " \x01wbecause it's not yours or you're not a sysop.");
		console.crlf();
		console.pause();
		returnObj.userCannotEdit = true;
		return returnObj;
	}

	// Confirm the action with the user (default to no).
	returnObj.userConfirmed = !console.noyes(replaceAtCodesInStr(format(this.text.msgEditConfirmText, +(pMsgIndex+1))));
	if (!returnObj.userConfirmed)
	{
		msgbase.close();
		return returnObj;
	}

	// Make use of bbs.edit_msg() if the function exists (it was added in
	// Synchronet 3.18c).  Otherwise, edit the old way.
	if (typeof(bbs.edit_msg) === "function")
	{
		if (!bbs.edit_msg(msgHeader))
		{
			var grpIdx = msg_area.sub[this.subBoardCode].grp_index;
			var areaDesc = msg_area.grp_list[grpIdx].description + " - " + msg_area.sub[this.subBoardCode].description;
			var logMsg = user.alias + " was unable to edit message number " + msgHeader.number + " in " + areaDesc;
			log(LOG_ERROR, logMsg);
			bbs.log_str(logMsg);
		}
	}
	else
		this.EditExistingMessageOldWay(msgbase, msgHeader, pMsgIndex);

	msgbase.close();

	return returnObj;
}
// Helper for DigDistMsgReader_EditExistingMsg(): Edits an existing message by writing it
// to a temporary file, having the user edit that, and saving it as a new message.
// This was done before the bbs.edit_msg() function existed (it was added in Synchronet
// 3.18c).
//
// Parameters:
//  pMsgbase: The MessageBase object.  Assumed to be open.
//  pOrigMsgHdr: The header of the original message
//  pMsgIndex: The index of the message to edit
function DigDistMsgReader_EditExistingMessageOldWay(pMsgbase, pOrigMsgHdr, pMsgIndex)
{
	// Dump the message body to a temporary file in the node dir
	//var originalMsgBody = pMsgbase.get_msg_body(true, pMsgIndex, false, false, true, true);
	var originalMsgBody;
	var tmpMsgHdr = this.GetMsgHdrByIdx(pMsgIndex, false, pMsgbase);
	var msgHdrIsBogus = (tmpMsgHdr.hasOwnProperty("isBogus") ? tmpMsgHdr.isBogus : false);
	if (msgHdrIsBogus)
		originalMsgBody = pMsgbase.get_msg_body(true, pMsgIndex, false, false, true, true);
	else
		originalMsgBody = pMsgbase.get_msg_body(false, tmpMsgHdr.number, false, false, true, true);
	var tempFilename = system.node_dir + "DDMsgLister_message.txt";
	var tmpFile = new File(tempFilename);
	if (tmpFile.open("w"))
	{
		var wroteToTempFile = tmpFile.write(word_wrap(originalMsgBody, 79));
		tmpFile.close();
		// If we were able to write to the temp file, then let the user
		// edit the file.
		if (wroteToTempFile)
		{
			// The following lines set some attributes in the bbs object
			// in an attempt to make the "To" name and subject appear
			// correct in the editor.
			// TODO: On May 14, 2013, Digital Man said bbs.msg_offset will
			// probably be removed because it doesn't provide any benefit.
			// bbs.msg_number is a unique message identifier that won't
			// change, so it's probably best for scripts to use bbs.msg_number
			// instead of offsets.
			bbs.msg_to = pOrigMsgHdr.to;
			bbs.msg_to_ext = pOrigMsgHdr.to_ext;
			bbs.msg_subject = pOrigMsgHdr.subject;
			bbs.msg_offset = pOrigMsgHdr.offset;
			bbs.msg_number = pOrigMsgHdr.number;

			// Let the user edit the temporary file
			console.editfile(tempFilename);
			// Load the temp file back into msgBodyColor and have pMsgbase
			// save the message.
			if (tmpFile.open("r"))
			{
				var newMsgBody = tmpFile.read();
				tmpFile.close();
				// If the new message body is different from the original message
				// body, then go ahead and save the message and mark the original
				// message for deletion. (Checking the new & original message
				// bodies seems to be the only way to check to see if the user
				// aborted out of the message editor.)
				if (newMsgBody != originalMsgBody)
				{
					var newHdr = { to: pOrigMsgHdr.to, to_ext: pOrigMsgHdr.to_ext, from: pOrigMsgHdr.from,
					               from_ext: pOrigMsgHdr.from_ext, attr: pOrigMsgHdr.attr,
					               subject: pOrigMsgHdr.subject };
					var savedNewMsg = pMsgbase.save_msg(newHdr, newMsgBody);
					// If the message was successfully saved, then mark the original
					// message for deletion and output a message to the user.
					if (savedNewMsg)
					{
						returnObj.msgEdited = true;
						returnObj.newMsgIdx = pMsgbase.total_msgs - 1;
						var message = "\x01n\x01cThe edited message has been saved as a new message.";
						if (pMsgbase.remove_msg(true, pMsgIndex))
							message += "  The original has been\r\nmarked for deletion.";
						else
							message += "  \x01h\x01yHowever, the original\r\ncould not be marked for deletion.";
						message += "\r\n\x01p";
						console.print(message);
					}
					else
						console.print("\r\n\x01n\x01h\x01yError: \x01wFailed to save the new message\r\n\x01p");
				}
			}
			else
			{
				console.print("\r\n\x01n\x01h\x01yError: \x01wUnable to read the temporary file\r\n");
				console.print("Filename: \x01b" + tempFilename + "\r\n");
				console.pause();
			}
		}
		else
		{
			console.print("\r\n\x01n\x01h\x01yError: \x01wUnable to write to temporary file\r\n");
			console.print("Filename: \x01b" + tempFilename + "\r\n");
			console.pause();
		}
	}
	else
	{
		console.print("\r\n\x01n\x01h\x01yError: \x01wUnable to open a temporary file for writing\r\n");
		console.print("Filename: \x01b" + tempFilename + "\r\n");
		console.pause();
	}
	// Delete the temporary file from disk.
	tmpFile.remove();
}

// For the DigDistMsgReader Class: Returns whether or not the user can delete
// their messages in the sub-board (distinct from being able to delete only
// their last message).
function DigDistMsgReader_CanDelete()
{
	var canDelete = user.is_sysop || this.readingPersonalEmail;
	var msgbase = new MsgBase(this.subBoardCode);
	if (msgbase.open())
	{
		if (msgbase.cfg != null)
			canDelete = canDelete || ((msgbase.cfg.settings & SUB_DEL) == SUB_DEL);
		msgbase.close();
	}
	return canDelete;
}
// For the DigDistMsgReader Class: Returns whether or not the user can delete
// the last message they posted in the sub-board.
function DigDistMsgReader_CanDeleteLastMsg()
{
	var canDelete = user.is_sysop;
	var msgbase = new MsgBase(this.subBoardCode);
	if (msgbase.open())
	{
		if (msgbase.cfg != null)
			canDelete = canDelete || ((msgbase.cfg.settings & SUB_DELLAST) == SUB_DELLAST);
		msgbase.close();
	}
	return canDelete;
}
// For the DigDistMsgReader Class: Returns whether or not the user can edit
// messages.
function DigDistMsgReader_CanEdit()
{
	var canEdit = user.is_sysop;
	var msgbase = new MsgBase(this.subBoardCode);
	if (msgbase.open())
	{
		if (msgbase.cfg != null)
			canEdit = canEdit || ((msgbase.cfg.settings & SUB_EDIT) == SUB_EDIT);
		msgbase.close();
	}
	return canEdit;
}
// For the DigDistMsgReader Class: Returns whether or not message quoting
// is enabled.
function DigDistMsgReader_CanQuote()
{
	var canQuote = this.readingPersonalEmail || user.is_sysop;
	var msgbase = new MsgBase(this.subBoardCode);
	if (msgbase.open())
	{
		if (msgbase.cfg != null)
			canQuote = canQuote || ((msgbase.cfg.settings & SUB_QUOTE) == SUB_QUOTE);
		msgbase.close();
	}
	return canQuote;
}

// For the DigDistMsgReader Class: Displays the stock Synchronet message header file for
// a given message header.
//
// Parameters:
//  pMsgHdr: The message header object
function DigDistMsgReader_DisplaySyncMsgHeader(pMsgHdr)
{
	if ((pMsgHdr == null) || (typeof(pMsgHdr) != "object"))
		return;

	// Note: The message header has the following fields:
	// 'number': The message number
	// 'offset': The message offset
	// 'to': Who the message is directed to (string)
	// 'from' Who wrote the message (string)
	// 'subject': The message subject (string)
	// 'date': The date - Full text (string)

	// Generate a string containing the message's import date & time.
	//var dateTimeStr = strftime("%Y-%m-%d %H:%M:%S", msgHeader.when_imported_time)
	// Use the date text in the message header, without the time
	// zone offset at the end.
	var dateTimeStr = pMsgHdr["date"].replace(/ [-+][0-9]+$/, "");

	// Check to see if there is a msghdr file in the sbbs/text/menu
	// directory.  If there is, then use it to display the message
	// header information.  Otherwise, output a default message header.
	var msgHdrFileOpened = false;
	var msgHdrFilename = this.GetMsgHdrFilenameFull();
	if (msgHdrFilename.length > 0)
	{
		var msgHdrFile = new File(msgHdrFilename);
		if (msgHdrFile.open("r"))
		{
			msgHdrFileOpened = true;
			var fileLine = null; // To store a line read from the file
			while (!msgHdrFile.eof)
			{
				// Read the next line from the header file
				fileLine = msgHdrFile.readln(2048);

				// fileLine should be a string, but I've seen some cases
				// where it isn't, so check its type.
				if (typeof(fileLine) != "string")
					continue;

				// Since we're displaying the message information ouside of Synchronet's
				// message read prompt, this script now has to parse & replace some of
				// the @-codes in the message header line, since Synchronet doesn't know
				// that the user is reading a message.
				console.putmsg(this.ParseMsgAtCodes(fileLine, pMsgHdr, null, dateTimeStr, false, true));
				console.crlf();
			}
			msgHdrFile.close();
		}
	}

	// If the msghdr file didn't open (or doesn't exist), then output the default
	// header.
	if (!msgHdrFileOpened)
	{
		// Generate a string describing the message attributes, then output the default
		// header.
		var allMsgAttrStr = makeAllMsgAttrStr(pMsgHdr);
		console.print("\x01n\x01w" + charStr(HORIZONTAL_DOUBLE, 78));
		console.crlf();
		var horizSingleFive = charStr(HORIZONTAL_SINGLE, 5);
		console.print("\x01n\x01w" + horizSingleFive + "\x01cFrom\x01w\x01h: \x01b" + pMsgHdr["from"].substr(0, console.screen_columns-12));
		console.crlf();
		console.print("\x01n\x01w" + horizSingleFive + "\x01cTo  \x01w\x01h: \x01b" + pMsgHdr["to"].substr(0, console.screen_columns-12));
		console.crlf();
		console.print("\x01n\x01w" + horizSingleFive + "\x01cSubj\x01w\x01h: \x01b" + pMsgHdr["subject"].substr(0, console.screen_columns-12));
		console.crlf();
		console.print("\x01n\x01w" + horizSingleFive + "\x01cDate\x01w\x01h: \x01b" + dateTimeStr.substr(0, console.screen_columns-12));
		console.crlf();
		console.print("\x01n\x01w" + horizSingleFive + "\x01cAttr\x01w\x01h: \x01b" + allMsgAttrStr.substr(0, console.screen_columns-12));
		console.crlf();
	}
}

// For the DigDistMsgReader class: Returns the name of the msghdr file in the
// sbbs/text/menu directory. If the user's terminal supports ANSI, this first
// checks to see if an .ans version exists.  Otherwise, checks to see if an
// .asc version exists.  If neither are found, this function will return an
// empty string.
function DigDistMsgReader_GetMsgHdrFilenameFull()
{
  // If the user's terminal supports ANSI and msghdr.ans exists
  // in the text/menu directory, then use that one.  Otherwise,
  // if msghdr.asc exists, then use that one.
  var ansiFileName = "menu/msghdr.ans";
  var asciiFileName = "menu/msghdr.asc";
  var msgHdrFilename = "";
  if (console.term_supports(USER_ANSI) && file_exists(system.text_dir + ansiFileName))
    msgHdrFilename = system.text_dir + ansiFileName;
  else if (file_exists(system.text_dir + asciiFileName))
    msgHdrFilename = system.text_dir + asciiFileName;
  return msgHdrFilename;
}

// For the DigDistMsgReader class: Returns the number of messages in the current
// sub-board.  This will be either the number of headers in this.msgSearchHdrs
// for the current sub-board (if non-empty and a search type specified) or
// msgbase.total_msgs.
//
// Parameters:
//  pMsgbase: Optional - A MessageBase object
//  pCheckDeletedAttributes: Optional boolean - Whether or not to check the
//                           'deleted' attributes of the messages and not
//                           count deleted messages.  Defaults to false.
//
// Return value: The number of messages
function DigDistMsgReader_NumMessages(pMsgbase, pCheckDeletedAttributes)
{
	var checkDeletedAttributes = (typeof(pCheckDeletedAttributes) == "boolean" ? pCheckDeletedAttributes : false);

	var msgbase = null;
	var closeMsgbaseInThisFunc = false;
	if ((pMsgbase != null) && (typeof(pMsgbase) == "object"))
		msgbase = pMsgbase;
	else
	{
		msgbase = new MsgBase(this.subBoardCode);
		if (msgbase.open())
			closeMsgbaseInThisFunc = true;
		else
			return 0;
	}

	var numMsgs = 0;
	if (this.SearchingAndResultObjsDefinedForCurSub())
		numMsgs = this.msgSearchHdrs[this.subBoardCode].indexed.length;
	else if (this.hdrsForCurrentSubBoard.length > 0)
		numMsgs = this.hdrsForCurrentSubBoard.length;
	else if ((msgbase != null) && msgbase.is_open)
	{
		//numMsgs = msgbase.total_msgs;
		// Count the number of readable messages in the messagebase (i.e.,
		// messages that are not deleted, unvalidated, or null headers)
		numMsgs = 0;
		var totalNumMsgs = msgbase.total_msgs;
		for (var msgIdx = 0; msgIdx < totalNumMsgs; ++msgIdx)
		{
			if (isReadableMsgHdr(msgbase.get_msg_header(true, msgIdx, false), this.subBoardCode))
				++numMsgs;
		}
	}

	// If the caller wants to check the deleted attributes, then do so.
	if ((numMsgs > 0) && checkDeletedAttributes)
	{
		var msgHdr;
		var originalNumMsgs = numMsgs;
		for (var msgIdx = 0; msgIdx < originalNumMsgs; ++msgIdx)
		{
			msgHdr = this.GetMsgHdrByIdx(msgIdx);
			if ((msgHdr.attr & MSG_DELETE) == MSG_DELETE)
			{
				--numMsgs;
				if (numMsgs < 0)
				{
					numMsgs = 0;
					break;
				}
			}
		}
	}

	if (closeMsgbaseInThisFunc)
		msgbase.close();

	return numMsgs;
}

// For the DigDistMsgReader class: Returns whether there are any non-deleted
// messages in the current sub-board.
//
// Return value: Boolean - Whether or not there are any non-deleted messages
//               in the current sub-board.
function DigDistMsgReader_NonDeletedMessagesExist()
{
	var messagesExist = false;

	var numMsgs = this.NumMessages();
	if (numMsgs > 0)
	{
		var msgHdr;
		for (var msgIdx = 0; (msgIdx < numMsgs) && !messagesExist; ++msgIdx)
		{
			msgHdr = this.GetMsgHdrByIdx(msgIdx);
			if ((msgHdr.attr & MSG_DELETE) == 0)
			{
				messagesExist = true;
				break;
			}
		}
	}

	return messagesExist;
}

// For the DigDistMsgReader class: Returns the highest message number (1-based), either from this.msgSearchHdrs
// (if it has search results for the current sub-board) or msgbase.  If
// there are no search results for the current sub-board in this.msgSearchHdrs,
// the highest message number is the same as the total number of messages
// in the sub-board (unless the Synchronet standard ever changes..).
function DigDistMsgReader_HighestMessageNum()
{
	var highestMessageNum = 0;
	var msgbase = new MsgBase(this.subBoardCode);
	if (msgbase.open())
	{
		highestMessageNum = msgbase.total_msgs;
		msgbase.close();
	}
	if (this.msgSearchHdrs.hasOwnProperty(this.subBoardCode) && (this.msgSearchHdrs[this.subBoardCode].indexed.length > 0))
		highestMessageNum = this.msgSearchHdrs[this.subBoardCode].indexed.length;
	return highestMessageNum;
}

// For the DigDistMsgReader class: Returns whether or not a message number (1-based)
// is a valid and existing message number.  This is intended for validating user
// input where not all the message numbers are consecutive.
//
// Parameters:
//  pMsgNum: The message number to validate
//
// Return value: Boolean - Whether or not the message number 
function DigDistMsgReader_IsValidMessageNum(pMsgNum)
{
	// The message numbers start at 1
	if (pMsgNum < 1)
		return false;
	// If there are search results for the current sub-board, then check to see if
	// the message number exists in its indexed array.  Otherwise, check with
	// msgbase.
	var msgNumIsValid = false;
	if (this.SearchingAndResultObjsDefinedForCurSub())
		msgNumIsValid = ((pMsgNum > 0) && (pMsgNum <= this.msgSearchHdrs[this.subBoardCode].indexed.length));
	else
	{
		var msgbase = new MsgBase(this.subBoardCode);
		if (msgbase.open())
		{
			msgNumIsValid = ((pMsgNum > 0) && (pMsgNum <= msgbase.total_msgs));
			msgbase.close();
		}
	}
	return msgNumIsValid;
}

// For the DigDistMsgReader class: Returns a message header by index.  Will look
// in this.msgSearchHdrs if it's not empty, then in this.hdrsForCurrentSubBoard
// if it's not empty, then from msgbase.
//
// Parameters:
//  pMsgIdx: The message index (0-based)
//  pExpandFields: Whether or not to expand fields.  Defaults to false.
//  pMsgbase: Optional - An open MsgBase object.  If not passed, the sub-board will be opened in this method.
function DigDistMsgReader_GetMsgHdrByIdx(pMsgIdx, pExpandFields, pMsgbase)
{
	var expandFields = (typeof(pExpandFields) == "boolean" ? pExpandFields : false);

	var msgHdr = null;
	if (this.msgSearchHdrs.hasOwnProperty(this.subBoardCode) &&
	    (this.msgSearchHdrs[this.subBoardCode].indexed.length > 0))
	{
		if ((pMsgIdx >= 0) && (pMsgIdx < this.msgSearchHdrs[this.subBoardCode].indexed.length))
		{
			if (expandFields)
				msgHdr = this.msgSearchHdrs[this.subBoardCode].indexed[pMsgIdx];
			else
				msgHdr = getHdrFromMsgbase(pMsgbase, this.subBoardCode, false, this.msgSearchHdrs[this.subBoardCode].indexed[pMsgIdx].number, expandFields);
		}
	}
	else if (this.hdrsForCurrentSubBoard.length > 0)
	{
		if ((pMsgIdx >= 0) && (pMsgIdx < this.hdrsForCurrentSubBoard.length))
		{
			if (expandFields)
				msgHdr = this.hdrsForCurrentSubBoard[pMsgIdx];
			else
				msgHdr = getHdrFromMsgbase(pMsgbase, this.subBoardCode, false, this.hdrsForCurrentSubBoard[pMsgIdx].number, expandFields);
		}
	}
	else
		msgHdr = getHdrFromMsgbase(pMsgbase, this.subBoardCode, true, pMsgIdx, pExpandFields);
	if (msgHdr == null)
		msgHdr = getBogusMsgHdr();
	return msgHdr;
}

// For the DigDistMsgReader class: Returns a message header by message number
// (1-based).  Will look in this.msgSearchHdrs if it's not empty, then in
// this.hdrsForCurrentSubBoard if it's not empty, then from msgbase.
//
// Parameters:
//  pMsgNum: The message number (1-based)
//  pExpandFields: Whether or not to expand fields.  Defaults to false.
//
// Return value: The message header for the message number, or null on error
function DigDistMsgReader_GetMsgHdrByMsgNum(pMsgNum, pExpandFields)
{
	var msgHdr = null;
	if (this.msgSearchHdrs.hasOwnProperty(this.subBoardCode) &&
			(this.msgSearchHdrs[this.subBoardCode].indexed.length > 0))
	{
		if ((pMsgNum > 0) && (pMsgNum <= this.msgSearchHdrs[this.subBoardCode].indexed.length))
			msgHdr = this.msgSearchHdrs[this.subBoardCode].indexed[pMsgNum-1];
	}
	else if (this.hdrsForCurrentSubBoard.length > 0)
	{
		if ((pMsgNum > 0) && (pMsgNum <= this.hdrsForCurrentSubBoard.length))
			msgHdr = this.hdrsForCurrentSubBoard.length[pMsgNum-1];
	}
	else
	{
		var msgbase = new MsgBase(this.subBoardCode);
		if (msgbase.open())
		{
			if ((pMsgNum > 0) && (pMsgNum <= msgbase.total_msgs))
			{
				var expandFields = (typeof(pExpandFields) == "boolean" ? pExpandFields : false);
				msgHdr = msgbase.get_msg_header(true, pMsgNum-1, expandFields);
			}
			msgbase.close();
		}
	}
	if (msgHdr == null)
		msgHdr = getBogusMsgHdr();
	return msgHdr;
}

// For the DigDistMsgReader class: Returns a message header by absolute message
// number.  If there is a problem, this method will return null.
//
// Parameters:
//  pMsgNum: The absolute message number
//  pExpandFields: Whether or not to expand fields.  Defaults to false.
//  pGetVoteInfo: Whether or not to get voting information.  Defaults to false.
//
// Return value: The message header for the message number, or null on error
function DigDistMsgReader_GetMsgHdrByAbsoluteNum(pMsgNum, pExpandFields, pGetVoteInfo)
{
	var msgHdr = null;
	var msgbase = new MsgBase(this.subBoardCode);
	if (msgbase.open())
	{
		var expandFields = (typeof(pExpandFields) == "boolean" ? pExpandFields : false);
		var getVoteInfo = (typeof(pGetVoteInfo) == "boolean" ? pGetVoteInfo : false);
		msgHdr = msgbase.get_msg_header(false, pMsgNum, expandFields, getVoteInfo);
		msgbase.close();
	}
	if (msgHdr == null)
		msgHdr = getBogusMsgHdr();
	return msgHdr;
}

// For the DigDistMsgReader class: Takes an absolute message number and returns
// its message index (offset).  On error, returns -1.
//
// Parameters:
//  pMsgNum: The absolute message number
//
// Return value: The message's index.  On error, returns -1.
function DigDistMsgReader_AbsMsgNumToIdx(pMsgNum)
{
	var msgIdx = -1;
	var msgbase = new MsgBase(this.subBoardCode);
	if (msgbase.open())
	{
		msgIdx = absMsgNumToIdx(msgbase, pMsgNum);
		msgbase.close();
	}
	return msgIdx;
}

// For the DigDistMsgReader class: Takes a message index and returns
// its absolute message number.  On error, returns -1.
//
// Parameters:
//  pMsgIdx: The message index
//
// Return value: The message's absolute message number.  On error, returns -1.
function DigDistMsgReader_IdxToAbsMsgNum(pMsgIdx)
{
	var msgIdx = -1;
	var msgbase = new MsgBase(this.subBoardCode);
	if (msgbase.open())
	{
		msgIdx = idxToAbsMsgNum(msgbase, pMsgIdx);
		msgbase.close();
	}
	return msgIdx;
}

// This function takes an absolute message number for a given messagebase objectand
// and returns the message index (offset).  On error, returns -1.
//
// Parameters:
//  pMsgbase: The messagebase object from which to retrieve the message header
//  pMsgNum: The absolute message number
//
// Return value: The message's index, or -1 on error.
function absMsgNumToIdx(pMsgbase, pMsgNum)
{
	if ((pMsgbase == null) || (typeof(pMsgbase) != "object"))
		return -1;
	if (!pMsgbase.is_open)
		return -1;

	if (typeof(pMsgNum) != "number")
		return -1;
	
	var messageIdx = 0;
	// If pMsgNum is 0xffffffff (0xffffffff, or ~0), that is a special value
	// for the user's scan_ptr meaning it should point to the latest message
	// in the messagebase.
	if (pMsgNum == 0xffffffff)
		messageIdx = pMsgbase.total_msgs - 1; // Or this.NumMessages() - 1 but can't because this isn't a class member function
	else
	{
		var msgHdr = pMsgbase.get_msg_header(false, pMsgNum, false);
		if ((msgHdr == null) && gCmdLineArgVals.verboselogging)
		{
			writeToSysAndNodeLog("Message area " + pMsgbase.cfg.code + ": Tried to get message header for absolute message number " +
			                     pMsgNum + " but got a null header object.");
		}
		messageIdx = (msgHdr != null ? msgHdr.offset : -1);
	}
	return messageIdx;
}

// Takes a message index and returns the message's absolute message number.
//
// Parameters:
//  pMsgbase: The messagebase object from which to retrieve the message header
//  pMsgIdx: The message index
//
// Return value: The absolue message number, or -1 on error.
function idxToAbsMsgNum(pMsgbase, pMsgIdx)
{
	if ((pMsgbase == null) || (typeof(pMsgbase) != "object"))
		return -1;
	if (!pMsgbase.is_open)
		return -1;

	var msgHdr = pMsgbase.get_msg_header(true, pMsgIdx, false);
	if ((msgHdr == null) && gCmdLineArgVals.verboselogging)
	{
		writeToSysAndNodeLog("Tried to get message header for message offset " +
		                     pMsgIdx + " but got a null header object.");
	}
	return (msgHdr != null ? msgHdr.number : -1);
}

// Prompts the user for a message number and keeps repeating if they enter and
// invalid message number.  It is assumed that the cursor has already been
// placed where it needs to be for the prompt text, but a cursor position is
// one of the parameters to this function so that this function can write an
// error message if the message number inputted from the user is invalid.
//
// Parameters:
//  pCurPos: The starting position of the cursor (used for positioning the
//           cursor at the prompt text location to display an error message).
//           If not needed (i.e., in a traditional user interface or non-ANSI
//           terminal), this parameter can be null.
//  pPromptText: The text to use to prompt the user
//  pClearToEOLAfterPrompt: Whether or not to clear to the end of the line after
//                          writing the prompt text (boolean)
//  pErrorPauseTimeMS: The time (in milliseconds) to pause after displaying the
//                     error that the message number is invalid
//  pRepeat: Boolean - Whether or not to ask repeatedly until the user enters a
//           valid message number.  Optional; defaults to false.
//
// Return value: The message number entered by the user.  If the user didn't
//               enter a message number, this will be 0.
function DigDistMsgReader_PromptForMsgNum(pCurPos, pPromptText, pClearToEOLAfterPrompt,
                                          pErrorPauseTimeMS, pRepeat)
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
            console.cleartoeol("\x01n");
         else
         {
            if (lastErrorLen > promptTextLen)
            {
               console.print("\x01n");
               console.gotoxy(pCurPos.x+promptTextLen, pCurPos.y);
               var clearLen = lastErrorLen - promptTextLen;
               for (var counter = 0; counter < clearLen; ++counter)
                  console.print(" ");
            }
         }
         console.gotoxy(pCurPos.x+promptTextLen, pCurPos.y);
      }
      msgNum = console.getnum(this.HighestMessageNum());
      // If the message number is invalid, then output an error message.
      if (msgNum != 0)
      {
         if (!this.IsValidMessageNum(msgNum))
         {
				// Output an error message that the message number is invalid
				if (useCurPos)
				{
					// TODO: Update to optionally not clear all of the line before writing
					// the error text, or specify how much of the line to clear.  I want
					// to do that for the enhanced message reader when prompting for a
					// message number to read - I don't want this to clear the whole line
					// because that would erase the scrollbar character on the right.
					writeWithPause(pCurPos.x, pCurPos.y,
					               "\x01n" + replaceAtCodesInStr(format(this.text.invalidMsgNumText, msgNum)) + "\x01n",
					               pErrorPauseTimeMS, "\x01n", true);
					console.gotoxy(pCurPos);
				}
				else
				{
					console.print("\x01n\x01w\x01h" + msgNum + " \x01yis an invalid message number.");
					console.crlf();
					if (pErrorPauseTimeMS > 0)
						mswait(pErrorPauseTimeMS);
				}
				// Set msgNum back to 0 to signify that the user didn't enter a (valid)
				// message number.
				msgNum = 0;
            lastErrorLen = 24 + msgNum.toString().length;
            continueAskingMsgNum = pRepeat;
         }
         else
            continueAskingMsgNum = false;
      }
      else
         continueAskingMsgNum = false;
   }
   while (continueAskingMsgNum)
   return msgNum;
}

// For the DigDistMsgReader class: Looks for complex @-code strings in a text line and
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
function DigDistMsgReader_ParseMsgAtCodes(pTextLine, pMsgHdr, pDisplayMsgNum, pDateTimeStr,
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
				textLine = this.ReplaceMsgAtCodeFormatStr(pMsgHdr, pDisplayMsgNum, textLine, substLen,
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
	if (this.readingPersonalEmail)
	{
		var subName = "Personal mail";
		var subDesc = "Personal mail";
	}
	else
	{
		grpIdx = msg_area.sub[this.subBoardCode].grp_index;
		groupName = msg_area.sub[this.subBoardCode].grp_name;
		groupDesc = msg_area.grp_list[grpIdx].description;
		subName = msg_area.sub[this.subBoardCode].name;
		subDesc = msg_area.sub[this.subBoardCode].description;
		msgConf = msg_area.grp_list[msg_area.sub[this.subBoardCode].grp_index].name + " "
		        + msg_area.sub[this.subBoardCode].name;
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
	var msgVoteInfo = getMsgUpDownvotesAndScore(pMsgHdr);
	// This list has some custom @-codes:
	var newTxtLine = textLine.replace(/@MSG_SUBJECT@/gi, pMsgHdr["subject"])
	                         .replace(/@MSG_TO@/gi, pMsgHdr["to"])
	                         .replace(/@MSG_TO_NAME@/gi, pMsgHdr["to"])
	                         .replace(/@MSG_TO_EXT@/gi, (typeof(pMsgHdr["to_ext"]) == "string" ? pMsgHdr["to_ext"] : ""))
	                         .replace(/@MSG_DATE@/gi, pDateTimeStr)
	                         .replace(/@MSG_ATTR@/gi, mainMsgAttrStr)
	                         .replace(/@MSG_AUXATTR@/gi, auxMsgAttrStr)
	                         .replace(/@MSG_NETATTR@/gi, netMsgAttrStr)
	                         .replace(/@MSG_ALLATTR@/gi, allMsgAttrStr)
	                         .replace(/@MSG_NUM_AND_TOTAL@/gi, messageNum.toString() + "/" + this.NumMessages())
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
							 .replace(/@SYSOP@/gi, system.operator)
							 .replace(/@DATE@/gi, system.datestr())
							 .replace(/@LOCATION@/gi, system.location)
							 .replace(/@DIR@/gi, fileDir)
							 .replace(/@DIRL@/gi, fileDirLong)
							 .replace(/@ALIAS@/gi, user.alias)
							 .replace(/@NAME@/gi, (user.name.length > 0 ? user.name : user.alias))
							 .replace(/@USER@/gi, user.alias)
							 .replace(/@MSG_UPVOTES@/gi, msgVoteInfo.upvotes)
							 .replace(/@MSG_DOWNVOTES@/gi,msgVoteInfo.downvotes)
							 .replace(/@MSG_SCORE@/gi, msgVoteInfo.voteScore);
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
		                       .replace(/@MSG_FROM_AND_FROM_NET@/gi, pMsgHdr["from"] + (typeof(pMsgHdr["from_net_addr"]) == "string" ? " (" + pMsgHdr["from_net_addr"] + ")" : ""))
		                       .replace(/@MSG_FROM_EXT@/gi, (typeof(pMsgHdr["from_ext"]) == "string" ? pMsgHdr["from_ext"] : ""));
	}
	if (!pAllowCLS)
		newTxtLine = newTxtLine.replace(/@CLS@/gi, "");
	newTxtLine = replaceAtCodesInStr(newTxtLine);
	return newTxtLine;
}
// For the DigDistMsgReader class: Helper for ParseMsgAtCodes(): Replaces a
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
function DigDistMsgReader_ReplaceMsgAtCodeFormatStr(pMsgHdr, pDisplayMsgNum, pTextLine, pSpecifiedLen,
                                  pAtCodeStr, pDateTimeStr, pUseBBSLocalTimeZone, pMsgMainAttrStr, pMsgAuxAttrStr,
								  pMsgNetAttrStr, pMsgAllAttrStr, pDashJustifyIdx)
{
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
			var fromWithNet = pMsgHdr["from"] + (typeof(pMsgHdr["from_net_addr"]) == "string" ? " (" + pMsgHdr["from_net_addr"] + ")" : "");
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
			replacementTxt = pMsgHdr["from"].substr(0, pSpecifiedLen);
	}
	else if (pAtCodeStr.indexOf("@MSG_FROM_EXT") > -1)
	{
		// If the user is not the sysop and the message was posted anonymously,
		// then replace the from name @-codes with "Anonymous".  Otherwise,
		// replace the from name @-codes with the actual from name.
		if (!user.is_sysop && ((pMsgHdr.attr & MSG_ANONYMOUS) == MSG_ANONYMOUS))
			replacementTxt = "Anonymous".substr(0, pSpecifiedLen);
		else
			replacementTxt = (typeof pMsgHdr["from_ext"] === "undefined" ? "" : pMsgHdr["from_ext"].substr(0, pSpecifiedLen));
	}
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
		replacementTxt = (messageNum.toString() + "/" + this.NumMessages()).substr(0, pSpecifiedLen); // "number" is also absolute number
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
		if (this.readingPersonalEmail)
			replacementTxt = "Personal mail".substr(0, pSpecifiedLen);
		else
			replacementTxt = msg_area.sub[this.subBoardCode].grp_name.substr(0, pSpecifiedLen);
			
	}
	else if (pAtCodeStr.indexOf("@GRPL") > -1)
	{
		if (this.readingPersonalEmail)
			replacementTxt = "Personal mail".substr(0, pSpecifiedLen);
		else
		{
			var grpIdx = msg_area.sub[this.subBoardCode].grp_index;
			replacementTxt = msg_area.grp_list[grpIdx].description.substr(0, pSpecifiedLen);
		}
	}
	else if (pAtCodeStr.indexOf("@SUB") > -1)
	{
		if (this.readingPersonalEmail)
			replacementTxt = "Personal mail".substr(0, pSpecifiedLen);
		else

			replacementTxt = msg_area.sub[this.subBoardCode].name.substr(0, pSpecifiedLen);
	}
	else if (pAtCodeStr.indexOf("@SUBL") > -1)
	{
		if (this.readingPersonalEmail)
			replacementTxt = "Personal mail".substr(0, pSpecifiedLen);
		else
			replacementTxt = msg_area.sub[this.subBoardCode].description.substr(0, pSpecifiedLen);
	}
	else if ((pAtCodeStr.indexOf("@BBS") > -1) || (pAtCodeStr.indexOf("@BOARDNAME") > -1))
		replacementTxt = system.name.substr(0, pSpecifiedLen);
	else if (pAtCodeStr.indexOf("@SYSOP") > -1)
		replacementTxt = system.operator.substr(0, pSpecifiedLen);
	else if (pAtCodeStr.indexOf("@CONF") > -1)
	{
		var msgConfDesc = msg_area.grp_list[msg_area.sub[this.subBoardCode].grp_index].name + " "
		                + msg_area.sub[this.subBoardCode].name;
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
// Helper for DigDistMsgReader_ParseMsgAtCodes() and
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

// Finds the offset (index) of the next message prior to or after a given offset
// that is not marked as deleted.  If none is found, the return value will be -1.
//
// Parameters:
//  pOffset: The message offset to search prior/after
//  pForward: Boolean - Whether or not to search forward (true) or backward (false).
//            If this is not specified, the default will be true (forward).
//
// Return value: The index of the next message prior/later that is not marked
//               as deleted, or -1 if none is found.
function DigDistMsgReader_FindNextNonDeletedMsgIdx(pOffset, pForward)
{
	// Sanity checking for the parameters & other things
	if (typeof(pOffset) != "number")
		return -1;
	var searchForward = (typeof(pForward) == "boolean" ? pForward : true);
	var msgbase = new MsgBase(this.subBoardCode);
	if (!msgbase.open())
		return -1;

	var newMsgIdx = -1;
	if (searchForward)
	{
		// Search forward for a message that isn't marked for deletion.
		var numOfMessages = this.NumMessages(msgbase);
		if (pOffset < numOfMessages - 1)
		{
			var hdrIsBogus;
			for (var messageIdx = pOffset+1; (messageIdx < numOfMessages) && (newMsgIdx == -1); ++messageIdx)
			{
				var nextMsgHdr = this.GetMsgHdrByIdx(messageIdx, false, msgbase);
				hdrIsBogus = (nextMsgHdr.hasOwnProperty("isBogus") ? nextMsgHdr.isBogus : false);
				if ((nextMsgHdr != null) && !hdrIsBogus && ((nextMsgHdr.attr & MSG_DELETE) == 0))
					newMsgIdx = messageIdx;
			}
		}
	}
	else
	{
		// Search backward for a message that isn't marked for deletion.
		if (pOffset > 0)
		{
			var hdrIsBogus;
			for (var messageIdx = pOffset-1; (messageIdx >= 0) && (newMsgIdx == -1); --messageIdx)
			{
				var prevMsgHdr = this.GetMsgHdrByIdx(messageIdx, false, msgbase);
				hdrIsBogus = (prevMsgHdr.hasOwnProperty("isBogus") ? prevMsgHdr.isBogus : false);
				if ((prevMsgHdr != null) && !hdrIsBogus && ((prevMsgHdr.attr & MSG_DELETE) == 0))
					newMsgIdx = messageIdx;
			}
		}
	}
	msgbase.close();
	return newMsgIdx;
}

// For the DigDistMsgReader class: Sets up the message base object for a new
// sub-board.  Leaves the msgbase object open.
//
// Parameters:
//  pNewSubBoardCode: The internal code of the new sub-board.
//                    If this is not a string, then this function
//                    will use bbs.cursub_code instead.
//
// Return value: An object with the following properties:
//               succeeded: Boolean - True if succeeded or false if not
//               lastReadMsgIdx: The index of the last-read message in the sub-board
//               lastReadMsgNum: The message number of the last-read message in the sub-board
// 
function DigDistMsgReader_ChangeSubBoard(pNewSubBoardCode)
{
	var retObj = {
		succeeded: false,
		lastReadMsgIdx: 0,
		lastReadMsgNum: 0
	};

	var newSubBoardCode = bbs.cursub_code;
	if (typeof(pNewSubBoardCode) == "string")
		newSubBoardCode = pNewSubBoardCode;
	if (typeof(msg_area.sub[newSubBoardCode]) != "object")
	{
		console.clear("\x01n");
		console.print("\x01n\x01h\x01y* \x01wSomething has gone wrong.  An invalid message sub-board code was");
		console.crlf();
		console.print("specified: " + newSubBoardCode);
		console.crlf();
		console.pause();
		return retObj;
	}

	// If the new sub-board code is different from the currently-set
	// sub-board code, then go ahead and change it.
	if (newSubBoardCode != this.subBoardCode)
	{
		this.setSubBoardCode(newSubBoardCode);
		this.PopulateHdrsForCurrentSubBoard();
	}

	// If there are no messages to display in the current sub-board, then just
	// return.  Note: A message regarding there being no messages would have
	// already been shown, so we don't need to show an 'Empty sub-board' message
	// here.
	if (this.NumMessages() == 0)
		return retObj;

	// Get the index of the user's last-read message in this sub-board.
	var lastReadMsgInfo = this.GetLastReadMsgIdxAndNum();
	retObj.lastReadMsgIdx = lastReadMsgInfo.lastReadMsgIdx;
	retObj.lastReadMsgNum = lastReadMsgInfo.lastReadMsgNum;
	if (retObj.lastReadMsgIdx == -1)
		retObj.lastReadMsgIdx = 0;

	retObj.succeeded = true;

	return retObj;
}

// For the enhanced reader functionality of the DigDistMsgReader class: Sets up
// the message base object for a new sub-board and refreshes the reader hotkey
// help line on the bottom of the screen.  Leaves the msgbase object open.
//
// Parameters:
//  pNewSubBoardCode: The internal code of the new sub-board.
//                    If this is not a string, then this function
//                    will use bbs.cursub_code instead.
//
// Return value: An object with the following properties:
//               succeeded: Boolean - True if succeeded or false if not
//               lastReadMsgIdx: The index of the last message read in the sub-board.
//                               Will be 0 on error.
function DigDistMsgReader_EnhancedReaderChangeSubBoard(pNewSubBoardCode)
{
	var retObj = this.ChangeSubBoard(pNewSubBoardCode);
	if (retObj.succeeded && (this.NumMessages() > 0))
	{
		// Clear the screen and refresh the help line at the bottom of the screen
		console.clear("\x01n");
		this.DisplayEnhancedMsgReadHelpLine(console.screen_rows);
	}
	return retObj;
}

// For the DigDistMsgReader class: Allows the user to reply to a message
//
// Parameters:
//  pMsgHdr: The header of the message to reply to.  This needs to be a header with
//           fields expanded.
//  pMsgText: The text (body) of the message
//  pPrivate: Optional - Boolean to specify whether not this should be a private
//            reply using the user's QWK or FIDO, etc. address.  Defaults to
//            false if not specified.
//  pMsgIdx: The message index (if there are search results, this might be
//           different than the message offset in the messagebase).  This
//           is intended for use in deleting a private email after reading it.
//
// Return value: An object containing the following properties:
//               postSucceeded: Boolean - Whether or not the message post succeeded
//               msgWasDeleted: Boolean - Whether or not the message was deleted after
//                              the user replied to it
function DigDistMsgReader_ReplyToMsg(pMsgHdr, pMsgText, pPrivate, pMsgIdx)
{
	var retObj = {
		postSucceeded: false,
		msgWasDeleted: false
	};

	if ((pMsgHdr == null) || (typeof(pMsgHdr) != "object"))
		return retObj;

	// If the "no-reply" attribute is enabled for the message, then don't
	// let the user rpely.
	if ((pMsgHdr.attr & MSG_NOREPLY) == MSG_NOREPLY)
	{
		console.crlf();
		console.print("\x01n\x01y\x01hReplies are not allowed for this message.");
		console.crlf();
		console.pause();
		return retObj;
	}

	// Make sure the user is allowed to post a message before allowing them to
	// reply.  If posting on a message sub-board, then we can check the can_read
	// property of the sub-board; otherwise, the user should be allowed to post
	// a message.
	var replyPrivately = (typeof(pPrivate) == "boolean" ? pPrivate : false);
	var canPost = true;
	if (!replyPrivately && (this.subBoardCode != "mail"))
		canPost = msg_area.sub[this.subBoardCode].can_post;
	if (!canPost)
	{
		console.crlf();
		console.print("\x01n\x01y\x01hYou are not allowed to post in this message area.");
		console.crlf();
		console.pause();
		return retObj;
	}

	retObj.postSucceeded = true;

	// No special behavior in the reply
	var replyMode = WM_NONE;

	// If quoting is allowed in the sub-board, then write QUOTES.TXT in
	// the node directory to allow the user to quote the original message.
	var msgbase = new MsgBase(this.subBoardCode);
	if (msgbase.open())
	{
		var quoteFile = null;
		if (this.CanQuote())
		{
			// Get the user's setting for whether or not to wrap quote lines (and how long) from
			// their external editor settings
			var editorQuoteCfg = getExternalEditorQuoteWrapCfgFromSCFG(user.editor);
			// Write the message text to the quotes file
			quoteFile = new File(system.node_dir + "QUOTES.TXT");
			if (quoteFile.open("w"))
			{
				var msgNum = (typeof(pMsgIdx) === "number" ? pMsgIdx+1 : null);
				var msgText = "";
				if (typeof(pMsgText) == "string")
					msgText = pMsgText;
				else
					msgText = msgbase.get_msg_body(false, pMsgHdr.number, false, false, true, true);
				if (editorQuoteCfg.quoteWrapEnabled && editorQuoteCfg.quoteWrapCols > 0)
					msgText = word_wrap(msgText, editorQuoteCfg.quoteWrapCols, msgText.length, false);
				quoteFile.write(msgText);

				quoteFile.close();
				// Let the user quote in the reply
				replyMode |= WM_QUOTE;
			}
		}
	}

	// Strip any control characters that the subject line might have
	pMsgHdr.subject = strip_ctrl(pMsgHdr.subject);

	// If the user is listing personal e-mail, then we need to call
	// bbs.email() to leave a reply; otherwise, use bbs.post_msg().
	if (this.readingPersonalEmail)
	{
		var privReplRetObj = this.DoPrivateReply(pMsgHdr, pMsgIdx, replyMode);
		retObj.postSucceeded = privReplRetObj.sendSucceeded;
		retObj.msgWasDeleted = privReplRetObj.msgWasDeleted;
	}
	else
	{
		// The user is posting in a public message sub-board.
		// Open a file in the node directory and write some information
		// about the current sub-board and message being read:
		// - The highest message number in the sub-board (last message)
		// - The total number of messages in the sub-board
		// - The number of the message being read
		// - The current sub-board code
		// This is for message editors that need to access the message
		// base (i.e., SlyEdit).  Normally (in Synchronet's message read
		// propmt), this information is stored in bbs.smb_last_msg,
		// bbs.smb_total_msgs, and bbs.smb_curmsg, but this message lister
		// can't change those values.  Thus, we need to write them to a file.
		var msgBaseInfoFile = new File(system.node_dir + "DDML_SyncSMBInfo.txt");
		if (msgBaseInfoFile.open("w"))
		{
			msgBaseInfoFile.writeln(msgbase.last_msg.toString()); // Highest message #
			msgBaseInfoFile.writeln(this.NumMessages(msgbase).toString()); // Total # messages
			// Message number (Note: For SlyEdit, requires SlyEdit 1.27 or newer).
			msgBaseInfoFile.writeln(pMsgHdr.number.toString()); // # of the message being read (New: 2013-05-14)
			msgBaseInfoFile.writeln(this.subBoardCode); // Sub-board code
			msgBaseInfoFile.close();
		}

		// Store the current total number of messages so that we can search new
		// messages if needed after the message is posted
		var numMessagesBefore = msgbase.total_msgs;

		// Let the user post the message.  Then, delete the message base info
		// file.  To be safe, and to ensure the messagebase object gets refreshed
		// with the latest information, close the messagebase object before
		// posting the message and re-open it afterward.  On Linux, the messagebase
		// object doesn't seem to get refreshed with the number of messages in the
		// sub-board, etc., but on Windows that doesn't seem to be an issue.
		// If we are to send a private message, then let the user send the reply
		// as a private email.  Otherwise, let the user post the reply as a public
		// message.
		// 2016-08-26: Updated to not close the messagebase because a private
		// reply on a networked sub-board needs to be able to get a message
		// header with fields expanded.
		msgbase.close();
		if (replyPrivately)
		{
			var privReplRetObj = this.DoPrivateReply(pMsgHdr, pMsgIdx, replyMode);
			retObj.postSucceeded = privReplRetObj.sendSucceeded;
			retObj.msgWasDeleted = privReplRetObj.msgWasDeleted;
		}
		else
		{
			// Not a private message - Post as a public message
			// TODO: Saw this error message once on the next line:
			// Error: Error -300 adding RFC822MSGID field to message header
			retObj.postSucceeded = bbs.post_msg(this.subBoardCode, replyMode, pMsgHdr);
			console.pause();
		}
		msgBaseInfoFile.remove();
		var msgbaseReOpened = msgbase.open();

		// If the user replied to the message and a message search was done that
		// would populate the search results, then search the last messages to
		// include the user's reply in the message matches or other new messages
		// that may have been posted that match the user's search.
		// TODO: Make sure this works for a newscan & new-to-user scan
		if (retObj.postSucceeded && msgbaseReOpened && (msgbase.total_msgs > numMessagesBefore))
		{
			if (this.SearchTypePopulatesSearchResults() && this.msgSearchHdrs.hasOwnProperty(this.subBoardCode))
			{
				if (!this.msgSearchHdrs.hasOwnProperty(this.subBoardCode))
					this.msgSearchHdrs[this.subBoardCode] = searchMsgbase(this.subBoardCode, this.searchType, this.searchString, this.readingPersonalEmailFromUser);
				else
				{
					var msgHeaders = searchMsgbase(this.subBoardCode, this.searchType, this.searchString, this.readingPersonalEmailFromUser, numMessagesBefore, msgbase.total_msgs);
					var msgNum = 0;
					for (var i = 0; i < msgHeaders.indexed.length; ++i)
					{
						this.msgSearchHdrs[this.subBoardCode].indexed.push(msgHeaders.indexed[i]);
						msgNum = msgHeaders.indexed[i].offset + 1;
					}
				}
			}
			else if (this.hdrsForCurrentSubBoard.length > 0)
			{
				// Pass false to get_all_msg_headers() to tell it not to return vote messages
				// (the parameter was introduced in Synchronet 3.17+)
				this.FilterMsgHdrsIntoHdrsForCurrentSubBoard(msgbase.get_all_msg_headers(true), true);
			}
		}
	}

	// Delete the quote file
	if (quoteFile != null)
		quoteFile.remove();

	msgbase.close();

	return retObj;
}

// For the DigDistMsgReader class: This function performs a private message reply.  Takes a message header
// and returns a boolean for whether or not it succeeded in sending the reply.
//
// Parameters:
//  pMsgHdr: A message header object.  This needs to be a header with expanded fields.
//  pMsgIdx: The message index (if there are search results, this might be
//           different than the message offset in the messagebase).  This
//           is intended for use in deleting a private email after reading it.
//  pReplyMode: Optional - A bitfield containing reply mode bits to use
//              in addition to the default network/email reply mode
//
// Return value: An object containing the following properties:
//               sendSucceeded: Boolean - Whether or not the message post succeeded
//               msgWasDeleted: Boolean - Whether or not the message was deleted after
//                              the user replied to it
function DigDistMsgReader_DoPrivateReply(pMsgHdr, pMsgIdx, pReplyMode)
{
	var retObj = {
		sendSucceeded: true,
		msgWasDeleted: false
	};
	
	if (pMsgHdr == null)
	{
		retObj.sendSucceeded = false;
		return retObj;
	}

	// Set up the initial reply mode bits
	var replyMode = WM_NONE;
	if (typeof(pReplyMode) == "number")
		replyMode |= pReplyMode;

	// If the message is a networked message, then try to address the message
	// to the network address.
	var couldNotDetermineNetAddr = true;
	var wasNetMailOrigin = false;
	if ((typeof(pMsgHdr.from_net_type) != "undefined") && (pMsgHdr.from_net_type != NET_NONE))
	{
		if (user.is_sysop) console.print("\x01n\r\nHere 1\r\n\x01p"); // Temporary
		wasNetMailOrigin = true;
		if ((typeof(pMsgHdr.from_net_addr) == "string") && (pMsgHdr.from_net_addr.length > 0))
		{
			if (user.is_sysop) console.print("\x01n\r\nHere 2\r\n\x01p"); // Temporary
			couldNotDetermineNetAddr = false;
			// Build the email address to reply to.  If the original message is
			// internet email, then simply use the from_net_addr field from the
			// message header.  Otherwise (i.e., on a networked sub-board), use
			// username@from_net_addr.
			var emailAddr = "";
			if (typeof(pMsgHdr.from_net_addr) === "string" && pMsgHdr.from_net_addr.length > 0)
			{
				if (user.is_sysop) console.print("\x01n\r\nHere 3\r\n\x01p"); // Temporary
				if (pMsgHdr.from_net_type == NET_INTERNET)
					emailAddr = pMsgHdr.from_net_addr;
				else
					emailAddr = pMsgHdr.from + "@" + pMsgHdr.from_net_addr;
			}
			// Prompt the user to verify the receiver's email address
			console.putmsg(bbs.text(Email), P_SAVEATR);
			emailAddr = console.getstr(emailAddr, 60, K_LINE|K_EDIT);
			if ((typeof(emailAddr) == "string") && (emailAddr.length > 0))
			{
				if (user.is_sysop) console.print("\x01n\r\nHere 4\r\n\x01p"); // Temporary
				replyMode |= WM_NETMAIL;
				retObj.sendSucceeded = bbs.netmail(emailAddr, replyMode, null, pMsgHdr);
				console.pause();
			}
			else
			{
				if (user.is_sysop) console.print("\x01n\r\nHere 5\r\n\x01p"); // Temporary
				retObj.sendSucceeded = false;
				console.putmsg(bbs.text(Aborted), P_SAVEATR);
				console.pause();
			}
		}
	}
	// If we could not determine the network mail address, we may need to try to look up the user to
	// reply locally.
	if (couldNotDetermineNetAddr)
	{
		// Most likely replying to a local user
		replyMode |= WM_EMAIL;
		// Look up the user number of the "from" user name in the message header
		var userNumber = findUserNumWithName(pMsgHdr.from); // Used to use system.matchuser(pMsgHdr.from)
		if (userNumber != 0)
		{
			// Output a newline to avoid ugly overwriting of text on the screen in
			// case the sender wants to forward to netmail, then send email to the
			// sender.  Note that if the send failed, that could be because the
			// user aborted the message.
			console.crlf();
			retObj.sendSucceeded = bbs.email(userNumber, replyMode, null, null, pMsgHdr);
			console.pause();
		}
		else
		{
			// If the 'from' username is blank (which can be the case if a guest sent the email), then
			// ask the user where or to whom they want to send the message
			if (pMsgHdr.from.length == 0)
			{
				console.attributes = "NC";
				console.crlf();
				console.print("Sender is unknown. Enter user name/number/email/netmail address\x01h:\x01n");
				console.crlf();
				var msgDest = console.getstr(console.screen_columns - 1, K_LINE);
				if (msgDest != "")
				{
					var recipientMatched = true;
					var sendViaNetmail = false;
					// See if the user entered a netmail address
					if (gEmailRegex.test(msgDest) || gFTNEmailRegex.test(msgDest))
						sendViaNetmail = true;
					// Check for a valid user number
					else if (/^[0-9]+/.test(msgDest))
					{
						if (system.username(+msgDest) != "")
							userNumber = +msgDest;
						else
							recipientMatched = false;
					}
					// Match local user by name/alias
					else
					{
						userNumber = findUserNumWithName(msgDest);
						if (userNumber <= 0)
							recipientMatched = false;
					}
					// If no recipient was matched, then output an error.  Otherwise, do a reply.
					if (!recipientMatched)
					{
						retObj.sendSucceeded = false;
						console.crlf();
						var errorMsg = "\x01n\x01h\x01yThe recipient (\x01w" + msgDest + "\x01y) was not found";
						if (wasNetMailOrigin)
							errorMsg += " and no network address was found for this message";
						errorMsg += "\x01n";
						console.print(errorMsg);
						console.crlf();
						console.pause();
					}
					else if (sendViaNetmail)
					{
						replyMode |= WM_NETMAIL;
						retObj.sendSucceeded = bbs.netmail(msgDest, replyMode, null, pMsgHdr);
						console.pause();
					}
					else
					{
						console.crlf();
						retObj.sendSucceeded = bbs.email(userNumber, replyMode, null, null, pMsgHdr);
						console.pause();
					}
				}
				else
				{
					console.attributes = "N";
					console.print("Canceled");
					console.crlf();
				}
			}
			else
			{
				retObj.sendSucceeded = false;
				console.crlf();
				var errorMsg = "\x01n\x01h\x01yThe recipient (\x01w" + pMsgHdr.from + "\x01y) was not found";
				if (wasNetMailOrigin)
					errorMsg += " and no network address was found for this message";
				errorMsg += "\x01n";
				console.print(errorMsg);
				console.crlf();
				console.pause();
			}
		}
	}

	// If the user replied to a personal email, then ask the user if they want
	// to delete the message that was just replied to, and if so, delete it.
	if (retObj.sendSucceeded && this.readingPersonalEmail && (typeof(pMsgIdx) == "number"))
	{
		// Get the delete mail confirmation text from text.dat and replace
		// the %s with the "from" name in the message header, and use that
		// as the confirmation text.
		// Note: If the message was deleted, the DeleteMessage() method will
		// refresh the header in the search results, if there are any search
		// results.
		if (!console.noyes(bbs.text(DeleteMailQ).replace("%s", pMsgHdr.from)))
			retObj.msgWasDeleted = this.PromptAndDeleteOrUndeleteMessage(pMsgIdx, null, true, null, null, false);
	}

	return retObj;
}

// For the DigDistMsgReader class: Displays the enhanced reader mode help screen.
//
// Parameters:
//  pDisplayChgAreaOpt: Optional boolean - Whether or not to show the "change area" option.
//                      Defaults to true.
//  pDisplayDLAttachmentOpt: Optional boolean - Whether or not to display the "download attachments"
//                           option.  Defaults to false.
function DigDistMsgReader_DisplayEnhancedReaderHelp(pDisplayChgAreaOpt, pDisplayDLAttachmentOpt)
{
	if (console.term_supports(USER_ANSI))
		console.clear("\x01n");

	var displayChgAreaOpt = (typeof(pDisplayChgAreaOpt) == "boolean" ? pDisplayChgAreaOpt : true);
	var displayDLAttachmentsOpt = (typeof(pDisplayDLAttachmentOpt) == "boolean" ? pDisplayDLAttachmentOpt : false);

	DisplayProgramInfo();
	console.crlf();

	// Display information about the current sub-board and search results.
	console.print("\x01n\x01cCurrently reading \x01g" + subBoardGrpAndName(this.subBoardCode));
	console.crlf();
	// If the user isn't reading personal messages (i.e., is reading a sub-board),
	// then output the total number of messages in the sub-board.  We probably
	// shouldn't output the total number of messages in the "mail" area, because
	// that includes more than the current user's email.
	if (!this.readingPersonalEmail)
	{
		var numOfMessages = 0;
		var msgbase = new MsgBase(this.subBoardCode);
		if (msgbase.open())
		{
			numOfMessages = msgbase.total_msgs;
			msgbase.close();
		}
		console.print("\x01n\x01cThere are a total of \x01g" + numOfMessages + " \x01cmessages in the current area.");
		console.crlf();
	}
	// If there is currently a search (which also includes personal messages),
	// then output the number of search results/personal messages.
	if (this.SearchingAndResultObjsDefinedForCurSub())
	{
		var numSearchResults = this.NumMessages();
		var resultsWord = (numSearchResults > 1 ? "results" : "result");
		console.print("\x01n\x01c");
		if (this.readingPersonalEmail)
			console.print("You have \x01g" + numSearchResults + " \x01c" + (numSearchResults == 1 ? "message" : "messages") + ".");
		else
		{
			if (numSearchResults == 1)
				console.print("There is \x01g1 \x01csearch result.");
			else
				console.print("There are \x01g" + numSearchResults + " \x01csearch results.");
		}
		console.crlf();
	}

	// Display the enhanced reader keys
	console.crlf();
	console.print("\x01n\x01cEnhanced reader mode keys");
	console.crlf();
	console.print("\x01h\x01k");
	for (var i = 0; i < 25; ++i)
		console.print(HORIZONTAL_SINGLE);
	console.crlf();
	var keyHelpLines = ["\x01h\x01cDown\x01g/\x01cup arrow    \x01g: \x01n\x01cScroll down\x01g/\x01cup in the message",
	                    "\x01h\x01cLeft\x01g/\x01cright arrow \x01g: \x01n\x01cGo to the previous\x01g/\x01cnext message",
						"\x01h\x01cEnter            \x01g: \x01n\x01cGo to the next message",
	                    "\x01h\x01cPageUp\x01g/\x01cPageDown  \x01g: \x01n\x01cScroll up\x01g/\x01cdown a page in the message",
	                    "\x01h\x01cHOME             \x01g: \x01n\x01cGo to the top of the message",
	                    "\x01h\x01cEND              \x01g: \x01n\x01cGo to the bottom of the message"];
	keyHelpLines.push("\x01h\x01cCtrl-U           \x01g: \x01n\x01cUser settings (including personal twit list)");
	if (user.is_sysop)
	{
		keyHelpLines.push("\x01h\x01cDEL              \x01g: \x01n\x01cDelete the current message");
		keyHelpLines.push("\x01h\x01cCtrl-S           \x01g: \x01n\x01cSave the message (to the BBS machine)");
		keyHelpLines.push("\x01h\x01c" + this.enhReaderKeys.validateMsg + "                \x01g: \x01n\x01cValidate the message");
	}
	else if (this.CanDelete() || this.CanDeleteLastMsg())
		keyHelpLines.push("\x01h\x01cDEL              \x01g: \x01n\x01cDelete the current message (if it's yours)");
	if (displayDLAttachmentsOpt)
		keyHelpLines.push("\x01h\x01cCtrl-A           \x01g: \x01n\x01cDownload attachments");
	// If not reading personal email or doing a search/scan, then include the
	// text for the message threading keys.
	if (!this.readingPersonalEmail && !this.SearchingAndResultObjsDefinedForCurSub())
	{
		// Thread ID keys: For Synchronet 3.16 and above, include the text "thread ID"
		// in the help line, since Synchronet 3.16 has the thread_id field in the message
		// headers.
		var threadIDLine = "\x01h\x01c" + this.enhReaderKeys.prevMsgByThreadID + " \x01n\x01cor \x01h" + this.enhReaderKeys.nextMsgByThreadID + "           \x01g: \x01n\x01cGo to the previous\x01g/\x01cnext message in the thread";
		/*
		if (system.version_num >= 31600)
			threadIDLine += " (thread ID)";
		*/
		keyHelpLines.push(threadIDLine);
		keyHelpLines.push("\x01h\x01c" + this.enhReaderKeys.prevMsgByTitle + " \x01n\x01cor \x01h" + this.enhReaderKeys.nextMsgByTitle + "           \x01g: \x01n\x01cGo to the previous\x01g/\x01cnext message by title (subject)");
		keyHelpLines.push("\x01h\x01c" + this.enhReaderKeys.prevMsgByAuthor + " \x01n\x01cor \x01h" + this.enhReaderKeys.nextMsgByAuthor + "           \x01g: \x01n\x01cGo to the previous\x01g/\x01cnext message by author");
		keyHelpLines.push("\x01h\x01c" + this.enhReaderKeys.prevMsgByToUser + " \x01n\x01cor \x01h" + this.enhReaderKeys.nextMsgByToUser + "           \x01g: \x01n\x01cGo to the previous\x01g/\x01cnext message by 'To user'");
	}
	keyHelpLines.push("\x01h\x01c" + this.enhReaderKeys.firstMsg + " \x01n\x01cor \x01h" + this.enhReaderKeys.lastMsg + "           \x01g: \x01n\x01cGo to the first\x01g/\x01clast message in the sub-board");
	if (displayChgAreaOpt)
	{
		if (this.doingMultiSubBoardScan)
			keyHelpLines.push("\x01h\x01c" + this.enhReaderKeys.nextSubBoard + "                \x01g: \x01n\x01cGo to the next message sub-board");
		else
			keyHelpLines.push("\x01h\x01c" + this.enhReaderKeys.prevSubBoard + "\x01n\x01c or \x01h" + this.enhReaderKeys.nextSubBoard + "           \x01g: \x01n\x01cGo to the previous\x01g/\x01cnext message sub-board");
		keyHelpLines.push("\x01h\x01c" + this.enhReaderKeys.chgMsgArea + "                \x01g: \x01n\x01cChange to a different message sub-board");
	}
	else if (this.doingMultiSubBoardScan)
		keyHelpLines.push("\x01h\x01c" + this.enhReaderKeys.nextSubBoard + "                \x01g: \x01n\x01cGo to the next message sub-board");
	if (user.is_sysop)
	{
		keyHelpLines.push("\x01h\x01c" + this.enhReaderKeys.editMsg + "                \x01g: \x01n\x01cEdit the current message");
		keyHelpLines.push("\x01h\x01c" + this.enhReaderKeys.userEdit + "                \x01g: \x01n\x01cEdit the user who wrote the message");
	}
	keyHelpLines.push("\x01h\x01c" + this.enhReaderKeys.showMsgList + "                \x01g: \x01n\x01cList messages in the current sub-board");
	if (user.is_sysop)
		keyHelpLines.push("\x01h\x01c" + this.enhReaderKeys.showHdrInfo + " \x01n\x01cor \x01h" + this.enhReaderKeys.showKludgeLines + "           \x01g: \x01n\x01cDisplay extended header info\x01g/\x01ckludge lines for the message");
	keyHelpLines.push("\x01h\x01c" + this.enhReaderKeys.reply + "                \x01g: \x01n\x01cReply to the current message");
	if (!this.readingPersonalEmail)
	{
		keyHelpLines.push("\x01h\x01c" + this.enhReaderKeys.privateReply + "                \x01g: \x01n\x01cPrivately reply to the current message (via email/NetMail)");
		keyHelpLines.push("\x01h\x01c" + this.enhReaderKeys.postMsg + "                \x01g: \x01n\x01cPost a message on the sub-board");
	}
	keyHelpLines.push("\x01h\x01cNumber           \x01g: \x01n\x01cGo to a specific message by number");
	keyHelpLines.push("\x01h\x01cSpacebar         \x01g: \x01n\x01cSelect message (for batch delete, etc.)");
	keyHelpLines.push("                   \x01n\x01cFor batch delete, open the message list and use CTRL-D.");
	keyHelpLines.push("\x01h\x01c" + this.enhReaderKeys.forwardMsg + "                \x01g: \x01n\x01cForward the message to user/email");
	if (typeof((new MsgBase(this.subBoardCode)).vote_msg) === "function")
	{
		keyHelpLines.push("\x01h\x01c" + this.enhReaderKeys.vote + "                \x01g: \x01n\x01cVote on the message");
		keyHelpLines.push("\x01h\x01c" + this.enhReaderKeys.closePoll + "                \x01g: \x01n\x01cClose a poll");
	}
	keyHelpLines.push("\x01h\x01c" + this.enhReaderKeys.showVotes + "                \x01g: \x01n\x01cShow vote (tally) stats for the message");
	keyHelpLines.push("\x01h\x01c" + this.enhReaderKeys.quit + "                \x01g: \x01n\x01cQuit back to the BBS");
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
}

// For the DigDistMsgReader class: Displays the enhanced reader mode message
// header information for a particular message header.
//
// Parameters:
//  pMsgHdr: The message header object containing message header info
//  pDisplayMsgNum: The message number to display, if different from the number
//                  in the header object.  This can be null, in which case the
//                  number in the header object will be used.
//  pStartScreenRow: The row on the screen at which to start displaying the
//                   header information.  Will be used if the user's terminal
//                   supports ANSI.
function DigDistMsgReader_DisplayEnhancedMsgHdr(pMsgHdr, pDisplayMsgNum, pStartScreenRow)
{
	if ((pMsgHdr == null) || (typeof(pMsgHdr) != "object"))
		return;
	// For the set of enhanced header lines, choose the regular set or the set with
	// the highlighted 'to' user, dependin on whether the message was written to the
	// logged-in (reading) user.
	var enhMsgHdrLines = (userNameHandleAliasMatch(pMsgHdr.to) ? this.enhMsgHeaderLinesToReadingUser : this.enhMsgHeaderLines);
	if (enhMsgHdrLines == null)
		return;
	if ((enhMsgHdrLines.length == 0) || (this.enhMsgHeaderWidth == 0))
		return;

	// Create a formatted date & time string.  Adjust the message's time to
	// the BBS local time zone if possible.
	var dateTimeStr = "";
	var msgWrittenLocalTime = msgWrittenTimeToLocalBBSTime(pMsgHdr);
	var useBBSLocalTimeZone = false;
	if (msgWrittenLocalTime != -1)
	{
		dateTimeStr = strftime("%a, %d %b %Y %H:%M:%S", msgWrittenLocalTime);
		useBBSLocalTimeZone = true;
	}
	else
		dateTimeStr = pMsgHdr.date.replace(/ [-+][0-9]+$/, "");
	
	// If using the internal header (not loaded externally) and the message is not a poll and
	// contains the properties total_votes and upvotes, then put some information in the header
	// containing information about the message's voting results.
	var msgIsAPoll = false;
	if (typeof(MSG_POLL) != "undefined")
		msgIsAPoll = ((pMsgHdr.attr & MSG_POLL) == MSG_POLL);
	var enhHdrLines = enhMsgHdrLines.slice(0);
	if (this.usingInternalEnhMsgHdr && !msgIsAPoll && pMsgHdr.hasOwnProperty("total_votes") && pMsgHdr.hasOwnProperty("upvotes"))
	{
		// Only add the vote information if the total_votes value is non-zero
		if (pMsgHdr.total_votes != 0)
		{
			var voteInfo = getMsgUpDownvotesAndScore(pMsgHdr);
			var voteStatsTxt = "\x01n\x01c" + RIGHT_T_SINGLE + "\x01h\x01gS\x01n\x01gcore\x01h\x01c: \x01b" + voteInfo.voteScore + " (+" + voteInfo.upvotes + ", -" + voteInfo.downvotes + ")\x01n\x01c" + LEFT_T_SINGLE;
			enhHdrLines[6] = enhHdrLines[6].slice(0, 10) + "\x01n\x01c" + voteStatsTxt + "\x01n\x01c" + HORIZONTAL_SINGLE + "\x01h\x01k" + enhHdrLines[6].slice(17 + strip_ctrl(voteStatsTxt).length);
		}
	}

	// If the user's terminal supports ANSI, we can move the cursor and
	// display the header where specified.
	if (console.term_supports(USER_ANSI))
	{
		// Display the header starting on the first column and the given screen row.
		var screenX = 1;
		var screenY = (typeof(pStartScreenRow) == "number" ? pStartScreenRow : 1);
		for (var hdrFileIdx = 0; hdrFileIdx < enhHdrLines.length; ++hdrFileIdx)
		{
			console.gotoxy(screenX, screenY++);
			console.putmsg(this.ParseMsgAtCodes(enhHdrLines[hdrFileIdx], pMsgHdr,
			               pDisplayMsgNum, dateTimeStr, useBBSLocalTimeZone, false));
		}
		// Older - Used to center the header lines, but I'm not sure this is necessary,
		// and it might even make the header off by one, which could be bad.
		// Display the message header information.  Make sure the header lines are
		// centered properly in case the user's terminal is more than 80 characters
		// wide.
		/*
		var screenX = Math.floor(console.screen_columns/2)
		            - Math.floor(this.enhMsgHeaderWidth/2);
		if (console.screen_columns > 80)
			++screenX;
		*/
		// If avatars are available, then show the sender's avatar on the right
		// side
		if (this.displayAvatars && (gAvatar != null) && ((pMsgHdr.attr & MSG_ANONYMOUS) == 0))
		{
			console.gotoxy(1, screenY-1);
			//gAvatar.draw(pMsgHdr.from_ext, pMsgHdr.from, pMsgHdr.from_net_addr, /* above: */true, /* right-justified: */true);
			gAvatar.draw(pMsgHdr.from_ext, pMsgHdr.from, pMsgHdr.from_net_addr, /* above: */true, /* right-justified: */this.rightJustifyAvatar);
			console.attributes = 0;	// Clear the background attribute as the next line might scroll, filling with BG attribute
			// If using the traditional (non-scrolling) user interface, then
			// put the cursor where it should be.  (If using the scrolling
			// interface, the cursor will be placed where it should be elsewhere.)
			if (!this.scrollingReaderInterface)
				console.gotoxy(1, screenY);
		}
	}
	else
	{
		// The user's terminal doesn't support ANSI - So just output the header
		// lines.
		for (var hdrFileIdx = 0; hdrFileIdx < enhHdrLines.length; ++hdrFileIdx)
		{
			console.putmsg(this.ParseMsgAtCodes(enhHdrLines[hdrFileIdx], pMsgHdr,
			               pDisplayMsgNum, dateTimeStr, useBBSLocalTimeZone, false));
		}
		// Note: Avatar display is only supported for ANSI
	}
}

// For the DigDistMsgReader class: Displays the area chooser header
//
// Parameters:
//  pStartScreenRow: The row on the screen at which to start displaying the
//                   header information.  Will be used if the user's terminal
//                   supports ANSI.
//  pClearRowsFirst: Optional boolean - Whether or not to clear the rows first.
//                   Defaults to true.  Only valid if the user's terminal supports
//                   ANSI.
function DigDistMsgReader_DisplayAreaChgHdr(pStartScreenRow, pClearRowsFirst)
{
	if (this.areaChangeHdrLines == null)
		return;
	if (this.areaChangeHdrLines.length == 0)
		return;

	// If the user's terminal supports ANSI and pStartScreenRow is a number, then
	// we can move the cursor and display the header where specified.
	if (console.term_supports(USER_ANSI) && (typeof(pStartScreenRow) == "number"))
	{
		// If specified to clear the rows first, then do so.
		var screenX = 1;
		var screenY = pStartScreenRow;
		var clearRowsFirst = (typeof(pClearRowsFirst) == "boolean" ? pClearRowsFirst : true);
		if (clearRowsFirst)
		{
			console.print("\x01n");
			for (var hdrFileIdx = 0; hdrFileIdx < this.areaChangeHdrLines.length; ++hdrFileIdx)
			{
				console.gotoxy(screenX, screenY++);
				console.cleartoeol();
			}
		}
		// Display the header starting on the first column and the given screen row.
		screenX = 1;
		screenY = pStartScreenRow;
		for (var hdrFileIdx = 0; hdrFileIdx < this.areaChangeHdrLines.length; ++hdrFileIdx)
		{
			console.gotoxy(screenX, screenY++);
			console.print(this.areaChangeHdrLines[hdrFileIdx]);
			//console.putmsg(this.areaChangeHdrLines[hdrFileIdx]);
			//console.cleartoeol("\x01n"); // Shouldn't do this, as it resets color attributes
		}
	}
	else
	{
		// The user's terminal doesn't support ANSI or pStartScreenRow is not a
		// number - So just output the header lines.
		for (var hdrFileIdx = 0; hdrFileIdx < this.areaChangeHdrLines.length; ++hdrFileIdx)
		{
			console.print(this.areaChangeHdrLines[hdrFileIdx]);
			//console.putmsg(this.areaChangeHdrLines[hdrFileIdx]);
			//console.cleartoeol("\x01n"); // Shouldn't do this, as it resets color attributes
			console.crlf();
		}
	}
}

// For the DigDistMsgReader class: Displays the whole/initial scrollbar for a message
// in enhanced reader mode.
//
// Parameters:
//  pSolidBlockStartRow: The starting row for the solid/bright blocks
//  pNumSolidBlocks: The number of solid/bright blocks to write
function DigDistMsgReader_DisplayEnhancedReaderWholeScrollbar(pSolidBlockStartRow, pNumSolidBlocks)
{
   //console.print("\x01n");
   var numSolidBlocksWritten = 0;
   var wroteBrightBlockColor = false;
   var wroteDimBlockColor = false;
   for (var screenY = this.msgAreaTop; screenY <= this.msgAreaBottom; ++screenY)
   {
      console.gotoxy(this.msgAreaRight+1, screenY);
      if ((screenY >= pSolidBlockStartRow) && (numSolidBlocksWritten < pNumSolidBlocks))
      {
         if (!wroteBrightBlockColor)
         {
            //console.print("\x01h\x01w");
			console.print("\x01n" + this.colors.scrollbarScrollBlockColor);
            wroteBrightBlockColor = true;
            wroteDimBlockColor = false;
         }
         //console.print(BLOCK2);
		 console.print(this.text.scrollbarScrollBlockChar);
         ++numSolidBlocksWritten;
      }
      else
      {
         if (!wroteDimBlockColor)
         {
            //console.print("\x01h\x01k");
			console.print("\x01n" + this.colors.scrollbarBGColor);
            wroteDimBlockColor = true;
         }
         //console.print(BLOCK1);
		 console.print(this.text.scrollbarBGChar);
      }
   }
}

// For the DigDistMsgReader class: Updates the scrollbar for a message, for use
// in enhanced reader mode.  This does only the necessary character updates to
// minimize the number of characters that need to be updated on the screen.
//
// Parameters:
//  pNewStartRow: The new (current) start row for solid/bright blocks
//  pOldStartRow: The old start row for solid/bright blocks
//  pNumSolidBlocks: The number of solid/bright blocks
function DigDistMsgReader_UpdateEnhancedReaderScollbar(pNewStartRow, pOldStartRow, pNumSolidBlocks)
{
   // Calculate the difference in the start row.  If the difference is positive,
   // then the solid block section has moved down; if the diff is negative, the
   // solid block section has moved up.
   var solidBlockStartRowDiff = pNewStartRow - pOldStartRow;
   var oldLastRow = pOldStartRow + pNumSolidBlocks - 1;
   var newLastRow = pNewStartRow + pNumSolidBlocks - 1;
   if (solidBlockStartRowDiff > 0)
   {
      // The solid block section has moved down
      if (pNewStartRow > oldLastRow)
      {
         // No overlap
         // Write dim blocks over the old solid block section
         //console.print("\x01n\x01h\x01k");
		 console.print("\x01n" + this.colors.scrollbarBGColor);
         for (var screenY = pOldStartRow; screenY <= oldLastRow; ++screenY)
         {
            console.gotoxy(this.msgAreaRight+1, screenY);
            //console.print(BLOCK1);
			console.print(this.text.scrollbarBGChar);
         }
         // Write solid blocks in the new locations
         //console.print("\x01w");
		 console.print("\x01n" + this.colors.scrollbarScrollBlockColor);
         for (var screenY = pNewStartRow; screenY <= newLastRow; ++screenY)
         {
            console.gotoxy(this.msgAreaRight+1, screenY);
            //console.print(BLOCK2);
			console.print(this.text.scrollbarScrollBlockChar);
         }
      }
      else
      {
         // There is some overlap
         // Write dim blocks on top
         //console.print("\x01n\x01h\x01k");
		 console.print("\x01n" + this.colors.scrollbarBGColor);
         for (var screenY = pOldStartRow; screenY < pNewStartRow; ++screenY)
         {
            console.gotoxy(this.msgAreaRight+1, screenY);
            //console.print(BLOCK1);
			console.print(this.text.scrollbarBGChar);
         }
         // Write bright blocks on the bottom
         //console.print("\x01w");
		 console.print("\x01n" + this.colors.scrollbarScrollBlockColor);
         for (var screenY = oldLastRow+1; screenY <= newLastRow; ++screenY)
         {
            console.gotoxy(this.msgAreaRight+1, screenY);
            //console.print(BLOCK2);
			console.print(this.text.scrollbarScrollBlockChar);
         }
      }
   }
   else if (solidBlockStartRowDiff < 0)
   {
      // The solid block section has moved up
      if (pOldStartRow > newLastRow)
      {
         // No overlap
         // Write dim blocks over the old solid block section
         //console.print("\x01n\x01h\x01k");
		 console.print("\x01n" + this.colors.scrollbarBGColor);
         for (var screenY = pOldStartRow; screenY <= oldLastRow; ++screenY)
         {
            console.gotoxy(this.msgAreaRight+1, screenY);
            //console.print(BLOCK1);
			console.print(this.text.scrollbarBGChar);
         }
         // Write solid blocks in the new locations
         //console.print("\x01w");
		 console.print("\x01n" + this.colors.scrollbarScrollBlockColor);
         for (var screenY = pNewStartRow; screenY <= newLastRow; ++screenY)
         {
            console.gotoxy(this.msgAreaRight+1, screenY);
            //console.print(BLOCK2);
			console.print(this.text.scrollbarScrollBlockChar);
         }
      }
      else
      {
         // There is some overlap
         // Write bright blocks on top
         //console.print("\x01n\x01h\x01w");
		 console.print("\x01n" + this.colors.scrollbarScrollBlockColor);
         var endRow = pOldStartRow;
         for (var screenY = pNewStartRow; screenY < endRow; ++screenY)
         {
            console.gotoxy(this.msgAreaRight+1, screenY);
            //console.print(BLOCK2);
			console.print(this.text.scrollbarScrollBlockChar);
         }
         // Write dim blocks on the bottom
         //console.print("\x01k");
		 console.print("\x01n" + this.colors.scrollbarBGColor);
         endRow = pOldStartRow + pNumSolidBlocks;
         for (var screenY = pNewStartRow+pNumSolidBlocks; screenY < endRow; ++screenY)
         {
            console.gotoxy(this.msgAreaRight+1, screenY);
            //console.print(BLOCK1);
			console.print(this.text.scrollbarBGChar);
         }
      }
   }
}

// For the DigDistMsgReader class: Returns whether a particular message is
// marked as deleted.  If the message base object is not open or the given
// offset is out of bounds, this method will return true.
//
// Parameters:
//  pOffset: The offset of the message to check
//
// Return value: Boolean - Whether or not the message is marked as deleted
function DigDistMsgReader_MessageIsDeleted(pOffset)
{
	var msgDeleted = false;
	var msgbase = new MsgBase(this.subBoardCode);
	if (msgbase.open())
	{
		if ((pOffset < 0) || (pOffset >= this.NumMessages(msgbase)))
			msgDeleted = true;
		else
		{
			// Get the message's header and see if it's marked as deleted
			var msgHdr = this.GetMsgHdrByIdx(pOffset, false, msgbase);
			if (msgHdr != null)
				msgDeleted = ((msgHdr.attr & MSG_DELETE) == MSG_DELETE);
		}
		msgbase.close();
	}

	return msgDeleted;
}

// For the DigDistMsgReader class: Returns whether a particular message is the
// last post in the sub-board from the current logged-in user.
//
// Parameters:
//  pOffset: The offset of the message to check
//
// Return value: Boolean - Whether or not the message is the last post in the
//               sub-board from the current logged-in user.
function DigDistMsgReader_MessageIsLastFromUser(pOffset)
{
	var msgIstLastFromUser = false;
	var msgbase = new MsgBase(this.subBoardCode);
	if (msgbase.open())
	{
		if (msgbase.cfg != null)
		{
			// TODO: Update to handle search results?
			if ((pOffset >= 0) && (pOffset < msgbase.total_msgs))
			{
				// First, see if the message at pOffset was posted by the user.  If it
				// is, then look for the last message posted by the logged-in user and
				// if found, see if that message has the same offset as the offset
				// passed in.
				var msgHdr = msgbase.get_msg_header(true, pOffset, false);
				if (userHandleAliasNameMatch(msgHdr["to"]))
				{
					var lastMsgOffsetFromUser = -1;
					for (var msgOffset = msgbase.total_msgs-1; (msgOffset >= pOffset) && (lastMsgOffsetFromUser == -1); --msgOffset)
					{
						msgHdr = msgbase.get_msg_header(true, msgOffset, false);
						if (userHandleAliasNameMatch(msgHdr["to"]))
							lastMsgOffsetFromUser = msgOffset;
					}
					// See if the passed-in offset is the last message we found from
					// the logged-in user.
					msgIstLastFromUser = (lastMsgOffsetFromUser == pOffset);
				}
			}
		}
		msgbase.close();
	}
	return msgIstLastFromUser;
}

// For the DigDistMsgReader class enhanced reader mode: Displays an error at the
// bottom of the message area for a moment, then refreshes the last 2 lines in
// the message area.  If the message string that is passed in is empty or not
// a string, then this will simply refresh the last 2 lines of the message area.
//
// Parameters:
//  pErrorMsg: The error message to show
//  pMessageLines: The array of lines from the message being displayed
//  pTopLineIdx: The index of the line being displayed at the top of the message area
//  pMsgLineFormatStr: Optional - The format string for message lines
function DigDistMsgReader_DisplayEnhReaderError(pErrorMsg, pMessageLines, pTopLineIdx,
                                                pMsgLineFormatStr)
{
   var msgLineFormatStr = "";
   if (typeof(pMsgLineFormatStr) == "string")
      msgLineFormatStr = pMsgLineFormatStr;
   else
      msgLineFormatStr = "%-" + this.msgAreaWidth + "s";

   var originalCurpos = console.getxy();
   // Move the cursor to the 2nd to last row of the screen and
   // show the error.  Ideally, I'd like
   // to put the cursor on the last row of the screen for this, but
   // console.getnum() lets the enter key shift everything on screen
   // up one row, and there's no way to avoid that.  So, to optimize
   // screen refreshing, the cursor is placed on the 2nd to the last
   // row on the screen to prompt for the message number.
   var promptPos = { x: this.msgAreaLeft, y: console.screen_rows-1 };
   // Write a line of characters above where the prompt will be placed,
   // to help get the user's attention.
   console.gotoxy(promptPos.x, promptPos.y-1);
   console.print("\x01n" + this.colors.enhReaderPromptSepLineColor);
   for (var lineCounter = 0; lineCounter < this.msgAreaWidth; ++lineCounter)
      console.print(HORIZONTAL_SINGLE);
   // Clear the inside of the message area, so as not to overwrite
   // the scrollbar character
   console.print("\x01n");
   console.gotoxy(promptPos);
   for (var lineCounter = 0; lineCounter < this.msgAreaWidth; ++lineCounter)
      console.print(" ");
   // Show the error if a valid error message was passed in
   if ((typeof(pErrorMsg) == "string") && (console.strlen(pErrorMsg) > 0))
   {
      writeWithPause(this.msgAreaLeft, console.screen_rows-1, pErrorMsg,
                     ERROR_PAUSE_WAIT_MS, "\x01n", true);
   }
   // Figure out the indexes of the message for the last lines of
   // the message and update that line on the screen.
   // If the index is valid, then output that message line; otherwise,
   // output a blank line.
   --promptPos.y;
   var msgLine = null;
   var msgLineIndex = pTopLineIdx + this.msgAreaHeight - 2;
   for (; msgLineIndex <= pTopLineIdx + this.msgAreaHeight - 1; ++msgLineIndex)
   {
      console.gotoxy(promptPos);
      console.print("\x01n" + this.colors["msgBodyColor"]);
      if ((msgLineIndex >= 0) && (msgLineIndex < pMessageLines.length))
      {
         console.print(pMessageLines[msgLineIndex]); // Already shortened to fit
         console.print("\x01n" + this.colors["msgBodyColor"]); // In case colors changed
         // Clear the rest of the line
         printf("%" + +(this.msgAreaWidth-console.strlen(pMessageLines[msgLineIndex])) + "s", "");
      }
      else
         printf(msgLineFormatStr, "");
      ++promptPos.y;
   }
   // Move the cursor back to its original position
   console.gotoxy(originalCurpos);
}

// For the DigDistMsgReader class enhanced reader mode: Prompts for a yes/no
// question at the bottom of the message area and refreshes the last 2 lines in
// the message area when the user has given an answer.  If the question string
// that is passed in is empty or not a string, then this will simply refresh the
// last 2 lines of the message area, and the return value will default to true.
//
// Parameters:
//  pQuestion: The yes/no question to display, without the ? on the end
//  pMessageLines: The array of lines from the message being displayed
//  pTopLineIdx: The index of the line being displayed at the top of the message area
//  pMsgLineFormatStr: The format string for message lines
//  pSolidScrollBlockStartRow: The starting row for solid scroll blocks (purely for
//                             the kludge of updating the last scrollbar block on the
//                             screen because the yes/no function erases it)
//  pNumSolidScrollBlocks: The number of solid scroll blocks (purely for the kludge of
//                         updating the last scrollbar block on the screen because the
//                         yes/no function erases it)
//  pNoYes: Optional boolean - Whether the default response should be No instead of
//          Yes.  Defaults to false (for a default Yes response).
//
// Return value: Boolean - True if the user selected yes, or false if the user selected no.
//               If the question string passed in is 0-length or not a valid string, the
//               return value will be true.
function DigDistMsgReader_EnhReaderPromptYesNo(pQuestion, pMessageLines, pTopLineIdx,
                                                pMsgLineFormatStr, pSolidScrollBlockStartRow,
                                                pNumSolidScrollBlocks, pDefaultNo)
{
	var msgLineFormatStr = "";
	if (typeof(pMsgLineFormatStr) == "string")
		msgLineFormatStr = pMsgLineFormatStr;
	else
		msgLineFormatStr = "%-" + +(this.msgAreaWidth) + "s";

	var originalCurpos = console.getxy();
	// Move the cursor to the 2nd to last row of the screen and
	// show the error.  Ideally, I'd like
	// to put the cursor on the last row of the screen for this, but
	// console.getnum() lets the enter key shift everything on screen
	// up one row, and there's no way to avoid that.  So, to optimize
	// screen refreshing, the cursor is placed on the 2nd to the last
	// row on the screen to prompt for the message number.
	var promptPos = { x: this.msgAreaLeft, y: console.screen_rows-1 };
	// Write a line of characters above where the prompt will be placed,
	// to help get the user's attention.
	console.gotoxy(promptPos.x, promptPos.y-1);
	console.print("\x01n" + this.colors.enhReaderPromptSepLineColor);
	for (var lineCounter = 0; lineCounter < this.msgAreaWidth; ++lineCounter)
	console.print(HORIZONTAL_SINGLE);
	// Clear the inside of the message area, so as not to overwrite
	// the scrollbar character
	console.print("\x01n");
	console.gotoxy(promptPos);
	for (var lineCounter = 0; lineCounter < this.msgAreaWidth; ++lineCounter)
	console.print(" ");
	// Prompt the question if a valid question string was passed in
	var yesNoResponse = true;
	if ((typeof(pQuestion) == "string") && (console.strlen(pQuestion) > 0))
	{
		console.gotoxy(this.msgAreaLeft, console.screen_rows-1);
		var defaultResponseNo = (typeof(pDefaultNo) == "boolean" ? pDefaultNo : false);
		if (defaultResponseNo)
			yesNoResponse = !console.noyes(pQuestion);
		else
			yesNoResponse = console.yesno(pQuestion);
		// Kludge: Update the last scroll block on the screen, since the yes/no
		// prompt erases it.
		//var scrollBlockChar = "\x01n\x01h\x01k" + BLOCK1; // Dim scroll block
		// Dim scroll block
		var scrollBlockChar = this.colors.scrollbarBGColor + this.text.scrollbarBGChar;
		if ((pSolidScrollBlockStartRow >= console.screen_rows-1) ||
		    (pSolidScrollBlockStartRow + pNumSolidScrollBlocks - 1 >= console.screen_rows-1))
		{
			//scrollBlockChar = "\x01n\x01h\x01w" + BLOCK2; // Bright, solid scroll block
			// Bright, solid scroll block
			scrollBlockChar = this.colors.scrollbarScrollBlockColor + this.text.scrollbarScrollBlockChar;
		}
		console.gotoxy(console.screen_columns, console.screen_rows-1);
		console.print(scrollBlockChar);
	}
	// Figure out the indexes of the message for the last lines of
	// the message and update that line on the screen.
	// If the index is valid, then output that message line; otherwise,
	// output a blank line.
	--promptPos.y;
	var msgLine = null;
	var msgLineIndex = pTopLineIdx + this.msgAreaHeight - 2;
	for (; msgLineIndex <= pTopLineIdx + this.msgAreaHeight - 1; ++msgLineIndex)
	{
		console.gotoxy(promptPos);
		console.print("\x01n" + this.colors["msgBodyColor"]);
		if ((msgLineIndex >= 0) && (msgLineIndex < pMessageLines.length))
		{
			console.print(pMessageLines[msgLineIndex]); // Already shortened to fit
			console.print("\x01n" + this.colors["msgBodyColor"]); // In case colors changed
			// Clear the rest of the line
			printf("%" + +(this.msgAreaWidth-console.strlen(pMessageLines[msgLineIndex])) + "s", "");
		}
		else
			printf(msgLineFormatStr, "");
		++promptPos.y;
	}
	// Move the cursor back to its original position
	console.gotoxy(originalCurpos);

	return yesNoResponse;
}

// For the DigDistMsgReader class: Allows the user to delete or undelete a message.  Checks
// whether the message was posted by the user and prompt for confirmation to
// delete it.  Checks for delete or delete_last permission.  If the sub-board has
// delete_last permission enabled, this checks whether the message is the user's
// last post on the sub-board and only lets them delete if so.
//
// Parameters:
//  pOffset: The offset of the message to be deleted
//  pPromptLoc: Optional - An object containing x and y properties for the location
//              on the console of the prompt/error messages
//  pDelete: Optional boolean: If true (default), deletes messages; if false, undeletes messages.
//  pClearPromptRowAtFirstUse: Optional - A boolean to specify whether or not to
//                             clear the remainder of the prompt row the first
//                             time text is written in that row.
//  pPromptRowWidth: Optional - The width of the prompt row (if pProptLoc is valid)
//  pConfirm: Optional boolean - Whether or not to confirm deleting/undeleting the message. Defaults to true.
//
// Return value: Boolean - Whether or not the message was deleted
function DigDistMsgReader_PromptAndDeleteOrUndeleteMessage(pOffset, pPromptLoc, pDelete, pClearPromptRowAtFirstUse,
                                                           pPromptRowWidth, pConfirm)
{
	// Sanity checking
	if ((pOffset == null) || (typeof(pOffset) != "number"))
		return false;
	if (!this.CanDelete() && !this.CanDeleteLastMsg())
		return false;
	var msgbase = new MsgBase(this.subBoardCode);
	if (!msgbase.open())
		return false;
	if ((pOffset < 0) || (pOffset >= this.NumMessages(msgbase)))
	{
		msgbase.close();
		return false;
	}
	var promptLocValid = ((pPromptLoc != null) && (typeof(pPromptLoc) == "object") &&
	                      (typeof(pPromptLoc.x) == "number") && (typeof(pPromptLoc.y) == "number"));

	var deleteMsg = (typeof(pDelete) === "boolean" ? pDelete : true);

	var opSucceeded = false;

	var msgNum = pOffset + 1;
	var msgHeader = this.GetMsgHdrByIdx(pOffset, false, msgbase);

	var errorMessage = "";
	var continueOn = false;
	if (deleteMsg)
	{
		// If it's already marked for deletion, then nothing needs to be done
		if ((msgHeader.attr & MSG_DELETE) == MSG_DELETE)
			opSucceeded = true;
		else
		{
			// The message is not marked for deletion.
			if (this.CanDelete())
			{
				if (msgHeader != null)
					continueOn = user.is_sysop || userHandleAliasNameMatch(msgHeader.from) || this.readingPersonalEmail;
				else
					continueOn = false;
			}
			else if (this.CanDeleteLastMsg())
			{
				continueOn = user.is_sysop || this.MessageIsLastFromUser(pOffset);
				if (!continueOn)
					errorMessage = replaceAtCodesInStr(format(this.text.cannotDeleteMsgText_notLastPostedMsg, msgNum));
			}
			else
				errorMessage = replaceAtCodesInStr(format(this.text.cannotDeleteMsgText_notYoursNotASysop, msgNum));
		}
	}
	else
	{
		// Undeleting a message marked for deletion
		if ((msgHeader.attr & MSG_DELETE) == MSG_DELETE)
			continueOn = true;
		else
			opSucceeded = true; // No action needed
	}
	if (continueOn)
	{
		// Determine whether or not to delete/undelete the message.  First, if we are to
		// have the user confirm whether to delete the message, then ask the
		// user to confirm first.  If we're not to have the user confirm, then
		// go ahead and delete the message.
		continueOn = true; // True in case of not confirming deletion
		var confirmOp = (typeof(pConfirm) == "boolean" ? pConfirm : true);
		if (confirmOp)
		{
			var confirmText = "\x01n";
			if (deleteMsg)
				confirmText += replaceAtCodesInStr(format(this.text.msgDelConfirmText, msgNum));
			else
				confirmText += replaceAtCodesInStr(format(this.text.msgUndelConfirmText, msgNum));
			if (promptLocValid)
			{
				// If the caller wants to clear the remainder of the row where the prompt
				// text will be, then do it.
				if (pClearPromptRowAtFirstUse)
				{
					// Adding 5 to the prompt text to account for the ? and "[X] " that
					// will be added when console.noyes() is called
					var promptTxtLen = console.strlen(confirmText) + 5;
					var numCharsRemaining = 0;
					if (typeof(pPromptRowWidth) == "number")
						numCharsRemaining = pPromptRowWidth - promptTxtLen;
					else
						numCharsRemaining = console.screen_columns - pPromptLoc.x - promptTxtLen;
					console.print("\x01n");
					console.gotoxy(pPromptLoc.x+promptTxtLen, pPromptLoc.y);
					for (var i = 0; i < numCharsRemaining; ++i)
						console.print(" ");
				}
				// Move the cursor to the prompt location
				console.gotoxy(pPromptLoc);
			}
			continueOn = !console.noyes(confirmText);
		}
		// If we are to delete/undelete the message, then do it.
		if (continueOn)
		{
			if (deleteMsg)
			{
				//opSucceeded = msgbase.remove_msg(true, msgHeader.offset);
				opSucceeded = msgbase.remove_msg(false, msgHeader.number);
			}
			else
			{
				var tmpMsgHdr = msgbase.get_msg_header(false, msgHeader.number, false);
				if (tmpMsgHdr != null)
				{
					tmpMsgHdr.attr = tmpMsgHdr.attr ^ MSG_DELETE;
					opSucceeded = msgbase.put_msg_header(false, msgHeader.number, tmpMsgHdr);
				}
				else
					opSucceeded = false;
			}
			if (opSucceeded)
			{
				// Delete/undelete any vote response messages for this message
				// Delete/undelete vote message headers
				var voteDelRetObj = toggleVoteMsgsDeleted(msgbase, msgHeader.number, msgHeader.id, deleteMsg, (this.subBoardCode == "mail"));
				// In case there are search results or saved message headers, refresh the header in
				// those arrays to enable the deleted attribute.
				this.RefreshHdrInSavedArrays(pOffset, MSG_DELETE, deleteMsg);
				if (!voteDelRetObj.allVoteMsgsAffected)
				{
					console.print("\x01n");
					console.crlf();
					console.print("\x01y\x01h* Failed to " + (deleteMsg ? "delete" : "undelete") + " all vote response messages for message " + msgNum + "\x01n");
					console.crlf();
					console.pause();
				}

				// Output a message saying the message has been marked for deletion/undeletion
				if (promptLocValid)
					console.gotoxy(pPromptLoc);
				if (deleteMsg)
					console.print("\x01n" + replaceAtCodesInStr(format(this.text.msgDeletedText, msgNum)));
				else
					console.print("\x01n" + replaceAtCodesInStr(format(this.text.msgUndeletedText, msgNum)));
				if (promptLocValid)
					console.inkey(K_NOSPIN|K_NOCRLF|K_NOECHO, ERROR_PAUSE_WAIT_MS);
				else
				{
					console.crlf();
					console.pause();
				}
			}
		}
	}
	else
	{
		if (errorMessage.length > 0)
		{
			if (promptLocValid)
				console.gotoxy(pPromptLoc);
			console.print(errorMessage);
			if (promptLocValid)
				console.inkey(K_NOSPIN|K_NOCRLF|K_NOECHO, ERROR_PAUSE_WAIT_MS);
			else
			{
				console.crlf();
				console.pause();
			}
		}
	}
	msgbase.close();
	return opSucceeded;
}

// For the DigDistMsgReader class: Allows the user to batch delete selected messages.
// Prompts the user for confirmation to delete the selected messages.
//
// Parameters:
//  pPromptLoc: Optional - An object containing x and y properties for the location
//              on the console of the prompt/error messages
//  pDelete: Optional boolean: If true (default), deletes messages; if false, undeletes messages.
//  pClearPromptRowAtFirstUse: Optional - A boolean to specify whether or not to
//                             clear the remainder of the prompt row the first
//                             time text is written in that row.
//  pPromptRowWidth: Optional - The width of the prompt row (if pProptLoc is valid)
//  pConfirm: Optional boolean - Whether or not to confirm deleting/undeleting the messages. Defaults to true.
//
// Return value: Boolean - Whether or not all messages were deleted
function DigDistMsgReader_PromptAndDeleteOrUndeleteSelectedMessages(pPromptLoc, pDelete, pClearPromptRowAtFirstUse,
                                                          pPromptRowWidth, pConfirm)
{
	var promptLocValid = ((pPromptLoc != null) && (typeof(pPromptLoc) === "object") &&
	                      (typeof(pPromptLoc.x) === "number") && (typeof(pPromptLoc.y) === "number"));

	var doDelete = (typeof(pDelete) === "boolean" ? pDelete : true);
	var allMsgsOpSuccessful = false;

	var errorMsg = ""; // In case anything goes wrong
	var continueOn = true; // Whether or not to continue with deletion/undeletion, depending on user confirmation etc.
	if (doDelete)
	{
		// If not all the messages can be deleted, then don't allow it.
		continueOn = this.AllSelectedMessagesCanBeDeleted();
		if (!this.AllSelectedMessagesCanBeDeleted())
		{
			continueOn = false;
			errorMsg = replaceAtCodesInStr(this.text.cannotDeleteAllSelectedMsgsText);
		}
	}
	if (continueOn)
	{
		// Determine whether or not to delete the message.  First, if we are to
		// have the user confirm whether to delete the message, then ask the
		// user to confirm first.  If we're not to have the user confirm, then
		// go ahead and delete the message.
		var confirmDoIt = (typeof(pConfirm) === "boolean" ? pConfirm : true);
		if (confirmDoIt)
		{
			if (promptLocValid)
			{
				var promptText = "";
				if (doDelete)
					promptText = replaceAtCodesInStr(this.text.delSelectedMsgsConfirmText);
				else
					promptText = replaceAtCodesInStr(this.text.undelSelectedMsgsConfirmText);
				// If the caller wants to clear the remainder of the row where the prompt
				// text will be, then do it.
				if (pClearPromptRowAtFirstUse)
				{
					// Adding 5 to the prompt text to account for the ? and "[X] " that
					// will be added when console.noyes() is called
					var promptTxtLen = console.strlen(promptText) + 5;
					var numCharsRemaining = 0;
					if (typeof(pPromptRowWidth) == "number")
						numCharsRemaining = pPromptRowWidth - promptTxtLen;
					else
						numCharsRemaining = console.screen_columns - pPromptLoc.x - promptTxtLen;
					console.print("\x01n");
					console.gotoxy(pPromptLoc.x+promptTxtLen, pPromptLoc.y);
					for (var i = 0; i < numCharsRemaining; ++i)
						console.print(" ");
				}
				// Move the cursor to the prompt location
				console.gotoxy(pPromptLoc);
				continueOn = !console.noyes(promptText);
			}
		}
		// If we are to delete/undelete the messages, then do so.
		if (continueOn)
		{
			// TODO: Return status & error message
			var deleteRetObj = this.DeleteOrUndeleteSelectedMessages(doDelete);
			allMsgsOpSuccessful = deleteRetObj.opSuccessful;
			// If all selected messages were successfully deleted, then output
			// a success message.  Otherwise, output an error.
			var statusMsg = "";
			if (deleteRetObj.opSuccessful)
				statusMsg = "\x01n\x01cAll selected messages were " + (doDelete ? "deleted." : "undeleted.");
			else
				statusMsg = "\x01n\x01h\x01y* Failure to " + (doDelete ? "delete" : "undelete") + " all selected messages";
			if (promptLocValid)
			{
				console.gotoxy(pPromptLoc);
				console.print("\x01n");
				console.cleartoeol();
			}
			else
				console.crlf();
			console.print(statusMsg);
			if (promptLocValid)
				console.inkey(K_NOSPIN|K_NOCRLF|K_NOECHO, ERROR_PAUSE_WAIT_MS);
			else
			{
				console.crlf();
				console.pause();
			}
		}
	}

	// If there was an error, then display it
	if (errorMsg.length > 0)
	{
		if (promptLocValid)
			console.gotoxy(pPromptLoc);
		console.print(replaceAtCodesInStr(this.text.cannotDeleteAllSelectedMsgsText));
		if (promptLocValid)
			console.inkey(K_NOSPIN|K_NOCRLF|K_NOECHO, ERROR_PAUSE_WAIT_MS);
		else
		{
			console.crlf();
			console.pause();
		}
	}

	return allMsgsOpSuccessful;
}

///////////////////////////////////////////////////////////////////////////////////
// Methods for message group/sub-board choosing

// For the DigDistMsgReader class: Writes the line of key help at the bottom
// row of the screen.
function DigDistMsgReader_WriteLightbarChgMsgAreaKeysHelpLine()
{
   console.gotoxy(1, console.screen_rows);
   //console.print(this.lightbarAreaChooserHelpLine);
   console.putmsg(this.lightbarAreaChooserHelpLine); // console.putmsg() can process @-codes, which we use for mouse click tracking
   console.print("\x01n");
}

// For the DigDistMsgReader class: Outputs the header line to appear above
// the list of message groups.
//
// Parameters:
//  pNumPages: The number of pages.  This is optional; if this is
//             not passed, then it won't be used.
//  pPageNum: The page number.  This is optional; if this is not passed,
//            then it won't be used.
function DigDistMsgReader_WriteGrpListTopHdrLine1(pNumPages, pPageNum)
{
	var descStr = "Description";
	if ((typeof(pPageNum) == "number") && (typeof(pNumPages) == "number"))
		descStr += "    (Page " + pPageNum + " of " + pNumPages + ")";
	else if ((typeof(pPageNum) == "number") && (typeof(pNumPages) != "number"))
		descStr += "    (Page " + pPageNum + ")";
	else if ((typeof(pPageNum) != "number") && (typeof(pNumPages) == "number"))
		descStr += "    (" + pNumPages + (pNumPages == 1 ? " page)" : " pages)");
	printf(this.msgGrpListHdrPrintfStr, "Group#", descStr, "# Sub-Boards");
	console.cleartoeol("\x01n");
}

// For the DigDistMsgReader class: Outputs the first header line to appear
// above the sub-board list for a message group.
//
// Parameters:
//  pGrpIndex: The index of the message group (assumed to be valid)
//  pNumPages: The number of pages.  This is optional; if this is
//             not passed, then it won't be used.
//  pPageNum: The page number.  This is optional; if this is not passed,
//            then it won't be used.
function DigDistMsgReader_WriteSubBrdListHdrLine(pGrpIndex, pNumPages, pPageNum)
{
	var descFormatStr = "\x01n" + this.colors.areaChooserSubBoardHeaderColor + "Sub-boards of \x01h%-25s     \x01n"
	                  + this.colors.areaChooserSubBoardHeaderColor;
	if ((typeof(pPageNum) == "number") && (typeof(pNumPages) == "number"))
		descFormatStr += "(Page " + pPageNum + " of " + pNumPages + ")";
	else if ((typeof(pPageNum) == "number") && (typeof(pNumPages) != "number"))
		descFormatStr += "(Page " + pPageNum + ")";
	else if ((typeof(pPageNum) != "number") && (typeof(pNumPages) == "number"))
		descFormatStr += "(" + pNumPages + (pNumPages == 1 ? " page)" : " pages)");
	printf(descFormatStr, msg_area.grp_list[pGrpIndex].description.substr(0, 25));
	console.cleartoeol("\x01n");
}

// For the DigDistMsgReader class: Lets the user choose a message group and
// sub-board via numeric input, using a lightbar interface (if enabled and
// if the user's terminal uses ANSI) or a traditional user interface.
function DigDistMsgReader_SelectMsgArea()
{
	if (this.msgListUseLightbarListInterface && console.term_supports(USER_ANSI))
		this.SelectMsgArea_Lightbar();
	else
		this.SelectMsgArea_Traditional();
}

// For the DigDistMsgReader class: Lets the user choose a message group and
// sub-board via numeric input, using a lightbar user interface.
//
// Parameters:
//  pMsgGrp: Optional boolean - Whether to let the user choose a message group first.
//           For internal use.  Defaults to true.
//  pGrpIdx: If pMsgGrp is true, then this specifies the group index so that
//           sub-boards can be displayed.
function DigDistMsgReader_SelectMsgArea_Lightbar(pMsgGrp, pGrpIdx)
{
	// If there are no message groups, then don't let the user
	// choose one.
	if (msg_area.grp_list.length == 0)
	{
		console.clear("\x01n");
		console.print("\x01y\x01hThere are no message groups.\r\n\x01p");
		return;
	}

	// Make a backup of the current message group & sub-board indexes so
	// that later we can tell if the user chose something different.
	var oldGrp = msg_area.sub[this.subBoardCode].grp_index;
	var oldSub = msg_area.sub[this.subBoardCode].index;

	var chooseMsgGrp = (typeof(pMsgGrp) == "boolean" ? pMsgGrp : true);

	// This function displays the header line(s) above the list
	function displayListHdrLines(pStartRow, pChooseMsgGrp, pReader)
	{
		console.gotoxy(1, pStartRow);
		if (pChooseMsgGrp)
			pReader.WriteGrpListHdrLine1();
		else
		{
			pReader.WriteSubBrdListHdrLine(pGrpIdx);
			console.gotoxy(1, pStartRow+1);
			printf(pReader.subBoardListHdrPrintfStr, "Sub #", "Name", "# Posts", "Latest date & time");
		}
	}

	// Clear the screen & write the header lines, help line and group list header
	console.clear("\x01n");
	this.DisplayAreaChgHdr(1);
	displayListHdrLines(this.areaChangeHdrLines.length+1, chooseMsgGrp, this);
	this.WriteChgMsgAreaKeysHelpLine();

	// Create a menu of message groups or sub-boards
	var msgAreaMenu = (chooseMsgGrp ? this.CreateLightbarMsgGrpMenu() : this.CreateLightbarSubBoardMenu(pGrpIdx));
	var drawMenu = true;
	var lastSearchText = "";
	var lastSearchFoundIdx = -1;
	var chosenIdx = -1;
	var continueOn = true;
	// Let the user choose a group, and also respond to other user choices
	while (continueOn)
	{
		chosenIdx = -1;
		var msgGrpIdx = msgAreaMenu.GetVal(drawMenu);
		drawMenu = true;
		var lastUserInputUpper = (typeof(msgAreaMenu.lastUserInput) == "string" ? msgAreaMenu.lastUserInput.toUpperCase() : msgAreaMenu.lastUserInput);
		if (typeof(msgGrpIdx) == "number")
			chosenIdx = msgGrpIdx;
		// If userChoice is not a number, then it should be null in this case,
		// and the user would have pressed one of the additional quit keys set
		// up for the menu.  So look at the menu's lastUserInput and do the
		// appropriate thing.
		else if ((lastUserInputUpper == "Q") || (lastUserInputUpper == KEY_ESC)) // Quit
			continueOn = false;
		else if ((lastUserInputUpper == "/") || (lastUserInputUpper == CTRL_F)) // Start of find
		{
			console.gotoxy(1, console.screen_rows);
			console.cleartoeol("\x01n");
			console.gotoxy(1, console.screen_rows);
			var promptText = "Search: ";
			console.print(promptText);
			var searchText = getStrWithTimeout(K_UPPER|K_NOCRLF|K_GETSTR|K_NOSPIN|K_LINE, console.screen_columns - promptText.length - 1, SEARCH_TIMEOUT_MS);
			lastSearchText = searchText;
			// If the user entered text, then do the search, and if found,
			// found, go to the page and select the item indicated by the
			// search.
			if (searchText.length > 0)
			{
				var oldLastSearchFoundIdx = lastSearchFoundIdx;
				var oldSelectedItemIdx = msgAreaMenu.selectedItemIdx;
				var idx = -1;
				if (chooseMsgGrp)
					idx = findMsgGrpIdxFromText(searchText, msgAreaMenu.selectedItemIdx);
				else
					idx = findSubBoardIdxFromText(pGrpIdx, searchText, msgAreaMenu.selectedItemIdx+1);
				lastSearchFoundIdx = idx;
				if (idx > -1)
				{
					// Set the currently selected item in the menu, and ensure it's
					// visible on the page
					msgAreaMenu.selectedItemIdx = idx;
					if (msgAreaMenu.selectedItemIdx >= msgAreaMenu.topItemIdx+msgAreaMenu.GetNumItemsPerPage())
						msgAreaMenu.topItemIdx = msgAreaMenu.selectedItemIdx - msgAreaMenu.GetNumItemsPerPage() + 1;
					else if (msgAreaMenu.selectedItemIdx < msgAreaMenu.topItemIdx)
						msgAreaMenu.topItemIdx = msgAreaMenu.selectedItemIdx;
					else
					{
						// If the current index and the last index are both on the same page on the
						// menu, then have the menu only redraw those items.
						msgAreaMenu.nextDrawOnlyItems = [msgAreaMenu.selectedItemIdx, oldLastSearchFoundIdx, oldSelectedItemIdx];
					}
				}
				else
				{
					if (chooseMsgGrp)
						idx = findMsgGrpIdxFromText(searchText, 0);
					else
						idx = findSubBoardIdxFromText(pGrpIdx, searchText, 0);
					lastSearchFoundIdx = idx;
					if (idx > -1)
					{
						// Set the currently selected item in the menu, and ensure it's
						// visible on the page
						msgAreaMenu.selectedItemIdx = idx;
						if (msgAreaMenu.selectedItemIdx >= msgAreaMenu.topItemIdx+msgAreaMenu.GetNumItemsPerPage())
							msgAreaMenu.topItemIdx = msgAreaMenu.selectedItemIdx - msgAreaMenu.GetNumItemsPerPage() + 1;
						else if (msgAreaMenu.selectedItemIdx < msgAreaMenu.topItemIdx)
							msgAreaMenu.topItemIdx = msgAreaMenu.selectedItemIdx;
						else
						{
							// The current index and the last index are both on the same page on the
							// menu, so have the menu only redraw those items.
							msgAreaMenu.nextDrawOnlyItems = [msgAreaMenu.selectedItemIdx, oldLastSearchFoundIdx, oldSelectedItemIdx];
						}
					}
					else
					{
						this.WriteLightbarKeyHelpErrorMsg("Not found");
						drawMenu = false;
					}
				}
			}
			else
				drawMenu = false;
			this.WriteChgMsgAreaKeysHelpLine();
		}
		else if (lastUserInputUpper == "N") // Next search result (requires an existing search term)
		{
			// This works but seems a little strange sometimes.
			// - Should this always start from the selected index?
			// - If it wraps around to one of the items on the first page,
			//   should it always set the top index to 0?
			if ((lastSearchText.length > 0) && (lastSearchFoundIdx > -1))
			{
				var oldLastSearchFoundIdx = lastSearchFoundIdx;
				var oldSelectedItemIdx = msgAreaMenu.selectedItemIdx;
				// Do the search, and if found, go to the page and select the item
				// indicated by the search.
				var idx = 0;
				if (chooseMsgGrp)
					idx = findMsgGrpIdxFromText(searchText, lastSearchFoundIdx+1);
				else
					idx = findSubBoardIdxFromText(pGrpIdx, searchText, lastSearchFoundIdx+1);
				if (idx > -1)
				{
					lastSearchFoundIdx = idx;
					// Set the currently selected item in the menu, and ensure it's
					// visible on the page
					msgAreaMenu.selectedItemIdx = idx;
					if (msgAreaMenu.selectedItemIdx >= msgAreaMenu.topItemIdx+msgAreaMenu.GetNumItemsPerPage())
					{
						msgAreaMenu.topItemIdx = msgAreaMenu.selectedItemIdx - msgAreaMenu.GetNumItemsPerPage() + 1;
						if (msgAreaMenu.topItemIdx < 0)
							msgAreaMenu.topItemIdx = 0;
					}
					else if (msgAreaMenu.selectedItemIdx < msgAreaMenu.topItemIdx)
						msgAreaMenu.topItemIdx = msgAreaMenu.selectedItemIdx;
					else
					{
						// The current index and the last index are both on the same page on the
						// menu, so have the menu only redraw those items.
						msgAreaMenu.nextDrawOnlyItems = [msgAreaMenu.selectedItemIdx, oldLastSearchFoundIdx, oldSelectedItemIdx];
					}
				}
				else
				{
					if (chooseMsgGrp)
						idx = findMsgGrpIdxFromText(searchText, 0);
					else
						idx = findSubBoardIdxFromText(pGrpIdx, searchText, 0);
					lastSearchFoundIdx = idx;
					if (idx > -1)
					{
						// Set the currently selected item in the menu, and ensure it's
						// visible on the page
						msgAreaMenu.selectedItemIdx = idx;
						if (msgAreaMenu.selectedItemIdx >= msgAreaMenu.topItemIdx+msgAreaMenu.GetNumItemsPerPage())
						{
							msgAreaMenu.topItemIdx = msgAreaMenu.selectedItemIdx - msgAreaMenu.GetNumItemsPerPage() + 1;
							if (msgAreaMenu.topItemIdx < 0)
								msgAreaMenu.topItemIdx = 0;
						}
						else if (msgAreaMenu.selectedItemIdx < msgAreaMenu.topItemIdx)
							msgAreaMenu.topItemIdx = msgAreaMenu.selectedItemIdx;
						else
						{
							// The current index and the last index are both on the same page on the
							// menu, so have the menu only redraw those items.
							msgAreaMenu.nextDrawOnlyItems = [msgAreaMenu.selectedItemIdx, oldLastSearchFoundIdx, oldSelectedItemIdx];
						}
					}
					else
					{
						this.WriteLightbarKeyHelpErrorMsg("Not found");
						drawMenu = false;
						this.WriteChgMsgAreaKeysHelpLine();
					}
				}
			}
			else
			{
				this.WriteLightbarKeyHelpErrorMsg("There is no previous search", REFRESH_MSG_AREA_CHG_LIGHTBAR_HELP_LINE);
				drawMenu = false;
				this.WriteChgMsgAreaKeysHelpLine();
			}
		}
		else if (lastUserInputUpper == "?") // Show help
		{
			this.ShowChooseMsgAreaHelpScreen(true, true);
			console.pause();
			// Refresh the screen
			console.clear("\x01n");
			console.gotoxy(1, 1);
			this.DisplayAreaChgHdr(1);
			displayListHdrLines(this.areaChangeHdrLines.length+1, chooseMsgGrp, this);
			this.WriteChgMsgAreaKeysHelpLine();
		}
		// If the user entered a numeric digit, then treat it as
		// the start of the message group number.
		if (lastUserInputUpper.match(/[0-9]/))
		{
			// Put the user's input back in the input buffer to
			// be used for getting the rest of the message number.
			console.ungetstr(lastUserInputUpper);
			// Move the cursor to the bottom of the screen and
			// prompt the user for the message number.
			console.gotoxy(1, console.screen_rows);
			console.clearline("\x01n");
			console.print("\x01cChoose group #: \x01h");
			var userInput = console.getnum(msg_area.grp_list.length);
			if (userInput > 0)
				chosenIdx = userInput - 1;
			else
			{
				// The user didn't make a selection.  So, we need to refresh
				// the screen due to everything being moved up one line.
				displayListHdrLines(this.areaChangeHdrLines.length+1, chooseMsgGrp, this);
				this.WriteChgMsgAreaKeysHelpLine();
			}
		}

		// If a group/sub-board was chosen, then deal with it.
		if (chosenIdx > -1)
		{
			// If choosing a message group, then let the user choose a
			// sub-board within the group.  Otherwise, return the user's
			// chosen sub-board.
			if (chooseMsgGrp)
			{
				//SelectMsgArea_Lightbar(pMsgGrp, pGrpIdx)
				// Show a "Loading..." text in case there are many sub-boards in
				// the chosen message group
				console.crlf();
				console.print("\x01nLoading...");
				console.line_counter = 0; // To prevent a pause before the message list comes up
				// Ensure that the sub-board printf information is created for
				// the chosen message group.
				this.BuildSubBoardPrintfInfoForGrp(chosenIdx);
				var chosenSubBoardIdx = this.SelectMsgArea_Lightbar(false, chosenIdx);
				if (chosenSubBoardIdx > -1)
				{
					// Set the current group & sub-board
					bbs.curgrp = chosenIdx;
					bbs.cursub = chosenSubBoardIdx;
					continueOn = false;
				}
				else
				{
					// A sub-board was not chosen, so we'll have to re-draw
					// the header and list of message groups.
					displayListHdrLines(this.areaChangeHdrLines.length+1, chooseMsgGrp, this);
				}
			}
			else
				return chosenIdx; // Return the chosen sub-board index
		}
	}

	// If the user chose a different message group & sub-board, then reset the
	// lister index & cursor variables, as well as this.subBoardCode, etc.
	if ((bbs.curgrp != oldGrp) || (bbs.cursub != oldSub))
	{
		this.tradListTopMsgIdx = -1;
		this.lightbarListTopMsgIdx = -1;
		this.lightbarListSelectedMsgIdx = -1;
		this.lightbarListCurPos = null;
		this.setSubBoardCode(msg_area.grp_list[bbs.curgrp].sub_list[bbs.cursub].code);
	}
}

// For the DigDistMsgReader class: Lets the user choose a message group and
// sub-board via numeric input, using a traditional user interface.
function DigDistMsgReader_SelectMsgArea_Traditional()
{
	// TODO: Allow searching
	// If there are no message groups, then don't let the user
	// choose one.
	if (msg_area.grp_list.length == 0)
	{
		console.clear("\x01n");
		console.print("\x01y\x01hThere are no message groups.\r\n\x01p");
		return;
	}

	// Make a backup of the current message group & sub-board indexes so
	// that later we can tell if the user chose something different.
	var oldGrp = msg_area.sub[this.subBoardCode].grp_index;
	var oldSub = msg_area.sub[this.subBoardCode].index;
	// Older:
	/*
	var oldGrp = bbs.curgrp;
	var oldSub = bbs.cursub;
	*/

	// Show the message groups & sub-boards and let the user choose one.
	var selectedGrp = 0;      // The user's selected message group
	var selectedSubBoard = 0; // The user's selected sub-board
	var grpSearchText = "";
	var continueChoosingMsgGroup = true;
	while (continueChoosingMsgGroup)
	{
		// Clear the BBS command string to make sure there are no extra
		// commands in there that could cause weird things to happen.
		bbs.command_str = "";

		console.clear("\x01n");
		this.DisplayAreaChgHdr();
		//console.crlf();
		this.ListMsgGrps(grpSearchText);
		console.crlf();
		console.print("\x01n\x01b\x01h" + TALL_UPPER_MID_BLOCK + " \x01n\x01cWhich, \x01h/\x01n\x01c or \x01hCTRL-F\x01n\x01c, \x01hQ\x01n\x01cuit, or [\x01h" +
		              +(msg_area.sub[this.subBoardCode].grp_index+1) + "\x01n\x01c]: \x01h");
		// Accept Q (quit), / or CTRL_F (Search) or a file library number
		selectedGrp = console.getkeys("Q/" + CTRL_F, msg_area.grp_list.length);

		// If the user just pressed enter (selectedGrp would be blank),
		// default to the current group.
		if (selectedGrp.toString() == "")
			selectedGrp = msg_area.sub[this.subBoardCode].grp_index + 1;
		// Older:
		/*
		if (selectedGrp.toString() == "")
			selectedGrp = bbs.curgrp + 1;
		*/

		if (selectedGrp.toString() == "Q")
			continueChoosingMsgGroup = false;
		// / or CTRL-F: Search
		else if ((selectedGrp.toString() == "/") || (selectedGrp.toString() == CTRL_F))
		{
			console.crlf();
			var searchPromptText = "\x01n\x01c\x01hSearch\x01g: \x01n";
			console.print(searchPromptText);
			var searchText = console.getstr("", console.screen_columns-strip_ctrl(searchPromptText).length-1, K_UPPER|K_NOCRLF|K_GETSTR|K_NOSPIN|K_LINE);
			if (searchText.length > 0)
				grpSearchText = searchText;
		}
		else
		{
			// If the user specified a message group number, then
			// set it and let the user choose a sub-board within
			// the group.
			if (selectedGrp > 0)
			{
				// Set the default sub-board #: The current sub-board, or if the
				// user chose a different group, then this should be set
				// to the first sub-board.
				var defaultSubBoard = msg_area.sub[this.subBoardCode].index + 1;
				if (selectedGrp-1 != msg_area.sub[this.subBoardCode].grp_index)
					defaultSubBoard = 1;
				// Older:
				/*
				var defaultSubBoard = bbs.cursub + 1;
				if (selectedGrp-1 != bbs.curgrp)
					defaultSubBoard = 1;
				*/

				var subSearchText = "";
				var continueChoosingSubBoard = true;
				while (continueChoosingSubBoard)
				{
					console.clear("\x01n");
					this.DisplayAreaChgHdr();
					this.ListSubBoardsInMsgGroup(selectedGrp-1, defaultSubBoard-1, null, subSearchText);
					console.crlf();
					console.print("\x01n\x01b\x01h" + TALL_UPPER_MID_BLOCK + " \x01n\x01cWhich, \x01h/\x01n\x01c or \x01hCTRL-F\x01n\x01c, \x01hQ\x01n\x01cuit, or [\x01h" +
					              defaultSubBoard + "\x01n\x01c]: \x01h");
					// Accept Q (quit), / or CTRL_F (Search) or a sub-board number
					selectedSubBoard = console.getkeys("Q/" + CTRL_F, msg_area.grp_list[selectedGrp - 1].sub_list.length);

					// If the user just pressed enter (selectedSubBoard would be blank),
					// default the selected directory.
					if (selectedSubBoard.toString() == "")
						selectedSubBoard = defaultSubBoard;

					// If the user chose to quit out of the sub-board list, then
					// return to the message group list.
					if (selectedSubBoard.toString() == "Q")
						continueChoosingSubBoard = false;
					// / or CTRL-F: Search
					else if ((selectedSubBoard.toString() == "/") || (selectedSubBoard.toString() == CTRL_F))
					{
						console.crlf();
						var searchPromptText = "\x01n\x01c\x01hSearch\x01g: \x01n";
						console.print(searchPromptText);
						var searchText = console.getstr("", console.screen_columns-strip_ctrl(searchPromptText).length-1, K_UPPER|K_NOCRLF|K_GETSTR|K_NOSPIN|K_LINE);
						if (searchText.length > 0)
							subSearchText = searchText;
					}
					// If the user chose a message sub-board, then validate the user's
					// sub-board choice; if that succeeds, then change the user's
					// sub-board to that and quit out of the chooser loops.
					else if (selectedSubBoard > 0)
					{
						// Validate the sub-board choice.  If a search is specified, the
						// validator function will search for messages in the selected
						// sub-board and will return true if there are messages to read
						// there or false if not.  If there is no search specified,
						// the validator function will return a 'true' value.
						var selectedGrpIdx = selectedGrp - 1;
						var selectedSubIdx = selectedSubBoard - 1;
						var msgAreaValidRetval = this.ValidateMsgAreaChoice(selectedGrpIdx, selectedSubIdx);
						if (msgAreaValidRetval.msgAreaGood)
						{
							bbs.curgrp = selectedGrpIdx;
							bbs.cursub = selectedSubIdx;
							continueChoosingSubBoard = false;
							continueChoosingMsgGroup = false;
						}
						else
						{
							// Output the error returned by the validator function
							console.print("\x01n\x01h\x01y" + msgAreaValidRetval.errorMsg);
							mswait(ERROR_PAUSE_WAIT_MS);
							// Set our loop variables to continue allowing the user to
							// choose a message sub-board
							continueChoosingSubBoard = true;
							continueChoosingMsgGroup = true;
						}
					}
				}
			}
		}
	}

	// If the user chose a different message group & sub-board, then reset the
	// lister index & cursor variables, as well as this.subBoardCode, etc.
	//msg_area.sub[this.subBoardCode].grp_index
	if ((bbs.curgrp != oldGrp) || (bbs.cursub != oldSub))
	{
		this.tradListTopMsgIdx = -1;
		this.lightbarListTopMsgIdx = -1;
		this.lightbarListSelectedMsgIdx = -1;
		this.lightbarListCurPos = null;
		this.setSubBoardCode(msg_area.grp_list[bbs.curgrp].sub_list[bbs.cursub].code);
	}
}

// For the DigDistMsgReader class: Lists all message groups (for the traditional
// user interface).
//
// Parameters:
//  pSearchText: Optional - Search text for the message groups
function DigDistMsgReader_ListMsgGrps_Traditional(pSearchText)
{
	// Print the header
	this.WriteGrpListHdrLine1();
	console.print("\x01n");

	var searchText = (typeof(pSearchText) == "string" ? pSearchText.toUpperCase() : "");

	// List the message groups
	var printIt = true;
	for (var i = 0; i < msg_area.grp_list.length; ++i)
	{
		if (searchText.length > 0)
			printIt = ((msg_area.grp_list[i].name.toUpperCase().indexOf(searchText) >= 0) || (msg_area.grp_list[i].description.toUpperCase().indexOf(searchText) >= 0));
		else
			printIt = true;

		if (printIt)
		{
			console.crlf();
			this.WriteMsgGroupLine(i, false);
		}
	}
}

// For the DigDistMsgReader class: Lists the sub-boards in a message group,
// for the traditional user interface.
//
// Parameters:
//  pGrpIndex: The index of the message group (0-based)
//  pMarkIndex: An index of a message group to highlight.  This
//                   is optional; if left off, this will default to
//                   the current sub-board.
//  pSortType: Optional - A string describing how to sort the list (if desired):
//             "none": Default behavior - Sort by sub-board #
//             "dateAsc": Sort by date, ascending
//             "dateDesc": Sort by date, descending
//             "description": Sort by description
//  pSearchText: Optional - Search text for the message sub-boards
function DigDistMsgReader_ListSubBoardsInMsgGroup_Traditional(pGrpIndex, pMarkIndex, pSortType, pSearchText)
{
	// Default to the current message group & sub-board if pGrpIndex
	// and pMarkIndex aren't specified.
	var grpIndex = bbs.curgrp;
	if ((pGrpIndex != null) && (typeof(pGrpIndex) == "number"))
		grpIndex = pGrpIndex;
	var highlightIndex = bbs.cursub;
	if ((pMarkIndex != null) && (typeof(pMarkIndex) == "number"))
		highlightIndex = pMarkIndex;

	// Make sure grpIndex and highlightIndex are valid (they might not be for
	// brand-new users).
	if ((grpIndex == null) || (typeof(grpIndex) == "undefined"))
		grpIndex = 0;
	if ((highlightIndex == null) || (typeof(highlightIndex) == "undefined"))
		highlightIndex = 0;

	// Ensure that the sub-board printf information is created for
	// this message group.
	this.BuildSubBoardPrintfInfoForGrp(grpIndex);

	// Print the headers
	this.WriteSubBrdListHdrLine(grpIndex);
	console.crlf();
	printf(this.subBoardListHdrPrintfStr, "Sub #", "Name", "# Posts", "Latest date & time");
	console.print("\x01n");

	// List each sub-board in the message group.
	var searchText = (typeof(pSearchText) == "string" ? pSearchText.toUpperCase() : "");
	var subBoardArray = null;       // For sorting, if desired
	var newestDate = {}; // For storing the date of the newest post in a sub-board
	var msgBase = null;    // For opening the sub-boards with a MsgBase object
	var msgHeader = null;  // For getting the date & time of the newest post in a sub-board
	var subBoardNum = 0;   // 0-based sub-board number (because the array index is the number as a str)
	var includeSubBoard = true;
	// If a sort type is specified, then add the sub-board information to
	// subBoardArray so that it can be sorted.
	if ((typeof(pSortType) == "string") && (pSortType != "") && (pSortType != "none"))
	{
		subBoardArray = [];
		var subBoardInfo = null;
		for (var arrSubBoardNum in msg_area.grp_list[grpIndex].sub_list)
		{
			if (searchText.length > 0)
				includeSubBoard = ((msg_area.grp_list[grpIndex].sub_list[arrSubBoardNum].name.toUpperCase().indexOf(searchText) >= 0) || (msg_area.grp_list[grpIndex].sub_list[arrSubBoardNum].description.toUpperCase().indexOf(searchText) >= 0));
			else
				includeSubBoard = true;
			if (!includeSubBoard)
				continue;

			// Open the current sub-board with the msgBase object.
			msgBase = new MsgBase(msg_area.grp_list[grpIndex].sub_list[arrSubBoardNum].code);
			if (msgBase.open())
			{
				subBoardInfo = new MsgSubBoardInfo();
				subBoardInfo.subBoardNum = +(arrSubBoardNum);
				subBoardInfo.description = msg_area.grp_list[grpIndex].sub_list[arrSubBoardNum].description;
				// Note: numReadableMsgs() is slow because it goes through and
				// checks for deleted messages, etc., so just use msgBase.total_msgs
				//subBoardInfo.numPosts = numReadableMsgs(msgBase, msg_area.grp_list[grpIndex].sub_list[arrSubBoardNum].code);
				subBoardInfo.numPosts = msgBase.total_msgs;

				// Get the date & time when the last message was imported.
				if (subBoardInfo.numPosts > 0)
				{
					var msgIdx = msgBase.total_msgs-1;
					msgHeader = msgBase.get_msg_header(true, msgIdx, false);
					while (!isReadableMsgHdr(msgHeader, msg_area.grp_list[grpIndex].sub_list[arrSubBoardNum].code) && (msgIdx >= 0))
					  msgHeader = msgBase.get_msg_header(true, --msgIdx, true);
					if (this.msgAreaList_lastImportedMsg_showImportTime)
						subBoardInfo.newestPostDate = msgHeader.when_imported_time
					else
					{
						//subBoardInfo.newestPostDate = msgHeader.when_written_time;
						var msgWrittenLocalTime = msgWrittenTimeToLocalBBSTime(msgHeader);
						if (msgWrittenLocalTime != -1)
							subBoardInfo.newestPostDate = msgWrittenTimeToLocalBBSTime(msgHeader);
						else
							subBoardInfo.newestPostDate = msgHeader.when_written_time;
					}
				}
			}
			msgBase.close();
			subBoardArray.push(subBoardInfo);
		}

		// Possibly sort the sub-board list.
		if (pSortType == "dateAsc")
		{
			subBoardArray.sort(function(pA, pB)
			{
				// Return -1, 0, or 1, depending on whether pA's date comes
				// before, is equal to, or comes after pB's date.
				var returnValue = 0;
				if (pA.newestPostDate < pB.newestPostDate)
					returnValue = -1;
				else if (pA.newestPostDate > pB.newestPostDate)
					returnValue = 1;
				return returnValue;
			});
		}
		else if (pSortType == "dateDesc")
		{
			subBoardArray.sort(function(pA, pB)
			{
				// Return -1, 0, or 1, depending on whether pA's date comes
				// after, is equal to, or comes before pB's date.
				var returnValue = 0;
				if (pA.newestPostDate > pB.newestPostDate)
					returnValue = -1;
				else if (pA.newestPostDate < pB.newestPostDate)
					returnValue = 1;
				return returnValue;
			});
		}
		else if (pSortType == "description")
		{
			// Binary safe string comparison  
			// 
			// version: 909.322
			// discuss at: http://phpjs.org/functions/strcmp    // +   original by: Waldo Malqui Silva
			// +      input by: Steve Hilder
			// +   improved by: Kevin van Zonneveld (http://kevin.vanzonneveld.net)
			// +    revised by: gorthaur
			// *     example 1: strcmp( 'waldo', 'owald' );    // *     returns 1: 1
			// *     example 2: strcmp( 'owald', 'waldo' );
			// *     returns 2: -1
			subBoardArray.sort(function(pA, pB)
			{
				return ((pA.description == pB.description) ? 0 : ((pA.description > pB.description) ? 1 : -1));
			});
		}

		// Display the sub-board list.
		for (var i = 0; i < subBoardArray.length; ++i)
		{
			console.crlf();
			console.print((subBoardArray[i].subBoardNum == highlightIndex) ? "\x01n" +
			              this.colors.areaChooserMsgAreaMarkColor + "*" : " ");
			printf(this.subBoardListPrintfInfo[grpIndex].printfStr, +(subBoardArray[i].subBoardNum+1),
			       subBoardArray[i].description.substr(0, this.subBoardNameLen),
			       subBoardArray[i].numPosts, strftime("%Y-%m-%d", subBoardArray[i].newestPostDate),
			       strftime("%H:%M:%S", subBoardArray[i].newestPostDate));
		}
	}
	// If no sort type is specified, then output the sub-board information in
	// order of sub-board number.
	else
	{
		for (var arrSubBoardNum in msg_area.grp_list[grpIndex].sub_list)
		{
			if (searchText.length > 0)
				includeSubBoard = ((msg_area.grp_list[grpIndex].sub_list[arrSubBoardNum].name.toUpperCase().indexOf(searchText) >= 0) || (msg_area.grp_list[grpIndex].sub_list[arrSubBoardNum].description.toUpperCase().indexOf(searchText) >= 0));
			else
				includeSubBoard = true;
			if (!includeSubBoard)
				continue;

			// Open the current sub-board with the msgBase object.
			msgBase = new MsgBase(msg_area.grp_list[grpIndex].sub_list[arrSubBoardNum].code);
			if (msgBase.open())
			{
				// Get the date & time when the last message was imported.
				// Note: numReadableMsgs() is slow because it goes through and
				// checks for deleted messages, etc., so just use msgBase.total_msgs
				//var numMsgs = numReadableMsgs(msgBase, msg_area.grp_list[grpIndex].sub_list[arrSubBoardNum].code);
				var numMsgs = msgBase.total_msgs;
				if (numMsgs > 0)
				{
					var msgIdx = msgBase.total_msgs-1;
					msgHeader = msgBase.get_msg_header(true, msgIdx, false);
					while (!isReadableMsgHdr(msgHeader, msg_area.grp_list[grpIndex].sub_list[arrSubBoardNum].code) && (msgIdx >= 0))
					  msgHeader = msgBase.get_msg_header(true, --msgIdx, true);
					// Construct the date & time strings of the latest post
					if (this.msgAreaList_lastImportedMsg_showImportTime)
					{
						newestDate.date = strftime("%Y-%m-%d", msgHeader.when_imported_time);
						newestDate.time = strftime("%H:%M:%S", msgHeader.when_imported_time);
					}
					else
					{
						//newestDate.date = strftime("%Y-%m-%d", msgHeader.when_written_time);
						//newestDate.time = strftime("%H:%M:%S", msgHeader.when_written_time);
						var msgWrittenLocalTime = msgWrittenTimeToLocalBBSTime(msgHeader);
						if (msgWrittenLocalTime != -1)
						{
							newestDate.date = strftime("%Y-%m-%d", msgWrittenLocalTime);
							newestDate.time = strftime("%H:%M:%S", msgWrittenLocalTime);
						}
						else
						{
							newestDate.date = strftime("%Y-%m-%d", msgHeader.when_written_time);
							newestDate.time = strftime("%H:%M:%S", msgHeader.when_written_time);
						}
					}
				}
				else
					newestDate.date = newestDate.time = "";

				// Print the sub-board information
				subBoardNum = +(arrSubBoardNum);
				console.crlf();
				console.print((subBoardNum == highlightIndex) ? "\x01n" +
				              this.colors.areaChooserMsgAreaMarkColor + "*" : " ");
				printf(this.subBoardListPrintfInfo[grpIndex].printfStr, +(subBoardNum+1),
				       msg_area.grp_list[grpIndex].sub_list[arrSubBoardNum].description.substr(0, this.subBoardListPrintfInfo[grpIndex].nameLen),
				       numMsgs, newestDate.date, newestDate.time);

				msgBase.close();
			}
		}
	}
}

//////////////////////////////////////////////
// Message group list stuff (lightbar mode) //
//////////////////////////////////////////////

// For the DigDistMsgReader class - Writes a message group information line.
//
// Parameters:
//  pGrpIndex: The index of the message group to write (assumed to be valid)
//  pHighlight: Boolean - Whether or not to write the line highlighted.
function DigDistMsgReader_writeMsgGroupLine(pGrpIndex, pHighlight)
{
	// TODO: If pHighlight is true, that causes the screen to be cleared
	// and the line is written on the first row of the console.
	console.print("\x01n");
	// Write the highlight background color if pHighlight is true.
	if (pHighlight)
	console.print(this.colors.areaChooserMsgAreaBkgHighlightColor);

	// Write the message group information line
	console.print(((typeof(bbs.curgrp) == "number") && (pGrpIndex == msg_area.sub[this.subBoardCode].grp_index)) ? this.colors.areaChooserMsgAreaMarkColor + "*" : " ");
	printf((pHighlight ? this.msgGrpListHilightPrintfStr : this.msgGrpListPrintfStr),
	       +(pGrpIndex+1),
	       msg_area.grp_list[pGrpIndex].description.substr(0, this.msgGrpDescLen),
	       msg_area.grp_list[pGrpIndex].sub_list.length);
	console.cleartoeol("\x01n");
}

//////////////////////////////////////////////////
// Message sub-board list stuff (lightbar mode) //
//////////////////////////////////////////////////

// Updates the page number text in the group list header line on the screen.
//
// Parameters:
//  pPageNum: The page number
//  pNumPages: The total number of pages
//  pGroup: Boolean - Whether or not this is for the group header.  If so,
//          then this will go to the right location for the group page text
//          and use this.colors.areaChooserMsgAreaHeaderColor for the text.
//          Otherwise, this will go to the right place for the sub-board page
//          text and use the sub-board header color.
//  pRestoreCurPos: Optional - Boolean - If true, then move the cursor back
//                  to the position where it was before this function was called
function DigDistMsgReader_updateMsgAreaPageNumInHeader(pPageNum, pNumPages, pGroup, pRestoreCurPos)
{
	var originalCurPos = null;
	if (pRestoreCurPos)
		originalCurPos = console.getxy();

	if (pGroup)
	{
		console.gotoxy(29, 1+this.areaChangeHdrLines.length);
		console.print("\x01n" + this.colors.areaChooserMsgAreaHeaderColor + pPageNum + " of " +
		              pNumPages + ")   ");
	}
	else
	{
		console.gotoxy(51, 1+this.areaChangeHdrLines.length);
		console.print("\x01n" + this.colors.areaChooserSubBoardHeaderColor + pPageNum + " of " +
		              pNumPages + ")   ");
	}

	if (pRestoreCurPos)
		console.gotoxy(originalCurPos);
}

// For the DigDistMsgReader class: Returns a formatted string with sub-board
// information for the message area chooser functionality.
//
// Parameters:
//  pGrpIndex: The index of the message group (assumed to be valid)
//  pSubIndex: The index of the sub-board within the message group (assumed to be valid)
//  pHighlight: Boolean - Whether or not to write the line highlighted.
//
// Return value: A string with the sub-board information
function DigDistMsgReader_GetMsgSubBrdLine(pGrpIndex, pSubIndex, pHighlight)
{
	// Determine if pGrpIndex and pSubIndex specify the user's
	// currently-selected group and sub-board.
	var currentSub = false;
	if ((typeof(bbs.curgrp) == "number") && (typeof(bbs.cursub) == "number"))
		currentSub = ((pGrpIndex == msg_area.sub[this.subBoardCode].grp_index) && (pSubIndex == msg_area.sub[this.subBoardCode].index));

	var subBoardStr = "";
	// Use the highlight background color if pHighlight is true.
	if (pHighlight)
		subBoardStr += this.colors.areaChooserMsgAreaBkgHighlightColor;
	// Open the current sub-board with the msgBase object (so that we can get
	// the date & time of the last imporeted message).
	var msgBase = new MsgBase(msg_area.grp_list[pGrpIndex].sub_list[pSubIndex].code);
	if (msgBase.open())
	{
		var newestDate = {}; // For storing the date of the newest post
		// Get the date & time when the last message was imported.
		//numReadableMsgs(pMsgbase, pSubBoardCode)
		//var numMsgs = numReadableMsgs(msgBase, msg_area.grp_list[pGrpIndex].sub_list[pSubIndex].code);
		var numMsgs = msgBase.total_msgs;
		if (numMsgs > 0)
		{
			// Get the header of the last message in the sub-board
			var msgHeader = null;
			var msgOffset = msgBase.total_msgs - 1;
			while (!isReadableMsgHdr(msgHeader, msg_area.grp_list[pGrpIndex].sub_list[pSubIndex].code) && (msgOffset >= 0))
				msgHeader = msgBase.get_msg_header(true, --msgOffset, true);
			// Construct the date & time strings of the latest post
			if (this.msgAreaList_lastImportedMsg_showImportTime)
			{
				newestDate.date = strftime("%Y-%m-%d", msgHeader.when_imported_time);
				newestDate.time = strftime("%H:%M:%S", msgHeader.when_imported_time);
			}
			else
			{
				//newestDate.date = strftime("%Y-%m-%d", msgHeader.when_written_time);
				//newestDate.time = strftime("%H:%M:%S", msgHeader.when_written_time);
				var msgWrittenLocalTime = msgWrittenTimeToLocalBBSTime(msgHeader);
				if (msgWrittenLocalTime != -1)
				{
					newestDate.date = strftime("%Y-%m-%d", msgWrittenLocalTime);
					newestDate.time = strftime("%H:%M:%S", msgWrittenLocalTime);
				}
				else
				{
					newestDate.date = strftime("%Y-%m-%d", msgHeader.when_written_time);
					newestDate.time = strftime("%H:%M:%S", msgHeader.when_written_time);
				}
			}
		}
		else
			newestDate.date = newestDate.time = "";

		// Print the sub-board information line.
		subBoardStr += (currentSub ? this.colors.areaChooserMsgAreaMarkColor + "*" : " ");
		subBoardStr += format((pHighlight ? this.subBoardListPrintfInfo[pGrpIndex].highlightPrintfStr : this.subBoardListPrintfInfo[pGrpIndex].printfStr),
		                      +(pSubIndex+1),
		                      msg_area.grp_list[pGrpIndex].sub_list[pSubIndex].description.substr(0, this.subBoardListPrintfInfo[pGrpIndex].nameLen),
		                      numMsgs, newestDate.date, newestDate.time);
		msgBase.close();
	}
	return subBoardStr;
}

///////////////////////////////////////////////
// Other functions for the msg. area chooser //
///////////////////////////////////////////////

// For the DigDistMsgReader class: Shows the help screen
//
// Parameters:
//  pLightbar: Boolean - Whether or not to show lightbar help.  If
//             false, then this function will show regular help.
//  pClearScreen: Boolean - Whether or not to clear the screen first
function DigDistMsgReader_showChooseMsgAreaHelpScreen(pLightbar, pClearScreen)
{
	if (pClearScreen && console.term_supports(USER_ANSI))
		console.clear("\x01n");
	else
		console.print("\x01n");
	DisplayProgramInfo();
	console.crlf();
	console.print("\x01n\x01c\x01hMessage area (sub-board) chooser");
	console.crlf();
	console.print("\x01k" + charStr(HORIZONTAL_SINGLE, 32) + "\x01n");
	console.crlf();
	console.print("\x01cFirst, a listing of message groups is displayed.  One can be chosen by typing");
	console.crlf();
	console.print("its number.  Then, a listing of sub-boards within that message group will be");
	console.crlf();
	console.print("shown, and one can be chosen by typing its number.");
	console.crlf();

	console.crlf();
	console.print("Keyboard commands:");
	console.crlf();
	console.print("\x01k\x01h" + charStr(HORIZONTAL_SINGLE, 18) + "\x01n");
	console.crlf();
	console.print("\x01n\x01c\x01h/\x01n\x01c or \x01hCTRL-F\x01n\x01c: Find group/sub-board");
	console.crlf();
	console.print("\x01n\x01c\x01h?\x01n\x01c: Show this help screen");
	console.crlf();
	console.print("\x01hQ\x01n\x01c: Quit");
	console.crlf();

	if (pLightbar)
	{
		console.crlf();
		console.print("\x01n\x01cThe lightbar interface also allows up & down navigation through the lists:");
		console.crlf();
		console.print("\x01k\x01h" + charStr(HORIZONTAL_SINGLE, 74));
		console.crlf();
		console.print("\x01n\x01c\x01hUp\x01n\x01c/\x01hdown arrow\x01n\x01c: Move the cursor up/down one line");
		console.crlf();
		console.print("\x01hPageUp\x01n\x01c/\x01hPageDown\x01n\x01c: Move up/down a page");
		console.crlf();
		console.print("\x01hENTER\x01n\x01c: Select the current group/sub-board");
		console.crlf();
		console.print("\x01hHOME\x01n\x01c: Go to the first item on the screen");
		console.crlf();
		console.print("\x01hEND\x01n\x01c: Go to the last item on the screen");
		console.crlf();
		console.print("\x01hF\x01n\x01c: Go to the first page");
		console.crlf();
		console.print("\x01hL\x01n\x01c: Go to the last page");
		console.crlf();
		console.print("\x01hN\x01n\x01c: Next search result");
		console.crlf();
	}
}

// Builds sub-board printf format information for a message group.
// The widths of the description & # messages columns are calculated
// based on the greatest number of messages in a sub-board for the
// message group.
//
// Parameters:
//  pGrpIndex: The index of the message group
function DigDistMsgReader_BuildSubBoardPrintfInfoForGrp(pGrpIndex)
{
   // If the array of sub-board printf strings doesn't contain the printf
   // strings for this message group, then figure out the largest number
   // of messages in the message group and add the printf strings.
   if (typeof(this.subBoardListPrintfInfo[pGrpIndex]) == "undefined")
   {
      var greatestNumMsgs = getGreatestNumMsgs(pGrpIndex);

      this.subBoardListPrintfInfo[pGrpIndex] = {};
      this.subBoardListPrintfInfo[pGrpIndex].numMsgsLen = greatestNumMsgs.toString().length;
      // Sub-board name length: With a # items length of 4, this should be
      // 47 for an 80-column display.
      this.subBoardListPrintfInfo[pGrpIndex].nameLen = console.screen_columns -
                                   this.areaNumLen -
                                   this.subBoardListPrintfInfo[pGrpIndex].numMsgsLen -
                                   this.dateLen - this.timeLen - 7;
      // Create the printf strings
      this.subBoardListPrintfInfo[pGrpIndex].printfStr =
               " " + this.colors.areaChooserMsgAreaNumColor
               + "%" + this.areaNumLen + "d "
               + this.colors.areaChooserMsgAreaDescColor + "%-"
               + this.subBoardListPrintfInfo[pGrpIndex].nameLen + "s "
               + this.colors.areaChooserMsgAreaNumItemsColor + "%"
               + this.subBoardListPrintfInfo[pGrpIndex].numMsgsLen + "d "
               + this.colors.areaChooserMsgAreaLatestDateColor + "%" + this.dateLen + "s "
               + this.colors.areaChooserMsgAreaLatestTimeColor + "%" + this.timeLen + "s";
      this.subBoardListPrintfInfo[pGrpIndex].highlightPrintfStr =
                              "\x01n" + this.colors.areaChooserMsgAreaBkgHighlightColor + " "
                              + "\x01n" + this.colors.areaChooserMsgAreaBkgHighlightColor
                              + this.colors.areaChooserMsgAreaNumHighlightColor
                              + "%" + this.areaNumLen + "d \x01n"
                              + this.colors.areaChooserMsgAreaBkgHighlightColor
                              + this.colors.areaChooserMsgAreaDescHighlightColor + "%-"
                              + this.subBoardListPrintfInfo[pGrpIndex].nameLen + "s \x01n"
                              + this.colors.areaChooserMsgAreaBkgHighlightColor
                              + this.colors.areaChooserMsgAreaNumItemsHighlightColor + "%"
                              + this.subBoardListPrintfInfo[pGrpIndex].numMsgsLen + "d \x01n"
                              + this.colors.areaChooserMsgAreaBkgHighlightColor
                              + this.colors.areaChooserMsgAreaDateHighlightColor + "%" + this.dateLen + "s \x01n"
                              + this.colors.areaChooserMsgAreaBkgHighlightColor
                              + this.colors.areaChooserMsgAreaTimeHighlightColor + "%" + this.timeLen + "s\x01n";
   }
}

// Returns an array of strings containing extended message header information,
// such as the kludge lines (for FidoNet-style networks), etc.
// For each kludge line, there will be a label string for the info line, the
// info line itself (wrapped to fit the message area width), then a blank
// line (except for the last kludge line).  The info lines that this method
// retrieves will only be retrieved if they exist in the given message header.
//
// Parameters:
//  pMsgHdr: A message header
//  pKludgeOnly: Boolean - Whether or not to only get the kludge lines.  If false,
//               then all header fields will be retrieved.
//
// Return value: An array of strings containing the extended message header information
function DigDistMsgReader_GetExtdMsgHdrInfo(pMsgHdr, pKludgeOnly)
{
	// If pMsgHdr is not valid, then just return an empty array.
	if (typeof(pMsgHdr) != "object")
		return [];

	// Get the message header with fields expanded so we can get the most info possible.
	var msgHdr = this.GetMsgHdrByAbsoluteNum(pMsgHdr.number, true, true);
	if (msgHdr == null)
		return [];
	// The message header retrieved that way might not have vote information,
	// so copy any additional header information from this.hdrsForCurrentSubBoard
	// if there's a header there for this message.
	if (this.hdrsForCurrentSubBoardByMsgNum.hasOwnProperty(pMsgHdr.number))
	{
		var tmpHdrIdx = this.hdrsForCurrentSubBoardByMsgNum[pMsgHdr.number];
		if (this.hdrsForCurrentSubBoard.hasOwnProperty(tmpHdrIdx))
		{
			for (var hdrProp in this.hdrsForCurrentSubBoard[tmpHdrIdx])
			{
				if (!msgHdr.hasOwnProperty(hdrProp))
					msgHdr[hdrProp] = this.hdrsForCurrentSubBoard[tmpHdrIdx][hdrProp];
			}
		}
	}

	var msgHdrInfoLines = [];

	var hdrInfoLineFields = [];
	var kludgeOnly = (typeof(pKludgeOnly) == "boolean" ? pKludgeOnly : false);
	if (kludgeOnly)
	{
		hdrInfoLineFields.push({ field: "ftn_msgid", label: "MSG ID:" });
		hdrInfoLineFields.push({ field: "X-FTN-MSGID", label: "MSG ID:" });
		hdrInfoLineFields.push({ field: "ftn_reply", label: "Reply ID:" });
		hdrInfoLineFields.push({ field: "X-FTN-REPLY", label: "Reply ID:" });
		hdrInfoLineFields.push({ field: "ftn_area", label: "Area tag:" });
		hdrInfoLineFields.push({ field: "X-FTN-AREA", label: "Area tag:" });
		hdrInfoLineFields.push({ field: "ftn_flags", label: "Flags:" });
		hdrInfoLineFields.push({ field: "ftn_pid", label: "Program ID:" });
		hdrInfoLineFields.push({ field: "ftn_tid", label: "Tosser ID:" });
		hdrInfoLineFields.push({ field: "X-FTN-TID", label: "Tosser ID:" });
		hdrInfoLineFields.push({ field: "X-FTN-Kludge", label: "Kludge:" });
		hdrInfoLineFields.push({ field: "X-FTN-SEEN-BY", label: "Seen-By:" });
		hdrInfoLineFields.push({ field: "X-FTN-PATH", label: "Path:" });
		hdrInfoLineFields.push({ field: "when_written_time", label: "When written time:" });
		hdrInfoLineFields.push({ field: "when_imported_time", label: "When imported time:" });
	}

	// This function returns the number of non-blank lines in a header info array.
	//
	// Return value: An object with the following properties:
	//               numNonBlankLines: The number of non-blank lines in the array
	//               firstNonBlankLineIdx: The index of the first non-blank line
	//               lastNonBlankLineIdx: The index of the last non-blank line
	function findHdrFieldDataArrayNonBlankLines(pHdrArray)
	{
		var retObj = {
			numNonBlankLines: 0,
			firstNonBlankLineIdx: -1,
			lastNonBlankLineIdx: -1
		};

		for (var lineIdx = 0; lineIdx < pHdrArray.length; ++lineIdx)
		{
			if (pHdrArray[lineIdx].length > 0)
			{
				++retObj.numNonBlankLines;
				if (retObj.firstNonBlankLineIdx == -1)
					retObj.firstNonBlankLineIdx = lineIdx;
				retObj.lastNonBlankLineIdx = lineIdx;
			}
		}

		return retObj;
	}

	// Counts the number of elements in a header field_list array with the
	// same type, starting at a given index.
	//
	// Parameters:
	//  pFieldList: The field_list array in a message header
	//  pStartIdx: The index of the starting element to start counting at
	//
	// Return value: The number of elements with the same type as the start index element
	function fieldListCountSameTypes(pFieldList, pStartIdx)
	{
		if (typeof(pFieldList) == "undefined")
			return 0;
		if (typeof(pStartIdx) != "number")
			return 0;
		if ((pStartIdx < 0) || (pStartIdx >= pFieldList.length))
			return 0;

		var itemCount = 1;
		for (var idx = pStartIdx+1; idx < pFieldList.length; ++idx)
		{
			if (pFieldList[idx].type == pFieldList[pStartIdx].type)
				++itemCount;
			else
				break;
		}
		return itemCount;
	}

	// Get the header fields
	var addTheField = true;
	var customFieldLabel = "";
	var propCounter = 1;
	for (var prop in msgHdr)
	{
		addTheField = true;
		customFieldLabel = "";

		if (prop == "field_list")
		{
			var hdrFieldLabel = "";
			var lastHdrFieldLabel = null;
			var addBlankLineAfterIdx = -1;
			for (var fieldI = 0; fieldI < msgHdr.field_list.length; ++fieldI)
			{
				// TODO: Some field types can be in the array multiple times but only
				// the last is valid.  For those, only get the last one:
				//  32 (Reply To)
				//  33 (Reply To agent)
				//  34 (Reply To net type)
				//  35 (Reply To net address)
				//  36 (Reply To extended)
				//  37 (Reply To position)
				//  38 (Reply To Organization)
				hdrFieldLabel = "\x01n" + this.colors.hdrLineLabelColor + msgHdrFieldListTypeToLabel(msgHdr.field_list[fieldI].type) + "\x01n";
				var infoLineWrapped = msgHdr.field_list[fieldI].data;
				var infoLineWrappedArray = lfexpand(infoLineWrapped).split("\r\n");
				var hdrArrayNonBlankLines = findHdrFieldDataArrayNonBlankLines(infoLineWrappedArray);
				if (hdrArrayNonBlankLines.numNonBlankLines > 0)
				{
					if (hdrArrayNonBlankLines.numNonBlankLines == 1)
					{
						var addExtraBlankLineAtEnd = false;
						var hdrItem = "\x01n" + this.colors.hdrLineValueColor + infoLineWrappedArray[hdrArrayNonBlankLines.firstNonBlankLineIdx] + "\x01n";
						// If the header field label is different, then add it to the
						// header info lines
						if ((lastHdrFieldLabel == null) || (hdrFieldLabel != lastHdrFieldLabel))
						{
							var numFieldItemsWithSameType = fieldListCountSameTypes(msgHdr.field_list, fieldI);
							if (numFieldItemsWithSameType > 1)
							{
								msgHdrInfoLines.push("");
								msgHdrInfoLines.push(hdrFieldLabel);
								addExtraBlankLineAtEnd = true;
								addBlankLineAfterIdx = fieldI + numFieldItemsWithSameType - 1;
							}
							else
							{
								hdrItem = hdrFieldLabel + " " + hdrItem;
								numFieldItemsWithSameType = -1;
							}
						}
						if (strip_ctrl(hdrItem).length < this.msgAreaWidth)
							msgHdrInfoLines.push(hdrItem);
						else
						{
							// If the header field label is different, then add a blank line
							// to the header info lines
							if ((lastHdrFieldLabel == null) || (hdrFieldLabel != lastHdrFieldLabel))
								msgHdrInfoLines.push("");
							msgHdrInfoLines.push(hdrFieldLabel);
							msgHdrInfoLines.push(infoLineWrappedArray[hdrArrayNonBlankLines.firstNonBlankLineIdx]);
							if ((lastHdrFieldLabel == null) || (hdrFieldLabel != lastHdrFieldLabel))
								msgHdrInfoLines.push("");
						}
					}
					else
					{
						// If the header field label is different, then add it to the
						// header info lines
						if ((lastHdrFieldLabel == null) || (hdrFieldLabel != lastHdrFieldLabel))
						{
							msgHdrInfoLines.push("");
							msgHdrInfoLines.push(hdrFieldLabel + "!");
						}
						var infoLineWrapped = msgHdr.field_list[fieldI].data;
						var infoLineWrappedArray = lfexpand(infoLineWrapped).split("\r\n");
						for (var lineIdx = 0; lineIdx < infoLineWrappedArray.length; ++lineIdx)
						{
							if (infoLineWrappedArray[lineIdx].length > 0)
								msgHdrInfoLines.push(infoLineWrappedArray[lineIdx]);
						}
						// If the header field label is different, then add a blank line to the
						// header info lines
						if ((lastHdrFieldLabel == null) || (hdrFieldLabel != lastHdrFieldLabel))
							msgHdrInfoLines.push("");
					}
					if (addBlankLineAfterIdx == fieldI)
						msgHdrInfoLines.push("");
				}
				lastHdrFieldLabel = hdrFieldLabel;
			}
		}
		else
		{
			// See if we should add this field
			addTheField = true;
			if (hdrInfoLineFields.length > 0)
			{
				addTheField = false;
				for (var infoLineFieldIdx = 0; infoLineFieldIdx < hdrInfoLineFields.length; ++infoLineFieldIdx)
				{
					if (prop == hdrInfoLineFields[infoLineFieldIdx].field)
					{
						addTheField = true;
						customFieldLabel = hdrInfoLineFields[infoLineFieldIdx].label;
						break;
					}
				}
			}
			if (!addTheField)
				continue;

			var propLabel = "";
			if (customFieldLabel.length > 0)
				propLabel = "\x01n" + this.colors.hdrLineLabelColor + customFieldLabel + "\x01n";
			else
			{
				// Remove underscores from the property for the label
				propLabel = prop.replace(/_/g, " ");
				// Apply good-looking capitalization to the property label
				if ((propLabel == "id") || (propLabel == "ftn tid"))
					propLabel = propLabel.toUpperCase();
				else if (propLabel == "ftn area")
					propLabel = "FTN Area";
				else if (propLabel == "ftn pid")
					propLabel = "Program ID";
				else if (propLabel == "thread id")
					propLabel = "Thread ID";
				else if (propLabel == "attr")
					propLabel = "Attributes";
				else if (propLabel == "auxattr")
					propLabel = "Auxiliary attributes";
				else if (propLabel == "netattr")
					propLabel = "Network attributes";
				else
					propLabel = capitalizeFirstChar(propLabel);
				// Add the label color and trailing colon to the label text
				propLabel = "\x01n" + this.colors.hdrLineLabelColor + propLabel + ":\x01n";
			}
			var infoLineWrapped = word_wrap(msgHdr[prop], this.msgAreaWidth);
			var infoLineWrappedArray = lfexpand(infoLineWrapped).split("\r\n");
			var itemValue = "";
			for (var lineIdx = 0; lineIdx < infoLineWrappedArray.length; ++lineIdx)
			{
				if (infoLineWrappedArray[lineIdx].length > 0)
				{
					// Set itemValue to the value that should be displayed
					if (typeof(msgHdr[prop]) === "function") // Such as get_rfc822_header
						continue;
					else if (prop == "when_written_time") //itemValue = system.timestr(msgHdr.when_written_time);
						itemValue = system.timestr(msgHdr.when_written_time) + " " + system.zonestr(msgHdr.when_written_zone);
					else if (prop == "when_imported_time") //itemValue = system.timestr(msgHdr.when_imported_time);
						itemValue = system.timestr(msgHdr.when_imported_time) + " " + system.zonestr(msgHdr.when_imported_zone);
					else if ((prop == "when_imported_zone") || (prop == "when_written_zone"))
						itemValue = system.zonestr(msgHdr[prop]);
					else if (prop == "attr")
						itemValue = makeMainMsgAttrStr(msgHdr[prop], "None");
					else if (prop == "auxattr")
						itemValue = makeAuxMsgAttrStr(msgHdr[prop], "None");
					else if (prop == "netattr")
						itemValue = makeNetMsgAttrStr(msgHdr[prop], "None");
					else
						itemValue = infoLineWrappedArray[lineIdx];
					// Add the value color to the value text
					itemValue = "\x01n" + this.colors.hdrLineValueColor + itemValue + "\x01n";

					var hdrItem = propLabel + " " + itemValue;
					if (strip_ctrl(hdrItem).length < this.msgAreaWidth)
						msgHdrInfoLines.push(hdrItem);
					else
					{
						// If this isn't the first header property, then add an empty
						// line to the info lines array for spacing
						if (propCounter > 1)
							msgHdrInfoLines.push("");
						msgHdrInfoLines.push(propLabel);
						// In case the item value is too long to fit into the
						// message reading area, split it and add all the split lines.
						var itemValueWrapped = word_wrap(itemValue, this.msgAreaWidth);
						var itemValueWrappedArray = lfexpand(itemValueWrapped).split("\r\n");
						for (var lineIdx2 = 0; lineIdx2 < itemValueWrappedArray.length; ++lineIdx2)
						{
							if (itemValueWrappedArray[lineIdx2].length > 0)
								msgHdrInfoLines.push(itemValueWrappedArray[lineIdx2]);
						}

						// If this isn't the first header property, then add an empty
						// line to the info lines array for spacing
						if (propCounter > 1)
							msgHdrInfoLines.push("");
					}
				}
			}
		}

		++propCounter;
	}

	// If some info lines were added, then insert a header line & blank line to
	// the beginning of the array, and remove the last empty line from the array.
	if (msgHdrInfoLines.length > 0)
	{
		if (kludgeOnly)
		{
			msgHdrInfoLines.splice(0, 0, "\x01n\x01c\x01hMessage Information/Kludge Lines\x01n");
			msgHdrInfoLines.splice(1, 0, "\x01n\x01g\x01h--------------------------------\x01n");
		}
		else
		{
			msgHdrInfoLines.splice(0, 0, "\x01n\x01c\x01hMessage Headers\x01n");
			msgHdrInfoLines.splice(1, 0, "\x01n\x01g\x01h---------------\x01n");
		}
		if (msgHdrInfoLines[msgHdrInfoLines.length-1].length == 0)
			msgHdrInfoLines.pop();
	}

	return msgHdrInfoLines;
}

// For the DigDistMsgReader class: Gets & prepares message information for
// the enhanced reader.
//
// Parameters:
//  pMsgHdr: The message header
//  pWordWrap: Boolean - Whether or not to word-wrap the message to fit into the
//             display area.  This is optional and defaults to true.  This should
//             be true for normal use; the only time this should be false is when
//             saving the message to a file.
//  pDetermineAttachments: Boolean - Whether or not to parse the message text to
//                         get attachments from it.  This is optional and defaults
//                         to true.  If false, then the message text will be left
//                         intact with any base64-encoded attachments that may be
//                         in the message text (for multi-part MIME messages).
//  pGetB64Data: Boolean - Whether or not to get the Base64-encoded data for
//               base64-encoded attachments (i.e., in multi-part MIME emails).
//               This is optional and defaults to true.  This is only used when
//               pDetermineAttachments is true.
//  pMsgBody: Optional - A string containing the message body.  If this is not included
//            or is not a string, then this method will retrieve the message body.
//  pMsgHasANSICodes: Optional boolean - If the caller already knows whether the
//                    message text has ANSI codes, the caller can pass this parameter.
//
// Return value: An object with the following properties:
//               msgText: The unaltered message text
//               messageLines: An array containing the message lines, wrapped to
//                             the message area width
//               topMsgLineIdxForLastPage: The top message line index for the last page
//               msgFractionShown: The fraction of the message shown
//               numSolidScrollBlocks: The number of solid scrollbar blocks
//               numNonSolidScrollBlocks: The number of non-solid scrollbar blocks
//               solidBlockStartRow: The starting row on the screen for the scrollbar blocks
//               hasAttachments: Boolean - Whether or not the message has attachments
//               attachments: An array of the attached filenames (as strings)
//               errorMsg: An error message, if something bad happened
function DigDistMsgReader_GetMsgInfoForEnhancedReader(pMsgHdr, pWordWrap, pDetermineAttachments,
                                                      pGetB64Data, pMsgBody)
{
	var retObj = {
		msgText: "",
		messageLines: [],
		topMsgLineIdxForLastPage: 0,
		msgFractionShown: 0.0,
		numSolidScrollBlocks: 0,
		numNonSolidScrollBlocks: 0,
		solidBlockStartRow: 0,
		hasAttachments: false,
		attachments: [],
		errorMsg: ""
	};

	var determineAttachments = (typeof(pDetermineAttachments) == "boolean" ? pDetermineAttachments : true);
	var getB64Data = (typeof(pGetB64Data) == "boolean" ? pGetB64Data : true);
	var msgBody = "";
	if (typeof(pMsgBody) == "string")
		msgBody = pMsgBody;
	else
	{
		var msgbase = new MsgBase(this.subBoardCode);
		if (msgbase.open())
		{
			msgBody = msgbase.get_msg_body(false, pMsgHdr.number, false, false, true, true);
			msgbase.close();
		}
		else
		{
			retObj.errorMsg = "Unable to open the sub-board";
			return retObj;
		}
	}
	retObj.msgText = word_wrap(msgBody, console.screen_columns - 1, true);

	var msgTextAltered = retObj.msgText; // Will alter the message text, but not yet
	// Only interpret @-codes if the user is reading personal email and the sender is a sysop.  There
	// are many @-codes that do some action such as move the cursor, execute a
	// script, etc., and I don't want users on message networks to do anything
	// malicious to users on other BBSes.
	if (this.readingPersonalEmail && msgSenderIsASysop(pMsgHdr))
		msgTextAltered = replaceAtCodesInStr(msgTextAltered); // Or this.ParseMsgAtCodes(msgTextAltered, pMsgHdr) to replace only some @ codes
	msgTextAltered = msgTextAltered.replace(/\t/g, this.tabReplacementText);
	// Convert other BBS color codes to Synchronet attribute codes if the settings
	// to do so are enabled.
	msgTextAltered = convertAttrsToSyncPerSysCfg(msgTextAltered, false);
	// If configured to convert Y-style MCI attribute codes to Synchronet attribute codes, then do so
	if (this.convertYStyleMCIAttrsToSync)
		msgTextAltered = YStyleMCIAttrsToSyncAttrs(msgTextAltered);

	// If this is a message with a "By: <name> to <name>" and a date, then
	// sometimes such a message might have enter characters (ASCII 13), which
	// can mess up the display of the message, so remove enter characters
	// from the beginning of the message.
	var msgTextWithoutAttrs = strip_ctrl(msgTextAltered);
	var fromToSearchStr = "By: " + pMsgHdr.from + " to " + pMsgHdr.to;
	var toFromSearchStr = "By: " + pMsgHdr.to + " to " + pMsgHdr.from;
	var fromToStrIdx = msgTextWithoutAttrs.indexOf(fromToSearchStr);
	var toFromStrIdx = msgTextWithoutAttrs.indexOf(toFromSearchStr);
	var strIdx = -1;
	if (fromToStrIdx > -1)
		strIdx = fromToStrIdx;
	else if (toFromStrIdx > -1)
		strIdx = toFromStrIdx;
	if (strIdx > -1)
	{
		// " on Mon Feb 13 2017 01:00 pm " // 29 characters long
		strIdx += toFromSearchStr.length + 29 + 37; // 37: Extra room for Synchronet attribute codes
		// Remove enter characters from the beginning of the message
		var tmpStr = msgTextAltered.substring(0, strIdx).replace(ascii(13), "");
		msgTextAltered = tmpStr + msgTextAltered.substr(strIdx);
		// To remove the "By: <name> to <name> on <date>" lines altogether:
		//msgTextAltered = msgTextAltered.substr(strIdx);
	}

	var wordWrapTheMsgText = true;
	if (typeof(pWordWrap) == "boolean")
		wordWrapTheMsgText = pWordWrap;
	if (wordWrapTheMsgText)
	{
		// Wrap the text to fit into the available message area.
		// Note: In Synchronet 3.15 (and some beta builds of 3.16), there seemed to
		// be a bug in the word_wrap() function where the word wrap length in Linux
		// was one less than Windows, so if the BBS is running 3.15 or earlier of
		// Synchronet, add 1 to the word wrap length if running in Linux.
		var textWrapLen = this.msgAreaWidth;
		if (system.version_num <= 31500)
			textWrapLen = gRunningInWindows ? this.msgAreaWidth : this.msgAreaWidth + 1;
		var msgTextWrapped = word_wrap(msgTextAltered, textWrapLen);
		retObj.messageLines = lfexpand(msgTextWrapped).split("\r\n");
		// Go through the message lines and trim them to ensure they'll easily fit
		// in the message display area without having to trim them later.  (Note:
		// this is okay to do since we're only using messageLines to display the
		// message on the screen; messageLines isn't used for quoting/replying).
		for (var msgLnIdx = 0; msgLnIdx < retObj.messageLines.length; ++msgLnIdx)
			retObj.messageLines[msgLnIdx] = shortenStrWithAttrCodes(retObj.messageLines[msgLnIdx], this.msgAreaWidth);
		// Set up some variables for displaying the message
		retObj.topMsgLineIdxForLastPage = retObj.messageLines.length - this.msgAreaHeight;
		if (retObj.topMsgLineIdxForLastPage < 0)
			retObj.topMsgLineIdxForLastPage = 0;
		// Variables for the scrollbar to show the fraction of the message shown
		retObj.msgFractionShown = this.msgAreaHeight / retObj.messageLines.length;
		if (retObj.msgFractionShown > 1)
			retObj.msgFractionShown = 1.0;
		retObj.numSolidScrollBlocks = Math.floor(this.msgAreaHeight * retObj.msgFractionShown);
		if (retObj.numSolidScrollBlocks == 0)
			retObj.numSolidScrollBlocks = 1;
	}
	else
	{
		retObj.messageLines = [];
		retObj.messageLines.push(msgTextAltered);
	}
	retObj.numNonSolidScrollBlocks = this.msgAreaHeight - retObj.numSolidScrollBlocks;
	retObj.solidBlockStartRow = this.msgAreaTop;

	return retObj;
}

// For the DigDistMsgReader class: Returns the index of the last read message in
// the current message area.  If reading personal email, this will look at the
// search results.  Otherwise, this will use the sub-board's last_read pointer.
// If there is no last read message or if there is a problem getting the last read
// message index, this method will return -1.
//
// Parameters:
//  pMailStartFromFirst: Optional boolean - Whether or not to start from the
//                       first message (rather than from the last message) if
//                       reading personal email.  Will stop looking at the first
//                       unread message.  Defaults to false.
//
// Return value: An object containing the following properties:
//               lastReadMsgIdx: The index of the last read message in the current message area
//               lastReadMsgNum: The number of the last read message in the current message area
function DigDistMsgReader_GetLastReadMsgIdxAndNum(pMailStartFromFirst)
{
	var retObj = {
		lastReadMsgIdx: -1,
		lastReadMsgNum: -1
	};

	if (this.readingPersonalEmail)
	{
		if (this.SearchingAndResultObjsDefinedForCurSub())
		{
			var startFromFirst = (typeof(pMailStartFromFirst) == "boolean" ? pMailStartFromFirst : false);
			if (startFromFirst)
			{
				for (var idx = 0; idx < this.msgSearchHdrs[this.subBoardCode].indexed.length; ++idx)
				{
					if ((this.msgSearchHdrs[this.subBoardCode].indexed[idx].attr & MSG_READ) == MSG_READ)
					{
						retObj.lastReadMsgIdx = idx;
						retObj.lastReadMsgNum = this.msgSearchHdrs[this.subBoardCode].indexed[idx].number;
					}
					else
						break;
				}
			}
			else
			{
				for (var idx = this.msgSearchHdrs[this.subBoardCode].indexed.length-1; idx >= 0; --idx)
				{
					if ((this.msgSearchHdrs[this.subBoardCode].indexed[idx].attr & MSG_READ) == MSG_READ)
					{
						retObj.lastReadMsgIdx = idx;
						retObj.lastReadMsgNum = this.msgSearchHdrs[this.subBoardCode].indexed[idx].number;
						break;
					}
				}
			}
			// Sanity checking for retObj.lastReadMsgIdx (note: this function should return -1 if
			// there is no last read message).
			if (retObj.lastReadMsgIdx >= this.msgSearchHdrs[this.subBoardCode].indexed.length)
			{
				retObj.lastReadMsgIdx = this.msgSearchHdrs[this.subBoardCode].indexed.length - 1;
				retObj.lastReadMsgNum = this.msgSearchHdrs[this.subBoardCode].indexed[retObj.lastReadMsgIdx].number;
			}
		}
	}
	else
	{
		//retObj.lastReadMsgIdx = this.AbsMsgNumToIdx(msg_area.sub[this.subBoardCode].last_read);
		retObj.lastReadMsgIdx = this.GetMsgIdx(msg_area.sub[this.subBoardCode].last_read);
		retObj.lastReadMsgNum = msg_area.sub[this.subBoardCode].last_read;
		/*
		this.hdrsForCurrentSubBoard = [];
		// hdrsForCurrentSubBoardByMsgNum is an object that maps absolute message numbers
		// to their index to hdrsForCurrentSubBoard
		this.hdrsForCurrentSubBoardByMsgNum = {};
		*/
		// Sanity checking for retObj.lastReadMsgIdx (note: this function should return -1 if
		// there is no last read message).
		var msgbase = new MsgBase(this.subBoardCode);
		if (msgbase.open())
		{
			// If retObj.lastReadMsgIdx is -1, as a result of GetMsgIdx(), then see what the last read
			// message index is according to the Synchronet message base.  If
			// this.hdrsForCurrentSubBoard.length has been populated, then if the last
			// message index according to Synchronet is greater than that, then set the
			// message index to the last index in this.hdrsForCurrentSubBoard.length.
			if (retObj.lastReadMsgIdx == -1)
			{
				var msgIdxAccordingToMsgbase = absMsgNumToIdx(msgbase, msg_area.sub[this.subBoardCode].last_read);
				if ((this.hdrsForCurrentSubBoard.length > 0) && (msgIdxAccordingToMsgbase >= this.hdrsForCurrentSubBoard.length))
				{
					retObj.lastReadMsgIdx = this.hdrsForCurrentSubBoard.length - 1;
					retObj.lastReadMsgNum = this.hdrsForCurrentSubBoard[retObj.lastReadMsgIdx].number;
				}
			}
			//if (retObj.lastReadMsgIdx >= msgbase.total_msgs)
			//	retObj.lastReadMsgIdx = msgbase.total_msgs - 1;
			// TODO: Is this code right?  Modified 3/24/2015 to replace
			// the above 2 commented lines.
			if ((retObj.lastReadMsgIdx < 0) || (retObj.lastReadMsgIdx >= msgbase.total_msgs))
			{
				// Look for the first message not marked as deleted
				var nonDeletedMsgIdx = this.FindNextNonDeletedMsgIdx(0, true);
				// If a non-deleted message was found, then set the last read
				// pointer to it.
				if (nonDeletedMsgIdx > -1)
				{
					var newLastRead = this.IdxToAbsMsgNum(nonDeletedMsgIdx);
					if (newLastRead > -1)
						msg_area.sub[this.subBoardCode].last_read = newLastRead;
					else
						msg_area.sub[this.subBoardCode].last_read = 0;
				}
				else
					msg_area.sub[this.subBoardCode].last_read = 0;
			}
		}
	}
	return retObj;
}

// For the DigDistMsgReader class: Returns the index of the message pointed to
// by the scan pointer in the current sub-board.  If reading personal email or
// if the message base isn't open, this will return 0.  If the scan pointer is
// 0 or if the messagebase is open and the scan pointer is invalid, this will
// return -1.
function DigDistMsgReader_GetScanPtrMsgIdx()
{
	if (this.readingPersonalEmail)
		return 0;
	if (msg_area.sub[this.subBoardCode].scan_ptr == 0)
		return -1;

	// If the user's scan pointer is a crazy value, that could be because
	// the user hasn't read messages in the sub-board yet.  In that case,
	// just use 0.  Otherwise, get the user's scan pointer message index.
	var msgIdx = 0;
	// If pMsgNum is 4294967295 (0xffffffff, or ~0), that is a special value
	// for the user's scan_ptr meaning it should point to the latest message
	// in the messagebase.
	if (msg_area.sub[this.subBoardCode].scan_ptr != 0xffffffff)
		msgIdx = this.GetMsgIdx(msg_area.sub[this.subBoardCode].scan_ptr);
	// Sanity checking for msgIdx
	var msgbase = new MsgBase(this.subBoardCode);
	if (msgbase.open())
	{
		if ((msgIdx < 0) || (msgIdx >= msgbase.total_msgs) || (msg_area.sub[this.subBoardCode].scan_ptr == 0xffffffff))
		{
			msgIdx = -1;
			// Look for the first message not marked as deleted
			var nonDeletedMsgIdx = this.FindNextNonDeletedMsgIdx(0, true);
			// If a non-deleted message was found, then set the scan pointer to it.
			if (nonDeletedMsgIdx > -1)
			{
				var newLastRead = this.IdxToAbsMsgNum(nonDeletedMsgIdx);
				if (newLastRead > -1)
					msg_area.sub[this.subBoardCode].scan_ptr = newLastRead;
				else
					msg_area.sub[this.subBoardCode].scan_ptr = 0;
			}
			else
				msg_area.sub[this.subBoardCode].scan_ptr = 0;
		}
		msgbase.close();
	}
	return msgIdx;
}

// For the DigDistMsgReader class: Returns whether there is a search specified
// (according to this.searchType) and the search result objects are defined for
// the current sub-board (as specified by this.subBoardCode).
//
// Return value: Boolean - Whether or not there is a search specified and the
//               search result objects are defined for the current sub-board
//               (as specified by this.subBoardCode).
function DigDistMsgReader_SearchingAndResultObjsDefinedForCurSub()
{
	return (this.SearchTypePopulatesSearchResults() && (this.msgSearchHdrs != null) &&
	         this.msgSearchHdrs.hasOwnProperty(this.subBoardCode) &&
	         (typeof(this.msgSearchHdrs[this.subBoardCode]) == "object") &&
	         (typeof(this.msgSearchHdrs[this.subBoardCode].indexed) != "undefined"));
}

// For the DigDistMsgReader class: Removes a message header from the search
// results array for the current sub-board.
//
// Parameters:
//  pMsgIdx: The index of the message header to remove (in the indexed messages,
//           not necessarily the actual message offset in the messagebase)
function DigDistMsgReader_RemoveFromSearchResults(pMsgIdx)
{
	if (typeof(pMsgIdx) != "number")
		return;

	if ((typeof(this.msgSearchHdrs) == "object") &&
	    this.msgSearchHdrs.hasOwnProperty(this.subBoardCode) &&
	    (typeof(this.msgSearchHdrs[this.subBoardCode].indexed) != "undefined"))
	{
		if ((pMsgIdx >= 0) && (pMsgIdx < this.msgSearchHdrs[this.subBoardCode].indexed.length))
			this.msgSearchHdrs[this.subBoardCode].indexed.splice(pMsgIdx, 1);
	}
}

// For the DigDistMsgReader class: Looks for the next message in the thread of
// a given message (by its header).
//
// Paramters:
//  pMsgHdr: A message header object - The next message in the thread will be
//           searched starting from this message
//  pThreadType: The type of threading to use.  Can be THREAD_BY_ID, THREAD_BY_TITLE,
//               THREAD_BY_AUTHOR, or THREAD_BY_TO_USER.
//  pPositionCursorForStatus: Optional boolean - Whether or not to move the cursor
//                            to the bottom row before outputting status messages.
//                            Defaults to false.
//
// Return value: The offset (index) of the next message thread, or -1 if none
//               was found.
function DigDistMsgReader_FindThreadNextOffset(pMsgHdr, pThreadType, pPositionCursorForStatus)
{
	if ((pMsgHdr == null) || (typeof(pMsgHdr) != "object"))
		return -1;

	var newMsgOffset = -1;

	switch (pThreadType)
	{
		case THREAD_BY_ID:
		default:
			// The thread_id field was introduced in Synchronet 3.16.  So, if
			// the Synchronet version is 3.16 or higher and the message header
			// has a thread_id field, then look for the next message with the
			// same thread ID.  If the Synchronet version is below 3.16 or there
			// is no thread ID, then fall back to using the header's thread_next,
			// if it exists.
			if ((system.version_num >= 31600) && (typeof(pMsgHdr.thread_id) == "number"))
			{
				// Look for the next message with the same thread ID.
				// Write "Searching.."  in case searching takes a while.
				console.print("\x01n");
				if (pPositionCursorForStatus)
				{
					console.gotoxy(1, console.screen_rows);
					console.cleartoeol();
					console.gotoxy(this.msgAreaLeft, console.screen_rows);
				}
				console.print("\x01h\x01ySearching\x01i...\x01n");
				// Look for the next message in the thread
				var nextMsgOffset = -1;
				var numOfMessages = this.NumMessages();
				/*
				if (pMsgHdr.offset < numOfMessages - 1)
				{
					var nextMsgHdr;
					for (var messageIdx = pMsgHdr.offset+1; (messageIdx < numOfMessages) && (nextMsgOffset == -1); ++messageIdx)
					{
						nextMsgHdr = this.GetMsgHdrByIdx(messageIdx);
						if (((nextMsgHdr.attr & MSG_DELETE) == 0) && (typeof(nextMsgHdr.thread_id) == "number") && (nextMsgHdr.thread_id == pMsgHdr.thread_id))
							nextMsgOffset = nextMsgHdr.offset;
					}
				}
				*/
				if (this.GetMsgIdx(pMsgHdr.number) < numOfMessages - 1)
				{
					var nextMsgHdr;
					for (var messageIdx = this.GetMsgIdx(pMsgHdr.number)+1; (messageIdx < numOfMessages) && (nextMsgOffset == -1); ++messageIdx)
					{
						nextMsgHdr = this.GetMsgHdrByIdx(messageIdx);
						if (((nextMsgHdr.attr & MSG_DELETE) == 0) && (typeof(nextMsgHdr.thread_id) == "number") && (nextMsgHdr.thread_id == pMsgHdr.thread_id))
						{
							//nextMsgOffset = nextMsgHdr.offset;
							nextMsgOffset = this.GetMsgIdx(nextMsgHdr.number);
						}
					}
				}
				if (nextMsgOffset > -1)
					newMsgOffset = nextMsgOffset;
			}
			// Fall back to thread_next if the Synchronet version is below 3.16 or there is
			// no thread_id field in the header
			else if ((typeof(pMsgHdr.thread_next) == "number") && (pMsgHdr.thread_next > 0))
			{
				//newMsgOffset = this.AbsMsgNumToIdx(pMsgHdr.thread_next);
				newMsgOffset = this.GetMsgIdx(pMsgHdr.thread_next);
			}
			break;
		case THREAD_BY_TITLE:
		case THREAD_BY_AUTHOR:
		case THREAD_BY_TO_USER:
			// Title (subject) searching will look for the subject anywhere in the
			// other messages' subjects (not a fully exact subject match), so if
			// the message subject is blank, we won't want to do the search.
			var doSearch = true;
			if ((pThreadType == THREAD_BY_TITLE) && (pMsgHdr.subject.length == 0))
				doSearch = false;
			if (doSearch)
			{
				var subjUppercase = "";
				var fromNameUppercase = "";
				var toNameUppercase = "";

				// Set up a message header matching function, depending on
				// which field of the header we want to match
				var msgHdrMatch;
				if (pThreadType == THREAD_BY_TITLE)
				{
					subjUppercase = pMsgHdr.subject.toUpperCase();
					// Remove any leading instances of "RE:" from the subject
					while (/^RE:/.test(subjUppercase))
						subjUppercase = subjUppercase.substr(3);
					while (/^RE: /.test(subjUppercase))
						subjUppercase = subjUppercase.substr(4);
					// Remove any leading & trailing whitespace from the subject
					subjUppercase = trimSpaces(subjUppercase, true, true, true);
					msgHdrMatch = function(pMsgHdr) {
						return (((pMsgHdr.attr & MSG_DELETE) == 0) && (pMsgHdr.subject.toUpperCase().indexOf(subjUppercase, 0) > -1));
					};
				}
				else if (pThreadType == THREAD_BY_AUTHOR)
				{
					fromNameUppercase = pMsgHdr.from.toUpperCase();
					msgHdrMatch = function(pMsgHdr) {
						return (((pMsgHdr.attr & MSG_DELETE) == 0) && (pMsgHdr.from.toUpperCase() == fromNameUppercase));
					};
				}
				else if (pThreadType == THREAD_BY_TO_USER)
				{
					toNameUppercase = pMsgHdr.to.toUpperCase();
					msgHdrMatch = function(pMsgHdr) {
						return (((pMsgHdr.attr & MSG_DELETE) == 0) && (pMsgHdr.to.toUpperCase() == toNameUppercase));
					};
				}

				// Perform the search
				// Write "Searching.."  in case searching takes a while.
				console.print("\x01n");
				if (pPositionCursorForStatus)
				{
					console.gotoxy(1, console.screen_rows);
					console.cleartoeol();
					console.gotoxy(this.msgAreaLeft, console.screen_rows);
				}
				console.print("\x01h\x01ySearching\x01i...\x01n");
				// Look for the next message that contains the given message's subject
				var nextMsgOffset = -1;
				var numOfMessages = this.NumMessages();
				/*
				if (pMsgHdr.offset < numOfMessages - 1)
				{
					var nextMsgHdr;
					for (var messageIdx = pMsgHdr.offset+1; (messageIdx < numOfMessages) && (nextMsgOffset == -1); ++messageIdx)
					{
						nextMsgHdr = this.GetMsgHdrByIdx(messageIdx);
						if (msgHdrMatch(nextMsgHdr))
							nextMsgOffset = nextMsgHdr.offset;
					}
				}
				*/
				if (this.GetMsgIdx(pMsgHdr.number) < numOfMessages - 1)
				{
					var nextMsgHdr;
					for (var messageIdx = this.GetMsgIdx(pMsgHdr.number)+1; (messageIdx < numOfMessages) && (nextMsgOffset == -1); ++messageIdx)
					{
						nextMsgHdr = this.GetMsgHdrByIdx(messageIdx);
						if (msgHdrMatch(nextMsgHdr))
						{
							//nextMsgOffset = nextMsgHdr.offset;
							nextMsgOffset = this.GetMsgIdx(nextMsgHdr.number);
						}
					}
				}
				if (nextMsgOffset > -1)
					newMsgOffset = nextMsgOffset;
			}
			break;
	}

	// If no messages were found, then output a message to say so with a momentary pause.
	if (newMsgOffset == -1)
	{
		if (pPositionCursorForStatus)
		{
			console.gotoxy(1, console.screen_rows);
			console.cleartoeol();
			console.gotoxy(this.msgAreaLeft, console.screen_rows);
		}
		else
			console.crlf();
		console.print("\x01n\x01h\x01yNo messages found.\x01n");
		mswait(ERROR_PAUSE_WAIT_MS);
	}

	return newMsgOffset;
}

// For the DigDistMsgReader class: Looks for the previous message in the thread of
// a given message (by its header).
//
// Paramters:
//  pMsgHdr: A message header object - The previous message in the thread will be
//           searched starting from this message
//  pThreadType: The type of threading to use.  Can be THREAD_BY_ID, THREAD_BY_TITLE,
//               THREAD_BY_AUTHOR, or THREAD_BY_TO_USER.
//  pPositionCursorForStatus: Optional boolean - Whether or not to move the cursor
//                            to the bottom row before outputting status messages.
//                            Defaults to false.
//
// Return value: The offset (index) of the previous message thread, or -1 if
//               none was found.
function DigDistMsgReader_FindThreadPrevOffset(pMsgHdr, pThreadType, pPositionCursorForStatus)
{
	if ((pMsgHdr == null) || (typeof(pMsgHdr) != "object"))
		return -1;

	var newMsgOffset = -1;

	switch (pThreadType)
	{
		case THREAD_BY_ID:
		default:
			// The thread_id field was introduced in Synchronet 3.16.  So, if
			// the Synchronet version is 3.16 or higher and the message header
			// has a thread_id field, then look for the previous message with the
			// same thread ID.  If the Synchronet version is below 3.16 or there
			// is no thread ID, then fall back to using the header's thread_next,
			// if it exists.
			if ((system.version_num >= 31600) && (typeof(pMsgHdr.thread_id) == "number"))
			{
				// Look for the previous message with the same thread ID.
				// Write "Searching.." in case searching takes a while.
				console.print("\x01n");
				if (pPositionCursorForStatus)
				{
					console.gotoxy(1, console.screen_rows);
					console.cleartoeol();
					console.gotoxy(this.msgAreaLeft, console.screen_rows);
				}
				console.print("\x01h\x01ySearching\x01i...\x01n");
				// Look for the previous message in the thread
				var nextMsgOffset = -1;
				/*
				if (pMsgHdr.offset > 0)
				{
					var prevMsgHdr;
					for (var messageIdx = pMsgHdr.offset-1; (messageIdx >= 0) && (nextMsgOffset == -1); --messageIdx)
					{
						prevMsgHdr = this.GetMsgHdrByIdx(messageIdx);
						if (((prevMsgHdr.attr & MSG_DELETE) == 0) && (typeof(prevMsgHdr.thread_id) == "number") && (prevMsgHdr.thread_id == pMsgHdr.thread_id))
							nextMsgOffset = prevMsgHdr.offset;
					}
				}
				*/
				if (this.GetMsgIdx(pMsgHdr.number) > 0)
				{
					var prevMsgHdr;
					for (var messageIdx = this.GetMsgIdx(pMsgHdr.number)-1; (messageIdx >= 0) && (nextMsgOffset == -1); --messageIdx)
					{
						prevMsgHdr = this.GetMsgHdrByIdx(messageIdx);
						if (((prevMsgHdr.attr & MSG_DELETE) == 0) && (typeof(prevMsgHdr.thread_id) == "number") && (prevMsgHdr.thread_id == pMsgHdr.thread_id))
						{
							//nextMsgOffset = prevMsgHdr.offset;
							nextMsgOffset = this.GetMsgIdx(prevMsgHdr.number);
							nextMsgOffset = 0;
						}
					}
				}
				if (nextMsgOffset > -1)
					newMsgOffset = nextMsgOffset;
			}
			// Fall back to thread_next if the Synchronet version is below 3.16 or there is
			// no thread_id field in the header
			else if ((typeof(pMsgHdr.thread_back) == "number") && (pMsgHdr.thread_back > 0))
			{
				//newMsgOffset = this.AbsMsgNumToIdx(pMsgHdr.thread_back);
				newMsgOffset = this.GetMsgIdx(pMsgHdr.thread_back);
				if (newMsgOffset < 0)
					newMsgOffset = 0;
			}

			/*
			// If thread_back is valid for the message header, then use that.
			if ((typeof(pMsgHdr.thread_back) == "number") && (pMsgHdr.thread_back > 0))
			{
				//newMsgOffset = this.AbsMsgNumToIdx(pMsgHdr.thread_back);
				newMsgOffset = this.GetMsgIdx(pMsgHdr.thread_back);
			}
			else
			{
				// If thread_id is defined and the index of the first message
				// in the thread is before the current message, then search
				// backwards for messages with a matching thread_id.
				//var firstThreadMsgIdx = this.AbsMsgNumToIdx(pMsgHdr.thread_first);
				var firstThreadMsgIdx = this.GetMsgIdx(pMsgHdr.thread_first);
				if ((typeof(pMsgHdr.thread_id) == "number") && (firstThreadMsgIdx < pMsgHdr.offset))
				{
					// Note (2014-10-11): Digital Man said thread_id was
					// introduced in Synchronet version 3.16 and was still
					// a work in progress and isn't 100% accurate for
					// networked sub-boards.

					// Look for the previous message with the same thread ID.
					// Note: I'm not sure when thread_id was introduced in
					// Synchronet, so I'm not sure of the minimum version where
					// this will work.
					// Write "Searching.." in case searching takes a while.
					console.print("\x01n");
					if (pPositionCursorForStatus)
					{
						console.gotoxy(1, console.screen_rows);
						console.cleartoeol();
						console.gotoxy(this.msgAreaLeft, console.screen_rows);
					}
					console.print("\x01h\x01ySearching\x01i...\x01n");
					// Look for the previous message in the thread
					var nextMsgOffset = -1;
					if (pMsgHdr.offset > 0)
					{
						for (var messageIdx = pMsgHdr.offset-1; (messageIdx >= 0) && (nextMsgOffset == -1); --messageIdx)
						{
							var prevMsgHdr = this.GetMsgHdrByIdx(messageIdx);
							if (((prevMsgHdr.attr & MSG_DELETE) == 0) && (typeof(prevMsgHdr.thread_id) == "number") && (prevMsgHdr.thread_id == pMsgHdr.thread_id))
								nextMsgOffset = prevMsgHdr.offset;
						}
					}
					if (nextMsgOffset > -1)
						newMsgOffset = nextMsgOffset;
				}
			}
			*/
			break;
		case THREAD_BY_TITLE:
		case THREAD_BY_AUTHOR:
		case THREAD_BY_TO_USER:
			// Title (subject) searching will look for the subject anywhere in the
			// other messages' subjects (not a fully exact subject match), so if
			// the message subject is blank, we won't want to do the search.
			var doSearch = true;
			if ((pThreadType == THREAD_BY_TITLE) && (pMsgHdr.subject.length == 0))
				doSearch = false;
			if (doSearch)
			{
				var subjUppercase = "";
				var fromNameUppercase = "";
				var toNameUppercase = "";

				// Set up a message header matching function, depending on
				// which field of the header we want to match
				var msgHdrMatch;
				if (pThreadType == THREAD_BY_TITLE)
				{
					subjUppercase = pMsgHdr.subject.toUpperCase();
					// Remove any leading instances of "RE:" from the subject
					while (/^RE:/.test(subjUppercase))
						subjUppercase = subjUppercase.substr(3);
					while (/^RE: /.test(subjUppercase))
						subjUppercase = subjUppercase.substr(4);
					// Remove any leading & trailing whitespace from the subject
					subjUppercase = trimSpaces(subjUppercase, true, true, true);
					msgHdrMatch = function(pMsgHdr) {
						return (((pMsgHdr.attr & MSG_DELETE) == 0) && (pMsgHdr.subject.toUpperCase().indexOf(subjUppercase, 0) > -1));
					};
				}
				else if (pThreadType == THREAD_BY_AUTHOR)
				{
					fromNameUppercase = pMsgHdr.from.toUpperCase();
					msgHdrMatch = function(pMsgHdr) {
						return (((pMsgHdr.attr & MSG_DELETE) == 0) && (pMsgHdr.from.toUpperCase() == fromNameUppercase));
					};
				}
				else if (pThreadType == THREAD_BY_TO_USER)
				{
					toNameUppercase = pMsgHdr.to.toUpperCase();
					msgHdrMatch = function(pMsgHdr) {
						return (((pMsgHdr.attr & MSG_DELETE) == 0) && (pMsgHdr.to.toUpperCase() == toNameUppercase));
					};
				}

				// Perform the search
				// Write "Searching.."  in case searching takes a while.
				console.print("\x01n");
				if (pPositionCursorForStatus)
				{
					console.gotoxy(1, console.screen_rows);
					console.cleartoeol();
					console.gotoxy(this.msgAreaLeft, console.screen_rows);
				}
				console.print("\x01h\x01ySearching\x01i...\x01n");
				// Look for the next message that contains the given message's subject
				var nextMsgOffset = -1;
				/*
				if (pMsgHdr.offset > 0)
				{
					var nextMsgHdr;
					for (var messageIdx = pMsgHdr.offset-1; (messageIdx >= 0) && (nextMsgOffset == -1); --messageIdx)
					{
						nextMsgHdr = this.GetMsgHdrByIdx(messageIdx);
						if (msgHdrMatch(nextMsgHdr))
							nextMsgOffset = nextMsgHdr.offset;
					}
				}
				*/
				if (this.GetMsgIdx(pMsgHdr.number) > 0)
				{
					var nextMsgHdr;
					for (var messageIdx = this.GetMsgIdx(pMsgHdr.number)-1; (messageIdx >= 0) && (nextMsgOffset == -1); --messageIdx)
					{
						nextMsgHdr = this.GetMsgHdrByIdx(messageIdx);
						if (msgHdrMatch(nextMsgHdr))
						{
							//nextMsgOffset = nextMsgHdr.offset;
							nextMsgOffset = this.GetMsgIdx(nextMsgHdr.number);
						}
					}
				}
				if (nextMsgOffset > -1)
					newMsgOffset = nextMsgOffset;
			}
			break;
	}

	// If no messages were found, then output a message to say so with a momentary pause.
	if (newMsgOffset == -1)
	{
		if (pPositionCursorForStatus)
		{
			console.gotoxy(1, console.screen_rows);
			console.cleartoeol();
			console.gotoxy(this.msgAreaLeft, console.screen_rows);
		}
		else
			console.crlf();
		console.print("\x01n\x01h\x01yNo messages found.\x01n");
		mswait(ERROR_PAUSE_WAIT_MS);
	}

	return newMsgOffset;
}

// For the DigDistMsgReader class: Calculates the top message index for a page,
// for the traditional-style message list.
//
// Parameters:
//  pPageNum: A page number (1-based)
function DigDistMsgReader_CalcTraditionalMsgListTopIdx(pPageNum)
{
   if (this.reverseListOrder)
      this.tradListTopMsgIdx = this.NumMessages() - (this.tradMsgListNumLines * (pPageNum-1)) - 1;
   else
      this.tradListTopMsgIdx = (this.tradMsgListNumLines * (pPageNum-1));
}

// For the DigDistMsgReader class: Calculates the top message index for a page,
// for the lightbar message list.
//
// Parameters:
//  pPageNum: A page number (1-based)
function DigDistMsgReader_CalcLightbarMsgListTopIdx(pPageNum)
{
	if (this.reverseListOrder)
		this.lightbarListTopMsgIdx = this.NumMessages() - (this.lightbarMsgListNumLines * (pPageNum-1)) - 1;
	else
	{
		//this.lightbarListTopMsgIdx = (this.lightbarMsgListNumLines * (pPageNum-1));
		var pageIdx = pPageNum - 1;
		if (pageIdx < 0)
			pageIdx = 0;
		this.lightbarListTopMsgIdx = this.lightbarMsgListNumLines * pageIdx;
	}
}

// For the DigDistMsgReader class: Given a message number (1-based), this calculates
// the screen index veriables (stored in the object) for the message list.  This is
// used for the enhanced reader mode when we want the message list to be in the
// correct place for the message being read.
//
// Parameters:
//  pMsgNum: The message number (1-based)
function DigDistMsgReader_CalcMsgListScreenIdxVarsFromMsgNum(pMsgNum)
{
	// Calculate the message list variables
	var numItemsPerPage = this.tradMsgListNumLines;
	if (this.msgListUseLightbarListInterface && canDoHighASCIIAndANSI())
		numItemsPerPage = this.lightbarMsgListNumLines;
	var newPageNum = findPageNumOfItemNum(pMsgNum, numItemsPerPage, this.NumMessages(), this.reverseListOrder);
	this.CalcTraditionalMsgListTopIdx(newPageNum);
	this.CalcLightbarMsgListTopIdx(newPageNum);
	this.lightbarListSelectedMsgIdx = pMsgNum - 1;
	if (this.lightbarListCurPos == null)
		this.lightbarListCurPos = {};
	this.lightbarListCurPos.x = 1;
	this.lightbarListCurPos.y = this.lightbarMsgListStartScreenRow + ((pMsgNum-1) - this.lightbarListTopMsgIdx);
}

// For the DigDistMsgReader class: Validates a user's choice in message area.
// Returns a status/error message for the caller to display if there's an
// error.  This function outputs intermediate status messages (i.e.,
// "Searching..").
//
// Parameters:
//  pGrpIdx: The message group index (i.e., bbs.curgrp)
//  pSubIdx: The message sub-board index (i.e., bbs.cursub)
//  pCurPos: Optional - An object containing x and y properties representing
//           the cursor position.  Used for outputting intermediate status
//           messages, but not for outputting the error message.
//
// Return value: An object containing the following properties:
//               msgAreaGood: A boolean to indicate whether the message area
//                            can be selected
//               errorMsg: If the message area can't be selected, this string
//                         will contain an eror message.  Otherwise, this will
//                         be an empty string.
function DigDistMsgReader_ValidateMsgAreaChoice(pGrpIdx, pSubIdx, pCurPos)
{
	var retObj = {
		msgAreaGood: true,
		errorMsg: ""
	};

	// Get the internal code of the sub-board from the given group & sub-board
	// indexes
	var subCode = msg_area.grp_list[pGrpIdx].sub_list[pSubIdx].code;

	// If a search is specified that would populate the search results, then do
	// a search in the given sub-board.
	if (this.SearchTypePopulatesSearchResults())
	{
		// See if we can use pCurPos to move the cursor before displaying messages
		var useCurPos = (console.term_supports(USER_ANSI) && (typeof(pCurPos) == "object") &&
		                 (typeof(pCurPos.x) == "number") && (typeof(pCurPos.y) == "number"));

		// TODO: In case new messages were posted in this sub-board, it might help
		// to check the current number of messages vs. the previous number of messages
		// and search the new messages if there are more.

		// Determine whether or not to search - If there are no search results for
		// the given sub-board already, then do a search in the sub-board.
		var doSearch = true;
		if (this.msgSearchHdrs.hasOwnProperty(subCode) &&
		    (typeof(this.msgSearchHdrs[subCode]) == "object") &&
		    (typeof(this.msgSearchHdrs[subCode].indexed) != "undefined"))
		{
			doSearch = (this.msgSearchHdrs[subCode].indexed.length == 0);
		}
		if (doSearch)
		{
			if (useCurPos)
			{
				console.gotoxy(pCurPos);
				console.cleartoeol("\x01n");
				console.gotoxy(pCurPos);
			}
			console.print("\x01n\x01h\x01wSearching\x01i...\x01n");
			this.msgSearchHdrs[subCode] = searchMsgbase(subCode, this.searchType, this.searchString, this.readingPersonalEmailFromUser);
			// If there are no messages, then set the return object variables to indicate so.
			if (this.msgSearchHdrs[subCode].indexed.length == 0)
			{
				retObj.msgAreaGood = false;
				retObj.errorMsg = "No search results found";
			}
		}
	}
	else
	{
		// No search is specified.  Just check to see if there are any messages
		// to read in the given sub-board.
		var msgBase = new MsgBase(subCode);
		if (msgBase.open())
		{
			if (msgBase.total_msgs == 0)
			{
				retObj.msgAreaGood = false;
				retObj.errorMsg = "No messages in that message area";
			}
			msgBase.close();
		}
	}

	return retObj;
}

// For the DigDistMsgReader class: Validates a message if the sub-board
// requires message validation.
//
// Parameters:
//  pSubBoardCode: The internal code of the sub-board
//  pMsgNum: The message number
//
// Return value: Boolean - Whether or not validating the message was successful
function DigDistMsgReader_ValidateMsg(pSubBoardCode, pMsgNum)
{
	if (!msg_area.sub[pSubBoardCode].is_moderated)
		return true;

	var validationSuccessful = false;
	var msgbase = new MsgBase(this.subBoardCode);
	if (msgbase.open())
	{
		var msgHdr = msgbase.get_msg_header(false, pMsgNum, false);
		if (msgHdr != null)
		{
			if ((msgHdr.attr & MSG_VALIDATED) == 0)
			{
				msgHdr.attr |= MSG_VALIDATED;
				validationSuccessful = msgbase.put_msg_header(false, msgHdr.number, msgHdr);
			}
			else
				validationSuccessful = true;
		}
		msgbase.close();
	}

	return validationSuccessful;
}

// For the DigDistMsgReader class: Gets the current sub-board's group name and description.
//
// Return value: An object with the following properties:
//               grpName: The group name
//               grpDesc: The group description
function DigDistMsgReader_GetGroupNameAndDesc()
{
	var retObj = {
		grpName: "",
		grpDesc: ""
	}
	var msgbase = new MsgBase(this.subBoardCode);
	if (msgbase.open())
	{
		retObj.grpName = msgbase.cfg.grp_name;
		retObj.grpDesc = msgbase.cfg.description;
		msgbase.close();
	}
	return retObj;
}

// For the DigDistMsgReader class:  Lets the user manage their preferences/settings (scrollable/ANSI user interface).
//
// Parameters:
//  pDrawBottomhelpLineFn: A function to draw the bottom help line (it could be the message list
//                         help line or reader help line), for refreshing after displaying an error.
//                         This function must take the reader (this) as a parameter.
//
// Return value: An object containing the following properties:
//               needWholeScreenRefresh: Boolean - Whether or not the whole screen needs to be
//                                       refreshed (i.e., when the user has edited their twitlist)
//               optionBoxTopLeftX: The top-left screen column of the option box
//               optionBoxTopLeftY: The top-left screen row of the option box
//               optionBoxWidth: The width of the option box
//               optionBoxHeight: The height of the option box
//               userTwitListChanged: Boolean - Whether or not the user's personal twit list changed
function DigDistMsgReader_DoUserSettings_Scrollable(pDrawBottomhelpLineFn)
{
	var retObj = {
		needWholeScreenRefresh: false,
		optionBoxTopLeftX: 1,
		optionBoxTopLeftY: 1,
		optionBoxWidth: 0,
		optionBoxHeight: 0,
		userTwitListChanged: false
	};

	if (!canDoHighASCIIAndANSI())
	{
		this.DoUserSettings_Traditional();
		return retObj;
	}

	// Save the user's current settings so that we can check them later to see if any
	// of them changed, in order to determine whether to save the user's settings file.
	var gOriginalUseEnhScrollbar = this.useEnhReaderScrollbar;
	/*
	var originalSettings = {};
	for (var prop in gUserSettings)
	{
		if (gUserSettings.hasOwnProperty(prop))
			originalSettings[prop] = gUserSettings[prop];
	}
	*/

	// Create the user settings box
	var optBoxTitle = "Setting                                      Enabled";
	var optBoxWidth = ChoiceScrollbox_MinWidth();
	var optBoxHeight = 10;
	var optBoxStartX = this.msgAreaLeft + Math.floor((this.msgAreaWidth/2) - (optBoxWidth/2));
	if (optBoxStartX < this.msgAreaLeft)
		optBoxStartX = this.msgAreaLeft;
	var optionBox = new ChoiceScrollbox(optBoxStartX, this.msgAreaTop+1, optBoxWidth, optBoxHeight, optBoxTitle,
										null/*gConfigSettings*/, false, true);
	optionBox.addInputLoopExitKey(CTRL_U);
	// Update the bottom help text to be more specific to the user settings box
	var bottomBorderText = "\x01n\x01h\x01c" + UP_ARROW + "\x01b, \x01c" + DOWN_ARROW + "\x01b, \x01cEnter\x01y=\x01bSelect\x01n\x01c/\x01h\x01btoggle, "
	                     + "\x01cESC\x01n\x01c/\x01hQ\x01n\x01c/\x01hCtrl-U\x01y=\x01bClose";
	// This one contains the page navigation keys..  Don't really need to show those,
	// since the settings box only has one page right now:
	/*var bottomBorderText = "\x01n\x01h\x01c"+ UP_ARROW + "\x01b, \x01c"+ DOWN_ARROW + "\x01b, \x01cN\x01y)\x01bext, \x01cP\x01y)\x01brev, "
						   + "\x01cF\x01y)\x01birst, \x01cL\x01y)\x01bast, \x01cEnter\x01y=\x01bSelect, "
						   + "\x01cESC\x01n\x01c/\x01hQ\x01n\x01c/\x01hCtrl-U\x01y=\x01bClose";*/

	optionBox.setBottomBorderText(bottomBorderText, true, false);

	// Add the options to the option box
	const checkIdx = 48;
	const ENH_SCROLLBAR_OPT_INDEX = optionBox.addTextItem("Scrollbar in reader                            [ ]");
	if (this.useEnhReaderScrollbar)
		optionBox.chgCharInTextItem(ENH_SCROLLBAR_OPT_INDEX, checkIdx, CHECK_CHAR);

	// Create an object containing toggle values (true/false) for each option index
	var optionToggles = {};
	optionToggles[ENH_SCROLLBAR_OPT_INDEX] = this.useEnhReaderScrollbar;

	// Other actions
	var USER_TWITLIST_OPT_INDEX = optionBox.addTextItem("Personal twit list");

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
				// Toggle the option and refresh it on the screen
				optionToggles[itemIndex] = !optionToggles[itemIndex];
				if (optionToggles[itemIndex])
					optionBox.chgCharInTextItem(itemIndex, checkIdx, CHECK_CHAR);
				else
					optionBox.chgCharInTextItem(itemIndex, checkIdx, " ");
				optionBox.refreshItemCharOnScreen(itemIndex, checkIdx);

				// Toggle the setting for the user in global user setting object.
				switch (itemIndex)
				{
					case ENH_SCROLLBAR_OPT_INDEX:
						this.readerObj.useEnhReaderScrollbar = !this.readerObj.useEnhReaderScrollbar;
						break;
					default:
						break;
				}
			}
			// For options that aren't on/off toggle options, take the appropriate action.
			else
			{
				switch (itemIndex)
				{
					//case DICTIONARY_OPT_INDEX:
					//	break;
					case USER_TWITLIST_OPT_INDEX:
						console.clear("\x01n", false);
						console.editfile(gUserTwitListFilename);
						// Re-read the user's twitlist and see if the user's twitlist changed
						var oldUserTwitList = this.readerObj.userSettings.twitList;
						this.readerObj.userSettings.twitList = [];
						this.readerObj.ReadUserSettingsFile(true);
						retObj.userTwitListChanged = !arraysHaveSameValues(this.readerObj.userSettings.twitList, oldUserTwitList);
						optionBox.continueInputLoopOverride = false; // Exit the input loop of the option box
						retObj.needWholeScreenRefresh = true;
						break;
					default:
						break;
				}
			}
		}
	}); // Option box enter key override function

	// Display the option box and have it do its input loop
	var boxRetObj = optionBox.doInputLoop(true);

	// If the user changed any of their settings, then save the user settings.
	// If the save fails, then output an error message.
	var settingsChanged = false;
	settingsChanged = (gOriginalUseEnhScrollbar != this.useEnhReaderScrollbar);
	for (var prop in this.userSettings)
	{
		if (this.userSettings.hasOwnProperty(prop))
		{
			// TODO
			//settingsChanged = settingsChanged || (originalSettings[prop] != this.userSettings[prop]);
			if (settingsChanged)
				break;
		}
	}
	if (settingsChanged)
	{
		if (!this.WriteUserSettingsFile())
		{
			writeWithPause(1, console.screen_rows, "\x01n\x01y\x01hFailed to save settings!\x01n", ERROR_PAUSE_WAIT_MS, "\x01n", true);
			// Refresh the help line
			pDrawBottomhelpLineFn(this);
		}
	}

	optionBox.addInputLoopExitKey(CTRL_U);

	// Prepare return object values and return
	retObj.optionBoxTopLeftX = optionBox.dimensions.topLeftX;
	retObj.optionBoxTopLeftY = optionBox.dimensions.topLeftY;
	retObj.optionBoxWidth = optionBox.dimensions.width;
	retObj.optionBoxHeight = optionBox.dimensions.height;
	return retObj;
}
// For the DigDistMsgReader class:  Lets the user manage their preferences/settings (traditional user interface)
//
// Return value: An object containing the following properties:
//               needWholeScreenRefresh: Boolean - Whether or not the whole screen needs to be
//                                       refreshed (i.e., when the user has edited their twitlist)
//               optionBoxTopLeftX: The top-left screen column of the option box
//               optionBoxTopLeftY: The top-left screen row of the option box
//               optionBoxWidth: The width of the option box
//               optionBoxHeight: The height of the option box
//               userTwitListChanged: Boolean - Whether or not the user's personal twit list changed
function DigDistMsgReader_DoUserSettings_Traditional()
{
	var retObj = {
		needWholeScreenRefresh: true,
		optionBoxTopLeftX: 1,
		optionBoxTopLeftY: 1,
		optionBoxWidth: 0,
		optionBoxHeight: 0,
		userTwitListChanged: false
	};

	console.crlf();
	console.print("\x01n\x01c\x01hU\x01n\x01cser \x01hS\x01n\x01cettings\x01n\r\n");
	console.print("\x01c\x01h1\x01g: \x01n\x01c\x01hP\x01n\x01cersonal \x01ht\x01n\x01cwit list\x01n\r\n");
	var USER_TWITLIST_OPT_NUM = 1;
	var HIGHEST_CHOICE_NUM = USER_TWITLIST_OPT_NUM;
	console.crlf();
	console.print("\x01cYour choice (\x01hQ\x01n\x01c: Quit)\x01h: \x01g");
	var userChoiceNum = console.getnum(HIGHEST_CHOICE_NUM);
	console.print("\x01n");
	var userChoiceStr = userChoiceNum.toString().toUpperCase();
	if (userChoiceStr.length == 0 || userChoiceStr == "Q")
		return retObj;

	switch (userChoiceNum)
	{
		case USER_TWITLIST_OPT_NUM:
			console.editfile(gUserTwitListFilename);
			// Re-read the user's twitlist and see if the user's twitlist changed
			var oldUserTwitList = this.userSettings.twitList;
			this.userSettings.twitList = [];
			this.ReadUserSettingsFile(true);
			retObj.userTwitListChanged = !arraysHaveSameValues(this.userSettings.twitList, oldUserTwitList);
			retObj.needWholeScreenRefresh = true;
			break;
	}

	// For future changes, if any settings that apply to the traditional interface:
	/*
	if (!WriteUserSettingsFile())
	{
		console.print("\x01n\r\n\x01y\x01hFailed to save settings!\x01n");
		console.crlf();
		console.pause();
	}
	*/

	return retObj;
}

// For the DigDistMsgReader class: Starts indexed mode
function DigDistMsgReader_DoIndexedMode()
{
	if (this.msgListUseLightbarListInterface && canDoHighASCIIAndANSI())
		this.DoIndexedModeLightbar();
	else
		this.DoIndexedModeTraditional();
}

function DigDistMsgReader_DoIndexedModeLightbar()
{
	// Stuff for DDMsgReader "Indexed" reader mode:
	// https://gitlab.synchro.net/main/sbbs/-/issues/354
	for (var grpIdx = 0; grpIdx < msg_area.grp_list.length; ++grpIdx)
	{
		console.print(msg_area.grp_list[grpIdx].name + " - " + msg_area.grp_list[grpIdx].description + ":\r\n");
		for (var subIdx = 0; subIdx < msg_area.grp_list[grpIdx].sub_list.length; ++subIdx)
		{
			//msg_area.grp_list[grpIdx].sub_list[subIdx].
			//console.print(" " + msg_area.grp_list[grpIdx].sub_list[subIdx].name + " - " + msg_area.grp_list[grpIdx].sub_list[subIdx].description + "\r\n");
			// scan_ptr: user's current new message scan pointer (highest-read message number)
			var displayThisSub = false;
			// posts: number of messages currently posted to this sub-board (introduced in v3.18c)
			var totalNumMsgsInSub = msg_area.grp_list[grpIdx].sub_list[subIdx].posts;
			var newMsgsInSub = 0;
			if (typeof(msg_area.grp_list[grpIdx].sub_list[subIdx].scan_ptr) === "number")
			{
				var msgbase = new MsgBase(msg_area.grp_list[grpIdx].sub_list[subIdx].code);
				if (msgbase.open())
				{
					//displayThisSub = (msg_area.grp_list[grpIdx].sub_list[subIdx].scan_ptr < msgbase.last_msg);
					if (msg_area.grp_list[grpIdx].sub_list[subIdx].scan_ptr < msgbase.last_msg)
					{
						displayThisSub = true;
						var msgIdx = msgbase.get_msg_index(false, msg_area.grp_list[grpIdx].sub_list[subIdx].scan_ptr, false);
						if (msgIdx != null)
						{
							newMsgsInSub = msg_area.grp_list[grpIdx].sub_list[subIdx].posts - msgIdx.offset;
							if (newMsgsInSub < 0) newMsgsInSub = 0;
						}
					}
					msgbase.close();
				}
			}
			/*
			// last_read: user's last-read message number
			if (typeof(msg_area.grp_list[grpIdx].sub_list[subIdx].last_read) === "number")
			{
				
			}
			*/
			if (displayThisSub)
			{
				var descWidth = 50;
				var numMsgsWidth = 5;
				var numNewMsgsWidth = 5;
				var formatStr = "%-" + descWidth + "s %" + numMsgsWidth + "s %" + numNewMsgsWidth + "s";
				var subDesc = msg_area.grp_list[grpIdx].sub_list[subIdx].name + " - " + msg_area.grp_list[grpIdx].sub_list[subIdx].description;
				subDesc = subDesc.substr(0, descWidth);
				printf(formatStr + "\r\n", "Description", "Total", "New");
				printf(formatStr + "\r\n", subDesc, totalNumMsgsInSub, newMsgsInSub);
				console.crlf();
			}
		}
	}
}

function DigDistMsgReader_DoIndexedModeTraditional()
{
}

function DigDistMsgReader_MakeLightbarIndexedModeMenu()
{
}

// For the DigDistMsgReader class: Writes message lines to a file on the BBS
// machine.
//
// Parameters:
//  pMsgHdr: The header object for the message
//  pFilename: The name of the file to write the message to
//
// Return value: An object containing the following properties:
//               succeeded: Boolean - Whether or not the file was successfully written
//               errorMsg: String - On failure, will contain the reason it failed
function DigDistMsgReader_SaveMsgToFile(pMsgHdr, pFilename)
{
	// Sanity checking
	if (typeof(pMsgHdr) !== "object")
		return({ succeeded: false, errorMsg: "Header object not given"});
	if (typeof(pFilename) != "string")
		return({ succeeded: false, errorMsg: "Filename parameter not a string"});
	if (pFilename.length == 0)
		return({ succeeded: false, errorMsg: "Empty filename given"});

	var retObj = {
		succeeded: true,
		errorMsg: ""
	};

	// Get the message text and save it
	// Note: GetMsgInfoForEnhancedReader() can expand @-codes in the message,
	// but for now we're saving the message basically as-is.
	//var msgInfo = this.GetMsgInfoForEnhancedReader(pMsgHdr, false, false, false);
	var msgbase = new MsgBase(this.subBoardCode);
	if (msgbase.open())
	{
		var msgBody = msgbase.get_msg_body(false, pMsgHdr.number, false, false, true, true);
		msgbase.close();

		var messageSaveFile = new File(pFilename);
		if (messageSaveFile.open("w"))
		{
			// Write some header information to the file
			if (pMsgHdr.hasOwnProperty("from"))
				messageSaveFile.writeln("From: " + pMsgHdr.from);
			if (pMsgHdr.hasOwnProperty("to"))
				messageSaveFile.writeln("  To: " + pMsgHdr.to);
			if (pMsgHdr.hasOwnProperty("subject"))
				messageSaveFile.writeln("Subj: " + pMsgHdr.subject);
			/*
			if (pMsgHdr.hasOwnProperty("when_written_time"))
				messageSaveFile.writeln(strftime("Date: %Y-%m-%d %H:%M:%S", msgHeader.when_written_time));
			*/
			if (pMsgHdr.hasOwnProperty("date"))
				messageSaveFile.writeln("Date: " + pMsgHdr.date);
			if (pMsgHdr.hasOwnProperty("from_net_addr"))
				messageSaveFile.writeln("From net address: " + pMsgHdr.from_net_addr);
			if (pMsgHdr.hasOwnProperty("to_net_addr"))
				messageSaveFile.writeln("To net address: " + pMsgHdr.to_net_addr);
			if (pMsgHdr.hasOwnProperty("id"))
				messageSaveFile.writeln("ID: " + pMsgHdr.id);
			if (pMsgHdr.hasOwnProperty("reply_id"))
				messageSaveFile.writeln("Reply ID: " + pMsgHdr.reply_id);
			messageSaveFile.writeln("===============================");

			// If the message body has ANSI, then use the Graphic object to strip it
			// of any cursor movement codes
			var msgHasANSICodes = msgBody.indexOf("\x1b[") >= 0;
			if (msgHasANSICodes)
			{
				if (textHasDrawingChars(msgBody))
				{
					var graphic = new Graphic(this.msgAreaWidth, this.msgAreaHeight);
					graphic.auto_extend = true;
					graphic.ANSI = ansiterm.expand_ctrl_a(msgBody);
					msgBody = syncAttrCodesToANSI(graphic.MSG);
				}
				else
					msgBody = syncAttrCodesToANSI(msgBody);
			}

			// Write the message body to the file
			messageSaveFile.write(msgBody);
			messageSaveFile.close();
		}
		else
		{
			retObj.succeeded = false;
			retObj.errorMsg = "Failed to open the file for writing";
		}
	}
	else
	{
		retObj.succeeded = false;
		retObj.errorMsg = "Unable to open the messagebase";
	}

	return retObj;
}

// For the DigDistMsgReader class: Toggles whether a message has been 'selected'
// (i.e., for things like batch delete, etc.)
//
// Parameters:
//  pSubCode: The internal sub-board code of the message sub-board where the
//            message resides
//  pMsgIdx: The index of the message to toggle
//  pSelected: Optional boolean to explictly specify whether the message should
//             be selected.  If this is not provided (or is null), then this
//             message will simply toggle the selection state of the message.
function DigDistMsgReader_ToggleSelectedMessage(pSubCode, pMsgIdx, pSelected)
{
	// Sanity checking
	if (typeof(pSubCode) != "string") return;
	if (typeof(pMsgIdx) != "number") return;

	// If the 'selected message' object doesn't have the sub code index,
	// then add it.
	if (!this.selectedMessages.hasOwnProperty(pSubCode))
		this.selectedMessages[pSubCode] = {};

	// If pSelected is a boolean, then it specifies the specific selection
	// state of the message (true = selected, false = not selected).
	if (typeof(pSelected) == "boolean")
	{
		if (pSelected)
		{
			if (!this.selectedMessages[pSubCode].hasOwnProperty(pMsgIdx))
				this.selectedMessages[pSubCode][pMsgIdx] = true;
		}
		else
		{
			if (this.selectedMessages[pSubCode].hasOwnProperty(pMsgIdx))
				delete this.selectedMessages[pSubCode][pMsgIdx];
		}
	}
	else
	{
		// pSelected is not a boolean, so simply toggle the selection state of
		// the message.
		// If the object for the given sub-board code contains the message
		// index, then remove it.  Otherwise, add it.
		if (this.selectedMessages[pSubCode].hasOwnProperty(pMsgIdx))
			delete this.selectedMessages[pSubCode][pMsgIdx];
		else
			this.selectedMessages[pSubCode][pMsgIdx] = true;
	}
}

// For the DigDistMsgReader class: Returns whether a message (by sub-board code & index)
// is selected (i.e., for batch delete, etc.).
//
// Parameters:
//  pSubCode: The internal sub-board code of the message sub-board where the
//            message resides
//  pMsgIdx: The index of the message to toggle
//
// Return value: Boolean - Whether or not the given message has been selected
function DigDistMsgReader_MessageIsSelected(pSubCode, pMsgIdx)
{
	return (this.selectedMessages.hasOwnProperty(pSubCode) && this.selectedMessages[pSubCode].hasOwnProperty(pMsgIdx));
}

// For the DigDistMsgReader class: Checks to see if all selected messages can
// be deleted (i.e., whether the user has permission to delete all of them).
function DigDistMsgReader_AllSelectedMessagesCanBeDeleted()
{
	// If the user has sysop access, then they should be able to delete messages.
	if (user.is_sysop)
		return true;

	var userCanDeleteAllSelectedMessages = true;

	var msgBase = null;
	for (var subBoardCode in this.selectedMessages)
	{
		// If the current sub-board is personal mail, then the user can delete
		// those messages.  Otherwise, check the sub-board configuration to see
		// if the user can delete messages in the sub-board.
		if (subBoardCode != "mail")
		{
			msgBase = new MsgBase(subBoardCode);
			if (msgBase.open())
			{
				userCanDeleteAllSelectedMessages = userCanDeleteAllSelectedMessages && ((msgBase.cfg.settings & SUB_DEL) == SUB_DEL);
				msgBase.close();
			}
		}
		if (!userCanDeleteAllSelectedMessages)
			break;
	}
	
	return userCanDeleteAllSelectedMessages;
}

// For the DigDistMsgReader class: Marks the 'selected messages' (in
// this.selecteMessages) as deleted, or not deleted.
//
// Parameters:
//  pDelete: Boolean - Whether or not the message should be marked deleted.  Defaults to true.
//           If false, the message will be marked not deleted (if it is marked as deleted).
//
// Return value: An object with the following
// properties:
//  opSuccessful: Boolean - Whether or not all messages were successfully marked
//              for deletion
//  failureList: An object containing indexes of messages that failed to get
//               marked for deletion, indexed by internal sub-board code, then
//               containing messages indexes as properties.  Reasons for failing
//               to mark messages deleted can include the user not having permission
//               to delete in a sub-board, failure to open the sub-board, etc.
function DigDistMsgReader_DeleteOrUndeleteSelectedMessages(pDelete)
{
	var retObj = {
		opSuccessful: false,
		failureList: {}
	};

	var markAsDeleted = (typeof(pDelete) === "boolean" ? pDelete : true);

	var msgBase = null;
	var msgHdr = null;
	for (var subBoardCode in this.selectedMessages)
	{
		msgBase = new MsgBase(subBoardCode);
		if (msgBase.open())
		{
			// If deleting messages, then check whether the user is the sysop, they're
			// reading their personal mail, or the sub-board allows deleting messages.
			var canContinue = true;
			if (markAsDeleted)
				canContinue = (user.is_sysop || (subBoardCode == "mail") || ((msgBase.cfg.settings & SUB_DEL) == SUB_DEL));
			if (canContinue)
			{
				for (var msgIdx in this.selectedMessages[subBoardCode])
				{
					// It seems that msgIdx is a string, so make sure we have a
					// numeric version.
					var msgIdxNumber = +msgIdx;
					// If doing a search (this.msgSearchHdrs has the sub-board code),
					// then get the message header by index from there.  Otherwise,
					// use the message base object to get the message by index.
					if (this.msgSearchHdrs.hasOwnProperty(subBoardCode) &&
					    (this.msgSearchHdrs[subBoardCode].indexed.length > 0))
					{
						if ((msgIdxNumber >= 0) && (msgIdxNumber < this.msgSearchHdrs[subBoardCode].indexed.length))
							msgHdr = this.msgSearchHdrs[subBoardCode].indexed[msgIdxNumber];
					}
					else if (this.hdrsForCurrentSubBoard.length > 0)
					{
						if ((msgIdxNumber >= 0) && (msgIdxNumber < this.hdrsForCurrentSubBoard.length))
							msgHdr = this.hdrsForCurrentSubBoard[msgIdxNumber];
					}
					else
					{
						if ((msgIdxNumber >= 0) && (msgIdxNumber < msgBase.total_msgs))
							msgHdr = msgBase.get_msg_header(true, msgIdxNumber, false);
					}
					// If we got the message header, then mark it for deletion.
					// If the message header wasn't marked as deleted, then add
					// the message index to the return object.
					if (msgHdr != null)
					{
						if (markAsDeleted)
						{
							// remove_msg() just marks a message for deletion
							//retObj.opSuccessful = msgBase.remove_msg(true, msgHdr.offset);
							retObj.opSuccessful = msgBase.remove_msg(false, msgHdr.number);
						}
						else
						{
							// If the message is marked deleted, unmark it.
							if ((msgHdr.attr & MSG_DELETE) == MSG_DELETE)
							{
								var tmpMsgHdr = msgBase.get_msg_header(false, msgHdr.number, false);
								if (tmpMsgHdr != null)
								{
									tmpMsgHdr.attr = tmpMsgHdr.attr ^ MSG_DELETE;
									retObj.opSuccessful = msgBase.put_msg_header(false, msgHdr.number, tmpMsgHdr);
								}
								else
									retObj.opSuccessful = false;
							}
							else
								retObj.opSuccessful = true; // No change necessary
						}
					}
					else
						retObj.opSuccessful = false;
					if (retObj.opSuccessful)
					{
						// Refresh the message header in the header arrays (if it exists there) and
						// remove the message index from the selectedMessages object.  Also, delete
						// or undelete any vote response messages that may exist for this message.
						this.RefreshHdrInSavedArrays(msgIdxNumber, MSG_DELETE, markAsDeleted, subBoardCode);
						var voteDelRetObj = toggleVoteMsgsDeleted(msgBase, msgHdr.number, msgHdr.id, markAsDeleted, (subBoardCode == "mail"));
						if (!voteDelRetObj.allVoteMsgsAffected)
						{
							retObj.opSuccessful = false;
							if (!retObj.failureList.hasOwnProperty(subBoardCode))
								retObj.failureList[subBoardCode] = [];
							retObj.failureList[subBoardCode].push(msgIdxNumber);
						}
						// If deleting and the user can't view deleted messages, then remove the message from this.selectedMessages
						if (markAsDeleted && !canViewDeletedMsgs())
							delete this.selectedMessages[subBoardCode][msgIdx];
					}
					else
					{
						retObj.opSuccessful = false;
						if (!retObj.failureList.hasOwnProperty(subBoardCode))
							retObj.failureList[subBoardCode] = [];
						retObj.failureList[subBoardCode].push(msgIdxNumber);
					}
				}
				if (markAsDeleted)
				{
					// If the sub-board index array no longer has any properties (i.e.,
					// all messages in the sub-board were marked as deleted) and the user
					// can't view deleted messages, then remove the sub-board property from
					// this.selectedMessages
					if (Object.keys(this.selectedMessages[subBoardCode]).length == 0 && !canViewDeletedMsgs())
						delete this.selectedMessages[subBoardCode];
				}
			}
			else
			{
				if (markAsDeleted && !canContinue)
				{
					// The user doesn't have permission to delete messages
					// in this sub-board.
					// Create an entry in retObj.failureList indexed by the
					// sub-board code to indicate failure to delete all
					// messages in the sub-board.
					retObj.opSuccessful = false;
					retObj.failureList[subBoardCode] = [];
				}
			}

			msgBase.close();
		}
		else
		{
			// Failure to open the sub-board.
			// Create an entry in retObj.failureList indexed by the
			// sub-board code to indicate failure to delete all messages
			// in the sub-board.
			retObj.opSuccessful = false;
			retObj.failureList[subBoardCode] = [];
		}
	}

	return retObj;
}

// For the DigDistMsgReader class: Returns the number of selected messages
function DigDistMsgReader_NumSelectedMessages()
{
	var numSelectedMsgs = 0;

	for (var subBoardCode in this.selectedMessages)
		numSelectedMsgs += Object.keys(this.selectedMessages[subBoardCode]).length;

	return numSelectedMsgs;
}

// Allows the user to forward a message to an email address or
// another user.  This function is interactive with the user.
//
// Parameters:
//  pMsgHeader: The header of the message being forwarded
//  pMsgBody: The body text of the message
//
// Return value: A blank string on success or a string containing a
//               message upon failure.
function DigDistMsgReader_ForwardMessage(pMsgHdr, pMsgBody)
{
	if (typeof(pMsgHdr) != "object")
		return "Invalid message header given";

	var retStr = "";

	console.print("\x01n");
	console.crlf();
	console.print("\x01cUser name/number/email address\x01h:\x01n");
	console.crlf();
	var msgDest = console.getstr(console.screen_columns - 1, K_LINE);
	console.print("\x01n");
	console.crlf();
	if (msgDest.length > 0)
	{
		var tmpMsgbase = new MsgBase("mail");
		if (tmpMsgbase.open())
		{
			// If the given message body is not a string, then get the
			// message body from the messagebase.
			if (typeof(pMsgBody) != "string")
			{
				var msgbase = new MsgBase(this.subBoardCode);
				if (msgbase.open())
				{
					pMsgBody = msgbase.get_msg_body(false, pMsgHdr.number, false, false, true, true);
					msgbase.close();
				}
				else
					return "Unable to open the sub-board to get the message body";
			}

			// Prepend some lines to the message body to describe where
			// the message came from originally.
			var newMsgBody = "This is a forwarded message from " + system.name + "\n";
			newMsgBody += "Forwarded by: " + user.alias;
			if (user.alias != user.name)
				newMsgBody += " (" + user.name + ")";
			newMsgBody += "\n";
			if (this.subBoardCode == "mail")
				newMsgBody += "From " + user.name + "'s personal email\n";
			else
			{
				newMsgBody += "From sub-board: "
				           + msg_area.grp_list[msg_area.sub[this.subBoardCode].grp_index].description
				           + ", " + msg_area.sub[this.subBoardCode].description + "\n";
			}
			newMsgBody += "From: " + pMsgHdr.from + "\n";
			newMsgBody += "To: " + pMsgHdr.to + "\n";
			newMsgBody += "Subject: " + pMsgHdr.subject + "\n";
			newMsgBody += "==================================\n\n";
			newMsgBody += pMsgBody;

			// Ask whether to edit the message before forwarding it,
			// and use console.editfile(filename) to edit it.
			if (!console.noyes("Edit the message before sending"))
			{
				var baseWorkDir = system.node_dir + "DDMsgReader_Temp";
				deltree(baseWorkDir + "/");
				if (mkdir(baseWorkDir))
				{
					// TODO: Let the user edit the message, then read it
					// and set newMsgBody to it
					var tmpMsgFilename = baseWorkDir + "/message.txt";
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
				else
				{
					console.print("\x01n\x01cCouldn't create temporary directory\x01n");
					console.crlf();
					console.pause();
				}
			}
			// End New (editing message)

			// Create part of a header object which will be used when saving/sending
			// the message.  The destination ("to" informatoin) will be filled in
			// according to the destination type.
			var destMsgHdr = { to_net_type: NET_NONE, from: user.name,
							   replyto: user.name, subject: "Fwd: " + pMsgHdr.subject };
			if (user.netmail.length > 0)
			{
				destMsgHdr.replyto_net_addr = user.netmail;
			}
			else
			{
				destMsgHdr.replyto_net_addr = user.email;
			}
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

function printMsgHdrInfo(pMsgHdr)
{
	if (typeof(pMsgHdr) != "object")
		return;

	for (var prop in pMsgHdr)
	{
		if (prop == "to_net_type")
			print(prop + ": " + toNetTypeToStr(pMsgHdr[prop]));
		else
			console.print(prop + ": " + pMsgHdr[prop]);
		console.crlf();
	}
}

function toNetTypeToStr(toNetType)
{
	var toNetTypeStr = "Unknown";
	if (typeof(toNetType) == "number")
	{
		switch (toNetType)
		{
			case NET_NONE:
				toNetTypeStr = "Local";
				break;
			case NET_UNKNOWN:
				toNetTypeStr = "Unknown networked";
				break;
			case NET_FIDO:
				toNetTypeStr = "FidoNet";
				break;
			case NET_POSTLINK:
				toNetTypeStr = "PostLink";
				break;
			case NET_QWK:
				toNetTypeStr = "QWK";
				break;
			case NET_INTERNET:
				toNetTypeStr = "Internet";
				break;
			default:
				toNetTypeStr = "Unknown";
				break;
		}
	}
	return toNetTypeStr;
}

// For the DigDistMsgReader class: Lets the user vote on a message
//
// Parameters:
//  pMsgHdr: The header of the mesasge being voted on
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
function DigDistMsgReader_VoteOnMessage(pMsgHdr, pRemoveNLsFromVoteText)
{
	var retObj = {
		BBSHasVoteFunction: false,
		savedVote: false,
		userQuit: false,
		errorMsg: "",
		mnemonicsRequiredForErrorMsg: false,
		updatedHdr: null
	};

	// Don't allow voting for personal email
	if (this.subBoardCode == "mail")
	{
		retObj.errorMsg = "Can not vote on personal email";
		return retObj;
	}
	
	// Check whether the user has the voting restiction
	if ((user.security.restrictions & UFLAG_V) == UFLAG_V)
	{
		// Use the line from text.dat that says the user is not allowed to vote,
		// and remove newlines from it.
		retObj.errorMsg = "\x01n" + bbs.text(typeof(R_Voting) != "undefined" ? R_Voting : 781).replace("\r\n", "").replace("\n", "").replace("\N", "").replace("\r", "").replace("\R", "").replace("\R\n", "").replace("\r\N", "").replace("\R\N", "");
		return retObj;
	}

	var msgbase = new MsgBase(this.subBoardCode);
	if (!msgbase.open())
	{
		return retObj;
	}

	// If the message vote function is not defined in the running verison of Synchronet,
	// then just return.
	retObj.BBSHasVoteFunction = (typeof(msgbase.vote_msg) == "function");
	if (!retObj.BBSHasVoteFunction)
	{
		msgbase.close();
		return retObj;
	}

	var removeNLsFromVoteText = (typeof(pRemoveNLsFromVoteText) === "boolean" ? pRemoveNLsFromVoteText : false)

	// See if voting is allowed in the current sub-board
	if ((msg_area.sub[this.subBoardCode].settings & SUB_NOVOTING) == SUB_NOVOTING)
	{
		retObj.errorMsg = bbs.text(typeof(VotingNotAllowed) != "undefined" ? VotingNotAllowed : 779);
		if (removeNLsFromVoteText)
			retObj.errorMsg = retObj.errorMsg.replace("\r\n", "").replace("\n", "").replace("\N", "").replace("\r", "").replace("\R", "").replace("\R\n", "").replace("\r\N", "").replace("\R\N", "");
		retObj.mnemonicsRequiredForErrorMsg = true;
		msgbase.close();
		return retObj;
	}

	// If the message is a poll question and has the maximum number of votes
	// already or is closed for voting, then don't let the user vote on it.
	if ((pMsgHdr.attr & MSG_POLL) == MSG_POLL)
	{
		var userVotedMaxVotes = false;
		var numVotes = (pMsgHdr.hasOwnProperty("votes") ? pMsgHdr.votes : 0);
		if (typeof(msgbase.how_user_voted) === "function")
		{
			var votes = msgbase.how_user_voted(pMsgHdr.number, (msgbase.cfg.settings & SUB_NAME) == SUB_NAME ? user.name : user.alias);
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
			msgbase.close();
			return retObj;
		}
		else if (userVotedMaxVotes)
		{
			retObj.errorMsg = bbs.text(typeof(VotedAlready) != "undefined" ? VotedAlready : 780);
			if (removeNLsFromVoteText)
				retObj.errorMsg = retObj.errorMsg.replace("\r\n", "").replace("\n", "").replace("\N", "").replace("\r", "").replace("\R", "").replace("\R\n", "").replace("\r\N", "").replace("\R\N", "");
			retObj.mnemonicsRequiredForErrorMsg = true;
			msgbase.close();
			return retObj;
		}
	}

	// If the user has voted on this message already, then set an error message and return.
	if (this.HasUserVotedOnMsg(pMsgHdr.number))
	{
		retObj.errorMsg = bbs.text(typeof(VotedAlready) != "undefined" ? VotedAlready : 780);
		if (removeNLsFromVoteText)
			retObj.errorMsg = retObj.errorMsg.replace("\r\n", "").replace("\n", "").replace("\N", "").replace("\r", "").replace("\R", "").replace("\R\n", "").replace("\r\N", "").replace("\R\N", "");
		retObj.mnemonicsRequiredForErrorMsg = true;
		msgbase.close();
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
		from: (msgbase.cfg.settings & SUB_NAME) == SUB_NAME ? user.name : user.alias
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
			console.clear("\x01n");
			var selectHdr = bbs.text(typeof(BallotHdr) != "undefined" ? BallotHdr : 791);
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
			var userInputNum = 0;
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
								userInputNum = 0;
								for (var i = 0; i < voteNumbers.length; ++i)
									userInputNum |= (1 << (voteNumbers[i]-1));
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
				var selectPromptText = bbs.text(SelectItemWhich);
				selectPromptText = selectPromptText.replace(/%[uU]/, 1).replace(/%[dD]/, 1);
				console.mnemonics(selectPromptText);
				var maxNum = optionNum - 1;
				userInputNum = console.getnum(maxNum);
				if (userInputNum == -1) // The user chose Q to quit
					retObj.userQuit = true;
				console.print("\x01n");
			}
			if (!retObj.userQuit)
			{
				voteMsgHdr.attr = MSG_VOTE;
				voteMsgHdr.votes = userInputNum;
			}
		}
	}
	else
	{
		// The message is not a poll - Prompt for up/downvote
		if ((typeof(MSG_UPVOTE) != "undefined") && (typeof(MSG_DOWNVOTE) != "undefined"))
		{
			var voteAttr = 0;
			// Get text line 783 to prompt for voting
			var textDatText = bbs.text(typeof(VoteMsgUpDownOrQuit) != "undefined" ? VoteMsgUpDownOrQuit : 783);
			if (removeNLsFromVoteText)
				textDatText = textDatText.replace("\r\n", "").replace("\n", "").replace("\N", "").replace("\r", "").replace("\R", "").replace("\R\n", "").replace("\r\N", "").replace("\R\N", "");
			console.print("\x01n");
			console.mnemonics(textDatText);
			console.print("\x01n");
			// Using getAllowedKeyWithMode() instead of console.getkeys() so we
			// can control the input mode better, so it doesn't output a CRLF
			switch (getAllowedKeyWithMode("UDQ" + KEY_UP + KEY_DOWN, K_NOCRLF|K_NOSPIN))
			{
				case "U":
				case KEY_UP:
					voteAttr = MSG_UPVOTE;
					break;
				case "D":
				case KEY_DOWN:
					voteAttr = MSG_DOWNVOTE;
					break;
				case "Q":
				default:
					retObj.userQuit = true;
					break;
			}
			// If the user voted, then save the user's vote in the attr property
			// in the header
			if (voteAttr != 0)
				voteMsgHdr.attr = voteAttr;
		}
		else
			retObj.errorMsg = "MSG_UPVOTE & MSG_DOWNVOTE are not defined";
	}

	// If the user hasn't quit and there is no error message, then save the vote
	// message header
	if (!retObj.userQuit && (retObj.errorMsg.length == 0))
	{
		console.print("\x01n  Submitting..");
		retObj.savedVote = msgbase.vote_msg(voteMsgHdr);
		// If the save was successful, then update
		// this.hdrsForCurrentSubBoard with the updated
		// message header (for the message that was read)
		if (retObj.savedVote)
		{
			if (this.hdrsForCurrentSubBoardByMsgNum.hasOwnProperty(pMsgHdr.number))
			{
				var originalMsgIdx = this.hdrsForCurrentSubBoardByMsgNum[pMsgHdr.number];
				var tmpHdrs = msgbase.get_all_msg_headers(true);
				if (tmpHdrs.hasOwnProperty(pMsgHdr.number))
				{
					this.hdrsForCurrentSubBoard[originalMsgIdx] = tmpHdrs[pMsgHdr.number];
					// Originally, this script assigned retObj.updatedHdr as follows:
					//retObj.updatedHdr = pMsgHdr;
					// However, after an update, there were a couple errors that total_votes and upvotes
					// were read-only, so it wuldn't assign to them, so now we copy pMsgHdr this way:
					retObj.updatedHdr = {};
					for (var prop in pMsgHdr)
						retObj.updatedHdr[prop] = pMsgHdr[prop];
					if (this.hdrsForCurrentSubBoard[originalMsgIdx].hasOwnProperty("total_votes"))
						retObj.updatedHdr.total_votes = this.hdrsForCurrentSubBoard[originalMsgIdx].total_votes;
					if (this.hdrsForCurrentSubBoard[originalMsgIdx].hasOwnProperty("upvotes"))
						retObj.updatedHdr.upvotes = this.hdrsForCurrentSubBoard[originalMsgIdx].upvotes;
					if (this.hdrsForCurrentSubBoard[originalMsgIdx].hasOwnProperty("tally"))
						retObj.updatedHdr.tally = this.hdrsForCurrentSubBoard[originalMsgIdx].tally;
				}
			}
		}
		else
		{
			// Failed to save the vote
			retObj.errorMsg = "Failed to save your vote";
		}
	}

	msgbase.close();

	return retObj;
}

// For the DigDistMsgReader class: Checks to see whether a user has voted on a message.
// The message must belong to the currently-open sub-board.
//
// Parameters:
//  pMsgNum: The message number
//  pUser: Optional - A user account to check.  If omitted, the current logged-in
//         user will be used.
function DigDistMsgReader_HasUserVotedOnMsg(pMsgNum, pUser)
{
	// Don't do this for personal email
	if (this.subBoardCode == "mail")
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
	var msgbase = new MsgBase(this.subBoardCode);
	if (msgbase.open())
	{
		if (typeof(msgbase.how_user_voted) === "function")
		{
			var votes = 0;
			if (typeof(pUser) == "object")
				votes = msgbase.how_user_voted(pMsgNum, (msgbase.cfg.settings & SUB_NAME) == SUB_NAME ? pUser.name : pUser.alias);
			else
				votes = msgbase.how_user_voted(pMsgNum, (msgbase.cfg.settings & SUB_NAME) == SUB_NAME ? user.name : user.alias);
			userHasVotedOnMsg = (votes > 0);
		}
		msgbase.close();
	}
	return userHasVotedOnMsg;
}

// Gets information about the upvotes and downvotes for a message.
// If the user is a sysop, this will also get who voted on the message.
//
// Parameters:
//  pMsgHdr: A header of a message that has upvotes & downvotes
//
// Return value: An array of strings containing information about the upvotes,
//               downvotes, tally, and (if the user is a sysop) who submitted
//               votes on the message.
function DigDistMsgReader_GetUpvoteAndDownvoteInfo(pMsgHdr)
{
	// If the message header doesn't have the "total_votes" or "upvotes" properties,
	// then there's no vote information, so just return an empty array.
	if (!pMsgHdr.hasOwnProperty("total_votes") || !pMsgHdr.hasOwnProperty("upvotes"))
		return [];

	var msgVoteInfo = getMsgUpDownvotesAndScore(pMsgHdr);
	var voteInfo = [];
	voteInfo.push("Upvotes: " + msgVoteInfo.upvotes);
	voteInfo.push("Downvotes: " + msgVoteInfo.downvotes);
	voteInfo.push("Score: " + msgVoteInfo.voteScore);
	if (pMsgHdr.hasOwnProperty("tally"))
		voteInfo.push("Tally: " + pMsgHdr.tally);

	// If the user is the sysop, then also add the names of people who
	// voted on the message.
	if (user.is_sysop)
	{
		// Check all the messages in the messagebase after the current one
		// to find response messages
		var msgbase = new MsgBase(this.subBoardCode);
		if (msgbase.open())
		{
			// Pass true to get_all_msg_headers() to tell it to return vote messages
			// (the parameter was introduced in Synchronet 3.17+)
			var tmpHdrs = msgbase.get_all_msg_headers(true);
			for (var tmpProp in tmpHdrs)
			{
				if (tmpHdrs[tmpProp] == null)
					continue;
				// If this header's thread_back or reply_id matches the poll message
				// number, then append the 'user voted' string to the message body.
				if ((tmpHdrs[tmpProp].thread_back == pMsgHdr.number) || (tmpHdrs[tmpProp].reply_id == pMsgHdr.id))
				{
					var tmpMessageBody = msgbase.get_msg_body(false, tmpHdrs[tmpProp].number, false, false, true, true);
					if ((tmpHdrs[tmpProp].field_list.length == 0) && (tmpMessageBody.length == 0))
					{
						var msgWrittenLocalTime = msgWrittenTimeToLocalBBSTime(tmpHdrs[tmpProp]);
						var voteDate = strftime("%a %b %d %Y %H:%M:%S", msgWrittenLocalTime);
						voteInfo.push("\x01n\x01c\x01h" + tmpHdrs[tmpProp].from + "\x01n\x01c voted on this message on " + voteDate + "\x01n");
					}
				}
			}
			msgbase.close();
		}
	}

	return voteInfo;
}

// For the DigDistMsgReader class: Gets the body (text) of a message.  If it's
// a poll, this method will format the message body with poll results.  Otherwise,
// this method will simply get the message body.
//
// Parameters:
//  pMsgHeader: The message header
//
// Return value: The poll results, colorized.  If the message is not a
//               poll message, then an empty string will be returned.
function DigDistMsgReader_GetMsgBody(pMsgHdr)
{
	var msgbase = new MsgBase(this.subBoardCode);
	if (!msgbase.open())
		return "";
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
			var unvotedOptionFormatStr = "\x01n\x01c\x01h%2d\x01n\x01c: \x01w\x01h%-" + voteOptDescLen + "s [%-4d %6.2f%]\x01n";
			var votedOptionFormatStr = "\x01n\x01c\x01h%2d\x01n\x01c: \x01" + "5\x01w\x01h%-" + voteOptDescLen + "s [%-4d %6.2f%]\x01n";
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
			var votingAllowed = ((this.subBoardCode != "mail") && (((msg_area.sub[this.subBoardCode].settings & SUB_NOVOTING) == 0)));
			if (votingAllowed && !this.HasUserVotedOnMsg(pMsgHdr.number))
				msgBody += "\x01n\r\n\x01gTo vote in this poll, press \x01w\x01h" + this.enhReaderKeys.vote + "\x01n\x01g now.\r\n";

			// If the current logged-in user created this poll, then show the
			// users who have voted on it so far.
			var msgFromUpper = pMsgHdr.from.toUpperCase();
			if ((msgFromUpper == user.name.toUpperCase()) || (msgFromUpper == user.handle.toUpperCase()))
			{
				// Check all the messages in the messagebase after the current one
				// to find poll response messages
				// Get the line from text.dat for writing who voted & when.  It
				// is a format string and should look something like this:
				//"\r\n\x01n\x01hOn %s, in \x01c%s \x01n\x01c%s\r\n\x01h\x01m%s voted in your poll: \x01n\x01h%s\r\n" 787 PollVoteNotice
				var userVotedInYourPollText = bbs.text(typeof(PollVoteNotice) != "undefined" ? PollVoteNotice : 787);

				// Pass true to get_all_msg_headers() to tell it to return vote messages
				// (the parameter was introduced in Synchronet 3.17+)
				var tmpHdrs = msgbase.get_all_msg_headers(true);
				for (var tmpProp in tmpHdrs)
				{
					if (tmpHdrs[tmpProp] == null)
						continue;
					// If this header's thread_back or reply_id matches the poll message
					// number, then append the 'user voted' string to the message body.
					if ((tmpHdrs[tmpProp].thread_back == pMsgHdr.number) || (tmpHdrs[tmpProp].reply_id == pMsgHdr.id))
					{
						var msgWrittenLocalTime = msgWrittenTimeToLocalBBSTime(tmpHdrs[tmpProp]);
						var voteDate = strftime("%a %b %d %Y %H:%M:%S", msgWrittenLocalTime);
						var grpName = "";
						var msgbaseCfgName = "";
						var msgbase = new MsgBase(this.subBoardCode);
						if (msgbase.open())
						{
							grpName = msgbase.cfg.grp_name;
							msgbaseCfgName = msgbase.cfg.name;
							msgbase.close();
						}
						msgBody += format(userVotedInYourPollText, voteDate, grpName, msgbaseCfgName, tmpHdrs[tmpProp].from, pMsgHdr.subject);
					}
				}
			}
		}
	}
	else
	{
		// If the message is UTF8 and the terminal is not UTF8-capable, then convert
		// the text to cp437.
		msgBody = msgbase.get_msg_body(false, pMsgHdr.number, false, false, true, true);
		if (pMsgHdr.hasOwnProperty("is_utf8") && pMsgHdr.is_utf8)
		{
			var userConsoleSupportsUTF8 = false;
			if (typeof(USER_UTF8) != "undefined")
				userConsoleSupportsUTF8 = console.term_supports(USER_UTF8);
			if (!userConsoleSupportsUTF8)
				msgBody = utf8_cp437(msgBody);
		}
		// Remove any initial coloring from the message body, which can color the whole message
		msgBody = removeInitialColorFromMsgBody(msgBody);
		// For HTML-formatted messages, convert HTML entities
		if (pMsgHdr.hasOwnProperty("text_subtype") && pMsgHdr.text_subtype.toLowerCase() == "html")
		{
			msgBody = html2asc(msgBody);
			// Remove excessive blank lines after HTML-translation
			msgBody = msgBody.replace(/\r\n\r\n\r\n/g, '\r\n\r\n');
		}
	}
	msgbase.close();

	// Remove any Synchronet pause codes that might exist in the message
	msgBody = msgBody.replace("\x01p", "").replace("\x01P", "");

	// If the user is a sysop, this is a moderated message area, and the message
	// hasn't been validated, then prepend the message with a message to let the
	// sysop now know to validate it.
	if (this.subBoardCode != "mail")
	{
		if (user.is_sysop && msg_area.sub[this.subBoardCode].is_moderated && ((pMsgHdr.attr & MSG_VALIDATED) == 0))
		{
			var validateNotice = "\x01n\x01h\x01yThis is an unvalidated message in a moderated area.  Press "
							   + this.enhReaderKeys.validateMsg + " to validate it.\r\n\x01g";
			for (var i = 0; i < 79; ++i)
				validateNotice += HORIZONTAL_SINGLE;
			validateNotice += "\x01n\r\n";
			msgBody = validateNotice + msgBody;
		}
	}

	return msgBody;
}

// For the DigDistMsgReader class: Refreshes a message header in one of the
// internal message arrays.
//
// Parameters:
//  pMsgNum: The number of the message to replace
function DigDistMsgReader_RefreshMsgHdrInArrays(pMsgNum)
{
	var msgbase = new MsgBase(this.subBoardCode);
	if (!msgbase.open())
		return;
	if (this.msgSearchHdrs.hasOwnProperty(this.subBoardCode) &&
	    (this.msgSearchHdrs[this.subBoardCode].indexed.length > 0))
	{
		for (var i = 0; i < this.msgSearchHdrs[this.subBoardCode].indexed.length; ++i)
		{
			if (this.msgSearchHdrs[this.subBoardCode].indexed[i].number == pMsgNum)
			{
				var newMsgHdr = msgbase.get_msg_header(false, pMsgNum, true);
				if (newMsgHdr != null)
					this.msgSearchHdrs[this.subBoardCode].indexed[i] = newMsgHdr;
				break;
			}
		}
	}
	else if (this.hdrsForCurrentSubBoard.length > 0)
	{
		if (this.hdrsForCurrentSubBoardByMsgNum.hasOwnProperty(pMsgNum))
		{
			var msgHdrs = msgbase.get_all_msg_headers(true);
			if (msgHdrs.hasOwnProperty(pMsgNum))
			{
				var msgIdx = this.hdrsForCurrentSubBoardByMsgNum[pMsgNum];
				this.hdrsForCurrentSubBoard[msgIdx] = msgHdrs[pMsgNum];
			}
		}
	}
	msgbase.close();
}

// For the DigDistMessageReader class: Re-calculates the message list widths and
// format strings
//
// Parameters:
//  pMsgNumLen: Optional - Length to use for the message number field.  If not specified,
//              then this will get the number of messages in the sub-board and use that
//              length.
function DigDistMsgReader_RecalcMsgListWidthsAndFormatStrs(pMsgNumLen)
{
	// Note: Constructing these strings must be done after reading the configuration
	// file in order for the configured colors to be used

	this.sMsgListHdrFormatStr = "";
	this.sMsgInfoFormatStr = "";
	this.sMsgInfoToUserFormatStr = "";
	this.sMsgInfoFromUserFormatStr = "";
	this.sMsgInfoFormatHighlightStr = "";

	this.MSGNUM_LEN = (typeof(pMsgNumLen) == "number" ? pMsgNumLen : this.NumMessages(null, true).toString().length);
	if (this.MSGNUM_LEN < 4)
		this.MSGNUM_LEN = 4;
	this.DATE_LEN = 10; // i.e., YYYY-MM-DD
	this.TIME_LEN = 8;  // i.e., HH:MM:SS
	// Variable field widths: From, to, and subject
	this.FROM_LEN = (console.screen_columns * (15/console.screen_columns)).toFixed(0);
	this.TO_LEN = (console.screen_columns * (15/console.screen_columns)).toFixed(0);
	//var colsLeftForSubject = console.screen_columns-this.MSGNUM_LEN-this.DATE_LEN-this.TIME_LEN-this.FROM_LEN-this.TO_LEN-6; // 6 to account for the spaces
	//this.SUBJ_LEN = (console.screen_columns * (colsLeftForSubject/console.screen_columns)).toFixed(0);
	this.SUBJ_LEN = console.screen_columns-this.MSGNUM_LEN-this.DATE_LEN-this.TIME_LEN-this.FROM_LEN-this.TO_LEN-6; // 6 to account for the spaces

	if (this.showScoresInMsgList)
	{
		this.SUBJ_LEN -= (this.SCORE_LEN + 1);
		this.sMsgListHdrFormatStr = "%" + this.MSGNUM_LEN + "s %-" + this.FROM_LEN + "s %-"
		                          + this.TO_LEN + "s %-" + this.SUBJ_LEN + "s %"
		                          + this.SCORE_LEN + "s %-" + this.DATE_LEN + "s %-"
		                          + this.TIME_LEN + "s";

		this.sMsgInfoFormatStr = this.colors.msgListMsgNumColor + "%" + this.MSGNUM_LEN + "d%s"
		                       + this.colors.msgListFromColor + "%-" + this.FROM_LEN + "s "
		                       + this.colors.msgListToColor + "%-" + this.TO_LEN + "s "
		                       + this.colors.msgListSubjectColor + "%-" + this.SUBJ_LEN + "s "
		                       + this.colors.msgListScoreColor + "%" + this.SCORE_LEN + "d "
		                       + this.colors.msgListDateColor + "%-" + this.DATE_LEN + "s "
		                       + this.colors.msgListTimeColor + "%-" + this.TIME_LEN + "s";
		// Message information format string with colors to use when the message is
		// written to the user.
		this.sMsgInfoToUserFormatStr = this.colors.msgListToUserMsgNumColor + "%" + this.MSGNUM_LEN + "d%s"
		                             + this.colors.msgListToUserFromColor
		                             + "%-" + this.FROM_LEN + "s " + this.colors.msgListToUserToColor + "%-"
		                             + this.TO_LEN + "s " + this.colors.msgListToUserSubjectColor + "%-"
		                             + this.SUBJ_LEN + "s " + this.colors.msgListToUserScoreColor + "%"
		                             + this.SCORE_LEN + "d " + this.colors.msgListToUserDateColor
		                             + "%-" + this.DATE_LEN + "s " + this.colors.msgListToUserTimeColor
		                             + "%-" + this.TIME_LEN + "s";
		// Message information format string with colors to use when the message is
		// from the user.
		this.sMsgInfoFromUserFormatStr = this.colors.msgListFromUserMsgNumColor + "%" + this.MSGNUM_LEN + "d%s"
		                               + this.colors.msgListFromUserFromColor
		                               + "%-" + this.FROM_LEN + "s " + this.colors.msgListFromUserToColor + "%-"
		                               + this.TO_LEN + "s " + this.colors.msgListFromUserSubjectColor + "%-"
		                               + this.SUBJ_LEN + "s " + this.colors.msgListFromUserScoreColor + "%"
		                               + this.SCORE_LEN + "d " + this.colors.msgListFromUserDateColor
		                               + "%-" + this.DATE_LEN + "s " + this.colors.msgListFromUserTimeColor
		                               + "%-" + this.TIME_LEN + "s";
		// Highlighted message information line for the message list (used for the
		// lightbar interface)
		this.sMsgInfoFormatHighlightStr = this.colors.msgListMsgNumHighlightColor
		                                + "%" + this.MSGNUM_LEN + "d%s"
		                                + this.colors.msgListFromHighlightColor + "%-" + this.FROM_LEN
		                                + "s " + this.colors.msgListToHighlightColor + "%-" + this.TO_LEN + "s "
		                                + this.colors.msgListSubjHighlightColor + "%-" + this.SUBJ_LEN + "s "
		                                + this.colors.msgListScoreHighlightColor + "%" + this.SCORE_LEN + "d "
		                                + this.colors.msgListDateHighlightColor + "%-" + this.DATE_LEN + "s "
		                                + this.colors.msgListTimeHighlightColor + "%-" + this.TIME_LEN + "s";
	}
	else
	{
		this.sMsgListHdrFormatStr = "%" + this.MSGNUM_LEN + "s %-" + this.FROM_LEN + "s %-"
		                          + this.TO_LEN + "s %-" + this.SUBJ_LEN + "s %-"
		                          + this.DATE_LEN + "s %-" + this.TIME_LEN + "s";

		this.sMsgInfoFormatStr = this.colors.msgListMsgNumColor + "%" + this.MSGNUM_LEN + "d%s"
		                       + this.colors.msgListFromColor + "%-" + this.FROM_LEN + "s "
		                       + this.colors.msgListToColor + "%-" + this.TO_LEN + "s "
		                       + this.colors.msgListSubjectColor + "%-" + this.SUBJ_LEN + "s "
		                       + this.colors.msgListDateColor + "%-" + this.DATE_LEN + "s "
		                       + this.colors.msgListTimeColor + "%-" + this.TIME_LEN + "s";
		// Message information format string with colors to use when the message is
		// written to the user.
		this.sMsgInfoToUserFormatStr = this.colors.msgListToUserMsgNumColor + "%" + this.MSGNUM_LEN + "d%s"
		                             + this.colors.msgListToUserFromColor
		                             + "%-" + this.FROM_LEN + "s " + this.colors.msgListToUserToColor + "%-"
		                             + this.TO_LEN + "s " + this.colors.msgListToUserSubjectColor + "%-"
		                             + this.SUBJ_LEN + "s " + this.colors.msgListToUserDateColor
		                             + "%-" + this.DATE_LEN + "s " + this.colors.msgListToUserTimeColor
		                             + "%-" + this.TIME_LEN + "s";
		// Message information format string with colors to use when the message is
		// from the user.
		this.sMsgInfoFromUserFormatStr = this.colors.msgListFromUserMsgNumColor + "%" + this.MSGNUM_LEN + "d%s"
		                               + this.colors.msgListFromUserFromColor
		                               + "%-" + this.FROM_LEN + "s " + this.colors.msgListFromUserToColor + "%-"
		                               + this.TO_LEN + "s " + this.colors.msgListFromUserSubjectColor + "%-"
		                               + this.SUBJ_LEN + "s " + this.colors.msgListFromUserDateColor
		                               + "%-" + this.DATE_LEN + "s " + this.colors.msgListFromUserTimeColor
		                               + "%-" + this.TIME_LEN + "s";
		// Highlighted message information line for the message list (used for the
		// lightbar interface)
		this.sMsgInfoFormatHighlightStr = this.colors.msgListMsgNumHighlightColor
		                                + "%" + this.MSGNUM_LEN + "d%s"
		                                + this.colors.msgListFromHighlightColor + "%-" + this.FROM_LEN
		                                + "s " + this.colors.msgListToHighlightColor + "%-" + this.TO_LEN + "s "
		                                + this.colors.msgListSubjHighlightColor + "%-" + this.SUBJ_LEN + "s "
		                                + this.colors.msgListDateHighlightColor + "%-" + this.DATE_LEN + "s "
		                                + this.colors.msgListTimeHighlightColor + "%-" + this.TIME_LEN + "s";
	}

	// If the user's terminal doesn't support ANSI, then append a newline to
	// the end of the header format string (we won't be able to move the cursor).
	if (!canDoHighASCIIAndANSI())
		this.sMsgListHdrFormatStr += "\r\n";
}

// For the DigDistMessageReader class: Writes a temporary error message at the key help line
// for lightbar mode.
//
// Parameters:
//  pErrorMsg: The error message to write
//  pHelpLineRefreshDef: Optional - Specifies which help line to refresh on the screen
//                       (i.e., REFRESH_MSG_AREA_CHG_LIGHTBAR_HELP_LINE)
function DigDistMsgReader_WriteLightbarKeyHelpErrorMsg(pErrorMsg, pLineRefreshDef)
{
	console.gotoxy(1, console.screen_rows);
	console.cleartoeol("\x01n");
	console.gotoxy(1, console.screen_rows);
	console.print("\x01y\x01h" + pErrorMsg + "\x01n");
	mswait(ERROR_WAIT_MS);
	var helpLineRefreshDef = (typeof(pHelpLineRefreshDef) == "number" ? pHelpLineRefreshDef : -1);
	if (helpLineRefreshDef == REFRESH_MSG_AREA_CHG_LIGHTBAR_HELP_LINE)
		this.WriteChgMsgAreaKeysHelpLine();
}

// For the scrollable reader interface: Refreshes a rectangular region on the screen
// by printing part of the message text.
//
//  pTxtLines: The array of text lines of the message being displayed
//  pTopLineIdx: The index of the text line currently at the top row in the reader area
//  pTopLeftX: The upper-left corner column of the rectangle to be refreshed (1-based) - Absolute screen coordinate
//  pTopLeftY: The upper-left corner row of the rectangle to be refreshed (1-based) - Absolute screen coordinate
//  pWidth: The width of the rectangle to be refreshed
//  pHeight: The height of the rectangle to be refreshed
function DigDistMsgReader_RefreshMsgAreaRectangle(pTxtLines, pTopLineIdx, pTopLeftX, pTopLeftY, pWidth, pHeight)
{
	if (typeof(pTxtLines) !== "object")
		return;
	if (typeof(pTopLineIdx) !== "number" || pTopLineIdx < 0 || pTopLineIdx >= pTxtLines.length)
		return;
	if (typeof(pTopLeftX) !== "number" || pTopLeftX < 1 || pTopLeftX > console.screen_columns)
		return;
	if (typeof(pTopLeftY) !== "number" || pTopLeftY < 1 || pTopLeftY > console.screen_rows)
		return;
	if (typeof(pWidth) !== "number" || pWidth <= 0 || typeof(pHeight) !== "number" || pHeight <= 0)
		return;

	var firstTxtLineIdx = pTopLeftY - pTopLineIdx - 1;
	if (firstTxtLineIdx < 0) firstTxtLineIdx = 0; // Shouldn't happen, but just in case
	//this.msgAreaLeft = 1;
	//this.msgAreaRight = console.screen_columns - 1;
	//this.msgAreaWidth = this.msgAreaRight - this.msgAreaLeft + 1;
	//this.msgAreaHeight = this.msgAreaBottom - this.msgAreaTop + 1;
	// Sanity checking
	if (pTopLeftY < this.msgAreaTop)
	{
		var diff = this.msgAreaTop - pTopLeftY;
		pTopLeftY = this.msgAreaTop;
		pTopLineIdx += diff;
	}
	var lastScreenRow = pTopLeftY + pHeight - 1; // Inclusive, for loop
	if (lastScreenRow > this.msgAreaBottom)
		lastScreenRow = this.msgAreaBottom;
	if (pTopLeftX + pWidth > this.msgAreaRight)
		pWidth = this.msgAreaRight - pTopLeftX + 1;

	// Print the parts of the text lines that make up the rectangle
	var txtLineIdx = pTopLineIdx + (pTopLeftY-this.msgAreaTop);
	if (txtLineIdx < 0) txtLineIdx = 0; // Just in case, but shouldn't happen
	var txtLineStartIdx = pTopLeftX - this.msgAreaLeft; // Within each text line (it seemed right to subtract 1 but it wasn't)
	if (txtLineStartIdx < 0) txtLineStartIdx = 0; // Just in case, but shouldn't happen
	var emptyFormatStr = "\x01n%" + pWidth + "s"; // For printing empty strings after printing all text lines
	console.print("\x01n");
	for (var screenRow = pTopLeftY; screenRow <= lastScreenRow; ++screenRow)
	{
		console.gotoxy(pTopLeftX, screenRow);
		// If the current text line index is within the array of text lines, then output the section of the
		// text line.  Otherwise, output an empty string.
		if (txtLineIdx < pTxtLines.length)
		{
			// Get the text attributes up to the current point and output them
			console.print(getAllEditLineAttrsUntilLineIdx(pTxtLines, txtLineIdx, true, txtLineStartIdx));
			// Get the section of line (and make sure it can fill the needed width), and print it
			// Note: substrWithAttrCodes(pStr, pStartIdx, pLen) is defined in dd_lightbar_menu.js
			var lineText = substrWithAttrCodes(pTxtLines[txtLineIdx], txtLineStartIdx, pWidth);
			var printableTxtLen = console.strlen(lineText);
			if (printableTxtLen < pWidth)
			{
				var lenDiff = pWidth - printableTxtLen;
				lineText += format("\x01n%" + lenDiff + "s", "");
			}
			console.print(lineText);
		}
		else // We've printed all the remaining text lines, so now print an empty string.
			printf(emptyFormatStr, "");

		++txtLineIdx;
	}
}

// For the scrollable reader interface: Returns whether a 'from' or 'to' name in a message header
// is in the user's personal twitlist.
//
// Parameters:
//  pMsgHdr: A message header to check
//
// Return value: Boolean - Whether or not the header 'from' or 'to' name is in the user's personal twitlist
function DigDistMsgReader_MsgHdrFromOrToInUserTwitlist(pMsgHdr)
{
	if (pMsgHdr == null || typeof(pMsgHdr) !== "object")
		return false;
	if (!pMsgHdr.hasOwnProperty("from") && !pMsgHdr.hasOwnProperty("to"))
		return false;

	// The names in the user's twitlist have been converted to lowercase for case-insensitive matching.
	var fromLower = pMsgHdr.from.toLowerCase();
	var toLower = pMsgHdr.to.toLowerCase();
	var hdrNamesInTwitlist = false;
	for (var i = 0; i < this.userSettings.twitList.length && !hdrNamesInTwitlist; ++i)
		hdrNamesInTwitlist = (this.userSettings.twitList[i] == fromLower || this.userSettings.twitList[i] == toLower);
	return hdrNamesInTwitlist;
}

///////////////////////////////////////////////////////////////////////////////////
// Helper functions

// Displays the program information.
function DisplayProgramInfo()
{
	displayTextWithLineBelow("Digital Distortion Message Reader", true, "\x01n\x01c\x01h", "\x01k\x01h")
	console.center("\x01n\x01cVersion \x01g" + READER_VERSION + " \x01w\x01h(\x01b" + READER_DATE + "\x01w)");
	console.crlf();
}

// This function returns an array of default colors used in the
// DigDistMessageReader class.
function getDefaultColors()
{
	return {
		// Colors for the message header displayed above messages in the scrollable reader mode
		msgHdrMsgNumColor: "\x01n\x01b\x01h", // Message #
		msgHdrFromColor: "\x01n\x01b\x01h",   // From username
		msgHdrToColor: "\x01n\x01b\x01h",     // To username
		msgHdrToUserColor: "\x01n\x01g\x01h", // To username when it's to the current user
		msgHdrSubjColor: "\x01n\x01b\x01h",   // Message subject
		msgHdrDateColor: "\x01n\x01b\x01h",   // Message date

		// Message list header line: "Current msg group:"
		msgListHeaderMsgGroupTextColor: "\x01n\x01" + "4\x01c", 	// Normal cyan on blue background
		//	msgListHeaderMsgGroupTextColor: "\x01n\x01" + "4\x01w", 	// Normal white on blue background

		// Message list header line: Message group name
		msgListHeaderMsgGroupNameColor: "\x01h\x01c", 	// High cyan
		//	msgListHeaderMsgGroupNameColor: "\x01h\x01w", 	// High white

		// Message list header line: "Current sub-board:"
		msgListHeaderSubBoardTextColor: "\x01n\x01" + "4\x01c", 	// Normal cyan on blue background
		//	msgListHeaderSubBoardTextColor: "\x01n\x01" + "4\x01w", 	// Normal white on blue background

		// Message list header line: Message sub-board name
		msgListHeaderMsgSubBoardName: "\x01h\x01c", 	// High cyan
		//	msgListHeaderMsgSubBoardName: "\x01h\x01w", 	// High white
		// Line with column headers
		//	msgListColHeader: "\x01h\x01w", 	// High white (keep blue background)
		msgListColHeader: "\x01n\x01h\x01w", 	// High white on black background
		//	msgListColHeader: "\x01h\x01c", 	// High cyan (keep blue background)
		//	msgListColHeader: "\x01" + "4\x01h\x01y", 	// High yellow (keep blue background)

		// Message list information
		msgListMsgNumColor: "\x01n\x01h\x01y",
		msgListFromColor: "\x01n\x01c",
		msgListToColor: "\x01n\x01c",
		msgListSubjectColor: "\x01n\x01c",
		msgListScoreColor: "\x01n\x01c",
		msgListDateColor: "\x01h\x01b",
		msgListTimeColor: "\x01h\x01b",
		// Message information for messages written to the user
		msgListToUserMsgNumColor: "\x01n\x01h\x01y",
		msgListToUserFromColor: "\x01h\x01g",
		msgListToUserToColor: "\x01h\x01g",
		msgListToUserSubjectColor: "\x01h\x01g",
		msgListToUserScoreColor: "\x01h\x01g",
		msgListToUserDateColor: "\x01h\x01b",
		msgListToUserTimeColor: "\x01h\x01b",
		// Message information for messages from the user
		msgListFromUserMsgNumColor: "\x01n\x01h\x01y",
		msgListFromUserFromColor: "\x01n\x01c",
		msgListFromUserToColor: "\x01n\x01c",
		msgListFromUserSubjectColor: "\x01n\x01c",
		msgListFromUserScoreColor: "\x01n\x01c",
		msgListFromUserDateColor: "\x01h\x01b",
		msgListFromUserTimeColor: "\x01h\x01b",

		// Message list highlight colors
		msgListHighlightBkgColor: "\x01" + "4", 	// Background
		msgListMsgNumHighlightColor: "\x01h\x01y",
		msgListFromHighlightColor: "\x01h\x01c",
		msgListToHighlightColor: "\x01h\x01c",
		msgListSubjHighlightColor: "\x01h\x01c",
		msgListScoreHighlightColor: "\x01h\x01c",
		msgListDateHighlightColor: "\x01h\x01w",
		msgListTimeHighlightColor: "\x01h\x01w",

		// Lightbar message list help line colors
		lightbarMsgListHelpLineBkgColor: "\x01" + "7", 	// Background
		lightbarMsgListHelpLineGeneralColor: "\x01b",
		lightbarMsgListHelpLineHotkeyColor: "\x01r",
		lightbarMsgListHelpLineParenColor: "\x01m",

		// Continue prompt colors
		tradInterfaceContPromptMainColor: "\x01n\x01g", 	// Main text color
		tradInterfaceContPromptHotkeyColor: "\x01h\x01c", 	// Hotkey color
		tradInterfaceContPromptUserInputColor: "\x01h\x01g", 	// User input color

		// Message body color
		msgBodyColor: "\x01n\x01w",

		// Read message confirmation colors
		readMsgConfirmColor: "\x01n\x01c",
		readMsgConfirmNumberColor: "\x01h\x01c",
		// Prompt for continuing to list messages after reading a message
		afterReadMsg_ListMorePromptColor: "\x01n\x01c",

		// Help screen text color
		tradInterfaceHelpScreenColor: "\x01n\x01h\x01w",

		// Colors for choosing a message group & sub-board
		areaChooserMsgAreaNumColor: "\x01n\x01w\x01h",
		areaChooserMsgAreaDescColor: "\x01n\x01c",
		areaChooserMsgAreaNumItemsColor: "\x01b\x01h",
		areaChooserMsgAreaHeaderColor: "\x01n\x01y\x01h",
		areaChooserSubBoardHeaderColor: "\x01n\x01g",
		areaChooserMsgAreaMarkColor: "\x01g\x01h",
		areaChooserMsgAreaLatestDateColor: "\x01n\x01g",
		areaChooserMsgAreaLatestTimeColor: "\x01n\x01m",
		// Highlighted colors (for lightbar mode)
		areaChooserMsgAreaBkgHighlightColor: "\x01" + "4", 	// Blue background
		areaChooserMsgAreaNumHighlightColor: "\x01w\x01h",
		areaChooserMsgAreaDescHighlightColor: "\x01c",
		areaChooserMsgAreaDateHighlightColor: "\x01w\x01h",
		areaChooserMsgAreaTimeHighlightColor: "\x01w\x01h",
		areaChooserMsgAreaNumItemsHighlightColor: "\x01w\x01h",
		// Lightbar area chooser help line
		lightbarAreaChooserHelpLineBkgColor: "\x01" + "7", 	// Background
		lightbarAreaChooserHelpLineGeneralColor: "\x01b",
		lightbarAreaChooserHelpLineHotkeyColor: "\x01r",
		lightbarAreaChooserHelpLineParenColor: "\x01m",

		// Scrollbar background and scroll block colors (for the enhanced
		// message reader interface)
		scrollbarBGColor: "\x01n\x01h\x01k",
		scrollbarScrollBlockColor: "\x01n\x01h\x01w",
		// Color for the line drawn in the 2nd to last line of the message
		// area in the enhanced reader mode before a prompt
		enhReaderPromptSepLineColor: "\x01n\x01h\x01g",
		// Colors for the enhanced reader help line
		enhReaderHelpLineBkgColor: "\x01" + "7",
		enhReaderHelpLineGeneralColor: "\x01b",
		enhReaderHelpLineHotkeyColor: "\x01r",
		enhReaderHelpLineParenColor: "\x01m",

		// Message header line colors
		hdrLineLabelColor: "\x01n\x01c",
		hdrLineValueColor: "\x01n\x01b\x01h",

		// Selected message marker color
		selectedMsgMarkColor: "\x01n\x01w\x01h"
	};
}

// This function returns the month number (1-based) from a capitalized
// month name.
//
// Parameters:
//  pMonthName: The name of the month
//
// Return value: The number of the month (1-12).
function getMonthNum(pMonthName)
{
	var monthNum = 1;

	if (pMonthName.substr(0, 3) == "Jan")
		monthNum = 1;
	else if (pMonthName.substr(0, 3) == "Feb")
		monthNum = 2;
	else if (pMonthName.substr(0, 3) == "Mar")
		monthNum = 3;
	else if (pMonthName.substr(0, 3) == "Apr")
		monthNum = 4;
	else if (pMonthName.substr(0, 3) == "May")
		monthNum = 5;
	else if (pMonthName.substr(0, 3) == "Jun")
		monthNum = 6;
	else if (pMonthName.substr(0, 3) == "Jul")
		monthNum = 7;
	else if (pMonthName.substr(0, 3) == "Aug")
		monthNum = 8;
	else if (pMonthName.substr(0, 3) == "Sep")
		monthNum = 9;
	else if (pMonthName.substr(0, 3) == "Oct")
		monthNum = 10;
	else if (pMonthName.substr(0, 3) == "Nov")
		monthNum = 11;
	else if (pMonthName.substr(0, 3) == "Dec")
		monthNum = 12;

	return monthNum;
}

// Clears each line from a given line to the end of the screen.
//
// Parameters:
//  pStartLineNum: The line number to start at (1-based)
function clearToEOS(pStartLineNum)
{
	if (typeof(pStartLineNum) == "undefined")
		return;
	if (pStartLineNum == null)
		return;

	for (var lineNum = pStartLineNum; lineNum <= console.screen_rows; ++lineNum)
	{
		console.gotoxy(1, lineNum);
		console.clearline();
	}
}

// Returns the number of messages in a sub-board.
//
// Parameters:
//  pSubBoardCode: The sub-board code (i.e., from bbs.cursub_code)
//
// Return value: The number of messages in the sub-board, or 0
//               if the sub-board could not be opened.
function numMessages(pSubBoardCode)
{
   var messageCount = 0;

   var myMsgbase = new MsgBase(pSubBoardCode);
	if (myMsgbase.open())
		messageCount = myMsgbase.total_msgs;
	myMsgbase.close();
	myMsgbase = null;

	return messageCount;
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

// Returns whether an internal sub-board code is valid.
//
// Parameters:
//  pSubBoardCode: The internal sub-board code to test
//
// Return value: Boolean - Whether or not the given internal code is a valid
//               sub-board code
function subBoardCodeIsValid(pSubBoardCode)
{
   return ((pSubBoardCode == "mail") || (typeof(msg_area.sub[pSubBoardCode]) == "object"))
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
	var centerText = (typeof(pCenter) == "boolean" ? pCenter : false);
	var textColor = (typeof(pTextColor) == "string" ? pTextColor : "\x01n\x01w");
	var lineColor = (typeof(pLineColor) == "string" ? pLineColor : "\x01n\x01k\x01h");

	// Output the text and a solid line on the next line.
	if (centerText)
	{
		console.center(textColor + pText);
		var solidLine = "";
		var textLength = console.strlen(pText);
		for (var i = 0; i < textLength; ++i)
			solidLine += HORIZONTAL_SINGLE;
		console.center(lineColor + solidLine);
	}
	else
	{
		console.print(textColor + pText);
		console.crlf();
		console.print(lineColor);
		var textLength = console.strlen(pText);
		for (var i = 0; i < textLength; ++i)
			console.print(HORIZONTAL_SINGLE);
		console.crlf();
	}
}

// Removes multiple, leading, and/or trailing spaces.
// The search & replace regular expressions used in this
// function came from the following URL:
//  http://qodo.co.uk/blog/javascript-trim-leading-and-trailing-spaces
//
// Parameters:
//  pString: The string to trim
//  pLeading: Whether or not to trim leading spaces (optional, defaults to true)
//  pMultiple: Whether or not to trim multiple spaces (optional, defaults to true)
//  pTrailing: Whether or not to trim trailing spaces (optional, defaults to true)
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

// Calculates & returns a page number.
//
// Parameters:
//  pTopIndex: The index (0-based) of the topmost item on the page
//  pNumPerPage: The number of items per page
//
// Return value: The page number
function calcPageNum(pTopIndex, pNumPerPage)
{
  return ((pTopIndex / pNumPerPage) + 1);
}

// Returns the greatest number of messages of all sub-boards within
// a message group.
//
// Parameters:
//  pGrpIndex: The index of the message group
//
// Returns: The greatest number of messages of all sub-boards within
//          the message group
function getGreatestNumMsgs(pGrpIndex)
{
  // Sanity checking
  if (typeof(pGrpIndex) != "number")
    return 0;
  if (typeof(msg_area.grp_list[pGrpIndex]) == "undefined")
    return 0;

  var greatestNumMsgs = 0;
  var msgBase = null;
  for (var subIndex = 0; subIndex < msg_area.grp_list[pGrpIndex].sub_list.length; ++subIndex)
  {
    msgBase = new MsgBase(msg_area.grp_list[pGrpIndex].sub_list[subIndex].code);
    if (msgBase == null) continue;
    if (msgBase.open())
    {
      if (msgBase.total_msgs > greatestNumMsgs)
        greatestNumMsgs = msgBase.total_msgs;
      msgBase.close();
    }
  }
  return greatestNumMsgs;
}

// Inputs a keypress from the user and handles some ESC-based
// characters such as PageUp, PageDown, and ESC.  If PageUp
// or PageDown are pressed, this function will return the
// string defined by KEY_PAGE_UP or KEY_PAGE_DOWN,
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
function getKeyWithESCChars(pGetKeyMode)
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
			default:
				break;
		}
	}

	return userInput;
}

// Finds the next or previous non-empty message sub-board.  Returns an
// object containing the message group & sub-board indexes.  If all of
// the next/previous sub-boards are empty, then the given current indexes
// will be returned.
//
// Parameters:
//  pStartGrpIdx: The index of the message group to start from
//  pStartSubIdx: The index of the sub-board in the message group to start from
//  pForward: Boolean - Whether or not to search forward (true) or backward (false).
//            Optional; defaults to true, to search forward.
//
// Return value: An object with the following properties:
//               foundSubBoard: Boolean - Whether or not a different sub-board was found
//               grpIdx: The message group index of the found sub-board
//               subIdx: The sub-board index in the group of the found sub-board
//               subCode: The internal code of the sub-board
//               subChanged: Boolean - Whether or not the found sub-board is
//                           different from the one that was passed in
//               paramsValid: Boolean - Whether or not all the passed-in parameters
//                            were valid.
function findNextOrPrevNonEmptySubBoard(pStartGrpIdx, pStartSubIdx, pForward)
{
   var retObj = {
	   grpIdx: pStartGrpIdx,
	   subIdx: pStartSubIdx,
	   subCode: msg_area.grp_list[pStartGrpIdx].sub_list[pStartSubIdx].code,
	   foundSubBoard: false
   };

   // Sanity checking
   retObj.paramsValid = ((pStartGrpIdx >= 0) && (pStartGrpIdx < msg_area.grp_list.length) &&
                         (pStartSubIdx >= 0) &&
                         (pStartSubIdx < msg_area.grp_list[pStartGrpIdx].sub_list.length));
   if (!retObj.paramsValid)
      return retObj;

   var grpIdx = pStartGrpIdx;
   var subIdx = pStartSubIdx;
   var searchForward = (typeof(pForward) == "boolean" ? pForward : true);
   if (searchForward)
   {
      // Advance the sub-board (and group) index, and determine whether or not
      // to do the search (i.e., we might not want to if the starting sub-board
      // is the last sub-board in the last group).
      var searchForSubBoard = true;
      if (subIdx <  msg_area.grp_list[grpIdx].sub_list.length - 1)
         ++subIdx;
      else
      {
         if ((grpIdx < msg_area.grp_list.length - 1) && (msg_area.grp_list[grpIdx+1].sub_list.length > 0))
         {
            subIdx = 0;
            ++grpIdx;
         }
         else
            searchForSubBoard = false;
      }
      // If we can search, then do it.
      if (searchForSubBoard)
      {
         while (numMsgsInSubBoard(msg_area.grp_list[grpIdx].sub_list[subIdx].code) == 0)
         {
            if (subIdx < msg_area.grp_list[grpIdx].sub_list.length - 1)
               ++subIdx;
            else
            {
               if ((grpIdx < msg_area.grp_list.length - 1) && (msg_area.grp_list[grpIdx+1].sub_list.length > 0))
               {
                  subIdx = 0;
                  ++grpIdx;
               }
               else
                  break; // Stop searching
            }
         }
      }
   }
   else
   {
      // Search the sub-boards in reverse
      // Decrement the sub-board (and group) index, and determine whether or not
      // to do the search (i.e., we might not want to if the starting sub-board
      // is the first sub-board in the first group).
      var searchForSubBoard = true;
      if (subIdx > 0)
         --subIdx;
      else
      {
         if ((grpIdx > 0) && (msg_area.grp_list[grpIdx-1].sub_list.length > 0))
         {
            --grpIdx;
            subIdx = msg_area.grp_list[grpIdx].sub_list.length - 1;
         }
         else
            searchForSubBoard = false;
      }
      // If we can search, then do it.
      if (searchForSubBoard)
      {
         while (numMsgsInSubBoard(msg_area.grp_list[grpIdx].sub_list[subIdx].code) == 0)
         {
            if (subIdx > 0)
               --subIdx;
            else
            {
               if ((grpIdx > 0) && (msg_area.grp_list[grpIdx-1].sub_list.length > 0))
               {
                  --grpIdx;
                  subIdx = msg_area.grp_list[grpIdx].sub_list.length - 1;
               }
               else
                  break; // Stop searching
            }
         }
      }
   }
   // If we found a sub-board with messages in it, then set the variables
   // in the return object
   if (numMsgsInSubBoard(msg_area.grp_list[grpIdx].sub_list[subIdx].code) > 0)
   {
      retObj.grpIdx = grpIdx;
      retObj.subIdx = subIdx;
      retObj.subCode = msg_area.grp_list[grpIdx].sub_list[subIdx].code;
      retObj.foundSubBoard = true;
      retObj.subChanged = ((grpIdx != pStartGrpIdx) || (subIdx != pStartSubIdx));
   }

   return retObj;
}

// Returns the number of messages in a sub-board.
//
// Parameters:
//  pSubBoardCode: The internal code of the sub-board to check
//  pIncludeDeleted: Optional boolean - Whether or not to include deleted
//                   messages in the count.  Defaults to false.
//
// Return value: The number of messages in the sub-board
function numMsgsInSubBoard(pSubBoardCode, pIncludeDeleted)
{
   var numMessages = 0;
   var msgbase = new MsgBase(pSubBoardCode);
   if (msgbase.open())
   {
      var includeDeleted = (typeof(pIncludeDeleted) == "boolean" ? pIncludeDeleted : false);
      if (includeDeleted)
         numMessages = msgbase.total_msgs;
      else
      {
         // Don't include deleted messages.  Go through each message
         // in the sub-board and count the ones that aren't marked
         // as deleted.
         for (var msgIdx = 0; msgIdx < msgbase.total_msgs; ++msgIdx)
         {
            var msgHdr = msgbase.get_msg_header(true, msgIdx, false);
            if ((msgHdr != null) && ((msgHdr.attr & MSG_DELETE) == 0))
               ++numMessages;
         }
      }
      msgbase.close();
   }
   return numMessages;
}

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

// Shortens a string, accounting for control/attribute codes.  Returns a new
// (shortened) copy of the string.
//
// Parameters:
//  pStr: The string to shorten
//  pNewLength: The new (shorter) length of the string
//  pFromLeft: Optional boolean - Whether to start from the left (default) or
//             from the right.  Defaults to true.
//
// Return value: The shortened version of the string
function shortenStrWithAttrCodes(pStr, pNewLength, pFromLeft)
{
	if (typeof(pStr) != "string")
		return "";
	if (typeof(pNewLength) != "number")
		return pStr;
	if (pNewLength >= console.strlen(pStr))
		return pStr;

	var fromLeft = (typeof(pFromLeft) == "boolean" ? pFromLeft : true);
	var strCopy = "";
	var tmpStr = "";
	var strIdx = 0;
	var lengthGood = true;
	if (fromLeft)
	{
		while (lengthGood && (strIdx < pStr.length))
		{
			tmpStr = strCopy + pStr.charAt(strIdx++);
			if (console.strlen(tmpStr) <= pNewLength)
				strCopy = tmpStr;
			else
				lengthGood = false;
		}
	}
	else
	{
		strIdx = pStr.length - 1;
		while (lengthGood && (strIdx >= 0))
		{
			tmpStr = pStr.charAt(strIdx--) + strCopy;
			if (console.strlen(tmpStr) <= pNewLength)
				strCopy = tmpStr;
			else
				lengthGood = false;
		}
	}
	return strCopy;
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

// Displays a range of text lines on the screen and allows scrolling through them
// with the up & down arrow keys, PageUp, PageDown, HOME, and END.  It is assumed
// that the array of text lines are already truncated to fit in the width of the
// text area, as a speed optimization.
//
// Parameters:
//  pTxtLines: The array of text lines to allow scrolling for
//  pTopLineIdx: The index of the text line to display at the top
//  pTxtAttrib: The attribute(s) to apply to the text lines
//  pWriteTxtLines: Boolean - Whether or not to write the text lines (in addition
//                  to doing the message loop).  If false, this will only do the
//                  the message loop.  This parameter is intended as a screen
//                  refresh optimization.
//  pTopLeftX: The upper-left corner column for the text area
//  pTopLeftY: The upper-left corner row for the text area
//  pWidth: The width of the text area
//  pHeight: The height of the text area
//  pPostWriteCurX: The X location for the cursor after writing the message
//                  lines
//  pPostWriteCurY: The Y location for the cursor after writing the message
//                  lines
// pUseScrollbar: Boolean - Whether or not to display the scrollbar.  If false,
//                this will display a scroll status line at the bottom instead.
//  pScrollUpdateFn: A function that the caller can provide for updating the
//                   scroll position.  This function has one parameter:
//                   - fractionToLastPage: The fraction of the top index divided
//                     by the top index for the last page (basically, the progress
//                     to the last page).
//
// Return value: An object with the following properties:
//               lastKeypress: The last key pressed by the user (a string)
//               topLineIdx: The new top line index of the text lines, in case of scrolling
function scrollTextLines(pTxtLines, pTopLineIdx, pTxtAttrib, pWriteTxtLines, pTopLeftX, pTopLeftY,
                         pWidth, pHeight, pPostWriteCurX, pPostWriteCurY, pUseScrollbar, pScrollUpdateFn,
                         pScrollbarInfo)
{
	// Variables for the top line index for the last page, scrolling, etc.
	var topLineIdxForLastPage = pTxtLines.length - pHeight;
	if (topLineIdxForLastPage < 0)
		topLineIdxForLastPage = 0;
	var msgFractionShown = pHeight / pTxtLines.length;
	if (msgFractionShown > 1)
		msgFractionShown = 1.0;
	var fractionToLastPage = 0;
	var lastTxtRow = pTopLeftY + pHeight - 1;
	var txtLineFormatStr = "%-" + pWidth + "s";

	var retObj = {
		lastKeypress: "",
		topLineIdx: pTopLineIdx
	};

	// Create an array of color/attribute codes for each line of
	// text, in case there are any such codes in the text lines,
	// so that the colors in the message are displayed properly.
	// First, get the last color/attribute codes from first text
	// line and apply them to the next line, and so on.
	var attrCodes = getAttrsBeforeStrIdx(pTxtLines[0], pTxtLines[0].length-1);
	for (var lineIdx = 1; lineIdx < pTxtLines.length; ++lineIdx)
	{
		pTxtLines[lineIdx] = attrCodes + pTxtLines[lineIdx];
		attrCodes = getAttrsBeforeStrIdx(pTxtLines[lineIdx], pTxtLines[lineIdx].length-1);
	}

	var writeTxtLines = pWriteTxtLines;
	var continueOn = true;
	var mouseInputOnly_continue = false;
	while (continueOn)
	{
		mouseInputOnly_continue = false;

		// If we are to write the text lines, then write each of them and also
		// clear out the rest of the row on the screen
		if (writeTxtLines)
		{
			// If the scroll update function parameter is a function, then calculate
			// the fraction to the last page and call the scroll update function.
			if (pUseScrollbar && typeof(pScrollUpdateFn) == "function")
			{
				if (topLineIdxForLastPage != 0)
					fractionToLastPage = retObj.topLineIdx / topLineIdxForLastPage;
				pScrollUpdateFn(fractionToLastPage);
			}
			var screenY = pTopLeftY;
			for (var lineIdx = retObj.topLineIdx; (lineIdx < pTxtLines.length) && (screenY <= lastTxtRow); ++lineIdx)
			{
				console.gotoxy(pTopLeftX, screenY++);
				// Print the text line, then clear the rest of the line
				console.print(pTxtAttrib + pTxtLines[lineIdx]);
				printf("\x01n%" + +(pWidth - console.strlen(pTxtLines[lineIdx])) + "s", "");
			}
			// If there are still some lines left in the message reading area, then
			// clear the lines.
			console.print("\x01n" + pTxtAttrib);
			while (screenY <= lastTxtRow)
			{
				console.gotoxy(pTopLeftX, screenY++);
				printf(txtLineFormatStr, "");
			}
		}

		writeTxtLines = false;

		// Get a keypress from the user and take action based on it
		console.gotoxy(pPostWriteCurX, pPostWriteCurY);
		retObj.lastKeypress = getKeyWithESCChars(K_UPPER|K_NOCRLF|K_NOECHO|K_NOSPIN);
		if (!continueOn)
			break;

		switch (retObj.lastKeypress)
		{
			case KEY_UP:
				if (retObj.topLineIdx > 0)
				{
					--retObj.topLineIdx;
					writeTxtLines = true;
				}
				break;
			case KEY_DOWN:
				if (retObj.topLineIdx < topLineIdxForLastPage)
				{
					++retObj.topLineIdx;
					writeTxtLines = true;
				}
				break;
			case KEY_PAGE_UP: // Previous page
				if (retObj.topLineIdx > 0)
				{
					retObj.topLineIdx -= pHeight;
					if (retObj.topLineIdx < 0)
						retObj.topLineIdx = 0;
					writeTxtLines = true;
				}
				break;
			case KEY_PAGE_DOWN: // Next page
				if (retObj.topLineIdx < topLineIdxForLastPage)
				{
					retObj.topLineIdx += pHeight;
					if (retObj.topLineIdx > topLineIdxForLastPage)
						retObj.topLineIdx = topLineIdxForLastPage;
					writeTxtLines = true;
				}
				break;
			case KEY_HOME: // First page
				if (retObj.topLineIdx > 0)
				{
					retObj.topLineIdx = 0;
					writeTxtLines = true;
				}
				break;
			case KEY_END: // Last page
				if (retObj.topLineIdx < topLineIdxForLastPage)
				{
					retObj.topLineIdx = topLineIdxForLastPage;
					writeTxtLines = true;
				}
				break;
			default:
				continueOn = false;
				break;
		}
	}
	return retObj;
}

// Gets all the attribute codes from an array of text lines until a certain index.
//
// Parameters:
//  pTxtLines: The array of text lines of the message being displayed
//  pEndArrayIdx: One past the last edit line index to get attributes for
//  pIncludeEndArrayIdxAttrs: Optional boolean: Whether or not to include the attributes for the line at the end array index.
//                            Defaults to false.
//  pLastLineTextEndIdx: Optional - Only used when pIncludeEndArrayIdxAttrs is true, this parameter specifies the
//                    end index (non-inclusive) in the last text line to include attributes for.  If not specified,
//                    the entire end line will be used to include its attributes.
//
// Return value: A string containing the relevant attribute codes to apply up to the given line index (non-inclusive).
function getAllEditLineAttrsUntilLineIdx(pTxtLines, pEndArrayIdx, pIncludeEndArrayIdxAttrs, pLastLineTextEndIdx)
{
	if (typeof(pTxtLines) !== "object" || typeof(pEndArrayIdx) !== "number" || pEndArrayIdx < 0)
		return "";

	var includeEndArrayIdxAttrs = (typeof(pIncludeEndArrayIdxAttrs) === "boolean" ? pIncludeEndArrayIdxAttrs : false);

	var syncAttrRegex = /\x01[krgybmcw01234567hinpq,;\.dtl<>\[\]asz]/gi;
	var attributesStr = "";
	var onePastLastIdx = (includeEndArrayIdxAttrs ? pEndArrayIdx + 1 : pEndArrayIdx);
	for (var i = 0; i < onePastLastIdx; ++i)
	{
		if (typeof(pTxtLines[i]) !== "string") continue;
		var attrCodes;
		if (typeof(pLastLineTextEndIdx) === "number" && i === (onePastLastIdx-1))
		{
			// Note: dd_lightbar_menu.js defines substrWithAttrCodes(pStr, pStartIdx, pLen)
			var textLine = substrWithAttrCodes(pTxtLines[i], 0, pLastLineTextEndIdx);
			attrCodes = textLine.match(syncAttrRegex);
		}
		else
			attrCodes = pTxtLines[i].match(syncAttrRegex);
		if (attrCodes != null)
		{
			for (var attrMatchI = 0; attrMatchI < attrCodes.length; ++attrMatchI)
				attributesStr += attrCodes[attrMatchI];
		}
	}
	// If there is a normal attribute code in the middle of the string, remove anything before it
	var normalAttrIdx = attributesStr.lastIndexOf("\x01n");
	if (normalAttrIdx < 0)
		normalAttrIdx = attributesStr.lastIndexOf("\x01N");
	if (normalAttrIdx > -1)
		attributesStr = attributesStr.substr(normalAttrIdx/*+2*/);
	return attributesStr;
}

// Finds the (1-based) page number of an item by number (1-based).  If no page
// is found, then the return value will be 0.
//
// Parameters:
//  pItemNum: The item number (1-based)
//  pNumPerPage: The number of items per page
//  pTotoalNum: The total number of items in the list
//  pReverseOrder: Boolean - Whether or not the list is in reverse order.  If not specified,
//                 this will default to false.
//
// Return value: The page number (1-based) of the item number.  If no page is found,
//               the return value will be 0.
function findPageNumOfItemNum(pItemNum, pNumPerPage, pTotalNum, pReverseOrder)
{
   if ((typeof(pItemNum) != "number") || (typeof(pNumPerPage) != "number") || (typeof(pTotalNum) != "number"))
      return 0;
   if ((pItemNum < 1) || (pItemNum > pTotalNum))
      return 0;

   var reverseOrder = (typeof(pReverseOrder) == "boolean" ? pReverseOrder : false);
   var itemPageNum = 0;
   if (reverseOrder)
   {
      var pageNum = 1;
      for (var topNum = pTotalNum; ((topNum > 0) && (itemPageNum == 0)); topNum -= pNumPerPage)
      {
         if ((pItemNum <= topNum) && (pItemNum >= topNum-pNumPerPage+1))
            itemPageNum = pageNum;
         ++pageNum;
      }
   }
   else // Forward order
      itemPageNum = Math.ceil(pItemNum / pNumPerPage);

   return itemPageNum;
}

// This function converts a search mode string to one of the defined search value
// constants.  If the passed-in mode string is unknown, then the return value will
// be SEARCH_NONE (-1).
//
// Parameters:
//  pSearchTypeStr: A string describing a search mode ("keyword_search", "from_name_search",
//                  "to_name_search", "to_user_search", "new_msg_scan", "new_msg_scan_cur_sub",
//                  "new_msg_scan_cur_grp", "new_msg_scan_all", "to_user_new_scan",
//                  "to_user_all_scan")
//
// Return value: An integer representing the search value (SEARCH_KEYWORD,
//               SEARCH_FROM_NAME, SEARCH_TO_NAME_CUR_MSG_AREA,
//               SEARCH_TO_USER_CUR_MSG_AREA), or SEARCH_NONE (-1) if the passed-in
//               search type string is unknown.
function searchTypeStrToVal(pSearchTypeStr)
{
	if (typeof(pSearchTypeStr) != "string")
		return SEARCH_NONE;

	var searchTypeInt = SEARCH_NONE;
	var modeStr = pSearchTypeStr.toLowerCase();
	if (modeStr == "keyword_search")
		searchTypeInt = SEARCH_KEYWORD;
	else if (modeStr == "from_name_search")
		searchTypeInt = SEARCH_FROM_NAME;
	else if (modeStr == "to_name_search")
		searchTypeInt = SEARCH_TO_NAME_CUR_MSG_AREA;
	else if (modeStr == "to_user_search")
		searchTypeInt = SEARCH_TO_USER_CUR_MSG_AREA;
	else if (modeStr == "new_msg_scan")
		searchTypeInt = SEARCH_MSG_NEWSCAN;
	else if (modeStr == "new_msg_scan_cur_sub")
		searchTypeInt = SEARCH_MSG_NEWSCAN_CUR_SUB;
	else if (modeStr == "new_msg_scan_cur_grp")
		searchTypeInt = SEARCH_MSG_NEWSCAN_CUR_GRP;
	else if (modeStr == "new_msg_scan_all")
		searchTypeInt = SEARCH_MSG_NEWSCAN_ALL;
	else if (modeStr == "to_user_new_scan")
		searchTypeInt = SEARCH_TO_USER_NEW_SCAN;
	else if (modeStr == "to_user_new_scan_cur_sub")
		searchTypeInt = SEARCH_TO_USER_NEW_SCAN_CUR_SUB;
	else if (modeStr == "to_user_new_scan_cur_grp")
		searchTypeInt = SEARCH_TO_USER_NEW_SCAN_CUR_GRP;
	else if (modeStr == "to_user_new_scan_all")
		searchTypeInt = SEARCH_TO_USER_NEW_SCAN_ALL;
	else if (modeStr == "to_user_all_scan")
		searchTypeInt = SEARCH_ALL_TO_USER_SCAN;
	return searchTypeInt;
}

// This function converts a search type value to a string description.
//
// Parameters:
//  pSearchType: The search type value to convert
//
// Return value: A string describing the search type value
function searchTypeValToStr(pSearchType)
{
	if (typeof(pSearchType) != "number")
		return "Unknown (not a number)";

	var searchTypeStr = "";
	switch (pSearchType)
	{
		case SEARCH_NONE:
			searchTypeStr = "None (SEARCH_NONE)";
			break;
		case SEARCH_KEYWORD:
			searchTypeStr = "Keyword (SEARCH_KEYWORD)";
			break;
		case SEARCH_FROM_NAME:
			searchTypeStr = "'From' name (SEARCH_FROM_NAME)";
			break;
		case SEARCH_TO_NAME_CUR_MSG_AREA:
			searchTypeStr = "'To' name (SEARCH_TO_NAME_CUR_MSG_AREA)";
			break;
		case SEARCH_TO_USER_CUR_MSG_AREA:
			searchTypeStr = "To you (SEARCH_TO_USER_CUR_MSG_AREA)";
			break;
		case SEARCH_MSG_NEWSCAN:
			searchTypeStr = "New message scan (SEARCH_MSG_NEWSCAN)";
			break;
		case SEARCH_MSG_NEWSCAN_CUR_SUB:
			searchTypeStr = "New in current message area (SEARCH_MSG_NEWSCAN_CUR_SUB)";
			break;
		case SEARCH_MSG_NEWSCAN_CUR_GRP:
			searchTypeStr = "New in current message group (SEARCH_MSG_NEWSCAN_CUR_GRP)";
			break;
		case SEARCH_MSG_NEWSCAN_ALL:
			searchTypeStr = "Newscan - All (SEARCH_MSG_NEWSCAN_ALL)";
			break;
		case SEARCH_TO_USER_NEW_SCAN:
			searchTypeStr = "To You new scan (SEARCH_TO_USER_NEW_SCAN)";
			break;
		case SEARCH_TO_USER_NEW_SCAN_CUR_SUB:
			searchTypeStr = "To You new scan, current sub-board (SEARCH_TO_USER_NEW_SCAN_CUR_SUB)";
			break;
		case SEARCH_TO_USER_NEW_SCAN_CUR_GRP:
			searchTypeStr = "To You new scan, current group (SEARCH_TO_USER_NEW_SCAN_CUR_GRP)";
			break;
		case SEARCH_TO_USER_NEW_SCAN_ALL:
			searchTypeStr = "To You new scan, all sub-boards (SEARCH_TO_USER_NEW_SCAN_ALL)";
			break;
		case SEARCH_ALL_TO_USER_SCAN:
			searchTypeStr = "All To You scan (SEARCH_ALL_TO_USER_SCAN)";
			break;
		default:
			searchTypeStr = "Unknown (" + pSearchType + ")";
			break;
	}
	return searchTypeStr;
}

// This function converts a reader mode string to one of the defined reader mode
// value constants.  If the passed-in mode string is unknown, then the return value
// will be -1.
//
// Parameters:
//  pModeStr: A string describing a reader mode ("read", "reader", "list", "lister")
//
// Return value: An integer representing the reader mode value (READER_MODE_READ,
//               READER_MODE_LIST), or -1 if the passed-in mode string is unknown.
function readerModeStrToVal(pModeStr)
{
   if (typeof(pModeStr) != "string")
      return -1;

   var readerModeInt = -1;
   var modeStr = pModeStr.toLowerCase();
   if ((modeStr == "read") || (modeStr == "reader"))
      readerModeInt = READER_MODE_READ;
   else if ((modeStr == "list") || (modeStr == "lister"))
      readerModeInt = READER_MODE_LIST;
   return readerModeInt;
}

// This function returns a boolean to signify whether or not the user's
// terminal supports both high-ASCII characters and ANSI codes.
function canDoHighASCIIAndANSI()
{
	//return (console.term_supports(USER_ANSI) && (user.settings & USER_NO_EXASCII == 0));
	return (console.term_supports(USER_ANSI));
}

// Searches a given range in an open message base and returns an object with arrays
// containing the message headers (0-based indexed and indexed by message number)
// with the message headers of any found messages.
//
// Parameters:
//  pSubCode: The internal code of the message sub-board
//  pSearchType: The type of search to do (one of the SEARCH_ values)
//  pSearchString: The string to search for.
//  pListingPersonalEmailFromUser: Optional boolean - Whether or not we're listing
//                                 personal email sent by the user.  This defaults
//                                 to false.
//  pStartIndex: The starting message index (0-based).  Optional; defaults to 0.
//  pEndIndex: One past the last message index.  Optional; defaults to the total number
//             of messages.
//
// Return value: An object with the following arrays:
//               indexed: A 0-based indexed array of message headers
function searchMsgbase(pSubCode, pSearchType, pSearchString, pListingPersonalEmailFromUser, pStartIndex, pEndIndex)
{
	var msgHeaders = {
		indexed: []
	};
	if ((pSubCode != "mail") && ((typeof(pSearchString) != "string") || !searchTypePopulatesSearchResults(pSearchType)))
		return msgHeaders;

	var msgbase = new MsgBase(pSubCode);
	if (!msgbase.open())
		return msgHeaders;

	var startMsgIndex = 0;
	var endMsgIndex = msgbase.total_msgs;
	if (typeof(pStartIndex) == "number")
	{
		if ((pStartIndex >= 0) && (pStartIndex < msgbase.total_msgs))
			startMsgIndex = pStartIndex;
	}
	if (typeof(pEndIndex) == "number")
	{
		if ((pEndIndex >= 0) && (pEndIndex > startMsgIndex) && (pEndIndex <= msgbase.total_msgs))
			endMsgIndex = pEndIndex;
	}

	// Define a search function for the message field we're going to search
	var readingPersonalEmailFromUser = (typeof(pListingPersonalEmailFromUser) == "boolean" ? pListingPersonalEmailFromUser : false);
	var matchFn = null;
	var useGetAllMsgHdrs = false;
	switch (pSearchType)
	{
		// It might seem odd to have SEARCH_NONE in here, but it's here because
		// when reading personal email, we need to search for messages only to
		// the current user.
		case SEARCH_NONE:
			if (pSubCode == "mail")
			{
				// Set up the match function slightly differently depending on whether
				// we're looking for mail from the current user or to the current user.
				if (readingPersonalEmailFromUser)
				{
					matchFn = function(pSearchStr, pMsgHdr, pMsgBase, pSubBoardCode) {
						var msgText = strip_ctrl(pMsgBase.get_msg_body(false, pMsgHdr.number, false, false, true, true));
						return gAllPersonalEmailOptSpecified || msgIsFromUser(pMsgHdr);
						
					}
				}
				else
				{
					// We're reading mail to the user
					matchFn = function(pSearchStr, pMsgHdr, pMsgBase, pSubBoardCode) {
						var msgText = strip_ctrl(pMsgBase.get_msg_body(false, pMsgHdr.number, false, false, true, true));
						var msgMatchesCriteria = (gAllPersonalEmailOptSpecified || msgIsToUserByNum(pMsgHdr));
						// If only new/unread personal email is to be displayed, then check
						// that the message has not been read.
						if (gCmdLineArgVals.onlynewpersonalemail)
							msgMatchesCriteria = (msgMatchesCriteria && ((pMsgHdr.attr & MSG_READ) == 0));
						return msgMatchesCriteria;
					}
				}
			}
			break;
		case SEARCH_KEYWORD:
			useGetAllMsgHdrs = true;
			matchFn = function(pSearchStr, pMsgHdr, pMsgBase, pSubBoardCode) {
				var msgText = strip_ctrl(pMsgBase.get_msg_body(false, pMsgHdr.number, false, false, true, true));
				var keywordFound = ((pMsgHdr.subject.toUpperCase().indexOf(pSearchStr) > -1) || (msgText.toUpperCase().indexOf(pSearchStr) > -1));
				if (pSubBoardCode == "mail")
					return keywordFound && msgIsToUserByNum(pMsgHdr);
				else
					return keywordFound;
			}
			break;
		case SEARCH_FROM_NAME:
			useGetAllMsgHdrs = true;
			matchFn = function(pSearchStr, pMsgHdr, pMsgBase, pSubBoardCode) {
				var fromNameFound = (pMsgHdr.from.toUpperCase() == pSearchStr.toUpperCase());
				if (pSubBoardCode == "mail")
					return fromNameFound && (gAllPersonalEmailOptSpecified || msgIsToUserByNum(pMsgHdr));
				else
					return fromNameFound;
			}
			break;
		case SEARCH_TO_NAME_CUR_MSG_AREA:
			useGetAllMsgHdrs = true;
			matchFn = function(pSearchStr, pMsgHdr, pMsgBase, pSubBoardCode) {
				return (pMsgHdr.to.toUpperCase() == pSearchStr);
			}
			break;
		case SEARCH_TO_USER_CUR_MSG_AREA:
			useGetAllMsgHdrs = true;
		case SEARCH_ALL_TO_USER_SCAN:
			matchFn = function(pSearchStr, pMsgHdr, pMsgBase, pSubBoardCode) {
				// See if the message is not marked as deleted and the 'To' name
				// matches the user's handle, alias, and/or username.
				return (((pMsgHdr.attr & MSG_DELETE) == 0) && userNameHandleAliasMatch(pMsgHdr.to));
			}
			break;
		case SEARCH_TO_USER_NEW_SCAN:
		case SEARCH_TO_USER_NEW_SCAN_CUR_SUB:
		case SEARCH_TO_USER_NEW_SCAN_CUR_GRP:
		case SEARCH_TO_USER_NEW_SCAN_ALL:
			if (pSubCode != "mail")
			{
				// If pStartIndex or pEndIndex aren't specified, then set
				// startMsgIndex to the scan pointer and endMsgIndex to one
				// past the index of the last message in the sub-board
				if (typeof(pStartIndex) != "number")
				{
					// First, write some messages to the log if verbose logging is enabled
					if (gCmdLineArgVals.verboselogging)
					{
						writeToSysAndNodeLog("New-to-user scan for " +
						                     subBoardGrpAndName(pSubCode) + " -- Scan pointer: " +
						                     msg_area.sub[pSubCode].scan_ptr);
					}
					//startMsgIndex = absMsgNumToIdx(msgbase, msg_area.sub[pSubCode].last_read);
					startMsgIndex = absMsgNumToIdx(msgbase, GetScanPtrOrLastMsgNum(pSubCode));
					if (startMsgIndex == -1)
					{
						msg_area.sub[pSubCode].scan_ptr = 0;
						startMsgIndex = 0;
					}
					else
					{
						// If this message has been read, then start at the next message.
						var startMsgHeader = msgbase.get_msg_header(true, startMsgIndex, false);
						if (startMsgHeader == null)
							++startMsgIndex;
						else
						{
							if ((startMsgHeader.attr & MSG_READ) == MSG_READ)
								++startMsgIndex;
						}
					}
				}
				if (typeof(pEndIndex) != "number")
					endMsgIndex = (msgbase.total_msgs > 0 ? msgbase.total_msgs : 0);
			}
			matchFn = function(pSearchStr, pMsgHdr, pMsgBase, pSubBoardCode) {
				// Note: This assumes pSubBoardCode is not "mail" (personal mail).
				// See if the message 'To' name matches the user's handle, alias,
				// and/or username and is not marked as deleted and is unread.
				return (((pMsgHdr.attr & MSG_DELETE) == 0) && ((pMsgHdr.attr & MSG_READ) == 0) && userNameHandleAliasMatch(pMsgHdr.to));
			}
			break;
		case SEARCH_MSG_NEWSCAN:
		case SEARCH_MSG_NEWSCAN_CUR_SUB:
		case SEARCH_MSG_NEWSCAN_CUR_GRP:
		case SEARCH_MSG_NEWSCAN_ALL:
			matchFn = function(pSearchStr, pMsgHdr, pMsgBase, pSubBoardCode) {
				// Note: This assumes pSubBoardCode is not "mail" (personal mail).
				// Get the offset of the last read message and compare it with the
				// offset of the given message header
				var lastReadMsgHdr = pMsgBase.get_msg_header(false, msg_area.sub[pSubBoardCode].last_read, false);
				var lastReadMsgOffset = 0;
				if (lastReadMsgHdr != null)
					lastReadMsgOffset = msgNumToIdxFromMsgbase(pSubBoardCode, lastReadMsgHdr.number);
				if (lastReadMsgOffset < 0)
					lastReadMsgOffset = 0;
				return (msgNumToIdxFromMsgbase(pSubBoardCode, pMsgHdr.number) > lastReadMsgOffset);
			}
			break;
	}
	// Search the messages
	if (matchFn != null)
	{
		// If get_all_msg_headers exists as a function, then use it.  Otherwise,
		// iterate through all message offsets and get the headers.  We want to
		// use get_all_msg_hdrs() if possible because that will include information
		// about how many votes each message got (up/downvotes for regular
		// messages or who voted for which options for poll messages).
		if (useGetAllMsgHdrs && (typeof(msgbase.get_all_msg_headers) === "function"))
		{
			// Pass false to get_all_msg_headers() to tell it not to include vote messages
			// (the parameter was introduced in Synchronet 3.17+)
			var tmpHdrs = msgbase.get_all_msg_headers(false);
			// Re-do startMsgIndex and endMsgIndex based on the message headers we got
			startMsgIndex = 0;
			endMsgIndex = msgbase.total_msgs;
			if (typeof(pStartIndex) == "number")
			{
				if ((pStartIndex >= 0) && (pStartIndex < tmpHdrs.length))
					startMsgIndex = pStartIndex;
			}
			if (typeof(pEndIndex) == "number")
			{
				if ((pEndIndex >= 0) && (pEndIndex > startMsgIndex) && (pEndIndex <= tmpHdrs.length))
					endMsgIndex = pEndIndex;
			}
			// Search the message headers
			var msgIdx = 0;
			for (var prop in tmpHdrs)
			{
				// Only add the message header if the message is readable to the user
				// and msgIdx is within bounds
				if ((msgIdx >= startMsgIndex) && (msgIdx < endMsgIndex) && isReadableMsgHdr(tmpHdrs[prop], pSubCode))
				{
					if (tmpHdrs[prop] != null)
					{
						if (matchFn(pSearchString, tmpHdrs[prop], msgbase, pSubCode))
							msgHeaders.indexed.push(tmpHdrs[prop]);
					}
				}
				++msgIdx;
			}
		}
		else
		{
			for (var msgIdx = startMsgIndex; msgIdx < endMsgIndex; ++msgIdx)
			{
				var msgHeader = msgbase.get_msg_header(true, msgIdx, false);
				// I've seen situations where the message header object is null for
				// some reason, so check that before running the search function.
				if (msgHeader != null)
				{
					if (matchFn(pSearchString, msgHeader, msgbase, pSubCode))
						msgHeaders.indexed.push(msgHeader);
				}
			}
		}
	}
	msgbase.close();
	return msgHeaders;
}

// Returns whether or not a message is to the current user (either the current
// logged-in user or the user specified by the userNum command-line argument)
// and is not deleted.
//
// Parameters:
//  pMsgHdr: A message header object
//
// Return value: Boolean - Whether or not the message is to the user and is not
//               deleted.
function msgIsToUserByNum(pMsgHdr)
{
	if (typeof(pMsgHdr) != "object")
		return false;
	// Return false if  the message is marked as deleted
	if ((pMsgHdr.attr & MSG_DELETE) == MSG_DELETE)
		return false;

	var msgIsToUser = false;
	// If an alternate user number was specified on the command line, then use that
	// user information.  Otherwise, use the current logged-in user.
	if (gCmdLineArgVals.hasOwnProperty("altUserNum"))
		msgIsToUser = (pMsgHdr.to_ext == gCmdLineArgVals.altUserNum);
	else
		msgIsToUser = (pMsgHdr.to_ext == user.number);
	return msgIsToUser;
}

// Returns whether or not a message is from the current user (either the current
// logged-in user or the user specified by the userNum command-line argument)
// and is not deleted.
//
// Parameters:
//  pMsgHdr: A message header object
//
// Return value: Boolean - Whether or not the message is from the logged-in user
//               and is not deleted.
function msgIsFromUser(pMsgHdr)
{
	if (typeof(pMsgHdr) != "object")
		return false;
	// Return false if  the message is marked as deleted
	if ((pMsgHdr.attr & MSG_DELETE) == MSG_DELETE)
		return false;

	var isFromUser = false;

	// If an alternate user number was specified on the command line, then use that
	// user information.  Otherwise, use the current logged-in user.

	if (pMsgHdr.hasOwnProperty("from_ext"))
	{
		if (gCmdLineArgVals.hasOwnProperty("altUserNum"))
			isFromUser = (pMsgHdr.from_ext == gCmdLineArgVals.altUserNum);
		else
			isFromUser = (pMsgHdr.from_ext == user.number);
	}
	else
	{
		var hdrFromUpper = pMsgHdr.from.toUpperCase();
		if (gCmdLineArgVals.hasOwnProperty("altUserName") && gCmdLineArgVals.hasOwnProperty("altUserAlias"))
			isFromUser = ((hdrFromUpper == gCmdLineArgVals.altUserAlias.toUpperCase()) || (hdrFromUpper == gCmdLineArgVals.altUserName.toUpperCase()));
		else
			isFromUser = ((hdrFromUpper == user.alias.toUpperCase()) || (hdrFromUpper == user.name.toUpperCase()));
	}

	return isFromUser;
}

// Returns whether a given message group index & sub-board index (or the current ones,
// based on bbs.curgrp and bbs.cursub) are for the last message sub-board on the system.
//
// Parameters:
//  pGrpIdx: Optional - The index of the message group.  If not specified, this will
//           default to bbs.curgrp.  If bbs.curgrp is not defined in that case,
//           then this method will return false.
//  pSubIdx: Optional - The index of the message sub-board.  If not specified, this will
//           default to bbs.cursub.  If bbs.cursub is not defined in that case,
//           then this method will return false.
//
// Return value: Boolean - Whether or not the current/given message group index & sub-board
//               index are for the last message sub-board on the system.  If there
//               are any issues with any of the values (including bbs.curgrp or
//               bbs.cursub), this method will return false.
function curMsgSubBoardIsLast(pGrpIdx, pSubIdx)
{
	var curGrp = 0;
	if (typeof(pGrpIdx) == "number")
		curGrp = pGrpIdx;
	else if (typeof(bbs.curgrp) == "number")
		curGrp = bbs.curgrp;
	else
		return false;
	var curSub = 0;
	if (typeof(pSubIdx) == "number")
		curSub = pSubIdx;
	else if (typeof(bbs.cursub) == "number")
		curSub = bbs.cursub;
	else
		return false;

	return (curGrp == msg_area.grp_list.length-1) && (curSub == msg_area.grp_list[msg_area.grp_list.length-1].sub_list.length-1);
}

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
	var argVals = getDefaultArgParseObj();

	// Sanity checking for argv - Make sure it's an array
	if ((typeof(argv) != "object") || (typeof(argv.length) != "number"))
		return argVals;

	// First, test the arguments to see if they're in a format as called by
	// Synchronet for loadable modules
	argVals = parseLoadableModuleArgs(argv);
	if (argVals.loadableModule)
		return argVals;

	// Go through argv looking for strings in the format -arg=val and parse them
	// into objects in the argVals array.
	var equalsIdx = 0;
	var argName = "";
	var argVal = "";
	var argValLower = ""; // For case-insensitive "true"/"false" matching
	var argValIsTrue = false;
	for (var i = 0; i < argv.length; ++i)
	{
		// We're looking for strings that start with "-", except strings that are
		// only "-".
		if ((typeof(argv[i]) != "string") || (argv[i].length == 0) ||
		    (argv[i].charAt(0) != "-") || (argv[i] == "-"))
		{
			continue;
		}

		// Look for an = and if found, split the string on the =
		equalsIdx = argv[i].indexOf("=");
		// If a = is found, then split on it and add the argument name & value
		// to the array.  Otherwise (if the = is not found), then treat the
		// argument as a boolean and set it to true (to enable an option).
		if (equalsIdx > -1)
		{
			argName = argv[i].substring(1, equalsIdx).toLowerCase();
			argVal = argv[i].substr(equalsIdx+1);
			argValLower = argVal.toLowerCase();
			// If the argument value is the word "true" or "false", then add it as a
			// boolean.  Otherwise, add it as a string.
			argValIsTrue = (argValLower == "true");
			if (argValIsTrue || (argValLower == "false"))
				argVals[argName] = argValIsTrue;
			else
				argVals[argName] = argVal;
		}
		else // An equals sign (=) was not found.  Add as a boolean set to true to enable the option.
		{
			argName = argv[i].substr(1).toLowerCase();
			if ((argName == "chooseareafirst") || (argName == "personalemail") ||
			    (argName == "personalemailsent") || (argName == "allpersonalemail") ||
			    (argName == "verboselogging") || (argName == "suppresssearchtypetext") ||
			    (argName == "onlynewpersonalemail") || (argName == "indexedmode"))
			{
				argVals[argName] = true;
			}
		}
	}

	// Sanity checking
	// If the arguments include personalEmail and personalEmail is enabled,
	// then check to see if a search type was specified - If so, only allow
	// keyword search and from name search.
	if (argVals.hasOwnProperty("personalemail") && argVals.personalemail)
	{
		// If a search type is specified, only allow keyword search & from name
		// search
		if (argVals.hasOwnProperty("search"))
		{
			var searchValLower = argVals.search.toLowerCase();
			if ((searchValLower != "keyword_search") && (searchValLower != "from_name_search"))
				delete argVals.search;
		}
	}
	// If the arguments include userNum, make sure the value is all digits.  If so,
	// add altUserNum to the arguments as a number type for user matching when looking
	// for personal email to the user.
	if (argVals.hasOwnProperty("usernum"))
	{
		if (/^[0-9]+$/.test(argVals.usernum))
		{
			var specifiedUserNum = Number(argVals.usernum);
			// If the specified number is different than the current logged-in
			// user, then load the other user account and read their name and
			// alias and also store their user number in the arg vals as a
			// number.
			if (specifiedUserNum != user.number)
			{
				var theUser = new User(specifiedUserNum);
				argVals.altUserNum = theUser.number;
				argVals.altUserName = theUser.name;
				argVals.altUserAlias = theUser.alias;
			}
			else
				delete argVals.usernum;
		}
		else
			delete argVals.usernum;
	}

	return argVals;
}
// Helper for parseArgs() - If we get loadable module arguments from Synchronet, this parses them.
//
// Parameters:
//  argv: An array of strings containing values in the format -arg=val
//
// Return value: An object containing the argument values.  The property "loadableModule"
//               in this object will be a boolean that specifies whether or not loadable
//               module arguments were specified.
function parseLoadableModuleArgs(argv)
{
	var argVals = getDefaultArgParseObj();

	var allDigitsRegex = /^[0-9]+$/; // To check if a string consists only of digits
	var arg1Lower = argv[0].toLowerCase();
	// 2 args, and the 1st arg is a sub-board code & the 2nd arg is numeric & is
	// the value of SCAN_INDEX: List messages in the specified sub-board (List Msgs module)
	if (argv.length == 2 && subBoardCodeIsValid(arg1Lower) && allDigitsRegex.test(argv[1]) && +(argv[1]) === SCAN_INDEX)
	{
		argVals.loadableModule = true;
		argVals.subboard = arg1Lower;
		argVals.startmode = "list";
	}
	// 2 parameters: Whether or not all subs are being scanned (0 or 1), and the scan mode (numeric)
	// (Scan Subs module)
	else if (argv.length == 2 && /^[0-1]$/.test(argv[0]) && allDigitsRegex.test(argv[1]))
	{
		argVals.loadableModule = true;
		var scanAllSubs = (argv[0] == "1");
		var scanMode = +(argv[1]);
		if ((scanMode & SCAN_NEW) == SCAN_NEW)
		{
			// Newscan
			// TODO: SCAN_CONST and SCAN_BACK could be used along with SCAN_NEW
			// SCAN_CONST: Continuous message scanning
			// SCAN_BACK: Display most recent message if none new
			argVals.search = "new_msg_scan";
			argVals.suppresssearchtypetext = true;
			if (scanAllSubs)
				argVals.search = "new_msg_scan_all";
		}
		else if (((scanMode & SCAN_TOYOU) == SCAN_TOYOU) || ((scanMode & SCAN_UNREAD) == SCAN_UNREAD))
		{
			// Scan for messages posted to you/new messages posted to you
			argVals.startmode = "read";
			argVals.search = "to_user_new_scan";
			argVals.suppresssearchtypetext = true;
			if (scanAllSubs)
				argVals.search = "to_user_new_scan_all";
		}
		else if ((scanMode & SCAN_FIND) == SCAN_FIND)
		{
			argVals.search = "keyword_search";
			argVals.startmode = "list";
		}
		else
		{
			// Stock Synchronet functionality.  Includes SCAN_CONST and SCAN_BACK.
			bbs.scan_subs(scanMode, scanAllSubs);
			argVals.exitNow = true;
		}
	}
	// Scan Msgs loadable module support:
	// 1. The sub-board internal code
	// 2. The scan mode (numeric)
	// 3. Optional: Search text (if any)
	else if ((argv.length == 2 || argv.length == 3) && subBoardCodeIsValid(arg1Lower) && allDigitsRegex.test(argv[1]))
	{
		argVals.loadableModule = true;
		var scanMode = +(argv[1]);
		if (scanMode == SCAN_READ)
		{
			argVals.subboard = arg1Lower;
			argVals.startmode = "read";
			// If a search string is specified (as the 3rd command-line argument),
			// then use it for a search scan.
			if (argv.length == 3 && argv[2] != "")
			{
				argVals.search = "keyword_search";
				argVals.searchtext = argv[2];
			}
		}
		else if (scanMode == SCAN_FIND)
		{
			argVals.subboard = arg1Lower;
			argVals.search = "keyword_search";
			argVals.startmode = "list";
			if (argv.length == 3 && argv[2] != "")
				argVals.searchtext = argv[2];
		}
		// Some modes that the Digital Distortion Message Reader doesn't handle yet: Use
		// Synchronet's stock behavior.
		else
		{
			if (argv.length == 3)
				bbs.scan_msgs(arg1Lower, scanMode, argv[2]);
			else
				bbs.scan_msgs(arg1Lower, scanMode);
			argVals.exitNow = true;
		}
	}
	// Reading personal email: 'Which' mailbox & user number (both numeric) (Read Mail module)
	else if ((argv.length == 2 || argv.length == 3) && allDigitsRegex.test(argv[0]) && allDigitsRegex.test(argv[1]) && isValidUserNum(+(argv[1])))
	{
		argVals.loadableModule = true;
		var whichMailbox = +(argv[0]);
		var userNum = +(argv[1]);
		// The optional 3rd argument in this case is mode bits.  See if we should only display
		// new (unread) personal email.
		var newMailOnly = false;
		if (argv.length >= 3)
		{
			var modeVal = +(argv[2]);
			newMailOnly = (((modeVal & SCAN_FIND) == SCAN_FIND) && ((modeVal & LM_UNREAD) == LM_UNREAD));
		}
		// Start in list mode
		argVals.startmode = "list"; // "read"
		// Note: MAIL_ANY won't be passed to this script.
		switch (whichMailbox)
		{
			case MAIL_YOUR: // Mail sent to you
				argVals.personalemail = true;
				argVals.usernum = argv[1];
				if (newMailOnly)
					argVals.onlynewpersonalemail = true;
				break;
			case MAIL_SENT: // Mail you have sent
				argVals.personalemailsent = true;
				argVals.usernum = argv[1];
				break;
			case MAIL_ALL:
				argVals.allpersonalemail = true;
				break;
			default:
				bbs.read_mail(whichMailbox);
				argVals.exitNow = true;
				break;
		}
	}
	return argVals;
}
// Returns an object with default settings for argument parsing
function getDefaultArgParseObj()
{
	return {
		chooseareafirst: false,
		personalemail: false,
		onlynewpersonalemail: false,
		personalemailsent: false,
		verboselogging: false,
		suppresssearchtypetext: false,
		indexedmode: false,
		loadableModule: false,
		exitNow: false
	};
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

// Given a sub-board code, this function returns a sub-board's group and name.
// If the given sub-board code is "mail", then this will return "Personal mail".
//
// Parameters:
//  pSubBoardCode: An internal sub-board code
//
// Return value: A string containing the sub-board code group & name, or
//               "Personal email" if it's the personal email sub-board
function subBoardGrpAndName(pSubBoardCode)
{
	if (typeof(pSubBoardCode) != "string")
		return "";

	var subBoardGrpAndName = "";
	if (pSubBoardCode == "mail")
		subBoardGrpAndName = "Personal mail";
	else
	{
		subBoardGrpAndName = msg_area.sub[pSubBoardCode].grp_name + " - "
                         + msg_area.sub[pSubBoardCode].name;
	}

	return subBoardGrpAndName;
}

// Returns whether a given string matches the current user's name, handle, or alias.
// Does a case-insensitive match.
//
// Parameters:
//  pStr: The string to match against the user's name/handle/alias
//
// Return value: Boolean - Whether or not the string matches the current user's name,
//               handle, or alias
function userNameHandleAliasMatch(pStr)
{
	if (typeof(pStr) != "string")
		return false;
	var strUpper = pStr.toUpperCase();
	return ((strUpper == user.name.toUpperCase()) || (strUpper == user.handle.toUpperCase()) || (strUpper == user.alias.toUpperCase()));
}

// Writes a log message to the system log (using LOG_INFO log level) and to the
// node log.  This will prepend the text "Digital Distortion Message Reader ("
// + user.alias + "): " to the log message.
// 
// Parameters:
//  pMessage: The message to log
//  pLogLevel: The log level.  Optional - Defaults to LOG_INFO.
function writeToSysAndNodeLog(pMessage, pLogLevel)
{
	if (typeof(pMessage) != "string")
		return;

	var logMessage = "Digital Distortion Message Reader (" +  user.alias + "): " + pMessage;
	var logLevel = (typeof(pLogLevel) == "number" ? pLogLevel : LOG_INFO);
	log(logLevel, logMessage);
	bbs.log_str(logMessage);
}

// This function looks up and returns a sub-board code from the sub-board number.
// If no matching sub-board is found, this will return an empty string.
//
// Parameters:
//  pSubBoardNum: A sub-board number
//
// Return value: The sub-board code.  If no matching sub-board is found, an empty
//               string will be returned.
function getSubBoardCodeFromNum(pSubBoardNum)
{
	// Ensure we're using a numeric type for the sub-board number
	// (in case pSubBoardNum is a string rather than a number)
	var subNum = Number(pSubBoardNum);

	var subBoardCode = "";
	for (var subCode in msg_area.sub)
	{
		if (msg_area.sub[subCode].number == subNum)
		{
			subBoardCode = subCode;
			break;
		}
	}
	return subBoardCode;
}

// This function recursively removes a directory and all of its contents.  Returns
// whether or not the directory was removed.
//
// Parameters:
//  pDir: The directory to remove (with trailing slash).
//
// Return value: Boolean - Whether or not the directory was removed.
function deltree(pDir)
{
	if ((pDir == null) || (pDir == undefined))
		return false;
	if (typeof(pDir) != "string")
		return false;
	if (pDir.length == 0)
		return false;
	// Make sure pDir actually specifies a directory.
	if (!file_isdir(pDir))
		return false;
	// Don't wipe out a root directory.
	if ((pDir == "/") || (pDir == "\\") || (/:\\$/.test(pDir)) || (/:\/$/.test(pDir)) || (/:$/.test(pDir)))
		return false;

	// If we're on Windows, then use the "RD /S /Q" command to delete
	// the directory.  Otherwise, assume *nix and use "rm -rf" to
	// delete the directory.
	if (deltree.inWindows == undefined)
		deltree.inWindows = (/^WIN/.test(system.platform.toUpperCase()));
	if (deltree.inWindows)
		system.exec("RD " + withoutTrailingSlash(pDir) + " /s /q");
	else
		system.exec("rm -rf " + withoutTrailingSlash(pDir));
	// The directory should be gone, so we should return true.  I'd like to verify that the
	// directory really is gone, but file_exists() seems to return false for directories,
	// even if the directory does exist.  So I test to make sure no files are seen in the dir.
	return (directory(pDir + "*").length == 0);

	/*
	// Recursively deleting each file & dir using JavaScript:
	var retval = true;

	// Open the directory and delete each entry.
	var files = directory(pDir + "*");
	for (var i = 0; i < files.length; ++i)
	{
		// If the entry is a directory, then deltree it (Note: The entry
		// should have a trailing slash).  Otherwise, delete the file.
		// If the directory/file couldn't be removed, then break out
		// of the loop.
		if (file_isdir(files[i]))
		{
			retval = deltree(files[i]);
			if (!retval)
				break;
		}
		else
		{
			retval = file_remove(files[i]);
			if (!retval)
				break;
		}
	}

	// Delete the directory specified by pDir.
	if (retval)
		retval = rmdir(pDir);

	return retval;
*/
}

// Removes a trailing (back)slash from a path.
//
// Parameters:
//  pPath: A directory path
//
// Return value: The path without a trailing (back)slash.
function withoutTrailingSlash(pPath)
{
	if ((pPath == null) || (pPath == undefined))
		return "";

	var retval = pPath;
	if (retval.length > 0)
	{
		var lastIndex = retval.length - 1;
		var lastChar = retval.charAt(lastIndex);
		if ((lastChar == "\\") || (lastChar == "/"))
			retval = retval.substr(0, lastIndex);
	}
	return retval;
}

// Adds double-quotes around a string if the string contains spaces.
//
// Parameters:
//  pStr: A string to add double-quotes around if it has spaces
//
// Return value: The string with double-quotes if it contains spaces.  If the
//               string doesn't contain spaces, then the same string will be
//               returned.
function quoteStrWithSpaces(pStr)
{
	if (typeof(pStr) != "string")
		return "";
	var strCopy = pStr;
	if (pStr.indexOf(" ") > -1)
		strCopy = "\"" + pStr + "\"";
	return strCopy;
}

// Given a message header field list type number (i.e., the 'type' property for an
// entry in the field_list array in a message header), this returns a text label
// to be used for outputting the field.
//
// Parameters:
//  pFieldListType: A field_list entry type (numeric)
//  pIncludeTrailingColon: Optional boolean - Whether or not to include a trailing ":"
//                         at the end of the returned string.  Defaults to true.
//
// Return value: A text label for the field (a string)
function msgHdrFieldListTypeToLabel(pFieldListType, pIncludeTrailingColon)
{
	// The page at this URL lists the header field types:
	// http://synchro.net/docs/smb.html#Header Field Types:

	var fieldTypeLabel = "Unknown (" + pFieldListType.toString() + ")";
	switch (pFieldListType)
	{
		case 0: // Sender
			fieldTypeLabel = "Sender";
			break;
		case 1: // Sender Agent
			fieldTypeLabel = "Sender Agent";
			break;
		case 2: // Sender net type
			fieldTypeLabel = "Sender Net Type";
			break;
		case 3: // Sender Net Address
			fieldTypeLabel = "Sender Net Address";
			break;
		case 4: // Sender Agent Extension
			fieldTypeLabel = "Sender Agent Extension";
			break;
		case 5: // Sending agent (Sender POS)
			fieldTypeLabel = "Sender Agent";
			break;
		case 6: // Sender organization
			fieldTypeLabel = "Sender Organization";
			break;
		case 16: // Author
			fieldTypeLabel = "Author";
			break;
		case 17: // Author Agent
			fieldTypeLabel = "Author Agent";
			break;
		case 18: // Author Net Type
			fieldTypeLabel = "Author Net Type";
			break;
		case 19: // Author Net Address
			fieldTypeLabel = "Author Net Address";
			break;
		case 20: // Author Extension
			fieldTypeLabel = "Author Extension";
			break;
		case 21: // Author Agent (Author POS)
			fieldTypeLabel = "Author Agent";
			break;
		case 22: // Author Organization
			fieldTypeLabel = "Author Organization";
			break;
		case 32: // Reply To
			fieldTypeLabel = "Reply To";
			break;
		case 33: // Reply To agent
			fieldTypeLabel = "Reply To Agent";
			break;
		case 34: // Reply To net type
			fieldTypeLabel = "Reply To net type";
			break;
		case 35: // Reply To net address
			fieldTypeLabel = "Reply To net address";
			break;
		case 36: // Reply To extension
			fieldTypeLabel = "Reply To (extended)";
			break;
		case 37: // Reply To position
			fieldTypeLabel = "Reply To position";
			break;
		case 38: // Reply To organization (0x26 hex)
			fieldTypeLabel = "Reply To organization";
			break;
		case 48: // Recipient (0x30 hex)
			fieldTypeLabel = "Recipient";
			break;
		case 162: // Seen-by
			fieldTypeLabel = "Seen-by";
			break;
		case 163: // Path
			fieldTypeLabel = "Path";
			break;
		case 176: // RFCC822 Header
			fieldTypeLabel = "RFCC822 Header";
			break;
		case 177: // RFC822 MSGID
			fieldTypeLabel = "RFC822 MSGID";
			break;
		case 178: // RFC822 REPLYID
			fieldTypeLabel = "RFC822 REPLYID";
			break;
		case 240: // UNKNOWN
			fieldTypeLabel = "UNKNOWN";
			break;
		case 241: // UNKNOWNASCII
			fieldTypeLabel = "UNKNOWN (ASCII)";
			break;
		case 255:
			fieldTypeLabel = "UNUSED";
			break;
		default:
			fieldTypeLabel = "Unknown (" + pFieldListType.toString() + ")";
			break;
	}

	var includeTrailingColon = (typeof(pIncludeTrailingColon) == "boolean" ? pIncludeTrailingColon : true);
	if (includeTrailingColon)
		fieldTypeLabel += ":";

	return fieldTypeLabel;
}

// Capitalizes the first character of a string.
//
// Parameters:
//  pStr: The string to capitalize
//
// Return value: A version of the sting with the first character capitalized
function capitalizeFirstChar(pStr)
{
	var retStr = "";
	if (typeof(pStr) == "string")
	{
		if (pStr.length > 0)
			retStr = pStr.charAt(0).toUpperCase() + pStr.slice(1);
	}
	return retStr;
}

// Parses a list of numbers (separated by commas or spaces), which may contain
// ranges separated by dashes.  Returns an array of the individual numbers.
//
// Parameters:
//  pList: A comma-separated list of numbers, some which may contain
//         2 numbers separated by a dash denoting a range of numbers.
//
// Return value: An array of the individual numbers from the list
function parseNumberList(pList)
{
	if (typeof(pList) != "string")
		return [];

	var numberList = [];

	// Split pList on commas or spaces
	var commaOrSpaceSepArray = pList.split(/[\s,]+/);
	if (commaOrSpaceSepArray.length > 0)
	{
		// Go through the comma-separated array - If the element is a
		// single number, then append it to the number list to be returned.
		// If there is a range (2 numbers separated by a dash), then
		// append each number in the range individually to the array to be
		// returned.
		for (var i = 0; i < commaOrSpaceSepArray.length; ++i)
		{
			// If it's a single number, append it to numberList.
			if (/^[0-9]+$/.test(commaOrSpaceSepArray[i]))
				numberList.push(+commaOrSpaceSepArray[i]);
			// If there are 2 numbers separated by a dash, then split it on the
			// dash and generate the intermediate numbers.
			else if (/^[0-9]+-[0-9]+$/.test(commaOrSpaceSepArray[i]))
			{
				var twoNumbers = commaOrSpaceSepArray[i].split("-");
				if (twoNumbers.length == 2)
				{
					var num1 = +twoNumbers[0];
					var num2 = +twoNumbers[1];
					// If the 1st number is bigger than the 2nd, then swap them.
					if (num1 > num2)
					{
						var temp = num1;
						num1 = num2;
						num2 = temp;
					}
					// Append each individual number in the range to numberList.
					for (var number = num1; number <= num2; ++number)
						numberList.push(number);
				}
			}
		}
	}

	return numberList;
}

// Inputs a single keypress from the user from a list of valid keys, allowing
// input modes (see K_* in sbbsdefs.js for mode bits).  This is similar to
// console.getkeys(), except that this allows mode bits (such as K_NOCRLF, etc.).
//
// Parameters:
//  pAllowedKeys: A list of allowed keys (string)
//  pMode: Mode bits (see K_* in sbbsdefs.js)
//
// Return value: The user's inputted keypress
function getAllowedKeyWithMode(pAllowedKeys, pMode)
{
	var userInput = "";

	var keypress = "";
	var i = 0;
	var matchedKeypress = false;
	while (!matchedKeypress)
	{
		keypress = console.getkey(K_NOECHO|pMode);
		// Check to see if the keypress is one of the allowed keys
		for (i = 0; i < pAllowedKeys.length; ++i)
		{
			if (keypress == pAllowedKeys[i])
				userInput = keypress;
			else if (keypress.toUpperCase() == pAllowedKeys[i])
				userInput = keypress.toUpperCase();
			else if (keypress.toLowerCase() == pAllowedKeys[i])
				userInput = keypress.toLowerCase();
			if (userInput.length > 0)
			{
				matchedKeypress = true;
				// If K_NOECHO is not in pMode, then output the user's keypress
				if ((pMode & K_NOECHO) == 0)
					console.print(userInput);
				// If K_NOCRLF is not in pMode, then output a CRLF
				if ((pMode & K_NOCRLF) == 0)
					console.crlf();
				break;
			}
		}
	}

	return userInput;
}

// Loads a text file (an .ans or .asc) into an array.  This will first look for
// an .ans version, and if exists, convert to Synchronet colors before loading
// it.  If an .ans doesn't exist, this will look for an .asc version.
//
// Parameters:
//  pFilenameBase: The filename without the extension
//  pMaxNumLines: Optional - The maximum number of lines to load from the text file
//
// Return value: An array containing the lines from the text file
function loadTextFileIntoArray(pFilenameBase, pMaxNumLines)
{
	if (typeof(pFilenameBase) != "string")
		return [];

	var maxNumLines = (typeof(pMaxNumLines) == "number" ? pMaxNumLines : -1);

	var txtFileLines = [];
	// See if there is a header file that is made for the user's terminal
	// width (areaChgHeader-<width>.ans/asc).  If not, then just go with
	// msgHeader.ans/asc.
	var txtFileExists = true;
	var txtFilenameFullPath = gStartupPath + pFilenameBase;
	var txtFileFilename = "";
	if (file_exists(txtFilenameFullPath + "-" + console.screen_columns + ".ans"))
		txtFileFilename = txtFilenameFullPath + "-" + console.screen_columns + ".ans";
	else if (file_exists(txtFilenameFullPath + "-" + console.screen_columns + ".asc"))
		txtFileFilename = txtFilenameFullPath + "-" + console.screen_columns + ".asc";
	else if (file_exists(txtFilenameFullPath + ".ans"))
		txtFileFilename = txtFilenameFullPath + ".ans";
	else if (file_exists(txtFilenameFullPath + ".asc"))
		txtFileFilename = txtFilenameFullPath + ".asc";
	else
		txtFileExists = false;
	if (txtFileExists)
	{
		var syncConvertedHdrFilename = txtFileFilename;
		// If the user's console doesn't support ANSI and the header file is ANSI,
		// then convert it to Synchronet attribute codes and read that file instead.
		if (!console.term_supports(USER_ANSI) && (getStrAfterPeriod(txtFileFilename).toUpperCase() == "ANS"))
		{
			syncConvertedHdrFilename = txtFilenameFullPath + "_converted.asc";
			if (!file_exists(syncConvertedHdrFilename))
			{
				if (getStrAfterPeriod(txtFileFilename).toUpperCase() == "ANS")
				{
					var dotIdx = txtFileFilename.lastIndexOf(".");
					if (dotIdx >= 0)
					{
						var filenameBase = txtFileFilename.substr(0, dotIdx);
						var cmdLine = system.exec_dir + "ans2asc \"" + txtFileFilename + "\" \""
									+ syncConvertedHdrFilename + "\"";
						// Note: Both system.exec(cmdLine) and
						// bbs.exec(cmdLine, EX_NATIVE, gStartupPath) could be used to
						// execute the command, but system.exec() seems noticeably faster.
						system.exec(cmdLine);
					}
				}
				else
					syncConvertedHdrFilename = txtFileFilename;
			}
		}
		/*
		// If the header file is ANSI, then convert it to Synchronet attribute
		// codes and read that file instead.  This is done so that this script can
		// accurately get the file line lengths using console.strlen().
		var syncConvertedHdrFilename = txtFilenameFullPath + "_converted.asc";
		if (!file_exists(syncConvertedHdrFilename))
		{
			if (getStrAfterPeriod(txtFileFilename).toUpperCase() == "ANS")
			{
				var filenameBase = txtFileFilename.substr(0, dotIdx);
				var cmdLine = system.exec_dir + "ans2asc \"" + txtFileFilename + "\" \""
				            + syncConvertedHdrFilename + "\"";
				// Note: Both system.exec(cmdLine) and
				// bbs.exec(cmdLine, EX_NATIVE, gStartupPath) could be used to
				// execute the command, but system.exec() seems noticeably faster.
				system.exec(cmdLine);
			}
			else
				syncConvertedHdrFilename = txtFileFilename;
		}
		*/
		// Read the header file into txtFileLines
		var hdrFile = new File(syncConvertedHdrFilename);
		if (hdrFile.open("r"))
		{
			var fileLine = null;
			while (!hdrFile.eof)
			{
				// Read the next line from the header file.
				fileLine = hdrFile.readln(2048);
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
				if (txtFileLines.length == maxNumLines)
					break;
			}
			hdrFile.close();
		}
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

// Adjusts a message's when-written time to the BBS's local time.
//
// Parameters:
//  pMsgHdr: A message header object
//
// Return value: The message's when_written_time adjusted to the BBS's local time.
//               If the message header doesn't have a when_written_time or
//               when_written_zone property, then this function will return -1.
function msgWrittenTimeToLocalBBSTime(pMsgHdr)
{
	if (!pMsgHdr.hasOwnProperty("when_written_time") || !pMsgHdr.hasOwnProperty("when_written_zone_offset") || !pMsgHdr.hasOwnProperty("when_imported_zone_offset"))
		return -1;

	var timeZoneDiffMinutes = pMsgHdr.when_imported_zone_offset - pMsgHdr.when_written_zone_offset;
	//var timeZoneDiffMinutes = pMsgHdr.when_written_zone - system.timezone;
	var timeZoneDiffSeconds = timeZoneDiffMinutes * 60;
	var msgWrittenTimeAdjusted = pMsgHdr.when_written_time + timeZoneDiffSeconds;
	return msgWrittenTimeAdjusted;
}

// Returns a string containing the message group & sub-board numbers and
// descriptions.
//
// Parameters:
//  pMsgbase: A MsgBase object
//
// Return value: A string containing the message group & sub-board numbers and
// descriptions
function getMsgAreaDescStr(pMsgbase)
{
	if (typeof(pMsgbase) != "object")
		return "";
	if (!pMsgbase.is_open)
		return "";

	var descStr = "";
	if (pMsgbase.cfg != null)
	{
		descStr = format("Group/sub-board num: %d, %d; %s - %s", pMsgbase.cfg.grp_number,
		                 pMsgbase.subnum, msg_area.grp_list[pMsgbase.cfg.grp_number].description,
		                 pMsgbase.cfg.description);
	}
	else
	{
		if ((pMsgbase.subnum == -1) || (pMsgbase.subnum == 65535))
			descStr = "Electronic Mail";
		else
			descStr = "Unspecified";
	}
	return descStr;
}

// Lets the sysop edit a user.
//
// Parameters:
//  pUsername: The name of the user to edit
//
// Return value: A function containing the following properties:
//               errorMsg: An error message on failure, or a blank string on success
function editUser(pUsername)
{
	var retObj = {
		errorMsg: ""
	};

	if (typeof(pUsername) != "string")
	{
		retObj.errorMsg = "Given username is not a string";
		return retObj;
	}

	// If the logged-in user is not a sysop, then just return.
	if (!user.is_sysop)
	{
		retObj.errorMsg = "Only a sysop can edit a user";
		return retObj;
	}

	// If the user exists, then let the sysop edit the user.
	var userNum = system.matchuser(pUsername);
	if (userNum != 0)
		bbs.exec("*str_cmds uedit " + userNum);
	else
		retObj.errorMsg = "User \"" + pUsername + "\" not found";
	
	return retObj;
}

// Returns an object containing bare minimum properties necessary to
// display an invalid message header.  Additionally, an object returned
// by this function will have an extra property, isBogus, that will be
// a boolean set to true.
//
// Parameters:
//  pSubject: Optional - A string to use as the subject in the bogus message
//            header object
function getBogusMsgHdr(pSubject)
{
	var msgHdr = {
		subject: (typeof(pSubject) == "string" ? pSubject : ""),
		when_imported_time: 0,
		when_written_time: 0,
		when_written_zone: 0,
		date: "Fri, 1 Jan 1960 00:00:00 -0000",
		attr: 0,
		to: "Nobody",
		from: "Nobody",
		number: 0,
		offset: 0,
		isBogus: true
	};
	return msgHdr;
}

// Returns whether a message is readable to the user, based on its
// header and the sub-board code.
//
// Parameters:
//  pMsgHdr: The header object for the message
//  pSubBoardCode: The internal code for the sub-board the message is in
//
// Return value: Boolean - Whether or not the message is readable for the user
function isReadableMsgHdr(pMsgHdr, pSubBoardCode)
{
	if (pMsgHdr === null)
		return false;
	// Let the sysop see unvalidated messages and private messages but not other users.
	if (!user.is_sysop)
	{
		if (pSubBoardCode != "mail")
		{
			if ((msg_area.sub[pSubBoardCode].is_moderated && ((pMsgHdr.attr & MSG_VALIDATED) == 0)) ||
			    (((pMsgHdr.attr & MSG_PRIVATE) == MSG_PRIVATE) && !userHandleAliasNameMatch(pMsgHdr.to)))
			{
				return false;
			}
		}
	}
	// If the message is deleted, determine whether it should be viewable, based
	// on the system settings.
	if ((pMsgHdr.attr & MSG_DELETE) == MSG_DELETE)
	{
		// If the user is a sysop, check whether sysops can view deleted messages.
		// Otherwise, check whether users can view deleted messages.
		if (user.is_sysop)
		{
			if ((system.settings & SYS_SYSVDELM) == 0)
				return false;
		}
		else
		{
			if ((system.settings & SYS_USRVDELM) == 0)
				return false;
		}
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

// Returns the number of readable messages in a sub-board.
//
// Parameters:
//  pMsgbase: The MsgBase object representing the sub-board
//  pSubBoardCode: The internal code of the sub-board
//
// Return value: The number of readable messages in the sub-board
function numReadableMsgs(pMsgbase, pSubBoardCode)
{
	if ((pMsgbase === null) || !pMsgbase.is_open)
		return 0;

	var numMsgs = 0;
	if (typeof(pMsgbase.get_all_msg_headers) === "function")
	{
		var msgHdrs = pMsgbase.get_all_msg_headers(true);
		for (var msgHdrsProp in msgHdrs)
		{
			if (msgHdrs[msgHdrsProp] == null)
				continue;
			else if (isReadableMsgHdr(msgHdrs[msgHdrsProp], pSubBoardCode))
				++numMsgs;
		}
	}
	else
	{
		var msgHeader;
		for (var i = 0; i < pMsgbase.total_msgs; ++i)
		{
			msgHeader = msgBase.get_msg_header(true, i, false);
			if (msgHeader == null)
				continue;
			else if (isReadableMsgHdr(msgHeader, pSubBoardCode))
				++numMsgs;
		}
	}
	return numMsgs;
}

// Marks or unmarks vote messages as deleted (messages that have voting response data for a message with
// a given message number).
//
// Parameters:
//  pMsgbase: A MessageBase object containing the messages to be deleted
//  pMsgNum: The number of the message for which vote messages should be deleted
//  pMsgID: The ID of the message for which vote messages should be deleted
//  pDoDelete: Boolean: If true, then mark for deletion.  If false, then remove the deleted attribute.
//  pIsMailSub: Boolean - Whether or not it's the personal email area
//
// Return value: An object containing the following properties:
//               numVoteMsgs: The number of vote messages for the given message number
//               toggleVoteMsgsAffected: The number of vote messages that were deleted/undeleted
//               allVoteMsgsAffected: Boolean - Whether or not all vote messages were deleted/undeleted
function toggleVoteMsgsDeleted(pMsgbase, pMsgNum, pMsgID, pDoDelete, pIsEmailSub)
{
	var retObj = {
		numVoteMsgs: 0,
		toggleVoteMsgsAffected: 0,
		allVoteMsgsAffected: true
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
				var msgWasAffected = false;
				if (pDoDelete)
					msgWasAffected = pMsgbase.remove_msg(false, msgHdrs[msgHdrsProp].number);
				else
				{
					var tmpMsgHdr = pMsgbase.get_msg_header(false, msgHdrs[msgHdrsProp].number, false);
					if (tmpMsgHdr != null)
					{
						if ((tmpMsgHdr.attr & MSG_DELETE) == MSG_DELETE)
						{
							tmpMsgHdr.attr = tmpMsgHdr.attr ^ MSG_DELETE;
							msgWasAffected = pMsgbase.put_msg_header(false, msgHdrs[msgHdrsProp].number, tmpMsgHdr);
						}
						else
							msgWasAffected = true; // No action needed
					}
				}
				retObj.allVoteMsgsAffected = (retObj.allVoteMsgsAffected && msgWasAffected);
				if (msgWasAffected)
					++retObj.toggleVoteMsgsAffected;
			}
		}
	}

	return retObj;
}

/////////////////////////////////////////////////////////////////////////
// Debug helper & error output functions

// Prints information from a message header on the screen, for debugging purpurposes.
//
// Parameters:
//  pMsgHdr: A message header object
function printMsgHdr(pMsgHdr)
{
	for (var prop in pMsgHdr)
	{
		if ((prop == "field_list") && (typeof(pMsgHdr[prop]) == "object"))
		{
			console.print(prop + ":\r\n");
			for (var objI = 0; objI < pMsgHdr[prop].length; ++objI)
			{
				console.print(" " + objI + ":\r\n");
				for (var innerProp in pMsgHdr[prop][objI])
					console.print("  " + innerProp + ": " + pMsgHdr[prop][objI][innerProp] + "\r\n");
			}
		}
		else
			console.print(prop + ": " + pMsgHdr[prop] + "\r\n");
	}
	console.pause();
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

// Gets a message header from the messagebase, either by index (offset) or number.
//
// Parameters:
//  pMsgbase: Optional messagebase object.  If this is provided, then pSubBoardCode is not used.
//  pSubBoardCode: The messagebase sub-board code
//  pByIdx: Boolean - Whether or not to get the message header by index (if false, then by number)
//  pMsgIdxOrNum: The message index or number of the message header to retrieve
//  pExpandFields: Boolean - Whether or not to expand fields for the message header
function getHdrFromMsgbase(pMsgbase, pSubBoardCode, pByIdx, pMsgIdxOrNum, pExpandFields)
{
	var msgbaseIsOpen = false;
	var msgbase = null;
	var msgHdr = null;
	if (pMsgbase == null)
	{
		msgbase = new MsgBase(pSubBoardCode);
		msgbaseIsOpen = msgbase.open();
	}
	else
	{
		msgbase = pMsgbase;
		msgbaseIsOpen = pMsgbase.is_open;
	}
	if (msgbaseIsOpen)
	{
		var getMsgHdr = true;
		if (pByIdx)
			getMsgHdr = ((pMsgIdxOrNum >= 0) && (pMsgIdxOrNum < msgbase.total_msgs))
		if (getMsgHdr)
			msgHdr = msgbase.get_msg_header(pByIdx, pMsgIdxOrNum, pExpandFields, true); // Last true: Include votes
		if (pMsgbase == null)
			msgbase.close();
	}
	return msgHdr;
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

// Adds message attributes to a message header and saves it in the messagebase.
// To do that, this function first loads the messag header from the messagebase
// without expanded fields, applies the attributes, and then saves the header
// back to the messagebase.
//
// Parameters:
//  pMsgbaseOrSubCode: An open MessageBase object or a sub-board code (string)
//  pMsgNum: The number of the message to update
//  pMsgAttrs: The message attributes to apply to the message (numeric bitfield)
//
// Return value: An object containing the following properties:
//               saveSucceeded: Boolean - Whether or not the message header was successfully saved
//               msgAttrs: A numeric bitfield containing the updated attributes of the message header
function applyAttrsInMsgHdrInMessagbase(pMsgbaseOrSubCode, pMsgNum, pMsgAttrs)
{
	var retObj = {
		saveSucceeded: false,
		msgAttrs: 0
	};

	var msgbaseOpen = false;
	var msgbase = null;
	if (typeof(pMsgbaseOrSubCode) == "object")
	{
		msgbase = pMsgbaseOrSubCode;
		msgbaseOpen = msgbase.is_open;
	}
	else if (typeof(pMsgbaseOrSubCode) == "string")
	{
		msgbase = new MsgBase(pMsgbaseOrSubCode);
		msgbaseOpen = msgbase.open();
	}
	else
		return retObj;

	if (msgbaseOpen)
	{
		// Get the message header without expanded fields (we can't save it with
		// expanded fields), then add the 'read' attribute and save it back to the messagebase.
		var msgHdr = msgbase.get_msg_header(false, pMsgNum, false);
		if (msgHdr != null)
		{
			msgHdr.attr |= pMsgAttrs;
			// TODO: Occasional when going to next message area:
			// Error: Error -110 adding SENDERNETADDR field to message header
			retObj.saveSucceeded = msgbase.put_msg_header(false, pMsgNum, msgHdr);
			if (retObj.saveSucceeded)
				retObj.msgAttrs = msgHdr.attr;
			else
			{
				writeToSysAndNodeLog("Failed to save message header with the following attributes: " + msgAttrsToString(pMsgAttrs), LOG_ERR);
				writeToSysAndNodeLog(getMsgAreaDescStr(msgbase), LOG_ERR);
				writeToSysAndNodeLog(format("Message offset: %d, number: %d", msgHdr.offset, msgHdr.number), LOG_ERR);
				writeToSysAndNodeLog("Status: " + msgbase.status, LOG_ERR);
				writeToSysAndNodeLog("Error: " + msgbase.error, LOG_ERR);
				/*
				// For sysops, output a debug message
				if (user.is_sysop)
				{
					console.print("\x01n");
					console.crlf();
					console.print("* Failed to save msg header the with the following attributes: " + msgAttrsToString(pMsgAttrs));
					console.crlf();
					console.print("Status: " + msgbase.status);
					console.crlf();
					console.print("Error: " + msgbase.error);
					console.crlf();
					console.crlf();
					//console.print("put_msg_header params: false, " + msgHdr.number + ", header:\r\n");
					//console.print("put_msg_header params: true, " + msgHdr.offset + ", header:\r\n");
					//console.print("put_msg_header params: " + msgHdr.number + ", header:\r\n");
					printMsgHdr(msgHdr);
				}
				*/
			}
		}

		// If a sub-board code was passed in, then close the messagebase object
		// that we created here.
		if (typeof(pMsgbaseOrSubCode) == "string")
			msgbase.close();
	}

	return retObj;
}

// Converts a message attributes bitfield to a string.
//
// Parameters:
//  pMsgAttrs: A numeric type with message attribute bits
//
// Return value: A string containing a list of the message attributes
function msgAttrsToString(pMsgAttrs)
{
	if (typeof(pMsgAttrs) != "number")
		return "";

	var attrsStr = "";
	if ((pMsgAttrs & MSG_PRIVATE) == MSG_PRIVATE)
	{
		if (attrsStr.length > 0)
			attrsStr += ", ";
		attrsStr += "MSG_PRIVATE";
	}
	if ((pMsgAttrs & MSG_READ) == MSG_READ)
	{
		if (attrsStr.length > 0)
			attrsStr += ", ";
		attrsStr += "MSG_READ";
	}
	if ((pMsgAttrs & MSG_PERMANENT) == MSG_PERMANENT)
	{
		if (attrsStr.length > 0)
			attrsStr += ", ";
		attrsStr += "MSG_PERMANENT";
	}
	if ((pMsgAttrs & MSG_LOCKED) == MSG_LOCKED)
	{
		if (attrsStr.length > 0)
			attrsStr += ", ";
		attrsStr += "MSG_LOCKED";
	}
	if ((pMsgAttrs & MSG_DELETE) == MSG_DELETE)
	{
		if (attrsStr.length > 0)
			attrsStr += ", ";
		attrsStr += "MSG_DELETE";
	}
	if ((pMsgAttrs & MSG_ANONYMOUS) == MSG_ANONYMOUS)
	{
		if (attrsStr.length > 0)
			attrsStr += ", ";
		attrsStr += "MSG_ANONYMOUS";
	}
	if ((pMsgAttrs & MSG_KILLREAD) == MSG_KILLREAD)
	{
		if (attrsStr.length > 0)
			attrsStr += ", ";
		attrsStr += "MSG_KILLREAD";
	}
	if ((pMsgAttrs & MSG_MODERATED) == MSG_MODERATED)
	{
		if (attrsStr.length > 0)
			attrsStr += ", ";
		attrsStr += "MSG_MODERATED";
	}
	if ((pMsgAttrs & MSG_VALIDATED) == MSG_VALIDATED)
	{
		if (attrsStr.length > 0)
			attrsStr += ", ";
		attrsStr += "MSG_VALIDATED";
	}
	if ((pMsgAttrs & MSG_REPLIED) == MSG_REPLIED)
	{
		if (attrsStr.length > 0)
			attrsStr += ", ";
		attrsStr += "MSG_REPLIED";
	}
	if ((pMsgAttrs & MSG_NOREPLY) == MSG_NOREPLY)
	{
		if (attrsStr.length > 0)
			attrsStr += ", ";
		attrsStr += "MSG_NOREPLY";
	}
	if ((pMsgAttrs & MSG_UPVOTE) == MSG_UPVOTE)
	{
		if (attrsStr.length > 0)
			attrsStr += ", ";
		attrsStr += "MSG_UPVOTE";
	}
	if ((pMsgAttrs & MSG_DOWNVOTE) == MSG_DOWNVOTE)
	{
		if (attrsStr.length > 0)
			attrsStr += ", ";
		attrsStr += "MSG_DOWNVOTE";
	}
	if ((pMsgAttrs & MSG_POLL) == MSG_POLL)
	{
		if (attrsStr.length > 0)
			attrsStr += ", ";
		attrsStr += "MSG_POLL";
	}
	return attrsStr;
}

// Returns the index of the first Synchronet attribute code before a given index
// in a string.
//
// Parameters:
//  pStr: The string to search in
//  pIdx: The index to search back from
//  pSeriesOfAttrs: Optional boolean - Whether or not to look for a series of
//                  attributes.  Defaults to false (look for just one attribute).
//  pOnlyInWord: Optional boolean - Whether or not to look only in the current word
//               (with words separated by whitespace).  Defaults to false.
//
// Return value: The index of the first Synchronet attribute code before the given
//               index in the string, or -1 if there is none or if the parameters
//               are invalid
function strIdxOfSyncAttrBefore(pStr, pIdx, pSeriesOfAttrs, pOnlyInWord)
{
	if (typeof(pStr) != "string")
		return -1;
	if (typeof(pIdx) != "number")
		return -1;
	if ((pIdx < 0) || (pIdx >= pStr.length))
		return -1;

	var seriesOfAttrs = (typeof(pSeriesOfAttrs) == "boolean" ? pSeriesOfAttrs : false);
	var onlyInWord = (typeof(pOnlyInWord) == "boolean" ? pOnlyInWord : false);

	var attrCodeIdx = pStr.lastIndexOf("\x01", pIdx-1);
	if (attrCodeIdx > -1)
	{
		// If we are to only check the current word, then continue only if
		// there isn't a space between the attribute code and the given index.
		if (onlyInWord)
		{
			if (pStr.lastIndexOf(" ", pIdx-1) >= attrCodeIdx)
				attrCodeIdx = -1;
		}
	}
	if (attrCodeIdx > -1)
	{
		var syncAttrRegexWholeWord = /^\x01[krgybmcw01234567hinpq,;\.dtl<>\[\]asz]$/i;
		if (syncAttrRegexWholeWord.test(pStr.substr(attrCodeIdx, 2)))
		{
			if (seriesOfAttrs)
			{
				for (var i = attrCodeIdx - 2; i >= 0; i -= 2)
				{
					if (syncAttrRegexWholeWord.test(pStr.substr(i, 2)))
						attrCodeIdx = i;
					else
						break;
				}
			}
		}
		else
			attrCodeIdx = -1;
	}
	return attrCodeIdx;
}

// Returns a string with any Synchronet color/attribute codes found in a string
// before a given index.
//
// Parameters:
//  pStr: The string to search in
//  pIdx: The index in the string to search before
//
// Return value: A string containing any Synchronet attribute codes found before
//               the given index in the given string
function getAttrsBeforeStrIdx(pStr, pIdx)
{
	if (typeof(pStr) != "string")
		return "";
	if (typeof(pIdx) != "number")
		return "";
	if (pIdx < 0)
		return "";

	var idx = (pIdx < pStr.length ? pIdx : pStr.length-1);
	var attrStartIdx = strIdxOfSyncAttrBefore(pStr, idx, true, false);
	var attrEndIdx = strIdxOfSyncAttrBefore(pStr, idx, false, false); // Start of 2-character code
	var attrsStr = "";
	if ((attrStartIdx > -1) && (attrEndIdx > -1))
		attrsStr = pStr.substring(attrStartIdx, attrEndIdx+2);
	return attrsStr;
}

// Given a message header, this function gets/calculates the message's
// upvotes, downvotes, and vote score, if that information is present.
//
// Parameters:
//  pMsgHdr: A message header object
//
// Return value: An object containign the following properties:
//               foundVoteInfo: Boolean - Whether the vote information exited in the header
//               upvotes: The number of upvotes
//               downvotes: The number of downvotes
//               voteScore: The overall vote score
function getMsgUpDownvotesAndScore(pMsgHdr, pVerbose)
{
	var retObj = {
		foundVoteInfo: false,
		upvotes: 0,
		downvotes: 0,
		voteScore: 0
	};

 	if ((pMsgHdr.hasOwnProperty("total_votes") || pMsgHdr.hasOwnProperty("downvotes")) && pMsgHdr.hasOwnProperty("upvotes"))
	{
		retObj.foundVoteInfo = true;
		retObj.upvotes = pMsgHdr.upvotes;
		if (pMsgHdr.hasOwnProperty("downvotes"))
			retObj.downvotes = pMsgHdr.downvotes;
		else
			retObj.downvotes = pMsgHdr.total_votes - pMsgHdr.upvotes;
		retObj.voteScore = pMsgHdr.upvotes - retObj.downvotes;
		if (pVerbose && user.is_sysop)
		{
			console.print("\x01n\r\n");
			console.print("Vote information from header:\r\n");
			console.print("Upvotes: " + pMsgHdr.upvotes + "\r\n");
			console.print("Downvotes: " + retObj.downvotes + "\r\n");
			console.print("Score: " + retObj.voteScore + "\r\n");
			console.pause();
		}
	}
	else
	{
		if (pVerbose && user.is_sysop)
			console.print("\x01n\r\nMsg header does NOT have needed vote info\r\n\x01p");
	}

	return retObj;
}

// Removes any initial Synchronet attribute(s) from a message body,
// which can sometimes color the whole message.
//
// Parameters:
//  pMsgBody: The original message body
//
// Return value: The message body, with any initial color removed
function removeInitialColorFromMsgBody(pMsgBody)
{
	if (pMsgBody == null)
		return "";

	var msgBody = pMsgBody;

	var msgBodyLines = pMsgBody.split("\r\n", 3);
	if (msgBodyLines.length == 3)
	{
		var onlySyncAttrsRegexWholeWord = new RegExp("^(\x01[krgybmcw01234567hinpq,;\.dtl<>\[\]asz])+$", 'i');
		var line1Match = /^  Re: .*/.test(strip_ctrl(msgBodyLines[0]));
		var line2Match = /^  By: .* on .*/.test(strip_ctrl(msgBodyLines[1]));
		var line3OnlySyncAttrs = onlySyncAttrsRegexWholeWord.test(msgBodyLines[2]);
		if (line1Match && line2Match)
		{
			msgBodyLines = pMsgBody.split("\r\n");
			msgBodyLines[0] = strip_ctrl(msgBodyLines[0]);
			msgBodyLines[1] = strip_ctrl(msgBodyLines[1]);
			if (line3OnlySyncAttrs)
			{
				var originalLine3SyncAttrs = msgBodyLines[2];
				msgBodyLines[2] = strip_ctrl(msgBodyLines[2]);
				// If the first word of the 4th line is only Synchronet attribute codes,
				// and they're the same as the codes on the 3rd line, then remove them.
				if (msgBodyLines.length >= 4)
				{
					var line4Words = msgBodyLines[3].split(" ");
					if ((line4Words.length > 0) && onlySyncAttrsRegexWholeWord.test(line4Words[0]) && (line4Words[0] == originalLine3SyncAttrs))
						msgBodyLines[3] = msgBodyLines[3].substr(line4Words[0].length);
				}
			}
			msgBody = "";
			for (var i = 0; i < msgBodyLines.length; ++i)
				msgBody += msgBodyLines[i] + "\r\n";
			// Remove the trailing \r\n characters from msgBody
			msgBody = msgBody.substr(0, msgBody.length-2);
		}
	}

	return msgBody;
}

// Finds a user with a name, alias, or handle matching a given string.
// If system.matchuser() can't find it, this will iterate through all users
// to find the first user with a name, alias, or handle matching the given
// name.
function findUserNumWithName(pName)
{
	if (typeof(pName) !== "string" || pName.length == 0)
		return 0;

	var userNum = system.matchuser(pName);
	if (userNum == 0)
	{
		try
		{
			userNum = system.matchuserdata(U_NAME, pName);
		}
		catch (error) {}
	}
	if (userNum == 0)
	{
		try
		{
			userNum = system.matchuserdata(U_ALIAS, pName);
		}
		catch (error) {}
	}
	if (userNum == 0)
	{
		try
		{
			userNum = system.matchuserdata(U_HANDLE, pName);
		}
		catch (error) {}
	}
	return userNum;
}

// Inputs a string from the user, with a timeout
//
// Parameters:
//  pMode: The mode bits to use for the input (i.e., defined in sbbsdefs.js)
//  pMaxLength: The maximum length of the string (0 or less for no limit)
//  pTimeout: The timeout (in milliseconds).  When the timeout is reached,
//            input stops and the user's input is returned.
//
// Return value: The user's input (string)
function getStrWithTimeout(pMode, pMaxLength, pTimeout)
{
	var inputStr = "";

	var mode = K_NONE;
	if (typeof(pMode) == "number")
		mode = pMode;
	var maxWidth = 0;
	if (typeof(pMaxLength) == "number")
			maxWidth = pMaxLength;
	var timeout = 0;
	if (typeof(pTimeout) == "number")
		timeout = pTimeout;

	var setNormalAttrAtEnd = false;
	if (((mode & K_LINE) == K_LINE) && (maxWidth > 0) && console.term_supports(USER_ANSI))
	{
		var curPos = console.getxy();
		printf("\x01n\x01w\x01h\x01" + "4%" + maxWidth + "s", "");
		console.gotoxy(curPos);
		setNormalAttrAtEnd = true;
	}

	var curPos = console.getxy();
	var userKey = "";
	do
	{
		userKey = console.inkey(mode, timeout);
		if ((userKey.length > 0) && isPrintableChar(userKey))
		{
			var allowAppendChar = true;
			if ((maxWidth > 0) && (inputStr.length >= maxWidth))
				allowAppendChar = false;
			if (allowAppendChar)
			{
				inputStr += userKey;
				console.print(userKey);
				++curPos.x;
			}
		}
		else if (userKey == BACKSPACE)
		{
			if (inputStr.length > 0)
			{
				inputStr = inputStr.substr(0, inputStr.length-1);
				console.gotoxy(curPos.x-1, curPos.y);
				console.print(" ");
				console.gotoxy(curPos.x-1, curPos.y);
				--curPos.x;
			}
		}
		else if (userKey == KEY_ENTER)
			userKey = "";
	} while(userKey.length > 0);

	if (setNormalAttrAtEnd)
		console.print("\x01n");

	return inputStr;
}

// Calculates the page number (1-based) and top index for the page (0-based),
// given an item index.
//
// Parameters:
//  pItemIdx: The index of the item
//  pNumItemsPerPage: The number of items per page
//
// Return value: An object containing the following properties:
//               pageNum: The page number of the item (1-based; will be 0 if not found)
//               pageTopIdx: The index of the top item on the page (or -1 if not found)
function calcPageNumAndTopPageIdx(pItemIdx, pNumItemsPerPage)
{
	var retObj = {
		pageNum: 0,
		pageTopIdx: -1
	};

	var pageNum = 1;
	var topIdx = 0;
	var continueOn = true;
	do
	{
		var endIdx = topIdx + pNumItemsPerPage;
		if ((pItemIdx >= topIdx) && (pItemIdx < endIdx))
		{
			continueOn = false;
			retObj.pageNum = pageNum;
			retObj.pageTopIdx = topIdx;
		}
		else
		{
			++pageNum;
			topIdx = endIdx;
		}
	} while (continueOn);

	return retObj;
}

// Finds the page number of a message group or sub-board, given some text to
// search for.
//
// Parameters:
//  pText: The text to search for in the items
//  pNumItemsPerPage: The number of items per page
//  pSubBoard: Boolean - If true, search the sub-board list for the given group index.
//             If false, search the group list.
//  pStartItemIdx: The item index to start at
//  pGrpIdx: The index of the group to search in (only for doing a sub-board search)
//
// Return value: An object containing the following properties:
//               pageNum: The page number of the item (1-based; will be 0 if not found)
//               pageTopIdx: The index of the top item on the page (or -1 if not found)
//               itemIdx: The index of the item (or -1 if not found)
function getMsgAreaPageNumFromSearch(pText, pNumItemsPerPage, pSubBoard, pStartItemIdx, pGrpIdx)
{
	var retObj = {
		pageNum: 0,
		pageTopIdx: -1,
		itemIdx: -1
	};

	// Sanity checking
	if ((typeof(pText) != "string") || (typeof(pNumItemsPerPage) != "number") || (typeof(pSubBoard) != "boolean"))
		return retObj;

	// Convert the text to uppercase for case-insensitive searching
	var srchText = pText.toUpperCase();
	if (pSubBoard)
	{
		if ((typeof(pGrpIdx) == "number") && (pGrpIdx >= 0) && (pGrpIdx < msg_area.grp_list.length))
		{
			// Go through the sub-board list of the given group and
			// search for text in the descriptions
			for (var i = pStartItemIdx; i < msg_area.grp_list[pGrpIdx].sub_list.length; ++i)
			{
				if ((msg_area.grp_list[pGrpIdx].sub_list[i].description.toUpperCase().indexOf(srchText) > -1) ||
				    (msg_area.grp_list[pGrpIdx].sub_list[i].name.toUpperCase().indexOf(srchText) > -1))
				{
					retObj.itemIdx = i;
					// Figure out the page number and top index for the page
					var pageObj = calcPageNumAndTopPageIdx(i, pNumItemsPerPage);
					if ((pageObj.pageNum > 0) && (pageObj.pageTopIdx > -1))
					{
						retObj.pageNum = pageObj.pageNum;
						retObj.pageTopIdx = pageObj.pageTopIdx;
					}
					break;
				}
			}
		}
	}
	else
	{
		// Go through the message group list and look for a match
		for (var i = pStartItemIdx; i < msg_area.grp_list.length; ++i)
		{
			if ((msg_area.grp_list[i].name.toUpperCase().indexOf(srchText) > -1) ||
			    (msg_area.grp_list[i].description.toUpperCase().indexOf(srchText) > -1))
			{
				retObj.itemIdx = i;
				// Figure out the page number and top index for the page
				var pageObj = calcPageNumAndTopPageIdx(i, pNumItemsPerPage);
				if ((pageObj.pageNum > 0) && (pageObj.pageTopIdx > -1))
				{
					retObj.pageNum = pageObj.pageNum;
					retObj.pageTopIdx = pageObj.pageTopIdx;
				}
				break;
			}
		}
	}

	return retObj;
}

// Finds a message group index with search text, matching either the name or
// description, case-insensitive.
//
// Parameters:
//  pSearchText: The name/description text to look for
//  pStartItemIdx: The item index to start at.  Defaults to 0
//
// Return value: The index of the message group, or -1 if not found
function findMsgGrpIdxFromText(pSearchText, pStartItemIdx)
{
	if (typeof(pSearchText) != "string")
		return -1;

	var grpIdx = -1;

	var startIdx = (typeof(pStartItemIdx) == "number" ? pStartItemIdx : 0);
	if ((startIdx < 0) || (startIdx > msg_area.grp_list.length))
		startIdx = 0;

	// Go through the message group list and look for a match
	var searchTextUpper = pSearchText.toUpperCase();
	for (var i = startIdx; i < msg_area.grp_list.length; ++i)
	{
		if ((msg_area.grp_list[i].name.toUpperCase().indexOf(searchTextUpper) > -1) ||
		    (msg_area.grp_list[i].description.toUpperCase().indexOf(searchTextUpper) > -1))
		{
			grpIdx = i;
			break;
		}
	}

	return grpIdx;
}

// Finds a message group index with search text, matching either the name or
// description, case-insensitive.
//
// Parameters:
//  pGrpIdx: The index of the message group
//  pSearchText: The name/description text to look for
//  pStartItemIdx: The item index to start at.  Defaults to 0
//
// Return value: The index of the message group, or -1 if not found
function findSubBoardIdxFromText(pGrpIdx, pSearchText, pStartItemIdx)
{
	if (typeof(pGrpIdx) != "number")
		return -1;
	if (typeof(pSearchText) != "string")
		return -1;

	var subBoardIdx = -1;

	var startIdx = (typeof(pStartItemIdx) == "number" ? pStartItemIdx : 0);
	if ((startIdx < 0) || (startIdx > msg_area.grp_list[pGrpIdx].sub_list.length))
		startIdx = 0;

	// Go through the message group list and look for a match
	var searchTextUpper = pSearchText.toUpperCase();
	for (var i = startIdx; i < msg_area.grp_list[pGrpIdx].sub_list.length; ++i)
	{
		if ((msg_area.grp_list[pGrpIdx].sub_list[i].name.toUpperCase().indexOf(searchTextUpper) > -1) ||
		    (msg_area.grp_list[pGrpIdx].sub_list[i].description.toUpperCase().indexOf(searchTextUpper) > -1))
		{
			subBoardIdx = i;
			break;
		}
	}

	return subBoardIdx;
}

// Searches for a @MSG_TO @-code in a string and inserts a color/attribute code
// before the @-code in the string.
//
// Parameters:
//  pStr: The string to look in
//  pToUserColor: The color/attribute code to insert before the @MSG_TO @-code
//
// Return value: A string with the given color/attribute code inserted before the
//               @MSG_TO @-code
function strWithToUserColor(pStr, pToUserColor)
{
	if ((typeof(pStr) != "string") || (typeof(pToUserColor) != "string"))
		return "";
	if (pToUserColor.length == 0)
		return pStr;

	// Find start & end indexes of a @MSG_TO* @-code, i.e.,
	// @MSG_TO, @MSG_TO_NAME, @MSG_TO_EXT, @MSG_TO_NET
	var toCodeStartIdx = pStr.indexOf("@MSG_TO");
	if (toCodeStartIdx < 0)
		return pStr;
	// Insert the color in the right position and return the line
	return pStr.substr(0, toCodeStartIdx) + "\x01n" + pToUserColor + pStr.substr(toCodeStartIdx) + "\x01n";
	/*
	// Insert the color in the right position, and
	// put a \x01n right after the end of the @-code
	var str = "";
	var toCodeEndIdx = pStr.indexOf("@", toCodeStartIdx+1);
	if (toCodeEndIdx >= 0)
	{
		str = pStr.substr(0, toCodeStartIdx) + "\x01n" + pToUserColor + pStr.substr(toCodeStartIdx, toCodeEndIdx-toCodeStartIdx+1)
		    + "\x01n" + pStr.substr(toCodeEndIdx);
	}
	else
		str = pStr.substr(0, toCodeStartIdx) + "\x01n" + pToUserColor + pStr.substr(toCodeStartIdx) + "\x01n";
	return str;
	*/
}

// Gets the value of the user's current scan_ptr in a sub-board, or if it's
// 0xffffffff, returns the message number of the last readable message in
// the sub-board (this is the message number, not the index).
//
// Parameters:
//  pSubCode: A sub-board internal code
//
// Return value: The user's scan_ptr value or the message number of the
//               last readable message in the sub-board
function GetScanPtrOrLastMsgNum(pSubCode)
{
	var msgNumToReturn = 0;
	// If pMsgNum is 4294967295 (0xffffffff, or ~0), that is a special value
	// for the user's scan_ptr meaning it should point to the latest message
	// in the messagebase.
	if (msg_area.sub[pSubCode].scan_ptr != 0xffffffff)
		msgNumToReturn = msg_area.sub[pSubCode].scan_ptr;
	else
	{
		var msgbase = new MsgBase(pSubCode);
		if (msgbase.open())
		{
			var numMsgs = msgbase.total_msgs;
			for (var msgIdx = numMsgs - 1; msgIdx >= 0; --msgIdx)
			{
				var msgHdr = msgbase.get_msg_header(true, msgIdx);
				if ((msgHdr != null) && ((msgHdr.attr & MSG_DELETE) == 0))
				{
					msgNumToReturn = msgHdr.number;
					break;
				}
			}

			msgbase.close();
		}
	}

	return msgNumToReturn;
}

// Returns whether a message header has one of the attachment flags
// enabled (for Synchtonet 3.17 or newer).
//
// Parameters:
//  pMsgHdr: A message header (returned from MsgBase.get_msg_header())
//
// Return value: Boolean - Whether or not the message has one of the attachment flags
function msgHdrHasAttachmentFlag(pMsgHdr)
{
	if (typeof(pMsgHdr) !== "object" || typeof(pMsgHdr.auxattr) === "undefined")
		return false;

	var attachmentFlag = false;
	if (typeof(MSG_FILEATTACH) !== "undefined" && typeof(MSG_MIMEATTACH) !== "undefined")
		attachmentFlag = (pMsgHdr.auxattr & (MSG_FILEATTACH|MSG_MIMEATTACH)) > 0;
	return attachmentFlag;
}

// Allows the user to download a message and its attachments, using the newer
// Synchronet interface (the function bbs.download_msg_attachments() must exist).
//
// Parameters:
//  pMsgHdr: The message header
//  pSubCode: The sub-board code that the message is in
function allowUserToDownloadMessage_NewInterface(pMsgHdr, pSubCode)
{
	if (typeof(bbs.download_msg_attachments) !== "function")
		return;
	if (typeof(pSubCode) !== "string")
		return;
	if (typeof(pMsgHdr) !== "object" || typeof(pMsgHdr.number) == "undefined")
		return;

	var msgBase = new MsgBase(pSubCode);
	if (msgBase.open())
	{
		// bbs.download_msg_attachments() requires a message header returned
		// by MsgBase.get_msg_header()
		var msgHdrForDownloading = msgBase.get_msg_header(false, pMsgHdr.number, false);
		// Allow the user to download the message
		if (!console.noyes("Download message", P_NOCRLF))
		{
			if (!download_msg(msgHdrForDownloading, msgBase, console.yesno("Plain-text only")))
				console.print("\x01n\r\nFailed\r\n");
		}
		// Allow the user to download the attachments
		console.creturn();
		bbs.download_msg_attachments(msgHdrForDownloading);
		msgBase.close();
	}
}

// From msglist.js - Prompts the user if they want to download the message text
function download_msg(msg, msgbase, plain_text)
{
	var fname = system.temp_dir + "msg_" + msg.number + ".txt";
	var f = new File(fname);
	if(!f.open("wb"))
		return false;
	var text = msgbase.get_msg_body(msg
				,/* strip ctrl-a */false
				,/* dot-stuffing */false
				,/* tails */true
				,plain_text);
	f.write(msg.get_rfc822_header(/* force_update: */false, /* unfold: */false
		,/* default_content_type */!plain_text));
	f.writeln(text);
	f.close();
	return bbs.send_file(fname);
}


////////// Message list sort functions

// For sorting message headers by date & time
//
// Parameters:
//  msgHdrA: The first message header
//  msgHdrB: The second message header
//
// Return value: -1, 0, or 1, depending on whether header A comes before,
//               is equal to, or comes after header B
function sortMessageHdrsByDateTime(msgHdrA, msgHdrB)
{
	// Return -1, 0, or 1, depending on whether msgHdrA's date & time comes
	// before, is equal to, or comes after msgHdrB's date & time
	// Convert when_written_time to local time before comparing the times
	var localWrittenTimeA = msgWrittenTimeToLocalBBSTime(msgHdrA);
	var localWrittenTimeB = msgWrittenTimeToLocalBBSTime(msgHdrB);
	var yearA = +strftime("%Y", localWrittenTimeA);
	var monthA = +strftime("%m", localWrittenTimeA);
	var dayA = +strftime("%d", localWrittenTimeA);
	var hourA = +strftime("%H", localWrittenTimeA);
	var minuteA = +strftime("%M", localWrittenTimeA);
	var secondA = +strftime("%S", localWrittenTimeA);
	var yearB = +strftime("%Y", localWrittenTimeB);
	var monthB = +strftime("%m", localWrittenTimeB);
	var dayB = +strftime("%d", localWrittenTimeB);
	var hourB = +strftime("%H", localWrittenTimeB);
	var minuteB = +strftime("%M", localWrittenTimeB);
	var secondB = +strftime("%S", localWrittenTimeB);
	if (yearA < yearB)
		return -1;
	else if (yearA > yearB)
		return 1;
	else
	{
		if (monthA < monthB)
			return -1;
		else if (monthA > monthB)
			return 1;
		else
		{
			if (dayA < dayB)
				return -1;
			else if (dayA > dayB)
				return 1;
			else
			{
				if (hourA < hourB)
					return -1;
				else if (hourA > hourB)
					return 1;
				else
				{
					if (minuteA < minuteB)
						return -1;
					else if (minuteA > minuteB)
						return 1;
					else
					{
						if (secondA < secondB)
							return -1;
						else if (secondA > secondB)
							return 1;
						else
							return 0;
					}
				}
			}
		}
	}
}

// Returns an array of internal sub-board codes to scan for a given scan scope.
//
// Parameters:
//  pScanScopeChar: A string specifying "A" for all sub-boards, "G" for current
//                  message group sub-boards, or "S" for the current sub-board
//
// Return value: An array of internal sub-board codes for sub-boards to scan
function getSubBoardsToScanArray(pScanScopeChar)
{
	var subBoardsToScan = [];
	if (pScanScopeChar == "A") // All sub-board scan
	{
		for (var grpIndex = 0; grpIndex < msg_area.grp_list.length; ++grpIndex)
		{
			for (var subIndex = 0; subIndex < msg_area.grp_list[grpIndex].sub_list.length; ++subIndex)
				subBoardsToScan.push(msg_area.grp_list[grpIndex].sub_list[subIndex].code);
		}
	}
	else if (pScanScopeChar == "G") // Group scan
	{
		for (var subIndex = 0; subIndex < msg_area.grp_list[bbs.curgrp].sub_list.length; ++subIndex)
			subBoardsToScan.push(msg_area.grp_list[bbs.curgrp].sub_list[subIndex].code);
	}
	else if (pScanScopeChar == "S") // Current sub-board scan
		subBoardsToScan.push(bbs.cursub_code);
	return subBoardsToScan;
}

// Returns whether a user number is valid (only an actual, active user)
//
// Parameters:
//  pUserNum: A user number
//
// Return value: Boolean - Whether or not the given user number is valid
function isValidUserNum(pUserNum)
{
	if (typeof(pUserNum) !== "number")
		return false;
	if (pUserNum < 1 || pUserNum > system.lastuser)
		return false;

	var userIsValid = false;
	var theUser = new User(pUserNum);
	if (theUser != null && (theUser.settings & USER_DELETED) == 0 && (theUser.settings & USER_INACTIVE) == 0)
		userIsValid = true;
	return userIsValid;
}

// Returns the index of the last ANSI code in a string.
//
// Parameters:
//  pStr: The string to search in
//  pANSIRegexes: An array of regular expressions to use for searching for ANSI codes
//
// Return value: The index of the last ANSI code in the string, or -1 if not found
function idxOfLastANSICode(pStr, pANSIRegexes)
{
	var lastANSIIdx = -1;
	for (var i = 0; i < pANSIRegexes.length; ++i)
	{
		var lastANSIIdxTmp = regexLastIndexOf(pStr, pANSIRegexes[i]);
		if (lastANSIIdxTmp > lastANSIIdx)
			lastANSIIdx = lastANSIIdxTmp;
	}
	return lastANSIIdx;
}

// Returns the index of the first ANSI code in a string.
//
// Parameters:
//  pStr: The string to search in
//  pANSIRegexes: An array of regular expressions to use for searching for ANSI codes
//
// Return value: The index of the first ANSI code in the string, or -1 if not found
function idxOfFirstANSICode(pStr, pANSIRegexes)
{
	var firstANSIIdx = -1;
	for (var i = 0; i < pANSIRegexes.length; ++i)
	{
		var firstANSIIdxTmp = regexFirstIndexOf(pStr, pANSIRegexes[i]);
		if (firstANSIIdxTmp > firstANSIIdx)
			firstANSIIdx = firstANSIIdxTmp;
	}
	return firstANSIIdx;
}

// Returns the number of times an ANSI code is matched in a string.
//
// Parameters:
//  pStr: The string to search in
//  pANSIRegexes: An array of regular expressions to use for searching for ANSI codes
//
// Return value: The number of ANSI code matches in the string
function countANSICodes(pStr, pANSIRegexes)
{
	var ANSICount = 0;
	for (var i = 0; i < pANSIRegexes.length; ++i)
	{
		var matches = pStr.match(pANSIRegexes[i]);
		if (matches != null)
			ANSICount += matches.length;
	}
	return ANSICount;
}

// Removes ANSI codes from a string.
//
// Parameters:
//  pStr: The string to remove ANSI codes from
//  pANSIRegexes: An array of regular expressions to use for searching for ANSI codes
//
// Return value: A version of the string without ANSI codes
function removeANSIFromStr(pStr, pANSIRegexes)
{
	if (typeof(pStr) != "string")
		return "";

	var theStr = pStr;
	for (var i = 0; i < pANSIRegexes.length; ++i)
		theStr = theStr.replace(pANSIRegexes[i], "");
	return theStr;
}

// Returns the last index in a string where a regex is found.
// From this page:
// http://stackoverflow.com/questions/273789/is-there-a-version-of-javascripts-string-indexof-that-allows-for-regular-expr
//
// Parameters:
//  pStr: The string to search
//  pRegex: The regular expression to match in the string
//  pStartPos: Optional - The starting position in the string.  If this is not
//             passed, then the end of the string will be used.
//
// Return value: The last index in the string where the regex is found, or -1 if not found.
function regexLastIndexOf(pStr, pRegex, pStartPos)
{
	pRegex = (pRegex.global) ? pRegex : new RegExp(pRegex.source, "g" + (pRegex.ignoreCase ? "i" : "") + (pRegex.multiLine ? "m" : ""));
	if (typeof(pStartPos) == "undefined")
		pStartPos = pStr.length;
	else if (pStartPos < 0)
		pStartPos = 0;
	var stringToWorkWith = pStr.substring(0, pStartPos + 1);
	var lastIndexOf = -1;
	var nextStop = 0;
	while ((result = pRegex.exec(stringToWorkWith)) != null)
	{
		lastIndexOf = result.index;
		pRegex.lastIndex = ++nextStop;
	}
    return lastIndexOf;
}

// Returns the first index in a string where a regex is found.
//
// Parameters:
//  pStr: The string to search
//  pRegex: The regular expression to match in the string
//
// Return value: The first index in the string where the regex is found, or -1 if not found.
function regexFirstIndexOf(pStr, pRegex)
{
	pRegex = (pRegex.global) ? pRegex : new RegExp(pRegex.source, "g" + (pRegex.ignoreCase ? "i" : "") + (pRegex.multiLine ? "m" : ""));
	var indexOfRegex = -1;
	var nextStop = 0;
	while ((result = pRegex.exec(pStr)) != null)
	{
		indexOfRegex = result.index;
		pRegex.lastIndex = ++nextStop;
	}
    return indexOfRegex;
}

// Returns whether or not a string has any ASCII drawing characters (typically above ASCII value 127).
//
// Parameters:
//  pText: The text to check
//
// Return value: Boolean - Whether or not the text has any ASCII drawing characters
function textHasDrawingChars(pText)
{
	if (typeof(pText) !== "string" || pText.length == 0)
		return false;

	if (typeof(textHasDrawingChars.chars) === "undefined")
	{
		textHasDrawingChars.chars = [ascii(169), ascii(170)];
		for (var asciiVal = 174; asciiVal <= 223; ++asciiVal)
			textHasDrawingChars.chars.push(ascii(asciiVal));
		textHasDrawingChars.chars.push(ascii(254));
	}
	var drawingCharsFound = false;
	for (var i = 0; i < textHasDrawingChars.chars.length && !drawingCharsFound; ++i)
		drawingCharsFound = drawingCharsFound || (pText.indexOf(textHasDrawingChars.chars[i]) > -1);
	return drawingCharsFound;
}

// Returns a string with a character repeated a given number of times
//
// Parameters:
//  pChar: The character to repeat in the string
//  pNumTimes: The number of times to repeat the character
//
// Return value: A string with the given character repeated the given number of times
function charStr(pChar, pNumTimes)
{
	if (typeof(pChar) !== "string" || pChar.length == 0 || typeof(pNumTimes) !== "number" || pNumTimes < 1)
		return "";

	var str = "";
	for (var i = 0; i < pNumTimes; ++i)
		str += pChar;
	return str;
}

// Returns whether the logged-in user can view deleted messages.
function canViewDeletedMsgs()
{
	var usersVDM = ((system.settings & SYS_USRVDELM) == SYS_USRVDELM);
	var sysopVDM = ((system.settings & SYS_SYSVDELM) == SYS_SYSVDELM);
	return (usersVDM || (user.is_sysop && sysopVDM));
}

///////////////////////////////////////////////////////////////////////////////////
// ChoiceScrollbox stuff (this was copied from SlyEdit_Misc.js; maybe there's a better way to do this)

// Returns the minimum width for a ChoiceScrollbox
function ChoiceScrollbox_MinWidth()
{
	return 73; // To leave room for the navigation text in the bottom border
}

// ChoiceScrollbox constructor
//
// Parameters:
//  pLeftX: The horizontal component (column) of the upper-left coordinate
//  pTopY: The vertical component (row) of the upper-left coordinate
//  pWidth: The width of the box (including the borders)
//  pHeight: The height of the box (including the borders)
//  pTopBorderText: The text to include in the top border
//  pCfgObj: The script/program configuration object (color settings are used)
//  pAddTCharsAroundTopText: Optional, boolean - Whether or not to use left & right T characters
//                           around the top border text.  Defaults to true.
// pReplaceTopTextSpacesWithBorderChars: Optional, boolean - Whether or not to replace
//                           spaces in the top border text with border characters.
//                           Defaults to false.
function ChoiceScrollbox(pLeftX, pTopY, pWidth, pHeight, pTopBorderText, pCfgObj,
                         pAddTCharsAroundTopText, pReplaceTopTextSpacesWithBorderChars)
{
	if (pCfgObj == null || typeof(pCfgObj) !== "object")
		pCfgObj = {};
	if (pCfgObj.colors == null || typeof(pCfgObj.colors) !== "object")
	{
		pCfgObj.colors = {
			listBoxBorder: "\x01n\x01g",
			listBoxBorderText: "\x01n\x01b\x01h",
			listBoxItemText: "\x01n\x01c",
			listBoxItemHighlight: "\x01n\x01" + "4\x01w\x01h"
		};
	}
	else
	{
		if (!pCfgObj.colors.hasOwnProperty("listBoxBorder"))
			pCfgObj.colors.listBoxBorder = "\x01n\x01g";
		if (!pCfgObj.colors.hasOwnProperty("listBoxBorderText"))
			pCfgObj.colors.listBoxBorderText = "\x01n\x01b\x01h";
		if (!pCfgObj.colors.hasOwnProperty("listBoxItemText"))
			pCfgObj.colors.listBoxItemText = "\x01n\x01c";
		if (!pCfgObj.colors.hasOwnProperty("listBoxItemHighlight"))
			pCfgObj.colors.listBoxItemHighlight = "\x01n\x01" + "4\x01w\x01h";
	}

	// The default is to add left & right T characters around the top border
	// text.  But also use pAddTCharsAroundTopText if it's a boolean.
	var addTopTCharsAroundText = true;
	if (typeof(pAddTCharsAroundTopText) == "boolean")
		addTopTCharsAroundText = pAddTCharsAroundTopText;
	// If pReplaceTopTextSpacesWithBorderChars is true, then replace the spaces
	// in pTopBorderText with border characters.
	if (pReplaceTopTextSpacesWithBorderChars)
	{
		var startIdx = 0;
		var firstSpcIdx = pTopBorderText.indexOf(" ", 0);
		// Look for the first non-space after firstSpaceIdx
		var nonSpcIdx = -1;
		for (var i = firstSpcIdx; (i < pTopBorderText.length) && (nonSpcIdx == -1); ++i)
		{
			if (pTopBorderText.charAt(i) != " ")
				nonSpcIdx = i;
		}
		var firstStrPart = "";
		var lastStrPart = "";
		var numSpaces = 0;
		while ((firstSpcIdx > -1) && (nonSpcIdx > -1))
		{
			firstStrPart = pTopBorderText.substr(startIdx, (firstSpcIdx-startIdx));
			lastStrPart = pTopBorderText.substr(nonSpcIdx);
			numSpaces = nonSpcIdx - firstSpcIdx;
			if (numSpaces > 0)
			{
				pTopBorderText = firstStrPart + "\x01n" + pCfgObj.colors.listBoxBorder;
				for (var i = 0; i < numSpaces; ++i)
					pTopBorderText += HORIZONTAL_SINGLE;
				pTopBorderText += "\x01n" + pCfgObj.colors.listBoxBorderText + lastStrPart;
			}

			// Look for the next space and non-space character after that.
			firstSpcIdx = pTopBorderText.indexOf(" ", nonSpcIdx);
			// Look for the first non-space after firstSpaceIdx
			nonSpcIdx = -1;
			for (var i = firstSpcIdx; (i < pTopBorderText.length) && (nonSpcIdx == -1); ++i)
			{
				if (pTopBorderText.charAt(i) != " ")
					nonSpcIdx = i;
			}
		}
	}

	this.programCfgObj = pCfgObj;

	var minWidth = ChoiceScrollbox_MinWidth();

	this.dimensions = {
		topLeftX: pLeftX,
		topLeftY: pTopY,
		width: 0,
		height: pHeight,
		bottomRightX: 0,
		bottomRightY: 0
	};
	// Make sure the width is the minimum width
	if ((pWidth < 0) || (pWidth < minWidth))
		this.dimensions.width = minWidth;
	else
		this.dimensions.width = pWidth;
	this.dimensions.bottomRightX = this.dimensions.topLeftX + this.dimensions.width - 1;
	this.dimensions.bottomRightY = this.dimensions.topLeftY + this.dimensions.height - 1;

	// The text item array and member variables relating to it and the items
	// displayed on the screen during the input loop
	this.txtItemList = [];
	this.chosenTextItemIndex = -1;
	this.topItemIndex = 0;
	this.bottomItemIndex = 0;

	// Top border string
	var innerBorderWidth = this.dimensions.width - 2;
	// Calculate the maximum top border text length to account for the left/right
	// T chars and "Page #### of ####" text
	var maxTopBorderTextLen = innerBorderWidth - (pAddTCharsAroundTopText ? 21 : 19);
	if (strip_ctrl(pTopBorderText).length > maxTopBorderTextLen)
		pTopBorderText = pTopBorderText.substr(0, maxTopBorderTextLen);
	this.topBorder = "\x01n" + pCfgObj.colors.listBoxBorder + UPPER_LEFT_SINGLE;
	if (addTopTCharsAroundText)
		this.topBorder += RIGHT_T_SINGLE;
	this.topBorder += "\x01n" + pCfgObj.colors.listBoxBorderText
	               + pTopBorderText + "\x01n" + pCfgObj.colors.listBoxBorder;
	if (addTopTCharsAroundText)
		this.topBorder += LEFT_T_SINGLE;
	const topBorderTextLen = strip_ctrl(pTopBorderText).length;
	var numHorizBorderChars = innerBorderWidth - topBorderTextLen - 20;
	if (addTopTCharsAroundText)
		numHorizBorderChars -= 2;
	for (var i = 0; i <= numHorizBorderChars; ++i)
		this.topBorder += HORIZONTAL_SINGLE;
	this.topBorder += RIGHT_T_SINGLE + "\x01n" + pCfgObj.colors.listBoxBorderText
	               + "Page    1 of    1" + "\x01n" + pCfgObj.colors.listBoxBorder + LEFT_T_SINGLE
	               + UPPER_RIGHT_SINGLE;

	// Bottom border string
	this.btmBorderNavText = "\x01n\x01h\x01c" + UP_ARROW + "\x01b, \x01c" + DOWN_ARROW + "\x01b, \x01cN\x01y)\x01bext, \x01cP\x01y)\x01brev, "
	                      + "\x01cF\x01y)\x01birst, \x01cL\x01y)\x01bast, \x01cHOME\x01b, \x01cEND\x01b, \x01cEnter\x01y=\x01bSelect, "
	                      + "\x01cESC\x01n\x01c/\x01h\x01cQ\x01y=\x01bEnd";
	this.bottomBorder = "\x01n" + pCfgObj.colors.listBoxBorder + LOWER_LEFT_SINGLE
	                  + RIGHT_T_SINGLE + this.btmBorderNavText + "\x01n" + pCfgObj.colors.listBoxBorder
	                  + LEFT_T_SINGLE;
	var numCharsRemaining = this.dimensions.width - strip_ctrl(this.btmBorderNavText).length - 6;
	for (var i = 0; i < numCharsRemaining; ++i)
		this.bottomBorder += HORIZONTAL_SINGLE;
	this.bottomBorder += LOWER_RIGHT_SINGLE;

	// Item format strings
	this.listIemFormatStr = "\x01n" + pCfgObj.colors.listBoxItemText + "%-"
	                      + +(this.dimensions.width-2) + "s";
	this.listIemHighlightFormatStr = "\x01n" + pCfgObj.colors.listBoxItemHighlight + "%-"
	                               + +(this.dimensions.width-2) + "s";

	// Key functionality override function pointers
	this.enterKeyOverrideFn = null;

	// inputLoopeExitKeys is an object containing additional keypresses that will
	// exit the input loop.
	this.inputLoopExitKeys = {};

	// For drawing the menu
	this.pageNum = 0;
	this.numPages = 0;
	this.numItemsPerPage = 0;
	this.maxItemWidth = 0;
	this.pageNumTxtStartX = 0;

	// Input loop quit override (to be used in overridden enter function if needed to quit the input loop there
	this.continueInputLoopOverride = true;

	// Object functions
	this.addTextItem = ChoiceScrollbox_AddTextItem; // Returns the index of the item
	this.getTextItem = ChoiceScrollbox_GetTextIem;
	this.replaceTextItem = ChoiceScrollbox_ReplaceTextItem;
	this.delTextItem = ChoiceScrollbox_DelTextItem;
	this.chgCharInTextItem = ChoiceScrollbox_ChgCharInTextItem;
	this.getChosenTextItemIndex = ChoiceScrollbox_GetChosenTextItemIndex;
	this.setItemArray = ChoiceScrollbox_SetItemArray; // Sets the item array; returns whether or not it was set.
	this.clearItems = ChoiceScrollbox_ClearItems; // Empties the array of items
	this.setEnterKeyOverrideFn = ChoiceScrollbox_SetEnterKeyOverrideFn;
	this.clearEnterKeyOverrideFn = ChoiceScrollbox_ClearEnterKeyOverrideFn;
	this.addInputLoopExitKey = ChoiceScrollbox_AddInputLoopExitKey;
	this.setBottomBorderText = ChoiceScrollbox_SetBottomBorderText;
	this.drawBorder = ChoiceScrollbox_DrawBorder;
	this.drawInnerMenu = ChoiceScrollbox_DrawInnerMenu;
	this.refreshOnScreen = ChoiceScrollbox_RefreshOnScreen;
	this.refreshItemCharOnScreen = ChoiceScrollbox_RefreshItemCharOnScreen;
	// Does the input loop.  Returns an object with the following properties:
	//  itemWasSelected: Boolean - Whether or not an item was selected
	//  selectedIndex: The index of the selected item
	//  selectedItem: The text of the selected item
	//  lastKeypress: The last key pressed by the user
	this.doInputLoop = ChoiceScrollbox_DoInputLoop;
}
function ChoiceScrollbox_AddTextItem(pTextLine, pStripCtrl)
{
   var stripCtrl = true;
   if (typeof(pStripCtrl) == "boolean")
      stripCtrl = pStripCtrl;

   if (stripCtrl)
      this.txtItemList.push(strip_ctrl(pTextLine));
   else
      this.txtItemList.push(pTextLine);
   // Return the index of the added item
   return this.txtItemList.length-1;
}
function ChoiceScrollbox_GetTextIem(pItemIndex)
{
   if (typeof(pItemIndex) != "number")
      return "";
   if ((pItemIndex < 0) || (pItemIndex >= this.txtItemList.length))
      return "";

   return this.txtItemList[pItemIndex];
}
function ChoiceScrollbox_ReplaceTextItem(pItemIndexOrStr, pNewItem)
{
   if (typeof(pNewItem) != "string")
      return false;

   // Find the item index
   var itemIndex = -1;
   if (typeof(pItemIndexOrStr) == "number")
   {
      if ((pItemIndexOrStr < 0) || (pItemIndexOrStr >= this.txtItemList.length))
         return false;
      else
         itemIndex = pItemIndexOrStr;
   }
   else if (typeof(pItemIndexOrStr) == "string")
   {
      itemIndex = -1;
      for (var i = 0; (i < this.txtItemList.length) && (itemIndex == -1); ++i)
      {
         if (this.txtItemList[i] == pItemIndexOrStr)
            itemIndex = i;
      }
   }
   else
      return false;

   // Replace the item
   var replacedIt = false;
   if ((itemIndex > -1) && (itemIndex < this.txtItemList.length))
   {
      this.txtItemList[itemIndex] = pNewItem;
      replacedIt = true;
   }
   return replacedIt;
}
function ChoiceScrollbox_DelTextItem(pItemIndexOrStr)
{
   // Find the item index
   var itemIndex = -1;
   if (typeof(pItemIndexOrStr) == "number")
   {
      if ((pItemIndexOrStr < 0) || (pItemIndexOrStr >= this.txtItemList.length))
         return false;
      else
         itemIndex = pItemIndexOrStr;
   }
   else if (typeof(pItemIndexOrStr) == "string")
   {
      itemIndex = -1;
      for (var i = 0; (i < this.txtItemList.length) && (itemIndex == -1); ++i)
      {
         if (this.txtItemList[i] == pItemIndexOrStr)
            itemIndex = i;
      }
   }
   else
      return false;

   // Remove the item
   var removedIt = false;
   if ((itemIndex > -1) && (itemIndex < this.txtItemList.length))
   {
      this.txtItemList = this.txtItemList.splice(itemIndex, 1);
      removedIt = true;
   }
   return removedIt;
}
function ChoiceScrollbox_ChgCharInTextItem(pItemIndexOrStr, pStrIndex, pNewText)
{
	// Find the item index
	var itemIndex = -1;
	if (typeof(pItemIndexOrStr) == "number")
	{
		if ((pItemIndexOrStr < 0) || (pItemIndexOrStr >= this.txtItemList.length))
			return false;
		else
			itemIndex = pItemIndexOrStr;
	}
	else if (typeof(pItemIndexOrStr) == "string")
	{
		itemIndex = -1;
		for (var i = 0; (i < this.txtItemList.length) && (itemIndex == -1); ++i)
		{
			if (this.txtItemList[i] == pItemIndexOrStr)
				itemIndex = i;
		}
	}
	else
		return false;

	// Change the character in the item
	var changedIt = false;
	if ((itemIndex > -1) && (itemIndex < this.txtItemList.length))
	{
		this.txtItemList[itemIndex] = chgCharInStr(this.txtItemList[itemIndex], pStrIndex, pNewText);
		changedIt = true;
	}
	return changedIt;
}
function ChoiceScrollbox_GetChosenTextItemIndex()
{
   return this.chosenTextItemIndex;
}
function ChoiceScrollbox_SetItemArray(pArray, pStripCtrl)
{
	var safeToSet = false;
	if (Object.prototype.toString.call(pArray) === "[object Array]")
	{
		if (pArray.length > 0)
			safeToSet = (typeof(pArray[0]) == "string");
		else
			safeToSet = true; // It's safe to set an empty array
	}

	if (safeToSet)
	{
		delete this.txtItemList;
		this.txtItemList = pArray;

		var stripCtrl = true;
		if (typeof(pStripCtrl) == "boolean")
			stripCtrl = pStripCtrl;
		if (stripCtrl)
		{
			// Remove attribute/color characters from the text lines in the array
			for (var i = 0; i < this.txtItemList.length; ++i)
				this.txtItemList[i] = strip_ctrl(this.txtItemList[i]);
		}
	}

	return safeToSet;
}
function ChoiceScrollbox_ClearItems()
{
   this.txtItemList.length = 0;
}
function ChoiceScrollbox_SetEnterKeyOverrideFn(pOverrideFn)
{
   if (Object.prototype.toString.call(pOverrideFn) == "[object Function]")
      this.enterKeyOverrideFn = pOverrideFn;
}
function ChoiceScrollbox_ClearEnterKeyOverrideFn()
{
   this.enterKeyOverrideFn = null;
}
function ChoiceScrollbox_AddInputLoopExitKey(pKeypress)
{
   this.inputLoopExitKeys[pKeypress] = true;
}
function ChoiceScrollbox_SetBottomBorderText(pText, pAddTChars, pAutoStripIfTooLong)
{
	if (typeof(pText) != "string")
		return;

	const innerWidth = (pAddTChars ? this.dimensions.width-4 : this.dimensions.width-2);

	if (pAutoStripIfTooLong)
	{
		if (strip_ctrl(pText).length > innerWidth)
			pText = pText.substr(0, innerWidth);
	}

	// Re-build the bottom border string based on the new text
	this.bottomBorder = "\x01n" + this.programCfgObj.colors.listBoxBorder + LOWER_LEFT_SINGLE;
	if (pAddTChars)
		this.bottomBorder += RIGHT_T_SINGLE;
	if (pText.indexOf("\x01n") != 0)
		this.bottomBorder += "\x01n";
	this.bottomBorder += pText + "\x01n" + this.programCfgObj.colors.listBoxBorder;
	if (pAddTChars)
		this.bottomBorder += LEFT_T_SINGLE;
	var numCharsRemaining = this.dimensions.width - strip_ctrl(this.bottomBorder).length - 3;
	for (var i = 0; i < numCharsRemaining; ++i)
		this.bottomBorder += HORIZONTAL_SINGLE;
	this.bottomBorder += LOWER_RIGHT_SINGLE;
}
function ChoiceScrollbox_DrawBorder()
{
	console.gotoxy(this.dimensions.topLeftX, this.dimensions.topLeftY);
	console.print(this.topBorder);
	// Draw the side border characters
	var screenRow = this.dimensions.topLeftY + 1;
	for (var screenRow = this.dimensions.topLeftY+1; screenRow <= this.dimensions.bottomRightY-1; ++screenRow)
	{
		console.gotoxy(this.dimensions.topLeftX, screenRow);
		console.print(VERTICAL_SINGLE);
		console.gotoxy(this.dimensions.bottomRightX, screenRow);
		console.print(VERTICAL_SINGLE);
	}
	// Draw the bottom border
	console.gotoxy(this.dimensions.topLeftX, this.dimensions.bottomRightY);
	console.print(this.bottomBorder);
}
function ChoiceScrollbox_DrawInnerMenu(pSelectedIndex)
{
	var selectedIndex = (typeof(pSelectedIndex) == "number" ? pSelectedIndex : -1);
	var startArrIndex = this.pageNum * this.numItemsPerPage;
	var endArrIndex = startArrIndex + this.numItemsPerPage;
	if (endArrIndex > this.txtItemList.length)
		endArrIndex = this.txtItemList.length;
	var selectedItemRow = this.dimensions.topLeftY+1;
	var screenY = this.dimensions.topLeftY + 1;
	for (var i = startArrIndex; i < endArrIndex; ++i)
	{
		console.gotoxy(this.dimensions.topLeftX+1, screenY);
		if (i == selectedIndex)
		{
			printf(this.listIemHighlightFormatStr, this.txtItemList[i].substr(0, this.maxItemWidth));
			selectedItemRow = screenY;
		}
		else
			printf(this.listIemFormatStr, this.txtItemList[i].substr(0, this.maxItemWidth));
		++screenY;
	}
	// If the current screen row is below the bottom row inside the box,
	// continue and write blank lines to the bottom of the inside of the box
	// to blank out any text that might still be there.
	while (screenY < this.dimensions.topLeftY+this.dimensions.height-1)
	{
		console.gotoxy(this.dimensions.topLeftX+1, screenY);
		printf(this.listIemFormatStr, "");
		++screenY;
	}

	// Update the page number in the top border of the box.
	console.gotoxy(this.pageNumTxtStartX, this.dimensions.topLeftY);
	printf("\x01n" + this.programCfgObj.colors.listBoxBorderText + "Page %4d of %4d", this.pageNum+1, this.numPages);
	return selectedItemRow;
}
function ChoiceScrollbox_RefreshOnScreen(pSelectedIndex)
{
	this.drawBorder();
	this.drawInnerMenu(pSelectedIndex);
}
function ChoiceScrollbox_RefreshItemCharOnScreen(pItemIndex, pCharIndex)
{
	if ((typeof(pItemIndex) != "number") || (typeof(pCharIndex) != "number"))
		return;
	if ((pItemIndex < 0) || (pItemIndex >= this.txtItemList.length) ||
	    (pItemIndex < this.topItemIndex) || (pItemIndex > this.bottomItemIndex))
	{
		return;
	}
	if ((pCharIndex < 0) || (pCharIndex >= this.txtItemList[pItemIndex].length))
		return;

	// Save the current cursor position so that we can restore it later
	const originalCurpos = console.getxy();
	// Go to the character's position on the screen and set the highlight or
	// normal color, depending on whether the item is the currently selected item,
	// then print the character on the screen.
	const charScreenX = this.dimensions.topLeftX + 1 + pCharIndex;
	const itemScreenY = this.dimensions.topLeftY + 1 + (pItemIndex - this.topItemIndex);
	console.gotoxy(charScreenX, itemScreenY);
	if (pItemIndex == this.chosenTextItemIndex)
		console.print(this.programCfgObj.colors.listBoxItemHighlight);
	else
		console.print(this.programCfgObj.colors.listBoxItemText);
	console.print(this.txtItemList[pItemIndex].charAt(pCharIndex));
	// Move the cursor back to where it was originally
	console.gotoxy(originalCurpos);
}
function ChoiceScrollbox_DoInputLoop(pDrawBorder)
{
	var retObj = {
		itemWasSelected: false,
		selectedIndex: -1,
		selectedItem: "",
		lastKeypress: ""
	};

	// Don't do anything if the item list doesn't contain any items
	if (this.txtItemList.length == 0)
		return retObj;

	//////////////////////////////////
	// Locally-defined functions

	// This function returns the index of the bottommost item that
	// can be displayed in the box.
	//
	// Parameters:
	//  pArray: The array containing the items
	//  pTopindex: The index of the topmost item displayed in the box
	//  pNumItemsPerPage: The number of items per page
	function getBottommostItemIndex(pArray, pTopIndex, pNumItemsPerPage)
	{
		var bottomIndex = pTopIndex + pNumItemsPerPage - 1;
		// If bottomIndex is beyond the last index, then adjust it.
		if (bottomIndex >= pArray.length)
			bottomIndex = pArray.length - 1;
		return bottomIndex;
	}



	//////////////////////////////////
	// Code

	// Variables for keeping track of the item list
	this.numItemsPerPage = this.dimensions.height - 2;
	this.topItemIndex = 0;    // The index of the message group at the top of the list
	// Figure out the index of the last message group to appear on the screen.
	this.bottomItemIndex = getBottommostItemIndex(this.txtItemList, this.topItemIndex, this.numItemsPerPage);
	this.numPages = Math.ceil(this.txtItemList.length / this.numItemsPerPage);
	const topIndexForLastPage = (this.numItemsPerPage * this.numPages) - this.numItemsPerPage;

	if (pDrawBorder)
		this.drawBorder();

	// User input loop
	// For the horizontal location of the page number text for the box border:
	// Based on the fact that there can be up to 9999 text replacements and 10
	// per page, there will be up to 1000 pages of replacements.  To write the
	// text, we'll want to be 20 characters to the left of the end of the border
	// of the box.
	this.pageNumTxtStartX = this.dimensions.topLeftX + this.dimensions.width - 19;
	this.maxItemWidth = this.dimensions.width - 2;
	this.pageNum = 0;
	var startArrIndex = 0;
	this.chosenTextItemIndex = retObj.selectedIndex = 0;
	var endArrIndex = 0; // One past the last array item
	var curpos = { // For keeping track of the current cursor position
		x: 0,
		y: 0
	};
	var refreshList = true; // For screen redraw optimizations
	this.continueInputLoopOverride = true;
	var continueOn = true;
	while (continueOn && this.continueInputLoopOverride)
	{
		if (refreshList)
		{
			this.bottomItemIndex = getBottommostItemIndex(this.txtItemList, this.topItemIndex, this.numItemsPerPage);

			// Write the list of items for the current page.  Also, drawInnerMenu()
			// will return the selected item row.
			var selectedItemRow = this.drawInnerMenu(retObj.selectedIndex);

			// Just for sane appearance: Move the cursor to the first character of
			// the currently-selected row and set the appropriate color.
			curpos.x = this.dimensions.topLeftX+1;
			curpos.y = selectedItemRow;
			console.gotoxy(curpos.x, curpos.y);
			console.print(this.programCfgObj.colors.listBoxItemHighlight);

			refreshList = false;
		}

		// Get a key from the user (upper-case) and take action based upon it.
		retObj.lastKeypress = getKeyWithESCChars(K_UPPER|K_NOCRLF|K_NOSPIN, this.programCfgObj);
		switch (retObj.lastKeypress)
		{
			case 'N': // Next page
			case KEY_PAGE_DOWN:
				refreshList = (this.pageNum < this.numPages-1);
				if (refreshList)
				{
					++this.pageNum;
					this.topItemIndex += this.numItemsPerPage;
					this.chosenTextItemIndex = retObj.selectedIndex = this.topItemIndex;
					// Note: this.bottomItemIndex is refreshed at the top of the loop
				}
				break;
			case 'P': // Previous page
			case KEY_PAGE_UP:
				refreshList = (this.pageNum > 0);
				if (refreshList)
				{
					--this.pageNum;
					this.topItemIndex -= this.numItemsPerPage;
					this.chosenTextItemIndex = retObj.selectedIndex = this.topItemIndex;
					// Note: this.bottomItemIndex is refreshed at the top of the loop
				}
				break;
			case 'F': // First page
				refreshList = (this.pageNum > 0);
				if (refreshList)
				{
					this.pageNum = 0;
					this.topItemIndex = 0;
					this.chosenTextItemIndex = retObj.selectedIndex = this.topItemIndex;
					// Note: this.bottomItemIndex is refreshed at the top of the loop
				}
				break;
			case 'L': // Last page
				refreshList = (this.pageNum < this.numPages-1);
				if (refreshList)
				{
					this.pageNum = this.numPages-1;
					this.topItemIndex = topIndexForLastPage;
					this.chosenTextItemIndex = retObj.selectedIndex = this.topItemIndex;
					// Note: this.bottomItemIndex is refreshed at the top of the loop
				}
				break;
			case KEY_UP:
				// Move the cursor up one item
				if (retObj.selectedIndex > 0)
				{
					// If the previous item index is on the previous page, then we'll
					// want to display the previous page.
					var previousItemIndex = retObj.selectedIndex - 1;
					if (previousItemIndex < this.topItemIndex)
					{
						--this.pageNum;
						this.topItemIndex -= this.numItemsPerPage;
						// Note: this.bottomItemIndex is refreshed at the top of the loop
						refreshList = true;
					}
					else
					{
						// Display the current line un-highlighted
						console.gotoxy(this.dimensions.topLeftX+1, curpos.y);
						printf(this.listIemFormatStr, this.txtItemList[retObj.selectedIndex].substr(0, this.maxItemWidth));
						// Display the previous line highlighted
						curpos.x = this.dimensions.topLeftX+1;
						--curpos.y;
						console.gotoxy(curpos);
						printf(this.listIemHighlightFormatStr, this.txtItemList[previousItemIndex].substr(0, this.maxItemWidth));
						console.gotoxy(curpos); // Move the cursor into place where it should be
						refreshList = false;
					}
					this.chosenTextItemIndex = retObj.selectedIndex = previousItemIndex;
				}
				break;
			case KEY_DOWN:
				// Move the cursor down one item
				if (retObj.selectedIndex < this.txtItemList.length - 1)
				{
					// If the next item index is on the next page, then we'll want to
					// display the next page.
					var nextItemIndex = retObj.selectedIndex + 1;
					if (nextItemIndex > this.bottomItemIndex)
					{
						++this.pageNum;
						this.topItemIndex += this.numItemsPerPage;
						// Note: this.bottomItemIndex is refreshed at the top of the loop
						refreshList = true;
					}
					else
					{
						// Display the current line un-highlighted
						console.gotoxy(this.dimensions.topLeftX+1, curpos.y);
						printf(this.listIemFormatStr, this.txtItemList[retObj.selectedIndex].substr(0, this.maxItemWidth));
						// Display the previous line highlighted
						curpos.x = this.dimensions.topLeftX+1;
						++curpos.y;
						console.gotoxy(curpos);
						printf(this.listIemHighlightFormatStr, this.txtItemList[nextItemIndex].substr(0, this.maxItemWidth));
						console.gotoxy(curpos); // Move the cursor into place where it should be
						refreshList = false;
					}
					this.chosenTextItemIndex = retObj.selectedIndex = nextItemIndex;
				}
				break;
			case KEY_HOME: // Go to the first row in the box
				if (retObj.selectedIndex > this.topItemIndex)
				{
					// Display the current line un-highlighted
					console.gotoxy(this.dimensions.topLeftX+1, curpos.y);
					printf(this.listIemFormatStr, this.txtItemList[retObj.selectedIndex].substr(0, this.maxItemWidth));
					// Select the top item, and display it highlighted.
					this.chosenTextItemIndex = retObj.selectedIndex = this.topItemIndex;
					curpos.x = this.dimensions.topLeftX+1;
					curpos.y = this.dimensions.topLeftY+1;
					console.gotoxy(curpos);
					printf(this.listIemHighlightFormatStr, this.txtItemList[retObj.selectedIndex].substr(0, this.maxItemWidth));
					console.gotoxy(curpos); // Move the cursor into place where it should be
					refreshList = false;
				}
				break;
			case KEY_END: // Go to the last row in the box
				if (retObj.selectedIndex < this.bottomItemIndex)
				{
					// Display the current line un-highlighted
					console.gotoxy(this.dimensions.topLeftX+1, curpos.y);
					printf(this.listIemFormatStr, this.txtItemList[retObj.selectedIndex].substr(0, this.maxItemWidth));
					// Select the bottommost item, and display it highlighted.
					this.chosenTextItemIndex = retObj.selectedIndex = this.bottomItemIndex;
					curpos.x = this.dimensions.topLeftX+1;
					curpos.y = this.dimensions.bottomRightY-1;
					console.gotoxy(curpos);
					printf(this.listIemHighlightFormatStr, this.txtItemList[retObj.selectedIndex].substr(0, this.maxItemWidth));
					console.gotoxy(curpos); // Move the cursor into place where it should be
					refreshList = false;
				}
				break;
			case KEY_ENTER:
				// If the enter key override function is set, then call it and pass
				// this object into it.  Otherwise, just select the item and quit.
				if (this.enterKeyOverrideFn !== null)
				this.enterKeyOverrideFn(this);
				else
				{
					retObj.itemWasSelected = true;
					// Note: retObj.selectedIndex is already set.
					retObj.selectedItem = this.txtItemList[retObj.selectedIndex];
					refreshList = false;
					continueOn = false;
				}
				break;
			case KEY_ESC: // Quit
			case CTRL_A:  // Quit
			case 'Q':     // Quit
				this.chosenTextItemIndex = retObj.selectedIndex = -1;
				refreshList = false;
				continueOn = false;
				break;
			default:
				// If the keypress is an additional key to exit the input loop, then
				// do so.
				if (this.inputLoopExitKeys.hasOwnProperty(retObj.lastKeypress))
				{
					this.chosenTextItemIndex = retObj.selectedIndex = -1;
					refreshList = false;
					continueOn = false;
				}
				else
				{
					// Unrecognized command.  Don't refresh the list of the screen.
					refreshList = false;
				}
				break;
		}
	}

	this.continueInputLoopOverride = true; // Reset

	console.print("\x01n"); // To prevent outputting highlight colors, etc..
	return retObj;
}

///////////////////////////////////////////////////////////////////////////////////

// Writes a default twitlist for the user if it doesn't exist
function writeDefaultUserTwitListIfNotExist()
{
	if (file_exists(gUserTwitListFilename))
		return;

	var outFile = new File(gUserTwitListFilename);
	if (outFile.open("w"))
	{
		outFile.writeln("; This is a personal twitlist for Digital Distortion Message Reader to block");
		outFile.writeln("; messages from (and to) certain usernames. The intention is that if you are");
		outFile.writeln("; being harassed by a specific person, or you simply do not wish to see their");
		outFile.writeln("; messages, you can filter that person by adding their name (or email address)");
		outFile.writeln("; to this file.");

		outFile.close();
	}
}

// Returns whether 2 arrays have the same values
function arraysHaveSameValues(pArray1, pArray2)
{
	if (pArray1 == null && pArray2 == null)
		return true;
	else if (pArray1 != null && pArray2 == null)
		return false;
	else if (pArray1 == null && pArray2 != null)
		return false;

	var arraysHaveSameValues = true;
	if (pArray1.length != pArray2.length)
		arraysHaveSameValues = false;
	else
	{
		for (var a1i = 0; a1i < pArray1.length && arraysHaveSameValues; ++a1i)
		{
			var seenInArray2 = false;
			for (var a2i = 0; a2i < pArray2.length && !seenInArray2; ++a2i)
				seenInArray2 = (pArray2[a2i] == pArray1[a1i]);
			arraysHaveSameValues = seenInArray2;
		}
	}
	return arraysHaveSameValues;
}

// Returns whether or not the sender of a message is a sysop.
//
// Parameters:
//  pMsgHdr: A message header
//
// Return value: Boolean: Whether or not the sender of the message is a sysop
function msgSenderIsASysop(pMsgHdr)
{
	if (typeof(pMsgHdr) !== "object")
		return false;

	var senderUserNum = 0;
	if (pMsgHdr.hasOwnProperty("sender_userid"))
		senderUserNum = system.matchuser(pMsgHdr.sender_userid);
	else if (pMsgHdr.hasOwnProperty("from"))
	{
		senderUserNum = system.matchuser(pMsgHdr.from);
		if (senderUserNum < 1)
			senderUserNum = system.matchuserdata(U_NAME, pMsgHdr.from);
	}

	var senderIsSysop = false;
	if (senderUserNum >= 1)
	{
		if (senderUserNum == 1)
			senderIsSysop = true;
		else
		{
			var senderUser = new User(senderUserNum);
			senderIsSysop = senderUser.is_sysop;
		}
	}
	return senderIsSysop;
}

// Gets the quote wrap settings for an external editor
//
// Parameters:
//  pEditorCode: The internal code of an external editor
//
// Return value: An object containing the following properties:
//               quoteWrapEnabled: Boolean: Whether or not quote wrapping is enabled for the editor
//               quoteWrapCols: The number of columns to wrap quote lines
//  If the given editor code is not found, quoteWrapEnabled will be false and quoteWrapCols will be -1
function getExternalEditorQuoteWrapCfgFromSCFG(pEditorCode)
{
	var retObj = {
		quoteWrapEnabled: false,
		quoteWrapCols: -1
	};

	if (typeof(pEditorCode) !== "string")
		return retObj;
	if (pEditorCode.length == 0)
		return retObj;

	var editorCode = pEditorCode.toLowerCase();
	if (!xtrn_area.editor.hasOwnProperty(editorCode))
		return retObj;

	// Set up a cache so that we don't have to keep repeatedly parsing the Synchronet
	// config every time the user replies to a message
	if (typeof(getExternalEditorQuoteWrapCfgFromSCFG.cache) === "undefined")
		getExternalEditorQuoteWrapCfgFromSCFG.cache = {};
	// If we haven't looked up the quote wrap cols setting yet, then do so; otherwise, use the
	// cached setting.
	if (!getExternalEditorQuoteWrapCfgFromSCFG.cache.hasOwnProperty(editorCode))
	{
		if ((xtrn_area.editor[editorCode].settings & XTRN_QUOTEWRAP) == XTRN_QUOTEWRAP)
		{
			retObj.quoteWrapEnabled = true;
			retObj.quoteWrapCols = console.screen_columns - 1;

			// For Synchronet 3.20 and newer, read the quote wrap setting from xtrn.ini
			if (system.version_num >= 32000)
			{
				// The INI section for the editor should be something like [editor:SLYEDICE], and
				// it should have a quotewrap_cols property
				var xtrnIniFile = new File(system.ctrl_dir + "xtrn.ini");
				if (xtrnIniFile.open("r"))
				{
					var quoteWrapCols = xtrnIniFile.iniGetValue("editor:" + pEditorCode.toUpperCase(), "quotewrap_cols", console.screen_columns - 1);
					if (quoteWrapCols > 0)
						retObj.quoteWrapCols = quoteWrapCols;
					xtrnIniFile.close();
				}
			}
			else
			{
				// Synchronet below version 3.20: Read the quote wrap setting from xtrn.cnf
				var cnflib = load({}, "cnflib.js");
				var xtrnCnf = cnflib.read("xtrn.cnf");
				if (typeof(xtrnCnf) === "object")
				{
					for (var i = 0; i < xtrnCnf.xedit.length; ++i)
					{
						if (xtrnCnf.xedit[i].code.toLowerCase() == editorCode)
						{
							if (xtrnCnf.xedit[i].hasOwnProperty("quotewrap_cols"))
							{
								if (xtrnCnf.xedit[i].quotewrap_cols > 0)
									retObj.quoteWrapCols = xtrnCnf.xedit[i].quotewrap_cols;
							}
							break;
						}
					}
				}
			}
		}
		getExternalEditorQuoteWrapCfgFromSCFG.cache[editorCode] = retObj;
	}
	else
		retObj = getExternalEditorQuoteWrapCfgFromSCFG.cache[editorCode];

	return retObj;
}

// Changes a character in a string, and returns the new string.  If any of the
// parameters are invalid, then the original string will be returned.
//
// Parameters:
//  pStr: The original string
//  pCharIndex: The index of the character to replace
//  pNewText: The new character or text to place at that position in the string
//
// Return value: The new string
function chgCharInStr(pStr, pCharIndex, pNewText)
{
   if (typeof(pStr) != "string")
      return "";
   if ((pCharIndex < 0) || (pCharIndex >= pStr.length))
      return pStr;
   if (typeof(pNewText) != "string")
      return pStr;

   return (pStr.substr(0, pCharIndex) + pNewText + pStr.substr(pCharIndex+1));
}

///////////////////////////////////////////////////////////////////////////////////

// For debugging: Writes some text on the screen at a given location with a given pause.
//
// Parameters:
//  pX: The column number on the screen at which to write the message
//  pY: The row number on the screen at which to write the message
//  pText: The text to write
//  pPauseMS: The pause time, in milliseconds
//  pClearLineAttrib: Optional - The color/attribute to clear the line with.
//                    If not specified or null is specified, defaults to normal attribute.
//  pClearLineAfter: Whether or not to clear the line again after the message is dispayed and
//                   the pause occurred.  This is optional.
function writeWithPause(pX, pY, pText, pPauseMS, pClearLineAttrib, pClearLineAfter)
{
	var clearLineAttrib = "\x01n";
	if ((pClearLineAttrib != null) && (typeof(pClearLineAttrib) == "string"))
		clearLineAttrib = pClearLineAttrib;
	console.gotoxy(pX, pY);
	console.cleartoeol(clearLineAttrib);
	console.print(pText);
	if (pPauseMS > 0)
		mswait(pPauseMS);
	if (pClearLineAfter)
	{
		console.gotoxy(pX, pY);
		console.cleartoeol(clearLineAttrib);
	}
}
