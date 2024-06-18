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
		"Search Filenames",
		"Search Meta Data",
		"Search Uploader",
		"Search Offline Files"
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

function select_dir()
{
	const lib_ctx = new uifc.list.CTX;
	while(!js.terminated) {
		var items = [];
		for(var i in file_area.lib)
			items.push(file_area.lib[i].name);
		var libnum = uifc.list(WIN_RHT|WIN_ACT|WIN_SAV, "File Libraries", items, lib_ctx);
		if(libnum == -1)
			break;
		const dir_ctx = new uifc.list.CTX;
		var lib = file_area.lib_list[libnum];
		while(!js.terminated) {
			var items = [];
			for(var i in lib.dir_list)
				items.push(lib.dir_list[i].name);
			var dirnum = uifc.list(WIN_SAV|WIN_ACT, lib.name + " Directories", items, dir_ctx);
			if(dirnum == -1)
				break;
			return lib.dir_list[dirnum].code;
		}
	}
	return null;
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
	list_files(file_area.dir[dircode].name + format(" Files (%u)", list.length), list, dircode);
}

function list_files(title, list, dircode)
{
	if(!list || !list.length) {
		uifc.msg("No files in " + title);
		return;
	}
	const wide_screen = uifc.screen_width >= 100;
	const ctx = new uifc.list.CTX;
	while(!js.terminated) {
		var items = [];
		var namelen = 12;
		var tagged = 0;
		for(var i in list) {
			if(wide_screen && list[i].name.length > namelen)
				namelen = list[i].name.length;
			if(list[i].tagged)
				++tagged;
		}
		for(var i in list)
			items.push(format("%-*s%-*s  %s"
				,tagged ? 2:0
				,list[i].tagged ? ascii(251):""
				,namelen, wide_screen ? list[i].name : FileBase().format_name(list[i].name)
				,list[i].desc || list[i].extdesc || ""));
		var result = uifc.list(WIN_SAV | WIN_RHT | WIN_ACT | WIN_DEL | WIN_DELACT | WIN_TAG
			,title, items, ctx);
		if(result == -1)
			break;
		if(result == MSK_TAGALL) {
			var tag = !Boolean(list[0].tagged);
			for(var i in list)
				list[i].tagged = tag;
			continue;
		}
		if(result & MSK_ON) {
			var op = result & MSK_ON;
			result &= MSK_OFF;
			var file = list[result];
			switch(op) {
				case MSK_TAG:
					list[result].tagged = !list[result].tagged;
					++ctx.cur;
					++ctx.bar;
					break;
				case MSK_DEL:
					var opts = [ "No", "Yes" ];
					if(tagged > 0) {
						opts.push("From Database Only");
						var choice = uifc.list(WIN_SAV, "Remove " + tagged + " files", opts);
						if(choice < 1)
							break;
						for(var i = list.length - 1; i >= 0; --i) {
							file = list[i];
							if(list[i].tagged === true) {
								exists = fexists(file, dircode);
								if(remove(file, exists && choice == 1, dircode) == true)
									list.splice(i, /* deleteCount: */1);
							}
						}
					} else {
						exists = fexists(file, dircode);
						if(exists)
							opts.push("From Database Only");
						var choice = uifc.list(WIN_SAV, "Remove " + file.name, opts);
						if(choice < 1)
							break;
						if(remove(file, exists && choice == 1, dircode) == true)
							list.splice(result, /* deleteCount: */1);
					}
					break;
			}
			continue;
		}
		if(tagged > 0) {
			multi(list, dircode, tagged);
		} else { // Single-file selected
			var file = list[result];
			if(edit(file, dircode)) // file (re)moved?
				list.splice(result, /* deleteCount: */1);
		}
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
//	file = base.get(file, FileBase.DETAIL.AUXDATA);
	var buf = [];
	if(file.extdesc)
		buf = buf.concat(truncsp(file.extdesc).split('\n'));
	buf.push(format("Size             " + file_size_float(base.get_size(file), 1, 1)));
	buf.push(format("Time             " + system.timestr(base.get_time(file))));
	uifc.showbuf(WIN_MID|WIN_HLP, base.get_path(file), buf.concat(base.dump(file.name)).join('\n'));
	base.close();
}

function view_archive(file, dircode)
{
	if(!dircode)
		dircode = file.dircode;
	var base = new FileBase(dircode);
	if(!base.open()) {
		uifc.msg("Unable to open base: " + dircode);
		return;
	}
	try {
		var list = new Archive(base.get_path(file)).list();
		var buf = [];
		var namelen = 0;
		for(var i in list)
			if(list[i].name.length > namelen)
				namelen = list[i].name.length;
		for(var i in list)
			buf.push(format("%-*s  %10u  %s"
				,namelen, list[i].name, list[i].size
				,system.timestr(list[i].time).slice(4)));
		uifc.showbuf(WIN_MID|WIN_HLP, base.get_path(file), buf.join('\n'));
	} catch(e) {
		uifc.msg(e);
	}
	base.close();
}

function confirm(prompt)
{
	var choice = uifc.list(WIN_MID|WIN_SAV, prompt, [ "Yes", "No" ]);
	if(choice >= 0)
		choice = choice == 0;
	return choice;
}

function edit(file, dircode)
{
	const ctx = new uifc.list.CTX;
	var orig = JSON.parse(JSON.stringify(file));
	while(!js.terminated) {
		var opts = [
			"View Details...",
			"View Archive...",
			"Filename: " + file.name,
			"Description: " + (file.desc || ""),
			"Uploader: " + (file.from || ""),
			"Added: " + system.timestr(file.added).slice(4),
			"Cost: " + file.cost,
			"Move File...",
			"Remove File..."
			];
		if(JSON.stringify(file) != JSON.stringify(orig))
			opts.push("Save changes...");
		const title = (dircode || file.dircode) + "/" +  orig.name;
		var choice = uifc.list(WIN_SAV|WIN_ACT, title, opts, ctx);
		if(choice < 0)
			break;
		switch(choice) {
			case 0:
				dump(file, dircode);
				break;
			case 1:
				view_archive(file, dircode);
				break;
			case 2:
				var name = uifc.input(WIN_MID|WIN_SAV, "Filename", file.name, 100, K_EDIT);
				if(!name)
					break;
				file.name = name;
				break;
			case 3:
				var desc = uifc.input(WIN_MID|WIN_SAV, "Description", file.desc, LEN_FDESC, K_EDIT);
				if(!desc)
					break;
				file.desc = desc;
				break;
			case 4:
				var from = uifc.input(WIN_MID|WIN_SAV, "Uploader", file.from, LEN_ALIAS, K_EDIT);
				if(!from)
					break;
				file.from = from;
				break;
			case 5:
				var date = uifc.input(WIN_MID|WIN_SAV, "Added", new Date(file.added * 1000).toString().slice(4), 20, K_EDIT);
				if(date)
					file.added = Date.parse(date) / 1000;
				break;
			case 6:
				var cost = uifc.input(WIN_MID|WIN_SAV, "Download Cost (in credits)", String(file.cost), 15, K_NUMBER|K_EDIT);
				if(cost !== undefined)
					file.cost = cost;
				break;
			case 7:
				var dest = select_dir();
				if(!dest)
					break;
				if(dest == dircode) {
					uifc.msg("Destination directory must be different");
					break;
				}
				if(move(file, dircode, dest))
					return true;
				break;
			case 8:
				var del = confirm("Delete File Also");
				if(del < 0)
					break;
				if(remove(file, del, dircode))
					return true;
				break;
			case 9:
				if(save(file, dircode, orig.name))
					orig = JSON.parse(JSON.stringify(file));
				break;
		}
	}
	file.name = orig.name;
	return false;
}

function move(file, dircode, dest)
{
	var result = false;
	var base = new FileBase(dest);
	if(!base.open()) {
		uifc.msg("Unable to open base: " + dest);
		return false;
	}
	var dest_path = base.get_path(file);
	if(file_exists(dest_path)) {
		uifc.msg("File already exists: " + dest_path);
		base.close();
		return false;
	}
	result = base.add(file, /* use_diz: */false);
	base.close();
	if(!result) {
		uifc.msg("Add failure " + base.status + ": " + file.name);
		return false;
	}
	if(!dircode)
		dircode = file.dircode;
	base = new FileBase(dircode);
	if(!base.open()) {
		uifc.msg("Unable to open base: " + dircode);
		return false;
	}
	try {
		var src_path = base.get_path(file);
		result = base.remove(file.name);
		if(!result)
			uifc.msg("remove failure " + base.status + ": " + file.name);
		else {
			if(!file_rename(src_path, dest_path)) {
				if(!file_copy(src_path, dest_path))
					uifc.msg("Error moving/copy file: " + src_path);
				else {
					if(!file_remove(src_path))
						uifc.msg("Error removing source file: " + src_path);
				}
			}
		}
	} catch(e) {
		uifc.msg(e);
	}
	base.close();
	return result;
}

function multi(list, dircode, count)
{
	const ctx = new uifc.list.CTX;
	while(!js.terminated) {
		var opts = [
			"Move Files...",
			"Remove Files..."
			];
		var choice = uifc.list(WIN_SAV|WIN_ACT, count + " files", opts, ctx);
		if(choice < 0)
			break;
		switch(choice) {
			case 0:
				var dest = select_dir();
				if(!dest)
					break;
				for(var i = list.length - 1; i >=0; --i) {
					var file = list[i];
					if(file.tagged && move(file, dircode, dest))
						list.splice(i, /* deleteCount: */1);
				}
				break;
			case 1:
				var del = confirm("Delete File Also");
				if(del < 0)
					break;
				for(var i = list.length - 1; i >=0; --i) {
					var file = list[i];
					if(file.tagged && remove(file, del, dircode))
						list.splice(i, /* deleteCount: */1);
				}
				break;
		}
	}
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
			uifc.msg("Update failure " + base.status + ": " + filename);
	} catch(e) {
		uifc.msg(e);
	}
	base.close();
	return result;
}
