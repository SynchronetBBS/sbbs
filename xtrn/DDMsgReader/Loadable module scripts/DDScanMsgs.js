// This script is to be executed for the 'Scan Msgs' loadable module, configured
// in SCFG in System > Loadable Modules.
//
// This module is used for:
// - Simply reading a sub-board
// - Find text in messages
//
// Date       Author            Description
// 2015-05-06 Eric Oulashin     Version 1.0 - Initial release
// 2015-06-10 Eric Oulashin     Version 1.02
//                              Bug fix: Switched to bbs.scan_msgs() instead of
//                              bbs.scan_subs() for all other scan modes besides
//                              SCAN_READ.

// For stock Synchronet functionality:
//bbs.scan_msgs([sub-board=current] [,mode=SCAN_READ] [,string find])

load("sbbsdefs.js");

console.print("\1n");

// Synchronet will pass at least 2 command-line arguments and sometimes 3:
// 1. The sub-board internal code
// 2. The scan mode (numeric)
// 3. Optional: Search text (if any)
if (argc < 2)
{
	console.print("\1h\1yNot enough arguments were passed to the Scan Messages module!  Please inform the");
	console.crlf();
	console.print("sysop.\1n");
	console.crlf();
	console.pause();
	exit(1);
}

var subBoardCode = argv[0];
var scanMode = Number(argv[1]);
var searchText = argv[2];


// SYSOPS: Change the msgReaderPath variable if you put Digital Distortion
// Message Reader in a different path
var msgReaderPath = "../xtrn/DDMsgReader";

// The start of the command string to use with bbs.exec()
var rdrCmdStrStart = "?" + msgReaderPath + "/DDMsgReader.js ";

// No extra mode bits set, only read: Use Digital Distortion Message Reader
// in read mode
if (scanMode == SCAN_READ)
	bbs.exec(rdrCmdStrStart + "-subBoard=" + subBoardCode + " -startMode=read");
// Some modes that the Digital Distortion Message Reader doesn't handle yet: Use
// Synchronet's stock behavior.
else
	bbs.scan_msgs(subBoardCode, scanMode, searchText);