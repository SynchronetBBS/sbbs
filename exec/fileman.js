// Synchronet File Manager

// run using jsexec

require("file_size.js", "file_size_float");
require("uifcdefs.js", "WIN_ORG");
require("sbbsdefs.js", "K_EDIT");

"use strict";

if(!uifc.init("Synchronet File Manager")) {
	alert("uifc init failure");
	exit(-1);
}
js.on_exit("uifc.bail()");

const main_ctx = new uifc.list.CTX;
while(!js.terminated) {
	const items = [
		"Browse",
		"Search filenames",
		"Search meta data",
		"Search uploader",
		"Search offline files"
	];
	try {
		var result = uifc.list(WIN_ORG | WIN_ACT, "Operations", items, main_ctx);

		if(result < 0)
			break;

		switch(result) {
			case 0:
				browse();
				break;
			case 1:
				search("Filename Pattern", find_filename);
				break;
			case 2:
				search("Text", find_metadata);
				break;
			case 3:
				search("Name", find_uploader);
				break;
			case 4:
				search(undefined, find_offline);
				break;
		}
	} catch(e) {
		uifc.msg(e);
		continue;
	}
}

function browse()
{
	const ctx = new uifc.list.CTX;
	while(!js.terminated) {
		var items = [];
		for(var i in file_area.lib)
			items.push(file_area.lib[i].name);
		var result = uifc.list(WIN_RHT|WIN_ACT, "File Libraries", items, ctx);
		if(result == -1)
			break;
		browse_lib(result);
	}
}

function search(prompt, func)
{
	var pattern;

	if(prompt) {
		pattern = uifc.input(WIN_SAV|WIN_MID, prompt);
		if(!pattern)
			return;
	}
	uifc.pop("Searching...");
	var found = [];
	for(var i in file_area.dir) {
		var dir = file_area.dir[i];
		var list = func(dir.code, pattern);
		if(list && list.length)
			found = found.concat(list);
	}
	uifc.pop();
	if(found.length)
		list_files(found.length + " " + (pattern || "offline") + " files found", found);
}

function find_filename(dircode, pattern)
{
	var base = new FileBase(dircode);
	if(!base.open()) {
		uifc.msg("Unable to open base: " + dircode);
		return;
	}
	var found = base.get_list(pattern);

	base.close();

	for(var i in found)
		found[i].dircode = dircode;

	return found;
}

function find_metadata(dircode, pattern)
{
	var base = new FileBase(dircode);
	if(!base.open()) {
		uifc.msg("Unable to open base: " + dircode);
		return;
	}
	var list = base.get_list();
	var found = [];
	pattern = pattern.toUpperCase();
	for(var i in list) {
		var file = list[i];
		if((file.desc && file.desc.toUpperCase().indexOf(pattern) >= 0)
		|| (file.extdesc && file.extdesc.toUpperCase().indexOf(pattern) >= 0)
		|| (file.tags && file.tags.toUpperCase().indexOf(pattern) >= 0)) {
			file.dircode = dircode;
			found.push(file);
		}
	}
	base.close();

	return found;
}

function find_uploader(dircode, name)
{
	var base = new FileBase(dircode);
	if(!base.open()) {
		uifc.msg("Unable to open base: " + dircode);
		return;
	}
	var list = base.get_list();
	var found = [];
	name = name.toUpperCase();
	for(var i in list) {
		var file = list[i];
		if(file.from && file.from.toUpperCase() == name) {
			file.dircode = dircode;
			found.push(file);
		}
	}
	base.close();

	return found;
}

function find_offline(dircode)
{
	var base = new FileBase(dircode);
	if(!base.open()) {
		uifc.msg("Unable to open base: " + dircode);
		return;
	}
	var list = base.get_list();
	var found = [];
	for(var i in list) {
		var file = list[i];
		if(!file_exists(base.get_path(file))) {
			file.dircode = dircode;
			found.push(file);
		}
	}
	base.close();

	return found;
}

function browse_lib(libnum)
{
	const ctx = new uifc.list.CTX;
	var lib = file_area.lib_list[libnum];
	while(!js.terminated) {
		var items = [];
		for(var i in lib.dir_list)
			items.push(lib.dir_list[i].name);
		var result = uifc.list(WIN_SAV|WIN_ACT, lib.name + " Directories", items, ctx);
		if(result == -1)
			break;
		browse_dir(lib.dir_list[result].code);
	}
}

function browse_dir(dircode)
{
	var base = new FileBase(dircode);
	if(!base.open()) {
		uifc.msg("Unable to open base: " + dircode);
		return;
	}

	var list = base.get_list();
	base.close();
	list_files(file_area.dir[dircode].name + " Files", list, dircode);
}

function list_files(title, list, dircode)
{
	if(!list || !list.length) {
		uifc.msg("No files in " + title);
		return;
	}
	const ctx = new uifc.list.CTX;
	while(!js.terminated) {
		var items = [];
		var namelen = 0;
		for(var i in list)
			if(list[i].name.length > namelen)
				namelen = list[i].name.length;
		for(var i in list)
			items.push(format("%-*s  %s", namelen, list[i].name, list[i].desc || list[i].extdesc || ""));
		var result = uifc.list(WIN_SAV | WIN_RHT | WIN_ACT | WIN_EDIT | WIN_DEL | WIN_DELACT, title, items, ctx);
		if(result == -1)
			break;
		if(result & MSK_ON) {
			var op = result & MSK_ON;
			result &= MSK_OFF;
			var file = list[result];
			switch(op) {
				case MSK_DEL:
					var opts = [ "No", "Yes" ];
					exists = fexists(file, dircode);
					if(exists)
						opts.push("From Database Only");
					var choice = uifc.list(WIN_SAV, "Remove " + file.name, opts);
					if(choice < 1)
						break;
					if(remove(file, exists && choice == 1, dircode) == true)
						list.splice(result, /* deleteCount: */1);
					break;
				case MSK_EDIT:
					var filename = file.name;
					if(edit(file, dircode))
						save(file, dircode, filename);
					break;
			}
			continue;
		}
		dump(list[result], dircode);
	}
}

function fexists(file, dircode)
{
	if(!dircode)
		dircode = file.dircode;
	var base = new FileBase(dircode);
	if(!base.open()) {
		uifc.msg("Unable to open base: " + dircode);
		return;
	}
	var result = file_exists(base.get_path(file));
	base.close();
	return result;
}

function remove(file, del, dircode)
{
	if(!dircode)
		dircode = file.dircode;
	var base = new FileBase(dircode);
	if(!base.open()) {
		uifc.msg("Unable to open base: " + dircode);
		return;
	}
	var result = false;
	var path = base.get_path(file);
	if(!del || file_exists(path))
		result = base.remove(file.name, del);
	else
		uifc.msg(path + " does not exist");
	base.close();
	return result;
}

function dump(file, dircode)
{
	if(!dircode)
		dircode = file.dircode;
	var base = new FileBase(dircode);
	if(!base.open()) {
		uifc.msg("Unable to open base: " + dircode);
		return;
	}
	var buf = [ base.get_path(file) ];
	buf.push(format("Size             " + file_size_float(base.get_size(file), 1, 1)));
	buf.push(format("Time             " + system.timestr(base.get_time(file))));
	uifc.showbuf(0, file.name, buf.concat(base.dump(file.name)).join('\n'));
	base.close();
}

function edit(file, dircode)
{
	const ctx = new uifc.list.CTX;
	var orig = JSON.parse(JSON.stringify(file));
	while(!js.terminated) {
		var opts = [ "Rename File..."
			,"Description: " + (file.desc || "")
			,"Uploader: " + (file.from || "")
			];
		if(JSON.stringify(file) != JSON.stringify(orig))
			opts.push("Save changes...");
		var choice = uifc.list(WIN_SAV|WIN_ACT, file.name, opts, ctx);
		if(choice < 0)
			break;
		switch(choice) {
			case 0:
				var name = uifc.input(WIN_MID|WIN_SAV, "Filename", file.name, 100, K_EDIT);
				if(!name)
					break;
				file.name = name;
				break;
			case 1:
				var desc = uifc.input(WIN_MID|WIN_SAV, "Description", file.desc, LEN_FDESC, K_EDIT);
				if(!desc)
					break;
				file.desc = desc;
				break;
			case 2:
				var from = uifc.input(WIN_MID|WIN_SAV, "Uploader", file.from, LEN_ALIAS, K_EDIT);
				if(!from)
					break;
				file.from = from;
				break;
			case 3:
				if(save(file, dircode, orig.name))
					orig = JSON.parse(JSON.stringify(file));
				break;
		}
	}
	return false;
}

function save(file, dircode, filename)
{
	if(!dircode)
		dircode = file.dircode;
	var base = new FileBase(dircode);
	if(!base.open()) {
		uifc.msg("Unable to open base: " + dircode);
		return false;
	}
	var result = false;
	try {
		result = base.update(filename, file);
		if(!result)
			uifc.msg("update failure: " + base.status + " " + filename);
	} catch(e) {
		uifc.msg(e);
	}
	base.close();
	return result;
}
