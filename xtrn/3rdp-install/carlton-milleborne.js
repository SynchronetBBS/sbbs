"use strict";

writeln("Generating config files...");

var i;

var gamedir = fullpath(js.startup_dir);

file_backup(gamedir + "MB.CFG", 3);

var cfg_filename = js.startup_dir + 'MB.CFG';
var file = new File(cfg_filename);
if (!file.open("r")) {
	writeln("Error " + file.error + " opening " + file.name);
	exit(1)
}

var lines = file.readAll();
file.close();

lines[2] = gamedir + "CARLTON.INF";
lines[3] = gamedir + "SCORE-MB.ASC";
lines[4] = gamedir + "SCORE-MB.ANS";

var file = new File(js.startup_dir + 'MB.CFG');
if (!file.open("w")) {
	writeln("Error " + file.error + " opening " + file.name + " for writing");
	exit(1)
}
file.writeAll(lines);
file.close();

writeln("Config generation complete");

exit(0);