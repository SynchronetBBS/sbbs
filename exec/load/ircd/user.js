/*

 ircd/user.js

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details:
 https://www.gnu.org/licenses/old-licenses/gpl-2.0.txt

 Handling of regular user-to-server communication in the IRCd.

 Copyright 2003-2021 Randy Sommerfeld <cyan@synchro.net>

*/

const USERMODE_NONE			=(1<<0); /* NONE */
const USERMODE_OPER			=(1<<1); /* o */
const USERMODE_INVISIBLE	=(1<<2); /* i */
const USERMODE_WALLOPS		=(1<<3); /* w */
const USERMODE_CHATOPS		=(1<<4); /* b */
const USERMODE_GLOBOPS		=(1<<5); /* g */
const USERMODE_SERVER		=(1<<6); /* s */
const USERMODE_CLIENT		=(1<<7); /* c */
const USERMODE_REJECTED		=(1<<8); /* r */
const USERMODE_KILL			=(1<<9); /* k */
const USERMODE_FLOOD		=(1<<10); /* f */
const USERMODE_STATS_LINKS	=(1<<11); /* y */
const USERMODE_DEBUG		=(1<<12); /* d */
const USERMODE_ROUTING		=(1<<13); /* n */
const USERMODE_HELP			=(1<<14); /* h */
const USERMODE_NOTHROTTLE	=(1<<15); /* F */
const USERMODE_ADMIN		=(1<<16); /* A */

const USERMODE_CHAR = {};
USERMODE_CHAR["o"] = USERMODE_OPER;
USERMODE_CHAR["i"] = USERMODE_INVISIBLE;
USERMODE_CHAR["w"] = USERMODE_WALLOPS;
USERMODE_CHAR["b"] = USERMODE_CHATOPS;
USERMODE_CHAR["g"] = USERMODE_GLOBOPS;
USERMODE_CHAR["s"] = USERMODE_SERVER;
USERMODE_CHAR["c"] = USERMODE_CLIENT;
USERMODE_CHAR["r"] = USERMODE_REJECTED;
USERMODE_CHAR["k"] = USERMODE_KILL;
USERMODE_CHAR["f"] = USERMODE_FLOOD;
USERMODE_CHAR["y"] = USERMODE_STATS_LINKS;
USERMODE_CHAR["d"] = USERMODE_DEBUG;
USERMODE_CHAR["n"] = USERMODE_ROUTING;
USERMODE_CHAR["h"] = USERMODE_HELP;
USERMODE_CHAR["F"] = USERMODE_NOTHROTTLE;
USERMODE_CHAR["A"] = USERMODE_ADMIN;

/* Most umodes aren't propagated across the network. Define the ones that are. */
const USERMODE_BCAST = {};
USERMODE_BCAST["o"] = true;
USERMODE_BCAST["i"] = true;
USERMODE_BCAST["h"] = true;
USERMODE_BCAST["A"] = true;

/* Services modes are broadcast but not displayed to the user. */
const USERMODE_SERVICES = {};

/* Various permissions that can be set on an O:Line */
const OLINE_CAN_REHASH		=(1<<0);	/* r */
const OLINE_CAN_RESTART		=(1<<1);	/* R */
const OLINE_CAN_DIE			=(1<<2);	/* D */
const OLINE_CAN_GLOBOPS		=(1<<3);	/* g */
const OLINE_CAN_WALLOPS		=(1<<4);	/* w */
const OLINE_CAN_LOCOPS		=(1<<5);	/* l */
const OLINE_CAN_LSQUITCON	=(1<<6);	/* c */
const OLINE_CAN_GSQUITCON	=(1<<7);	/* C */
const OLINE_CAN_LKILL		=(1<<8);	/* k */
const OLINE_CAN_GKILL		=(1<<9);	/* K */
const OLINE_CAN_KLINE		=(1<<10);	/* b */
const OLINE_CAN_UNKLINE		=(1<<11);	/* B */
const OLINE_CAN_LGNOTICE	=(1<<12);	/* n */
const OLINE_CAN_GGNOTICE	=(1<<13);	/* N */
const OLINE_IS_ADMIN		=(1<<14);	/* A */
/* Synchronet IRCd doesn't have umode +a	RESERVED */
const OLINE_CAN_UMODEC		=(1<<16);	/* c */
const OLINE_CAN_CHATOPS		=(1<<19);	/* s */
const OLINE_CHECK_SYSPASSWD	=(1<<20);	/* S */
const OLINE_CAN_EVAL		=(1<<21);	/* x */
const OLINE_IS_GOPER		=(1<<22);	/*  "big O" */

/* Bits used for walking the complex WHO flags. */
const WHO_AWAY				=(1<<0);	/* a */
const WHO_CHANNEL			=(1<<1);	/* c */
const WHO_REALNAME			=(1<<2);	/* g */
const WHO_HOST				=(1<<3);	/* h */
const WHO_IP				=(1<<4);	/* i */
const WHO_CLASS				=(1<<5);	/* l */
const WHO_UMODE				=(1<<6);	/* m */
const WHO_NICK				=(1<<7);	/* n */
const WHO_OPER				=(1<<8);	/* o */
const WHO_SERVER			=(1<<9);	/* s */
const WHO_TIME				=(1<<10);	/* t */
const WHO_USER				=(1<<11);	/* u */
const WHO_FIRST_CHANNEL		=(1<<12);	/* C */
const WHO_MEMBER_CHANNEL	=(1<<13);	/* M */
const WHO_SHOW_IPS_ONLY		=(1<<14);	/* I */
const WHO_SHOW_PARENT		=(1<<15);	/* X */
const WHO_SHOW_ID			=(1<<16);	/* Y */
const WHO_SHOW_CALLBACKID	=(1<<17);	/* Z */

/* Bits used for walking complex LIST flags. */
const LIST_CHANMASK				=(1<<0);	/* a */
const LIST_CREATED				=(1<<1);	/* c */
const LIST_MODES				=(1<<2);	/* m */
const LIST_TOPIC				=(1<<3);	/* o */
const LIST_PEOPLE				=(1<<4);	/* p */
const LIST_TOPICAGE				=(1<<5);	/* t */
const LIST_DISPLAY_CHAN_MODES	=(1<<6);	/* M */
const LIST_DISPLAY_CHAN_TS  	=(1<<7);	/* T */

function IRC_User(id) {
	this.local = true;	/* are we a local socket? */
	this.pinged = false;	/* sent PING? */
	this.server = false;	/* No, we're not a server. */
	this.uline = false;	/* Are we services? */
	this.away = "";
	this.channels = {};
	this.connecttime = Epoch();
	this.created = 0;
	this.flags = 0;
	this.hops = 0;
	this.hostname = "";
	this.idletime = system.timer;
	this.invited = "";
	this.ircclass = 0;
	this.mode = 0;
	this.nick = "";
	this.parent = 0;
	this.realname = "";
	this.servername = ServerName;
	this.silence = [];
	this.talkidle = Epoch();
	this.uprefix = "";
	this.id = id;
	this.socket = "";
	/* Functions */
	this.issilenced = User_IsSilenced;
	this.quit = User_Quit;
	this.work = User_Work;
	this.ircout=ircout;
	this.originatorout=originatorout;
	this.rawout=rawout;
	this.sendq = new IRC_Queue(this);
	this.recvq = new IRC_Queue(this);
	this.bcast_to_channel=IRCClient_bcast_to_channel;
	this.bcast_to_channel_servers=IRCClient_bcast_to_channel_servers;
	this.bcast_to_list=IRCClient_bcast_to_list;
	this.bcast_to_servers=IRCClient_bcast_to_servers;
	this.bcast_to_servers_raw=IRCClient_bcast_to_servers_raw;
	this.bcast_to_uchans_unique=IRCClient_bcast_to_uchans_unique;
	this.do_admin=IRCClient_do_admin;
	this.do_connect=IRCClient_do_connect;
	this.do_info=IRCClient_do_info;
	this.do_join=IRCClient_do_join;
	this.do_links=IRCClient_do_links;
	this.do_msg=IRCClient_do_msg;
	this.do_part=IRCClient_do_part;
	this.do_stats=IRCClient_do_stats;
	this.do_summon=IRCClient_do_summon;
	this.do_trace=IRCClient_do_trace;
	this.do_users=IRCClient_do_users;
	this.do_whois=IRCClient_do_whois;
	this.global=IRCClient_global;
	this.globops=IRCClient_globops;
	this.lusers=IRCClient_lusers;
	this.motd=IRCClient_motd;
	this.names=IRCClient_names;
	this.part_all=IRCClient_part_all;
	this.server_notice=IRCClient_server_notice;
	this.services_msg=IRCClient_services_msg;
	this.trace_all_opers=IRCClient_trace_all_opers;
	/* WHO */
	this.do_basic_who=IRCClient_do_basic_who;
	this.do_complex_who=IRCClient_do_complex_who;
	this.do_who_usage=IRCClient_do_who_usage;
	this.match_who_mask=IRCClient_match_who_mask;
	/* LIST */
	this.do_basic_list=IRCClient_do_basic_list;
	this.do_complex_list=IRCClient_do_complex_list;
	this.do_list_usage=IRCClient_do_list_usage;
	/* Global functions */
	this.check_nickname=IRCClient_check_nickname;
	this.check_timeout=IRCClient_check_timeout;
	this.get_usermode=IRCClient_get_usermode;
	this.netsplit=IRCClient_netsplit;
	this.onchanwith=IRCClient_onchanwith;
	this.rmchan=IRCClient_RMChan;
	this.setusermode=IRCClient_setusermode;
	this.set_chanmode=IRCClient_set_chanmode;
	/* Numerics */
	this.numeric=IRCClient_numeric;
	this.numeric200=IRCClient_numeric200;
	this.numeric201=IRCClient_numeric201;
	this.numeric202=IRCClient_numeric202;
	this.numeric203=IRCClient_numeric203;
	this.numeric204=IRCClient_numeric204;
	this.numeric205=IRCClient_numeric205;
	this.numeric206=IRCClient_numeric206;
	this.numeric208=IRCClient_numeric208;
	this.numeric261=IRCClient_numeric261;
	this.numeric321=IRCClient_numeric321;
	this.numeric322=IRCClient_numeric322;
	this.numeric331=IRCClient_numeric331;
	this.numeric332=IRCClient_numeric332;
	this.numeric333=IRCClient_numeric333;
	this.numeric351=IRCClient_numeric351;
	this.numeric352=IRCClient_numeric352;
	this.numeric353=IRCClient_numeric353;
	this.numeric382=IRCClient_numeric382;
	this.numeric391=IRCClient_numeric391;
	this.numeric401=IRCClient_numeric401;
	this.numeric402=IRCClient_numeric402;
	this.numeric403=IRCClient_numeric403;
	this.numeric411=IRCClient_numeric411;
	this.numeric412=IRCClient_numeric412;
	this.numeric440=IRCClient_numeric440;
	this.numeric441=IRCClient_numeric441;
	this.numeric442=IRCClient_numeric442;
	this.numeric445=IRCClient_numeric445;
	this.numeric446=IRCClient_numeric446;
	this.numeric451=IRCClient_numeric451;
	this.numeric461=IRCClient_numeric461;
	this.numeric462=IRCClient_numeric462;
	this.numeric481=IRCClient_numeric481;
	this.numeric482=IRCClient_numeric482;
	/* Getters */
	this.__defineGetter__("nuh", function() {
		return(this.nick + "!" + this.uprefix + "@" + this.hostname);
	});

}

function User_Work(cmdline) {
	var clockticks = system.timer;
	var cmd, p;
	var tmp, i, j, k; /* Temp vars used during command processing */

	this.idletime = system.timer;

	cmdline = cmdline.slice(0,512); /* 512 bytes per RFC1459 */

	log(LOG_DEBUG,format("[%s<-%s]: %s",ServerName,this.nick,cmdline));

	cmd = IRC_parse(cmdline);

	if (!cmd.verb)
		return 0;

	if (cmd.source.nick && cmd.source.nick.toUpperCase() != this.nick.toUpperCase())
		return 0;

	if (cmd.verb.match(/^[0-9]+/))
		return 0;

	p = cmd.params;

	switch (cmd.verb) {
	/* Prioritize PING as per RFC1459 */
	case "PING":
		if (!p[0]) {
			this.numeric(409,":No origin specified.");
			break;
		}
		if (p[1]) {
			tmp = searchbyserver(p[1]);
			if (!tmp) {
				this.numeric402(p[1]);
				break;
			}
			if (tmp != -1) {
				tmp.rawout(format(
					":%s PING %s :%s",
					this.nick,
					this.nick,
					p[1]
				));
				break;
			}
		}
		this.ircout(format("PONG %s :%s",
			ServerName,
			p[0]
		));
		break;
	case "PRIVMSG":
		if (!p[0]) {
			this.numeric411("PRIVMSG");
			break;
		}
		if (!p[1]) {
			this.numeric412();
			break;
		}
		tmp = p[0].split(',');
		for (i in tmp) {
			this.do_msg(tmp[i],"PRIVMSG",p[1]);
		}
		this.talkidle = Epoch();
		break;
	case "MODE":
		if (!p[0])
			break;
		if (!p[1]) { /* User requesting info only */
			if ((p[0][0] == "#") || (p[0][0] == "&")) {
				tmp = Channels[p[0].toUpperCase()];
				if (!tmp) {
					this.numeric401(p[0]);
					break;
				}
				this.numeric(324, format("%s %s",
					tmp.nam,
					tmp.chanmode(this.channels[p[0].toUpperCase()] ? true : false)
				));
				this.numeric(329, format("%s %lu",
					tmp.nam,
					tmp.created
				));
				break;
			} else { /* Requesting a usermode */
				if (p[0].toUpperCase() == this.nick.toUpperCase())
					this.numeric(221, this.get_usermode());
				else if (Users[p[0].toUpperCase()])
					this.numeric(502, ":Can't view mode for other users.");
				else
					this.numeric401(p[0]);
				break;
			}
		} else { /* Mode flags were passed */
			if ((p[0][0] == "#") || (p[0][0] == "&")) {
				tmp = Channels[p[0].toUpperCase()];
				if (!tmp) {
					this.numeric403(p[0]);
					break;
				}
				p.shift();
				this.set_chanmode(tmp, p, false /* bounce? */);
			} else if (p[0].toUpperCase() == this.nick.toUpperCase()) {
				this.setusermode(p[1]);
			} else {
				this.numeric(502, ":Can't change mode for other users.");
			}
		}
		break;
	case "AWAY":
		if (p[0] && p[0] != "") {
			this.away = p[0].slice(0, MAX_AWAYLEN);
			this.numeric(306, ":You have been marked as being away.");
			this.bcast_to_servers("AWAY :" + this.away);
		} else {
			this.away="";
			this.numeric(305, ":You are no longer marked as being away.");
			this.bcast_to_servers("AWAY");
		}
		break;
	case "JOIN":
		if (!p[0]) {
			this.numeric461("JOIN");
			break;
		}
		k = []; /* channel keys */
		if (p[1])
			k = p[1].split(",");

		j = p[0].split(",");
		for (i in j) {
			if (j[i] == "0") {
				this.part_all();
				break;
			}
			if (   Channels[j[i].toUpperCase()]
				&& k[0]
				&& (Channels[j[i].toUpperCase()].mode&CHANMODE_KEY)
			) {
				this.do_join(j[i], k[0]);
				k.shift();
			} else {
				this.do_join(j[i].slice(0, MAX_CHANLEN));
			}
		}
		break;
	case "PART":
		if (!p[0]) {
			this.numeric461("PART");
			break;
		}
		tmp = p[0].split(",");
		for (i in tmp) {
			this.do_part(tmp[i]);
		}
		break;
	case "KICK":
		if (!p[1]) {
			this.numeric461("KICK");
			break;
		}
		tmp = Channels[p[0].toUpperCase()];
		if (!tmp) {
			this.numeric403(p[0]);
			break;
		}
		if (!tmp.modelist[CHANMODE_OP][this.id]) {
			this.numeric482(tmp.nam);
			break;
		}
		var j = Users[p[1].toUpperCase()]; /* target */
		if (!j) {
			j = search_nickbuf(p[1]);
			if (!j) {
				this.numeric401(p[1]);
				break;
			}
		}
		if (!j.channels[tmp.nam.toUpperCase()]) {
			this.numeric(441, format(
				"%s %s :They aren't on that channel!",
				j.nick,
				tmp.nam
			));
			break;
		}

		i = format("KICK %s %s :%s",
			tmp.nam,
			j.nick,
			p[2] ? p[2] : j.nick
		);

		this.bcast_to_channel(tmp, i, true);
		this.bcast_to_servers(i);
		j.rmchan(tmp);
		break;
	case "TOPIC":
		if (!p[0]) {
			this.numeric461("TOPIC");
			break;
		}
		var tmp = Channels[p[0].toUpperCase()];
		if (!tmp) {
			this.numeric403(p[0]);
			break;
		}
		if (!this.channels[tmp.nam.toUpperCase()]) {
			this.numeric442(tmp.nam);
			break;
		}
		if (p[1]) { /* Setting a topic */
			if (!(tmp.mode&CHANMODE_TOPIC) || tmp.modelist[CHANMODE_OP][this.id]) {
				j = p[1].slice(0, MAX_TOPICLEN);
				if (j == tmp.topic)
					break;
				tmp.topic = j;
				tmp.topictime = Epoch();
				tmp.topicchangedby = this.nick;
				this.bcast_to_channel(
					tmp,
					format("TOPIC %s :%s", tmp.nam, tmp.topic),
					true /* bounceback */
				);
				this.bcast_to_servers(format(
					"TOPIC %s %s %s :%s",
					tmp.nam,
					this.nick,
					tmp.topictime,
					tmp.topic
				));
			} else {
				this.numeric482(tmp.nam);
			}
		} else { /* Looking at a topic */
			if (tmp.topic) {
				this.numeric332(tmp);
				this.numeric333(tmp);
			} else {
				this.numeric331(tmp);
			}
		}
		break;
	case "WHOIS":
		if (!p[0]) {
			this.numeric(431, ":No nickname given.");
			break;
		}
		if (p[1]) { /* Asking remote server */
			tmp = searchbyserver(p[0]);
			if (!tmp) {
				this.numeric402(p[0]);
				break;
			}
			if (tmp != -1) {
				tmp.rawout(format(
					":%s WHOIS %s :%s",
					this.nick,
					tmp.nick,
					p[0]
				));
				break;
			}
		}
		tmp = p[0].split(",");
		for (i in tmp) {
			if (Users[tmp[i].toUpperCase()])
				this.do_whois(Users[tmp[i].toUpperCase()]);
			else
				this.numeric401(tmp[i]);
		}
		this.numeric(318, tmp[0]+" :End of /WHOIS list.");
		break;
	case "ADMIN":
		if (p[0]) {
			tmp = searchbyserver(p[0]);
			if (!tmp) {
				this.numeric402(p[0]);
				break;
			}
			if (tmp != -1) {
				tmp.rawout(format(
					":%s ADMIN :%s",
					this.nick,
					tmp.nick
				));
				break;
			}
		}
		this.do_admin();
		break;
	case "CHATOPS":
		if (!((this.mode&USERMODE_OPER) && (this.flags&OLINE_CAN_CHATOPS))) {
			this.numeric481();
			break;
		}
		if (!p[0])
			break;
		umode_notice(USERMODE_CHATOPS,"ChatOps",format(
			"from %s: %s",
			this.nick,
			p[0]
		));
		server_bcast_to_servers(format(
			":%s CHATOPS :%s",
			this.nick,
			p[0]
		));
		break;
	case "CONNECT":
		if (!((this.mode&USERMODE_OPER) && (this.flags&OLINE_CAN_LSQUITCON) )) {
			this.numeric481();
			break;
		}
		if (!p[0]) {
			this.numeric461("CONNECT");
			break;
		}
		if (p[2]) { /* Asking remote server to CONNECT */
			tmp = searchbyserver(p[2]);
			if (!tmp) {
				this.numeric402(p[2]);
				break;
			}
			if (tmp != -1) {
				if (!(this.flags&OLINE_CAN_GSQUITCON)) {
					this.numeric481();
					break;
				}
				tmp.rawout(format(
					":%s CONNECT %s %s %s",
					this.nick,
					p[0],
					p[1],
					tmp.nick
				));
				break;
			}
		}
		this.do_connect(p[0],p[1]);
		break;
	case "EVAL":
		if (!(this.mode&USERMODE_OPER) || !(this.flags&OLINE_CAN_EVAL)) {
			this.numeric481();
			break;
		}
		if (!p[0]) {
			this.server_notice("No expression provided to evaluate.");
			break;
		}
		tmp = p.join(" ");
		umode_notice(USERMODE_DEBUG,"Debug",format(
			"Oper %s is using EVAL: %s",
			this.nick,
			tmp
		));
		try {
				this.server_notice("Result: " + eval(tmp));
			} catch(e) {
				this.server_notice("!" + e);
		}
		break;
	case "DIE":
		if (!((this.mode&USERMODE_OPER) && (this.flags&OLINE_CAN_DIE))) {
			this.numeric481();
			break;
		}
		if (Die_Password && !p[0]) {
			this.numeric461("DIE");
			break;
		} else if (Die_Password && (p[0] != Die_Password)) {
			this.server_notice("Invalid DIE password.");
			break;
		}
		log(LOG_ERR,"!ERROR! Shutting down the ircd as per " + this.nuh);
		js.do_callbacks = false;
		break;
	case "ERROR":
		break; /* ERROR is silently ignored for users */
	case "GLOBOPS":
		if (!((this.mode&USERMODE_OPER) && (this.flags&OLINE_CAN_GLOBOPS))) {
			this.numeric481();
			break;
		}
		if (!p[0]) {
			this.numeric461("GLOBOPS");
			break;
		}
		this.globops(p[0]);
		break;
	case "INFO":
		if (p[0]) {
			tmp = searchbyserver(p[0]);
			if (!tmp) {
				this.numeric402(cmd[1]);
				break;
			}
			if (tmp != -1) {
				tmp.rawout(format(
					":%s INFO :%s",
					this.nick,
					tmp.nick
				));
				break;
			}
		}
		this.do_info();
		break;
	case "INVITE":
		if (!p[1]) {
			this.numeric461("INVITE");
			break;
		}
		var tmp = Channels[p[1].toUpperCase()];
		if (!tmp) {
			this.numeric403(p[1]);
			break;
		}
		if (!tmp.modelist[CHANMODE_OP][this.id]) {
			this.numeric482(tmp.nam);
			break;
		}
		j = Users[p[0].toUpperCase()];
		if (!j) {
			this.numeric401(p[0]);
			break;
		}
		if (j.channels[p[1].toUpperCase()]) {
			this.numeric(443, format(
				"%s %s :is already on channel.",
				j.nick,
				tmp.nam
			));
			break;
		}
		this.numeric("341", j.nick + " " + tmp.nam);
		j.originatorout("INVITE " + j.nick + " :" + tmp.nam,this);
		j.invited = tmp.nam.toUpperCase();
		break;
	case "ISON":
		if (!p[0])
			break; /* Drop silently */
		tmp = "";
		for (i in p) {
			j = Users[p[i].toUpperCase()];
			if (j) {
				if (!tmp.length)
					tmp = ":";
				else
					tmp += " ";
				tmp += j.nick;
			}
		}
		this.numeric("303", tmp);
		break;
	case "KILL":
		if (!(this.mode&USERMODE_OPER) || !(this.flags&OLINE_CAN_LKILL)) {
			this.numeric481();
			break;
		}
		if (!p[0]) {
			this.numeric461("KILL");
			break;
		}
		if (p[0].match(/[.]/)) {
			this.numeric(483, ":You can't kill a server!");
			break;
		}
		if (!p[1]) {
			this.numeric(461, "KILL :You MUST specify a reason for /KILL.");
			break;
		}
		tmp = p[0].split(",");
		for (i in tmp) {
			k = Users[tmp[i].toUpperCase()];
			if (!k)
				k = search_nickbuf(tmp[i]);
			if (!k) {
				this.numeric401(tmp[i]);
				continue;
			}
			if (k.local) {
				k.quit(format(
					"Local kill by %s (%s)",
					this.nick,
					p[1]
				));
			} else {
				if (!(this.flags&OLINE_CAN_GKILL)) {
					this.numeric481();
					continue;
				}
				j = searchbyserver(k.servername);
				if (j && j.uline) {
					this.numeric(483,
						":You may not KILL clients on a U:Lined server.");
					continue;
				}
				server_bcast_to_servers(format(
					":%s KILL %s :%s",
					this.nick,
					k.nick,
					p[1]
				));
				k.quit(format(
					"Killed (%s (%s))",
					this.nick,
					p[1]
				), true /* Suppress broadcast */ );
			}
		}
		break;
	case "KLINE":
		if (!((this.mode&USERMODE_OPER) &&
		      (this.flags&OLINE_CAN_KLINE))) {
			this.numeric481();
			break;
		}
		if (!p[1]) {
			this.numeric461("KLINE");
			break;
		}
		k = create_ban_mask(p[0],true);
		if (!k) {
			this.server_notice("Invalid K:Line mask.");
			break;
		}
		if (isklined(k)) {
			this.server_notice("K:Line already exists!");
			break;
		}
		KLines.push(new KLine(k,p[1],"k"));
		umode_notice(USERMODE_OPER,"Notice",format(
			"%s added k-line for [%s]",
			this.nick,
			k
		));
		Scan_For_Banned_Clients();
		break;
	case "UNKLINE":
		if (!((this.mode&USERMODE_OPER) &&
		      (this.flags&OLINE_CAN_UNKLINE))) {
			this.numeric481();
			break;
		}
		if (!p[0]) {
			this.numeric461("UNKLINE");
			break;
		}
		k = create_ban_mask(p[0],true);
		if (!k) {
			this.server_notice("Invalid K:Line mask.");
			break;
		}
		if (!isklined(k)) {
			this.server_notice("No such K:Line.");
			break;
		}
		remove_kline(k);
		umode_notice(USERMODE_OPER,"Notice",format(
			"%s has removed the K-Line for: [%s]",
			this.nick,
			k
		));
		break;
	case "LINKS":
		if (!p[0]) {
			this.do_links();
			break;
		} else if (p[1]) {
			tmp = searchbyserver(p[0]);
			if (!tmp) {
				this.numeric402(p[0]);
				break;
			}
			if (tmp != -1) {
				tmp.rawout(format(
					":%s LINKS %s %s",
					this.nick,
					tmp.nick,
					p[1]
				));
				break;
			}
		}
		this.do_links(p[0]);
		break;
	case "LIST":
		if (!p[0] || p[0][0].toUpperCase() == "A") {
			this.do_basic_list("*");
			break;
		}
		if (p[0][0] == "?" || p[0][0].toUpperCase() == "H") {
			this.do_list_usage();
			break;
		}
		if (p[0][0] != "+" && p[0][0] != "-") {
			this.do_basic_list(p[0]);
			break;
		}
		this.do_complex_list(p);
		break;
	case "SILENCE":
		if (!p[0]) { /* Just display our SILENCE list */
			for (i in this.silence) {
				this.numeric(271, format(
					"%s %s",
					this.nick,
					this.silence[i]
				));
			}
		} else if (Users[p[0].toUpperCase()]) {
			if (!this.mode&USERMODE_OPER) {
				this.numeric481();
				break;
			}
			for (i in Users[p[0].toUpperCase()].silence) {
				this.numeric(271, format(
					"%s %s",
					Users[p[0].toUpperCase()].nick,
					Users[p[0].toUpperCase()].silence[i]
				));
			}
		} else { /* Adding or removing an entry to our SILENCE list */
			if (p[0][0] == "-") {
				p[0] = p[0].slice(1);
				tmp = create_ban_mask(p[0]);
				j = this.issilenced(tmp);
				if (j) {
					delete this.silence[j];
					this.originatorout(format(
						"SILENCE -%s",
						tmp
					));
				}
			} else {
				if (p[0][0] == "+")
					p[0] = p[0].slice(1);
				tmp = create_ban_mask(p[0]);
				if (this.issilenced(tmp))
					break; /* Exit silently if already on SILENCE list */
				if (this.silence.length >= MAX_SILENCE) {
					this.numeric(511, format(
						"%s %s :Your SILENCE list is full.",
						this.nick,
						p[0]
					));
					break;
				}
				this.silence.push(tmp);
			}
			break;
		}
		this.numeric(272, ":End of SILENCE List.");
		break;
	case "LOCOPS":
		if (!((this.mode&USERMODE_OPER) && (this.flags&OLINE_CAN_LOCOPS))) {
			this.numeric481();
			break;
		}
		umode_notice(USERMODE_OPER,"LocOps",format(
			"from %s :%s",
			this.nick,
			p[0]
		));
		break;
	case "LUSERS":
		this.lusers();
		break;
	case "MOTD":
		if (p[0]) {
			tmp = searchbyserver(p[0]);
			if (!tmp) {
				this.numeric402(p[0]);
				break;
			}
			if (tmp != -1) {
				tmp.rawout(format(
					":%s MOTD :%s",
					this.nick,
					tmp.nick
				));
				break;
			}
		}
		umode_notice(USERMODE_STATS_LINKS,"StatsLinks",format(
			"MOTD requested by %s (%s@%s) [%s]",
			this.nick,
			this.uprefix,
			this.hostname,
			this.servername
		));
		this.motd();
		break;
	case "NAMES":
		if (!p[0]) {
			for (i in Channels) {
				if (this.channels[Channels[i].nam.toUpperCase()]
					|| (   !(Channels[i].mode&CHANMODE_SECRET)
						&& !(Channels[i].mode&CHANMODE_PRIVATE)
						)
				) {
					this.names(Channels[i]);
				}
			}
			tmp = "";
			j = 0;
			for (i in Users) {
				if (   !true_array_len(Users[i].channels)
				    && !(Users[i].mode&USERMODE_INVISIBLE)
				) {
					if (tmp)
						tmp += " ";
					tmp += Users[i].nick;
					j++;
					if (j >= 59) {
						this.numeric(353,"* * :"+tmp);
						j = 0;
						tmp = "";
					}
				}
			}
			if (j)
				this.numeric(353,"* * :"+tmp);
			this.numeric(366, "* :End of /NAMES list.");
		} else {
			tmp = p[0].split(",");
			for (i in tmp) {
				if (   (tmp[i][0] == "#")
					|| (tmp[i][0] == "&")
				) {
					if (Channels[tmp[i].toUpperCase()])
						this.names(Channels[tmp[i].toUpperCase()]);
					else
						continue;
				} else {
					continue;
				}
			}
			this.numeric(366, tmp[i] + " :End of /NAMES list.");
		}
		break;
	case "NICK":
		if (!p[0]) {
			this.numeric(431, ":No nickname given.");
			break;
		}
		p[0] = p[0].slice(0,MAX_NICKLEN);
		if(this.check_nickname(p[0]) > 0) {
			this.bcast_to_uchans_unique("NICK " + p[0]);
			this.originatorout("NICK " + p[0],this);
			if (p[0].toUpperCase() != this.nick.toUpperCase()) {
				this.created = Epoch();
				push_nickbuf(this.nick,p[0]);
				Users[p[0].toUpperCase()] = this;
				delete Users[this.nick.toUpperCase()];
			}
			this.bcast_to_servers(format(
				"NICK %s :%lu",
				p[0],
				this.created
			));
			this.nick = p[0];
		}
		break;
	case "NOTICE":
		if (!p[0]) {
			this.numeric411("NOTICE");
			break;
		}
		if (!p[1]) {
			this.numeric412();
			break;
		}
		tmp = p[0].split(",");
		for (i in tmp) {
			this.do_msg(tmp[i],"NOTICE",p[1]);
		}
		break;
	case "OPER":
		if (!p[1]) {
			this.numeric461("OPER");
			break;
		}
		if (this.mode&USERMODE_OPER) {
			this.server_notice("You're already an IRC operator.");
			break;
		}
		for (i in OLines) {
			if (  (p[0].toUpperCase() == OLines[i].nick.toUpperCase())
				&& (wildmatch(this.uprefix + "@" + this.hostname,OLines[i].hostmask))
				&& (
						(  (p[1] == OLines[i].password)
							&& !(OLines[i].flags&OLINE_CHECK_SYSPASSWD)
						)
					|| (
						(OLines[i].flags&OLINE_CHECK_SYSPASSWD)
						&& system.check_syspass(p[1])
					)
				)
			) {
				this.ircclass = OLines[i].ircclass;
				this.flags = OLines[i].flags;
				this.numeric(381, ":You are now an IRC operator.");
				this.mode |= USERMODE_OPER;
				this.rawout(format(
					":%s MODE %s +o",
					this.nick,
					this.nick
				));
				umode_notice(USERMODE_SERVER,"Notice",format(
					"%s (%s@%s) [%s] is now operator (O)",
					this.nick,
					this.uprefix,
					this.hostname,
					this.ip
				));
				if (OLines[i].flags&OLINE_IS_GOPER)
					this.bcast_to_servers("MODE "+ this.nick +" +o");
				break;
			}
		}
		if (this.mode&USERMODE_OPER)
			break;
		this.numeric(491, ":No O:Lines for your host.  Attempt logged.");
		umode_notice(USERMODE_OPER,"Notice",format(
			"Failed OPER attempt by %s (%s@%s) [%s]",
			this.nick,
			this.uprefix,
			this.hostname,
			this.ip
		));
		break;
	case "PASS":
	case "USER":
		this.numeric462();
		break;
	case "PONG":
		if (p[1]) {
			tmp = searchbyserver(p[1]);
			if (!tmp) {
				this.numeric402(p[1]);
				break;
			}
			if (tmp != -1) {
				tmp.rawout(format(
					":%s PONG %s %s",
					this.nick,
					p[0],
					tmp.nick
				));
				break;
			}
		}
		this.pinged = false;
		break;
	case "QUIT":
		this.quit(p[0]);
		break;
	case "REHASH":
		if (!((this.mode&USERMODE_OPER) && (this.flags&OLINE_CAN_REHASH))) {
			this.numeric481();
			break;
		}
		if (p[0]) {
			switch(p[0].toUpperCase()) {
			case "TKLINES":
				this.numeric382("temp klines");
				umode_notice(USERMODE_SERVER,"Notice",format(
					"%s is clearing temp klines while whistling innocently",
					this.nick
				));
				for (i in KLines) {
					if(KLines[i].type == "k") {
						delete KLines[i];
					}
				}
				break;
			case "GC":
				if (js.gc!=undefined) {
					this.numeric382("garbage collecting");
					umode_notice(USERMODE_SERVER,"Notice",format(
						"%s is garbage collecting while whistling innocently",
						this.nick
					));
					js.gc();
				}
				break;
			case "AKILLS":
				this.numeric382("akills");
				umode_notice(USERMODE_SERVER,"Notice",format(
					"%s is rehashing akills while whistling innocently",
					this.nick
				));
				for (i in KLines) {
					if(KLines[i].type == "A")
						delete KLines[i];
				}
				break;
			default:
				break;
			}
		} else {
			this.numeric382("Rehashing.");
			umode_notice(USERMODE_SERVER,"Notice",format(
				"%s is rehashing Server config file while whistling innocently",
				this.nick
			));
			Read_Config_File();
		}
		break;
	case "RESTART":
		if (!((this.mode&USERMODE_OPER) && (this.flags&OLINE_CAN_RESTART))) {
			this.numeric481();
			break;
		}
		if (Restart_Password && !p[0]) {
			this.numeric461("RESTART");
			break;
		} else if (Restart_Password && (p[0] != Restart_Password)) {
			this.server_notice("Invalid RESTART password.");
			break;
		}
		umode_notice(USERMODE_SERVER,"Notice",format(
			"Aieeeee!!!  Restarting server..."
		));
		terminate_everything("Aieeeee!!!  Restarting server...");
		break;
	case "SQUIT":
		if (!((this.mode&USERMODE_OPER) && (this.flags&OLINE_CAN_LSQUITCON))) {
			this.numeric481();
			break;
		}
		if(!p[0])
			break;
		tmp = searchbyserver(p[0]);
		if(!tmp) {
			this.numeric402(p[0]);
			break;
		}
		if (!p[1])
			p[1] = this.nick;
		if (tmp == -1) {
			this.quit(p[1]);
			break;
		}
		if (!tmp.local) {
			if (!(this.flags&OLINE_CAN_GSQUITCON)) {
				this.numeric481();
				break;
			}
			tmp.rawout(format(
				":%s SQUIT %s :%s",
				this.nick,
				tmp.nick,
				p[1]
			));
			break;
		}
		umode_notice(USERMODE_ROUTING,"Routing",format(
			"from %s: Received SQUIT %s from %s [%s@%s] (%s)",
			ServerName,
			p[0],
			this.nick,
			this.uprefix,
			this.hostname,
			p[1]
		));
		tmp.quit(p[1]);
		break;
	case "STATS":
		if(!p[0])
			break;
		if (p[1]) {
			tmp = searchbyserver(p[1]);
			if (!tmp) {
				this.numeric402(p[1]);
				break;
			}
			if (tmp != -1) {
				tmp.rawout(format(
					":%s STATS %s :%s",
					this.nick,
					p[0][0],
					tmp.nick
				));
				break;
			}
		}
		this.do_stats(p[0][0]);
		break;
	case "SUMMON":
		if(!p[0]) {
			this.numeric411("SUMMON");
			break;
		}
		if (p[1]) {
			tmp = searchbyserver(p[0]);
			if (!tmp) {
				this.numeric402(p[1]);
				break;
			}
			if (tmp != -1) {
				tmp.rawout(format(
					":%s SUMMON %s :%s",
					this.nick,
					p[0],
					tmp.nick
				));
				break;
			}
		}
		if (!SUMMON) {
			this.numeric445();
			break;
		}
		this.do_summon(p[0]);
		break;
	case "TIME":
		if (p[0]) {
			tmp = searchbyserver(p[0]);
			if (!tmp) {
				this.numeric402(p[0]);
				break;
			}
			if (tmp != -1) {
				tmp.rawout(format(
					":%s TIME :%s",
					this.nick,
					tmp.nick
				));
				break;
			}
		}
		this.numeric391();
		break;
	case "TRACE":
		this.do_trace(p[0] ? p[0] : ServerName);
		break;
	case "USERS":
		if (p[0]) {
			tmp = searchbyserver(p[0]);
			if (!tmp) {
				this.numeric402(p[0]);
				break;
			}
			if (tmp != -1) {
				tmp.rawout(format(
					":%s USERS :%s",
					this.nick,
					tmp.nick
				));
				break;
			}
		}
		if (!SUMMON) {
			this.numeric446();
			break;
		}
		this.do_users();
		break;
	case "USERHOST":
		if (!p[0]) {
			this.numeric461("USERHOST");
			break;
		}

		j = p.length;
		if (j > MAX_USERHOST)
			j = MAX_USERHOST;

		k = "";
		for (i = 0; i < j; i++) {
			tmp = Users[p[i].toUpperCase()];
			if (tmp) {
				if (k)
					k += " ";
				k += tmp.nick;
				if (tmp.mode&USERMODE_OPER)
					k += "*";
				k += "=";
				if (tmp.away)
					k += "-";
				else
					k += "+";
				k += tmp.uprefix;
				k += "@";
				k += tmp.hostname;
			}
		}
		this.numeric(302, ":" + k);
		break;
	case "VERSION":
		if (p[0]) {
			tmp = searchbyserver(p[0]);
			if (!tmp) {
				this.numeric402(p[0]);
				break;
			}
			if (tmp != -1) {
				tmp.rawout(format(
					":%s VERSION :%s",
					this.nick,
					tmp.nick
				));
				break;
			}
		}
		this.numeric351();
		break;
	case "WALLOPS":
		if (!((this.mode&USERMODE_OPER) && (this.flags&OLINE_CAN_WALLOPS))) {
			this.numeric481();
			break;
		}
		if (!p[0]) {
			this.numeric461("WALLOPS");
			break;
		}
		Write_All_Opers(format(
			":%s WALLOPS :%s",
			this.nuh,
			p[0]
		));
		server_bcast_to_servers(format(
			":%s WALLOPS :%s",
			this.nick,
			p[0]
		));
		break;
	case "WHO":
		if (!p[0] || p[0] == "?" || p[0] == "HELP") {
			this.do_who_usage();
			break;
		}
		if (p[1] || p[0][0] == "-" || p[0][0] == "+") {
			p.unshift(""); /* FIXME: Hack to force p[0]=cmd[1] in do_complex_who() */
			this.do_complex_who(p);
			break;
		}
		this.do_basic_who(p[0]);
		break;
	case "WHOWAS":
		if (!p[0]) {
			this.numeric(431, ":No nickname given.");
			break;
		}
		tmp = WhoWas[p[0].toUpperCase()];
		j = "";
		if (tmp) {
			for (i in tmp) {
				this.numeric(314,format(
					"%s %s %s * :%s",
					tmp[i].nick,
					tmp[i].uprefix,
					tmp[i].host,
					tmp[i].realname
				));
				this.numeric(312,format(
					"%s %s :%s",
					tmp[i].nick,
					tmp[i].server,
					tmp[i].serverdesc
				));
				if (!j)
					j = tmp[i].nick;
			}
		}
		if (!j) {
			this.numeric(406,p[0] + " :There was no such nickname.");
			j = p[0];
		}
		this.numeric(369,j+" :End of /WHOWAS command.");
		break;
	case "CS":
	case "CHANSERV":
		if (!p[0]) {
			this.numeric412();
			break;
		}
		if (p[1])
			p[0] = p.join(" ");
		this.services_msg("ChanServ",p[0]);
		break;
	case "NS":
	case "NICKSERV":
		if (!p[0]) {
			this.numeric412();
			break;
		}
		if (p[1])
			p[0] = p.join(" ");
		this.services_msg("NickServ",p[0]);
		break;
	case "MS":
	case "MEMOSERV":
		if (!p[0]) {
			this.numeric412();
			break;
		}
		if (p[1])
			p[0] = p.join(" ");
		this.services_msg("MemoServ",p[0]);
		break;
	case "OS":
	case "OPERSERV":
		if (!p[0]) {
			this.numeric412();
			break;
		}
		if (p[1])
			p[0] = p.join(" ");
		this.services_msg("OperServ",p[0]);
		break;
	case "HELP":
	case "HS":
	case "HELPSERV":
		if (!p[0]) {
			p[0] = "HELP";
		}
		if (p[1])
			p[0] = p.join(" ");
		this.services_msg("HelpServ",p[0]);
		break;
	case "IDENTIFY":
		if (!p[0]) {
			this.numeric412();
			break;
		}
		if (p[1])
			p[0] = p.join(" ");
		this.services_msg(p[0][0] == "#" ? "ChanServ" : "NickServ","IDENTIFY " + p[0]);
		break;
	default:
		this.numeric("421", cmd.verb + " :Unknown command.");
		return 0;
	}

	/* This part only executed if the command was legal. */

	if (!Profile[cmd.verb])
		Profile[cmd.verb] = new StatsM();
	Profile[cmd.verb].executions++;
	Profile[cmd.verb].ticks += system.timer - clockticks;

}

function User_Quit(str,suppress_bcast,is_netsplit,origin) {
	var i;

	if (!str)
		str = this.nick;

	var ww_serverdesc = ServerDesc;

	this.bcast_to_uchans_unique(format("QUIT :%s", str));
	for (i in this.channels) {
		this.rmchan(this.channels[i]);
	}

	if (this.parent)
		ww_serverdesc = Servers[this.servername.toLowerCase()].info;

	var nick_uc = this.nick.toUpperCase();
	if (!WhoWas[nick_uc])
		WhoWas[nick_uc] = new Array;
	var ww = WhoWas[nick_uc];

	var ptr = ww.unshift(new WhoWasObj(
		this.nick,
		this.uprefix,
		this.hostname,
		this.realname,
		this.servername,
		ww_serverdesc
	)) - 1;
	WhoWasMap.unshift(ww[ptr]);

	if (WhoWasMap.length > MAX_WHOWAS) {
		var ww_pop = WhoWasMap.pop();
		var ww_obj = WhoWas[ww_pop.nick.toUpperCase()];
		ww_obj.pop();
		if (!ww_obj.length)
			delete WhoWas[ww_pop.nick.toUpperCase()];
	}

	if (!suppress_bcast)
		this.bcast_to_servers(format("QUIT :%s", str));

	if (this.local) {
		if (this.socket.is_connected) {
			this.socket.send(format("ERROR :Closing Link: [%s@%s] (%s)\r\n",
				this.uprefix,
				this.hostname,
				str
			));
		}
		umode_notice(USERMODE_CLIENT,"Client",format(
			"Client exiting: %s (%s@%s) [%s] [%s]",
			this.nick,
			this.uprefix,
			this.hostname,
			str,
			this.ip
		));
		log(LOG_NOTICE,format(
			"IRC user quitting: %s (%s@%s)",
			this.nick,
			this.uprefix,
			this.hostname
		));
		if (server.client_remove !== undefined)
			server.client_remove(this.socket);
		if (this.socket !== undefined) {
			this.socket.clearOn("read", this.socket.callback_id);
			this.socket.close();
		}
		js.clearInterval(this.pinginterval);
	}

	delete Assigned_IDs[this.id];
	delete Local_Users[this.id];
	delete Users[this.nick.toUpperCase()];
}

function User_IsSilenced(sil_mask) {
	var i;

	for (i in this.silence) {
		if (wildmatch(sil_mask,this.silence[i]))
			return i;
	}
	return false; /* Not Silenced. */
}
