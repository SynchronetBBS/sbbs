"use strict";

writeln("Generating config files...");

var i;

var gamedir = fullpath(js.startup_dir);
var conffilesrc = "BETSY.CFG";
var cfg_filename = gamedir + conffilesrc;

if (!file_exists(cfg_filename)) {
	writeln("Conf not found: " + cfg_filename);
	exit(1);
}

var file = new File(cfg_filename);
if (!file.open("r")) {
	writeln("Error " + file.error + " opening " + file.name);
	exit(1)
}

var lines = file.readAll();
file.close();

lines[1] = system.name;
var op = system.operator.split(" ", 2);
lines[2] = op[0];
lines[3] = op[1];

lines[6] = gamedir + "betsy.ans";
lines[7] = gamedir + "betsy.asc";
lines[8] = gamedir + "betsyhof.ans";
lines[9] = gamedir + "betsyhof.asc";

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