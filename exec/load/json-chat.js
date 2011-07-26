if(!js.global.JSONClient) 
	load(js.global,"json-client.js");
	
function JSONChat(jsonclient) {

	this.nick = new Nick(user.handle,system.name,user.ip_address);
	this.channels = {};
	this.client = jsonclient;
	if(!this.client) 
		this.client = new JSONClient();

	this.connect = function() {
		this.client.subscribe("chat.channels." + this.nick.name + ".messages");
	}
	
	this.submit = function(target,str) {
		var message = new Message(this.nick,str,Date.now());
		this.client.write("chat.channels." + target + ".messages",message,2);
		this.client.push("chat.channels." + target + ".history",message,2);
		this.channels[target.toUpperCase()].messages.push(message);
	}
	
	this.clear = function(target) {
		this.client.write("chat.channels." + target + ".history",[],2);
		this.channels[target.toUpperCase()].messages = [];
	}
	
	this.join = function(target,str) {
		this.client.subscribe("chat.channels." + target + ".messages");
		this.channels[target.toUpperCase()] = new Channel(target);
		var history = this.client.read("chat.channels." + target + ".history",1);
		var msgcount = 0;
		for each(var m in history) {
			this.channels[target.toUpperCase()].messages.push(m);
			msgcount++;
		}
		if(msgcount == 0) 
			this.clear(target);
		this.channels[target.toUpperCase()].users = this.client.who("chat.channels." + target + ".messages");
	}
	
	this.part = function(target) {
		this.client.unsubscribe("chat.channels." + target + ".messages");
		delete this.channels[target.toUpperCase()];
	}
	
	this.who = function(target,str) {
		this.channels[target.toUpperCase()].users = this.client.who("chat.channels." + target + ".messages");
		return this.channels[target.toUpperCase()].users;
	}
	
	this.disconnect = function() {
		this.client.unsubscribe("chat." + this.nick.name + ".messages");
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
}
