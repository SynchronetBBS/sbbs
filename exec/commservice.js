//$Id$
/*
	Inter-BBS/Inter-Node socket service
	for Synchronet v3.15+ 
	by Matt Johnson - 2009
	Allows real-time gaming/chatting between "LOCAL" bbs nodes and "SERVER" bbs systems

	The following files can be located in the Synchronet CVS repository:
	
	Add to "/sbbs/exec/" directory:
		commservice.js
	
	Add to "/sbbs/exec/load/" directory:
		commclient.js
		chateng.js
		funclib.js
	
	Add to "/sbbs/ctrl" directory:
		filesync.ini
	
	Add to "/sbbs/ctrl/services.ini" file:

	[Commserv]
	Port=10088
	MaxClients=20
	Options=STATIC
	Command=commservice.js thebrokenbubble.com 10088
	
*/
load("funclib.js");
load("synchronet-json.js");

const VERSION=				"$Revision$".split(' ')[1];
const CONNECTION_TIMEOUT=	1;//SECONDS
const CONNECTION_INTERVAL=	60;
const CONNECTION_ATTEMPTS=	10;
const MAX_BUFFER=			512;
const MAX_RECV=				10240;

var modules=[];
var servers=[];
var servers_map=[];
var data_queue=[];
var remote_clients=[];
var local_clients=[];
var awaiting_auth=[];
var blocking=[];

server.socket.nonblocking = true;

/* main service loop */
function cycle()
{
	while(!js.terminated) {
		try {
			if(unknown_sessions_active()) {
				authenticate();
			}
			if(known_sessions_active()) {
				inbound();
				outbound();
			}
			if(!server.socket.poll(0.1)) {
				if(js.terminated) break;
				continue;
			}
			
			var incoming=server.socket.accept();
			debug("connection from " + incoming.remote_ip_address,LOG_DEBUG);
			awaiting_auth.push(get_socket(incoming));
			
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

/* service module initialization (from ctrl/filesync.ini) */
function load_modules()
{
	var mfile=new File(system.ctrl_dir + "filesync.ini");
	if(!mfile.open('r')) {
		log(LOG_ERROR,"Error opening module file");
		return false;
	}
	var list=mfile.iniGetSections();
	for(l=0;l<list.length;l++) {
		var module_name=list[l];
		var module=mfile.iniGetObject(module_name);
		init_module(module,module_name);
	}
	mfile.close();
}
function init_module(module,module_name) 
{
	debug("loading module: " + module_name);
	modules[module_name]=new Object();
	
	/* if a working directory has been provided..... (it should be) */
	if(module.working_directory) {
		modules[module_name].dir=module.working_directory;
	}
		
	/* if there is a special handler for this module's data */
	if(module.handler) {
		load(modules[module_name],module.working_directory+module.handler);
	}
	
	/* if a server.ini file exists for this module, that will be our hub (if not.. we ARE the hub) */
	if(file_exists(module.working_directory + "server.ini")) {
		var sfile=new File(module.working_directory + "server.ini");
		if(!sfile.open('r')) {
			log(LOG_ERROR,"Error opening server file");
			return false;
		}
		
		var host=sfile.iniGetValue(null,"host");
		var port=sfile.iniGetValue(null,"port");
		var auth=false;
		
		if(sfile.iniGetSections("auth").length > 0) {
			auth=sfile.iniGetObject("auth");
			for(var i in auth) auth[i]=eval(auth[i]);
			log("creating auth: " + auth.toSource());
		}
		
		if(host && port) {
			servers_map[module_name]=host;
			if(!servers[host]) {
				servers[host]=new Server(host,port,auth);
				log("created server for: " + module_name);
			}
		}
	}
}

/* socket IO */
function inbound()
{
	for(var m in servers) {
		servers[m].receive();
	}
	for(var l in local_clients) {
		receive_from_local(local_clients[l]);
	}
	receive_from_remote(remote_clients);
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
			delete_socket(awaiting_auth,a);
			a--;
		}
	}
	for(var r=0;r<remote_clients.length;r++)	{
		if(!remote_clients[r].cycle()) {
			delete_socket(remote_clients,r);
			r--;
		}
	}
	for(var s in local_clients) {
		for(var i=0;i<local_clients[s].length;i++) {
			if(!local_clients[s][i].cycle()) {
				delete_socket(local_clients[s],i);
				i--;
			}
		}
	}
}
function receive_from_remote(sock_array)
{
	var socks=socket_select(sock_array);
	for(var s in socks) {
		var sock=sock_array[socks[s]];
		var data=sock.receive();
		if(!data) return false;
		
		process_remote_data(sock,data);
		if(!servers_map[data.id] && modules[data.id] && modules[data.id].handler) {
			modules[data.id].handler(data);
		}
	}
}
function receive_from_local(sock_array) 
{
	var socks=socket_select(sock_array);
	for(var s in socks) {
		var sock=sock_array[socks[s]];
		var data=sock.receive();
		if(!data) return false;
		
		process_local_data(sock,data);
		if(!servers_map[data.id] && modules[data.id] && modules[data.id].handler) {
			modules[data.id].handler(data);
		}
	}
}
function process_server_data(sock,data) 
{
	switch(data.func) 
	{
	case "FILESYNC":
		handle_filesync(sock,data);
		break;
	case "QUERY":
		handle_query(sock,data);
		break;
	default:
		queue(data);
		break;
	}
	if(modules[data.id]) {
		if(modules[data.id].handler) {
			modules[data.id].handler(data);
		}
	} else {
		log("no such module: " + data.toSource());
	}
}
function process_local_data(sock,data)
{
	switch(data.func) 
	{
	case "FILESYNC":
		var module_server=servers[servers_map[data.id]];
		if(module_server) {
			if(data.blocking) {
				add_receipt_request(sock,data);
			}
			module_server.enqueue(data);
		} else {
			if(data.blocking) send_receipt(sock,data);
			send_updates(sock,data);
		}
		break;
	case "QUERY":
		handle_query(sock,data);
		break;
	default:
		queue(data);
		break;
	}
}
function send_receipt(sock,data)
{
	var receipt=new Packet("FILESYNC");
	receipt.filemask=data.filemask;
	sock.enqueue(receipt);
}
function add_receipt_request(sock,query)
{
	if(!blocking[query.id]) blocking[query.id]=[];
	if(!blocking[query.id][query.filemask]) blocking[query.id][query.filemask]=[];
	blocking[query.id][query.filemask].push(sock);
}
function process_remote_data(sock,data)
{
	switch(data.func) 
	{
	case "FILESYNC":
		var module_server=servers[servers_map[data.id]];
		if(module_server) {
			if(data.blocking && !blocking[sock.descriptor]) {
				blocking[sock.descriptor]=sock;
			}
			module_server.enqueue(data);
		} else {
			handle_filesync(sock,data);
		}
		break;
	case "QUERY":
		handle_query(sock,data);
		break;
	default:
		queue(data);
		break;
	}
}
function get_socket(sock) 
{
	sock.queue="";
	sock.nonblocking=true;
	sock.receive=sock_receive;
	sock.enqueue=sock_enqueue;
	sock.cycle=sock_cycle;
	return sock;
}
function delete_socket(array,index)
{
	log("session terminated: " + array[index].remote_ip_address);
	server.client_remove(array[index]);
	array[index].close();
	array.splice(index,1);
	log("clients: " + server.clients);
}
function authenticate()
{
	var socks=socket_select(awaiting_auth);
	for(var s=0;s<socks.length;s++) {
		var sock=awaiting_auth[socks[s]];
		if(!sock) continue;
		
		if(!sock.is_connected) {
			awaiting_auth.splice(socks[s],1);
			continue;
		}

		debug("authenticating socket");
		var data=sock.receive();
		if(!data) continue;
		
		switch(data.context) 
		{
		case "CLIENT":
			log("local connection: " + data.nickname + " running " + data.id);
			sock.nickname=data.nickname;
			if(!local_clients[data.id]) local_clients[data.id]=[];
			local_clients[data.id].push(sock);
			enable_server(data.id);
			server.client_add(sock);
			awaiting_auth.splice(socks[s],1);
			queue(data);
			break;
		case "SERVER":
			if(data.version==VERSION) {
				log("remote connection: " + data.system);
				sock.system=data.system;
				remote_clients.push(sock);
				server.client_add(sock);
				awaiting_auth.splice(socks[s],1);
				queue(data);
				break;
			} else {
				debug("incompatible with remote server version: " + data.version,LOG_WARNING);
				sock.close();
				awaiting_auth.splice(socks[s],1);
				continue;
			}
		default:
			debug("unknown connection type: " + data.context,LOG_WARNING);
			sock.close();
			awaiting_auth.splice(socks[s],1);
			continue;
		}
		log("clients: " + server.clients);
		break;
	}
}
function enable_server(id)
{
	if(servers_map[id]) {
		servers[servers_map[id]].enabled=true;
	}
}
function queue(data)
{ 
	var module_server=servers[servers_map[data.id]];
	if(module_server && module_server.enabled) {
		module_server.enqueue(data);
	}
	for(var r=0;r<remote_clients.length;r++)	{
		remote_clients[r].enqueue(data);
	}
	for(var l=0;local_clients[data.id] && l<local_clients[data.id].length;l++)	{
		local_clients[data.id][l].enqueue(data);
	}
}
function known_sessions_active() 
{
	if(remote_clients.length>0) {
		return true;
	}
	for(var s in local_clients) {
		if(local_clients[s].length>0) {
			return true;
		} else if(servers_map[s]) {
			//servers[servers_map[s]].disconnect();
		}
	}
	return false;
}
function unknown_sessions_active() 
{
	if(awaiting_auth.length>0) {
		return true;
	}
}
function count_local_sockets()
{
	var count=0;
	for(var l in local_clients)
	{
		if(local_clients[l].length) count+=local_clients[l].length;
	}
	return count;
}
function count_remote_sockets()
{
	return remote_clients.length;
}

/* filesync */
function handle_filesync(socket,query)
{
	try {
		var module=modules[query.id];
		if(!module) return false;
		switch(query.command)
		{
			case "TRYSEND":
				sync_remote(socket,module.dir,query);
				break;
			case "TRYRECV":
				if(!sync_local(socket,module.dir,query)) {
					send_receipts(query);
				}
				break;
			case "DORECV":
				if(recv_file(socket,module.dir,query)) {
					send_updates(socket,query);
					send_receipts(query);
				}
				break;
			case "DOSEND":
				send_file(socket,module.dir,query);
				break;
			default:
				debug("unknown sync request: " + query.command,LOG_WARNING);
				break;
		}
	} catch(e) {
		debug("FILESYNC ERROR: " + e,LOG_ERROR);
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
		query.command="TRYRECV";
		query.filemask=file_getname(files[f]);
		query.filedate=file_date(files[f]);
		query.descriptor=server.socket.descriptor;
		socket.enqueue(query);
	}
}	
function send_updates(socket,query)
{
	var module=modules[query.id];
	if(!module) return false;
	for(r=0;r<remote_clients.length;r++) {
		if(remote_clients[r].descriptor != socket.descriptor) {
			log("sending server updates: " + query.filemask);
			sync_remote(remote_clients[r],dir,query);
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
		data.command="DORECV";
		data.func="FILESYNC";
		socket.enqueue(data);
		debug("file sent: " + data.filemask,LOG_DEBUG);
	}
}
function sync_local(socket,dir,query)
{
	var r_filedate=query.filedate;
	var l_filedate=file_date(dir+query.filemask);
	if(compare_dates(l_filedate,r_filedate)) {
		query.command="DOSEND";
		query.descriptor=server.socket.descriptor;
		socket.enqueue(query);
		return true;
	} else {
		debug("skipping file: " + query.filemask);
		return false;
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
function compare_dates(local,remote)
{
	if(local<0) return true;
	//will reject files with a time_t older than the local copy
	//if(local>remote) return true;
	//will treat numbers with a difference of 1 or 0 as the same, due to issues with some file systems
	if(Math.abs(local-remote)>1) return true;
	else return false;
}
function send_receipts(query)
{
	if(blocking[query.id] && blocking[query.id][query.filemask]) {
		while(blocking[query.id][query.filemask].length) {
			var receipt=new Packet("FILESYNC");
			receipt.filemask=query.filemask;
			blocking[query.id][query.filemask].shift().enqueue(receipt);
		}
	}
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

//TODO: queries
function handle_query(socket,query)
{
	log("query from: " + socket.nickname);
	switch(query.cmd)
	{
		case "WHO":
			whos_online(socket,query);
			break;
		case "FINDUSER":
			find_user(socket,query);
			break;
		case "PAGEUSER":
			page_user(socket,query);
			break;
		default:
			debug("unknown query type: " + query.task,LOG_WARNING);
			debug(query,LOG_WARNING);
			break;
	}
}
function whos_online(socket,data)
{
	for(var l in local_clients[data.id]) {
		for(var u in local_clients[data.id][l].users) {
			var usr=local_clients[data.id][l].users[u];
			debug("USER: " + usr.nickname + " CHAN: " + usr.chan);
			socket.enqueue(local_clients[data.id][l].users[u]);
		}
	}
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

/* socket prototype methods */
function sock_receive()
{
	if(!this.data_waiting) return false;
	var data=this.recvline(MAX_RECV);
	if(data == null) return false;
	try {
		data=JSON.parse(data);
		data.descriptor=this.descriptor;
	} catch(e) {
		log(LOG_ERROR,"error parsing JSON data: " + data);
		return false;
	}
	return data;
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

/* generic data packet */
function Packet(func)
{
	this.func=func;
}
/* server object */
function Server(addr,port,auth)
{
	this.address=addr;
	this.port=port;
	this.sock=false;
	this.enabled=true;
	this.last_attempt=0;
	this.attempts=0;
	this.auth=auth;
	this.users=[];

	this.connect=function()
	{
		debug("connecting to " + this.address + " on port " + this.port,LOG_DEBUG);
		this.sock=get_socket(new Socket());
		this.sock.connect(this.address,this.port,CONNECTION_TIMEOUT);
		if(!this.sock.is_connected) {
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
			this.enabled=false;
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
		if(this.sock.is_connected) {
			var data=this.sock.receive();
			if(!data) return false;
			process_server_data(this.sock,data);
		}
	}
	this.enqueue=function(data)
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
		data.context="SERVER";
		data.version=VERSION;
		data.system=system.name;
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