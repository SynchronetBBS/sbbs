/*
 * tdfiglet.js - Synchronet JS conversion of tdfiglet.c
 * Based on the C code by Unknown/Modified by The Draw
 * Converted to Synchronet JS by Nelgin
 * @format.tab-size 4, @format.use-tabs true
 */

load("tdfonts_lib.js");

// Global options object (using var)
opt = {
    justify: LEFT_JUSTIFY,
    width: DEFAULT_WIDTH,
    random: false,
    info: false
};

// Function declarations (Synchronet JS style)
function usage() {
    writeln("usage: [options] input");
    writeln("");
    writeln("    -f [font] Specify font file used.");
    writeln("    -j l|r|c  Justify left, right, or center. Default is left.");
    writeln("    -w n      Set screen width. Default is 80.");
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
var input_args = []; // Collect non-option arguments here

// Corrected argument parsing: Explicitly consume options and their arguments.
var args = [];
for (var i = 0; i < argc; i++)
        args.push(argv[i]);
var i = 0;
var input_string = "";
while (i < args.length) {
	var arg = args[i];

	if (arg === "-f" && i + 1 < args.length) {
		fontfile = args[i + 1];
		i += 2;
	} else if (arg === "-j" && i + 1 < args.length) {
		switch (args[i + 1]) {
			case "l":
					opt.justify = LEFT_JUSTIFY;
					break;
			case "r":
					opt.justify = RIGHT_JUSTIFY;
					break;
			case "c":
					opt.justify = CENTER_JUSTIFY;
					break;
			default:
					log("Invalid justification option. Use l, r, or c.");
			exit(1);
		}
		i += 2;
	} else if (arg === "-w" && i + 1 < args.length) {
		opt.width = parseInt(args[i + 1], 10);
		i += 2;
	} else if (arg === "-x" && i + 1 < args.length) {
		opt.index = parseInt(args[i + 1], 10);
		i += 2;
	} else if (arg === "-a") {
		opt.ansi = true;
		++i;
	} else if (arg === "-u") {
		opt.utf8 = true;
		++i;
	} else if (arg === "-i") {
		opt.info = true;
		i += 1;
	} else if (arg === "-r") {
		opt.random = true;
		i += 1;
	} else if (arg === "-h") {
		usage();
	} else {
		input_string += (input_string ? " " : "") + arg;
		i += 1;
	}
}


if (!fontfile && !opt.random)
	usage();

writeln("");

printstr(input_string, fontfile);
