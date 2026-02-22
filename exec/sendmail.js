#!/sbbs/exec/jsexec -x
// Change previous line to correct jsexec path!

// Suitable as a replacement for /usr/sbin/sendmail
// and/or                        /usr/bin/mail
// and/or                        /usr/bin/mailx

var log = new File(system.data_dir + "sendmail.log");
log.open('a');
log.writeln(system.timestr() + ' ' + js.exec_file + ' argv=' + argv);

load("sbbsdefs.js");
load("mailutil.js");

/* This script emulates the "sendmail" or "mail" *nix command,
 * sending a message from stdin to recipients specified
 * in the message headers and not using . on a line by itself
 * to terminate a message.
 */

var line;
var verbosity = 0;
var hdr = { from: 'sendmail', subject: 'no subject specified' };
var sendmail_mode = (js.exec_file != 'mail' && js.exec_file != 'mailx');
var done_headers = !sendmail_mode;
var body = '';
var rcpt_list = [];
var parse_options = true;
var parse_recipients = false;

for(var i = 0; i < argv.length; i++) {
	if(parse_options && argv[i][0] == '-') {
		switch(argv[i][1]) {
			case '-':
				// "Stop processing command flags
				//  and use the rest of the arguments as addresses."
				parse_options = false;
				break;
			case 'v':
				verbosity++;
				break;
			case 'F':
				if(sendmail_mode) {
					if(argv[i].length > 2)
						hdr.from = argv[i].substring(2);
					else
						hdr.from = argv[++i];
				}
				break;
			case 'r':
			case 'f':
				if(argv[i].length > 2)
					hdr.from_net_addr = argv[i].substring(2);
				else
					hdr.from_net_addr = argv[++i];
				break;
			case 's':
				if(!sendmail_mode)
					hdr.subject = argv[++i];
				break;
			case 't':
				parse_recipients = true;
				done_headers = false;
				break;
		}
	} else {
		rcpt_list.push(parse_mail_recipient(argv[i]));
	}
}

if(parse_recipients && !sendmail_mode) {
	// "Recipients specified on the command line are ignored."
	rcpt_list.length = 0;
}

while((line=readln()) != undefined) {
	log.writeln(line);
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
					if(!parse_recipients)
						break;
					addys=m[2].split(',');
					for (addy in addys) {
						rcpt_list.push(parse_mail_recipient(addys[addy]));
					}
					break;
				case 'from':
					hdr.from = mail_get_name(m[2]);
					hdr.from_net_addr = mail_get_address(m[2]);
					break;
				case 'subject':
					hdr.subject=m[2];
					break;
				case 'reply-to':
					hdr.replyto = mail_get_name(m[2]);
					hdr.replyto_net_addr = mail_get_address(m[2]);
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
				default:
					if(hdr.field_list == undefined)
						hdr.field_list = [];
					hdr.field_list.push(
						{	type: RFC822HEADER,
							data: line
						}
					);
					break;
			}
		} else
			print("Non-header line received: " + line);
	}
	else {
		body += line + "\r\n";
	}
}

// Sanity-check the from_net_addr (SENDERNETADDR) field
if(typeof hdr.from_net_addr == "string") {
	if(hdr.from_net_addr.indexOf('@') < 0)
		hdr.from_net_addr += '@';
	if(hdr.from_net_addr.indexOf('@') == hdr.from_net_addr.length - 1)
		hdr.from_net_addr += system.inet_addr;
}
log.writeln("---");
log.writeln("hdr = " + JSON.stringify(hdr, null, 4));
log.writeln("rcpt_list = " + JSON.stringify(rcpt_list, null, 4));

if(rcpt_list.length < 1) {
	writeln("No recipients specified!");
	exit(1);
}
var msgbase = new MsgBase('mail');
if(!msgbase.open()) {
	writeln("Cannot send email (open error)!");
	exit(1);
}
if(!msgbase.save_msg(hdr, body, rcpt_list)) {
	writeln("Cannot send email: " + msgbase.error);
	exit(1);
}

log.writeln("Sent successfully");
log.writeln("===");
