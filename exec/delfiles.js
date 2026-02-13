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
			writeln("  -lib=<name>     operate on files in specified library only");
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

if(options.test)
	log("Running in test (read-only) mode");
var now = time();
for(var i in dir_list) {
	var dir_code = dir_list[i];
	var dir = file_area.dir[dir_code];
	log(LOG_DEBUG, dir_code);
	var base = new FileBase(dir_code);
	if(!base.open())
		throw new Error(base.last_error);
	if(options.offline) {
		log(dir_code + ": Purging offline files");
		var dirent = directory(dir.path + '*');
		var list = base.get_names(/* sort: */false);
		var removed = 0;
		for(var j = 0; j < list.length; j++) {
			var fname = list[j];
			if(exclude.indexOf(fname.toUpperCase()) >= 0)
				continue;
			var di = dirent.indexOf(dir.path + fname);
			if(di >= 0) {
				dirent.splice(di, 1);
				continue;
			}
			if(base.get_size(fname) < 0) {
				var path;
				try {
					path = base.get_path(fname);
				} catch (e) {
					alert(e);
					continue;
				}
				log(LOG_INFO, dir_code + ": Removing offline file: " + path);
				if(options.test)
					removed++;
				else {
					if(!base.remove(fname, /* delete: */false))
						alert(base.error);
					else
						removed++;
				}
			}
		}
		if(removed)
			log(LOG_NOTICE, dir_code + ": Removed " + removed + " offline files");
	}
	var max_age = base.max_age || dir.max_age;
	if(max_age) {
		log(dir_code + ": Purging old files, imposing max age of " + max_age + " days");
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
			if(file_age > max_age) {
				var path;
				try {
					path = base.get_path(file.name);
				} catch (e) {
					alert(e);
					continue;
				}
				log(LOG_INFO, dir_code + ": Removing " + path + " " + age_desc + " " + file_age + " days ago");
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
		if(removed)
			log(LOG_NOTICE, dir_code + ": Removed " + removed + " of " + list.length + " files due to age greater than " + max_age + " days");
	}
	var max_files = base.max_files || dir.max_files;
	var tfiles = file_area.dir[dir_code].files;
	if(max_files && tfiles > max_files) {
		log(dir_code + ": Purging excess files, imposing max files limit of " + max_files + " in area with " + tfiles + " files");
		var list = base.get_list(FileBase.DETAIL.MIN, /* sort: */false);
		var removed = 0;
		var excess = list.length - max_files;
		for(var j = 0; j < list.length && removed < excess; j++) {
			var file = list[j];
			if(exclude.indexOf(file.name.toUpperCase()) >= 0)
				continue;
			log(LOG_INFO, dir_code + ": Removing " + file.name);
			if(options.test)
				removed++;
			else {
				if(!base.remove(file.name, /* delete: */true))
					alert(base.error);
				else
					removed++;
			}
		}
		if(removed)
			log(LOG_NOTICE, dir_code + ": Removed " + removed + " of " + list.length + " files due to max file limit of " + max_files);
	}

	base.close();
}
