/*
	Inter-BBS/Inter-Node client (for use with commservice.js)
	for Synchronet v3.15+
	by Matt Johnson - 2009
*/

load("funclib.js");
load("synchronet-json.js");
load("sbbsdefs.js");
load("sockdefs.js");

function ServiceConnection(id)
{
	const CONNECTION_TIMEOUT=	5;
	const CONNECTION_ATTEMPTS=	2;
	const CONNECTION_INTERVAL=	5;
	const MAX_RECV=			102400;
	
	var services=new File(system.ctrl_dir + "services.ini");
	services.open('r',true);
	//hub should point to the local, internal ip address or domain of the computer running commservice.js
	const hub=					system.local_host_name;
	//port should point to the port the gaming service is set to use in commservice.js
	const port=				services.iniGetValue("CommServ","Port");
	services.close();

	this.id=				(id?id:"DEFAULT");
	this.queue=				[];
	this.notices=			[];
	this.sock=				false;
	this.nick=				false;
	this.registered=		false;
	
	var attempts=0;
	var last_attempt=0;
	
	this.connect=function()
	{
		if((time()-last_attempt)<CONNECTION_INTERVAL) return false;
		if(attempts>=CONNECTION_ATTEMPTS) {
			debug("error connecting to hub, exiting program",LOG_WARNING);
			exit();
		}
		
		this.sock=new Socket();
		this.sock.nonblocking=true;
		this.sock.connect(hub,port,CONNECTION_TIMEOUT);
		
		if(!this.sock.is_connected) {
			attempts++;
			debug("error connecting to hub, attempt " + attempts,LOG_WARNING);
			last_attempt=time();
			this.registered=false;
			return false;
		}

		attempts=0;
		last_attempt=0;
		return true;
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
		if(!this.sock.is_connected)	{
			if(!this.connect()) return false;
		}
		if(this.sock.is_connected && this.sock.data_waiting) {
			var raw_data=this.sock.recvline(MAX_RECV,CONNECTION_TIMEOUT);
			if(raw_data == null) return false;
			try {
				var data=JSON.parse(raw_data);
				return data;
			} catch(e) {
				debug("error parsing JSON data: " + raw_data,LOG_ERROR);
				return false;
			}
		}
	}
	this.register=function()
	{
		debug("registering client");
		if(!this.nick) {
			if(user.alias) this.nick=new Nick(user.alias,user.name,system.inet_addr,this.id);
			else this.nick=new Nick("SYSTEM","SYSTEM",system.inet_addr,this.id);
		} 
		this.queue.push(JSON.stringify(this.nick)+"\r\n");
		this.registered=true;
	}
	this.send=function(data)
	{
		if(!this.sock.is_connected) {
			if(!this.connect()) return false;
		}
		if(this.sock.is_connected && !this.registered) {
			this.register();
		}
		
		data.id=this.id;
		data=JSON.stringify(data)+"\r\n";
		this.queue.push(data);
		
		while(this.queue.length && this.sock.is_connected) {
			this.sock.write(this.queue.shift());
		}
		return true;
	}
	this.recvfile=function(filemask,blocking)
	{
		this.filesync(filemask,"TRYSEND",blocking);
	}
	this.sendfile=function(filemask,blocking)
	{
		this.filesync(filemask,"TRYRECV",blocking);
	}
	this.filesync=function(filemask,command,blocking)
	{
		var q=new Object();
		q.filemask=filemask;
		q.command=command;
		q.blocking=blocking;
		q.func="FILESYNC";
		
		this.send(q);
		if(q.blocking) {
			var timeout=10;
			var start=time();
			while(time()-start<timeout) {
				var data=this.receive();
				if(!data) {
					continue;
				}
				if(data.filemask==q.filemask) {
					log("file received");
					return true;
				}
			}
			log("file request timed out");
			return false;
		}
		return true;
	}
	this.close=function()
	{
		debug("terminating service connection",LOG_DEBUG);
		while(this.queue.length) {
			this.sock.write(this.queue.shift());
		}
		this.sock.close();
	}

	if(!this.connect() && attempts>=CONNECTION_ATTEMPTS) return false;
}
function Nick(nick,username,host,id)
{
	this.context="CLIENT";
	this.id=id;
	this.func="NICK";
	this.nickname=nick;
	this.hops=1;
	this.created=time();
	this.username=nick;
	this.realname=username;
	this.hostname=host;
	this.servername=host;
}

	
