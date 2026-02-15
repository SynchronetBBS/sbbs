// Renegade Clone command shell

// @format.tab-size 4, @format.use-tabs true

"use strict";

require("sbbsdefs.js", "SYS_RA_EMU");
require("userdefs.js", "UFLAG_T");
require("nodedefs.js", "NODE_MAIN");
require("key_defs.js", "KEY_UP");
require("gettext.js", "gettext");
load("termsetup.js");
var shell = load({}, "shell_lib.js");

system.settings |= SYS_RA_EMU;  // Swap Msg ReRead/Reply commands

shell.help_key = '?';
// If user has unlimited time, display time-used rather than time-remaining
const time_code = user.security.exemptions & UFLAG_T ? "@TUSED@" : "@TLEFT@";
const time_prefix = user.security.exemptions & UFLAG_T ? gettext("Used") : gettext("Left");

shell.main_menu = {
	cls: true,
	file: "renegade/main",
	eval: 'bbs.main_cmds++',
	node_action: NODE_MAIN,
	prompt: "\x01n\x01b\x01h" + gettext("Time") + " " + time_prefix + ": [\x01c" + time_code + "\x01b] "
		+ "\x01b\x01h(\x01c" + shell.help_key + "\x01b=\x01c"
		+ gettext("Help") + "\x01b)\r\n"
		+ "\x01c" + gettext("Main") + " \x01b" + gettext("Menu") + " \x01m: \x01n",
	command: {
	 'A': { eval: 'bbs.auto_msg()' },
	'/A': { exec: 'avatar_chooser.js'
			,ars: 'ANSI and not GUEST'
			,err: '\r\n' + gettext("Sorry, only regular users with ANSI terminals can do that.") + '\r\n' },
	 'B': { eval: 'bbs.exec_xtrn("sbbslist")' },
	 'C': { eval: 'bbs.chat_sec()' },
	 'P': { eval: 'bbs.user_config(); exit()' },
	 'S': { eval: 'bbs.text_sec()' },
	 'I': { eval: 'main_info()' },
	 'Y': { eval: 'bbs.user_info()' },
     'L': { eval: 'bbs.list_logons()' },
    '/L': { eval: 'bbs.list_nodes()' },
	 '#': { eval: 'bbs.list_nodes()' },
	 'N': { eval: 'send_feedback()' },
	 'O': { eval: 'bbs.xtrn_sec()' },
	 'G': { eval: 'logoff(/* fast: */false)' },
	'/G': { eval: 'logoff(/* fast: */true)' },
	 'U': { eval: 'list_users()' },
	 'V': { exec: 'scanpolls.js', args: ['all'] },
	 'W': { eval: 'bbs.whos_online()' },
	 'X': { eval: 'user.settings ^= USER_EXPERT' },
	 '$': { eval: 'bbs.time_bank()' },
	 '!': { eval: 'bbs.qwk_sec()' },
	 '*': { eval: 'bbs.menu("sysmain")'
			,ars: 'SYSOP or EXEMPT Q or I or N' },
	},
	nav: {
	'\r': { },
	 'E': { eval: 'menu = email_menu' },
 	 'M': { eval: 'menu = message_menu' },
	 'F': { eval: 'enter_file_section(); menu = file_menu' },
	},
};

shell.message_menu = {
	cls: true,
	file: "renegade/message",
	eval: 'bbs.main_cmds++',
	node_action: NODE_RMSG,
	prompt: "\x01n\x01b\x01h[\x01n\x01c@GN@\x01b\x01h][\x01n\x01c@SN@\x01b\x01h] \x01n\x01c- \x01h@GRP@ @SUB@\r\n"
		+ "\x01n\x01b\x01h" + gettext("Time") + " " + time_prefix + ": [\x01c" + time_code + "\x01b] "
		+ "\x01b\x01h(\x01c" + shell.help_key + "\x01b=\x01c"
		+ gettext("Help") + "\x01b)\r\n"
		+ "\x01c" + gettext("Message") + " \x01b" + gettext("Menu") + " \x01m: \x01n",
	num_input: shell.get_sub_num,
	slash_num_input: shell.get_grp_num,
	command: {
	 'A': { eval: 'select_msg_area()' },
     'J': { eval: 'select_msg_area()' },
     'C': { eval: 'bbs.chat_sec()' },
	'/L': { eval: 'bbs.list_nodes()' },
	'/M': { exec: 'postmeme.js' },
	 'N': { eval: 'bbs.scan_subs(SCAN_NEW)'
			,msg: '\r\n\x01c\x01h' + gettext("New Message Scan") + '\r\n' },
	'/N': { eval: 'bbs.scan_subs(SCAN_NEW, /* all */true)' },
	 'G': { eval: 'logoff(/* fast: */false)' },
	'/G': { eval: 'logoff(/* fast: */true)' },
	 'P': { eval: 'bbs.post_msg()' },
	'/P': { exec: 'postpoll.js' },
	 '!': { eval: 'bbs.qwk_sec()' },
	 'R': { eval: 'bbs.scan_msgs()' },
	 'S': { eval: 'bbs.scan_subs(SCAN_FIND)'
			,msg: '\r\n\x01c\x01h' + gettext("Find Text in Messages") + '\r\n' },
	'/S': { eval: 'bbs.scan_subs(SCAN_FIND, /* all */true)' },
	 'Y': { eval: 'bbs.scan_subs(SCAN_TOYOU)'
			,msg: '\r\n\x01c\x01h' + gettext("Scan for Messages Posted to You") + '\r\n' },
	'/Y': { eval: 'bbs.scan_subs(SCAN_TOYOU, /* all */true)' },
     'U': { eval: 'bbs.list_users(UL_SUB)' },
	'/U': { eval: 'list_users()' },
	 'V': { exec: 'scanpolls.js' },
	'/V': { exec: 'scanpolls.js', args: ['all'] },
	 'W': { eval: 'bbs.whos_online()' },
	 'X': { eval: 'user.settings ^= USER_EXPERT' },
	 '$': { eval: 'bbs.time_bank()' },
	 '&': { exec: 'msgscancfg.js' },
	 'Z': { exec: 'msgscancfg.js' },
	 '*': { eval: 'bbs.menu("sysmain")'
			,ars: 'SYSOP or EXEMPT Q or I or N' },
	 '#': {  msg: '\r\n\x01c\x01h' + gettext("Type the actual number, not the symbol.") + '\r\n' },
	'/#': {  msg: '\r\n\x01c\x01h' + gettext("Type the actual number, not the symbol.") + '\r\n' },
	},
	nav: {
	'\r': { },
	 'E': { eval: 'menu = email_menu' },
	 'F': { eval: 'enter_file_section(); menu = file_menu' },
	 'Q': { eval: 'menu = main_menu' },
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
shell.message_menu.nav[KEY_UP] = { eval: 'sub_up()' };
shell.message_menu.nav[KEY_DOWN] = { eval: 'sub_down()' };
shell.message_menu.nav[KEY_RIGHT] = { eval: 'grp_up()' };
shell.message_menu.nav[KEY_LEFT] = { eval: 'grp_down()' };

shell.file_menu = {
	cls: true,
	file: "renegade/transfer",
	eval: 'bbs.file_cmds++',
	node_action: NODE_XFER,
	prompt: "\x01n\x01b\x01h[\x01n\x01c@LN@\x01b\x01h][\x01n\x01c@DN@\x01b\x01h] \x01n\x01c- \x01h@LIB@ @DIR@\r\n"
		+ "\x01b\x01h" + gettext("Time") + " " + time_prefix + ": [\x01c" + time_code + "\x01b] "
		+ "\x01b\x01h(\x01c" + shell.help_key + "\x01b=\x01c"
		+ gettext("Help") + "\x01b)\r\n"
		+ "\x01c" + gettext("File") + " \x01b" + gettext("Menu") + " \x01m: \x01n",
	num_input: shell.get_dir_num,
	slash_num_input: shell.get_lib_num,
	command: {
	 'A': { eval: 'select_file_area()' },
	 'J': { eval: 'select_file_area()' },
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
	 'G': { eval: 'logoff(/* fast: */false)' },
	'/G': { eval: 'logoff(/* fast: */true)' },
	 'L': { eval: 'list_files()' },
	'/L': { eval: 'bbs.list_nodes()' },
	 'N': { eval: 'bbs.scan_dirs(FL_ULTIME)'
			,msg: '\r\n\x01c\x01h' + gettext("New File Scan") + '\r\n' },
	'/N': { eval: 'bbs.scan_dirs(FL_ULTIME, /* all */true)' },
	 'O': { eval: 'logoff(/* fast: */false)' },
	'/O': { eval: 'logoff(/* fast: */true)' },
	 '&': { exec: 'filescancfg.js' },
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
	 'W': { eval: 'bbs.whos_online()' },
	 'X': { eval: 'user.settings ^= USER_EXPERT' },
	 'Y': { eval: 'bbs.user_info()' },
	 'Z': { eval: 'upload_sysop_file()'
			,msg: '\r\n\x01c\x01h' + gettext("Upload File to Sysop") + '\r\n' },
	 '*': { eval: 'bbs.menu("sysxfer")'
			,ars: 'SYSOP' },
	 '$': { eval: 'bbs.time_bank()' },
	 '#': {  msg: '\r\n\x01c\x01h' + gettext("Type the actual number, not the symbol.") + '\r\n' },
	'/#': {  msg: '\r\n\x01c\x01h' + gettext("Type the actual number, not the symbol.") + '\r\n' },
	},
	nav: {
	'\r': { },
	 'M': { eval: 'menu = message_menu' },
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

shell.email_menu = {
	cls: true,
	file: "renegade/email",
	prompt: "\x01n\x01b\x01h" + gettext("Time") + " " + time_prefix + ": [\x01c" + time_code + "\x01b] "
		+ "\x01b\x01h(\x01c" + shell.help_key + "\x01b=\x01c"
		+ gettext("Help") + "\x01b)\r\n"
		+ "\x01c" + gettext("Email") + " \x01b" + gettext("Menu") + " \x01m: \x01n",
	command: {
		'R': { eval: 'bbs.read_mail(MAIL_YOUR, user.number)' },
		'E': { eval: 'send_email()' },
		'F': { eval: 'send_feedback()' },
		'V': { eval: 'bbs.read_mail(MAIL_SENT, user.number)' },
		'N': { eval: 'send_netmail()' },
		'G': { eval: 'logoff(/* fast: */false)' },
	   '/G': { eval: 'logoff(/* fast: */true)' },
	},
	nav: {
	'\r': { },
	 'Q': { eval: 'menu = main_menu' },
	},
};

shell.menu = shell.main_menu;
shell.menu_loop();
