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

var line;
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
		client.socket.send("+\r\n");
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

function sendrecent()
{
	untagged("0 RECENT");
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
			untagged("0 RECENT");
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
			untagged("0 RECENT");
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
				send_fetch_response(seq[i], data_items);
			}
			//tagged(tag, "OK", "There they are!");
			tagged(tag, "NO", "Can't fetch... I'm useless.");
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

			tagged(tag, "NO", "Help, I'm useless.");
		},
	}
};

var line;
client.socket.send("* OK Give 'er\r\n");
while(1) {
	line=client.socket.recvline(1024, 300);
	if(line != null) {
		debug_log("IMAP RECV: "+line);
		args=parse_line();
		execute_line(args);
	}
	else {
		untagged("BYE No lolligaggers here!");
		client.socket.close();
		exit(0);
	}
}
