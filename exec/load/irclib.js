// $Id$
//
// irclib.js
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
// A library of useful IRC functions and objects that can be used to assist
// in the creation of IRC clients, bots, servers, or custom shells.
//
// If you use this to create something neat, let me know about it! :)
// Either email, or find me on #synchronet, irc.synchro.net, nick 'Cyan'
//
// Copyright 2003-2005 Randolph Erwin Sommerfeld <sysop@rrx.ca>
//

const IRCLIB_REVISION = "$Revision$".split(' ')[1];
const IRCLIB_VERSION = "irclib.js-" + IRCLIB_REVISION;

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

	sock = new Socket();
	sock.connect(hostname,port);
	if (sock.is_connected) {
		if (password)
			sock.send("PASS " + password + "\r\n");
		sock.send("NICK " + nick + "\r\n");
		sock.send("USER " + username + " * * :" + realname + "\r\n");
		return sock;
	} else {
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
		return 0;
	}
}

// Simply takes a string and returns the 'IRC string' (i.e. what's after a :)
// Moves up to 'arg' argument before parsing, if defined.
// RETURNS: The 'IRC string' of 'str'
/* Fixme: Real IRCd behaviour involves ignoring any ":" after the definied
   offset. */
function IRC_string(str,arg) {
	var cindex;
	var sw_counter;
	var sindex;

	if (arg) {
		for(sw_counter=0;sw_counter<arg;sw_counter++) {
			var my_index = str.indexOf(" ");
			if (my_index == -1)
				return ""; /* If we can't get to it, then the str is empty. */
			str=str.slice(str.indexOf(" ")+1);
		}
	}
	cindex = str.indexOf(":")+1;
	if (cindex)
		return(str.slice(cindex));
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
	while(server.is_connected && time()-start<5)
		mswait(500);
}

// This function is intended to match against so-called "IRC wildcards", which
// is a simple wildcarding syntax (* = match 0 or more characters, ? = always
// match one character only.)  The match is case insensitive.
// EXAMPLE: IRC_match("cyan@weyland-yutani.net","*@weyland-yutani.net");
// RETURNS: Same as Javascript match() (the matched string on success, or
// false on failure)
function IRC_match(mtchstr,mask) {
	var uc_mtchstr = mtchstr.toUpperCase();
	var uc_mask = mask.toUpperCase();
	var mask_len = mask.length;
	var mptr = 0;

	for (c in uc_mtchstr) {
		/* Jump to the last '*' in a series of them. */
		while ( (uc_mask[mptr] == "*") && (uc_mask[mptr+1] == "*") )
			mptr++;
		if (uc_mask[mptr] == "*") {
			if (uc_mtchstr[c] == uc_mask[mptr+1])
				mptr += 2;
			continue;
		} else if ( (uc_mtchstr[c] == uc_mask[mptr]) ||
			    (uc_mask[mptr] == "?") ) {
			mptr++;
			continue;
		} else {
			return false; /* Instant failure */
		}
	}
	if ( (mptr >= mask.length) ||
	     ( ((mptr+1) == mask.length) && (uc_mask[mptr] == "*") ) )
		return mtchstr;
	return false;
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
	tmp[0] = str.split("!")[0];
	tmp[1] = str.split("!")[1].split("@")[0];
	tmp[2] = str.split("@")[1];
	return tmp;
}
