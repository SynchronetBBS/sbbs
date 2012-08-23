// Lightbar message area selector for Synchronet 3.16+
// echicken -at- bbs.electronicchicken.com

load(js.exec_dir + "msglib.js");

var lbg = BG_CYAN;		// Lightbar background
var lfg = WHITE;		// Foreground colour of highlighted text
var nfg = LIGHTGRAY;	// Foreground colour of non-highlighted text
var fbg = BG_BLUE;		// Title, Header, Help frame background colour
var xfg = LIGHTCYAN;	// Subtree expansion indicator colour
var tfg = LIGHTCYAN;	// Uh, that line beside subtree items
var hfg = "\1h\1w"; 	// Heading text (CTRL-A format, for now)
var sffg = "\1h\1c";	// Heading sub-field text (CTRL-A format, for now)
var mfg = "\1n\1w";		// Message text colour (CTRL-A format, for now)

if(argc < 4) {
	x = 1;
	y = 1;
	width = console.screen_columns;
	height = console.screen_rows;
} else {
	x = argv[0];
	y = argv[1];
	width = argv[2];
	height = argv[3];
}

messageAreaSelector(x, y, width, height);