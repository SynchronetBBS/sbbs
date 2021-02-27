// irc.js

// Deuce's IRC client module for Synchronet
// With the "Manny Mods".  :-)

// $Id: irc.js,v 1.60 2020/08/29 01:02:01 rswindell Exp $

// disable auto-termination.
var old_auto_terminate=js.auto_terminate;
js.on_exit("js.auto_terminate=old_auto_terminate");
js.auto_terminate=false;

const REVISION = "$Revision: 1.60 $".split(' ')[1];
const SPACEx80 = "                                                                                ";
const MAX_HIST = 50;

load("sbbsdefs.js");
load("nodedefs.js");
load("sockdefs.js");	// SOCK_DGRAM

load("irclib.js");

// Global vars
var irc_server="irc.synchro.net";
var irc_port=6667;
var default_channel="#synchronet";
var connect_timeout=15;	// Seconds
var connected=0;
var quit=0;
var nick=user.handle;
var nicks=new Array();
var loading=true;
var real_names=true;
js.on_exit("console.ctrlkey_passthru = " + console.ctrlkey_passthru);
console.ctrlkey_passthru=~(134217728);

// Commands to send...
var client_cmds = {
	'PASS':{minparam:1,maxparam:1},		// Must be sent before NICK/USER
	'NICK':{minparam:1,maxparam:1},
	'USER':{minparam:4,maxparam:4},
	'OPER':{minparam:2,maxparam:2},
	'QUIT':{minparam:0,maxparam:1},
	'JOIN':{minparam:1,maxparam:2},
	'PART':{minparam:1,maxparam:2},
	'TOPIC':{minparam:1,maxparam:2},
	'NAMES':{minparam:1,maxparam:1},
	'LIST':{minparam:1,maxparam:2},
	'MOTD':{minparam:0,maxparam:1},
	'VERSION':{minparam:0,maxparam:1},
	'ADMIN':{minparam:0,maxparam:1},
	'CONNECT':{minparam:1,maxparam:3},
	'TIME':{minparam:0,maxparam:1},
	'STATS':{minparam:0,maxparam:2},
	'INFO':{minparam:0,maxparam:1},
	'MODE':{minparam:1,maxparam:Infinity},
	'PRIVMSG':{minparam:2,maxparam:2},
	'NOTICE':{minparam:2,maxparam:2},
	'USERHOST':{minparam:1,maxparam:Infinity},
	'KILL':{minparam:1,maxparam:2},
	'SQUIT':{minparam:2,maxparam:2},
	'INVITE':{minparam:2,maxparam:2},
	'KICK':{minparam:2,maxparam:3},
	'LINKS':{minparam:0,maxparam:2},
	'TRACE':{minparam:0,maxparam:1},
	'WHO':{minparam:0,maxparam:2},
	'WHOIS':{minparam:1,maxparam:2},
	'WHOWAS':{minparam:1,maxparam:3},
	'PING':{minparam:1,maxparam:2},
	'PONG':{minparam:1,maxparam:2},
	'AWAY':{minparam:0,maxparam:1},
	'REHASH':{minparam:0,maxparam:0},
	'RESTART':{minparam:0,maxparam:0},
	'SUMMON':{minparam:1,maxparam:3},
	'USERS':{minparam:0,maxparam:1},
	'WALLOPS':{minparam:1,maxparam:1},
	'ISON':{minparam:1,maxparam:Infinity},
	'LUSERS':{minparam:0,maxparam:2},
	'SERVLIST':{minparam:0,maxparam:2},
	'SQUERY':{minparam:2,maxparam:2},
	'DIE':{minparam:0,maxparam:0},
};

/* Command-line options go BEFORE command-line args */
var irc_theme = "irc-default.js";
ARGPARSE: for (cmdarg=0;cmdarg<argc;cmdarg++) {
	switch(argv[cmdarg]) {
		case "-A":
		case "-a":
			real_names=false;
			break;
		case "-T":
		case "-t":
			irc_theme=argv[++cmdarg];
			break;
		default:
			break ARGPARSE;
	}
}

/* Load the defined theme. -- Cyan */
load(irc_theme);

/* Command-line args can override default server values */
if(argv[cmdarg]!=undefined)
	irc_server=argv[cmdarg++];
if(argv[cmdarg]!=undefined)
	irc_port=Number(argv[cmdarg++]);
if(argv[cmdarg]!=undefined)
	default_channel=argv[cmdarg++];

default_channel=default_channel.replace(/\s+/g,"_");

sock=new Socket();
sock.bind(0,server.interface_ip_address);	// Use globally defined intereface in sbbs.ini
history=new History();
screen=new Screen();

// Connect
if(!sock.connect(irc_server,irc_port)) {
	log(format("!IRC connection to %s FAILED with error %d"
		,irc_server,sock.last_error));
	alert(irc_server+" not available");
	clean_exit();
}

send_cmd("PASS", user.security.password);	// for use with JS IRC server
if (nick=="")
	nick=user.alias;
nick=nick.replace(/\s+/g,"_");
send_cmd("NICK", nick);
username=user.alias;
username=username.replace(/\s+/g,"_");
send_cmd("USER", username+" 0 * :"+(real_names?user.name:user.alias)+" ("+client.ip_address+")");

channels=new Channels();

// Wait for welcome message...
while(!connected)  {
	if(sock.poll(connect_timeout))  {
		wait_for(["433","422","376"]);
		connected=1;
	}
	else  {
		alert("Response timeout");
		sock.close();
		clean_exit();
	}
}

// Main loop
while(!quit)  {
	if(!sock.is_connected || !connected)  {
		alert("Lost connection");
		sock.close();
		clean_exit();
	}

	if(!client.socket.is_connected)  {
		send_cmd("QUIT",":Dropped Carrier");
		quit=1;
		sock.close();
		bbs.hangup();
		clean_exit();
	}

	if(js.terminated) {
		send_cmd("QUIT",":Client terminated.");
		quit=1;
		sock.close();
		bbs.hangup();
		clean_exit();
	}

	if(bbs.get_time_left && !bbs.get_time_left()) {
		send_cmd("QUIT",":Out of time.");
		quit=1;
		sock.close();
		clean_exit();
	}

	if(sock.poll(.01)) {
		receive_command();
		screen.update(0);
	}
	else
		screen.update(100);
}
sock.close();
clean_exit();

function send_cmd(command, params)
{
	var snd;
	var plist;
	var pcnt = 0;
	var cmd;

	if (params === undefined)
		plist = [];
	else {
		if (params[0] == ':')
			params = params.substr(1);
		plist = params.split(/ \:?/);
	}

	command = command.toUpperCase();
	cmd = client_cmds[command];

	if (cmd === undefined) {
		screen.print_line("\x01H\x01R!! \x01N\x01R"+command+" not supported.\x01N\x01W");
		return;
	}
	snd = command;
	while (plist.length) {
		snd += ' ';
		pcnt++;
		if (pcnt == cmd.maxparam && plist.length > 1)
			snd += ':';
		snd += plist.shift();
	}
	if (pcnt < cmd.minparam) {
		screen.print_line("\x01H\x01R!! \x01N\x01R"+command+" requires at least "+cmd.minparam+" parameters\x01N\x01W");
		return;
	}
	sock.send(snd+"\r\n");
}

function handle_command(tag, prefix, command, message)  {
	var from_nick=null;
	var full_message=null;
	var tmp_str=null;
	var tmp_str2=null;
	var	i=0;

	switch(command) {
		case "PING":
			send_cmd("PONG",message.join(' '));
			break;
		case "NOTICE":
			message.shift();	// Target
			from_nick=get_highlighted_nick(prefix,message);
			full_message=message.join(" ");
			full_message=full_message.replace(/\x01/g,"");
			screen.print_line(format(NOTICE_FORMAT,from_nick,full_message));
			break;
		case "KICK":
			tmp_str=message.shift();	// Channel
			tmp_str2=message.shift();	// User
			from_nick=get_nick(prefix);
			full_message=message.join(" ");
			if(tmp_str2.toUpperCase()==nick.toUpperCase())  {
				channels.part(tmp_str,"");
			}
			screen.print_line(format(KICK_FORMAT,tmp_str2,tmp_str,full_message));
			break;
		case "PRIVMSG":
			if(message[1][0]=="\x01")  {
				// CTCP
				handle_ctcp(prefix,message);
			}
			else  {
				if(prefix=="")
					from_nick="[-]";
				else  {
					from_nick=get_highlighted_nick(prefix,message);
					message[0]=message[0].toUpperCase();
					if(channels.current != undefined)  {
						if(message[0]==channels.current.name)  {
							from_nick=format(FROM_NICK_CURCHAN,from_nick);
						}
						else  {
							from_nick=format(MSG_FORMAT,from_nick);
							if(message[0][0]=="#" || message[0][0]=="&")  {
								from_nick=from_nick+"\x01N\x01C"+message[0]+":\x01N\x01W ";
							}
							else
								console.beep();
						}
					}
					else  {
						from_nick=format(FROM_NICK_CURCHAN,from_nick);
					}
				}
				message.shift();	// Receiver
				screen.print_line(from_nick+" "+message.join(" "));
			}
			break;
		case "JOIN":
			from_nick=get_highlighted_nick(prefix,message);
			tmp_str=get_nick(prefix);
			if(tmp_str.toUpperCase()==nick.toUpperCase())  {
				channels.joined(message[0]);
			}
			else  {
				channels.nick_add(tmp_str,message[0]);
			}
			prefix=prefix.split("!")[1];
			screen.print_line(format(JOIN_FORMAT,from_nick,prefix,message[0]));
			break;
		case "QUIT":
			from_nick=get_highlighted_nick(prefix,message);
			tmp_str=get_nick(prefix);
			prefix=prefix.split("!")[1];
			full_message=message.shift();
			screen.print_line(format(QUIT_FORMAT,from_nick,channels.current.display,full_message+" "+message.join(" ")));
			channels.nick_quit(tmp_str);
			break;
		case "NICK":
			from_nick=get_highlighted_nick(prefix,message);
			tmp_str2=get_nick(prefix);
			prefix=prefix.split("!")[1];
			tmp_str=message.shift();
			tmp_str=tmp_str.split("!",1)[0]
			screen.print_line(format(NICK_FORMAT,from_nick,tmp_str));
			if(tmp_str2.toUpperCase()==nick.toUpperCase())  {
				nick=tmp_str;
				screen.update_statline();
			}
			channels.nick_change(tmp_str2,tmp_str);
			break;
		case "SQUIT":
			if(prefix.length > 0)  {
				from_nick=get_nick(prefix);
				tmp_str=message.shift();
				tmp_str2=message.shift();
				screen.print_line(format(SQUIT_FROM_NICK,from_nick,tmp_str,tmp_str2+message.join(" ")));
			}
			else  {
				tmp_str=message.shift();
				tmp_str2=message.shift();
				screen.print_line(SQUIT_FROM_SERVER,tmp_str,tmp_str2+message.join(" "));
			}
		case "PART":
			from_nick=get_highlighted_nick(prefix,message);
			tmp_str=get_nick(prefix);
			prefix=prefix.split("!")[1];
			screen.print_line(format(PART_FORMAT,from_nick,prefix,message[0]));
			channels.nick_part(tmp_str,message[0]);
			break;
		case "MODE":
			from_nick=get_highlighted_nick(prefix,message);
			prefix=prefix.split("!")[1];
			modestr="";
			for (modeidx=1;modeidx<message.length;modeidx++) {
				if (modeidx > 1)
					modestr += " ";
				modestr += message[modeidx];
			}
			screen.print_line(format(MODE_FORMAT,from_nick,message[0],modestr));
			break;
		case "TOPIC":
			from_nick=get_highlighted_nick(prefix,message);
			tmp_str=message.shift();
			tmp_str2=message.join(" ");
			for(i=0;i<channels.length;i++)  {
				if(tmp_str.toUpperCase()==channels.channel[i].name)  {
					channels.channel[i].topic=tmp_str2;
					screen.update_statline();
					screen.print_line(format(TOPIC_FORMAT,from_nick,tmp_str,tmp_str2));
				}
			}
			break;

		// Numeric reply codes.
		case "211":		// Trace Server
		case "352":		// WHO reply
		case "206":		// Trace Server
		case "213":		// Stats CLINE
		case "214":		// Stats NLINE
		case "215":		// Stats ILINE
		case "216":		// Stats KLINE
		case "218":		// Stats YLINE
		case "241":		// Stats LLINE
		case "311":		// WHOIS reply
		case "314":		// WHOWAS reply
		case "367":		// Ban List
		case "200":		// Trace Link
		case "243":		// Stats OLINE
		case "244":		// Stats HLINE
		case "317":		// WHOISIDLE Reply
		case "324":		// Channel Modes
		case "201":		// Trace Connecting
		case "202":		// Trace Handshake
		case "203":		// Trace Unknown
		case "204":		// Trace Operator
		case "205":		// Trace User
		case "208":		// New type of trace
		case "261":		// Trace LOG
		case "312":		// WHOISSERVER Reply
		case "322":		// LIST data
		case "341":		// Invite being sent
		case "351":		// (server) VERSION reply
		case "364":		// Links
		case "212":		// Stats Command
		case "313":		// WHOISOPERATOR Reply
		case "319":		// WHOISCHANNELS Reply
		case "301":		// AWAY Reply
		case "318":		// End of WHOIS Reply
		case "369":		// End of WHOWAS Reply
		case "321":		// LIST Start
		case "342":		// Is Summoning.
		case "315":		// End of WHO
		case "365":		// End of LINKS
		case "368":		// End of ban list
		case "382":		// Rehashing
		case "391":		// Time reply
		case "219":		// End if stats
		case "221":		// UMODE reply
		case "252":		// # Operators online
		case "253":		// # unknown connections
		case "254":		// # channels
		case "256":		// Admin info
		case "001":		// Sent on successful registration
		case "002":		// Sent on successful registration
		case "003":		// Sent on successful registration
		case "004":		// Sent on successful registration
		case "005":		// Sent on successful registration
		case "265":             // Local Users
		case "266":             // Global Users
		case "381":		// You're OPER
		case "302":		// USERHOST Reply
		case "303":		// ISON Reply
		case "305":		// You are no longer away
		case "306":		// You are now marked as away
		case "323":		// LIST end
		case "371":		// INFO
		case "374":		// End if info list
		case "392":		// USERS Start
		case "393":		// USERS
		case "394":		// USERS End
		case "395":		// No users
		case "242":		// Stats Uptime
		case "251":		// LUSER Client
		case "255":		// # Clients / # Servers
		case "257":		// Admin LOC 1
		case "258":		// Admin LOC 2
		case "259":		// Admin e-mail
		case "375":		// MOTD Start
		case "372":		// MOTD
		case "333":		// Extended TOPIC info (apparently)
			message.shift();	// Client
			screen.print_line("\x01H\x01C!! \x01N\x01C"+message.join(" ")+"\x01N\x01W");
			break;

		// Things that actually need dealing with...
		case "376":		// MOTD End
			if (loading) {
				loading = false;
				channels.join(default_channel);
			}
			message.shift();	// Client
			screen.print_line("\x01H\x01C!! \x01N\x01C"+message.join(" ")+"\x01N\x01W");
			break;
		case "331":		// No Topic
		case "332":		// Topic
			for(i=0;i<channels.length;i++)  {
				if(message[1].toUpperCase()==channels.channel[i].name)  {
					channels.channel[i].topic=message[2];
					screen.update_statline();
				}
			}
			break;

		case "353":		// Name reply
			message.shift();		// Client
			message.shift();		// Symbol
			tmp_str=message.shift().toUpperCase();	// Channel
			message = message[0].split(' ');
			for(i=0;i<message.length;i++) {
				switch(message[i][0]) {
					case '~':	// Founder
					case '&':	// Protected
					case '@':	// Op
					case '%':	// Half-op
					case '+':	// Voice
						message[i]=message[i].substr(1);
						break;
				}
			}
			for(i=0;i<channels.length;i++)  {
				if(tmp_str == channels.channel[i].name)  {
					channels.channel[i].nick=message;
				}
			}
			screen.print_line("\x01N\x01B\1hPeople in "+tmp_str+" right now: "+message.join(" "));
			break;

		case "366":		// End of Names
			break;

		// Error Codes
		case "433":		// Nickname already in use
			message.shift();	// Client
			nick=message.shift()+"_";
			send_cmd("NICK", nick);
			break;
		// <word1> <word2> :Message errors
		case "441":		// Nick not on channel
		case "443":		// User already on channel (invite)
		case "401":		// No such nick
		case "402":		// No such server
		case "403":		// No such channel
		case "404":		// Cannot send to channel
		case "405":		// Too Many Channels
		case "406":		// Was no suck nickname
		case "407":		// Too many targets
		case "413":		// No toplevel domain specified
		case "414":		// Wildcard in TLD
		case "421":		// Unknown Command
		case "423":		// No admin info available
		case "432":		// Erroneous Nick... bad chars
		case "436":		// Nick collision KILL
		case "442":		// You aren't on that channel
		case "444":		// User not logged in (From SUMMON command)
		case "461":		// Not enough params
		case "467":		// Channel key already set
		case "471":		// Channel is full (+l)
		case "472":		// Unknown mode
		case "473":		// Invide only channel (And YOU aren't invided)
		case "474":		// Banned from channel
		case "475":		// Bad channel key (+k)
		case "482":		// Not ChanOP so can't do that.
		case "422":		// MOTD is missing
		case "409":		// No origin
		case "411":		// No recipient
		case "412":		// No text to send
		case "424":		// File Error
		case "431":		// No nickname given
		case "445":		// SUMMON disabled
		case "446":		// USERS disabled
		case "451":		// You aren't registered
		case "462":		// You're ALREADY registered
		case "463":		// You aren't allowed to connect (HOST based)
		case "464":		// Incorrect password
		case "465":		// You are banned
		case "481":		// You aren't an IRCOP so you can't do that.
		case "483":		// You can't kill a server.
		case "491":		// No O-lines for your host
		case "501":		// Unknown MODE flag
		case "502":		// Can't change other users mode
			message.shift();	// Client
			if (message.length > 1) {
				tmp_str=message.shift();
				screen.print_line("\x01H\x01R!! \x01N\x01R"+tmp_str+" - "+message.join(" ")+"\x01N\x01W");
			}
			else {
				screen.print_line("\x01H\x01R!! \x01N\x01R"+message.join(" ")+"\x01N\x01W");
			}
			break;
		default:
			screen.print_line("\x01N\x01R"+prefix+" "+command+" "+message.join(" ")+"\x01N\x01W");
	}
}

function get_command()
{
	var tag = "";
	var prefix="";
	var command=null;
	var line;
	var message = [];
	var i;

	if(sock.poll(0)) {
		line=sock.recvline();
		if(!line)
			return;

		if (line[0] == '@') {
			tag = line.slice(1, line.indexOf(" "));
			line = line.substr(tag.length + 2);
		}
		if (line[0] == ':') {
			prefix = line.slice(1, line.indexOf(" "));
			line = line.substr(prefix.length + 2);
		}
		message.push(tag);
		message.push(prefix);
		while (line.length > 0) {
			if (line[0] == ':') {
				message.push(line.substr(1));
				line = '';
			}
			else {
				i = line.indexOf(' ');
				if (i == -1) {
					message.push(line);
					line = '';
				}
				else {
					message.push(line.slice(0, i));
					line = line.substr(i + 1);
				}
			}
		}
		return message;
	}
}

function receive_command() {
	var tag = "";
	var prefix="";
	var command=null;
	var message;

	var message = get_command();
	if (message == undefined)
		return;
	tag = message.shift();
	prefix = message.shift();
	command = message.shift();

	handle_command(tag, prefix, command, message);
}

function wait_for(commands)  {
	var tag = '';
	var prefix="";
	var command=null;
	var message="";
	var i=0;

	while(1)  {
		if(!sock.is_connected)  {
			alert("Lost connection");
			sock.close();
			clean_exit();
		}

		if(!client.socket.is_connected)  {
			send_cmd("QUIT", ":Dropped Carrier.");
			quit=1;
			sock.close();
			bbs.hangup();
			clean_exit();
		}
		message = get_command();
		if (message != undefined) {
			tag = message.shift();
			prefix = message.shift();
			command = message.shift();
			handle_command(tag,prefix,command,message);
			for(i=0;i<commands.length;i++)  {
				if(command==commands[i])  {
					return command;
				}
			}
		}
		// Don't handle user input at this point!
		// screen.update();
	}
}

function send_command(command,param)  {
	var params=[null];
	var send_to=null;
	var full_params;
	var i=0;
	var got="";

	switch(command)  {
		case "HELP":
			const fmt = "/%-25s %s";
			screen.print_line(format(fmt, "help", "Display this list"));
			screen.print_line(format(fmt, "q[uit]", "Leave IRC Module " + REVISION));
			screen.print_line(format(fmt, "me <text>", "Send an action message"));
			screen.print_line(format(fmt, "quote <text>", "Send a literal message"));
			screen.print_line(format(fmt, "msg <nick>", "Send a private message"));
			screen.print_line(format(fmt, "j[oin] <#channel>", "Join a channel"));
			screen.print_line(format(fmt, "n[ext]", "Switch to next channel"));
			screen.print_line(format(fmt, "p[revious]", "Switch to previous channel"));
			screen.print_line(format(fmt, "part", "Leave current channel"));
			screen.print_line(format(fmt, "topic [#channel] <text>", "Set channel topic"));
			screen.print_line(format(fmt, "kick [nick]", "Kick a user from channel"));
			break;
		case "MSG":
			params=param.split(" ");
			send_to=params.shift();
			send_cmd("PRIVMSG", send_to+" :"+params.join(" "));
			screen.print_line(send_to+"\x01H\x01C<\x01N\x01C-\x01N\x01W "+params.join(" "));
			break;
		case "X":
		case "Q":
		case "QUIT":
			send_cmd("QUIT", param);
			quit=1;
			sock.close();
			clean_exit();
			break;
		case "J":
		case "JOIN":
			channels.join(param);
			break;
		case "ME":
			if(channels.current==undefined)  {
				screen.print_line("\x01H\x01RYou are not in a channel!\x01N\x01W");
			}
			else  {
				channels.current.send("\x01ACTION "+param+"\x01");
				screen.print_line("\x01N\x01B*\x01W "+nick+" "+param);
			}
			break;
		case "CTCP":
			params=param.split(" ");
			send_to=params.shift();
			full_params=params.join(" ");
			full_params=full_params.toUpperCase();
			send_cmd("PRIVMSG", send_to+" :\x01"+full_params+"\x01");
			break;
		case "PART":
			// If the user specifies a channel, this SHOULD part that channel,
			// not the current one.
			channels.part(channels.current.name,param);
			break;
		case "N":
		case "NEXT":
			channels.index+=1;
			if(channels.index>=channels.length)  {
				channels.index=0;
			}
			screen.update_statline();
			break;
		case "P":
		case "PREVIOUS":
		case "PREV":
			channels.index-=1;
			if(channels.index<0)  {
				channels.index=channels.length-1;
			}
			screen.update_statline();
			break;
		case "TOPIC":
			if (param.substr(0,1) == '#' || param.substr(0,1) == '&')  {
				send_cmd(command, param);
			}
			else  {
				send_cmd(command, channels.current.name+" "+param);
			}
			break;
		case "KICK":
			if (param.substr(0,1) == '#' || param.substr(0,1) == '&')  {
				send_cmd(command, param);
			}
			else  {
				send_cmd(command, channels.current.name+" "+param);
			}
			break;
		case "QUOTE":
			if (param.length)
				sock.send(param+"\r\n");
			break;
		default:
			if(command[0]=="#" || command[0]=="&")  {
				for(i=0;i<channels.length;i++)  {
					if(command.toUpperCase()==channels.channel[i].name)  {
						channels.index=i;
						screen.update_statline();
					}
				}
			}
			else  {
				send_cmd(command, param);
			}
	}
}

function handle_ctcp(prefix,message)  {
	var from_nick=null;
	var to_nick=null;
	var full_message=null;

	ctcp_command=message[1].substr(2);
	ctcp_command=ctcp_command.replace(/\x01/g,"");
	switch(ctcp_command)  {
		case "ACTION":
			message.shift();
			message.shift();
			from_nick=get_highlighted_nick(prefix,message);
			full_message=message.join(" ");
			full_message=full_message.replace(/\x01/g,"");
			screen.print_line("\x01N\x01B*\x01W "+from_nick+" "+full_message);
			break;
		case "FINGER":
			from_nick=get_highlighted_nick(prefix,message);
			to_nick=get_nick(prefix);
			send_cmd("NOTICE", to_nick+" :\x01FINGER :"+user.name+" ("+user.alias+") Idle: "+user.timeout+"\x01");
			screen.print_line(">"+from_nick+"<"+" CTCP FINGER Reply: "+user.name+" ("+user.alias+") Idle: "+user.timeout);
			break;
		case "VERSION":
			from_nick=get_highlighted_nick(prefix,message);
			to_nick=get_nick(prefix);
			send_cmd("NOTICE", to_nick+" :\x01VERSION Synchronet IRC Module:"+REVISION+":Synchronet"+"\x01");
			screen.print_line(">"+from_nick+"<"+" CTCP VERSION Reply: VERSION Synchronet IRC Module:"+REVISION+":Synchronet");
			break;
		case "PING":
			message.shift();
			message.shift();
			from_nick=get_highlighted_nick(prefix,message);
			to_nick=get_nick(prefix);
			send_cmd("NOTICE", to_nick+" :\x01PING "+message.join(" ")+"\x01");
			screen.print_line(">"+from_nick+"<"+" CTCP PING Reply.");
			break;
		case "TIME":
			from_nick=get_highlighted_nick(prefix,message);
			to_nick=get_nick(prefix);
			send_cmd("NOTICE", to_nick+" :\x01TIME "+strftime("%A, %B %d, %I:%M:%S%p, %Y %Z",time())+"\x01");
			screen.print_line(">"+from_nick+"<"+" CTCP TIME Reply: "+strftime("%A, %B %d, %I:%M:%S%p, %Y %Z",time() ));
			break;
	}
}

function get_highlighted_nick(prefix,message)  {
	var nick_mentioned=0;
	var from_nick=null;

	// Check if your nick is in the text...
	from_nick=prefix;
	from_nick=from_nick.split("!",1)[0];
	re=new RegExp("\\b"+nick+"\\b","i");
	for(j=0;j<message.length;j++)  {
		if(message[j].search(re)>=0)  {
			nick_mentioned=1;
		}
	}
	if(nick_mentioned==1)  {
		from_nick="\x01N\x01Y"+from_nick+"\x01N\x01W";
	}
	// Not sure if I have to do this... but it makes me feel better.
	delete re;
	return from_nick;
}

function get_nick(prefix)  {
	// Check if your nick is in the text...
	var to_nick;

	to_nick=prefix.split("!",1)[0];
	return to_nick;
}

function clean_exit()  {
	exit();
}

// channel object
function Channel(cname)  {
	var got="";
	this.topic="No topic set";
	this.name=cname.toUpperCase();
	this.display=cname;
	this.topic="";
	this.part=Channel_part;
	this.send=Channel_send;
	this.nick=new Array();
	this.matchnick=Channel_matchnick;
}

function Channel_part(message)  {
	send_cmd("PART", this.name+" :"+message);
	screen.print_line("PART "+this.name+" "+message);
	this.name=null;
	this.display=null;
	this.topic=null;
	this.part=null;
}

function Channel_send(message)  {
	send_cmd("PRIVMSG", this.name+" :"+message);
}

function Channel_matchnick(nickpart)  {
	var i=0;
	var j=0;
	var count=0;
	var tmp_str="\x01N\x01BMatching Nicks:";
	var nick_var="";
	var partial=nickpart.toUpperCase();
	var matched="";
	var start="";

	if(partial=="")  {
		screen.print_line("\x01H\x01BNothing to match.\x01N\x01W");
		return null;
	}
	for(i=0;i<this.nick.length;i++)  {
		if(partial==this.nick[i].substr(0,partial.length).toUpperCase())  {
			tmp_str=tmp_str+" "+this.nick[i];
			nick_var=this.nick[i];
			count++;
			if(matched=="")  {
				matched=nick_var.toUpperCase();
			}
			else  {
				nick_var=nick_var.substr(0,matched.length);
				for(j=nick_var.length;matched.substr(0,j) != nick_var.substr(0,j).toUpperCase();j--)  {}
				nick_var=nick_var.substr(0,j);
				matched=nick_var.toUpperCase();
			}
		}
	}
	if(count<1)  {
		screen.print_line("\x01H\x01BNo matching nicks.\x01N\x01W");
		return null;
	}
	if(count>1 && matched.length==partial.length) {
		screen.print_line(tmp_str+"\x01N\x01W");
		return null;
	}
	return nick_var;
}

// channels object
function Channels()  {
	this.length=0;
	this.index=0;
	this.channel=new Array();
	this.join=Channels_join;
	this.part=Channels_part;
	this.joined=Channels_joined;
	this.__defineGetter__("current", function() {return this.channel[this.index];});
// Joining is not attempted until numeric 376 (End of MOTD)
//	this.join(default_channel);
	this.nick_change=Channels_nick_change;
	this.nick_quit=Channels_nick_quit;
	this.nick_part=Channels_nick_part;
	this.nick_add=Channels_nick_add;
}

function Channels_nick_change(from,to)  {
	var i=0;
	var j=0;

	for(i=0;i<this.channel.length;i++)  {
		for(j=0;j<this.channel[i].nick.length;j++)  {
			if(this.channel[i].nick[j].toUpperCase()==from.toUpperCase())  {
				this.channel[i].nick[j]=to;
			}
		}
	}
}

function Channels_nick_quit(nick)  {
	var i=0;
	var j=0;

	for(i=0;i<this.channel.length;i++)  {
		for(j=0;j<this.channel[i].nick.length;j++)  {
			if(this.channel[i].nick[j].toUpperCase()==nick.toUpperCase())  {
				this.channel[i].nick.splice(j,1);
			}
		}
	}
}

function Channels_nick_part(nick,cname)  {
	var i=0;
	var j=0;

	for(i=0;i<this.channel.length;i++)  {
		if(cname.toUpperCase()==this.channel[i].name)  {
			for(j=0;j<this.channel[i].nick.length;j++)  {
				if(this.channel[i].nick[j].toUpperCase()==nick.toUpperCase())  {
					this.channel[i].nick.splice(j,1);
				}
			}
		}
	}
}

function Channels_nick_add(nick,cname)  {
	var i=0;
	var j=0;

	for(i=0;i<this.channel.length;i++)  {
		if(cname.toUpperCase()==this.channel[i].name)  {
			this.channel[i].nick.push(nick);
		}
	}
}

function Channels_join(cname)  {
	send_cmd("JOIN", cname);
}

function Channels_joined(cname)  {
	this.index=this.channel.length;
	this.channel[this.channel.length]=new Channel(cname);
	this.length++;
}

function Channels_part(cname,message)  {
	var i;

	if(this.current==undefined)  {
		return;
	}
	cname=this.current.name;
	for(i=0;i<this.channel.length;i++)  {
		if(cname.toUpperCase()==this.channel[i].name)  {
			this.channel[i].part(message);
			this.channel.splice(i,1);
			this.length -= 1;
		}
	}
	if(this.index>=(this.channel.length-1))  {
		this.index=0;
	}
}

// Screen object
function Screen()  {
	console.clear();

	this.line=new Array(console.screen_rows-3);
	this.rows=console.screen_rows-3;	// Title, Status, and input rows are not counted.
	this.update_input_line=Screen_update_input_line;
	this.print_line=Screen_print_line;
	this.update_statline=Screen_update_statline;
	this.__defineGetter__("statusline", function() {
		// THIS NEEDS TO GO INTO THE SCREEN BUFFER!!! ToDo
		bbs.nodesync();
		if(connected)  {
			if(channels != undefined)  {
				if(channels.current != undefined)  {
					var nick_chan="";
					nick_char=format("\x01N\x014 Nick: %s   Channel: %s (%d)",nick,channels.current.display,channels.current.nick.length)+SPACEx80;
					return nick_char.substr(0,67)+" /help for help \x01N\x010\x01W";
				}
			}
		}
		return "\x01N\x014 Nick: "+nick+"   Channel: No Channel (0)"+SPACEx80.substr(0,79-49-nick.length)+" /help for help \x01N\x010\x01W";
	});
	this.__defineGetter__("topicline", function() {
		if(connected)  {
			if(channels != undefined)  {
				if(channels.current != undefined)  {
					if(channels.current.topic != undefined && channels.current.topic != '') {
						return "\x01H\x01Y\x014"+channels.current.topic.substr(0,79)+SPACEx80.substr(0,(79-channels.current.topic.length)>0?(79-channels.current.topic.length):0)+"\x01N\x01W\x010";
					}
				}
			}
		}
		return "\x01H\x01Y\x014No Topic"+SPACEx80.substr(0,71)+"\x01N\x01W\x010";
	});
	this.input_buffer="";
	this.input_pos=0;
	this.handle_key=Screen_handle_key;
	this.update=Screen_update;
	this.print_line("\1n\1hSynchronet \1cInternet Relay Chat \1wModule \1n" + REVISION + "\r\n");
}

function Screen_update_statline()  {
	var cname="";
	var topic="";

	console.ansi_gotoxy(1,console.screen_rows-2);
	if(channels.current==undefined)  {
		cname="No channel";
		topic="Not in channel";
	}
	else  {
		cname=channels.current.display;
		topic=channels.current.topic;
	}
	console.print(this.topicline);
	console.crlf();
	console.print(this.statusline);
	console.crlf();
	this.update_input_line();
}

function Screen_print_line(line)  {
	var i=0;
	var lastspace=0;
	var linestart=0;
	var prev_colour="";
	var last_colour="";
	var cname="";
	var topic="";

	console.line_counter=0;	// defeat pause
	console.ansi_gotoxy(1,console.screen_rows-2);
	// Remove bold
	line=line.replace(/\x02/g,"");
	// mIRC colour codes
	line=line.replace(/\x03([0-9\,]{1,5})/g,
		function(str,p1,offset,s)  {
			var p2;
			var ending=null;
			var codes=[null];
			var ret=null;

			ending="";
			codes=p1.split(",");
			p1=codes[0];
			codes.shift();
			p2=codes[0];
			codes.shift();
			ending=","+codes.join(",");
			if(p2==undefined)  {
				p2="-1";
			}
			if(p2.length>2)  {
				ending=p2.substr(2)+ending;
				p2=p2.substr(0,2);
			}
			if(p1.length>2)  {
				ending=p1.substr(2)+ending;
				p1=p1.substr(0,2);
			}
			p1=Number(p1);
			p2=Number(p2);
			p1=p1 % 16;
			if(p2>=0 && p2<=99)  {
				p2=Number(p2) % 8;
			}
			else  {
				p2=8;
			}
			switch(p1)  {
				case 0:
					ret="\x01H\x01W";
					break;
				case 1:
					ret="\x01N\x01K";
					break;
				case 2:
					ret="\x01N\x01B";
					break;
				case 3:
					ret="\x01N\x01G";
					break;
				case 4:
					ret="\x01N\x01R";
					break;
				case 5:
					ret="\x01N\x01Y";
					break;
				case 6:
					ret="\x01H\x01M";
					break;
				case 7:
					// This is supposed to be ORANGE damnit!
					ret="\x01H\x01R";
					break;
				case 8:
					ret="\x01H\x01Y";
					break;
				case 9:
					ret="\x01H\x01G";
					break;
				case 10:
					ret="\x01N\x01C";
					break;
				case 11:
					ret="\x01H\x01C";
					break;
				case 12:
					ret="\x01H\x01B";
					break;
				case 13:
					ret="\x01H\x01M";
					break;
				case 14:
					ret="\x01H\x01K";
					break;
				case 15:
					ret="\x01N\x01W";
					break;
				default:
					ret="";
			}
			switch(p2)  {
				case 0:
					ret=ret+"\x017";
					break;
				case 1:
					ret=ret+"\x010";
					break;
				case 2:
					ret=ret+"\x014";
					break;
				case 3:
					ret=ret+"\x012";
					break;
				case 4:
					ret=ret+"\x011";
					break;
				case 5:
					ret=ret+"\x013";
					break;
				case 6:
					ret=ret+"\x015";
					break;
				case 7:
					// This is supposed to be ORANGE damnit!
					ret=ret+"\x016";
					break;
			}
			return ret+ending;
		}
	);
	if(line.length > 78)  {
		// Word Wrap...
		for(var j=0;j<=line.length;j++)  {
			switch(line.charAt(j))  {
				case "\x01":
					last_colour=last_colour+line.substr(j,2);
					j+=1;
					break;
				case " ":
					lastspace=j;
				default:
					if(i>=78)  {
						if(lastspace==linestart-1)  {
							lastspace=j;
						}
						console.print(prev_colour+line.substring(linestart,lastspace+1));
						prev_colour=last_colour;
						console.cleartoeol();
						console.crlf();
						this.line.shift();
						this.line.push(prev_colour+line.substring(linestart,lastspace+1));
						linestart=lastspace+1;
						j=lastspace;
						i=0;
					}
					i+=1;
			}
		}
	}
	if(i<=78)  {
		console.print(prev_colour+line.substr(linestart));
		console.cleartoeol();
		this.line.shift();
		this.line.push(prev_colour+line.substr(linestart));
		console.crlf();
	}
	if(!connected)  {
		cname="No channel";
		topic="Not in channel";
	}
	else if (channels==undefined)  {
		cname="No channel";
		topic="Not in channel";
	}
	else if (channels.current==undefined)  {
		cname="No channel";
		topic="Not in channel";
	}
	else  {
		cname=channels.current.display;
		topic=channels.current.topic;
	}
	console.print(this.topicline);
	console.crlf();
	console.print(this.statusline);
	console.crlf();
	this.update_input_line();
}

function Screen_update_input_line()  {
	var line_pos=this.input_pos;
	var line_str=this.input_buffer;
	var line_start=0;
	var line_len=this.input_buffer.length;

	if(line_len-this.input_pos < 39)  {
		line_start=line_len-78;
	}
	else if (this.input_pos < 39)  {
		line_start=0;
	}
	else  {
		line_start=this.input_pos-39;
	}
	if(line_start<0)  {
		line_start=0;
	}
	line_pos=this.input_pos-line_start;
	line_str=this.input_buffer.substr(line_start,78);
	if(line_start>0)  {
		line_str='+'+line_str.substr(1);
	}
	if(line_start+78 < line_len)  {
		line_str=line_str.slice(0,77)+'+';
	}

	console.line_counter=0;	// defeat pause
	console.ansi_gotoxy(1,console.screen_rows);
	console.clearline();
	printf("%s",line_str);
	console.ansi_gotoxy(line_pos+1,console.screen_rows);
}

function Screen_update(wait)  {
	while(1) {
		var key=console.inkey(wait);
		if(key!="")
			this.handle_key(key);
		else
			break;
	}
}

function Screen_handle_key(key)  {
	var commands=[null];
	var command=null;
	var nickmatch="";
	var lastspace=0;

	switch(key)  {
		case "\r":
			if(this.input_buffer=="")  {
				break;
			}
			history.addline(this.input_buffer);
			while(history.line.length>MAX_HIST)  {
				history.shift();
			}
			this.input_buffer=this.input_buffer.replace(/%C/g,"\x03");
			this.input_buffer=this.input_buffer.replace(/%%/g,"%");
			if(this.input_buffer.substr(0,1)=="/" && this.input_buffer.substr(1,1)!="/")  {
				commands=this.input_buffer.split(" ");
				command=commands.shift();
				command=command.substr(1);
				command=command.toUpperCase();
				send_command(command,commands.join(" "));
			}
			else  {
				if(this.input_buffer.substr(0,1)=="/")  {
					this.input_buffer=this.input_buffer.substr(1);
				}
				if(channels.current==undefined)  {
					this.print_line("\x01H\x01RYou are not in a channel!\x01N\x01W");
				}
				else  {
					channels.current.send(this.input_buffer);
					this.print_line("\x01N\x01M<\x01N\x01W"+nick+"\x01N\x01M>\x01N\x01W "+this.input_buffer);
				}
			}
			this.input_buffer="";
			this.input_pos=0;
			this.update_input_line();
			break;
		case "\x7f":
			if(this.input_buffer.length > this.input_pos) {
				this.input_buffer=this.input_buffer.slice(0,this.input_pos)+this.input_buffer.slice(this.input_pos+1);
				this.update_input_line();
				break;
			}
			/* Fall-through - delete at end of line is backspace */
		case "\x08":
			if(this.input_pos > 0)  {
				this.input_buffer=this.input_buffer.slice(0,this.input_pos-1)+this.input_buffer.slice(this.input_pos);
				this.input_pos--;
				this.update_input_line();
			}
			break;
		case "\x0b":
			this.input_buffer=this.input_buffer+"%C";
			this.input_pos+=2;
			break;
		case "\x1e":		// Up arrow
			if(history.index==null)  {
				history.incomplete=this.input_buffer;
			}
			this.input_buffer=history.previous;
			this.input_pos=this.input_buffer.length;
			this.update_input_line()
			break;
		case "\x0a":		// Down arrow
			if(history.index==null)  {
				history.incomplete=this.input_buffer;
			}
			this.input_buffer=history.next;
			this.input_pos=this.input_buffer.length;
			this.update_input_line()
			break;
		case "\x1d":		// Left arrow
			if(this.input_pos > 0)  {
				this.input_pos--;
			}
			this.update_input_line();
			break;
		case "\x02":		// Home
			this.input_pos=0;
			this.update_input_line()
			break;
		case "\x05":		// End
			this.input_pos=this.input_buffer.length;
			this.update_input_line()
			break;
		case "\x06":		// right arrow
			if(this.input_pos < (this.input_buffer.length))  {
				this.input_pos++;
			}
			this.update_input_line();
			break;
		case "\t":			// Tab
			lastspace=this.input_buffer.lastIndexOf(" ",this.input_pos);
			nickmatch=channels.current.matchnick(this.input_buffer.substr(lastspace+1,this.input_pos-lastspace-1));
			if(nickmatch != null) {
				this.input_buffer=this.input_buffer.substr(0,lastspace+1)+nickmatch+(lastspace==-1?": ":" ")+this.input_buffer.substr(this.input_pos);
				this.input_pos=lastspace+2+nickmatch.length;
				if(lastspace==-1)
					this.input_pos++;
			}
			else
				console.beep();
			this.update_input_line();
			break;
		default:
			if(ascii(key)<ascii(' '))  {
				if(ascii(key) != 27 && console.handle_ctrlkey!=undefined)  {
					console.line_counter=0;	// defeat pause
					console.ansi_gotoxy(1,console.screen_rows-1);
					console.clearline();
					console.ansi_gotoxy(1,console.screen_rows);
					console.clearline();
					console.ansi_gotoxy(1,console.screen_rows-2);
					console.clearline();
					console.handle_ctrlkey(key,0); // for now
					console.print(this.topicline);
					console.crlf();
					console.print(this.statusline);
					console.crlf();
					this.update_input_line();
				}
			}
			else  {
				this.input_buffer=this.input_buffer.slice(0,this.input_pos)+key+this.input_buffer.slice(this.input_pos);
				this.input_pos++;
				this.update_input_line();
			}
	}
}

// History object
function History()  {
	this.index=0;
	this.max=MAX_HIST;
	this.line=new Array("");
	this.addline=History_addline;
	this.index=-1;
	this.incomplete="";
	this.__defineGetter__("next", function()  {
		if(this.index==null)  {
			this.index=this.line.length;
		}
		this.index+=1;
		if(this.index>=this.line.length)  {
			this.index=null;
			return this.incomplete;
		}
		else  {
			return this.line[this.index];
		}
	});
	this.__defineGetter__("previous", function()  {
		if(this.index==null)  {
			this.index=this.line.length;
		}
		this.index-=1;
		if(this.index<0)  {
			this.index=0;
		}
		return this.line[this.index];
	});
}

function History_addline(line)  {
	this.line.push(line);
	while(this.line.length>this.max)  {
		this.line.shift();
	}
	this.index=this.line.length-1;
	this.index=null;
}
