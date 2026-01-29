// Just a command-line interface to console.printfile()

load("sbbsdefs.js");

var fname;
var mode = P_NONE;
var columns;

for(var i in argv) {
	if(fname==undefined) {
		fname = file_getcase(argv[i]);
	} else {
		var intval = parseInt(argv[i], 10);
		if(isNaN(intval))
			mode |= eval(argv[i]);
		else
			columns = intval;
	}
}

if(fname==undefined) {
	print("usage: printfile <filename> [P_mode][...] [columns]");
	exit(1);
}

console.printfile(fname, mode, columns);
