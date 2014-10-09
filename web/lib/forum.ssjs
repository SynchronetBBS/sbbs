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
		if(msg_area.grp_list[g].sub_list.length < 1)
			continue;
		var out = format(
			"<div class='border box msg' onclick='toggleVisibility(\"group-%s\")' style='cursor:pointer'>"
			+ "<a class='ulLink'>%s</a>"
			+ "<br />%s sub-boards</div>"
			+ "<div id='group-%s' style=display:none;'>",
			msg_area.grp_list[g].name,
			msg_area.grp_list[g].name,
			msg_area.grp_list[g].sub_list.length,
			msg_area.grp_list[g].name
		);
		for(var s = 0; s < msg_area.grp_list[g].sub_list.length; s++) {
			out += format(
				"<div class='border msg indentBox1' onclick='loadThreads(\"http://%s/%sforum-async.ssjs\", \"%s\")' style='cursor:pointer'>"
				+ "<a class='ulLink'>%s</a><br />",
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
 			for(var n = msgBase.last_msg; n >= msgBase.first_msg; n--) {
				var h = msgBase.get_msg_header(n);
				if(h === null || h.attr&MSG_DELETE)
					continue;
				out += format("Latest: %s, by %s on %s", h.subject, h.from, system.timestr(h.when_written_time));
				break;
			}
			msgBase.close();
			if(user.alias != webIni.WebGuest && msg_area.sub[msgBase.cfg.code].can_post) {
				out += format(
					"<br />"
					+ "<a class='ulLink' onclick='addPost(\"http://%s/%sforum-async.ssjs\", \"%s\", \"%s\", \"%s\");event.stopPropagation()'>Post a new message</a>"
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
			+ "<div class='border %s msg' onclick='loadThread(\"http://%s/%sforum-async.ssjs\", \"%s\", \"%s\")' style='cursor:pointer'>"
			+ "<a class='ulLink'>%s</a><br />"
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

var formatBody = function(body, ANSI_formatted, hide_quotes)
{
	if(ANSI_formatted) {
		body=html_encode(body, true, false, true, true);
		body=body.replace(/\r?\n+(<\/span>)?$/,'$1');
		body=linkify(body);

		// Get the last line
		var body_m=body.match(/\n([^\n]*)$/);
		if(body_m != null) {
			body = '<pre>'+body;
			body_m[1]=body_m[1].replace(/&[^;]*;/g,".");
			body_m[1]=body_m[1].replace(/<[^>]*>/g,"");
			var lenremain=80-body_m[1].length;
			while(lenremain > 0) {
				body += '&nbsp;';
				lenremain--;
			}
			body += '</pre>';
		}
		else {
			/* If we couldn't get the last line, add a line of 80 columns */
			body = '<pre>'+body;
			body += '&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;</pre>';
		}
	}
	else {
		
		var blockquote_start = '<blockquote title="Click to expand" onclick="toggle_quote(this);event.stopPropagation();return false" style="cursor:pointer"';
		if(hide_quotes==undefined || hide_quotes)
			blockquote_start += ' class="hiddenQuote"';
		blockquote_start += '>';
		var blockquote_end = '</blockquote>';
		
		// Strip CTRL-A
		body=body.replace(/\1./g,'');
		// Strip ANSI
		body=body.replace(/\x1b\[[\x30-\x3f]*[\x20-\x2f]*[\x40-\x7e]/g,'');
		body=body.replace(/\x1b[\x40-\x7e]/g,'');
		// Strip unprintable control chars (NULL, BEL, DEL, ESC)
		body=body.replace(/[\x00\x07\x1b\x7f]/g,'');

		// Wrap and encode
		body=word_wrap(body, body.length);
		body=html_encode(body, true, false, false, false);

		// Magical quoting stuff!
		/*
		 * Each /[^ ]{0,3}> / is a quote block
		 */
		var lines=body.split(/\r?\n/);
		var quote_depth=0;
		var prefixes=new Array();
		body='';
		for (l in lines) {
			var line_prefix='';
			var m=lines[l].match(/^((?:\s?[^\s]{0,3}&gt;\s?)+)/);

			if(m!=null) {
				var new_prefixes=m[1].match(/\s?[^\s]{0,3}&gt;\s?/g);
				var p;
				var broken=false;

				line=lines[l];
				
				// If the new length is smaller than the old one, close the extras
				for(p=new_prefixes.length; p<prefixes.length; p++) {
					if(quote_depth > 0) {
						line_prefix = line_prefix + blockquote_end;
						quote_depth--;
					}
					else {
						log("BODY: Depth problem 1");
					}
				}
				for(p in new_prefixes) {
					// Remove prefix from start of line
					line=line.substr(new_prefixes[p].length);

					if(prefixes[p]==undefined) {
						/* New depth */
						line_prefix = line_prefix + blockquote_start;
						quote_depth++;
					}
					else if(broken) {
						line_prefix = line_prefix + blockquote_start;
						quote_depth++;
					}
					else if(prefixes[p].replace(/^\s*(.*?)\s*$/,"$1") != new_prefixes[p].replace(/^\s*(.*?)\s*$/,"$1")) {
						// Close all remaining old prefixes and start one new one
						var o;
						for(o=p; o<prefixes.length && o<new_prefixes.length; o++) {
							if(quote_depth > 0) {
								line_prefix = blockquote_end+line_prefix;
								quote_depth--;
							}
							else {
								log("BODY: Depth problem 2");
							}
						}
						line_prefix = blockquote_start+line_prefix;
						quote_depth++;
						broken=true;
					}
				}
				prefixes=new_prefixes.slice();
				line=line_prefix+line;
			}
			else {
				for(p=0; p<prefixes.length; p++) {
					if(quote_depth > 0) {
						line_prefix = line_prefix + blockquote_end;
						quote_depth--;
					}
					else {
						log("BODY: Depth problem 3");
					}
				}
				prefixes=new Array();
				line=line_prefix + lines[l];
			}
			body = body + line + "\r\n";
		}
		if(quote_depth != 0) {
			log("BODY: Depth problem 4");
			for(;quote_depth > 0; quote_depth--) {
				body += blockquote_end;
			}
		}

		body=linkify(body);
		body=body.replace(/\r\n$/,'');
		body=body.replace(/(\r?\n)/g, "<br>$1");
		return body;
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
		var body = msgBase.get_msg_body(header.number, header, true /* strip_ctrl_a */);
		if(body === null)
			continue;
		body = expand_body(body, system.settings);
		body = formatBody(body, false, true);
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
			body
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
		} else if(header.attr^MSG_READ) {
			header.attr|=MSG_READ;
			msgBase.put_msg_header(header.number, header);
		}
		out += format(
			"<a class='ulLink' href='#thread-%s' onclick='toggleVisibility(\"sub-%s-thread-%s\")'>Collapse Thread</a>",
			t,
			sub,
			t
		);
		if(user.alias != webIni.WebGuest && (sub == "mail" || msg_area.sub[sub].can_post))
			out += format(
				" - <a class='ulLink' onclick='addReply(\"http://%s/%sforum-async.ssjs\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\")'>Reply</a>",
				http_request.host, webIni.appendURL, sub, t, header.number, header.from, user.alias, header.subject
			);
		out += format("<div id='sub-%s-message-%s-replyBox'></div>", sub, header.number);
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
	if(sub != "mail" && !msg_area.sub[msgBase.cfg.code].can_post)
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
