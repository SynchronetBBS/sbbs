// Add files to a file base/area directory for SBBS v3.19+
// Replaces functionality of the old ADDFILES program written in C

require("sbbsdefs.js", 'LEN_FDESC');

const lib = load({}, "filelist_lib.js");

"use strict";

const default_excludes = lib.filenames.concat([
	"FILE_ID.DIZ",
]);

function datestr(t)
{
	if(date_fmt)
		return strftime(date_fmt, t);
	return system.datestr(t);
}

function archive_date(file)
{
	try {
		var list = Archive(file).list();
	} catch(e) {
		return file_date(file);
	}
	var t = 0;
	for(var i = 0; i < list.length; i++)
		t = Math.max(list[i].time, t);
	return t;
}

function proper_lib_name(name)
{
	for(var i in file_area.lib_list) {
		var lib = file_area.lib_list[i];
		if(lib.name.toLowerCase() == name.toLowerCase())
			return lib.name;
	}
	return name;
}

var uploader;
var listfile;
var date_fmt;
var desc_off = 0;
var options = {};
var exclude = [];
var include = "*";
var dir_list = [];
var verbosity = 0;
for(var i = 0; i < argc; i++) {
	var arg = argv[i];
	if(arg[0] == '-') {
		var opt = arg;
		while(opt[0] == '-')
			opt = opt.slice(1);
		if(opt == '?' || opt.toLowerCase() == "help") {
			writeln("usage: [-options] [dir-code] [listfile] [desc-off]");
			writeln("options:");
			writeln("  -all            add files in all libraries/directories (implies -auto)");
			writeln("  -lib=<name>     add files in all directories of specified library (implies -auto)");
			writeln("  -auto           add files only to directories that have Auto-ADDFILES enabled");
			writeln("  -dir=<code,...> add files in directories (can be specified multiple times)");
			writeln("  -from=<name>    specify uploader's user name (may require quotes)");
			writeln("  -file=<name>    specify files to add (wildcards supported, default: *)");
			writeln("  -ex=<filename>  add to excluded filename list");
			writeln("                  (default: " + default_excludes.join(',') + ")");
			writeln("  -diz            always extract/use description in archive");
			writeln("  -update         update existing file entries (default is to skip them)");
			writeln("  -readd          re-add existing file entries (so they appear as newly-uploaded");
			writeln("  -date[=fmt]     include today's date in description");
			writeln("  -fdate[=fmt]    include file's date in description");
			writeln("  -adate[=fmt]    include newest archived file date in description");
			writeln("                  (fmt = optional strftime date/time format string)");
			writeln("  -delete         delete list after import");
			writeln("  -v              increase verbosity of output");
			writeln("  -debug          enable debug output");
			writeln("optional:");
			writeln("  dir-code:       File directory internal code");
			writeln("  listfile:       Name of listfile (e.g. FILES.BBS)");
			writeln("  desc-off:       Description character offset (number)");
			exit(0);
		}
		if(opt.indexOf("ex=") == 0) {
			exclude.push(opt.slice(3).toUpperCase());
			continue;
		}
		if(opt.indexOf("lib=") == 0) {
			var libname = proper_lib_name(opt.slice(4));
			if(!file_area.lib[libname]) {
				alert("Library not found: " + libname);
				exit(1);
			}
			for(var j = 0; j < file_area.lib[libname].dir_list.length; j++)
				dir_list.push(file_area.lib[libname].dir_list[j].code);
			options.auto = true;
			continue;
		}
		if(opt.indexOf("dir=") == 0) {
			dir_list.push.apply(dir_list, opt.slice(4).split(','));
			continue;
		}
		if(opt.indexOf("file=") == 0) {
			include = opt.slice(5);
			continue;
		}
		if(opt.indexOf("from=") == 0) {
			uploader = opt.slice(5);
			continue;
		}
		if(opt.indexOf("date=") == 0) {
			date_fmt = opt.slice(5);
			options.date = true;
			continue;
		}
		if(opt.indexOf("fdate=") == 0) {
			date_fmt = opt.slice(6);
			options.fdate = true;
			continue;
		}
		if(opt.indexOf("adate=") == 0) {
			date_fmt = opt.slice(6);
			options.adate = true;
			continue;
		}
		if(opt == "all") {
			for(var dir in file_area.dir)
				dir_list.push(dir);
			options.auto = true;
			continue;
		}
		if(opt[0] == 'v') {
			var j = 0;
			while(opt[j++] == 'v')
				verbosity++;
			continue;
		}
		options[opt] = true;
	} else {
		if(Number(arg))
			desc_off = Number(arg);
		else if(!dir_list.length)
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
			writeln(d);
		code = prompt("Directory code");
	}
	dir_list.push(code);
}

var added = 0;
var updated = 0;
var renamed = 0;
var missing = [];
for(var d = 0; d < dir_list.length; d++) {

	var code = dir_list[d].toLowerCase();
	var dir = file_area.dir[code];
	if(!dir) {
		alert("Directory '" + code + "' does not exist in configuration");
		continue;
	}
	if(options.auto && (dir.settings & DIR_NOAUTO))
		continue;
	writeln("Adding files to " + dir.lib_name + " " + dir.name);

	var filebase = new FileBase(code);
	if(!filebase.open("r")) {
		alert("Failed to open: " + filebase.file);
		continue;
	}

	var name_list = filebase.get_names();
	// Convert to uppercase
	for(var i = 0; i < name_list.length; i++) {
		name_list[i] = name_list[i].toUpperCase();
		if(options.debug)
			writeln(name_list[i]);
	}
	var file_list = [];

	var listpath;
	if(listfile) {
		if(file_getname(listfile) == listfile)
			listpath = dir.path + listfile;
		else
			listpath = listfile;
		var realpath = file_getcase(listpath);
		if(!realpath) {
			alert(listpath + " does not exist");
			continue;
		}
		var f = new File(realpath);
		writeln("Opening " + f.name);
		if(!f.open('r')) {
			alert("Error " + f.error + " (" + strerror(f.error) + ") opening " + f.name);
			exit(1);
		}
		file_list = lib.parse(f.readAll(), desc_off, verbosity);
		f.close();
	}
	else {
		var list = directory(dir.path + '*');
		for(var i = 0; i < list.length; i++) {
			if(!file_isdir(list[i]))
				file_list.push({ name: file_getname(list[i]) });
		}
	}
	file_list = file_list.filter(function(obj) { return wildmatch(obj.name, include); });

	for(var i = 0; i < file_list.length; i++) {
		var file = file_list[i];
		file.from = uploader;
		if(options.debug)
			writeln(JSON.stringify(file, null, 4));
		else if(verbosity)
			write(file.name + " ");
		if(exclude.indexOf(file.name.toUpperCase()) >= 0) {
			if(verbosity)
				writeln("excluded (ignored)");
			continue;
		}
		file.extdesc = lfexpand(file.extdesc);
		if(verbosity > 1)
			writeln(JSON.stringify(file));
		var exists = name_list.indexOf(filebase.get_name(file.name).toUpperCase()) >= 0;
		if(exists && !options.update) {
			if(verbosity)
				writeln("already added");
			continue;
		}
		var path = file_area.dir[code].path + file.name;
		var realpath = file_getcase(path);
		if(!realpath) {
			realpath = file_getcase(path.replace(/-/g, "_"));
			if(!realpath) {
				alert("does not exist: " + path);
				missing.push(path);
				continue;
			}
		}
		if(file.name != file_getname(realpath)) {
			print("Renamed " + file.name + " to " + file_getname(realpath));
			++renamed;
		}
		path = realpath;
		file.name = file_getname(path);
		if(options.date)
			file.desc = datestr(time()) + " " + (file.desc || "");
		else if(options.fdate)
			file.desc = datestr(file_date(path)) + " " + (file.desc || "");
		else if(options.adate)
			file.desc = datestr(archive_date(path)) + " " + (file.desc || "");
		file.cost = file_size(path);
		if(exists) {
			var hash = filebase.hash(file.name);
			if(hash) {
				file.size = hash.size;
				file.crc16 = hash.crc16;
				file.crc32 = hash.crc32;
				file.md5 = hash.md5;
				file.sha1 = hash.sha1;
			}
			if(!filebase.update(file.name, file, options.diz, options.readd)) {
				alert("Error " + filebase.last_error + " updating " + file.name);
			} else {
				writeln("Updated " + file.name);
				updated++;
			}
		} else {
			// Add file here:
			if(!filebase.add(file, options.diz)) {
				alert("Error " + filebase.last_error + " adding " + file.name);
			} else {
				writeln("Added " + file.name);
				added++;
			}
		}
	}
	if(listpath && options.delete) {
		if(verbosity)
			writeln("Deleting list file: " + listpath);
		if(file_remove(listpath))
			writeln("List file deleted: " + listpath);
		else
			alert("Failed to delete list file: " + listpath);
	}
	filebase.close();
}
writeln(added + " files added");
if(updated)
	writeln(updated + " files updated");
if(renamed)
	writeln(renamed + " files renamed");
if(missing.length) {
	alert(missing.length + " files missing");
	if(verbosity) {
		for(var i in missing)
			alert(missing[i]);
	}
}
