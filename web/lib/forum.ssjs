// Forum functions for ecWeb v3
// echicken -at- bbs.electronicchicken.com

load('sbbsdefs.js');
load('msgutils.js');

var getSig = function() {
	var sigFile = format("%04s", user.number.toString()) + ".sig";
	if(file_exists(system.data_dir + "user/" + sigFile)) {
		var f = new File(system.data_dir + "user/" + sigFile);
		f.open("r");
		var sig = f.read();
		f.close();
	}
	if(sig !== undefined)
		return sig;
	else
		return false;
}

var sig = getSig();

var linkify = function(body) {
	// Magical URL-ify
	urlRE=/(?:https?|ftp|telnet|ssh|gopher|rlogin|news):\/\/[^\s'"'<>()]*|[-\w.+]+@(?:[-\w]+\.)+[\w]{2,6}/gi;
	body=body.replace(urlRE, 
		function (str) {
			var ret=''
			var p=0;
			var link=str.replace(/\.*$/, '');
			var linktext=link;
			if(link.indexOf('://')==-1)
				link='mailto:'+link;
			return('<a class="ulLink" href="'+link+'">'+linktext+'</a>'+str.substr(linktext.length));
		}
	);
	return(body);
}

var printBoards = function() {
	for(var g = 0; g < msg_area.grp_list.length; g++) {
		var out = format(
			"<div class='border box msg'>"
			+ "<a class='ulLink' onclick='toggleVisibility(\"group-%s\")'>%s</a>"
			+ "<br />%s sub-boards</div>"
			+ "<div id='group-%s' style=display:none;'>",
			msg_area.grp_list[g].name,
			msg_area.grp_list[g].name,
			msg_area.grp_list[g].sub_list.length,
			msg_area.grp_list[g].name
		);
		for(var s = 0; s < msg_area.grp_list[g].sub_list.length; s++) {
			out += format(
				"<div class='border msg indentBox1'>"
				+ "<a class='ulLink' onclick='loadThreads(\"http://%s/%sforum-async.ssjs\", \"%s\")'>%s</a><br />",
				http_request.host,
				webIni.appendURL,
				msg_area.grp_list[g].sub_list[s].code,
				msg_area.grp_list[g].sub_list[s].name
			);
			try {
				var msgBase = new MsgBase(msg_area.grp_list[g].sub_list[s].code);
				msgBase.open();
			} catch(err) {
				log(LOG_ERR, err);
				continue;
			}
			out += ((webIni.maxMessages > 0 && webIni.maxMessages < msgBase.total_msgs)?webIni.maxMessages:msgBase.total_msgs) + " messages.<br />";
			var h = msgBase.get_msg_header(msgBase.last_msg);
			msgBase.close();
			if(h !== null)
				out += format("Latest: %s, by %s on %s", h.subject, h.from, system.timestr(h.when_written_time));
			if(user.alias != webIni.WebGuest && user.compare_ars(msgBase.cfg.post_ars)) {
				out += format(
					"<br />"
					+ "<a class='ulLink' onclick='addPost(\"http://%s/%sforum-async.ssjs\", \"%s\", \"%s\", \"%s\")'>Post a new message</a>"
					+ "<div id='sub-%s-newMsgBox'></div>",
					http_request.host,
					webIni.appendURL,
					msg_area.grp_list[g].sub_list[s].code,
					user.alias,
					user.name,
					msg_area.grp_list[g].sub_list[s].code
				);
			}
			out += format(
				"</div>"
				+ "<div id='sub-%s-info' class='border msg indentBox2' style='display:none;'></div>"
				+ "<div id='sub-%s' style='display:none;'></div>",
				msg_area.grp_list[g].sub_list[s].code,
				msg_area.grp_list[g].sub_list[s].code
			);
		}
		out += "</div>";
		print(out);
	}
	return;
}

var printThreads = function(sub) {
	try {
		msgBase = new MsgBase(sub);
		msgBase.open();
	} catch(err) {
		log(LOG_ERR, err);
		return false;
	}
	msgBase.close();
	var threads = getMessageThreads(sub, webIni.maxMessages);
	for(var t in threads.order) {
		var header = threads.thread[threads.order[t]].messages[0];
		var out = format(
			"<a class='ulLink' name='thread-%s'></a>"
			+ "<div class='border %s msg'>"
			+ "<a class='ulLink' onclick='loadThread(\"http://%s/%sforum-async.ssjs\", \"%s\", \"%s\")'>%s</a><br />"
			+ "Started by %s on %s<br />",
			header.number,
			((sub == "mail")?"box":"indentBox2"),
			http_request.host,
			webIni.appendURL,
			sub,
			threads.order[t],
			header.subject,
			header.from,
			system.timestr(header.when_written_time)
		);
		if(threads.thread[threads.order[t]].messages.length > 1) {
			out += format(
				"Latest reply from %s on %s",
				threads.thread[threads.order[t]].messages[threads.thread[threads.order[t]].messages.length - 1].from,
				system.timestr(threads.thread[threads.order[t]].messages[threads.thread[threads.order[t]].messages.length - 1].when_written_time)
			);
		}
		out += format(
			"</div>"
			+ "<div id='sub-%s-thread-%s' style='display:none;'></div>"
			+ "<div id='sub-%s-thread-%s-info' class='border msg indentBox2' style='display:none;'></div>",
			sub, threads.order[t], sub, threads.order[t]
		);
		print(out);
	}
}

var printThread = function(sub, t) {
	try {
		var msgBase = new MsgBase(sub);
		msgBase.open();
	} catch(err) {
		log(LOG_ERR, err);
		return false;
	}
	var threads = getMessageThreads(sub, webIni.maxMessages);
	if(typeof threads.thread[t] == "undefined") {
		msgBase.close();
		print("Thread does not exist.");
		return true;
	}
	for(var m in threads.thread[t].messages) {
		var header = threads.thread[t].messages[m];
		var body = msgBase.get_msg_body(header.number, strip_ctrl_a=true);
		if(body === null)
			continue;
		var out = format(
			"<a name='%s-%s'></a>"
			+ "<div class='border %s msg' id='sub-%s-thread-%s-%s'>"
			+ "From <b>%s</b> to <b>%s</b> on <b>%s</b><br /><br />"
			+ "%s<br /><br />",
			sub,
			header.number,
			((sub == "mail")?"indentBox2":"indentBox3"),
			sub,
			t,
			header.number,
			header.from,
			header.to,
			system.timestr(header.when_written_time),
			linkify(strip_exascii(body).replace(/\r\n/g, "<br />").replace(/\n/g, "<br />"))
		);
		if(sub == 'mail' || msg_area.sub[msgBase.cfg.code].is_operator) {
			out += format(
				"<a class='ulLink' onclick='deleteMessage(\"http://%s/%sforum-async.ssjs\", \"%s\", \"%s\", \"sub-%s-thread-%s-%s\")'>Delete Message</a> - ",
				http_request.host, webIni.appendURL, sub, header.number, sub, t, header.number
			);
		}
		if(sub != 'mail') {
			out += format(
				"<a class='ulLink' href='./index.xjs?page=002-forum.ssjs&board=%s&sub=%s&thread=%s#thread-%s'>Thread URL</a> - "
				+ "<a class='ulLink' href='./index.xjs?page=002-forum.ssjs&board=%s&sub=%s&thread=%s&message=%s#%s-%s'>Message URL</a> - ",
				msgBase.cfg.grp_name,
				sub,
				t,
				t,
				msgBase.cfg.grp_name,
				sub,
				t,
				header.number,
				sub,
				header.number
			);
		} else if(header.attr|MSG_READ) {
			header.attr|=MSG_READ;
			msgBase.put_msg_header(header.number, header);
		}
		out += format(
			"<a class='ulLink' href='#thread-%s' onclick='toggleVisibility(\"sub-%s-thread-%s\")'>Collapse Thread</a>",
			t,
			sub,
			t
		);
		if(user.alias != webIni.WebGuest && sub == 'mail' || (sub != 'mail' && user.compare_ars(msgBase.cfg.post_ars)))
			out += format(
				" - <a class='ulLink' onclick='addReply(\"http://%s/%sforum-async.ssjs\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\")'>Reply</a>",
				http_request.host, webIni.appendURL, sub, t, header.number, header.from, user.alias, header.subject
			);
		out += "</div>";
		print(out);
	}
	msgBase.close();
}

var printMessage = function(sub, number) {
	try {
		var msgBase = new MsgBase(sub);
		msgBase.open();
	} catch(err) {
		log(LOG_ERR, err);
		return false;
	}
	if(sub != "mail" && (!user.compare_ars(msgBase.cfg.read_ars) || !user.compare_ars(msgBase.cfg.post_ars)))
		return false;
	var number = parseInt(number);
	if(isNaN(number))
		return false;
	var ret = { "header" : msgBase.get_msg_header(number) };
	if(ret.header === null)
		return false;
	ret.body = strip_exascii(msgBase.get_msg_body(number));
	msgBase.close();
	ret.user = { 
		"alias" : user.alias,
		"name" : user.name
	};
	if(sig)
		ret.sig = sig;
	print(JSON.stringify(ret));
}

var postMessage = function(sub, irt, to, from, subject, body) {
	for(var a = 0; a < arguments.length; a++) {
		if(a == "irt")
			continue;
		if(arguments[a] === undefined || arguments[a] === null || arguments[a] == "")
			return false;
	}
	if(user.alias != from && user.name != from)
		return false;
	try {
		var msgBase = new MsgBase(sub);
		msgBase.open();
	} catch(err) {
		log(LOG_ERR, err);
		return false;
	}
	if(sub != "mail" && !user.compare_ars(msgBase.cfg.post_ars))
		return false;
	var header = {
		"to" : to,
		"from" : from,
		"from_ext" : user.number,
		"replyto" : from,
		"replyto_ext" : user.number,
		"subject" : subject
	}
	if(irt !== undefined && parseInt(irt) > 0)
		header.thread_back = irt;
	if(sub == "mail") {
		var na = netaddr_type(to);
		if(na > 0) {
			header.to_net_type = na;
			header.to_net_addr = to;
		}
	}
	msgBase.save_msg(header, body);
	msgBase.close();
	print("Your message has been posted.");
	return true;
}

var deleteMessage = function(sub, message) {
	try {
		var msgBase = new MsgBase(sub);
		msgBase.open();
	} catch(err) {
		log(LOG_ERR, err);
		return false;
	}
	if(sub != "mail" && !msg_area.sub[msgBase.cfg.code].is_operator) {
		print("You're not allowed to delete that message.");
		return false;
	}
	var message = parseInt(message);
	if(isNaN(message)) {
		print("Invalid message number.");
		return false;
	}
	var header = msgBase.get_msg_header(message);
	if(header === null) {
		print("Invalid message.");
		return false;
	}
	if(sub == "mail" && header.to != user.alias && header.to != user.name && header.to_ext != user.number) {
		print("You're not allowed to delete that message.");
		return false;
	}
	header.attr|=MSG_DELETE;
	msgBase.put_msg_header(message, header);
	print("Message deleted.");
	return true;
}