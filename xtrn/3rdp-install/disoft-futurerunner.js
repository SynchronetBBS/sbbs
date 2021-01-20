"use strict";

writeln("Generating batch files...");

var i;

var lines = [];

lines[0] = "@ECHO OFF";
writeln("Beginning node config generation...");
for (i in system.node_list) {
	var nodenum = parseInt(i, 10) + 1;
	var nodenumpad = ("00" + nodenum).slice(-2);
	lines[1] = "FUTURE _FUTURE.C" + nodenumpad;
	
	writeln("Creating " + js.startup_dir + 'future' + nodenum + '.bat');
	
	var file = new File(js.startup_dir + 'future' + nodenum + '.bat');
	if (!file.open("w")) {
		writeln("Error " + file.error + " opening " + file.name + " for writing");
		exit(1)
	}
	file.writeAll(lines);
	file.close();
}

writeln("Batch file generation complete");

exit(0);
