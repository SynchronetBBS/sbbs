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
const VERSION_STR = "Synchronet " + system.version + system.revision + "-" + system.platform + " (IRCd by Randy Sommerfeld)";
// This will dump all I/O to and from the server to your Synchronet console.
// It also enables some more verbose WALLOPS, especially as they pertain to
// blocking functions.
// The special "DEBUG" oper command also switches this value.
// FIXME: This introduces privacy concerns.  Should we make it difficult for
// the sysop to snoop on private conversations?  A diligent sysop will always
// be able to snoop regardless (either by custom hacks, or, a packet sniffer.)
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
var ob_sock_timeout = 3;

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
var max_chanlen = 100;			// Maximum channel name length.
var max_nicklen = 30;			// Maximum nickname length.
var max_modes = 6;			// Maximum modes on single MODE command
var max_user_chans = 10;		// Maximum channels users can join
var max_bans = 25;			// Maximum bans (+b) per channel
var max_topiclen = 307;			// Maximum length of topic per channel
var max_kicklen = 307;			// Maximum length of kick reasons

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
const CHANLIST_OP		=0;
const CHANLIST_DEOP		=1;
const CHANLIST_VOICE		=2;
const CHANLIST_DEVOICE		=3;
const CHANLIST_BAN		=4;
const CHANLIST_UNBAN		=5; 

// These are used in the mode crunching section to figure out what character
// to display in the crunched MODE line.
const_modechar = new Array();
const_modechar[CHANLIST_OP]	="o";
const_modechar[CHANLIST_DEOP]	="o";
const_modechar[CHANLIST_VOICE]	="v";
const_modechar[CHANLIST_DEVOICE]="v";
const_modechar[CHANLIST_BAN]	="b";
const_modechar[CHANLIST_UNBAN]	="b";

// Connection Types
const TYPE_EMPTY		=0;
const TYPE_UNREGISTERED		=1;
const TYPE_USER			=2;
const TYPE_USER_REMOTE		=3;
// Type 4 is reserved for a possible future client type.
const TYPE_SERVER		=5;
const TYPE_ULINE		=6;
// Anything 5 or above SHOULD be a server-type connection.

////////// Functions not linked to an object //////////
function ip_to_dec(ip) {
	quads = ip.split(".");
	addr=(quads[0]&0xff)<<24;
	addr|=(quads[1]&0xff)<<16;
	addr|=(quads[2]&0xff)<<8;
	addr|=(quads[3]&0xff);
	return addr;
}

function dec_to_ip(ip) {
	// TODO XXX :)
}

function terminate_everything(terminate_reason) {
	for(thisClient in Clients) {
		var client = Clients[thisClient];
		if (client.local)
			client.quit(terminate_reason,false)
	}
}

function searchbynick(nick) {
	if (!nick)
		return 0;
	for(thisClient in Clients) {
		Client=Clients[thisClient];
		if ((nick.toUpperCase() == Client.nick.toUpperCase()) &&
		    Client.conntype && !Client.server)
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

function searchbyserver(server_name) {
	if (!server_name)
		return 0;
	if (match_irc_mask(servername,server_name))
		return -1; // the server passed to us is our own.
	for(thisServer in Clients) {
		Server=Clients[thisServer];
		if ( (Server.conntype == TYPE_SERVER) &&
		     match_irc_mask(Server.nick,server_name) )
			return Server;
	}
	// if we've had no success so far, try nicknames and glean a server
	// from there.
	for(thisNick in Clients) {
		Nick=Clients[thisNick];
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
		    ((ILines[thisILine].password == "") ||
		     (this.password == ILines[thisILine].password)) &&
		    (match_irc_mask(this.uprefix + "@" + this.hostname,
		     ILines[thisILine].hostmask))
		   )
			return ILines[thisILine].ircclass;
	}
	return 0;
}

// IRC is funky.  A "string" in IRC is anything after a :, and anything before
// that is usually treated as fixed arguments.  So, this function is commonly
// used to fetch 'strings' from all sorts of commands.  PRIVMSG, NOTICE,
// USER, PING, etc.
function ircstring(str,startword) {
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

function oper_notice(ntype,nmessage) {
	for(thisoper in Clients) {
		oper=Clients[thisoper];
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
	if (!the_port && this_cline.port)
		the_port = this_cline.port;
	else if (!the_port)
		the_port = "6667"; // try a safe default.
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

function IRCClient_tweaktmpmode(tmp_bit) {
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

function IRCClient_tweaktmpmodelist(tmp_cl,tmp_ncl) {
	if ((!chan.ismode(this.id,CHANLIST_OP)) &&
	    (!this.server) && (!this.parent)) {
		this.numeric482(chan.nam);
		return 0;
	}
	if (add) {
		tmp_match = false;
		for (lstitem in chan_tmplist[tmp_cl]) {
			if (chan_tmplist[tmp_cl][lstitem] &&
			    (chan_tmplist[tmp_cl][lstitem] ==
			     cm_cmd[mode_args_counter])
			   )
				tmp_match = true;
		}
		if(!tmp_match) {
			chan_tmplist[tmp_cl][chan_tmplist[tmp_cl].length] = cm_cmd[mode_args_counter];
			for (lstitem in chan_tmplist[tmp_ncl]) {
				if (chan_tmplist[tmp_ncl][lstitem] &&
				    (chan_tmplist[tmp_ncl][lstitem] == 
				     cm_cmd[mode_args_counter])
				   )
					chan_tmplist[tmp_ncl][lstitem] = "";
			}
		}
	} else {
		tmp_match = false;
		for (lstitem in chan_tmplist[tmp_ncl]) {
			if (chan_tmplist[tmp_ncl][lstitem] &&
			    (chan_tmplist[tmp_ncl][lstitem] ==
			     cm_cmd[mode_args_counter])
			   )
				tmp_match = true;
		}
		if(!tmp_match) {
			chan_tmplist[tmp_ncl][chan_tmplist[tmp_ncl].length] = cm_cmd[mode_args_counter];
			for (lstitem in chan_tmplist[tmp_cl]) {
				if (chan_tmplist[tmp_cl][lstitem] &&
				    (chan_tmplist[tmp_cl][lstitem] ==
				     cm_cmd[mode_args_counter])
				   )
					chan_tmplist[tmp_cl][lstitem] = "";
			}
		}
	}
}

function count_channels() {
	tmp_counter=0;
	for (tmp_count in Channels) {
		if (Channels[tmp_count])
			tmp_counter++;
	}
	return tmp_counter;
}

function count_servers(count_all) {
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
	if(!count_bit)
		count_bit=USERMODE_NONE;
	tmp_counter=0;
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
	tmp_counter=0;
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
	ILines = new Array();
	KLines = new Array();
	NLines = new Array();
	OLines = new Array();
	QLines = new Array();
	ULines = new Array();
	diepass = "";
	restartpass = "";
	YLines = new Array();
	ZLines = new Array();
	fname="";
	if (config_filename && config_filename.length)
		fname=system.ctrl_dir + config_filename;
	else {
		fname=system.ctrl_dir + "ircd." + system.local_host_name + ".conf";
		if(!file_exists(fname))
			fname=system.ctrl_dir + "ircd." + system.host_name + ".conf";
		if(!file_exists(fname))
			fname=system.ctrl_dir + "ircd.conf";
	}
	conf = new File(fname);
	if (conf.open("r")) {
		log("Reading Config: " + fname);
		while (!conf.eof) {
			conf_line = conf.readln();
			if ((conf_line != null) && conf_line.match("[:]")) {
				arg = conf_line.split(":");
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
						CLines.push(new CLine(arg[1],arg[2],arg[3],arg[4],arg[5]));
						break;
					case "I":
						if (!arg[5])
							break;
						ILines.push(new ILine(arg[1],arg[2],arg[3],arg[4],arg[5]));
						break;
					case "K":
						if (!arg[2])
							break;
						kline_mask = create_ban_mask(arg[1],true);
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
						break;
					case "N":
						if (!arg[5])
							break;
						NLines.push(new NLine(arg[1],arg[2],arg[3],arg[4],arg[5]));
						break;
					case "O":
						if (!arg[5])
							break;
						OLines.push(new OLine(arg[1],arg[2],arg[3],arg[4],parseInt(arg[5])));
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

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
//////////////   End of functions.  Start main() program here.   //////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

Clients = new Array;
Channels = new Array;

server.socket.nonblocking = true;	// REQUIRED!
server.socket.debug = false;		// Will spam your log if true :)

hcc_total = 0;
hcc_users = 0;
hcc_counter = 0;
server_uptime = time();

WhoWasHistory = new Array;
NickHistory = new Array;
whowas_buffer = 10000;
whowas_pointer = 0;
nick_buffer = 10000;
nick_pointer = 0;

// Parse command-line arguments.
config_filename="";
for (cmdarg=0;cmdarg<argc;cmdarg++) {
	if(argv[cmdarg].toLowerCase()=="-f") {
		config_filename = argv[cmdarg+1];
		cmdarg++;
	}
}

read_config_file();
log(VERSION + " started.");

///// Main Loop /////
while (!server.terminated) {

	if(this.branch!=undefined)
		branch.limit=0; // we're not an infinite loop.
	mswait(1); // don't peg the CPU

	if(server.terminated)
		break;

	// Setup a new socket if a connection is accepted.
	if (server.socket.poll()) {
		client_sock=server.socket.accept();
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

	// Poll all sockets for commands, then pass to the appropriate func.
	for(thisClient in Clients) {
		ClientObject=Clients[thisClient];
		if ( (ClientObject != undefined) &&
		     ClientObject.socket && ClientObject.conntype &&
		     ClientObject.local )
			ClientObject.work();
	}

	// Scan C:Lines for servers to connect to automatically.
	for(thisCL in CLines) {
		var my_cline = CLines[thisCL];
		if (my_cline.port && YLines[my_cline.ircclass].connfreq &&
		    !(searchbyserver(my_cline.servername)) &&
		    ((time() - my_cline.lastconnect) > YLines[my_cline.ircclass].connfreq) ) {
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
	this.numeric351=IRCClient_numeric351;
	this.numeric353=IRCClient_numeric353;
	this.numeric401=IRCClient_numeric401;
	this.numeric402=IRCClient_numeric402;
	this.numeric403=IRCClient_numeric403;
	this.numeric411=IRCClient_numeric411;
	this.numeric440=IRCClient_numeric440;
	this.numeric441=IRCClient_numeric441;
	this.numeric442=IRCClient_numeric442;
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
	this.server_nick_info=IRCClient_server_nick_info;
	this.server_chan_info=IRCClient_server_chan_info;
	this.server_info=IRCClient_server_info;
	this.synchronize=IRCClient_synchronize;
	this.decip=0;
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
			this.decip = ip_to_dec(this.socket.remote_ip_address);
		this.connecttime = time();
		this.idletime = time();
		this.talkidle = time();
		this.conntype = TYPE_UNREGISTERED;
		this.server_notice("*** " + VERSION + " (" + serverdesc + ") Ready.");
	}
}

function IRCClient_Quit(str,do_bcast) {
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
		if (do_bcast && (this.conntype > TYPE_UNREGISTERED) )
			this.bcast_to_servers(tmp);
	} else if (this.server) {
		this.netsplit(servername + " " + this.nick);
		this.bcast_to_servers_raw("SQUIT " + this.nick + " :" + str);
	}

	if((server.client_remove!=undefined) &&
	   (this.conntype > TYPE_UNREGISTERED))
		server.client_remove(this.socket);

	if (this.local) {
		this.rawout("ERROR :Closing Link: [" + this.uprefix + "@" + this.hostname + "] (" + str + ")");
		oper_notice("Notice","Client exiting: " + this.nick + " (" + this.uprefix + "@" + this.hostname + ") [" + str + "] [" + this.decip + "]");
		this.socket.close();
	}

	this.conntype=TYPE_EMPTY;
	delete this.nick;
	delete this.socket;
	delete Clients[this.id];
	delete this;
}

function IRCClient_netsplit(ns_reason) {
	if (!ns_reason)
		ns_reason = "net.split.net.split net.split.net.split";
	for (sqclient in Clients) {
		if (Clients[sqclient] && (Clients[sqclient] != null) &&
		    (Clients[sqclient] != undefined)) {
			if ((Clients[sqclient].servername == this.nick) ||
			    (Clients[sqclient].linkparent == this.nick))
				Clients[sqclient].quit(ns_reason,false);
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
			chan = Channels[my_channel];
			this.rawout("TOPIC " + chan.nam + " " + servername + " " + chan.topictime + " :" + chan.topic);
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
	this.rawout("NICK " + sni_client.nick + " " + sni_client.hops + " " + sni_client.created + " " + sni_client.get_usermode() + " " + sni_client.uprefix + " " + sni_client.hostname + " " + sni_client.servername + " " + sni_client.decip + " 0 :" + sni_client.realname);
}

function IRCClient_server_chan_info(sni_chan) {
	this.ircout("SJOIN " + sni_chan.created + " " + sni_chan.nam + " " + sni_chan.chanmode(true) + " :" + sni_chan.occupants())
	modecounter=0;
	modestr="+";
	modeargs="";
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
	if (sni_chan.topic)
		this.ircout("TOPIC " + sni_chan.nam + " rrx.ca " + sni_chan.topictime + " :" + sni_chan.topic);
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
		delete rmchan_obj.nam;
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
function IRCClient_numeric351() {
	this.numeric(351, VERSION + " " + servername + " :" + VERSION_STR);
}

function IRCClient_numeric353(chan, str) {
	// = public @ secret * everything else
	if (Channels[chan].mode&CHANMODE_SECRET)
		ctype_str = "@";
	else
		ctype_str = "=";
	this.numeric("353", ctype_str + " " + Channels[chan].nam + " :" + str);
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

function IRCClient_numeric440(str) {
	this.numeric(440, str + " :Services is currently down, sorry.");
}

function IRCClient_numeric441(str) {
	this.numeric("441", str + " :They aren't on that channel.");
}

function IRCClient_numeric442(str) {
	this.numeric("442", str + " :You're not on that channel.");
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
	this.numeric("481", ":Permission Denied.  You're not an IRC operator.");
}

function IRCClient_numeric482(tmp_chan_nam) {
	this.numeric("482", tmp_chan_nam + " :You're not a channel operator.");
}

//////////////////// Multi-numeric Display Functions ////////////////////

function IRCClient_lusers() {
	this.numeric("251", ":There are " + count_nicks() + " users and " + count_nicks(USERMODE_INVISIBLE) + " invisible on " + count_servers(true) + " servers.");
	this.numeric("252", count_nicks(USERMODE_OPER) + " :IRC operators online.");
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
	numnicks=0;
	tmp="";
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

function IRCClient_num_channels_on() {
	uchan_counter=0;
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
	sent_to_servers = new Array();
	chan_tuc = chan_str.toUpperCase();
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
	delete sent_to_servers;
}

function IRCClient_check_nickname(newnick) {
	newnick = newnick.slice(0,max_nicklen);
	// If you're trying to NICK to yourself, drop silently.
	if(newnick.toUpperCase() == this.nick.toUpperCase())
		return 0;
	// First, check for valid characters.
	regexp="^[A-Za-z\{\}\`\^\_\|\\]\\[\\\\][A-Za-z0-9\-\{\}\`\^\_\|\\]\\[\\\\]*$";
	if(!newnick.match(regexp)) {
		this.numeric("432", newnick + " :Foobar'd Nickname.");
		return 0;
	}
	// Second, check for existing nickname.
	if(searchbynick(newnick)) {
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
	userchans="";
	for (i in wi.channels) {
		if(wi.channels[i] &&
		   (!(Channels[wi.channels[i]].mode&CHANMODE_SECRET ||
		      Channels[wi.channels[i]].mode&CHANMODE_PRIVATE) ||
		     this.onchannel(wi.channels[i]))) {
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
	// First, make sure the nick exists.
	var service_nickname = searchbynick(svcnick);
	if (!service_nickname) {
		this.numeric440(svcnick);
		return 0;
	}
	var service_server = searchbyserver(service_nickname.servername);
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
		if (target.match("[@]+")) {
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
			    !this.server)
				this.numeric(301, target_socket.nick + " :" + target_socket.away);
		} else {
			this.numeric401(target);
			return 0;
		}
	}
	return 1;
}

function IRCClient_do_join(chan_name,join_key) {
	if((chan_name[0] != "#") && (chan_name[0] != "&") && !this.parent) {
		this.numeric403(chan_name);
		return 0;
	}
	for (theChar in chan_name) {
		theChar_code = chan_name[theChar].charCodeAt(0);
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
	chan = chan_name.toUpperCase().slice(0,max_chanlen);
	if (Channels[chan] != undefined) {
		if (!this.parent) {
			if ((Channels[chan].mode&CHANMODE_INVITE) &&
			    (chan != this.invited)) {
				this.numeric("473", Channels[chan].nam + " :Cannot join channel (+i: invite only)");
				return 0;
			}
			if ((Channels[chan].mode&CHANMODE_LIMIT) &&
			    (Channels[chan].count_users() >= Channels[chan].limit)) {
				this.numeric("471", Channels[chan].nam + " :Cannot join channel (+l: channel is full)");
				return 0;
			}
			if ((Channels[chan].mode&CHANMODE_KEY) &&
			    (Channels[chan].key != join_key)) {
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
		str="JOIN :" + Channels[chan].nam;
		if (this.parent)
			this.bcast_to_channel(Channels[chan].nam, str, false);
		else
			this.bcast_to_channel(Channels[chan].nam, str, true);
		if (chan_name[0] != "&")
			server_bcast_to_servers(":" + servername + " SJOIN " + Channels[chan].created + " " + Channels[chan].nam + " " + Channels[chan].chanmode() + " :" + this.nick);
	} else {
		// create a new channel
		Channels[chan]=new Channel(chan);
		Channels[chan].nam=chan_name.slice(0,max_chanlen);
		Channels[chan].mode=CHANMODE_NONE;
//		Channels[chan].mode|=CHANMODE_TOPIC;
//		Channels[chan].mode|=CHANMODE_NOOUTSIDE;
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
//		this.ircout("MODE " + chan_name + " " + Channels[chan].chanmode());
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
	for(thisChannel in this.channels) {
		partingChannel=this.channels[thisChannel];
		this.do_part(Channels[partingChannel].nam);
	}
}

function IRCClient_get_usermode() {
	tmp_mode = "+";
	if (this.mode&USERMODE_INVISIBLE)
		tmp_mode += "i";
	if (this.mode&USERMODE_OPER)
		tmp_mode += "o";
	return tmp_mode;
}

function IRCClient_set_chanmode(cmdline,bounce_modes) {
	cm_cmd=cmdline.split(' ');
	chan = searchbychannel(cm_cmd[1]);
	if (!chan)
		return;
	modestr = cm_cmd[2];
	add=true;
	mode_args=new Array();
	mode_args_counter=2;
	addbits=CHANMODE_NONE;
	delbits=CHANMODE_NONE;
	chan_key="";
	chan_limit="";
	chan_tmplist=new Array();
	chan_tmplist[CHANLIST_OP]=new Array();
	chan_tmplist[CHANLIST_DEOP]=new Array();
	chan_tmplist[CHANLIST_VOICE]=new Array();
	chan_tmplist[CHANLIST_DEVOICE]=new Array();
	chan_tmplist[CHANLIST_BAN]=new Array();
	chan_tmplist[CHANLIST_UNBAN]=new Array();
	mode_counter=0;
	for (modechar in modestr) {
		mode_counter++;
		switch (modestr[modechar]) {
			case "+":
				add=true;
				mode_counter--;
				break;
			case "-":
				add=false;
				mode_counter--;
				break;
			case "b":
				mode_args_counter++;
				if(add && !cm_cmd[mode_args_counter]) {
					addbits|=CHANMODE_BAN; // list bans
					break;
				}
				this.tweaktmpmodelist(CHANLIST_BAN,CHANLIST_UNBAN);
				break;
			case "i":
				this.tweaktmpmode(CHANMODE_INVITE);
				break;
			case "k":
				mode_args_counter++;
				if(cm_cmd[mode_args_counter]) {
					this.tweaktmpmode(CHANMODE_KEY);
					chan_key=cm_cmd[mode_args_counter];
				}
				break;
			case "l":
				mode_args_counter++;
				this.tweaktmpmode(CHANMODE_LIMIT);
				if(add && cm_cmd[mode_args_counter]) {
					regexp = "^[0-9]{1,4}$";
					if(cm_cmd[mode_args_counter].match(regexp))
						chan_limit=cm_cmd[mode_args_counter];
				}
				break;
			case "m":
				this.tweaktmpmode(CHANMODE_MODERATED);
				break;
			case "n":
				this.tweaktmpmode(CHANMODE_NOOUTSIDE);
				break;
			case "o":
				mode_args_counter++;
				if(!cm_cmd[mode_args_counter])
					break;
				this.tweaktmpmodelist(CHANLIST_OP,CHANLIST_DEOP);
				break;
			case "p":
				this.tweaktmpmode(CHANMODE_PRIVATE);
				break;
			case "s":
				this.tweaktmpmode(CHANMODE_SECRET);
				break;
			case "t":
				this.tweaktmpmode(CHANMODE_TOPIC);
				break;
			case "v":
				mode_args_counter++;
				if (!cm_cmd[mode_args_counter])
					break;
				this.tweaktmpmodelist(CHANLIST_VOICE,CHANLIST_DEVOICE);
				break;
			default:
				if ((!this.parent) && (!this.server))
					this.numeric("472", modestr[modechar] + " :is unknown mode char to me.");
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

	if (bounce_modes) {
		bouncemodes=CHANMODE_NONE;
		bouncemodes|=CHANMODE_INVITE;
		bouncemodes|=CHANMODE_KEY;
		bouncemodes|=CHANMODE_LIMIT;
		bouncemodes|=CHANMODE_MODERATED;
		bouncemodes|=CHANMODE_NOOUTSIDE;
		bouncemodes|=CHANMODE_PRIVATE;
		bouncemodes|=CHANMODE_SECRET;
		bouncemodes|=CHANMODE_TOPIC;
	}

	if ((addbits&CHANMODE_INVITE) && !(chan.mode&CHANMODE_INVITE)) {
		addmodes += "i";
		chan.mode |= CHANMODE_INVITE;
		if (bounce_modes)
			bouncemodes &= ~CHANMODE_INVITE;
	} else if ((delbits&CHANMODE_INVITE) && (chan.mode&CHANMODE_INVITE)) {
		delmodes += "i";
		chan.mode &= ~CHANMODE_INVITE;
		if (bounce_modes)
			bouncemodes |= CHANMODE_INVITE;
	}
	if ((addbits&CHANMODE_KEY) && chan_key && chan.key) {
		if (!this.server && !this.parent)
			this.numeric("467", chan.nam + " :Channel key aready set.");
	} else if ((addbits&CHANMODE_KEY) && chan_key && !chan.key) {
		addmodes += "k";
		chan.mode |= CHANMODE_KEY;
		addmodeargs += " " + chan_key;
		chan.key = chan_key;
		if (bounce_modes)
			bouncemodes &= ~CHANMODE_KEY;
	} else if ((delbits&CHANMODE_KEY) && (chan.mode&CHANMODE_KEY) &&
		   (chan.key == chan_key) ) {
		delmodes += "k";
		chan.mode &= ~CHANMODE_KEY;
		delmodeargs += " " + chan_key;
		chan.key = "";
		if (bounce_modes)
			bouncemodes |= CHANMODE_KEY;
	}
	if ((addbits&CHANMODE_LIMIT) && chan_limit && (chan_limit != chan.limit)) {
		addmodes += "l";
		chan.mode |= CHANMODE_LIMIT;
		addmodeargs += " " + chan_limit;
		chan.limit = chan_limit;
		if (bounce_modes)
			bouncemodes &= ~CHANMODE_LIMIT;
	} else if ((delbits&CHANMODE_LIMIT) && (chan.mode&CHANMODE_LIMIT)) {
		delmodes += "l";
		chan.mode &= ~CHANMODE_LIMIT;
		if (bounce_modes)
			bouncemodes |= CHANMODE_LIMIT;
	}
	if ((addbits&CHANMODE_MODERATED) && !(chan.mode&CHANMODE_MODERATED)) {
		addmodes += "m";
		chan.mode |= CHANMODE_MODERATED;
		if (bounce_modes)
			bouncemodes &= ~CHANMODE_MODERATED;
	} else if ((delbits&CHANMODE_MODERATED) && (chan.mode&CHANMODE_MODERATED)) {
		delmodes += "m";
		chan.mode &= ~CHANMODE_MODERATED;
		if (bounce_modes)
			bouncemodes |= CHANMODE_MODERATED;
	}
	if ((addbits&CHANMODE_NOOUTSIDE) && !(chan.mode&CHANMODE_NOOUTSIDE)) {
		addmodes += "n";
		chan.mode |= CHANMODE_NOOUTSIDE;
		if (bounce_modes)
			bouncemodes &= ~CHANMODE_NOOUTSIDE;
	} else if ((delbits&CHANMODE_NOOUTSIDE) && (chan.mode&CHANMODE_NOOUTSIDE)) {
		delmodes += "n";
		chan.mode &= ~CHANMODE_NOOUTSIDE;
		if (bounce_modes)
			bouncemodes |= CHANMODES_NOOUTSIDE;
	}
	if ((delbits&CHANMODE_PRIVATE) && (chan.mode&CHANMODE_PRIVATE)) {
		delmodes += "p";
		chan.mode &= ~CHANMODE_PRIVATE;
		if (bounce_modes)
			bouncemodes |= CHANMODE_PRIVATE;
	}
	if ((delbits&CHANMODE_SECRET) && (chan.mode&CHANMODE_SECRET)) {
		delmodes += "s";
		chan.mode &= ~CHANMODE_SECRET;
		if (bounce_modes)
			bouncemodes |= CHANMODE_SECRET;
	}
	if ((addbits&CHANMODE_PRIVATE) && !(chan.mode&CHANMODE_PRIVATE) &&
	    !(chan.mode&CHANMODE_SECRET)) {
		addmodes += "p";
		chan.mode |= CHANMODE_PRIVATE;
		if (bounce_modes)
			bouncemodes &= ~CHANMODE_PRIVATE;
	}
	if ((addbits&CHANMODE_SECRET) && !(chan.mode&CHANMODE_SECRET) &&
	    !(chan.mode&CHANMODE_PRIVATE)) {
		addmodes += "s";
		chan.mode |= CHANMODE_SECRET;
		if (bounce_modes)
			bouncemodes &= ~CHANMODE_SECRET;
	}
	if ((addbits&CHANMODE_TOPIC) && !(chan.mode&CHANMODE_TOPIC)) {
		addmodes += "t";
		chan.mode |= CHANMODE_TOPIC;
		if (bounce_modes)
			bouncemodes &= ~CHANMODE_TOPIC;
	} else if ((delbits&CHANMODE_TOPIC) && (chan.mode&CHANMODE_TOPIC)) {
		delmodes += "t";
		chan.mode &= ~CHANMODE_TOPIC;
		if (bounce_modes)
			bouncemodes |= CHANMODE_TOPIC;
	}
	if (addbits&CHANMODE_BAN) {
		for (the_ban in chan.modelist[CHANLIST_BAN]) {
			this.numeric("367", chan.nam + " " + chan.modelist[CHANLIST_BAN][the_ban] + " " + chan.bancreator[the_ban] + " " + chan.bantime[the_ban]);
		}
		this.numeric("368", chan.nam + " :End of Channel Ban List.");
	}

	if (bounce_modes) {
		str = "MODE " + chan.nam + " " + inverse_chanmode(bouncemodes);
		this.bcast_to_channel(chan.nam, str, false);
		for (op in chan.modelist[CHANLIST_OP]) {
			str = "MODE " + chan.nam + " -o " + Clients[chan.modelist[CHANLIST_OP][op]].nick;
			chan.del_modelist(chan.modelist[CHANLIST_OP][op],CHANLIST_OP);
			this.bcast_to_channel(chan.nam, str, false);
		}
		for (voice in chan.modelist[CHANLIST_VOICE]) {
			str = "MODE " + chan.nam + " -v " + Clients[chan.modelist[CHANLIST_VOICE][voice]].nick;
			chan.del_modelist(chan.modelist[CHANLIST_VOICE][voice],CHANLIST_VOICE);
			this.bcast_to_channel(chan.nam, str, false);
		}
		for (ban in chan.modelist[CHANLIST_BAN]) {
			str = "MODE " + chan.nam + " -b " + Clients[chan.modelist[CHANLIST_VOICE][voice]].nick;
			chan.del_modelist(chan.modelist[CHANLIST_BAN][ban],CHANLIST_BAN);
			this.bcast_to_channel(chan.nam, str, false);
		}
	}

	this.affect_mode_list(true,CHANLIST_VOICE);
	this.affect_mode_list(false,CHANLIST_DEVOICE);
	this.affect_mode_list(true,CHANLIST_OP);
	this.affect_mode_list(false,CHANLIST_DEOP);

	for (unban in chan_tmplist[CHANLIST_UNBAN]) {
		for (ban in chan.modelist[CHANLIST_BAN]) {
			if (chan_tmplist[CHANLIST_UNBAN][unban] ==
			    chan.modelist[CHANLIST_BAN][ban]) {
				delmodes += "b";
				delmodeargs += " " + chan_tmplist[CHANLIST_UNBAN][unban];
				banid = chan.del_modelist(chan_tmplist[CHANLIST_UNBAN][unban],CHANLIST_BAN);
				delete chan.bantime[banid];
				delete chan.bancreator[banid];
			}
		}
	}

	for (ban in chan_tmplist[CHANLIST_BAN]) {
		set_ban = create_ban_mask(chan_tmplist[CHANLIST_BAN][ban]);
		if (chan.count_modelist(CHANLIST_BAN) >= max_bans) {
			this.numeric(478, chan.nam + " " + set_ban + " :Cannot add ban, channel's ban list is full.");
		} else if (set_ban && !chan.isbanned(set_ban)) {
			addmodes += "b";
			addmodeargs += " " + set_ban;
			banid = chan.add_modelist(set_ban,CHANLIST_BAN);
			chan.bantime[banid] = time();
			chan.bancreator[banid] = this.ircnuh;
		}
	}
	
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

	str = "MODE " + chan.nam + " " + final_modestr;
	if (!this.server)
		this.bcast_to_channel(chan.nam, str, true);
	else
		this.bcast_to_channel(chan.nam, str, false);
	if (chan.nam[0] != "&")
		this.bcast_to_servers(str);

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
				if (add && (this.parent)) {
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

function IRCClient_affect_mode_list(tmp_add,list_bit) {
	for (tmp_index in chan_tmplist[list_bit]) {
		tmp_nick = searchbynick(chan_tmplist[list_bit][tmp_index]);
		if (!tmp_nick)
			tmp_nick = searchbynick(search_nickbuf(chan_tmplist[list_bit][tmp_index]));
		if (tmp_nick) { // FIXME: check for user existing on channel?
			if (tmp_add && (!chan.ismode(tmp_nick.id,list_bit))) {
				addmodes += const_modechar[list_bit];
				addmodeargs += " " + tmp_nick.nick;
				chan.add_modelist(tmp_nick.id,list_bit);
			} else if (chan.ismode(tmp_nick.id,list_bit-1)) {
				delmodes += const_modechar[list_bit];
				delmodeargs += " " + tmp_nick.nick;
				chan.del_modelist(tmp_nick.id,list_bit-1);
			}
		} else {
			this.numeric401(chan_tmplist[list_bit][tmp_index]);
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
				this.numeric461(command);
				break;
			}
			if (cmd[1][0] == ":")
				cmd[1] = cmd[1].slice(1);
			this.ircout("PONG " + servername + " :" + cmd[1]);
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
			this_nline = "";
			for (nl in NLines) {
				if ((NLines[nl].password == this.password) &&
				    (NLines[nl].servername == cmd[1])) {
					this_nline = NLines[nl];
					break;
				}
			}
			if (!this_nline) {
				this.rawout("ERROR :Server not configured.");
				this.quit("ERROR: Server not configured.");
				return 0;
			}
			if (searchbyserver(cmd[1])) {
				this.rawout("ERROR :Server already exists.");
				this.quit("ERROR: Server already exists.");
				return 0;
			}
			this.nick = cmd[1];
			this.hops = cmd[2];
			this.realname = ircstring(cmdline);
			this.server = true;
			this.conntype = TYPE_SERVER;
			this.linkparent = servername;
			this.parent = this.id;
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
		this.quit("You've been K:Lined from this server.");
		return 0;
	}
	if (this.password && (unreg_username || (this.nick != "*"))) {
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
	if (this.realname && this.uprefix && (this.nick != "*")) {
		// Check for a valid I:Line.
		this.ircclass = this.searchbyiline();
		if (!this.ircclass) {
			this.quit("You are not authorized to use this server.");
			return 0;
		}
		// We meet registration criteria. Continue.
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
		server_bcast_to_servers("NICK " + this.nick + " 1 " + this.created + " " + this.get_usermode() + " " + this.uprefix + " " + this.hostname + " " + servername + " " + this.decip + " 0 :" + this.realname);
	} else if (this.nick.match("[.]") && this.hops && this.realname &&
		   this.server && (this.conntype == TYPE_SERVER)) {
		hcc_counter++;
		oper_notice("Routing","Link with " + this.nick + "[unknown@" + this.hostname + "] established, states: TS");
		if (server.client_update != undefined)
			server.client_update(this.socket, this.nick, this.hostname);
		if (!this.sentps) {
			for (cl in CLines) {
				if(CLines[cl].servername == this.nick) {
					this.rawout("PASS " + CLines[cl].password + " :TS");
					break;
				}
			}
			this.rawout("CAPAB " + server_capab);
			this.rawout("SERVER " + servername + " 1 :" + serverdesc);
			this.sentps = true;
		}
		this.bcast_to_servers_raw(":" + servername + " SERVER " + this.nick + " 2 :" + this.realname);
		this.synchronize();
	}
}

// Registered users are ConnType 2
function IRCClient_registered_commands(command, cmdline) {
	if (command.match(/^[0-9]+/))
		return 0; // ignore numerics from clients.
	cmd=cmdline.split(' ');
	switch(command) {
		case "ADMIN":
			if (Admin1 && Admin2 && Admin3) {
				this.numeric(256, ":Administrative info about " + servername);
				this.numeric(257, ":" + Admin1);
				this.numeric(258, ":" + Admin2);
				this.numeric(259, ":" + Admin3);
			} else {
				this.numeric(423, servername + " :No administrative information available.");
			}
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
			// CONNECT <server> <port> <remote-on-network>
			if (!(this.mode&USERMODE_OPER)) {
				this.numeric481();
				break;
			}
			if (!cmd[1]) {
				this.numeric461("CONNECT");
				break;
			}
			con_cline = "";
			for (ccl in CLines) {
				if (match_irc_mask(CLines[ccl].servername,cmd[1]) ||
				     match_irc_mask(CLines[ccl].host,cmd[1]) ) {
					con_cline = CLines[ccl];
					break;
				}
			}
			if (!con_cline) {
				this.numeric402(cmd[1]);
				break;
			}
			if (!cmd[2] && con_cline.port)
				cmd[2] = con_cline.port;
			if (!cmd[2] && !con_cline.port)
				cmd[2] = "6667";
			if (!cmd[2].match(/^[0-9]+$/)) {
				this.server_notice("Invalid port: " + cmd[2]);
				break;
			}
			oper_notice("Routing","from " + servername + ": Local CONNECT " + con_cline.servername + " " + cmd[2] + " from " + this.nick + "[" + this.uprefix + "@" + this.hostname + "]");
			connect_to_server(con_cline,cmd[2]);
			break;
		case "DEBUG":
			if (!(this.mode&USERMODE_OPER)) {
				this.numeric481();
				break;
			}
			if (!cmd[1]) {
				this.server_notice("Usage:");
				this.server_notice("  DEBUG D       - Toggle DEBUG mode on/off");
				this.server_notice("  DEBUG Y <val> - Set yield frequency to <val>");
				break;
			}
			switch (cmd[1][0].toUpperCase()) {
				case "D":
					if (debug) {
						debug=false;
						oper_notice("Notice","Debug mode disabled by " + this.ircnuh);
						log("!NOTICE debug mode disabled by " + this.ircnuh);
					} else {
						debug=true;
						oper_notice("Notice","Debug mode enabled by " + this.ircnuh);
						log("!NOTICE debug mode enabled by " + this.ircnuh);
					}
					break;
				case "Y":
					if (cmd[2]) {
						oper_notice("Notice","branch.yield_freq set to " + cmd[2] + " by " + this.ircnuh);
						branch.yield_freq = parseInt(cmd[2]);
					}
					break;
				default:
					break;
			}
			break;
		case "EVAL":	/* Evaluate a JavaScript expression */
			if (!(this.mode&USERMODE_OPER)) {
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
			this.numeric("371", ":" + VERSION + " Copyright 2003 Randy Sommerfeld.");
			this.numeric("371", ":" + system.version_notice + " " + system.copyright + ".");
			this.numeric("371", ": ");
			this.numeric("371", ":--- A big thanks to the following for their assistance: ---");
			this.numeric("371", ":     Deuce: Hacking and OOP conversions.");
			this.numeric("371", ":DigitalMan: Additional hacking and API stuff.");
			this.numeric("371", ": ");
			this.numeric("371", ":Running on " + system.os_version);
			this.numeric("371", ":Compiled with " + system.compiled_with + " at " + system.compiled_when);
			this.numeric("371", ":Socket Library: " + system.socket_lib);
			this.numeric("371", ":Message Base Library: " + system.msgbase_lib);
			this.numeric("371", ":JavaScript Library: " + system.js_version);
			this.numeric("371", ":This BBS has been up since " + system.timestr(system.uptime));
			this.numeric("371", ": ");
			this.numeric("371", ":This program is free software; you can redistribute it and/or modify");
			this.numeric("371", ":it under the terms of the GNU General Public License as published by");
			this.numeric("371", ":the Free Software Foundation; either version 2 of the License, or");
			this.numeric("371", ":(at your option) any later version.");
			this.numeric("371", ": ");
			this.numeric("371", ":This program is distributed in the hope that it will be useful,");
			this.numeric("371", ":but WITHOUT ANY WARRANTY; without even the implied warranty of");
			this.numeric("371", ":MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the");
			this.numeric("371", ":GNU General Public License for more details:");
			this.numeric("371", ":http://www.gnu.org/licenses/gpl.txt");
			this.numeric("374", ":End of /INFO list.");
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
			the_channels = cmd[1].split(",");
			the_keys = "";
			if (cmd[2])
				the_keys = cmd[2].split(",");
			key_counter = 0;
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
			nickid = searchbynick(cmd[2]);
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
					target.quit("Local kill by " + this.nick + " (" + reason + ")",true);
				} else if (target) {
					server_bcast_to_servers(":" + this.nick + " KILL " + target.nick + " :" + reason);
					target.quit("Killed (" + this.nick + " (" + reason + "))");
				} else {
					this.numeric401(kills[kill]);
				}
			}
			break;
		case "KLINE":
			if (!(this.mode&USERMODE_OPER)) {
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
			if (!(this.mode&USERMODE_OPER)) {
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
			for(thisServer in Clients) {
				Server=Clients[thisServer];
				if (Server && 
				    (Server.conntype == TYPE_SERVER) )
					this.numeric(364, Server.nick + " " + Server.linkparent + " :" + Server.hops + " " + Server.realname);
			}
			this.numeric(364, servername + " " + servername + " :0 " + serverdesc);
			this.numeric(365, "* :End of /LINKS list.");
			break;
		case "LIST":
			// ignore args for now
			this.numeric("321", "Channel :Users  Name");
			for(chan in Channels) {
				if (Channels[chan].mode&CHANMODE_PRIVATE)
					channel_name = "Priv";
				else
					channel_name = Channels[chan].nam
				if (!(Channels[chan].mode&CHANMODE_SECRET))
					this.numeric(322, channel_name + " " + Channels[chan].count_users() + " :" + Channels[chan].topic);
			}
			this.numeric("323", "End of /LIST");
			break;
		case "LUSERS":
			this.lusers();
			break;
		case "MODE":
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
					// we're setting/looking at a mode
					this.set_chanmode(cmdline);
				} else if (cmd[1].toUpperCase() == this.nick.toUpperCase()) {
					this.setusermode(cmd[2]);
				} else {
					this.numeric("502", ":Can't change mode for other users.");
				}
			}
			break;
		case "MOTD":
			this.motd();
			break;
		case "NAMES":
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
				this.numeric(412," :No text to send.");
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
				if((cmd[1].toUpperCase() ==
				    OLines[ol].nick.toUpperCase()) &&
				   (match_irc_mask(this.uprefix + "@" +
				    this.hostname,OLines[ol].hostmask)) &&
				   ((OLines[ol].password == "SYSTEM_PASSWORD")
				    && (system.check_syspass(cmd[2]))) ||
				   ((cmd[2] == OLines[ol].password) &&
				    ((OLines[ol].password != "SYSTEM_PASSWORD")
				))) {
					oper_success=true;
					this.ircclass = OLines[ol].ircclass;
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
				log("!WARNING " + this.ircnuh + " tried to OPER unsuccessfully!");
			}
			break;
		case "PART":
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
				this.numeric461("PING");
			} else {
				if (cmd[1][0] == ":") 
					cmd[1] = cmd[1].slice(1);
				this.ircout("PONG " + servername + " :" + cmd[1]);
			}
			break;
		case "PONG":
			this.pinged = false;
			break;
		case "QUIT":
			this.quit(ircstring(cmdline),true);
			break;
		case "PRIVMSG":
			if (!cmd[1]) {
				this.numeric411("PRIVMSG");
				break;
			}
			if ( !cmd[2] || ( !cmd[3] && (
			     (cmd[2] == ":") && (ircstring(cmdline) == "")
			   ) ) ) {
				this.numeric(412," :No text to send.");
				break;
			}
			privmsg_targets = cmd[1].split(',');
			for (privmsg in privmsg_targets) {
				this.do_msg(privmsg_targets[privmsg],"PRIVMSG",ircstring(cmdline));
			}
			this.talkidle = time();
			break;
		case "DIE":
			if (!(this.mode&USERMODE_OPER)) {
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
			terminated = true;
			break;
		case "REHASH":
			if (!(this.mode&USERMODE_OPER)) {
				this.numeric481();
				break;
			}
			this.numeric(382, this.nick + " ircd.conf :Rehashing.");
			oper_notice("Notice",this.nick + " is rehashing Server config file while whistling innocently");
			log("!NOTICE Rehashing server config as per " + this.ircnuh);
			read_config_file();
			break;
		case "RESTART":
			if (!(this.mode&USERMODE_OPER)) {
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
			rs_str = "Aieeeee!!!  Restarting server...";
			oper_notice("Notice",rs_str);
			log("!WARNING " + rs_str + " per " + this.ircnuh);
			terminate_everything(rs_str);
			exit();
			break;
		case "SQUIT":
			if (!(this.mode&USERMODE_OPER)) {
				this.numeric481();
				break;
			}
			if(!cmd[1])
				break;
			sq_server = searchbyserver(cmd[1]);
			if(!sq_server) {
				this.numeric402(cmd[1]);
				break;
			}
			reason = ircstring(cmdline);
			if (!reason)
				reason = this.nick;
			if (sq_server == -1) {
				this.quit(reason);
				break;
			}
			oper_notice("Routing","from " + servername + ": Received SQUIT " + cmd[1] + " from " + this.nick + "[" + this.uprefix + "@" + this.hostname + "] (" + ircstring(cmdline) + ")");
			sq_server.quit(ircstring(cmdline));
			break;
		case "STATS":
			if(!cmd[1])
				break;
			switch (cmd[1][0]) {
				case "C":
				case "c":
					for (cline in CLines) {
						if(CLines[cline].port)
							cline_port = CLines[cline].port;
						else
							cline_port = "*";
						this.numeric(213,"C " + CLines[cline].host + " * " + CLines[cline].servername + " " + cline_port + " " + CLines[cline].ircclass);
					}
					break;
				case "H":
				case "h":
					this.numeric(244,"H hostmask * servername");
					break;
				case "I":
				case "i":
					for (iline in ILines) {
						if (!ILines[iline].port)
							var my_port = "*";
						else
							var my_port = ILines[iline].port;
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
				case "l":
					this.numeric(211,"<linkname> <sendq> <sentmessages> <sentbytes> <receivedmessages> <receivedbytes> <timeopen>");
					break;
				case "M":
				case "m":
					this.numeric(212,"<command> <count>");
					break;
				case "N":
				case "n":
					for (nline in NLines) {
						this.numeric(214, "N " + NLines[nline].host + " * " + NLines[nline].servername + " " + NLines[nline].flags + " " + NLines[nline].ircclass);
					}
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
					upsecs=time() - server_uptime;
					updays=upsecs / 86400;
					if (updays < 1)
						updays=0;
					uphours=(upsecs-(updays*86400))/3600;
					if (uphours < 1)
						uphours=0;
					upmins=((upsecs-(updays*86400))-(uphours*3600))/60;
					if (upmins < 1)
						upmins=0;
					upsec=(((upsecs-(updays*86400))-(uphours*3600))-(upmins*60));
					this.numeric(242,":Server Up " + updays + " days " + uphours + ":" + upmins + ":" + upsec);
					break;
				case "Y":
				case "y":
					for (thisYL in YLines) {
						var yl = YLines[thisYL];
						this.numeric(218,"Y " + thisYL + " " + yl.pingfreq + " " + yl.connfreq + " " + yl.maxlinks + " " + yl.sendq);
					}
					break;
				default:
					break;
			}
			this.numeric(219,cmd[1][0]+" :End of /STATS Command.");
			break;
		case "SUMMON":
			if(!cmd[1]) {
				this.numeric411('SUMMON');
			}
			else {
				// Check if exists.
				usernum=system.matchuser(cmd[1]);
				if(!usernum)
					this.numeric(444,":No such user.");
				else {
					// Check if logged in
					isonline=0;
					for(node in system.node_list) {
						if(system.node_list[node].status == NODE_INUSE && system.node_list[node].useron == usernum)
							isonline=1;
					}
					if(!isonline)
						this.numeric(444,":User not logged in.");
					else {
						summon_message=ircstring(cmdline);
						if(summon_message != '') {
							summon_message=' ('+summon_message+')';
						system.put_telegram(''+usernum,this.nick+' is summoning you to IRC chat'+summon_message+'\r\n');
						this.numeric(342,cmd[1]+' :Summoning user to IRC');
						}
					}
				}
			}
			break;
		case "TIME":
			this.numeric(391, servername + " :" + strftime("%A %B %e %Y -- %H:%M %z",time()));
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
					tmp_topic = ircstring(cmdline).slice(0,max_topiclen);
					if (tmp_topic == chanid.topic)
						break;
					chanid.topic = tmp_topic;
					chanid.topictime = time();
					chanid.topicchangedby = this.nick;
					str = "TOPIC " + chanid.nam + " :" + chanid.topic;
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
			this.server_notice("TRACE isn't implemented yet.  Sorry.");
			break;
		case "USER":
			this.numeric462();
			break;
		case "USERS":
			this.numeric(392,':UserID                    Terminal  Host');
			usersshown=0;
			for(node in system.node_list) {
				if(system.node_list[node].status == NODE_INUSE) {
					var u=new User(system.node_list[node].useron);
					this.numeric(393,format(':%-25s %-9s %-30s',u.alias,'Node'+node,u.host_name));
					usersshown++;
				}
			}
			if(usersshown) {
				this.numeric(394,':End of users');
			}
			else {
				this.numeric(395,':Nobody logged in');
			}
			break;
		case "USERHOST":
			if (!cmd[1]) {
				this.numeric461("USERHOST");
				break;
			}
			uhnick = searchbynick(cmd[1]);
			if (uhnick)
				this.numeric("302", ":" + uhnick.nick + "=+" + uhnick.uprefix + "@" + uhnick.hostname);
			else
				this.numeric("302", ":");
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
			if (!(this.mode&USERMODE_OPER)) {
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
				channel="*";
			} else if ((cmd[1][0] == "#") || (cmd[1][0] == "&")) {
				chan=cmd[1].toUpperCase();
				if (Channels[chan] != undefined) {
					channel=Channels[chan].nam;
					for(i in Channels[chan].users) {
						if (Channels[chan].users[i]) {
							Client=Clients[Channels[chan].users[i]];
							who_mode="";
							if (Client.away)
								who_mode += "G";
							else
								who_mode += "H";
							if (Channels[chan].ismode(Client.id,CHANLIST_OP))
								who_mode += "@";
							else if (Channels[chan].ismode(Client.id,CHANLIST_VOICE))
								who_mode += "+";
							if (Client.mode&USERMODE_OPER)
								who_mode += "*";
							this.numeric("352", channel + " " + Client.uprefix + " " + Client.hostname + " " + servername + " " + Client.nick + " " + who_mode + " :0 " + Client.realname);
						}
					}
				} else {
					channel="*";
				}
			} else {
				channel="*";
			}
			this.numeric("315", channel + " :End of /WHO list.");
			break;
		case "WHOIS":
			if (!cmd[1]) {
				this.numeric(431, ":No nickname given.");
				break;
			}
			wi_nicks = cmd[1].split(',');
			for (wi_nick in wi_nicks) {
				wi = searchbynick(wi_nicks[wi_nick]);
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
			this.services_msg("ChanServ",ircstring(cmdline));
			break;
		case "NS":
		case "NICKSERV":
			this.services_msg("NickServ",ircstring(cmdline));
			break;
		case "MS":
		case "MEMOSERV":
			this.services_msg("MemoServ",ircstring(cmdline));
			break;
		case "OS":
		case "OPERSERV":
			this.services_msg("OperServ",ircstring(cmdline));
			break;
		case "IDENTIFY":
			if (cmd[1][0] == "#")
				var services_target = "ChanServ";
			else
				var services_target = "NickServ";
			this.services_msg(services_target,"IDENTIFY " + ircstring(cmdline));
			break;
		default:
			this.numeric("421", command + " :Unknown command.");
			break;
	}
}

// Server connections are ConnType 5
function IRCClient_server_commands(origin, command, cmdline) {
	ThisOrigin = searchbynick(origin);
	if (!ThisOrigin)
		ThisOrigin = searchbyserver(origin);
	if (!ThisOrigin)
		ThisOrigin = this;

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
		case "AWAY":
			if (!cmd[1])
				ThisOrigin.away = "";
			else
				ThisOrigin.away = ircstring(cmdline);
			break;
		case "ERROR":
			oper_notice("Notice", "ERROR :from " + ThisOrigin.nick + "[(+)0@" + this.hostname + "] -- " + ircstring(cmdline));
			ThisOrigin.quit();
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
			the_channels = cmd[1].split(",");
			for (jchan in the_channels) {
				if (the_channels[jchan][0] != "#")
					break;
				ThisOrigin.do_join(the_channels[jchan].slice(0,max_chanlen),"");
			}
			break;
		case "SJOIN":
			if (!cmd[2])
				break;
			if (cmd[2][0] != "#")
				break;
			chan = searchbychannel(cmd[2]);
			if (!chan) {
				cn_tuc = cmd[2].toUpperCase();
				Channels[cn_tuc]=new Channel(cn_tuc);
				chan = Channels[cn_tuc];
				chan.nam = cmd[2];
				chan.created = cmd[1];
				chan.topic = "";
				chan.users = new Array();
				chan.modelist[CHANLIST_BAN] = new Array();
				chan.modelist[CHANLIST_VOICE] = new Array();
				chan.modelist[CHANLIST_OP] = new Array();
				chan.mode = CHANMODE_NONE;
			}
			if (cmd[3]) {
				if (chan.created == cmd[1])
					bounce_modes = false;
				else
					bounce_modes = true;
				mode_args="";
				tmp_modeargs=0;
				for (tmpmc in cmd[3]) {
					if (cmd[3][tmpmc] == "k") {
						tmp_modeargs++;
						mode_args += cmd[3 + tmp_modeargs];
					} else if (cmd[3][tmpmc] == "l") {
						tmp_modeargs++;
						mode_args += cmd[3 + tmp_modeargs];
					}
				}
				if (chan.created >= cmd[1]) {
					if (mode_args)
						this.set_chanmode("MODE " + chan.nam + " " + cmd[3] + " " + mode_args, bounce_modes);
					else
						this.set_chanmode("MODE " + chan.nam + " " + cmd[3], bounce_modes);
				}

				if ((cmd[4] == "") && cmd[5])
					tmp_modeargs++;

				chan_members = ircstring(cmdline,4+tmp_modeargs).split(' ');

				for (member in chan_members) {
					is_op = false;
					is_voice = false;
					if (chan_members[member][0] == "@") {
						is_op = true;
						chan_members[member] = chan_members[member].slice(1);
					}
					if (chan_members[member][0] == "+") {
						is_voice = true;
						chan_members[member] = chan_members[member].slice(1);
					}
					member_obj = searchbynick(chan_members[member]);
					if (!member_obj)
						break;
					member_obj.channels.push(chan.nam.toUpperCase());
					chan.users.push(member_obj.id);
					member_obj.bcast_to_channel(chan.nam, "JOIN " + chan.nam, false);
					if (chan.created >= cmd[1]) {
						if (is_op) {
							chan.modelist[CHANLIST_OP].push(member_obj.id);
							this.bcast_to_channel(chan.nam, "MODE " + chan.nam + " +o " + member_obj.nick);
						}
						if (is_voice) {
							chan.modelist[CHANLIST_VOICE].push(member_obj.id);
							this.bcast_to_channel(chan.nam, "MODE " + chan.nam + " +v " + member_obj.nick);
						}
					}
				}
			} else {
				ThisOrigin.channels.push(chan.nam.toUpperCase());
				chan.users.push(ThisOrigin.id);
				ThisOrigin.bcast_to_channel(chan.nam, "JOIN " + chan.nam, false);
			}
			this.bcast_to_servers_raw(":" + servername + " SJOIN " + chan.created + " " + chan.nam + " " + chan.chanmode(true) + " :" + ircstring(cmdline))
			break;
		case "SQUIT":
			if (!cmd[2])
				break;
			sq_server = searchbyserver(cmd[1]);
			if (!sq_server)
				break;
			sq_server.quit(ThisOrigin.nick + " " + sq_server.nick);
			break;
		case "KILL":
			if (!cmd[2])
				break;
			if (cmd[1].match(/[.]+/))
				break;
			if (cmd[2] == ":")
				break;
			reason = ircstring(cmdline);
			kills = cmd[1].split(",");
			for(kill in kills) {
				target = searchbynick(kills[kill]);
				if (!target)
					target = searchbynick(search_nickbuf(kills[kill]));
				if (target) {
					this.bcast_to_servers_raw(":" + ThisOrigin.nick + " KILL " + target.nick + " :" + reason);
					target.quit("KILLED by " + ThisOrigin.nick + " (" + reason + ")",false);
				}
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
			if (cmd[1][0] == "#")
				ThisOrigin.set_chanmode(cmdline);
			break;
		case "NICK":
			if (!cmd[8] && (cmd[2][0] != ":"))
				break;
			collide = searchbynick(cmd[1]);
			if ((collide) && (parseInt(collide.created) <
			    parseInt(cmd[3]) ) ) {
				// FIXME: At the moment, we rely on the remote
				// end to do the right thing.
				break;
			} else if ((collide) && (parseInt(collide.created) >
			    parseInt(cmd[3]) ) ) {
				// Nuke our side of things, allow this newly
				// introduced nick to overrule.
				this.bcast_to_servers("KILL " + collide.nick + " :Nickname Collision.");
				collide.quit("Nickname Collision");
			}
			if (cmd[2][0] == ":") {
				cmd[2] = cmd[2].slice(1);
				ThisOrigin.created = cmd[2];
				ThisOrigin.bcast_to_uchans_unique("NICK " + cmd[1]);
				this.bcast_to_servers_raw(":" + origin + " NICK " + cmd[1] + " :" + cmd[2]);
				push_nickbuf(ThisOrigin.nick,cmd[1]);
				ThisOrigin.nick = cmd[1];
			} else if (cmd[10]) {
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
				NewNick.decip = cmd[9];
				NewNick.setusermode(cmd[4]);
				true_hops = parseInt(NewNick.hops)+1;
				this.bcast_to_servers_raw("NICK " + NewNick.nick + " " + true_hops + " " + NewNick.created + " " + NewNick.get_usermode() + " " + NewNick.uprefix + " " + NewNick.hostname + " " + NewNick.servername + " 0 " + NewNick.decip + " :" + NewNick.realname);
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
			if (!cmd[1]) {
				break;
			} else {
				if (cmd[1][0] == ":")
					cmd[1] = cmd[1].slice(1);
				this.ircout("PONG " + servername + " :" + cmd[1]);              
			}                                                       
			break;
		case "PONG":
			this.pinged = false;
			break;
		case "QUIT":
			ThisOrigin.quit(ircstring(cmdline),true);
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
				break;
			}
			this.bcast_to_servers_raw(":" + newsrv.linkparent + " SERVER " + newsrv.nick + " " + (parseInt(newsrv.hops)+1) + " :" + newsrv.realname);
			break;
		case "TOPIC":
			if (!cmd[4])
				break;
			chan = searchbychannel(cmd[1]);
			if (!chan)
				break;
			chan.topic = ircstring(cmdline);
			chan.topictime = cmd[3];
			chan.topicchangedby = cmd[2];
			str = "TOPIC " + chan.nam + " :" + chan.topic;
			ThisOrigin.bcast_to_channel(chan.nam,str,false);
			this.bcast_to_servers_raw(":" + ThisOrigin.nick + " TOPIC " + chan.nam + " " + ThisOrigin.nick + " " + chan.topictime + " :" + chan.topic);
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
		case "AKILL":
			if (!cmd[6])
				break;
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

function IRCClient_work() {
	if (!this.socket.is_connected) {
		this.quit("Connection reset by peer",true);
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
			log("!ERROR: Client has bogus conntype!");
			oper_notice("Notice","Client has bogus conntype!");
		}
	}
	if (this.nick) {
		if ( (this.pinged) && ((time() - this.pinged) > YLines[this.ircclass].pingfreq) ) {
			this.pinged = false;
			this.quit("Ping Timeout",true);
		} else if (!this.pinged && ((time() - this.idletime) > YLines[this.ircclass].pingfreq)) {
			this.pinged = time();
			this.rawout("PING :" + servername);
		}
	}
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
	this.limit=0;
	this.key="";
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
			tmp_extras += " " + this.key;
	}
	if (this.mode&CHANMODE_LIMIT) {
		tmp_mode += "l";
		if(show_args)
			tmp_extras += " " + this.limit;
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
