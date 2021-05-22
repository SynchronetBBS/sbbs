/*
   irclib.js
  
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
  
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details:
   https://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
  
   A library of useful IRC functions and objects that can be used to assist
   in the creation of IRC clients, bots, servers, or custom shells.
  
   If you use this to create something neat, let me know about it! :)
   Either email, or find me on #synchronet, irc.synchro.net, nick 'Cyan'
  
   Copyright 2003-2021 Randy Sommerfeld <cyan@synchro.net>
*/

const IRCLIB_REVISION = "1.24";
const IRCLIB_VERSION = "irclib.js-" + IRCLIB_REVISION;

/*
	This is the "grand parser" that you should be using to handle all
	incoming messages, whether from a client or server.
	Returns an object "ret" as defined below.
	Note it is possible for nothing to be defined in 'source', in
	which case you should assign the source to the socket name.  i.e.:
	if (!source.name)
		source.name = this.socket.name;
*/
function IRC_parse(str) {
	var w, e;

	var ret = {
		tags: {},
		source: {
			name: "",
			user: "",
			host: "",
			is_server: false
		},
		verb: "",
		params: []
	};

	for (w = 0; str.length; w++) {
		e = str.indexOf(" ");
		if (e == -1)
			e = str.length;
		if (!ret.verb && w < 2) {
			switch(str[0]) {
				case "@":
					ret.tags = IRC_parse_tags(str.substr(0, e));
					str = str.slice(e+1);
					continue;
				case ":":
					ret.source = IRC_parse_source(str.substr(0, e));
					str = str.slice(e+1);
					continue;
			}
		}
		if (!ret.verb) {
			ret.verb = str.substr(0, e).toUpperCase();
			str = str.slice(e+1);
			continue;
		}
		if (str[0] == ":") {
			ret.params.push(str.slice(1));
			break;
		}
		ret.params.push(str.substr(0, e));
		str = str.slice(e+1);
	}

	return ret;
}

/*
	IRCv3 style tag parsing
	Returns an array of tags, with tag name in the key
*/
function IRC_parse_tags(str) {
	var ret = {};
	var e, q;
	
	if (!str || str[0] != "@")
		return ret;

	str = str.slice(1);

	while (str.length) {
		e = str.indexOf(";");
		if (!e || e == -1)
			e = str.length;
		q = str.indexOf("=");
		if (!q || q > e) {
			ret[str.substr(0, e)] = true;
			str = str.slice(e+1);
			continue;
		}
		ret[str.substr(0, q)] = str.substr(q+1, e);
		str = str.slice(e+1);
	}

	return ret;
}

/*
	Returns an object "ret" based on where it's from.
	It's possible this can be empty.
*/
function IRC_parse_source(str) {
	var nuh;
	var ret = {
		name: "",
		user: "",
		host: "",
		is_server: false
	};

	if (!str || str[0] != ":")
		return ret;

	str = str.slice(1);

	if (str.match(/!/)) {
		nuh = IRC_split_nuh(str);
		ret.name = nuh[0];
		ret.user = nuh[1];
		ret.host = nuh[2];
	} else if (str.match(/[.]/)) {
		ret.name = str;
		ret.is_server = true;
	} else {
		ret.name = str;
	}

	return ret;
}

// Connect to a server as a client.
//	hostname	Hostname to connect to
//	nick		Desired nickname
//	username	Desired username (i.e. username@host) [RECOMMENDED]
//	realname	Desired IRCname field [RECOMMENDED]
//	port		Port to connect to [OPTIONAL]
//	password	Password to use (if applicable) [OPTIONAL]
// RETURNS: socket object on success, 0 on failure.
function IRC_client_connect(hostname,nick,username,realname,port,password) {
	var sock;

	if (!port)
		port = 6667;
	if (!username)
		username = "irclib";
	if (!realname)
		realname = IRCLIB_VERSION;

	if (js.global.ConnectedSocket != undefined) {
		try {
			sock = new ConnectedSocket(hostname, port);
		}
		catch(e) {
			return 0;
		}
	}
	else {
		sock = new Socket();
		sock.connect(hostname,port);
	}
	if (sock.is_connected) {
		if (password)
			sock.send("PASS " + password + "\r\n");
		sock.send("NICK " + nick + "\r\n");
		sock.send("USER " + username + " * * :" + realname + "\r\n");
		return sock;
	} else {
		sock.close();
		return 0;
	}
}

// Connect to a server as a server
//	hostname	Hostname to connect to
//	servername	Desired server name
//	password	Password to use [REQUIRED]
//	description	Description of IRC server (realname) [RECOMMENDED]
//	port		Port to connect to [OPTIONAL]
// RETURNS: socket object on success, 0 on failure.
function IRC_server_connect(hostname,servername,password,description,port) {
	var sock;

	if (!port)
		port = 6667;
	if (!description)
		description = IRCLIB_VERSION;

	sock = new Socket();
	sock.connect(hostname,port);
	if (sock.is_connected) {
		sock.send("PASS " + password + "\r\n");
		sock.send("SERVER " + servername + " 1 :" +description+"\r\n");
		return sock;
	} else {
		sock.close();
		return 0;
	}
}

/* Takes a string and returns the proper "IRC string".  Starts scanning on
   the "arg"th word.
   EXAMPLES:
      IRC_string("PRIVMSG Cyan Hello World",2); returns "Hello"
      IRC_string("PRIVMSG Cyan :Hello World",2); returns "Hello World"
   RETURNS:
      The entire string from the "arg"th word and beyond, if the "arg"th word
      begins with a ":".  If it does not, it returns the "arg"th word only.
      If it cannot scan to the "arg"th word, an empty string is returned. */
function IRC_string(str,arg) {
	var cindex;
	var sindex;

	for(sw_counter=0;sw_counter<arg;sw_counter++) {
		var my_index = str.indexOf(" ");
		if (my_index == -1)
			return ""; /* If we can't get to it, then the str is empty. */
		str = str.slice(my_index+1);
	}

	if (str[0] == ":")
		return(str.slice(1));

	sindex = str.indexOf(" ");
	if (sindex != -1)
		return(str.slice(0,sindex));

	return(str);
}

// Takes a string in and strips off the IRC originator, if applicable.
// RETURNS: an array containing the command arguments, cmd[0] is the command,
// uppercased.
function IRC_parsecommand(str) {
	var cmd;

	if (str[0] == ":")
		str = str.slice(str.indexOf(" ")+1);

	if (!str)
		return 0; // nothing in the string!

	cmd = str.split(' ');
	cmd[0] = cmd[0].toUpperCase();
	return cmd;
}

// Quits from a server, regardless of whether we're connected as a client or
// a server.  'server' must be a valid socket object which points to the
// server in question.  'reason' is an optional QUIT message which displays
// your reason for quitting.
// RETURNS: void.
function IRC_quit(server,reason) {
	if (!reason)
		reason = IRCLIB_VERSION;

	server.send("QUIT :" + reason + "\r\n");

	/* wait up to 5 seconds for server to disconnect */
	var start=time();
	while(server.is_connected && time()-start<5) {
		if (server.poll(0.5))
			server.recvline();
	}
}

// This will create a 'default mask' when given a complete user@host string.
// If the 'host' contains less than two dots, then it's returned verbatim,
// but the 'user' portion is always prefixed with a *, and the ~ is lopped off.
// Useful for creating access masks (for bots), or quick ban masks (for bans)
// In the case of a ban, simply prefix the output with '*!' to get a valid ban.
// ** This function is intelligent enough to strip off any 'Nick!' notation
//    before the user@host portion.
// EXAMPLE: user@somehost.com -> *user@somehost.com
//          ~hello@something.cool.com -> *hello@*.cool.com
//          ~hi@whatever.com -> *hi@whatever.com
//          test@10.20.30.40 -> *test@10.20.30.*
// RETURNS: The formatted mask in user@host form.
function IRC_create_default_mask(uh) {
	if (uh.match(/[!]/))
		tmp = uh.slice(uh.indexOf("!")+1);
	var tmp = uh.split("@");
	if (tmp[0][0] == "~")
		tmp[0] = tmp[0].slice(1);
	tmp[0] = tmp[0].slice(0,9); // always make sure there's room.
	// Check to see if we're an IP address
	var last_quad = tmp[1].slice(tmp[1].lastIndexOf(".")+1);
	var uh_chopped;
	if (last_quad == parseInt(last_quad)) { //ip
		uh_chopped = tmp[1].slice(0,tmp[1].lastIndexOf(".")) + ".*";
	} else { //hostname
		uh_chopped = tmp[1].slice(tmp[1].indexOf(".")+1);
		if (uh_chopped.indexOf(".") == -1)
			uh_chopped = tmp[1];
		else
			uh_chopped = "*." + uh_chopped;
	}
	return "*" + tmp[0] + "@" + uh_chopped;
}

// Checks to see if a given nickname is a legal IRC nickname.  Nickname
// length is checked only if provided as an argument, otherwise it's ignored.
// Remember, nickname length can differ per IRC network.
// RETURNS: 1 on failure (nickname is illegal), 0 on success (nick is legal)
function IRC_check_nick(nick,mnicklen) {
	if (mnicklen && (nick.length > mnicklen))
		return 1;
	var regexp="^[A-Za-z\{\}\`\^\_\|\\]\\[\\\\][A-Za-z0-9\-\{\}\`\^\_\|\\]\\[\\\\]*$";
	if (!nick.match(regexp))
		return 1;
	return 0;
}

// This will check to see if the host as passed is valid.  This *only* checks
// the hostname, not anything else (i.e. username, nickname)
// Set 'wilds' to true if you also allow IRC wildcards to be in the hostname.
// Set 'uh' to true if you're allowing '@' (as in, passing the full u@h)
// Set 'nick' to true if passing the entire nick!user@host string
// RETURNS: 1 on failure (illegal hostname), 0 on success (legal hostname)
function IRC_check_host(host,wilds,uh,nick) {
	var regexp = "^[A-Za-z0-9\-\.";
	if (wilds)
		regexp += "\?\*";
	if (uh) {
		if (host.slice(host.indexOf("@")+1).indexOf("@") != -1)
			return 1; // only one @ allowed.
		regexp += "\@";
	}
	if (nick) {
		if (host.slice(host.indexOf("!")+1).indexOf("!") != -1)
			return 1; // only one ! allowed.
		regexp += "\!";
	}
	regexp += "]+$";
	if (!host.match(regexp))
		return 1;
	return 0;
}

// Splits a "nick!user@host" string into its three distinct parts.
// RETURNS: An array containing nick in [0], user in [1], and host in [2].
function IRC_split_nuh(str) {
	var tmp = new Array;

	if (str[0] == ":")
		str = str.slice(1);

	if (str.search(/[!]/) != -1) {
		tmp[0] = str.split("!")[0];
		tmp[1] = str.split("!")[1].split("@")[0];
	} else {
		tmp[0] = undefined;
		tmp[1] = str.split("@")[0];
	}
	tmp[2] = str.split("@")[1];
	return tmp;
}

/* Convert a dotted-quad IP address to an integer, i.e. for use in CTCP */
function ip_to_int(ip) {
	var quads;

	if (!ip)
		return 0;

	quads = ip.split(".");

	return quads[0] * 0x1000000 + quads[1] * 0x10000 + quads[2] * 0x100 + quads[3];
}

/* Convert an integer to an IP address, i.e. for receiving CTCP's */
function int_to_ip(ip) {
	return(format("%u.%u.%u.%u"
		,(ip>>24)&0xff
		,(ip>>16)&0xff
		,(ip>>8)&0xff
		,ip&0xff
	));
}

/* A handy object for keeping track of nicks and their properties */
function IRC_User(nick) {
	this.nick = nick;
	this.realname = "Unknown";
	this.hostname = "unknown@unknown";
	this.server = false; /* IRC_Server Object */
}

/* A handy object for keeping track of channels and their properties */
function IRC_Channel(name) {
	this.name = name;
	this.topic = "No topic is set.";
	this.users = new Array;
}

/* A handy object for keeping track of servers and their properties */
function IRC_Server(name) {
	this.name = name;
	this.info = "Unknown Server";
}

/* Object prototype for defining channel mode behaviour */
function IRC_Channel_Mode(modechar,args,state,list,isnick) {
	this.modechar = modechar;	/* The mode's character */
	this.args = args;			/* Does this mode take only a single arg? */
	this.state = state;			/* Stateful? (changes channel behaviour) */
	this.list = list;			/* Does this mode accept a list? */
	this.isnick = isnick;		/* Is nick (true) or a n!u@h mask (false) */
}
