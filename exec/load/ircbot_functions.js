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

 Copyright 2010 Randolph E. Sommerfeld <sysop@rrx.ca>

*/

/* Server methods */
function Server_command(srv,cmdline,onick,ouh) {
	var cmd=IRC_parsecommand(cmdline);
	var chan=get_command_channel(srv,cmd);
	var main_cmd=cmd.shift();
	if(Server_Commands[main_cmd]) Server_Commands[main_cmd](srv,cmd,onick,ouh);
	for(var m in Modules) {
		var module=Modules[m];
		if(chan	&& chan.modules[m] != true) {
			continue;
		}
		if(module 
			&& module.enabled 
			&& module.Server_Commands[main_cmd]) {
			try {
				cmd=IRC_parsecommand(cmdline);
				cmd.shift();
				module.Server_Commands[main_cmd](srv,cmd,onick,ouh);	
			} catch(e) {
				log(m + " module error: " + e);
				module.enabled=false;
			}
		}
	}
}

function Server_CTCP(onick,ouh,cmd) {
	switch (cmd[0]) {
		case "DCC":
			var usr = new User(system.matchuser(onick));
			if (!usr.number) {
				this.o(onick, "I don't talk to strangers.", "NOTICE");
				return;
			}
			if (cmd[4]) {
				if ((cmd[1].toUpperCase() == "CHAT")
					&& (cmd[2].toUpperCase() == "CHAT")
					&& (parseInt(cmd[3]) == cmd[3])
					&& (parseInt(cmd[4]) == cmd[4])) {
						var ip = int_to_ip(cmd[3]);
						var port = parseInt(cmd[4]);
						var sock = new Socket();
						sock.connect(ip, port, 3 /* Timeout */);
						if (sock.is_connected) {
							sock.write("Enter your password.\r\n");
							dcc_chats.push(new DCC_Chat(sock,onick));
						}
				}
			}
			break;
		case "PING":
			var reply = "PING ";
			if (parseInt(cmd[1]) == cmd[1]) {
				reply += cmd[1];
				if (cmd[2] && (parseInt(cmd[2]) == cmd[2]))
					reply += " " + cmd[2];
				this.ctcp_reply(onick, reply);
			}
			break;
		case "VERSION":
			this.ctcp_reply(onick, "VERSION "
				+ "Synchronet IRC Bot by Randy E. Sommerfeld <cyan@rrx.ca> & Matt D. Johnson <mdj1979@gmail.com>");
			break;
		case "FINGER":
			this.ctcp_reply(onick, "FINGER "
				+ "Finger message goes here.");
			break;
		default:
			break;
	}
	return;
}

function Server_CTCP_reply(nick,str) {
	this.writeout("NOTICE " + nick + " :\1" + str + "\1");
}

function Server_bot_command(srv,bot_cmds,target,onick,ouh,cmdline) {
	var cmd=IRC_parsecommand(cmdline);
	
	var access_level = srv.bot_access(onick,ouh);
	var botcmd = bot_cmds[cmd[0].toUpperCase()];
	if (botcmd) {
		if (botcmd.ident_needed && !srv.users[onick.toUpperCase()].ident) {
			srv.o(target,"You must be identified to use this command.");
			return 0;
		}
		if (access_level < botcmd.min_security) {
			srv.o(target,"You do not have sufficient access to this command.");
			return 0;
		}
		if ((botcmd.args_needed == true) && !cmd[1]) {
			srv.o(target,"Hey buddy, I need some arguments for this command.");
			return 0;
		} else if ((parseInt(botcmd.args_needed) == botcmd.args_needed)
					&& !cmd[botcmd.args_needed]) {
			srv.o(target,"Hey buddy, incorrect number of arguments provided.");
			return 0;
		}
		/* If we made it this far, we're good. */
		try {
			botcmd.command(target,onick,ouh,srv,access_level,cmd);
		} catch (err) {
			srv.o(target,err);
			srv.o(target,"file: " + err.fileName);
			srv.o(target,"line: " + err.lineNumber);
		}
		return 1;
	}
	return 0; /* No such command */
}

function Server_bot_access(nick,uh) { // return the access level of this user.
	var ucnick = nick.toUpperCase();
	if (this.users[ucnick] && this.users[ucnick].ident) {
		var usrnum = this.users[ucnick].ident;
		var thisuser = new User(usrnum);
		return thisuser.security.level;
	}
	var usrnum = system.matchuser(nick);
    if (!usrnum)
        return 0;
    var thisuser = new User(usrnum);
    for (m in Masks[usrnum]) {
        if (wildmatch(uh,Masks[usrnum][m]))
            return thisuser.security.level;
    }
    return 0; // assume failure
}

function Server_writeout(str) {
	log("--> " + this.host + ": " + str);
	
	var target="~";
	var target_buffer=false;
	
	for(t=0;t<this.buffers.length;t++) {
		if(this.buffers[t].target==target) {
			target_buffer=this.buffers[t];
			break;
		}
	}
	if(target_buffer) {
		target_buffer.buffer.push(str.slice(0, 512) + "\r\n");
	} else {
		new_buff=new Server_Buffer(target);
		new_buff.buffer.push(str.slice(0, 512) + "\r\n");
		this.buffers.push(new_buff);
	}
}

function Server_target_out(target,str,msgtype) {
	for (c in Squelch_List) {
		if (target.toUpperCase() == Squelch_List[c].toUpperCase())
			return;
	}

	if (!msgtype)
		msgtype = "PRIVMSG";

	var outstr = msgtype + " " + target + " :" + str;
	log("--> " + this.host + ": " + outstr);

	var target_buffer=false;
	for(t=0;t<this.buffers.length;t++) {
		if(this.buffers[t].target==target) {
			target_buffer=this.buffers[t];
			break;
		}
	}
	if(target_buffer) {
		target_buffer.buffer.push(outstr.slice(0, 512) + "\r\n");
	} else {
		new_buff=new Server_Buffer(target);
		new_buff.buffer.push(outstr.slice(0, 512) + "\r\n");
		this.buffers.push(new_buff);
	}
}

/* server functions */
function save_everything() { // save user data, and call save() method for all enabled modules
	if (!config.open("r+"))
		return false;

	config.iniSetValue(null, "command_prefix", command_prefix);
	config.iniSetValue(null, "real_name", real_name);
	config.iniSetValue(null, "config_write_delay", config_write_delay);
	config.iniSetValue(null, "squelch_list", Squelch_List.join(","));

	for (m in Masks) {
		var uid_str = format("%04u", m);
		var us_filename = system.data_dir + "user/" +uid_str+ ".ircbot.ini";
		var us_file = new File(us_filename);
		if (us_file.open(file_exists(us_filename) ? 'r+':'w+')) {
			us_file.iniSetValue(null, "masks", Masks[m].join(","));
			us_file.close();
		}
	}

	for (q in Quotes) {
		config.iniSetValue("quotes", q, Quotes[q]);
	}

	config.close();

	for (var m in Modules) {
		var module=Modules[m];
		if(module && module.enabled && module.save) {
			try {
				module.save();
			} catch(e) {
				log(m + " module error: " + e);
				module.enabled=false;
			}
		}
	}

	config_last_write = time();
	return true;
}

function get_command_channel(srv,cmd) {
	switch(cmd[0]) {
	case "PRIVMSG":
		break;
	case "PART":
	case "QUIT":
	case "KICK":
	case "JOIN":
		if (cmd[1][0] == ":")
			cmd[1] = cmd[1].substr(1);
		break;
	default:
		return false;
	}
	var chan_str = cmd[1].toUpperCase();
	var chan = srv.channel[chan_str];
	if (!chan)
		return false;
	return chan;
}

function parse_cmd_prefix(cmd) {
	var pre=truncsp(get_cmd_prefix());

	cmd[1] = cmd[1].substr(1).toUpperCase();
	if ((cmd[1] == pre) 
		 && cmd[2]) {
		cmd.shift();
		cmd.shift();
	} else if(cmd[1].search(new RegExp(pre+"\s")) == 0) {
		cmd.shift();
		cmd[0] = cmd[0].replace(new RegExp(pre+"\s*"));
	} else if(pre=="") {
		cmd.shift();
	} else {
		return false;
	}
	cmd[0] = cmd[0].toUpperCase();
	return cmd;
}

function get_cmd_prefix() {
	if(command_prefix) {
		if(command_prefix.length<=1) return command_prefix.toUpperCase()+"";
		return command_prefix.toUpperCase()+" ";
	}
	return "";
}

function parse_channel_list(str) {
	var channels=[];
	if(str) {
		str = str.split(",");
		for (var c in str) {
			var channel=str[c].split(" ");
			var name=channel[0];
			var key=channel[1];
			channels[name.toUpperCase()] = new Bot_IRC_Channel(name,key);
		}
	}
	return channels;
}

function true_array_len(my_array) {
	var counter = 0;
	for (i in my_array) {
		counter++;
	}
	return counter;
}

function login_user(usr) {
	usr.connection = "IRC";
	usr.logontime = time();
}
