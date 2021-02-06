// $Id: text_sec.js,v 1.9 2020/05/08 17:27:47 rswindell Exp $

// [General] Text File Section ("G-Files")
// Replacement for Baja TEXT_FILE_SECTION and JS bbs.text_sec() functions
// Ported from src/sbbs3/text_sec.cpp

"use strict";

require("text.js", 'TextSectionLstHdr');
require("nodedefs.js", 'NODE_RTXT');
require("cga_defs.js", 'LIGHTGRAY');
require("sbbsdefs.js", 'P_CPM_EOF');

if(!bbs.mods.cnflib)
	bbs.mods.cnflib = load({}, "cnflib.js");

var file_cnf = bbs.mods.cnflib.read("file.cnf");

function txtsec_data(sec)
{
	return system.data_dir + "text/" + sec.code.toLowerCase();
}

function read_list(sec)
{
	var f = new File(txtsec_data(sec) + ".ini");
	if(!f.open("rt"))
		return [];
	var list = f.iniGetAllObjects();
	f.close();
	for(var i = 0; i < list.length; i++) {
		var fname = list[i].name;
		var path = file_getcase(txtsec_data(sec) + "/" + fname);
		if(!path)
			path = file_getcase(bbs.cmdstr(fname));
		if(path)
			list[i].path = path;
	}
	return list;
}

function write_list(sec, list)
{
	var f = new File(txtsec_data(sec) + ".ini");
	if(!f.open("wt"))
		return false;
	f.iniSetAllObjects(list);
	f.close();
	return true;
}

var usrsec = [];
for(var i in file_cnf.txtsec) {
	if(bbs.compare_ars(file_cnf.txtsec[i].ars))
		usrsec.push(file_cnf.txtsec[i]);
}
if(!usrsec.length) {
	console.print(bbs.text(NoTextSections));
	exit();
}
bbs.node_action = NODE_RTXT;
while(bbs.online) {
	if(bbs.menu_exists("text_sec"))
		bbs.menu("text_sec");
	else {
		console.print(bbs.text(TextSectionLstHdr));
		for(var i = 0; i < usrsec.length; i++) {
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
		if(bbs.menu_exists(menu))
			bbs.menu(menu);
		else {
			console.print(format(bbs.text(TextFilesLstHdr), usrsec[cursec].name));
			for(var i = 0; i < list.length; i++) {
				console.add_hotspot(i + 1);
				console.print(format(bbs.text(TextFilesLstFmt), i + 1, list[i].desc + "\r\n"));
			}
		}
		bbs.nodesync();
		var keys = "Q?";
		if(user.is_sysop) {
			keys += "ARED";
			console.mnemonics(bbs.text(WhichTextFileSysop));
		} else
			console.mnemonics(bbs.text(WhichTextFile));
		var cmd = console.getkeys(keys, list.length);
		if(cmd == 'Q' || (!cmd && !prev))
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
				var path;
				var files = directory(backslash(txtsec_data(usrsec[cursec])) + "*");
				for(var f = 0; f < files.length; f++) {
					var match = false;
					for(var j = 0; j < list.length && !match; j++) {
						if(file_getname(list[j].path) == file_getname(files[f]))
							match = true;
					}
					if(!match) {
						path = file_getname(files[f]);
						break;
					}
				}
				console.print(format(bbs.text(AddTextFilePath)
					,system.data_dir, usrsec[cursec].code.toLowerCase()));
				var path = console.getstr(path, 128, K_EDIT|K_LINE|K_TRIM);
				if(!path || console.aborted)
					break;
				if(!file_exists(path)) {
					var default_path = backslash(txtsec_data(usrsec[cursec])) + path;
					if (!file_exists(default_path)) {
						console.print(bbs.text(FileNotFound));
						break;
					} else {
						// only change the path if the file was found, otherwise
						// leave it alone so they can correct it 
						path = default_path;
					}
				}
				console.printfile(path);
				console.crlf();
				console.print(bbs.text(AddTextFileDesc));
				var desc = console.getstr(file_getname(path), 70, K_EDIT|K_LINE|K_TRIM|K_AUTODEL);
				if(!desc || console.aborted)
					break;
				list.splice(i, 0, { name: file_getname(path), desc: desc, path: path });
				write_list(usrsec[cursec], list);
				break;
			}
			case 'R':
				console.print(bbs.text(RemoveWhichTextFile));
				var i = console.getnum(list.length);
				if(i < 1)
					break;
				i--;
				var path = list[i].path;
				list.splice(i, 1);
				if(write_list(usrsec[cursec], list) == true) {
					if(file_exists(path) && !console.noyes(format(bbs.text(DeleteTextFileQ), path)))
						file_remove(path);
				}
				break;
			case 'E':
				console.print(bbs.text(EditWhichTextFile));
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
				if(!console.aborted
					&& !console.noyes("Edit " + file_getname(list[i].path)))
					console.editfile(list[i].path);
				break;
			default:
				if(!cmd && typeof(prev) == "number")
					cmd = prev + 1;
				if(typeof(cmd) == "number" && cmd <= list.length) {
					prev = cmd;
					cmd--;
					console.attributes = LIGHTGRAY;
					if(!bbs.compare_ars(list[cmd].ars)) {
						alert("Sorry, you can't read that file");
						break;
					}
					if(!list[cmd].path) {
						alert("Sorry, that file doesn't exist yet");
						break;
					}
					if(file_size(list[cmd].path) < 1) {
						alert("Sorry, that file doens't have any content yet");
						break;
					}
					var mode = P_OPENCLOSE | P_CPM_EOF;
					if(list[cmd].mode !== undefined)
						mode = eval(list[cmd].mode);
					if(list[cmd].petscii_graphics)
						console.putbyte(142);
					if(list[cmd].tail)
						console.printtail(list[cmd].path, list[cmd].tail, mode);
					else
						console.printfile(list[cmd].path, mode);
					log(LOG_INFO, "read text file: " + list[cmd].path);
					console.pause();
				}
				break;
		}
	}
}
