// $Id: init-tickit.js,v 1.2 2020/04/10 20:25:29 rswindell Exp $
/*
 Nigel Reed - nigel@nigelreed.net sysop@endofthelinebbs.com

 This short crappy bit of code will create a tickit.ini file for you. You won't have
 edit it by hang any more and it's easy to add a new network or new file areas.
 It will parse init-tickit.ini and automatically create /sbbs/ctrl/tickit.ini
*/

"use strict";

var init_ini = js.exec_dir + "init-tickit.ini";     
var tickit_ini = system.ctrl_dir + "tickit.ini";

var ini = [];
var f = new File(init_ini);
if(f.open("r")) {
	ini = f.iniGetAllObjects();
	f.close();
}
file_backup(tickit_ini);
var f = new File(tickit_ini);
if(!f.open("w")) {
	alert("Error " + f.error + " opening " + f.name);
	exit(1);
}
var list = {};
for(var i in ini) {
	list[ini[i].name] = ini[i];
}
for(var code in file_area.dir) {

	var area = file_area.dir[code];
	var ticline;
	f.writeln('[' + area.name + "]\nDir=" + code.toUpperCase());
	if (list[area.name]) {
		if(list[area.name].domain) {
			f.writeln('Handler=tickit/nodelist_handler.js');
			ticline = 'HandlerArg={"domain":"' + list[area.name].domain + '"';
			if (list[area.name].match) {
				ticline += ' ,"match":"' + list[area.name].match + '"';
			}
			ticline  += ', "nlmatch":"' + list[area.name].nlmatch + '"}';
			f.writeln(ticline);
		}
		if (list[area.name].forcereplace)
			f.writeln('Forcereplace = true');
	}
	f.writeln();
}
f.close();
