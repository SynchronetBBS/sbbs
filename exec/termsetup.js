require("sbbsdefs.js", 'CON_BLINK_FONT');
require("userdefs.js", 'USER_ICE_COLOR');
if(user.settings & USER_ANSI) {
	if(argv.indexOf("force") >=0 || !(console.status&(CON_BLINK_FONT|CON_HBLINK_FONT))) {
		var cterm = load({}, "cterm_lib.js");
		cterm.bright_background(Boolean(user.settings & USER_ICE_COLOR));
	}
	console.ansi_getlines();
}
