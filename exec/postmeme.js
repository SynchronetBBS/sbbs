// Post a meme
// @format.tab-size 4, @format.use-tabs true

"use strict";
require("key_defs.js", "KEY_LEFT");
require("sbbsdefs.js", "K_LINEWRAP");

// Note: BORDER_COUNT should *not* equal length of attr array
var BORDER_NONE = 0;
var BORDER_SINGLE = 1;
var BORDER_MIXED1 = 2;
var BORDER_MIXED2 = 3;
var BORDER_DOUBLE = 4;
var BORDER_ORNATE1 = 5;
var BORDER_ORNATE2 = 6;
var BORDER_ORNATE3 = 7;
var BORDER_COUNT = 8;

var maxMsgLen = 500;

// We don't have String.repeat() in ES5
function repeat(ch, length)
{
	var result = "";
	for (var i = 0; i < length; ++i)
		result += ch;
	return result;
}

function top_border(border, width)
{
	var str;
	switch (border) {
		case BORDER_NONE:
			str = format("%*s", width, "");
			break;
		case BORDER_SINGLE:
			str = format("\xDA%s\xBF", repeat("\xC4", width - 2));
			break;
		case BORDER_MIXED1:
			str = format("\xD6%s\xB7", repeat("\xC4", width - 2));
			break;
		case BORDER_MIXED2:
			str = format("\xD5%s\xB8", repeat("\xCD", width - 2));
			break;
		case BORDER_DOUBLE:
			str = format("\xC9%s\xBB", repeat("\xCD", width - 2));
			break;
		case BORDER_ORNATE1:
			str = format("\xC4\xC5\xC4%s\xC4\xC5\xC4", repeat(" ", width - 6));
			break;
		case BORDER_ORNATE2:
			str = format("\xDA\xC4%s\xC4\xBF", repeat(" ", width - 4));
			break;
		case BORDER_ORNATE3:
			str = format("\xC9\xC4%s\xC4\xBB", repeat(" ", width - 4));
			break;
	}
	return str + "\x01N\r\n";
}

function mid_border(border, width, margin, line)
{
	var str;
	switch (border) {
		case BORDER_NONE:
		case BORDER_ORNATE1:
		case BORDER_ORNATE2:
			str = format("%*s%-*s", margin, "", width - margin, line);
			break;
		case BORDER_SINGLE:
		case BORDER_MIXED2:
		case BORDER_ORNATE3:
			str = format("\xB3%*s%-*s\xB3", margin - 1, "", width - (margin + 1), line);
			break;
		case BORDER_DOUBLE:
		case BORDER_MIXED1:
			str = format("\xBA%*s%-*s\xBA", margin - 1, "", width - (margin + 1), line);
			break;
	}
	return str + "\x01N\r\n";
}

function bottom_border(border, width)
{
	var str;
	switch (border) {
		case BORDER_NONE:
			str = format("%*s", width, "");
			break;
		case BORDER_SINGLE:
			str = format("\xC0%s\xD9", repeat("\xC4", width - 2));
			break;
		case BORDER_MIXED1:
			str = format("\xD3%s\xBD", repeat("\xC4", width - 2));
			break;
		case BORDER_MIXED2:
			str = format("\xD4%s\xBE", repeat("\xCD", width - 2));
			break;
		case BORDER_DOUBLE:
			str = format("\xC8%s\xBC", repeat("\xCD", width - 2));
			break;
		case BORDER_ORNATE1:
			str = format("\xC4\xC5\xC4%s\xC4\xC5\xC4", repeat(" ", width - 6));
			break;
		case BORDER_ORNATE2:
			str = format("\xC0\xC4%s\xC4\xD9", repeat(" ", width - 4));
			break;
		case BORDER_ORNATE3:
			str = format("\xC8\xC4%s\xC4\xBC", repeat(" ", width - 4));
			break;
	}
	return str + "\x01N\r\n";
}

function generate(attr, border, text)
{
	var width = 39;
	var msg = attr + top_border(border, width);
	var array = word_wrap(text, width - 4).split("\n");
	for (var i in array) {
		var line = truncsp(array[i]);
		if (!line && i >= array.length - 1)
			break;
		var margin = Math.floor((width - line.length) / 2);
		msg += attr + mid_border(border, width, margin, line);
	}
	msg += attr + bottom_border(border, width);
	return msg;
}

function choose(style)
{
	console.mnemonics(format("Style: ~@Next@, ~@Previous@, or ~@Quit@ [%u]: ", style));
	switch(console.getkeys(KEY_LEFT + KEY_RIGHT + "\r" + console.next_key + console.prev_key + console.quit_key)) {
		case console.quit_key:
			console.aborted = true;
			return -1;
		case '\r':
			return 0;
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
var style = 0;
var msg;
while (!js.terminated) {
	msg = generate(attr[style % attr.length], style % BORDER_COUNT, text);
	console.clear();
	print(msg);
	var ch = choose(style);
	if (console.aborted)
		exit(1);
	if (ch == 0)
		break;
	style += ch;
	if (style < 0) {
		console.beep();
		style = 0;
	}
}

if (!msg)
	exit(0);

var sub = msg_area.sub[bbs.cursub_code];
console.print(format(bbs.text("Posting"), sub.grp_name, sub.description));
	
var hdr = { from_ext: user.number, from:  sub.settings & SUB_NAME ? user.name : user.alias };
console.print(bbs.text("PostTo"));
hdr.to = console.getstr("All", LEN_ALIAS, K_EDIT | K_LINE | K_AUTODEL);
if (!hdr.to)
	exit(0);
console.print(bbs.text("SubjectPrompt"));
hdr.subject = console.getstr(text, LEN_TITLE, K_EDIT | K_LINE)
if (!hdr.subject)
	exit(0);

console.print(bbs.text("Saving"));
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