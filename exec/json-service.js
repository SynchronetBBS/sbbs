load("event-timer.js");
load("json-sock.js");
load("json-db.js");

var timer = new Timer();
var maindb = new JSONdb(system.data_dir+"db.json");

/* server object */
service = new (function() {

	this.VERSION = "$Revision$".split(' ')[1];
	this.sockets = [];
	this.timeout = 1;
	
	this.init = function() {

		/* method called when socket is selected for reading */
		Socket.prototype.cycle = function() {
			var packet=this.recvJSON();
			if(!packet) 
				return false;
			switch(packet.func.toUpperCase()) {
			/* main database queries */
			case "QUERY":
				maindb.query(this,packet.data);
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
			client.nonblocking = true;
			client.id = client.descriptor;
			service.sockets.push(client);
			log(LOG_INFO,"connected: " + client.remote_ip_address);
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
		} 
	}

	this.init();
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
