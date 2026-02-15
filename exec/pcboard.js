// PCBoard v15.1 menu/command emulation

// @format.tab-size 4, @format.use-tabs true

"use strict";

require("sbbsdefs.js", 'FI_REMOVE');
require("userdefs.js", 'USER_EXPERT');
var shell = load({}, "shell_lib.js");

if (bbs.main_cmds < 1) {
	console.clear();
	bbs.menu("pcboard/welcome");
}

// The menu-display/command-prompt loop
while(bbs.online && !js.terminated) {

	if (!(user.settings & USER_EXPERT)) {
		console.clear();
		bbs.menu("pcboard/brdm");
	}
	bbs.node_action = NODE_MAIN;
	bbs.nodesync();

	console.putmsg("\x01n\r\n\x01y\x01h(\x01r@MINLEFT@\x01y min. left) @GRP@ (@GN@) @SUB@ (@SN@) Command? \x01n", P_SAVEATR);
	var cmd = console.getstr(60, K_TRIM);
	if (!cmd)
		continue;

	bbs.log_str(cmd);
	bbs.main_cmds++;

	var word = cmd.split(" ");
	var arg = word[1];
	switch(word[0].toUpperCase()) {
		case 'HELP':
		case '?':
			if (user.settings & USER_EXPERT) {
				console.clear();
				bbs.menu("pcboard/brdm");
			}
			continue;
		case 'F':
			if (shell.select_file_area())
				bbs.list_files();
			continue;
		case 'EXT':
			if (shell.select_file_area())
				bbs.list_file_info();
			continue;
		case 'VIEW':
			shell.view_files();
			continue;
		case 'BATCH':
			bbs.batch_menu();
			continue;
		case 'REMOVE':
			shell.view_file_info(FI_REMOVE);
			continue;
		case 'D':
			shell.download_files();
			continue;
		case 'U':
			shell.upload_file();
			continue;
		case 'L':
			bbs.scan_dirs(FL_NO_HDR);
			continue;
		case 'N':
			if (!console.yesno("\r\n\x01b\x01hUse \x01c@LASTNEW@\x01b for new file scan date")) {
				var val = bbs.get_newscantime(bbs.new_file_time);
				if(val !== null)
					bbs.new_file_time = val;
			}
			bbs.scan_dirs(FL_ULTIME);
			continue;
		case 'Z':
			bbs.scan_dirs(FL_FINDDESC);
			continue;
		case 'T':
			bbs.temp_xfer();
			continue;
		case 'C':
			shell.send_feedback();
			continue;
		case 'Y':
			bbs.scan_subs(SCAN_TOYOU);
			continue;
		case 'TS':
			bbs.scan_subs(SCAN_FIND);
			continue;
		case 'QWK':
			bbs.qwk_sec();
			continue;
		case 'AUTO':
			bbs.auto_msg();
			continue;
		case 'SELECT':
			bbs.cfg_msg_scan(SCAN_CFG_NEW);
			continue;
		case 'A':
			bbs.cfg_msg_ptrs();
			continue;
		case 'R':
			bbs.scan_msgs();
			continue;
		case 'S':
			bbs.sys_info();
			continue;
		case 'E':
			bbs.post_msg();
			continue;
		case 'RN':
			bbs.scan_subs(SCAN_NEW);
			continue;
		case 'RC':
			bbs.scan_subs(SCAN_NEW | SCAN_CONT);
			continue;
		case 'J':
		case 'JOIN':
			if (arg && msg_area.sub[arg] && msg_area.sub[arg].can_access)
				bbs.cursub = arg;
			else
				shell.select_msg_area();
			continue;
		case 'M':
			bbs.email_sec();
			continue;
		case 'X':
			user.settings ^= USER_EXPERT;
			console.putmsg("\r\nExpert mode is now: @EXPERT@");
			continue;
		case 'B':
			bbs.text_sec();
			continue;
		case 'V':
			bbs.user_info();
			continue;
		case 'G':
			shell.logoff(/* fast: */false);
			continue;
		case 'W':
			bbs.user_config();
			exit();
		case 'O':
			if(!bbs.page_sysop()
				&& !deny(format(bbs.text(bbs.text.ChatWithGuruInsteadQ), system.guru || "The Guru")))
				bbs.page_guru();
			continue;
		case 'P':
			bbs.private_message();
			continue;
		case 'I':
			bbs.menu("../answer");
			console.clear();
			bbs.menu("pcboard/welcome");
			continue;
		case 'NODES':
			bbs.list_nodes();
			continue;
		case 'WHO':
			bbs.whos_online();
			continue;
		case 'CHAT':
			bbs.chat_sec();
			continue;
		case 'NEWS':
			if (!bbs.menu("logon", P_NOERROR))
				console.print("No news is good news.\r\n");
			continue;
		case 'DOOR':
		case 'OPEN':
			if (arg && xtrn_area.prog[arg] && xtrn_area.prog[arg].can_access)
				bbs.exec_xtrn(arg);
			else
				bbs.xtrn_sec();
			continue;
		case 'USER':
			var name = arg;
			if (!name) {
				console.print("\r\n\x01y\x01hUsername search string (Enter=List Conferences Users): \x01w");
				name = console.getstr(25, K_UPRLWR);
			}
			if (!name)
				bbs.list_users(UL_SUB);
			else {
				if (system.matchuser(name, /* sysop alias: */false))
					console.print("\x01y\x01hVerified: \x01w" + name);
				console.newline();
			}
			continue;
	}

	if(cmd[0] == ';') {
		load({}, "str_cmds.js", cmd.slice(1));
		continue;
	}
	
	console.putmsg("\r\n\x01r\x01hInvalid Entry!  Please try again, @FIRST@ ...\r\n");
}
