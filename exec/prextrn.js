// prextrn.js

// External Program Pre-execution Module
// These actions execute before an external program is launched via bbs.exec_xtrn()

"use strict";

function exec_xtrn_pre(prog)
{
	var options;

	if ((options = load({}, "modopts.js","xtrn:" + prog.code)) == null) {
		if ((options = load({}, "modopts.js","xtrn_sec")) == null)
			options = {};	// default values
	}

	// If displaying CODE.ans file, prompt the user to continue to enter the game or not
	if (typeof options.prompt_on_info === "undefined") {
		options.prompt_on_info = false;
	}

	if (typeof options.prompt_on_info_fmt === "undefined") {
		options.prompt_on_info_fmt = "\r\n\x01n\x01cDo you wish to continue";
	}

	require("nodedefs.js", "NODE_LOGN");
	if (bbs.node_action == NODE_LOGN) {
		if (options.disable_pre_on_logon_event) {
			exit(0);
		}
	} else {
		if (options.restricted_user_msg === undefined) {
			require("text.js", "R_ExternalPrograms");
			options.restricted_user_msg = bbs.text(R_ExternalPrograms);
		}
		require("userdefs.js", "UFLAG_X");
		if (user.security.restrictions&UFLAG_X) {
			write(options.restricted_user_msg);
			exit(1);
		}
	}

	if (bbs.menu_exists("xtrn/" + prog.code)) {
		bbs.menu("xtrn/" + prog.code);

		// If displaying CODE.ans file, prompt the user to continue to enter the game or not
		if (options.prompt_on_info) {
			if (!console.yesno(options.prompt_on_info_fmt)) {
				exit(1);
			}
		} else {
			console.pause();
		}
		console.line_counter=0;
	}

	require("cga_defs.js", "LIGHTGRAY");
	console.attributes = LIGHTGRAY;

	if (options.clear_screen_on_exec) {
		console.clear();
	}

	if (options.eval_before_exec) {
		eval(options.eval_before_exec);
	}

	load('fonts.js', 'xtrn:' + prog.code);
}


/* main: */
{
	exec_xtrn_pre(xtrn_area.prog[argv[0].toLowerCase()]);
}
