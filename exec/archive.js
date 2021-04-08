// Deal with archive files using Synchronet v3.19 Archive class

// Install "Viewable File Types" using 'jsexec archive.js install'

"use strict";

var cmd = argv.shift();
var fname = argv.shift();
var verbose = false;
var i = argv.indexOf('-v');
if(i >= 0) {
	verbose = true;
	argv.splice(i, 1);
}
switch(cmd) {
	case 'list':
		list(fname, verbose);
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
		print(Archive(fname).type);
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
		 list = Archive(filename).list(verbose);
	} catch(e) {
		alert(file_getname(filename) + ": Unsupported archive format");
		return;
	}
	
	var dir_fmt = "\x01n%s";
	var file_fmt = "\x01n \x01c\x01h%-*s \x01n\x01c%10lu  ";
	if(verbose)
		file_fmt += "\x01h%08lX  ";
	file_fmt += "\x01h\x01w%s";
	if(!js.global.console) {
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
					,system.timestr(list[i].time).slice(4)));
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
	var viewable_exts = [
		'7z',
		'exe',
		'bz',
		'gz',
		'iso',
		'lha',
		'lzh',
		'tbz',
		'tgz',
		'rar',
		'xar',
		'zip'
	];
	
	var cnflib = load({}, "cnflib.js");
	var file_cnf = cnflib.read("file.cnf");
	if(!file_cnf) {
		alert("Failed to read file.cnf");
		exit(-1);
	}
	for(var e in viewable_exts) {
		file_cnf.fview.push({
			extension: viewable_exts[e],
			cmd: '?archive list %f'
			});
	}
	if(!cnflib.write("file.cnf", undefined, file_cnf)) {
		alert("Failed to write file.cnf");
		exit(-1);
	}
	exit(0);
}	
