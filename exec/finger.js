// $Id: finger.js,v 1.8 2019/01/12 01:47:34 rswindell Exp $

// A simple finger/systat (who) client suitable for running via the BBS or JSexec

"use strict";

var lib = load({}, "finger_lib.js");
var dest;
var use_udp = false;
var protocol = "finger";

var i;
for(i = 0; i < argc; i++) {
	if(argv[i] == '-udp')
		use_udp = true;
	else if(argv[i] == '-s')
		protocol = "systat";
	else if(argv[i].indexOf('@')!=-1)
		dest = argv[i];
	else {
		alert("Unsupported option: " + argv[i]);
		exit();
	}
}

function finger(dest, protocol, use_udp)
{
	if(!dest && (dest = prompt("User (user@hostname)"))==null)
		return;

	writeln();
	var hp;
	if((hp = dest.indexOf('@')) == -1) {
		dest += "@" + system.host_name;
		hp = dest.indexOf('@')
	}

	var host = dest.slice(hp + 1);
	var result = lib.request(host, dest.slice(0, hp), protocol, use_udp);
	if(typeof result != 'object')
		alert(result);
	else
		for(var i in result)
			writeln(result[i]);
}

finger(dest, protocol, use_udp);