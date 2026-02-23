// File Scan Configuration menu

"use strict";

require("sbbsdefs.js", "USER_EXPERT");
require("gettext.js", "gettext");
var prompts = bbs.mods.prompts || load(bbs.mods.prompts = {}, "user_info_prompts.js");
var options = load("modopts.js", "filescancfg");
if (!options)
	options = {};

const menufile = "xfercfg";

while(bbs.online && !js.terminated) {
	if(!(user.settings & USER_EXPERT))
		bbs.menu(menufile);
	bbs.nodesync();
	console.print(options.prompt || ("\r\n\x01y\x01h" + gettext("Config") + ": \x01n"), P_ATCODES);
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
			if (user.settings & USER_EXPERT) {
				console.print("\r\n" + gettext("Batch flagging in file listings is now") + ": \x01h");
				console.print(bbs.text((user.settings & USER_BATCHFLAG) ? bbs.text.On : bbs.text.Off));
				console.newline();
			}
			break;
		case 'E':
			user.settings ^= USER_EXTDESC;
			if (user.settings & USER_EXPERT) {
				console.print("\r\n" + gettext("Extended file description display is now") + ": \x01h");
				console.print(bbs.text((user.settings & USER_EXTDESC) ? bbs.text.On : bbs.text.Off));
				console.newline();
			}
			break;
		case 'Z':
			prompts.get_protocol(user, options);
			break;
		default:
			exit();
			
	}
	if(user.settings & USER_EXPERT)
		break;
}
