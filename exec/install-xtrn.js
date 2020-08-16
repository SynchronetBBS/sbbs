// $Id: install-xtrn.js,v 1.14 2020/04/26 06:58:46 rswindell Exp $

// Installer for Synchronet External Programs

// This script parses a .ini file (default filename is install-xtrn.ini)
// and installs the external programs defined within into the Synchronet BBS
// configuration file: ctrl/xtrn.cnf.  The programs defined within this file
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
// [prog:<code>]
// 		name 			= program name or description (40 chars max)
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
// [exec:<file>.js [args]]  ; execute file.js with these arguments
//		startup_dir		= directory to make current before execution
//
// [eval:<js-expression>]
//      cmd             = evaluate this string rather than the js-expression
//
// [ini:<filename.ini>[:section]]
//      keys            = comma-separated list of keys to add/update in .ini
//      values          = list of values to eval() and assign to keys[]

// Additionally, each section can have the following optional keys that are
// only used by this script (i.e. not written to any configuration files):
//		note			= note to sysop displayed before installation
//      prompt          = confirmation prompt (or false if no prompting)
//		required		= if true, this item must be installed to continue
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

const REVISION = "$Revision: 1.14 $".split(' ')[1];
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

function install_xtrn_item(cnf, type, name, desc, item)
{
	if (!item.code)
		return false;

	if (!item.name)
		item.name = name || item.code;
	
	function find_code(objs, code)
	{
		if (!options.overwrite) {
			for (var i=0; i < objs.length; i++)
				if (objs[i].code.toLowerCase() == code.toLowerCase())
					return i;
		}
		return -1;
	}
	
	if(find_code(cnf[type], item.code) >= 0) {
		if(options.auto
			|| deny(desc + " (" + item.code + ") already exists, continue"))
			return false;
	}

	while (!item.code && !aborted()
		|| (find_code(cnf[type], item.code) >= 0
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
	
	if (type == "xtrn") {
		if (!xtrn_area.sec_list.length)
			return "No external program sections have been created";
		
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
	cnf[type].push(item);
	if (options.debug)
		print(JSON.stringify(cnf[type], null, 4));
	
	print(desc + " (" + item.name + ") installed successfully");
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

	var cnflib = load({}, "cnflib.js");
	var xtrn_cnf = cnflib.read(system.ctrl_dir + "xtrn.cnf");
	if (!xtrn_cnf)
		return "Failed to read " + system.ctrl_dir + "xtrn.cnf";
	
	var startup_dir = ini_fname.substr(0, Math.max(ini_fname.lastIndexOf("/"), ini_fname.lastIndexOf("\\"), 0));
	startup_dir = relpath.get(system.ctrl_dir, startup_dir);

	const types = {
		prog:	{ desc: "External Program", 	struct: "xtrn" },
		event:	{ desc: "External Timed Event", struct: "event" },
		editor:	{ desc: "External Editor",		struct: "xedit" }
	};
	
	for (var t in types) {
		var list = ini_file.iniGetAllObjects("code", t + ":");
		for (var i = 0; i < list.length; i++) {
			var item = list[i];
			if (item.startup_dir === undefined)
				item.startup_dir = startup_dir;
			var result = install_xtrn_item(xtrn_cnf, types[t].struct, name, types[t].desc, item);
			if (typeof result !== 'boolean')
				return result;
			if (result === true)
				installed++;
			else if(item.required)
				return false;
		}
	}
	
	var services_ini = new File(file_cfgname(system.ctrl_dir, "services.ini"));
	var list = ini_file.iniGetAllObjects("protocol", "service:");
	for (var i = 0; i < list.length; i++) {
		var item = list[i];
		var prompt = "Install/enable the " + item.protocol + " service in " + services_ini.name;
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
	}
	
	var list = ini_file.iniGetAllObjects("filename", "ini:");
	for (var i = 0; i < list.length; i++) {
		var item = list[i];
		var a = item.filename.split(':');
		item.filename = file_cfgname(system.ctrl_dir, a[0]);
		item.section = a[1] || null;
		item.keys = item.keys.split(',');
		item.values = item.values.split(',');
		var prompt = "Add/update the " + (item.section || "root") + " section of " + item.filename;
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
			print("Setting " + item.keys[k] + " = " + eval(item.values[k]));
			result = file.iniSetValue(item.section, item.keys[k], eval(item.values[k]));
		}
		file.close();
		if(required && result !== true)
			return false;
	}
	
	var list = ini_file.iniGetAllObjects("cmd", "exec:");
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
			return "Error " + result + " executing " + item.cmd;
	}
	
	var list = ini_file.iniGetAllObjects("str", "eval:");
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
		if (!eval(item.cmd)) {
			if (item.required == true)
				return "Truthful evaluation of '" + item.cmd + "' is required to continue";
		}
	}

	if (installed) {
		if (!options.debug && !cnflib.write(system.ctrl_dir + "xtrn.cnf", undefined, xtrn_cnf))
			return "Failed to write " + system.ctrl_dir + "xtrn.cnf";
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

var xtrn_dirs = fullpath(system.ctrl_dir + "../xtrn/*");
if(!ini_list.length) {
	var dir_list = directory(xtrn_dirs);
	for(var d in dir_list) {
		var fname = file_getcase(dir_list[d] + ini_fname);
		if(fname)
			ini_list.push(fname);
	}
}

if(!ini_list.length) {
	if(options.auto) {
		alert("No install files (" + ini_fname + ") found in " + xtrn_dirs);
		exit(0);
	}
	var ini_path;
	while (!ini_path || !file_exists(ini_path)) {
		ini_path = prompt("Location of " + ini_fname);
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
print("Installed " + installed + " external programs.");
if(installed > 0) {
	print("Requesting Synchronet recycle (configuration-reload)");
	if(!file_touch(system.ctrl_dir + "recycle"))
		alert("Recycle semaphore file update failure");
	if(!this.jsexec_revision) {
		print();
		print("It appears you have run this script from the BBS. You must log-off now for the");
		print("server to recycle and configuration changes to take effect.");
	}
}
