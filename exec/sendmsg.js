// sendmsg.js

// Send a short message (telegram) to a user on another Synchronet system
// Requires v3.10f or later

if(argc>0 && argv[0].indexOf('@')!=-1)
	dest = argv[0];
else if((dest = prompt("User (user@hostname)"))==null)
	exit();
if((hp = dest.indexOf('@'))==-1) {
	alert("Invalid user");
	exit();
}
if((msg = prompt("Message"))==null)
	exit();
host = dest.slice(hp+1);
sock = new Socket();
//sock.debug = true;
do {
	if(!sock.connect(host,25)) {
		alert("Connection to " + host + " failed with error " + sock.last_error);
		break;
	}

	if(Number((rsp=sock.recvline()).slice(0,3))!=220) {
		alert("Invalid connection response:\r\n" + rsp);
		break;
	}
	sock.send("HELO "+system.inetaddr+"\r\n");
	if(Number((rsp=sock.recvline()).slice(0,3))!=250) {
		alert("Invalid HELO response: " + rsp);
		break;
	}
	sock.send("SOML FROM: "+user.email+"\r\n");
	if(Number((rsp=sock.recvline()).slice(0,3))!=250) {
		alert("Invalid SOML response: " + rsp);
		break;
	}
	sock.send("RCPT TO: "+dest+"\r\n");
	if(Number((rsp=sock.recvline()).slice(0,3))!=250) {
		alert("Invalid RCPT TO response: " + rsp);
		break;
	}
	sock.send("DATA\r\n");
	if(Number((rsp=sock.recvline()).slice(0,3))!=354) {
		alert("Invalid DATA response: " + rsp);
		break;
	}
	sock.send(msg);
	sock.send("\r\n.\r\n");
	if(Number((rsp=sock.recvline()).slice(0,3))!=250) {
		alert("Invalid end of message response: " + rsp);
		break;
	}
	sock.send("QUIT\r\n");
	print("Message delivered successfully.");

} while(0);

sock.close();
