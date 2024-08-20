// Synchronet Telnet Gateway v2.0

// @format.tab-size 4, @format.use-tabs true

// usage: ?telgate address[:port] [options]
// options:
//   -T <connect-timeout-seconds> (default: 10 seconds)
//   -m <telnet-gateway-mode> (Number or TG_* vars OR'd together, default: 0)
//   -q don't display banner or pause prompt (quiet)
//   -v increase verbosity (display remote host name/address/port in messages)
//   -P don't pause for user key-press
//   -C don't clear screen after successful session
//   -N send blank line (CRLF) after connect (multiple may be specified)
//   -s <string-to-send after connect> (multiple may be specified)
//   -S <CRLF-terminated-string-to-send> (multiple may be specified)

// legacy usage (still supported, but deprecated):
// ?telgate address[:port] [telnet-gateway-mode]

// See https://wiki.synchro.net/module:telgate for details

load("sbbsdefs.js");

"use strict";

var mode = 0;
var addr;
var send = [];
var options;
if((options = load({}, "modopts.js","telgate")) == null)
	options = {};
var quiet = options.quiet === undefined ? false : options.quiet;
var pause = options.pause === undefined ? true : options.pause;
var clear = options.clear === undefined ? true : options.clear;
var timeout = options.timeout === undefined ? 10 : options.timeout;
var verbosity = options.verbosity === undefined ? 0 : options.verbosity;

for(var i = 0; i < argv.length; i++) {
	var arg = argv[i];
	if(arg[0] != '-') {
		if(!addr)
			addr = arg;
		else if(!mode)
			mode = eval(arg);
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
		case 'N':
			send.push("\r\n");
			continue;
	}
	var value = arg.length > 2 ? arg.substring(2) : argv[++i];
	switch(arg[1]) { // value options
		case 'T':
			timeout = Number(value);
			break;
		case 'm':
			mode = eval(value);
			break;
		case 's':
			send.push(value);
			break;
		case 'S':
			send.push(value + '\r\n');
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
var remote_host = (verbosity > 0 ? addr : "remote host");
if(!quiet) {
	write(options.help_msg || "\r\n\x01h\x01hPress \x01yCtrl-]\x01w for a control menu anytime.\r\n\r\n");
	if(pause)
		console.pause();
	write(format(options.connecting_msg || "\x01h\x01yConnecting to \x01w%s \x01n...\r\n", remote_host));
}

var result = bbs.telnet_gate(addr, mode, timeout, send);
if(result === false)
	alert(options.failed_connect_msg || (js.exec_file + ": Failed to connect to " + remote_host));
else if(clear)
	console.clear();
