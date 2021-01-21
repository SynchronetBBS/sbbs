"use strict";

writeln("Generating config files...");

var i;

var gamedir = fullpath(js.startup_dir);

if (mkdir(gamedir + "/data")) {
	
}


var lines = [];
lines[0] = 'GAP';
lines[2] = system.name;
lines[3] = system.operator;
lines[4] = '38400';

writeln("Beginning node config generation...");
for (i in system.node_list) {
	var nodenum = parseInt(i, 10) + 1;
	lines[1] = system.node_list[i].dir;
	
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
