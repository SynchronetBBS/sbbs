// File Scan Configuration menu

require("sbbsdefs.js", "USER_EXPERT");
require("gettext.js", "gettext");
var prompts = bbs.mods.prompts || load(bbs.mods.prompts = {}, "user_info_prompts.js");

"use strict";

const menufile = "xfercfg";

while(bbs.online && !js.terminated) {
	if(!(user.settings & USER_EXPERT))
		bbs.menu(menufile);
	bbs.nodesync();
	console.print("\r\n\x01y\x01h" + gettext("Config") + ": \x01n");
	var key = console.getkeys("?QBEPZ\r");
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
			console.print("\r\n" + gettext("Batch flagging in file listings is now") + ": \1h");
			console.print(bbs.text((user.settings & USER_BATCHFLAG) ? bbs.text.On : bbs.text.Off));
			console.crlf();
			break;
		case 'E':
			user.settings ^= USER_EXTDESC;
			console.print("\r\n" + gettext("Extended file description display is now") + ": \1h");
			console.print(bbs.text((user.settings & USER_EXTDESC) ? bbs.text.On : bbs.text.Off));
			console.crlf();
			break;
		case 'Z':
			log("OK");		
			prompts.get_protocol();
			log("Wha?!?");			
			break;
		default:
			exit();
			
	}
	if(user.settings & USER_EXPERT)
		break;
}
