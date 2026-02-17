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

shell.help_key = '?';
// If user has unlimited time, display time-used rather than time-remaining
const time_code = user.security.exemptions & UFLAG_T ? "@TUSED@" : "@TLEFT@";

shell.main_menu = {
	file: "main",
	cls: true,
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
	 'E': { eval: 'bbs.email_sec()' },
	 'F': { eval: 'bbs.scan_subs(SCAN_FIND)'
			,msg: '\r\n\x01c\x01h' + gettext("Find Text in Messages") + '\r\n' },
	'/F': { eval: 'bbs.scan_subs(SCAN_FIND, /* all */true)' },
	 'G': { eval: 'bbs.text_sec()' },
	 'I': { eval: 'main_info()' },
	 'J': { eval: 'select_msg_area()' },
	 'L': { eval: 'bbs.list_msgs()' },
	'/L': { eval: 'bbs.list_nodes()' },
	 'M': { eval: 'bbs.time_bank()' },
	'/M': { exec: 'postmeme.js' },
	 'N': { eval: 'bbs.scan_subs(SCAN_NEW)'
			,msg: '\r\n\x01c\x01h' + gettext("New Message Scan") + '\r\n' },
	'/N': { eval: 'bbs.scan_subs(SCAN_NEW, /* all */true)' },
	 'O': { eval: 'logoff(/* fast: */false)' },
	'/O': { eval: 'logoff(/* fast: */true)' },
	 'P': { eval: 'bbs.post_msg()' },
	'/P': { exec: 'postpoll.js' },
	 'Q': { eval: 'bbs.qwk_sec()' },
	 'R': { eval: 'bbs.scan_msgs()' },
	 'S': { eval: 'bbs.scan_subs(SCAN_TOYOU)'
			,msg: '\r\n\x01c\x01h' + gettext("Scan for Messages Posted to You") + '\r\n' },
	'/S': { eval: 'bbs.scan_subs(SCAN_TOYOU, /* all */true)' },
	 'U': { eval: 'list_users()' },
	'/U': { eval: 'bbs.list_users(UL_ALL)' },
	 'V': { exec: 'scanpolls.js' },
	'/V': { exec: 'scanpolls.js', args: ['all'] },
	 'W': { eval: 'bbs.whos_online()' },
	 'X': { eval: 'bbs.xtrn_sec()' },
	 'Z': { eval: 'bbs.scan_subs(SCAN_NEW | SCAN_CONT)'
			,msg: '\r\n\x01c\x01h' + gettext("Continuous New Message Scan") + '\r\n' },
	'/Z': { eval: 'bbs.scan_subs(SCAN_NEW | SCAN_CONT, /* all */true)' },
	 '*': { eval: 'show_subs(bbs.curgrp)' },
	'/*': { eval: 'show_grps()' },
	 '&': { exec: 'msgscancfg.js' },
	 '!': { eval: 'bbs.menu("sysmain")'
			,ars: 'SYSOP or EXEMPT Q or I or N' },
	 '#': {  msg: '\r\n\x01c\x01h' + gettext("Type the actual number, not the symbol.") + '\r\n' },
	'/#': {  msg: '\r\n\x01c\x01h' + gettext("Type the actual number, not the symbol.") + '\r\n' },
	},
	nav: {
	'\r': { },
	 'T': { eval: 'enter_file_section(); menu = file_menu' },
	 '>': { eval: 'sub_up()' },
	 '}': { eval: 'sub_up()' },
	 ')': { eval: 'sub_up()' },
	 '+': { eval: 'sub_up()' },
	 '=': { eval: 'sub_up()' },
	 '<': { eval: 'sub_down()' },
	 '{': { eval: 'sub_down()' },
	 '(': { eval: 'sub_down()' },
	 '-': { eval: 'sub_down()' },
	 ']': { eval: 'grp_up()' },
	 '[': { eval: 'grp_down()' },
	},
};

// Can't do these statically through initialization:
shell.main_menu.nav[KEY_UP] = { eval: 'sub_up()' };
shell.main_menu.nav[KEY_DOWN] = { eval: 'sub_down()' };
shell.main_menu.nav[KEY_RIGHT] = { eval: 'grp_up()' };
shell.main_menu.nav[KEY_LEFT] = { eval: 'grp_down()' };

shell.file_menu = {
	file: "transfer",
	cls: true,
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
	 'D': { eval: 'download_files()'
			,msg: '\r\n\x01c\x01h' + gettext("Download File(s)") + '\r\n'
			,ars: 'REST NOT D' },
	'/D': { eval: 'download_user_files()'
			,msg: '\r\n\x01c\x01h' + gettext("Download File(s) from User(s)") + '\r\n'
			,ars: 'REST NOT D' },
	 'E': { eval: 'view_file_info(FI_INFO)'
			,msg: '\r\n\x01c\x01h' + gettext("List Extended File Information") + '\r\n' },
	 'F': { eval: 'bbs.scan_dirs(FL_FINDDESC);'
			,msg: '\r\n\x01c\x01h' + gettext("Find Text in File Descriptions (no wildcards)") + '\r\n' },
	'/F': { eval: 'bbs.scan_dirs(FL_FINDDESC, /* all: */true);' },
	 'I': { eval: 'file_info()' },
	 'J': { eval: 'select_file_area()' },
	 'L': { eval: 'list_files()' },
	'/L': { eval: 'bbs.list_nodes()' },
	 'N': { eval: 'bbs.scan_dirs(FL_ULTIME)'
			,msg: '\r\n\x01c\x01h' + gettext("New File Scan") + '\r\n' },
	'/N': { eval: 'bbs.scan_dirs(FL_ULTIME, /* all */true)' },
	 'O': { eval: 'logoff(/* fast: */false)' },
	'/O': { eval: 'logoff(/* fast: */true)' },
	 'R': { eval: 'view_file_info(FI_REMOVE)'
			,msg: '\r\n\x01c\x01h' + gettext("Remove/Edit File(s)") + '\r\n' },
	 'S': { eval: 'bbs.scan_dirs(FL_NO_HDR)'
			,msg: '\r\n\x01c\x01h' + gettext("Search for Filename(s)") + '\r\n' },
	'/S': { eval: 'bbs.scan_dirs(FL_NO_HDR, /* all */true) ' },
	 'T': { eval: 'bbs.temp_xfer()' },
	 'U': { eval: 'upload_file()'
			,msg: '\r\n\x01c\x01h' + gettext("Upload File") + '\r\n' },
	'/U': { eval: 'upload_user_file()'
			,msg: '\r\n\x01c\x01h' + gettext("Upload File to User") + '\r\n' },
	 'V': { eval: 'view_files()'
			,msg: '\r\n\x01c\x01h' + gettext("View File(s)") + '\r\n' },
	 'W': { eval: 'bbs.whos_online()' },
	 'Z': { eval: 'upload_sysop_file()'
			,msg: '\r\n\x01c\x01h' + gettext("Upload File to Sysop") + '\r\n' },
	 '*': { eval: 'show_dirs(bbs.curlib)' },
	'/*': { eval: 'show_libs()' },
	 '&': { exec: 'filescancfg.js' },
	 '!': { eval: 'bbs.menu("sysxfer")'
			,ars: 'SYSOP' },
	 '#': {  msg: '\r\n\x01c\x01h' + gettext("Type the actual number, not the symbol.") + '\r\n' },
	'/#': {  msg: '\r\n\x01c\x01h' + gettext("Type the actual number, not the symbol.") + '\r\n' },
	},
	nav: {
	'\r': { },
	 'Q': { eval: 'menu = main_menu' },
	 '>': { eval: 'dir_up()' },
	 '}': { eval: 'dir_up()' },
	 ')': { eval: 'dir_up()' },
	 '+': { eval: 'dir_up()' },
	 '=': { eval: 'dir_up()' },
	 '<': { eval: 'dir_down()' },
	 '{': { eval: 'dir_down()' },
	 '(': { eval: 'dir_down()' },
	 '-': { eval: 'dir_down()' },
	 ']': { eval: 'lib_up()' },
	 '[': { eval: 'lib_down()' },
	},
};

// Can't do these statically through initialization:
shell.file_menu.nav[KEY_UP] = { eval: 'dir_up()' };
shell.file_menu.nav[KEY_DOWN] = { eval: 'dir_down()' };
shell.file_menu.nav[KEY_RIGHT] = { eval: 'lib_up()' };
shell.file_menu.nav[KEY_LEFT] = { eval: 'lib_down()' };

shell.menu = shell.main_menu;
shell.menu_loop();