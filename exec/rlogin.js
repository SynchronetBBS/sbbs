// rlogin.js

// Telnet Gateway using RLogin protocol - Requires v3.00c

// $Id: rlogin.js,v 1.4 2017/10/25 08:59:15 rswindell Exp $

// @format.tab-size 4, @format.use-tabs true

load("sbbsdefs.js");

write("\r\n\001h\1hPress \001yCtrl-]\001w for a control menu anytime.\r\n\r\n");
console.pause();
writeln("\001h\001yConnecting to: \001w" + argv[0] + "\001n");
var flags = 0;
if (argc > 1)
    flags = eval(argv[1]);
bbs.rlogin_gate(argv[0], flags);
console.clear();
