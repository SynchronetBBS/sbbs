// Synchronet File Manager

// run using jsexec

require("file_size.js", "file_size_float");
require("uifcdefs.js", "WIN_ORG");
require("sbbsdefs.js", "K_EDIT");

var filelist = load({}, "filelist_lib.js");

"use strict";

if(!uifc.init("Synchronet File Manager", argv[0])) {
	alert("uifc init failure");
	exit(-1);
}
js.on_exit("uifc.bail()");

// TODO: make these configurable
var uploader = system.operator;
var use_diz = false;
var desc_offset;
const excludes = filelist.filenames.concat([
	"FILES.TXT",
	"FILE_ID.DIZ",
	"00_INDEX.HTM",
	"WILDCAT.TXT",
	"MEDIA.LST",
	"AREADESC"
]);

const main_ctx = new uifc.list.CTX;
while(!js.terminated) {
	const items = [
		"Browse",
		"Search Filenames",
		"Search Meta Data",
		"Search Uploader",
		"Search Pending Files",
		"Search Offline Files"
	];
	try {
		uifc.help_text =
			"`Synchronet File Manager`\n" +
			"\n" +
			"This utility provides an alternate method for system oeprators to add\n" +
			"and manage files in their Synchronet filebases.\n" +
			"\n" +
			"The operations available are:\n" +
			"\n" +
			"~ Browse ~\n" +
			" Explore your file directories and add, edit, move, or remove files.\n" +
			"\n" +
			"~ Search Filenames ~\n" +
			" Find files by name (including wildcards) in any and all directories.\n" +
			"\n" +
			"~ Search Meta Data ~\n" +
			" Find text in any file meta data (e.g. description).\n" +
			"\n" +
			"~ Search Uploader ~\n" +
			" Find files imported/uploaded by a specific user name.\n" +
			"\n" +
			"~ Search Pending Files ~\n" +
			" Find files in storage directories that have not been indexed in bases.\n" +
			"\n" +
			"~ Search Offline Files ~\n" +
			" Find files that are indexed in filebases, but don't physically exist.\n" +
			"";
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
				search(undefined, find_pending, "Pending", list_pending_files);
				break;
			case 5:
				search(undefined, find_offline, "Offline");
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

function search(prompt, func, title, list_func)
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
	if(!list_func)
		list_func = list_files;
	if(found.length)
		list_func(found.length + " " + (title || pattern) + " Files Found", found);
	else
		uifc.msg("No " + (title || "") + " Files Found");
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

function find_pending(dircode)
{
	var base = new FileBase(dircode);
	if(!base.open()) {
		uifc.msg("Unable to open base: " + dircode);
		return;
	}
	var list = base.get_list();
	var found = find_new_files(list, dircode);
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
		if(!file_exists(base.get_path(file.name))) {
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

function find_file(fname, list)
{
	for(var i = 0; i < list.length; ++i)
		if(list[i].name.toLowerCase() == fname.toLowerCase())
			return i;
	return -1;
}

function get_file_details(dircode, filename)
{
	var base = new FileBase(dircode);
	if(!base.open()) {
		uifc.msg("Unable to open base: " + dircode);
		return null;
	}

	var file = base.get(filename, FileBase.DETAIL.EXTENDED);
	base.close();
	return file;
}

function list_files(title, list, dircode)
{
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
		for(var i in list) {
			var file = list[i];
			items.push(format("%-*s%-*s  %s"
				,tagged ? 2:0
				,file.tagged ? ascii(251):""
				,namelen, wide_screen ? file.name : FileBase.format_name(file.name)
				,file.desc || ""));
		}
		var win_mode = WIN_SAV | WIN_RHT | WIN_ACT | WIN_DEL | WIN_DELACT | WIN_TAG;
		if(dircode)
			win_mode |= WIN_INS | WIN_INSACT;
		if(!tagged)
			win_mode |= WIN_EDIT;
		var result = uifc.list(win_mode, title, items, ctx);
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
				case MSK_INS:
					add_files(list, dircode);
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
							if(file.tagged === true) {
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
				case MSK_EDIT:
					list[result] = get_file_details(dircode, file.name);
					file = list[result];
					if(!file) {
						uifc.msg("Error getting file details");
						break;
					}
					var orig_name = file.name;
					if(edit_filename(file))
						save(file, dircode, orig_name);
					break;
			}
			continue;
		}
		if(tagged > 0) {
			multi(list, dircode, tagged);
		} else { // Single-file selected
			if(edit(list[result], dircode)) // file (re)moved?
				list.splice(result, /* deleteCount: */1);
		}
		--uifc.save_num;
	}
}

function getvpath(dircode)
{
	return file_area.dir[dircode].vpath || (dircode + "/");
}

function list_pending_files(title, list)
{
	const ctx = new uifc.list.CTX;
	while(!js.terminated) {
		var items = [];
		var namelen = 12;
		var tagged = 0;
		for(var i in list) {
			if(list[i].tagged)
				++tagged;
		}
		for(var i in list) {
			var file = list[i];
			items.push(format("%-*s%s%s"
				,tagged ? 2:0
				,file.tagged ? ascii(251):""
				,getvpath(file.dircode)
				,file.name));
		}
		var win_mode = WIN_SAV | WIN_RHT | WIN_ACT | WIN_TAG | WIN_DEL | WIN_DELACT;
		title = list.length + " Pending (non-imported) Files";
		var result = uifc.list(win_mode, title, items, ctx);
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
					if(tagged > 0) {
						if(deny("Delete " + tagged + " files"))
							continue;
						for(var i = list.length - 1; i >= 0; --i) {
							var file = list[i];
							if(file.tagged === true) {
								var path = file_area.dir[file.dircode].path + file.name;
								if(file_remove(path))
									list.splice(i, /* deleteCount: */1);
								else
									uifc.msg("Error deleting " + path);
							}
						}
					} else {
						var path = file_area.dir[file.dircode].path + file.name;
						if(deny("Delete " + path))
							continue;
						if(file_remove(path))
							list.splice(result, /* deleteCount: */1);
						else
							uifc.msg("Error deleting " + path);
					}
					break;
			}
			continue;
		}
		if(tagged > 0) {
			for(var i = list.length - 1; i >=0; --i) {
				var file = list[i];
				if(file.tagged && add_file(file.name, file.dircode))
					list.splice(i, /* deleteCount: */1);
			}
		} else {  // Single-file selected
			var file = list[result];
			if(add_file(file.name, file.dircode))
				list.splice(result, /* deleteCount: */1);
		}
	}
}

function find_new_files(list, dircode)
{
	var new_files = find_new_filenames(list, dircode, excludes);
	for(var i in new_files)
		new_files[i] = { name: new_files[i], dircode: dircode };
	return new_files;
}

function find_new_filenames(list, dircode, excludes)
{
	var all_files = directory(file_area.dir[dircode].path + "*");
	var new_files = [];
	for(var i in all_files) {
		if(!file_isdir(all_files[i])) {
			var fname = file_getname(all_files[i]);
			if(find_file(fname, list) < 0 && (!excludes || excludes.indexOf(fname.toUpperCase()) < 0))
				new_files.push(fname);
		}
	}
	return new_files;
}

function is_filelist(filename)
{
	return filelist.filenames.indexOf(filename.toUpperCase()) >= 0;
}

function add_files(list, dircode)
{
	new_files = find_new_filenames(list, dircode);
	if(new_files.length < 1) {
		uifc.msg("No new files to add in " + file_area.dir[dircode].path);
		return;
	}
	const ctx = new uifc.list.CTX;
	var tagged = [];
	while(!js.terminated && new_files.length > 0) {
		const new_title = new_files.length + " New Files in " + file_area.dir[dircode].path;
		var opts = [];
		for(var i in new_files) {
			var filename = new_files[i];
			opts[i] = format("%-*s%s%s"
				,tagged.length ? 2:0, tagged.indexOf(filename) >= 0 ? ascii(251):"", filename
				,is_filelist(filename) ? " <- File List (likely suitable for import)" : "");
		}
		var choice = uifc.list(WIN_SAV|WIN_ACT|WIN_TAG, new_title, opts, ctx);
		if(choice < 0)
			break;
		if(choice == MSK_TAGALL) {
			if(tagged.length)
				tagged = [];
			else
				for(var i in new_files)
					tagged[i] = new_files[i];
			continue;
		}
		if((choice & MSK_ON) == MSK_TAG) {
			choice &= MSK_OFF;
			var i = tagged.indexOf(new_files[choice]);
			if(i >= 0)
				tagged.splice(i, /* deleteCount: */1);
			else
				tagged.push(new_files[choice]);
			++ctx.cur;
			++ctx.bar;
			continue;
		}
		var files = tagged;
		if(!files.length)
			files = [new_files[choice]];
		for(var i = files.length -1; i >= 0; --i) {
			var filename = files[i];
			if(is_filelist(filename)) {
				import_filelist(filename, dircode);
				continue;
			}
			var new_file = add_file(filename, dircode);
			if(!new_file)
				break;
			list.push(new_file);
			new_files.splice(i, /* deleteCount: */1);
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
	var result = file_exists(base.get_path(file.name));
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
	var path = base.get_path(file.name);
	if(!del || file_exists(path))
		result = base.remove(file.name, del);
	else
		uifc.msg(path + " does not exist");
	base.close();
	return result;
}

function view_details(file, dircode)
{
	if(!dircode)
		dircode = file.dircode;
	var base = new FileBase(dircode);
	if(!base.open()) {
		uifc.msg("Unable to open base: " + dircode);
		return;
	}
	var dir = file_area.dir[dircode];
	var buf = [];
	buf.push("Library          " + file_area.lib[dir.lib_name].description);
	buf.push("Directory        " + dir.description);
	buf.push("Size             " + file_size_float(base.get_size(file.name), 1, 1));
	buf.push("Time             " + system.timestr(base.get_time(file.name)));
	uifc.showbuf(WIN_MID|WIN_HLP, base.get_path(file.name), buf.concat(base.dump(file.name)).join('\n'));
	base.close();
}

function view_archive(file, dircode)
{
	var path = file_area.dir[dircode || file.dircode].path + file.name;
	try {
		var list = new Archive(path).list();
		var buf = [];
		var namelen = 0;
		for(var i in list)
			if(list[i].name.length > namelen)
				namelen = list[i].name.length;
		for(var i in list)
			buf.push(format("%-*s  %10u  %s"
				,namelen, list[i].name, list[i].size
				,system.timestr(list[i].time).slice(4)));
		uifc.showbuf(WIN_MID, path, buf.join('\n'));
	} catch(e) {
		uifc.msg(e);
	}
}

function viewable_text_file(filename)
{
	switch(filename.toLowerCase()) {
		case 'read.me':
		case 'readme':
		case 'readme.now':
		case 'compiling':
			return true;
	}
	var ext = file_getext(filename);
	if(!ext)
		return false;
	switch(ext.toLowerCase()) {
		case '.asc':
		case '.txt':
		case '.diz':
		case '.nfo':
		case '.doc':
		case '.c':
		case '.cc':
		case '.cpp':
		case '.h':
		case '.hh':
		case '.hpp':
		case '.js':
		case '.json':
		case '.htm':
		case '.html':
		case '.css':
		case '.ini':
		case '.bbs':
			return true;
	}
	return false;
}

function view_text_file(file, dircode, col_counter)
{
	var path = file_area.dir[dircode || file.dircode].path + file.name;
	var f = new File(path);
	if(f.open("r")) {
		var txt = f.readAll();
		if(txt) {
			if(col_counter) {
				txt.unshift("0123456789012345678901234567890123456789012345678901234567890123456789012345678");
				txt.unshift("          1         2         3         4         5         6         7");
			}
			uifc.showbuf(WIN_MID|WIN_SAV, path, txt.join('\n'));
		}
		f.close();
	}
}

function view_contents(file, dircode, col_counter)
{
	if(viewable_text_file(file.name))
		view_text_file(file, dircode, col_counter);
	else
		view_archive(file, dircode);
}

function confirm(prompt)
{
	var choice = uifc.list(WIN_MID|WIN_SAV, prompt, [ "Yes", "No" ]);
	if(choice >= 0)
		choice = choice == 0;
	return choice;
}

function deny(prompt)
{
	return uifc.list(WIN_MID|WIN_SAV, prompt, [ "No", "Yes" ]) != 1;
}

function edit_filename(file)
{
	var name = uifc.input(WIN_MID|WIN_SAV, "Filename", file.name, 100, K_EDIT);
	if(!name)
		return false;
	name = file_getname(name);
	if(name == file.name)
		return false;
	file.name = name;
	return true;
}

function edit_desc(file)
{
	var desc = uifc.input(WIN_MID|WIN_SAV, "Description", file.desc || "", LEN_FDESC, K_EDIT);
	if(desc !== undefined)
		file.desc = desc;
}

function edit_uploader(file)
{
	var from = uifc.input(WIN_MID|WIN_SAV, "Uploader", file.from, LEN_ALIAS, K_EDIT);
	if(from !== undefined)
		file.from = from;
	return file.from;
}

function edit_cost(file)
{
	var cost = uifc.input(WIN_MID|WIN_SAV, "Download Cost (in credits)", String(file.cost), 15, K_NUMBER|K_EDIT);
	if(cost !== undefined)
		file.cost = cost;
}

function edit(file, dircode)
{
	if(!dircode)
		dircode = file.dircode;
	file.extdesc = get_extdesc(file, dircode);
	const ctx = new uifc.list.CTX;
	var orig = JSON.parse(JSON.stringify(file));
	while(!js.terminated) {
		var opts = [
			"Move File...",
			"Remove File...",
			"View Details...",
			"View Contents...",
			"Filename: " + file.name,
			"Description: " + (file.desc || ""),
			"Uploader: " + (file.from || ""),
			"Added: " + system.timestr(file.added).slice(4),
			"Cost: " + file.cost,
			"|---------- Extended Description -----------|"
			];
		if(file.extdesc) {
			var extdesc = strip_ctrl_a(file.extdesc).split('\n');
			opts = opts.concat(extdesc);
		}
		const title = getvpath(dircode) +  orig.name;
		var choice = uifc.list(WIN_SAV|WIN_ACT|WIN_ESC|WIN_XTR|WIN_INS|WIN_DEL, title, opts, ctx);
		if(choice == -1) {
			if(JSON.stringify(file) != JSON.stringify(orig)) {
				var choice = confirm("Save Changes?");
				if(choice < 0)
					continue;
				if(choice && save(file, dircode, orig.name))
					return false;
			}
			break;
		}
		var mask = choice & MSK_ON;
		choice &= MSK_OFF;
		if(mask && choice < 10)
			continue;
		switch(choice) {
			case 0:
				var dest = select_dir();
				if(!dest)
					break;
				if(dest == dircode) {
					uifc.msg("Destination directory must be different");
					break;
				}				if(move(file, dircode, dest))
					return true;
				break;
			case 1:
				var del = confirm("Delete File Also");
				if(del < 0)
					break;
				if(remove(file, del, dircode))
					return true;
				break;
			case 2:
				view_details(file, dircode);
				break;
			case 3:
				view_contents(file, dircode);
				break;
			case 4:
				edit_filename(file);
				break;
			case 5:
				edit_desc(file);
				break;
			case 6:
				edit_uploader(file);
				break;
			case 7:
				var date = uifc.input(WIN_MID|WIN_SAV, "Added", new Date(file.added * 1000).toString().slice(4), 20, K_EDIT);
				if(date)
					file.added = Date.parse(date) / 1000;
				break;
			case 8:
				edit_cost(file);
				break;
			case 9:
				break;
			default: // Extended description
				if(file.extdesc)
					file.extdesc = file.extdesc.replace(/\r/g, '');
				var extdesc = file.extdesc ? file.extdesc.split('\n') : [];
				var index = choice - 10;
				if(mask & MSK_DEL) {
					extdesc.splice(index, /* deleteCount: */1);
				}
				else if(mask & MSK_INS) {
					var str = uifc.input(WIN_SAV, 1, ctx.bar + 2, "", 79, K_MSG);
					if(str !== undefined)
						extdesc.splice(index, /* deleteCount: */0, str);
				}
				else if(extdesc.length <= index) {
					var str = uifc.input(WIN_SAV, 1, ctx.bar + 2, "", 79, K_MSG);
					if(str !== undefined)
						extdesc.push(str);
				}
				else {
					var str = uifc.input(WIN_SAV, 1, ctx.bar + 2, "", extdesc[index], 79, K_EDIT|K_MSG);
					if(str !== undefined)
						extdesc[index] = str;
				}
				file.extdesc = extdesc.join('\r\n');
				break;
		}
	}
	for(var i in file)
		file[i] = orig[i];

	return false;
}

function get_extdesc(file, dircode)
{
	if(file.extdesc)
		return file.extdesc;
	if(!dircode)
		dircode = file.dircode;
	var base = new FileBase(dircode);
	if(!base.open()) {
		uifc.msg("Unable to open base: " + dircode);
		return false;
	}
	var f = base.get(file.name, FileBase.DETAIL.EXTENDED);
	if(f) file = f;
	base.close();
	return file.extdesc;
}

function move(file, dircode, dest)
{
	var dest_path = file_area.dir[dest].path + file.name;
	if(file_exists(dest_path)) {
		uifc.msg("File already exists: " + dest_path);
		return false;
	}
	var result = false;
	var base = new FileBase(dest);
	if(!base.open()) {
		uifc.msg("Unable to open base: " + dest);
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
		var src_path = base.get_path(file.name);
		if(!src_path) {
			uifc.msg(format("Error %d (%s) getting source path for: %s"
				,base.status, base.error, file.name));
			base.close();
			return false;
		}
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

function add_file(filename, dircode)
{
	if(system.illegal_filename(filename)) {
		uifc.msg("Illegal filename: " + filename);
		return false;
	}
	if(!system.allowed_filename(filename)) {
		uifc.msg("Disallowed filename: " + filename);
		return false;
	}
	var base = new FileBase(dircode);
	if(!base.open()) {
		uifc.msg("Unable to open base: " + dircode);
		return false;
	}
	var result = false;
	var file = {
		name: filename,
		from: uploader,
		cost: file_size(file_area.dir[dircode].path + filename)
		};
	const ctx = new uifc.list.CTX;
	const title = "Adding " + filename + " to " + getvpath(dircode);
	while(!js.terminated) {
		var opts = [
			"Add File...",
			"View Contents...",
			"Description: " + (file.desc || ""),
			"Uploader: " + (file.from || ""),
			"Extract/Import DIZ: " + ((file_area.dir[dircode].settings & DIR_DIZ) ? (use_diz ? "Yes" : "No") : "N/A"),
			];
		var choice = uifc.list(WIN_MID|WIN_SAV|WIN_ACT, title, opts, ctx);
		if(choice < 0)
			break;
		switch(choice) {
			case 1:
				view_contents(file, dircode);
				continue;
			case 2:
				edit_desc(file);
				continue;
			case 3:
				uploader = edit_uploader(file);
				continue;
			case 4:
				if(file_area.dir[dircode].settings & DIR_DIZ)
					use_diz = !use_diz;
				continue;
		}
		uifc.pop(title);
		result = base.add(file, use_diz);
		uifc.pop();
		if(result) {
			file = base.get(filename);
		} else
			uifc.msg(format("Error %d (%s) adding file", base.status, base.error));
		--uifc.save_num;
		break;
	}
	base.close();
	return result ? file : false;
}

function import_filelist(filename, dircode)
{
	var filepath = file_area.dir[dircode].path + filename;
	var base = new FileBase(dircode);
	if(!base.open()) {
		uifc.msg("Unable to open base: " + dircode);
		return false;
	}
	var file = {
		name: filename,
		from: uploader
	};
	const ctx = new uifc.list.CTX;
	const title = "Importing " + filename + " to " + getvpath(dircode);
	while(!js.terminated) {
		var opts = [
			"Parse List File...",
			"View File...",
			"Uploader: " + uploader,
			"Extract/Import DIZ: " + ((file_area.dir[dircode].settings & DIR_DIZ) ? (use_diz ? "Yes" : "No") : "N/A"),
			"Description Offset: " + desc_offset
			];
		var choice = uifc.list(WIN_MID|WIN_SAV|WIN_ACT, title, opts, ctx);
		if(choice < 0)
			break;
		switch(choice) {
			case 1:
				view_contents(file, dircode, /* column counter */true);
				continue;
			case 2:
				uploader = edit_uploader(file);
				continue;
			case 3:
				if(file_area.dir[dircode].settings & DIR_DIZ)
					use_diz = !use_diz;
				continue;
			case 4:
				var desc_off = uifc.input(WIN_SAV|WIN_MID, "Description character offset",
					desc_offset || "", 3, K_EDIT | K_NUMBER);
				if(desc_off === undefined)
					continue;
				desc_offset = desc_off || undefined;
				continue;
		}
		--uifc.save_num;
		var f = new File(filepath);
		if(!f.open("r")) {
			uifc.msg("Error " + f.error + " opening " + f.name);
			break;
		}
		var lines = f.readAll();
		f.close();
		var list = filelist.parse(lines, desc_offset);
		for(var i in list)
			list[i].from = uploader;
		list_parsed_filelist(filepath, list, dircode);
		break;
	}
	base.close();
	return result ? file : false;
}

function list_parsed_filelist(listfile, list, dircode)
{
	const title = "Parsed " + listfile;
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
		for(var i in list) {
			var file = list[i];
			items.push(format("%-*s%-*s  %s"
				,tagged ? 2:0
				,file.tagged ? ascii(251):""
				,namelen, wide_screen ? file.name : FileBase.format_name(file.name)
				,file.desc || ""));
		}
		var win_mode = WIN_SAV | WIN_BOT | WIN_DEL | WIN_DELACT | WIN_TAG | WIN_ACT;
		if(!tagged)
			win_mode |= WIN_EDIT;
		var result = uifc.list(win_mode, title, items, ctx);
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
					if(tagged > 0) {
						for(var i = list.length - 1; i >= 0; --i) {
							file = list[i];
							if(file.tagged === true)
								list.splice(i, /* deleteCount: */1);
						}
					} else {
						list.splice(result, /* deleteCount: */1);
					}
					break;
				case MSK_EDIT:
					edit_desc(file);
					break;
			}
			continue;
		}
		edit_parsed_file(list, result, dircode);
		--uifc.save_num;
	}
}

function edit_parsed_file(list, index, dircode)
{
	var file = list[index];
	const ctx = new uifc.list.CTX;
	while(!js.terminated) {
		var opts = [
			"Import Parsed File List...",
			"View Contents...",
			"Description: " + (file.desc || ""),
			"Uploader: " + (file.from || ""),
			"|---------- Extended Description -----------|"
			];
		if(file.extdesc) {
			var extdesc = strip_ctrl_a(file.extdesc).split('\n');
			opts = opts.concat(extdesc);
		}
		const title = getvpath(dircode) +  file.name;
		var choice = uifc.list(WIN_SAV|WIN_ACT|WIN_DEL|WIN_INS, title, opts, ctx);
		if(choice == -1) {
			break;
		}
		var mask = choice & MSK_ON;
		choice &= MSK_OFF;
		if(mask && choice < 10)
			continue;
		switch(choice) {
			case 0:
				break;
			case 1:
				view_contents(file, dircode);
				break;
			case 2:
				edit_desc(file);
				break;
			case 3:
				edit_uploader(file);
				break;
			case 4:
				break;
			default: // Extended description
			{
				if(file.extdesc)
					file.extdesc = file.extdesc.replace(/\r/g, '');
				var extdesc = file.extdesc ? file.extdesc.split('\n') : [];
				var index = choice - 5;
				if(mask & MSK_DEL) {
					extdesc.splice(index, /* deleteCount: */1);
				}
				else if(mask & MSK_INS) {
					var str = uifc.input(WIN_SAV, 1, ctx.bar + 2, "", 79, K_MSG);
					if(str !== undefined)
						extdesc.splice(index, /* deleteCount: */0, str);
				}
				else if(extdesc.length <= index) {
					var str = uifc.input(WIN_SAV, 1, ctx.bar + 2, "", 79, K_MSG);
					if(str !== undefined)
						extdesc.push(str);
				}
				else {
					var str = uifc.input(WIN_SAV, 1, ctx.bar + 2, "", extdesc[index], 79, K_EDIT|K_MSG);
					if(str !== undefined)
						extdesc[index] = str;
				}
				file.extdesc = extdesc.join('\r\n');
				break;
			}
		}
	}
	return false;
}

