"use strict";

var gamedir = fullpath(js.startup_dir);

var lines = [];
lines[1] = gamedir;
lines[2] = gamedir;
lines[3] = "   0";

writeln("Beginning node config generation...");
for(i = 0; i < system.nodes; i++) {
	var nodenum = i + 1;
	
	file_backup(gamedir + 'exnod' + nodenum + '.cfg');

	lines[0] = system.node_list[i].dir + 'door.sys';
	var file = new File(gamedir + 'exnod' + nodenum + '.cfg');
	if (!file.open("w")) {
		writeln("Error " + file.error + " opening " + file.name + " for writing");
		exit(1)
	}
	file.writeAll(lines);
	file.close();
}

writeln("Config file generation complete");

exit(0);
