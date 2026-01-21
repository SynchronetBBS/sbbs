// Default/Classic Synchronet Command Shell
// replaces default.src/bin

// @format.tab-size 4

"use strict";

require("sbbsdefs.js", "K_UPPER");
require("userdefs.js", "UFLAG_T");
require("nodedefs.js", "NODE_MAIN");
require("key_defs.js", "KEY_UP");
require("gettext.js", "gettext");
load("termsetup.js");
var shell = load({}, "shell_lib.js");

system.settings &= ~SYS_RA_EMU; // Use (R)e-read and (A)uto-reply keys

const help_key = '?';
// If user has unlimited time, display time-used rather than time-remaining
const time_code = user.security.exemptions & UFLAG_T ? "@TUSED@" : "@TLEFT@";

const main_menu = {
	file: "main",
	eval: 'bbs.main_cmds++',
	node_action: NODE_MAIN,
	prompt: gettext("\x01-\x01c\xfe \x01b\x01h", "shell_prompt_begin")
		+ gettext("Main")
		+ gettext(" \x01n\x01c\xfe \x01h", "shell_prompt_middle")
		+ time_code
		+ gettext(" \x01n\x01c[\x01h@GN@\x01n\x01c] @GRP@\x01\\ [\x01h@SN@\x01n\x01c] @SUB@: \x01n", "shell_main_prompt_end"),
	num_input: shell.get_sub_num,
	slash_num_input: shell.get_grp_num,
	command: {
	 'A': { eval: 'bbs.auto_msg()' },
	'/A': { exec: 'avatar_chooser.js'
			,ars: 'ANSI and not GUEST'
			,err: '\r\n' + gettext("Sorry, only regular users with ANSI terminals can do that.") + '\r\n' },
	 'B': { eval: 'bbs.scan_subs(SCAN_BACK)'
			,msg: '\r\n\x01c\x01h' + gettext("Browse/New Message Scan") + '\r\n' },
	 'C': { eval: 'bbs.chat_sec()' },
	 'D': { eval: 'bbs.user_config(); exit()' },
	 'E': { exec: 'email_sec.js' },
	 'F': { eval: 'bbs.scan_subs(SCAN_FIND)'
			,msg: '\r\n\x01c\x01h' + gettext("Find Text in Messages") + '\r\n' },
	'/F': { eval: 'bbs.scan_subs(SCAN_FIND, /* all */true)' },
	 'G': { eval: 'bbs.text_sec()' },
	 'I': { eval: 'shell.main_info()' },
	 'J': { eval: 'shell.select_msg_area()' },
	 'L': { eval: 'bbs.list_msgs()' },
	'/L': { eval: 'bbs.list_nodes()' },
	 'M': { eval: 'bbs.time_bank()' },
	'/M': { exec: 'postmeme.js' },
	 'N': { eval: 'bbs.scan_subs(SCAN_NEW)'
			,msg: '\r\n\x01c\x01h' + gettext("New Message Scan") + '\r\n' },
	'/N': { eval: 'bbs.scan_subs(SCAN_NEW, /* all */true)' },
	 'O': { eval: 'shell.logoff(/* fast: */false)' },
	'/O': { eval: 'shell.logoff(/* fast: */true)' },
	 'P': { eval: 'bbs.post_msg()' },
	'/P': { exec: 'postpoll.js' },
	 'Q': { eval: 'bbs.qwk_sec()' },
	 'R': { eval: 'bbs.scan_msgs()' },
	 'S': { eval: 'bbs.scan_subs(SCAN_TOYOU)'
			,msg: '\r\n\x01c\x01h' + gettext("Scan for Messages Posted to You") + '\r\n' },
	'/S': { eval: 'bbs.scan_subs(SCAN_TOYOU, /* all */true)' },
	 'U': { eval: 'shell.list_users()' },
	'/U': { eval: 'bbs.list_users(UL_ALL)' },
	 'V': { exec: 'scanpolls.js' },
	'/V': { exec: 'scanpolls.js', args: ['all'] },
	 'W': { eval: 'bbs.whos_online()' },
	 'X': { eval: 'bbs.xtrn_sec()' },
	 'Z': { eval: 'bbs.scan_subs(SCAN_NEW | SCAN_CONT)'
			,msg: '\r\n\x01c\x01h' + gettext("Continuous New Message Scan") + '\r\n' },
	'/Z': { eval: 'bbs.scan_subs(SCAN_NEW | SCAN_CONT, /* all */true)' },
	 '*': { eval: 'shell.show_subs(bbs.curgrp)' },
	'/*': { eval: 'shell.show_grps()' },
	 '&': { exec: 'msgscancfg.js' },
	 '!': { eval: 'bbs.menu("sysmain")'
			,ars: 'SYSOP or EXEMPT Q or I or N' },
	 '#': {  msg: '\r\n\x01c\x01h' + gettext("Type the actual number, not the symbol.") + '\r\n' },
	'/#': {  msg: '\r\n\x01c\x01h' + gettext("Type the actual number, not the symbol.") + '\r\n' },
	},
	nav: {
	'\r': { },
	 'T': { eval: 'shell.enter_file_section(); menu = file_menu' },
	 '>': { eval: 'shell.sub_up()' },
	 '}': { eval: 'shell.sub_up()' },
	 ')': { eval: 'shell.sub_up()' },
	 '+': { eval: 'shell.sub_up()' },
	 '=': { eval: 'shell.sub_up()' },
	 '<': { eval: 'shell.sub_down()' },
	 '{': { eval: 'shell.sub_down()' },
	 '(': { eval: 'shell.sub_down()' },
	 '-': { eval: 'shell.sub_down()' },
	 ']': { eval: 'shell.grp_up()' },
	 '[': { eval: 'shell.grp_down()' },
	},
};

// Can't do these statically through initialization:
main_menu.nav[KEY_UP] = { eval: 'shell.sub_up()' };
main_menu.nav[KEY_DOWN] = { eval: 'shell.sub_down()' };
main_menu.nav[KEY_RIGHT] = { eval: 'shell.grp_up()' };
main_menu.nav[KEY_LEFT] = { eval: 'shell.grp_down()' };

const file_menu = {
	file: "transfer",
	eval: 'bbs.file_cmds++',
	node_action: NODE_XFER,
	prompt: gettext("\x01-\x01c\xfe \x01b\x01h", "shell_prompt_begin")
		+ gettext("File")
		+ gettext(" \x01n\x01c\xfe \x01h", "shell_prompt_middle")
		+ time_code
		+ gettext(" \x01n\x01c(\x01h@LN@\x01n\x01c) @LIB@\x01\\ (\x01h@DN@\x01n\x01c) @DIR@: \x01n", "shell_file_prompt_end"),
	num_input: shell.get_dir_num,
	slash_num_input: shell.get_lib_num,
	command: {
	 'B': { eval: 'bbs.batch_menu()' },
	 'C': { eval: 'bbs.chat_sec()' },
	 'D': { eval: 'shell.download_files()'
			,msg: '\r\n\x01c\x01h' + gettext("Download File(s)") + '\r\n'
			,ars: 'REST NOT D' },
	'/D': { eval: 'shell.download_user_files()'
			,msg: '\r\n\x01c\x01h' + gettext("Download File(s) from User(s)") + '\r\n'
			,ars: 'REST NOT D' },
	 'E': { eval: 'shell.view_file_info(FI_INFO)'
			,msg: '\r\n\x01c\x01h' + gettext("List Extended File Information") + '\r\n' },
	 'F': { eval: 'bbs.scan_dirs(FL_FINDDESC);'
			,msg: '\r\n\x01c\x01h' + gettext("Find Text in File Descriptions (no wildcards)") + '\r\n' },
	'/F': { eval: 'bbs.scan_dirs(FL_FINDDESC, /* all: */true);' },
	 'I': { eval: 'shell.file_info()' },
	 'J': { eval: 'shell.select_file_area()' },
	 'L': { eval: 'shell.list_files()' },
	'/L': { eval: 'bbs.list_nodes()' },
	 'N': { eval: 'bbs.scan_dirs(FL_ULTIME)'
			,msg: '\r\n\x01c\x01h' + gettext("New File Scan") + '\r\n' },
	'/N': { eval: 'bbs.scan_dirs(FL_ULTIME, /* all */true)' },
	 'O': { eval: 'shell.logoff(/* fast: */false)' },
	'/O': { eval: 'shell.logoff(/* fast: */true)' },
	 'R': { eval: 'shell.view_file_info(FI_REMOVE)'
			,msg: '\r\n\x01c\x01h' + gettext("Remove/Edit File(s)") + '\r\n' },
	 'S': { eval: 'bbs.scan_dirs(FL_NO_HDR)'
			,msg: '\r\n\x01c\x01h' + gettext("Search for Filename(s)") + '\r\n' },
	'/S': { eval: 'bbs.scan_dirs(FL_NO_HDR, /* all */true) ' },
	 'T': { eval: 'bbs.temp_xfer()' },
	 'U': { eval: 'shell.upload_file()'
			,msg: '\r\n\x01c\x01h' + gettext("Upload File") + '\r\n' },
	'/U': { eval: 'shell.upload_user_file()'
			,msg: '\r\n\x01c\x01h' + gettext("Upload File to User") + '\r\n' },
	 'V': { eval: 'shell.view_files()'
			,msg: '\r\n\x01c\x01h' + gettext("View File(s)") + '\r\n' },
	 'W': { eval: 'bbs.whos_online()' },
	 'Z': { eval: 'shell.upload_sysop_file()'
			,msg: '\r\n\x01c\x01h' + gettext("Upload File to Sysop") + '\r\n' },
	 '*': { eval: 'shell.show_dirs(bbs.curlib)' },
	'/*': { eval: 'shell.show_libs()' },
	 '&': { exec: 'filescancfg.js' },
	 '!': { eval: 'bbs.menu("sysxfer")'
			,ars: 'SYSOP' },
	 '#': {  msg: '\r\n\x01c\x01h' + gettext("Type the actual number, not the symbol.") + '\r\n' },
	'/#': {  msg: '\r\n\x01c\x01h' + gettext("Type the actual number, not the symbol.") + '\r\n' },
	},
	nav: {
	'\r': { },
	 'Q': { eval: 'menu = main_menu' },
	 '>': { eval: 'shell.dir_up()' },
	 '}': { eval: 'shell.dir_up()' },
	 ')': { eval: 'shell.dir_up()' },
	 '+': { eval: 'shell.dir_up()' },
	 '=': { eval: 'shell.dir_up()' },
	 '<': { eval: 'shell.dir_down()' },
	 '{': { eval: 'shell.dir_down()' },
	 '(': { eval: 'shell.dir_down()' },
	 '-': { eval: 'shell.dir_down()' },
	 ']': { eval: 'shell.lib_up()' },
	 '[': { eval: 'shell.lib_down()' },
	},
};

// Can't do these statically through initialization:
file_menu.nav[KEY_UP] = { eval: 'shell.dir_up()' };
file_menu.nav[KEY_DOWN] = { eval: 'shell.dir_down()' };
file_menu.nav[KEY_RIGHT] = { eval: 'shell.lib_up()' };
file_menu.nav[KEY_LEFT] = { eval: 'shell.lib_down()' };

var menu = main_menu;
var last_str_cmd = "";

// The menu-display/command-prompt loop
while(bbs.online && !js.terminated) {
	if(!(user.settings & USER_EXPERT)) {
		console.clear();
		bbs.menu(menu.file);
	}
	bbs.node_action = menu.node_action;
	bbs.nodesync();
	eval(menu.eval);
	console.newline();
	console.aborted = false;
	console.putmsg(menu.prompt, P_SAVEATR);
	var cmd = console.getkey(K_UPPER);
	if(cmd > ' ')
		console.print(cmd);
	if(cmd == ';') {
		cmd = console.getstr(100, K_LINEWRAP);
		if(cmd == '!')
			cmd = last_str_cmd;
		load({}, "str_cmds.js", cmd);
		last_str_cmd = cmd;
		continue;
	}
	if(cmd == '/') {
		cmd = console.getkey(K_UPPER);
		console.print(cmd);
		if(cmd >= '1' && cmd <= '9') {
			menu.slash_num_input(cmd);
			continue;
		}
		cmd = '/' + cmd;
	}
	if(cmd >= '1' && cmd <= '9') {
		menu.num_input(cmd);
		continue;
	}
	if(cmd > ' ') {
		bbs.log_key(cmd, /* comma: */true);
	}
	if(menu.nav[cmd]) {
		if(menu.nav[cmd].eval)
			eval(menu.nav[cmd].eval);
		continue;
	}
	console.newline();
	console.line_counter = 0;
	if(cmd == help_key) {
		if(user.settings & USER_EXPERT)
			bbs.menu(menu.file);
		continue;
	}
	var menu_cmd = menu.command[cmd];
	if(!menu_cmd) {
		console.print("\r\n\x01c\x01h" + gettext("Unrecognized command."));
		if(user.settings & USER_EXPERT)
			console.print("  " + gettext("Hit") + " '\x01i" + help_key + "\x01n\x01c\x01h' " + gettext("for a menu."));
		console.print("  " + gettext("Type \x01y;help\x01c for more commands."));
		console.newline();
		continue;
	}
	if(menu_cmd.ars === undefined || bbs.compare_ars(menu_cmd.ars)) {
		if(menu_cmd.msg)
			console.print(menu_cmd.msg);
		if(menu_cmd.eval)
			eval(menu_cmd.eval);
		if(menu_cmd.exec) {
			var script = system.mods_dir + menu_cmd.exec;
			if(!file_exists(script))
				script = system.exec_dir + menu_cmd.exec;
			if(menu_cmd.args)
				js.exec.apply(null, [script, {}].concat(menu_cmd.args));
			else
				js.exec(script, {});
		}
	}
	else if(menu_cmd.err)
		console.print(menu_cmd.err);
}
