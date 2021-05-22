/*

 ircd/unregistered.js

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details:
 https://www.gnu.org/licenses/old-licenses/gpl-2.0.txt

 How unregistered clients are handled in the IRCd.

 Copyright 2003-2021 Randy Sommerfeld <cyan@synchro.net>

*/

////////// Objects //////////
function Unregistered_Client(id,socket) {
	////////// VARIABLES
	// Bools/Flags that change depending on connection state.
	this.pinged = false;
	this.local = true;			// FIXME: this is redundant.
	// Variables containing user/server information as we receive it.
	this.id = id;
	this.nick = "*";
	this.realname = "";
	this.uprefix = "";
	this.password = "";
	this.ircclass = 0;
	this.idletime = system.timer;
	this.ip = socket.remote_ip_address;
	/* Render IPv4 in IPv6 addresses as IPv4 */
	if (this.ip.toUpperCase().slice(0,7) == "::FFFF:") {
		this.ip = this.ip.slice(7);
	}
	this.hostname = this.ip;
	this.pending_resolve = false;
	this.dns_pending = time();
	// Variables (consts, really) that point to various state information
	this.socket = socket;
	this.socket.irc = this;
	////////// FUNCTIONS
	this.work = Unregistered_Commands;
	this.quit = Unregistered_Quit;
	this.check_timeout = IRCClient_check_timeout;
	this.welcome = Unregistered_Welcome;
	this.QWK_Master_Authentication = QWK_Master_Authentication;
	this.rawout = rawout;
	this.originatorout = originatorout;
	this.ircout = ircout;
	this.sendq = new IRC_Queue(this);
	this.recvq = new IRC_Queue(this);
	this.server_notice = IRCClient_server_notice;
	this.check_nickname = IRCClient_check_nickname;
	this.Unregistered_Check_User_Registration = Unregistered_Check_User_Registration;
	this.numeric = IRCClient_numeric;
	this.numeric451 = IRCClient_numeric451;
	this.numeric461 = IRCClient_numeric461;
	this.numeric462 = IRCClient_numeric462;
	this.pinginterval = js.setInterval(
		IRCClient_check_timeout,
		YLines[0].pingfreq * 1000,
		this
	);
	if (this.socket.outbound) {
		this.dns_pending = false;
		return 0;
	}
	log(LOG_NOTICE, format("%04u Accepted new connection: %s port %s",
		socket.descriptor,
		this.ip,
		socket.remote_port
	));
	if (   (this.ip.slice(0,4) == "127.")
		|| (this.ip.slice(0,3) == "10.")
		|| (this.ip.slice(0,8) == "192.168.")
		|| (this.ip.slice(0,7) == "100.64.")
		|| (this.ip.slice(0,7) == "172.16." )
		|| (this.ip.slice(0,2) == "::")
		|| (this.ip.slice(0,6).toUpperCase() == "FE80::")
	) {
		this.hostname = ServerName;
		this.dns_pending = false;
	} else {
		this.reverse_resolver = function(resp) {
			if (!this.dns_pending) {
				log(LOG_DEBUG,format("[UNREG] WARNING: Received extraneous RDNS reply."));
				return false;
			}
			log(LOG_DEBUG,format("[UNREG] Received RDNS reply: %s", resp[0]));
			if (resp[0] === undefined) {
				/* Fall through */
			} else if (resp[0].search(/[.]/) == -1 || resp[0].search(/[.]local$/i) > -1) {
				this.hostname = ServerName;
			} else {
				this.hostname = resp[0];
				log(LOG_DEBUG,format("[UNREG] Resolving hostname: %s", resp[0]));
				DNS_Resolver.resolve(resp[0], this.forward_resolver, this);
				return true;
			}
			this.dns_pending = false;
			this.Unregistered_Check_User_Registration();
			return false;
		}
		this.forward_resolver = function(resp) {
			if (!this.dns_pending) {
				log(LOG_DEBUG,format("[UNREG] WARNING: Received extraneous DNS reply."));
				return false;
			}
			log(LOG_DEBUG,format("[UNREG] Received DNS reply: %s", resp[0]));
			if ((resp[0] === undefined) || (resp[0] != this.ip)) {
				this.hostname = this.ip;
			}
			this.dns_pending = false;
			this.Unregistered_Check_User_Registration();
			return true;
		}
		log(LOG_DEBUG,format("[UNREG] Resolving IP: %s", this.ip));
		DNS_Resolver.reverse(this.ip, this.reverse_resolver, this);
	}
	this.server_notice(format("*** %s (%s) Ready.",
		VERSION,
		ServerDesc
	));
}

function Unregistered_Commands(cmdline) {
	cmdline = cmdline.slice(0,512); /* 512 bytes per RFC1459 */

	log(LOG_DEBUG,"[UNREG]: " + cmdline);

	var clockticks = system.timer;
	var i, cmd, p;

	cmd = IRC_parse(cmdline);

	if (!cmd.verb)
		return 0;

	if (cmd.verb.match(/^[0-9]+/))
		return 0;

	p = cmd.params;

	switch(cmd.verb) {
		case "PING":
			if (!p[0]) {
				this.numeric(409,":No origin specified.");
				break;
			}
			this.rawout("PONG " + ServerName + " :" + p[0]);
			break;
		case "CAP":
		case "CAPAB":
			break; /* Silently ignore, for now. */
		case "NICK":
			if (!p[0]) {
				this.numeric(431, ":No nickname given.");
				break;
			}
			p[0] = p[0].slice(0,MAX_NICKLEN);
			if (this.check_nickname(p[0]))
				this.nick = p[0];
			this.Unregistered_Check_User_Registration();
			break;
		case "PASS":
			if (!p[0] || this.password)
				break;
			this.password = p[0];
			break;
		case "PONG":
			this.pinged = false;
			break;
		case "SERVER":
			if (this.nick != "*") {
				this.numeric462();
				break;
			}
			if (!p[2]) {
				this.numeric461("SERVER");
				break;
			}
			if (Servers[p[0].toLowerCase()]) {
				if (parseInt(p[1]) < 2)
					this.quit("Server already exists.");
				return 0;
			}
			if (parseInt(p[1]) < 2) {
				for (i in NLines) {
					if (   (NLines[i].flags&NLINE_CHECK_QWKPASSWD)
						&& wildmatch(p[0],NLines[i].servername)
						&& Check_QWK_Password(
							p[0].slice(0,p[0].indexOf(".")).toUpperCase(),
							this.password
						   )
					) {
						Register_Unregistered_Local_Server(this, p, NLines[i]);
						return true;
					}
					if (   (NLines[i].flags&NLINE_CHECK_WITH_QWKMASTER)
						&& wildmatch(p[0],NLines[i].servername)
						&& this.QWK_Master_Authentication(
							p[0].slice(0,p[0].indexOf(".")).toUpperCase()
						)
					) {
						this.quit("QWK relaying not yet implemented.");
						return true;
					}
					if (   (NLines[i].password == this.password)
						&& (wildmatch(p[0],NLines[i].servername))
					) {
						Register_Unregistered_Local_Server(this, p, NLines[i]);
						return true;
					}
				}
			}
			this.quit("No matching server configuration found.");
			break;
		case "USER":
			if (this.uprefix)
				break;
			if (!p[3]) {
				this.numeric461("USER");
				break;
			}
			this.realname = p[3].slice(0,MAX_REALNAME);
			this.uprefix = parse_username(p[0]);
			this.Unregistered_Check_User_Registration();
			break;
		case "QUIT":
			this.quit();
			return 0;
		case "NOTICE":
			break; /* Drop silently */
		default:
			this.numeric451();
			return 0;
	}

	/* This part only executed if the command was legal. */
	if (!Profile[cmd.verb])
		Profile[cmd.verb] = new StatsM;
	Profile[cmd.verb].executions++;
	Profile[cmd.verb].ticks += system.timer - clockticks;
}

function Unregistered_Check_User_Registration() {
	var usernum, bbsuser;

	if (!this.dns_pending && !this.socket.outbound && this.uprefix && this.nick != "*") {
		if (this.password && (system.matchuser !== undefined)) {
			usernum = system.matchuser(this.uprefix);
			if (!usernum)
				usernum = system.matchuser(this.nick);
			if (usernum) {
				bbsuser = new User(usernum);
				if (this.password.toUpperCase() == bbsuser.security.password) {
					this.uprefix = parse_username(bbsuser.handle);
					bbsuser.connection = "IRC";
					bbsuser.logontime = time();
				}
			}
		}
		if (!usernum)
			this.uprefix = "~" + this.uprefix;
		if (this.hostname)
			this.welcome();
	}
}

function Unregistered_Quit(msg) {
	this.recvq.purge();
	this.sendq.purge();
	if (msg && this.socket.is_connected)
		this.socket.send(format("ERROR :%s\r\n", msg));
	if (server.client_remove !== undefined)
		server.client_remove(this.socket);
	if(server.clients != undefined)
		log(LOG_DEBUG,format("%d clients", server.clients));
	else
		log(LOG_INFO, format('[UNREG] QUIT ("%s")', msg));
	if (this.socket.outbound) {
		if (YLines[this.ircclass].active > 0) {
			YLines[this.ircclass].active--;
			log(LOG_DEBUG, format("Class %d down to %d active out of %d",
				this.ircclass,
				YLines[this.ircclass].active,
				YLines[this.ircclass].maxlinks
			));
		} else {
			log(LOG_ERR, format("Class %d YLine going negative", this.ircclass));
		}
	}
	this.socket.clearOn("read", this.socket.callback_id);
	this.socket.close();
	log(LOG_NOTICE,format(
		"%04u Connection closed.",
		this.socket.descriptor
	));
	js.clearInterval(this.pinginterval);
	delete Assigned_IDs[this.id];
	delete Unregistered[this.id];
}

function Unregistered_Welcome() {
	var i, my_iline;

	if (isklined(this.uprefix + "@" + this.hostname)) {
		this.numeric(465, ":You've been K:Lined from this server.");
		this.quit("You've been K:Lined from this server.");
		return 0;
	}
	/* FIXME: We don't compare connecting port. */
	for (i in ILines) {
		if (   (wildmatch(this.uprefix + "@" + this.ip, ILines[i].ipmask))
			&& (wildmatch(this.uprefix + "@" + this.hostname, ILines[i].hostmask))
		) {
			my_iline = ILines[i];
			break;
		}
	}
	if (!my_iline) {
		this.numeric(463, ":Your host isn't among the privileged.");
		this.quit("You are not authorized to use this server.");
		return 0;
	}
	if (my_iline.password && (my_iline.password!=this.password)) {
		this.numeric(464, ":Password Incorrect.");
		this.quit("Denied.");
		return 0;
	}
	Users[this.nick.toUpperCase()] = new IRC_User(this.id);
	var new_user = Users[this.nick.toUpperCase()];
	Local_Users[this.id] = new_user;
	new_user.socket = this.socket;
	new_user.socket.irc = new_user;
	new_user.nick = this.nick;
	new_user.uprefix = this.uprefix;
	new_user.hostname = this.hostname;
	new_user.realname = this.realname;
	new_user.created = time();
	new_user.ip = this.ip;
	new_user.ircclass = my_iline.ircclass;
	new_user.sendq = this.sendq;
	new_user.recvq = this.recvq;
	new_user.sendq.irc = new_user;
	new_user.recvq.irc = new_user;
	/* Remove the unregistered ping interval and install a new on based on Y:Line */
	js.clearInterval(this.pinginterval);
	new_user.pinginterval = js.setInterval(
		IRCClient_check_timeout,
		YLines[my_iline.ircclass].pingfreq * 1000,
		new_user
	);
	HCC_Counter++;
	if ( (true_array_len(Local_Users) + true_array_len(Local_Servers)) > HCC_Total)
		HCC_Total = true_array_len(Local_Users) + true_array_len(Local_Servers);
	if (true_array_len(Local_Users) > HCC_Users)
		HCC_Users = true_array_len(Local_Users);
	this.numeric("001", ":Welcome to the Synchronet IRC Service, " + new_user.nuh);
	this.numeric("002", ":Your host is " + ServerName + ", running version " + VERSION);
	this.numeric("003", format(":This server was created %s", SERVER_UPTIME_STRF));
	this.numeric("004", ServerName + " " + VERSION + " oiwbgscrkfydnhF biklmnopstv");
	/* This needs to be rewritten so the server capabilities are rendered dynamically */
	this.numeric("005","NETWORK=Synchronet"
		+ " MAXBANS=" + MAX_BANS
		+ " MAXCHANNELS=" + MAX_USER_CHANS
		+ " CHANNELLEN=" + MAX_CHANLEN
		+ " KICKLEN=" + MAX_KICKLEN
		+ " NICKLEN=" + MAX_NICKLEN
		+ " TOPICLEN=" + MAX_TOPICLEN
		+ " MODES=" + MAX_MODES
		+ " CHANTYPES=#&"
		+ " CHANLIMIT=#&:" + MAX_USER_CHANS
		+ " PREFIX=(ov)@+"
		+ " STATUSMSG=@+"
		+ " :are available on this server.");
	this.numeric("005","CASEMAPPING=ascii"
		+ " SILENCE=" + MAX_SILENCE
		+ " ELIST=cmnt"
		+ " CHANMODES=b,k,l,imnpst"
		+ " MAXLIST=b:" + MAX_BANS
		+ " TARGMAX=JOIN:,KICK:,KILL:,NOTICE:,PART:,PRIVMSG:,WHOIS:,WHOWAS: " /* FIXME */
		+ " :are available on this server.");
	this.numeric("005","AWAYLEN=" + MAX_AWAYLEN
		+ " :are available on this server.");
	new_user.lusers();
	new_user.motd();
	umode_notice(USERMODE_CLIENT,"Client",format(
		"Client connecting: %s (%s@%s) [%s] {%d}",
		this.nick,
		this.uprefix,
		this.hostname,
		this.ip,
		HCC_Counter
	));
	if (server.client_update != undefined)
		server.client_update(this.socket, this.nick, this.hostname);
	server_bcast_to_servers(format("NICK %s 1 %s + %s %s %s 0 %s :%s",
			this.nick,
			new_user.created,
			this.uprefix,
			this.hostname,
			ServerName,
			ip_to_int(new_user.ip),
			this.realname
	));
	log(LOG_NOTICE, format(
		"New IRC user: %s (%s@%s)",
		this.nick,
		this.uprefix,
		this.hostname
	));
	/* we're no longer unregistered. */
	delete Unregistered[this.id];
}

function QWK_Master_Authentication(qwkid) {
    var i, s;

    for (i in NLines) {
        if (NLines[i].flags&NLINE_IS_QWKMASTER) {
            s = searchbyserver(NLines[i].servername);
            if (!s) {
				this.quit("No QWK master available for authentication.");
				return false;
			}
			s.rawout(format(
				":%s PASS %s :%s QWK",
				ServerName,
				NLines[i].password,
				qwkid
			));
			return true;
		}
	}
	return false;
}

function Register_Unregistered_Local_Server(unreg, p, nline) {
	var i, s;

	Servers[p[0].toLowerCase()] = new IRC_Server();
	s = Servers[p[0].toLowerCase()];
	Local_Servers[p[0].toLowerCase()] = s;
	s.socket = unreg.socket;
	s.socket.irc = s;
	s.hops = p[1];
	s.info = p[2];
	s.parent = p[0];
	s.linkparent = ServerName;
	s.id = p[0].toLowerCase();
	s.flags = nline.flags;
	s.nick = p[0];
	s.hostname = unreg.hostname;
	s.recvq = unreg.recvq;
	s.sendq = unreg.sendq;
	s.recvq.irc = s;
	s.sendq.irc = s;
	s.ircclass = unreg.ircclass;

	for (i in HLines) {
		if (HLines[i].servername.toLowerCase() == p[0].toLowerCase()) {
			s.hub = true;
			break;
		}
	}
	for (i in ULines) {
		if (ULines[i] == p[0]) {
			s.uline = true;
			break;
		}
	}

	/* Remove the unregistered ping interval and install a new on based on Y:Line */
	js.clearInterval(unreg.pinginterval);
	s.pinginterval = js.setInterval(
		IRCClient_check_timeout,
		YLines[unreg.ircclass].pingfreq * 1000,
		s
	);
	s.finalize_server_connect("TS");
	delete Unregistered[unreg.id];
	delete Assigned_IDs[unreg.id];
}
