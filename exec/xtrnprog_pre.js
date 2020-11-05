// xtrnprog_pre.js

// External Program Pre Module
// These actions execute before an external program is launched via bbs.exec_xtrn()

// $Id: xtrn_sec.js,v 1.29 2020/05/09 10:11:23 rswindell Exp $

"use strict";

load("sbbsdefs.js");

/* text.dat entries */
load("text.js");

var options, program;

if((options=load({}, "modopts.js","xtrn_sec")) == null)
	options = {};	// default values


function exec_xtrn_pre(program)
{
	if(options.restricted_user_msg === undefined)
		options.restricted_user_msg = bbs.text(R_ExternalPrograms);

	if(user.security.restrictions&UFLAG_X) {
		write(options.restricted_user_msg);
		return;
	}

	if(bbs.menu_exists("xtrn/" + program.code)) {
		bbs.menu("xtrn/" + program.code);
		console.pause();
		console.line_counter=0;
	}

	console.attributes = LIGHTGRAY;

	if(options.clear_screen_on_exec)
		console.clear();

	if(options.eval_before_exec)
		eval(options.eval_before_exec);

	load('fonts.js', 'xtrn:' + proggram.code);
}


/* main: */
{
	if(!$argv[0]) {
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
			exec_xtrn_pre(progeam);
		}
	}
}
