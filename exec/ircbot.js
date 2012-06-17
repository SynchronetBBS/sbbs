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

/* Global Arrays */
var Modules = {};
var Bot_Servers = {};
var Quotes = [];
var Masks = {};
var DCC_Chats = [];
var Squelch_List = [];

/* Global Variables */
var command_prefix = "";
var real_name = "";
var config_write_delay = 300;
var config_last_write = time(); /* Store when the config was last written. */
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

function init() {
	load("load/ircbot_functions.js");

	if (!config.open("r")) {
		exit("Couldn't open config file!");
	}
	
	command_prefix = 
		config.iniGetValue(null, "command_prefix");
	real_name = 
		config.iniGetValue(null, "real_name");
	config_write_delay=parseInt(
		config.iniGetValue(null, "config_write_delay"));
//	Squelch_List = 
//		config.iniGetValue(null, "Squelch_List").split(",");

	/* load server & module data */
	init_servers(config);
	/* load module files */
	init_modules();
	/* initialize user settings */
	init_users();

	config.close();
	
	load("load/ircbot_commands.js");
}

function init_servers(config) {
	var server_channels=[];
	/* Modules */
	var ini_modules = config.iniGetSections("module_");
	for (var m in ini_modules) {
		var mysec = ini_modules[m];
		var module_name = mysec.replace("module_", "");
		log ("--- Adding Module: " + module_name);
		
		var lib_list=config.iniGetValue(mysec,"lib");
		var lib=new Array();
		if(lib_list) {
			lib_list=lib_list.split(",");
			for(var l in lib_list) lib.push(removeSpaces(lib_list[l]));
		}
		
		var dir=backslash(config.iniGetValue(mysec,"dir"));
		var load_list=directory(dir+"*.js");
		var global=config.iniGetValue(mysec,"global");
		var channels=parse_channel_list(config.iniGetValue(mysec, "channels"));
		for(var c in channels) {
			if(!server_channels[c] && c[0] != "!") {
				server_channels[c]=channels[c];
				log (" --- Adding Channel: " + c);
			}
		}
		Modules[module_name.toUpperCase()]=new Bot_Module(
			module_name,
			dir,
			load_list,
			global,
			channels,
			lib
		);		
	}
	
	/* Servers */
	var ini_server_secs = config.iniGetSections("server_");
	for (s in ini_server_secs) {
		var mysec = ini_server_secs[s];
		var network = mysec.replace("server_", "");
		log ("--- Adding Server: " + network);
		
		var address = config.iniGetValue(mysec, "addresses");
		var port = Number(config.iniGetValue(mysec,"port"));
		var nick = config.iniGetValue(mysec, "nick");
		var pw = config.iniGetValue(mysec, "services_password");
		var channels = parse_channel_list(config.iniGetValue(mysec, "channels"));
		
		for(var c in server_channels) {
			if(!channels[c]) {
				channels[c]=server_channels[c];
			}
		}
		for(var m in Modules) {
			if(Modules[m].global == true) {
				for(var c in channels) {
					if(Modules[m].channels["!" + c] == true) {
						channels[c].modules[m]=false;
					} else {
						channels[c].modules[m]=true;
					}
				}
			} else {
				for(var c in Modules[m].channels) {
					if(c[0] == "!") {
						if(channels[c.substr(1)]) {
							channels[c.substr(1)].modules[m]=false;
						}
					} else {
						channels[c].modules[m]=true;
					}
				}
			}
		}
		
		Bot_Servers[network.toUpperCase()] = (new Bot_IRC_Server(
			0,  /* Socket */
			address,
			nick,
			pw,
			channels,
			port,
			network
		));
	}
}

function init_modules() {
	for(var m in Modules) {
		for(var l in Modules[m].lib) {
			if(Modules[m].lib[l]) load(Modules[m],Modules[m].lib[l]);
		}
		for(var l in Modules[m].load) {
			if(Modules[m].load[l]) load(Modules[m],Modules[m].load[l]);
		}
	}
}

function init_users() {
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
}

function main() {
	log("*** Entering Main Loop. ***");
	while (!js.terminated) {
		// Build array of sockets for select()
		var socks=[];
		for (my_srv in Bot_Servers) {
			var srv=Bot_Servers[my_srv];
			if (!srv.sock &&(srv.lastcon <time())) { //we're not connected.
				var consock = IRC_client_connect(srv.host, srv.nick,
					srv.nick, real_name, srv.port);
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
			}
			else if(srv.sock) {
				// Run through some commands.
				if (srv.is_registered) {
					if(!srv.is_identified) {
						/* If we have a password for services, send it now */
						if(srv.svspass) srv.writeout("IDENTIFY " + srv.svspass);
						// TODO: verify that the raw IDENTIFY command works, and if not, send /MSG NickServ IDENTIFY <Pass>
						srv.is_identified = true;
					}
					for (c in srv.channel) {
						if (!srv.channel[c].is_joined &&
							(srv.channel[c].lastjoin < time())) {
							if(srv.channel[c].key) srv.writeout("JOIN " + srv.channel[c].name + " " + srv.channel[c].key);
							else srv.writeout("JOIN " + srv.channel[c].name);
							srv.channel[c].lastjoin = time() + 60;
						}
						// Cycle any available module "main" functions
						for(var m in Modules) {
							var module=Modules[m];
							if(module && module.enabled && module.main) {
								try {
									module.main(srv,srv.channel[c].name);
								} catch(e) {
									srv.o(srv.channel[c].name,m.toLowerCase() + " module error: " + e,"NOTICE");
									module.enabled=false;
								}
							}
						}
					}
				}
			
				// Cycle server output buffer (srv.buffer)
				while(srv.burst<srv.max_burst) {
					srv.burst++;
					if(!srv.buffers.length>0) break;
					var next_output=srv.buffers.shift();
					if(!srv.sock.write(next_output.buffer.shift())) {
						srv.sock.close();
						srv.sock=0;
						continue;
					}
					if(next_output.buffer.length) srv.buffers.push(next_output);
				}
				var curtime=system.timer;
				if ((curtime-srv.lastout)>srv.delay) {
					srv.lastout=system.timer;
					srv.burst=0;
				}

				// Push into the select array
				srv.sock.srv=my_srv;
				socks.push(srv.sock);
			}
		}
		for (c in DCC_Chats) {
			DCC_Chats[c].sock.chat=c;
			socks.push(DCC_Chats[c].sock);
		}
		var ready=socket_select(socks, 0.1);

		for(var s in ready) {
			if(socks[s].srv != undefined) {
				var srv=Bot_Servers[socks[s].srv];
				if(srv.sock && (!srv.sock.is_connected)) {
					srv.sock.close();
					srv.sock=0;
				}
				if (srv.sock && srv.sock.poll(0.1) &&
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
						srv.server_command(srv,cmdline,onick,ouh);
					}
				}

			}
			if(socks[s].chat != undefined) {
				var c=socks[s].chat;
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
					DCC_Chats[c].bot_command(cmd);
				} catch (err) {
					DCC_Chats[c].o(null,err);
					DCC_Chats[c].o(null,"file: " + err.fileName);
					DCC_Chats[c].o(null,"line: " + err.lineNumber);
				}
			}
		}

		if ( (time() - config_last_write) > config_write_delay )
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

	// Dynamic variables (used for the bot's state.
	this.curnick = nick;
	this.lastcon = 0;			// When it's OK to reconnect.
	this.lastout = 0;			// When it's OK to send the next socket ouput
	this.burst = 0;
	this.max_burst = 5;
	this.delay = 3;
	this.is_registered = false;
	this.is_identified = false;
	this.juped = false;
	this.users = {};	// Store local nicks & uh info.
	this.buffers= [];
	this.ignore = [];
	this.channel = channels;

	// Functions
	this.ctcp = Server_CTCP;
	this.ctcp_reply = Server_CTCP_reply;
	this.o = Server_target_out;
	this.writeout = Server_writeout;
	this.bot_access = Server_bot_access;
	this.bot_command = Server_bot_command;
	this.server_command = Server_command;
}

function Bot_IRC_Channel(name,key) {
	// Statics.
	this.name = name;
	this.key = key;
	// Dynamics.
	this.is_joined = false;
	this.lastjoin = 0;
	// Modules active in this channel (associative)
	this.modules = [];
	// Functions.
}

function Bot_Module(name,dir,load,global,channels,lib) {
	this.name=name;
	this.dir=dir;
	this.load=load;
	this.global=global;
	this.channels=channels;
	this.lib=lib;
	this.enabled=true;
	this.Bot_Commands = {};
	this.Server_Commands = {};
}

function Server_User(uh,nick) {
	this.uh = uh;
	this.nick = nick;
	this.servername = undefined;
	this.ident = false;
	this.last_spoke = false;
	this.channels = [];
}

function Server_Buffer(target) {
	this.target = target;
	this.buffer = [];
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
	this.bot_command = function(cmd) {
		Server_bot_command(this,Bot_Commands,null,this.id,null,cmd);
		for(var bot_cmd in Modules) {
			Server_bot_command(
				this,
				Modules[bot_cmd].Bot_Commands,
				null,
				this.id,
				null,
				cmd
			);
		}
	}
}

function DCC_Out(target,str) {
	this.sock.write(str + "\r\n");
}

init();
main();
