// $Id$

/*
	Javascript Modular Chat Engine 
	by Matt Johnson (MCMLXXIX) 2010
*/

load("nodedefs.js");
load("sbbsdefs.js");
load("synchronet-json.js");

var chat_settings=new ChatSettings();

function ChatSettings()
{
	this.CONNECTION_ATTEMPTS=	5;
	this.CONNECTION_INTERVAL=	10;

	this.ALERT_COLOR=			"\1r";
	this.MOTD_COLOR=			"\1c";
	this.MSG_COLOR=				"\1n";
	this.HIGHLIGHT_COLOR=		"\1b\1h";
	this.TOPIC_COLOR=			"\1y";
	this.SNOTICE_COLOR=			"\1g";
	this.ERROR_COLOR=			"\1r";
}
function ChatEngine(protocol,host,port)
{
	var protocol=protocol;
	var host=host;
	var port=port;
	
	var conn_attempts=0;
	var last_conn_attempt=0;

	this.disabled=false;
	this.connect=true;
	this.connected=false;
	this.connecting=false;
	this.connection_established=false;

	this.registered=false;
	this.registering=false;
	this.channels=[];
	this.nick=false;

	this.init=function()
	{
		/* set queue naming convention */
		var qname=format("%s_%d_%d",host,port,bbs.node_num);
		var qfound=false;
		
		/* scan current named queue list for an existing chat session */
		var qlist=list_named_queues();
		for each(var q in qlist) {
			if(q == qname) {
				qfound=true;
				break;
			}
		}
		/* if there is no existing chat session, load one into background */
		if(!qfound) {
			log(LOG_DEBUG,"initializing background socket");
			load(true,"socket_bg.js",bbs.node_num,host,port);
		}
		/* initialize queue connection for background socket */
		this.queue=new Queue(qname);
		log("name: " + this.queue.name);
		
		/* load protocol information */
		switch(protocol.toUpperCase()) {
		case "IRC":
			load("protocols/irc_protocol.js");
			IRC_init.apply(this);
			break;
		case "AIM":
			break;
		}

		/* load user chat settings */
		//var s_file=new File(system.ctrl_dir + "chat.ini");
		//s_file.open('r',true);
		//s_file.close();
	}
	this.synchronize=function(data)
	{
		switch(data.toLowerCase()) {
		case "connecting":
			this.server_chan.post(chat_settings.SNOTICE_COLOR + "connecting to " + host + "...");
			break;
		case "connected":
			this.reset_counters();
			this.connect=false;
			this.connecting=false;
			this.connected=true;
			this.onConnect();
			break;
		case "failed":
			this.server_chan.post(chat_settings.ERROR_COLOR + "connection failed");
			this.connect=true;
			this.connecting=false;
			break;
		case "disconnected":
			this.server_chan.post(chat_settings.ERROR_COLOR + "disconnected");
			this.reset_connection();
			this.onDisconnect();
			break;
		}
	}
	this.enqueue=function(data,name)
	{
		//log("bg-->" + data);
		this.queue.write(data,name);
	}
	this.cycle=function() 
	{
		/* 	if chat has been disabled, 
			do not cycle events */
		if(this.disabled) {
			return false;
		}
			
		/* process synchronization data */
		var sync_data=this.queue.read("sync");
		if(sync_data) {
			//log("bg<--" + sync_data);
			this.synchronize(sync_data);
		}

		/* if we are not connected or in the process of connecting */
		if(!this.connected && !this.connecting) {
			/* 	if we are not connected and have exceeded our connection attempt limit,c
				disable chat */
			if(conn_attempts>chat_settings.CONNECTION_ATTEMPTS) {
				this.server_chan.post("\1rconnection limit exceeded");
				this.enqueue("disconnect","sync");
				this.reset_connection();
				this.disabled=true;
			}
			/* 	if we are scheduled to connect, establish socket connection */
			if((time()-last_conn_attempt)>chat_settings.CONNECTION_INTERVAL) {
				this.enqueue("connect","sync");
				conn_attempts++;
				this.connecting=true;
			}
		}
		
		/* if our socket is connected */
		else if(this.connected) {
			/* perform any protocol cycle operations */
			this.cycleProtocol();
			
			/* decode and process chat data */
			var data=false;
			var raw_data=this.queue.read("data");
			if(raw_data) {
				//log("bg<--" + raw_data);
				data=this.decode(raw_data);
			}
			if(data)
				data=this.process(data);
			return data;
		}
	}
	this.send=function(data)
	{	
		this.enqueue(this.encode(data),"data");
	}
	this.handle_command=function(text,target)
	{
		var data=this.parse(text,target);
		if(!data) return false;

		this.enqueue(this.encode(data),"data");
		return data;
	}
	this.reset_counters=function()
	{
		this.conn_attempts=0;
		this.last_conn_attempt=0;
	}
	this.reset_connection=function()
	{
		this.connection_established=false;
		this.connected=false;
		this.connect=true;
		this.registered=false;
		this.registering=false;
	}
	this.init();
}

