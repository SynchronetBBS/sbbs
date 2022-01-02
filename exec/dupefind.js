// Find duplicate files in a Synchronet v3.19 file bases

// Can search for duplicate file names and/or hash sums through one or more
// directories with simple text or JSON-formatted output

load("file_size.js");

"use strict";

var detail = FileBase.DETAIL.NORM;
var min_size = 1024;
var dir_list = [];
var exclude = [];
var json_space = 4;
var hash_type;
var options = {};
for(var i = 0; i < argc; i++) {
	var arg = argv[i];
	if(arg[0] == '-') {
		var opt = arg;
		while(opt[0] == '-')
			opt = opt.slice(1);
		if(opt == "help" || opt == "?") {
			writeln("usage: [-options] [[dir_code] [...]]");
			writeln("options:");
			writeln("  -lib=<name>     search for duplicates in specified library only");
			writeln("  -min=<bytes>    specify minimum file size to compare hash/sum");
			writeln("  -ex=<filename>  add to excluded file name list (case-insensitive)");
			writeln("  -names          search for duplicate file names (case-insensitive)");
			writeln("  -arc            search for duplicate contents of archive files");
			writeln("  -hash           hash each archived file contents for comparison");
			writeln("  -sha1           search for duplicate SHA-1 sums (the default)");
			writeln("  -crc32          search for duplicate CRC-32 sums");
			writeln("  -md5            search for duplicate MD5 sums");
			writeln("  -json[=space]   create JSON formatted output");
			writeln("  -v              increase verbosity of JSON output");
			writeln("  -sort           sort each directory file list");
			writeln("  -reverse        reverse each directory file list");
			writeln("  -dedupe         remove/delete duplicate files");
			exit(0);
		}
		if(opt.indexOf("ex=") == 0) {
			exclude.push(opt.slice(3).toUpperCase());
			continue;
		}
		if(opt.indexOf("lib=") == 0) {
			var lib = opt.slice(4);
			if(!file_area.lib[lib]) {
				alert("Library not found: " + lib);
				exit(1);
			}
			for(var j = 0; j < file_area.lib[lib].dir_list.length; j++)
				dir_list.push(file_area.lib[lib].dir_list[j].code);
			continue;
		}
		if(opt.indexOf("min=") == 0) {
			min_size = parseInt(opt.slice(4), 10);
			continue;
		}
		if(opt[0] == 'v') {
			var j = 0;
			while(opt[j++] == 'v')
				detail++;
			continue;
		}
		if(opt.indexOf("json=") == 0) {
			options.json = true;
			json_space = parseInt(opt.slice(5), 10);
			continue;
		}
		switch(opt) {
			case 'crc16':
			case 'crc32':
			case 'md5':
			case 'sha1':
				hash_type = opt;
				break;
		}
		options[opt] = true;
		continue;
	}
	dir_list.push(arg);
}
if(dir_list.length < 1)
	for(var dir in file_area.dir)
		dir_list.push(dir);
if(!options.names && !options.arc && !hash_type)
	hash_type = "sha1";

log("Reading file areas...");
var name = {};
var hash = {};
var arc = {};
var total_files = 0;
var total_bytes = 0;
for(var i in dir_list) {
	var dir_code = dir_list[i];
	var dir = file_area.dir[dir_code];
	var base = new FileBase(dir_code);
	if(!base.open())
		throw new Error(base.last_error);
	var list = base.get_list(detail, options.sort);
	if(options.reverse)
		list.reverse();
	for(var j = 0; j < list.length; j++) {
		var file = list[j];
		if(exclude.indexOf(file.name.toUpperCase()) >= 0)
			continue;
		file.dir = dir_code;
		file.path = base.get_path(file.name);
		if(options.names) {
			var fname = file.name.toLowerCase();
			if(!name[fname])
				name[fname] = [];
			name[fname].push(file);
		}
		if(file.size >= min_size && file[hash_type] !== undefined) {
			if(!hash[file[hash_type]])
				hash[file[hash_type]] = [];
			hash[file[hash_type]].push(file);
		}
		if(options.arc) {
			var contents = undefined;
			try {
				var contents = new Archive(file.path).list(options.hash);
				contents.sort(function(a,b) { if(a.name < b.name) return -1; return a.name > b.name; } );
				for(var a = 0; a < contents.length; a++) {
					delete contents[a].format;
					delete contents[a].compression;
					delete contents[a].mode;
					if(options.hash)
						delete contents[a].time;
				}
			} catch(e) { }
			if(contents) {
				var key = sha1_calc(JSON.stringify(contents));
				if(!arc[key])
					arc[key] = [];
				arc[key].push(file);
			}
			contents = undefined;
		}
		total_bytes += file.size;
	}
	base.close();
	total_files += list.length;
}

log("Searching for duplicates in " + total_files + " files (" 
	+ file_size_str(total_bytes, /* unit */1, /* precision */1) + " bytes) ...");
var dupe = { name: [], hash: [], arc: []};
var name_bytes = 0;
var hash_bytes = 0;
var arc_bytes = 0;
for(var n in name) {
	var f = name[n];
	if(f.length <= 1)
		continue;
	dupe.name.push(f);
	for(var i = 1; i < f.length; i++)
		name_bytes += f[i].size;
}

for(var n in hash) {
	var f = hash[n];
	if(f.length <= 1)
		continue;
	dupe.hash.push(f);
	for(var i = 1; i < f.length; i++)
		hash_bytes += f[i].size;
}

for(var n in arc) {
	var f = arc[n];
	if(f.length <= 1)
		continue;
	dupe.arc.push(f);
	for(var i = 1; i < f.length; i++)
		arc_bytes += f[i].size;
}

if(options.names) {
	log(dupe.name.length + " duplicate file names (" + file_size_str(name_bytes,1 , 1) + " bytes)");
	if(options.dedupe)
		writeln(remove_list(dupe.name, "name") + " files removed");
	else if(options.json)
		writeln(JSON.stringify(dupe.name, null, json_space));
	else
		print_list(dupe.name, "name");
}
if(hash_type) {
	log(dupe.hash.length + " duplicate file " + hash_type.toUpperCase() + " sums of at least " 
		+ min_size + " bytes (" + file_size_str(hash_bytes, 1, 1) + " bytes)");
	if(options.dedupe)
		writeln(remove_list(dupe.hash, hash_type) + " files removed");
	else if(options.json)
		writeln(JSON.stringify(dupe.hash, null, json_space));
	else
		print_list(dupe.hash, hash_type);
}
if(options.arc) {
	log(dupe.arc.length + " duplicate archives (" + file_size_str(arc_bytes,1 , 1) + " bytes)");
	if(options.dedupe)
		writeln(remove_list(dupe.arc, "contents") + " files removed");
	else if(options.json)
		writeln(JSON.stringify(dupe.arc, null, json_space));
	else
		print_list(dupe.arc, "contents");
}

function print_list(list, key)
{
	for(var i = 0; i < list.length; i++) {
		var value = list[i][0][key];
		if(key == 'crc32')
			value = format("%08X", value);
		else if(!value)
			value = list[i][0].name;
		writeln("Duplicate file " + key + " #" + (i + 1) + ": " + value);
		for(var j = 0; j < list[i].length; j++) {
			var file = list[i][j];
			writeln("  " + file.path);
		}
	}
}

function remove_list(list, key)
{
	var removed = 0;
	for(var i = 0; i < list.length; i++) {
		var value = list[i][0][key];
		if(key == 'crc32')
			value = format("%08X", value);
		else if(!value)
			value = list[i][0].name;
		writeln("Duplicates of file " + key + " #" + (i + 1) + ": " + value);
		writeln("  Keeping " + list[i][0].path);
		for(var j = 1; j < list[i].length; j++) {
			var file = list[i][j];
			var base = new FileBase(file.dir);
			if(!base.open()) {
				alert(base.errror);
				break;
			}
			writeln("  Removing " + file.path);
			if(base.remove(file.name, /* delete: */true))
				removed++;
			else
				alert(base.error);
			base.close();
		}
	}
	return removed;
}
