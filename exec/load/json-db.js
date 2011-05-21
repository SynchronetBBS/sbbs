/*     
	JSON database  - for Synchronet 3.15a+ (2011)

	-	code by mcmlxxix
	-	model concept by deuce
	-	dongs by echicken
	-	positive reinforcement by cyan
	-	function names by weatherman
	-	mild concern by digitalman 

	methods:
	
	-	Database.query(client,query);
	-	Database.cycle(Date.now());
	-	Database.load();
	-	Database.save(Date.now());
	-	Database.lock(client,record,child_name,lock_type);
	-	Database.read(client,record,child_name);
	-	Database.write(client,record,child_name,data);
	-	Database.delete(client,record,child_name);
	-	Database.subscribe(client,record,child_name);
	-	Database.unsubscribe(client,record,child_name);
	
	optional arguments:
	
	-	argv[0] = filename;
	
*/

function JSONdb (fileName) {
	this.VERSION = "$Revision$".split(' ')[1];
	
    /* database storage file */
	if(fileName) 
		this.file=new File(fileName);
    else 
		this.file=undefined;
		
    /* master database object */
    this.data={};
    
    /* database record subscription and locking object */
    this.shadow={};
	
	/* queued array of data requests (get/put/create/delete) */
	this.queue=[];
	
	/* database settings */
	this.settings={

		/* record lock constants, incremental (do not change the order or value)  */
		LOCK_UNLOCK:-1,
		LOCK_NONE:undefined,
		LOCK_READ:1,
		LOCK_WRITE:2,
		
		/* file read buffer */
		FILE_BUFFER:65535,
		
		/* autosave interval */
		AUTO_SAVE:30 /*seconds*/ *1000,
		LAST_SAVE:-1,
		UPDATES:false,
		
		/* error constants */
		ERROR_INVALID_REQUEST:0,
		ERROR_OBJECT_NOT_FOUND:1,
		ERROR_NOT_LOCKED:2,
		ERROR_LOCKED_WRITE:3,
		ERROR_LOCKED_READ:4,
		ERROR_DUPLICATE_LOCK:5
	};
	
	/*************************** database methods ****************************/
    /* subscribe a client to an object */
    this.subscribe = function(client,record) {
        record.shadow[record.child_name]._subscribers[client.id]=client;
        /*  TODO: track duplicate subscribers (deny?) and 
		expire existing subscribers after  certain amount of time, maybe */
		return true;
    };
    
    /* unsubscribe a client from an object */
    this.unsubscribe = function(client,record) {
        delete record.shadow[record.child_name]._subscribers[client.id];
        /*  TODO: validate existing subscription 
		before attempting to delete */
		return true;
    };
    
    /* The point of a read lock is *only* to prevent someone from
	getting a write lock... just a simple counter is usually enough. */
    this.lock = function(client,record,lock_type) {
		switch(lock_type) {
		/* if the client wants to read... */
		case this.settings.LOCK_READ:
			/* if this is a duplicate lock attempt */
			if(record.info.lock[client.id]) {
				this.error(client,this.settings.ERROR_DUPLICATE_LOCK);
				return true;
			}
			switch(record.info.lock_type) {
			case this.settings.LOCK_READ:
				/* if there are any pending write locks, deny */
				if(record.info.lock_pending) {
					return false;
				}
				/* otherwise, we can read lock */
				else {
					record.shadow[record.child_name]._lock[client.id] = new Lock(
						lock_type,
						Date.now()
					);
					return true;
				}
			/* we cant lock a record that is already locked for reading */
			case this.settings.LOCK_WRITE:
				return false;
			/* if the record isnt locked at all, we can lock */
			case this.settings.LOCK_NONE:
				record.shadow[record.child_name]._lock[client.id] = new Lock(
					lock_type,
					Date.now()
				);
				return true;
			}
			break;
		/* if the client wants to write... */
		case this.settings.LOCK_WRITE:
			/* if this is a duplicate lock attempt */
			if(record.info.lock[client.id]) {
				this.error(client,this.settings.ERROR_DUPLICATE_LOCK);
				return true;
			}
			switch(record.info.lock_type) {
			/* ...and the record is already locked, flag for pending write lock */
			case this.settings.LOCK_READ:
			case this.settings.LOCK_WRITE:
				record.shadow[record.child_name]._lock_pending=true;
				return false;
			/* ...and the record isnt locked, lock for writing and remove flag */
			case this.settings.LOCK_NONE:
				record.shadow[record.child_name]._lock[client.id] = new Lock(
					lock_type,
					Date.now()
				);
				record.shadow[record.child_name]._lock_pending=false;
				return true;
			}
			break;
		/* if the client wants to unlock, check credentials */
		case this.settings.LOCK_UNLOCK:
			var client_lock=record.shadow[record.child_name]._lock[client.id];
			/* if the client has a lock on this record, release the lock */
			if(client_lock) {
				/* if this was a write lock, send an update to all record subscribers */
				if(client_lock.type == this.settings.LOCK_WRITE) {
					this.settings.UPDATES=true;
					send_updates(client,record);
				}
				delete record.shadow[record.child_name]._lock[client.id];
				return true;
			}
			/* otherwise deny */
			else {
				this.error(client,this.settings.ERROR_NOT_LOCKED);
				return true;
			}
		}
		/* fallthrough? */
		return false;
    };
    
    /* server's data retrieval method (not directly called by client) */    
    this.read = function(client,record) {
		/* if this client has this record locked, read */
		if(record.info.lock[client.id]) {
            send_packet(client,record.data[record.child_name],"RESPONSE");
			return true;
		}
        /* if there is no lock for this client, error */
        else {
            return false;
        }
    };
    
    /* server's data submission method (not directly called by client) */    
    this.write = function(client,record,data) {
	
		/* if this client has this record locked  */
		if(record.info.lock[client.id] && 
		record.info.lock[client.id].type == this.settings.LOCK_WRITE) {
			record.data[record.child_name]=data;
			/* populate this object's children with shadow objects */
			composite_sketch(record.data[record.child_name],record.shadow[record.child_name]);
			/* send data updates to all subscribers */
			return true;
		}
        /* if there is no lock for this client, error */
        else {
            return false;
        }
    };
    
    /* remove a record from the database (requires WRITE_LOCK) */
    this.delete = function(client,record) {
		/* if this client has this record locked */
		if(record.shadow[record.child_name]._lock[client.id] && 
		record.shadow[record.child_name]._lock[client.id].type == this.settings.LOCK_WRITE) {
			delete record.data[record.child_name];
			delete record.shadow[record.child_name];
			/* send data updates to all subscribers */
			return true;
		}
        /* if there is no lock for this client, error */
        else {
			this.error(client,this.settings.ERROR_NOT_LOCKED);
            return false;
        }
    };

	/* return an object representing a record's overall subscriber and lock status */
	this.status = function(client,record) {
		var sub=[];
		for(var s in record.info.subscribers) 
			sub.push(s);
		var data={
			subscribers:sub,
			lock:record.info.lock_type,
			pending:record.info.lock_pending,
			location:record.parent_name + "." + record.child_name
		}
		send_packet(client,data,"RESPONSE");
		return true;
	}

	/* generic query handler, will process locks, reads, writes, and unlocks
	and put them into the appropriate queues */
	this.query = function(client,query) {
        /* strip the last child identifier from the string */
        var parent_name = query.location.substring(0,query.location.lastIndexOf("."));
        /* store the child name */
        var child_name = query.location.substr(query.location.lastIndexOf(".")+1);
		
		/* if the data request is invalid or empty, return an error */
		if(!parent_name || !child_name) {
			this.error(client,this.settings.ERROR_INVALID_REQUEST);
			return false;
		}

		/* temporary array for queue additions */
		var q=[];

		/* if an operation is requested */
		if(query.operation !== undefined) {
			request = new Request(client,query.operation,parent_name,child_name,query.data);
			/* push this query into a queue to be processed at the next response cycle (this.cycle()) */;
			q.push(request);
		}
		
		/* if there is an attached lock operation, process accordingly */
		if(query.lock !== undefined) {	
			/* put lock ahead of the operation in request queue */
			q.unshift(new Request(
				client,"LOCK",parent_name,child_name,query.lock
			));
			/* put unlock after the operation in the request queue */
			q.push(new Request(
				client,"LOCK",parent_name,child_name,this.settings.LOCK_UNLOCK
			));
		}
		/* add the temporary queue to the main queue */
		this.queue=this.queue.concat(q);
	}
    
	/* generate an error object and send it to client */
	this.error = function(client,error_num) {
		var error_desc="Unknown error";
		switch(error_num) {
		case this.settings.ERROR_INVALID_REQUEST:
			error_desc="Invalid record request";
			break;
		case this.settings.ERROR_OBJECT_NOT_FOUND:
			error_desc="Record not found";
			break;
		case this.settings.ERROR_NOT_LOCKED:
			error_desc="Record not locked";
			break;
		case this.settings.ERROR_LOCKED_WRITE:
			error_desc="Record locked for writing";
			break;
		case this.settings.ERROR_LOCKED_READ:
			error_desc="Record locked for reading";
			break;
		case this.settings.ERROR_DUPLICATE_LOCK:
			error_desc="Duplicate record lock request";
			break;
		}
		var error=new Error(error_num,error_desc);
		send_packet(client,error,"ERROR");
	}
	
    /* internal periodic data storage method 
	TODO: this should probably eventually be a background
	thread to prevent lag when writing a large database to file */
    this.save = function(timestamp) { 
		if(!this.file)
			return;
		if(!this.settings.UPDATES)
			return;
		if(!timestamp) 
			timestamp = Date.now();
		/* if we are due for a data update, save everything to file */
        if(timestamp - this.settings.LAST_SAVE >= this.settings.AUTO_SAVE) {
			//TODO: create n backups before overwriting data file
			this.file.open("w");
			// This function gets called every 30 seconds or so
			// And flushes all objects to disk in case of crash
			// Also, this is called on clean exit.
			this.file.write(JSON.stringify(this.data));
			this.file.close();
			this.settings.LAST_SAVE=timestamp;
			this.settings.UPDATES=false;
		}
    };
    
    /* data initialization (likely happens only once) */
    this.load = function() { 
		if(!this.file)
			return;
		if(file_exists(this.file.name)) {
			this.file.open("r");
			this.data = JSON.parse(
				this.file.readAll(this.settings.FILE_BUFFER).join('\n')
			);
			this.file.close(); 
		}
    };
	
	/* create a copy of data object keys and give them 
	locking properties */
	this.init = function() {
		this.load();
        this.shadow=composite_sketch(this.data);
		
        /* initialize autosave timer */
        this.settings.LAST_SAVE = Date.now();
		log(LOG_INFO,"JSON database initialized (v" + this.VERSION + ")");
	}
    
	/* release any locks or subscriptions held by a disconnected client */
	this.release = function(client_id) {
		free_prisoner(client_id,this.shadow);
		for(var c=0;c<this.queue.length;c++) {
			if(this.queue[c].client.id == client_id)
				this.queue.splice(c--,1);
		}
	}
	
    /* main "loop" called by server */
    this.cycle = function(timestamp) {
		this.save();
		
		/* process request queue, removing successful operations */
		for(var r=0;r<this.queue.length;r++) {
			var request=this.queue[r];
			var result=false;
			
			/* locate the requested record within the database */
			var record=identify_remains.call(
				this,request.client,request.parent_name,request.child_name
			);
			
			if(!record) {
				log(LOG_DEBUG,"db: bad request removed from queue");
				this.queue.splice(r--,1);
			}
			
			switch(request.operation.toUpperCase()) {
			case "READ":
				result=this.read(request.client,record);
				break;
			case "WRITE":
				result=this.write(request.client,record,request.data);
				break;
			case "DELETE":
				result=this.delete(request.client,record);
				break;
			case "CREATE":
				result=this.create(request.client,record,request.data);
				break;
			case "SUBSCRIBE":
				result=this.subscribe(request.client,record);
				break;
			case "UNSUBSCRIBE":
				result=this.unsubscribe(request.client,record);
				break;
			case "LOCK":
				result=this.lock(request.client,record,request.data);
				break;
			case "STATUS":
				result=this.status(request.client,record);
				break;
			}
			if(result == true) {
				log(LOG_DEBUG,"db: " + 
					request.operation + " " + 
					request.parent_name + "." + 
					request.child_name + " OK"
				);
				this.queue.splice(r--,1);
			}
			else {
				log(LOG_DEBUG,"db: " + 
					request.operation + " " + 
					request.parent_name + "." + 
					request.child_name + " FAILED"
				); 
			}
		} 
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
		this.operation="ERROR";
		this.error_num=error_num;
		this.error_desc=error_desc;
	}
	
	/* shadow properties generated by composite_sketch() 
	contains a relative object's subscribers, locks, and child name */
	function Shadow() {
		this._lock=[];
		this._lock_pending=false;
		this._subscribers=[];
	}

	/* record object returned by identify_remains() 
	contains the requested object, its shadow object,
	the prevailing lock state of that object (affected by parents/children),
	and a list of subscribers associated with that object or its parents */
	function Record(data,shadow,parent_name,child_name,info) {
		this.data=data;
		this.shadow=shadow;
		this.parent_name=parent_name;
		this.child_name=child_name;
		this.info=info;
	}

	/* request object generated by Database.queue() method
	contains the requested object parent, the specific child property requested,
	data (in the case of a PUT operation ) */
	function Request(client,operation,parent_name,child_name,data) {
		this.client=client;
		this.operation=operation;
		this.parent_name=parent_name;
		this.child_name=child_name;
		this.data=data;
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
	function identify_remains(client,parent_name,child_name) {

		var p=parent_name.split(/\./);
		var data=this.data;
		var shadow=this.shadow;
		var info={
			lock:[],
			lock_type:this.settings.LOCK_NONE,
			lock_pending:false,
			subscribers:[]
		}

		/* iterate through split object name checking the keys against the database and 
		checking the lock statuses against the shadow copy */
		for each(var c in p) {
			
			verify(data,shadow,c);

			/* keep track of current object, and store the immediate parent of the request object */
			data=data[c];
			shadow=shadow[c];

			/* check the current object's lock and subscriber status along the way */
			info = investigate(shadow,info);
		}
		
		verify(data,shadow,child_name);
			
		/* continue on through the selected shadow object's children to check for locked children */
		info = search_party(data[child_name],shadow[child_name],info);
		
		/* return selected database object, shadow object, and overall lock status of the chosen tree */
		return new Record(data,shadow,parent_name,child_name,info);
	}
	
	/* if the requested child object does not exist, create it */
	function verify(data,shadow,child_name) {
		if(!data[child_name]) {
			log(LOG_DEBUG,"creating new data: " + child_name);
			data[child_name]={};
		}
		if(!shadow[child_name]) {
			log(LOG_DEBUG,"creating new shadow: " + child_name);
			shadow[child_name]=new Shadow();
		}
	 }
	
	/* release subscriptions and locks on an object recursively */
	function free_prisoner(client_id,shadow) {
		if(shadow._lock) {
			if(shadow._lock[client_id]) {
				log(LOG_DEBUG,"releasing lock: " + client_id);
				delete shadow._lock[client_id];
			}
		}
		if(shadow._subscribers) {
			if(shadow._subscribers[client_id]) {
				log(LOG_DEBUG,"releasing subscriber: " + client_id);
				delete shadow._subscribers[client_id];
			}
		}
		for(var s in shadow) {
			if(typeof shadow[s] == "object") 
				free_prisoner(client_id,shadow[s]);
		}
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
	return highest lock level (LOCK_WRITE > LOCK_READ > LOCK_NONE)  */
	function search_party(data,shadow,info) {
		if(!shadow)
			return info;
			
		info = investigate(shadow,info);
		for(var i in data) {
			info = search_party(data[i],shadow[i],info);
		}
		return info;
	}

	/* send updates of this object to all subscribers */
	function send_updates(client,record) {
		for each(var c in record.info.subscribers) {
			/* do not send updates to request originator */
			if(c.id == client.id)
				continue;
			var data = {
				location:record.parent_name + "." + record.child_name,
				data:record.data[record.child_name]
			};
			send_packet(c,data,"UPDATE");
		}
	}

	/* send data packet to a client */
	function send_packet(client,object,func) {
		var data={
			func:func,
			data:object
		};
		client.sendJSON(data);
	}

	/* constructor */
	this.init();
};



