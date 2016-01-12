load("sbbsdefs.js");
load("uifcdefs.js");
load("fidocfg.js");

var cfg=new FREQITCfg();

// Backward compatability hack.
if (typeof uifc.list.CTX === "undefined") {
	uifc.list.CTX = function () {
		this.cur = 0;
		this.bar = 0;
	}
}

/*
 * Pass false if you will not be immediately redrawing your menu
 */
var dctx = {};
var gctx = new uifc.list.CTX();
function pick_dir(act)
{
	var cmd = 0;
	var libs = Object.keys(file_area.lib);
	var actflags = act ? WIN_ACT : 0;

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
			dir = uifc.list(WIN_BOT|WIN_SAV|actflags, "Select Dir", dirs, dctx[grp]);
			if (dir >= 0)
				return dircodes[dir];
		}
		return undefined;
	}

	while (cmd >= 0) {
		cmd = uifc.list(WIN_RHT|WIN_SAV||actflags, "Select Group" , libs, gctx);
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

	while (dir >= 0) {
		dirs = list.map(function(v){return file_area.dir[v].name;});
		dir = uifc.list(WIN_ACT|WIN_INS|WIN_INSACT|WIN_DEL|WIN_XTR|WIN_SAV, "Directories", dirs, dirctx);
		if (dir == -1)
			return undefined;
		if (dir == dirs.length || dir & MSK_INS) {
			dir &= MSK_OFF;
			var newdir=pick_dir(true);
			if (newdir) {
				if (list.indexOf(newdir) !== -1)
					uifc.msg(newdir+" already in list");
				else
					list.splice(dir, 0, newdir);
			}
		}
		else if (dir & MSK_DEL) {
			dir &= MSK_OFF;
			list.splice(dir, 1);
		}
		else {
			// TODO... something when it's selected...
		}
	}
	return undefined;
}

function edit_magic(magic, sect)
{
	var cmd=0;
	var ctx = new uifc.list.CTX();
	var opts;
	var val;

	while(cmd >= 0) {
		opts = ["Dir:    "+magic.dir,
			"Secure: "+(magic.secure?'Yes':'No'),
			"Match:  "+magic.match];
		cmd = uifc.list(WIN_ACT|WIN_BOT|WIN_SAV, "Edit "+sect, opts, ctx);
		switch(cmd) {
			case 0:
				val = pick_dir(true);
				if (val != undefined)
					magic.dir = val;
				break;
			case 1:
				magic.secure = !magic.secure;
				break;
			case 2:
				val = uifc.input(WIN_MID|WIN_SAV, "Wildcard Match", magic.match, K_EDIT);
				if (val != undefined) {
					if (val.length == 0)
						uifc.msg("Invalid match!");
					else
						magic.match = val;
				}
				break;
			default:
				break;
		}
	}
}

var mctx = new uifc.list.CTX();
function edit_magics()
{
	var magic=0;
	var magics;
	var val;
	var dir;
	var mask;
	var secure;

	while (magic >= 0) {
		magics = Object.keys(cfg.magic);
		magic = uifc.list(WIN_ACT|WIN_INS|WIN_INSACT|WIN_DEL|WIN_XTR|WIN_SAV, "Magic Names", magics, mctx);
		if (magic == -1)
			return;
		if (magic == magic.length || magic & MSK_INS) {
			magic &= MSK_OFF;
			// TODO: Does WIN_RHT not work for input fields?
			val = uifc.input(WIN_MID|WIN_RHT|WIN_SAV, "Magic Name", 20);
			if (val != undefined && val !== '') {
				if (Object.keys(cfg.magic).map(function(v){return v.toUpperCase();}).indexOf(val.toUpperCase()) >= 0) {
					uifc.msg("Magic name '"+val+"' already exists!");
				}
				else {
					dir = pick_dir(false);
					if (dir != undefined) {
						mask = uifc.input(WIN_MID|WIN_SAV, "Wildcard Match");
						if (mask != undefined && mask.length > 0) {
							switch(uifc.list(WIN_MID|WIN_SAV, "Secure Only?", ["No", "Yes"])) {
								case 0:
									secure=false;
									break;
								case 1:
									secure=true;
									break;
								case -1:
									continue;
							}
						}
						cfg.magic[val]={dir:dir, secure:secure, mask:mask};
					}
				}
			}
		}
		else if (magic & MSK_DEL) {
			magic &= MSK_OFF;
			delete cfg.magic[magics[magic]];
		}
		else {
			edit_magic(cfg.magic[magics[magic]], magics[magic]);
		}
	}
}

function main()
{
	var cmd=0;
	var ctx = new uifc.list.CTX();
	var opts;
	var val;

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
				val = uifc.input(WIN_MID|WIN_SAV, "Max Files", cfg.maxfiles.toString(), 10, K_NUMBER|K_EDIT);
				if (val != undefined)
					cfg.maxfiles=parseInt(val, 10);
				break;
			case 3:		// Magic Names
				edit_magics();
				break;
			case -1:	// Done
				switch(uifc.list(WIN_ACT|WIN_MID, 'Save File', ['Yes', 'No'])) {
					case 0:
						uifc.bail();
						cfg.save();
						break;
					case 1:
						uifc.bail();
						break;
					case -1:
						cmd = 0;
						break;
				}
		}
	}
}

main();
