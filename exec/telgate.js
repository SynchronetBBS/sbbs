// telgate.js

// @format.tab-size 4, @format.use-tabs true

load("sbbsdefs.js");

write("\r\n\1h\1hPress \001yCtrl-]\001w for a control menu anytime.\r\n\r\n");
console.pause();
writeln("\001h\001yConnecting to: \001w"+argv[0]+"\001n");
var flags=TG_NONE;
if(argc>1)
	flags=eval(argv[1]);
bbs.telnet_gate(argv[0],flags);
console.clear();
