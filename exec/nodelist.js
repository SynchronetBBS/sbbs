// $Id$

// Node Listing / Who's Online display script, replaces the bbs.whos_online() function
// Installable as a Ctrl-U key handler ('jsexec nodelist install') or as an
// 'exec/node list' replacement ('jexec nodelist').

if(js.global.bbs)
	require("text.js", 'NodeLstHdr');

"use strict";

var presence = load({}, "presence_lib.js");

if(argv.indexOf("install") >= 0) {
	var cnflib = load({}, "cnflib.js");
	var xtrn_cnf = cnflib.read("xtrn.cnf");
	if(!xtrn_cnf) {
		alert("Failed to read xtrn.cnf");
		exit(-1);
	}
	xtrn_cnf.hotkey.push({ key: /* Ctrl-U */21, cmd: '?nodelist.js -active' });
	
	if(!cnflib.write("xtrn.cnf", undefined, xtrn_cnf)) {
		alert("Failed to write xtrn.cnf");
		exit(-1);
	}
	exit(0);
}


writeln();
if(js.global.bbs) {
	var options = load({}, "modopts.js", "nodelist");
	if(!options)
		options = {};
	if(!options.format)
		options.format = "\1n\1h%3d  \1n\1g%s";
	if(!options.username_prefix)
		options.username_prefix = '\1h';
	if(!options.status_prefix)
		options.status_prefix = '\1n\1g';	
	if(!options.errors_prefix)
		options.errors_prefix = '\1h\1r';	
	console.print(bbs.text(NodeLstHdr));
} else{ // e.g. invoked via JSexec
	var REVISION = "$Revision$".split(' ')[1];
	options = { format: "Node %2d: %s", include_age: true, include_gender: true };
	writeln("Synchronet Node Display Module v" + REVISION);
	writeln();
}

var active = argv.indexOf('-active') >= 0;
var listself = argv.indexOf('-notself') < 0;
var is_sysop = js.global.bbs ? user.is_sysop : true;

var output = presence.nodelist(/* print: */true, active, listself, is_sysop, options);
for(var i in output)
	writeln(output[i]);
if(js.global.bbs && active && !output)
	write(bbs.text(NoOtherActiveNodes));
