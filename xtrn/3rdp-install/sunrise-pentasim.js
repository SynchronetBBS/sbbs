"use strict";

writeln("Generating config files...");

var i;

var gamedir = fullpath(js.startup_dir);

var lines = [];

lines[1] = system.name;
var op = system.operator.split(" ", 2);
lines[2] = op[0];
lines[3] = op[1];
lines[4] = "1000"; // krrons player starts with
lines[5] = "20"; // cyborg battles per day
lines[6] = "2"; // ckm card games per day
lines[7] = "1"; // scripts allowed per day

for (i in system.node_list) {
	var nodenum = parseInt(i, 10) + 1;
	lines[0] = system.node_list[i].dir + "\DOOR.SYS";
	
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