// A mail processor that expands a recipient list for mail received for a specific user

// Configure in mailproc.ini like:
// [multi]
// Command = mailproc_multi.js
// to = username
// Passthru = true

// Write your additional recipients (full Internet email addresses) here:
var recipients = ["someone@somewhere", "another@mailbox"];

require("smbdefs.js", "NET_INTERNET");

var rcptlst = new File(recipient_list_filename);
if(!rcptlst.open("r")) {
	alert("Failed to open " + rcptlst.name);
	exit();
}
var list=rcptlst.iniGetAllObjects("number");
rcptlst.close();

for(var i = 0; i < recipients.length; ++i) {
	list.push( { number: list.length, To: recipients[i], ToNetType: NET_INTERNET });
}
if(!rcptlst.open("w")) {
	alert("Failed to open/create " + rcptlst.name);
	exit();
}
rcptlst.iniSetAllObjects(list, "number");
rcptlst.close();
