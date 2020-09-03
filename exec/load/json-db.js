/* $Id: json-db.js,v 1.40 2015/11/04 09:43:45 deuce Exp $ */

/*     
	JSON database  - for Synchronet 3.15a+ (2011)

	-	code by mcmlxxix
	-	model concept by deuce
	-	dongs by echicken
	-	positive reinforcement by cyan
	-	function names by weatherman
	-	mild concern by digitalman 

	methods:
	
	-	Database.status(client,location);
	-	Database.who(client,location); 
	-	Database.query(client,query);
	-	Database.cycle(Date.now());
	-	Database.load();
	-	Database.save(Date.now());
	-	Database.lock(client,record,property,lock_type);
	-	Database.read(client,record,property);
	-	Database.pop(client,record,property);
	-	Database.shift(client,record,property);
	-	Database.write(client,record,property,data);
	-	Database.push(client,record,property,data);
	-	Database.unshift(client,record,property,data);
	-	Database.remove(client,record,property);
	-	Database.subscribe(client,record,property);
	-	Database.unsubscribe(client,record,property);
	
	optional arguments:
	
	-	argv[0] = filename;
	
*/

function JSONdb (fileName, scope) {
	this.VERSION = "$Revision: 1.40 $".replace(/\$/g,'').split(' ')[1];
	
    /* database storage file */
	if(fileName) 
		this.file=new File(fileName);
    else 
		this.file=undefined;
	
    /* master database object */
	this.masterData={
		/* top-level data object */
		data:{}
	};

    /* master shadow object */
	this.masterShadow={
		/* top-level data object */
		data:{}
	};
	
	/* queued array of data requests (get/put/create/delete) */
	this.queue={};
	
	/* general list of db subscribers (for quick release if no subscriptions) */
	this.subscriptions={};
	
	/* list of disconnected clients */
	this.disconnected={};
	
	/* database settings */
	this.settings={
		/* misc settings */
		FILE:system.data_dir + "json-db.ini",
		FILE_BUFFER:524288,
		LAST_SAVE:-1,
		SAVE_INTERVAL:-1,
		KEEP_READABLE:false,
		READ_ONLY:false,
		UPDATES:false,
		DEFAULT_TIMEOUT:0
	};
	
	/* lock constants */
	var locks = {
		UNLOCK:-1,
		NONE:undefined,
		READ:1,
		WRITE:2,
	}
	
	/* operation constants */
	var opers = {
		READ:0,
		WRITE:1,
		LOCK:2,
		PUSH:3,
		POP:4,
		SHIFT:5,
		UNSHIFT:6,
		DELETE:7,
		SUBSCRIBE:8,
		UNSUBSCRIBE:9,
		WHO:10,
		STATUS:11,
		KEYS:12,
		SLICE:13,
		KEYTYPES:14,
		SPLICE:15
	}
	
	/* error constants */
	var errors = {
		INVALID_REQUEST:0,
		OBJECT_NOT_FOUND:1,
		NOT_LOCKED:2,
		LOCKED_WRITE:3,
		LOCKED_READ:4,
		DUPLICATE_LOCK:5,
		DUPLICATE_SUB:6,
		NON_ARRAY:7,
		READ_ONLY:8,
		INVALID_LOCK:9,
		INVALID_OPER:10
	};
	
	/*************************** database methods ****************************/
    /* subscribe a client to an object */
    this.subscribe = function(request,record) {
 		var client = request.client;
		/*  TODO: expire existing subscribers after  certain amount of time, maybe */
		if(!this.subscriptions[client.id]) {
			this.subscriptions[client.id] = {};
		}
		if(!this.subscriptions[client.id][record.location]) {
	        record.shadow[record.property]._subscribers[client.id]=client;
			this.subscriptions[client.id][record.location]=record;
			record.subtime = Date.now();
			send_subscriber_updates(client,record,"SUBSCRIBE");
		}
		else if(this.subscriptions[client.id][record.location]) {
			log(LOG_WARNING,"duplicate subscription: " + client.id + "@" + record.location);
			//this.error(client,errors.DUPLICATE_SUB);
		}
		return true;
    };
    
    /* unsubscribe a client from an object */
    this.unsubscribe = function(request,record) {
		var client = request.client;
		if(this.subscriptions[client.id] && this.subscriptions[client.id][record.location]) {
			delete record.shadow[record.property]._subscribers[client.id];
			delete this.subscriptions[client.id][record.location];
			if(count(this.subscriptions[client.id]) == 0)
				delete this.subscriptions[client.id];
			send_subscriber_updates(client,record,"UNSUBSCRIBE");
		}
		else {
			log(LOG_WARNING,"invalid subscription: " + client.id + "@" + record.location);
			//this.error(client,errors.INVALID_REQUEST);
		}
		return true;
    };
    
    /* The point of a read lock is *only* to prevent someone from
	getting a write lock... just a simple counter is usually enough. */
    this.lock = function(request,record) {
		var client = request.client;
		switch(request.data) {
		/* if the client wants to read... */
		case locks.READ:
			/* if this is a duplicate lock attempt */
			if(record.info.lock[client.id]) {
				this.error(client,errors.DUPLICATE_LOCK);
				return true;
			}
			switch(record.info.lock_type) {
			case locks.READ:
				/* if there are any pending write locks, deny */
				if(record.info.lock_pending) {
					return false;
				}
				/* otherwise, we can read lock */
				else {
					record.shadow[record.property]._lock[client.id] = new Lock(
						request.data,
						Date.now()
					);
					return true;
				}
			/* we cant lock a record that is already locked for reading */
			case locks.WRITE:
				return false;
			/* if the record isnt locked at all, we can lock */
			case locks.NONE:
				record.shadow[record.property]._lock[client.id] = new Lock(
					request.data,
					Date.now()
				);
				return true;
			}
			break;
		/* if the client wants to write... */
		case locks.WRITE:
			/* if this db is read-only */
			if(this.settings.READ_ONLY) {
				this.error(client,errors.READ_ONLY);
				return true;
			}
			/* if this is a duplicate lock attempt */
			if(record.info.lock[client.id]) {
				this.error(client,errors.DUPLICATE_LOCK);
				return true;
			}
			switch(record.info.lock_type) {
			/* ...and the record is already locked, flag for pending write lock */
			case locks.READ:
			case locks.WRITE:
				record.shadow[record.property]._lock_pending=true;
				return false;
			/* ...and the record isnt locked, lock for writing and remove flag */
			case locks.NONE:
				record.shadow[record.property]._lock[client.id] = new Lock(
					request.data,
					Date.now()
				);
				record.shadow[record.property]._lock_pending=false;
				return true;
			}
			break;
		/* if the client wants to unlock, check credentials */
		case locks.UNLOCK:
			var client_lock=record.shadow[record.property]._lock[client.id];
			/* if the client has a lock on this record, release the lock */
			if(client_lock) {
				/* if this was a write lock, send an update to all record subscribers */
				if(client_lock.type == locks.WRITE) {
					this.settings.UPDATES=true;
					send_data_updates(client,record,this.scope);
				}
				delete record.shadow[record.property]._lock[client.id];
				return true;
			}
			/* otherwise deny */
			else {
				this.error(client,errors.NOT_LOCKED);
				return true;
			}
		case locks.NONE:
			return true;
			break
		}
    };
	
    /* server's data retrieval method (not directly called by client) */    
    this.read = function(request,record) {
		var client = request.client;
		/* if the requested data does not exist, result is undefined */
		if(record.data === undefined) {
            send_packet(client,"RESPONSE","READ",record.location,undefined);
			return true;
		}
		/* if this client has this record locked, read */
		else if(record.info.lock[client.id]) {
            send_packet(client,"RESPONSE","READ",record.location,record.data[record.property]);
			return true;
		}
        /* if there is no lock for this client, error */
        else {
            return false;
        }
    };
    
	/* pop a record off the end of an array */
	this.pop = function(request,record) {
		var client = request.client;
		/* if the requested data does not exist, result is undefined */
		if(record.data === undefined) {
            send_packet(client,"RESPONSE","POP",record.location,undefined);
			return true;
		}
		/* if this client has this record locked  */
		else if(record.info.lock[client.id] && 
			record.info.lock[client.id].type == locks.WRITE) {
			if(record.data[record.property] instanceof Array) 
				send_packet(client,"RESPONSE","POP",record.location,record.data[record.property].pop());
			else
				this.error(client,errors.NON_ARRAY);
			return true;
		}
        /* if there is no lock for this client, error */
        else {
            return false;
        }
	}
	
	/* shift a record off the end of an array */
	this.shift = function(request,record) {
		var client = request.client;
		/* if the requested data does not exist, result is undefined */
		if(record.data === undefined) {
            send_packet(client,"RESPONSE","SHIFT",record.location,undefined);
			return true;
		}
		/* if this client has this record locked  */
		else if(record.info.lock[client.id] && 
		record.info.lock[client.id].type == locks.WRITE) {
			if(record.data[record.property] instanceof Array) 
				send_packet(client,"RESPONSE","SHIFT",record.location,record.data[record.property].shift());
			else
				this.error(client,errors.NON_ARRAY);
			return true;
		}
        /* if there is no lock for this client, error */
        else {
            return false;
        }
	}

	/* push a record onto the end of an array */
	this.push = function(request,record) {
		var client = request.client;
		var data = request.data;
		/* if the requested data does not exist, result is undefined */
		if(record.data === undefined) {
			this.error(client,errors.OBJECT_NOT_FOUND);
			return true;
		}
		/* if this client has this record locked  */
		else if(record.info.lock[client.id] && 
			record.info.lock[client.id].type == locks.WRITE) {
			if(record.data[record.property] instanceof Array) {
				var index = record.data[record.property].length;
				record.data[record.property].push(data);
				/* populate this object's children with shadow objects */
				composite_sketch(record.data[record.property][index],record.shadow[record.property][index]);
			}
			else
				this.error(client,errors.NON_ARRAY);
			return true;
		}
        /* if there is no lock for this client, error */
        else {
            return false;
        }
	}

	/* splice an array */
	this.splice = function(request,record) {
		var client = request.client;
		var data = request.data;
		/* if the requested data does not exist, result is undefined */
		if(record.data === undefined) {
			this.error(client,errors.OBJECT_NOT_FOUND);
			return true;
		}
		/* if this client has this record locked  */
		else if(record.info.lock[client.id] && 
			record.info.lock[client.id].type == locks.WRITE) {
			if(record.data[record.property] instanceof Array) {
				record.data[record.property].splice(request.data.start,request.data.num,request.data.data);

				/* remove existing shadow records that have been replaced by new data */
				// if(record.shadow[record.property] instanceof Array) 
					// record.shadow[record.property].splice(request.data.start,request.data.num,new Shadow());
				/* populate this object's children with shadow objects */
				composite_sketch(record.data[record.property][request.data.start],record.shadow[record.property][request.data.start]);
			}
			else {
				this.error(client,errors.NON_ARRAY);
			}
			return true;
		}
        /* if there is no lock for this client, error */
        else {
            return false;
        }
	}
	
	/* push a record onto the end of an array */
	this.unshift = function(request,record) {
		var client = request.client;
		var data = request.data;
		/* if the requested data does not exist, result is undefined */
		if(record.data === undefined) {
			this.error(client,errors.OBJECT_NOT_FOUND);
			return true;
		}
		/* if this client has this record locked  */
		else if(record.info.lock[client.id] && 
			record.info.lock[client.id].type == locks.WRITE) {
			if(record.data[record.property] instanceof Array) {
				var index = record.data[record.property].length;
				record.data[record.property].unshift(data);
				/* populate this object's children with shadow objects */
				composite_sketch(record.data[record.property][index],record.shadow[record.property][index]);
			}
			else
				this.error(client,errors.NON_ARRAY);
			return true;
		}
        /* if there is no lock for this client, error */
        else {
            return false;
        }
	}
	
	/* shift a record off the end of an array */
	this.slice = function(request,record) {
		var client = request.client;
		/* if the requested data does not exist, result is undefined */
		if(record.data === undefined) {
			send_packet(client,"RESPONSE","SLICE",record.location,undefined);
			return true;
		}
		/* if this client has this record locked  */
		else if(record.info.lock[client.id]) {
			if(record.data[record.property] instanceof Array) {
				var d = record.data[record.property].slice(request.data.start,request.data.end);
				send_packet(client,"RESPONSE","SLICE",record.location,d);
			}
			else {
				this.error(client,errors.NON_ARRAY);
			}
			return true;
		}
        /* if there is no lock for this client, error */
        else {
            return false;
        }
	}
	
    /* server's data submission method (not directly called by client) */    
    this.write = function(request,record) {
		var client = request.client;
		var data = request.data;
		/* if this client has this record locked  */
		if(record.info.lock[client.id] && 
		record.info.lock[client.id].type == locks.WRITE) {
			record.data[record.property]=data;
			/* populate this object's children with shadow objects */
			composite_sketch(record.data[record.property],record.shadow[record.property]);
			/* send data updates to all subscribers */
			return true;
		}
        /* if there is no lock for this client, error */
        else {
            return false;
        }
    };

	/* retrieve a list of object keys */
	this.keys = function(request,record) {
		var client = request.client;
		var keys=[];
		/* if the requested data does not exist, result is undefined */
		if(record.data === undefined) {
			send_packet(client,"RESPONSE","KEYS",record.location,undefined);
			return true;
		}
		/* if this client has this record locked, read */
		if(record.info.lock[client.id]) {
			for(var k in record.data[record.property])
				keys.push(k);
			send_packet(client,"RESPONSE","KEYS",record.location,keys);
			return true;
		}
        /* if there is no lock for this client, error */
        else {
            return false;
        }
	}
 
	/* retrieve a list of object keys */
	this.keyTypes = function(request,record) {
		var client = request.client;
		var keys={};
		/* if the requested data does not exist, result is undefined */
		if(record.data === undefined) {
			send_packet(client,"RESPONSE","KEYTYPES",record.location,undefined);
			return true;
		}
		/* if this client has this record locked, read */
		if(record.info.lock[client.id]) {
			for(var k in record.data[record.property]) {
				var type = typeof record.data[record.property][k];
				if(record.data[record.property][k] instanceof Array)
					type = "array";
				keys[k] = type;
			}
			send_packet(client,"RESPONSE","KEYTYPES",record.location,keys);
			return true;
		}
        /* if there is no lock for this client, error */
        else {
            return false;
        }
	}
 
    /* remove a record from the database (requires WRITE_LOCK) */
    this.remove = function(request,record) {
		var client = request.client;
		
		/* if the requested data does not exist, do nothing */
		if(record.data === undefined || !record.data.hasOwnProperty(record.property)) {
			return true;
		}
		
		/* if this client has this record locked */
		else if(record.shadow[record.property]._lock[client.id] && 
			record.shadow[record.property]._lock[client.id].type == locks.WRITE) {
			delete record.data[record.property];
			return true;
		}
        /* if there is no lock for this client, error */
        else {
			this.error(client,errors.NOT_LOCKED);
            return false;
        }
    };

	/* return an object representing a record's overall subscriber and lock status */
	this.status = function(request,record) {
		var client = request.client;
		var sub=[];
		for(var s in record.info.subscribers) 
			sub.push(s);
		var data={
			subscribers:sub,
			lock:record.info.lock_type,
			pending:record.info.lock_pending
		};
		send_packet(client,"RESPONSE","STATUS",record.location,data);
		return true;
	}

	/* return a list of subscriptions and associated IP address for clients */
	this.who = function(request,record) {
		var client = request.client;
		var data = get_subscriber_list(record);
		send_packet(client,"RESPONSE","WHO",record.location,data);
		return true;
	}

	/* generic query handler, will process locks, reads, writes, and unlocks
	and put them into the appropriate queues */
	this.query = function(client,query) {
		
		/* retain original query location for subscriber updates */
		var location = query.location;
		if(location == undefined || location.length == 0)
			location = "data";
		else 
			location = "data." + location;
			
		/* store the parent name */
		var parent = get_pname(location);
		/* strip the last child identifier from the string */
		var property = get_cname(location);
		
		/* temporary array for queue additions */
		var q=[];

		/* if there is a username supplied, attach it to the client object */
		if(query.nick !== undefined) {
			client.nick = query.nick;
		}
		
		/* if there is a username supplied, attach it to the client object */
		if(query.system !== undefined) {
			client.system = query.system;
		}

		/* if the requested operation is invalid, error */
		if(!valid_oper(query.oper)) {
			this.error(client,errors.INVALID_OPER);
			return false;
		}

		/* if the requested lock is invalid, error */
		if(!valid_lock(query.lock)) {
			this.error(client,errors.INVALID_LOCK);
			return false;
		}

		q.push(new Request(
			client,query.oper,query.location,parent,property,query.data,query.timeout
		));
		/* push this query into a queue to be processed at the next response cycle (this.cycle()) */;
		
		
		/* if there is an attached lock operation, process accordingly */
		if(query.lock !== locks.NONE) {	
			/* put lock ahead of the operation in request queue */
			q.unshift(new Request(
				client,"LOCK",query.location,parent,property,query.lock
			));
			/* put unlock after the operation in the request queue */
			q.push(new Request(
				client,"LOCK",query.location,parent,property,locks.UNLOCK
			));
		}
		
		/* add the temporary queue to the main queue */
		if(!this.queue[client.id])
			this.queue[client.id]=[];
		this.queue[client.id]=this.queue[client.id].concat(q);
	}
    
	/* generate an error object and send it to client */
	this.error = function(client,error_num) {
		var error_desc="Unknown error";
		switch(error_num) {
		case errors.INVALID_REQUEST:
			error_desc="Invalid record request";
			break;
		case errors.OBJECT_NOT_FOUND:
			error_desc="Record not found";
			break;
		case errors.NOT_LOCKED:
			error_desc="Record not locked";
			break;
		case errors.LOCKED_WRITE:
			error_desc="Record locked for writing";
			break;
		case errors.LOCKED_READ:
			error_desc="Record locked for reading";
			break;
		case errors.DUPLICATE_LOCK:
			error_desc="Duplicate record lock request";
			break;
		case errors.DUPLICATE_SUB:
			error_desc="Duplicate record subscription request";
			break;
		case errors.NON_ARRAY:
			error_desc="Record is not an array";
			break;
		case errors.READ_ONLY:
			error_desc="Record is read-only";
			break;
		case errors.INVALID_LOCK:
			error_desc="Unknown lock type";
			break;
		case errors.INVALID_OPER:
			error_desc="Unknown operation";
			break;
		}
		var error=new Error(error_num,error_desc);
		send_packet(client,"ERROR",undefined,undefined,error);
	}
	
    /* internal periodic data storage method 
	TODO: this should probably eventually be a background
	thread to prevent lag when writing a large database to file */
    this.save = function() { 
		if(!this.file)
			return;
		/* if we are due for a data update, save everything to file */
		//TODO: create n backups before overwriting data file
		if(!this.file.open("w",false))
			return;
		// This function gets called every 30 seconds or so
		// And flushes all objects to disk in case of crash
		// Also, this is called on clean exit.
		if(this.settings.KEEP_READABLE)
			this.file.write(JSON.stringify(this.masterData.data,undefined,'\t'));
		else 
			this.file.write(JSON.stringify(this.masterData.data));
		this.file.close();
		this.settings.LAST_SAVE=Date.now();
		this.settings.UPDATES=false;
    };
    
    /* data initialization (likely happens only once) */
    this.load = function() { 
		if(!this.file)
			return;
		if(!file_exists(this.file.name))
			return;
		if(!this.file.open("r",true))
			return;
		var data = this.file.readAll(this.settings.FILE_BUFFER).join('\n');
		this.masterData.data = JSON.parse(data);
		this.file.close(); 
        this.masterShadow.data=composite_sketch(this.masterData.data);
    };
	
	/* initialize db and settings */
	this.init = function() {
		/* load general db settings */
		if(file_exists(this.settings.FILE)) {
			var file = new File(this.settings.FILE);
			file.open("r",true);
			var settings = file.iniGetObject();
			file.close(); 
			if(settings.save_interval)
				this.settings.SAVE_INTERVAL = Number(settings.save_interval);
			if(settings.keep_readable)
				this.settings.KEEP_READABLE = Boolean(settings.keep_readable);
		}

        /* initialize autosave timer */
        this.settings.LAST_SAVE = Date.now();
		log(LOG_INFO,"database initialized (v" + this.VERSION + "): " + fileName);
	};
 
	/* schedule client for subscription and lock release */
	this.release = function(client) {
		this.disconnected[client.id] = client;
	}
 
    /* main "loop" called by server */
    this.cycle = function() {
		/* process request queue, removing successful operations */
		for(var c in this.queue) {
			if(!this.queue[c] || this.queue[c].length == 0) {
				delete this.queue[c];
				continue;
			}
			for(var r=0;r<this.queue[c].length;r++) {
				var request=this.queue[c][r];
				var result=false;
				
				/* locate the requested record within the database */
				var record=identify_remains.call(
					this,request.oper,request.location,request.parent,request.property
				);
				
				/* if there was an error parsing object location, delete request */
				if(!record) {
					log(LOG_WARNING,"db: bad request removed from queue");
					this.error(request.client,errors.INVALID_REQUEST);
					this.queue[c].splice(r--,1);
					continue;
				}
				
				switch(request.oper.toUpperCase()) {
				case "READ":
					result=this.read(request,record);
					break;
				case "POP":
					result=this.pop(request,record);
					break;
				case "SHIFT":
					result=this.shift(request,record);
					break;
				case "WRITE":
					result=this.write(request,record);
					break;
				case "KEYS":
					result=this.keys(request,record);
					break;
				case "KEYTYPES":
					result=this.keys(request,record);
					break;
				case "PUSH":
					result=this.push(request,record);
					break;
				case "UNSHIFT":
					result=this.unshift(request,record);
					break;
				case "SPLICE":
					result=this.splice(request,record);
					break;
				case "SLICE":
					result=this.slice(request,record);
					break;
				case "DELETE":
					result=this.remove(request,record);
					break;
				case "SUBSCRIBE":
					result=this.subscribe(request,record);
					break;
				case "UNSUBSCRIBE":
					result=this.unsubscribe(request,record);
					break;
				case "LOCK":
					result=this.lock(request,record);
					break;
				case "STATUS":
					result=this.status(request,record);
					break;
				case "WHO":
					result=this.who(request,record);
					break;
				default:
					this.error(client,errors.INVALID_REQUEST);
					this.queue[c].splice(r--,1);
					break;
				}
				
				/* if the request did not succeed, move to the next user queue */
				if(!check_result(request,record,result))
					break;
				this.queue[c].splice(r--,1);
			}
		} 
		
		/* terminate any disconnected clients after processing queue */
		for each(var c in this.disconnected) {
			/* release any locks the client holds */
			free_prisoners(c,this.masterShadow);
			
			/* release any subscriptions the client holds */
			cancel_subscriptions(c,this.subscriptions);
			
			/* remove any remaining client queries */
			fuh_queue(c,this.queue);
		}
		
		/* reset disconnected client object */
		this.disconnected={};
		
		if(!this.settings.UPDATES)
			return;
		if(!this.settings.SAVE_INTERVAL > 0)
			return;
        if(Date.now() - this.settings.LAST_SAVE < (this.settings.SAVE_INTERVAL * 1000)) 
			return;
		this.save();
	};

	/**************************** database objects *****************************/
	/* locking object generated by Database.lock() method 
	contains the type of lock requested, and the time at which the request came in
	TODO: possibly "expire" unfulfilled lock requests after a certain period */
	function Lock(lock_type,timestamp) {
		this.type=lock_type;
		this.timestamp=timestamp;
	}

	/* error object containing a description of the error */
	function Error(error_num,error_desc) {
		this.num=error_num;
		this.description=error_desc;
	}
	
	/* shadow properties generated by composite_sketch() 
	contains a relative object's subscribers, locks, and child name */
	function Shadow() {
		this._lock={};
		this._lock_pending=false;
		this._subscribers={};
	}

	/* record object returned by identify_remains() 
	contains the requested object, its shadow object,
	the prevailing lock state of that object (affected by parents/children),
	and a list of subscribers associated with that object or its parents */
	function Record(data,shadow,location,property,info) {
		this.data=data;
		this.shadow=shadow;
		this.location=location;
		this.property=property;
		this.info=info;
	}

	/* request object generated by queue() method
	contains the requested object parent, the specific child property requested,
	data (in the case of a PUT operation ) */
	function Request(client,operation,location,parent,property,data,timeout) {
		this.client=client;
		this.oper=operation;
		this.location=location;
		this.parent=parent;
		this.property=property;
		this.data=data;
		this.time=Date.now();
		this.timeout=timeout;
	}

	/*************************** database functions ****************************/
	/*  traverse object and create a shadow copy of the object structure
	for record locking and subscribers, and create location names for database objects */
	function composite_sketch(obj,shadow)  {
		/* create shadow object */
		if(!shadow)
			shadow=new Shadow();
		   
		/* iterate object members */
		for(var p in obj) {
			if(typeof obj[p] == "object")
				shadow[p]=composite_sketch(obj[p],shadow[p]);
		}
		
		/*  returns an object containing the passed objects property keys
		with their own lock, subscribers, and name properties
		also adds a location property to keys of original object */
		
		return shadow;
	}

	/* parse an object location name and return the object (ex: dicewarz2.games.1.players.1.tiles.0)
	an object containing the corresponding data and its shadow object */
	function identify_remains(oper,location,parent,property) {
	
		var object=this.masterData;
		var shadow=this.masterShadow;
		var info={
			lock:{},
			lock_type:locks.NONE,
			lock_pending:false,
			subscribers:{}
		};

		if(parent !== undefined) {
			/* iterate through split object name checking the keys against the database and 
			checking the lock statuses against the shadow copy */
			var p=parent.split(/\./);
			for each(var c in p) {
				/* in the event of a write request, create new data if it does not exist*/
				/* ensure that the shadow object exists in order to allow for non-read operations */
				if(shadow[c] === undefined) 
					create_shadow(shadow,c);
				shadow=shadow[c];
				/* keep track of current object, and store the immediate parent of the request object */
				if(object !== undefined) {
					if(object[c] === undefined && oper == "WRITE")
						create_data(object,c);
					object=object[c];
				}
				/* check the current object's lock and subscriber status along the way */
				info = investigate(shadow,info);
			}
		}

		/* ensure requested shadow object's existance */
		if(shadow[property] === undefined || shadow[property]._lock === undefined) 
			create_shadow(shadow,property);
			
		/* continue on through the selected shadow object's children to check for locked children */
		info = search_party(shadow[property],info);
		
		/* return selected database object, shadow object, and overall lock status of the chosen tree */
		return new Record(object,shadow,location,property,info);
	}
	
	/* if the requested child object does not exist, create it */
	function create_shadow(shadow,property) {
		log(LOG_DEBUG,"creating new shadow: " + property);
		shadow[property]=new Shadow();
	}
	
	/* if the requested child object does not exist, create it */
	function create_data(data,property) {
		log(LOG_DEBUG,"creating new data: " + property);
		data[property]={};
	}
	
	/* release locks on an object recursively */
	function free_prisoners(client,shadow) {
		if(shadow._lock && shadow._lock[client.id]) {
			log(LOG_DEBUG,"releasing lock: " + client.id);
			delete shadow._lock[client.id];
		}
		for(var s in shadow) {
			if(typeof shadow[s] == "object") 
				free_prisoners(client,shadow[s]);
		}
	}
	
	/* remove any remaining client queries from queue */
	function fuh_queue(client,queue) {
		while(queue[client.id] && queue[client.id].length > 0) {
			var query = queue[client.id].shift();
			log(LOG_DEBUG,format("removing query: %s %s.%s",
				query.oper,query.parent,query.property));
		}
		delete queue[client.id];
	}
	
	/* release subscriptions on an object recursively */
	function cancel_subscriptions(client,subscriptions) {
		var records = subscriptions[client.id];
		for(var r in records) {
			var record = records[r];
			log(LOG_DEBUG,"releasing subscription: " + record.location);
			delete record.shadow[record.property]._subscribers[client.id];
			send_subscriber_updates(client,record,"UNSUBSCRIBE");
		}
		delete subscriptions[client.id];
	}
	
	/* return the prevailing lock type and pending lock status for an object */
	function investigate(shadow, info) {
		/* if we havent found a write locked record yet, keep searching */
		if(info.lock_type == undefined) {
			for(var l in shadow._lock) {
				info.lock_type = shadow._lock[l].type;
				info.lock[l] = shadow._lock[l];
			}
		}
		for(var s in shadow._subscribers) {
			info.subscribers[s]=shadow._subscribers[s];
		}
		info.lock_pending=shadow._lock_pending;
		return info;
	}

	/* recursively search object for any existing locked children, 
	return highest lock level (WRITE > READ > NONE)  */
	function search_party(shadow,info) {
		if(shadow == undefined)
			return info;
		info = investigate(shadow,info);
		
		for each(var i in shadow) 
			if(i instanceof Shadow)
				info = search_party(i,info);
		return info;
	}

	/* check the result of a request */
	function check_result(request,record,result) {
		if(result) {
			log(LOG_DEBUG,format("db: %s %s %s OK",
				request.client.id,request.oper,record.location));  
			if(request.timeout >= 0) {
				switch(request.oper.toUpperCase()) {
				case "WRITE":
				case "PUSH":
				case "UNSHIFT":
				case "DELETE":
				case "SUBSCRIBE":
				case "UNSUBSCRIBE":
				case "LOCK":
					//notify client of success on non-read operations
					send_packet(request.client,"RESPONSE",request.oper,record.location,true);
					break;
				}
			}
			return true;
		}
		else {
			log(LOG_DEBUG,format("db: %s %s %s FAILED",
				request.client.id,request.oper,record.location));
			if(request.timeout >= 0 && timeout_expired(request)) {
				send_packet(request.client,"RESPONSE",request.oper,record.location,false);
				return true;
			}
			return false;
		}
	}
	
	/* send updates of this object to all subscribers */
	function send_data_updates(client,record) {
		for each(var c in record.info.subscribers) {
			/* do not send updates to request originator */
			if(c.id == client.id)
				continue;
			send_packet(c,"UPDATE","WRITE",record.location,get_record_data(record));
		}
	}
	
	/* retrieve record data, if present */
	function get_record_data(record) {
		if(record.data === undefined)
			return undefined;
		return record.data[record.property];
	}
	
	/* send update of client subscription to all subscribers */
	function send_subscriber_updates(client,record,oper) {
		for each(var c in record.info.subscribers) {
			/* do not send updates to request originator */
			if(c.id == client.id)
				continue;
			send_packet(c,"UPDATE",oper,record.location,get_client_info(client));
		}
	}
	
	/* retrieve client nickname and system name, if present, from client socket */
	function get_client_info(client) {
		return {
			id:client.id,
			nick:client.nick,
			system:client.system
		}
	}
	
	/* retrieve a list of subscribers to this record */
	function get_subscriber_list(record) {
		var data = [];
		for each(var s in record.info.subscribers) {
			data.push(get_client_info(s));
		}
		return data;
	}

	/* parse parent object name from a dot-delimited string */
	function get_pname(str) {
		var i = str.lastIndexOf('.');
		if(i > 0)
			return str.substr(0,i);
		return undefined;
	}
	
	/* check a lock value against valid lock types */
	function valid_lock(lock) {
		for each(var l in locks) {
			if(l == lock) 
				return true;
		}
		return false;
	}
	
	/* check an operation against valid operation types */
	function valid_oper(oper) {
		if(opers[oper] !== undefined)
			return true;
		return false;
	}
	
	/* parse child object name from a dot-delimited string */
	function get_cname(str) {
		var i = str.lastIndexOf('.');
		if(i > 0)
			return str.substr(i+1);
		return str;
	}

	/* calculate timeout */
	function timeout_expired(request) {
		return (Date.now() - request.time >= request.timeout);
	}
	
	/* count object members */
	function count(obj) {
		var c=0;
		for(var i in obj)
			c++;
		return c;
	}
	
	/* send data packet to a client */
	function send_packet(client,func,oper,location,data) {
		var packet = {
			scope:scope,
			func:func,
			oper:oper,
			location:location,
			data:data
		};
		client.sendJSON(packet);
	}

	/* constructor */
	this.init();
	this.load();
};




