"use strict";

writeln("Generating config files...");

var i;

var gamedir = fullpath(js.startup_dir);

var lines = [];

lines[2] = system.name;
lines[3] = system.operator;
lines[4] = "0";
lines[5] = gamedir + "hexx.ans";
lines[6] = gamedir + "hexx.asc";
lines[7] = gamedir + "hexxhof.ans";
lines[8] = gamedir + "hexxhof.asc";
lines[9] = "10";
lines[10] = "3";
lines[11] = "20";
lines[12] = "3";

for(i = 0; i < system.nodes; i++) {
	var nodenum = i + 1;

	lines[0] = nodenum;
	lines[1] = system.node_list[i].dir + "door.sys";
	
	writeln("Creating " + js.startup_dir + 'NODE' + nodenum + '.CFG');
	
	var file = new File(js.startup_dir + 'NODE' + nodenum + '.CFG');
	if (!file.open("w")) {
		writeln("Error " + file.error + " opening " + file.name + " for writing");
		exit(1)
	}
	file.writeAll(lines);
	file.close();
}

writeln("Config generation complete");

exit(0);