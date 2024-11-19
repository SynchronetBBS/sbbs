// Export legacy areas.bbs file to (new/modern) areas.ini format

var areas_bbs = argv[0] || file_getcase(system.data_dir + "areas.bbs");
var areas_ini = system.data_dir + "areas.ini";
if(file_exists(areas_ini) && deny("Overwrite " + areas_ini))
	exit(0);

var fin = new File(areas_bbs);
if(!fin.open("r"))
	throw new Error("Error " + fin.error + " opening " + fin.name);
var fout = new File(areas_ini);
if(!fout.open("w"))
	throw new Error("Error " + fout.error + " opening " + fout.name);
var text = fin.readAll();
var areas = 0;
for(var i in text) {
	var line = text[i];
	var match = line.match(/(\S+)\s+(\S+)\s+(.*)$/);
	if(!match || match.length < 3)
		continue;
	if(match[1][0] == ';') // comment
		continue;
	var tag = match[2];
	var sub = match[1];
	var links = match[3];
	fout.writeln(format("[%s]", tag));
	if(sub.toUpperCase() == "P")
		fout.writeln("pass-through = true");
	else
		fout.writeln(format("sub = %s", sub));
	fout.writeln(format("links = %s", links));
	fout.writeln();
	++areas;
}
print(areas + " areas from " + areas_bbs + " exported to " + areas_ini);