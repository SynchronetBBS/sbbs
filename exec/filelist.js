// List files in a Synchronet v3.19 file base directory

"use strict";

var options = { sort: false};
var detail = -1;
var dir_list = [];
var filespec = "";
var props = [];
var fmt;
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
		if(opt == "arc") {
			fmt = "arc";
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
	props = ["name", "size", "from", "desc", "extdesc"];
if(!fmt) {
	fmt = "%-13s %10s  %-25s  %s";
	if(detail > 1)
		fmt += "\n%s";
}

var output = [];
for(var i in dir_list) {
	var dir_code = dir_list[i];
	var dir = file_area.dir[dir_code];
	if(!dir) {
		alert("dir not found: " + dir_code);
		continue;
	}
	if(options.hdr) {
		var hdr = format("%-15s %-40s Files: %d", dir.lib_name, dir.description, dir.files);
		output.push(hdr);
		output.push(format("%.*s", hdr.length
			, "-------------------------------------------------------------------------------"));
	}
	output = output.concat(listfiles(dir_code, filespec, detail, fmt, props));
}
//if(options.sort)
//	output.sort();
for(var i in output)
	print(output[i]);

function archive_contents(path, list)
{
	var output = [];
	for(var i = 0; i < list.length; i++) {
		var fname = path + list[i];
		print(fname);
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

function listfiles(dir_code, filespec, detail, fmt, props)
{
	var base = new FileBase(dir_code);
	if(!base.open())
		return base.last_error;
	var output = [];
	if(detail < 0) {
		var list = base.get_names(filespec, options.sort);
		if(fmt == 'json')
			output = JSON.stringify(list, null, 4).split('\n');
		else if(fmt == 'arc')
			output = archive_contents(file_area.dir[dir_code].path, list);
		else
			output = list;
	} else {
		var list = base.get_list(filespec, detail, options.sort);
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
