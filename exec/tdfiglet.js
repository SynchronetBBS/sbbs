/*
 * tdfiglet.js - Synchronet JS conversion of tdfiglet.c
 * Based on the C code by Unknown/Modified by The Draw
 * Converted to Synchronet JS by Nelgin
 *
 * Note: This is a best-effort conversion based on the provided C code.
 * Synchronet JS environment differences (like file I/O, binary data handling,
 * and character encoding) may require adjustments.
 * The C code's mmap and directory listing will be replaced with Synchronet JS equivalents.
 * The iconv part for IBM437 to UTF-8 conversion will need a JavaScript equivalent,
 * possibly a lookup table or relying on Synchronet's native encoding handling.
 */

load("tdfonts_lib.js");

// Global options object (using var)
opt = {
    justify: LEFT_JUSTIFY,
    width: DEFAULT_WIDTH,
    color: COLOR_ANSI, // Default to ANSI
    encoding: ENC_ANSI, // Default to ANSI
    random: false,
    info: false,
    index: 0
};

// Character list
var charlist = "!\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~";

// Function declarations (Synchronet JS style)
function usage() {
    writeln("usage: [options] input");
    writeln("");
    writeln("    -f [font] Specify font file used.");
    writeln("    -j l|r|c  Justify left, right, or center. Default is left.");
    writeln("    -w n      Set screen width. Default is 80.");
    writeln("    -c a|m    Color format ANSI or mirc. Default is ANSI.");
    writeln("    -e u|a    Encode as unicode or ASCII. Default is ANSI.");
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
                } else if (arg === "-c" && i + 1 < args.length) {
                        switch (args[i + 1]) {
                                case "a":
                                        opt.color = COLOR_ANSI;
                                        break;
                                case "m":
                                        opt.color = COLOR_MIRC;
                                        break;
                                default:
                                        log("Invalid color option. Use a or m.");
					exit(1);
                        }
                        i += 2;
                } else if (arg === "-e" && i + 1 < args.length) {
                        switch (args[i + 1]) {
                                case "u":
                                        opt.encoding = ENC_UNICODE;
                                        break;
                                case "a":
                                        opt.encoding = ENC_ANSI;
                                        break;
                                default:
                                        log("Invalid encoding option. Use u or a.");
					exit(1);
                        }
                        i += 2;
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


// Handle random font selection
if (!fontfile && opt.random) {
	var fontDir = FONT_DIR;
	var files = directory(fontDir + "/*.tdf"); // Get all .tdf files
	if (files.length > 0) {
		var randomIndex = random((files.length)+1);
		var filename = file_getname(files[randomIndex]);
		fontfile = filename.replace(/\.tdf$/i, "");
	}
}
if (!fontfile)
	usage();

var font = loadfont(fontfile);

writeln("");

printstr(input_string, font);
