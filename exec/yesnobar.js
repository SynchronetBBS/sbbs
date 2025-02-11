// JS version of yesnobar.src
require("sbbsdefs.js", "P_NOABORT");

"use strict";

const yes_str = bbs.text(bbs.text.Yes);
const yes_key = yes_str[0];
const no_str = bbs.text(bbs.text.No);
const no_key = no_str[0];

var options = load("modopts.js", "yesnobar");
if(!options)
	options = {};

while(console.question.substring(0, 2) == "\r\n") {
	console.crlf();
	console.question = console.question.substring(2);
}

if(console.question.substring(0, 2) == "\x01\?") {
	console.print(console.question.substring(0, 2));
	console.question = console.question.substring(2);
}

console.putmsg(options.yesno_question || "\x01n\x01b\x01h[\x01c@CHECKMARK@\x01b] \x01y@QUESTION->@? @CLEAR_HOT@", P_NOABORT);
var affirm = true;
while(bbs.online && !js.terminated) {
	var str;
	if(affirm)
		str = format(options.highlight_yes_fmt || "\x01n\x01b\x01h \x01~%s \x014\x01w\x01e[\x01~%s]", no_str, yes_str);
	else
		str = format(options.highlight_no_fmt || "\x01h\x014\x01w\x01e[\x01~%s]\x01n\x01b\x01h \x01~%s ", no_str, yes_str);
	console.print(str);
	var key = console.getkey(0).toUpperCase();
	console.backspace(console.strlen(str));
	if(console.term_supports(USER_MODE7))
		console.print(" ");
	else
		console.print("\x01n\x01h\x01>");
	
	if(console.aborted)
		break;
	if(key == '\r')
		break;
	if(key == yes_key) {
		affirm = true;
		break;
	}
	if(key == no_key) {
		affirm = false;
		break;
	}
	affirm = !affirm;
}

if(!console.aborted)
	console.ungetstr(affirm ? yes_key : no_key);
