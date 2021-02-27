// $Id: exportcfg.js,v 1.4 2019/02/15 11:11:07 rswindell Exp $
// vi: tabstop=4

// *****************************************************************
// Output a list of configuration items (e.g. msg areas, file areas)
// *****************************************************************

"use strict";

// global options (per list)
var options = {
	hdr: false,
	ini: false,
	delim: '\t',	// field delimiter
	term: '',		// record terminator
	quote: false,	// JS-encode strings
	strip: false,	// Strip Ctrl/Ctrl-A codes from strings
	ascii: false,	// Convert to ASCII
	quiet: false,
	sort: false,
	json: false,
};
const defaults = JSON.parse(JSON.stringify(options));
// property options (per property)
var popts = {
	upper: [],		// uppercase
	lower: [],		// lowercase
	under: [],		// replace space with underscore
};
var props = [];
var	propfmt = {};	// printf-style format string
var propex = [];	// properties to exclude
var grp = [];
var include = [];	// items to include
var exclude = [];	// items to exclude

var cnflib = load({}, "cnflib.js");
var file_cnf = cnflib.read("file.cnf");

var cfgtype;
var cfgtypes = {
	'msg-grps': 	msg_area.grp,
	'msg-subs':		msg_area.sub,
	'file-libs': 	file_area.lib,
	'file-dirs':	file_area.dir,
	'file-prots':	file_cnf.prot,
	'file-extrs':   file_cnf.fextr,
	'file-comps':   file_cnf.fcomp,
	'file-viewers': file_cnf.fview,
	'file-testers': file_cnf.ftest,
	'file-dlevents':file_cnf.dlevent,
	'xtrn-secs': 	xtrn_area.sec,
	'xtrn-progs': 	xtrn_area.prog,
	'xtrn-events': 	xtrn_area.event,
	'xtrn-editors':	xtrn_area.editor,
};

function usage(msg)
{
	if(msg) {
		writeln();
		alert(msg);
		writeln();
	}
	writeln("usage: exportcfg.js <cfg-type>");
	writeln("\t[[-grp=<msg_area.grp.name | file_area.lib.name | xtrn_area.sec.code>] [...]]");
	writeln("\t[[-inc=<internal_code> | -exc=<internal_code>] [...]]");
	writeln("\t[-<option>[=<value>] [...]]");
	writeln("\t[[[property][=<printf-format> [-upper | -lower | -under]] [...]]");
	writeln("\t[[-ex=<property>] [...]]");
	writeln();
	writeln("cfg-types (choose one):");
	for(var c in cfgtypes)
		writeln("\t" + c);
	writeln();
	writeln("options:");
	writeln(format("\t%-12s <default>", "<option>"));
	for(var o in defaults)
		writeln(format("\t%-12s =%s", '-' + o, JSON.stringify(defaults[o])));
	writeln("\t(tip: use -json=4 for pretty JSON output)");
	writeln("\t(tip: use -ini='\\t%s = %s\\n' for pretty .ini output)");
	writeln();
	writeln("per-property options:");
	writeln(format("\t%-12s <default>", "<option>"));
	for(var o in popts)
		writeln(format("\t%-12s =%s", '-' + o, popts[o].length !== undefined ? 'false' : ''));
	exit(0);
}

for(var i = 0; i < argc; i++) {
	var value = undefined;
	var arg = argv[i];
	var eq = arg.indexOf('=');
	if(eq >= 0) {
		if(eq < 1)
			throw("invalid format: " + arg);
		arg = arg.slice(0, eq);
		value = argv[i].slice(eq + 1);
	}
	if(arg.charAt(0) != '-') {
		if(cfgtype === undefined) {
			if(cfgtypes[arg] === undefined)
				usage("unsuppoted cfg-type: " + arg);
			cfgtype = arg;
		 } else {
			props.push(arg);
			if(value !== undefined)
				propfmt[arg] = JSON.parse('"' + value + '"');
		}
		continue;
	}
	while(arg.charAt(0) == '-')
		arg = arg.slice(1);
	switch(arg) {
		case 'inc':	// include item (by code)
			if(value === undefined)
				value = argv[++i];
			include.push(value.toLowerCase());
			continue;
		case 'exc':	// exclude item (by code)
			if(value === undefined)
				value = argv[++i];
			exclude.push(value.toLowerCase());
			continue;
		case 'lib':
		case 'grp':
		case 'sec':
			if(value === undefined)
				value = argv[++i];
			grp.push(value.toLowerCase());
			continue;
		case 'ex':	// exclude property
			if(value === undefined)
				value = argv[++i];
			propex.push(value);
			continue;
		case '?':
		case 'help':
			usage();
			break;
		default:
			if(options[arg] === undefined && popts[arg] === undefined) {
				usage('unrecognized option: ' + arg);
			}
			if(popts[arg] !== undefined) {
				if(props.length < 1)
					throw(argv[i] + " must follow a property specification");
				popts[arg][props[props.length -1]] = true;
				continue;
			}
			if(value !== undefined) {
				if(typeof options[arg] == 'boolean') {
					try {
						value = eval(value);
					} catch(err) {}
				}
				options[arg] = JSON.parse('"' + value + '"');			// support C-encoded chars (e.g. \t\r\n)
			} else {
				if(typeof options[arg] == 'string') {
					options[arg] = JSON.parse('"' + argv[++i] + '"');	// support C-encoded chars (e.g. \t\r\n)
					if(options[arg] === undefined)
						throw("option value undefined: " + arg);
				} else {
					if(typeof options[arg] == 'boolean') {
						try {
							value = eval(argv[i + 1]);
						} catch(err) {}
						if(typeof value == 'boolean') {
							options[arg] = value, i++;
							continue;
						}
					}
					var value = parseInt(argv[i + 1], 10);
					if(value >= 0)
						options[arg] = value, i++;
					else
						options[arg] = true;
				}
			}
			continue;
	}
}

if(!cfgtype)
	usage("cfg-type not specified");

if(0) {
	writeln("props");
	writeln(JSON.stringify(props, null, 4));
	writeln();

	writeln("popts");
	writeln(JSON.stringify(popts, null, 4));
	writeln();

	writeln("exclude");
	writeln(JSON.stringify(exclude, null, 4));
	writeln();
}

var cfglist = cfgtypes[cfgtype];
var output = [];
for(var i in cfglist) {
	var item = cfglist[i];
	if(grp.length) {
		switch(cfgtype) {
			case 'msg-subs':
				if(grp.indexOf(item.grp_name.toLowerCase()) < 0)
					continue;
				break;
			case 'file-dirs':
				if(grp.indexOf(item.lib_name.toLowerCase()) < 0)
					continue;
				break;
			case 'xtrn-progs':
				if(grp.indexOf(item.sec_code.toLowerCase()) < 0)
					continue;
				break;
		}
	}
	if(include.length && include.indexOf(i) < 0)
		continue;
	if(exclude.length && exclude.indexOf(i) >= 0)
		continue;
	var obj = {};
	if(props.length == 0) {
		obj = item; //JSON.parse(JSON.stringify(item));
		if(obj.code === undefined)
			obj.code = i;
	}
	else {
		for(var f in props) {
			var value = item[props[f]];
			if(value === undefined && props[f] == 'code')
				value = i;
			if(typeof value == 'string') {
				if(popts.under[props[f]])
					value = value.replace(' ', '_');
				if(popts.upper[props[f]])
					value = value.toUpperCase();
				else if(popts.lower[props[f]])
					value = value.toLowerCase();
			}
			obj[props[f]] = value;
		}
	}
	// Remove excluded properties
	for(var e in propex)
		delete obj[propex[e]];
	for(var p in obj) {
		if(typeof obj[p] == 'object')
			delete obj[p];
	}
	var text = '';
	if(options.json !== false)
		text = JSON.stringify(obj, null, parseInt(options.json, 10));
	else {
		if(options.ini) {
			var section = item.code;
			if(!section)
				section = item.name;
			text += format("[%s]\n", section);
		}
		else if(options.hdr == true && !output.length) {
			for(var f in obj) {
				if(propfmt[f])
					write(format(propfmt[f], f));
				else
					write(f);
				write(options.delim);
			}
			writeln();
		}
		for(var f in obj) {
			var value = obj[f];
			if(value === undefined) {
				if(props.length)
					throw(f + ' is undefined');
				continue;
			}
			if(options.strip)
				value = strip_ctrl(value);
			if(options.ascii)
				value = ascii_str(value);
			if(options.quote)
				value = JSON.stringify(value);
			if(propfmt[f])
				value = format(propfmt[f], value);
			if(options.ini)
				text += format(typeof options.ini == 'string' ? options.ini : "%s = %s\n", f, value);
			else {
				if(text.length)
					text += options.delim;
				text += value;
			}
		}
		if(options.term)
			text += options.term;
	}
	if(js.global.bbs)
		line = lfexpand(text);
	output.push(text);
}

if(options.sort)
	output.sort();
if(!options.quiet)
	for(var i in output)
		writeln(output[i]);

output;

