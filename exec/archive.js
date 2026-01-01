// Deal with archive files using Synchronet v3.19 Archive class
// @format.tab-size 4, @format.use-tabs true

// Install "Viewable File Types" using 'jsexec archive.js install'

"use strict";

var cmd = argv.shift();
var verbose = 0;
var json = false;
var sort = false;
var i;
while((i = argv.indexOf('-v')) >= 0) {
	verbose++;
	argv.splice(i, 1);
}
if((i = argv.indexOf('-json')) >= 0) {
	json = true;
	argv.splice(i, 1);
}
if((i = argv.indexOf('-sort')) >= 0) {
	sort = true;
	argv.splice(i, 1);
}
var fname = [ argv.shift() ];

switch(cmd) {
	case 'list':
		fname = fname.concat(argv);
		for (i in fname) {
			if (fname.length > 1)
				writeln(fname[i]);
			list(fname[i], verbose);
		}
		break;
	case 'json':
		writeln(JSON.stringify(Archive(fname).list(verbose, argv[0]), null, 4));
		break;
	case 'create':
		print(Archive(fname).create(directory(argv[0])) + " files archived");
		break;
	case 'extract':
		var a = Archive(fname);
		print(a.extract.apply(a, argv) + " files extracted");
		break;
	case 'read':
		print(Archive(fname).read(argv[0]));
		break;
	case 'type':
		fname = fname.concat(argv);
		for (i in fname) {
			if (fname.length > 1)
				writeln(fname[i]);
			print(Archive(fname[i]).type);
		}
		break;
	case 'install':
		install();
		break;
	default:
		throw new Error("invalid command: " + cmd);
}

function list(filename, verbose)
{
	var list;
	try {
		 list = Archive(filename).list(Boolean(verbose));
	} catch(e) {
		log(LOG_DEBUG, filename + " " + e);
		alert(file_getname(filename) + ": Unsupported archive");
		return;
	}

	if(sort)
		list.sort(function(a,b) { if(a.name < b.name) return -1; return a.name > b.name; } );

	if(json) {
		writeln(JSON.stringify(list, null, 4));
		return;
	}

	var dir_fmt = "\x01n%s";
	var file_fmt = "\x01n \x01c\x01h%-*s \x01n\x01c%10lu  ";
	if(verbose)
		file_fmt += "\x01h%08lX  ";
	file_fmt += "\x01h\x01w%s";
	if(verbose > 1)
		file_fmt += "  %s";
	if(js.global.console) {
		console.cond_newline();
	} else {
		dir_fmt = strip_ctrl(dir_fmt);
		file_fmt = strip_ctrl(file_fmt);
	}
	var longest_name = 0;
	for(var i = 0; i < list.length; i++) {
		longest_name = Math.max(longest_name, file_getname(list[i].name).length);
	}
	var curpath;
	for(var i = 0; i < list.length && !js.terminated && (!js.global.console || !console.aborted); i++) {
		if(list[i].type != "file")
			continue;
		else {
			var fname = file_getname(list[i].name);
			var path = list[i].name.slice(0, -fname.length);
			if(path != curpath)
				writeln(format(dir_fmt, path ? path : "[root]"));
			if(verbose)
				writeln(format(file_fmt
					,longest_name, fname, list[i].size, list[i].crc32
					,system.timestr(list[i].time).slice(4)
					,list[i].format));
			else
				writeln(format(file_fmt
					,longest_name, fname, list[i].size
					,system.timestr(list[i].time).slice(4)));
			curpath = path;
		}
	}
}

function install()
{
	var f = new File(system.ctrl_dir + "file.ini");
	if(!f.open(f.exists ? 'r+':'w+')) {
		alert("Failed to open " + f.name);
		exit(-1);
	}

	if(!f.iniSetObject("viewer:new", {
		extension: "*",
		cmd: '?archive list %f'
		})) {
		alert("Failed to write " + f.name);
		exit(-1);
	}
	f.close();
	exit(0);
}
