// $Id: ircd_server.js,v 1.60 2020/04/03 22:21:51 deuce Exp $
//
// ircd_channel.js                
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details:
// http://www.gnu.org/licenses/gpl.txt
//
// Synchronet IRC Daemon as per RFC 1459, link compatible with Bahamut 1.4
//
// Copyright 2003-2009 Randolph Erwin Sommerfeld <sysop@rrx.ca>
//
// ** Server to server operation is governed here.
//

////////// Constants / Defines //////////
const SERVER_REVISION = "$Revision: 1.60 $".split(' ')[1];

// Various N:Line permission bits
const NLINE_CHECK_QWKPASSWD		=(1<<0);	// q
const NLINE_IS_QWKMASTER		=(1<<1);	// w
const NLINE_CHECK_WITH_QWKMASTER	=(1<<2);	// k
const NLINE_IS_DREAMFORGE		=(1<<3);	// d

////////// Objects //////////
function IRC_Server() {
	////////// VARIABLES
	// Bools/Flags that change depending on connection state.
	this.hub = false;	// are we a hub?
	this.local = true;	// are we a local socket?
	this.pinged = false;	// have we sent a PING?
	this.server = true;	// yep, we're a server.
	this.uline = false;	// are we services?
	// Variables containing user/server information as we receive it.
	this.flags = 0;
	this.hops = 0;
	this.hostname = "";
	this.id = 0;
	this.ip = "";
	this.ircclass = 0;
	this.linkparent="";
	this.nick = "";
	this.parent = 0;
	this.info = "";
	this.idletime = time();
	this.outgoing = false;
	// Variables (consts, really) that point to various state information
	this.socket = "";
	this.type = BAHAMUT; /* Assume bahamut by default */
	////////// FUNCTIONS
	// Functions we use to control clients (specific)
	this.quit = Server_Quit;
	this.work = Server_Work;
	this.netsplit = IRCClient_netsplit;
	// Socket functions
	this.ircout=ircout;
	this.originatorout=originatorout;
	this.rawout=rawout;
	this.sendq = new IRC_Queue();
	this.recvq = new IRC_Queue();
	// IRC protocol sending functions
	this.bcast_to_channel=IRCClient_bcast_to_channel;
	this.bcast_to_servers=IRCClient_bcast_to_servers;
	this.bcast_to_servers_raw=IRCClient_bcast_to_servers_raw;
	this.bcast_to_uchans_unique=IRCClient_bcast_to_uchans_unique;
	this.globops=IRCClient_globops;
	// Output helpers
	this.server_nick_info=IRCClient_server_nick_info;
	this.server_chan_info=IRCClient_server_chan_info;
	this.server_info=IRCClient_server_info;
	this.synchronize=IRCClient_synchronize;
	this.reintroduce_nick=IRCClient_reintroduce_nick;
	this.finalize_server_connect=IRCClient_finalize_server_connect;
	// Global Functions
	this.check_timeout=IRCClient_check_timeout;
	this.check_queues=IRCClient_check_queues;
	this.set_chanmode=IRCClient_set_chanmode;
	this.check_nickname=IRCClient_check_nickname;
	// Output helper functions (shared)
}

////////// Command Parser //////////

function Server_Work(cmdline) {
	var clockticks = system.timer;
	var cmd;
	var command;

	if (debug)
		log(format("[%s<-%s]: %s",servername,this.nick,cmdline));

	cmd = cmdline.split(" ");

	if (cmdline[0] == ":") {
		// Silently ignore NULL originator commands.
		if (!cmd[1])
			return 0;
		origin = cmd[0].slice(1);
		command = cmd[1].toUpperCase();
		cmdline = cmdline.slice(cmdline.indexOf(" ")+1);
		// resplit cmd[]
		cmd = cmdline.split(" ");
	} else {
		command = cmd[0].toUpperCase();
		origin = this.nick;
	}

	var killtype;
	var ThisOrigin;
	if (origin.match(/[.]/)) {
		ThisOrigin = Servers[origin.toLowerCase()];
		killtype = "SQUIT";
	} else {
		ThisOrigin = Users[origin.toUpperCase()];
		killtype = "KILL";
	}
	if (!ThisOrigin) {
		umode_notice(USERMODE_OPER,"Notice","Server " + this.nick +
			" trying to pass message for non-existent origin: " +
			origin);
		this.rawout(killtype + " " + origin + " :" + servername + " (" + origin + "(?) <- " + this.nick + ")");
		return 0;
	}

	this.idletime = time();

	if (command.match(/^[0-9]+/)) { // passing on a numeric to the client
		if (!cmd[1])
			return 0; // uh...?
		var destination = Users[cmd[1].toUpperCase()];
		if (!destination)
			return 0;
		destination.rawout(":" + ThisOrigin.nick + " " + cmdline);
		return 1;
	}

	var legal_command = true; /* For tracking STATS M */

	switch(command) {
	// PING at the top thanks to RFC1459
	case "PING":
		if (!cmd[1])
			break;
		if (cmd[2] && (cmd[2][0] == ":"))
			cmd[2] = cmd[2].slice(1);
		var tmp_server;
		if (cmd[2])
			tmp_server = searchbyserver(cmd[2]);
		if (tmp_server && (tmp_server != -1) && (tmp_server.id != ThisOrigin.id)) {
			tmp_server.rawout(":" + ThisOrigin.nick + " PING " + ThisOrigin.nick + " :" + tmp_server.nick);
			break;
		}
		if (cmd[1][0] == ":")
			cmd[1] = cmd[1].slice(1);
		if (!cmd[2])
			this.ircout("PONG " + servername + " :" + cmd[1]);
		else
			this.ircout("PONG " + cmd[2] + " :" + cmd[1]);
		break;
	case "ADMIN":
		if (!cmd[1] || ThisOrigin.server)
			break;
		if (cmd[1][0] == ":")
			cmd[1] = cmd[1].slice(1);
		if (wildmatch(servername,cmd[1])) {
			ThisOrigin.do_admin();
		} else {
			var dest_server = searchbyserver(cmd[1]);
			if (!dest_server)
				break;
			dest_server.rawout(":" + ThisOrigin.nick + " ADMIN :" + dest_server.nick);
		}
		break;
	case "AKILL":
		var this_uh;

		if (!cmd[6])
			break;
		if (!ThisOrigin.uline) {
			umode_notice(USERMODE_OPER,"Notice","Non-U:Lined server "
				+ ThisOrigin.nick + " trying to utilize AKILL.");
			break;
		}

		this.bcast_to_servers_raw(":" + ThisOrigin.nick + " " + cmdline);

		this_uh = cmd[2] + "@" + cmd[1];
		if (isklined(this_uh))
			break;
		KLines.push(new KLine(this_uh,IRC_string(cmdline,6),"A"));
		scan_for_klined_clients();
		break;
	case "AWAY":
		if (ThisOrigin.server)
			break;
		var send_away = "AWAY";
		var my_ircstr = IRC_string(cmdline,1);
		if (!my_ircstr) {
			ThisOrigin.away = "";
		} else {
			ThisOrigin.away = my_ircstr;
			send_away += " :" + my_ircstr;
		}
		this.bcast_to_servers_raw(":" + ThisOrigin.nick + " " + send_away);
		break;
	case "CHATOPS":
		if (!cmd[1])
			break;
		var my_ircstr = IRC_string(cmdline,1);
		umode_notice(USERMODE_CHATOPS,"ChatOps","from " +
			ThisOrigin.nick + ": " + my_ircstr);
		this.bcast_to_servers_raw(":" + ThisOrigin.nick + " " +
			"CHATOPS :" + my_ircstr);
		break;
	case "CONNECT":
		if (!cmd[3] || !this.hub || ThisOrigin.server)
			break;
		if (wildmatch(servername, cmd[3])) {
			ThisOrigin.do_connect(cmd[1],cmd[2]);
		} else {
			var dest_server = searchbyserver(cmd[3]);
			if (!dest_server)
				break;
			dest_server.rawout(":" + ThisOrigin.nick + " CONNECT " + cmd[1] + " " + cmd[2] + " "
				+ dest_server.nick);
		}
		break;
	case "GLOBOPS":
		if (!cmd[1])
			break;
		var my_ircstr = IRC_string(cmdline,1);
		ThisOrigin.globops(my_ircstr);
		break;
	case "GNOTICE":
		if (!cmd[1])
			break;
		var my_ircstr = IRC_string(cmdline,1);
		umode_notice(USERMODE_ROUTING,"Routing","from " + ThisOrigin.nick + ": " + my_ircstr);
		this.bcast_to_servers_raw(":" + ThisOrigin.nick + " " + "GNOTICE :" + my_ircstr);
		break;
	case "ERROR":
		var my_ircstr = IRC_string(cmdline,1);
		gnotice("ERROR from " + this.nick + " [(+)0@" + this.hostname + "] -- " + my_ircstr);
		ThisOrigin.quit(my_ircstr);
		break;
	case "INFO":
		if (!cmd[1] || ThisOrigin.server)
			break;
		if (cmd[1][0] == ":")
			cmd[1] = cmd[1].slice(1);
		if (wildmatch(servername, cmd[1])) {
			ThisOrigin.do_info();
		} else {
			var dest_server = searchbyserver(cmd[1]);
			if (!dest_server)
				break;
			dest_server.rawout(":" + ThisOrigin.nick + " INFO :" + dest_server.nick);
		}
		break;
	case "INVITE":
		if (!cmd[2] || ThisOrigin.server)
			break;
		if (cmd[2][0] == ":")
			cmd[2] = cmd[2].slice(1);
		var chan = Channels[cmd[2].toUpperCase()];
		if (!chan)
			break;
		if (!chan.modelist[CHANMODE_OP][ThisOrigin.id])
			break;
		var nick = Users[cmd[1].toUpperCase()];
		if (!nick)
			break;
		if (!nick.channels[chan.nam.toUpperCase()])
			break;
		nick.originatorout("INVITE " + nick.nick + " :" + chan.nam,ThisOrigin);
		nick.invited = chan.nam.toUpperCase();
		break;
	case "JOIN":
		if (!cmd[1] || ThisOrigin.server)
			break;
		if (cmd[1][0] == ":")
			cmd[1]=cmd[1].slice(1);
		var the_channels = cmd[1].split(",");
		for (jchan in the_channels) {
			if (the_channels[jchan][0] != "#")
				continue;
			ThisOrigin.do_join(the_channels[jchan].slice(0,max_chanlen),"");
		}
		break;
	case "KICK":
		var chanid;
		var nickid;
		var str;
		var kick_reason;

		if (!cmd[2])
			break;
		chanid = Channels[cmd[1].toUpperCase()];
		if (!chanid)
			break;
		nickid = Users[cmd[2].toUpperCase()];
		if (!nickid)
			nickid = search_nickbuf(cmd[2]);
		if (!nickid)
			break;
		if (!nickid.channels[chanid.nam.toUpperCase()])
			break;
		if (cmd[3])
			kick_reason = IRC_string(cmdline,3).slice(0,max_kicklen);
		else
			kick_reason = ThisOrigin.nick;
		str = "KICK " + chanid.nam + " " + nickid.nick + " :" + kick_reason;
		ThisOrigin.bcast_to_channel(chanid, str, false);
		this.bcast_to_servers_raw(":" + ThisOrigin.nick + " " + str);
		nickid.rmchan(chanid);
		break;
	case "KILL":
		if (!cmd[1] || !cmd[2])
			break;
		if (cmd[1].match(/[.]/))
			break;
		if (cmd[2] == ":")
			break;
		var reason = IRC_string(cmdline,2);
		var kills = cmd[1].split(",");
		for(kill in kills) {
			var target = Users[kills[kill].toUpperCase()];
			if (!target)
				target = search_nickbuf(kills[kill]);
			if (target && (this.hub || (target.parent == this.nick)) ) {
				umode_notice(USERMODE_KILL,"Notice","Received KILL message for " + target.nuh
					+ ". From " + ThisOrigin.nick + " Path: " + target.nick + "!Synchronet!"
					+ ThisOrigin.nick + " (" + reason + ")");
				this.bcast_to_servers_raw(":" + ThisOrigin.nick + " KILL " + target.nick + " :" + reason);
				target.quit("KILLED by " + ThisOrigin.nick + " (" + reason + ")",true);
			} else if (target && !this.hub) {
				umode_notice(USERMODE_OPER,"Notice","Non-Hub server " + this.nick + " trying to KILL "
					+ target.nick);
				this.reintroduce_nick(target);
			}
		}
		break;
	case "LINKS":
		if (!cmd[1] || !cmd[2] || ThisOrigin.server)
			break;
		if (match_irc_mask(servername, cmd[1])) {
			ThisOrigin.do_links(cmd[2]);
		} else {
			var dest_server = searchbyserver(cmd[1]);
			if (!dest_server)
				break;
			dest_server.rawout(":" + ThisOrigin.nick + " LINKS " + dest_server.nick + " " + cmd[2]);
		}
		break;
	case "MODE":
		if (!cmd[1] || !cmd[2])
			break;
		if (cmd[1][0] == "#") {
			var chan = Channels[cmd[1].toUpperCase()];
			if (!chan)
				break;
			var modeline;
			var cmdend = cmd.length - 1;
			var cmd2_int = parseInt(cmd[2]);
			var cmdend_int = parseInt(cmd[cmdend]);
			var bounce_modes = false;
			/* Detect if this is a TSMODE.  If so, handle. */
			if (cmd2_int == cmd[2]) {
				if (cmd2_int > chan.created) {
					break;
				} else if ((cmd2_int < chan.created) && ThisOrigin.server) {
					bounce_modes = true;
					chan.created = cmd2_int;
				}
				cmd.shift();
			} else if ( (cmdend_int == cmd[cmdend])
					&& (this.type == DREAMFORGE) ) {
				/* DreamForge style TS */
				if (cmdend_int > chan.created) {
					break;
				} else if ((cmdend_int < chan.created) && ThisOrigin.server) {
					bounce_modes = true;
					chan.created = cmdend_int;
				}
				cmd.pop(); /* Strip the TS. */
			}
			cmd.shift();
			cmd.shift();
			var modeline = cmd.join(" ");
			ThisOrigin.set_chanmode(chan,modeline,bounce_modes);
		} else { // assume it's for a user
			var my_modes = ThisOrigin.setusermode(IRC_string(cmd[2],0));
			if (my_modes) {
				this.bcast_to_servers_raw(":" + ThisOrigin.nick
					+ " MODE " + ThisOrigin.nick + " " + my_modes);
			}
		}
		break;
	case "MOTD":
		if (!cmd[1] || ThisOrigin.server)
			break;
		if (cmd[1][0] == ":")
			cmd[1] = cmd[1].slice(1);
		if (wildmatch(servername, cmd[1])) {
			umode_notice(USERMODE_SPY,"Spy",
				"MOTD requested by " + ThisOrigin.nick+
				" (" + ThisOrigin.uprefix + "@" +
				ThisOrigin.hostname + ") [" +
				ThisOrigin.servername + "]");
			ThisOrigin.motd();
		} else {
			var dest_server = searchbyserver(cmd[1]);
			if (!dest_server)
				break;
			dest_server.rawout(":" + ThisOrigin.nick + " MOTD :" + dest_server.nick);
		}
		break;
	case "NICK":
		if (!cmd[2])
			break;
		if (ThisOrigin.server && cmd[8]) {
			var collide = Users[cmd[1].toUpperCase()];
			if (collide) {
				if (collide.parent == this.nick) {
					gnotice("Server " + this.nick + " trying to introduce nick " + collide.nick
						+ " twice?! Ignoring.");
					break;
				} else if ((parseInt(collide.created) >
				    parseInt(cmd[3])) && this.hub) {
					// Nuke our side of things, allow this newly
					// introduced nick to overrule.
					collide.numeric(436, collide.nick + " :Nickname Collision KILL.");
					this.bcast_to_servers("KILL " + collide.nick + " :Nickname Collision.");
					collide.quit("Nickname Collision",true);
				} else if (!this.hub) {
					gnotice("Server " + this.nick + " trying to collide nick " + collide.nick
						+ " forwards, reversing.");
					// Don't collide our side of things from a leaf
					this.ircout("KILL " + cmd[1] + " :Inverse Nickname Collision.");
					// Reintroduce our nick, because the remote end
					// probably killed it already on our behalf.
					this.reintroduce_nick(collide);
					break;
				} else if (this.hub) {
					// Other side should nuke on our behalf.
					break;
				}
			}
			var uprefixptr = 5; /* Bahamut */
			if (this.type == DREAMFORGE)
				uprefixptr = 4;
			if (!this.hub) {
				if(!this.check_nickname(cmd[1],true)) {
					gnotice("Server " + this.nick + " trying to introduce invalid nickname: " + cmd[1]
						+ ", killed.");
					this.ircout("KILL " + cmd[1] + " :Bogus Nickname.");
					break;
				}
				cmd[2] = 1; // Force hops to 1.
				cmd[3] = time(); // Force TS on nick.
				cmd[7] = this.nick // Force server name
			} else { // if we're a hub
				var test_server = searchbyserver(cmd[uprefixptr+2]);
				if (!test_server || (this.nick !=
				    test_server.parent)) {
					if (debug && test_server)
						log(LOG_DEBUG,"this.nick: " + this.nick + " test_server.parent: " + test_server.parent);
					umode_notice(USERMODE_OPER,"Notice","Server " + this.nick
						+ " trying to introduce nick from server not behind it: "
						+ cmd[1] + "@" + cmd[uprefixptr+2]);
					this.ircout("KILL " + cmd[1] + " :Invalid Origin.");
					break;
				}
			}
			if (cmd[uprefixptr+1][0] == ".")	/* CR "hidden mask" fix. */
				cmd[uprefixptr+1] = cmd[uprefixptr+1].slice(1);
			var new_id = "id" + next_client_id;
			next_client_id++;
			Users[cmd[1].toUpperCase()] = new IRC_User(new_id);
			var NewNick = Users[cmd[1].toUpperCase()];
			NewNick.local = false; // not local. duh.
			NewNick.nick = cmd[1];
			/* What the hell.  CR reverses these at random. */
			if (parseInt(cmd[2]) > 100) {
				NewNick.created = cmd[2];
				NewNick.hops = cmd[3];
			} else {
				NewNick.hops = cmd[2];
				NewNick.created = cmd[3];
			}
			NewNick.uprefix = cmd[uprefixptr];
			NewNick.hostname = cmd[uprefixptr+1];
			NewNick.servername = cmd[uprefixptr+2];
			var rnptr = 10; /* Bahamut */
			if (this.type == DREAMFORGE)
				rnptr = 8;
			NewNick.realname = IRC_string(cmdline,rnptr);
			NewNick.parent = this.nick;
			if (this.type == DREAMFORGE)
				NewNick.ip = 0;
			else /* Bahamut */
				NewNick.ip = int_to_ip(cmd[9]);
			if (this.type == BAHAMUT)
				NewNick.setusermode(cmd[4]);
			for (u in ULines) {
				if (ULines[u] == cmd[uprefixptr+2]) {
					NewNick.uline = true;
					break;
				}
			}
			var true_hops = parseInt(NewNick.hops)+1;
			this.bcast_to_servers_raw("NICK "
				+ NewNick.nick + " "
				+ true_hops + " "
				+ NewNick.created + " "
				+ NewNick.get_usermode(true) + " "
				+ NewNick.uprefix + " "
				+ NewNick.hostname + " "
				+ NewNick.servername + " "
				+ "0 "
				+ ip_to_int(NewNick.ip)
				+ " :" + NewNick.realname,BAHAMUT);
			this.bcast_to_servers_raw("NICK "
				+ NewNick.nick + " "
				+ true_hops + " "
				+ NewNick.created + " "
				+ NewNick.uprefix + " "
				+ NewNick.hostname + " "
				+ NewNick.servername + " "
				+ "0 "
				+ ":" + NewNick.realname,DREAMFORGE);
		} else { // we're a user changing our nick.
			var ctuc = cmd[1].toUpperCase();
			if ((Users[ctuc])&&Users[ctuc].nick.toUpperCase() !=
			    ThisOrigin.nick.toUpperCase()) {
				gnotice("Server " + this.nick + " trying to collide nick via NICK changeover: "
					+ ThisOrigin.nick + " -> " + cmd[1]);
				server_bcast_to_servers("KILL " + ThisOrigin.nick + " :Bogus nickname changeover.");
				ThisOrigin.quit("Bogus nickname changeover.");
				this.ircout("KILL " + cmd[1] + " :Bogus nickname changeover.");
				this.reintroduce_nick(Users[ctuc]);
				break;
			}
			if (ThisOrigin.check_nickname(cmd[1]) < 1) {
				gnotice("Server " + this.nick + " trying to change to bogus nick: "
					+ ThisOrigin.nick + " -> " + cmd[1]);
				this.bcast_to_servers_raw("KILL " + ThisOrigin.nick + " :Bogus nickname switch detected.");
				this.ircout("KILL " + cmd[1] + " :Bogus nickname switch detected.");
				ThisOrigin.quit("Bogus nickname switch detected.",true);
				break;
			}
			if (this.hub)
				ThisOrigin.created = parseInt(IRC_string(cmd[2],0));
			else
				ThisOrigin.created = time();
			ThisOrigin.bcast_to_uchans_unique("NICK " + cmd[1]);
			this.bcast_to_servers_raw(":" + ThisOrigin.nick + " NICK " + cmd[1] + " :" + ThisOrigin.created);
			if (ctuc != ThisOrigin.nick.toUpperCase()) {
				push_nickbuf(ThisOrigin.nick,cmd[1]);
				Users[cmd[1].toUpperCase()] = ThisOrigin;
				delete Users[ThisOrigin.nick.toUpperCase()];
			}
			ThisOrigin.nick = cmd[1];
		}
		break;
	case "NOTICE":
		// FIXME: servers should be able to send notices.
		if (!cmd[1] || ThisOrigin.server)
			break;
		if (this.nick != ThisOrigin.parent)
			break;  /* Fix for CR bouncing back msgs. */
		var my_ircstr = IRC_string(cmdline,2);
		if (!cmd[2] || !my_ircstr)
			break;
		var targets = cmd[1].split(",");
		for (nt in targets) {
			if (targets[nt][0] != "&")
				ThisOrigin.do_msg(targets[nt],"NOTICE",my_ircstr);
		}
		break;
	case "PART":
		if (!cmd[1] || ThisOrigin.server)
			break;
		var the_channels = cmd[1].split(",");
		for(pchan in the_channels) {
			ThisOrigin.do_part(the_channels[pchan]);
		}
		break;
	case "PASS":
		var result;

		if (!this.hub || !cmd[3])
			break;
		if (cmd[3] != "QWK")
			break;
		if (cmd[2][0] == ":")
			cmd[2] = cmd[2].slice(1);
		if (cmd[4]) { // pass the message on to target.
			var dest_server = searchbyserver(cmd[4]);
			if (!dest_server) {
				break;
			} else if ((dest_server == -1) &&
			    (this.flags&NLINE_IS_QWKMASTER)) {
				var qwkid = cmd[2].toLowerCase();
				var hunt = qwkid + ".synchro.net";
				var my_server = 0;
				for (ur in Unregistered) {
					if (Unregistered[ur].nick ==
					    hunt) {
						my_server = Unregistered[ur];
						break;
					}
				}
				if (!my_server)
					break;
				if (cmd[1] != "OK") {
					my_server.quit("S Server not configured.");
					break;
				}
				Servers[my_server.nick.toLowerCase()] = new IRC_Server();
				var ns = Servers[my_server.id];
				ns.id = my_server.id;
				ns.nick = my_server.nick;
				ns.info = my_server.realname;
				ns.socket = my_server.socket;
				delete Unregistered[my_server.id];
				ns.finalize_server_connect("QWK",true);
				break;
			} else if (dest_server) {
				if (dest_server == -1)
					break; // security trap
				dest_server.rawout(":" + ThisOrigin.nick + " PASS " + cmd[1] + " :" + cmd[2]
					+ " QWK " + dest_server.nick);
			}
			break;
		}
		// Are we passing this on to our qwk-master?
		for (nl in NLines) {
			if (NLines[nl].flags&NLINE_IS_QWKMASTER) {
				var qwk_master = searchbyserver(NLines[nl].servername);
				if (qwk_master) {
					qwk_master.rawout(":" + ThisOrigin.nick + " PASS " + cmd[1] + " :" + cmd[2] + " QWK");
					return 0;
				}
			}
		}
		// If we got here, we must be the qwk master. Process.
		if (check_qwk_passwd(cmd[2],cmd[1]))
			result = "OK";
		else
			result = "VOID";
		this.rawout(":" + servername + " PASS " + result + " :" + cmd[2] + " QWK " + ThisOrigin.nick);
		break;
	case "PONG":
		if (cmd[2]) {
			if (cmd[2][0] == ":")
				cmd[2] = cmd[2].slice(1);
			var my_server = searchbyserver(cmd[2]);
			if (!my_server) {
				break;
			} else if (my_server == -1) {
				var my_nick = Users[cmd[2].toUpperCase()];
				if (my_nick)
					my_nick.rawout(":" + ThisOrigin.nick + " PONG " + cmd[1] + " :" + my_nick.nick);
				else
					this.pinged = false;
				break;
			} else if (my_server) {
				my_server.rawout(":" + ThisOrigin.nick + " PONG " + cmd[1] + " :" + cmd[2]);
				break;
			}
		}
		this.pinged = false;
		break;
	case "PRIVMSG":
		if (!cmd[1] || ThisOrigin.server)
			break;
		if (this.nick != ThisOrigin.parent)
			break;	/* Fix for CR bouncing back msgs. */
		var my_ircstr = IRC_string(cmdline,2);
		if (!cmd[2] || !my_ircstr)
			break;
		var targets = cmd[1].split(",");
		for (pm in targets) {
			if (targets[pm][0] != "&")
				ThisOrigin.do_msg(targets[pm],"PRIVMSG",my_ircstr);
		}
		break;
	case "QUIT":
		ThisOrigin.quit(IRC_string(cmdline,1));
		break;
	case "SERVER":
		if (!cmd[3])
			break;
		// FIXME: when on Earth does this happen? :P?
		var hops = parseInt(cmd[2]);
		if ((hops == 1) && !this.info) {
			umode_notice(USERMODE_OPER,"Notice", "Server " + cmd[1] + " updating info after handshake???");
			this.nick = cmd[1];
			this.hops = 1;
			this.info = IRC_string(cmdline,3);
			this.linkparent = servername;
			this.parent = this.nick;
			var newsrv = this;
		} else if (hops > 1) {
			if (this.hub) {
				if (searchbyserver(cmd[1])) {
					this.quit("Server " + cmd[1] + " already exists.");
					return 0;
				}
				var lcserver = cmd[1].toLowerCase();
				var new_id = "id" + next_client_id;
				next_client_id++;
				Servers[lcserver] = new IRC_Server();
				var newsrv = Servers[lcserver];
				newsrv.hops = cmd[2];
				newsrv.nick = cmd[1];
				newsrv.info = IRC_string(cmdline,3);
				newsrv.parent = this.nick;
				newsrv.linkparent = ThisOrigin.nick;
				newsrv.local = false;
				for (u in ULines) {
					if (ULines[u] == cmd[1]) {
						newsrv.uline = true;
						break;
					}
				}
			} else {
				umode_notice(USERMODE_ROUTING,"Routing","from " + servername
					+ ": Non-Hub link " + this.nick + " introduced " + cmd[1] + "(*).");
				this.quit("Too many servers.  You have no H:Line to introduce " + cmd[1] + ".",true);
				return 0;
			}
		} else {
			umode_notice(USERMODE_OPER,"Notice","Refusing to comply with supposedly bogus SERVER command from "
				+ this.nick + ": " + cmdline);
			break;
		}
		this.bcast_to_servers_raw(":" + newsrv.linkparent + " SERVER " + newsrv.nick + " "
			+ (parseInt(newsrv.hops)+1) + " :" + newsrv.info);
		break;
	case "SJOIN":
		if (!cmd[2])
			break;
		if (cmd[2][0] != "#")
			break;

		var chan_members;
		var cm_array;
		var mode_args;

		if (cmd[3]) {
			var incoming_modes = new Object;
			var tmp_modeargs = 3;

			/* Parse the modes and break them down */
			for (tmpmc in cmd[3]) {
				var my_modechar = cmd[3][tmpmc];
				if (my_modechar == "+")
					continue;
				if ((my_modechar == "k") ||
				    (my_modechar == "l")) {
					tmp_modeargs++;
					incoming_modes[my_modechar] = cmd[tmp_modeargs];
				} else {
					incoming_modes[my_modechar] = true;
				}
			}

			/* Reconstruct our modes into a string now */
			var mode_args_chars = "+";
			var mode_args_args = "";
			for (my_mc in incoming_modes) {
				mode_args_chars += my_mc;
				if ((my_mc == "k") || (my_mc == "l")) {
					mode_args_args += " " + incoming_modes[my_mc];
				}
			}
			mode_args = mode_args_chars + mode_args_args;

			/* The following corrects a bug in Bahamut.. */
			if ((cmd[4] == "") && cmd[5])
				tmp_modeargs++;
			
			tmp_modeargs++; /* Jump to start of string */
			chan_members = IRC_string(cmdline,tmp_modeargs).split(' ');

			if (chan_members == "") {
				umode_notice(USERMODE_OPER,"Notice","Server " + this.nick + " trying to SJOIN empty channel "
					+ cmd[2] + " before processing.");
				break;
			}

			cm_array = new Array; /* True array */

			for (cm in chan_members) {
				var isop = false;
				var isvoice = false;
				if (chan_members[cm][0] == "@") {
					isop = true;
					chan_members[cm] = chan_members[cm].slice(1);
				}
				if (chan_members[cm][0] == "+") {
					isvoice = true;
					chan_members[cm] = chan_members[cm].slice(1);
				}
				var tmp_nick = Users[chan_members[cm].toUpperCase()];
				if (!tmp_nick)
					continue;
				cm_array.push(new SJOIN_Nick(tmp_nick,isop,isvoice));
			}

			if (cm_array.length < 1) {
				umode_notice(USERMODE_OPER,"Notice","Server " + this.nick
					+ " trying to SJOIN empty channel "
					+ cmd[2] + " post processing.");
				break;
			}

		}

		var cn_tuc = cmd[2].toUpperCase();
		var chan = Channels[cn_tuc];
		if (!chan) {
			Channels[cn_tuc]=new Channel(cn_tuc);
			chan = Channels[cn_tuc];
			chan.nam = cmd[2];
			chan.created = parseInt(cmd[1]);
		}

		if (cmd[3]) {
			var bounce_modes = true;
			if (!ThisOrigin.local || (chan.created == parseInt(cmd[1])))
				bounce_modes = false;
			if (chan.created >= parseInt(cmd[1]))
				this.set_chanmode(chan, mode_args, bounce_modes);

			var num_sync_modes = 0;
			var push_sync_modes = "+";
			var push_sync_args = "";
			var new_chan_members = "";
			for (member in cm_array) {
				if (new_chan_members)
					new_chan_members += " ";

				var member_obj = cm_array[member].nick;
				var is_voice = cm_array[member].isvoice;
				var is_op = cm_array[member].isop;

				if (member_obj.channels[chan.nam.toUpperCase()])
					continue;
				member_obj.channels[chan.nam.toUpperCase()] = chan;
				chan.users[member_obj.id] = member_obj;
				var joinstr = "JOIN " + chan.nam;
				member_obj.bcast_to_channel(chan, joinstr, false);
				member_obj.bcast_to_servers_raw(":" + member_obj.nick
					+ " " + joinstr, DREAMFORGE);
				if (chan.created >= parseInt(cmd[1])) {
					if (is_op) {
						chan.modelist[CHANMODE_OP][member_obj.id]=member_obj.id;
						push_sync_modes += "o";
						push_sync_args += " " + member_obj.nick;
						num_sync_modes++;
						new_chan_members += "@";
					}
					if (num_sync_modes >= max_modes) {
						var mode1str = "MODE " + chan.nam + " "
							+ push_sync_modes + push_sync_args;
						this.bcast_to_channel(chan,mode1str);
						this.bcast_to_servers(mode1str,DREAMFORGE);
						push_sync_modes = "+";
						push_sync_args = "";
						num_sync_modes = 0;
					}
					if (is_voice) {
						chan.modelist[CHANMODE_VOICE][member_obj.id]=member_obj;
						push_sync_modes += "v";
						push_sync_args += " " + member_obj.nick;
						num_sync_modes++;
						new_chan_members += "+";
					}
					if (num_sync_modes >= max_modes) {
						var mode2str = "MODE " + chan.nam + " "
							+ push_sync_modes + push_sync_args;
						this.bcast_to_channel(chan,mode2str);
						this.bcast_to_servers(mode2str,DREAMFORGE);
						push_sync_modes = "+";
						push_sync_args = "";
						num_sync_modes = 0;
					}
				}
				new_chan_members += member_obj.nick;
			}
			if (num_sync_modes) {
				var mode3str = "MODE " + chan.nam + " "
					+ push_sync_modes + push_sync_args;
				this.bcast_to_channel(chan, mode3str);
				this.bcast_to_servers(mode3str,DREAMFORGE);
			}

			// Synchronize the TS to what we received.
			if (chan.created > parseInt(cmd[1]))
				chan.created = parseInt(cmd[1]);

			this.bcast_to_servers_raw(":" + ThisOrigin.nick + " "
				+ "SJOIN "
				+ chan.created + " "
				+ chan.nam + " "
				+ chan.chanmode(true) + " "
				+ ":" + new_chan_members,BAHAMUT)
		} else {
			if (ThisOrigin.server) {
				umode_notice(USERMODE_OPER,"Notice", "Server " + ThisOrigin.nick
					+ " trying to SJOIN itself to a channel?!");
				break;
			}
			if (ThisOrigin.channels[chan.nam.toUpperCase()])
				break;
			ThisOrigin.channels[chan.nam.toUpperCase()] = chan;
			chan.users[ThisOrigin.id] = ThisOrigin;
			ThisOrigin.bcast_to_channel(chan, "JOIN " + chan.nam, false);
			this.bcast_to_servers_raw(":" + ThisOrigin.nick + " SJOIN " + chan.created +" "+ chan.nam,BAHAMUT);
			this.bcast_to_servers_raw(":" + ThisOrigin.nick + " JOIN " + chan.nam,DREAMFORGE);
		}
		break;
	case "SQUIT":
		var sq_server;
		var reason;

		if (!cmd[1] || !this.hub)
			sq_server = this;
		else
			sq_server = searchbyserver(cmd[1]);
		if (!sq_server)
			break;
		reason = IRC_string(cmdline,2);
		if (!reason || !cmd[2])
			reason = ThisOrigin.nick;
		if (sq_server == -1) {
			this.bcast_to_servers_raw("SQUIT " + this.nick + " :Forwards SQUIT.");
			this.quit("Forwards SQUIT.",true);
			break;
		}
		// message from our uplink telling us a server is gone
		if (this.nick == sq_server.parent) {
			sq_server.quit(reason,false,false,ThisOrigin);
			break;
		}
		// oper or server going for squit of a server
		if (!sq_server.local) {
			sq_server.rawout(":" + ThisOrigin.nick + " SQUIT " + sq_server.nick + " :" + reason);
			break;
		}
		var msg = "Received SQUIT " + cmd[1] + " from " +
			ThisOrigin.nick + "(" + reason + ")";
		server_bcast_to_servers("GNOTICE :" + msg);
		umode_notice(USERMODE_ROUTING,"Routing","from " +
			servername + ": " + msg);
		sq_server.quit(reason);
		break;
	case "STATS":
		if (!cmd[2] || ThisOrigin.server)
			break;
		if (cmd[2][0] == ":")
			cmd[2] = cmd[2].slice(1);
		if (wildmatch(servername, cmd[2])) {
			ThisOrigin.do_stats(cmd[1][0]);
		} else {
			var dest_server = searchbyserver(cmd[2]);
			if (!dest_server)
				break;
			dest_server.rawout(":" + ThisOrigin.nick + " STATS " + cmd[1][0] + " :" + dest_server.nick);
		}
		break;
	case "SUMMON":
		if (!cmd[2] || ThisOrigin.server)
			break;
		if (cmd[2][0] == ":")
			cmd[2] = cmd[2].slice(1);
		if (wildmatch(servername, cmd[2])) {
			if (enable_users_summon) {
				ThisOrigin.do_summon(cmd[1]);
			} else {
				ThisOrigin.numeric445();
				break;
			}
		} else {
			var dest_server = searchbyserver(cmd[1]);
			if (!dest_server)
				break;
			dest_server.rawout(":" + ThisOrigin.nick + " SUMMON " + cmd[1] + " :" + dest_server.nick);
		}
		break;
	case "TIME":
		if (!cmd[1] || ThisOrigin.server)
			break;
		if (cmd[1][0] == ":")
			cmd[1] = cmd[1].slice(1);
		if (wildmatch(servername, cmd[1])) {
			ThisOrigin.numeric391();
		} else {
			var dest_server = searchbyserver(cmd[1]);
			if (!dest_server)
				break;
			dest_server.rawout(":" + ThisOrigin.nick + " TIME :" + dest_server.nick);
		}
		break;
	case "TOPIC":
		if (!cmd[4])
			break;
		var chan = Channels[cmd[1].toUpperCase()];
		if (!chan)
			break;
		var the_topic = IRC_string(cmdline,4);
		if (the_topic == chan.topic)
			break;
		chan.topic = the_topic;
		if (this.hub)
			chan.topictime = cmd[3];
		else
			chan.topictime = time();
		chan.topicchangedby = cmd[2];
		var str = "TOPIC " + chan.nam + " :" + chan.topic;
		ThisOrigin.bcast_to_channel(chan,str,false);
		this.bcast_to_servers_raw(":" + ThisOrigin.nick + " TOPIC " + chan.nam + " " + cmd[2] + " "
			+ chan.topictime + " :" + chan.topic);
		break;
	case "TRACE":
		if (!cmd[1] || ThisOrigin.server)
			break;
		ThisOrigin.do_trace(cmd[1]);
		break;
	case "USERS":
		if (!cmd[1] || ThisOrigin.server)
			break;
		if (cmd[1][0] == ":")
			cmd[1] = cmd[1].slice(1);
		if (searchbyserver(cmd[1]) == -1) {
			ThisOrigin.numeric351();
		} else {
			// psst, pass it on
			var dest_server = searchbyserver(cmd[1]);
			if (!dest_server)
				break;
			dest_server.rawout(":" + ThisOrigin.nick + " USERS :" + dest_server.nick);
		}
		break;
	case "VERSION":
		if (!cmd[1] || ThisOrigin.server)
			break;
		if (cmd[1][0] == ":")
			cmd[1] = cmd[1].slice(1);
		if (searchbyserver(cmd[1]) == -1) {
			// it's for us, return the message
			ThisOrigin.numeric351();
		} else {
			// psst, pass it on
			var dest_server = searchbyserver(cmd[1]);
			if (!dest_server)
				break; // someone messed up.
			dest_server.rawout(":" + ThisOrigin.nick + " VERSION :" + dest_server.nick);
		}
		break;
	case "WALLOPS":
		if (!cmd[1])
			break;
		var str = ":" + ThisOrigin.nick + " WALLOPS :" +
			IRC_string(cmdline,1);
		wallopers(str);
		this.bcast_to_servers_raw(str);
		break;
	case "WHOIS":
		if (!cmd[2] || ThisOrigin.server)
			break;
		if (searchbyserver(cmd[1]) == -1) {
			var wi_nicks = IRC_string(cmd[2],0).split(",");
			for (wi_nick in wi_nicks) {
			var wi = Users[wi_nicks[wi_nick].toUpperCase()];
				if (wi)
					ThisOrigin.do_whois(wi);
				else
					ThisOrigin.numeric401(wi_nicks[wi_nick]);
			} 
		ThisOrigin.numeric(318, wi_nicks[0]+" :End of /WHOIS list.");
		} else {
			var dest_server = searchbyserver(cmd[1]);
			if (!dest_server)
				break;
			dest_server.rawout(":" + ThisOrigin.nick + " WHOIS "
				+ dest_server.nick + " :" + IRC_string(cmd[2],0));
		}
		break;
	case "TKL": /* DreamForge/Unreal style global network line modification */
		if (!ThisOrigin.uline) {
			umode_notice(USERMODE_OPER,"Notice","Non-U:Lined server "
				+ ThisOrigin.nick + " trying to utilize TKL.");
			break;
		}
		if (cmd[2] != "G") /* We don't support anything other than G:Lines. */
			break;
		var tkl_add = false;
		if (cmd[1] == "+")
			tkl_add = true;

		var akill_reason = IRC_string(cmdline,8);

		var snd_prefix = ":" + ThisOrigin.nick + " ";

		/* Propagate this to the network */
		this.bcast_to_servers_raw(snd_prefix + cmdline,DREAMFORGE);
		this.bcast_to_servers_raw(snd_prefix + "AKILL "
			+ cmd[4] + " "				/* host */
			+ cmd[3] + " "				/* user */
			+ (cmd[7]-cmd[6]) + " "		/* length */
			+ cmd[5] + " "				/* akiller */
			+ ":" + akill_reason		/* reason */
		,BAHAMUT);

		var this_uh = cmd[3] + "@" + cmd[4];
		if (isklined(this_uh))
			break;
		KLines.push(new KLine(this_uh,akill_reason,"A"));
		scan_for_klined_clients();
		break;
	case "SAMODE": /* :source o #channel modechange modeparams */
		break;
	case "CAPAB":
	case "BURST":
	case "SVSMODE":
	case "NETINFO": /* Dreamforge/Unreal/CR */
	case "SMO": /* Dreamforge/Unreal/CR -- Send Usermode */
	case "EOS": /* Dreamforge/Unreal/CR -- End Of Synch */
	case "TUNL": /* Dreamforge/Unreal/CR */
	case "SETHOST": /* We do not honour SETHOST. */
		break; // Silently ignore for now.
	default:
		umode_notice(USERMODE_OPER,"Notice","Server " + ThisOrigin.nick + " sent unrecognized command: "
			+ cmdline);
		return 0;
	}

	/* This part only executed if the command was legal. */

	if (!Profile[command])
		Profile[command] = new StatsM;
	Profile[command].executions++;
	Profile.ticks += system.timer - clockticks;
}

////////// Functions //////////

function server_bcast_to_servers(str,type) {
	for(thisClient in Local_Servers) {
		var srv = Local_Servers[thisClient];
		if (!type || (srv.type == type))
			srv.rawout(str);
	}
}

function IRCClient_bcast_to_servers(str,type) {
	for(thisClient in Local_Servers) {
		var srv = Local_Servers[thisClient];
		if ( (srv.nick != this.parent) && (!type || (srv.type == type)))
			Local_Servers[thisClient].originatorout(str,this);
	}
}

function IRCClient_bcast_to_servers_raw(str,type) {
	for(thisClient in Local_Servers) {
		var srv = Local_Servers[thisClient];
		if ( (srv.nick != this.parent) && (!type || (srv.type == type)))
			Local_Servers[thisClient].rawout(str);
	}
}

function Server_Quit(str,suppress_bcast,is_netsplit,origin) {
	if (!str)
		str = this.nick;

	if (is_netsplit) {
		this.netsplit(str);
		/* Fix for DreamForge's lack of NOQUIT */
		this.bcast_to_servers_raw("SQUIT " + this.nick + " :" + str,DREAMFORGE);
	} else if (this.local) {
		this.netsplit(servername + " " + this.nick);
		if (!suppress_bcast)
			this.bcast_to_servers_raw("SQUIT " + this.nick + " :" + str);
	} else if (origin) {
		this.netsplit(origin.nick + " " + this.nick);
		if (!suppress_bcast)
			this.bcast_to_servers_raw(":" + origin.nick + " SQUIT " + this.nick + " :" + str);
	} else {
		umode_notice(USERMODE_OPER,"Notice",
			"Netspliting a server which isn't local and doesn't " +
			"have an origin?!");
		if (!suppress_bcast)
			this.bcast_to_servers_raw("SQUIT " + this.nick + " :" + str);
		this.netsplit();
	}

	if (this.local) {
		if (server.client_remove!=undefined)
			server.client_remove(this.socket);

		// FIXME: wrong phrasing below
		gnotice("Closing Link: " + this.nick + " (" + str + ")");
		this.rawout("ERROR :Closing Link: [" + this.uprefix + "@" + this.hostname + "] (" + str + ")");

		if (this.socket!=undefined)
			this.socket.close();
		delete Local_Sockets[this.id];
	}
	if (this.outgoing) {
		if (YLines[this.ircclass].active > 0) {
			YLines[this.ircclass].active--;
			log(LOG_DEBUG, "Class "+this.ircclass+" down to "+YLines[this.ircclass].active+" active out of "+YLines[this.ircclass].maxlinks);
		}
		else
			log(LOG_ERROR, format("Class %d YLine going negative", this.ircclass));
	}
	delete Local_Sockets[this.id];
	delete Local_Sockets_Map[this.id];
	delete Local_Servers[this.id];
	delete Servers[this.nick.toLowerCase()];
	delete this;
	rebuild_socksel_array = true;
}

function IRCClient_synchronize() {
	this.rawout("BURST"); // warn of impending synchronization
	for (my_server in Servers) {
		if (Servers[my_server].id != this.id)
			this.server_info(Servers[my_server]);
	}
	for (my_client in Users) {
		this.server_nick_info(Users[my_client]);
	}
	for (my_channel in Channels) {
		if (my_channel[0] == "#")
			this.server_chan_info(Channels[my_channel]);
	}
	gnotice(this.nick + " has processed user/channel burst, "+
		"sending topic burst.");
	for (my_channel in Channels) {
		if ((my_channel[0] == "#") && Channels[my_channel].topic) {
			var chan = Channels[my_channel];
			this.rawout("TOPIC " + chan.nam + " " + chan.topicchangedby + " " + chan.topictime
				+ " :" + chan.topic);
		}
	}
	this.rawout("BURST 0"); // burst completed.
	gnotice(this.nick + " has processed topic burst " +
		"(synched to network data).");
}

function IRCClient_server_info(sni_server) {
	var realhops = parseInt(sni_server.hops)+1;
	this.rawout(":" + sni_server.linkparent + " SERVER " + sni_server.nick + " " + realhops
		+ " :" + sni_server.info);
}

function IRCClient_server_nick_info(sni_client) {
	var actual_hops = parseInt(sni_client.hops) + 1;
	var sendstr = "NICK " + sni_client.nick + " " + actual_hops + " " + sni_client.created + " ";
	if (this.type == BAHAMUT)
		sendstr += sni_client.get_usermode(true) + " ";
	sendstr += sni_client.uprefix + " " + sni_client.hostname + " " + sni_client.servername + " 0 ";
	if (this.type == BAHAMUT)
		sendstr += ip_to_int(sni_client.ip) + " ";
	sendstr += ":" + sni_client.realname;

	this.rawout(sendstr);

	if (this.type == DREAMFORGE)
		this.rawout(":" + sni_client.nick + " MODE " + sni_client.nick + " :" + sni_client.get_usermode(true));

	if (sni_client.away)
		this.rawout(":" + sni_client.nick + " AWAY :" + sni_client.away);
}

function IRCClient_reintroduce_nick(nick) {
	this.server_nick_info(nick);

	for (uchan in nick.channels) {
		var chan = nick.channels[uchan];
		if (this.type == DREAMFORGE) {
			this.rawout(":" + nick.nick + " JOIN " + chan.nam);
			if (chan.modelist[CHANMODE_OP][nick.id])
				this.ircout("MODE " + chan.nam + " +o " + nick.nick + " " + chan.created);
			if (chan.modelist[CHANMODE_VOICE][nick.id])
				this.ircout("MODE " + chan.nam + " +v " + nick.nick + " " + chan.created);
		} else { /* Bahamut */
			var cmodes = "";
			if (chan.modelist[CHANMODE_OP][nick.id])
				cmodes += "@";
			if (chan.modelist[CHANMODE_VOICE][nick.id])
				cmodes += "+";
			this.rawout("SJOIN " + chan.created + " " + chan.nam + " " + chan.chanmode(true)
				+ " :" + cmodes + nick.nick);
		}
		if (chan.topic)
			this.rawout("TOPIC " + chan.nam + " " + chan.topicchangedby + " " + chan.topictime
				+ " :" + chan.topic);
	}
}

function IRCClient_server_chan_info(sni_chan) {
	if (this.type == DREAMFORGE) {
		var df_chan_occs = sni_chan.occupants().split(' ');
		for (dfocc in df_chan_occs) {
			var cmember = df_chan_occs[dfocc];
			var mem_is_op = false;
			var mem_is_voice = false;
			if (cmember[0] == "@") {
				cmember = cmember.slice(1);
				mem_is_op = true;
			}
			if (cmember[0] == "+") {
				cmember = cmember.slice(1);
				mem_is_voice = true;
			}
			this.rawout(":" + cmember + " JOIN " + sni_chan.nam);
			if (mem_is_op)
				this.ircout("MODE " + sni_chan.nam + " +o " + cmember + " "
					+ sni_chan.created);
			if (mem_is_voice)
				this.ircout("MODE " + sni_chan.nam + " +v " + cmember + " "
					+ sni_chan.created);
		}
		this.ircout("MODE " + sni_chan.nam + " " + sni_chan.chanmode(true) + " " + sni_chan.created);
	} else { /* Bahamut */
		this.rawout("SJOIN " + sni_chan.created + " " + sni_chan.nam + " " + sni_chan.chanmode(true)
			+ " :" + sni_chan.occupants())
	}
	var modecounter=0;
	var modestr="+";
	var modeargs="";
	for (aBan in sni_chan.modelist[CHANMODE_BAN]) {
		modecounter++;
		modestr += "b";
		if (modeargs)
			modeargs += " ";
		modeargs += sni_chan.modelist[CHANMODE_BAN][aBan];
		if (modecounter >= max_modes) {
			var outstr = "MODE " + sni_chan.nam + " " + modestr + " " + modeargs;
			if (this.type == DREAMFORGE)
				outstr += " " + sni_chan.created;
			this.ircout(outstr);
			modecounter=0;
			modestr="+";
			modeargs="";
		}
	}
	if (modeargs) {
		var outstr = "MODE " + sni_chan.nam + " " + modestr + " " + modeargs;
		if (this.type == DREAMFORGE)
			outstr += " " + sni_chan.created;
		this.ircout(outstr);
	}
}

function gnotice(str) {
	umode_notice(USERMODE_ROUTING,"Routing","from " + servername + ": " + str);
	server_bcast_to_servers("GNOTICE :" + str);
}
