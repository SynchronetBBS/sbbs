"use strict";

writeln("Generating config files...");

var i;

var gamedir = fullpath(js.startup_dir);
var conffilesrc = "STAIRWAY.CFG";

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

file_backup(cfg_filename, 3);

lines[0] = "%PCBDRIVE%%PCBDIR%door.sys";
lines[1] = system.name;
var op = system.operator.split(" ", 2);
lines[2] = op[0];
lines[3] = op[1];
lines[10] = gamedir + "stair.asc";
lines[11] = gamedir + "stair.ans";
lines[12] = gamedir + "stairhof.asc";
lines[13] = gamedir + "stairhof.ans";

writeln("Creating " + cfg_filename);

var file = new File(cfg_filename);
if (!file.open("w")) {
	writeln("Error " + file.error + " opening " + file.name + " for writing");
	exit(1)
}
file.writeAll(lines);
file.close();

writeln("Config generation complete");

exit(0);