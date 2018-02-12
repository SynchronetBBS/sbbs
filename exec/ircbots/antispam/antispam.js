/* SERVER COMMANDS */
Server_Commands["PRIVMSG"] = function (srv,cmd,onick,ouh)	{ 
	if (cmd[0][0] == "#" || cmd[0][0] == "&") {
		var chanName = cmd.shift().toUpperCase();
		var chan=srv.channel[chanName];
		if(!chan) return;
		
		/* allocate channel users */
		var chanUsers = [];
		for each(var u in srv.users) {
			if(u.channels[chanName] == true) {
				chanUsers.push(u.nick.toUpperCase());
			}
		}
		
		/* check message for user nicks */
		var numUsers = chanUsers.length;
		
		/* dont do this for sparsely populated channels */
		if(numUsers < 6) {
			return;
		}
		
		var userMatchCount = 0;
		while(cmd.length > 0) {
			var msgUser = cmd.shift();
			if(msgUser[0] == ':') {
				msgUser = msgUser.substr(1);
			}
			if(chanUsers.indexOf(msgUser.toUpperCase()) > -1) {
				userMatchCount++;
			}
		}
		if(numUsers - userMatchCount <= 2) {
			srv.o(chan.name,"stop spamming, eh?","NOTICE");
			srv.writeout("KICK " + chan.name + " " + onick);
		}
	}
}

