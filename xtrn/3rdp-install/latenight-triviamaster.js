"use strict";

writeln("Generating config file...");

var i;

var gamedir = fullpath(js.startup_dir);
var configfile = gamedir + "TRIVIA1.CFG";

file_backup(configfile);

var lines = [];

var file = new File(configfile);
if (file.open(configfile, 'r')) {
	lines = file.readAll();
	file.close();
} else {
	writeln("Error " + file.error + " opening " + file.name + " for writing");
	exit(1)
}

var op = system.operator.split(" ", 2);
lines[0] = system.name;
lines[1] = op[0];
lines[2] = op[1];

lines[4] = gamedir + "triviam.asc";
lines[5] = gamedir + "triviam.ans";

file.writeAll(lines);
file.close();

writeln("Beginning node config generation...");
for(i = 0; i < system.nodes; i++) {
	var nodenum = i + 1;
	lines[3] = system.node_list[i].dir + "DOOR.SYS";
	lines[11] = nodenum;

	writeln("Creating " + gamedir + 'NODE' + nodenum + '.CFG');
	
	var file = new File(gamedir + 'NODE' + nodenum + '.CFG');
	if (!file.open("w")) {
		writeln("Error " + file.error + " opening " + file.name + " for writing");
		exit(1)
	}
	file.writeAll(lines);
	file.close();
}

writeln("Config generation complete");

exit(0);
