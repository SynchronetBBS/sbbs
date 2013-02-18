if(!js.global.JSONClient)
	load(js.global,"json-client.js");

function JSONChat(usernum,jsonclient,host,port) {

	this.nick;
	this.channels = {};
	this.client = jsonclient;
	this.chatView = undefined;
	this.userView = undefined;
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
		/* ignore escape sequences */
		switch(str) {
		case KEY_UP:
		case KEY_DOWN:
		case KEY_LEFT:
		case KEY_RIGHT:
		case KEY_HOME:
		case KEY_END:
		case KEY_DEL:
		case "\x1b":
			return false;
		}
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
		if(this.chatView) {
			var tab =  this.chatView.getTab(target)
			if(tab)
				tab.frame.clear();
		}
	} 
	
	this.join = function(target,str) {
		this.client.subscribe("chat","channels." + target + ".messages");
		this.channels[target.toUpperCase()] = new Channel(target);
		var history = this.client.read("chat","channels." + target + ".history",1);
		var msgcount = 0;
		var lastMsg = 0;
		for each(var m in history) {
			this.channels[target.toUpperCase()].messages.push(m);
			lastMsg = m.time;
			msgcount++;
		}
		if(msgcount == 0) {
			this.clear(target);
		}
		else {
			var d = new Date(lastMsg);
			var str = format("Last msg: %.2d:%.2d on %.2d/%.2d/%.4d",
				(d.getHours()+1),(d.getMinutes()+1),(d.getMonth()+1),d.getDate(),d.getFullYear());
			this.channels[target.toUpperCase()].messages.push(
				new Message("",str,Date.now())
			);
		}
		this.who(target);
	}
	
	this.part = function(target) {
		var chan = this.channels[target.toUpperCase()];
		this.client.unsubscribe("chat","channels." + chan.name + ".messages");
		delete this.channels[target.toUpperCase()];
	}
	
	this.who = function(target) {
		var chan = this.channels[target.toUpperCase()];
		var users = this.client.who("chat","channels." + chan.name + ".messages");
		if(chan)
			chan.users = users;
		if(this.userView)
			updateUserView(this.userView,chan);
		var uList=[];
		for(var u in users) {
			uList.push(users[u].nick);
		}
		chan.messages.push(new Message(undefined,"Users in " + chan.name + ": " + uList.join(", "),Date.now()));
		return users;
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
			this.who(channel.name);
			break;
		case "UNSUBSCRIBE":
			channel.messages.push(new Message("",packet.data.nick + " has left.",Date.now()));
			this.who(channel.name);
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
		if(this.chatView)
			syncChatView(this.chatView,this);
		if(this.userView)
			syncUserView(this.userView,this);
		return true;
	}

	/* perform an action */
	this.action = function(target,action) {
		var message = new Message(undefined,this.nick.name + " " + action,Date.now());
		var chan = this.channels[target.toUpperCase()];
		this.client.write("chat","channels." + chan.name + ".messages",message,2);
		this.client.push("chat","channels." + chan.name + ".history",message,2);
		chan.messages.push(message);
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
		case "J":
		case "JOIN":
			cmdstr.shift();
			var chan = cmdstr.shift();
			if(chan)
				this.join(chan,cmdstr.join(" "));
			break;
		case "P":
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
			cmdstr.shift();
			var cname = cmdstr.shift();
			if(!cname)
				cname = target;
			this.who(cname);
			break;
		case "ME":
			cmdstr.shift();
			this.action(target,cmdstr.join(" "));
			break;
		case "INVITE":
			cmdstr.shift();
			var usr = cmdstr.join(" ");
			var message = new Message(undefined,this.nick.name + " has invited you to " + target,Date.now());
			this.client.write("chat","channels." + usr + ".messages",message,2);
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
	
	/* adapter for updating chat layout view */
	function syncChatView(view,chat) {
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
		for (var t = 0;t<view.tabs.length;t++) {
			if(!chat.channels[view.tabs[t].title.toUpperCase()]) {
				view.delTab(t--);
			}
		}
	}
	
	/* adapter for updating user list layout view */
	function syncUserView(view,chat) {
		for each(var c in chat.channels) {
			var found = false;
			for each(var t in view.tabs) {
				if(t.title == c.name) {
					found = true;
					break;
				}
			}
			if(!found) {
				view.addTab(c.name);
				updateUserView(view,c);
			}
		}
		for (var t = 0;t<view.tabs.length;t++) {
			if(!chat.channels[view.tabs[t].title.toUpperCase()]) {
				view.delTab(t--);
			}
		}
	}
	
	/* adapter for listing channel users */
	function updateUserView(view,chan) {
		var tab = view.getTab(chan.name);
		if(tab && chan.users) {
			tab.frame.clear();
			for each(var u in chan.users)
				tab.frame.putmsg(u.nick + "\r\n");
		}
	}
	
	/* constructor */
	this.connect();
}
