// New User Registration Prompts module
// for Synchronet Terminal Server v3.21+

// At minimum, this script must set user.alias (to a unique, legal user name)

// Options (e.g. in ctrl/modopts/newuser_prompts.ini) with defaults:
//   lang=false
//   autoterm=false
//   backspace=true
//   mouse=true
//   exascii=true

require("userdefs.js", "USER_AUTOTERM");
require("sbbsdefs.js", "UQ_ALIASES");
require("key_defs.js", "KEY_DEL");
require("gettext.js", "gettext");

var prompts = bbs.mods.prompts || load(bbs.mods.prompts = {}, "user_info_prompts.js", "new user registration");

var options = load("modopts.js", "newuser_prompts");
if(!options)
	options = {};

"use strict";

const PETSCII_DELETE = CTRL_T;
const PETSCII_UPPERLOWER = 14;

while(bbs.online && !js.terminated) {

	if (options.lang === true) {
		var lang = load({}, "lang.js");
		if (lang.count() && !console.noyes(gettext("Choose an alternate" + " " + bbs.text(bbs.text.Language).toLowerCase() + "\x01\\")))
			lang.select();
	}
	if ((console.autoterm && options.autoterm !== true)
		|| (bbs.text(bbs.text.AutoTerminalQ) && console.yesno(bbs.text(bbs.text.AutoTerminalQ)))) {
		user.settings |= USER_AUTOTERM;
		user.settings |= console.autoterm;
	} else
		user.settings &= ~USER_AUTOTERM;

	if (options.backspace !== false) {
		while (bbs.text(bbs.text.HitYourBackspaceKey) && !(user.settings & (USER_PETSCII | USER_SWAP_DELETE)) && bbs.online) {
			console.putmsg(bbs.text(bbs.text.HitYourBackspaceKey));
			var key = console.getkey(K_CTRLKEYS);
			console.putmsg(format(bbs.text(bbs.text.CharacterReceivedFmt), key.charCodeAt(0), key.charCodeAt(0)));
			if (key == '\b')
				break;
			if (key == KEY_DEL) {
				if (!bbs.text(bbs.text.SwapDeleteKeyQ) || console.yesno(bbs.text(bbs.text.SwapDeleteKeyQ)))
					user.settings |= USER_SWAP_DELETE;
			}
			else if (key == PETSCII_DELETE)
				user.settings |= (USER_PETSCII | USER_COLOR);
			else if (bbs.online) {
				bbs.logline(LOG_NOTICE, "N!", format("Unsupported backspace key received: %02Xh", key));
				prompts.ask_to_cancel(format(bbs.text(bbs.text.InvalidBackspaceKeyFmt), key, key));
			}
		}
	}

	if (!(user.settings & (USER_AUTOTERM | USER_PETSCII))) {
		if (bbs.text(bbs.text.AnsiTerminalQ) && console.yesno(bbs.text(bbs.text.AnsiTerminalQ)))
			user.settings |= USER_ANSI;
		else
			user.settings &= ~USER_ANSI;
	}

	if (user.settings & USER_ANSI) {
		user.rows = 0; // TERM_ROWS_AUTO
		user.cols = 0; // TERM_COLS_AUTO
		if (!(system.newuser_questions & UQ_COLORTERM) || (user.settings & USER_RIP) || console.yesno(bbs.text(bbs.text.ColorTerminalQ)))
			user.settings |= USER_COLOR;
		else
			user.settings &= ~(USER_COLOR | USER_AUTOTERM);
		if (options.mouse !== false && bbs.text(bbs.text.MouseTerminalQ) && console.yesno(bbs.text(bbs.text.MouseTerminalQ)))
			user.settings |= USER_MOUSE;
		else
			user.settings &= ~USER_MOUSE;
	}

	if (user.settings & USER_PETSCII) {
		console.autoterm |= USER_PETSCII;
		console.putbyte(PETSCII_UPPERLOWER);
		console.putmsg(bbs.text(bbs.text.PetTerminalDetected));
	} else if (!(user.settings & USER_UTF8)) {
		if (options.exascii !== false && !console.yesno(bbs.text(bbs.text.ExAsciiTerminalQ)))
			user.settings |= USER_NO_EXASCII;
		else
			user.settings &= ~USER_NO_EXASCII;
	}
	console.term_updated();

	prompts.get_alias();
	prompts.get_name();
	if (!bbs.online)
		exit(1);
	if (!user.alias) {
		log(LOG_ERR, "New user alias was blank");
		exit(1);
	}
	if (!user.handle)
		user.handle = user.alias;
	user.handle = user.handle.trimRight();
	if(system.newuser_questions & UQ_HANDLE)
		prompts.get_handle();
	if(system.newuser_questions & UQ_ADDRESS)
		prompts.get_address();
	if(system.newuser_questions & (UQ_ADDRESS | UQ_LOCATION))
		prompts.get_location();	
	if(system.newuser_questions & UQ_ADDRESS)
		prompts.get_zipcode();
	if(system.newuser_questions & UQ_PHONE)
		prompts.get_phone();
	if(system.newuser_questions & UQ_SEX)
		prompts.get_gender();
	if(system.newuser_questions & UQ_BIRTH)
		prompts.get_birthdate();
	if(!(system.newuser_questions & UQ_NONETMAIL))
		prompts.get_netmail();

	if (!bbs.text(bbs.text.UserInfoCorrectQ) || console.yesno(bbs.text(bbs.text.UserInfoCorrectQ)))
		break;
	console.print(gettext("Restarting new user registration") + "\r\n");
}