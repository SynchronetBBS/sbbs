/*	Menu editor for menushell.js, menushell-lb.js
	echicken -at- bbs.electronicchicken.com

	I am not the least bit proud of this mess.  But it "works."
*/

load("sbbsdefs.js");
load("uifcdefs.js");
load("menu-commands.js");

var help = {
	'main' : [
		"Main menu",
		"Select a menu to edit, and hit <enter>.",
		"Hit <enter> on a blank line to create a new menu."
	],
	'editMenu' : [
		"Edit a menu",
		"   Title: String shown at top if header graphic unspecified.",
		"  Header: File to be displayed at the top of the menu.",
		"    List: Whether to display the list of menu items.",
		"  Prompt: The prompt string.",
		"Commands: Submenu for assigning actions to hotkeys.",
		" Default: Whether or not this is the default menu.",
		"",
		"CTRL-A and @-codes are supported in Title and Prompt."
	],
	'editCommands' : [
		"Menu commands",
		"A list of menu commands/items.",
		"Format is:",
		"Hotkey, Text, Area->Command"
	],
	'editCommand' : [
		"Edit a command",
		"  Hotkey: The hotkey for this menu item.",
		"    Text: The text displayed for this item. CTRL-A and @ codes supported.",
		"     ARS: Access-requirements string for this item.",
		"Position: This item's position in the menu (starts at 0.)",
		"    Area: The command-area of this item.",
		" Command: If 'Area' is not 'Menus', the command to be executed.",
		"    Menu: If 'Area' is 'Menus', the menu to be loaded."
	],
	'chooseCommandArea' : [
		"Choose a command area",
		"     User: User information and configuration commands.",
		"   System: System areas & commands that don't fit elsewhere.",
		" Messages: Message area functions.",
		"    Files: File area functions.",
		"Externals: External programs.",
		"    Menus: Other menus."
	],
	'editLightbarOptions' : [
		"Lightbar Options",
		"Position: Centered, Left, Right, or Fixed",
		"x: X-coordinate of top left corner of menu box (fixed only)",
		"y: Y-coordinate of top left corner of menu (fixed only)",
		"Width: Width of menu box (fixed only)", 
		"Height: Height of menu box (fixed only)",
		"Background: Background graphic to load",
		"ActiveForeground: CTRL-A code, active item foreground color",
		"ActiveBackground: CTRL-A code, active item background color",
		"InactiveForeground: CTRL-A code, inactive item foreground",
		"InactiveBackground: CTRL-A code, inactive item background",
		"HotkeyColor: CTRL-A code, hotkey color",
		"BorderColor: CTRL-A code, menu box border color",
		"Hotkeys: Whether or not to use hotkeys"
	],
	'commands' : []
};

var log = function(msg) {
	var f = new File(system.mods_dir + "uifc.log");
	f.open("a");
	f.writeln(system.timestr() + ": " + msg);
	f.close();
}

var saveMenus = function() {
	file_backup(menuFile);
	var f = new File(menuFile);
	f.open("w");
	f.write(JSON.stringify(Commands.Menus));
	f.close();
}

var init = function() {
	uifc.init("Synchronet Menu Editor");
	js.on_exit("uifc.bail()");
}

var uifcYesNo = function(question) {
	while(1) {
		var ret = uifc.list(WIN_MID, question + "?", ["Yes", "No"]);
		if(ret >= 0)
			break;
	}
	return (ret == 0);
}

var uifcHelp = function(section) {
	uifc.help_text = word_wrap(help[section].join("\r\n\r\n"));
}

var fileBrowser = function(dir) {
	if(!file_isdir(dir))
		return;
	while(1) {
		var file;
		var files = directory(dir + "*", GLOB_MARK);
		var displayFiles = [];
		for(var f = 0; f < files.length; f++) {
			if(file_isdir(files[f])) {
				var fn = (files[f][files[f].length - 1] == "/") ? files[f].split("/") : files[f].split("\\");
				displayFiles.push(fn[fn.length - 2] + "/");
			} else {
				displayFiles.push(file_getname(files[f]));
			}
		}
		var selection = uifc.list(WIN_ORG|WIN_MID, "Browsing " + dir, displayFiles);
		if(selection < 0)
			break;
		else if(file_isdir(files[selection]))
			var file = fileBrowser(files[selection]);
		else
			var file = files[selection];
		if(typeof file != "undefined")
			break;
	}
	return file;
}

var main = function() {

	var last = 0;
	while(1) {
		getMenus();
		uifcHelp("main");
		var menus = Object.keys(Commands.Menus);
		var menuTitles = [];
		for(var m in Commands.Menus)
			menuTitles.push(Commands.Menus[m].Title);
		var selection = uifc.list(
			WIN_ORG|WIN_MID|WIN_XTR|WIN_DEL|WIN_ACT|WIN_ESC,
			0, 0, 0, last, last,
			"Menus",
			menuTitles
		);
		if(selection < 0) {
			break;
		} else if(selection&MSK_DEL) {
			selection ^= MSK_DEL;
			delete Commands.Menus[menus[selection]];
			selection--;
		} else if(selection >= menus.length) {
			editMenu();
		} else {
			editMenu(selection);
		}
		last = Math.max(selection, 0);
		if(uifcYesNo("Save changes"))
			saveMenus();
	}

}

var editMenu = function(menu) {

	if(typeof menu == "undefined") {
		while(typeof ID == "undefined" || typeof Commands.Menus[ID] != "undefined")
			var ID = time();
		Commands.Menus[ID] = {
			'Title' : "Title",
			'Header' : "head.asc",
			'List' : true,
			'Prompt' : "Your choice:",
			'Commands' : [],
			'Lightbar' : {
				'Position' : "Centered",
				'x' : 1,
				'y' : 1,
				'Width' : 1,
				'Height' : 1,
				'Background' : "",
				'ActiveForeground' : "\\1h\\1w",
				'ActiveBackground' : "\\16",
				'InactiveForeground' : "\\1n\\1w",
				'InactiveBackground' : "\\10",
				'HotkeyColor' : "\\1h\\1c",
				'BorderColor' : "\\1h\\1w",
				'Hotkeys' : true
			}
		};
		var keys = Object.keys(Commands.Menus);
		var menu = keys[keys.length - 1];
		Commands.Menus[ID].Default = (keys.length > 1) ? false : true;
	} else {
		var menu = Object.keys(Commands.Menus)[menu];
	}
	var last = 0;
	while(1) {
		uifcHelp("editMenu");
		var selections = Object.keys(Commands.Menus[menu]);
		var items = [];
		for(var item in Commands.Menus[menu]) {
			items.push(
				format(
					"%8s  %s",
					item,
					(typeof Commands.Menus[menu][item] != "object") ? Commands.Menus[menu][item] : "..."
				)
			);
		}
		var selection = uifc.list(
			WIN_ORG|WIN_MID|WIN_ACT|WIN_ESC,
			0, 0, 0, last, last,
			Commands.Menus[menu].Title + ": Items",
			items
		);
		if(selection < 0)
			break;
		switch(selections[selection]) {
			case "Header":
				var selection2 = uifc.list(
					WIN_ORG|WIN_MID,
					"Header file:",
					["Enter a path", "Browse"]
				);
				if(selection2 == 0) {
					var header = uifc.input(
						WIN_MID,
						Commands.Menus[menu].Header,
						0,
						K_EDIT
					);
				} else if(selection2 == 1) {
					var header = fileBrowser(system.text_dir + "menu/");
				}
				if(typeof header == "string")
					Commands.Menus[menu].Header = header.replace(system.text_dir + "menu/", "");
				break;
			case "List":
				var yes = uifcYesNo("List menu items");
				if(yes == -1)
					break;
				Commands.Menus[menu].List = yes;
				break;
			case "Commands":
				editCommands(menu);
				break;
			case "Lightbar":
				editLightbarOptions(menu);
				break;
			case "Default":
				var yes = uifcYesNo("Default menu");
				Commands.Menus[menu].Default = (yes);
				if(!yes)
					break;
				for(var m in Commands.Menus) {
					if(m == menu)
						continue;
					Commands.Menus[m].Default = false;
				}
				break;
			default:
				editItem(menu, selections[selection]);
				break;
		}
		last = selection;
	}

}

var editItem = function(menu, item) {
	var text = uifc.input(WIN_MID, item, Commands.Menus[menu][item], 0, K_EDIT);
	if(typeof text != "undefined")
		Commands.Menus[menu][item] = text;
}

var editCommands = function(menu) {
	var last = 0;
	while(1) {
		uifcHelp("editCommands");
		var menuCommands = [];
		for(var c in Commands.Menus[menu].Commands) {
			if(Commands.Menus[menu].Commands[c].command.split(".")[1] == "Menus") {
				menuCommands.push(
					format(
						"%s  %-30s  Menus->%s",
						Commands.Menus[menu].Commands[c].hotkey,
						Commands.Menus[menu].Commands[c].text.substr(0, 30),
						Commands.Menus[Commands.Menus[menu].Commands[c].menu].Title.substr(0, uifc.screen_width - 35)
					)
				);
			} else {
				var path = Commands.Menus[menu].Commands[c].command.split(".");
				menuCommands.push(
					format(
						"%s  %-30s  %s->%s",
						Commands.Menus[menu].Commands[c].hotkey,
						Commands.Menus[menu].Commands[c].text.substr(0, 30),
						path[1],
						path[2]
					)
				);
			}
		}
		var selection = uifc.list(
			WIN_ORG|WIN_MID|WIN_XTR|WIN_DEL|WIN_ACT|WIN_ESC,
			0, 0, 0, last, last,
			Commands.Menus[menu].Title + ": Commands",
			menuCommands
		);
		if(selection < 0) {
			break;
		} else if(selection&MSK_DEL) {
			Commands.Menus[menu].Commands.splice((selection^MSK_DEL), 1);
			last = Math.max((selection^MSK_DEL) - 1, 0);
		} else if(selection < Commands.Menus[menu].Commands.length) {
			last = editCommand(menu, selection);
		} else {
			last = editCommand(menu);
		}
	}
}

var editCommand = function(menu, selection) {

	if(typeof selection == "undefined") {
		Commands.Menus[menu].Commands.push(
			{	'hotkey' : '',
				'text' : '',
				'ARS' : '',
				'command' : 'Commands.Messages.Find'
			}
		);
		selection = Commands.Menus[menu].Commands.length - 1;
	}

	while(1) {
		uifcHelp("editCommand");
		var path = Commands.Menus[menu].Commands[selection].command.split(".");
		var options = [
			"  Hotkey: " + Commands.Menus[menu].Commands[selection].hotkey,
			"    Text: " + Commands.Menus[menu].Commands[selection].text,
			"     ARS: " + Commands.Menus[menu].Commands[selection].ARS,
			"Position: " + selection,
			"    Area: " + path[1]
		];
		if(path.length > 2)
			options.push(" Command: " + path[2]);
		else
			options.push("    Menu: " + Commands.Menus[Commands.Menus[menu].Commands[selection].menu].Title);
		var selection2 = uifc.list(WIN_ORG|WIN_MID, "Edit command", options);
		if(selection2 < 0)
			break;
		if(selection2 == 0) {
			Commands.Menus[menu].Commands[selection].hotkey = uifc.input(
				WIN_MID,
				"Hotkey",
				Commands.Menus[menu].Commands[selection].hotkey,
				1,
				K_EDIT|K_UPPER
			);
		} else if(selection2 == 1) {
			var text = uifc.input(
				WIN_MID,
				"Text",
				Commands.Menus[menu].Commands[selection].text,
				0,
				K_EDIT
			);
			if(typeof text != "undefined")
				Commands.Menus[menu].Commands[selection].text = text;
		} else if(selection2 == 2) {
			if(Commands.Menus[menu].Commands[selection].command.split(".")[1] == "Externals")
				continue;
			var ars = uifc.input(
				WIN_MID,
				"Access Requirements",
				Commands.Menus[menu].Commands[selection].ARS,
				0,
				K_EDIT
			);
			if(typeof ars != "undefined")
				Commands.Menus[menu].Commands[selection].ARS = ars;
		} else if(selection2 == 3) {
			var position = uifc.input(
				WIN_MID,
				"Position",
				selection.toString(),
				0,
				K_NUMBER|K_EDIT
			);
			if(typeof position == "undefined" || parseInt(position) >= Commands.Menus[menu].Commands.length)
				continue;
			var cut = Commands.Menus[menu].Commands.splice(selection, 1)[0];
			Commands.Menus[menu].Commands.splice(position, 0, cut);
			selection = parseInt(position);
		} else {
			var area = chooseCommandArea();
			if(typeof area == "undefined")
				break;
			var command = chooseCommand(area);
			if(typeof command == "undefined")
				break;
			var areas = Object.keys(Commands);
			if(areas[area] == "Menus") {
				var menus = Object.keys(Commands.Menus);
				Commands.Menus[menu].Commands[selection].command = "Commands.Menus";
				Commands.Menus[menu].Commands[selection].menu = menus[command];
			} else if(areas[area] == "Externals") {
				Commands.Menus[menu].Commands[selection].command = "Commands.Externals." + command;
				Commands.Menus[menu].Commands[selection].ARS = xtrn_area.prog[command].ars;
			} else {
				var commands = Object.keys(Commands[areas[area]]);
				Commands.Menus[menu].Commands[selection].command = "Commands." + areas[area] + "." + commands[command];
			}
		}
	}
	return selection;
}

var chooseCommandArea = function() {
	uifcHelp("chooseCommandArea");
	var areas = Object.keys(Commands);
	var selection = uifc.list(WIN_MID, "Command Areas", areas);
	if(selection >= 0)
		return selection;			
}

var chooseCommand = function(area) {
	var areas = Object.keys(Commands);
	if(areas[area] == "Menus") {
		var commands = [];
		for(var m in Commands.Menus)
			commands.push(Commands.Menus[m].Title);
	} else if(areas[area] == "Externals") {
		var sec = chooseXtrnSec();
		return chooseXtrn(sec);
	} else {
		var commands = Object.keys(Commands[areas[area]]);
		help.commands = [areas[area] + " commands"];
		for(var c = 0; c < commands.length; c++)
			help.commands.push(commands[c] + ": " + Commands[areas[area]][commands[c]].Description);
	}
	uifcHelp("commands");
	while(1) {
		var selection = uifc.list(WIN_MID, "Commands", commands);
		if(selection < 0)
			break;
		return selection;
	}
	help.commands = [];
}

var chooseXtrnSec = function() {
	var sex = [];
	var codes = [];
	for(var sec in xtrn_area.sec) {
		sex.push(xtrn_area.sec[sec].name);
		codes.push(sec);
	}
	var selection = uifc.list(WIN_MID, "External Program Areas", sex);
	if(selection >= 0)
		return codes[selection];
}

var chooseXtrn = function(area) {
	if(typeof area == "undefined")
		return;
	var xtrns = [];
	var codes = [];
	for(var prog in xtrn_area.sec[area].prog_list) {
		xtrns.push(xtrn_area.sec[area].prog_list[prog].name);
		codes.push(xtrn_area.sec[area].prog_list[prog].code);
	}
	var selection = uifc.list(WIN_MID, "External Programs", xtrns);
	if(selection >=0 )
		return codes[selection];
}

var editLightbarOptions = function(menu) {
	var last = 0;
	while(1) {
		var options = [];
		for(var property in Commands.Menus[menu].Lightbar) {
			options.push(
				format(
					"%22s  %s",
					property, Commands.Menus[menu].Lightbar[property]
				)
			);
		}
		var selection = uifc.list(
			WIN_ORG|WIN_MID|WIN_ACT,
			0, 0, 0, last, last,
			"Lightbar Options",
			options
		);
		last = selection;
		if(selection < 0)
			return;
		switch(selection) {
			case 0:
				var opts = ["Centered", "Left", "Right", "Fixed"];
				var pos = uifc.list(WIN_MID, "Lightbar menu position", opts);
				if(pos >= 0)
					Commands.Menus[menu].Lightbar.Position = opts[pos];
				break;
			case 1:
				if(Commands.Menus[menu].Lightbar.Position != "Fixed")
					break;
				var x = parseInt(
					uifc.input(
						WIN_MID,
						"X coordinate",
						Commands.Menus[menu].Lightbar.x.toString(),
						2,
						K_EDIT
					)
				);
				if(!isNaN(x) && x > 0)
					Commands.Menus[menu].Lightbar.x = x;
				break;
			case 2:
				if(Commands.Menus[menu].Lightbar.Position != "Fixed")
					break;
				var y = parseInt(
					uifc.input(
						WIN_MID,
						"Y coordinate",
						Commands.Menus[menu].Lightbar.y.toString(),
						2,
						K_EDIT
					)
				);
				if(!isNaN(y) && y > 0)
					Commands.Menus[menu].Lightbar.y = y;
				break;
			case 3:
				if(Commands.Menus[menu].Lightbar.Position != "Fixed")
					break;
				var w = parseInt(
					uifc.input(
						WIN_MID,
						"Width",
						Commands.Menus[menu].Lightbar.Width.toString(),
						2,
						K_EDIT
					)
				);
				if(!isNaN(w) && w > 0)
					Commands.Menus[menu].Lightbar.Width = w;
				break;
			case 4:
				if(Commands.Menus[menu].Lightbar.Position != "Fixed")
					break;
				var h = parseInt(
					uifc.input(
						WIN_MID,
						"Height",
						Commands.Menus[menu].Lightbar.Height.toString(),
						2,
						K_EDIT
					)
				);
				if(!isNaN(h) && h > 0)
					Commands.Menus[menu].Lightbar.Height = h;
				break;
			case 5:
				var b = uifc.list(
					WIN_ORG|WIN_MID,
					"Background file:",
					["Enter a path", "Browse"]
				);
				if(b == 0) {
					var b = uifc.input(
						WIN_MID,
						Commands.Menus[menu].Lightbar.Background,
						0,
						K_EDIT
					);
				} else if(b == 1) {
					var bg = fileBrowser(system.text_dir + "menu/");
				}
				if(typeof bg == "string")
					Commands.Menus[menu].Lightbar.Background = bg.replace(system.text_dir + "menu/", "");
				break;
			case 6:
				var fg = uifc.input(
					WIN_MID,
					"Active foreground",
					Commands.Menus[menu].Lightbar.ActiveForeground,
					12,
					K_EDIT
				);
				if(typeof fg != "undefined" && fg != "")
					Commands.Menus[menu].Lightbar.ActiveForeground = fg;
				break;
			case 7:
				var bg = uifc.input(
					WIN_MID,
					"Active background",
					Commands.Menus[menu].Lightbar.ActiveBackground,
					6,
					K_EDIT
				);
				if(typeof bg != "undefined" && bg != "")
					Commands.Menus[menu].Lightbar.ActiveBackground = bg;
				break;
			case 8:
				var fg = uifc.input(
					WIN_MID,
					"Inactive foreground",
					Commands.Menus[menu].Lightbar.InactiveForeground,
					12,
					K_EDIT
				);
				if(typeof fg != "undefined" && fg != "")
					Commands.Menus[menu].Lightbar.InactiveForeground = fg;
				break;
			case 9:
				var bg = uifc.input(
					WIN_MID,
					"Inactive background",
					Commands.Menus[menu].Lightbar.InactiveBackground,
					6,
					K_EDIT
				);
				if(typeof bg != "undefined" && bg != "")
					Commands.Menus[menu].Lightbar.InactiveBackground = bg;
				break;
			case 10:
				var h = uifc.input(
					WIN_MID,
					"Hotkey color",
					Commands.Menus[menu].Lightbar.HotkeyColor,
					12,
					K_EDIT
				);
				if(typeof h != "undefined" && h != "")
					Commands.Menus[menu].Lightbar.InactiveForeground = h;
				break;
			case 11:
				var b = uifc.input(
					WIN_MID,
					"Border color",
					Commands.Menus[menu].Lightbar.BorderColor,
					12,
					K_EDIT
				);
				if(typeof b != "undefined" && b != "")
					Commands.Menus[menu].Lightbar.BorderColor = b;
				break;
			case 12:
				Commands.Menus[menu].Lightbar.Hotkeys = uifcYesNo("Hotkeys");
				break;
			default:
				break;
		}
	}
}

try {
	init();
	main();
} catch(err) {
	log(err);
}
