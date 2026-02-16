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
if(!options)
	options = {};
var ssh_support = server.options & (1<< 12); // BBS_OPT_ALLOW_SSH

prompts.operation = "";

while(bbs.online && !js.terminated) {
	var keys = 'Q\r';
	console.aborted = false;	
	console.clear();
	console.print("\x01U" + gettext("Personal Information") + " " + gettext("for") + format(" \x01U%s #%u:", user.alias, user.number));
	console.newline(2);
	if (options.alias === true) {
		console.print("\x01u[\x01UA\x01u] " + gettext("Alias") + format(": \x01U%s", user.alias));
		keys += 'A';
		console.add_hotspot('A');
		console.newline();
	}
	if (options.name === true) {
		console.print("\x01u[\x01UR\x01u] " + gettext("Real Name") + format(": \x01U%s", user.name));
		keys += 'R';
		console.add_hotspot('R');
		console.newline();
	}
	if (options.handle === true) {
		console.print("\x01u[\x01UH\x01u] " + gettext("Handle") + format(": \x01U%s", user.handle));
		keys += 'H';
		console.add_hotspot('H');
		console.newline();
	}
	if (options.phone === true) {
		console.print("\x01u[\x01UN\x01u] " + gettext("Phone Number") +  format(": \x01U%s", user.phone));
		keys += 'N';
		console.add_hotspot('N');
		console.newline();
	}
	if (options.location === true) {
		var location = user.address;
		if (location) location += " ";
		location += user.location;
		console.print("\x01u[\x01UL\x01u] " + gettext("Location") + format(": \x01U%s", location));
		keys += 'L';
		console.add_hotspot('L');
		console.newline();
	}
	if (options.gender === true) {
		console.print("\x01u[\x01UG\x01u] " + gettext("Gender") + format(": \x01U%s", user.gender));
		keys += 'G';
		console.add_hotspot('G');
		console.newline();
	}
	if (system.settings & SYS_PWEDIT) {
		console.print("\x01u[\x01UP\x01u] " + gettext("Password")
			+ format(" (%s: \x01U%s\x01u)", gettext("last changed"), system.datestr(user.security.password_date)));
		keys += 'P';
		console.add_hotspot('P');
		console.newline();
	}
	if (ssh_support) {
		console.print("\x01u[\x01US\x01u] " + gettext("SSH Keys")
			+ format(" (%s: \x01U%s\x01u)", gettext("last changed"), system.datestr(file_date(prompts.ssh_keys_filename(user)))));
		keys += 'S';
		console.add_hotspot('S');
		console.newline();	
	}
	console.print("\x01u[\x01UM\x01u] " + gettext("Sign-off")
		+ format(" (%s: \x01U%s\x01u)", gettext("last changed"), system.datestr(file_date(prompts.sig_filename(user)))));
	keys += 'M';
	console.add_hotspot('M');
	console.newline(2);	
	console.putmsg("\x01n\x01h\x01u@Which@ or [\x01UQ\x01u] to @Quit@: \x01U", P_SAVEATR);
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
