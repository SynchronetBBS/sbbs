"use strict";

writeln("Generating batch files...");

var i;

var lines = [];

lines[0] = "@ECHO OFF";
writeln("Beginning node config generation...");
for(i = 0; i < system.nodes; i++) {
	var nodenum = i + 1;
	var nodenumpad = ("00" + nodenum).slice(-2);
	lines[1] = "REbEL _REBEL.C" + nodenumpad;
	
	writeln("Creating " + js.startup_dir + 'rebel' + nodenum + '.bat');
	
	var file = new File(js.startup_dir + 'rebel' + nodenum + '.bat');
	if (!file.open("w")) {
		writeln("Error " + file.error + " opening " + file.name + " for writing");
		exit(1)
	}
	file.writeAll(lines);
	file.close();
}

writeln("Batch file generation complete");

exit(0);
