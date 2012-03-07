load("json-sock.js");
/*     
	JSON client  - for Synchronet 3.15a+ (2011)

	-	code by mcmlxxix
	
	-	it is recommended to create a callback function specific to your program
	-	and assign it to the callback property of the JSONClient object. If you do
	-	not do this, any packets not specifically requested by your program
	-	will be pushed into an array and sit there until you handle them

	direct methods: these methods can be called directly by the main script
	
	-	JSONClient.cycle(); 
	-	JSONClient.connect();
	-	JSONClient.disconnect();
	-	JSONClient.read();
	-	JSONClient.pop();
	-	JSONClient.shift();
	-	JSONClient.write();
	-	JSONClient.push();
	-	JSONClient.unshift();
	-	JSONClient.lock();
	-	JSONClient.unlock();
	-	JSONClient.subscribe();
	-	JSONClient.unsubscribe();
	-	JSONClient.status();
	-	JSONClient.who();
	-	JSONClient.ident();
	
	indirect methods: these will generally be called automatically by the other methods
	and you will not typically need to use them
	
	-	JSONClient.callback();     
	-	JSONClient.send();
	-	JSONClient.receive();
	-	JSONClient.wait();
	
	arguments: 
	
	-	argv[0] = serverAddr;
	-	argv[1] = serverPort;
	
	sample usage:
	
		var LOCK_READ = 1;
		var LOCK_WRITE = 2;
		var UNLOCK = -1;
	
		load("json-client.js");
		var client=new JSONClient(myServer,myPort);
		
		function callback(data) {
			myData = data;
		}
		
		while(1) {
			doSomething();
			client.lock("mydatabase.dong",LOCK_READ);
			var dong=client.read("mydatabase.dong");
			client.unlock("mydatabase.dong");
			print("look at my " + dong);
			client.cycle();
		}
	
*/

function JSONClient(serverAddr,serverPort) {
	this.VERSION = "$Revision$".replace(/\$/g,'').split(' ')[1];
	this.serverAddr=serverAddr;
    if(this.serverAddr==undefined) 
		throw("no host specified");

	this.serverPort=serverPort;
    if(this.serverPort==undefined)
		throw("no port specified");
	
	this.settings={
		CONNECTION_TIMEOUT:		5,
		PING_INTERVAL:			60*1000,
		PING_TIMEOUT:			10*1000,
		RECV_TIMEOUT:			10
	};
        
    this.socket=undefined; 
	this.callback;
	this.updates=[];
	
	/* convert null values to undefined when parsing */
	Socket.prototype.reviver = function(k,v) { if(v === null) return undefined; return v; };
	
	/* create new socket connection to server */
    this.connect = function() {
        this.socket=new Socket();
		this.socket.connect(this.serverAddr,this.serverPort,this.settings.CONNECTION_TIMEOUT);
		
		if(!this.socket.is_connected)
			throw("error connecting to server");
    }
    
    this.disconnect = function() {
        this.socket.close();
    }
    
	/* subscribe to object updates */
    this.subscribe=function(scope,location) {
		this.send(scope,"QUERY",{
            oper:"SUBSCRIBE",
			nick:user?user.alias:undefined,
			system:system?system.name:undefined,
            location:location
        });
    }
    
    this.unsubscribe=function(scope,location) {
		this.send(scope,"QUERY",{
            oper:"UNSUBSCRIBE",
            location:location
        });
    }
	
	/* lock an object */
	this.lock = function(scope,location,lock) {
		this.send(scope,"QUERY",{
            location:location,
			oper:"LOCK",
			data:lock
        });
	}
	
	/* unlock an object */ 
	this.unlock = function(scope,location) {
		this.lock(scope, location, -1);
	}
    
	/* read object data (lock for reading or writing, blocking) */
    this.read=function(scope,location,lock) {
		this.send(scope,"QUERY",{
            oper:"READ",
            location:location,
			lock:lock
        });
		return this.wait("RESPONSE");
    }
	
	/* shift object data (lock for reading or writing, blocking) */
    this.shift=function(scope,location,lock) {
		this.send(scope,"QUERY",{
            oper:"SHIFT",
            location:location,
			lock:lock
        });
		return this.wait("RESPONSE");
    }

	/* pop object data (lock for reading or writing, blocking) */
    this.pop=function(scope,location,lock) {
		this.send(scope,"QUERY",{
            oper:"POP",
            location:location,
			lock:lock
        });
		return this.wait("RESPONSE");
    }
    
	/* store object data (lock for writing) */
    this.write=function(scope,location,data,lock) {
        this.send(scope,"QUERY",{
			oper:"WRITE",
            location:location,
            data:data,
			lock:lock
        });
    }

	/* store object data (lock for writing) */
    this.remove=function(scope,location,lock) {
        this.send(scope,"QUERY",{
			oper:"WRITE",
            location:location,
			data:undefined,
			lock:lock
        });
    }

	/* unshift object data (lock for writing) */
    this.unshift=function(scope,location,data,lock) {
        this.send(scope,"QUERY",{
            oper:"UNSHIFT",
            location:location,
            data:data,
			lock:lock
        });
    }

	/* push object data (lock for writing) */
    this.push=function(scope,location,data,lock) {
        this.send(scope,"QUERY",{
            oper:"PUSH",
            location:location,
            data:data,
			lock:lock
        });
    }
	
	/* package a query and send through the socket */
	this.send=function(scope,func,data) {
		if(!this.socket.is_connected)
			throw("socket disconnected");
		var packet={
			scope:scope,
			func:func,
			data:data
		};
		this.socket.sendJSON(packet);
	}

	/* receive a data packet */
	this.receive=function() {
		if(!this.socket.is_connected)
			throw("socket disconnected");
		if(!this.socket.data_waiting) 
			return false;
		var packet=this.socket.recvJSON();
		switch(packet.func.toUpperCase()) {
		case "PING":
			this.socket.pingOut("PONG");
			return false;
		case "PONG":
			this.socket.pingIn(packet);
			return false;
		case "ERROR":
			throw(packet.data.description);
			return false;
		}
		return packet;
	}
	
	/* do not return until the expected response is received */
	this.wait=function(func) {
		var start = time();
		do {
			var packet = this.receive();
			
			if(!packet)
				continue;
			else if(packet.func == func) 
				return packet.data;
			else if(typeof this.callback == "function")
				this.callback(packet.data);
			else 
				this.updates.push(packet.data);
		} while(time() - start < this.settings.RECV_TIMEOUT);
		
		log(LOG_ERROR,"timed out waiting for server response");
		exit();
	}

	/* check socket for data, and process it if a callback is specified */
	this.cycle=function() {
		var packet=this.receive();
		if(!packet)
			return false;
		else if(typeof this.callback == "function")
			this.callback(packet.data);
		else 
			this.updates.push(packet.data);
	}
	
	/* identify this client as a bbs user */
	this.ident=function(scope,username,pw) {
		pw = md5_calc(pw.toUpperCase(),true);
		this.send(scope,"IDENT",{
            username:username,
			pw:pw
        });
	}

	/* return a list of record subscriber IP addresses */
	this.who=function(scope,location) {
		this.send(scope,"QUERY",{
            oper:"WHO",
            location:location
        });
		return this.wait("RESPONSE");
	}

	/* retrieve the overall lock and subscription status of an object */
	this.status=function(scope,location) {
		this.send(scope,"QUERY",{
			oper:"STATUS",
			location:location
        });
		return this.wait("RESPONSE");
	}
	
	this.connect();
	log(LOG_INFO,"JSON client initialized (v" + this.VERSION + ")");
};
