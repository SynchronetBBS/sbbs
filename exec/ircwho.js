/* ircmsg.js */

/* $Id$ */

load("irclib.js");	// Thanks Cyan!

const REVISION = "$Revision$".split(' ')[1];

var server="vert.synchro.net";
var channel="#synchronet";
var port=6667;
var nick="sw";
var msg;
var join=false;
var passedmsg=0;

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
	}
}

my_server = IRC_client_connect(server,nick,undefined,undefined,port);
if (!my_server) {
        log("!Couldn't connect to " + server);
        exit();
}

var done=0;
while(!done) {
	while(!done && (response=my_server.recvline())) {
		var resp=response.split(/\s+/);
		if(resp[1]=='433') {
			/* Nick in use... */
			nick+='_';
			my_server.send("NICK " + nick + "\r\n");
			
		}
		if(resp[1]=='422' || resp[1]=='376')
			done=1;
	}
}

if(join) {
	my_server.send("JOIN " + channel + "\r\n");
	while(my_server.poll(5) && (response=my_server.recvline()));
}

send("NAMES "+channel);

while(my_server.poll(5) && (response=my_server.recvline())) {
	var resp=response.split(/\s+/);
	if(resp[1]=='353')
		logusers(response);
	if(resp[1]=='366')
		break;
}

IRC_quit(my_server);

function send(msg)
{
	if(!my_server.send(msg+"\r\n"))
		alert("send failure");
}

function logusers(resp)
{
	var txt=resp.split(/:/,3);
	var names=txt[2].split(/\s+/);
	for(name in names.sort(sortfunc))
		log(names[name]);
}

function sortfunc(a, b)
{
	if(a.toUpperCase() < b.toUpperCase())
		return -1;
	if(a.toUpperCase() > b.toUpperCase())
		return 1;
	return 0;
}
