load("sbbsdefs.js");
load("uifcdefs.js");
load("tickit_objs.js");
load("filebase.js");

// Backward compatability hack.
if (typeof uifc.list.CTX === "undefined") {
	uifc.list.CTX = function () {
		this.cur = 0;
		this.bar = 0;
	}
}

var sbbsecho = new SBBSEchoCfg();
var tickit = new TickITCfg();

function pick_file()
{
	var cmd = 0;
	var libs = Object.keys(file_area.lib);
	var ctx = new uifc.list.CTX();
	var dctx = {};
	var fctx = {};

	function do_pick_file(dir) {
		var filedir = new FileBase(dir);
		var files;
		var file;

		if (fctx[dir] === undefined)
			fctx[dir] = new uifc.list.CTX();

		files = filedir.map(function(v){return format("%-12s - %s", v.name, v.desc);});
		file = uifc.list(WIN_SAV|WIN_ACT|WIN_RHT, "Select File", files, fctx[dir]);
		if (file >= 0)
			return files[file];
		return undefined;
	}

	function pick_dir(grp) {
		var dirs;
		var dircodes;
		var dir;
		var ret;

		dir = 0;
		while (dir >= 0) {
			if (dctx[grp] === undefined)
				dctx[grp] = new uifc.list.CTX();

			dircodes = file_area.lib[libs[cmd]].dir_list.map(function(v){return v.code;});
			dirs = dircodes.map(function(v){return file_area.dir[v].name;});
			dir = uifc.list(WIN_SAV|WIN_ACT, "Select Dir", dirs, dctx[grp]);
			if (dir >= 0) {
				ret = do_pick_file(dircodes[dir]);
				if (ret !== undefined)
					return ret;
			}
		}
		return undefined;
	}

	while (cmd >= 0) {
		cmd = uifc.list(WIN_SAV|WIN_ACT|WIN_MID, "Select Group" , libs, ctx);
		if (cmd >= 0) {
			file = pick_dir(libs[cmd]);
			if (file !== undefined)
				return file;
		}
	}
	return undefined;
}

function main() {
	uifc.init('HatchIT');
	log("Hatching file: "+pick_file());
	uifc.bail();
}

main();
