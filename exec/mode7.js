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
const time_code = user.security.exemptions & UFLAG_T ? "\x86Time Used: @TUSED@" : "\x86Time Left: @TLEFT@";

const main_menu = {
	file: "mode7/mode7_main",
	eval: 'bbs.main_cmds++',
	node_action: NODE_MAIN,
	prompt: time_code
		+ gettext("\x87->\x83Main\x87-> "),
	command: {
	 'C': { exec: 'mode7/mode7_chat.js' },
	 'D': { exec: 'eotl_xtrn.js' },
	 'E': { exec: 'mode7/mode7_email.js' },
	 'F': { exec: 'eotl_xfer.js' },
	 'G': { eval: 'shell.logoff(/* fast: */false)' },
	 'N': { exec: 'mode7/mode7_forums.js' },
	'/G': { eval: 'shell.logoff(/* fast: */true)' },
	'/O': { eval: 'shell.logoff(/* fast: */true)' },
	 'S': { exec: 'eotl_settings.js' },
	 'T': { exec: '../xtrn/ansiview/ansiview.js' },
	 'X': { exec: 'eotl_sysop.js'
			,ars: 'SYSOP' },
	 '!': { eval: 'bbs.menu("sysmain")'
			,ars: 'SYSOP' },
	},
	nav: {
	'\r': { },
	},
};

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
		cmd = console.getstr();
		if(cmd == '!')
			cmd = last_str_cmd;
//		var script = system.mods_dir + "str_cmds.js";
//		if(!file_exists(script))
//			script = system.exec_dir + "str_cmds.js";
//		js.exec(script, {}, cmd);
		load({}, "str_cmds.js", cmd);
		last_str_cmd = cmd;
		continue;
	}
	if(cmd == '/') {
		cmd = console.getkey(K_UPPER);
		console.print(cmd);
		cmd = '/' + cmd;
	}
	if(cmd > ' ') {
		bbs.log_key(cmd, /* comma: */true);
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
		console.print("\r\n\x81" + gettext("Unrecognized command."));
		if(user.settings & USER_EXPERT)
			console.print(" " + gettext("Hit") + "'" + help_key + "'" + gettext("for a menu."));
		console.newline();
		continue;
	}
	if(!bbs.compare_ars(menu_cmd.ars))
		console.print(menu_cmd.err);
	else {
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
}
// Can't do these statically through initialization:
main_menu.nav[KEY_UP] = { eval: 'shell.sub_up()' };
main_menu.nav[KEY_DOWN] = { eval: 'shell.sub_down()' };
main_menu.nav[KEY_RIGHT] = { eval: 'shell.grp_up()' };
main_menu.nav[KEY_LEFT] = { eval: 'shell.grp_down()' };
