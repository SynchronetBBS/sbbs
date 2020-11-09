// postxtrn.js

// External Program Post Module
// These actions execute after an external program is launched via bbs.exec_xtrn()

"use strict";

load("sbbsdefs.js");

/* text.dat entries */
load("text.js");

var options, program;

if((options=load({}, "modopts.js","xtrn_sec")) == null)
	options = {};	// default values

if(options.clear_screen === undefined)
	options.clear_screen = true;

function exec_xtrn_post(program)
{
	if ((options.disable_post_on_logon_event) && (bbs.node_action == NODE_LOGN)) {
		exit(1);
	}

	console.attributes = 0;
	console.attributes = LIGHTGRAY;

	load('fonts.js', 'default');

	if (options.eval_after_exec) {
		eval(options.eval_after_exec);
	}
}


/* main: */
{
	exec_xtrn_post(xtrn_area.prog[argv[0].toLowerCase()] );
}
