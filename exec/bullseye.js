// bullseye.js

// Bulletins written in Baja by Rob Swindell
// Translated to JS by Stehen Hurd
// Refactored by Rob Swindell

// @format.tab-size 4, @format.use-tabs true

load("sbbsdefs.js");

"use strict";

// Load the configuration file

var i=0;
var b=0;
var html=user.settings&USER_HTML;

if(html) {
	if(!file_exists(system.text_dir+"bullseye.html"))
		html=0;
}

if(!html) {
	writeln("");
	writeln("Synchronet BullsEye! Version 3.00 by Rob Swindell");
}

console.line_counter=0;
var file=new File(system.text_dir+"bullseye.cfg");
if(!file.open("r", true)) {
	writeln("");
	writeln("!ERROR "+file.error+" opening "+ file.name);
	exit(1);
}
file.readln(); // First line is not used (mode?)
bull = file.readAll();
file.close();

bull = bull.filter(function(str) { return truncsp(str) && file_getcase(str); });

if(bull.length < 1) {
	alert("No bulletins listed in " + file.name);
	exit(0);
}

// Display menu, list bulletins, display prompt, etc.

while(bbs.online && !js.terminated) {
	if(bbs.menu("../bullseye")) {
		console.mnemonics("\r\nEnter number of bulletin or [~Quit]: ");
		b = console.getnum(bull.length);
	} else {
		for(i = 0; i < bull.length; ++i)
			console.uselect(i + 1, "Bulletin", file_getname(bull[i]));
		b = console.uselect();
	}
	if(b < 1)
		break;
	if(b > bull.length) {
		alert("Invalid bulletin number: "+b);
	} else {
		console.clear(7);
		var fname = truncsp(bull[b - 1]);
		var ext = file_getext(fname);
		if(ext == ".*")
			bbs.menu(fname.slice(0, -2));
		else if(fname.search(/\.htm/)!=-1)
			load(new Object, "typehtml.js", "-color", fname);
		else
			load(new Object, "typeasc.js", fname, "BullsEye Bulletin #"+b);
		log("Node "+bbs.node_num+" "+user.alias+" viewed bulletin #"+i+": "+fname);
		console.aborted=false;
	}
}
