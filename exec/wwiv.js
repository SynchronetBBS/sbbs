// WWIV emulation script
// replaces wwiv.src/bin

// @format.tab-size 4

"use strict";

require("sbbsdefs.js", "SYS_RA_EMU");
require("userdefs.js", "UFLAG_T");
require("nodedefs.js", "NODE_MAIN");
require("gettext.js", "gettext");
load("termsetup.js");

var shell = load({}, "shell_lib.js");

system.settings |= SYS_RA_EMU; // Use (R)e-read and (A)uto-reply keys

shell.help_key = '?';
// If user has unlimited time, display time-used rather than time-remaining
const time_code = user.security.exemptions & UFLAG_T ? "@TUSED@" : "@TLEFT@";

shell.main_menu = {
	file: "wwiv/main",
	cls: true,
	eval: 'bbs.main_cmds++',
	node_action: NODE_MAIN,
	prompt: gettext("\r\n\x01nT - ")
		+ time_code
		+ gettext("\r\n\x01y\x01h(@GN@:@SN@) (@GRP@: @SUBL@) : \x01n"),
	num_input: shell.get_sub_num,
	slash_num_input: shell.get_grp_num,
	command: {
	 'A': { eval: 'bbs.auto_msg()' },
	 'C': { eval: 'bbs.chat_sec()' },
	 'D': { eval: 'bbs.user_config(); exit()' },
	 'E': { eval: 'bbs.email_sec()' },
	 'F': { eval: 'bbs.scan_subs(SCAN_FIND)'
			,msg: '\r\n\x01c\x01h' + gettext("Find Text in Messages") + '\r\n' },
	 'G': { eval: 'bbs.text_sec()' },
	 'I': { eval: 'main_info();' },
	 'J': { eval: 'select_msg_area()' },
	 'L': { eval: 'bbs.exec_xtrn("sbbslist")' },
	 'M': { eval: 'bbs.qwk_sec()' },
	 'N': { eval: 'bbs.scan_subs(SCAN_NEW)'
			,msg: '\r\n\x01c\x01h' + gettext("New Message Scan") + '\r\n' },
	 'O': { eval: 'logoff(/* fast: */false)' },
	'/O': { eval: 'logoff(/* fast: */true)' },
	 'P': { eval: 'bbs.post_msg()' },
	 'Q': { eval: 'bbs.scan_msgs(SCAN_NEW)' },
	 'Y': { eval: 'bbs.scan_subs(SCAN_TOYOU)'
			,msg: '\r\n\x01c\x01h' + gettext("Scan for Messages Posted to You") + '\r\n' },
	 'S': { eval: 'bbs.scan_msgs()'},
	'/S': { eval: 'bbs.scan_subs(SCAN_TOYOU, /* all */true)' }, //
	 'U': { eval: 'list_users()' },
	 'Z': { eval: 'bbs.scan_subs(SCAN_NEW | SCAN_CONT)'
			,msg: '\r\n\x01c\x01h' + gettext("Continuous New Message Scan") + '\r\n' },
	'/Z': { eval: 'bbs.scan_subs(SCAN_NEW | SCAN_CONT, /* all */true)' },
	 '$': { eval: 'bbs.time_bank()' },
	 '.': { eval: 'bbs.xtrn_sec()' },
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
	 '}': { eval: 'sub_up()' },
	 '{': { eval: 'sub_down()' },
	 ']': { eval: 'grp_up()' },
	 '[': { eval: 'grp_down()' },
	},
};

if (typeof bbs.email_sec != 'function')
	shell.main_menu.command['E'] = { exec: 'email_sec.js' };

shell.file_menu = {
	cls: true,
	file: "wwiv/transfer",
	eval: 'bbs.file_cmds++',
	node_action: NODE_XFER,
	prompt: gettext("\r\n\x01nT - ")
		+ time_code
		+ gettext("\x01y\x01h[@LN@:@DN@] [@LIB@: @DIRL@] : \x01n"),
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
	 'G': { eval: 'bbs.temp_xfer()' },
	 'J': { eval: 'select_file_area()' },
	 'L': { eval: 'list_files()' },
	'/L': { eval: 'bbs.list_nodes()' },
	 'N': { eval: 'bbs.scan_dirs(FL_ULTIME)'
			,msg: '\r\n\x01c\x01h' + gettext("New File Scan") + '\r\n' },
	'/N': { eval: 'bbs.scan_dirs(FL_ULTIME, /* all */true)' },
	 'O': { eval: 'logoff(/* fast: */false)' },
	'/O': { eval: 'logoff(/* fast: */true)' },
	 'P': { exec: 'filescancfg.js' },
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
	 'Y': { eval: 'bbs.user_info()' },
	 'Z': { eval: 'upload_sysop_file()'
			,msg: '\r\n\x01c\x01h' + gettext("Upload File to Sysop") + '\r\n' },
	 '*': { eval: 'show_subs(bbs.curgrp)' },
	'/*': { eval: 'show_grps()' },
	 '!': { eval: 'bbs.menu("sysxfer")'
			,ars: 'SYSOP' },
	 '$': { eval: 'bbs.time_bank()' },
	 '#': {  msg: '\r\n\x01c\x01h' + gettext("Type the actual number, not the symbol.") + '\r\n' },
	'/#': {  msg: '\r\n\x01c\x01h' + gettext("Type the actual number, not the symbol.") + '\r\n' },
	},
	nav: {
	'\r': { },
	 'Q': { eval: 'menu = main_menu' },
	 '>': { eval: 'dir_up()' },
	 '+': { eval: 'dir_up()' },
	 '<': { eval: 'dir_down()' },
	 '(': { eval: 'dir_down()' },
	 '-': { eval: 'dir_down()' },
	 ']': { eval: 'lib_up()' },
	 '[': { eval: 'lib_down()' },
	},
};

shell.menu = shell.main_menu;
shell.menu_loop();
