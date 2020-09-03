//$Id: maze.js,v 1.10 2020/06/04 16:57:53 mcmlxxix Exp $
/*
	SYNCHRONET MAZE RACE 
	A Javascript remake 
	of the Atari game "Maze Craze"

	For Synchronet v3.15+
	Matt Johnson(2008)
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
}
*/

load(root + "game.js");