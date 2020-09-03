// $Id: echoareas.js,v 1.1 2018/02/21 04:23:09 rswindell Exp $

// Displays (in areas.bbs format) all the FTN-linked sub-boards with their
// configured "GroupHub" (from sbbsecho.ini -> [node:*] -> GroupHub).
// Requires that you're using SBBSecho v3.x and configured one or more hubs in
// EchoCfg->Linked Nodes.

// To use this script to generate an area file, just redirect the output, e.g.
// jsexec echoareas > /sbbs/data/areas.bbs

// Warning the above example will *overwrite* your existing areas.bbs file
// if you have one, so use with caution.

// Thanks to Nelgin (endofthelinebbs.com) for the idea for this script.

load('sbbsdefs.js');

var ini = new File(file_cfgname(system.ctrl_dir, "sbbsecho.ini"));
if(!ini.open("r")) {
	alert("Error " + ini.error + " opening " + ini.name);
	exit(1);
}
var node = ini.iniGetAllObjects("addr", "node:");
ini.close();

var t = time();
var sub;
for(sub in msg_area.sub) {
	if(!(msg_area.sub[sub].settings & SUB_FIDO))
		continue;
	printf("%-16s %-35s ", sub, msg_area.sub[sub].name.replace(' ', '_').toUpperCase());
	for(var n in node)
		if(node[n].GroupHub
			&& node[n].GroupHub.toLowerCase() == msg_area.sub[sub].grp_name.toLowerCase())
			printf("%s ", node[n].addr);	// there *should* be only one hub per area
	print();	// end-of-line
}
