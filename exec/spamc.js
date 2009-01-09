// spamc.js

// SpamAssasin client for Synchronet
// For use as mailproc.ini script to check messages against a running/listening spamd

// Example mailproc.ini entries:

// ;Process and pass-through all messages:
// [spamc.js]

// ;Check for and reject SPAM messages:
// [spamc.js check]

// ;Check for and reject SPAM messages over specified threshold
// [spamc.js reject 8.0]

// $Id$

load('sockdefs.js');
load('salib.js');

function main()
{
	var address = '127.0.0.1';
	var tcp_port = 783;
	var user;
	var cmd = 'PROCESS';	// Default: process
	var reject_threshold;

	// Process arguments:
	for(i in argv) {

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

		// spamc.js command:
		else if(argv[i]=='reject')	// Only valid with PROCESS command
			reject_threshold = parseFloat(argv[++i]);
		else if(argv[i]=='check')
			cmd = 'CHECK';
		else if(argv[i]=='check-verbose' || argv[i]=='tests')
			cmd = 'SYMBOLS';
	}

	var msg=new SPAMC_Message(message_text_filename, address, tcp_port, user);
	if(msg.error != undefined) {
		log(LOG_ERR,"spamc: !ERROR "+msg.error);
		return;
	}
	msg.debug=true;
	log(LOG_INFO, "spamc: Executing command: " + cmd);
	var ret=msg.DoCommand(cmd);
	if(ret.warning != undefined)
		log(LOG_WARNING, "spamc: WARNING "+ret.warning);
	if(ret.error != undefined) {
		log(LOG_ERR,"spamc: !ERROR "+ret.error);
		return;
	}

	log(LOG_INFO, "spamc: Score: " + ret.score + ' / ' + ret.threshold);

	if(cmd == 'PROCESS' && (!reject_threshold || ret.score < reject_threshold)) {
		var msg_file = new File(message_text_filename);
		if(!msg_file.open("w")) {
			log(LOG_ERR,format("spamc: !ERROR %d opening message text file: %s"
				,msg_file.error, message_text_filename));
			return;
		}
		msg_file.write(ret.message);
		msg_file.close();
		return;
	}
	if(!ret.isSpam)
		return;
	log(LOG_INFO, "spamc: rejecting SPAM with SMTP error");
	var error_file = new File(processing_error_filename);
	if(!error_file.open("w")) {
		log(LOG_ERR,format("spamc: !ERROR %d opening processing error file: %s"
			,error_file.error, processing_error_filename));
		return;
	}
	var rejection = ret.score + ' / ' + ret.threshold;
	if(ret.symbols && ret.symbols.length)
		rejection += ' ' + ret.symbols;
	error_file.writeln("SpamAssassin rejected your mail: " + rejection);
	error_file.close();
}

main();
