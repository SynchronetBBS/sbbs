// $Id: ircd_unreg.js,v 1.53 2020/04/04 03:34:03 deuce Exp $
//
// ircd_unreg.js
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
// Copyright 2003-2010 Randolph Erwin Sommerfeld <sysop@rrx.ca>
//
// ** Handle unregistered clients.
//

const UNREG_REVISION = "$Revision: 1.53 $".split(' ')[1];

////////// Objects //////////
function Unregistered_Client(id,socket) {
	////////// VARIABLES
	// Bools/Flags that change depending on connection state.
	this.pinged = false;	   // Sent PING?
	this.local = true;	   // FIXME: this is redundant.
	this.criteria_met = false; // Have we met registration criteria?
	// Variables containing user/server information as we receive it.
	this.id = id;
	this.nick = "*";
	this.realname = "";
	this.uprefix = "";
	this.hostname = "";
	this.password = "";
	this.ircclass = 0;
	this.idletime = time();
	this.ip = socket.remote_ip_address;
	this.pending_resolve = false;
	this.pending_resolve_time = time();
	this.sendps = true; // Send the PASS/SERVER pair by default.
	this.outgoing = false; /* We're an incoming connection by default */
	// Variables (consts, really) that point to various state information
	this.socket = socket;
	////////// FUNCTIONS
	// Functions we use to control clients (specific)
	this.work = Unregistered_Commands;
//	this.JSON_Unregistered_Commands = JSON_Unregistered_Commands;
	this.IRC_Unregistered_Commands = IRC_Unregistered_Commands;
	this.quit = Unregistered_Quit;
	this.check_timeout = IRCClient_check_timeout;
	this.check_queues = IRCClient_check_queues;
	this.resolve_check = Unregistered_Resolve_Check;
	this.welcome = Unregistered_Welcome;
	// Output helper functions (shared)
	this.rawout = rawout;
	this.originatorout = originatorout;
	this.ircout = ircout;
	this.sendq = new IRC_Queue();
	this.recvq = new IRC_Queue();
	this.server_notice = IRCClient_server_notice;
	this.check_nickname = IRCClient_check_nickname;
	// Numerics (shared)
	this.numeric = IRCClient_numeric;
	this.numeric451 = IRCClient_numeric451;
	this.numeric461 = IRCClient_numeric461;
	this.numeric462 = IRCClient_numeric462;
	////////// EXECUTION
	// We're a local socket that must be polled.
	Local_Sockets[id] = socket.descriptor;
	Local_Sockets_Map[id] = this;
	rebuild_socksel_array = true;
	log(format("%04u",socket.descriptor)
		+ " Accepted new connection: " + this.ip
		+ " port " + socket.remote_port);
	if ((this.ip.slice(0,4) == "127.") ||
	    (this.ip.slice(0,3) == "10.") ||
	    (this.ip.slice(0,8) == "192.168.") ||
	    (this.ip.slice(0,7) == "172.16." )) {
		this.hostname = servername;
		this.pending_resolve_time = false;
	} else {
		this.pending_resolve = load(true,"dnshelper.js",this.ip);
	}
	this.server_notice("*** " + VERSION + " (" + serverdesc + ") Ready. " + id);
}

////////// Command Parsers //////////

function Unregistered_Commands(cmdline) {
	// Only accept up to 512 bytes from unregistered clients.
	cmdline = cmdline.slice(0,512);

	// Kludge for broken clients.
	if ((cmdline[0] == "\r") || (cmdline[0] == "\n"))
		cmdline = cmdline.slice(1);

	if (debug)
		log(LOG_DEBUG,"[UNREG]: " + cmdline);

	this.IRC_Unregistered_Commands(cmdline);
}

function IRC_Unregistered_Commands(cmdline) {
	var clockticks = system.timer;
	var cmd;
	var command;

	cmd = cmdline.split(" ");
	if (cmdline[0] == ":") {
		// Silently ignore NULL originator commands.
		if (!cmd[1])
			return 0;
		// We allow non-matching :<origin> commands through FOR NOW,
		// since some *broken* IRC clients use this in the unreg stage.
		command = cmd[1].toUpperCase();
		cmdline = cmdline.slice(cmdline.indexOf(" ")+1);
		cmd = cmdline.split(" ");
	} else {
		command = cmd[0].toUpperCase();
	}

	// we ignore all numerics from unregistered clients.
	if (command.match(/^[0-9]+/))
		return 0;

	var legal_command = true; /* For tracking STATS M */
	
	switch(command) {
		case "PING":
			if (!cmd[1]) {
				this.numeric(409,":No origin specified.");
				break;
			}
			this.rawout("PONG " + servername + " :" + IRC_string(cmdline,1));
			break;
		case "CAPAB":
			break; // Silently ignore, for now.
		case "NICK":
			if (!cmd[1]) {
				this.numeric(431, ":No nickname given.");
				break;
			}
			var the_nick = IRC_string(cmd[1],0).slice(0,max_nicklen);
			if (this.check_nickname(the_nick))
				this.nick = the_nick;
			break;
		case "PASS":
			if (!cmd[1] || this.password)
				break;
			this.password = IRC_string(cmd[1],0);
			break;
		case "PONG":
			this.pinged = false;
			break;
		case "SERVER":
			if ((this.nick != "*") || this.criteria_met) {
				this.numeric462();
				break;
			}
			if (!cmd[3]) {
				this.numeric461("SERVER");
				break;
			}
			if (Servers[cmd[1].toLowerCase()]) {
	     			if (parseInt(cmd[2]) < 2)
					this.quit("Server already exists.");
				return 0;
			}
			var this_nline = 0;
			var qwk_slave = false;
			var qwkid = cmd[1].slice(0,cmd[1].indexOf(".")).toUpperCase();
     			if (parseInt(cmd[2]) < 2) {
				for (nl in NLines) {
					if ((NLines[nl].flags&NLINE_CHECK_QWKPASSWD) &&
					    wildmatch(cmd[1],NLines[nl].servername)) {
						if (check_qwk_passwd(qwkid,this.password)) {
							this_nline = NLines[nl];
							break;
						}
					} else if ((NLines[nl].flags&NLINE_CHECK_WITH_QWKMASTER) &&
						   wildmatch(cmd[1],NLines[nl].servername)) {
							for (qwkm_nl in NLines) {
								if (NLines[qwkm_nl].flags&NLINE_IS_QWKMASTER) {
									var qwk_master = searchbyserver(NLines[qwkm_nl].servername);
									if (!qwk_master) {
										this.quit("No QWK master available for authorization.");
										return 0;
									} else {
										qwk_master.rawout(":" + servername + " PASS " + this.password + " :" + qwkid + " QWK");
										qwk_slave = true;
									}
								}
							}
					} else if ((NLines[nl].password == this.password) &&
						   (wildmatch(cmd[1],NLines[nl].servername))
						  ) {
							this_nline = NLines[nl];
							break;
					}
				}
			}
			if ( (!this_nline ||
			      ( (this_nline.password == "*") && !this.outgoing
				&& !(this_nline.flags&NLINE_CHECK_QWKPASSWD) )
			     ) && !qwk_slave) {
	     			if (parseInt(cmd[2]) < 2)
					this.quit("UR Server not configured.");
				return 0;
			}
			// Take care of registration right now.
			Servers[cmd[1].toLowerCase()] = new IRC_Server();
			var new_server = Servers[cmd[1].toLowerCase()];
			Local_Servers[this.id] = new_server;
			Local_Sockets_Map[this.id] = new_server;
			delete Unregistered[this.id];
			rebuild_socksel_array = true;
			new_server.socket = this.socket;
			new_server.hops = cmd[2];
			new_server.info = IRC_string(cmdline,3);
			new_server.parent = cmd[1];
			new_server.linkparent = servername;
			new_server.id = this.id;
			new_server.flags = this_nline.flags;
			new_server.nick = cmd[1];
			new_server.hostname = this.hostname;
			new_server.recvq = this.recvq;
			new_server.sendq = this.sendq;
			new_server.ircclass = this.ircclass;
			new_server.outgoing = this.outgoing;
			if (!qwk_slave) { // qwk slaves should never be hubs.
				for (hl in HLines) {
					if (HLines[hl].servername.toLowerCase()
					    == cmd[1].toLowerCase()) {
						new_server.hub = true;
						break;
					}
				}
				// nor should they be ulined.
				for (u in ULines) {
					if (ULines[u] == cmd[1]) {
						new_server.uline = true;
						break;
					}
				}
			}
			if (this_nline.flags&NLINE_IS_DREAMFORGE)
				new_server.type = DREAMFORGE;
			new_server.finalize_server_connect("TS",this.sendps);
			this.replaced_with = new_server;
			break;
		case "USER":
			if (this.uprefix)
				break;
			if (!cmd[4]) {
				this.numeric461("USER");
				break;
			}
			this.realname = IRC_string(cmdline,4).slice(0,50);
			this.uprefix = parse_username(cmd[1]);
			break;
		case "QUIT":
			this.quit();
			return 0;
		case "NOTICE":
			break; // drop silently
		default:
			this.numeric451();
			return 0;
	}

	/* This part only executed if the command was legal. */

	if (!this.criteria_met && this.uprefix && (this.nick != "*") ) {
		var usernum;
		if (this.password) {
			usernum = system.matchuser(this.uprefix);
			if (!usernum)
				usernum = system.matchuser(this.nick);
			if (usernum) {
				var bbsuser = new User(usernum);
				if (this.password.toUpperCase() == bbsuser.security.password) {
					this.uprefix = parse_username(bbsuser.handle);
					bbsuser.connection = "IRC";
					bbsuser.logontime = time();
				}
			}
		}
		if (!usernum)
			this.uprefix = "~" + this.uprefix;
		this.criteria_met = true;
		if (this.hostname && !this.pending_resolve_time)
			this.welcome();
	}

	if (!Profile[command])
		Profile[command] = new StatsM;
	Profile[command].executions++;
	Profile[command].ticks += system.timer - clockticks;

}

////////// Functions //////////

function Unregistered_Quit(msg) {
	if (msg)
		this.rawout("ERROR :" + msg);
	if (server.client_remove!=undefined)
		server.client_remove(this.socket);
	if(server.clients != undefined)
		log(LOG_DEBUG,format("%d clients", server.clients));
	else
		log(LOG_INFO, "Unregistered_Quit(\""+msg+"\")");
	this.socket.close();
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
	delete Unregistered[this.id];
	delete this;
	rebuild_socksel_array = true;
}

function Unregistered_Resolve_Check() {
	var my_resolved = this.pending_resolve.read();
	if (my_resolved) {
		if (my_resolved.search(/[.]/))
			this.hostname = my_resolved;
		else
			this.hostname = servername;
		this.pending_resolve_time = false;
	} else if ( (time() - this.pending_resolve_time) > 5) {
		this.hostname = this.ip;
		this.pending_resolve_time = false;
	}
	if (this.criteria_met && !this.pending_resolve_time)
		this.welcome();
	return 0;
}

function Unregistered_Welcome() {
	if (isklined(this.uprefix + "@" + this.hostname)) {
		this.numeric(465, ":You've been K:Lined from this server.");
		this.quit("You've been K:Lined from this server.");
		return 0;
	}
	// Check for a valid I:Line.
	var my_iline;
	// FIXME: We don't compare connecting port.
	for(thisILine in ILines) {
		if ((wildmatch(this.uprefix + "@" +
		    this.ip,
		    ILines[thisILine].ipmask)) &&
		    (wildmatch(this.uprefix + "@" +
		    this.hostname,
		    ILines[thisILine].hostmask))
		   ) {
			my_iline = ILines[thisILine];
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
	// This user is good to go, add his connection to the total.
	if ( (true_array_len(Local_Users) + true_array_len(Local_Servers)) > hcc_total)
		hcc_total = true_array_len(Local_Users) + true_array_len(Local_Servers);
	if (true_array_len(Local_Users) > hcc_users)
		hcc_users = true_array_len(Local_Users);
	// Amazing.  We meet the registration criteria.
	Users[this.nick.toUpperCase()] = new IRC_User(this.id);
	var new_user = Users[this.nick.toUpperCase()];
	Local_Sockets_Map[this.id] = new_user;
	Local_Users[this.id] = new_user;
	rebuild_socksel_array = true;
	new_user.socket = this.socket;
	new_user.nick = this.nick;
	new_user.uprefix = this.uprefix;
	new_user.hostname = this.hostname;
	new_user.realname = this.realname;
	new_user.created = time();
	new_user.ip = this.ip;
	new_user.ircclass = my_iline.ircclass;
	new_user.sendq = this.sendq;
	new_user.recvq = this.recvq;
	// Shouldn't be a thing...
	new_user.outgoing = this.outgoing;
	hcc_counter++;
	this.numeric("001", ":Welcome to the Synchronet IRC Service, " + new_user.nuh);
	this.numeric("002", ":Your host is " + servername + ", running version " + VERSION);
	this.numeric("003", ":This server was created " + strftime("%a %b %d %Y at %H:%M:%S %Z",server_uptime));
	this.numeric("004", servername + " " + VERSION + " oiwbgscrkfydnhF biklmnopstv");
	this.numeric("005", "NETWORK=Synchronet MAXBANS=" + max_bans + " "
		+ "MAXCHANNELS=" + max_user_chans + " CHANNELLEN=" + max_chanlen + " "
		+ "KICKLEN=" + max_kicklen + " NICKLEN=" + max_nicklen + " "
		+ "TOPICLEN=" + max_topiclen + " MODES=" + max_modes + " "
		+ "CHANTYPES=#& CHANLIMIT=#:" + max_user_chans + " PREFIX=(ov)@+ "
		+ "STATUSMSG=@+ :are available on this server.");
	this.numeric("005", "CASEMAPPING=ascii SILENCE=" + max_silence + " "
		+ "ELIST=cmnt CHANMODES=b,k,l,imnpst "
		+ "MAXLIST=b:" + max_bans + " "
		+ "TARGMAX=JOIN:,KICK:,KILL:,NOTICE:,PART:,PRIVMSG:,WHOIS:,WHOWAS: "
		+ ":are available on this server.");
	new_user.lusers();
	new_user.motd();
	umode_notice(USERMODE_CLIENT,"Client","Client connecting: " +
		this.nick + " (" + this.uprefix + "@" + this.hostname +
		") [" + this.ip + "] {" + hcc_counter + "}");
	if (server.client_update != undefined)
		server.client_update(this.socket, this.nick, this.hostname);
	var nickstr = "NICK " + this.nick + " 1 " + new_user.created + " ";
	server_bcast_to_servers(nickstr + "+ " + this.uprefix + " " + this.hostname + " " + servername + " 0 " + ip_to_int(new_user.ip) + " :" + this.realname,BAHAMUT);
	server_bcast_to_servers(nickstr + this.uprefix + " " + this.hostname + " " + servername + " 0 " + " :" + this.realname,DREAMFORGE);
	// we're no longer unregistered.
	this.replaced_with = new_user;
	delete Unregistered[this.id];
	delete this;
}
