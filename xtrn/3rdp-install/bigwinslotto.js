"use strict";

writeln("Generating config files...");

var i;

var gamedir = fullpath(js.startup_dir);

file_backup(gamedir + "BWLOTTO.CFG", 3);

var cfg_filename = js.startup_dir + 'BWLOTTO.CFG';
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

lines[9] = gamedir + "winner.txt";

var file = new File(js.startup_dir + 'BWLOTTO.CFG');
if (!file.open("w")) {
	writeln("Error " + file.error + " opening " + file.name + " for writing");
	exit(1)
}
file.writeAll(lines);
file.close();

writeln("Config generation complete");

exit(0);