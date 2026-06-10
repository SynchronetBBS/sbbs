// Wildcat Clone script
// replaces wildcat.src/bin

// @format.tab-size 4

"use strict";

require("sbbsdefs.js", "K_UPPER");
require("userdefs.js", "UFLAG_T");
require("nodedefs.js", "NODE_MAIN");
require("gettext.js", "gettext");
load("termsetup.js");

var shell = load({}, "shell_lib.js");

system.settings &= SYS_RA_EMU; // Use (R)e-read and (A)uto-reply keys

shell.help_key = '?';

shell.main_menu = {
	file: "wildcat/main",
	cls: true,
	eval: 'bbs.main_cmds++',
	node_action: NODE_MAIN,
	prompt: (user.settings & USER_EXPERT)
		? "\r\n\x01y\x01h@GRP@ @SUB@, \x01wMAIN MENU\x01y: ? \x01n"
		: "\r\n\x01w\x01hCommand >>? \x01n",
	command: {
	 'A': { eval: 'bbs.auto_msg()' },
	 'B': { eval: 'bbs.text_sec();' },
	 'C': { eval: 'send_feedback()' },
	 'D': { eval: 'bbs.xtrn_sec()' },
	 'E': { eval: 'bbs.email_sec()' },
	 'G': { eval: 'bbs.logoff( !(user.settings & USER_EXPERT) );' },
	 'H': { eval: 'user.settings ^= USER_EXPERT; \
			console.putmsg("\\r\\nExpert mode is now: @EXPERT@"); \
			console.crlf(); menu.prompt = (user.settings & USER_EXPERT) \
			? "\\r\\n\\x01y\\x01h@GRP@ @SUB@, \\x01wMAIN MENU\\x01y: ? \\x01n" \
			: "\\r\\n\\x01w\\x01hCommand >>? \\x01n";' },
   	 'I': { eval: 'bbs.menu("../answer");' },
	 'J': { eval: 'console.clear(); select_msg_area()' },
	 'N': { eval: 'if(!bbs.menu("logon", P_NOERROR)) { \
			console.print("\\r\\nNo news is good news.\\r\\n"); \ }' },
	 'P': { eval: 'bbs.page_sysop()' },
	 'R': { eval: 'bbs.user_info()' }, //not asking for a key in expert mode ??
	 'S': { eval: 'bbs.sys_info()'},
	 'T': { eval: 'bbs.chat_sec()' },
	 'U': { eval: 'list_users()' },
	 'V': { eval: 'js.wildcat_verify_user()' },
	 'W': { eval: 'bbs.whos_online()' },
	 'Y': { eval: 'bbs.user_config(); exit()' },
	 '*': { eval: 'show_subs(bbs.curgrp)' },
	'/*': { eval: 'show_grps()' },
	 '&': { exec: 'msgscancfg.js' },
	 '1': { eval: 'bbs.menu("sysmain")'
			,ars: 'SYSOP or EXEMPT Q or I or N' },
	 '#': {  msg: '\r\n\x01c\x01h' + gettext("Type the actual number, not the symbol.") + '\r\n' },
	'/#': {  msg: '\r\n\x01c\x01h' + gettext("Type the actual number, not the symbol.") + '\r\n' },
	},
	nav: {
	'\r': { },
	 'F': { eval: 'enter_file_section(); menu = file_menu; \
			menu.prompt = (user.settings & USER_EXPERT) \
			? "\\r\\n\\x01y\\x01h@LIB@ @DIR@, \\x01wFILE MENU\\x01y: ? \\x01n" \
			: "\\r\\n\\x01w\\x01hCommand >>? \\x01n";' },
	 'M': { eval: 'menu = message_menu' },
	},
};

if (typeof bbs.email_sec != 'function')
	shell.main_menu.command['E'] = { exec: 'email_sec.js' };

shell.message_menu = {
	cls: true,
	file: "wildcat/msg",
	eval: 'bbs.main_cmds++',
	node_action: NODE_RMSG,
	prompt:  (user.settings & USER_EXPERT)
		? "\r\n\x01y\x01h@GRP@ @SUB@, \x01wMESSAGE MENU\x01y: ? \x01n"
		: "\r\n\x01w\x01hCommand >>? \x01n",
	command: {
	 'C': { eval: 'bbs.scan_subs(SCAN_TOYOU)'
 			,msg: '\r\n\x01c\x01h' + gettext("Scan for Messages Posted to You") + '\r\n' },
	 'E': { eval: 'bbs.post_msg()' },
	 'G': { eval: 'bbs.logoff( !(user.settings & USER_EXPERT) );' },
	 'H': { eval: 'user.settings ^= USER_EXPERT; \
			console.putmsg("\\r\\nExpert mode is now: @EXPERT@"); \
			console.crlf(); menu.prompt = (user.settings & USER_EXPERT) \
			? "\\r\\n\\x01y\\x01h@GRP@ @SUB@, \\x01wMESSAGE MENU\\x01y: ? \\x01n" \
			: "\\r\\n\\x01w\\x01hCommand >>? \\x01n";' },
	 'J': { eval: 'console.clear(); select_msg_area()' },
	 'R': { eval: 'bbs.scan_msgs()' },
	 'S': { eval: 'bbs.scan_subs(SCAN_FIND)'
			,msg: '\r\n\x01c\x01h' + gettext("Find Text in Messages") + '\r\n' },
	 'T': { eval: 'bbs.qwk_sec()' },
	 'U': 	{ exec: 'filescancfg.js' },
	 '1': { eval: 'bbs.menu("sysmain")'
			,ars: 'SYSOP or EXEMPT Q or I or N' },
	},
	nav: {
	'\r': { },
	 'F': { eval: 'enter_file_section(); menu = file_menu; \
			menu.prompt = (user.settings & USER_EXPERT) \
			? "\\r\\n\\x01y\\x01h@LIB@ @DIR@, \\x01wFILE MENU\\x01y: ? \\x01n" \
			: "\\r\\n\\x01w\\x01hCommand >>? \\x01n";' },
	 'Q': { eval:'menu = main_menu; menu.prompt = (user.settings & USER_EXPERT) \
			? "\\r\\n\\x01y\\x01h@GRP@ @SUB@, \\x01wMAIN MENU\\x01y: ? \\x01n" \
			: "\\r\\n\\x01w\\x01hCommand >>? \\x01n";'},
	},
};

shell.file_menu = {
	cls: true,
	file: "wildcat/file",
	eval: 'bbs.file_cmds++',
	node_action: NODE_XFER,
	prompt: (user.settings & USER_EXPERT)
		? "\r\n\x01y\x01h@LIB@ @DIR@, \x01wFILE MENU\x01y: ? \x01n"
		: "\r\n\x01w\x01hCommand >>? \x01n",
	command: {
	 'D': { eval: 'download_files()'
			,msg: '\r\n\x01c\x01h' + gettext("Download File(s)") + '\r\n'
			,ars: 'REST NOT D' },
	 'E': { eval: 'bbs.batch_menu()' },
	 'F': { eval: 'bbs.xfer_policy()' },
	 'G': { eval: 'bbs.logoff( !(user.settings & USER_EXPERT) );' },
	 'H': { eval: 'user.settings ^= USER_EXPERT; \
			console.putmsg("\\r\\nExpert mode is now: @EXPERT@"); \
			console.crlf(); menu.prompt = (user.settings & USER_EXPERT) \
			? "\\r\\n\\x01y\\x01h@GRP@ @SUB@, \\x01wMAIN MENU\\x01y: ? \\x01n" \
			: "\\r\\n\\x01w\\x01hCommand >>? \\x01n";' },
	 'I': { eval: 'view_file_info(FI_INFO)'
			,msg: '\r\n\x01c\x01h' + gettext("List Extended File Information") + '\r\n' },
	 'J': { eval: 'console.clear(); select_file_area()' },
	 'L': { eval: 'list_files()' },
	 'N': { eval: 'bbs.scan_dirs(FL_ULTIME)'
			,msg: '\r\n\x01c\x01h' + gettext("New File Scan") + '\r\n' },
	 'P': { eval: 'bbs.user_info() console.pause();' },
	 'R': { eval: 'view_file_info(FI_REMOVE)'
			,msg: '\r\n\x01c\x01h' + gettext("Remove/Edit File(s)") + '\r\n' },
	 'S': { eval: 'bbs.scan_dirs(FL_NO_HDR)'
			,msg: '\r\n\x01c\x01h' + gettext("Search for Filename(s)") + '\r\n' },
	 'T': { eval: 'bbs.temp_xfer()' },
	 'U': { eval: 'upload_file()'
			,msg: '\r\n\x01c\x01h' + gettext("Upload File") + '\r\n' },
	 'V': { eval: 'view_files()'
			,msg: '\r\n\x01c\x01h' + gettext("View File(s)") + '\r\n' },
	 '1': { eval: 'bbs.menu("sysxfer")'
			,ars: 'SYSOP' },
	},
	nav: {
	'\r': { },
	 'M': { eval: 'menu = main_menu; menu.prompt = (user.settings & USER_EXPERT) \
			? "\\r\\n\\x01y\\x01h@GRP@ @SUB@, \\x01wMAIN MENU\\x01y: ? \\x01n" \
			: "\\r\\n\\x01w\\x01hCommand >>? \\x01n";' },
	 'Q': { eval: 'menu = main_menu; menu.prompt = (user.settings & USER_EXPERT) \
			? "\\r\\n\\x01y\\x01h@GRP@ @SUB@, \\x01wMAIN MENU\\x01y: ? \\x01n" \
			: "\\r\\n\\x01w\\x01hCommand >>? \\x01n";' },
	},
};

js.wildcat_verify_user = function()
{
	console.print("\r\n\x01y\x01hUsername search string: \x01w");
	var name = console.getstr(25, K_UPRLWR);
	if(!name)
		return;

	var num = bbs.finduser(name);
	if(num)
		console.print("\x01y\x01hVerified: \x01w" + system.username(num) + "\r\n");
	else
		console.print("\r\n\x01r\x01hUnknown User.\r\n");
}

shell.menu = shell.main_menu;
shell.menu_loop();
