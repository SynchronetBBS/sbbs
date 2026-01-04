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

var options = load("modopts.js", "newuser_prompts");
if(!options)
	options = {};

"use strict";

const PETSCII_DELETE = CTRL_T;
const PETSCII_UPPERLOWER = 14;

var kmode = (system.newuser_questions & UQ_NOEXASC) | K_EDIT | K_AUTODEL | K_TRIM;
if (!(system.newuser_questions & UQ_NOUPRLWR))
	kmode |= K_UPRLWR;

function ask_to_cancel(msg)
{
	console.aborted = false;
	if (msg) {
		console.print(msg);
		console.cond_newline();
	}
	if (!console.yesno(gettext("Continue new user registration")))
		exit(1);
}

while(bbs.online && !js.terminated) {

	if (options.lang === true) {
		var lang = load({}, "lang.js");
		if (lang.count() && !console.noyes(gettext("Choose an alternate" + " " + bbs.text(bbs.text.Language))))
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
				ask_to_cancel(format(bbs.text(bbs.text.InvalidBackspaceKeyFmt), key, key));
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

	var prompt = bbs.text(bbs.text.EnterYourRealName);
	if (system.newuser_questions & UQ_ALIASES)
		prompt = bbs.text(bbs.text.EnterYourAlias);
	while (prompt && bbs.online) {
		console.putmsg(prompt, P_SAVEATR);
		user.alias = console.getstr(user.alias, LEN_ALIAS, kmode);
		if (!system.check_name(user.alias)
			|| bbs.matchuserdata(U_NAME, user.alias)
			|| (!(system.newuser_questions & UQ_ALIASES) && !system.check_realname(user.alias))) {
			bbs.logline(LOG_NOTICE, "N!", format("Invalid or duplicate user real name or alias: '%s'", user.alias));
			ask_to_cancel(bbs.text(bbs.text.YouCantUseThatName));
			continue;
		}
		break;
	}
	if ((system.newuser_questions & UQ_ALIASES) && (system.newuser_questions & UQ_REALNAME)) {
		while (bbs.online && bbs.text(bbs.text.EnterYourRealName)) {
			console.putmsg(bbs.text(bbs.text.EnterYourRealName), P_SAVEATR);
			user.name = console.getstr(user.name, LEN_NAME, kmode);
			if (!system.check_name(user.name, /* unique: */true)
				|| !system.check_realname(user.name)
				|| ((system.newuser_questions & UQ_DUPREAL)
					&& bbs.matchuserdata(U_NAME, user.name))) {
				bbs.logline(LOG_NOTICE, "N!", format("Invalid or duplicate user real name: '%s'", user.name));
				ask_to_cancel(bbs.text(bbs.text.YouCantUseThatName));
			} else
				break;
		}
	}
	else if (system.newuser_questions & UQ_COMPANY && bbs.text(bbs.text.EnterYourCompany)) {
		console.putmsg(bbs.text(bbs.text.EnterYourCompany), P_SAVEATR);
		user.name = console.getstr(user.name, LEN_NAME, kmode);
	}
	if (!bbs.online)
		exit(1);
	if (!user.alias) {
		log(LOG_ERR, "New user alias was blank");
		exit(1);
	}
	if (!user.handle)
		user.handle = user.alias;
	user.handle = user.handle.trimRight();
	while ((system.newuser_questions & UQ_HANDLE) && bbs.online && bbs.text(bbs.text.EnterYourHandle)) {
		console.putmsg(bbs.text(bbs.text.EnterYourHandle), P_SAVEATR);
		user.handle = console.getstr(user.handle, LEN_HANDLE
					, K_LINE | K_EDIT | K_AUTODEL | K_TRIM | (system.newuser_questions & UQ_NOEXASC));
		if (!user.handle
			|| user.handle.indexOf(0xff) >= 0
			|| ((system.newuser_questions & UQ_DUPHAND)
				&& bbs.matchuserdata(U_HANDLE, user.handle))
			|| bbs.trashcan(user.handle, "name")) {
			bbs.logline(LOG_NOTICE, "N!", format("Invalid or duplicate user handle: '%s'", user.name));
			ask_to_cancel(bbs.text(bbs.text.YouCantUseThatName));
		} else
			break;
	}
	if (system.newuser_questions & UQ_ADDRESS) {
		while (bbs.online && bbs.text(bbs.text.EnterYourAddress)) {       /* Get address and zip code */
			console.putmsg(bbs.text(bbs.text.EnterYourAddress), P_SAVEATR);
			user.address = console.getstr(user.address, LEN_ADDRESS, kmode)
			if (user.address)
				break;
			ask_to_cancel(gettext("Sorry, that address is not acceptable"));
		}
	}
	while ((system.newuser_questions & UQ_LOCATION) && bbs.online && bbs.text(bbs.text.EnterYourCityState)) {
		console.putmsg(bbs.text(bbs.text.EnterYourCityState), P_SAVEATR);
		user.location = console.getstr(user.location, LEN_LOCATION, kmode)
		if (!(system.newuser_questions & UQ_NOCOMMAS) && user.location.indexOf(',') < 1) {
			ask_to_cancel(bbs.text(bbs.text.CommaInLocationRequired));
			user.location = "";
		} else if(user.location)
			break;
	}
	if (system.newuser_questions & UQ_ADDRESS) {
		while (bbs.online && bbs.text(bbs.text.EnterYourZipCode)) {
			console.putmsg(bbs.text(bbs.text.EnterYourZipCode), P_SAVEATR);
			user.zipcode = console.getstr(user.zipcode, LEN_ZIPCODE
							, K_UPPER | (system.newuser_questions & UQ_NOEXASC) | K_EDIT | K_AUTODEL | K_TRIM);
			if (user.zipcode)
				break;
			ask_to_cancel();
		}
	}
	if ((system.newuser_questions & UQ_PHONE) && bbs.text(bbs.text.EnterYourPhoneNumber)) {
		var usa = false;
		if (bbs.text(bbs.text.CallingFromNorthAmericaQ))
			usa = console.yesno(bbs.text(bbs.text.CallingFromNorthAmericaQ));
		while (bbs.online) {
			console.putmsg(bbs.text(bbs.text.EnterYourPhoneNumber));
			if (!usa) {
				user.phone = console.getstr(user.phone, LEN_PHONE
								, K_UPPER | K_LINE | (system.newuser_questions & UQ_NOEXASC) | K_EDIT | K_AUTODEL | K_TRIM);
				if (user.phone.length < 5) {
					ask_to_cancel(gettext("Sorry, that phone number is not acceptable"));
					continue;
				}
			}
			else {
				user.phone = console.gettemplate(system.phonenumber_template, user.phone
								, K_LINE | (system.newuser_questions & UQ_NOEXASC) | K_EDIT);
				if (user.phone.length < system.phonenumber_template.length) {
					ask_to_cancel(gettext("Sorry, that phone number is not acceptable"));
					continue;
				}
			}
			if (!bbs.trashcan(user.phone, "phone"))
				break;
			ask_to_cancel(gettext("Sorry, that phone number is not acceptable"));
		}
	}
	while ((system.newuser_questions & UQ_SEX) && bbs.text(bbs.text.EnterYourGender) && bbs.atcode("GENDERS") && bbs.online) {
		console.putmsg(bbs.text(bbs.text.EnterYourGender));
		var gender = console.getkeys(bbs.atcode("GENDERS"), 0);
		if (gender) {
			user.gender = gender;
			break;
		}
		ask_to_cancel();
	}
	while ((system.newuser_questions & UQ_BIRTH) && bbs.online && bbs.text(bbs.text.EnterYourBirthday)) {
		console.putmsg(format(bbs.text(bbs.text.EnterYourBirthday), system.birthdate_format), P_SAVEATR);
		var birthdate = console.gettemplate(system.birthdate_template, bbs.atcode("BIRTH"), K_EDIT);
		if (system.check_birthdate(birthdate)) {
			user.birthdate = birthdate;
			break;
		}
		ask_to_cancel(gettext("Sorry, that birthdate is not acceptable"));
	}
	while (!(system.newuser_questions & UQ_NONETMAIL) && bbs.online && bbs.text(bbs.text.EnterNetMailAddress)) {
		console.putmsg(bbs.text(bbs.text.EnterNetMailAddress));
		user.netmail = console.getstr(user.netmail, LEN_NETMAIL, K_EDIT | K_AUTODEL | K_LINE | K_TRIM);
		if (!user.netmail
			|| bbs.trashcan(user.netmail, "email")
			|| ((system.newuser_questions & UQ_DUPNETMAIL) && bbs.matchuserdata(U_NETMAIL, user.netmail)))
			ask_to_cancel(bbs.text(bbs.text.YouCantUseThatNetmail));
		else
			break;
	}
	user.settings &= ~USER_NETMAIL;
	if ((system.settings & SYS_FWDTONET) && system.check_netmail_addr(user.netmail)
		&& bbs.text(bbs.text.ForwardMailQ) && console.yesno(bbs.text(bbs.text.ForwardMailQ)))
		user.settings |= USER_NETMAIL;

	if (!bbs.text(bbs.text.UserInfoCorrectQ) || console.yesno(bbs.text(bbs.text.UserInfoCorrectQ)))
		break;
	console.print(gettext("Restarting new user registration") + "\r\n");
}