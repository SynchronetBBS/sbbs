/* IRC protocol initialization */
function IRC_init() {
	this.encoding="RFC1459";
	this.parse=IRC_parse;
	this.register=IRC_register;
	this.encode=IRC_encode;
	this.decode=IRC_decode;
	this.process=IRC_process;
	this.onConnect=IRC_onConnect;
	this.onDisconnect=IRC_onDisconnect;
	this.cycleProtocol=IRC_cycle;
	this.quit=IRC_quit;
	
	if(!this.channels["IRC"]) {
		var chan=new IRC_Channel("irc");
		chan.joinable=false;
		this.channels["IRC"]=chan;
		this.server_chan=chan;
	}
}

/* IRC protocol objects */
function IRC_Channel(name) {
	this.type="IRC";
	this.name=name;
	this.joinable=true;
	this.joined=true;
	this.update_users=false;
	this.users=[];
	this.message_list=[];
	this.post=function(data) {
		this.message_list.push(data);
	}
}

function IRC_User(nick,modes) {
	this.nick=nick;
	this.modes=modes || [];
}

/* IRC protocol methods */
function IRC_onDisconnect() {
	this.encoding="RFC1459";
	for each(var c in this.channels) 
		c.joined=false;
}

function IRC_onConnect() {
	for each(var c in this.channels) {
		if(c.joinable) {
			var data=new IRC_Commands["JOIN"](c.name,this.nick.nickname);
			this.send(data);
		}
	}
}

function IRC_cycle() {
	/* if we have received data (thus verifying readability) */
	if(this.connection_established && !this.registered) {
		/* 	if we are connected but are not registered, 
			register client */
		if(!this.registering) {
			this.register();
			this.registering=true;
		}
	}
}

function IRC_encode(data) {
	if(this.encoding=="JSON") {
		return JSON_encode.apply(this,arguments);
	} 
	else if(this.encoding=="RFC1459") {
		return RFC1459_encode.apply(this,arguments);
	}
	return false;
}

function IRC_decode(raw_data) {
	if(this.encoding=="JSON") {
		return JSON_decode.apply(this,arguments);
	} 
	else if(this.encoding=="RFC1459") {
		return(RFC1459_decode.apply(this,arguments));
	}
	return false;
}

function IRC_quit() {
	var data=new IRC_Commands["QUIT"](this.nick.nickname,"Quitting.");
	this.send(data);
}

function IRC_register() {
	if(!this.nick) {
		if(user.alias) this.nick=new IRC_Commands["NICK"](user.alias.replace(/[\s.]/g,"_"),user.name,system.inet_addr);
		else this.nick=new IRC_Commands["NICK"]("SYSTEM","SYSTEM",system.inet_addr);
	} 
	this.send(this.nick);
}

function IRC_parse(text,channel) {
	var data=false;
	
	/* process offline commands */
	if(text[0] == "/") {
		cmd=text.split(" ");
		switch(cmd[0].substr(1).toUpperCase()) {
		case "CONNECT":
			if(this.disabled) {
				this.disabled=false;
				this.reset_counters();
			} 
			return false;
		case "DISCONNECT":
			if(!this.disabled) {
				this.disabled=true;
				this.reset_counters();
				this.reset_connection();
			}
			this.enqueue("disconnect","sync");
			return false;
		case "CLOSE": /* close a chat room */
			if(cmd[1]) channel=cmd[1];
			/* check for channel existence */
			if(!this.channels[channel.toUpperCase()]) 
				return false;
			/* remove channel record */
			if(!this.channels[channel.toUpperCase()].joinable) 
				delete this.channels[channel.toUpperCase()];
			/* send part command only for joinable channels */
			else if(this.connected) {
				data=new IRC_Commands["PART"](channel,this.nick.nickname);
				return data;
			}
			return false;
		}
	}
	/* if socket is disconnected */
	if(!this.connected) {
		this.server_chan.post(chat_settings.ERROR_COLOR + 
		"Not connected. Type /connect");
		return false;
	}
	/* if no server message has been received */
	if(!this.connection_established) {
		this.server_chan.post(chat_settings.ERROR_COLOR + 
		"Not connected. Please wait...");
		return false;
	}
	/* if user is not yet registered */
	if(!this.registered) {
		this.server_chan.post(chat_settings.ERROR_COLOR + 
		"Not registered. Please wait...");
		return false;
	}
	/* process online commands */
	if(this.connected) {
		var data=false;
		if(text[0] == "/") {
			cmd=text.split(" ");
			switch(cmd[0].substr(1).toUpperCase()) {
			case "AWAY":
				cmd.shift();
				data=new IRC_Commands["AWAY"](cmd.join(" "));
				break;
			case "BACK":
				data=new IRC_Commands["AWAY"]("");
				break;
			case "KICK":
				cmd.shift();
				data=new IRC_Commands["KICK"](channel,cmd.shift(),cmd.join(" ")); 
				break;
			case "NAMES":
				if(cmd[1]) channel=cmd[1];
				data=new IRC_Commands["NAMES"](channel);
				break;
			case "INVITE":
				data=new IRC_Commands["INVITE"](channel,cmd[1]);
				break;
			case "LIST":
				cmd.shift();
				data=new IRC_Commands["LIST"](cmd.join(" "));
				break;
			case "MODE":
				cmd.shift();
				data=new IRC_Commands["MODE"](channel,cmd.join(" "));
				break;
			case "WHO":
				cmd.shift();
				data=new IRC_Commands["WHO"](cmd.join(" "));
				break;
			case "NICK":
				//ToDo: implement nick changes
				break;
			case "ME":
				cmd.shift();
				data=new IRC_Commands["PRIVMSG"](
				"\x01ACTION " + cmd.join(" ") +"\x01",this.nick.nickname,channel);
				this.process(data);
				break;
			case "PART":
			case "P":
				if(cmd[1]) channel=cmd[1];
				data=new IRC_Commands["PART"](channel,this.nick.nickname);
				break;
			case "PING":
				/* CTCP ping a user */
				if(cmd[1]) {
					data=new IRC_Commands["PRIVMSG"](
					"\x01PING " + Date.now() +"\x01",this.nick.nickname,cmd[1]);
				} 
				/* server ping */
				else {
					data=new IRC_Commands["PING"](Date.now());
				}
				break;
			case "JOIN":
			case "J":
				if(!cmd[1]) {
					this.channels[channel.toUpperCase()].post(
						chat_settings.ERROR_COLOR +
						"usage: /JOIN <channel>");
					break;
				}
				data=new IRC_Commands["JOIN"](cmd[1],this.nick.nickname,cmd[2]);
				break;
			case "MSG":
				cmd.shift();
				var dest=cmd.shift();
				if(!dest) {
					this.channels[channel.toUpperCase()].post(
						chat_settings.ERROR_COLOR +
						"usage: /MSG <target> <text>");
					break;
				}
				data=new IRC_Commands["PRIVMSG"](cmd.join(" "),this.nick.nickname,dest);
				break;
			case "TOPIC":
				cmd.shift();
				data=new IRC_Commands["TOPIC"](cmd.join(" "),this.nick.nickname,channel);
				break;
			case "WHOIS":
				data=new IRC_Commands["WHOIS"](cmd[1]);
				break;
			default:
				data={func:cmd[0].substr(1).toUpperCase()};
				break;
			}
		}
		/* package message to be sent to current channel */
		else {
			data=new IRC_Commands["PRIVMSG"](text,this.nick.nickname,channel);
			this.process(data);
		}
		return data;
	}
}

// Splits a "nick!user@host" string into its three distinct parts.
// RETURNS: An array containing nick in [0], user in [1], and host in [2].
function IRC_split_nuh(str) {
	var tmp = new Array;

	if (str[0] == ":")
		str = str.slice(1);

	if (str.search(/[!]/) != -1) {
		tmp[0] = str.split("!")[0];
		tmp[1] = str.split("!")[1].split("@")[0];
	} else {
		tmp[0] = undefined;
		tmp[1] = str.split("@")[0];
	}
	tmp[2] = str.split("@")[1];
	return tmp;
}

function IRC_process(data) {
	//log(LOG_DEBUG,"<--" + data.toSource());
	if(isNaN(data.func))
		return IRC_processCommand.apply(this,arguments);
	else
		return IRC_processNumeric.apply(this,arguments);
}

/* extension of IRC_process() */
function IRC_processCommand(data) {
	switch(data.func) {
	case "JOIN":
		if(data.origin.toUpperCase() == this.nick.nickname.toUpperCase()) {
			if(!this.channels[data.chan.toUpperCase()])
				this.channels[data.chan.toUpperCase()]=new IRC_Channel(data.chan);
			else 
				this.channels[data.chan.toUpperCase()].joined=true;
		}
		else {
			this.channels[data.chan.toUpperCase()].post(data.origin + " has joined " + data.chan);
		}
		if(!this.channels[data.chan.toUpperCase()].users[data.origin.toUpperCase()])
			this.channels[data.chan.toUpperCase()].users[data.origin.toUpperCase()]=new IRC_User(data.origin);
		this.channels[data.chan.toUpperCase()].update_users=true;
		break;
	case "PART":
		if(data.origin.toUpperCase() == this.nick.nickname.toUpperCase()) {
			if(this.channels[data.chan.toUpperCase()])
				delete this.channels[data.chan.toUpperCase()];
		}
		else {
			delete this.channels[data.chan.toUpperCase()].users[data.origin.toUpperCase()];
			this.channels[data.chan.toUpperCase()].post(data.origin + " has left " + data.chan);
			this.channels[data.chan.toUpperCase()].update_users=true;
		}
		break;
	case "PRIVMSG":
	case "NOTICE":
		var msg="";
		/* if this is CTCP data */
		if(data.msg[0] == "\x01") 
			return IRC_processCTCP.apply(this,arguments);
		/* if this is a normal message */
		if(data.func == "PRIVMSG") 
			msg=data.origin + ": " + data.msg;
		/* if this is a notice */
		else if(data.func == "NOTICE")
			msg="!! " + data.origin + ": " + data.msg;
			
		/* if this is a server message */
		if(data.origin.search(/[.]/) >= 0) {
			this.server_chan.post(msg);
			break;
		}
		/* if this is a user message */
		if(data.dest.toUpperCase() == this.nick.nickname.toUpperCase()) {
			if(!this.channels[data.origin.toUpperCase()]) {
				this.channels[data.origin.toUpperCase()]=new IRC_Channel(data.origin);
				this.channels[data.origin.toUpperCase()].joinable=false;
			}
			this.channels[data.origin.toUpperCase()].post(msg);
			break;
		}
		/* post message to destination channel */
		if(this.channels[data.dest.toUpperCase()]) {
			this.channels[data.dest.toUpperCase()].post(msg);
			break;
		}
		/* in case we are not in the destination channel */
		log(LOG_WARNING,"message received for unknown channel");
		break;
	case "KICK":
		if(data.nick.toUpperCase() == this.nick.nickname.toUpperCase()) {
			if(this.channels[data.chan.toUpperCase()]) 
				this.channels[data.chan.toUpperCase()].joined=false;
			this.server_chan.post(format(
				"You have been kicked from %s (%s)"
				,data.chan,data.str));
		}
		else {
			delete this.channels[data.chan.toUpperCase()].users[data.nick.toUpperCase()];
			this.channels[data.chan.toUpperCase()].post(format(
				"%s has been kicked from %s (%s)"
				,data.nick,data.chan,data.str));		
		}
		this.channels[data.chan.toUpperCase()].update_users=true;
		break;
	case "QUIT":
		for each(var c in this.channels) {
			if(c.users[data.origin.toUpperCase()]) {
				delete c.users[data.origin.toUpperCase()];
				c.post(format("%s quit (%s)"
					,data.origin,data.str));		
			}
			c.update_users=true;
		}
		break;
	case "PING":
		data.func="PONG";
		this.send(data);
		break;
	case "PONG":
		var difference=Date.now()-data.pingtime;
		this.server_chan.post(format(
		"* Ping reply from %s: %dms"
		,data.origin,difference));
		break;
	case "TOPIC":
		if(this.channels[data.chan.toUpperCase()]) {
			this.channels[data.chan.toUpperCase()].post(format(chat_settings.TOPIC_COLOR + 
			"%s has set the topic to: %s"
			,data.origin,data.topic));
		}
		break;
	case "ERROR":
		log(LOG_ERROR,"IRC error: " + data.str);
		break;
	}
	
	return data;
}

/* extension of IRC_process() */
function IRC_processNumeric(data) {
	switch(data.func) {
	case '001':
		this.server_chan.post(chat_settings.SNOTICE_COLOR + 
		"Registered nickname");
		this.registered=true;
		break;
	case '002':
		this.server_name=data.servername;
		this.server_chan.post(
		"Your host is " + data.servername + " running " + data.version);
		break;
	case '003':
		this.server_chan.post(
		"Server created " + data.strftime);
		break;
	case '004':
		break;
	case '005':
		this.server_chan.post(
		data.str + ": are available on this server");
		break;
	case '200':
	case '204':
	case '205':
	case '219':
	case '221':
		break;
	case '250':
		this.server_chan.post(
		"Highest connection count: " + data.totalcount);
		break;
	case '251':
		this.server_chan.post(format(
			"There are %i users and %i invisible on %i servers"
			,data.usercount,data.inviscount,data.servercount));
		break;
	case '252':
		this.server_chan.post(
		data.opercount + " IRC operators online");
		break;
	case '253':
		this.server_chan.post(
		data.unknowncount + " unknown connections");
		break;
	case '254':
		this.server_chan.post(
		data.chancount + " channels formed");
		break;
	case '255':
		this.server_chan.post(format(
		"I have %i clients and %i servers"
		,data.usercount,data.servercount));
		break;
	case '301':
		this.server_chan.post(
		data.nick + " is away");
		break;
	case '305':
		this.server_chan.post(chat_settings.ERROR_COLOR + 
		"You are no longer marked as away");
		break;
	case '306':
		this.server_chan.post(chat_settings.ERROR_COLOR + 
		"You are marked as away");
		break;
	case '315':
		/* /WHO list end */
		break;
	case '311':
	case '314':
		this.server_chan.post(format(
		"%s %s %s * :%s"
		,data.nick,data.uprefix,data.hostname,data.info));
		break;
	case '312':
		this.server_chan.post(format(
		"%s %s :%s"
		,data.nick,data.servername,data.info));
		break;
	case '313':
		this.server_chan.post( 
		data.nick + " is an IRC operator");
		break;
	case '317':
//Numerics[317] = "$nick $time $created :seconds idle, signon time.";
		break;
	case '318':
		this.server_chan.post( 
		data.nick + ": END /WHOIS");
		break;
	case '321':
//Numerics[321] = "Channel :Users  Name"; /* RPL_LISTSTART */
		break;
	case '322':
//Numerics[322] = "$chan $numusers :$topic"; /* RPL_LIST */
		break;
	case '323':
//Numerics[323] = ":End of /LIST."; /* RPL_LISTEND */
		break;
	case '324':
//Numerics[324] = "$chan $mode"; /* RPL_CHANNELMODEIS */
		break;
	case '329':
//Numerics[329] = "$chan $created"; 
		break;
	case '331':
//Numerics[331] = "No topic set for $chan";
		break;
	case '332':
		if(this.channels[data.chan.toUpperCase()]) {
			this.channels[data.chan.toUpperCase()].post(format(chat_settings.TOPIC_COLOR +
			"[%s topic] %s",data.chan,data.str));
		}
		break;
	case '333':
		if(this.channels[data.chan.toUpperCase()]) {
			this.channels[data.chan.toUpperCase()].post(format(chat_settings.TOPIC_COLOR +
			"Topic set by %s - %s"
			,data.changedby,timeStamp(data.topictime)));
		}
		break;
	case '352':
		/* who reply */
		break;
	case '353':
		if(this.channels[data.chan.toUpperCase()]) {
			var ulist=data.str.split(" ");
			for each(var u in ulist) {
				var offset=0;
				var modes=[];
				var finished=false;
				while(!finished) {
					switch(u[offset]) {
					case "@":
					case "+":	
						modes.push(u[offset]);
						offset++;
						break;
					default:
						finished=true;
						break;
					}
				}
				var uname=u.substr(offset);
				this.channels[data.chan.toUpperCase()].users[uname.toUpperCase()]=
					new IRC_User(uname,modes);
			}
			this.channels[data.chan.toUpperCase()].update_users=true;
		}
		break;
	case '366':
		break;
	case '372':
		for each(var l in data.str)
			this.server_chan.post(chat_settings.MOTD_COLOR + l);
		break;
	case '375':
		this.server_chan.post(chat_settings.MOTD_COLOR + 
		"- " + data.servername + " MOTD -");
		break;
	case '376':
		this.server_chan.post(chat_settings.MOTD_COLOR + 
		"- END MOTD -");
		break;
	case '401':
		this.server_chan.post(chat_settings.ERROR_COLOR + 
		data.dest + ": no such channel/nick");
		break;
	case '403':
		this.server_chan.post(chat_settings.ERROR_COLOR + 
		data.dest + ": no such channel");
		break;
	case '404':
		this.server_chan.post(chat_settings.ERROR_COLOR + 
		data.chan + ": can't send to channel");
		break;
	case '405':
		this.server_chan.post(chat_settings.ERROR_COLOR + 
		data.chan + ": can't join more channels");
		break;
	case '406':
		this.server_chan.post(chat_settings.ERROR_COLOR + 
		data.nick + ": there was no such nickname");
		break;
	case '407':
		this.server_chan.post(chat_settings.ERROR_COLOR + 
		data.dest + ": duplicate recipients. No message delivered");
		break;
	case '409':
		this.server_chan.post(chat_settings.ERROR_COLOR + 
		"No origin specified");
		break;
	case '411':
		this.server_chan.post(chat_settings.ERROR_COLOR + 
		"No recipient given (" + data.cmd + ")");
		break;
	case '412':
		this.server_chan.post(chat_settings.ERROR_COLOR + 
		"No text to send");
		break;
	case '413':
//Numerics[413] = "%s :No toplevel domain specified."; /* ERR_NOTOPLEVEL */
		break;
	case '414':
//Numerics[414] = "%s :Wildcard in top level domain."; /* ERR_WILDTOPLEVEL */
		break;
	case '421':
		this.server_chan.post(chat_settings.ERROR_COLOR + 
		data.cmd + ": Unknown command");
		break;
	case '422':
//Numerics[422] = ":MOTD file missing: $filename"; /* ERR_NOMOTD */
		break;
	case '423':
//Numerics[423] = "$servername :No administrative information available.";
		break;
	case '424':
//Numerics[424] = ":File error doing $errno on $filename"; /* ERR_FILEERROR */
		break;
	case '431':
		this.server_chan.post(chat_settings.ERROR_COLOR + 
		"No nickname given");
		break;
	case '432':
		this.server_chan.post(chat_settings.ERROR_COLOR + 
		data.nick + ": foobar'd nickname");
		break;
	case '433':
		this.server_chan.post(chat_settings.ERROR_COLOR + 
		data.nick + ": nickname already in use");
		break;
	case '436':
		this.server_chan.post(chat_settings.ERROR_COLOR + 
		data.nick + ": nickname collision kill");
		break;
	case '441':
		this.server_chan.post(chat_settings.ERROR_COLOR + 
		data.nick + " is not in channel " + data.chan);
		break;
	case '442':
		this.server_chan.post(chat_settings.ERROR_COLOR + 
		"You are not in channel: " + data.chan);
		break;
	case '443':
		this.server_chan.post(chat_settings.ERROR_COLOR +  
		data.nick + " is already in channel: " + data.chan);
		break;
	case '445':
		this.server_chan.post(chat_settings.ERROR_COLOR + 
		"SUMMON has been disabled");
		break;
	case '446':
		this.server_chan.post(chat_settings.ERROR_COLOR + 
		"USERS has been disabled");
		break;
	case '451':
		this.server_chan.post(chat_settings.ERROR_COLOR + 
		"You have not registered");
		break;
	case '461':
		if(this.channels[data.chan.toUpperCase()]) {
			this.channels[data.chan.toUpperCase()].post(
			data.cmd + ": not enough parameters");
		}
		break;
	case '462':
		this.server_chan.post(chat_settings.ERROR_COLOR + 
		"You cannot re-register");
		break;
	case '463':
		this.server_chan.post(chat_settings.ERROR_COLOR + 
		"Your host is not allowed on this server");
		break;
	case '464':
		this.server_chan.post(chat_settings.ERROR_COLOR + 
		"Incorrect password");
		break;
	case '465':
		this.server_chan.post(chat_settings.ERROR_COLOR + 
		"You have been banned from this server");
		break;
	case '467':
		this.server_chan.post(chat_settings.ERROR_COLOR + 
		"Channel key already set");
		break;
	case '472':
		this.server_chan.post(chat_settings.ERROR_COLOR + 
		"Invalid mode character: " + data.str);
		break;
	case '473':
		this.server_chan.post(chat_settings.ERROR_COLOR + 
		"Can't join " + data.chan + " (invite only)");
		break;
	case '474':
		this.server_chan.post(chat_settings.ERROR_COLOR + 
		"Can't join " + data.chan + " (banned)");
		break;
	case '475':
		this.server_chan.post(chat_settings.ERROR_COLOR + 
		"Can't join " + data.chan + " (invalid key)");
		break;
	case '479':
		this.server_chan.post(chat_settings.ERROR_COLOR + 
		"Illegal channel name: " + data.chan);
		break;
	case '481':
		this.channels[data.chan.toUpperCase()].post(chat_settings.ERROR_COLOR + 
		"Permission Denied: not an IRC operator");
		break;
	case '482':
		this.channels[data.chan.toUpperCase()].post(chat_settings.ERROR_COLOR + 
		"Permission Denied: not a channel operator");
		break;
	default:
		log(LOG_WARNING,"unknown data: " + data.toSource());
		return false;
	}
	return data;
}

/* extension of IRC_process() */
function IRC_processCTCP(data) {
	var text=data.msg || data.str;
	var cmd=text.split(" ");
	
	switch(cmd.shift().substr(1).toUpperCase()) {
	case "ACTION":
		var str=cmd.join(" ");
		var msg="* " + data.origin + " " + str.substr(0,str.length-1);
		
		/* if this is a user message */
		if(data.dest.toUpperCase() == this.nick.nickname.toUpperCase()) {
			if(!this.channels[data.origin.toUpperCase()]) {
				this.channels[data.origin.toUpperCase()]=new IRC_Channel(data.origin);
				this.channels[data.origin.toUpperCase()].joinable=false;
			}
			this.channels[data.origin.toUpperCase()].post(msg);
		}
		/* post message to destination channel */
		else if(this.channels[data.dest.toUpperCase()]) {
			this.channels[data.dest.toUpperCase()].post(msg);
		}
		break;
	case "PING":
		/* generate CTCP ping reply */
		if(data.func == "PRIVMSG") {
			var pingtime=cmd[0].substr(0,cmd[0].length-1);
			var reply=new IRC_Commands["NOTICE"](
				"\x01PING " + pingtime + "\x01",this.nick.nickname,data.origin);
			this.send(reply);
		}
		/* handle CTCP ping reply */
		else if(data.func == "NOTICE") {
			var pingtime=cmd[0].substr(0,cmd[0].length-1);
			msg=format("* Ping reply from %s: %dms", data.origin, Date.now()-pingtime);
			this.server_chan.post(msg);
		}
		break;
	}
	return false;
}

/* stringify an IRC_Command object */
function JSON_encode(cmd) {
	cmd=JSON.stringify(cmd);
	return cmd + "\r\n";
}

/* parse a JSON string */
function JSON_decode(raw_data) {
	try {
		var cmd=JSON.parse(raw_data);
		return cmd;
	} catch(e) {
		/* if we received nonsense */
		log(LOG_ERROR,"error parsing JSON data: " + raw_data);
		return false;
	}
}

/* parse an RFC1459 string */
function RFC1459_decode(str) {
	var origin="";
	var username="";
	var hostname="";
	var cmd = str.split(' ');
	if (cmd[0][0] == ":") {
		if(cmd[0].indexOf("@") >= 0) {
			var nuh=IRC_split_nuh(cmd.shift().substr(1));
			origin=nuh[0];
			username=nuh[1];
			hostname=nuh[2];
		}
		else 
			origin=cmd.shift();
	}

	/* process commands */
	if(isNaN(cmd[0])) {
		switch(cmd[0].toUpperCase()) {
		case "PING":
			cmd[0]="PONG";
			this.queue.write(cmd.join(" ")+"\r\n","data");
			break;
		case "PONG":
			break;
		case "QUIT":
			cmd.shift();
			return({
				func:"QUIT",
				origin:origin,
				str:cmd.join(" ")
			});
			break;
		case "JOIN":
			cmd.shift();
			var chan=cmd.shift();
			if(chan[0] == ":") chan=chan.substr(1);
			return({
				func:"JOIN",
				origin:origin,
				chan:chan
			});
			break;
		case "KICK":
			cmd.shift();
			return({
				func:"KICK",
				chan:cmd.shift(),
				nick:cmd.shift(),
				str:cmd.join(" ")
			});
			break;
		case "PART":
			cmd.shift();
			var chan=cmd.shift();
			if(chan[0] == ":") chan=chan.substr(1);
			return({
				func:"PART",
				origin:origin,
				chan:chan
			});
			break;
		case "NOTICE":
			cmd.shift();
			/* process notices as normal */
			if(this.connection_established) {
				return({
					func:"NOTICE",
					origin:origin,
					dest:cmd.shift(),
					msg:cmd.join(" ").substr(1)
				});
			}
			/* if we received an IRCd welcome message */
			else {
				this.connection_established=true;
				this.server_chan.post(chat_settings.SNOTICE_COLOR + 
					"Established connection");
				var version=str.match(/SynchronetIRCd-(.*)\(.*\)\s/);
				if(version) {
					version=version[1];
					/* 	if the IRCd version is greater than/equal to 2.0, 
						enable JSON mode */
					if(version[0] >= 2) 
						this.encoding="JSON";
				}
				return false;
			}
			break;
		case "PRIVMSG":
			cmd.shift();
			return({
				func:"PRIVMSG",
				origin:origin,
				dest:cmd.shift(),
				msg:cmd.join(" ").substr(1)
			});
			break;
		case "TOPIC":
			cmd.shift();
			return({
				func:"TOPIC",
				origin:origin,
				chan:cmd.shift(),
				topic:cmd.join(" ").substr(1)
			});
			break;
		}
	}
	/* process numerics */
	else {
		var num=cmd.shift();
		cmd.shift(); /* throw away destination user */
		if (cmd[0] == ":")
			cmd[0]=cmd[0].substr(1);
		
		/* create JSON commands for the following numerics */
		switch(num) {
		case '001':
			this.registered=true;
			return({
				func:num,
				nuh:cmd.join(" ")
			});
		case '332':
			return({
				func:num,
				chan:cmd.shift(),
				str:cmd.join(" ").substr(1)
			});
		case '333':
			return({
				func:num,
				chan:cmd.shift(),
				changedby:cmd.shift(),
				topictime:cmd.shift()
			});
		case '366':
			return({
				func:num,
				chan:cmd.shift()
			});
		case '352':
			return({
				func:num,
				disp:cmd.shift(),
				uprefix:cmd.shift(),
				disphost:cmd.shift(),
				servername:cmd.shift(),
				nick:cmd.shift(),
				mode:cmd.shift(),
				hops:cmd.shift().substr(1),
				info:cmd.join(" ")
			});
		case '353':
			return({
				func:num,
				ctype:cmd.shift(),
				chan:cmd.shift(),
				str:cmd.join(" ").substr(1)
			});
		}
		
		/* display these numerics in MOTD colors */
		if(num == 372 || num == 375 || num == 376)
			this.server_chan.post(chat_settings.MOTD_COLOR + cmd.join(" "));

		/* display numerics in this range normally */
		else if(num > 0 && num < 400)
			this.server_chan.post(cmd.join(" "));
			
		/* display numerics over 400 in error colors */
		else if(num > 399)
			this.server_chan.post(chat_settings.ERROR_COLOR + cmd.join(" "));
	}
	
	return false;
}

/* create an RFC1459 string from an IRC_Command object */
function RFC1459_encode(cmd) {
	switch(cmd.func) {
	case 'NICK': /* break up this command to register properly */
		return(format(
			"NICK %s\r\n" +
			"USER %s * * :%s\r\n",
			this.nick.nickname,
			this.nick.username,
			this.nick.realname
		));
		break;
	case 'INVITE':
		return(format(
			"INVITE %s :%s\r\n",
			cmd.nick,
			cmd.chan
		));
	case 'JOIN':
		return(format(
			"JOIN %s %s\r\n",
			cmd.chan,
			cmd.key
		));
	case 'KICK':
		return(format(
			"KICK %s %s :%s\r\n",
			cmd.chan,
			cmd.nick,
			cmd.str
		));
	case 'MODE':
		return(format(
			"MODE %s %s\r\n",
			cmd.dest,
			cmd.mode
		));
	case 'NOTICE':
		return(format(
			"NOTICE %s :%s\r\n",
			cmd.dest,
			cmd.str
		));
	case 'PART':
		var partmsg = "";
		if (cmd.str)
			partmsg = " :" + cmd.str;
		return(format(
			"PART %s%s\r\n",
			cmd.chan,
			partmsg
		));
	case 'PING':
		return(format(
			"PING :%s\r\n",
			cmd.pingtime
		));
	case 'PONG':
		return(format(
			"PONG :%s %s\r\n",
			cmd.origin,
			cmd.pingtime
		));
	case 'PRIVMSG':
		var str = "PRIVMSG %s :%s\r\n";
		return(format(str, cmd.dest, cmd.msg));
	case 'QUIT':
		return(format(
			"QUIT :%s\r\n",
			cmd.str
		));
	case 'TOPIC':
		return(format(
			"TOPIC %s :%s\r\n",
			cmd.chan,
			cmd.topic
		));
	}
	return false;
}

/* IRC protocol command objects */
var IRC_Commands=[];

IRC_Commands["QUIT"]=function(origin,str)
{
	this.func="QUIT";
	this.origin=origin;
	this.str=str;
}
IRC_Commands["AWAY"]=function(str)
{
	this.func="AWAY";
	this.str=str;
}
IRC_Commands["PING"]=function(pingtime)
{
	this.func="PING";
	this.pingtime=pingtime;
}
IRC_Commands["NICK"]=function(nick,username,host)
{
	this.func="NICK";
	this.nickname=nick;
	this.hops=1;
	this.created=time();
	this.username=nick;
	this.realname=username;
	this.hostname=host;
	this.servername=host;
}
IRC_Commands["TOPIC"]=function(str,nick,chan) 
{
	this.func="TOPIC";
	this.chan=chan;
	this.origin=nick;
	this.str=str;
}
IRC_Commands["WHO"]=function(args)
{
	this.func="WHO";
	this.args=args || "+M";
}
IRC_Commands["PRIVMSG"]=function(msg,origin,dest)
{
	this.func="PRIVMSG";
	this.msg=msg;
	this.origin=origin;
	this.dest=dest;
}
IRC_Commands["NOTICE"]=function(msg,origin,dest)
{
	this.func="NOTICE";
	this.msg=msg;
	this.origin=origin;
	this.dest=dest;
}
IRC_Commands["JOIN"]=function(chan,origin,key) 
{
	this.func="JOIN";
	this.chan=chan;
	this.key=key;
	this.origin=origin;
}
IRC_Commands["PART"]=function(chan,origin) 
{
	this.func="PART";
	this.chan=chan;
	this.origin=origin;
}
IRC_Commands["INVITE"] = function(chan,nick) 
{
	this.func="INVITE";
	this.nick=nick;
	this.chan=chan;
}
IRC_Commands["KICK"] = function(chan,nick,str) 
{
	this.func="KICK";
	this.chan=chan;
	this.nick=nick;
	this.str=str;
}
IRC_Commands["LIST"] = function(str) 
{
	this.func="LIST";
	this.str=str;
}
IRC_Commands["NAMES"] = function(chan) 
{
	this.func="NAMES";
	this.chan=chan;
}
IRC_Commands["MODE"] = function(dest,modestr) 
{
	this.func="MODE";
	this.dest=dest;
	this.modestr=modestr;
}

