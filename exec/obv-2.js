// Oblivion/2 BBS Clone for Synchronet BBS

"use strict";

require("sbbsdefs.js", "K_UPPER");
require("userdefs.js", "USER_EXPERT");
require("nodedefs.js", "NODE_MAIN");
require("key_defs.js", "KEY_UP");
require("gettext.js", "gettext");
load("termsetup.js");

function show_sysop_wants()
{
	console.crlf();
	var f = system.text_dir + "sysop_wants.txt";
	if (file_exists(f))
		console.printfile(f);
	else
		console.print("\r\n\x01n\x01hThe SysOp has not posted any requests.\r\n");
}

var shell = load({}, "shell_lib.js");

shell.show_sysop_wants = show_sysop_wants;

system.settings &= ~SYS_RA_EMU;

shell.help_key = '?';
// If user has unlimited time, display time-used rather than time-remaining
const time_code = user.security.exemptions & UFLAG_T ? "@TUSED@" : "@TLEFT@";

shell.main_menu = {
	file: "obv-2/main",
	cls: true,
	eval: 'bbs.main_cmds++',
	node_action: NODE_MAIN,
	prompt: "\x01n\x01gMain \x01c\xFE\x01h@"
		+ time_code
		+ "\x01n\x01c\xFE \x01h\x01kC\x01n\x01go\x01hm\x01nm\x01hand: ",
	command: {
	 'A': { eval: 'bbs.auto_msg()' },
	 'B': { eval: 'bbs.exec_xtrn("sbbslist")' },
	 'C': { eval: 'bbs.page_sysop()' },
	 'D': { eval: 'bbs.list_logons()' },
	 'F': { eval: 'send_feedback()' },
	 'G': { eval: 'logoff(false)' },
	'/G': { eval: 'logoff(true)' },
	 'H': { eval: 'bbs.whos_online()' },
	 'I': { eval: 'bbs.chat_sec()' },
	 'K': { eval: 'bbs.user_config(); exit()' },
	 'Y': { eval: 'bbs.user_info()' },
	 'L': { eval: 'list_users()' },
	 'P': { eval: 'bbs.xtrn_sec()' },
	 'S': { eval: 'main_info()' },
	 'U': { eval: 'bbs.time_bank()' },
	 'W': { exec: 'logonlist.js', args: ['-l'] },
	 'X': { eval: 'user.settings ^= USER_EXPERT' },
	 'Z': { eval: 'bbs.reinit_msg_ptrs()' },
	 '+': { eval: 'bbs.user_config(); exit()' },
	 '!': { eval: 'bbs.menu("sysmain")'
			,ars: 'SYSOP or EXEMPT Q or I or N' },
	},
	nav: {
	'\r': {},
	 'E': { eval: 'menu = email_menu' },
	 'M': { eval: 'menu = message_menu' },
	 'Q': { eval: 'menu = quick_menu' },
	 'T': { eval: 'enter_file_section(); menu = file_menu' },
	}
};

shell.file_menu = {
	file: "obv-2/files",
	cls: true,
	eval: 'bbs.main_cmds++',
	node_action: NODE_XFER,
	prompt: "\x01n\x01gFile \x01c\xFE \x01h"
		+ time_code
		+ " \x01n\x01c\xFE \x01h\x01c(@LN@) @LIB@ (@DN@) @DIR@: ",
	num_input: shell.get_dir_num,
	slash_num_input: shell.get_lib_num,
	command: {
	 'A': { eval: 'console.clear(); select_file_area();' },
	 'B': { eval: 'bbs.batch_menu()' },
	 'C': { exec: 'filescancfg.js' },
	'/D': { eval: 'download_files()'
			,msg: '\r\n\x01c\x01hDownload File(s)\r\n'
			,ars: 'REST NOT D' },
	 'E': { eval: 'view_file_info(FI_REMOVE)'
	 		,msg: '\r\n\x01c\x01hRemove/Edit File(s)\r\n' },
	 'G': { eval: 'logoff(false)' },
	'/G': { eval: 'logoff(true)' },
	 'K': { exec: 'filescancfg.js' },
	 'L': { eval: 'list_files()' },
	 'N': { eval: 'bbs.scan_dirs(FL_ULTIME)'
			,msg: '\r\n\x01c\x01hNew File Scan\r\n' },
	 'R': { eval: 'bbs.batch_menu()' },
	 'S': { eval: 'bbs.scan_dirs(FL_NO_HDR)'
			,msg: '\r\n\x01c\x01hSearch for Filename(s)\r\n' },
	 'T': { eval: 'bbs.temp_xfer()' },
	 'U': { eval: 'upload_file()'
			,msg: '\r\n\x01c\x01hUpload File\r\n' },
	'/U': { eval: 'upload_user_file()'
			,msg: '\r\n\x01c\x01hUpload File to User\r\n' },
	 'V': { eval: 'view_files()'
			,msg: '\r\n\x01c\x01hView File(s)\r\n' },
	 'W': { eval: 'view_file_info(FI_INFO)'
			,msg: '\r\n\x01c\x01hExtended File Information\r\n' },
	 'X': { eval: 'show_sysop_wants()' },
	},
	nav: {
	'\r': {},
	 'M': { eval: 'menu = message_menu' },
	 'Q': { eval: 'menu = main_menu' },
	 '+': { eval: 'dir_up()' },
	 '-': { eval: 'dir_down()' },
	 '*': { eval: 'show_dirs(bbs.curlib)' },
	'/*': { eval: 'show_libs()' },
	'/-': { eval: 'lib_down()' },
	'/+': { eval: 'lib_up()' },
	},
};

// Arrow keys
shell.file_menu.nav[KEY_UP]    = { eval: 'dir_up()' };
shell.file_menu.nav[KEY_DOWN]  = { eval: 'dir_down()' };
shell.file_menu.nav[KEY_RIGHT] = { eval: 'lib_up()' };
shell.file_menu.nav[KEY_LEFT]  = { eval: 'lib_down()' };

shell.quick_menu = {
	cls: true,
	file: "obv-2/quick",
	eval: 'bbs.main_cmds++',
	node_action: NODE_MAIN,
	prompt: "\x01h\x01kQuick \x01n\x01g\xFE\x01h"
		+ time_code
		+ "\x01n\x01g\xFE \x01h\x01kC\x01n\x01go\x01hm\x01nm\x01hand: ",
	command: {
	 'C': { eval: 'bbs.page_sysop()' },
	 'F': { eval: 'send_feedback()' },
	 'G': { eval: 'logoff(false)' },
	'/G': { eval: 'logoff(true)' },
	},
	nav: {
	'\r': {},
	 'M': { eval: 'menu = message_menu' },
	 'E': { eval: 'menu = email_menu' },
	 'T': { eval: 'enter_file_section(); menu = file_menu' },
	 'Q': { eval: 'menu = main_menu' },
	}
};

shell.message_menu = {
	cls: true,
	file: "obv-2/message",
	eval: 'bbs.main_cmds++',
	node_action: NODE_RMSG,
	prompt: "\x01n\x01gMessage \x01c\xFE \x01h"
		+ time_code
		+ "\x01n\x01c\xFE \x01c\x01h[@GN@] @GRP@ [@SN@] @SUB@: ",
	num_input: shell.get_sub_num,
	slash_num_input: shell.get_grp_num,
	command: {
	 'A': { eval: 'console.clear(); select_msg_area()' },
	 'B': { eval: 'show_subs(bbs.curgrp)' },
	 'C': { eval: 'console.clear(); bbs.cfg_msg_scan(SCAN_CFG_NEW)' },
	 'G': { eval: 'logoff(false)' },
	'/G': { eval: 'logoff(true)' },
	 'J': { eval: 'show_grps()' },
	 'O': { eval: 'bbs.menu_dir = "obv-2"; bbs.qwk_sec(); bbs.menu_dir = "";' },
	 'P': { eval: 'console.clear(); bbs.post_msg()' },
	 'R': { eval: 'bbs.scan_msgs()' },
	 'Y': { eval: 'console.clear(); bbs.scan_subs(FL_TOYOU)' },
	 'Z': { eval: 'bbs.scan_subs(SCAN_NEW)' },
	 '-': { eval: 'sub_down()' },
	 '>': { eval: 'group_up()' },
	 '<': { eval: 'grp_down()' },
	 '*': { eval: 'show_subs(bbs.curgrp)' },
	'/*': { eval: 'show_grps()' },
	},
	nav: {
	'\r': {},
	 'Q': { eval: 'menu = main_menu' },
	 'T': { eval: 'enter_file_section(); menu = file_menu' },
	}
};

shell.email_menu = {
	cls: true,
	file: "obv-2/email",
	eval: 'bbs.main_cmds++',
	node_action: NODE_RMAL,
	prompt: "\x01h\x01kEmail \x01n\x01g\xFE\x01h\x01g"
		+ time_code
		+ "\x01n\x01g\xFE \x01h\x01kC\x01n\x01go\x01hm\x01nm\x01hand: \x01n",
	command: {
	 'F': { eval: 'send_email()' },
	 'R': { eval: 'bbs.read_mail(MAIL_YOUR, user.number)' },
	 'S': { eval: 'send_email()' },
	 'V': { eval: 'bbs.read_mail(MAIL_SENT, user.number)' },
	},
	nav: {
	'\r': {},
	 'Q': { eval: 'menu = main_menu' },
	}
};

shell.menu = shell.main_menu;
shell.menu_loop();
