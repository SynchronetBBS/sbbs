/**
 * Xtrn Menu Mod
 * Custom External Program Menus
 * by Michael Long mlong  innerrealmbbs.us
 *
 * This is the loadable module that displays the custom external menus
 * in terminal server (telnet/rlogin/ssh)
 *
 * To jump to a specific menu, pass the ID as an argument
 *
 * To set options, add to modopts.ini [xtrnmenu]
 *
 * See instructions at http://wiki.synchro.net/module:xtrnmenu
 */

"use strict";

require("sbbsdefs.js", "K_NONE");

require("xtrnmenulib.js", "MENU_LOADED");

var options, xsec = -1;

//// Main
var ExternalMenus = new ExternalMenus();
const menuconfig = ExternalMenus.menuconfig;

var i,j;

if ((argv[0] == 'command') && (typeof argv[1] != "undefined")) {
	docommand(argv[1]);
} else if (argv[0] == 'gamesrv') {
	external_section_menu_custom('gamesrv');
} else {
	for (i in argv) {
		for (j in xtrn_area.sec_list) {
			if (argv[i].toLowerCase() == xtrn_area.sec_list[j].code)
				xsec = j;
		}
	}

	if (xsec > -1) {
		// if its the id of a standard section menu
		if (options.use_xtrn_sec) {
			// stock menu
			js.exec("xtrn_sec.js", {}, xsec);
		} else {
			external_section_menu_custom(xsec);
		}
	} else if (typeof argv[0] !== "undefined") {
		// if its not a section menu, assume it is a custom menu
		external_section_menu_custom(argv[0]);
	} else {
		// main custom menu
		external_section_menu_custom();
	}
}

// Runs custom commands, for gamesrv
function docommand(command)
{
	switch (command) {
		case 'checkmail':
			var lmsg = user.stats.mail_wait;
			if (lmsg > 0) {
				console.putmsg('\r\n\x01gYou have \x01c' + parseInt(lmsg) + ' \x01gMessages waiting');
			} else {
				console.putmsg('\r\n\x01gNo New Messages');
			}
			mswait(1500);
			console.clear();
			break;
		case 'feedback':
			var subject;
			console.putmsg('\r\n\x01\gPlease choose \x01wYes \x01gto forward to netmail!\r\n\r\n');
			bbs.email(1, subject = "Game Server Feedback\r\n");
			console.putmsg('Thank you for your Feedback, @SYSOP@ will get back to you ASAP!\r\n\r\n');
			break;
		case 'prefs':
			bbs.user_config();
			break;
		case 'sysop':
			require("str_cmds.js", "str_cmds");
			console.putmsg("\r\n\x01gCommand: ");
			var commandstr;
			commandstr = console.getstr();
			str_cmds(commandstr);
			break;
		default:
			doerror("Unknown command " + command);
			break;
	}
}

// Renders the top-level external menu
function external_section_menu_custom(menuid)
{
	var i, menucheck, menuobj, item_multicolumn_fmt, item_singlecolumn_fmt,
		cost, multicolumn, menuitemsfiltered = [];
	var validkeys = []; // valid chars on menu selection
	var keymax = 0; // max integer allowed on menu selection

	var gamesrv = menuid == "gamesrv" ? true : false;
	if (gamesrv) {
		menuid = undefined;
	}

	var options = ExternalMenus.getOptions('custommenu', menuid);

	menuobj = ExternalMenus.getMenu(menuid);

	// Allow overriding auto-format on a per-menu basis
	var multicolumn_fmt = options.multicolumn_fmt;
	var singlecolumn_fmt = options.singlecolumn_fmt;

	while (bbs.online) {
		console.aborted = false;

		if (typeof menuobj === "undefined") {
			menuobj = ExternalMenus.getSectionMenu(menuid);
			if (typeof menuobj === "undefined") {
				doerror(options.custom_menu_not_found_msg.replace('%MENUID%', menuid));
				break;
			}
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

		system.node_list[bbs.node_num-1].aux = 0; /* aux is 0, only if at menu */
		bbs.node_action = NODE_XTRN;
		bbs.node_sync();

		menuitemsfiltered = ExternalMenus.getSortedItems(menuobj);

		if (!bbs.menu("xtrnmenu_head_" + menuid, P_NOERROR) && !bbs.menu("xtrnmenu_head", P_NOERROR)) {
			bbs.menu("xtrn_head", P_NOERROR);
		}

		// if file exists text/menu/xtrnmenu_(menuid).[rip|ans|mon|msg|asc], 
		// then display that, otherwise dynamiic
		if (!bbs.menu("xtrnmenu_" + menuid, P_NOERROR)) {

			// if no custom menu file in text/menu, create a dynamic one
			multicolumn = options.multicolumn && menuitemsfiltered.length > options.singlecolumn_height;
			printf(options.header_fmt, menuobj.title);
			if(options.titles.trimRight() != '')
				write(options.titles);
			if(multicolumn) {
				write(options.multicolumn_separator);
				if (options.titles.trimRight() != '')
					write(options.titles);
			}
			if(options.underline.trimRight() != '') {
				console.crlf();
				write(options.underline);
			}
			if(multicolumn) {
				write(options.multicolumn_separator);
				if (options.underline.trimRight() != '')
					write(options.underline);
			}
			console.crlf();

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

				console.add_hotspot(menuitemsfiltered[i].input.toString());

				validkeys.push(menuitemsfiltered[i].input.toString());
				var intCheck = Number(menuitemsfiltered[i].input);
				if (!intCheck.isNaN) {
					if (intCheck > keymax) {
						keymax = menuitemsfiltered[i].input;
					}
				}

				// allow overriding format on a per-item basis
				// great for featuring a specific game
				var checkkey = menuitemsfiltered[i].target + '-multicolumn_fmt';
				checkkey = checkkey.toLowerCase();
				item_multicolumn_fmt = (typeof options[checkkey] !== "undefined") ?
					options[checkkey] : options.multicolumn_fmt;

				checkkey = menuitemsfiltered[i].target + '-singlecolumn_fmt'
				checkkey = checkkey.toLowerCase();
				item_singlecolumn_fmt = (typeof options[checkkey] !== "undefined") ?
					options[checkkey] : options.singlecolumn_fmt;

				printf(multicolumn ? item_multicolumn_fmt : item_singlecolumn_fmt,
					menuitemsfiltered[i].input.toString().toUpperCase(),
					menuitemsfiltered[i].title,
					cost
				);

				if (multicolumn) {
					if ((typeof(menuitemsfiltered[j]) !== "undefined")) {
						validkeys.push(menuitemsfiltered[j].input.toString());

						var intCheck = Number(menuitemsfiltered[j].input);
						if (!intCheck.isNaN) {
							if (intCheck > keymax) {
								keymax = menuitemsfiltered[j].input;
							}
						}

						// allow overriding format on a per-item basis
						// great for featuring a specific game
						var checkkey = menuitemsfiltered[j].target + '-multicolumn_fmt';
						checkkey = checkkey.toLowerCase();
						item_multicolumn_fmt = (typeof options[checkkey] !== "undefined") ?
							options[checkkey] : options.multicolumn_fmt;

						checkkey = menuitemsfiltered[j].target + '-singlecolumn_fmt'
						checkkey = checkkey.toLowerCase();

						write(options.multicolumn_separator);
						console.add_hotspot(menuitemsfiltered[j].input.toString());
						printf(item_multicolumn_fmt,
							menuitemsfiltered[j].input.toString().toUpperCase(),
							menuitemsfiltered[j].title,
							cost
						);
					} else {
						write(options.multicolumn_separator);
					}
					j++;
				}
				console.crlf();
			}

			if (gamesrv) {
				if (!bbs.menu("xtrn_gamesrv_tail_" + menuid, P_NOERROR)) {
					bbs.menu("xtrn_gamesrv_tail", P_NOERROR);
				}
			}

			if (!bbs.menu("xtrnmenu_tail_" + menuid, P_NOERROR) && !bbs.menu("xtrnmenu_tail", P_NOERROR)) {
				bbs.menu("xtrn_tail", P_NOERROR);
			}

			bbs.node_sync();
			console.mnemonics(options.which);
		}

		validkeys.push('q');

		var keyin, keyin2;
		var maxkeylen = 0;
		var maxfirstkey = 0;
		var morekeys = [];
		var k;
		for (k in validkeys) {
			if (validkeys[k].length > maxkeylen) {
				maxkeylen = validkeys[k].length;
			}
			if (validkeys[k].length > 1) {
				morekeys.push(validkeys[k].toString().toLowerCase().substring(0,1));
			}
		}

		// get first key
		keyin = console.getkey();
		keyin = keyin.toString().toLowerCase();

		// The logic below is to make it not require enter for as
		// many items as possible

		// if max keys is 2 and they entered something that might have
		// a second digit/char, then get the key
		if (maxkeylen == 2) {
			if (morekeys.indexOf(keyin) !== -1) {
				write(keyin);
				keyin2 = console.getkey(); // either the second digit or enter
				if ((keyin2 != "\r") && (keyin2 != "\n") && (keyin2 != "\r\n")) {
					keyin = keyin + keyin2.toLowerCase();
				}
			}
		} else if (maxkeylen > 2) {
			// there there are more than 99 items, then just use getkeys 
			// for the rest
			write(keyin);
			keyin2 = console.getkeys(validkeys, keymax);
			keyin = keyin + keyin2.toLowerCase();
		}

		if (keyin) {
			if (keyin == 'q') {
				if (gamesrv && ('menuid' == 'main')) {
					bbs.logoff();
				} else {
					console.clear();
					return;
				}
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
							if (options.use_xtrn_sec) {
								js.exec("xtrn_sec.js", {}, menutarget);
							} else {
								external_section_menu_custom(menutarget);
							}
							return true;
						// external program
						case 'xtrnprog':
							// run the external program
							if (typeof xtrn_area.prog[menutarget] !== "undefined") {
								bbs.exec_xtrn(menutarget);
								return true;
							} else {
								doerror(options.custom_menu_program_not_found_msg.replace('%PROGRAMID%', menutarget));
							}
							break;
						case 'command':
							bbs.exec(menutarget);
							return true;
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

