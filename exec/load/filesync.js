const connection_timeout=10;
const tries=5;

function comm_sync()
{
	var data=parse_query(query);
	var session_id=data[0].substr(1);
	var task=data[1];
	var filemask=data[2];
	var filedate=data[3];
	var module=get_module(session_id);
	
	if(!module) return false;
	
	log("synchronizing files: " + filemask);
	switch(task)
	{
		case "#send":
			sync_remote(session_id,module.working_directory,filemask);
			break;
		case "#recv":
			sync_local(session_id,module.working_directory,filemask);
			break;
		case "#askrecv":
			receive_file(session_id,module.working_directory,filemask,filedate);
			break;
		case "#endquery":
			break;
		default:
			log("unknown sync request: " + task);
			break;
	}
	sock.close();
	return true;
}
function get_module(id)
{
	var comminit=new File(system.ctrl_dir + "filesync.ini");
	comminit.open('r');
	var match=comminit.iniGetSections(id);
	if(!match.length) {
		log("module not found, aborting: " + id);
		sock.send("#abort\r\n");
		return false;
	}
	var module=comminit.iniGetObject(id);
	comminit.close();
	return module;
}
function parse_query(q)
{
	if(q) return q.split("");
	return false;
}
function sync_remote(session_id,dir,filemask)
{
	var files=directory(dir + file_getname(filemask));
	if(files.length>0) log("sending " + files.length + " files");
	else {
		log("file(s) not found: " + dir + filemask);
		sock.send("@" + session_id + "#abort\r\n");
	}
	for(var f=0;f<files.length;f++)
	{
		var filename=file_getname(files[f]);
		var filedate=file_date(files[f]);
		
		sock.send("@" + session_id + "#askrecv" + filename + "" + filedate + "\r\n");
		var data=sock.recvline(1024,connection_timeout);
		if(data!=null)
		{
			var response=parse_query(data);
			switch(response[1])
			{
				case "#ok":
					log("sending file: " + filename);
					sock.sendfile(files[f]);
					sock.send("#eof\r\n");
					log("file sent: " + filename);
					break;
				case "#skip":
					log("skipping file: " + filename);
					break;
				case "#abort":
					log("aborting query");
					return false;
				default:
					log("unknown response: " + response);
					sock.send("#abort\r\n");
					return false;
			}
		} else {
			log("transfer timed out: " + files[f]);
			return false;
		}
	}
	sock.send("@" + session_id + "#endquery\r\n");
	return true;
}
function sync_local(session_id,dir,filemask)
{
	log("retrieving files: " + filemask);
	if(!sock.is_connected) log("connection interrupted");
	sock.send("@" + session_id + "#send" + file_getname(filemask) + "\r\n");
	
	while(sock.is_writeable) 
	{
		var data=sock.recvline(1024,connection_timeout);
		if(data!=null)
		{
			data=parse_query(data);
			var response=data[1];
			var file=data[2];
			var date=data[3];
			
			switch(response)
			{
				case "#askrecv":
					receive_file(dir,file,date);
					break;
				case "#abort":
					sock.close();
					break;
				case "#endquery":
					sock.close();
					break;
				default:
					log("unknown response: " + response);
					sock.close();
					break;
			}
		} else {
			log("transfer timed out: " + filemask);
			sock.close();
		}
	}
}
function receive_file(session_id,dir,filename,filedate)
{
	fname=dir+filename;
	if(file_date(fname)==filedate)
	{
		sock.send("@" + session_id + "#skip\r\n");
		return false;
	}
	sock.send("@" + session_id + "#ok\r\n");

	var file=new File(fname + ".tmp");
	file.open('w',false);
	log("receiving file: " + filename);
	while(sock.is_writeable)
	{
		var data=sock.recvline(1024,connection_timeout);
		if(data!=null)
		{
			switch(data)
			{
				case "#abort":
					log("transfer aborted");
					file.close();
					file_remove(file.name);
					return false;
				case "#eof":
					log("file received: " + filename);
					file.close();
					file_rename(fname,fname+".bck");
					file_rename(file.name,fname);
					file_utime(filename,time(),filedate);
					parent_queue.write({'filename':filename,'session_id':session_id,'filedate':filedate,'remote_ip_address':sock.remote_ip_address});
					return true;
				default:
					file.writeln(data);
					break;
			}
		} else {
			log(sock.error);
			log("transfer timed out: " + filename);
			file.close();
			file_remove(file.name);
			return false;
		}
	}
	file.close();
	file_remove(file.name);
	return false;
}
function hub_route(hub_address,hub_port)
{
	log("routing data to hub");
	var hub=new Socket();
	hub.connect(hub_address,hub_port,connection_timeout);
	hub.send(query + "\r\n");
	log("sent query: " + query);
	
	while(hub.is_connected && sock.is_writeable)
	{
		if(hub.data_waiting) 
		{
			sock.send(hub.recvline(1024,connection_timeout)+"\r\n");
		}
		if(sock.data_waiting) 
		{
			hub.send(sock.recvline(1024,connection_timeout)+"\r\n");
		}
	}
}

var descriptor=argv[0];
var query=argv[1];
var hub_address=argv[2];
var hub_port=argv[3];

var sock=new Socket();
sock.descriptor=descriptor;

if(hub_address && hub_port) hub_route(hub_address,hub_port);
else comm_sync();

