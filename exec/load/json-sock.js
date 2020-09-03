//load("synchronet-json.js");
/* socket mod version */
Socket.prototype.VERSION = "$Revision: 1.18 $".replace(/\$/g,'').split(' ')[1];
/* round trip packet time */
Socket.prototype.latency = 0;
/* one way (latency / 2) */
Socket.prototype.time_offset = 0; 
/* largest receivable packet */
Socket.prototype.max_recv = 131072;
/* timeout for socket.recvline() */
Socket.prototype.recv_wait = 30;
/* last ping sent */
Socket.prototype.ping_sent = 0;
/* debug logging */
Socket.prototype.debug_logging = false;

/* socket prototype to automatically encode JSON data */
Socket.prototype.sendJSON = function(object) {
	try {
		var oldnb = this.nonblocking;
		var data=JSON.stringify(object,this.replacer,this.space)+"\r\n";
		this.nonblocking = false;
		if(!this.send(data)) {
			log(LOG_ERROR,"send failed ("+this.error+"): " + data);
		}
		else if(this.debug_logging) {
			log(LOG_DEBUG,"-->" + this.descriptor + ": " + data);
		}
		this.nonblocking=oldnb;
	} catch(e) {
		log(LOG_ERROR,e);
	}
};

/*  socket prototype to automatically decode JSON data */
Socket.prototype.recvJSON = function() { 
	var packet=this.recvline(this.max_recv,this.recv_wait); 
	if(packet != null) {
		if(this.debug_logging)
			log(LOG_DEBUG,"<--" + this.descriptor + ": " + packet);
		try {
			packet=JSON.parse(packet,this.reviver);
			if(packet.scope && packet.scope.toUpperCase() == "SOCKET") {
				this.process(packet);
			}
		} 
		catch(e) {
			log(LOG_ERROR,e);
		}
	}
	return packet;
};

Socket.prototype.process = function(packet) {
	if(packet.func && packet.func.toUpperCase() == "PONG") {
		this.pingIn(packet);
	}
	else if(packet.func && packet.func.toUpperCase() == "PING") {
		this.pingOut("PONG");
	}
}

/* ping pong */		
Socket.prototype.pingOut = function(func) {
	var ping={
		scope:"SOCKET",
		func:func,
		data:Date.now()
	}
	this.sendJSON(ping);
	this.ping_sent=ping.time;
};

/* calculate latency */
Socket.prototype.pingIn = function(packet) {
	var time=Date.now();
	var latency=(time-this.ping_sent)/2;
	var gap=(this.ping_sent+latency) - packet.data;
	if(this.time_offset != gap) {
		this.time_offset=gap;
		this.latency=latency;
		log(LOG_DEBUG,"<--" + this.descriptor + ": rt " + this.latency + " ow " + this.time_offset);
	}
};
