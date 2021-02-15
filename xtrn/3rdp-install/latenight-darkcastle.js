"use strict";

writeln("Generating config file...");

var i;

var gamedir = fullpath(js.startup_dir);
var configfile = gamedir + "CASTLE.CFG";

file_backup(configfile);

var lines = [];

var file = new File(configfile);
if (file.open(configfile, 'r+')) {
	lines = file.readAll();
} else {
	writeln("Error " + file.error + " opening " + file.name + " for writing");
	exit(1)
}

if (lines[0] != "BBSFiles.com") {
	var op = system.operator.split(" ", 2);
	lines[0] = system.name;
	lines[1] = op[0];
	lines[2] = op[1];
}

lines[4] = gamedir + "castle.asc";
lines[5] = gamedir + "castle.ans";

file.writeAll(lines);

writeln("Config generation complete");

exit(0);
