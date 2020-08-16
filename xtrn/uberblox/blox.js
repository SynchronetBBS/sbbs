/*
	UBER BLOX!

    $Id: blox.js,v 1.14 2012/12/24 11:46:29 rswindell Exp $

	A javascript block puzzle game similar to GameHouse "Super Collapse" 
	by Matt Johnson (2009)

	for Synchronet v3.15+
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