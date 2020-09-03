// typehtml.js

// Convert HTML to plain-text with (optional) Synchronet attribute (Ctrl-A) codes

// Planned replacement for exec/typehtml.src (Baja version)

// $Id: typehtml.js,v 1.10 2014/09/17 22:39:16 deuce Exp $

load("sbbsdefs.js");	// USER_HTML
load("html2asc.js");

var f;
var mono=true;
var i;
var buf;

for(i in argv) {
	switch(argv[i].toLowerCase()) {
	case "-mono":
		mono=true;
		break;
	case "-color":
		mono=false;
		break;
	default:
		f = new File(argv[i]);
		break;
	}
}

if(this.f==undefined) {
	print("usage: typehtml [-mono | -color] <filename>");
	exit(1);
}

if(user.settings & USER_HTML)
	console.printfile(f.name,P_HTML);
else {
	if(!f.open("r")) {
		alert("Error " + errno + " opening " + f.name);
		exit(errno);
	}

	buf=f.read(f.length);
	f.close();

	write(html2asc(buf,mono));
}
