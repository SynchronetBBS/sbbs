#!/usr/bin/env -S jsexec -n

load("sbbsdefs.js");
load("uifcdefs.js");
load("fidocfg.js");
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
		var filedir = new OldFileBase(dir);
		var files;
		var file;

		if (fctx[dir] === undefined)
			fctx[dir] = new uifc.list.CTX();

		files = filedir.map(function(v){return format("%-12s - %s", v.name, v.desc);});
		file = uifc.list(WIN_SAV|WIN_ACT|WIN_RHT, "Select File", files, fctx[dir]);
		if (file >= 0)
			return filedir[file];
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

function pick_area()
{
	areas = Object.keys(tickit.acfg).sort();
	areas = areas.map(function(v){return v.toUpperCase();});
	area = uifc.list(WIN_ORG|WIN_MID, "Select Area", areas);
	if (area >= 0)
		return areas[area];
	return undefined;
}

function pick_origin()
{
	var addr = uifc.list(WIN_ORG|WIN_MID, "Select Origin", system.fido_addr_list);
	if (addr >= 0)
		return system.fido_addr_list[addr];
	return undefined;
}

function add_links(seenbys, links, list)
{
	var l;
	var i;

	l = list.split(/,/);
	for (i=0; i<l.length; i++) {
		if (seenbys[l[i]] !== undefined) {
			log(LOG_DEBUG, "Node "+l[i]+" has already seen this.");
			continue;
		}
		links[l[i]]='';
	}
}

function file_crc(fname)
{
	var f = new File(fname);
	var crc;

	if (!f.open("rb")) {
		uifc.msg("Unable to open file '"+f.name+"'.");
		log(LOG_WARNING, "Unable to open file '"+f.name+"'.");
		return false;
	}
	crc32 = crc32_calc(f.read());
	f.close();
	return crc32;
}

function parse_addr(addr)
{
	var m;
	var ret={};

	m = addr.match(/^([0-9]+):/);
	if (m !== null)
		ret.zone = parseInt(m[1], 10);

	m = addr.match(/([0-9]+)\//);
	if (m !== null)
		ret.net = parseInt(m[1], 10);

	m = addr.match(/\/([0-9]+)/);
	if (m !== null)
		ret.node = parseInt(m[1], 10);

	m = addr.match(/\.([0-9]+)/);
	if (m !== null)
		ret.point = parseInt(m[1], 10);

	m = addr.match(/@.+$/);
	if (m !== null)
		ret.domain = m[1];

	return ret;
}

function get_zone(addr)
{
	var m;

	// Figure out the default zone.
	m = addr.match(/^([0-9]+):/);
	if (m===null)
		return undefined;
	return parseInt(m[1], 10);
}

function hatch_file(file, area, origin, replaces)
{
	var seenbys={};
	var links={};
	var cfg;
	var link;
	var tf;
	var ff;
	var bf;
	var defzone;
	var addr;
	var outb;
	var flobase;
	var pw;
	var i;
	var tic = {path:[], seenby:[]};
	var lfile;
	var ldesc;

	defzone = get_zone(system.fido_addr_list[0]);
	if (defzone === undefined) {
		log(LOG_ERROR, "Unable to detect default zone!");
		return false;
	}

	// Add us to the path...
	tic.path.push(system.fido_addr_list[0]);

	// Add all our addresses...
	system.fido_addr_list.forEach(function(addr) {
		seenbys[addr]='';
	});

	// Calculate links
	if (tickit.gcfg.links !== undefined)
		add_links(seenbys, links, tickit.gcfg.links);
	cfg = tickit.acfg[area.toLowerCase()];
	if (cfg !== undefined && cfg.links !== undefined)
		add_links(seenbys, links, cfg.links);

	// Add links to seenbys
	for (i in links)
		tic.seenby.push(i);

	// Now, start generating the TIC/FLO files...
	for (link in links) {
		if (!sbbsecho.is_flo) {
			log(LOG_ERROR, "TickIT doesn't support non-FLO mailers.");
			return false;
		}

		pw = sbbsecho.get_ticpw(FIDO.parse_addr(link).toString());

		if (pw===undefined)
			pw = '';

		// Figure out the outbound dir...
		addr = parse_addr(link);
		if (addr.zone === undefined)
			addr.zone = defzone;

		if (addr.zone === undefined || addr.net === undefined || addr.node === undefined) {
			log(LOG_ERROR, "Address '"+link+"' is invalid!");
			continue;
		}

		outb = sbbsecho.outbound.replace(/[\\\/]+$/g, '');
		if (addr.zone !== defzone)
			outb += format(".%03x", addr.zone);

		if (addr.point !== undefined) {
			outb += format("/%04x", addr.net);
			outb += format("%04x", addr.node);
			outb += ".pnt"
		}

		outb = fullpath(outb);
		outb = backslash(outb);

		// Create TIC file first...
		tf = new File(outb+tickit.get_next_tic_filename());
		if(!tf.open("wb")) {
			log(LOG_ERROR, "Unable to create TIC file for "+link+".  He will not get file '"+file.name+"'!");
			continue;
		}
		tf.write('Area '+area+'\r\n');
		tf.write('Origin '+origin+'\r\n');
		tf.write('From '+origin+'\r\n');
		tf.write('To '+link+'\r\n');
		tf.write('File '+file.name+'\r\n');
		if (file_getcase(file.path).length > file.path.length) {
			lfile = file_getcase(file.path);
			lfile.replace(/^.*\\\/([^\\\/]+)$/,'$1');
			tf.write('Lfile '+lfile+'\r\n');
		}
		if (replaces)
			tf.write('Replaces ' + replaces + '\r\n');
		tf.write('Size '+file_size(file.path)+'\r\n');
		tf.write('Date '+file_date(file.path)+'\r\n');
		tf.write('Desc '+file.desc+'\r\n');
		if (file.extdesc !== undefined) {
			ldesc = file.extdesc.split(/\r?\n/);
			ldesc.forEach(function(line) {
				tf.write('Ldesc '+line+'\r\n');
			});
		}
		tf.write('Created by TickIT '+"$Revision: 1.6 $".split(' ')[1]+'\r\n');
		tf.printf('Crc %08lX\r\n', file_crc(file.path));
		for (i=0; i<tic.path.length; i++)
			tf.write('Path '+tic.path[i]+'\r\n');
		for (i=0; i<tic.seenby.length; i++) {
			if(tic.seenby[i] != link)
				tf.write('Seenby '+tic.seenby[i]+'\r\n');
		}
		tf.write('Pw '+pw+'\r\n');
		tf.close();

		// Create bsy file...
		if (addr.point !== undefined) {
			flobase = outb+format("0000%04x", addr.point);
		} else {
			flobase = outb+format("%04x%04x", addr.net, addr.node);
		}
		bf = new File(flobase+'.bsy');
		while (!bf.open('wxb+')) {
			// TODO: This waits forever...
			log(LOG_WARNING, "Waiting for BSY file '"+bf.name+"'...");
			mswait(1000);
		}

		// Append to FLO file...
		ff = new File(flobase+'.flo');
		if (!ff.open('ab+')) {
			log(LOG_ERROR, "Unable to append to '"+ff.name+"' for "+link+".  He will not get file '"+file.name+"'!");
			bf.close();
			bf.remove();
			continue;
		}
		ff.writeln(file.path);
		ff.writeln('^'+tf.name);
		ff.close();
		bf.close();
		bf.remove();
	}

	return true;
}

function interactive() {
	var file;
	var area;
	var origin;
	var replaces;

	uifc.init('HatchIT');
	js.on_exit('uifc.bail()');
	file = pick_file();
	if (file === undefined || file.path === undefined)
		return;
	area = pick_area();
	if (area === undefined)
		return;
	origin = pick_origin();
	if (origin === undefined)
		return;
	replaces = uifc.input(WIN_ORG|WIN_MID, "Replaces (ENTER=none)");
	if(replaces === undefined)
		return;
	var msg = 'Hatch file `'+file.name+'` into `'+area+'` from `'+origin+'`\r\n\r\n'+
		'Desc: `'+file.desc+'`';
	if (file.extdesc)
		msg += '\r\n\r\nLong Desc:\r\n`' + file.extdesc +'`';
	if (replaces)
		msg += "\r\n\r\nReplaces: `" + replaces + '`';
	if (uifc.showhelp !== undefined) {
		uifc.help_text = msg;
		uifc.showhelp();
	}
	if (uifc.list(WIN_ORG|WIN_MID, "Proceed?", ["No", "Yes"]) == 1) {
		hatch_file(file, area, origin, replaces);
	}
}

function main() {
	var dir;
	var file;
	var area;
	var origin;
	var replace;

	for(var i = 0; i < argc; i++) {
		var arg = argv[i];
		if(arg[0] == '-') {
			var opt = arg;
			while(opt[0] == '-')
				opt = opt.slice(1);
			if(opt == "help" || opt == "?" || opt == "h") {
				writeln("usage: hatchit.js [option]");
				writeln("options:");
				writeln("  -dir=<filedir>    File directory Internal Code");
				writeln("  -file=<filename>  Name of file to hatch");
				writeln("  -area=<areatag>   File area of file to hatch");
				writeln("  -origin=<FTN AKA> Origin FTN AKA");
				writeln("  -replace          File replaces older version");
				writeln("");
				writeln("If no option is given, HatchIT will start in interactive mode.");
				exit(0);
			}
			if(opt.indexOf("dir=") == 0) {
				var indir = opt.slice(4);
				dir = file_area.dir[indir.toLowerCase()];
				if (!dir) {
					alert("File directory not found: " + indir);
					exit(1);
				}
				continue;
			}
			if(opt.indexOf("area=") == 0) {
				var inarea = opt.slice(5);
				area = inarea.toUpperCase();
				if (!tickit.acfg[inarea.toLowerCase()]) {
					alert("File area not found: " + area);
					exit(1);
				}
				continue;
			}
			if(opt.indexOf("file=") == 0) {
				infile = opt.slice(5);
				var fb = new OldFileBase(dir.code.toLowerCase());
				fb.forEach(function(f) {
					if (f.name == infile) {
						file = f;
					}
				});
				if (!file) {
					alert("Cannot open file: " + infile);
					exit(1);
				}
				continue;
			}
			if(opt.indexOf("origin=") == 0) {
				origin = opt.slice(7);
				if (system.fido_addr_list.indexOf(origin) == -1) {
					alert("Origin is not in list of AKAs: " + origin);
					exit(1);
				}
				continue;
			}
			if(opt.indexOf("replace") == 0) {
				replace = true;
			} else {
				replace = false;
			}
			continue;
		}
	}

	if (dir == undefined && area == undefined && file == undefined && origin == undefined && replace == undefined)
		interactive()

	writeln("File   : " + file.name);
	writeln("Area   : " + area);
	writeln("Origin : " + origin);
	writeln("Desc   : " + file.desc);
	if (replace) writeln("Replace: yes");
	else writeln("Replace: no");
	if (file.extdesc) writeln("LDesc : " + file.extdesc);

	hatch_file(file, area, origin, replace);
}

main();
