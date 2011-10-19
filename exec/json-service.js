load("event-timer.js");
load("json-sock.js");
load("json-db.js");

/**** SERVICE MODULES
 * 
 * 	main service (socket service)
 * 		socket output
 * 		socket input
 * 		routing
 * 			module engine
 * 			service administration
 * 			chat engine
 * 
 * 	module engine (game service)
 * 		database queries
 * 		module administration
 * 		module events
 * 
 * 	event timer
 * 		timed events
 * 			
 * 	service administration 
 * 		user authentication
 * 		service control
 * 		access control
 * 		service information
 * 
 * 	chat engine 
 * 		database queries
 * 		administration
 * */
 
/**** PACKET DATA
 * 
 * 	module = 
 * 		"SOCKET"
 * 		"SERVICE"
 * 		"CHAT"
 * 		(module name)
 *
 * 	func = 
 * 		"QUERY"
 *		"ADMIN"
 *		(other function)
 * 
 *	data =
 *		{..}
 * 
 * */

/* error values */
var errors = {
	UNKNOWN_MODULE:"Unknown module: %s",
	UNKNOWN_FUNCTION:"Unknown function: %s",
	UNKNOWN_COMMAND:"Unknown command: %s",
	UNKNOWN_USER:"User not found: %s",
	SERVICE_OFFLINE:"Service offline",
	MODULE_OFFLINE:"Module offline"
};
 
/* server object */
service = new (function() {

	this.VERSION = "$Revision$".replace(/\$/g,'').split(' ')[1];
	this.online = true;
	this.sockets = [];
	this.denyhosts = [];
	this.timeout = 1;
	
	/* initialize server */
	this.init = function() {
 
		/* method called when socket is selected for reading */
		Socket.prototype.cycle = function() {
			var packet=this.recvJSON();
			/* if nothing was received, return */
			if(!packet) 
				return false;
			/* if no module was specified, data assumed invalid */
			if(!packet.scope)
				return false;
			switch(packet.scope.toUpperCase()) {
			/* service administration */
			case "ADMIN":
				admin.process(this,packet);
				break;
			/* chat service */
			case "CHAT":
				chat.process(this,packet);
				break;
			/* socket data */
			case "SOCKET":
				service.process(this,packet);
				break;
			/* modular queries */
			default:
				engine.process(this,packet);
				break;
			}
		}
		
		/* configure server socket */
		server.socket.nonblocking = true;
		server.socket.cycle = function() {
			var client=this.accept();
			/* open and immediately close sockets if service is offline */
			if(!service.online) {
				client.close();
				return;
			}
			/* log and disconnect banned hosts */
			if(service.denyhosts[client.remote_ip_address]) {
				log(LOG_INFO,"blocked: " + client.remote_ip_address);
				client.close();
				return;
			} 
			
			/* log and initialize normal connections */
			log(LOG_INFO,"connected: " + client.remote_ip_address);
			client.nonblocking = true;
			client.id = client.descriptor;
			service.sockets.push(client);
		}
		
		this.sockets.push(server.socket);
		log(LOG_INFO,"server initialized (v" + this.VERSION + ")");
	}
	/* process socket data */
	this.process = function(client,packet) {
		switch(packet.func.toUpperCase()) {
		case "PING":
			client.pingOut("PONG");
			break;
		case "PONG":
			client.pingIn(packet);
			break;
		default:
			error(client,errors.UNKNOWN_FUNCTION,packet.func);
			break;
		}
	}
	/* monitor sockets */
	this.cycle = function() {
		var active=socket_select(this.sockets,this.timeout);
		for each(var s in active) {
			this.sockets[s].cycle();
		}
		for(var s=1;s<this.sockets.length;s++) {
			if(!this.sockets[s].is_connected) {
				log(LOG_INFO,"disconnected: " + this.sockets[s].remote_ip_address);
				this.release(this.sockets[s]);
				this.sockets.splice(s--,1);
			}
			if(this.denyhosts[this.sockets[s].remote_ip_address]) {
				log(LOG_INFO,"disconnecting: " + this.sockets[s].remote_ip_address);
				this.release(this.sockets[s]);
				this.sockets[s].close();
				this.sockets.splice(s--,1);
			}
		} 
	}	
	/* release all locks and auths for a socket */
	this.release = function(client) {
		engine.release(client);
		admin.release(client);
		chat.release(client);
	}

	this.init();
})();

/* chat handler */
chat = new (function() {
	this.db = new JSONdb(system.data_dir+"chat.json");
	this.cycle = function() {
		this.db.cycle();
	}
	this.process = function(client,packet) {
		switch(packet.func.toUpperCase()) {
		case "QUERY":
			this.db.query(client,packet.data);
			break;
		default:
			error(client,errors.UNKNOWN_FUNCTION,packet.func);
			break;
		}
	}
	this.release = function(client) {
		this.db.release(client);
	}
	log(LOG_DEBUG,"chat initialized");
})();

/* administrative tools */
admin = new (function() {
	this.authenticated = [];
	/* route packet to correct function */
	this.process = function(client,packet) {
		switch(packet.func.toUpperCase()) {
		case "IDENT":
			this.ident(client.descriptor,packet.data.username,packet.data.pw);
			break;
		case "RESTART":
			this.restart(client.descriptor);
			break;
		case "BAN":
			this.ban(client.remote_ip_address,packet.data.ip);
			break;
		case "UNBAN":
			this.unban(client.remote_ip_address,packet.data.ip);
			break;
		case "CLOSE":
			this.close(client.descriptor);
			break;
		case "OPEN":
			this.open(client.descriptor);
			break;
		default:
			error(client,errors.UNKNOWN_FUNCTION,packet.func);
			break;
		}
	}
	/* release a socket from the list of authenticated users */
	this.release = function(client) {
		if(this.authenticated[client.id]) {
			log(LOG_DEBUG,"releasing auth: " + client.id);
			delete this.authenticated[client.id];
		}
	}
	this.ident = function(descriptor,username,pw) {
		var usernum = system.matchuser(username);
		if(usernum  == 0) {
			log(LOG_WARNING,"no such user: " + username);
			return false;
		}
		var usr = new User(usernum);
		var pass = md5_calc(usr.security.password,true);
		
		if(md5_calc(usr.security.password,true) != pw) {
			log(LOG_WARNING,"failed pw attempt for user: " + username);
			return false;
		}
		if(usr.security.level < 90) {
			log(LOG_WARNING,"insufficient access: " + username);
			return false;
		}
		this.authenticated[descriptor] = true;
		log(LOG_DEBUG,"identified: " + username);
		return true;
	}
	this.close = function(descriptor) {
		if(!this.authenticated[descriptor])
			return false;
		log(LOG_WARNING,"socket service offline");
		service.online = false;
	}
	this.open = function(descriptor) {
		if(!this.authenticated[descriptor])
			return false;
		log(LOG_WARNING,"socket service online");
		service.online = true;
	}
	this.restart = function(descriptor) {
		if(!this.authenticated[descriptor])
			return false;
		log(LOG_WARNING,"restarting service");
		exit();
	}
	this.ban = function(descriptor,source,target) {
		if(!this.authenticated[descriptor])
			return false;
		if(source == target)
			return false;
		log(LOG_WARNING,"ban added: " + target);
		service.denyhosts[target] = true;
	}
	this.unban = function(descriptor,source,target) {
		if(!this.authenticated[descriptor])
			return false;
		if(source == target)
			return false;
		log(LOG_WARNING,"ban removed: " + target);
		delete service.denyhosts[target];
	}
	log(LOG_DEBUG,"admin initialized");
})();

/* module handler */
engine = new (function() {
	this.modules = [];
	this.file = new File(system.ctrl_dir + "json-service.ini");
	/* load module list */
	this.init = function() {
		/* if there is no module initialization file, fuck it */
		if(!file_exists(this.file.name))
			return;
		this.file.open('r',true);
		var modules=this.file.iniGetObject();
		this.file.close();
		for(var l in modules) {
			this.modules[l.toUpperCase()]=new Module(modules[l],l);
			log(LOG_DEBUG,"loaded module: " + l);
		}
	}
	/* cycle module databases */
	this.cycle = function() {
		for each(var m in this.modules) {
			if(!m.online)
				continue;
			if(typeof m.cycle == "function") 
				m.cycle();
			m.db.cycle();
		}
	}
	/* process a module function */
	this.process = function(client,packet) {
		var module = this.modules[packet.scope.toUpperCase()];
		if(!module) {
			error(client,errors.UNKNOWN_MODULE,packet.scope);
			return false;
		}
		switch(packet.func.toUpperCase()) {
		case "QUERY":
			module.db.query(client,packet.data);
			break;
		case "IDENT":
			break;
		case "RESET":
			break;
		case "CLOSE":
			module.online = false;
			break;
		case "OPEN":
			module.online = true;
			break;
		default:
			error(client,errors.UNKNOWN_FUNCTION,packet.func);
			break;
		}
	}
	/* release clients from module authentication and subscription */
	this.release = function(client) {
		for each(var m in this.modules) 
			m.db.release(client);
	}
	/* module data */
	function Module(dir,name) {
		this.online = true;
		this.db = new JSONdb(dir+name+".json");
		/* load module service files */
		if(file_exists(dir + "service.js")) {
			try {
				load(this,dir + "service.js");
			} 
			catch(e) {
				this.online = false;
				log(LOG_ERROR,e);
			}
		}
	}
	
	this.init();
})();

/* error reporting/logging */
error = function(client,err,value) {
	var desc = format(err,value);
	log(LOG_ERROR,format(
		"Error: (%s) %s",
		client.descriptor,desc
	));
	client.sendJSON({
		func:"ERROR",
		data:{
			description:desc,
			client:client.descriptor,
			ip:client.remote_ip_address
		}
	});
}

/* event timer */
var timer = new Timer();

/* main service loop */
while(!js.terminated) {
	service.cycle();
	chat.cycle();
	engine.cycle();
	timer.cycle();
}
