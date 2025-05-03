// Post a meme
// @format.tab-size 4, @format.use-tabs true

"use strict";
require("key_defs.js", "KEY_LEFT");
require("sbbsdefs.js", "K_LINEWRAP");

var options = load({}, "modopts.js", "postmeme");
if (!options) options = {};
var lib = load({}, "meme_lib.js");

var maxMsgLen = 500;

function choose(border)
{
	console.mnemonics(format("Style: ~Color, ~@Next@, ~@Previous@, or ~@Quit@ [%u]: ", border % lib.BORDER_COUNT));
	switch(console.getkeys("C" + KEY_LEFT + KEY_RIGHT + "\r" + console.next_key + console.prev_key + console.quit_key)) {
		case console.quit_key:
			return false;
		case '\r':
			return true;
		case 'C':
			return 'C';
		case KEY_UP:
		case KEY_LEFT:
		case console.prev_key:
			return -1;
		default:
			return 1;
	}
}

console.print("\x01N\x01Y\x01HWhat do you want to say?\x01N\r\n");
var text = console.getstr(maxMsgLen, K_LINEWRAP);
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
var border = Number(options.border);
var color = Number(options.color);
if (options.random) {
	border = random(lib.BORDER_COUNT);
	color = random(attr.length);
}
var msg;
while (!js.terminated) {
	msg = lib.generate(attr[color % attr.length], border  % lib.BORDER_COUNT, text);
	console.clear();
	print(msg);
	var ch = choose(border);
	if (ch === false)
		exit(1);
	if (ch === true)
		break;
	if (ch === 'C') {
		++color;
	} else {
		border += ch;
		if (border < 0) {
			console.beep();
			border = 0;
		}
	}
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
