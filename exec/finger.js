// finger.js

// A simple finger client

if(argc>0 && argv[0].indexOf('@')!=-1)
	dest = argv[0];
else if((dest = prompt("User (user@hostname)"))==null)
	exit();
if((hp = dest.indexOf('@'))==-1) {
	alert("Invalid user");
	exit();
}
host = dest.slice(hp+1);
sock = new Socket();
//sock.debug = true;
if(!sock.connect(host,"finger")) 
	alert("Connection to " + host + " failed with error " + sock.last_error);
else {
	sock.send(dest.slice(0,hp)+"\r\n");
	while(bbs.online && sock.is_connected)
		print(sock.readline());
}
sock.close();
