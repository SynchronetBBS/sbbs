/*
 * tdfiglet.js - Synchronet JS conversion of tdfiglet.c
 * Based on the C code by Unknown/Modified by The Draw
 * Converted to Synchronet JS by Nelgin
 *
 * More information at:
 *   https://wiki.synchro.net/custom:thedrawfonts
 *   https://wiki.synchro.net/module:tdfiglet
 *
 * @format.tab-size 4, @format.use-tabs true
 */

var tdf = load("tdfonts_lib.js");

tdf.opt = {};

function usage() {
	writeln("usage: tdfiglet [options] input");
	writeln("");
	writeln("    -f [font]  Specify font file to use (full filename, include .tdf)");
	writeln("    -d [dir]   Specify font directory to search (optional)");
	writeln("               If using -d do not specify a path with -f");
	writeln("    -j l|r|c   Justify left, right, or center (default: left)");
	writeln("    -w n       Set screen width  (default: auto-detect or 80)");
	writeln("    -m n       Set margin/offset (for left or right justification)");
	writeln("    -a         Enable ANSI sequences (default: Synchronet Ctrl-A)");
	writeln("    -u         Encode characters as UTF-8 (default: CP437)");
	writeln("    -x n       Number of font within file (default: 1) or 0 for random");
	writeln("    -n         No blank line between wrapped output lines");
	writeln("    -W         Always word-wrap the output");
	writeln("    -i         Print font details");
	writeln("    -l         Loop through the available fonts");
	writeln("    -p         Pause between fonts");
	writeln("    -r         Use random font and/or index (if not specified with -x)");
	writeln("    -R n       Use random font and auto-retry specified number of times");
	writeln("               upon exception. (default: 3)");
	writeln("    -h         Usage");
	writeln("");
}

var fontfile = null;
var loopfonts = false;
var input_string = "";

tdf.opt.index = 1;
tdf.opt.retryno = 3;
tdf.opt.pause = false;

for(i = 0; i < argv.length; ++i) {
	var arg = argv[i];

	if (arg === "-f" && i + 1 < argv.length) {
		fontfile = argv[i + 1];
		++i;
	} else if (arg === "-d" && i + 1 < argv.length) {
		tdf.opt.fontpath = argv[i + 1];
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
	} else if (arg === "-l") {
		loopfonts = true;
	} else if (arg === "-p") {
		tdf.opt.pause = true;
	} else if (arg === "-r") {
		tdf.opt.random = true;
	} else if (arg === "-R") {
		tdf.opt.random = true;
		tdf.opt.retry = true;
				if (i + 1 < argv.length && !isNaN(parseInt(argv[i + 1], 10))) {
					tdf.opt.retryno = parseInt(argv[i + 1], 10);
					++i;
				}
	} else if (arg === "-n") {
		tdf.opt.blankline = false;
	} else if (arg === "-W") {
		tdf.opt.wrap = true;
	} else if (arg === "-h") {
		usage();
	} else {
		input_string += (input_string ? " " : "") + arg;
	}
}

// We may not catch very collsion here but we can get a few
// of the most likely.

if (tdf.opt.fontpath && fontfile) {
    if (fontfile.indexOf('/') !== -1) {
	writeln("Error: -f with a path and -d specified is not allowed.");
	exit(1);
    }
} else if (fontfile && opt.random) {
	writeln("Error: -f cannot be used with -r");
	exit(1);
} else if ((fontfile || tdf.opt.random) && loopfonts) {
	writeln("Error: -f or -r cannot be used with -l");
	exit(1);
}


if (!input_string)
	usage();

if (loopfonts)
	tdf.display_all(input_string, tdf.opt.fontpath);
else
	tdf.printstr(input_string, fontfile);

