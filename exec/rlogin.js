// Synchronet RLogin Gateway v2.1

// usage: ?rlogin address[:port] [options]
// options:
//   -c <client-name> (default: user alias) - may be specified multiple
//   -s <server-name> (default: user real name) - may be specified multiple
//   -t <terminal-type> (e.g. "xtrn=doorcode" to auto-exec door on server)
//   -T <connect-timeout-seconds> (default: 10 seconds)
//   -m <telnet-gateway-mode> (Number or TG_* vars OR'd together, default: 0)
//   -p send current user alias and password as server and client-name values
//   -h[pepper] send current user alias and hashed-user-password as server and client-name
//   -H <password> send current user alias and specified hashed-password as server and client-name
//   -q don't display banner or pause prompt (quiet)
//   -v increase verbosity (display remote host name/address/port in messages)
//   -P don't pause for user key-press
//   -C don't clear screen after successful session

// legacy usage (still supported, but deprecated):
// ?rlogin address[:port] [telnet-gateway-mode] [client-name] [server-name] [terminal-type]

// See http://wiki.synchro.net/module:rlogin for details

"use strict";

require("sbbsdefs.js", 'TG_RLOGINSWAP');

var mode = 0;
var addr;
var client_name = '';
var server_name = '';
var term_type;
var options;
if((options = load({}, "modopts.js","rlogin")) == null) {
	if((options = load({}, "modopts.js","telgate")) == null)
		options = {};
}
var quiet = options.quiet === undefined ? false : options.quiet;
var pause = options.pause === undefined ? true : options.pause;
var clear = options.clear === undefined ? true : options.clear;
var timeout = options.timeout === undefined ? 10 : options.timeout;
var verbosity = options.verbosity === undefined ? 0 : options.verbosity;

function hashed_password(password, pepper)
{
	return sha1_calc(password
		+ user.number
		+ user.stats.firston_date
		+ (options.salt || system.qwk_id)
		+ pepper
		, /* hex: */true);
}

for(var i = 0; i < argv.length; i++) {
	var arg = argv[i];
	if(arg[0] != '-') {
		if(!addr)
			addr = arg;
		else if(!mode)
			mode = eval(arg);
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
			continue;
		case 'C':
			clear = false;
			continue;
		case 'v':
			++verbosity;
			continue;
		case 'h': // send alias and hashed-user-password
			client_name = hashed_password(user.security.password, arg.substring(2));
			server_name = user.alias;
			continue;
		case 'p': // send alias and password as expected by Synchronet
			client_name = user.security.password;
			server_name = user.alias;
			continue;
	}
	var value = arg.length > 2 ? arg.substring(2) : argv[++i];
	switch(arg[1]) { // value options
		case 'c':
			client_name += value;
			break;
		case 's':
			server_name += value;
			break;
		case 't':
			term_type = value;
			break;
		case 'T':
			timeout = Number(value);
			break;
		case 'm':
			mode = eval(value);
			break;
		case 'H': // send alias and hashed-password
			client_name = hashed_password(value, "");
			server_name = user.alias;
			continue;
		default:
			alert(js.exec_file + ": Unrecognized option: " + arg);
			exit(1);
	}
}
if(!addr) {
	alert(js.exec_file + ": No destination address specified");
	exit(1);
}
var remote_host = (verbosity > 0 ? addr : "remote host");
if(!quiet) {
	write(options.help_msg || "\r\n\x01h\x01hPress \x01yCtrl-]\x01w for a control menu anytime.\r\n\r\n");
	if(pause)
		console.pause();
	write(format(options.connecting_msg || "\x01h\x01yConnecting to \x01w%s \x01n...\r\n", remote_host));
}
var result = bbs.rlogin_gate(
	 addr // address[:port]
	,client_name || (mode & TG_RLOGINSWAP ? user.name : user.alias)
	,server_name || (mode & TG_RLOGINSWAP ? user.alias : user.name)
	,term_type
	,mode
	,timeout
	);
if(result === false)
	alert(options.failed_connect_msg || (js.exec_file + ": Failed to connect to " + remote_host));
else if(clear)
	console.clear();
