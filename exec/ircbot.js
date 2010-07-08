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
load("funclib.js");
load("http.js");

load("synchronet-json.js");

js.branch_limit=0; /* we're not an infinite loop. */

Modules=new Array();
Bot_Commands = new Object();
Config_Last_Write = time(); /* Store when the config was last written. */
load("load/ircbot_functions.js");

/* Global Arrays */
Bot_Servers = new Object();
Quotes = new Array();
Masks = new Object();
DCC_Chats = new Array();
Squelch_List = new Array();

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
	config_write_delay=parseInt(config.iniGetValue(null, "config_write_delay"));
//	Squelch_List = config.iniGetValue(null, "Squelch_List").split(",");

	/* Modules */
	var ini_modules = config.iniGetSections("module_");
	for (var m in ini_modules) {
		var mysec = ini_modules[m];
		var module_name=config.iniGetValue(mysec,"name").toUpperCase();
		Modules[module_name]=new Object();
		Modules[module_name].Bot_Commands=new Object();
		Modules[module_name].enabled=true;
		Modules[module_name].dir=config.iniGetValue(mysec,"dir");
		Modules[module_name].load=directory(Modules[module_name].dir+"*.js");
		Modules[module_name].lib=[];
		var lib_list=config.iniGetValue(mysec,"lib");
		if(lib_list) {
			lib_list=lib_list.split(",");
			for(var l in lib_list) Modules[module_name].lib.push(removeSpaces(lib_list[l]));
		}
	}
	
	/* Servers */
	var ini_server_secs = config.iniGetSections("server_");
	for (s in ini_server_secs) {
		var mysec = ini_server_secs[s];
		var network = mysec.replace("server_", "").toUpperCase();
		Bot_Servers[network] = (new Bot_IRC_Server(
			0,  /* Socket */
			config.iniGetValue(mysec, "addresses"),
			config.iniGetValue(mysec, "nick"),
			config.iniGetValue(mysec, "services_password"),
			config.iniGetValue(mysec, "channels"),
			parseInt(config.iniGetValue(mysec, "port")),
			mysec.slice(7)  /* Network Name */
		));
	}

	config.close();
} else {
	exit("Couldn't open config file!");
}

//LOAD MODULE FILES
for(var m in Modules) {
	for(var l in Modules[m].lib) {
		if(Modules[m].lib[l]) load(Modules[m],Modules[m].lib[l]);
	}
	for(var l in Modules[m].load) {
		if(Modules[m].load[l]) load(Modules[m],Modules[m].load[l]);
	}
}

var user_settings_files = directory(system.data_dir + "user/*.ircbot.ini");
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
		var read_Masks = us_file.iniGetValue(null, "masks");
		if (read_Masks)
			Masks[parseInt(uid_str)] = read_Masks.split(",");
	}
}

log("*** Entering Main Loop. ***");

function main() {
	while (!js.terminated) {
		for (my_srv in Bot_Servers) {
			var cmdline;
			var srv = Bot_Servers[my_srv];
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
				if(!srv.ignore[onick.toUpperCase()]) {
					srv.server_command(cmdline,onick,ouh);
				}
			}

			// Run through some commands.
			if (srv.sock && srv.is_registered) {
				for (c in srv.channel) {
					if (!srv.channel[c].is_joined &&
						(srv.channel[c].lastjoin < time())) {
						srv.writeout("JOIN " + srv.channel[c].name);
						srv.channel[c].lastjoin = time() + 60;
					}
					// Cycle any available module "main" functions
					for(var m in Modules) {
						if(Modules[m].main) Modules[m].main(srv,srv.channel[c].name);
					}
				}
			}
			
			// Cycle server output buffer (srv.buffer)
			while(srv.sock && srv.burst<srv.max_burst) {
				srv.burst++;
				if(!srv.buffers.length>0) break;
				var next_output=srv.buffers.shift();
				srv.sock.write(next_output.buffer.shift());
				if(next_output.buffer.length) srv.buffers.push(next_output);
			}
			var curtime=system.timer;
			if ((curtime-srv.lastout)>srv.delay) {
				srv.lastout=system.timer;
				srv.burst=0;
			}
		}

		/* Take care of DCC chat sessions */
		for (c in DCC_Chats) {
			if (!DCC_Chats[c].sock.is_connected) {
				log("Closing session.");
				DCC_Chats[c].sock.close();
				delete DCC_Chats[c];
				continue;
			}
			if (DCC_Chats[c].waiting_for_password) {
				var dcc_pwd;
				if (dcc_pwd=DCC_Chats[c].sock.readln()) {
					var usr = new User(system.matchuser(DCC_Chats[c].id));
					if (!usr ||
						(dcc_pwd.toUpperCase() != usr.security.password)) {
						DCC_Chats[c].o(null,"Access Denied.");
						DCC_Chats[c].sock.close();
						delete DCC_Chats[c];
						continue;
					}
					if (dcc_pwd.toUpperCase() == usr.security.password) {
						DCC_Chats[c].waiting_for_password = false;
						DCC_Chats[c].o(null,"Welcome aboard.");
					}
				}
				continue;
			}
			var line = DCC_Chats[c].sock.readln();
			if (!line || line == "")
				continue;
			var usr = new User(system.matchuser(DCC_Chats[c].id));
			var cmd = line.split(" ");
			cmd[0] = cmd[0].toUpperCase();
			try {
				DCC_Chats[c].check_bot_command(cmd);
			} catch (err) {
				DCC_Chats[c].o(null,"ERROR: " + err);
			}
		}

		if ( (time() - Config_Last_Write) > config_write_delay )
			save_everything();

		mswait(10); /* Don't peg the CPU */
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
	this.lastout = 0;			// When it's OK to send the next socket ouput
	this.burst = 0;
	this.max_burst = 5;
	this.delay = 3;
	this.is_registered = false;
	this.juped = false;
	this.users = new Object();	// Store local nicks & uh info.
	// Functions
	this.ctcp = Server_CTCP;
	this.ctcp_reply = Server_CTCP_Reply;
	this.o = Server_target_out;
	this.writeout = Server_writeout;
	this.buffers=[];
	this.bot_access = Server_Bot_Access;
	this.ignore = new Array();
	
	this.check_bot_command = function(target,onick,ouh,cmd) {
		Server_check_bot_command(this,Bot_Commands,target,onick,ouh,cmd);
		for(var bot_cmd in Modules) {
			if(Modules[bot_cmd].enabled)
				Server_check_bot_command(this,Modules[bot_cmd].Bot_Commands,target,onick,ouh,cmd);
		}
	}

	this.server_command = function(cmdline,onick,ouh) {
		Server_command(this,cmdline,onick,ouh);
		for(var srv_cmd in Modules) {
			if(Modules[srv_cmd].Server_command) Modules[srv_cmd].Server_command(this,cmdline,onick,ouh);
		}
	}
}

function Bot_IRC_Channel(name) {
	// Statics.
	this.name = name;
	// Dynamics.
	this.is_joined = false;
	this.lastjoin = 0;
	// Functions.
}

function Server_User(uh,nick) {
	this.uh = uh;
	this.nick = nick;
	this.servername = undefined;
	this.ident = false;
	this.last_spoke = false;
	this.channels=[];
}

function Server_Buffer(target) {
	this.target=target;
	this.buffer=[];
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
	this.check_bot_command = function(cmd) {
		Server_check_bot_command(this,Bot_Commands,null,this.id,null,cmd);
		for(var bot_cmd in Modules) {
			Server_check_bot_command(this,Modules[bot_cmd].Bot_Commands,
				null,this.id,null,cmd
			);
		}
	}
}

function DCC_Out(target,str) {
	this.sock.write(str + "\r\n");
}

/* This must be at the very bottom. */
load("load/ircbot_commands.js");
main();
