//$Id$
/*
	Inter-BBS/Inter-Node socket service
	for Synchronet v3.15+ 
	by Matt Johnson - 2009
	Allows real-time gaming/chatting between LOCAL bbs nodes and REMOTE bbs systems

	The following files can be located in the Synchronet CVS repository:
	
	Add to "/sbbs/exec/" directory:
		commservice.js
	
	Add to "/sbbs/exec/load/" directory:
		commclient.js
		chateng.js
		funclib.js
		FILESYNC.js
	
	Add to "/sbbs/ctrl" directory:
		FILESYNC.ini
	
	Add to "/sbbs/ctrl/services.ini" file:

	[Commserv]
	Port=10088
	MaxClients=20
	Options=STATIC
	Command=commservice.js thebrokenbubble.com 10088
	
*/
load("funclib.js");

const version=			"$Revision$";
//Connection type constants
const REMOTE=			"*";
const LOCAL=			"&";
const FILESYNC=		"@";
const QUERY=			"?";

const hub_address=			argv[0];
const hub_port=			argv[1];
const connection_timeout=	5;//SECONDS
const connection_interval=	10;
const connection_attempts=	10;

var local_sessions=[];
var remote_sessions=[];
var data_queue=[];
var hub=new Server(hub_address,hub_port);

//server.socket.debug = false;		
//server.socket.nonblocking = true;

//main service loop
while(!js.terminated) 
{
	try {
		sock_cycle();
		if(!server.socket.poll(0.1))
		{
			if(js.terminated) break;
			continue;
		}
		store_socket(server.socket.accept());
	} catch(e) {
		debug(e,LOG_WARNING);
	}
}
if(hub.enabled) hub.disconnect();

function sock_cycle()
{
	if(sessions_active()) {
		if(hub.enabled) hub.cycle();
		inbound();
		outbound();
		//send_updates();
	} else {
		if(hub.enabled) hub.disconnect();
	}
}
function inbound()
{
	//read data from each socket
	for(var l in local_sessions) {
		for(var s=0;s<local_sessions[l].length;) {
			var session=local_sessions[l][s];
			if(!session.receive()) delete_session(local_sessions[l],s);
			else s++;
		}
	}
	for(var r=0;r<remote_sessions.length;) {
		var session=remote_sessions[r];
		if(!session.receive()) delete_session(remote_sessions,r);
		else r++;
	}
}
function outbound()
{ 
	//send all data to hub and to appropriate LOCAL socket group
	while(data_queue.length) {
		var data=data_queue.shift();
		if(hub.enabled) hub.send(data);
		for(var r=0;r<remote_sessions.length;)	{
			if(!remote_sessions[r].send(data)) delete_session(remote_sessions,r);
			else r++;
		}
		for(var s=0;local_sessions[data.id] && s<local_sessions[data.id].length;)	{
			if(!local_sessions[data.id][s].send(data)) delete_session(local_sessions[data.id],s);
			else s++;
		}
	}
}
function parse_inbound(socket)
{
	if(!testSocket(socket)) {
		debug("socket test failed",LOG_WARNING);
		return false;
	}
	var incoming=socket.recvline(1024,connection_timeout);
	if(incoming != null) {
		return js.eval(incoming);
	} else	return false;
}
function socket_send(data,socket)
{
	if(!testSocket(socket)) return false;
	if(data.descriptor != socket.descriptor) {
		var d=data.toSource() + "\r\n";
		socket.write(d);
	}
	return true;
}
function delete_session(array,index)
{
	log("session terminated: " + array[index].address);
	server.client_remove(array[index].sock);
	array.splice(index,1);
	log("clients: " + server.clients);
}
function store_socket(sock)
{
	debug("connection from " + sock.remote_ip_address,LOG_DEBUG);
	//receive connection identifier from incoming socket connection (should always be first transmission)
	var data=parse_inbound(sock);
	if(!data) {
		debug("timed out waiting for handshake",LOG_WARNING);
		sock.write("please visit http://cvs.synchro.net and verify that your files are up to date\r\n");
		sock.close();
		return false;
	}
	if(!data.context) {
		debug("invalid handshake",LOG_WARNING);
		sock.write("please visit http://cvs.synchro.net and verify that your files are up to date\r\n");
		sock.close();
		return false;
	}
	switch(data.context)
	{
		case REMOTE:
			add_remote_session(sock,data);
			break;
		case LOCAL:
			add_local_session(sock,data);
			break;
		case FILESYNC:
			handle_filesync(sock,data);
			break;
		case QUERY:
			handle_query(sock,data);
			break;
		default:
			debug("unknown connection type: " + data.context,LOG_WARNING);
			sock.close();
			return false;
	}
	return true;
}
function add_remote_session(sock,data)
{
	if(!data.system) {
		debug("invalid handshake",LOG_WARNING);
		sock.write("please visit http://cvs.synchro.net and verify that your files are up to date\r\n");
		sock.close();
		return false;
	}
	var response=new Object();
	response.version=version;
	
	if(data.version != version) {
		debug("incompatible with REMOTE server version: " + data.version,LOG_WARNING);
		response.response="UPDATE";
		socket_send(response,sock);
		sock.close();
		return false;
	} 
	
	log("REMOTE connection: " + data.system + "(" + sock.remote_ip_address + ")");
	response.response="OK";
	socket_send(response,sock);
	remote_sessions.push(new Session(sock,data));
	server.client_add(sock);
	log("clients: " + server.clients);
	return true;
}
function add_local_session(sock,data)
{
	if(!data.alias || !data.id) {
		debug("invalid handshake",LOG_WARNING);
		debug(data,LOG_DEBUG);
		sock.write("please visit http://cvs.synchro.net and verify that your files are up to date\r\n");
		sock.close();
		return false;
	}
	log("LOCAL connection: " + data.alias);
	if(!local_sessions[data.id]) local_sessions[data.id]=[];
	local_sessions[data.id].push(new Session(sock,data));
	server.client_add(sock);
	log("clients: " + server.clients);
	return true;
}
function sessions_active() 
{
	if(remote_sessions.length) return true;
	for(var l in local_sessions) {
		if(local_sessions[l].length) return true;
	}
	return false;
}
function count_local_sockets()
{
	var count=0;
	for(var l in local_sessions)
	{
		if(local_sessions[l].length) count+=local_sessions[l].length;
	}
	return count;
}
function count_remote_sockets()
{
	return remote_sessions.length;
}
function handle_query(socket,query)
{
	switch(query.task)
	{
		case "who":
			whos_online(socket,query);
			break;
		case "finduser":
			find_user(socket,query);
			break;
		case "pageuser":
			page_user(socket,query);
			break;
		case "adduser":
			add_user(socket,query);
			break;
		case "removeuser":
			remove_user(socket,query);
			break;
		default:
			debug("unknown query type: " + query.task,LOG_WARNING);
			debug(query,LOG_WARNING);
			break;
	}
}
function handle_filesync(socket,query)
{
	log("file sync request from: " + socket.remote_ip_address);
	//background.push(load(true,"filesync.js",socket.descriptor,query.toSource(),hub.address,hub.port));
}
function send_updates()
{
	if(background.length)
	{
		var bgq=background.shift();
		if(bgq.data_waiting)
		{
			var data=bgq.read();
			if(data) {
				for(var r=0;r<remote_sessions.length;r++)
				{
					var session=remote_sessions[r];
					if(data.remote_ip_address != session.address)
					{
						load(true,"filesync.js",session.descriptor,data.toSource(),hub.address,hub.port);
					}
				}
			}
		}
		else background.push(bgq);
	}
}

//TODO
function whos_online(socket)
{
}
function add_user(socket,data)
{
}
function remove_user(socket,data)
{
}
function find_user()
{
}
function page_user()
{
}
function notify_sysop(msg)
{
	system.put_telegram(1,msg);
}
//END TODO

function Session(socket,packet)
{
	this.sock=socket;
	this.context=packet.context;
	this.id=packet.id;
	this.alias=packet.alias;
	this.system=packet.system;
	this.address=socket.remote_ip_address;
	this.port=socket.remote_port;
	this.descriptor=socket.descriptor;
	this.receive=function()
	{
		if(!testSocket(this.sock)) return false;
		if(this.sock.data_waiting)	{
			//store data in master array to be distributed later
			var data=parse_inbound(this.sock);
			if(data) {
				switch(data.context) 
				{
					case FILESYNC:	
						handle_filesync(this.sock,data);
						break;
					case QUERY:
						handle_query(this.sock,data);
						break;
					default:
						data.id=data.id?data.id:this.id;
						data.descriptor=this.descriptor;
						data_queue.push(data);
						break;
				}
			}
		}
		return true;
	}
	this.send=function(data)
	{
		if(!socket_send(data,this.sock)) return false;
		return true;
	}
}
function Server(addr,port)
{
	this.address=addr;
	this.port=port;
	this.sock=false;
	this.queue=[];
	this.enabled=true;
	this.last_attempt=0;
	this.attempts=0;

	this.connect=function()
	{
		debug("connecting to " + this.address + " on port " + this.port,LOG_DEBUG);
		this.sock=new Socket();
		this.sock.nonblocking=true;
		this.sock.connect(this.address,this.port,connection_timeout);
		if(this.sock.is_connected) 
		{
			//send node identifier and bbs name so hub knows this is a distribution point
			var hello=new Object();
			hello.system=system.name;
			hello.context=REMOTE;
			hello.version=version;
			socket_send(hello,this.sock);
			var response=parse_inbound(this.sock);
			if(response) {
				switch(response.response)
				{
					case "OK":
						debug("connection to " + this.address + " successful",LOG_DEBUG);
						debug("REMOTE server " + response.version,LOG_DEBUG);
						this.attempts=0;
						this.last_attempt=0;
						return true;
					case "UPDATE":
						var msg="service file out of date, please visit http://cvs.synchro.net/ to update to " + response.version;
						debug(msg,LOG_WARNING);
						notify_sysop(msg);
						this.sock.close();
						this.enabled=false;
						return false;
					default:
						debug("unknown response",LOG_WARNING);
						debug(response,LOG_WARNING);
						this.sock.close();
						break;
		 		}
			}
			else {
				debug("no hub response received",LOG_DEBUG);
				this.sock.close();
			}
		} else {
			debug("connection to " + this.address + " failed with error " + this.sock.error,LOG_WARNING);
		}
		this.attempts++;
		this.last_attempt=time();
		return false;
	}
	this.disconnect=function()
	{
		if(this.sock.is_connected) {
			log("disconnecting from server: " + this.address);
			while(this.queue.length) {
				var data=this.queue.shift();
				socket_send(data,this.sock);
			}
			this.sock.close();
		}
	}
	this.cycle=function()
	{
		if(!this.sock.is_connected) {
			if(this.attempts>=connection_attempts) {
				this.enabled=false;
				return false;
			}
			if((time()-this.last_attempt)>=connection_interval) {
				this.connect();
			}
		}
		if(this.sock.is_connected) {
			if(this.queue.length) {
				var data=this.queue.shift();
				socket_send(data,this.sock);
			}
			this.receive();
		}
	}
	this.receive=function()
	{
		if(!testSocket(this.sock)) return false;
		if(this.sock.data_waiting)	{
			//store data in master array to be distributed later
			var data=parse_inbound(this.sock);
			if(data) {
				data.descriptor=this.sock.descriptor;
				data_queue.push(data);
			}
		}
		return true;
	}
	this.send=function(data)
	{
		if(this.sock && (data.descriptor != this.sock.descriptor)) this.queue.push(data);
	}
	
	if(!addr && !port) {
		this.enabled=false;
	}
	else if(!addr || !port) {
		debug("invalid hub address parameters",LOG_DEBUG);
		this.enabled=false;
	}
}
