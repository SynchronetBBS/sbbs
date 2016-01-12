load("uifcdefs.js");
load("fidocfg.js");

var cfg=new FREQITCfg();

var dctx = {};
var gctx = new uifc.list.CTX();
function pick_dir()
{
	var cmd = 0;
	var libs = Object.keys(file_area.lib);

	function do_pick_dir(grp) {
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
			dir = uifc.list(WIN_BOT|WIN_SAV|WIN_ACT, "Select Dir", dirs, dctx[grp]);
			if (dir >= 0)
				return dircodes[dir];
		}
		return undefined;
	}

	while (cmd >= 0) {
		cmd = uifc.list(WIN_RHT|WIN_SAV|WIN_ACT, "Select Group" , libs, gctx);
		if (cmd >= 0) {
			file = do_pick_dir(libs[cmd]);
			if (file !== undefined)
				return file;
		}
	}
	return undefined;
}

var dirctx = new uifc.list.CTX();
function edit_dirs(list)
{
	var dir=0;
	var dirs;

	while(dir >= 0) {
		dirs = list.map(function(v){return file_area.dir[v].name;});
		dir = uifc.list(WIN_INS|WIN_INSACT|WIN_DEL|WIN_XTR|WIN_SAV, "Directories", dirs, dirctx);
		if (dir == -1)
			return undefined;
		if (dir == dirs.length-1 || dir & MSK_INS) {
			dir &= MSK_OFF;
			var newdir=pick_dir();
			if (newdir)
				list.splice(dir, 0, newdir);
		}
		else if (dir & MSK_DEL) {
			dir &= MSK_OFF;
			list.splice(dir, 1);
		}
	}
	return undefined;
}

function main()
{
	var cmd=0;
	var ctx = new uifc.list.CTX();
	var opts;

	uifc.init("FREQIT Config");
	while (cmd >= 0) {
		opts = ["Dirs...", "Secure Dirs...", format("Max Files (%d)", cfg.maxfiles), "Magic Names..."];
		cmd = uifc.list(WIN_ACT|WIN_ORG|WIN_MID, "FREQIT Options", opts, ctx);
		switch(cmd) {
			case 0:		// Dirs
				edit_dirs(cfg.dirs);
				break;
			case 1:		// Secure Dirs
				edit_dirs(cfg.securedirs);
				break;
			case 2:		// Max Files
			case 3:		// Magic Names
			case -1:	// Done
				// TODO: Save.
				uifc.bail();
		}
	}
}

main();
