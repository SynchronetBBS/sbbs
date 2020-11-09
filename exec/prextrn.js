// prextrn.js

// External Program Pre Module
// These actions execute before an external program is launched via bbs.exec_xtrn()

"use strict";

load("sbbsdefs.js");

/* text.dat entries */
load("text.js");

var options, program;

if((options=load({}, "modopts.js","xtrn_sec")) == null)
	options = {};	// default values


function exec_xtrn_pre(program)
{
	if ((options.disable_pre_on_logon_event) && (bbs.node_action == NODE_LOGN)) {
		exit(1);
	}
	
	if (options.restricted_user_msg === undefined) {
		options.restricted_user_msg = bbs.text(R_ExternalPrograms);
	}

	if (user.security.restrictions&UFLAG_X) {
		write(options.restricted_user_msg);
		exit(1);
	}

	if (bbs.menu_exists("xtrn/" + program.code)) {
		bbs.menu("xtrn/" + program.code);
		console.pause();
		console.line_counter=0;
	}

	console.attributes = LIGHTGRAY;

	if (options.clear_screen_on_exec) {
		console.clear();
	}

	if (options.eval_before_exec) {
		eval(options.eval_before_exec);
	}

	load('fonts.js', 'xtrn:' + program.code);
}


/* main: */
{
	exec_xtrn_pre(xtrn_area.prog[argv[0].toLowerCase()]);
}
