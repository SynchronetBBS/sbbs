// $Id: admin_commands.js,v 1.18 2019/05/15 14:53:35 mcmlxxix Exp $
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

Bot_Commands["WHOIS"] = new Bot_Command(0,false,false);
Bot_Commands["WHOIS"].usage =
	get_cmd_prefix() + "WHOIS <nick>";
Bot_Commands["WHOIS"].help =
	"Brings up information about a user.  If the <nick> argument is omitted, "
	+ "then it will display information about you.";
Bot_Commands["WHOIS"].command = function (target,onick,ouh,srv,lvl,cmd) {
	if (!cmd[1]) {
		srv.o(target,"You are recognized as access level " + lvl);
		cmd[1] = onick;
	}
	var usr = new User(system.matchuser(cmd[1]));
	if (usr.number > 0) {
		srv.o(target,usr.alias+" has an access level of "
			+usr.security.level+". (UID: " + usr.number + ")");
		if (Masks[usr.number])
			srv.o(target,"Masks: " + Masks[usr.number].join(" "));
		else
			srv.o(target,usr.alias + " has no IRC masks defined.");
		srv.o(target,usr.alias + " last signed on "
			+ strftime("%m/%d/%Y %H:%M",usr.stats.laston_date) + " via "
			+ usr.connection + ".");
	} else {
		srv.o(target,"I have no such user in my database.");
	}
	return;
}

Bot_Commands["ADDMASK"] = new Bot_Command(50,1,false);
Bot_Commands["ADDMASK"].usage =
	get_cmd_prefix() + "ADDMASK <nick> <user>@<host>";
Bot_Commands["ADDMASK"].help =
	"Stores information used by the bot to identify you.";
Bot_Commands["ADDMASK"].command = function (target,onick,ouh,srv,lvl,cmd) {
	var addmask = false;
	var delmask = false;
	if (cmd[0] == "ADDMASK") {
		addmask = true;
	} else if (cmd[0] == "DELMASK") {
		delmask = true;
	}
	if (!addmask && !delmask) {
		srv.o(target,"Huh? I'm confused :(");
		return;
	}
	if (!cmd[2]) {
		cmd[2] = cmd[1];
		cmd[1] = onick;
	}
	if (!cmd[2].match(/[@]/)) {
		srv.o(target,"Typically, hostmasks need a '@' in them.");
		return;
	}
	var usr = new User(system.matchuser(cmd[1]));
	if (usr.number == 0) {
		srv.o(target,"That user doesn't exist!");
		return;
	}
	var self_change = (onick.toUpperCase() == cmd[1].toUpperCase());
	if ( (lvl < 80) && !self_change) {
		srv.o(target,"You do not have permission to change IRC masks "
			+ "for other users.");
		return;
	}
	if ((usr.security.level >= lvl) && !self_change) {
		srv.o(target,"You cannot add or delete masks for a user whose "
			+ "access level is equal to or greater than yours.");
		return;
	}
	if (addmask) {
		for (m in Masks[usr.number]) {
			if (wildmatch(cmd[2],Masks[usr.number][m])) {
				srv.o(target,"This user already has a mask matching that. "
					+ "Try deleting it with 'DELMASK' first.");
				return;
			}
		}
	}
	if (delmask) {
		for (m in Masks[usr.number]) {
			if (Masks[usr.number][m].toUpperCase() == cmd[2].toUpperCase()) {
				Masks[usr.number].splice(m,1);
				srv.o(target,"Mask deleted for user " + usr.alias + ": "
					+ cmd[2]);
				return;
			}
		}
		srv.o(target,"Couldn't find the mask you were looking for.");
	} else if (addmask) {
		if (!Masks[usr.number])
			Masks[usr.number] = new Array();
		Masks[usr.number].push(cmd[2]);
		srv.o(target,"Mask added for user " + usr.alias + ": " + cmd[2]);
	}
	return;
}
Bot_Commands["DELMASK"] = Bot_Commands["ADDMASK"];

Bot_Commands["ADDUSER"] = new Bot_Command(80,2,false);
Bot_Commands["ADDUSER"].command = function (target,onick,ouh,srv,lvl,cmd) {
	if (IRC_check_nick(cmd[1],40)) {
		srv.o(target,cmd[1] + " isn't a valid nickname.");
		return;
	}
	var usr = cmd[1].toUpperCase();
	var syncusr = new User(system.matchuser(cmd[1]));
	if (!srv.users[usr] && !cmd[2]) {
		srv.o(target,cmd[1] + " is not on any channels that I'm currently on. "
			+ "To force an add for this user, please specify a mask.");
		return;
	} else if (syncusr.number > 0) {
		srv.o(target,cmd[1] + " already exists in my database!");
		return;
	}
	var mask;
	var level = 50;
	if (cmd[2] && cmd[2].match(/[.]/)) {//2nd arg is a mask
		mask = cmd[2];
	} else if (cmd[2]) { // must be a level.
		if (!srv.users[usr] && !cmd[3]) {
			srv.o(target,cmd[1] + " is not on any channels that I'm "
				+ "currently on.  To force an add for this user, please "
				+ "specify a mask.");
			return;
		}
		level = parseInt(cmd[2]);
		if (level >= lvl) {
			srv.o(target,"You may only add users with a lower access level "
				+ "than your own.");
			return;
		}
	}
	if (cmd[3] && !mask)
		mask = cmd[3];
	// create a mask for this user.
	if (!mask)
		mask = IRC_create_default_mask(srv.users[usr].uh);
	if (!mask && !level) {
		srv.o(target,"Uh oh, something bogus happened.  "
			+ "Alert the bot owner.  (!mask && !level)");
		return;
	}
	var mask_array = mask.split(",");
	var inval_mask = false;
	for (my_mask in mask_array) {
		if (IRC_check_host(mask_array[my_mask],true,true,false)) {
			srv.o(target,"The mask (" + mask_array[my_mask] + ") is "
				+ "invalid.  No user added.");
			inval_mask = true;
			return;
		}
	}
	if (inval_mask)
		return;
	srv.o(target,"Added " + cmd[1] + " as level " + level
		+ " with mask(s): " + mask);
	srv.o(target,"This user should now set a password with /MSG " + srv.nick
		+ " PASS");
	var newuser = system.new_user(cmd[1]);
	Masks[newuser.number] = mask_array;
	login_user(newuser);
	newuser.settings |= USER_INACTIVE;
	newuser.security.level = level;
	newuser.handle = cmd[1];
	return;
}

Bot_Commands["CHANGE"] = new Bot_Command(80,2,false);
Bot_Commands["CHANGE"].usage =
	get_cmd_prefix() + "CHANGE <nick> <security_level>";
Bot_Commands["CHANGE"].help =
	"Allows you to change the security level of users with a lower level " +
	"than you up to, but not including yours.";
Bot_Commands["CHANGE"].command = function (target,onick,ouh,srv,lvl,cmd) {
	var usr = new User(system.matchuser(cmd[1]));
	if (usr.number == 0) {
		srv.o(target,"The user " + cmd[1] + " doesn't exist in my database.");
		return;
	}
	if (lvl <= usr.security.level) {
		srv.o(target,"You can only use the change command on users with a "
			+ "lower level than yours. (" + lvl + ")  "
			+ "This error message Copyright 2006 Deuce. ;)");
		return;
	}
	if (parseInt(cmd[2]) >= lvl) {
		srv.o(target,"You cannot change an access level to be higher or equal "
			+ "to your own. (" + lvl + ")");
		return;
	}
	srv.o(target,"Access level for " + usr.alias + " changed to "
		+ parseInt(cmd[2]));
	usr.security.level = parseInt(cmd[2]);
	return;
}

Bot_Commands["RESETPASS"] = new Bot_Command(90,true,false);
Bot_Commands["RESETPASS"].command = function (target,onick,ouh,srv,lvl,cmd) {
	var usr = new User(system.matchuser(cmd[1]));
	if (usr.number > 0) {
		srv.o(target,usr.alias + "'s password has been reset.  "
			+ "They should now set a new one with /MSG " + srv.nick + " "
			+ "PASS <newpass>");
		usr.security.password = "";
		usr.settings |= USER_INACTIVE;
	} else {
		srv.o(target,cmd[1] + " doesn't exist in my database!");
	}
	return;
}

Bot_Commands["PASS"] = new Bot_Command(50,true,false);
Bot_Commands["PASS"].command = function (target,onick,ouh,srv,lvl,cmd) {
	if ((target[0] == "#") || (target[0] == "&")) {
		srv.o(target,"Fool!  I'm not setting your password to something "
			+ "you broadcast in a public channel.  Pick a new password and "
			+ "then /MSG " + srv.nick + " PASS <newpass>");
		return;
	}
	var usr = new User(system.matchuser(onick));
	if (usr.number == 0) {
		srv.o(target,"Huh?  You don't exist.  Inform the bot owner  (!usr)");
		return;
	}
	if (usr.security.password != "") {
		if (!cmd[2]) {
			srv.o(target,"I need your old password too, bud.  "
				+ "/MSG " + srv.nick + " PASS <newpass> <oldpass>");
			return;
		}
		if (cmd[2].toUpperCase() != usr.security.password) {
			srv.o(target,"Password mismatch.  /MSG " + srv.nick + " PASS "
				+ "<newpass> <oldpass>");
			return;
		}
	}
	srv.o(target,"Your password has now been set to '" + cmd[1] + "', "
		+"don't forget it!");
	usr.security.password = cmd[1];
	if (usr.settings&USER_INACTIVE)
		usr.settings &= ~USER_INACTIVE;
	return;
}

Bot_Commands["SEVAL"] = new Bot_Command(90,true,true);
Bot_Commands["SEVAL"].command = function (target,onick,ouh,srv,lvl,cmd) {
	cmd.shift();
	var query = cmd.join(" ");
	js.branch_limit=600; // protection
	js.branch_counter=0; // resets
	try {
		srv.o(target,eval(query));
	} catch(e) {
		srv.o(target,"ERROR: "+e);
	}
	js.branch_limit=0; // protection
	return;
}

Bot_Commands["EVAL"] = new Bot_Command(0,true,false);
Bot_Commands["EVAL"].command = function (target,onick,ouh,srv,lvl,cmd) {
	var query = "";

	var usr = new User(system.matchuser(onick));
	if (usr.number) {
		var uid_str = format("%04u", usr.number);
		var user_js = "/home/bbs/data/user/" +uid_str+ ".eval.js";
		if (file_exists(user_js)) {
			var user_file = new File(user_js);
			if (user_file.open('r+')) {
				var str;
				while ((str=user_file.readln())!=null) {
					query += str;
				}
				user_file.close();
			}
		}
	}

	cmd.shift();
	query += cmd.join(" ");
	js.branch_limit=10; // protection
	js.branch_counter=0; // resets
	var result = js.eval(query);
	// if(result)
		// result = strip_ctrl(result.toString().slice(0,512));
	// else if (result==undefined)
		// result = system.popen("tail -1 /home/bbs/log/ircbot/stderr");
	if (result==undefined)
		result = system.popen("tail -1 /home/bbs/log/ircbot/stderr");
	srv.o(target,result);
	js.branch_limit=0; // protection off
	return;
}

Bot_Commands["GROUPS"] = new Bot_Command(50,true,false);
Bot_Commands["GROUPS"].command = function (target,onick,ouh,srv,lvl,cmd) {
	for (g in msg_area.grp_list) {
		srv.o(target,"[" + msg_area.grp_list[g].number + "] "
			+ msg_area.grp_list[g].description);
	}
	return;
}

Bot_Commands["SUBS"] = new Bot_Command(50,true,false);
Bot_Commands["SUBS"].command = function (target,onick,ouh,srv,lvl,cmd) {
	var groupnum = parseInt(cmd[1]);
	if (!msg_area.grp_list[groupnum]) {
		srv.o(target,"Group number " + cmd[1] + " doesn't exist!");
		return;
	}
	var sg = msg_area.grp_list[groupnum].sub_list;
	for (g in msg_area.grp_list[groupnum].sub_list) {
		srv.o(target,"[" + sg[g].number + "] " + sg[g].description
			+ " (" + sg[g].code + ")");
	}
	return;
}
Bot_Commands["SUBGROUPS"] = Bot_Commands["SUBS"];

Bot_Commands["READ"] = new Bot_Command(50,true,false);
Bot_Commands["READ"].command = function (target,onick,ouh,srv,lvl,cmd) {
	if (!cmd[2]) { // user wants to list msgs?
		var msgs = new MsgBase(cmd[1]);
		if(!msgs) {
			srv.o(target,"No such message base!");
			return;
		}
		msgs.open();
		srv.o(target,"There are " + msgs.total_msgs + " messages to read from "
			+ msgs.first_msg + " to " + msgs.last_msg);
		msgs.close();
		return;
	} else if (cmd[2]) { // reading a msg
		var msgs = new MsgBase(cmd[1]);
		if(!msgs) {
			srv.o(target,"No such message base!");
			return;
		}
		var mn = parseInt(cmd[2]);
		msgs.open();
		var mh = msgs.get_msg_header(mn);
		srv.o(target,"  To: " + mh.to);
		srv.o(target,"From: " + mh.from);
		srv.o(target,"Subj: " + mh.subject);
		var my_msg = msgs.get_msg_body(mn).split("\r\n");
		for (line in my_msg) {
			if (!my_msg[line])
				my_msg[line] = " ";
			srv.o(target, my_msg[line]);
		}
		msgs.close();
		return;
	}
	return;
}

Bot_Commands["FINGER"] = new Bot_Command(50,true,false);
Bot_Commands["FINGER"].command = function (target,onick,ouh,srv,lvl,cmd) {
	var udpfinger = false;
	if (cmd[0] == "UDPFINGER")
		udpfinger = true;
	var f_host;
	var f_user;
	if (cmd[1].match(/[@]/)) { // user@host
		f_host = cmd[1].split("@")[1];
		f_user = cmd[1].split("@")[0];
	} else { // assume just host
		f_host = cmd[1];
		f_user = "";
	}
	var f_sock;
	if (udpfinger) {
		f_sock = new Socket(SOCK_DGRAM);
		f_sock.nonblocking = true;
	} else {
		f_sock = new Socket();
		f_sock.nonblocking = false;
	}
	if (!f_sock.connect(f_host,79)) {
		srv.o(target,"Couldn't connect to "+ f_host +": " + f_sock.last_error);
		return;
	} else {
		f_sock.send(f_user + "\r\n");
		var f_line_count = 0;
		var timeout = time()+10;
		while(f_sock.is_connected) {
			if (udpfinger) {
				var tmp = f_sock.read();
				if (tmp) {
					var udp_lines = tmp.split("\r\n");
					for (ul in udp_lines) {
						srv.o(target, strip_ctrl(udp_lines[ul]));
						f_line_count++;
						if ((f_line_count > 10) && (lvl < 75) &&
						    ((target[0] == "#") || (target[0] == "&")) ) {
							srv.o(target,"*** Connection Terminated "
								+ "(output squelched after 10 lines)");
							return;
						}
					}
					return;
				}
			 } else {
				var line=f_sock.readline();

				if(line != null) {
					srv.o(target, strip_ctrl(line));
					f_line_count++;
					if ((f_line_count > 10) &&
					    (lvl < 75) &&
					    ((target[0] == "#") ||
					     (target[0] == "&")) ) {
						srv.o(target,"*** Connection Terminated "
							+"(output squelched after 10 lines)");
						return;
					}
				}
			}
			if (time() >= timeout) {
				srv.o(target,"*** Your query timed out after 10 seconds.");
				return;
			}
		}
		f_sock.close();
	}
	return;
}
Bot_Commands["UDPFINGER"] = Bot_Commands["FINGER"];

Bot_Commands["EXEC"] = new Bot_Command(99,true,true);
Bot_Commands["EXEC"].command = function (target,onick,ouh,srv,lvl,cmd) {
	cmd.shift();
	var query = cmd.join(" ");
	var this_poutput = system.popen(query);
	if (!this_poutput) {
		srv.o(target,"Command failed. :(");
		return;
	}
	for (line in this_poutput) {
		if (!this_poutput[line])
			this_poutput[line] = " ";
		srv.o(target, this_poutput[line]);
	}
	return;
}

Bot_Commands["SAY"] = new Bot_Command(80,true,false);
Bot_Commands["SAY"].command = function (target,onick,ouh,srv,lvl,cmd) {
	cmd.shift();
	var say_target = cmd.shift();
	if ( (say_target[0] != "#") && (say_target[0] != "&")
		&& (lvl<99) ) {
		srv.o(target, "Can only 'say' to a channel when access level < 99.");
		return;
	}
	var query = cmd.join(" ");
	srv.o(say_target, strip_ctrl(query));
	return;
}

Bot_Commands["LASTSPOKE"] = new Bot_Command(0,false,false);
Bot_Commands["LASTSPOKE"].command = function (target,onick,ouh,srv,lvl,cmd) {
	if (!cmd[1]) {
		srv.o(target,"You spoke just now, chief.");
		return;
	}
	var usr = new User(system.matchuser(cmd[1]));
	var srv_usr=srv.users[cmd[1].toUpperCase()];
	if (usr.number > 0 && srv_usr && srv_usr.last_spoke>0) {
		var last_date=strftime("%m/%d/%Y",srv_usr.last_spoke);
		var last_time=strftime("%H:%M",srv_usr.last_spoke);
		srv.o(target,usr.alias + " last spoke on " + last_date + " at " + last_time + ".");
	} else {
		srv.o(target,"I have no such user in my database.");
	}
	return;
}

Bot_Commands["FORCE"] = new Bot_Command(90,true,true)
Bot_Commands["FORCE"].command = function (target,onick,ouh,srv,lvl,cmd) {
	cmd.shift();
	if(!cmd[0] || !cmd[1]) {
		srv.o(target,"Invalid arguments.");
		return;
	}
	onick=cmd.shift();
	if(!srv.users[onick.toUpperCase()]) {
		srv.o(target,"No such user.");
		return;
	}
	srv.check_bot_command(target,onick,srv.users[onick.toUpperCase()].uh,cmd.join(" "));
}

Bot_Commands["DEBUG"] = new Bot_Command(99,true,true);
Bot_Commands["DEBUG"].command = function (target,onick,ouh,srv,lvl,cmd) {
	cmd.shift();
	var data=eval(cmd.join(" "));
	var str=debug_trace(data,1).split("\r\n");
	while(str.length) srv.o(target,str.shift());
	return;
}

Bot_Commands["OBJKEYS"] = new Bot_Command(99,true,true);
Bot_Commands["OBJKEYS"].command = function (target,onick,ouh,srv,lvl,cmd) {
	cmd.shift();
	cmdline=cmd.join(" ");
	var data=eval(cmdline);
	if(data) {
		var keystr="";
		for(var i in data) keystr+=", " + i;
		var output=word_wrap(keystr.substr(2),512).split("\r\n");
		while(output.length) srv.o(target,"KEYS: " + output.shift());
	}
	return;
}

Bot_Commands["TAIL"] = new Bot_Command(90,true,true);
Bot_Commands["TAIL"].command = function (target,onick,ouh,srv,lvl,cmd) {
	cmd.shift();
	if(tail_history[cmd[0].toUpperCase()]) {
		delete tail_history[cmd[0].toUpperCase()];
		srv.o(target,"stopped monitoring: " + cmd[0].toLowerCase());
	}
	else {
		tail_history[cmd[0].toUpperCase()]=new History(cmd[0],target);
		srv.o(target,"monitoring: " + cmd[0].toLowerCase());
	}
	return;
}

Bot_Commands["PIPE"] = new Bot_Command(80,false,false);
Bot_Commands["PIPE"].command = function (target,onick,ouh,srv,lvl,cmd) {
	if (!cmd[1]) {
		var pipelist = "";
		for (s in Bot_Servers) {
			if (Bot_Servers[s].pipe) {
				for (p in Bot_Servers[s].pipe) {
					pipelist += p + "(" + s + ") ";
				}
			}
		}
		if (!pipelist) {
			srv.o(target, "No channels being piped on any network.");
			return;
		}
		return;
	}
	if (!cmd[3]) {
		srv.o(target, "Invalid number of arguments.  Usage: ADD <chan> <net>");
		return;
	}
	cmd[1] = cmd[1].toUpperCase();
	var pipe_chan = cmd[2].toUpperCase();
	var pipe_srv = Bot_Servers[cmd[3].toUpperCase()];
	if (!pipe_srv) {
		srv.o(target, "No such network.");
		return;
	}
	if (!pipe_srv.channel[pipe_chan]) {
		srv.o(target, "I'm not on that channel.");
		return;
	}
	if (!pipe_srv.pipe)
		pipe_srv.pipe = new Object();
	if (cmd[1] == "ADD") {
		if (pipe_srv.pipe[pipe_chan]) {
			srv.o(target, "I'm already piping that channel!");
			return;
		}
		pipe_srv.pipe[pipe_chan] = new Object();
		pipe_srv.pipe[pipe_chan].srv = srv;
		pipe_srv.pipe[pipe_chan].target = target;
		srv.o(target, "Now piping " + cmd[2] + " on " + cmd[3]);
		return;
	} else if (cmd[1] == "DEL") {
		if (!pipe_srv.pipe[pipe_chan]) {
			srv.o(target, "I'm not piping that!");
			return;
		}
		delete pipe_srv.pipe[pipe_chan];
		srv.o(target, "Okay, stopped piping " + cmd[2] + " on " + cmd[3]);
		return;
	}
	srv.o(target, "Invalid arguments.");
	return;
}
