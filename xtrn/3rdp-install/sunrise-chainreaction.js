"use strict";

writeln("Generating config files...");

var i;

var gamedir = fullpath(js.startup_dir);
var conffilesrc = "SAMPLE.CFG";

if (!file_exists(gamedir + conffilesrc)) {
	writeln("Conf not found: " + gamedir + conffilesrc);
	exit(1);
}

var cfg_filename = js.startup_dir + conffilesrc;
var file = new File(cfg_filename);
if (!file.open("r")) {
	writeln("Error " + file.error + " opening " + file.name);
	exit(1)
}

var lines = file.readAll();
file.close();

lines[0] = system.name;
lines[1] = system.operator;
lines[3] = gamedir + "reactsc.ans";
lines[4] = gamedir + "reactsc.asc";

file_backup(gamedir + "CHREACT.CFG", 3);

var file = new File(js.startup_dir + 'CHREACT.CFG');
if (!file.open("w")) {
	writeln("Error " + file.error + " opening " + file.name + " for writing");
	exit(1)
}
file.writeAll(lines);
file.close();

writeln("Config generation complete");

exit(0);