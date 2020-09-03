#!/sbbs/exec/jsexec -x -c/sbbs/ctrl
// Change previous line to correct jsexec and ctrl paths!

load("sbbsdefs.js");
load("mailutil.js");

/* This script emulates the "sendmail -i -t" or "sendmail -oi -t"
 * *nix command, sending a message from stdin to recipients specified
 * in the message headers and not using . on a line by itself
 * to terminate a message.
 */

var line;
var verbosity = 0;
var done_headers=false;
var hdr = { from: 'sendmail' };
var body = '';
var rcpt_list = [];

function rcpt(str)
{
	return {
		to: mail_get_name(str),
		to_net_addr: mail_get_address(str),
		to_net_type: NET_INTERNET
	};
}

for(var i = 0; i < argv.length; i++) {
	if(argv[i][0] == '-') {
		switch(argv[i][1]) {
			case 'v':
				verbosity++;
				break;
			case 'F':
				hdr.from = argv[++i];
				break;
		}
	} else {
		rcpt_list.push(rcpt(argv[i]));
	}
}

while((line=readln()) != undefined) {
	if(!done_headers) {
		if(line == '') {
			done_headers=true;
			continue;
		}
		if(verbosity)
			print("sendmail hdr line: '" + line + "'");
		var m=line.match(/^([^:]*):\s*(.*)$/);
		if(m != undefined && m.index>-1) {
			switch(m[1].toLowerCase()) {
				case 'to':
					addys=m[2].split(',');
					for (addy in addys) {
						rcpt_list.push(rcpt(addys[addy]));
					}
					break;
				case 'from':
					hdr.from=mail_get_name(m[2]);
					hdr.from_net_type=NET_INTERNET;
					hdr.from_net_addr=mail_get_address(m[2]);
					break;
				case 'subject':
					hdr.subject=m[2];
					break;
				case 'reply-to':
					hdr.replyto=mail_get_name(m[2]);
					hdr.replyto_net_type=NET_INTERNET;
					hdr.replyto_net_addr=mail_get_address(m[2]);
					break;
				case 'message-id':
					hdr.id=m[2];
					break;
				case 'references':
					hdr.reply_id=m[2];
					break;
				case 'date':
					hdr.date=m[2];
					break;
			}
		}
	}
	else {
		body += line + "\r\n";
	}
}

var msgbase = new MsgBase('mail');
if(!msgbase.open()) {
	writeln("Cannot send email (open error)!");
	exit();
}
if(!msgbase.save_msg(hdr, body, rcpt_list)) {
	writeln("Cannot send email: " + msgbase.error);
	exit();
}
