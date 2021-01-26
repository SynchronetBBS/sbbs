"use strict";

writeln("Generating config files...");

var i;

var gamedir = fullpath(js.startup_dir);

var lines = [];

lines[1] = system.name;
var op = system.operator.split(" ", 2);
lines[2] = op[0];
lines[3] = op[1];
lines[4] = "5"; // games allowed per day
lines[5] = "1000"; // rubies each player starts with
lines[6] = "30"; // warrior battles player gets per day
lines[7] = "2"; // player battles player gets per day
lines[8] = "N"; // cleasn mode

for(i = 0; i < system.nodes; i++) {
	var nodenum = i + 1;

	lines[0] = system.node_list[i].dir + "door.sys";
	
	writeln("Creating " + js.startup_dir + 'QUEST' + nodenum + '.CFG');
	
	var file = new File(js.startup_dir + 'QUEST' + nodenum + '.CFG');
	if (!file.open("w")) {
		writeln("Error " + file.error + " opening " + file.name + " for writing");
		exit(1)
	}
	file.writeAll(lines);
	file.close();
}

writeln("Config generation complete");

exit(0);