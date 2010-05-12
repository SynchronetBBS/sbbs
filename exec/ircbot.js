// $Id$
/*

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details:
 http://www.gnu.org/licenses/gpl.txt

 An IRC bot written in JS that interfaces with the local BBS.

 Copyright 2010 Randolph E. Sommerfeld <sysop@rrx.ca>

*/

load("sockdefs.js");
load("sbbsdefs.js");
load("irclib.js");

js.branch_limit=0; /* we're not an infinite loop. */

Module=new Object();
Module.Save_Data = new Object();
Module.Functions = new Object();
Bot_Commands = new Object();
Config_Last_Write = time(); /* Store when the config was last written. */

load("load/ircbot_functions.js");
//load("load/trivia.js");

/* Global Arrays */
bot_servers = new Array();
masks = new Object();
quotes = new Array();
dcc_chats = new Array();
squelch_list = new Array();

var config_filename = "ircbot.ini";
for (cmdarg=0;cmdarg<argc;cmdarg++) {
	switch(argv[cmdarg].toLowerCase()) {
		case "-f":
			config_filename = argv[++cmdarg];
			break;
		default:
			break;
	}
}

var config = new File(system.ctrl_dir + config_filename);
if (config.open("r")) {
	/* Global Variables */
	command_prefix = config.iniGetValue(null, "command_prefix");
	real_name = config.iniGetValue(null, "real_name");
	help_filename = config.iniGetValue(null, "help_filename");
	help_file = new File(help_filename);
	config_write_delay=parseInt(config.iniGetValue(null, "config_write_delay"));
//	squelch_list = config.iniGetValue(null, "squelch_list").split(",");

	/* Servers */
	var ini_server_secs = config.iniGetSections("server_");
	for (s in ini_server_secs) {
	var mysec = ini_server_secs[s];
		bot_servers.push(new Bot_IRC_Server(
			0,  /* Socket */
			config.iniGetValue(mysec, "addresses"),
			config.iniGetValue(mysec, "nick"),
			config.iniGetValue(mysec, "services_password"),
			config.iniGetValue(mysec, "channels"),
			parseInt(config.iniGetValue(mysec, "port")),
			mysec.slice(7)  /* Network Name */
		));
	}

	/* Quotes */
	var ini_quotes = config.iniGetKeys("quotes");
	for (q in ini_quotes) {
		quotes.push(config.iniGetValue("quotes", ini_quotes[q]));
	}

	config.close();
} else {
	exit("Couldn't open config file!");
}

var user_settings_files = directory(sysem.data_dir + "user/*.ircbot.ini");
for (f in user_settings_files) {
	var us_file = new File(user_settings_files[f]);
	if (us_file.open("r")) {
		var tokenized_path = user_settings_files[f].split("/");
		var us_filename = tokenized_path[tokenized_path.length-1];
		var uid_str = us_filename.split(".")[0];
		while (uid_str[0] == "0") {
			uid_str = uid_str.slice(1);
		}
		printf("***Reading: " + us_file.name + "\r\n");
		var read_masks = us_file.iniGetValue(null, "masks");
		if (read_masks)
			masks[parseInt(uid_str)] = read_masks.split(",");
	}
}

log("*** Entering Main Loop. ***");

function main() {
	while (!js.terminated) {
		for (my_srv in bot_servers) {
			var cmdline;
			var srv = bot_servers[my_srv];
			if (!srv.sock &&(srv.lastcon <time())) { //we're not connected.
				var consock = IRC_client_connect(srv.host, srv.nick,
					command_prefix, real_name, srv.port);
				if (consock) {
					srv.sock = consock;
					log("--- Connected to " + srv.host);
					/* If we just connected, then clear all our joined channels. */
					for (c in srv.channel) {
						srv.channel[c].is_joined = false;
					}
				} else {
					log("--- Connect to " + srv.host + " failed, "
						+ "retry in 60 seconds.");
					srv.lastcon = time() + 60;
				}
			} else if (srv.sock && srv.sock.data_waiting &&
					(cmdline=srv.sock.recvline(4096,0))) {
				var onick;
				var ouh;
				var outline;
				var sorigin = cmdline.split(" ")[0].slice(1);
				if ((cmdline[0] == ":") && sorigin.match(/[@]/)) {
					onick = sorigin.split("!")[0];
					ouh = sorigin.split("!")[1];
					outline = "["+onick+"("+ouh+")] " + cmdline;
				} else {
					onick = "";
					ouh = "";
					outline = cmdline;
				}
				log("<-- " + srv.host + ": " + outline);
				srv.server_command(IRC_parsecommand(cmdline),onick,ouh);
				// 	TODO: Process module data parsing, if any
				//	for(var m in Module.command) {
				//		Module.command[m](IRC_parsecommand(cmdline),onick,ouh);
				//	}
			}

			// Run through some commands.
			if (srv.sock && srv.is_registered) {
				for (c in srv.channel) {
					if (!srv.channel[c].is_joined &&
						(srv.channel[c].lastjoin < time())) {
						srv.writeout("JOIN " + srv.channel[c].name);
						srv.channel[c].lastjoin = time() + 60;
					}
				}
			}
			
			// Cycle any available module "main" functions
			for(var l in Module.Functions) {
				Module.Functions[l](srv);
			}
			mswait(10); /* Don't peg the CPU */
		}
		if ( (time() - Config_Last_Write) > config_write_delay )
			save_everything();

	}
}

//////////////////// Objects and Functions ////////////////////
function Bot_IRC_Server(sock,host,nick,svspass,channels,port,name) {
	// Static variables (never change)
	this.sock = sock;
	this.host = host;
	this.nick = nick;
	this.svspass = svspass;
	this.port = port;
	this.name = name;
	// Channels
	this.channel = new Array();
	var my_channels = channels.split(",");
	for (c in my_channels) {
		log ("--- Adding Channel: " + my_channels[c]);
		this.channel[my_channels[c].toUpperCase()] = new Bot_IRC_Channel(
			my_channels[c]);
	}
	// Dynamic variables (used for the bot's state.
	this.curnick = nick;
	this.lastcon = 0;			// When it's OK to reconnect.
	this.is_registered = false;
	this.juped = false;
	this.users = new Object();	// Store local nicks & uh info.
	// Functions
	this.ctcp = Server_CTCP;
	this.ctcp_reply = Server_CTCP_Reply;
	this.o = Server_target_out;
	this.writeout = Server_writeout;
	this.server_command = Server_command;
	this.check_bot_command = Server_check_bot_command;
	this.bot_access = Server_Bot_Access;
}

function Bot_IRC_Channel(name) {
	// Statics.
	this.name = name;
	// Dynamics.
	this.is_joined = false;
	this.lastjoin = 0;
	// Functions.
}

function Server_User(uh) {
	this.uh = uh;
	this.ident = false;
	this.last_spoke = false;
}

function Bot_Command(min_security,args_needed,ident_needed) {
	this.min_security = min_security;
	this.args_needed = args_needed;
	this.ident_needed = ident_needed;
	this.command = false;
}

function DCC_Chat(sock,id) {
	this.sock = sock;
	this.id = id;
	/* State info */
	this.waiting_for_password = true;
	/* Functions */
	this.o = DCC_Out;
}

function DCC_Out(target,str) {
	this.sock.write(str + "\r\n");
}

/* This must be at the very bottom. */
load("load/ircbot_commands.js");
main();
