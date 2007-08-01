/* ircmsg.js */

/* Send a message (from stdin or the command-line) to an IRC channel */

/* $Id$ */

load("irclib.js");	// Thanks Cyan!

const REVISION = "$Revision$".split(' ')[1];

var server="irc.synchro.net";
var channel="#channel";
var port=6667;
var nick="nick";
var msg;
var join=false;
var exclude=new Array();

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

log(LOG_INFO,"Using nick: " + nick);
log(LOG_INFO,"Connecting to: " +server+ " port " + port);
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
			log(response);
			/* Nick in use... */
			nick+='_';
			log(LOG_INFO,"Using nick: " + nick);
			my_server.send("NICK " + nick + "\r\n");
		}
		if(resp[1]=='422' || resp[1]=='376')
			done=1;
		//log(response);
	}
	if(!my_server.is_connected) {
		alert("Disconnected");
		exit();
	}
}

if(join) {
	log("Joining: " + channel);
	my_server.send("JOIN " + channel + "\r\n");
	while(my_server.poll(5) && (response=my_server.recvline()))
		log(response);
}

if(msg)
	send(msg);
else while((msg=readln())!=undefined)	/* read from stdin */
	send(msg);

while(my_server.poll(0) && (response=my_server.recvline()))
	log(response);

IRC_quit(my_server);
log("Exiting");
exit();

function send(msg)
{
	if(msg==undefined || msg.search(/^[\r\n]*$/)!=-1) {
		log("Not sending blank message");
		return;
	}
	for(i in exclude)
		if(msg.search(exclude[i])>=0)
			return;
	log("Sending: " + msg);
	if(!my_server.send("PRIVMSG "+channel+" :"+expand_tabs(msg)+"\r\n"))
		alert("send failure");
	else
		mswait(2000);	// Cyan: IRCd throttles clients that send text to the server too quickly
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
