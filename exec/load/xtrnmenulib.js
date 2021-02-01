/**
 * Custom External Program Menu Library for Custom External Program Menus
 * by Michael Long mlong  innerrealmbbs.us
 * 
 * This provides common functionality for retrieving menus used by 
 * the loadable module xtrnmenu.js and by the web interface
 * 099-xtrnmenu-games.xjs
 */

"use strict";

load("sbbsdefs.js", "K_NONE");

/* text.dat entries */
require("text.js", "XtrnProgLstFmt");

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

	// Get xtrn_custommenu options from modopts.ini [xtrn_custommenu]
	if ((this.xtrn_custommenu_options = load({}, "modopts.js", "xtrnmenu")) == null) {
		this.xtrn_custommenu_options = { };
	}

	// in all cases, we start with the xtrn_sec options as the base and set the defaults
	if (this.options.multicolumn === undefined)
		this.options.multicolumn = true;

	if (this.options.multicolumn_separator === undefined)
		this.options.multicolumn_separator = " ";

	if (this.options.multicolumn_fmt === undefined)
		this.options.multicolumn_fmt = system.text(XtrnProgLstFmt);
	
	if (this.options.singlecolumn_fmt === undefined)
		this.options.singlecolumn_fmt = "\x01h\x01c%3u \xb3 \x01n\x01c%s\x01h ";

	if (this.options.singlecolumn_margin == undefined)
		this.options.singlecolumn_margin = 7;

	if (typeof bbs !== "undefined") {
		if (this.options.singlecolumn_height == undefined)
			this.options.singlecolumn_height = console.screen_rows - this.options.singlecolumn_margin;

		// override and turn off multicolumn if terminal width is less than 80
		if (console.screen_columns < 80)
			options.multicolumn = false;
	}
	
	if (this.options.restricted_user_msg === undefined)
		this.options.restricted_user_msg = system.text(R_ExternalPrograms);
	
	if (this.options.no_programs_msg === undefined) 
		this.options.no_programs_msg =  system.text(NoXtrnPrograms);
	
	if (this.options.header_fmt === undefined)
		this.options.header_fmt = system.text(XtrnProgLstHdr);

	if (this.options.titles === undefined)
		this.options.titles = system.text(XtrnProgLstTitles);

	if (this.options.underline === undefined)
		this.options.underline = system.text(XtrnProgLstUnderline);

	if (this.options.which === undefined) 
		this.options.which = system.text(WhichXtrnProg);

	if (this.options.clear_screen === undefined)
		this.options.clear_screen = true;

	if (this.options.section_fmt === undefined)
		this.options.section_fmt = "\x01y\x01g%3d:\x01n\x01g %s"

	if (this.options.section_header_fmt === undefined)
		this.options.section_header_fmt = "\x01-\x01gSelect \x01hExternal Program Section\x01-\x01g:"

	if (this.options.section_which === undefined)
		this.options.section_which = "\r\n\x01-\x01gWhich, \x01w\x01h~Q\x01n\x01guit or [1]: \x01h"

	// if its a custom menu, then override with custommenu_options
	if (menutype == 'custommenu') {
		if (typeof this.xtrn_custommenu_options.multicolumn_fmt !== "undefined") {
			this.options.multicolumn_fmt = this.xtrn_custommenu_options.multicolumn_fmt;
		} else {
			// cannot default to xtrn_sec multicolumn_fmt due to use of %u instead of %s
			this.options.multicolumn_fmt = "\x01h\x01c%3s \xb3 \x01n\x01c%-32.32s\x01h ";
		}

		if (typeof this.xtrn_custommenu_options.singlecolumn_fmt !== "undefined") {
			this.options.singlecolumn_fmt = this.xtrn_custommenu_options.singlecolumn_fmt;
		} else {
			// cannot default to xtrn_sec multicolumn_fmt due to use of %u instead of %s
			this.options.singlecolumn_fmt = "\x01h\x01c%3s \xb3 \x01n\x01c%s\x01h ";
		}

		this.options.header_fmt = (typeof this.xtrn_custommenu_options.header_fmt !== "undefined") 
			? this.xtrn_custommenu_options.header_fmt : this.options.header_fmt;

		this.options.titles = (typeof this.xtrn_custommenu_options.titles !== "undefined")
			? this.xtrn_custommenu_options.titles : this.options.titles;

		this.options.which = (typeof this.xtrn_custommenu_options.which !== "undefined")
			? this.xtrn_custommenu_options.which : this.options.which;

		this.options.underline = (typeof this.xtrn_custommenu_options.underline !== "undefined")
			? this.xtrn_custommenu_options.underline : this.options.underline;

		this.options.multicolumn_separator = (typeof this.xtrn_custommenu_options.multicolumn_separator !== "undefined")
			? this.xtrn_custommenu_options.multicolumn_separator : this.options.multicolumn_separator;

		this.options.multicolumn = (typeof this.xtrn_custommenu_options.multicolumn !== "undefined")
			? this.xtrn_custommenu_options.multicolumn : this.options.multicolumn;

		this.options.sort = (typeof this.xtrn_custommenu_options.sort !== "undefined")
			? this.xtrn_custommenu_options.sort : this.options.sort;

		this.options.clear_screen = (typeof this.xtrn_custommenu_options.clear_screen !== "undefined")
			? this.xtrn_custommenu_options.clear_screen : this.options.clear_screen;

		this.options.singlecolumn_margin = (typeof this.xtrn_custommenu_options.singlecolumn_margin !== "undefined")
			? this.xtrn_custommenu_options.singlecolumn_margin : this.options.singlecolumn_margin;

		if (typeof bbs !== "undefined") {
			this.options.singlecolumn_height = (typeof this.xtrn_custommenu_options.singlecolumn_height !== "undefined")
				? this.xtrn_custommenu_options.singlecolumn_height : this.options.singlecolumn_height;
		}

		// no need to override restricted_user_msg or no_programs_msg - these
		// will be the same for both types of menus
		
		// Allow overriding on a per-menu basis
		var menuoptions = load({}, "modopts.js", "xtrnmenu:" + menuid);
		if ((typeof menuid !== "undefined") && (menuoptions != null)) {
			for (var m in menuoptions) {
				this.options[m] = menuoptions[m];
			}
		}
	}
	
	// these options only apply to terminals/consoles
	
	// the intention is to obtain all the mod_opts options for xtrn_sec, and
	// override if a custom menu global setting is set

	//// The following are used for the enhanced custom menu functionality
	if (this.options.custom_menu_not_found_msg === undefined) {
		this.options.custom_menu_not_found_msg = "Menu %MENUID% not found";
	}
	
	if (this.options.custom_menu_program_not_found_msg === undefined) {
		this.options.custom_menu_program_not_found_msg = "Program %PROGRAMID% not found";
	}
	
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
					"input": i,
					"target": sec.code,
					"type": "xtrnmenu",
					"title": sec.name,
					"access_string": sec.ars
				});
				i++;
			}
		});
		
		menu = {
			"id": "main",
			"title": "Main Menu",
			"items": menuitems
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
	var menu, title;
	
	xtrn_area.sec_list.some(function (sec) {
		
		if (sec.code.toLowerCase() == menuid.toLowerCase()) {
			title = sec.name;

			if (!sec.can_access || sec.prog_list.length < 1) {
				return false;
			}
			
			var i = 1;
			sec.prog_list.some(function (prog) {
				menuitems.push({
					'input' : i,
					'target': prog.code,
					'title': prog.name,
					'type': 'xtrnprog',
					'access_string': prog.ars,
					'cost': prog.cost
				});
				i++;
			});
			
			if (menuitems.length > 0) {
				menu = {
					'id': menuid,
					'title': title,
					'items': menuitems,
				};
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
