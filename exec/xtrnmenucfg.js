"use strict"

/**
 * Menu editor for the enhanced external program menu system (see xtrn_sec.js)
 *
 * This edits the file xtrnmenu.json in the ctrl dir
 *
 * Editor Based initially on menushell.js
 */

load("sbbsdefs.js");
load("uifcdefs.js");

var log = function(msg) {
	var f = new File(system.mods_dir + "uifc.log");
	f.open("a");
	f.writeln(system.timestr() + ": " + msg);
	f.close();
}

/**
 * Write the menus out to a file
 */
var saveMenus = function() {

	var filename = file_cfgname(system.ctrl_dir, "xtrnmenu.json");

	file_backup(filename, 5);

	var config_file = new File(filename);
	if (!config_file.open('w+')) {
		uifc.msg("ERROR: Could not write to " + filename);
		return;
	}
	config_file.write(JSON.stringify(menuconfig, null, '    '));
	config_file.close();

	uifc.msg("Config Saved");
}

/**
 * Edit a custom menu
 * @param menuid
 */
var editMenu = function(menuid) {
	var menuindex, menu, menuindex, selection, selection2, editproperty;
	var last = 0;
	var selections = [], displayoptions = [], displayoptionids = [];
	var menusize = menuconfig.menus.length;

	// new menu but no code given, make one
	if ((typeof menuid === "undefined") || (!menuid)) {
		menuid = time();
	}

	// look for existing menu
	for (var i in menuconfig.menus) {
		if (menuconfig.menus[i].id == menuid) {
			menu = menuconfig.menus[i];
			menuindex = i;
		}
	}

	if (typeof menu  === "undefined") {
		menuindex = menusize;
		menuconfig.menus[menuindex] = {
			'id': menuid,
			'title': "New Generated Menu " + menuid,
			"multicolumn_separator": null,
			"custom_menu_multicolumn_fmt": null,
			"custom_menu_singlecolumn_fmt": null,
			"header_fmt": null,
			"title_fmt": null,
			"underline_fmt": null,
			"which": null,
			"singlecolumn_height": null,
			"sort_type": "name",
			'items': []
		};
		menu = menuconfig.menus[menuindex];
	}

	while(1) {
		uifc.help_text = word_wrap("This screen allows you to edit the configuration options for the custom menu.\r\n\r\nMost options default or are set in modopts.ini, but here you can define them on a per-menu basis.\r\n\r\nClick Edit Items to edit the individual entries (programs, menus, etc.)");

		selections = [];
		for (var j in menu.items) {
			selections.push(menu.items.index);
		}

		displayoptions = [];
		displayoptionids = [];

		// setup display menu
		displayoptions.push(format("%23s: %s", "id",
			("id" in menu ? menu.id : time())));
		displayoptionids.push("id");

		displayoptions.push(format("%23s: %s", "title",
			("title" in menu ? menu.title : "")));
		displayoptionids.push("title");

		displayoptions.push(format("%23s: %s", "multicolumn_separator",
			("multicolumn_separator" in menu ? menu.multicolumn_separator : "(default)")));
		displayoptionids.push("multicolumn_separator");

		displayoptions.push(format("%23s: %s", "custom_menu_multicolumn_fmt",
			("custom_menu_multicolumn_fmt" in menu ? menu.custom_menu_multicolumn_fmt : "(default)")));
		displayoptionids.push("custom_menu_multicolumn_fmt");

		displayoptions.push(format("%23s: %s", "custom_menu_singlecolumn_fmt",
			("custom_menu_singlecolumn_fmt" in menu ? menu.custom_menu_singlecolumn_fmt : "(default)")));
		displayoptionids.push("custom_menu_singlecolumn_fmt");

		displayoptions.push(format("%23s: %s", "singlecolumn_height",
			("singlecolumn_height" in menu ? menu.singlecolumn_height : "(default)")));
		displayoptionids.push("singlecolumn_height");

		displayoptions.push(format("%23s: %s", "sort_type",
			("sort_type" in menu ? menu.sort_type : "(default)")));
		displayoptionids.push("sort_type");

		displayoptions.push(format("%23s: %s", "header_fmt",
			("header_fmt" in menu ? menu.header_fmt : "(default)")));
		displayoptionids.push("header_fmt");

		displayoptions.push(format("%23s: %s", "title_fmt",
			("title_fmt" in menu ? menu.title_fmt : "(default)")));
		displayoptionids.push("title_fmt");

		displayoptions.push(format("%23s: %s", "underline_fmt",
			("underline_fmt" in menu ? menu.underline_fmt : "(default)")));
		displayoptionids.push("underline_fmt");

		displayoptions.push(format("%23s: %s", "which",
			("which" in menu ? menu.which : "(default)")));
		displayoptionids.push("which");

		displayoptions.push(format("%23s: %s", "Edit Items", "[...]"));
		displayoptionids.push("items");

		selection = uifc.list(WIN_ORG|WIN_MID|WIN_ACT|WIN_ESC, 0, 0, 0, last, last,
			menu.title + ": Options", displayoptions);

		if (selection == -1) {
			// escape key
			break;
		}

		editproperty = displayoptionids[selection];

		switch (editproperty) {
			case 'id':
				uifc.help_text = word_wrap("This is a unique ID for the menu, which can be used as the target to\r\ncall the menu from other menus.\r\n\r\nFor the top-level menu, the id should be 'main'.");
				var selection2 = uifc.input(WIN_ORG|WIN_MID, "Menu ID:", menu.id, 50, K_EDIT);
				if (selection2 == -1) {
					// escape key
					break;
				}
				menu.id = selection2;
				break;
			case 'title':
				uifc.help_text = word_wrap("Title for the menu, to be shown at the top of the menu");
				selection2 = uifc.input(WIN_ORG|WIN_MID, "Menu Title:", menu.title, 255, K_EDIT);
				if (selection2 == -1) {
					// escape key
					break;
				}
				menu.title = selection2;
				break;

			// deletable properties
			case 'multicolumn_separator':
			case 'custom_menu_multicolumn_fmt':
			case 'custom_menu_singlecolumn_fmt':
			case 'singlecolumn_height':
			case 'header_fmt':
			case 'title_fmt':
			case 'underline_fmt':
			case 'which':
				switch (editproperty) {
					case 'multicolumn_separator':
						uifc.help_text = word_wrap("Multicolumn separator is what is printed between columns.\r\n\r\nIf defined here, it will override the default one or one set in modopts.ini.\r\n\r\nIt may be easier to edit it directly in the xtrnmenu.json.\r\n\r\nTo use Ctrl-A codes, enter \\u0001 for Ctrl-A, and enter the \\u code for any ANSI chars.");
						break;
					case 'custom_menu_multicolumn_fmt':
						uifc.help_text = word_wrap("Formatting for the multi-column mode.\r\n\r\nIf defined here, it will override the default one or one set in modopts.ini.\r\n\r\nIt may be easier to edit it directly in the xtrnmenu.json.\r\n\r\nTo use Ctrl-A codes, enter \\u0001 for Ctrl-A, and enter the \\u code for any ANSI chars.");
						break;
					case 'custom_menu_singlecolumn_fmt':
						uifc.help_text = word_wrap("Formatting for the single column mode.\r\n\r\nIf defined here, it will override the default one or one set in modopts.ini.\r\n\r\nIt may be easier to edit it directly in the xtrnmenu.json.\r\n\r\nTo use Ctrl-A codes, enter \\u0001 for Ctrl-A, and enter the \\u code for any ANSI chars.");
						break;
					case 'singlecolumn_height':
						uifc.help_text = word_wrap("The maximum number of items in the menu before it switches to multi-column.");
						break;
					case 'header_fmt':
						uifc.help_text = word_wrap("The text and formatting for the header portion that contains the menu name/title.\r\n\r\nIf defined here, it will override the default one or one set in modopts.ini.\r\n\r\nIt may be easier to edit it directly in the xtrnmenu.json.\r\n\r\nTo use Ctrl-A codes, enter \\u0001 for Ctrl-A, and enter the \\u code for any ANSI chars.");
						break;
					case 'title_fmt':
						uifc.help_text = word_wrap("The text and formatting for the title portion (legend) that contains the column titles. \r\n\r\nIf defined here, it will override the default one or one set in modopts.ini.\r\n\r\nIt may be easier to edit it directly in the xtrnmenu.json.\r\n\r\nTo use Ctrl-A codes, enter \\u0001 for Ctrl-A, and enter the \\u code for any ANSI chars.");
						break;
					case 'underline_fmt':
						uifc.help_text = word_wrap("The text and formatting for the underline portion of the menu.\r\n\r\nIf defined here, it will override the default one or one set in modopts.ini.\r\n\r\nIt may be easier to edit it directly in the xtrnmenu.json.\r\n\r\nTo use Ctrl-A codes, enter \\u0001 for Ctrl-A, and enter the \\u code for any ANSI chars.");
						break;
					case 'which':
						uifc.help_text = word_wrap("The prompt at the bottom of the menu.\r\n\r\nIf defined here, it will override the default one or one set in modopts.ini.\r\n\r\nIt may be easier to edit it directly in the xtrnmenu.json.\r\n\r\nTo use Ctrl-A codes, enter \\u0001 for Ctrl-A, and enter the \\u code for any ANSI chars.");
						break;
					default:
						break;
				}

				selection2 = uifc.input(WIN_ORG|WIN_MID, editproperty,
					((editproperty in menu) && (displayoptions[editproperty] !== "(default)")) ? JSON.stringify(menu[editproperty]).substr(1).slice(0, -1).replace(/\\\\/g, "\\") : "",
					500, K_EDIT|K_MSG);
				if (selection2 == -1) {
					// escape key
					break;
				}

				if (!selection2) {
					delete menu[editproperty];
				} else {
					menu[editproperty] = selection2.replace(/\\\\/g, "\\");
				}
				break;

			case 'sort_type':
				uifc.help_text = word_wrap("How to sort the menu:\r\nby input key\r\nby title\r\nnone (the items remain in the order they are in the config)");

				switch (uifc.list(WIN_ORG|WIN_MID, "Sort Type", ["key", "title", "none"])) {
					case 0:
						menu.sort_type = "key";
						break;
					case 1:
						menu.sort_type = "title";
						break;
					case 2:
						delete menu.sort_type;
						break;
					default:
						//esc key
						break;
				}

				break;

			case 'items':
				editItems(menuid);
				break;

			default:
				// this isn't supposed to happen
				uifc.msg("Unknown option");
				break;
		}

		last = Math.max(selection, 0);
	}
}

/**
 * Edit menu items in a menu
 * @param menuid
 */
var editItems = function(menuid) {
	var menuindex, menu, selection, selection2, keyused, items = [], itemids = [];
	var i, last = 0;

	if (typeof menuid === "undefined") {
		uifc.msg("Menu could not be found");
		return;
	} else {
		for (i in menuconfig.menus) {
			if (menuconfig.menus[i].id == menuid) {
				menu = menuconfig.menus[i];
				menuindex = i;
			}
		}
	}

	uifc.help_text = word_wrap("This menu allows editing the various items in this menu.\r\n\r\n"
		+ "If you leave input key blank, it will use an auto-generated number at display time.\r\n\r\n"
		+ "Choose a type first and the dropdown to choose tha target will allow you to select your target.\r\n\r\n"
		+ "Access string only applies to custom menu items. For external sections or external programs, use the access settings in scfg.\r\n\r\n"
		+ "Setting a singlecolumn or multicolumn format allows you to highlight certain items with various colors, etc.\r\n\r\n");

	while(1) {
		items = [];
		itemids = [];
		for(i in menu.items) {
			items.push(format(
				"%6s %10s %s",
				menu.items[i].input ? menu.items[i].input : '(auto)',
				menu.items[i].type,
				menu.items[i].title
			));
			itemids.push(i);
		}
		// WIN_ORG = original menu
		// WIN_MID = centered mid
		// WIN_ACT = menu remains active after a selection
		// WIN_ESC = screen is active when escape is pressed
		// WIN_XTR = blank line to insert
		// WIN_INS = insert key
		// WIN_DEL = delete
		// WIN_CUT = cut ctrl-x
		// WIN_COPY = copy ctrl-c
		// WIN_PUT = paste ctrl-v
		selection = uifc.list(
			WIN_ORG|WIN_MID|WIN_ACT|WIN_ESC|WIN_XTR|WIN_INS|WIN_DEL|WIN_CUT|WIN_COPY|WIN_PASTE,
			0, 0, 0, last, last,
			menu.title + ": Items",
			items
		);

		if (selection == -1) {
			// esc key
			break;
		}

		if ((selection & MSK_ON) == MSK_DEL) {
			// delete item
			selection &= MSK_OFF;
			//renumber array so there are no gaps
			var menuitems2 = [];
			for (var i in menu.items) {
				if (i != itemids[selection]) {
					menuitems2.push(menu.items[i]);
				}
			}
			menu.items = menuitems2;
		} else if (((selection & MSK_ON) == MSK_INS)) {
			// new item from INSERT KEY
			editItem(menuid, null);
		} else if ((selection & MSK_ON) == MSK_COPY) {
			// copy item
			selection &= MSK_OFF;
			copyitem = JSON.parse(JSON.stringify(menu.items[itemids[selection]])); // make copy
		} else if ((selection & MSK_ON) == MSK_CUT) {
			// cut item
			selection &= MSK_OFF;
			copyitem = menu.items[itemids[selection]];

			//renumber array so there are no gaps
			var menuitems2 = [];
			for (var i in menu.items) {
				if (i != itemids[selection]) {
					menuitems2.push(menu.items[i]);
				}
			}
			menu.items = menuitems2;
		} else if ((selection & MSK_ON) == MSK_PASTE) {
			// paste item
			selection &= MSK_OFF;

			var oktopaste = true;

			// only paste if there is an item copied
			if ("type" in copyitem) {
				// if item already exists in list, modify if since you can't have dupes (except empty input keys)
				for (var i in menu.items) {
					if ((menu.items[i].input == copyitem.input) && (copyitem.input !== null) && (copyitem.input !== "")) {
						oktopaste = true;
						while(1) {
							selection2 = uifc.input(WIN_ORG | WIN_MID, "Enter New Input Key", "", 3, K_EDIT);
							if ((selection2 == -1) || (selection2 === "") || (selection2 === null)) {
								// escape key
								copyitem.input = null;
								break;
							}
							if (selection2 || selection2 === 0) {
								selection2 = selection2.toUpperCase();
								keyused = false;
								for (var j in menu.items) {
									if (menu.items[j].input && (menu.items[j].input.toUpperCase() == selection2)) {
										keyused = true;
									}
								}
								if (keyused) {
									uifc.msg("This input key is alread used for another item");
									oktopaste = false;
								} else {
									copyitem.input = selection2;
									oktopaste = true;
									break;
								}
							}
						}
						copyitem.input = selection2;
					}
				}
				if ((oktopaste) || (copyitem.input === "null") || (copyitem.input === "")) {
					var menuitems2 = [];
					for (i in menu.items) {
						menuitems2.push(menu.items[i]);
						// paste copied item after selected item
						if (i == itemids[selection]) {
							menuitems2.push(copyitem);
						}
					}
					menu.items = menuitems2;
				}
			}
		} else if (selection >= menu.items.length) {
			// new item from blank line
			editItem(menuid, null);
		} else {
			editItem(menuid, itemids[selection]);
		}
		last = Math.max(selection, 0);
	}
}

/**
 * Edit a specific menu item entry
 * @param menuid
 * @param itemindex
 */
var editItem = function(menuid, itemindex) {
	var menu, menuindex, item;
	var keyused, selection, selection2, i, last = 0;
	var displayoptions = [], displayoptionids = [], newitems = [];
	// used for building target selection
	var custommenuitems = [], custommenuitemsids = [], custommenunames = [];

	if (typeof menuid === "undefined") {
		uifc.msg("Menu could not be found");
		return;
	} else {
		for (i in menuconfig.menus) {
			if (menuconfig.menus[i].id == menuid) {
				menu = menuconfig.menus[i];
				menuindex = i;
			}
		}
	}

	if (typeof menu.items[itemindex] === "undefined") {
		// new item
		menu.items.push({
			"input": null,
			"title": "New Item " + time(),
			"type": null,
			"target": null,
			"access_string": null,
			"singlecolumn_fmt": null,
			"multicolumn_fmt": null
		});
		itemindex = menu.items.length - 1;
	}
	item = menu.items[itemindex];

	while(1) {
		displayoptions = [];
		displayoptionids = [];

		// setup display menu
		displayoptions.push(format("%23s: %s", "input",
			(("input" in item) && (item.input !== null) && (item.input !== "") ? item.input : "(auto)")));
		displayoptionids.push("input");

		displayoptions.push(format("%23s: %s", "title",
			("title" in item ? item.title : "")));
		displayoptionids.push("title");

		displayoptions.push(format("%23s: %s", "type",
			("type" in item ? item.type : "")));
		displayoptionids.push("type");

		displayoptions.push(format("%23s: %s", "target",
			("target" in item ? item.target : "")));
		displayoptionids.push("target");

		if (item.type == "custommenu") {
			displayoptions.push(format("%23s: %s", "access_string",
				("access_string" in item ? item.access_string : "(default)")));
			displayoptionids.push("access_string");
		}

		displayoptions.push(format("%23s: %s", "multicolumn_fmt",
			("multicolumn_fmt" in item ? item.multicolumn_fmt : "")));
		displayoptionids.push("multicolumn_fmt");

		displayoptions.push(format("%23s: %s", "singlecolumn_fmt",
			("singlecolumn_fmt" in item ? item.singlecolumn_fmt : "")));
		displayoptionids.push("singlecolumn_fmt");

		selection = uifc.list(WIN_ORG | WIN_MID | WIN_ACT | WIN_ESC,
			0, 0, 0, last, last, menu.title + ": Item " + itemindex, displayoptions);

		if (selection < 0) {
			if (!item.title || !item.type || !item.target) {
				if (uifc.list(WIN_MID, "This item is missing required items.", ["Remove Item", "Edit Item"]) == 0) {
					// delete item and continue
					newitems = [];
					for (i in menu.items) {
						if (i != itemindex) {
							newitems.push(menu.items[i]);
						}
					}
					menu.items = newitems;
					break;
				}
			} else {
				// leave menu
				break;
			}
		}

		switch (displayoptionids[selection]) {
			case 'input':
				uifc.help_text = word_wrap("The input key to access this item. Can be anything except Q. Leave blank to auto-generate a number.");
				selection2 = uifc.input(WIN_ORG | WIN_MID, "Input Key", item.input, 3, K_EDIT);
				if (selection2 == -1) {
					// escape key
					break;
				}

				if ((selection2 !== null) && (selection2 !== "")) {
					selection2 = selection2.toUpperCase();

					keyused = false;
					for (i in menu.items) {
						if ((menu.items[i].input === null) || (menu.items[i].input === "")) {
							// continue here as toUpperCase would break it, and they don't need to match
							// anyway because you can have multiple auto-assigned input items
							continue;
						}
						if ((menu.items[i].input.toUpperCase() == selection2) && (i != itemindex)) {
							keyused = true;
						}
					}

					if (keyused) {
						uifc.msg("This input key is already used by another item.");
					} else {
						item.input = selection2;
					}
				} else {
					// save blank
					item.input = selection2;
				}
				break;

			case 'title':
				uifc.help_text = word_wrap("The menu item title.");
				selection2 = uifc.input(WIN_ORG | WIN_MID, "Title", item.title, 255, K_EDIT);
				if (typeof selection2 === "undefined") {
					// escape key
					break;
				}
				if (!selection2 && selection2 !== 0) {
					uifc.msg("Title is required.");
				} else {
					item.title = selection2;
				}
				break;

			case 'type':
				uifc.help_text = word_wrap(
					"This is the type of target this item points to.\r\n\r\n"
					+ "custommenu is a custom menu defined in this tool.\r\n\r\n"
					+ "xtrnmenu is a standard Syncrhonet External Section Menu (refer to the scfg tool).\r\n\r\n"
					+ "xtrnprog is a direct link to an external program (refer to the scfg tool)");

				switch (uifc.list(WIN_ORG | WIN_MID, "Target Type", ["custommenu", "xtrnmenu", "xtrnprog"])) {
					case 0:
						item.type = "custommenu";
						break;
					case 1:
						item.type = "xtrnmenu"
						break;
					case 2:
						item.type = "xtrnprog";
						break;
					default:
						// includes escape key
						break;
				}
				break;

			case 'target':
				uifc.help_text = word_wrap("This is the ID of the custom menu, external program section, or external program to link to.");

				switch (item.type) {
					case "custommenu":
						// present list of custom menus
						custommenuitems = [];
						custommenuitemsids = [];
						custommenunames = [];
						for (i in menuconfig.menus) {
							custommenuitems.push(format("%23s: %s", menuconfig.menus[i].id, menuconfig.menus[i].title));
							custommenuitemsids.push(menuconfig.menus[i].id);
							custommenunames.push(menuconfig.menus[i].title);
						}

						selection2 = uifc.list(WIN_ORG | WIN_MID, "Target", custommenuitems);
						if (typeof selection2 === "undefined") {
							// escape key
							break;
						}

						item.target = custommenuitemsids[selection2];

						while(1) {
							if (uifc.list(WIN_MID, "Replace item title with sections's name?", ["No", "Yes"]) == 1) {
								item.title = custommenunames[selection2]; // for external program, change title to program name
							}
							break;
						}
						break;
						break;

					case "xtrnmenu":
						// present list of external program sections
						var seclist = [];
						for (i in xtrn_area.sec_list) {
							seclist.push({ code: xtrn_area.sec_list[i].code, name: xtrn_area.sec_list[i].name});
						};
						seclist.sort(sort_by_code);

						custommenuitems = [];
						custommenuitemsids = [];
						custommenunames = [];
						for (i in seclist) {
							custommenuitems.push(format("%23s: %s", seclist[i].code, seclist[i].name));
							custommenuitemsids.push(seclist[i].code);
							custommenunames.push(seclist[i].name);
						}

						selection2 = uifc.list(WIN_ORG | WIN_MID, "Target", custommenuitems);
						if (typeof selection2 === "undefined") {
							// escape key
							break;
						}

						item.target = custommenuitemsids[selection2];

						while(1) {
							if (uifc.list(WIN_MID, "Replace item title with sections's name?", ["No", "Yes"]) == 1) {
								item.title = custommenunames[selection2]; // for external program, change title to program name
							}
							break;
						}
						break;

					case "xtrnprog":

						// present list of external programs
						// create sorted list
						var proglist = [];
						custommenunames = [];
						for (i in xtrn_area.prog) {
							proglist.push({ code: xtrn_area.prog[i].code, name: xtrn_area.prog[i].name});
						};
						proglist.sort(sort_by_code);
						for (i in proglist) {
							custommenuitems.push(format("%23s: %s", proglist[i].code, proglist[i].name));
							custommenuitemsids.push(proglist[i].code);
							custommenunames.push(proglist[i].name);
						}
						selection2 = uifc.list(WIN_ORG | WIN_MID, "Target", custommenuitems);
						if (selection2 == -1) {
							// escape key
							break;
						}
						if (selection2 || selection2 === 0) {
							item.target = custommenuitemsids[selection2];
							while(1) {
								if (uifc.list(WIN_MID, "Replace item title with program's name?", ["No", "Yes"]) == 1) {
									item.title = custommenunames[selection2]; // for external program, change title to program name
								}
								break;
							}
						}
						break;

					default:
						selection2 = uifc.input(WIN_ORG | WIN_MID, "Target", item.target, 50, K_EDIT);
						if (typeof selection2 === "undefined") {
							// escape key
							break;
						}

						item.target = selection2;
						break;
				}
				break;

			case 'access_string':
				uifc.help_text = word_wrap("The access string for the custom menu.\r\n\r\nOnly applies to custom menu items.\r\n\r\nExample: LEVEL 60");
				selection2 = uifc.input(WIN_ORG | WIN_MID, "Access String", item.access_string, 255, K_EDIT);
				if (typeof selection2 === "undefined") {
					// escape key
					break;
				}
				item.access_string = selection2;
				break;

			case 'multicolumn_fmt':
				uifc.help_text = word_wrap("Define the item format.\r\n\r\nYou can use this to apply different colors, etc. to an item to highlight it.\r\n\r\nEnter \\u0001 for a ctrl-a.");
				selection2 = uifc.input(WIN_ORG | WIN_MID, "Multi Column Format",
					item.multicolumn_fmt ? JSON.stringify(item.multicolumn_fmt).substr(1).slice(0, -1).replace(/\\\\/g, "\\") : "",
					255, K_EDIT|K_MSG);
				if (typeof selection2 === "undefined") {
					// escape key
					break;
				}
				item.multicolumn_fmt = selection2.replace(/\\\\/g, "\\");
				break;

			case 'singlecolumn_fmt':
				uifc.help_text = word_wrap("Define the item format.\r\n\r\nYou can use this to apply different colors, etc. to an item to highlight it.\r\n\r\nEnter \\u0001 for a ctrl-a.");
				selection2 = uifc.input(WIN_ORG | WIN_MID, "Single Column Format",
					item.singlecolumn_fmt ? JSON.stringify(item.singlecolumn_fmt).substr(1).slice(0, -1).replace(/\\\\/g, "\\") : "",
					255, K_EDIT|K_MSG);
				if (typeof selection2 === "undefined") {
					// escape key
					break;
				}
				item.singlecolumn_fmt = selection2.replace(/\\\\/g, "\\");
				break;
		}
		last = Math.max(selection, 0);
	}
}

function sort_by_name(a, b)
{
	if (a.name.toLowerCase() > b.name.toLowerCase()) return 1;
	if (a.name.toLowerCase() < b.name.toLowerCase()) return -1;
	return 0;
}

function sort_by_code(a, b)
{
	if (a.code.toLowerCase() > b.code.toLowerCase()) return 1;
	if (a.code.toLowerCase() < b.code.toLowerCase()) return -1;
	return 0;
}

function sort_by_id(a, b)
{
	if (a.id.toLowerCase() > b.id.toLowerCase()) return 1;
	if (a.id.toLowerCase() < b.id.toLowerCase()) return -1;
	return 0;
}


// MAIN
try {
	var menuconfig = {};
	var copyitem = {}; // for menu item copy/paste
	var config_file = new File(file_cfgname(system.ctrl_dir, "xtrnmenu.json"));
	if (config_file.open('r+')) {
		var config_src = config_file.read();
		try {
			menuconfig = JSON.parse(config_src.toString());
			if (!menuconfig) {
				writeln("ERROR! Could not parse xtrnmenu.json. JSON may be invalid.");
				exit();
			}
		} catch (e) {
			writeln("ERROR! Could not parse xtrnmenu.json. JSON may be invalid. " + e.toString());
			exit();
		}
	}
	config_file.close();

	uifc.init("Enhanced External Program Menus Configurator");
	js.on_exit("if (uifc.initialized) uifc.bail()");

	while(1) {
		uifc.help_text = word_wrap("This program allows managing the Enhanced External Program Menu feature.");

		// no menus or no main menu
		var mainmenufound = false;
		for (var m in menuconfig.menus) {
			if (menuconfig.menus[m].id.toLowerCase() == "main") {
				mainmenufound = true;
			}
		}
		if (!mainmenufound || (menuconfig.menus.length < 1)) {
			uifc.msg("No menus defined and/or missing the main menu. Setting up now.");
			editMenu("main");
		}

		// menus is array of menuconfig menu ids
		var menus = [];
		var menuTitles = [];
		menuconfig.menus.sort(sort_by_id);
		for (var m in menuconfig.menus) {
			menus.push(menuconfig.menus[m].id);
			menuTitles.push(format("%20s: %s", menuconfig.menus[m].id, menuconfig.menus[m].title));
		}

		// cur bar top left width
		var ctx = new uifc.list.CTX(0, 0, 0, 0, 0);

		// WIN_ORG = original menu, destroy valid screen area
		// WIN_MID = place window in middle of screen
		// WIN_XTR = add extra line at end for inserting at end
		// WIN_DEL = allow user to use delete key
		// WIN_ACT = menu remains active after a selection
		// WIN_ESC = screen is active when escape is hit

		// WIN_INS = allow user to use insert key
		var selection = uifc.list(
			WIN_ORG|WIN_MID|WIN_XTR|WIN_DEL|WIN_ACT|WIN_ESC|WIN_INS,
			"Enhanced External Menus",
			menuTitles,
			ctx
		);

		if (selection < 0) {
			while (1) {
				var ret = uifc.list(WIN_MID, "Save Changes Before Exit?", ["Yes", "No", "Cancel"]);
				if (ret == 0) {
					saveMenus();
					exit();
				} else if (ret == 1) {
					// no - exit
					exit();
				} else {
					// cancel
					break;
				}
			}

		} else if ((selection & MSK_ON) == MSK_DEL) {
			selection &= MSK_OFF;
			for (var m in menuconfig.menus) {
				if (menuconfig.menus[m].id == menus[selection]) {
					delete menuconfig.menus[m];
				}
			}
			//selection--;
		} else if (((selection & MSK_ON) == MSK_INS) || (selection >= menuconfig.menus.length)) {
			// new menu
			var newid = uifc.input(
				WIN_MID,
				"Enter a short unique id for the menu",
				"",
				0
			);
			if (typeof newid !== "undefined") {
				var menufound = false;
				for (var mf in menuconfig.menus) {
					if (menuconfig.menus[mf].id == newid) {
						menufound = true;
					}
				}
				if (menufound) {
					uifc.msg("That ID is already in use. Please choose another.");
				} else {
					editMenu(newid);
				}
			}
		} else {
			editMenu(menuconfig.menus[selection].id);
		}
	}
} catch(err) {
	if ((typeof uifc !== "undefined") && uifc.initialized) {
		uifc.bail();
	}
	writeln(err);
	log(err);
	console.pause();
}
