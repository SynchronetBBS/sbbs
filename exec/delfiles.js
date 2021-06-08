// Delete files from Synchronet v3.19 file bases

require("sbbsdefs.js", "DIR_SINCEDL");

"use strict";

var dir_list = [];
var exclude = [];
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
			writeln("  -ex=<filename>  add to excluded file name list (case-insensitive)");
			writeln("  -offline        remove files that are offline (don't exist on disk)");
			writeln("  -test           don't actually remove files, just report findings");
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
		options[opt] = true;
		continue;
	}
	dir_list.push(arg);
}

if(dir_list.length < 1)
	for(var dir in file_area.dir)
		dir_list.push(dir);

var now = time();
for(var i in dir_list) {
	var dir_code = dir_list[i];
	var dir = file_area.dir[dir_code];
	var base = new FileBase(dir_code);
	if(!base.open())
		throw new Error(base.last_error);
	if(options.offline) {
		log("Purging offline files");
		var list = base.get_names(/* sort: */false);
		var removed = 0;
		for(var j = 0; j < list.length; j++) {
			var file = list[j];
			if(exclude.indexOf(file.toUpperCase()) >= 0)
				continue;
			if(base.get_size(file) < 0) {
				log("Removing offline file: " + base.get_path(file));
				if(options.test)
					removed++;
				else {
					if(!base.remove(file, /* delete: */false))
						alert(base.error);
					else
						removed++;
				}
			}
		}
		log("Removed " + removed + " offline files");
	}
	if(base.max_age) {
		log("Purging old files, imposing max age of " + base.max_age + " days");
		var list = base.get_list(FileBase.DETAIL.NORM, /* sort: */false);
		var removed = 0;
		for(var j = 0; j < list.length; j++) {
			var file = list[j];
			if(exclude.indexOf(file.name.toUpperCase()) >= 0)
				continue;
			var t = file.added;
			var age_desc = "uploaded";
			if(file.last_downloaded
				&& (file_area.dir[dir_code].settings & DIR_SINCEDL)) {
				t = file.last_downloaded;
				age_desc = "last downloaded";
			}
			var file_age = Math.floor((now - t) / (24 * 60 * 60));
			if(file_age > base.max_age) {
				log("Removing " + base.get_path(file.name) + " " + age_desc + " " + file_age + " days ago");
				if(options.test)
					removed++;
				else {
					if(!base.remove(file.name, /* delete: */true))
						alert(base.error);
					else
						removed++;
				}
			}
		}
		log("Removed " + removed + " of " + list.length + " files due to age of " + base.max_age + " days");
	}
	if(base.max_files) {
		log("Purging excess files, imposing max files limit of " + base.max_files);
		var list = base.get_list(FileBase.DETAIL.MIN, /* sort: */false);
		var removed = 0;
		var excess = list.length - base.max_files;
		for(var j = 0; j < list.length && removed < excess; j++) {
			var file = list[j];
			if(exclude.indexOf(file.name.toUpperCase()) >= 0)
				continue;
			log("Removing " + file.name);
			if(options.test)
				removed++;
			else {
				if(!base.remove(file.name, /* delete: */true))
					alert(base.error);
				else
					removed++;
			}
		}
		log("Removed " + removed + " of " + list.length + " files due to max file limit of " + base.max_files);
	}

	base.close();
}
