// $Id$
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
// Copyright 2003 Randolph Erwin Sommerfeld <sysop@rrx.ca>
//

load("sbbsdefs.js");
load("sockdefs.js");
load("nodedefs.js");

// CVS revision
const REVISION = "$Revision$".split(' ')[1];

// Please don't play with this, unless you're making custom hacks.
// IF you're making a custom version, it'd be appreciated if you left the
// version number alone, and add a token in the form of +hack (i.e. 1.0+cyan)
// This is so everyone knows your revision base, AND type of hack used.
const VERSION = "SynchronetIRCd-1.0b(" + REVISION + ")";
const VERSION_STR = "Synchronet " 
	+ system.version + system.revision + "-" + system.platform + 
	system.beta_version + " (IRCd by Randy Sommerfeld)";

// This will dump all I/O to and from the server to your Synchronet console.
// It also enables some more verbose WALLOPS, especially as they pertain to
// blocking functions.
// The special "DEBUG" oper command also switches this value.
var debug = false;

// Resolve connecting clients' hostnames?  If set to false, everyone will have
// an IP address instead of a hostname in their nick!user@host identifier.
// Resolving hostnames is a BLOCKING operation, so your IRCD *will* freeze for
// the amount of time it takes to resolve a host.
// If you have a local (caching) name server, this shouldn't be a problem, but
// the slower your connection to the net and the further away your named is,
// the longer lookups will block for.  Busy servers should almost always
// disable this.
// Exception: 'localhost' and '127.0.0.1' always get resolved internally to
// the hostname defined on the M:Line regardless of this setting.
var resolve_hostnames = true;

// The number of seconds to block before giving up on outbound CONNECT
// attempts (when connecting to another IRC server -- i.e. a hub)  This value
// is important because connecing is a BLOCKING operation, so your IRC *will*
// freeze for the amount of time it takes to connect.
var ob_sock_timeout = 6;

// Should we enable the USERS and SUMMON commands?  These allow IRC users to
// view users on the local BBS and summon them to IRC via a Synchronet telegram
// message respectively.  Some people might be running the ircd standalone, or
// otherwise don't want anonymous IRC users to have access to these commands.
// We enable this by default because there's typically nothing wrong with
// seeing who's on an arbitrary BBS or summoning them to IRC.
var enable_users_summon = true;

// what our server is capable of from a server point of view.
// TS3 = Version 3 of accepted interserver timestamp protocol.
// NOQUIT = NOQUIT interserver command supported.
// SSJOIN = SJOIN interserver command supported without dual TS, single TS only.
// BURST = Sending of network synch data is done in a 3-stage burst (BURST cmd)
// UNCONNECT = UNCONNECT interserver command supported.
// NICKIP = 9th parameter of interserver NICK command is an integer IP.
// TSMODE = Timestamps are given on plain MODE commands.
// ZIP = server supports gzip on the fly for interserver communication.
var server_capab = "TS3 NOQUIT SSJOIN BURST UNCONNECT NICKIP TSMODE";

// EVERY server on the network MUST have the same values in ALL of these
// categories.  If you change these, you WILL NOT be able to link to the
// Synchronet IRC network.  Linking servers with different values here WILL
// cause your network to desynchronize (and possibly crash the IRCD)
// Remember, this is Synchronet, not Desynchronet ;)
var max_chanlen = 100;		// Maximum channel name length.
var max_nicklen = 30;		// Maximum nickname length.
var max_modes = 6;		// Maximum modes on single MODE command
var max_user_chans = 10;	// Maximum channels users can join
var max_bans = 25;		// Maximum bans (+b) per channel
var max_topiclen = 307;		// Maximum length of topic per channel
var max_kicklen = 307;		// Maximum length of kick reasons
var max_who = 100;		// Maximum replies to WHO for non-oper users

var default_port = 6667;

// User modes
var USERMODE_NONE		=(1<<0); // NONE
var USERMODE_OPER		=(1<<1); // o
var USERMODE_INVISIBLE		=(1<<2); // i

// Channel modes
var CHANMODE_NONE		=(1<<0); // NONE
var CHANMODE_BAN		=(1<<1); // b
var CHANMODE_INVITE		=(1<<2); // i
var CHANMODE_KEY		=(1<<3); // k
var CHANMODE_LIMIT		=(1<<4); // l
var CHANMODE_MODERATED		=(1<<5); // m
var CHANMODE_NOOUTSIDE		=(1<<6); // n
var CHANMODE_OP			=(1<<7); // o
var CHANMODE_PRIVATE		=(1<<8); // p
var CHANMODE_SECRET		=(1<<9); // s
var CHANMODE_TOPIC		=(1<<10); // t
var CHANMODE_VOICE		=(1<<11); // v

// Channel lists.  Inverses of a mode MUST always be 1 'away' from each other
// in the reverse direction.  For example, -o is +1 from +o, and +o is -1
// from -o.
const CHANLIST_NONE		=0; // null
const CHANLIST_OP		=1; // +o
const CHANLIST_DEOP		=2; // -o
const CHANLIST_VOICE		=3; // +v
const CHANLIST_DEVOICE		=4; // -v
const CHANLIST_BAN		=5; // +b
const CHANLIST_UNBAN		=6; // -b

// These are used in the mode crunching section to figure out what character
// to display in the crunched MODE line.
function Mode (modechar,args,state,list) {
	this.modechar = modechar;
	this.args = args;
	this.state = state;
	this.list = list;
}

MODE = new Array();
MODE[CHANMODE_BAN] 		= new Mode("b",true,false,CHANLIST_BAN);
MODE[CHANMODE_INVITE] 		= new Mode("i",false,true,0);
MODE[CHANMODE_KEY] 		= new Mode("k",true,true,0);
MODE[CHANMODE_LIMIT] 		= new Mode("l",true,true,0);
MODE[CHANMODE_MODERATED] 	= new Mode("m",false,true,0);
MODE[CHANMODE_NOOUTSIDE] 	= new Mode("n",false,true,0);
MODE[CHANMODE_OP] 		= new Mode("o",true,false,CHANLIST_OP);
MODE[CHANMODE_PRIVATE] 		= new Mode("p",false,true,0);
MODE[CHANMODE_SECRET] 		= new Mode("s",false,true,0);
MODE[CHANMODE_TOPIC] 		= new Mode("t",false,true,0);
MODE[CHANMODE_VOICE]		= new Mode("v",true,false,CHANLIST_VOICE);

MODECHAR = new Array();
MODECHAR[CHANLIST_BAN]		= "b";
MODECHAR[CHANLIST_OP]		= "o";
MODECHAR[CHANLIST_VOICE]	= "v";

// Connection Types
const TYPE_EMPTY		=0;
const TYPE_UNREGISTERED		=1;
const TYPE_USER			=2;
const TYPE_USER_REMOTE		=3;
// Type 4 is reserved for a possible future client type.
const TYPE_SERVER		=5;
const TYPE_ULINE		=6;
// Anything 5 or above SHOULD be a server-type connection.

// Various permissions that can be set on an O:Line
var OLINE_CAN_REHASH		=(1<<0);	// r
var OLINE_CAN_RESTART		=(1<<1);	// R
var OLINE_CAN_DIE		=(1<<2);	// D
var OLINE_CAN_GLOBOPS		=(1<<3);	// g
var OLINE_CAN_WALLOPS		=(1<<4);	// w
var OLINE_CAN_LOCOPS		=(1<<5);	// l
var OLINE_CAN_LSQUITCON		=(1<<6);	// c
var OLINE_CAN_GSQUITCON		=(1<<7);	// C
var OLINE_CAN_LKILL		=(1<<8);	// k
var OLINE_CAN_GKILL		=(1<<9);	// K
var OLINE_CAN_KLINE		=(1<<10);	// b
var OLINE_CAN_UNKLINE		=(1<<11);	// B
var OLINE_CAN_LGNOTICE		=(1<<12);	// n
var OLINE_CAN_GGNOTICE		=(1<<13);	// N
// Synchronet IRCd doesn't have umode +Aa	RESERVED
// Synchronet IRCd doesn't have umode +a	RESERVED
// Synchronet IRCd doesn't have umode +c	RESERVED
// Synchronet IRCd doesn't have umode +f	RESERVED
// Synchronet IRCd doesn't have umode +F	RESERVED
var OLINE_CAN_CHATOPS		=(1<<19);	// s
var OLINE_CHECK_SYSPASSWD	=(1<<20);	// S
var OLINE_CAN_DEBUG		=(1<<21);	// x

// Various N:Line permission bits
var NLINE_CHECK_QWKPASSWD	=(1<<0);	// q
var NLINE_IS_QWKMASTER		=(1<<1);	// w
var NLINE_CHECK_WITH_QWKMASTER	=(1<<2);	// k

// Bits used for walking the complex WHO flags.
var WHO_AWAY			=(1<<0);	// a
var WHO_CHANNEL			=(1<<1);	// c
var WHO_REALNAME		=(1<<2);	// g
var WHO_HOST			=(1<<3);	// h
var WHO_IP			=(1<<4);	// i
var WHO_UMODE			=(1<<5);	// m
var WHO_NICK			=(1<<6);	// n
var WHO_OPER			=(1<<7);	// o
var WHO_SERVER			=(1<<8);	// s
var WHO_USER			=(1<<9);	// u
var WHO_FIRST_CHANNEL		=(1<<10);	// C
var WHO_MEMBER_CHANNEL		=(1<<11);	// M

////////// Functions not linked to an object //////////
function ip_to_int(ip) {
	var quads = ip.split(".");
	var addr = (quads[0]&0xff)<<24;
	addr|=(quads[1]&0xff)<<16;
	addr|=(quads[2]&0xff)<<8;
	addr|=(quads[3]&0xff);
	return addr;
}

function int_to_ip(ip) {
	return(format("%u.%u.%u.%u"
		,(ip>>24)&0xff
		,(ip>>16)&0xff
		,(ip>>8)&0xff
		,ip&0xff
		));
}

function terminate_everything(terminate_reason) {
	log("Terminating: " + terminate_reason);
	for(thisClient in Clients) {
		var Client = Clients[thisClient];
		if (Client && Client.local)
			Client.quit(terminate_reason,true)
	}
	exit();
}

function searchbynick(nick) {
	if (!nick)
		return 0;
	for(thisClient in Clients) {
		var Client=Clients[thisClient];
		if (Client 
			&& nick.toUpperCase() == Client.nick.toUpperCase() 
			&& Client.conntype 
			&& !Client.server)
			return Client; // success!
	}
	return 0; // failure
}

function searchbychannel(chan) {
	if (!chan)
		return 0;
	for(i in Channels) {
		if (chan.toUpperCase() == i.toUpperCase())
			return Channels[i]; // success!
	}
	return 0; // failure
}

function searchbyserver(server_name,ignore_wildcards) {
	if (!server_name)
		return 0;
	if ((ignore_wildcards &&
	    (servername.toUpperCase() == server_name.toUpperCase()) ) ||
	   (!ignore_wildcards && match_irc_mask(servername,server_name)) )
		return -1; // the server passed to us is our own.
	for(thisServer in Clients) {
		var Server=Clients[thisServer];
		if ((ignore_wildcards && (Server.conntype == TYPE_SERVER) &&
		    (Server.nick.toUpperCase() == server_name.toUpperCase())) ||
		   (!ignore_wildcards &&(Server.conntype == TYPE_SERVER) &&
		     match_irc_mask(Server.nick,server_name) ) )
			return Server;
	}
	// No wildcards implies not doing searches on nicks, either.
	if (ignore_wildcards)
		return 0;
	// if we've had no success so far, try nicknames and glean a server
	// from there.
	for(thisNick in Clients) {
		var Nick=Clients[thisNick];
		if (!Nick.server && match_irc_mask(Nick.nick,server_name))
			return searchbyserver(Nick.servername);
	}
	return 0; // looks like we failed after all that hard work :(
}

function IRCClient_searchbyiline() {
	for(thisILine in ILines) {
		// FIXME: We don't compare connecting port for now.
		if (
		    (match_irc_mask(this.uprefix + "@" + 
		     this.socket.remote_ip_address,ILines[thisILine].ipmask)) &&
		    (match_irc_mask(this.uprefix + "@" + this.hostname,
		     ILines[thisILine].hostmask))
		   )
			return ILines[thisILine];
	}
	return 0;
}

// IRC is funky.  A "string" in IRC is anything after a :, and anything before
// that is usually treated as fixed arguments.  So, this function is commonly
// used to fetch 'strings' from all sorts of commands.  PRIVMSG, NOTICE,
// USER, PING, etc.
function ircstring(str,startword) {
	var cindex;

	if (startword) {
		for(sw_counter=0;sw_counter<startword;sw_counter++) {
			str=str.slice(str.indexOf(" ")+1);
		}
	}
	cindex = str.indexOf(":")+1;
	if (!cindex)
		cindex = str.lastIndexOf(" ")+1;
	if (!cindex)
		return str;
	return(str.slice(cindex));
}

// Only allow letters, numbers and underscore in username to a maximum of
// 9 characters for 'anonymous' users (i.e. not using PASS to authenticate.)
// hostile characters like !,@,: etc would be bad here :)
function parse_username(str) {
	str.replace(/[^\w]/g,"");
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
			default:
				log("!WARNING Unknown N:Line flag '" + flags[thisflag] + "' in config.");       
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
			case "A":
			case "a":
			case "u":
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
				break;
			default:
				log("!WARNING Unknown O:Line flag '" + flags[thisflag] + "' in config.");
				break;
		}
	}
	return oline_flags;
}

function oper_notice(ntype,nmessage) {
	log(ntype + ": " + nmessage);
	for(thisoper in Clients) {
		var oper=Clients[thisoper];
		if ((oper.mode&USERMODE_OPER) && !oper.parent)
			oper.rawout(":" + servername + " NOTICE " + oper.nick + " :*** " + ntype + " -- " + nmessage);
	}
}
	
function create_ban_mask(str,kline) {
	tmp_banstr = new Array;
	tmp_banstr[0] = "";
	tmp_banstr[1] = "";
	tmp_banstr[2] = "";
	bchar_counter = 0;
	part_counter = 0; // BAN: 0!1@2 KLINE: 0@1
	regexp="[A-Za-z\{\}\`\^\_\|\\]\\[\\\\0-9\-.*?\~]";
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
		finalstr = tmp_banstr[0].slice(0,max_nicklen) + "!" + tmp_banstr[1].slice(0,10) + "@" + tmp_banstr[2].slice(0,80);
        while (finalstr.match(/[*][*]/)) {
                finalstr=finalstr.replace(/[*][*]/g,"*");
        }
	return finalstr;
}

function match_irc_mask(mtchstr,mask) {
	final_mask="^";
	mask=mask.replace(/[.]/g,"\\\.");
	mask=mask.replace(/[?]/g,".");
	mask=mask.replace(/[*]/g,".*?");
	final_mask=final_mask + mask + "$";
	return mtchstr.toUpperCase().match(final_mask.toUpperCase());
}

function isklined(kl_str) {
	for(the_kl in KLines) {
		if (KLines[the_kl].hostmask &&
		    match_irc_mask(kl_str,KLines[the_kl].hostmask))
			return 1;
	}
	return 0;
}

function iszlined(zl_ip) {
	for(the_zl in ZLines) {
		if (ZLines[the_zl].ipmask &&
		    match_irc_mask(zl_ip,ZLines[the_zl].ipmask))
			return 1;
	}
	return 0;
}

function scan_for_klined_clients() {
	for(thisUser in Clients) {
		if (Clients[thisUser]) {
			theuser=Clients[thisUser];
			if (theuser.local && !theuser.server && 
			    (theuser.conntype == TYPE_USER)) {
				if (isklined(theuser.uprefix + "@" + theuser.hostname))
					theuser.quit("User has been K:Lined (" + KLines[thiskl].reason + ")");
				if (iszlined(theuser.ip))
					theuser.quit("User has been Z:Lined");
			}
		}
	}
}

function remove_kline(kl_hm) {
	for(the_kl in KLines) {
		if (KLines[the_kl].hostmask &&
		    match_irc_mask(kl_hm,KLines[the_kl].hostmask)) {
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
	connect_sock = new Socket();
	connect_sock.connect(this_cline.host,the_port,ob_sock_timeout);
	if (connect_sock.is_connected) {
		oper_notice("Routing","Connected!  Sending info...");
		connect_sock.send("PASS " + this_cline.password + " :TS\r\n");
		connect_sock.send("CAPAB " + server_capab + "\r\n");
		connect_sock.send("SERVER " + servername + " 1 :" + serverdesc + "\r\n");
		new_id = get_next_clientid();
		Clients[new_id]=new IRCClient(connect_sock,new_id,true,true);
		Clients[new_id].sentps = true;
	}
	this_cline.lastconnect = time();
}

function wallopers(str) {
	var oper;

	for(thisoper in Clients) {
		oper=Clients[thisoper];
		if ((oper.mode&USERMODE_OPER) && !oper.parent)
			oper.rawout(str);
	}
}

function push_nickbuf(oldnick,newnick) {
	NickHistory.unshift(new NickBuf(oldnick,newnick));
	if(NickHistory.length >= nick_buffer)
		NickHistory.pop();
}

function search_nickbuf(bufnick) {
	for (nb=0;nb<NickHistory.length;nb++) {
		if (NickHistory[nb] && (bufnick.toUpperCase() == NickHistory[nb].oldnick.toUpperCase())) {
			if (!searchbynick(NickHistory[nb].newnick))
				return search_nickbuf(NickHistory[nb].newnick);
			else
				return NickHistory[nb].newnick;
		}
	}
	return 0;
}

function IRCClient_tweaktmpmode(tmp_bit,chan) {
	if ((!chan.ismode(this.id,CHANLIST_OP)) && (!this.server) && (!this.parent)) {
		this.numeric482(chan.nam);
		return 0;
	}
	if (add) {
		addbits|=tmp_bit;
		delbits&=~tmp_bit;
	} else {
		addbits&=~tmp_bit;
		delbits|=tmp_bit;
	}
}

function IRCClient_tweaktmpmodelist(tmp_cl,tmp_ncl,chan) {
	if ((!chan.ismode(this.id,CHANLIST_OP)) &&
	    (!this.server) && (!this.parent)) {
		this.numeric482(chan.nam);
		return 0;
	}
	if (add) {
		tmp_match = false;
		for (lstitem in chan_tmplist[tmp_cl]) {
			if (chan_tmplist[tmp_cl][lstitem] &&
			    (chan_tmplist[tmp_cl][lstitem].toUpperCase() ==
			     cm_args[mode_args_counter].toUpperCase())
			   )
				tmp_match = true;
		}
		if(!tmp_match) {
			chan_tmplist[tmp_cl][chan_tmplist[tmp_cl].length] = cm_args[mode_args_counter];
			for (lstitem in chan_tmplist[tmp_ncl]) {
				if (chan_tmplist[tmp_ncl][lstitem] &&
				    (chan_tmplist[tmp_ncl][lstitem].toUpperCase() == 
				     cm_args[mode_args_counter].toUpperCase())
				   )
					chan_tmplist[tmp_ncl][lstitem] = "";
			}
		}
	} else {
		tmp_match = false;
		for (lstitem in chan_tmplist[tmp_ncl]) {
			if (chan_tmplist[tmp_ncl][lstitem] &&
			    (chan_tmplist[tmp_ncl][lstitem].toUpperCase() ==
			     cm_args[mode_args_counter].toUpperCase())
			   )
				tmp_match = true;
		}
		if(!tmp_match) {
			chan_tmplist[tmp_ncl][chan_tmplist[tmp_ncl].length] = cm_args[mode_args_counter];
			for (lstitem in chan_tmplist[tmp_cl]) {
				if (chan_tmplist[tmp_cl][lstitem] &&
				    (chan_tmplist[tmp_cl][lstitem].toUpperCase() ==
				     cm_args[mode_args_counter].toUpperCase())
				   )
					chan_tmplist[tmp_cl][lstitem] = "";
			}
		}
	}
}

function count_channels() {
	var tmp_counter = 0;

	for (tmp_count in Channels) {
		if (Channels[tmp_count])
			tmp_counter++;
	}
	return tmp_counter;
}

function count_servers(count_all) {
	var tmp_counter;

	if (count_all)
		tmp_counter=1; // we start by counting ourself.
	else
		tmp_counter=0; // we're just counting servers connected to us
	for (tmp_count in Clients) {
		if ((Clients[tmp_count] != undefined) &&
		    Clients[tmp_count].server)  {
			if (Clients[tmp_count].local || count_all)
				tmp_counter++;
		}
	}
	return tmp_counter;
}

function count_nicks(count_bit) {
	var tmp_counter = 0;

	if(!count_bit)
		count_bit=USERMODE_NONE;
	for (tmp_count in Clients) {
		if ((Clients[tmp_count] != undefined) && 
		    ((Clients[tmp_count].conntype == TYPE_USER) ||
		     (Clients[tmp_count].conntype == TYPE_USER_REMOTE)) &&
		    (Clients[tmp_count].mode&count_bit) &&
		     (!(Clients[tmp_count].mode&USERMODE_INVISIBLE) ||
		       (count_bit==USERMODE_INVISIBLE) ||
		       (count_bit==USERMODE_OPER) ) )
			tmp_counter++;
	}
	return tmp_counter;
}

function count_local_nicks() {
	var tmp_counter = 0;

	for (tmp_count in Clients) {
		if ((Clients[tmp_count] != undefined) &&
		    !Clients[tmp_count].parent)
			tmp_counter++;
	}
	return tmp_counter;
}

function get_next_clientid() {
	for (tmp_client in Clients) {
		if (!Clients[tmp_client].conntype) {
			return tmp_client;
		}
	}
	return Clients.length + 1;
}

function read_config_file() {
	Admin1 = "";
	Admin2 = "";
	Admin3 = "";
	CLines = new Array();
	HLines = new Array();
	ILines = new Array();
	KLines = new Array();
	NLines = new Array();
	OLines = new Array();
	PLines = new Array();
	QLines = new Array();
	ULines = new Array();
	diepass = "";
	restartpass = "";
	YLines = new Array();
	ZLines = new Array();
	var fname="";
	if (config_filename && config_filename.length)
		fname=system.ctrl_dir + config_filename;
	else {
		fname=system.ctrl_dir + "ircd." + system.local_host_name + ".conf";
		if(!file_exists(fname))
			fname=system.ctrl_dir + "ircd." + system.host_name + ".conf";
		if(!file_exists(fname))
			fname=system.ctrl_dir + "ircd.conf";
	}
	var conf = new File(fname);
	if (conf.open("r")) {
		log("Reading Config: " + fname);
		while (!conf.eof) {
			var conf_line = conf.readln();
			if ((conf_line != null) && conf_line.match("[:]")) {
				var arg = conf_line.split(":");
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
						CLines.push(new CLine(arg[1],arg[2],arg[3],arg[4],parseInt(arg[5])));
						break;
					case "H":
						if (!arg[3])
							break;
						HLines.push(new HLine(arg[1],arg[3]));
						break;
					case "I":
						if (!arg[5])
							break;
						ILines.push(new ILine(arg[1],arg[2],arg[3],arg[4],arg[5]));
						break;
					case "K":
						if (!arg[2])
							break;
						var kline_mask = create_ban_mask(arg[1],true);
						if (!kline_mask) {
							log("!WARNING Invalid K:Line (" + arg[1] + ")");
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
						NLines.push(new NLine(arg[1],arg[2],arg[3],parse_nline_flags(arg[4]),arg[5]));
						break;
					case "O":
						if (!arg[5])
							break;
						OLines.push(new OLine(arg[1],arg[2],arg[3],parse_oline_flags(arg[4]),parseInt(arg[5])));
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
						YLines[parseInt(arg[1])] = new YLine(parseInt(arg[2]),parseInt(arg[3]),parseInt(arg[4]),parseInt(arg[5]));
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
		log ("WARNING! No config file found or unable to open.  Proceeding with defaults.");
	}
	scan_for_klined_clients();
	YLines[0] = new YLine(120,600,1,5050000); // default irc class
}

function create_new_socket(port) {
	log("Creating new socket object on port " + port);
	var newsock = new Socket();
	if(!newsock.bind(port)) {
		log("!Error " + newsock.error + " binding socket to TCP port " + port);
		return 0;
	}
	log(format("%04u ",newsock.descriptor)
		+ "IRC server socket bound to TCP port " + port);
	if(!newsock.listen(5 /* backlog */)) {
		log("!Error " + newsock.error + " setting up socket for listening");
		return 0;
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

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
//////////////   End of functions.  Start main() program here.   //////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

log(VERSION + " started.");

Clients = new Array;
Channels = new Array;

hcc_total = 0;
hcc_users = 0;
hcc_counter = 0;
server_uptime = time();

WhoWasHistory = new Array;
NickHistory = new Array;
whowas_buffer = 1000;
whowas_pointer = 0;
nick_buffer = 1000;
nick_pointer = 0;

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

if(this.js==undefined)			// v3.10?
	js = { terminated: false };

if(this.server==undefined) {	// Running from JSexec?
	if (cmdline_port)
		default_port = cmdline_port;
	else if (mline_port)
		default_port = mline_port;

	server = { socket: false, terminated: false };
	server.socket = create_new_socket(default_port)
	if (!server.socket)
		exit();
}

server.socket.nonblocking = true;	// REQUIRED!
server.socket.debug = false;		// Will spam your log if true :)

// Now open additional listening sockets as defined on the P:Line in ircd.conf
open_plines = new Array();
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

///// Main Loop /////
while (!server.terminated) {

//	mswait(1); // don't peg the CPU

	if(server.terminated)
		break;
	if(js.terminated)
		break;

	// Setup a new socket if a connection is accepted.
	for (pl in open_plines) {
		if (open_plines[pl].poll()) {
			var client_sock=open_plines[pl].accept();
			if(client_sock) {
				if (iszlined(client_sock.remote_ip_address)) {
					client_sock.send(":" + servername + " 465 * :You've been Z:Lined from this server.\r\n");
					client_sock.close();
				} else {
					new_id = get_next_clientid();
					Clients[new_id]=new IRCClient(client_sock,new_id,true,true);
					if(server.client_add != undefined)
						server.client_add(client_sock);
				}
			}
		}
	}

	// Poll all sockets for commands, then pass to the appropriate func.
    poll_clients=new Array;
    poll_client_map=new Array;
    for(thisClient in Clients) {
        ClientObject=Clients[thisClient];
        if ( (ClientObject != undefined) &&
             ClientObject.socket && ClientObject.conntype &&
             ClientObject.local )
		{
			ClientObject.check_timeout();
			if(this.socket_select==undefined)
			{
				ClientObject.work();
			}
             poll_clients.push(ClientObject.socket.descriptor);
             poll_client_map.push(thisClient);
		}
    }
	if(this.socket_select!=undefined)
	{
		readme=socket_select(poll_clients, 1 /* seconds */);
		for(thisPolled in readme)
		{
			Clients[poll_client_map[readme[thisPolled]]].work();
		}
	}

	// Scan C:Lines for servers to connect to automatically.
	var my_cline;
	for(thisCL in CLines) {
		my_cline = CLines[thisCL];
		if (my_cline.port && YLines[my_cline.ircclass].connfreq &&
		    !searchbyserver(my_cline.servername,true) &&
		     ((time() - my_cline.lastconnect) >
		     YLines[my_cline.ircclass].connfreq)
		   ) {
			oper_notice("Routing","Auto-connecting to " + CLines[thisCL].servername);
			connect_to_server(CLines[thisCL]);
		}
	}
}

// End of our run, so terminate everything before we go.
terminate_everything("Terminated.");

//////////////////////////////// END OF MAIN ////////////////////////////////

// Client Object

// FIXME: this whole thing is a mess.  It should be cleaned up and streamlined.
function IRCClient(socket,new_id,local_client,do_newconn) {
	this.pinged=false;
	this.nick = "";
	this.away = "";
	this.realname = "";
	this.uprefix = "";
	this.hostname = "";
	this.conntype = TYPE_UNREGISTERED;
	this.ircclass = 0;
	this.flags = 0;
	this.idletime = "";
	this.invited = "";
	this.password = "";
	this.sentps = false;
	this.local = local_client;
	this.mode = USERMODE_NONE;
	this.socket = socket;
	this.channels = new Array();
	this.searchbyiline=IRCClient_searchbyiline;
	this.quit=IRCClient_Quit;
	this.netsplit=IRCClient_netsplit;
	this.ircnuh getter = function() {
		return(this.nick + "!" + this.uprefix + "@" + this.hostname);
	};
	this.isulined getter = function() {
		for (my_ul in ULines) {
			if (this.nick.toUpperCase() == ULines[my_ul].toUpperCase())
				return 1;
		}
		return 0;
	};
	this.hops=0;
	this.server=false;
	this.hub=false;
	this.servername = servername;
	this.affect_mode_list=IRCClient_affect_mode_list;
	this.tweaktmpmode=IRCClient_tweaktmpmode;
	this.tweaktmpmodelist=IRCClient_tweaktmpmodelist;
	this.rmchan=IRCClient_RMChan;
	this.rawout=IRCClient_rawout;
	this.originatorout=IRCClient_originatorout;
	this.ircout=IRCClient_ircout;
	this.server_notice=IRCClient_server_notice;
	this.setusermode=IRCClient_setusermode;
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
	this.numeric322=IRCClient_numeric322;
	this.numeric351=IRCClient_numeric351;
	this.numeric352=IRCClient_numeric352;
	this.numeric353=IRCClient_numeric353;
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
	this.lusers=IRCClient_lusers;
	this.motd=IRCClient_motd;
	this.names=IRCClient_names;
	this.set_chanmode=IRCClient_set_chanmode;
	this.get_usermode=IRCClient_get_usermode;
	this.onchannel=IRCClient_onchannel;
	this.onchanwith=IRCClient_onchanwith;
	this.num_channels_on=IRCClient_num_channels_on;
	this.bcast_to_uchans_unique=IRCClient_bcast_to_uchans_unique;
	this.check_nickname=IRCClient_check_nickname;
	this.bcast_to_channel=IRCClient_bcast_to_channel;
	this.bcast_to_list=IRCClient_bcast_to_list;
	this.bcast_to_channel_servers=IRCClient_bcast_to_channel_servers;
	this.bcast_to_servers=IRCClient_bcast_to_servers;
	this.bcast_to_servers_raw=IRCClient_bcast_to_servers_raw;
	this.do_join=IRCClient_do_join;
	this.do_part=IRCClient_do_part;
	this.do_whois=IRCClient_do_whois;
	this.do_msg=IRCClient_do_msg;
	this.do_admin=IRCClient_do_admin;
	this.do_info=IRCClient_do_info;
	this.do_stats=IRCClient_do_stats;
	this.do_users=IRCClient_do_users;
	this.do_summon=IRCClient_do_summon;
	this.do_links=IRCClient_do_links;
	this.do_trace=IRCClient_do_trace;
	this.trace_all_opers=IRCClient_trace_all_opers;
	this.do_connect=IRCClient_do_connect;
	this.do_basic_who=IRCClient_do_basic_who;
	this.do_complex_who=IRCClient_do_complex_who;
	this.do_who_usage=IRCClient_do_who_usage;
	this.match_who_mask=IRCClient_match_who_mask;
	this.global=IRCClient_global;
	this.services_msg=IRCClient_services_msg;
	this.part_all=IRCClient_part_all;
	this.server_commands=IRCClient_server_commands;
	this.unregistered_commands=IRCClient_unregistered_commands;
	this.registered_commands=IRCClient_registered_commands;
	this.id=new_id;
	this.created=time();
	this.parent=0;
	this.work=IRCClient_work;
	this.check_timeout=IRCClient_check_timeout;
	this.server_nick_info=IRCClient_server_nick_info;
	this.server_chan_info=IRCClient_server_chan_info;
	this.server_info=IRCClient_server_info;
	this.synchronize=IRCClient_synchronize;
	this.reintroduce_nick=IRCClient_reintroduce_nick;
	this.finalize_server_connect=IRCClient_finalize_server_connect;
	this.ip="";
	this.linkparent="";
	if (this.socket && local_client && do_newconn) {
		log(format("%04u",this.socket.descriptor)
			+ " Accepted new connection: " + this.socket.remote_ip_address
			+ " port " + this.socket.remote_port);
		this.nick = "*";
		this.realname = "";
		this.away = "";
		this.uprefix = "";
		this.mode = USERMODE_NONE;
		if (this.socket.remote_ip_address == "127.0.0.1") {
			this.hostname = servername;
		} else {
			if (debug && resolve_hostnames)
				went_into_hostname = time();
			if (resolve_hostnames)
				possible_hostname = resolve_host(this.socket.remote_ip_address);
			if (resolve_hostnames && possible_hostname) {
				this.hostname = possible_hostname;
				if(server.client_update != undefined)
					server.client_update(this.socket,this.nick, this.hostname);
			} else 
				this.hostname = this.socket.remote_ip_address;
			if (debug && resolve_hostnames) {
				went_into_hostname = time() - went_into_hostname;
				if (went_into_hostname == "NaN")
					went_into_hostname=0;
				oper_notice("DEBUG","resolve_host took " + went_into_hostname + " seconds.");
			}
		}
		if (this.socket.remote_ip_address)
			this.ip = this.socket.remote_ip_address;
		this.connecttime = time();
		this.idletime = time();
		this.talkidle = time();
		this.conntype = TYPE_UNREGISTERED;
		this.server_notice("*** " + VERSION + " (" + serverdesc + ") Ready.");
	}
}

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
function IRCClient_Quit(str,suppress_bcast,is_netsplit,origin) {
	var tmp;

	if (!str)
		str = this.nick;

	if (!this.server) {
		tmp = "QUIT :" + str;
		this.bcast_to_uchans_unique(tmp);
		for(thisChannel in this.channels) {
			this.rmchan(Channels[this.channels[thisChannel]]);
		}
		if (whowas_pointer >= whowas_buffer)
			whowas_pointer = 0;
		if (!this.parent)
			ww_serverdesc = serverdesc;
		else
			ww_serverdesc = Clients[this.parent].realname;
		WhoWasHistory[whowas_pointer] = new WhoWas(this.nick,this.uprefix,this.hostname,this.realname,this.servername,ww_serverdesc);
		whowas_pointer++;
		if (!suppress_bcast && (this.conntype > TYPE_UNREGISTERED) )
			this.bcast_to_servers(tmp);
	} else if (this.server && is_netsplit) {
		this.netsplit(str);
	} else if (this.server && this.local) {
		if (!suppress_bcast)
			this.bcast_to_servers_raw("SQUIT " + this.nick + " :" + str);
		this.netsplit(servername + " " + this.nick);
	} else if (this.server && origin) {
		if (!suppress_bcast)
			this.bcast_to_servers_raw(":" + origin.nick + " SQUIT " + this.nick + " :" + str);
		this.netsplit(origin.nick + " " + this.nick);
	} else { // we should never land here
		oper_notice("Notice","Netspliting a server which isn't local and doesn't have an origin.");
		if (!suppress_bcast)
			this.bcast_to_servers_raw("SQUIT " + this.nick + " :" + str);
		this.netsplit();
	}

	if((server.client_remove!=undefined) && !this.sentps && this.local)
		server.client_remove(this.socket);

	if (this.local) {
		this.rawout("ERROR :Closing Link: [" + this.uprefix + "@" + this.hostname + "] (" + str + ")");
		oper_notice("Notice","Client exiting: " + this.nick + " (" + this.uprefix + "@" + this.hostname + ") [" + str + "] [" + this.ip + "]");
		this.socket.close();
	}

	this.conntype=TYPE_EMPTY;
	delete this.nick;
	delete this.socket;
	delete Clients[this.id];
}

function IRCClient_netsplit(ns_reason) {
	if (!ns_reason)
		ns_reason = "net.split.net.split net.split.net.split";
	for (sqclient in Clients) {
		if (Clients[sqclient] && (Clients[sqclient] != null) &&
		    (Clients[sqclient] != undefined)) {
			if ((Clients[sqclient].servername == this.nick) ||
			    (Clients[sqclient].linkparent == this.nick))
				Clients[sqclient].quit(ns_reason,true,true);
		}
	}
}

function IRCClient_synchronize() {
	this.rawout("BURST"); // warn of impending synchronization
	for (my_server in Clients) {
		if ((Clients[my_server].conntype == TYPE_SERVER) &&
		    (Clients[my_server].id != this.id) ) {
			this.server_info(Clients[my_server]);
		}
	}
	for (my_client in Clients) {
		if ((Clients[my_client].conntype == TYPE_USER) ||
		    (Clients[my_client].conntype == TYPE_USER_REMOTE)) {
			this.server_nick_info(Clients[my_client]);
			if (Clients[my_client].away) 
				this.rawout(":" + Clients[my_client].nick + " AWAY :" + Clients[my_client].away);
		}
	}
	for (my_channel in Channels) {
		if (my_channel[0] == "#") {
			this.server_chan_info(Channels[my_channel]);
		}
	}
	oper_notice("Routing","from " + servername + ": " + this.nick + " has processed user/channel burst, sending topic burst.");
	for (my_channel in Channels) {
		if ((my_channel[0] == "#") && Channels[my_channel].topic) {
			var chan = Channels[my_channel];
			this.rawout("TOPIC " + chan.nam + " " + chan.topicchangedby + " " + chan.topictime + " :" + chan.topic);
		}
	}
	this.rawout("BURST 0"); // burst completed.
	oper_notice("Routing","from " + servername + ": " + this.nick + " has processed topic burst (synched to network data).");
}

function IRCClient_server_info(sni_server) {
	var realhops = parseInt(sni_server.hops)+1;
	this.rawout(":" + sni_server.linkparent + " SERVER " + sni_server.nick + " " + realhops + " :" + sni_server.realname);
}

function IRCClient_server_nick_info(sni_client) {
	this.rawout("NICK " + sni_client.nick + " " + sni_client.hops + " " + sni_client.created + " " + sni_client.get_usermode() + " " + sni_client.uprefix + " " + sni_client.hostname + " " + sni_client.servername + " 0 " + ip_to_int(sni_client.ip) + " :" + sni_client.realname);
}

function IRCClient_reintroduce_nick(nick) {
	var chan;
	var cmodes;

	if (!this.server || !nick)
		return 0;

	this.server_nick_info(nick);

	if (nick.away)
		this.rawout(":" + nick.nick + " AWAY :" + nick.away);

	for (uchan in nick.channels) {
		cmodes = "";
		if (nick.channels[uchan]) {
			chan = Channels[nick.channels[uchan]];
			if (chan.ismode(nick.id,CHANLIST_OP))
				cmodes += "@";
			if (chan.ismode(nick.id,CHANLIST_VOICE))
				cmodes += "+";
			this.rawout("SJOIN " + chan.created + " " + chan.nam + " " + chan.chanmode(true) + " :" + cmodes + nick.nick);
			if (chan.topic)
				this.rawout("TOPIC " + chan.nam + " " + chan.topicchangedby + " " + chan.topictime + " :" + chan.topic);
		}
	}
}

function IRCClient_server_chan_info(sni_chan) {
	this.rawout("SJOIN " + sni_chan.created + " " + sni_chan.nam + " " + sni_chan.chanmode(true) + " :" + sni_chan.occupants())
	var modecounter=0;
	var modestr="+";
	var modeargs="";
	for (aBan in sni_chan.modelist[CHANLIST_BAN]) {
		modecounter++;
		modestr += "b";
		if (modeargs)
			modeargs += " ";
		modeargs += sni_chan.modelist[CHANLIST_BAN][aBan];
		if (modecounter >= max_modes) {
			this.ircout("MODE " + sni_chan.nam + " " + modestr + " " + modeargs);
			modecounter=0;
			modestr="+";
			modeargs="";
		}
	}
	if (modeargs)
		this.ircout("MODE " + sni_chan.nam + " " + modestr + " " + modeargs);
}

function IRCClient_RMChan(rmchan_obj) {
	if (!rmchan_obj)
		return 0;
	for (k in rmchan_obj.users) {
		if (rmchan_obj.users[k] == this.id)
			delete rmchan_obj.users[k]
	}
	for(j in this.channels) {
		if (this.channels[j] == rmchan_obj.nam.toUpperCase())
			delete this.channels[j];
	}
	rmchan_obj.del_modelist(this.id,CHANLIST_OP);
	rmchan_obj.del_modelist(this.id,CHANLIST_VOICE);
	if (!rmchan_obj.count_users()) {
		delete rmchan_obj.users;
		delete rmchan_obj.mode;
		delete Channels[rmchan_obj.nam.toUpperCase()];
	}
}

//////////////////// Output Helper Functions ////////////////////
function IRCClient_rawout(str) {
	if (debug)
		log(format("[RAW->%s]: %s",this.nick,str));
	if(this.conntype && this.local) {
		this.socket.send(str + "\r\n");
	} else if (this.conntype && this.parent) {
		if ((str[0] == ":") && str[0].match(["!"])) {
			str_end = str.slice(str.indexOf(" ")+1);
			str_beg = str.slice(0,str.indexOf("!"));
			str = str_beg + " " + str_end;
		}
		Clients[this.parent].socket.send(str + "\r\n");
	}
}

function IRCClient_originatorout(str,origin) {
	if (debug)
		log(format("[%s->%s]: %s",origin.nick,this.nick,str));
	if((this.conntype == TYPE_USER) && this.local && !this.server) {
		this.socket.send(":" + origin.ircnuh + " " + str + "\r\n");
	} else if (this.conntype && this.parent) {
		Clients[this.parent].socket.send(":" + origin.nick + " " + str + "\r\n");
	} else if (this.conntype && this.server) {
		this.socket.send(":" + origin.nick + " " + str + "\r\n");
	}
}

function IRCClient_ircout(str) {
	if (debug)
		log(format("[%s->%s]: %s",servername,this.nick,str));
	if(this.conntype && this.local) {
		this.socket.send(":" + servername + " " + str + "\r\n");
	} else if (this.conntype && this.parent) {
		Clients[this.parent].socket.send(":" + servername + " " + str + "\r\n");
	}
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
	this.numeric(206, "Serv " + ircclass + " " + sint + "S " + cint + "C *!*@" + server);
}

function IRCClient_numeric208(type,clientname) {
	this.numeric(208, type + " 0 " + clientname);
}

function IRCClient_numeric261(file) {
	this.numeric(261, "File " + file + " " + debug);
}

function IRCClient_numeric322(chan) {
	var channel_name;

	if ((chan.mode&CHANMODE_PRIVATE) && !(this.mode&USERMODE_OPER) &&
	    !this.onchannel(chan.nam.toUpperCase) )
		channel_name = "Priv";
	else
		channel_name = chan.nam;

	if (!(chan.mode&CHANMODE_SECRET) || (this.mode&USERMODE_OPER) ||
	    this.onchannel(chan.nam.toUpperCase()) )
		this.numeric(322, channel_name + " " + chan.count_users() + " :" + chan.topic);
}

function IRCClient_numeric351() {
	this.numeric(351, VERSION + " " + servername + " :" + VERSION_STR);
}

function IRCClient_numeric352(user,chan) {
	var who_mode="";
	var disp;

	if (!chan)
		disp = "*";
	else
		disp = chan.nam;

	if (user.away)
		who_mode += "G";
	else
		who_mode += "H";
	if (chan) {
		if (chan.ismode(user.id,CHANLIST_OP))
			who_mode += "@";
		else if (chan.ismode(user.id,CHANLIST_VOICE))
			who_mode += "+";
	}
	if (user.mode&USERMODE_OPER)
		who_mode += "*";
	this.numeric(352, disp + " " + user.uprefix + " " + user.hostname + " " + user.servername + " " + user.nick + " " + who_mode + " :" + user.hops + " " + user.realname);
}

function IRCClient_numeric353(chan, str) {
	// = public @ secret * everything else
	if (Channels[chan].mode&CHANMODE_SECRET)
		var ctype_str = "@";
	else
		var ctype_str = "=";
	this.numeric("353", ctype_str + " " + Channels[chan].nam + " :" + str);
}

function IRCClient_numeric391() {
	this.numeric(391, servername + " :" + strftime("%A %B %d %Y -- %H:%M %z",time()));
}

function IRCClient_numeric401(str) {
	this.numeric("401", str + " :No such nick/channel.");
}

function IRCClient_numeric402(str) {
	this.numeric(402, str + " :No such server.");
}

function IRCClient_numeric403(str) {
	this.numeric("403", str + " :No such channel or invalid channel designation.");
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
	this.numeric("481", ":Permission Denied.  You do not have the correct IRC operator privileges.");
}

function IRCClient_numeric482(tmp_chan_nam) {
	this.numeric("482", tmp_chan_nam + " :You're not a channel operator.");
}

//////////////////// Multi-numeric Display Functions ////////////////////

function IRCClient_lusers() {
	this.numeric("251", ":There are " + count_nicks() + " users and " + count_nicks(USERMODE_INVISIBLE) + " invisible on " + count_servers(true) + " servers.");
	this.numeric("252", count_nicks(USERMODE_OPER) + " :IRC operators online.");
	this.numeric("253", "0 :unknown connection(s).");
	this.numeric("254", count_channels() + " :channels formed.");
	this.numeric("255", ":I have " + count_local_nicks() + " clients and " + count_servers(false) + " servers.");
	this.numeric("250", ":Highest connection count: " + hcc_total + " (" + hcc_users + " clients.)");
	this.server_notice(hcc_counter + " clients have connected since " + strftime("%a %b %e %H:%M:%S %Y %Z",server_uptime));
}

function IRCClient_motd() {
	motd_file = new File(system.text_dir + "ircmotd.txt");
	if (motd_file.exists==false)
		this.numeric(422, ":MOTD file missing: " + motd_file.name);
	else if (motd_file.open("r")==false)
		this.numeric(424, ":MOTD error " + errno + " opening: " + motd_file.name);
	else {
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
	for(thisChannel_user in Channels[chan].users) {
		Channel_user=Clients[Channels[chan].users[thisChannel_user]];
		if (Channel_user.nick &&
		    (Channel_user.conntype != TYPE_SERVER) && (
		    !(Channel_user.mode&USERMODE_INVISIBLE) ||
		    (this.onchannel(chan)) ) ) {
			if (numnicks)
				tmp += " ";
			if (Channels[chan].ismode(Channel_user.id,CHANLIST_OP))
				tmp += "@";
			else if (Channels[chan].ismode(Channel_user.id,CHANLIST_VOICE))
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

function IRCClient_onchannel(chan_str) {
	if (!chan_str || !Channels[chan_str])
		return 0;
	for (k in Channels[chan_str].users) {
		if (Channels[chan_str].users[k] == this.id) {
			return 1;
		}
	}
	return 0; // if we got this far, we failed.
}

// Traverse each channel the user is on and see if target is on any of the
// same channels.
function IRCClient_onchanwith(target) {
	for (c in this.channels) {
		for (i in target.channels) {
			if (this.channels[c] == target.channels[i])
				return 1; // success
		}
	}
	return 0; // failure.
}

function IRCClient_num_channels_on() {
	var uchan_counter=0;
	for(tmp_uchan in this.channels) {
		if (this.channels[tmp_uchan])
			uchan_counter++;
	}
	return uchan_counter;
}

//////////////////// Auxillary Functions ////////////////////

function server_bcast_to_servers(str) {
	for(thisClient in Clients) {
		if ((Clients[thisClient].conntype == TYPE_SERVER) &&
		    (Clients[thisClient].hops == 1)) {
			Clients[thisClient].rawout(str,this);
		}
	}
}

function IRCClient_bcast_to_servers(str) {
	for(thisClient in Clients) {
		if ((Clients[thisClient].conntype == TYPE_SERVER) &&
		    (Clients[thisClient].id != this.parent) &&
		    (Clients[thisClient].id != this.id) &&
		    (Clients[thisClient].parent != this.id) &&
		    (Clients[thisClient].hops == 1)) {
			Clients[thisClient].originatorout(str,this);
		}
	}
}

function IRCClient_bcast_to_servers_raw(str) {
	for(thisClient in Clients) {
		if ((Clients[thisClient].conntype == TYPE_SERVER) &&
		    (Clients[thisClient].id != this.parent) &&
		    (Clients[thisClient].id != this.id) &&
		    (Clients[thisClient].parent != this.id) &&
		    (Clients[thisClient].hops == 1)) {
			Clients[thisClient].rawout(str);
		}
	}
}

function IRCClient_bcast_to_uchans_unique(str) {
	already_bcast = new Array();
	for(thisChannel in this.channels) {
		userchannel=this.channels[thisChannel];
		if(userchannel) {
			for (j in Channels[userchannel].users) {
				did_i_already_broadcast = false;
				for (alb in already_bcast) {
					if (already_bcast[alb] == Channels[userchannel].users[j]) {
						did_i_already_broadcast = true;
						break;
					}
				}
				if (!did_i_already_broadcast &&
				    (Channels[userchannel].users[j] != this.id) &&
				     Channels[userchannel].users[j] &&
				     !Clients[Channels[userchannel].users[j]].server &&
				     !Clients[Channels[userchannel].users[j]].parent ) {
					Clients[Channels[userchannel].users[j]].originatorout(str,this);
					already_bcast[already_bcast.length] = Channels[userchannel].users[j];
				}
			}
		}
	}
}

function IRCClient_bcast_to_list(chan, str, bounce, list_bit) {
	if (chan != undefined) {
		for (thisUser in chan.users) {
			aUser = chan.users[thisUser];
			if (aUser && ( aUser != this.id || (bounce) ) &&
			    ( chan.ismode(aUser,list_bit) ) )
				Clients[aUser].originatorout(str,this);
		}
	} else {
		this.numeric401(chan.nam);
	}
}

function IRCClient_bcast_to_channel(chan_str, str, bounce) {
	chan_tuc = chan_str.toUpperCase();
	if (Channels[chan_tuc] != undefined) {
		for(thisUser in Channels[chan_tuc].users) {
			aUser=Channels[chan_tuc].users[thisUser];
			if (aUser && ( aUser != this.id || (bounce) ) &&
			    !Clients[aUser].parent && !Clients[aUser].server ) {
				Clients[aUser].originatorout(str,this);
			}
		}
	} else if (!this.parent && !this.server) {
		this.numeric401(chan_str);
	}
}

function IRCClient_bcast_to_channel_servers(chan_str, str) {
	var sent_to_servers = new Array();
	var chan_tuc = chan_str.toUpperCase();
	if (Channels[chan_tuc] != undefined) {
		for(thisUser in Channels[chan_tuc].users) {
			aUser=Channels[chan_tuc].users[thisUser];
			if (aUser && ( Clients[aUser].parent ) &&
			    !sent_to_servers[Clients[aUser].parent] &&
			    (Clients[aUser].parent != this.parent) ) {
				Clients[aUser].originatorout(str,this);
				sent_to_servers[Clients[aUser].parent] = true;
			}
		}
	}
}

function IRCClient_check_nickname(newnick) {
	var qline_nick;
	var checknick;
	var regexp;

	newnick = newnick.slice(0,max_nicklen);
	// If you're trying to NICK to yourself, drop silently.
	if(newnick == this.nick)
		return 0;
	// First, check for valid characters.
	regexp="^[A-Za-z\{\}\`\^\_\|\\]\\[\\\\][A-Za-z0-9\-\{\}\`\^\_\|\\]\\[\\\\]*$";
	if(!newnick.match(regexp)) {
		if (!this.server)
			this.numeric("432", newnick + " :Foobar'd Nickname.");
		return 0;
	}
	// Second, check for existing nickname.
	checknick = searchbynick(newnick);
	if(checknick && (checknick.nick != this.nick) ) {
		if (!this.server)
			this.numeric("433", newnick + " :Nickname is already in use.");
		return 0;
	}
	// Third, match against Q:Lines
	for (ql in QLines) {
		qline_nick = QLines[ql].nick;
		qline_nick = qline_nick.replace(/[?]/g,".");
		qline_nick = qline_nick.replace(/[*]/g,".*?");
		regexp = new RegExp("^" + qline_nick + "$","i");
		if(newnick.match(regexp)) {
			if (!this.server)
				this.numeric(432, newnick + " :" + QLines[ql].reason);
			return 0;
		}
	}
	return 1; // if we made it this far, we're good!
}

function IRCClient_do_whois(wi) {
	if (!wi)
		return 0;
	this.numeric(311, wi.nick + " " + wi.uprefix + " " + wi.hostname + " * :" + wi.realname);
	var userchans="";
	for (i in wi.channels) {
		if(wi.channels[i] &&
		   (!(Channels[wi.channels[i]].mode&CHANMODE_SECRET ||
		      Channels[wi.channels[i]].mode&CHANMODE_PRIVATE) ||
		     this.onchannel(wi.channels[i]) ||
		     this.mode&USERMODE_OPER)) {
			if (userchans)
				userchans += " ";
			if (Channels[wi.channels[i]].ismode(wi.id,CHANLIST_OP))
				userchans += "@";
			else if (Channels[wi.channels[i]].ismode(wi.id,CHANLIST_VOICE))
				userchans += "+";
			userchans += Channels[wi.channels[i]].nam;
		}
	}
	if (userchans)
		this.numeric(319, wi.nick + " :" + userchans);
	if (wi.local)
		this.numeric(312, wi.nick + " " + servername + " :" + serverdesc);
	else {
		wi_server = searchbyserver(wi.servername);
		this.numeric(312, wi.nick + " " + wi_server.nick + " :" + wi_server.realname);
	}
	if (wi.mode&USERMODE_OPER)
		this.numeric(313, wi.nick + " :is an IRC operator.");
	if (wi.away)
		this.numeric(301, wi.nick + " :" + wi.away);
	if (wi.local)
		this.numeric(317, wi.nick + " " + (time() - wi.talkidle) + " " + wi.connecttime + " :seconds idle, signon time");
}

function IRCClient_services_msg(svcnick,send_str) {
	var service_nickname;
	var service_server;

	if (!send_str) {
		this.numeric412();
		return 0;
	}
	// First, make sure the nick exists.
	service_nickname = searchbynick(svcnick);
	if (!service_nickname) {
		this.numeric440(svcnick);
		return 0;
	}
	service_server = searchbyserver(service_nickname.servername);
	if (!service_server || !service_server.isulined) {
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
	for(globClient in Clients) {
		var Client = Clients[globClient];
		if (target[0] == "#")
			var global_match = Client.hostname;
		else // assume $
			var global_match = Client.servername;
		if ((Client.conntype == TYPE_USER) && Client.local &&
		    match_irc_mask(global_match,global_mask) )
			Client.originatorout(global_str,this);
	}
	global_str = ":" + this.nick + " " + global_str;
	if(this.parent)
		Clients[this.parent].bcast_to_servers_raw(global_str);
	else
		server_bcast_to_servers(global_str);
	return 1;
}

function IRCClient_do_msg(target,type_str,send_str) {
	if ((target[0] == "$") && (this.mode&USERMODE_OPER))
		return this.global(target,type_str,send_str);

	send_to_list = -1;
	if (target[0] == "@" && ( (target[1] == "#") || target[1] == "&") ) {
		send_to_list = CHANLIST_OP;
		target = target.slice(1);
	} else if (target[0]=="+" && ((target[1] == "#")|| target[1] == "&")) {
		send_to_list = CHANLIST_VOICE;
		target = target.slice(1);
	}
		
	if ((target[0] == "#") || (target[0] == "&")) {
		chan = searchbychannel(target);
		if (!chan) {
			// check to see if it's a #*hostmask* oper message
			if ((target[0] == "#") && (this.mode&USERMODE_OPER)) {
				return this.global(target,type_str,send_str);
			} else {
				this.numeric401(target);
				return 0;
			}
		}
		if ( (chan.mode&CHANMODE_NOOUTSIDE) &&
		     !this.onchannel(chan.nam.toUpperCase())) {
			this.numeric(404, chan.nam + " :Cannot send to channel (+n: no outside messages)");
			return 0;
		}
		if ( (chan.mode&CHANMODE_MODERATED) &&
		     !chan.ismode(this.id,CHANLIST_VOICE) &&
		     !chan.ismode(this.id,CHANLIST_OP) ) {
			this.numeric(404, chan.nam + " :Cannot send to channel (+m: moderated)");
			return 0;
		}
		if (chan.isbanned(this.ircnuh) &&
		   !chan.ismode(this.id,CHANLIST_VOICE) &&
		   !chan.ismode(this.id,CHANLIST_OP) ) {
			this.numeric(404, chan.nam + " :Cannot send to channel (+b: you're banned!)");
			return 0;
		}
		if(send_to_list == -1) {
			str = type_str +" "+ chan.nam +" :"+ send_str;
			this.bcast_to_channel(chan.nam, str, false);
			this.bcast_to_channel_servers(chan.nam, str);
		} else {
			if (send_to_list == CHANLIST_OP)
				prefix_chr="@";
			else if (send_to_list == CHANLIST_VOICE)
				prefix_chr="+";
			str = type_str +" " + prefix_chr + chan.nam + " :"+ send_str;
			this.bcast_to_list(chan, str, false, send_to_list);
			this.bcast_to_channel_servers(chan.nam, str);
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
				this.numeric(407, target + " :Duplicate recipients, no message delivered.");
				return 0;
			}
			target = msg_arg[0] + "@" + msg_arg[1];
		} else {
			var real_target = target;
		}
		target_socket = searchbynick(real_target);
		if (target_socket) {
			if (target_server &&
			    (target_server.parent != target_socket.parent)) {
				this.numeric401(target);
				return 0;
			}
			if (target_server && 
			    (target_server.id == target_socket.parent) )
				target = real_target;
			str = type_str + " " + target + " :" + send_str;
			target_socket.originatorout(str,this);
			if (target_socket.away && (type_str == "PRIVMSG") &&
			    !this.server && this.local)
				this.numeric(301, target_socket.nick + " :" + target_socket.away);
		} else {
			this.numeric401(target);
			return 0;
		}
	}
	return 1;
}

function IRCClient_do_admin() {
	if (Admin1 && Admin2 && Admin3) {
		this.numeric(256, ":Administrative info about " + servername);
		this.numeric(257, ":" + Admin1);
		this.numeric(258, ":" + Admin2);
		this.numeric(259, ":" + Admin3);
	} else {
		this.numeric(423, servername + " :No administrative information available.");
	}
}

function IRCClient_do_info() {
	this.numeric(371, ":" + VERSION + " Copyright 2003 Randy Sommerfeld.");
	this.numeric(371, ":" + system.version_notice + " " + system.copyright + ".");
	this.numeric(371, ": ");
	this.numeric(371, ":--- A big thanks to the following for their assistance: ---");
	this.numeric(371, ":     Deuce: Hacking and OOP conversions.");
	this.numeric(371, ":DigitalMan: Additional hacking and API stuff.");
	this.numeric(371, ": ");
	this.numeric(371, ":Synchronet " + system.full_version);
	this.numeric(371, ":Running on " + system.os_version);
	this.numeric(371, ":Compiled with " + system.compiled_with + " at " + system.compiled_when);
	this.numeric(371, ":Socket Library: " + system.socket_lib);
	this.numeric(371, ":Message Base Library: " + system.msgbase_lib);
	this.numeric(371, ":JavaScript Library: " + system.js_version);
	this.numeric(371, ":This BBS has been up since " + system.timestr(system.uptime));
	this.numeric(371, ": ");
	this.numeric(371, ":This program is free software; you can redistribute it and/or modify");
	this.numeric(371, ":it under the terms of the GNU General Public License as published by");
	this.numeric(371, ":the Free Software Foundation; either version 2 of the License, or");
	this.numeric(371, ":(at your option) any later version.");
	this.numeric(371, ": ");
	this.numeric(371, ":This program is distributed in the hope that it will be useful,");
	this.numeric(371, ":but WITHOUT ANY WARRANTY; without even the implied warranty of");
	this.numeric(371, ":MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the");
	this.numeric(371, ":GNU General Public License for more details:");
	this.numeric(371, ":http://www.gnu.org/licenses/gpl.txt");
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
				this.numeric(213,"C " + CLines[cline].host + " * " + CLines[cline].servername + " " + cline_port + " " + CLines[cline].ircclass);
				if (NLines[cline])
					this.numeric(214,"N " + NLines[cline].host + " * " + NLines[cline].servername + " " + NLines[cline].flags + " " + NLines[cline].ircclass);
			}
			break;  
		case "H":
		case "h":
			for (hl in HLines) {
				this.numeric(244,"H " + HLines[hl].allowedmask + " * " + HLines[hl].servername);
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
				this.numeric(215,"I " + ILines[iline].ipmask + " * " + ILines[iline].hostmask + " " + my_port + " " + ILines[iline].ircclass);
			}
			break;
		case "K":
		case "k":
			for (kline in KLines) {
				if(KLines[kline].hostmask)
					this.numeric(216, KLines[kline].type + " " + KLines[kline].hostmask + " * * :" + KLines[kline].reason);
			}
			break;
		case "L":
			this.numeric(241,"L <hostmask> * <servername> <maxdepth>");
			break;
		case "l":
			this.numeric(211,"<linkname> <sendq> <sentmessages> <sentbytes> <receivedmessages> <receivedbytes> <timeopen>");
			break;
		case "M":
		case "m":
			this.numeric(212,"<command> <count>");
			break;          
		case "O":       
		case "o":
			for (oline in OLines) {
				this.numeric(243, "O " + OLines[oline].hostmask + " * " + OLines[oline].nick);
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
			var str = format("Server Up %u days %u:%02u:%02u",
				updays,uphours,upmins,upsec);
			this.numeric(242,":" + str);
			break;
		case "Y":
		case "y":
			var yl;
			for (thisYL in YLines) {
				yl = YLines[thisYL];
				this.numeric(218,"Y " + thisYL + " " + yl.pingfreq + " " + yl.connfreq + " " + yl.maxlinks + " " + yl.sendq);
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
			this.numeric(393,format(':%-25s %-9s %-30s',u.alias,'Node'+node,u.host_name));
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
		if(system.node_list[node].status == NODE_INUSE &&
		   system.node_list[node].useron == usernum)
			isonline=1;
		}
		if(!isonline)
			this.numeric(444,":User not logged in.");
		else {
//			var summon_message=ircstring(cmdline);
//			if(summon_message != '')
//				var summon_message=' ('+summon_message+')';
//			system.put_telegram('^G^G'+usernum,this.nick+' is summoning you to IRC chat'+summon_message+'\r\n');
			system.put_telegram(usernum,''+this.nick+' is summoning you to IRC chat.\r\n');
			this.numeric(342,summon_user+' :Summoning user to IRC');
		}
	}
}

function IRCClient_do_links(mask) {
	if (!mask)
		mask = "*";
	for(thisServer in Clients) {
		var Server=Clients[thisServer];
		if (Server && (Server.conntype == TYPE_SERVER) &&
		    match_irc_mask(Server.nick,mask) )
			this.numeric(364, Server.nick + " " + Server.linkparent + " :" + Server.hops + " " + Server.realname);
	}
	if (match_irc_mask(servername,mask))
		this.numeric(364, servername + " " + servername + " :0 " + serverdesc);
	this.numeric(365, mask + " :End of /LINKS list.");
}

// Don't hunt for servers based on nicknames, as TRACE is more explicit.
function IRCClient_do_trace(target) {
	var server;
	var nick;

	if (target.match(/[.]/)) { // probably a server
		server = searchbyserver(target);
		if (server == -1) { // we hunted ourselves
			// FIXME: What do these numbers mean? O_o?
			this.numeric206("30","1","0",servername);
			this.trace_all_opers();
		} else if (server) {
			server.rawout(":" + this.nick + " TRACE " + target);
			this.numeric200(target,server.nick);
			return 0;
		} else {
			this.numeric402(target);
			return 0;
		}
	} else { // user.
		nick = searchbynick(target);
		if (nick.local) {
			if (nick.mode&USERMODE_OPER)
				this.numeric204(nick);
			else
				this.numeric205(nick);
		} else if (nick) {
			nick.rawout(":" + this.nick + " TRACE " + target);
			this.numeric200(target,Clients[nick.parent].nick);
			return 0;
		} else {
			this.numeric402(target);
			return 0;
		}
	}
	this.numeric(262, target + " :End of /TRACE.");
}

function IRCClient_trace_all_opers() {
	for(thisoper in Clients) {
		var oper=Clients[thisoper];
		if ((oper.mode&USERMODE_OPER) && !oper.parent)
			this.numeric204(oper);
	}
}

function IRCClient_do_connect(con_server,con_port) {
	var con_cline = "";
	for (ccl in CLines) {
		if (match_irc_mask(CLines[ccl].servername,con_server) ||
		    match_irc_mask(CLines[ccl].host,con_server) ) {
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
	var con_type = "Local";
	if (this.parent)
		con_type = "Remote";
	oper_notice("Routing","from " + servername + ": " + con_type + " CONNECT " + con_cline.servername + " " + con_port + " from " + this.nick + "[" + this.uprefix + "@" + this.hostname + "]");
	connect_to_server(con_cline,con_port);
	return 1;
}

function IRCClient_do_basic_who(whomask) {
	var eow = "*";

	if ((whomask[0] == "#") || (whomask[0] == "&")) {
		var chan = whomask.toUpperCase();
		if (Channels[chan] != undefined) {
			var channel=Channels[chan].nam;
			for(i in Channels[chan].users) {
				if (Channels[chan].users[i])
					this.numeric352(Clients[Channels[chan].users[i]],Channels[chan]);
			}
			eow = Channels[chan].nam;
		}
	} else {
		for (i in Clients) {
			if (Clients[i] && !Clients[i].server &&
			    ((Clients[i].conntype > 1) &&
			     (Clients[i].conntype < 4)) &&
			    Clients[i].match_who_mask(whomask))
				this.numeric352(Clients[i]);
		}
		eow = whomask;
	}
	this.numeric(315, eow + " :End of /WHO list. (Basic)");
}

function IRCClient_do_complex_who(cmd) {
	var who = new Who();
	var tmp;
	var eow = "*";
	var add;
	var arg = 1;
	var whomask = "";
	var chan;

	if (cmd[2].toLowerCase() == "o") { // Compatibility with RFC1459.
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
			default:
				break;
		}
	}

	// Check to see if the user passed a generic mask to us for processing.
	arg++;
	if (cmd[arg])
		whomask = cmd[arg];

	// allow +c/-c to override.
	if (!who.Channel && ((whomask[0] == "#") || (whomask[0] == "&")))
		who.Channel = whomask;

	// Now we traverse everything and apply the criteria the user passed.
	var who_count = 0;
	for (who_client in Clients) {
		if (Clients[who_client] && !Clients[who_client].server &&
		    ( (Clients[who_client].conntype > 1) &&
		      (Clients[who_client].conntype < 4) ) ) {
			var wc = Clients[who_client];

			var flag_M = this.onchanwith(wc);

			// Don't even bother if the target is +i and the
			// user isn't an oper or on a channel with the target.
			if ( (wc.mode&USERMODE_INVISIBLE) &&
			     (!(this.mode&USERMODE_OPER) ||
			      !flag_M) )
				continue;

			if ((who.add_flags&WHO_AWAY) && !wc.away)
				continue;
			else if ((who.del_flags&WHO_AWAY) && wc.away)
				continue;
			if ((who.add_flags&WHO_CHANNEL) &&
			    !wc.onchannel(who.Channel.toUpperCase()))
				continue;
			else if ((who.del_flags&WHO_CHANNEL) &&
			    wc.onchannel(who.Channel.toUpperCase()))
				continue;
			if ((who.add_flags&WHO_REALNAME) &&
			    !match_irc_mask(wc.realname,who.RealName))
				continue;
			else if ((who.del_flags&WHO_REALNAME) &&
			    match_irc_mask(wc.realname,who.RealName))
				continue;
			if ((who.add_flags&WHO_HOST) &&
			    !match_irc_mask(wc.hostname,who.Host))
				continue;
			else if ((who.del_flags&WHO_HOST) &&
			    match_irc_mask(wc.hostname,who.Host))
				continue;
			if ((who.add_flags&WHO_IP) &&
			    !match_irc_mask(wc.ip,who.IP))
				continue;
			else if ((who.del_flags&WHO_IP) &&
			    match_irc_mask(wc.ip,who.IP))
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
							if (
							   (!madd && (wc.mode&
							   USERMODE_OPER))
							||
							   (madd && !(wc.mode&
							   USERMODE_OPER))
							   )
								sic = true;
							break;
						case "i":
							if (
							   (!madd && (wc.mode&
							   USERMODE_INVISIBLE))
							||
							   (madd && !(wc.mode&
							   USERMODE_INVISIBLE))
							   )
								sic = true;
							break;
						default:
							break;
					}
				}
				if (sic)
					continue;
			}
			if ((who.add_flags&WHO_NICK) &&
			    !match_irc_mask(wc.nick,who.Nick))
				continue;
			else if ((who.del_flags&WHO_NICK) &&
			    match_irc_mask(wc.nick,who.Nick))
				continue;
			if ((who.add_flags&WHO_OPER) &&
			    !(wc.mode&USERMODE_OPER))
				continue;
			else if ((who.del_flags&WHO_OPER) &&
			    (wc.mode&USERMODE_OPER))
				continue;
			if ((who.add_flags&WHO_SERVER) &&
			    !match_irc_mask(wc.servername,who.Server))
				continue;
			else if ((who.del_flags&WHO_SERVER) &&
			    match_irc_mask(wc.servername,who.Server))
				continue;
			if ((who.add_flags&WHO_USER) &&
			    !match_irc_mask(wc.uprefix,who.User))
				continue;
			else if ((who.del_flags&WHO_USER) &&
			    match_irc_mask(wc.uprefix,who.User))
				continue;
			if ((who.add_flags&WHO_MEMBER_CHANNEL) && !flag_M)
				continue;
			else if ((who.del_flags&WHO_MEMBER_CHANNEL) && flag_M)
				continue;

			if (whomask && !wc.match_who_mask(whomask))
				continue;       

			chan = "";
			if ((who.add_flags&WHO_FIRST_CHANNEL) && !who.Channel) {
				for (x in wc.channels) {
					if(wc.channels[x] &&
					   (!(Channels[wc.channels[x]].mode&
					       CHANMODE_SECRET ||
					      Channels[wc.channels[x]].mode&
					       CHANMODE_PRIVATE
					      ) ||
						this.onchannel(wc.channels[x])||
						this.mode&USERMODE_OPER)
					   ) {
						chan = Channels[wc.channels[x]].nam;
						break;
					}
				}
			}

			if (who.Channel)
				chan = who.Channel;

			// If we made it this far, we're good.
			if (chan && Channels[chan.toUpperCase()])
				this.numeric352(wc,Channels[chan.toUpperCase()]);
			else
				this.numeric352(wc);
			who_count++;
		}
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
		if ( match_irc_mask(this.nick,mask.split("!")[0]) &&
		     match_irc_mask(this.uprefix,mask.slice(mask.indexOf("!")+1).split("@")[0]) &&
		     match_irc_mask(this.hostname,mask.slice(mask.indexOf("@")+1)) )
			return 1;
	} else if (mask.match(/[@]/)) { // user@host
		if ( match_irc_mask(this.uprefix,mask.split("@")[0]) &&
		     match_irc_mask(this.hostname,mask.split("@")[1]) )
			return 1;
	} else if (mask.match(/[.]/)) { // host only
		if ( match_irc_mask(this.hostname,mask) )
			return 1;
	} else { // must be a nick?
		if ( match_irc_mask(this.nick,mask) )
			return 1;
	}
	return 0;
}

function IRCClient_do_who_usage() {
	this.numeric(334,":/WHO [+|-][acghimnosuCM] <args>");
	this.numeric(334,":The modes as above work exactly like channel modes.");
	this.numeric(334,":a       : User is away.");
	this.numeric(334,":c <chan>: User is on <channel>, no wildcards.");
	this.numeric(334,":g <rnam>: Check against realname field, wildcards allowed.");
	this.numeric(334,":h <host>: Check user's hostname, wildcards allowed.");
	this.numeric(334,":i <ip>  : Check against IP address, wildcards allowed.");
	this.numeric(334,":m <umde>: User has <umodes> set, -+ allowed.");
	this.numeric(334,":n <nick>: User's nickname matches <nick>, wildcards allowed.");
	this.numeric(334,":o       : User is an IRC Operator.");
	this.numeric(334,":s <srvr>: User is on <server>, wildcards allowed.");
	this.numeric(334,":u <user>: User's username field matches, wildcards allowed.");
	this.numeric(334,":C       : Only display first channel that the user matches.");
	this.numeric(334,":M       : Only check against channels you're a member of.");
	this.numeric(315,"? :End of /WHO list. (Usage)");
}

function IRCClient_do_join(chan_name,join_key) {
	if((chan_name[0] != "#") && (chan_name[0] != "&") && !this.parent) {
		this.numeric403(chan_name);
		return 0;
	}
	for (theChar in chan_name) {
		var theChar_code = chan_name[theChar].charCodeAt(0);
		if ((theChar_code <= 32) || (theChar_code == 44) ||
		    (chan_name[theChar].charCodeAt(0) == 160)) {
			if (!this.parent)
				this.numeric(479, chan_name + " :Channel name contains illegal characters.");
			return 0;
		}
	}
	if (this.onchannel(chan_name.toUpperCase()))
		return 0;
	if ((this.num_channels_on() >= max_user_chans) && !this.parent) {
		this.numeric("405", chan_name + " :You have joined too many channels.");
		return 0;
	}
	var chan = chan_name.toUpperCase().slice(0,max_chanlen);
	if (Channels[chan] != undefined) {
		if (!this.parent) {
			if ((Channels[chan].mode&CHANMODE_INVITE) &&
			    (chan != this.invited)) {
				this.numeric("473", Channels[chan].nam + " :Cannot join channel (+i: invite only)");
				return 0;
			}
			if ((Channels[chan].mode&CHANMODE_LIMIT) &&
			    (Channels[chan].count_users() >= Channels[chan].arg[CHANMODE_LIMIT])) {
				this.numeric("471", Channels[chan].nam + " :Cannot join channel (+l: channel is full)");
				return 0;
			}
			if ((Channels[chan].mode&CHANMODE_KEY) &&
			    (Channels[chan].arg[CHANMODE_KEY] != join_key)) {
				this.numeric("475", Channels[chan].nam + " :Cannot join channel (+k: key required)");
				return 0;
			}
			if (Channels[chan].isbanned(this.ircnuh) &&
			    (chan != this.invited) ) {
				this.numeric("474", Channels[chan].nam + " :Cannot join channel (+b: you're banned!)");
				return 0;
			}
		}
		// add to existing channel
		Channels[chan].users.push(this.id);
		var str="JOIN :" + Channels[chan].nam;
		if (this.parent)
			this.bcast_to_channel(Channels[chan].nam, str, false);
		else
			this.bcast_to_channel(Channels[chan].nam, str, true);
		if (chan_name[0] != "&")
			server_bcast_to_servers(":" + this.nick + " SJOIN " + Channels[chan].created + " " + Channels[chan].nam);
	} else {
		// create a new channel
		Channels[chan]=new Channel(chan);
		Channels[chan].nam=chan_name.slice(0,max_chanlen);
		Channels[chan].mode=CHANMODE_NONE;
		Channels[chan].topic="";
		Channels[chan].created=time();
		Channels[chan].users = new Array();
		Channels[chan].users[0] = this.id;
		Channels[chan].modelist[CHANLIST_BAN] = new Array();
		Channels[chan].modelist[CHANLIST_VOICE] = new Array();
		Channels[chan].modelist[CHANLIST_OP] = new Array();
		Channels[chan].modelist[CHANLIST_OP].push(this.id);
		str="JOIN :" + chan_name;
		this.originatorout(str,this);
		if (chan_name[0] != "&")
			server_bcast_to_servers(":" + servername + " SJOIN " + Channels[chan].created + " " + Channels[chan].nam + " " + Channels[chan].chanmode() + " :@" + this.nick);
	}
	if (this.invited.toUpperCase() == Channels[chan].nam.toUpperCase())
		this.invited = "";
	this.channels.push(chan);
	if (!this.parent) {
		this.names(chan);
		this.numeric(366, Channels[chan].nam + " :End of /NAMES list.");
	}
	return 1; // success
}

function IRCClient_do_part(chan_name) {
	var chan;
	var str;

	if((chan_name[0] != "#") && (chan_name[0] != "&") && !this.parent) {
		this.numeric403(chan_name);
		return 0;
	}
	chan = chan_name.toUpperCase();
	if (Channels[chan] != undefined) {
		if (this.onchannel(chan)) {
			str = "PART " + Channels[chan].nam;
			if (this.parent)
				this.bcast_to_channel(Channels[chan].nam, str, false);
			else
				this.bcast_to_channel(Channels[chan].nam, str, true);
			this.rmchan(Channels[chan]);
			if (chan_name[0] != "&")
				this.bcast_to_servers(str);
		} else if (!this.parent) {
			this.numeric442(chan_name);
		}
	} else if (!this.parent) {
		this.numeric403(chan_name);
	}
}

function IRCClient_part_all() {
	var partingChannel;

	for(thisChannel in this.channels) {
		partingChannel=this.channels[thisChannel];
		this.do_part(Channels[partingChannel].nam);
	}
}

function IRCClient_get_usermode() {
	var tmp_mode = "+";

	if (this.mode&USERMODE_INVISIBLE)
		tmp_mode += "i";
	if (this.mode&USERMODE_OPER)
		tmp_mode += "o";

	return tmp_mode;
}

function IRCClient_set_chanmode(chan,modeline,bounce_modes) {
	if (!chan || !modeline)
		return;

	cm_args = modeline.split(' ');

	add=true;
	addbits=CHANMODE_NONE;
	delbits=CHANMODE_NONE;

	state_arg = new Array();
	state_arg[CHANMODE_KEY] = "";
	state_arg[CHANMODE_LIMIT] = "";

	chan_tmplist=new Array();
	chan_tmplist[CHANLIST_OP]=new Array();
	chan_tmplist[CHANLIST_DEOP]=new Array();
	chan_tmplist[CHANLIST_VOICE]=new Array();
	chan_tmplist[CHANLIST_DEVOICE]=new Array();
	chan_tmplist[CHANLIST_BAN]=new Array();
	chan_tmplist[CHANLIST_UNBAN]=new Array();
	mode_counter=0;
	mode_args_counter=1; // start counting at the args, not the modestring

	for (modechar in cm_args[0]) {
		mode_counter++;
		switch (cm_args[0][modechar]) {
			case "+":
				add=true;
				mode_counter--;
				break;
			case "-":
				add=false;
				mode_counter--;
				break;
			case "b":
				if(add && (cm_args.length <= mode_args_counter)) {
					addbits|=CHANMODE_BAN; // list bans
					break;
				}
				this.tweaktmpmodelist(CHANLIST_BAN,CHANLIST_UNBAN,chan);
				mode_args_counter++;
				break;
			case "i":
				this.tweaktmpmode(CHANMODE_INVITE,chan);
				break;
			case "k":
				if(cm_args.length > mode_args_counter) {
					this.tweaktmpmode(CHANMODE_KEY,chan);
					state_arg[CHANMODE_KEY]=cm_args[mode_args_counter];
					mode_args_counter++;
				}
				break;
			case "l":
				if (add && (cm_args.length > mode_args_counter)) {
					this.tweaktmpmode(CHANMODE_LIMIT,chan);
					regexp = "^[0-9]{1,4}$";
					if(cm_args[mode_args_counter].match(regexp))
						state_arg[CHANMODE_LIMIT]=cm_args[mode_args_counter];
					mode_args_counter++;
				} else if (!add) {
					this.tweaktmpmode(CHANMODE_LIMIT,chan);
					if (cm_args.length > mode_args_counter)
						mode_args_counter++;
				}
				break;
			case "m":
				this.tweaktmpmode(CHANMODE_MODERATED,chan);
				break;
			case "n":
				this.tweaktmpmode(CHANMODE_NOOUTSIDE,chan);
				break;
			case "o":
				if (cm_args.length <= mode_args_counter)
					break;
				this.tweaktmpmodelist(CHANLIST_OP,CHANLIST_DEOP,chan);
				mode_args_counter++;
				break;
			case "p":
				if( (add && !(chan.mode&CHANMODE_SECRET) ||
				     (delbits&CHANMODE_SECRET) ) || (!add) )
					this.tweaktmpmode(CHANMODE_PRIVATE,chan);
				break;
			case "s":
				if( (add && !(chan.mode&CHANMODE_PRIVATE) ||
				     (delbits&CHANMODE_PRIVATE) ) || (!add) )
					this.tweaktmpmode(CHANMODE_SECRET,chan);
				break;
			case "t":
				this.tweaktmpmode(CHANMODE_TOPIC,chan);
				break;
			case "v":
				if (cm_args.length <= mode_args_counter)
					break;
				this.tweaktmpmodelist(CHANLIST_VOICE,CHANLIST_DEVOICE,chan);
				mode_args_counter++;
				break;
			default:
				if ((!this.parent) && (!this.server))
					this.numeric("472", cm_args[0][modechar] + " :is unknown mode char to me.");
				mode_counter--;
				break;
		}
		if (mode_counter == max_modes)
			break;
	}

	addmodes = "";
	delmodes = "";
	addmodeargs = "";
	delmodeargs = "";

	// If we're bouncing modes, traverse our side of what the modes look
	// like and remove any modes not mentioned by what was passed to the
	// function.  Or, clear any ops, voiced members, or bans on the 'bad'
	// side of the network sync.  FIXME: Bans get synchronized later.
	if (bounce_modes) {
		for (cm in MODE) {
			if (MODE[cm].state && (chan.mode&cm) && !(addbits&cm)) {
				delbits |= cm;
			} else if (MODE[cm].list) {
				for (member in chan.modelist[MODE[cm].list]) {
					delmodes += MODE[cm].modechar;
					delmodeargs += " " + Clients[chan.modelist[MODE[cm].list][member]].nick;
					chan.del_modelist(chan.modelist[MODE[cm].list][member],MODE[cm].list);
				}
			}
		}
	}

	// Now we run through all the mode toggles and construct our lists for
	// later display.  We also play with the channel bit switches here.
	for (cm in MODE) {
		if (MODE[cm].state) {
			if ((cm&CHANMODE_KEY) && (addbits&CHANMODE_KEY) && 
			    state_arg[cm] && chan.arg[cm] && !this.server &&
			    !this.parent && !bounce_modes) {
				this.numeric(467, chan.nam + " :Channel key already set.");
			} else if ((addbits&cm) && !(chan.mode&cm)) {
				addmodes += MODE[cm].modechar;
				chan.mode |= cm;
				if (MODE[cm].args && MODE[cm].state) {
					addmodeargs += " " + state_arg[cm];
					chan.arg[cm] = state_arg[cm];
				}
			} else if ((delbits&cm) && (chan.mode&cm)) {
				delmodes += MODE[cm].modechar;
				chan.mode &= ~cm;
				if (MODE[cm].args && MODE[cm].state) {
					delmodeargs += " " + state_arg[cm];
					chan.arg[cm] = "";
				}
			}
		}
	}

	// This is a special case, if +b was passed to us without arguments,
	// we simply display a list of bans on the channel.
	if (addbits&CHANMODE_BAN) {
		for (the_ban in chan.modelist[CHANLIST_BAN]) {
			this.numeric(367, chan.nam + " " + chan.modelist[CHANLIST_BAN][the_ban] + " " + chan.bancreator[the_ban] + " " + chan.bantime[the_ban]);
		}
		this.numeric(368, chan.nam + " :End of Channel Ban List.");
	}

	// Now we play with the channel lists by adding or removing what was
	// given to us on the modeline.
	this.affect_mode_list(CHANLIST_OP,chan)
	this.affect_mode_list(CHANLIST_VOICE,chan);
	this.affect_mode_list(CHANLIST_BAN,chan);

	if (!addmodes && !delmodes)
		return 0;

	final_modestr = "";

	if (addmodes)
		final_modestr += "+" + addmodes;
	if (delmodes)
		final_modestr += "-" + delmodes;
	if (addmodeargs)
		final_modestr += addmodeargs;
	if (delmodeargs)
		final_modestr += delmodeargs;

	var final_args = final_modestr.split(' ');
	var arg_counter = 0;
	var mode_counter = 0;
	var mode_output = "";
	var f_mode_args = "";
	for (marg in final_args[0]) {
		switch (final_args[0][marg]) {
			case "+":
				mode_output += "+";
				add = true;
				break;
			case "-":
				mode_output += "-";
				add = false;
				break;
			case "l":
				if (!add) {
					mode_counter++;
					mode_output += final_args[0][marg];
					break;
				}
			case "b": // only modes with arguments
			case "k":
			case "l":
			case "o":
			case "v":
				arg_counter++;
				f_mode_args += " " + final_args[arg_counter];
			default:
				mode_counter++;
				mode_output += final_args[0][marg];
				break;
		}
		if (mode_counter >= max_modes) {
			var str = "MODE " + chan.nam + " " + mode_output + f_mode_args;
			if (!this.server)
				this.bcast_to_channel(chan.nam, str, true);
			else
				this.bcast_to_channel(chan.nam, str, false);
			if (chan.nam[0] != "&")
				this.bcast_to_servers(str);

			if (add)
				mode_output = "+";
			else
				mode_output = "-";
			f_mode_args = "";
		}
	}

	if (mode_output.length > 1) {
		str = "MODE " + chan.nam + " " + mode_output + f_mode_args;
		if (!this.server)
			this.bcast_to_channel(chan.nam, str, true);
		else
			this.bcast_to_channel(chan.nam, str, false);
		if (chan.nam[0] != "&")
			this.bcast_to_servers(str);
	}

	return 1;
}

function IRCClient_setusermode(modestr) {
	if (!modestr)
		return 0;
	add=true;
	unknown_mode=false;
	addbits=USERMODE_NONE;
	delbits=USERMODE_NONE;
	for (modechar in modestr) {
		switch (modestr[modechar]) {
			case "+":
				add=true;
				break;
			case "-":
				add=false;
				break;
			case "i":
				if(add) {
					addbits|=USERMODE_INVISIBLE;
					delbits&=~USERMODE_INVISIBLE;
				} else {
					addbits&=~USERMODE_INVISIBLE;
					delbits|=USERMODE_INVISIBLE;
				}
				break;
			case "o":
				// Allow +o only by servers or non-local users.
				if (add && (this.parent) &&
				    Clients[this.parent].hub) {
					addbits|=USERMODE_OPER;
					delbits&=~USERMODE_OPER;
				}
				if(!add) {
					addbits&=~USERMODE_OPER;
					delbits|=USERMODE_OPER;
				}
				break;
			default:
				if (!unknown_mode && !this.parent) {
					this.numeric("501", ":Unknown MODE flag");
					unknown_mode=true;
				}
				break;
		}
	}
	addmodes = "";
	delmodes = "";
	if ((addbits&USERMODE_INVISIBLE) && !(this.mode&USERMODE_INVISIBLE)) {
		addmodes += "i";
		this.mode |= USERMODE_INVISIBLE;
	} else if ((delbits&USERMODE_INVISIBLE) && (this.mode&USERMODE_INVISIBLE)) {
		delmodes += "i";
		this.mode &= ~USERMODE_INVISIBLE;
	}
	if ((addbits&USERMODE_OPER) && !(this.mode&USERMODE_OPER)) {
		addmodes += "o";
		this.mode |= USERMODE_OPER;
	} else if ((delbits&USERMODE_OPER) && (this.mode&USERMODE_OPER)) {
		delmodes += "o";
		this.mode &= ~USERMODE_OPER;
	}
	if (!addmodes && !delmodes)
		return 0;
	final_modestr = "";
	if (addmodes)
		final_modestr += "+" + addmodes;
	if (delmodes)
		final_modestr += "-" + delmodes;
	if (!this.parent) {
		str = "MODE " + this.nick + " " + final_modestr;
		this.originatorout(str,this);
		this.bcast_to_servers(str);
	}
	return 1;
}

function IRCClient_affect_mode_list(list_bit,chan) {
	for (x=list_bit;x<=(list_bit+1);x++) {
		for (tmp_index in chan_tmplist[x]) {
			if (list_bit >= CHANLIST_BAN) {
				if (x == CHANLIST_BAN) {
					var set_ban = create_ban_mask(chan_tmplist[x][tmp_index]);
					if (chan.count_modelist(CHANLIST_BAN) >= max_bans) {
						this.numeric(478, chan.nam + " " + set_ban + " :Cannot add ban, channel's ban list is full.");
					} else if (set_ban && !chan.isbanned(set_ban)) {
						addmodes += "b";
						addmodeargs += " " + set_ban;
						var banid = chan.add_modelist(set_ban,CHANLIST_BAN);
						chan.bantime[banid] = time();
						chan.bancreator[banid] = this.ircnuh;
					}
				} else if (x == CHANLIST_UNBAN) {
					for (ban in chan.modelist[CHANLIST_BAN]) {
						if (chan_tmplist[CHANLIST_UNBAN][tmp_index].toUpperCase() == chan.modelist[CHANLIST_BAN][ban].toUpperCase()) {
							delmodes += "b";
							delmodeargs += " " + chan_tmplist[CHANLIST_UNBAN][tmp_index];
							var banid = chan.del_modelist(chan_tmplist[CHANLIST_UNBAN][tmp_index],CHANLIST_BAN);
							delete chan.bantime[banid];
							delete chan.bancreator[banid];
						}
					}
				}
			} else {
				var tmp_nick = searchbynick(chan_tmplist[x][tmp_index]);
				if (!tmp_nick)
					tmp_nick = searchbynick(search_nickbuf(chan_tmplist[x][tmp_index]));
				if (tmp_nick) { // FIXME: check for user existing on channel?
					if ((x == list_bit) && (!chan.ismode(tmp_nick.id,list_bit))) {
						addmodes += MODECHAR[list_bit];
						addmodeargs += " " + tmp_nick.nick;
						chan.add_modelist(tmp_nick.id,x);
					} else if (chan.ismode(tmp_nick.id,list_bit)) {
						delmodes += MODECHAR[list_bit];
						delmodeargs += " " + tmp_nick.nick;
						chan.del_modelist(tmp_nick.id,list_bit);
					}
				} else {
					this.numeric401(chan_tmplist[x][tmp_index]);
				}
			}
		}
	}
}

//////////////////// Command Parsers ////////////////////

// Unregistered users are ConnType 1
function IRCClient_unregistered_commands(command, cmdline) {
	if (command.match(/^[0-9]+/))
		return 0; // we ignore all numerics from unregistered clients.
	cmd=cmdline.split(' ');
	switch(command) {
		case "CAPAB":
			break; // silently ignore for now.
		case "NICK":
			if (!cmd[1]) {
				this.numeric(431, ":No nickname given.");
				break;
			}
			the_nick = ircstring(cmd[1]).slice(0,max_nicklen);
			if (this.check_nickname(the_nick))
				this.nick = the_nick;
			break;
		case "PASS":
			if (!cmd[1])
				break;
			this.password = cmd[1];
			break;
		case "PING":
			if (!cmd[1]) {
				this.numeric(409,":No origin specified.");
				break;
			}
			this.ircout("PONG " + servername + " :" + ircstring(cmdline));
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
				    match_irc_mask(cmd[1],NLines[nl].servername)) {
					if (check_qwk_passwd(qwkid,this.password)) {
						this_nline = NLines[nl];
						break;
					}
				} else if ((NLines[nl].flags&NLINE_CHECK_WITH_QWKMASTER) &&
				           match_irc_mask(cmd[1],NLines[nl].servername)) {
						for (qwkm_nl in NLines) {
							if (NLines[qwkm_nl].flags&NLINE_IS_QWKMASTER) {
								var qwk_master = searchbyserver(NLines[qwkm_nl].servername);
								if (!qwk_master) {
									this.quit("No QWK master available for authorization.",true);
									return 0;
								} else {
									qwk_master.rawout(":" + servername + " PASS " + this.password + " :" + qwkid + " QWK");
									qwk_slave = true;
								}
							}
						}
				} else if ((NLines[nl].password == this.password) &&
				           (match_irc_mask(cmd[1],NLines[nl].servername))
				          ) {
						this_nline = NLines[nl];
						break;
				}
			}
			if ((!this_nline || ((this_nline.password == "*") &&
			    !this.sentps)) && !qwk_slave) {
				this.quit("ERROR: Server not configured.",true);
				return 0;
			}
			if (searchbyserver(cmd[1])) {
				this.quit("ERROR: Server already exists.",true);
				return 0;
			}
			this.nick = cmd[1].toLowerCase();
			this.hops = cmd[2];
			this.realname = ircstring(cmdline);
			this.linkparent = servername;
			this.parent = this.id;
			this.flags = this_nline.flags;
			if (!qwk_slave) { // qwk slaves should never be hubs.
				this.server = true;
				this.conntype = TYPE_SERVER;
				for (hl in HLines) {
					if (HLines[hl].servername.toUpperCase()
					    == this.nick.toUpperCase()) {
						this.hub = true;
						break;
					}
				}
			}
			break;
		case "USER":
			if (this.server) {
				this.numeric462();
				break;
			}
			if (!cmd[4]) {
				this.numeric461("USER");
				break;
			}
			this.realname = ircstring(cmdline).slice(0,50);
			var unreg_username = parse_username(cmd[1]);
			this.uprefix = "~" + unreg_username;
			break;
		case "QUIT":
			this.quit(ircstring(cmdline),true);
			return;
		default:
			this.numeric451();
			break;
	}
	if (this.uprefix && isklined(this.uprefix + "@" + this.hostname)) {
		this.numeric(465, ":You've been K:Lined from this server.");
		this.quit("You've been K:Lined from this server.",true);
		return 0;
	}
	if (this.password && (unreg_username || (this.nick != "*")) &&
	    !this.server) {
		if (unreg_username)
			var usernum = system.matchuser(unreg_username);
		if (!usernum && (this.nick != "*"))
			var usernum = system.matchuser(this.nick);
		if (usernum) {
			bbsuser = new User(usernum);
			if (this.password.toUpperCase() == bbsuser.security.password)
				this.uprefix = parse_username(bbsuser.handle).toLowerCase().slice(0,10);
		}
	}
	if ( (count_local_nicks() + count_servers(false)) > hcc_total)
		hcc_total = count_local_nicks() + count_servers(false);
	if (count_local_nicks() > hcc_users)
		hcc_users = count_local_nicks();
	if (this.realname && this.uprefix && (this.nick != "*") &&
	    !this.server) {
		// Check for a valid I:Line.
		var tmp_iline;
		tmp_iline = this.searchbyiline();
		if (!tmp_iline) {
			this.numeric(463, ":Your host isn't among the privileged.");
			this.quit("You are not authorized to use this server.",true);
			return 0;
		}
		if (tmp_iline.password) {
			if (tmp_iline.password != this.password) {
				this.numeric(464, ":Password Incorrect.");
				this.quit("Denied.",true);
				return 0;
			}
		}
		// We meet registration criteria. Continue.
		this.ircclass = tmp_iline.ircclass;
		hcc_counter++;
		this.conntype = TYPE_USER;
		this.numeric("001", ":Welcome to the Synchronet IRC Service, " + this.ircnuh);
		this.numeric("002", ":Your host is " + servername + ", running " + VERSION);
		this.numeric("003", ":This server was created " + strftime("%a %b %e %Y at %H:%M:%S %Z",server_uptime));
		this.numeric("004", servername + " " + VERSION + " oi biklmnopstv");
		this.numeric("005", "MODES=" + max_modes + " MAXCHANNELS=" + max_user_chans + " CHANNELLEN=" + max_chanlen + " MAXBANS=" + max_bans + " NICKLEN=" + max_nicklen + " TOPICLEN=" + max_topiclen + " KICKLEN=" + max_kicklen + " CHANTYPES=#& PREFIX=(ov)@+ NETWORK=Synchronet CASEMAPPING=ascii CHANMODES=b,k,l,imnpst STATUSMSG=@+ :are available on this server.");
		this.lusers();
		this.motd();
		oper_notice("Notice","Client connecting: " + this.nick + " (" + this.uprefix + "@" + this.hostname + ") [" + this.socket.remote_ip_address + "] {1}");
		if (server.client_update != undefined)
			server.client_update(this.socket, this.nick, this.hostname);
		server_bcast_to_servers("NICK " + this.nick + " 1 " + this.created + " " + this.get_usermode() + " " + this.uprefix + " " + this.hostname + " " + servername + " 0 " + ip_to_int(this.ip) + " :" + this.realname);
	/// Otherwise, it's a server trying to connect.
	} else if (this.nick.match("[.]") && this.hops && this.realname &&
		   this.server && (this.conntype == TYPE_SERVER)) {
			this.finalize_server_connect("TS");
	}
}

// Registered users are ConnType 2
function IRCClient_registered_commands(command, cmdline) {
	if (command.match(/^[0-9]+/))
		return 0; // ignore numerics from clients.
	cmd=cmdline.split(' ');
	switch(command) {
		case "ADMIN":
			if (cmd[1]) {
				if (cmd[1][0] == ":")
					cmd[1] = cmd[1].slice(1);
				var dest_server = searchbyserver(cmd[1]);
				if (!dest_server) {
					this.numeric402(cmd[1]);
					break;
				}
				if (dest_server != -1) {
					dest_server.rawout(":" + this.nick + " ADMIN :" + dest_server.nick);
					break;
				}
			}
			this.do_admin();
			break;
		case "AWAY":
			if (cmd[1] && (cmd[1] != ":") ) {
				this.away=ircstring(cmdline).slice(0,80);
				this.numeric("306", ":You have been marked as being away.");
				this.bcast_to_servers("AWAY :" + this.away);
			} else {
				this.away="";
				this.numeric("305", ":You are no longer marked as being away.");
				this.bcast_to_servers("AWAY");
			}
			break;
		case "CONNECT":
			if (!((this.mode&USERMODE_OPER) &&
			      (this.flags&OLINE_CAN_LSQUITCON) )) {
				this.numeric481();
				break;
			}
			if (!cmd[1]) {
				this.numeric461("CONNECT");
				break;
			}
			if (cmd[3]) {
				var dest_server = searchbyserver(cmd[3]);
				if (!dest_server) {
					this.numeric402(cmd[3]);
					break;
				} else if (dest_server != -1) {
					if (!(this.flags&OLINE_CAN_GSQUITCON)) {
						this.numeric481();
						break;
					}
					dest_server.rawout(":" + this.nick + " CONNECT " + cmd[1] + " " + cmd[2] + " " + dest_server.nick);
					break;
				}
			}
			this.do_connect(cmd[1],cmd[2]);
			break;
		case "DEBUG":
			if (!((this.mode&USERMODE_OPER) &&
			      (this.flags&OLINE_CAN_DEBUG))) {
				this.numeric481();
				break;
			}
			if (!cmd[1]) {
				this.server_notice("Usage:");
				this.server_notice("  DEBUG D       - Toggle DEBUG mode on/off");
				this.server_notice("  DEBUG Y <val> - Set yield frequency to <val>");
				this.server_notice("  DEBUG U       - Dump all users stored in mem");
				this.server_notice("  DEBUG C       - Dump all channels stored in mem");
				break;
			}
			switch (cmd[1][0].toUpperCase()) {
				case "D":
					if (debug) {
						debug=false;
						oper_notice("Notice","Debug mode disabled by " + this.ircnuh);
					} else {
						debug=true;
						oper_notice("Notice","Debug mode enabled by " + this.ircnuh);
					}
					break;
				case "Y":
					if (cmd[2]) {
						oper_notice("Notice","branch.yield_freq set to " + cmd[2] + " by " + this.ircnuh);
						branch.yield_freq = parseInt(cmd[2]);
					}
					break;
				case "U":
					for (myuser in Clients) {
						if (Clients[myuser]) {
							this.server_notice(Clients[myuser].nick+","+Clients[myuser].local+","+Clients[myuser].server+","+Clients[myuser].parent+","+Clients[myuser].id+","+Clients[myuser].conntype);
						}
					}
					break;
				case "C":
					for (mychan in Channels) {
						if (Channels[mychan]) {
							this.server_notice(Channels[mychan].nam+","+Channels[mychan].mode+","+Channels[mychan].users);
						}
					}
				default:
					break;
			}
			break;
		case "EVAL":	/* Evaluate a JavaScript expression */
			if (!((this.mode&USERMODE_OPER) &&
			      (this.flags&OLINE_CAN_DEBUG))) {
				this.numeric481();
				break;
			}
			cmd.shift();
			var exp = cmd.join(' ');	/* expression */
			this.server_notice("Evaluating: " + exp);
			try {
				this.server_notice("Result: " + eval(exp));
			} catch(e) {
				this.server_notice("!" + e);
			}
			break;
		case "INFO":
			if (cmd[1]) {
				if (cmd[1][0] == ":")
					cmd[1] = cmd[1].slice(1);
				var dest_server = searchbyserver(cmd[1]);
				if (!dest_server) {
					this.numeric402(cmd[1]);
					break;
				}
				if (dest_server != -1) {
					dest_server.rawout(":" + this.nick + " INFO :" + dest_server.nick);
					break;
				}
			}
			this.do_info();
			break;
		case "INVITE":
			if (!cmd[2]) {
				this.numeric461("INVITE");
				break;
			}
			chanid = searchbychannel(cmd[2]);
			if (!chanid) {
				this.numeric403(cmd[2]);
				break;
			}
			if (!chanid.ismode(this.id,CHANLIST_OP)) {
				this.numeric482(chanid.nam);
				break;
			}
			nickid = searchbynick(cmd[1]);
			if (!nickid) {
				this.numeric401(cmd[1]);
				break;
			}
			if (nickid.onchannel(chanid.nam.toUpperCase())) {
				this.numeric(443, nickid.nick + " " + chanid.nam + " :is already on channel.");
				break;
			}
			this.numeric("341", nickid.nick + " " + chanid.nam);
			nickid.originatorout("INVITE " + nickid.nick + " :" + chanid.nam,this);
			nickid.invited=chanid.nam.toUpperCase();
			break;
		case "ISON":
			if (!cmd[1])
				break; // drop silently
			if (cmd[1][0] == ":")
				cmd[1] = cmd[1].slice(1);
			isonstr = ":";
			for(ison in cmd) {
				if (ison) {
					ison_nick_id = searchbynick(cmd[ison]);
					if (ison_nick_id) {
						if (isonstr != ":")
							isonstr += " ";
						isonstr += ison_nick_id.nick;
					}
				}
			}
			this.numeric("303", isonstr);
			break;
		case "JOIN":
			if (!cmd[1]) {
				this.numeric461("JOIN");
				break;
			}
			if (cmd[1][0] == ":")
				cmd[1]=cmd[1].slice(1);
			var the_channels = cmd[1].split(",");
			var the_keys = "";
			if (cmd[2])
				the_keys = cmd[2].split(",");
			var key_counter = 0;
			for(jchan in the_channels) {
				regexp = "^[0]{1,}$" // 0 is a special case.
				if(the_channels[jchan].match(regexp)) {
					this.part_all();
				} else {
					if (Channels[the_channels[jchan].toUpperCase()] && the_keys[key_counter] && (Channels[the_channels[jchan].toUpperCase()].mode&CHANMODE_KEY)) {
						this.do_join(the_channels[jchan].slice(0,max_chanlen),the_keys[key_counter])
						key_counter++;
					} else {
						this.do_join(the_channels[jchan].slice(0,max_chanlen),"");
					}
				}
			}
			break;
		case "KICK":
			if (!cmd[2]) {
				this.numeric461("KICK");
				break;
			}
			chanid = searchbychannel(cmd[1]);
			if (!chanid) {
				this.numeric403(cmd[1]);
				break;
			}
			if (!chanid.ismode(this.id,CHANLIST_OP)) {
				this.numeric482(chanid.nam);
				break;
			}
			var nickid = searchbynick(cmd[2]);
			if (!nickid) {
				nickid = searchbynick(search_nickbuf(cmd[2]));
				if (!nickid) {
					this.numeric401(cmd[2]);
					break;
				}
			}
			if (!nickid.onchannel(chanid.nam.toUpperCase())) {
				this.numeric("441", nickid.nick + " " + chanid.nam + " :They aren't on that channel!");
				break;
			}
			if (cmd[3])
				kick_reason = ircstring(cmdline).slice(0,max_kicklen);
			else
				kick_reason = this.nick;
			str = "KICK " + chanid.nam + " " + nickid.nick + " :" + kick_reason;
			this.bcast_to_channel(chanid.nam, str, true);
			this.bcast_to_servers(str);
			nickid.rmchan(Channels[chanid.nam.toUpperCase()]);
			break;
		case "KILL":
			if (!(this.mode&USERMODE_OPER)) {
				this.numeric481();
				break;
			}
			if (!cmd[2]) {
				this.numeric461("KILL");
				break;
			}
			if (cmd[1].match(/[.]+/)) {
				this.numeric("483", ":You can't kill a server!");
				break;
			}
			if (cmd[2] == ":") {
				this.numeric("461", command + " :You MUST specify a reason for /KILL.");
				break;
			}
			reason = ircstring(cmdline);
			kills = cmd[1].split(",");
			for(kill in kills) {
				target = searchbynick(kills[kill]);
				if (!target)
					target = searchbynick(search_nickbuf(kills[kill]));
				if (target && target.local) {
					target.quit("Local kill by " + this.nick + " (" + reason + ")");
				} else if (target) {
					server_bcast_to_servers(":" + this.nick + " KILL " + target.nick + " :" + reason);
					target.quit("Killed (" + this.nick + " (" + reason + "))",true);
				} else {
					this.numeric401(kills[kill]);
				}
			}
			break;
		case "KLINE":
			if (!((this.mode&USERMODE_OPER) &&
			      (this.flags&OLINE_CAN_KLINE))) {
				this.numeric481();
				break;
			}
			if (!cmd[2]) {
				this.numeric461("KLINE");
				break;
			}
			kline_mask = create_ban_mask(cmd[1],true);
			if (!kline_mask) {
				this.server_notice("Invalid K:Line mask.");
				break;
			}
			if (isklined(kline_mask)) {
				this.server_notice("K:Line already exists!");
				break;
			}
			KLines.push(new KLine(kline_mask,ircstring(cmdline),"k"));
			oper_notice("Notice", this.nick + " added temporary 99 min. k-line for [" + kline_mask + "] [0]");
			scan_for_klined_clients();
			break;
		case "UNKLINE":
			if (!((this.mode&USERMODE_OPER) &&
			      (this.flags&OLINE_CAN_UNKLINE))) {
				this.numeric481();
				break;
			}
			if (!cmd[1]) {
				this.numeric461("UNKLINE");
				break;
			}
			kline_mask = create_ban_mask(cmd[1],true);
			if (!kline_mask) {
				this.server_notice("Invalid K:Line mask.");
				break;
			}
			if (!isklined(kline_mask)) {
				this.server_notice("No such K:Line.");
				break;
			}
			remove_kline(kline_mask);
			oper_notice("Notice", this.nick + " has removed the K-Line for: [" + kline_mask + "] (1 matches)");
			break;
		case "LINKS":
			if (!cmd[1]) { // *
				this.do_links();
				break;
			} else if (cmd[2]) { // <remote-server> <mask>
				var dest_server = searchbyserver(cmd[1]);
				if (!dest_server) {
					this.numeric402(cmd[1]);
					break;
				}
				if (dest_server != -1) {
					dest_server.rawout(":" + this.nick + " LINKS " + dest_server.nick + " " + cmd[2]);
					break;
				}
			} else if (cmd[1]) { // <mask>
				this.do_links(cmd[1]);
				break;
			}
			break;
		case "LIST":
			var split_chans;

			this.numeric("321", "Channel :Users  Name");

			if (cmd[1]) {
				split_chans = cmd[1].split(',');
				for(chan in split_chans) {
					if (Channels[split_chans[chan].toUpperCase()])
						this.numeric322(Channels[split_chans[chan].toUpperCase()]);
				}
			} else {
				for(chan in Channels) {
					this.numeric322(Channels[chan]);
				}
			}

			this.numeric("323", "End of /LIST.");
			break;
		case "LUSERS":
			this.lusers();
			break;
		case "MODE":
			var chan;

			if (!cmd[1])
				break; // silently ignore
			if (!cmd[2]) {
				// information only
				if ((cmd[1][0] == "#") || (cmd[1][0] == "&")) {
					chan=cmd[1].toUpperCase();
					if (Channels[chan] != undefined) {
						if(this.onchannel(Channels[chan].nam.toUpperCase()))
							this.numeric("324", Channels[chan].nam + " " + Channels[chan].chanmode(true));
						else
							this.numeric("324", Channels[chan].nam + " " + Channels[chan].chanmode(false));
						this.numeric("329", Channels[chan].nam + " " + Channels[chan].created);
					} else {
						this.numeric401(cmd[1]);
					}
				} else {
					// getting the umode
					if (cmd[1].toUpperCase() ==
					    this.nick.toUpperCase())
						this.numeric(221, this.get_usermode());
					else if (searchbynick(cmd[1]))
						this.numeric(502, ":Can't view mode for other users.");
					else
						this.numeric401(cmd[1]);
				}
			} else {
				if ((cmd[1][0] == "#") || (cmd[1][0] == "&")) {
					chan = searchbychannel(cmd[1])
					if (!chan) {
						this.numeric403(cmd[1])
						break;
					}
					// Not an error: MODE #channel +xyz
					var modeline = cmdline.slice(cmdline.indexOf(" ")+1);
					var modeline = modeline.slice(modeline.indexOf(" ")+1);
					this.set_chanmode(chan,modeline,false);
				} else if (cmd[1].toUpperCase() == this.nick.toUpperCase()) {
					this.setusermode(cmd[2]);
				} else {
					this.numeric("502", ":Can't change mode for other users.");
				}
			}
			break;
		case "MOTD":
			if (cmd[1]) {
				if (cmd[1][0] == ":")
					cmd[1] = cmd[1].slice(1);
				var dest_server = searchbyserver(cmd[1]);
				if (!dest_server) {
					this.numeric402(cmd[1]);
					break;
				}
				if (dest_server != -1) {
					dest_server.rawout(":" + this.nick + " MOTD :" + dest_server.nick);
					break;
				}
			}
			this.motd();
			break;
		case "NAMES":
			var numnicks;
			var tmp;
			var chan;
			var chans;
			var Client;

			if (!cmd[1]) {
				for(chan in Channels) {
					if(!(Channels[chan].mode&CHANMODE_SECRET) && !(Channels[chan].mode&CHANMODE_PRIVATE))
						this.names(chan);
				}
				numnicks=0;
				tmp="";
				for(thisClient in Clients) {
					Client=Clients[thisClient];
					if (!Client.num_channels_on() &&
					    !(Client.mode&USERMODE_INVISIBLE) &&
					    !Client.server) {
						if (numnicks)
							tmp += " ";
						tmp += Client.nick;
						numnicks++;
						if (numnicks >= 59) {
							this.numeric(353,"* * :"+tmp);
							numnicks=0;
							tmp="";
						}
					}
				}
				if (numnicks)
					this.numeric(353,"* * :"+tmp);
				this.numeric(366, "* :End of /NAMES list.");
			} else {
				chans=cmd[1].split(',');
				for (nchan in chans) {
					if ((chans[nchan][0] == "#") ||
					    (chans[nchan][0] == "&")) {
						chan=chans[nchan].toUpperCase();
						if (Channels[chan] != undefined) {
							this.names(chan);
						} else {
							this.numeric401(chans[nchan]);
							break;
						}
					} else {
						this.numeric403(chans[nchan]);
						break;
					}
				}
				this.numeric(366, Channels[chan].nam + " :End of /NAMES list.");
			}
			break;
		case "NICK":
			var the_nick;

			if (!cmd[1]) {
				this.numeric(431, ":No nickname given.");
				break;
			}
			the_nick = ircstring(cmd[1]).slice(0,max_nicklen);
			if(this.check_nickname(the_nick)) {
				str="NICK " + the_nick;
				this.bcast_to_uchans_unique(str);
				this.originatorout(str,this);
				this.created = time();
				this.bcast_to_servers(str + " :" + this.created);
				push_nickbuf(this.nick,the_nick);
				this.nick = the_nick;
			}
			break;
		case "NOTICE":
			if (!cmd[1]) {
				this.numeric411("NOTICE");
				break;
			}
			if ( !cmd[2] || ( !cmd[3] && (
			     (cmd[2] == ":") && (ircstring(cmdline) == "")
			   ) ) ) {
				this.numeric412();
				break;
			}
			notice_targets = cmd[1].split(',');
			for (notice in notice_targets) {
				this.do_msg(notice_targets[notice],"NOTICE",ircstring(cmdline));
			}
			break;
		case "OPER":
			if (!cmd[2]) {
				this.numeric461(command);
				break;
			}
			if (this.mode&USERMODE_OPER) {
				this.server_notice("You're already an IRC operator.");
				break;
			}
			oper_success=false;
			for (ol in OLines) {
				if(((cmd[1].toUpperCase() ==
				    OLines[ol].nick.toUpperCase()) &&
				   (match_irc_mask(this.uprefix + "@" +
				    this.hostname,OLines[ol].hostmask)) &&
				   (cmd[2] == OLines[ol].password) &&
				   !(OLines[ol].flags&OLINE_CHECK_SYSPASSWD))
				||
				   ((OLines[ol].flags&OLINE_CHECK_SYSPASSWD) &&
				    system.check_syspass(cmd[2]) )
				) {
					oper_success=true;
					this.ircclass = OLines[ol].ircclass;
					this.flags = OLines[ol].flags;
					break;
				}
			}
			if (oper_success) {
				this.numeric("381", ":You are now an IRC operator.");
				this.mode|=USERMODE_OPER;
				this.rawout(":" + this.nick + " MODE " + this.nick + " +o");
				oper_notice("Notice", this.nick + " (" + this.uprefix + "@" + this.hostname + ") is now operator (O)");
				this.bcast_to_servers("MODE "+ this.nick +" +o");
			} else {
				this.numeric("491", ":No O:Lines for your host.  Attempt logged.");
				oper_notice("Notice","Failed OPER attempt by " + this.nick + " (" + this.uprefix + "@" + this.hostname + ")");
			}
			break;
		case "PART":
			var the_channels;

			if (!cmd[1]) {
				this.numeric461(command);
				break;
			}
			the_channels = cmd[1].split(",");
			for(pchan in the_channels) {
				this.do_part(the_channels[pchan]);
			}
			break;
		case "PASS":
			this.numeric462();
			break;
		case "PING":
			if (!cmd[1]) {
				this.numeric(409,":No origin specified.");
				break;
			}
			if (cmd[2]) {
				if (cmd[2][0] == ":")
					cmd[2] = cmd[2].slice(1);
				var dest_server = searchbyserver(cmd[2]);
				if (!dest_server) {
					this.numeric402(cmd[2]);
					break;
				}
				if (dest_server != -1) {
					dest_server.rawout(":" + this.nick + " PING " + this.nick + " :" + cmd[2]);
					break;
				}
			}
			if (cmd[1][0] == ":") 
				cmd[1] = cmd[1].slice(1);
			this.ircout("PONG " + servername + " :" + cmd[1]);
			break;
		case "PONG":
			if (cmd[2]) {
				var dest_server = searchbyserver(cmd[2]);
				if (!dest_server) {
					this.numeric402(cmd[2]);
					break;
				}
				if (dest_server != -1) {
					dest_server.rawout(":" + this.nick + " PONG " + cmd[1] + " " + dest_server.nick);
					break;
				}
			}
			this.pinged = false;
			break;
		case "QUIT":
			this.quit(ircstring(cmdline));
			break;
		case "PRIVMSG":
			if (!cmd[1]) {
				this.numeric411("PRIVMSG");
				break;
			}
			if ( !cmd[2] || ( !cmd[3] && (
			     (cmd[2] == ":") && (ircstring(cmdline) == "")
			   ) ) ) {
				this.numeric412();
				break;
			}
			privmsg_targets = cmd[1].split(',');
			for (privmsg in privmsg_targets) {
				this.do_msg(privmsg_targets[privmsg],"PRIVMSG",ircstring(cmdline));
			}
			this.talkidle = time();
			break;
		case "DIE":
			if (!((this.mode&USERMODE_OPER) &&
			      (this.flags&OLINE_CAN_DIE))) {
				this.numeric481();
				break;
			}
			if (diepass && !cmd[1]) {
				this.numeric461("DIE");
				break;
			} else if (diepass && (cmd[1] != diepass)) {
				this.server_notice("Invalid DIE password.");
				break;
			}
			log("!ERROR! Shutting down the ircd as per " + this.ircnuh);
			js.terminated = true;
			break;
		case "REHASH":
			if (!((this.mode&USERMODE_OPER) &&
			      (this.flags&OLINE_CAN_REHASH))) {
				this.numeric481();
				break;
			}
			this.numeric(382, this.nick + " ircd.conf :Rehashing.");
			oper_notice("Notice",this.nick + " is rehashing Server config file while whistling innocently");
			read_config_file();
			break;
		case "RESTART":
			if (!((this.mode&USERMODE_OPER) &&
			      (this.flags&OLINE_CAN_RESTART))) {
				this.numeric481();
				break;
			}
			if (restartpass && !cmd[1]) {
				this.numeric461("RESTART");
				break;
			} else if (restartpass && (cmd[1] != restartpass)) {
				this.server_notice("Invalid RESTART password.");
				break;
			}
			var rs_str = "Aieeeee!!!  Restarting server...";
			oper_notice("Notice",rs_str);
			terminate_everything(rs_str);
			break;
		case "SQUIT":
			if (!((this.mode&USERMODE_OPER) &&
			      (this.flags&OLINE_CAN_LSQUITCON))) {
				this.numeric481();
				break;
			}
			if(!cmd[1])
				break;
			var sq_server = searchbyserver(cmd[1]);
			if(!sq_server) {
				this.numeric402(cmd[1]);
				break;
			}
			var reason = ircstring(cmdline);
			if (!reason)
				reason = this.nick;
			if (sq_server == -1) {
				this.quit(reason);
				break;
			}
			if (!sq_server.local) { 
				if (!(this.flags&OLINE_CAN_GSQUITCON)) {
					this.numeric481();
					break;
				}
				sq_server.rawout(":" + this.nick + " SQUIT " + sq_server.nick + " :" + reason);
				break;
			}
			oper_notice("Routing","from " + servername + ": Received SQUIT " + cmd[1] + " from " + this.nick + "[" + this.uprefix + "@" + this.hostname + "] (" + reason + ")");
			sq_server.quit(reason);
			break;
		case "STATS":
			if(!cmd[1])
				break;
			if (cmd[2]) {
				if (cmd[2][0] == ":")
					cmd[2] = cmd[2].slice(1);
				var dest_server = searchbyserver(cmd[2]);
				if (!dest_server) {
					this.numeric402(cmd[2]);
					break;
				}
				if (dest_server != -1) {
					dest_server.rawout(":" + this.nick + " STATS " + cmd[1][0] + " :" + dest_server.nick);
					break;
				}
			}
			this.do_stats(cmd[1][0]);
			break;
		case "SUMMON":
			if(!cmd[1]) {
				this.numeric411("SUMMON");
				break;
			}
			if (cmd[2]) {
				if (cmd[2][0] == ":")
					cmd[2] = cmd[2].slice(1);
				var dest_server = searchbyserver(cmd[1]);
				if (!dest_server) {
					this.numeric402(cmd[2]);
					break;
				}
				if (dest_server != -1) {
					dest_server.rawout(":" + this.nick + " SUMMON " + cmd[1] + " :" + dest_server.nick);
					break;
				}
			}
			if(!enable_users_summon) {
				this.numeric445();
				break;
			}
			this.do_summon(cmd[1]);
			break;
		case "TIME":
			if (cmd[1]) {
				if (cmd[1][0] == ":")
					cmd[1] = cmd[1].slice(1);
				var dest_server = searchbyserver(cmd[1]);
				if (!dest_server) {
					this.numeric402(cmd[1]);
					break;
				}
				if (dest_server != -1) {
					dest_server.rawout(":" + this.nick + " TIME :" + dest_server.nick);
					break;
				}
			}
			this.numeric391();
			break;
		case "TOPIC":
			if (!cmd[1]) {
				this.numeric461("TOPIC");
				break;
			}
			chanid = searchbychannel(cmd[1]);
			if (!chanid) {
				this.numeric403(cmd[1]);
				break;
			}
			if (!this.onchannel(chanid.nam.toUpperCase())) {
				this.numeric442(chanid.nam);
				break;
			}
			if (cmd[2]) {
				if (!(chanid.mode&CHANMODE_TOPIC) ||
				     chanid.ismode(this.id,CHANLIST_OP) ) {
					var tmp_topic = ircstring(cmdline).slice(0,max_topiclen);
					if (tmp_topic == chanid.topic)
						break;
					chanid.topic = tmp_topic;
					chanid.topictime = time();
					chanid.topicchangedby = this.nick;
					var str = "TOPIC " + chanid.nam + " :" + chanid.topic;
					this.bcast_to_channel(chanid.nam.toUpperCase(), str, true);
					this.bcast_to_servers("TOPIC " + chanid.nam + " " + this.nick + " " + chanid.topictime + " :" + chanid.topic);
				} else {
					this.numeric482(chanid.nam);
				}
			} else { // we're just looking at one
				if (chanid.topic) {
					this.numeric("332", chanid.nam + " :" + chanid.topic);
					this.numeric("333", chanid.nam + " " + chanid.topicchangedby + " " + chanid.topictime);
				} else {
					this.numeric("331", chanid.nam + " :No topic is set.");
				}
			}
			break;
		case "TRACE":
			if (cmd[1]) {
				this.do_trace(cmd[1]);
			} else { // no args? pass our servername as the target
				this.do_trace(servername);
			}
			break;
		case "USER":
			this.numeric462();
			break;
		case "USERS":
			if (cmd[1]) {
				if (cmd[1][0] == ":")
					cmd[1] = cmd[1].slice(1);
				var dest_server = searchbyserver(cmd[1]);
				if (!dest_server) {
					this.numeric402(cmd[1]);
					break;
				}
				if (dest_server != -1) {
					dest_server.rawout(":" + this.nick + " USERS :" + dest_server.nick);
					break;
				}
			}
			if (!enable_users_summon) {
				this.numeric446();
				break;
			}
			this.do_users();
			break;
		case "USERHOST":
			var uhnick;
			var uh;
			var uhstr = "";
			var uh_argcount;

			if (!cmd[1]) {
				this.numeric461("USERHOST");
				break;
			}

			if (cmd.length > 6)
				uh_argcount = 6;
			else
				uh_argcount = cmd.length;

			for (uh=1 ; uh < uh_argcount ; uh++) {
				uhnick = searchbynick(cmd[uh]);
				if (uhnick) {
					if (uhstr)
						uhstr += " ";
					uhstr += uhnick.nick;
					if (uhnick.mode&USERMODE_OPER)
						uhstr += "*";
					uhstr += "=";
					if (uhnick.away)
						uhstr += "-";
					else
						uhstr += "+";
					uhstr += uhnick.uprefix;
					uhstr += "@";
					uhstr += uhnick.hostname;
				}
			}
			this.numeric(302, ":" + uhstr);
			break;
		case "VERSION":
			if (cmd[1]) {
				if (cmd[1][0] == ":")
					cmd[1] = cmd[1].slice(1);
				var dest_server = searchbyserver(cmd[1]);
				if (!dest_server) {
					this.numeric402(cmd[1]);
					break;
				}
				if (dest_server != -1) {
					dest_server.rawout(":" + this.nick + " VERSION :" + dest_server.nick);
					break;
				}
			}
			this.numeric351();
			break;
		case "LOCOPS":
			if (!((this.mode&USERMODE_OPER) &&
			      (this.flags&OLINE_CAN_LOCOPS))) {
				this.numeric481();
				break;
			}
			oper_notice("LocOps","from " + this.nick + ": " + ircstring(cmdline));
			break;
		case "GLOBOPS":
		case "WALLOPS":
			// allow non-opers to wallop for assistance, perhaps.
			if (!cmd[1]) {
				this.numeric461("WALLOPS");
				break;
			}
			wallopers(":" + this.ircnuh + " WALLOPS :" + ircstring(cmdline));
			server_bcast_to_servers(":" + this.nick + " WALLOPS :" + ircstring(cmdline));
			break;
		case "WHO":
			if (!cmd[1]) {
				this.do_who_usage();
				break;
			}
			if (cmd[1] == "?") {
				this.do_who_usage();
				break;
			}
			if (cmd[2]) {
				this.do_complex_who(cmd);
			} else {
				this.do_basic_who(cmd[1]);
			}
			break;
		case "WHOIS":
			if (!cmd[1]) {
				this.numeric(431, ":No nickname given.");
				break;
			}
			if (cmd[2]) {
				var dest_server = searchbyserver(cmd[2]);
				if (!dest_server) {
					this.numeric402(cmd[2]);
					break;
				}
				if (dest_server != -1) {
					dest_server.rawout(":" + this.nick + " WHOIS " + cmd[1] + " " + dest_server.nick);
					break;
				}
			}
			var wi_nicks = cmd[1].split(',');
			for (wi_nick in wi_nicks) {
				var wi = searchbynick(wi_nicks[wi_nick]);
				if (wi)
					this.do_whois(wi);
				else
					this.numeric401(wi_nicks[wi_nick]);
			}
			this.numeric(318, wi_nicks[0]+" :End of /WHOIS list.");
			break;
		case "WHOWAS":
			if (!cmd[1]) {
				this.numeric(431, ":No nickname given.");
				break;
			}
			firstnick="";
			for (aWhoWas=whowas_pointer;aWhoWas>=0;aWhoWas--) {
				if(WhoWasHistory[aWhoWas] &&
				   (WhoWasHistory[aWhoWas].nick.toUpperCase() ==
				   cmd[1].toUpperCase())) {
					this.numeric(314,WhoWasHistory[aWhoWas].nick + " " + WhoWasHistory[aWhoWas].uprefix + " " + WhoWasHistory[aWhoWas].host + " * :" + WhoWasHistory[aWhoWas].realname);
					this.numeric(312,WhoWasHistory[aWhoWas].nick + " " + WhoWasHistory[aWhoWas].server + " :" + WhoWasHistory[aWhoWas].serverdesc);
					if (!firstnick)
						firstnick = WhoWasHistory[aWhoWas].nick;
				}
			}
			for (aWhoWas=whowas_buffer;aWhoWas>=whowas_pointer;aWhoWas--) {
				if(WhoWasHistory[aWhoWas] &&
				   (WhoWasHistory[aWhoWas].nick.toUpperCase() ==
				   cmd[1].toUpperCase())) {
					this.numeric(314,WhoWasHistory[aWhoWas].nick + " " + WhoWasHistory[aWhoWas].uprefix + " " + WhoWasHistory[aWhoWas].host + " * :" + WhoWasHistory[aWhoWas].realname);
					this.numeric(312,WhoWasHistory[aWhoWas].nick + " " + WhoWasHistory[aWhoWas].server + " :" + WhoWasHistory[aWhoWas].serverdesc);
					if (!firstnick)
						firstnick = WhoWasHistory[aWhoWas].nick;
				}
			}
			if (!firstnick) {
				this.numeric(406,cmd[1] + " :There was no such nickname.");
				firstnick = cmd[1];
			}
			this.numeric(369,firstnick+" :End of /WHOWAS command.");
			break;
		case "ERROR":
			break; // silently ignore.
		case "CS":
		case "CHANSERV":
			if (!cmd[1]) {
				this.numeric412();
				break;
			}
			var str = cmdline.slice(cmdline.indexOf(" ")+1);
			if (str[0] == ":")
				str = str.slice(1);
			this.services_msg("ChanServ",str);
			break;
		case "NS":
		case "NICKSERV":
			if (!cmd[1]) {
				this.numeric412();
				break;
			}
			var str = cmdline.slice(cmdline.indexOf(" ")+1);
			if (str[0] == ":")
				str = str.slice(1);
			this.services_msg("NickServ",str);
			break;
		case "MS":
		case "MEMOSERV":
			if (!cmd[1]) {
				this.numeric412();
				break;
			}
			var str = cmdline.slice(cmdline.indexOf(" ")+1);
			if (str[0] == ":")
				str = str.slice(1);
			this.services_msg("MemoServ",str);
			break;
		case "OS":
		case "OPERSERV":
			if (!cmd[1]) {
				this.numeric412();
				break;
			}
			var str = cmdline.slice(cmdline.indexOf(" ")+1);
			if (str[0] == ":")
				str = str.slice(1);
			this.services_msg("OperServ",str);
			break;
		case "IDENTIFY":
			if (!cmd[1]) {
				this.numeric412();
                                break;
                        }
			var str = cmdline.slice(cmdline.indexOf(" ")+1);
			if (str[0] == ":")
				str = str.slice(1);
			if (cmd[1][1] == "#")
				var services_target = "ChanServ";
			else
				var services_target = "NickServ";
			this.services_msg(services_target,"IDENTIFY " + str);
			break;
		default:
			this.numeric("421", command + " :Unknown command.");
			break;
	}
}

// Server connections are ConnType 5
function IRCClient_server_commands(origin, command, cmdline) {
	if (origin.match(/[.]/)) {
		var ThisOrigin = searchbyserver(origin);
		var killtype = "SQUIT";
	} else {
		var ThisOrigin = searchbynick(origin);
		var killtype = "KILL";
	}
	if (!ThisOrigin) {
		oper_notice("Notice","Server " + this.nick + " trying to pass message for non-existent origin: " + origin);
		this.rawout(killtype + " " + origin + " :" + servername + " (" + origin + "(?) <- " + this.nick + ")");
		return 0;
	}

	cmd=cmdline.split(' ');

	if (command.match(/^[0-9]+/)) { // passing on a numeric to the client
		if (!cmd[1])
			return 0; // uh...?
		var destination = searchbynick(cmd[1]);
		if (!destination)
			return 0;
		destination.rawout(":" + ThisOrigin.nick + " " + cmdline);
		return 1;
	}

	switch(command) {
		case "GNOTICE":
		case "GLOBOPS":
		case "WALLOPS":
			if (!cmd[1])
				break;
			str = ":" + origin + " WALLOPS :" + ircstring(cmdline);
			wallopers(str);
			this.bcast_to_servers_raw(str);
			break;
		case "ADMIN":
			if (!cmd[1])
				break;
			if (cmd[1][0] == ":")
				cmd[1] = cmd[1].slice(1);
			if (match_irc_mask(servername, cmd[1])) {
				ThisOrigin.do_admin();
			} else {
				var dest_server = searchbyserver(cmd[1]);
				if (!dest_server)
					break;
				dest_server.rawout(":" + ThisOrigin.nick + " ADMIN :" + dest_server.nick);
			}
			break;
		case "AWAY":
			if (!cmd[1])
				ThisOrigin.away = "";
			else
				ThisOrigin.away = ircstring(cmdline);
			break;
		case "CONNECT":
			if (!cmd[3] || !this.hub)
				break;
			if (match_irc_mask(servername, cmd[3])) {
				ThisOrigin.do_connect(cmd[1],cmd[2]);
			} else {
				var dest_server = searchbyserver(cmd[3]);
				if (!dest_server)
					break;
				dest_server.rawout(":" + ThisOrigin.nick + " CONNECT " + cmd[1] + " " + cmd[2] + " " + dest_server.nick);
			}
			break;
		case "ERROR":
			oper_notice("Notice", "ERROR from " + this.nick + "[(+)0@" + this.hostname + "] -- " + ircstring(cmdline));
			ThisOrigin.quit(ircstring(cmdline));
			break;
		case "INFO":
			if (!cmd[1])
				break;
			if (cmd[1][0] == ":")
				cmd[1] = cmd[1].slice(1);
			if (match_irc_mask(servername, cmd[1])) {
				ThisOrigin.do_info();
			} else {
				var dest_server = searchbyserver(cmd[1]);
				if (!dest_server)
					break;
				dest_server.rawout(":" + ThisOrigin.nick + " INFO :" + dest_server.nick);
			}
			break;
		case "INVITE":
			if (!cmd[2])
				break;
			chanid = searchbychannel(cmd[2]);
			if (!chanid)
				break;
			if (!chanid.ismode(this.id,CHANLIST_OP))
				break;
			nickid = searchbynick(cmd[1]);
			if (!nickid)
				break;
			if (nickid.onchannel(chanid.nam.toUpperCase()))
				break;
			nickid.originatorout("INVITE " + nickid.nick + " :" + chanid.nam,this);
			nickid.invited=chanid.nam.toUpperCase();
			break;
		case "KICK":
			var chanid;
			var nickid;
			var str;
			var kick_reason;

			if (!cmd[2])
				break;
			chanid = searchbychannel(cmd[1]);
			if (!chanid)
				break;
			nickid = searchbynick(cmd[2]);
			if (!nickid)
				nickid = searchbynick(search_nickbuf(cmd[2]));
			if (!nickid)
				break;
			if (!nickid.onchannel(chanid.nam.toUpperCase()))
				break;
			if (cmd[3])
				kick_reason = ircstring(cmdline).slice(0,max_kicklen);
			else
				kick_reason = ThisOrigin.nick;
			str = "KICK " + chanid.nam + " " + nickid.nick + " :" + kick_reason;
			ThisOrigin.bcast_to_channel(chanid.nam, str, false);
			this.bcast_to_servers_raw(":" + ThisOrigin.nick + " " + str);
			nickid.rmchan(Channels[chanid.nam.toUpperCase()]);
			break;
		case "JOIN":
			if (!cmd[1])
				break;
			if (cmd[1][0] == ":")
				cmd[1]=cmd[1].slice(1);
			var the_channels = cmd[1].split(",");
			for (jchan in the_channels) {
				if (the_channels[jchan][0] != "#")
					break;
				ThisOrigin.do_join(the_channels[jchan].slice(0,max_chanlen),"");
			}
			break;
		case "PASS":
			var result;

			if (!this.hub || !cmd[3])
				break;
			if (cmd[3] != "QWK")
				break;
			if (cmd[2][0] == ":")
				cmd[2] = cmd[2].slice(1);
			if (cmd[4]) { // pass the message on to target.
				var dest_server = searchbyserver(cmd[4]);
				if (!dest_server) {
					break;
				} else if ((dest_server == -1) && 
				           (this.flags&NLINE_IS_QWKMASTER)) {
					var qwkid = cmd[2].toLowerCase();
					var my_server = searchbynick(qwkid + ".synchro.net");
					if (!my_server)
						break;
					if ((my_server.conntype ==
					    TYPE_UNREGISTERED) &&
					    (cmd[1] == "OK") ) {
						my_server.server = true;
						my_server.conntype = TYPE_SERVER;
						my_server.finalize_server_connect("QWK");
						break;
					} else {
						my_server.quit("ERROR: Server not configured.",true);
						break;
					}
				} else if (dest_server) {
					if (dest_server == -1)
						break; // security trap
					dest_server.rawout(":" + ThisOrigin.nick + " PASS " + cmd[1] + " :" + cmd[2] + " QWK " + dest_server.nick);
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
			// If we got here, we must be the qwk master. Process.
			if (check_qwk_passwd(cmd[2],cmd[1]))
				result = "OK";
			else
				result = "VOID";
			this.rawout(":" + servername + " PASS " + result + " :" + cmd[2] + " QWK " + ThisOrigin.nick);
			break;
		case "SJOIN":
			if (!cmd[2])
				break;
			if (cmd[2][0] != "#")
				break;

			if (cmd[3]) {
				var mode_args = "";
				var tmp_modeargs = 0;

				for (tmpmc in cmd[3]) {
					if (cmd[3][tmpmc] == "k") {
						tmp_modeargs++;
						mode_args += cmd[3 + tmp_modeargs];
					} else if (cmd[3][tmpmc] == "l") {
						tmp_modeargs++;
						mode_args += cmd[3 + tmp_modeargs];
					}
				}
				if ((cmd[4] == "") && cmd[5])
					tmp_modeargs++;

				var chan_members = ircstring(cmdline,4+tmp_modeargs).split(' ');

				if (chan_members == "") {
					oper_notice("Notice","Server " + this.nick + " trying to SJOIN empty channel " + cmd[2]);
					break;
				}
			}

			var chan = searchbychannel(cmd[2]);
			if (!chan) {
				var cn_tuc = cmd[2].toUpperCase();
				Channels[cn_tuc]=new Channel(cn_tuc);
				var chan = Channels[cn_tuc];
				chan.nam = cmd[2];
				chan.created = parseInt(cmd[1]);
				chan.topic = "";
				chan.users = new Array();
				chan.modelist[CHANLIST_BAN] = new Array();
				chan.modelist[CHANLIST_VOICE] = new Array();
				chan.modelist[CHANLIST_OP] = new Array();
				chan.mode = CHANMODE_NONE;
			}
			if (cmd[3]) {
				if (!ThisOrigin.local || (chan.created == parseInt(cmd[1])))
					var bounce_modes = false;
				else
					var bounce_modes = true;

				if (chan.created >= parseInt(cmd[1])) {
					if (mode_args)
						this.set_chanmode(chan, cmd[3] + " " + mode_args, bounce_modes);
					else
						this.set_chanmode(chan, cmd[3], bounce_modes);
				}

				var num_sync_modes = 0;
				var push_sync_modes = "+";
				var push_sync_args = "";
				var new_chan_members = "";
				for (member in chan_members) {
					if (new_chan_members)
						new_chan_members += " ";
					var is_op = false;
					var is_voice = false;
					if (chan_members[member][0] == "@") {
						is_op = true;
						chan_members[member] = chan_members[member].slice(1);
					}
					if (chan_members[member][0] == "+") {
						is_voice = true;
						chan_members[member] = chan_members[member].slice(1);
					}
					var member_obj = searchbynick(chan_members[member]);
					if (!member_obj)
						continue;

					if (member_obj.onchannel(chan.nam.toUpperCase()))
						continue;

					member_obj.channels.push(chan.nam.toUpperCase());
					chan.users.push(member_obj.id);
					member_obj.bcast_to_channel(chan.nam, "JOIN " + chan.nam, false);
					if (chan.created >= parseInt(cmd[1])) {
						if (is_op) {
							chan.modelist[CHANLIST_OP].push(member_obj.id);
							push_sync_modes += "o";
							push_sync_args += " " + member_obj.nick;
							num_sync_modes++;
							new_chan_members += "@";
						}
						if (num_sync_modes >= max_modes) {
							this.bcast_to_channel(chan.nam, "MODE " + chan.nam + " " + push_sync_modes + push_sync_args);
							push_sync_modes = "+";
							push_sync_args = "";
							num_sync_modes = 0;
						}
						if (is_voice) {
							chan.modelist[CHANLIST_VOICE].push(member_obj.id);
							push_sync_modes += "v";
							push_sync_args += " " + member_obj.nick;
							num_sync_modes++;
							new_chan_members += "+";
						}
						if (num_sync_modes >= max_modes) {
							this.bcast_to_channel(chan.nam, "MODE " + chan.nam + " " + push_sync_modes + push_sync_args);
							push_sync_modes = "+";
							push_sync_args = "";
							num_sync_modes = 0;
						}
					}
					new_chan_members += member_obj.nick;
				}
				if (num_sync_modes)
					this.bcast_to_channel(chan.nam, "MODE " + chan.nam + " " + push_sync_modes + push_sync_args);

				// Synchronize the TS to what we received.
				if (chan.created > parseInt(cmd[1]))
					chan.created = parseInt(cmd[1]);

				this.bcast_to_servers_raw(":" + ThisOrigin.nick + " SJOIN " + chan.created + " " + chan.nam + " " + chan.chanmode(true) + " :" + new_chan_members)
			} else {
				if (ThisOrigin.onchannel(chan.nam.toUpperCase()))
					break;

				ThisOrigin.channels.push(chan.nam.toUpperCase());
				chan.users.push(ThisOrigin.id);
				ThisOrigin.bcast_to_channel(chan.nam, "JOIN " + chan.nam, false);
				this.bcast_to_servers_raw(":" + ThisOrigin.nick + " SJOIN " + chan.created + " " + chan.nam);
			}
			break;
		case "SQUIT":
			var sq_server;
			var reason;

			if (!cmd[1] || !this.hub)
				sq_server = this;
			else
				sq_server = searchbyserver(cmd[1]);
			if (!sq_server)
				break;
			reason = ircstring(cmdline);
			if (!reason || !cmd[2])
				reason = ThisOrigin.nick;
			if (sq_server == -1) {
				this.bcast_to_servers_raw("SQUIT " + this.nick + " :Forwards SQUIT.");
				this.quit("Forwards SQUIT.",true);
				break;
			}
			// message from our uplink telling us a server is gone
			if (this.id == sq_server.parent) {
				sq_server.quit(reason,false,false,ThisOrigin);
				break;
			}
			// oper or server going for squit of a server
			if (!sq_server.local) {
				sq_server.rawout(":" + ThisOrigin.nick + " SQUIT " + sq_server.nick + " :" + reason);
				break;
			}
			sq_server.quit(reason);
			break;
		case "KILL":
			if (!cmd[2])
				break;
			if (cmd[1].match(/[.]/))
				break;
			if (cmd[2] == ":")
				break;
			var reason = ircstring(cmdline);
			var kills = cmd[1].split(",");
			for(kill in kills) {
				var target = searchbynick(kills[kill]);
				if (!target)
					target = searchbynick(search_nickbuf(kills[kill]));
				if (target && (this.hub ||
				    (target.parent == this.id)) ) {
					this.bcast_to_servers_raw(":" + ThisOrigin.nick + " KILL " + target.nick + " :" + reason);
					target.quit("KILLED by " + ThisOrigin.nick + " (" + reason + ")",true);
				} else if (target && !this.hub) {
					oper_notice("Notice","Non-Hub server " + this.nick + " trying to KILL " + target.nick);
					this.reintroduce_nick(target);
				}
			}
			break;
		case "LINKS":
			if (!cmd[2])
				break;
			if (match_irc_mask(servername, cmd[1])) {
				ThisOrigin.do_links(cmd[2]);
			} else {
				var dest_server = searchbyserver(cmd[1]);
				if (!dest_server)
					break;
				dest_server.rawout(":" + ThisOrigin.nick + " LINKS " + dest_server.nick + " " + cmd[2]);
			}
			break;
		case "MODE":
			if (!cmd[1])
				break;
			// nasty kludge since we don't support per-mode TS yet.
			if (cmd[2].match(/^[0-9]+$/) && 
			    (cmd[1][0] == "#") ) {
				cmdline="MODE " + cmd[1];
				for(xx=3;xx<cmd.length;xx++) {
					cmdline += " ";
					cmdline += cmd[xx];
				}
			}
			if (cmd[1][0] == "#") {
				var chan = searchbychannel(cmd[1])
				if (!chan)
					break;
				var modeline = cmdline.slice(cmdline.indexOf(" ")+1);
				var modeline = modeline.slice(modeline.indexOf(" ")+1);
				ThisOrigin.set_chanmode(chan,modeline,false);
			} else { // assume it's for a user
				ThisOrigin.setusermode(cmd[2]);
			}
			break;
		case "MOTD":
			if (!cmd[1])
				break;
			if (cmd[1][0] == ":")
				cmd[1] = cmd[1].slice(1);
			if (match_irc_mask(servername, cmd[1])) {
				ThisOrigin.motd();
			} else {
				var dest_server = searchbyserver(cmd[1]);
				if (!dest_server)
					break;
				dest_server.rawout(":" + ThisOrigin.nick + " MOTD :" + dest_server.nick);
			}
			break;
		case "NICK":
			if (!cmd[2] || (!cmd[8] && (cmd[2][0] != ":")) )
				break;
			var collide = searchbynick(cmd[1]);
			if ((collide) && (parseInt(collide.created) >
			    parseInt(cmd[3]) ) && this.hub) {
				// Nuke our side of things, allow this newly
				// introduced nick to overrule.
				collide.numeric(436, collide.nick + " :Nickname Collision KILL.");
				this.bcast_to_servers("KILL " + collide.nick + " :Nickname Collision.");
				collide.quit("Nickname Collision",true);
			} else if (collide && !this.hub) {
				oper_notice("Notice","Server " + this.nick + " trying to collide nick " + collide.nick + " forwards, reversing.");
				// Don't collide our side of things from a leaf
				this.ircout("KILL " + cmd[1] + " :Inverse Nickname Collision.");
				// Reintroduce our nick, because the remote end
				// probably killed it already on our behalf.
				this.reintroduce_nick(collide);
				break;
			} else if (collide && this.hub) {
				break;
			}
			if (cmd[2][0] == ":") {
				cmd[2] = cmd[2].slice(1);
				ThisOrigin.created = cmd[2];
				ThisOrigin.bcast_to_uchans_unique("NICK " + cmd[1]);
				this.bcast_to_servers_raw(":" + origin + " NICK " + cmd[1] + " :" + cmd[2]);
				push_nickbuf(ThisOrigin.nick,cmd[1]);
				ThisOrigin.nick = cmd[1];
			} else if (cmd[10]) {
				if (!this.hub) {
					if(!this.check_nickname(cmd[1])) {
						oper_notice("Notice","Server " + this.nick + " trying to introduce invalid nickname: " + cmd[1] + ", killed.");
						this.ircout("KILL " + cmd[1] + " :Bogus Nickname.");
						break;
					}
					cmd[2] = 1; // Force hops to 1.
					cmd[3] = time(); // Force TS on nick.
					cmd[7] = this.nick // Force server name
				} else { // if we're a hub
					var test_server = searchbyserver(cmd[7]);
					if (!test_server || (this.id !=
					    test_server.parent)) {
						oper_notice("Notice","Server " + this.nick + " trying to introduce nick from server not behind it: " + cmd[1] + "@" + cmd[7]);
						this.ircout("KILL " + cmd[1] + " :Invalid Origin.");
						break;
					}
				}
				new_id = get_next_clientid();
				Clients[new_id]=new IRCClient(-1,new_id,false,true);
				NewNick = Clients[new_id];
				NewNick.nick = cmd[1];
				NewNick.hops = cmd[2];
				NewNick.created = cmd[3];
				NewNick.uprefix = cmd[5];
				NewNick.hostname = cmd[6];
				NewNick.servername = cmd[7];
				NewNick.realname = ircstring(cmdline,10);
				NewNick.conntype = TYPE_USER_REMOTE;
				NewNick.away = "";
				NewNick.mode = USERMODE_NONE;
				NewNick.connecttime = 0;
				NewNick.idletime = 0;
				NewNick.talkidle = 0;
				NewNick.parent = this.id;
				NewNick.ip = int_to_ip(cmd[9]);
				NewNick.setusermode(cmd[4]);
				true_hops = parseInt(NewNick.hops)+1;
				this.bcast_to_servers_raw("NICK " + NewNick.nick + " " + true_hops + " " + NewNick.created + " " + NewNick.get_usermode() + " " + NewNick.uprefix + " " + NewNick.hostname + " " + NewNick.servername + " 0 " + cmd[9] + " :" + NewNick.realname);
			}
			break;
		case "NOTICE":
			if (!cmd[1])
				break;
			if ( !cmd[2] || ( !cmd[3] && (
			     (cmd[2] == ":") && (ircstring(cmdline) == "")
			    ) ) )
				break;
			notice_targets = cmd[1].split(',');
			for (notice in notice_targets) {
				if (notice_targets[notice][0] != "&")
					ThisOrigin.do_msg(notice_targets[notice],"NOTICE",ircstring(cmdline));
			}
			break;
		case "PART":
			if (!cmd[1])
				break;
			the_channels = cmd[1].split(",");
			for(pchan in the_channels) {
				ThisOrigin.do_part(the_channels[pchan]);
			}
			break;
		case "PRIVMSG":
			if (!cmd[1])
				break;
			if ( !cmd[2] || ( !cmd[3] && (
			     (cmd[2] == ":") && (ircstring(cmdline) == "")
			   ) ) )
				break;
			privmsg_targets = cmd[1].split(',');
			for (privmsg in privmsg_targets) {
				if (privmsg_targets[privmsg][0] != "&")
					ThisOrigin.do_msg(privmsg_targets[privmsg],"PRIVMSG",ircstring(cmdline));
			}
			break;
		case "PING":
			if (!cmd[1])
				break;
			if (cmd[2] && (cmd[2][0] == ":"))
				cmd[2] = cmd[2].slice(1);
			var tmp_server;
			if (cmd[2])
				tmp_server = searchbyserver(cmd[2]);
			if (tmp_server && (tmp_server != -1) &&
			    (tmp_server.id != ThisOrigin.id)) {
				tmp_server.rawout(":" + ThisOrigin.nick + " PING " + ThisOrigin.nick + " :" + tmp_server.nick);
				break;
			}
			if (cmd[1][0] == ":")
				cmd[1] = cmd[1].slice(1);
			if (!cmd[2])
				this.ircout("PONG " + servername + " :" + cmd[1]);
			else
				this.ircout("PONG " + cmd[2] + " :" + cmd[1]);
			break;
		case "PONG":
			if (cmd[2]) {
				if (cmd[2][0] == ":")
					cmd[2] = cmd[2].slice(1);
				var my_server = searchbyserver(cmd[2]);
				if (!my_server) {
					break;
				} else if (my_server == -1) {
					var my_nick = searchbynick(cmd[2]);
					if (my_nick)
						my_nick.rawout(":" + ThisOrigin.nick + " PONG " + cmd[1] + " :" + my_nick.nick);
					else
						this.pinged = false;
					break;
				} else if (my_server) {
					my_server.rawout(":" + ThisOrigin.nick + " PONG " + cmd[1] + " :" + cmd[2]);
					break;
				}
			}
			this.pinged = false;
			break;
		case "QUIT":
			ThisOrigin.quit(ircstring(cmdline));
			break;
		case "SERVER":
			if (!cmd[3])
				break;
			if ((cmd[2] == 1) && !this.realname) {
				this.nick = cmd[1];
				this.hops = 1;
				this.realname = ircstring(cmdline);
				this.linkparent = servername;
				this.parent = this.id;
				newsrv = this;
			} else if (parseInt(cmd[2]) > 1) {
				if (this.hub) {
					new_id = get_next_clientid();
					Clients[new_id] = new IRCClient(-1,new_id,false,false);
					newsrv = Clients[new_id];
					newsrv.hops = cmd[2];
					newsrv.nick = cmd[1];
					newsrv.realname = ircstring(cmdline);
					newsrv.server = true;
					newsrv.parent = this.id;
					newsrv.conntype = TYPE_SERVER;
					newsrv.linkparent = ThisOrigin.nick;
				} else {
					oper_notice("Routing","from " + servername + ": Non-Hub link " + this.nick + " introduced " + cmd[1] + "(*).");
					this.quit("Too many servers.  You have no H:Line to introduce " + cmd[1] + ".",true);
					return 0;
				}
			} else {
				break;
			}
			this.bcast_to_servers_raw(":" + newsrv.linkparent + " SERVER " + newsrv.nick + " " + (parseInt(newsrv.hops)+1) + " :" + newsrv.realname);
			break;
		case "STATS":
			if (!cmd[2])
				break;
			if (cmd[2][0] == ":")
				cmd[2] = cmd[2].slice(1);
			if (match_irc_mask(servername, cmd[2])) {
				ThisOrigin.do_stats(cmd[1][0]);
			} else {
				var dest_server = searchbyserver(cmd[2]);
				if (!dest_server)
					break;
				dest_server.rawout(":" + ThisOrigin.nick + " STATS " + cmd[1][0] + " :" + dest_server.nick);
			}
			break;
		case "TIME":
			if (!cmd[1])
				break;
			if (cmd[1][0] == ":")
				cmd[1] = cmd[1].slice(1);
			if (match_irc_mask(servername, cmd[1])) {
				ThisOrigin.numeric391();
			} else {
				var dest_server = searchbyserver(cmd[1]);
				if (!dest_server)
					break;
				dest_server.rawout(":" + ThisOrigin.nick + " TIME :" + dest_server.nick);
			}
			break;
		case "TRACE":
			if (!cmd[1])
				break;
			ThisOrigin.do_trace(cmd[1]);
			break;
		case "SUMMON":
			if (!cmd[2])
				break;
			if (cmd[2][0] == ":")
				cmd[2] = cmd[2].slice(1);
			if (match_irc_mask(servername, cmd[2])) {
				if (enable_users_summon) {
					ThisOrigin.do_summon(cmd[1]);
				} else {
					this.numeric445();
					break;
				}
			} else {
				var dest_server = searchbyserver(cmd[1]);
				if (!dest_server)
					break;
				dest_server.rawout(":" + ThisOrigin.nick + " SUMMON " + cmd[1] + " :" + dest_server.nick);
			}
			break;
		case "TOPIC":
			if (!cmd[4])
				break;
			var chan = searchbychannel(cmd[1]);
			if (!chan)
				break;
			var the_topic = ircstring(cmdline);
			if (the_topic == chan.topic)
				break;
			chan.topic = the_topic;
			if (this.hub)
				chan.topictime = cmd[3];
			else
				chan.topictime = time();
			chan.topicchangedby = cmd[2];
			str = "TOPIC " + chan.nam + " :" + chan.topic;
			ThisOrigin.bcast_to_channel(chan.nam,str,false);
			this.bcast_to_servers_raw(":" + ThisOrigin.nick + " TOPIC " + chan.nam + " " + ThisOrigin.nick + " " + chan.topictime + " :" + chan.topic);
			break;
		case "USERS":
			if (!cmd[1])
				break;
			if (cmd[1][0] == ":")
				cmd[1] = cmd[1].slice(1);
			if (match_irc_mask(servername, cmd[1])) {
				if (!enable_users_summon) {
					ThisOrigin.numeric446();
					break;
				} else {
					ThisOrigin.do_users();
				}
			} else {
				var dest_server = searchbyserver(cmd[1]);
				if (!dest_server)
					break;
				dest_server.rawout(":" + ThisOrigin.nick + " USERS :" + dest_server.nick);
			}
			break;
		case "VERSION":
			if (!cmd[1])
				break;
			if (cmd[1][0] == ":")
				cmd[1] = cmd[1].slice(1);
			if (match_irc_mask(servername, cmd[1])) {
				// it's for us, return the message
				ThisOrigin.numeric351();
			} else {
				// psst, pass it on
				var dest_server = searchbyserver(cmd[1]);
				if (!dest_server)
					break; // someone messed up.
				dest_server.rawout(":" + ThisOrigin.nick + " VERSION :" + dest_server.nick);
			}
			break;
		case "WHOIS":
			if (!cmd[2])
				break;
			if (cmd[2][0] == ":")
				cmd[2] = cmd[2].slice(1);
			if (match_irc_mask(servername, cmd[2])) {
				var wi_nicks = cmd[1].split(',');
				for (wi_nick in wi_nicks) {
					var wi = searchbynick(wi_nicks[wi_nick]);
					if (wi)
						ThisOrigin.do_whois(wi);
					else
						ThisOrigin.numeric401(wi_nicks[wi_nick]);
				}
				ThisOrigin.numeric(318, wi_nicks[0]+" :End of /WHOIS list.");
			} else {
				var dest_server = searchbyserver(cmd[2]);
				if (!dest_server)
					break;
				dest_server.rawout(":" + ThisOrigin.nick + " WHOIS " + cmd[1] + " " + dest_server.nick);
			}
			break;
		case "AKILL":
			var this_uh;

			if (!cmd[6])
				break;
			if (!ThisOrigin.isulined) {
				oper_notice("Notice","Non-U:Lined server " + ThisOrigin.nick + " trying to utilize AKILL.");
				break;
			}

			this_uh = cmd[2] + "@" + cmd[1];
			if (isklined(this_uh))
				break;
			KLines.push(new KLine(this_uh,ircstring(cmdline),"A"));
			this.bcast_to_servers_raw(":" + ThisOrigin.nick + " " + cmdline);
			scan_for_klined_clients();
			break;
		default:
			break;
	}
}

function IRCClient_check_timeout() {
	if (this.nick) {
		if ( (this.pinged) && ((time() - this.pinged) > YLines[this.ircclass].pingfreq) ) {
			this.pinged = false;
			this.quit("Ping Timeout");
		} else if (!this.pinged && ((time() - this.idletime) > YLines[this.ircclass].pingfreq)) {
			this.pinged = time();
			this.rawout("PING :" + servername);
		}
	}
}

function IRCClient_work() {
	var command;
	var cmdline;
	var origin;

	if (!this.socket.is_connected) {
		this.quit("Connection reset by peer");
		return 0;
	}
	if (this.socket.data_waiting &&
	    (cmdline=this.socket.recvline(4096,0)) ) {

		if(cmdline==null)
			return 0; // nothing to do

		// Allow only 512 bytes per RFC for clients, however, servers
		// are allowed to fill up the entire buffer for bandwidth
		// savings, since we trust them more.
		if(!this.server)
			cmdline=cmdline.slice(0,512);

		// Some (broken) clients add extra CR garbage to the
		// beginning (end) of a line. Fix.
		// Eventually we should just strip cmdline of all
		// invalid characters, including these and NUL (00)
		// among others.
		if ((cmdline[0] == "\r") || (cmdline[0] == "\n"))
			cmdline=cmdline.slice(1);

		if (debug)
			log(format("[%s<-%s]: %s",servername,this.nick,cmdline));
		cmd=cmdline.split(' ');
		if (cmdline[0] == ":") {
			// Silently ignore NULL originator commands.
			if (!cmd[1])
				return 0;
			// if :<originator> doesn't match nick of originating
			// socket, drop silently per RFC.
			origin = cmd[0].slice(1);
			if ((this.conntype == TYPE_USER) &&
			    (origin.toUpperCase() != this.nick.toUpperCase()))
				return 0;
			command = cmd[1].toUpperCase();
			cmdline = cmdline.slice(cmdline.indexOf(" ")+1);
		} else {
			command = cmd[0].toUpperCase();
			origin = this.nick;
		}

		this.idletime = time();
		if (this.conntype == TYPE_UNREGISTERED) {
			this.unregistered_commands(command,cmdline);
		} else if (this.conntype == TYPE_USER) {
			this.registered_commands(command,cmdline);
		} else if (this.conntype == TYPE_SERVER) {
			this.server_commands(origin,command,cmdline);
		} else {
			oper_notice("Notice","Client has bogus conntype!");
		}
	}
}

function IRCClient_finalize_server_connect(states) {
	hcc_counter++;
	oper_notice("Routing","Link with " + this.nick + "[unknown@" + this.hostname + "] established, states: " + states);
	if (server.client_update != undefined)
		server.client_update(this.socket, this.nick, this.hostname);
	if (!this.sentps) {
	for (cl in CLines) {
		if(match_irc_mask(this.nick,CLines[cl].servername)) {
			this.rawout("PASS " + CLines[cl].password + " :" + states);
				break;
			}
		}
		this.rawout("CAPAB " + server_capab);
		this.rawout("SERVER " + servername + " 1 :" + serverdesc);
	}
	this.bcast_to_servers_raw(":" + servername + " SERVER " + this.nick + " 2 :" + this.realname);
	this.synchronize();
}


///////////////////////////////////
///////////////////////////////////
///// End of IRCClient object /////
///////////////////////////////////
///////////////////////////////////

function Channel(nam)  {
	this.nam=nam;
	this.mode=CHANMODE_NONE;
	this.topic="";
	this.topictime=0;
	this.topicchangedby="";
	this.arg = new Array;
	this.arg[CHANMODE_LIMIT] = 0;
	this.arg[CHANMODE_KEY] = "";
	this.users=new Array;
	this.modelist=new Array;
	this.modelist[CHANLIST_OP]=new Array;
	this.modelist[CHANLIST_VOICE]=new Array;
	this.modelist[CHANLIST_BAN]=new Array;
	this.bantime=new Array;
	this.bancreator=new Array;
	this.created=time();
	this.ismode=Channel_ismode;
	this.add_modelist=Channel_add_modelist;
	this.del_modelist=Channel_del_modelist;
	this.locate_on_list=Channel_locate_on_list;
	this.count_users=Channel_count_users;
	this.chanmode=Channel_chanmode;
	this.isbanned=Channel_isbanned;
	this.count_modelist=Channel_count_modelist;
	this.occupants=Channel_occupants;
}

function Channel_ismode(tmp_str,mode_bit) {
	if (!this.modelist[mode_bit])
		return 0;
	for (tmp_element in this.modelist[mode_bit]) {
		if (this.modelist[mode_bit][tmp_element] == tmp_str)
			return 1;
	}
	return 0;
}

function Channel_locate_on_list(tmp_str,mode_bit) {
	for (tmp_index in this.modelist[mode_bit]) {
		if (this.modelist[mode_bit][tmp_index] == tmp_str) {
			return tmp_index;
		}
	}
}

function Channel_add_modelist(tmp_nickid,list_bit) {
	if (this.ismode(tmp_nickid,list_bit))
		return 0;
	pushed = this.modelist[list_bit].push(tmp_nickid);
	return pushed-1;
}

function Channel_del_modelist(tmp_nickid,list_bit) {
	if (!this.ismode(tmp_nickid,list_bit))
		return 0;
	delete_index = this.locate_on_list(tmp_nickid,list_bit);
	delete this.modelist[list_bit][delete_index];
	return delete_index;
}

function Channel_count_users() {
	tmp_counter=0;
	for (tmp_count in this.users) {
		if (this.users[tmp_count])
			tmp_counter++;
	}
	return tmp_counter;
}

function Channel_count_modelist(list_bit) {
	tmp_counter=0;
	for (tmp_count in this.modelist[list_bit]) {
		if (this.modelist[list_bit][tmp_count])
			tmp_counter++;
	}
	return tmp_counter;
}

function Channel_chanmode(show_args) {
	tmp_mode = "+";
	tmp_extras = "";
	if (this.mode&CHANMODE_INVITE)
		tmp_mode += "i";
	if (this.mode&CHANMODE_KEY) {
		tmp_mode += "k";
		if(show_args)
			tmp_extras += " " + this.arg[CHANMODE_KEY];
	}
	if (this.mode&CHANMODE_LIMIT) {
		tmp_mode += "l";
		if(show_args)
			tmp_extras += " " + this.arg[CHANMODE_LIMIT];
	}
	if (this.mode&CHANMODE_MODERATED)
		tmp_mode += "m";
	if (this.mode&CHANMODE_NOOUTSIDE)
		tmp_mode += "n";
	if (this.mode&CHANMODE_PRIVATE)
		tmp_mode += "p";
	if (this.mode&CHANMODE_SECRET)
		tmp_mode += "s";
	if (this.mode&CHANMODE_TOPIC)
		tmp_mode += "t";
	if (!tmp_extras)
		tmp_extras = " ";
	return tmp_mode + tmp_extras;
}

function inverse_chanmode(bitlist) {
	tmp_mode = "-";
	if (bitlist&CHANMODE_INVITE)
		tmp_mode += "i";
	if (bitlist&CHANMODE_KEY)
		tmp_mode += "k";
	if (bitlist&CHANMODE_LIMIT)
		tmp_mode += "l";
	if (bitlist&CHANMODE_MODERATED)
		tmp_mode += "m";
	if (bitlist&CHANMODE_NOOUTSIDE)
		tmp_mode += "n";
	if (bitlist&CHANMODE_PRIVATE)
		tmp_mode += "p";
	if (bitlist&CHANMODE_SECRET)
		tmp_mode += "s";
	if (bitlist&CHANMODE_TOPIC)
		tmp_mode += "t";
	return tmp_mode;
}

function Channel_isbanned(banned_nuh) {
	for (this_ban in this.modelist[CHANLIST_BAN]) {
		if (match_irc_mask(banned_nuh,this.modelist[CHANLIST_BAN][this_ban]))
			return 1;
	}
	return 0;
}

function Channel_occupants() {
	chan_occupants="";
	for(thisChannel_user in this.users) {
		Channel_user=Clients[this.users[thisChannel_user]];
		if (Channel_user.nick &&
		    (Channel_user.conntype != TYPE_SERVER)) {
			if (chan_occupants)
				chan_occupants += " ";
			if (this.ismode(Channel_user.id,CHANLIST_OP))
				chan_occupants += "@";
			if (this.ismode(Channel_user.id,CHANLIST_VOICE))
				chan_occupants += "+";
			chan_occupants += Channel_user.nick;
		}
	}
	return chan_occupants;
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
}

function ZLine(ipmask,reason) {
	this.ipmask = ipmask;
	this.reason = reason;
}

function WhoWas(nick,uprefix,host,realname,server,serverdesc) {
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
