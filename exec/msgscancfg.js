// Message Scan Configuration menu

require("sbbsdefs.js", "USER_EXPERT");

"use strict";

const menufile = "maincfg";

while(bbs.online) {
	if(!(user.settings & USER_EXPERT))
		bbs.menu(menufile);
	bbs.nodesync();
	console.print("\r\n\x01y\x01hConfig: \x01n");
	var key = console.getkeys("?QNPISRW\r");
	bbs.log_key(key);

	switch(key) {
		case '?':
			if(user.settings & USER_EXPERT)
				bbs.menu(menufile);
			continue;
		case 'N':
			bbs.cfg_msg_scan(SCAN_CFG_NEW);
			break;
		case 'S':
			bbs.cfg_msg_scan(SCAN_CFG_TOYOU);
			break;
		case 'P':
			bbs.cfg_msg_ptrs();
			break;
		case 'I':
			bbs.reinit_msg_ptrs();
			break;
		case 'R':
			bbs.reload_msg_scan();
			console.print("\r\nMessage scan configuration and pointers re-loaded.\r\n");
			break;
		case 'W':
			bbs.save_msg_scan();
			console.print("\r\nMessage scan configuration and pointers saved.\r\n");
			break;
	}
	exit();
}
