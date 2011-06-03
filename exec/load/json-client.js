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
	-	JSONClient.write();
	-	JSONClient.lock();
	-	JSONClient.unlock();
	-	JSONClient.subscribe();
	-	JSONClient.unsubscribe();
	-	JSONClient.status();
	
	indirect methods: these will generally be called automatically by the other methods
	and you will not typically need to use them
	
	-	JSONClient.callback();     
	-	JSONClient.send();
	-	JSONClient.receive();
	-	JSONClient.wait();
	
	optional arguments: if these are not supplied, 
	the client will connect to the default server (bbs.thebrokenbubble.com)
	
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

	this.VERSION = "$Revision$".split(' ')[1];
	
	this.serverAddr=serverAddr;
    if(this.serverAddr==undefined) {
        this.serverAddr="bbs.thebrokenbubble.com"; 
		log(LOG_DEBUG,"using default server address: " + this.serverAddr);
	}

	this.serverPort=serverPort;
    if(this.serverPort==undefined) {
        this.serverPort=10088;
		log(LOG_DEBUG,"using default server port: " + this.serverPort);
	}
	
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
	Socket.prototype.reviver = function(k,v) { if(v == null) return undefined; return v; };
	
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
    this.subscribe=function(location) {
		this.send({
            operation:"SUBSCRIBE",
            location:location,
        },"QUERY");
    }
    
    this.unsubscribe=function(location) {
		this.send({
            operation:"UNSUBSCRIBE",
            location:location,
        },"QUERY");
    }
	
	/* lock an object */
	this.lock = function(location,lock_type) {
		this.send({
            location:location,
			operation:"LOCK",
			data:lock_type
        },"QUERY");
	}
	
	/* unlock an object */ 
	this.unlock = function(location) {
		this.send({
            location:location,
			operation:"LOCK",
			data:-1,
        },"QUERY");
	}
    
	/* read object data (lock for reading or writing, blocking) */
    this.read=function(location,lock_type) {
		this.send({
            operation:"READ",
            location:location,
			lock:lock_type
        },"QUERY");
		return this.wait("RESPONSE");
    }
	
	/* shift object data (lock for reading or writing, blocking) */
    this.shift=function(location,lock_type) {
		this.send({
            operation:"SHIFT",
            location:location,
			lock:lock_type
        },"QUERY");
		return this.wait("RESPONSE");
    }

	/* pop object data (lock for reading or writing, blocking) */
    this.pop=function(location,lock_type) {
		this.send({
            operation:"POP",
            location:location,
			lock:lock_type
        },"QUERY");
		return this.wait("RESPONSE");
    }
    
	/* store object data (lock for writing) */
    this.write=function(location,obj,lock_type) {
        this.send({
            operation:"WRITE",
            location:location,
            data:obj,
			lock:lock_type
        },"QUERY");
    }

	/* unshift object data (lock for writing) */
    this.unshift=function(location,obj,lock_type) {
        this.send({
            operation:"UNSHIFT",
            location:location,
            data:obj,
			lock:lock_type
        },"QUERY");
    }

	/* push object data (lock for writing) */
    this.push=function(location,obj,lock_type) {
        this.send({
            operation:"PUSH",
            location:location,
            data:obj,
			lock:lock_type
        },"QUERY");
    }
	
	/* package an object and send through the socket */
	this.send=function(obj,func) {
		var packet={
			func:func,
			data:obj
		};
		this.socket.sendJSON(packet);
	}

	/* receive a data packet */
	this.receive=function() {
		if(!this.socket.is_connected)
			throw("socket disconnected");
		if(!this.socket.data_waiting) 
			return false;
		else
			return this.socket.recvJSON();
	}
	
	/* do not return until the expected response is received */
	this.wait=function(func) {
		var start = time();
		do {
			var response = this.receive();
			
			if(!response)
				continue;
			else if(response.func == func) 
				return response.data;
			else
				this.callback(response.data);
				
		} while(time() - start < this.settings.RECV_TIMEOUT);
		
		log(LOG_ERROR,"timed out waiting for response");
		exit();
	}

	/* check socket for data, and process it if a callback is specified */
	this.cycle=function() {
		var packet=this.receive();
		if(!packet)
			return false;
		else if(this.callback)
			this.callback(packet.data);
		else 
			this.updates.push(packet.data);
	}
	
	/* retrieve the overall lock and subscription status of an object */
	this.status=function(location) {
		this.send({
			operation:"STATUS",
			location:location
		},"QUERY");
		return this.wait("RESPONSE");
	}
	
	this.connect();
	log(LOG_INFO,"JSON client initialized (v" + this.VERSION + ")");
};















