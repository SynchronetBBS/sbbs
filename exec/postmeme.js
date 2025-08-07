// Post a meme
// @format.tab-size 4, @format.use-tabs true

// Supported options (e.g. in ctrl/modopts/postmeme.ini):
//   color (default: 0)
//   border (default: 0)
//   random (default: false)
//   max_length (default: 500)
//   width (default: 39)
//   justify (default: center)

require("sbbsdefs.js", "K_LINEWRAP");
"use strict";

var options = load({}, "modopts.js", "postmeme");
if (!options) options = {};


if(!msg_area.sub[bbs.cursub_code].can_post) {
	alert("Sorry, you can't post on sub-board: " + msg_area.sub[bbs.cursub_code].name);
	exit(0);
}

console.print("\x01N\x01Y\x01HWhat do you want to say?\x01N\r\n");
var text = console.getstr(options.max_length || 500, K_LINEWRAP);
if (!text)
	exit(0);

var msg = load("meme_chooser.js", text, options);
if (!msg)
	exit(0);

var sub = msg_area.sub[bbs.cursub_code];
console.print(format(bbs.text("Posting"), sub.grp_name, sub.description));

var hdr = { auxattr: MSG_FIXED_FORMAT, from_ext: user.number, from:  sub.settings & SUB_NAME ? user.name : user.alias };
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
