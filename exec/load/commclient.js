/*
	Inter-BBS/Inter-Node client (for use with commservice.js)
	for Synchronet v3.15+
	by Matt Johnson - 2009
*/

load("funclib.js");
load("sbbsdefs.js");
load("sockdefs.js");

const normal_scope=	"#";
const global_scope=	"!";
const priv_scope=		"%";

function GameConnection(id)
{
	this.session_id=			(id?id:"default");

	//hub should point to the local, internal ip address or domain of the computer running gameservice.js
	//port should point to the port the gaming service is set to use in gameservice.js
	const hub=					"localhost";
	const port=				10088;
	const tries=				5;
	const connection_timeout=	2;
	
	var sock=new Socket();
	sock.bind(0,server.interface_ip_address);

	this.init=function()
	{
		log("initializing communication service connection");
		for(var t=0;t<tries;t++)
		{
			sock.connect(hub,port,connection_timeout);
			if(sock.is_connected) 
			{
				sock.send("&" + this.session_id + "\r\n");
				log("connection to " + hub + " successful");
				return true;
			}
			mswait(50);
		}
		log("connection to " + hub + " failed with error " + sock.last_error);
		return false;
	}
	this.receive=function()
	{
		var data=0;
		if(!sock.is_connected)
		{
			log("connecting to hub");
			if(!this.init()) return -1;
		}
		if(sock.data_waiting)
		{
			var raw_data=sock.recvline(4092,connection_timeout);
			data=js.eval(raw_data);
		}
		return data;
	}
	this.send=function(data)
	{
		if(!sock.is_connected)
		{
			log("connecting to hub");
			if(!this.init()) return -1;
		}
		if(!data.scope) 
		{
			log("scope undefined");
			return -1;
		}
		var packet=data.scope + this.session_id + ":" + data.toSource() + "\r\n";
		sock.send(packet);
	}
}

	
