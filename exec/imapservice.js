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

	function get_header() {
		if(hdr == undefined)
			hdr=base.get_msg_header(msgnum);
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

	resp=idx.offset + " FETCH (";
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
				resp += "FLAGS () ";
				break;
			case 'UID':
				resp += "UID "+idx.number+" ";
				sent_uid=true;
				break;
			case 'INTERNALDATE':
				get_header();
				resp += 'INTERNALDATE '+strftime('"%d-%a-%C%y %H:%M:%S +0000" ', hdr.when_imported_time);
				break;
			case 'RFC822.SIZE':
				get_rfc822_size();
				resp += "RFC822.SIZE "+rfc822.size+" ";
				break;
			case 'RFC822.TEXT':
			case 'BODY[TEXT]':
				// Set "Seen" flag
				// fall-through
			case 'BODY.PEEK[TEXT]':
				get_rfc822_text();
				resp += format[i].replace(/\.PEEK/,"").toUpperCase()+" {"+rfc822.text.length+"}\r\n"+rfc822.text+" ";
				break;
			case 'BODY[HEADER]':
			case 'RFC822.HEADER':
				// Set "Seen" flag
				// fall-through
			case 'BODY.PEEK[HEADER]':
				get_rfc822_header();
				resp += format[i].replace(/\.PEEK/,"").toUpperCase()+" {"+rfc822.header.length+"}\r\n"+rfc822.header+" ";
				break;
			case 'RFC822':
			case 'BODY[]':
				// Set "Seen" flag
				// fall-through
			case 'BODY.PEEK[]':
				get_rfc822();
				resp += format[i].replace(/\.PEEK/,"").toUpperCase()+" {"+(rfc822.header.length+rfc822.text.length)+"}\r\n"+rfc822.header+rfc822.text+" ";
				break;
			case 'BODY[HEADER.FIELDS':
			case 'BODY.PEEK[HEADER.FIELDS':
				objtype=format[i].replace(/\.PEEK/,"").toUpperCase();
				break;
		}
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
	else
		max=base.total_msgs;
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

	return(execute_line(parse_line()));
}

// Global variables
const UnAuthenticated=0;
const Authenticated=1;
const Selected=2;
var state=UnAuthenticated;
var base;

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
	if(perm)
		untagged("OK [PERMANENTFLAGS ()] No flags yet... working on 'er");
	else
		untagged("FLAGS () Someday I'll add flags... *sigh*");
}

function sendexists()
{
	untagged(base.total_msgs+" EXISTS");
}

function sublist(group, match, subscribed)
{
	var grp;
	var sub;
	var ret=[];

	match=match.replace(/\%/, "*");
	group=group.replace(/\%/, "*");
	for(grp in msg_area.grp_list) {
		if(group=='' || wildmatch(true, msg_area.grp_list[grp].description, group)) {
			ret.push(msg_area.grp_list[grp].description+sepchar);

			for(sub in msg_area.grp_list[grp].sub_list) {
				if(wildmatch(true, msg_area.grp_list[grp].sub_list[sub].description, match, false)) {
					if((!subscribed) || msg_area.grp_list[grp].sub_list[sub].scan_cfg&SCAN_CFG_NEW)
						ret.push(msg_area.grp_list[grp].description+sepchar+msg_area.grp_list[grp].sub_list[sub].description);
				}
			}
		}
	}
	return(ret);
}

function getsub(longname) {
	var components;
	var grp;
	var sub;

	longname=longname.replace(/^"(.*)"$/, "$1");
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

authenticated_command_handlers = {
	SELECT:{
		arguments:1,
		handler:function(args){
			var tag=args[0];
			var sub=getsub(args[1]);

			base=new MsgBase(sub);
			if(base == undefined || (!base.open())) {
				tagged(tag, "NO", "Can't find "+args[1]+" ("+sub+")");
				return;
			}
			sendflags(false);
			untagged(base.total_msgs+" EXISTS");
			untagged(0+" RECENT");
			untagged("OK [UNSEEN "+(base.total_msgs+1)+"]");
			sendflags(true);
			untagged("OK [UIDNEXT "+(base.last_msg+1)+"]");
			untagged("OK [UIDVALIDITY 0]");
			tagged(tag, "OK", "[READ-ONLY] Mailbox "+sub+" has been selected");
			state=Selected;
		},
	},
	EXAMINE:{	// Same as select onlt without the writability
		arguments:1,
		handler:function(args){
			var tag=args[0];
			var sub=getsub(args[1]);

			base=new MsgBase(sub);
			if(base == undefined || (!base.open())) {
				tagged(tag, "NO", "Can't find your mailbox");
				return;
			}
			sendflags(false);
			untagged(base.total_msgs+" EXISTS");
			untagged(0+" RECENT");
			untagged("OK [UNSEEN "+(base.total_msgs+1)+"]");
			sendflags(true);
			untagged("OK [UIDNEXT "+(base.last_msg+1)+"]");
			untagged("OK [UIDVALIDITY 0]");
			tagged(tag, "OK", "[READ-ONLY] Mailbox "+sub+" has been selected");
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

			for(group in groups) {
				if(groups[group].substr(-1)==sepchar)
					untagged('LIST (\\Noselect) "'+sepchar+'" "'+groups[group].substr(0,groups[group].length-1)+'"');
				else
					untagged('LIST () "'+sepchar+'" "'+groups[group]+'"');
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

			if(typeof(items)!="object")
				items=[items];
			base=new MsgBase(sub);
			if(base == undefined || (!base.open())) {
				tagged(tag, "NO", "Can't find your mailbox");
				return;
			}
			for(i in items) {
				switch(items[i].toUpperCase()) {
					case 'MESSAGES':
						response.push("MESSAGES");
						response.push(base.total_msgs);
						break;
					case 'RECENT':
						response.push("RECENT");
						response.push(0);
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
						response.push(base.total_msgs);
						break;
				}
			}
			base.close();
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

selected_command_handlers = {
	SELECT:{	// Outlook uses SELECT when selected...
		arguments:1,
		handler:function(args) {
			base.close();
			authenticated_command_handlers.SELECT.handler(args);
		},
	},
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

			base.close();
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
			//tagged(tag, "NO", "Can't fetch... I'm useless.");
		},
	},
	STORE:{
		arguments:3,
		handler:function(args) {
			var tag=args[0];

			tagged(tag, "NO", "This is read-only.");
		},
	},
	COPY:{
		arguments:2,
		handler:function(args) {
			var tag=args[0];

			tagged(tag, "NO", "Hah! You can't even FETCH yet and you want to COPY?!?!");
		},
	},
	UID:{
		arguments:3,
		handler:function(args) {
			var tag=args[0];
			var cmd=args[1];
			var seq;
			var data_items;
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
				default:
					tagged(tag, "NO", "Help, I'm useless.");
			}
		},
	}
};

var line;
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
