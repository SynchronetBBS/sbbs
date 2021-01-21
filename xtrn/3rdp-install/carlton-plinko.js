"use strict";

writeln("Generating config files...");

var i;

var gamedir = fullpath(js.startup_dir);

var cfg_filename = js.startup_dir + 'PLINKO.CFG';
var file = new File(cfg_filename);
if (!file.open("r")) {
	writeln("Error " + file.error + " opening " + file.name);
	exit(1)
}

var lines = file.readAll();
file.close();

lines[0] = "FREE COPY";
lines[1] = "BBSFILES.COM";
lines[2] = "1";
lines[4] = "38400";
lines[5] = gamedir;
lines[6] = "PLINKO.ASC";
lines[7] = "PLINKO.ANS";
lines[8] = "10";
lines[9] = "10";
lines[10] = "@PAUSE@";
lines[11] = "1537381269776M";

writeln("Beginning node config generation...");
for (i in system.node_list) {
	var nodenum = parseInt(i, 10) + 1;
	lines[3] = system.node_list[i].dir;
	
	writeln("Creating " + js.startup_dir + 'node' + nodenum + '.cfg');
	
	var file = new File(js.startup_dir + 'node' + nodenum + '.cfg');
	if (!file.open("w")) {
		writeln("Error " + file.error + " opening " + file.name + " for writing");
		exit(1)
	}
	file.writeAll(lines);
	file.close();
}

writeln("Config generation complete");

exit(0);
