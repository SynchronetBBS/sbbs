"use strict";

var gamedir = fullpath(js.startup_dir);

file_backup(gamedir + "GAME.CTL", 3);

var cfg_filename = gamedir + 'GAME.CTL';

var lines = [];

var op = system.operator.split(" ", 2);

lines[0] = "SYSOPFIRST " + op[0];
lines[1] = "SYSOPLAST " + op[1];
lines[2] = "BBSNAME " + system.name;
lines[3] = "BBSTYPE DOORSYS";
lines[4] = "FOSSIL";
lines[5] = "LOCKBAUD 38400";
lines[6] = "COMPORT 1";
lines[7] = "STATUS ON";
lines[8] = "STATFORE 7";
lines[9] = "STATBACK 1";

var file = new File(cfg_filename);
if (!file.open("w")) {
	writeln("Error " + file.error + " opening " + file.name + " for writing");
	exit(1)
}
file.writeAll(lines);
file.close();

exit(0);