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
load("822header.js");
load("mime.js");

var sepchar="|";
var debug=true;
var debugRX=true;


/*****************************/
/* Message header extensions */
/*****************************/

MsgBase.HeaderPrototype.get_from=function (force)
{
	if(force===true)
		delete this.from_header;

	if(this.from_header==undefined) {
		if(!this.from_net_type || this.from_net_addr.length==0)    /* local message */
			this.from_header = this.from + " <" + this.from.replace(/ /g,".").toLowerCase() + "@" + system.inetaddr + ">";
		else if(!this.from_net_addr.length)
			this.from_header = this.from;
		else if(this.from_net_addr.indexOf('@')!=-1)
			this.from_header = this.from+" <"+this.from_net_addr+">";
		else
			this.from_header = this.from+" <"+this.from.replace(/ /g,".").toLowerCase()+"@"+this.from_net_addr+">";
	}
	return(this.from_header);
}

MsgBase.HeaderPrototype.parse_headers=function(force)
{
	if(force===true)
		delete this.parsed_headers;

	if(this.parsed_headers==undefined)
		this.parsed_headers=parse_headers(this.get_rfc822_header(force))
	return(this.parsed_headers);
}

MsgBase.HeaderPrototype.get_envelope=function (force)
{
	function parse_header(header, is_addresses) {
		if(header==undefined || header.length==0)
			return("NIL");

		header=header.pop().replace(new RegExp("^"+abnf.field_name+abnf.WSP+"*:","i"),"");
		header=header.replace(/\r\n$/,'');
		header=strip_CFWS(header);

		/* TODO: Use mime.js ABNF to parse this correctly */
		if(is_addresses) {
			if((m2=header.match(/^\s*(.*)\s+<([^@]*)@(.*)>\s*$/))!=null) {
				m2[1]=m2[1].replace(/^"(.*)"$/, "$1");
				return '(('+[encode_string(m2[1]), "NIL", encode_string(m2[2]), encode_string(m2[3])].join(" ")+'))';
			}
			else if((m2=header.match(/^\s*(.*)\s+<([^@]*)>\s*$/))!=null) {
				m2[1]=m2[1].replace(/^"(.*)"$/, "$1");
				return '(('+[encode_string(m2[1]), "NIL", encode_string(m2[2]), "NIL"].join(" ")+'))';
			}
			else if((m2=header.match(/^\s*<([^@]*)@(.*)>\s*$/))!=null) {
				return '(('+["NIL", "NIL", encode_string(m2[1]), encode_string(m2[2])].join(" ")+'))';
			}
			else if((m2=header.match(/^\s*([^@]*)@(.*)\s*$/))!=null) {
				return '(('+["NIL", "NIL", encode_string(m2[1]), encode_string(m2[2])].join(" ")+'))';
			}
			else
				return '(('+[encode_string(header), "NIL", "NIL", "NIL"].join(" ")+'))';
		}
		else
			return(encode_string(header));
	}

	var hdrs;

	if(this.envelope==undefined) {
		hdrs=this.parse_headers();
		envelope=[];
		envelope.push(parse_header(hdrs.date, false));
		envelope.push(parse_header(hdrs.subject, false));
		envelope.push(parse_header(hdrs.from, true));
		envelope.push(parse_header(hdrs.sender, true));
		envelope.push(parse_header(hdrs['reply-to'], true));
		envelope.push(parse_header(hdrs.to, true));
		envelope.push(parse_header(hdrs.cc, true));
		envelope.push(parse_header(hdrs.bcc, true));
		envelope.push(parse_header(hdrs['in-reply-to'], false));
		envelope.push(parse_header(hdrs['message-id'], false));
	}
	return(envelope);
}


/***********************/
/* Debugging Functions */
/***********************/

function dump_obj(obj, name)
{
	var i;

	for(i in obj) {
		if(typeof(obj[i])=='object')
			dump_obj(obj[i], name+'['+i+']');
		else
			log(name+'['+i+']="'+obj[i]+'"');
	}
}

function debug_log(line, rx)
{
	if(debug)
		log(line);
	else if(rx && debugRX)
		log(line);
}


/**************/
/* Socket I/O */
/**************/

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


/**********************/
/* Encoding functions */
/**********************/
/* 
 * These encode at least as is passed (token/string/binary)
 */
function encode_binary(str)
{
	return '{'+str.length+'}\r\n'+str;
}

function encode_string(str)
{
	if(str=='')
		return('""');

	if(str.search(/[\r\n\x80-\xff]/)==-1) {
		str=str.replace(/[\\\"]/, "\\$1");
		return '"'+str+'"';
	}

	return encode_binary(str);
}

function encode_token(str)
{
	if(str=='')
		return(encode_string(str));

	if(str.search(/[\(\)\{ \x00-\x1F\*\%\"\\\]]/)==-1)
		return str;

	return(encode_string(str));
}

/*************************************************************/
/* Fetch response generation... this is the tricky bit.  :-) */
/*************************************************************/
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
	var mime;
	var part;
	var extension;

	/*
	 * Most of these functions just diddle variables in this function
	 */
	function get_mime() {
		if(mime==undefined) {
			get_rfc822();
			mime=parse_message(rfc822.header+rfc822.text);
		}
	}

	function get_header() {
		if(hdr == undefined) {
			hdr=base.get_msg_header(msgnum);
		}
	}

	function get_rfc822_header() {
		get_header();
		rfc822.header=hdr.get_rfc822_header();
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

	/*
	 * Sets the seen flag on a message (for INBOX only)
	 */
	function set_seen_flag() {
		if(readonly)
			return;
		if(base.subnum==-1) {
			get_header();
			if(!(hdr.attr & MSG_READ)) {
				hdr.attr |= MSG_READ;
				base.put_msg_header(msgnum);
				index=read_index(base);
				hdr=base.get_msg_header(msgnum);
				if(hdr.attr & MSG_READ)
					seen_changed=true;
			}
		}
		else {
			idx.attr |= MSG_READ;
		}
	}

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

	idx=index.idx[msgnum];
	resp=idx.offset;
	resp += " FETCH (";
	fmat=fmat.sort(sort_format);

	for(i in fmat) {
		/*
		 * This bit is for when a paremeter includes a list.
		 * This list will be an object, so we need special handling
		 */
		if(typeof(fmat[i])=='object') {
			// We already handled this I hope...
			if(objtype == undefined)
				continue;

			if(objtype.search(/^BODY\[[0-9.]*HEADER\.FIELDS$/)==0) {
				tmp='';
				for(j in fmat[i]) {
					if(part.headers[fmat[i][j].toLowerCase()]!=undefined)
						tmp += part.headers[fmat[i][j].toLowerCase()];
				}

				resp += objtype+" ("+fmat[i].join(" ")+")] "+encode_binary(tmp+"\r\n")+" ";
			}
			if(objtype.search(/^BODY\[[0-9.]*HEADER\.FIELDS\.NOT$/)==0) {
				tmp=eval(part.headers.toSource());
				delete tmp['::'];
				delete tmp[':mime:'];
				for(j in fmat[i]) {
					if(tmp[fmat[i][j].toLowerCase()]!=undefined)
						delete tmp[fmat[i][j].toLowerCase()];
				}
				tmp2='';
				for(j in tmp)
					tmp2 += tmp[j];

				resp += objtype+" ("+fmat[i].join(" ")+")] "+encode_binary(tmp2+"\r\n")+" ";
			}
			continue;
		}
		/*
		 * Handle the MIME stuff
		 */
		if(fmat[i].toUpperCase().substr(0,4)=='BODY') {
			function get_mime_part(fmat) {
				var m=fmat.match(/^BODY((?:\.PEEK)?)\[([^[\]]*)(?:\]\<([0-9]+)\.([0-9]+)\>)?/i);
				var specifiers;
				var i;
				var tmp;
				var part_name='';

				function encode_binary_part(start, len, str)
				{
					if(start==undefined || start=='')
						start=0;
					else
						start=parseInt(start,10);
					if(len==undefined || len=='')
						len=str.length;
					else
						len=parseInt(len,10);
					return(encode_binary(str.substr(start,len)));
				}

				part=mime;
				if(m==null)
					return(undefined);
				if(m[1].toUpperCase()!='.PEEK')
					set_seen_flag();
				part_name='BODY['+m[2]+']';
				specifiers=m[2].split('.');
				for(i=0; i<specifiers.length; i++) {
					tmp=parseInt(specifiers[i], 10);
					if(tmp > 0) {
						if(part.mime != undefined && part.mime.parts != undefined && part.mime.parts[tmp-1]!=undefined) {
							part=part.mime.parts[tmp-1];
						}
					}
					else
						break;
				}
				if(m[3]!=undefined && m[3]!='')
					part_name += '<'+m[3]+'>';
				switch(specifiers[i]) {
					case 'HEADER':
						if(specifiers[i+1]!=undefined) {
							objtype='BODY['+specifiers.join('.');
							return undefined;
						}
						else
							return(part_name+" "+encode_binary_part(m[3],m[4],part.headers['::'].join('')+"\r\n")+' ');
					case 'MIME':
						return(part_name+" "+encode_binary_part(m[3],m[4],part.headers[':mime:'].join('')+"\r\n")+' ');
					case '':
						if(specifiers.length==1)
							return(part_name+" "+encode_binary_part(m[3],m[4],part.headers['::'].join('')+'\r\n'+part.text)+' ');
						// Fall-through
					case undefined:
					case 'TEXT':
						return(part_name+' '+encode_binary_part(m[3],m[4],part.text)+' ');
				}
			}

			get_mime();

			if((tmp=get_mime_part(fmat[i].toUpperCase()))==undefined) {
				extension=true;
				switch(fmat[i].toUpperCase()) {
					case 'BODY':
						extension=false;
					case 'BODYSTRUCTURE':
						function add_part(mime) {
							var i;
							var ret='(';

							if(mime.mime.parts != undefined) {
								for(i in mime.mime.parts)
									ret += add_part(mime.mime.parts[i]);
							}
							else
								ret += encode_string(mime.mime.parsed['content-type'].vals[0])+" ";

							ret += encode_string(mime.mime.parsed['content-type'].vals[1])+" ";
							if(mime.mime.parsed['content-type'].attrs==undefined)
								ret += 'NIL ';
							else {
								ret += '(';
								for(i in mime.mime.parsed['content-type'].attrs) {
									ret += encode_string(i)+" ";
									ret += encode_string(mime.mime.parsed['content-type'].attrs[i])+" ";
								}
								ret=ret.replace(/ $/, ") ");
							}
							if(mime.mime.parsed['content-id']==undefined)
								ret += 'NIL ';
							else
								ret += encode_string(mime.mime.parsed['content-id'].vals[0])+' ';
							if(mime.mime.parsed['content-description']==undefined)
								ret += 'NIL ';
							else
								ret += encode_string(mime.mime.parsed['content-description'].vals[0])+' ';

							if(mime.mime.parsed['content-type'].vals[0]!='multipart') {
								if(mime.mime.parsed['content-transfer-encoding']==undefined)
									ret += 'NIL ';
								else
									ret += encode_string(mime.mime.parsed['content-transfer-encoding'].vals[0])+' ';

								ret += encode_token(mime.text.length.toString())+' ';
							}

							if(mime.mime.parsed['content-type'].vals[0]=='text' || mime.mime.parsed['content-type'].vals[0]=='message') {
								i=mime.text.split(/\x0d\x0a/);
								ret=ret+encode_token(i.length.toString())+' ';
							}
							
							if(extension) {
								// TODO Add extension data...
							}
							
							ret=ret.replace(/ $/, ') ');
							return ret;
						}

						resp += 'BODYSTRUCTURE '+add_part(mime);
						break;
				}
			}
			else
				resp += tmp;
		}
		else {
			switch(fmat[i].toUpperCase()) {
				case 'FLAGS':
					get_header();
					resp += "FLAGS ("+calc_msgflags(idx.attr, hdr.netattr, base.subnum, msgnum, readonly)+") ";
					sent_flags=true;
					break;
				case 'UID':
					resp += "UID "+idx.number+" ";
					sent_uid=true;
					break;
				case 'INTERNALDATE':
					get_header();
					resp += 'INTERNALDATE '+strftime('"%d-%b-%C%y %H:%M:%S ', hdr.when_imported_time)+format('%+05d" ', hdr.when_imported_zone_offset);
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
					get_header();
					resp += 'ENVELOPE ('+hdr.get_envelope().join(" ")+') ';
					break;
			}
		}
	}
	if(seen_changed && !sent_flags) {
		get_header();
		resp += "FLAGS ("+calc_msgflags(idx.attr, hdr.netattr, base.subnum, msgnum, readonly)+") ";
	}
	if(uid && !sent_uid)
		resp += "UID "+idx.number+" ";
	resp=resp.replace(/ $/,'');
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
	var msgnum;

	if(uid)
		max=base.last_msg;
	else
		max=index.offsets.length;
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
			msgnum=parseInt(i, 10);
			if(!uid)
				msgnum=index.offsets[msgnum-1];
			if(msgnum==undefined || index.idx[msgnum]==undefined)
				continue;
			response.push(msgnum);
		}
	}
	response=response.sort(function(a,b) { return a-b; });
	for(i=0; i<response.length; i++) {
		if(response[i]==response[i+1]) {
			response.splice(i+1,1);
			i--;
		}
	}
	return(response);
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
			line=client.socket.recvline(10240, 1800);
	
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
var index={offsets:[],idx:{}};

// Command handling functions
any_state_command_handlers = {
	CAPABILITY:{
		arguments:0,
		handler:function (args) {
			var tag=args[0];

			untagged("CAPABILITY IMAP4rev1 AUTH=PLAIN LOGINDISABLED CHILDREN IDLE UNSELECT");
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
	IDLE:{	// RFC2177
		arguments:0,
		handler:function(args) {
			var tag=args[0];
			var elapsed=0;

			client.socket.send("+ Ooo, Idling... my favorite.\r\n");
			while(1) {
				line=client.socket.recvline(10240, 5);
				if(line==null) {
					elapsed += 5;
					if(elapsed > 1800) {
						untagged("BYE And I though *I* liked to idle!");
						client.socket.close();
						exit(0);
					}
					update_status();
				}
				else {
					debug_log("DONE IDLE: '"+line+"'", true);
					tagged(tag, "OK", "That was fun.");
					return;
				}
			}
		}
	}
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
				line=client.socket.recvline(10240, 1800);
				args=base64_decode(line).split(/\x00/);
				if(!login(args[1],args[2])) {
					tagged(tag, "NO", "No AUTH for you.");
					return;
				}
				tagged(tag, "OK", "Howdy.");
				read_cfg();
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
			read_cfg();
			state=Authenticated;
		},
	}
};

function sendflags(perm)
{
	var flags="";
	var pflags="";

	flags=calc_msgflags(0xffff, 0xffff, base.subnum, base.last_msg, readonly);
	pflags=calc_msgflags(0xffff, 0xffff, base.subnum, base.last_msg, readonly);
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

function calc_msgflags(attr, netattr, num, msg, readonly)
{
	var flags='';

	if(attr & MSG_PRIVATE)
		flags += "PRIVATE ";
	if(attr & MSG_READ)
		flags += "\\Seen ";
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
		if(num==-1)
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

	if(attr==0xffff || orig_ptrs[num] < msg) {
		flags += '\\Recent ';
	}

	if(!readonly) {
		if(msg > msg_ptrs[num])
			msg_ptrs[num]=msg;
		if(base != undefined && base.is_open)
			scan_ptr=msg;
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
				if((!subscribed) || (msg_area.grp_list[grp].sub_list[sub].scan_cfg&SCAN_CFG_NEW && (!(msg_area.grp_list[grp].sub_list[sub].scan_cfg&SCAN_CFG_YONLY))))
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

function count_unseen(index)
{
	var i;
	var count=0;
	var idx;

	for(i in index.offsets) {
		idx=index.idx[index.offsets[i]];
		if((idx.attr & MSG_READ)==0)
			count++;
	}
	return(count);
}

function count_recent(index, scan_ptr)
{
	var i;
	var count=0;
	var idx;

	for(i in index.offsets) {
		idx=index.idx[index.offsets[i]];
		if(idx.number > scan_ptr)
				count++;
	}
	return(count);
}

function update_status()
{
	var i;
	var idx;

	if(base==undefined || !base.is_open) {
		curr_status.exists=-1;
		curr_status.recent=-1;
		curr_status.unseen=-1;
		curr_status.uidnext=-1;
		curr_status.uidvalidity=-1;
		return;
	}
	if(base.subnum != index.subnum || base.last_msg != index.last)
		index=read_index(base);
	curr_status.exists=index.offsets.length;
	curr_status.recent=count_recent(index, orig_ptrs[base.subnum]);
	curr_status.unseen=count_unseen(index);
	if(index.offsets.length == 0)
		curr_status.uidnext=1;
	else
		curr_status.uidnext=index.idx[index.offsets[index.offsets.length-1]].number+1;
	curr_status.uidvalidity=0;
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

function read_index(base)
{
	var i;
	var idx;
	var index;

	index={offsets:[],idx:{}};
	index.first=base.first_msg;
	index.last=base.last_msg;
	index.subnum=base.subnum;
	index.total=base.total_msgs;

	if(base.cfg != undefined)
		index.code=base.cfg.code;
	for(i=0; i<index.total; i++) {
		idx=base.get_msg_index(true,i);
		if(idx==null)
			continue;
		if(base.subnum==-1) {
			if(idx.to != user.number)
				continue;
		}
		else {
			// Fake \Seen
			idx.attr &= ~MSG_READ;
			if(idx.number <= msg_area.sub[base.cfg.code].scan_ptr)
				idx.attr |= MSG_READ;
		}
		index.idx[idx.number]=idx;
		index.offsets.push(idx.number);
		idx.offset=index.offsets.length;
	}
	return(index);
}

function exit_func()
{
	close_sub();
	save_cfg();
}

function save_cfg()
{
	var cfg;

	if(user.number > 0) {
		cfg=new File(format(system.data_dir+"user/%04d.imap", user.number));
		if(cfg.open()) {
			cfg.iniSetObject("inbox",saved_config.inbox);
			cfg.close();
		}
	}
}

function close_sub()
{
	if(base != undefined && base.is_open) {
		msg_ptrs[base.subnum]=scan_ptr;
		if(msg_ptrs[base.subnum]!=orig_ptrs[base.subnum]) {
			if(base.subnum==-1) {
				saved_config.inbox.scan_ptr=scan_ptr;
			}
			else
				msg_area.sub[base.cfg.code].scan_ptr=scan_ptr;
		}
		base.close();
	}
}

function open_sub(sub)
{
	var i;
	var idx;

	close_sub();
	base=new MsgBase(sub);
	if(base == undefined || sub=="NONE!!!" || (!base.open())) {
		update_status();
		return false;
	}
	if(base.cfg != undefined) {
		if(orig_ptrs[base.subnum]==undefined) {
			orig_ptrs[base.subnum]=msg_area.sub[base.cfg.code].scan_ptr;
			msg_ptrs[base.subnum]=msg_area.sub[base.cfg.code].scan_ptr;
		}
	}
	scan_ptr=orig_ptrs[base.subnum];
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
			untagged(cmd+' (\\Noselect \\HasChildren) '+encode_string(sepchar)+' '+encode_string(groups[group].substr(0,groups[group].length-1)));
		else {
			if(groups[group] == '') {
				untagged(cmd+' (\\Noselect \\HasChildren) '+encode_string(sepchar)+' '+encode_string(groups[group]));
			}
			else {
				base=new MsgBase(getsub(groups[group]));
				if(base.cfg != undefined) {
					if(base.last_msg > msg_area.sub[base.cfg.code].scan_ptr)
						untagged(cmd+' (\\Noinferiors \\Marked \\HasNoChildren) '+encode_string(sepchar)+' '+encode_string(groups[group]));
					else
						untagged(cmd+' (\\Noinferiors \\UnMarked \\HasNoChildren) '+encode_string(sepchar)+' '+encode_string(groups[group]));
				}
				else
					untagged(cmd+' (\\Noinferiors \\HasNoChildren) '+encode_string(sepchar)+' '+encode_string(groups[group]));
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

			readonly=false;
			if(!open_sub(sub)) {
				tagged(tag, "NO", "Can't find "+args[1]+" ("+sub+")");
				state=Authenticated;
				return;
			}
			tagged(tag, "OK", "[READ-WRITE] Mailbox "+sub+" has been selected");
			state=Selected;
		},
	},
	EXAMINE:{	// Same as select onlt without the writability
		arguments:1,
		handler:function(args){
			var tag=args[0];
			var sub=getsub(args[1]);

			readonly=true;
			if(!open_sub(sub)) {
				tagged(tag, "NO", "Can't find "+args[1]+" ("+sub+")");
				state=Authenticated;
				return;
			}
			tagged(tag, "OK", "[READ-ONLY] Mailbox "+sub+" has been examined");
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
			var index;

			if(typeof(items)!="object")
				items=[items];
			base=new MsgBase(sub);
			if(base == undefined || (!base.open())) {
				tagged(tag, "NO", "Can't find your mailbox");
				return;
			}
			if(base.cfg != undefined && orig_ptrs[base.subnum]==undefined)
				orig_ptrs[base.subnum]=msg_area.sub[base.cfg.code].scan_ptr;
			index=read_index(base);
			base.close();
			for(i in items) {
				switch(items[i].toUpperCase()) {
					case 'MESSAGES':
						response.push("MESSAGES");
						response.push(index.offsets.length);
						break;
					case 'RECENT':
						response.push("RECENT");
						response.push(count_recent(index, orig_ptrs[base.subnum]));
						break;
					case 'UIDNEXT':
						response.push("UIDNEXT");
						if(index.offsets.length==0)
							response.push(1);
						else
							response.push(index.idx[index.offsets[index.offsets.length-1]].number+1);
						break;
					case 'UIDVALIDITY':
						response.push("UIDVALIDITY");
						response.push(0);
						break;
					case 'UNSEEN':
						response.push("UNSEEN");
						response.push(count_unseen(index));
						break;
				}
			}
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
		chflags=check_msgflags(chflags.attr, chflags.netattr, base.subnum==-1, hdr.to_net_type==NET_NONE?hdr.to==user.number:false, hdr.from_net_type==NET_NONE?hdr.from==user.number:false,base.is_operator);
		if(chflags.attr || chflags.netattr) {
			hdr.attr ^= chflags.attr;
			hdr.netattr ^= chflags.netattr;
			if(!readonly) {
				base.put_msg_header(seq[i], hdr);
				index=read_index(base);
			}
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
	m={Jan:0,Feb:1,Mar:2,Apr:3,May:4,Jun:5,Jul:6,Aug:7,Sep:8,Oct:9,Nov:10,Dec:11}[match[2]];
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
	var search_set={idx:[],hdr:[],body:[],all:[]};
	var i,j;
	var failed;
	var idx,hdr,body;
	var result=[];

	function get_func(args)
	{
		var next1,next2;
		var tmp;
	
		switch(args.shift().toUpperCase()) {
			case 'ALL':
				type="idx";
				search=(function(idx) { return true; });
				break;
			case 'ANSWERED':
				type="idx";
				search=(function(idx) { if(base.subnum==-1 && (idx.attr & MSG_REPLIED)) return true; return false; });
				break;
			case 'BODY':
				type="body";
				search=(eval("function(body) { return(body.indexOf("+args.shift().toUpperCase().toSource()+")!=-1) }"));
				break;
			case 'DELETED':
				type="idx";
				search=(function(idx) { if(idx.attr & MSG_DELETE) return true; return false; });
				break;
			case 'DRAFT':
				type="idx";
				search=(function(idx) { return false; });
				break;
			case 'FLAGGED':
				type="idx";
				search=(function(idx) { if(idx.attr & MSG_VERIFIED) return true; return false; });
				break;
			case 'FROM':
				type="hdr";
				search=(eval("function(hdr) { return(hdr.get_from()).toUpperCase().indexOf("+args.shift().toUpperCase().toSource()+")!=-1) }"));
				break;
			case 'KEYWORD':
				type="hdr";
				search=(eval("function(hdr) { var flags="+parse_flags([args.shift()]).toSource()+"; if((hdr.attr & flags.attr)==flags.attr && (hdr.netattr & flags.netattr)==flags.netattr) return true; return false;}"));
				break;
			case 'NEW':
				type="idx";
				search=(eval("function(idx) { if((idx.number > orig_ptrs[base.subnum]) && (idx.attr & MSG_READ)==0) return true; return false; }"));
				break;
			case 'OLD':
				type="idx";
				search=(eval("function(idx) { if(idx.number <= orig_ptrs[base.subnum]) return true; return false; }"));
				break;
			case 'RECENT':
				type="idx";
				search=(eval("function(idx) { if(idx.number > orig_ptrs[base.subnum]) return true; return false; }"));
				break;
			case 'SEEN':
				type="idx";
				search=(function(idx) { if(idx.attr & MSG_READ) return true; return false });
				break;
			case 'SUBJECT':
				type="hdr";
				search=(eval("function(hdr) { return(hdr.subject.toUpperCase().indexOf("+args.shift().toUpperCase().toSource()+")!=-1) }"));
				break;
			case 'TO':
				type="hdr";
				search=(eval("function(hdr) { return(hdr.to.toUpperCase().indexOf("+args.shift().toUpperCase().toSource()+")!=-1) }"));
				break;
			case 'UID':
				type="idx";
				search=(eval("function(idx) { var good_uids="+parse_seq_set(args.shift(), true).toSource()+"; var i; for(i in good_uids) { if(good_uids[i]==idx.number) return true; } return false; }"));
				break;
			case 'UNANSWERED':
				type="idx";
				search=(function(idx) { if(base.subnum==-1 && (idx.attr & MSG_REPLIED)) return false; return true; });
				break;
			case 'UNDELETED':
				type="idx";
				search=(function(idx) { if(idx.attr & MSG_DELETE) return false; return true; });
				break;
			case 'UNDRAFT':
				type="idx";
				search=(function(idx) { return true; });
				break;
			case 'UNFLAGGED':
				type="idx";
				search=(function(idx) { if(idx.attr & MSG_VERIFIED) return false; return true; });
				break;
			case 'UNKEYWORD':
				type="hdr";
				search=(eval("function(hdr) { var flags="+parse_flags([args.shift()]).toSource()+"; if((hdr.attr & flags.attr)==flags.attr && (hdr.netattr & flags.netattr)==flags.netattr) return false; return true;}"));
				break;
			case 'BEFORE':
				type="hdr";
				search=(eval("function(hdr) { var before="+parse_date(args.shift()).toSource()+"; if(hdr.when_imported_time < before) return true; return false; }"));
				break;
			case 'ON':
				type="hdr";
				search=(eval("function(hdr) { var on="+datestr(parse_date(args.shift())).toSource()+"; if(datestr(hdr.when_imported_time) == on) return true; return false; }"));
				break;
			case 'SINCE':
				type="hdr";
				search=(eval("function(hdr) { var since="+parse_date(args[0]).toSource()+"; var since_str="+datestr(parse_date(args.shift())).toSource()+"; if(hdr.when_imported_time > since && datestr(hdr.when_imported_time) != since_str) return true; return false; }"));
				break;
			case 'SENTBEFORE':
				type="hdr";
				search=(eval("function(hdr) { var before="+parse_date(args.shift()).toSource()+"; if(parse_rfc822_date(hdr.date) < before) return true; return false; }"));
				break;
			case 'SENTON':
				type="hdr";
				search=(eval("function(hdr) { var on="+datestr(parse_date(args.shift())).toSource()+"; if(datestr(parse_rfc822_date(hdr.date)) == on) return true; return false; }"));
				break;
			case 'SENTSINCE':
				type="hdr";
				search=(eval("function(hdr) { var since="+parse_date(args[0]).toSource()+"; var since_str="+datestr(parse_date(args.shift())).toSource()+"; if(parse_rfc822_date(hdrdate) > since && datestr(parse_rfc822_date(hdr.date)) != since_str) return true; return false; }"));
				break;
			case 'HEADER':
				type="hdr";
				search=(eval("function(hdr) { var hname="+args.shift().toLowerCase().toSource()+"; var match=new RegExp('^('+abnf.field_name+')'+abnf.WSP+'*:.*'+"+args.shift().toSource()+"; var hdrs=hdr.parse_headers(); var i; for(i in hdrs[hname]) if(hdrs[hname][i].search(match)==0) return true; return false;}"));
				break;
			case 'LARGER':
				type="all";
				search=(eval("function(idx,hdr,body) { var min="+parseInt(args.shift(),10)+"; if(body.length + hdr.get_rfc822_header().length > min) return true; return false;}"));
				break;
			case 'SMALLER':
				type="all";
				search=(eval("function(idx,hdr,body) { var max="+parseInt(args.shift(),10)+"; if(body.length + hdr.get_rfc822_header().length < max) return true; return false;}"));
				break;
			case 'CC':
				type="hdr";
				search=(eval("function(hdr) { var match=new RegExp('^('+abnf.field_name+')'+abnf.WSP+'*:.*'+"+args.shift().toSource()+"; var hdrs=hdr.parse_headers(); var i; if(hdrs.cc == undefined) return false; for(i in hdrs.cc) if(hdrs.cc[i].search(match)==0) return true; return false;}"));
				break;
			case 'BCC':
				type="hdr";
				search=(eval("function(hdr) { var match=new RegExp('^('+abnf.field_name+')'+abnf.WSP+'*:.*'+"+args.shift().toSource()+"; var hdrs=hdr.parse_headers(); var i; if(hdrs.bcc == undefined) return false; for(i in hdrs.bcc) if(hdrs.bcc[i].search(match)==0) return true; return false;}"));
				break;
			case 'TEXT':
				type="all";
				search=(eval("function(idx,hdr,body) { var str="+args.shift().toSource()+"; if(hdr.get_rfc822_header().indexOf(str)!=-1) return true; if(body.indexOf(str)!=-1) return true; return false}"));
				break;
			case 'NOT':
				next1=get_func(args);
				type=next1[0];
				search=(eval("function(x) { return !"+next1[1].toSource()+"(x)}"));
				break;
			case 'OR':
				next1=get_func(args);
				next2=get_func(args);
				if(next1[0]==next1[1]) {
					type=next1[0];
					search=(eval("function(x) { return ("+next1[1].toSource()+"(x)||"+next2[1].toSource()+"(x))}"));
				}
				else {
					// Needs to be all (sigh)
					type='all';
					tmp="function(idx,hdr,body) { return ("+next1[1].toSource();
					switch(next1[0]) {
						case 'idx':
						case 'hdr':
						case 'body':
							tmp += '('+next1[0]+')';
							break;
						case 'all':
							tmp += '(idx,hdr,body)';
							break;
					}
					tmp += '||'+next2[1].toSource();
					switch(next2[0]) {
						case 'idx':
						case 'hdr':
						case 'body':
							tmp += '('+next2[0]+')';
							break;
						case 'all':
							tmp += '(idx,hdr,body)';
							break;
					}
					tmp += ')}';
					search=eval(tmp);
				}
				break;
			default:
				type="idx";
				search=(function(idx) { return false; });
		}
		return([type,search]);
	}

	while(args.length) {
		i=get_func(args);
		search_set[i[0]].push(i[1]);
	}

	for(i in index.offsets) {
		failed=false;
		idx=index.idx[index.offsets[i]];
		if(search_set.idx.length > 0) {
			for(j in search_set.idx) {
				if(search_set.idx[j](idx)==false)
					failed=true;
			}
			if(failed)
				continue;
		}
		if(search_set.hdr.length > 0 || search_set.all.length > 0) {
			hdr=base.get_msg_header(idx.number);
			for(j in search_set.hdr) {
				if(search_set.hdr[j](hdr)==false)
					failed=true;
			}
			if(failed)
				continue;
		}
		if(search_set.body.length > 0 || search_set.all.length > 0) {
			body=base.get_msg_body(idx.number,true,true,true).toUpperCase();
			for(j in search_set.body) {
				if(search_set.body[j](body)==false)
					failed=true;
			}
			if(failed)
				continue;
		}
		if(search_set.all.length > 0) {
			for(j in search_set.all) {
				if(search_set.all[j](idx,hdr,body)==false)
					failed=true;
			}
			if(failed)
				continue;
		}
		if(!failed)
			result.push(uid?idx.number:idx.offset);
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

			close_sub();
			update_status();
			tagged(tag, "OK", "Closed.");
			state=Authenticated;
		},
	},
	UNSELECT:{	// RFC3691... like CLOSE with no implied EXPUNGE
		arguments:0,
		handler:function(args) {
			var tag=args[0];

			close_sub();
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
	},
};

function read_cfg()
{
	var cfg=new File(format(system.data_dir+"user/%04d.imap",user.number));

	if(cfg.open("r+")) {
		saved_config.inbox=cfg.iniGetObject("inbox");
		cfg.close();
	}
	else
		saved_config={inbox:{scan_ptr:0}};
	msg_ptrs[-1]=saved_config.inbox.scan_ptr;
	orig_ptrs[-1]=saved_config.inbox.scan_ptr;
}

var line;
var readonly=true;
var orig_ptrs={};
var msg_ptrs={};
var curr_status={exists:0,recent:0,unseen:0,uidnext:0,uidvalidity:0};
var saved_config={inbox:{scan_ptr:0}};
var scan_ptr;
const RFC822HEADER = 0xb0;  // from smbdefs.h

js.on_exit("exit_func()");
client.socket.send("* OK Give 'er\r\n");
while(1) {
	line=client.socket.recvline(10240, 1800);
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
