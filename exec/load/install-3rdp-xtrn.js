// install-3rdp-xtrn.js

// Looks for matching (name and MD5-sum) executables in xtrn/*.
// If one is found, the corresponding .ini file is copied to that xtrn/*
// (startup) directory with the name install-xtrn.ini so it'll be picked up by
// install-xtrn.js and xtrn-setup.js.
// Multiple md5 values can be specified, separated by commas, for multiple
// supported versions of the same door.

// The additional install-xtrn.ini keys are:
// exe = filename of door's executable
// md5 = comma-separated list of md5-sums (in lowercase hexadecimal)

"use strict";

function scan(options)
{
	if(options === undefined)
		options = {};
	if(!options.src_dir)
		options.src_dir = "../xtrn/3rdp-install/";
	if(!options.xtrn_dir)
		options.xtrn_dir = "../xtrn/";
	
	var out = [];
	var exe_list = {};
	
	directory(options.src_dir + '*.ini').forEach(function (e) {
		const f = new File(e);
		if (!f.open('r')) {
			out.push("!Error " + f.error + " opening " + f.name);
			return;
		}
		const ini = f.iniGetObject(/* lowercase: */true);
		f.close();
		if (!ini.exe) {
			out.push("!No executable filename specified in " + f.name);
			return;
		}
		if (!ini.md5) {
			out.push("!No md5 list specified in " + f.name);
			return;
		}
		if (!exe_list[ini.exe])
			exe_list[ini.exe] = {};
		var md5_list = ini.md5.split(',');
		for(var i in md5_list)
			exe_list[ini.exe][md5_list[i]] = f.name;
	});
	
	for(var i in exe_list) {
		directory(options.xtrn_dir + '*').forEach(function (e) {
			const f = new File(e + i);
			if (!f.open('rb')) {
				return;
			}
			var md5 = f.md5_hex;
			f.close();
			if(!exe_list[i][md5]) {
				if(options.debug)
					out.push("!MD5 sum of " + f.name + " (" + md5 + ") not found.");
				return;
			}
			var startup_dir = f.name.substr(0, Math.max(f.name.lastIndexOf("/"), f.name.lastIndexOf("\\"), 0));
			var ini_fname = startup_dir + "/install-xtrn.ini";
			if (file_exists(ini_fname) && !options.overwrite) {
				if(options.debug)
					out.push(ini_fname + " already exists");
				return;
			}
			if (!file_copy(exe_list[i][md5], ini_fname)) {
				out.push("!Error copying " + exe_list[i][md5] + " to " + ini_fname);
				return;
			}
			out.push(exe_list[i][md5] + " copied to " + ini_fname);
		});
	}
	return out;
}

this;