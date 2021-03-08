// List files in a Synchronet v3.19 file base directory

"use strict";

var options = { sort: false};
var detail = -1;
var dir_list = [];
var filespec = "";
var props = [];
var fmt = "%10s %-13s  %-25s  %s";
for(var i = 0; i < argc; i++) {
	var arg = argv[i];
	if(arg[0] == '-') {
		var opt = arg.slice(1);
		if(opt[0] == 'v') {
			var j = 0;
			while(opt[j++] == 'v')
				detail++;
			continue;
		}
		if(opt.indexOf("p=") == 0) {
			props.push(opt.slice(2));
			continue;
		}
		if(opt == "json") {
			fmt = "json";
			continue;
		}
		if(opt.indexOf("fmt=") == 0) {
			fmt = opt.slice(4);
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
if(props.length < 1)
	props = ["size", "name", "from", "desc"];

var output = [];
for(var i in dir_list)
	output = output.concat(listfiles(dir_list[i], filespec, detail, fmt, props));
//if(options.sort)
//	output.sort();
for(var i in output)
	print(output[i]);

function listfiles(dir_code, filespec, detail, fmt, props)
{
	var base = new FileBase(dir_code);
	if(!base.open())
		return base.last_error;
	var output = [];
	if(detail < 0) {
		var list = base.get_file_names(filespec, options.sort);
		if(fmt == 'json')
			output = JSON.stringify(list, null, 4).split('\n');
		else
			output = list;
	} else {
		var list = base.get_file_list(filespec, detail, options.sort);
		if(fmt == 'json')
			output.push(JSON.stringify(list, null, 4));
		else {
			for(var i = 0; i < list.length; i ++)
				output.push(list_file(list[i], fmt, props));
		}
	}
	base.close();
	return output;
}

function list_file(file, fmt, props)
{
	if(typeof file == 'string') {
		print(file);
		return;
	}
	if(fmt === undefined)
		fmt = "%s";
	var a = [fmt];
	for(var i in props) {
		if(file[props[i]] === undefined)
			a.push('');
		else
			a.push(file[props[i]]);
	}
	return format.apply(this, a);
}
