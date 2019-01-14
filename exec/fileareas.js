// $Id$

// *****************************************************************
// Output a file area listing from the BBS's file area configuration
// *****************************************************************

// Description:
// The output of this utility module contains information about file
// "directories", not the files that might be stored in those directories
// (use the 'filelist' utility for that purpose).

// The default behavior is to output a tab-delimited list of the configuration
// properties of all directories in all file libraries in SCFG->File Areas.

// To include column headings, use the -hdr command-line option, for example:
// jsexec fileareas.js -hdr > fileareas.tab

// To restrict the output to specific libraries, specify one or more 
// '-lib <library_name>' options on the command-line.

// To output a comma-delimited file (e.g. suitable for import into spreadsheet
// applications), you may to quote string values, for example:
// jsexec fileareas.js -delim , -quote > fileareas.cvs

// If you specify property names on the command-line, only those configuration
// properties will be included in the output. You may also follow a property
// name with =<format> to specify a printf-style format string to use when
// outputting the property value. You can also *follow* property names with the
// -upr or -lwr options to convert the property value to uppercase or lowercase
// respectively.

// As an example, to generate a RAID/FILEBONE.NA/FILEGATE.ZXX format listing:
// jsexec fileareas.js code="Area %-16s 0 !" -upr description

// For a list of available properties, see this:
// http://synchro.net/docs/jsobjs.html#file_area.lib_list.dir_list_properties

// When using the -json or command-line option, the output will be formatted
// as JSON objects and certain options (e.g. -delim, -hdr, -quote) will have no
// effect on the output. For example, to output a pretty-printed JSON
// representation of all file areas:
// jsexec fileareas.js -json 4 > fileareas.json

// NOTE:
// By default, JSexec will change the current working directory to your
// Synchronet "ctrl" directory before executing the script, so the redirected
// output (stdout) of the example commands above would go into files created
// there. To write output files to a different directory, either specify the
// *full* path or use the jsexec '-C' command-line option to disable the
// change-directory function.

// To load this module from another JavaScript module and capture the output,
// var output = load({}, "fileareas.js", "-quiet");

"use strict";

var options = {
	hdr: false,
	sort: false,
	quote: false,
	quiet: false,
};
var json;
var delim = '\t';
var props = [];
var fmt = {}
var upr = [];
var lwr = [];
var lib = [];

for(var i = 0; i < argc; i++) {
	if(argv[i].charAt(0) != '-') {
		var arg = argv[i]; 
		var eq = arg.indexOf('=');
		if(eq >= 0) {
			if(eq < 1)
				throw("invalid format: " + arg);
			arg = arg.slice(0, eq);
			fmt[arg] = argv[i].slice(eq + 1);
		}
		props.push(arg);
		continue;
	}
	switch(argv[i]) {
		case '-lib':
			lib.push(argv[++i].toLowerCase());
			continue;
		case '-fmt':	// Alternate syntax to <prop>=<fmt>
			if(props.length > 0)
				fmt[props[props.length - 1]] = argv[++i];
			else
				throw(argv[i] + " must follow a property specification");
			continue;
		case '-upr':
			if(props.length > 0)
				upr[props[props.length - 1]] = true;
			else
				throw(argv[i] + " must follow a property specification");
			continue;
		case '-lwr':
			if(props.length > 0)
				lwr[props[props.length - 1]] = true;
			else
				throw(argv[i] + " must follow a property specification");
			continue;
		case '-delim':
			delim = argv[++i];
			continue;
		case '-json':
			var spacing = parseInt(argv[i + 1], 10);
			if(!isNaN(spacing))
				json = spacing, i++;
			else
				json = 0;
			break;
		default:
			var arg = argv[i].slice(1);
			if(options[arg] === undefined) {
				writeln("usage: fileareas.js [[-lib <name>] [...]] [[-option] [...]] [[[prop][=<format> [-upr | -lwr]] [...]]");
				writeln("options:");
				writeln(format("\t%-12s <default>", "<option>"));
				for(var o in options)
					writeln(format("\t%-12s %s", '-' + o, JSON.stringify(options[o])));
				writeln(format("\t%-12s \\t", "-delim"));
				writeln(format("\t%-12s undefined (specify a numeric argument for pretty-printing)", "-json"));
				exit(0);
			}
			var value = parseInt(argv[i + 1], 10);
			if(value >= 0)
				options[arg] = value, i++;
			else
				options[arg] = true;
			continue;
	}
}

var output = [];
for(var i in file_area.dir) {
	var area = file_area.dir[i];
	if(lib.length && lib.indexOf(area.lib_name.toLowerCase()) < 0)
		continue;
	var obj = {};
	if(props.length == 0)
		obj = area;
	else
		for(var f in props) {
			var value = area[props[f]];
			if(typeof value == 'string') {
				if(upr[props[f]])
					value = value.toUpperCase();
				else if(lwr[props[f]])
					value = value.toLowerCase();
			}
			obj[props[f]] = value;
		}
	var line = '';
	if(json !== undefined)
		line = JSON.stringify(obj, null, json);
	else {
		if(options.hdr == true && !output.length) {
			for(var f in obj) {	
				if(fmt[f])
					write(format(fmt[f], f));
				else
					write(f);
				write(delim);
			}
			writeln();
		}
		for(var f in obj) {
			var value = obj[f];
			if(value === undefined)
				throw(f + ' is undefined');
			if(options.quote)
				value = JSON.stringify(value);
			if(line.length)
				line += delim;
			if(fmt[f])
				line += format(fmt[f], value);
			else
				line += value;
		}
	}
	if(js.global.bbs)
		line = lfexpand(line);
	output.push(line);
}

if(options.sort)
	output.sort();
if(!options.quiet)
	for(var i in output)
		writeln(output[i]);

output;