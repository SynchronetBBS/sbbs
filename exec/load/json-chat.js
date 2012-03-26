if(!js.global.JSONClient)
	load(js.global,"json-client.js");

function JSONChat(usernum,jsonclient,host,port) {

	this.nick;
	this.channels = {};
	this.client = jsonclient;
	this.view = undefined;
	this.settings = {
		NICK_COLOR:GREEN,
		TEXT_COLOR:LIGHTGRAY,
		PRIV_COLOR:RED,
		ACTION_COLOR:LIGHTGREEN,
		NOTICE_COLOR:BROWN
	}
	
	if(!this.client) {
		if(!host || isNaN(port))
			throw("invalid client arguments");
		this.client = new JSONClient(host,port);
	}
		
	this.connect = function() {
		var usr;
		if(usernum > 0 && system.username(usernum)) 
			usr = new User(usernum);
		if(usr) 
			this.nick = new Nick(usr.alias,system.name,usr.ip_address);
		else if(user && user.number > 0)
			this.nick = new Nick(user.alias,system.name,user.ip_address);
		if(this.nick)
			this.client.subscribe("chat","channels." + this.nick.name + ".messages");
		else
			throw("invalid user number");
	}
	
	this.submit = function(target,str) {
		/* if the string has been passed with a leading '/' */
		if(str[0] == "/") 
			return this.getcmd(target,str);
		var message = new Message(this.nick,str,Date.now());
		var chan = this.channels[target.toUpperCase()];
		this.client.write("chat","channels." + chan.name + ".messages",message,2);
		this.client.push("chat","channels." + chan.name + ".history",message,2);
		chan.messages.push(message);
		return true;
	}
	
	this.clear = function(target) {
		var chan = this.channels[target.toUpperCase()];
		this.client.write("chat","channels." + chan.name + ".history",[],2);
		chan.messages = [];
	}
	
	this.join = function(target,str) {
		this.client.subscribe("chat","channels." + target + ".messages");
		this.channels[target.toUpperCase()] = new Channel(target);
		var history = this.client.read("chat","channels." + target + ".history",1);
		var msgcount = 0;
		for each(var m in history) {
			this.channels[target.toUpperCase()].messages.push(m);
			msgcount++;
		}
		if(msgcount == 0) 
			this.clear(target);
		this.channels[target.toUpperCase()].users = this.client.who("chat","channels." + target + ".messages");
	}
	
	this.part = function(target) {
		var chan = this.channels[target.toUpperCase()];
		this.client.unsubscribe("chat","channels." + chan.name + ".messages");
		delete this.channels[target.toUpperCase()];
	}
	
	this.who = function(target,str) {
		var chan = this.channels[target.toUpperCase()];
		chan.users = this.client.who("chat","channels." + chan.name + ".messages");
		return chan.users;
	}
	
	this.disconnect = function() {
		this.client.unsubscribe("chat","channels." + this.nick.name + ".messages");
		for each(var c in this.channels) 
			this.client.unsubscribe("chat","channels." + c.name + ".messages");
		this.channels = {};
	}
	
	/* pass any client update packets to this function to process inbound messages/status updates */
	this.update = function(packet) {
		var arr = packet.location.split(".");
		var channel;
		var usr;
		var message;
			
		while(arr.length > 0) {
			switch(arr.shift().toUpperCase()) {
			case "CHANNELS":
				channel = this.channels[arr[0].toUpperCase()];
				break;
			case "MESSAGES":
				message = packet.data;
				break;
			case "USERS":
				usr = channel.users[arr[0].toUpperCase()];
				break;
			}
		}
		
		switch(packet.oper.toUpperCase()) {
		case "SUBSCRIBE":
			channel.messages.push(new Message("",packet.data.nick + " is here.",Date.now()));
			break;
		case "UNSUBSCRIBE":
			channel.messages.push(new Message("",packet.data.nick + " has left.",Date.now()));
			break;
		case "WRITE":
			channel.messages.push(message);
			break;
		default:
			log(LOG_WARNING,"Unhandled response");
			break;
		}
		return true;
	}
	
	/* check client for update packets */
	this.cycle = function() {
		this.client.cycle();
		while(this.client.updates.length) 
			this.update(this.client.updates.shift());
		if(this.view)
			updateChatView(this.view,this);
		return true;
	}

	/* process chat commands */
	this.getcmd = function(target,cmdstr) {
		/* if the command string is empty */
		if(!cmdstr) 
			return false;
			
		/* if the command has been passed with a leading '/' */
		if(cmdstr[0] == "/")
			cmdstr = cmdstr.substr(1);
			
		cmdstr = cmdstr.split(" ");
		switch(cmdstr[0].toUpperCase()) {
		case "JOIN":
			cmdstr.shift();
			var chan = cmdstr.shift();
			if(chan)
				this.join(chan,cmdstr.join(" "));
			break;
		case "PART":
			cmdstr.shift();
			var chan = cmdstr.shift();
			if(!chan)
				chan = target;
			this.part(chan);
			break;
		case "KICK":
			// todo
			break;
		case "CLEAR":	
			this.clear(target);
			break;
		case "WHO":
			var users = this.who(target);
			for(var u in users)
				this.channels[target.toUpperCase()].users.push(users[u]);
			break;
		case "INVITE":
			break;
		case "DISCONNECT":
		case "CLOSE":
			this.disconnect();
			break;
		case "CONNECT":
		case "OPEN":
			this.connect();
			break;
		case "IGNORE":
			// todo
			break;
		case "BAN":
			// todo
			break;
		default:
			return false;
		}
		return true;
	}
	
	/* user identification object */
	function Nick(name,host,ip)	{
		this.name = name;
		this.host = host;
		this.ip = ip;
	}
	
	/* channel object (stores users and messages) */
	function Channel(name) {
		this.name = name;
		this.messages = [];
		this.users = [];
	}
	
	/* message object (Nick, String, Time) */
	function Message(nick,str,time) {
		this.nick = nick;
		this.str = str;
		this.time = time;
	}
	
	/* adapter for updating layout views */
	function updateChatView(view,chat) {
		for each(var c in chat.channels) {
			var found = false;
			for each(var t in view.tabs) {
				if(t.title == c.name) {
					found = true;
					break;
				}
			}
			if(!found) {
				view.addTab(c.name,"chat",chat);
			}
		}
	}
	
	/* constructor */
	this.connect();
}
