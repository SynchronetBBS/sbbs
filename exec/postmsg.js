// $Id: postmsg.js,v 1.7 2020/03/30 08:31:14 rswindell Exp $

// Post a message to a local sub-board or mail message base
// a preferred alternative to using "smbutil i"

var hdrs = { };
var sub_code;
var import_fname;

function usage()
{
	print();
	print("usage: postmsg.js [-option] [-option] [...] <sub-code>");
	print();
	print("<sub-code> must be a valid sub-board (msgbase) internal code or 'mail'");
	print();
	print("options:");
	print("\t-i<filename>  import body text from filename rather than stdin");
	print("\t-t<name>      set 'to' user name");
	print("\t-n<addr>      set 'to' netmail address");
	print("\t-u<number>    set 'to' user number");
	print("\t-f<name>      set 'from' user name");
	print("\t-e<number>    set 'from' user number");
	print("\t-s<subject>   set 'subject'");
	print("\t-d            use default values (no prompt) for to, from, and subject");
	print();
	print("Note: You may need to enclose multi-word options in quotes (e.g. \"-fMy Name\")");
	print();
}

for(var i in argv) {
		if(argv[i][0] == '-') {
			var val = argv[i].substr(2);
			switch(argv[i][1]) {
				case 't':
					if(val.length)
						hdrs.to = val;
					break;
				case 'n':
					if(val.length)
						hdrs.to_net_addr = val;
					break;
				case 'u':
					if(val.length)
						hdrs.to_ext = val;
					break;
				case 'f':
					if(val.length)
						hdrs.from = val;
					break;
				case 'e':
					if(val.length)
						hdrs.from_ext = val;
					break;
				case 's':
					if(val.length)
						hdrs.subject = val;
					break;
				case 'i':
					if(val.length)
						import_fname = val;
					break;
				case 'd':
					hdrs.to = "All";
					hdrs.from = system.operator;
					hdrs.subject = "Announcement";
					break;
				default:
					usage();
					exit();
					break;
			}
		} else
			sub_code = argv[i];
}

if(!sub_code) {
	usage();
	exit();
}

if(sub_code != 'mail' && !msg_area.sub[sub_code.toLowerCase()]) {
	alert("Invalid sub-code: " + sub_code);
	print();
	print("Valid sub-codes:");
	print();
	print('mail');
	for(var s in msg_area.sub)
		print(s);
	exit();
}

var msgbase = new MsgBase(sub_code);
if(msgbase.open() == false) {
	alert("Error opening msgbase (" + sub_code + "): " + msgbase.last_error);
	exit();
}

var body = "";
if(import_fname) {
	var file = new File(import_fname);
	if(!file.open("r")) {
		alert("Error " + file.error + " opening file: "+ import_fname);
		exit();
	}
	body = file.readAll().join("\r\n");
	file.close();
} else {
	print("Reading message body from stdin");
	var line;
	while((line=readln()) != undefined) {
		body += line + "\r\n";
	}
}

body = body.trimRight();
if(!body.length) {
	alert("No body text to post");
	exit();
}

if(!hdrs.to)
	hdrs.to = prompt("To User Name");
if(!hdrs.from)
	hdrs.from = prompt("From User name");
if(!hdrs.subject)
	hdrs.subject = prompt("Subject");
var num;
if(!hdrs.to_ext && sub_code == 'mail' && !hdrs.to_net_addr && (num = system.matchuser(hdrs.to)) != 0)
	hdrs.to_ext = num;
if(!hdrs.from_ext && (num = system.matchuser(hdrs.from)) != 0)
	hdrs.from_ext = num;
if(!msgbase.save_msg(hdrs, body)) {
	alert("Error saving message: " + msgbase.last_error);
	exit();
}
print("Message posted successfully to: " + sub_code);
