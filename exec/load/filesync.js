//$Id$
//$Revision$
//FILE SYNCHRONIZATION SCRIPT
//USED BY "commservice.js" & "commclient.js"
load("funclib.js");

const connection_timeout=5;
const max_buffer=10240;
var descriptor=argv[0];
var query=js.eval(argv[1]);
var hub_address=argv[2];
var hub_port=argv[3];
var sock=new Socket();

function file_sync()
{
	if(hub_address && hub_port) {
		sock.connect(hub_address,hub_port,connection_timeout);
	} else if(descriptor) {
		sock.descriptor=descriptor;
	} else {
		debug("invalid arguments",LOG_WARNING);
		return false;
	}
	if(!testSocket(sock)) {
		return false;
	}
	var module=get_module(query.id);
	if(!module) return false;
	
	debug("synchronizing files: " + query.filemask,LOG_DEBUG);
	try {
		switch(query.command)
		{
			case "send":
				sync_remote(query,module.working_directory);
				break;
			case "recv":
				sync_local(query,module.working_directory);
				break;
			case "askrecv":
				receive_file(module.working_directory+query.filemask,query.filedate);
				break;
			default:
				debug("unknown sync request: " + query.command,LOG_WARNING);
				break;
		}
	} catch(e) {
		debug("FILESYNC ERROR: " + e,LOG_WARNING);
	}
	if(sock.is_connected) sock.close();
	return true;
}
function get_module(id)
{
	var comminit=new File(system.ctrl_dir + "filesync.ini");
	comminit.open('r');
	var match=comminit.iniGetSections(id);
	if(!match.length) {
		debug("module not found, aborting: " + id,LOG_WARNING);
		sock.send("#abort\r\n");
		return false;
	}
	var module=comminit.iniGetObject(id);
	comminit.close();
	return module;
}
function sync_remote(data,dir)
{
	var files=directory(dir + file_getname(data.filemask));
	if(files.length>0) debug("sending " + files.length + " files",LOG_DEBUG);
	else {
		debug("file(s) not found: " + dir + data.filemask,LOG_WARNING);
		return false;
	}
	for(var f=0;f<files.length;f++) {
		if(!send_file(files[f],data)) break;
	}
	respond("endquery");
}	
function respond(response)
{
	var data=new Object();
	data.command=response;
	if(!socket_send(data)) return false;
	return true;
}
function send_file(filename,data)
{
	/*
		"data" object already contains the properties 
		needed by commservice to recognize the information
		as a file request
	*/
	data.command="askrecv";
	data.filemask=file_getname(filename);
	data.filedate=file_date(filename);
	if(!socket_send(data)) return false;
	var response=get_response();
	if(response) {
		switch(response.command)
		{
			case "ok":
				if(!socket_send(load_file(filename))) {
					debug("file not sent: " + filename,LOG_WARNING);
					return false;
				}
				debug("file sent: " + filename,LOG_DEBUG);
				break;
			case "skip":
				debug("skipping file: " + filename,LOG_DEBUG);
				break;
			case "abort":
				debug("aborting query",LOG_DEBUG);
				return false;
			default:
				debug("unknown response: " + response.command,LOG_WARNING);
				return false;
		}
		return true;
	} else {
		debug("transfer timed out: " + filename,LOG_WARNING);
		return false;
	}
}
function socket_send(data)
{
	if(!testSocket(sock)) return false;
	if(!sock.send(data.toSource() + "\r\n")) return false;
	return true;
}
function load_file(filename)
{
	var d=new Object();
	var f=new File(filename);
	f.open('r',true,max_buffer);
	var contents=f.readAll();
	f.close();
	var filesize=file_size(filename);
	d.file=contents;
	d.filesize=filesize;
	return d;
}
function get_response()
{
	if(!testSocket(sock)) {
		debug("error reading from socket",LOG_WARNING);
		return false;
	}
	response=sock.recvline(max_buffer,connection_timeout);
	if(response != null) {
		return js.eval(response);
	} else {
		if(sock.error) {
			debug("SOCKET ERROR: " + sock.error,LOG_WARNING);
			debug(sock,LOG_WARNING);
		}
		else {
			debug("socket timed out",LOG_WARNING);
		}
		return false;
	}
}
function sync_local(data,dir)
{
	var aborted=false;
	data.command="send";
	if(!socket_send(data)) {
		debug("error retrieving files",LOG_WARNING);
		return false;
	}
	
	debug("retrieving files: " + dir + data.filemask,LOG_DEBUG);
	while(!aborted) {
		var response=get_response();
		if(response) {
			switch(response.command)
			{
				case "askrecv":
					if(compare_dates(file_date(response.filemask),response.filedate))
					{
						debug("skipping file: " + response.filemask,LOG_DEBUG);
						respond("skip");
						break;
					} else {
						respond("ok");
						if(!receive_file(dir+response.filemask,response.filedate)) {
							aborted=true;
						} 
						break;
					}
				case "abort":
					debug("aborting query",LOG_DEBUG);
					aborted=true;
					break;
				case "endquery":
					debug("query complete",LOG_DEBUG);
					aborted=true;
					break;
				default:
					debug("unknown response: " + response.command,LOG_WARNING);
					aborted=true;
					break;
			}
		} else {
			aborted=true;
		}
	}
	if(sock.is_connected) sock.close();
}
function tell_mommy(ip,file,date)
{
	if(parent_queue) {
		var data=new Object();
		data.remote_ip_address=ip;
		data.filename=file;
		data.filedate=date;
		parent_queue.write(data);
	}
}
function receive_file(filename,filedate)
{
	var file=new File(filename + ".tmp");
	file.open('w',false);
	if(!file.is_open) {
		debug("error opening file: " + filename,LOG_WARNING);
		return false;
	}
	
	debug("receiving file: " + filename,LOG_DEBUG);
	var data=get_response();
	if(data) {
		file.writeAll(data.file);
		log("received: " + data.filesize + " bytes");
		file.close();
		if(file_exists(filename+".bck")) file_remove(filename+".bck");
		file_rename(filename,filename+".bck");
		file_rename(file.name,filename);
		file_utime(filename,time(),filedate);
		tell_mommy(sock.remote_ip_address,filename,filedate);
		return true;
	} else {
		debug("error transferring file:" + filename,LOG_WARNING);
		file.close();
		file_remove(file.name);
		return false;
	}
}
function compare_dates(local,remote)
{
	//will treat numbers with a difference of 1 or 0 as the same, due to issues with some file systems
	if(Math.abs(local-remote)<=1) return true;
	//will reject files with a time_t older than the local copy
	if(local>remote) return true;
	else return false;
}

file_sync();
