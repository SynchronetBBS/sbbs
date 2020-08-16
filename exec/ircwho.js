/* ircmsg.js */

/* $Id: ircwho.js,v 1.4 2016/05/13 01:33:20 deuce Exp $ */

if(this.IRCLIB_REVISION==undefined)
	load("irclib.js");	// Thanks Cyan!

var ircserver="vert.synchro.net";
var channel="#synchronet";
var port=6667;
var nick="sw";
var msg;
var join=false;
var passedmsg=0;

for(i=0;i<argc;i++) {
	switch(argv[i]) {
		case "-s":
			ircserver=argv[++i];
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

my_server = IRC_client_connect(ircserver,nick,undefined,undefined,port);
if (!my_server) {
        wrlog("!Couldn't connect to " + ircserver);
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
var sent_heads=0;

while(my_server.poll(5) && (response=my_server.recvline())) {
	var resp=response.split(/\s+/);
	if(resp[1]=='353') {
		if(this.http_request!=undefined) {
			if(!sent_heads) {
				writeln("<html><head><title>Users on "+channel+" now</title></head><body>");
				sent_heads=1;
			}
		}
		logusers(response);
	}
	if(resp[1]=='366')
		break;
}

IRC_quit(my_server);

if(!sent_heads && this.http_request!=undefined)
	writeln("</body></html>");

function send(msg)
{
	if(my_server.send(msg+"\r\n") != msg.length + 2)
		alert("send failure");
}

function logusers(resp)
{
	var txt=resp.split(/:/,3);
	var names=txt[2].split(/\s+/);
	for(name in names.sort(sortfunc)) {
		writeln(names[name]);
		if(this.http_request!=undefined)
			writeln("<BR>");
	}
}

function sortfunc(a, b)
{
	if(a.toUpperCase() < b.toUpperCase())
		return -1;
	if(a.toUpperCase() > b.toUpperCase())
		return 1;
	return 0;
}

function wrlog(msg)
{
	log(msg);
	writeln(msg);
	if(http_request!=undefined)
		writeln("<BR>");
}
