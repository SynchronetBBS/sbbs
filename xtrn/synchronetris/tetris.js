/*
	JAVASCRIPT TETRIS INTERBBS
	For Synchronet v3.15+
	Matt Johnson(2009)
*/
//$Id: tetris.js,v 1.11 2012/12/24 11:45:59 rswindell Exp $
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
}
*/

load(root + "lobby.js");