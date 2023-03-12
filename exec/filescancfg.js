// File Scan Configuration menu

require("sbbsdefs.js", "USER_EXPERT");

"use strict";

const menufile = "xfercfg";

while(bbs.online && !js.terminated) {
	if(!(user.settings & USER_EXPERT))
		bbs.menu(menufile);
	bbs.nodesync();
	console.print("\r\n\x01y\x01hConfig: \x01n");
	var key = console.getkeys("?QBEP\r");
	bbs.log_key(key);

	switch(key) {
		case '?':
			if(user.settings & USER_EXPERT)
				bbs.menu(menufile);
			continue;
		case 'P':
			var val = bbs.get_newscantime(bbs.new_file_time);
			if(val !== null)
				bbs.new_file_time = val;
			break;
		case 'B':
			user.settings ^= USER_BATCHFLAG;
			console.print("\r\nBatch flagging in file listings is now: \1h");
			if(user.settings & USER_BATCHFLAG)
				console.print("ON");
			else
				console.print("OFF");
			console.crlf();
			break;
		case 'E':
			user.settings ^= USER_EXTDESC;
			console.print("\r\nExtended file description display is now: \1h");
			if(user.settings & USER_EXTDESC)
				console.print("ON");
			else
				console.print("OFF");
			console.crlf();
			break;
	}
	break;
}
