// $Id: nodelist.js,v 1.12 2020/04/21 20:30:19 rswindell Exp $

// Node Listing / Who's Online Module
// Installed in SCFG->System->Loadable Modules->List Nodes / Who's Online
// or as an 'exec/node list' replacement ('jexec nodelist').
//
// Command-line / load() arguments supported:
// -active      Include active users/nodes only
// -noself      Exclude current/own node from list/output
// -noweb       Exclude web users from list/output
// -clear       Clear the screen (if possible) before list
// -home        Home the cursor (if possible) before list
// -loop [n]    Loop the list, delaying n seconds (default: 2.0 seconds)

// The modopts.ini [nodelist] and [web] settings are only used when executing
// from the BBS/terminal server (e.g. not when executed via JSexec)

"use strict";

require("sbbsdefs.js", 'K_NONE');

if(argv.indexOf("install") >= 0) {
	// nothing to do here
	exit(0);
}

// Load the user-presence library (if not already loaded)
var presence;
if(js.global.bbs) {
	presence = bbs.mods.presence_lib;
	require("text.js", 'NodeLstHdr');
}

if(!presence) {
	presence = load({}, "presence_lib.js");
	if(js.global.bbs)
		bbs.mods.presence_lib = presence;	// cache the loaded-lib
}

writeln();
if(js.global.bbs) {
	var options = bbs.mods.nodelist_options;
	if(!options)
		options = load({}, "nodelist_options.js");
	js.on_exit("console.status = " + console.status);
} else{ // e.g. invoked via JSexec
	var REVISION = "$Revision: 1.12 $".split(' ')[1];
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
var clear = argv.indexOf('-clear') >= 0;
var home = argv.indexOf('-home') >= 0;
var listself = argv.indexOf('-noself') < 0;
var loop = argv.indexOf('-loop');
var delay = 2;
if(loop >= 0) {
	var tmp = parseFloat(argv[loop + 1]);
	if(!isNaN(tmp))
		delay = tmp;
	loop = true;
} else 
	loop = false;
var is_sysop = js.global.bbs ? user.is_sysop : true;

if(clear && js.global.console)
	console.clear();

do {
	if(js.global.console) {
		console.line_counter = 0;
		if(home) {
			console.home();
			console.status |= CON_CR_CLREOL;
		}
		else if(clear)
			console.clear();
		write(bbs.text(NodeLstHdr));
	}
	var output = presence.nodelist(/* print: */true, active, listself, is_sysop, options);
	for(var i in output)
		writeln(output[i]);
	if(js.global.bbs && active && !output)
		write(bbs.text(NoOtherActiveNodes));
	if(!loop)
		break;
	if(js.global.console) {
		if(console.aborted)
			break;
		console.inkey(K_NONE, delay * 1000);
	} else
		mswait(delay * 1000);
} while(!js.terminated && (!js.global.console || !console.aborted));
