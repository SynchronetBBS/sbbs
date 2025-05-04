// Post a meme
// @format.tab-size 4, @format.use-tabs true

// Supported options (e.g. in ctrl/modopts/postmeme.ini):
//   color (default: 0)
//   border (default: 0)
//   random (default: false)
//   max_length (default: 500)
//   width (default: 39)
//   justify (default: center)

"use strict";
require("key_defs.js", "KEY_LEFT");
require("sbbsdefs.js", "K_LINEWRAP");

var options = load({}, "modopts.js", "postmeme");
if (!options) options = {};
var lib = load({}, "meme_lib.js");

function choose(border)
{
	console.mnemonics(format("~Border, ~Color, ~Justify, ~@Quit@, or [Select]: "));
	var ch = console.getkeys("BCJ" + KEY_LEFT + KEY_RIGHT + "\r" + console.next_key + console.prev_key + console.quit_key, lib.BORDER_COUNT);
	if (typeof ch == "number")
		return ch - 1;
	switch (ch) {
		case console.quit_key:
			return false;
		case '\r':
			return true;
		case 'C':
		case 'J':
		case 'B':
			return ch;
		case KEY_UP:
		case KEY_LEFT:
		case console.prev_key:
			return console.prev_key;
		default:
			return console.next_key;
	}
}

console.print("\x01N\x01Y\x01HWhat do you want to say?\x01N\r\n");
var text = console.getstr(options.max_length || 500, K_LINEWRAP);
if (!text)
	exit(0);
var attr = [
	"\x01H\x01W\x011",
	"\x01H\x01W\x012",
	"\x01H\x01W\x013",
	"\x01H\x01W\x014",
	"\x01H\x01W\x015",
	"\x01H\x01W\x016",
	"\x01N\x01K\x017",
];
var justify = options.justify || 0;
var border = options.border || 0;
var color = options.color || 0;
if (options.random) {
	border = random(lib.BORDER_COUNT);
	color = random(attr.length);
}
var msg;
while (!js.terminated) {
	msg = lib.generate(options.width || 39, attr[color % attr.length], border  % lib.BORDER_COUNT, text, justify % lib.JUSTIFY_COUNT);
	console.clear();
	console.attributes = WHITE | HIGH;
	console.print(format("Meme \x01N\x01C(border \x01H%u \x01N\x01Cof \x01H%u\x01N\x01C, color \x01H%u\x01N\x01C of \x01H%u\x01N\x01C):"
		, (border % lib.BORDER_COUNT) + 1, lib.BORDER_COUNT
		, (color % attr.length) + 1, attr.length));
	console.newline(2);
	print(msg);
	var ch = choose(border);
	if (ch === false)
		exit(1);
	if (ch === true)
		break;
	if (typeof ch == "number")
		border = ch;
	else if (ch === 'C')
		++color;
	else if (ch === 'J')
		++justify;
	else if (ch === 'B')
		++border;
	else if (ch == console.next_key && border < lib.BORDER_COUNT - 1)
		++border;
	else if (ch == console.prev_key && border > 0)
		--border;
	else
		console.beep();
}

if (!msg)
	exit(0);

var sub = msg_area.sub[bbs.cursub_code];
console.print(format(bbs.text("Posting"), sub.grp_name, sub.description));

var hdr = { from_ext: user.number, from:  sub.settings & SUB_NAME ? user.name : user.alias };
console.print(bbs.text("PostTo"));
hdr.to = console.getstr("All", LEN_ALIAS, K_EDIT | K_LINE | K_AUTODEL);
if (console.aborted || !hdr.to)
	exit(0);
console.print(bbs.text("SubjectPrompt"));
hdr.subject = console.getstr(text, LEN_TITLE, K_EDIT | K_LINE)
if (console.aborted || !hdr.subject)
	exit(0);

console.putmsg(bbs.text("Saving"));
var msgbase = new MsgBase(sub.code);
if (!msgbase.open()) {
	alert("Error opening msgbase: " + msgbase.last_error);
	exit(1);
}

if (!msgbase.save_msg(hdr, msg)) {
	alert("Error saving message: " + msgbase.last_error);
	exit(1);
}
console.print(format(bbs.text("SavedNBytes"), msg.length, msg.split("\n").length));
console.print(format(bbs.text("Posted"), sub.grp_name, sub.description));
