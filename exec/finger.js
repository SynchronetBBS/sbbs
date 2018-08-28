// finger.js

// A simple finger client

load('sockdefs.js');
var dest;
var udp = false;
var protocol = "finger";

var i;
for(i = 0; i < argc; i++) {
	if(argv[i] == '-udp')
		use_udp = true;
	else if(argv[i] == '-s')
		protocol = "systat";
	else if(argv[i].indexOf('@')!=-1)
		dest = argv[i];
	else {
		alert("Unsupported option: " + argv[i]);
		exit();
	}
}

if(!dest && (dest = prompt("User (user@hostname)"))==null)
	exit();

if((hp = dest.indexOf('@'))==-1) {
        dest += "@" + system.host_name;
	hp = dest.indexOf('@')
}

host = dest.slice(hp+1);
sock = new Socket(use_udp ? SOCK_DGRAM : SOCK_STREAM);
//sock.debug = true;
if(!sock.connect(host, protocol)) 
	alert("Connection to " + host + " failed with error " + sock.last_error);
else {
	sock.send(dest.slice(0,hp)+"\r\n");
	if(use_udp)
		print(sock.recvfrom().data);
	else while(sock.is_connected && !js.terminated)
		print(sock.readline());
}
sock.close();
