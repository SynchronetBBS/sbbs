/*
	Inter-BBS/Inter-Node socket service
	for Synchronet v3.15+ 
	by Matt Johnson - 2009
	Allows real-time gaming/chatting between local bbs nodes and remote bbs systems

	The following files can be located in the Synchronet CVS repository:
	
	Add to "/sbbs/exec/" directory:
		commservice.js
	
	Add to "/sbbs/exec/load/" directory:
		commclient.js
		chateng.js
		funclib.js
		filesync.js
	
	Add to "/sbbs/ctrl" directory:
		filesync.ini
	
	Add to "/sbbs/ctrl/services.ini" file:

	[Commserv]
	Port=10088
	MaxClients=20
	Options=STATIC
	Command=commservice.js thebrokenbubble.com 10088
	
*/

//Data type constants
const normal=			"#";
const global=			"!";
const priv=				"~";
const query=			"?";

//Connection type constants
const remote=			"*";
const local=			"&";
const filesync=		"@";

const hub_address=argv[0];
const hub_port=argv[1];
const connection_timeout=2;
const connection_attempts=5;

var local_sessions=[];
var remote_sessions=[];
var data_queue=[];
var background=[];
var hub=false;

//main service loop
while(!server.terminated) 
{
	if(server.terminated) break;
	sock_cycle();
	if(server.socket.poll()<1)
	{
		mswait(100);
		continue;
	}
	store_socket(server.socket.accept());
}

function sock_cycle()
{
	if(count_local_sockets()>0 || count_remote_sockets()>0)
	{
		if(hub_address && hub_port)
		{
			if(!hub || !hub.is_connected)
				hub_connect();
		}
		else if(hub_address || hub_port)
		{
			log("invalid main hub information");
		}
		inbound();
		outbound();
		send_updates();
	}
	else
	{
		if(hub && hub.is_connected)
		{
			hub_disconnect();
		}
	}
}
function inbound()
{
	//read data from each socket
	for(var l in local_sessions)
	{
		for(var s=0;s<local_sessions[l].length;s++)
		{
			var session=local_sessions[l][s];
			if(session.sock.is_connected)
			{
				if(session.sock.data_waiting)
				{
					socket_receive(session.sock);
				}
			}
			else
			{
				s=delete_local_session(l,s);
			}
		}
	}
	for(var r=0;r<remote_sessions.length;r++)
	{
		var session=remote_sessions[r];
		if(session.sock.is_connected)
		{
			socket_receive(session.sock);
		}
		else
		{
			r=delete_remote_session(r);
		}
	}
	if(hub && hub.is_connected)
	{
		socket_receive(hub);
	}
}
function outbound()
{
	//send all data to hub and to appropriate local socket group
	for(var d=0;d<data_queue.length;d++)
	{
		var data=data_queue[d];
		//if the central hub is connected and was not the origin of the data, send thru
		if(hub && hub.is_connected)
		{
			socket_send_remote(data,hub);
		}
		//send data to all remotely connected nodes
		for(var r=0;r<remote_sessions.length;r++)
		{
			var socket=remote_sessions[r].sock;
			if(socket.is_connected)
			{
				socket_send_remote(data,socket);
			}
			else
			{
				r=delete_remote_session(r);
			}
		}
		switch(data.scope)
		{
			//if data is meant to be sent to all connected clients, do so (usually chat messages)
			case global:
				for(var l in local_sessions)
				{
					for(var s=0;s<local_sessions[l].length;s++)
					{
						var socket=local_sessions[l][s].sock;
						if(socket.is_connected)
						{
							socket_send_local(data,socket);
						}
						else
						{
							s=delete_local_session(data.session,s);
						}
					}
				}
				break;
			//distribute data to appropriate local sessions
			case normal:
			default:
				for(var s=0;local_sessions[data.session] && s<local_sessions[data.session].length;s++)
				{
					var socket=local_sessions[data.session][s].sock;
					if(socket.is_connected)
					{
						socket_send_local(data,socket);
					}
					else
					{
						s=delete_local_session(data.session,s);
					}
				}
				break;
		}
	}
	if(data_queue.length) data_queue=[];
}
function delete_local_session(session_id,index)
{
	log("local socket connection terminated: " + session_id);
	local_sessions[session_id].splice(index,1);
	return index-1;
}
function socket_receive(socket)
{
	if(socket.data_waiting)
	{
		//store data in master array to be distributed later
		var raw=socket.recvline(16384,connection_timeout);
		var data=raw.split("");
		var scope=data[0].charAt(0);
		var session=data[0].substr(1);
		
		switch(scope)
		{
			case global:
			case normal:
				data_queue.push(new Packet(scope,socket.descriptor,session,data[1]));
				break;
			case query:
				handle_query(socket,data);
				break;
			default:
				log("received unknown data type");
				break;
		}
	}
}
function socket_send_local(data,socket)
{
	if(socket.descriptor!=data.descriptor)
	{
		var d=data.data + "\r\n";
		socket.write(d);
	}
}
function socket_send_remote(data,socket)
{
	if(socket.descriptor!=data.descriptor)
	{
		var d=data.scope + data.session + "" + data.data + "\r\n";
		socket.write(d);
	}
}
function delete_remote_session(index)
{
	log("remote socket connection terminated: " + remote_sessions[index].sock.remote_ip_address);
	remote_sessions.splice(index,1);
	return index-1;
}
function hub_connect()
{
	log("connecting to hub");
	hub=new Socket();
	hub.bind(0,server.interface_ip_address);
	//if a central hub address is provided, attempt connection
	hub.connect(hub_address,hub_port,connection_timeout);
	if(hub.is_connected) 
	{
		//send node identifier and bbs name so hub knows this is a distribution point
		hub.send(remote + system.name + "\r\n");
		log("connection to " + hub_address + " successful");
		return true;
	}
	log("connection to " + hub_address + " failed with error " + hub.error);
	return false;
}
function hub_disconnect()
{
	log("disconnecting from main hub: " + hub_address);
	hub.close();
}
function store_socket(sock)
{
	//receive connection identifier from incoming socket connection (should always be first transmission)
	var handshake=sock.recvline(1024,connection_timeout);
	var identifier=handshake.charAt(0);
	var session_id=handshake.substr(1);
	switch(identifier)
	{
		case remote:
			log("remote connection from: " + sock.remote_ip_address + ":" + session_id);
			remote_sessions.push(new Session(sock,session_id));
			break;
		case local:
			log("local connection from: " + sock.remote_ip_address + ":" + session_id);
			if(!local_sessions[session_id]) local_sessions[session_id]=[];
			local_sessions[session_id].push(new Session(sock,session_id));
			break;
		case filesync:
			log("file sync connection from: " + sock.remote_ip_address + ":" + session_id);
			distribute_files(sock,handshake);
			break;
		default:
			log("unknown connection type: " + identifier);
			sock.close();
			break;
	}
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
	var data=query.split("");
	var session_id=data[0].substr(1);
	var descriptor=data[1];
	var task=		data[2];
	
	switch(task)
	{
		case "#who":
			whos_online(socket);
			break;
		case "#finduser":
			find_user(socket,data);
			break;
		case "#pageuser":
			page_user(socket,data);
			break;
		default:
			log("unknown query type: " + task);
			break;
	}
}
function distribute_files(socket,query)
{
	background.push(load(true,"filesync.js",socket.descriptor,query,hub_address,hub_port));
}
function send_updates(socket)
{
	while(background.length)
	{
		var bgq=background.shift();
		while(bgq.data_waiting)
		{
			var data=bgq.read();
			for(var r=0;r<remote_sessions.length;r++)
			{
				var sock=remote_sessions[r].sock;
				if(data.remote_ip_address != socket.remote_ip_address)
				{
					var query=("@" + data.session_id + "#send" + data.filename + "" + data.filedate);
					load(true,"filesync.js",sock.descriptor,query);
				}
			}
		}
	}
}

//TODO
function whos_online()
{
	var list=[];
	for(var rc=0;rc<count_remote_sessions;rc++)
	{
		var conn=remote_sessions[rc];
		//hmmmmm
	}
	for(var g in local_sessions)
	{
		for(var lc=0;lc<local_sessions[g].length;lc++)
		{
			var conn=local_sessions[g][lc];
			//todo?!
		}
	}
	return list;
}
function find_user()
{
}
function page_user()
{
}

function Packet(scope,descriptor,session,data)
{	
	this.session=session;
	this.descriptor=descriptor;
	this.data=data;
	this.scope=scope;
}
function Session(socket,session)
{
	this.sock=socket;
	this.session=session;
}
