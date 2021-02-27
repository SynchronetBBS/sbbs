// $Id: automsg.js,v 1.3 2020/04/19 03:15:35 rswindell Exp $
// vi: tabstop=4

"use strict";

require("text.js", 'AutoMsg');
require("userdefs.js", 'UFLAG_W');
require("sbbsdefs.js", 'P_NOABORT');

function automsg()
{
	const quote_fmt=" > %.*s\r\n";
	var automsg = system.data_dir + "msgs/auto.msg";
	while(bbs.online && !js.termiated) {
		bbs.nodesync();
		console.mnemonics(bbs.text(AutoMsg));
		switch(console.getkeys("RWQD",0)) {
			case 'R':
				console.printfile(automsg,P_NOABORT|P_NOATCODES|P_WORDWRAP|P_NOERROR);
				break;
			case 'W':
				if(user.security.restrictions&UFLAG_W) {
					console.print(bbs.text[R_AutoMsg]);
					break;
				}
				bbs.action=NODE_AMSG;
				bbs.nodesync();
				console.print("\r\nMaximum of 3 lines:\r\n");
				var str = console.getstr(str, 76, K_WRAP|K_MSG);
				if(!str)
					break;
				var buf = format(quote_fmt, 79, str);
				str = console.getstr(str, 76, K_WRAP|K_MSG);
				if(str) {
					buf += format(quote_fmt, 79, str);
					str = console.getstr(str, 76, K_MSG);
					if(str) {
						buf += format(quote_fmt, 79, str);
					}
				}
				if(console.yesno(bbs.text(OK))) {
					var anon = false;
					if(user.security.exemptions&UFLAG_A) {
						if(!console.noyes(bbs.text(AnonymousQ)))
							anon = true;
					}
					var file = new File(automsg);
					if(!file.open("w")) {
						alert("Error " + file.error + " opening " + file.name);
						return;
					}
					var tmp = format("%s #%d", user.alias, user.number);
					if(anon)
						tmp = bbs.text(Anonymous);
					str = format(bbs.text(AutoMsgBy), tmp, system.timestr());
					file.write(str);
					file.write(buf);
					file.close();
				}
				break;
			case 'D':
				if(user.is_sysop)
					file_remove(automsg);
				break;
			case 'Q':
				return;
		}
	}
}

automsg();
