"use strict";

writeln("Generating config files...");

var i;

var gamedir = fullpath(js.startup_dir);

var lines = [];
lines[0] = 'DOOR32';
lines[2] = system.name;
lines[3] = system.operator;
lines[4] = 'TelNet';
lines[5] = gamedir;
lines[6] = '0';
lines[7] = '0';
lines[8] = '0';
lines[9] = '0';

writeln("Beginning node config generation...");
for(i = 0; i < system.nodes; i++) {
	var nodenum = i + 1;
	lines[1] = system.node_list[i].dir;
	
	writeln("Creating " + js.startup_dir + 'NODE' + nodenum + '.CTL');
	
	var file = new File(js.startup_dir + 'NODE' + nodenum + '.CTL');
	if (!file.open("w")) {
		writeln("Error " + file.error + " opening " + file.name + " for writing");
		exit(1)
	}
	file.writeAll(lines);
	file.close();
}

writeln("Config generation complete");

exit(0);
