// Synchronet Custom External Menus Library

"use strict";

load("sbbsdefs.js");

/* text.dat entries */
load("text.js");

function ExternalMenus() {
	this.options = {};
	this.menuconfig = {};
	
	this.getOptions();
	this.getMenuConfig();
}

ExternalMenus.prototype.getMenuConfig = function() {
	var config_file = new File(file_cfgname(system.ctrl_dir, "xtrnmenu.json"));
	if (!config_file.open('r+')) {
		this.menuconfig = undefined;
	}
	
	var config_src = config_file.read();
	config_file.close();
	
	this.menuconfig = JSON.parse(config_src.toString());
}

ExternalMenus.prototype.getOptions = function() {
	// get defined options, setup defaults if not defined
	// To set options, add to modopts.ini [xtrn_sec]
	if ((this.options=load({}, "modopts.js", "xtrn_sec")) == null) {
		// default values if no options
		this.options = {multicolumn: true, sort: false};
	}
	
	// set default options on a per-option basis
	
	// these options only apply to terminals/consoles
	if (typeof bbs !== "undefined") {
		// multi column this.options
		if (this.options.multicolumn === undefined) {
			this.options.multicolumn = true;
		}
		
		if (this.options.multicolumn_separator === undefined) {
			this.options.multicolumn_separator = " ";
		} else {
			this.options.multicolumn_seperator = this.convert_input_literals_to_js(this.options.multicolumn_seperator);
		}
		
		if (this.options.multicolumn_fmt === undefined) {
			this.options.multicolumn_fmt = bbs.text(XtrnProgLstFmt);
		} else {
			this.options.multicolumn_fmt = this.convert_input_literals_to_js(this.options.multicolumn_fmt);
		}
		
		// single column this.options
		if (this.options.singlecolumn_fmt === undefined) {
			this.options.singlecolumn_fmt = "\u0001h\u0001c%3u \xb3 \u0001n\u0001c%s\u0001h ";
		} else {
			this.options.singlecolumn_fmt = this.convert_input_literals_to_js(this.options.singlecolumn_fmt);
		}
		
		if (this.options.singlecolumn_margin === undefined) {
			this.options.singlecolumn_margin = 7;
		}
		
		if (this.options.singlecolumn_height === undefined) {
			if (typeof console !== "undefined") {
				this.options.singlecolumn_height = console.screen_rows - this.options.singlecolumn_margin;
			}
			// will remain undefined if no console
		}
		
		if (this.options.header_fmt === undefined) {
			this.options.header_fmt = bbs.text(XtrnProgLstHdr);
		} else {
			this.options.header_fmt = this.convert_input_literals_to_js(this.options.header_fmt);
		}
		
		// legacy
		if ((this.options.titles !== undefined) && this.options.titles) {
			this.options.title_fmt = this.convert_input_literals_to_js(this.options.titles);
		}
		// new name
		if (this.options.title_fmt === undefined) {
			this.options.title_fmt = bbs.text(XtrnProgLstTitles);
		} else {
			this.options.title_fmt = this.convert_input_literals_to_js(this.options.title_fmt);
		}
		
		// legacy
		if ((this.options.underline !== undefined) && this.options.underline) {
			this.options.underline_fmt = this.convert_input_literals_to_js(this.options.underline);
		}
		
		// new name
		if (this.options.underline_fmt === undefined) {
			this.options.underline_fmt = bbs.text(XtrnProgLstUnderline);
		} else {
			this.options.underline_fmt = this.convert_input_literals_to_js(this.options.underline_fmt);
		}
		
		if (this.options.which === undefined) {
			this.options.which = bbs.text(WhichXtrnProg);
		} else {
			this.options.which = this.convert_input_literals_to_js(this.options.which);
		}
		
	}
	
	if (this.options.restricted_user_msg === undefined) {
		if (typeof bbs !== "undefined") {
			this.options.restricted_user_msg = bbs.text(R_ExternalPrograms);
		} else if (typeof this.options.restricted_user_msg_noconsole !== "undefined") {
			this.options.restricted_user_msg = this.options.restricted_user_msg_noconsole;
		} else {
			this.options.restricted_user_msg = "This program is not available.";
		}
	}
	
	if (this.options.no_programs_msg === undefined) {
		if (typeof bbs !== "undefined") {
			this.options.no_programs_msg = bbs.text(NoXtrnPrograms);
		} else if (typeof this.options.no_programs_msg_noconsole !== "undefined") {
			this.options.no_programs_msg = this.options.no_programs_msg_noconsole;
		} else {
			this.options.no_programs_msg = "No external program available.";
		}
	}
	
	// Sort the list of external programs alphabetically by name
	if (this.options.sort === undefined) {
		this.options.sort = false;
	}
	
	// Auto-execute the only available program when entering a program section
	if (this.options.autoexec === undefined) {
		this.options.autoexec = false;
	}
	
	// clear screen on menu display
	if (this.options.clear_screen === undefined) {
		this.options.clear_screen = true;
	}
	
	// Clear the (remote) terminal screen before executing a program
	if (this.options.clear_screen_on_exec === undefined) {
		this.options.clear_screen_on_exec = false;
	}
	
	// If displaying CODE.ans file, prompt the user to continue to enter the game or not
	if (this.options.prompt_on_info === undefined) {
		this.options.prompt_on_info = false;
	}
	
	if (this.options.prompt_on_info_fmt === undefined) {
		this.options.prompt_on_info_fmt = "\r\n\u0001n\u0001cDo you wish to continue";
	} else {
		this.options.prompt_on_info_fmt = this.convert_input_literals_to_js(this.options.prompt_on_info_fmt);
	}
	
	//// The following are used for the enhanced custom menu functionality
	
	if (this.options.custom_menu_not_found_msg === undefined) {
		this.options.custom_menu_not_found_msg = "Menu %MENUID% not found";
	}
	
	if (this.options.custom_menu_program_not_found_msg === undefined) {
		this.options.custom_menu_program_not_found_msg = "Program %PROGRAMID% not found";
	}
	
	// this is different than multicolumn_fmt because multicolumn_fmt has a built-in
	// printf for number and thus wouldn't be able to display alpha inputs
	if (this.options.custom_menu_multicolumn_fmt === undefined) {
		this.options.custom_menu_multicolumn_fmt = "\u0001h\u0001c%3.3s \u00b3 \u0001n\u0001c%-32.32s\u0001h ";
	} else {
		this.options.custom_menu_multicolumn_fmt = this.convert_input_literals_to_js(this.options.custom_menu_multicolumn_fmt);
	}
	
	// this is different than singlecolumn_fmt because singlecolumn_fmt has a built-in
	// printf for number and thus wouldn't be able to display alpha inputs
	if (this.options.custom_menu_singlecolumn_fmt === undefined) {
		this.options.custom_menu_singlecolumn_fmt = "\u0001h\u0001c%3.3s \u00b3 \u0001n\u0001c%s\u0001h ";
	} else {
		this.options.custom_menu_singlecolumn_fmt = this.convert_input_literals_to_js(this.options.custom_menu_singlecolumn_fmt);
	}
}

ExternalMenus.prototype.convert_input_literals_to_js = function(val) {
	if (val === undefined) {
		throw "incorrect configuration, please check modopts.ini";
	}
	return unescape(val.replace(/\\u/g, '%u')).replace(/\\r/g, "\r").replace(/\\n/g, "\n");
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
	
	this.menuconfig.menus.some(function (indmenu) {
		if (indmenu.id.toLowerCase() == menuid) {
			menu = indmenu;
		}
	});
	
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
					if (typeof bbs !== "undefined") {
						if (bbs.compare_ars(menuobj.items[i].access_string)) {
							// they have access
							menuitemsfiltered.push(menuobj.items[i]);
						}
					} else {
						if (user.compare_ars(menuobj.items[i].access_string)) {
							// they have access
							menuitemsfiltered.push(menuobj.items[i]);
						}
					}
				}
				break;
		}
	}
	
	// if no custom input keys are specified for an input, then assign a numeric input
	// this would mimic the built-in external section menu functionality
	// but this is only assigned on sort_type key because if the sort type is for
	// titles, we need to sort by title first before assigning the sequential numbers
	var sortind = 1;
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
