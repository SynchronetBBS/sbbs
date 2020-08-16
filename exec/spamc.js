// spamc.js

// SpamAssasin client for Synchronet
// For use as mailproc.ini script to check messages against a running/listening spamd

// $Id: spamc.js,v 1.22 2015/08/15 09:42:36 rswindell Exp $

// ---------------------------------------------------------------------------
// Example mailproc.ini entries:

// ;Modify and pass-through all messages:
// [spamc.js]

// ;Modify SPAM messages only, pass-through all:
// [spamc.js spamonly]

// ;Reject SPAM messages, modify and pass-through HAM:
// [spamc.js reject]

// ;Reject SPAM messages over specified score threshold, modify and pass-through HAM:
// [spamc.js reject 8.0]

// ;Reject SPAM messages over specified score threshold, modify SPAM, and pass-through HAM&SPAM:
// [spamc.js reject 8.0 spamonly]
// ---------------------------------------------------------------------------


// Options:
// dest <ip_address>
// port <tcp_port>
// username <user>
// max-size <bytes>
// reject [threshold]
// spamonly
// debug

load('sockdefs.js');
load('salib.js');

function main()
{
	var address = '127.0.0.1';
	var tcp_port = 783;
	var user;
	var reject = false;
	var reject_threshold;
	var spamonly = false;
	var debug = false;
	var	max_size = 500000;	/* bytes */

	// Process arguments:
	for(i=0; i<argc; i++) {

		// Strip any pre-pended slashes
		while(argv[i].charAt(0)=='-')
			argv[i]=argv[i].slice(1);

		// Standard spamc options:
		if(argv[i]=='d' || argv[i]=='dest')	
			address = argv[++i]; // Note: only one address supported (unlike spamc)
		else if(argv[i]=='p' || argv[i]=='port')
			tcp_port = Number(argv[++i]);
		else if(argv[i]=='u' || argv[i]=='username')
			user = argv[++i];
		else if(argv[i]=='s' || argv[i]=='max-size')
			max_size = Number(argv[++i]);

		// spamc.js command:
		else if(argv[i]=='reject') {
			reject = true;
			if(!isNaN(reject_threshold = parseFloat(argv[i+1])))
				i++;
		}
		else if(argv[i]=='spamonly')
			spamonly = true;
		else if(argv[i]=='debug')
			debug = true;
	}
	if(max_size && file_size(message_text_filename) > max_size) {
		log(LOG_INFO,"message size > max_size (" + max_size + ")");
		return;
	}
	if(system.findstr(system.ctrl_dir + "dnsbl_exempt.cfg", reverse_path)) {
		log("Sender exempt from SPAM checking: " + reverse_path);
		return;
	}
	var msg=new SPAMC_Message(message_text_filename, address, tcp_port, user);
	if(msg.error != undefined) {
		log(LOG_ERR,"!ERROR "+msg.error);
		return;
	}

	log(LOG_INFO, "processing message with SPAMD at " + address + " port " + tcp_port);
	msg.debug = debug;
	msg.reverse_path = reverse_path;
//	if(this.hello_name)
//		msg.hello_name = this.hello_name;
	var ret=msg.process();
	if(ret.warning != undefined)
		log(LOG_WARNING, "WARNING "+ret.warning);
	if(ret.error != undefined) {
		log(LOG_ERR,"!ERROR "+ret.error);
		return;
	}

	var details = 'score: ' + ret.score + ' / ' + ret.threshold;
	if(ret.symbols && ret.symbols.length)
		details += ', tests: ' + ret.symbols;

	log(LOG_INFO, details);
	if(!ret.isSpam || ret.score < reject_threshold)
		reject = false;

	if(reject) {
		log(LOG_INFO, "rejecting SPAM with SMTP error");
		var error_file = new File(processing_error_filename);
		if(!error_file.open("w")) {
			log(LOG_ERR,format("!ERROR %d opening processing error file: %s"
				,error_file.error, processing_error_filename));
			return;
		}
		error_file.writeln("SpamAssassin rejected your mail: " + details);
		error_file.close();
		system.spamlog("SMTP","REJECTED"
			,"SpamAssassin " + details
			,client.host_name, client.ip_address
			,recipient_address
			,reverse_path);
		return;
	}

	// Modify message
	if(spamonly && !ret.isSpam)
		return;
	log(LOG_INFO, "re-writing message");
	var msg_file = new File(message_text_filename);
	if(!msg_file.open("wb")) {
		log(LOG_ERR,format("!ERROR %d opening message text file: %s"
			,msg_file.error, message_text_filename));
		return;
	}
	msg_file.write(ret.message);
	msg_file.close();
}

main();
