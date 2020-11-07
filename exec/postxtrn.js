// postxtrn.js

// External Program Post Module
// These actions execute after an external program is launched via bbs.exec_xtrn()

// $Id: xtrn_sec.js,v 1.29 2020/05/09 10:11:23 rswindell Exp $

"use strict";

load("sbbsdefs.js");

/* text.dat entries */
load("text.js");

var options, program;

if((options=load({}, "modopts.js","xtrn_sec")) == null)
	options = {};	// default values

if(options.restricted_user_msg === undefined)
	options.restricted_user_msg = bbs.text(R_ExternalPrograms);

if(options.clear_screen === undefined)
	options.clear_screen = true;

function exec_xtrn_post(program)
{
	if ((options.disable_xtrnpost_on_logon_event) && (bbs.node_action == NODE_LOGN)) {
		return;
	}

	console.attributes = 0;
	console.attributes = LIGHTGRAY;

	load('fonts.js', 'default');

	if(options.eval_after_exec)
		eval(options.eval_after_exec);
}


/* main: */
{
	if(!argv[0]) {
		write(bbs.text(NoXtrnProgram));
	} else {
		xtrn_area.sec_list.some(function(sec) {
			sec.prog_list.some(function (prog) {
				if (prog.code.toLowerCase() == argv[0]) {
					program = prog;
					return true;
				}
			});
		});

		if (!program) {
			write(bbs.text(NoXtrnProgram));
		} else {
			exec_xtrn_pre(program);
		}
	}
}
