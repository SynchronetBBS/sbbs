// $Id$
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
// Copyright 2003 Randolph Erwin Sommerfeld <sysop@rrx.ca>
//
// ** Handle unregistered clients.
//

const UNREG_REVISION = "$Revision$".split(' ')[1];

////////// Objects //////////
function Unregistered_Client(id,socket) {
	////////// VARIABLES
	// Bools/Flags that change depending on connection state.
	this.pinged = false;	// Sent PING?
	this.sentps = false;	// Sent PASS/SERVER?
	this.local = true;	// FIXME: this is redundant.
	// Variables containing user/server information as we receive it.
	this.id = id;
	this.nick = "*";
	this.realname = "";
	this.uprefix = "";
	this.hostname = "";
	this.password = "";
	this.ircclass = 0;
	this.idletime = time();
	// Variables (consts, really) that point to various state information
	this.socket = socket;
	////////// FUNCTIONS
	// Functions we use to control clients (specific)
	this.work = Unregistered_Commands;
	this.quit = Unregistered_Quit;
	this.check_timeout = IRCClient_check_timeout;
	// Output helper functions (shared)
	this.rawout = rawout;
	this.originatorout = originatorout;
	this.ircout = ircout;
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
	rebuild_socksel_array();
	log(format("%04u",socket.descriptor)
		+ " Accepted new connection: " + socket.remote_ip_address
		+ " port " + socket.remote_port);
	if ((socket.remote_ip_address.slice(0,4) == "127.") ||
	    (socket.remote_ip_address.slice(0,3) == "10.") ||
	    (socket.remote_ip_address.slice(0,8) == "192.168.") ||
	    (socket.remote_ip_address.slice(0,7) == "172.16." )) {
		this.hostname = servername;
	} else {
		var went_into_hostname;
		var possible_hostname;
		if (debug && resolve_hostnames)
			went_into_hostname = time();
		if (resolve_hostnames)
			possible_hostname = resolve_host(socket.remote_ip_address);
		if (resolve_hostnames && possible_hostname) {
			this.hostname = possible_hostname;
			if(server.client_update != undefined)
				server.client_update(socket,this.nick,this.hostname);
		} else {
			this.hostname = socket.remote_ip_address;
		}
		if (debug && resolve_hostnames) {
			went_into_hostname = time() - went_into_hostname;
			if (went_into_hostname == "NaN")
				went_into_hostname=0;
			umode_notice(USERMODE_DEBUG,"Debug",
				"resolve_host took " +
				went_into_hostname + " seconds.");
		}
	}
	this.server_notice("*** " + VERSION + " (" + serverdesc + ") Ready.");
}

////////// Command Parsers //////////

function Unregistered_Commands() {
	var cmdline;
	var cmd;
	var command;

	if (!this.socket.is_connected) {
		this.quit();
		return 0;
	}
	cmdline=this.socket.recvline(4096,0);

	if (!cmdline)
		return 0;
	// Only accept up to 512 bytes from unregistered clients.
	cmdline = cmdline.slice(0,512);
	// Kludge for broken clients.
	if ((cmdline[0] == "\r") || (cmdline[0] == "\n"))
		cmdline = cmdline.slice(1);
	if (debug)
		log("[UNREG]: " + cmdline);
	cmd = cmdline.split(" ");
	if (cmdline[0] == ":") {
		// Silently ignore NULL originator commands.
		if (!cmd[1])
			return 0;
		// We allow non-matching :<origin> commands through FOR NOW,
		// since some *broken* IRC clients use this in the unreg stage.
		command = cmd[1].toUpperCase();
		cmdline = cmdline.slice(cmdline.indexOf(" ")+1);
	} else {
		command = cmd[0].toUpperCase();
	}

	// we ignore all numerics from unregistered clients.
	if (command.match(/^[0-9]+/))
		return 0;
	
	switch(command) {
		case "PING":
			if (!cmd[1]) {
				this.numeric(409,":No origin specified.");
				break;
			}
			this.rawout("PONG " + servername + " :" + IRC_string(cmdline));
			break;
		case "CAPAB":
			break; // Silently ignore, for now.
		case "NICK":
			if (!cmd[1]) {
				this.numeric(431, ":No nickname given.");
				break;
			}
			var the_nick = IRC_string(cmd[1]).slice(0,max_nicklen);
			if (this.check_nickname(the_nick))
				this.nick = the_nick;
			break;
		case "PASS":
			if (!cmd[1])
				break;
			this.password = cmd[1];
			break;
		case "PONG":
			this.pinged = false;
			break;
		case "SERVER":
			if (this.nick != "*") {
				this.numeric462();
				break;
			}
			if (!cmd[3]) {
				this.numeric461("SERVER");
				break;
			}
			var this_nline = 0;
			var qwk_slave = false;
			var qwkid = cmd[1].slice(0,cmd[1].indexOf(".")).toUpperCase();
			for (nl in NLines) {
				if ((NLines[nl].flags&NLINE_CHECK_QWKPASSWD) &&
				    IRC_match(cmd[1],NLines[nl].servername)) {
					if (check_qwk_passwd(qwkid,this.password)) {
						this_nline = NLines[nl];
						break;
					}
				} else if ((NLines[nl].flags&NLINE_CHECK_WITH_QWKMASTER) &&
					   IRC_match(cmd[1],NLines[nl].servername)) {
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
					   (IRC_match(cmd[1],NLines[nl].servername))
					  ) {
						this_nline = NLines[nl];
						break;
				}
			}
			if ( (!this_nline ||
			      ( (this_nline.password == "*") && !this.sentps &&
				!(this_nline.flags&NLINE_CHECK_QWKPASSWD) )
			     ) && !qwk_slave) {
				this.quit("Server not configured.");
				return 0;
			}
			if (Servers[cmd[1].toUpperCase()]) {
				this.quit("Server already exists.");
				return 0;
			}
			// Take care of registration right now.
			Servers[cmd[1].toLowerCase()] = new IRC_Server;
			var new_server = Servers[cmd[1].toLowerCase()];
			Local_Servers[this.id] = new_server;
			Local_Sockets_Map[this.id] = new_server;
			rebuild_socksel_array();
			new_server.socket = this.socket;
			new_server.hops = cmd[2];
			new_server.info = IRC_string(cmdline);
			new_server.parent = cmd[1].toLowerCase();
			new_server.linkparent = servername;
			new_server.id = this.id;
			new_server.flags = this_nline.flags;
			new_server.nick = cmd[1];
			new_server.hostname = this.hostname;
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
			new_server.finalize_server_connect("TS");
			break;
		case "USER":
			if (!cmd[4]) {
				this.numeric461("USER");
				break;
			}
			this.realname = IRC_string(cmdline).slice(0,50);
			var unreg_username = parse_username(cmd[1]);
			this.uprefix = "~" + unreg_username;
			break;
		case "QUIT":
			this.quit();
			return 0;
		case "NOTICE":
			break; // drop silently
		default:
			this.numeric451();
			break;
	}
	if (this.uprefix && isklined(this.uprefix + "@" + this.hostname)) {
		this.numeric(465, ":You've been K:Lined from this server.");
		this.quit("You've been K:Lined from this server.");
		return 0;
	}
	if (this.password && (unreg_username || (this.nick != "*"))) {
		var usernum;
		if (unreg_username)
			usernum = system.matchuser(unreg_username);
		if (!usernum && (this.nick != "*"))
			usernum = system.matchuser(this.nick);
		if (usernum) {
			var bbsuser = new User(usernum);
			if (this.password.toUpperCase() == bbsuser.security.password)
				this.uprefix = parse_username(bbsuser.handle).toLowerCase().slice(0,10);
		}
	}
	if ( (true_array_len(Local_Users) + true_array_len(Local_Servers)) > hcc_total)
		hcc_total = true_array_len(Local_Users) + true_array_len(Local_Servers);
	if (true_array_len(Local_Users) > hcc_users)
		hcc_users = true_array_len(Local_Users);
	if (this.realname && this.uprefix && (this.nick != "*")) {
		// Check for a valid I:Line.
		var my_iline;
		// FIXME: We don't compare connecting port.
		for(thisILine in ILines) {
			if ((IRC_match(this.uprefix + "@" +
			    this.socket.remote_ip_address,
			    ILines[thisILine].ipmask)) &&
			    (IRC_match(this.uprefix + "@" + 
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
		if (!this.check_nickname(this.nick))
			return 0;
		// Amazing.  We meet the registration criteria.
		Users[this.nick.toUpperCase()] = new IRC_User(this.id);
		new_user = Users[this.nick.toUpperCase()];
		Local_Sockets_Map[this.id] = new_user;
		Local_Users[this.id] = new_user;
		rebuild_socksel_array();
		new_user.socket = this.socket;
		new_user.nick = this.nick;
		new_user.uprefix = this.uprefix;
		new_user.hostname = this.hostname;
		new_user.realname = this.realname;
		new_user.created = time();
		if (this.socket.remote_ip_address)
			new_user.ip = this.socket.remote_ip_address;
		new_user.ircclass = my_iline.ircclass;
		hcc_counter++;
		this.numeric("001", ":Welcome to the Synchronet IRC Service, " + new_user.nuh);
		this.numeric("002", ":Your host is " + servername + ", running " + VERSION);
		this.numeric("003", ":This server was created " + strftime("%a %b %e %Y at %H:%M:%S %Z",server_uptime));
		this.numeric("004", servername + " " + VERSION + " oiwbgscrkfydnhF biklmnopstv");
		this.numeric("005", "MODES=" + max_modes + " MAXCHANNELS=" + max_user_chans + " CHANNELLEN=" + max_chanlen + " MAXBANS=" + max_bans + " NICKLEN=" + max_nicklen + " TOPICLEN=" + max_topiclen + " KICKLEN=" + max_kicklen + " CHANTYPES=#& PREFIX=(ov)@+ NETWORK=Synchronet CASEMAPPING=ascii CHANMODES=b,k,l,imnpst STATUSMSG=@+ :are available on this server.");
		new_user.lusers();
		new_user.motd();
		umode_notice(USERMODE_CLIENT,"Client","Client connecting: " +
			this.nick + " (" + this.uprefix + "@" + this.hostname +
			") [" + this.socket.remote_ip_address + "] {1}");
		if (server.client_update != undefined)
			server.client_update(this.socket, this.nick, this.hostname);
		server_bcast_to_servers("NICK " + this.nick + " 1 " + this.created + " + " + this.uprefix + " " + this.hostname + " " + servername + " 0 " + ip_to_int(new_user.ip) + " :" + this.realname);
		// we're no longer unregistered.
		delete Unregistered[this];
	}
}

////////// Functions //////////

function Unregistered_Quit(msg) {
	if (msg)
		this.rawout("ERROR :" + msg);
	this.socket.close();
	delete Local_Sockets[this.id];
	delete Local_Sockets_Map[this.id];
	delete Unregistered[this.id];
	delete this;
	rebuild_socksel_array();
}

