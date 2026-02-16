// User Information Prompts Library
// for Synchronet Terminal Server v3.21+

require("userdefs.js", "USER_AUTOTERM");
require("sbbsdefs.js", "UQ_ALIASES");
require("key_defs.js", "KEY_DEL");
require("gettext.js", "gettext");

"use strict";

const PETSCII_DELETE = CTRL_T;
const PETSCII_UPPERLOWER = 14;

var kmode = (system.newuser_questions & UQ_NOEXASC) | K_EDIT | K_AUTODEL | K_TRIM;
if (!(system.newuser_questions & UQ_NOUPRLWR))
	kmode |= K_UPRLWR;

var operation = this.argc ? argv[0] : "";

function confirm(text, yes)
{
	if (yes)
		return console.yesno(text);
	else
		return !console.noyes(text);
}

function ask_to_cancel(msg)
{
	console.aborted = false;
	if (msg) {
		console.print(msg);
		console.cond_newline();
	}
	if (operation && !console.yesno(gettext("Continue " + operation + "\x01\\")))
		exit(1);
}

function get_lang(user)
{
	if (!user)
		user = js.global.user;
	if (!bbs.text(bbs.text.Language))
		return;
	var lang = bbs.mods.lang || load(bbs.mods.lang = {}, "lang.js");
	while (bbs.online) {
		if (lang.count() && !console.noyes(gettext("Choose an alternate" + " " + bbs.text(bbs.text.Language).toLowerCase() + "\x01\\")))
			lang.select();
		if (console.aborted) {
			ask_to_cancel();
			continue;
		}
		break;
	}
}

function get_autoterm(user)
{
	if (!user)
		user = js.global.user;
	if (!bbs.text(bbs.text.AutoTerminalQ))
		return;
	while (bbs.online) {
		var autoterm = console.yesno(bbs.text(bbs.text.AutoTerminalQ));
		if (console.aborted) {
			ask_to_cancel();
			continue;
		}
		if (autoterm) {
			user.settings |= USER_AUTOTERM;
			user.settings |= console.autoterm;
		} else
			user.settings &= ~USER_AUTOTERM;
		console.term_updated();
		break;
	}
}

function get_ansi(user)
{
	if (!user)
		user = js.global.user;
	if (!bbs.text(bbs.text.AnsiTerminalQ))
		return;
	while (bbs.online) {
		var ansi = confirm(bbs.text(bbs.text.AnsiTerminalQ), user.settings & USER_ANSI);
		if (console.aborted) {
			ask_to_cancel();
			continue;
		}
		if (ansi)
			user.settings |= USER_ANSI;
		else
			user.settings &= ~(USER_ANSI | USER_AUTOTERM);
		console.term_updated();
		break;
	}
}

function get_color(user)
{
	if (!user)
		user = js.global.user;
	if (!bbs.text(bbs.text.ColorTerminalQ))
		return;
	while (bbs.online) {
		var color = confirm(bbs.text(bbs.text.ColorTerminalQ), user.settings & USER_COLOR);
		if (console.aborted) {
			ask_to_cancel();
			continue;
		}
		if (color)
			user.settings |= USER_COLOR;
		else
			user.settings &= ~(USER_COLOR | USER_AUTOTERM);
		console.term_updated();
		break;
	}
}

function get_mouse(user)
{
	if (!user)
		user = js.global.user;
	if (!bbs.text(bbs.text.MouseTerminalQ))
		return;
	while (bbs.online) {
		var mouse = confirm(bbs.text(bbs.text.MouseTerminalQ), user.settings & USER_MOUSE);
		if (console.aborted) {
			ask_to_cancel();
			continue;
		}
		if (mouse)
			user.settings |= USER_MOUSE;
		else
			user.settings &= ~USER_MOUSE;
		console.term_updated();
		break;
	}
}

function get_exascii(user)
{
	if (!user)
		user = js.global.user;
	if (!bbs.text(bbs.text.ExAsciiTerminalQ))
		return;
	while (bbs.online) {
		var exascii = confirm(bbs.text(bbs.text.ExAsciiTerminalQ), !(user.settings & USER_NO_EXASCII));
		if (console.aborted) {
			ask_to_cancel();
			continue;
		}
		if (exascii)
			user.settings &= ~USER_NO_EXASCII;
		else
			user.settings |= USER_NO_EXASCII;
		console.term_updated();
		break;
	}
}

function get_backspace(user)
{
	if (!user)
		user = js.global.user;
	if (!bbs.text(bbs.text.HitYourBackspaceKey))
		return;
	while (!(user.settings & (USER_PETSCII | USER_SWAP_DELETE)) && bbs.online) {
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
	console.term_updated();
}

function get_terminal(user, options)
{
	if (options.lang === true)
		get_lang();
	if (console.autoterm && options.autoterm !== false)
		get_autoterm();
	if (options.backspace !== false)
		get_backspace();
	if (!(user.settings & (USER_AUTOTERM | USER_PETSCII)))
		get_ansi();
	if (user.settings & USER_ANSI) {
		user.rows = 0; // TERM_ROWS_AUTO
		user.cols = 0; // TERM_COLS_AUTO
		if ((system.newuser_questions & UQ_COLORTERM) && !(user.settings & USER_RIP))
			get_color();
		if (options.mouse !== false)
			get_mouse();
	}
	if (user.settings & USER_PETSCII) {
		console.autoterm |= USER_PETSCII;
		console.putbyte(PETSCII_UPPERLOWER);
		console.putmsg(bbs.text(bbs.text.PetTerminalDetected));
	} else if (!(user.settings & USER_UTF8)) {
		if (options.exascii === false)
			user.settings &= ~USER_NO_EXASCII;
		else
			get_exascii();
	}
	console.term_updated();
}

function get_alias(user)
{
	if (!user)
		user = js.global.user;
	var prompt = bbs.text(bbs.text.EnterYourRealName);
	if (system.newuser_questions & UQ_ALIASES)
		prompt = bbs.text(bbs.text.EnterYourAlias);
	while (prompt && bbs.online) {
		console.putmsg(prompt, P_SAVEATR);
		var alias = console.getstr(user.alias, LEN_ALIAS, kmode);
		if (!operation && (alias == user.alias || console.aborted))
			return false;
		if (console.aborted) {
			ask_to_cancel();
			continue;
		}
		if (!system.check_name(alias)
			|| bbs.matchuserdata(U_NAME, alias)
			|| (!(system.newuser_questions & UQ_ALIASES) && !system.check_realname(alias))) {
			bbs.logline(LOG_NOTICE, "N!", format("Invalid or duplicate user real name or alias: '%s'", alias));
			ask_to_cancel(bbs.text(bbs.text.YouCantUseThatName));
			if (!operation)
				return false;
			continue;
		}
		user.alias = alias;
		break;
	}
	return true;
}

function get_name(user)
{
	if (!user)
		user = js.global.user;
	if ((system.newuser_questions & UQ_ALIASES) && (system.newuser_questions & UQ_REALNAME)) {
		while (bbs.online && bbs.text(bbs.text.EnterYourRealName)) {
			console.putmsg(bbs.text(bbs.text.EnterYourRealName), P_SAVEATR);
			var name = console.getstr(user.name, LEN_NAME, kmode);
			if (!operation && (name == user.name || console.aborted))
				return false;
			if (console.aborted) {
				ask_to_cancel();
				continue;
			}
			if (!system.check_name(name, /* unique: */true)
				|| !system.check_realname(name)
				|| ((system.newuser_questions & UQ_DUPREAL)
					&& bbs.matchuserdata(U_NAME, name))) {
				bbs.logline(LOG_NOTICE, "N!", format("Invalid or duplicate user real name: '%s'", name));
				ask_to_cancel(bbs.text(bbs.text.YouCantUseThatName));
				if (!operation)
					return false;
			} else {
				user.name = name;
				break;
			}
		}
	}
	else if (system.newuser_questions & UQ_COMPANY && bbs.text(bbs.text.EnterYourCompany)) {
		console.putmsg(bbs.text(bbs.text.EnterYourCompany), P_SAVEATR);
		user.name = console.getstr(user.name, LEN_NAME, kmode);
		if (console.aborted)
			ask_to_cancel();
	}
}

function get_handle(user)
{
	if (!user)
		user = js.global.user;
	if (!bbs.text(bbs.text.EnterYourHandle))
		return;
	while (bbs.online) {
		console.putmsg(bbs.text(bbs.text.EnterYourHandle), P_SAVEATR);
		var handle = console.getstr(user.handle, LEN_HANDLE, kmode | K_LINE);
		if (!operation && (handle == user.handle || console.aborted))
			return false;
		if (console.aborted) {
			ask_to_cancel();
			continue;
		}
		if (!handle
			|| handle.indexOf(0xff) >= 0
			|| ((system.newuser_questions & UQ_DUPHAND)
				&& bbs.matchuserdata(U_HANDLE, handle))
			|| bbs.trashcan(handle, "name")) {
			bbs.logline(LOG_NOTICE, "N!", format("Invalid or duplicate user handle: '%s'", handle));
			ask_to_cancel(bbs.text(bbs.text.YouCantUseThatName));
		} else {
			user.handle = handle;
			break;
		}
	}
}

function get_address(user)
{
	if (!user)
		user = js.global.user;
	if (!bbs.text(bbs.text.EnterYourAddress))
		return;
	while (bbs.online) {
		console.putmsg(bbs.text(bbs.text.EnterYourAddress), P_SAVEATR);
		user.address = console.getstr(user.address, LEN_ADDRESS, kmode)
		if (!operation && console.aborted)
			return false;
		if (console.aborted) {
			ask_to_cancel();
			continue;
		}
		if (user.address)
			break;
		ask_to_cancel(gettext("Sorry, that address is not acceptable"));
	}
}

function get_location(user)
{
	if (!user)
		user = js.global.user;
	if (!bbs.text(bbs.text.EnterYourCityState))
		return;
	while (bbs.online) {
		console.putmsg(bbs.text(bbs.text.EnterYourCityState), P_SAVEATR);
		var location = console.getstr(user.location, LEN_LOCATION, kmode)
		if (!operation && (location == user.location || console.aborted))
			return false;
		if (console.aborted) {
			ask_to_cancel();
			continue;
		}
		if (!(system.newuser_questions & UQ_NOCOMMAS) && location.indexOf(',') < 1)
			ask_to_cancel(bbs.text(bbs.text.CommaInLocationRequired));
		else if (location) {
			user.location = location;
			break;
		}
	}
}

function get_zipcode(user)
{
	if (!user)
		user = js.global.user;
	if (!bbs.text(bbs.text.EnterYourZipCode))
		return;
	while (bbs.online) {
		console.putmsg(bbs.text(bbs.text.EnterYourZipCode), P_SAVEATR);
		var zipcode = console.getstr(user.zipcode, LEN_ZIPCODE, kmode | K_UPPER);
		if (!operation && (zipcode == user.zipcode || console.aborted))
			return false;
		if (console.aborted) {
			ask_to_cancel();
			continue;
		}
		if (zipcode) {
			user.zipcode = zipcode;
			break;
		}
		ask_to_cancel();
	}
}

function get_phone(user)
{
	if (!user)
		user = js.global.user;
	if (!bbs.text(bbs.text.EnterYourPhoneNumber))
		return;
	var usa = false;
	if (bbs.text(bbs.text.CallingFromNorthAmericaQ))
		usa = console.yesno(bbs.text(bbs.text.CallingFromNorthAmericaQ));
	while (bbs.online) {
		console.putmsg(bbs.text(bbs.text.EnterYourPhoneNumber));
		var phone;
		if (!usa) {
			phone = console.getstr(user.phone, LEN_PHONE, kmode | K_LINE);
			if (!operation && (phone == user.phone || console.aborted))
				return false;
			if (console.aborted) {
				ask_to_cancel();
				continue;
			}
			if (phone.length < 5) {
				ask_to_cancel(gettext("Sorry, that phone number is not acceptable"));
				continue;
			}
		}
		else {
			phone = console.gettemplate(system.phonenumber_template, user.phone, kmode | K_LINE);
			if (!operation && (phone == user.phone || console.aborted))
				return false;
			if (console.aborted) {
				ask_to_cancel();
				continue;
			}
			if (phone.length < system.phonenumber_template.length) {
				ask_to_cancel(gettext("Sorry, that phone number is not acceptable"));
				continue;
			}
		}
		if (!bbs.trashcan(phone, "phone")) {
			user.phone = phone;
			break;
		}
		ask_to_cancel(gettext("Sorry, that phone number is not acceptable"));
	}
}

function get_gender(user)
{
	if (!user)
		user = js.global.user;
	if (!bbs.text(bbs.text.EnterYourGender))
		return;
	while (bbs.atcode("GENDERS") && bbs.online) {
		console.putmsg(bbs.text(bbs.text.EnterYourGender));
		var gender = console.getkeys(bbs.atcode("GENDERS"), 0);
		if (!operation && console.aborted)
			return false;
		if (console.aborted) {
			ask_to_cancel();
			continue;
		}
		if (gender) {
			user.gender = gender;
			break;
		}
		ask_to_cancel();
	}
}

function get_birthdate(user)
{
	if (!user)
		user = js.global.user;
	if (!bbs.text(bbs.text.EnterYourBirthday))
		return;
	while (bbs.online) {
		console.putmsg(format(bbs.text(bbs.text.EnterYourBirthday), system.birthdate_format), P_SAVEATR);
		var birthdate = console.gettemplate(system.birthdate_template, bbs.atcode("BIRTH"), K_EDIT);
		if (console.aborted) {
			ask_to_cancel();
			continue;
		}
		if (birthdate.length != system.birthdate_template.length)
			continue;
		if (typeof system.check_birthdate != "function" || system.check_birthdate(birthdate)) {
			user.birthdate = birthdate;
			break;
		}
		ask_to_cancel(gettext("Sorry, that birthdate is not acceptable"));
	}
}

function get_netmail(user)
{
	if (!user)
		user = js.global.user;
	while (bbs.online && bbs.text(bbs.text.EnterNetMailAddress)) {
		console.putmsg(bbs.text(bbs.text.EnterNetMailAddress));
		var netmail = console.getstr(user.netmail, LEN_NETMAIL, kmode | K_LINE);
		if (!operation && (netmail == user.netmail || console.aborted))
			return false;
		if (console.aborted) {
			ask_to_cancel();
			continue;
		}
		if (!netmail
			|| bbs.trashcan(netmail, "email")
			|| ((system.newuser_questions & UQ_DUPNETMAIL) && bbs.matchuserdata(U_NETMAIL, netmail)))
			ask_to_cancel(bbs.text(bbs.text.YouCantUseThatNetmail));
		else {
			user.netmail = netmail;
			break;
		}
	}
	user.settings &= ~USER_NETMAIL;
	if ((system.settings & SYS_FWDTONET)
		&& (typeof system.check_netmail_addr != "function" || system.check_netmail_addr(user.netmail))
		&& bbs.text(bbs.text.ForwardMailQ) && console.yesno(bbs.text(bbs.text.ForwardMailQ)))
		user.settings |= USER_NETMAIL;
}

function change_password(user)
{
	if (!user)
		user = js.global.user;
	console.putmsg(bbs.text(bbs.text.CurrentPassword));
	console.status |= CON_PASSWORD;
	var str = console.getstr(LEN_PASS * 2, K_UPPER);
	console.status &= ~CON_PASSWORD;
	if (!str)
		return false;
	bbs.user_sync();
	if (str !== user.security.password.toUpperCase()) {
		console.putmsg(bbs.text(bbs.text.WrongPassword));
		return false;
	}
	console.putmsg(format(bbs.text(bbs.text.NewPasswordPromptFmt)
		,system.min_password_length, system.max_password_length));
	str = console.getstr(LEN_PASS, K_UPPER | K_LINE | K_TRIM);
	if (!bbs.good_password(str)) {
		console.newline();
		return false;
	}
	console.putmsg(bbs.text(bbs.text.VerifyPassword));
	console.status |= CON_PASSWORD;
	var pw = console.getstr(LEN_PASS, K_UPPER | K_LINE | K_TRIM);
	console.status &= ~CON_PASSWORD;
	if (str !== pw) {
		console.putmsg(bbs.text(bbs.text.WrongPassword));
		return false;
	}
	user.security.password = str;
	console.putmsg(bbs.text(bbs.text.PasswordChanged));
	log(LOG_NOTICE, "changed password");
	return true;
}

function sig_filename(user)
{
	if (!user)
		user = js.global.user;
	return  system.data_dir + "user/" + format("%04d.sig", user.number);
}

function change_signature(user)
{
	const userSigFilename = sig_filename(user);

	if (!file_exists(userSigFilename)) {
		if (console.yesno(gettext("Create signature")))
			console.editfile(userSigFilename);
	} else {
		console.printfile(userSigFilename);
		if (!console.noyes(gettext("Edit signature")))
			console.editfile(userSigFilename);
		if (!console.noyes(bbs.text(bbs.text.DeleteSignatureQ)))
			file_remove(userSigFilename);
	}
}

function ssh_keys_filename(user)
{
	if (!user)
		user = js.global.user;
	return system.data_dir + "user/" + format("%04d.sshkeys", user.number);
}

function change_ssh_keys(user)
{
	const userSSHKeysFilename = sshkeys_filename(user);

	if (!file_exists(userSSHKeysFilename)) {
		if (!console.noyes(gettext('Create SSH Keys')))
			console.editfile(userSSHKeysFilename);
	} else {
		console.printfile(userSSHKeysFilename);
		if (!console.noyes(gettext('Edit SSH Keys')))
			console.editfile(userSSHKeysFilename);
		else if (!console.noyes(gettext('Delete SSH Keys')))
			file_remove(userSSHKeysFilename);
	}
}

this;