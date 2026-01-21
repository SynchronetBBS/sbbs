// Private Message Module
// Installed in SCFG->System->Loadable Modules->Private Msg

if(argc == 1 && argv[0] == "install") {
	// Nothing to do
	exit(0);
}

require("sbbsdefs.js", 'SS_USERON');
require("text.js", 'PrivateMsgPrompt');
require("gettext.js", "gettext");

if(!(bbs.sys_status&SS_USERON))	// Don't allow use until logged-on
	exit();

var saved_node_action = bbs.node_action;

outer_loop:
while(bbs.online && !(console.aborted)) {
	if(user.security.restrictions & UFLAG_C) {
		console.print(bbs.text(R_SendMessages));
		break; 
	}
	bbs.nodesync();
	console.print("\x01n\r\n\xfe \x01b\x01h" + gettext("Private") + " \x01n\xfe ");
	console.mnemonics("~Telegram, ~Message, ~Chat,\x01\\ ~InterBBS, ~View, or [~@Quit@]: ");
	bbs.sys_status&=~SS_ABORT;
	var ch;
	var keys = "TMCIV\r" + console.quit_key;
	while(bbs.online && !console.aborted) {  /* Watch for incoming messages */
		ch=console.inkey(/* mode: */K_UPPER, /* timeout: */1000);
		if(ch && keys.indexOf(ch)>=0)
			break;
		
		console.line_counter = 0;
		bbs.nodesync(true);
		if(console.line_counter)	// Something was displayed, so redisplay our prompt
			continue outer_loop;
	}

	console.line_counter = 0;
	switch(ch) {
		case '':
			continue;
		case 'T':   /* Telegram */
		{
			writeln("Telegram");
			var SentHistoryLength = 10;
			var userprops = load({}, "userprops.js");
			var props_section = "telegram sent";
			var to_list = userprops.get(props_section, "to", []);
			var shown = 0;
			var nodelist_options = bbs.mods.nodelist_options;
			if(!nodelist_options)
				nodelist_options = load({}, "nodelist_options.js");
			var presence = load({}, "presence_lib.js");
			var users = [];
			var n;
			for(var n = 0; n < system.node_list.length; n++) {
				if(n == bbs.node_num - 1)
					continue;
				var node = system.get_node(n + 1);
				if(node.status != NODE_INUSE)
					continue;
				if(node.misc & NODE_POFF)
					continue;
				if(!shown) {
					writeln();
					write(bbs.text(NodeLstHdr));
				}
				writeln(format(nodelist_options.format, n + 1
					,presence.node_status(node, user.is_sysop, nodelist_options, n)));
				users[n] = node.useron;
				shown++;
			}
			var web_users = presence.web_users();
			for(var w = 0; w < web_users.length; w++) {
				var web_user = web_users[w];
				if(web_user.do_not_disturb)
					continue;
				if(!shown) {
					writeln();
					write(bbs.text(NodeLstHdr));
				}
				writeln(format(nodelist_options.format, n + w + 1
					,presence.web_user_status(web_user, nodelist_options.web_browsing, nodelist_options)));
				shown++;
				users[n + w] = web_user.usernum;
			}
			console.mnemonics(bbs.text(NodeToPrivateChat));
			var str = console.getstr(to_list[0], LEN_ALIAS, K_LINE|K_EDIT|K_AUTODEL, to_list.slice(1));
			if(!str || console.aborted) {
				console.aborted = false;
				break;
			}
			log("Sending Telegram to: " + str);
			var node_num = parseInt(str, 10);
			var user_num;
			if(node_num > 0) {
				user_num = users[node_num - 1];
				if(user_num == undefined) {
					write(format(bbs.text(NodeNIsNotInUse), node_num));
					break;
				}
			}
			else if(str.charAt(0) == '#')
				user_num = parseInt(str.slice(1), 10);
			else if(str.charAt(0) == '\'')
				user_num = system.matchuserdata(U_HANDLE, str.slice(1));
			else
				user_num = system.matchuser(str, /* sysop-alias */true);
			if(!user_num || system.username(user_num) == '') {
				user_num = bbs.finduser(str);
				if(!user_num) {
					write(bbs.text(UnknownUser));
					console.aborted = false;
					break;
				}
			}
			if(user_num == user.number && !user.is_sysop) {
				write(format(bbs.text(NoNeedToSendMsgToSelf)));
				break;
			}
			var user_name = system.username(user_num);
			write(format(bbs.text(SendingTelegramToUser), user_name, user_num));
			var msg = [];
			while(msg.length < 5) {
				write("\x01n: \x01h");
				var line = console.getstr(70, msg.length < 4 ? (K_WRAP|K_MSG) : K_MSG);
				if(!line)
					break;
				msg.push(line);
			}
			if(!msg.length || console.aborted) {
				console.aborted = false;
				break;
			}
			var telegram = format(bbs.text(TelegramFmt), user.alias, system.timestr());
			telegram += '    ' + msg.join('\r\n    ') + '\r\n';
			if(system.put_telegram(user_num, telegram)) {
				var to_idx = to_list.indexOf(user_name);
				if(to_idx >= 0)	 
					to_list.splice(to_idx, 1);
				to_list.unshift(user_name);
				if(to_list.length > SentHistoryLength)
					to_list.length = SentHistoryLength;
				userprops.set(props_section, "to", to_list);
				userprops.set(props_section, "localtime", new Date().toString());
				
				write(format(bbs.text(MsgSentToUser), "Telegram", system.username(user_num), user_num));
			}
			else
				alert("Failure sending telegram");
			break;
		}
		case 'M':   /* Message */
			writeln("Message");
			if(!bbs.put_node_message())
				//alert("Failed!")
				;
			break;
		case 'C':   /* Chat */
			writeln("Chat");
			bbs.private_chat();
			bbs.node_action = saved_node_action;
			break;
		case 'I':	/* InterBBS */
			writeln("InterBBS");
			writeln();
			load({}, 'sbbsimsg.js');
			break;
		case 'V':
			load({}, 'viewimsgs.js');
			break;
		default:
			console.putmsg("@Quit@\r\n");
			exit();
	}
}

