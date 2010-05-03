/*
	Inter-BBS/Inter-Node client (for use with commservice.js)
	for Synchronet v3.15+
	by Matt Johnson - 2009
*/

load("funclib.js");
load("sbbsdefs.js");
load("sockdefs.js");

function ServiceConnection(id,alias)
{
	const QUERY=				"?";
	const LOCAL=				"&";
	const FILESYNC=			"@";
	const connection_timeout=	5;
	const connection_attempts=	2;
	const connection_interval=	5;

	this.id=				(id?id:"default");
	this.notices=			[];
	this.sock=				false;
	var attempts=0;
	var last_attempt=0;
	
	//hub should point to the local, internal ip address or domain of the computer running commservice.js
	//port should point to the port the gaming service is set to use in commservice.js
	const hub=					"localhost";
	const port=				10088;
	
	this.init=function()
	{
		if((time()-last_attempt)<connection_interval || attempts>=connection_attempts) {
			debug("error connecting to hub, exiting program",LOG_WARNING);
			exit();
		}
		this.sock=new Socket();
		this.sock.connect(hub,port,connection_timeout);
		if(testSocket(this.sock)) {
			var hello=new Object();
			hello.context=LOCAL;
			hello.alias=alias?alias:"unknown";
			this.send(hello);
			attempts=0;
			last_attempt=0;
			return true;
		} else {
			attempts++;
			debug("error connecting to hub, attempt " + attempts,LOG_WARNING);
			last_attempt=time();
			return false;
		}
	}
	this.getNotices=function()
	{
		var notices=[];
		while(this.notices.length) {
			notices.push(this.notices.shift());
		}
		return notices;
	}
	this.receive=function()
	{
		if(!testSocket(this.sock))
		{
			if(!this.init()) return false;
		}
		if(this.sock.data_waiting)
		{
			var raw_data=this.sock.recvline(1024,connection_timeout);
			if(raw_data != null) return(js.eval(raw_data));
			return false;
		}
	}
	this.send=function(data)
	{
		if(!testSocket(this.sock))
		{
			if(!this.init()) return false;
		}
		if(!data.id) data.id=this.id;
		if(!data.system) data.system=system.name;
		
		this.sock.send(data.toSource() + "\r\n");
		return true;
	}
	this.recvfile=function(files,blocking)
	{
		this.filesync(files,"recv",blocking);
	}
	this.sendfile=function(files,blocking)
	{
		this.filesync(files,"send",blocking);
	}
	this.filesync=function(filemask,command,blocking)
	{
		var q=new Object();
		q.filemask=filemask;
		q.command=command;
		q.blocking=blocking;
		q.context=FILESYNC;
		
		this.send(q);
		/*
		if(blocking) {
			var response=this.receive();
		}
		*/
	}
	this.close=function()
	{
		debug("terminating service connection",LOG_DEBUG);
		this.sock.close();
	}

	if(!this.init() && attempts>=connection_attempts) return false;
}

	
