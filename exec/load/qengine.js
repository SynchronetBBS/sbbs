/*
	JAVASCRIPT MULTI-USER ENGINE -- BY MCMLXXIX (Matt Johnson) 05/2008
	USING  FILE I/O FOR USER PRESENCE DETECTION & NAMED QUEUES FOR REAL-TIME MULTIPLAYER DATA
*/

load("sbbsdefs.js");
load("logging.js");
load("funclib.js");

var KillFile;
var KillThread;
js.on_exit("QuitQueue(KillFile,KillThread)");

function DataQueue(root,name,log_)
{
	this.root=			(root?root:"/sbbs/");
	this.name=			(name?name:"default");
	this.queuelog=		(log_?log_:new Logger(this.root,this.name));

	this.notices=[];
	this.stream=	new Queue(this.name + "." + user.number);
	this.user_file=	new File(this.root + "users.lst");
	this.last_user_update=0;
	this.users;
	this.thread;

	KillFile=this.user_file;
	
	this.ReceiveData=function(ident)
	{
		this.UpdateUsers();
		var data=[];
		if(ident)
		{
			while(this.stream.poll()==ident)
			{
				var incoming_data=this.stream.read(ident);
				this.log("-Receiving signed data");
				data.push=incoming_data;
			}
		}
		else 
		{
			while(this.stream.data_waiting)
			{
				var incoming_data=this.stream.read();
				this.log("-Receiving unsigned data");
				data.push=incoming_data;
			}
		}
		return data;
	}
	this.SendData=function(data,ident)
	{
		this.UpdateUsers();
		for(user_ in this.users) 
		{
			this.users[user_].Send(data,ident);
			this.log("sending data to user: " + this.users[user_].name);
		}
	}
	this.UpdateUsers=function()
	{
		if(this.user_file.date==this.last_user_update) return;
		this.last_user_update=this.user_file.date;
		this.user_file.open('r+');
		var user_list=this.user_file.iniGetKeys(this.thread);
		for(user_ in user_list)
		{
			var user_name=user_list[user_];
			var user_status=this.user_file.iniGetValue(this.thread,user_name);
			var user_number=system.matchuser(user_name);

			if(user_name!=user.alias && !this.users[user_name])
			{
				var user_queue=this.name+"."+user_number;
				this.users[user_name]=new QueueUser(user_name,user_status,user_queue);
				this.notices.push("\1r\1h" + user_name + " is here.");
				this.log("loaded new user: " + user_name);
			}
		}
		for(user_ in this.users) 
		{
			if(!this.user_file.iniGetValue(this.thread,this.users[user_].name))
			{
				this.notices.push("\1r\1h" + this.users[user_].name + " has left.");
				this.log("removing absent user: " + this.users[user_].name);
				delete this.users[user_];
			}
		}
		this.user_file.close();
	}
	this.DataWaiting=function(ident)
	{
		if(this.stream.poll()==ident) return true;
		return false;
	}
	this.Init=function(thread,status)
	{
		if(thread==this.thread) return;
		else 
		{
			this.users=[];
			this.UpdateUsers();
			this.user_file.open(file_exists(this.user_file.name) ? 'r+':'w+'); 	
			this.user_file.iniRemoveKey(this.thread,user.alias);
			this.thread=thread;
			KillThread=this.thread;
			this.log("Initializing queue: room: " + this.thread + " file: " + this.user_file.name);
			this.user_file.iniSetValue(thread,user.alias,"(" + status + ")");
			this.user_file.close();
		}
	}
	this.log=function(text)
	{
		this.queuelog.Log(text);
	}
}
function QueueUser(user_name,user_status,user_queue)
{
	this.name=user_name;
	this.status=user_status;
	this.queue=new Queue(user_queue);
	this.Send=function(data,ident)
	{
		this.queue.write(data,ident);
	}
}
function QuitQueue(userfile,thread)
{
	userfile.open("r+");
	userfile.iniRemoveKey(thread,user.alias);
	userfile.close();
}
