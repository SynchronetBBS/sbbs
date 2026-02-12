// bullseye.js

// Bulletins written in Baja by Rob Swindell
// Translated to JS by Stehen Hurd
// Refactored by Rob Swindell

// @format.tab-size 4, @format.use-tabs true

require("sbbsdefs.js", "P_NOERROR");
require("gettext.js", 'gettext');

"use strict";

// Load the configuration file

var i=0;
var b=0;

writeln("");
writeln("Synchronet BullsEye! Version 3.00 by Rob Swindell");

console.line_counter=0;
var file=new File(system.text_dir+"bullseye.cfg");
if(!file.open("r", true)) {
	writeln("");
	writeln("!ERROR "+file.error+" opening "+ file.name);
	exit(1);
}
var p_mode = file.readln();
if (p_mode)
	p_mode = eval(p_mode);
bull = file.readAll();
file.close();

bull = bull.filter(function(str) { return truncsp(str) });

if(bull.length < 1) {
	alert("No bulletins listed in " + file.name);
	exit(0);
}

// Display menu, list bulletins, display prompt, etc.

while(bbs.online && !js.terminated) {
	if(bbs.menu("../bullseye", P_NOERROR)) {
	console.newline();
	console.mnemonics(gettext("Enter number of bulletin or [~Quit]: ", "choose_bulletin"));
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
		var success = false;
		if(ext == ".*")
			success = bbs.menu(fname.slice(0, -2), p_mode);
		else if(fname.search(/\.htm/)!=-1)
			success = load(new Object, "typehtml.js", "-color", fname);		
		else
			success = console.printfile(fname, p_mode);
		if(success)
			log("viewed bulletin #" + b + ": "+fname);
		else
			log(LOG_WARNING, "Failed to view bulletin #" + b + " (" + fname + " is missing?)");
		console.aborted=false;
	}
}
