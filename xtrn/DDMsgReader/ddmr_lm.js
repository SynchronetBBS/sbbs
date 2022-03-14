// SYSOPS: Change the msgReaderPath variable if you put Digital Distortion
// Message Reader in a different path
var msgReaderPath = "../xtrn/DDMsgReader";

// Build a command line string and then run DDMsgReader
var argsStr = "";
for (var i = 0; i < argv.length; ++i)
{
	argsStr += argv[i];
	if (i < argv.length - 1)
		argsStr += " ";
}

// Run Digital Distortion Message Reader
bbs.exec("?" + msgReaderPath + "/DDMsgReader.js " + argsStr);