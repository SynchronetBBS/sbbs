/*
 * IMAP Server... whee!
 * Refer to RFC 3501
 *
 * Copyright 2009, Stephen Hurd.
 * Don't steal my code bitches.
 *
 * $Id$
 */

var sepchar="|";
var debug=true;

// State handling functions
function not_authenticated()
{
}

function authenticated()
{
}

function selected()
{
}

function logout()
{
}

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

function parse_line(line) {
	var args=[];
	var tmp;
	var tmp2;
	var i;
	
	tmp=line.split(/ /);

	while(tmp.length) {
		if(tmp[0].substr(0,1)=='"') {
			tmp2='';
			while(tmp[0].substr(-1)!='"') {
				tmp2 += tmp.shift()+" ";
			}
			tmp2 += tmp.shift();
			tmp2=tmp2.replace(/^"(.*)"$/, "$1");
			args.push(tmp2);
			continue;
		}
		if(tmp[0].substr(0,1)=='(') {
			tmp2=[];
			while(tmp[0].substr(-1)!=')') {
				tmp2 += tmp.shift();
			}
			tmp2 += tmp.shift();
			tmp2=tmp2.replace(/^"(.*)"$/, "$1");
			args.push(tmp2);
			continue;
		}
		if(tmp[0].search(/^{[0-9]+}$/) != -1) {
			tmp2=tmp[0].replace(/^{([0-9]+)}$/, "$1");
			args.push(client.socket.recv(parseInt(tmp2)));
			tmp2=client.socket.recvline(1024, 300);
			tmp=tmp2.split(/ /);
			continue;
		}
		args.push(tmp.shift());
	}

	if(args.length < 2) {
		tagged(args[0], "BAD", "Bad dog, no cookie.");
		return;
	}
	command=args[1].toUpperCase();
	args.splice(1,1);
	if(any_state_command_handlers[command]!=undefined) {
		if(args.length-1 == any_state_command_handlers[command].arguments) {
			any_state_command_handlers[command].handler(args);
			return;
		}
	}
	switch(state) {
		case UnAuthenticated:
			if(unauthenticated_command_handlers[command]!=undefined) {
				if(args.length-1 == unauthenticated_command_handlers[command].arguments) {
					unauthenticated_command_handlers[command].handler(args);
					return;
				}
			}
			break;
		case Authenticated:
			if(authenticated_command_handlers[command]!=undefined) {
				if(args.length-1 == authenticated_command_handlers[command].arguments) {
					authenticated_command_handlers[command].handler(args);
					return;
				}
			}
			break;
	}
	tagged(args[0], "BAD", "Bad dog, no cookie.");
}

// Global variables
const UnAuthenticated=0;
const Authenticated=1;
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

function sublist(group, match)
{
	var grp;
	var sub;
	var ret=[];

	match=match.replace(/\%/, "*");
	for(grp in msg_area.grp_list) {
//		if(group=='' || msg_area.grp_list[grp].description==group) {
			ret.push(msg_area.grp_list[grp].description+sepchar);

			for(sub in msg_area.grp_list[grp].sub_list) {
				ret.push(msg_area.grp_list[grp].description+sepchar+msg_area.grp_list[grp].sub_list[sub].description);
			}
//		}
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
		},
	},
	EXAMINE:{	// Same as select onlt without the writability
		arguments:1,
		handler:function(args){
			var tag=args[0];
			var sub=args[1];

			base=new MsgBase(sub);
			if(base == undefined) {
				tagged(tag, "NO", "Can't find your mailbox");
				return;
			}
			sendflags(false);
			untagged("EXISTS");
			untagged("RECENT");
			untagged("OK [UNSEEN]");
			sendflags(true);
			untagged("OK [UIDNEXT "+(base.last_msg+1)+"]");
			untagged("OK [UIDVALIDITY 0]");
			tagged(tag, "OK", "[READ-ONLY] Mailbox "+sub+" has been selected");
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
			var sub=args[1];

			if(msg_area.sub[sub]!=undefined && msg_area.sub[sub].can_read) {
				tagged(tag, "OK", "Subscribed...");
				//msg_area.sub[sub].scan_cfg=SCAN_CFG_NEW;
			}
			else
				tagged(tag, "NO", "Can't subscribe to that sub (what is it?)");
		},
	},
	UNSUBSCRIBE:{
		arguments:1,
		handler:function(args) {
			var tag=args[0];
			var sub=args[1];

			if(msg_area.sub[sub]!=undefined && msg_area.sub[sub].can_read) {
				tagged(tag, "OK", "Unsubscribed...");
				//msg_area.sub[sub].scan_cfg=0;
			}
			else
				tagged(tag, "NO", "Can't unsubscribe that sub (what is it?)");
		},
	},
	LIST:{
		arguments:2,
		handler:function(args) {
			var tag=args[0];
			var ref=args[1];
			var sub=args[2];
			
			tagged(tag, "OK", "There you go.");
		},
	},
	LSUB:{
		arguments:2,
		handler:function(args) {
			var tag=args[0];
			var group=args[1];
			var sub=args[2];
			var groups=sublist(group, sub);
			var group;

			for(group in groups) {
				if(groups[group].substr(-1)==sepchar)
					untagged('LSUB (\Noselect) "'+sepchar+'" "'+groups[group].substr(0,-1)+'"');
				else
					untagged('LSUB () "'+sepchar+'" "'+groups[group]+'"');
			}
			tagged(tag, "OK", "There you go.");
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
		parse_line(line);
	}
	else {
		untagged("BYE No lolligaggers here!");
		client.socket.close();
		exit(0);
	}
}
