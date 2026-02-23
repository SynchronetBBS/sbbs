/*
 * tdfiglet.js - Synchronet JS conversion of tdfiglet.c
 * Based on the C code by Unknown/Modified by The Draw
 * Converted to Synchronet JS by Nelgin
 * @format.tab-size 4, @format.use-tabs true
 */

var tdf = load("tdfonts_lib.js");

tdf.opt = {};

function usage() {
	writeln("usage: tdfiglet [options] input");
	writeln("");
	writeln("    -f [font] Specify font file to use");
	writeln("    -d <dir>  Specify directory to find font files (default: " + system.ctrl_dir + "tdfonts)");
	writeln("    -j l|r|c  Justify left, right, or center (default: left)");
	writeln("    -w n      Set screen width  (default: auto-detect or 80)");
	writeln("    -m n      Set margin/offset (for left or right justification)");
	writeln("    -a        Enable ANSI sequences (default: Synchronet Ctrl-A)");
	writeln("    -u        Encode characters as UTF-8 (default: CP437)");
	writeln("    -x n      Index to font within file (default: 0)");
	writeln("    -n        No blank line between wrapped output lines");
	writeln("    -W        Always word-wrap the output");
	writeln("    -i        Print font details");
	writeln("    -l        Loop through the available fonts");
	writeln("    -p        Pause between fonts");
	writeln("    -r        Use random font and/or index (if not specified with -x)");
	writeln("    -R        Use random font and auto-retry upon exception");
	writeln("    -h        Print usage");
	writeln("");
	exit(1);
}

var fontfile = null;
var loopfonts = false;
var pause = false;
var input_string = "";
for(i = 0; i < argv.length; ++i) {
	var arg = argv[i];

	if (arg === "-f" && i + 1 < argv.length) {
		fontfile = argv[i + 1];
		++i;
	} else if (arg === "-d" && i + 1 < argv.length) {
		tdf.opt.fontdir = argv[i + 1];
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
		pause = true;
	} else if (arg === "-r") {
		tdf.opt.random = true;
	} else if (arg === "-R") {
		tdf.opt.random = true;
		tdf.opt.retry = true;
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

if (!fontfile && !tdf.opt.random && !loopfonts)
	usage();

if (!input_string)
	usage();

if (loopfonts) {
	var list = tdf.getlist();
	for (var i in list) {
		if (pause && i > 0)
			prompt("Hit enter");
		try {
			tdf.printstr(input_string, list[i]);
		} catch(e) {
			if (tdf.opt.info)
				print("exception: " + e);
		}
	}
} else
	tdf.printstr(input_string, fontfile);
