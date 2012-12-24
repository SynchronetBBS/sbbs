/*
	BUBBLE BOGGLE 
	for Synchronet v3.15+ (javascript)
	by Matt Johnson (2009)

    $Id$
	
	Game uses standard "Big Boggle" rules, scoring, and dice
	Dictionary files created from ENABLe gaming dictionary
	
	for customization or installation help contact:
	Matt Johnson ( MCMLXXIX@BBS.THEBROKENBUBBLE.COM )
*/

load("json-client.js");
var root = js.exec_dir;


var server_file = new File(file_cfgname(root, "server.ini"));
server_file.open('r',true);

//var autoUpdate=server_file.iniGetValue(null,"autoUpdate");
var serverAddr=server_file.iniGetValue(null,"host","localhost");
var serverPort=server_file.iniGetValue(null,"port",10088);
server_file.close();

var client=new JSONClient(serverAddr,serverPort);

/*
if(autoUpdate) {
	console.putmsg("\r\n\1nChecking for updates...");
}
*/

load(root + "game.js");