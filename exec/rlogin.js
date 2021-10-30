// rlogin.js

// Telnet Gateway using RLogin protocol

// usage: ?rlogin address[:port] [telnet_gateway_mode] [client-name] [server-name] [terminal-type]

load("sbbsdefs.js");

write("\r\n\001h\1hPress \001yCtrl-]\001w for a control menu anytime.\r\n\r\n");
console.pause();
writeln("\001h\001yConnecting to: \001w" + argv[0] + "\001n");
bbs.rlogin_gate(
	 argv[0] // address[:port]
	,argv[2] // client-name
	,argv[3] // server-name
	,argv[4] // terminal-type
	,eval(argv[1]) // mode flags
	);
console.clear();
