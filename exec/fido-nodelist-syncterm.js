// Export Telnet (?) addresses from FidoNet nodelist to syncterm.lst format

"use strict";

require('ftn_nodelist.js', 'NodeList');

var nodelist_filename = argv[0];
var syncterm_listfile = argv[1];

const nodelist = new NodeList(nodelist_filename);

var out = {};
for(var i in nodelist.entries) {
	var entry = nodelist.entries[i];

	if(entry.private)
		continue;
	if(entry.down)
		continue;
	if(!entry.flags) {
		log("No flags for " + entry.name);
		continue;
	}
	var inet_address = entry.flags.INA;
	if(typeof inet_address != "string")
		inet_address = entry.flags.ITN;
	if(typeof inet_address != "string")
		inet_address = entry.flags.IBN;
	if(typeof inet_address != "string") {
		log("No Telnet address available for " + entry.addr + " " + entry.name);
		continue;
	}
	var colon = inet_address.indexOf(':');
	if(colon > 0)
		inet_address = inet_address.substring(0, colon);

	out[inet_address] = entry;
}

var lst = new File(syncterm_listfile);
if(!lst.open("w")) {
	log("Error " + lst.error + " opening " + lst.name);
	exit(1);
}

lst.writeln("; Exported " + nodelist.entries.length + " from " + file_getname(nodelist_filename) + " on " + new Date().toString());
lst.writeln();
var exported = 0;
for(var i in out) {
	var entry = out[i];
	lst.writeln(format("[%s]", entry.name));
	lst.writeln(format("\tAddress=%s", i));
	lst.writeln(format("\tConnectionType=%s", "Telnet"));
	var itn = entry.flags.ITN;
	if(typeof itn == "string") {
		var colon = itn.indexOf(':');
		if(colon >= 0)
			itn = itn.substring(colon + 1);
		var port = parseInt(itn, 10);
		if(port > 0)
			lst.writeln(format("\tPort=%u", port));
	}
	var comment = format("%s node %s", nodelist.domain, entry.addr);
	if(entry.sysop)
		comment += ", " + entry.sysop;
	if(entry.location)
		comment += ", " + entry.location;
	lst.writeln("\tComment=" + comment);
	exported++;
}
log("Exported " + exported + " entries to " + syncterm_listfile);
lst.close();

