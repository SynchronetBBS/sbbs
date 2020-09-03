// $Id: DDReadPersonalMail.js,v 1.5 2020/05/23 23:35:48 nightfox Exp $

// This script is to be executed for the 'Read mail' loadable module, configured
// in SCFG in System > Loadable Modules.

if (typeof(require) === "function")
	require("sbbsdefs.js", "SCAN_UNREAD");
else
	load("sbbsdefs.js");

console.print("\1n");

// Synchronet will pass 2 command-line arguments:
// 1. The 'which' mailbox value (numeric) - MAIL_YOUR, MAIL_SENT, or MAIL_ALL.
//    MAIL_ANY won't be passed to this script.
// 2. The user number (numeric)
if (argc < 2)
{
	console.print("\1h\1yNot enough arguments were passed to the Read Mail module!  Please inform the");
	console.crlf();
	console.print("sysop.\1n");
	console.crlf();
	console.pause();
	exit(1);
}
//bbs.read_mail(whichMailbox);
//exit(0);


var whichMailbox = Number(argv[0]);
var userNum = Number(argv[1]);
// The 3rd argument is mode bits.  See if we should only display new (unread)
// personal email.
var newMailOnly = false;
if (argv.length >= 3)
{
	var modeVal = +(argv[2]);
	newMailOnly = (((modeVal & SCAN_FIND) == SCAN_FIND) && ((modeVal & LM_UNREAD) == LM_UNREAD));
}


// SYSOPS: Change the msgReaderPath variable if you put Digital Distortion
// Message Reader in a different path
var msgReaderPath = "../xtrn/DDMsgReader";

// The readerStartMode variable, below, controls whether the reader
// is to start in reader mode or message list mode.  Set it to "list"
// for list mode or "read" for reader mode.
var readerStartmode = "list";
//var readerStartmode = "read";


// The start of the command string to use with bbs.exec()
var rdrCmdStrStart = "?" + msgReaderPath + "/DDMsgReader.js ";

// Launch Digital Distortion depending on the value whichMailBox.
// Note: MAIL_ANY won't be passed to this script.
switch (whichMailbox)
{
	case MAIL_YOUR: // Mail sent to you
		var cmdArgs = "-personalEmail -userNum=" + userNum;
		if (newMailOnly)
			cmdArgs += " -onlyNewPersonalEmail";
		cmdArgs += " -startMode=" + readerStartmode;
		bbs.exec(rdrCmdStrStart + cmdArgs);
		break;
	case MAIL_SENT: // Mail you have sent
		bbs.exec(rdrCmdStrStart + "-personalEmailSent -userNum=" + userNum + " -startMode=" + readerStartmode);
		break;
	case MAIL_ALL:
		bbs.exec(rdrCmdStrStart +  "-allPersonalEmail -startMode=" + readerStartmode);
		break;
	default:
		bbs.read_mail(whichMailbox);
		break;
}