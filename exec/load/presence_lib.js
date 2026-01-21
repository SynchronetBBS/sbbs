// Library for reporting user presence (e.g. BBS node listings, who's online)
// Much of the code was derived from src/sbbs3/getnode.cpp: nodelist(), whos_online(), printnodedat()

require("nodedefs.js", 'NODE_INUSE');
require("sbbsdefs.js", 'USER_DELETED');

"use strict";

// Returns a string, hopefully the full name of the External Program referenced by 'code' 
// or referenced by 1-based number (as stored in node.dab 'aux' field)
function xtrn_name(code)
{
	if(typeof code == 'number') {
		for(var i in xtrn_area.prog)
			if(xtrn_area.prog[i].number == code - 1)
				return xtrn_area.prog[i].name;
	}
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
				return bbs.text(bbs.text.NodeConnectionTelnet);
				break;
			case NODE_CONNECTION_RLOGIN:
				return bbs.text(bbs.text.NodeConnectionRLogin);
				break;
			case NODE_CONNECTION_SSH:
				return bbs.text(bbs.text.NodeConnectionSSH);
				break;
			case NODE_CONNECTION_SFTP:
				return bbs.text(bbs.text.NodeConnectionSFTP);
				break;
			case NODE_CONNECTION_RAW:
				return bbs.text(bbs.text.NodeConnectionRaw);
				break;
			default:
				return format(bbs.text(bbs.text.NodeConnectionModem), node.connection);
				break;
		}
	} else {
		var conn = NodeConnection[node.connection];
		if(conn)
			return ' via ' + conn;
		else
			return format(' at %ubps', node.connection);
	}
	return null;
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
		if(node_misc&NODE_FCHAT)
			flags += 'F';
		if(node.status == NODE_QUIET)
			flags += 'Q';
		if(flags)
			output += format(" [%s]", flags);
	}
	return output;
}

function user_age_and_gender(user, options)
{
	var output = '';

	if((options.include_age && user.age) || (options.include_gender && user.gender > ' ')) {
		output += " (";
		if(options.include_age && user.age) {
			if(options.age_prefix)
				output += options.age_prefix;
			output += user.age;
		}
		if(options.include_gender && user.gender > ' ') {
			if(options.gender_prefix)
				output += options.gender_prefix;
			var sep = options.gender_separator;
			if(sep == undefined)
				sep = ' ';
			output += ((options.include_age && user.age )?  sep : '') + user.gender;
		}
		if(options.status_prefix)
			output += options.status_prefix;
		output += ")";
	}
	return output;
}

// Returns a string describing the node status, suitable for printing on a single line
//
// num is zero-based
//
// options values supported/used:
// .include_age
// .include_gender
// .exclude_username
// .username_prefix
// .status_prefix
// .age_prefix
// .gender_prefix
// .gender_separator
// .exclude_connection
// .connection_prefix
// .errors_prefix
function node_status(node, is_sysop, options, num)
{
	var node_status = node.status;
	var misc = node.misc;
	var output = '';

	switch(node_status) {
		case NODE_QUIET:
			if(!is_sysop)
				return node.vstatus || format(NodeStatus[NODE_WFC],node.aux);
			/* Fall-through */
		case NODE_INUSE:
		{
			var user = new User(node.useron);

			if (!options.exclude_username) {
				if(options.username_prefix)
					output += options.username_prefix;
				if(js.global.bbs && (misc&NODE_ANON) && !is_sysop)
					output += bbs.text(bbs.text.UNKNOWN_USER);
				else
					output += user.alias;
			}
			if(options.status_prefix)
				output += options.status_prefix;
			output += user_age_and_gender(user, options);
			output += " ";
			if(node.activity)
				output += node.activity;
			else {
				switch(node.action) {
					case NODE_PCHT:
						if(node.aux == 0)
							output += NodeAction[NODE_LCHT];
						else
							output += format(NodeAction[node.action], node.aux);
						break;
					case NODE_XTRN:
						if(node.aux)
							output += "running " + xtrn_name(node.aux);
						else
							output += NodeAction[node.action];
						break;
					default:
						output += format(NodeAction[node.action], node.aux);
						break;
				}
			}
			if(options.status_prefix)
				output += options.status_prefix;
			if (!options.exclude_connection) {
				if(options.connection_prefix)
					output += options.connection_prefix;
				output += node_connection_desc(node);
			}
			break;
		}
		case NODE_LOGON:
		case NODE_NEWUSER:
			output += node.vstatus || format(NodeStatus[node_status], node.aux);
			output += node_connection_desc(node);
			break;
		case NODE_LOGOUT:
			if (node.vstatus)
				output += node.vstatus
			else {
				output += NodeStatus[node_status];
				if(options.username_prefix)
					output += options.username_prefix;
				if(js.global.bbs && (misc&NODE_ANON) && !is_sysop)
					output += bbs.text(bbs.text.UNKNOWN_USER);
				else
					output += system.username(node.useron);
			}
			break;
		default:
			output += node.vstatus || format(NodeStatus[node_status], node.aux);
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

function web_user_misc(web_user)
{
	var flags = '';
	if(web_user.msg_waiting)
		flags += 'M';
	if(web_user.do_not_disturb)
		flags += 'P';
	if(flags)
		return format(" (%s)", flags);
	return '';
}

function web_user_status(web_user, browsing, options)
{
	if(!browsing)
		browsing = 'browsing';
	var output = '';

	if(options.username_prefix)
		output += options.username_prefix;
	output += web_user.name;
	if(options.status_prefix)
		output += options.status_prefix;
	output += user_age_and_gender(web_user, options);
	if(web_user.action) {
		output += " " + browsing + " ";
		output += web_user.action;
	}
	if(options.connection_prefix)
		output += options.connection_prefix;
	output += " via web";
	output += web_user_misc(web_user);
	return output;
}

function web_users(max_inactivity)
{
	var user = new User;
	var users = [];
	const lastuser = system.lastuser;
    const sessions = directory(system.data_dir + 'user/*.web');
	if(!max_inactivity)
		max_inactivity = 15 * 60;

    sessions.forEach(function (e) {
		const base = file_getname(e).replace(file_getext(e), '');
        const un = parseInt(base, 10);
        if (isNaN(un) || un < 1 || un > lastuser) return;
        if (time() - file_date(e) >= max_inactivity)
			return;
		user.number = un;
		if(user.settings & (USER_DELETED|USER_QUIET))
			return;
        const f = new File(e);
        if (f.open('r')) {
            const session = f.iniGetObject();
			users.push({
				name: user.alias,
				usernum: un,
				action: session.action,
				age: user.age,
				gender: user.gender,
				location: user.location,
				logontime: file_date(e), // TODO: this is probably not the actual logon time, but more like "last activity" time (?)
				do_not_disturb: (user.chat_settings & CHAT_NOPAGE) ? true : undefined,
				msg_waiting: (file_size(format(system.data_dir + "msgs/%04u.msg", un)) > 0) ? true : undefined
			});
            f.close();
        }
	});
	return users;
}

// Returns either:
// 1. number of nodes printed other than 'self' or (if print is false):
// 2. an array of lines suitable for printing
//
// In addition to the options properties supported by node_status(), also supports:
// options.format - a printf-style format for the node status line (e.g. "%d %s")
// options.include_web_users - if false, don't include web users in output/result
// options.web_inactivity - seconds before considering a web user offline
// options.web_browsing - string to use to represent web actions (default: 'browsing')
function nodelist(print, active, listself, is_sysop, options)
{
	var others = 0;
	var output = [];

	if(!options.format)
		options.format = "%3d  %s";

	var n;
	for(n = 0; n < system.node_list.length; n++) {
		var node = system.get_node(n + 1);
		if(active && node.status != NODE_INUSE
			&& (!is_sysop || node.status != NODE_QUIET))
			continue;
		if(js.global.bbs && n == (bbs.node_num - 1)) {
			if(!listself)
				continue;
		} else
			others++;

		var line = format(options.format, n + 1, node_status(node, is_sysop, options, n));
		if(print)
			writeln(line);
		else
			output.push(line);
	}
	if(options.include_web_users !== false) {
		var web_user = web_users(options.web_inactivity);
		for(var w in web_user) {
			var line = format(options.format, ++n, web_user_status(web_user[w], options.web_browsing, options));
			if(print)
				writeln(line);
			else
				output.push(line);
			others++;
		}
	}
	if(print)
		return others;
	return output;
}

this;
