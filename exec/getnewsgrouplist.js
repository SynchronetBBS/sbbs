// $Id: getnewsgrouplist.js,v 1.1 2019/12/01 03:02:31 rswindell Exp $

"use strict";

if(argc < 3) {
	print("usage: " + js.exec_file + " <user> <password> <host> [port] [pattern]");
	exit(0);
}

var socket = new Socket();
var name = argv[0];
var pass = argv[1];
var host = argv[2];
var port = argv[3];
var pattern = argv[4];
if(!pattern)
	pattern = "";
if(!port)
	port = 119;
if(!socket.connect(host,port)) {
	printf("!Error %d connecting to %s port %d\r\n"
		,socket.last_error,host,port);
	exit();
}
var resp = socket.recvline();
if(parseInt(resp, 10) != 200) {
	alert("Unexpected response: " + resp);
	exit();
}
socket.sendline("AUTHINFO USER " + name);
resp = socket.recvline();
socket.sendline("AUTHINFO PASS " + pass);
resp = socket.recvline();
if(parseInt(resp, 10) != 281) {
	alert("Unexpected response: " + resp);
	exit();
}
socket.sendline("LIST NEWSGROUPS");
resp = socket.recvline();
if(parseInt(resp, 10) != 215) {
	alert("Unexpected response: " + resp);
	exit();
}
while(socket.is_connected) {
	resp = socket.recvline();
	if(resp === ".")
		break;
	if(resp.match(pattern))
		print(resp);
}
if(socket.is_conected)
	socket.sendline("QUIT");
