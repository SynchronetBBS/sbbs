load("sbbsdefs.js");
load("uifcdefs.js");
load("tickit_objs.js");
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
				return dirs[dir];
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
				dir = uifc.input(WIN_SAV|WIN_MID, "Path", tickit.gcfg.path === undefined ? '' : tickit.gcfg.path, 1024, K_EDIT);
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

function edit_area(obj, name)
{
	var cmd = 0;
	var link = 0;
	var links;
	var menu = ["Location", "Links"];
	var tmp;
	var tmp2;
	var ctx = new uifc.list.CTX();

	while(cmd >= 0) {
		cmd = uifc.list(WIN_SAV|WIN_ACT|WIN_BOT|WIN_RHT, name+" Options", menu, ctx);
		switch(cmd) {
			case 0:
				set_location(obj);
				break;
			case 1:
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
	var area = 0;
	var tmp;
	var ctx = new uifc.list.CTX();

	while(area >= 0) {
		areas = Object.keys(tickit.acfg).sort();
		area = uifc.list(WIN_SAV|WIN_BOT|WIN_ACT|WIN_DEL|WIN_INS|WIN_DELACT|WIN_XTR, "Select Area", areas, ctx);
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

function main()
{
	uifc.init("TickIT Config Program");

	var cmd = 0;
	var link = 0;
	var links;
	var menu = ["Global Location", "Global Links", "Areas..."];
	var tmp;
	var tmp2;
	var ctx = new uifc.list.CTX();

	while(cmd >= 0) {
		cmd = uifc.list(WIN_ORG|WIN_ACT|WIN_MID, "Global Options", menu, ctx);
		switch(cmd) {
			case 0:
				set_location(tickit.gcfg);
				break;
			case 1:
				if (tickit.gcfg.links === undefined)
					links = [];
				else
					links = tickit.gcfg.links.split(/,/);
				tmp = edit_links(links);
				if (tmp.length === 0)
					delete tickit.gcfg.links;
				else
					tickit.gcfg.links = tmp.join(',');
				break;
			case 2:
				edit_areas();
				uifc.bail();
				break;
			case -1:
				// TODO: Save changes?
				break;
			default:
				uifc.msg("Unhandled Return: "+cmd);
				break;
		}
	}
}

main();
