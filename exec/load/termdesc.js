// Terminal type/parameters (set or detected) descriptive strings

// Common function arguments:
//      verbose: true if you want verbose output (default to false)
//      usr: Pass a User object (defaults to current 'user')

// e.g. load("termdesc.js".type()
//      load("termdesc.js".charset()
//      load("termdesc.js".rows()
//      load("termdesc.js".columns()
//      load("termdesc.js".type(true)
//      load("termdesc.js".type(true, User(4))

require("text.js", "TerminalAutoDetect");
require("userdefs.js", "USER_PETSCII");

"use strict";

function charset(term)
{
	if(term === undefined)
		term = console.term_supports();
	if(term & USER_PETSCII)
		return "CBM-ASCII";
	if(term & USER_MODE7)
		return "MODE7";
	if(term & USER_UTF8)
		return "UTF-8";
	if(term & USER_NO_EXASCII)
		return "US-ASCII";
	return "CP437";
}

function rows(verbose, usr)
{
	if(usr === undefined) // Use current user by default
		usr = user;
	var rows = usr.screen_rows || console.screen_rows;
	if(verbose !== true)
		return String(rows);
	return format("%s%d %s", usr.screen_rows ? "":bbs.text(TerminalAutoDetect), rows, bbs.text(TerminalRows));
}

function columns(verbose, usr)
{
	if(usr === undefined) // Use current user by default
		usr = user;
	var cols = usr.screen_columns || console.screen_columns;
	if(verbose !== true)
		return String(cols);
	return format("%s%d %s", usr.screen_columns ? "":bbs.text(TerminalAutoDetect), cols, bbs.text(TerminalColumns));
}

function type(verbose, usr)
{
	var term;

	if(usr === undefined) { // Use current user by default
		usr = user;
		term = console.term_supports(); // user current-detected terminal params
	} else
		term = usr.settings;

	var type = "DUMB";
	if(term & USER_PETSCII)
		type = "PETSCII";
	if(term & USER_MODE7)
		type = "MODE7";
	if(term & USER_RIP)
		type = "RIP";
	if(term & USER_ANSI)
		type = "ANSI";
	if(verbose !== true)
		return type;

	// Verbose
	if(term & USER_PETSCII)
		return ((usr.settings & USER_AUTOTERM) ? bbs.text(TerminalAutoDetect) : "") + "CBM/PETSCII";
	if(term & USER_MODE7)
		return ((usr.settings & USER_AUTOTERM) ? bbs.text(TerminalAutoDetect) : "") + "MODE7";
	return format("%s%s / %s %s%s%s"
		,(usr.settings & USER_AUTOTERM) ? bbs.text(TerminalAutoDetect) : ""
		,this.charset(term)
		,type
		,(term & USER_COLOR) ? (term & USER_ICE_COLOR ? bbs.text(TerminalIceColor) : bbs.text(TerminalColor)) : bbs.text(TerminalMonochrome)
		,(term & USER_MOUSE) ? bbs.text(TerminalMouse) : ""
		,(term & USER_SWAP_DELETE) ? "DEL=BS" : "").trimRight();
}

this;
