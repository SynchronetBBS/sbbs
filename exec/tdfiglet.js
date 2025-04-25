/*
 * tdfiglet.js - Synchronet JS conversion of tdfiglet.c
 * Based on the C code by Unknown/Modified by The Draw
 * Converted to Synchronet JS by Nelgin
 * @format.tab-size 4, @format.use-tabs true
 */

var tdf = load("tdfonts_lib.js");

// Global options object (using var)
tdf.opt = {
    justify: LEFT_JUSTIFY,
    random: false,
    info: false
};

// Function declarations (Synchronet JS style)
function usage() {
	writeln("usage: [options] input");
	writeln("");
	writeln("    -f [font] Specify font file used.");
	writeln("    -j l|r|c  Justify left, right, or center. Default is left.");
	writeln("    -w n      Set screen width. Default is auto-detect or 80.");
	writeln("    -m n      Set margin/offset (for left or right justification).");
	writeln("    -a        Color sequences: ANSI. Default is Synchronet Ctrl-A.");
	writeln("    -u        Encode charaters as UTF-8. Default is CP437.");
	writeln("    -x n      Index to font within file. Default is 0.");
	writeln("    -i        Print font details.");
	writeln("    -r        Use random font.");
	writeln("    -h        Print usage.");
	writeln("");
	exit(1); // Use Synchronet's exit
}

// Main execution block (equivalent to C main function)

// Access command line arguments via argv global
// argv[0] is typically the script name in Synchronet JS
// Subsequent elements are the arguments

var fontfile = null;

// Corrected argument parsing: Explicitly consume options and their arguments.
var input_string = "";
for(i = 0; i < argv.length; ++i) {
	var arg = argv[i];

	if (arg === "-f" && i + 1 < argv.length) {
		fontfile = argv[i + 1];
		++i;
	} else if (arg === "-j" && i + 1 < argv.length) {
		switch (argv[i + 1]) {
			case "l":
				tdf.opt.justify = LEFT_JUSTIFY;
				break;
			case "r":
				tdf.opt.justify = RIGHT_JUSTIFY;
				break;
			case "c":
				tdf.opt.justify = CENTER_JUSTIFY;
				break;
			default:
				alert("Invalid justification option. Use l, r, or c.");
				exit(1);
		}
		++i;
	} else if (arg === "-m" && i + 1 < argv.length) {
		tdf.opt.margin = parseInt(argv[i + 1], 10);
		++i;
	} else if (arg === "-w" && i + 1 < argv.length) {
		tdf.opt.width = parseInt(argv[i + 1], 10);
		++i;
	} else if (arg === "-x" && i + 1 < argv.length) {
		tdf.opt.index = parseInt(argv[i + 1], 10);
		++i;
	} else if (arg === "-a") {
		tdf.opt.ansi = true;
	} else if (arg === "-u") {
		tdf.opt.utf8 = true;
	} else if (arg === "-i") {
		tdf.opt.info = true;
	} else if (arg === "-r") {
		tdf.opt.random = true;
	} else if (arg === "-h") {
		usage();
	} else {
		input_string += (input_string ? " " : "") + arg;
	}
}

if (!fontfile && !tdf.opt.random)
	usage();

writeln("");

tdf.printstr(input_string, fontfile);
