// User personal information / credentials menu
// for Synchronet v3.21+

// Configurable via modopts.ini [user_personal]
// alias = false
// name = false
// handle = false
// phone = false
// location = false
// gender = false

// Run from the Terminal Server via js.exec()

"use strict";

require("sbbsdefs.js", 'SYS_PWEDIT');
require("gettext.js", 'gettext');
var prompts = bbs.mods.prompts || load(bbs.mods.prompts = {}, "user_info_prompts.js");
var options = load("modopts.js", "user_personal");
if (!options)
	options = {};
if (!options.option_fmt)
	options.option_fmt = "\x01u[\x01U%c\x01u] ";
if (!options.option_val)
	options.option_val = ": \x01U%s";
if (!options.option_date)
	options.option_date = " (%s: \x01U%s\x01u)";
var ssh_support = server.options & (1<< 12); // BBS_OPT_ALLOW_SSH

prompts.operation = "";

while(bbs.online && !js.terminated) {
	var keys = 'Q\r';
	console.aborted = false;	
	console.clear();
	console.print((options.header_prefix || "\x01U")
		+ gettext("Personal Information")
		+ " " + gettext("for")
		+ format(options.user_fmt || " \x01U%s #%u:", user.alias, user.number));
	console.newline(2);
	if (options.alias === true) {
		console.print(format(options.option_fmt, 'A') + gettext("Alias") + format(options.option_val, user.alias));
		keys += 'A';
		console.add_hotspot('A');
		console.newline();
	}
	if (options.name === true) {
		console.print(format(options.option_fmt, 'R') + gettext("Real Name") + format(options.option_val, user.name));
		keys += 'R';
		console.add_hotspot('R');
		console.newline();
	}
	if (options.handle === true) {
		console.print(format(options.option_fmt, 'H') + gettext("Handle") + format(options.option_val, user.handle));
		keys += 'H';
		console.add_hotspot('H');
		console.newline();
	}
	if (options.phone === true) {
		console.print(format(options.option_fmt, 'N') + gettext("Phone Number") +  format(options.option_val, user.phone));
		keys += 'N';
		console.add_hotspot('N');
		console.newline();
	}
	if (options.location === true) {
		var location = (system.newuser_questions & UQ_ADDRESS) ? user.address : "";
		if (location) location += " ";
		location += user.location;
		console.print(format(options.option_fmt, 'L') + gettext("Location") + format(options.option_val, location));
		keys += 'L';
		console.add_hotspot('L');
		console.newline();
	}
	if (options.gender === true) {
		console.print(format(options.option_fmt, 'G') + gettext("Gender") + format(options.option_val, user.gender));
		keys += 'G';
		console.add_hotspot('G');
		console.newline();
	}
	if (system.settings & SYS_PWEDIT) {
		console.print(format(options.option_fmt, 'P') + gettext("Password")
			+ format(options.option_date, gettext("last changed"), system.datestr(user.security.password_date)));
		keys += 'P';
		console.add_hotspot('P');
		console.newline();
	}
	if (ssh_support) {
		console.print(format(options.option_fmt, 'S') + gettext("SSH Keys")
			+ format(options.option_date, gettext("last changed"), system.datestr(file_date(prompts.ssh_keys_filename(user)))));
		keys += 'S';
		console.add_hotspot('S');
		console.newline();	
	}
	console.print(format(options.option_fmt, 'M') + gettext("Sign-off")
		+ format(options.option_date, gettext("last changed"), system.datestr(file_date(prompts.sig_filename(user)))));
	keys += 'M';
	console.add_hotspot('M');
	console.newline(2);	
	console.putmsg(options.prompt || "\x01n\x01h\x01u@Which@ or [\x01UQ\x01u] to @Quit@: \x01U", P_SAVEATR);
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
