/*
	JAVASCRIPT CHESS 2.0 INTERBBS
	For Synchronet v3.15+
	Matt Johnson(2009)
*/

load("commclient.js");

var root = js.exec_dir;
var stream = false;

var server_file=new File(root + "server.ini");
if(file_exists(server_file.name)) {
	stream=new ServiceConnection("chess");

	server_file.open('r',true);
	var update=server_file.iniGetValue("autoupdate");
	server_file.close();
	
	if(update != true) {
		if((user.compare_ars("SYSOP") || (bbs.sys_status&SS_TMPSYSOP)) 
			&& console.yesno("Check for game updates?")) 
			update=true;
	}
	
	if(update) {
		console.putmsg("\r\n\1nChecking for updates...");
		stream.recvfile("*.bin");
		stream.recvfile("*.js");
		stream.recvfile("interbbs.doc");
		stream.recvfile("sysop.doc",true);
	}
	
	console.putmsg("\r\n\1nSynchronizing data files...");
	stream.recvfile("*.chs");
	stream.recvfile("players.ini",true);
}

load(root + "game.js",stream);