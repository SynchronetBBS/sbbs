"use strict";

writeln("Generating batch file...");

var i;

var gamedir = fullpath(js.startup_dir);

var conffilesrc = "warlord.bat";
var cfg_filename = gamedir + conffilesrc;

var lines = [];
lines[0] = "@echo off";
lines[1] = "warlord %1";
lines[2] = "wargame %1";

writeln("Creating " + cfg_filename);

var file = new File(cfg_filename);
if (!file.open("w")) {
	writeln("Error " + file.error + " opening " + file.name + " for writing");
	exit(1)
}
file.writeAll(lines);
file.close();

writeln("Batch file generation complete");

exit(0);