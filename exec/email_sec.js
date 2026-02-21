// E-mail Section

// Note: this module replaces the old ### E-mail section ### Baja code in exec/*.src
// replace "call E-mail" with "exec_bin email_sec"

require("sbbsdefs.js", "WM_NONE");
require("userdefs.js", "USER_EXPERT");
var shell = load({}, "shell_lib.js");
var userprops = bbs.mods.userprops;
if(!userprops)
	userprops = load(bbs.mods.userprops = {}, "userprops.js");
const ini_section = "netmail sent";

while(bbs.online) {
	if(!(user.settings & USER_EXPERT))
		bbs.menu("e-mail");
	bbs.nodesync();
	console.print("\r\n\x01_\x01y\x01hE-mail: \x01n");
	var wm_mode = WM_NONE;
	var cmdkeys = "LSARUFNKQ?\r";
	var key = console.getkeys(cmdkeys,K_UPPER);
	switch(key) {
		case 'L':	// List/read your mail
			bbs.exec("?msglist.js mail -preview");
			break;
		case 'R':	// Read your mail
		case 'U':	// Read your un-read mail
		case 'K':	// Read/Kill sent mail
			const MAIL_LM_MODE = LM_REVERSE;
			var lm_mode = user.mail_settings & MAIL_LM_MODE;
			if(key == 'U')
				lm_mode |= LM_UNREAD;
			var new_lm_mode = bbs.read_mail(key == 'K' ? MAIL_SENT : MAIL_YOUR, user.number, lm_mode) & MAIL_LM_MODE;
			if(new_lm_mode != (lm_mode & MAIL_LM_MODE)) {
				if(new_lm_mode & MAIL_LM_MODE)
					user.mail_settings |= MAIL_LM_MODE;
				else
					user.mail_settings &= ~MAIL_LM_MODE;
			}
			break;
		case 'F':	// Send Feedback
			shell.send_feedback();
			break;
		case 'A':	// Send file attachment
			wm_mode = WM_FILE;
		case 'S':	// Send Mail
			shell.send_email();
			break;
		case 'N':	// Send NetMail
			shell.send_netmail();
			break;
		default:
			exit(0);
		case '?':	// Display menu
			if(user.settings & USER_EXPERT)
				bbs.menu("e-mail");
			break;
	}
}
