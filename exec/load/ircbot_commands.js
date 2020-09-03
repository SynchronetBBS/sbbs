// $Id: ircbot_commands.js,v 1.37 2015/03/04 18:25:38 mcmlxxix Exp $
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

 Synchronet IRC Daemon as per RFC 1459, link compatible with Bahamut

 Copyright 2010 Randolph Erwin Sommerfeld <sysop@rrx.ca>

 An IRC bot written in JS that interfaces with the local BBS.

*/

/* Main bot commands */
var Bot_Commands = new Object();

Bot_Commands["RELOAD"] = new Bot_Command(50,false,true);
Bot_Commands["RELOAD"].usage =
	command_prefix?command_prefix.toUpperCase():""+" RELOAD";
Bot_Commands["RELOAD"].help = 
	"Reloads the internal bot command and function structure.  No arguments.";
Bot_Commands["RELOAD"].command = function (target,onick,ouh,srv,lvl,cmd) {
	load("load/ircbot_commands.js");
	load("load/ircbot_functions.js");
	for(var m in Modules) {
		/* Don't reload libraries??
		for(var l in Modules[m].lib) {
			if(Modules[m].lib[l]) load(Modules[m],Modules[m].lib[l]);
		} */
		for(var l in Modules[m].load) {
			if(Modules[m].load[l]) load(Modules[m],Modules[m].load[l]);
		}
	}
	srv.o(target,"Reloaded.","NOTICE");
	return;
}

Bot_Commands["LOAD"] = new Bot_Command(90,false,true);
Bot_Commands["LOAD"].command = function (target,onick,ouh,srv,lvl,cmd) {
	cmd.shift();
	var fName = cmd.shift();
	load("" + fName,cmd[0],cmd[1],cmd[2],cmd[3],cmd[4]);
	srv.o(target,"loaded " + fName,"NOTICE");
	return;
}

Bot_Commands["JOIN"] = new Bot_Command(99,true,true);
Bot_Commands["JOIN"].command = function (target,onick,ouh,srv,lvl,cmd) {
	cmd.shift();
	if(cmd.length == 0 || (cmd[0][0]!="#" && cmd[0][0]!="&")) {
		srv.o(target,"Invalid channel name","NOTICE");
		return;
	}
	var chan=cmd.shift().toUpperCase();
	var key=cmd.shift();
	
	if(srv.channel[chan]) {
		srv.o(target,"I am already in that channel","NOTICE");
		return;
	}
	srv.channel[chan]=new Bot_IRC_Channel(chan,key);
	srv.writeout("WHO " + chan);
	srv.o(target,"Ok.","NOTICE");
	return;
}

Bot_Commands["PART"] = new Bot_Command(99,true,true);
Bot_Commands["PART"].command = function (target,onick,ouh,srv,lvl,cmd) {
	cmd.shift();
	if(cmd[0][0]!="#" && cmd[0][0]!="&") {
		srv.o(target,"Invalid channel name","NOTICE");
		return;
	}
	var chan=cmd[0].toUpperCase();
	if(!srv.channel[chan]) {
		srv.o(target,"I am not in that channel","NOTICE");
		return;
	}
	srv.writeout("PART " + chan + " :" + onick + " has asked me to leave."); 
	delete srv.channel[chan];
	srv.o(target,"Ok.","NOTICE");
	return;
}

Bot_Commands["DIE"] = new Bot_Command(90,false,true);
Bot_Commands["DIE"].usage =
	get_cmd_prefix() + "DIE";
Bot_Commands["DIE"].help =
	"Causes me to die. You don't want that, do you?";
Bot_Commands["DIE"].command = function (target,onick,ouh,srv,lvl,cmd) {
	for (s in Bot_Servers) {
		Bot_Servers[s].writeout("QUIT :" + onick + " told me to die. :(");
	}
	js.terminated=true;
	return;
}

Bot_Commands["RESTART"] = new Bot_Command(90,false,true);
Bot_Commands["RESTART"].command = function (target,onick,ouh,srv,lvl,cmd) {
	for (s in Bot_Servers) {
		Bot_Servers[s].writeout("QUIT :Restarting as per " + onick);
	}
	exit();
	return;
}

Bot_Commands["HELP"] = new Bot_Command(0,false,false);
Bot_Commands["HELP"].usage =
	get_cmd_prefix() + "HELP <command>";
Bot_Commands["HELP"].help =
	"Displays helpful information about bot commands.";
Bot_Commands["HELP"].command = function (target,onick,ouh,srv,lvl,cmd) {
	function help_out(bot_cmd) {
		if(bot_cmd) {
			if(bot_cmd.usage) srv.o(onick,format("Usage: " + bot_cmd.usage,srv.curnick),"NOTICE");
			if(bot_cmd.min_security>0) srv.o(onick,"Access level: " + bot_cmd.min_security,"NOTICE");
			if(bot_cmd.help) srv.o(onick,"Help: " + bot_cmd.help,"NOTICE");
			else srv.o(onick,"No help available for this command.","NOTICE");
			return true;
		} 
		return false;
	}
	function list_out(bot_cmds,name) {
		var cmdstr="";
		for(var c in bot_cmds) {
			if(bot_cmds[c].no_help)
				continue;
			if(lvl>=bot_cmds[c].min_security) cmdstr+=","+c;
		}
		srv.o(onick,"[" + name + "] " + cmdstr.substr(1).toLowerCase(),"NOTICE");
	}
	
	var hlp_main=cmd[1];
	/* if no module or command was specified, list all commands, and general command usage */
	if(!hlp_main) {
		srv.o(onick,"Usage: HELP <module> <command> | HELP <command>","NOTICE");
		list_out(Bot_Commands,"main");
		for(var m in Modules) {
			list_out(Modules[m].Bot_Commands,m.toLowerCase());
		}
	/* if there is a module matching the first command parameter 
		display help information for that module */
	} else	if(Modules[hlp_main.toUpperCase()]) {
		var module=Modules[hlp_main.toUpperCase()];
		var hlp_cmd=cmd[2];
		
		/* if a command was specified, list that command's help info */
		if(hlp_cmd) {
			if(module.Bot_Commands.HELP) {
				module.Bot_Commands.HELP.command(target,onick,ouh,srv,lvl,cmd);
			} else {
				help_out(module.Bot_Commands[hlp_cmd.toUpperCase()]);
			}
		/* if no command was specified, list module's commands */
		} else {
			list_out(module.Bot_Commands,hlp_main.toLowerCase());
		}
	} else {
		/* if there is help information for the command specified show it */
		if(Bot_Commands[hlp_main.toUpperCase()]) 
			help_out(Bot_Commands[hlp_main.toUpperCase()]);
		
		/* otherwise if this is not a private message, check the current channel's
			ACTIVE modules for a command that matches */
		else if(target != onick) {
			var chan=srv.channel[target.toUpperCase()];
			for(var m in chan.modules) {
				var module=Modules[m];
				if(module.Bot_Commands.HELP) 
					module.Bot_Commands.HELP.command(target,onick,ouh,srv,lvl,cmd);
				else if(module.Bot_Commands[hlp_main.toUpperCase()]) 
					help_out(module.Bot_Commands[hlp_main.toUpperCase()]);
			}
		}
	}
	return;
}
Bot_Commands["?"] = Bot_Commands["HELP"];

Bot_Commands["IDENT"] = new Bot_Command(0,true,false);
Bot_Commands["IDENT"].usage =
	"/MSG %s IDENT <nick> <pass>";
Bot_Commands["IDENT"].help =
	"Identifies a user by alias and password. Use via private message only.";
Bot_Commands["IDENT"].command = function (target,onick,ouh,srv,lvl,cmd) {
	var usr = new User(system.matchuser(onick));
	if (cmd[2]) { /* Username passed */
		usr = new User(system.matchuser(cmd[1]));
		cmd[1] = cmd[2];
	}
	if (!usr.number) {
		srv.o(target,"No such user.","NOTICE");
		return;
	}
	if ((target[0] == "#") || (target[0] == "&")) {
		if (lvl >= 50) {
			srv.o(target,"Fool!  You've just broadcasted your password to "
				+ "a public channel!  Because of this, I've reset your "
				+ "password.  Pick a new password, then /MSG " + srv.nick + " "
				+ "PASS <newpass>","NOTICE");
			usr.security.password = "";
		} else {
			srv.o(target,"Is broadcasting a password to a public channel "
				+ "really a smart idea?","NOTICE");
		}
		return;
	}
	if (usr.security.password == "") {
		srv.o(target,"Your password is blank.  Please set one with /MSG "
			+ srv.nick + " PASS <newpass>, and then use IDENT.","NOTICE");
		return;
	}
	if (cmd[1].toUpperCase() == usr.security.password) {
		srv.o(target,"You are now recognized as user '" + usr.alias + "'","NOTICE");
		srv.users[onick.toUpperCase()].ident = usr.number;
		login_user(usr);
		return;
	}
	srv.o(target,"Incorrect password","NOTICE");
	return;
}

Bot_Commands["ADDQUOTE"] = new Bot_Command(80,true,false);
Bot_Commands["ADDQUOTE"].command = function (target,onick,ouh,srv,lvl,cmd) {
	cmd.shift();
	var the_quote = cmd.join(" ");
	Quotes.push(the_quote);
	srv.o(target,"Thanks for the quote!");
	return;
}

Bot_Commands["QUOTE"] = new Bot_Command(0,false,false);
Bot_Commands["QUOTE"].command = function (target,onick,ouh,srv,lvl,cmd) {
	if(Quotes.length == 0) {
		srv.o(target,"I have no quotes. :(");
		return;
	}
	if (cmd[1]) {
		cmd.shift();
		var searched_quotes = new Object();
		var search_params = cmd.join(" ");
		var lucky_number;
		while (true_array_len(searched_quotes) < Quotes.length) {
			lucky_number = random(Quotes.length);
			if (!searched_quotes[lucky_number]) {
				if (Quotes[lucky_number].toUpperCase().match(search_params.toUpperCase())) {
					srv.o(target, Quotes[lucky_number]);
					return;
				}
				searched_quotes[lucky_number] = true;
			}
		}
		srv.o(target,"Couldn't find a quote that matches your criteria.");
		return;
	}
	srv.o(target, Quotes[random(Quotes.length)]);
	return;
}

Bot_Commands["GREET"] = new Bot_Command(50,false,false);
Bot_Commands["GREET"].usage =
	get_cmd_prefix() + "GREET <greeting>, " +
	get_cmd_prefix() + "GREET CLEAR, " +
	get_cmd_prefix() + "GREET";
Bot_Commands["GREET"].help =
	"Sets or clears the greeting I will display when you enter the room.";
Bot_Commands["GREET"].command = function (target,onick,ouh,srv,lvl,cmd) {
	var usr = new User(system.matchuser(onick));
	if (!usr.number) {
		srv.o(target,"You don't exist.","NOTICE");
		return;
	}
	if (cmd[1]) {
		if (cmd[1].toUpperCase() == "DELETE" ||
			cmd[1].toUpperCase() == "DEL" ||
			cmd[1].toUpperCase() == "CLEAR") {
			srv.o(target,"Your greeting has been erased.","NOTICE");
			usr.comment = "";
			return;
		}
		if (cmd[1].toUpperCase() == "NULL") 
			cmd[1] = "";
		cmd.shift();
		var the_greet = cmd.join(" ");
		srv.o(target,"Your greeting has been set.","NOTICE");
		usr.comment = the_greet;
		return;
	} else {
		if(usr.comment.length) srv.o(target,"[" + onick + "] " + usr.comment);
		else srv.o(target,"You have not defined a greeting.","NOTICE");
		return;
	}
	return;
}

Bot_Commands["SAVE"] = new Bot_Command(80,false,true);
Bot_Commands["SAVE"].command = function (target,onick,ouh,srv,lvl,cmd) {
	if (save_everything()) {
		srv.o(target,"Data successfully written.  Congratulations.");
	} else {
		srv.o(target,"Oops, couldn't write to disk.  Sorry, bud.");
	}
	return;
}

Bot_Commands["PREFIX"] = new Bot_Command(80,false,true);
Bot_Commands["PREFIX"].usage =
	get_cmd_prefix() + "PREFIX <command prefix>, " + 
	get_cmd_prefix() + "PREFIX";
Bot_Commands["PREFIX"].help =
	"Changes the bot command prefix.";
Bot_Commands["PREFIX"].command = function (target,onick,ouh,srv,lvl,cmd) {
	cmd.shift();
	if (cmd[0]) {
		command_prefix = cmd[0].toUpperCase();
		srv.o(target,"Bot command prefix set: " + command_prefix);
	} else {
		command_prefix = "";
		srv.o(target,"Bot command prefix cleared");
	}
	return;
}

Bot_Commands["NICK"] = new Bot_Command(80,1,true);
Bot_Commands["NICK"].command = function (target,onick,ouh,srv,lvl,cmd) {
	cmd.shift();
	if (cmd[0]) {
		srv.writeout("NICK " + cmd[0]);
		srv.curnick = cmd[0];
	}
	return;
}

Bot_Commands["MODULE"] = new Bot_Command(80,true,true);
Bot_Commands["MODULE"].usage =
	get_cmd_prefix() + "MODULE [ALL or #<chan> #<chan>...] [+/-ALL or +/-<module> +/-<module>...], " +
	"-ex: '" + get_cmd_prefix() + "MODULE #bbs #synchronet +ALL', " +
	"-ex: '" + get_cmd_prefix() + "MODULE ALL -poker +admin -trivia'";
Bot_Commands["MODULE"].help =
	"Toggle the status of modules in channels.";
Bot_Commands["MODULE"].command = function (target,onick,ouh,srv,lvl,cmd) {
	cmd.shift();

	if (cmd[0].toUpperCase() == "ENABLE") {
		var mod=Modules[cmd[1].toUpperCase()];
		if (mod) {
			if(!mod.enabled) {
				mod.enabled=true;
				srv.o(target,cmd[1].toUpperCase() + " module enabled.");
			} else {
				srv.o(target,cmd[1].toUpperCase() + " module is already enabled.");
			}
		} else {
			srv.o(target,"No such module.");
		}
		return;
	}

	if (cmd[0].toUpperCase() == "DISABLE") {
		var mod=Modules[cmd[1].toUpperCase()];
		if (mod) {
			if(mod.enabled) {
				mod.enabled=false;
				srv.o(target,cmd[1].toUpperCase() + " module disabled.");
			} else {
				srv.o(target,cmd[1].toUpperCase() + " module is already disabled.");
			}
		} else {
			srv.o(target,"No such module.");
		}
		return;
	}

	if (cmd[0].toUpperCase() == "LIST") {
		cmd.shift();
		var chan=cmd.shift();
		if(chan) {
			if(!srv.channel[chan.toUpperCase()]) {
				srv.o(target,"I am not in that channel");
				return;
			}
			chan=srv.channel[chan.toUpperCase()];
		} else if(target == onick) {
			var str="modules (msg):";
			for(m in Modules) {
				if(Modules[m].enabled) 
					str+=" +" + m.toLowerCase();
			}
			srv.o(target,str);
			return;
		} else {
			chan=srv.channel[target.toUpperCase()];
		}
	
		var str="modules (" + chan.name + "):";
		var mods=chan.modules;
		for(m in Modules) {
			if(Modules[m].enabled) {
				if(mods[m] == true) str+=" +" + m.toLowerCase();
				else str+=" -" + m.toLowerCase();
			}
		}
		srv.o(target,str);
		return;
	}

	/* If we get this far, then we're manipulating modules being enabled
	   or disabled on a per-channel basis. */

	/* first parameters should be either a channel name or ALL */
	var channels=[];
	var count=0;
	
	if(cmd[0].toUpperCase() == "ALL") {
		cmd.shift();
		for(var c in srv.channel) {
			channels.push(c);
		}
	} else {
		while(cmd[0] && cmd[0][0] == "#" || cmd[0][0] == "&") {
			var chan_str=cmd.shift();
			if(!srv.channel[chan_str.toUpperCase()]) {
				srv.o(target,"I am not in channel: " + chan_str.toLowerCase());
				return;
			}
			channels.push(chan_str.toUpperCase());
		}
	}
	
	var chan_list=[];
	var mod_list=[];
	
	var chan_str="";
	var mod_str="";
	
	var mode=cmd.shift();
	if(!mode) {
		srv.o(target,"Syntax error");
		return;
	}
	
	while(mode) {
		var operator = 0;
		if(mode[0] == "+") {
			operator = true;
		} else if (mode[0] == "-") {
			operator = false;
		} else {
			srv.o(target,"Syntax error: " + mode[0]);
			return;
		}
		var mod=mode.substr(1).toUpperCase();
		mode=mode[0];
		
		if(mod == "ALL") {
			for(var m in Modules) {
				if(channels.length == 0) {
					channels.push(target.toUpperCase());
				} 
				for(var c=0;c<channels.length;c++) {
					var chan=srv.channel[channels[c].toUpperCase()];
					if(chan.modules[m] !== operator) {
						if(!chan_list[chan.name]) {
							chan_list[chan.name]=true;
							chan_str+=" " + chan.name;
						}
						if(!mod_list[m]) {
							mod_list[m]=true;
							mod_str+=" " + mode + m.toLowerCase();
						}
						chan.modules[m] = operator;
						count++;
					}
				}
			}
		} else if(Modules[mod]) {
			if(channels.length == 0) {
				channels.push(target.toUpperCase());
			} 
			for(var c=0;c<channels.length;c++) {
				var chan=srv.channel[channels[c].toUpperCase()];
				if(chan.modules[mod] !== operator) {
					if(!chan_list[chan.name]) {
						chan_list[chan.name]=true;
						chan_str+=" " + chan.name;
					}
					if(!mod_list[mod]) {
						mod_list[mod]=true;
						mod_str+=" " + mode + mod.toLowerCase();
					}
					chan.modules[mod] = operator;
					count++;
				}
			}
		} else {
			srv.o(target,"No such module: " + mod.toLowerCase());
			return;
		}
		mode=cmd.shift();
	}
	if(count == 0) {
		srv.o(target,"No mode changes were made");
		return;
	}
	srv.o(target,"mode set:" + chan_str + mod_str);
	return;
}
Bot_Commands["MODULES"] = Bot_Commands["MODULE"];

Bot_Commands["ABORT"] = new Bot_Command(80,false,false);
Bot_Commands["ABORT"].command = function (target,onick,ouh,srv,lvl,cmd) {
	srv.buffers=[];
	srv.o(target,"Server output aborted.","NOTICE");
	return;
}


Bot_Commands["IGNORE"] = new Bot_Command(80,true,true);
Bot_Commands["IGNORE"].command = function (target,onick,ouh,srv,lvl,cmd) {
	cmd.shift();
	var usr=cmd[0].toUpperCase();
	if(!srv.users[usr]) {
		srv.o(target,usr + " is not in my database.","NOTICE");
		return;
	} 
	if(srv.ignore[usr]) {
		srv.o(target,usr + " removed from ignore list.","NOTICE");
		srv.ignore[usr]=false;
		return;
	}
	srv.o(target,usr + " added to ignore list.","NOTICE");
	srv.ignore[usr]=true;
	return;
}

/* Server Commands */
var Server_Commands = new Object();

Server_Commands["JOIN"]= function (srv,cmd,onick,ouh) {
	if (cmd[0][0] == ":")
		cmd[0] = cmd[0].slice(1);
	var chan = srv.channel[cmd[0].toUpperCase()];
	
	if(!chan) {
		log("channel not in server channel list");
		return;
	}
	
	// Me joining.
	if ((onick == srv.curnick) && !chan.is_joined) {
		chan.is_joined = true;
		srv.writeout("WHO " + cmd[0]);
		return;
	}
	
	// Someone else joining.
	if(!srv.users[onick.toUpperCase()])	srv.users[onick.toUpperCase()] = new Server_User(ouh,onick);
	else srv.users[onick.toUpperCase()].uh=ouh;
	srv.users[onick.toUpperCase()].channels[cmd[0].toUpperCase()]=true;
	var lvl = srv.bot_access(onick,ouh);
	if (lvl >= 50) {
		var usr = new User(system.matchuser(onick));
		if (lvl >= 60)
			srv.writeout("MODE " + cmd[0] + " +o " + onick);
		if (usr.number > 0) {
			if (usr.comment)
				srv.o(cmd[0],"[" + onick + "] " + usr.comment);
			login_user(usr);
		}
	}
}

Server_Commands["001"] = function (srv,cmd,onick,ouh)	{ // "Welcome."
	srv.is_registered = true;
}

Server_Commands["352"] = function (srv,cmd,onick,ouh)	{ // WHO reply.  Process into local cache.
	var nick = cmd[5];
	if(!srv.users[nick.toUpperCase()]) srv.users[nick.toUpperCase()] = new Server_User(cmd[2] + "@" + cmd[3],nick);
	else srv.users[nick.toUpperCase()].uh=cmd[2] + "@" + cmd[3];
	srv.users[nick.toUpperCase()].channels[cmd[1].toUpperCase()]=true;
}

Server_Commands["433"] = function (srv,cmd,onick,ouh)	{ // Nick already in use.
	srv.juped = true;
	var newnick = srv.nick+"-"+random(50000).toString(36);
	srv.writeout("NICK " + newnick);
	srv.curnick = newnick;
	log("*** Trying alternative nick, my nick is jupitered.  "
		+ "(Temp: " + newnick + ")");
}

Server_Commands["PART"] = function (srv,cmd,onick,ouh)	{
	if (cmd[0][0] == ":")
		cmd[0] = cmd[0].slice(1);
	var chan = srv.channel[cmd[0].toUpperCase()];
	if ((onick == srv.curnick) && chan && chan.is_joined) {
		chan.is_joined = false;
		return;
	}
	// Someone else parting.
	if(srv.users[onick.toUpperCase()]) {
		delete srv.users[onick.toUpperCase()].channels[cmd[0].toUpperCase()];
		var chan_count=0;
		for(var c in srv.users[onick.toUpperCase()].channels) {
			chan_count++;
		}
		if(chan_count==0) delete srv.users[onick.toUpperCase()];
	}
}
Server_Commands["QUIT"]=Server_Commands["PART"];

Server_Commands["KICK"] = function (srv,cmd,onick,ouh)	{
	if (cmd[0][0] == ":")
		cmd[0] = cmd[0].slice(1);
		
	var chan_name=cmd.shift();
	var kicked=cmd.shift();
	var chan = srv.channel[chan_name.toUpperCase()];
	
	if ((kicked == srv.curnick) && chan && chan.is_joined) {
		chan.is_joined = false;
		return;
	}
	// Someone else parting.
	if(srv.users[kicked.toUpperCase()]) {
		delete srv.users[kicked.toUpperCase()].channels[chan_name.toUpperCase()];
		var chan_count=0;
		for(var c in srv.users[kicked.toUpperCase()].channels) {
			chan_count++;
		}
		if(chan_count==0) delete srv.users[kicked.toUpperCase()];
	}
}

Server_Commands["PRIVMSG"] = function (srv,cmd,onick,ouh)	{ 
	if(!srv.users[onick.toUpperCase()])	
		srv.users[onick.toUpperCase()] = new Server_User(ouh,onick);
	srv.users[onick.toUpperCase()].last_spoke=time();

	if (cmd[0][0] == "#" || cmd[0][0] == "&") {
		var chan=srv.channel[cmd[0].toUpperCase()];
		if(!chan) return;
		
		if (srv.pipe && srv.pipe[chan.name.toUpperCase()]) {
			var thispipe = srv.pipe[chan.name.toUpperCase()];
			thispipe.srv.o(thispipe.target, "<" + onick + "> " 
				+ IRC_string(cmd.join(" "), 1));
		}
		
		cmd=parse_cmd_prefix(cmd);
		if(!cmd)
			return false;
		if(cmd.length == 0)
			return false;
		if(cmd[0].length == 0)
			return false;

		/* check main bot commands */
		try {
			srv.bot_command(srv,Bot_Commands,chan.name,onick,ouh,cmd.join(" "));
		} catch(e) {
			srv.o(chan.name,e);
		}
		
		for(var m in Modules) {
			var module=Modules[m];
			if(!module || !module.enabled) continue;
			if(chan.modules[m]) {
				try {
					srv.bot_command(srv,module.Bot_Commands,chan.name,onick,ouh,cmd.join(" "));
				} catch(e) {
					srv.o(chan.name,m.toLowerCase() + " bot command error: " + e,"NOTICE");
					module.enabled=false;
				}
			}
		}
	} else if (cmd[0].toUpperCase() == 
			   srv.curnick.toUpperCase()) { // MSG?
		cmd[1] = cmd[1].slice(1).toUpperCase();
		cmd.shift();
		if (cmd[0][0] == "\1") {
			cmd[0] = cmd[0].slice(1).toUpperCase();
			cmd[cmd.length-1] = cmd[cmd.length-1].slice(0,-1);
			srv.ctcp(onick,ouh,cmd);
			return;
		}
		srv.bot_command(srv,Bot_Commands,onick,onick,ouh,cmd.join(" "));
		
		for(var m in Modules) {
			var module=Modules[m];
			if(!module || !module.enabled) continue;
			
			try {
				srv.bot_command(srv,module.Bot_Commands,onick,onick,ouh,cmd.join(" "));
			} catch(e) {
				srv.o(onick,m.toLowerCase() + " bot command error: " + e,"NOTICE");
				module.enabled=false;
			}
		}
	}
}

Server_Commands["PING"] = function (srv,cmd,onick,ouh)	{ // "Ping."
	srv.writeout("PONG :" + cmd.join(" ").substr(1).toLowerCase());
}

Server_Commands["ERROR"] = function (srv,cmd,onick,ouh)	{ 
			srv.sock.close();
			srv.sock = 0;
}





