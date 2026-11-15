// Lightbar @-codes: @BAR:cmd@ ... @BAR@, the @[text]cmd@ shorthand and
// @BARATTR@. These only record menu items for a script to navigate (see
// load/menubar.js), so the test displays a line of items and checks what
// console.bars, console.bar_attr and console.bar_hot_attr report.
// Each line it displays is erased again.

require("sbbsdefs.js", "P_SAVEATR");
require("cga_defs.js", "BG_RED");

var failures = [];

function check(what, got, expect)
{
	if (got !== expect)
		failures.push(format("%s = '%s', expected '%s'", what, got, expect));
}

function check_bar(n, expect)
{
	var bar = console.bars[n];
	var prop;

	if (bar === undefined) {
		failures.push("console.bars[" + n + "] is missing");
		return;
	}
	for (prop in expect)
		check("console.bars[" + n + "]." + prop, bar[prop], expect[prop]);
}

// Display 'text' at the start of the line below the test runner's, run 'checks'
// with the 1-based row it was displayed on, then erase the line for the next
var started = false;

function display(text, checks)
{
	console.clear_hotspots();	// forgets the lightbar items and colors too
	if (!started) {
		console.crlf();
		started = true;
	}
	var row = console.current_row + 1;
	console.putmsg(text);
	try {
		checks(row);
	} finally {
		console.print("\r");
		console.cleartoeol();
	}
}

// The begin/end pair: position, width and the source text between the codes
display("ab@BAR:M@ Multi-player @BAR@ @BAR:Q@Quit@BAR@", function(row) {
	check("console.bars.length", console.bars.length, 2);
	check_bar(0, { cmd: "M", x: 3, y: row, width: 14, attr: LIGHTGRAY, text: " Multi-player " });
	check_bar(1, { cmd: "Q", x: 18, y: row, width: 4, text: "Quit" });
	check("console.bar_attr", console.bar_attr, 0);
	check("console.bar_hot_attr", console.bar_hot_attr, 0);
});

// Ctrl-A codes inside the pair are kept in the text but take no width, and
// the item's attribute is the one in effect where it begins
display("\x01h\x01y@BAR:H@\x01cH\x01yelp@BAR@", function(row) {
	check_bar(0, { cmd: "H", x: 1, width: 4, attr: YELLOW, text: "\x01cH\x01yelp" });
});

// A command may be more than one character, and may use C-escapes
display("@BAR:DL@Download@BAR@@BAR:\\r@Enter@BAR@", function(row) {
	check_bar(0, { cmd: "DL", x: 1, width: 8 });
	check_bar(1, { cmd: "\r", x: 9, width: 5 });
});

// Rows of a multi-row item share a command; each is reported separately
display("@BAR:A@one@BAR@@BAR:A@two@BAR@", function(row) {
	check("console.bars.length", console.bars.length, 2);
	check_bar(0, { cmd: "A", text: "one" });
	check_bar(1, { cmd: "A", text: "two" });
});

// The single-code shorthand displays underscores as spaces
display("@[_Single_player_]S@@[Quit]Q@", function(row) {
	check("console.bars.length", console.bars.length, 2);
	check_bar(0, { cmd: "S", x: 1, y: row, width: 15, attr: LIGHTGRAY, text: " Single player " });
	check_bar(1, { cmd: "Q", x: 16, width: 4, text: "Quit" });
	check("console.current_column", console.current_column, 19);
});

// A shorthand code with no command is not a lightbar item
display("@[text]@", function(row) {
	check("console.bars.length", console.bars.length, 0);
});

// @BARATTR:attr@, and its optional second (command key) attribute
display("@BARATTR:WH1@", function(row) {
	check("console.bar_attr", console.bar_attr, WHITE | BG_RED);
	check("console.bar_hot_attr", console.bar_hot_attr, 0);
	check("console.current_column", console.current_column, 0);
});
display("@BARATTR:WH1:R@", function(row) {
	check("console.bar_attr", console.bar_attr, WHITE | BG_RED);
	check("console.bar_hot_attr", console.bar_hot_attr, RED);
});
display("@BARATTR:WH1@@BARATTR::G@", function(row) {
	check("console.bar_attr", console.bar_attr, WHITE | BG_RED);
	check("console.bar_hot_attr", console.bar_hot_attr, GREEN);
});

// A bare @BARATTR@ takes the attribute in effect where it is placed
display("\x01h\x01y\x014@BARATTR@\x01n", function(row) {
	check("console.bar_attr", console.bar_attr, YELLOW | BG_BLUE);
});

// Clearing the hot-spots (as clearing the screen does) forgets all of it
display("@BARATTR:WH1:R@@BAR:M@Menu@BAR@", function(row) {
	check("console.bars.length", console.bars.length, 1);
	console.clear_hotspots();
	check("console.bars.length", console.bars.length, 0);
	check("console.bar_attr", console.bar_attr, 0);
	check("console.bar_hot_attr", console.bar_hot_attr, 0);
});

if (failures.length)
	throw new Error(failures.length + " failed:\r\n" + failures.join("\r\n"));
