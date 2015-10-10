// This script is to be executed for the 'Read mail' loadable module, configured
// in SCFG in System > Loadable Modules.

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


var whichMailbox = Number(argv[0]);
var userNum = Number(argv[1]);

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
		bbs.exec(rdrCmdStrStart + "-personalEmail -userNum=" + userNum + " -startMode=" + readerStartmode);
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