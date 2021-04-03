// Add files to a file base/area directory for SBBS v3.19+
// Replaces functionality of the old ADDFILES program written in C

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
	print("options:");
	print("-all            add files in all libraries/directories (implies -auto)");
	print("-lib=<name>     add files in all directories of specified library (implies -auto)");
	print("-from=<name>    specify uploader's user name (may require quotes)");
	print("-ex=<filename>  add to excluded filename list");
	print("                (default: " + default_excludes.join(',') + ")");
	print("-diz            always extract/use description in archive");
	print("-update         update existing file entries (default is to skip them)");
	print("-date[=fmt]     include today's date in description");
	print("-fdate[=fmt]    include file's date in description");
	print("-adate[=fmt]    include newest archived file date in description");
	print("                (fmt = optional strftime date/time format string)");
	print("-v              increase verbosity of output");
	print("-debug          enable debug output");
	exit(0);
}

function datestr(t)
{
	if(date_fmt)
		return strftime(date_fmt, t);
	return system.datestr(t);
}

function archive_date(file)
{
	try {
		var list = Archive(file).directory;
	} catch(e) {
		return file_date(file);
	}
	var t = 0;
	for(var i = 0; i < list.length; i++)
		t = Math.max(list[i].time, t);
	return t;
}

var uploader;
var listfile;
var date_fmt;
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
		if(arg.indexOf("-lib=") == 0) {
			var lib = arg.slice(5);
			if(!file_area.lib[lib]) {
				alert("Library not found: " + lib);
				exit(1);
			}
			for(var j = 0; j < file_area.lib[lib].dir_list.length; j++)
				dir_list.push(file_area.lib[lib].dir_list[j].code);
			options.auto = true;
			continue;
		}
		if(arg.indexOf("-from=") == 0) {
			uploader = arg.slice(6);
			continue;
		}
		if(arg.indexOf("-date=") == 0) {
			date_fmt = arg.slice(6);
			options.date = true;
			continue;
		}
		if(arg.indexOf("-fdate=") == 0) {
			date_fmt = arg.slice(7);
			options.fdate = true;
			continue;
		}
		if(arg.indexOf("-adate=") == 0) {
			date_fmt = arg.slice(7);
			options.adate = true;
			continue;
		}
		if(arg == '-' || arg == '-all') {
			for(var dir in file_area.dir)
				dir_list.push(dir);
			options.auto = true;
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
var updated = 0;
for(var d = 0; d < dir_list.length; d++) {
	
	var code = dir_list[d];
	var dir = file_area.dir[code];
	if(!dir) {
		alert("Directory '" + code + "' does not exist in configuration");
		continue;
	}
	if(options.auto && (dir.settings & DIR_NOAUTO))
		continue;
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
		var listpath = file_getcase(dir.path + listfile) || file_getcase(listfile);
		var f = new File(listpath);
		if(f.exists) {
			print("Opening " + f.name);
			if(!f.open('r')) {
				alert("Error " + f.error + " (" + strerror(f.error) + ") opening " + f.name);
				exit(1);
			}
			file_list = parse_file_list(f.readAll());
			f.close();
		} else {
			alert(dir.path + file_getname(listfile) + " does not exist");
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
		file.from = uploader;
		if(options.debug)
			print(JSON.stringify(file, null, 4));
		else if(verbosity)
			printf("%s ", file.name);
		if(exclude.indexOf(file.name.toUpperCase()) >= 0) {
			if(verbosity)
				print("excluded (ignored)");
			continue;
		}
		file.extdesc = lfexpand(file.extdesc);
		if(verbosity > 1)
			print(JSON.stringify(file));
		var exists = name_list.indexOf(filebase.get_file_name(file.name).toUpperCase()) >= 0;
		if(exists && !options.update) {
			if(verbosity)
				print("already added");
			continue;
		}
		var path = file_area.dir[code].path + file.name;
		if(!file_exists(path)) {
			alert("does not exist: " + path);
			continue;
		}
		if(options.date)
			file.desc = datestr(time()) + " " + file.desc;
		else if(options.fdate)
			file.desc = datestr(file_date(path)) + " " + file.desc;
		else if(options.adate)
			file.desc = datestr(archive_date(path)) + " " + file.desc;
		file.cost = file_size(path);
		if(exists) {
			var hash = filebase.hash_file(file.name);
			if(hash) {
				file.size = hash.size;
				file.crc16 = hash.crc16;
				file.crc32 = hash.crc32;
				file.md5 = hash.md5;
				file.sha1 = hash.sha1;
			}
			if(!filebase.update_file(file.name, file, options.diz)) {
				alert("Error " + filebase.last_error + " updating " + file.name);
			} else {
				print("Updated " + file.name);
				updated++;
			}
		} else {
			// Add file here:
			if(!filebase.add_file(file, options.diz)) {
				alert("Error " + filebase.last_error + " adding " + file.name);
			} else {
				print("Added " + file.name);
				added++;
			}
		}
	}
	
	filebase.close();
}
print(added + " files added");
if(updated)
	print(updated + " files updated");

// Parse a FILES.BBS (or similar) file listing file
// Note: file descriptions must begin with an alphabetic character
function parse_file_list(lines)
{
	var file_list = [];
	for(var i = 0; i < lines.length; i++) {
		var line = lines[i];
		var match = line.match(/(^[\w]+[\w\-\!\#\.]*)\W+[^A-Za-z]*(.*)/);
//		print('fname line match: ' + JSON.stringify(match));
		if(match && match.length > 1) {
			var file = { name: match[1], desc: match[2] };
			if(file.desc && file.desc.length > LEN_FDESC)
				file.extdesc = word_wrap(file.desc, 45);
			file_list.push(file);
			continue;
		}
		match = line.match(/\W+\|\s+(.*)/);
		if(!match) {
			if(verbosity)
				alert("Ignoring line: " + line);
			continue;
		}
//		print('match: ' + JSON.stringify(match));
		if(match && match.length > 1 && file_list.length) {
			var file = file_list[file_list.length - 1];
			if(!file.extdesc)
				file.extdesc = file.desc + "\n";
			file.extdesc += match[1] + "\n";
			var combined = file.desc + " " + match[1].trim();
			if(combined.length <= LEN_FDESC)
				file.desc = combined;
		}
	}
	return file_list;
}
