/*********************************
*		STAR STOCKS (2007) 		*
*		for Synchronet v3.15 		*
*		by Matt Johnson			*
*********************************/

load("json-client.js");
var root = js.exec_dir;

var server_fname = file_cfgname(root, "server.ini");
if(!file_exists(server_fname)) {
	throw("server initialization file missing");
}

var server_file = new File(server_fname);
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