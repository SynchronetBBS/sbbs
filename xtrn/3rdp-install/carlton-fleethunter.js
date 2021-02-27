"use strict";

writeln("Generating config files...");

var i;

var gamedir = fullpath(js.startup_dir);

file_backup(gamedir + "HUNTER.CFG", 3);

var cfg_filename = js.startup_dir + 'SAMPLE.CFG';
var file = new File(cfg_filename);
if (!file.open("r")) {
	writeln("Error " + file.error + " opening " + file.name);
	exit(1)
}

var lines = file.readAll();
file.close();

lines[2] = gamedir + "CARLTON.INF";
lines[3] = gamedir + "TRISCORE.ANS";
lines[4] = gamedir + "TRISCORE.ASC";

var file = new File(js.startup_dir + 'HUNTER.CFG');
if (!file.open("w")) {
	writeln("Error " + file.error + " opening " + file.name + " for writing");
	exit(1)
}
file.writeAll(lines);
file.close();

writeln("Config generation complete");

exit(0);