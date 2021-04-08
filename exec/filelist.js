// List files in one or more Synchronet v3.19 file base directories

load("file_size.js");
load("string.js");

"use strict";

function datestr(t)
{
	if(date_fmt)
		return strftime(date_fmt, t);
	return system.datestr(t);
}

var sort_prop = "name";
var exclude_list = [];
var options = { sort: false};
var json_space;
var detail = -1;
var dir_list = [];
var filespec = "";
var extdesc_offset = 25;
var since = 0;
var props = [];
var fmt;
var date_fmt;
var byte_fmt = 2;
var byte_fmts = [
//	 bytes,		precision
	[undefined, undefined 	],
	[false,		0			],
	[false,		1			],
	[false,		2			],
	[1024,		0			],
	[1024,		1			],
	[1024,		2			],
	[1024*1024,	0			],
	[1024*1024,	1			],
	[1024*1024,	2			],
	[1024*1024,	3			],
	[true,		undefined	],
	[false,		undefined	],
	];
for(var i = 0; i < argc; i++) {
	var arg = argv[i];
	if(arg[0] == '-') {
		var opt = arg;
		while(opt[0] == '-')
			opt = opt.slice(1);
		if(opt == '?' || opt.toLowerCase() == "help") {
			writeln("usage: [-options] [dir-code [...]] [include-filespec=*]");
			writeln("options:");
			writeln("  -all            list files in all libraries/directories");
			writeln("  -lib=<name>     list files in all directories of specified library");
			writeln("  -ex=<filespec>  add filename or spec to excluded filename list");
			writeln("  -hdr            include header for each directory");
			writeln("  -sort[=prop]    enable case-sensitive output sorting");
			writeln("  -isort[=prop]   enable case-insensitive output sorting");
			writeln("  -json[=spaces]  use JSON formatted output");
			writeln("  -new=<days>     include new files uploaded in past <days>");
			writeln("  -date=<fmt>     specify date/time display format (strftime syntax)");
			writeln("  -p=[list]       specify comma-separated list of property names to print");
			writeln("  -fmt=<fmt>      specify output format string (printf syntax)");
			writeln("  -ext            include extended file descriptions");
			writeln("  -v              increase verbosity (detail) of output");
			exit(0);
		}
		if(opt.indexOf("ex=") == 0) {
			exclude_list.push(opt.slice(3));
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
			options.auto = true;
			continue;
		}
		if(opt.indexOf("new=") == 0) {
			var days  = parseInt(opt.slice(4), 10);
			since = time() - (days * (24 * 60 * 60));
			continue;
		}
		if(opt.indexOf("pre=") == 0) {
			precision = parseInt(opt.slice(4), 10);
			continue;
		}
		if(opt.indexOf("date=") == 0) {
			date_fmt = opt.slice(5);
			continue;
		}
		if(opt.indexOf("unit=") == 0) {
			unit = parseInt(opt.slice(5), 10);
			continue;
		}
		if(opt.indexOf("sort=") == 0) {
			sort_prop = opt.slice(5);
			options.sort = true;
			continue;
		}
		if(opt.indexOf("isort=") == 0) {
			sort_prop = opt.slice(6);
			options.isort = true;
			continue;
		}
		if(opt[0] == 'v') {
			var j = 0;
			while(opt[j++] == 'v')
				detail++;
			continue;
		}
		if(opt.indexOf("ext=") == 0) {
			extdesc_offset = parseInt(opt.slice(4), 10);
			detail = FileBase.DETAIL.EXTENDED;
			continue;
		}
		if(opt == "ext") {
			detail = FileBase.DETAIL.EXTENDED;
			continue;
		}
		if(opt.indexOf("p=") == 0) {
			props = props.concat(opt.slice(2).split(','));
			continue;
		}
		if(opt == "json") {
			fmt = "json";
			json_space = 4;
			continue;
		}
		if(opt.indexOf("json=") == 0) {
			fmt = "json";
			json_space = parseInt(opt.slice(5), 10);
			continue;
		}
		if(opt == "arc") {
			fmt = "arc";
			continue;
		}
		if(opt.indexOf("fmt=") == 0) {
			fmt = opt.slice(4);
			continue;
		}
		if(opt.indexOf("b=") == 0) {
			byte_fmt = parseInt(opt.slice(2), 10);
			if(byte_fmt >= byte_fmts.length)
				byte_fmt = 0;
			continue;
		}
		if(opt == "all") {
			for(var dir in file_area.dir)
				dir_list.push(dir);
			continue;
		}
		options[opt] = true;
		continue;
	}
	if(file_area.dir[arg])
		dir_list.push(arg);
	else
		filespec = arg;
}
if(fmt != "json") {

	if(options.hdr || props.length)
		detail = FileBase.DETAIL.EXTENDED; //Math.max(0, detail);

	if(!props.length) {
		var f = "%-13s";
		props.push("name");
		if(detail >= FileBase.DETAIL.MIN) {
			props.push("size");
			f += "  %10s";
		}
		if(detail >= FileBase.DETAIL.NORM) {
			props.push("desc");
			f += "  %-58s";
		}
		if(detail >= FileBase.DETAIL.EXTENDED) {
			props.push("extdesc");
			f += "%s";
		}
		if(!fmt)
			fmt = f;
	}
	if(!fmt)
		fmt = "%s ".repeat(props.length);
}

if(!dir_list.length) {
	var code;
	while(!file_area.dir[code] && !js.terminated) {
		for(var d in file_area.dir)
			writeln(d);
		code = prompt("Directory code");
	}
	dir_list.push(code);
}

log("Reading " + dir_list.length + " file areas...");
var dir_code;
var file_list = [];
for(var i = 0; i < dir_list.length; i++) {
	dir_code = dir_list[i];
	var base = new FileBase(dir_code);
	if(!base.open())
		throw new Error(base.last_error);
	if(detail < 0)
		file_list = file_list.concat(base.get_names(filespec, since, /* sort: */false));
	else {
		var list = base.get_list(filespec, detail, since, /* sort: */false);
		for(var j = 0; j < list.length; j++) {
			list[j].dir = dir_code;
			if(list[j].extdesc)
				list[j].extdesc = "\n" + list[j].extdesc;
		}
		file_list = file_list.concat(list);
	}
	base.close();
}

if(!file_list.length)
	exit(0);

if(options.isort) {
	log("Sorting " + file_list.length + " files...");
	if(typeof file_list[0] == "string")
		file_list.sort(
			function(a, b) {
				if (a.toLowerCase() < b.toLowerCase()) return -1;
				if (a.toLowerCase() > b.toLowerCase()) return 1;
				return 0;
			}
		);
	else {
		file_list.sort(
			function(a, b) {
				var a = String(a[sort_prop]).toLowerCase();
				var b = String(b[sort_prop]).toLowerCase();
				if (a < b) return -1;
				if (a > b) return 1;
				return 0;
			}
		);
	}
}
else if(options.sort) {
	log("Sorting " + file_list.length + " files...");
	if(typeof file_list[0] == "string")
		file_list.sort();
	else {
		file_list.sort(
			function(a, b) {
				if (a[sort_prop] < b[sort_prop]) return -1;
				if (a[sort_prop] > b[sort_prop]) return 1;
				return 0;
			}
		);
	}
}

log("Formatting " + file_list.length + " files for output...");
if(fmt == "json") {
	writeln(JSON.stringify(file_list, null, json_space));
	exit(0);
}
var output = [];
dir_code = undefined;
for(var i = 0; i < file_list.length; i++) {
	var file = file_list[i];
	switch(typeof file) {
		case "string":
			if(excluded(file))
				continue;
			break;
		case "object":
			if(excluded(file.name))
				continue;
			break;
		default:
			throw new Error(typeof file);
			break;
	}
	if(typeof file == "object" && options.hdr && dir_code != file.dir) {
		dir_code = file.dir;
		var dir = file_area.dir[dir_code];
		if(i)
			output.push("");
		var hdr = format("%-15s %-40s Files: %d", dir.lib_name, dir.description, dir.files);
		output.push(hdr);
		output.push(format("%.*s", hdr.length
			, "-------------------------------------------------------------------------------"));
	}
	output.push(list_file(file, fmt, props));
}

for(var i in output)
	writeln(output[i]);

function archive_contents(path, list)
{
	var output = [];
	for(var i = 0; i < list.length; i++) {
		var fname = path + list[i];
		writeln(fname);
		output.push(fname);
		var contents;
		try {
			contents = Archive(fname).list();
		} catch(e) {
//			alert(e);
			continue;
		}
		for(var j = 0; j < contents.length; j++)
			output.push(contents[j].name + " " + contents[j].size);
	}
	return output;
}

function list_file(file, fmt, props)
{
	if(typeof file == 'string')
		return file;
	if(fmt === undefined)
		fmt = "%s";
	var a = [fmt];
	for(var i in props) {
		var p = file[props[i]];
		switch(typeof p) {
			case "undefined":
				a.push('');
				break;
			case "string":
				if(props[i] == 'extdesc')
					a.push(p.replace(/([^\n]+)/g, format("%*s| $&", extdesc_offset, "")).trimRight());
				else
					a.push(p);
				break;
			case "number":
				switch(props[i]) {
					case 'time':
					case 'added':
					case 'last_downloaded':
						a.push(datestr(p));
						break;
					case 'crc32':
						a.push(format("%08lx", p));
						break;
					case 'crc16':
						a.push(format("%04lx", p));
						break;
					case 'size':
					case 'cost':
						if(byte_fmts[byte_fmt][0] !== undefined
							|| byte_fmts[byte_fmt][1] !== undefined) {
							a.push(file_size_str(p
								,/* unit: */ byte_fmts[byte_fmt][0]
								,/* precision: */ byte_fmts[byte_fmt][1]));
							break;
						}
					default:
						a.push(p);
						break;
				}
				break;
		}
	}
	return format.apply(this, a);
}

function excluded(fname)
{
	for(var i = 0; i < exclude_list.length; i++)
		if(wildmatch(fname, exclude_list[i]))
			return true;
	return false;
}
