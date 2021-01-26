"use strict";

writeln("Generating config files...");

var i;

var gamedir = fullpath(js.startup_dir);
var conffile = "PAIR3.CFG";

if (!file_exists(gamedir + conffile)) {
	writeln("Conf not found: " + gamedir + conffile);
	exit(1);
}

file_backup(gamedir + conffile, 3);

var cfg_filename = js.startup_dir + conffile;
var file = new File(cfg_filename);
if (!file.open("r")) {
	writeln("Error " + file.error + " opening " + file.name);
	exit(1)
}

var lines = file.readAll();
file.close();

lines[0] = '%PCBDRIVE%%PCBDIR%\DOOR.SYS';
lines[1] = system.name;

var op = system.operator.split(" ", 2);
lines[2] = op[0];
lines[3] = op[1];

lines[7] = gamedir + "3pptop.asc";
lines[8] = gamedir + "3pptop.ans";
lines[9] = gamedir + "3pphof.asc";
lines[10] = gamedir + "3pphof.ans";

lines[11] = "1";

var file = new File(js.startup_dir + conffile);
if (!file.open("w")) {
	writeln("Error " + file.error + " opening " + file.name + " for writing");
	exit(1)
}
file.writeAll(lines);
file.close();

writeln("Config generation complete");

exit(0);