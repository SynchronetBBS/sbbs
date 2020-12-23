// postxtrn.js

// External Program Post-execution Module
// These actions execute after an external program is launched via bbs.exec_xtrn()

"use strict";

function exec_xtrn_post(prog)
{
	var options;

	if ((options = load({}, "modopts.js","xtrn:" + prog.code)) == null) {
		if ((options = load({}, "modopts.js","xtrn_sec")) == null)
			options = {};	// default values
	}

	require("nodedefs.js", "NODE_LOGN");
	if ((options.disable_post_on_logon_event) && (bbs.node_action == NODE_LOGN)) {
		exit(0);
	}

	require("cga_defs.js", "LIGHTGRAY");
	console.attributes = 0;
	console.attributes = LIGHTGRAY;

	load('fonts.js', 'default');

	if (options.eval_after_exec) {
		eval(options.eval_after_exec);
	}
}

/* main: */
{
	exec_xtrn_post(xtrn_area.prog[argv[0].toLowerCase()]);
}
