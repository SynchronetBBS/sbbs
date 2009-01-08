// spamc.js

// SpamAssasin client for Synchronet
// For use as mailproc.ini script to check messages with spamd

// $Id$

load('sockdefs.js');
load('salib.js');

var spamd_address = '127.0.0.1';
var spamd_tcp_port = 783;

function main()
{
	// Process arguments:
	for(i in argv) {
		if(argv[i]=='-d' || argv[i]=='--dest')
			spamd_address = argv[++i];
		else if(argv[i]=='-p' || argv[i]=='--port')
			spamd_tcp_port = Number(argv[++i]);
	}

	var msg=new SPAMC_Message(message_text_filename, spamd_address, spamd_tcp_port);
	if(msg === false)
		return;
	var ret=msg.check();
	if(ret === false)
		return;
	if(!ret.isSpam)
		return;
	var error_file = new File(processing_error_filename);
	if(!error_file.open("w")) {
		log(LOG_ERR,format("spamc: !ERROR %d opening processing error file: %s"
			,error_file.error, processing_error_filename));
		return;
	}
	error_file.writeln("SpamAssassin rejected your mail: " + ret.score + ' / ' + ret.threshold);
	error_file.close();
}

main();
