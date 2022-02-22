// Auto-message (ala WWIV)

// modopts.ini [automsg] options:
//   prompt
//   sysop_prompt
//   intro
//   header_fmt
//   user_fmt
//   line_fmt
//   max_line_len

"use strict";

require("text.js", 'AutoMsg');
require("userdefs.js", 'UFLAG_W');
require("sbbsdefs.js", 'P_NOABORT');

var options = load('modopts.js', "automsg");
if(!options)
	options = {};
if(!options.max_line_len)
	options.max_line_len = 76;

function automsg()
{
	const fmt = options.line_fmt || " > %.79s\r\n";
	var automsg = system.data_dir + "msgs/auto.msg";
	while(bbs.online && !js.termiated && !console.aborted) {
		bbs.nodesync();
		if(user.is_sysop)
			console.mnemonics(options.sysop_prompt
				|| "\r\nAuto Message - ~Read, ~Write, ~Delete or ~Quit: ");
		else
			console.mnemonics(options.prompt || bbs.text(AutoMsg));
		switch(console.getkeys("RWQD",0)) {
			case 'R':
				console.printfile(automsg,P_NOABORT|P_NOATCODES|P_WORDWRAP|P_NOERROR);
				break;
			case 'W':
				if(user.security.restrictions&UFLAG_W) {
					console.print(bbs.text(R_AutoMsg));
					break;
				}
				bbs.action=NODE_AMSG;
				bbs.nodesync();
				console.print(options.intro || "\r\nMaximum of 3 lines:\r\n");
				var str = console.getstr(str, options.max_line_len, K_WRAP|K_MSG);
				if(!str)
					break;
				var buf = format(fmt, str);
				str = console.getstr(str, options.max_line_len, K_WRAP|K_MSG);
				if(str) {
					buf += format(fmt, str);
					str = console.getstr(str, options.max_line_len, K_MSG);
					if(str) {
						buf += format(fmt, str);
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
					var tmp = format(options.user_fmt || "%s #%d", user.alias, user.number);
					if(anon)
						tmp = bbs.text(Anonymous);
					str = format(options.header_fmt || bbs.text(AutoMsgBy), tmp, system.timestr());
					file.write(str);
					file.write(buf);
					file.close();
				}
				break;
			case 'D':
				if(user.is_sysop && !console.noyes(format(bbs.text(DeleteTextFileQ), automsg)))
					file_remove(automsg);
				break;
			case 'Q':
				return;
		}
	}
}

automsg();
