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
const VERSION = "SynchronetIRCd-1.9";
const VERSION_STR = format(
	"Synchronet %s%s-%s%s (IRCd by Randy Sommerfeld)",
	system.version, system.revision,
	system.platform, system.beta_version
);
/* This will be replaced with a dynamic CAPAB system */
const SERVER_CAPAB = "TS3 NOQUIT SSJOIN BURST UNCONNECT NICKIP TSMODE";
/* This will be in the configuration for 2.0 */
const SUMMON = true;

/* Need to detect when a server doesn't line up with these on the network. */
const MAX_CHANLEN = 100;      /* Maximum channel name length. */
const MAX_NICKLEN = 30;       /* Maximum nickname length. */
const MAX_MODES = 6;          /* Maximum modes on single MODE command */
const MAX_USER_CHANS = 100;   /* Maximum channels users can join */
const MAX_BANS = 25;          /* Maximum bans (+b) per channel */
const MAX_TOPICLEN = 307;     /* Maximum length of topic per channel */
const MAX_KICKLEN = 307;      /* Maximum length of kick reasons */
const MAX_WHO = 100;          /* Maximum replies to WHO for non-oper users */
const MAX_SILENCE = 10;       /* Maximum entries on a user's SILENCE list */
const MAX_WHOWAS = 1000;      /* Size of the WHOWAS buffer */
const MAX_NICKHISTORY = 1000; /* Size of the nick change history buffer */
const MAX_CLIENT_RECVQ = 2560;/* Maximum size of unregistered & user recvq */
const MAX_AWAYLEN = 80;       /* Maximum away message length */
const MAX_USERHOST = 6;       /* Maximum arguments to USERHOST command */
const MAX_REALNAME = 50;      /* Maximum length of users real name field */

const SERVER_UPTIME = system.timer;
const SERVER_UPTIME_STRF = strftime("%a %b %d %Y at %H:%M:%S %Z",Epoch());

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

function config_rehash_semaphore_check() {
	if(file_date(system.ctrl_dir + "ircd.rehash") > Time_Config_Read) {
		Read_Config_File();
	}
}
js.setInterval(config_rehash_semaphore_check, 1000 /* milliseconds */);

Open_PLines();

/* We exit here and pass everything to the callback engine. */
/* Deuce says I can't use exit(). So just pretend it's here instead. */
