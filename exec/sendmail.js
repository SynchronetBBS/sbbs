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
var done_headers=false;
var hdrs=new Object;
var body='';
var rcpt_list=new Array();

while((line=readln()) != undefined) {
	if(!done_headers) {
		if(line == '') {
			done_headers=true;
			continue;
		}
		var m=line.match(/^([^:]*):\s*(.*)$/);
		if(m != undefined && m.index>-1) {
			switch(m[1].toLowerCase()) {
				case 'to':
					addys=m[2].split(',');
					for (addy in addys) {
						var hdr=new Object;
						hdr.to=mail_get_name(addys[addy]);
						hdr.to_net_type=NET_INTERNET;
						hdr.to_net_addr=mail_get_address(addys[addy]);
						rcpt_list.push(hdr);
					}
					break;
				case 'from':
					hdrs.from=mail_get_name(m[2]);
					hdrs.from_net_type=NET_INTERNET;
					hdrs.from_net_addr=mail_get_address(m[2]);
					break;
				case 'subject':
					hdrs.subject=m[2];
					break;
				case 'reply-to':
					hdrs.replyto=mail_get_name(m[2]);
					hdrs.replyto_net_type=NET_INTERNET;
					hdrs.replyto_net_addr=mail_get_address(m[2]);
					break;
				case 'message-id':
					hdrs.id=m[2];
					break;
				case 'references':
					hdrs.reply_id=m[2];
					break;
				case 'date':
					hdrs.date=m[2];
					break;
			}
		}
	}
	else {
		body += line + "\r\n";
	}
}

var msgbase = new MsgBase('mail');
if(msgbase.open!=undefined && msgbase.open()==false) {
	writeln("Cannot send email (open error)!");
	exit();
}
if(!msgbase.save_msg(hdrs, body, rcpt_list)) {
	writeln("Cannot send email (save error)!");
	exit();
}
