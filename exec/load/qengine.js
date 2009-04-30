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
	this.queuelog=		(log_?log_:false);

	this.notices=[];
	this.stream=	new Queue(this.name + "." + user.alias);
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
				this.log("-Receiving signed data: " + ident);
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
			this.log("sending: " + ident + " to user: " + this.users[user_].name);
		}
	}
	this.FindUser=function(id)
	{
		var exclusive;
		if(!this.user_file.is_open) 
		{
			exclusive=true;
			this.user_file.open('r+',true);
		}
		var section_list=this.user_file.iniGetSections();
		for(section in section_list)
		{
			var user_list=this.user_file.iniGetKeys(section_list[section]);
			for(u in user_list)
			{
				if(user_list[u]==id) return section_list[section];
			}
		}
		if(exclusive) this.user_file.close();
		return false;
	}
	this.UpdateUsers=function()
	{
		if(file_date(this.user_file.name)==this.last_user_update) return;
		this.last_user_update=file_date(this.user_file.name);
		this.user_file.open('r+',true);
		var user_list=this.user_file.iniGetKeys(this.thread);
		for(user_ in user_list)
		{
			var user_fullname=user_list[user_];
			var user_alias=user_fullname.substr(user_fullname.indexOf(".")+1);
			var user_status=this.user_file.iniGetValue(this.thread,user_fullname);
			if(user_alias!=user.alias && !this.users[user_fullname])
			{
				var user_queue=this.name+"."+user_alias;
				this.users[user_fullname]=new QueueUser(user_fullname,user_status,user_queue);
				this.notices.push(user_alias + " is here.");
				this.log("loaded new user: " + user_fullname);
			}
		}
		for(user_ in this.users) 
		{
			var usr=this.users[user_];
			if(!this.user_file.iniGetValue(this.thread,usr.name))
			{
				var room=this.FindUser(usr.name);
				if(room)
				{
					this.notices.push(usr.alias + " joined " + room + ".");
				}
				else 
				{
					this.notices.push(usr.alias + " has left.");
				}
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
			this.user_file.open((file_exists(this.user_file.name) ? 'r+':'w+'),true); 	
			this.user_file.iniRemoveKey(this.thread,system.qwk_id + "." + user.alias);
			this.thread=thread;
			KillThread=this.thread;
			this.log("Initializing queue: room: " + this.thread + " file: " + this.user_file.name + " time: " + time());
			this.user_file.iniSetValue(thread,system.qwk_id + "." + user.alias,"(" + status + ")");
			this.user_file.close();
		}
	}
	this.log=function(text)
	{
		if(this.queuelog) this.queuelog.Log(text);
	}
}
function QueueUser(user_name,user_status,user_queue)
{
	this.name=user_name;
	this.alias=this.name.substr(this.name.indexOf(".")+1);
	this.status=user_status;
	this.queue=new Queue(user_queue);
	this.Send=function(data,ident)
	{
		this.queue.write(data,ident);
	}
}
function QuitQueue(userfile,thread)
{
	userfile.open("r+",true);
	userfile.iniRemoveKey(thread,system.qwk_id + "." + user.alias);
	userfile.close();
}
