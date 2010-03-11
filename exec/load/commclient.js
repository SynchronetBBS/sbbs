/*
	Inter-BBS/Inter-Node client (for use with commservice.js)
	for Synchronet v3.15+
	by Matt Johnson - 2009
*/

load("funclib.js");
load("sbbsdefs.js");
load("sockdefs.js");
	
const normal_scope=		"#";
const global_scope=		"!";
const hub_query=			"?";
const priv_scope=			"%";
const file_sync=			"@";
const tries=				5;
const connection_timeout=	5;

function ServiceConnection(id)
{
	this.session_id=			(id?id:"default");
	this.notices=				[];

	var comminit=new File(system.ctrl_dir + "filesync.ini");
	comminit.open('r');
	var module=comminit.iniGetObject(this.session_id);
	comminit.close();
	
	const working_dir=module.working_directory;
	
	//hub should point to the local, internal ip address or domain of the computer running commservice.js
	//port should point to the port the gaming service is set to use in commservice.js
	const hub=						"localhost";
	const port=					10088;
	
	var sock=new Socket();
	this.init=function()
	{
		this.notices.push("Connecting to hub... ");
		sock.connect(hub,port,connection_timeout);
		if(sock.is_connected) 
		{
			sock.send("&" + this.session_id + "\r\n");
			return true;
		}
		this.notices.push("Connection to " + hub + " failed...");
		return false;
	}
	this.getNotices=function()
	{
		if(this.notices.length) return this.notices;
		return false;
	}
	this.receive=function()
	{
		var data=0;
		if(!sock.is_connected)
		{
			if(!this.init()) return -1;
		}
		if(sock.data_waiting)
		{
			var raw_data=sock.recvline(16384,connection_timeout);
			data=js.eval(raw_data);
		}
		return data;
	}
	this.send=function(data)
	{
		if(!sock.is_connected)
		{
			if(!this.init()) return false;
		}
		if(!data.scope) 
		{
			log("scope undefined");
			return false;
		}
		var packet=data.scope + this.session_id + "" + data.toSource() + "\r\n";
		sock.send(packet);
		return true;
	}
	this.recvfile=function(remote_file)
	{
		if(!sock.is_connected)
		{
			if(!this.init()) return false;
		}
		var filesock=new Socket();
		filesock.connect(hub,port,connection_timeout);
		if(!filesock.is_connected) { 
			log("error connecting to service");
			return false;
		}
		
		filesock.send(file_sync + this.session_id + "#send" + file_getname(remote_file) + "\r\n");
		while(filesock.is_connected)
		{
			var data=filesock.recvline(1024,connection_timeout);
			if(data!=null)
			{
				log(data);
				data=parse_query(data);
				var response=data[1];
				var file=data[2];
				var date=data[3];
				
				switch(response)
				{
					case "#askrecv":
						receive_file(filesock,this.session_id,file,date);
						break;
					case "#abort":
						filesock.close();
						break;
					case "#endquery":
						filesock.close();
						break;
					default:
						filesock.close();
						log("unknown response: " + response);
						break;
				}
			} else {
				log("transfer timed out: " + (file?file:remote_file));
				filesock.close();
			}
		}
	}
	this.sendfile=function(local_file)
	{
		if(!sock.is_connected)
		{
			if(!this.init()) return false;
		}
		
		var filesock=new Socket();
		filesock.connect(hub,port,connection_timeout);
		if(!filesock.is_connected) { 
			log("error connecting to service");
			return false;
		}
		
		var files=directory(working_dir + file_getname(local_file));
		for(var f=0;f<files.length && filesock.is_connected;f++)
		{
			var filename=file_getname(files[f]);
			var filedate=file_date(files[f]);

			filesock.send(file_sync + this.session_id + "#askrecv" + filename + "" + filedate + "\r\n");
			var data=filesock.recvline(1024,connection_timeout);
			if(data!=null)
			{
				data=parse_query(data);
				var response=data[1];
				switch(response)
				{
					case "#ok":
						log("sending file: " + filename);
						filesock.sendfile(files[f]);
						filesock.send("#eof\r\n");
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
						filesock.send("#abort\r\n");
						return false;
				}
			} else {
				log("query timed out");
				break;
			}
		}
		filesock.send("#endquery\r\n");
		filesock.close();
		return true;
	}
	this.close=function()
	{
		log("terminating service connection");
		sock.close();
	}

	function receive_file(socket,session_id,filename,filedate)
	{
		filename=working_dir+filename;
		if(file_date(filename)==filedate)
		{
			log("skipping file: " + file_getname(filename));
			socket.send(file_sync + session_id + "#skip\r\n");
			return false;
		}
		
		socket.send(file_sync + session_id + "#ok\r\n");
		var file=new File(filename + ".tmp");
		file.open('w',false);
		log("receiving file: " + file_getname(filename));
		while(socket.is_connected)
		{
			var data=socket.recvline(1024,connection_timeout);
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
						file_rename(filename,filename+".bck");
						file_rename(file.name,filename);
						file_utime(filename,time(),filedate);
						return true;
					default:
						file.writeln(data);
						break;
				}
			} else {
				log("transfer timed out: " +  file_getname(filename));
				file.close();
				file_remove(file.name);
				return false;
			}
		}
		log("socket connection error");
		file.close();
		file_remove(file.name);
		return false;
	}
	function parse_query(query)
	{
		if(query) return query.split("");
		return false;
	}

	if(!sock.is_connected)
	{
		if(!this.init()) return false;
	}
}

	
