/**
 *	Internode/InterBBS Full Screen Chat 
 *		for Synchronet 3.15a+
 *		by Matt Johnson - 2011
 *	
 *	for local setup, see
 *		exec/load/json-service.js
 
 *	to use a different chat server, see
 *		ctrl/json-chat.ini
 */

(function() {

	const VERSION = "$Revision: 1.5 $".split(' ')[1];
	load("sbbsdefs.js");
	bbs.command_str='';
	load("str_cmds.js");

	var chat_options = load("modopts.js","jsonchat");

	load("json-client.js");
	var chat_client = new JSONClient(chat_options.host,chat_options.port);
	load("json-chat.js");
	var chat = new JSONChat(user.number,chat_client);

	var channels = [];
	var channels_map = [];
	var channel_index = 0;
	//var today = new Date();
	//var laststamp;

	var chat_settings = {
		ALERT_COLOR:RED,
		NOTICE_COLOR:BROWN,
		CHANNEL_COLOR:DARKGRAY,
		TEXT_COLOR:LIGHTGRAY,
		NICK_COLOR:CYAN,
		INPUT_COLOR:GREEN,
		COMMAND_COLOR:YELLOW,
		TIMESTAMP_COLOR:WHITE
	}

	function initChat() {
		console.attributes = BG_BLACK + LIGHTGRAY;
		console.clear();
		console.putmsg(format("\1y\1hSynchronet Chat (v%s)\r\n",VERSION));
		console.putmsg("\1n\1yType '\1h/help\1n\1y' for a list of commands\r\n");
		chat.join("#main");
	}

	function chatMain() {
		while(1) {
			/* force client to check for updates */
			chat.cycle();
			
			/* check for channel messages and update local channel cache */
			for(var c in chat.channels) {
				var chan = chat.channels[c];
				
				/* verify this channels presence in the local cache */
				verifyLocalCache(chan);
			
				/* display any new messages */
				while(chan.messages.length > 0) 
					printMessage(chan,chan.messages.shift());
			}
			
			/* synchronize local cache with chat client */
			updateLocalCache();
			
			/* receive and process user input */
			getInput();
		}
	}
	
	function verifyLocalCache(chan) {
		if(channels_map[chan.name] == undefined) {
		
			console.attributes = chat_settings.NOTICE_COLOR;
			console.writeln("joining channel: " + chan.name);
			
			channels_map[chan.name] = channels.length;
			channel_index = channels.length;
			channels.push(chan.name);
		}
	}
	
	function updateLocalCache() {
		/* verify local channel cache */
		for(var c in channels_map) {
			if(!chat.channels[c.toUpperCase()]) {
			
				console.attributes = chat_settings.NOTICE_COLOR;
				console.writeln("parting channel: " + c);
				
				channels.splice(channels_map[c],1);
				delete channels_map[c];
				if(!channels[channel_index])
					channel_index = channels.length-1;
			}	
		}
	}
	
	function printMessage(chan,msg) {
		// this is broken.... not sure why
		// var stamp = strftime("%x",msg.time);
		// if(stamp != laststamp) {
			// laststamp = stamp;
			// console.attributes = chat_settings.TIMESTAMP_COLOR;
			// console.write(stamp + " ");
		// }
		
		
		if(!msg.nick) {
			console.attributes = chat_settings.NOTICE_COLOR;
			console.writeln(msg.str);
			return;
		}
		
		console.attributes = chat_settings.CHANNEL_COLOR;
		console.write("[" + chan.name + "] ");
		
		console.attributes = chat_settings.NICK_COLOR;
		console.write(msg.nick.name);
		
		console.attributes = chat_settings.TEXT_COLOR;
		console.writeln(": " + msg.str);
	}

	function getInput() {
		/* get user input */
		var k = console.inkey(K_NONE,25);
		if(k) {
			switch(k) {
			/* quit chat */
			case '\x1b':
			case ctrl('Q'):
				exit();
				break;
			/* do nothing for CRLF on a blank line */
			case '\r':
			case '\n':
				break;
			/* switch to the next channel in our channel list */
			case '\t':
				if(channels.length > 1) {
					channel_index++;
					if(channel_index >= channels.length)
						channel_index = 0;
					console.attributes = chat_settings.NOTICE_COLOR;
					console.writeln("now chatting in: " + channels[channel_index]);
				}
				break;
			/* process a user command */
			case '/':
				console.attributes = chat_settings.COMMAND_COLOR;
				console.write(k);
				chat.getcmd(channels[channel_index],console.getstr(500));
				break;
			/* process a sysop command */
			case ';':
				if(user.compare_ars("SYSOP") || bbs.sys_status&SS_TMPSYSOP) {
					console.attributes = chat_settings.COMMAND_COLOR;
					console.write(k);
					str_cmds(console.getstr(500));
				}
				break;
			/* process all other input */
			default:
				/* send a message to the current channel */
				if(channels.length > 0) {
					console.attributes = chat_settings.INPUT_COLOR;
					console.ungetstr(k);
					chat.submit(channels[channel_index],console.getstr(500));
				}
				/* if we have not joined any channels, we cant send any messages */
				else {
					console.attributes = chat_settings.ALERT_COLOR;
					console.writeln("you must join a channel first");
				}
				break;
			}
		}
	}
	
	initChat();
	chatMain();
	
})();
