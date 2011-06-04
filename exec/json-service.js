load("event-timer.js");
load("json-sock.js");
load("json-db.js");

var timer = new Timer();
var maindb = new JSONdb(system.data_dir+"db.json");

/* server object */
service = new (function() {

	this.VERSION = "$Revision$".split(' ')[1];
	this.online = true;
	this.sockets = [];
	this.denyhosts = [];
	this.timeout = 1;
	
	this.init = function() {
 
		/* method called when socket is selected for reading */
		Socket.prototype.cycle = function() {
			var packet=this.recvJSON();
			if(!packet) 
				return false;
			if(!packet.func)
				return false;
			switch(packet.func.toUpperCase()) {
			/* main database queries */
			case "QUERY":
				maindb.query(this,packet.data);
				break;
			case "ADMIN":
				admin.query(this,packet.data);
				break;
			/* unknown queries (usually a specific module) */
			default:
				engine.query(this,packet);
				break;
			}
		}
		
		server.socket.nonblocking = true;
		server.socket.cycle = function() {
			var client=this.accept();
			if(!service.online) {
				client.close();
			}
			else if(service.denyhosts[client.remote_ip_address]) {
				log(LOG_INFO,"blocked: " + client.remote_ip_address);
				client.close();
			}
			else {
				log(LOG_INFO,"connected: " + client.remote_ip_address);
				client.nonblocking = true;
				client.id = client.descriptor;
				service.sockets.push(client);
			}
		}
		
		this.sockets.push(server.socket);
		log(LOG_INFO,"JSON server initialized (v" + this.VERSION + ")");
	}
	
	this.cycle = function() {
		var active=socket_select(this.sockets,this.timeout);
		for each(var s in active) {
			this.sockets[s].cycle();
		}
		for(var s=1;s<this.sockets.length;s++) {
			if(!this.sockets[s].is_connected) {
				log(LOG_INFO,"disconnected: " + this.sockets[s].remote_ip_address);
				maindb.release(this.sockets[s].descriptor);
				this.sockets.splice(s--,1);
			}
			if(this.denyhosts[this.sockets[s].remote_ip_address]) {
				log(LOG_INFO,"disconnecting: " + this.sockets[s].remote_ip_address);
				maindb.release(this.sockets[s].descriptor);
				this.sockets[s].close();
				this.sockets.splice(s--,1);
			}
		} 
	}

	this.init();
})();

/* administrative tools */
admin = new (function() {
	this.authorized = [];
	this.query = function(client,data) {
		switch(data.operation) {
		case "IDENT":
			this.ident(client.descriptor,data.username,data.pw);
			break;
		case "RESTART":
			this.restart(client.descriptor);
			break;
		case "BAN":
			this.ban(client.remote_ip_address,data.ip);
			break;
		case "UNBAN":
			this.unban(client.remote_ip_address,data.ip);
			break;
		case "CLOSE":
			this.close(client.descriptor);
			break;
		case "OPEN":
			this.open(client.descriptor);
			break;
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
		this.authorized[descriptor] = true;
		log(LOG_DEBUG,"identified: " + username);
		return true;
	}
	this.close = function(descriptor) {
		if(!this.authorized[descriptor])
			return false;
		log(LOG_WARNING,"socket service offline");
		service.online = false;
	}
	this.open = function(descriptor) {
		if(!this.authorized[descriptor])
			return false;
		log(LOG_WARNING,"socket service online");
		service.online = true;
	}
	this.restart = function(descriptor) {
		if(!this.authorized[descriptor])
			return false;
		log(LOG_WARNING,"restarting service");
		exit();
	}
	this.ban = function(descriptor,source,target) {
		if(!this.authorized[descriptor])
			return false;
		if(source == target)
			return false;
		log(LOG_WARNING,"ban added: " + target);
		service.denyhosts[target] = true;
	}
	this.unban = function(descriptor,source,target) {
		if(!this.authorized[descriptor])
			return false;
		if(source == target)
			return false;
		log(LOG_WARNING,"ban removed: " + target);
		delete service.denyhosts[target];
	}
})();

/* module handler */
engine = new (function() {

	this.modules = [];
	
	this.file = new File(system.ctrl_dir + "json-service.ini");
	
	this.init = function() {
		/* if there is no module initialization file, fuck it */
		if(!file_exists(this.file.name))
			return;

		this.file.open('r');
		var modules=this.file.iniGetObject();
		this.file.close();

		for(var l in modules) {
			this.modules[l.toUpperCase()]=new Module(modules[l],l);
		}
	}

	this.cycle = function() {
		for each(var m in this.modules) {
			m.cycle();
		}
	}

	this.query = function(client,query) {
		if(this.modules[query.func.toUpperCase()]) {
			this.modules[module.toUpperCase()].query(client,query.data);
		}
		else {
			var error={
				func:"ERROR",
				data:{
					operation:"ERROR",
					error_num:0,
					error_desc:"module not found"
				}
			};
			client.sendJSON(error);
		}
	}

	function Module(dir,name) {
		this.dir=dir;
		this.name=name;
		this.init=function() {
			log(LOG_DEBUG,"initializing module: " + l);
			if(file_exists(this.dir + "service.js")) {
				load(this,this.dir + "service.js");
			}
		}
		
		this.init();
	}
	
	this.init();
})();	

/* main service loop */
while(!js.terminated) {
	service.cycle();
	engine.cycle();
	timer.cycle();
	maindb.cycle();
}
