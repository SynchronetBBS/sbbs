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
var debugRX=true;

function debug_log(line, rx)
{
	if(debug)
		log(line);
	else if(rx && debugRX)
		log(line);
}

// Output handling functions
function tagged(tag, msg, desc)
{
	client.socket.send(tag+" "+msg+" "+desc+"\r\n");
	debug_log("IMAP Send: "+tag+" "+msg+" "+desc, false);
}

function untagged(msg)
{
	client.socket.send("* "+msg+"\r\n");
	debug_log("IMAP Send: * "+msg, false);
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

function compNumbers(a,b)
{
	return(a-b);
}

function encode_binary(str)
{
	return '{'+str.length+'}\r\n'+str;
}

function encode_string(str)
{
	if(str=='')
		return('""');

	// An Atom
	//if(str.search(/[\(\)\{ \x00-\x1F\*\%\"\\\]]*/)==-1)
	//	return str;

	if(str.search(/[\r\n\x80-\xff]/)==-1) {
		str=str.replace(/[\\\"]/, "\\$1");
		return '"'+str+'"';
	}

	return encode_binary(str);
}

function get_from(hdr)
{
	// From
	if(!hdr.from_net_type || !hdr.from_net_addr)    /* local message */
		return(hdr.from + " <" + hdr.from.replace(/ /g,".").toLowerCase() + "@" + system.inetaddr + ">");
	else if(!hdr.from_net_addr.length)
		return(hdr.from);
	else if(hdr.from_net_addr.indexOf('@')!=-1)
		return(hdr.from+" <"+hdr.from_net_addr+">");
	return(hdr.from+" <"+hdr.from.replace(/ /g,".").toLowerCase()+"@"+hdr.from_net_addr+">");
}

function send_fetch_response(msgnum, fmat, uid)
{
	var idx;
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
	var envelope;

	idx=base.get_msg_index(msgnum);

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

			rfc822.header += "From: "+get_from(hdr)+"\r\n";

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

			// Unwrap headers
			rfc822.header=rfc822.header.replace(/\s*\r\n\s+/g, " ");
			rfc822.header += "\r\n";
		}
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

	function get_envelope() {
		function parse_header(header, is_addresses) {
			var m;
			var m2;
			var re;

			re=new RegExp("^"+header+":\\s*(.*?)$","im");
			m=re.exec(rfc822.header);
			if(m==null) {
				return("NIL");
			}
			else {
				if(is_addresses) {
					if((m2=m[1].match(/^\s*(.*)\s+<([^@]*)@(.*)>\s*$/))!=null) {
						return '(('+[encode_string(m2[1]), "NIL", encode_string(m2[2]), encode_string(m2[3])].join(" ")+'))';
					}
					else if((m2=m[1].match(/^\s*(.*)\s+<([^@]i)>\s*$/))!=null) {
						return '(('+[encode_string(m2[1]), "NIL", encode_string(m2[2]), NIL].join(" ")+'))';
					}
					else
						return '(('+[encode_string(m[1]), "NIL", "NIL", "NIL"].join(" ")+'))';
				}
				else
					return(encode_string(m[1]));
			}
		}

		var m;

		if(envelope==undefined) {
			get_rfc822_header();

			envelope=[];
			envelope.push(parse_header("Date", false));
			envelope.push(parse_header("Subject", false));
			envelope.push(parse_header("From", true));
			envelope.push(parse_header("Sender", true));
			envelope.push(parse_header("Reply-To", true));
			envelope.push(parse_header("To", true));
			envelope.push(parse_header("CC", true));
			envelope.push(parse_header("BCC", true));
			envelope.push(parse_header("In-Reply-To", false));
			envelope.push(parse_header("Message-ID", false));
		}
	}

	if(base.subnum==65535)
		resp=inbox_map.offset[msgnum];
	else
		resp=idx.offset+1;
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

	fmat=fmat.sort(sort_format);

	for(i in fmat) {
		if(typeof(fmat[i])=='object') {
			// We already handled this I hope...
			if(objtype == undefined)
				continue;
			switch(objtype) {
				case 'BODY[HEADER.FIELDS':
					tmp='';
					get_rfc822_header();
					resp += objtype+" (";
					for(j in fmat[i]) {
						resp+=fmat[i][j]+" ";
						re=new RegExp("^("+fmat[i][j]+":.*)$", "im");
						m=re.exec(rfc822.header);
						if(m!=null) {
							tmp += m[1]+"\r\n";
						}
					}
					tmp += "\r\n";
					resp=resp.substr(0,resp.length-1)+")] "+encode_binary(tmp)+" ";
					break;
			}
			continue;
		}
		if(fmat[i].toUpperCase().substr(0,4)=='BODY') {
			// TODO: Handle BODY* stuff correctly (MIME Decode)
			switch(fmat[i].toUpperCase()) {
				case 'BODY[TEXT]':
					set_seen_flag();
					// fall-through
				case 'BODY.PEEK[TEXT]':
					get_rfc822_text();
					resp += fmat[i].replace(/\.PEEK/,"").toUpperCase()+" "+encode_binary(rfc822.text)+" ";
					break;

				case 'BODY[HEADER]':
					set_seen_flag();
					// fall-through
				case 'BODY.PEEK[HEADER]':
					get_rfc822_header();
					resp += fmat[i].replace(/\.PEEK/,"").toUpperCase()+" "+encode_binary(rfc822.header)+" ";
					break;

				case 'BODY[]':
					set_seen_flag();
					// fall-through
				case 'BODY.PEEK[]':
					get_rfc822();
					resp += fmat[i].replace(/\.PEEK/,"").toUpperCase()+" "+encode_binary(rfc822.header+rfc822.text)+" ";
					break;

				case 'BODY[HEADER.FIELDS':
					set_seen_flag();
				case 'BODY.PEEK[HEADER.FIELDS':
					objtype=fmat[i].replace(/\.PEEK/,"").toUpperCase();
					break;

				case 'BODYSTRUCTURE':
					get_rfc822_size();
					get_rfc822_text();
					resp += 'BODYSTRUCTURE ("TEXT" "PLAIN" ("CHARSET" "IBM437") NIL NIL "8BIT" '+rfc822.text.length+' '+rfc822.text.split(/\r\n/).length+" NIL NIL NIL) ";
					break;

				case 'BODY[1]':
					set_seen_flag();
				case 'BODY.PEEK[1]':
					get_rfc822_text();
					resp += 'BODY[1] '+encode_binary(rfc822.text)+' ';
					break;
			}
		}
		else {
			switch(fmat[i].toUpperCase()) {
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
					resp += 'INTERNALDATE '+strftime('"%d-%b-%C%y %H:%M:%S ', hdr.when_imported_time)+format('%+05d " ', hdr.when_imported_zone_offset);
					break;
				case 'RFC822.SIZE':
					get_rfc822_size();
					resp += "RFC822.SIZE "+rfc822.size+" ";
					break;
				case 'RFC822.TEXT':
					set_seen_flag();
					get_rfc822_text();
					resp += fmat[i].replace(/\.PEEK/,"").toUpperCase()+" "+encode_binary(rfc822.text)+" ";
					break;
				case 'RFC822.HEADER':
					set_seen_flag();
					get_rfc822_header();
					resp += fmat[i].replace(/\.PEEK/,"").toUpperCase()+" "+encode_binary(rfc822.header)+" ";
					break;
				case 'RFC822':
					set_seen_flag();
					get_rfc822();
					resp += fmat[i].replace(/\.PEEK/,"").toUpperCase()+" "+encode_binary(rfc822.header+rfc822.text)+" ";
					break;
				case 'ENVELOPE':
					set_seen_flag();
					get_envelope();
					resp += 'ENVELOPE ('+envelope.join(" ")+') ';
					break;
			}
		}
	}
	if(seen_changed && !sent_flags) {
		get_header();
		resp += "FLAGS ("+calc_msgflags(hdr.attr, hdr.netattr, base.subnum==65535, base.cfg==undefined?null:base.cfg.code, msgnum, readonly)+") ";
	}
	if(uid && !sent_uid)
		resp += "UID "+idx.number+" ";
	resp=resp.replace(/\s*$/,'');
	resp += ")";
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
				idx=base.get_msg_index(uid?parseInt(i,10):inbox_map.uid[parseInt(i,10)-1]);
				if(idx==null || idx.to != user.number)
					continue;
			}
			else
				idx=base.get_msg_index(uid?false:true, parseInt(i,10)-(uid?0:1));
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
			line=client.socket.recvline(10240, 300);
	
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
				line=client.socket.recvline(10240, 300);
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

	if(msg==null || msg_ptrs[code] < msg) {
		flags += '\\Recent ';
	}

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
	var re;
	var has_sep=false;

	if(match=='')
		return([""]);

	re=new RegExp("\\"+sepchar+"$");
	if(group.search(re)!=-1)
		has_sep=true;
	re=new RegExp("^\\"+sepchar);
	if(match.search(re)!=-1)
		has_sep=true;
	wmatch=group;
	if(wmatch.length > 0 && !has_sep)
		wmatch += sepchar;
	wmatch += match;
	wmatch=wmatch.replace(/([\\\^\$\+\?\.\(\)\|\{\}])/g,"\\$1");
	wmatch=wmatch.replace(/\*/g, ".\*");
	wmatch=wmatch.replace(/\%/g, "[^"+sepchar+"]\*");
	wmatch="^"+wmatch+"$";
	re=new RegExp(wmatch);

	if(re.test("INBOX"))
		ret.push("INBOX");

	for(grp in msg_area.grp_list) {
		if(re.test(msg_area.grp_list[grp].description))
			ret.push(msg_area.grp_list[grp].description+sepchar);

		for(sub in msg_area.grp_list[grp].sub_list) {
			if(re.test(msg_area.grp_list[grp].description+sepchar+msg_area.grp_list[grp].sub_list[sub].description)) {
				if((!subscribed) || msg_area.grp_list[grp].sub_list[sub].scan_cfg&SCAN_CFG_NEW)
					ret.push(msg_area.grp_list[grp].description+sepchar+msg_area.grp_list[grp].sub_list[sub].description);
			}
		}
	}
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

function generate_inbox_map(base)
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
		inbox_map=generate_inbox_map(base);
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

function display_list(cmd, groups)
{
	var group;
	var base;

	for(group in groups) {
		if(groups[group].substr(-1)==sepchar)
			untagged(cmd+' (\\Noselect) '+encode_string(sepchar)+' '+encode_string(groups[group].substr(0,groups[group].length-1)));
		else {
			if(groups[group] == '') {
				untagged(cmd+' (\\Noselect) '+encode_string(sepchar)+' '+encode_string(groups[group]));
			}
			else {
				base=new MsgBase(getsub(groups[group]));
				if(base.cfg != undefined) {
					if(base.last_msg > msg_area.sub[base.cfg.code].scan_ptr)
						untagged(cmd+' (\\Noinferiors \\Marked) '+encode_string(sepchar)+' '+encode_string(groups[group]));
					else
						untagged(cmd+' (\\Noinferiors \\UnMarked) '+encode_string(sepchar)+' '+encode_string(groups[group]));
				}
				else
					untagged(cmd+' (\\Noinferiors) '+encode_string(sepchar)+' '+encode_string(groups[group]));
			}
		}
	}
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

			display_list("LIST", groups);
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

			display_list("LSUB", groups);
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
								inbox_map=generate_inbox_map(base);
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
								inbox_map=generate_inbox_map(base);
							response.push(inbox_map.unseen);
						}
						else
							response.push(base.total_msgs+1);
						break;
				}
			}
			base.close();
			update_status();
			untagged("STATUS "+encode_string(args[1])+" ("+response.join(" ")+")");
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

function datestr(date)
{
	return(strftime('%d-%b-%C%y', date));
}

function parse_date(date)
{
	var ret=0;
	var match;
	var m,d,y;
	var dt;

	match=date.match(/^([0-9]{1,2})-([A-Za-z]{3})-([0-9]{4})$/);
	if(match==null)
		return(0);
	d=parseInt(match[1],10);
	m={Jan:1,Feb:2,Mar:3,Apr:4,May:5,Jun:6,Jul:7,Aug:8,Sep:9,Oct:10,Nov:11,Dec:12}[match[2]];
	y=parseInt(match[3],10);
	if(m==undefined)
		return(0);
	if(d<1 || d>31)
		return(0);
	if(y<1970 || y>2099)
		return(0);
	dt=new Date(y,m,d);
	return(dt.valueOf()/1000);
}

function parse_rfc822_date(date)
{
	var dt=new Date(date);

	return(dt.valueOf()/1000);
}

function do_search(args, uid)
{
	var search_set={idx:[],hdr:[],body:[]};
	var i,j;
	var failed;
	var idx,hdr,body;
	var result=[];

	while(args.length) {
		switch(args.shift().toUpperCase()) {
			case 'ALL':
				search_set.idx.push(function(idx) { return true; });
				break;
			case 'ANSWERED':
				search_set.idx.push(function(idx) { if(base.subnum==65535 && (idx.attr & MSG_REPLIED)) return true; return false; });
				break;
			case 'BODY':
				search_set.body.push(eval("function(body) { return(body.indexOf("+args.shift().toUpperCase().toSource()+")!=-1) }"));
				break;
			case 'DELETED':
				search_set.idx.push(function(idx) { if(idx.attr & MSG_DELETE) return true; return false; });
				break;
			case 'DRAFT':
				search_set.idx.push(function(idx) { return false; });
				break;
			case 'FLAGGED':
				search_set.idx.push(function(idx) { if(idx.attr & MSG_VERIFIED) return true; return false; });
				break;
			case 'FROM':
				search_set.hdr.push(eval("function(hdr) { return(get_from(hdr).toUpperCase().indexOf("+args.shift().toUpperCase().toSource()+")!=-1) }"));
				break;
			case 'KEYWORD':
				search_set.hdr.push(eval("function(hdr) { var flags="+parse_flags([args.shift()]).toSource()+"; if((hdr.attr & flags.attr)==flags.attr && (hdr.netattr & flags.netattr)==flags.netattr) return true; return false;}"));
				break;
			case 'NEW':
				// TODO: We don't do \Recent in INBOX and don't do \Seen in subs
				search_set.idx.push(eval("function(idx) { if(base.cfg==undefined) return false; if(msg_ptrs[base.cfg.code] < idx.number) return true; return false; }"));
				break;
			case 'OLD':
				// TODO: We don't do \Recent in INBOX and don't do \Seen in subs
				search_set.idx.push(eval("function(idx) { if(base.cfg==undefined) return true; if(msg_ptrs[base.cfg.code] < idx.number) return false; return true; }"));
				break;
			case 'RECENT':
				// TODO: We don't do \Recent in INBOX
				search_set.idx.push(eval("function(idx) { if(base.cfg==undefined) return false; if(msg_ptrs[base.cfg.code] < idx.number) return true; return false; }"));
				break;
			case 'SEEN':
				// We don't do \Seen in subs
				search_set.idx.push(function(idx) { if(base.subnum != 65535) return false; if(idx.attr & MSG_READ) return true; return false });
				break;
			case 'SUBJECT':
				search_set.hdr.push(eval("function(hdr) { return(hdr.subject.toUpperCase().indexOf("+args.shift().toUpperCase().toSource()+")!=-1) }"));
				break;
			case 'TO':
				search_set.hdr.push(eval("function(hdr) { return(hdr.to.toUpperCase().indexOf("+args.shift().toUpperCase().toSource()+")!=-1) }"));
				break;
			case 'UID':
				search_set.idx.push(eval("function(idx) { var good_uids="+parse_seq_set(args.shift(), true).toSource()+"; var i; for(i in good_uids) { if(good_uids[i]==idx.number) return true; } return false; }"));
				break;
			case 'UNANSWERED':
				search_set.idx.push(function(idx) { if(base.subnum==65535 && (idx.attr & MSG_REPLIED)) return false; return true; });
				break;
			case 'UNDELETED':
				search_set.idx.push(function(idx) { if(idx.attr & MSG_DELETE) return false; return true; });
				break;
			case 'UNDRAFT':
				search_set.idx.push(function(idx) { return true; });
				break;
			case 'UNFLAGGED':
				search_set.idx.push(function(idx) { if(idx.attr & MSG_VERIFIED) return false; return true; });
				break;
			case 'UNKEYWORD':
				search_set.hdr.push(eval("function(hdr) { var flags="+parse_flags([args.shift()]).toSource()+"; if((hdr.attr & flags.attr)==flags.attr && (hdr.netattr & flags.netattr)==flags.netattr) return false; return true;}"));
				break;
			case 'BEFORE':
				search_set.hdr.push(eval("function(hdr) { var before="+parse_date(args.shift()).toSource()+"; if(hdr.when_imported_time < before) return true; return false; }"));
				break;
			case 'ON':
				search_set.hdr.push(eval("function(hdr) { var on="+datestr(parse_date(args.shift())).toSource()+"; if(datestr(hdr.when_imported_time) == on) return true; return false; }"));
				break;
			case 'SINCE':
				search_set.hdr.push(eval("function(hdr) { var since="+parse_date(args[0]).toSource()+"; var since_str="+datestr(parse_date(args.shift())).toSource()+"; if(hdr.when_imported_time > since && datestr(hdr.when_imported_time) != since_str) return true; return false; }"));
				break;
			case 'SENTBEFORE':
				search_set.hdr.push(eval("function(hdr) { var before="+parse_date(args.shift()).toSource()+"; if(parse_rfc822_date(hdr.date) < before) return true; return false; }"));
				break;
			case 'SENTON':
				search_set.hdr.push(eval("function(hdr) { var on="+datestr(parse_date(args.shift())).toSource()+"; if(datestr(parse_rfc822_date(hdr.date)) == on) return true; return false; }"));
				break;
			case 'SENTSINCE':
				search_set.hdr.push(eval("function(hdr) { var since="+parse_date(args[0]).toSource()+"; var since_str="+datestr(parse_date(args.shift())).toSource()+"; if(parse_rfc822_date(hdrdate) > since && datestr(parse_rfc822_date(hdr.date)) != since_str) return true; return false; }"));
				break;
			// TODO: Unhandled SEARCH terms
			case 'HEADER':
				args.shift();
			case 'LARGER':
			case 'SMALLER':
				// Calculate RFC822.SIZE
			case 'CC':
				// Is there a CC header?
			case 'BCC':
				// There's a BCC header?
			case 'TEXT':
				// Needs RFC822
				args.shift();
			case 'NOT':
			case 'OR':
			default:
				search_set.idx.push(function(idx) { return false; });
		}
	}

	for(i=base.first_msg; i<=base.last_msg; i++) {
		failed=false;
		idx=base.get_msg_index(i);
		if(idx==null)
			continue;
		if(base.subnum==65535) {
			if(idx.to != user.number)
				continue;
		}
		if(search_set.idx.length > 0) {
			for(j in search_set.idx) {
				if(search_set.idx[j](idx)==false)
					failed=true;
			}
			if(failed)
				continue;
		}
		if(search_set.hdr.length > 0) {
			hdr=base.get_msg_header(i);
			for(j in search_set.hdr) {
				if(search_set.hdr[j](hdr)==false)
					failed=true;
			}
			if(failed)
				continue;
		}
		body=base.get_msg_index(i);
		if(search_set.body.length > 0) {
			body=base.get_msg_body(i,true,true,true).toUpperCase();
			for(j in search_set.body) {
				if(search_set.body[j](body)==false)
					failed=true;
			}
			if(failed)
				continue;
		}
		if(!failed)
			result.push(uid?idx.number:(base.subnum==65535?inbox_map.offset[i]:idx.offset+1));
	}

	untagged("SEARCH "+result.join(" "));
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

			tagged(tag, "OK", "How about I pretend to expunge and you pretend that's OK?");
			//tagged(tag, "NO", "Can't expunge... wait for maintenance");
		},
	},
	SEARCH:{
		arguments_valid:function(count) {
			return(count >= 1);
		},
		handler:function(args) {
			var tag=args[0];

			// TODO: Support (or ignore) CHARSET in search commands
			if(args[1]=='CHARSET') {
				tagged(tag, "NO", "I don't support CHARSET in SEARCH.");
				return;
			}
			do_search(args.slice(1), false);
			tagged(tag, "OK", "And that was your results.");
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
				case 'SEARCH':
					// TODO: Support (or ignore) CHARSET in SEARCH
					if(args[2]=='CHARSET') {
						tagged(tag, "NO", "I don't support CHARSET in SEARCH.");
						return;
					}
					do_search(args.slice(2), true);
					tagged(tag, "OK", "And that was your results.");
					break;
				default:
					tagged(tag, "NO", "Help, I'm useless.");
					break;
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
	line=client.socket.recvline(10240, 300);
	if(line != null) {
		debug_log("IMAP RECV: "+line, true);
		parse_command(line);
	}
	else {
		untagged("BYE No lolligaggers here!");
		client.socket.close();
		exit(0);
	}
}
