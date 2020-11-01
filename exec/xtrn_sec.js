// xtrn_sec.js

// Synchronet External Program Section
// Menus displayed to users via Terminal Server (Telnet/RLogin/SSH)

// To jump straight to a specific xtrn section, pass the section code as an
// argument

// To set options, add to modopts.ini [xtrn_sec]

// $Id: xtrn_sec.js,v 1.29 2020/05/09 10:11:23 rswindell Exp $

"use strict";

load("sbbsdefs.js");

/* text.dat entries */
load("text.js");

load("xtrnmenulib.js");

var options, menuconfig, xsec = -1;


//// Main
var ExternalMenus = new ExternalMenus();
var options = ExternalMenus.options;
var menuconfig = ExternalMenus.menuconfig;

// override and turn off multicolumn if terminal width is less than 80
if (console.screen_columns < 80) {
	options.multicolumn = false;
}

/* See if an xtrn section code or xtrn program code was passed as an argument */
if (typeof argv[0] !== "undefined") {
	xtrn_area.sec_list.some( function(sec) {
		if (argv[0].toLowerCase() == sec.code) {
			external_program_menu(sec.index);
			return true;
		}

		sec.prog_list.some( function(prog) {
			if (argv[0].toLowerCase() == prog.code) {
				exec_xtrn(prog);
				return true;
			}
		});
	});
} else {
	//if (xtrn_area.sec_list.length == 1) {
	// if only one section defined, display it
	//	external_program_menu(0);
	//} else {
	// display all sections menu

	//if (typeof menuconfig.menus !== "undefined") {
	external_section_menu_custom();
	//} else {
	//	external_section_menu();
	//s}
	//}
}

// Run an external program
function exec_xtrn(prog)
{
	// if custom file exists as text/menu/xtern/CODE[.rip|.ans|.mon|.msg|.asc], display that file
	if (bbs.menu_exists("xtrn/" + prog.code)) {
		bbs.menu("xtrn/" + prog.code);
		console.line_counter = 0; // this was in original code, not sure why. maybe to prevent extra "hit a key"
		if (options.prompt_on_info) {
			if (!confirm(options.prompt_on_info_fmt)) {
				return;
			}
		} else {
			console.pause();
		}
	}

	console.attributes = LIGHTGRAY;

	if (options.clear_screen_on_exec) {
		console.clear();
	}

	if (options.eval_before_exec) {
		eval(options.eval_before_exec);
	}

	load('fonts.js', 'xtrn:' + prog.code);

	// execute external program
	// if doorscan exists, run it
	if (file_exists("../xtrn/doorscan/doorscan.js")) {
		try {
			// when running multiple times, it loses the cwd so explicitly set system.exec_dir
			js.exec(system.exec_dir + "../xtrn/doorscan/doorscan.js", {}, "run", prog.code);
		} catch(e) {
			log("Error running " + prog.code + " " + e);
		}
	} else {
		// no doorscan, launch door normally
		bbs.exec_xtrn(prog.code);
	}

	// reset console attributes after program is done
	console.attributes = 0;
	console.attributes = LIGHTGRAY;
	load('fonts.js', 'default');
	if (options.eval_after_exec) {
		eval(options.eval_after_exec);
	}

	if (prog.settings&XTRN_PAUSE) {
		console.pause();
	} else {
		console.line_counter = 0;
	}
}

// Display a specific section menu, listing all of the external programs in it
function external_program_menu(xsec)
{
	var i, j;

	while (bbs.online) {

		console.aborted = false;
		if (user.security.restrictions&UFLAG_X) {
			write(options.restricted_user_msg);
			break;
		}

		var prog_list=xtrn_area.sec_list[xsec].prog_list.slice();   /* prog_list is a possibly-sorted copy of xtrn_area.sec_list[x].prog_list */

		if (!prog_list.length) {
			write(options.no_programs_msg);
			console.pause();
			break;
		}

		// If there's only one program available to the user in the section, just run it (or try to)
		if (options.autoexec && prog_list.length == 1) {
			exec_xtrn(prog_list[0]);
			break;
		}

		if (options.clear_screen) {
			console.clear(LIGHTGRAY);
		}

		var secnum = xtrn_area.sec_list[xsec].number+1

		if (bbs.menu_exists("xtrn" + secnum + "_head")) {
			bbs.menu("xtrn" + secnum + "_head");
		} else if (bbs.menu_exists("xtrn_" + xsec + "_head")) {
			bbs.menu("xtrn_" + xsec + "_head");
		}

		if (bbs.menu_exists("xtrn" + secnum)) {
			bbs.menu("xtrn" + secnum);
		} else if (bbs.menu_exists("xtrn_" + xsec)) {
			bbs.menu("xtrn_" + xsec);
		} else {
			var multicolumn = options.multicolumn && prog_list.length > options.singlecolumn_height;

			if (options.sort) {
				prog_list.sort(ExternalMenus.sort_by_name);
			}

			printf(options.header_fmt, xtrn_area.sec_list[xsec].name);

			// title line
			if (options.title_fmt != "none") {
				write(options.title_fmt);
				if (multicolumn) {
					write(options.multicolumn_separator);
					write(options.title_fmt);
				}
				console.crlf();
			}

			// underline line
			if (options.underline_fmt != "none") {
				write(options.underline_fmt);
				if (multicolumn) {
					write(options.multicolumn_separator);
					write(options.underline_fmt);
				}
				console.crlf();
			}

			var n;

			if (multicolumn) {
				n = Math.floor(prog_list.length / 2) + (prog_list.length & 1);
			} else {
				n = prog_list.length;
			}

			for (i=0; i<n && !console.aborted; i++) {
				console.add_hotspot(i+1);
				printf(multicolumn ? options.multicolumn_fmt : options.singlecolumn_fmt,
					i+1,
					prog_list[i].name,
					prog_list[i].cost
				);

				if (multicolumn) {
					j=Math.floor(prog_list.length/2)+i+(prog_list.length&1);
					if (j<prog_list.length) {
						write(options.multicolumn_separator);
						console.add_hotspot(j+1);
						printf(options.multicolumn_fmt, j+1,
							prog_list[j].name,
							prog_list[j].cost
						);
					}
				}
				console.crlf();
			}
			bbs.node_sync();
			console.mnemonics(options.which);
		}

		system.node_list[bbs.node_num-1].aux=0; /* aux is 0, only if at menu */
		bbs.node_action=NODE_XTRN;
		bbs.node_sync();

		if ((i = console.getnum(prog_list.length)) < 1) {
			break;
		}

		i--;

		// run the external program
		exec_xtrn(prog_list[i]);
	}
}

/*
// Renders the top-level external menu
function external_section_menu()
{
    var i, j;

    while (bbs.online) {
		console.aborted = false;
	    if (user.security.restrictions&UFLAG_X) {
		    write(options.restricted_user_msg);
		    break;
	    }

	    if (!xtrn_area.sec_list.length) {
		    write(options.no_programs_msg);
		    break;
	    }

	    var xsec = 0;
	    var sec_list = xtrn_area.sec_list.slice();    // sec_list is a possibly-sorted copy of xtrn_area.sect_list

		system.node_list[bbs.node_num-1].aux = 0; // aux is 0, only if at menu
		bbs.node_action = NODE_XTRN;
		bbs.node_sync();

		if (bbs.menu_exists("xtrn_sec")) {
			// if file exists text/menu/xtern_sec.[rip|ans|mon|msg|asc], then display that instead
			bbs.menu("xtrn_sec");
			xsec=console.getnum(sec_list.length);
			if (xsec <= 0) {
				break;
			}
			xsec--;
		} else {
			// if no custom menu file in text/menu, create a dynamic one
			if (options.sort) {
				sec_list.sort(ExternalMenus.sort_by_name);
			}
			for (i in sec_list) {
				// user selection menu, called for each item
				// args: number, menu title, item
				console.uselect(Number(i), "External Program Section", sec_list[i].name);
			}

			// user selection menu, called with no args will display select menu
			xsec = console.uselect();
		}
	    if (xsec < 0) {
			break;
		}

        external_program_menu(sec_list[xsec].index);
    }
}
*/

// Renders the top-level external menu
function external_section_menu_custom(menuid)
{
	var i, menucheck, menuobj, item_multicolumn_format, item_singlecolumn_format,
		cost, multicolumn, menuitemsfiltered = [];
	var keylimit = 1; // max chars allowed on menu selection

	menuobj = ExternalMenus.getMenu(menuid);

	// Allow overriding auto-format on a per-menu basis
	var multicolumn_format, singlecolumn_format, header_format, title_format,
		underline_format, multicolumn_separator, which, singlecolumn_height;

	if ((typeof menuobj.custom_menu_multicolumn_fmt !== "undefined") && menuobj.custom_menu_multicolumn_fmt) {
		multicolumn_format = menuobj.custom_menu_multicolumn_fmt;
	} else if (options.custom_menu_multicolumn_fmt) {
		multicolumn_format = options.custom_menu_multicolumn_fmt;
	} else {
		multicolumn_format = options.multicolumn_fmt;
	}

	if ((typeof menuobj.custom_menu_singlecolumn_fmt !== "undefined") && menuobj.custom_menu_singlecolumn_fmt) {
		singlecolumn_format = menuobj.custom_menu_singlecolumn_fmt;
	} else if (options.custom_menu_singlecolumn_fmt) {
		singlecolumn_format = options.custom_menu_singlecolumn_fmt;
	} else {
		singlecolumn_format = options.singlecolumn_fmt;
	}

	if ((typeof menuobj.header_fmt !== "undefined") && menuobj.header_fmt) {
		header_format = menuobj.header_fmt;
	} else {
		header_format = options.header_fmt;
	}

	if ((typeof menuobj.title_fmt !== "undefined") && menuobj.title_fmt) {
		title_format = menuobj.title_fmt;
	} else {
		title_format = options.title_fmt;
	}

	if ((typeof menuobj.underline_fmt !== "undefined") && menuobj.underline_fmt) {
		underline_format = menuobj.underline_fmt;
	} else {
		underline_format = options.underline_fmt;
	}

	if ((typeof menuobj.multicolumn_separator !== "undefined") && menuobj.multicolumn_separator) {
		multicolumn_separator = menuobj.multicolumn_separator;
	} else {
		multicolumn_separator = options.multicolumn_separator;
	}

	if ((typeof menuobj.which !== "undefined") && menuobj.which) {
		which = menuobj.which;
	} else {
		which = options.which;
	}

	if ((typeof menuobj.singlecolumn_height !== "undefined") && menuobj.singlecolumn_height) {
		singlecolumn_height = menuobj.singlecolumn_height;
	} else {
		singlecolumn_height = options.singlecolumn_height;
	}

	while (bbs.online) {
		console.aborted = false;

		if (typeof menuobj === "undefined") {
			doerror(options.custom_menu_not_found_msg.replace('%MENUID%', menuid));
			break;
		}

		if (options.clear_screen) {
			console.clear(LIGHTGRAY);
		}

		if (user.security.restrictions&UFLAG_X) {
			write(options.restricted_user_msg);
			break;
		}

		if (!xtrn_area.sec_list.length) {
			write(options.no_programs_msg);
			break;
		}

		var keyin;

		system.node_list[bbs.node_num-1].aux = 0; /* aux is 0, only if at menu */
		bbs.node_action = NODE_XTRN;
		bbs.node_sync();

		menuitemsfiltered = ExternalMenus.getSortedItems(menuobj);

		if (bbs.menu_exists("xtrn_custom_" + menuid)) {
			// if file exists text/menu/xtern_custom_(menuid).[rip|ans|mon|msg|asc], then display that instead
			bbs.menu("xtrn_custom_" + menuid);
		} else {
			// if no custom menu file in text/menu, create a dynamic one
			console.crlf();

			multicolumn = options.multicolumn && menuitemsfiltered.length > singlecolumn_height;
			printf(header_format, menuobj.title);

			// title line
			if (title_format != "none") {
				write(title_format);
				if (multicolumn) {
					write(multicolumn_separator);
					write(title_format);
				}
				console.crlf();
			}

			// underline line
			if (underline_format != "none") {
				write(underline_format);
				if (multicolumn) {
					write(multicolumn_separator);
					write(underline_format);
				}
				console.crlf();
			}

			// n is the number of items for the 1st column
			var n;
			if (multicolumn) {
				n = Math.floor(menuitemsfiltered.length / 2) + (menuitemsfiltered.length & 1);
			} else {
				n = menuitemsfiltered.length;
			}

			// j is the index for each menu item on 2nd column
			var j = n; // start j at the first item for 2nd column
			for (i = 0; i < n && !console.aborted; i++) {
				cost = "";

				if (menuitemsfiltered[i].type == "xtrnprog") {
					// if its an external program, get the cost
					cost = xtrn_area.prog[menuitemsfiltered[i].target.toLowerCase()].cost;
				}

				console.add_hotspot(menuitemsfiltered[i].input);

				if (menuitemsfiltered[i].input.toString().length > keylimit) {
					keylimit = menuitemsfiltered[i].input.toString().length;
				}

				// allow overriding format on a per-item basis
				// great for featuring a specific game
				item_multicolumn_format = menuitemsfiltered[i].multicolumn_fmt
					? convert_input_literals_to_js(menuitemsfiltered[i].multicolumn_fmt) : multicolumn_format;
				item_singlecolumn_format = menuitemsfiltered[i].singlecolumn_fmt
					? convert_input_literals_to_js(menuitemsfiltered[i].singlecolumn_fmt) : singlecolumn_format;

				printf(multicolumn ? item_multicolumn_format : item_singlecolumn_format,
					menuitemsfiltered[i].input.toString().toUpperCase(),
					menuitemsfiltered[i].title,
					cost
				);

				if (multicolumn) {
					if ((typeof(menuitemsfiltered[j]) !== "undefined")) {
						// allow overriding format on a per-item basis
						// great for featuring a specific game
						item_multicolumn_format = menuitemsfiltered[j].multicolumn_fmt
							? convert_input_literals_to_js(menuitemsfiltered[j].multicolumn_fmt) : multicolumn_format;
						item_singlecolumn_format = menuitemsfiltered[j].singlecolumn_fmt
							? convert_input_literals_to_js(menuitemsfiltered[j].singlecolumn_fmt) : singlecolumn_format;

						if (menuitemsfiltered[i].input.length > keylimit) {
							keylimit = menuitemsfiltered[i].input.length;
						}
						write(multicolumn_separator);
						console.add_hotspot(menuitemsfiltered[j].input);
						printf(item_multicolumn_format,
							menuitemsfiltered[j].input.toString().toUpperCase(),
							menuitemsfiltered[j].title,
							cost
						);
					} else {
						write(multicolumn_separator);
					}
					j++;
				}
				console.crlf();
			}

			// synchronize with node database, checks for messages,
			// interruption, etc. (AKA node_sync), clears the current console
			// line if there's a message to print when clearline is true.
			bbs.node_sync();

			// print a mnemonics string, command keys highlighted with
			// tilde (~) characters; prints "Which or Quit"
			console.mnemonics(which);
		}

		keyin = console.getkey(K_NONE);
		// if there are 2 or 3 letter input options, then give the user a chance to type the 2nd and third chars
		// unfortunately, this will introduce latency for the single key options
		// and if the typist is slow, they are going to get the single key menu option selected instead if one exists
		if (keylimit > 1) {
			// check for possible second digit (on 2 and 3 char inputs)
			var keyin2 = console.inkey(K_NONE, 1000);
			keyin = keyin + keyin2;
		} else if (keylimit == 3) {
			// check for possible third digit
			var keyin3 = console.inkey(K_NONE, 1000);
			keyin = keyin + keyin3;
		}
		keyin = keyin.toLowerCase();

		if (keyin) {
			// q for quit
			if (keyin == "q") {
				console.clear();
				break;
			}

			menuitemsfiltered.some(function (menuitemfiltered) {
				var menutarget = menuitemfiltered.target.toLowerCase();
				var menuinput = menuitemfiltered.input.toString().toLowerCase();

				if (menuinput == keyin) {
					switch (menuitemfiltered.type) {
						// custom menu, defined in xtrnmenu.json
						case 'custommenu':
							external_section_menu_custom(menutarget);
							return true;
						// external program section
						case 'xtrnmenu':
							var n;
							var menufound = false;
							for (n in xtrn_area.sec_list) {
								if (xtrn_area.sec_list[n].code.toLowerCase() == menutarget) {
									menufound = true;
									external_program_menu(n);
									return true;
								}
							}

							if (!menufound) {
								doerror(options.custom_menu_not_found_msg.replace('%MENUID%', menutarget));
							}
							break;
						// external program
						case 'xtrnprog':
							// run the external program
							if (typeof xtrn_area.prog[menutarget] !== "undefined") {
								exec_xtrn(xtrn_area.prog[menutarget]);
								return true;
							} else {
								doerror(options.custom_menu_program_not_found_msg.replace('%PROGRAMID%', menutarget));
							}
							break;
					} //switch
				} // if menu item matched keyin
			}); // foreach menu item
		} // if keyin
	} // main bbs.online loop
}


// Display error message to console and save to log
function doerror(msg)
{
	console.crlf();
	log(LOG_ERR, msg);
	console.putmsg('\x01+\x01h\x01r' +msg + ". The sysop has been notified." + '\x01-\r\n');
}

