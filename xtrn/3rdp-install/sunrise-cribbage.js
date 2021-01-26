"use strict";

writeln("Generating config files...");

var i;

var gamedir = fullpath(js.startup_dir);
var conffilesrc = "CRIBSQ.CFG";
var cfg_filename = gamedir + conffilesrc;

if (!file_exists(cfg_filename)) {
	writeln("Conf not found: " + cfg_filename);
	exit(1);
}

file_backup(cfg_filename, 3);

var file = new File(cfg_filename);
if (!file.open("r")) {
	writeln("Error " + file.error + " opening " + file.name);
	exit(1)
}

var lines = file.readAll();
file.close();

lines[0] = "%PCBDRIVE%%PCBDIR%door.sys";
lines[1] = system.name;

var op = system.operator.split(" ", 2);
lines[2] = op[0];
lines[3] = op[1];

lines[7] = gamedir + "scrib.ans";
lines[8] = gamedir + "scrib.asc";
lines[9] = gamedir + "scribhof.ans";
lines[10] = gamedir + "scribhof.asc";
lines[11] = "1";
lines[38] = "G";

var file = new File(js.startup_dir + conffilesrc);
if (!file.open("w")) {
	writeln("Error " + file.error + " opening " + file.name + " for writing");
	exit(1)
}
file.writeAll(lines);
file.close();

writeln("Config generation complete");

exit(0);