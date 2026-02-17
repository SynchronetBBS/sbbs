// New User Registration Prompts module
// for Synchronet Terminal Server v3.21+

// At minimum, this script must set user.alias (to a unique, legal user name)

// Options (e.g. in ctrl/modopts/newuser_prompts.ini) with defaults:
//   lang=false
//   autoterm=true
//   backspace=true
//   mouse=true
//   charset=true

require("sbbsdefs.js", "UQ_ALIASES");
require("gettext.js", "gettext");

var prompts = bbs.mods.prompts || load(bbs.mods.prompts = {}, "user_info_prompts.js", "new user registration");

var options = load("modopts.js", "newuser_prompts");
if (!options)
	options = {};

"use strict";

while(bbs.online && !js.terminated) {

	if (options.lang)
		prompts.get_lang();
	prompts.get_terminal(user, options);
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
	if (system.newuser_questions & UQ_HANDLE)
		prompts.get_handle();
	if (system.newuser_questions & UQ_ADDRESS)
		prompts.get_address();
	if (system.newuser_questions & (UQ_ADDRESS | UQ_LOCATION))
		prompts.get_location();
	if (system.newuser_questions & UQ_ADDRESS)
		prompts.get_zipcode();
	if (system.newuser_questions & UQ_PHONE)
		prompts.get_phone();
	if (system.newuser_questions & UQ_SEX)
		prompts.get_gender();
	if (system.newuser_questions & UQ_BIRTH)
		prompts.get_birthdate();
	if (!(system.newuser_questions & UQ_NONETMAIL))
		prompts.get_netmail();

	if (!bbs.text(bbs.text.UserInfoCorrectQ) || console.yesno(bbs.text(bbs.text.UserInfoCorrectQ)))
		break;
	prompts.ask_to_cancel();
	console.print(gettext("Restarting new user registration") + "\r\n");
}