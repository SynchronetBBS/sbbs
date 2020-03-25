// $Id$

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
	var f = new File(txtsec_data(sec) + ".ixt");
	if(!f.open("r"))
		return [];
	var lines = f.readAll();
	f.close();
	var list = [];
	for(var i = 0; i < lines.length; i += 2) {
		var fname = lines[i];
		var path = file_getcase(txtsec_data(sec) + "/" + fname);
		if(!path)
			path = file_getcase(bbs.cmdstr(fname));
		if(path)
			list.push({ path: path, orig_path: lines[i], desc: lines[i + 1]});
	}
	return list;
}

function write_list(sec, list)
{
	var f = new File(txtsec_data(sec) + ".ixt");
	if(!f.open("w"))
		return false;
	for(var i = 0; i < list.length; i++) {
		f.writeln(list[i].orig_path || list[i].path);
		f.writeln(list[i].desc);
	}
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
		var list = read_list(usrsec[cursec]);
		var menu = "text" + (cursec + 1);
		if(bbs.menu_exists(menu))
			bbs.menu(menu);
		else {
			console.print(format(bbs.text(TextFilesLstHdr), usrsec[cursec].name));
			for(var i = 0; i < list.length; i++)
				console.print(format(bbs.text(TextFilesLstFmt), i + 1, list[i].desc + "\r\n"));
		}
		bbs.nodesync();
		var keys = "Q?";
		if(user.is_sysop) {
			keys += "ARE";
			console.mnemonics(bbs.text(WhichTextFileSysop));
		} else
			console.mnemonics(bbs.text(WhichTextFile));
		var cmd = console.getkeys(keys, list.length);
		if(cmd == 'Q')
			break;
		switch(cmd) {
			case 'A':
			{	
				var i = 0;
				if(list.length) {
					console.print(bbs.text(AddTextFileBeforeWhich));
					i = console.getnum(list.length + 1);
					if(i < 1)
						break;
					i--;
				}
				console.print(format(bbs.text(AddTextFilePath)
					,system.data_dir, usrsec[cursec].code));
				var path = console.getstr();
				if(!path)
					break;
				console.print(bbs.text(AddTextFileDesc));
				var desc = console.getstr();
				if(!desc)
					break;
				list.splice(i, 0, { path: path, desc: desc });
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
				console.editfile(list[i].path);
				break;
			default:
				if(typeof(cmd) == "number") {
					cmd--;
					console.attributes = LIGHTGRAY;
					console.printfile(list[cmd].path, P_CPM_EOF);
					log(LOG_INFO, "read text file: " + list[cmd].path);
					console.pause();
				}
		}
	}
}
