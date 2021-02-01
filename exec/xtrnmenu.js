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

load("xtrnmenulib.js");

var options, xsec = -1;

//// Main
var ExternalMenus = new ExternalMenus();
const menuconfig = ExternalMenus.menuconfig;

{
	var i,j;
	for(i in argv) {
		for(j in xtrn_area.sec_list) {
			if(argv[i].toLowerCase()==xtrn_area.sec_list[j].code)
				xsec=j;
		}
	}
}
if (xsec > -1) {
	// if its the id of a standard section menu, send it to the 
	// stock menu
	js.exec("xtrn_sec.js", {}, xsec);
} else if (typeof argv[0] !== "undefined") {
	// if its not a section menu, assume it is a custom menu
	external_section_menu_custom(argv[0]);
} else {
	// main custom menu
	external_section_menu_custom();
}


// Renders the top-level external menu
function external_section_menu_custom(menuid)
{
	var i, menucheck, menuobj, item_multicolumn_fmt, item_singlecolumn_fmt,
        cost, multicolumn, menuitemsfiltered = [];
	var validkeys = []; // valid chars on menu selection
	var keymax = 0; // max integer allowed on menu selection

	var options = ExternalMenus.getOptions('custommenu', menuid);
	
	menuobj = ExternalMenus.getMenu(menuid);

	// Allow overriding auto-format on a per-menu basis
	var multicolumn_fmt = options.multicolumn_fmt;
	var singlecolumn_fmt = options.singlecolumn_fmt;

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

			if (!bbs.menu("xtrnmenu_tail_" + menuid, P_NOERROR) && !bbs.menu("xtrnmenu_tail", P_NOERROR)) {
				bbs.menu("xtrn_tail", P_NOERROR);
			}

			bbs.node_sync();
			console.mnemonics(options.which);
		}

		validkeys.push('q');
		keyin = console.getkeys(validkeys, keymax, K_NONE);
		keyin = keyin.toString().toLowerCase();

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
							js.exec("xtrn_sec.js", {}, menutarget);
							//js.exec(js.exec_dir + 'xtrn_sec.js ' + menutarget)
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

