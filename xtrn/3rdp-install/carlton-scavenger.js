"use strict";

writeln("Generating config files...");

var i;

var gamedir = fullpath(js.startup_dir);

file_backup(gamedir + "SCAVHUNT.CFG", 3);

var cfg_filename = js.startup_dir + 'SCAVHUNT.CFG';
var file = new File(cfg_filename);
if (!file.open("r")) {
	writeln("Error " + file.error + " opening " + file.name);
	exit(1)
}

var lines = file.readAll();
file.close();

lines[2] = gamedir + "CARLTON.INF";

var file = new File(js.startup_dir + 'SCAVHUNT.CFG');
if (!file.open("w")) {
	writeln("Error " + file.error + " opening " + file.name + " for writing");
	exit(1)
}
file.writeAll(lines);
file.close();

var file = new File(js.startup_dir + 'HUNTLIST.TXT');
if (!file.open("w")) {
	writeln("Error " + file.error + " opening " + file.name + " for writing");
	exit(1)
}
file.writeAll("\r\n");
file.close();

writeln("Config generation complete");

exit(0);