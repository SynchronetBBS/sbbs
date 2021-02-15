"use strict";

writeln("Generating config files...");

var gamedir = fullpath(js.startup_dir);
var configfile = gamedir + "RTB.CFG";
var nodefile = gamedir + "NODES.CFG";

file_backup(configfile);
file_backup(nodefile);

var conf = new File(configfile);
if (conf.open(file_exists(configfile) ? 'r+' : 'w+')) {
	conf.iniSetValue("DOOR SETTINGS", "BBSNAME", system.name);
	conf.close();
} else {
	writeln("Could not open " + configfile + " for writing");
	exit(1);
}

var nodeconf = new File(nodefile);
if (nodeconf.open(file_exists(nodefile) ? 'r+' : 'w+')) {
	for(i = 0; i < system.nodes; i++) {
		var nodenum = i + 1;
		nodeconf.iniSetValue("NODE" + nodenum, "DROPFILE", system.node_list[i].dir + 'door.sys');
		nodeconf.iniSetValue("NODE" + nodenum, "FOSSIL", "Y");
	}
	nodeconf.close();
} else {
	writeln("Could not open " + nodefile + " for writing");
	exit(1);
}


writeln("Config generation complete");

exit(0);
