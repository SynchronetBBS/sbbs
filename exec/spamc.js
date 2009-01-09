// spamc.js

// SpamAssasin client for Synchronet
// For use as mailproc.ini script to check messages against a running/listening spamd

// Example mailproc.ini entries:

// [spamc.js -c]

// $Id$

load('sockdefs.js');
load('salib.js');

function main()
{
	var address = '127.0.0.1';
	var tcp_port = 783;
	var user;
	var cmd = 'PROCESS';	// Default: process
	var threshold;

	// Process arguments:
	for(i in argv) {
		if(argv[i]=='-d' || argv[i]=='--dest')	
			address = argv[++i]; // Note: only one address supported (unlike spamc)
		else if(argv[i]=='-p' || argv[i]=='--port')
			tcp_port = Number(argv[++i]);
		else if(argv[i]=='-u' || argv[i]=='--username')
			user = argv[++i];
		else if(argv[i]=='-T' || argv[i]=='--threshold')
			threshold = parseFloat(argv[++i]);
		else if(argv[i]=='-c' || argv[i]=='--check')
			cmd = 'CHECK';
		else if(argv[i]=='-y' || argv[i]=='--tests')
			cmd = 'SYMBOLS';
		else if(argv[i]=='-R' || argv[i]=='--full')
			cmd = 'REPORT';
		else if(argv[i]=='-r' || argv[i]=='--fullspam')
			cmd = 'REPORT_IFSPAM';
	}

	var msg=new SPAMC_Message(message_text_filename, address, tcp_port, user);
	if(msg.error != undefined) {
		log(LOG_ERR,"spamc: !ERROR "+msg.error);
		return;
	}
	msg.debug=true;
	log(LOG_INFO, "spamc: Executing command: " + cmd);
	var ret=msg.DoCommand(cmd);
	if(ret.error != undefined) {
		log(LOG_ERR,"spamc: !ERROR "+ret.error);
		return;
	}
	if(!isNaN(ret.score)) {
		log(LOG_INFO, "spamc: Score: " + ret.score + ' / ' + ret.threshold);
		if(threshold && ret.score < threshold)
			var ret=msg.DoCommand(cmd='PROCESS');
	}
	if(cmd == 'PROCESS') {
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
