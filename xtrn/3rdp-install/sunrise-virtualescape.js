"use strict";

writeln("Generating config files...");

var i;

var gamedir = fullpath(js.startup_dir);
var conffilesrc = "VIRTUAL.CFG";

var cfg_filename = gamedir + conffilesrc;

file_backup(cfg_filename, 3);

var lines = [];

lines[0] = "%PCBDRIVE%%PCBDIR%door.sys";
lines[1] = system.name;
var op = system.operator.split(" ", 2);
lines[2] = op[0];
lines[3] = op[1];

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