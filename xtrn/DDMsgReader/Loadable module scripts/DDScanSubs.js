// This script is to be executed for the 'Scan Subs' loadable module, configured
// in SCFG in System > Loadable Modules.
//
// This script is used for:
// - Continuous new scan
// - Browse New Scan
// - New message scan
// - Scan for messages to you (new messages to you)
// - Find text in messages


load("sbbsdefs.js");

console.print("\1n");

// Synchronet will pass 2 command-line arguments: Whether or not all subs
// are being scanned, and the scan mode (numeric).
if (argc < 2)
{
	console.print("\1h\1yNot enough arguments were passed to the Scan Subs module!  Please inform the");
	console.crlf();
	console.print("sysop.\1n");
	console.crlf();
	console.pause();
	exit(1);
}

var scanAllSubs = (argv[0] == "1");
var scanMode = Number(argv[1]);

// SYSOPS: Change the msgReaderPath variable if you put Digital Distortion
// Message Reader in a different path
var msgReaderPath = "../xtrn/DDMsgReader";

// The start of the command string to use with bbs.exec()
var rdrCmdStrStart = "?" + msgReaderPath + "/DDMsgReader.js ";

// Note: SCAN_READ is 0, so the mode bits will always look like they have
// SCAN_READ.

// For modes the Digital Distortion Message Reader doesn't handle yet, use
// Synchronet's stock behavior.
if (((scanMode & SCAN_CONST) == SCAN_CONST) || ((scanMode & SCAN_BACK) == SCAN_BACK))
	bbs.scan_subs(scanMode, scanAllSubs);
else if ((scanMode & SCAN_NEW) == SCAN_NEW)
{
	// Newscan
	if (scanAllSubs)
		bbs.exec(rdrCmdStrStart +"-search=new_msg_scan_all -suppressSearchTypeText");
	else // Prompt for sub-board, group, or all
		bbs.exec(rdrCmdStrStart + "-search=new_msg_scan -suppressSearchTypeText");
}
else if (((scanMode & SCAN_TOYOU) == SCAN_TOYOU) || ((scanMode & SCAN_UNREAD) == SCAN_UNREAD))
{
	// Scan for messages posted to you/new messages posted to you
	if (scanAllSubs)
		bbs.exec(rdrCmdStrStart + "-startMode=read -search=to_user_new_scan_all -suppressSearchTypeText");
	else // Prompt for sub-board, group, or all
		bbs.exec(rdrCmdStrStart + "-startMode=read -search=to_user_new_scan -suppressSearchTypeText");
}
else if ((scanMode & SCAN_FIND) == SCAN_FIND)
	bbs.exec(rdrCmdStrStart + "-search=keyword_search -startMode=list");
else // Stock Synchronet functionality
	bbs.scan_subs(scanMode, scanAllSubs);