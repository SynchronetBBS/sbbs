// Spitfire BBS Command Shell for Synchronet
// Inspired by the Spitfire BBS interface
// Developed for Unix-Bit BBS / x-bit.org
//
// Co-authored-by: Claude Sonnet 4.6 <claude@anthropic.com>
//
// Menu files go in: sbbs/text/menu/spitfire/
//   main.msg   - Main menu
//   msg.msg    - Message menu
//   file.msg   - File menu
//   chat.msg   - Chat menu
//   e-mail.msg - E-Mail/NetMail menu
//   qwk.msg    - QWK menu
//
// Register in SCFG -> Command Shells:
//   Name:          Spitfire
//   Internal Code: SPITFIRE

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

// Time display: used if unlimited time exemption, remaining otherwise
const time_code = user.security.exemptions & UFLAG_T ? "@TUSED@" : "@TLEFT@";

// Prompt builder -

function sf_prompt(section) {
	return "\x01n\x01c\xfe \x01b\x01h" + section
		+ " \x01n\x01c\xfe \x01h" + time_code
		+ " \x01n\x01c[\x01h@GN@\x01n\x01c]: \x01n";
}

// Section wrappers -
// Per Digital Man: set/reset bbs.menu_dir only around captured section calls
// that have their own custom menu files in the spitfire/ subfolder.
// Direct bbs.menu() calls use the full path (e.g. "spitfire/main") instead.

shell.sf_chat_sec = function() {
	console.clear();
	bbs.menu_dir = "spitfire";
	bbs.chat_sec();
	bbs.menu_dir = "";
};

shell.sf_email_sec = function() {
	console.clear();
	bbs.menu_dir = "spitfire";
	if (typeof bbs.email_sec == 'function')
		bbs.email_sec();
	else
		js.exec(system.exec_dir + "email_sec.js", {});
	bbs.menu_dir = "";
};

shell.sf_qwk_sec = function() {
	console.clear();
	bbs.menu_dir = "spitfire";
	bbs.qwk_sec();
	bbs.menu_dir = "";
};

// MESSAGE MENU -

shell.msg_menu = {
	file: "spitfire/msg",
	cls: true,
	eval: 'bbs.main_cmds++',
	node_action: NODE_MAIN,
	prompt: sf_prompt("Message"),
	num_input: shell.get_sub_num,
	slash_num_input: shell.get_grp_num,
	command: {
		'N':  { eval: 'bbs.scan_subs(SCAN_NEW)',
		        msg: '\r\n\x01c\x01hNew Message Scan\x01n\r\n' },
		'L':  { eval: 'bbs.list_msgs()' },
		'R':  { eval: 'bbs.scan_msgs()' },
		'J':  { eval: 'select_msg_area()' },
		'K':  { eval: 'sf_qwk_sec()' },
		'E':  { eval: 'sf_email_sec()' },
		'A':  { eval: 'bbs.auto_msg()' },
		'P':  { eval: 'bbs.post_msg()' },
		'/P': { exec: 'postpoll.js' },
		'V':  { exec: 'scanpolls.js' },
		'F':  { eval: 'bbs.scan_subs(SCAN_FIND)',
		        msg: '\r\n\x01c\x01hFind Text in Messages\x01n\r\n' },
		'S':  { eval: 'bbs.scan_subs(SCAN_TOYOU)',
		        msg: '\r\n\x01c\x01hScan for Messages Posted to You\x01n\r\n' },
		'*':  { eval: 'show_subs(bbs.curgrp)' },
		'/*': { eval: 'show_grps()' },
		'O':  { eval: 'logoff(false)' },
		'/O': { eval: 'logoff(true)' },
		'!':  { eval: 'bbs.menu("sysmain")',
		        ars: 'SYSOP or EXEMPT Q or I or N' },
	},
	nav: {
		'\r': { },
		'Q':  { eval: 'menu = main_menu' },
		'>':  { eval: 'sub_up()' },
		'}':  { eval: 'sub_up()' },
		')':  { eval: 'sub_up()' },
		'+':  { eval: 'sub_up()' },
		'=':  { eval: 'sub_up()' },
		'<':  { eval: 'sub_down()' },
		'{':  { eval: 'sub_down()' },
		'(':  { eval: 'sub_down()' },
		'-':  { eval: 'sub_down()' },
		']':  { eval: 'grp_up()' },
		'[':  { eval: 'grp_down()' },
	},
};

shell.msg_menu.nav[KEY_UP]    = { eval: 'sub_up()' };
shell.msg_menu.nav[KEY_DOWN]  = { eval: 'sub_down()' };
shell.msg_menu.nav[KEY_RIGHT] = { eval: 'grp_up()' };
shell.msg_menu.nav[KEY_LEFT]  = { eval: 'grp_down()' };

// MAIN MENU -

shell.main_menu = {
	file: "spitfire/main",
	cls: true,
	eval: 'bbs.main_cmds++',
	node_action: NODE_MAIN,
	prompt: sf_prompt("Main"),
	num_input: shell.get_sub_num,
	slash_num_input: shell.get_grp_num,
	command: {
		'M':  { eval: 'menu = msg_menu' },
		'F':  { eval: 'enter_file_section(); menu = file_menu' },
		'D':  { eval: 'bbs.xtrn_sec()' },
		'B':  { exec: 'bullseye.js' },
		'G':  { eval: 'bbs.text_sec()' },
		'C':  { eval: 'sf_chat_sec()' },
		'U':  { eval: 'list_users()' },
		'Y':  { eval: 'bbs.user_config(); exit()' },
		'I':  { eval: 'main_info()' },
		'/A': { exec: 'avatar_chooser.js',
		        ars: 'ANSI and not GUEST',
		        err: '\r\n\x01c\x01hSorry, only regular users with ANSI terminals can do that.\x01n\r\n' },
		'/L': { eval: 'bbs.list_nodes()' },
		'O':  { eval: 'logoff(false)' },
		'/O': { eval: 'logoff(true)' },
		'!':  { eval: 'bbs.menu("sysmain")',
		        ars: 'SYSOP or EXEMPT Q or I or N' },
	},
	nav: {
		'\r': { },
		'>':  { eval: 'sub_up()' },
		'}':  { eval: 'sub_up()' },
		')':  { eval: 'sub_up()' },
		'+':  { eval: 'sub_up()' },
		'=':  { eval: 'sub_up()' },
		'<':  { eval: 'sub_down()' },
		'{':  { eval: 'sub_down()' },
		'(':  { eval: 'sub_down()' },
		'-':  { eval: 'sub_down()' },
		']':  { eval: 'grp_up()' },
		'[':  { eval: 'grp_down()' },
	},
};

shell.main_menu.nav[KEY_UP]    = { eval: 'sub_up()' };
shell.main_menu.nav[KEY_DOWN]  = { eval: 'sub_down()' };
shell.main_menu.nav[KEY_RIGHT] = { eval: 'grp_up()' };
shell.main_menu.nav[KEY_LEFT]  = { eval: 'grp_down()' };

// FILE MENU -

shell.file_menu = {
	file: "spitfire/file",
	cls: true,
	eval: 'bbs.file_cmds++',
	node_action: NODE_XFER,
	prompt: sf_prompt("File"),
	num_input: shell.get_dir_num,
	slash_num_input: shell.get_lib_num,
	command: {
		'L':  { eval: 'list_files()' },
		'J':  { eval: 'select_file_area()' },
		'N':  { eval: 'bbs.scan_dirs(FL_ULTIME)',
		        msg: '\r\n\x01c\x01hNew File Scan\x01n\r\n' },
		'/N': { eval: 'bbs.scan_dirs(FL_ULTIME, true)' },
		'*':  { eval: 'show_dirs(bbs.curlib)' },
		'/*': { eval: 'show_libs()' },
		'E':  { eval: 'view_file_info(FI_INFO)',
		        msg: '\r\n\x01c\x01hList Extended File Information\x01n\r\n' },
		'F':  { eval: 'bbs.scan_dirs(FL_FINDDESC)',
		        msg: '\r\n\x01c\x01hFind Text in File Descriptions\x01n\r\n' },
		'/F': { eval: 'bbs.scan_dirs(FL_FINDDESC, true)' },
		'S':  { eval: 'bbs.scan_dirs(FL_NO_HDR)',
		        msg: '\r\n\x01c\x01hSearch for Filename(s)\x01n\r\n' },
		'/S': { eval: 'bbs.scan_dirs(FL_NO_HDR, true)' },
		'D':  { eval: 'download_files()',
		        msg: '\r\n\x01c\x01hDownload File(s)\x01n\r\n',
		        ars: 'REST NOT D' },
		'/D': { eval: 'download_user_files()',
		        msg: '\r\n\x01c\x01hDownload File(s) from User(s)\x01n\r\n',
		        ars: 'REST NOT D' },
		'U':  { eval: 'upload_file()',
		        msg: '\r\n\x01c\x01hUpload File\x01n\r\n' },
		'/U': { eval: 'upload_user_file()',
		        msg: '\r\n\x01c\x01hUpload File to User\x01n\r\n' },
		'B':  { eval: 'bbs.batch_menu()' },
		'Z':  { eval: 'upload_sysop_file()',
		        msg: '\r\n\x01c\x01hUpload File to Sysop\x01n\r\n' },
		'T':  { eval: 'bbs.temp_xfer()' },
		'R':  { eval: 'view_file_info(FI_REMOVE)',
		        msg: '\r\n\x01c\x01hRemove/Edit File(s)\x01n\r\n' },
		'&':  { exec: 'filescancfg.js' },
		'I':  { eval: 'file_info()' },
		'V':  { eval: 'view_files()',
		        msg: '\r\n\x01c\x01hView File(s)\x01n\r\n' },
		'C':  { eval: 'sf_chat_sec()' },
		'/L': { eval: 'bbs.list_nodes()' },
		'O':  { eval: 'logoff(false)' },
		'/O': { eval: 'logoff(true)' },
		'!':  { eval: 'bbs.menu("sysxfer")', ars: 'SYSOP' },
	},
	nav: {
		'\r': { },
		'Q':  { eval: 'menu = main_menu' },
		'>':  { eval: 'dir_up()' },
		'}':  { eval: 'dir_up()' },
		')':  { eval: 'dir_up()' },
		'+':  { eval: 'dir_up()' },
		'=':  { eval: 'dir_up()' },
		'<':  { eval: 'dir_down()' },
		'{':  { eval: 'dir_down()' },
		'(':  { eval: 'dir_down()' },
		'-':  { eval: 'dir_down()' },
		']':  { eval: 'lib_up()' },
		'[':  { eval: 'lib_down()' },
	},
};

shell.file_menu.nav[KEY_UP]    = { eval: 'dir_up()' };
shell.file_menu.nav[KEY_DOWN]  = { eval: 'dir_down()' };
shell.file_menu.nav[KEY_RIGHT] = { eval: 'lib_up()' };
shell.file_menu.nav[KEY_LEFT]  = { eval: 'lib_down()' };

// Boot the shell -

shell.menu = shell.main_menu;
shell.menu_loop();
