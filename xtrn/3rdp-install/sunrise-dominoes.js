"use strict";

writeln("Generating config files...");

var i;

var gamedir = fullpath(js.startup_dir);
var conffilesrc = "DOMINOES.CFG";

if (!file_exists(gamedir + conffilesrc)) {
	writeln("Conf not found: " + gamedir + conffilesrc);
	exit(1);
}

file_backup(gamedir + conffilesrc, 3);

var cfg_filename = js.startup_dir + conffilesrc;
var file = new File(cfg_filename);
if (!file.open("r")) {
	writeln("Error " + file.error + " opening " + file.name);
	exit(1)
}

var lines = file.readAll();
file.close();

lines[0] = "%PCBDRIVE%%PCBDIR%\DOOR.SYS";
lines[1] = system.name;

var op = system.operator.split(" ", 2);
lines[2] = op[0];
lines[3] = op[1];

lines[11] = gamedir + "scdom.asc";
lines[12] = gamedir + "scdom.ans";
lines[13] = gamedir + "scdomhof.asc";
lines[14] = gamedir + "scdomhof.ans";

var file = new File(js.startup_dir + conffilesrc);
if (!file.open("w")) {
	writeln("Error " + file.error + " opening " + file.name + " for writing");
	exit(1)
}
file.writeAll(lines);
file.close();

writeln("Config generation complete");

exit(0);
