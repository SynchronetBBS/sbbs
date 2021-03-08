// Add files to a file base/area directory

require("sbbsdefs.js", 'LEN_FDESC');

"use strict";

const default_excludes = [
	"FILES.BBS",
	"FILE_ID.DIZ",
	"DESCRIPT.ION",
	"SFFILES.BBS"
];

if(argv.indexOf("-help") >= 0 || argv.indexOf("-?") >= 0) {
	print("usage: [-options] [dir-code] [listfile]");
	exit(0);
}

var listfile;
var options = {};
var exclude = [];
var dir_list = [];
var verbosity = 0;
for(var i = 0; i < argc; i++) {
	var arg = argv[i];
	if(arg[0] == '-') {
		if(arg.indexOf("-ex=") == 0) {
			exclude.push(arg.slice(4).toUpperCase());
			continue;
		}
		if(arg == '-' || arg == '-all') {
			for(var dir in file_area.dir)
				dir_list.push(dir);
			continue;
		}
		if(arg[1] == 'v') {
			var j = 1;
			while(arg[j++] == 'v')
				verbosity++;
			continue;
		}
		options[arg.slice(1)] = true;
	} else {
		if(!dir_list.length)
			dir_list.push(arg);
		else
			listfile = arg;
	}
}

if(exclude.length < 1)
	exclude = default_excludes;
if(listfile)
	exclude.push(listfile.toUpperCase());

if(!dir_list.length) {
	var code;
	while(!file_area.dir[code] && !js.terminated) {
		for(var d in file_area.dir)
			print(d);
		code = prompt("Directory code");
	}
	dir_list.push(code);
}

var added = 0;
for(var d = 0; d < dir_list.length; d++) {
	
	var code = dir_list[d];
	var dir = file_area.dir[code];
	if(!dir) {
		alert("Directory '" + code + "' does not exist in configuration");
		continue;
	}
	print("Adding files to " + dir.lib_name + " " + dir.name);
	
	var filebase = new FileBase(code);
	if(!filebase.open("r")) {
		alert("Failed to open: " + filebase.file);
		continue;
	}

	var name_list = filebase.get_file_names();
	// Convert to uppercase
	for(var i = 0; i < name_list.length; i++) {
		name_list[i] = name_list[i].toUpperCase();
		if(options.debug)
			print(name_list[i]);
	}
	var file_list = [];

	if(listfile) {
		var listpath = file_getcase(dir.path + listfile);
		if(!listpath) {
			var tmp = file_getcase(listfile);
			if(tmp)
				listpath = tmp;
		}
		var f = new File(file_getcase(listpath));
		if(f.exists) {
			print("Opening " + f.name);
			if(!f.open('r')) {
				alert("Error " + f.error + " (" + strerror(f.error) + ") opening " + f.name);
				exit(1);
			}
			file_list = parse_file_list(f.readAll());
			f.close();
		} else {
			alert(dir.path + listfile + " does not exist");
		}
	}
	else {
		var list = directory(dir.path + '*');
		for(var i = 0; i < list.length; i++) {
			if(!file_isdir(list[i]))
				file_list.push({ name: file_getname(list[i]) });
		}
	}
	
	for(var i = 0; i < file_list.length; i++) {
		var file = file_list[i];
		if(options.debug)
			print(JSON.stringify(file, null, 4));
		else if(verbosity)
			printf("%s ", file.name);
		if(exclude.indexOf(file.name.toUpperCase()) >= 0) {
			if(verbosity)
				print("excluded (ignored)");
			continue;
		}
		if(name_list.indexOf(file.name.toUpperCase()) >= 0) {
			if(verbosity)
				print("already added");
			continue;
		}
		var path = file_area.dir[code].path + file.name;
		if(!file_exists(path)) {
			alert("does not exist: " + path);
			continue;
		}
		//if(!(file_area.dir[code].settings & DIR_FREE)) ??
		file.cost = file_size(path);
		// Add file here:
		if(!filebase.add_file(file)) {
			alert("Error " + filebase.last_error + " adding " + file.name);
		} else {
			print("Added " + file.name);
			added++;
		}
	}
	
	filebase.close();
}
print(added + " files added");

function parse_file_list(lines)
{
	var file_list = [];
	for(var i = 0; i < lines.length; i++) {
		var line = lines[i];
		var match = line.match(/(^[\w]+[\w\-\.]*)\W+[^A-Za-z]*(.*)/);
		if(match && match.length > 1) {
			var file = { name: match[1], desc: match[2] };
			if(file.desc && file.desc.length > LEN_FDESC)
				file.extdesc = word_wrap(file.desc, 45);
			file_list.push(file);
			continue;
		}
		match = line.match(/\W+\|\W+(.*)/);
		if(!match)
			continue;
		print('match: ' + JSON.stringify(match));
		if(match && match.length > 1 && file_list.length) {
			var file = file_list[file_list.length - 1];
			if(!file.extdesc)
				file.extdesc = file.desc + "\n";
			file.extdesc += match[1] + "\n";
		}
	}
	return file_list;
}
