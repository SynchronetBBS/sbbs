// $Id: chat_sec.js,v 1.15 2020/01/05 23:50:35 rswindell Exp $

// Chat Section for any/all Synchronet command shells

"use strict";

require("sbbsdefs.js", 'USER_EXPERT');
require("nodedefs.js", 'NODE_CHAT');
require("text.js", 'R_Chat');

// Over-ride these default values by creating/modifying the [chat] section in your ctrl/modopts.ini file
var options = load("modopts.js", "chat");
if (!options)
	options = load("modopts.js", "chat_sec");
if (!options)
	options = {};
if (options.irc === undefined)
	options.irc = true;
if (options.irc_server === undefined)
	options.irc_server = "irc.synchro.net 6667";
if (options.irc_channel === undefined)
	options.irc_channel = "#Synchronet";
if (options.irc_seclevel === undefined)
	options.irc_seclevel = 90;
if (options.finger === undefined)
	options.finger = true;
if (options.imsg === undefined)
	options.imsg = true;

if(user.security.restrictions & UFLAG_C) {
    write(bbs.text(R_Chat));
	exit(0);
}

// Set continue point for main menu commands
menu:
while(1) {
	var str="";
	// Display TEXT\MENU\CHAT.* if not in expert mode
	bbs.nodesync();
	if(!(user.settings & USER_EXPERT)) {
		bbs.menu("mode7/mode7_chat");
				}

	// Update node status
	bbs.node_action = NODE_CHAT;
	bbs.nodesync();
	if(user.compare_ars("exempt T"))
        	console.putmsg ("\x86Time Used: @TUSED@");
	else
        	console.putmsg ("\x86Time Left: @TLEFT@");
		
	// Display main Prompt
	console.putmsg ("\x87->\x83Chat Menu\x87-> ");

	var keys = "ACEMNPQSTYWX?$%";
	if(options.imsg)
		keys += "I";
	if(options.irc)
		keys += "R";
	if(options.finger)
		keys += "F";
	switch(console.getkeys(keys, K_UPPER)) {
		case "S":
			var val = user.chat_settings ^= CHAT_SPLITP;
			writeln("");
			break;
		case "A":
			var val = user.chat_settings ^= CHAT_NOACT;
			writeln("");
			system.node_list[bbs.node_num-1].misc ^= NODE_AOFF;
			break;
		case 'P':
			var val = user.chat_settings ^= CHAT_NOPAGE;
			writeln("");
			system.node_list[bbs.node_num-1].misc ^= NODE_POFF;
			break;
		case 'E':
			if(user.compare_ars("SYSOP")) {
				writeln("");
				system.operator_available = !system.operator_available;
			}
			break;
		case 'F':
			writeln("");
			load("finger.js");
			break;
		case 'I':
			writeln("");
			load({}, "sbbsimsg.js");
			break;
		case 'R':
		{
			var server=options.irc_server;
			if(user.security.level >= options.irc_seclevel || user.security.exemptions&UFLAG_C) {
				write("\r\n\x01n\x01y\x01hIRC Server: ");
				server=console.getstr(options.irc_server, 40, K_EDIT|K_LINE|K_AUTODEL);
				if(console.aborted || server.length < 4)
					break;
			}
			if(server.indexOf(' ') < 0)
				server += " 6667";
			write("\r\n\x01n\x01y\x01hIRC Channel: ");
			var channel=console.getstr(options.irc_channel, 40, K_EDIT|K_LINE|K_AUTODEL);
			if(!console.aborted && channel.length) {
				log("IRC to " + server + " " + channel);
				bbs.replace_text(NodeActionCustom,"in Internet Relay Chat via " + client.protocol); 
				bbs.node_action = NODE_CUSTOM;
				bbs.nodesync();
				bbs.exec("?irc -a " + server + " " + channel); // can't be load()ed because it calls exit()
				bbs.revert_text(NodeActionCustom);
			}
			break;
		}
		case 'C':
			bbs.multinode_chat();
			break;
		case 'N':
			bbs.private_chat();
			break;
		case 'Y':
			if(!bbs.page_sysop())
				bbs.page_guru();
			break;
		case 'T':
			bbs.page_guru();
			break;
		case 'M':
			bbs.exec_xtrn("MULTIRELAYCHAT");
			break;
		case '?':
			if(user.settings & USER_EXPERT)
				bbs.menu("mode7/mode7_chat");
			break;
                case 'Q':
			break menu;

		default:
			console.clear();
			break;
	}
}
