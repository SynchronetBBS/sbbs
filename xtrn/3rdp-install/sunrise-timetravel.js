"use strict";

writeln("Generating config files...");

var i;

var gamedir = fullpath(js.startup_dir);
var conffilesrc = "TTRAVEL.CFG";
var cfg_filename = gamedir + conffilesrc;

file_backup(cfg_filename, 3);

var file = new File(cfg_filename);
if (!file.open("r")) {
	writeln("Error " + file.error + " opening " + file.name);
	exit(1)
}
var lines = file.readAll();
file.close();

lines[0] = system.node_list[0].dir + 'door.sys';
lines[1] = system.name;
var op = system.operator.split(" ", 2);
lines[2] = op[0];
lines[3] = op[1];
lines[4] = gamedir;

writeln("Creating " + cfg_filename);

var file = new File(cfg_filename);
if (!file.open("w")) {
	writeln("Error " + file.error + " opening " + file.name + " for writing");
	exit(1)
}
file.writeAll(lines);
file.close();

for(i = 0; i < system.nodes; i++) {
	var nodenum = i + 1;

	lines[0] = system.node_list[i].dir + "door.sys";
	
	writeln("Creating " + js.startup_dir + 'TTRAVEL' + nodenum + '.CFG');
	
	var file = new File(js.startup_dir + 'TTRAVEL' + nodenum + '.CFG');
	if (!file.open("w")) {
		writeln("Error " + file.error + " opening " + file.name + " for writing");
		exit(1)
	}
	file.writeAll(lines);
	file.close();
}

writeln("Config generation complete");

exit(0);