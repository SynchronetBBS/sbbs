/* ircmsg.js */

load("irclib.js");	// Thanks Cyan!

var server="irc.synchro.net";
var channel="#channel";
var port=6667;
var nick="nick";
var msg;

for(i=0;i<argc;i++) {
	switch(argv[i]) {
		case "-s"
			server=argv[++i];
			break;
		case "-c"
			channel=argv[++i];
			break;
		case "-p"
			port=argv[++i];
			break;
		case "-n"
			nick=argv[++i];
			break;
		case "-m"
			msg=argv[++i];
			break;
	}
}

log("Connecting to " +server+ " port " + port);
my_server = IRC_client_connect(server,nick,undefined,undefined,port);
if (!my_server) {
        log("!Couldn't connect to " + server);
        exit();
}
while(my_server.data_waiting && (response=my_server.recvline()))
	log(response);

log("Joining " + channel);
my_server.send("JOIN " + channel + "\r\n");
while(my_server.data_waiting && (response=my_server.recvline()))
	log(response);

while(msg || msg=readln()) {
	my_server.send("PRIVMSG "+channel+" :"+msg+"\r\n");
	delete msg;
}

while(my_server.data_waiting && (response=my_server.recvline()))
	log(response);

IRC_quit(my_server);
