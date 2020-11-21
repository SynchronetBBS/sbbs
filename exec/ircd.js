/*

 ircd.js

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details:
 http://www.gnu.org/licenses/gpl.txt

 Synchronet IRC Daemon. Link compatible with Bahamut.

 Copyright 2003-2020 Randy Sommerfeld <cyan@rrx.ca>

*/

/* Synchronet libraries */
load("sbbsdefs.js");
load("sockdefs.js");
load("nodedefs.js");
load("irclib.js");

/* Libraries specific to the IRCd */
load("ircd/core.js");
load("ircd/unregistered.js");
load("ircd/user.js");
load("ircd/channel.js");
load("ircd/server.js");
load("ircd/objects.js");

const VERSION = "SynchronetIRCd-1.9a";
const VERSION_STR = format(
	"Synchronet %s%s-%s%s (IRCd by Randy Sommerfeld)",
	system.version, system.revision,
	system.platform, system.beta_version
);

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

var default_port = 6667;

/* This was previously on its own in the functions
   Maybe there was a reason why? */
var time_config_read;

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

