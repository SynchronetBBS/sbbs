/*

 ircd/core.js

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details:
 https://www.gnu.org/licenses/old-licenses/gpl-2.0.txt

 Core IRCd functions.

 Copyright 2003-2021 Randy Sommerfeld <cyan@synchro.net>

*/

function true_array_len(my_array) {
	var i;
	var counter = 0;
	for (i in my_array) {
		counter++;
	}
	return counter;
}

function terminate_everything(str, error) {
	var i;
	var sendstr;

	sendstr = format("ERROR :%s\r\n", str);

	log(error ? LOG_ERR : LOG_NOTICE, "Terminating: " + str);

	for (i in Unregistered) {
		Unregistered[i].socket.send(sendstr);
		Unregistered[i].socket.close();
	}
	for (i in Local_Servers) {
		Local_Servers[i].socket.send(sendstr);
		Local_Servers[i].socket.close();
	}
	for (i in Local_Users) {
		Local_Users[i].socket.send(sendstr);
		Local_Users[i].socket.close();
	}

	js.do_callbacks = false;
	exit(error);
}

function search_server_only(server_name) {
	var i;

	if (!server_name)
		return 0;

	for (i in Servers) {
		if (wildmatch(Servers[i].nick,server_name))
			return Servers[i];
	}
	if (wildmatch(ServerName,server_name))
		return -1; /* the server passed to us is our own. */
	return 0; /* Failure */
}

function searchbyserver(servnick) {
	var i;

	if (!servnick)
		return false;

	i = search_server_only(servnick);
	if (i)
		return i;

	for (i in Users) {
		if (wildmatch(Users[i].nick,servnick))
			return search_server_only(Users[i].servername);
	}

	return false;
}

/* Only allow letters, numbers and underscore in username to a maximum of
   9 characters for 'anonymous' users (i.e. not using PASS to authenticate.)
   hostile characters like !,@,: etc would be bad here :) */
function parse_username(str) {
	str = str.replace(/[^\w]/g,"").toLowerCase();
	if (!str)
		str = "user"; // nothing? we'll give you something boring.
	return str.slice(0,9);
}

function umode_notice(bit,ntype,nmessage) {
	var i, user;

	log(LOG_INFO, format("%s: %s", ntype, nmessage));

	for (i in Local_Users) {
		user = Local_Users[i];
		if (user.mode && ((user.mode&bit)==bit)) {
			user.rawout(format(":%s NOTICE %s :*** %s -- %s",
				ServerName,
				user.nick,
				ntype,
				nmessage
			));
		}
	}

}

function create_ban_mask(str,kline) {
	var tmp_banstr = new Array;
	tmp_banstr[0] = "";
	tmp_banstr[1] = "";
	tmp_banstr[2] = "";
	var i;
	var bchar_counter = 0;
	var part_counter = 0; // BAN: 0!1@2 KLINE: 0@1
	var regexp="[A-Za-z\{\}\`\^\_\|\\]\\[\\\\0-9\-.*?\~:]";
	var finalstr;
	for (i in str) {
		if (str[i].match(regexp)) {
			tmp_banstr[part_counter] += str[i];
			bchar_counter++;
		} else if ((str[i] == "!") && (part_counter == 0) &&
			   !kline) {
			part_counter = 1;
			bchar_counter = 0;
		} else if ((str[i] == "@") && (part_counter == 1) &&
			   !kline) {
			part_counter = 2;
			bchar_counter = 0;
		} else if ((str[i] == "@") && (part_counter == 0)) {
			if (kline) {
				part_counter = 1;
			} else {
				tmp_banstr[1] = tmp_banstr[0];
				tmp_banstr[0] = "*";
				part_counter = 2;
			}
			bchar_counter = 0;
		}
	}
	if (!tmp_banstr[0] && !tmp_banstr[1] && !tmp_banstr[2])
		return 0;
	if (tmp_banstr[0].match(/[.]/) && !tmp_banstr[1] && !tmp_banstr[2]) {
		if (kline)
			tmp_banstr[1] = tmp_banstr[0];
		else
			tmp_banstr[2] = tmp_banstr[0];
		tmp_banstr[0] = "";
	}
	if (!tmp_banstr[0])
		tmp_banstr[0] = "*";
	if (!tmp_banstr[1])
		tmp_banstr[1] = "*";
	if (!tmp_banstr[2] && !kline)
		tmp_banstr[2] = "*";
	if (kline)
		finalstr = tmp_banstr[0].slice(0,10) + "@" + tmp_banstr[1].slice(0,80);
	else
		finalstr = format("%s!%s@%s",
			tmp_banstr[0].slice(0,MAX_NICKLEN),
			tmp_banstr[1].slice(0,10),
			tmp_banstr[2].slice(0,80)
		);
		while (finalstr.match(/[*][*]/)) {
			finalstr=finalstr.replace(/[*][*]/g,"*");
		}
	return finalstr;
}

function isklined(kl_str) {
	var i;

	for (i in KLines) {
		if (KLines[i].hostmask && wildmatch(kl_str,KLines[i].hostmask)) {
			return KLines[i];
		}
	}
	return 0;
}

function IP_Banned(ip) {
	var i;

	for (i in ZLines) {
		if (ZLines[i].ipmask && wildmatch(ip,ZLines[i].ipmask)) {
			return 1;
		}
	}
	return 0;
}

function Scan_For_Banned_Clients() {
	var i, user;

	for (i in Local_Users) {
		user = Local_Users[i];
		if (   isklined(user.uprefix + "@" + user.hostname)
			|| IP_Banned(user.ip)
		) {
			theuser.quit("User has been banned");
		}
	}
}

function remove_kline(kl_hm) {
	var i;

	for (i in KLines) {
		if (KLines[i].hostmask && wildmatch(kl_hm,KLines[i].hostmask)) {
			delete KLines[i];
			return 1;
		}
	}
	return 0;
}

/* this = cline */
function Automatic_Server_Connect() {
	var sock = new Socket();

	sock.array_buffer = false; /* JS78, we want strings */
	sock.cline = this;
	sock.outbound = true;

	if (   Servers[this.servername.toLowerCase()]
		|| YLines[this.ircclass].active >= YLines[this.ircclass].maxlinks
	) {
		this.next_connect = js.setTimeout(
			Automatic_Server_Connect,
			YLines[this.ircclass].connfreq * 1000,
			this
		);
		return false;
	}

	if (Outbound_Connect_in_Progress) {
		this.next_connect = js.setTimeout(
			Automatic_Server_Connect,
			1000,
			this
		);
		return false;
	}

	Outbound_Connect_in_Progress = true;

	umode_notice(USERMODE_ROUTING,"Routing",format(
		"Auto-connecting to %s (%s) on port %u",
		this.servername,
		this.host,
		this.port
	));

	sock.connect(this.host, this.port, handle_outbound_server_connect);

	if (this.next_connect)
		delete this.next_connect;
}

/* socket object with .cline attached */
function handle_outbound_server_connect() {
	var id;

	if (!Outbound_Connect_in_Progress)
		log(LOG_DEBUG,"WARNING: Outbound connection while !Outbound_Connect_in_Progress");

	Outbound_Connect_in_Progress = false;

	if (this.is_connected) {
		umode_notice(USERMODE_ROUTING,"Routing","Connected!  Sending info...");
		this.send(format("PASS %s :TS\r\n", this.cline.password));
		this.send("CAPAB " + SERVER_CAPAB + "\r\n");
		this.send("SERVER " + ServerName + " 1 :" + ServerDesc +"\r\n");
		id = Generate_ID();
		Unregistered[id] = new Unregistered_Client(id,this);
		Unregistered[id].server = true; /* Avoid recvq limitation */
		Unregistered[id].ircclass = this.cline.ircclass;
		Unregistered[id].cline = this.cline;
		this.callback_id = this.on("read", Socket_Recv);
		return true;
	}

	if (YLines[this.cline.ircclass].connfreq > 0 && parseInt(this.cline.port) > 0) {
		umode_notice(USERMODE_ROUTING,"Routing",format(
			"Failed to connect to %s (%s). Trying again in %u seconds.",
			this.cline.servername,
			this.cline.host,
			YLines[this.cline.ircclass].connfreq
		));
		Reset_Autoconnect(this.cline, YLines[this.cline.ircclass].connfreq * 1000);
	} else {
		umode_notice(USERMODE_ROUTING,"Routing",format(
			"Failed to connect to %s (%s).",
			this.cline.servername,
			this.cline.host
		));
	}

	this.close();

	return false;
}

function Write_All_Opers(str) {
	var i;

	for (i in Local_Users) {
		if (Local_Users[i].mode&USERMODE_OPER)
			Local_Users[i].rawout(str);
	}
}

function push_nickbuf(oldnick,newnick) {
	NickHistory.unshift(new NickBuf(oldnick,newnick));
	if(NickHistory.length >= MAX_NICKHISTORY)
		NickHistory.pop();
}

function search_nickbuf(bufnick) {
	var nb;
	for (nb=NickHistory.length-1;nb>-1;nb--) {
		if (bufnick.toUpperCase() == NickHistory[nb].oldnick.toUpperCase()) {
			if (!Users[NickHistory[nb].newnick.toUpperCase()])
				bufnick = NickHistory[nb].newnick;
			else
				return Users[NickHistory[nb].newnick.toUpperCase()];
		}
	}
	return 0;
}

function create_new_socket(port) {
	var newsock;

	log(LOG_DEBUG,"Creating new socket object on port " + port);
	if (js.global.ListeningSocket != undefined) {
		try {
			newsock = new ListeningSocket(server.interface_ip_addr_list, port, "IRCd");
			log(LOG_NOTICE, format("IRC server socket bound to TCP port %d", port));
		}
		catch(e) {
			log(LOG_ERR,"!Error " + e + " creating listening socket on port " + port);
			return 0;
		}
	} else {
		newsock = new Socket();
		if(!newsock.bind(port,server.interface_ip_address)) {
			log(LOG_ERR,"!Error " + newsock.error + " binding socket to TCP port "
				+ port);
			return 0;
		}
		log(LOG_NOTICE, format(
			"%04u IRC server socket bound to TCP port %d",
			newsock.descriptor,
			port
		));
		if(!newsock.listen(5 /* backlog */)) {
			log(LOG_ERR,"!Error " + newsock.error + " setting up socket for listening");
			return 0;
		}
	}
	return newsock;
}

function Check_QWK_Password(qwkid,password) {
	var u;

	if (!password || !qwkid)
		return false;
	qwkid = qwkid.toUpperCase();
	u = new User(system.matchuser(qwkid));
	if (!u)
		return false;
	if (   (password.toUpperCase() == u.security.password.toUpperCase())
		&& (u.security.restrictions&UFLAG_Q)
	) {
		return true;
	}
	return false;
}

function IRCClient_netsplit(ns_reason) {
	var i;

	if (!ns_reason)
		ns_reason = "net.split.net.split net.split.net.split";

	for (i in Users) {
		if (Users[i] && (Users[i].servername == this.nick)) {
			Users[i].quit(ns_reason,true,true);
		}
	}

	for (i in Servers) {
		if (Servers[i] && (Servers[i].linkparent == this.nick)) {
			Servers[i].quit(ns_reason,true,true);
		}
	}
}

function IRCClient_RMChan(rmchan_obj) {
	if (!rmchan_obj)
		return 0;
	if (rmchan_obj.users[this.id])
		delete rmchan_obj.users[this.id];
	if (this.channels[rmchan_obj.nam.toUpperCase()])
		delete this.channels[rmchan_obj.nam.toUpperCase()];
	delete rmchan_obj.modelist[CHANMODE_OP][this.id];
	delete rmchan_obj.modelist[CHANMODE_VOICE][this.id];
	if (!true_array_len(rmchan_obj.users))
		delete Channels[rmchan_obj.nam.toUpperCase()];
}

function Generate_ID() {
	var i;
	for (i = 0; Assigned_IDs[i]; i++) {
		/* do nothing */
	}
	Assigned_IDs[i] = true;
	return i;
}

function rawout(str) {
	var sendconn;
	var str_end;
	var str_beg;

	log(LOG_DEBUG, format("[RAW->%s]: %s",this.nick,str));

	if (this.local) {
		sendconn = this;
	} else if (!this.local) {
		if ((str[0] == ":") && str[0].match(["!"])) {
			str_end = str.slice(str.indexOf(" ")+1);
			str_beg = str.slice(0,str.indexOf("!"));
			str = str_beg + " " + str_end;
		}
		sendconn = Servers[this.parent.toLowerCase()];
	} else {
		log(LOG_ERR,"!ERROR: No connection to send to?");
		return 0;
	}

	if (sendconn.sendq.empty) {
		js.setImmediate(Process_Sendq, sendconn.sendq);
	}
	sendconn.sendq.add(str);
}

function originatorout(str,origin) {
	var send_data;
	var sendconn;

	log(LOG_DEBUG,format("[%s->%s]: %s",origin.nick,this.nick,str));

	sendconn = this;
	if(this.local && !this.server) {
		if (origin.server)
			send_data = ":" + origin.nick + " " + str;
		else
			send_data = ":" + origin.nuh + " " + str;
	} else if (this.server) {
		send_data = ":" + origin.nick + " " + str;
	} else if (!this.local) {
		sendconn = Servers[this.parent.toLowerCase()];
		send_data = ":" + origin.nick + " " + str;
	} else {
		log(LOG_ERR,"!ERROR: No connection to send to?");
		return 0;
	}

	if (sendconn.sendq.empty) {
		js.setImmediate(Process_Sendq, sendconn.sendq);
	}
	sendconn.sendq.add(send_data);
}

function ircout(str) {
	var send_data;
	var sendconn;

	log(LOG_DEBUG,format("[%s->%s]: %s",ServerName,this.nick,str));

	if(this.local) {
		sendconn = this;
	} else if (this.parent) {
		sendconn = Servers[this.parent.toLowerCase()];
	} else {
		log(LOG_ERR,"!ERROR: No socket to send to?");
		return 0;
	}

	send_data = ":" + ServerName + " " + str;

	if (sendconn.sendq.empty) {
		js.setImmediate(Process_Sendq, sendconn.sendq);
	}
	sendconn.sendq.add(send_data);
}

/* this = socket object passed from sock.on() */
function Socket_Recv() {
	if (!this.is_connected) {
		/* We purge the queue and send immediately */
		this.irc.recvq.purge();
		this.irc.sendq.purge();
		this.irc.quit("Connection reset by peer.");
		return 1;
	}
	this.irc.recvq.recv(this);
}

function Queue_Recv(sock) {
	var pos;
	var cmd;
	var str;

	str = sock.recv(65536,0);
	if (str !== null && str.length > 0) {
		this._recv_bytes += str;
		while ((pos = this._recv_bytes.search('\n')) != -1) {
			cmd = this._recv_bytes.substr(0, pos);
			this._recv_bytes = this._recv_bytes.substr(pos+1);
			if (cmd[cmd.length-1] == '\r')
				cmd = cmd.substr(0, cmd.length - 1);
			this.add(cmd);
		}
		if (sock.irc.server) {
			if (sock.irc.recvq.queue.length > 0)
				js.setImmediate(Unthrottled_Recvq, sock.irc.recvq);
			return 1;
		}
		if (sock.irc.recvq.size > MAX_CLIENT_RECVQ) {
			sock.irc.quit(format("Excess Flood (%d bytes)",sock.irc.recvq.size));
			return 1;
		}
		if (sock.irc.recvq.queue.length > 0) {
			js.setImmediate(Process_Recvq, sock.irc.recvq);
		}
	}
	return 0;
}

function Unthrottled_Recvq() {
	if (this.queue.length == 0)
		return 1;
	this.irc.work(this.queue.shift());
	if (this.queue.length > 0) {
		js.setImmediate(Unthrottled_Recvq, this);
	}
	return 0;
}

/* this = IRC_Queue object */
function Recvq_Unthrottle() {
	this.throttled = false;
	this.num_sent = 3;
	this.time_marker = system.timer;
	js.setImmediate(Process_Recvq, this);
}

/* this = IRC_Queue object */
function Process_Recvq() {
	if (this.queue.length == 0)
		return 1;
	var timediff = system.timer - this.time_marker;
	if (timediff >= 2) {
		this.num_sent = 0;
		this.time_marker = system.timer;
	}
	if (this.throttled) {
		if (timediff < 2)
			return 1;
	} else if ((timediff <= 2) && (this.num_sent >= 4)) {
		this.throttled = true;
		this.time_marker = system.timer;
		js.setTimeout(Recvq_Unthrottle, 2000, this);
		return 1;
	}
	this.irc.work(this.queue.shift());
	this.num_sent++;
	if (this.queue.length > 0) {
		js.setImmediate(Process_Recvq, this);
	}
	return 0;
}

/* this = IRC_Queue object */
function Process_Sendq() {
	if (!this.empty) {
		this.send(this.socket);
	}
}

function Queue_Send(sock) {
	var sent;

	if (this.queue.length) {
		this._send_bytes += this.queue.join('\r\n')+'\r\n';
		this.queue = [];
	}
	if (this._send_bytes.length) {
		sent = sock.send(this._send_bytes);
		if (sent > 0) {
			this._send_bytes = this._send_bytes.substr(sent);
			if (this._send_bytes.length > 0) {
				js.setImmediate(Process_Sendq, this);
			}
		}
	}
}

function Queue_Prepend(str) {
	this.queue.unshift(str);
}

function Queue_Add(str) {
	this.queue.push(str);
}

function Queue_Del() {
	return this.queue.shift();
}

function IRCClient_server_notice(str) {
	this.ircout("NOTICE " + this.nick + " :" + str);
}

function IRCClient_numeric(num, str) {
	this.ircout(num + " " + this.nick + " " + str);
}

function IRCClient_numeric200(dest,next) {
	this.numeric(200, "Link " + VERSION + " " + dest + " " + next);
}

function IRCClient_numeric201(ircclass,server) {
	this.numeric(201, "Try. " + ircclass + " " + server);
}

function IRCClient_numeric202(ircclass,server) {
	this.numeric(202, "H.S. " + ircclass + " " + server);
}

function IRCClient_numeric203(ircclass,ip) {
	this.numeric(203, "???? " + ircclass + " [" + ip + "]");
}

function IRCClient_numeric204(nick) {
	this.numeric(204, "Oper " + nick.ircclass + " " + nick.nick);
}

function IRCClient_numeric205(nick) {
	this.numeric(205, "User " + nick.ircclass + " " + nick.nick);
}

function IRCClient_numeric206(ircclass,sint,cint,server) {
	this.numeric(206, "Serv " + ircclass + " " + sint + "S " + cint
		+ "C *!*@" + server);
}

function IRCClient_numeric208(type,clientname) {
	this.numeric(208, type + " 0 " + clientname);
}

function IRCClient_numeric261(file) { /* not currently used */
	this.numeric(261, "File " + file + " debug");
}

function IRCClient_numeric321() {
	this.numeric("321", "Channel :Users  Name");
}

function IRCClient_numeric322(chan,modes) {
	var channel_name, tmp;
	var disp_topic = "";
	var is_onchan = this.channels[chan.nam.toUpperCase()];

	if (modes&LIST_DISPLAY_CHAN_MODES) {
		tmp = chan.chanmode();
		disp_topic += format("[%s] ", tmp.slice(0,tmp.length-1));
	} else if (modes&LIST_DISPLAY_CHAN_TS) {
		disp_topic += format("[%lu] ", chan.created);
	}

	if ((chan.mode&CHANMODE_PRIVATE) && !(this.mode&USERMODE_OPER) && !is_onchan ) {
		channel_name = "*";
	} else {
		channel_name = chan.nam;
		disp_topic += chan.topic;
	}
	if (!(chan.mode&CHANMODE_SECRET) || (this.mode&USERMODE_OPER) || is_onchan ) {
		this.numeric(322, format("%s %d :%s",
			channel_name,
			true_array_len(chan.users),
			disp_topic
		));
	}
}

function IRCClient_numeric331(chan) {
	this.numeric(331, chan.nam + " :No topic is set.");
}

function IRCClient_numeric332(chan) {
	this.numeric(332, chan.nam + " :" + chan.topic);
}

function IRCClient_numeric333(chan) {
	this.numeric(333, chan.nam + " " + chan.topicchangedby + " "
		+ chan.topictime);
}

function IRCClient_numeric351() {
	this.numeric(351, VERSION + " " + ServerName + " :" + VERSION_STR);
}

function IRCClient_numeric352(user,who_options,chan) {
	var who_mode="";
	var gecos;

	if (!user)
		throw "WHO Numeric 352 was called without a user.";

	if (user.away)
		who_mode += "G";
	else
		who_mode += "H";
	if (chan) {
		if (chan.modelist[CHANMODE_OP][user.id])
			who_mode += "@";
		else if (chan.modelist[CHANMODE_VOICE][user.id])
			who_mode += "+";
	}
	if (user.mode&USERMODE_OPER)
		who_mode += "*";

	gecos = user.realname;
	if (who_options&WHO_SHOW_PARENT)
		gecos = user.parent;
	if (who_options&WHO_SHOW_ID)
		gecos = user.id;
	if (who_options&WHO_SHOW_CALLBACKID)
		gecos = user.local ? user.socket.callback_id : "Not a local user.";

	this.numeric(352, format(
		"%s %s %s %s %s %s :%s %s",
		chan ? chan.nam : "*",
		user.uprefix,
		(who_options&WHO_SHOW_IPS_ONLY) ? user.ip : user.hostname,
		user.servername,
		user.nick,
		who_mode,
		user.hops,
		gecos
	));
	return 1;
}

function IRCClient_numeric353(chan, str) {
	/* = public @ secret * everything else */
	this.numeric(353, format(
		"%s %s :%s",
		(chan.mode&CHANMODE_SECRET) ? "@" : "=",
		chan.nam,
		str
	));
}

function IRCClient_numeric382(str) {
	this.numeric(382, "ircd.conf :" + str);
}

function IRCClient_numeric391() {
	this.numeric(391, ServerName + " :"
		+ strftime("%A %B %d %Y -- %H:%M %z",Epoch()));
}

function IRCClient_numeric401(str) {
	this.numeric("401", str + " :No such nick/channel.");
}

function IRCClient_numeric402(str) {
	this.numeric(402, str + " :No such server.");
}

function IRCClient_numeric403(str) {
	this.numeric("403", str
		+ " :No such channel or invalid channel designation.");
}

function IRCClient_numeric411(str) {
	this.numeric("411", ":No recipient given. (" + str + ")");
}

function IRCClient_numeric412() {
	this.numeric(412, " :No text to send.");
}

function IRCClient_numeric440(str) {
	this.numeric(440, str + " :Services is currently down, sorry.");
}

function IRCClient_numeric441(str) {
	this.numeric("441", str + " :They aren't on that channel.");
}

function IRCClient_numeric442(str) {
	this.numeric("442", str + " :You're not on that channel.");
}

function IRCClient_numeric445() {
	this.numeric(445, ":SUMMON has been disabled.");
}

function IRCClient_numeric446() {
	this.numeric(446, ":USERS has been disabled.");
}

function IRCClient_numeric451() {
	this.numeric("451", ":You have not registered.");
}

function IRCClient_numeric461(cmd) {
	this.numeric("461", cmd + " :Not enough parameters.");
}

function IRCClient_numeric462() {
	this.numeric("462", ":You may not re-register.");
}

function IRCClient_numeric481() {
	this.numeric("481", ":Permission Denied.  "
		+ "You do not have the correct IRC operator privileges.");
}

function IRCClient_numeric482(tmp_chan_nam) {
	this.numeric("482", tmp_chan_nam + " :You're not a channel operator.");
}

function IRCClient_lusers() {
	this.numeric(251,format(
		":There are %d users and %d invisible on %d servers.",
		num_noninvis_users(),
		num_invis_users(),
		true_array_len(Servers) + 1 /* count ourselves */
	));
	this.numeric(252, num_opers() + " :IRC operators online.");
	var unknown_connections = true_array_len(Unregistered);
	if (unknown_connections)
		this.numeric(253, unknown_connections + " :unknown connection(s).");
	this.numeric(254, true_array_len(Channels) + " :channels formed.");
	this.numeric(255, ":I have " + true_array_len(Local_Users)
		+ " clients and " + true_array_len(Local_Servers) + " servers.");
	this.numeric(250, ":Highest connection count: " + HCC_Total + " ("
		+ HCC_Users + " clients.)");
	this.server_notice(format(
		"%d clients have connected since %s",
		HCC_Counter,
		SERVER_UPTIME_STRF
	));
}

function num_noninvis_users() {
	var i;
	var counter = 0;

	for (i in Users) {
		if (!(Users[i].mode&USERMODE_INVISIBLE))
			counter++;
	}
	return counter;
}

function num_invis_users() {
	var i;
	var counter = 0;

	for (i in Users) {
		if (Users[i].mode&USERMODE_INVISIBLE)
			counter++;
	}
	return counter;
}

function num_opers() {
	var i;
	var counter = 0;

	for (i in Users) {
		if (Users[i].mode&USERMODE_OPER)
			counter++;
	}
	return counter;
}

function IRCClient_motd() {
	var motd_line;
	var motd_file = new File(system.text_dir + "ircmotd.txt");
	if (motd_file.exists==false) {
		this.numeric(422, ":MOTD file missing.");
		umode_notice(USERMODE_OPER,"Notice",format(
			"MOTD file missing: %s",motd_file.name
		));
	} else if (motd_file.open("r")==false) {
		this.numeric(424, ":Error opening MOTD file.");
		umode_notice(USERMODE_OPER,"Notice",format(
			"MOTD error %d opening: %s", errno, motd_file.name
		));
	} else {
		motd_file.codepage = 0;
		this.numeric(375, ":- " + ServerName + " Message of the Day -");
		this.numeric(372, ":- " + strftime("%m/%d/%Y %H:%M",motd_file.date));
		while (!motd_file.eof) {
			motd_line = motd_file.readln();
			if (motd_line != null) {
				if (typeof codepage_encode == 'function')
					motd_line = codepage_encode(false, 0, motd_line);
				this.numeric(372, ":- " + motd_line);
			}
		}
		motd_file.close();
	}
	this.numeric(376, ":End of /MOTD command.");
}

function IRCClient_names(chan) {
	var Channel_user;
	var numnicks=0;
	var tmp="";
	var i;
	for (i in chan.users) {
		Channel_user=chan.users[i];
		if (!(Channel_user.mode&USERMODE_INVISIBLE) ||
		     (this.channels[chan.nam.toUpperCase()]) ) {
			if (numnicks)
				tmp += " ";
			if (chan.modelist[CHANMODE_OP][Channel_user.id])
				tmp += "@";
			else if (chan.modelist[CHANMODE_VOICE][Channel_user.id])
				tmp += "+";
			tmp += Channel_user.nick;
			numnicks++;
			if (numnicks >= 59) {
				// dump what we've got, it's too much.
				this.numeric353(chan, tmp);
				numnicks=0;
				tmp="";
			}
		}
	}
	if(numnicks)
		this.numeric353(chan, tmp);
}

/* Traverse each channel the user is on and see if target is on any of the
   same channels. */
function IRCClient_onchanwith(target) {
	var c, i;

	for (c in this.channels) {
		for (i in target.channels) {
			if (c == i)
				return 1; // success
		}
	}
	return 0; // failure.
}

function IRCClient_bcast_to_uchans_unique(str) {
	var i, j;
	var already_bcast = {};

	for (i in this.channels) {
		for (j in this.channels[i].users) {
			if (   !already_bcast[this.channels[i].users[j].nick]
				&& (this.channels[i].users[j].id != this.id)
				&& this.channels[i].users[j].local
			) {
				this.channels[i].users[j].originatorout(str,this);
				already_bcast[this.channels[i].users[j].nick] = true;
			}
		}
	}
}

function IRCClient_bcast_to_list(chan, str, bounce, list_bit) {
	var i, user;

	for (i in chan.users) {
		var user = chan.users[i];
		if (   user
			&& (user.id != this.id || (bounce))
			&& chan.modelist[list_bit][user.id]
		) {
			user.originatorout(str,this);
		}
	}
}

function IRCClient_bcast_to_channel(chan, str, bounce) {
	var i, user;

	for (i in chan.users) {
		var user = chan.users[i];
		if ((user.id != this.id || bounce) && user.local) {
			user.originatorout(str,this);
		}
	}
}

function IRCClient_bcast_to_channel_servers(chan, str) {
	var i, user;

	var sent_to_servers = new Object;
	for (i in chan.users) {
		user = chan.users[i];
		if (   !user.local
			&& (this.parent != user.parent)
			&& !sent_to_servers[user.parent.toLowerCase()]
		) {
			user.originatorout(str,this);
			sent_to_servers[user.parent.toLowerCase()] = true;
		}
	}
}

function IRCClient_check_nickname(newnick,squelch) {
	var qline_nick;
	var checknick;
	var regexp;
	var i;

	newnick = newnick.slice(0,MAX_NICKLEN);
	/* If you're trying to NICK to yourself, drop silently. */
	if(newnick == this.nick)
		return -1;
	/* First, check for valid nick against irclib. */
	if(IRC_check_nick(newnick)) {
		if (!squelch)
			this.numeric("432", newnick + " :Foobar'd Nickname.");
		return 0;
	}
	/* Second, check for existing nickname. */
	checknick = Users[newnick.toUpperCase()];
	if(checknick && (checknick.nick != this.nick) ) {
		if (!squelch)
			this.numeric("433", newnick + " :Nickname is already in use.");
		return 0;
	}
	/* Third, match against Q:Lines */
	for (i in QLines) {
		if(wildmatch(newnick, QLines[i].nick)) {
			if (!squelch)
				this.numeric(432, newnick + " :" + QLines[i].reason);
			return 0;
		}
	}
	return 1; /* Success */
}

function IRCClient_do_whois(wi) {
	var i;
	var userchans = "";
	var chan;

	if (!wi)
		throw "do_whois() called without an argument.";
	if (!wi.nick)
		throw "do_whois() called without a valid nick.";

	this.numeric(311, format(
		"%s %s %s * :%s",
		wi.nick, wi.uprefix, wi.hostname, wi.realname
	));

	for (i in wi.channels) {
		var chan = wi.channels[i];
		if (   this.channels[chan.nam.toUpperCase()]
			|| !(chan.mode&CHANMODE_SECRET || chan.mode&CHANMODE_PRIVATE)
			|| this.mode&USERMODE_OPER
		) {
			if (userchans)
				userchans += " ";
			if (chan.modelist[CHANMODE_OP][wi.id])
				userchans += "@";
			else if (chan.modelist[CHANMODE_VOICE][wi.id])
				userchans += "+";
			userchans += chan.nam;
		}
	}
	if (userchans) {
		this.numeric(319, format(
			"%s :%s",
			wi.nick, userchans
		));
	}
	if (wi.local) {
		this.numeric(312, format(
			"%s %s :%s",
			wi.nick, ServerName, ServerDesc
		));
	} else {
		i = searchbyserver(wi.servername);
		this.numeric(312, format(
			"%s %s :%s",
			wi.nick, i.nick, i.info
		));
	}
	if (wi.uline) {
		this.numeric(313, format(
			"%s :is a Network Service.",
			wi.nick
		));
	} else if (wi.mode&USERMODE_OPER) {
		this.numeric(313, format(
			"%s :is an IRC Operator.",
			wi.nick
		));
	}
	if (wi.away) {
		this.numeric(301, format(
			"%s :%s",
			wi.nick, wi.away
		));
	}
	if (wi.local) {
		if (wi.socket.ssl_server) {
			this.numeric(671, format(
				"%s :is using a secure connection.",
				wi.nick
			));
		}
		this.numeric(317, format(
			"%s %lu %lu :seconds idle, signon time",
			wi.nick,
			Epoch() - wi.talkidle,
			wi.connecttime
		));
	}
}

function IRCClient_services_msg(svcnick,send_str) {
	var service_server;

	if (!send_str) {
		this.numeric412();
		return 0;
	}
	var usr = Users[svcnick.toUpperCase()];
	if (!usr) {
		this.numeric440(svcnick);
		return 0;
	}
	service_server = searchbyserver(usr.servername);
	if (!service_server || !service_server.uline) {
		this.numeric440(svcnick);
		return 0;
	}
	this.do_msg(svcnick,"PRIVMSG",send_str);
}

function IRCClient_global(target,type_str,send_str) {
	var i;

	if (!target.match("[.]")) {
		this.numeric(413,target + " :No top-level domain specified.");
		return 0;
	}
	var domain_part = target.split('.');
	if (domain_part[domain_part.length - 1].match("[?*]")) {
		this.numeric(414,target + " :Wildcard found in top-level domain.");
		return 0;
	}
	var global_mask = target.slice(1);
	var global_str = type_str + " " + target + " :" + send_str;
	for (i in Local_Users) {
		if (target[0] == "#")
			var global_match = Local_Users[i].hostname;
		else /* assume $ */
			var global_match = Local_Users[i].servername;
		if (wildmatch(global_match,global_mask))
			Local_Users[i].originatorout(global_str,this);
	}
	global_str = ":" + this.nick + " " + global_str;
	if(this.local && this.parent) /* Incoming from a local server */
		Servers[this.parent.toLowerCase()].bcast_to_servers_raw(global_str);
	else if (this.flags&OLINE_CAN_GGNOTICE) /* From a local oper */
		server_bcast_to_servers(global_str);
	return 1;
}

function IRCClient_globops(str) {
	var globops_bits = 0;
	globops_bits |= USERMODE_OPER;
	globops_bits |= USERMODE_GLOBOPS;
	umode_notice(globops_bits,"Global",format(
		"from %s: %s",
		this.nick,
		str
	));
	if (this.parent) {
		Servers[this.parent.toLowerCase()].bcast_to_servers_raw(format(
			":%s GLOBOPS :%s",
			this.nick,
			str
		));
	} else {
		server_bcast_to_servers(format(
			":%s GLOBOPS :%s",
			this.nick,
			str
		));
	}
}

function IRCClient_do_msg(target,type_str,send_str) {
	if (   (target[0] == "$")
		&& (this.mode&USERMODE_OPER)
		&& ((this.flags&OLINE_CAN_LGNOTICE) || !this.local)
	) {
		return this.global(target,type_str,send_str);
	}

	var send_to_list = -1;
	if (target[0] == "@" && ( (target[1] == "#") || target[1] == "&") ) {
		send_to_list = CHANMODE_OP;
		target = target.slice(1);
	} else if (target[0]=="+" && ((target[1] == "#")|| target[1] == "&")) {
		send_to_list = CHANMODE_VOICE;
		target = target.slice(1);
	}

	if ((target[0] == "#") || (target[0] == "&")) {
		var chan = Channels[target.toUpperCase()];
		if (!chan) {
			/* check to see if it's a #*hostmask* oper message */
			if ( (target[0] == "#") && (this.mode&USERMODE_OPER) &&
			     ( (this.flags&OLINE_CAN_LGNOTICE) || !this.local )
			   ) {
				return this.global(target,type_str,send_str);
			} else {
				this.numeric401(target);
				return 0;
			}
		}
		if ((chan.mode&CHANMODE_NOOUTSIDE)
			&& !this.channels[chan.nam.toUpperCase()]) {
			this.numeric(404, chan.nam + " :Cannot send to channel "
				+ "(+n: no outside messages)");
			return 0;
		}
		if (   (chan.mode&CHANMODE_MODERATED)
			&& !chan.modelist[CHANMODE_VOICE][this.id]
			&& !chan.modelist[CHANMODE_OP][this.id]
		) {
			this.numeric(404,
				format("%s :Can't send to channel (+m: moderated)"),
				chan.nam
			);
			return 0;
		}
		if (   chan.isbanned(this.nuh)
			&& !chan.modelist[CHANMODE_VOICE][this.id]
			&& !chan.modelist[CHANMODE_OP][this.id]
		) {
			this.numeric(404,
				format("%s :Can't send to channel (+b: you're banned!)"),
				chan.nam
			);
			return 0;
		}
		if(send_to_list == -1) {
			var str = type_str +" "+ chan.nam +" :"+ send_str;
			this.bcast_to_channel(chan, str, false);
			this.bcast_to_channel_servers(chan, str);
		} else {
			var prefix_chr;
			if (send_to_list == CHANMODE_OP)
				prefix_chr="@";
			else if (send_to_list == CHANMODE_VOICE)
				prefix_chr="+";
			var str = type_str +" " + prefix_chr + chan.nam + " :"+ send_str;
			this.bcast_to_list(chan, str, false, send_to_list);
			this.bcast_to_channel_servers(chan, str);
		}
	} else {
		if (target.match("[@]")) {
			var msg_arg = target.split('@');
			var real_target = msg_arg[0];
			var target_server = searchbyserver(msg_arg[1]);
			if (!target_server) {
				this.numeric401(target);
				return 0;
			}
			if (target_server == -1) {
				this.numeric(407, target
					+ " :Duplicate recipients, no message delivered.");
				return 0;
			}
			target = msg_arg[0] + "@" + msg_arg[1];
		} else {
			var real_target = target;
		}
		var target_socket = Users[real_target.toUpperCase()];
		if (target_socket) {
			if (target_server && (target_server.parent != target_socket.parent)) {
				this.numeric401(target);
				return 0;
			}
			if (target_server && (target_server.id == target_socket.parent) ) {
				target = real_target;
			}
			if (target_socket.issilenced(this.nuh))
				return 0;	/* On SILENCE list.  Silently ignore. */
			var str = type_str + " " + target + " :" + send_str;
			target_socket.originatorout(str,this);
			if (   target_socket.away
				&& (type_str == "PRIVMSG")
				&& !this.server
				&& target_socket.local
			) {
				this.numeric(301, format("%s :%s",
					target_socket.nick,
					target_socket.away
				));
			}
		} else {
			this.numeric401(target);
			return 0;
		}
	}
	return 1;
}

function IRCClient_do_admin() {
	umode_notice(USERMODE_STATS_LINKS,"StatsLinks",format(
		"ADMIN requested by %s (%s@%s) [%s]",
		this.nick,
		this.uprefix,
		this.hostname,
		this.servername
	));
	if (Admin1 && Admin2 && Admin3) {
		this.numeric(256, ":Administrative info about " + ServerName);
		this.numeric(257, ":" + Admin1);
		this.numeric(258, ":" + Admin2);
		this.numeric(259, ":" + Admin3);
	} else {
		this.numeric(423, ServerName
			+ " :No administrative information available.");
	}
}

function IRCClient_do_info() {
	umode_notice(USERMODE_STATS_LINKS,"StatsLinks",format(
		"INFO requested by %s (%s@%s) [%s]",
		this.nick,
		this.uprefix,
		this.hostname,
		this.servername
	));
	this.numeric(371, ":--=-=-=-=-=-=-=-=-=-=*[ The Synchronet IRCd v1.9b ]*=-=-=-=-=-=-=-=-=-=--");
	this.numeric(371, ":   IRCd Copyright 2003-2021 by Randy Sommerfeld <cyan@synchro.net>");
	this.numeric(371, ":" + system.version_notice + " " + system.copyright + ".");
	this.numeric(371, ":--=-=-=-=-=-=-=-=-=-( A big thanks to the following )-=-=-=-=-=-=-=-=-=--");
	this.numeric(371, ":DigitalMan (Rob Swindell): Resident coder god, various hacking all");
	this.numeric(371, ":   around the IRCd, countless helpful suggestions and tips, and");
	this.numeric(371, ":   tons of additions to the Synchronet JS API that made this possible.");
	this.numeric(371, ":Deuce (Stephen Hurd): Resident Perl guru and ex-Saskatchewan zealot.");
	this.numeric(371, ":   Originally converted the IRCd to be object-oriented, various small");
	this.numeric(371, ":   hacks, and lots of guidance.");
	this.numeric(371, ":Thanks to the DALnet Bahamut team for their help over the years.");
	this.numeric(371, ":Greets to: Arrak, ElvishMerchant, Foobar, Grimp, Kufat, Psyko,");
	this.numeric(371, ":   Samael, TheStoryTillNow, Torke, and all the #square oldbies.");
	this.numeric(371, ":In memory of Palidor (Alex Kimbel) March 17, 1984 - December 12, 2012.");
	this.numeric(371, ":--=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--");
	this.numeric(371, ":Synchronet " + system.full_version);
	this.numeric(371, ":Compiled with " + system.compiled_with + " at " + system.compiled_when);
	this.numeric(371, ":Running on " + system.os_version);
	this.numeric(371, ":Utilizing socket library: " + system.socket_lib);
	this.numeric(371, ":Javascript library: " + system.js_version);
	this.numeric(371, ":This BBS has been up since " + system.timestr(system.uptime));
	this.numeric(371, ":-  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -");
	if (server.version_detail !== undefined) {
		this.numeric(371, ":This IRCd was executed via:");
		this.numeric(371, ":" + server.version_detail);
	}
	this.numeric(371, ":IRClib Version: " + IRCLIB_VERSION);
	this.numeric(371, ":--=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--");
	this.numeric(371, ":This program is distributed under the terms of the GNU General Public");
	this.numeric(371, ":License, version 2. https://www.gnu.org/licenses/old-licenses/gpl-2.0.txt");
	this.numeric(371, ":--=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--");
	this.numeric(374, ":End of /INFO list.");
}

function IRCClient_do_stats(statschar) {
	var i;

	switch(statschar[0]) {
		case "C":
		case "c":
			for (i in CLines) {
				this.numeric(213,format(
					"C %s * %s %s %s",
					CLines[i].host,
					CLines[i].servername,
					CLines[i].port ? CLines[i].port : "*",
					CLines[i].ircclass
				));
				if (NLines[i]) {
					this.numeric(214,format(
						"N %s * %s %s %s",
						NLines[i].host,
						NLines[i].servername,
						NLines[i].flags,
						NLines[i].ircclass
					));
				}
			}
			break;  
		case "H":
		case "h":
			for (i in HLines) {
				this.numeric(244,format(
					"H %s * %s",
					HLines[i].allowedmask,
					HLines[i].servername
				));
			}
			break;
		case "I":
		case "i":
			for (i in ILines) {
				this.numeric(215,format(
					"I %s * %s %s %s",
					ILines[i].ipmask,
					ILines[i].hostmask,
					ILines[i].port ? ILines[i].port : "*",
					ILines[i].ircclass
				));
			}
			break;
		case "K":
		case "k":
			for (i in KLines) {
				if (KLines[i].hostmask) {
					this.numeric(216, format(
						"%s %s * * :%s",
						KLines[i].type,
						KLines[i].hostmask,
						KLines[i].reason
					));
				}
			}
			break;
		case "L": /* FIXME */
			this.numeric(241,"L <hostmask> * <servername> <maxdepth>");
			break;
		case "l": /* FIXME */
			this.numeric(211,"<linkname> <sendq> <sentmessages> <sentbytes> <receivedmessages> <receivedbytes> <timeopen>");
			break;
		case "M":
		case "m":
			for (i in Profile) {
				this.numeric(212, format(
					"%s %s %s",
					i,
					Profile[i].ticks,
					Profile[i].executions
				));
			}
			break;
		case "O":
		case "o":
			for (i in OLines) {
				this.numeric(243, format(
					"O %s * %s",
					OLines[i].hostmask,
					OLines[i].nick
				));
			}
			break;
		case "U":
			for (i in ULines) {
				this.numeric(246, format(
					"U %s * * 0 -1",
					ULines[i]
				));
			}
			break;
		case "u":
			this.numeric(242,format(":%s", Uptime_String()));
			break;
		case "Y":
		case "y":
			for (i in YLines) {
				this.numeric(218,format(
					"Y %s %s %s %s %s",
					i,
					YLines[i].pingfreq,
					YLines[i].connfreq,
					YLines[i].maxlinks,
					YLines[i].sendq
				));
			}
			break;
		default:
			break;
	}
	this.numeric(219, statschar[0] + " :End of /STATS Command.");
}

function IRCClient_do_users() {
	var i, u;
	var usersshown = false;

	this.numeric(392,':UserID                    Terminal  Host');
	usersshown=0;
	for (i in system.node_list) {
		if(system.node_list[i].status == NODE_INUSE) {
			u=new User(system.node_list[i].useron);
			this.numeric(393,format(':%-25s %-9s %-30s',u.alias,'Node'+node,
				u.host_name));
			usersshown++;
		}
	}
	if(usersshown) {
		this.numeric(394,':End of users');
	} else {
		this.numeric(395,':Nobody logged in');
	}
}

function IRCClient_do_summon(summon_user) {
	var usernum, isonline, i;

	usernum = system.matchuser(summon_user);
	if(!usernum) {
		this.numeric(444,":No such user.");
	} else {
		isonline = 0;
		for (i in system.node_list) {
			if( (system.node_list[i].status == NODE_INUSE)
				&& (system.node_list[i].useron == usernum) ) {
				isonline = true;
				break;
			}
		}
		if(!isonline) {
			this.numeric(444,":User not logged in.");
		} else {
			system.put_telegram(usernum, "" + this.nick
				+ " is summoning you to IRC chat.\r\n");
			this.numeric(342, summon_user + " :Summoning user to IRC");
		}
	}
}

function IRCClient_do_links(mask) {
	var i;

	if (!mask)
		mask = "*";
	umode_notice(USERMODE_STATS_LINKS,"StatsLinks",format(
		"LINKS %s requested by %s (%s@%s) [%s]",
		mask,
		this.nick,
		this.uprefix,
		this.hostname,
		this.servername
	));
	for (i in Servers) {
		if (wildmatch(Servers[i].nick,mask)) {
			this.numeric(364, format(
				"%s %s :%s %s",
				Servers[i].nick,
				Servers[i].linkparent,
				Servers[i].hops,
				Servers[i].info
			));
		}
	}
	if (wildmatch(ServerName,mask))
		this.numeric(364, ServerName + " " + ServerName + " :0 " + ServerDesc);
	this.numeric(365, mask + " :End of /LINKS list.");
}

/* Don't hunt for servers based on nicknames, as TRACE is more explicit. */
function IRCClient_do_trace(target) {
	var server = searchbyserver(target);

	if (server == -1) { /* we hunted ourselves */
		this.trace_all_opers();
	} else if (server) {
		server.rawout(":" + this.nick + " TRACE " + target);
		this.numeric200(target,server.nick);
		return 0;
	} else {
		/* Okay, we've probably got a user. */
		var nick = Users[target.toUpperCase()];
		if (!nick) {
			this.numeric402(target);
			return 0;
		} else if (nick.local) {
			if (nick.mode&USERMODE_OPER)
				this.numeric204(nick);
			else
				this.numeric205(nick);
		} else {
			nick.rawout(":" + this.nick + " TRACE " + target);
			this.numeric200(target,Servers[nick.parent.toLowerCase()].nick);
		}
	}
	this.numeric(262, target + " :End of /TRACE.");
}

function IRCClient_trace_all_opers() {
	var i;
	for (i in Local_Users) {
		if (Local_Users[i].mode&USERMODE_OPER)
			this.numeric204(Local_Users[i]);
	}
}

function Find_CLine_by_Server(str) {
	var i;
	for (i in CLines) {
		if (wildmatch(CLines[i].servername,str) || wildmatch(CLines[i].host,str))
			return CLines[i];
	}
	return false;
}

function IRCClient_do_connect(con_server,con_port) {
	var i;
	var con_cline;
	var msg;
	var con_type;
	var sock;

	con_cline = Find_CLine_by_Server(con_server);

	if (!con_cline) {
		this.numeric402(con_server);
		return false;
	}

	if (!con_port && con_cline.port)
		con_port = con_cline.port;
	if (!con_port && !con_cline.port)
		con_port = String(Default_Port);
	if (!con_port.match(/^[0-9]+$/)) {
		this.server_notice("Invalid port: " + con_port);
		return false;
	}

	if (Outbound_Connect_in_Progress) {
		this.server_notice("Already busy with an outbound connection. CONNECT ignored.");
		return false;
	}

	msg = format(" CONNECT %s %u from %s [%s@%s]",
		con_cline.servername,
		con_port,
		this.nick,
		this.uprefix,
		this.hostname
	);
	con_type = "Local";
	if (this.parent) {
		con_type = "Remote";
		server_bcast_to_servers("GNOTICE :Remote" + msg);
	}
	umode_notice(USERMODE_ROUTING,"Routing",format("from %s: %s%s",
		ServerName,
		con_type,
		msg
	));

	if (con_cline.next_connect)
		js.clearTimeout(con_cline.next_connect);

	sock = new Socket();
	
	sock.array_buffer = false; /* JS78, we want strings */
	sock.cline = con_cline;
	sock.outbound = true;

	sock.connect(con_cline.host, con_port, handle_outbound_server_connect);

	return true;
}

function IRCClient_do_basic_who(whomask) {
	var i;
	var eow = "*";

	var regexp = "^[0]{1,}$";
	if (whomask.match(regexp))
		whomask = "*";

	if ((whomask[0] == "#") || (whomask[0] == "&")) {
		var chan = Channels[whomask.toUpperCase()];
		if (   chan
			&& (
				   (!(chan.mode&CHANMODE_SECRET) && !(chan.mode&CHANMODE_PRIVATE))
				|| this.channels[chan.nam.toUpperCase()]
				|| (this.mode&USERMODE_OPER)
			)
		) {
			for (i in chan.users) {
				var usr = chan.users[i];
				if (   !(usr.mode&USERMODE_INVISIBLE)
					|| (this.mode&USERMODE_OPER)
					|| this.onchanwith(usr)
				) {
					var chkwho = this.numeric352(usr,0 /* who_options */,chan);
					if (!chkwho) {
						umode_notice(USERMODE_OPER,"Notice",format(
							"WHO returned 0 for user: %s (A)",
							usr.nick
						));
					}
				}
			}
			eow = chan.nam;
		}
	} else {
		for (i in Users) {
			var usr = Users[i];
			if (   usr.match_who_mask(whomask)
				&& (
					   !(usr.mode&USERMODE_INVISIBLE)
					|| (this.mode&USERMODE_OPER)
					|| this.onchanwith(usr)
				)
			) {
				var chkwho = this.numeric352(usr);
				if (!chkwho) {
					umode_notice(USERMODE_OPER,"Notice",format(
						"WHO returned 0 for user: %s (B)",
						usr.nick
					));
				}
			}
		}
		eow = whomask;
	}
	this.numeric(315, eow + " :End of /WHO list. (Basic)");
}

function IRCClient_do_complex_who(cmd) {
	var tmp, chan, i, x;
	var who = new Who();
	var eow = "*";
	var add = true;	/* We assume + so things like 'MODE #chan o cyan' work */
	var arg = 1;
	var whomask = "";

	/* RFC1459 Compatibility.  "WHO <mask> o"  Only do it if we find a
	   wildcard, otherwise assume we're doing a complex WHO with 'modes' */
	if (  cmd[2]
		&& (
			   cmd[1].match(/[*]/)
			|| cmd[1].match(/[?]/)
		)
		|| cmd[1].match(/[0]/)
		&& !cmd[3]
		&& (cmd[2].toLowerCase() == "o")
	) {
		tmp = cmd[1];
		cmd[1] = cmd[2];
		cmd[2] = tmp;
	}

	for (i in cmd[1]) {
		switch (cmd[1][i]) {
			case "+":
				if (!add)
					add = true;
				break;
			case "-":
				if (add)
					add = false;
				break;
			case "a":
				who.tweak_mode(WHO_AWAY,add);
				break;
			case "c":
				arg++;
				if (cmd[arg]) {
					who.tweak_mode(WHO_CHANNEL,add);
					who.Channel = cmd[arg];
				}
				break;
			case "g":
				arg++;
				if (cmd[arg]) {
					who.tweak_mode(WHO_REALNAME,add);
					who.RealName = cmd[arg];
				}
				break;
			case "h":
				arg++;
				if (cmd[arg]) {
					who.tweak_mode(WHO_HOST,add);
					who.Host = cmd[arg];
				}
				break;
			case "i":
				arg++;
				if (cmd[arg]) {
					who.tweak_mode(WHO_IP,add);
					who.IP = cmd[arg];
				}
				break;
			case "l":
				arg++;
				if (cmd[arg]) {
					who.tweak_mode(WHO_CLASS,add);
					who.Class = parseInt(cmd[arg]);
				}
			case "m": /* we never set -m */
				arg++;
				if (cmd[arg]) {
					who.tweak_mode(WHO_UMODE,true);
					if (!add) {
						var tmp_umode = "";
						if (   (cmd[arg][0] != "+")
							|| (cmd[arg][0] != "-")
						) {
							tmp_umode += "+";
						}
						tmp_umode += cmd[arg].replace(/[-]/g," ");
						tmp_umode = tmp_umode.replace(/[+]/g,"-");
						who.UMode = tmp_umode.replace(/[ ]/g,"+");
					} else {
						who.UMode = cmd[arg];
					}
				}
				break;
			case "n":
				arg++;
				if (cmd[arg]) {
					who.Nick = cmd[arg];
					who.tweak_mode(WHO_NICK,add);
				}
				break;
			case "o":
				who.tweak_mode(WHO_OPER,add);
				break;
			case "s":
				arg++;
				if (cmd[arg]) {
					who.Server = cmd[arg];
					who.tweak_mode(WHO_SERVER,add);
				}
				break;
			case "t":
				arg++;
				if (cmd[arg]) {
					who.Time = parseInt(cmd[arg]);
					who.tweak_mode(WHO_TIME,add);
				}
				break;
			case "u":
				arg++;
				if (cmd[arg]) {
					who.User = cmd[arg];
					who.tweak_mode(WHO_USER,add);
				}
				break;
			case "C":
				who.tweak_mode(WHO_FIRST_CHANNEL,add);
				break;
			case "M":
				who.tweak_mode(WHO_MEMBER_CHANNEL,add);
				break;
			case "I":
				who.tweak_mode(WHO_SHOW_IPS_ONLY,add);
				break;
			case "X":
				if (this.mode&USERMODE_OPER)
					who.tweak_mode(WHO_SHOW_PARENT,add);
				break;
			case "Y":
				if (this.mode&USERMODE_OPER)
					who.tweak_mode(WHO_SHOW_ID,add);
				break;
			case "Z":
				if (this.mode&USERMODE_OPER)
					who.tweak_mode(WHO_SHOW_CALLBACKID,add);
				break;
			default:
				break;
		}
	}

	/* Check to see if the user passed a generic mask to us for processing. */
	arg++;
	if (cmd[arg])
		whomask = cmd[arg];

	var regexp = "^[0]{1,}$";
	if (whomask.match(regexp))
		whomask = "*";

	/* allow +c/-c to override. */
	if (!who.Channel && ((whomask[0] == "#") || (whomask[0] == "&")))
		who.Channel = whomask;

	/* Strip off any @ or + in front of a channel and set the flags. */
	var sf_op = false;
	var sf_voice = false;
	var sf_done = false;
	var tmp_wc = who.Channel;
	for (i in tmp_wc) {
		switch(tmp_wc[i]) {
			case "@":
				sf_op = true;
				who.Channel = who.Channel.slice(1);
				break;
			case "+":
				sf_voice = true;
				who.Channel = who.Channel.slice(1);
				break;
			default: // assume we're done
				sf_done = true;
				break;
		}
		if (sf_done)
			break;
	}

	/* Now we traverse everything and apply the criteria the user passed. */
	var who_count = 0;
	for (i in Users) {
		var wc = Users[i];
		var flag_M = this.onchanwith(wc);

		if (   (wc.mode&USERMODE_INVISIBLE)
			&& !(this.mode&USERMODE_OPER)
			&& !flag_M
		) {
			continue;
		}

		if ((who.add_flags&WHO_AWAY) && !wc.away)
			continue;
		else if ((who.del_flags&WHO_AWAY) && wc.away)
			continue;
		if (who.add_flags&WHO_CHANNEL) {
			if (!wc.channels[who.Channel.toUpperCase()])
				continue;
			if (   sf_op
				&& Channels[who.Channel.toUpperCase()]
				&& !Channels[who.Channel.toUpperCase()].modelist[CHANMODE_OP][wc.id]
			) {
				continue;
			}
			if (   sf_voice
				&& Channels[who.Channel.toUpperCase()]
				&& !Channels[who.Channel.toUpperCase()].modelist[CHANMODE_VOICE][wc.id]
			) {
				continue;
			}
		} else if (who.del_flags&WHO_CHANNEL) {
			if (wc.channels[who.Channel.toUpperCase()])
				continue;
			if (   sf_op
				&& Channels[who.Channel.toUpperCase()]
				&& Channels[who.Channel.toUpperCase()].modelist[CHANMODE_OP][wc.id]
			) {
				continue;
			}
			if (   sf_voice
				&& Channels[who.Channel.toUpperCase()]
				&& Channels[who.Channel.toUpperCase()].modelist[CHANMODE_VOICE][wc.id]
			) {
				continue;
			}
		}
		if ((who.add_flags&WHO_REALNAME) && !wildmatch(wc.realname,who.RealName)) {
			continue;
		} else if ((who.del_flags&WHO_REALNAME) && wildmatch(wc.realname,who.RealName)) {
			continue;
		}
		if ((who.add_flags&WHO_HOST) && !wildmatch(wc.hostname,who.Host)) {
			continue;
		} else if ((who.del_flags&WHO_HOST) && wildmatch(wc.hostname,who.Host)) {
			continue;
		}
		if ((who.add_flags&WHO_IP) && !wildmatch(wc.ip,who.IP)) {
			continue;
		} else if ((who.del_flags&WHO_IP) && wildmatch(wc.ip,who.IP)) {
			continue;
		}
		if (who.add_flags&WHO_UMODE) { /* no -m */
			var sic = false;
			var madd = true;
			for (mm in who.UMode) {
				switch(who.UMode[mm]) {
					case "+":
						if (!madd)
							madd = true;
						break;
					case "-":
						if (madd)
							madd = false;
						break;
					case "o":
					case "i":
					case "w":
					case "b":
					case "g":
					case "s":
					case "c":
					case "r":
					case "k":
					case "f":
					case "y":
					case "d":
					case "n":
					case "h":
					case "F":
						if (
						   (!madd && (wc.mode&
						   USERMODE_CHAR
							[who.UMode[mm]])
						   )
						||
						   (madd && !(wc.mode&
						   USERMODE_CHAR
							[who.UMode[mm]])
						   ) )
							sic = true;
						break;
					default:
						break;
				}
				if (sic)
					break;
			}
			if (sic)
				continue;
		}
		if ((who.add_flags&WHO_NICK) && !wildmatch(wc.nick,who.Nick))
			continue;
		else if ((who.del_flags&WHO_NICK) && wildmatch(wc.nick,who.Nick))
			continue;
		if ((who.add_flags&WHO_OPER) && !(wc.mode&USERMODE_OPER))
			continue;
		else if ((who.del_flags&WHO_OPER) && (wc.mode&USERMODE_OPER))
			continue;
		if ((who.add_flags&WHO_SERVER) && !wildmatch(wc.servername,who.Server))
			continue;
		else if ((who.del_flags&WHO_SERVER) && wildmatch(wc.servername,who.Server))
			continue;
		if ((who.add_flags&WHO_USER) && !wildmatch(wc.uprefix,who.User))
			continue;
		else if ((who.del_flags&WHO_USER) && wildmatch(wc.uprefix,who.User))
			continue;
		if ((who.add_flags&WHO_MEMBER_CHANNEL) && !flag_M)
			continue;
		else if ((who.del_flags&WHO_MEMBER_CHANNEL) && flag_M)
			continue;
		if ((who.add_flags&WHO_TIME) && ((Epoch() - wc.connecttime) < who.Time))
			continue;
		else if ((who.del_flags&WHO_TIME) && ((Epoch() - wc.connecttime) > who.Time))
			continue;
		if ((who.add_flags&WHO_CLASS) && (wc.ircclass != who.Class))
			continue;
		else if ((who.del_flags&WHO_CLASS) && (wc.ircclass == who.Class))
			continue;

		if (whomask && !wc.match_who_mask(whomask))
			continue;

		chan = "";
		if ((who.add_flags&WHO_FIRST_CHANNEL) && !who.Channel) {
			for (x in wc.channels) {
				if (!(Channels[x].mode&CHANMODE_SECRET
						|| Channels[x].mode&CHANMODE_PRIVATE)
					|| this.channels[x] || this.mode&USERMODE_OPER)
				{
					chan = Channels[x].nam;
					break;
				}
			}
		} else if (who.Channel) {
			chan = who.Channel;
		}

		/* If we made it this far, we're good. */
		if (chan && Channels[chan.toUpperCase()]) {
			var chkwho = this.numeric352(wc, who.add_flags, Channels[chan.toUpperCase()]);
			if (!chkwho) {
				umode_notice(USERMODE_OPER,"Notice",
					"WHO returned 0 for user: " + wc.nick + " (C)");
			}
		} else {
			var chkwho = this.numeric352(wc, who.add_flags);
			if (!chkwho) {
				umode_notice(USERMODE_OPER,"Notice",
					"WHO returned 0 for user: " + wc.nick + " (D)");
			}
		}
		who_count++;

		if (!(this.mode&USERMODE_OPER) && (who_count >= MAX_WHO))
			break;
	}

	if (who.Channel)
		eow = who.Channel;
	else if (cmd[2])
		eow = cmd[2];

	this.numeric(315, eow + " :End of /WHO list. (Complex)");
}

/* Object prototype for storing WHO bits and arguments. */
function Who() {
	this.add_flags = 0;
	this.del_flags = 0;
	this.tweak_mode = Who_tweak_mode;
	this.Channel = "";
	this.RealName = "";
	this.Host = "";
	this.IP = "";
	this.UMode = "";
	this.Nick = "";
	this.Server = "";
	this.User = "";
	this.Time = 0;
	this.Class = 0;
}

function Who_tweak_mode(bit,add) {
	if (add) {
		this.add_flags |= bit;
		this.del_flags &= ~bit;
	} else {
		this.add_flags &= ~bit;
		this.del_flags |= bit;
	}
}

/* Take a generic mask in and try to figure out if we're matching a channel,
   mask, nick, or something else.  Return 1 if the user sent to us matches it
   in some fashion. */
function IRCClient_match_who_mask(mask) {
	if ((mask[0] == "#") || (mask[0] == "&")) { // channel
		if (Channels[mask.toUpperCase()])
			return 1;
		else
			return 0; // channel doesn't exist.
	} else if (mask.match(/[!]/)) { // nick!user@host
		if (   wildmatch(this.nick,mask.split("!")[0])
			&& wildmatch(this.uprefix,mask.slice(mask.indexOf("!")+1).split("@")[0])
			&& wildmatch(this.hostname,mask.slice(mask.indexOf("@")+1))
		) {
			return 1;
		}
	} else if (mask.match(/[@]/)) { // user@host
		if (   wildmatch(this.uprefix,mask.split("@")[0])
			&& wildmatch(this.hostname,mask.split("@")[1])
		) {
			return 1;
		}
	} else if (mask.match(/[.]/)) { // host only
		if ( wildmatch(this.hostname,mask) )
			return 1;
	} else { // must be a nick?
		if ( wildmatch(this.nick,mask) )
			return 1;
	}
	return 0;
}

function IRCClient_do_who_usage() {
	this.numeric(334,":/WHO [+|-][acghilmnostuCIM] <args> <mask>");
	this.numeric(334,":The modes as above work exactly like channel modes.");
	this.numeric(334,":<mask> may be '*' or in nick!user@host notation.");
	this.numeric(334,":i.e. '/WHO +a *.ca' would match all away users from *.ca");
	this.numeric(334,":No args/mask matches to everything by default.");
	this.numeric(334,":a       : User is away.");
	this.numeric(334,":c <chan>: User is on <@+><#channel>, no wildcards. Can check +o/+v.");
	this.numeric(334,":g <rnam>: Check against realname field, wildcards allowed.");
	this.numeric(334,":h <host>: Check user's hostname, wildcards allowed.");
	this.numeric(334,":i <ip>  : Check against IP address, wildcards allowed.");
	this.numeric(334,":l <clas>: User is a member of <clas> irc class as defined on a Y:Line.");
	this.numeric(334,":m <umde>: User has <umodes> set, -+ allowed.");
	this.numeric(334,":n <nick>: User's nickname matches <nick>, wildcards allowed.");
	this.numeric(334,":o       : User is an IRC Operator.");
	this.numeric(334,":s <srvr>: User is on <server>, wildcards allowed.");
	this.numeric(334,":t <time>: User has been on for more than (+) or less than (-) <time> secs.");
	this.numeric(334,":u <user>: User's username field matches, wildcards allowed.");
	this.numeric(334,":C       : Only display first channel that the user matches.");
	this.numeric(334,":I       : Only return IP addresses (as opposed to hostnames.)");
	this.numeric(334,":M       : Only check against channels you're a member of.");
	if (this.mode&USERMODE_OPER) {
		this.numeric(334,":X       : *OPER ONLY* Show 'parent' object in realname field.");
		this.numeric(334,":Y       : *OPER ONLY* Show object 'id' in realname field.");
		this.numeric(334,":Z       : *OPER ONLY* Show 'callback id' in realname field.");
	}
	this.numeric(315,"? :End of /WHO list. (Usage)");
}

function IRCClient_do_basic_list(mask) {
	var i, my_split;

	this.numeric321();
	/* Only allow commas if we're not doing wildcards, otherwise strip
	   off anything past the first comma and pass to the generic parser
	   to see if it can make heads or tails out of it. */
	if (mask.match(/[,]/)) {
		if (mask.match(/[*?]/)) {
			mask = mask.slice(0,mask.indexOf(","))
		} else { /* parse it out, but junk anything that's not a chan */
			my_split = mask.split(",");
			for (i in my_split) {
				if (Channels[my_split[i].toUpperCase()])
					this.numeric322(Channels[my_split[i].toUpperCase()]);
			}
			this.numeric(323, ":End of /LIST. (Basic: Comma-list)");
			return;
		}
	}
	for (i in Channels) {
		if (Channels[i] && Channels[i].match_list_mask(mask))
			this.numeric322(Channels[i]);
	}
	this.numeric(323, ":End of /LIST. (Basic)");
	return;
}

function IRCClient_do_complex_list(cmd) {
	var i, l, tmp;
	var add = true;
	var arg = 0;
	var list = new List();
	var listmask;
	var listmask_items;

	this.numeric321();

	for (i in cmd[0]) {
		switch(cmd[0][i]) {
			case "+":
				if (!add)
					add = true;
				break;
			case "-":
				if (add)
					add = false;
				break;
			case "a":
				arg++;
				if (cmd[arg]) {
					list.tweak_mode(LIST_CHANMASK,add);
					list.Mask = cmd[arg];
				}
				break;
			case "c":
				arg++;
				if (cmd[arg]) {
					list.tweak_mode(LIST_CREATED,add);
					list.Created = parseInt(cmd[arg])*60;
				}
				break;
			case "m": /* we never set -m, inverse. */
				arg++;
				if (cmd[arg]) {
					list.tweak_mode(LIST_MODES,true);
					if (!add) {
						tmp = "";
						if((cmd[arg][0] != "+") ||
						   (cmd[arg][0] != "-") )
							tmp += "+";
						tmp += cmd[arg].replace(/[-]/g," ");
						tmp = tmp.replace(/[+]/g,"-");
						list.Modes = tmp.replace(/[ ]/g,"+");
					} else {
						list.Modes = cmd[arg];
					}
				}
				break;
			case "o":
				arg++;
				if (cmd[arg]) {
					list.tweak_mode(LIST_TOPIC,add);
					list.Topic = cmd[arg];
				}
				break;
			case "p":
				arg++;
				if (cmd[arg]) {
					list.tweak_mode(LIST_PEOPLE,add);
					list.People = parseInt(cmd[arg]);
				}
				break;
			case "t":
				arg++;
				if (cmd[arg]) {
					list.tweak_mode(LIST_TOPICAGE,add);
					list.TopicTime = parseInt(cmd[arg])*60;
				}
				break;
			case "M":
				list.tweak_mode(LIST_DISPLAY_CHAN_MODES,add);
				break;
			case "T":
				list.tweak_mode(LIST_DISPLAY_CHAN_TS,add);
				break;
			default:
				break;
		}
	}

	/* Generic mask atop all this crap? */
	arg++;
	if (cmd[arg])
		listmask = cmd[arg];

	for (i in Channels) {
		if (   !(Channels[i].mode&CHANMODE_SECRET)
			|| (this.mode&USERMODE_OPER)
			|| this.channels[i]
		) {
			
			if ((list.add_flags&LIST_CHANMASK) &&
			    !wildmatch(i,list.Mask.toUpperCase()))
				continue;
			else if ((list.del_flags&LIST_CHANMASK) &&
			    wildmatch(i,list.Mask.toUpperCase()))
				continue;
			if ((list.add_flags&LIST_CREATED) &&
			   (Channels[i].created < (Epoch() - list.Created)))
				continue;
			else if ((list.del_flags&LIST_CREATED) &&
			   (Channels[i].created > (Epoch() - list.Created)))
				continue;
			if ((list.add_flags&LIST_TOPIC) &&
			   (!wildmatch(Channels[i].topic,list.Topic)))
				continue;
			else if ((list.del_flags&LIST_TOPIC) &&
			   (wildmatch(Channels[i].topic,list.Topic)))
				continue;
			if ((list.add_flags&LIST_PEOPLE) &&
			    (true_array_len(Channels[i].users) < list.People) )
				continue;
			else if ((list.del_flags&LIST_PEOPLE) &&
			    (true_array_len(Channels[i].users) >= list.People) )
				continue;
			if ((list.add_flags&LIST_TOPICAGE) && list.TopicTime &&
			  (Channels[i].topictime > (Epoch()-list.TopicTime)))
				continue;
			else if((list.del_flags&LIST_TOPICAGE)&&list.TopicTime&&
			  (Channels[i].topictime < (Epoch()-list.TopicTime)))
				continue;
			if (list.add_flags&LIST_MODES) { /* there's no -m */
				var sic = false;
				var madd = true;
				var c = Channels[i];
				for (mm in list.Modes) {
					switch(list.Modes[mm]) {
						case "+":
							if (!madd)
								madd = true;
							break;
						case "-":
							if (madd)
								madd = false;
							break;
						case "i":
							if (   (!madd && (c.mode&CHANMODE_INVITE))
								|| (madd && !(c.mode&CHANMODE_INVITE))
							) {
								sic = true;
							}
							break;
						case "k":
							if (   (!madd && (c.mode&CHANMODE_KEY))
								|| (madd && !(c.mode&CHANMODE_KEY))
							) {
								sic = true;
							}
							break;
						case "l":
							if (   (!madd && (c.mode&CHANMODE_LIMIT))
								|| (madd && !(c.mode&CHANMODE_LIMIT))
							) {
								sic = true;
							}
							break;
						case "m":
							if (   (!madd && (c.mode&CHANMODE_MODERATED))
								|| (madd && !(c.mode&CHANMODE_MODERATED))
							) {
								sic = true;
							}
							break;
						case "n":
							if (   (!madd && (c.mode&CHANMODE_NOOUTSIDE))
								|| (madd && !(c.mode&CHANMODE_NOOUTSIDE))
							) {
								sic = true;
							}
							break;
						case "p":
							if (   (!madd && (c.mode&CHANMODE_PRIVATE))
								|| (madd && !(c.mode&CHANMODE_PRIVATE))
							) {
								sic = true;
							}
							break;
						case "s":
							if (   (!madd && (c.mode&CHANMODE_SECRET))
								|| (madd && !(c.mode&CHANMODE_SECRET))
							) {
								sic = true;
							}
							break;
						case "t":
							if (   (!madd && (c.mode&CHANMODE_TOPIC))
								|| (madd && !(c.mode&CHANMODE_TOPIC))
							) {
								sic = true;
							}
							break;
						default:
							break;
					}
					if (sic)
						break;
				}
				if (sic)
					continue;
			}

			if (listmask)
				listmask_items = listmask.split(",");
			var l_match = false; /* assume we match nothing. */
			if (listmask_items) {
				for (l in listmask_items) {
					if (Channels[i].match_list_mask
					   (listmask_items[l])) {
						l_match = true;
						break;
					}
				}
				if (!l_match)
					continue;
			}

			this.numeric322(Channels[i],list.add_flags);
		}
	}

	this.numeric(323, ":End of /LIST. (Complex)");
}
		
/* Object prototype for storing LIST bits and arguments. */
function List() {
	this.add_flags = 0;
	this.del_flags = 0;
	this.tweak_mode = List_tweak_mode;
	this.People = 0;
	this.Created = 0;
	this.TopicTime = 0;
	this.Mask = "";
	this.Modes = "";
	this.Topic = "";
}

function List_tweak_mode(bit,add) {
	if (add) {
		this.add_flags |= bit;
		this.del_flags &= ~bit;
	} else {
		this.add_flags &= ~bit;
		this.del_flags |= bit;
	}
}

function IRCClient_do_list_usage() {
	this.numeric(334,":/LIST [+|-][acmoptM] <args> <mask|channel{,channel}>");
	this.numeric(334,":The modes as above work exactly like channel modes.");
	this.numeric(334,":<mask> may be just like Bahamut notation.");
	this.numeric(334,":(Bahamut Notation = >num,<num,C>num,C<num,T>num,T<num,*mask*,!*mask*)");
	this.numeric(334,":i.e. '/LIST +p 50 #*irc*' lists chans w/ irc in the name and 50+ users.");
	this.numeric(334,":No args/mask matches to everything by default.");
	this.numeric(334,":a <mask>: List channels whose names match the mask.  Wildcards allowed.");
	this.numeric(334,":c <time>: Chans created less than (-) or more than (+) <time> mins ago.");
	this.numeric(334,":m <mods>: Channel has <modes> set.  -+ allowed.");
	this.numeric(334,":o <topc>: Match against channel's <topic>, wildcards allowed.");
	this.numeric(334,":p <num> : Chans with more or equal to (+) members, or (-) less than.");
	this.numeric(334,":t <time>: Only channels whose topics were created <time> mins ago.");
	this.numeric(334,":M       : Prefix topic with the current channel mode.");
	this.numeric(334,":T       : Prefix topic with channel's creation date in Unix epoch.");
	/* No "end of" numeric for this. */
}

/* does 'this' (channel) match the 'mask' passed to us?  Use 'complex'
   Bahamut-style parsing to determine that. */
function Channel_match_list_mask(mask) {
	if (mask[0] == ">") { /* Chan has more than X people? */
		if (true_array_len(this.users) < parseInt(mask.slice(1)))
			return 0;
	} else if (mask[0] == "<") { /* Chan has less than X people? */
		if (true_array_len(this.users) >= parseInt(mask.slice(1)))
			return 0;
	} else if (mask[0].toUpperCase() == "C") { /* created X mins ago? */
		if (   (mask[1] == ">")
			&& (this.created < (Epoch() - (parseInt(mask.slice(2)) * 60)))
		) {
			return 0;
		} else if (   (mask[1] == "<")
					&& (this.created > (Epoch() - (parseInt(mask.slice(2)) * 60)))
		) {
			return 0;
		}
	} else if (mask[0].toUpperCase() == "T") { /* topics older than X mins? */
		if (   (mask[1] == ">")
			&& (this.topictime < (Epoch() - (parseInt(mask.slice(2)) * 60)) )
		) {
			return 0;
		} else if (   (mask[1] == "<")
					&& (this.topictime > (Epoch() - (parseInt(mask.slice(2)) * 60)))
		) {
			return 0;
		}
	} else if (mask[0] == "!") { /* doesn't match mask X */
		if (wildmatch(this.nam,mask.slice(1).toUpperCase()))
			return 0;
	} else { /* if all else fails, we're matching a generic channel mask. */
		if (!wildmatch(this.nam,mask))
			return 0;
	}
	return 1; /* if we made it here, we matched something. */
}

function IRCClient_get_usermode(bcast_modes) {
	var i;
	var tmp_mode = "+";

	for (i in USERMODE_CHAR) {
		if (   (!bcast_modes || (bcast_modes && USERMODE_BCAST[i]))
			&& this.mode&USERMODE_CHAR[i]
		) {
			tmp_mode += i;
		}
	}
	return tmp_mode;
}

function UMode_tweak_mode(bit,add) {
	if (add) {
		this.add_flags |= bit;
		this.del_flags &= ~bit;
	} else {
		this.add_flags &= ~bit;
		this.del_flags |= bit;
	}
}

function UMode() {
	this.add_flags = 0;
	this.del_flags = 0;
	this.tweak_mode = UMode_tweak_mode;
}

function IRCClient_setusermode(modestr) {
	var i;

	if (!modestr)
		return 0;
	var add=true;
	var unknown_mode=false;
	var umode = new UMode();
	for (i in modestr) {
		switch (modestr[i]) {
			case "+":
				if (!add)
					add=true;
				break;
			case "-":
				if (add)
					add=false;
				break;
			case "i":
			case "w":
			case "s":
			case "k":
			case "g":
				umode.tweak_mode(USERMODE_CHAR
					[modestr[i]],add);
				break;
			case "b":
			case "r":
			case "f":
			case "y":
			case "d":
			case "n":
				if (this.mode&USERMODE_OPER)
					umode.tweak_mode(USERMODE_CHAR
						[modestr[i]],add);
				break;
			case "o":
				/* Allow +o only by servers or non-local users. */
				if (add && this.parent &&
				    Servers[this.parent.toLowerCase()].hub)
					umode.tweak_mode(USERMODE_OPER,true);
				else if (!add)
					umode.tweak_mode(USERMODE_OPER,false);
				break;
			case "c":
				if ((this.mode&USERMODE_OPER) &&
				    (this.flags&OLINE_CAN_UMODEC))
					umode.tweak_mode(USERMODE_CLIENT,add);
				break;
			case "A":
				if ( ((this.mode&USERMODE_OPER) && (this.flags&OLINE_IS_ADMIN))
					|| (this.parent && Servers[this.parent.toLowerCase()].hub) )
					umode.tweak_mode(USERMODE_ADMIN,add);
				break;
			default:
				if (!unknown_mode && !this.parent) {
					this.numeric(501, ":Unknown MODE flag");
					unknown_mode=true;
				}
				break;
		}
	}
	var addmodes = "";
	var delmodes = "";
	var bcast_addmodes = "";
	var bcast_delmodes = "";
	for (i in USERMODE_CHAR) {
		if (   (umode.add_flags&USERMODE_CHAR[i])
			&& !(this.mode&USERMODE_CHAR[i])
		) {
			addmodes += i;
			if (USERMODE_BCAST[i])
				bcast_addmodes += i;
			this.mode |= USERMODE_CHAR[i];
		} else if (   (umode.del_flags&USERMODE_CHAR[i])
					&& (this.mode&USERMODE_CHAR[i])
		) {
			delmodes += i;
			if (USERMODE_BCAST[i])
				bcast_delmodes += i;
			this.mode &= ~USERMODE_CHAR[i];
		}
	}
	if (!addmodes && !delmodes)
		return 0;
	var final_modestr = "";
	var bcast_modestr = "";
	if (addmodes)
		final_modestr += "+" + addmodes;
	if (delmodes)
		final_modestr += "-" + delmodes;
	if (bcast_addmodes)
		bcast_modestr += "+" + bcast_addmodes;
	if (bcast_delmodes)
		bcast_modestr += "-" + bcast_delmodes;
	if (this.local && !this.server) {
		this.originatorout("MODE "+this.nick+" "+final_modestr,this);
		if (bcast_addmodes || bcast_delmodes)
			this.bcast_to_servers("MODE "+this.nick+" "+bcast_modestr);
	}
	return bcast_modestr;
}

function IRCClient_check_timeout() {
	if (   !this.pinged
		&& ((system.timer - this.idletime) > YLines[this.ircclass].pingfreq)
	) {
		this.pinged = system.timer;
		this.rawout("PING :" + ServerName);
	} else if (   this.pinged
				&& ((system.timer - this.pinged) > YLines[this.ircclass].pingfreq)
	) {
		this.quit(format("Ping Timeout (%d seconds)", YLines[this.ircclass].pingfreq));
		return 1;
	}
	return 0;
}

function IRCClient_finalize_server_connect(states) {
	var i;

	HCC_Counter++;
	gnotice(format("Link with %s [unknown@%s] established, states: %s",
		this.nick,
		this.hostname,
		states
	));
	if (server.client_update !== undefined)
		server.client_update(this.socket, this.nick, this.hostname);
	if (!this.socket.outbound) {
		for (i in CLines) {
			if(wildmatch(this.nick,CLines[i].servername)) {
				this.rawout(format(
					"PASS %s :%s",
					CLines[i].password,
					states
				));
				break;
			}
		}
		this.rawout("CAPAB " + SERVER_CAPAB);
		this.rawout("SERVER " + ServerName + " 1 :" + ServerDesc);
	}
	this.bcast_to_servers_raw(format(
		":%s SERVER %s 2 :%s",
		ServerName,
		this.nick,
		this.info
	));
	this.synchronize();
}

function accept_new_socket() {
	var unreg_obj;
	var id;
	var sock;

	sock = this.accept();

	if (!sock) {
		log(LOG_DEBUG,format("!ERROR %s accepting connection", this.error));
		return false;
	}

	sock.array_buffer = false; /* JS78, we want strings */

	if (!sock.local_port || !sock.remote_port) {
		log(LOG_DEBUG,"!ERROR Socket seems to have no port.  Closing.");
		sock.close();
		return false;
	}

	log(LOG_DEBUG,"Accepting new connection on port " + sock.local_port);

	switch (sock.local_port) {
		case 994:
		case 6697:
			try {
				sock.ssl_server = 1;
			} catch(e) {
				log(LOG_INFO,"!ERROR Couldn't enable SSL on new connection. Closing.");
				sock.close();
				return 1;
			}
	}

	if (!sock.remote_ip_address) {
		log(LOG_DEBUG,"!ERROR Socket has no IP address.  Closing.");
		sock.close();
		return false;
	}

	if (IP_Banned(sock.remote_ip_address)) {
		sock.send(format(
			":%s 465 * :You've been banned from this server.\r\n",
			ServerName
		));
		sock.close();
		return false;
	}

	if (server.client_add !== undefined) {
		server.client_add(sock);
	}

	if (server.clients !== undefined) {
		log(LOG_DEBUG,format("%d clients", server.clients));
	}

	sock.callback_id = sock.on("read", Socket_Recv);

	id = Generate_ID();
	unreg_obj = new Unregistered_Client(id, sock);

	Unregistered[id] = unreg_obj;

	return true;
}

function Startup() {
	/* Parse command-line arguments. */
	var cmdline_port;
	var cmdline_addr;
	var cmdarg;

	for (cmdarg=0;cmdarg<argc;cmdarg++) {
		switch(argv[cmdarg].toLowerCase()) {
			case "-f":
				Config_Filename = argv[++cmdarg];
				break;
			case "-p":
				cmdline_port = parseInt(argv[++cmdarg]);
				break;
			case "-a":
				cmdline_addr = argv[++cmdarg].split(',');
				break;
		}
	}

	Read_Config_File();

	/* This tests if we're running from JSexec or not */
	if (server == "JSexec") {

		if (cmdline_port)
			Default_Port = cmdline_port;

		server = {
			socket: false,
			terminated: false,
			interface_ip_addr_list: (cmdline_addr || ["0.0.0.0","::"])
		};

		if (typeof jsexec_revision_detail !== 'undefined')
			server.version_detail = jsexec_revision_detail;
		else if (typeof jsdoor_revision_detail !== 'undefined')
			server.version_detail = jsdoor_revision_detail;

		server.socket = create_new_socket(Default_Port)

		if (!server.socket)
			exit();
	}

	server.socket.nonblocking = true;
	server.socket.debug = false;
	server.socket.callback_id = server.socket.on("read", accept_new_socket);
}

function Open_PLines() {
	var i, sock;

	for (i in PLines) {
		sock = create_new_socket(PLines[i]);
		if (sock) {
			sock.nonblocking = true;
			sock.debug = false;
			sock.on("read", accept_new_socket);
		}
	}
}

function Uptime_String() {
	var uptime, days, hours, mins, secs;

	uptime = system.timer - SERVER_UPTIME;
	days = Math.floor(uptime / 86400);
	if (days)
		uptime %= 86400;
	hours = Math.floor(uptime/(60*60));
	mins = (Math.floor(uptime/60))%60;
	secs = uptime%60;

	return format(
		"Server Up %u days, %u:%02u:%02u",
		days, hours, mins, secs
	);
}

function Epoch() {
	return parseInt(new Date().getTime()/1000);
}

function YLine_Decrement(yline) {
	if (typeof yline !== 'object')
		throw "YLine_Decrement() called without yline object.";

	if (yline.active < 1) {
		log(LOG_DEBUG, "Y:Line trying to decrement below zero");
		yline.active = 0;
		return false;
	}

	yline.active--;
	log(LOG_DEBUG, format("Class down to %u active out of %u",
		yline.active,
		yline.maxlinks
	));
	return true;
}

function YLine_Increment(yline) {
	if (typeof yline !== 'object')
		throw "YLine_Increment() called without yline object.";

	yline.active++;
	log(LOG_DEBUG, format("Class up to %u active out of %u",
		yline.active,
		yline.maxlinks
	));
	return true;
}

/** Global object prototypes **/

function CLine(host,password,servername,port,ircclass) {
	this.host = host;
	this.password = password;
	this.servername = servername;
	this.port = port;
	this.ircclass = ircclass;
	if (   YLines[ircclass].connfreq > 0
		&& parseInt(port) > 0
		&& !Servers[servername.toLowerCase()]
	) {
		Reset_Autoconnect(this, 1 /* connect immediately */);
	}
}

function HLine(allowedmask,servername) {
	this.allowedmask = allowedmask;
	this.servername = servername;
}

function ILine(ipmask,password,hostmask,port,ircclass) {
	this.ipmask = ipmask;
	this.password = password;
	this.hostmask = hostmask;
	this.port = port;
	this.ircclass = ircclass;
}

function KLine(hostmask,reason,type) {
	this.hostmask = hostmask;
	this.reason = reason;
	this.type = type;
}

function NLine(host,password,servername,flags,ircclass) {
	this.host = host;
	this.password = password;
	this.servername = servername;
	this.flags = flags;
	this.ircclass = ircclass;
}

function OLine(hostmask,password,nick,flags,ircclass) {
	this.hostmask = hostmask;
	this.password = password;
	this.nick = nick;
	this.flags = flags;
	this.ircclass = ircclass;
}

function QLine(nick,reason) {
	this.nick = nick;
	this.reason = reason;
}

function YLine(pingfreq,connfreq,maxlinks,sendq) {
	this.pingfreq = pingfreq;
	this.connfreq = connfreq;
	this.maxlinks = maxlinks;
	this.sendq = sendq;
	this.active = 0;
}

function ZLine(ipmask,reason) {
	this.ipmask = ipmask;
	this.reason = reason;
}

function WhoWasObj(nick,uprefix,host,realname,server,serverdesc) {
	this.nick = nick;
	this.uprefix = uprefix;
	this.host = host;
	this.realname = realname;
	this.server = server;
	this.serverdesc = serverdesc;
}

function NickBuf(oldnick,newnick) {
	this.oldnick = oldnick;
	this.newnick = newnick;
}

/* An "SJOIN-style nick" can be in the form of:
   Nick, @Nick, @+Nick, +Nick. */
function SJOIN_Nick(str) {
	if (!str)
		return 0;

	this.isop = false;
	this.isvoice = false;
	if (str[0] == "@") {
		this.isop = true;
		str = str.slice(1);
	}
	if (str[0] == "+") {
		this.isvoice = true;
		str = str.slice(1);
	}
	this.nick = str;
}

function IRC_Queue(irc) {
	this.irc = irc;
	this.socket = irc.socket;
	this.queue = [];
	this._recv_bytes = '';
	this._send_bytes = '';
	this.add = Queue_Add;
	this.del = Queue_Del;
	this.prepend = Queue_Prepend;
	this.recv = Queue_Recv;
	this.send = Queue_Send;
	this.limit = 1000000; /* 1MB by default is plenty */
	this.time_marker = 0; /* system.timer */
	this.num_sent = 0;
	this.throttled = false;
	this.purge = function() {
		this.queue = [];
		this._recv_bytes = '';
		this._send_bytes = '';
	}
	this.__defineGetter__("size", function() {
		var ret = 0;
		var i;

		for (i in this.queue) {
			ret += this.queue[i].length;
		}

		ret += this._recv_bytes.length + this._send_bytes.length;

		return ret;
	});
	this.__defineGetter__("empty", function() {
		if (this.queue.length > 0)
			return false;
		if (this._recv_bytes.length > 0) 
			return false;
		if (this._send_bytes.length > 0)
			return false;
		return true;
	});
}

/* /STATS M, for profiling. */
function StatsM() {
	this.ticks = 0;
	this.executions = 0;
}

