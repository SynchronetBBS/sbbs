// New User Registration Prompts module

// At minimum, this script must set user.alias (to a unique, legal user name)

require("text.js", "AutoTerminalQ");
require("userdefs.js", "USER_AUTOTERM");
require("sbbsdefs.js", "UQ_ALIASES");
require("key_desf.js", "KEY_DEL");

const PETSCII_DELETE = CTRL_T;

var kmode = (system.newuser_questions & UQ_NOEXASC) | K_EDIT | K_AUTODEL | K_TRIM;
if (!(system.newuser_questions & UQ_NOUPRLWR))
	kmode |= K_UPRLWR;

while(bbs.online && !js.terminated) {
	if (console.autoterm || (bbs.text(AutoTerminalQ) && console.yesno(bbs.text(AutoTerminalQ)))) {
		user.settings |= USER_AUTOTERM;
		user.settings |= console.autoterm;
	} else
		user.settings &= ~USER_AUTOTERM;

	while (bbs.text(HitYourBackspaceKey) && !(user.settings & (USER_PETSCII | USER_SWAP_DELETE)) && bbs.online) {
		console.putmsg(bbs.text(HitYourBackspaceKey));
		var key = console.getkey(K_CTRLKEYS);
		console.putmsg(format(bbs.text(CharacterReceivedFmt), key.charCodeAt(0), key.charCodeAt(0)));
		if (key == '\b')
			break;
		if (key == KEY_DEL) {
			if (!bbs.text(SwapDeleteKeyQ) || console.yesno(bbs.text(SwapDeleteKeyQ)))
				user.settings |= USER_SWAP_DELETE;
		}
		else if (key == PETSCII_DELETE)
			user.settings |= (USER_PETSCII | USER_COLOR);
		else if (bbs.online) {
			bbs.logline(LOG_NOTICE, "N!", format("Unsupported backspace key received: %02Xh", key));
			console.putmsg(format(bbs.text(InvalidBackspaceKeyFmt), key, key));
			if (bbs.text(ContinueQ) && !console.yesno(bbs.text(ContinueQ)))
				exit(1);
		}
	}

	if (!(user.settings & (USER_AUTOTERM | USER_PETSCII))) {
		if (bbs.text(AnsiTerminalQ) && console.yesno(bbs.text(AnsiTerminalQ)))
			user.settings |= USER_ANSI;
		else
			user.settings &= ~USER_ANSI;
	}

	if (user.settings & USER_ANSI) {
		user.rows = 0; // TERM_ROWS_AUTO
		user.cols = 0; // TERM_COLS_AUTO
		if (!(system.newuser_questions & UQ_COLORTERM) || user.settings & (USER_RIP) || console.yesno(bbs.text(ColorTerminalQ)))
			user.settings |= USER_COLOR;
		else
			user.settings &= ~USER_COLOR;
		if (bbs.text(MouseTerminalQ) && console.yesno(bbs.text(MouseTerminalQ)))
			user.settings |= USER_MOUSE;
		else
			user.settings &= ~USER_MOUSE;
	}

	if (user.settings & USER_PETSCII) {
		console.autoterm |= USER_PETSCII;
		term_out(PETSCII_UPPERLOWER);
		console.putmsg(bbs.text(PetTerminalDetected));
	} else if (!(user.settings & USER_UTF8)) {
		if (!console.yesno(bbs.text(ExAsciiTerminalQ)))
			user.settings |= USER_NO_EXASCII;
		else
			user.settings &= ~USER_NO_EXASCII;
	}
	bbs.sys_status |= SS_NEWUSER;
	console.term_updated();

	var prompt = bbs.text(EnterYourRealName);
	if (system.newuser_questions & UQ_ALIASES)
		prompt = bbs.text(EnterYourAlias);
	while (prompt && bbs.online) {
		console.putmsg(prompt, P_SAVEATR);
		user.alias = console.getstr(user.alias, LEN_ALIAS, kmode);
		if (!system.check_name(user.alias)
			|| system.matchuserdata(U_NAME, user.alias)
			|| (!(system.newuser_questions & UQ_ALIASES) && !system.check_realname(user.alias))) {
			bbs.logline(LOG_NOTICE, "N!", format("Invalid or duplicate user real name or alias: '%s'", user.alias));
			console.putmsg(bbs.text(YouCantUseThatName));
			if (bbs.text(ContinueQ) && !console.yesno(bbs.text(ContinueQ)))
				exit(1);
			continue;
		}
		break;
	}
	if ((system.newuser_questions & UQ_ALIASES) && (system.newuser_questions & UQ_REALNAME)) {
		while (bbs.online && bbs.text(EnterYourRealName)) {
			console.putmsg(bbs.text(EnterYourRealName), P_SAVEATR);
			user.name = console.getstr(user.name, LEN_NAME, kmode);
			if (!system.check_name(user.name, /* unique: */true)
				|| !system.check_realname(user.name)
				|| ((system.newuser_questions & UQ_DUPREAL)
					&& system.matchuserdata(U_NAME, user.name))) {
				bbs.logline(LOG_NOTICE, "N!", format("Invalid or duplicate user real name: '%s'", user.name));
				console.putmsg(bbs.text(YouCantUseThatName));
			} else
				break;
			if (bbs.text(ContinueQ) && !console.yesno(bbs.text(ContinueQ)))
				exit(1);
		}
	}
	else if (system.newuser_questions & UQ_COMPANY && bbs.text(EnterYourCompany)) {
		console.putmsg(bbs.text(EnterYourCompany), P_SAVEATR);
		user.name = console.getstr(user.name, LEN_NAME, kmode);
	}
	if (!user.alias) {
		log(LOG_ERR, "New user alias was blank");
		exit(1);
	}
	if (!user.handle)
		user.handle = user.alias;
	while ((system.newuser_questions & UQ_HANDLE) && bbs.online && bbs.text(EnterYourHandle)) {
		console.putmsg(bbs.text(EnterYourHandle), P_SAVEATR);
		user.handle = console.getstr(user.handle, LEN_HANDLE
					, K_LINE | K_EDIT | K_AUTODEL | K_TRIM | (system.newuser_questions & UQ_NOEXASC));
		if (!user.handle
			|| user.handle.indexOf(0xff) >= 0
			|| ((system.newuser_questions & UQ_DUPHAND)
				&& system.matchuserdata(U_HANDLE, user.handle))
			|| bbs.trashcan(user.handle, "name")) {
			bbs.logline(LOG_NOTICE, "N!", format("Invalid or duplicate user handle: '%s'", user.name));
			console.putmsg(bbs.text(YouCantUseThatName));
		} else
			break;
		if (bbs.text(ContinueQ) && !console.yesno(bbs.text(ContinueQ)))
			exit(1);
	}
	if (system.newuser_questions & UQ_ADDRESS)
		while (bbs.online && bbs.text(EnterYourAddress)) {       /* Get address and zip code */
			console.putmsg(bbs.text(EnterYourAddress), P_SAVEATR);
			user.address = console.getstr(user.address, LEN_ADDRESS, kmode)
			if (user.address)
				break;
		}
	while ((system.newuser_questions & UQ_LOCATION) && bbs.online && bbs.text(EnterYourCityState)) {
		console.putmsg(bbs.text(EnterYourCityState), P_SAVEATR);
		user.location = console.getstr(user.location, LEN_LOCATION, kmode)
		if (!user.location)
			continue;
		if (!(system.newuser_questions & UQ_NOCOMMAS) && strchr(user.location, ',') == NULL) {
			console.putmsg(bbs.text(CommaInLocationRequired));
			user.location = "";
		} else
			break;
	}
	if (system.newuser_questions & UQ_ADDRESS)
		while (bbs.online && bbs.text(EnterYourZipCode)) {
			console.putmsg(bbs.text(EnterYourZipCode), P_SAVEATR);
			user.zipcode = console.getstr(user.zipcode, LEN_ZIPCODE
							, K_UPPER | (system.newuser_questions & UQ_NOEXASC) | K_EDIT | K_AUTODEL | K_TRIM);
			if (user.zipcode)
				break;
		}
	if ((system.newuser_questions & UQ_PHONE) && bbs.text(EnterYourPhoneNumber)) {
		var usa = false;
		if (bbs.text(CallingFromNorthAmericaQ))
			usa = console.yesno(bbs.text(CallingFromNorthAmericaQ));
		while (bbs.online) {
			console.putmsg(bbs.text(EnterYourPhoneNumber));
			if (!usa) {
				user.phone = console.getstr(user.phone, LEN_PHONE
								, K_UPPER | K_LINE | (system.newuser_questions & UQ_NOEXASC) | K_EDIT | K_AUTODEL | K_TRIM);
				if (user.phone.length < 5)
					continue;
			}
			else {
				user.phone = console.gettemplate(system.phonenumber_template, user.phone
								, K_LINE | (system.newuser_questions & UQ_NOEXASC) | K_EDIT);
				if (user.phone.length < system.phonenumber_template.length)
					continue;
			}
			if (!bbs.trashcan(user.phone, "phone"))
				break;
		}
	}
	while ((system.newuser_questions & UQ_SEX) && bbs.text(EnterYourGender) && bbs.atcode("GENDERS") && bbs.online) {
		console.putmsg(bbs.text(EnterYourGender));
		var gender = console.getkeys(bbs.atcode("GENDERS"), 0);
		if (gender) {
			user.sex = gender;
			break;
		}
	}
	while ((system.newuser_questions & UQ_BIRTH) && bbs.online && bbs.text(EnterYourBirthday)) {
		console.putmsg(format(bbs.text(EnterYourBirthday), system.birthdate_format), P_SAVEATR);
		user.birthdate = console.gettemplate(system.birthdate_template, bbs.atcode("BIRTH"), K_EDIT);
		if (user.age >= 1 && user.age <= 200) { // TODO: Configurable min/max user age
			break;
		}
	}
	while (!(system.newuser_questions & UQ_NONETMAIL) && bbs.online && bbs.text(EnterNetMailAddress)) {
		console.putmsg(bbs.text(EnterNetMailAddress));
		user.netmail = console.getstr(user.netmail, LEN_NETMAIL, K_EDIT | K_AUTODEL | K_LINE | K_TRIM);
		if (!user.netmail
			|| bbs.trashcan(user.netmail, "email")
			|| ((system.newuser_questions & UQ_DUPNETMAIL) && system.matchuserdata(U_NETMAIL, user.netmail)))
			console.putmsg(bbs.text(YouCantUseThatNetmail));
		else
			break;
	}
	user.settings &= ~USER_NETMAIL;
	if ((system.settings & SYS_FWDTONET) && system.check_netmail_addr(user.netmail)
		&& bbs.text(ForwardMailQ) && console.yesno(bbs.text(ForwardMailQ)))
		user.settings |= USER_NETMAIL;

	if (!bbs.text(UserInfoCorrectQ) || console.yesno(bbs.text(UserInfoCorrectQ)))
		break;
}