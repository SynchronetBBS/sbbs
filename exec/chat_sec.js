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

function on_or_off(on)
{
	return bbs.text(on ? ON : OFF);
}

// Set continue point for main menu commands
menu:
while(1) {
	var str="";

	// Display TEXT\MENU\CHAT.* if not in expert mode
	if(!(user.settings & USER_EXPERT)) {
		bbs.menu("chat");
	}

	// Update node status
	bbs.node_action = NODE_CHAT;
	bbs.nodesync();
	write(bbs.text(ChatPrompt));

	var keys = "ACDJPQST?\r";
	if(options.imsg)
		keys += "I";
	if(options.irc)
		keys += "R";
	if(options.finger)
		keys += "F";
	switch(console.getkeys(keys, K_UPPER)) {
		case "S":
			var val = user.chat_settings ^= CHAT_SPLITP;
			write("\x01n\r\nPrivate split-screen chat is now: \x01h");
			writeln(on_or_off(val & CHAT_SPLITP));
			break;
		case "A":
			var val = user.chat_settings ^= CHAT_NOACT;
			write("\x01n\r\nNode activity alerts are now: \x01h");
			writeln(on_or_off(!(val & CHAT_NOACT)));
			system.node_list[bbs.node_num-1].misc ^= NODE_AOFF;
			break;
		case 'D':
			var val = user.chat_settings ^= CHAT_NOPAGE;
			write("\x01n\r\nUser chat/messaging availability is now: \x01h");
			writeln(on_or_off(!(val & CHAT_NOPAGE)));
			system.node_list[bbs.node_num-1].misc ^= NODE_POFF;
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
				bbs.exec("?irc -a " + server + " " + channel); // can't be load()ed because it calls exit()
			}
			break;
		}
		case 'J':
			bbs.multinode_chat();
			break;
		case 'P':
			bbs.private_chat();
			break;
		case 'C':
			if(!bbs.page_sysop())
				bbs.page_guru();
			break;
		case 'T':
			bbs.page_guru();
			break;
		case '?':
			if(user.settings & USER_EXPERT)
				bbs.menu("chat");
			break;
		default:
			break menu;
	}
}
