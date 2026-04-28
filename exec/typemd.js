// typemd.js

// Convert Markdown to plain-text with (optional) Synchronet attribute (Ctrl-A) codes

// Similar in spirit to typehtml.js

load("sbbsdefs.js");
load("md2asc.js");

var f;
var mono = true;
var i;
var buf;

for(i in argv) {
	switch(argv[i].toLowerCase()) {
	case "-mono":
		mono = true;
		break;
	case "-color":
		mono = false;
		break;
	default:
		f = new File(argv[i]);
		break;
	}
}

if(this.f == undefined) {
	print("usage: typemd [-mono | -color] <filename>");
	exit(1);
}

if(!f.open("r")) {
	alert("Error " + errno + " opening " + f.name);
	exit(errno);
}

buf = f.read(f.length);
f.close();

write(md2asc(buf, mono));
