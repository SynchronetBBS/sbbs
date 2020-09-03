// mudgate.js

// $Id: mudgate.js,v 1.1 2005/09/12 19:02:31 deuce Exp $

// @format.tab-size 4, @format.use-tabs true

load("sbbsdefs.js");

write("\r\n\1h\1hPress \001yCtrl-]\001w for a control menu anytime.\r\n\r\n");
console.pause();
write("\001h\001yConnecting to MUD: \001w"+argv[0]+"\001n\r\n");
bbs.telnet_gate(argv[0],TG_ECHO|TG_CRLF|TG_LINEMODE|TG_NODESYNC|TG_CTRLKEYS);
console.clear();
