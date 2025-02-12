// E-mail Section

// $Id: email_sec.js,v 1.10 2020/04/24 08:05:39 rswindell Exp $

// Note: this module replaces the old ### E-mail section ### Baja code in exec/*.src
// replace "call E-mail" with "exec_bin email_sec"

require("sbbsdefs.js", "WM_NONE");
require("userdefs.js", "USER_EXPERT");
var text = bbs.mods.text;
if(!text)
	text = load(bbs.mods.text = {}, "text.js");
var userprops = bbs.mods.userprops;
if(!userprops)
	userprops = load(bbs.mods.userprops = {}, "userprops.js");
const ini_section = "netmail sent";

const NetmailAddressHistoryLength = 10;

while(bbs.online) {
	if(!(user.settings & USER_EXPERT))
		bbs.menu("mode7/mode7_email");

	bbs.replace_text(720,'at email menu');
        bbs.node_action = NODE_CUSTOM;
	bbs.nodesync();
	bbs.revert_text(720);

	writeln();
        if(user.compare_ars("exempt T"))
                console.putmsg ("\x86Time Used: @TUSED@");
        else
                console.putmsg ("\x86Time Left: @TLEFT@");

        // Display main Prompt
        console.putmsg ("\x87->\x83Email Menu\x87-> ");

	var wm_mode = WM_NONE;
	var cmdkeys = "EFKNRSUQ?\r";
	switch(console.getkeys(cmdkeys,K_UPPER)) {
		case 'R':	// Read your mail
			if(user.compare_ars("GUEST")) {
				console.putmsg("Guests are not permitted to read emails\r\n");
				break;
			}
			bbs.read_mail(MAIL_YOUR, user.number);
			break;
		case 'U':	// Read your un-read mail
			if(user.compare_ars("GUEST")) {
				console.putmsg("Guests are not permitted to read emails\r\n");
				break;
			}
			bbs.read_mail(MAIL_YOUR, user.number, LM_UNREAD|LM_REVERSE);
			break;
		case 'K':	// Read/Kill sent mail
			if(user.compare_ars("GUEST")) {
				console.putmsg("Guests are not permitted to read emails\r\n");
				break;
			}
			bbs.read_mail(MAIL_SENT, user.number, LM_REVERSE);
			break;
		case 'E':	// Send Feedback
			bbs.email(/* user # */1, bbs.text(text.ReFeedback));
			break;
		case 'F':
			if(user.compare_ars("GUEST")) {
				console.putmsg("Guests are not permitted to search emails\r\n");
				break;
			}
			bbs.exec("?../xtrn/DDMsgReader/DDMsgReader.js -search=keyword_search -subBoard=mail -startMode=list");
			break;
		case 'S':	// Send Mail
		case 'N':	// Send Mail
			if(user.compare_ars("GUEST")) {
				console.putmsg("You are not permitted to send mail as a guest\r\n");
				break;
			}
			bbs.exec("?/sbbs/xtrn/addressbook/addressbook.js",null,"/sbbs/xtrn/addressbook/");
			break;
		case 'Q':	// Quit
		case '\r':
			exit(0);
		case '?':	// Display menu
			if(user.settings & USER_EXPERT)
				bbs.menu("mode7/mode7_email.js");
			break;
	}
}
