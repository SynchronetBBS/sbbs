// spamc.js

// SpamAssasin client for Synchronet
// For use as mailproc.ini script to check messages with spamd

// $Id$

load('sockdefs.js');

var spamd_address = '127.0.0.1';
var spamd_tcp_port = 783;

var sock = new Socket(SOCK_STREAM, 'spamd');

function writeln(str)
{
	sock.send(str + '\r\n');
}

function main()
{
	// Process arguments:
	for(i in argv) {
		if(argv[i]=='-d' || argv[i]=='--dest')
			spamd_address = argv[++i];
		else if(argv[i]=='-p' || argv[i]=='--port')
			spamd_tcp_port = Number(argv[++i]);
	}

	if(!sock.connect(spamd_address, spamd_tcp_port)) {
		log('Socket error ' + sock.error + ' connecting to ' + spamd_address);
		return;
	}

	writeln('CHECK SPAMC/1.2');
	writeln('Content-length: ' + file_size(message_text_filename));
	writeln('');
	sock.sendfile(message_text_filename);
	var response=sock.recvline();
	log('SPAMD response: ' + response);
	var token=response.split(/\s+/);
	if(token[1]!='0') {
		log(LOG_ERR,'SPAMD ERROR: ' + response);
		return;
	}
	var results=sock.recvline();
	log('SPAMD results: ' + results);
	if(results.split(';')[0] != 'Spam: True ')
		return;
	var error_file = new File(processing_error_filename);
	if(!error_file.open("w")) {
		log(LOG_ERR,format("spamc: !ERROR %d opening processing error file: %s"
			,error_file.error, processing_error_filename));
		return;
	}
	error_file.writeln("SpamAssassin rejected your mail: " + results);
	error_file.close();
}

main();
