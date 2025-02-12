// Forums Section

'use strict';
require("sbbsdefs.js", "K_UPPER");
require("userdefs.js", "USER_EXPERT");
require("nodedefs.js", "NODE_MAIN");
require("gettext.js", "gettext");

var shell = load({}, "shell_lib.js");
var text = bbs.mods.text;
var last_str_cmd = "";
if(!text)
	text = load(bbs.mods.text = {}, "text.js");

main:
while(bbs.online && !console.aborted) {
	console.aborted = false;
	if(!(user.settings & USER_EXPERT)) {
		console.home();
		bbs.menu("mode7/mode7_forums");
	}

	bbs.replace_text(720,'at forums menu via ' + client.protocol.toLowerCase());
        bbs.node_action = NODE_CUSTOM;
	bbs.nodesync();
	bbs.revert_text(720);

	console.crlf();
        console.putmsg ("\x83@GRP@\x86<@GN@>\x83@SUB@\x86<@SN@>");
	console.crlf();
	console.crlf();
        if(user.compare_ars("exempt T"))
                console.putmsg ("\x86Time Used: @TUSED@");
        else
                console.putmsg ("\x86Time Left: @TLEFT@");

        // Display main Prompt
        console.putmsg ("\x87->\x83Forums\x87-> ");

	var cmd = console.getkey(K_UPPER);
	console.print(cmd);
	switch(cmd) {
		case 'N':	writeln('\r\n\x86' + gettext("New Message Scan"));
				bbs.scan_subs(SCAN_NEW);
				break;
		case 'R':	bbs.scan_msgs();
				break;
		case 'L':	bbs.list_msgs()
				break;
		case 'P':	bbs.post_msg();
				break;
		case 'C':	writeln('\r\n\x86' + gettext("Continuous New Message Scan") + '\r\n');
				bbs.scan_subs(SCAN_CONT);
				break;
		case 'C':	bbs.exec("?filescancfg.js");
				break;
		case 'M':	console.newline();
				var res = bbs.exec("?postpoll.js");
				if(console.aborted) {
					console.aborted = false;
				}
				if(res > 1)
					console.pause();
				break;
		case 'V':	bbs.exec("?scanpolls.js");
				break;
		case 'T':	console.home();
				bbs.qwk_sec()
				break;
		case 'J':	shell.select_msg_area()
				break;
		case 'F':	writeln('\r\n\x86' + gettext("Find Text in Messages") + '\r\n');
				bbs.scan_subs(SCAN_FIND);
				break;
		case 'S':	writeln('\r\n\x86' + gettext("Scan for Messages Posted to You") + '\r\n');
				bbs.scan_subs(SCAN_TOYOU)
				break;
		case '&':	console.home()
				bbs.exec("?eotl_msgscanconf.js");
				break;
		case '!':	if (user.compare_ars("SYSOP"))
					bbs.menu("sysmain");
				break;
		case '*':	shell.show_subs(bbs.curgrp)
				break;
		case '#':	writeln('\r\n\x86' + gettext("Type the actual number, not the symbol."));
				console.pause();
				break;
		case KEY_RIGHT:
		case '>':
		case '}':
		case ')':
		case '+':
				shell.sub_up();
				break;
		case KEY_LEFT:
		case '<':
		case '{':
		case '(':
		case '-':	shell.sub_down();
				break;
		case KEY_UP:
		case ']': 	shell.grp_up();
				break;
		case KEY_DOWN:
		case '[': 	shell.grp_down();
				break;
		case '?':	// Display menu
			if(user.settings & USER_EXPERT)
				bbs.menu("mode7/mode7_forums");
				break;
		case 'Q':	exit();
				break;
	}
	if(cmd >= '1' && cmd <= '9') {
		shell.get_sub_num(cmd);
		continue;
	}
	if(cmd == ';' && user.compare_ars("SYSOP")) {
		cmd = console.getstr();
		if(cmd == '!')
			cmd = last_str_cmd;
		var script = system.mods_dir + "str_cmds.js";
		if(!file_exists(script))
			script = system.exec_dir + "str_cmds.js";
		js.exec(script, {}, cmd);
		last_str_cmd = cmd;
		continue;
	}

	if(cmd == '/') {
		cmd = console.getkey(K_UPPER);
		console.print(cmd);
		switch(cmd) {
			case 'D':	if (user.compare_ars("REST NOT D")) {
						writeln('\r\n\x86' + gettext("Download File(s) from User(s)"));
						shell.download_user_files();
					}
					continue main;
			case 'F':	writeln('\r\n\x86' + gettext("Find Text in Messages"));
					bbs.scan_subs(SCAN_FIND);
					continue main;
			case 'N':	bbs.scan_subs(SCAN_NEW, /* all */true);
					continue main;
			case 'U':	writeln('\r\n\x86' + gettext("Upload File to User"));
					shell.upload_user_file();
					continue main;
			case 'O':	shell.logoff(/* fast: */true);
					continue main;
			case 'S':	bbs.scan_subs(SCAN_TOYOU, /* all */true)
					continue main;
			case '*':	shell.show_grps();
					continue main;
			case '#':	writeln('\r\n\x86' + gettext("Type the actual number, not the symbol."));
					console.pause();
					continue main;
		}
			
		if(cmd >= '1' && cmd <= '9') {
			shell.get_grp_num(cmd);
			continue;
		}

        }
	console.home();
	console.aborted = false;
}
