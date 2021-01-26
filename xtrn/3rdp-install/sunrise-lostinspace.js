"use strict";

writeln("Generating config files...");

var i;

var gamedir = fullpath(js.startup_dir);

var lines = [];

lines[1] = system.name;
var op = system.operator.split(" ", 2);
lines[2] = op[0];
lines[3] = op[1];
lines[4] = "2000"; // starting currency
lines[5] = "20"; // enemy engages per day
lines[6] = "2"; // player engages per day
lines[7] = "0"; // ?

for (i in system.node_list) {
	var nodenum = parseInt(i, 10) + 1;
	lines[0] = system.node_list[i].dir + "\DOOR.SYS";
	
	writeln("Creating " + js.startup_dir + 'LIS' + nodenum + '.CFG');
	
	var file = new File(js.startup_dir + 'LIS' + nodenum + '.CFG');
	if (!file.open("w")) {
		writeln("Error " + file.error + " opening " + file.name + " for writing");
		exit(1)
	}
	file.writeAll(lines);
	file.close();
}

writeln("Config generation complete");

exit(0);