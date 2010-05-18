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

/********** Command Processors. **********/
function Server_command(srv,cmdline,onick,ouh) {
	var cmd=IRC_parsecommand(cmdline);
	switch (cmd[0]) {
		case "001":	// "Welcome."
			srv.is_registered = true;
			break;
		case "352":	// WHO reply.  Process into local cache.
			var nick = cmd[6].toUpperCase();
			if(!srv.users[nick]) srv.users[nick] = new Server_User(cmd[3] + "@" + cmd[4]);
			else srv.users[nick].uh=cmd[3] + "@" + cmd[4];
			srv.users[nick].channels[cmd[2].toUpperCase()]=true;
			break;
		case "433":	// Nick already in use.
			srv.juped = true;
			var newnick = srv.nick+"-"+random(50000).toString(36);
			srv.writeout("NICK " + newnick);
			srv.curnick = newnick;
			log("*** Trying alternative nick, my nick is jupitered.  "
				+ "(Temp: " + newnick + ")");
			break;
		case "JOIN":
			if (cmd[1][0] == ":")
				cmd[1] = cmd[1].slice(1);
			var chan = srv.channel[cmd[1].toUpperCase()];
			if ((onick == srv.curnick) && chan && !chan.is_joined) {
				chan.is_joined = true;
				srv.writeout("WHO " + cmd[1]);
				break;
			}
			// Someone else joining.
			if(!srv.users[onick.toUpperCase()])	srv.users[onick.toUpperCase()] = new Server_User(ouh);
			else srv.users[onick.toUpperCase()].uh=ouh;
			srv.users[onick.toUpperCase()].channels[cmd[1].toUpperCase()]=true;
			var lvl = srv.bot_access(onick,ouh);
			if (lvl >= 50) {
				var usr = new User(system.matchuser(onick));
				if (lvl >= 60)
					srv.writeout("MODE " + cmd[1] + " +o " + onick);
				if (usr.number > 0) {
					if (usr.comment)
						srv.o(cmd[1],"[" + onick + "] " + usr.comment);
					login_user(usr);
				}
			}
			break;
		case "PART":
		case "QUIT":
		case "KICK":
			if (cmd[1][0] == ":")
				cmd[1] = cmd[1].slice(1);
			var chan = srv.channel[cmd[1].toUpperCase()];
			if ((onick == srv.curnick) && chan && chan.is_joined) {
				chan.is_joined = false;
				break;
			}
			// Someone else parting.
			if(srv.users[onick.toUpperCase()]) {
				delete srv.users[onick.toUpperCase()].channels[cmd[1].toUpperCase()];
				var chan_count=0;
				for(var c in srv.users[onick.toUpperCase()].channels) {
					chan_count++;
				}
				if(chan_count==0) delete srv.users[onick.toUpperCase()];
			}
			break;
		case "PRIVMSG":
			if ((cmd[1][0] == "#") || (cmd[1][0] == "&")) {
				var chan = srv.channel[cmd[1].toUpperCase()];
				if (!chan)
					break;
				if (!chan.is_joined)
					break;
				cmd[2] = cmd[2].substr(1).toUpperCase();
				if ((cmd[2].toUpperCase() == truncsp(get_cmd_prefix())) 
					 && cmd[3]) {
					cmd[3] = cmd[3].toUpperCase();
					cmd.shift();
					cmd.shift();
					cmd.shift();
					srv.check_bot_command(chan.name,onick,ouh,cmd.join(" "));
				} else if(cmd[2][0] == truncsp(get_cmd_prefix())) {
					cmd.shift();
					cmd.shift();
					cmd[0] = cmd[0].substr(1).toUpperCase();
					srv.check_bot_command(chan.name,onick,ouh,cmd.join(" "));
				} else if(get_cmd_prefix()=="") {
					cmd.shift();
					cmd.shift();
					cmd[0] = cmd[0].toUpperCase();
					srv.check_bot_command(chan.name,onick,ouh,cmd.join(" "));
				}
			} else if (cmd[1].toUpperCase() == 
			           srv.curnick.toUpperCase()) { // MSG?
				cmd[2] = cmd[2].slice(1).toUpperCase();
				cmd.shift();
				cmd.shift();
				if (cmd[0][0] == "\1") {
					cmd[0] = cmd[0].slice(1).toUpperCase();
					cmd[cmd.length-1] = cmd[cmd.length-1].slice(0,-1);
					srv.ctcp(onick,ouh,cmd);
					break;
				}
				srv.check_bot_command(onick,onick,ouh,cmd.join(" "));
			}
			break;
		case "PING":
			srv.writeout("PONG :" + IRC_string(cmdline));
			break;
		case "ERROR":
			srv.sock.close();
			srv.sock = 0;
			break;
		default:
			break;
	}
}

function Server_CTCP(onick,ouh,cmd) {
	switch (cmd[0]) {
		case "DCC":
			if (cmd[4]) {
				log("cmd1:" + cmd[1] + ":");
				log("cmd2:" + cmd[2] + ":");
				log("cmd3:" + cmd[3] + ":");
				log("cmd4:" + cmd[4] + ":");
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
				+ "Synchronet IRC Bot by Randy E. Sommerfeld <cyan@rrx.ca>");
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

function Server_CTCP_Reply(nick,str) {
	this.writeout("NOTICE " + nick + " :\1" + str + "\1");
}

function Server_check_bot_command(srv,bot_cmds,target,onick,ouh,cmdline) {
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
		botcmd.command(target,onick,ouh,srv,access_level,cmd);
		return 1;
	}
	return 0; /* No such command */
}

//////////////////// Non-object Functions ////////////////////

/* Save everything */
function save_everything() {
	if (!config.open("r+"))
		return false;

	config.iniSetValue(null, "command_prefix", command_prefix);
	config.iniSetValue(null, "real_name", real_name);
	config.iniSetValue(null, "config_write_delay", config_write_delay);
	config.iniSetValue(null, "squelch_list", squelch_list.join(","));

	for (m in masks) {
		var uid_str = format("%04u", m);
		var us_filename = system.data_dir + "user/" +uid_str+ ".ircbot.ini";
		var us_file = new File(us_filename);
		if (us_file.open(file_exists(us_filename) ? 'r+':'w+')) {
			us_file.iniSetValue(null, "masks", masks[m].join(","));
			us_file.close();
		}
	}

	for (q in quotes) {
		config.iniSetValue("quotes", q, quotes[q]);
	}

	config.close();

	for (var m in Modules) {
		if(Modules[m].save) Modules[m].save();
	}

	Config_Last_Write = time();
	return true;
}

function get_cmd_prefix() {
	if(command_prefix) {
		if(command_prefix.length<=1) return command_prefix.toUpperCase()+"";
		return command_prefix.toUpperCase()+" ";
	}
	return "";
}

// return the access level of this user.
function Server_Bot_Access(nick,uh) {
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
    for (m in masks[usrnum]) {
        if (wildmatch(uh,masks[usrnum][m]))
            return thisuser.security.level;
    }
    return 0; // assume failure
}

function Server_writeout(str) {
	log("--> " + this.host + ": " + str);
	this.sock.write(str.slice(0, 512) + "\r\n");
}

function Server_target_out(target,str) {
	for (c in squelch_list) {
		if (target.toUpperCase() == squelch_list[c].toUpperCase())
			return;
	}
	var outstr = "PRIVMSG " + target + " :" + str;
	log("--> " + this.host + ": " + outstr);
	this.sock.write(outstr.slice(0, 512) + "\r\n");
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
