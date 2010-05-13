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

 Synchronet IRC Daemon as per RFC 1459, link compatible with Bahamut

 Copyright 2010 Randolph Erwin Sommerfeld <sysop@rrx.ca>

 An IRC bot written in JS that interfaces with the local BBS.

*/

Bot_Commands["RELOAD"] = new Bot_Command(50,false,false);
Bot_Commands["RELOAD"].usage =
	command_prefix?command_prefix.toUpperCase():""+" RELOAD";
Bot_Commands["RELOAD"].help = 
	"Reloads the internal bot command and function structure.  No arguments.";
Bot_Commands["RELOAD"].command = function (target,onick,ouh,srv,lvl,cmd) {
	load("load/ircbot_commands.js");
	load("load/ircbot_functions.js");
	for(var m in Modules) {
		for(var l in Modules[m].load) {
			load(Modules[m],Modules[m].dir + Modules[m].load[l]);
		}
	}
	srv.o(target,"Reloaded.");
	return;
}
Bot_Commands["JOINCHAN"] = new Bot_Command(99,true,true);
Bot_Commands["JOINCHAN"].command = function (target,onick,ouh,srv,lvl,cmd) {
	cmd.shift();
	if(cmd[0][0]!="#" && cmd[0][0]!="&") {
		srv.o(target,"Invalid channel name");
		return;
	}
	var chan=cmd[0].toUpperCase();
	if(srv.channel[chan]) {
		srv.o(target,"I am already in that channel");
		return;
	}
	srv.channel[chan]=new Bot_IRC_Channel(chan);
	srv.o(target,"Ok.");
	return;
}

Bot_Commands["PARTCHAN"] = new Bot_Command(99,true,true);
Bot_Commands["PARTCHAN"].command = function (target,onick,ouh,srv,lvl,cmd) {
	cmd.shift();
	if(cmd[0][0]!="#" && cmd[0][0]!="&") {
		srv.o(target,"Invalid channel name");
		return;
	}
	var chan=cmd[0].toUpperCase();
	if(!srv.channel[chan]) {
		srv.o(target,"I am not in that channel");
		return;
	}
	srv.writeout("PART " + chan + " :" + onick + " has asked me to leave."); 
	delete srv.channel[chan];
	srv.o(target,"Ok.");
	return;
}

Bot_Commands["DIE"] = new Bot_Command(90,false,false);
Bot_Commands["DIE"].usage =
	get_cmd_prefix() + "DIE";
Bot_Commands["DIE"].help =
	"Causes me to die. You don't want that, do you?";
Bot_Commands["DIE"].command = function (target,onick,ouh,srv,lvl,cmd) {
	for (s in bot_servers) {
		bot_servers[s].writeout("QUIT :" + onick + " told me to die. :(");
	}
	js.terminated=true;
	return;
}

Bot_Commands["RESTART"] = new Bot_Command(90,false,false);
Bot_Commands["RESTART"].command = function (target,onick,ouh,srv,lvl,cmd) {
	for (s in bot_servers) {
		bot_servers[s].writeout("QUIT :Restarting as per " + onick);
	}
	exit();
	return;
}

Bot_Commands["HELP"] = new Bot_Command(50,false,false);
Bot_Commands["HELP"].usage =
	get_cmd_prefix() + "HELP <command>";
Bot_Commands["HELP"].help =
	"Displays helpful information about bot commands.";
Bot_Commands["HELP"].command = function (target,onick,ouh,srv,lvl,cmd) {
	cmd.shift();
	function help_out(bot_cmd) {
		if(bot_cmd) {
			if(bot_cmd.usage) srv.o(onick,format("Usage: " + bot_cmd.usage,srv.curnick));
			if(bot_cmd.min_security>0) srv.o(onick,"Access level: " + bot_cmd.min_security);
			if(bot_cmd.help) srv.o(onick,"Help: " + bot_cmd.help);
			else srv.o(onick,"No help available for this command.");
			return true;
		} 
		return false;
	}
	function list_out(bot_cmds,name) {
		var cmdstr="";
		for(var c in bot_cmds) {
			if(lvl>=bot_cmds[c].min_security) cmdstr+=","+c;
		}
		srv.o(onick,"[" + name + " COMMANDS] " + cmdstr.substr(1));
	}
	if(cmd[0]) {
		var found_cmd=false;
		if(help_out(Bot_Commands[cmd[0].toUpperCase()])) {
			found_cmd=true;
		}
		for(var m in Modules) {
			if(help_out(Modules[m].Bot_Commands[cmd[0].toUpperCase()])) {
				found_cmd=true;
			}
		}
		if(!found_cmd) srv.o(onick,"No such command: " + cmd[0]);
	} else {
		srv.o(onick,"Command usage: " + get_cmd_prefix() + "<command> <arguments>");
		list_out(Bot_Commands,"MAIN");
		for(var m in Modules) {
			list_out(Modules[m].Bot_Commands,Modules[m].name.toUpperCase());
		}
	}
	return;
}
Bot_Commands["?"] = Bot_Commands["HELP"];

Bot_Commands["GREET"] = new Bot_Command(50,false,false);
Bot_Commands["GREET"].usage =
	get_cmd_prefix() + "GREET <greeting> or " +
	get_cmd_prefix() + "GREET CLEAR"; 
Bot_Commands["GREET"].help =
	"Sets or clears the greeting I will display when you enter the room.";
Bot_Commands["GREET"].command = function (target,onick,ouh,srv,lvl,cmd) {
	var usr = new User(system.matchuser(onick));
	if (!usr.number) {
		srv.o(target,"You don't exist.");
		return;
	}
	if (cmd[1]) {
		if (cmd[1].toUpperCase() == "DELETE" ||
			cmd[1].toUpperCase() == "DEL" ||
			cmd[1].toUpperCase() == "CLEAR") {
			srv.o(target,"Your greeting has been erased.");
			usr.comment = "";
			return;
		}
		if (cmd[1].toUpperCase() == "NULL") 
			cmd[1] = "";
		cmd.shift();
		var the_greet = cmd.join(" ");
		srv.o(target,"Your greeting has been set.");
		usr.comment = the_greet;
		return;
	} else {
		if(usr.comment.length) srv.o(target,"[" + onick + "] " + usr.comment);
		else srv.o(target,"You have not defined a greeting.");
		return;
	}
	return;
}

Bot_Commands["SAVE"] = new Bot_Command(80,false,false);
Bot_Commands["SAVE"].command = function (target,onick,ouh,srv,lvl,cmd) {
	if (save_everything()) {
		srv.o(target,"Data successfully written.  Congratulations.");
	} else {
		srv.o(target,"Oops, couldn't write to disk.  Sorry, bud.");
	}
	return;
}

Bot_Commands["PREFIX"] = new Bot_Command(80,false,false);
Bot_Commands["PREFIX"].command = function (target,onick,ouh,srv,lvl,cmd) {
	cmd.shift();
	if (cmd[0]) {
		command_prefix = cmd[0].toUpperCase();
		srv.o(target,"Bot command prefix set: " + command_prefix);
	} else {
		command_prefix = "";
		srv.o(target,"Bot command prefix cleared.");
	}
	return;
}
