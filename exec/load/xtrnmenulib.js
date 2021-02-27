/**
 * Custom External Program Menu Library for Custom External Program Menus
 * by Michael Long mlong  innerrealmbbs.us
 *
 * This provides common functionality for retrieving menus used by
 * the loadable module xtrnmenu.js and by the web interface
 * 099-xtrnmenu-games.xjs
 */

"use strict";

require("sbbsdefs.js", "K_NONE");

/* text.dat entries */
require("text.js", "XtrnProgLstFmt");

var MENU_LOADED = true;

function ExternalMenus() {
	this.options = {};
	this.xtrn_custommenu_options = {};
	this.menuconfig = {};
	
	this.getOptions();
	this.getMenuConfig();
}

ExternalMenus.prototype.getMenuConfig = function() {
	var config_file = new File(system.ctrl_dir + "xtrnmenu.cfg");
	var config_src;
	this.menuconfig = undefined;
	if (config_file.open('r+')) {
		config_src = config_file.read();
		config_file.close();
	}
	
	if (typeof config_src !== "undefined") {
		this.menuconfig = JSON.parse(config_src.toString());
	}
}

ExternalMenus.prototype.getOptions = function(menutype, menuid) {
	if (typeof menutype === "undefined") {
		menutype = 'custommenu';
	}

	// Get xtrn_sec options from modopts.ini [xtrn_sec]
	if ((this.options = load({}, "modopts.js", "xtrn_sec")) == null) {
		this.options = { multicolumn: true, sort: false };
	}

	// Get xtrn_custommenu options from modopts.d/xtrnmenu.ini [xtrnmenu]
	if ((this.xtrn_custommenu_options = load({}, "modopts.js", "xtrnmenu")) == null) {
		this.xtrn_custommenu_options = { };
	}

	// We start with the option from xtrnmenu. If it doesn't exist,
	// we use the option from xtrn_sec (if applicable). And if that doesn't exist, we use the 
	// default (system or in code).

	if (typeof this.xtrn_custommenu_options.multicolumn !== "undefined") {
		this.options.multicolumn = this.xtrn_custommenu_options.multicolumn;
	} else if (typeof this.options.multicolumn === "undefined") {
		this.options.multicolumn = true;
	}

	if (typeof this.xtrn_custommenu_options.multicolumn_separator !== "undefined") {
		this.options.multicolumn_separator = this.xtrn_custommenu_options.multicolumn_separator;
	} else if (typeof this.options.multicolumn_seperator === "undefined") {
		this.options.multicolumn_separator = " ";
	}

	if (typeof this.xtrn_custommenu_options.multicolumn_fmt !== "undefined") {
		this.options.multicolumn_fmt = this.xtrn_custommenu_options.multicolumn_fmt;
	} else if (typeof this.options.multicolumn_fmt === "undefined") {
		// cannot default to xtrn_sec multicolumn_fmt due to use of %u instead of %s
		this.options.multicolumn_fmt = "\x01h\x01c%3s \xb3 \x01n\x01c%-32.32s\x01h ";
	}

	if (typeof this.xtrn_custommenu_options.singlecolumn_fmt !== "undefined") {
		this.options.singlecolumn_fmt = this.xtrn_custommenu_options.singlecolumn_fmt;
	} else if (typeof this.options.singlecolumn_fmt === "undefined") {
		// cannot default to xtrn_sec multicolumn_fmt due to use of %u instead of %s
		this.options.singlecolumn_fmt = "\x01h\x01c%3s \xb3 \x01n\x01c%s\x01h ";
	}

	if (typeof this.xtrn_custommenu_options.multicolumn_fmt_inverse !== "undefined") {
		this.options.multicolumn_fmt_inverse = this.xtrn_custommenu_options.multicolumn_fmt_inverse;
	} else {
		// cannot default to xtrn_sec multicolumn_fmt due to use of %u instead of %s
		this.options.multicolumn_fmt_inverse = "\x01h\x01c%3s \xb3 \x01n\x016\x01w\x01h%-32.32s \x01n\x01h";
	}

	if (typeof this.xtrn_custommenu_options.singlecolumn_fmt_inverse !== "undefined") {
		this.options.singlecolumn_fmt_inverse = this.xtrn_custommenu_options.singlecolumn_fmt_inverse;
	} else {
		// cannot default to xtrn_sec multicolumn_fmt due to use of %u instead of %s
		this.options.singlecolumn_fmt_inverse = "\x01h\x01c%3s \xb3 \x01n\x016\x01w\x01h%s \x01n\x01h";
	}

	if (typeof this.xtrn_custommenu_options.return_multicolumn_fmt !== "undefined") {
		this.options.return_multicolumn_fmt = this.xtrn_custommenu_options.return_multicolumn_fmt;
	} else if (typeof this.options.return_multicolumn_fmt === "undefined") {
		// cannot default to xtrn_sec multicolumn_fmt due to use of %u instead of %s
		this.options.return_multicolumn_fmt = this.options.multicolumn_fmt;
	}

	if (typeof this.xtrn_custommenu_options.return_singlecolumn_fmt !== "undefined") {
		this.options.return_singlecolumn_fmt = this.xtrn_custommenu_options.return_singlecolumn_fmt;
	} else if (typeof this.options.return_singlecolumn_fmt === "undefined") {
		// cannot default to xtrn_sec singlecolumn_fmt due to use of %u instead of %s
		this.options.return_singlecolumn_fmt = this.options.singlecolumn_fmt;
	}

	if (typeof this.xtrn_custommenu_options.return_multicolumn_fmt_inverse !== "undefined") {
		this.options.return_multicolumn_fmt_inverse = this.xtrn_custommenu_options.return_multicolumn_fmt_inverse;
	} else if (typeof this.options.return_multicolumn_fmt_inverse === "undefined") {
		this.options.return_multicolumn_fmt_inverse = this.options.multicolumn_fmt_inverse;
	}

	if (typeof this.xtrn_custommenu_options.return_singlecolumn_fmt_inverse !== "undefined") {
		this.options.return_singlecolumn_fmt_inverse = this.xtrn_custommenu_options.return_singlecolumn_fmt_inverse;
	} else if (typeof this.options.return_singlecolumn_fmt_inverse === "undefined") {
		this.options.return_singlecolumn_fmt_inverse = this.options.singlecolumn_fmt_inverse;
	}

	this.options.multicolumn_fmt_special = typeof this.xtrn_custommenu_options.multicolumn_fmt_special !== "undefined"
		? this.xtrn_custommenu_options.multicolumn_fmt_special : this.options.multicolumn_fmt;

	this.options.singlecolumn_fmt_special = typeof this.xtrn_custommenu_options.singlecolumn_fmt_special !== "undefined"
		? this.xtrn_custommenu_options.singlecolumn_fmt_special : this.options.singlecolumn_fmt;

	this.options.multicolumn_fmt_special_inverse = typeof this.xtrn_custommenu_options.multicolumn_fmt_special_inverse !== "undefined"
		? this.xtrn_custommenu_options.multicolumn_fmt_special_inverse : this.options.multicolumn_fmt_inverse;

	this.options.singlecolumn_fmt_special_inverse = typeof this.xtrn_custommenu_options.singlecolumn_fmt_special_inverse !== "undefined"
		? this.xtrn_custommenu_options.singlecolumn_fmt_special_inverse : this.options.singlecolumn_fmt_inverse;

	this.options.return_singlecolumn_special_fmt = typeof this.xtrn_custommenu_options.return_singlecolumn_special_fmt
		? this.xtrn_custommenu_options.return_singlecolumn_special_fmt : this.options.singlecolumn_fmt;

	this.options.return_multicolumn_special_fmt = typeof this.xtrn_custommenu_options.return_multicolumn_special_fmt
		? this.xtrn_custommenu_options.return_multicolumn_special_fmt : this.options.multicolumn_fmt;

	this.options.return_singlecolumn_special_fmt_inverse = typeof this.xtrn_custommenu_options.return_singlecolumn_special_fmt_inverse
		? this.xtrn_custommenu_options.return_singlecolumn_special_fmt_inverse : this.options.singlecolumn_fmt_inverse;

	this.options.return_multicolumn_special_fmt_inverse = typeof this.xtrn_custommenu_options.return_multicolumn_special_fmt_inverse
		? this.xtrn_custommenu_options.return_multicolumn_special_fmt_inverse : this.options.multicolumn_fmt_inverse_inverse;

	if (typeof this.xtrn_custommenu_options.singlecolumn_margin !== "undefined") {
		this.options.singlecolumn_margin = this.xtrn_custommenu_options.singlecolumn_margin;
	} else if (typeof this.options.singlecolumn_margin === "undefined") {
		this.options.singlecolumn_margin = 7;
	}

	if (typeof this.xtrn_custommenu_options.restricted_user_msg !== "undefined") {
		this.options.restricted_user_msg = this.xtrn_custommenu_options.restricted_user_msg;
	} else if (typeof this.options.restricted_user_msg === "undefined") {
		this.options.restricted_user_msg = system.text(R_ExternalPrograms);
	}

	if (typeof this.xtrn_custommenu_options.no_programs_msg !== "undefined") {
		this.options.no_programs_msg = this.xtrn_custommenu_options.no_programs_msg;
	} else if (typeof this.options.no_programs_msg === "undefined") {
		this.options.no_programs_msg = system.text(NoXtrnPrograms);
	}

	if (typeof this.xtrn_custommenu_options.header_fmt !== "undefined") {
		this.options.header_fmt = this.xtrn_custommenu_options.header_fmt;
	} else if (typeof this.options.header_fmt === "undefined") {
		this.options.header_fmt = system.text(XtrnProgLstHdr);
	}

	if (typeof this.xtrn_custommenu_options.titles !== "undefined") {
		this.options.titles = this.xtrn_custommenu_options.titles;
	} else if (typeof this.options.title === "undefined") {
		this.options.titles = system.text(XtrnProgLstTitles);
	}

	if (typeof this.xtrn_custommenu_options.which !== "undefined") {
		this.options.which = this.xtrn_custommenu_options.which;
	} else if (typeof this.options.which === "undefined") {
		this.options.which = system.text(WhichXtrnProg);
	}

	if (typeof this.xtrn_custommenu_options.underline !== "undefined") {
		this.options.underline = this.xtrn_custommenu_options.underline;
	} else if (typeof this.options.underline === "undefined") {
		this.options.underline = system.text(XtrnProgLstUnderline);
	}

	if (typeof this.xtrn_custommenu_options.clear_screen !== "undefined") {
		this.options.clear_screen = this.xtrn_custommenu_options.clear_screen;
	} else if (typeof this.options.clear_screen === "undefined") {
		this.options.clear_screen = true;
	}

	if (typeof this.xtrn_custommenu_options.section_fmt !== "undefined") {
		this.options.section_fmt = this.xtrn_custommenu_options.section_fmt;
	} else if (typeof this.options.section_fmt === "undefined") {
		this.options.section_fmt = "\x01y\x01g%3d:\x01n\x01g %s";
	}

	if (typeof this.xtrn_custommenu_options.section_header_fmt !== "undefined") {
		this.options.section_header_fmt = this.xtrn_custommenu_options.section_header_fmt;
	} else if (typeof this.options.section_header_fmt === "undefined") {
		this.options.section_header_fmt = "\x01-\x01gSelect \x01hExternal Program Section\x01-\x01g:";
	}

	if (typeof this.xtrn_custommenu_options.section_which !== "undefined") {
		this.options.section_which = this.xtrn_custommenu_options.section_which;
	} else if (typeof this.options.section_which === "undefined") {
		this.options.section_which = "\r\n\x01-\x01gWhich, \x01w\x01h~Q\x01n\x01guit or [1]: \x01h";
	}

	if (typeof this.xtrn_custommenu_options.sort !== "undefined") {
		this.options.sort = this.xtrn_custommenu_options.sort;
	} else if (typeof this.options.sort === "undefined") {
		this.options.sort = false;
	}

	this.options.custom_menu_not_found_msg = (typeof this.options.custom_menu_not_found_msg !== "undefined")
		? this.options.custom_menu_not_found_msg : "Menu %MENUID% not found";
	
	this.options.custom_menu_program_not_found_msg = (typeof this.options.custom_menu_program_not_found_msg  !== "undefined")
		? this.options.custom_menu_program_not_found_msg  : "Menu %MENUID% not found";

	this.options.use_xtrn_sec = (typeof this.xtrn_custommenu_options.use_xtrn_sec !== "undefined")
		? this.xtrn_custommenu_options.use_xtrn_sec : false;
	
	this.options.custom_quit_msg = (typeof this.xtrn_custommenu_options.custom_quit_msg !== "undefined")
		? this.xtrn_custommenu_options.custom_quit_msg : 'Quit';

	this.options.custom_return_msg = (typeof this.xtrn_custommenu_options.custom_return_msg !== "undefined")
		? this.xtrn_custommenu_options.custom_return_msg : 'Return to Previous Menu';

	// Allow overriding on a per-menu basis
	var menuoptions = load({}, "modopts.js", "xtrnmenu:" + menuid);
	if ((typeof menuid !== "undefined") && (menuoptions !== null)) {
		for (var m in menuoptions) {
			this.options[m] = menuoptions[m];
		}
	}

	this.options.custom_logoff_msg = (typeof this.xtrn_custommenu_options.custom_logoff_msg !== "undefined")
		? this.xtrn_custommenu_options.custom_logoff_msg : 'Logoff';
	
	this.options.json_enabled = (typeof this.xtrn_custommenu_options.json_enabled !== undefined)
		? this.xtrn_custommenu_options.json_enabled : false;
	this.options.json_host = (typeof this.xtrn_custommenu_options.json_host !== undefined)
		? this.xtrn_custommenu_options.json_host : "localhost";
	this.options.json_port = (typeof this.xtrn_custommenu_options.json_port !== undefined)
		? this.xtrn_custommenu_options.json_port : 10088;
	
	this.options.blacklist_tracking_xtrncodes = (typeof this.xtrn_custommenu_options.blacklist_tracking_xtrncodes !== undefined)
		? this.xtrn_custommenu_options.blacklist_tracking_xtrncodes : "";
	this.options.blacklist_tracking_xtrnsec = (typeof this.xtrn_custommenu_options.blacklist_tracking_xtrnsec !== undefined)
		? this.xtrn_custommenu_options.blacklist_tracking_xtrnsec : "";
	
	this.options.custom = this.xtrn_custommenu_options;

	return this.options;
}

// return a custom menu object
ExternalMenus.prototype.getMenu = function(menuid) {
	// grab the specified menu, or get the main menu
	if ((typeof menuid === "undefined") || !menuid) {
		menuid = "main";
	} else {
		menuid = menuid.toLowerCase();
	}
	
	var menu;
	
	if ((typeof this.menuconfig !== "undefined") && (typeof this.menuconfig.menus !== "undefined")) {
		this.menuconfig.menus.some(function (indmenu) {
			if (indmenu.id.toLowerCase() == menuid) {
				menu = indmenu;
			}
		});
	}
	
	if (!menu && (menuid == "main")) {
		// no custom menus defined, make one to mimic old behavior
		
		var menuitems = [];
		
		var i;
		xtrn_area.sec_list.forEach(function (sec) {
			if (sec.can_access) {
				menuitems.push({
					input: i,
					target: sec.code,
					type: "xtrnmenu",
					title: sec.name,
					access_string: sec.ars
				});
				i++;
			}
		});
		
		menu = {
			id: "main",
			title: "Main Menu",
			items: menuitems
		};
	}
	return menu;
}

// return a section menu object (stock synchronet external door section)
ExternalMenus.prototype.getSectionMenu = function(menuid) {
	
	// grab the specified menu, or get the main menu
	if ((typeof menuid === "undefined") || !menuid) {
		return false;
	}
	
	var menuitems = [];
	var title;
	
	var menu = {
		id: menuid,
		title: null,
		items: []
	}
	
	xtrn_area.sec_list.some(function (sec) {
		if (sec.code.toLowerCase() == menuid.toLowerCase()) {
			menu.title = sec.name;
			
			if (!sec.can_access || sec.prog_list.length < 1) {
				return false;
			}
			
			sec.prog_list.some(function (prog) {
				menuitems.push({
					input : null,
					target: prog.code,
					title: prog.name,
					type: 'xtrnprog',
					access_string: prog.ars,
					cost: prog.cost
				});
			});
			
			if (menuitems.length > 0) {
				menu.items = menuitems;
			}
			return;
		}
	});
	
	return menu;
}

// Sort the menu items according to options
ExternalMenus.prototype.getSortedItems = function(menuobj) {
	var sort_type;
	
	if ((typeof menuobj.sort_type !== "undefined") && menuobj.sort_type) {
		sort_type = menuobj.sort_type; // "name" or "key"
	} else {
		sort_type = this.options.sort; // bool
	}
	
	// first, build a new menu with only options they have access to
	var menuitemsfiltered = [];
	for (i in menuobj.items) {
		switch (menuobj.items[i].type) {
			case 'xtrnmenu':
				for (j in xtrn_area.sec_list) {
					if (xtrn_area.sec_list[j].code.toUpperCase() == menuobj.items[i].target.toUpperCase()) {
						if (xtrn_area.sec_list[j].can_access) {
							menuitemsfiltered.push(menuobj.items[i]);
						}
					}
				}
				break;
			
			case 'xtrnprog':
				for (j in xtrn_area.sec_list) {
					for (var k in xtrn_area.sec_list[j].prog_list) {
						if (xtrn_area.sec_list[j].prog_list[k].code.toUpperCase() == menuobj.items[i].target.toUpperCase()) {
							if (xtrn_area.sec_list[j].prog_list[k].can_access) {
								menuitemsfiltered.push(menuobj.items[i]);
							}
						}
					}
				}
				break;
			
			case 'custommenu':
			default:
				if ((typeof menuobj.items[i].access_string === "undefined") || !menuobj.items[i].access_string) {
					// no access string defined, everyone gets access
					menuitemsfiltered.push(menuobj.items[i]);
				} else {
					if (user.compare_ars(menuobj.items[i].access_string)) {
						// they have access
						menuitemsfiltered.push(menuobj.items[i]);
					}
				}
				break;
		}
	}
	
	// if no custom input keys are specified for an input, then assign a numeric input
	// this would mimic the built-in external section menu functionality
	// but this is only assigned on sort_type key because if the sort type is for
	// titles, we need to sort by title first before assigning the sequential numbers
	var sortind = 0;
	// make sure we only use the next available number for automatic assignment
	for (i in menuitemsfiltered) {
		if (!isNaN(menuitemsfiltered[i].input)) {
			if (menuitemsfiltered[i].input > sortind) {
				sortind = menuitemsfiltered[i].input;
			}
		}
	}
	sortind++;
	
	if (sort_type == "key") {
		for (i in menuitemsfiltered) {
			if (!menuitemsfiltered[i].input) {
				menuitemsfiltered[i].input = sortind;
				sortind++;
			}
		}
	}
	
	if (sort_type) {
		switch (sort_type) {
			case "key":
				menuitemsfiltered.sort(this.sort_by_input);
				break;
			default:
			case "title":
			case "name":
				menuitemsfiltered.sort(this.sort_by_title);
				break;
		}
	}
	
	// if this is a sort by title and the key is empty, it will be assigned the next available number
	// this is to support auto-generated inputs like is done on the built-in section menus
	// however, to keep the numeric keys in order, the numbers could not be pre-assigned like it done
	// on the sort_type key
	for (i in menuitemsfiltered) {
		if ((sort_type !== "key") && !menuitemsfiltered[i].input) {
			menuitemsfiltered[i].input = sortind;
			sortind++;
		}
	}
	
	return menuitemsfiltered;
}

// Original sort function used by external_program_menu and external_section_menu
ExternalMenus.prototype.sort_by_name = function(a, b) {
	if(a.name.toLowerCase()>b.name.toLowerCase()) return 1;
	if(a.name.toLowerCase()<b.name.toLowerCase()) return -1;
	return 0;
}

// Sort by title - used by external_section_menu_custom
ExternalMenus.prototype.sort_by_title = function(a, b) {
	if (a.title.toLowerCase() < b.title.toLowerCase()) {
		return -1;
	}
	if (a.title.toLowerCase() > b.title.toLowerCase()) {
		return 1;
	}
	return 0;
}

// Sort by input key - used by external_section_menu_custom
ExternalMenus.prototype.sort_by_input = function(a, b) {
	if (a.input.toString().toLowerCase() < b.input.toString().toLowerCase()) {
		return -1;
	}
	if (a.input.toString().toLowerCase() > b.input.toString().toLowerCase()) {
		return 1;
	}
	return 0;
}

// Handle json update
ExternalMenus.prototype.processUpdate = function(update) {
	log(ERROR, "Unhandled JSON DB packet: " + JSON.stringify(update));
}

// Sort for special menus, by value numerically desc
ExternalMenus.prototype.sort_special = function(obj) {
	var keys = [];
	for(var key in obj) {
		keys.push(key);
	}
	return keys.sort(function(a,b) { return obj[b]-obj[a]});
}

// return menu object for special menu (most recent, etc.)
ExternalMenus.prototype.getSpecial = function(menutype, title, itemcount) {
	var options = this.getOptions('custommenu', menutype);

	var menu;
	var menuitems = [];
	
	if (itemcount === undefined) {
		itemcount = 0;
	}
 	
	if (menutype === undefined) {
		log(LOG_DEBUG, "xtrnmenulib: getSpecial called without menutype");
		return false;
	}
	
	if (!this.options.json_enabled) {
		log(LOG_DEBUG, "xtrnmenulib: Skipping " + menutype + " because JSON is not enabled");
		return false;
	}

	try {
		require("json-client.js", "JSONClient");
		var jsonClient = new JSONClient(this.options.json_host, this.options.json_port);
		jsonClient.callback = this.processUpdate;
	} catch (e) {
		writeln("NP" + e);
		log(LOG_ERR, "xtrnmenulib: Could not initialize JSON database so special menu is now disabled: " + e);
		return false;
	}

	var sortedItems = [];
	var jsonData = [];
	switch (menutype) {
		// game keys sorted by most recent descending
		case 'recentall':
			jsonData = jsonClient.read("xtrnmenu", "launchstart", 1);
			break;
		case 'recentuser':
			jsonData = jsonClient.read("xtrnmenu", "launchstart_user_" + user.alias, 1);
			break;
		case 'mostlaunchedall':
			jsonData = jsonClient.read("xtrnmenu", "launchnum", 1);
			break;
		case 'mostlauncheduser':
			jsonData = jsonClient.read("xtrnmenu", "launchnum_user_" + user.alias, 1);
			break;
		case 'longestrunall':
			jsonData = jsonClient.read("xtrnmenu", "timeran", 1);
			break;
		case 'longestrunuser':
			jsonData = jsonClient.read("xtrnmenu", "timeran_user_" + user.alias, 1);
			break;
		default:
			log(LOG_ERR, "xtrnmenulib: Unknown special menu type: " + menutype);
			return false;
	}
	
	if (!jsonData) {
		jsonData = {};
	}
	sortedItems = this.sort_special(jsonData);
	
	var i = 1;
	for (var d=0; d<sortedItems.length; d++) {
		var doorid = sortedItems[d];
		for (var e in xtrn_area.prog) {
			if (xtrn_area.prog[e].code.toLowerCase() == doorid.toLowerCase()) {
				var stats;
				switch (menutype) {
					case 'longestrunall':
					case 'longestrunuser':
						var seconds = jsonData[doorid];
						var hours = ~~(seconds/3600);
						var mins = ~~((seconds % 3600) / 60);
						var secs = ~~seconds % 60;
						stats = ("00"+hours).slice(-2) + ":" + ("00"+mins).slice(-2) + ":" + ("00"+secs).slice(-2);
						break;
					case 'recentall':
					case 'recentuser':
						var elapsedtime = (time() - jsonData[doorid])*1000;
						var msPerMin = 60000;
						var msPerHour = msPerMin * 60;
						var msPerDay = msPerHour * 24;
						var msPerMon = msPerDay * 30;
						var msPerYear = msPerDay * 365;
						if (elapsedtime < msPerMin) {
							stats = Math.round(elapsedtime/1000) + ' sec ago';
						} else if (elapsedtime < msPerHour) {
							stats = Math.round(elapsedtime/msPerMin) + ' min ago';
						} else if (elapsedtime < msPerDay) {
							var stat = Math.round(elapsedtime/msPerHour)
							stats = Math.round(elapsedtime/msPerHour) + ' hr ago';
						} else if (elapsedtime < msPerMon) {
							stats = Math.round(elapsedtime/msPerDay) + ' day ago';
						} else if (elapsedtime < msPerYear) {
							stats = Math.round(elapsedtime/msPerMonth) + ' mon ago';
						} else {
							stats = Math.round(elapsedtime/msPerYear) + ' yr ago';
						}
						break;
					default:
						stats = jsonData[doorid].toString().replace(/\B(?=(\d{3})+(?!\d))/g, ",");;
						break;
				}
				menuitems.push({
					input: i,
					target: xtrn_area.prog[e].code,
					title: xtrn_area.prog[e].name,
					type: 'xtrnprog',
					access_string: xtrn_area.prog[e].ars,
					cost: xtrn_area.prog[e].cost,
					stats: stats,
				});
				i++;
				break;
			}
		}
		
		if ((itemcount > 0) && ((d + 1) >= itemcount)) {
			break;
		}
	}
	if (menuitems.length > 0) {
		menu = {
			id: menutype,
			title: title,
			items: menuitems,
		};
	}

	return menu;
}

// return menu object for user's favorites
ExternalMenus.prototype.getFavorites = function(title, itemcount) {
	var menu;
	var menuitems = [];
	
	if (itemcount === undefined) {
		itemcount = 0;
	}
	
	if (!this.options.json_enabled) {
		log(LOG_DEBUG, "xtrnmenulib: Skipping favorites because JSON is not enabled");
		return false;
	}
	
	try {
		require("json-client.js", "JSONClient");
		var jsonClient = new JSONClient(this.options.json_host, this.options.json_port);
		jsonClient.callback = this.processUpdate;
	} catch (e) {
		log(LOG_ERR, "xtrnmenulib: Could not initialize JSON database so favorites is now disabled: " + e);
		return false;
	}
	
	var sortedItems = [];
	var jsonData = jsonClient.read("xtrnmenu", "favorites_" + user.alias, 1);
	
	if (!jsonData) {
		jsonData = {};
	}
	
	var i = 1;
	for (var d=0; d<jsonData.length; d++) {
		for (var e in xtrn_area.prog) {
			if (xtrn_area.prog[e].code.toLowerCase() == jsonData[d].toLowerCase()) {
				var stats;
				
				menuitems.push({
					input: i,
					target: xtrn_area.prog[e].code,
					title: xtrn_area.prog[e].name,
					type: 'xtrnprog',
					access_string: xtrn_area.prog[e].ars,
					cost: xtrn_area.prog[e].cost,
				});
				i++;
				break;
			}
		}
		
		if ((itemcount > 0) && ((d + 1) >= itemcount)) {
			break;
		}
	}

	menu = {
		id: 'favorites',
		title: title !== undefined ? title : 'Favorites',
		items: menuitems,
	};
	
	return menu;
}