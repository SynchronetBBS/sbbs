// User terminal settings menu
// for Synchronet v3.21+

// Run from the Terminal Server via js.exec()

"use strict";

require("sbbsdefs.js", 'P_SAVEATR');
require("userdefs.js", 'USER_ANSI');
require("gettext.js", 'gettext');
var prompts = bbs.mods.prompts || load(bbs.mods.prompts = {}, "user_info_prompts.js");
var termdesc = bbs.mods.termdesc || load(bbs.mods.termdesc = {}, "termdesc.js");
var options = load("modopts.js", "user_terminal", {});
if (!options.option_fmt)
	options.option_fmt = "\x01v[\x01V%c\x01v] %s";
if (!options.option_with_val_fmt)
	options.option_with_val_fmt = "\x01v[\x01V%c\x01v] %s: \x01V%s";

prompts.operation = "";

function on_or_off(on)
{
	return bbs.text(on ? bbs.text.On : bbs.text.Off);
}

function confirm(text, yes)
{
	if (yes)
		return console.yesno(text) && !console.aborted;
	else
		return !console.noyes(text);
}

function term_supports(x)
{
	if (user.number == js.global.user.number)
		return console.term_supports(x || 0);
	return x ? (user.settings & x) : user.settings;
}

while(bbs.online && !js.terminated) {
	var keys = 'Q\r';
	console.aborted = false;
	if (user.number === js.global.user.number) {
		js.global.user.settings = user.settings;
		bbs.load_user_text();
		console.term_updated();
	}
	console.clear();
	console.print((options.header_prefix || "\x01v")
		+ (options.header || (gettext("Terminal Settings") + " " + gettext("for")))
		+ format(options.user_fmt || " \x01V%s #%u\x01v:", user.alias, user.number));
	console.newline(2);
	console.print(format(options.option_with_val_fmt, 'T', gettext("Type")
				,termdesc.type(/* verbosity: */1, (bbs.sys_status & SS_USERON)
					&& (user.number == js.global.user.number && !user.is_guest && false) ? undefined : user)));
	keys += 'T';
	console.add_hotspot('T');
	console.newline();
	
	console.print(format(options.option_with_val_fmt, 'W', options.text_width || gettext("Width"), termdesc.columns(true, user)));
	keys += 'W';
	console.add_hotspot('W');
	console.newline();
	
	console.print(format(options.option_with_val_fmt, 'H', options.text_height || gettext("Height"), termdesc.rows(true, user)));
	keys += 'H';
	console.add_hotspot('H');
	console.newline();

	if (term_supports(USER_ANSI)) {
		console.print(format(options.option_with_val_fmt, 'C', options.text_color || gettext("Color")
			, (term_supports(USER_ICE_COLOR) ? bbs.text(bbs.text.TerminalIceColor) : "") + on_or_off(term_supports(USER_COLOR))));
		keys += 'C';
		console.add_hotspot('C');
		console.newline();
	}
	if (term_supports(USER_ANSI) && bbs.text(bbs.text.MouseTerminalQ).length) {
		console.print(format(options.option_with_val_fmt, 'M', options.text_mouse || gettext("Mouse"), on_or_off(term_supports(USER_MOUSE))));
		keys += 'M';
		console.add_hotspot('M');
		console.newline();
	}
	if (!term_supports(USER_PETSCII)) {
		console.print(format(options.option_with_val_fmt, 'B', options.text_backspace || gettext("Backspace")
			, (user.settings & USER_SWAP_DELETE) ? options.text_del || gettext("DEL") : options.text_ctrl_h || gettext("Ctrl-H")));
		keys += 'B';
		console.add_hotspot('B');
		console.newline();
	}
	if ((user.settings & (USER_AUTOTERM | USER_PETSCII)) != (USER_AUTOTERM | USER_PETSCII)) {
		console.print(format(options.option_fmt, 'E', options.text_charset || gettext("Character Set/Encoding")));
		keys += 'E';
		console.add_hotspot('E');
		console.newline();
	}
	console.newline();	
	console.putmsg(options.prompt || "\x01v@Which@ or [\x01VQ\x01v] to @Quit@: \x01V", P_SAVEATR);
	console.add_hotspot('Q');
	
	switch(console.getkeys(keys, 0, K_UPPER)) {
		case 'T':
			console.newline();
			if (confirm(bbs.text(bbs.text.AutoTerminalQ), user.settings & USER_AUTOTERM)) {
				user.settings |= USER_AUTOTERM;
				user.settings &= ~(USER_ANSI | USER_RIP | USER_WIP | USER_HTML | USER_PETSCII | USER_UTF8);
				if (user.number === js.global.user.number)
					user.settings |= console.autoterm;
			}
			else if (!console.aborted)
				user.settings &= ~USER_AUTOTERM;
			if (console.aborted)
				break;
			if (!(user.settings & USER_AUTOTERM)) {
				if (confirm(bbs.text(bbs.text.AnsiTerminalQ), user.settings & USER_ANSI)) {
					user.settings |= USER_ANSI;
					user.settings &= ~USER_PETSCII;
				} else
					user.settings &= ~USER_ANSI;
			}
			if (console.aborted)
				break;
			if (!(user.settings & USER_AUTOTERM)
				&& (term_supports() & (USER_ANSI|USER_NO_EXASCII)) == USER_ANSI) {
				if (!console.noyes(bbs.text(bbs.text.RipTerminalQ)) && !console.aborted)
					user.settings |= USER_RIP;
				else
					user.settings &= ~USER_RIP;
			}
			break;
		case 'E':
			console.newline();
			prompts.get_charset(user);
			break;
		case 'B':
			console.newline();
			prompts.get_backspace(user);
			break;
		case 'C':
			console.newline();
			prompts.get_color(user);
			break;
		case 'W':
			console.newline();
			prompts.get_columns(user);
			break;
		case 'H':
			console.newline();
			prompts.get_rows(user);
			break;
		case 'M':
			console.newline();
			prompts.get_mouse(user);
			break;
		case 'Q':
		case '\r':
			console.clear_hotspots();
			exit();
	}
}
