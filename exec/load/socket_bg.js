// $Id$
/* 
	Javascript Modular Chat Engine
	by Matt Johnson (MCMLXXIX) - 2010
	
	NOTE: Do not load this file directly.
	This script is meant to be loaded in
	the background, preferrably by chatclient.js
	or another script that can interact properly 
*/
	
load("nodedefs.js");
var node=argv[0];
var host=argv[1];
var port=argv[2];

if(	isNaN(node) ||
	host==undefined ||
	isNaN(port)) {
	log(LOG_ERROR,"invalid client arguments");
	exit();
}

const CONNECTION_TIMEOUT=	5;
const MAX_RECV=				2048;

var sock=false;
var disabled=false;
var outbound=[];
var parent=new Queue(format("%s_%d_%d",host,port,node));

function main()
{
	try {
		while(system.node_list[node-1].status == NODE_INUSE ||
				system.node_list[node-1].status == NODE_QUIET) {
			cycle();
			mswait(10);
		}
	}
	catch(e) {
		log("ERROR: " + e);
	}

	disconnect();
	exit();
}
function connect()
{
	parent.write("connecting","sync");
	sock=new Socket();
	sock.connect(host,port,CONNECTION_TIMEOUT);
	if(!sock.is_connected) {
		parent.write("failed","sync");
		sock=false;
	}
	else {
		parent.write("connected","sync");
	}
}
function cycle()
{
	/* read any parent script control requests */
	var syncdata=parent.read("sync");
	if(syncdata) {
		//log("parent<--" + syncdata);
		sync(syncdata);
	}
		
	/* read any outbound chat data from the parent script */
	var outdata=parent.read("data");
	if(outdata) {
		//log("parent<--" + outdata);
		outbound.push(outdata);
	}
		
	/* 	if chat has been disabled, 
		do not run socket events */
	if(disabled) 
		return;
	
	/* if connection has not been initialized */
	if(!sock) 
		return;
	
	/* if active socket connection is broken */
	if(!sock.is_connected) {
		sock=false;
		parent.write("disconnected","sync");
		return;
	}
	
	/* if socket connection is established */
	if(sock.data_waiting) {
		var raw_data=sock.recvline(MAX_RECV);
		if(raw_data != null) {
			log(LOG_DEBUG,"sock<--" + raw_data);
			parent.write(raw_data,"data");
		}
	}
	
	/* send outbound socket data */
	var data=outbound.shift();
	if(data) {
		if(sock.write(data)) 
			log(LOG_DEBUG,"sock-->" + data);
		else 
			outbound.unshift(data);
	}
}
function sync(cmd)
{
	switch(cmd.toLowerCase()) {
	case "connect":
		if(!sock.is_connected) {
			disabled=false;
			connect();
		}
		break;
	case "disconnect":
		disabled=true;
		disconnect();		
		break;
	}
}
function disconnect()
{
	if(sock.is_connected) {
		log(LOG_DEBUG,"terminating client connection");
		while(outbound.length > 0) 
			sock.write(outbound.shift());
		sock.close();
	}
	sock=false;
}

main();
