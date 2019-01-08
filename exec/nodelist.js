// $Id$

// Node Listing / Who's Online display script

require("nodedefs.js", 'NODE_INUSE');
require("text.js", 'UNKNOWN_USER');
require("attrdefs.js", 'ATTR_NODENUM');

"use strict";

if(argv.indexOf("install") >= 0) {
	var cnflib = load({}, "cnflib.js");
	var xtrn_cnf = cnflib.read("xtrn.cnf");
	if(!xtrn_cnf) {
		alert("Failed to read xtrn.cnf");
		exit(-1);
	}
	xtrn_cnf.hotkey.push({ key: /* Ctrl-U */21, cmd: '?nodelist.js -active' });
	
	if(!cnflib.write("xtrn.cnf", undefined, xtrn_cnf)) {
		alert("Failed to write xtrn.cnf");
		exit(-1);
	}
	exit(0);
}

function xtrn_name(code)
{
	if(this.xtrn_area==undefined)
		return(code);

	if(xtrn_area.prog!=undefined)
		if(xtrn_area.prog[code]!=undefined && xtrn_area.prog[code].name!=undefined)
			return(xtrn_area.prog[code].name);
	else {	/* old way */
		for(var s in xtrn_area.sec_list)
			for(var p in xtrn_area.sec_list[s].prog_list)
				if(xtrn_area.sec_list[s].prog_list[p].code.toLowerCase()==code.toLowerCase())
					return(xtrn_area.sec_list[s].prog_list[p].name);
	}
	return(code);
}

function node_connection_desc(node)
{
	if(js.global.bbs) {
		switch(node.connection) {
			case NODE_CONNECTION_LOCAL:
				return "Locally";
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

// Derived from src/sbbs3/getnode.cpp: nodelist(), whos_online(), printnodedat()
function nodelist(active, listself)
{
	var options = load({}, "modopts.js", "nodelist");
	if(!options)
		options = {};
	var others = 0;
	
	writeln();
	if(js.global.bbs)
		console.print(bbs.text(NodeLstHdr));
	
	var is_sysop = js.global.bbs ? user.is_sysop : true;
	var u = new User(1);
	
	for(n = 0; n < system.node_list.length; n++) {
		var node = system.node_list[n];
		var node_status = node.status;
		if(active && node_status != NODE_INUSE)
			continue;
		if(js.global.bbs && n == (bbs.node_num - 1)) {
			if(!listself)
				continue;
		} else
			others++;
		if(js.global.console)
			console.attributes = console.color_list[ATTR_NODENUM];
		printf("%3d  ", n + 1);
		if(js.global.console)
			console.attributes = console.color_list[ATTR_NODESTATUS];
	
		switch(node_status) {
			case NODE_QUIET:
				if(!is_sysop) {
					write(format(NodeStatus[NODE_WFC],node.aux));
					break;
				}
				/* Fall-through */
			case NODE_INUSE:
				u.number = node.useron;
				if(js.global.console)
					console.attributes = console.color_list[ATTR_NODEUSER];
				if(js.global.bbs && (node.misc&NODE_ANON) && !is_sysop)
					write(bbs.text(UNKNOWN_USER));
				else
					write(u.alias);
				if(js.global.console)
					console.attributes = console.color_list[ATTR_NODESTATUS];
				if(options.include_age || options.include_gender) {
					write(" (");
					if(options.include_age)
						write(u.age);
					if(options.include_gender)
						write((options.include_age ? ' ' : '') + u.gender);
					write(")");
				}
				write(" ");
				switch(node.action) {
					case NODE_PCHT:
						if(node.aux == 0)
							write(NodeAction[NODE_LCHT]);
						else
							printf(NodeAction[node.action], node.aux);
						break;
					case NODE_XTRN:
						if(node.aux)
							printf("running %s", xtrn_name(u.curxtrn));
						else
							printf(NodeAction[node.action], node.aux);
						break;
					default:
						printf(NodeAction[node.action], node.aux);
						break;
				}
				write(node_connection_desc(node));
				break;
			case NODE_LOGON:
			case NODE_NEWUSER:
				write(format(NodeStatus[node_status], node.aux));
				write(node_connection_desc(node));
				break;
			default:
				write(format(NodeStatus[node_status], node.aux));
				break;
		}

		var node_misc = node.misc;
		if(!is_sysop && node_status != NODE_INUSE)
			node_misc &= ~(NODE_AOFF|NODE_POFF|NODE_MSGW|NODE_NMSG);
		var flags = '';
		if(node_misc&NODE_AOFF)
			flags += 'A';
		if(node_misc&NODE_LOCK)
			flags += 'L';
		if(node_misc&NODE_MSGW)
			flags += 'M';
		if(node_misc&NODE_NMSG)
			flasg += 'N';
		if(node_misc&NODE_POFF)
			flags += 'P';
		if(flags)
			printf(" (%s)", flags);

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
			if(node_status == NODE_QUIET)
				flags += 'Q';
			if(flags)
				printf(" [%s]", flags);
		}
			
		if(node.errors && is_sysop) {
			if(js.global.console)
				console.attributes = console.color_list[ATTR_ERROR];
			printf(" %d error%s", node.errors, node.errors > 1 ? "s" : "");
		}
		writeln();
	}
	if(js.global.bbs && active && !others)
		write(bbs.text(NoOtherActiveNodes));
}

if(argc || !js.global.bbs)
	nodelist(argv.indexOf('-active') >= 0, argv.indexOf('-notself') < 0);

this;