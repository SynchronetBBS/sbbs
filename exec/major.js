// MajorBBS Clone Command Shell for Synchronet

"use strict";

require("sbbsdefs.js", "K_UPPER");
require("userdefs.js", "USER_EXPERT");
require("nodedefs.js", "NODE_MAIN");
load("termsetup.js");

var shell = load({}, "shell_lib.js");

system.settings &= ~SYS_RA_EMU;

shell.help_key = '?';

shell.main_menu = {
	file: "major/main",
	cls: true,
	eval: 'bbs.main_cmds++',
	node_action: NODE_MAIN,
	prompt: "\x01h\x01c(TOP)\r\n"
		+ "Make your selection (T,I,F,E,L,A,P,R,S,? for help, or 'X' to exit): ",
	command: {
	 'D': { eval: 'bbs.xtrn_sec()' },
	 'I': { eval: 'bbs.text_sec()' },
	 'R': { eval: 'list_users()' },
	 'T': { eval: 'bbs.chat_sec()' },
	 'X': { eval: 'logoff(false)' },
	 'S': { eval: 'bbs.menu("sysmain")'
			,ars: 'SYSOP or EXEMPT Q or I or N' },
	},
	nav: {
	'\r': {},
	 'A': { eval: 'menu = userdefs_menu' },
	 'E': { eval: 'menu = email_menu' },
	 'F': { eval: 'menu = message_menu' },
	 'L': { eval: 'enter_file_section(); menu = file_menu' },
	}
};

shell.file_menu = {
	file: "major/file",
	cls: true,
	eval: 'bbs.main_cmds++',
	node_action: NODE_XFER,
	prompt: "\x01h\x01cCurrent LIB: @LIB@ @DIR@\r\n"
		+ "Select a letter from this list (or 'X' to exit): ",
	command: {
	'D': { eval: 'download_files()'
		   ,msg: '\r\n\x01c\x01hDownload File(s)\r\n'
		   ,ars: 'REST NOT D' },
	'F': { eval: 'list_files()' },
	'S': { eval: 'console.clear(); select_file_area();' },
	'U': { eval: 'upload_file()'
		   ,msg: '\r\n\x01c\x01hUpload File\r\n' },
	},
	nav: {
	'\r': {},
	 'X': { eval: 'menu = main_menu' },
	},
};

shell.userdefs_menu = {
	file: "major/userdefs",
	cls: true,
	eval: 'bbs.main_cmds++',
	node_action: NODE_MAIN,
	prompt: "\x01h\x01cYour choice (or 'X' to exit): ",
	command: {
	 'A': { eval: 'bbs.user_config(); exit()' },
	 'S': { eval: 'bbs.user_info()' },
	},
	nav: {
	'\r': {},
	 'X': { eval: 'menu = main_menu' },
	}
};

shell.message_menu = {
	cls: true,
	file: "major/msg",
	eval: 'bbs.main_cmds++',
	node_action: NODE_RMSG,
	prompt: "\x01h\x01cSelect a letter from this list, or 'X' to exit: ",
	command: {
	 'R': { eval: 'bbs.scan_msgs()' },
	 'S': { eval: 'console.clear(); select_msg_area()' },
	 'T': { eval: 'bbs.chat_sec()' },
	 'Q': { eval: 'toggle_user_misc(UM_ASK_SSCAN)' },
	 'W': { eval: 'console.clear(); bbs.post_msg()' },
	},
	nav: {
	'\r': {},
	 'Q': { eval: 'menu = quickscn_menu' },
	 'X': { eval: 'menu = main_menu' },
	}
};

shell.email_menu = {
	cls: true,
	file: "major/email",
	eval: 'bbs.main_cmds++',
	node_action: NODE_RMAL,
	prompt: "\x01h\x01cSelect a letter from this list, or 'X' to exit: ",
	command: {
	 'E': { eval: 'bbs.read_mail(MAIL_SENT, user.number)'},
	 'R': { eval: 'bbs.read_mail(MAIL_YOUR, user.number)' },
	 'W': { eval: 'send_email()' },
	 'S': { eval: 'send_netmail()' },
	 'U': { eval: 'send_email()' }, 
	},
	nav: {
	'\r': {},
	 'X': { eval: 'menu = main_menu' },
	}
};

shell.quickscn_menu = {
	cls: true,
	file: "major/quickscn",
	eval: 'bbs.main_cmds++',
	node_action: NODE_MAIN,
	prompt: "\x01h\x01cSelect a letter from this list, or 'X' to exit: ",
	command: {
	 'C': { exec: 'msgscancfg.js' },
	 'K': { eval: 'bbs.scan_subs(SCAN_FIND)' },
	 'L': { eval: 'bbs.scan_subs(SCAN_TOYOU)' },
	 'S': { eval: 'bbs.scan_subs(SCAN_NEW)' }, 
	 },
	nav: {
	'\r': {},
	 'X': { eval: 'menu = main_menu' },
	}
};

shell.menu = shell.main_menu;
shell.menu_loop();
