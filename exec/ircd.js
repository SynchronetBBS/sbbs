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
 https://www.gnu.org/licenses/old-licenses/gpl-2.0.txt

 Synchronet IRC Daemon. Link compatible with Bahamut.

 Copyright 2003-2022 Randy Sommerfeld <cyan@synchro.net>

*/

"use strict";

/* Synchronet libraries */
load("sbbsdefs.js");
load("sockdefs.js");
load("nodedefs.js");
load("irclib.js");
load("dns.js");

/* Libraries specific to the IRCd */
load("ircd/core.js");
load("ircd/unregistered.js");
load("ircd/user.js");
load("ircd/channel.js");
load("ircd/server.js");
load("ircd/config.js");

/*** Global Constants - Always in ALL_UPPERCASE ***/
var VERSION = "SynchronetIRCd-2.2";
var VERSION_STR = format(
	"Synchronet %s%s-%s%s (IRCd by Randy Sommerfeld)",
	system.version, system.revision,
	system.platform, system.beta_version
);
/* This will be replaced with a dynamic CAPAB system */
/* TSJOIN signals support for the TS-bearing JOIN burst wire format.
   Replace this token (not append) when the next network-breaking change lands. */
var SERVER_CAPAB = "TS3 NOQUIT SSJOIN BURST UNCONNECT NICKIP NICKIPSTR TSMODE TSJOIN";
/* This will be in the configuration for 2.0 */
var SUMMON = true;

/* Need to detect when a server doesn't line up with these on the network. */
var MAX_CHANLEN = 100;      /* Maximum channel name length. */
var MAX_NICKLEN = 30;       /* Maximum nickname length. */
var MAX_MODES = 6;          /* Maximum modes on single MODE command */
var MAX_USER_CHANS = 100;   /* Maximum channels users can join */
var MAX_BANS = 25;          /* Maximum bans (+b) per channel */
var MAX_TOPICLEN = 307;     /* Maximum length of topic per channel */
var MAX_KICKLEN = 307;      /* Maximum length of kick reasons */
var MAX_WHO = 100;          /* Maximum replies to WHO for non-oper users */
var MAX_SILENCE = 10;       /* Maximum entries on a user's SILENCE list */
var MAX_WHOWAS = 1000;      /* Size of the WHOWAS buffer */
var MAX_NICKHISTORY = 1000; /* Size of the nick change history buffer */
var MAX_CLIENT_RECVQ = 2560;/* Maximum size of unregistered & user recvq */
var MAX_AWAYLEN = 80;       /* Maximum away message length */
var MAX_USERHOST = 6;       /* Maximum arguments to USERHOST command */
var MAX_REALNAME = 50;      /* Maximum length of users real name field */

/* The rehash semaphore is polled with a blocking stat(), so how often we
   look at it is how often we're willing to block on ctrl_dir.  That dir is
   not always local (SBBSCTRL can name a network share), and a wedged share
   blocks the single-threaded callback engine along with everything else. */
var REHASH_POLL_MSECS = 15000;     /* Normal ircd.rehash poll interval */
var REHASH_POLL_MAX_MSECS = 300000;/* Backoff ceiling when ctrl_dir is slow */
var REHASH_SLOW_SECS = 1;          /* A stat this slow means ctrl_dir is sick */

var STALL_TICK_MSECS = 1000;       /* Event-loop watchdog tick */
var STALL_THRESHOLD_SECS = 10;     /* A tick gap this big is a real stall */

var SERVER_UPTIME = system.timer;
var SERVER_UPTIME_STRF = strftime("%a %b %d %Y at %H:%M:%S %Z",Epoch());

var IRCDCFG_Editor = false;

/*** Global Objects, Arrays and Variables - Always in Mixed_Case ***/

/* Global Objects */
var DNS_Resolver = new DNS();

/* Every object (unregistered, server, user) is tagged with a unique ID */
var Assigned_IDs = {};      /* Key: Numeric ID */

var Unregistered = {};      /* Key: Numeric ID */
var Users = {};             /* Key: .toUpperCase() nick */
var Servers = {};           /* Key: .toLowerCase() nick */
var Channels = {};          /* Key: .toUpperCase() channel name, including prefix */

var Local_Users = {};
var Local_Servers = {};

var WhoWas = {};			/* Stores uppercase nicks */
var WhoWasMap = [];			/* An array pointing to WhoWas object entries */

var NickHistory = [];		/* Nick change tracking */

var Profile = {};			/* CPU profiling */

/* Global Variables */
var Default_Port = 6667;

var Time_Config_Read;		/* Stores unix epoch of when the config was last read */

var Outbound_Connect_in_Progress = false;

/* Will this server try to enforce good network behaviour? */
/* Setting to "true" results in bouncing bad modes, KILLing bogus NICKs, etc. */
var Enforcement = true;

/* Reject server links missing required CAPAB tokens when true; warn-only when false */
var CapabEnforce = false;

/* Highest Connection Count ("HCC") tracking */
var HCC_Total = 0;
var HCC_Users = 0;
var HCC_Counter = 0;

var ServerName;
var ServerDesc = "";

/* Runtime configuration */
var Config_Filename = "";

var Restart_Password;
var Die_Password;

var Admin1;
var Admin2;
var Admin3;

var CLines = [];	/* Server [C]onnect lines */
var NLines = [];	/* Server inbound connect lines */
var HLines = [];	/* Hubs */
var ILines = [];	/* IRC Classes */
var KLines = [];	/* user@hostname based bans */
var OLines = [];	/* IRC Operators */
var PLines = [];	/* Ports for the IRCd to listen to */
var QLines = [];	/* [Q]uarantined (reserved) nicknames */
var ULines = [];	/* Servers allowed to send unchecked MODE amongst other things */
var YLines = [];	/* Defines what user & server objects get what settings */
var ZLines = [];	/* IP based bans */
var RBL = [];		/* RBL's */

/** Begin executing code **/

log(LOG_NOTICE, VERSION + " started.");

/* If we're running from JSexec we don't have a global server object, so fake one. */
if (server === undefined) {
	/* Define the global here so Startup() can manipulate it. */
	var server = "JSexec";
}

Startup();

js.do_callbacks = true;
js.branch_limit = 0; /* Disable infinite loop detection */

/* Polling the rehash semaphore means a blocking stat() of ctrl_dir on the
   one thread that also services every socket.  When ctrl_dir lives on a
   network share (SBBSCTRL can name one) and that share wedges, each poll
   can block for minutes: the daemon stops answering PINGs and the entire
   network splits off.  So poll at a leisurely interval, and back off
   further -- up to REHASH_POLL_MAX_MSECS -- for as long as the stat itself
   is slow, rather than piling into a dead filesystem once a second. */
var Rehash_Poll_Msecs = REHASH_POLL_MSECS;

function adjust_rehash_poll_interval(elapsed) {
	if (elapsed >= REHASH_SLOW_SECS) {
		if (Rehash_Poll_Msecs >= REHASH_POLL_MAX_MSECS)
			return;
		Rehash_Poll_Msecs = Math.min(
			Rehash_Poll_Msecs * 2,
			REHASH_POLL_MAX_MSECS
		);
		log(LOG_WARNING, format(
			"Reading %sircd.rehash took %.1f seconds, "
			+ "backing off to one check every %u seconds",
			system.ctrl_dir,
			elapsed,
			Rehash_Poll_Msecs / 1000
		));
		return;
	}
	if (Rehash_Poll_Msecs == REHASH_POLL_MSECS)
		return;
	Rehash_Poll_Msecs = REHASH_POLL_MSECS;
	log(LOG_NOTICE, format(
		"%s is responsive again, resuming one check every %u seconds",
		system.ctrl_dir,
		Rehash_Poll_Msecs / 1000
	));
}

function config_rehash_semaphore_check() {
	/* Re-arm from finally: a throw out of Read_Config_File() must not be
	   what stops us ever looking at the semaphore again. */
	try {
		var started = system.timer;
		var semaphore_date = file_date(system.ctrl_dir + "ircd.rehash");

		adjust_rehash_poll_interval(system.timer - started);
		if (semaphore_date > Time_Config_Read)
			Read_Config_File();
	} finally {
		js.setTimeout(config_rehash_semaphore_check, Rehash_Poll_Msecs);
	}
}
js.setTimeout(config_rehash_semaphore_check, Rehash_Poll_Msecs);

/* Everything here runs on a single thread, so anything that blocks -- a
   stat() of an unreachable ctrl_dir, a suspended machine, a long GC --
   freezes every timer at once and every peer looks silent when we come
   back.  Watch the gap between our own ticks so IRCClient_check_timeout()
   can tell "the peer went away" from "we weren't running"; see
   Stall_Generation in ircd/core.js. */
var Last_Stall_Tick = system.timer;

function stall_watchdog() {
	var now = system.timer;
	var gap = now - Last_Stall_Tick;

	Last_Stall_Tick = now;
	if (gap < STALL_THRESHOLD_SECS)
		return;
	Stall_Generation++;
	log(LOG_WARNING, format(
		"Callback engine stalled for %.1f seconds, "
		+ "forgiving one ping round on every connection",
		gap
	));
	gnotice(format("Server stalled for %.1f seconds.", gap));
}
js.setInterval(stall_watchdog, STALL_TICK_MSECS);

/* When Synchronet's services subsystem (the Windows control panel, or
   sbbscon/the service manager on Linux) asks us to stop, it sets
   server.terminated.  Notice it promptly, notify the rest of the network,
   and let the callback engine wind down.  When run standalone under JSexec,
   `server` is the string "JSexec", so this check is simply skipped. */
function shutdown_semaphore_check() {
	if (typeof server === "object" && server.terminated) {
		Notify_Servers_Of_Shutdown("Server shutting down");
		js.do_callbacks = false;
	}
}
js.setInterval(shutdown_semaphore_check, 1000 /* milliseconds */);

/* Belt-and-suspenders: js.on_exit fires whenever this script is torn down --
   by the services subsystem, the sysop, or a JSexec exit -- on every
   platform, even if the terminated poll above never ran.  Notify is
   idempotent, so the DIE/RESTART path and this backstop never double-send. */
js.on_exit("Notify_Servers_Of_Shutdown('Server shutting down');");

Open_PLines();

/* We exit here and pass everything to the callback engine. */
/* Deuce says I can't use exit(). So just pretend it's here instead. */
