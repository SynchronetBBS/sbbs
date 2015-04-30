// This script is to be executed for the 'Scan Posts' loadable module, configured
// in SCFG in System > Loadable Modules.
//
// This module is used for:
// - Simply reading a sub-board
// - Find text in messages

// For stock Synchronet functionality:
//bbs.scan_msgs([sub-board=current] [,mode=SCAN_READ] [,string find])

load("sbbsdefs.js");

console.print("\1n");

// Synchronet will pass at least 2 command-line arguments and sometimes 3:
// 1. The sub-board number (numeric)
// 2. The scan mode (numeric)
// 3. Optional: Search text (if any)
if (argc < 2)
{
	console.print("\1h\1yNot enough arguments were passed to the Scan Posts module!  Please inform the");
	console.crlf();
	console.print("sysop.\1n");
	console.crlf();
	console.pause();
	exit(1);
}

var subBoardNum = Number(argv[0]);
var scanMode = Number(argv[1]);
var searchText = argv[2];


/*
// Temporary - For debugging
console.print("Sub-board #: " + subBoardNum + " (" + typeof(subBoardNum) + ")\r\n");
console.print("Scan mode: " + scanMode  + " (" + typeof(scanMode) + ")\r\n");
console.print("Search text:" + searchText + ": (" + typeof(searchText) + ")\r\n");

console.print("DDScanPosts\r\n");
if (scanMode == SCAN_READ)
	console.print("SCAN_READ\r\n");
if ((scanMode & SCAN_CONST) == SCAN_CONST)
	console.print("SCAN_CONST\r\n");
if ((scanMode & SCAN_NEW) == SCAN_NEW)
	console.print("SCAN_NEW\r\n");
if ((scanMode & SCAN_BACK) == SCAN_BACK)
	console.print("SCAN_BACK\r\n");
if ((scanMode & SCAN_TOYOU) == SCAN_TOYOU)
	console.print("SCAN_TOYOU\r\n");
if ((scanMode & SCAN_FIND) == SCAN_FIND)
	console.print("SCAN_FIND\r\n");
if ((scanMode & SCAN_UNREAD) == SCAN_UNREAD)
	console.print("SCAN_UNREAD\r\n");
console.pause();
// End Temporary
*/

// SYSOPS: Change the msgReaderPath variable if you put Digital Distortion
// Message Reader in a different path
var msgReaderPath = "../xtrn/DDMsgReader";


// The start of the command string to use with bbs.exec()
var rdrCmdStrStart = "?" + msgReaderPath + "/DDMsgReader.js ";

// No extra mode bits set, only read: Use Digital Distortion Message Reader
// in read mode
if (scanMode == SCAN_READ)
	bbs.exec(rdrCmdStrStart + "-startMode=read");
// Some modes that the Digital Distortion Message Reader doesn't handle yet: Use
// Synchronet's stock behavior.
else if (((scanMode & SCAN_CONST) == SCAN_CONST) || ((scanMode & SCAN_BACK) == SCAN_BACK))
	bbs.scan_subs(scanMode, scanAllSubs);
else // Stock Synchronet behavior
	bbs.scan_msgs(subBoardNum, scanMode, searchText);