//load("commsync.js");
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
		chat.js
		funclib.js
		commsync.js
	
	Add to "/sbbs/ctrl" directory:
		commservice.ini
		commsync.ini
	
	Add to "/sbbs/ctrl/services.ini" file:

	[Commserv]
	Port=10088
	MaxClients=20
	Options=STATIC
	Command=commservice.js MDJ.ATH.CX 10088
	
*/

const normal_scope=	"#";
const global_scope=	"!";
const node_prefix=		"*";
const local_prefix=	"&";

const hub_address=argv[0];
const hub_port=argv[1];
const connection_timeout=2;
const connection_attempts=5;

var local_socks=[];
var remote_socks=[];
var data_queue=[];
var remote_hub=false;

//main service loop
while(!server.terminated) 
{
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
			if(!remote_hub || !remote_hub.is_connected)
				hub_connect();
		}
		else if(hub_address || hub_port)
		{
			log("invalid main hub information");
		}
		inbound();
		outbound();
	}
	else
	{
		if(remote_hub && remote_hub.is_connected)
		{
			hub_disconnect();
		}
	}
}
function inbound()
{
	//read data from each socket
	for(var l in local_socks)
	{
		for(var s=0;s<local_socks[l].length;s++)
		{
			var socket=local_socks[l][s].sock;
			if(socket.is_connected)
			{
				if(socket.data_waiting)
				{
					socket_receive(socket);
				}
			}
			else
			{
				s=delete_local_session(l,s);
			}
		}
	}
	for(var r=0;r<remote_socks.length;r++)
	{
		var node=remote_socks[r];
		if(node.sock.is_connected)
		{
			socket_receive(node.sock);
		}
		else
		{
			r=delete_remote_session(r);
		}
	}
	if(remote_hub && remote_hub.is_connected)
	{
		socket_receive(remote_hub);
	}
}
function outbound()
{
	//send all data to hub and to appropriate local socket group
	for(var d=0;d<data_queue.length;d++)
	{
		var data=data_queue[d];
		//if the central hub is connected and was not the origin of the data, send thru
		if(remote_hub && remote_hub.is_connected)
		{
			socket_send_remote(data,remote_hub);
		}
		//send data to all remotely connected nodes
		for(var r=0;r<remote_socks.length;r++)
		{
			var socket=remote_socks[r].sock;
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
			case global_scope:
				for(var l in local_socks)
				{
					for(var s=0;s<local_socks[l].length;s++)
					{
						var socket=local_socks[l][s].sock;
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
			case normal_scope:
			default:
				for(var s=0;local_socks[data.session] && s<local_socks[data.session].length;s++)
				{
					var socket=local_socks[data.session][s].sock;
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
	local_socks[session_id].splice(index,1);
	return index-1;
}
function socket_receive(socket)
{
	if(socket.data_waiting)
	{
		//store data in master array to be distributed later
		var raw=socket.recvline(4092,connection_timeout);
		var scope=raw.charAt(0);
		var session_id=raw.substring(1,raw.indexOf(":"));
		var data=raw.substr(raw.indexOf(":")+1);
		
		data_queue.push(new Packet(scope,session_id,socket.descriptor,data));
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
		var d=data.scope + data.session + ":" + data.data + "\r\n";
		socket.write(d);
	}
}
function delete_remote_session(index)
{
	log("remote socket connection terminated: " + remote_socks[index].sock.remote_ip_address);
	remote_socks.splice(index,1);
	return index-1;
}
function hub_connect()
{
	remote_hub=new Socket();
	remote_hub.bind(0,server.interface_ip_address);
	//if a central hub address is provided, attempt connection
	for(var t=0;t<connection_attempts;t++)
	{
		if(remote_hub.connect(hub_address,hub_port,connection_timeout)) 
		{
			//send node identifier and bbs name so hub knows this is a distribution point
			remote_hub.send(node_prefix + system.name + "\r\n");
			
			log("connection to " + hub_address + " successful");
			return true;
		}
		mswait(50);
	}
	log("connection to " + hub_address + " failed with error " + remote_hub.error);
	return false;
}
function hub_disconnect()
{
	log("disconnecting from main hub: " + hub_address);
	remote_hub.close();
}
function store_socket(sock)
{
	//receive connection identifier from incoming socket connection (should always be first transmission)
	var handshake=sock.recvline(4092,connection_timeout);
	var identifier=handshake.charAt(0);
	var session_id=handshake.substr(1);
	
	switch(identifier)
	{
		case node_prefix:
			remote_socks.push(new Node(sock,session_id));
			log("remote node connection from: " + session_id + "@" + sock.remote_ip_address);
			break;
		case local_prefix:
			if(!local_socks[session_id]) local_socks[session_id]=[];
			local_socks[session_id].push(new Session(sock));
			log("local node connection: " + session_id);
			break;
		default:
			log("unknown connection type");
			sock.close();
			break;
	}
}
function count_local_sockets()
{
	var count=0;
	for(var l in local_socks)
	{
		if(local_socks[l].length) count+=local_socks[l].length;
	}
	return count;
}
function count_remote_sockets()
{
	return remote_socks.length;
}

//TODO
function find_user()
{
}
function page_user()
{
}

function Packet(scope,session,descriptor,data)
{
	this.session=session;
	this.descriptor=descriptor;
	this.data=data;
	this.scope=scope;
}
function Node(socket,name)
{
	this.sock=socket;
	this.name=name;
}
function Session(socket,session)
{
	this.sock=socket;
	this.data=[];
}
