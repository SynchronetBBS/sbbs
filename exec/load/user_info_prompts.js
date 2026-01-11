// User Information Prompts Library
// for Synchronet Terminal Server v3.21+

require("userdefs.js", "USER_AUTOTERM");
require("sbbsdefs.js", "UQ_ALIASES");
require("key_defs.js", "KEY_DEL");
require("gettext.js", "gettext");

"use strict";

var kmode = (system.newuser_questions & UQ_NOEXASC) | K_EDIT | K_AUTODEL | K_TRIM;
if (!(system.newuser_questions & UQ_NOUPRLWR))
	kmode |= K_UPRLWR;

var operation = this.argc ? argv[0] : "";

function ask_to_cancel(msg)
{
	console.aborted = false;
	if (msg) {
		console.print(msg);
		console.cond_newline();
	}
	if (!console.yesno(gettext("Continue " + operation)))
		exit(1);
}

function get_alias(user)
{
	if(!user)
		user = js.global.user;
	var prompt = bbs.text(bbs.text.EnterYourRealName);
	if (system.newuser_questions & UQ_ALIASES)
		prompt = bbs.text(bbs.text.EnterYourAlias);
	while (prompt && bbs.online) {
		console.putmsg(prompt, P_SAVEATR);
		var alias = console.getstr(user.alias, LEN_ALIAS, kmode);
		if (!system.check_name(alias)
			|| bbs.matchuserdata(U_NAME, alias)
			|| (!(system.newuser_questions & UQ_ALIASES) && !system.check_realname(alias))) {
			bbs.logline(LOG_NOTICE, "N!", format("Invalid or duplicate user real name or alias: '%s'", alias));
			ask_to_cancel(bbs.text(bbs.text.YouCantUseThatName));
			continue;
		}
		user.alias = alias;
		break;
	}
}

function get_name(user)
{
	if(!user)
		user = js.global.user;
	if ((system.newuser_questions & UQ_ALIASES) && (system.newuser_questions & UQ_REALNAME)) {
		while (bbs.online && bbs.text(bbs.text.EnterYourRealName)) {
			console.putmsg(bbs.text(bbs.text.EnterYourRealName), P_SAVEATR);
			var name = console.getstr(user.name, LEN_NAME, kmode);
			if (!system.check_name(name, /* unique: */true)
				|| !system.check_realname(name)
				|| ((system.newuser_questions & UQ_DUPREAL)
					&& bbs.matchuserdata(U_NAME, name))) {
				bbs.logline(LOG_NOTICE, "N!", format("Invalid or duplicate user real name: '%s'", name));
				ask_to_cancel(bbs.text(bbs.text.YouCantUseThatName));
			} else {
				user.name = name;
				break;
			}
		}
	}
	else if (system.newuser_questions & UQ_COMPANY && bbs.text(bbs.text.EnterYourCompany)) {
		console.putmsg(bbs.text(bbs.text.EnterYourCompany), P_SAVEATR);
		user.name = console.getstr(user.name, LEN_NAME, kmode);
	}
}

function get_handle(user)
{
	if(!user)
		user = js.global.user;
	while (bbs.online && bbs.text(bbs.text.EnterYourHandle)) {
		console.putmsg(bbs.text(bbs.text.EnterYourHandle), P_SAVEATR);
		var handle = console.getstr(user.handle, LEN_HANDLE
					, K_LINE | K_EDIT | K_AUTODEL | K_TRIM | (system.newuser_questions & UQ_NOEXASC));
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
	if(!user)
		user = js.global.user;
	while (bbs.online && bbs.text(bbs.text.EnterYourAddress)) {
		console.putmsg(bbs.text(bbs.text.EnterYourAddress), P_SAVEATR);
		user.address = console.getstr(user.address, LEN_ADDRESS, kmode)
		if (user.address)
			break;
		ask_to_cancel(gettext("Sorry, that address is not acceptable"));
	}
}

function get_location(user)
{
	if(!user)
		user = js.global.user;
	while (bbs.online && bbs.text(bbs.text.EnterYourCityState)) {
		console.putmsg(bbs.text(bbs.text.EnterYourCityState), P_SAVEATR);
		var location = console.getstr(user.location, LEN_LOCATION, kmode)
		if (!(system.newuser_questions & UQ_NOCOMMAS) && location.indexOf(',') < 1)
			ask_to_cancel(bbs.text(bbs.text.CommaInLocationRequired));
		else if(location) {
			user.location = location;
			break;
		}
	}
}

function get_zipcode(user)
{
	if(!user)
		user = js.global.user;
	while (bbs.online && bbs.text(bbs.text.EnterYourZipCode)) {
		console.putmsg(bbs.text(bbs.text.EnterYourZipCode), P_SAVEATR);
		var zipcode = console.getstr(user.zipcode, LEN_ZIPCODE
						, K_UPPER | (system.newuser_questions & UQ_NOEXASC) | K_EDIT | K_AUTODEL | K_TRIM);
		if (zipcode) {
			user.zipcode = zipcode;
			break;
		}
		ask_to_cancel();
	}
}

function get_phone(user)
{
	if(!user)
		user = js.global.user;
	if (bbs.text(bbs.text.EnterYourPhoneNumber)) {
		var usa = false;
		if (bbs.text(bbs.text.CallingFromNorthAmericaQ))
			usa = console.yesno(bbs.text(bbs.text.CallingFromNorthAmericaQ));
		while (bbs.online) {
			console.putmsg(bbs.text(bbs.text.EnterYourPhoneNumber));
			var phone;
			if (!usa) {
				phone = console.getstr(user.phone, LEN_PHONE
								, K_UPPER | K_LINE | (system.newuser_questions & UQ_NOEXASC) | K_EDIT | K_AUTODEL | K_TRIM);
				if (phone.length < 5) {
					ask_to_cancel(gettext("Sorry, that phone number is not acceptable"));
					continue;
				}
			}
			else {
				phone = console.gettemplate(system.phonenumber_template, user.phone
								, K_LINE | (system.newuser_questions & UQ_NOEXASC) | K_EDIT);
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
}

function get_gender(user)
{
	if(!user)
		user = js.global.user;
	while (bbs.text(bbs.text.EnterYourGender) && bbs.atcode("GENDERS") && bbs.online) {
		console.putmsg(bbs.text(bbs.text.EnterYourGender));
		var gender = console.getkeys(bbs.atcode("GENDERS"), 0);
		if (gender) {
			user.gender = gender;
			break;
		}
		ask_to_cancel();
	}
}

function get_birthdate(user)
{
	if(!user)
		user = js.global.user;
	while (bbs.online && bbs.text(bbs.text.EnterYourBirthday)) {
		console.putmsg(format(bbs.text(bbs.text.EnterYourBirthday), system.birthdate_format), P_SAVEATR);
		var birthdate = console.gettemplate(system.birthdate_template, bbs.atcode("BIRTH"), K_EDIT);
		if (system.check_birthdate(birthdate)) {
			user.birthdate = birthdate;
			break;
		}
		ask_to_cancel(gettext("Sorry, that birthdate is not acceptable"));
	}
}

function get_netmail(user)
{
	if(!user)
		user = js.global.user;
	while (bbs.online && bbs.text(bbs.text.EnterNetMailAddress)) {
		console.putmsg(bbs.text(bbs.text.EnterNetMailAddress));
		var netmail = console.getstr(user.netmail, LEN_NETMAIL, K_EDIT | K_AUTODEL | K_LINE | K_TRIM);
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
	if ((system.settings & SYS_FWDTONET) && system.check_netmail_addr(user.netmail)
		&& bbs.text(bbs.text.ForwardMailQ) && console.yesno(bbs.text(bbs.text.ForwardMailQ)))
		user.settings |= USER_NETMAIL;
}

this;