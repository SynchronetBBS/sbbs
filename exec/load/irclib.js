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
// Copyright 2003 Randolph Erwin Sommerfeld <sysop@rrx.ca>
//

const IRCLIB_REVISION = "$Revision$".split(' ')[1];
const IRCLIB_VERSION = "irclib.js-0.1(" + IRCLIB_REVISION + ")";

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
function IRC_string(str,arg) {
	var cindex;

	if (arg) {
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
}

// This function is intended to match against so-called "IRC wildcards", which
// is a simple wildcarding syntax (* = match 0 or more characters, ? = always
// match one character only.)  The match is case insensitive.
// EXAMPLE: IRC_match("cyan@weyland-yutani.net","*@weyland-yutani.net");
// RETURNS: Same as Javascript match() (the matched string on success, or
// false on failure)
function IRC_match(mtchstr,mask) {
	var final_mask="^";
	mask=mask.replace(/[.]/g,"\\\.");
	mask=mask.replace(/[?]/g,".");
	mask=mask.replace(/[*]/g,".*?");
	final_mask=final_mask + mask + "$";
	return mtchstr.toUpperCase().match(final_mask.toUpperCase());
}
