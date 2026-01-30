// Installer for Synchronet External Programs

// This script parses a .ini file (default filename is install-xtrn.ini)
// and installs the external programs defined within into the Synchronet BBS
// configuration file: ctrl/xtrn.ini.  The programs defined within this file
// can by online programs (a.k.a. Doors), editors, or events.
//
// This script is intended for use by sysops using JSexec, for example:
//   $ jsexec install-xtrn ../xtrn/minesweeper
//
// This script can aso be invoked using the ;EXEC sysop command while logged
// into the terminal server, for example:
//   ;exec ?install-xtrn ../xtrn/minesweeper
//
// The .ini root section keys supported:
//
// Name = Name of the program being installed (79 chars or less)
// Desc = Description of the program being installed (79 chars or less)
// By   = Comma-separated list of programmers/authors/publishers
// Cats = Comma-separated list of applicable categories (e.g. "Games")
// Subs = Comma-separated list of applicable sub-categories (e.g. "Adventure")
// Inst = Installer .ini file source/revision/author/date information
//
// The .ini sections and keys supported (zero or more of each may be included):
//
// [pre-exec:<file>.js [args]] ; execute file.js before installing programs
//		startup_dir		= directory to make current before execution
//
// [pre-eval:<js-expression>] ; evaluate js-expression before installing progs
//      cmd             = evaluate this string rather than the js-expression
//
// [prog:<code>]
// 		name 			= program name or description (40 chars max)
//      cats            = additional target installation categories (sections)
//		cmd 			= command-line to execute (63 chars max)
//		clean_cmd 		= clean-up command-line, if needed (63 chars max)
//		settings 		= bit-flags (see XTRN_* in sbbsdefs.js)
//		ars				= access requirements string (40 chars max)
//		execution_ars	= execution requirements string (40 chars max)
//		type			= drop-file type (see XTRN_* in sbbsdefs.js)
//		event			= event-type (see EVENT_* in sbbsdefs.j)
//		cost			= cost to run, in credits
//		startup_dir		= directory to make current before execution
//		textra			= extra time (minutes) to allow to run this program
//		max_time		= maximum time (minutes) allowed to run this program
//
// [event:<code>]
//		cmd 			= command-line to execute (63 chars max)
//		days			= bit-field representing days of the week to execute
//		time			= time of day to run this event
//		node_num		= node number to run this event
//		settings		= bit-flags (see XTRN_* in sbbsdefs.js)
//		startup_dir		= directory to make current before execution
//		freq			= frequency of execution
//		mdays			= days of month (if non-zero) for execution
//		months			= bit-field representing which months to execute
//		xtrn            = code of external program to run exclusive to
//
// [editor:<code>]
//		name 			= editor name or description (40 chars max)
//		cmd 			= command-line to execute (63 chars max)
//		type			= drop-file type (see XTRN_* in sbbsdefs.js)
//		settings 		= bit-flags (see XTRN_* in sbbsdefs.js)
//		ars				= access requirements string (40 chars max)
//
// [service:<protocol>]
//      see ctrl/services.ini 
//
// [exec:<file>.js [args]] ; execute file.js with these arguments - after
//		startup_dir		= directory to make current before execution
//
// [eval:<js-expression>] ; evaluate js-expression after installing programs
//      cmd             = evaluate this string rather than the js-expression
//
// [ini:<filename.ini>[:section]]
//      keys            = comma-separated list of keys to add/update in .ini
//      values          = list of values to eval() and assign to keys[]
//                        Note: string values must be enclosed in quotes!
//
// [copy:<filename>]
//      dest            = filename to copy to

// Additionally, each section can have the following optional keys that are
// only used by this script (i.e. not written to any configuration files):
//		note			= note to sysop displayed before installation
//		fail			= note to sysop displayed upon failure
//      prompt          = confirmation prompt (or false if no prompting)
//		required		= if true, this item must be successful to continue
//      last            = if true, this item will be the last of its type
//      done            = if true, no more installer items will be processed
//
// Notes:
//
// - The startup_dir will default to the location of the .ini file if this
//   key is not defined within the .ini file.
//
// - The only required values are the <code> (internal code, 8 chars max) and
//   cmd; all other keys will have functional default values if not defined in
//   the .ini file.

"use strict";

const REVISION = "3.21a";
const ini_fname = "install-xtrn.ini";

load("sbbsdefs.js");
var relpath = load({}, "relpath.js");

var options = {
	debug: false,
	overwrite: false
};

function aborted()
{
	if(js.terminated || (js.global.console && console.aborted))
		exit(1);
	return false;
}

function install_xtrn_item(cfg, type, name, desc, item, cats)
{
	if (!item.code)
		return false;

	if (!item.name)
		item.name = name || item.code;

	if(item.cats)
		item.cats = item.cats.split(',').concat(cats);
	else
		item.cats = cats;

	function find_code(objs, code)
	{
		if (!options.overwrite) {
			for (var i=0; i < objs.length; i++)
				if (objs[i].code.toLowerCase() == code.toLowerCase())
					return i;
		}
		return -1;
	}
	
	if(find_code(cfg[type], item.code) >= 0) {
		if(options.auto
			|| deny(desc + " (" + item.code + ") already exists, continue"))
			return false;
	}

	while (!item.code && !aborted()
		|| (find_code(cfg[type], item.code) >= 0
			&& print(desc + " Internal Code (" + item.code + ") already exists!")))
		item.code = js.global.prompt(desc + " Internal code");

	if(aborted())
		return false;
	
	if (item.note)
		print(item.note);
	
	var prompt = "Install " + desc + ": " + item.name;
	if (item.prompt !== undefined)
		prompt = item.prompt;
	
	if (prompt && !confirm(prompt)) {
/**		
		if (item.required == true)
			return "Installation of " + item.name + " is required to continue";
**/
		return false;
	}
	
	if (type == "prog") {
		if (!xtrn_area.sec_list.length)
			return "No external program sections have been created";

		for (var i = 0; i < item.cats.length; i++) {
			var code = item.cats[i].toLowerCase();
			if(xtrn_area.sec[code]
				&& confirm("Install " + item.name + " into " + xtrn_area.sec[code].name + " section")) {
				item.sec = xtrn_area.sec[code].number;
				break;
			}
		}
		if(item.sec === undefined) {
			for (var i = 0; i < xtrn_area.sec_list.length; i++)
				print(format("%2u: ", i + 1) + xtrn_area.sec_list[i].name);

			var which;
			while ((!which || which > xtrn_area.sec_list.length) && !aborted())
				which = js.global.prompt("Install " + item.name  + " into which section");
			if(aborted())
				return false;
			which = parseInt(which, 10);
			if (!which)
				return false;

			item.sec = xtrn_area.sec_list[which - 1].number;
		}
	}


	try {
		item.code = item.code.toUpperCase();
		if (item.settings)
			item.settings = eval(item.settings);
		if (item.event)
			item.event = eval(item.event);
		if (item.type)
			item.type = eval(item.type);
	} catch(e) {
		return e;
	}
	cfg[type].push(item);
	if (options.debug)
		print(JSON.stringify(cfg[type], null, 4));
	
	print(desc + " (" + item.name + ") installed successfully");
	return true;
}

function install_exec_cmd(ini_file, section, startup_dir)
{
	var list = ini_file.iniGetAllObjects("cmd", section);
	for (var i = 0; i < list.length; i++) {
		var item = list[i];
		var js_args = item.cmd.split(/\s+/);
		var js_file = js_args.shift();

		if (file_getext(js_file).toLowerCase() != ".js")
			return "Only '.js' files may be executed: " + js_file;

		if (item.note)
			print(item.note);

		var prompt = "Execute: " + item.cmd;
		if (item.prompt !== undefined)
			prompt = item.prompt;

		if (prompt && !confirm(prompt)) {
			if (item.required == true)
				return prompt + " is required to continue";
			continue;
		}

		if (item.startup_dir === undefined)
			item.startup_dir = startup_dir;
		if (!item.args)
			item.args = "";
		var result = js.exec.apply(null
			,[js_file, item.startup_dir, {}].concat(js_args));
		if (result !== 0 && item.required)
			return item.fail || ("Error " + result + " executing " + item.cmd);
		if (item.last === true)
			break;
		if (item.done)
			return false;
	}
	return true;
}

function install_eval_cmd(ini_file, section, startup_dir)
{
	var list = ini_file.iniGetAllObjects("str", section);
	for (var i = 0; i < list.length; i++) {
		var item = list[i];
		if (!item.cmd)
			item.cmd = item.str; // the str can't contain [], so allow cmd to override
		var prompt = "Evaluate: " + item.cmd;
		if (item.prompt !== undefined)
			prompt = item.prompt;
		if (prompt && !confirm(prompt)) {
			if (item.required == true)
				return prompt + " is required to continue";
			continue;
		}
		try {
			var result = eval(item.cmd);
		} catch(e) {
			return e;
		}
		if (!result && item.required)
			return item.fail || ("Truthful evaluation of '" + item.cmd + "' is required to continue");
		if (item.last === true)
			break;
		if (item.done)
			return false;
	}
	return true;
}

function install(ini_fname)
{
	ini_fname = fullpath(ini_fname);
	var installed = 0;
	if(!options.auto || options.debug) {
		var banner = "* Installing " + ini_fname + " use Ctrl-C to abort *";
		var line = "";
		for (var i = 0; i < banner.length; i++)
			line += "*";
		print(line);
		print(banner);
		print(line);
	}
	var ini_file = new File(ini_fname);
	if (!ini_file.open("r"))
		return ini_file.name + " open error " + ini_file.error;
	
	var inst = ini_file.iniGetValue(null, "inst");
	if(inst)
		print("Install file: " + inst);
	var name = ini_file.iniGetValue(null, "name");
	if(name)
		print("[ " + name + " ]");
	var desc = ini_file.iniGetValue(null, "desc");
	if(desc)
		print(desc);
	var by = ini_file.iniGetValue(null, "by", []);
	if(by.length)
		print("By: " + by.join(", "));
	var cats = ini_file.iniGetValue(null, "cats", []);
	if(cats.length)
		print("Categories: " + cats.join(", "));
	var subs = ini_file.iniGetValue(null, "subs", []);
	if(subs.length)
		print("Sub-categories: " + subs.join(", "));

	var cfglib = load({}, "cfglib.js");
	var xtrn_cfg = cfglib.read(system.ctrl_dir + "xtrn.ini");
	if (!xtrn_cfg)
		return "Failed to read " + system.ctrl_dir + "xtrn.ini";
	
	var startup_dir = ini_fname.substr(0, Math.max(ini_fname.lastIndexOf("/"), ini_fname.lastIndexOf("\\"), 0));
	startup_dir = relpath.get(system.ctrl_dir, startup_dir);

	var result = install_exec_cmd(ini_file, "pre-exec:", startup_dir);
	if(result !== true)
		return result;

	result = install_eval_cmd(ini_file, "pre-eval:", startup_dir);
	if(result !== true)
		return result;

	const types = {
		prog:	{ desc: "External Program", 	struct: "prog" },
		event:	{ desc: "External Timed Event", struct: "event" },
		editor:	{ desc: "External Editor",		struct: "editor" }
	};
	
	var done = false;
	for (var t in types) {
		var list = ini_file.iniGetAllObjects("code", t + ":");
		for (var i = 0; i < list.length && !done; i++) {
			var item = list[i];
			if (item.startup_dir === undefined)
				item.startup_dir = startup_dir;
			var result = install_xtrn_item(xtrn_cfg, types[t].struct, name, types[t].desc, item, cats);
			if (typeof result !== 'boolean')
				return result;
			if (result === true)
				installed++;
			else if(item.required)
				return false;
			if(item.last === true)
				break;
			done = Boolean(item.done);
		}
	}
	
	var list = ini_file.iniGetAllObjects("filename", "ini:");
	for (var i = 0; i < list.length && !done; i++) {
		var item = list[i];
		var a = item.filename.split(':');
		item.filename = startup_dir + a[0];
		if(!file_exists(item.filename))
			item.filename = file_cfgname(system.ctrl_dir, a[0]);
		item.section = a[1] || null;
		item.keys = item.keys.split(',');
		item.values = item.values.split(',');
		var prompt = "Add/update the " + (item.section || "root") + " section of " + file_getname(item.filename);
		if (item.prompt !== undefined)
			prompt = item.prompt;
		if (prompt && !confirm(prompt)) {
			if (item.required == true)
				return prompt + " is required to continue";
			continue;
		}
		var file = new File(item.filename);
		if(!file.open(file.exists ? 'r+':'w+'))
			return "Error " + file.error + " opening " + file.name;
		var result = true;
		if (options.debug)
			print(JSON.stringify(item));
		for(var k in item.keys) {
			try {
				var value = eval(item.values[k]);
			} catch(e) {
				return e;
			}
			print("Setting " + item.keys[k] + " = " + value);
			result = file.iniSetValue(item.section, item.keys[k], value);
		}
		file.close();
		if(required && result !== true)
			return false;
		if(item.last === true)
			break;
		done = Boolean(item.done);
	}

	var list = ini_file.iniGetAllObjects("filename", "copy:");
	for (var i = 0; i < list.length && !done; i++) {
		var item = list[i];
		var result = false;
		var src = file_getcase(startup_dir + item.filename);
		if (!src)
			alert("Copy source file does not exist: " + startup_dir + item.filename);
		else {
			var dest = file_getcase(startup_dir + item.dest);
			if (dest) {
				if (!item.overwrite) {
					var msg = "Copy destination file already exists: " + dest;
					if(options.auto || deny(msg + ", continue"))
						return msg;
				}
			} else
				dest = startup_dir + item.dest
			result = file_copy(src, dest);
		}
		if(required && result !== true)
			return false;
		if(item.last === true)
			break;
		done = Boolean(item.done);
	}

	var services_ini = new File(file_cfgname(system.ctrl_dir, "services.ini"));
	var list = ini_file.iniGetAllObjects("protocol", "service:");
	for (var i = 0; i < list.length && !done; i++) {
		var item = list[i];
		var prompt = "Install/enable the " + item.protocol + " service in " + file_getname(services_ini.name);
		if (item.prompt !== undefined)
			prompt = item.prompt;
		if (prompt && !confirm(prompt)) {
			if (item.required == true)
				return prompt + " is required to continue";
			continue;
		}
		var required = item.required;
		if(!services_ini.open(services_ini.exists ? 'r+':'w+'))
			return "Error " + services_ini.error + " opening " + services_ini.name;
		var service = services_ini.iniGetObject(item.protocol);
		var enabled = services_ini.iniGetValue(item.protocol, "enabled", true);
		var result = true;
		if(!service || !enabled) {
			if(!service)
				service = JSON.parse(JSON.stringify(item));
			service.Enabled = true;
			delete service.prompt;
			delete service.required;
			delete service.protocol;
//			print("Adding " + JSON.stringify(service) + " to " + services_ini.name);
			result = services_ini.iniSetObject(item.protocol, service);
			if (result === true)
				installed++;
			else
				alert("Failed to add " + JSON.stringify(service) + " to " + services_ini.name);
		}
		services_ini.close();
		if(required && result !== true)
			return false;
		if(item.last === true)
			break;
		done = Boolean(item.done);
	}
	
	if(done === false) {
		result = install_exec_cmd(ini_file, "exec:", startup_dir);
		if(typeof result !== 'boolean')
			return result;
		done = !result;
	}

	if(done === false) {
		result = install_eval_cmd(ini_file, "eval:", startup_dir);
		if(typeof result !== 'boolean')
			return result;
		done = !result;
	}

	if (installed) {
		if (!options.debug && !cfglib.write(system.ctrl_dir + "xtrn.ini", undefined, xtrn_cfg))
			return "Failed to write " + system.ctrl_dir + "xtrn.ini";
		print("Installed " + installed + " items from " + ini_fname + " successfully");
	}

	return installed >= 1; // success
}

print(js.exec_file + " v" + REVISION);

// Command-line argument parsing
var ini_list = [];
for (var i = 0; i < argc; i++) {
	if (argv[i][0] == '-')
		options[argv[i].substr(1)] = true;
	else
		ini_list.push(argv[i]);
}

if(!ini_list.length) {
	var lib = load({}, "install-3rdp-xtrn.js");
	var out = lib.scan(options);
	for (var i in out) {
		print(out[i]);
	}
}

function find_startup_dir(dir)
{
	for (var i in xtrn_area.prog) {
		if(!xtrn_area.prog[i].startup_dir)
			continue;
		if (xtrn_area.prog[i].startup_dir.toLowerCase() == dir.toLowerCase())
			return i;
	}
	return null;
}

var xtrn_dirs = fullpath(system.ctrl_dir + "../xtrn/*");
if(!ini_list.length) {
	var dir_list = directory(xtrn_dirs);
	for(var d in dir_list) {
		if(!options.overwrite && find_startup_dir(fullpath(dir_list[d])) != null)
			continue;
		var fname = file_getcase(dir_list[d] + ini_fname);
		if(fname)
			ini_list.push(fname);
	}
}

if(!options.auto && ini_list.length > 1) {
	for(var i = 0; i < ini_list.length; i++) {
		printf("%3d: %s\r\n", i+1, ini_list[i].substr(0, ini_list[i].length - ini_fname.length));
	}
	var which;
	while(!which || which < 1 || which > ini_list.length) {
		var str = prompt("Which or [Q]uit");
		if(aborted())
			exit(0);
		if(str && str.toUpperCase() == 'Q')
			exit(0);
		which = parseInt(str, 10);
	}
	ini_list = [ini_list[which - 1]];
}

if(!ini_list.length) {
	if(options.auto) {
		alert("No install files (" + ini_fname + ") found in " + xtrn_dirs);
		exit(0);
	}
	var ini_path;
	while (!ini_path || !file_exists(ini_path)) {
		ini_path = prompt("Location of " + ini_fname);
		if(aborted())
			exit(0);
		if (file_isdir(ini_path))
			ini_path = backslash(ini_path) + ini_fname;
	}
	ini_list.push(ini_path);
}

var installed = 0;
for(var i in ini_list) {
	var ini_path = ini_list[i];
	// Locate the .ini file
	if (file_isdir(ini_path))
		ini_path = backslash(ini_path) + ini_fname;
	if (!file_exists(ini_path)) {
		alert(ini_path + " does not exist");
		continue;
	}
		
	var result = install(ini_path);
	if(aborted())
		break;
	if (result === true)
		installed++;
	else if (typeof result !== 'boolean')
		alert(result);
}
if (options.debug) {
	print("Would have Installed " + installed + " external programs.");
	print("No programs installed due to DEBUG mode.");
} else {
	print("Installed " + installed + " external programs.");
}

if((installed > 0) && !options.debug) {
	print("Requesting Synchronet recycle (configuration-reload)");
	if(!file_touch(system.ctrl_dir + "recycle"))
		alert("Recycle semaphore file update failure");
	if(!this.jsexec_revision) {
		print();
		print("It appears you have run this script from the BBS. You must log-off now for the");
		print("server to recycle and configuration changes to take effect.");
	}
}
