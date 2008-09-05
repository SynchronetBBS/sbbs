/*
	JAVASCRIPT MULTI-USER ENGINE -- BY MCMLXXIX (Matt Johnson) 05/2008
	USING  FILE I/O FOR USER PRESENCE DETECTION & NAMED QUEUES FOR REAL-TIME MULTIPLAYER DATA
*/

load("sbbsdefs.js");
load("logging.js");
load("funclib.js");

var QueueEngineLog;

function DataQueue(root,name,log_)
{
	QueueEngineLog=(log_?log_:new Logger(root,name));

	this.root=root;
	this.name=name;
	this.users=[];
	this.notices=[];
	this.stream=	new Queue(this.name + "_" + user.number);
	this.user_file=	new File(this.root + user.number + ".usr");
	
	this.ReceiveData=function(ident)
	{
		this.UpdateUsers();
		var data=[];
		if(this.stream.poll(ident)) {
			while(this.stream.poll(ident)) 
			{
				var incoming_data=this.stream.read(ident);
				Log("received data from user: " + system.username(incoming_data.user));
				data.push=incoming_data;
			}
		}
		return data;
	}
	this.SendData=function(data,ident)
	{
		for(user_ in this.users) 
		{
			this.UpdateUsers();
			this.users[user_].Send(data,ident);
			Log("sending data to user: " + this.users[user_].user);
		}
	}
	this.UpdateUsers=function()
	{
		var user_list=directory(this.root + "*.usr");
		for(user_ in user_list)
		{
			var user_file=user_list[user_];
			var user_file_name=file_getname(user_file);
			var user_number=user_file_name.substring(0,user_file_name.indexOf("."));
			if(!this.users[user_number] && user_number!=user.number) 
			{
				var user_queue=this.name+"_"+user_number;
				this.users[user_number]=new QueueUser(user_number,user_file,user_queue);
				this.notices.push(system.username(user_number) + " is here.");
				Log("loaded new user: " + system.username(user_number));
			}
		}
		for(user_ in this.users) 
		{
			if(!file_exists(this.users[user_].file))
			{
				delete this.users[user_];
				this.notices.push(system.username(user_) + " has left.");
				Log("removing absent user: " + system.username(user_));
			}
		}
	}
	this.Init=function(data)
	{
		this.UpdateUsers();
		this.user_file.open('w+');
		if(data) this.user_file.writeAll(data);
		this.user_file.close();
	}
	function Log(text)
	{
		QueueEngineLog.Log(text);
	}
	this.Init();
}
function QueueUser(user_number,user_file,user_queue)
{
	this.user=user_number;
	this.file=user_file;
	this.queue=new Queue(user_queue);
	this.Send=function(data,ident)
	{
		this.queue.write(data,ident);
	}
}
