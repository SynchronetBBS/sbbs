"use strict";

writeln("Generating config files...");

var gamedir = fullpath(js.startup_dir);

var nodefile = gamedir + "NODES.CFG";

file_backup(nodefile);

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
