// [General] Text File Section ("G-Files")
// Replacement for Baja TEXT_FILE_SECTION and JS bbs.text_sec() functions
// Ported from src/sbbs3/text_sec.cpp

"use strict";

require("text.js", 'TextSectionLstHdr');
require("nodedefs.js", 'NODE_RTXT');
require("cga_defs.js", 'LIGHTGRAY');
require("sbbsdefs.js", 'P_CPM_EOF');
require("select_item.js", "select_item");

// modopts.ini [text_sec] options:
// - selected_item_prefix, unselected_item_prefix: what marks the default item
//   of a list (and what takes its place before the other items)
// - selected_item_attr: attributes of the default item's highlighted name
// - more_fmt: the indicator shown when a list doesn't fit on the screen
// Without any of the first three, no default item is marked and the
// navigation keys only scroll a list that is longer than the screen.
var options = load("modopts.js", "text_sec", {});
var marked = options.selected_item_prefix !== undefined
	|| options.unselected_item_prefix !== undefined
	|| Boolean(options.selected_item_attr);

var file_cnf = new File(system.ctrl_dir + "file.ini");
if(!file_cnf.open('r')) {
	alert("Error opening " + file_cnf.name);
	exit(0);
}

function txtsec_data(sec)
{
	return system.data_dir + "text/" + sec.code.toLowerCase();
}

function get_fpath(sec, fname)
{
	var path = file_getcase(txtsec_data(sec) + "/" + fname);
	if(!path)
		path = file_getcase(bbs.cmdstr(fname));
	if(!path)
		path = txtsec_data(sec) + "/" + file_getname(fname);
	return path;
}

function read_list(sec)
{
	var f = new File(txtsec_data(sec) + ".ini");
	if(!f.open("r"))
		return [];
	var list = f.iniGetAllObjects();
	f.close();
	return list;
}

function read_mode(sec)
{
	var dflt = "P_CPM_EOF";
	var f = new File(txtsec_data(sec) + ".ini");
	if(!f.open("r"))
		return dflt;
	var mode = f.iniGetValue(null, "mode", dflt);
	f.close();
	return mode;
}

function read_cols(sec)
{
	var dflt = undefined;
	var f = new File(txtsec_data(sec) + ".ini");
	if(!f.open("r"))
		return dflt;
	var cols = f.iniGetValue(null, "cols", dflt);
	f.close();
	return cols;
}

function write_list(sec, list)
{
	var f = new File(txtsec_data(sec) + ".ini");
	if(!f.open(f.exists ? 'r+':'w+'))
		return false;
	f.iniRemoveSections();
	f.iniSetAllObjects(list);
	f.close();
	return true;
}

function item_prefix(is_default)
{
	var prefix = is_default ? options.selected_item_prefix : options.unselected_item_prefix;
	return prefix === undefined ? "" : prefix;
}

function prefix_width()
{
	return Math.max(strip_ctrl(item_prefix(true)).length, strip_ctrl(item_prefix(false)).length);
}

// The widest name (or description) in a list, as far as fits on a line
// after the item's prefix and number
function widest_name(list, prop)
{
	var max = Math.max(console.screen_columns - prefix_width() - 6, 1);
	var widest = 0;
	for(var i = 0; i < list.length; i++)
		widest = Math.max(widest, Math.min(String(list[i][prop]).length, max));
	return widest;
}

// The default item's name is padded to the list's widest and highlighted,
// but only when the text.dat format displays it with a plain %s, so that a
// customized format is never misaligned
function item_name(fmt, name, widest, is_default)
{
	if(!is_default || !options.selected_item_attr || fmt.indexOf("%s") < 0)
		return name;
	return options.selected_item_attr + format("%-*s", widest, String(name).substr(0, widest)) + "\x01n";
}

// Enter completes a clicked number that could be the start of a longer one
function add_hotspot(num, count)
{
	console.add_hotspot(num * 10 <= count ? num + "\r" : num);
}

function item_str(fmt, num, name, widest, is_default)
{
	// select_item() ends the line
	return format(fmt, num, item_name(fmt, name, widest, is_default)).replace(/[\r\n]+$/, "");
}

function show_menu(name)
{
	if(!bbs.menu_exists(name))
		return false;
	bbs.menu(name);
	return true;
}

var usrsec = [];
var txtsec = file_cnf.iniGetAllObjects("code", "text:");
file_cnf.close();
for(var i in txtsec) {
	if(bbs.compare_ars(txtsec[i].ars))
		usrsec.push(txtsec[i]);
}
if(!usrsec.length) {
	console.print(bbs.text(NoTextSections));
	exit();
}
bbs.node_action = NODE_RTXT;
var cursec = 0;
var sec_widest = widest_name(usrsec, "name");
while(bbs.online) {
	console.aborted = false;
	bbs.nodesync();
	// A menu file displayed in place of the list can't mark a default item
	var sec_marked = marked && !bbs.menu_exists("text_sec");
	cursec = select_item({
		count: usrsec.length,
		current: sec_marked ? cursec : -1,
		no_default: !sec_marked,
		header: bbs.text(TextSectionLstHdr),
		prompt: function() { return bbs.text(WhichTextSection); },
		print_item: function(i, is_default, redraw) {
			console.print(item_prefix(is_default));
			if(i<9) console.print(' ');
			if(!redraw)
				add_hotspot(i + 1, usrsec.length);
			console.print(item_str(bbs.text(TextSectionLstFmt), i + 1, usrsec[i].name, sec_widest, is_default));
		},
		search: function(i) { return usrsec[i].name; },
		menu: function() { return show_menu("text_sec"); },
		more_fmt: options.more_fmt,
		more_indent: prefix_width() + 1
	});
	if(cursec < 0)	// Quit
		break;
	var curfile = 0;
	while(bbs.online) {
		var prev;
		var list = read_list(usrsec[cursec]);
		var menu = "text" + (cursec + 1);
		var cmd;
		console.aborted = false;
		var keys = user.is_sysop ? "?ARED" : "?";
		console.cmd_keys = keys;
		bbs.nodesync();
		if(list.length < 1) {	// nothing to select from
			if(!show_menu(menu))
				console.print(format(bbs.text(TextFilesLstHdr), usrsec[cursec].name));
			console.mnemonics(bbs.text(user.is_sysop ? WhichTextFileSysop : WhichTextFile));
			cmd = console.getkeys(console.quit_key + keys, 0);
		} else {
			var file_marked = marked && !bbs.menu_exists(menu);
			var file_widest = widest_name(list, "desc");
			var sel = {
				count: list.length,
				current: file_marked ? Math.min(curfile, list.length - 1) : -1,
				no_default: !file_marked,
				keys: keys,
				header: format(bbs.text(TextFilesLstHdr), usrsec[cursec].name),
				prompt: function() { return bbs.text(user.is_sysop ? WhichTextFileSysop : WhichTextFile); },
				print_item: function(i, is_default, redraw) {
					console.print(item_prefix(is_default));
					if(!redraw)
						add_hotspot(i + 1, list.length);
					console.print(item_str(bbs.text(TextFilesLstFmt), i + 1, list[i].desc, file_widest, is_default));
				},
				search: function(i) { return [list[i].desc, file_getname(list[i].name)]; },
				menu: function() { return show_menu(menu); },
				more_fmt: options.more_fmt,
				more_indent: prefix_width() + 1
			};
			cmd = select_item(sel);
			if(cmd === -1)	// Quit
				break;
			if(cmd === -2)	// Enter, with no default item
				cmd = "";
			else if(typeof cmd == "number")
				cmd++;
			else
				curfile = sel.current;
		}
		if(cmd == console.quit_key || (!cmd && !prev))
			break;
		switch(cmd) {
			case 'D':
				print(lfexpand(JSON.stringify(list, null, 4)));
				break;
			case 'A':
			{	
				var i = 0;
				if(list.length) {
					console.print(bbs.text(AddTextFileBeforeWhich));
					i = console.getnum(list.length + 1, list.length + 2);
					if(i < 1)
						break;
					i--;
				}
				var fname = '';
				var files = directory(backslash(txtsec_data(usrsec[cursec])) + "*");
				for(var f = 0; f < files.length; f++) {
					var match = false;
					for(var j = 0; j < list.length && !match; j++) {
						if(file_getname(list[j].name) == file_getname(files[f]))
							match = true;
					}
					if(!match) {
						fname = file_getname(files[f]);
						break;
					}
				}
				console.print(format(bbs.text(AddTextFilePath)
					,system.data_dir, usrsec[cursec].code.toLowerCase()));
				fname = console.getstr(fname, 128, K_EDIT|K_LINE|K_TRIM);
				if(!fname || console.aborted)
					break;
				var path = fname;
				if(!file_exists(path))
					path = backslash(txtsec_data(usrsec[cursec])) + fname;
				if(!file_exists(path)) {
					console.print(format(bbs.text(FileDoesNotExist), path));
					if(confirm("Create it"))
						if(!(console.editfile(path)))
							break;
				}
				console.printfile(path, eval(read_mode(usrsec[cursec])), read_cols(usrsec[cursec]));
				console.crlf();
				console.print(bbs.text(AddTextFileDesc));
				var desc = console.getstr(file_getname(path), 70, K_EDIT|K_LINE|K_TRIM|K_AUTODEL);
				if(!desc || console.aborted)
					break;
				list.splice(i, 0, { name: path, desc: desc });
				write_list(usrsec[cursec], list);
				break;
			}
			case 'R':
				console.putmsg(bbs.text(RemoveWhichTextFile));
				var i = console.getnum(list.length);
				if(i < 1)
					break;
				i--;
				var fpath = get_fpath(usrsec[cursec], list[i].name);
				list.splice(i, 1);
				if(write_list(usrsec[cursec], list) == true) {
					if(file_exists(fpath) && !console.noyes(format(bbs.text(DeleteTextFileQ), fpath)))
						file_remove(fpath);
				}
				break;
			case 'E':
				console.putmsg(bbs.text(EditWhichTextFile));
				var i = console.getnum(list.length);
				if(i < 1)
					break;
				i--;
				console.print("Desc: ");
				{
					var str = console.getstr(list[i].desc, 75, K_EDIT|K_LINE|K_AUTODEL|K_TRIM);
					if(str && !console.aborted) {
						list[i].desc = str;
						write_list(usrsec[cursec], list);
					}
				}
				var fpath = get_fpath(usrsec[cursec], list[i].name);
				if(!console.aborted
					&& !console.noyes("Edit " + fpath))
					console.editfile(fpath);
				break;
			default:
				if(!cmd && typeof(prev) == "number")
					cmd = prev + 1;
				if(typeof(cmd) == "number" && cmd <= list.length) {
					prev = cmd;
					curfile = cmd;	// the next file is the new default
					cmd--;
					console.attributes = LIGHTGRAY;
					if(list[cmd].ars !== undefined && !bbs.compare_ars(list[cmd].ars)) {
						alert("Sorry, you can't read that file");
						break;
					}
					var mode = list[cmd].mode || read_mode(usrsec[cursec]);
					var cols = list[cmd].cols || read_cols(usrsec[cursec]);
					if(list[cmd].petscii_graphics)
						console.putbyte(142);
					if(console.term_supports(USER_RIP))
						console.write("\x02|*\r\n");
					var fpath = get_fpath(usrsec[cursec], list[cmd].name);
					if(list[cmd].tail)
						console.printtail(fpath, list[cmd].tail, eval(mode), cols);
					else
						console.printfile(fpath, eval(mode), cols);
					log(LOG_INFO, "read text file: " + fpath);
					console.pause();
					if(console.term_supports(USER_RIP))
						console.write("\x02|*\r\n");
				}
				break;
		}
	}
	console.cmd_keys = "";
}
