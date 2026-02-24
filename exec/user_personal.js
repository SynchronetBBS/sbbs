// User personal information / credentials menu
// for Synchronet v3.21+

// Configurable via modopts.ini [user_personal]
// alias = false
// name = false
// handle = false
// netmail = true
// phone = false
// location = false
// gender = false

// Customizable look via modopts.ini [user_personal]
// header_prefix
// header
// user_fmt
// text_alias
// text_name
// text_handle
// text_phone
// text_location
// text_gender
// text_password
// text_ssh_keys
// text_signature
// option_with_val_fmt
// option_with_date_fmt
// prompt

// Run from the Terminal Server via js.exec()

"use strict";

require("sbbsdefs.js", 'SYS_PWEDIT');
require("gettext.js", 'gettext');
var prompts = bbs.mods.prompts || load(bbs.mods.prompts = {}, "user_info_prompts.js");
var options = load("modopts.js", "user_personal", {});
if (!options.option_with_val_fmt)
	options.option_with_val_fmt = "\x01v[\x01V%c\x01v] %s: \x01V%s";
if (!options.option_with_date_fmt)
	options.option_with_date_fmt = "\x01v[\x01V%c\x01v] %-9s (%s: \x01V%s\x01v)";
var ssh_support = server.options & (1<< 12); // BBS_OPT_ALLOW_SSH

prompts.operation = "";

while(bbs.online && !js.terminated) {
	var keys = 'Q\r';
	console.aborted = false;	
	console.clear();
	console.print((options.header_prefix || "\x01v")
		+ (options.header || (gettext("Personal Information") + " " + gettext("for")))
		+ format(options.user_fmt || " \x01V%s #%u\x01v:", user.alias, user.number));
	console.newline(2);
	if (options.alias === true) {
		console.print(format(options.option_with_val_fmt, 'A', options.text_alias || gettext("Alias"), user.alias));
		keys += 'A';
		console.add_hotspot('A');
		console.newline();
	}
	if (options.name === true) {
		console.print(format(options.option_with_val_fmt, 'R', options.text_name || gettext("Real Name"), user.name));
		keys += 'R';
		console.add_hotspot('R');
		console.newline();
	}
	if (options.handle === true) {
		console.print(format(options.option_with_val_fmt, 'H', options.text_handle || gettext("Handle"), user.handle));
		keys += 'H';
		console.add_hotspot('H');
		console.newline();
	}
	if (options.netmail !== false) {
		console.print(format(options.option_with_val_fmt, 'E', options.text_netmail || gettext("NetMail Address"), user.netmail));
		keys += 'E';
		console.add_hotspot('E');
		console.newline();
	}
	if (options.phone === true) {
		console.print(format(options.option_with_val_fmt, 'N', options.text_phone || gettext("Phone Number"), user.phone));
		keys += 'N';
		console.add_hotspot('N');
		console.newline();
	}
	if (options.location === true) {
		var location = (system.newuser_questions & UQ_ADDRESS) ? user.address : "";
		if (location) location += " ";
		location += user.location;
		console.print(format(options.option_with_val_fmt, 'L', options.text_location || gettext("Location"), location));
		keys += 'L';
		console.add_hotspot('L');
		console.newline();
	}
	if (options.gender === true) {
		console.print(format(options.option_with_val_fmt, 'G', options.text_gender || gettext("Gender"), user.gender));
		keys += 'G';
		console.add_hotspot('G');
		console.newline();
	}
	if (system.settings & SYS_PWEDIT) {
		console.print(format(options.option_with_date_fmt, 'P', options.text_password || gettext("Password"), gettext("last changed")
			, system.datestr(user.security.password_date)));
		keys += 'P';
		console.add_hotspot('P');
		console.newline();
	}
	if (ssh_support) {
		console.print(format(options.option_with_date_fmt, 'S', options.text_ssh_keys || gettext("SSH Keys"), gettext("last changed")
			, system.datestr(file_date(prompts.ssh_keys_filename(user)))));
		keys += 'S';
		console.add_hotspot('S');
		console.newline();	
	}
	console.print(format(options.option_with_date_fmt, 'M', options.text_signature || gettext("Signature"), gettext("last changed")
		, system.datestr(file_date(prompts.sig_filename(user)))));
	keys += 'M';
	console.add_hotspot('M');
	console.newline(2);	
	console.print(options.prompt || "\x01v@Which@ or [\x01VQ\x01v] to @Quit@: \x01V", P_ATCODES);
	console.add_hotspot('Q');

	switch(console.getkeys(keys, K_UPPER)) {
		case 'A':
			prompts.get_alias(user);
			break;
		case 'R':
			prompts.get_name(user);
			break;
		case 'H':
			prompts.get_handle(user);
			break;
		case 'E':
			prompts.get_netmail(user);
			break;
		case 'L':
			if (system.newuser_questions & UQ_ADDRESS)
				prompts.get_address(user);
			prompts.get_location(user);
			if (system.newuser_questions & UQ_ADDRESS)
				prompts.get_zipcode(user);
			break;
		case 'N':
			prompts.get_phone(user);
			break;
		case 'G':
			prompts.get_gender(user);
			break;
		case 'P':
			prompts.change_password(user);
			break;
		case 'M':
			prompts.change_signature(user);
			break;
		case 'S':
			prompts.change_ssh_keys(user);
			break;
		case 'Q':
		case '\r':
			console.clear_hotspots();
			exit();
	}
}
