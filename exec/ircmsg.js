/* ircmsg.js */

/* Send a message (from stdin or the command-line) to an IRC channel */

/* $Id: ircmsg.js,v 1.34 2015/11/25 09:05:26 deuce Exp $ */

load("irclib.js");	// Thanks Cyan!

const REVISION = "$Revision: 1.34 $".split(' ')[1];

var server="irc.synchro.net";
var channel="#channel";
var port=6667;
var nick="nick";
var msg;
var join=false;
var exclude=new Array();
var quiet=false;

function mylog(msg)
{
    if(!quiet)
        log(LOG_INFO,"ircmsg: " + msg);
}

for(i=0;i<argc;i++) {
	switch(argv[i]) {
		case "-s":
			server=argv[++i];
			break;
		case "-j":
			join=true;
			break;
		case "-c":
			channel=argv[++i];
			break;
		case "-p":
			port=argv[++i];
			break;
		case "-n":
			nick=argv[++i];
			break;
        case "-q":
            quiet=true;
            break;
		case "-m":
			msg=argv[++i];
			if(msg==undefined || msg.search(/^[\r\n]*$/)!=-1) {
				alert("-m specified with blank message... aborting");
				exit();
			}
			break;
		case "-x":
			exclude.push(RegExp(argv[++i],"i"));
			break;
	}
}

mylog("Using nick: " + nick);
mylog("Connecting to: " +server+ " port " + port);
my_server = IRC_client_connect(server,nick,undefined,undefined,port);
if(!my_server) {
	alert("!Couldn't connect to " + server);
	exit();
}

var done=0;
while(!done) {
	while(!done && (response=my_server.recvline())) {
		var resp=response.split(/\s+/);
		if(resp[1]=='433') {
			mylog(response);
			/* Nick in use... */
			nick+='_';
			mylog("Using nick: " + nick);
			my_server.send("NICK " + nick + "\r\n");
		}
		if(resp[1]=='422' || resp[1]=='376')
			done=1;
		//mylog(response);
	}
	if(!my_server.is_connected) {
		alert("Disconnected");
		exit();
	}
}

if(join) {
	mylog("Joining: " + channel);
	my_server.send("JOIN " + channel + "\r\n");
	while(my_server.poll(5) && (response=my_server.recvline()))
		mylog(response);
}

var delay=0;
if(msg)
	send(msg, delay);
else while((msg=readln())!=undefined) {	/* read from stdin */
	var send_count = 0;
	if (send(msg, delay)) {
		send_count++;
		if (send_count > 4)
			delay = 2050;
	}
}

while(my_server.poll(0) && (response=my_server.recvline()))
	mylog(response);

IRC_quit(my_server);
mylog("Exiting");
exit();

function send(msg, delay)
{
	if(msg==undefined || msg.search(/^[\r\n]*$/)!=-1) {
		mylog("Not sending blank message");
		return false;
	}
	for(i in exclude)
		if(msg.search(exclude[i])>=0)
			return false;
	if (delay)
		mswait(delay);	// Cyan: IRCd throttles clients that send text to the server too quickly
	mylog("Sending: " + msg);
	if(!my_server.send("PRIVMSG "+channel+" :"+expand_tabs(msg)+"\r\n")) {
		alert("send failure");
		return false;
	}
	return true;
}

function expand_tabs(msg)
{
	if(msg!=undefined) {
		var s=msg;
		do {
			msg=s;
			s=msg.replace(/\t/,
				function(str,offset,s) {
					return('        '.substr(0,8 - offset % 8));
			});
		} while(s!=msg);
	}
	return(msg);
}
