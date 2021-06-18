/*

 ircd/server.js

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details:
 https://www.gnu.org/licenses/old-licenses/gpl-2.0.txt

 IRCd inter-server communication.

 Copyright 2003-2021 Randy Sommerfeld <cyan@synchro.net>

*/

// Various N:Line permission bits
const NLINE_CHECK_QWKPASSWD		=(1<<0);	// q
const NLINE_IS_QWKMASTER		=(1<<1);	// w
const NLINE_CHECK_WITH_QWKMASTER	=(1<<2);	// k

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
	this.ip = "";
	this.ircclass = 0;
	this.linkparent = "";
	this.nick = "";
	this.parent = 0;
	this.info = "";
	this.idletime = system.timer;
	// Variables (consts, really) that point to various state information
	this.socket = "";
	////////// FUNCTIONS
	// Functions we use to control clients (specific)
	this.quit = Server_Quit;
	this.work = Server_Work;
	this.netsplit = IRCClient_netsplit;
	// Socket functions
	this.ircout=ircout;
	this.originatorout=originatorout;
	this.rawout=rawout;
	this.sendq = new IRC_Queue(this);
	this.recvq = new IRC_Queue(this);
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
	this.set_chanmode=IRCClient_set_chanmode;
	this.check_nickname=IRCClient_check_nickname;
	this.do_msg=IRCClient_do_msg;
	// Output helper functions (shared)
}

////////// Command Parser //////////

function Server_Work(cmdline) {
	var clockticks = system.timer;
	var cmd, p;
	var tmp, i, j, k, n; /* Temp vars used during command processing */
	var origin;

	log(LOG_DEBUG,format("[%s<-%s]: %s",ServerName,this.nick,cmdline));

	cmd = IRC_parse(cmdline);

	if (!cmd.source.name) {
		cmd.source.name = this.nick;
		cmd.source.is_server = true;
	}

	if (cmd.source.is_server)
		origin = Servers[cmd.source.name.toLowerCase()];
	else
		origin = Users[cmd.source.name.toUpperCase()];

	if (!origin) {
		umode_notice(USERMODE_OPER,"Notice",format(
			"Server %s trying to pass message for non-existent origin: %s",
			this.nick,
			cmd.source.name
		));
		if (Enforcement) {
			this.rawout(format("%s %s :%s (%s(?) <- %s)",
				cmd.source.is_server ? "SQUIT" : "KILL",
				cmd.source.name,
				ServerName,
				cmd.source.name,
				this.nick
			));
		} else {
			this.quit(format(
				"Server %s trying to pass message for non-existent origin: %s",
				this.nick,
				cmd.source.name
			));
		}
		return 0;
	}

	this.idletime = system.timer;

	p = cmd.params;

	if (cmd.verb.match(/^[0-9]+/)) { /* Passing on a numeric to a client */
		if (!p[0])
			return 0;
		var tmp = Users[p[0].toUpperCase()];
		if (!tmp)
			return 0;
		tmp.rawout(cmdline); /* We don't process numerics directly, pass on */
		return 1;
	}

	switch(cmd.verb) {
	case "PING": /* RFC1459 says to respond to these first */
		if (!p[0])
			break;
		if (p[1])
			tmp = searchbyserver(p[1]);
		if (tmp && tmp != -1 && tmp.id != origin.id) {
			tmp.rawout(format(
				":%s PING %s :%s",
				origin.nick,
				origin.nick,
				tmp.nick
			));
			break;
		}
		if (!p[1]) {
			this.ircout(format(
				"PONG %s :%s",
				ServerName,
				p[0]
			))
			break;
		}
		this.ircout(format(
			"PONG %s :%s",
			p[1],
			p[0]
		));
		break;
	case "ADMIN":
		if (!p[0] || origin.server)
			break;
		if (wildmatch(ServerName,p[0])) {
			origin.do_admin();
			break;
		}
		tmp = searchbyserver(p[0]);
		if (!tmp)
			break;
		tmp.rawout(format(
			":%s ADMIN :%s",
			origin.nick,
			tmp.nick
		));
		break;
	case "AKILL":
		if (!p[5])
			break;

		if (!origin.uline) {
			umode_notice(USERMODE_OPER,"Notice",format(
				"Non-U:Lined server %s trying to utilize AKILL.",
				origin.nick
			));
			break;
		}

		this.bcast_to_servers_raw(format(
			":%s %s",
			origin.nick,
			p.join(" ")
		));

		k = p[1] + "@" + p[0];
		if (isklined(k))
			break;
		KLines.push(new KLine(k,p[5],"A"));
		Scan_For_Banned_Clients();
		break;
	case "AWAY":
		if (origin.server)
			break;
		origin.away = p[0] ? p[0] : "";
		this.bcast_to_servers_raw(format(
			":%s AWAY%s",
			origin.nick,
			p[0] ? format(" :%s", p[0]) : ""
		));
		break;
	case "CHATOPS":
		if (!p[0])
			break;
		umode_notice(USERMODE_CHATOPS,"ChatOps",format(
			"from %s: %s",
			origin.nick,
			p[0]
		));
		this.bcast_to_servers_raw(format(
			":%s CHATOPS :%s",
			origin.nick,
			p[0]
		));
		break;
	case "CONNECT":
		if (!p[2] || !this.hub || origin.server)
			break;
		if (wildmatch(ServerName, p[2])) {
			origin.do_connect(p[0],p[1]);
			break;
		}
		tmp = searchbyserver(p[2]);
		if (!tmp)
			break;
		tmp.rawout(format(
			":%s CONNECT %s %s %s",
			origin.nick,
			p[0],
			p[1],
			tmp.nick
		));
		break;
	case "GLOBOPS":
		if (!p[0])
			break;
		origin.globops(p[0]);
		break;
	case "GNOTICE":
		if (!p[0])
			break;
		umode_notice(USERMODE_ROUTING,"Routing",format(
			"from %s: %s",
			origin.nick,
			p[0]
		));
		this.bcast_to_servers_raw(format(
			":%s GNOTICE :%s",
			origin.nick,
			p[0]
		));
		break;
	case "ERROR":
		if (!p[0])
			p[0] = "No error message received.";
		gnotice(format(
			"ERROR from %s [(+)0@%s] -- %s",
			this.nick,
			this.hostname,
			p[0]
		));
		origin.quit(p[0]);
		break;
	case "INFO":
		if (!p[0] || origin.server)
			break;
		if (wildmatch(ServerName, p[0])) {
			origin.do_info();
			break;
		}
		tmp = searchbyserver(p[0]);
		if (!tmp)
			break;
		tmp.rawout(format(
			":%s INFO :%s",
			origin.nick,
			tmp.nick
		));
		break;
	case "INVITE":
		if (!p[1] || origin.server)
			break;
		tmp = Channels[cmd[2].toUpperCase()];
		if (!tmp)
			break;
		if (!tmp.modelist[CHANMODE_OP][origin.id])
			break;
		j = Users[p[0].toUpperCase()];
		if (!j)
			break;
		if (!j.channels[tmp.nam.toUpperCase()])
			break;
		j.originatorout(format(
			"INVITE %s :%s",
			j.nick,
			tmp.nam
		),origin);
		j.invited = tmp.nam.toUpperCase();
		break;
	case "JOIN":
		if (!p[0] || origin.server)
			break;
		k = p[0].split(",");
		for (i in k) {
			if (k[i][0] != "#")
				continue;
			origin.do_join(k[i].slice(0,MAX_CHANLEN),"");
		}
		break;
	case "KICK":
		if (!p[1])
			break;
		tmp = Channels[p[0].toUpperCase()];
		if (!tmp)
			break;
		j = Users[p[1].toUpperCase()];
		if (!j)
			j = search_nickbuf(p[1]);
		if (!j)
			break;
		if (!j.channels[tmp.nam.toUpperCase()])
			break;
		origin.bcast_to_channel(tmp, format(
			"KICK %s %s :%s",
			tmp.nam,
			j.nick,
			p[2] ? p[2] : j.nick
		),false /* bounceback */);
		this.bcast_to_servers_raw(format(
			":%s KICK %s %s :%s",
			origin.nick,
			tmp.nam,
			j.nick,
			p[2] ? p[2] : j.nick
		));
		j.rmchan(tmp);
		break;
	case "KILL":
		if (!p[1])
			break;
		if (p[0].match(/[.]/))
			break;
		k = p[0].split(",");
		for (i in k) {
			tmp = Users[k[i].toUpperCase()];
			if (!tmp)
				tmp = search_nickbuf(k[i]);
			if (!tmp)
				continue;
			if (!Enforcement || this.hub || (tmp.parent == this.nick)) {
				umode_notice(USERMODE_KILL,"Notice",format(
					"Received KILL message for %s. From %s Path: %s!Synchronet!%s (%s)",
					tmp.nuh,
					origin.nick,
					tmp.nick,
					origin.nick,
					p[1]
				));
				this.bcast_to_servers_raw(format(
					":%s KILL %s :%s",
					origin.nick,
					tmp.nick,
					p[1]
				));
				tmp.quit(format(
					"KILLED by %s (%s)",
					origin.nick,
					p[1]
				),true /* suppress_bcast */);
				continue;
			}
			if (!this.hub && Enforcement) {
				umode_notice(USERMODE_OPER,"Notice",format(
					"Non-Hub server %s trying to KILL %s",
					this.nick,
					tmp.nick
				));
				this.reintroduce_nick(tmp);
			}
		}
		break;
	case "LINKS":
		if (!p[1] || origin.server)
			break;
		tmp = searchbyserver(p[1]);
		if (!tmp)
			break;
		if (tmp == -1) {
			origin.do_links(p[0]);
			break;
		}
		tmp.rawout(format(
			":%s LINKS %s %s",
			origin.nick,
			p[0],
			tmp.nick
		));
		break;
	case "MODE":
		if (!p[1])
			break;
		if (p[0][0] != "#") { /* Setting a user mode */
			tmp = origin.setusermode(p[1]);
			if (tmp) {
				this.bcast_to_servers_raw(format(
					":%s MODE %s %s",
					origin.nick,
					origin.nick,
					tmp
				));
			}
			break;
		}
		/* Setting a channel mode */
		tmp = Channels[p[0].toUpperCase()];
		if (!tmp)
			break;
		j = false; /* bounce our side? */
		if (parseInt(p[1]) > tmp.created) /* TS violation TODO: bounce their side */
			break;
		if (parseInt(p[1]) < tmp.created && origin.server)
			j = true;
		p.shift();
		origin.set_chanmode(tmp,p.join(" "),j);
		break;
	case "MOTD":
		if (!p[0] || origin.server)
			break;
		if (wildmatch(ServerName, p[0])) {
			umode_notice(USERMODE_STATS_LINKS,"StatsLinks",format(
				"MOTD requested by %s (%s@%s) [%s]",
				origin.nick,
				origin.uprefix,
				origin.hostname,
				origin.servername
			));
			origin.motd();
			break;
		}
		tmp = searchbyserver(p[0]);
		if (!tmp)
			break;
		tmp.rawout(format(
			":%s MOTD :%s",
			origin.nick,
			tmp.nick
		));
		break;
	case "NICK":
		if (!p[1])
			break;
		if (origin.server && p[9]) { /* New nick being introduced */
			tmp = Users[p[0].toUpperCase()];
			if (tmp) { /* Nickname collision */
				if (tmp.parent == this.nick) {
					gnotice(format(
						"Server %s trying to introduce nick %s twice?! Ignoring.",
						this.nick,
						tmp.nick
					));
					break;
				}
				if (tmp.created > parseInt(p[2]) && (this.hub || !Enforcement)) {
					/* We're on the wrong side. */
					tmp.numeric(436, tmp.nick + " :Nickname Collision KILL.");
					this.bcast_to_servers(format(
						"KILL %s :Nickname Collision.",
						tmp.nick
					));
					tmp.quit("Nickname Collision",true);
					break;
				}
				if (!this.hub && Enforcement) {
					gnotice(format(
						"Server %s trying to collide nick %s forwards. Reversing.",
						this.nick,
						tmp.nick
					));
					this.ircout(format(
						"KILL %s :Inverse Nickname Collision.",
						p[0]
					));
					this.reintroduce_nick(tmp);
					break;
				}
				if (this.hub) {
					/* Our TS greater than what was passed to us. */
					/* Other side should nuke on our behalf. */
					break;
				}
			}
			if (!this.hub) {
				if (!this.check_nickname(p[0],true)) {
					gnotice(format(
						"Server %s trying to introduce invalid nickname: %s",
						this.nick,
						p[0]
					));
					if (Enforcement) {
						this.ircout(format(
							"KILL %s :Bogus Nickname.",
							p[0]
						));
					} else {
						this.quit(format(
							"Server %s trying to introduce invalid nickname: %s",
							this.nick, cmd[1]
						))
					}
					break;
				}
				/* Don't trust what a leaf tells us */
				p[1] = 1;
				p[2] = Epoch();
				p[6] = this.nick;
			} else { /* Hub (trusted) */
				tmp = searchbyserver(p[6]);
				if (!tmp || (this.nick != tmp.parent)) {
					umode_notice(USERMODE_OPER,"Notice",format(
						"Server %s trying illegal NICK %s@%s (parent: %s)",
						origin.nick,
						p[0],
						p[6],
						tmp ? tmp.parent : "none"
					));
					if (Enforcement) {
						this.ircout(format(
							"KILL %s :Invalid Origin.",
							p[0]
						));
					} else {
						this.quit(format(
							"Server %s trying illegal NICK %s@%s (parent: %s)",
							origin.nick,
							p[0],
							p[6],
							tmp ? tmp.parent : "none"
						));
					}
					break;
				}
			}
			Users[p[0].toUpperCase()] = new IRC_User(Generate_ID());
			j = Users[p[0].toUpperCase()];
			j.local = false;
			j.nick = p[0];
			j.hops = parseInt(p[1]);
			j.created = parseInt(p[2]);
			j.uprefix = p[4];
			j.hostname = p[5];
			j.servername = p[6];
			j.realname = p[9];
			j.parent = this.nick;
			j.ip = int_to_ip(p[8]);
			j.setusermode(p[3]);
			for (i in ULines) {
				if (ULines[i] == p[6]) {
					j.uline = true;
					break;
				}
			}
			this.bcast_to_servers_raw(
				format("NICK %s %s %s %s %s %s %s 0 %s :%s",
					j.nick,
					j.hops + 1,
					j.created,
					j.get_usermode(true),
					j.uprefix,
					j.hostname,
					j.servername,
					ip_to_int(j.ip),
					j.realname
				)
			);
			umode_notice(USERMODE_DEBUG,"RemoteClient",format(
				"NICK %s %s@%s %s %s :%s",
				j.nick,
				j.uprefix,
				j.hostname,
				j.servername,
				j.ip,
				j.realname,
				j.id
			));
			break;
		} else { /* A user changing their nick */
			if (origin.server) {
				gnotice(format(
					"Server %s (origin %s) sent malformed NICK message: %s",
					this.nick,
					origin.nick,
					p.join(" ")
				));
				break;
			}
			tmp = Users[p[0].toUpperCase()];
			if (tmp && tmp.nick.toUpperCase() != origin.nick.toUpperCase()) {
				gnotice(format(
					"Server %s trying to collide via NICK changeover: %s -> %s",
					this.nick,
					origin.nick,
					p[0]
				));
				if (Enforcement) {
					server_bcast_to_servers(format(
						"KILL %s :Bogus nickname changeover.",
						origin.nick
					));
					origin.quit("Bogus nickname changeover.");
					this.ircout(format(
						"KILL %s :Bogus nickname changeover.",
						p[0]
					));
					this.reintroduce_nick(tmp);
				} else {
					this.quit(format(
						"Server %s trying to collide via NICK changeover: %s -> %s",
						this.nick,
						origin.nick,
						p[0]
					));
				}
				break;
			}
			if (origin.check_nickname(p[0]) < 1) {
				gnotice(format(
					"Server %s trying to change to bogus nick: %s -> %s",
					this.nick,
					origin.nick,
					p[0]
				));
				if (Enforcement) {
					this.bcast_to_servers_raw(format(
						"KILL %s :Bogus nickname switch detected.",
						origin.nick
					));
					this.ircout(format(
						"KILL %s :Bogus nickname switch detected.",
						p[0]
					));
					origin.quit("Bogus nickname switch detected.",true);
				} else {
					this.quit(format(
						"Server %s trying to change to bogus nick: %s -> %s",
						this.nick,
						origin.nick,
						p[0]
					));
				}
				break;
			}
			if (this.hub)
				origin.created = parseInt(p[1]);
			else
				origin.created = Epoch();
			origin.bcast_to_uchans_unique(format(
				"NICK %s",
				p[0]
			));
			this.bcast_to_servers_raw(format(
				":%s NICK %s :%s",
				origin.nick,
				p[0],
				origin.created
			));
			if (p[0].toUpperCase() != origin.nick.toUpperCase()) { 
				push_nickbuf(origin.nick,p[0]);
				Users[p[0].toUpperCase()] = origin;
				delete Users[origin.nick.toUpperCase()];
			}
			origin.nick = p[0];
		}
		break;
	case "NOTICE":
		if (!p[1])
			break;
		tmp = p[0].split(",");
		for (i in tmp) {
			if (tmp[i][0] != "&" && tmp[i][0] != "*")
				origin.do_msg(tmp[i],"NOTICE",p[1]);
		}
		break;
	case "PART":
		if (!p[0] || origin.server)
			break;
		tmp = p[0].split(",");
		for (i in tmp) {
			origin.do_part(tmp[i]);
		}
		break;
	case "PASS":
		break; /* XXX FIXME XXX */
		if (!this.hub || !p[2])
			break;
		if (p[2] != "QWK")
			break;
		if (p[3]) { /* pass the message on to target. */
			tmp = searchbyserver(p[3]);
			if (!tmp)
				break;
			if (tmp == -1 && this.flags&NLINE_IS_QWKMASTER) {
				var qwkid = cmd[2].toLowerCase();
				var hunt = qwkid + ".synchro.net";
				var my_server = 0;
				for (ur in Unregistered) {
					if (Unregistered[ur].nick == hunt) {
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
				ns.id = my_server.nick.toLowerCase();
				ns.nick = my_server.nick;
				ns.info = my_server.realname;
				ns.socket = my_server.socket;
				delete Unregistered[my_server.id];
				ns.finalize_server_connect("QWK");
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
		/* If we got here, we must be the qwk master. Process. */
		if (Check_QWK_Password(p[1],p[0]))
			result = "OK";
		else
			result = "VOID";
		this.rawout(":" + ServerName + " PASS " + result + " :" + cmd[2] + " QWK " + ThisOrigin.nick);
		break;
	case "PONG":
		if (p[1]) {
			tmp = searchbyserver(p[1]);
			if (!tmp)
				break;
			if (tmp == -1) {
				j = Users[p[1].toUpperCase()];
				if (j) {
					j.rawout(format(
						":%s PONG %s :%s",
						origin.nick,
						p[0],
						j.nick
					));
				}
			} else if (tmp) {
				tmp.rawout(format(
					":%s PONG %s :%s",
					origin.nick,
					p[0],
					p[1]
				));
				break;
			}
		}
		this.pinged = false;
		break;
	case "PRIVMSG":
		if (!p[1] || origin.server)
			break;
		tmp = p[0].split(",");
		for (i in tmp) {
			if (tmp[i][0] != "&")
				origin.do_msg(tmp[i],"PRIVMSG",p[1]);
		}
		break;
	case "QUIT":
		if (!p[0])
			p[0] = origin.nick;
		origin.quit(p[0]);
		break;
	case "SERVER":
		if (!p[2])
			break;
		if (p[1] == 1 && !this.info) {
			umode_notice(USERMODE_OPER,"Notice",format(
				"Server %s updating info after handshake???",
				p[0]
			));
			this.nick = p[0];
			this.hops = 1;
			this.info = p[2];
			this.linkparent = ServerName;
			this.parent = this.nick;
		} else if (p[1] > 1) {
			if (this.hub) {
				if (searchbyserver(p[0])) {
					umode_notice(USERMODE_OPER,"Notice",format(
						"Server %s trying to introduce %s but it already exists.",
						this.nick,
						p[0]
					));
					if (Enforcement) {
						this.rawout(format(
							":%s SQUIT %s :Server already exists.",
							ServerName,
							p[0]
						));
					} else {	
						this.quit(format(
							"Server %s already exists.",
							p[0]
						));
					}
					break;
				}
				Servers[p[0].toLowerCase()] = new IRC_Server();
				tmp = Servers[p[0].toLowerCase()];
				tmp.id = p[0].toLowerCase();
				tmp.hops = parseInt(p[1]);
				tmp.nick = p[0];
				tmp.info = p[2];
				tmp.parent = this.nick;
				tmp.linkparent = origin.nick;
				tmp.local = false;
				for (i in ULines) {
					if (ULines[i] == p[0]) {
						tmp.uline = true;
						break;
					}
				}
				this.bcast_to_servers_raw(format(
					":%s SERVER %s %d :%s",
					tmp.linkparent,
					tmp.nick,
					tmp.hops + 1,
					tmp.info
				));
			} else {
				umode_notice(USERMODE_ROUTING,"Routing",format(
					"from %s: Non-Hub link %s introduced %s(*).",
					ServerName,
					this.nick,
					p[0]
				));
				this.quit(format(
					"Too many servers.  You have no H:Line to introduce %s.",
					p[0]
				),true /* suppress_bcast */);
				break;
			}
		} else {
			umode_notice(USERMODE_OPER,"Notice",format(
				"Refusing to comply with supposedly bogus SERVER command from %s: %s",
				this.nick,
				p.join(" ")
			));
			break;
		}
		break;
	case "SJOIN":
		if (!p[1] || p[1][0] != "#")
			break;

		tmp = Channels[p[1].toUpperCase()];
		if (!tmp) {
			Channels[p[1].toUpperCase()] = new Channel(p[1].toUpperCase());
			tmp = Channels[p[1].toUpperCase()];
			tmp.nam = p[1];
			tmp.created = parseInt(p[0]);
		}

		if (p[2]) {
			this.set_chanmode(
				tmp, /* Channel object */
				p.splice(2,p.length-4).join(" "), /* Channel mode */
				(tmp.created == parseInt(p[0])) ? false : true /* TS */
			);

			j = p[p.length-1].split(" "); /* Channel members */

			if (!j[0]) {
				umode_notice(USERMODE_OPER,"Notice",format(
					"Server %s trying to SJOIN empty channel %s before processing.",
					this.nick,
					p[1]
				));
				break;
			}

			for (i in j) {
				k = new SJOIN_Nick(j[i]);
				n = Users[k.nick.toUpperCase()];
				if (!n)
					continue;

				if (!n.channels[tmp.nam.toUpperCase()]) {
					tmp.users[n.id] = n;
					n.channels[tmp.nam.toUpperCase()] = tmp;
					n.bcast_to_channel(tmp, format(
						"JOIN %s",
						tmp.nam
					), false /*bcast*/);
				}

				if (tmp.created >= parseInt(p[0])) { /* They have TS superiority */
					if (k.isop)
						tmp.modelist[CHANMODE_OP][n.id] = n.id;
					if (k.isvoice)
						tmp.modelist[CHANMODE_VOICE][n.id] = n.id;
					if (k.isop || k.isvoice) {
						origin.bcast_to_channel(tmp, format(
							"MODE %s +%s%s %s",
							tmp.nam,
							k.isop ? "o" : "",
							k.isvoice ? "v" : "",
							n.nick
						), false /*bcast*/);
					}
				} else { /* We have TS superiority */
					if (k.isop) {
						k.isop = false;
						this.rawout(format(":%s MODE %s -o %s",
							ServerName,
							tmp.nam,
							n.nick
						));
					}
					if (k.isvoice) {
						k.isvoice = false;
						this.rawout(format(":%s MODE %s -v %s",
							ServerName,
							tmp.nam,
							n.nick
						));
					}
				}

				j[i] = format("%s%s%s",
					k.isop ? "@" : "",
					k.isvoice ? "+" : "",
					k.nick
				);
			}

			if (tmp.created > parseInt(p[0]))
				tmp.created = parseInt(p[0]);

			this.bcast_to_servers_raw(
				format(":%s SJOIN %s %s %s :%s",
					origin.nick,
					tmp.created,
					tmp.nam,
					tmp.chanmode(true /* pass args */),
					j.join(" ")
				)
			);
		} else { /* User single SJOIN */
			if (origin.server) {
				umode_notice(USERMODE_OPER,"Notice",format(
					"Server %s trying to SJOIN itself to a channel?!",
					origin.nick
				));
				break;
			}
			if (origin.channels[tmp.nam.toUpperCase()])
				break;
			origin.channels[tmp.nam.toUpperCase()] = tmp;
			tmp.users[origin.id] = origin;
			origin.bcast_to_channel(tmp, format(
				"JOIN %s",
				tmp.nam
			), false /*bcast*/);
			this.bcast_to_servers_raw(
				format(":%s SJOIN %s %s",
					origin.nick,
					tmp.created,
					tmp.nam
				)
			);
		}
		break;
	case "SQUIT":
		if (!p[0] || !this.hub)
			tmp = this;
		else
			tmp = searchbyserver(p[0]);
		if (!tmp)
			break;
		if (!p[1]) /* Reason */
			p[1] = origin.nick;
		if (tmp == -1) {
			this.bcast_to_servers_raw(format(
				"SQUIT %s :Forwards SQUIT.",
				this.nick
			));
			this.quit("Forwards SQUIT.",true);
			break;
		}
		/* message from our uplink telling us a server is gone */
		if (this.nick == tmp.parent) {
			tmp.quit(p[1],false,false,origin);
			break;
		}
		/* oper or server going for squit of a server */
		if (!tmp.local) {
			tmp.rawout(format(
				":%s SQUIT %s :%s",
				origin.nick,
				tmp.nick,
				p[1]
			));
			break;
		}
		server_bcast_to_servers(format(
			"GNOTICE :Received SQUIT %s from %s (%s)",
			p[0],
			origin.nick,
			p[1]
		));
		umode_notice(USERMODE_ROUTING,"Routing",format(
			"from %s: Received SQUIT %s from %s (%s)",
			ServerName,
			p[0],
			origin.nick,
			p[1]
		));
		tmp.quit(p[1]);
		break;
	case "STATS":
		if (!p[1] || origin.server)
			break;
		if (wildmatch(ServerName, p[1])) {
			origin.do_stats(p[0][0]);
			break;
		}
		tmp = searchbyserver(p[1]);
		if (!tmp)
			break;
		tmp.rawout(format(
			":%s STATS %s :%s",
			origin.nick,
			p[0][0],
			tmp.nick
		));
		break;
	case "SUMMON":
		if (!p[1] || origin.server)
			break;
		if (wildmatch(ServerName, p[1])) {
			if (SUMMON)
				origin.do_summon(p[0]);
			else
				origin.numeric445();
			break;
		}
		tmp = searchbyserver(p[1]);
		if (!tmp)
			break;
		tmp.rawout(format(
			":%s SUMMON %s :%s",
			origin.nick,
			p[1],
			tmp.nick
		));
		break;
	case "TIME":
		if (!p[0] || origin.server)
			break;
		if (wildmatch(ServerName, p[0])) {
			origin.numeric391();
			break;
		}
		tmp = searchbyserver(p[0]);
		if (!tmp)
			break;
		tmp.rawout(format(
			":%s TIME :%s",
			origin.nick,
			tmp.nick
		));
		break;
	case "TOPIC":
		if (!p[3])
			break;
		tmp = Channels[p[0].toUpperCase()];
		if (!tmp)
			break;
		if (p[3] == tmp.topic)
			break;
		tmp.topictime = this.hub ? p[2] : Epoch();
		tmp.topic = p[3];
		tmp.topicchangedby = p[1];
		origin.bcast_to_channel(tmp, format(
			"TOPIC %s :%s",
			tmp.nam,
			tmp.topic
		),false /*bcast*/);
		this.bcast_to_servers_raw(format(
			":%s TOPIC %s %s %d :%s",
			origin.nick,
			tmp.nam,
			p[1],
			tmp.topictime,
			tmp.topic
		));
		break;
	case "TRACE":
		if (!p[0] || origin.server)
			break;
		origin.do_trace(p[0]);
		break;
	case "USERS":
		if (!p[0] || origin.server)
			break;
		tmp = searchbyserver(p[0]);
		if (!tmp)
			break;
		if (tmp == -1) {
			origin.numeric351();
			break;
		}
		tmp.rawout(format(
			":%s USERS :%s",
			origin.nick,
			tmp.nick
		));
		break;
	case "VERSION":
		if (!p[0] || origin.server)
			break;
		tmp = searchbyserver(p[0]);
		if (!tmp)
			break;
		if (tmp == -1) {
			origin.numeric351();
			break;
		}
		tmp.rawout(format(
			":%s VERSION :%s",
			origin.nick,
			tmp.nick
		));
		break;
	case "WALLOPS":
		if (!p[0])
			break;
		Write_All_Opers(format(
			":%s WALLOPS :%s",
			origin.nick,
			p[0]
		));
		this.bcast_to_servers_raw(format(
			":%s WALLOPS :%s",
			origin.nick,
			p[0]
		));
		break;
	case "WHOIS":
		if (!p[1] || origin.server)
			break;
		tmp = searchbyserver(p[0]);
		if (!tmp)
			break;
		if (tmp == -1) {
			k = p[1].split(",");
			for (i in k) {
				n = Users[k[i].toUpperCase()];
				if (n) {
					origin.do_whois(n);
					continue;
				}
				origin.numeric401(k[i]);
			}
			origin.numeric(318, format(
				"%s :End of /WHOIS list.",
				k[0]
			));
			break;
		}
		tmp.rawout(format(
			":%s WHOIS %s :%s",
			origin.nick,
			tmp.nick,
			p[1]
		));
		break;
	case "CAPAB":
	case "BURST":
		return 0; /* Silently ignore these commands */
	default:
		umode_notice(USERMODE_OPER,"Notice",format(
			"Server %s sent unrecognized command: %s %s",
			origin.nick,
			cmd.verb,
			p.join(" ")
		));
		return 0;
	}

	/* This part only executed if the command was legal. */

	if (!Profile[cmd.verb])
		Profile[cmd.verb] = new StatsM;
	Profile[cmd.verb].executions++;
	Profile.ticks += system.timer - clockticks;
}

////////// Functions //////////

function server_bcast_to_servers(str,type) {
	var i;

	for (i in Local_Servers) {
		if (!type || (Local_Servers[i].type == type))
			Local_Servers[i].rawout(str);
	}
}

function IRCClient_bcast_to_servers(str) {
	var i;

	for (i in Local_Servers) {
		if (Local_Servers[i].nick != this.parent)
			Local_Servers[i].originatorout(str,this);
	}
}

function IRCClient_bcast_to_servers_raw(str) {
	var i;

	for (i in Local_Servers) {
		if (Local_Servers[i].nick != this.parent)
			Local_Servers[i].rawout(str);
	}
}

function Server_Quit(str,suppress_bcast,is_netsplit,origin) {
	if (!str)
		str = this.nick;

	if (is_netsplit) {
		this.netsplit(str);
	} else if (this.local) {
		this.netsplit(ServerName + " " + this.nick);
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

	if (this.socket.outbound) {
		if (YLines[this.ircclass].active > 0) {
			YLines[this.ircclass].active--;
			log(LOG_DEBUG, format("Class %s down to %d active out of %d",
				this.ircclass,
				YLines[this.ircclass].active,
				YLines[this.ircclass].maxlinks
			));
		} else {
			log(LOG_ERR, format("Class %d YLine going negative", this.ircclass));
		}
	}

	if (this.local) {
		this.recvq.purge();
		this.sendq.purge();

		if (server.client_remove !== undefined)
			server.client_remove(this.socket);

		gnotice("Closing Link: " + this.nick + " (" + str + ")");

		if (this.socket !== undefined) {
			if (this.socket.is_connected) {
				this.socket.send(format(
					"ERROR :Closing Link: [%s@%s] (%s)",
					this.uprefix,
					this.hostname,
					str
				));
			}
			log(LOG_NOTICE,format(
				"Connection with server %s was closed. (%s)",
				this.nick,
				str
			));
			if (this.socket.callback_id !== undefined) {
				this.socket.clearOn("read", this.socket.callback_id);
				delete this.socket.callback_id;
			}
			this.socket.close();
			delete this.socket;
		}

		js.clearInterval(this.pinginterval);
	}

	delete Local_Servers[this.nick.toLowerCase()];
	delete Servers[this.nick.toLowerCase()];
	delete this;
}

function IRCClient_synchronize() {
	var i;

	log(LOG_NOTICE,format(
		"Connection established with server %s",
		this.nick
	));

	this.rawout("BURST"); // warn of impending synchronization
	for (i in Servers) {
		if (Servers[i].id != this.id)
			this.server_info(Servers[i]);
	}
	for (i in Users) {
		this.server_nick_info(Users[i]);
	}
	for (i in Channels) {
		if (i[0] == "#")
			this.server_chan_info(Channels[i]);
	}

	gnotice(format(
		"%s has processed user/channel burst, sending topic burst.",
		this.nick
	));

	for (i in Channels) {
		if ((i[0] == "#") && Channels[i].topic) {
			this.rawout(format(
				"TOPIC %s %s %s :%s",
				Channels[i].nam,
				Channels[i].topicchangedby,
				Channels[i].topictime,
				Channels[i].topic
			));
		}
	}
	this.rawout("BURST 0"); /* burst completed. */

	gnotice(format(
		"%s has processed topic burst (synched to network data).",
		this.nick
	));
}

function IRCClient_server_info(sni_server) {
	this.rawout(format(
		":%s SERVER %s %s :%s",
		sni_server.linkparent,
		sni_server.nick,
		parseInt(sni_server.hops)+1,
		sni_server.info
	));
}

function IRCClient_server_nick_info(sni_client) {
	this.rawout(
		format("NICK %s %s %s %s %s %s %s 0 %s :%s",
			sni_client.nick,
			parseInt(sni_client.hops) + 1,
			sni_client.created,
			sni_client.get_usermode(true),
			sni_client.uprefix,
			sni_client.hostname,
			sni_client.servername,
			ip_to_int(sni_client.ip),
			sni_client.realname
		)
	);

	if (sni_client.away) {
		this.rawout(format(":%s AWAY :%s",
			sni_client.nick,
			sni_client.away
		));
	}
}

function IRCClient_reintroduce_nick(nick) {
	if (Enforcement) {
		log(LOG_DEBUG, "Trying to reintroduce nick while Enforcement mode is off.");
		return 0;
	}

	this.server_nick_info(nick);

	for (uchan in nick.channels) {
		var chan = nick.channels[uchan];
		var cmodes = "";
		if (chan.modelist[CHANMODE_OP][nick.id])
			cmodes += "@";
		if (chan.modelist[CHANMODE_VOICE][nick.id])
			cmodes += "+";
		this.rawout(
			format("SJOIN %s %s %s :%s%s",
				chan.created,
				chan.nam,
				chan.chanmode(true),
				cmodes,
				nick.nick
			)
		);
		if (chan.topic) {
			this.rawout(
				format("TOPIC %s %s %s :%s",
					chan.nam,
					chan.topicchangedby,
					chan.topictime,
					chan.topic
				)
			);
		}
	}
}

function IRCClient_server_chan_info(sni_chan) {
	var i;

	this.rawout(format("SJOIN %s %s %s :%s",
			sni_chan.created,
			sni_chan.nam,
			sni_chan.chanmode(true),
			sni_chan.occupants()
	));
	var modecounter=0;
	var modestr="+";
	var modeargs="";
	for (i in sni_chan.modelist[CHANMODE_BAN]) {
		modecounter++;
		modestr += "b";
		if (modeargs)
			modeargs += " ";
		modeargs += sni_chan.modelist[CHANMODE_BAN][i];
		if (modecounter >= MAX_MODES) {
			this.ircout(format("MODE %s %s %s",
				sni_chan.nam,
				modestr,
				modeargs
			));
			modecounter=0;
			modestr="+";
			modeargs="";
		}
	}
	if (modeargs) {
		this.ircout(format("MODE %s %s %s",
			sni_chan.nam,
			modestr,
			modeargs
		));
	}
}

function gnotice(str) {
	umode_notice(USERMODE_ROUTING,"Routing","from " + ServerName + ": " + str);
	server_bcast_to_servers("GNOTICE :" + str);
}
