// Find duplicate files in a Synchronet v3.19 file bases

// Can search for duplicate file names and/or hash sums through one or more
// directories with simple text or JSON-formatted output

load("file_size.js");

"use strict";

if(argv.indexOf("-help") >= 0 || argv.indexOf("-?") >= 0) {
	writeln("usage: [-options] [[dir_code] [...]]");
	writeln("options:");
	writeln("  -lib=<name>     search for duplicates in specified library only");
	writeln("  -min=<bytes>    specify minimum file size to compare hash/sum");
    writeln("  -ex=<filename>  add to excluded file name list");
	writeln("  -crc32          search for duplicate CRC-32 sums");
	writeln("  -md5            search for duplicate MD5 sums");
	writeln("  -sha1           search for duplicate SHA-1 sums (the default)");
	writeln("  -names          search for duplicate file names (case-insensitive)");
	writeln("  -json           use JSON formatted output");
	writeln("  -v              increase verbosity of output");
	exit(0);
}

var detail = 0;
var min_size = 1024;
var dir_list = [];
var exclude = [];
var hash_type;
var options = {};
for(var i = 0; i < argc; i++) {
	var arg = argv[i];
	if(arg[0] == '-') {
		var opt = arg.slice(1);
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
if(!options.names && !hash_type)
	hash_type = "sha1";

log("Reading file areas...");
var name = {};
var hash = {};
var total_files = 0;
var total_bytes = 0;
for(var i in dir_list) {
	var dir_code = dir_list[i];
	var dir = file_area.dir[dir_code];
	var base = new FileBase(dir_code);
	if(!base.open())
		throw new Error(base.last_error);
	var list = base.get_list(detail, /* sort: */false);
	for(var j = 0; j < list.length; j++) {
		var file = list[j];
		if(exclude.indexOf(file.name.toUpperCase()) >= 0)
			continue;
		file.dir = dir_code;
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
		total_bytes += file.size;
	}
	base.close();
	total_files += list.length;
}

log("Searching for duplicates in " + total_files + " files (" 
	+ file_size_str(total_bytes, /* unit */1, /* precision */1) + " bytes) ...");
var dupe = { name: [], hash: []};
var name_bytes = 0;
var hash_bytes = 0;
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

if(options.names) {
	log(dupe.name.length + " duplicate file names (" + file_size_str(name_bytes,1 , 1) + " bytes)");
	if(options.json)
		writeln(JSON.stringify(dupe.name, null, 4));
	else
		print_list(dupe.name, "name");
}
if(hash_type) {
	log(dupe.hash.length + " duplicate file " + hash_type.toUpperCase() + " sums of at least " 
		+ min_size + " bytes (" + file_size_str(hash_bytes, 1, 1) + " bytes)");
	if(options.json)
		writeln(JSON.stringify(dupe.hash, null, 4));
	else
		print_list(dupe.hash, hash_type);
}

function print_list(list, key)
{
	for(var i = 0; i < list.length; i++) {
		writeln("Duplicate file " + key + " #" + (i + 1) + ": " + list[i][0][key]);
		for(var j = 0; j < list[i].length; j++) {
			var f = list[i][j];
			writeln(format("  %s%s", file_area.dir[f.dir].path, f.name));
		}
	}
}
