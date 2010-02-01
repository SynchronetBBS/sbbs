const connection_timeout=2;
const tries=5;

function comm_sync()
{
	var data=parse_query(query);
	var session_id=data[0].substr(1);
	var task=data[1];
	var filemask=data[2];
	var filedate=data[3];
	
	log("synchronizing files: " + filemask);
	var comminit=new File(system.ctrl_dir + "commsync.ini");
	comminit.open('r');
	var module=comminit.iniGetObject(session_id);
	comminit.close();

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
function parse_query(query)
{
	if(query) return query.split("");
	return false;
}
function sync_remote(session_id,dir,filemask)
{
	var files=directory(dir + file_getname(filemask));
	for(var f=0;f<files.length;f++)
	{
		var filename=file_getname(files[f]);
		var filedate=file_date(files[f]);
		
		sock.send("@" + session_id + "#askrecv" + filename + "" + filedate + "\r\n");
		var data=sock.recvline(1024,connection_timeout);
		data=parse_query(data);
		var response=data[1];
		
		switch(response)
		{
			case "#ok":
				log("sending file: " + filename);
				sock.sendfile(files[f]);
				sock.send("#eof\r\n");
				log("file sent: " + filename);
				break;
			case "#skip":
				log("skipping file");
				break;
			case "#abort":
				log("aborting query");
				return false;
			default:
				log("unknown response: " + response);
				sock.send("#abort\r\n");
				return false;
		}
	}
	sock.send("@" + session_id + "#endquery\r\n");
	sock.close();
	return true;
}
function sync_local(session_id,dir,filemask)
{
	log("retrieving files: " + filemask);
	if(!sock.is_connected) log("connection interrupted");
	sock.send("@" + session_id + "#send" + file_getname(filemask) + "\r\n");
	while(sock.is_connected)
	{
		var data=sock.recvline(1024,connection_timeout);
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
				return false;
			case "#endquery":
				return true;
			default:
				log("unknown response: " + response);
				return false;
		}
	}
	sock.close();
	log("complete");
}
function receive_file(session_id,dir,filename,filedate)
{
	fname=dir+filename;
	log("receiving file: " + filename);
	if(file_date(fname)==filedate)
	{
		sock.send("@" + session_id + "#skip\r\n");
		return false;
	}
	sock.send("@" + session_id + "#ok\r\n");

	var file=new File(fname + ".tmp");
	file.open('w',false);
	var count=0;

	while(count<50)
	{
		var data=false;
		if(sock.data_waiting) data=sock.recvline(1024,connection_timeout);
		if(data)
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
					file_utime(file.name,time(),filedate);
					file_remove(fname);
					file_rename(file.name,fname);
					parent_queue.write({'filename':filename,'session_id':session_id,'filedate':filedate,'remote_ip_address':sock.remote_ip_address});
					return true;
				default:
					file.writeln(data);
					break;
				
			}
		}
		else 
		{
			mswait(25);
			count++;
		}
	}
	log("transfer timed out");
	file.close();
	file_remove(file.name);
	return false;
}
function hub_route(hub_address,hub_port)
{
	log("routing data to hub");
	var hub=new Socket();
	hub.bind(0,"localhost");
	hub.connect(hub_address,hub_port,connection_timeout);
	hub.send(query + "\r\n");
	log("sent query: " + query);
	
	var count=0;
	while(count<50)
	{
		var data=false;
		if(hub.data_waiting) 
		{
			sock.send(hub.recvline(1024,connection_timeout)+"\r\n");
			data=true;
		}
		if(sock.data_waiting) 
		{
			hub.send(sock.recvline(1024,connection_timeout)+"\r\n");
			data=true;
		}
		if(!data) 
		{
			count++;
			mswait(25);
		}
	}
}

var descriptor=argv[0];
var query=argv[1];
var hub_address=argv[2];
var hub_port=argv[3];

var sock=new Socket();
sock.bind(0,"localhost");
sock.descriptor=descriptor;

if(hub_address && hub_port) hub_route(hub_address,hub_port);
else comm_sync();

