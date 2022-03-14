// SYSOPS: Change the msgReaderPath variable if you put Digital Distortion
// Message Reader in a different path
var msgReaderPath = "../xtrn/DDMsgReader";

// Run Digital Distortion Message Reader
bbs.exec("?" + msgReaderPath + "/DDMsgReader.js " + argv.join(" "));