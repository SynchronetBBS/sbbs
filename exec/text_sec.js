// [General] Text File Section ("G-Files")
// Replacement for Baja TEXT_FILE_SECTION and JS bbs.text_sec() functions
// Ported from src/sbbs3/text_sec.cpp

"use strict";

require("text.js", 'TextSectionLstHdr');
require("nodedefs.js", 'NODE_RTXT');
require("cga_defs.js", 'LIGHTGRAY');
require("sbbsdefs.js", 'P_CPM_EOF');

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
while(bbs.online) {
	console.aborted = false;
	if(bbs.menu_exists("text_sec"))
		bbs.menu("text_sec");
	else {
		console.print(bbs.text(TextSectionLstHdr));
		for(var i = 0; i < usrsec.length && !console.aborted; i++) {
			if(i<9) console.print(' ');
			console.add_hotspot(i + 1);
			console.print(format(bbs.text(TextSectionLstFmt), i + 1, usrsec[i].name));
		}
	}
	bbs.nodesync();
	console.mnemonics(bbs.text(WhichTextSection));
	var cursec = console.getnum(usrsec.length);
	if(cursec < 1)	// Quit
		break;
	cursec--;
	while(bbs.online) {
		var prev;
		var list = read_list(usrsec[cursec]);
		var menu = "text" + (cursec + 1);
		console.aborted = false;
		if(bbs.menu_exists(menu))
			bbs.menu(menu);
		else {
			console.print(format(bbs.text(TextFilesLstHdr), usrsec[cursec].name));
			for(var i = 0; i < list.length && !console.aborted; i++) {
				console.add_hotspot(i + 1);
				console.print(format(bbs.text(TextFilesLstFmt), i + 1, list[i].desc + "\r\n"));
			}
		}
		bbs.nodesync();
		var keys = console.quit_key + "?";
		if(user.is_sysop) {
			keys += "ARED";
			console.mnemonics(bbs.text(WhichTextFileSysop));
		} else
			console.mnemonics(bbs.text(WhichTextFile));
		var cmd = console.getkeys(keys, list.length);
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
}
