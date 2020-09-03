// $Id: ircd.js,v 1.193 2020/04/04 08:32:04 deuce Exp $
//
// ircd.js
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

//load("synchronet-json.js");

load("sbbsdefs.js");
load("sockdefs.js");
load("nodedefs.js");

load("irclib.js");

load("ircd_unreg.js");
load("ircd_user.js");
load("ircd_channel.js");
load("ircd_server.js");

// CVS revision
const MAIN_REVISION = "$Revision: 1.193 $".split(' ')[1];

// Please don't play with this, unless you're making custom hacks.
// IF you're making a custom version, it'd be appreciated if you left the
// version number alone, and add a token in the form of +hack (i.e. 1.0+cyan)
// This is so everyone knows your revision base, AND type of hack used.
const VERSION = "SynchronetIRCd-1.3a(" + MAIN_REVISION + ")";
const VERSION_STR = "Synchronet " 
	+ system.version + system.revision + "-" + system.platform 
	+ system.beta_version + " (IRCd by Randy Sommerfeld)";

// This will dump all I/O to and from the server to your Synchronet console.
// It also enables some more verbose WALLOPS, especially as they pertain to
// blocking functions.
// The special "DEBUG" oper command also switches this value.
var debug = false;

// The number of seconds to block before giving up on outbound CONNECT
// attempts (when connecting to another IRC server -- i.e. a hub)  This value
// is important because connecing is a BLOCKING operation, so your IRC *will*
// freeze for the amount of time it takes to connect.
const ob_sock_timeout = 3;

// Should we enable the USERS and SUMMON commands?  These allow IRC users to
// view users on the local BBS and summon them to IRC via a Synchronet telegram
// message respectively.  Some people might be running the ircd standalone, or
// otherwise don't want anonymous IRC users to have access to these commands.
// We enable this by default because there's typically nothing wrong with
// seeing who's on an arbitrary BBS or summoning them to IRC.
const enable_users_summon = true;

// what our server is capable of from a server point of view.
// TS3 = Version 3 of accepted interserver timestamp protocol.
// NOQUIT = QUIT clients on behalf of a SQUIT server? (no netsplit spam)
// SSJOIN = SJOIN interserver command without dual TS, single TS only.
// BURST = Sending of network synch data is done in a 3-stage burst (BURST cmd)
// UNCONNECT = Server SQUIT is routable.
// NICKIP = 9th parameter of interserver NICK command is an integer IP.
// TSMODE = 2nd arg to standard MODE is the channel's TS.
const server_capab = "TS3 NOQUIT SSJOIN BURST UNCONNECT NICKIP TSMODE";

// EVERY server on the network MUST have the same values in ALL of these
// categories.  If you change these, you WILL NOT be able to link to the
// Synchronet IRC network.  Linking servers with different values here WILL
// cause your network to desynchronize (and possibly crash the IRCD)
// Remember, this is Synchronet, not Desynchronet ;)
const max_chanlen = 100;	// Maximum channel name length.
const max_nicklen = 30;		// Maximum nickname length.
const max_modes = 6;		// Maximum modes on single MODE command
const max_user_chans = 10;	// Maximum channels users can join
const max_bans = 25;		// Maximum bans (+b) per channel
const max_topiclen = 307;	// Maximum length of topic per channel
const max_kicklen = 307;	// Maximum length of kick reasons
const max_who = 100;		// Maximum replies to WHO for non-oper users
const max_silence = 10;		// Maximum entries on a user's SILENCE list

/* Server types */
const BAHAMUT = 1;
const DREAMFORGE = 2;

var default_port = 6667;

log(VERSION + " started.");

// Our primary arrays.
Unregistered = new Object;
Users = new Object;
Servers = new Object;
Channels = new Object;

Local_Sockets = new Object;
Local_Sockets_Map = new Object;

Selectable_Sockets = new Object;
Selectable_Sockets_Map = new Object;

Global_CommandLine = ""; // We use this to track if a cmdline causes a crash.

hcc_total = 0;
hcc_users = 0;
hcc_counter = 0;
server_uptime = time();

WhoWas = new Object;	/* Stores uppercase nicks */
WhoWasMap = new Array;	/* A true push/pop array pointing to WhoWas entries */
WhoWas_Buffer = 1000;	/* Maximum number of WhoWas entries to keep track of */

NickHistory = new Array;	/* A true array using push and pop */
nick_buffer = 1000;
nick_pointer = 0;

/* Keep track of commands and how long they take to execute. */
Profile = new Object;

// This is where our unique ID for each client comes from for unreg'd clients.
next_client_id = 0;

// An array containing all the objects containing local sockets that we need
// to poll.
Local_Users = new Object;
Local_Servers = new Object;

rebuild_socksel_array = true;

network_debug = false;

last_recvq_check = 0;

/*
 * A tri-state variable indicating if socket.send is "old" (ie: returns bool)
 * or "new" (ie: returns count of bytes sent).
 */
var new_socket_send;

// Parse command-line arguments.
config_filename="";
var cmdline_port;
for (cmdarg=0;cmdarg<argc;cmdarg++) {
	switch(argv[cmdarg].toLowerCase()) {
		case "-f":
			config_filename = argv[++cmdarg];
			break;
		case "-p":
			cmdline_port = parseInt(argv[++cmdarg]);
			break;
		case "-d":
			debug=true;
			break;
	}
}

read_config_file();

if(this.server==undefined) {		// Running from JSexec?
	if (!jsexec_revision_detail)
		jsexec_revision_detail = "JSexec";
	if (cmdline_port)
		default_port = cmdline_port;
	else if (mline_port)
		default_port = mline_port;

	server = { socket: false, terminated: false,
		version_detail: jsexec_revision_detail, interface_ip_addr_list: ["0.0.0.0","::"] };
	server.socket = create_new_socket(default_port)
	if (!server.socket)
		exit();
}

server.socket.nonblocking = true;	// REQUIRED!
server.socket.debug = false;		// Will spam your log if true :)

// Now open additional listening sockets as defined on the P:Line in ircd.conf
open_plines = new Array(); /* True Array */
// Make our 'server' object the first open P:Line
open_plines[0] = server.socket;
for (pl in PLines) {
	var new_pl_sock = create_new_socket(PLines[pl]);
	if (new_pl_sock) {
		new_pl_sock.nonblocking = true;
		new_pl_sock.debug = false;
		open_plines.push(new_pl_sock);
	}
}

js.branch_limit=0; // we're not an infinite loop.
js.auto_terminate=false; // we handle our own termination requests

///// Main Loop /////
while (!js.terminated) {

	if(file_date(system.ctrl_dir + "ircd.rehash") > time_config_read)
		read_config_file();

	// Setup a new socket if a connection is accepted.
	for (pl in open_plines) {
		if (open_plines[pl].poll()) {
			var client_sock=open_plines[pl].accept();
			log(LOG_DEBUG,"Accepting new connection on port "
				+ client_sock.local_port);
			if(client_sock) {
				client_sock.nonblocking = true;
				switch(client_sock.local_port) {
					case 994:
					case 6697:
						client_sock.ssl_server=1;
				}
				if (!client_sock.remote_ip_address) {
					log(LOG_DEBUG,"Socket has no IP address.  Closing.");
					client_sock.close();
				} else if (iszlined(client_sock.remote_ip_address)) {
					client_sock.send(":" + servername
						+ " 465 * :You've been Z:Lined from this server.\r\n");
					client_sock.close();
				} else {
					var new_id = "id" + next_client_id;
					next_client_id++;
					if(server.client_add != undefined)
						server.client_add(client_sock);
					if(server.clients != undefined)
						log(LOG_DEBUG,format("%d clients", server.clients));
					Unregistered[new_id] = new Unregistered_Client(new_id,
						client_sock);
				}
			} else
				log(LOG_DEBUG,"!ERROR " + open_plines[pl].error
					+ " accepting connection");
		}
	}

	// Check for pending DNS hostname resolutions.
	for(this_unreg in Unregistered) {
		if (Unregistered[this_unreg] &&
		    Unregistered[this_unreg].pending_resolve_time)
			Unregistered[this_unreg].resolve_check();
	}

	// Only rebuild our selectable sockets if required.
	if (rebuild_socksel_array) {
		Selectable_Sockets = new Array;
		Selectable_Sockets_Map = new Array;
		for (i in Local_Sockets) {
			Selectable_Sockets.push(Local_Sockets[i]);
			Selectable_Sockets_Map.push(Local_Sockets_Map[i]);
		}
		rebuild_socksel_array = false;
	}

	/* Check for ping timeouts and process queues. */
	/* FIXME/TODO: These need to be changed to a mapping system ASAP. */
	for(this_sock in Selectable_Sockets) {
		if (Selectable_Sockets_Map[this_sock]) {
			Selectable_Sockets_Map[this_sock].check_timeout();
            Selectable_Sockets_Map[this_sock].check_queues();
        }
    }

	// do some work.
	if (Selectable_Sockets.length) {
		var readme = socket_select(Selectable_Sockets, 1 /*secs*/);
		try {
			for(thisPolled in readme) {
				if (Selectable_Sockets_Map[readme[thisPolled]]) {
					var conn = Selectable_Sockets_Map[readme[thisPolled]];
					if (!conn.socket.is_connected) {
						conn.quit("Connection reset by peer.");
						continue;
					}
					conn.recvq.recv(conn.socket);
				}
			}
		} catch(e) {
			gnotice("FATAL ERROR: " + e + " CMDLINE: " + Global_CommandLine);
			log(LOG_ERR,"JavaScript exception: " + e + " CMDLINE: "
				+ Global_CommandLine);
			terminate_everything("A fatal error occured!", /* ERROR? */true);
		}
	} else {
		mswait(100);
	}

	// Scan C:Lines for servers to connect to automatically.
	var my_cline;
	for(thisCL in CLines) {
		my_cline = CLines[thisCL];
		if (my_cline.port && YLines[my_cline.ircclass].connfreq &&
		    (YLines[my_cline.ircclass].maxlinks > YLines[my_cline.ircclass].active) &&
		    (search_server_only(my_cline.servername) < 1) &&
		     ((time() - my_cline.lastconnect) >
		     YLines[my_cline.ircclass].connfreq)
		   ) {
			umode_notice(USERMODE_ROUTING,"Routing",
				"Auto-connecting to " +
				CLines[thisCL].servername + " ("+CLines[thisCL].host+")");
			connect_to_server(CLines[thisCL]);
		}
	}

}

// End of our run, so terminate everything before we go.
terminate_everything("Terminated.");

//////////////////////////////// END OF MAIN ////////////////////////////////

// Okay, welcome to my world.
// str = The string used for the quit reason UNLESS 'is_netsplit' is set to
//       true, in which case it becomes the string used to QUIT individual
//       clients in a netsplit (i.e. "server.one server.two")
// suppress_bcast = Set to TRUE if you don't want the message to be broadcast
//       accross the entire network.  Useful for netsplits, global kills, or
//       other network messages where the rest of the network is nuking the
//       client on their own.
// is_netsplit = Should never be used except in a recursive call from the
//       'this.netsplit()' function.  Tells the function that we're recursive
//       and to use 'str' as the reason for quiting all the clients
// origin = an object typically only passed in the case of a SQUIT, contains
//       the client who originated the message (i.e. for generating netsplit
//       messages.)
// FIXME: this function split into three. comments kept for now, but nuke later

////////// Functions not linked to an object //////////

// Sigh, there's no way to tell the length of an associative array in JS, so,
// we have this to help us:
function true_array_len(my_array) {
	var counter = 0;
	for (i in my_array) {
		counter++;
	}
	return counter;
}

function terminate_everything(terminate_reason, error) {
	log(error ? LOG_ERR : LOG_NOTICE, "Terminating: " + terminate_reason);
	for(thisClient in Local_Sockets_Map) {
		var Client = Local_Sockets_Map[thisClient];
		Client.rawout("ERROR :" + terminate_reason);
		Client.socket.close();
	}
	exit(error);
}

function search_server_only(server_name) {
	if (!server_name)
		return 0;
	for(thisServer in Servers) {
		var Server=Servers[thisServer];
		if (wildmatch(Server.nick,server_name))
			return Server;
	}
	if (wildmatch(servername,server_name))
		return -1; // the server passed to us is our own.
	// No success.
	return 0;
}

function searchbyserver(servnick) {
	if (!servnick)
		return 0;
	var server_try = search_server_only(servnick);
	if (server_try) {
		return server_try;
	} else {
		for(thisNick in Users) {
			var Nick=Users[thisNick];
			if (wildmatch(Nick.nick,servnick))
				return search_server_only(Nick.servername);
		}
	}
	return 0; // looks like we failed after all that hard work :(
}

// Only allow letters, numbers and underscore in username to a maximum of
// 9 characters for 'anonymous' users (i.e. not using PASS to authenticate.)
// hostile characters like !,@,: etc would be bad here :)
function parse_username(str) {
	str = str.replace(/[^\w]/g,"").toLowerCase();
	if (!str)
		str = "user"; // nothing? we'll give you something boring.
	return str.slice(0,9);
}

function parse_nline_flags(flags) {
	var nline_flags = 0;
	for(thisflag in flags) {
		switch(flags[thisflag]) {
			case "q":
				nline_flags |= NLINE_CHECK_QWKPASSWD;
				break;
			case "w":
				nline_flags |= NLINE_IS_QWKMASTER;
				break;
			case "k":
				nline_flags |= NLINE_CHECK_WITH_QWKMASTER;
				break;
			case "d":
				nline_flags |= NLINE_IS_DREAMFORGE;
				break;
			default:
				log(LOG_WARNING,"!WARNING Unknown N:Line flag '"
					+ flags[thisflag] + "' in config.");
				break;
		}
	}
	return nline_flags;
}

function parse_oline_flags(flags) {
	var oline_flags = 0;
	for(thisflag in flags) {
		switch(flags[thisflag]) {
			case "r":
				oline_flags |= OLINE_CAN_REHASH;
				break;
			case "R":
				oline_flags |= OLINE_CAN_RESTART;
				break;
			case "D":
				oline_flags |= OLINE_CAN_DIE;
				break;
			case "g":
				oline_flags |= OLINE_CAN_GLOBOPS;
				break;
			case "w":
				oline_flags |= OLINE_CAN_WALLOPS;
				break;
			case "l":
				oline_flags |= OLINE_CAN_LOCOPS;
				break;
			case "c":
				oline_flags |= OLINE_CAN_LSQUITCON;
				break;
			case "C":
				oline_flags |= OLINE_CAN_GSQUITCON;
				break;
			case "k":
				oline_flags |= OLINE_CAN_LKILL;
				break;
			case "K":
				oline_flags |= OLINE_CAN_GKILL;
				break;
			case "b":
				oline_flags |= OLINE_CAN_KLINE;
				break;
			case "B":
				oline_flags |= OLINE_CAN_UNKLINE;
				break;
			case "n":
				oline_flags |= OLINE_CAN_LGNOTICE;
				break;
			case "N":
				oline_flags |= OLINE_CAN_GGNOTICE;
				break;
			case "u":
				oline_flags |= OLINE_CAN_UMODEC;
				break;
			case "A":
				oline_flags |= OLINE_IS_ADMIN;
				break;
			case "a":
			case "f":
			case "F":
				break; // All reserved for future use.
			case "s":
				oline_flags |= OLINE_CAN_CHATOPS;
				break;
			case "S":
				oline_flags |= OLINE_CHECK_SYSPASSWD;
				break;
			case "x":
			case "X":
				oline_flags |= OLINE_CAN_DEBUG;
				break;
			case "O":
				oline_flags |= OLINE_IS_GOPER;
				oline_flags |= OLINE_CAN_GSQUITCON;
				oline_flags |= OLINE_CAN_GKILL;
				oline_flags |= OLINE_CAN_GGNOTICE;
				oline_flags |= OLINE_CAN_CHATOPS;
			case "o":
				oline_flags |= OLINE_CAN_REHASH;
				oline_flags |= OLINE_CAN_GLOBOPS;
				oline_flags |= OLINE_CAN_WALLOPS;
				oline_flags |= OLINE_CAN_LOCOPS;
				oline_flags |= OLINE_CAN_LSQUITCON;
				oline_flags |= OLINE_CAN_LKILL;
				oline_flags |= OLINE_CAN_KLINE;
				oline_flags |= OLINE_CAN_UNKLINE;
				oline_flags |= OLINE_CAN_LGNOTICE;
				oline_flags |= OLINE_CAN_UMODEC;
				break;
			default:
				log(LOG_WARNING,"!WARNING Unknown O:Line flag '"
					+ flags[thisflag] + "' in config.");
				break;
		}
	}
	return oline_flags;
}

function umode_notice(bit,ntype,nmessage) {
	log(ntype + ": " + nmessage);
	for (thisuser in Local_Users) {
		var user = Local_Users[thisuser];
		if (user.mode && ((user.mode&bit)==bit))
			user.rawout(":" + servername + " NOTICE " + user.nick
				+ " :*** " + ntype + " -- " + nmessage);
	}

}

function create_ban_mask(str,kline) {
	var tmp_banstr = new Array;
	tmp_banstr[0] = "";
	tmp_banstr[1] = "";
	tmp_banstr[2] = "";
	var bchar_counter = 0;
	var part_counter = 0; // BAN: 0!1@2 KLINE: 0@1
	var regexp="[A-Za-z\{\}\`\^\_\|\\]\\[\\\\0-9\-.*?\~:]";
	var finalstr;
	for (bchar in str) {
		if (str[bchar].match(regexp)) {
			tmp_banstr[part_counter] += str[bchar];
			bchar_counter++;
		} else if ((str[bchar] == "!") && (part_counter == 0) &&
			   !kline) {
			part_counter = 1;
			bchar_counter = 0;
		} else if ((str[bchar] == "@") && (part_counter == 1) &&
			   !kline) {
			part_counter = 2;
			bchar_counter = 0;
		} else if ((str[bchar] == "@") && (part_counter == 0)) {
			if (kline) {
				part_counter = 1;
			} else {
				tmp_banstr[1] = tmp_banstr[0];
				tmp_banstr[0] = "*";
				part_counter = 2;
			}
			bchar_counter = 0;
		}
	}
	if (!tmp_banstr[0] && !tmp_banstr[1] && !tmp_banstr[2])
		return 0;
	if (tmp_banstr[0].match(/[.]/) && !tmp_banstr[1] && !tmp_banstr[2]) {
		if (kline)
			tmp_banstr[1] = tmp_banstr[0];
		else
			tmp_banstr[2] = tmp_banstr[0];
		tmp_banstr[0] = "";
	}
	if (!tmp_banstr[0])
		tmp_banstr[0] = "*";
	if (!tmp_banstr[1])
		tmp_banstr[1] = "*";
	if (!tmp_banstr[2] && !kline)
		tmp_banstr[2] = "*";
	if (kline)
		finalstr = tmp_banstr[0].slice(0,10) + "@" + tmp_banstr[1].slice(0,80);
	else
		finalstr = tmp_banstr[0].slice(0,max_nicklen) + "!"
			+ tmp_banstr[1].slice(0,10) + "@" + tmp_banstr[2].slice(0,80);
        while (finalstr.match(/[*][*]/)) {
                finalstr=finalstr.replace(/[*][*]/g,"*");
        }
	return finalstr;
}

function isklined(kl_str) {
	for(the_kl in KLines) {
		if (KLines[the_kl].hostmask &&
		    wildmatch(kl_str,KLines[the_kl].hostmask))
			return KLines[the_kl];
	}
	return 0;
}

function iszlined(zl_ip) {
	for(the_zl in ZLines) {
		if (ZLines[the_zl].ipmask &&
		    wildmatch(zl_ip,ZLines[the_zl].ipmask))
			return 1;
	}
	return 0;
}

function scan_for_klined_clients() {
	for(thisUser in Local_Users) {
		var theuser=Local_Users[thisUser];
		var kline=isklined(theuser.uprefix + "@" + theuser.hostname);
		if (kline)
			theuser.quit("User has been K:Lined (" + kline.reason + ")");
		if (iszlined(theuser.ip))
			theuser.quit("User has been Z:Lined");
	}
}

function remove_kline(kl_hm) {
	for(the_kl in KLines) {
		if (KLines[the_kl].hostmask &&
		    wildmatch(kl_hm,KLines[the_kl].hostmask)) {
			KLines[the_kl].hostmask = "";
			KLines[the_kl].reason = "";
			KLines[the_kl].type = "";
			return 1;
		}
	}
	return 0; // failure.
}

function connect_to_server(this_cline,the_port) {
	var connect_sock;
	var new_id;

	if (!the_port && this_cline.port)
		the_port = this_cline.port;
	else if (!the_port)
		the_port = default_port; // try a safe default.
	if (js.global.ConnectedSocket != undefined) {
		try {
			connect_sock = new ConnectedSocket(this_cline.host, the_port, {timeout:ob_sock_timeout, bindaddrs:server.interface_ip_addr_list});
		}
		catch(e) {
			connect_sock = new Socket();
		}
	}
	else {
		connect_sock = new Socket();
		connect_sock.bind(0,server.interface_ip_address);
		connect_sock.connect(this_cline.host,the_port,ob_sock_timeout);
	}

	var sendts = true; /* Assume Bahamut */

	for (nl in NLines) {
		var mynl = NLines[nl];
		if ((mynl.flags&NLINE_IS_DREAMFORGE) && 
		    (mynl.servername == this_cline.servername)) {
			sendts = false;
			break;
		}
	}

	if (connect_sock.is_connected) {
		umode_notice(USERMODE_ROUTING,"Routing",
			"Connected!  Sending info...");
		var sendstr = "PASS " + this_cline.password;
		if (sendts)
			sendstr += " :TS";
		connect_sock.send(sendstr + "\r\n");
		connect_sock.send("CAPAB " + server_capab + "\r\n");
		connect_sock.send("SERVER " + servername + " 1 :" + serverdesc +"\r\n");
		new_id = "id" + next_client_id;
		next_client_id++;
		Unregistered[new_id]=new Unregistered_Client(new_id,connect_sock);
		Unregistered[new_id].sendps = false; // Don't do P/S pair again
		Unregistered[new_id].outgoing = true; /* Outgoing Connection */
		Unregistered[new_id].ircclass = this_cline.ircclass;
		YLines[this_cline.ircclass].active++;
		log(LOG_DEBUG, "Class "+this_cline.ircclass+" up to "+YLines[this_cline.ircclass].active+" active out of "+YLines[this_cline.ircclass].maxlinks);
	}
	else {
		umode_notice(USERMODE_ROUTING,"Routing",
			"Failed to connect to " +
			this_cline.servername + " ("+this_cline.host+")");
		connect_sock.close();
	}
	this_cline.lastconnect = time();
}

function wallopers(str) {
	for(thisoper in Local_Users) {
		var oper=Local_Users[thisoper];
		if (oper.mode&USERMODE_OPER)
			oper.rawout(str);
	}
}

function push_nickbuf(oldnick,newnick) {
	NickHistory.unshift(new NickBuf(oldnick,newnick));
	if(NickHistory.length >= nick_buffer)
		NickHistory.pop();
}

function search_nickbuf(bufnick) {
	for (nb=NickHistory.length-1;nb>-1;nb--) {
		if (bufnick.toUpperCase() == NickHistory[nb].oldnick.toUpperCase()) {
			if (!Users[NickHistory[nb].newnick.toUpperCase()])
				bufnick = NickHistory[nb].newnick;
			else
				return Users[NickHistory[nb].newnick.toUpperCase()];
		}
	}
	return 0;
}

var time_config_read;

function read_config_file() {
	/* All of these variables are global. */
	Admin1 = "";
	Admin2 = "";
	Admin3 = "";
	CLines = new Array;
	HLines = new Array;
	ILines = new Array;
	KLines = new Array;
	NLines = new Array;
	OLines = new Array;
	PLines = new Array;
	QLines = new Array;
	ULines = new Array;
	diepass = "";
	restartpass = "";
	YLines = new Array;
	ZLines = new Array;
	/* End of global variables */
	var fname="";
	if (config_filename && config_filename.length) {
		if(config_filename.indexOf('/')>=0 || config_filename.indexOf('\\')>=0)
			fname=config_filename;
		else
			fname=system.ctrl_dir + config_filename;
	} else {
		fname=system.ctrl_dir + "ircd." + system.local_host_name + ".conf";
		if(!file_exists(fname))
			fname=system.ctrl_dir + "ircd." + system.host_name + ".conf";
		if(!file_exists(fname))
			fname=system.ctrl_dir + "ircd.conf";
	}
	log(LOG_INFO,"Reading Config: " + fname);
	if (fname.substr(fname.length-3,3) == "ini")
		read_ini_config(fname);
	else
		read_conf_config(fname);

	time_config_read = time();
}

function read_ini_config(fname) {
	var conf = new File(fname);
	if (conf.open("r")) {
		/* Global Variables */
	}
	conf.close();
}

function read_conf_config(fname) {
	var conf = new File(fname);

	function fancy_split(line) {
		var ret = [];
		var i;
		var s = 0;
		var inb = false;
		var str;

		for (i = 0; i < line.length; i++) {
			if (line[i] == ':') {
				if (inb)
					continue;
				if (i > 0 && line[i-1] == ']')
					str = line.slice(s, i-1);
				else
					str = line.slice(s, i);
				ret.push(str);
				s = i + 1;
			}
			else if (!inb && line[i] == '[' && s == i) {
				inb = true;
				s = i + 1;
			}
			else if (line[i] == ']' && (i+1 == line.length || line[i+1] == ':')) {
				inb = false;
			}
		}
		if (s < line.length) {
			if (i > 0 && line[i-1] == ']')
				str = line.slice(s, i-1);
			else
				str = line.slice(s, i);
			ret.push(str);
		}

		return ret;
	}

	if (conf.open("r")) {
		while (!conf.eof) {
			var conf_line = conf.readln();
			if ((conf_line != null) && conf_line.match("[:]")) {
				var arg = fancy_split(conf_line);
				for(argument in arg) {
					arg[argument]=arg[argument].replace(
						/SYSTEM_HOST_NAME/g,system.host_name);
					arg[argument]=arg[argument].replace(
						/SYSTEM_NAME/g,system.name);
					arg[argument]=arg[argument].replace(
						/SYSTEM_QWKID/g,system.qwk_id.toLowerCase());
					arg[argument]=arg[argument].replace(
						/VERSION_NOTICE/g,system.version_notice);
				}
				switch (conf_line[0].toUpperCase()) {
					case "A":
						if (!arg[3])
							break;
						Admin1 = arg[1];
						Admin2 = arg[2];
						Admin3 = arg[3];
						break;
					case "C":
						if (!arg[5])
							break;
						CLines.push(new CLine(arg[1],arg[2],arg[3],arg[4],
							parseInt(arg[5]) ));
						break;
					case "H":
						if (!arg[3])
							break;
						HLines.push(new HLine(arg[1],arg[3]));
						break;
					case "I":
						if (!arg[5])
							break;
						ILines.push(new ILine(arg[1],arg[2],arg[3],arg[4],
							arg[5]));
						break;
					case "K":
						if (!arg[2])
							break;
						var kline_mask = create_ban_mask(arg[1],true);
						if (!kline_mask) {
							log(LOG_WARNING,"!WARNING Invalid K:Line ("
								+ arg[1] + ")");
							break;
						}
						KLines.push(new KLine(kline_mask,arg[2],"K"));
						break;
					case "M":
						if (!arg[3])
							break;
						servername = arg[1];
						serverdesc = arg[3];
						mline_port = parseInt(arg[4]);
						break;
					case "N":
						if (!arg[5])
							break;
						NLines.push(new NLine(arg[1],arg[2],arg[3],
							parse_nline_flags(arg[4]),arg[5]) );
						break;
					case "O":
						if (!arg[5])
							break;
						OLines.push(new OLine(arg[1],arg[2],arg[3],
							parse_oline_flags(arg[4]),parseInt(arg[5]) ));
						break;
					case "P":
						PLines.push(parseInt(arg[4]));
						break;
					case "Q":
						if (!arg[3])
							break;
						QLines.push(new QLine(arg[3],arg[2]));
						break;
					case "U":
						if (!arg[1])
							break;
						ULines.push(arg[1]);
						break;
					case "X":
						diepass = arg[1];
						restartpass = arg[2];
						break;
					case "Y":
						if (!arg[5])
							break;
						YLines[parseInt(arg[1])] = new YLine(parseInt(arg[2]),
							parseInt(arg[3]),parseInt(arg[4]),parseInt(arg[5]));
						break;
					case "Z":
						if (!arg[2])
							break;
						ZLines.push(new ZLine(arg[1],arg[2]));
						break;
					case "#":
					case ";":
					default:
						break;
				}
			}
		}
		conf.close();
	} else {
		log ("WARNING! No config file found or unable to open."
			+ " Proceeding with defaults.");
	}
	scan_for_klined_clients();
	YLines[0] = new YLine(120,600,1,5050000); // default irc class
}

function create_new_socket(port) {
	var newsock;

	log(LOG_DEBUG,"Creating new socket object on port " + port);
	if (js.global.ListeningSocket != undefined) {
		try {
			newsock = new ListeningSocket(server.interface_ip_addr_list, port, "IRCd");
			log(format("IRC server socket bound to TCP port " + port));
		}
		catch(e) {
			log(LOG_ERR,"!Error " + e + " creating listening socket on port "
				+ port);
			return 0;
		}
	}
	else {
		newsock = new Socket();
		if(!newsock.bind(port,server.interface_ip_address)) {
			log(LOG_ERR,"!Error " + newsock.error + " binding socket to TCP port "
				+ port);
			return 0;
		}
		log(format("%04u ",newsock.descriptor)
			+ "IRC server socket bound to TCP port " + port);
		if(!newsock.listen(5 /* backlog */)) {
			log(LOG_ERR,"!Error " + newsock.error
				+ " setting up socket for listening");
			return 0;
		}
	}
	return newsock;
}

function check_qwk_passwd(qwkid,password) {
	if (!password || !qwkid)
		return 0;
	qwkid = qwkid.toUpperCase();
	var usernum = system.matchuser(qwkid);
	var bbsuser = new User(usernum);
	if ((password.toUpperCase() ==
	     bbsuser.security.password.toUpperCase()) &&
	    (bbsuser.security.restrictions&UFLAG_Q) )
		return 1;
	return 0;
}
function IRCClient_netsplit(ns_reason) {
	if (!ns_reason)
		ns_reason = "net.split.net.split net.split.net.split";
	for (sqclient in Users) {
		if (Users[sqclient] &&
		    (Users[sqclient].servername == this.nick)
		   )
			Users[sqclient].quit(ns_reason,true,true);
	}
	for (sqserver in Servers) {
		if (Servers[sqserver] &&
		    (Servers[sqserver].linkparent == this.nick)
		   )
			Servers[sqserver].quit(ns_reason,true,true);
	}
}

function IRCClient_RMChan(rmchan_obj) {
	if (!rmchan_obj)
		return 0;
	if (rmchan_obj.users[this.id])
		delete rmchan_obj.users[this.id];
	if (this.channels[rmchan_obj.nam.toUpperCase()])
		delete this.channels[rmchan_obj.nam.toUpperCase()];
	delete rmchan_obj.modelist[CHANMODE_OP][this.id];
	delete rmchan_obj.modelist[CHANMODE_VOICE][this.id];
	if (!true_array_len(rmchan_obj.users))
		delete Channels[rmchan_obj.nam.toUpperCase()];
}

//////////////////// Output Helper Functions ////////////////////
function rawout(str) {
	var sendconn;
	var str_end;
	var str_beg;

	if (debug)
		log(format("[RAW->%s]: %s",this.nick,str));

	if (this.local) {
		sendconn = this;
	} else if (!this.local) {
		if ((str[0] == ":") && str[0].match(["!"])) {
			str_end = str.slice(str.indexOf(" ")+1);
			str_beg = str.slice(0,str.indexOf("!"));
			str = str_beg + " " + str_end;
		}
		sendconn = Servers[this.parent.toLowerCase()];
	} else {
		log(LOG_ERR,"!ERROR: No connection to send to?");
		return 0;
	}

	sendconn.sendq.add(str);
}

function originatorout(str,origin) {
	var send_data;
	var sendconn;

	if (debug)
		log(format("[%s->%s]: %s",origin.nick,this.nick,str));

	sendconn = this;
	if(this.local && !this.server) {
		if (origin.server)
			send_data = ":" + origin.nick + " " + str;
		else
			send_data = ":" + origin.nuh + " " + str;
	} else if (this.server) {
		send_data = ":" + origin.nick + " " + str;
	} else if (!this.local) {
		sendconn = Servers[this.parent.toLowerCase()];
		send_data = ":" + origin.nick + " " + str;
	} else {
		log(LOG_ERR,"!ERROR: No connection to send to?");
		return 0;
	}

	sendconn.sendq.add(send_data);
}

function ircout(str) {
	var send_data;
	var sendconn;

	if (debug)
		log(format("[%s->%s]: %s",servername,this.nick,str));

	if(this.local) {
		sendconn = this;
	} else if (this.parent) {
		sendconn = Servers[this.parent.toLowerCase()];
	} else {
		log(LOG_ERR,"!ERROR: No socket to send to?");
		return 0;
	}

	send_data = ":" + servername + " " + str;
	sendconn.sendq.add(send_data);
}

function Queue_Recv(sock) {
	var pos;
	var cmd;
	var str;

	str = sock.recv(65536,0);
	if (str !== null && str.length > 0) {
		this._recv_bytes += str;
		while ((pos = this._recv_bytes.search('\n')) != -1) {
			cmd = this._recv_bytes.substr(0, pos);
			this._recv_bytes = this._recv_bytes.substr(pos+1);
			if (cmd[cmd.length-1] == '\r')
				cmd = cmd.substr(0, cmd.length - 1);
			this.add(cmd);
		}
	}
}

function Queue_Send(sock) {
	var sent;
	var oldnb;

	if (this.queue.length) {
		this._send_bytes += this.queue.join('\r\n')+'\r\n';
		this.queue = [];
	}
	if (this._send_bytes.length) {
		if (new_socket_send === undefined || new_socket_send === false) {
			oldnb = sock.nonblocking;
			sock.nonblocking = false;
		}
		sent = sock.send(this._send_bytes);
		if (new_socket_send === undefined) {
			if (sent === true)
				new_socket_send = false;
			else if (sent === false)
				new_socket_send = false;
			else {
				new_socket_send = true;
				sock.nonblocking = oldnb;
			}
		}
		if (new_socket_send === false) {
			sock.nonblocking = oldnb;
			this._send_bytes = '';
		}
		if (new_socket_send === true) {
			if (sent > 0)
				this._send_bytes = this._send_bytes.substr(sent);
		}
	}
}

function Queue_Prepend(str) {
	this.queue.unshift(str);
}

function Queue_Add(str) {
	this.queue.push(str);
}

function Queue_Del() {
	return this.queue.shift();
}

function IRCClient_server_notice(str) {
	this.ircout("NOTICE " + this.nick + " :" + str);
}

function IRCClient_numeric(num, str) {
	this.ircout(num + " " + this.nick + " " + str);
}

//////////////////// Numeric Functions ////////////////////
function IRCClient_numeric200(dest,next) {
	this.numeric(200, "Link " + VERSION + " " + dest + " " + next);
}

function IRCClient_numeric201(ircclass,server) {
	this.numeric(201, "Try. " + ircclass + " " + server);
}

function IRCClient_numeric202(ircclass,server) {
	this.numeric(202, "H.S. " + ircclass + " " + server);
}

function IRCClient_numeric203(ircclass,ip) {
	this.numeric(203, "???? " + ircclass + " [" + ip + "]");
}

function IRCClient_numeric204(nick) {
	this.numeric(204, "Oper " + nick.ircclass + " " + nick.nick);
}

function IRCClient_numeric205(nick) {
	this.numeric(205, "User " + nick.ircclass + " " + nick.nick);
}

function IRCClient_numeric206(ircclass,sint,cint,server) {
	this.numeric(206, "Serv " + ircclass + " " + sint + "S " + cint
		+ "C *!*@" + server);
}

function IRCClient_numeric208(type,clientname) {
	this.numeric(208, type + " 0 " + clientname);
}

function IRCClient_numeric261(file) {
	this.numeric(261, "File " + file + " " + debug);
}

function IRCClient_numeric321() {
	this.numeric("321", "Channel :Users  Name");
}

function IRCClient_numeric322(chan,show_modes) {
	var channel_name;
	var disp_topic = "";
	var is_onchan = this.channels[chan.nam.toUpperCase()];

	if (show_modes) {
		var chanm = chan.chanmode()
		disp_topic += "[" + chanm.slice(0,chanm.length-1) + "]"
	}

	if ((chan.mode&CHANMODE_PRIVATE) && !(this.mode&USERMODE_OPER) &&
	    !is_onchan ) {
		channel_name = "*";
	} else {
		channel_name = chan.nam;
		if (disp_topic)
			disp_topic += " ";
		disp_topic += chan.topic;
	}
	if (!(chan.mode&CHANMODE_SECRET) || (this.mode&USERMODE_OPER) ||
	    is_onchan )
		this.numeric(322, channel_name + " " + true_array_len(chan.users)
			+ " :" + disp_topic);
}

function IRCClient_numeric331(chan) {
	this.numeric(331, chan.nam + " :No topic is set.");
}

function IRCClient_numeric332(chan) {
	this.numeric(332, chan.nam + " :" + chan.topic);
}

function IRCClient_numeric333(chan) {
	this.numeric(333, chan.nam + " " + chan.topicchangedby + " "
		+ chan.topictime);
}

function IRCClient_numeric351() {
	this.numeric(351, VERSION + " " + servername + " :" + VERSION_STR);
}

function IRCClient_numeric352(user,show_ips_only,chan) {
	var who_mode="";
	var disp;
	var disphost;

	if (!user)
		return 0;

	if (!chan)
		disp = "*";
	else
		disp = chan.nam;

	if (user.away)
		who_mode += "G";
	else
		who_mode += "H";
	if (chan) {
		if (chan.modelist[CHANMODE_OP][user.id])
			who_mode += "@";
		else if (chan.modelist[CHANMODE_VOICE][user.id])
			who_mode += "+";
	}
	if (user.mode&USERMODE_OPER)
		who_mode += "*";

	if (show_ips_only)
		disphost = user.ip;
	else
		disphost = user.hostname;

	this.numeric(352, disp + " " + user.uprefix + " " + disphost + " "
		+ user.servername + " " + user.nick + " " + who_mode
		+ " :" + user.hops + " " + user.realname);
	return 1;
}

function IRCClient_numeric353(chan, str) {
	// = public @ secret * everything else
	if (chan.mode&CHANMODE_SECRET)
		var ctype_str = "@";
	else
		var ctype_str = "=";
	this.numeric("353", ctype_str + " " + chan.nam + " :" + str);
}

function IRCClient_numeric382(str) {
	this.numeric(382, "ircd.conf :" + str);
}

function IRCClient_numeric391() {
	this.numeric(391, servername + " :"
		+ strftime("%A %B %d %Y -- %H:%M %z",time()));
}

function IRCClient_numeric401(str) {
	this.numeric("401", str + " :No such nick/channel.");
}

function IRCClient_numeric402(str) {
	this.numeric(402, str + " :No such server.");
}

function IRCClient_numeric403(str) {
	this.numeric("403", str
		+ " :No such channel or invalid channel designation.");
}

function IRCClient_numeric411(str) {
	this.numeric("411", ":No recipient given. (" + str + ")");
}

function IRCClient_numeric412() {
	this.numeric(412, " :No text to send.");
}

function IRCClient_numeric440(str) {
	this.numeric(440, str + " :Services is currently down, sorry.");
}

function IRCClient_numeric441(str) {
	this.numeric("441", str + " :They aren't on that channel.");
}

function IRCClient_numeric442(str) {
	this.numeric("442", str + " :You're not on that channel.");
}

function IRCClient_numeric445() {
	this.numeric(445, ":SUMMON has been disabled.");
}

function IRCClient_numeric446() {
	this.numeric(446, ":USERS has been disabled.");
}

function IRCClient_numeric451() {
	this.numeric("451", ":You have not registered.");
}

function IRCClient_numeric461(cmd) {
	this.numeric("461", cmd + " :Not enough parameters.");
}

function IRCClient_numeric462() {
	this.numeric("462", ":You may not re-register.");
}

function IRCClient_numeric481() {
	this.numeric("481", ":Permission Denied.  "
		+ "You do not have the correct IRC operator privileges.");
}

function IRCClient_numeric482(tmp_chan_nam) {
	this.numeric("482", tmp_chan_nam + " :You're not a channel operator.");
}

//////////////////// Multi-numeric Display Functions ////////////////////

function IRCClient_lusers() {
	this.numeric(251, ":There are " + num_noninvis_users() + " users and "
		+ num_invis_users() + " invisible on " + true_array_len(Servers)
		+ " servers.");
	this.numeric(252, num_opers() + " :IRC operators online.");
	var unknown_connections = true_array_len(Unregistered);
	if (unknown_connections)
		this.numeric(253, unknown_connections + " :unknown connection(s).");
	this.numeric(254, true_array_len(Channels) + " :channels formed.");
	this.numeric(255, ":I have " + true_array_len(Local_Users)
		+ " clients and " + true_array_len(Local_Servers) + " servers.");
	this.numeric(250, ":Highest connection count: " + hcc_total + " ("
		+ hcc_users + " clients.)");
	this.server_notice(hcc_counter + " clients have connected since "
		+ strftime("%a %b %d %H:%M:%S %Y %Z",server_uptime));
}

function num_noninvis_users() {
	var counter = 0;
	for(myuser in Users) {
		if (!(Users[myuser].mode&USERMODE_INVISIBLE))
			counter++;
	}
	return counter;
}

function num_invis_users() {
	var counter = 0;
	for(myuser in Users) {
		if (Users[myuser].mode&USERMODE_INVISIBLE)
			counter++;
	}
	return counter;
}

function num_opers() {
	var counter = 0;
	for(myuser in Users) {
		if (Users[myuser].mode&USERMODE_OPER)
			counter++;
	}
	return counter;
}

function IRCClient_motd() {
	var motd_file = new File(system.text_dir + "ircmotd.txt");
	if (motd_file.exists==false) {
		this.numeric(422, ":MOTD file missing: " + motd_file.name);
	} else if (motd_file.open("r")==false) {
		this.numeric(424, ":MOTD error " + errno + " opening: "
			+ motd_file.name);
	} else {
		this.numeric(375, ":- " + servername + " Message of the Day -");
		this.numeric(372, ":- " + strftime("%m/%d/%Y %H:%M",motd_file.date));
		while (!motd_file.eof) {
			motd_line = motd_file.readln();
			if (motd_line != null)
				this.numeric(372, ":- " + motd_line);
		}
		motd_file.close();
	}
	this.numeric(376, ":End of /MOTD command.");
}

function IRCClient_names(chan) {
	var Channel_user;
	var numnicks=0;
	var tmp="";
	for(thisChannel_user in chan.users) {
		Channel_user=chan.users[thisChannel_user];
		if (!(Channel_user.mode&USERMODE_INVISIBLE) ||
		     (this.channels[chan.nam.toUpperCase()]) ) {
			if (numnicks)
				tmp += " ";
			if (chan.modelist[CHANMODE_OP][Channel_user.id])
				tmp += "@";
			else if (chan.modelist[CHANMODE_VOICE][Channel_user.id])
				tmp += "+";
			tmp += Channel_user.nick;
			numnicks++;
			if (numnicks >= 59) {
				// dump what we've got, it's too much.
				this.numeric353(chan, tmp);
				numnicks=0;
				tmp="";
			}
		}
	}
	if(numnicks)
		this.numeric353(chan, tmp);
}

// Traverse each channel the user is on and see if target is on any of the
// same channels.
function IRCClient_onchanwith(target) {
	for (c in this.channels) {
		for (i in target.channels) {
			if (c == i)
				return 1; // success
		}
	}
	return 0; // failure.
}

//////////////////// Auxillary Functions ////////////////////

function IRCClient_bcast_to_uchans_unique(str) {
	var already_bcast = new Object;
	for(thisChannel in this.channels) {
		var userchannel=this.channels[thisChannel];
		for (j in userchannel.users) {
			var usr = userchannel.users[j];
			if (!already_bcast[usr.nick] && (usr.id != this.id) && usr.local) {
				usr.originatorout(str,this);
				already_bcast[usr.nick] = true;
			}
		}
	}
}

function IRCClient_bcast_to_list(chan, str, bounce, list_bit) {
	for (thisUser in chan.users) {
		var aUser = chan.users[thisUser];
		if (aUser && ( aUser.id != this.id || (bounce) ) &&
		    chan.modelist[list_bit][aUser.id])
			aUser.originatorout(str,this);
	}
}

function IRCClient_bcast_to_channel(chan, str, bounce) {
	for(thisUser in chan.users) {
		var aUser=chan.users[thisUser];
		if ( ( aUser.id != this.id || (bounce) ) &&
		     aUser.local )
			aUser.originatorout(str,this);
	}
}

function IRCClient_bcast_to_channel_servers(chan, str) {
	var sent_to_servers = new Object;
	for(thisUser in chan.users) {
		var aUser=chan.users[thisUser];
		if (!aUser.local && (this.parent != aUser.parent) &&
		    !sent_to_servers[aUser.parent.toLowerCase()]) {
			aUser.originatorout(str,this);
			sent_to_servers[aUser.parent.toLowerCase()] = true;
		}
	}
}

function IRCClient_check_nickname(newnick,squelch) {
	var qline_nick;
	var checknick;
	var regexp;

	newnick = newnick.slice(0,max_nicklen);
	// If you're trying to NICK to yourself, drop silently.
	if(newnick == this.nick)
		return -1;
	// First, check for valid nick against irclib.
	if(IRC_check_nick(newnick)) {
		if (!squelch)
			this.numeric("432", newnick + " :Foobar'd Nickname.");
		return 0;
	}
	// Second, check for existing nickname.
	checknick = Users[newnick.toUpperCase()];
	if(checknick && (checknick.nick != this.nick) ) {
		if (!squelch)
			this.numeric("433", newnick + " :Nickname is already in use.");
		return 0;
	}
	// Third, match against Q:Lines
	for (ql in QLines) {
		if(wildmatch(newnick, QLines[ql].nick)) {
			if (!squelch)
				this.numeric(432, newnick + " :" + QLines[ql].reason);
			return 0;
		}
	}
	return 1; // if we made it this far, we're good!
}

function IRCClient_do_whois(wi) {
	if (!wi)
		return 0;
	this.numeric(311, wi.nick + " " + wi.uprefix + " " + wi.hostname + " * :"
		+ wi.realname);
	var userchans="";
	for (i in wi.channels) {
		var chan = wi.channels[i];
		if (!(chan.mode&CHANMODE_SECRET||chan.mode&CHANMODE_PRIVATE) ||
		     this.channels[chan.nam.toUpperCase()] || this.mode&USERMODE_OPER) {
			if (userchans)
				userchans += " ";
			if (chan.modelist[CHANMODE_OP][wi.id])
				userchans += "@";
			else if (chan.modelist[CHANMODE_VOICE][wi.id])
				userchans += "+";
			userchans += chan.nam;
		}
	}
	if (userchans)
		this.numeric(319, wi.nick + " :" + userchans);
	if (wi.local)
		this.numeric(312, wi.nick + " " + servername + " :" + serverdesc);
	else {
		wi_server = searchbyserver(wi.servername);
		this.numeric(312, wi.nick + " " + wi_server.nick
		+ " :" + wi_server.info);
	}
	if (wi.mode&USERMODE_OPER)
		this.numeric(313, wi.nick + " :is an IRC operator.");
	if (wi.away)
		this.numeric(301, wi.nick + " :" + wi.away);
	if (wi.local)
		this.numeric(317, wi.nick + " " + (time() - wi.talkidle) + " "
		+ wi.connecttime + " :seconds idle, signon time");
}

function IRCClient_services_msg(svcnick,send_str) {
	var service_server;

	if (!send_str) {
		this.numeric412();
		return 0;
	}
	// First, make sure the nick exists.
	var usr = Users[svcnick.toUpperCase()];
	if (!usr) {
		this.numeric440(svcnick);
		return 0;
	}
	service_server = searchbyserver(usr.servername);
	if (!service_server || !service_server.uline) {
		this.numeric440(svcnick);
		return 0;
	}
	this.do_msg(svcnick,"PRIVMSG",send_str);
}

function IRCClient_global(target,type_str,send_str) {
	if (!target.match("[.]")) {
		this.numeric(413,target + " :No top-level domain specified.");
		return 0;
	}
	var domain_part = target.split('.');
	if (domain_part[domain_part.length - 1].match("[?*]")) {
		this.numeric(414,target + " :Wildcard found in top-level domain.");
		return 0;
	}
	var global_mask = target.slice(1);
	var global_str = type_str + " " + target + " :" + send_str;
	for(globClient in Local_Users) {
		var Client = Local_Users[globClient];
		if (target[0] == "#")
			var global_match = Client.hostname;
		else // assume $
			var global_match = Client.servername;
		if (wildmatch(global_match,global_mask))
			Client.originatorout(global_str,this);
	}
	global_str = ":" + this.nick + " " + global_str;
	if(this.local && this.parent) /* Incoming from a local server */
		Servers[this.parent.toLowerCase()].bcast_to_servers_raw(global_str);
	else if (this.flags&OLINE_CAN_GGNOTICE) /* From a local oper */
		server_bcast_to_servers(global_str);
	return 1;
}

function IRCClient_globops(str) {
	var globops_bits = 0;
	globops_bits |= USERMODE_OPER;
	globops_bits |= USERMODE_GLOBOPS;
	umode_notice(globops_bits,"Global","from " + this.nick +": " + str);
	if (this.parent)
		Servers[this.parent.toLowerCase()].bcast_to_servers_raw(":" + this.nick
			+ " GLOBOPS :" + str);
	else
		server_bcast_to_servers(":" + this.nick + " GLOBOPS :" + str);
}

function IRCClient_do_msg(target,type_str,send_str) {
	if ( (target[0] == "$") && (this.mode&USERMODE_OPER) &&
	     ( (this.flags&OLINE_CAN_LGNOTICE) || !this.local)
	   )
		return this.global(target,type_str,send_str);

	var send_to_list = -1;
	if (target[0] == "@" && ( (target[1] == "#") || target[1] == "&") ) {
		send_to_list = CHANMODE_OP;
		target = target.slice(1);
	} else if (target[0]=="+" && ((target[1] == "#")|| target[1] == "&")) {
		send_to_list = CHANMODE_VOICE;
		target = target.slice(1);
	}
		
	if ((target[0] == "#") || (target[0] == "&")) {
		var chan = Channels[target.toUpperCase()];
		if (!chan) {
			// check to see if it's a #*hostmask* oper message
			if ( (target[0] == "#") && (this.mode&USERMODE_OPER) &&
			     ( (this.flags&OLINE_CAN_LGNOTICE) || !this.local )
			   ) {
				return this.global(target,type_str,send_str);
			} else {
				this.numeric401(target);
				return 0;
			}
		}
		if ((chan.mode&CHANMODE_NOOUTSIDE)
			&& !this.channels[chan.nam.toUpperCase()]) {
			this.numeric(404, chan.nam + " :Cannot send to channel "
				+ "(+n: no outside messages)");
			return 0;
		}
		if ( (chan.mode&CHANMODE_MODERATED) &&
		     !chan.modelist[CHANMODE_VOICE][this.id] &&
		     !chan.modelist[CHANMODE_OP][this.id] ) {
			this.numeric(404, chan.nam + " :Cannot send to channel "
				+ "(+m: moderated)");
			return 0;
		}
		if (chan.isbanned(this.nuh) &&
		   !chan.modelist[CHANMODE_VOICE][this.id] &&
		   !chan.modelist[CHANMODE_OP][this.id] ) {
			this.numeric(404, chan.nam + " :Cannot send to channel "
				+ "(+b: you're banned!)");
			return 0;
		}
		if(send_to_list == -1) {
			var str = type_str +" "+ chan.nam +" :"+ send_str;
			this.bcast_to_channel(chan, str, false);
			this.bcast_to_channel_servers(chan, str);
		} else {
			var prefix_chr;
			if (send_to_list == CHANMODE_OP)
				prefix_chr="@";
			else if (send_to_list == CHANMODE_VOICE)
				prefix_chr="+";
			var str = type_str +" " + prefix_chr + chan.nam + " :"+ send_str;
			this.bcast_to_list(chan, str, false, send_to_list);
			this.bcast_to_channel_servers(chan, str);
		}
	} else {
		if (target.match("[@]")) {
			var msg_arg = target.split('@');
			var real_target = msg_arg[0];
			var target_server = searchbyserver(msg_arg[1]);
			if (!target_server) {
				this.numeric401(target);
				return 0;
			}
			if (target_server == -1) {
				this.numeric(407, target
					+ " :Duplicate recipients, no message delivered.");
				return 0;
			}
			target = msg_arg[0] + "@" + msg_arg[1];
		} else {
			var real_target = target;
		}
		var target_socket = Users[real_target.toUpperCase()];
		if (target_socket) {
			if (target_server &&
			    (target_server.parent != target_socket.parent)) {
				this.numeric401(target);
				return 0;
			}
			if (target_server && 
			    (target_server.id == target_socket.parent) )
				target = real_target;
			if (target_socket.issilenced(this.nuh))
				return 0;	/* On SILENCE list.  Silently ignore. */
			var str = type_str + " " + target + " :" + send_str;
			target_socket.originatorout(str,this);
			if (target_socket.away && (type_str == "PRIVMSG") &&
			    !this.server && target_socket.local)
				this.numeric(301, target_socket.nick + " :"
					+ target_socket.away);
		} else {
			this.numeric401(target);
			return 0;
		}
	}
	return 1;
}

function IRCClient_do_admin() {
	umode_notice(USERMODE_SPY,"ADMIN requested by " + this.nick + " (" +
		this.uprefix + "@" + this.hostname + ") [" + this.servername +
		"]");
	if (Admin1 && Admin2 && Admin3) {
		this.numeric(256, ":Administrative info about " + servername);
		this.numeric(257, ":" + Admin1);
		this.numeric(258, ":" + Admin2);
		this.numeric(259, ":" + Admin3);
	} else {
		this.numeric(423, servername
			+ " :No administrative information available.");
	}
}

function IRCClient_do_info() {
	umode_notice(USERMODE_SPY,"Spy","INFO requested by " + this.nick +
		" (" + this.uprefix + "@" + this.hostname + ") [" +
		this.servername + "]");
	this.numeric(371, ":--=-=-=-=-=-=-=-=-=*[ The Synchronet IRCd v1.3a ]*=-=-=-=-=-=-=-=-=--");
	this.numeric(371, ":  IRCd Copyright 2003-2009 by Randolph E. Sommerfeld <cyan@rrx.ca>");
	this.numeric(371, ":" + system.version_notice + " " + system.copyright + ".");
	this.numeric(371, ":--=-=-=-=-=-=-=-=-( A big thanks to the following )-=-=-=-=-=-=-=-=--");
	this.numeric(371, ":DigitalMan (Rob Swindell): Resident coder god, various hacking all");
	this.numeric(371, ":   around the IRCd, countless helpful suggestions and tips, and");
	this.numeric(371, ":   tons of additions to the Synchronet JS API that made this possible.");
	this.numeric(371, ":Deuce (Stephen Hurd): Resident Perl guru and ex-Saskatchewan zealot.");
	this.numeric(371, ":   Originally converted the IRCd to be object-oriented, various small");
	this.numeric(371, ":   hacks, and lots of guidance.");
	this.numeric(371, ":Thanks to the DALnet Bahamut team for their help from time to time.");
	this.numeric(371, ":Greets to: Arrak, ElvishMerchant, Foobar, Grimp, Kufat,");
	this.numeric(371, ":   Psyko, Samael, Shaun, Torke, and all the #square oldbies.");
	this.numeric(371, ":--=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--");
	this.numeric(371, ":Synchronet " + system.full_version);
	this.numeric(371, ":Compiled with " + system.compiled_with + " at " + system.compiled_when);
	this.numeric(371, ":Running on " + system.os_version);
	this.numeric(371, ":Utilizing socket library: " + system.socket_lib);
	this.numeric(371, ":Javascript library: " + system.js_version);
	this.numeric(371, ":This BBS has been up since " + system.timestr(system.uptime));
	this.numeric(371, ": -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -");
	if (server.version_detail!=undefined) {
		this.numeric(371, ":This IRCd was executed via:");
		this.numeric(371, ":" + server.version_detail);
	}
	this.numeric(371, ":IRCd CVS revisions:")
	this.numeric(371, ":Main(" + MAIN_REVISION + ") User(" + USER_REVISION + ") Channel(" + CHANNEL_REVISION + ") Server(" + SERVER_REVISION + ") Unreg(" + UNREG_REVISION + ")");
	this.numeric(371, ":IRClib Version: " + IRCLIB_VERSION);
	this.numeric(371, ":--=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--");
	this.numeric(371, ":This program is distributed under the terms of the GNU General Public");
	this.numeric(371, ":License, version 2.  http://www.gnu.org/licenses/gpl.txt");
	this.numeric(371, ":--=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--");
	this.numeric(374, ":End of /INFO list.");
}

function IRCClient_do_stats(statschar) {
	switch(statschar[0]) {
		case "C":
		case "c":
			var cline_port;
			for (cline in CLines) {
				if(CLines[cline].port)
					cline_port = CLines[cline].port;
				else
					cline_port = "*";
				this.numeric(213,"C " + CLines[cline].host + " * "
					+ CLines[cline].servername + " " + cline_port + " "
					+ CLines[cline].ircclass);
				if (NLines[cline])
					this.numeric(214,"N " + NLines[cline].host + " * "
						+ NLines[cline].servername + " " + NLines[cline].flags
						+ " " + NLines[cline].ircclass);
			}
			break;  
		case "H":
		case "h":
			for (hl in HLines) {
				this.numeric(244,"H " + HLines[hl].allowedmask + " * "
					+ HLines[hl].servername);
			}
			break;
		case "I":
		case "i":
			var my_port;
			for (iline in ILines) {
				if (!ILines[iline].port)
					my_port = "*";
				else
					my_port = ILines[iline].port;
				this.numeric(215,"I " + ILines[iline].ipmask + " * "
					+ ILines[iline].hostmask + " " + my_port + " "
					+ ILines[iline].ircclass);
			}
			break;
		case "K":
		case "k":
			for (kline in KLines) {
				if (KLines[kline].hostmask) {
					this.numeric(216, KLines[kline].type + " "
						+ KLines[kline].hostmask + " * * :"
						+ KLines[kline].reason);
				}
			}
			break;
		case "L": /* FIXME */
			this.numeric(241,"L <hostmask> * <servername> <maxdepth>");
			break;
		case "l": /* FIXME */
			this.numeric(211,"<linkname> <sendq> <sentmessages> <sentbytes> <receivedmessages> <receivedbytes> <timeopen>");
			break;
		case "M":
		case "m":
			for (c in Profile) {
				var sm = Profile[c];
				this.numeric(212, c + " " + sm.ticks + " " + sm.executions);
			}
			break;          
		case "O":       
		case "o":
			for (oline in OLines) {
				this.numeric(243, "O " + OLines[oline].hostmask + " * "
					+ OLines[oline].nick);
			}
			break;
		case "U":
			for (uline in ULines) {
				this.numeric(246, "U " + ULines[uline] + " * * 0 -1");                  
			}       
			break;
		case "u":
			var this_uptime=time() - server_uptime;
			var updays=Math.floor(this_uptime / 86400);
			if (updays)
				this_uptime %= 86400;
			var uphours=Math.floor(this_uptime/(60*60));
			var upmins=(Math.floor(this_uptime/60))%60;
			var upsec=this_uptime%60;
			var str = format("Server Up %u days, %u:%02u:%02u",
				updays,uphours,upmins,upsec);
			this.numeric(242,":" + str);
			break;
		case "Y":
		case "y":
			var yl;
			for (thisYL in YLines) {
				yl = YLines[thisYL];
				this.numeric(218,"Y " + thisYL + " " + yl.pingfreq + " "
					+ yl.connfreq + " " + yl.maxlinks + " " + yl.sendq);
			}
			break;
		default:
			break;
	}
	this.numeric(219, statschar[0] + " :End of /STATS Command.");
}

function IRCClient_do_users() {
	var usersshown;
	var u;

	this.numeric(392,':UserID                    Terminal  Host');
	usersshown=0;
	for(node in system.node_list) {
		if(system.node_list[node].status == NODE_INUSE) {
			u=new User(system.node_list[node].useron);
			this.numeric(393,format(':%-25s %-9s %-30s',u.alias,'Node'+node,
				u.host_name));
			usersshown++;
		}
	}
	if(usersshown) {
		this.numeric(394,':End of users');
	} else {
		this.numeric(395,':Nobody logged in');
	}
}

function IRCClient_do_summon(summon_user) {
	var usernum;
	var isonline;

	// Check if exists.
	usernum = system.matchuser(summon_user);
	if(!usernum)
		this.numeric(444,":No such user.");
	else {
		// Check if logged in
		isonline = 0;
		for(node in system.node_list) {
			if( (system.node_list[node].status == NODE_INUSE)
				&& (system.node_list[node].useron == usernum) ) {
				isonline = 1;
				break;
			}
		}
		if(!isonline) {
			this.numeric(444,":User not logged in.");
		} else {
			system.put_telegram(usernum, "" + this.nick
				+ " is summoning you to IRC chat.\r\n");
			this.numeric(342, summon_user + " :Summoning user to IRC");
		}
	}
}

function IRCClient_do_links(mask) {
	if (!mask)
		mask = "*";
	umode_notice(USERMODE_SPY,"Spy","LINKS " + mask + " requested by " +
		this.nick + " (" + this.uprefix + "@" + this.hostname + ") [" +
		this.servername + "]");
	for(thisServer in Servers) {
		var Server=Servers[thisServer];
		if (wildmatch(Server.nick,mask)) {
			this.numeric(364, Server.nick + " " + Server.linkparent + " :"
				+ Server.hops + " " + Server.info);
		}
	}
	if (wildmatch(servername,mask))
		this.numeric(364, servername + " " + servername + " :0 " + serverdesc);
	this.numeric(365, mask + " :End of /LINKS list.");
}

// Don't hunt for servers based on nicknames, as TRACE is more explicit.
function IRCClient_do_trace(target) {
	var server = searchbyserver(target);

	if (server == -1) { // we hunted ourselves
		// FIXME: What do these numbers mean? O_o?
		this.numeric206("30","1","0",servername);
		this.trace_all_opers();
	} else if (server) {
		server.rawout(":" + this.nick + " TRACE " + target);
		this.numeric200(target,server.nick);
		return 0;
	} else {
		// Okay, we've probably got a user.
		var nick = Users[target.toUpperCase()];
		if (!nick) {
			this.numeric402(target);
			return 0;
		} else if (nick.local) {
			if (nick.mode&USERMODE_OPER)
				this.numeric204(nick);
			else
				this.numeric205(nick);
		} else {
			nick.rawout(":" + this.nick + " TRACE " + target);
			this.numeric200(target,Servers[nick.parent.toLowerCase()].nick);
		}
	}
	this.numeric(262, target + " :End of /TRACE.");
}

function IRCClient_trace_all_opers() {
	for(thisoper in Local_Users) {
		var oper=Local_Users[thisoper];
		if (oper.mode&USERMODE_OPER)
			this.numeric204(oper);
	}
}

function IRCClient_do_connect(con_server,con_port) {
	var con_cline = "";
	for (ccl in CLines) {
		if (wildmatch(CLines[ccl].servername,con_server) ||
		    wildmatch(CLines[ccl].host,con_server) ) {
			con_cline = CLines[ccl];
			break;
		}
	}
	if (!con_cline) {
		this.numeric402(con_server);
		return 0;
	}
	if (!con_port && con_cline.port)
		con_port = con_cline.port;
	if (!con_port && !con_cline.port)
		con_port = String(default_port);
	if (!con_port.match(/^[0-9]+$/)) {
		this.server_notice("Invalid port: " + con_port);
		return 0;
	}
	var msg = " CONNECT " + con_cline.servername + " " + con_port +
		" from " + this.nick + "[" + this.uprefix + "@" +
		this.hostname + "]";
	var con_type = "Local";
	if (this.parent) {
		con_type = "Remote";
		server_bcast_to_servers("GNOTICE :Remote" + msg);
	}
	umode_notice(USERMODE_ROUTING,"Routing","from "+servername+": " + 
		con_type + msg);
	connect_to_server(con_cline,con_port);
	return 1;
}

function IRCClient_do_basic_who(whomask) {
	var eow = "*";

	var regexp = "^[0]{1,}$";
	if (whomask.match(regexp))
		whomask = "*";

	if ((whomask[0] == "#") || (whomask[0] == "&")) {
		var chan = Channels[whomask.toUpperCase()];
		if (chan && ( ( !(chan.mode&CHANMODE_SECRET) &&
		    !(chan.mode&CHANMODE_PRIVATE) ) ||
		    this.channels[chan.nam.toUpperCase()] || (this.mode&USERMODE_OPER))
		   ) {
			for(i in chan.users) {
				var usr = chan.users[i];
				if (!(usr.mode&USERMODE_INVISIBLE) ||
				    (this.mode&USERMODE_OPER) ||
				    this.onchanwith(usr) ) {
					var chkwho = this.numeric352(usr,false,chan);
					if (!chkwho) {
						umode_notice(USERMODE_OPER,"Notice",
							"WHO returned 0 for user: " + usr.nick + " (A)");
					}
				}
			}
			eow = chan.nam;
		}
	} else {
		for (i in Users) {
			var usr = Users[i];
			if (usr.match_who_mask(whomask) &&
			    (!(usr.mode&USERMODE_INVISIBLE) ||
			     (this.mode&USERMODE_OPER) ||
			     this.onchanwith(usr) ) ) {
				var chkwho = this.numeric352(usr);
				if (!chkwho) {
					umode_notice(USERMODE_OPER,"Notice",
						"WHO returned 0 for user: " + usr.nick + " (B)");
				}
			}
		}
		eow = whomask;
	}
	this.numeric(315, eow + " :End of /WHO list. (Basic)");
}

function IRCClient_do_complex_who(cmd) {
	var who = new Who();
	var tmp;
	var eow = "*";
	var add = true;	// assume the user is doing + by default
	var arg = 1;
	var whomask = "";
	var chan;

	// RFC1459 Compatibility.  "WHO <mask> o"  Only do it if we find a
	// wildcard, otherwise assume we're doing a complex WHO with 'modes'
	if (cmd[2] && ( (cmd[1].match(/[*]/)) || cmd[1].match(/[?]/)) ||
	    (cmd[1].match(/[0]/)) && !cmd[3] && (cmd[2].toLowerCase() == "o")) {
		tmp = cmd[1];
		cmd[1] = cmd[2];
		cmd[2] = tmp;
	}

	for (myflag in cmd[1]) {
		switch (cmd[1][myflag]) {
			case "+":
				if (!add)
					add = true;
				break;
			case "-":
				if (add)
					add = false;
				break;
			case "a":
				who.tweak_mode(WHO_AWAY,add);
				break;
			case "c":
				arg++;
				if (cmd[arg]) {
					who.tweak_mode(WHO_CHANNEL,add);
					who.Channel = cmd[arg];
				}
				break;
			case "g":
				arg++;
				if (cmd[arg]) {
					who.tweak_mode(WHO_REALNAME,add);
					who.RealName = cmd[arg];
				}
				break;
			case "h":
				arg++;
				if (cmd[arg]) {
					who.tweak_mode(WHO_HOST,add);
					who.Host = cmd[arg];
				}
				break;
			case "i":
				arg++;
				if (cmd[arg]) {
					who.tweak_mode(WHO_IP,add);
					who.IP = cmd[arg];
				}
				break;
			case "l":
				arg++;
				if (cmd[arg]) {
					who.tweak_mode(WHO_CLASS,add);
					who.Class = parseInt(cmd[arg]);
				}
			case "m": // we never set -m
				arg++;
				if (cmd[arg]) {
					who.tweak_mode(WHO_UMODE,true);
					if (!add) {
						var tmp_umode = "";
						if ((cmd[arg][0] != "+") ||
						    (cmd[arg][0] != "-") )
							tmp_umode += "+";
						tmp_umode += cmd[arg].replace(/[-]/g," ");
						tmp_umode = tmp_umode.replace(/[+]/g,"-");
						who.UMode = tmp_umode.replace(/[ ]/g,"+");
					} else {
						who.UMode = cmd[arg];
					}
				}
				break;
			case "n":
				arg++;
				if (cmd[arg]) {
					who.Nick = cmd[arg];
					who.tweak_mode(WHO_NICK,add);
				}
				break;
			case "o":
				who.tweak_mode(WHO_OPER,add);
				break;
			case "s":
				arg++;
				if (cmd[arg]) {
					who.Server = cmd[arg];
					who.tweak_mode(WHO_SERVER,add);
				}
				break;
			case "t":
				arg++;
				if (cmd[arg]) {
					who.Time = parseInt(cmd[arg]);
					who.tweak_mode(WHO_TIME,add);
				}
				break;
			case "u":
				arg++;
				if (cmd[arg]) {
					who.User = cmd[arg];
					who.tweak_mode(WHO_USER,add);
				}
				break;
			case "C":
				who.tweak_mode(WHO_FIRST_CHANNEL,add);
				break;
			case "M":
				who.tweak_mode(WHO_MEMBER_CHANNEL,add);
				break;
			case "I":
				who.tweak_mode(WHO_SHOW_IPS_ONLY,add);
				break;
			default:
				break;
		}
	}

	// Check to see if the user passed a generic mask to us for processing.
	arg++;
	if (cmd[arg])
		whomask = cmd[arg];

	var regexp = "^[0]{1,}$";
	if (whomask.match(regexp))
		whomask = "*";

	// allow +c/-c to override.
	if (!who.Channel && ((whomask[0] == "#") || (whomask[0] == "&")))
		who.Channel = whomask;

	// Strip off any @ or + in front of a channel and set the flags.
	var sf_op = false;
	var sf_voice = false;
	var sf_done = false;
	var tmp_wc = who.Channel;
	for (cc in tmp_wc) {
		switch(tmp_wc[cc]) {
			case "@":
				sf_op = true;
				who.Channel = who.Channel.slice(1);
				break;
			case "+":
				sf_voice = true;
				who.Channel = who.Channel.slice(1);
				break;
			default: // assume we're done
				sf_done = true;
				break;
		}
		if (sf_done)
			break;
	}
	delete tmp_wc; // don't need this anymore.

	// Now we traverse everything and apply the criteria the user passed.
	var who_count = 0;
	for (who_client in Users) {
		var wc = Users[who_client];
		var flag_M = this.onchanwith(wc);

		// Don't even bother if the target is +i and the
		// user isn't an oper or on a channel with the target.
		if ( (wc.mode&USERMODE_INVISIBLE) &&
		     !(this.mode&USERMODE_OPER) &&
		     !flag_M)
			continue;

		if ((who.add_flags&WHO_AWAY) && !wc.away)
			continue;
		else if ((who.del_flags&WHO_AWAY) && wc.away)
			continue;
		if (who.add_flags&WHO_CHANNEL) {
			if (!wc.channels[who.Channel.toUpperCase()])
				continue;
			if (sf_op && Channels[who.Channel.toUpperCase()]&&
			    !Channels[who.Channel.toUpperCase()].modelist
			    [CHANMODE_OP][wc.id])
				continue;
			if(sf_voice&&Channels[who.Channel.toUpperCase()]&&
			    !Channels[who.Channel.toUpperCase()].modelist
			    [CHANMODE_VOICE][wc.id])
				continue;
		} else if (who.del_flags&WHO_CHANNEL) {	
			if (wc.channels[who.Channel.toUpperCase()])
				continue;
			if (sf_op && Channels[who.Channel.toUpperCase()]&&
			    Channels[who.Channel.toUpperCase()].modelist
			    [CHANMODE_OP][wc.id])
				continue;
			if(sf_voice&&Channels[who.Channel.toUpperCase()]&&
			    Channels[who.Channel.toUpperCase()].modelist
			    [CHANMODE_VOICE][wc.id])
				continue;
		}
		if ((who.add_flags&WHO_REALNAME) &&
		    !wildmatch(wc.realname,who.RealName))
			continue;
		else if ((who.del_flags&WHO_REALNAME) &&
		    wildmatch(wc.realname,who.RealName))
			continue;
		if ((who.add_flags&WHO_HOST) &&
		    !wildmatch(wc.hostname,who.Host))
			continue;
		else if ((who.del_flags&WHO_HOST) &&
		    wildmatch(wc.hostname,who.Host))
			continue;
		if ((who.add_flags&WHO_IP) &&
		    !wildmatch(wc.ip,who.IP))
			continue;
		else if ((who.del_flags&WHO_IP) &&
		    wildmatch(wc.ip,who.IP))
			continue;
		if (who.add_flags&WHO_UMODE) { // no -m
			var sic = false;
			var madd = true;
			for (mm in who.UMode) {
				switch(who.UMode[mm]) {
					case "+":
						if (!madd)
							madd = true;
						break;
					case "-":
						if (madd)
							madd = false;
						break;
					case "o":
					case "i":
					case "w":
					case "b":
					case "g":
					case "s":
					case "c":
					case "r":
					case "k":
					case "f":
					case "y":
					case "d":
					case "n":
					case "h":
					case "F":
						if (
						   (!madd && (wc.mode&
						   USERMODE_CHAR
							[who.UMode[mm]])
						   )
						||
						   (madd && !(wc.mode&
						   USERMODE_CHAR
							[who.UMode[mm]])
						   ) )
							sic = true;
						break;
					default:
						break;
				}
				if (sic)
					break;
			}
			if (sic)
				continue;
		}
		if ((who.add_flags&WHO_NICK) &&
		    !wildmatch(wc.nick,who.Nick))
			continue;
		else if ((who.del_flags&WHO_NICK) &&
		    wildmatch(wc.nick,who.Nick))
			continue;
		if ((who.add_flags&WHO_OPER) &&
		    !(wc.mode&USERMODE_OPER))
			continue;
		else if ((who.del_flags&WHO_OPER) &&
		    (wc.mode&USERMODE_OPER))
			continue;
		if ((who.add_flags&WHO_SERVER) &&
		    !wildmatch(wc.servername,who.Server))
			continue;
		else if ((who.del_flags&WHO_SERVER) &&
		    wildmatch(wc.servername,who.Server))
			continue;
		if ((who.add_flags&WHO_USER) &&
		    !wildmatch(wc.uprefix,who.User))
			continue;
		else if ((who.del_flags&WHO_USER) &&
		    wildmatch(wc.uprefix,who.User))
			continue;
		if ((who.add_flags&WHO_MEMBER_CHANNEL) && !flag_M)
			continue;
		else if ((who.del_flags&WHO_MEMBER_CHANNEL) && flag_M)
			continue;
		if ((who.add_flags&WHO_TIME) &&
		    ((time() - wc.connecttime) < who.Time) )
			continue;
		else if ((who.del_flags&WHO_TIME) &&
		    ((time() - wc.connecttime) > who.Time) )
			continue;
		if ((who.add_flags&WHO_CLASS) &&
		    (wc.ircclass != who.Class))
			continue;
		else if ((who.del_flags&WHO_CLASS) &&
		    (wc.ircclass == who.Class))
			continue;

		if (whomask && !wc.match_who_mask(whomask))
			continue;       

		chan = "";
		if ((who.add_flags&WHO_FIRST_CHANNEL) && !who.Channel) {
			for (x in wc.channels) {
				if (!(Channels[x].mode&CHANMODE_SECRET
						|| Channels[x].mode&CHANMODE_PRIVATE)
					|| this.channels[x] || this.mode&USERMODE_OPER)
				{
					chan = Channels[x].nam;
					break;
				}
			}
		} else if (who.Channel) {
			chan = who.Channel;
		}

		var show_ips_only;
		if (who.add_flags&WHO_SHOW_IPS_ONLY)
			show_ips_only = true;
		else
			show_ips_only = false;

		// If we made it this far, we're good.
		if (chan && Channels[chan.toUpperCase()]) {
			var chkwho = this.numeric352(wc,show_ips_only,
				Channels[chan.toUpperCase()]);
			if (!chkwho) {
				umode_notice(USERMODE_OPER,"Notice",
					"WHO returned 0 for user: " + wc.nick + " (C)");
			}
		} else {
			var chkwho = this.numeric352(wc,show_ips_only);
			if (!chkwho) {
				umode_notice(USERMODE_OPER,"Notice",
					"WHO returned 0 for user: " + wc.nick + " (D)");
			}
		}
		who_count++;

		if (!(this.mode&USERMODE_OPER) && (who_count >= max_who))
			break;
	}

	if (who.Channel)
		eow = who.Channel;
	else if (cmd[2])
		eow = cmd[2];

	this.numeric(315, eow + " :End of /WHO list. (Complex)");
}

// Object which stores WHO bits and arguments.
function Who() {
	this.add_flags = 0;
	this.del_flags = 0;
	this.tweak_mode = Who_tweak_mode;
	this.Channel = "";
	this.RealName = "";
	this.Host = "";
	this.IP = "";
	this.UMode = "";
	this.Nick = "";
	this.Server = "";
	this.User = "";
	this.Time = 0;
	this.Class = 0;
}

function Who_tweak_mode(bit,add) {
	if (add) {
		this.add_flags |= bit;
		this.del_flags &= ~bit;
	} else {
		this.add_flags &= ~bit;
		this.del_flags |= bit;
	}
}

// Take a generic mask in and try to figure out if we're matching a channel,
// mask, nick, or something else.  Return 1 if the user sent to us matches it
// in some fashion.
function IRCClient_match_who_mask(mask) {
	if ((mask[0] == "#") || (mask[0] == "&")) { // channel
		if (Channels[mask.toUpperCase()])
			return 1;
		else
			return 0; // channel doesn't exist.
	} else if (mask.match(/[!]/)) { // nick!user@host
		if ( wildmatch(this.nick,mask.split("!")[0])
		     && wildmatch(this.uprefix,
				mask.slice(mask.indexOf("!")+1).split("@")[0])
		     && wildmatch(this.hostname,mask.slice(mask.indexOf("@")+1)) )
			return 1;
	} else if (mask.match(/[@]/)) { // user@host
		if ( wildmatch(this.uprefix,mask.split("@")[0]) &&
		     wildmatch(this.hostname,mask.split("@")[1]) )
			return 1;
	} else if (mask.match(/[.]/)) { // host only
		if ( wildmatch(this.hostname,mask) )
			return 1;
	} else { // must be a nick?
		if ( wildmatch(this.nick,mask) )
			return 1;
	}
	return 0;
}

function IRCClient_do_who_usage() {
	this.numeric(334,":/WHO [+|-][acghilmnostuCIM] <args> <mask>");
	this.numeric(334,":The modes as above work exactly like channel modes.");
	this.numeric(334,":<mask> may be '*' or in nick!user@host notation.");
	this.numeric(334,":i.e. '/WHO +a *.ca' would match all away users from *.ca");
	this.numeric(334,":No args/mask matches to everything by default.");
	this.numeric(334,":a       : User is away.");
	this.numeric(334,":c <chan>: User is on <@+><#channel>, no wildcards. Can check +o/+v.");
	this.numeric(334,":g <rnam>: Check against realname field, wildcards allowed.");
	this.numeric(334,":h <host>: Check user's hostname, wildcards allowed.");
	this.numeric(334,":i <ip>  : Check against IP address, wildcards allowed.");
	this.numeric(334,":l <clas>: User is a member of <clas> irc class as defined on a Y:Line.");
	this.numeric(334,":m <umde>: User has <umodes> set, -+ allowed.");
	this.numeric(334,":n <nick>: User's nickname matches <nick>, wildcards allowed.");
	this.numeric(334,":o       : User is an IRC Operator.");
	this.numeric(334,":s <srvr>: User is on <server>, wildcards allowed.");
	this.numeric(334,":t <time>: User has been on for more than (+) or less than (-) <time> secs.");
	this.numeric(334,":u <user>: User's username field matches, wildcards allowed.");
	this.numeric(334,":C       : Only display first channel that the user matches.");
	this.numeric(334,":I       : Only return IP addresses (as opposed to hostnames.)");
	this.numeric(334,":M       : Only check against channels you're a member of.");
	this.numeric(315,"? :End of /WHO list. (Usage)");
}

function IRCClient_do_basic_list(mask) {
	this.numeric321();
	// Only allow commas if we're not doing wildcards, otherwise strip
	// off anything past the first comma and pass to the generic parser
	// to see if it can make heads or tails out of it.
	if (mask.match(/[,]/)) {
		if (mask.match(/[*?]/)) {
			mask = mask.slice(0,mask.indexOf(","))
		} else { // parse it out, but junk anything that's not a chan
			var my_split = mask.split(",");
			for (myChan in my_split) {
				if (Channels[my_split[myChan].toUpperCase()])
					this.numeric322(Channels[my_split[myChan].toUpperCase()]);
			}
			// our adventure ends here.
			this.numeric(323, ":End of /LIST. (Basic: Comma-list)");
			return;
		}
	}
	for (chan in Channels) {
		if (Channels[chan] && Channels[chan].match_list_mask(mask))
			this.numeric322(Channels[chan]);
	}
	this.numeric(323, ":End of /LIST. (Basic)");
	return;
}

// So, the user wants to go the hard way...
function IRCClient_do_complex_list(cmd) {
	var add = true;
	var arg = 1;
	var list = new List();
	var listmask;
	var listmask_items;

	this.numeric321();

	for (lc in cmd[1]) {
		switch(cmd[1][lc]) {
			case "+":
				if (!add)
					add = true;
				break;
			case "-":
				if (add)
					add = false;
				break;
			case "a":
				arg++;
				if (cmd[arg]) {
					list.tweak_mode(LIST_CHANMASK,add);
					list.Mask = cmd[arg];
				}
				break;
			case "c":
				arg++;
				if (cmd[arg]) {
					list.tweak_mode(LIST_CREATED,add);
					list.Created = parseInt(cmd[arg])*60;
				}
				break;
			case "m": // we never set -m, inverse.
				arg++;
				if (cmd[arg]) {
					list.tweak_mode(LIST_MODES,true);
					if (!add) {
						var tmp_mode = "";
						if((cmd[arg][0] != "+") ||
						   (cmd[arg][0] != "-") )
							tmp_mode += "+";
						tmp_mode += cmd[arg].replace(/[-]/g," ");
						tmp_mode = tmp_mode.replace(/[+]/g,"-");
						list.Modes = tmp_mode.replace(/[ ]/g,"+");
					} else {
						list.Modes = cmd[arg];
					}
				}
				break;
			case "o":
				arg++;
				if (cmd[arg]) {
					list.tweak_mode(LIST_TOPIC,add);
					list.Topic = cmd[arg];
				}
				break;
			case "p":
				arg++;
				if (cmd[arg]) {
					list.tweak_mode(LIST_PEOPLE,add);
					list.People = parseInt(cmd[arg]);
				}
				break;
			case "t":
				arg++;
				if (cmd[arg]) {
					list.tweak_mode(LIST_TOPICAGE,add);
					list.TopicTime = parseInt(cmd[arg])*60;
				}
				break;
			case "M":
				list.tweak_mode(LIST_DISPLAY_CHAN_MODES,add);
				break;
			default:
				break;
		}
	}

	// Generic mask atop all this crap?
	arg++;
	if (cmd[arg])
		listmask = cmd[arg];

	// Here we go...
	for (aChan in Channels) {
		// Is the user allowed to see this channel, for starters?
		if (!(Channels[aChan].mode&CHANMODE_SECRET) ||
		     (this.mode&USERMODE_OPER) || this.channels[aChan]) {
			
			if ((list.add_flags&LIST_CHANMASK) &&
			    !wildmatch(aChan,list.Mask.toUpperCase()))
				continue;
			else if ((list.del_flags&LIST_CHANMASK) &&
			    wildmatch(aChan,list.Mask.toUpperCase()))
				continue;
			if ((list.add_flags&LIST_CREATED) &&
			   (Channels[aChan].created < (time() - list.Created)))
				continue;
			else if ((list.del_flags&LIST_CREATED) &&
			   (Channels[aChan].created > (time() - list.Created)))
				continue;
			if ((list.add_flags&LIST_TOPIC) &&
			   (!wildmatch(Channels[aChan].topic,list.Topic)))
				continue;
			else if ((list.del_flags&LIST_TOPIC) &&
			   (wildmatch(Channels[aChan].topic,list.Topic)))
				continue;
			if ((list.add_flags&LIST_PEOPLE) &&
			    (true_array_len(Channels[aChan].users) < list.People) )
				continue;
			else if ((list.del_flags&LIST_PEOPLE) &&
			    (true_array_len(Channels[aChan].users) >= list.People) )
				continue;
			if ((list.add_flags&LIST_TOPICAGE) && list.TopicTime &&
			  (Channels[aChan].topictime > (time()-list.TopicTime)))
				continue;
			else if((list.del_flags&LIST_TOPICAGE)&&list.TopicTime&&
			  (Channels[aChan].topictime < (time()-list.TopicTime)))
				continue;
			if (list.add_flags&LIST_MODES) { // there's no -m
				var sic = false;
				var madd = true;
				var c = Channels[aChan];
				for (mm in list.Modes) {
					switch(list.Modes[mm]) {
						case "+":
							if (!madd)
								madd = true;
							break;
						case "-":
							if (madd)
								madd = false;
							break;
						case "i":
							if (
							   (!madd && (c.mode&
							   CHANMODE_INVITE))
							||
							   (madd && !(c.mode&
							   CHANMODE_INVITE))
							   )
								sic = true;
							break;
						case "k":
							if (
							   (!madd && (c.mode&
							   CHANMODE_KEY))
							||
							   (madd && !(c.mode&
							   CHANMODE_KEY))
							   )
								sic = true;
							break;
						case "l":
							if (
							   (!madd && (c.mode&
							   CHANMODE_LIMIT))
							||
							   (madd && !(c.mode&
							   CHANMODE_LIMIT))
							   )
								sic = true;
							break;
						case "m":
							if (
							   (!madd && (c.mode&
							   CHANMODE_MODERATED))
							||
							   (madd && !(c.mode&
							   CHANMODE_MODERATED))
							   )
								sic = true;
							break;
						case "n":
							if (
							   (!madd && (c.mode&
							   CHANMODE_NOOUTSIDE))
							||
							   (madd && !(c.mode&
							   CHANMODE_NOOUTSIDE))
							   )
								sic = true;
							break;
						case "p":
							if (
							   (!madd && (c.mode&
							   CHANMODE_PRIVATE))
							||
							   (madd && !(c.mode&
							   CHANMODE_PRIVATE))
							   )
								sic = true;
							break;
						case "s":
							if (
							   (!madd && (c.mode&
							   CHANMODE_SECRET))
							||
							   (madd && !(c.mode&
							   CHANMODE_SECRET))
							   )
								sic = true;
							break;
						case "t":
							if (
							   (!madd && (c.mode&
							   CHANMODE_TOPIC))
							||
							   (madd && !(c.mode&
							   CHANMODE_TOPIC))
							   )
								sic = true;
							break;
						default:
							break;
					}
					if (sic)
						break;
				}
				if (sic)
					continue;
			}

			if (listmask)
				listmask_items = listmask.split(",");
			var l_match = false; // assume we match nothing.
			if (listmask_items) {
				for (l in listmask_items) {
					if (Channels[aChan].match_list_mask
					   (listmask_items[l])) {
						l_match = true;
						break;
					}
				}
				if (!l_match)
					continue;
			}

			// We made it.
			if (list.add_flags&LIST_DISPLAY_CHAN_MODES)
				this.numeric322(Channels[aChan],true);
			else
				this.numeric322(Channels[aChan]);
		}
	}

	this.numeric(323, ":End of /LIST. (Complex)");
}
		
// Object which stores LIST bits and arguments.
function List() {
	this.add_flags = 0;
	this.del_flags = 0;
	this.tweak_mode = List_tweak_mode;
	this.People = 0;
	this.Created = 0;
	this.TopicTime = 0;
	this.Mask = "";
	this.Modes = "";
	this.Topic = "";
}       

function List_tweak_mode(bit,add) {
	if (add) {
		this.add_flags |= bit;
		this.del_flags &= ~bit;
	} else {
		this.add_flags &= ~bit;
		this.del_flags |= bit;
	}
}

function IRCClient_do_list_usage() {
	this.numeric(334,":/LIST [+|-][acmoptM] <args> <mask|channel{,channel}>");
	this.numeric(334,":The modes as above work exactly like channel modes.");
	this.numeric(334,":<mask> may be just like Bahamut notation.");
	this.numeric(334,":(Bahamut Notation = >num,<num,C>num,C<num,T>num,T<num,*mask*,!*mask*)");
	this.numeric(334,":i.e. '/LIST +p 50 #*irc*' lists chans w/ irc in the name and 50+ users.");
	this.numeric(334,":No args/mask matches to everything by default.");
	this.numeric(334,":a <mask>: List channels whose names match the mask.  Wildcards allowed.");
	this.numeric(334,":c <time>: Chans created less than (-) or more than (+) <time> mins ago.");
	this.numeric(334,":m <mods>: Channel has <modes> set.  -+ allowed.");
	this.numeric(334,":o <topc>: Match against channel's <topic>, wildcards allowed.");
	this.numeric(334,":p <num> : Chans with more or equal to (+) members, or (-) less than.");
	this.numeric(334,":t <time>: Only channels whose topics were created <time> mins ago.");
	this.numeric(334,":M       : Show channel's mode in front of the list topic.");
	// No "end of" numeric for this.
}

// does 'this' (channel) match the 'mask' passed to us?  Use 'complex'
// Bahamut parsing to determine that.
function Channel_match_list_mask(mask) {
	if (mask[0] == ">") { // Chan has more than X people?
		if (true_array_len(this.users) < parseInt(mask.slice(1)))
			return 0;
	} else if (mask[0] == "<") { // Chan has less than X people?
		if (true_array_len(this.users) >= parseInt(mask.slice(1)))
			return 0;
	} else if (mask[0].toUpperCase() == "C") { //created X mins ago?
		if ((mask[1] == ">") && (this.created <
		    (time() - (parseInt(mask.slice(2)) * 60)) ) )
			return 0;
		else if ((mask[1] == "<") && (this.created >
		         (time() - (parseInt(mask.slice(2)) * 60)) ) )
			return 0;
	} else if (mask[0].toUpperCase() == "T") { //topics older than X mins?
		if ((mask[1] == ">") && (this.topictime <
		    (time() - (parseInt(mask.slice(2)) * 60)) ) )
			return 0;
		else if ((mask[1] == "<") && (this.topictime >
		         (time() - (parseInt(mask.slice(2)) * 60)) ) )
			return 0;
	} else if (mask[0] == "!") { // doesn't match mask X
		if (wildmatch(this.nam,mask.slice(1).toUpperCase()))
			return 0;
	} else { // if all else fails, we're matching a generic channel mask.
		if (!wildmatch(this.nam,mask))
			return 0;
	}
	return 1; // if we made it here, we matched something.
}

function IRCClient_get_usermode(bcast_modes) {
	var tmp_mode = "+";
	for (ch in USERMODE_CHAR) {
		if ((!bcast_modes || (bcast_modes && USERMODE_BCAST[ch])) &&
		    this.mode&USERMODE_CHAR[ch])
			tmp_mode += ch;
	}
	return tmp_mode;
}

function UMode_tweak_mode(bit,add) {
	if (add) {
		this.add_flags |= bit;
		this.del_flags &= ~bit;
	} else {
		this.add_flags &= ~bit;
		this.del_flags |= bit;
	}
}

function UMode() {
	this.add_flags = 0;
	this.del_flags = 0;
	this.tweak_mode = UMode_tweak_mode;
}

function IRCClient_setusermode(modestr) {
	if (!modestr)
		return 0;
	var add=true;
	var unknown_mode=false;
	var umode = new UMode();
	for (modechar in modestr) {
		switch (modestr[modechar]) {
			case "+":
				if (!add)
					add=true;
				break;
			case "-":
				if (add)
					add=false;
				break;
			case "i":
			case "w":
			case "s":
			case "k":
			case "g":
				umode.tweak_mode(USERMODE_CHAR
					[modestr[modechar]],add);
				break;
			case "b":
			case "r":
			case "f":
			case "y":
			case "d":
			case "n":
				if (this.mode&USERMODE_OPER)
					umode.tweak_mode(USERMODE_CHAR
						[modestr[modechar]],add);
				break;
			case "o":
				// Allow +o only by servers or non-local users.
				if (add && this.parent &&
				    Servers[this.parent.toLowerCase()].hub)
					umode.tweak_mode(USERMODE_OPER,true);
				else if (!add)
					umode.tweak_mode(USERMODE_OPER,false);
				break;
			case "c":
				if ((this.mode&USERMODE_OPER) &&
				    (this.flags&OLINE_CAN_UMODEC))
					umode.tweak_mode(USERMODE_CLIENT,add);
				break;
			case "A":
				if ( ((this.mode&USERMODE_OPER) && (this.flags&OLINE_IS_ADMIN))
					|| (this.parent && Servers[this.parent.toLowerCase()].hub) )
					umode.tweak_mode(USERMODE_ADMIN,add);
				break;
			default:
				if (!unknown_mode && !this.parent) {
					this.numeric(501, ":Unknown MODE flag");
					unknown_mode=true;
				}
				break;
		}
	}
	var addmodes = "";
	var delmodes = "";
	var bcast_addmodes = "";
	var bcast_delmodes = "";
	for (mym in USERMODE_CHAR) {
		if ((umode.add_flags&USERMODE_CHAR[mym]) &&
		    !(this.mode&USERMODE_CHAR[mym])) {
			addmodes += mym;
			if (USERMODE_BCAST[mym])
				bcast_addmodes += mym;
			this.mode |= USERMODE_CHAR[mym];
		} else if ((umode.del_flags&USERMODE_CHAR[mym]) &&
		    (this.mode&USERMODE_CHAR[mym])) {
			delmodes += mym;
			if (USERMODE_BCAST[mym])
				bcast_delmodes += mym;
			this.mode &= ~USERMODE_CHAR[mym];
		}
	}
	if (!addmodes && !delmodes)
		return 0;
	var final_modestr = "";
	var bcast_modestr = "";
	if (addmodes)
		final_modestr += "+" + addmodes;
	if (delmodes)
		final_modestr += "-" + delmodes;
	if (bcast_addmodes)
		bcast_modestr += "+" + bcast_addmodes;
	if (bcast_delmodes)
		bcast_modestr += "-" + bcast_delmodes;
	if (this.local && !this.server) {
		this.originatorout("MODE "+this.nick+" "+final_modestr,this);
		if (bcast_addmodes || bcast_delmodes)
			this.bcast_to_servers("MODE "+this.nick+" "+bcast_modestr);
	}
	return bcast_modestr;
}

function IRCClient_check_timeout() {
	if (!this.pinged && ((time() - this.idletime) >
	    YLines[this.ircclass].pingfreq)) {
		this.pinged = time();
		this.rawout("PING :" + servername);
	} else if (this.pinged && ((time() - this.pinged) >
	    YLines[this.ircclass].pingfreq)) {
		this.quit("Ping Timeout");
		return 1; // ping timeout
	}
	return 0; // no ping timeout
}

function IRCClient_check_queues() {
	var cmd;

	this.sendq.send(this.socket);
	while (this.recvq.queue.length) {
		cmd = this.recvq.del();
		Global_CommandLine = cmd;
		this.work(cmd);
		if (this.replaced_with !== undefined) {
			this.replaced_with.check_queues();
			break;
		}
	}
}

function IRCClient_finalize_server_connect(states,sendps) {
	var statestr = " :" + states;
	/* DreamForge-based servers do not support passing state information
	   along on the PASS message */
	if (this.type&DREAMFORGE)
		statestr = "";
	hcc_counter++;
	gnotice("Link with " + this.nick + "[unknown@" + this.hostname +
		"] established, states: " + states);
	if (server.client_update != undefined)
		server.client_update(this.socket, this.nick, this.hostname);
	if (sendps) {
		for (cl in CLines) {
			if(wildmatch(this.nick,CLines[cl].servername)) {
				this.rawout("PASS " + CLines[cl].password + statestr);
				break;
			}
		}
		if (this.type&BAHAMUT)
			this.rawout("CAPAB " + server_capab);
		this.rawout("SERVER " + servername + " 1 :" + serverdesc);
	}
	this.bcast_to_servers_raw(":" + servername + " SERVER " + this.nick
		+ " 2 :" + this.info);
	this.synchronize();
}

function CLine(host,password,servername,port,ircclass) {
	this.host = host;
	this.password = password;
	this.servername = servername;
	this.port = port;
	this.ircclass = ircclass;
	this.lastconnect = 0;
}

function HLine(allowedmask,servername) {
	this.allowedmask = allowedmask;
	this.servername = servername;
}

function ILine(ipmask,password,hostmask,port,ircclass) {
	this.ipmask = ipmask;
	this.password = password;
	this.hostmask = hostmask;
	this.port = port;
	this.ircclass = ircclass;
}

function KLine(hostmask,reason,type) {
	this.hostmask = hostmask;
	this.reason = reason;
	this.type = type;
}

function NLine(host,password,servername,flags,ircclass) {
	this.host = host;
	this.password = password;
	this.servername = servername;
	this.flags = flags;
	this.ircclass = ircclass;
}

function OLine(hostmask,password,nick,flags,ircclass) {
	this.hostmask = hostmask;
	this.password = password;
	this.nick = nick;
	this.flags = flags;
	this.ircclass = ircclass;
}

function QLine(nick,reason) {
	this.nick = nick;
	this.reason = reason;
}

function YLine(pingfreq,connfreq,maxlinks,sendq) {
	this.pingfreq = pingfreq;
	this.connfreq = connfreq;
	this.maxlinks = maxlinks;
	this.sendq = sendq;
	this.active = 0;
}

function ZLine(ipmask,reason) {
	this.ipmask = ipmask;
	this.reason = reason;
}

function WhoWasObj(nick,uprefix,host,realname,server,serverdesc) {
	this.nick = nick;
	this.uprefix = uprefix;
	this.host = host;
	this.realname = realname;
	this.server = server;
	this.serverdesc = serverdesc;
}

function NickBuf(oldnick,newnick) {
	this.oldnick = oldnick;
	this.newnick = newnick;
}

// used for tracking true SJOIN nicks.
function SJOIN_Nick(nick,isop,isvoice) {
	this.nick = nick;
	this.isop = isop;
	this.isvoice = isvoice;
}

// Track IRC socket queues
function IRC_Queue() {
	this.queue = new Array;
	this._recv_bytes = '';
	this._send_bytes = '';
	this.add = Queue_Add;
	this.del = Queue_Del;
	this.prepend = Queue_Prepend;
	this.recv = Queue_Recv;
	this.send = Queue_Send;
}

/* /STATS M, for profiling. */
function StatsM() {
	this.ticks = 0;
	this.executions = 0;
}

