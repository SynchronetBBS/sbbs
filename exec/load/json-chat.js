if(!js.global.JSONClient)
	load(js.global,"json-client.js");

function JSONChat(jsonclient,host,port,usernum) {

	this.nick;
	this.channels = {};
	this.client = jsonclient;
	
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
			this.nick = new Nick(usr.handle,system.name,usr.ip_address);
		else if(user && user.number > 0)
			this.nick = new Nick(user.handle,system.name,user.ip_address);
		if(this.nick)
			this.client.subscribe("chat","channels." + this.nick.name + ".messages");
		else
			throw("invalid user number");
	}
	
	this.submit = function(target,str) {
		var message = new Message(this.nick,str,Date.now());
		var chan = this.channels[target.toUpperCase()];
		this.client.write("chat","channels." + chan.name + ".messages",message,2);
		this.client.push("chat","channels." + chan.name + ".history",message,2);
		chan.messages.push(message);
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
		if(arr.shift().toUpperCase() != "CHAT") 
			return false;
			
		var channel;
		var usr;
		
		while(arr.length > 0) {
			switch(arr.shift().toUpperCase()) {
			case "CHANNELS":
				channel = this.channels[arr[0].toUpperCase()];
				break;
			case "MESSAGES":
				channel.messages.push(packet.data);
				break;
			case "USERS":
				usr = channel.users[arr[0].toUpperCase()];
				break;
			}
		}
	}
	
	/* check client for update packets */
	this.cycle = function() {
		this.client.cycle();
		while(this.client.updates.length) 
			this.update(this.client.updates.shift());
	}

	/* process chat commands */
	this.handle_command = function(target,cmdstr) {
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
				this.channels[target.toUpperCase()].messages.push(users[u]);
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
		}
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
	
	this.connect();
}
