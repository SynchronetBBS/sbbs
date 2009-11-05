/*
 * IMAP Server... whee!
 * Refer to RFC 3501
 *
 * Copyright 2009, Stephen Hurd.
 * Don't steal my code bitches.
 *
 * $Id$
 */

load("sbbsdefs.js");
const RFC822HEADER = 0xb0;  // from smbdefs.h

var sepchar="|";
var debug=true;

function debug_log(line)
{
	if(debug)
		log(line);
}

// Output handling functions
function tagged(tag, msg, desc)
{
	client.socket.send(tag+" "+msg+" "+desc+"\r\n");
	debug_log("IMAP Send: "+tag+" "+msg+" "+desc);
}

function untagged(msg)
{
	client.socket.send("* "+msg+"\r\n");
	debug_log("IMAP Send: * "+msg);
}

function handle_command(command, args, defs)
{
	if(defs[command] != undefined) {
		if(defs[command].arguments != undefined) {
			if(args.length-1 == defs[command].arguments) {
				defs[command].handler(args);
				return(true);
			}
		}
		else if(defs[command].arguments_valid(args.length-1)) {
			defs[command].handler(args);
			return(true);
		}
	}
	return false;
}

function compNumbers(a,b) {
	return(a-b);
}

function send_fetch_response(msgnum, format, uid)
{
	var idx=base.get_msg_index(msgnum);
	var resp='';
	var sent_uid=false;
	var i,j;
	var hdr;
	var rfc822={};
	var objtype;
	var m;
	var re;
	var tmp;
	var sent_flags=false;
	var seen_changed=false;

	function get_header() {
		if(hdr == undefined) {
			hdr=base.get_msg_header(msgnum);
		}
	}

	function get_rfc822_header() {
		var content_type;
		var i;

		get_header();
		if(rfc822.header==undefined) {
			rfc822.header='';
			rfc822.header += "To: "+hdr.to+"\r\n";
			rfc822.header += "Subject: "+hdr.subject+"\r\n";
			rfc822.header += "Message-ID: "+hdr.id+"\r\n";
			rfc822.header += "Date: "+hdr.date+"\r\n";

			// From
			if(!hdr.from_net_type || !hdr.from_net_addr)    /* local message */
				rfc822.header += "From: " + hdr.from + " <" + hdr.from.replace(/ /g,".").toLowerCase() + "@" + system.inetaddr + ">\r\n";
			else if(!hdr.from_net_addr.length)
				rfc822.header += "From: "+hdr.from+"\r\n";
			else if(hdr.from_net_addr.indexOf('@')!=-1)
				rfc822.header += "From: "+hdr.from+" <"+hdr.from_net_addr+">\r\n";
			else
				rfc822.header += "From: "+hdr.from+" <"+hdr.from.replace(/ /g,".").toLowerCase()+"@"+hdr.from_net_addr+">\r\n";

			rfc822.header += "X-Comment-To: "+hdr.to+"\r\n";
			if(hdr.path != undefined)
				rfc822.header += "Path: "+system.inetaddr+"!"+hdr.path+"\r\n";
			if(hdr.from_org != undefined)
				rfc822.header += "Organization: "+hdr.from_org+"\r\n";
			if(hdr.newsgroups != undefined)
				rfc822.header += "Newsgroups: "+hdr.newsgroups+"\r\n";
			if(hdr.replyto != undefined)
				rfc822.header += "Reply-To: "+hdr.replyto+"\r\n";
			if(hdr.reply_id != undefined)
				rfc822.header += "In-Reply-To: "+hdr.reply_id+"\r\n";
			if(hdr.references != undefined)
				rfc822.header += "References: "+hdr.references+"\r\n";
			else if(hdr.reply_id != undefined)
				rfc822.header += "References: "+hdr.reply_id+"\r\n";
			if(hdr.reverse_path != undefined)
				rfc822.header += "Return-Path: "+hdr.reverse_path+"\r\n";

			// Fidonet headers
			if(hdr.ftn_area != undefined)
				rfc822.header += "X-FTN-AREA: "+hdr.ftn_area+"\r\n";
			if(hdr.ftn_pid != undefined)
				rfc822.header += "X-FTN-PID: "+hdr.ftn_pid+"\r\n";
			if(hdr.ftn_TID != undefined)
				rfc822.header += "X-FTN-TID: "+hdr.ftn_tid+"\r\n";
			if(hdr.ftn_flags != undefined)
				rfc822.header += "X-FTN-FLAGS: "+hdr.ftn_flags+"\r\n";
			if(hdr.ftn_msgid != undefined)
				rfc822.header += "X-FTN-MSGID: "+hdr.ftn_msgid+"\r\n";
			if(hdr.ftn_reply != undefined)
				rfc822.header += "X-FTN-REPLY: "+hdr.ftn_reply+"\r\n";

			// Other RFC822 headers
			if(hdr.field_list!=undefined) {
				for(i in hdr.field_list) 
					if(hdr.field_list[i].type==RFC822HEADER) {
						if(hdr.field_list[i].data.toLowerCase().indexOf("content-type:")==0)
							content_type = hdr.field_list[i].data;
						rfc822.header += hdr.field_list[i].data+"\r\n";
					}
			}
			if(content_type==undefined) {
				/* No content-type specified, so assume IBM code-page 437 (full ex-ASCII) */
				rfc822.header += "Content-Type: text/plain; charset=IBM437\r\n";
				rfc822.header += "Content-Transfer-Encoding: 8bit\r\n";
			}
		}
		rfc822.header += "\r\n";
	}

	function get_rfc822_text() {
		if(rfc822.text==undefined)
			rfc822.text=base.get_msg_body(msgnum, true, true, true);
	}

	function get_rfc822() {
		get_rfc822_header();
		get_rfc822_text();
	}

	function get_rfc822_size() {
		get_rfc822();
		rfc822.size=rfc822.header.length+rfc822.text.length;
	}

	function set_seen_flag() {
		if(readonly)
			return;
		if(base.subnum==65535) {
			get_header();
			if(!(hdr.attr & MSG_READ)) {
				hdr.attr |= MSG_READ;
				base.put_msg_header(msgnum);
				hdr=base.get_msg_header(msgnum);
				if(hdr.attr & MSG_READ)
					seen_changed=true;
			}
		}
	}

	if(base.subnum==65535)
		resp=inbox_map.offset[msgnum];
	else
		resp=idx.offset 
	resp += " FETCH (";

	// Moves flags to the end...
	function sort_format(a,b)
	{
		if(typeof(a)=='object')
			a=0;
		else {
			if(a.substr(0,5).toUpperCase()=='FLAGS')
				a=100;
			else
				a=0;
		}
		if(typeof(b)=='object')
			b=0;
		else {
			if(b.substr(0,5).toUpperCase()=='FLAGS')
				b=100;
			else
				b=0;
		}

		return a-b;
	}

	format=format.sort(sort_format);

	for(i in format) {
		if(typeof(format[i])=='object') {
			// We already handled this I hope...
			if(objtype == undefined)
				continue;
			switch(objtype) {
				case 'BODY[HEADER.FIELDS':
					tmp='';
					get_rfc822_header();
					resp += objtype+" (";
					for(j in format[i]) {
						resp+=format[i][j]+" ";
						re=new RegExp("^("+format[i][j]+":.*)$", "im");
						m=re.exec(rfc822.header);
						if(m!=null) {
							tmp += m[1]+"\r\n";
						}
					}
					tmp += "\r\n";
					resp=resp.substr(0,resp.length-1)+")] {"+tmp.length+"}\r\n"+tmp+" ";
					break;
			}
			continue;
		}
		switch(format[i].toUpperCase()) {
			case 'FLAGS':
				get_header();
				resp += "FLAGS ("+calc_msgflags(hdr.attr, hdr.netattr, base.subnum==65535, base.cfg==undefined?null:base.cfg.code, msgnum, readonly)+") ";
				sent_flags=true;
				break;
			case 'UID':
				resp += "UID "+idx.number+" ";
				sent_uid=true;
				break;
			case 'INTERNALDATE':
				get_header();
				resp += 'INTERNALDATE '+strftime('"%d-%b-%C%y %H:%M:%S +0000" ', hdr.when_imported_time);
				break;
			case 'RFC822.SIZE':
				get_rfc822_size();
				resp += "RFC822.SIZE "+rfc822.size+" ";
				break;
			case 'RFC822.TEXT':
			case 'BODY[TEXT]':
				set_seen_flag();
				// fall-through
			case 'BODY.PEEK[TEXT]':
				get_rfc822_text();
				resp += format[i].replace(/\.PEEK/,"").toUpperCase()+" {"+rfc822.text.length+"}\r\n"+rfc822.text+" ";
				break;
			case 'BODY[HEADER]':
			case 'RFC822.HEADER':
				set_seen_flag();
				// fall-through
			case 'BODY.PEEK[HEADER]':
				get_rfc822_header();
				resp += format[i].replace(/\.PEEK/,"").toUpperCase()+" {"+rfc822.header.length+"}\r\n"+rfc822.header+" ";
				break;
			case 'RFC822':
			case 'BODY[]':
				set_seen_flag();
				// fall-through
			case 'BODY.PEEK[]':
				get_rfc822();
				resp += format[i].replace(/\.PEEK/,"").toUpperCase()+" {"+(rfc822.header.length+rfc822.text.length)+"}\r\n"+rfc822.header+rfc822.text+" ";
				break;
			case 'BODY[HEADER.FIELDS':
				set_seen_flag();
			case 'BODY.PEEK[HEADER.FIELDS':
				objtype=format[i].replace(/\.PEEK/,"").toUpperCase();
				break;
		}
	}
	if(seen_changed && !sent_flags) {
		get_header();
		resp += "FLAGS ("+calc_msgflags(hdr.attr, hdr.netattr, base.subnum==65535, base.cfg==undefined?null:base.cfg.code, msgnum, readonly)+") ";
	}
	if(uid && !sent_uid)
		resp += "UID "+idx.number+" ";
	resp = resp.substr(0, resp.length-1)+")";
	untagged(resp);
}

/*
 * Parses a data items FETCH parameter for send_fetch_response()
 */
function parse_data_items(obj)
{
	if(typeof(obj)=='string') {
		switch(obj.toUpperCase()) {
			case 'ALL':
				obj=["FLAGS","INTERNALDATE","RFC822.SIZE","ENVELOPE"];
				break;
			case 'FAST':
				obj=["FLAGS","INTERNALDATE","RFC822.SIZE"];
				break;
			case 'FULL':
				obj=["FLAGS","INTERNALDATE","RFC822.SIZE","ENVELOPE","BODY"];
				break;
			default:
				obj=[];
		}
	}
	return(obj);
}

/*
 * Returns an array of Message Numbers which correspond to the specified sets
 */
function parse_seq_set(set, uid) {
	var response=[];
	var chunks=set.split(/,/);
	var chunk;
	var range;
	var i;
	var max;
	var idx;

	if(uid)
		max=base.last_msg;
	else {
		if(base.subnum==65535)
			max=inbox_map.uid.length;
		else
			max=base.total_msgs;
	}
	for(chunk in chunks) {
		range=chunks[chunk].split(/:/);
		if(range.length == 1)
			range.push(range[0]);
		if(range[0]=='*')
			range[0]=max;
		if(range[1]=='*')
			range[1]=max;
		range[0]=parseInt(range[0],10);
		range[1]=parseInt(range[1],10);
		if(range[0] > range[1]) {
			i=range[0];
			range[0]=range[1];
			range[1]=i;
		}
		for(i=range[0]; i<=range[1]; i++) {
			if(base.subnum==65535) {
				idx=base.get_msg_index(uid?parseInt(i,10):inbox_map.uid[parseInt(i,10)]);
				if(idx==null || idx.to != user.number)
					continue;
			}
			else
				idx=base.get_msg_index(uid?false:true, parseInt(i,10));
			if(idx!=null) {
				response.push(idx.number);
			}
		}
	}
	response=response.sort(compNumbers);
	for(i=0; i<response.length; i++) {
		if(response[i]==response[i+1]) {
			response.splice(i+1,1);
			i--;
		}
	}
	return(response);
}

function parse_command(line)
{
	function execute_line(args) {
		if(args.length >= 2) {
			command=args[1].toUpperCase();
			args.splice(1,1);
			if(handle_command(command, args, any_state_command_handlers))
				return;
			switch(state) {
				case UnAuthenticated:
					if(handle_command(command, args, unauthenticated_command_handlers))
						return;
					break;
				case Authenticated:
					if(handle_command(command, args, authenticated_command_handlers))
						return;
					break;
				case Selected:
					if(handle_command(command, args, selected_command_handlers))
						return;
					if(handle_command(command, args, authenticated_command_handlers))
						return;
					break;
			}
		}
		tagged(args[0], "BAD", "Bad dog, no cookie.");
	}

	function parse_line() {
		var at_start=true;
		var	in_quote=false;
		var paren_depth=0;
		var string_len;
		var args=[];
		var pos;

		function parse_atom() {
			var ret='';

			while(line.length) {
				switch(line.charAt(0)) {
					case ')':
						return(ret);
					case ' ':
						line=line.substr(1);
						return(ret);
					default:
						ret += line.charAt(0);
						line=line.substr(1);
						break;
				}
			}
			return(ret);
		}

		function parse_string()
		{
			var ret='';

			line=line.replace(/^{([0-9]+)}$/, "$1");
			client.socket.send("+ Give me more of that good stuff\r\n");
			ret=client.socket.recv(parseInt(line));
			line=client.socket.recvline(1024, 300);
	
			return(ret);
		}

		function parse_quotedstring() {
			var ret='';
	
			line=line.substr(1);	// Remove leading "
			while(line.length) {
				switch(line.charAt(0)) {
					case '"':
						line=line.substr(1);
						return(ret);
					default:
						ret += line.charAt(0);
						line=line.substr(1);
						break;
				}
			}
			return(ret);
		}

		while(line.length) {
			switch(line.charAt(0)) {
				case '"':
					args.push(parse_quotedstring());
					break;
				case ')':
					line=line.substr(1);
					return(args);
				case '(':
					line=line.substr(1);
					args.push(parse_line());
					break;
				case '{':
					args.push(parse_string());
					break;
				case ' ':
					line=line.substr(1);
					break;
				default:
					args.push(parse_atom());
					break;
			}
		}
		return(args);
	}

	send_updates();
	return(execute_line(parse_line()));
}

// Global variables
const UnAuthenticated=0;
const Authenticated=1;
const Selected=2;
var state=UnAuthenticated;
var base;
var inbox_map;

// Command handling functions
any_state_command_handlers = {
	CAPABILITY:{
		arguments:0,
		handler:function (args) {
			var tag=args[0];

			untagged("CAPABILITY IMAP4rev1 AUTH=PLAIN LOGINDISABLED");
			tagged(tag, "OK", "Capability completed, no TLS support... deal with it.");
		},
	},
	NOOP:{
		arguments:0,
		handler:function(args) {
			var tag=args[0];

			tagged(tag, "OK", "No operation performed... anything else you'd like me to not do?");
		},
	},
	LOGOUT:{
		arguments:0,
		handler:function(args) {
			var tag=args[0];
		
			untagged("BYE I'll go now");
			tagged(tag, "OK", "Thanks for stopping by.");
			client.socket.close();
			exit();
		}
	},
};

unauthenticated_command_handlers = {
	STARTTLS:{
		arguments:0,
		handler:function(args) {
			var tag=args[0];

			tagged(tag, "BAD", "I told you to deal with the lack of TLS damnit!");
		},
	},
	AUTHENTICATE:{
		arguments:1,
		handler:function(args) {
			var tag=args[0];
			var mechanism=args[1];
			var line;
			var args;

			if(mechanism.toUpperCase()=="PLAIN") {
				client.socket.send("+\r\n");
				line=client.socket.recvline(1024, 300);
				args=base64_decode(line).split(/\x00/);
				if(!login(args[1],args[2])) {
					tagged(tag, "NO", "No AUTH for you.");
					return;
				}
				tagged(tag, "OK", "Howdy.");
				state=Authenticated;
			}
			else
				tagged(tag, "NO", "No "+mechanism+" authenticate supported yet... give me a reason to do it and I'll think about it.");
		},
	},
	LOGIN:{
		arguments:2,
		handler:function(args) {
			var tag=args[0];
			var usr=args[1];
			var pass=args[2];

			if(!login(usr, pass)) {
				tagged(tag, "NO", "No login for you.");
				return;
			}
			tagged(tag, "OK", "Sure, come on in.");
			state=Authenticated;
		},
	}
};

function sendflags(perm)
{
	var flags="";
	var pflags="";

	if(base.subnum==65535) {
		flags=calc_msgflags(0xffff, 0xffff, true, null, null, true);
		pflags=calc_msgflags(0xffff, 0xffff, true, null, null, true);
	}
	else {
		flags=calc_msgflags(0xffff, 0xffff, false, null, null, true);
		pflags=calc_msgflags(0xffff, 0xffff, false, null, null, true);
	}
	if(perm)
		untagged("OK [PERMANENTFLAGS ("+pflags+")] Overkill!");
	else
		untagged("FLAGS ("+flags+") Overkill!");
}

function parse_flags(inflags)
{
	var i;
	var flags={attr:0, netattr:0};

	for(i in inflags) {
		switch(inflags[i].toUpperCase()) {
			case '\\SEEN':
			case 'READ':
				flags.attr |= MSG_READ;
				break;
			case '\\ANSWERED':
			case 'REPLIED':
				flags.attr |= MSG_REPLIED;
				break;
			case '\\FLAGGED':
			case 'VALIDATED':
				flags.attr |= MSG_VALIDATED;
				break;
			case '\\DELETED':
			case 'DELETE':
				flags.attr |= MSG_DELETE;
				break;
			case 'ANONYMOUS':
				flags.attr |= MSG_ANONYMOUS;
				break;
			case 'KILLREAD':
				flags.attr |= MSG_KILLREAD;
				break;
			case 'MODERATED':
				flags.attr |= MSG_MODERATED;
				break;
			case 'NOREPLY':
				flags.attr |= MSG_NOREPLY;
				break;

			case 'LOCAL':
				flags.netattr |= MSG_LOCAL;
				break;
			case 'INTRANSIT':
				flags.netattr |= MSG_INTRANSIT;
				break;
			case 'SENT':
				flags.netattr |= MSG_SENT;
				break;
			case 'KILLSENT':
				flags.netattr |= MSG_KILLSENT;
				break;
			case 'ARCHIVESENT':
				flags.netattr |= MSG_ARCHIVESENT;
				break;
			case 'HOLD':
				flags.netattr |= MSG_HOLD;
				break;
			case 'CRASH':
				flags.netattr |= MSG_CRASH;
				break;
			case 'IMMEDIATE':
				flags.netattr |= MSG_IMMEDIATE;
				break;
			case 'DIRECT':
				flags.netattr |= MSG_DIRECT;
				break;
			case 'GATE':
				flags.netattr |= MSG_GATE;
				break;
			case 'ORPHAN':
				flags.netattr |= MSG_ORPHAN;
				break;
			case 'FPU':
				flags.netattr |= MSG_FPU;
				break;
			case 'TYPELOCAL':
				flags.netattr |= MSG_TYPELOCAL;
				break;
			case 'TYPEECHO':
				flags.netattr |= MSG_TYPECHO;
				break;
			case 'TYPENET':
				flags.netattr |= MSG_TYPENET;
				break;
		}
	}
	return(flags);
}

function check_msgflags(attr, netattr, ismail, touser, fromuser, isoperator)
{
	var flags={attr:attr, netattr:netattr};
	const op_perms=(MSG_DELETE|MSG_VALIDATED);
	const to_perms=(MSG_READ);
	const from_perms=0;
	var perms=0;

	if(user.compare_ars("SYSOP"))
		return(flags);	// SYSOP has godly powers.

	// Only the sysop can diddle the network flags
	flags.netattr=0;

	if(isoperator)
		perms |= op_perms;
	if(touser)
		perms |= to_perms;
	if(fromuser)
		perms |= from_perms;

	flags.attr &= perms;
	return(flags);
}

function calc_msgflags(attr, netattr, ismail, code, msg, readonly)
{
	var flags='';

	if(attr & MSG_PRIVATE)
		flags += "PRIVATE ";
	if(attr & MSG_READ) {
		if(base.subnum==65535)
			flags += "\\Seen ";
		else
			flags += "READ ";
	}
	if(attr & MSG_PERMANENT)
		flags += "PERMANENT ";
	if(attr & MSG_LOCKED)
		flags += "LOCKED ";
	if(attr & MSG_DELETE)
		flags += "\\Deleted ";
	if(attr & MSG_ANONYMOUS)
		flags += "ANONYMOUS ";
	if(attr & MSG_KILLREAD)
		flags += "KILLREAD ";
	if(attr & MSG_MODERATED)
		flags += "MODERATED ";
	if(attr & MSG_VALIDATED)
		flags += "\\Flagged ";
	if(attr & MSG_REPLIED) {
		if(base.subnum==65535)
			flags += "\\Answered ";
		else
			flags += "REPLIED ";
	}
	if(attr & MSG_NOREPLY)
		flags += "NOREPLY ";

	if(netattr & MSG_INTRANSIT)
		flags += "INTRANSIT ";
	if(netattr & MSG_SENT)
		flags += "SENT ";
	if(netattr & MSG_KILLSENT)
		flags += "KILLSENT ";
	if(netattr & MSG_ARCHIVESENT)
		flags += "ARCHIVESENT ";
	if(netattr & MSG_HOLD)
		flags += "HOLD ";
	if(netattr & MSG_CRASH)
		flags += "CRASH ";
	if(netattr & MSG_IMMEDIATE)
		flags += "IMMEDIATE ";
	if(netattr & MSG_DIRECT)
		flags += "DIRECT ";
	if(netattr & MSG_GATE)
		flags += "GATE ";
	if(netattr & MSG_ORPHAN)
		flags += "ORPHAN ";
	if(netattr & MSG_FPU)
		flags += "FPU ";
	if(netattr & MSG_TYPELOCAL)
		flags += "TYPELOCAL ";
	if(netattr & MSG_TYPEECHO)
		flags += "TYPEECHO ";
	if(netattr & MSG_TYPENET)
		flags += "TYPENET ";

	if(msg==null || msg_ptrs[code] < msg)
		flags += '\\Recent ';

	if(!readonly) {
		if(msg > msg_ptrs[code])
			msg_ptrs[code]=msg;
	}

	if(flags.length)
		return(flags.substr(0, flags.length-1));
	return("");
}

function sublist(group, match, subscribed)
{
	var grp;
	var sub;
	var ret=[];
	var groups={};
	var wmatch,wgroup;
	var fmatch;

	if(match=='')
		return([""]);

	wmatch=match.replace(/^\%/, "|*");
	wgroup=group.replace(/\%$/, "*|");

	if(wgroup == '')
		wgroup='*';
	fmatch=(wgroup+wmatch).replace(/\|\|/,"|");
	for(grp in msg_area.grp_list) {
		if(wildmatch(true, msg_area.grp_list[grp].description, fmatch)) {
			ret.push(msg_area.grp_list[grp].description+sepchar);

			for(sub in msg_area.grp_list[grp].sub_list) {
				if(wildmatch(true, msg_area.grp_list[grp].sub_list[sub].description, fmatch, false)) {
					if((!subscribed) || msg_area.grp_list[grp].sub_list[sub].scan_cfg&SCAN_CFG_NEW)
						ret.push(msg_area.grp_list[grp].description+sepchar+msg_area.grp_list[grp].sub_list[sub].description);
				}
			}
		}
	}
	if(wildmatch(true, "INBOX", fmatch, false))
		ret.push("INBOX");
	return(ret);
}

function getsub(longname) {
	var components;
	var grp;
	var sub;

	if(longname=='INBOX')
		return("mail");
	components=longname.split(sepchar);
	for(grp in msg_area.grp_list) {
		if(msg_area.grp_list[grp].description==components[0]) {
			for(sub in msg_area.grp_list[grp].sub_list) {
				if(components[1]==msg_area.grp_list[grp].sub_list[sub].description) {
					return(msg_area.grp_list[grp].sub_list[sub].code);
				}
			}
		}
	}
	return("NONE!!!");
}

function generate_inbox_map()
{
	var i;
	var idx;
	var count=0;
	var unseen=0;
	var uid=[];
	var offset={};

	for(i=base.first_msg; i<=base.last_msg; i++) {
		idx=base.get_msg_index(i);
		if(idx != null) {
			if(idx.to != user.number)
				continue;
			uid.push(idx.number);
			offset[idx.number]=uid.length;
		}
	}
	return({uid:uid,unseen:unseen,offset:offset});
}

function count_recent(base)
{
	var i;
	var count=0;
	var idx;

	if(base.cfg==undefined)
		return 0;
	for(i=base.first_msg; i<= base.last_msg; i++) {
		idx=base.get_msg_index(i);
		if(idx==null)
			continue;
		if(base.subnum==65535 && idx.to != user.number)
			continue;
		if(i > msg_area.sub[base.cfg.code].scan_ptr)
			count++;
	}
	return(count);
}

function update_status()
{
	if(base==undefined || !base.is_open) {
		curr_status={exists:-1,recent:-1,unseen:-1,uidnext:-1,uidvalidity:-1};
		return;
	}
	if(base.subnum==65535) {
		inbox_map=generate_inbox_map(true);
		curr_status.exists=inbox_map.uid.length;
		curr_status.recent=count_recent(base);
		curr_status.unseen=inbox_map.unseen;
		if(inbox_map.uid.length > 0)
			curr_status.uidnext=inbox_map.uid[inbox_map.uid.length-1]+1;
		else
			curr_status.uidnext=1;
		curr_status.uidvalidity=0;
	}
	else {
		curr_status.exists=base.total_msgs;
		curr_status.recent=count_recent(base);
		curr_status.unseen=base.total_msgs;
		curr_status.uidnext=base.last_msg+1;
		curr_status.uidvalidity=0;
	}
}

function send_updates()
{
	var old_status;

	if(state==Selected) {
		old_status=eval(curr_status.toSource());
		update_status();
		if(old_status.exists != curr_status.exists)
			untagged(curr_status.exists+" EXISTS");
		if(old_status.recent != curr_status.recent)
			untagged(curr_status.recent+" RECENT");
		if(old_status.unseen != curr_status.unseen)
			untagged("OK [UNSEEN "+(curr_status.unseen)+"]");
		if(old_status.uidnext != curr_status.uidnext)
			untagged("OK [UIDNEXT "+(curr_status.uidnext)+"]");
		if(old_status.uidvalidity != curr_status.uidvalidity)
			untagged("OK [UIDVALIDITY "+curr_status.uidvalidity+"]");
	}
}

function open_sub(sub)
{
	if(base != undefined && base.is_open && base.cfg != undefined) {
		if(msg_ptrs[base.cfg.code]!=orig_ptrs[base.cfg.code])
			msg_base.sub[base.cfg.code]=msg_ptrs[base.cfg.code];
		base.close();
		update_status();
	}
	base=new MsgBase(sub);
	if(base == undefined || (!base.open()))
		return false;
	if(base.cfg != undefined) {
		if(orig_ptrs[base.cfg.code]==undefined) {
			orig_ptrs[base.cfg.code]=msg_area.sub[base.cfg.code];
			msg_ptrs[base.cfg.code]=msg_area.sub[base.cfg.code];
		}
	}
	update_status();
	sendflags(false);
	untagged(curr_status.exists+" EXISTS");
	untagged(curr_status.recent+" RECENT");
	untagged("OK [UNSEEN "+(curr_status.unseen)+"]");
	sendflags(true);
	untagged("OK [UIDNEXT "+(curr_status.uidnext)+"]");
	untagged("OK [UIDVALIDITY "+curr_status.uidvalidity+"]");
	return true;
}

authenticated_command_handlers = {
	SELECT:{
		arguments:1,
		handler:function(args){
			var tag=args[0];
			var sub=getsub(args[1]);

			if(!open_sub(sub)) {
				tagged(tag, "NO", "Can't find "+args[1]+" ("+sub+")");
				return;
			}
			tagged(tag, "OK", "[READ-WRITE] Mailbox "+sub+" has been selected");
			readonly=false;
			state=Selected;
		},
	},
	EXAMINE:{	// Same as select onlt without the writability
		arguments:1,
		handler:function(args){
			var tag=args[0];
			var sub=getsub(args[1]);

			if(!open_sub(sub)) {
				tagged(tag, "NO", "Can't find "+args[1]+" ("+sub+")");
				return;
			}
			tagged(tag, "OK", "[READ-ONLY] Mailbox "+sub+" has been examined");
			readonly=true;
			state=Selected;
		},
	},
	CREATE:{
		arguments:1,
		handler:function(args) {
			var tag=args[0];
			var sub=args[1];

			tagged(tag, "NO", "No creating groups buddy.");
		},
	},
	DELETE:{
		arguments:1,
		handler:function(args) {
			var tag=args[0];
			var sub=args[1];

			tagged(tag,"NO", "No can do.");
		},
	},
	RENAME:{
		arguments:2,
		handler:function(args) {
			var tag=args[0];
			var oldsub=args[1];
			var newsub=args[2];

			tagged(tag,"NO", "No renaming for you!");
		},
	},
	SUBSCRIBE:{
		arguments:1,
		handler:function(args) {
			var tag=args[0];
			var sub=getsub(args[1]);

			if(msg_area.sub[sub]!=undefined && msg_area.sub[sub].can_read) {
				tagged(tag, "OK", "Subscribed...");
				msg_area.sub[sub].scan_cfg|=SCAN_CFG_NEW;
			}
			else
				tagged(tag, "NO", "Can't subscribe to that sub (what is it?)");
		},
	},
	UNSUBSCRIBE:{
		arguments:1,
		handler:function(args) {
			var tag=args[0];
			var sub=getsub(args[1]);

			if(msg_area.sub[sub]!=undefined && msg_area.sub[sub].can_read) {
				tagged(tag, "OK", "Unsubscribed...");
				// This may leave the to you only bit set... yay.
				msg_area.sub[sub].scan_cfg&=~SCAN_CFG_NEW;
			}
			else
				tagged(tag, "NO", "Can't unsubscribe that sub (what is it?)");
		},
	},
	LIST:{
		arguments:2,
		handler:function(args) {
			var tag=args[0];
			var group=args[1];
			var sub=args[2];
			var groups=sublist(group, sub, false);
			var group;
			var base;
			var interesting=false;

			for(group in groups) {
				if(groups[group].substr(-1)==sepchar)
					untagged('LIST (\\Noselect) "'+sepchar+'" "'+groups[group].substr(0,groups[group].length-1)+'"');
				else {
					if(groups[group] == '') {
						untagged('LIST (\\Noselect) "'+sepchar+'" "'+groups[group]+'"');
					}
					else {
						base=new MsgBase(getsub(groups[group]));
						if(base.cfg != undefined) {
							if(base.last_msg > msg_area.sub[base.cfg.code].scan_ptr)
								untagged('LIST (\\Noinferiors \\Marked) "'+sepchar+'" "'+groups[group]+'"');
							else
								untagged('LIST (\\Noinferiors \\Unmarked) "'+sepchar+'" "'+groups[group]+'"');
						}
						else
							untagged('LIST (\\Noinferiors) "'+sepchar+'" "'+groups[group]+'"');
					}
				}
			}
			tagged(tag, "OK", "There you go.");
		},
	},
	LSUB:{
		arguments:2,
		handler:function(args) {
			var tag=args[0];
			var group=args[1];
			var sub=args[2];
			var groups=sublist(group, sub, true);
			var group;

			for(group in groups) {
				if(groups[group].substr(-1)==sepchar)
					untagged('LSUB (\\Noselect) "'+sepchar+'" "'+groups[group].substr(0,groups[group].length-1)+'"');
				else
					untagged('LSUB () "'+sepchar+'" "'+groups[group]+'"');
			}
			tagged(tag, "OK", "There you go.");
		},
	},
	STATUS:{
		arguments:2,
		handler:function(args) {
			var tag=args[0];
			var sub=getsub(args[1]);
			var items=args[2];
			var i;
			var response=[];
			var base;
			var mademap=false;
			var inbox_map;

			if(typeof(items)!="object")
				items=[items];
			base=new MsgBase(sub);
			if(base == undefined || (!base.open())) {
				tagged(tag, "NO", "Can't find your mailbox");
				return;
			}
			if(base.cfg != undefined && orig_ptrs[base.cfg.code]==undefined) {
				orig_ptrs[base.cfg.code]=msg_area.sub[base.cfg.code];
				msg_ptrs[base.cfg.code]=msg_area.sub[base.cfg.code];
			}
			for(i in items) {
				switch(items[i].toUpperCase()) {
					case 'MESSAGES':
						response.push("MESSAGES");
						if(base.subnum==65535) {
							if(!mademap)
								inbox_map=generate_inbox_map(false);
							response.push(inbox_map.length);
						}
						else
							response.push(base.total_msgs);
						break;
					case 'RECENT':
						response.push("RECENT");
						response.push(count_recent(base));
						break;
					case 'UIDNEXT':
						response.push("UIDNEXT");
						response.push(base.last_msg+1);
						break;
					case 'UIDVALIDITY':
						response.push("UIDVALIDITY");
						response.push(0);
						break;
					case 'UNSEEN':
						response.push("UNSEEN");
						if(base.subnum==65535) {
							if(!mademap)
								inbox_map=generate_inbox_map();
							response.push(inbox_map.unseen);
						}
						else
							response.push(base.total_msgs+1);
						break;
				}
			}
			base.close();
			update_status();
			untagged("STATUS {"+(args[1].length)+"}\r\n"+args[1]+" ("+response.join(" ")+")");
			tagged(tag, "OK", "And that's the way it is.");
		},
	},
	APPEND:{
		arguments_valid:function(count) {
			if(count >= 2 && count <= 4)
				return(true);
			return(false);
		},
		handler:function(args) {
			var tag=args[0];

			tagged(tag, "NO", "No appending yet... sorry.");
		},
	},
};

function do_store(seq, uid, item, data)
{
	var silent=false;
	var i,j;
	var flags;
	var chflags;
	var hdr;

	for(i in seq) {
		hdr=base.get_msg_header(seq[i]);
		flags=parse_flags(data);
		switch(item.toUpperCase()) {
			case 'FLAGS.SILENT':
				silent=true;
			case 'FLAGS':
				chflags={attr:hdr.attr^flags.attr, netattr:hdr.netattr^flags.netattr};
				break;
			case '+FLAGS.SILENT':
				silent=true;
			case '+FLAGS':
				chflags={attr:hdr.attr^(hdr.attr|flags.attr), netattr:hdr.netattr^(hdr.netattr|flags.netattr)};
				break;
			case '-FLAGS.SILENT':
				silent=true;
			case '-FLAGS':
				chflags={attr:hdr.attr^(hdr.attr&~flags.attr), netattr:hdr.netattr^(hdr.netattr&~flags.netattr)};
				break
		}
		chflags=check_msgflags(chflags.attr, chflags.netattr, base.subnum==65535, hdr.to_net_type==NET_NONE?hdr.to==user.number:false, hdr.from_net_type==NET_NONE?hdr.from==user.number:false,base.is_operator);
		if(chflags.attr || chflags.netattr) {
			hdr.attr ^= chflags.attr;
			hdr.netattr ^= chflags.netattr;
			if(!readonly)
				base.put_msg_header(seq[i], hdr);
			hdr=base.get_msg_header(seq[i]);
		}
		if(!silent)
			send_fetch_response(seq[i], ["FLAGS"], uid);
	}
}

selected_command_handlers = {
	CHECK:{
		arguments:0,
		handler:function(args) {
			var tag=args[0];

			tagged(tag, "OK", "Check.");
		},
	},
	CLOSE:{
		arguments:0,
		handler:function(args) {
			var tag=args[0];

			if(base.cfg != undefined) {
				if(msg_ptrs[base.cfg.code]!=orig_ptrs[base.cfg.code])
					msg_base.sub[base.cfg.code]=msg_ptrs[base.cfg.code];
			}
			base.close();
			update_status();
			tagged(tag, "OK", "Closed.");
			state=Authenticated;
		},
	},
	EXPUNGE:{
		arguments:0,
		handler:function(args) {
			var tag=args[0];

			tagged(tag, "NO", "Can't expunge... wait for maintenance");
		},
	},
	SEARCH:{
		arguments_valid:function(count) {
			return(count >= 1);
		},
		handler:function(args) {
			var tag=args[0];

			tagged(tag, "NO", "Can't search... I'm useless.");
		}
	},
	FETCH:{
		arguments:2,
		handler:function(args) {
			var tag=args[0];
			var seq=parse_seq_set(args[1],false);
			var data_items=parse_data_items(args[2]);
			var i;

			for(i in seq) {
				send_fetch_response(seq[i], data_items, false);
			}
			tagged(tag, "OK", "There they are!");
		},
	},
	STORE:{
		arguments:3,
		handler:function(args) {
			var tag=args[0];
			var seq=parse_seq_set(args[1], false);
			var item=args[2];
			var data=args[3];

			do_store(seq, false, item, data);
			tagged(tag, "OK", "Stored 'em up");
		},
	},
	COPY:{
		arguments:2,
		handler:function(args) {
			var tag=args[0];

			tagged(tag, "NO", "Hah! Not likely!");
		},
	},
	UID:{
		arguments_valid:function(count) {
			if(count==3 || count==4)
				return(true);
			return(false);
		},
		handler:function(args) {
			var tag=args[0];
			var cmd=args[1];
			var seq;
			var data_items;
			var data;
			var i;

			switch(cmd.toUpperCase()) {
				case 'FETCH':
					if(args.length != 4) {
						tagged(args[0], "BAD", "Bad dog, no UID cookie.");
						return;
					}
					seq=parse_seq_set(args[2],true);
					data_items=parse_data_items(args[3]);
					for(i in seq) {
						send_fetch_response(seq[i], data_items, true);
					}
					tagged(tag, "OK", "There they are (with UIDs)!");
					break;
				case 'STORE':
					if(args.length != 5) {
						tagged(args[0], "BAD", "Bad dog, no UID STORE cookie.");
						return;
					}
					seq=parse_seq_set(args[2],true);
					data_items=args[3];
					data=args[4];
					do_store(seq, true, data_items, data);
					tagged(tag, "OK", "Stored 'em up (with UIDs)!");
					break;
				default:
					tagged(tag, "NO", "Help, I'm useless.");
			}
		},
	}
};

var line;
var readonly=true;
var orig_ptrs={};
var msg_ptrs={};
var curr_status={exists:0,recent:0,unseen:0,uidnext:0,uidvalidity:0};
client.socket.send("* OK Give 'er\r\n");
while(1) {
	line=client.socket.recvline(1024, 300);
	if(line != null) {
		debug_log("IMAP RECV: "+line);
		parse_command(line);
	}
	else {
		untagged("BYE No lolligaggers here!");
		client.socket.close();
		exit(0);
	}
}
