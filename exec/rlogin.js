// Synchronet RLogin Gateway v2.0

// usage: ?rlogin address[:port] [options]
// options:
//   -c <client-name> (default: user alias)
//   -s <server-name> (default: user real name)
//   -t <terminal-type> (e.g. "xtrn=doorcode" to auto-exec door on server)
//   -T <connect-timeout-seconds> (default: 10 seconds)
//   -m <telnet-gateway-mode> (Number or TG_* vars OR'd together, default: 0)
//   -p send current user alias and password as server and client-name values
//   -q don't display banner or pause prompt (quiet)
//   -P don't pause for user key-press
//   -C don't clear screen after successful session

// legacy usage (still supported, but deprecated):
// ?rlogin address[:port] [telnet-gateway-mode] [client-name] [server-name] [terminal-type]

// See http://wiki.synchro.net/module:rlogin for details

"use strict";

require("sbbsdefs.js", 'TG_RLOGINSWAP');

var quiet = false;
var pause = true;
var clear = true;
var mode;
var addr;
var client_name;
var server_name;
var term_type;
var timeout = 10;

for(var i = 0; i < argv.length; i++) {
	var arg = argv[i];
	if(arg[0] != '-') {
		if(!addr)
			addr = arg;
		else if(!mode)
			mode = arg;
		else if(!client_name)
			client_name = arg;
		else if(!server_name)
			server_name = arg;
		else if(!term_type)
			term_type = arg;
		else {
			alert(js.exec_file + ": Unexpected argument: " + arg);
			exit(1);
		}
		continue;
	}
	switch(arg[1]) { // non-value options
		case 'q':
			quiet = true;
			continue;
		case 'P':
			pause = false;
			break;
		case 'C':
			clear = false;
			break;
		case 'p': // send alias and password as expected by Synchronet
			client_name = user.security.password;
			server_name = user.alias;
			continue;
	}
	var value = arg.length > 2 ? arg.substring(2) : argv[++i];
	switch(arg[1]) { // value options
		case 'c':
			client_name = value;
			break;
		case 's':
			server_name = value;
			break;
		case 't':
			term_type = value;
			break;
		case 'T':
			timeout = Number(value);
			break;
		case 'm':
			mode = value;
			break;
		default:
			alert(js.exec_file + ": Unrecognized option: " + arg);
			exit(1);
	}
}
if(!addr) {
	alert(js.exec_file + ": No destination address specified");
	exit(1);
}
if(!quiet) {
	write("\r\n\x01h\x01hPress \x01yCtrl-]\x01w for a control menu anytime.\r\n\r\n");
	if(pause)
		console.pause();
	writeln("\x01h\x01yConnecting to: \x01w" + addr + "\x01n");
}
mode = eval(mode);
var result = bbs.rlogin_gate(
	 addr // address[:port]
	,client_name || (mode & TG_RLOGINSWAP ? user.name : user.alias)
	,server_name || (mode & TG_RLOGINSWAP ? user.alias : user.name)
	,term_type
	,mode
	,timeout
	);
if(result === false)
	alert(js.exec_file + ": Failed to connect to: " + addr);
else if(clear)
	console.clear();
