"use strict";

writeln("Generating config files...");

var i;

var gamedir = fullpath(js.startup_dir);

var cfg_filename = gamedir + 'MINEZONE.CFG';
var file = new File(cfg_filename);
if (!file.open("r")) {
	writeln("Error " + file.error + " opening " + file.name);
	exit(1)
}

var lines = file.readAll();
file.close();

lines[4] = gamedir + "MINESCOR.ASC";
lines[5] = gamedir + "MINESCOR.ANS";

writeln("Beginning node config generation...");
for(i = 0; i < system.nodes; i++) {
	var nodenum = i + 1;
	lines[0] = system.node_list[i].dir + 'door.sys';
	
	writeln("Creating " + js.startup_dir + 'node' + nodenum + '.cfg');
	
	var file = new File(js.startup_dir + 'node' + nodenum + '.cfg');
	if (!file.open("w")) {
		writeln("Error " + file.error + " opening " + file.name + " for writing");
		exit(1)
	}
	file.writeAll(lines);
	file.close();
}

writeln("Config generation complete");

exit(0);
