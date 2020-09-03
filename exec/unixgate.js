// unixgate.js

// Telnet Gateway for Unix servers - Requires v3.00c

// $Id: unixgate.js,v 1.1 2005/09/12 19:48:34 deuce Exp $

// @format.tab-size 4, @format.use-tabs true

load("sbbsdefs.js");

write("\r\n\1h\1hPress \001yCtrl-]\001w for a control menu anytime.\r\n\r\n");
console.pause();
writeln("\001h\001yConnecting to: \001w"+argv[0]+"\001n");
bbs.telnet_gate(argv[0],TG_PASSTHRU|TG_NOTERMTYPE);
console.clear();
