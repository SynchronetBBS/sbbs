// Forum functions for ecWeb v3
// echicken -at- bbs.electronicchicken.com

load('msgutils.js'); // Supplies getMessageThreads(sub)

function getSig() {
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

function linkify(body) {
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
			return('<a href="'+link+'">'+linktext+'</a>'+str.substr(linktext.length));
		}
	);
	return(body);
}

function printBoards() {
	out = "";
	for(var g = 0; g < msg_area.grp_list.length; g++) {
		out += "<div class='border box msg'>";
		out += format(
			"<a class='ulLink' onclick='toggleVisibility(\"group-%s\")'>%s</a><br />",
			msg_area.grp_list[g].name, msg_area.grp_list[g].name
			);
		out += msg_area.grp_list[g].sub_list.length + " sub-boards";
		out += "</div>";
		out += "<div id='group-" + msg_area.grp_list[g].name + "' style='display:none;'>";
		for(var s = 0; s < msg_area.grp_list[g].sub_list.length; s++) {
			out += "<div class='border msg indentBox1'>";
			out += format(
				"<a class='ulLink' onclick='loadThreads(\"http://%s:%s/%s/forum-async.ssjs\", \"%s\")'>%s</a><br />",
				system.inet_addr, webIni.HTTPPort, webIni.appendURL, msg_area.grp_list[g].sub_list[s].code, msg_area.grp_list[g].sub_list[s].name
			);
			var msgBase = new MsgBase(msg_area.grp_list[g].sub_list[s].code);
			msgBase.open();
			out += msgBase.total_msgs + " messages.<br />";
			var h = msgBase.get_msg_header(msgBase.last_msg);
			msgBase.close();
			if(h !== null)
				out += format("Latest: %s, by %s on %s", h.subject, h.from, system.timestr(h.when_written_time));
			if(user.alias != webIni.WebGuest && user.compare_ars(msgBase.cfg.post_ars)) {
				out += format(
					"<br /><a class='ulLink' onclick='addPost(\"http://%s:%s/%s/forum-async.ssjs\", \"%s\", \"%s\", \"%s\", \"%s\")'>Post a new message</a>",
					system.inet_addr, webIni.HTTPPort, webIni.appendURL, msg_area.grp_list[g].sub_list[s].code, user.alias, user.name
				);
				out += format("<div id='sub-%s-newMsgBox'></div>", msg_area.grp_list[g].sub_list[s].code);
			}
			out += "</div>";
			out += format(
				"<div id='sub-%s-info' class='border msg indentBox2' style='display:none;'></div>",
				msg_area.grp_list[g].sub_list[s].code
			);
			out += format("<div id='sub-%s' style='display:none;'></div>", msg_area.grp_list[g].sub_list[s].code);
		}
		out += "</div>";
	}
	print(out);
	return;
}

function printThreads(sub) {
	var msgBase = new MsgBase(sub);
	if(!msgBase.open())
		return false;
	msgBase.close();
	var threads = getMessageThreads(sub);
	var out = "";	
	for(var t in threads.order) {
		var header = threads.thread[threads.order[t]].messages[0];
		out += format("<a name='thread-%s'></a>", header.number);
		out += "<div class='border indentBox2 msg'>";
		out += format(
			"<a class='ulLink' onclick='loadThread(\"http://%s:%s/%s/forum-async.ssjs\", \"%s\", \"%s\")'>%s</a><br />",
			system.inet_addr, webIni.HTTPPort, webIni.appendURL, sub, threads.order[t], header.subject
			);
		out += format("Started by %s on %s<br />", header.from, system.timestr(header.when_written_time));
		if(threads.thread[threads.order[t]].messages.length > 1) {
			out += "Latest reply from ";
			out += threads.thread[threads.order[t]].messages[threads.thread[threads.order[t]].messages.length - 1].from;
			out += " on ";
			out += system.timestr(threads.thread[threads.order[t]].messages[threads.thread[threads.order[t]].messages.length - 1].when_written_time);
		}
		out += "</div>";
		out += format("<div id='sub-%s-thread-%s' style='display:none;'></div>", sub, threads.order[t]);
		out += format("<div id='sub-%s-thread-%s-info' class='border msg indentBox2' style='display:none;'></div>", sub, threads.order[t]);
	}
	print(out);
}

function printThread(sub, t) {
	var msgBase = new MsgBase(sub);
	if(!msgBase.open())
		return false;
	var threads = getMessageThreads(sub);
	var out = "";
	for(var m in threads.thread[t].messages) {
		var header = threads.thread[t].messages[m];
		var body = msgBase.get_msg_body(header.number, strip_ctrl_a=true);
		if(body === null)
			continue;
		out += format("<a name='%s-%s'></a>", sub, header.number);
		out += format("<div class='border indentBox3 msg' id='sub-%s-thread-%s-%s'>", sub, t, header.number);
		out += format(
			"From <b>%s</b> to <b>%s</b> on <b>%s</b><br /><br />",
			header.from, header.to, system.timestr(header.when_written_time)
		);
		out += linkify(strip_exascii(body).replace(/\r\n/g, "<br />").replace(/\n/g, "<br />"));
		out += "<br /><br />";
		out += format(
			"<a href='./index.xjs?page=002-forum.ssjs&board=%s&sub=%s&thread=%s#thread-%s'>Thread URL</a> - ",
			msgBase.cfg.grp_name, sub, t, t
		);
		out += format(
			"<a href='./index.xjs?page=002-forum.ssjs&board=%s&sub=%s&thread=%s&message=%s#%s-%s'>Message URL</a> - ",
			msgBase.cfg.grp_name, sub, t, header.number, sub, header.number
		);
		out += format("<a class=ulLink onclick='toggleVisibility(\"sub-%s-thread-%s\")'>Collapse Thread</a>", sub, t);
		if(user.alias != webIni.WebGuest && user.compare_ars(msgBase.cfg.post_ars))
			out += format(
				" - <a class='ulLink' onclick='addReply(\"http://%s:%s/%s/forum-async.ssjs\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\")'>Reply</a>",
				system.inet_addr, webIni.HTTPPort, webIni.appendURL, sub, t, header.number, header.from, user.alias, header.subject
		);
		out += "</div>";
	}
	print(out);
	msgBase.close();
}

function printMessage(sub, number) {
	var msgBase = new MsgBase(sub);
	if(!msgBase.open())
		return false;
	if(!user.compare_ars(msgBase.cfg.read_ars) || !user.compare_ars(msgBase.cfg.post_ars))
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
	var sig = getSig();
	if(sig)
		ret.sig = sig;
	print(JSON.stringify(ret));
}

function postMessage(sub, irt, to, from, subject, body) {
	for(var a = 0; a < arguments.length; a++) {
		if(a == "irt")
			continue;
		if(arguments[a] === undefined || arguments[a] === null || arguments[a] == "")
			return false;
	}
	if(user.alias != from && user.name != from)
		return false
	var msgBase = new MsgBase(sub);
	if(!msgBase.open())
		return false;
	if(!user.compare_ars(msgBase.cfg.post_ars))
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
	msgBase.save_msg(header, body);
	msgBase.close();
	print("Your message has been posted.");
	return true;
}