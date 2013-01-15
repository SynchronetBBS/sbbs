load("event-timer.js");
load("json-sock.js");
load("json-db.js");

// Running from jsexec presumably...
if(js.global.server==undefined) {
	load("sockdefs.js");
	server={};
	server.socket=new Socket(SOCK_STREAM, 'JSONDB');
	server.socket.bind(10088,'127.0.0.1');
	server.socket.listen();
}

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

/* service module initialization file */
var serviceIniFile = system.ctrl_dir + "json-service.ini";
 
/* error values */
var errors = {
	UNKNOWN_MODULE:"Unknown module: %s",
	UNKNOWN_FUNCTION:"Unknown function: %s",
	UNKNOWN_COMMAND:"Unknown command: %s",
	UNKNOWN_USER:"User not found: %s",
	SERVICE_OFFLINE:"Service offline",
	MODULE_OFFLINE:"Module offline",
	IDENT_REQUIRED:"You must identify before using this command",
	NOT_AUTHORIZED:"You are not authorized to use that command",
	INVALID_PASSWORD:"Password incorrect"
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
			if(!service.verify(client)) {
				log(LOG_WARNING,"<--" + client.descriptor + ": " + client.remote_ip_address + " blocked");
				client.close();
				return;
			} 
			
			/* log and initialize normal connections */
			log(LOG_INFO,"<--" + client.descriptor + ": " + client.remote_ip_address + " connected");
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
				log(LOG_INFO,"<--" + this.sockets[s].descriptor + ": " 
					+ this.sockets[s].remote_ip_address + " disconnected");
				this.release(this.sockets[s]);
				this.sockets.splice(s--,1);
			}
			// else if(!this.verify(this.sockets[s])) {
				// log(LOG_WARNING,"<--" + this.sockets[s].descriptor + ": " 
					// + this.sockets[s].remote_ip_address + " terminated");
				// this.release(this.sockets[s]);
				// this.sockets[s].close();
				// this.sockets.splice(s--,1);
			// }
		} 
	}
	/* check ip/host against ip/host.can && this.denyhosts[] */
	this.verify = function(client) {
		if(this.denyhosts[client.remote_ip_address])
			return false;
		if(system.trashcan("ip.can",client.remote_ip_address))
			return false;
		if(system.trashcan("ip-silent.can",client.remote_ip_address))
			return false;
		return true;
	}
	/* release all locks and auths for a socket */
	this.release = function(client) {
		engine.release(client);
		admin.release(client);
		chat.release(client);
	}
	/* disconnect all clients and refuse new connections */
	this.close = function(client) {
		this.online = false;
		for(var s=1;s<this.sockets.length;s++) {
			if(this.sockets[s].descriptor != client.descriptor) {
				log(LOG_INFO,"<--" + this.sockets[s].descriptor + ": " 
					+ this.sockets[s].remote_ip_address + " disconnected");
				this.sockets[s].close();
				this.release(this.sockets[s]);
				this.sockets.splice(s--,1);
			}
		}
		confirm(client,"Service offline");
	}
	/* open service for new connections */
	this.open = function(client) {
		this.online = true;
		confirm(client,"Service online");
	}

	this.init();
})();

/* chat handler */
chat = new (function() {
	this.db = new JSONdb(system.data_dir+"chat.json");
	this.authenticated = [];
	this.deny_hosts = [];
	
	this.cycle = function() {
		this.db.cycle();
	}
	this.process = function(client,packet) {
		switch(packet.func.toUpperCase()) {
		case "IDENT":
			this.ident(client,packet.data);
			break;
		case "BAN":
			if(!admin.verify(client,packet,90))
				break;
			this.ban(client,packet.data.ip);
			break;
		case "UNBAN":
			if(!admin.verify(client,packet,90))
				break;
			this.unban(client,packet.data.ip);
			break;
		case "QUERY":
			this.db.query(client,packet.data);
			break;
		default:
			error(client,errors.UNKNOWN_FUNCTION,packet.func);
			break;
		}
	}
	this.ident = function(client,data) {
		var username = data.username;
		var pw = data.pw;
		var usernum = system.matchuser(username);
		if(usernum  == 0) {
			error(client,errors.UNKNOWN_USER,username);
			return false;
		}
		var usr = new User(usernum);
		var pass = md5_calc(usr.security.password,true);
		
		if(md5_calc(usr.security.password,true) != pw) {
			error(client,errors.INVALID_PASSWORD);
			return false;
		}
		this.authenticated[client.descriptor] = usr;
		confirm(client,"Identified: " + username);
		return true;
	}
	this.ban = function(client,target) {
		if(client.remote_ip_address == target)
			return false;
		confirm(client,"Ban Added: " + target);
		this.denyhosts[target] = true;
	}
	this.unban = function(client,target) {
		if(client.remote_ip_address == target)
			return false;
		confirm(client,"Ban Removed: " + target);
		delete this.denyhosts[target];
	}
	this.release = function(client) {
		if(this.authenticated[client.id]) {
			log(LOG_DEBUG,"releasing auth: " + client.id);
			delete this.authenticated[client.id];
		}
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
			this.ident(client,packet.data);
			break;
		case "RESTART":
			if(!this.verify(client,packet,90))
				break;
			this.restart(client);
			break;
		case "BAN":
			if(!this.verify(client,packet,90))
				break;
			this.ban(client,packet.data.ip);
			break;
		case "UNBAN":
			if(!this.verify(client,packet,90))
				break;
			this.unban(client,packet.data.ip);
			break;
		case "CLOSE":
			if(!this.verify(client,packet,90))
				break;
			service.close(client);
			break;
		case "OPEN":
			if(!this.verify(client,packet,90))
				break;
			service.open(client);
			break;
		case "MODULES":
			if(!this.verify(client,packet,90))
				break;
			this.modules(client);
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
	this.modules = function(client) {
		var list = [];
		for each(var m in engine.modules) {
			list.push({name:m.name,status:m.online});
		}
		respond(client,list);
	}
	this.ident = function(client,data) {
		var username = data.username;
		var pw = data.pw;
		var usernum = system.matchuser(username);
		if(usernum  == 0) {
			error(client,errors.UNKNOWN_USER,username);
			return false;
		}
		var usr = new User(usernum);
		var pass = md5_calc(usr.security.password,true);
		
		if(md5_calc(usr.security.password,true) != pw) {
			error(client,errors.INVALID_PASSWORD);
			return false;
		}
		this.authenticated[client.descriptor] = usr;
		confirm(client,"Identified: " + username);
		return true;
	}
	this.restart = function(client) {
		confirm(client,"Service Restarted");
		exit();
	}
	this.ban = function(client,target) {
		if(client.remote_ip_address == target)
			return false;
		confirm(client,"Ban Added: " + target);
		service.denyhosts[target] = true;
	}
	this.unban = function(client,target) {
		if(client.remote_ip_address == target)
			return false;
		confirm(client,"Ban Removed: " + target);
		delete service.denyhosts[target];
	}
	this.verify = function(client,packet,level) {
		if(!this.authenticated[client.id]) {
			error(client,errors.IDENT_REQUIRED,packet.func);
			return false;
		}
		else if(this.authenticated[client.id].security.level < level) {
			error(client,errors.NOT_AUTHORIZED,packet.func);
			return false;
		}
		return true;
	}
	log(LOG_DEBUG,"admin initialized");
})();

/* module handler */
engine = new (function() {
	this.modules = [];
	this.queue = undefined;
	this.file = new File(serviceIniFile);
	
	/* load module list */
	this.init = function() {
		/* if there is no module initialization file, fuck it */
		if(!file_exists(this.file.name))
			return;
		this.file.open('r',true);
		var modules=this.file.iniGetAllObjects();
		this.file.close();
		for each(var m in modules) {
			this.modules[m.name.toUpperCase()]=new Module(m.name,m.dir,m.auth);
			log(LOG_DEBUG,"loaded module: " + m.name);
		}
	}
	
	/* cycle module databases */
	this.cycle = function() {
		for each(var m in this.modules) {
			if(!m.online)
				continue;
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
			if(!this.verify(client,packet,module)) {
				error(client,errors.NOT_AUTHORIZED,packet.func);
				break;
			}
			module.db.query(client,packet.data);
			break;
		case "IDENT":
			break;
		case "RELOAD":
			if(!admin.verify(client,packet,90))
				break;
			if(module.queue)
				module.queue.write("RELOAD");
			confirm(client,module.name + " reloaded");
			module.init();
			break;
		case "CLOSE":
			if(!admin.verify(client,packet,90))
				break;
			module.online = false;
			confirm(client,module.name + " offline");
			break;
		case "STATUS":
			respond(client,module.online);
			break;
		case "OPEN":
			if(!admin.verify(client,packet,90))
				break;
			module.online = true;
			confirm(client,module.name + " online");
			break;
		case "READABLE":
			if(!admin.verify(client,packet,90))
				break;
			module.db.settings.KEEP_READABLE = !module.db.settings.KEEP_READABLE;
			break;
		case "SAVE":
			if(!admin.verify(client,packet,90))
				break;
			module.db.save();
			confirm(client,module.name + " data saved");
			break;
		case "READONLY":
			if(!admin.verify(client,packet,90))
				break;
			module.db.settings.READ_ONLY = !module.db.settings.READ_ONLY;
			break;
		default:
			error(client,errors.UNKNOWN_FUNCTION,packet.func);
			break;
		}
	}

	/* verify a client's access to queries */
	this.verify = function(client,packet,module) {
		switch(packet.data.oper) {
		case "READ":
		case "SLICE":
			if(module.read > 0) {
				if(!admin.verify(client,packet,module.read)) {
					error(client,errors.NOT_AUTHORIZED,packet.data.oper);
					return false;
				}
			}
			break;
		case "WRITE":
		case "PUSH":
		case "UNSHIFT":
		case "DELETE":
		case "POP":
		case "SHIFT":
			if(module.write > 0) {
				if(!admin.verify(client,packet,module.write)) {
					error(client,errors.NOT_AUTHORIZED,packet.data.oper);
					return false;
				}
			}
			break;
		}
		return true;
	}

	/* release clients from module authentication and subscription */
	this.release = function(client) {
		for each(var m in this.modules) {
			m.db.release(client);
		}
	}

	/* module data */
	function Module(name, dir, auth) {
		this.online = true;
		this.name = name;
		this.dir = dir;
		this.queue = undefined;
		this.auth = auth;
		this.db;
		
		this.init = function() {
			this.db=new JSONdb(this.dir+this.name+".json");
			/* load module service files */
			if(file_exists(this.dir + "service.js")) {
				try {
					this.queue = load(true,this.dir + "service.js",this.dir);
				} catch(e) {
					log(LOG_ERROR,"error loading module: " + this.name);
				}
			}
		}
		this.init();
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

/* confirmation to client */
confirm = function(client,message) {
	log(LOG_INFO,message);
	client.sendJSON({
		func:"OK",
		data:message
	});
}

/* packet request response to client */
respond = function(client,data) {
	client.sendJSON({
		func:"RESPONSE",
		data:data
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
