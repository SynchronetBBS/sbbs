// E-mail Section

// Note: this module replaces the old ### E-mail section ### Baja code in exec/*.src
// replace "call E-mail" with "exec_bin email_sec"

require("sbbsdefs.js", "WM_NONE");
require("userdefs.js", "USER_EXPERT");
var userprops = bbs.mods.userprops;
if(!userprops)
	userprops = load(bbs.mods.userprops = {}, "userprops.js");
const ini_section = "netmail sent";

const NetmailAddressHistoryLength = 10;

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
			bbs.email(/* user # */1, bbs.text(bbs.text.ReFeedback));
			break;
		case 'A':	// Send file attachment
			wm_mode = WM_FILE;
		case 'S':	// Send Mail
			console.putmsg(bbs.text(bbs.text.Email));
			var name = console.getstr(40, K_TRIM);
			if(!name)
				break;
			if(name.indexOf('@') > 0) {
				bbs.netmail(name);
				break;
			}
			var number = bbs.finduser(name);
			if(console.aborted)
				break;
			if(!number)
				number = system.matchuser(name);
			if(!number && (msg_area.settings&MM_REALNAME))
				number = system.matchuserdata(U_NAME, name);
			if(number)
				bbs.email(number, wm_mode);
			else
				console.putmsg(bbs.text(bbs.text.UnknownUser));
			break;
		case 'N':	// Send NetMail
			var netmail = msg_area.fido_netmail_settings | msg_area.inet_netmail_settings;
			console.crlf();
			if((netmail&NMAIL_FILE) && !console.noyes("Attach a file"))
				wm_mode = WM_FILE;
			if(console.aborted)
				break;
			console.putmsg(bbs.text(bbs.text.EnterNetMailAddress));
			var addr_list = userprops.get(ini_section, "address", []) || [];
			var addr = console.getstr(256, K_LINE | K_TRIM, addr_list);
			if(!addr || console.aborted)
				break;
			if(bbs.netmail(addr.split(','), wm_mode)) {
				var addr_idx = addr_list.indexOf(addr);
				if(addr_idx >= 0)
					addr_list.splice(addr_idx, 1);
				addr_list.unshift(addr);
				if(addr_list.length > NetmailAddressHistoryLength)
					addr_list.length = NetmailAddressHistoryLength;
				userprops.set(ini_section, "address", addr_list);
				userprops.set(ini_section, "localtime", new Date().toString());
			}
			break;
		default:
			exit(0);
		case '?':	// Display menu
			if(user.settings & USER_EXPERT)
				bbs.menu("e-mail");
			break;
	}
}
