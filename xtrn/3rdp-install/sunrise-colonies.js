"use strict";

writeln("Generating config files...");

var i;

var gamedir = fullpath(js.startup_dir);

var lines = [];
lines[1] = system.name;
var op = system.operator.split(" ", 2);
lines[2] = op[0];
lines[3] = op[1];
lines[4] = 'N';
lines[5] = '2';
lines[6] = '60';
lines[7] = '5';
lines[8] = strftime("%m-%d-%Y");

for(i = 0; i < system.nodes; i++) {
	var nodenum = i + 1;
	lines[0] = system.node_list[i].dir + "door.sys";
	
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