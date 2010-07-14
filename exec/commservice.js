//$Id$
/*
	Inter-BBS/Inter-Node socket service
	for Synchronet v3.15+ 
	by Matt Johnson - 2009
	Allows real-time gaming/chatting between LOCAL bbs nodes and REMOTE bbs systems

	The following files can be located in the Synchronet CVS repository:
	
	Add to "/sbbs/exec/" directory:
		commservice.js
	
	Add to "/sbbs/exec/load/" directory:
		commclient.js
		chateng.js
		funclib.js
	
	Add to "/sbbs/ctrl" directory:
		FILESYNC.ini
	
	Add to "/sbbs/ctrl/services.ini" file:

	[Commserv]
	Port=10088
	MaxClients=20
	Options=STATIC
	Command=commservice.js thebrokenbubble.com 10088
	
*/
load("funclib.js");
load("synchronet-json.js");

const VERSION=				"$Revision$";
const REMOTE=				"*";
const LOCAL=				"&";
const FILESYNC=			"@";
const QUERY=				"?";
const CONNECTION_TIMEOUT=	1;//SECONDS
const CONNECTION_INTERVAL=	60;
const CONNECTION_ATTEMPTS=	10;
const MAX_BUFFER=			512;
const MAX_RECV=			10240;

var modules=[];
var servers=[];
var server_map=[];
var data_queue=[];
var remote_sessions=[];
var local_sessions=[];
var awaiting_auth=[];

server.socket.nonblocking = true;
//main service loop
function load_modules()
{
	var mfile=new File(system.ctrl_dir + "filesync.ini");
	mfile.open('r');
	var list=mfile.iniGetSections();
	for(l=0;l<list.length;l++) {
		var module_name=list[l];
		modules[module_name]=new Object();
		var module=mfile.iniGetObject(module_name);
		if(module.working_directory) 
			modules[module_name].dir=module.working_directory;
		if(module.handler)
			load(modules[module_name],module.working_directory+module.handler);
		if(module.address && module.port) {
			server_map[module_name]=module.address;
			var auth=false;
			if(module.servername && module.password) {
				auth=new Object();
				auth.servername=module.servername;
				auth.password=module.password;
				auth.system=system.name;
			}
			if(!servers[module.address]) {
				servers[module.address]=new Server(module.address,module.port,auth);
			}
		}
	}
	mfile.close();
}
function cycle()
{
	while(!js.terminated) {
		try {
			if(sessions_active()) {
				authenticate();
				inbound();
				outbound();
			}
			if(!server.socket.poll(0.1)) {
				if(js.terminated) break;
				continue;
			}
			store_socket(server.socket.accept(),awaiting_auth);
		} catch(e) {
			debug(e,LOG_WARNING);
			break;
		}
	}
	for(var s in servers) {
		if(servers[s].enabled)
			servers[s].disconnect();
	}
}
function inbound()
{
	for(var m in servers) {
		if(servers[m].enabled) {
			servers[m].receive();
		}
	}
	for(var l in local_sessions) {
		receive_from(local_sessions[l]);
	}
	receive_from(remote_sessions);
}
function outbound()
{
	for(var s in servers) {
		if(servers[s].enabled) {
			servers[s].cycle();
		}
	}
	for(var a=0;a<awaiting_auth.length;a++)	{
		if(!awaiting_auth[a].cycle()) {
			delete_session(awaiting_auth,a);
			a--;
		}
	}
	for(var r=0;r<remote_sessions.length;r++)	{
		if(!remote_sessions[r].cycle()) {
			delete_session(remote_sessions,r);
			r--;
		}
	}
	for(var s in local_sessions) {
		for(var i=0;i<local_sessions[s].length;i++) {
			if(!local_sessions[s][i].cycle()) {
				delete_session(local_sessions[s],i);
				i--;
			}
		}
	}
}
function receive_from(sock_array) 
{
	var socks=socket_select(sock_array);
	for(var s in socks) {
		var sock=sock_array[socks[s]];
		var data=sock.recvline(MAX_RECV);
		if(data == null) return false;
		try {
			data=JSON.parse(data);
		} catch(e) {
			log(LOG_ERROR,"error parsing JSON data: " + data);
			return false;
		}
		queue(sock,data);
	}
}
function authenticate()
{
	var socks=socket_select(awaiting_auth);
	for(var s in socks) {
		var sock=awaiting_auth[socks[s]];
		if(!sock) continue;
		debug("authenticating socket");
		var data=sock.recvline();
		if(data == null) continue;
		data=JSON.parse(data);
		switch(data.context) {
			case LOCAL:
				if(!local_sessions[data.id]) local_sessions[data.id]=[];
				local_sessions[data.id].push(sock);
				log("local connection: " + data.origin + " running " + data.id);
				break;
			case REMOTE:
				if(data.version==VERSION) {
					remote_sessions.push(sock);
					log("remote connection: " + data.system);
					break;
				} else {
					debug("incompatible with remote server version: " + data.version,LOG_WARNING);
					sock.close();
					continue;
				}
			default:
				debug("unknown connection type: " + data.context);
				sock.close();
				continue;
		}
		sock.id=data.id;
		sock.context=data.context;
		sock.origin=data.origin;
		sock.system=data.system;
		server.client_add(sock);
		awaiting_auth.splice(socks[s],1);
		queue(sock,data);
		log("clients: " + server.clients);
	}
}
function queue(sock,data)
{ 
	data.descriptor=sock.descriptor;
	switch(data.type) {
		case FILESYNC:
			var module_server=servers[server_map[data.id]];
			if(module_server && module_server.enabled) {
				module_server.enqueue(data)
			} else {
				handle_filesync(sock,data);
			}
			break;
		case QUERY:
			break;
		default:
			var module_server=servers[server_map[data.id]];
			if(module_server && module_server.enabled) {
				module_server.enqueue(data);
			}
			for(var r=0;r<remote_sessions.length;r++)	{
				remote_sessions[r].enqueue(data);
			}
			for(var s=0;local_sessions[data.id] && s<local_sessions[data.id].length;s++)	{
				local_sessions[data.id][s].enqueue(data);
			}
			break;
	}
	if(!server_map[data.id] && modules[data.id] && modules[data.id].handler) {
		modules[data.id].handler(data);
	}
}
function sessions_active() 
{
	if(awaiting_auth.length>0 || remote_sessions.length>0) {
		return true;
	}
	for(var s in local_sessions) {
		if(local_sessions[s].length>0) {
			return true;
		}
	}
	return false;
}
function count_local_sockets()
{
	var count=0;
	for(var l in local_sessions)
	{
		if(local_sessions[l].length) count+=local_sessions[l].length;
	}
	return count;
}
function count_remote_sockets()
{
	return remote_sessions.length;
}

//FILESYNC
function handle_filesync(socket,query)
{
	try {
		var module=modules[query.id];
		if(!module) return false;
		switch(query.command)
		{
			case "trysend":
				sync_remote(socket,module.dir,query);
				break;
			case "tryrecv":
				sync_local(socket,module.dir,query);
				//send_updates(socket,module.dir,query); 
				break;
			case "dorecv":
				if(recv_file(socket,module.dir,query)) {
					//send_updates(socket,module.dir,query);
				}
				break;
			case "dosend":
				send_file(socket,module.dir,query);
				break;
			default:
				debug("unknown sync request: " + query.command,LOG_WARNING);
				break;
		}
	} catch(e) {
		debug("FILESYNC ERROR: " + e,LOG_WARNING);
	}
}
function sync_remote(socket,dir,query)
{
	var files=directory(dir + file_getname(query.filemask));
	if(files.length>0) debug("sending " + files.length + " files",LOG_DEBUG);
	else {
		debug("file(s) not found: " + dir + query.filemask,LOG_WARNING);
		return false;
	}
	for(var f=0;f<files.length;f++) {
		query.command="tryrecv";
		query.filemask=file_getname(files[f]);
		query.filedate=file_date(files[f]);
		query.descriptor=server.socket.descriptor;
		socket.enqueue(query);
	}
}	
function send_updates(socket,dir,query)
{
	for(r=0;r<remote_sessions.length;r++) {
		if(remote_sessions[r].descriptor != socket.descriptor) {
			log("sending node updates: " + query.filemask);
			sync_remote(remote_sessions[r],dir,query);
		}
	}
}
function send_file(socket,dir,query)
{
	/*
		"data" object already contains the properties 
		needed by commservice to recognize the information
		as a file request
	*/
	data=load_file(dir + file_getname(query.filemask));
	if(data) {
		data.descriptor=server.socket.descriptor;
		data.id=query.id;
		data.command="dorecv";
		data.type=FILESYNC;
		socket.enqueue(data);
		debug("file sent: " + data.filemask,LOG_DEBUG);
	}
}
function sync_local(socket,dir,query)
{
	var r_filedate=query.filedate;
	var l_filedate=file_date(dir+query.filemask);
	if(compare_dates(l_filedate,r_filedate)) {
		query.command="dosend";
		query.descriptor=server.socket.descriptor;
		socket.enqueue(query);
	} else {
		debug("skipping file: " + query.filemask);
		debug("local date: " + l_filedate + " remote date: " + r_filedate);
	}
}
function recv_file(socket,dir,query)
{
	var filename=dir+file_getname(query.filemask);
	log("receiving file: " + filename);
	var file=new File(filename + ".tmp");
	file.open('w',true);
	file.base64=true;
	if(!file.is_open) {
		log(LOG_WARNING,"error opening file: " + file.name);
		return false;
	}
	file.writeAll(query.file);
	log("received: " + query.filesize + " bytes");
	file.close();
	if(file_exists(filename+".bck")) file_remove(filename+".bck");
	file_rename(filename,filename+".bck");
	file_rename(file.name,filename);
	file_utime(filename,time(),query.filedate);
	return true;
}

/* Non-socket file functions */
function compare_dates(local,remote)
{
	if(local<0) return true;
	//will reject files with a time_t older than the local copy
	//if(local>remote) return true;
	//will treat numbers with a difference of 1 or 0 as the same, due to issues with some file systems
	if(Math.abs(local-remote)>1) return true;
	else return false;
}
function load_file(filename)
{
	var d=new Object();
	var f=new File(filename);
	f.open('r',true);
	if(!f.is_open) {
		log(LOG_WARNING,"error opening file: " + f.name);
		return false;
	}
	f.base64=true;
	d.file=f.readAll();
	f.close();
	d.filesize=file_size(filename);
	d.filedate=file_date(filename);
	d.filemask=file_getname(filename);
	return d;
}
//END FILESYNC

//TODO QUERIES
function handle_query(socket,query)
{
	log("query from: " + socket.remote_ip_address);
	switch(query.task)
	{
		case "who":
			whos_online(socket,query);
			break;
		case "finduser":
			find_user(socket,query);
			break;
		case "pageuser":
			page_user(socket,query);
			break;
		case "adduser":
			add_user(socket,query);
			break;
		case "removeuser":
			remove_user(socket,query);
			break;
		default:
			debug("unknown query type: " + query.task,LOG_WARNING);
			debug(query,LOG_WARNING);
			break;
	}
}
function whos_online(socket)
{
}
function add_user(socket,data)
{
}
function remove_user(socket,data)
{
}
function find_user()
{
}
function page_user()
{
}
function notify_sysop(msg)
{
	system.put_telegram(1,msg);
}
//END TODO

function get_server_socket() 
{
	var sock=new Socket();
	sock.queue="";
	sock.nonblocking=true;
	sock.enqueue=function(data) {
		if(data.descriptor==this.descriptor) return false;
		data.context=REMOTE;
		data.version=VERSION;
		data.system=system.name;
		this.queue+=JSON.stringify(data)+"\r\n";
	}
	sock.cycle=sock_cycle;
	return sock;
}
function delete_session(array,index)
{
	log("session terminated: " + array[index].remote_ip_address);
	server.client_remove(array[index]);
	array[index].close();
	array.splice(index,1);
	log("clients: " + server.clients);
}
function store_socket(sock,array)
{
	debug("connection from " + sock.remote_ip_address,LOG_DEBUG);
	sock.nonblocking=true;
	sock.queue="";
	sock.enqueue=sock_enqueue;
	sock.cycle=sock_cycle;
	array.push(sock);
}	
function sock_cycle()	
{
	if(!this.is_connected) return false;
	if(this.queue.length>0 && this.write(this.queue.substr(0,MAX_BUFFER))) {
		this.queue=this.queue.substr(MAX_BUFFER);
	}
	return true;
}
function sock_enqueue(data)
{
	if(data.descriptor==this.descriptor) return false;
	this.queue+=JSON.stringify(data)+"\r\n";
}

/* Server object */
function Server(addr,port,auth)
{
	this.address=addr;
	this.port=port;
	this.sock=false;
	this.enabled=true;
	this.queue="";
	this.last_attempt=0;
	this.attempts=0;
	this.auth=auth;

	this.connect=function()
	{
		debug("connecting to " + this.address + " on port " + this.port,LOG_DEBUG);
		this.sock=get_server_socket();
		this.sock.connect(this.address,this.port,CONNECTION_TIMEOUT);
		if(!this.sock.is_connected) 
		{
			this.attempts++;
			this.last_attempt=time();
			return false;
		} 
		if(this.auth) {
			this.sock.enqueue(this.auth);
		}
		return true;
	}
	this.disconnect=function()
	{
		if(this.sock.is_connected) {
			log("disconnecting from server: " + this.address);
			while(this.sock.queue.length) {
				if(!this.cycle()) break;
			}
			this.sock.close();
		}
	}
	this.receive=function()
	{
		if(!this.sock || !this.sock.is_connected) {
			if(this.attempts>=CONNECTION_ATTEMPTS) {
				log(LOG_ERROR,"error connecting to server: " + this.address);
				this.enabled=false;
				return false;
			}
			if((time()-this.last_attempt)>=CONNECTION_INTERVAL) {
				this.connect();
			}
		}
		if(this.sock.is_connected && this.sock.data_waiting) {
			var data=this.sock.recvline(MAX_RECV);
			if(data == null) return false;
			try {
				data=JSON.parse(data);
			} catch(e) {
				log(LOG_ERROR,"error parsing JSON data: " + data);
				return false;
			}
			switch(data.type) {
				case FILESYNC:
					handle_filesync(this.sock,data);
					break;
				case QUERY:
					handle_query(this.sock,data);
					break;
				default:
					queue(this.sock,data);
					break;
			}
			if(modules[data.id] && modules[data.id].handler) {
				modules[data.id].handler(data);
			}
		}
	}
	this.enqueue=function(data)
	{	
		this.sock.enqueue(data);
	}
	this.cycle=function()
	{
		if(!this.sock || !this.sock.is_connected) {
			if(this.attempts>=CONNECTION_ATTEMPTS) {
				log(LOG_ERROR,"error connecting to server: " + this.address);
				this.enabled=false;
				return false;
			}
			if((time()-this.last_attempt)>=CONNECTION_INTERVAL) {
				this.connect();
			}
		}
		if(this.sock) this.sock.cycle();
		return true;
	}
	
}

load_modules();
cycle();