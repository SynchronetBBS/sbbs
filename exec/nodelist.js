// $Id$

// Node Listing / Who's Online display script, replaces the bbs.whos_online() function
// Installable as a Ctrl-U key handler ('jsexec nodelist install') or as an
// 'exec/node list' replacement ('jexec nodelist').
//
// Command-line / load() options supported:
// -active      Include active users/nodes only
// -noself      Exclude current/own node from list/output
// -noweb       Exclude web users from list/output

// The modopts.ini [nodelist] settings are only used when executing from the BBS/terminal server
// (e.g. not when executed via JSexec)

"use strict";

if(js.global.bbs)
	require("text.js", 'NodeLstHdr');

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
		options.format = "\x01n\x01h%3d  \x01n\x01g%s";
	if(!options.username_prefix)
		options.username_prefix = '\x01h';
	if(!options.status_prefix)
		options.status_prefix = '\x01n\x01g';	
	if(!options.errors_prefix)
		options.errors_prefix = '\x01h\x01r';
	console.print(bbs.text(NodeLstHdr));
} else{ // e.g. invoked via JSexec
	var REVISION = "$Revision$".split(' ')[1];
	options = { 
		format: "Node %2d: %s", 
		include_age: true, 
		include_gender: true, 
		include_web_users: argv.indexOf('-noweb') == -1
	};
	writeln("Synchronet Node Display Module v" + REVISION);
	writeln();
}

var active = argv.indexOf('-active') >= 0;
var listself = argv.indexOf('-noself') < 0;
var is_sysop = js.global.bbs ? user.is_sysop : true;

var output = presence.nodelist(/* print: */true, active, listself, is_sysop, options);
for(var i in output)
	writeln(output[i]);
if(js.global.bbs && active && !output)
	write(bbs.text(NoOtherActiveNodes));
