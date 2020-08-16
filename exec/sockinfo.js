// $Id: sockinfo.js,v 1.4 2005/10/14 07:52:39 rswindell Exp $

load("sockdefs.js");

var global=js.global;
if(global==undefined)
	global=this;

var socket;
if(global.client!=undefined)
	socket=client.socket;
else
	socket=new Socket(argv[0]=="udp" ? SOCK_DGRAM : SOCK_STREAM,"test");

var option_list;
if(socket.option_list)
	option_list = socket.option_list; // dynamic list (v3.13b)
else
 	option_list = sockopts; // static list (sockdefs.js)
var opt;
for(opt in option_list)
	print(option_list[opt] +" = "+ socket.getoption(option_list[opt]));