// $Id$
/*
 Nigel Reed - nigel@nigelreed.net sysop@endofthelinebbs.com

 This short crappy bit of code will create a tickit.ini file for you. You won't have
 edit it by hang any more and it's easy to add a new network or new file areas.
 It will parse init-tickit.ini and automatically create /sbbs/ctrl/tickit.ini
*/

"use strict";

var init_ini = js.exec_dir + "init-ticket.ini";     
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

for(var code in file_area.dir) {

	var area = file_area.dir[code];
	var ticline;
	f.writeln('[' + area.name + "]\nDir=" + code.toUpperCase());
	if (ini[area.name]) {
		if(ini[area.name].domain) {
			f.writeln('Handler=tickit/nodelist_handler.js');
			ticline = 'HandlerArg={"domain":"' + ini[area.name].domain + '"';
			if (ini[area.name].match) {
				ticline += ' ,"match":"' + ini[area.name].match + '"';
			}
			ticline  += ', "nlmatch":"' + ini[area.name].nlmatch + '"}';
			f.writeln(ticline);
		}
		if (ini[area.name].forcereplace)
			f.writeln('Forcereplace = true');
	}
	f.writeln();
}
f.close();
