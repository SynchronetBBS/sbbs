// OBV/2 Command Shell 
// Pure OBV/2 behavior, using shell_lib.js engine
// @format.tab-size 4, @format.use-tabs true

"use strict";

require("sbbsdefs.js", "SYS_RA_EMU");
require("userdefs.js", "USER_EXPERT");
require("nodedefs.js", "NODE_MAIN");
require("key_defs.js", "KEY_UP");
require("gettext.js", "gettext");
load("termsetup.js");

var shell = load({}, "shell_lib.js");

system.settings |= SYS_RA_EMU;  // Swap Msg ReRead/Reply commands

shell.help_key = '?';

// Force OBV/2 behavior: Cold Keys always ON
user.settings |= USER_COLDKEYS;

shell.main_menu = {
    cls: true,
    file: "obv-2/main",
    eval: 'bbs.main_cmds++',
    node_action: NODE_MAIN,
    prompt: "\x01n\x01gMain \x01cþ\x01h@TLEFT@\x01n\x01cþ " 
	    + "\x01h\x01kC\x01n\x01go\x01hm\x01nm\x01hand: \x01n",
    command: {
        'A': { eval: 'bbs.auto_msg()' },
        'B': { eval: 'bbs.exec_xtrn("sbbslist")' },
        'C': { eval: 'bbs.page_sysop()' },
        'D': { eval: 'bbs.list_logons()' },
        'F': { eval: 'send_feedback()' },
        'G': { eval: 'logoff(/* fast: */false)' },
       '/G': { eval: 'logoff(/* fast: */true)' },
        'H': { eval: 'bbs.whos_online()' },
        'I': { eval: 'bbs.chat_sec()' },
        'K': { eval: 'bbs.user_config(); exit()' },
        '+': { eval: 'bbs.user_config(); exit()' },
        'L': { eval: 'bbs.list_users(UL_ALL)' },
        'P': { eval: 'bbs.xtrn_sec()' },
        'S': { eval: 'bbs.sys_info();' },
        'U': { eval: 'bbs.time_bank()' },
        'W': { eval: 'bbs.exec("?logonlist")' },
        'Y': { eval: 'bbs.user_info()' },
        'X': { eval: 'user.settings ^= USER_EXPERT' },
        'Z': { eval: 'bbs.reinit_msg_ptrs()' }
    },
    nav: {
        '\r': { },
        'E': { eval: 'menu = email_menu' },
        'M': { eval: 'menu = message_menu' },
        'Q': { eval: 'menu = quick_menu' },
        'T': { eval: 'enter_file_section(); menu = file_menu' }
    }
};

shell.message_menu = {
    cls: true,
    file: "obv-2/message",
    node_action: NODE_RMSG,
    prompt: "\x01n\x01gMessage \x01cþ \x01h@TLEFT@\x01n\x01cþ " 
        + "\x01c\x01h[@GN@] @GRP@ [@SN@] @SUB@: \x01n",
    command: {
        'A': { eval: 'console.clear();'
               + 'select_msg_area();' },
        'B': { eval: 'console.clear();'
               + 'show_subs(bbs.curgrp)' },
        '*': { eval: 'console.clear();'
               + 'show_subs(bbs.curgrp);' },
        'C': { eval: 'bbs.cfg_msg_scan(SCAN_NEW)' },
        'G': { eval: 'logoff(/* fast: */false)' },
       '/G': { eval: 'logoff(/* fast: */true)' },
        'J': { eval: 'console.clear();'
               + 'select_msg_area();' },
        'N': { eval: 'bbs.scan_msgs()' },
        'O': { eval: 'bbs.menu_dir = "obv-2";'
               + 'bbs.qwk_sec();'
               + 'bbs.menu_dir = "";' },
        'P': { eval: 'bbs.post_msg()' },
        'R': { eval: 'bbs.scan_msgs()' },
        'Y': { eval: 'bbs.scan_subs(SCAN_TOYOU)' },
        'Z': { exec: 'msgscancfg.js' },
		'+': { eval: 'sub_up()' },
        '-': { eval: 'sub_down()' },
        '>': { eval: 'grp_up()' },
        '<': { eval: 'grp_down()' },
    },
    nav: {
        '\r': { },
        'Q': { eval: 'menu = main_menu' },
        'T': { eval: 'enter_file_section(); menu = file_menu' },
        '+': { eval: 'sub_up()' },
        '-': { eval: 'sub_down()' },
        '>': { eval: 'grp_up()' },
        '<': { eval: 'grp_down()' },
        '*': { eval: 'show_subs(bbs.curgrp)' },
        '/*': { eval: 'show_grps()' }
    }
};

// Arrow key navigation
shell.message_menu.nav[KEY_UP] = { eval: 'sub_up()' };
shell.message_menu.nav[KEY_DOWN] = { eval: 'sub_down()' };
shell.message_menu.nav[KEY_RIGHT] = { eval: 'grp_up()' };
shell.message_menu.nav[KEY_LEFT] = { eval: 'grp_down()' };

shell.file_menu = {
    cls: true,
    file: "obv-2/files",
    eval: 'bbs.file_cmds++',
    node_action: NODE_XFER,
    prompt: "\x01n\x01gFile \x01cþ \x01h@TLEFT@ \x01n\x01cþ " 
        + "\x01h\x01c(@LN@) @LIB@ (@DN@) @DIR@: ",
    num_input: shell.get_dir_num,
    slash_num_input: shell.get_lib_num,
    command: {
        'A': { eval: 'console.clear();'
               + 'select_file_area();' },
        'B': { eval: 'bbs.batch_menu()' },
        'C': { exec: 'filescancfg.js' },
       '/D': { eval: 'download_files()'
               ,msg: '\r\n\x01c\x01h' + gettext("Download File(s)") + '\r\n'
	           ,ars: 'REST NOT D' },
		'E': { eval: 'view_file_info(FI_REMOVE)'
			   ,msg: '\r\n\x01c\x01h' + gettext("Remove/Edit File(s)") + '\r\n' },	
        'G': { eval: 'logoff(/* fast: */false)' },
       '/G': { eval: 'logoff(/* fast: */true)' },
		'K': { eval: "console.clear();" + "console.print('\\x01n\\x01h\\x01cFile Configuration Listing\\r\\n');" +
            "console.print('-----------------------------------\\r\\n');" +
            "var curlib = bbs.curlib;" +
            "var curdir = bbs.curdir;" +
            "console.print('Current Library : ' + curlib + ' - ' " +
            "+ file_area.lib_list[curlib].description + '\\r\\n');" +
			"console.print('Current Dir     : ' + curdir + ' - ' " +
            "+ file_area.lib_list[curlib].dir_list[curdir].description + '\\r\\n');" +
            "console.print('New-scan Date   : ' + system.timestr(user.new_file_time) + '\\r\\n');" +
			"var batch = (bbs.batch_size ? bbs.batch_size : 0);" +
			"console.print('Batch Queue     : ' + batch + ' file(s)\\r\\n');" +
			"var prot = 'Zmodem';" +
			"if (user.download_protocol == 'Y')" +
            "prot = 'Ymodem';" +
			"else if (user.download_protocol == 'X')" +
            "prot = 'Xmodem';" +
			"else if (user.download_protocol == '')" +
            "prot = 'None';" +
			"console.print('Protocol        : ' + prot + '\\r\\n');" +
			"var ul = (typeof user.ulb === 'number' ? user.ulb : 0);" +
            "var dl = (typeof user.dlb === 'number' ? user.dlb : 0);" +
            "console.print('UL Today        : ' + ul + ' bytes\\r\\n');" +
            "console.print('DL Today        : ' + dl + ' bytes\\r\\n');" +
            "console.crlf();" },
        'L': { eval: 'list_files()' },
        'N': { eval: 'bbs.scan_dirs(FL_ULTIME)'
               ,msg: '\r\n\x01c\x01h' + gettext("New File Scan") + '\r\n' },
        'R': { eval: 'bbs.batch_menu()' },       
        'S': { eval: 'bbs.scan_dirs(FL_NO_HDR)'
               ,msg: '\r\n\x01c\x01h' + gettext("Search for Filename(s)") + '\r\n' },
        'T': { eval: 'bbs.temp_xfer()' },
        'U': { eval: 'upload_file()',
        	   ,msg: '\r\n\x01c\x01h' + gettext("Upload File") + '\r\n' },
	   '/U': { eval: 'upload_user_file()'
			   ,msg: '\r\n\x01c\x01h' + gettext("Upload File to User") + '\r\n' },
        'V': { eval: 'view_files()'
               ,msg: '\r\n\x01c\x01h' + gettext("View File(s)") + '\r\n' },
        'W': { eval: 'view_file_info(FI_INFO)'
               ,msg: '\r\n\x01c\x01h' + gettext("List Extended File Information") + '\r\n' },
        'X': { eval: "if (file_exists(system.text_dir + 'jsyswant.txt')) {"
			   + "console.crlf();"
               + "bbs.printfile(system.text_dir + 'jsyswant.txt');"
               + "} else {"
               + "console.crlf();"
               + "console.print('\\x01n\\x01hNo particular files are being sought.\\r\\n');"
               + "console.crlf();"
               + "}" },
	},
    nav: {
        '\r': { },
        'M': { eval: 'menu = message_menu' },
        'Q': { eval: 'menu = main_menu' },
        '+': { eval: 'dir_up()' },
        '-': { eval: 'dir_down()' },
		'*': { eval: 'show_dirs(bbs.curlib)' }, 
	   '/*': { eval: 'show_libs()' },
       '/+': { eval: 'lib_up()' },
       '/-': { eval: 'lib_down()' },
    }
};

// Arrow key navigation
shell.file_menu.nav[KEY_UP] = { eval: 'dir_up()' };
shell.file_menu.nav[KEY_DOWN] = { eval: 'dir_down()' };
shell.file_menu.nav[KEY_RIGHT] = { eval: 'lib_up()' };
shell.file_menu.nav[KEY_LEFT] = { eval: 'lib_down()' };

shell.email_menu = {
    cls: true,
    file: "obv-2/email",
    prompt: "\x01h\x01kEmail \x01n\x01gþ\x01h@TLEFT@\x01n\x01gþ "
        + "\x01h\x01kC\x01n\x01go\x01hm\x01nm\x01hand: \x01n",
    command: {
        'F': { eval: 'send_email()' },
        'R': { eval: 'bbs.read_mail(MAIL_YOUR, user.number)' },
        'S': { eval: 'send_email()' },
        'V': { eval: 'bbs.read_mail(MAIL_SENT, user.number)' }
    },
    nav: {
        '\r': { },
        'Q': { eval: 'menu = main_menu' }
    }
};

shell.quick_menu = {
    cls: true,
    file: "obv-2/quick",
    prompt: "\x01n\x01gQuick \x01cþ\x01h@TLEFT@\x01n\x01cþ "
		+ "\x01h\x01kC\x01n\x01go\x01hm\x01nm\x01hand: \x01n",
    command: {
        'C': { eval: 'bbs.page_sysop()' },
        'E': { eval: 'menu = email_menu' },
        'F': { eval: 'send_feedback()' },
        'G': { eval: 'logoff(/* fast: */false)' },
       '/G': { eval: 'logoff(/* fast: */true)' },
        'M': { eval: 'menu = message_menu' },
        'Q': { eval: 'menu = main_menu' },
        'T': { eval: 'menu = file_menu' }
    },
    nav: {
        '\r': { },
        'E': { eval: 'menu = email_menu' },
        'M': { eval: 'menu = message_menu' },
        'Q': { eval: 'menu = main_menu' },
        'T': { eval: 'menu = file_menu' }
    }
};

shell.menu = shell.main_menu;
shell.menu_loop();