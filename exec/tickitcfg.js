load("sbbsdefs.js");
load("uifcdefs.js");
load("fidocfg.js");
var tickit = new TickITCfg();

// Backward compatability hack.
if (typeof uifc.list.CTX === "undefined") {
	uifc.list.CTX = function () {
		this.cur = 0;
		this.bar = 0;
	}
}

function pick_dir(obj)
{
	var cmd = 0;
	var libs = Object.keys(file_area.lib);
	var dirs;
	var dircodes;
	var dir;
	var ctx = new uifc.list.CTX();
	var dctx;
	var i;

	if (obj.dir !== undefined && file_area.lib[obj.dir.toLowerCase()] !== undefined) {
		for (i=0; i<libs.length; i++) {
			if (file_area.lib[obj.dir.toLowerCase()].dir_name.toLowerCase() === libs[i].toLowerCase()) {
				ctx.cur = i;
				ctx.bar = i;
				break;
			}
		}
	}
	while (cmd >= 0) {
		cmd = uifc.list(WIN_SAV|WIN_ACT|WIN_RHT, "Select Group" , libs, ctx);
		if (cmd >= 0) {
			dctx = new uifc.list.CTX();
			dircodes = file_area.lib[libs[cmd]].dir_list.map(function(v){return v.code;});
			dirs = dircodes.map(function(v){return file_area.dir[v].name;});
			if (obj.dir !== undefined) {
				for (i=0; i<dircodes.length; i++) {
					if (dircodes[i].toLowerCase() === obj.dir.toLowerCase()) {
						dctx.cur = i;
						dctx.bar = i;
					}
				}
			}
			dir = uifc.list(WIN_SAV|WIN_ACT|WIN_BOT, "Select Dir", dirs, dctx);
			if (dir >= 0) {
				return dircodes[dir];
			}
		}
	}
	return undefined;
}

function set_location(obj)
{
	var cmd = 0;
	var dir;
	var ctx = new uifc.list.CTX();
	var opts;

	while (cmd >= 0) {
		opts = ["Synchronet File Directory", "System Path"];
		opts[0] = ((obj.dir === undefined) ? '  ' : '* ')+opts[0];
		opts[1] = ((obj.path === undefined) ? '  ' : '* ')+opts[1];

		cmd = uifc.list(WIN_SAV|WIN_ACT, "Location Type", opts, ctx);
		switch(cmd) {
		case 0:
			dir = pick_dir(obj);
			if (dir !== undefined) {
				obj.dir = dir;
				delete obj.path;
				return;
			}
			cmd = 0;
			break;
		case 1:
			dir = null;
			while(dir !== undefined) {
				dir = uifc.input(WIN_SAV|WIN_MID, "Path", obj.path === undefined ? '' : obj.path, 1024, K_EDIT);
				if (dir != undefined) {
					if (file_isdir(dir)) {
						delete obj.dir;
						obj.path = dir;
						return;
					}
					else {
						uifc.msg("Invalid path!");
					}
				}
			}
			break;
		}
	}
}

function set_akamatching(obj)
{
	switch(uifc.list(WIN_MID|WIN_SAV, "AKA Matching", ["Yes", "No"])) {
	case 0:
		obj.akamatching = true;
		break;
	case 1:
		obj.akamatching = false;
		break;
	}
}

function set_secureonly(obj)
{
	switch(uifc.list(WIN_MID|WIN_SAV, "Secure Only", ["Yes", "No"])) {
	case 0:
		obj.secureonly = true;
		break;
	case 1:
		obj.secureonly = false;
		break;
	}
}

function set_ignorepassword(obj)
{
	switch(uifc.list(WIN_MID|WIN_SAV, "Ignore Password", ["Yes", "No"])) {
	case 0:
		obj.ignorepassword = true;
		break;
	case 1:
		obj.ignorepassword = false;
		break;
	}
}

function set_forcereplace(obj)
{
	switch(uifc.list(WIN_MID|WIN_SAV, "Force Replace", ["Yes", "No"])) {
	case 0:
		obj.forcereplace = true;
		break;
	case 1:
		obj.forcereplace = false;
		break;
	}
}

function edit_links(links)
{
	var tmp;
	var link = 0;
	var ctx = new uifc.list.CTX();

	while (link >= 0) {
		link = uifc.list(WIN_SAV|WIN_ACT|WIN_DEL|WIN_INS|WIN_DELACT|WIN_XTR, "Links", links, ctx);
		if (link == -1) {
			break;
		}
		if (link == links.length || (link & MSK_INS) == MSK_INS) {
			link &= MSK_OFF;
			tmp = uifc.input(WIN_SAV|WIN_MID, "Address", 30);
			if (tmp !== undefined)
				links.splice(link, 0, tmp);
		}
		if (link & MSK_DEL) {
			link &= MSK_OFF;
			links.splice(link, 1);
		}
	}

	return links;
}

function select_sourceaddress(obj)
{
	var ctx = new uifc.list.CTX();
	var tmp;
	var addr;
	var addrs = ["No Source Address"]; // initial value so we can remove object's sourceaddress

	// TODO: error dialog if system.fido_addr_list is empty.
	for (addr in system.fido_addr_list)
		addrs.push(FIDO.parse_addr(system.fido_addr_list[addr]));
	tmp = uifc.list(WIN_SAV|WIN_MID|WIN_ESC, "Select Address", addrs, ctx);
	if (tmp == -1)
		return;							// ESC hit so do not change anything
	if (tmp === 0)
		delete obj.sourceaddress;		// selected No Source Address so delete it from the object
	else
		obj.sourceaddress = addrs[tmp];	// save selected source address string value in object's sourceaddress
}

function edit_uploader(obj)
{
	var tmp;

	tmp = uifc.input(WIN_SAV|WIN_MID, "Uploader", obj.uploader === undefined ? '' : obj.uploader, 32, K_EDIT);
	if (tmp != undefined)
		obj.uploader = tmp;
}

const menu_fmt = "%-20s %s";

function edit_area(obj, name)
{
	var cmd = 0;
	var link = 0;
	var links;
	var tmp;
	var ctx = new uifc.list.CTX();

	while(obj && cmd >= 0) {
		var menu = [format(menu_fmt, "AKA Matching", (obj.akamatching === true ? "Yes" : "No")),
					format(menu_fmt, "Force Replace", (obj.forcereplace === true ? "Yes" : "No")),
					format(menu_fmt, "Source Address", (obj.sourceaddress === undefined ? "" : obj.sourceaddress)),
					format(menu_fmt, "Uploader Name", (obj.uploader === undefined ? "" : obj.uploader)),
					format(menu_fmt, "Location", (obj.path === undefined ? obj.dir : obj.path)),
					"Links..."];
		cmd = uifc.list(WIN_SAV|WIN_ACT|WIN_BOT|WIN_RHT, name+" Options", menu, ctx);
		switch(cmd) {
			case 0:
				set_akamatching(obj);
				break;
			case 1:
				set_forcereplace(obj);
				break;
			case 2:
				select_sourceaddress(obj);
				break;
			case 3:
				edit_uploader(obj);
				break;
			case 4:
				set_location(obj);
				break;
			case 5:
				if (obj.links === undefined)
					links = [];
				else
					links = obj.links.split(/,/);
				tmp = edit_links(links);
				if (tmp.length === 0)
					delete obj.links;
				else
					obj.links = tmp.join(',');
				break;
			case -1:
				// Save changes?
				break;
			default:
				uifc.msg("Unhandled Return: "+cmd);
				break;
		}
	}
}

function edit_areas()
{
	var areas;
	var areas_list;
	var area = 0;
	var tmp;
	var ctx = new uifc.list.CTX();

	while(area >= 0) {
		areas = Object.keys(tickit.acfg).sort();
		areas_list = areas.map(function(v){return v.toUpperCase();});
		area = uifc.list(WIN_SAV|WIN_BOT|WIN_ACT|WIN_DEL|WIN_INS|WIN_DELACT|WIN_XTR, "Select Area", areas_list, ctx);
		if (area == -1) {
			break;
		}
		else if (area == areas.length || (area & MSK_INS) == MSK_INS) {
			area &= MSK_OFF;
			tmp = uifc.input(WIN_SAV|WIN_MID, "Area", 30);
			if (tmp !== undefined) {
				if (tickit.acfg[tmp.toLowerCase()] === undefined) {
					tickit.acfg[tmp.toLowerCase()] = {};
					edit_area(tickit.acfg[areas[area]], tmp.toLowerCase());
				}
				else {
					uifc.msg("Area already in config!");
				}
			}
		}
		else if (area & MSK_DEL) {
			area &= MSK_OFF;
			delete tickit.acfg[areas[area]];
		}
		else {
			edit_area(tickit.acfg[areas[area]], areas[area]);
		}
	}
}

function import_areas(libname)
{
	var links = prompt("Links");
	print("Import File Areas from library: " + libname);

	var file = new File(tickit.cfgfile);
	if(!file.open("at"))
		return alert("Cannot open " + file.name);
	print("Appending to " + file.name);
	for(var d in file_area.dir) {
		if(file_area.dir[d].lib_name != libname)
			continue;
		var dir = file_area.dir[d];
		print(dir.name);
		file.printf("[%s]\n", dir.name);
		file.printf("Dir=%s\n", dir.code);
		file.printf("Links=%s\n", links);
	}
	file.close();
}

function edit_globals(obj)
{

	var cmd = 0;
	var link = 0;
	var links;
	var tmp;
	var ctx = new uifc.list.CTX();

	while(cmd >= 0) {
		var menu = [format(menu_fmt, "AKA Matching", (obj.akamatching === true ? "Yes" : "No")),
					format(menu_fmt, "Force Replace", (obj.forcereplace === true ? "Yes" : "No")),
					format(menu_fmt, "Ignore Password", (obj.ignorepassword === true ? "Yes" : "No")),
					format(menu_fmt, "Secure Only", (obj.secureonly === true ? "Yes" : "No")),
					format(menu_fmt, "Source Address", (obj.sourceaddress === undefined ? "" : obj.sourceaddress)),
					format(menu_fmt, "Uploader Name", (obj.uploader === undefined ? "" : obj.uploader)),
					format(menu_fmt, "Location", (obj.path === undefined ? obj.dir : obj.path)),
					"Links..."];
		cmd = uifc.list(WIN_ACT|WIN_ESC|WIN_MID|WIN_SAV, "Global Options", menu, ctx);
		switch(cmd) {
			case 0:
				set_akamatching(obj);
				break;
			case 1:
				set_forcereplace(obj);
				break;
			case 2:
				set_ignorepassword(obj);
				break;
			case 3:
				set_secureonly(obj);
				break;
			case 4:
				select_sourceaddress(obj);
				break;
			case 5:
				edit_uploader(obj);
				break;
			case 6:
				set_location(obj);
				break;
			case 7:
				if (obj.links === undefined)
					links = [];
				else
					links = obj.links.split(/,/);
				tmp = edit_links(links);
				if (tmp.length === 0)
					delete obj.links;
				else
					obj.links = tmp.join(',');
				break;
			case -1:
				break;
			default:
				uifc.msg("Unhandled Return: "+cmd);
				break;
		}
	}
}

var unexpected_exit = true;
function main()
{

	for(var i = 0; i < argc; i++)
		if(argv[i].substr(0, 7) == "import=")
			return import_areas(argv[i].substr(7));

	uifc.init("TickIT Config Program");
	js.on_exit("uifc.bail();");
	js.on_exit("if(unexpected_exit) uifc.msg('Abnormal Exit');");

	var cmd = 0;
	var ctx = new uifc.list.CTX();

	while(cmd >= 0) {
		var menu = ["Global Options","Per Area Options"];
		cmd = uifc.list(WIN_ORG|WIN_ACT|WIN_MID|WIN_ESC, "TickIT Options", menu, ctx);
		switch(cmd) {
			case 0:
				edit_globals(tickit.gcfg);
				break;
			case 1:
				edit_areas();
				break;
			case -1:
				// TODO: ask to save ini only if changes were made.
				switch(uifc.list(WIN_MID|WIN_SAV, "Write INI", ["Yes", "No"])) {
					case 0:
						var result = tickit.save();
						if(result !== true)
							uifc.msg(result);
						/* fall-through */
					case 1:
						uifc.pop("Exiting");
						unexpected_exit = false;
						return;
					default:
						cmd = 0;
						break;
				}
				break;
			default:
				uifc.msg("Unhandled Return: "+cmd);
				break;
		}
	}
}

main();
