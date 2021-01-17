"use strict";

var cfg_filename = js.startup_dir + 'FOODFITE.CFG';

var file = new File(cfg_filename);
if (!file.open("r")) {
    writeln("Error " + file.error + " opening " + file.name);
    exit(1)
}

var lines = file.readAll();
file.close();

lines[0] = "LINE";
lines[1] = system.name;
lines[2] = system.operator;
lines[9] = "NONE";
lines[10] = "";
lines[11] = "";
lines[12] = "";
lines[13] = "";
lines[16] = "PORT:F:1";

var file = new File(cfg_filename);
if (!file.open("w")) {
  writeln("Error " + file.error + " opening " + file.name + " for writing");
  exit(1)
}
file.writeAll(lines);
file.close();

exit(0);