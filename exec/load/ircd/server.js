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

 Copyright 2003-2023 Randy Sommerfeld <cyan@synchro.net>

*/

////////// Objects //////////
function IRC_Server() {
	////////// VARIABLES
	// Bools/Flags that change depending on connection state.
	this.hub = false;	// are we a hub?
	this.local = true;	// are we a local socket?
	this.pinged = false;	// have we sent a PING?
	this.quitting = false;	// are we already inside quit()?
	this.server = true;	// yep, we're a server.
	this.uline = false;	// are we services?
	this.admitted = false;	// clock-checked and announced to the network?
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
	this.connecttime = Epoch();
	this.sent_msgs = 0;
	this.sent_bytes = 0;
	this.recv_msgs = 0;
	this.recv_bytes = 0;
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
	var join_ts, join_chans_str;
	var sjoin_ts, ts_rejected, valid_nicks, now, mode_ts;

	log(LOG_DEBUG,format("[%s<-%s]: %s",ServerName,this.nick,IRC_log_command(cmdline)));

	cmd = IRC_parse(cmdline);

	if (!cmd.source.name) {
		cmd.source.name = this.nick;
		cmd.source.is_server = true;
	}
	if (!this.admitted && !this.uline
		&& ["CLOCK", "PING", "PONG", "ERROR", "SVINFO"].indexOf(cmd.verb) < 0) {
		this.quit("State received before server clock verification");
		return 0;
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

	/* A server prefix is an assertion by the authenticated peer, not a
	   channel user's MODE request.  Before accepting either kind of MODE
	   (or a KICK), make sure the claimed origin is actually on this link. */
	if ((cmd.verb == "MODE" || cmd.verb == "KICK"
		|| cmd.verb == "JOIN" || cmd.verb == "SJOIN" || cmd.verb == "TOPIC" || cmd.verb == "TSMAP" || cmd.verb == "LINKSEC"
		|| cmd.verb == "PART" || cmd.verb == "PRIVMSG" || cmd.verb == "NOTICE"
		|| cmd.verb == "NICK" || cmd.verb == "QUIT" || cmd.verb == "SERVER" || cmd.verb == "CLIENTTLS")
		&& origin !== this
		&& (origin.local || !origin.parent
			|| origin.parent.toLowerCase() != this.nick.toLowerCase())
	) {
		umode_notice(USERMODE_OPER, "Notice", format(
			"Ignoring %s from %s: origin %s is on another link",
			cmd.verb, this.nick, origin.nick
		));
		return 0;
	}

	this.idletime = system.timer;
	this.recv_msgs++;
	this.recv_bytes += cmdline.length;

	p = cmd.params;

	if (cmd.verb.match(/^[0-9]+/)) { /* Passing on a numeric to a client */
		if (!p[0])
			return 0;
		var tmp = Users[p[0].toUpperCase()];
		if (!tmp)
			return 0;
		/* Derive WHOIS path security at the requesting client's own server.
		   A legacy 671 describes only the target's client socket. */
		if (tmp.local && (cmd.verb == "671" || (cmd.verb == "320" && String(p[2]).indexOf("TLS path:") == 0))) return 0;
		tmp.rawout(cmdline);
		if (tmp.local && cmd.verb == "311" && p[1] && Users[p[1].toUpperCase()]) IRC_whois_tls(tmp, Users[p[1].toUpperCase()]);
		return 1;
	}

	switch(cmd.verb) {
	case "CLIENTTLS":
		IRC_receive_clienttls(this, origin, p);
		return 0;
	case "LINKSEC":
		IRC_receive_linksec(this, origin, p);
		return 0;
	case "TSMAP":
		IRC_receive_tsmap(this, origin, p);
		return 0;
	case "CLOCK":
		if (origin === this && !this.uline)
			IRC_receive_clock(this, p);
		return 0;
	case "SVINFO":
		/* Legacy services may send standard TS negotiation. They are not
		   required to implement our freshness-checked CLOCK extension. */
		return 0;
	case "CSYNC":
		/* Only our directly connected, configured hub can replace leaf
		   state. Ordinary SJOIN timestamp ordering never grants this power. */
		if (origin !== this || !this.hub)
			break;
		IRC_receive_channel_sync(this, p);
		break;
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
			":%s AKILL %s %s %d %s %lu :%s",
			origin.nick,
			p[0],
			p[1],
			parseInt(p[2]),
			p[3],
			parseInt(p[4]),
			p[5]
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
		if (!p[0] || !p[1] || origin.server)
			break;
		tmp = Channels[p[1].toUpperCase()];
		if (!tmp)
			break;
		if (!origin.uline && !tmp.modelist[CHANMODE_OP][origin.id])
			break;
		j = Users[p[0].toUpperCase()];
		if (!j)
			break;
		if (j.channels[tmp.nam.toUpperCase()])
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
		/* Burst JOIN may include a channel timestamp as the first argument:
		     :nick JOIN <ts> <channel>
		   Traditional JOIN has no ts:
		     :nick JOIN <channel>[,<channel>...]
		   Detect by checking if p[0] looks like a numeric timestamp. */
		join_ts = 0;
		join_chans_str = p[0];
		if (p[1] && p[0].match(/^[0-9]+$/)) {
			join_ts = IRC_parse_channel_timestamp(p[0]);
			if (join_ts === null || (join_ts === 0 && !this.uline && !origin.uline && !IRC_legacy_zero_timestamp(this))) {
				this.quit("Invalid JOIN channel timestamp");
				return 0;
			}
			join_chans_str = p[1];
		}
		k = join_chans_str.split(",");
		for (i in k) {
			if (k[i][0] != "#")
				continue;
			IRC_server_join(this, origin, k[i].slice(0,MAX_CHANLEN), join_ts);
		}
		break;
	case "KICK":
		if (!p[1]) {
			log(LOG_DEBUG, "KICK: missing target param");
			break;
		}
		tmp = Channels[p[0].toUpperCase()];
		if (!tmp) {
			log(LOG_DEBUG, format("KICK: channel %s not found", p[0]));
			break;
		}
		j = Users[p[1].toUpperCase()];
		if (!j)
			j = search_nickbuf(p[1]);
		if (!j) {
			log(LOG_WARNING, format("KICK: target user %s not found", p[1]));
			break;
		}
		if (!j.channels[tmp.nam.toUpperCase()]) {
			log(LOG_WARNING, format("KICK: %s not in %s", j.nick, tmp.nam));
			break;
		}
		if (!origin) {
			log(LOG_ERR, format("KICK: origin is null for %s from %s", p[1], this.nick));
			break;
		}
		if (!this.hub && !origin.uline && !tmp.modelist[CHANMODE_OP][origin.id]) {
			log(LOG_WARNING, format("KICK: %s is not opped in %s (from non-hub %s); rejecting.",
				origin.nick, tmp.nam, this.nick));
			umode_notice(USERMODE_OPER, "Notice", format(
				"%s attempted KICK of %s from %s without ops (via %s)",
				origin.nick, j.nick, tmp.nam, this.nick
			));
			IRC_resync_leaf(this, tmp);
			break;
		}
		log(LOG_DEBUG, format("KICK: %s kicked %s from %s (origin=%s local=%s, target local=%s, from server=%s)",
			origin.nick,
			j.nick, tmp.nam,
			origin.local ? "yes" : "no",
			j.local ? "yes" : "no",
			this.nick));
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
		if (!this.hub && !origin.uline)
			this.ircout(format("KICK %s %s :%s", tmp.nam, j.nick, p[2] || j.nick));
		j.recvq.filter_channel(tmp.nam);
		j.flush_channel_sendqs(tmp);
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
				this.quit("Non-Hub kill denied.");
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
		if (!p[1]) {
			umode_notice(USERMODE_OPER,"Notice",format(
				"Origin %s sent MODE without enough arguments.",
				this.nick
			));
			break;
		}
		/* A numeric p[1] is a TSMODE channel timestamp and must be consumed
		   even when it is 0 - services (ircservices) sends a ts of 0 to mean
		   "apply unconditionally".  Testing the value's truthiness would leave
		   the "0" in place, where it gets misparsed as the mode string and the
		   real modes silently vanish. */
		mode_ts = 0;
		if (/^[0-9]+$/.test(p[1])) {
			mode_ts = IRC_parse_channel_timestamp(p[1]);
			if (mode_ts === null) {
				this.quit("Invalid MODE channel timestamp");
				return 0;
			}
			p.splice(1,1);
		}
		if (p[0][0] == "#") {
			/* Setting a channel mode */
			tmp = Channels[p[0].toUpperCase()];
			if (!tmp) {
				umode_notice(USERMODE_OPER,"Notice",format(
					"Origin %s sent MODE with invalid channel %s",
					this.nick,
					p[0]
				));
				break;
			}
			/* Legacy bursts use separate server MODEs after JOIN. Accept
			   those only for a channel first introduced by this leaf, while
			   every member still belongs to it. Established channels never
			   gain this exemption. */
			var founder = p[1] == "+o" && p.length == 3 ? Users[p[2].toUpperCase()] : null;
			var founder_grant = origin === this && founder
				&& tmp.pending_founder == founder.id && tmp.users[founder.id]
				&& founder.parent == this.nick && (!mode_ts || mode_ts == tmp.created);
			/* Relayed server grants retain their original server prefix.
			   Track live creation separately from burst and manual ops. */
			var creator_grant = origin.server && !origin.uline && !this.bursting
				&& founder && tmp.pending_founder == founder.id;
			if (!this.hub && !origin.uline
				&& (origin.server ? !(founder_grant || IRC_leaf_burst_channel(this, origin, tmp))
					: !tmp.modelist[CHANMODE_OP][origin.id])) {
				umode_notice(USERMODE_OPER,"Notice",format(
					"Leaf %s: %s attempted MODE %s without ops - bouncing",
					this.nick, origin.nick, tmp.nam
				));
				IRC_resync_leaf(this, tmp, p.slice(1));
				break;
			}
			/* Having channel ops authorizes a MODE change, never a change
			   to the hub's canonical channel age. */
			if (!this.hub && !this.uline && !origin.uline)
				mode_ts = tmp.created;
			p.shift();
			p = IRC_exception_filter_modes(p, IRC_has_chanexcept(this));
			if (!p) break;
			var creator_result = origin.set_chanmode(tmp, p, mode_ts,
				{merge: this.bursting && !origin.uline});
			if (creator_grant && creator_result !== 2 && tmp.modelist[CHANMODE_OP][founder.id])
				tmp.services_creator_op = founder.id;
			break;
		}
		/* Setting a user mode */
		if (typeof origin.setusermode !== 'function') {
			umode_notice(USERMODE_OPER,"Notice",format(
				"Server %s sent non-channel MODE from invalid origin.",
				this.nick
			));
			break;
		}
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
				if (Enforcement) {
					tmp.numeric(436, tmp.nick + " :Nickname Collision KILL.");
					server_bcast_to_servers(format(
						":%s KILL %s :Nickname Collision.",
						ServerName,
						tmp.nick
					));
					tmp.quit("Nickname Collision",true);
					/* Kill the incoming nick too - both sides lose */
					this.ircout(format(
						"KILL %s :Nickname Collision.",
						p[0]
					));
				}
				break;
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
							this.nick, p[0]
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
			/* 1.3 sends NICKIP as an integer; newer hubs may relay that
			   unchanged despite advertising NICKIPSTR themselves. */
			j.ip = /^[0-9]+$/.test(p[8]) && Number(p[8]) <= 4294967295
				? int_to_ip(Number(p[8])) : p[8];
			j.setusermode(p[3]);
			for (i in ULines) {
				if (ULines[i] == p[6]) {
					j.uline = true;
					break;
				}
			}
			IRC_bcast_nick(j, this);
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
					"Server %s trying to collide via NICK changeover: %s -> %s, doing SQUIT",
					this.nick,
					origin.nick,
					p[0]
				));
				this.quit("Attempted collide via NICK");
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
				":%s NICK %s :%lu",
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
			if (!this.hub && !this.uline && IRC_has_tsmap(this) && /^[0-9]+[.][0-9]+$/.test(p[1]))
				this.ircout("TSMAP PARTACK " + tmp[i] + " " + p[1]);
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
		this.pinged = false;
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
			}
		}
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
	case "RAKILL":
		if (!p[1])
			break;

		if (!origin.uline) {
			umode_notice(USERMODE_OPER,"Notice",format(
				"Non-U:Lined server %s trying to utilize RAKILL.",
				origin.nick
			));
			break;
		}

		this.bcast_to_servers_raw(format(
			":%s RAKILL %s %s",
			origin.nick,
			p[0],
			p[1]
		));

		k = p[1] + "@" + p[0];
		if (!isklined(k))
			break;
		remove_kline(k);
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
				if (IRC_Services && IRC_Services.owns_server(p[0])) IRC_Services.stop_central(this);
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
		sjoin_ts = IRC_parse_channel_timestamp(p[0]);
		if (sjoin_ts === null || (sjoin_ts === 0 && !this.uline && !origin.uline && !IRC_legacy_zero_timestamp(this))) {
			this.quit("Invalid SJOIN channel timestamp");
			return 0;
		}
		/* Services use zero as an unspecified timestamp. Old daemons also
		   echo zero after receiving authoritative MODE 0 corrections. Keep
		   a real network age; the normal route and leaf authority checks
		   below still apply to every member, flag, and status claim. */
		if (sjoin_ts === 0) {
			var service_channel = Channels[p[1].toUpperCase()];
			sjoin_ts = service_channel ? service_channel.created : Epoch();
		}
		if (!p[2]) {
			/* Preserve the joining user and original TS through every hop. */
			if (!origin.server)
				IRC_server_join(this, origin, p[1], sjoin_ts);
			break;
		}
		if (!origin.server || p.length < 4)
			break;

		/* Validate members before changing the channel's timestamp or modes.
		   A snapshot can introduce only users routed through its sender. */
		var members = [], seen_members = {}, member_modes = p.slice(2, -1);
		member_modes = IRC_exception_filter_modes(member_modes, IRC_has_chanexcept(this)) || ["+"];
		j = p[p.length - 1].split(/ +/);
		for (i = 0; i < j.length; i++) {
			if (!j[i]) continue;
			k = new SJOIN_Nick(j[i]);
			n = Users[k.nick.toUpperCase()];
			if (!n || n.local || !n.parent
				|| n.parent.toLowerCase() != this.nick.toLowerCase()) {
				umode_notice(USERMODE_OPER, "Notice", format(
					"Ignoring SJOIN member %s from wrong or unknown route %s",
					k.nick, this.nick));
				continue;
			}
			if (seen_members[n.id]) continue;
			seen_members[n.id] = true;
			members.push({user: n, isop: k.isop, isvoice: k.isvoice});
		}
		if (!members.length)
			break;
		tmp = Channels[p[1].toUpperCase()];
		var leaf_assertion = !this.hub && !origin.uline;
		var existing_channel = !!tmp;
		var leaf_ts = leaf_assertion ? IRC_leaf_timestamp(this, p[1]) : sjoin_ts;
		if (leaf_ts === null) return 0;
		if (!tmp) {
			tmp = Channels[p[1].toUpperCase()] = new Channel(p[1]);
			/* A leaf cannot backdate a channel it first introduces to the
			   hub. The hub assigns its network timestamp once, then sends
			   that same canonical value back to the leaf. */
			tmp.created = leaf_ts;
			if (leaf_assertion && this.bursting) tmp.pending_burst = this;
		}
		if (leaf_assertion && existing_channel)
			ts_rejected = true;
		else
			ts_rejected = this.set_chanmode(tmp, member_modes, leaf_assertion ? 0 : sjoin_ts,
				{merge: !origin.uline, local_only: true}) === 2;
		valid_nicks = [];
		if (tmp.pending_burst && tmp.pending_burst !== this)
			delete tmp.pending_burst;
		for (i = 0; i < members.length; i++) {
			k = members[i];
			n = k.user;
			if (!n.channels[tmp.nam.toUpperCase()]) {
				tmp.users[n.id] = n;
				n.channels[tmp.nam.toUpperCase()] = tmp;
				n.bcast_to_channel(tmp, "JOIN :" + tmp.nam, false);
			}
			var status_modes = "", status_args = "";
			if (!ts_rejected && k.isop && !tmp.modelist[CHANMODE_OP][n.id]) {
				tmp.modelist[CHANMODE_OP][n.id] = n;
				status_modes += "o";
				status_args += " " + n.nick;
			}
			if (!ts_rejected && k.isvoice && !tmp.modelist[CHANMODE_VOICE][n.id]) {
				tmp.modelist[CHANMODE_VOICE][n.id] = n;
				status_modes += "v";
				status_args += " " + n.nick;
			}
			if (status_modes)
				origin.bcast_to_channel(tmp, "MODE " + tmp.nam + " +"
					+ status_modes + status_args, false);
			if (!existing_channel && !this.bursting && !origin.uline
				&& members.length == 1 && !ts_rejected && k.isop)
				tmp.services_creator_op = n.id;
			valid_nicks.push(leaf_assertion
				? (tmp.modelist[CHANMODE_OP][n.id] ? "@" : "")
					+ (tmp.modelist[CHANMODE_VOICE][n.id] ? "+" : "") + n.nick
				: (k.isop ? "@" : "") + (k.isvoice ? "+" : "") + n.nick);
		}
		/* Forward the original claim, not a mixture of the winning local TS
		   and losing remote members. Each receiver resolves the same event. */
		var relay = format(":%s SJOIN %lu %s %s :%s",
			leaf_assertion ? ServerName : origin.nick,
			leaf_assertion ? tmp.created : sjoin_ts, tmp.nam,
			leaf_assertion ? tmp.chanmode(true) : member_modes.join(" "), valid_nicks.join(" "));
		for (var link in Local_Servers) {
			var recipient = Local_Servers[link];
			if (recipient === this || !recipient.admitted) continue;
			if (IRC_legacy_leaf(recipient)) {
				IRC_resync_leaf(recipient, tmp);
			} else recipient.rawout(relay);
		}
		if (leaf_assertion)
			IRC_resync_leaf(this, tmp, member_modes);
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
		if (p[3] === undefined)
			break;
		tmp = Channels[p[0].toUpperCase()];
		if (!tmp) {
			log(LOG_DEBUG, format("TOPIC: channel %s not found", p[0]));
			break;
		}
		if (!origin) {
			log(LOG_ERR, format("TOPIC: origin is null for %s from %s", p[0], this.nick));
			break;
		}
		var leaf_topic = !this.hub && !origin.uline && !this.uline;
		if (IRC_legacy_leaf(this)) IRC_note_legacy_topic(this, tmp, p[2]);
		var topic_ts = leaf_topic ? Math.max(Epoch(), tmp.topictime + 1)
			: IRC_parse_channel_timestamp(p[2]);
		if (topic_ts === null || !topic_ts)
			break;
		if (leaf_topic) {
			if (!IRC_leaf_burst_channel(this, origin, tmp)
				&& (origin.server || !origin.channels[tmp.nam.toUpperCase()]
					|| ((tmp.mode & CHANMODE_TOPIC) && !tmp.modelist[CHANMODE_OP][origin.id]))) {
				IRC_resync_leaf(this, tmp);
				break;
			}
			/* As with channel creation, an untrusted leaf cannot choose an
			   authoritative timestamp. Preserve the hub's decision downstream. */
			topic_ts = Math.max(Epoch(), tmp.topictime + 1);
		}
		/* Preserve the source timestamp on every route. Concurrent changes
		   with equal timestamps choose the same record independent of order. */
		if (topic_ts < tmp.topictime
			|| (topic_ts == tmp.topictime
				&& (p[1] + "\x00" + p[3]) <= (tmp.topicchangedby + "\x00" + tmp.topic)))
			break;
		log(LOG_DEBUG, format("TOPIC: %s set topic on %s (origin local=%s, from server=%s)",
			origin.nick, tmp.nam,
			origin.local ? "yes" : "no",
			this.nick));
		tmp.topictime = topic_ts;
		tmp.topic = p[3];
		tmp.topicchangedby = p[1];
		origin.bcast_to_channel(tmp, format(
			"TOPIC %s :%s",
			tmp.nam,
			tmp.topic
		),false /*bcast*/);
		IRC_bcast_topic(origin, tmp, this);
		if (leaf_topic)
			IRC_send_topic_sync(this, tmp);
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
	case "SVSMODE":
		return 0; /* Silently ignore these commands */
	case "BURST":
		this.bursting = p[0] !== "0";
		if (!this.bursting) {
			for (i in Channels)
				if (Channels[i].pending_burst === this) delete Channels[i].pending_burst;
		}
		if (!this.bursting && this.hub) IRC_admit_waiting_leaves();
		if (!this.bursting && !this.hub && !this.uline) {
			for (i in Channels)
				IRC_resync_leaf(this, Channels[i]);
		}
		return 0;
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
		Profile[cmd.verb] = new StatsM();
	Profile[cmd.verb].executions++;
	Profile[cmd.verb].ticks += system.timer - clockticks;
}

////////// Functions //////////

function IRC_leaf_has_member(peer, chan) {
	for (var i in chan.users)
		if (chan.users[i].parent == peer.nick)
			return true;
	return false;
}

function IRC_legacy_leaf(peer) {
	return !peer.hub && !peer.uline
		&& (!peer.their_capab || peer.their_capab.indexOf("CSYNC") < 0);
}

function IRC_legacy_zero_timestamp(peer) {
	return !peer.their_capab || peer.their_capab.indexOf("CSYNC") < 0;
}

/* Older daemons do not consistently hide explicit client queries. Give
   these leaves a partial view, but keep hubs/services and FULLCHAN peers
   fully informed. CSYNC alone is insufficient: released 2.4 lacks the
   client reply fixes too. Entries live on the channel and expire with it. */
function IRC_filtered_leaf(peer) {
	return !peer.hub && !peer.uline
		&& (!peer.their_capab || peer.their_capab.indexOf("FULLCHAN") < 0);
}

function IRC_leaf_view(peer, chan) {
	if (!chan.leaf_views) chan.leaf_views = {};
	var view = chan.leaf_views[peer.nick];
	if (!view || view.peer !== peer)
		view = chan.leaf_views[peer.nick] = {peer: peer, visible: false, users: {}};
	return view;
}

function IRC_hide_leaf_channel(peer, chan, view) {
	if (!view.visible) return;
	view.visible = false;
	/* Synthetic PARTs withdraw only the leaf's cached remote memberships.
	   They never change hub state or propagate to other links. */
	peer.withdrawing_channel = true;
	try {
		for (var id in view.users) {
			var user = view.users[id];
			if (chan.users[id] === user && user.parent != peer.nick)
				peer.rawout(":" + user.nick + " PART " + chan.nam);
		}
	} finally { delete peer.withdrawing_channel; }
	view.users = {};
}

function IRC_prepare_leaf_channel(peer, chan) {
	if (!IRC_filtered_leaf(peer)) return true;
	var view = IRC_leaf_view(peer, chan);
	if ((chan.mode & (CHANMODE_SECRET | CHANMODE_PRIVATE)) && !IRC_leaf_has_member(peer, chan)) {
		IRC_hide_leaf_channel(peer, chan, view);
		return false;
	}
	view.visible = true;
	return true;
}

function IRC_refresh_leaf_views(chan, departed) {
	if (!chan.leaf_views) return;
	for (var name in chan.leaf_views) {
		var view = chan.leaf_views[name], peer = view.peer;
		delete view.users[departed.id];
		if (peer.admitted && !peer.quitting && IRC_filtered_leaf(peer)
			&& (chan.mode & (CHANMODE_SECRET | CHANMODE_PRIVATE)) && !IRC_leaf_has_member(peer, chan))
			IRC_hide_leaf_channel(peer, chan, view);
	}
}

function IRC_filter_leaf_output(peer, line) {
	if (!IRC_filtered_leaf(peer) || peer.withdrawing_channel) return true;
	var cmd = IRC_parse(line), p = cmd.params, name;
	switch (cmd.verb) {
	case "SJOIN": case "CSYNC": name = p[1]; break;
	case "JOIN": name = p.length > 1 ? p[1] : p[0]; break;
	case "MODE": case "TOPIC": case "PART": case "KICK":
	case "PRIVMSG": case "NOTICE": name = p[0]; break;
	default: return true;
	}
	if (!name) return true;
	name = name.replace(/^[@+]/, "");
	var chan = Channels[name.toUpperCase()];
	if (!chan) return true;
	var known = chan.leaf_views && chan.leaf_views[peer.nick]
		&& chan.leaf_views[peer.nick].peer === peer;
	var view = IRC_leaf_view(peer, chan);
	if ((chan.mode & (CHANMODE_SECRET | CHANMODE_PRIVATE)) && !IRC_leaf_has_member(peer, chan)) {
		IRC_hide_leaf_channel(peer, chan, view);
		return false;
	}
	if (!view.visible) {
		view.visible = true;
		/* A normal public founder JOIN starts an incremental view. Every
		   hidden-to-visible transition needs all old members and their ops
		   before any further user-prefixed MODE can be accepted. */
		if (known || cmd.verb != "JOIN") {
			peer.server_chan_info(chan);
			if (chan.topictime) IRC_send_topic(peer, chan);
			if (["JOIN", "SJOIN", "MODE", "TOPIC", "CSYNC"].indexOf(cmd.verb) >= 0)
				return false;
		}
	}
	var user, members, i;
	if (cmd.verb == "JOIN") {
		user = Users[cmd.source.name.toUpperCase()];
		if (user && user.parent != peer.nick) view.users[user.id] = user;
	} else if (cmd.verb == "SJOIN" || (cmd.verb == "CSYNC" && p[2] == "MORE")) {
		members = p[p.length - 1].split(" ");
		for (i = 0; i < members.length; i++) {
			user = Users[members[i].replace(/^[@+]*/, "").toUpperCase()];
			if (user && user.parent != peer.nick) view.users[user.id] = user;
		}
	} else if (cmd.verb == "CSYNC" && p[2] == "BEGIN") {
		view.users = {};
	} else if (cmd.verb == "PART" || cmd.verb == "KICK") {
		user = Users[(cmd.verb == "PART" ? cmd.source.name : p[1]).toUpperCase()];
		if (user) delete view.users[user.id];
	}
	return true;
}

function IRC_mode_timestamp(peer, ts) {
	return peer.their_capab && (peer.their_capab.indexOf("TSMODE") >= 0
		|| peer.their_capab.indexOf("TSJOIN") >= 0) ? " " + ts + " " : " ";
}

function IRC_leaf_burst_channel(peer, origin, chan) {
	if (origin !== peer || !peer.bursting || chan.pending_burst !== peer) return false;
	for (var i in chan.users)
		if (chan.users[i].parent != peer.nick) {
			delete chan.pending_burst;
			return false;
		}
	return true;
}

/* Old leaves reject non-newer topics, even when their clock is ahead of the
   hub. Keep a presentation counter on this channel for that leaf only. It
   never becomes a network timestamp or travels over a hub link. */
function IRC_note_legacy_topic(peer, chan, ts) {
	if (!chan.legacy_topics) chan.legacy_topics = {};
	var entry = chan.legacy_topics[peer.nick];
	if (!entry || entry.peer !== peer)
		entry = chan.legacy_topics[peer.nick] = {peer: peer, time: 0};
	var parsed = IRC_parse_channel_timestamp(ts);
	if (parsed !== null) entry.time = Math.max(entry.time, parsed);
	return entry;
}

function IRC_send_topic(peer, chan, origin) {
	var ts = chan.topictime;
	if (IRC_legacy_leaf(peer)) {
		var entry = IRC_note_legacy_topic(peer, chan, ts);
		ts = entry.time = Math.max(ts, entry.time + 1);
	}
	var line = "TOPIC " + chan.nam + " " + (chan.topicchangedby || ServerName)
		+ " " + ts + " :" + chan.topic;
	/* Server prefixes are implicit on direct links. Omitting them also
	   keeps maximum-length topics and channel names inside the wire limit. */
	if (!origin || origin.server) peer.rawout(line);
	else peer.originatorout(line, origin);
}

function IRC_bcast_topic(origin, chan, except) {
	for (var i in Local_Servers) {
		var peer = Local_Servers[i];
		if (peer !== except && peer.admitted && peer.nick != origin.parent)
			IRC_send_topic(peer, chan, origin);
	}
}

function IRC_send_topic_sync(peer, chan) {
	if (!peer.their_capab || peer.their_capab.indexOf("CSYNC") < 0) {
		IRC_send_topic(peer, chan);
		return;
	}
	/* CSYNC is accepted only from the directly configured hub. Its implicit
	   prefix saves enough wire space for maximum-length topics/channels. */
	peer.rawout("CSYNC " + chan.created + " " + chan.nam + " TOPIC "
		+ (chan.topicchangedby || ServerName) + " " + chan.topictime + " :" + chan.topic);
}

function IRC_server_join(peer, user, name, ts) {
	var chan = Channels[name.toUpperCase()];
	var fresh = !chan;
	var leaf = !peer.hub && !user.uline && !peer.uline;
	if (user.channels[name.toUpperCase()]) {
		delete peer.timestamp_claim;
		return;
	}
	/* A hub is the authority at the untrusted-leaf boundary. Existing
	   channel incarnations cannot be replaced by a leaf's claimed age. */
	if (leaf)
		ts = IRC_leaf_timestamp(peer, name);
	if (ts === null) return;
	if (user.do_join(name, "", ts) && leaf) {
		chan = Channels[name.toUpperCase()];
		if (fresh && peer.bursting) chan.pending_burst = peer;
		IRC_resync_leaf(peer, chan);
	}
}

function IRC_resync_leaf(peer, chan, rejected_modes) {
	if (peer.hub || peer.uline)
		return;
	if (!IRC_prepare_leaf_channel(peer, chan)) return;
	if (!peer.their_capab || peer.their_capab.indexOf("CSYNC") < 0) {
		IRC_resync_legacy_leaf(peer, chan, rejected_modes);
		return;
	}
	var header = ":" + ServerName + " CSYNC " + chan.created + " " + chan.nam + " ";
	peer.rawout(header + "BEGIN " + chan.chanmode(true));
	var names = "", i, u, token;
	for (i in chan.users) {
		u = chan.users[i];
		token = (chan.modelist[CHANMODE_OP][u.id] ? "@" : "")
			+ (chan.modelist[CHANMODE_VOICE][u.id] ? "+" : "") + u.nick;
		if (names && (header + "MORE :" + names + " " + token).length > 510) {
			peer.rawout(header + "MORE :" + names); names = "";
		}
		names += (names ? " " : "") + token;
	}
	if (names) peer.rawout(header + "MORE :" + names);
	names = "";
	for (i in chan.modelist[CHANMODE_BAN]) {
		token = chan.modelist[CHANMODE_BAN][i];
		if (names && (header + "BANS :" + names + " " + token).length > 510) {
			peer.rawout(header + "BANS :" + names); names = "";
		}
		names += (names ? " " : "") + token;
	}
	if (names) peer.rawout(header + "BANS :" + names);
	if (IRC_has_chanexcept(peer)) {
		names = "";
		for (i in chan.modelist[CHANMODE_EXCEPT]) {
			token = chan.modelist[CHANMODE_EXCEPT][i];
			if (names && (header + "EXCEPTS :" + names + " " + token).length > 510) {
				peer.rawout(header + "EXCEPTS :" + names); names = "";
			}
			names += (names ? " " : "") + token;
		}
		if (names) peer.rawout(header + "EXCEPTS :" + names);
	}
	IRC_send_topic_sync(peer, chan);
	peer.rawout(header + "END");
}

function IRC_send_legacy_flags(peer, chan) {
	for (var bit in MODE) {
		if (!MODE[bit].state || !(chan.mode & bit)) continue;
		peer.ircout("MODE " + chan.nam + IRC_mode_timestamp(peer, 0)
			+ "+" + MODE[bit].modechar + (MODE[bit].args ? " " + chan.arg[bit] : ""));
	}
}

function IRC_resync_legacy_leaf(peer, chan, rejected_modes) {
	var header = "MODE " + chan.nam + IRC_mode_timestamp(peer, 0), i, u;
	/* A rejected +b has already changed the old leaf's local list. Remove
	   just those noncanonical masks, including during its initial burst. */
	if (rejected_modes && rejected_modes.length) {
		var arg = 1, add = true, modes = rejected_modes[0];
		for (i = 0; i < modes.length; i++) {
			var m = modes[i];
			if (m == "+" || m == "-") { add = m == "+"; continue; }
			if ("bkov".indexOf(m) < 0 && !(m == "l" && add)) continue;
			var value = rejected_modes[arg++];
			if (m != "b" || !add || !value) continue;
			var mask = create_ban_mask(value);
			if (mask && !chan.is_str_member_of_chanmode_list(CHANMODE_BAN, mask))
				peer.ircout(header + "-b " + mask);
		}
	}
	/* Do not erase the state of a new channel between its JOINs and the
	   separate MODE/TOPIC lines in an old daemon's initial burst. Existing
	   hub channels still need their members/ops introduced immediately: a
	   runtime MODE can arrive before the old leaf finishes its own burst. */
	if (peer.bursting) {
		if (chan.pending_burst !== peer) {
			peer.server_chan_info(chan);
			if (chan.topictime) IRC_send_topic(peer, chan);
		}
		return;
	}
	for (i in chan.users) {
		u = chan.users[i];
		/* Its own members can have a PART in flight. Reintroduce only
		   remote users here; legacy peers have no PART acknowledgment. */
		if (u.parent != peer.nick) peer.rawout(":" + u.nick + " JOIN " + chan.nam);
	}
	peer.ircout(header + "-imnpst");
	peer.ircout(header + "-k *");
	peer.ircout(header + "-l");
	IRC_send_legacy_flags(peer, chan);
	for (i in chan.users) {
		u = chan.users[i];
		peer.ircout(header + "-ov " + u.nick + " " + u.nick);
		if (chan.modelist[CHANMODE_OP][u.id]) peer.ircout(header + "+o " + u.nick);
		if (chan.modelist[CHANMODE_VOICE][u.id]) peer.ircout(header + "+v " + u.nick);
	}
	for (i in chan.modelist[CHANMODE_BAN])
		peer.ircout(header + "+b " + chan.modelist[CHANMODE_BAN][i]);
	if (chan.topictime) IRC_send_topic(peer, chan);
}

function IRC_receive_channel_sync(peer, p) {
	var ts = IRC_parse_channel_timestamp(p[0]), name = p[1];
	if (!ts || !name || name[0] != "#" || name.length > MAX_CHANLEN)
		return;
	if (p[2] == "BEGIN") {
		peer.channel_sync = {ts: ts, name: name, modes: p.slice(3), members: {}, bans: []};
		if (IRC_has_chanexcept(peer)) peer.channel_sync.exceptions = [];
		return;
	}
	var sync = peer.channel_sync, i, k, u, chan;
	if (p[2] == "TOPIC" && p[5] !== undefined) {
		var topic_time = IRC_parse_channel_timestamp(p[4]);
		if (topic_time === null) return;
		var topic = {setter: p[3], time: topic_time, text: p[5]};
		if (sync && sync.ts == ts && sync.name == name)
			sync.topic = topic;
		else {
			chan = Channels[name.toUpperCase()];
			if (chan && chan.created == ts) IRC_apply_topic_sync(peer, chan, topic);
		}
		return;
	}
	if (!sync || sync.ts != ts || sync.name != name)
		return;
	if (p[2] == "MORE" && p[3]) {
		var names = p[3].split(" ");
		for (i = 0; i < names.length; i++) {
			k = new SJOIN_Nick(names[i]);
			u = Users[k.nick.toUpperCase()] || search_nickbuf(k.nick);
			if (u) sync.members[u.id] = {user: u, isop: k.isop, isvoice: k.isvoice};
		}
		return;
	}
	if (p[2] == "BANS" && p[3]) {
		sync.bans = sync.bans.concat(p[3].split(" "));
		return;
	}
	if (p[2] == "EXCEPTS" && p[3]) {
		if (sync.exceptions) sync.exceptions = sync.exceptions.concat(p[3].split(" "));
		return;
	}
	if (p[2] != "END")
		return;
	delete peer.channel_sync;
	chan = Channels[name.toUpperCase()];
	if (!chan) chan = Channels[name.toUpperCase()] = new Channel(name);
	/* This is an authoritative snapshot, not a timestamp conflict. Keep
	   matching state intact so reconnects do not flap modes or replace ban
	   metadata. Only differences should generate MODEs for local clients. */
	chan.created = ts;
	delete chan.pending_founder;
	delete chan.pending_burst;
	var flags = sync.modes[0] || "", bit, remove;
	for (bit in MODE) {
		if (!MODE[bit].state || !(chan.mode & bit) || flags.indexOf(MODE[bit].modechar) >= 0) continue;
		remove = ["-" + MODE[bit].modechar];
		if (bit == CHANMODE_KEY) remove.push(chan.arg[bit]);
		peer.set_chanmode(chan, remove, 0, {local_only: true});
	}
	var bans = {}, mask;
	for (i = 0; i < sync.bans.length; i++) {
		mask = create_ban_mask(sync.bans[i]);
		if (mask) bans["$" + mask.toUpperCase()] = true;
	}
	for (i in chan.modelist[CHANMODE_BAN]) {
		mask = chan.modelist[CHANMODE_BAN][i];
		if (!bans["$" + mask.toUpperCase()]) peer.set_chanmode(chan, ["-b", mask], 0, {local_only: true});
	}
	if (sync.exceptions) {
		var exceptions = {}, entry;
		for (i = 0; i < sync.exceptions.length; i++) {
			entry = IRC_exception_parse(sync.exceptions[i]);
			if (entry) exceptions["$" + entry.value.toUpperCase()] = entry.value;
		}
		for (i in chan.modelist[CHANMODE_EXCEPT]) {
			mask = chan.modelist[CHANMODE_EXCEPT][i];
			if (!exceptions["$" + mask.toUpperCase()]) peer.set_chanmode(chan, ["-e", mask], 0, {local_only:true});
		}
		for (i in exceptions) peer.set_chanmode(chan, ["+e", exceptions[i]], 0, {local_only:true});
	}
	peer.set_chanmode(chan, sync.modes, 0, {local_only: true});
	var members = {};
	for (i in sync.members) {
		k = sync.members[i]; u = k.user;
		if (Users[u.nick.toUpperCase()] !== u) continue;
		var pending = u.local && u.pending_channel_parts[name.toUpperCase()];
		if (pending && pending.peer === peer && !u.channels[name.toUpperCase()]) continue;
		if (!u.channels[name.toUpperCase()]) {
			chan.users[u.id] = u; u.channels[name.toUpperCase()] = chan;
			u.bcast_to_channel(chan, "JOIN :" + name, true);
		}
		members[u.id] = k;
	}
	var statuses = [{bit: CHANMODE_OP, mode: "o", field: "isop"},
		{bit: CHANMODE_VOICE, mode: "v", field: "isvoice"}];
	for (var status = 0; status < statuses.length; status++) {
		var list = statuses[status];
		for (i in chan.modelist[list.bit]) {
			u = chan.modelist[list.bit][i]; k = members[i];
			if (!k || k.user !== u || !k[list.field]) peer.set_chanmode(chan, ["-" + list.mode, u.nick], 0, {local_only: true});
		}
	}
	for (i in members) {
		k = members[i]; u = k.user;
		if (k.isop) peer.set_chanmode(chan, ["+o", u.nick], 0, {local_only: true});
		if (k.isvoice) peer.set_chanmode(chan, ["+v", u.nick], 0, {local_only: true});
	}
	for (i = 0; i < sync.bans.length; i++)
		peer.set_chanmode(chan, ["+b", sync.bans[i]], 0, {local_only: true});
	if (sync.topic) IRC_apply_topic_sync(peer, chan, sync.topic);
	if (!true_array_len(chan.users)) delete Channels[name.toUpperCase()];
}

function IRC_apply_topic_sync(peer, chan, topic) {
	if (chan.topic != topic.text)
		peer.bcast_to_channel(chan, "TOPIC " + chan.nam + " :" + topic.text, false);
	chan.topic = topic.text;
	chan.topicchangedby = topic.setter;
	chan.topictime = topic.time;
}

function server_bcast_to_servers(str,type) {
	var i;

	for (i in Local_Servers) {
		if (Local_Servers[i].admitted && (!type || (Local_Servers[i].type == type)))
			Local_Servers[i].rawout(str);
	}
}

function IRCClient_bcast_to_servers(str) {
	var i;

	for (i in Local_Servers) {
		if (Local_Servers[i].admitted && Local_Servers[i].nick != this.parent)
			Local_Servers[i].originatorout(str,this);
	}
}

function IRCClient_bcast_to_servers_raw(str) {
	var i;

	for (i in Local_Servers) {
		if (Local_Servers[i].admitted && Local_Servers[i].nick != this.parent)
			Local_Servers[i].rawout(str);
	}
}

function Reset_Autoconnect(cline, freq) {
	if (typeof cline !== 'object')
		throw "Reset_Autoconnect() called without cline object.";

	log(LOG_DEBUG,format(
		"Reset_Autoconnect() for %s on port %u.",
		cline.servername,
		cline.port
	));

	if (cline.next_connect)
		js.clearTimeout(cline.next_connect);

	if (!cline.port || Servers[cline.servername.toLowerCase()]) {
		log(LOG_DEBUG,format(
			"Reset_Autoconnect() cancelled for %s because no port or server exists.",
			cline.servername
		));
		return false;
	}

	cline.next_connect = js.setTimeout(
		Automatic_Server_Connect,
		freq,
		cline
	);

	return true;
}

function Server_Quit(str,suppress_bcast,is_netsplit,origin) {
	var cline;

	/* netsplit() quits every server whose linkparent is us, and those quits
	   netsplit() in turn.  A loop anywhere in the linkparent graph would
	   otherwise recurse until the JS engine aborts the IRCd with "too much
	   recursion", so never descend into a server that's already quitting. */
	if (this.quitting)
		return;
	this.quitting = true;
	if (this.local && !this.admitted)
		suppress_bcast = true;

	if (!str)
		str = this.nick;

	if (is_netsplit) {
		this.netsplit(str);
	} else if (this.local) {
		this.netsplit(format("%s %s", ServerName, this.nick));
		if (!suppress_bcast) {
			this.bcast_to_servers_raw(format("SQUIT %s :%s",
				this.nick,
				str
			));
		}
	} else if (origin) {
		this.netsplit(format("%s %s", origin.nick, this.nick));
		if (!suppress_bcast) {
			this.bcast_to_servers_raw(format(":%s SQUIT %s :%s",
				origin.nick,
				this.nick,
				str
			));
		}
	} else {
		umode_notice(USERMODE_OPER,"Notice","Bogus netsplit???");
		if (!suppress_bcast) {
			this.bcast_to_servers_raw(format("SQUIT %s :%s",
				this.nick,
				str
			));
		}
		this.netsplit();
	}

	if (this.local) {
		if (YLines[this.ircclass].connfreq)
			cline = Find_CLine_by_Server(this.nick);

		if (YLines[this.ircclass].active > 0)
			YLine_Decrement(YLines[this.ircclass]);

		this.recvq.purge();
		this.sendq.purge();

		if (server.client_remove !== undefined)
			server.client_remove(this.socket);

		gnotice("Closing Link: " + this.nick + " (" + str + ")");

		if (this.socket !== undefined) {
			if (this.socket.is_connected) {
				this.socket.send(format(
					"ERROR :Closing Link: [%s@%s] (%s)\r\n",
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
		if (this.clockinterval !== undefined)
			js.clearInterval(this.clockinterval);
		for (var uid in Local_Users) {
			var parts = Local_Users[uid].pending_channel_parts;
			for (var channel in parts)
				if (parts[channel].peer === this) delete parts[channel];
		}
	}

	delete Local_Servers[this.nick.toLowerCase()];
	delete Servers[this.nick.toLowerCase()];
	for (var cname in Channels) {
		var views = Channels[cname].leaf_views;
		if (views && views[this.nick] && views[this.nick].peer === this)
			delete views[this.nick];
	}
	IRC_admit_waiting_leaves();

	if (cline)
		Reset_Autoconnect(cline, YLines[this.ircclass].connfreq * 1000);
}

function IRCClient_synchronize() {
	var i;

	log(LOG_NOTICE,format(
		"Connection established with server %s",
		this.nick
	));

	this.rawout("BURST");
	for (i in Servers) {
		if (Servers[i].id != this.id && (!Servers[i].local || Servers[i].admitted))
			this.server_info(Servers[i]);
	}
	IRC_tsmap_burst(this);
	IRC_linksec_burst(this);
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
		if ((i[0] == "#") && Channels[i].topictime) {
			IRC_send_topic(this, Channels[i]);
		}
	}
	if (IRC_is_leaf_uplink(this)) this.ircout("TSMAP END " + Leaf_Process);
	this.rawout("BURST 0");

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

function IRC_bcast_nick(user, except) {
	for (var i in Local_Servers) {
		var peer = Local_Servers[i];
		if (peer.admitted && peer !== except) peer.server_nick_info(user);
	}
}

function IRC_nick_ip(peer, ip) {
	if (peer.their_capab && peer.their_capab.indexOf("NICKIPSTR") >= 0)
		/* A middle IRC parameter cannot start with ':'. Keep compressed
		   IPv6 such as ::1 from swallowing the following realname field. */
		return String(ip)[0] == ":" ? "0" + ip : ip;
	/* Old NICKIP has no IPv6 representation. Zero is its unknown address. */
	return /^(?:[0-9]{1,3}\.){3}[0-9]{1,3}$/.test(ip) ? String(ip_to_int(ip)) : "0";
}

function IRCClient_server_nick_info(sni_client) {
	this.rawout(
		format("NICK %s %d %lu %s %s %s %s 0 %s :%s",
			sni_client.nick,
			parseInt(sni_client.hops) + 1,
			sni_client.created,
			sni_client.get_usermode(true),
			sni_client.uprefix,
			sni_client.hostname,
			sni_client.servername,
			IRC_nick_ip(this, sni_client.ip),
			sni_client.realname
		)
	);

	IRC_client_tls_burst(this, sni_client);
	if (sni_client.away) {
		this.rawout(format(":%s AWAY :%s",
			sni_client.nick,
			sni_client.away
		));
	}
}

function IRCClient_server_chan_info(sni_chan) {
	if (!IRC_prepare_leaf_channel(this, sni_chan)) return;
	var i, u, token, names = "", capable = !this.uline && this.their_capab
		&& this.their_capab.indexOf("TSJOIN") >= 0 && !IRC_legacy_leaf(this);
	var timestamped_modes = this.their_capab
		&& (this.their_capab.indexOf("TSJOIN") >= 0 || this.their_capab.indexOf("TSMODE") >= 0);
	var mode_ts = IRC_legacy_leaf(this) ? 0 : sni_chan.created;
	var header = format(":%s SJOIN %lu %s %s :",
		ServerName, IRC_is_leaf_uplink(this) ? sni_chan.leaf_timestamp : sni_chan.created,
		sni_chan.nam, sni_chan.chanmode(true));

	/* FULLCHAN servers receive complete state; older leaves receive it only
	   while visible, with runtime output filtered by the same policy. */
	IRC_tsmap_claim(this, sni_chan);
	for (i in sni_chan.users) {
		u = sni_chan.users[i];
		if (u.parent == this.nick) continue;
		if (capable) {
			token = (sni_chan.modelist[CHANMODE_OP][u.id] ? "@" : "")
				+ (sni_chan.modelist[CHANMODE_VOICE][u.id] ? "+" : "") + u.nick;
			if (names && (header + names + " " + token).length > 510) {
				this.rawout(header + names);
				IRC_tsmap_claim(this, sni_chan);
				names = "";
			}
			names += (names ? " " : "") + token;
		} else {
			/* Keep the established JOIN/MODE dialect for legacy services. */
			this.rawout(format(":%s JOIN %s", u.nick, sni_chan.nam));
			if (sni_chan.modelist[CHANMODE_OP][u.id])
				this.ircout(format("MODE %s %s+o %s", sni_chan.nam,
					timestamped_modes ? mode_ts + " " : "", u.nick));
			if (sni_chan.modelist[CHANMODE_VOICE][u.id])
				this.ircout(format("MODE %s %s+v %s", sni_chan.nam,
					timestamped_modes ? mode_ts + " " : "", u.nick));
		}
	}
	if (names)
		this.rawout(header + names);
	if (IRC_legacy_leaf(this))
		IRC_send_legacy_flags(this, sni_chan);
	else if (!capable && sni_chan.chanmode(true) != "+")
		this.ircout(format("MODE %s %s%s", sni_chan.nam,
			timestamped_modes ? sni_chan.created + " " : "", sni_chan.chanmode(true)));

	/* Ban lists survive TS resets and are merged from both sides. Explicit
	   TS zero avoids making a ban burst reset the winning channel state. */
	header = ":" + ServerName + " MODE " + sni_chan.nam + (timestamped_modes ? " 0 " : " ");
	var modes = "+", args = "", count = 0;
	for (i in sni_chan.modelist[CHANMODE_BAN]) {
		token = sni_chan.modelist[CHANMODE_BAN][i];
		if (count && (count == MAX_MODES || (header + modes + "b" + args + " " + token).length > 510)) {
			this.rawout(header + modes + args);
			modes = "+"; args = ""; count = 0;
		}
		modes += "b"; args += " " + token; count++;
	}
	if (count)
		this.rawout(header + modes + args);
	if (IRC_has_chanexcept(this)) {
		var entries = [];
		for (i in sni_chan.modelist[CHANMODE_EXCEPT]) entries.push(sni_chan.modelist[CHANMODE_EXCEPT][i]);
		var exception_fields = ["+" + entries.map(function() { return "e"; }).join("")].concat(entries);
		var exception_batches = IRC_chanmode_batches(exception_fields, header, MAX_MODES);
		for (i = 0; i < exception_batches.length; i++) this.rawout(header + exception_batches[i]);
	}
}

function gnotice(str) {
	umode_notice(USERMODE_ROUTING,"Routing","from " + ServerName + ": " + str);
	server_bcast_to_servers("GNOTICE :" + str);
}
