// $Id$

// Library for reporting user presence (e.g. BBS node listings, who's online)
// Much of the code was derived from src/sbbs3/getnode.cpp: nodelist(), whos_online(), printnodedat()

require("nodedefs.js", 'NODE_INUSE');
require("text.js", 'UNKNOWN_USER');

"use strict";

// Returns a string, hopefully the full name of the External Program referenced by 'code'
function xtrn_name(code)
{
	if(xtrn_area.prog[code] != undefined && xtrn_area.prog[code].name != undefined)
		return xtrn_area.prog[code].name;
	return code;
}

// Returns a string, representing the current node connection (with a prepended space)
function node_connection_desc(node)
{
	if(js.global.bbs) {
		switch(node.connection) {
			case NODE_CONNECTION_LOCAL:
				return " Locally";	/* obsolete */
				break;
			case NODE_CONNECTION_TELNET:
				return bbs.text(NodeConnectionTelnet);
				break;
			case NODE_CONNECTION_RLOGIN:
				return bbs.text(NodeConnectionRLogin);
				break;
			case NODE_CONNECTION_SSH:
				return bbs.text(NodeConnectionSSH);
				break;
			default:
				return format(bbs.text(NodeConnectionModem), node.connection);
				break;
		}
	} else {
		var conn = NodeConnection[node.connection];
		if(conn)
			return ' via ' + conn;
		else
			return format(' at %ubps', node.connection);
	}
}

// Returns a string representation of the node "misc" flags
function node_misc(node, is_sysop)
{
	var output = '';
	var node_misc = node.misc;
	
	if(!is_sysop && node.status != NODE_INUSE)
		node_misc &= ~(NODE_AOFF|NODE_POFF|NODE_MSGW|NODE_NMSG);
	var flags = '';
	if(node_misc&NODE_AOFF)
		flags += 'A';
	if(node_misc&NODE_LOCK)
		flags += 'L';
	if(node_misc&NODE_MSGW)
		flags += 'M';
	if(node_misc&NODE_NMSG)
		flags += 'N';
	if(node_misc&NODE_POFF)
		flags += 'P';
	if(flags)
		output += format(" (%s)", flags);

	if(is_sysop) {
		flags = '';
		if(node_misc&NODE_ANON)
			flags += 'A';
		if(node_misc&NODE_INTR)
			flags += 'I';
		if(node_misc&NODE_RRUN)
			flags += 'R';
		if(node_misc&NODE_UDAT)
			flags += 'U';
		if(node_misc&NODE_EVENT)
			flags += 'E';
		if(node_misc&NODE_DOWN)
			flags += 'D';
		if(node_misc&NODE_LCHAT)
			flags += 'C';
		if(node.status == NODE_QUIET)
			flags += 'Q';
		if(flags)
			output += format(" [%s]", flags);
	}
	return output;
}	

// Returns a string describing the node status, suitable for printing on a single line
//
// options values supported/used:
// .include_age
// .include_gender
// .username_prefix
// .status_prefix
// .age_prefix
// .gender_prefix
// .gender_separator
// .connection_prefix
// .errors_prefix
function node_status(node, is_sysop, options)
{
	var node_status = node.status;
	var output = '';

	switch(node_status) {
		case NODE_QUIET:
			if(!is_sysop)
				return format(NodeStatus[NODE_WFC],node.aux);
			/* Fall-through */
		case NODE_INUSE:
		{
			var u = new User();
		
			u.number = node.useron;
			if(options.username_prefix)
				output += options.username_prefix;
			if(js.global.bbs && (node.misc&NODE_ANON) && !is_sysop)
				output += bbs.text(UNKNOWN_USER);
			else
				output += u.alias;
			if(options.status_prefix)
				output += options.status_prefix;
			if(options.include_age || options.include_gender) {
				output += " (";
				if(options.include_age) {
					if(options.age_prefix)
						output += options.age_prefix;
					output += u.age;
				}
				if(options.include_gender) {
					if(options.gender_prefix)
						output += options.gender_prefix;
					var sep = options.gender_separator;
					if(sep == undefined)
						sep = ' ';
					output += (options.include_age ?  sep : '') + u.gender;
				}
				if(options.status_prefix)
					output += options.status_prefix;
				output += ")";
			}
			output += " ";
			switch(node.action) {
				case NODE_PCHT:
					if(node.aux == 0)
						output += NodeAction[NODE_LCHT];
					else
						output += format(NodeAction[node.action], node.aux);
					break;
				case NODE_XTRN:
					if(node.aux)
						output += "running " + xtrn_name(u.curxtrn);
					else
						output += format(NodeAction[node.action], node.aux);
					break;
				default:
					output += format(NodeAction[node.action], node.aux);
					break;
			}
			if(options.connection_prefix)
				output += options.connection_prefix;
			output += node_connection_desc(node);
			break;
		}
		case NODE_LOGON:
		case NODE_NEWUSER:
			output += format(NodeStatus[node_status], node.aux);
			output += node_connection_desc(node);
			break;
		default:
			output += format(NodeStatus[node_status], node.aux);
			break;
	}
	
	output += node_misc(node, is_sysop);
		
	if(node.errors && is_sysop) {
		if(options.errors_prefix)
			output += options.errors_prefix;
		output += format(" %d error%s", node.errors, node.errors > 1 ? "s" : "");
	}
	return output;
}

// Returns either:
// 1. number of nodes printed other than 'self' or (if print is false):
// 2. an array of lines suitable for printing
//
// In addition to the options properties supported by node_status(), also supports:
// options.format - a printf-style format for the node status line (e.g. "%d %s")
function nodelist(print, active, listself, is_sysop, options)
{
	var others = 0;
	var output = [];
	
	if(!options.format)
		options.format = "%3d  %s";
	
	for(var n = 0; n < system.node_list.length; n++) {
		var node = system.node_list[n];
		if(active && node.status != NODE_INUSE)
			continue;
		if(js.global.bbs && n == (bbs.node_num - 1)) {
			if(!listself)
				continue;
		} else
			others++;
		
		var line = format(options.format, n + 1, this.node_status(node, is_sysop, options));
		if(print)
			writeln(line);
		else
			output.push(line);
	}
	if(print)
		return others;
	return output;
}

this;