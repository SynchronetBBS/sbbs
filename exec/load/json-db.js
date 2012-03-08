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
	-	Database.lock(client,record,child_name,lock_type);
	-	Database.read(client,record,child_name);
	-	Database.pop(client,record,child_name);
	-	Database.shift(client,record,child_name);
	-	Database.write(client,record,child_name,data);
	-	Database.push(client,record,child_name,data);
	-	Database.unshift(client,record,child_name,data);
	-	Database.remove(client,record,child_name);
	-	Database.subscribe(client,record,child_name);
	-	Database.unsubscribe(client,record,child_name);
	
	optional arguments:
	
	-	argv[0] = filename;
	
*/

function JSONdb (fileName) {
	this.VERSION = "$Revision$".replace(/\$/g,'').split(' ')[1];
	
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
	
	/* general list of db subscribers (for quick release if no subscriptions) */
	this.subscriptions=[];
	
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
	};
	
	/* lock constants */
	var locks = {
		UNLOCK:-1,
		NONE:undefined,
		READ:1,
		WRITE:2,
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
		READ_ONLY:8
	};
	
	/*************************** database methods ****************************/
    /* subscribe a client to an object */
    this.subscribe = function(client,record) {
        /*  TODO: expire existing subscribers after  certain amount of time, maybe */
		if(!this.subscriptions[client.id]) {
			this.subscriptions[client.id] = {};
		}
		if(!this.subscriptions[client.id][record.location]) {
	        record.shadow[record.child_name]._subscribers[client.id]=client;
			this.subscriptions[client.id][record.location] = record;
			send_subscriber_updates(client,record,"SUBSCRIBE");
		}
		else {
			this.error(client,errors.DUPLICATE_SUB);
		}
		return true;
    };
    
    /* unsubscribe a client from an object */
    this.unsubscribe = function(client,record) {
		if(this.subscriptions[client.id][record.location]) {
			delete record.shadow[record.child_name]._subscribers[client.id];
			delete this.subscriptions[client.id][record.location];
			if(count(this.subscriptions[client.id]) == 0)
				delete this.subscriptions[client.id];
			send_subscriber_updates(client,record,"UNSUBSCRIBE");
		}
		else {
			this.error(client,errors.INVALID_REQUEST);
		}
		return true;
    };
    
    /* The point of a read lock is *only* to prevent someone from
	getting a write lock... just a simple counter is usually enough. */
    this.lock = function(client,record,lock_type) {
		switch(lock_type) {
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
					record.shadow[record.child_name]._lock[client.id] = new Lock(
						lock_type,
						Date.now()
					);
					return true;
				}
			/* we cant lock a record that is already locked for reading */
			case locks.WRITE:
				return false;
			/* if the record isnt locked at all, we can lock */
			case locks.NONE:
				record.shadow[record.child_name]._lock[client.id] = new Lock(
					lock_type,
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
				record.shadow[record.child_name]._lock_pending=true;
				return false;
			/* ...and the record isnt locked, lock for writing and remove flag */
			case locks.NONE:
				record.shadow[record.child_name]._lock[client.id] = new Lock(
					lock_type,
					Date.now()
				);
				record.shadow[record.child_name]._lock_pending=false;
				return true;
			}
			break;
		/* if the client wants to unlock, check credentials */
		case locks.UNLOCK:
			var client_lock=record.shadow[record.child_name]._lock[client.id];
			/* if the client has a lock on this record, release the lock */
			if(client_lock) {
				/* if this was a write lock, send an update to all record subscribers */
				if(client_lock.type == locks.WRITE) {
					this.settings.UPDATES=true;
					send_data_updates(client,record);
				}
				delete record.shadow[record.child_name]._lock[client.id];
				return true;
			}
			/* otherwise deny */
			else {
				this.error(client,errors.NOT_LOCKED);
				return true;
			}
		}
		/* fallthrough? */
		return false;
    };
    
    /* server's data retrieval method (not directly called by client) */    
    this.read = function(client,record) {
		/* if the requested data does not exist, result is undefined */
		if(record.data === undefined) {
            send_packet(client,undefined,"RESPONSE");
			return true;
		}
		/* if this client has this record locked, read */
		else if(record.info.lock[client.id]) {
            send_packet(client,record.data[record.child_name],"RESPONSE");
			return true;
		}
        /* if there is no lock for this client, error */
        else {
            return false;
        }
    };
    
	/* pop a record off the end of an array */
	this.pop = function(client,record) {
		/* if the requested data does not exist, result is undefined */
		if(record.data === undefined) {
            send_packet(client,undefined,"RESPONSE");
			return true;
		}
		/* if this client has this record locked  */
		else if(record.info.lock[client.id] && 
			record.info.lock[client.id].type == locks.WRITE) {
			if(record.data[record.child_name] instanceof Array) 
				send_packet(client,record.data[record.child_name].pop(),"RESPONSE");
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
	this.shift = function(client,record) {
		/* if the requested data does not exist, result is undefined */
		if(record.data === undefined) {
            send_packet(client,undefined,"RESPONSE");
			return true;
		}
		/* if this client has this record locked  */
		else if(record.info.lock[client.id] && 
		record.info.lock[client.id].type == locks.WRITE) {
			if(record.data[record.child_name] instanceof Array) 
				send_packet(client,record.data[record.child_name].shift(),"RESPONSE");
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
	this.push = function(client,record,data) {
		/* if the requested data does not exist, result is undefined */
		if(record.data === undefined) {
			this.error(client,errors.OBJECT_NOT_FOUND);
			return true;
		}
		/* if this client has this record locked  */
		else if(record.info.lock[client.id] && 
			record.info.lock[client.id].type == locks.WRITE) {
			if(record.data[record.child_name] instanceof Array) {
				var index = record.data[record.child_name].length;
				record.data[record.child_name].push(data);
				/* populate this object's children with shadow objects */
				composite_sketch(record.data[record.child_name][index],record.shadow[record.child_name][index]);
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

	/* push a record onto the end of an array */
	this.unshift = function(client,record,data) {
		/* if the requested data does not exist, result is undefined */
		if(record.data === undefined) {
			this.error(client,errors.OBJECT_NOT_FOUND);
			return true;
		}
		/* if this client has this record locked  */
		else if(record.info.lock[client.id] && 
			record.info.lock[client.id].type == locks.WRITE) {
			if(record.data[record.child_name] instanceof Array) {
				var index = record.data[record.child_name].length;
				record.data[record.child_name].unshift(data);
				/* populate this object's children with shadow objects */
				composite_sketch(record.data[record.child_name][index],record.shadow[record.child_name][index]);
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
	
    /* server's data submission method (not directly called by client) */    
    this.write = function(client,record,data) {
	
		/* if this client has this record locked  */
		if(record.info.lock[client.id] && 
		record.info.lock[client.id].type == locks.WRITE) {
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
    this.remove = function(client,record) {
		/* if the requested data does not exist, do nothing */
		if(record.data === undefined) 
			return true;
		/* if this client has this record locked */
		else if(record.shadow[record.child_name]._lock[client.id] && 
			record.shadow[record.child_name]._lock[client.id].type == locks.WRITE) {
			delete record.data[record.child_name];
			return true;
		}
        /* if there is no lock for this client, error */
        else {
			this.error(client,errors.NOT_LOCKED);
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
			location:record.location
		}
		send_packet(client,data,"RESPONSE");
		return true;
	}

	/* return a list of subscriptions and associated IP address for clients */
	this.who = function(client,record) {
		var data = get_subscriber_list(record);
		send_packet(client,data,"RESPONSE");
		return true;
	}

	/* generic query handler, will process locks, reads, writes, and unlocks
	and put them into the appropriate queues */
	this.query = function(client,query) {
        /* store the child name */
        var parent_name = get_pname(query.location);
        /* strip the last child identifier from the string */
        var child_name = get_cname(query.location);
		
		/* if the data request is invalid or empty, return an error */
		if(!child_name) {
			this.error(client,errors.INVALID_REQUEST);
			return false;
		}

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

		/* if an operation is requested */
		if(query.oper !== undefined) {
			request = new Request(client,query.oper,parent_name,child_name,query.data);
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
				client,"LOCK",parent_name,child_name,locks.UNLOCK
			));
		}
		
		/* add the temporary queue to the main queue */
		this.queue=this.queue.concat(q);
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
		case this.settings.READ_ONLY:
			error_desc="Record is read-only";
			break;
		}
		var error=new Error(error_num,error_desc);
		send_packet(client,error,"ERROR");
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
			this.file.write(JSON.stringify(this.data,undefined,'\t'));
		else 
			this.file.write(JSON.stringify(this.data));
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
		this.data = JSON.parse(
			this.file.readAll(this.settings.FILE_BUFFER).join('\n')
		);
		this.file.close(); 
        this.shadow=composite_sketch(this.data);
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
		log(LOG_INFO,"database initialized (v" + this.VERSION + ")");
	};
    
	/* release any locks or subscriptions held by a disconnected client */
	this.release = function(client) {
		free_prisoners(client,this.shadow);
		for (var s in this.subscriptions[client.id]) {
			cancel_subscriptions(client,this.subscriptions[client.id]);
			delete this.subscriptions[client.id];
		}
		for(var c=0;c<this.queue.length;c++) {
			if(this.queue[c].client.id == client.id)
				this.queue.splice(c--,1);
		}
	};
	
    /* main "loop" called by server */
    this.cycle = function(timestamp) {
		/* process request queue, removing successful operations */
		for(var r=0;r<this.queue.length;r++) {
			var request=this.queue[r];
			var result=false;
			/* locate the requested record within the database */
			var record=identify_remains.call(
				this,request.client,request.parent_name,request.child_name,
				request.oper.toUpperCase() == "WRITE"
			);
			
			if(!record) {
				log(LOG_WARNING,"db: bad request removed from queue");
				this.queue.splice(r--,1);
				continue;
			}
			
			switch(request.oper.toUpperCase()) {
			case "READ":
				result=this.read(request.client,record);
				break;
			case "POP":
				result=this.pop(request.client,record);
				break;
			case "SHIFT":
				result=this.shift(request.client,record);
				break;
			case "WRITE":
				result=this.write(request.client,record,request.data);
				break;
			case "PUSH":
				result=this.push(request.client,record,request.data);
				break;
			case "UNSHIFT":
				result=this.unshift(request.client,record,request.data);
				break;
			case "DELETE":
				result=this.remove(request.client,record);
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
			case "WHO":
				result=this.who(request.client,record);
				break;
			}
			if(result == true) {
				log(LOG_DEBUG,"db: " + 
					request.client.id + " " + 
					request.oper + " " + 
					record.location + " OK"
				);
				this.queue.splice(r--,1);
			}
			else {
				log(LOG_DEBUG,"db: " + 
					request.client.id + " " + 
					request.oper + " " + 
					record.location + " FAILED"
				); 
			}
		} 
		if(!this.settings.UPDATES)
			return;
		if(!this.settings.SAVE_INTERVAL > 0)
			return;
		if(!timestamp)
			timestamp = Date.now();
        if(timestamp - this.settings.LAST_SAVE < (this.settings.SAVE_INTERVAL * 1000)) 
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
	function Record(data,shadow,location,child_name,info) {
		this.data=data;
		this.shadow=shadow;
		this.location=location;
		this.child_name=child_name;
		this.info=info;
	}

	/* request object generated by queue() method
	contains the requested object parent, the specific child property requested,
	data (in the case of a PUT operation ) */
	function Request(client,operation,parent_name,child_name,data) {
		this.client=client;
		this.oper=operation;
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
	function identify_remains(client,parent_name,child_name,create_new) {

		var data=this.data;
		var shadow=this.shadow;
		var location=child_name;
		var info={
			lock:{},
			lock_type:locks.NONE,
			lock_pending:false,
			subscribers:{}
		};

		if(parent_name !== undefined) {
			/* iterate through split object name checking the keys against the database and 
			checking the lock statuses against the shadow copy */
			var p=parent_name.split(/\./);
			for each(var c in p) {
				/* in the event of a write request, create new data if it does not exist*/
				/* ensure that the shadow object exists in order to allow for non-read operations */
				if(shadow[c] === undefined) 
					create_shadow(shadow,c);
				shadow=shadow[c];
				/* keep track of current object, and store the immediate parent of the request object */
				if(data !== undefined) {
					if(data[c] === undefined && create_new) 
						create_data(data,c);
					data=data[c];
				}
				/* check the current object's lock and subscriber status along the way */
				info = investigate(shadow,info);
			}
			location = parent_name + "." + child_name;
		}
		
		/* ensure requested shadow object's existance */
		if(shadow[child_name] === undefined) 
			create_shadow(shadow,child_name);
			
		/* continue on through the selected shadow object's children to check for locked children */
		info = search_party(shadow[child_name],info);
		
		/* return selected database object, shadow object, and overall lock status of the chosen tree */
		return new Record(data,shadow,location,child_name,info);
	}
	
	/* if the requested child object does not exist, create it */
	function create_shadow(shadow,child_name) {
		log(LOG_DEBUG,"creating new shadow: " + child_name);
		shadow[child_name]=new Shadow();
	}
	
	/* if the requested child object does not exist, create it */
	function create_data(data,child_name) {
		log(LOG_DEBUG,"creating new data: " + child_name);
		data[child_name]={};
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
	
	/* release subscriptions on an object recursively */
	function cancel_subscriptions(client,records) {
		for(var r in records) {
			var record = records[r];
			log(LOG_DEBUG,"releasing subscription: " + record.location);
			delete record.shadow[record.child_name]._subscribers[client.id];
			send_subscriber_updates(client,record,"UNSUBSCRIBE");
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

	/* send updates of this object to all subscribers */
	function send_data_updates(client,record) {
		var data = {
			oper:"WRITE",
			location:record.location,
			data:get_record_data(record)
		};
		for each(var c in record.info.subscribers) {
			/* do not send updates to request originator */
			if(c.id == client.id)
				continue;
			send_packet(c,data,"UPDATE");
		}
	}
	
	/* retrieve record data, if present */
	function get_record_data(record) {
		if(record.data === undefined)
			return undefined;
		return record.data[record.child_name];
	}
	
	/* send update of client subscription to all subscribers */
	function send_subscriber_updates(client,record,oper) {
		var data = {
			oper:oper,
			location:record.location,
			data:get_client_info(client)
		};
		for each(var c in record.info.subscribers) {
			/* do not send updates to request originator */
			if(c.id == client.id)
				continue;
			send_packet(c,data,"UPDATE");
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
	
	/* parse child object name from a dot-delimited string */
	function get_cname(str) {
		var i = str.lastIndexOf('.');
		if(i > 0)
			return str.substr(i+1);
		return str;
	}

	/* count object members */
	function count(obj) {
		var c=0;
		for(var i in obj)
			c++;
		return c;
	}
	
	/* send data packet to a client */
	function send_packet(client,data,func) {
		var data={
			func:func,
			data:data
		};
		client.sendJSON(data);
	}

	/* constructor */
	this.init();
	this.load();
};


